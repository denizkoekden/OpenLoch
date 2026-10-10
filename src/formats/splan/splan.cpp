#include "splan.h"
#include "records.h"
#include "language.h"
#include "legacy_reader.h"
#include "modules/schematic/text.h"
#include "modules/schematic/dimension.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineF>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QSaveFile>
#include <QUuid>
#include <QtEndian>
#include <QtMath>
#include <cmath>
#include <cstring>
#include <numbers>
#include <functional>

namespace openloch::splan {
using namespace schematic;
namespace {
constexpr double unit=10;                               // file units (tenths of a millimetre) per millimetre
constexpr qint64 automaticSize=1209250;                 // a junction's automatic size: step plus this in the outline colour
// Arrow sizes XS to XXL: the length of an arrow is the line's width (at least 0.2 mm) times this factor.
const double endFactors[]={5,6,8,12,17,24};
double arrowWidth(double lineWidth){return std::max(lineWidth,.2);}
constexpr int ellipsePoints=32;

QColor colourOf(qint64 c){return QColor(int(c&0xff),int((c>>8)&0xff),int((c>>16)&0xff));}
quint32 colourRef(const QColor &c){return quint32(c.red())|(quint32(c.green())<<8)|(quint32(c.blue())<<16);}
// Line styles: lines keep theirs in the header, closed shapes in a field of their own; both count 0 solid, 1 dotted,
// 2 dashed, 3 dash-dot, 4 dash-dot-dot (5 is solid as well, 6 dotted).
PenStyle penStyleOf(qint64 v){
    switch(v){case 1:case 6:return PenStyle::Dot;case 2:return PenStyle::Dash;case 3:return PenStyle::DashDot;case 4:return PenStyle::DashDotDot;default:return PenStyle::Solid;}
}
int penStyleValue(PenStyle s){
    switch(s){case PenStyle::Dot:return 1;case PenStyle::Dash:return 2;case PenStyle::DashDot:return 3;case PenStyle::DashDotDot:return 4;default:return 0;}
}
// Fill styles: 0 solid, 1 clear, 2 horizontal, 3 vertical, 4 falling ("\"), 5 rising ("/"), 6 cross, 7 diagonal cross;
// the reference draws nothing for other values.
FillStyle fillStyleOf(qint64 v){
    switch(v){case 0:return FillStyle::Solid;case 2:return FillStyle::Horizontal;case 3:return FillStyle::Vertical;case 4:return FillStyle::BackDiagonal;
        case 5:return FillStyle::Diagonal;case 6:return FillStyle::Cross;case 7:return FillStyle::DiagonalCross;default:return FillStyle::None;}
}
int fillStyleValue(FillStyle s){
    switch(s){case FillStyle::Horizontal:return 2;case FillStyle::Vertical:return 3;case FillStyle::BackDiagonal:return 4;case FillStyle::Diagonal:return 5;
        case FillStyle::Cross:return 6;case FillStyle::DiagonalCross:return 7;default:return 0;}
}
// The unit of a sheet's scale as the millimetres it holds; the reference shows other values as millimetres.
ScaleUnit scaleUnitOf(qint64 v){return v==10?ScaleUnit::Centimetre:v==1000?ScaleUnit::Metre:v==1000000?ScaleUnit::Kilometre:ScaleUnit::Millimetre;}
int scaleUnitValue(ScaleUnit u){switch(u){case ScaleUnit::Centimetre:return 10;case ScaleUnit::Metre:return 1000;case ScaleUnit::Kilometre:return 1000000;default:return 1;}}
// The print settings of a sheet (sPlan 8, 36 bytes): orientation (0 automatic, 1 portrait, 2 landscape), the offset as
// two floats in tenths of a millimetre with the opposite sign (stored -500 puts the content 50 mm right of the printable
// area's corner), the free scale in percent, whether it is free, banner pages across and down, their overlap in
// millimetres, four bytes more (kept, meaning unknown).
PrintSettings printOf(const QByteArray &b){
    PrintSettings p;if(b.size()!=36)return p;
    auto i32=[&](int at){return qFromLittleEndian<qint32>(b.constData()+at);};
    auto f32=[&](int at){const float v=qFromLittleEndian<float>(b.constData()+at);return std::isfinite(v)&&std::abs(v)<=1e7f?-double(v)/10:0.;};
    p.offset=QPointF(f32(4),f32(8));
    const int o=i32(0);p.orientation=o==1?PrintSettings::Orientation::Portrait:o==2?PrintSettings::Orientation::Landscape:PrintSettings::Orientation::Automatic;
    const int percent=i32(12);p.scale=percent>=1&&percent<=10000?percent/100.:1;
    p.free=b[16]!=0;
    const int x=i32(20),y=i32(24),overlap=i32(28);p.bannerX=x>=1&&x<=100?x:1;p.bannerY=y>=1&&y<=100?y:1;p.overlap=overlap>=0&&overlap<=1000?overlap:0;
    return p;
}
QByteArray printBytes(QByteArray b,const PrintSettings &p){
    auto put=[&](int at,qint32 v){qToLittleEndian(v,b.data()+at);};
    put(0,p.orientation==PrintSettings::Orientation::Portrait?1:p.orientation==PrintSettings::Orientation::Landscape?2:0);
    qToLittleEndian(float(-p.offset.x()*10),b.data()+4);qToLittleEndian(float(-p.offset.y()*10),b.data()+8);
    put(12,qint32(std::lround(p.scale*100)));b[16]=p.free?1:0;put(20,p.bannerX);put(24,p.bannerY);put(28,qint32(std::lround(p.overlap)));
    return b;
}
// Direction of a vector in degrees, counter-clockwise on the screen.
double angleOf(QPointF d){double a=qRadiansToDegrees(std::atan2(-d.y(),d.x()));if(a<0)a+=360;if(a>=360-1e-9)a=0;return a;}
QPointF fromUnits(QPointF p){return p/unit;}
double length(QPointF p){return std::hypot(p.x(),p.y());}
double cross(QPointF a,QPointF b){return a.x()*b.y()-a.y()*b.x();}
QPointF unitVector(double degrees){const double a=qDegreesToRadians(degrees);return QPointF(std::cos(a),-std::sin(a));}
// Guide lines from their floats in tenths of a millimetre.
QList<double> guides(const QByteArray &b){
    QList<double> out;for(qsizetype k=0;k+4<=b.size();k+=4){float f;std::memcpy(&f,b.constData()+k,4);out<<double(f)/unit;}
    return out;
}

// --- sPlan's reference point of an object, in file units: from it a component keeps its insertion point.
QPointF referencePoint(const Record &r){
    switch(r.type){
    case Ellipse:{
        const auto p=r.points("points");
        if(p.size()>=ellipsePoints)return QPointF(p[ellipsePoints/2].x(),p[ellipsePoints/4].y());
        return p.isEmpty()?QPointF():p[0];
    }
    case Rectangle:case Picture:case TextBox:{const auto c=r.points("corners");return c.isEmpty()?QPointF():c[0];}
    case Text:case Contact:{
        const auto c=r.points("corners");if(c.size()<2)return {};
        const qint64 a=r.integer("align");return a==1?(c[0]+c[1])/2:a==2?c[1]:c[0];
    }
    case Line:case Polygon:case Bezier:{const auto p=r.points("points");return p.isEmpty()?QPointF():p[0];}
    case Junction:return r.point("pos");
    case Dimension:{const auto p=r.points("points");return p.isEmpty()?QPointF():p[0];}
    case Group:{
        // The first child's point, replaced by any later one that lies (rounded) not further right and further up.
        // Contacts take no part.
        bool first=true;QPointF best;
        for(const auto &c:r.lists.value("children")){
            if(c.type==Contact)continue;
            const QPointF p=referencePoint(c);
            if(first){best=p;first=false;continue;}
            if(std::nearbyint(p.x())<=std::nearbyint(best.x())&&std::nearbyint(p.y())<std::nearbyint(best.y()))best=p;
        }
        return best;
    }
    case Component:return r.child("group")?referencePoint(*r.child("group")):QPointF();
    }
    return {};
}
// The title block mark of an object: groups take their last child's, components their designator's.
int titleMark(const Record &r){
    if(r.fields.contains("titleBlock"))return int(r.integer("titleBlock"));
    if(r.type==Group){const auto c=r.lists.value("children");return c.isEmpty()?0:titleMark(c.last());}
    if(r.type==Component&&r.child("designator"))return titleMark(*r.child("designator"));
    return 0;
}

// What the notes of a partial preview count.
struct Counts {
    int links=0,parents=0,contacts=0,arcs=0,nested=0,guessed=0,printUnknown=0;
};

// --- reading records into items
class Importer {
public:
    int version;
    Document *document;
    Counts counts;
    bool pins=true;                 // contacts get connection points (not in title blocks, where they are form fields)
    Importer(int v,Document *d):version(v),document(d){}

    void penAndFill(const Record &r,Item &i,bool line){
        i.pen.width=r.integer("width")/unit;i.pen.color=colourOf(r.integer("colour"));
        i.pen.style=r.integer("noOutline")?PenStyle::None:penStyleOf(line?r.integer("style"):r.integer("dash"));
        if(r.fields.contains("twoColour")){i.pen.twoColour=r.integer("twoColour");i.pen.color2=colourOf(r.integer("colour2"));}
        if(r.fields.contains("inner")){
            i.pen.inner=r.integer("inner");i.pen.innerColor=colourOf(r.integer("innerColour"));
            i.pen.cross=r.integer("cross");i.pen.crossColor=colourOf(r.integer("crossColour"));
        }
        if(line)return;
        i.fill.style=r.integer("noFill")?FillStyle::None:fillStyleOf(r.integer("fillStyle"));
        i.fill.color=colourOf(r.integer("fillColour"));
        // sPlan 7 keeps neither; sPlan 8 shows its files with its own defaults, those of the model.
        if(r.fields.contains("hatchSpacing")){i.fill.lineWidth=r.integer("hatchWidth")/unit;i.fill.spacing=std::max<double>(.01,r.integer("hatchSpacing")/unit);}
    }
    void lineEnds(const Record &r,Item &i){
        auto end=[](qint64 v){return v>=0&&v<=12?LineEnd(v):LineEnd::None;};
        i.startEnd=end(r.integer("startEnd"));i.endEnd=end(r.integer("endEnd"));
        const qint64 s=r.integer("endSize");i.endSize=(s>=0&&s<6?endFactors[s]:endFactors[2])*arrowWidth(i.pen.width);
    }
    // A text, contact or the like from its box: corners top left, top right, bottom right, bottom left.
    void textBox(const Record &r,Item &i){
        const auto c=r.points("corners");
        const qint64 a=r.integer("align");i.align=a==1?Align::Centre:a==2?Align::Right:Align::Left;
        const QPointF x=fromUnits(c[1]-c[0]),y=fromUnits(c[3]-c[0]);
        i.mirrored=cross(x,y)<0;
        i.rotation=angleOf(i.mirrored?-x:x);
        const QPointF anchor=i.align==Align::Centre?(c[0]+c[1])/2:i.align==Align::Right?c[1]:c[0];
        i.pos=fromUnits(anchor);
        i.font.family=r.string("font");i.font.height=r.integer("size")/unit;i.font.color=colourOf(r.integer("textColour"));
        const qint64 style=r.integer("fontStyle");i.font.bold=style&1;i.font.italic=style&2;i.font.underline=style&4;i.font.strikeOut=style&8;
        if(r.fields.contains("background")){i.background=r.integer("background");i.backgroundColor=colourOf(r.integer("backgroundColour"));}
        // The byte after the background: the writing turned ("Textrichtung umkehren").
        if(r.fields.contains("afterBackground"))i.reversed=r.integer("afterBackground")!=0;
    }
    void box(const Record &r,Item &i,int topLeft,int topRight,int bottomLeft){
        const auto c=r.points("corners");QPointF centre;for(auto p:c)centre+=p;centre/=c.size();
        const QPointF x=fromUnits(c[topRight]-c[topLeft]),y=fromUnits(c[bottomLeft]-c[topLeft]);
        i.centre=fromUnits(centre);i.size=QSizeF(length(x),length(y));i.rotation=angleOf(x);
    }

    Item item(const Record &r,bool inComponent){
        // The kept bytes start with the version they were read in.
        Item i;i.splan=QByteArray(1,char(version))+r.bytes;
        switch(r.type){
        case Line:case Bezier:{
            i.type=r.type==Line?ItemType::Line:ItemType::Bezier;
            for(auto p:r.points("points"))i.points<<fromUnits(p);
            penAndFill(r,i,true);lineEnds(r,i);i.electrical=!inComponent&&r.type==Line;
            if(i.type==ItemType::Bezier&&(i.points.size()<4||(i.points.size()-1)%3)){i.type=ItemType::Line;}
            break;
        }
        case Polygon:i.type=ItemType::Polygon;for(auto p:r.points("points"))i.points<<fromUnits(p);penAndFill(r,i,false);break;
        case Rectangle:{
            // Corners bottom left, top left, top right, bottom right; the rounding point lies on the bottom edge.
            i.type=ItemType::Rectangle;penAndFill(r,i,false);box(r,i,1,2,0);
            const auto c=r.points("corners");const double round=length(r.point("rounding")-c[0])/unit;
            const double shorter=std::min(i.size.width(),i.size.height());
            if(round>1e-4&&shorter>0){i.corner=std::min(50.,round/shorter*100);i.corners=r.integer("cornerStyle")==1?Corners::Bevel:Corners::Round;}
            break;
        }
        case Ellipse:{
            i.type=ItemType::Ellipse;penAndFill(r,i,false);
            const QPointF c=r.point("centre");const auto p=r.points("points");
            i.centre=fromUnits(c);
            if(p.size()>=ellipsePoints){
                const QPointF u=fromUnits(p[0]-c),v=fromUnits(p[ellipsePoints/4]-c);
                i.size=QSizeF(2*length(u),2*length(v));i.rotation=angleOf(u);
                // The arc's angles are angles on the sheet; the model counts them from the ellipse's own axis.
                const double s=qRadiansToDegrees(r.real("start"))-i.rotation,e=qRadiansToDegrees(r.real("stop"))-i.rotation;
                if(std::abs(r.real("start")-r.real("stop"))<1e-9){i.start=0;i.stop=360;i.arc=ArcStyle::Full;}
                else{
                    i.start=std::fmod(s+720,360.);i.stop=i.start+std::fmod(e-s+720,360.);
                    const qint64 a=r.integer("arc");i.arc=a==1?ArcStyle::Pie:a==2?ArcStyle::Chord:ArcStyle::Arc;counts.arcs++;
                }
            }
            break;
        }
        case Text:case Contact:{
            i.type=r.type==Text?ItemType::Text:ItemType::Contact;textBox(r,i);i.text=r.string("text");
            if(r.type==Text)i.link=r.string("link");
            if(r.type==Contact){i.name=r.string("name");if(pins)counts.contacts++;}
            break;
        }
        case TextBox:{
            i.type=ItemType::TextBox;penAndFill(r,i,false);box(r,i,0,1,3);
            i.text=r.fields.value("lines").toStringList().join(u'\n');
            i.font.family=r.string("font");i.font.height=r.integer("size")/unit;i.font.color=colourOf(r.integer("textColour"));
            const qint64 style=r.integer("fontStyle");i.font.bold=style&1;i.font.italic=style&2;i.font.underline=style&4;i.font.strikeOut=style&8;
            break;
        }
        case Junction:{
            // The diameter in "style", the colour as the filling's; the outline's colour field holds the automatic size:
            // its step plus 1209250 while it is on, the bare step while it is off.
            i.type=ItemType::Junction;i.pos=fromUnits(r.point("pos"));const double d=r.integer("style")/unit;
            i.size=QSizeF(d,d);i.pen.color=colourOf(r.integer("fillColour"));
            const qint64 step=r.integer("colour");
            i.autoSize=r.integer("noOutline")!=0&&step>=automaticSize&&step<automaticSize+5;
            i.sizeStep=int(i.autoSize?step-automaticSize:step>=0&&step<5?step:3);break;
        }
        case Picture:{
            i.type=ItemType::Image;box(r,i,0,1,3);
            const QByteArray data=r.raw("image#data");
            const QByteArray image=data.size()>4?data.mid(4,qFromLittleEndian<qint32>(data.constData())):QByteArray();
            const QString key=QString::fromLatin1(QCryptographicHash::hash(image,QCryptographicHash::Sha256).toHex());
            document->resources.insert(key,{QStringLiteral("bmp"),image});i.resource=key;
            break;
        }
        case Group:{
            // A group is its children: nothing more to keep.
            i.type=ItemType::Group;i.splan.clear();
            for(const auto &c:r.lists.value("children"))i.children<<item(c,inComponent);
            break;
        }
        case Dimension:{
            // The kind, three points, the distance of the dimension line (a double), arrows, colours, the number of
            // decimals as a power of ten, the switches and strings; the value's text gives the font, the two small
            // texts the tolerances. sPlan works out the drawing and the shown value itself.
            i.type=ItemType::Dimension;
            const qint64 kind=r.integer("beforePoints");i.dimension=kind>=0&&kind<=3?DimensionKind(kind):DimensionKind::Standard;
            for(const QPointF p:r.points("points"))i.points<<fromUnits(p);
            {const QByteArray b=r.raw("offset");double v=0;if(b.size()==8)std::memcpy(&v,b.constData(),8);i.offset=std::isfinite(v)?v/unit:0;}
            i.arrowAngle=std::clamp<double>(r.integer("value1"),1,170);i.arrowLength=std::max<double>(0,r.integer("value2")/unit);
            i.extensionColor=colourOf(r.integer("value3"));i.lineColor=colourOf(r.integer("value4"));
            const qint64 power=r.integer("value5");i.digits=power>0?std::clamp(int(std::lround(std::log10(double(power)))),0,6):1;
            i.showDiameter=r.integer("flag1");i.autoValue=r.integer("flag2");i.decimalPoint=r.integer("flag3");
            i.fixedValue=r.string("s1");i.suffix=r.string("s2");i.prefix=r.string("s3");
            if(const Record *x=r.child("text1"))i.font=item(*x,inComponent).font;
            if(const Record *x=r.child("text2"))i.upperTolerance=item(*x,inComponent).text;
            if(const Record *x=r.child("text3"))i.lowerTolerance=item(*x,inComponent).text;
            break;
        }
        case Component:{
            const Record *group=r.child("group"),*designator=r.child("designator"),*value=r.child("value");
            if(inComponent){
                // A component inside a component is a group of its parts and its shown texts; it keeps all its bytes.
                i.type=ItemType::Group;counts.nested++;
                if(group)for(const auto &c:group->lists.value("children"))i.children<<item(c,true);
                const QByteArray flags=r.raw("flags");
                for(auto [x,shown]:{std::pair{designator,flags.size()>3&&flags[3]},std::pair{value,flags.size()>2&&flags[2]}}){
                    Item t=item(*x,true);t.splan.clear();t.visible=shown;if(!t.text.isEmpty())i.children<<t;
                }
                break;
            }
            i.type=ItemType::Component;
            i.pos=fromUnits(referencePoint(r)+r.point("insertion"));
            QList<Item> children;
            if(group)for(const auto &c:group->lists.value("children"))children<<item(c,true);
            Item d=item(*designator,true),v=item(*value,true);
            d.role=TextRole::Designator;v.role=TextRole::Value;i.designator=d.text;i.value=v.text;d.text.clear();v.text.clear();
            children<<d<<v;
            for(auto &c:children)move(c,-i.pos);
            i.children=children;
            const QByteArray flags=r.raw("flags");
            i.valueVisible=flags.size()>2&&flags[2];i.designatorVisible=flags.size()>3&&flags[3];
            i.caption=r.string("caption");
            i.extra={r.string("extra1"),r.string("extra2"),r.string("extra3"),r.string("extra4")};
            i.parent=r.integer("parent")!=0;
            // sPlan keeps no connection points: the contacts get the line ends next to them.
            if(pins){const int n=guessPins(i);counts.contacts-=n;counts.guessed+=n;}
            // Kept without the parts, which keep their own bytes.
            {Record frame=r;frame.lists["group"][0].lists["children"]={};i.splan=QByteArray(1,char(version))+writeObject(frame,version);}
            break;
        }
        default:throw FormatError(ui("Unbekanntes Element in der sPlan-Datei (bei Byte %1)").arg(r.offset));
        }
        return i;
    }
};

// Items compared without their ids and kept bytes, by the fields their kind has (as in the own format), so that
// values without meaning for a kind do not count.
// The links to a parent and to a text are compared by the keys in the file (see Exporter::keys and guids).
void strip(Item &i){i.id.clear();i.splan.clear();i.parentId.clear();i.linkTarget.clear();i.linkable=false;for(auto &c:i.children)strip(c);}
// Numbers to a millionth of a millimetre: moving parts into a component and out again may change their last bits,
// far below what the file's numbers hold.
QJsonValue rounded(const QJsonValue &v){
    if(v.isDouble())return std::round(v.toDouble()*1e6)/1e6;
    if(v.isArray()){QJsonArray a;for(const auto &x:v.toArray())a.append(rounded(x));return a;}
    if(v.isObject()){QJsonObject o=v.toObject();for(auto it=o.begin();it!=o.end();++it)*it=rounded(*it);return o;}
    return v;
}
bool sameContent(Item a,Item b){strip(a);strip(b);return rounded(itemToJson(a))==rounded(itemToJson(b));}

int recordType(const Item &i){
    switch(i.type){
    case ItemType::Line:return Line;case ItemType::Bezier:return Bezier;case ItemType::Polygon:return Polygon;case ItemType::Rectangle:return Rectangle;
    case ItemType::Ellipse:return Ellipse;case ItemType::Text:case ItemType::NetLabel:return Text;case ItemType::TextBox:return TextBox;
    case ItemType::Junction:return Junction;case ItemType::Image:return Picture;case ItemType::Group:return Group;case ItemType::Component:return Component;
    case ItemType::Contact:return Contact;case ItemType::Dimension:return Dimension;
    }
    return Group;
}

// --- writing items into records
class Exporter {
public:
    int version;
    const Document &document;
    QStringList losses;
    int netLabels=0,pins=0,drawings=0,v8only=0,dimensions=0,nested=0;
    bool keep=true;                 // unchanged elements are written as read
    QHash<QString,qint64> keys;     // the key of each component in the file, by id
    QHash<QString,QByteArray> guids;// the key of each text in the file (16 bytes, sPlan 8), by id
    int internalLinks=0;
    int stripes=0;                  // elements whose stripes sPlan 7 cannot keep
    int hatches=0;                  // hatches whose spacing or line width sPlan 7 cannot keep
    int reversedTexts=0;            // texts written the other way round, which sPlan 7 cannot keep
    QByteArray targetOf(const Item &item) const{return item.linkTarget.isEmpty()?QByteArray(16,'\0'):guids.value(item.linkTarget,QByteArray(16,'\0'));}
    qint64 parentKey(const Item &item) const{return item.parentId.isEmpty()?0:keys.value(item.parentId,0);}
    Exporter(int v,const Document &d):version(v),document(d){}

    void header(Record &r){
        r.set("noOutline",0);r.set("noFill",1);r.set("width",2);r.set("colour",0u);r.set("style",0);r.set("fillColour",0xffffffu);r.set("fillStyle",0);r.set("titleBlock",0);
    }
    void stripeDefaults(Record &r){r.set("inner",0);r.set("cross",0);r.set("innerColour",0xffffu);r.set("crossColour",0xff0000u);r.set("hatchWidth",1);r.set("hatchSpacing",20);}
    void textDefaults(Record &r){
        header(r);r.setPoints("corners",{QPointF(),QPointF(),QPointF(),QPointF()});r.set("text",QString());r.set("font",QStringLiteral("Arial"));
        r.set("fontStyle",0);r.set("textColour",0u);r.set("size",40);r.set("align",0);r.set("afterAlign",0);r.set("string7",QString());r.set("link",QString());
        r.set("guid",QUuid::createUuid().toRfc4122());r.set("target",QByteArray(16,'\0'));r.set("background",0);r.set("backgroundColour",0xffffffu);r.set("afterBackground",0);
    }
    // A record of the type with the values a new element has in the reference.
    Record fresh(int type){
        Record r;r.type=type;
        switch(type){
        case Line:case Bezier:header(r);r.setPoints("points",{});r.set("colour2",255u);r.set("twoColour",0);r.set("startEnd",0);r.set("endEnd",0);r.set("endSize",2);stripeDefaults(r);break;
        case Polygon:header(r);r.setPoints("points",{});r.set("twoColour",0);r.set("colour2",255u);r.set("dash",0);stripeDefaults(r);break;
        case Rectangle:header(r);r.setPoints("corners",{QPointF(),QPointF(),QPointF(),QPointF()});r.setPoint("rounding",QPointF());r.set("twoColour",0);r.set("colour2",255u);r.set("dash",0);r.set("cornerStyle",0);stripeDefaults(r);break;
        case Ellipse:header(r);r.setPoints("points",{});r.setPoint("centre",QPointF());r.setReal("start",0);r.setReal("stop",0);r.set("twoColour",0);r.set("colour2",255u);r.set("dash",0);r.set("arc",0);stripeDefaults(r);break;
        case Text:textDefaults(r);break;
        case Contact:textDefaults(r);r.set("name",QString());break;
        case TextBox:header(r);r.setPoints("corners",{QPointF(),QPointF(),QPointF(),QPointF()});r.set("beforeFont",0);r.set("font",QStringLiteral("Arial"));r.set("fontStyle",0);
            r.set("textColour",0u);r.set("size",40);r.set("lines",QStringList());r.set("afterLines1",0);r.set("afterLines2",0);r.set("afterLines3",0);r.set("afterLines4",0);break;
        case Junction:header(r);r.set("noFill",0);r.set("style",12);r.setPoint("pos",QPointF());break;
        case Picture:header(r);r.setPoints("corners",{QPointF(),QPointF(),QPointF(),QPointF()});r.set("image",QByteArray());r.set("image#data",QByteArray());r.set("imageKey",0);break;
        case Group:r.lists["children"]={};break;
        case Dimension:{
            header(r);
            Record t;t.type=Text;textDefaults(t);r.lists["text1"]={t};t.set("size",20);r.lists["text2"]={t};r.lists["text3"]={t};
            r.set("beforePoints",0);r.setPoints("points",{QPointF(),QPointF(),QPointF()});r.setPoint("point4",QPointF());r.set("offset",QByteArray(8,'\0'));
            r.set("flag1",0);r.set("flag2",1);r.set("s1",QString());r.set("s2",QString());r.set("s3",QString());
            r.set("value1",15);r.set("value2",28);r.set("value3",0u);r.set("value4",0u);r.set("flag3",0);r.set("value5",10);
            break;
        }
        case Component:{
            Record g;g.type=Group;g.lists["children"]={};r.lists["group"]={g};
            Record d;d.type=Text;textDefaults(d);r.lists["designator"]={d};Record v;v.type=Text;textDefaults(v);r.lists["value"]={v};
            r.set("flags",QByteArray("\0\0\1\1",4));r.set("caption",QString());r.set("prefix",QString());r.set("number",0);r.setPoint("insertion",QPointF());
            r.set("afterInsertion",10);r.set("afterInsertionFlag",1);for(const char *e:{"extra1","extra2","extra3","extra4"})r.set(e,QString());
            r.set("parent",0);r.set("key",qint32(QRandomGenerator::global()->bounded(1,0x7fffffff)));r.set("parentKey",0);r.set("lastByte",0);
            break;
        }
        }
        return r;
    }
    void setMark(Record &r,int mark){
        if(r.fields.contains("titleBlock"))r.set("titleBlock",mark);
        for(auto &list:r.lists)for(auto &c:list)setMark(c,mark);
    }
    // Bold, italic, underlined and struck out in the low bits; the others as read.
    static int fontStyle(const Record &r,const Font &f){
        return int((r.integer("fontStyle")&~15)|(f.bold?1:0)|(f.italic?2:0)|(f.underline?4:0)|(f.strikeOut?8:0));
    }
    void penAndFill(const Item &i,Record &r,const Item *o,bool line){
        if(!o||i.pen.width!=o->pen.width)r.set("width",int(std::lround(i.pen.width*unit)));
        if(!o||i.pen.color!=o->pen.color)r.set("colour",colourRef(i.pen.color));
        if(!o||i.pen.style!=o->pen.style){
            r.set("noOutline",i.pen.style==PenStyle::None?1:0);
            if(i.pen.style!=PenStyle::None)r.set(line?"style":"dash",penStyleValue(i.pen.style));
        }
        if(r.fields.contains("twoColour")){
            if(!o||i.pen.twoColour!=o->pen.twoColour)r.set("twoColour",i.pen.twoColour?1:0);
            if(!o||i.pen.color2!=o->pen.color2)r.set("colour2",colourRef(i.pen.color2));
        }
        if(version>=80&&r.fields.contains("inner")){
            if(!o||i.pen.inner!=o->pen.inner)r.set("inner",i.pen.inner?1:0);
            if(!o||i.pen.innerColor!=o->pen.innerColor)r.set("innerColour",colourRef(i.pen.innerColor));
            if(!o||i.pen.cross!=o->pen.cross)r.set("cross",i.pen.cross?1:0);
            if(!o||i.pen.crossColor!=o->pen.crossColor)r.set("crossColour",colourRef(i.pen.crossColor));
        }else if(version<80&&(i.pen.inner||i.pen.cross))stripes++;
        if(line)return;
        if(!o||i.fill.style!=o->fill.style){r.set("noFill",i.fill.style==FillStyle::None?1:0);if(i.fill.style!=FillStyle::None)r.set("fillStyle",fillStyleValue(i.fill.style));}
        if(!o||i.fill.color!=o->fill.color)r.set("fillColour",colourRef(i.fill.color));
        if(!o||i.fill.lineWidth!=o->fill.lineWidth)r.set("hatchWidth",int(std::lround(i.fill.lineWidth*unit)));
        if(!o||i.fill.spacing!=o->fill.spacing)r.set("hatchSpacing",int(std::lround(i.fill.spacing*unit)));
        const Fill plain;
        if(version<80&&i.fill.style!=FillStyle::None&&i.fill.style!=FillStyle::Solid&&(i.fill.spacing!=plain.spacing||i.fill.lineWidth!=plain.lineWidth))hatches++;
    }
    static QList<QPointF> toUnits(const QPolygonF &p){QList<QPointF> out;for(auto q:p)out<<q*unit;return out;}
    // The box of a text as the reference stores it: corners top left, top right, bottom right, bottom left.
    QList<QPointF> textCorners(const Item &t,const QString &shown){
        const QRectF r=textRect(t,shown);const QTransform f=textFrame(t);
        return {f.map(r.topLeft())*unit,f.map(r.topRight())*unit,f.map(r.bottomRight())*unit,f.map(r.bottomLeft())*unit};
    }
    void text(const Item &i,Record &r,const Item *o,const QString &shown){
        const bool moved=!o||i.pos!=o->pos||i.rotation!=o->rotation||i.mirrored!=o->mirrored||i.align!=o->align||!(i.font==o->font)||i.text!=o->text;
        if(moved)r.setPoints("corners",textCorners(i,shown));
        if(!o||i.align!=o->align)r.set("align",i.align==Align::Centre?1:i.align==Align::Right?2:0);
        if(!o||i.font.family!=o->font.family)r.set("font",i.font.family);
        if(!o||i.font.height!=o->font.height)r.set("size",int(std::lround(i.font.height*unit)));
        if(!o||i.font.color!=o->font.color)r.set("textColour",colourRef(i.font.color));
        if(!o||i.font.bold!=o->font.bold||i.font.italic!=o->font.italic||i.font.underline!=o->font.underline||i.font.strikeOut!=o->font.strikeOut)
            r.set("fontStyle",fontStyle(r,i.font));
        if(!o||i.background!=o->background)r.set("background",i.background?1:0);
        if(!o||i.backgroundColor!=o->backgroundColor)r.set("backgroundColour",colourRef(i.backgroundColor));
        if(version>=80){if(!o||i.reversed!=o->reversed)r.set("afterBackground",i.reversed?1:0);}
        else if(i.reversed)reversedTexts++;
    }
    Record record(const Item &item,bool inComponent,int mark){
        Record base;bool haveBase=false;Item original;
        if(item.splan.size()>1&&item.splan[0]==char(version)){
            try{base=readObject(item.splan.mid(1),version);haveBase=base.type==recordType(item)||((base.type==Dimension||base.type==Component)&&item.type==ItemType::Group);}catch(const FormatError &){haveBase=false;}
            if(haveBase){
                Document scratch;Importer importer(version,&scratch);original=importer.item(base,inComponent);
                bool same=titleMark(base)==mark&&sameContent(original,item);
                if(base.type==Component&&item.type==ItemType::Component)
                    same=same&&(!keys.contains(item.id)||base.integer("key")==keys[item.id])&&base.integer("parentKey")==parentKey(item);
                if(base.type==Text&&item.type==ItemType::Text&&version>=80)
                    same=same&&(!guids.contains(item.id)||base.raw("guid")==guids[item.id])&&base.raw("target")==targetOf(item);
                // Dimensions read by older versions as groups of their texts, and components inside components, are
                // kept only as read: changed, they are written as groups.
                const bool asGroup=(base.type==Dimension||base.type==Component)&&item.type==ItemType::Group;
                if(same&&(keep||asGroup))return base;
                if(asGroup&&base.type==Dimension){haveBase=false;dimensions++;}
                else if(asGroup){haveBase=false;nested++;}
            }
        }
        const int type=recordType(item);
        Record r=haveBase?base:fresh(type);const Item *o=haveBase?&original:nullptr;
        switch(item.type){
        case ItemType::Line:case ItemType::Bezier:
            if(!o||item.points!=o->points)r.setPoints("points",toUnits(item.points));
            penAndFill(item,r,o,true);
            if(!o||item.startEnd!=o->startEnd)r.set("startEnd",int(item.startEnd));
            if(!o||item.endEnd!=o->endEnd)r.set("endEnd",int(item.endEnd));
            if(!o||item.endSize!=o->endSize||item.pen.width!=o->pen.width){
                const double f=item.endSize/arrowWidth(item.pen.width);int best=0;
                for(int k=1;k<6;k++)if(std::abs(endFactors[k]-f)<std::abs(endFactors[best]-f))best=k;
                r.set("endSize",best);
            }
            if(!item.electrical&&!inComponent&&item.type==ItemType::Line)drawings++;
            break;
        case ItemType::Polygon:
            if(!o||item.points!=o->points)r.setPoints("points",toUnits(item.points));
            penAndFill(item,r,o,false);break;
        case ItemType::Rectangle:{
            penAndFill(item,r,o,false);
            if(!o||item.centre!=o->centre||item.size!=o->size||item.rotation!=o->rotation||item.corner!=o->corner||item.corners!=o->corners){
                const QPolygonF c=corners(item);   // top left, top right, bottom right, bottom left
                r.setPoints("corners",{c[3]*unit,c[0]*unit,c[1]*unit,c[2]*unit});
                const double round=item.corners==Corners::Square?0:item.corner/100*std::min(item.size.width(),item.size.height());
                const QPointF along=c[2]-c[3];const double l=length(along);
                r.setPoint("rounding",(c[3]+(l>0?along/l*round:QPointF()))*unit);
                if(item.corners!=Corners::Square)r.set("cornerStyle",item.corners==Corners::Bevel?1:0);
            }
            break;
        }
        case ItemType::Ellipse:{
            penAndFill(item,r,o,false);
            if(!o||item.centre!=o->centre||item.size!=o->size||item.rotation!=o->rotation){
                QList<QPointF> points;const double rx=item.size.width()/2,ry=item.size.height()/2;
                const QPointF u=unitVector(item.rotation),v=unitVector(item.rotation+90);
                for(int k=0;k<ellipsePoints;k++){const double a=2*std::numbers::pi*k/ellipsePoints;points<<(item.centre+u*(rx*std::cos(a))+v*(ry*std::sin(a)))*unit;}
                r.setPoints("points",points);r.setPoint("centre",item.centre*unit);
            }
            if(!o||item.start!=o->start||item.stop!=o->stop||item.arc!=o->arc||item.rotation!=o->rotation){
                if(item.arc==ArcStyle::Full||std::abs(item.stop-item.start)>=360){r.setReal("start",0);r.setReal("stop",0);}
                else{r.setReal("start",qDegreesToRadians(std::fmod(item.start+item.rotation+720,360.)));r.setReal("stop",qDegreesToRadians(std::fmod(item.stop+item.rotation+720,360.)));
                    r.set("arc",item.arc==ArcStyle::Pie?1:item.arc==ArcStyle::Chord?2:0);}
            }
            break;
        }
        case ItemType::Text:case ItemType::NetLabel:case ItemType::Contact:{
            if(item.type==ItemType::NetLabel)netLabels++;
            if(item.type==ItemType::Contact&&item.hasPin)pins++;   // contacts in components: see component()
            if(!o||item.text!=o->text){r.set("text",item.text);if(hasDisplayString(item.text))r.set("display",item.text);}
            if(item.type==ItemType::Contact&&(!o||item.name!=o->name))r.set("name",item.name);
            text(item,r,o,item.text);
            if(item.type==ItemType::Text){
                // The external link, the text's own key and the key of its target.
                if(!o||item.link!=o->link)r.set("link",item.link);
                if(version>=80){
                    if(guids.contains(item.id)&&r.raw("guid")!=guids[item.id])r.set("guid",guids[item.id]);
                    if(r.raw("target")!=targetOf(item))r.set("target",targetOf(item));
                }else if(!item.linkTarget.isEmpty())internalLinks++;
            }
            break;
        }
        case ItemType::TextBox:{
            penAndFill(item,r,o,false);
            if(!o||item.centre!=o->centre||item.size!=o->size||item.rotation!=o->rotation){const QPolygonF c=corners(item);r.setPoints("corners",{c[0]*unit,c[1]*unit,c[2]*unit,c[3]*unit});}
            if(!o||item.text!=o->text)r.set("lines",item.text.split(u'\n'));
            if(!o||item.font.family!=o->font.family)r.set("font",item.font.family);
            if(!o||item.font.height!=o->font.height)r.set("size",int(std::lround(item.font.height*unit)));
            if(!o||item.font.color!=o->font.color)r.set("textColour",colourRef(item.font.color));
            if(!o||item.font.bold!=o->font.bold||item.font.italic!=o->font.italic||item.font.underline!=o->font.underline||item.font.strikeOut!=o->font.strikeOut)
                r.set("fontStyle",fontStyle(r,item.font));
            break;
        }
        case ItemType::Junction:
            if(!o||item.pos!=o->pos)r.setPoint("pos",item.pos*unit);
            if(!o||item.size!=o->size)r.set("style",int(std::lround(item.size.width()*unit)));
            if(!o||item.pen.color!=o->pen.color)r.set("fillColour",colourRef(item.pen.color));
            if(!o||item.autoSize!=o->autoSize||item.sizeStep!=o->sizeStep){r.set("noOutline",1);r.set("colour",qint64(item.autoSize?automaticSize:0)+std::clamp(item.sizeStep,0,4));}
            break;
        case ItemType::Image:{
            if(!o||item.centre!=o->centre||item.size!=o->size||item.rotation!=o->rotation){const QPolygonF c=corners(item);r.setPoints("corners",{c[0]*unit,c[1]*unit,c[2]*unit,c[3]*unit});}
            if(!o||item.resource!=o->resource){
                const auto res=document.resources.value(item.resource);QByteArray bmp=res.data;
                if(res.kind!=u"bmp"){QImage image=QImage::fromData(res.data);QBuffer buffer(&bmp);buffer.open(QIODevice::WriteOnly);bmp.clear();image.save(&buffer,"BMP");}
                QByteArray data(4,'\0');qToLittleEndian<qint32>(qint32(bmp.size()),data.data());data+=bmp;
                r.set("image#data",data);r.set("image",QByteArray());
            }
            break;
        }
        case ItemType::Group:{
            QList<Record> children;for(const auto &c:item.children)children<<record(c,inComponent,mark);
            r.lists["children"]=children;
            break;
        }
        case ItemType::Component:component(item,r,o);break;
        case ItemType::Dimension:dimension(item,r,o);break;
        }
        setMark(r,mark);
        return r;
    }
    double sheetScale=1;               // of the sheet being written, for the values of dimensions
    void dimension(const Item &item,Record &r,const Item *o){
        QPolygonF points=item.points;while(points.size()<3)points<<points.value(0);
        const bool geometry=!o||item.points!=o->points||item.offset!=o->offset||item.dimension!=o->dimension;
        if(!o||item.points!=o->points)r.setPoints("points",toUnits(points));
        if(!o||item.offset!=o->offset){const double v=item.offset*unit;QByteArray b(8,'\0');std::memcpy(b.data(),&v,8);r.set("offset",b);}
        if(!o||item.dimension!=o->dimension)r.set("beforePoints",int(item.dimension));
        if(!o||item.arrowAngle!=o->arrowAngle)r.set("value1",int(std::lround(item.arrowAngle)));
        if(!o||item.arrowLength!=o->arrowLength)r.set("value2",int(std::lround(item.arrowLength*unit)));
        if(!o||item.extensionColor!=o->extensionColor)r.set("value3",colourRef(item.extensionColor));
        if(!o||item.lineColor!=o->lineColor)r.set("value4",colourRef(item.lineColor));
        if(!o||item.digits!=o->digits)r.set("value5",int(std::lround(std::pow(10.,item.digits))));
        if(!o||item.showDiameter!=o->showDiameter)r.set("flag1",item.showDiameter?1:0);
        if(!o||item.autoValue!=o->autoValue)r.set("flag2",item.autoValue?1:0);
        if(!o||item.decimalPoint!=o->decimalPoint)r.set("flag3",item.decimalPoint?1:0);
        if(!o||item.fixedValue!=o->fixedValue)r.set("s1",item.fixedValue);
        if(!o||item.suffix!=o->suffix)r.set("s2",item.suffix);
        if(!o||item.prefix!=o->prefix)r.set("s3",item.prefix);
        // The texts: the shown value only when it differs (sPlan works it out again), font and place as drawn.
        const auto drawn=dimensionDrawing(item,sheetScale);const auto was=o?dimensionDrawing(*o,sheetScale):DimensionDrawing{};
        auto part=[&](const char *list,int index,const QString &shown,const QString &before,double height){
            Record &p=r.lists[list][0];
            Item t;t.type=ItemType::Text;t.font=item.font;t.font.height=height;t.text=shown;t.align=index?Align::Left:Align::Centre;
            Item w;const Item *from=nullptr;
            if(o){w.type=ItemType::Text;w.font=o->font;w.font.height=index?o->font.height/2:o->font.height;w.text=before;w.align=t.align;from=&w;}
            // The place where it is drawn; the tolerances keep theirs while the drawing has none.
            for(const auto &d:drawn.texts)if(d.text==shown&&d.font.height==height){t.pos=d.pos;t.rotation=d.rotation;break;}
            if(o)for(const auto &d:was.texts)if(d.text==before&&d.font.height==w.font.height){w.pos=d.pos;w.rotation=d.rotation;break;}
            if(!o||shown!=before){p.set("text",shown);if(hasDisplayString(shown))p.set("display",shown);}
            text(t,p,from,shown);
        };
        part("text1",0,dimensionText(item,sheetScale),o?dimensionText(*o,sheetScale):QString(),item.font.height);
        part("text2",1,item.upperTolerance,o?o->upperTolerance:QString(),item.font.height/2);
        part("text3",2,item.lowerTolerance,o?o->lowerTolerance:QString(),item.font.height/2);
        if(geometry&&!drawn.texts.isEmpty())r.setPoint("point4",drawn.texts[0].pos*unit);
    }
    void component(const Item &item,Record &r,const Item *o){
        // The parts in sheet coordinates, as the reference keeps them.
        QList<Item> parts;Item designator,value;
        // Connection points that are not the ones guessed when reading are lost; the parts are written without them.
        {
            const auto guessed=guessedPins(item);int k=0;
            std::function<void(const QList<Item>&)> count=[&](const QList<Item> &items){
                for(const auto &c:items){
                    if(c.type==ItemType::Group)count(c.children);
                    if(c.type!=ItemType::Contact)continue;
                    const auto g=guessed.value(k++);
                    if(c.hasPin&&(!g||QLineF(*g,c.pin).length()>1e-6))pins++;
                }
            };
            count(item.children);
        }
        std::function<void(QList<Item>&)> unpin=[&](QList<Item> &items){for(auto &c:items){c.hasPin=false;c.pin=QPointF();unpin(c.children);}};
        for(Item c:item.children){
            if(isText(c)){
                Item placed=placedText(c,item);placed.splan=c.splan;placed.hasPin=false;placed.pin=QPointF();
                if(c.role==TextRole::Designator){placed.text=item.designator;placed.role=TextRole::Plain;designator=placed;continue;}
                if(c.role==TextRole::Value){placed.text=item.value;placed.role=TextRole::Plain;value=placed;continue;}
                if(c.type==ItemType::Contact)placed.type=ItemType::Contact;
                parts<<placed;continue;
            }
            unpin(c.children);
            if(item.mirrored)mirror(c,0,true);
            rotate(c,QPointF(),item.rotation);schematic::move(c,item.pos);parts<<c;
        }
        QList<Record> children;for(const auto &p:parts)children<<record(p,true,0);
        r.lists["group"][0].lists["children"]=children;
        const Record d=r.lists["designator"][0],v=r.lists["value"][0];
        r.lists["designator"]={partText(designator,d,true)};r.lists["value"]={partText(value,v,false)};
        if(!o||o->valueVisible!=item.valueVisible||o->designatorVisible!=item.designatorVisible){
            QByteArray flags=r.raw("flags");flags.resize(4,'\0');flags[2]=item.valueVisible?1:0;flags[3]=item.designatorVisible?1:0;r.set("flags",flags);
        }
        if(!o||item.caption!=o->caption)r.set("caption",item.caption);
        // Its own key, the key of its parent, and whether it is a parent.
        if(keys.contains(item.id)&&r.integer("key")!=keys[item.id])r.set("key",qint32(keys[item.id]));
        if(r.integer("parentKey")!=parentKey(item))r.set("parentKey",qint32(parentKey(item)));
        if(bool(r.integer("parent"))!=item.parent)r.set("parent",item.parent?1:0);
        if(!o||item.designator!=o->designator){
            static const QRegularExpression split(QStringLiteral("^(.*?)(\\d+)$"));const auto m=split.match(item.designator);
            if(m.hasMatch()){r.set("prefix",m.captured(1));r.set("number",m.captured(2).toInt());}else r.set("prefix",item.designator);
        }
        for(int k=0;k<4;k++){
            const QString e=k<item.extra.size()?item.extra[k]:QString();
            if(version<80&&k>0&&!e.isEmpty())v8only++;
            if(!o||e!=(k<o->extra.size()?o->extra[k]:QString()))r.set(QStringLiteral("extra%1").arg(k+1),e);
        }
        // The insertion point is kept relative to the reference point of the parts.
        // An insertion point that has not moved against the parts keeps its value as read.
        const QPointF insertion=item.pos*unit-referencePoint(r),was=r.point("insertion");
        if(!o||std::abs(insertion.x()-was.x())>1e-3||std::abs(insertion.y()-was.y())>1e-3)r.setPoint("insertion",insertion);
    }
    void report(QStringList *losses) const{
        if(netLabels)losses->append(ui("%1 Netznamen werden als einfache Texte geschrieben; sPlan kennt keine Netze.").arg(netLabels));
        if(pins)losses->append(ui("Die Anschlusspunkte von %1 Kontakten entfallen; sPlan speichert keine.").arg(pins));
        if(drawings)losses->append(ui("%1 Linien sind keine elektrischen Verbindungen; sPlan unterscheidet das nicht, beim Lesen gelten sie wieder als Leitungen.").arg(drawings));
        if(v8only)losses->append(ui("Zusatztexte 2 bis 4 gibt es erst in sPlan 8."));
        if(nested)losses->append(ui("%1 geänderte Bauteile in Bauteilen werden als Gruppen geschrieben.").arg(nested));
        if(internalLinks)losses->append(ui("%1 interne Links entfallen; sPlan 7 kennt sie nicht.").arg(internalLinks));
        if(dimensions)losses->append(ui("%1 geänderte Bemaßungen werden als Gruppen ihrer Texte geschrieben.").arg(dimensions));
        if(stripes)losses->append(ui("Die Längs- und Querstreifen von %1 Elementen entfallen; sPlan 7 kennt sie nicht.").arg(stripes));
        if(hatches)losses->append(ui("%1 Schraffuren erhalten den Linienabstand 2 mm und die Linienstärke 0,1 mm; sPlan 7 speichert beides nicht.").arg(hatches));
        if(reversedTexts)losses->append(ui("%1 Texte mit umgekehrter Textrichtung werden normal geschrieben; sPlan 7 kennt sie nicht.").arg(reversedTexts));
    }
    Record partText(const Item &t,const Record &before,bool designator){
        Record r=before;
        Document scratch;Importer importer(version,&scratch);Item o=importer.item(before,true);
        if(keep&&sameContent(o,t))return before;
        if(t.text!=o.text){r.set("text",t.text);if(designator||hasDisplayString(t.text))r.set("display",t.text);}
        text(t,r,&o,t.text);
        return r;
    }
};

Record sheetRecord(const Sheet &s,int version,Exporter &out,QStringList *problems){
    Record r;bool base=false;
    if(s.splan.size()>1&&s.splan[0]==char(version)){try{r=readSheetFrame(s.splan.mid(1),version);base=true;}catch(const FormatError &){base=false;}}
    if(!base){
        r.set("name",s.name);r.set("height",int(std::lround(s.height)));r.set("width",int(std::lround(s.width)));r.set("b2",QByteArray(2,'\0'));
        r.set("grid",int(std::max<long>(1,std::lround(s.grid*unit))));r.set("scrollX",0);r.set("scrollY",0);r.setReal("zoom",1);
        r.set("guidesY",QByteArray());r.set("guidesX",QByteArray());r.set("reserved8",QByteArray(8,'\0'));
        QByteArray scale(8,'\0');const double one=1;std::memcpy(scale.data(),&one,8);r.set("scale",scale);r.set("scaleUnit",1);
        r.set("reserved36",QByteArray::fromHex("000000000000000000000000640000000000000001000000010000000000000000000000"));r.set("frameFlag",0);
        r.setReal("frameWidth",2800);r.setReal("frameHeight",1900);r.setReal("frameX",100);r.setReal("frameY",100);
        r.set("columns",10);r.set("rows",8);r.set("columnStart",1);r.set("rowStart",1);r.set("showGrid",0);
    }
    if(r.string("name")!=s.name)r.set("name",s.name);
    // Guide lines as floats in tenths of a millimetre; those that read back the same keep their bytes.
    for(auto [key,list]:{std::pair{"guidesY",&s.horizontalGuides},std::pair{"guidesX",&s.verticalGuides}}){
        QByteArray b(list->size()*4,'\0');
        for(int k=0;k<list->size();k++){const float f=float(list->at(k)*unit);std::memcpy(b.data()+4*k,&f,4);}
        if(guides(r.raw(key))!=*list)r.set(key,b);
    }
    if(r.integer("width")!=std::lround(s.width))r.set("width",int(std::lround(s.width)));
    if(r.integer("height")!=std::lround(s.height))r.set("height",int(std::lround(s.height)));
    if(std::abs(s.width-std::round(s.width))>1e-6||std::abs(s.height-std::round(s.height))>1e-6)
        problems->append(ui("Blattgrößen werden auf ganze Millimeter gerundet."));
    if(r.string("description")!=s.description)r.set("description",s.description);
    {const QByteArray b=r.raw("scale");double v=1;if(b.size()==8)std::memcpy(&v,b.constData(),8);
        if(v!=s.scale){QByteArray n(8,'\0');std::memcpy(n.data(),&s.scale,8);r.set("scale",n);}}
    if(scaleUnitOf(r.integer("scaleUnit"))!=s.scaleUnit)r.set("scaleUnit",scaleUnitValue(s.scaleUnit));
    // The print settings (sPlan 8): changed fields into the bytes as read.
    {const PrintSettings &now=s.print;
        if(version>=80){
            const QByteArray b=r.raw("reserved36");
            if(b.size()==36&&!(printOf(b)==now))r.set("reserved36",printBytes(b,now));
        }else if(!(now==PrintSettings()))problems->append(ui("Die Druckeinstellungen der Blätter entfallen; sPlan 7 speichert keine."));}
    // The reference keeps the grid in tenths of a millimetre, for each sheet.
    if(std::abs(r.integer("grid")/unit-s.grid)>1e-9){
        r.set("grid",int(std::max<long>(1,std::lround(s.grid*unit))));
        if(std::abs(s.grid*unit-std::round(s.grid*unit))>1e-6)problems->append(ui("Das Raster wird auf 0,1 mm gerundet."));
    }
    const TitleBlock &t=s.titleBlock;
    if(version>=80&&!t.frame.isEmpty()){
        const QRectF was(r.real("frameX")/unit,r.real("frameY")/unit,r.real("frameWidth")/unit,r.real("frameHeight")/unit);
        if(was!=t.frame){r.setReal("frameX",t.frame.x()*unit);r.setReal("frameY",t.frame.y()*unit);r.setReal("frameWidth",t.frame.width()*unit);r.setReal("frameHeight",t.frame.height()*unit);}
        if(r.integer("columns")!=t.columns&&t.columns>0)r.set("columns",t.columns);
        if(r.integer("rows")!=t.rows&&t.rows>0)r.set("rows",t.rows);
    }
    // The starts of the numbering as read (sPlan also takes 0, OpenLoch counts from 1), unless they were changed.
    if(version>=80){
        if(std::clamp<qint64>(r.integer("columnStart"),1,10000)!=t.columnStart)r.set("columnStart",t.columnStart);
        if(std::clamp<qint64>(r.integer("rowStart"),1,10000)!=t.rowStart)r.set("rowStart",t.rowStart);
    }
    if(version>=80&&bool(r.integer("showGrid"))!=t.showGrid)r.set("showGrid",t.showGrid?1:0);
    // sPlan 7 has no grid of the title block: frame, columns, rows, their starts and "Zeige Gitter" are lost.
    if(version<80&&problems&&(!t.frame.isEmpty()||t.columns>0||t.rows>0||t.showGrid||t.columnStart!=1||t.rowStart!=1))
        problems->append(ui("Das Gitter des Formblatts (Rahmen, Spalten, Zeilen, ihr Beginn und „Zeige Gitter“) entfällt; sPlan 7 speichert es nicht."));
    // The title block's objects come first, unless the file read had them mixed with the others: then each takes its
    // place in that order again, as far as there are objects of its kind.
    QList<Record> title,items,objects;
    out.sheetScale=s.scale;
    for(const auto &i:t.items)title<<out.record(i,false,1);
    for(const auto &i:s.items)items<<out.record(i,false,0);
    qsizetype a=0,b=0;
    for(char m:base?r.raw("order"):QByteArray()){if(m&&a<title.size())objects<<title[a++];else if(!m&&b<items.size())objects<<items[b++];}
    objects<<title.mid(a)<<items.mid(b);
    r.lists["children"]=objects;
    return r;
}
}

namespace {
// The notes of a partial preview.
QStringList notesOf(const Counts &c){
    QStringList out;
    if(c.nested)out.append(ui("%1 Bauteile liegen in anderen Bauteilen; OpenLoch zeigt sie als Gruppen ihrer Teile.").arg(c.nested));
    if(c.guessed)out.append(ui("Die Anschlusspunkte von %1 Kontakten sind aus den Linienenden der Bauteile abgeleitet; sPlan speichert keine.").arg(c.guessed));
    if(c.contacts)out.append(ui("%1 Kontakte haben keinen Anschlusspunkt; sPlan speichert keinen, deshalb verbinden die Netze sie nicht.").arg(c.contacts));
    if(c.parents)out.append(ui("%1 Bauteile verweisen auf ein Parent-Bauteil, das in der Datei fehlt.").arg(c.parents));
    if(c.links)out.append(ui("%1 Texte verweisen auf ein Linkziel, das in der Datei fehlt.").arg(c.links));
    if(c.printUnknown)out.append(ui("Bei %1 Blättern ist ein Feld der Druckeinstellungen belegt, dessen Bedeutung nicht bekannt ist: in der Datei erhalten, von OpenLoch nicht angewendet.").arg(c.printUnknown));
    return out;
}
// The pictures an item shows.
void collectResources(const Item &i,const QMap<QString,Resource> &from,QMap<QString,Resource> *to){
    if(i.type==ItemType::Image&&from.contains(i.resource))to->insert(i.resource,from[i.resource]);
    for(const auto &c:i.children)collectResources(c,from,to);
}
}
namespace {
// The components of a document's sheets and title blocks, also in groups.
void eachComponent(QList<Item> &items,const std::function<void(Item&)> &f){
    for(auto &i:items){if(i.type==ItemType::Component)f(i);else if(i.type==ItemType::Group)eachComponent(i.children,f);}
}
// The key a component had in the file it was read from (0 for none).
qint64 keptKey(const Item &c,int v,qint64 *parentKey=nullptr){
    if(c.splan.size()<2||c.splan[0]!=char(v))return 0;
    try{const Record r=readObject(c.splan.mid(1),v);if(r.type!=Component)return 0;if(parentKey)*parentKey=r.integer("parentKey");return r.integer("key");}
    catch(const FormatError &){return 0;}
}
// A key for every component: children find their parents by it. Keys are written as read; sPlan itself leaves a copy
// the key of its original. Only a parent whose key another parent has already, and a new component, get a new one.
QHash<QString,qint64> componentKeys(const Document &document,int v){
    Document copy=document;QHash<QString,qint64> keys;QSet<qint64> used,parents;
    QList<Item*> fresh;
    auto take=[&](Item &c){
        const qint64 k=keptKey(c,v);
        if(!k||(c.parent&&parents.contains(k))){fresh<<&c;return;}
        keys.insert(c.id,k);used.insert(k);if(c.parent)parents.insert(k);
    };
    for(auto &s:copy.sheets){eachComponent(s.titleBlock.items,take);eachComponent(s.items,take);}
    for(Item *c:fresh){qint64 k;do k=QRandomGenerator::global()->bounded(1,0x7fffffff);while(used.contains(k));used.insert(k);keys.insert(c->id,k);}
    return keys;
}
}
namespace {
// The texts of a document's sheets, also in groups.
void eachText(QList<Item> &items,const std::function<void(Item&)> &f){
    for(auto &i:items){if(i.type==ItemType::Text)f(i);else if(i.type==ItemType::Group)eachText(i.children,f);}
}
// The key and target key a text had in the file it was read from (empty for none, sPlan 8 only).
QByteArray keptGuid(const Item &t,int v,QByteArray *target=nullptr){
    if(v<80||t.splan.size()<2||t.splan[0]!=char(v))return {};
    try{const Record r=readObject(t.splan.mid(1),v);if(r.type!=Text)return {};if(target)*target=r.raw("target");return r.raw("guid");}
    catch(const FormatError &){return {};}
}
// A key for every text: the one it was read with unless another text has it, otherwise a new one.
QHash<QString,QByteArray> textGuids(const Document &document,int v){
    if(v<80)return {};
    Document copy=document;QHash<QString,QByteArray> guids;QSet<QByteArray> used;const QByteArray none(16,'\0');
    QList<Item*> fresh;
    auto take=[&](Item &t){const QByteArray g=keptGuid(t,v);if(g.size()==16&&g!=none&&!used.contains(g)){used.insert(g);guids.insert(t.id,g);}else fresh<<&t;};
    for(auto &s:copy.sheets)eachText(s.items,take);
    for(Item *t:fresh){QByteArray g;do g=QUuid::createUuid().toRfc4122();while(used.contains(g));used.insert(g);guids.insert(t->id,g);}
    return guids;
}
}
int version(const QByteArray &bytes){return fileVersion(bytes);}
int sourceVersion(const Document &document){return document.splan.isEmpty()?0:fileVersion(document.splan);}

Document read(const QByteArray &bytes,QStringList *notes){
    const Record file=readFile(bytes);
    const int v=fileVersion(bytes);
    Document d;d.id=newId();d.splan=writeFileFrame(file);
    // "Bauteile mit Seitennummer", "Blätter mit Seitennummer" and the prefix (sPlan 8) belong to the file.
    d.designatorPageNumbers=file.integer("b1")!=0;d.sheetNumbers=file.integer("b2")!=0;d.designatorPrefix=file.string("s1");
    for(const auto &var:file.fields.value("variables").toStringList()){
        const qsizetype bar=var.indexOf(u'|');
        d.variables.append(bar<0?Variable{var,QString()}:Variable{var.left(bar),var.mid(bar+1)});
    }
    Importer importer(v,&d);
    for(const auto &sr:file.lists.value("sheets")){
        Sheet s;s.id=newId();s.name=sr.string("name");s.width=std::max<qint64>(1,sr.integer("width"));s.height=std::max<qint64>(1,sr.integer("height"));
        s.description=sr.string("description");
        {   // The order of title block and sheet objects, kept when they are mixed.
            Record frame=sr;QByteArray order;for(const auto &o:frame.lists.value("children"))order.append(char(titleMark(o)?1:0));
            if(order.contains(QByteArray("\0\1",2)))frame.set("order",order);
            s.splan=QByteArray(1,char(v))+writeSheetFrame(frame,v);
        }
        s.grid=sr.integer("grid")>0?sr.integer("grid")/unit:1;
        {const QByteArray b=sr.raw("scale");double v=1;if(b.size()==8)std::memcpy(&v,b.constData(),8);s.scale=std::isfinite(v)&&v>0?v:1;}
        s.scaleUnit=scaleUnitOf(sr.integer("scaleUnit"));
        if(sr.fields.contains("reserved36")){
            const QByteArray b=sr.raw("reserved36");s.print=printOf(b);
            if(b.size()==36&&b.mid(32)!=QByteArray(4,'\0'))importer.counts.printUnknown++;
        }
        s.horizontalGuides=guides(sr.raw("guidesY"));s.verticalGuides=guides(sr.raw("guidesX"));
        if(v>=80){
            s.titleBlock.frame=QRectF(sr.real("frameX")/unit,sr.real("frameY")/unit,sr.real("frameWidth")/unit,sr.real("frameHeight")/unit);
            s.titleBlock.columns=int(std::clamp<qint64>(sr.integer("columns"),0,1000));s.titleBlock.rows=int(std::clamp<qint64>(sr.integer("rows"),0,1000));
            s.titleBlock.columnStart=int(std::clamp<qint64>(sr.integer("columnStart"),1,10000));s.titleBlock.rowStart=int(std::clamp<qint64>(sr.integer("rowStart"),1,10000));
            s.titleBlock.showGrid=sr.integer("showGrid");
        }
        for(const auto &o:sr.lists.value("children")){
            importer.pins=!titleMark(o);Item i=importer.item(o,false);assignIds(i);
            (titleMark(o)?s.titleBlock.items:s.items).append(i);
        }
        d.sheets.append(s);
    }
    if(d.sheets.isEmpty())throw FormatError(ui("Die sPlan-Datei enthält kein Blatt"));
    d.activeSheet=int(std::clamp<qint64>(file.integer("active"),0,d.sheets.size()-1));
    {   // Children find their parents by the keys in the file.
        // Keys need not be unique (copies keep their original's); parents come first, the first of a key counts.
        QHash<qint64,QString> parents,any;QList<std::pair<Item*,qint64>> children;
        auto look=[&](Item &c){
            qint64 parent=0;const qint64 key=keptKey(c,v,&parent);
            if(key){if(c.parent&&!parents.contains(key))parents.insert(key,c.id);if(!any.contains(key))any.insert(key,c.id);}
            if(parent)children<<std::pair{&c,parent};};
        for(auto &s:d.sheets){eachComponent(s.titleBlock.items,look);eachComponent(s.items,look);}
        for(auto [c,parent]:children){
            const QString id=parents.value(parent,any.value(parent));
            if(!id.isEmpty()&&id!=c->id)c->parentId=id;else importer.counts.parents++;
        }
    }
    {   // Links find their target texts by the texts' keys (sPlan 8).
        QHash<QByteArray,Item*> byGuid;QList<std::pair<Item*,QByteArray>> links;const QByteArray none(16,'\0');
        auto look=[&](Item &t){QByteArray target;const QByteArray g=keptGuid(t,v,&target);if(g.size()==16&&g!=none&&!byGuid.contains(g))byGuid.insert(g,&t);if(target.size()==16&&target!=none)links<<std::pair{&t,target};};
        for(auto &s:d.sheets)eachText(s.items,look);
        for(auto [t,target]:links){
            Item *to=byGuid.value(target);
            if(to&&to!=t){t->linkTarget=to->id;to->linkable=true;}else importer.counts.links++;
        }
    }
    if(notes)notes->append(notesOf(importer.counts));
    return d;
}

QByteArray write(const Document &document,int v,QStringList *losses,bool keepUnchanged){
    if(!supportedVersion(v))throw FormatError(ui("sPlan-Dateien der Version %1 werden nicht unterstützt").arg(v/10.,0,'f',1));
    Record file;bool base=false;
    if(!document.splan.isEmpty()&&fileVersion(document.splan)==v){try{file=readFileFrame(document.splan);base=true;}catch(const FormatError &){base=false;}}
    if(!base){
        QByteArray head=QByteArray("\x07SPLAN",6)+QByteArray::number(v)+QByteArray(3,'\0');file.set("head",head);
        file.set("b1",0);file.set("b2",1);file.set("s1",QString());file.set("s2",QString());
    }
    QStringList vars;for(const auto &x:document.variables)vars<<x.name+u'|'+x.value;
    if(file.fields.value("variables").toStringList()!=vars)file.set("variables",vars);
    file.set("active",document.activeSheet);
    if((file.integer("b1")!=0)!=document.designatorPageNumbers)file.set("b1",document.designatorPageNumbers?1:0);
    if((file.integer("b2")!=0)!=document.sheetNumbers)file.set("b2",document.sheetNumbers?1:0);
    if(v>=80&&file.string("s1")!=document.designatorPrefix)file.set("s1",document.designatorPrefix);
    Exporter out(v,document);out.keep=keepUnchanged;QStringList problems;
    out.keys=componentKeys(document,v);out.guids=textGuids(document,v);
    QList<Record> sheets;
    for(const auto &s:document.sheets){
        sheets<<sheetRecord(s,v,out,&problems);
        if(s.spare)problems.append(ui("Das Kennzeichen „Reserveblatt“ wird nicht geschrieben."));
        if(v<80&&s.name.size()>30)problems.append(ui("Blattnamen werden auf 30 Zeichen gekürzt."));
    }
    file.lists["sheets"]=sheets;
    const QByteArray bytes=writeFile(file);
    // Nothing is written that the reader would not take back.
    readFile(bytes);
    if(losses){
        out.report(losses);
        if(v<80&&sourceVersion(document)==80)losses->append(ui("Felder, die es erst in sPlan 8 gibt (Linkziele und Hintergründe von Texten), entfallen."));
        if(v<80&&!document.designatorPrefix.isEmpty())losses->append(ui("Der Präfix der Bauteilbezeichner entfällt (erst ab sPlan 8)."));
        problems.removeDuplicates();losses->append(problems);
    }
    return bytes;
}
QStringList losses(const Document &document,int v){QStringList l;write(document,v,&l);return l;}

namespace {
// The components of a library page that hang on a parent of the same page by its key, as the reference keeps the
// children of a parent (the gates of a 7400): their records' indexes among the page's objects.
QSet<int> childRecords(const QList<Record> &objects){
    QSet<qint64> keys;for(const auto &r:objects)if(r.type==Component&&r.integer("key")!=0)keys<<r.integer("key");
    QSet<int> out;
    for(int k=0;k<objects.size();k++){const Record &r=objects[k];if(r.type==Component&&r.integer("parentKey")!=0&&keys.contains(r.integer("parentKey")))out<<k;}
    return out;
}
}
LibraryPage readLibrary(const QByteArray &bytes,QStringList *notes){
    const Record file=readLibraryFile(bytes);
    Document scratch;Importer importer(fileVersion(bytes),&scratch);
    LibraryPage page;page.name=file.string("name");
    const QList<Record> objects=file.lists.value("children");const QSet<int> children=childRecords(objects);
    QHash<qint64,int> entryOfKey;QHash<qint64,QPointF> placeOfKey;
    for(int k=0;k<objects.size();k++){
        if(children.contains(k))continue;
        const Record &o=objects[k];Item i=importer.item(o,false);
        if(i.type==ItemType::Group)move(i,-fromUnits(referencePoint(o)));
        else if(i.type!=ItemType::Component)continue;
        if(i.type==ItemType::Component){entryOfKey.insert(o.integer("key"),int(page.entries.size()));placeOfKey.insert(o.integer("key"),i.pos);}
        i.pos=QPointF();
        LibraryEntry e;e.caption=i.caption;e.symbol=i;collectResources(i,scratch.resources,&e.resources);
        page.entries.append(e);
    }
    // Children at their places relative to their parent's insertion point, in the order of the page.
    for(int k=0;k<objects.size();k++){
        if(!children.contains(k))continue;
        const Record &o=objects[k];const qint64 parent=o.integer("parentKey");if(!entryOfKey.contains(parent))continue;
        Item c=importer.item(o,false);c.pos-=placeOfKey.value(parent);c.parentId.clear();
        LibraryEntry &e=page.entries[entryOfKey.value(parent)];collectResources(c,scratch.resources,&e.resources);e.children<<c;
    }
    if(notes)notes->append(notesOf(importer.counts));
    return page;
}
QByteArray writeLibrary(const LibraryPage &page,const QByteArray &original,int v,QStringList *losses){
    Record file;
    if(!original.isEmpty()){file=readLibraryFile(original);v=fileVersion(original);}
    else{
        // A new page: an A4 sheet, as the reference's pages are.
        file.set("head",(v==80?QByteArray("\x07SPLAN80",8):QByteArray("\x07SPLAN70",8))+QByteArray(3,'\0'));file.set("first",0);file.set("second",0);
        file.set("name",QString());file.set("height",297);file.set("width",210);if(v<80)file.set("afterSize",QByteArray(2,'\0'));
        file.set("grid",100);file.set("scrollX",0);file.set("scrollY",0);file.setReal("zoom",1);file.lists["children"]={};
    }
    if(file.string("name")!=page.name)file.set("name",page.name);
    Document scratch;for(const auto &e:page.entries)for(auto it=e.resources.cbegin();it!=e.resources.cend();++it)scratch.resources.insert(it.key(),it.value());
    Exporter out(v,scratch);Document read;Importer importer(v,&read);
    // The page's components and groups as the entries they were read as (components at no insertion point, groups around
    // their reference point, children relative to their parent), to find a record and to see whether it changed.
    struct Original {Record record;Item entry;bool child=false;bool used=false;};
    const QList<Record> objects=file.lists.value("children");const QSet<int> childSet=childRecords(objects);
    QHash<qint64,QPointF> placeOfKey;
    for(const auto &r:objects)if(r.type==Component)placeOfKey.insert(r.integer("key"),importer.item(r,false).pos);
    QList<Original> originals;QHash<int,int> originalOfObject;
    for(int k=0;k<objects.size();k++){
        const Record &r=objects[k];if(r.type!=Component&&r.type!=Group)continue;
        Original o{r,importer.item(r,false),childSet.contains(k)};
        if(o.entry.type==ItemType::Group)move(o.entry,-fromUnits(referencePoint(r)));else if(o.entry.type!=ItemType::Component)continue;
        if(o.child){o.entry.pos-=placeOfKey.value(r.integer("parentKey"));o.entry.parentId.clear();}
        else if(o.entry.type==ItemType::Component)o.entry.pos=QPointF();
        originalOfObject.insert(k,int(originals.size()));originals<<o;
    }
    // Components by the bytes they keep, groups (which keep none) by their content.
    auto find=[&](const Item &i,bool child)->Original*{
        for(auto &o:originals)if(!o.used&&o.child==child&&o.entry.type==i.type&&(i.splan.isEmpty()?sameContent(o.entry,i):o.entry.splan==i.splan))return &o;
        return nullptr;};
    // An entry's record: unchanged as it was, changed back where the page had it, new in rows of eight 30 mm apart.
    auto record=[&](const LibraryEntry &e,qsizetype index)->Record{
        Item i=e.symbol;if(i.type==ItemType::Component)i.caption=e.caption;
        Original *match=find(i,false);
        if(match){
            match->used=true;
            if(sameContent(match->entry,i))return match->record;
            if(i.type==ItemType::Component)i.pos=importer.item(match->record,false).pos;else move(i,fromUnits(referencePoint(match->record)));
        }else{
            const QPointF at(20+30*double(index%8),20+30*double(index/8));
            if(i.type==ItemType::Component)i.pos=at;else move(i,at);
        }
        return out.record(i,false,0);};
    // The entries in the places of the page's entries (in the page's order), then their children: a child read from the
    // page in the place it had (unchanged as it was, changed written anew), at its place relative to its parent's record
    // and hanging on that parent's key; new children after the page's objects.
    QList<Record> entries;for(qsizetype k=0;k<page.entries.size();k++)entries<<record(page.entries[k],k);
    QList<Record> childRecordsOut;QHash<int,Record> kept;   // by the index of the original
    for(qsizetype k=0;k<page.entries.size();k++){
        if(entries[k].type!=Component)continue;
        const Record &parent=entries[k];
        for(const auto &c:page.entries[k].children){
            Original *match=find(c,true);
            if(match){
                match->used=true;const int was=int(match-originals.data());
                if(sameContent(match->entry,c)&&match->record.integer("parentKey")==parent.integer("key")){kept.insert(was,match->record);continue;}
                Item i=c;i.pos+=importer.item(parent,false).pos;Record r=out.record(i,false,0);r.set("parentKey",qint32(parent.integer("key")));kept.insert(was,r);continue;
            }
            Item i=c;i.pos+=importer.item(parent,false).pos;Record r=out.record(i,false,0);r.set("parentKey",qint32(parent.integer("key")));childRecordsOut<<r;
        }
    }
    QList<Record> result;qsizetype next=0;
    for(int k=0;k<objects.size();k++){
        const Record &r=objects[k];
        const bool entry=(r.type==Component||r.type==Group)&&originalOfObject.contains(k);
        if(!entry){result<<r;continue;}
        const int index=originalOfObject.value(k);
        if(originals[index].child){if(kept.contains(index))result<<kept.value(index);continue;}
        if(next<entries.size())result<<entries[next++];
    }
    for(;next<entries.size();next++)result<<entries[next];
    result<<childRecordsOut;
    file.lists["children"]=result;
    const QByteArray bytes=writeLibraryFile(file,v);
    readLibraryFile(bytes);
    if(losses)out.report(losses);
    return bytes;
}
QList<Item> readTitleBlock(const QByteArray &bytes,QMap<QString,Resource> *resources,QStringList *notes){
    const Record form=readFormFile(bytes);
    Document scratch;Importer importer(formVersion(bytes),&scratch);importer.pins=false;
    QList<Item> items;
    for(const auto &o:form.lists.value("children")){Item i=importer.item(o,false);assignIds(i);items.append(i);}
    if(resources)for(auto it=scratch.resources.cbegin();it!=scratch.resources.cend();++it)resources->insert(it.key(),it.value());
    if(notes)notes->append(notesOf(importer.counts));
    return items;
}
QByteArray writeTitleBlock(const QList<Item> &items,const QMap<QString,Resource> &resources,int v,QStringList *losses){
    Document scratch;scratch.resources=resources;
    Exporter out(v,scratch);Record form;form.set("second",0);
    QList<Record> objects;for(const auto &i:items)objects<<out.record(i,false,1);
    form.lists["children"]=objects;
    const QByteArray bytes=writeFormFile(form,v);
    readFormFile(bytes);
    if(losses)out.report(losses);
    return bytes;
}
void save(const Document &document,const QString &path,int v,QStringList *losses){
    const QByteArray bytes=write(document,v,losses);
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))throw FormatError(file.errorString());
    if(file.write(bytes)!=bytes.size()||!file.commit())throw FormatError(file.errorString());
}
}
