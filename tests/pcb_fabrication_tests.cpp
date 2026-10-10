// Tests of the PCB module's fabrication data. The Gerber and Excellon files are read back by small readers of their own
// that follow the formats' specifications and know nothing of the board model; their pictures and hole lists are then
// compared with the board.
#include "language.h"
#include "modules/pcb/copper.h"
#include "modules/pcb/draw.h"
#include "modules/pcb/fabrication.h"
#include "modules/pcb/gerberimport.h"
#include "modules/pcb/font.h"
#include "modules/pcb/milling.h"
#include "modules/pcb/model.h"
#include "legacy_reader.h"
#include "modules/pcb/outputs.h"
#include "modules/pcb/editor.h"
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace openloch;
using namespace openloch::pcb;
namespace {
void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
void require(bool b,const QString &message){if(!b)throw std::runtime_error(message.toStdString());}
bool near(double a,double b,double eps=1e-6){return std::abs(a-b)<eps;}
bool near(QPointF a,QPointF b,double eps=1e-6){return near(a.x(),b.x(),eps)&&near(a.y(),b.y(),eps);}

// The part of the output plane a picture shows, in millimetres with y upwards, and its pixels per millimetre.
struct Window {double left,bottom,right,top,scale;};
QImage blank(const Window &w){
    QImage image(int(std::lround((w.right-w.left)*w.scale)),int(std::lround((w.top-w.bottom)*w.scale)),QImage::Format_Grayscale8);image.fill(0);return image;
}

// Reads extended Gerber (RS-274X) as far as a board's layer needs it: format and unit, circle and rectangle apertures,
// polarity, linear and circular interpolation (multi quadrant), regions, draws and flashes. Dark polarity paints
// white, clear polarity black.
class GerberReader {
public:
    QImage image;
    int flashes=0,draws=0,arcs=0,regions=0,clears=0;
    QMap<int,QPair<char,QList<double>>> apertures;
    QList<QPair<QPointF,double>> flashed;   // position and size of each flash of a circle (output millimetres)
    GerberReader(const QByteArray &data,const Window &window):image(blank(window)),w(window),painter(&image){
        painter.setRenderHint(QPainter::Antialiasing,false);
        for(qsizetype i=0;i<data.size();){
            const char c=data[i];
            if(c=='%'){const qsizetype j=data.indexOf('%',i+1);require(j>i,"unterminated extended command");
                for(const auto &part:data.mid(i+1,j-i-1).split('*'))if(!part.trimmed().isEmpty())extended(part.trimmed());i=j+1;continue;}
            if(c=='\n'||c=='\r'||c==' '){i++;continue;}
            const qsizetype j=data.indexOf('*',i);require(j>i,"unterminated word command");command(data.mid(i,j-i).trimmed());i=j+1;
            if(ended)break;
        }
        painter.end();require(ended,"no M02 at the end");
    }
private:
    Window w;QPainter painter;
    int decimals=-1,mode=1,aperture=-1;double unit=0;bool dark=true,inRegion=false,quadrants=false,ended=false;
    QPointF at;QList<QPolygonF> contours;
    QPointF pixel(QPointF mm) const{return QPointF((mm.x()-w.left)*w.scale,(w.top-mm.y())*w.scale);}
    QColor colour() const{return dark?Qt::white:Qt::black;}
    void extended(const QByteArray &p){
        static const QRegularExpression format("^FSLAX(\\d)(\\d)Y(\\d)(\\d)$"),define("^ADD(\\d+)([CRO]),([0-9.]+)(?:X([0-9.]+))?$");
        if(auto m=format.match(QString::fromLatin1(p));m.hasMatch()){decimals=m.captured(2).toInt();require(m.captured(4).toInt()==decimals,"same format for x and y");return;}
        if(p=="MOMM"){unit=1;return;}if(p=="MOIN"){unit=25.4;return;}
        if(p=="LPD"||p=="LPC"){dark=p=="LPD";if(!dark)clears++;return;}
        if(auto m=define.match(QString::fromLatin1(p));m.hasMatch()){
            QList<double> v{m.captured(3).toDouble()};if(!m.captured(4).isEmpty())v.append(m.captured(4).toDouble());
            apertures[m.captured(1).toInt()]={m.captured(2)[0].toLatin1(),v};return;
        }
        throw std::runtime_error(("unknown extended command "+p).toStdString());
    }
    double coordinate(const QString &digits) const{require(decimals>=0&&unit>0,"coordinates before format and unit");return digits.toLongLong()/std::pow(10.0,decimals)*unit;}
    QPolygonF arcPoints(QPointF from,QPointF to,QPointF centre) const{
        const double r=std::hypot(from.x()-centre.x(),from.y()-centre.y());
        const double a0=std::atan2(from.y()-centre.y(),from.x()-centre.x()),a1=std::atan2(to.y()-centre.y(),to.x()-centre.x());
        double sweep=a1-a0;const bool full=std::hypot(from.x()-to.x(),from.y()-to.y())<1e-9;
        if(mode==3){while(sweep<=1e-12)sweep+=2*M_PI;if(full)sweep=2*M_PI;}else{while(sweep>=-1e-12)sweep-=2*M_PI;if(full)sweep=-2*M_PI;}
        const int n=std::max(16,int(std::abs(sweep)*r*w.scale));QPolygonF out;
        for(int k=0;k<=n;k++){const double a=a0+sweep*k/n;out.append(centre+QPointF(r*std::cos(a),r*std::sin(a)));}
        return out;
    }
    void command(const QByteArray &word){
        if(word.startsWith("G04"))return;
        if(word=="G01"){mode=1;return;}if(word=="G02"){mode=2;return;}if(word=="G03"){mode=3;return;}
        if(word=="G75"){quadrants=true;return;}
        if(word=="G36"){inRegion=true;contours.clear();return;}
        if(word=="G37"){
            require(inRegion,"G37 without G36");inRegion=false;painter.setPen(Qt::NoPen);painter.setBrush(colour());
            for(const auto &c:contours){require(c.size()>=4&&std::hypot(c.first().x()-c.last().x(),c.first().y()-c.last().y())<1e-9,"region contour not closed");
                QPolygonF px;for(auto p:c)px.append(pixel(p));painter.drawPolygon(px,Qt::WindingFill);}
            regions++;return;
        }
        if(word=="M02"){ended=true;return;}
        static const QRegularExpression select("^D(\\d+)$"),data("^(?:X(-?\\d+))?(?:Y(-?\\d+))?(?:I(-?\\d+))?(?:J(-?\\d+))?D0([123])$");
        if(auto m=select.match(QString::fromLatin1(word));m.hasMatch()){aperture=m.captured(1).toInt();require(apertures.contains(aperture),"undefined aperture");return;}
        const auto m=data.match(QString::fromLatin1(word));require(m.hasMatch(),QStringLiteral("unknown command ")+QString::fromLatin1(word));
        const QPointF to(m.captured(1).isEmpty()?at.x():coordinate(m.captured(1)),m.captured(2).isEmpty()?at.y():coordinate(m.captured(2)));
        const QPointF offset(m.captured(3).isEmpty()?0:coordinate(m.captured(3)),m.captured(4).isEmpty()?0:coordinate(m.captured(4)));
        const int d=m.captured(5).toInt();
        if(d==2){at=to;if(inRegion)contours.append(QPolygonF{to});return;}
        if(d==1){
            QPolygonF path;if(mode==1)path={at,to};else{require(quadrants,"arcs need G75");path=arcPoints(at,to,at+offset);arcs++;}
            if(inRegion){require(!contours.isEmpty(),"region segment without a start");for(qsizetype k=1;k<path.size();k++)contours.last().append(path[k]);}
            else{
                const auto a=apertures.value(aperture);require(a.first=='C',"draws need a circle aperture");
                QPolygonF px;for(auto p:path)px.append(pixel(p));painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(colour(),a.second[0]*w.scale,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawPolyline(px);draws++;
            }
            at=to;return;
        }
        // D03: a flash
        require(!inRegion,"no flashes in regions");const auto a=apertures.value(aperture);at=to;flashes++;
        painter.setPen(Qt::NoPen);painter.setBrush(colour());const QPointF c=pixel(to);
        if(a.first=='C'){painter.drawEllipse(c,a.second[0]/2*w.scale,a.second[0]/2*w.scale);flashed.append({to,a.second[0]});}
        else if(a.first=='R'){const double x=a.second[0]*w.scale,y=a.second.value(1)*w.scale;painter.drawRect(QRectF(c-QPointF(x/2,y/2),QSizeF(x,y)));}
        else throw std::runtime_error("unexpected aperture");
    }
};

// The board drawn as the screen and the printout draw it, in output coordinates: white where the layer has something.
QImage reference(const Board &b,int layer,const Window &w){
    QImage image=blank(w);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing,false);
    p.scale(w.scale,w.scale);p.translate(-(b.origin.x()+w.left),w.top-b.origin.y());
    if(isCopper(layer)){
        if(b.groundPlane[layer]){p.setPen(Qt::NoPen);p.setBrush(Qt::white);p.drawPath(groundPlane(b,layer));}
        for(const auto &e:b.elements)if(!copperOn(e,layer,b).isEmpty())paintElement(p,e,Qt::white);
    }else for(const auto &e:b.elements)if(e.layer==layer&&!e.cutout)paintElement(p,e,Qt::white);
    return image;
}
// Pixels that differ although the reference has the same value all around them: differences of more than a pixel.
int mismatches(const QImage &image,const QImage &ref,bool flipped=false){
    require(image.size()==ref.size(),"pictures of different size");int bad=0;
    for(int y=1;y+1<ref.height();y++)for(int x=1;x+1<ref.width();x++){
        const uchar v=image.constScanLine(y)[flipped?ref.width()-1-x:x],r=ref.constScanLine(y)[x];if(v==r)continue;
        bool inside=true;for(int dy=-1;dy<=1&&inside;dy++)for(int dx=-1;dx<=1;dx++)if(ref.constScanLine(y+dy)[x+dx]!=r){inside=false;break;}
        if(inside)bad++;
    }
    return bad;
}
// The picture read back matches the reference; otherwise both are saved for a look and the test fails.
void same(const QImage &image,const QImage &ref,const QString &what,bool flipped=false){
    const int bad=mismatches(image,ref,flipped);if(!bad)return;
    QString name=what;name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")),QStringLiteral("-"));
    image.save(QStringLiteral("fabrication-%1-read.png").arg(name));ref.save(QStringLiteral("fabrication-%1-reference.png").arg(name));
    throw std::runtime_error(QStringLiteral("%1: %2 pixels differ from the board").arg(what).arg(bad).toStdString());
}
int whitePixels(const QImage &image){int n=0;for(int y=0;y<image.height();y++)for(int x=0;x<image.width();x++)n+=image.constScanLine(y)[x]>127;return n;}

// Reads Excellon drill data: the tool table between M48 and %, then tools and coordinates. Numbers without a decimal
// point take the digits `integers`.`decimals`; with "LZ" the leading zeros are there and trailing ones may be missing,
// with "TZ" the trailing zeros are there and leading ones may be missing.
struct DrillReader {
    QMap<int,double> tools;     // diameters in millimetres
    QList<std::pair<int,QPointF>> holes;
    double unit=0;QString zeros;
    DrillReader(const QByteArray &data,int integers,int decimals){
        bool header=false;int tool=0;
        for(auto line:data.split('\n')){
            line=line.trimmed();if(line.isEmpty()||line.startsWith(';'))continue;
            if(line=="M48"){header=true;continue;}
            if(header){
                if(line=="%"){header=false;continue;}
                if(line.startsWith("METRIC")||line.startsWith("INCH")){unit=line.startsWith("METRIC")?1:25.4;zeros=QString::fromLatin1(line.split(',').value(1));continue;}
                if(line=="FMAT,2")continue;
                static const QRegularExpression def("^T(\\d+)C([0-9.]+)$");const auto m=def.match(QString::fromLatin1(line));require(m.hasMatch(),"unknown header line");
                tools[m.captured(1).toInt()]=m.captured(2).toDouble()*unit;continue;
            }
            if(line=="G90"||line=="G05"||line=="M30"||line=="T0")continue;
            static const QRegularExpression select("^T(\\d+)$"),xy("^X(-?[0-9.]+)Y(-?[0-9.]+)$");
            if(auto m=select.match(QString::fromLatin1(line));m.hasMatch()){tool=m.captured(1).toInt();require(tools.contains(tool),"undefined tool");continue;}
            const auto m=xy.match(QString::fromLatin1(line));require(m.hasMatch(),"unknown drill line");
            auto value=[&](QString v){
                if(v.contains('.'))return v.toDouble()*unit;
                const bool negative=v.startsWith('-');if(negative)v.remove(0,1);
                if(zeros=="TZ")v=v.rightJustified(integers+decimals,'0');else v=v.leftJustified(integers+decimals,'0');
                return (negative?-1:1)*v.toLongLong()/std::pow(10.0,decimals)*unit;
            };
            holes.append({tool,QPointF(value(m.captured(1)),value(m.captured(2)))});
        }
    }
};

// Reads HPGL as a milling program does: IN, PA, SP, PU and PD with or without points, CI. Each pen gets the paths
// drawn with the pen down, the plunges (PD without points, perhaps followed by one short PA) and the circles.
struct HpglReader {
    struct Pen {QList<QPolygonF> paths;QList<QPointF> plunges;QList<std::pair<QPointF,double>> circles;};
    QMap<int,Pen> pens;QList<int> order;
    HpglReader(const QByteArray &data,double perUnit){
        int pen=-1;bool down=false;QPointF at;
        for(auto command:data.split(';')){
            command=command.trimmed();if(command.isEmpty())continue;
            const QByteArray name=command.left(2),rest=command.mid(2);
            QList<double> v;if(!rest.isEmpty())for(const auto &n:rest.split(','))v.append(n.toDouble()*perUnit);
            require(v.size()%2==0||name=="CI"||name=="SP","coordinates in pairs");
            if(name=="IN"||(name=="PA"&&v.isEmpty()))continue;
            if(name=="SP"){pen=int(rest.toInt());if(pen>0){order.append(pen);pens[pen];}continue;}
            require(pen>0,"drawing before a pen is chosen");auto &p=pens[pen];
            if(name=="PU"){down=false;if(v.size()>=2)at=QPointF(v[v.size()-2],v.last());continue;}
            if(name=="PD"){
                if(v.isEmpty()){down=true;p.plunges.append(at);continue;}
                if(!down){p.paths.append(QPolygonF{at});down=true;}
                for(qsizetype k=0;k+1<v.size();k+=2){at=QPointF(v[k],v[k+1]);p.paths.last().append(at);}
                continue;
            }
            if(name=="PA"){require(down&&v.size()==2&&std::hypot(v[0]-at.x(),v[1]-at.y())<.03,"a short feed in a plunge");continue;}
            if(name=="CI"){p.circles.append({at,v.value(0)});continue;}
            throw std::runtime_error(("unknown HPGL command "+command).toStdString());
        }
    }
};
// The copper of a side united in fine units, grown by `by`.
QPainterPath copperOf(const Board &b,int side,double by){
    QPainterPath copper;copper.setFillRule(Qt::WindingFill);
    for(const auto &e:b.elements)copper.addPath(QTransform::fromScale(100,100).map(copperOn(e,side,b)));
    copper=copper.simplified();
    if(by>0){QPainterPathStroker grow;grow.setWidth(200*by);grow.setJoinStyle(Qt::RoundJoin);grow.setCapStyle(Qt::RoundCap);copper=copper.united(grow.createStroke(copper));}
    return QTransform::fromScale(.01,.01).map(copper);
}
// The distance of a point from a line segment.
double segmentDistance(QPointF q,const QLineF &l){
    const QPointF d=l.p2()-l.p1();const double t=std::clamp(QPointF::dotProduct(q-l.p1(),d)/std::max(1e-300,QPointF::dotProduct(d,d)),0.0,1.0);
    const QPointF c=l.p1()+t*d;return std::hypot(q.x()-c.x(),q.y()-c.y());
}
// The isolation paths of the bottom side as plain path booleans give them: the copper of the side grown pass by pass,
// each hatched area (its grid lines and border stroked as drawn) grown on its own.
QList<QPolygonF> isolationReference(const Board &b,const MillingSettings &s,const QList<int> &selection){
    constexpr double fine=100;auto scaled=[](const QPainterPath &path,double f){return QTransform::fromScale(f,f).map(path);};
    auto grown=[](const QPainterPath &shape,double distance){QPainterPathStroker g;g.setWidth(2*distance);g.setJoinStyle(Qt::RoundJoin);g.setCapStyle(Qt::RoundCap);return shape.united(g.createStroke(shape));};
    QPainterPath copper;copper.setFillRule(Qt::WindingFill);QList<QPainterPath> grids;
    for(int i=0;i<b.elements.size();i++){
        const auto &e=b.elements[i];if(s.onlySelected&&!selection.contains(i))continue;const auto shape=copperOn(e,CopperBottom,b);if(shape.isEmpty())continue;
        if(e.type!=ElementType::Area||!e.hatched){copper.addPath(scaled(shape,fine));continue;}
        QPainterPath columns,rows;for(const auto &l:hatchLines(e)){auto &to=l.x1()==l.x2()?columns:rows;to.moveTo(l.p1()*fine);to.lineTo(l.p2()*fine);}
        QPainterPathStroker line;line.setWidth(hatchLineWidth(e)*fine);line.setCapStyle(Qt::RoundCap);line.setJoinStyle(Qt::RoundJoin);
        QPainterPath grid=line.createStroke(columns).simplified().united(line.createStroke(rows).simplified());
        if(e.width>0){QPainterPath border;border.addPolygon(e.points);border.closeSubpath();line.setWidth(e.width*fine);grid=grid.united(line.createStroke(scaled(border,fine)));}
        grids.append(grid);
    }
    copper=copper.simplified();if(b.groundPlane[CopperBottom]&&!s.onlySelected)copper=copper.united(scaled(groundPlane(b,CopperBottom),fine));
    QList<QPolygonF> out;const double step=std::max(.001,s.toolWidth*(1-s.overlap/100));
    for(int k=0;k<std::max(1,s.passes);k++){
        const double d=(s.toolWidth/2+k*step)*fine;QPainterPath all=grown(copper,d);for(const auto &g:grids)all=all.united(grown(g,d));
        // As the module: slivers below a hundredth of a millimetre are no paths.
        for(auto p:all.toSubpathPolygons()){const QRectF r=p.boundingRect();if(p.size()<2||std::max(r.width(),r.height())<1)continue;if(p.first()!=p.last())p.append(p.first());out.append(QTransform::fromScale(1/fine,1/fine).map(p));}
    }
    return out;
}
double signedArea(const QPolygonF &p){double s=0;for(qsizetype i=0;i+1<p.size();i++)s+=p[i].x()*p[i+1].y()-p[i+1].x()*p[i].y();return s/2;}
// Whether two lists hold the same closed paths, in any order and from any corner: bounding boxes within `tolerance`,
// areas within `tolerance` times the length of the path, turning the same way.
bool samePaths(const QList<QPolygonF> &a,const QList<QPolygonF> &b,double tolerance){
    if(a.size()!=b.size())return false;QList<bool> used(b.size(),false);
    for(const auto &p:a){
        const QRectF r=p.boundingRect();const double area=signedArea(p);double length=0;for(qsizetype i=0;i+1<p.size();i++)length+=QLineF(p[i],p[i+1]).length();
        bool found=false;
        for(qsizetype k=0;k<b.size()&&!found;k++){
            if(used[k])continue;const QRectF q=b[k].boundingRect();const double other=signedArea(b[k]);
            if(std::abs(r.left()-q.left())<tolerance&&std::abs(r.top()-q.top())<tolerance&&std::abs(r.right()-q.right())<tolerance&&std::abs(r.bottom()-q.bottom())<tolerance
                    &&std::abs(area-other)<tolerance*length&&(area>0)==(other>0))used[k]=found=true;
        }
        if(!found)return false;
    }
    return true;
}
// Paths in milling order by a plain search over all of them: always the nearest next, closed paths entered at their
// nearest corner, open ones at the nearer end; at equal distances the path listed first and its first corner.
QList<QPolygonF> plainOrder(QList<QPolygonF> paths,QPointF here){
    QList<QPolygonF> out;
    while(!paths.isEmpty()){
        qsizetype best=0,corner=0;double d=1e300;
        for(qsizetype i=0;i<paths.size();i++){
            const auto &p=paths[i];const bool closed=p.size()>2&&p.first()==p.last();
            for(qsizetype k=0;k<(closed?p.size()-1:1);k++){const double e=std::hypot(p[k].x()-here.x(),p[k].y()-here.y());if(e<d){d=e;best=i;corner=k;}}
            if(!closed){const double e=std::hypot(p.last().x()-here.x(),p.last().y()-here.y());if(e<d){d=e;best=i;corner=-1;}}
        }
        QPolygonF p=paths.takeAt(best);
        if(corner<0)std::reverse(p.begin(),p.end());
        else if(corner>0){QPolygonF turned;for(qsizetype k=0;k<p.size()-1;k++)turned.append(p[(corner+k)%(p.size()-1)]);turned.append(turned.first());p=turned;}
        out.append(p);here=p.last();
    }
    return out;
}

// A board with every kind of copper on the bottom side, a ground plane with a thermal pad, a keep-out area, a
// through-plated pad and a few things on the top side.
Board fabricationBoard(){
    Board b=newBoard(QStringLiteral("Fertigung"),40,30);b.groundPlane[CopperBottom]=true;
    auto pad=[&](QPointF at,PadShape shape,double size,double drill,double rotation=0){
        auto e=newElement(ElementType::Pad);e.pos=at;e.shape=shape;e.size=size;e.size2=drill;e.rotation=rotation;e.clearance=.4;updateOutline(e);b.elements<<e;return int(b.elements.size())-1;};
    const int via=pad({5,5},PadShape::Round,2,.8);b.elements[via].via=true;
    pad({10,5},PadShape::Square,2,.8);pad({15,5},PadShape::Octagon,2,.8,22.5);pad({20,5},PadShape::OvalWide,1.6,.7,30);
    {auto e=newElement(ElementType::SmdPad);e.layer=CopperBottom;e.pos={26,5};e.size=1.2;e.size2=2.4;e.rotation=30;e.clearance=.3;updateOutline(e);b.elements<<e;}
    {auto e=newElement(ElementType::Track);e.points={{5,10},{15,10},{15,15}};e.width=.6;e.flatEnd=true;e.clearance=.35;b.elements<<e;}
    {auto e=newElement(ElementType::Area);e.points={{20,10},{30,10},{30,16},{25,18},{20,16}};e.width=.3;e.clearance=.5;b.elements<<e;}
    {auto e=newElement(ElementType::Area);e.points={{2,19},{12,19},{12,27},{2,27}};e.width=.2;e.hatched=true;e.hatchAuto=false;e.hatchPitch=1;e.clearance=.4;b.elements<<e;}
    {auto e=newElement(ElementType::Circle);e.layer=CopperBottom;e.pos={33,24};e.size=3;e.width=.4;e.start=30;e.stop=300;e.clearance=.4;b.elements<<e;}
    {auto e=newElement(ElementType::Circle);e.layer=CopperBottom;e.pos={20,23};e.size=2;e.width=.2;e.start=0;e.stop=90;e.filled=true;e.clearance=.4;b.elements<<e;}
    {auto e=newElement(ElementType::Text);e.layer=CopperBottom;e.text=QStringLiteral("Ai");e.pos={15,28.5};e.size=2.5;e.mirrored=true;e.clearance=.3;updateStrokes(e);b.elements<<e;}
    {auto e=newElement(ElementType::Area);e.points={{35,2},{39,2},{39,8},{35,8}};e.width=0;e.cutout=true;b.elements<<e;}
    {const int t=pad({35,14},PadShape::Round,2,.8);b.elements[t].thermal=true;b.elements[t].thermalSpokes=0x55;b.elements[t].clearance=.5;}
    {auto e=newElement(ElementType::Track);e.points={{31,19},{38,19}};e.width=.5;e.clearance=0;b.elements<<e;}
    {auto e=newElement(ElementType::Area);e.points={{24,20},{29,20},{29,22},{24,22}};e.width=0;e.maskOnly=true;b.elements<<e;}
    // The top side
    {auto e=newElement(ElementType::SmdPad);e.layer=CopperTop;e.pos={30,27};e.size=1;e.size2=1.6;updateOutline(e);b.elements<<e;}
    {auto e=newElement(ElementType::Track);e.layer=CopperTop;e.points={{5,5},{5,15}};e.width=.4;e.solderMask=true;b.elements<<e;}
    {auto e=newElement(ElementType::Text);e.layer=SilkTop;e.text=QStringLiteral("OL");e.pos={2,3};e.size=2;updateStrokes(e);b.elements<<e;}
    {auto e=newElement(ElementType::Track);e.layer=Outline;e.points={{1,1},{39,1},{39,29},{1,29},{1,1}};e.width=.2;b.elements<<e;}
    return b;
}
}

int fabricationTests(){
    const Board b=fabricationBoard();const Window window{0,0,40,30,40};
    // --- The frame: millimetres from the origin, y upwards; mirroring turns about the middle of the working area.
    {const auto f=outputFrame(b);require(f.map({5,5})==QPointF(5,25)&&f.map({0,30})==QPointF(0,0),"output frame");
        const auto m=outputFrame(b,1);require(m.map({5,5})==QPointF(35,25)&&m.reverses(),"mirrored frame");
        Board moved=b;moved.origin={10,20};require(outputFrame(moved).map({15,5})==QPointF(5,15),"frame from the origin");}

    // --- Gerber: every layer read back and compared with the board's picture
    for(int layer:{CopperBottom,CopperTop,SilkTop,Outline}){
        const QByteArray data=gerber(b,layer,{});
        require(data.contains("%FSLAX46Y46*%")&&data.contains("%MOMM*%")&&data.endsWith("M02*\n"),"Gerber header and end");
        GerberReader read(data,window);const QImage ref=reference(b,layer,window);
        require(whitePixels(ref)>1000,"reference picture has something");
        same(read.image,ref,QStringLiteral("Gerber layer %1").arg(layer));
        if(layer==CopperBottom)require(read.clears==1&&read.regions>10&&read.arcs>=2&&read.flashes>=4,"ground plane with clear polarity, regions, arcs and flashes");
    }
    {const QByteArray data=gerber(b,CopperTop,{});require(!data.contains("%LPC*%")&&!data.contains("G36"),"no polarity change or region without a ground plane");}
    // Hatched areas: their lines are strokes of a round aperture half the pitch wide, not cut to the outline; the plane
    // leaves the whole outline free, also at clearance 0.
    {Board h=newBoard(QStringLiteral("Raster"),40,30);h.groundPlane[CopperBottom]=true;
        auto hatched=[&](const QPolygonF &outline,double width,double pitch,double clearance){
            auto e=newElement(ElementType::Area);e.layer=CopperBottom;e.points=outline;e.width=width;e.hatched=true;e.hatchAuto=pitch<=0;e.hatchPitch=pitch;e.clearance=clearance;h.elements<<e;};
        hatched({{3,3},{13,3},{13,13},{3,13}},.2,2,0);                                      // line ends 0.4 mm beyond the border, on the plane
        hatched({{17,3},{29,3},{29,9},{23,13},{17,9}},.2,2,.2);                             // line ends reaching through the clearance
        hatched({{3,17},{13,17},{13,27},{9,27},{9,22},{6,22},{6,27},{3,27}},.15,0,0);      // pitch from the border, at least 0.5 mm; edges on lines
        hatched({{17.2,17.2},{30,17.2},{24,27}},.3,1.5,.4);
        const QByteArray data=gerber(h,CopperBottom,{});GerberReader read(data,window);const QImage ref=reference(h,CopperBottom,window);
        same(read.image,ref,QStringLiteral("hatched areas"));
        require(read.clears==1&&data.contains("C,1*%")&&data.contains("C,0.25*%")&&data.contains("C,0.75*%"),"grid lines with round apertures of half the pitch");
        auto copper=[&](double x,double y){return ref.pixelColor(int(x*window.scale),int(y*window.scale)).value()>127;};
        require(copper(2.85,5)&&copper(2.95,5)&&!copper(3.2,5)&&!copper(5,5),"plane up to the border at clearance 0, none in the gaps");
        require(copper(18,2.8)&&!copper(19,2.8)&&copper(19,2.6),"a line end reaching across the clearance");
        // Read back by the Gerber import, the layer draws the same.
        GerberImport in;in.layers[CopperBottom]=readGerber(data);same(reference(importGerber(in,QStringLiteral("Import")),CopperBottom,window),ref,QStringLiteral("imported hatched areas"));}
    // Mirrored output: the same picture, flipped left-right.
    {GerberSettings s;s.mirrored=true;GerberReader read(gerber(b,CopperBottom,s),window);same(read.image,reference(b,CopperBottom,window),QStringLiteral("mirrored Gerber"),true);}
    // Holes left open, or marked by a small punch mark.
    {GerberSettings s;s.clearHoles=true;GerberReader read(gerber(b,CopperBottom,s),window);
        QImage ref=reference(b,CopperBottom,window);{QPainter p(&ref);p.setRenderHint(QPainter::Antialiasing,false);p.scale(window.scale,window.scale);p.translate(0,0);
            p.setPen(Qt::NoPen);p.setBrush(Qt::black);for(const auto &e:b.elements)if(e.type==ElementType::Pad&&e.size2>0)p.drawEllipse(QPointF(e.pos.x(),e.pos.y()),e.size2/2,e.size2/2);}
        same(read.image,ref,QStringLiteral("holes left open"));
        s.punchMarks=true;GerberReader marks(gerber(b,CopperBottom,s),window);int dots=0;
        for(const auto &[at,size]:marks.flashed)dots+=near(size,punchMarkDiameter);require(dots==5,"punch marks");}
    // The frame of the working area.
    {GerberSettings s;s.frame=true;GerberReader read(gerber(b,SilkTop,s),window);
        require(read.image.pixelColor(400,1).value()==255&&read.image.pixelColor(1,600).value()==255&&read.image.pixelColor(400,600).value()==0,"frame");}
    // Solder mask: openings larger than the copper, pads with sharp corners; inverted it is the mask itself.
    {GerberSettings s;s.padOffset=.15;s.smdOffset=.1;s.otherOffset=.05;
        auto maskReference=[&](int side){
            QImage ref=blank(window);QPainter p(&ref);p.setRenderHint(QPainter::Antialiasing,false);p.scale(window.scale,window.scale);
            p.setPen(Qt::NoPen);p.setBrush(Qt::white);
            for(const auto &e:b.elements){
                if(e.type==ElementType::Area&&e.maskOnly){if(e.layer==side)p.drawPath(copperShape(e));continue;}
                if(!e.solderMask)continue;
                const bool here=e.type==ElementType::Pad?(e.layer==side||e.via):!copperOn(e,side,b).isEmpty();if(!here)continue;
                const double by=e.type==ElementType::Pad?s.padOffset:e.type==ElementType::SmdPad?s.smdOffset:s.otherOffset;
                // Grown in hundredths of a millimetre, where the union keeps curves round.
                QPainterPath shape=QTransform::fromScale(100,100).map(copperShape(e));if(e.type==ElementType::Area)shape.setFillRule(Qt::WindingFill);
                QPainterPathStroker grow;grow.setWidth(200*by);
                const bool polygon=(e.type==ElementType::Pad&&e.points.size()>2)||e.type==ElementType::SmdPad;
                grow.setJoinStyle(polygon?Qt::MiterJoin:Qt::RoundJoin);grow.setMiterLimit(10);grow.setCapStyle(Qt::RoundCap);
                p.drawPath(QTransform::fromScale(.01,.01).map(shape.united(grow.createStroke(shape))));
            }
            return ref;
        };
        for(int side:{CopperBottom,CopperTop}){
            const int output=side==CopperTop?SolderMaskTop:SolderMaskBottom;
            GerberReader read(gerber(b,output,s),window);const QImage ref=maskReference(side);require(whitePixels(ref)>500,"mask reference");
            same(read.image,ref,QStringLiteral("solder mask %1").arg(output));
            s.maskInverted=true;GerberReader inverted(gerber(b,output,s),window);s.maskInverted=false;
            QImage negative=ref;negative.invertPixels();same(inverted.image,negative,QStringLiteral("inverted solder mask"));
        }
        s.maskPads=false;GerberReader noPads(gerber(b,SolderMaskBottom,s),window);require(noPads.flashes==0,"pads left out of the mask");}
    // SMD mask: the SMD pads, smaller by a negative offset.
    {GerberSettings s;s.pasteOffset=-.1;GerberReader read(gerber(b,PasteMaskTop,s),window);
        require(read.flashes==1&&near(read.apertures.first().second[0],.8)&&near(read.apertures.first().second[1],1.4),"SMD mask of an upright pad");
        GerberReader bottom(gerber(b,PasteMaskBottom,s),window);require(bottom.regions==1,"SMD mask of a turned pad");
        QImage ref=blank(window);{QPainter p(&ref);p.setRenderHint(QPainter::Antialiasing,false);p.scale(window.scale,window.scale);
            const auto &e=b.elements[4];QPainterPath pad;pad.addPolygon(e.points);pad.closeSubpath();QPainterPathStroker cut;cut.setWidth(.2);cut.setJoinStyle(Qt::MiterJoin);
            p.fillPath(pad.subtracted(cut.createStroke(pad)),Qt::white);}
        same(bottom.image,ref,QStringLiteral("smaller SMD mask"));}
    {require(gerberOutputUsed(b,CopperBottom)&&gerberOutputUsed(b,SilkTop)&&!gerberOutputUsed(b,SilkBottom)&&!gerberOutputUsed(b,Inner1),"outputs in use");
        require(gerberOutputUsed(b,SolderMaskTop)&&gerberOutputUsed(b,PasteMaskBottom),"masks in use");
        require(gerberOutputs().size()==gerberOutputCount&&gerberSuffix(CopperTop)==".GTL","outputs and suffixes");}

    // --- Excellon: tools, hole counts and positions against the board
    {const auto holes=drillHoles(b);require(holes.size()==5&&drillHoles(b,1).size()==1&&drillHoles(b,2).size()==4,"holes by kind");
        struct Case {DrillSettings s;int integers,decimals;};
        QList<Case> cases;
        {DrillSettings s;cases.append({s,2,4});}
        {DrillSettings s;s.suppressLeadingZeros=true;cases.append({s,2,4});}
        {DrillSettings s;s.metric=true;s.integerDigits=3;s.decimalDigits=3;s.fromBelow=true;cases.append({s,3,3});}
        {DrillSettings s;s.decimalPoint=true;s.sorted=false;s.holes=2;cases.append({s,2,4});}
        for(const auto &c:cases){
            const QByteArray data=excellon(b,c.s);
            require(data.startsWith("M48\n")&&data.endsWith("M30\n")&&data.contains(c.s.metric?"METRIC":"INCH"),"Excellon header and end");
            require(data.contains(c.s.decimalPoint?QByteArray("\nINCH\n"):c.s.metric?QByteArray("\nMETRIC,LZ,000.000\n"):c.s.suppressLeadingZeros?QByteArray("\nINCH,TZ\n"):QByteArray("\nINCH,LZ\n")),"zero format in the header");
            DrillReader read(data,c.integers,c.decimals);
            const auto expected=drillHoles(b,c.s.holes);require(read.holes.size()==expected.size(),"hole count");
            const auto frame=outputFrame(b,c.s.fromBelow?1:0);const double tolerance=c.s.metric?1e-3:.0001*25.4;
            for(const auto &h:expected){
                bool found=false;const QPointF at=frame.map(h.at);
                for(const auto &[tool,p]:read.holes)found|=near(p.x(),at.x(),tolerance)&&near(p.y(),at.y(),tolerance)&&near(read.tools[tool],h.diameter,tolerance);
                require(found,"hole at its place with its diameter");
            }
            // The smallest tool first.
            double last=0;for(auto d:read.tools){require(d>=last,"tools sorted by size");last=d;}
        }
        // Sorted holes take a short path: never longer than the board's order.
        auto length=[](const DrillReader &r){double l=0;QPointF at;for(const auto &[t,p]:r.holes){l+=std::hypot(p.x()-at.x(),p.y()-at.y());at=p;}return l;};
        DrillSettings sorted;sorted.metric=true;sorted.decimalPoint=true;DrillSettings plain=sorted;plain.sorted=false;
        require(length(DrillReader(excellon(b,sorted),0,0))<=length(DrillReader(excellon(b,plain),0,0))+1e-9,"sorted holes");
        // The documented digits: inch 2.4 without decimal point, leading zeros written or left out.
        Board one=newBoard(QStringLiteral("x"),50,50);auto e=newElement(ElementType::Pad);e.pos={25.4,50-12.7};updateOutline(e);one.elements<<e;
        require(excellon(one,{}).contains("X010000Y005000\n"),"inch 2.4 with leading zeros");
        one.name=QStringLiteral("Größe*%");require(excellon(one,{}).contains(";OpenLoch drill data, board Gr__e__\n")&&gerber(one,CopperBottom,{}).contains("board Gr__e__:"),"comments in ASCII");
        DrillSettings lz;lz.suppressLeadingZeros=true;require(excellon(one,lz).contains("X10000Y5000\n"),"inch 2.4 without leading zeros");
        // The reference's special options: M71/M72 for the unit, no G90, no comment lines.
        {DrillSettings o;o.m71=true;o.noG90=true;o.noComments=true;const QByteArray text=excellon(one,o);
            require(text.contains("\nM72\n")&&!text.contains("INCH")&&!text.contains("G90")&&!text.contains(';')&&text.contains("G05"),"M72, no G90, no comments");
            o.metric=true;require(excellon(one,o).contains("\nM71\n"),"M71 for millimetres");}
        // Drilled from below for HPGL machines: mirrored about the middle of the holes with their edges, not of the area.
        {Board two=newBoard(QStringLiteral("zwei"),50,50);auto p=newElement(ElementType::Pad);p.pos={10,25};p.size=2;p.size2=1;updateOutline(p);auto q=p;q.pos={20,25};q.size2=.6;updateOutline(q);two.elements<<p<<q;
            DrillSettings g;g.fromBelow=true;g.metric=true;g.decimalPoint=true;g.decimalDigits=3;DrillSettings h=g;h.mirrorHoles=true;
            require(excellon(two,g).contains("X40.000Y25.000")&&excellon(two,g).contains("X30.000Y25.000"),"mirrored about the middle of the working area");
            // The holes span 9.5 to 20.3: mirrored about 14.9.
            require(excellon(two,h).contains("X19.800Y25.000")&&excellon(two,h).contains("X9.800Y25.000"),"mirrored about the middle of the holes");}
        // The list of holes: each diameter with its count, plain and through-plated ones as chosen.
        {Board l=newBoard(QStringLiteral("liste"),50,50);
            auto hole=[&](QPointF at,double d,bool via){auto e=newElement(ElementType::Pad);e.pos=at;e.size=d+1;e.size2=d;e.via=via;updateOutline(e);l.elements<<e;};
            hole({5,5},.8,false);hole({10,5},.8,false);hole({15,5},1,true);hole({20,5},.6,true);
            require(holeTable(l,true,true)==QList<std::pair<double,int>>{{.6,1},{.8,2},{1,1}},"diameters with their counts, smallest first");
            require(holeTable(l,true,false)==QList<std::pair<double,int>>{{.8,2}}&&holeTable(l,false,true).size()==2,"plain or through-plated holes");
            require(holeList(l,true,true).endsWith(ui("Zusammen: %1").arg(4)),"the sum");}}

    // --- Component data: one asymmetric component on the top side and a copy turned onto the bottom side
    {Board c=newBoard(QStringLiteral("Bauteile"),50,40);
        auto smd=[&](QPointF at,int layer,int part){auto e=newElement(ElementType::SmdPad);e.layer=layer;e.pos=at;e.size=1;e.size2=1.5;e.part=part;updateOutline(e);c.elements<<e;};
        smd({10,10},CopperTop,1);smd({14,10},CopperTop,1);
        {auto e=newElement(ElementType::Track);e.layer=SilkTop;e.points={{8,8},{18,8},{18,12},{8,12}};e.width=.2;e.part=1;c.elements<<e;}
        {auto e=newElement(ElementType::Text);e.layer=SilkTop;e.role=TextRole::Designator;e.text=QStringLiteral("U1");e.pos={9,7};e.size=1.5;e.part=1;
            e.package=QStringLiteral("SOT,23");e.pickAndPlace=true;updateStrokes(e);c.elements<<e;}
        {auto e=newElement(ElementType::Text);e.layer=SilkTop;e.role=TextRole::Value;e.text=QStringLiteral("BC847");e.pos={9,14};e.size=1.5;e.part=1;updateStrokes(e);c.elements<<e;}
        const auto list=components(c);require(list.size()==1,"one component");
        require(near(pickPlaceCentre(c,list[0]).x(),12)&&near(pickPlaceCentre(c,list[0]).y(),10),"centre of the SMD pads");
        c.elements[3].pickCentre=1;require(near(pickPlaceCentre(c,list[0]).x(),13)&&near(pickPlaceCentre(c,list[0]).y(),10),"centre of the silkscreen, without the texts");
        c.elements[3].pickCentre=0;c.elements[3].pickOffset={1,.5};require(near(pickPlaceCentre(c,list[0]).x(),13)&&near(pickPlaceCentre(c,list[0]).y(),9.5),"offset upwards");
        // The copy: turned by 90° and put on the bottom side; its rotation counts clockwise there.
        QList<Element> copy;for(int i:list[0].members){auto e=c.elements[i];e.part=2;if(e.role==TextRole::Designator)e.text=QStringLiteral("U2");
            mirror(e,25);if(e.layer==CopperTop)e.layer=CopperBottom;else if(e.layer==SilkTop)e.layer=SilkBottom;rotate(e,{30,10},90);copy<<e;}
        c.elements+=copy;const auto both=components(c);require(both.size()==2&&!componentOnTop(c,both[1])&&componentOnTop(c,both[0]),"sides");
        require(near(c.elements[both[1].designator].componentRotation,270),"bottom rotation clockwise");
        require(near(c.elements[both[1].designator].pickOffset.x(),-.5)&&near(c.elements[both[1].designator].pickOffset.y(),-1),"offset turns with the component");
        ComponentDataSettings s;const QString text=componentData(c,s);const auto lines=text.trimmed().split('\n');
        require(lines.size()==3&&lines[0]=="Bezeichner,Wert,Package,Position X,Position Y,Rotation,Layer","header line");
        require(lines[1]=="U1,BC847,\"SOT,23\",13.00,30.50,0,Top","top component");
        const QPointF at=outputFrame(c).map(pickPlaceCentre(c,both[1]));
        require(lines[2]==QStringLiteral("U2,BC847,\"SOT,23\",%1,%2,270,Bottom").arg(at.x(),0,'f',2).arg(at.y(),0,'f',2),"bottom component");
        s.fields={FieldDesignator,FieldRotation,FieldX};s.separator=";";s.rotationPrefix=true;s.unit=1;s.decimals=0;s.header=false;s.onBottom=false;
        require(componentData(c,s)=="U1;R0;512\n","fields, separator, mil and the top side only");
        s.onBottom=true;s.pickPlaceOnly=true;c.elements[both[1].designator].pickAndPlace=false;require(componentData(c,s).count('\n')==1,"only components with pick and place data");
        c.elements[both[1].designator].pickAndPlace=true;
        // SMD components at first; through-hole ones only when chosen.
        {Board t=c;auto pad=newElement(ElementType::Pad);pad.pos={30,30};pad.part=7;updateOutline(pad);
            auto name=newElement(ElementType::Text);name.text=QStringLiteral("J1");name.role=TextRole::Designator;name.part=7;name.pos={30,33};name.size=1.5;updateStrokes(name);t.elements<<pad<<name;
            ComponentDataSettings k;k.fields={FieldDesignator};k.header=false;require(componentData(t,k)=="U1\nU2\n","SMD components only at first");
            k.throughHoleParts=true;require(componentData(t,k)=="J1\nU1\nU2\n","through-hole ones when chosen, in the order of the component list");
            k.smdParts=false;require(componentData(t,k)=="J1\n","or through-hole ones alone");}
        {ComponentDataSettings z;z.fields={FieldX,FieldY};z.header=false;z.onBottom=false;z.decimals=3;z.stripZeros=true;require(componentData(c,z)=="13,30.5\n","zeros at the end left out");}
        // The running number counts the lines written, after the filter.
        {ComponentDataSettings n;n.fields={FieldIndex,FieldDesignator};n.header=false;require(componentData(c,n)=="1,U1\n2,U2\n","running number");
            n.onTop=false;require(componentData(c,n)=="1,U2\n","the running number counts what is written");}
        // The dialog: its preview is the file; the checked fields of its list are written.
        ComponentDataDialog dialog(c,QStringLiteral("platine"),ComponentDataSettings{});
        require(dialog.preview()==componentData(c,ComponentDataSettings{})&&dialog.suggestedFile()=="platine.csv","preview of the component data");
        auto *fieldList=dialog.findChild<QListWidget*>("componentFields");require(fieldList&&fieldList->count()==componentFieldCount,"list of all fields");
        fieldList->item(0)->setCheckState(Qt::Unchecked);
        require(!dialog.settings().fields.contains(FieldDesignator)&&dialog.findChild<QPlainTextEdit*>("componentPreview")->toPlainText()==dialog.preview(),"fields follow the list");}
    // --- Isolation milling
    {Board m=newBoard(QStringLiteral("Fräsen"),30,20);
        auto pad=[&](QPointF at,PadShape shape,double drill){auto e=newElement(ElementType::Pad);e.pos=at;e.shape=shape;e.size=2;e.size2=drill;updateOutline(e);m.elements<<e;};
        auto track=[&](QPointF a,QPointF c,double width,int layer=CopperBottom){auto e=newElement(ElementType::Track);e.layer=layer;e.points={a,c};e.width=width;m.elements<<e;};
        pad({5,10},PadShape::Round,.8);pad({15,10},PadShape::Square,.8);track({5,10},{15,10},.5);pad({25,10},PadShape::Round,.5);
        // Two tracks closer than the cutter: no path between them.
        track({18,15},{28,15},.4);track({18,15.55},{28,15.55},.4);
        track({1,1},{29,1},.2,Outline);
        MillingSettings s;s.toolWidth=.2;
        auto jobs=millingJobs(m,s);require(jobs.size()==1&&jobs[0].kind==MillingJobKind::Isolation&&jobs[0].side==CopperBottom,"isolation of the bottom side");
        require(jobs[0].paths.size()==3,"a closed path around each copper island, none between copper closer than the cutter");
        const auto near1=copperOf(m,CopperBottom,.095),far1=copperOf(m,CopperBottom,.105);
        for(const auto &p:jobs[0].paths){require(p.first()==p.last()&&p.size()>8,"closed paths");
            for(auto q:p)require(!near1.contains(q)&&far1.contains(q),"paths half a cutter width from the copper");}
        // Two passes: the second a cutter width less the overlap further out.
        s.passes=2;s.overlap=25;QList<int> steps;jobs=millingJobs(m,s,{},true,[&](int done,int total){steps<<done<<total;});
        require(jobs[0].paths.size()==6,"two paths around each island");require(steps==QList<int>({0,2,1,2}),"each path a step of the progress");
        {const auto near2=copperOf(m,CopperBottom,.245),far2=copperOf(m,CopperBottom,.255);int second=0;
            for(const auto &p:jobs[0].paths){bool out=true;for(auto q:p)out&=!near2.contains(q)&&far2.contains(q);second+=out;}require(second==3,"second pass further out");}
        s.passes=1;
        // A ground plane with a thermal pad and a turned SMD pad: the slivers the boolean operations leave are no paths
        // (they were thousands, some on the spokes), and no path comes within the cutter's radius of copper.
        {Board g=newBoard("Masse",20,20);g.groundPlane[CopperBottom]=true;
            auto pad=newElement(ElementType::Pad);pad.pos={10,10};pad.size=2;pad.size2=.8;pad.clearance=.5;pad.thermal=true;pad.thermalSpokes=0x55;updateOutline(pad);g.elements<<pad;
            auto smd=newElement(ElementType::SmdPad);smd.layer=CopperBottom;smd.pos={5,5};smd.size=1.2;smd.size2=2.4;smd.rotation=30;smd.clearance=.3;updateOutline(smd);g.elements<<smd;
            MillingSettings t;t.toolWidth=.2;const auto made=millingJobs(g,t);
            int slivers=0;for(const auto &p:made[0].paths){const QRectF r=p.boundingRect();slivers+=std::max(r.width(),r.height())<.05;}
            require(slivers==0&&made[0].paths.size()<=12,"no slivers milled");
            // The plane's holes keep their own fill rule only in a boolean union.
            QPainterPath copper=QTransform::fromScale(100,100).map(groundPlane(g,CopperBottom));
            for(const auto &e:g.elements)copper=copper.united(QTransform::fromScale(100,100).map(copperOn(e,CopperBottom,g)));
            QPainterPathStroker grow;grow.setWidth(200*.09);grow.setJoinStyle(Qt::RoundJoin);grow.setCapStyle(Qt::RoundCap);
            const QPainterPath close=QTransform::fromScale(.01,.01).map(copper.united(grow.createStroke(copper)));
            int touching=0;for(const auto &p:made[0].paths)for(auto q:p)touching+=close.contains(q);require(touching==0,"never within the cutter's radius of copper");}
        // Single line texts are milled along their strokes.
        {Board t=m;auto e=newElement(ElementType::Text);e.layer=CopperBottom;e.text=QStringLiteral("L");e.pos={8,18};e.size=2;updateStrokes(e);t.elements<<e;
            s.texts=1;const auto lines=millingJobs(t,s)[0].paths;s.texts=0;const auto outlined=millingJobs(t,s)[0].paths;
            require(lines.size()==3+e.strokes.size()&&outlined.size()==4,"texts as single lines or outlined");}
        // Drilling: a plunge per hole, a job per diameter, or milled circles with the cutter's radius taken off.
        s.bottom=false;s.drillSide=2;s.drillMode=2;jobs=millingJobs(m,s);
        require(jobs.size()==2&&jobs[0].plunges.size()==1&&std::abs(jobs[0].tool-.5)<1e-9&&jobs[1].plunges.size()==2,"a job per diameter");
        s.drillMode=0;s.drillToolWidth=.6;jobs=millingJobs(m,s);
        require(jobs.size()==1&&jobs[0].circles.size()==2&&std::abs(jobs[0].circles[0].second-.1)<1e-9&&jobs[0].skipped==1,"milled holes");
        // Registration holes first, the contour last; HPGL read back in the frame of each job.
        s.bottom=true;s.punchBottom=true;s.drillMode=1;s.contourSide=2;s.registrationCorners=1|2;s.registrationDistance=4;s.minimalFeed=true;
        jobs=millingJobs(m,s);
        require(jobs.size()==4&&jobs[0].kind==MillingJobKind::Registration&&jobs[1].kind==MillingJobKind::Isolation&&jobs[2].kind==MillingJobKind::Drill
                &&jobs[3].kind==MillingJobKind::Contour&&jobs[1].plunges.size()==3,"job order");
        require(jobs[0].plunges==QList<QPointF>{{-4,-4},{34,-4}},"registration holes off the corners");
        // Mirrored for the bottom side, the machine's zero lies at the origin's mirror image (30, 20): the contour starts
        // at the end nearer to it.
        require(jobs[3].paths.size()==1&&jobs[3].paths[0]==QPolygonF(QList<QPointF>{{29,1},{1,1}}),"contour along the outline's centre, from the end nearer the machine's zero");
        const auto text=hpgl(m,jobs,s);require(text.startsWith("IN;\nPA;\nSP1;")&&text.endsWith("SP0;\n"),"HPGL start and end");
        HpglReader read(text,.0254);require(read.order==QList<int>{1,2,3,4},"a pen per job");
        const auto frame=outputFrame(m,1);
        auto same=[&](QPointF a,QPointF b){return std::hypot(a.x()-b.x(),a.y()-b.y())<.02;};
        for(int k=0;k<4;k++){
            const auto &job=jobs[k];const auto &pen=read.pens[k+1];
            require(pen.paths.size()==job.paths.size()&&pen.plunges.size()==job.plunges.size(),"paths and plunges of a job");
            for(int i=0;i<job.paths.size();i++){require(pen.paths[i].size()==job.paths[i].size(),"points of a path");
                for(int n=0;n<job.paths[i].size();n++)require(same(pen.paths[i][n],frame.map(job.paths[i][n])),"path mirrored left-right, from the origin");}
            for(int i=0;i<job.plunges.size();i++)require(same(pen.plunges[i],frame.map(job.plunges[i])),"plunge positions");
        }
        s.roundedScale=true;s.drillMode=0;s.minimalFeed=false;jobs=millingJobs(m,s);HpglReader rounded(hpgl(m,jobs,s),.025);
        const auto &circles=rounded.pens[3].circles;require(circles.size()==2&&std::abs(circles[0].second-.1)<.02&&same(circles[0].first,frame.map(jobs[2].circles[0].first)),"CI circles at 0.025 mm per unit");
        require(millingJobList(jobs).count('\n')==4&&millingJobList(jobs).contains("SP3"),"job list");
        // The registration holes at all eight places; with a file per job, in every file before its job on their pen.
        {MillingSettings r=s;r.registrationCorners=255;r.registrationDistance=4;const auto all=millingJobs(m,r);
            require(all[0].plunges==QList<QPointF>({{-4,-4},{34,-4},{-4,24},{34,24},{15,-4},{34,10},{15,24},{-4,10}}),"corners and middles of the sides");
            r.separateFiles=true;auto files=millingFiles(m,all,r,QStringLiteral("platte"),QStringLiteral("plt"));
            require(files.size()==all.size()&&files[1].first=="platte_2.plt"&&!files[1].second.contains("SP1;"),"a file per job");
            r.registrationEveryFile=true;files=millingFiles(m,all,r,QStringLiteral("platte"),QStringLiteral("plt"));
            HpglReader second(files[1].second,.0254);require(second.order==QList<int>({1,2})&&second.pens[1].plunges.size()==8,"the registration holes first in every file, on their pen");
            r.separateFiles=false;require(millingFiles(m,all,r,QStringLiteral("platte"),QStringLiteral("plt")).size()==1,"or one file");}
        // Only the selected elements.
        s=MillingSettings();s.onlySelected=true;require(millingJobs(m,s,{3})[0].paths.size()==1,"only the selected elements");
        // A hatched area as it is made: a path round it and one in each gap of its grid.
        {Board g=newBoard(QStringLiteral("Raster"),20,20);auto e=newElement(ElementType::Area);e.layer=CopperBottom;e.points={{3,3},{13,3},{13,13},{3,13}};
            e.width=.2;e.hatched=true;e.hatchAuto=false;e.hatchPitch=2;g.elements<<e;
            MillingSettings gs;gs.toolWidth=.2;const auto paths=millingJobs(g,gs)[0].paths;int inGaps=0;
            for(const auto &p:paths){const QRectF r=p.boundingRect();inGaps+=r.width()<1&&r.height()<1;}
            require(paths.size()==37&&inGaps==36,"milled into the gaps of a hatched area");}
        // With the ground plane at clearance 0 the plane meets the border of a hatched area: every path inside the area
        // still lies half a cutter width from its lines and its border, one in each gap.
        {Board g=newBoard(QStringLiteral("Raster"),35,25);g.groundPlane[CopperBottom]=true;auto e=newElement(ElementType::Area);e.layer=CopperBottom;
            e.points={{5,5},{15,5},{15,15},{5,15}};e.width=.2;e.hatched=true;e.clearance=0;g.elements<<e;
            const auto grid=hatchLines(e);const double lw=hatchLineWidth(e);
            auto fromCopper=[&](QPointF q){double d=1e9;for(const auto &l:grid)d=std::min(d,segmentDistance(q,l)-lw/2);
                for(int k=0;k<4;k++)d=std::min(d,segmentDistance(q,QLineF(e.points[k],e.points[(k+1)%4]))-e.width/2);return d;};
            int inside=0;const auto jobs=millingJobs(g,MillingSettings());
            for(const auto &p:jobs[0].paths){
                const QRectF r=p.boundingRect();if(r.left()<=5||r.top()<=5||r.right()>=15||r.bottom()>=15)continue;inside++;
                for(auto q:p)require(std::abs(fromCopper(q)-.1)<.005,"paths in the gaps of a hatched area beside a plane at clearance 0");}
            require(inside==400,"a path in each gap of a hatched area beside a plane at clearance 0");}
        // Hatched areas among other copper give the same paths as plain path booleans: pads in the gaps, a thin spike of
        // the outline reaching into a cell, a thermal pad under the plane, overlapping areas of different pitch, a
        // diagonal track, no border beside a plane at clearance 0, a pitch off whole millimetres, only some elements,
        // further paths where the gaps have closed, a thin spike along grid crossings (its lines stay whole) and a
        // border wider than the lines just beside a line.
        for(int variant=0;variant<11;variant++){
            Board v=newBoard(QStringLiteral("Vergleich"),16,13);MillingSettings vs;QList<int> chosen;
            auto area=[&](const QPolygonF &outline,double width,double pitch){auto e=newElement(ElementType::Area);e.points=outline;e.width=width;e.hatched=true;
                e.hatchAuto=pitch<=0;e.hatchPitch=pitch;e.clearance=.3;v.elements<<e;};
            auto pad=[&](ElementType type,QPointF at,double size,double drill){auto e=newElement(type);e.layer=CopperBottom;e.pos=at;e.size=size;e.size2=drill;e.clearance=.3;updateOutline(e);v.elements<<e;};
            const QPolygonF box{{2,2},{13,2},{13,11},{2,11}};
            switch(variant){
            case 0:area(box,.2,1);pad(ElementType::SmdPad,{4.5,4.5},.3,.3);pad(ElementType::Pad,{7.5,6.5},.6,0);pad(ElementType::Pad,{10.5,8.5},.7,.3);break;
            case 1:area({{2,2},{13,2},{13,11},{7.3,11},{7.3,5.5},{7.3,11},{2,11}},.2,1);break;
            case 2:area(box,.2,1);pad(ElementType::Pad,{6.5,5.5},1,.4);v.elements.last().thermal=true;v.groundPlane[CopperBottom]=true;break;
            case 3:area({{2,2},{10,2},{10,8},{2,8}},.2,1);area({{6,5},{14,5},{14,11},{6,11}},.25,1.3);break;
            case 4:{area(box,.2,1);auto t=newElement(ElementType::Track);t.points={{2.5,10.5},{12.5,3}};t.width=.3;v.elements<<t;break;}
            case 5:area(box,0,1);v.elements.last().clearance=0;v.groundPlane[CopperBottom]=true;break;
            case 6:area({{2.15,2.4},{13.05,2.4},{13.05,10.9},{2.15,10.9}},.25,1.3);break;
            case 7:area(box,.2,1);pad(ElementType::Pad,{4.5,4.5},.6,0);pad(ElementType::Pad,{9.5,7.5},.6,0);v.groundPlane[CopperBottom]=true;vs.onlySelected=true;chosen={0,2};break;
            case 8:area({{2,2},{9,2},{9,7},{2,7}},.2,0);vs.passes=4;break;
            case 9:area({{2,2},{13,2},{13,11.5},{2.5,11.5},{7.4,6.6},{2.5,11.5},{2,11.5}},.2,1);break;
            default:area({{2.85,2.85},{13.15,2.85},{13.15,11.15},{2.85,11.15}},1,1);break;
            }
            const auto jobs=millingJobs(v,vs,chosen);
            require(samePaths(jobs[0].paths,isolationReference(v,vs,chosen),.01),QStringLiteral("paths of hatched areas as path booleans give them, case %1").arg(variant));
        }
        // A large grid at the smallest pitch: a path round the area and one in each of its 6400 gaps.
        {Board g=newBoard(QStringLiteral("Raster"),50,50);auto e=newElement(ElementType::Area);e.layer=CopperBottom;e.points={{5,5},{45,5},{45,45},{5,45}};
            e.width=.2;e.hatched=true;g.elements<<e;require(millingJobs(g,MillingSettings())[0].paths.size()==6401,"a path in each gap of a grid at 0.5 mm");}
        // The order of paths, here of the outline: the same as a plain search over all paths gives. Paths on whole
        // millimetres, so that distances tie, open ones both ways and closed ones, far from the origin.
        {Board o=newBoard(QStringLiteral("Reihenfolge"),60,40);o.origin={-300,500};
            auto add=[&](const QPolygonF &points){auto e=newElement(ElementType::Track);e.layer=Outline;e.points=points;e.width=.1;o.elements<<e;};
            for(int y=0;y<40;y+=2)for(int x=0;x<60;x+=3){
                const double a=x,c=y;
                switch((x*7+y*3)%4){
                case 0:add({{a,c},{a+1,c}});break;
                case 1:add({{a,c},{a+1,c},{a+1,c+1},{a,c+1},{a,c}});break;
                case 2:add({{a+2,c+1},{a+1,c+1},{a,c}});break;
                default:add({{a,c+1},{a+1,c},{a+2,c+1},{a,c+1}});break;
                }}
            QList<QPolygonF> input;for(const auto &e:o.elements)input.append(e.points);
            MillingSettings os;os.bottom=false;os.contourSide=2;const auto jobs=millingJobs(o,os);
            // From the machine's zero: the origin mirrored, as the bottom side is milled.
            require(jobs.size()==1&&jobs[0].kind==MillingJobKind::Contour&&jobs[0].paths==plainOrder(input,QPointF(o.width-o.origin.x(),o.origin.y())),"paths in the order of a plain nearest search");}
        // The dialog lists the jobs; dragging changes the order of the jobs written.
        s=MillingSettings();s.drillSide=2;s.drillMode=2;MillingDialog dialog(m,{},s);QStringList names;for(const auto &j:millingJobs(m,s,{},false))names.append(j.name);
        require(dialog.jobNames()==names&&names.size()==3,"job list of the dialog");
        auto *list=dialog.findChild<QListWidget*>("millingJobs");list->insertItem(0,list->takeItem(2));
        const auto ordered=dialog.jobs();require(ordered.size()==3&&ordered[0].name==names[2]&&ordered[1].kind==MillingJobKind::Isolation&&!ordered[1].paths.isEmpty(),"jobs in the order of the list");}
    // --- Gerber import: the board's own files read back draw the same layers; pads, the via, the thermal pad and the
    // ground plane come back as such.
    {GerberImport in;for(int layer:{CopperBottom,CopperTop,SilkTop,Outline})in.layers[layer]=readGerber(gerber(b,layer,{}));
        const QByteArray drills=excellon(b,{});in.drills=readExcellon(drills,drillFormat(drills));
        int layersBegun=0;const Board back=importGerber(in,QStringLiteral("Import"),[&](int,int total){layersBegun++;require(total==int(in.layers.size()),"progress over the layers");});
        require(layersBegun==int(in.layers.size()),"each layer a step of the progress");
        require(near(back.width,40,1e-3)&&near(back.height,30,1e-3)&&back.origin==b.origin,"working area and origin from the files");
        for(int layer:{CopperBottom,CopperTop,SilkTop,Outline})same(reference(back,layer,window),reference(b,layer,window),QStringLiteral("imported layer %1").arg(layer));
        int pads=0,vias=0,thermals=0;for(const auto &e:back.elements)if(e.type==ElementType::Pad&&e.size2>0){pads++;vias+=e.via;thermals+=e.thermal;}
        require(pads==5&&vias==1&&thermals==1&&back.groundPlane[CopperBottom]&&!back.groundPlane[CopperTop],"pads, via, thermal pad and ground plane recognised");}
    // --- The Gerber reader on files written by hand: older forms, macros, arcs, regions, repeats, polarity, errors.
    {auto area=[](const QPainterPath &path){double a=0;for(auto c:QTransform::fromScale(1000,1000).map(path).simplified().toFillPolygons()){double s=0;for(qsizetype i=0;i+1<c.size();i++)s+=c[i].x()*c[i+1].y()-c[i+1].x()*c[i].y();a+=std::abs(s)/2e6;}return a;};
        // Inches with trailing zeros left out, combined commands, a coordinate kept from before.
        const auto inch=readGerber("%FSTAX24Y24*%\n%MOIN*%\n%ADD10C,0.010*%\nG54D10*\nX01Y01D02*\nG01X02D01*\nM02*\n");
        require(inch.objects.size()==1&&inch.objects[0].kind==GerberObject::Draw&&near(inch.objects[0].from.x(),25.4)&&near(inch.objects[0].to.x(),50.8)
                &&near(inch.objects[0].to.y(),25.4)&&near(inch.apertures[10].parameters[0],.254),"inches, trailing zeros, modal coordinates");
        // A macro with a variable, arithmetic and a hole.
        const auto macro=readGerber("%FSLAX36Y36*%%MOMM*%\n%AMBOX*\n0 a rectangle with a hole*\n$3=$1x0.5*\n21,1,$1,$2,0,0,0*\n1,0,$3,0,0*%\n%ADD20BOX,2X1*%\nD20*\nX5000000Y5000000D03*\nM02*\n");
        require(macro.objects.size()==1&&near(area(macro.shape(macro.objects[0])),2-M_PI/4,1e-3)&&near(macro.shape(macro.objects[0]).boundingRect().center().x(),5),"macro with variables and an opening");
        // Arcs: single quadrant finds the centre from unsigned offsets, multi quadrant draws a full circle.
        const auto arcs=readGerber("%FSLAX24Y24*%%MOMM*%%ADD10C,0.1*%D10*G74*X10000Y0D02*G03X0Y10000I10000J0D01*G75*X10000Y0D02*G03X10000Y0I-10000J0D01*M02*");
        require(arcs.objects.size()==2&&arcs.objects[0].kind==GerberObject::Arc&&near(arcs.objects[0].centre,QPointF(0,0))&&near(arcs.objects[1].centre,QPointF(0,0))
                &&near(arcs.shape(arcs.objects[1]).boundingRect().width(),2.1,1e-3),"single and multi quadrant arcs");
        // A region with an arc, repeated 2 × 3 times; clear polarity.
        const auto rep=readGerber("%FSLAX24Y24*%%MOMM*%%SRX2Y3I5.0J4.0*%G36*X0Y0D02*G01X20000Y0D01*G75*G03X0Y0I-10000J0D01*G37*%SR*%%LPC*%%ADD11R,1X1*%D11*X0Y0D03*M02*");
        require(rep.objects.size()==7&&rep.objects[0].kind==GerberObject::Region&&near(area(rep.shape(rep.objects[0])),M_PI/2,5e-3)&&!rep.objects.last().dark
                &&near(rep.shape(rep.objects[5]).boundingRect().center(),QPointF(6,8.5)),"region with an arc, step and repeat, clear polarity");
        auto broken=[](const QByteArray &data){try{readGerber(data);}catch(const FormatError &){return true;}return false;};
        require(broken("%MOMM*%%ADD10C,1*%D10*X1Y1D03*M02*")&&broken("%FSLAX24Y24*%%MOMM*%D12*X1Y1D03*M02*")&&broken("%FSLAX24Y24*%%MOMM*%G36*X0Y0D02*M02*"),"broken Gerber files refused");
        // Old drill files: tools with feeds, leading zeros left out.
        const QByteArray old="M48\nINCH,TZ\nT01F200S65C0.0315\n%\nT01\nX10000Y5000\nY15000\nM30\n";
        const auto holes=readExcellon(old,drillFormat(old));
        require(holes.size()==2&&near(holes[0].at,QPointF(25.4,12.7))&&near(holes[1].at,QPointF(25.4,38.1))&&near(holes[0].diameter,.8001,1e-3),"old drill file");
        // The other two ways of writing numbers: all digits, or a decimal point (a number without one counts whole units).
        const QByteArray full="M48\nMETRIC\nT01C0.8\n%\nT01\nX012500Y005000\nM30\n";DrillFormat all;all.metric=true;all.integerDigits=3;all.decimalDigits=3;all.allDigits=true;
        require(near(readExcellon(full,all)[0].at,QPointF(12.5,5)),"all digits written");
        const QByteArray point="M48\nMETRIC\nT01C0.8\n%\nT01\nX12Y5.5\nM30\n";DrillFormat decimal;decimal.metric=true;decimal.decimalPoint=true;
        require(near(readExcellon(point,decimal)[0].at,QPointF(12,5.5)),"numbers with a decimal point, whole units without one");}
    // --- The import dialog reads the chosen files; the editor adds the board as a new one or onto the active one.
    {QTemporaryDir dir;QString error;const auto written=writeFabricationFiles(b,dir.path(),QStringLiteral("platine"),{},{},&error);require(!written.isEmpty(),"files to import");
        GerberImportDialog dialog;
        for(int layer:{CopperBottom,CopperTop,SilkTop,Outline})require(dialog.setFile(layer,dir.filePath("platine"+gerberSuffix(layer))),"Gerber file read");
        require(dialog.setDrillFile(dir.filePath("platine.drl"))&&dialog.import().drills.size()==5&&dialog.import().layers.size()==4,"drill file read");
        require(!dialog.setFile(Inner1,dir.filePath("platine.drl"))&&!dialog.state(Inner1).isEmpty()&&!dialog.import().layers.contains(Inner1),"a drill file is no Gerber file");
        require(dialog.boardName()=="platine"&&dialog.newBoard(),"name and target");
        // Files taken out again by their buttons.
        dialog.findChild<QPushButton*>(QStringLiteral("remove-%1").arg(SilkTop))->click();require(dialog.import().layers.size()==3&&!dialog.import().layers.contains(SilkTop),"a layer file removed");
        dialog.findChild<QPushButton*>("removeDrills")->click();require(dialog.import().drills.isEmpty(),"the drill file removed");
        require(dialog.setFile(SilkTop,dir.filePath("platine"+gerberSuffix(SilkTop)))&&dialog.setDrillFile(dir.filePath("platine.drl")),"and chosen again");
        Editor editor;Document d;d.boards={newBoard("leer",50,40)};editor.setDocument(d);
        const Board imported=importGerber(dialog.import(),dialog.boardName());
        editor.importBoard(imported,true);require(editor.document().boards.size()==2&&editor.document().activeBoard==1&&editor.document().board().elements==imported.elements,"imported as a new board");
        editor.undo();require(editor.document().boards.size()==1,"import undone");
        editor.importBoard(imported,false);const auto &on=editor.document().board();
        require(on.elements.size()==imported.elements.size()&&on.groundPlane[CopperBottom]&&near(on.elements[0].pos-on.origin,imported.elements[0].pos-imported.origin),"imported onto the active board at its origin");}
    // --- All fabrication files at once, as the command line of the demo writes them.
    {QTemporaryDir dir;QString error;const auto written=writeFabricationFiles(b,dir.path(),QStringLiteral("platine"),{},{},&error);
        QStringList expected;for(int o:gerberOutputs())if(gerberOutputUsed(b,o))expected.append(dir.filePath("platine"+gerberSuffix(o)));expected.append(dir.filePath("platine.drl"));
        require(written==expected,"Gerber files of the outputs in use and the drill file");
        QFile drill(dir.filePath("platine.drl"));require(drill.open(QIODevice::ReadOnly)&&drill.readAll()==excellon(b,{}),"the drill file");
        require(writeFabricationFiles(b,dir.filePath("missing"),QStringLiteral("x"),{},{},&error).isEmpty()&&!error.isEmpty(),"the folder must exist");
        require(writeFabricationFiles(newBoard("leer"),dir.path(),QStringLiteral("x"),{},{},&error).isEmpty(),"nothing to write");}
    // --- The Gerber dialog writes a file per chosen output.
    {QTemporaryDir dir;require(dir.isValid(),"temporary folder");
        GerberDialog dialog(b,QStringLiteral("platine"),dir.path(),GerberSettings{});const auto chosen=dialog.chosen();
        require(chosen.contains(CopperBottom)&&chosen.contains(CopperTop)&&chosen.contains(SilkTop)&&chosen.contains(Outline)&&chosen.contains(SolderMaskBottom)
                &&!chosen.contains(SilkBottom)&&!chosen.contains(Inner1),"outputs in use are chosen");
        require(dialog.fileName(CopperBottom)=="platine.GBL","file name of an output");
        require(dialog.writeFiles()==chosen.size()&&dialog.log().size()==chosen.size(),"one file per output");
        QFile f(dir.filePath(QStringLiteral("platine.GBL")));require(f.open(QIODevice::ReadOnly)&&f.readAll()==gerber(b,CopperBottom,dialog.settings()),"the file holds its layer");
        dialog.choose(CopperBottom,false);require(!dialog.chosen().contains(CopperBottom),"an output left out");
        {auto *suffix=dialog.findChildren<QLineEdit*>().value(1);if(suffix){suffix->setText(QStringLiteral(".xyz"));}
            dialog.findChild<QPushButton*>("standardSuffixes")->click();bool usual=true;for(auto *e:dialog.findChildren<QLineEdit*>())usual&=!e->text().contains(".xyz");
            require(usual,"the usual endings back");}
        DrillDialog drill(b,QStringLiteral("platine"),DrillSettings{});require(drill.suggestedFile()=="platine.drl"&&drill.settings().integerDigits==2,"drill dialog");}

    // --- Areas whose outline crosses itself fill by the even-odd rule everywhere, as the reference fills them on the
    // screen, in its ground plane and in its test: a star leaves its middle open, a bow tie fills both halves, an outline
    // running twice round the same way leaves a hole. Copper, plane, test, design rule check and Gerber agree.
    {Board x=newBoard(QStringLiteral("Stern"),40,30);x.groundPlane[CopperBottom]=true;
        QPolygonF star;for(int i=0;i<5;i++){const double a=M_PI/2+i*4*M_PI/5;star<<QPointF(10+8*std::cos(a),12-8*std::sin(a));}
        const QPolygonF bow{{22,4},{36,12},{36,4},{22,12}},loop{{22,16},{38,16},{38,28},{22,28},{22,16},{26,20},{34,20},{34,24},{26,24},{26,20}};
        for(const auto &outline:{star,bow,loop}){auto e=newElement(ElementType::Area);e.layer=CopperBottom;e.points=outline;e.width=.2;e.clearance=.4;x.elements<<e;}
        auto pad=newElement(ElementType::Pad);pad.pos={10,12};pad.size=1;pad.size2=.4;pad.clearance=.4;updateOutline(pad);x.elements<<pad;
        require(crossesItself(star)&&crossesItself(bow)&&crossesItself(loop)&&!crossesItself({{0,0},{1,0},{1,1},{0,1}}),"outlines that cross or touch themselves");
        const auto starCopper=copperShape(x.elements[0]),bowCopper=copperShape(x.elements[1]),loopCopper=copperShape(x.elements[2]);
        require(starCopper.contains(QPointF(10,5.5))&&!starCopper.contains(QPointF(10,13.4)),"a star's points are copper, its middle is not");
        require(bowCopper.contains(QPointF(24,8))&&bowCopper.contains(QPointF(34,8))&&!bowCopper.contains(QPointF(29,5)),"a bow tie fills both halves");
        require(loopCopper.contains(QPointF(23,22))&&!loopCopper.contains(QPointF(30,22)),"an outline running round twice leaves a hole");
        {QPainterPath winding=loopCopper,oddEven=loopCopper;winding.setFillRule(Qt::WindingFill);oddEven.setFillRule(Qt::OddEvenFill);
            require(!winding.contains(QPointF(30,22))&&!oddEven.contains(QPointF(30,22))&&winding.contains(QPointF(23,22))&&oddEven.contains(QPointF(23,22)),
                    "holes run the other way round: either fill rule fills the copper the same");}
        const auto plane=groundPlane(x,CopperBottom);
        require(plane.contains(QPointF(10,13.4))&&plane.contains(QPointF(30,22))&&!plane.contains(QPointF(10,5.5))&&!plane.contains(QPointF(10,12)),
                "the ground plane flows into the star's middle and the hole, round the pad there");
        const auto joined=connections(x,false);require(joined[3]!=joined[0],"a pad in the star's middle is not connected to it");
        require(connectedAt(x,{10,13.4},CopperBottom,false).isEmpty()&&connectedAt(x,{10,5.5},CopperBottom,false)==QList<int>{0},"the test finds nothing in the middle, the star at a point");
        require(checkDesign(x,Rules{}).isEmpty(),"no findings of the design rule check");
        const QByteArray data=gerber(x,CopperBottom,{});GerberReader read(data,window);
        same(read.image,reference(x,CopperBottom,window),QStringLiteral("areas crossing themselves"));
        // Each region a contour that does not cross itself, as Gerber wants it: the star and the bow tie as their parts,
        // the outline round a hole as bands.
        int contours=0;bool crossing=false;QPolygonF contour;bool inRegion=false;
        static const QRegularExpression point(QStringLiteral("^X(-?\\d+)Y(-?\\d+)D0[12]\\*$"));
        for(const auto &line:data.split('\n')){
            if(line=="G36*"){inRegion=true;contour.clear();continue;}
            if(line=="G37*"){inRegion=false;contours++;crossing|=crossesItself(contour);continue;}
            const auto m=point.match(QString::fromLatin1(line));if(inRegion&&m.hasMatch())contour<<QPointF(m.captured(1).toDouble()/1e6,m.captured(2).toDouble()/1e6);
        }
        require(contours>=10&&!crossing,"each region a contour that does not cross itself");
        Board without=x;without.groundPlane[CopperBottom]=false;GerberReader plain(gerber(without,CopperBottom,{}),window);
        same(plain.image,reference(without,CopperBottom,window),QStringLiteral("areas crossing themselves without a plane"));}
    return 0;
}
