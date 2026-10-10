#include "hpgl.h"
#include "project.h"
#include "language.h"
#include "legacy_reader.h"
#include "legacy_writer.h"
#include <QJsonArray>
#include <QtEndian>
#include <array>
#include <bit>
#include <cmath>
#include <numbers>
#include <functional>
namespace openloch {
namespace {
// Exact fixed-point numbers that reproduce the original's x87 arithmetic: every operand is a dyadic rational, so
// sums and products are exact until rounded on purpose to the 64-bit significand (half to even) like the FPU does.
class Extended {
public:
    static constexpr int Limbs=8,Fraction=160;  // magnitude / 2^160 in 32-bit limbs, low limb first
    std::array<quint32,Limbs> m{};bool negative=false;
    static Extended integer(qint64 v){Extended e;const quint64 a=v<0?quint64(0)-quint64(v):quint64(v);e.m[5]=quint32(a);e.m[6]=quint32(a>>32);e.negative=v<0;return e;}
    static Extended fromDouble(double v){
        Extended e;if(v==0)return e;int exponent=0;const double f=std::frexp(std::abs(v),&exponent);const auto mantissa=quint64(std::ldexp(f,53));
        for(int i=0;i<64;i++)if(mantissa>>i&1){const int p=Fraction+exponent-53+i;if(p>=0&&p<Limbs*32)e.m[p/32]|=1u<<(p%32);}
        e.negative=v<0;return e;
    }
    bool zero() const{for(auto l:m)if(l)return false;return true;}
    int top() const{for(int i=Limbs-1;i>=0;i--)if(m[i])return i*32+31-std::countl_zero(m[i]);return -1;}
    bool bit(int p) const{return p>=0&&p<Limbs*32&&(m[p/32]>>(p%32)&1);}
    bool anyBelow(int p) const{for(int i=0;i<p&&i<Limbs*32;i++)if(bit(i))return true;return false;}
    static void add(std::array<quint32,Limbs> &a,const std::array<quint32,Limbs> &b){quint64 c=0;for(int i=0;i<Limbs;i++){c+=quint64(a[i])+b[i];a[i]=quint32(c);c>>=32;}}
    static void subtract(std::array<quint32,Limbs> &a,const std::array<quint32,Limbs> &b){quint64 borrow=0;for(int i=0;i<Limbs;i++){const quint64 d=quint64(a[i])-b[i]-borrow;a[i]=quint32(d);borrow=d>>63;}}
    static int compare(const Extended &a,const Extended &b){for(int i=Limbs-1;i>=0;i--)if(a.m[i]!=b.m[i])return a.m[i]<b.m[i]?-1:1;return 0;}
    Extended operator+(const Extended &o) const{
        if(negative==o.negative){Extended r=*this;add(r.m,o.m);return r;}
        const int c=compare(*this,o);if(c==0)return {};
        Extended r=c>0?*this:o;subtract(r.m,(c>0?o:*this).m);return r;
    }
    Extended operator*(qint64 v) const{  // |v| < 2^32
        Extended r;const quint64 a=v<0?quint64(0)-quint64(v):quint64(v);quint64 c=0;
        for(int i=0;i<Limbs;i++){c+=quint64(m[i])*a;r.m[i]=quint32(c);c>>=32;}
        r.negative=!r.zero()&&negative!=(v<0);return r;
    }
    Extended operator*(const Extended &o) const{
        std::array<quint32,2*Limbs> p{};
        for(int i=0;i<Limbs;i++){quint64 c=0;for(int j=0;j<Limbs;j++){const quint64 t=quint64(m[i])*o.m[j]+p[i+j]+c;p[i+j]=quint32(t);c=t>>32;}p[i+Limbs]=quint32(c);}
        Extended r;for(int i=0;i<Limbs;i++)r.m[i]=p[i+Fraction/32];r.negative=!r.zero()&&negative!=o.negative;return r;
    }
    // Rounded to a 64-bit significand, half to even.
    Extended rounded() const{
        const int t=top();if(t<64)return *this;const int cut=t-63;
        Extended r=*this;const bool half=bit(cut-1),up=half&&(anyBelow(cut-1)||bit(cut));
        for(int p=0;p<cut;p++)r.m[p/32]&=~(1u<<(p%32));
        if(up){Extended unit;unit.m[cut/32]=1u<<(cut%32);add(r.m,unit.m);}
        return r;
    }
    // Quotient rounded to 64 bits; a remainder sets the lowest bit so that it counts for the rounding.
    Extended divided(qint64 d) const{
        Extended r;const quint64 a=d<0?quint64(0)-quint64(d):quint64(d);quint64 rest=0;
        for(int i=Limbs-1;i>=0;i--){const quint64 current=rest<<32|m[i];r.m[i]=quint32(current/a);rest=current%a;}
        if(rest)r.m[0]|=1;r.negative=!r.zero()&&negative!=(d<0);return r.rounded();
    }
    // Delphi's Round (FISTP): to the nearest integer, half to even.
    qint64 round() const{
        quint64 value=quint64(m[5])|quint64(m[6])<<32;if(bit(Fraction-1)&&(anyBelow(Fraction-1)||(value&1)))++value;
        return negative?-qint64(value):qint64(value);
    }
};
// t = 0, 0.05, 0.1, … accumulated in extended precision from the double 0.05, and t² rounded: 20 samples per curve.
struct Steps {
    std::array<Extended,20> t,t2;
    Steps(){const auto step=Extended::fromDouble(0.05);Extended v;for(int k=0;k<20;k++){t[k]=v;t2[k]=(v*v).rounded();v=(v+step).rounded();}}
};
const Steps &steps(){static const Steps s;return s;}
qint64 roundHalfEven(double v){return qint64(std::nearbyint(v));}
// The original's quadratic curve from S over the control point B to E: 20 samples and E.
void bezier(QList<QPoint> &out,QPoint s,QPoint b,QPoint e){
    const qint64 ax=qint64(s.x())-2*b.x()+e.x(),bx=2*qint64(b.x())-2*s.x(),ay=qint64(s.y())-2*b.y()+e.y(),by=2*qint64(b.y())-2*s.y();
    const auto &k=steps();
    for(int i=0;i<20;i++){
        auto coordinate=[&](qint64 a,qint64 b,int start){return int(((((k.t2[i]*a).rounded()+(k.t[i]*b).rounded()).rounded())+Extended::integer(start)).rounded().round());};
        out.append(QPoint(coordinate(ax,bx,s.x()),coordinate(ay,by,s.y())));
    }
    out.append(e);
}
// mid(A,B) = A + (B−A) div 2, truncated toward zero, so mid(A,B) and mid(B,A) can differ.
QPoint mid(QPoint a,QPoint b){return QPoint(a.x()+(b.x()-a.x())/2,a.y()+(b.y()-a.y())/2);}
int distance(QPoint a,QPoint b){const double dx=a.x()-b.x(),dy=a.y()-b.y();return dx==0&&dy==0?0:int(roundHalfEven(std::sqrt(dx*dx+dy*dy)));}
// The point at distance r from B toward A, or A itself when A is not farther away than r.
QPoint cut(QPoint a,QPoint b,double r){
    const int d=distance(a,b);if(!(r<d))return a;const auto ratio=Extended::fromDouble(r).divided(d);
    return QPoint(int(((ratio*(qint64(a.x())-b.x())).rounded()+Extended::integer(b.x())).rounded().round()),int(((ratio*(qint64(a.y())-b.y())).rounded()+Extended::integer(b.y())).rounded().round()));
}
QList<QPoint> pathOf(const QList<QPoint> &p,int kind,bool smooth,int style,double r){
    QList<QPoint> out;const int n=int(p.size());if(n==0)return out;const bool closed=kind==6||kind==7;
    auto at=[&](int i){return p[i%n];};
    auto corner=[&](int i){
        if(style==1){out.append(cut(at(i),at(i+1),r));out.append(cut(at(i+2),at(i+1),r));}
        else if(style==2)bezier(out,cut(at(i),at(i+1),r),at(i+1),cut(at(i+2),at(i+1),r));
        else bezier(out,mid(at(i),at(i+1)),at(i+1),mid(at(i+1),at(i+2)));
    };
    if(closed){
        if(!smooth){out=p;out.append(p.first());}
        else if(style==1){for(int i=0;i<=n;i++)corner(i);out.append(out.first());}
        else if(style==2){for(int i=0;i<n;i++)corner(i);out.append(out.first());}
        else for(int i=0;i<n;i++)corner(i);
    }else if(!smooth)out=p;
    else{out.append(p.first());for(int i=0;i<n-2;i++)corner(i);out.append(p.last());}
    return out;
}
QPoint point(const QJsonValue &v){const auto a=v.toArray();return QPoint(a.at(0).toInt(),a.at(1).toInt());}
// Plotter units: Round((x − Ursprung)·0.4), which never meets a half, so integer arithmetic gives the same.
int unit(qint64 v){const qint64 n=4*v+5;return int(n>=0?n/10:-((-n+9)/10));}
class Plot {
public:
    QByteArray bytes;bool down=false,open=false,layer2=false;
    void write(const QByteArray &s){if(!open){open=true;write("IN;");write(layer2?"SP2;":"SP1;");write("PT0;");down=true;penUp();}bytes+=s+"\r\n";}
    void penUp(){if(down)write("PU;");down=false;}
    void penDown(){if(!down)write("PD;");down=true;}
    void move(QPoint p){penUp();write("PA"+QByteArray::number(p.x())+","+QByteArray::number(p.y())+";");}
    void line(QPoint p){penDown();write("PA"+QByteArray::number(p.x())+","+QByteArray::number(p.y())+";");}
};
}
QString plotNumber(double value){auto text=QString::number(value,'g',15);return englishUi()?text:text.replace('.',',');}
PlotBoard plotBoard(const Project &project){
    auto single=project;single.boards={};single.activeBoard=0;const auto document=LegacyReader(writeLegacyProject(single)).read(true);
    PlotBoard board;board.name=project.title;const auto size=document["size"].toArray();board.width=size.at(0).toInt();board.height=size.at(1).toInt();
    const auto raw=QByteArray::fromHex(document["raw_points"].toString().toLatin1());if(raw.size()>=8)board.origin=QPoint(qFromLittleEndian<qint32>(raw.constData()),qFromLittleEndian<qint32>(raw.constData()+4));
    // The original collects by kind in this order, walking groups in place: cuts, drills, circles, outlines, lines.
    for(const int kind:{3,14,5,6,7,4}){
        std::function<void(const QJsonArray&)> walk=[&](const QJsonArray &objects){for(const auto &v:objects){const auto o=v.toObject();if(o.contains("children"))walk(o["children"].toArray());else if(o["kind"].toInt()==kind)board.objects.append(o);}};
        walk(document["objects"].toArray());
    }
    return board;
}
QList<PlotJob> plotJobs(const PlotBoard &board,PlotOptions &options){
    QList<int> drills,mills,cuts;
    for(int i=0;i<board.objects.size();i++){
        const auto &o=board.objects[i];const auto type=o["type"].toString();
        if(type=="TBohrung"){if(options.drills)drills.append(i);}else if(type.startsWith("TTrenner")){if(options.cuts)cuts.append(i);}else if(o["flag3"].toBool()&&options.mills)mills.append(i);
    }
    // The original's unstable exchange sort, copied literally: the order of equal keys matters for the files.
    auto sort=[&](QList<int> &list,auto key){for(int i=0;i+1<list.size();i++)for(int j=i+1;j<list.size();j++)if(key(list[j])<key(list[i]))std::swap(list[i],list[j]);};
    auto diameter=[&](int i){return board.objects[i]["diameter"].toDouble();};auto width=[&](int i){return board.objects[i]["width"].toInt();};
    sort(drills,diameter);sort(mills,width);sort(cuts,width);
    // The spin edit is lowered to the smallest drill (10 mm without drills) and read back as float32 from its text.
    double smallest=10;for(int i:drills)smallest=std::min(smallest,diameter(i));
    if(options.tool>smallest)options.tool=float(QString::number(smallest,'f',2).toDouble());
    QList<PlotJob> jobs;
    if(options.millDrills&&!drills.isEmpty())jobs.append({PlotJob::MillDrill,ui("Bohrungen fräsen mit ")+(englishUi()?QString::number(double(options.tool),'f',2):QString::number(double(options.tool),'f',2).replace('.',','))+" mm",double(options.tool),options.layer2,drills});
    else{double last=-1;for(int i:drills){if(diameter(i)!=last){last=diameter(i);jobs.append({PlotJob::Drill,ui("Bohren mit ")+plotNumber(last)+" mm",last,options.layer2,{}});}jobs.last().objects.append(i);}}
    {int last=-1;for(int i:mills){if(width(i)!=last){last=width(i);jobs.append({PlotJob::Mill,ui("Fräsen mit ")+plotNumber(last/100.0)+" mm",last/100.0,options.layer2,{}});}jobs.last().objects.append(i);}}
    // Each cut width gets a job of the same name; the widest one's file is written last and remains.
    {int last=-1;for(int i:cuts){if(width(i)!=last){last=width(i);jobs.append({PlotJob::Cut,ui("Trennstellen"),-1,false,{}});}jobs.last().objects.append(i);}}
    if(options.outline)jobs.append({PlotJob::Outline,ui("Aussenrechteck mit ")+plotNumber(options.outlineWidth/100.0)+" mm",options.outlineWidth/100.0,options.layer2,{}});
    return jobs;
}
QList<QPoint> plotPath(const QJsonObject &object){
    if(object["type"]=="TKreis")return plotPath(object["inner"].toObject());
    QList<QPoint> points;for(const auto &v:object["path"].toArray())points.append(point(v));
    return pathOf(points,object["kind"].toInt(),object["flag2"].toBool(),object["style"].toInt(),object["rotation"].toDouble());
}
QList<QPoint> plotDrillCircle(const QJsonObject &drill,float tool){
    // The original adds the diameter in mm to a coordinate in 1/100 mm, so the radius is about that of the tool.
    const QPoint c=point(drill["center"]);
    const auto top=(Extended::integer(c.y())+Extended::fromDouble(drill["diameter"].toDouble()/2)).rounded()+(Extended::fromDouble(double(tool))*-100).rounded().divided(2);
    const qint64 py=top.rounded().round();const double radius=std::abs(double(py-c.y()))/std::cos(2*std::numbers::pi/16/2);
    QList<QPoint> corners;for(int i=1;i<=16;i++){const double a=(2*std::numbers::pi/16)*i;corners.append(QPoint(int(roundHalfEven(std::cos(a)*radius+c.x())),int(roundHalfEven(std::sin(a)*radius+c.y()))));}
    return pathOf(corners,7,true,0,0);
}
QList<QPoint> plotOutline(const PlotBoard &board,int width){
    const double h=width/2.0;auto r=[](double v){return int(roundHalfEven(v));};
    return {QPoint(r(-h),r(board.height+h)),QPoint(r(board.width+h),r(board.height+h)),QPoint(r(board.width+h),r(-h)),QPoint(r(-h),r(-h))};
}
QByteArray plotFile(const PlotJob &job,const PlotBoard &board,const PlotOptions &options){
    Plot plot;plot.layer2=job.layer2;auto map=[&](QPoint p){return QPoint(unit(qint64(p.x())-board.origin.x()),unit(qint64(p.y())-board.origin.y()));};
    auto polyline=[&](const QList<QPoint> &points){for(int i=0;i<points.size();i++){if(i==0)plot.move(map(points[i]));else plot.line(map(points[i]));}};
    if(options.commonOrigin){
        // With the outer rectangle the mark uses the same value for both coordinates, including the origin offset of x.
        if(options.outline){const int v=int(roundHalfEven(options.outlineWidth/2.0));const int px=unit(qint64(v)-board.origin.x());plot.move(QPoint(-px,-px));}
        else plot.move(QPoint(0,0));
        plot.penDown();plot.penUp();
    }
    if(job.type==PlotJob::Outline)polyline(plotOutline(board,options.outlineWidth));
    for(int i:job.objects){
        const auto &o=board.objects[i];
        if(job.type==PlotJob::Drill){const auto p=map(point(o["center"]));plot.move(p);plot.line(p);}
        else if(job.type==PlotJob::MillDrill)polyline(plotDrillCircle(o,options.tool));
        else if(job.type==PlotJob::Cut){const auto r=o["rect"].toArray();const int left=r[0].toInt(),top=r[1].toInt(),right=r[2].toInt(),bottom=r[3].toInt();polyline({{left,top},{left,bottom},{right,bottom},{right,top},{left,top}});}
        else polyline(plotPath(o));
    }
    plot.move(QPoint(0,0));return plot.bytes;
}
}
