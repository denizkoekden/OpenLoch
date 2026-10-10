#include "sprint.h"
#include "language.h"
#include "legacy_reader.h"
#include "modules/pcb/font.h"
#include <QMap>
#include <QSet>
#include <QtEndian>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace openloch::sprint {
using namespace pcb;
namespace {
// Two record layouts. Version 6 stores lengths in tenths of a micrometre as 32-bit floats, version 4 (Sprint-Layout
// 4.0) in hundredths of a millimetre as 32-bit integers, with a shorter fixed part and board header. Version 5
// (Sprint-Layout 5) has the layout of version 6 with its lengths in hundredths of a millimetre; its board headers and
// records are converted to version 6 when they are read (see fromVersion5). In all, y points up: the top edge of the
// working area is y = 0 and the board lies below it.
struct Variant {int version;double unit;bool floats;int fixed,header;};
constexpr Variant current{6,10000,true,0x4c,0x212},old{4,100,false,0x4a,0x209};
constexpr int macroTrailer=102;     // two short strings at the end of a macro
// Version 6 marks the records of its pads with this number; only then are the thermal fields read, in version 5 files
// as well (whose pads carry no mark as a rule, and anything in those bytes).
constexpr quint32 padMark=123456789;
enum Type : quint8 {TypePad=2,TypeArea=4,TypeCircle=5,TypeTrack=6,TypeText=7,TypeSmd=8};
// Offsets in the fixed part, counted from the record's type byte; the same in both variants up to the clearance.
enum Offset {OffX=0x01,OffY=0x05,OffOuter=0x09,OffInner=0x0d,OffWidth=0x11,OffLayer=0x16,OffForm=0x17,OffPart=0x1c,OffSpokes=0x1f,OffFill=0x28,
             OffClearance=0x29,OffMirror=0x32,OffMirrorV=0x33,OffHatch=0x32,OffHatchAuto=0x33,OffCutout=0x34,OffAngle=0x35,OffHatchPitch=0x35,OffThermal=0x32,
             OffSpokesPerLayer=0x28,OffVia=0x39,OffMask=0x3a,OffPadMark=0x3e,OffMirror4=0x36,OffMirrorV4=0x37,OffMaskCutout=0x4a};
// The mark of a track laid by the autorouter (0 for none); version 4 keeps it four bytes further back.
constexpr int OffAutoroute=0x31,OffAutoroute4=0x35;

// Windows code page 1252, as the files store text; 0x80–0x9f differ from Latin-1.
const char16_t cp1252[32]={0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,
                           0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
QString ansi(const QByteArray &bytes){
    QString s;s.reserve(bytes.size());
    for(char c:bytes){const auto b=quint8(c);s.append(b>=0x80&&b<0xa0?QChar(cp1252[b-0x80]):QChar(b));}
    return s;
}
QByteArray ansi(const QString &text){
    QByteArray out;out.reserve(text.size());
    for(QChar c:text){
        const char16_t u=c.unicode();char b='?';
        if(u<0x80||(u>=0xa0&&u<0x100))b=char(u);else for(int i=0;i<32;i++)if(cp1252[i]==u&&!(u>=0x80&&u<0xa0)){b=char(0x80+i);break;}
        out.append(b);
    }
    return out;
}

double floatAt(const char *p){const auto v=qFromLittleEndian<quint32>(p);float f;std::memcpy(&f,&v,4);return f;}
void putFloat(char *p,double value){const float f=float(value);quint32 v;std::memcpy(&v,&f,4);qToLittleEndian(v,p);}
double doubleAt(const char *p){const auto v=qFromLittleEndian<quint64>(p);double d;std::memcpy(&d,&v,8);return d;}
void putDouble(char *p,double d){quint64 v;std::memcpy(&v,&d,8);qToLittleEndian(v,p);}
class Reader {
public:
    Reader(const QByteArray &b,const Variant &v):bytes(b),variant(v),file(b.isEmpty()?0:quint8(b[0])){}
    void need(qsizetype n) const{if(n<0||pos+n>bytes.size())throw FormatError(ui("Die Sprint-Layout-Datei ist unvollständig"));}
    QByteArray raw(qsizetype n){need(n);auto r=bytes.mid(pos,n);pos+=n;return r;}
    quint8 u8(){need(1);return quint8(bytes[pos++]);}
    quint32 u32(){need(4);auto v=qFromLittleEndian<quint32>(bytes.constData()+pos);pos+=4;return v;}
    quint32 count(quint32 limit){const auto n=u32();if(n>limit)throw FormatError(ui("Die Sprint-Layout-Datei ist beschädigt (Position 0x%1)").arg(QString::number(pos-4,16)));return n;}
    QByteArray string(){return raw(count(16*1024*1024));}
    QByteArray shortString(int capacity){const int n=std::min<int>(u8(),capacity);auto s=raw(capacity);return s.left(n);}
    // The points of a track, area or pad outline; `extra` is how much the stored count exceeds their number. A
    // coordinate that is no finite number breaks the file, in version 5 also one that becomes none times 100 as a float.
    QList<QPointF> points(quint32 extra=0){
        auto n=count(1000000);
        if(n<extra)throw FormatError(ui("Die Sprint-Layout-Datei ist beschädigt (Position 0x%1)").arg(QString::number(pos-4,16)));
        n-=extra;need(qsizetype(n)*8);QList<QPointF> out;out.reserve(n);const double limit=std::numeric_limits<float>::max()/(file==5?100:1);
        for(quint32 i=0;i<n;i++){const QPointF p(numberAt(pos+8*i),numberAt(pos+8*i+4));
            if(!(std::abs(p.x())<=limit&&std::abs(p.y())<=limit))throw FormatError(ui("Die Sprint-Layout-Datei ist beschädigt (Position 0x%1)").arg(QString::number(pos+8*i,16)));out<<p;}
        pos+=qsizetype(n)*8;return out;
    }
    double numberAt(qsizetype at) const{return variant.floats?floatAt(bytes.constData()+at):double(qFromLittleEndian<qint32>(bytes.constData()+at));}
    qsizetype pos=0;
    const QByteArray &bytes;
    const Variant &variant;
    int file;           // the version byte of the file (5 and 6 share the layout)
};
quint32 u32(const QByteArray &fixed,int offset){return qFromLittleEndian<quint32>(fixed.constData()+offset-1);}
quint8 u8(const QByteArray &fixed,int offset){return quint8(fixed[offset-1]);}
quint16 u16(const QByteArray &fixed,int offset){return qFromLittleEndian<quint16>(fixed.constData()+offset-1);}
// A length field: a float in versions 5/6, an integer in version 4.
double number(const Variant &v,const QByteArray &fixed,int offset){
    return v.floats?floatAt(fixed.constData()+offset-1):double(qint32(u32(fixed,offset)));
}
// Fields are only rewritten when their value changed, so unchanged imported records are written back as read.
void setNumber(const Variant &v,QByteArray &fixed,int offset,double value){
    if(v.floats){if(float(value)!=float(number(v,fixed,offset)))putFloat(fixed.data()+offset-1,value);}
    else{const auto i=qint32(std::clamp<double>(std::round(value),-2147483647.0,2147483647.0));if(i!=qint32(u32(fixed,offset)))qToLittleEndian(i,fixed.data()+offset-1);}
}
void setU32(QByteArray &fixed,int offset,quint32 v){qToLittleEndian(v,fixed.data()+offset-1);}
void setU8(QByteArray &fixed,int offset,quint8 v){fixed[offset-1]=char(v);}

double normalized(double a){a=std::fmod(a,360.0);return a<0?a+360:a;}
// The component data after the strokes of a designator: the pick and place offset (two floats in tenths of a
// micrometre, y downwards), the centre (0 copper, 1 silkscreen, 2 and above both) and the rotation (a double).
// Values that are not finite or beyond what the model holds count as 0.
double offsetAt(const char *p){const double v=floatAt(p)/10000;return std::isfinite(v)&&std::abs(v)<=1000?v:0;}
double rotationAt(const char *p){const double v=doubleAt(p+9);return std::isfinite(v)&&std::abs(v)<=1e6?v:0;}
int centreAt(const char *p){return std::min<int>(quint8(p[8]),2);}
// Text roles: bit 0 marks a designator, bit 1 a value.
TextRole roleOf(const QByteArray &fixed){const int r=u8(fixed,OffForm);return r&1?TextRole::Designator:r&2?TextRole::Value:TextRole::Plain;}
// Spoke width of thermal pads in percent (16 bits; the thermal switch follows); older files leave the field unused.
int spokeWidth(quint32 v){return v>1000?100:int(v);}
void setSpokeWidth(QByteArray &fixed,int width){if(spokeWidth(u16(fixed,OffAngle))!=width)qToLittleEndian(quint16(width),fixed.data()+OffAngle-1);}
// Arc angles are thousandths of a degree; values below 1000 are read as whole degrees, as older macros and version 4 store them.
double arcAngle(quint32 v){return v<1000?double(v):v/1000.0;}
quint32 arcValue(double degrees){
    auto v=quint32(std::llround(normalized(degrees)*1000))%360000;
    if(v>0&&v<1000)v=v<500?0:1000;   // would be taken for whole degrees
    return v;
}
int expectedOutline(PadShape s){
    return s==PadShape::Round||s==PadShape::OvalWide||s==PadShape::OvalTall?2:s==PadShape::Octagon||s==PadShape::OctagonWide||s==PadShape::OctagonTall?8:4;
}
// The direction of a pad's stored outline, counter-clockwise in degrees, as its rotation.
double outlineRotation(const Element &e){
    if(e.points.size()<2)return 0;
    // The top edge: the first one, or the second one of an octagon (its first edge is a cut corner).
    const int first=e.points.size()==8?1:0;
    const QPointF d=e.points[first+1]-e.points[first];double a=qRadiansToDegrees(std::atan2(-d.y(),d.x()));
    if(e.type==ElementType::Pad&&e.shape==PadShape::OvalTall)a+=90;
    const double r=normalized(a);return std::abs(r-std::round(r))<1e-6?std::round(r):r;
}

struct Record {
    quint8 type=0;QByteArray fixed;QByteArray text,text2;QList<int> groups;QList<QPointF> points;
    QList<std::pair<QByteArray,QList<QPointF>>> strokes;
    QByteArray component,package,comment;quint8 pickAndPlace=0;
    qint32 twin=-1;     // version 4: the pad on the other copper side that makes a through-plated pad with this one
};
[[noreturn]] void unknownElement(qsizetype at){throw FormatError(ui("Unbekanntes Element in der Sprint-Layout-Datei (Position 0x%1)").arg(QString::number(at,16)));}
// Version 6 marks an area on a copper side that only opens the solder mask; for such an area the stored point count is
// one more than the points that follow.
bool maskOpening(const QByteArray &fixed,quint8 type,int file){
    const int layer=u8(fixed,OffLayer);
    return file>=6&&type==TypeArea&&u8(fixed,OffMaskCutout)&&(layer==CopperTop||layer==CopperBottom);
}
// Version 5 (Sprint-Layout 5) has the record layout of version 6 with lengths in hundredths of a millimetre. Its fixed
// part becomes one of version 6 as the reference converts it when opening such a file: only the fields that version 5
// has are taken over (the others become 0, among them the component number and the mark of mask openings); x, y, the
// radii or sizes and the clearance become 0 when they are no number or lie beyond ±10 000 000 (100 m), and are then
// multiplied by 100, except the second size of a text, its stroke thickness. The width field is multiplied while it
// lies within ±10 000 000, but not for circles and texts, where it holds the stop angle and the style. Angles, the
// hatch fields of areas and the thermal fields stay as stored. The fields that version 6 added get their defaults:
// no spokes per layer, round track ends, plain and visible texts.
void fromVersion5(QByteArray &f,quint8 type){
    QByteArray kept(f.size(),'\0');
    for(const auto &[first,last]:{std::pair{0x01,0x1b},std::pair{0x1e,0x3a},std::pair{0x3e,0x41}})kept.replace(first-1,last-first+1,f.mid(first-1,last-first+1));
    for(int at:{OffX,OffY,OffOuter,OffInner}){
        double value=floatAt(kept.constData()+at-1);if(!(std::abs(value)<=1e7))value=0;
        putFloat(kept.data()+at-1,at==OffInner&&type==TypeText?value:value*100);
    }
    const qint64 clearance=qint32(u32(kept,OffClearance)),width=qint32(u32(kept,OffWidth));
    setU32(kept,OffClearance,quint32(std::abs(clearance)<=10000000?clearance*100:0));
    if(type!=TypeCircle&&type!=TypeText&&std::abs(width)<10000000)setU32(kept,OffWidth,quint32(width*100));
    if(type==TypePad)setU8(kept,OffSpokesPerLayer,0);
    if(type==TypeTrack||type==TypeText)setU8(kept,OffForm,0);
    if(type==TypeText)setU8(kept,OffFill,1);
    f=kept;
}
// The points of a version 5 record (outlines, nodes, text strokes) in tenths of a micrometre.
QList<QPointF> fromVersion5(QList<QPointF> points){
    for(auto &p:points)p=QPointF(float(p.x()*100),float(p.y()*100));
    return points;
}
Record readRecord(Reader &r){
    const auto &v=r.variant;Record rec;rec.type=r.u8();
    if(rec.type!=TypePad&&rec.type!=TypeArea&&rec.type!=TypeCircle&&rec.type!=TypeTrack&&rec.type!=TypeText&&rec.type!=TypeSmd)unknownElement(r.pos-1);
    rec.fixed=r.raw(v.fixed);if(r.file==5)fromVersion5(rec.fixed,rec.type);
    rec.text=r.string();if(v.version>=5)rec.text2=r.string();
    const auto groups=r.count(100000);for(quint32 i=0;i<groups;i++)rec.groups.append(int(std::min<quint32>(r.u32(),2147483647)));
    if(rec.type==TypeText){
        const auto n=r.count(1000000);
        for(quint32 i=0;i<n;i++){
            if(r.u8()!=TypeTrack)unknownElement(r.pos-1);
            auto fixed=r.raw(v.fixed);auto points=r.points();
            if(r.file==5){fromVersion5(fixed,TypeTrack);points=fromVersion5(points);}
            rec.strokes.append({fixed,points});
        }
        // Only version 6 has component data, after every text with the designator bit.
        if(r.file>=6&&roleOf(rec.fixed)==TextRole::Designator){rec.component=r.raw(17);rec.package=r.string();rec.comment=r.string();rec.pickAndPlace=r.u8();}
    }else if(v.version<5&&rec.type==TypePad)rec.twin=qint32(r.u32());
    else if(rec.type!=TypeCircle&&!(v.version<5&&rec.type==TypeSmd)){
        rec.points=r.points(maskOpening(rec.fixed,rec.type,r.file)?1:0);
        if(r.file==5)rec.points=fromVersion5(rec.points);
    }
    return rec;
}
Element toElement(const Record &rec,const Variant &v,int file){
    Element e;const auto &f=rec.fixed;const double unit=v.unit;
    auto toModel=[unit](QPointF p){return QPointF(p.x()/unit,-p.y()/unit);};
    e.layer=u8(f,OffLayer);if(e.layer<1||e.layer>layerCount)throw FormatError(ui("Ungültiger Layer in der Sprint-Layout-Datei"));
    // The clearance to the ground plane is an integer in both variants.
    e.clearance=std::max(0.0,double(qint32(u32(f,OffClearance)))/unit);e.groups=rec.groups;e.sprint={f,rec.component,rec.text2};
    // The elements of a component share its number (version 6; the reference reads none from older files).
    if(file>=6)e.part=u16(f,OffPart);
    // Keep-out (cut-out of the ground plane) and solder mask switches of tracks, areas, circles and texts.
    if(v.version>=5&&rec.type!=TypePad&&rec.type!=TypeSmd){e.cutout=u8(f,OffCutout);e.solderMask=u8(f,OffMask);}
    const QPointF pos=toModel({number(v,f,OffX),number(v,f,OffY)});
    QPolygonF points;for(auto p:rec.points)points<<toModel(p);
    switch(rec.type){
    case TypePad:{
        e.type=ElementType::Pad;e.pos=pos;e.size=2*number(v,f,OffOuter)/unit;e.size2=2*number(v,f,OffInner)/unit;
        const int form=u8(f,OffForm);e.shape=form>=1&&form<=9?PadShape(form):PadShape::Round;e.name=ansi(rec.text);
        if(v.version<5){e.solderMask=true;updateOutline(e);break;}
        e.via=u8(f,OffVia);e.solderMask=u8(f,OffMask);
        if(u32(f,OffPadMark)==padMark){e.thermal=u8(f,OffThermal);e.thermalSpokes=u32(f,OffSpokes);e.thermalPerLayer=u8(f,OffSpokesPerLayer);e.thermalWidth=spokeWidth(u16(f,OffAngle));}
        e.points=points;e.rotation=outlineRotation(e);if(e.points.size()!=expectedOutline(e.shape))updateOutline(e);
        break;}
    case TypeSmd:
        e.type=ElementType::SmdPad;e.pos=pos;e.size=number(v,f,OffOuter)/unit;e.size2=number(v,f,OffInner)/unit;e.name=ansi(rec.text);
        if(v.version<5){e.solderMask=true;updateOutline(e);break;}
        e.solderMask=u8(f,OffMask);
        if(u32(f,OffPadMark)==padMark){e.thermal=u8(f,OffThermal);e.thermalSpokes=u32(f,OffSpokes);e.thermalWidth=spokeWidth(u16(f,OffAngle));}
        e.points=points;e.rotation=outlineRotation(e);if(e.points.size()!=4)updateOutline(e);break;
    case TypeTrack:case TypeArea:
        e.type=rec.type==TypeTrack?ElementType::Track:ElementType::Area;e.width=double(u32(f,OffWidth))/unit;e.points=points;e.name=ansi(rec.text);
        if(e.points.isEmpty())throw FormatError(ui("Eine Leiterbahn oder Fläche ohne Knoten in der Sprint-Layout-Datei"));
        e.maskOnly=maskOpening(f,rec.type,file);
        if(rec.type==TypeTrack&&v.version>=4)e.autorouted=u8(f,v.version<5?OffAutoroute4:OffAutoroute);
        while(e.points.size()<(e.type==ElementType::Track?2:3))e.points<<e.points.last();
        break;
    case TypeCircle:{
        e.type=ElementType::Circle;e.pos=pos;const double outer=number(v,f,OffOuter)/unit,inner=number(v,f,OffInner)/unit;
        e.size=(outer+inner)/2;e.width=outer-inner;e.start=normalized(arcAngle(u32(f,OffSpokes)));e.stop=normalized(arcAngle(u32(f,OffWidth)));e.filled=u8(f,OffFill);
        e.name=ansi(rec.text);break;}
    case TypeText:{
        e.type=ElementType::Text;e.pos=pos;e.size=number(v,f,OffOuter)/unit;e.text=ansi(rec.text);
        e.thickness=qBound(0,int(std::lround(v.floats?number(v,f,OffInner):double(qint32(u32(f,OffInner))))),2);
        e.style=int(std::min<quint32>(u32(f,OffWidth),2));
        // A text is mirrored left to right and top to bottom about its start as the two flags say, then turned
        // clockwise. Top to bottom is the same as left to right and half a turn. Version 4 turns texts in quarter turns
        // (0 to 3, anything else is no turn) and knows no component texts; the reference reads no text roles from
        // version 5.
        const bool horizontal=u8(f,v.version<5?OffMirror4:OffMirror),vertical=u8(f,v.version<5?OffMirrorV4:OffMirrorV);
        const quint32 quarters=u32(f,OffSpokes);
        const double clockwise=v.version<5?(quarters<4?90.0*quarters:0):double(qint32(u32(f,OffAngle)));
        e.mirrored=horizontal!=vertical;e.rotation=normalized((vertical?180:0)-clockwise);e.flipped=vertical;
        if(file>=6)e.role=roleOf(f);
        for(const auto &[fixed,line]:rec.strokes){QPolygonF s;for(auto p:line)s<<toModel(p);e.strokes.append(s);if(e.strokeWidth==0)e.strokeWidth=double(u32(fixed,OffWidth))/unit;}
        if(e.role==TextRole::Designator&&rec.component.size()==17){
            // The offset counts y downwards there, upwards in the model.
            const char *c=rec.component.constData();e.pickOffset=QPointF(offsetAt(c),0.0-offsetAt(c+4));e.pickCentre=centreAt(c);e.componentRotation=rotationAt(c);
            e.package=ansi(rec.package);e.comment=ansi(rec.comment);e.pickAndPlace=rec.pickAndPlace;
        }
        break;}
    }
    // Square track ends and hidden component texts came with version 6 (a converted version 5 record has round ends
    // and plain texts). The hatch fields of areas are read from versions 5 and 6, as the reference reads them; version
    // 4 has none of these fields.
    if(v.version>=5){
        if(rec.type==TypeTrack){const int ends=u8(f,OffForm);e.flatStart=ends&1;e.flatEnd=ends&2;}
        if(rec.type==TypeArea){e.hatched=u8(f,OffHatch);e.hatchAuto=u8(f,OffHatchAuto);e.hatchPitch=u32(f,OffHatchPitch)/unit;}
        if(rec.type==TypeText&&e.role!=TextRole::Plain)e.visible=u8(f,OffFill);
    }
    return e;
}
// Version 4 makes a through-plated pad from two pads, one on each copper side, that name each other. They become one
// pad with `via` set (the bottom one is kept), as newer versions store it; airwires of both stay with it.
void joinTwins(QList<Element> &elements,const QList<qint32> &twins){
    QList<int> keep(elements.size());for(int i=0;i<elements.size();i++)keep[i]=i;
    QList<int> drop;
    for(int i=0;i<elements.size();i++){
        const int t=twins.value(i,-1);if(t<=i||t>=elements.size()||twins.value(t,-1)!=i||drop.contains(i))continue;
        auto &a=elements[i],&b=elements[t];if(a.type!=ElementType::Pad||b.type!=ElementType::Pad||a.layer==b.layer)continue;
        const int kept=b.layer==CopperBottom?t:i,gone=kept==i?t:i;
        elements[kept].via=true;keep[gone]=kept;drop.append(gone);
        for(int c:elements[gone].connections)if(!elements[kept].connections.contains(c))elements[kept].connections.append(c);
    }
    if(drop.isEmpty())return;
    for(auto &e:elements){QList<int> c;for(int t:e.connections){const int k=keep.value(t,t);if(!c.contains(k))c.append(k);}e.connections=c;
        for(int &p:e.autoroutePads)if(p>=0)p=keep.value(p,p);}
    for(int i=0;i<elements.size();i++)elements[i].connections.removeAll(i);
    Board b;b.elements=elements;removeElements(b,drop);elements=b.elements;
}
// The pads of the autorouted tracks, after the airwires: for each marked track in element order two signed 32-bit
// element indexes. A negative one names no pad, one of an element that is no pad is taken as none, and one beyond the
// elements breaks the file. In version 4 they count the records before the two pads of a via are joined.
void readRoutedPads(Reader &r,QList<Element> &elements){
    for(auto &e:elements){
        if(e.type!=ElementType::Track||!e.autorouted)continue;
        for(int &p:e.autoroutePads){
            const auto i=qint32(r.u32());if(i>=elements.size())throw FormatError(ui("Die Sprint-Layout-Datei ist beschädigt (Position 0x%1)").arg(QString::number(r.pos-4,16)));
            p=i>=0&&(elements.at(i).type==ElementType::Pad||elements.at(i).type==ElementType::SmdPad)?int(i):-1;
        }
    }
}
// Reads the elements of a board or macro and the airwires that follow them, one list per pad in element order.
QList<Element> readElements(Reader &r){
    const auto n=r.count(1000000);QList<Element> elements;elements.reserve(n);QList<qint32> twins;
    for(quint32 i=0;i<n;i++){const auto rec=readRecord(r);twins.append(rec.twin);elements.append(toElement(rec,r.variant,r.file));}
    for(auto &e:elements){
        if(e.type!=ElementType::Pad&&e.type!=ElementType::SmdPad)continue;
        const auto c=r.count(n);for(quint32 i=0;i<c;i++){const auto t=r.u32();if(t<n)e.connections.append(int(t));}
    }
    readRoutedPads(r,elements);
    if(r.variant.version<5)joinTwins(elements,twins);
    return elements;
}

// The fixed part of a new record: what the reference writes for a fresh element of that kind (unused bytes zero).
QByteArray freshRecord(const Variant &v,quint8 type){
    QByteArray f(v.fixed,'\0');setU32(f,OffClearance,quint32(std::llround(.4*v.unit)));
    if(v.version<5)return f;
    if(type==TypePad||type==TypeSmd){setU32(f,OffSpokes,0x55);qToLittleEndian(quint16(100),f.data()+OffAngle-1);setU8(f,OffMask,1);setU32(f,OffPadMark,padMark);}
    if(type==TypeArea){setU8(f,OffHatchAuto,1);setU32(f,OffHatchPitch,60);}
    if(type==TypeText)setU8(f,OffFill,1);
    return f;
}
QByteArray lengthPrefixed(const QByteArray &b){QByteArray out(4,'\0');qToLittleEndian(quint32(b.size()),out.data());return out+b;}
QByteArray u32Bytes(quint32 v){QByteArray out(4,'\0');qToLittleEndian(v,out.data());return out;}
QByteArray pointBytes(const Variant &v,const QPolygonF &points,quint32 extra=0){
    QByteArray out=u32Bytes(quint32(points.size())+extra);
    for(auto p:points)for(double c:{p.x()*v.unit,-p.y()*v.unit}){
        char b[4];if(v.floats)putFloat(b,c);else qToLittleEndian(qint32(std::clamp<double>(std::round(c),-2147483647.0,2147483647.0)),b);out.append(b,4);
    }
    return out;
}
// The thermal fields of a pad; a record from version 5 gets the mark and clean values for the bytes it left unused.
void thermalFields(QByteArray &f,const Element &e){
    if(u32(f,OffPadMark)!=padMark){
        setU32(f,OffPadMark,padMark);for(int at:{0x37,0x3b,0x3c,0x3d})setU8(f,at,0);
        qToLittleEndian(quint16(e.thermalWidth),f.data()+OffAngle-1);setU8(f,0x38,0);
    }else setSpokeWidth(f,e.thermalWidth);
    setU32(f,OffSpokes,e.thermalSpokes);setU8(f,OffThermal,e.thermal);
    // Through-hole pads may give each copper layer its own spokes; SMD pads have one layer.
    if(e.type==ElementType::Pad)setU8(f,OffSpokesPerLayer,e.thermalPerLayer);
}
quint8 fileType(ElementType t){
    switch(t){case ElementType::Pad:return TypePad;case ElementType::SmdPad:return TypeSmd;case ElementType::Track:return TypeTrack;
        case ElementType::Area:return TypeArea;case ElementType::Circle:return TypeCircle;case ElementType::Text:return TypeText;}
    return TypeTrack;
}
// One record. In version 4, `twin` is the index of the second pad of a through-plated pad (-1 for none); in version 6,
// `part` the component number the element gets.
QByteArray recordBytes(const Variant &v,const Element &source,qint32 twin=-1,quint16 part=0){
    Element e=source;const quint8 type=fileType(e.type);const double unit=v.unit;
    QByteArray f=e.sprint.record.size()==v.fixed?e.sprint.record:freshRecord(v,type);
    auto setInteger=[&](int offset,quint32 value){if(u32(f,offset)!=value)setU32(f,offset,value);};
    setU8(f,OffLayer,quint8(e.layer));setInteger(OffClearance,quint32(std::llround(std::max(0.0,e.clearance)*unit)));
    if(v.version>=5&&u16(f,OffPart)!=part)qToLittleEndian(part,f.data()+OffPart-1);
    const QPointF pos(e.pos.x()*unit,-e.pos.y()*unit);
    switch(e.type){
    case ElementType::Pad:{
        if(e.points.size()!=expectedOutline(e.shape))updateOutline(e);   // the reference draws pads from this outline and needs all of it
        setNumber(v,f,OffX,pos.x());setNumber(v,f,OffY,pos.y());setNumber(v,f,OffOuter,e.size*unit/2);setNumber(v,f,OffInner,e.size2*unit/2);
        setU8(f,OffForm,quint8(e.shape));
        if(v.version>=5){setU8(f,OffVia,e.via);setU8(f,OffMask,e.solderMask);thermalFields(f,e);}
        break;}
    case ElementType::SmdPad:
        if(e.points.size()!=4)updateOutline(e);
        setNumber(v,f,OffX,pos.x());setNumber(v,f,OffY,pos.y());setNumber(v,f,OffOuter,e.size*unit);setNumber(v,f,OffInner,e.size2*unit);
        if(v.version>=5){setU8(f,OffMask,e.solderMask);thermalFields(f,e);}
        break;
    case ElementType::Track:case ElementType::Area:
        setInteger(OffWidth,quint32(std::llround(e.width*unit)));
        // The autoroute mark; an imported nonzero value stays as long as the track is autorouted.
        if(e.type==ElementType::Track){const int at=v.version<5?OffAutoroute4:OffAutoroute;if(!e.autorouted)setU8(f,at,0);else if(!u8(f,at))setU8(f,at,1);}
        if(v.version<5)break;
        if(e.type==ElementType::Track)setU8(f,OffForm,quint8((e.flatStart?1:0)|(e.flatEnd?2:0)));
        else{setU8(f,OffHatch,e.hatched);setU8(f,OffHatchAuto,e.hatchAuto);setInteger(OffHatchPitch,quint32(std::llround(std::max(0.0,e.hatchPitch)*unit)));
            // The mark of a solder mask opening, always written: version 5 files can hold anything there.
            setU8(f,OffMaskCutout,e.maskOnly&&(e.layer==CopperTop||e.layer==CopperBottom));}
        break;
    case ElementType::Circle:{
        setNumber(v,f,OffX,pos.x());setNumber(v,f,OffY,pos.y());setNumber(v,f,OffOuter,(e.size+e.width/2)*unit);setNumber(v,f,OffInner,(e.size-e.width/2)*unit);
        auto angle=[&](double degrees){return v.version<5?quint32(std::llround(normalized(degrees)))%360:arcValue(degrees);};
        if(std::abs(normalized(arcAngle(u32(f,OffSpokes)))-normalized(e.start))>1e-9)setU32(f,OffSpokes,angle(e.start));
        if(std::abs(normalized(arcAngle(u32(f,OffWidth)))-normalized(e.stop))>1e-9)setU32(f,OffWidth,angle(e.stop));
        setU8(f,OffFill,e.filled);break;}
    case ElementType::Text:{
        setNumber(v,f,OffX,pos.x());setNumber(v,f,OffY,pos.y());setNumber(v,f,OffOuter,e.size*unit);
        if(v.floats)setNumber(v,f,OffInner,e.thickness);else setInteger(OffInner,quint32(e.thickness));
        setInteger(OffWidth,quint32(e.style));
        // The reference stores the text angle clockwise in whole degrees (version 4 in quarter turns), and the quarter
        // turns once more. A text mirrored top to bottom keeps that flag: the model's mirror and turn are then written as
        // a horizontal mirror and an angle that give the same text with it.
        const int horizontalAt=v.version<5?OffMirror4:OffMirror,verticalAt=v.version<5?OffMirrorV4:OffMirrorV;
        const bool vertical=e.flipped;const double flip=vertical?180:0;
        if((u8(f,horizontalAt)!=0)!=(e.mirrored!=vertical)||(u8(f,verticalAt)!=0)!=vertical){setU8(f,horizontalAt,e.mirrored!=vertical);setU8(f,verticalAt,vertical);}
        const auto angle=qint32(std::lround(normalized(flip-e.rotation)))%360;const quint32 quarters=quint32(((angle+45)/90)%4);
        if(v.version<5){const quint32 q=u32(f,OffSpokes);if(std::abs(normalized(flip-(q<4?90.0*q:0))-normalized(e.rotation))>1e-9)setU32(f,OffSpokes,quarters);break;}
        if(roleOf(f)!=e.role)setU8(f,OffForm,quint8(e.role));if(e.role!=TextRole::Plain)setU8(f,OffFill,e.visible);
        if(std::abs(normalized(flip-double(qint32(u32(f,OffAngle))))-normalized(e.rotation))>1e-9){setU32(f,OffAngle,quint32(angle));setU8(f,OffSpokes,quint8(quarters));}
        break;}
    }
    if(v.version>=5&&e.type!=ElementType::Pad&&e.type!=ElementType::SmdPad){setU8(f,OffCutout,e.cutout);setU8(f,OffMask,e.solderMask);}
    QByteArray out(1,char(type));out+=f;
    out+=lengthPrefixed(ansi(e.type==ElementType::Text?e.text:e.name));if(v.version>=5)out+=lengthPrefixed(e.sprint.text2);
    out+=u32Bytes(quint32(e.groups.size()));for(int g:e.groups)out+=u32Bytes(quint32(g));
    if(e.type==ElementType::Text){
        out+=u32Bytes(quint32(e.strokes.size()));
        for(const auto &s:e.strokes){
            QByteArray sub=freshRecord(v,TypeTrack);setU8(sub,OffLayer,quint8(e.layer));setU32(sub,OffWidth,quint32(std::llround(e.strokeWidth*unit)));
            out+=char(TypeTrack);out+=sub;out+=pointBytes(v,s);
        }
        if(v.version>=5&&e.role==TextRole::Designator){
            // Fields of the component data keep their bytes while their value is unchanged; a zero offset is +0.
            QByteArray c=e.sprint.component.size()==17?e.sprint.component:QByteArray(17,'\0');
            if(float(e.pickOffset.x()*unit)!=float(offsetAt(c.constData())*unit))putFloat(c.data(),e.pickOffset.x()*unit);
            if(float(e.pickOffset.y()*unit)!=float(-offsetAt(c.constData()+4)*unit))putFloat(c.data()+4,0.0-e.pickOffset.y()*unit);
            if(centreAt(c.constData())!=e.pickCentre)c[8]=char(std::clamp(e.pickCentre,0,2));
            if(rotationAt(c.constData())!=e.componentRotation)putDouble(c.data()+9,e.componentRotation);
            out+=c;out+=lengthPrefixed(ansi(e.package));out+=lengthPrefixed(ansi(e.comment));out+=char(e.pickAndPlace?1:0);
        }
    }else if(v.version<5&&e.type==ElementType::Pad)out+=u32Bytes(quint32(twin));
    else if(e.type!=ElementType::Circle&&!(v.version<5&&e.type==ElementType::SmdPad))out+=pointBytes(v,e.points,maskOpening(f,type,v.version)?1:0);
    return out;
}
// Version 4 has neither inner layers nor the outline layer nor areas that only open the solder mask, no turned pads and
// makes a through-plated pad from two: the elements as that version can hold them, each with the index of its model
// element (-1 for the second pad of a via) and its twin. What it cannot hold is left out (and counted) before the twins
// are numbered, so that they name the records as written.
struct Placed {Element element;int source;qint32 twin=-1;};
// A text as the reference can show it: strokes from the font where none are kept (the own format may leave them out).
// The reference deletes a text without strokes when it opens the file, so one that has none even then (an empty text)
// is not written.
bool withStrokes(Element &e){if(e.type!=ElementType::Text)return true;if(e.strokes.isEmpty())updateStrokes(e);return !e.strokes.isEmpty();}
QList<Placed> forVersion4(const QList<Element> &elements,int *skipped){
    QList<Placed> out;int left=0;
    for(int i=0;i<elements.size();i++){
        Element e=elements[i];
        if(e.layer>SilkBottom||(e.type==ElementType::Area&&e.maskOnly)){left++;continue;}
        if(!withStrokes(e))continue;
        if(e.type==ElementType::Pad||e.type==ElementType::SmdPad){
            // Pads stand upright or lie across: a quarter turn swaps the long forms and the SMD sides.
            const int quarter=int(std::lround(normalized(e.rotation)/90))%4;
            if(quarter%2){
                if(e.type==ElementType::SmdPad)std::swap(e.size,e.size2);
                else{static const QMap<PadShape,PadShape> turned{{PadShape::OvalWide,PadShape::OvalTall},{PadShape::OvalTall,PadShape::OvalWide},
                        {PadShape::OctagonWide,PadShape::OctagonTall},{PadShape::OctagonTall,PadShape::OctagonWide},
                        {PadShape::SquareWide,PadShape::SquareTall},{PadShape::SquareTall,PadShape::SquareWide}};
                    e.shape=turned.value(e.shape,e.shape);}
            }
            e.rotation=0;updateOutline(e);
        }
        if(e.type==ElementType::Pad&&e.via){
            e.via=false;if(!isCopper(e.layer)||e.layer>CopperBottom)e.layer=CopperBottom;
            Element other=e;other.layer=e.layer==CopperBottom?CopperTop:CopperBottom;other.connections.clear();other.sprint={};other.name.clear();
            const int at=int(out.size());out.append({e,i,at+1});out.append({other,-1,at});
        }else out.append({e,i,-1});
    }
    if(skipped)*skipped=left;
    return out;
}
// The pads of the written autorouted tracks as indexes into the written elements (`index` maps model to written
// indexes; a via of version 4 is its first pad), -1 for a pad that is not written.
QByteArray routedPadBytes(const QList<Placed> &placed,const QMap<int,int> &index){
    QByteArray out;
    for(const auto &p:placed){
        if(p.element.type!=ElementType::Track||!p.element.autorouted)continue;
        for(int pad:p.element.autoroutePads){
            const int k=index.value(pad,-1);const bool known=k>=0&&(placed[k].element.type==ElementType::Pad||placed[k].element.type==ElementType::SmdPad);
            out+=u32Bytes(quint32(known?k:-1));
        }
    }
    return out;
}
QByteArray elementBytes(const Variant &v,const QList<Element> &elements,int *skipped){
    QList<Placed> placed;
    if(v.version<5)placed=forVersion4(elements,skipped);
    else{if(skipped)*skipped=0;for(int i=0;i<elements.size();i++){Element e=elements[i];if(withStrokes(e))placed.append({e,i,-1});}}
    // Areas that only open the solder mask: version 6 keeps those on K1 and K2; the other layers know none, and a plain
    // area would be copper (version 4 has left them all out above).
    for(qsizetype k=placed.size()-1;k>=0;k--){
        const auto &e=placed[k].element;
        if(e.type==ElementType::Area&&e.maskOnly&&e.layer!=CopperTop&&e.layer!=CopperBottom){placed.removeAt(k);if(skipped)++*skipped;}
    }
    QMap<int,int> index;for(int k=0;k<placed.size();k++)if(placed[k].source>=0)index[placed[k].source]=k;
    // Component numbers: the model's where they fit into 1 to 65534 (16 bits, without 0xFFFF), the next free ones for
    // the others.
    QMap<int,quint16> parts;
    if(v.version>=5){
        QSet<int> used;int top=0;for(const auto &p:placed){const int n=p.element.part;if(n>=1&&n<=65534){used.insert(n);top=std::max(top,n);}}
        for(const auto &p:placed){
            const int n=p.element.part;if(!n||parts.contains(n))continue;if(n>=1&&n<=65534){parts[n]=quint16(n);continue;}
            int fresh=top+1;if(fresh>65534){fresh=1;while(fresh<=65534&&used.contains(fresh))fresh++;}
            if(fresh<=65534){parts[n]=quint16(fresh);used.insert(fresh);top=std::max(top,fresh);}
        }
    }
    QByteArray out=u32Bytes(quint32(placed.size()));for(const auto &p:placed)out+=recordBytes(v,p.element,p.twin,parts.value(p.element.part,0));
    for(const auto &p:placed){
        const auto &e=p.element;if(e.type!=ElementType::Pad&&e.type!=ElementType::SmdPad)continue;
        QList<int> c;for(int t:e.connections)if(index.contains(t))c.append(index[t]);
        out+=u32Bytes(quint32(c.size()));for(int t:c)out+=u32Bytes(quint32(t));
    }
    return out+routedPadBytes(placed,index);
}
QByteArray shortString(const QString &text,int capacity){
    QByteArray b=ansi(text).left(capacity);QByteArray out(1,char(b.size()));out+=b;out.append(QByteArray(capacity-b.size(),'\0'));return out;
}
// Board header positions after the name, which differ between the variants: version 4 has switches for four layers
// instead of seven and no multilayer byte, so its templates and origin lie 8 bytes earlier. Scanned templates have, for
// the top and the bottom side, a switch to show it (`shown`, one byte each), a file name (`names`, a length byte and
// 200 characters each), the resolution in dots per inch, the offset (`offsets`: x top, x bottom, y top, y bottom) and
// the colour; the board's coordinate origin (x, y) follows.
struct HeaderLayout {int width,height,ground,grounds,grid,zoom,active,visible,visibles,shown,names,resolution,offsets,colours,origin;};
constexpr HeaderLayout header6{0x23,0x27,0x2b,7,0x32,0x3a,0x4a,0x4e,7,0x55,0x57,0x1e9,0x1f1,0x201,0x209},
                       header4{0x23,0x27,0x2b,2,0x2d,0x35,0x45,0x49,4,0x4d,0x4f,0x1e1,0x1e9,0x1f9,0x201};
// The layers the AutoMasse and visibility switches stand for: all seven in versions 5/6, K1/K2 and K1, B1, K2, B2 in 4.
int groundLayer(const Variant &v,int k){return v.version<5?(k==0?CopperTop:CopperBottom):k+1;}
double i32At(const QByteArray &h,int at){return qFromLittleEndian<qint32>(h.constData()+at);}
// The origin counts from the top left corner of the working area with y up, in `unit` per millimetre. Beyond the
// model's 10 m it stands for the top left corner (the stored bytes stay while the board keeps that).
QPointF originAt(const QByteArray &h,const HeaderLayout &l,double unit){
    const QPointF o(i32At(h,l.origin)/unit,-i32At(h,l.origin+4)/unit);
    return std::abs(o.x())<=10000&&std::abs(o.y())<=10000?o:QPointF();
}
// Template offsets: tenths of a millimetre from the top left corner of the working area to that of the picture, y down;
// the reference takes anything beyond 300 mm as that.
double offsetAt(const QByteArray &h,int at){return std::clamp(i32At(h,at),-3000.0,3000.0)/10;}
// A template resolution in dots per inch: the reference shows values beyond its dialog's 20 to 2400 at that limit; 0 is
// no resolution (the usual 600).
double dpiAt(const QByteArray &h,int at){const qint32 dpi=qFromLittleEndian<qint32>(h.constData()+at);return dpi==0?600:std::clamp<double>(dpi,20,2400);}
// Template colours as Windows keeps them (0x00bbggrr).
QColor colourAt(const QByteArray &h,int at){const auto c=qFromLittleEndian<quint32>(h.constData()+at);return QColor(c&0xff,(c>>8)&0xff,(c>>16)&0xff);}
// The board header of version 5 has the layout of version 6 with lengths in hundredths of a millimetre. It becomes one
// of version 6 as the reference converts it: width, height and grid times 100, the zoom divided by 100; templates and
// the rest stay. The reference divides the origin by 10 only; OpenLoch takes it in the unit of the other lengths and
// multiplies it by 100 as well. Values beyond 32 bits become the largest ones, which the reader refuses or ignores.
void headerFromVersion5(QByteArray &h){
    const auto &l=header6;
    for(int at:{l.width,l.height})qToLittleEndian(quint32(std::min<quint64>(quint64(qFromLittleEndian<quint32>(h.constData()+at))*100,0xffffffffu)),h.data()+at);
    putDouble(h.data()+l.grid,doubleAt(h.constData()+l.grid)*100);
    putDouble(h.data()+l.zoom,doubleAt(h.constData()+l.zoom)/100);
    for(int at:{l.origin,l.origin+4}){
        const qint64 v=qint64(qFromLittleEndian<qint32>(h.constData()+at))*100;
        qToLittleEndian(qint32(std::clamp<qint64>(v,std::numeric_limits<qint32>::min(),std::numeric_limits<qint32>::max())),h.data()+at);
    }
}
QByteArray headerBytes(const Variant &v,const Board &b){
    const auto &l=v.version<5?header4:header6;
    QByteArray h=b.sprintHeader.size()==v.header?b.sprintHeader:QByteArray(v.header,'\0');
    if(b.sprintHeader.size()!=v.header){
        putDouble(h.data()+l.zoom,v.version<5?.05:.0004);
        qToLittleEndian<quint32>(600,h.data()+l.resolution);qToLittleEndian<quint32>(600,h.data()+l.resolution+4);   // resolution of scanned templates
    }
    if(ansi(h.mid(1,std::min<int>(quint8(h[0]),30)))!=b.name)h.replace(0,31,shortString(b.name,30));
    qToLittleEndian(quint32(std::llround(b.width*v.unit)),h.data()+l.width);qToLittleEndian(quint32(std::llround(b.height*v.unit)),h.data()+l.height);
    for(int k=0;k<l.grounds;k++)h[l.ground+k]=char(b.groundPlane[groundLayer(v,k)]);
    for(int k=0;k<l.visibles;k++)h[l.visible+k]=char(b.visible[k+1]);
    if(doubleAt(h.constData()+l.grid)/v.unit!=b.grid)putDouble(h.data()+l.grid,b.grid*v.unit);
    h[l.active]=char(v.version<5&&b.activeLayer>SilkBottom?CopperBottom:b.activeLayer);
    if(v.version>=5)h[v.header-1]=char(b.multilayer);    // the last byte marks boards with inner layers
    // Values that read as the model has them stay as they are, also offsets beyond the reference's limit.
    for(int k=0;k<2;k++){
        const auto &t=b.templates[k];const int name=l.names+201*k;h[l.shown+k]=char(t.shown);
        if(ansi(h.mid(name+1,std::min<int>(quint8(h[name]),200)))!=t.file)h.replace(name,201,shortString(t.file,200));
        if(!t.file.isEmpty()&&dpiAt(h,l.resolution+4*k)!=t.dpi)qToLittleEndian(qint32(std::clamp<double>(std::round(t.dpi),20,2400)),h.data()+l.resolution+4*k);
        const double offset[2]={t.offset.x(),t.offset.y()};
        for(int axis=0;axis<2;axis++){const int at=l.offsets+8*axis+4*k;if(offsetAt(h,at)!=offset[axis])qToLittleEndian(qint32(std::clamp<double>(std::round(offset[axis]*10),-3000,3000)),h.data()+at);}
        if(colourAt(h,l.colours+4*k).rgb()!=t.colour.rgb())qToLittleEndian(quint32(t.colour.red()|t.colour.green()<<8|t.colour.blue()<<16),h.data()+l.colours+4*k);
    }
    // The origin in the length unit of the variant.
    if(originAt(h,l,v.unit)!=b.origin)for(int k=0;k<2;k++)qToLittleEndian(qint32(std::clamp<double>(std::round((k?-b.origin.y():b.origin.x())*v.unit),-2147483647.0,2147483647.0)),h.data()+l.origin+4*k);
    return h;
}
// --- Files older than Sprint-Layout 4.0
// Version 3 has a record of 74 bytes with the text inside it, versions 0 to 2 one of 49 bytes with 16-bit numbers and
// counts; lengths are hundredths of a millimetre. They are converted as Sprint-Layout 6 converts them when opening:
// copper layers 1 and 3 change places, old texts become stroke texts, and nothing is kept for writing them again.
enum OldType : quint8 {OldLine=1,OldText=3};
struct OldRecord {
    int type=0,layer=0,shape=0;qint32 x=0,y=0,a=0,b=0,c=0,d=0,clearance=0;bool filled=false;
    QByteArray text;QList<qint64> groups;QList<QPointF> points;qint32 partner=-1;QList<quint32> wires;
};
struct OldCounts {int texts=0,skipped=0,wires=0;};     // converted texts, elements and airwires left out
quint16 readU16(Reader &r){const auto b=r.raw(2);return qFromLittleEndian<quint16>(b.constData());}
OldRecord readOldRecord(Reader &r){
    OldRecord rec;rec.type=r.u8();const bool wide=r.file==3;const auto f=r.raw(wide?73:48);
    auto number=[&](int wideAt,int narrowAt){return wide?qint32(u32(f,wideAt)):qint32(qint16(u16(f,narrowAt)));};
    rec.x=number(1,1);rec.y=number(5,3);rec.a=number(9,5);rec.b=number(13,7);rec.c=number(17,9);rec.d=number(47,37);
    rec.layer=u8(f,wide?22:12);rec.shape=u8(f,wide?23:13);
    if(rec.type==OldText){const int at=wide?24:14;rec.text=f.mid(at,std::min<int>(u8(f,at),15));}
    if(wide){rec.filled=u8(f,56);rec.clearance=qint32(u32(f,57));}
    // Groups: 32 bits in version 3, 16 bits with a sign in version 2, none before.
    if(wide){const auto n=r.count(100000);for(quint32 i=0;i<n;i++)rec.groups<<qint64(r.u32());}
    else if(r.file==2){const int n=readU16(r);for(int i=0;i<n;i++)rec.groups<<qint64(qint16(readU16(r)));}
    if(rec.type==TypeArea||rec.type==TypeTrack){
        if(wide)rec.points=r.points();
        else{const int n=readU16(r);for(int i=0;i<n;i++){const qint16 x=qint16(readU16(r)),y=qint16(readU16(r));rec.points<<QPointF(x,y);}}
    }
    if(wide&&rec.type==TypePad)rec.partner=qint32(r.u32());
    return rec;
}
// The elements of a board or macro of versions 0 to 3. Converted texts follow all other elements, as in Sprint-Layout 6.
QList<Element> readOldElements(Reader &r,bool macro,OldCounts &counts){
    const bool wide=r.file==3;const int n=wide?int(r.count(1000000)):readU16(r);
    QList<OldRecord> recs;for(int i=0;i<n;i++)recs.append(readOldRecord(r));
    if(wide)for(auto &rec:recs)if(rec.type==TypePad){const auto c=r.count(n);for(quint32 i=0;i<c;i++)rec.wires<<r.u32();}
    // Version 3 makes a through-plated pad from two pads that name each other: the first one in the file stays. A pad that
    // names itself is removed, with its airwires, as Sprint-Layout 6 removes it.
    QList<int> keeper(n);QList<bool> via(n,false);for(int i=0;i<n;i++)keeper[i]=i;
    for(int i=0;i<n;i++){
        const int p=recs[i].partner;
        if(recs[i].type!=TypePad||keeper[i]!=i||p<0||p>=n||recs[p].type!=TypePad||recs[p].partner!=i)continue;
        if(p==i){keeper[i]=-1;continue;}
        keeper[p]=i;via[i]=true;
    }
    // Lengths beyond 100 m count as 0; a line width beyond it is kept in tenths of a micrometre.
    auto range=[](qint32 v){return v>=-10000000&&v<=10000000?v:0;};
    auto at=[&](qint32 x,qint32 y){return QPointF(range(x)/100.0,-range(y)/100.0);};
    // Group numbers start at 1 for every board. Version 1 has no group lists, and its keys make no group, as in Sprint-Layout 6.
    QMap<qint64,int> numbers;auto group=[&](qint64 g){if(!numbers.contains(g))numbers.insert(g,int(numbers.size())+1);return numbers[g];};
    QList<Element> out(n);QList<int> drop;QList<Element> texts;
    for(int i=0;i<n;i++){
        const auto &rec=recs[i];Element &e=out[i];drop<<i;     // taken back below for the elements that stay
        if(rec.type==0||rec.layer==0||keeper[i]!=i)continue;
        if(rec.type==TypeText||rec.type>TypeSmd){counts.skipped++;continue;}    // type 7 has no strokes in these versions
        // Old layer 1 is the bottom copper, 3 the top; old macros keep their pads on 5 and the silkscreen on 6.
        e.layer=rec.layer==1?CopperBottom:rec.layer==3?CopperTop:macro&&rec.layer==5?CopperBottom:macro&&rec.layer==6?SilkTop:rec.layer;
        if(e.layer<1||e.layer>layerCount)throw FormatError(ui("Ungültiger Layer in der Sprint-Layout-Datei"));
        for(auto g:rec.groups)e.groups<<group(g);
        e.clearance=wide?std::max(0.0,range(rec.clearance)/100.0):.4;
        const double width=std::max(0.0,std::abs(rec.c)<10000000?rec.c/100.0:rec.c/10000.0);
        switch(rec.type){
        case OldLine:case TypeTrack:case TypeArea:
            e.type=rec.type==TypeArea?ElementType::Area:ElementType::Track;e.width=width;
            if(rec.type==OldLine)e.points={at(rec.x,rec.y),at(rec.a,rec.b)};else for(auto p:rec.points)e.points<<QPointF(p.x()/100,-p.y()/100);
            if(e.points.size()<2)continue;
            if(e.type==ElementType::Area){e.hatchPitch=.006;while(e.points.size()<3)e.points<<e.points.last();}   // Sprint-Layout 6 gives such areas 60
            break;
        case TypePad:
            e.type=ElementType::Pad;e.pos=at(rec.x,rec.y);e.size=std::max(0,2*range(rec.a))/100.0;e.size2=std::max(0,2*range(rec.b))/100.0;
            e.shape=rec.shape>=1&&rec.shape<=9?PadShape(rec.shape):PadShape::Round;e.via=via[i]&&isCopper(e.layer);e.solderMask=true;updateOutline(e);break;
        case TypeSmd:
            e.type=ElementType::SmdPad;e.pos=at(rec.x,rec.y);e.size=std::max(0,range(rec.a))/100.0;e.size2=std::max(0,range(rec.b))/100.0;e.solderMask=true;updateOutline(e);break;
        case TypeCircle:{
            e.type=ElementType::Circle;e.pos=at(rec.x,rec.y);const double outer=range(rec.a)/100.0,inner=range(rec.b)/100.0;
            e.size=std::max(0.0,(outer+inner)/2);e.width=std::max(0.0,outer-inner);
            // Version 3: whole degrees while both angles are below 1000, else thousandths. Older versions draw full rings.
            if(wide){const double scale=rec.d<1000&&rec.c<1000?1:.001;e.start=normalized(rec.d*scale);e.stop=normalized(rec.c*scale);e.filled=rec.filled;}
            break;}
        case OldText:{
            // A stroke text with thick strokes in the normal style. Its height comes from the stored font size as
            // Sprint-Layout 6 computes it for a 96 dpi screen, to 0.1 mm. The box around its strokes, widened by the
            // clearance, puts the corner that was the old text's top left before turning on the stored point.
            e.type=ElementType::Text;e.text=ansi(rec.text);e.size=qBound(.1,std::round(-double(rec.c)*.0675)/10,50.0);e.style=1;e.thickness=2;
            const int quarter=rec.d==90||rec.d==180||rec.d==270?rec.d/90:0;e.rotation=normalized(-90.0*quarter);e.clearance=.4;updateStrokes(e);
            if(e.strokes.isEmpty())continue;
            const QRectF box=bounds(e).adjusted(-e.clearance,-e.clearance,e.clearance,e.clearance);
            pcb::move(e,at(rec.x,rec.y)-(quarter==0?box.topLeft():quarter==1?box.topRight():quarter==2?box.bottomRight():box.bottomLeft()));
            texts<<e;counts.texts++;continue;}
        default:continue;
        }
        drop.removeLast();
    }
    // Airwires by element index; those of the second pad of a through-plated pad, and those to it, go to the first.
    for(int i=0;i<n;i++)for(quint32 t:recs[i].wires){
        if(t>=quint32(n)){counts.wires++;continue;}
        const int from=keeper[i],to=keeper[int(t)];if(from<0||to<0)continue;auto &c=out[from].connections;if(to!=from&&!c.contains(to))c<<to;
    }
    Board b;b.elements=out;removeElements(b,drop);return b.elements+texts;
}
void oldNotes(QStringList *notes,int version,bool layout,const OldCounts &counts){
    if(!notes)return;
    if(layout)notes->append(ui("Datei aus Sprint-Layout vor Version 4.0 (Dateiversion %1): Die alten Kupferlayer 1 und 3 sind jetzt K2 und K1.").arg(version));
    if(counts.texts)notes->append(ui("%1 alte Texte wurden in Strichschrift umgewandelt.").arg(counts.texts));
    if(counts.skipped)notes->append(ui("%1 Elemente unbekannter Art wurden übergangen.").arg(counts.skipped));
    if(counts.wires)notes->append(ui("%1 Luftlinien zu fehlenden Elementen wurden übergangen.").arg(counts.wires));
}
Document readOldLayout(const QByteArray &bytes,QStringList *notes){
    Reader r(bytes,old);r.pos=4;Document d;OldCounts counts;
    auto checkSize=[](const Board &b){if(b.width<=0||b.height<=0||b.width>10000||b.height>10000)throw FormatError(ui("Ungültige Platinengröße in der Sprint-Layout-Datei"));};
    if(r.file<3){
        // Versions 0 to 2: only the size of one board, which gets the default name; coordinates count from its bottom left corner.
        Board b;b.name=ui("Platine 1");b.width=qint32(r.u32())/100.0;b.height=qint32(r.u32())/100.0;b.origin={0,b.height};checkSize(b);
        b.elements=readOldElements(r,false,counts);d.boards.append(b);
    }else{
        const auto boards=r.count(1000);if(!boards)throw FormatError(ui("Die Datei enthält keine Platine"));
        for(quint32 i=0;i<boards;i++){
            // The board header of version 4 without the origin; the layer visibility is not taken.
            Board b;const auto h=r.raw(513);auto i32=[&](int at){return qFromLittleEndian<qint32>(h.constData()+at);};
            b.name=ansi(h.mid(1,std::min<int>(quint8(h[0]),30)));b.width=i32(35)/100.0;b.height=i32(39)/100.0;checkSize(b);
            // The two ground plane switches change places with the layers; unequal ones are both inverted first.
            const quint8 first=h[43],second=h[44];b.groundPlane[CopperTop]=(first==second?first:first^1)!=0;b.groundPlane[CopperBottom]=(first==second?second:second^1)!=0;
            const double grid=doubleAt(h.constData()+45)/100;if(std::isfinite(grid)&&grid>0&&grid<1000)b.grid=grid;
            if(i32(69)>=1&&i32(69)<=layerCount)b.activeLayer=i32(69);
            for(int k=0;k<2;k++){
                auto &t=b.templates[k];t.shown=h[77+k]!=0;const int name=k?280:79;t.file=ansi(h.mid(name+1,std::min<int>(quint8(h[name]),200)));
                t.dpi=dpiAt(h,481+4*k);
                t.offset=QPointF(offsetAt(h,489+4*k),offsetAt(h,497+4*k));t.colour=colourAt(h,505+4*k);
            }
            b.elements=readOldElements(r,false,counts);d.boards.append(b);
        }
        // The board shown, then 8 bytes without effect.
        d.activeBoard=qBound(0,int(qint32(r.u32())),int(boards)-1);r.raw(8);
    }
    oldNotes(notes,r.file,true,counts);
    return d;
}

const Variant &variantFor(const QByteArray &bytes,const QString &notSprint){
    const int v=fileVersion(bytes);if(v<0)throw FormatError(notSprint);
    if(v>6)throw FormatError(ui("Die Datei stammt aus einer neueren Sprint-Layout-Version (Version %1) und kann nicht gelesen werden").arg(v));
    return v<5?old:current;
}
QByteArray versionBytes(int version){return QByteArray::fromHex(version==4?"0433aaff":"0633aaff");}
}

int fileVersion(const QByteArray &bytes){
    if(bytes.size()<4||quint8(bytes[1])!=0x33||quint8(bytes[2])!=0xaa||quint8(bytes[3])!=0xff)return -1;
    const int v=quint8(bytes[0]);return v<=11?v:-1;
}
Document readLayout(const QByteArray &bytes,QStringList *notes){
    const Variant v=variantFor(bytes,ui("Keine Sprint-Layout-Datei"));if(quint8(bytes[0])<4)return readOldLayout(bytes,notes);
    const auto &l=v.version<5?header4:header6;
    Reader r(bytes,v);r.pos=4;Document d;const auto boards=r.count(1000);
    if(!boards)throw FormatError(ui("Die Datei enthält keine Platine"));
    for(quint32 i=0;i<boards;i++){
        Board b;b.sprintHeader=r.raw(v.header);if(r.file==5)headerFromVersion5(b.sprintHeader);const auto &h=b.sprintHeader;
        const int n=std::min<int>(quint8(h[0]),30);b.name=ansi(h.mid(1,n));
        b.width=qFromLittleEndian<quint32>(h.constData()+l.width)/v.unit;b.height=qFromLittleEndian<quint32>(h.constData()+l.height)/v.unit;
        const double grid=doubleAt(h.constData()+l.grid)/v.unit;if(std::isfinite(grid)&&grid>0&&grid<1000)b.grid=grid;
        const int active=quint8(h[l.active]);if(active>=1&&active<=layerCount)b.activeLayer=active;
        for(int k=0;k<l.grounds;k++)b.groundPlane[groundLayer(v,k)]=h[l.ground+k]!=0;
        for(int k=0;k<l.visibles;k++)b.visible[k+1]=h[l.visible+k]!=0;
        if(v.version>=5)b.multilayer=h[v.header-1]!=0;
        for(int k=0;k<2;k++){
            auto &t=b.templates[k];const int name=l.names+201*k;t.shown=h[l.shown+k]!=0;t.file=ansi(h.mid(name+1,std::min<int>(quint8(h[name]),200)));
            t.dpi=dpiAt(h,l.resolution+4*k);
            t.offset=QPointF(offsetAt(h,l.offsets+4*k),offsetAt(h,l.offsets+8+4*k));t.colour=colourAt(h,l.colours+4*k);
        }
        // The origin in the length unit of the variant (version 5 is converted above).
        b.origin=originAt(h,l,v.unit);
        if(b.width<=0||b.height<=0||b.width>10000||b.height>10000)throw FormatError(ui("Ungültige Platinengröße in der Sprint-Layout-Datei"));
        b.elements=readElements(r);d.boards.append(b);
    }
    // Project data: the board shown after loading, title, author, company and a comment.
    if(r.pos<bytes.size()){
        d.activeBoard=int(std::min<quint32>(r.u32(),boards-1));
        d.title=ansi(r.shortString(100));d.author=ansi(r.shortString(100));d.company=ansi(r.shortString(100));d.comment=ansi(r.string());
    }
    return d;
}
QByteArray writeLayout(const Document &document,int version,int *skipped){
    const auto &v=version==4?old:current;int left=0;
    QByteArray out=versionBytes(v.version)+u32Bytes(quint32(document.boards.size()));
    for(const auto &b:document.boards){int s=0;out+=headerBytes(v,b)+elementBytes(v,b.elements,&s);left+=s;}
    out+=u32Bytes(quint32(qBound(0,document.activeBoard,int(document.boards.size())-1)));
    out+=shortString(document.title,100)+shortString(document.author,100)+shortString(document.company,100)+lengthPrefixed(ansi(document.comment));
    if(skipped)*skipped=left;
    return out;
}
QList<Element> readMacro(const QByteArray &bytes,QStringList *notes){
    const Variant v=variantFor(bytes,ui("Kein Sprint-Layout-Makro"));
    Reader r(bytes,v);r.pos=4;
    if(r.file<4){OldCounts counts;const auto elements=readOldElements(r,true,counts);oldNotes(notes,r.file,false,counts);return elements;}
    return readElements(r);
}
QByteArray writeMacro(const QList<Element> &elements,int version,int *skipped){
    const auto &v=version==4?old:current;
    return versionBytes(v.version)+elementBytes(v,elements,skipped)+QByteArray(macroTrailer,'\0');
}
QStringList readMacroText(const QByteArray &bytes){
    const int version=fileVersion(bytes);if(version<4||version>6||bytes.size()<4+macroTrailer)return {};
    const QByteArray end=bytes.right(macroTrailer);QStringList lines;
    for(int at:{0,macroTrailer/2})lines<<ansi(end.mid(at+1,std::min<int>(quint8(end[at]),macroTrailer/2-1)));
    return lines;
}
QByteArray withMacroText(QByteArray macro,const QString &first,const QString &second){
    if(readMacroText(macro).isEmpty())return macro;
    macro.chop(macroTrailer);return macro+shortString(first,macroTrailer/2-1)+shortString(second,macroTrailer/2-1);
}
bool mirroredVertically(const Element &text){return text.type==ElementType::Text&&text.flipped;}
}
