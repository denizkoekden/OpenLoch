#include "model.h"
#include "font.h"
#include "language.h"
#include "legacy_reader.h"
#include "formats/sprint/sprint.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QPainterPathStroker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTransform>
#include <QUuid>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <tuple>

namespace openloch::pcb {
namespace {
// Limits of the own format: 10 m of coordinates, 1 m of sizes, 100 000 nodes per element.
constexpr double maxCoordinate=10000,maxSize=1000;
constexpr int maxNodes=100000;
// Component numbers have 16 bits, as in Sprint-Layout; new ones stay below 0xFFFF.
constexpr int maxPart=0xffff;

const char *const shapeNames[]={"round","octagon","square","ovalWide","octagonWide","squareWide","ovalTall","octagonTall","squareTall"};
const char *const roleNames[]={"plain","designator","value"};
const char *typeName(ElementType type){
    switch(type){case ElementType::Pad:return "pad";case ElementType::SmdPad:return "smd";case ElementType::Track:return "track";
        case ElementType::Area:return "area";case ElementType::Circle:return "circle";case ElementType::Text:return "text";}
    return "track";
}

QJsonArray pointArray(const QPolygonF &points){QJsonArray a;for(auto p:points)a.append(QJsonArray{p.x(),p.y()});return a;}
QJsonArray flatArray(const QPolygonF &points){QJsonArray a;for(auto p:points){a.append(p.x());a.append(p.y());}return a;}
QJsonArray intArray(const QList<int> &values){QJsonArray a;for(int v:values)a.append(v);return a;}

[[noreturn]] void invalid(const QString &message){throw FormatError(message);}
double number(const QJsonObject &o,const char *key,double fallback,double low,double high){
    if(!o.contains(key))return fallback;
    const auto v=o[key];if(!v.isDouble()||!std::isfinite(v.toDouble())||v.toDouble()<low||v.toDouble()>high)invalid(ui("Ungültiger Zahlenwert in der Leiterplatte"));
    return v.toDouble();
}
int integer(const QJsonObject &o,const char *key,int fallback,int low,int high){
    const double v=number(o,key,fallback,low,high);if(v!=std::floor(v))invalid(ui("Ungültiger Zahlenwert in der Leiterplatte"));return int(v);
}
bool flag(const QJsonObject &o,const char *key,bool fallback){
    if(!o.contains(key))return fallback;if(!o[key].isBool())invalid(ui("Ungültiger Schalter in der Leiterplatte"));return o[key].toBool();
}
QString text(const QJsonObject &o,const char *key){
    if(!o.contains(key))return {};if(!o[key].isString()||o[key].toString().size()>65536)invalid(ui("Ungültiger Text in der Leiterplatte"));return o[key].toString();
}
QPointF point(const QJsonValue &v){
    const auto a=v.toArray();if(!v.isArray()||a.size()!=2)invalid(ui("Ungültiger Knoten in der Leiterplatte"));
    QPointF p;for(int i=0;i<2;i++){const auto c=a[i];if(!c.isDouble()||!std::isfinite(c.toDouble())||std::abs(c.toDouble())>maxCoordinate)invalid(ui("Ungültiger Knoten in der Leiterplatte"));(i?p.ry():p.rx())=c.toDouble();}
    return p;
}
QPolygonF points(const QJsonObject &o,const char *key){
    QPolygonF out;if(!o.contains(key))return out;const auto a=o[key].toArray();
    if(!o[key].isArray()||a.size()>maxNodes)invalid(ui("Ungültiger Knoten in der Leiterplatte"));
    for(const auto &v:a)out<<point(v);return out;
}
QPolygonF flatPoints(const QJsonValue &v){
    const auto a=v.toArray();if(!v.isArray()||a.size()%2||a.size()>2*maxNodes)invalid(ui("Ungültiger Knoten in der Leiterplatte"));
    QPolygonF out;for(qsizetype i=0;i<a.size();i+=2)out<<point(QJsonArray{a[i],a[i+1]});return out;
}
QList<int> integers(const QJsonObject &o,const char *key,int high){
    QList<int> out;if(!o.contains(key))return out;const auto a=o[key].toArray();
    if(!o[key].isArray()||a.size()>maxNodes)invalid(ui("Ungültige Liste in der Leiterplatte"));
    for(const auto &v:a){if(!v.isDouble()||v.toDouble()!=std::floor(v.toDouble())||v.toDouble()<0||v.toDouble()>high)invalid(ui("Ungültige Liste in der Leiterplatte"));out.append(int(v.toDouble()));}
    return out;
}
QByteArray base64(const QJsonObject &o,const char *key,int maxBytes){
    if(!o.contains(key))return {};
    if(!o[key].isString()||o[key].toString().size()>maxBytes*2)invalid(ui("Ungültige Quelldaten in der Leiterplatte"));
    const auto r=QByteArray::fromBase64Encoding(o[key].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(!r||r.decoded.size()>maxBytes)invalid(ui("Ungültige Quelldaten in der Leiterplatte"));return r.decoded;
}
QJsonObject sprintJson(const SprintSource &s){
    QJsonObject o;if(!s.record.isEmpty())o["record"]=QString::fromLatin1(s.record.toBase64());
    if(!s.component.isEmpty())o["component"]=QString::fromLatin1(s.component.toBase64());if(!s.text2.isEmpty())o["text2"]=QString::fromLatin1(s.text2.toBase64());
    return o;
}

QJsonObject elementJson(const Element &e){
    QJsonObject o{{"type",typeName(e.type)},{"layer",e.layer}};
    if(!e.name.isEmpty())o["name"]=e.name;
    switch(e.type){
    case ElementType::Pad:
        o["x"]=e.pos.x();o["y"]=e.pos.y();o["diameter"]=e.size;o["drill"]=e.size2;o["shape"]=shapeNames[int(e.shape)-1];o["rotation"]=e.rotation;
        o["via"]=e.via;o["outline"]=pointArray(e.points);o["solderMask"]=e.solderMask;
        o["thermal"]=e.thermal;o["thermalSpokes"]=qint64(e.thermalSpokes);o["thermalPerLayer"]=e.thermalPerLayer;o["thermalWidth"]=e.thermalWidth;break;
    case ElementType::SmdPad:
        o["x"]=e.pos.x();o["y"]=e.pos.y();o["sizeX"]=e.size;o["sizeY"]=e.size2;o["rotation"]=e.rotation;o["outline"]=pointArray(e.points);
        o["solderMask"]=e.solderMask;o["thermal"]=e.thermal;o["thermalSpokes"]=qint64(e.thermalSpokes);o["thermalWidth"]=e.thermalWidth;break;
    case ElementType::Track:
        o["width"]=e.width;o["points"]=pointArray(e.points);o["flatStart"]=e.flatStart;o["flatEnd"]=e.flatEnd;o["autorouted"]=e.autorouted;o["solderMask"]=e.solderMask;o["cutout"]=e.cutout;
        // Also when no pad is known: without the field the pads would be looked for under the track's ends.
        if(e.autorouted)o["autoroutePads"]=QJsonArray{e.autoroutePads[0],e.autoroutePads[1]};
        break;
    case ElementType::Area:
        o["width"]=e.width;o["points"]=pointArray(e.points);o["hatched"]=e.hatched;o["hatchAuto"]=e.hatchAuto;o["hatchPitch"]=e.hatchPitch;o["solderMask"]=e.solderMask;o["cutout"]=e.cutout;
        o["maskOnly"]=e.maskOnly;break;
    case ElementType::Circle:
        o["x"]=e.pos.x();o["y"]=e.pos.y();o["radius"]=e.size;o["width"]=e.width;o["start"]=e.start;o["stop"]=e.stop;o["filled"]=e.filled;
        o["solderMask"]=e.solderMask;o["cutout"]=e.cutout;break;
    case ElementType::Text:{
        o["x"]=e.pos.x();o["y"]=e.pos.y();o["text"]=e.text;o["height"]=e.size;o["style"]=e.style;o["thickness"]=e.thickness;o["rotation"]=e.rotation;
        o["mirrored"]=e.mirrored;if(e.flipped)o["flipped"]=true;o["role"]=roleNames[int(e.role)];o["visible"]=e.visible;o["solderMask"]=e.solderMask;o["cutout"]=e.cutout;
        QJsonArray strokes;for(const auto &s:e.strokes)strokes.append(flatArray(s));o["strokes"]=strokes;o["strokeWidth"]=e.strokeWidth;
        if(e.role==TextRole::Designator){o["package"]=e.package;o["comment"]=e.comment;o["componentRotation"]=e.componentRotation;o["pickAndPlace"]=e.pickAndPlace;
            o["pickCentre"]=e.pickCentre;o["pickOffsetX"]=e.pickOffset.x();o["pickOffsetY"]=e.pickOffset.y();}
        break;}
    }
    o["clearance"]=e.clearance;
    if(!e.component.isEmpty())o["component"]=e.component;
    if(!e.pin.isEmpty())o["pin"]=e.pin;
    if(e.part)o["part"]=e.part;
    if(!e.groups.isEmpty())o["groups"]=intArray(e.groups);
    if(!e.connections.isEmpty())o["connections"]=intArray(e.connections);
    const auto s=sprintJson(e.sprint);if(!s.isEmpty())o["sprint"]=s;
    return o;
}

Element elementFrom(const QJsonObject &o,int elementCount){
    Element e;const QString type=o["type"].toString();
    if(type=="pad")e.type=ElementType::Pad;else if(type=="smd")e.type=ElementType::SmdPad;else if(type=="track")e.type=ElementType::Track;
    else if(type=="area")e.type=ElementType::Area;else if(type=="circle")e.type=ElementType::Circle;else if(type=="text")e.type=ElementType::Text;
    else invalid(ui("Unbekanntes Element in der Leiterplatte"));
    e=newElement(e.type);
    e.layer=integer(o,"layer",e.layer,1,layerCount);e.name=text(o,"name");
    e.pos={number(o,"x",0,-maxCoordinate,maxCoordinate),number(o,"y",0,-maxCoordinate,maxCoordinate)};
    e.clearance=number(o,"clearance",e.clearance,0,maxSize);e.solderMask=flag(o,"solderMask",e.solderMask);e.cutout=flag(o,"cutout",false);
    e.component=text(o,"component");e.pin=text(o,"pin");e.part=integer(o,"part",0,0,maxPart);
    e.groups=integers(o,"groups",2147483647);e.connections=integers(o,"connections",std::max(0,elementCount-1));
    if(o.contains("sprint")){
        const auto s=o["sprint"].toObject();if(!o["sprint"].isObject())invalid(ui("Ungültige Quelldaten in der Leiterplatte"));
        e.sprint.record=base64(s,"record",4096);e.sprint.component=base64(s,"component",4096);e.sprint.text2=base64(s,"text2",65536);
    }
    auto thermalFields=[&]{
        e.thermal=flag(o,"thermal",false);e.thermalSpokes=quint32(number(o,"thermalSpokes",0x55,0,4294967295.0));e.thermalWidth=integer(o,"thermalWidth",100,0,1000);
        e.thermalPerLayer=flag(o,"thermalPerLayer",false);
    };
    switch(e.type){
    case ElementType::Pad:{
        e.size=number(o,"diameter",e.size,0,maxSize);e.size2=number(o,"drill",e.size2,0,maxSize);e.rotation=number(o,"rotation",0,-1e6,1e6);
        const QString shape=o.contains("shape")?text(o,"shape"):"round";const auto names=std::begin(shapeNames);
        const auto it=std::find(names,std::end(shapeNames),shape);if(it==std::end(shapeNames))invalid(ui("Unbekannte Lötaugenform"));
        e.shape=PadShape(int(it-names)+1);e.via=flag(o,"via",false);thermalFields();
        e.points=points(o,"outline");if(e.points.isEmpty())updateOutline(e);
        const int expected=e.shape==PadShape::Round||e.shape==PadShape::OvalWide||e.shape==PadShape::OvalTall?2:
                           e.shape==PadShape::Octagon||e.shape==PadShape::OctagonWide||e.shape==PadShape::OctagonTall?8:4;
        if(e.points.size()!=expected)invalid(ui("Die Kontur des Lötauges passt nicht zu seiner Form"));
        break;}
    case ElementType::SmdPad:
        e.size=number(o,"sizeX",e.size,0,maxSize);e.size2=number(o,"sizeY",e.size2,0,maxSize);e.rotation=number(o,"rotation",0,-1e6,1e6);thermalFields();
        e.points=points(o,"outline");if(e.points.isEmpty())updateOutline(e);if(e.points.size()!=4)invalid(ui("Die Kontur des SMD-Pads braucht vier Ecken"));
        break;
    case ElementType::Track:case ElementType::Area:
        e.width=number(o,"width",e.width,0,maxSize);e.points=points(o,"points");
        if(e.points.size()<(e.type==ElementType::Track?2:3))invalid(e.type==ElementType::Track?ui("Eine Leiterbahn braucht mindestens zwei Knoten"):ui("Eine Fläche braucht mindestens drei Knoten"));
        e.flatStart=flag(o,"flatStart",false);e.flatEnd=flag(o,"flatEnd",false);e.hatched=flag(o,"hatched",false);e.autorouted=flag(o,"autorouted",false);
        e.hatchAuto=flag(o,"hatchAuto",true);e.hatchPitch=number(o,"hatchPitch",.5,0,1000);if(e.type==ElementType::Area)e.maskOnly=flag(o,"maskOnly",false);
        if(e.type==ElementType::Track&&e.autorouted&&o.contains("autoroutePads")){
            const auto a=o["autoroutePads"].toArray();if(!o["autoroutePads"].isArray()||a.size()!=2)invalid(ui("Ungültige Liste in der Leiterplatte"));
            for(int k=0;k<2;k++){const double v=a[k].toDouble(-2);if(!a[k].isDouble()||v!=std::floor(v)||v<-1||v>=elementCount)invalid(ui("Ungültige Liste in der Leiterplatte"));e.autoroutePads[k]=int(v);}
        }
        break;
    case ElementType::Circle:
        e.size=number(o,"radius",e.size,0,maxSize);e.width=number(o,"width",e.width,0,maxSize);
        e.start=number(o,"start",0,-1e6,1e6);e.stop=number(o,"stop",0,-1e6,1e6);e.filled=flag(o,"filled",false);break;
    case ElementType::Text:{
        e.text=text(o,"text");e.size=number(o,"height",e.size,0,maxSize);e.style=integer(o,"style",1,0,2);e.thickness=integer(o,"thickness",1,0,2);
        e.rotation=number(o,"rotation",0,-1e6,1e6);e.mirrored=flag(o,"mirrored",false);e.flipped=flag(o,"flipped",false);e.visible=flag(o,"visible",true);
        const QString role=o.contains("role")?text(o,"role"):"plain";
        if(role=="plain")e.role=TextRole::Plain;else if(role=="designator")e.role=TextRole::Designator;else if(role=="value")e.role=TextRole::Value;
        else invalid(ui("Unbekannte Textart"));
        const auto strokes=o["strokes"].toArray();if(o.contains("strokes")&&(!o["strokes"].isArray()||strokes.size()>maxNodes))invalid(ui("Ungültiger Knoten in der Leiterplatte"));
        for(const auto &s:strokes)e.strokes.append(flatPoints(s));
        e.strokeWidth=number(o,"strokeWidth",0,0,maxSize);
        e.package=text(o,"package");e.comment=text(o,"comment");e.componentRotation=number(o,"componentRotation",0,-1e6,1e6);e.pickAndPlace=flag(o,"pickAndPlace",false);
        e.pickCentre=integer(o,"pickCentre",0,0,2);e.pickOffset=QPointF(number(o,"pickOffsetX",0,-maxSize,maxSize),number(o,"pickOffsetY",0,-maxSize,maxSize));
        break;}
    }
    return e;
}
// Before version 4 a component was the innermost group of its designator: the elements of that group get its number
// (the innermost such group of an element counts).
void partsFromGroups(QList<Element> &elements){
    QMap<int,int> number;
    for(const auto &e:elements)if(e.type==ElementType::Text&&e.role==TextRole::Designator&&!e.groups.isEmpty()&&!number.contains(e.groups.first()))
        number.insert(e.groups.first(),int(number.size())+1);
    for(auto &e:elements){e.part=0;for(int g:e.groups)if(number.contains(g)){e.part=number[g];break;}}
}
QJsonArray layerList(const std::array<bool,layerCount+1> &on){QJsonArray a;for(int l=1;l<=layerCount;l++)if(on[l])a.append(l);return a;}
std::array<bool,layerCount+1> layerSet(const QJsonObject &o,const char *key,std::array<bool,layerCount+1> fallback){
    if(!o.contains(key))return fallback;std::array<bool,layerCount+1> on{};for(int l:integers(o,key,layerCount)){if(l<1)invalid(ui("Ungültiger Layer"));on[l]=true;}return on;
}
bool isPad(const Element &e){return e.type==ElementType::Pad||e.type==ElementType::SmdPad;}
// The pads under the first and the last node of a track (-1 where there is none, both -1 when it is the same pad): what
// OpenLoch turned an autorouted track back into before it kept the pads.
std::array<int,2> padsUnderEnds(const Board &b,const Element &track){
    auto under=[&](QPointF at){for(int i=0;i<b.elements.size();i++)if(isPad(b.elements[i])&&copperShape(b.elements[i]).contains(at))return i;return -1;};
    const int one=under(track.points.first()),two=under(track.points.last());
    return one==two?std::array<int,2>{-1,-1}:std::array<int,2>{one,two};
}
}

bool isCopper(int layer){return layer==CopperTop||layer==CopperBottom||layer==Inner1||layer==Inner2;}
QString layerName(int layer){
    static const char *const names[]={"","K1","B1","K2","B2","I1","I2","U"};return layer>=1&&layer<=layerCount?ui(names[layer]):QString();
}
QList<int> drawingOrder(int active,bool fromBelow){
    // Copper first: the group of the active layer (outer or inner) above the other group, the active layer on top of
    // its group. Then the silkscreens, the active side on top (the top side with K1 or B1 active), the outline last.
    switch(active){
    case CopperTop:case SilkTop:return {Inner1,Inner2,CopperBottom,CopperTop,SilkBottom,SilkTop,Outline};
    case CopperBottom:case SilkBottom:return {Inner1,Inner2,CopperTop,CopperBottom,SilkTop,SilkBottom,Outline};
    case Inner1:return {CopperTop,CopperBottom,Inner2,Inner1,SilkTop,SilkBottom,Outline};
    case Inner2:return {CopperTop,CopperBottom,Inner1,Inner2,SilkTop,SilkBottom,Outline};
    default:
        return fromBelow?QList<int>{CopperTop,Inner1,Inner2,CopperBottom,SilkTop,SilkBottom,Outline}:QList<int>{CopperBottom,Inner2,Inner1,CopperTop,SilkBottom,SilkTop,Outline};
    }
}
QList<int> layerOrder(){return {CopperTop,SilkTop,Inner1,Inner2,CopperBottom,SilkBottom,Outline};}

QJsonObject toJson(const Document &document){
    QJsonArray boards;
    for(const auto &b:document.boards){
        QJsonArray elements;for(const auto &e:b.elements)elements.append(elementJson(e));
        QJsonObject o{{"id",b.id},{"name",b.name},{"width",b.width},{"height",b.height},{"grid",b.grid},{"activeLayer",b.activeLayer},{"multilayer",b.multilayer},{"originX",b.origin.x()},{"originY",b.origin.y()},
                      {"visibleLayers",layerList(b.visible)},{"groundPlanes",layerList(b.groundPlane)},{"elements",elements}};
        if(!b.sprintHeader.isEmpty())o["sprintHeader"]=QString::fromLatin1(b.sprintHeader.toBase64());
        // Also without pictures: offsets and colours read from Sprint-Layout stay with the board.
        if(b.templates[0]!=Template{}||b.templates[1]!=Template{}){
            QJsonArray list;
            for(const auto &t:b.templates)list.append(QJsonObject{{"file",t.file},{"dpi",t.dpi},{"x",t.offset.x()},{"y",t.offset.y()},{"shown",t.shown},{"colour",t.colour.name(QColor::HexRgb)}});
            o["templates"]=list;
        }
        boards.append(o);
    }
    return {{"format","OpenLoch PCB"},{"version",4},{"unit","mm"},{"title",document.title},{"author",document.author},{"company",document.company},
            {"comment",document.comment},{"activeBoard",document.activeBoard},{"boards",boards}};
}
Document fromJson(const QJsonObject &json){
    if(json["format"]!="OpenLoch PCB")invalid(ui("Keine OpenLoch-Leiterplatte"));
    // Version 1 has neither the pick and place centre nor solder mask openings, versions 1 and 2 no identifiers: they
    // read as their defaults, the identifiers come new. Versions 1 to 3 know no component numbers.
    if(!json["version"].isDouble()||json["version"].toInt()<1||json["version"].toInt()>4)invalid(ui("Diese Version der Leiterplattendatei wird nicht unterstützt"));
    const int version=json["version"].toInt();
    if(json.contains("unit")&&json["unit"]!="mm")invalid(ui("Unbekannte Einheit in der Leiterplatte"));
    Document d;d.title=text(json,"title");d.author=text(json,"author");d.company=text(json,"company");d.comment=text(json,"comment");
    const auto boards=json["boards"].toArray();if(!json["boards"].isArray()||boards.isEmpty()||boards.size()>1000)invalid(ui("Die Datei enthält keine Platine"));
    for(const auto &value:boards){
        if(!value.isObject())invalid(ui("Ungültige Platine"));const auto o=value.toObject();
        Board b;b.id=text(o,"id");b.name=text(o,"name");b.width=number(o,"width",160,.1,maxSize*10);b.height=number(o,"height",100,.1,maxSize*10);
        b.grid=number(o,"grid",1.27,.001,1000);b.activeLayer=integer(o,"activeLayer",CopperBottom,1,layerCount);
        b.visible=layerSet(o,"visibleLayers",b.visible);b.groundPlane=layerSet(o,"groundPlanes",{});b.sprintHeader=base64(o,"sprintHeader",4096);
        b.multilayer=flag(o,"multilayer",false);
        b.origin=QPointF(number(o,"originX",0,-maxSize*10,maxSize*10),number(o,"originY",b.height,-maxSize*10,maxSize*10));
        if(o.contains("templates")){
            const auto list=o["templates"].toArray();if(!o["templates"].isArray()||list.size()>2)invalid(ui("Ungültige Platine"));
            for(int k=0;k<list.size();k++){const auto t=list[k].toObject();auto &to=b.templates[k];to.file=text(t,"file");to.dpi=number(t,"dpi",600,1,100000);
                to.offset=QPointF(number(t,"x",0,-maxSize*10,maxSize*10),number(t,"y",0,-maxSize*10,maxSize*10));to.shown=flag(t,"shown",false);
                if(t.contains("colour")){const QString c=text(t,"colour");to.colour=QColor::fromString(c);if(!c.startsWith('#')||c.size()!=7||!to.colour.isValid())invalid(ui("Ungültige Platine"));}}
        }
        const auto elements=o["elements"].toArray();if(o.contains("elements")&&(!o["elements"].isArray()||elements.size()>maxNodes))invalid(ui("Ungültige Platine"));
        for(const auto &e:elements){if(!e.isObject())invalid(ui("Unbekanntes Element in der Leiterplatte"));b.elements.append(elementFrom(e.toObject(),int(elements.size())));}
        // The pads of autorouted tracks; files written before OpenLoch kept them get the pads under the track's ends.
        for(int i=0;i<b.elements.size();i++){
            if(b.elements[i].type!=ElementType::Track||!b.elements[i].autorouted)continue;
            if(!elements[i].toObject().contains("autoroutePads"))b.elements[i].autoroutePads=padsUnderEnds(b,b.elements[i]);
            for(int p:b.elements[i].autoroutePads)if(p>=0&&!isPad(b.elements[p]))invalid(ui("Ungültige Liste in der Leiterplatte"));
        }
        if(version<4)partsFromGroups(b.elements);
        d.boards.append(b);
    }
    d.activeBoard=integer(json,"activeBoard",0,0,int(d.boards.size())-1);
    assignIds(d);
    return d;
}
QByteArray encode(const Document &document){return QJsonDocument(toJson(document)).toJson(QJsonDocument::Indented);}
Document decode(const QByteArray &bytes){
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())invalid(ui("Die Leiterplattendatei ist beschädigt"));
    return fromJson(doc.object());
}
Document load(const QString &path,QStringList *notes){
    QFile f(path);if(!f.open(QIODevice::ReadOnly))throw FormatError(f.errorString());
    if(f.size()>128*1024*1024)invalid(ui("Datei ist zu groß"));const auto bytes=f.readAll();
    // Sprint-Layout by its marker bytes, also versions this reader does not know (it names them as such).
    const bool sprintBytes=bytes.size()>=4&&quint8(bytes[1])==0x33&&quint8(bytes[2])==0xaa&&quint8(bytes[3])==0xff;
    if(sprint::fileVersion(bytes)>=0||sprintBytes){auto d=sprint::readLayout(bytes,notes);assignIds(d);return d;}
    return decode(bytes);
}
void save(const Document &document,const QString &path){
    const auto data=encode(document);QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))throw FormatError(file.errorString());
    if(file.write(data)!=data.size()||!file.commit())throw FormatError(file.errorString());
}

namespace {
// Offsets in a y-up frame (as in the reference's files) turned counter-clockwise and placed at `centre` (y down).
QPolygonF placed(const QList<QPointF> &offsets,QPointF centre,double rotation){
    const double a=qDegreesToRadians(rotation),c=std::cos(a),s=std::sin(a);QPolygonF out;
    for(auto o:offsets){const double x=o.x()*c-o.y()*s,y=o.x()*s+o.y()*c;out<<QPointF(centre.x()+x,centre.y()-y);}
    return out;
}
// An octagon with half extents hx × hy; the reference cuts the corners at 0.585 of the smaller half extent.
QList<QPointF> octagon(double hx,double hy){
    const double c=.585*std::min(hx,hy);
    return {{-hx,hy-c},{-hx+c,hy},{hx-c,hy},{hx,hy-c},{hx,-hy+c},{hx-c,-hy},{-hx+c,-hy},{-hx,-hy+c}};
}
QList<QPointF> rectangle(double hx,double hy){return {{-hx,hy},{hx,hy},{hx,-hy},{-hx,-hy}};}
QPointF turned(QPointF p,QPointF pivot,double degrees){
    const double a=qDegreesToRadians(degrees),c=std::cos(a),s=std::sin(a);const QPointF d=p-pivot;
    return pivot+QPointF(d.x()*c+d.y()*s,-d.x()*s+d.y()*c);
}
double normalized(double degrees){double a=std::fmod(degrees,360.0);if(a<0)a+=360;return a;}
}

QPolygonF padOutline(PadShape shape,QPointF centre,double diameter,double rotation){
    const double r=diameter/2;
    switch(shape){
    case PadShape::Round:case PadShape::OvalWide:return placed({{-r,0},{r,0}},centre,rotation);
    case PadShape::OvalTall:return placed({{0,r},{0,-r}},centre,rotation);
    case PadShape::Octagon:return placed(octagon(r,r),centre,rotation);
    case PadShape::OctagonWide:return placed(octagon(2*r,r),centre,rotation);
    case PadShape::OctagonTall:return placed(octagon(r,2*r),centre,rotation);
    case PadShape::Square:return placed(rectangle(r,r),centre,rotation);
    case PadShape::SquareWide:return placed(rectangle(2*r,r),centre,rotation);
    case PadShape::SquareTall:return placed(rectangle(r,2*r),centre,rotation);
    }
    return {};
}
QPolygonF smdOutline(QPointF centre,double width,double height,double rotation){return placed(rectangle(width/2,height/2),centre,rotation);}
void updateOutline(Element &pad){
    if(pad.type==ElementType::Pad)pad.points=padOutline(pad.shape,pad.pos,pad.size,pad.rotation);
    else if(pad.type==ElementType::SmdPad)pad.points=smdOutline(pad.pos,pad.size,pad.size2,pad.rotation);
}

bool crossesItself(const QPolygonF &outline){
    QPolygonF p=outline;if(p.size()>1&&p.first()==p.last())p.removeLast();const qsizetype n=p.size();if(n<4)return false;
    auto side=[](QPointF a,QPointF b,QPointF c){const double v=(b.x()-a.x())*(c.y()-a.y())-(b.y()-a.y())*(c.x()-a.x());return v>1e-12?1:v<-1e-12?-1:0;};
    auto within=[](QPointF a,QPointF b,QPointF c){return std::min(a.x(),b.x())-1e-12<=c.x()&&c.x()<=std::max(a.x(),b.x())+1e-12&&std::min(a.y(),b.y())-1e-12<=c.y()&&c.y()<=std::max(a.y(),b.y())+1e-12;};
    for(qsizetype i=0;i<n;i++){
        const QPointF a=p[i],b=p[(i+1)%n];
        for(qsizetype j=i+2;j<n;j++){
            if(i==0&&j==n-1)continue;     // the closing edge meets the first one at their shared node
            const QPointF c=p[j],d=p[(j+1)%n];
            if(std::max(a.x(),b.x())<std::min(c.x(),d.x())-1e-12||std::max(c.x(),d.x())<std::min(a.x(),b.x())-1e-12||
               std::max(a.y(),b.y())<std::min(c.y(),d.y())-1e-12||std::max(c.y(),d.y())<std::min(a.y(),b.y())-1e-12)continue;
            const int s1=side(a,b,c),s2=side(a,b,d),s3=side(c,d,a),s4=side(c,d,b);
            if(s1*s2<0&&s3*s4<0)return true;
            if((s1==0&&within(a,b,c))||(s2==0&&within(a,b,d))||(s3==0&&within(c,d,a))||(s4==0&&within(c,d,b)))return true;
        }
    }
    return false;
}
QPainterPath orientedRegion(const QPainterPath &region){
    // In micrometres, so that the clipper's tolerances stay far below what a board shows.
    const QTransform up=QTransform::fromScale(1000,1000),down=QTransform::fromScale(.001,.001);
    QList<QPolygonF> rings;
    for(auto r:up.map(region).simplified().toSubpathPolygons()){if(r.size()>1&&r.first()==r.last())r.removeLast();if(r.size()>=3)rings.append(r);}
    auto area=[](const QPolygonF &r){double a=0;for(qsizetype i=0;i<r.size();i++){const QPointF u=r[i],v=r[(i+1)%r.size()];a+=u.x()*v.y()-v.x()*u.y();}return a/2;};
    QPainterPath out;out.setFillRule(Qt::WindingFill);
    for(qsizetype i=0;i<rings.size();i++){
        QPolygonF r=rings[i];const double a=area(r);if(std::abs(a)<1e-6)continue;
        // A point just inside the ring beside the middle of its longest edge: the rings holding it give its depth, even
        // for an outer contour, odd for a hole.
        qsizetype k=0;double longest=0;
        for(qsizetype j=0;j<r.size();j++){const QPointF d=r[(j+1)%r.size()]-r[j];const double l=std::hypot(d.x(),d.y());if(l>longest){longest=l;k=j;}}
        const QPointF d=(r[(k+1)%r.size()]-r[k])/longest,inward=a>0?QPointF(-d.y(),d.x()):QPointF(d.y(),-d.x());
        const QPointF probe=(r[k]+r[(k+1)%r.size()])/2+inward*std::min(.25,longest*1e-3);
        int depth=0;for(qsizetype j=0;j<rings.size();j++)if(j!=i&&rings[j].containsPoint(probe,Qt::OddEvenFill))depth++;
        if((a>0)!=(depth%2==0))std::reverse(r.begin(),r.end());
        out.addPolygon(down.map(r));out.closeSubpath();
    }
    return out;
}
QPainterPath evenOddArea(const QPolygonF &outline){
    QPainterPath path;path.addPolygon(outline);path.closeSubpath();
    if(!crossesItself(outline)){path.setFillRule(Qt::WindingFill);return path;}
    path.setFillRule(Qt::OddEvenFill);return orientedRegion(path);
}
QPainterPath copperShape(const Element &e){
    QPainterPath path;
    auto stroked=[](const QPainterPath &line,double width,bool flat){
        QPainterPathStroker s;s.setWidth(std::max(width,1e-3));s.setCapStyle(flat?Qt::FlatCap:Qt::RoundCap);s.setJoinStyle(Qt::RoundJoin);return s.createStroke(line);
    };
    switch(e.type){
    case ElementType::Pad:
        if(e.points.size()==2){
            if(e.shape==PadShape::Round){path.addEllipse(e.pos,e.size/2,e.size/2);break;}
            QPainterPath line(e.points[0]);line.lineTo(e.points[1]);path=stroked(line,e.size,false);
        }else path.addPolygon(e.points),path.closeSubpath();
        break;
    case ElementType::SmdPad:path.addPolygon(e.points);path.closeSubpath();break;
    case ElementType::Track:{
        QPainterPath line;line.addPolygon(e.points);path=stroked(line,e.width,false);
        if(e.points.size()>1&&(e.flatStart||e.flatEnd)){
            QPainterPath ends;
            if(e.flatStart)ends.addPolygon(squareEnd(e.points.first(),e.points[1],e.width));
            if(e.flatEnd)ends.addPolygon(squareEnd(e.points.last(),e.points[e.points.size()-2],e.width));
            path=path.united(ends);
        }
        break;}
    case ElementType::Area:{
        // By the even-odd rule; an outline crossing itself keeps contours that either fill rule fills the same, also
        // with its border.
        path=evenOddArea(e.points);
        if(e.width>0){QPainterPath outline;outline.addPolygon(e.points);outline.closeSubpath();path=path.united(stroked(outline,e.width,false));
            if(crossesItself(e.points))path=orientedRegion(path);}
        break;}
    case ElementType::Circle:{
        const double outer=e.size+e.width/2;const bool full=std::abs(normalized(e.stop-e.start))<1e-9;
        const double span=full?360:normalized(e.stop-e.start);const QRectF box(e.pos-QPointF(e.size,e.size),QSizeF(2*e.size,2*e.size));
        if(e.filled){
            const QRectF o(e.pos-QPointF(outer,outer),QSizeF(2*outer,2*outer));
            if(full)path.addEllipse(o);else{path.moveTo(e.pos);path.arcTo(o,e.start,span);path.closeSubpath();}
        }else{
            QPainterPath arc;if(full){arc.addEllipse(box);}else{arc.arcMoveTo(box,e.start);arc.arcTo(box,e.start,span);}
            path=stroked(arc,e.width,false);
        }
        break;}
    case ElementType::Text:
        for(const auto &s:e.strokes){QPainterPath line;line.addPolygon(s);if(s.size()==1)line.lineTo(s[0]);path.addPath(stroked(line,e.strokeWidth,false));}
        if(e.strokes.isEmpty())path.addRect(QRectF(e.pos-QPointF(0,e.size),QSizeF(e.size*.6*std::max<qsizetype>(1,e.text.size()),e.size)));
        break;
    }
    return path;
}
QPolygonF withoutRedundantNodes(const QPolygonF &nodes,bool closed){
    QPolygonF out;for(auto p:nodes)if(out.isEmpty()||std::hypot(out.last().x()-p.x(),out.last().y()-p.y())>1e-9)out<<p;
    if(closed)while(out.size()>3&&std::hypot(out.last().x()-out.first().x(),out.last().y()-out.first().y())<=1e-9)out.removeLast();
    // A middle point goes when it lies on the straight line between its neighbours, in the same direction.
    for(qsizetype i=1;i+1<out.size();){
        const QPointF a=out[i]-out[i-1],c=out[i+1]-out[i];const double cross=a.x()*c.y()-a.y()*c.x(),dot=a.x()*c.x()+a.y()*c.y();
        if(std::abs(cross)<=1e-9*std::max(1.0,std::hypot(a.x(),a.y())*std::hypot(c.x(),c.y()))&&dot>0)out.remove(i);else i++;
    }
    return out;
}
QPolygonF squareEnd(QPointF end,QPointF previous,double width){
    QPointF d=end-previous;const double l=std::hypot(d.x(),d.y());if(l<=0)d=QPointF(1,0);else d/=l;
    const QPointF n(-d.y(),d.x()),h=n*width/2,o=d*width/2;
    return QPolygonF({end+h,end+o+h,end+o-h,end-h});
}
double hatchSpacing(const Element &e){return std::max(.5,e.hatchAuto?2*e.width:e.hatchPitch);}
// The grid is laid out in the reference's whole units of a tenth of a micrometre.
static constexpr double hatchUnit=1e4;
double hatchLineWidth(const Element &e){return double(std::llround(hatchSpacing(e)*hatchUnit)/2)/hatchUnit;}
QList<QLineF> hatchLines(const Element &e){
    QList<QLineF> out;if(e.type!=ElementType::Area||e.points.size()<3)return out;
    const qint64 p=std::max<qint64>(5000,std::llround(hatchSpacing(e)*hatchUnit));
    // The nodes in whole units; a node exactly on a grid line moves 2 units right or up, off the line, so that the lines
    // only ever cross edges.
    QList<std::pair<qint64,qint64>> v;qint64 left=0,right=0,top=0,bottom=0;
    for(auto q:e.points){
        // A node beyond what the own format holds, or no number at all (a damaged Sprint-Layout file), gives no grid:
        // its units would not fit the integer arithmetic below.
        if(!(std::abs(q.x())<=maxCoordinate&&std::abs(q.y())<=maxCoordinate))return out;
        qint64 x=std::llround(q.x()*hatchUnit),y=std::llround(q.y()*hatchUnit);if(x%p==0)x+=2;if(y%p==0)y-=2;
        if(v.isEmpty()){left=right=x;top=bottom=y;}else{left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}
        v.append({x,y});
    }
    if((right-left)/p>100000||(bottom-top)/p>100000)return out;
    // The crossings of every edge with the columns (x = k·p) and rows (y = k·p) between its ends, rounded to units.
    auto beyond=[p](qint64 a){return (a>=0?a/p:-((p-1-a)/p))+1;};    // the first line after a
    QMap<qint64,QList<qint64>> columns,rows;const qsizetype n=v.size();
    for(qsizetype i=0;i<n;i++){
        const auto [ax,ay]=v[i];const auto [bx,by]=v[(i+1)%n];
        for(qint64 k=beyond(std::min(ax,bx));k*p<std::max(ax,bx);k++)columns[k].append(std::llrint(ay+double(k*p-ax)*double(by-ay)/double(bx-ax)));
        for(qint64 k=beyond(std::min(ay,by));k*p<std::max(ay,by);k++)rows[k].append(std::llrint(ax+double(k*p-ay)*double(bx-ax)/double(by-ay)));
    }
    // Along each line the copper runs from the 1st crossing to the 2nd, the 3rd to the 4th and so on; pieces without
    // length drop out.
    auto pieces=[&](const QMap<qint64,QList<qint64>> &lines,bool column){
        for(auto it=lines.cbegin();it!=lines.cend();++it){
            auto at=it.value();std::sort(at.begin(),at.end());const double c=double(it.key()*p)/hatchUnit;
            for(qsizetype i=0;i+1<at.size();i+=2)if(at[i]!=at[i+1]){const double a=double(at[i])/hatchUnit,b=double(at[i+1])/hatchUnit;out.append(column?QLineF(c,a,c,b):QLineF(a,c,b,c));}
        }
    };
    pieces(columns,true);pieces(rows,false);
    return out;
}
QRectF bounds(const Element &e){
    QRectF r=copperShape(e).boundingRect();
    if(e.type==ElementType::Pad)r=r.united(QRectF(e.pos-QPointF(e.size2/2,e.size2/2),QSizeF(e.size2,e.size2)));
    return r;
}

// Tracks and areas have no reference point of their own: only their nodes move.
static bool hasPosition(const Element &e){return e.type!=ElementType::Track&&e.type!=ElementType::Area;}
void move(Element &e,QPointF delta){
    if(hasPosition(e))e.pos+=delta;e.points.translate(delta);for(auto &s:e.strokes)s.translate(delta);
}
void rotate(Element &e,QPointF pivot,double degrees){
    if(hasPosition(e))e.pos=turned(e.pos,pivot,degrees);for(auto &p:e.points)p=turned(p,pivot,degrees);
    for(auto &s:e.strokes)for(auto &p:s)p=turned(p,pivot,degrees);
    switch(e.type){
    case ElementType::Pad:case ElementType::SmdPad:case ElementType::Text:e.rotation=normalized(e.rotation+degrees);break;
    case ElementType::Circle:if(std::abs(normalized(e.stop-e.start))>1e-9){e.start=normalized(e.start+degrees);e.stop=normalized(e.stop+degrees);}break;
    default:break;
    }
    if(e.role==TextRole::Designator){
        // The rotation for pick and place counts clockwise on the bottom side; the offset turns with the component.
        const bool bottom=e.layer==SilkBottom||e.layer==CopperBottom;e.componentRotation=normalized(e.componentRotation+(bottom?-degrees:degrees));
        const double a=qDegreesToRadians(degrees),c=std::cos(a),s=std::sin(a);const QPointF o=e.pickOffset;
        e.pickOffset=QPointF(o.x()*c-o.y()*s,o.x()*s+o.y()*c);
        if(std::abs(e.pickOffset.x())<1e-12)e.pickOffset.setX(0);if(std::abs(e.pickOffset.y())<1e-12)e.pickOffset.setY(0);
    }
}
void mirror(Element &e,double axis){
    auto flip=[axis](QPointF p){return QPointF(2*axis-p.x(),p.y());};
    if(hasPosition(e))e.pos=flip(e.pos);for(auto &p:e.points)p=flip(p);for(auto &s:e.strokes)for(auto &p:s)p=flip(p);
    switch(e.type){
    case ElementType::Pad:case ElementType::SmdPad:{
        // The outline keeps starting at its top left corner: mirroring turns a turned pad the other way.
        e.rotation=normalized(-e.rotation);updateOutline(e);break;}
    case ElementType::Text:e.mirrored=!e.mirrored;e.rotation=normalized(-e.rotation);if(e.role==TextRole::Designator)e.pickOffset.setX(-e.pickOffset.x());break;
    case ElementType::Circle:if(std::abs(normalized(e.stop-e.start))>1e-9){const double a=e.start;e.start=normalized(180-e.stop);e.stop=normalized(180-a);}break;
    default:break;
    }
}
void removeElements(Board &board,QList<int> indexes){
    std::sort(indexes.begin(),indexes.end());indexes.erase(std::unique(indexes.begin(),indexes.end()),indexes.end());
    const QSet<int> gone(indexes.begin(),indexes.end());QList<int> map(board.elements.size(),-1);
    QList<Element> kept;for(int i=0;i<board.elements.size();i++)if(!gone.contains(i)){map[i]=int(kept.size());kept.append(board.elements[i]);}
    for(auto &e:kept){QList<int> c;for(int t:e.connections)if(t>=0&&t<map.size()&&map[t]>=0)c.append(map[t]);e.connections=c;
        for(int &p:e.autoroutePads)p=p>=0&&p<map.size()?map[p]:-1;}
    board.elements=kept;
}
void offsetLinks(Element &e,int base){for(int &c:e.connections)c+=base;for(int &p:e.autoroutePads)if(p>=0)p+=base;}
int nextGroup(const Board &board){int n=0;for(const auto &e:board.elements)for(int g:e.groups)n=std::max(n,g);return n+1;}
int freePart(QSet<int> &used){
    int n=1;for(int u:used)n=std::max(n,u+1);if(n>=maxPart){n=1;while(n<maxPart-1&&used.contains(n))n++;}
    used.insert(n);return n;
}
int nextPart(const Board &board){QSet<int> used;for(const auto &e:board.elements)if(e.part)used.insert(e.part);return freePart(used);}
QList<int> withGroups(const Board &board,QList<int> indexes){
    // Grown until nothing joins any more: an outer group can hold components, and the members of a component groups.
    QSet<int> outer,parts;const auto &els=board.elements;
    auto join=[&](const Element &e){
        bool more=false;if(!e.groups.isEmpty()&&!outer.contains(e.groups.last())){outer.insert(e.groups.last());more=true;}
        if(e.part&&!parts.contains(e.part)){parts.insert(e.part);more=true;}return more;
    };
    auto member=[&](const Element &e){return (!e.groups.isEmpty()&&outer.contains(e.groups.last()))||(e.part&&parts.contains(e.part));};
    bool grown=false;for(int i:indexes)if(i>=0&&i<els.size())grown|=join(els[i]);
    while(grown){grown=false;for(const auto &e:els)if(member(e))grown|=join(e);}
    if(!outer.isEmpty()||!parts.isEmpty())for(int i=0;i<els.size();i++)if(member(els[i]))indexes.append(i);
    std::sort(indexes.begin(),indexes.end());indexes.erase(std::unique(indexes.begin(),indexes.end()),indexes.end());return indexes;
}
void freshParts(QList<Element> &elements,const Board &board){
    QSet<int> taken;for(const auto &e:board.elements)if(e.part)taken.insert(e.part);
    QSet<int> used=taken;for(const auto &e:elements)if(e.part)used.insert(e.part);
    QMap<int,int> fresh;for(auto &e:elements)if(e.part&&taken.contains(e.part)){if(!fresh.contains(e.part))fresh[e.part]=freePart(used);e.part=fresh[e.part];}
}

QList<Component> components(const Board &b){
    // The elements by component number; a number without a designator text makes no component.
    QMap<int,Component> byPart;
    for(int i=0;i<b.elements.size();i++){
        const auto &e=b.elements[i];if(!e.part)continue;auto &c=byPart[e.part];c.part=e.part;c.members.append(i);
        if(e.type==ElementType::Text&&e.role==TextRole::Designator)c.designator=i;
        if(e.type==ElementType::Text&&e.role==TextRole::Value)c.value=i;
        if((e.type==ElementType::Pad||e.type==ElementType::SmdPad)&&!e.pin.isEmpty()&&!c.pins.contains(e.pin))c.pins.append(e.pin);
    }
    QList<Component> out;
    for(auto &c:byPart){
        if(c.designator<0)continue;c.id=b.elements[c.designator].component;
        // Pins in natural order: 2 before 10.
        std::sort(c.pins.begin(),c.pins.end(),[](const QString &x,const QString &y){bool a,b;const int u=x.toInt(&a),v=y.toInt(&b);return a&&b?u<v:a!=b?a:x<y;});
        out.append(c);
    }
    std::sort(out.begin(),out.end(),[](const Component &x,const Component &y){return x.designator<y.designator;});
    // Designators sort by their letters, then by number.
    static const QRegularExpression parts(QStringLiteral("^(\\D*)(\\d*)(.*)$"));
    auto key=[&](const Component &c){const auto m=parts.match(b.elements[c.designator].text);return std::make_tuple(m.captured(1),m.captured(2).toLongLong(),m.captured(3));};
    std::stable_sort(out.begin(),out.end(),[&](const Component &x,const Component &y){return key(x)<key(y);});
    return out;
}
QList<SelectorGroup> selectorGroups(const Board &b,ElementType type,int property,int layers,bool viasOnly){
    // Each element gets a sort key and a label; equal labels make one group.
    struct Key {double number=0;QString text;bool operator<(const Key &o) const{return number!=o.number?number<o.number:text<o.text;}};
    QMap<Key,SelectorGroup> groups;
    auto mm=[](double v){return uiLocale().toString(v,'f',2)+QStringLiteral(" mm");};
    static const QStringList forms{ui("rund"),ui("achteckig"),ui("quadratisch"),ui("quer, abgerundet"),ui("quer, achteckig"),ui("quer, rechteckig"),
                                   ui("hoch, abgerundet"),ui("hoch, achteckig"),ui("hoch, rechteckig")};
    for(int i=0;i<b.elements.size();i++){
        const auto &e=b.elements[i];if(e.type!=type||(viasOnly&&!(e.type==ElementType::Pad&&e.via)))continue;
        if((layers==1&&!isCopper(e.layer))||(layers==2&&e.layer!=SilkTop&&e.layer!=SilkBottom&&e.layer!=Outline))continue;
        Key key;QString label;
        switch(type){
        case ElementType::Pad:
            if(property==1){key.number=std::round(e.size2*1e4);label=mm(e.size2);}
            else if(property==2){key.number=int(e.shape);label=forms.value(int(e.shape)-1);}
            else if(property==3){key.number=e.via;label=e.via?ui("ja"):ui("nein");}
            else{key.number=std::round(e.size*1e4);label=mm(e.size);}
            break;
        case ElementType::SmdPad:key.number=std::round(e.size*1e4);key.text=QString::number(std::lround(e.size2*1e4)).rightJustified(12,'0');
            label=uiLocale().toString(e.size,'f',2)+QStringLiteral(" × ")+mm(e.size2);break;
        case ElementType::Circle:if(property==1){key.number=std::round(e.width*1e4);label=mm(e.width);}else{key.number=std::round(e.size*1e4);label=mm(e.size);}break;
        case ElementType::Text:if(property==1){key.text=e.text;label=e.text;}else{key.number=std::round(e.size*1e4);label=mm(e.size);}break;
        default:key.number=std::round(e.width*1e4);label=mm(e.width);break;
        }
        auto &g=groups[key];g.label=label;g.elements.append(i);
    }
    return groups.values();
}
QPointF componentCentre(const Board &b,const Component &c){
    QRectF pads,all;
    for(int i:c.members){const QRectF r=bounds(b.elements[i]);all=all.united(r);if(b.elements[i].type==ElementType::Pad||b.elements[i].type==ElementType::SmdPad)pads=pads.united(r);}
    return (pads.isNull()?all:pads).center();
}
bool componentOnTop(const Board &b,const Component &c){
    const int layer=b.elements[c.designator].layer;return layer==SilkTop||layer==CopperTop;
}
QPointF pickPlaceCentre(const Board &b,const Component &c){
    // The elements on the copper layers (inner ones too), on the silkscreen or on both; never texts, never the outline.
    const auto &id=b.elements[c.designator];
    auto counts=[mode=id.pickCentre](const Element &e){
        if(e.type==ElementType::Text)return false;const bool silk=e.layer==SilkTop||e.layer==SilkBottom;
        return mode==0?isCopper(e.layer):mode==1?silk:isCopper(e.layer)||silk;
    };
    // The rectangle lies in a frame turned with the component: the rotation in whole degrees (halves to even), its
    // remainder by 90° with the sign kept; the elements are turned counter-clockwise by it about the corner of the
    // working area, boxed, and the middle of the box is turned back.
    const double m=double(std::llrint(id.componentRotation)%90);
    auto box=[m](const Element &e){if(m==0)return bounds(e);Element t=e;rotate(t,{},m);return bounds(t);};
    QRectF r;for(int i:c.members)if(counts(b.elements[i]))r=r.united(box(b.elements[i]));
    if(r.isNull())for(int i:c.members){const auto &e=b.elements[i];if(e.type!=ElementType::Text||e.visible||e.role==TextRole::Plain)r=r.united(box(e));}
    const QPointF centre=m==0?r.center():turned(r.center(),{},-m);
    // The offset counts along the board's axes whatever the side and rotation.
    return centre+QPointF(id.pickOffset.x(),-id.pickOffset.y());
}
QString newId(){return QUuid::createUuid().toString(QUuid::Id128);}
// `used` holds the identifiers taken so far: two designators with one identifier are two components (a copy made by
// another program), and so are two boards.
static void assignIds(Board &b,QSet<QString> &boards,QSet<QString> &used){
    if(b.id.isEmpty()||boards.contains(b.id))b.id=newId();boards.insert(b.id);
    for(const auto &c:components(b)){
        auto &id=b.elements[c.designator].component;
        if(id.isEmpty()||used.contains(id))id=newId();used.insert(id);
        for(int i:c.members){auto &e=b.elements[i];if((e.type==ElementType::Pad||e.type==ElementType::SmdPad)&&e.pin.isEmpty())e.pin=e.name.trimmed();}
    }
}
void assignIds(Board &b){QSet<QString> boards,used;assignIds(b,boards,used);}
void assignIds(Document &d){QSet<QString> boards,used;for(auto &b:d.boards)assignIds(b,boards,used);}
QString componentOf(const Board &b,int element){
    if(element<0||element>=b.elements.size())return {};
    for(const auto &c:components(b))if(c.members.contains(element))return b.elements[c.designator].component;
    return {};
}
bool setComponentText(Board &b,const QString &id,const QString &designator,const QString &value){
    if(id.isEmpty())return false;
    for(const auto &c:components(b)){
        auto &d=b.elements[c.designator];if(d.component!=id)continue;
        if(d.text!=designator){d.text=designator;updateStrokes(d);}
        if(c.value>=0){auto &v=b.elements[c.value];if(v.text!=value){v.text=value;updateStrokes(v);}}
        else if(!value.isEmpty()){
            // A component without a value text gets one below its designator.
            Element v=newElement(ElementType::Text);v.layer=d.layer;v.role=TextRole::Value;v.text=value;v.size=d.size;v.style=d.style;v.thickness=d.thickness;
            v.rotation=d.rotation;v.mirrored=d.mirrored;v.pos=d.pos+QPointF(0,d.size*1.5);v.part=d.part;v.groups=d.groups;updateStrokes(v);b.elements.append(v);
        }
        return true;
    }
    return false;
}
Board newBoard(const QString &name,double width,double height){Board b;b.id=newId();b.name=name;b.width=width;b.height=height;b.origin={0,height};return b;}
Board newBoard(const QString &name,const NewBoard &shape){
    if(shape.kind==NewBoard::Plain){Board b=newBoard(name,shape.width,shape.height);if(shape.originTopLeft)b.origin={0,0};return b;}
    const bool round=shape.kind==NewBoard::Round;const double w=round?shape.diameter:shape.width,h=round?shape.diameter:shape.height,m=std::max(0.0,shape.margin);
    Board b=newBoard(name,w+2*m,h+2*m);b.origin={m,shape.originTopLeft?m:m+h};
    if(round){auto e=newElement(ElementType::Circle);e.layer=Outline;e.pos={m+w/2,m+h/2};e.size=w/2;e.width=shape.line;e.start=e.stop=0;b.elements<<e;}
    else{auto e=newElement(ElementType::Track);e.layer=Outline;e.width=shape.line;e.points={{m,m},{m+w,m},{m+w,m+h},{m,m+h},{m,m}};b.elements<<e;}
    return b;
}
Element newElement(ElementType type){
    Element e;e.type=type;
    switch(type){
    case ElementType::Pad:e.layer=CopperBottom;e.size=1.8;e.size2=.6;e.solderMask=true;break;
    case ElementType::SmdPad:e.layer=CopperTop;e.size=.9;e.size2=1.8;e.solderMask=true;break;
    case ElementType::Track:e.layer=CopperBottom;e.width=.8;break;
    case ElementType::Area:e.layer=CopperBottom;e.width=.4;break;
    case ElementType::Circle:e.layer=SilkTop;e.size=2;e.width=.4;break;
    case ElementType::Text:e.layer=SilkTop;e.size=2;break;
    }
    return e;
}
}
