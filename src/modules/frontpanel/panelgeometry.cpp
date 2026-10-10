#include "panelgeometry.h"
#include "strokefont.h"
#include <QFontMetricsF>
#include <QJsonArray>
#include <QLineF>
#include <QPainterPathStroker>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace openloch::frontpanel {
namespace {
constexpr double degree=std::numbers::pi/180;
QPointF mid(QPointF a,QPointF b){return (a+b)/2;}
// The point `size` away from the corner b towards a; at most halfway, so that two cuts on one edge never cross.
QPointF cut(QPointF a,QPointF b,double size){
    const double d=QLineF(b,a).length();if(d<=0)return b;
    return b+(a-b)*(std::min(size,d/2)/d);
}
// Builds a contour once for both outputs: Bézier curves into a path, or sampled into points.
struct Builder {
    QPainterPath *path=nullptr;QPolygonF *points=nullptr;int steps=16;
    void move(QPointF p){if(path)path->moveTo(p);if(points)*points<<p;}
    void line(QPointF p){if(path)path->lineTo(p);if(points)*points<<p;}
    void quad(QPointF c,QPointF e){
        if(path)path->quadTo(c,e);
        if(points){const QPointF s=points->isEmpty()?e:points->last();for(int k=1;k<=steps;k++){const double t=double(k)/steps,u=1-t;*points<<u*u*s+2*t*u*c+t*t*e;}}
    }
    void close(){if(path)path->closeSubpath();if(points&&!points->isEmpty())*points<<points->first();}
};
void build(Builder &b,const QPolygonF &p,bool closed,const Contour &contour){
    const int n=p.size();if(n==0)return;
    auto at=[&](int i){return p[((i%n)+n)%n];};
    if(contour.corners==Corners::Sharp||n<3){b.move(p[0]);for(int i=1;i<n;i++)b.line(p[i]);if(closed&&n>2)b.close();return;}
    if(contour.corners==Corners::Spline||contour.corners==Corners::ArcSpline){
        if(closed){b.move(mid(at(-1),at(0)));for(int i=0;i<n;i++)b.quad(at(i),mid(at(i),at(i+1)));b.close();return;}
        // Open, as in the original: through the middles of the edges, for a line also straight from the first point
        // and to the last one; an arc's spline begins and ends in the middle of the first and last edge.
        const bool ends=contour.corners==Corners::Spline;
        if(ends){b.move(p[0]);b.line(mid(p[0],p[1]));}else b.move(mid(p[0],p[1]));
        for(int i=1;i+1<n;i++)b.quad(p[i],mid(p[i],p[i+1]));
        if(ends)b.line(p[n-1]);
        return;
    }
    const bool round=contour.corners==Corners::Round;const double size=std::max(0.0,contour.size);
    auto corner=[&](int i){const QPointF to=cut(at(i+1),at(i),size);if(round)b.quad(at(i),to);else b.line(to);};
    if(closed){b.move(cut(at(-1),at(0),size));for(int i=0;i<n;i++){corner(i);b.line(cut(at(i),at(i+1),size));}b.close();}
    else{b.move(p[0]);for(int i=1;i+1<n;i++){b.line(cut(p[i-1],p[i],size));corner(i);}b.line(p[n-1]);}
}
// 2 × 2 singular value decomposition of the linear part (column vectors): L = R(phi) · diag(sx, sy) · R(theta),
// R being the mathematical rotation; sy is negative for a mirroring map.
struct Svd {double phi,theta,sx,sy;};
Svd svd(double a,double b,double c,double d){
    const double e=(a+d)/2,f=(a-d)/2,g=(c+b)/2,h=(c-b)/2;
    const double q=std::hypot(e,h),r=std::hypot(f,g),a1=std::atan2(g,f),a2=std::atan2(h,e);
    return {(a2+a1)/2,(a2-a1)/2,q+r,q-r};
}
double norm360(double a){a=std::fmod(a,360);if(a<0)a+=360;return a;}
}

QPainterPath contourPath(const QPolygonF &points,bool closed,const Contour &contour){QPainterPath path;Builder b;b.path=&path;build(b,points,closed,contour);return path;}
QPolygonF contourPoints(const QPolygonF &points,bool closed,const Contour &contour,int steps){QPolygonF out;Builder b;b.points=&out;b.steps=std::max(1,steps);build(b,points,closed,contour);return out;}

QTransform rotationAbout(QPointF c,double degrees){
    const double s=std::sin(degrees*degree),k=std::cos(degrees*degree);
    return QTransform(k,-s,s,k,c.x()-(k*c.x()+s*c.y()),c.y()-(-s*c.x()+k*c.y()));
}
QTransform ellipseMap(const Element &e){
    const double s=std::sin(e.rotation*degree),k=std::cos(e.rotation*degree);
    return QTransform(e.radiusX*k,-e.radiusX*s,e.radiusY*s,e.radiusY*k,e.center.x(),e.center.y());
}
void setEllipseMap(Element &e,const QTransform &map,bool keepArc){
    // map takes (cos t, -sin t) to the ellipse; as column vectors its linear part is [[m11, m21], [m12, m22]].
    // With L = R(phi) diag(sx, sy) R(theta) the ellipse is turned by -phi; R(theta) moves the parameter t to t - theta,
    // and a mirroring map (sy < 0) to theta - t, which reverses the arc.
    const Svd s=svd(map.m11(),map.m21(),map.m12(),map.m22());
    e.center=QPointF(map.dx(),map.dy());e.radiusX=s.sx;e.radiusY=std::abs(s.sy);
    e.rotation=norm360(-s.phi/degree);if(e.rotation>180)e.rotation-=360;
    if(keepArc){
        const double theta=s.theta/degree;
        if(s.sy>=0)e.startAngle=norm360(e.startAngle-theta);
        else e.startAngle=norm360(theta-e.startAngle-e.spanAngle);
    }
    // A circle has no own direction: turning it only moves where an arc starts.
    if(e.radiusX-e.radiusY<=1e-6*std::max(1.0,e.radiusX)){if(keepArc)e.startAngle=norm360(e.startAngle+e.rotation);e.rotation=0;e.radiusY=e.radiusX;}
}
QPointF ellipsePoint(const Element &e,double degrees){return ellipseMap(e).map(QPointF(std::cos(degrees*degree),-std::sin(degrees*degree)));}

QTransform frameMap(const QPolygonF &f,QSizeF size){
    if(f.size()<3||size.width()<=0||size.height()<=0)return {};
    const QPointF x=(f[1]-f[0])/size.width(),y=(f[2]-f[0])/size.height();
    return QTransform(x.x(),x.y(),y.x(),y.y(),f[0].x(),f[0].y());
}
QPolygonF frameCorners(const QPolygonF &f){if(f.size()<3)return f;return {f[0],f[1],f[1]+(f[2]-f[0]),f[2]};}
QPolygonF rectFrame(const QRectF &r){return {r.topLeft(),r.topRight(),r.bottomLeft()};}

QFont textFont(const Element &e){
    QFont f(e.font.isEmpty()?QStringLiteral("Arial"):e.font);f.setPixelSize(100);f.setBold(e.bold);f.setItalic(e.italic);
    f.setUnderline(e.underline);f.setStrikeOut(e.strikeOut);f.setStyleStrategy(QFont::PreferOutline);return f;
}
static QString oneLine(const QString &t){QString s=t;s.replace('\n',' ').replace('\r',' ');return s;}
bool strokeText(const Element &e){return e.type==ElementType::Text&&strokeFontFor(e.strokeFont);}
// A stroke font fills the frame with capital height plus descender depth, like an outline font with its full height.
static double strokeHeight(const StrokeFont &f){return f.above+std::max(f.below,0);}
double naturalTextWidth(const Element &e,double height){
    if(const StrokeFont *f=e.type==ElementType::Text?strokeFontFor(e.strokeFont):nullptr){double advance=0;f->text(oneLine(e.text),&advance);return std::max(advance,1.0)*height/strokeHeight(*f);}
    const QFontMetricsF m(textFont(e));const double h=m.ascent()+m.descent();
    return h>0?std::max(m.horizontalAdvance(oneLine(e.text)),1.0)*height/h:height;
}
QPainterPath textPath(const Element &e){
    if(const StrokeFont *f=strokeFontFor(e.strokeFont)){
        // Strokes in font units with y upwards: turned over so that the base line lies at the capital height.
        double advance=0;const QPainterPath strokes=f->text(oneLine(e.text),&advance);
        return frameMap(e.frame,QSizeF(std::max(advance,1.0),strokeHeight(*f))).map(QTransform(1,0,0,-1,0,f->above).map(strokes));
    }
    const QFont font=textFont(e);const QFontMetricsF m(font);const QString text=oneLine(e.text);
    const double width=std::max(m.horizontalAdvance(text),1.0),height=m.ascent()+m.descent();
    QPainterPath path;path.setFillRule(Qt::WindingFill);path.addText(QPointF(0,m.ascent()),font,text);
    // QPainterPath leaves the decorations out; add them as bars like the font draws them.
    const double bar=std::max(m.lineWidth(),1.0);
    if(e.underline)path.addRect(QRectF(0,m.ascent()+m.underlinePos()-bar/2,width,bar));
    if(e.strikeOut)path.addRect(QRectF(0,m.ascent()-m.strikeOutPos()-bar/2,width,bar));
    return frameMap(e.frame,QSizeF(width,height)).map(path);
}

QPainterPath elementPath(const Element &e){
    QPainterPath path;
    switch(e.type){
    case ElementType::Line:return contourPath(e.points,false,e.contour);
    case ElementType::Polygon:case ElementType::Rectangle:return contourPath(e.points,true,e.contour);
    case ElementType::Ellipse:{QPainterPath unit;unit.addEllipse(QPointF(),1,1);return ellipseMap(e).map(unit);}
    case ElementType::Arc:{
        QPainterPath unit;const QRectF circle(-1,-1,2,2);
        if(e.arcStyle==ArcStyle::Pie){unit.moveTo(0,0);unit.arcTo(circle,e.startAngle,e.spanAngle);unit.closeSubpath();}
        else{unit.arcMoveTo(circle,e.startAngle);unit.arcTo(circle,e.startAngle,e.spanAngle);if(e.arcStyle==ArcStyle::Chord)unit.closeSubpath();}
        return ellipseMap(e).map(unit);
    }
    case ElementType::Text:return textPath(e);
    case ElementType::Drill:path.addEllipse(e.center,e.diameter/2,e.diameter/2);return path;
    case ElementType::Image:case ElementType::Picture:path.addPolygon(frameCorners(e.frame));path.closeSubpath();return path;
    default:for(const auto &c:e.children)path.addPath(elementPath(c));if(e.combined())path.setFillRule(Qt::OddEvenFill);return path;
    }
}
static bool penVisible(const Element &e){return e.pen.style!=PenStyle::None;}
QRectF elementBounds(const Element &e){
    if(e.isContainer()&&!e.combined())return elementsBounds(e.children);
    QRectF r=elementPath(e).boundingRect();
    if(e.type==ElementType::Text&&r.isEmpty())r=frameCorners(e.frame).boundingRect();
    if(penVisible(e)){const double w=e.pen.width/2;r.adjust(-w,-w,w,w);}
    return r;
}
QRectF elementsBounds(const QList<Element> &elements){QRectF r;for(const auto &e:elements){const QRectF b=elementBounds(e);r=r.isNull()?b:r.united(b);}return r;}

void transformElement(Element &e,const QTransform &map){
    const double scale=std::sqrt(std::abs(map.determinant()));
    if(e.hasAnchor)e.anchor=map.map(e.anchor);
    switch(e.type){
    case ElementType::Line:case ElementType::Polygon:case ElementType::Rectangle:e.points=map.map(e.points);break;
    case ElementType::Ellipse:case ElementType::Arc:setEllipseMap(e,ellipseMap(e)*map,e.type==ElementType::Arc);break;
    case ElementType::Text:case ElementType::Image:case ElementType::Picture:e.frame=map.map(e.frame);break;
    case ElementType::Drill:e.center=map.map(e.center);if(scale>0)e.diameter*=scale;break;
    default:{
        for(auto &c:e.children)transformElement(c,map);
        // Generators keep their anchor points and where they were placed, so that they can be built again.
        if(e.parameters.contains("anchors")){QJsonArray moved;for(const auto &v:e.parameters["anchors"].toArray()){const auto a=v.toArray();const QPointF p=map.map(QPointF(a[0].toDouble(),a[1].toDouble()));moved.append(QJsonArray{p.x(),p.y()});}e.parameters["anchors"]=moved;}
        if(e.parameters.contains("placement")){const auto a=e.parameters["placement"].toArray();if(a.size()==6){const QTransform t=QTransform(a[0].toDouble(),a[1].toDouble(),a[2].toDouble(),a[3].toDouble(),a[4].toDouble(),a[5].toDouble())*map;
            e.parameters["placement"]=QJsonArray{t.m11(),t.m12(),t.m21(),t.m22(),t.dx(),t.dy()};}}
    }
    }
}

bool hitElement(const Element &e,QPointF p,double tolerance){
    if(e.isContainer()&&!e.combined()){for(const auto &c:e.children)if(hitElement(c,p,tolerance))return true;return false;}
    const QPainterPath path=elementPath(e);
    const bool area=e.type==ElementType::Text||e.type==ElementType::Image||e.type==ElementType::Picture||e.type==ElementType::Drill||(e.closed()&&e.fill.style!=FillStyle::None);
    if(area){if(e.type==ElementType::Text){if(frameCorners(e.frame).containsPoint(p,Qt::OddEvenFill))return true;}else if(path.contains(p))return true;}
    QPainterPathStroker stroker;stroker.setWidth(2*tolerance+(penVisible(e)?e.pen.width:0));stroker.setCapStyle(Qt::RoundCap);stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(path).contains(p);
}
}
