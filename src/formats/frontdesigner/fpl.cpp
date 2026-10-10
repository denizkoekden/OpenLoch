#include "fpl.h"
#include "fdplot.h"
#include "fdshapes.h"
#include "strokefont.h"
#include "delphistream.h"
#include "panelgeometry.h"
#include "language.h"
#include "legacy_reader.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <tuple>

namespace openloch::frontdesigner {
using namespace frontpanel;
namespace {
constexpr double unit=50;                    // file units per millimetre
constexpr double newestVersion=3.16;
constexpr int maxDepth=32,maxObjects=200000,maxPoints=200000,maxPanels=100,maxViews=10000;
constexpr qint64 maxFile=128*1024*1024;
const QString key=QStringLiteral("frontDesigner");  // foreign values of this format
constexpr double pi=std::numbers::pi;

QString hex(const QByteArray &b){return QString::fromLatin1(b.toHex());}
QByteArray unhex(const QJsonValue &v){return QByteArray::fromHex(v.toString().toLatin1());}
QString b64(const QByteArray &b){return QString::fromLatin1(b.toBase64());}
QByteArray unb64(const QJsonValue &v){return QByteArray::fromBase64(v.toString().toLatin1());}
// Colours are stored as red, green, blue and a flag byte.
QColor colourOf(const QByteArray &b){return QColor(quint8(b[0]),quint8(b[1]),quint8(b[2]));}
QByteArray colourBytes(const QColor &c,int flags=0){QByteArray b(4,0);b[0]=char(c.red());b[1]=char(c.green());b[2]=char(c.blue());b[3]=char(flags);return b;}
// A colour with its flag byte (palette or system colour bits), which is kept apart and written back.
QColor colourWithFlags(const QByteArray &b,QJsonObject &fd,const QString &name){if(b[3])fd[name+"Flags"]=int(quint8(b[3]));return colourOf(b);}
QString shortText(const QByteArray &b,int capacity){return b.isEmpty()?QString():fromWindows1252(b.mid(1,std::min<int>({int(quint8(b[0])),capacity,int(b.size())-1})));}
QByteArray shortString(const QString &text,int capacity){QByteArray b(capacity+1,0);const QByteArray t=toWindows1252(text).left(capacity);b[0]=char(t.size());b.replace(1,t.size(),t);return b;}
QString fingerprint(const Element &e){return hex(QCryptographicHash::hash(QJsonDocument(elementToJson(e,false)).toJson(QJsonDocument::Compact),QCryptographicHash::Sha1));}
QPointF pointOf(const QByteArray &twenty){return {fromExtended(twenty.constData())/unit,fromExtended(twenty.constData()+10)/unit};}
QByteArray pointBytes(QPointF p){return toExtended(p.x()*unit)+toExtended(p.y()*unit);}
// The print settings of a panel as the file keeps them (from 3,05 on): tiles across and down, the sheet printed
// alone, the gaps between tiles, 13 options, the scale and the first tile's place in file units from the corner of
// the printable area, with the sign turned; from 3,16 whether texts are printed.
PrintSettings printSettingsOf(const QJsonObject &fd){
    PrintSettings s;s.texts=fd["value4"].toBool(true);
    const QJsonArray counts=fd["print"].toArray(),flags=fd["printFlags"].toArray(),offset=fd["printOffset"].toArray();
    if(counts.size()!=3||flags.size()!=13||offset.size()!=2)return s;
    s.tilesX=std::clamp(counts[0].toInt(),1,PrintSettings::maxTiles);s.tilesY=std::clamp(counts[1].toInt(),1,PrintSettings::maxTiles);s.sheet=std::max(1,counts[2].toInt());
    const QByteArray gaps=unhex(fd["printArea"]);
    if(gaps.size()==20){const QPointF g=pointOf(gaps);if(std::isfinite(g.x())&&std::isfinite(g.y())){s.gapX=std::clamp(g.x(),0.0,1000.0);s.gapY=std::clamp(g.y(),0.0,1000.0);}}
    bool *options[]={&s.machining,&s.objects,&s.data,&s.rulers,&s.frame,&s.background,&s.mirror,&s.landscape,&s.original,&s.centred,&s.dimensions,&s.cutMarks,&s.onlyOne};
    for(int i=0;i<13;i++)*options[i]=flags[i].toBool();
    const QByteArray zoom=unhex(fd["printScale"]);
    if(zoom.size()==10){const double z=fromExtended(zoom.constData());if(std::isfinite(z)&&z>=0.01&&z<=100)s.zoom=z;}
    s.left=-offset[0].toInt()/unit;s.top=-offset[1].toInt()/unit;
    return s;
}
bool printBlockKept(const QJsonObject &fd){
    return fd["print"].toArray().size()==3&&unhex(fd["printArea"]).size()==20&&fd["printFlags"].toArray().size()==13&&unhex(fd["printScale"]).size()==10
        &&fd["printOffset"].toArray().size()==2;
}
// The area of a saved view: left, top, right and bottom.
bool viewArea(const QByteArray &forty,QRectF *area){
    if(forty.size()!=40)return false;
    const QPointF a=pointOf(forty.left(20)),b=pointOf(forty.mid(20,20));
    for(double v:{a.x(),a.y(),b.x(),b.y()})if(!std::isfinite(v)||std::abs(v)>1e5)return false;
    *area=QRectF(a,b).normalized();return true;
}
QByteArray viewBytes(const QRectF &r){return pointBytes(r.topLeft())+pointBytes(r.bottomRight());}
// Corner forms: 0 B-spline, 1 chamfer, 2 round, 3 the B-spline of arcs (open without the pieces to the end points).
Corners cornersOf(bool smooth,int style){if(!smooth)return Corners::Sharp;return style==1?Corners::Chamfer:style==2?Corners::Round:style==3?Corners::ArcSpline:Corners::Spline;}
int cornerStyleOf(Corners c,int fallback){switch(c){case Corners::Spline:return 0;case Corners::Chamfer:return 1;case Corners::Round:return 2;case Corners::ArcSpline:return 3;default:return fallback;}}
// The class an element of these kinds is written as: the one it was read as while that still fits.
QString contourClass(const Element &e,const QString &c){
    switch(e.type){
    case ElementType::Line:return QStringList{"TLinie","TRechteck","TPolygon","TSkaleBogen","TBogen"}.contains(c)?c:"TLinie";
    case ElementType::Polygon:return QStringList{"TRechteck","TPolygon","TSkaleBogen","TKreis","TBogen"}.contains(c)?c:"TPolygon";
    case ElementType::Rectangle:return QStringList{"TRechteck","TPolygon","TSkaleBogen"}.contains(c)?c:"TRechteck";
    case ElementType::Ellipse:return "TKreis";
    case ElementType::Arc:return "TBogen";
    default:return {};
    }
}
// The 16 support points of a circle or ellipse, pushed out by 1/cos(11.25 degrees) like the original's.
QPolygonF ellipseSupport(const Element &e){
    const QTransform m=ellipseMap(e);const double out=1/std::cos(pi/16);QPolygonF inner;
    for(int k=1;k<=16;k++){const double a=k*pi/8;inner<<m.map(QPointF(out*std::cos(a),out*std::sin(a)));}
    return inner;
}
// The points of an arc from the start ray clockwise (on screen) to the end ray, pushed out like the original does, and
// its parameters: centre, radius point, start and end ray. The spline through the points begins and ends in the middle
// of the outer edges, so the points reach half a step beyond the arc.
QPolygonF arcSupport(const Element &e,QByteArray *params){
    const int n=21;const double step=std::abs(e.spanAngle)/(n-2)*pi/180,out=step>0?1/std::cos(step/2):1;QPolygonF p;
    for(int k=0;k<n;k++){const QPointF q=ellipsePoint(e,e.startAngle+e.spanAngle*(k-0.5)/(n-2));p<<e.center+(q-e.center)*out;}
    const bool counterClockwise=(e.spanAngle>=0)==(ellipseMap(e).determinant()>0);
    if(counterClockwise)std::reverse(p.begin(),p.end());
    QPointF first=ellipsePoint(e,e.startAngle),last=ellipsePoint(e,e.startAngle+e.spanAngle);if(counterClockwise)std::swap(first,last);
    if(params)*params=pointBytes(e.center)+pointBytes(first)+pointBytes(first)+pointBytes(last);
    return p;
}
// The stored point values of an unchanged object, when they belong to these points.
QByteArray storedPoints(const QJsonObject &fd,qsizetype count){const QByteArray b=unb64(fd["points"]);return b.size()==count*20?b:QByteArray();}
// The stored support points of a circle or arc, however many the original made.
QByteArray storedSupport(const QJsonObject &fd){const QByteArray b=unb64(fd["points"]);return b.size()>=20&&b.size()%20==0?b:QByteArray();}
// The four corners of a text as stored: top left, top right, bottom right, bottom left (the stored ones while unchanged).
QByteArray textCorners(const Element &e,const QJsonObject &fd,bool unchanged){
    const QByteArray stored=unhex(fd["corners"]);if(unchanged&&stored.size()==80)return stored;
    const QPolygonF c=frameCorners(e.frame);QByteArray out;for(int i=0;i<4;i++)out+=pointBytes(c.value(i));return out;
}
// The font height of a text in file units: the stored one while it still fits the frame.
int textHeight(const Element &e,const QJsonObject &fd){
    const QPolygonF c=frameCorners(e.frame);const double height=c.size()==4?QLineF(c[0],c[3]).length()*unit:0;const int stored=fd["fontHeight"].toInt();
    return stored>0&&std::abs(stored-height)<=0.02*height+1?stored:qRound(height);
}
// Two flags tell that the text was mirrored, the first across, the second upside down. The corners show the mirroring
// already, so only whether exactly one is set matters; a text mirrored here gets the first one.
std::pair<bool,bool> textFlags(const Element &e,const QJsonObject &fd){
    const QPolygonF c=frameCorners(e.frame);const QPointF u=c.value(1)-c.value(0),v=c.value(3)-c.value(0);const bool mirrored=u.x()*v.y()-u.y()*v.x()<0;
    bool first=fd["flagA"].toBool(),second=fd["flagB"].toBool();if((first!=second)!=mirrored){first=mirrored;second=false;}
    return {first,second};
}

// Circles and arcs are stored as points at equal angles, pushed out by 1/cos(half step) so that the B-spline through
// them runs on the curve. These fits find the ellipse those points belong to, also after turning or stretching.
bool fitEllipse(const QPolygonF &p,Element &e){
    const int n=p.size();if(n<8)return false;
    QPointF c;for(auto q:p)c+=q;c/=n;QPointF u,v;
    for(int k=0;k<n;k++){const double a=2*pi*k/n;u+=(p[k]-c)*std::cos(a);v+=(p[k]-c)*std::sin(a);}
    u*=2.0/n;v*=2.0/n;const double size=std::hypot(u.x(),u.y())+std::hypot(v.x(),v.y());if(size<=1e-9)return false;
    for(int k=0;k<n;k++){const double a=2*pi*k/n;if(QLineF(c+u*std::cos(a)+v*std::sin(a),p[k]).length()>std::max(0.015,1e-4*size))return false;}
    const double shrink=std::cos(pi/n);u*=shrink;v*=shrink;
    setEllipseMap(e,QTransform(u.x(),u.y(),-v.x(),-v.y(),c.x(),c.y()),false);return true;
}
bool fitArc(const QPolygonF &p,Element &e){
    // Equal steps on an ellipse satisfy p[k-1] + p[k+1] = 2 cos(step) p[k] + (2 - 2 cos(step)) centre.
    const int n=p.size();if(n<4)return false;
    double a11=0,a12=0,a13=0,a22=0,b1=0,b2=0,b3=0;
    for(int k=1;k+1<n;k++){
        const double x=2*p[k].x(),y=2*p[k].y(),sx=p[k-1].x()+p[k+1].x(),sy=p[k-1].y()+p[k+1].y();
        a11+=x*x+y*y;a12+=x;a13+=y;a22+=1;b1+=x*sx+y*sy;b2+=sx;b3+=sy;
    }
    // [[a11,a12,a13],[a12,a22,0],[a13,0,a22]] (c, dx, dy) = (b1, b2, b3)
    const double det=a11*a22*a22-a12*a12*a22-a13*a13*a22;if(std::abs(det)<1e-12)return false;
    const double c=(b1*a22*a22-a12*b2*a22-a13*b3*a22)/det;
    const double dx=(b2-a12*c)/a22,dy=(b3-a13*c)/a22;
    if(!(c>-1&&c<1-1e-12))return false;
    const double step=std::acos(c);const QPointF centre=QPointF(dx,dy)/(2-2*c);
    const QPointF u=p[0]-centre,v=(p[1]-centre-u*std::cos(step))/std::sin(step);
    const double size=std::hypot(u.x(),u.y())+std::hypot(v.x(),v.y());if(size<=1e-9)return false;
    for(int k=0;k<n;k++)if(QLineF(centre+u*std::cos(k*step)+v*std::sin(k*step),p[k]).length()>std::max(0.015,1e-4*size))return false;
    // The B-spline through the points runs from the middle of the first edge to the middle of the last one.
    const double shrink=std::cos(step/2);
    e.startAngle=step/2*180/pi;e.spanAngle=(n-2)*step*180/pi;
    setEllipseMap(e,QTransform(u.x()*shrink,u.y()*shrink,-v.x()*shrink,-v.y()*shrink,centre.x(),centre.y()),true);
    return true;
}

class Reader {
public:
    Reader(QByteArray bytes,QStringList *notes):r(std::move(bytes)),notes(notes){}
    Document read(bool library);
private:
    DelphiReader r;QStringList *notes;Document doc;double version=newestVersion;int objects=0,strokeFonts=0,approximated=0;
    bool at(double v) const{return version>=v-1e-9;}
    Panel panel();
    Element object(const QString &cls,int depth);
    void base(Element &e,QJsonObject &fd);
    // The points of an object. Their stored values go into `stored` when the model's millimetres do not give them
    // back exactly (or always with `always`), for the writer and the HPGL outlines of unchanged objects.
    QPolygonF points(QJsonObject *stored=nullptr,bool always=false);
    QPointF place(QPointF p){if(!std::isfinite(p.x())||!std::isfinite(p.y())||std::abs(p.x())>1e5||std::abs(p.y())>1e5)r.fail(ui("Ungültige Koordinate"));return p;}
    void shape(Element &e,const QJsonObject &fd,const QPolygonF &p,bool closed,const QString &cls);
    void group(Element &e,QJsonObject &fd,int depth);
};

void Reader::base(Element &e,QJsonObject &fd){
    fd["kind"]=r.integer();
    e.pen.width=std::max(0,r.integer())/unit;
    e.pen.color=colourWithFlags(r.raw(4),fd,"pen");
    const int penStyle=quint8(r.raw(1)[0]);e.pen.style=penStyle<=4?PenStyle(penStyle):PenStyle::Solid;if(penStyle>4)fd["penStyle"]=penStyle;
    e.fill.color=colourWithFlags(r.raw(4),fd,"brush");
    int brushStyle=quint8(r.raw(1)[0]);
    const bool filled=r.boolean();if(!at(3.04)&&!filled)brushStyle=1;   // older files kept "filled" apart from the style
    e.fill.style=brushStyle<=7?FillStyle(brushStyle):FillStyle::None;if(brushStyle>7)fd["brushStyle"]=brushStyle;
    fd["member"]=r.boolean();
    bool mill=false,smooth=false,engrave=false;int cornerStyle=1;QString symbol;
    if(at(2.0)){
        mill=r.boolean();smooth=r.boolean();cornerStyle=r.integer();const QByteArray size=r.extendedRaw();e.contour.size=std::max(0.0,fromExtended(size.constData())/unit);fd["cornerSize"]=hex(size);
        const QByteArray name=r.raw(41);fd["shortName"]=b64(name);symbol=shortText(name,40);
        // The insertion point of a symbol; unset points are zero or -1.
        const QByteArray ax=r.extendedRaw(),ay=r.extendedRaw();fd["anchor"]=QJsonArray{hex(ax),hex(ay)};
        const double x=fromExtended(ax.constData()),y=fromExtended(ay.constData());
        if(std::isfinite(x)&&std::isfinite(y)&&!(x==0&&y==0)&&!(x==-1&&y==-1)){e.hasAnchor=true;e.anchor=place(QPointF(x/unit,y/unit));}
    }
    if(at(3.0))e.name=r.string();
    if(at(3.02)){e.fill.color2=colourWithFlags(r.raw(4),fd,"color2");const int g=r.integer();e.fill.gradient=g>=0&&g<=4?Gradient(g):Gradient::None;if(g<0||g>4)fd["gradient"]=g;}
    else e.fill.color2=Qt::white;
    if(at(3.03))engrave=r.boolean();
    if(at(3.15))symbol=r.string();
    fd["symbolName"]=symbol;fd["cornerStyle"]=cornerStyle;
    e.contour.corners=cornersOf(smooth,cornerStyle);
    e.machining=mill?Machining::Mill:engrave?Machining::Engrave:Machining::None;if(mill&&engrave)fd["engraveToo"]=true;
}

QPolygonF Reader::points(QJsonObject *stored,bool always){
    const int last=r.integer();if(last<-1||last>=maxPoints)r.fail(ui("Ungültige Punktanzahl"));
    QPolygonF p;p.reserve(last+1);QByteArray values;bool exact=true;
    for(int i=0;i<=last;i++){
        const QByteArray x=r.extendedRaw(),y=r.extendedRaw();const QPointF q=place(QPointF(fromExtended(x.constData())/unit,fromExtended(y.constData())/unit));
        p<<q;values+=x+y;exact=exact&&toExtended(q.x()*unit)==x&&toExtended(q.y()*unit)==y;
    }
    if(stored&&(always||!exact))(*stored)["points"]=b64(values);
    return p;
}

void Reader::shape(Element &e,const QJsonObject &fd,const QPolygonF &p,bool closed,const QString &cls){
    e.points=p;
    if(closed&&p.size()>=3)e.type=cls=="TRechteck"&&fd["kind"].toInt()==4&&p.size()==4?ElementType::Rectangle:ElementType::Polygon;
    else{e.type=ElementType::Line;if(e.points.isEmpty())e.points<<QPointF();while(e.points.size()<2)e.points<<e.points.last();}
}

void Reader::group(Element &e,QJsonObject &fd,int depth){
    base(e,fd);
    const int last=r.integer();if(last<-1||last>=maxObjects)r.fail(ui("Ungültige Objektanzahl"));
    fd["groupName"]=b64(r.raw(41));
    for(int i=0;i<=last;i++)e.children.append(object(r.string(),depth+1));
}

Element Reader::object(const QString &cls,int depth){
    if(depth>maxDepth)r.fail(ui("Gruppen sind zu tief verschachtelt"));
    if(++objects>maxObjects)r.fail(ui("Zu viele Objekte"));
    Element e;e.id=newId();QJsonObject fd{{"class",cls}};const qsizetype start=r.position();bool keepRaw=true;
    if(cls=="TLinie"){base(e,fd);shape(e,fd,points(&fd),false,cls);}
    else if(cls=="TRechteck"||cls=="TPolygon"||cls=="TSkaleBogen"){
        base(e,fd);const QPolygonF p=points(&fd);const bool closed=r.boolean();fd["closed"]=closed;
        if(cls=="TSkaleBogen")fd["scaleFlag"]=int(quint8(r.raw(1)[0]));
        shape(e,fd,p,closed,cls);
    }
    else if(cls=="TKreis"){
        base(e,fd);const QByteArray centre=r.raw(20),radius=r.raw(20);fd["centre"]=hex(centre);fd["radius"]=hex(radius);
        if(at(2.0)){
            Element inner;QJsonObject innerFd;base(inner,innerFd);const QPolygonF p=points(&innerFd,true);innerFd["closed"]=r.boolean();fd["inner"]=innerFd;
            e.type=ElementType::Ellipse;
            if(!fitEllipse(p,e)){e.type=ElementType::Polygon;e.points=p;e.contour=inner.contour;approximated++;if(p.size()<3)shape(e,fd,p,false,cls);}
        }else{
            // Old files keep only centre and a point on the circle.
            e.type=ElementType::Ellipse;e.center=place(pointOf(centre));e.radiusX=e.radiusY=QLineF(e.center,place(pointOf(radius))).length();
        }
    }
    else if(cls=="TBogen"){
        QByteArray params;int mode=0;QPolygonF p;
        if(!at(2.0)){
            // The oldest layout: only the first stroke values, the arc parameters and whether it is a pie.
            fd["kind"]=r.integer();e.pen.width=std::max(0,r.integer())/unit;e.pen.color=colourWithFlags(r.raw(4),fd,"pen");const int ps=quint8(r.raw(1)[0]);e.pen.style=ps<=4?PenStyle(ps):PenStyle::Solid;
            e.fill.color=colourWithFlags(r.raw(4),fd,"brush");const int bs=quint8(r.raw(1)[0]);e.fill.style=bs<=7?FillStyle(bs):FillStyle::None;r.boolean();fd["member"]=r.boolean();
            params=r.raw(80);mode=r.boolean()?1:0;
        }else{base(e,fd);p=points(&fd,true);params=r.raw(80);mode=r.integer();}
        fd["arc"]=hex(params);fd["mode"]=mode;
        e.arcStyle=mode==1?ArcStyle::Pie:mode==2?ArcStyle::Chord:ArcStyle::Open;e.type=ElementType::Arc;
        if(p.isEmpty()){
            // Build it from the parameters: centre, a point giving the radius, start and end ray, clockwise on screen.
            const QPointF c=place(pointOf(params.left(20)));const double rad=QLineF(c,place(pointOf(params.mid(20,20)))).length();
            const QPointF s=pointOf(params.mid(40,20))-c,f=pointOf(params.mid(60,20))-c;
            const double a=std::atan2(-s.y(),s.x())*180/pi,b=std::atan2(-f.y(),f.x())*180/pi;double sweep=std::fmod(a-b+720,360);if(sweep<=1e-9)sweep=360;
            e.center=c;e.radiusX=e.radiusY=rad;e.rotation=0;e.startAngle=b;e.spanAngle=sweep;
        }else if(!fitArc(p,e)){e.points=p;e.type=e.arcStyle==ArcStyle::Open||p.size()<3?ElementType::Line:ElementType::Polygon;if(e.points.size()<2)shape(e,fd,p,false,cls);approximated++;}
    }
    else if(cls=="TTextLabel"){
        base(e,fd);e.type=ElementType::Text;const QByteArray corners=r.raw(80);fd["corners"]=hex(corners);
        const QPointF p0=place(pointOf(corners.left(20))),p1=place(pointOf(corners.mid(20,20))),p3=place(pointOf(corners.mid(60,20)));e.frame={p0,p1,p3};
        e.font=r.string();fd["fontHeight"]=r.integer();fd["unused"]=hex(r.extendedRaw());const QString old=r.string();
        fd["flagA"]=r.boolean();fd["flagB"]=r.boolean();
        int style=0;if(at(1.01))style=quint8(r.raw(1)[0]);fd["style"]=style;e.bold=style&1;e.italic=style&2;e.underline=style&4;e.strikeOut=style&8;
        bool shx=false;QString shxName;if(at(3.04)){shx=r.boolean();shxName=r.string();}
        fd["shxName"]=shxName;if(shx){e.strokeFont=shxName.isEmpty()?QStringLiteral("SHX"):shxName;if(!findStrokeFont(e.strokeFont))strokeFonts++;}
        e.text=at(3.15)?r.string():old;
    }
    else if(cls=="TBohrung"){
        base(e,fd);e.type=ElementType::Drill;const QByteArray x=r.extendedRaw(),y=r.extendedRaw(),d=r.extendedRaw();
        e.center=place(QPointF(fromExtended(x.constData())/unit,fromExtended(y.constData())/unit));e.diameter=std::clamp(fromExtended(d.constData())/unit,0.0,1000.0);
        if(toExtended(e.center.x()*unit)!=x||toExtended(e.center.y()*unit)!=y||toExtended(e.diameter*unit)!=d)fd["drill"]=b64(x+y+d);
    }
    else if(cls=="TBtmap"||cls=="TMeta"){
        base(e,fd);const QPolygonF p=points();fd["closed"]=r.boolean();if(p.size()!=4)r.fail(ui("Ungültiger Bildrahmen"));
        e.frame={p[0],p[1],p[3]};keepRaw=false;fd["rawFrame"]=b64(r.since(start));
        const QByteArray head=r.peek(64);
        if(cls=="TBtmap"){
            if(head.size()<14||!head.startsWith("BM"))r.fail(ui("Ungültiges Bild"));
            const quint32 size=qFromLittleEndian<quint32>(head.constData()+2);if(size<26||size>quint32(r.size()-r.position()))r.fail(ui("Ungültiges Bild"));
            e.type=ElementType::Image;e.resource=doc.addResource(r.raw(size),"bmp");
            const qsizetype tail=r.position();e.transparentColor=colourWithFlags(r.raw(4),fd,"transparent");e.transparent=r.boolean();fd["rawTail"]=b64(r.since(tail));
        }else{
            if(head.size()<52||qFromLittleEndian<quint32>(head.constData())!=1||head.mid(40,4)!=" EMF")r.fail(ui("Ungültige Vektorgrafik"));
            const quint32 size=qFromLittleEndian<quint32>(head.constData()+48);if(size<88||size>quint32(r.size()-r.position()))r.fail(ui("Ungültige Vektorgrafik"));
            e.type=ElementType::Picture;e.resource=doc.addResource(r.raw(size),"emf");
        }
    }
    else if(cls=="TGruppe"||cls=="TKombination"||cls=="TSchalttafel"){
        group(e,fd,depth);e.type=cls=="TSchalttafel"?ElementType::Cutout:ElementType::Group;keepRaw=false;if(cls=="TKombination")e.parameters["combine"]=true;
    }
    else if(cls=="TBemassung"){
        group(e,fd,depth);e.type=ElementType::Dimension;keepRaw=false;
        // Where the dimension line lies, and the two measured points.
        const QByteArray anchors=r.raw(60);fd["anchors"]=hex(anchors);QJsonArray list;
        for(int i=0;i<3;i++){const QPointF a=place(pointOf(anchors.mid(i*20,20)));list.append(QJsonArray{a.x(),a.y()});}
        e.parameters["anchors"]=list;
        if(at(3.04)){fd["flagA"]=r.boolean();fd["flagB"]=r.boolean();}
        // Measure again when changed: the style comes from the parts as they are.
        if(e.children.size()==5&&e.children[2].type==ElementType::Text&&!e.children[0].children.isEmpty()){
            const Element &value=e.children[2],&stroke=e.children[0].children[0];const QString text=value.text;const int comma=std::max(text.lastIndexOf(','),text.lastIndexOf('.'));
            const QPolygonF c=frameCorners(value.frame);
            e.parameters["generator"]="dimension";
            e.parameters["style"]=QJsonObject{{"color",stroke.pen.color.name()},{"lineWidth",stroke.pen.width},{"textHeight",c.size()==4?QLineF(c[0],c[3]).length():3.0},
                {"decimals",comma<0?0:int(text.size()-comma-1)},{"font",value.font}};
        }
    }
    else if(cls=="TScale"){
        group(e,fd,depth);e.type=ElementType::Scale;keepRaw=false;const qsizetype from=r.position();
        r.raw(1);for(int i=0;i<3;i++)r.real();for(int i=0;i<5;i++)r.boolean();r.integer();r.real();r.integer();r.real();r.raw(20);r.real();
        fd["scaleParameters"]=b64(r.since(from));
    }
    else r.fail(ui("Unbekannte Objektklasse: %1").arg(cls));
    // Original bytes are only reusable in a file of the same layout, which is the one written here.
    if(keepRaw&&at(newestVersion))fd["raw"]=b64(r.since(start));
    if(!at(newestVersion)){fd.remove("rawFrame");fd.remove("rawTail");}
    fd["fingerprint"]=fingerprint(e);e.foreign[key]=fd;
    return e;
}

Panel Reader::panel(){
    Panel p;p.id=newId();QJsonObject fd;
    const QByteArray header=r.raw(41);
    QString versionText=QString::fromLatin1(header.mid(37,4));versionText.remove(QChar(0));versionText=versionText.trimmed();
    if(versionText.isEmpty())versionText="1,00";
    bool ok=false;version=QString(versionText).replace(',','.').toDouble(&ok);
    if(!ok||version<0.5)r.fail(ui("Unbekannte Dateiversion: %1").arg(versionText));
    if(version>newestVersion+0.06)r.fail(ui("Die Datei stammt aus einer neueren FrontDesigner-Version (%1)").arg(versionText));
    p.name=shortText(header,36);fd["header"]=b64(header);fd["version"]=versionText;
    const int count=r.integer();if(count<0||count>maxObjects)r.fail(ui("Ungültige Objektanzahl"));
    for(int i=0;i<count;i++)p.elements.append(object(r.string(),0));
    fd["value1"]=r.integer();
    auto real=[&](const QString &name){const QByteArray raw=r.extendedRaw();fd[name]=hex(raw);return fromExtended(raw.constData())/unit;};
    p.width=real("width");p.height=real("height");
    if(at(2.02))p.grid=real("grid");else p.grid=r.integer()/unit;
    if(!(p.width>=1&&p.width<=10000&&p.height>=1&&p.height<=10000))r.fail(ui("Ungültige Plattengröße"));
    if(!(p.grid>=0.001&&p.grid<=1000))p.grid=1;
    fd["value2"]=r.integer();
    const QByteArray origin=r.raw(20);fd["origin"]=hex(origin);p.origin=place(pointOf(origin));
    p.gridVisible=r.boolean();
    p.color=colourWithFlags(r.raw(4),fd,"colour");
    // From 3,01: the panel's unit (0 mm, 1 inch) and the units chosen in the fields for grid (three) and origin (two).
    if(at(3.01)){p.gridColor=colourWithFlags(r.raw(4),fd,"gridColour");fd["value3"]=r.integer();p.inch=fd["value3"].toInt()!=0;QJsonArray units;for(int i=0;i<5;i++)units.append(r.integer());fd["units"]=units;}
    if(at(3.02)){p.color2=colourWithFlags(r.raw(4),fd,"colour2");const int g=r.integer();p.gradient=g>=0&&g<=4?Gradient(g):Gradient::None;}
    if(at(3.05)){
        QJsonArray print;for(int i=0;i<3;i++)print.append(r.integer());fd["print"]=print;fd["printArea"]=hex(r.raw(20));
        QJsonArray flags;for(int i=0;i<13;i++)flags.append(r.boolean());fd["printFlags"]=flags;fd["printScale"]=hex(r.extendedRaw());
        const int x=r.integer(),y=r.integer();fd["printOffset"]=QJsonArray{x,y};
    }
    if(at(3.15)){const QString name=r.string();if(!name.isEmpty())p.name=name;}
    if(at(3.16))fd["value4"]=r.boolean();
    p.print=printSettingsOf(fd);
    fd["name"]=p.name;
    p.foreign[key]=fd;
    return p;
}

Document Reader::read(bool library){
    if(r.size()>maxFile)r.fail(ui("Datei ist zu groß"));
    doc.panels.clear();doc.resources.clear();
    doc.panels.append(panel());
    QJsonObject fd{{"source",library?"lib":"fpl"},{"version",doc.panels[0].foreign[key].toObject()["version"]}};
    if(!library){
        const int count=r.integer();if(count<0||count>maxViews)r.fail(ui("Ungültige Ansichtenliste"));
        QJsonArray views;
        for(int i=0;i<count;i++){
            const QByteArray area=r.raw(40);const int panel=at(2.0)?qFromLittleEndian<qint32>(r.raw(4).constData()):0;
            views.append(QJsonObject{{"area",hex(area)},{"panel",panel},{"name",r.string()}});
        }
        fd["views"]=views;
        if(at(2.0)&&!r.atEnd()){
            const int panels=r.integer();if(panels<1||panels>maxPanels)r.fail(ui("Ungültige Anzahl von Frontplatten"));
            for(int i=1;i<panels;i++)doc.panels.append(panel());
        }
        // A view is the shown area of a panel: left, top, right and bottom; views of missing panels are left out.
        for(const auto &v:views){
            const auto o=v.toObject();const QByteArray area=unhex(o["area"]);const int index=o["panel"].toInt();if(index<0||index>=doc.panels.size())continue;
            QRectF rect;if(viewArea(area,&rect))doc.views.append(View{o["name"].toString(),index,rect});
        }
    }
    doc.foreign[key]=fd;doc.activePanel=0;
    if(notes){
        if(!r.atEnd())notes->append(ui("Nach dem Ende der Daten folgen %1 weitere Bytes; sie wurden nicht gelesen.").arg(r.size()-r.position()));
        if(strokeFonts)notes->append(ui("%1 Texte verwenden eine SHX-Strichschrift; sie werden mit einer Ersatzschrift gezeigt.").arg(strokeFonts));
        if(approximated)notes->append(ui("%1 Kreise oder Bögen bilden keine Ellipse mehr und wurden als Kontur übernommen.").arg(approximated));
    }
    return std::move(doc);
}

class Writer {
public:
    Writer(const Document &d,const WriteOptions &o):doc(d),options(o){}
    QByteArray project();
    QByteArray library(int panel){writePanel(doc.panels.value(panel));return w.bytes;}
private:
    const Document &doc;WriteOptions options;DelphiWriter w;
    void writePanel(const Panel &p);
    void object(const Element &e,int depth);
    void base(const Element &e,const QJsonObject &fd,int depth,int kind);
    // Points; `stored` are the values read for them (an unchanged object), written instead when they fit.
    void points(const QPolygonF &p,const QByteArray &stored={}){
        if(stored.size()==p.size()*20){storedValues(stored);return;}
        w.integer(p.size()-1);for(auto q:p){w.real(q.x()*unit);w.real(q.y()*unit);}
    }
    void storedValues(const QByteArray &stored){w.integer(stored.size()/20-1);for(qsizetype i=0;i<stored.size();i+=10)w.extendedRaw(stored.mid(i,10));}
    // The stored extended value when it still gives the same number of millimetres.
    void real(double mm,const QJsonValue &original){
        const QByteArray raw=unhex(original);if(raw.size()==10&&fromExtended(raw.constData())/unit==mm)w.extendedRaw(raw);else w.real(mm*unit);
    }
    QString classFor(const Element &e,const QJsonObject &fd) const;
    QByteArray bitmap(const Element &e) const;
    void groupPart(const Element &e,const QJsonObject &fd,int depth,int kind);
};

QString Writer::classFor(const Element &e,const QJsonObject &fd) const{
    const QString c=fd["class"].toString();
    switch(e.type){
    case ElementType::Line:case ElementType::Polygon:case ElementType::Rectangle:case ElementType::Ellipse:case ElementType::Arc:return contourClass(e,c);
    case ElementType::Text:return "TTextLabel";
    case ElementType::Drill:return "TBohrung";
    case ElementType::Image:return "TBtmap";
    case ElementType::Picture:return doc.resources.value(e.resource).kind=="emf"?"TMeta":"TBtmap";
    case ElementType::Group:return e.combined()?"TKombination":"TGruppe";
    // The original expects exactly its five parts (two arrows, value, two extension lines) and the three anchors.
    case ElementType::Dimension:return e.children.size()==5&&e.parameters["anchors"].toArray().size()==3?"TBemassung":"TGruppe";
    case ElementType::Scale:return c=="TScale"&&fd.contains("scaleParameters")?c:"TGruppe";
    case ElementType::Cutout:return "TSchalttafel";
    }
    return "TGruppe";
}

void Writer::base(const Element &e,const QJsonObject &fd,int depth,int kind){
    w.integer(fd.contains("kind")?fd["kind"].toInt():kind);
    w.integer(qRound64(e.pen.width*unit));
    w.raw(colourBytes(e.pen.color,fd["penFlags"].toInt()));
    w.raw(QByteArray(1,char(e.pen.style==PenStyle::Solid&&fd.contains("penStyle")?fd["penStyle"].toInt():int(e.pen.style))));
    w.raw(colourBytes(e.fill.color,fd["brushFlags"].toInt()));
    w.raw(QByteArray(1,char(e.fill.style==FillStyle::None&&fd.contains("brushStyle")?fd["brushStyle"].toInt():int(e.fill.style))));
    w.boolean(false);
    w.boolean(fd.contains("member")?fd["member"].toBool():depth>0);
    w.boolean(e.machining==Machining::Mill);
    w.boolean(e.contour.corners!=Corners::Sharp);
    w.integer(cornerStyleOf(e.contour.corners,fd.contains("cornerStyle")?fd["cornerStyle"].toInt():1));
    real(e.contour.size,fd["cornerSize"]);
    const QString symbol=fd["symbolName"].toString();
    // The short form of the name stays as read; newer files carry the full name at the end.
    const QByteArray stored=unb64(fd["shortName"]);w.raw(stored.size()==41?stored:shortString(symbol,40));
    const auto anchor=fd["anchor"].toArray();
    if(e.hasAnchor){real(e.anchor.x(),anchor.at(0));real(e.anchor.y(),anchor.at(1));}
    else if(anchor.size()==2){w.extendedRaw(unhex(anchor[0]));w.extendedRaw(unhex(anchor[1]));}else{w.real(0);w.real(0);}
    w.string(e.name);
    w.raw(colourBytes(e.fill.color2,fd["color2Flags"].toInt()));
    w.integer(e.fill.gradient==Gradient::None&&fd.contains("gradient")?fd["gradient"].toInt():int(e.fill.gradient));
    w.boolean(e.machining==Machining::Engrave||(e.machining==Machining::Mill&&fd["engraveToo"].toBool()));
    w.string(symbol);
}

void Writer::groupPart(const Element &e,const QJsonObject &fd,int depth,int kind){
    base(e,fd,depth,kind);w.integer(e.children.size()-1);
    const QByteArray name=unb64(fd["groupName"]);w.raw(name.size()==41?name:shortString(ui("Gruppe"),40));
    for(const auto &c:e.children)object(c,depth+1);
}

QByteArray Writer::bitmap(const Element &e) const{
    const Resource res=doc.resources.value(e.resource);if(res.kind=="bmp")return res.data;
    QImage image=QImage::fromData(res.data);if(image.isNull()){image=QImage(1,1,QImage::Format_RGB32);image.fill(Qt::white);}
    QByteArray bmp;QBuffer buffer(&bmp);buffer.open(QIODevice::WriteOnly);image.convertToFormat(QImage::Format_RGB32).save(&buffer,"BMP");return bmp;
}

void Writer::object(const Element &e,int depth){
    const QJsonObject fd=e.foreign[key].toObject();const QString cls=classFor(e,fd);
    w.string(cls);
    const bool unchanged=options.keepUnchanged&&fd["class"]==cls&&fd["fingerprint"]==fingerprint(e);
    if(unchanged&&fd.contains("raw")){w.raw(unb64(fd["raw"]));return;}
    if(unchanged&&fd.contains("rawFrame")){
        // A picture keeps the bytes of its frame and transparency; the picture itself is the stored resource.
        w.raw(unb64(fd["rawFrame"]));w.raw(cls=="TBtmap"?bitmap(e):doc.resources.value(e.resource).data);if(cls=="TBtmap")w.raw(unb64(fd["rawTail"]));return;
    }
    if(cls=="TLinie"||cls=="TRechteck"||cls=="TPolygon"||cls=="TSkaleBogen"){
        base(e,fd,depth,e.type==ElementType::Rectangle?4:0);points(e.points,unchanged?storedPoints(fd,e.points.size()):QByteArray());
        if(cls!="TLinie")w.boolean(e.type!=ElementType::Line);
        if(cls=="TSkaleBogen")w.raw(QByteArray(1,char(fd["scaleFlag"].toInt())));
    }
    else if(cls=="TKreis"){
        base(e,fd,depth,4);QPolygonF inner;const QJsonObject innerRead=fd["inner"].toObject();
        if(e.type==ElementType::Ellipse){
            inner=ellipseSupport(e);
            const QByteArray c=unhex(fd["centre"]),r=unhex(fd["radius"]);
            if(unchanged&&c.size()==20&&r.size()==20){w.raw(c);w.raw(r);}else{w.raw(pointBytes(e.center));w.raw(pointBytes(ellipseMap(e).map(QPointF(1,0))));}
        }else{inner=e.points;const QByteArray c=unhex(fd["centre"]),r=unhex(fd["radius"]);w.raw(c.size()==20?c:QByteArray(20,0));w.raw(r.size()==20?r:QByteArray(20,0));}
        // The inner closed contour carries the same stroke and fill, smoothed as B-spline.
        Element shape=e;shape.contour={Corners::Spline,e.type==ElementType::Ellipse?0.0:e.contour.size};if(e.type==ElementType::Polygon)shape.contour=e.contour;
        QJsonObject innerFd=innerRead;innerFd.remove("kind");if(e.type==ElementType::Ellipse&&!unchanged)innerFd["cornerStyle"]=0;
        base(shape,innerFd,depth,0);
        if(unchanged&&e.type==ElementType::Ellipse&&!storedSupport(innerRead).isEmpty())storedValues(storedSupport(innerRead));
        else points(inner,unchanged?storedPoints(innerRead,inner.size()):QByteArray());
        w.boolean(true);
    }
    else if(cls=="TBogen"){
        // An arc is stored as its points with B-spline corners, plus centre, radius point, start and end ray.
        QPolygonF p;QByteArray params;Element shape=e;QJsonObject arcFd=fd;
        if(e.type==ElementType::Arc){
            p=arcSupport(e,&params);shape.contour={Corners::ArcSpline,0};
            if(unchanged&&unhex(fd["arc"]).size()==80)params=unhex(fd["arc"]);else arcFd["cornerStyle"]=3;
        }else{p=e.points;params=unhex(fd["arc"]);if(params.size()!=80)params=QByteArray(80,0);}
        base(shape,arcFd,depth,9);
        if(unchanged&&e.type==ElementType::Arc&&!storedSupport(fd).isEmpty())storedValues(storedSupport(fd));
        else points(p,unchanged?storedPoints(fd,p.size()):QByteArray());
        w.raw(params);
        w.integer(e.type==ElementType::Arc?int(e.arcStyle):fd["mode"].toInt());
    }
    else if(cls=="TTextLabel"){
        base(e,fd,depth,7);w.raw(textCorners(e,fd,unchanged));
        w.string(e.font);w.integer(textHeight(e,fd));
        real(0,fd["unused"]);w.string(QString());
        const auto [first,second]=textFlags(e,fd);w.boolean(first);w.boolean(second);
        const int style=(fd["style"].toInt()&~15)|(e.bold?1:0)|(e.italic?2:0)|(e.underline?4:0)|(e.strikeOut?8:0);w.raw(QByteArray(1,char(style)));
        const bool shx=!e.strokeFont.isEmpty();w.boolean(shx);
        w.string(shx?(e.strokeFont=="SHX"?fd["shxName"].toString():e.strokeFont):(fd.contains("shxName")?fd["shxName"].toString():QStringLiteral("DIN1451")));
        w.string(e.text);
    }
    else if(cls=="TBohrung"){
        base(e,fd,depth,8);const QByteArray stored=unb64(fd["drill"]);
        if(unchanged&&stored.size()==30){w.extendedRaw(stored.left(10));w.extendedRaw(stored.mid(10,10));w.extendedRaw(stored.mid(20,10));}
        else{w.real(e.center.x()*unit);w.real(e.center.y()*unit);w.real(e.diameter*unit);}
    }
    else if(cls=="TBtmap"||cls=="TMeta"){
        base(e,fd,depth,cls=="TBtmap"?10:11);points(frameCorners(e.frame));w.boolean(true);
        if(cls=="TBtmap"){w.raw(bitmap(e));w.raw(colourBytes(e.transparentColor,fd["transparentFlags"].toInt()));w.boolean(e.transparent);}
        else w.raw(doc.resources.value(e.resource).data);
    }
    else if(cls=="TBemassung"){
        groupPart(e,fd,depth,13);const auto anchors=e.parameters["anchors"].toArray();const QByteArray stored=unhex(fd["anchors"]);
        QByteArray out;for(int i=0;i<3;i++){const auto a=anchors.at(i).toArray();const QPointF p(a.at(0).toDouble(),a.at(1).toDouble());
            const QByteArray old=stored.mid(i*20,20);out+=old.size()==20&&pointOf(old)==p?old:pointBytes(p);}
        w.raw(out);w.boolean(fd["flagA"].toBool());w.boolean(fd["flagB"].toBool());
    }
    else if(cls=="TScale"){groupPart(e,fd,depth,6);w.raw(unb64(fd["scaleParameters"]));}
    else groupPart(e,fd,depth,cls=="TKombination"?12:6);
}

void Writer::writePanel(const Panel &p){
    const QJsonObject fd=p.foreign[key].toObject();
    QByteArray header=unb64(fd["header"]);
    // The header keeps the bytes it was read with as long as the name is unchanged.
    if(header.size()!=41||fd["name"].toString()!=p.name){header=QByteArray(41,0);const QByteArray t=toWindows1252(p.name).left(36);header[0]=char(t.size());header.replace(1,t.size(),t);}
    header.replace(37,4,"3,16");w.raw(header);
    w.integer(p.elements.size());for(const auto &e:p.elements)object(e,0);
    w.integer(fd["value1"].toInt());
    real(p.width,fd["width"]);real(p.height,fd["height"]);real(p.grid,fd["grid"]);
    w.integer(fd.contains("value2")?fd["value2"].toInt():1);
    const QByteArray origin=unhex(fd["origin"]);w.raw(origin.size()==20&&pointOf(origin)==p.origin?origin:pointBytes(p.origin));
    w.boolean(p.gridVisible);w.raw(colourBytes(p.color,fd["colourFlags"].toInt()));w.raw(colourBytes(p.gridColor,fd["gridColourFlags"].toInt()));
    // Switching the unit also switches the origin fields, as in the original.
    const bool asRead=(fd["value3"].toInt()!=0)==p.inch;w.integer(asRead?fd["value3"].toInt():int(p.inch));
    const auto units=fd["units"].toArray();for(int i=0;i<5;i++)w.integer(asRead||i<3?units.at(i).toInt():int(p.inch));
    w.raw(colourBytes(p.color2,fd["colour2Flags"].toInt()));w.integer(int(p.gradient));
    // The print settings: the bytes as read while they still say the same, otherwise those of the panel.
    const PrintSettings &s=p.print;
    if(printBlockKept(fd)&&printSettingsOf(fd)==s){
        for(const auto &v:fd["print"].toArray())w.integer(v.toInt());
        w.raw(unhex(fd["printArea"]));for(const auto &v:fd["printFlags"].toArray())w.boolean(v.toBool());w.extendedRaw(unhex(fd["printScale"]));
        for(const auto &v:fd["printOffset"].toArray())w.integer(v.toInt());
    }else{
        w.integer(s.tilesX);w.integer(s.tilesY);w.integer(s.sheet);w.raw(pointBytes({s.gapX,s.gapY}));
        for(bool f:{s.machining,s.objects,s.data,s.rulers,s.frame,s.background,s.mirror,s.landscape,s.original,s.centred,s.dimensions,s.cutMarks,s.onlyOne})w.boolean(f);
        w.real(s.original?1:s.zoom);w.integer(int(std::lround(-s.left*unit)));w.integer(int(std::lround(-s.top*unit)));
    }
    w.string(p.name);
    w.boolean(s.texts);
}

QByteArray Writer::project(){
    writePanel(doc.panels.value(0));
    // The views of the document; one that is still as read keeps its original bytes.
    QList<QByteArray> read;for(const auto &v:doc.foreign[key].toObject()["views"].toArray())read.append(unhex(v.toObject()["area"]));
    QList<View> views;for(const auto &v:doc.views)if(v.panel>=0&&v.panel<doc.panels.size())views.append(v);
    w.integer(views.size());
    for(const View &v:views){
        QByteArray area=viewBytes(v.area);
        for(int k=0;k<read.size();k++){QRectF old;if(viewArea(read[k],&old)&&old==v.area){area=read.takeAt(k);break;}}
        w.raw(area);char index[4];qToLittleEndian(qint32(v.panel),index);w.raw(QByteArray(index,4));w.string(v.name);
    }
    w.integer(doc.panels.size());
    for(int i=1;i<doc.panels.size();i++)writePanel(doc.panels[i]);
    return w.bytes;
}
}

Document readFrontDesigner(const QByteArray &bytes,bool library,QStringList *notes){
    if(bytes.size()>maxFile)throw FormatError(ui("Datei ist zu groß"));
    Reader reader(bytes,notes);Document d=reader.read(library);
    d.title=d.panels[0].name;return d;
}
Document loadFrontDesigner(const QString &path,QStringList *notes){
    QFile file(path);if(!file.open(QIODevice::ReadOnly))throw FormatError(file.errorString());
    if(file.size()>maxFile)throw FormatError(ui("Datei ist zu groß"));
    Document d=readFrontDesigner(file.readAll(),QFileInfo(path).suffix().compare("lib",Qt::CaseInsensitive)==0,notes);
    d.title=QFileInfo(path).completeBaseName();return d;
}
QByteArray writeFrontDesigner(const Document &document,const WriteOptions &options){return Writer(document,options).project();}
QByteArray writeFrontDesignerLibrary(const Document &document,int panel,const WriteOptions &options){return Writer(document,options).library(panel);}
bool isFrontDesignerFile(const QString &path){const QString s=QFileInfo(path).suffix().toLower();return s=="fpl"||s=="lib";}

// The outlines of the HPGL export: what the original plots for an element, from the values this writer stores for it
// (the stored ones while it is unchanged since it was read).
namespace {
QList<ExtendedPoint> extendedPoints(const QPolygonF &mm,const QByteArray &stored){
    QList<ExtendedPoint> out;
    if(!stored.isEmpty()){for(qsizetype i=0;i+20<=stored.size();i+=20)out<<ExtendedPoint{Extended::fromBytes(stored.constData()+i),Extended::fromBytes(stored.constData()+i+10)};return out;}
    for(auto q:mm)out<<ExtendedPoint{Extended::fromDouble(q.x()*unit),Extended::fromDouble(q.y()*unit)};
    return out;
}
// A value as Writer::real() stores it: the stored one while it still gives the same millimetres.
Extended extendedValue(double mm,const QJsonValue &stored){
    const QByteArray raw=unhex(stored);
    return raw.size()==10&&fromExtended(raw.constData())/unit==mm?Extended::fromBytes(raw.constData()):Extended::fromDouble(mm*unit);
}
bool unchangedSinceRead(const Element &e,const QJsonObject &fd,const QString &cls){return fd["class"]==cls&&fd["fingerprint"]==fingerprint(e);}
// The corner values as Writer::base() stores them.
void cornersInto(PlotShape &s,const Contour &c,const QJsonObject &fd){
    s.smooth=c.corners!=Corners::Sharp;s.cornerStyle=cornerStyleOf(c.corners,fd.contains("cornerStyle")?fd["cornerStyle"].toInt():1);
    s.cornerSize=extendedValue(c.size,fd["cornerSize"]);
}
// Centre, x, y and diameter of a drill in file units.
void drillValues(const Element &d,ExtendedPoint &centre,Extended &diameter){
    const QJsonObject fd=d.foreign[key].toObject();const QByteArray stored=unb64(fd["drill"]);
    if(unchangedSinceRead(d,fd,"TBohrung")&&stored.size()==30){
        centre={Extended::fromBytes(stored.constData()),Extended::fromBytes(stored.constData()+10)};diameter=Extended::fromBytes(stored.constData()+20);return;
    }
    centre={Extended::fromDouble(d.center.x()*unit),Extended::fromDouble(d.center.y()*unit)};diameter=Extended::fromDouble(d.diameter*unit);
}
}

QPolygon plotOutline(const Element &e){
    const QJsonObject fd=e.foreign[key].toObject();const QString cls=contourClass(e,fd["class"].toString());
    if(cls.isEmpty())return {};
    const bool unchanged=unchangedSinceRead(e,fd,cls);PlotShape s;
    if(cls=="TKreis"){
        // The circle plots its inner closed contour.
        const QJsonObject inner=fd["inner"].toObject();
        if(e.type==ElementType::Ellipse){s.points=extendedPoints(ellipseSupport(e),unchanged?storedSupport(inner):QByteArray());cornersInto(s,{Corners::Spline,0},inner);}
        else{s.points=extendedPoints(e.points,unchanged?storedPoints(inner,e.points.size()):QByteArray());cornersInto(s,e.contour,inner);}
        s.closed=true;
    }else if(cls=="TBogen"){
        // An arc: its open contour, closed through the centre (pie) or straight (chord) as its mode says.
        QPolygonF p;QByteArray params;
        if(e.type==ElementType::Arc){p=arcSupport(e,&params);if(unchanged&&unhex(fd["arc"]).size()==80)params=unhex(fd["arc"]);cornersInto(s,{Corners::ArcSpline,0},fd);s.arcMode=int(e.arcStyle);}
        else{p=e.points;params=unhex(fd["arc"]);if(params.size()!=80)params=QByteArray(80,0);cornersInto(s,e.contour,fd);s.arcMode=fd["mode"].toInt();}
        s.points=extendedPoints(p,!unchanged?QByteArray():e.type==ElementType::Arc?storedSupport(fd):storedPoints(fd,p.size()));
        s.arcCentre={Extended::fromBytes(params.constData()),Extended::fromBytes(params.constData()+10)};
    }else{
        s.points=extendedPoints(e.points,unchanged?storedPoints(fd,e.points.size()):QByteArray());cornersInto(s,e.contour,fd);
        s.closed=cls!="TLinie"&&e.type!=ElementType::Line;
    }
    return plotPolygon(s);
}

bool plotText(const Element &e,QList<QPolygon> &strokes){
    strokes.clear();
    if(e.type!=ElementType::Text||e.strokeFont.isEmpty())return false;
    // A font in the original's own format holds the strokes as it draws them; it makes them of a shape font itself.
    const StrokeFont *font=findStrokeFont(e.strokeFont);
    if(!font||!originalLetters(*font))return false;
    const QJsonObject fd=e.foreign[key].toObject();const QByteArray corners=textCorners(e,fd,unchangedSinceRead(e,fd,"TTextLabel"));
    StrokeText t;for(int i=0;i<4;i++)t.corners[i]={Extended::fromBytes(corners.constData()+20*i),Extended::fromBytes(corners.constData()+20*i+10)};
    t.height=textHeight(e,fd);std::tie(t.flipX,t.flipY)=textFlags(e,fd);t.text=toWindows1252(e.text);t.font=font;
    strokes=plotStrokeText(t);return true;
}

QPoint plotDrill(const Element &drill){
    ExtendedPoint c;Extended d;drillValues(drill,c,d);
    return QPoint(int(qint32(c.x.rounded())),int(qint32(c.y.rounded())));
}

QPolygon plotMilledDrill(const Element &drill,double tool,double diameter){
    // A temporary circle with the drill's centre and a radius point (d / 2 - tool / 2) below it, as the original makes it.
    ExtendedPoint c;Extended d;drillValues(drill,c,d);
    if(diameter>=0)d=Extended::fromDouble(std::nearbyint(diameter*unit));
    const Extended two=Extended::fromInt(2),fifty=Extended::fromInt(50);
    const Extended below=(d/two+c.y)-Extended::fromDouble(tool)*fifty/two;
    const Extended dy=c.y-below,tiny=Extended::fromBytes(QByteArray::fromHex("97a5b436415f7089e13f").constData());
    const Extended radius=dy.abs()<tiny?Extended():(Extended()+dy*dy).sqrt();
    PlotShape s;s.points=circlePoints(c,radius.abs());s.closed=true;s.smooth=true;s.cornerStyle=0;
    return plotPolygon(s);
}

QPolygon plotPanelOutline(const Panel &panel,double tool){
    // The tool is the rectangle's line width in whole file units.
    const QJsonObject fd=panel.foreign[key].toObject();
    const Extended w=Extended::fromDouble(std::nearbyint(tool*unit)),half=w/Extended::fromInt(2);
    const Extended width=extendedValue(panel.width,fd["width"]),height=extendedValue(panel.height,fd["height"]);
    const Extended left=Extended()-half,top=Extended()-half,right=half+width,bottom=half+height;
    PlotShape s;s.points={{left,bottom},{right,bottom},{right,top},{left,top}};s.closed=true;
    return plotPolygon(s);
}
}
