// The own file format of the schematic module (.olsch): JSON, written and read here.
#include "model.h"
#include "language.h"
#include "legacy_reader.h"
#include "formats/splan/splan.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <cmath>

namespace openloch::schematic {
namespace {
// Limits: 10 m of coordinates, 100 000 nodes per element, 1 000 000 elements per sheet, nesting 32 deep.
constexpr double maxCoordinate=10000;
constexpr int maxNodes=100000,maxItems=1000000,maxDepth=32,maxSheets=10000;

[[noreturn]] void invalid(const QString &message){throw FormatError(message);}
[[noreturn]] void invalidItem(){invalid(ui("Ungültiges Element im Schaltplan"));}

const char *const penStyles[]={"solid","dash","dot","dashDot","dashDotDot","none"};
const char *const fillStyles[]={"none","solid","horizontal","vertical","cross","diagonal","backDiagonal","diagonalCross"};
const char *const lineEnds[]={"none","triangle","arrow","openArrow","circle","dot","diamond","filledDiamond","square","filledSquare","bar","backArrow","backTriangle"};
const char *const aligns[]={"left","centre","right"};
const char *const arcs[]={"full","arc","pie","chord"};
const char *const cornerStyles[]={"square","round","bevel"};
const char *const roles[]={"plain","designator","value"};
const char *const types[]={"line","polygon","bezier","rectangle","ellipse","text","textBox","junction","image","group","component","contact","netLabel","dimension"};
const char *const dimensionKinds[]={"standard","radial","diameter","angle"};
const char *const scaleUnits[]={"mm","cm","m","km"};
const char *const orientations[]={"automatic","portrait","landscape"};
template<class E,size_t N>
QString enumName(E value,const char *const (&names)[N]){const auto i=size_t(int(value));return QString::fromLatin1(names[i<N?i:0]);}
template<class E,size_t N>
E enumValue(const QJsonObject &o,const char *key,E fallback,const char *const (&names)[N]){
    if(!o.contains(key))return fallback;
    const auto v=o[key];if(v.isString())for(size_t i=0;i<N;i++)if(v.toString()==QLatin1String(names[i]))return E(i);
    invalidItem();
}

QJsonArray point(QPointF p){return QJsonArray{p.x(),p.y()};}
QJsonArray pointList(const QPolygonF &points){QJsonArray a;for(auto p:points)a.append(point(p));return a;}
QString colour(const QColor &c){return c.name(QColor::HexRgb);}

double number(const QJsonObject &o,const char *key,double fallback,double low,double high){
    if(!o.contains(key))return fallback;
    const auto v=o[key];if(!v.isDouble()||!std::isfinite(v.toDouble())||v.toDouble()<low||v.toDouble()>high)invalid(ui("Ungültiger Zahlenwert im Schaltplan"));
    return v.toDouble();
}
int integer(const QJsonObject &o,const char *key,int fallback,int low,int high){
    const double v=number(o,key,fallback,low,high);if(v!=std::floor(v))invalid(ui("Ungültiger Zahlenwert im Schaltplan"));return int(v);
}
bool flag(const QJsonObject &o,const char *key,bool fallback){
    if(!o.contains(key))return fallback;if(!o[key].isBool())invalid(ui("Ungültiger Schalter im Schaltplan"));return o[key].toBool();
}
QString text(const QJsonObject &o,const char *key,int limit=65536){
    if(!o.contains(key))return {};if(!o[key].isString()||o[key].toString().size()>limit)invalid(ui("Ungültiger Text im Schaltplan"));return o[key].toString();
}
QColor colourValue(const QJsonObject &o,const char *key,QColor fallback){
    if(!o.contains(key))return fallback;
    static const QRegularExpression pattern(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    if(!o[key].isString()||!pattern.match(o[key].toString()).hasMatch())invalid(ui("Ungültige Farbe im Schaltplan"));
    return QColor(o[key].toString());
}
QPointF pointValue(const QJsonValue &v){
    const auto a=v.toArray();if(!v.isArray()||a.size()!=2)invalid(ui("Ungültiger Punkt im Schaltplan"));
    QPointF p;
    for(int i=0;i<2;i++){
        const auto c=a[i];if(!c.isDouble()||!std::isfinite(c.toDouble())||std::abs(c.toDouble())>maxCoordinate)invalid(ui("Ungültiger Punkt im Schaltplan"));
        (i?p.ry():p.rx())=c.toDouble();
    }
    return p;
}
QPointF pointOf(const QJsonObject &o,const char *key,QPointF fallback={}){return o.contains(key)?pointValue(o[key]):fallback;}
QPolygonF points(const QJsonObject &o,const char *key){
    QPolygonF out;if(!o.contains(key))return out;const auto a=o[key].toArray();
    if(!o[key].isArray()||a.size()>maxNodes)invalid(ui("Ungültiger Punkt im Schaltplan"));
    for(const auto &v:a)out<<pointValue(v);
    return out;
}
QSizeF sizeOf(const QJsonObject &o){
    if(!o.contains("size"))return {};
    const auto p=pointValue(o["size"]);if(p.x()<0||p.y()<0)invalid(ui("Ungültiger Zahlenwert im Schaltplan"));return QSizeF(p.x(),p.y());
}

QJsonObject penJson(const Pen &p){
    return {{"color",colour(p.color)},{"width",p.width},{"style",enumName(p.style,penStyles)},{"twoColour",p.twoColour},{"color2",colour(p.color2)},
            {"inner",p.inner},{"innerColor",colour(p.innerColor)},{"cross",p.cross},{"crossColor",colour(p.crossColor)}};
}
// Second colour and stripes since version 7.
Pen penOf(const QJsonObject &o,int version){
    if(!o.contains("pen"))return {};if(!o["pen"].isObject())invalidItem();const auto j=o["pen"].toObject();
    Pen p;p.color=colourValue(j,"color",p.color);p.width=number(j,"width",p.width,0,100);p.style=enumValue(j,"style",p.style,penStyles);
    if(version>=7){
        p.twoColour=flag(j,"twoColour",false);p.color2=colourValue(j,"color2",p.color2);p.inner=flag(j,"inner",false);p.innerColor=colourValue(j,"innerColor",p.innerColor);
        p.cross=flag(j,"cross",false);p.crossColor=colourValue(j,"crossColor",p.crossColor);
    }
    return p;
}
QJsonObject fillJson(const Fill &f){return {{"style",enumName(f.style,fillStyles)},{"color",colour(f.color)},{"spacing",f.spacing},{"lineWidth",f.lineWidth}};}
Fill fillOf(const QJsonObject &o){
    if(!o.contains("fill"))return {};if(!o["fill"].isObject())invalidItem();const auto j=o["fill"].toObject();
    Fill f;f.style=enumValue(j,"style",f.style,fillStyles);f.color=colourValue(j,"color",f.color);
    f.spacing=number(j,"spacing",f.spacing,.01,1000);f.lineWidth=number(j,"lineWidth",f.lineWidth,0,100);return f;
}
QJsonObject fontJson(const Font &f){
    return {{"family",f.family},{"height",f.height},{"bold",f.bold},{"italic",f.italic},{"underline",f.underline},{"strikeOut",f.strikeOut},{"color",colour(f.color)}};
}
// Struck out since version 7.
Font fontOf(const QJsonObject &o,int version){
    if(!o.contains("font"))return {};if(!o["font"].isObject())invalidItem();const auto j=o["font"].toObject();
    Font f;if(j.contains("family"))f.family=text(j,"family",256);f.height=number(j,"height",f.height,.01,1000);
    f.bold=flag(j,"bold",false);f.italic=flag(j,"italic",false);f.underline=flag(j,"underline",false);f.color=colourValue(j,"color",f.color);
    if(version>=7)f.strikeOut=flag(j,"strikeOut",false);
    return f;
}

QString base64(const QByteArray &b){return QString::fromLatin1(b.toBase64());}
QByteArray fromBase64(const QJsonObject &o,const char *key){
    if(!o.contains(key))return {};
    if(!o[key].isString())invalid(ui("Ungültiger Text im Schaltplan"));
    const auto r=QByteArray::fromBase64Encoding(o[key].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(!r)invalid(ui("Ungültiger Text im Schaltplan"));
    return *r;
}
QJsonObject itemJson(const Item &i){
    // Items of library pages may have no id yet.
    QJsonObject o{{"type",enumName(i.type,types)}};
    if(!i.id.isEmpty())o["id"]=i.id;
    if(!i.splan.isEmpty())o["splan"]=base64(i.splan);
    auto textFields=[&]{o["text"]=i.text;o["font"]=fontJson(i.font);o["align"]=enumName(i.align,aligns);};
    auto box=[&]{o["centre"]=point(i.centre);o["size"]=point(QPointF(i.size.width(),i.size.height()));o["rotation"]=i.rotation;};
    auto children=[&]{QJsonArray a;for(const auto &c:i.children)a.append(itemJson(c));o["children"]=a;};
    switch(i.type){
    case ItemType::Line:
        o["points"]=pointList(i.points);o["pen"]=penJson(i.pen);o["startEnd"]=enumName(i.startEnd,lineEnds);o["endEnd"]=enumName(i.endEnd,lineEnds);
        o["endSize"]=i.endSize;o["electrical"]=i.electrical;break;
    case ItemType::Polygon:o["points"]=pointList(i.points);o["pen"]=penJson(i.pen);o["fill"]=fillJson(i.fill);break;
    case ItemType::Bezier:
        o["points"]=pointList(i.points);o["pen"]=penJson(i.pen);o["startEnd"]=enumName(i.startEnd,lineEnds);o["endEnd"]=enumName(i.endEnd,lineEnds);o["endSize"]=i.endSize;break;
    case ItemType::Rectangle:box();o["pen"]=penJson(i.pen);o["fill"]=fillJson(i.fill);o["corners"]=enumName(i.corners,cornerStyles);o["corner"]=i.corner;break;
    case ItemType::Ellipse:box();o["pen"]=penJson(i.pen);o["fill"]=fillJson(i.fill);o["start"]=i.start;o["stop"]=i.stop;o["arc"]=enumName(i.arc,arcs);break;
    case ItemType::Text:
        textFields();o["pos"]=point(i.pos);o["rotation"]=i.rotation;o["mirrored"]=i.mirrored;o["role"]=enumName(i.role,roles);
        o["background"]=i.background;o["backgroundColor"]=colour(i.backgroundColor);o["visible"]=i.visible;
        if(!i.link.isEmpty())o["link"]=i.link;
        if(!i.linkTarget.isEmpty())o["linkTarget"]=i.linkTarget;
        o["linkable"]=i.linkable;o["reversed"]=i.reversed;break;
    case ItemType::TextBox:box();textFields();o["pen"]=penJson(i.pen);o["fill"]=fillJson(i.fill);o["wrap"]=i.wrap;o["middle"]=i.middle;break;
    case ItemType::Junction:o["pos"]=point(i.pos);o["diameter"]=i.size.width();o["color"]=colour(i.pen.color);o["autoSize"]=i.autoSize;o["sizeStep"]=i.sizeStep;break;
    case ItemType::Image:box();o["resource"]=i.resource;break;
    case ItemType::Group:children();break;
    case ItemType::Component:{
        o["pos"]=point(i.pos);o["rotation"]=i.rotation;o["mirrored"]=i.mirrored;o["designator"]=i.designator;o["value"]=i.value;
        QJsonArray extra;for(const auto &e:i.extra)extra.append(e);o["extra"]=extra;
        o["designatorVisible"]=i.designatorVisible;o["valueVisible"]=i.valueVisible;o["autoNumber"]=i.autoNumber;o["askValue"]=i.askValue;
        o["inPartsList"]=i.inPartsList;o["caption"]=i.caption;o["libraryEntry"]=i.libraryEntry;o["parent"]=i.parent;
        if(!i.parentId.isEmpty())o["parentId"]=i.parentId;
        children();break;
    }
    case ItemType::Contact:
        textFields();o["pos"]=point(i.pos);o["rotation"]=i.rotation;o["name"]=i.name;o["visible"]=i.visible;
        if(i.hasPin)o["pin"]=point(i.pin);
        break;
    case ItemType::NetLabel:textFields();o["pos"]=point(i.pos);o["rotation"]=i.rotation;o["global"]=i.global;break;
    case ItemType::Dimension:
        o["kind"]=enumName(i.dimension,dimensionKinds);o["points"]=pointList(i.points);o["offset"]=i.offset;
        o["arrowAngle"]=i.arrowAngle;o["arrowLength"]=i.arrowLength;o["extensionColor"]=colour(i.extensionColor);o["lineColor"]=colour(i.lineColor);
        o["digits"]=i.digits;o["decimalPoint"]=i.decimalPoint;o["showDiameter"]=i.showDiameter;o["auto"]=i.autoValue;o["fixedValue"]=i.fixedValue;
        o["prefix"]=i.prefix;o["suffix"]=i.suffix;o["upperTolerance"]=i.upperTolerance;o["lowerTolerance"]=i.lowerTolerance;o["font"]=fontJson(i.font);
        break;
    }
    return o;
}

struct Reader {
    int version=formatVersion;
    QSet<QString> ids;
    int count=0;
    QSet<QString> resources;
    bool requireIds=true;
    bool anyResource=false;         // a single element: its pictures are checked with the document
    QSet<QString> componentIds,textIds;
    QList<std::pair<QString,QString>> parentLinks,textLinks;  // child and parent, text and target
    QString identifier(const QJsonObject &o){
        static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_.-]{1,64}$"));
        if(!requireIds&&!o.contains("id"))return newId();
        const QString id=text(o,"id",64);
        if(!pattern.match(id).hasMatch())invalid(ui("Ungültige Kennung im Schaltplan"));
        if(ids.contains(id))invalid(ui("Eine Kennung kommt im Schaltplan doppelt vor: %1").arg(id));
        ids.insert(id);return id;
    }
    Item item(const QJsonValue &value,int depth,bool inComponent){
        if(!value.isObject())invalidItem();
        if(depth>maxDepth||++count>maxItems)invalid(ui("Der Schaltplan ist zu tief verschachtelt oder zu groß"));
        const auto o=value.toObject();Item i;i.id=identifier(o);
        if(!o["type"].isString())invalidItem();
        i.type=enumValue(o,"type",ItemType::Line,types);
        auto textFields=[&]{i.text=text(o,"text");i.font=fontOf(o,version);i.align=enumValue(o,"align",i.align,aligns);};
        auto rotation=[&]{i.rotation=number(o,"rotation",0,-360,720);};
        auto box=[&]{i.centre=pointOf(o,"centre");i.size=sizeOf(o);rotation();};
        auto children=[&](bool component){
            if(!o.contains("children"))return;if(!o["children"].isArray())invalidItem();
            for(const auto &c:o["children"].toArray())i.children.append(item(c,depth+1,component));
        };
        auto lineEndsOf=[&]{
            // Version 1 knew one square, filled.
            auto end=[&](const char *key){const LineEnd e=enumValue(o,key,LineEnd::None,lineEnds);return version<2&&e==LineEnd::Square?LineEnd::FilledSquare:e;};
            i.startEnd=end("startEnd");i.endEnd=end("endEnd");i.endSize=number(o,"endSize",i.endSize,0,1000);
        };
        i.splan=fromBase64(o,"splan");
        switch(i.type){
        case ItemType::Line:
            i.points=points(o,"points");if(i.points.size()<2)invalid(ui("Eine Linie braucht mindestens zwei Punkte"));
            i.pen=penOf(o,version);lineEndsOf();i.electrical=flag(o,"electrical",true);break;
        case ItemType::Polygon:i.points=points(o,"points");if(i.points.size()<3)invalid(ui("Ein Polygon braucht mindestens drei Punkte"));i.pen=penOf(o,version);i.fill=fillOf(o);break;
        case ItemType::Bezier:
            i.points=points(o,"points");if(i.points.size()<4||(i.points.size()-1)%3)invalid(ui("Ungültige Bezierkurve im Schaltplan"));
            i.pen=penOf(o,version);lineEndsOf();break;
        case ItemType::Rectangle:box();i.pen=penOf(o,version);i.fill=fillOf(o);i.corners=enumValue(o,"corners",i.corners,cornerStyles);i.corner=number(o,"corner",0,0,50);break;
        case ItemType::Ellipse:
            box();i.pen=penOf(o,version);i.fill=fillOf(o);i.start=number(o,"start",0,-720,720);i.stop=number(o,"stop",360,-720,1080);i.arc=enumValue(o,"arc",i.arc,arcs);break;
        case ItemType::Text:
            textFields();i.pos=pointOf(o,"pos");rotation();i.mirrored=flag(o,"mirrored",false);i.role=enumValue(o,"role",i.role,roles);
            i.background=flag(o,"background",false);i.backgroundColor=colourValue(o,"backgroundColor",i.backgroundColor);i.visible=flag(o,"visible",true);
            // Version 4: links.
            i.link=text(o,"link",4096);i.linkTarget=text(o,"linkTarget",64);i.linkable=flag(o,"linkable",false);
            // Version 13: the writing direction.
            if(version>=13)i.reversed=flag(o,"reversed",false);
            textIds.insert(i.id);if(!i.linkTarget.isEmpty())textLinks.append({i.id,i.linkTarget});
            if(i.role!=TextRole::Plain&&!inComponent)invalid(ui("Bezeichner und Wert gibt es nur in Bauteilen"));
            break;
        case ItemType::TextBox:box();textFields();i.pen=penOf(o,version);i.fill=fillOf(o);i.wrap=flag(o,"wrap",true);i.middle=flag(o,"middle",false);break;
        case ItemType::Junction:{
            i.pos=pointOf(o,"pos");const double d=number(o,"diameter",1,0,1000);i.size=QSizeF(d,d);
            i.pen.color=colourValue(o,"color",i.pen.color);i.autoSize=flag(o,"autoSize",true);
            // Version 13: the step of the automatic size (before it, L).
            if(version>=13)i.sizeStep=integer(o,"sizeStep",3,0,4);
            break;
        }
        case ItemType::Image:
            box();i.resource=text(o,"resource",64);if(!anyResource&&!resources.contains(i.resource))invalid(ui("Ein Bild des Schaltplans fehlt"));break;
        case ItemType::Group:children(inComponent);break;
        case ItemType::Component:{
            if(inComponent)invalid(ui("Ein Bauteil kann kein weiteres Bauteil enthalten"));
            i.pos=pointOf(o,"pos");rotation();i.mirrored=flag(o,"mirrored",false);i.designator=text(o,"designator",4096);i.value=text(o,"value",4096);
            if(o.contains("extra")){
                if(!o["extra"].isArray()||o["extra"].toArray().size()>4)invalidItem();
                for(const auto &e:o["extra"].toArray()){if(!e.isString())invalidItem();i.extra.append(e.toString());}
            }
            i.designatorVisible=flag(o,"designatorVisible",true);i.valueVisible=flag(o,"valueVisible",true);i.autoNumber=flag(o,"autoNumber",true);
            i.askValue=flag(o,"askValue",false);i.inPartsList=flag(o,"inPartsList",true);i.caption=text(o,"caption",4096);i.libraryEntry=text(o,"libraryEntry",4096);
            // Version 3: parent and child.
            i.parent=flag(o,"parent",false);i.parentId=text(o,"parentId",64);
            componentIds.insert(i.id);if(!i.parentId.isEmpty())parentLinks.append({i.id,i.parentId});
            children(true);break;
        }
        case ItemType::Contact:
            if(!inComponent)invalid(ui("Kontakte gibt es nur in Bauteilen"));
            textFields();i.pos=pointOf(o,"pos");rotation();i.name=text(o,"name",4096);i.visible=flag(o,"visible",true);
            i.hasPin=o.contains("pin");i.pin=pointOf(o,"pin");break;
        case ItemType::NetLabel:
            textFields();i.pos=pointOf(o,"pos");rotation();i.global=flag(o,"global",false);
            if(i.text.trimmed().isEmpty())invalid(ui("Ein Netzname darf nicht leer sein"));
            break;
        case ItemType::Dimension:
            // Version 6: dimensions.
            if(version<6)invalidItem();
            i.dimension=enumValue(o,"kind",i.dimension,dimensionKinds);i.points=points(o,"points");
            if(i.points.size()<2||i.points.size()>3||(i.dimension==DimensionKind::Angle&&i.points.size()!=3))invalid(ui("Ungültige Bemaßung im Schaltplan"));
            i.offset=number(o,"offset",0,-maxCoordinate,maxCoordinate);i.arrowAngle=number(o,"arrowAngle",15,1,170);i.arrowLength=number(o,"arrowLength",2.8,0,1000);
            i.extensionColor=colourValue(o,"extensionColor",i.extensionColor);i.lineColor=colourValue(o,"lineColor",i.lineColor);
            i.digits=integer(o,"digits",1,0,6);i.decimalPoint=flag(o,"decimalPoint",false);i.showDiameter=flag(o,"showDiameter",false);i.autoValue=flag(o,"auto",true);
            i.fixedValue=text(o,"fixedValue",4096);i.prefix=text(o,"prefix",4096);i.suffix=text(o,"suffix",4096);
            i.upperTolerance=text(o,"upperTolerance",4096);i.lowerTolerance=text(o,"lowerTolerance",4096);i.font=fontOf(o,version);
            break;
        }
        return i;
    }
    QList<Item> items(const QJsonObject &o,const char *key){
        QList<Item> out;if(!o.contains(key))return out;
        if(!o[key].isArray())invalidItem();
        for(const auto &v:o[key].toArray())out.append(item(v,0,false));
        return out;
    }
};
}

QJsonObject itemToJson(const Item &item){return itemJson(item);}
Item itemFromJson(const QJsonValue &value,bool ids){
    Reader reader;reader.requireIds=ids;reader.anyResource=true;return reader.item(value,0,false);
}

namespace {
// Links to parents that are no longer there (a parent deleted, a child pasted elsewhere) are not written.
void componentIdsOf(const QList<Item> &items,QSet<QString> &ids,QSet<QString> &texts){
    for(const auto &i:items){if(i.type==ItemType::Component)ids.insert(i.id);if(i.type==ItemType::Text)texts.insert(i.id);componentIdsOf(i.children,ids,texts);}
}
void dropLostParents(QList<Item> &items,const QSet<QString> &ids,const QSet<QString> &texts){
    for(auto &i:items){
        if(!i.parentId.isEmpty()&&(i.parentId==i.id||!ids.contains(i.parentId)))i.parentId.clear();
        if(!i.linkTarget.isEmpty()&&(i.linkTarget==i.id||!texts.contains(i.linkTarget)))i.linkTarget.clear();
        dropLostParents(i.children,ids,texts);
    }
}
}
QJsonObject toJson(const Document &original){
    QSet<QString> ids,texts;for(const auto &s:original.sheets){componentIdsOf(s.items,ids,texts);componentIdsOf(s.titleBlock.items,ids,texts);}
    Document document=original;
    for(auto &s:document.sheets){dropLostParents(s.items,ids,texts);dropLostParents(s.titleBlock.items,ids,texts);}
    QJsonArray sheets;
    for(const auto &s:document.sheets){
        QJsonArray items;for(const auto &i:s.items)items.append(itemJson(i));
        QJsonArray form;for(const auto &i:s.titleBlock.items)form.append(itemJson(i));
        const auto &t=s.titleBlock;
        QJsonObject title{{"name",t.name},{"frame",QJsonArray{t.frame.x(),t.frame.y(),t.frame.width(),t.frame.height()}},{"columns",t.columns},{"rows",t.rows},
                          {"columnStart",t.columnStart},{"rowStart",t.rowStart},{"showGrid",t.showGrid},{"items",form}};
        QJsonObject sheet{{"id",s.id},{"name",s.name},{"description",s.description},{"width",s.width},{"height",s.height},{"grid",s.grid},{"spare",s.spare},
                          {"titleBlock",title},{"items",items}};
        if(!s.horizontalGuides.isEmpty()||!s.verticalGuides.isEmpty()){
            QJsonArray h,v;for(double y:s.horizontalGuides)h.append(y);for(double x:s.verticalGuides)v.append(x);
            sheet["guides"]=QJsonObject{{"horizontal",h},{"vertical",v}};
        }
        if(s.scale!=1)sheet["scale"]=s.scale;
        if(s.scaleUnit!=ScaleUnit::Millimetre)sheet["scaleUnit"]=enumName(s.scaleUnit,scaleUnits);
        if(!(s.print==PrintSettings())){
            const PrintSettings &p=s.print;
            sheet["print"]=QJsonObject{{"free",p.free},{"scale",p.scale},{"offset",QJsonArray{p.offset.x(),p.offset.y()}},{"orientation",enumName(p.orientation,orientations)},
                                       {"pages",QJsonArray{p.bannerX,p.bannerY}},{"overlap",p.overlap}};
        }
        if(!s.splan.isEmpty())sheet["splan"]=base64(s.splan);
        sheets.append(sheet);
    }
    QJsonArray variables;for(const auto &v:document.variables)variables.append(QJsonObject{{"name",v.name},{"value",v.value}});
    QJsonObject resources;
    for(auto it=document.resources.cbegin();it!=document.resources.cend();++it)
        resources[it.key()]=QJsonObject{{"kind",it->kind},{"data",QString::fromLatin1(it->data.toBase64())}};
    QJsonObject o{{"format","OpenLoch Schematic"},{"version",formatVersion},{"unit","mm"},{"id",document.id},{"activeSheet",document.activeSheet},
                  {"variables",variables},{"resources",resources},{"sheets",sheets}};
    if(!document.sheetNumbers)o["sheetNumbers"]=false;
    if(document.designatorPageNumbers)o["designatorPageNumbers"]=true;
    if(!document.designatorPrefix.isEmpty())o["designatorPrefix"]=document.designatorPrefix;
    if(!document.splan.isEmpty())o["splan"]=base64(document.splan);
    return o;
}

Document fromJson(const QJsonObject &json){
    if(json["format"]!="OpenLoch Schematic")invalid(ui("Keine OpenLoch-Schaltplandatei"));
    if(!json["version"].isDouble()||json["version"].toDouble()!=std::floor(json["version"].toDouble())||json["version"].toInt()<1)
        invalid(ui("Diese Version der Schaltplandatei wird nicht unterstützt"));
    if(json["version"].toInt()>formatVersion)invalid(ui("Die Schaltplandatei stammt von einer neueren Version von OpenLoch und kann nicht gelesen werden"));
    if(json.contains("unit")&&json["unit"]!="mm")invalid(ui("Unbekannte Einheit im Schaltplan"));
    Document d;Reader reader;reader.version=json["version"].toInt();
    d.id=text(json,"id",128);d.splan=fromBase64(json,"splan");
    // Version 10: sheet numbers on the tabs and designators with prefix and sheet number (before: tabs numbered,
    // designators as entered).
    if(reader.version>=10){
        d.sheetNumbers=flag(json,"sheetNumbers",true);d.designatorPageNumbers=flag(json,"designatorPageNumbers",false);
        d.designatorPrefix=text(json,"designatorPrefix",256);
    }
    // Version 1 has one grid for the document.
    const double documentGrid=number(json,"grid",1.27,.001,1000);
    if(json.contains("variables")){
        if(!json["variables"].isArray())invalid(ui("Ungültige Variable im Schaltplan"));
        for(const auto &v:json["variables"].toArray()){
            if(!v.isObject())invalid(ui("Ungültige Variable im Schaltplan"));
            d.variables.append({text(v.toObject(),"name",256),text(v.toObject(),"value")});
        }
    }
    if(json.contains("resources")){
        if(!json["resources"].isObject())invalid(ui("Ein Bild des Schaltplans fehlt"));
        const auto r=json["resources"].toObject();
        for(auto it=r.begin();it!=r.end();++it){
            const auto o=it.value().toObject();const QString kind=text(o,"kind",8);
            if(kind!=u"png"&&kind!=u"jpg"&&kind!=u"bmp")invalid(ui("Ungültiges Bild im Schaltplan"));
            const auto data=QByteArray::fromBase64(text(o,"data",128*1024*1024).toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
            if(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex()!=it.key().toLatin1())invalid(ui("Ungültiges Bild im Schaltplan"));
            d.resources.insert(it.key(),{kind,data});reader.resources.insert(it.key());
        }
    }
    const auto sheets=json["sheets"].toArray();
    if(!json["sheets"].isArray()||sheets.isEmpty()||sheets.size()>maxSheets)invalid(ui("Der Schaltplan enthält kein Blatt"));
    for(const auto &value:sheets){
        if(!value.isObject())invalid(ui("Ungültiges Blatt im Schaltplan"));
        const auto o=value.toObject();Sheet s;s.id=reader.identifier(o);s.name=text(o,"name",4096);s.description=text(o,"description");
        s.width=number(o,"width",297,1,maxCoordinate);s.height=number(o,"height",210,1,maxCoordinate);s.spare=flag(o,"spare",false);s.splan=fromBase64(o,"splan");
        s.grid=number(o,"grid",documentGrid,.001,1000);
        // Version 6: the sheet's scale.
        if(reader.version>=6)s.scale=number(o,"scale",1,.001,1000000);
        // Version 8: its unit.
        if(reader.version>=8)s.scaleUnit=enumValue(o,"scaleUnit",ScaleUnit::Millimetre,scaleUnits);
        // Version 9: how it is printed.
        if(reader.version>=9&&o.contains("print")){
            if(!o["print"].isObject())invalid(ui("Ungültiges Blatt im Schaltplan"));
            const auto p=o["print"].toObject();PrintSettings &ps=s.print;
            ps.free=flag(p,"free",false);ps.scale=number(p,"scale",1,.01,100);ps.orientation=enumValue(p,"orientation",ps.orientation,orientations);
            if(p.contains("offset"))ps.offset=pointValue(p["offset"]);
            if(p.contains("pages")){const QPointF n=pointValue(p["pages"]);
                if(n.x()<1||n.y()<1||n.x()>100||n.y()>100||n.x()!=std::floor(n.x())||n.y()!=std::floor(n.y()))invalid(ui("Ungültiger Zahlenwert im Schaltplan"));
                ps.bannerX=int(n.x());ps.bannerY=int(n.y());}
            ps.overlap=number(p,"overlap",0,0,1000);
        }
        if(reader.version>=5&&o.contains("guides")){
            // Version 4 and older have no guide lines.
            if(!o["guides"].isObject())invalid(ui("Ungültiges Blatt im Schaltplan"));
            const auto g=o["guides"].toObject();
            for(auto [key,list]:{std::pair{"horizontal",&s.horizontalGuides},std::pair{"vertical",&s.verticalGuides}}){
                if(!g.contains(key))continue;
                const auto a=g[key].toArray();if(!g[key].isArray()||a.size()>10000)invalid(ui("Ungültiges Blatt im Schaltplan"));
                for(const auto &v:a){if(!v.isDouble()||!std::isfinite(v.toDouble())||std::abs(v.toDouble())>maxCoordinate)invalid(ui("Ungültiges Blatt im Schaltplan"));*list<<v.toDouble();}
            }
        }
        if(o.contains("titleBlock")){
            if(!o["titleBlock"].isObject())invalid(ui("Ungültiges Blatt im Schaltplan"));
            const auto t=o["titleBlock"].toObject();s.titleBlock.name=text(t,"name",4096);
            if(t.contains("frame")){
                const auto f=t["frame"].toArray();if(!t["frame"].isArray()||f.size()!=4)invalid(ui("Ungültiges Blatt im Schaltplan"));
                double v[4];for(int k=0;k<4;k++){if(!f[k].isDouble()||!std::isfinite(f[k].toDouble())||std::abs(f[k].toDouble())>maxCoordinate)invalid(ui("Ungültiges Blatt im Schaltplan"));v[k]=f[k].toDouble();}
                if(v[2]<0||v[3]<0)invalid(ui("Ungültiges Blatt im Schaltplan"));
                s.titleBlock.frame=QRectF(v[0],v[1],v[2],v[3]);
            }
            s.titleBlock.columns=integer(t,"columns",0,0,1000);s.titleBlock.rows=integer(t,"rows",0,0,1000);s.titleBlock.showGrid=flag(t,"showGrid",false);
            if(reader.version>=12){s.titleBlock.columnStart=integer(t,"columnStart",1,1,10000);s.titleBlock.rowStart=integer(t,"rowStart",1,1,10000);}
            s.titleBlock.items=reader.items(t,"items");
        }
        s.items=reader.items(o,"items");
        // Versions 9 and 10 kept the print offset of the sheet's corner from the paper's; since version 11 it is the
        // printed content's from the printable area's (the paper's corner taken for it here).
        if(reader.version<11&&!s.print.offset.isNull()){
            QRectF content;for(const auto &i:s.titleBlock.items)content|=bounds(i);for(const auto &i:s.items)content|=bounds(i);
            if(!content.isNull())s.print.offset+=content.topLeft()*s.print.factor();
        }
        d.sheets.append(s);
    }
    d.activeSheet=integer(json,"activeSheet",0,0,int(d.sheets.size())-1);
    for(const auto &[child,parent]:reader.parentLinks)
        if(parent==child||!reader.componentIds.contains(parent))invalid(ui("Ein Bauteil verweist auf ein fehlendes Parent-Bauteil"));
    for(const auto &[from,to]:reader.textLinks)
        if(from==to||!reader.textIds.contains(to))invalid(ui("Ein Text verweist auf ein fehlendes Linkziel"));
    if(d.id.isEmpty())d.id=newId();
    return d;
}

QByteArray encode(const Document &document){return QJsonDocument(toJson(document)).toJson(QJsonDocument::Indented);}
Document decode(const QByteArray &bytes){
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())invalid(ui("Die Schaltplandatei ist beschädigt"));
    return fromJson(doc.object());
}
Document load(const QString &path){
    QFile f(path);if(!f.open(QIODevice::ReadOnly))throw FormatError(f.errorString());
    if(f.size()>128*1024*1024)invalid(ui("Datei ist zu groß"));
    const QByteArray bytes=f.readAll();
    if(splan::version(bytes))return splan::read(bytes);
    return decode(bytes);
}
void save(const Document &document,const QString &path){
    Document complete=document;completeIds(complete);
    const auto data=encode(complete);
    // Read back first: nothing is written that the reader would not take back unchanged.
    if(encode(decode(data))!=data)invalid(ui("Der Schaltplan lässt sich nicht verlustfrei speichern"));
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))throw FormatError(file.errorString());
    if(file.write(data)!=data.size()||!file.commit())throw FormatError(file.errorString());
}
}
