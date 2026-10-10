#include "fabrication.h"
#include "copper.h"
#include "font.h"
#include "language.h"
#include <QDir>
#include <QPainterPath>
#include <QSaveFile>
#include <QStringList>
#include <QTransform>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <map>

namespace openloch::pcb {
namespace {
QPointF onCircle(QPointF centre,double radius,double degrees){
    const double a=qDegreesToRadians(degrees);return centre+QPointF(radius*std::cos(a),-radius*std::sin(a));
}
// A decimal number without needless zeros ("0.8", "1.27").
QByteArray decimal(double v,int decimals=6){
    QByteArray s=QByteArray::number(v,'f',decimals);if(s.contains('.')){while(s.endsWith('0'))s.chop(1);if(s.endsWith('.'))s.chop(1);}
    return s=="-0"?QByteArray("0"):s;
}
double signedArea(const QPolygonF &p){
    double a=0;for(qsizetype i=0;i<p.size();i++){const QPointF u=p[i],v=p[(i+1)%p.size()];a+=u.x()*v.y()-v.x()*u.y();}return a/2;
}
// A convex polygon with every edge moved outwards by `distance` (inwards when negative), its corners kept sharp.
// Empty when it vanishes.
QPolygonF offsetConvex(const QPolygonF &polygon,double distance){
    const QPolygonF p=withoutRedundantNodes(polygon,true);const qsizetype n=p.size();if(n<3)return {};
    if(distance==0)return p;
    const double area=signedArea(p),sign=area>0?1:-1;
    QList<QPointF> normals;
    for(qsizetype i=0;i<n;i++){const QPointF e=p[(i+1)%n]-p[i];const double l=std::hypot(e.x(),e.y());normals.append(QPointF(e.y(),-e.x())*sign/l);}
    QPolygonF out;
    for(qsizetype i=0;i<n;i++){
        const QPointF a=normals[(i+n-1)%n],b=normals[i];const double d=1+a.x()*b.x()+a.y()*b.y();if(d<1e-9)return {};
        out.append(p[i]+(a+b)*distance/d);
    }
    const double grown=signedArea(out);return grown*area>0&&std::abs(grown)>1e-12?out:QPolygonF();
}
// An upright rectangle (four corners with edges along the axes).
bool upright(const QPolygonF &p){
    if(p.size()!=4)return false;auto same=[](double a,double b){return std::abs(a-b)<1e-7;};
    return (same(p[0].x(),p[1].x())&&same(p[1].y(),p[2].y())&&same(p[2].x(),p[3].x())&&same(p[3].y(),p[0].y()))||
           (same(p[0].y(),p[1].y())&&same(p[1].x(),p[2].x())&&same(p[2].y(),p[3].y())&&same(p[3].x(),p[0].x()));
}
// Text for a comment in a fabrication file: printable ASCII only, anything else (and Gerber's `*` and `%`) as `_`.
QByteArray commentText(const QString &text){
    QByteArray out;for(QChar ch:text)out+=ch.unicode()>=32&&ch.unicode()<127&&ch!='*'&&ch!='%'?char(ch.unicode()):'_';return out;
}
// What an outline crossing itself encloses by the even-odd rule, as pieces without holes for Gerber regions (whose
// contours must not cross): between the heights of all nodes and crossings the edges run side by side, and every second
// gap between them is inside. The pieces of one pair of edges in successive bands join into one.
QList<QPolygonF> evenOddPieces(const QPolygonF &outline){
    QPolygonF p=outline;if(p.size()>1&&p.first()==p.last())p.removeLast();const qsizetype n=p.size();if(n<3)return {};
    struct Edge {QPointF a,b;};QList<Edge> edges;QList<double> heights;
    for(qsizetype i=0;i<n;i++){QPointF a=p[i],b=p[(i+1)%n];heights.append(a.y());if(a.y()==b.y())continue;if(a.y()>b.y())std::swap(a,b);edges.append({a,b});}
    // Where edges cross: there the order of the edges changes.
    for(qsizetype i=0;i<edges.size();i++)for(qsizetype j=i+1;j<edges.size();j++){
        const QPointF a=edges[i].a,r=edges[i].b-a,c=edges[j].a,s=edges[j].b-c;const double d=r.x()*s.y()-r.y()*s.x();if(std::abs(d)<1e-15)continue;
        const double t=((c.x()-a.x())*s.y()-(c.y()-a.y())*s.x())/d,u=((c.x()-a.x())*r.y()-(c.y()-a.y())*r.x())/d;
        if(t>0&&t<1&&u>0&&u<1)heights.append(a.y()+t*r.y());
    }
    std::sort(heights.begin(),heights.end());heights.erase(std::unique(heights.begin(),heights.end(),[](double x,double y){return std::abs(x-y)<1e-9;}),heights.end());
    auto xAt=[&](int e,double y){const Edge &g=edges[e];return g.a.x()+(y-g.a.y())/(g.b.y()-g.a.y())*(g.b.x()-g.a.x());};
    struct Run {int left,right;QPolygonF down,up;};QList<Run> open,done;
    for(qsizetype k=0;k+1<heights.size();k++){
        const double y0=heights[k],y1=heights[k+1],middle=(y0+y1)/2;
        QList<std::pair<double,int>> across;for(int e=0;e<edges.size();e++)if(edges[e].a.y()<middle&&middle<edges[e].b.y())across.append({xAt(e,middle),e});
        std::sort(across.begin(),across.end());QList<Run> next;
        for(qsizetype m=0;m+1<across.size();m+=2){
            const int l=across[m].second,r=across[m+1].second;
            auto same=std::find_if(open.begin(),open.end(),[&](const Run &x){return x.left==l&&x.right==r;});
            if(same!=open.end()){Run run=*same;open.erase(same);run.down<<QPointF(xAt(l,y1),y1);run.up<<QPointF(xAt(r,y1),y1);next.append(run);}
            else next.append({l,r,{QPointF(xAt(l,y0),y0),QPointF(xAt(l,y1),y1)},{QPointF(xAt(r,y0),y0),QPointF(xAt(r,y1),y1)}});
        }
        done.append(open);open=next;
    }
    done.append(open);
    QList<QPolygonF> out;
    for(const auto &run:done){
        QPolygonF q=run.down;for(qsizetype i=run.up.size()-1;i>=0;i--)q<<run.up[i];
        QPolygonF clean;for(auto x:q)if(clean.isEmpty()||QLineF(clean.last(),x).length()>1e-9)clean<<x;
        if(clean.size()>1&&QLineF(clean.first(),clean.last()).length()<=1e-9)clean.removeLast();
        if(clean.size()>=3)out.append(clean);
    }
    return out;
}
// Polygons of a path, its curves flattened to a thousandth of a millimetre.
QList<QPolygonF> polygons(const QPainterPath &path){
    if(path.isEmpty())return {};
    QList<QPolygonF> out;const QTransform back=QTransform::fromScale(.001,.001);
    for(const auto &p:path.toSubpathPolygons(QTransform::fromScale(1000,1000)))out.append(back.map(p));
    return out;
}

// Writes the commands of one Gerber file; the apertures are collected and defined in the header.
class GerberWriter {
public:
    explicit GerberWriter(const OutputFrame &frame):frame(frame){}
    void setDark(bool on){if(on!=dark){body+=on?"%LPD*%\n":"%LPC*%\n";dark=on;}}
    void flashCircle(QPointF at,double diameter){
        if(diameter<=0)return;use("C,"+decimal(diameter));body+=xy(at)+"D03*\n";
    }
    void flashRectangle(QPointF at,double width,double height){
        if(width<=0||height<=0)return;use("R,"+decimal(width)+"X"+decimal(height));body+=xy(at)+"D03*\n";
    }
    void line(const QPolygonF &points,double width,bool closed=false){
        if(width<=0||points.isEmpty())return;
        if(points.size()==1||(points.boundingRect().size()==QSizeF(0,0))){flashCircle(points[0],width);return;}
        use("C,"+decimal(width));linear();body+=xy(points[0])+"D02*\n";
        for(qsizetype i=1;i<points.size();i++)body+=xy(points[i])+"D01*\n";
        if(closed)body+=xy(points[0])+"D01*\n";
    }
    // An arc of the circle about `centre` from `start` counter-clockwise over `span` degrees (360: the full circle).
    void arc(QPointF centre,double radius,double start,double span,double width){
        if(width<=0)return;if(radius<=0){flashCircle(centre,width);return;}
        use("C,"+decimal(width));linear();const QPointF a=onCircle(centre,radius,start);body+=xy(a)+"D02*\n";
        arcTo(centre,a,onCircle(centre,radius,start+span),span>=360);
    }
    void region(const QPolygonF &polygon){
        QPolygonF p=polygon;if(p.size()>1&&p.first()==p.last())p.removeLast();if(p.size()<3||std::abs(signedArea(p))<1e-12)return;
        linear();body+="G36*\n"+xy(p[0])+"D02*\n";for(qsizetype i=1;i<p.size();i++)body+=xy(p[i])+"D01*\n";body+=xy(p[0])+"D01*\nG37*\n";
    }
    // A sector of a circle: filled as a region, or its outline drawn `width` wide.
    void sector(QPointF centre,double radius,double start,double span,double width=0){
        const QPointF a=onCircle(centre,radius,start),b=onCircle(centre,radius,start+span);
        if(width>0)use("C,"+decimal(width));linear();if(width<=0)body+="G36*\n";
        body+=xy(centre)+"D02*\n"+xy(a)+"D01*\n";arcTo(centre,a,b,false);linear();body+=xy(centre)+"D01*\n";
        if(width<=0)body+="G37*\n";
    }
    QByteArray finish(const QStringList &comments) const{
        QByteArray out;
        for(const auto &c:comments)out+="G04 "+commentText(c)+"*\n";
        out+="%FSLAX46Y46*%\n%MOMM*%\n%LPD*%\nG75*\n";
        for(qsizetype i=0;i<definitions.size();i++)out+="%ADD"+QByteArray::number(10+i)+definitions[i]+"*%\n";
        return out+body+"M02*\n";
    }
private:
    OutputFrame frame;
    QList<QByteArray> definitions;      // the aperture with code D10 + index
    QByteArray body;
    qsizetype aperture=-1;
    int mode=0;                         // 1 linear, 2 clockwise, 3 counter-clockwise
    bool dark=true;
    static qint64 units(double mm){return qint64(std::llround(mm*1e6));}
    QByteArray xy(QPointF p) const{const QPointF q=frame.map(p);return "X"+QByteArray::number(units(q.x()))+"Y"+QByteArray::number(units(q.y()));}
    void use(const QByteArray &definition){
        qsizetype i=definitions.indexOf(definition);if(i<0){definitions.append(definition);i=definitions.size()-1;}
        if(i!=aperture){body+="D"+QByteArray::number(10+i)+"*\n";aperture=i;}
    }
    void linear(){if(mode!=1){body+="G01*\n";mode=1;}}
    // From `a` (the current point) counter-clockwise on the board to `b`; a full circle when `full`.
    void arcTo(QPointF centre,QPointF a,QPointF b,bool full){
        const QPointF c=frame.map(centre),s=frame.map(a),e=frame.map(b);
        const qint64 sx=units(s.x()),sy=units(s.y()),ex=units(e.x()),ey=units(e.y());
        // An arc too short to show stays a straight piece: equal ends would mean a full circle.
        if(!full&&sx==ex&&sy==ey)return;
        const int m=frame.reverses()?2:3;if(mode!=m){body+=m==2?"G02*\n":"G03*\n";mode=m;}
        body+=(full?xy(a):xy(b))+"I"+QByteArray::number(units(c.x())-sx)+"J"+QByteArray::number(units(c.y())-sy)+"D01*\n";
    }
};

// Draws an element grown by `grow` on every side: rounded like a clearance, or with the sharp corners of a mask
// opening (`sharp`; only polygonal pads differ). Lines without width are drawn `thinnest` wide, left out when that
// is 0 and nothing grows. Hatched areas are drawn as their grid unless they grow or `solid`.
void draw(GerberWriter &w,const Element &e,double grow,bool sharp,double thinnest,bool solid=false){
    auto lineWidth=[&](double width){return (width>0?width:thinnest)+2*grow;};
    auto polygon=[&](const QPolygonF &outline){
        if(sharp||grow==0){const QPolygonF p=grow==0?outline:offsetConvex(outline,grow);
            if(upright(p)){const QRectF r=p.boundingRect();w.flashRectangle(r.center(),r.width(),r.height());}else w.region(p);return;}
        if(upright(outline)){const QRectF r=outline.boundingRect();w.flashRectangle(r.center(),r.width(),r.height());}else w.region(outline);
        if(grow>0)w.line(outline,2*grow,true);
    };
    switch(e.type){
    case ElementType::Pad:
        if(e.points.size()==2&&e.shape==PadShape::Round)w.flashCircle(e.pos,e.size+2*grow);
        else if(e.points.size()==2)w.line(e.points,e.size+2*grow);
        else polygon(e.points);
        break;
    case ElementType::SmdPad:polygon(e.points);break;
    case ElementType::Track:{
        w.line(e.points,lineWidth(e.width));
        if(e.points.size()<2||e.width<=0)break;
        for(int end=0;end<2;end++){
            if(!(end?e.flatEnd:e.flatStart))continue;
            const QPointF at=end?e.points.last():e.points.first(),from=end?e.points[e.points.size()-2]:e.points[1];
            if(sharp||grow==0)w.region(squareEnd(at,from,e.width+2*grow));
            else{const QPolygonF q=squareEnd(at,from,e.width);w.region(q);w.line(q,2*grow,true);}
        }
        break;}
    case ElementType::Area:{
        if(e.hatched&&grow==0&&!solid){
            // The lines of the grid as strokes of a round aperture half the pitch wide, then the border.
            const double width=hatchLineWidth(e);for(const auto &l:hatchLines(e))w.line({l.p1(),l.p2()},width);
            if(e.width>0)w.line(e.points,e.width,true);
            break;
        }
        // Filled by the even-odd rule. An outline crossing itself goes out as the contours of what it encloses when none
        // of them is a hole (outer ones run the positive way round), else as bands without holes.
        if(crossesItself(e.points)){
            const auto rings=evenOddArea(e.points).toSubpathPolygons();
            if(std::all_of(rings.begin(),rings.end(),[](const QPolygonF &r){return signedArea(r)>0;}))for(const auto &r:rings)w.region(r);
            else for(const auto &piece:evenOddPieces(e.points))w.region(piece);
        }else w.region(e.points);
        const double width=e.width+2*grow;if(width>0)w.line(e.points,width,true);
        break;}
    case ElementType::Circle:{
        const bool full=std::abs(std::fmod(std::fmod(e.stop-e.start,360.0)+360,360.0))<1e-9;
        const double span=full?360:std::fmod(std::fmod(e.stop-e.start,360.0)+360,360.0);
        if(e.filled){
            const double outer=e.size+e.width/2;
            if(full)w.flashCircle(e.pos,2*outer+2*grow);
            else{w.sector(e.pos,outer,e.start,span);if(grow>0)w.sector(e.pos,outer,e.start,span,2*grow);}
        }else w.arc(e.pos,e.size,e.start,span,lineWidth(e.width));
        break;}
    case ElementType::Text:{
        const auto strokes=e.strokes.isEmpty()?textStrokes(e.text,e.pos,e.size,e.style,e.thickness,e.rotation,e.mirrored):e.strokes;
        const double width=lineWidth(e.strokes.isEmpty()?textStrokeWidth(e.size,e.thickness):e.strokeWidth);
        for(const auto &s:strokes)w.line(s,width);
        break;}
    }
}
bool shownText(const Element &e){return e.type!=ElementType::Text||e.visible||e.role==TextRole::Plain;}
// Whether an element's copper reaches a side for its mask opening: pads on their layer or through-plated, the others
// with copper on that layer.
bool onSide(const Element &e,int side,const Board &b){
    if(e.type==ElementType::Pad)return e.layer==side||(e.via&&e.size2>0);
    return !copperOn(e,side,b).isEmpty();
}
struct MaskKind {bool on;double offset;};
MaskKind maskKind(const Element &e,const GerberSettings &s){
    if(e.type==ElementType::Pad)return {s.maskPads,s.padOffset};
    if(e.type==ElementType::SmdPad)return {s.maskSmd,s.smdOffset};
    return {s.maskOther,s.otherOffset};
}
}

QPointF OutputFrame::map(QPointF p) const{
    if(mirror==1)p.setX(area.width()-p.x());else if(mirror==2)p.setY(area.height()-p.y());
    return QPointF(p.x()-origin.x(),origin.y()-p.y());
}
QPointF OutputFrame::zero() const{
    QPointF p=origin;if(mirror==1)p.setX(area.width()-p.x());else if(mirror==2)p.setY(area.height()-p.y());return p;
}
OutputFrame outputFrame(const Board &board,int mirror,bool fromOrigin){
    OutputFrame f;f.origin=fromOrigin?board.origin:QPointF();f.area=QSizeF(board.width,board.height);f.mirror=mirror;return f;
}

QList<int> gerberOutputs(){
    QList<int> out=layerOrder();out<<SolderMaskTop<<SolderMaskBottom<<PasteMaskTop<<PasteMaskBottom;return out;
}
QString gerberOutputName(int output){
    switch(output){
    case CopperTop:return ui("Kupfer oben (K1)");case SilkTop:return ui("Bestückung oben (B1)");
    case CopperBottom:return ui("Kupfer unten (K2)");case SilkBottom:return ui("Bestückung unten (B2)");
    case Inner1:return ui("Kupfer innen 1 (I1)");case Inner2:return ui("Kupfer innen 2 (I2)");case Outline:return ui("Umriss (U)");
    case SolderMaskTop:return ui("Lötstoppmaske oben");case SolderMaskBottom:return ui("Lötstoppmaske unten");
    case PasteMaskTop:return ui("SMD-Maske oben");case PasteMaskBottom:return ui("SMD-Maske unten");
    }
    return {};
}
QString gerberSuffix(int output){
    switch(output){
    case CopperTop:return QStringLiteral(".GTL");case SilkTop:return QStringLiteral(".GTO");case CopperBottom:return QStringLiteral(".GBL");
    case SilkBottom:return QStringLiteral(".GBO");case Inner1:return QStringLiteral(".G1");case Inner2:return QStringLiteral(".G2");
    case Outline:return QStringLiteral(".GKO");case SolderMaskTop:return QStringLiteral(".GTS");case SolderMaskBottom:return QStringLiteral(".GBS");
    case PasteMaskTop:return QStringLiteral(".GTP");case PasteMaskBottom:return QStringLiteral(".GBP");
    }
    return {};
}
bool gerberOutputUsed(const Board &b,int output){
    if(output>=1&&output<=layerCount){
        if(isCopper(output)){
            if(!copperLayers(b).contains(output))return false;if(b.groundPlane[output])return true;
            for(const auto &e:b.elements)if(!copperOn(e,output,b).isEmpty())return true;
            return false;
        }
        for(const auto &e:b.elements)if(e.layer==output&&!e.cutout&&shownText(e))return true;
        return false;
    }
    const int side=output==SolderMaskTop||output==PasteMaskTop?CopperTop:CopperBottom;
    for(const auto &e:b.elements){
        if(output==PasteMaskTop||output==PasteMaskBottom){if(e.type==ElementType::SmdPad&&e.layer==side)return true;}
        else if((e.solderMask&&onSide(e,side,b))||(e.type==ElementType::Area&&e.maskOnly&&e.layer==side))return true;
    }
    return false;
}
QByteArray gerber(const Board &b,int output,const GerberSettings &s){
    GerberWriter w(outputFrame(b,s.mirrored?1:0,s.fromOrigin));
    const QPolygonF area(QRectF(0,0,b.width,b.height));
    if(output>=1&&output<=layerCount&&isCopper(output)){
        const int layer=output;
        if(copperLayers(b).contains(layer)){
            if(b.groundPlane[layer]){
                // The ground plane: the working area, without the clearances, keep-out areas and drill holes, joined to
                // thermal pads by their spokes. Hatched areas keep it out of their whole outline, also at clearance 0.
                w.region(area);w.setDark(false);
                for(const auto &e:b.elements){
                    if((e.clearance>0||(e.type==ElementType::Area&&e.hatched))&&!copperOn(e,layer,b).isEmpty())draw(w,e,std::max(0.0,e.clearance),false,0,true);
                    if(e.cutout&&e.layer==layer)draw(w,e,0,false,0,true);
                    if(e.type==ElementType::Pad&&e.size2>0)w.flashCircle(e.pos,e.size2);
                }
                w.setDark(true);
                for(const auto &e:b.elements)for(const auto &p:polygons(thermalBridges(e,layer,b)))w.region(p);
            }
            for(const auto &e:b.elements)if(!copperOn(e,layer,b).isEmpty())draw(w,e,0,false,0);
            if(s.clearHoles){
                w.setDark(false);
                for(const auto &e:b.elements)if(e.type==ElementType::Pad&&e.size2>0)w.flashCircle(e.pos,s.punchMarks?std::min(punchMarkDiameter,e.size2):e.size2);
                w.setDark(true);
            }
        }
    }else if(output>=1&&output<=layerCount){
        for(const auto &e:b.elements)if(e.layer==output&&!e.cutout&&shownText(e))draw(w,e,0,false,thinnestLine);
    }else if(output>=SolderMaskTop&&output<=PasteMaskBottom){
        const bool paste=output==PasteMaskTop||output==PasteMaskBottom;
        const int side=output==SolderMaskTop||output==PasteMaskTop?CopperTop:CopperBottom;
        const bool inverted=paste?s.pasteInverted:s.maskInverted;
        if(inverted){w.region(area);w.setDark(false);}
        for(const auto &e:b.elements){
            if(paste){if(e.type==ElementType::SmdPad&&e.layer==side)draw(w,e,s.pasteOffset,true,0);continue;}
            // Areas that only open the mask open it as drawn.
            if(e.type==ElementType::Area&&e.maskOnly){if(e.layer==side)draw(w,e,0,true,0,true);continue;}
            if(!e.solderMask||!onSide(e,side,b))continue;
            const auto kind=maskKind(e,s);if(kind.on)draw(w,e,kind.offset,true,0,true);
        }
        w.setDark(true);
    }
    if(s.frame)w.line(area,thinnestLine,true);
    return w.finish({QStringLiteral("OpenLoch, board %1: %2").arg(b.name,gerberOutputName(output)),s.fromOrigin?QStringLiteral("Millimetres, origin at the board's origin, y upwards"):QStringLiteral("Millimetres, origin at the top left corner of the working area, y upwards")});
}

QList<DrillHole> drillHoles(const Board &b,int which){
    QList<DrillHole> out;
    for(const auto &e:b.elements){
        if(e.type!=ElementType::Pad||e.size2<=0)continue;
        if((which==1&&!e.via)||(which==2&&e.via))continue;
        out.append({e.pos,e.size2,e.via});
    }
    return out;
}
QByteArray excellonNumber(double v,const DrillSettings &s){
    const int integers=std::clamp(s.integerDigits,1,6),decimals=std::clamp(s.decimalDigits,0,6);
    if(s.decimalPoint)return QByteArray::number(v,'f',std::max(1,decimals));
    const qint64 n=qint64(std::llround(v*std::pow(10.0,decimals)));
    QByteArray digits=QByteArray::number(std::abs(n)).rightJustified(integers+decimals,'0');
    if(s.suppressLeadingZeros){while(digits.size()>1&&digits.startsWith('0'))digits.remove(0,1);}
    return (n<0?QByteArray("-"):QByteArray())+digits;
}
QByteArray excellon(const Board &b,const DrillSettings &s){
    const bool aboutHoles=s.fromBelow&&s.mirrorHoles;const OutputFrame frame=outputFrame(b,s.fromBelow&&!aboutHoles?1:0,s.fromOrigin);const double unit=s.metric?1:25.4;
    const int integers=std::clamp(s.integerDigits,1,6),decimals=std::clamp(s.decimalDigits,0,6);
    auto number=[&](double mm){return excellonNumber(mm/unit,s);};
    const auto all=drillHoles(b,s.holes);
    // Mirrored for HPGL machines: about the middle of the holes, their edges included, as the reference does it.
    double left=0,right=0;
    if(aboutHoles&&!all.isEmpty()){left=1e300;right=-1e300;for(const auto &h:all){const double x=frame.map(h.at).x();left=std::min(left,x-h.diameter/2);right=std::max(right,x+h.diameter/2);}}
    auto place=[&](QPointF at){QPointF p=frame.map(at);if(aboutHoles)p.setX(left+right-p.x());return p;};
    // One tool per diameter (equal to a thousandth of a millimetre), the smallest first.
    std::map<qint64,QList<QPointF>> tools;
    for(const auto &h:all)tools[std::llround(h.diameter*1000)].append(place(h.at));
    QByteArray out="M48\n";if(!s.noComments)out+=";OpenLoch drill data, board "+commentText(b.name)+"\n";
    // The digits also as readers of other programs look for them: a comment and, unless inches in the usual 2.4, a
    // pattern of zeros after the unit.
    const QByteArray pattern=QByteArray(integers,'0')+"."+QByteArray(decimals,'0');
    if(!s.decimalPoint&&!s.noComments)out+=";FILE_FORMAT="+QByteArray::number(integers)+":"+QByteArray::number(decimals)+"\n";
    out+="FMAT,2\n";
    if(s.m71)out+=s.metric?"M71":"M72";
    else{
        out+=QByteArray(s.metric?"METRIC":"INCH");
        if(!s.decimalPoint)out+=QByteArray(s.suppressLeadingZeros?",TZ":",LZ")+(s.metric||integers!=2||decimals!=4?","+pattern:QByteArray());
    }
    out+="\n";
    int tool=1;for(const auto &[size,holes]:tools){Q_UNUSED(holes);out+="T"+QByteArray::number(tool++)+"C"+QByteArray::number(size/1000.0/unit,'f',s.metric?3:4)+"\n";}
    out+=s.noG90?"%\nG05\n":"%\nG90\nG05\n";
    tool=1;
    for(auto [size,holes]:tools){
        Q_UNUSED(size);
        if(s.sorted){
            // The nearest hole next, starting at the origin.
            QList<QPointF> path;QPointF at;
            while(!holes.isEmpty()){
                qsizetype best=0;double d=1e300;
                for(qsizetype i=0;i<holes.size();i++){const double e=std::hypot(holes[i].x()-at.x(),holes[i].y()-at.y());if(e<d){d=e;best=i;}}
                at=holes.takeAt(best);path.append(at);
            }
            holes=path;
        }
        out+="T"+QByteArray::number(tool++)+"\n";
        for(const auto &h:holes)out+="X"+number(h.x())+"Y"+number(h.y())+"\n";
    }
    return out+"T0\nM30\n";
}

QList<std::pair<double,int>> holeTable(const Board &b,bool plain,bool plated){
    std::map<qint64,int> counts;
    for(const auto &h:drillHoles(b,0))if(h.plated?plated:plain)counts[std::llround(h.diameter*1000)]++;
    QList<std::pair<double,int>> out;for(const auto &[size,n]:counts)out.append({size/1000.0,n});return out;
}
QString holeList(const Board &b,bool plain,bool plated){
    // A table in fixed columns: diameter and count, then the sum.
    QStringList lines;int total=0;
    for(const auto &[diameter,n]:holeTable(b,plain,plated)){lines<<ui("Ø %1 mm: %2").arg(uiLocale().toString(diameter,'f',2).rightJustified(6),QString::number(n).rightJustified(5));total+=n;}
    lines<<QString()<<ui("Zusammen: %1").arg(total);return lines.join('\n');
}
QStringList writeFabricationFiles(const Board &b,const QString &folder,const QString &name,const GerberSettings &gs,const DrillSettings &ds,QString *error){
    const QDir dir(folder);if(!dir.exists()){if(error)*error=ui("Den Ordner gibt es nicht: %1").arg(folder);return {};}
    QList<std::pair<QString,QByteArray>> files;
    for(int o:gerberOutputs())if(gerberOutputUsed(b,o))files.append({name+gerberSuffix(o),gerber(b,o,gs)});
    if(!drillHoles(b,ds.holes).isEmpty())files.append({name+QStringLiteral(".drl"),excellon(b,ds)});
    if(files.isEmpty()){if(error)*error=ui("Auf der Platine ist nichts für die Fertigung.");return {};}
    QStringList written;
    for(const auto &[file,data]:files){
        QSaveFile f(dir.filePath(file));
        if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit()){if(error)*error=ui("Die Datei kann nicht geschrieben werden: %1").arg(dir.filePath(file));return {};}
        written.append(dir.filePath(file));
    }
    return written;
}
QString componentFieldName(int field){
    switch(field){
    case FieldDesignator:return ui("Bezeichner");case FieldValue:return ui("Wert");case FieldLayer:return ui("Layer");
    case FieldX:return ui("Position X");case FieldY:return ui("Position Y");case FieldRotation:return ui("Rotation");
    case FieldPackage:return ui("Package");case FieldComment:return ui("Kommentar");case FieldIndex:return ui("Laufende Nummer");
    }
    return {};
}
QString componentData(const Board &b,const ComponentDataSettings &s){
    const OutputFrame frame=outputFrame(b,0,s.fromOrigin);const double perMm=s.unit==1?1000/25.4:s.unit==2?1/25.4:1;
    auto quoted=[&](const QString &v){
        if(s.separator.isEmpty()||(!v.contains(s.separator)&&!v.contains('"')&&!v.contains('\n')))return v;
        return QStringLiteral("\"")+QString(v).replace('"',QStringLiteral("\"\""))+QStringLiteral("\"");
    };
    QStringList lines;
    if(s.header){QStringList names;for(int f:s.fields)names.append(quoted(componentFieldName(f)));lines.append(names.join(s.separator));}
    int number=0;   // the running number counts the lines written
    for(const auto &c:components(b)){
        const auto &id=b.elements[c.designator];const bool top=componentOnTop(b,c);
        bool smdPads=false,holes=false;for(int i:c.members){smdPads|=b.elements[i].type==ElementType::SmdPad;holes|=b.elements[i].type==ElementType::Pad;}
        const bool smd=smdPads&&!holes;
        if((smd?!s.smdParts:!s.throughHoleParts)||(top?!s.onTop:!s.onBottom)||(s.pickPlaceOnly&&!id.pickAndPlace))continue;
        number++;
        const QPointF at=frame.map(pickPlaceCentre(b,c))*perMm;
        QStringList values;
        for(int f:s.fields){
            QString v;
            switch(f){
            case FieldDesignator:v=id.text;break;
            case FieldValue:v=c.value>=0?b.elements[c.value].text:QString();break;
            case FieldLayer:v=top?s.top:s.bottom;break;
            case FieldX:case FieldY:{
                v=QString::number(f==FieldX?at.x():at.y(),'f',std::clamp(s.decimals,0,6));
                if(s.stripZeros&&v.contains('.')){while(v.endsWith('0'))v.chop(1);if(v.endsWith('.'))v.chop(1);if(v=="-0")v=QStringLiteral("0");}
                break;}
            case FieldRotation:{double r=std::fmod(id.componentRotation,360.0);if(r<0)r+=360;if(std::abs(r-360)<1e-9)r=0;
                v=(s.rotationPrefix?QStringLiteral("R"):QString())+QString::fromLatin1(decimal(r,2));break;}
            case FieldPackage:v=id.package;break;
            case FieldComment:v=id.comment;break;
            case FieldIndex:v=QString::number(number);break;
            }
            values.append(quoted(v));
        }
        lines.append(values.join(s.separator));
    }
    return lines.join('\n')+'\n';
}
}
