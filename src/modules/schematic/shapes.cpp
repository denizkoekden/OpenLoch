#include "shapes.h"
#include <QtMath>
#include <cmath>
#include <functional>
#include <numbers>

namespace openloch::schematic {
namespace {
Item open(const Item &line,ItemType type,const QPolygonF &points){
    Item i=line;i.type=type;i.points=points;i.children.clear();i.electrical=false;i.startEnd=i.endEnd=LineEnd::None;return i;
}
Item closed(const Item &shape,const QPolygonF &points){Item i=shape;i.type=ItemType::Polygon;i.points=points;i.children.clear();return i;}
// Points on an ellipse in the frame, the first one straight up, turned counter-clockwise by `offset` degrees.
QPolygonF around(QRectF r,int count,double offset,const std::function<double(int)> &radius){
    QPolygonF out;
    for(int k=0;k<count;k++){
        const double a=qDegreesToRadians(90+offset+360.*k/count),f=radius(k);
        out<<QPointF(r.center().x()+r.width()/2*f*std::cos(a),r.center().y()-r.height()/2*f*std::sin(a));
    }
    return out;
}
}

QList<Item> specialShape(SpecialShape kind,QPointF a,QPointF b,const SpecialShapeOptions &o,const Item &line,const Item &shape,const Item &text){
    const QRectF r=QRectF(a,b).normalized();
    const double x1=a.x(),y1=a.y(),x2=b.x(),y2=b.y(),w=x2-x1,h=y2-y1,cx=r.center().x(),cy=r.center().y();
    // A point of the frame by fractions of its width and height, measured from where it was started.
    auto at=[&](double u,double v){return QPointF(x1+u*w,y1+v*h);};
    auto ring=[](QPolygonF p){p<<p.first();return p;};
    switch(kind){
    case SpecialShape::RegularPolygon:{
        const QPolygonF p=around(r,std::max(3,o.corners),o.polygonOffset,[](int){return 1.;});
        return {o.polygonAsLine?open(line,ItemType::Line,ring(p)):closed(shape,p)};
    }
    case SpecialShape::Star:{
        const double inner=std::clamp(1-o.spikeDepth/100,0.,1.);
        const QPolygonF p=around(r,2*std::max(3,o.spikes),o.starOffset,[inner](int k){return k%2?inner:1.;});
        return {o.starAsLine?open(line,ItemType::Line,ring(p)):closed(shape,p)};
    }
    case SpecialShape::Grid:{
        Item group;group.type=ItemType::Group;
        const int columns=std::max(1,o.columns),rows=std::max(1,o.rows);
        const double cw=r.width()/columns,rh=r.height()/rows;
        if(o.frame){Item f=shape;f.type=ItemType::Rectangle;f.children.clear();f.centre=r.center();f.size=r.size();f.rotation=0;f.fill.style=FillStyle::None;
            f.pen=line.pen;group.children<<f;}
        for(int c=1;c<columns;c++)group.children<<open(line,ItemType::Line,{QPointF(r.left()+c*cw,r.top()),QPointF(r.left()+c*cw,r.bottom())});
        for(int k=1;k<rows;k++)group.children<<open(line,ItemType::Line,{QPointF(r.left(),r.top()+k*rh),QPointF(r.right(),r.top()+k*rh)});
        if(o.textFields)for(int k=0;k<rows;k++)for(int c=0;c<columns;c++){
            Item t=text;t.type=ItemType::Text;t.children.clear();t.text=QStringLiteral("Text");t.align=Align::Centre;t.rotation=0;t.mirrored=false;
            t.font.height=std::max(.1,rh*o.textHeight/100);
            t.pos=QPointF(r.left()+(c+.5)*cw,r.top()+(k+.5)*rh-t.font.height/2);
            group.children<<t;
        }
        return {group};
    }
    case SpecialShape::Wave:{
        // Periods side by side in the drawing direction, between the top and the bottom of the frame.
        const int n=std::max(1,o.waves);const double p=w/n,top=r.top(),bottom=r.bottom();
        QPolygonF pts;
        if(o.wave==WaveKind::Sine){
            // Each half period a cubic curve: its control points a third of the way out and 4/3 of the amplitude high.
            const double half=p/2,c=half*4/3/std::numbers::pi,amp=(bottom-top)/2*4/3;
            pts<<QPointF(x1,cy);
            for(int k=0;k<2*n;k++){
                const double s=x1+k*half,dy=k%2?amp:-amp;
                pts<<QPointF(s+c,cy+dy)<<QPointF(s+half-c,cy+dy)<<QPointF(s+half,cy);
            }
            return {open(line,ItemType::Bezier,pts)};
        }
        pts<<QPointF(x1,bottom);
        for(int k=0;k<n;k++){
            const double s=x1+k*p;
            switch(o.wave){
            case WaveKind::Square:pts<<QPointF(s,top)<<QPointF(s+p/2,top)<<QPointF(s+p/2,bottom)<<QPointF(s+p,bottom);break;
            case WaveKind::Trapezoid:pts<<QPointF(s+p/4,top)<<QPointF(s+p/2,top)<<QPointF(s+3*p/4,bottom)<<QPointF(s+p,bottom);break;
            case WaveKind::Triangle:pts<<QPointF(s+p/2,top)<<QPointF(s+p,bottom);break;
            case WaveKind::Sawtooth:pts<<QPointF(s+p,top)<<QPointF(s+p,bottom);break;
            case WaveKind::Sine:break;
            }
        }
        return {open(line,ItemType::Line,pts)};
    }
    case SpecialShape::CurlyBracket:{
        // Two curves meeting at the tip, on the side where the frame was started.
        Item group;group.type=ItemType::Group;const double m=(x1+x2)/2;
        group.children<<open(line,ItemType::Bezier,{QPointF(x2,r.top()),QPointF(m,r.top()),QPointF(m,cy),QPointF(x1,cy)})
                      <<open(line,ItemType::Bezier,{QPointF(x1,cy),QPointF(m,cy),QPointF(m,r.bottom()),QPointF(x2,r.bottom())});
        return {group};
    }
    case SpecialShape::RoundBracket:{
        const double c=x2+(x1-x2)*4/3;
        return {open(line,ItemType::Bezier,{QPointF(x2,r.top()),QPointF(c,r.top()),QPointF(c,r.bottom()),QPointF(x2,r.bottom())})};
    }
    case SpecialShape::Triangle:return {closed(shape,{at(0,1),at(.5,0),at(1,1)})};
    case SpecialShape::RightTriangle:return {closed(shape,{at(0,0),at(0,1),at(1,1)})};
    case SpecialShape::Square:return {closed(shape,{at(0,0),at(1,0),at(1,1),at(0,1)})};
    case SpecialShape::Diamond:return {closed(shape,{at(.5,0),at(1,.5),at(.5,1),at(0,.5)})};
    case SpecialShape::Parallelogram:{
        // Sides at 60°, leaning in the drawing direction.
        const double s=std::min(std::abs(h)/std::tan(std::numbers::pi/3),std::abs(w)/2)*(w<0?-1:1);
        return {closed(shape,{QPointF(x1+s,y1),QPointF(x2,y1),QPointF(x2-s,y2),QPointF(x1,y2)})};
    }
    case SpecialShape::Hexagon:return {closed(shape,{at(0,.5),at(.25,0),at(.75,0),at(1,.5),at(.75,1),at(.25,1)})};
    case SpecialShape::Octagon:{
        const double e=1/(2+std::sqrt(2.));   // the corners cut as on a regular octagon in a square
        return {closed(shape,{at(e,0),at(1-e,0),at(1,e),at(1,1-e),at(1-e,1),at(e,1),at(0,1-e),at(0,e)})};
    }
    case SpecialShape::ArrowHorizontal:{
        // Pointing where the frame was drawn to; the head a third of the length, at most as long as the arrow is high.
        const double head=std::min(std::abs(w)/3,std::abs(h))*(w<0?-1:1),t=std::abs(h)/4;
        return {closed(shape,{QPointF(x1,cy-t),QPointF(x2-head,cy-t),QPointF(x2-head,r.top()),QPointF(x2,cy),QPointF(x2-head,r.bottom()),QPointF(x2-head,cy+t),QPointF(x1,cy+t)})};
    }
    case SpecialShape::ArrowVertical:{
        const double head=std::min(std::abs(h)/3,std::abs(w))*(h<0?-1:1),t=std::abs(w)/4;
        return {closed(shape,{QPointF(cx-t,y1),QPointF(cx-t,y2-head),QPointF(r.left(),y2-head),QPointF(cx,y2),QPointF(r.right(),y2-head),QPointF(cx+t,y2-head),QPointF(cx+t,y1)})};
    }
    case SpecialShape::SpeechBubble:return {closed(shape,{at(0,0),at(1,0),at(1,.75),at(.45,.75),at(.2,1),at(.25,.75),at(0,.75)})};
    case SpecialShape::Lightning:
        return {closed(shape,{at(.3,0),at(.7,0),at(.52,.33),at(.72,.33),at(.45,.62),at(.62,.62),at(.1,1),at(.32,.68),at(.17,.68),at(.38,.4),at(.2,.4)})};
    }
    return {};
}
std::optional<Item> convertedTo(const Item &item,ItemType type){
    if(type==ItemType::Bezier){
        // "Wandeln in Kurve": the outline as cubic pieces, straight ones with their control points at a third and two
        // thirds, curves as they are; a closed outline returns to its start.
        if(item.type!=ItemType::Line&&item.type!=ItemType::Polygon&&item.type!=ItemType::Rectangle&&item.type!=ItemType::Ellipse)return std::nullopt;
        const QPainterPath p=path(item);QPolygonF points;
        for(int k=0;k<p.elementCount();k++){
            const QPainterPath::Element e=p.elementAt(k);const QPointF at(e.x,e.y);
            if(e.isMoveTo()){if(!points.isEmpty())break;points<<at;}
            else if(e.isLineTo()){const QPointF from=points.last();points<<from+(at-from)/3<<from+(at-from)*2/3<<at;}
            else if(e.isCurveTo()&&k+2<p.elementCount()){const QPainterPath::Element c=p.elementAt(k+1),t=p.elementAt(k+2);points<<at<<QPointF(c.x,c.y)<<QPointF(t.x,t.y);k+=2;}
        }
        const bool closed=item.type==ItemType::Polygon||item.type==ItemType::Rectangle||(item.type==ItemType::Ellipse&&item.arc!=ArcStyle::Arc);
        if(closed&&points.size()>1&&std::hypot(points.last().x()-points.first().x(),points.last().y()-points.first().y())>1e-9){
            const QPointF from=points.last(),to=points.first();points<<from+(to-from)/3<<from+(to-from)*2/3<<to;}
        if(points.size()<4)return std::nullopt;
        Item out=item;out.type=type;out.points=points;out.children.clear();out.electrical=false;
        if(item.type!=ItemType::Line)out.startEnd=out.endEnd=LineEnd::None;
        return out;
    }
    if(type!=ItemType::Line&&type!=ItemType::Polygon)return std::nullopt;
    if(item.type!=ItemType::Line&&item.type!=ItemType::Polygon&&item.type!=ItemType::Bezier&&item.type!=ItemType::Rectangle&&item.type!=ItemType::Ellipse)
        return std::nullopt;
    // Flattened forty times larger, so that Qt's tolerance of half a unit becomes a fortieth of a millimetre.
    constexpr double fine=40;
    QPolygonF points;
    for(const QPolygonF &part:path(item).toSubpathPolygons(QTransform::fromScale(fine,fine)))
        for(const QPointF p:part){const QPointF q=p/fine;if(points.isEmpty()||std::hypot(q.x()-points.last().x(),q.y()-points.last().y())>1e-9)points<<q;}
    const bool closed=item.type==ItemType::Polygon||item.type==ItemType::Rectangle||(item.type==ItemType::Ellipse&&item.arc!=ArcStyle::Arc);
    const auto same=[](QPointF a,QPointF b){return std::hypot(a.x()-b.x(),a.y()-b.y())<1e-9;};
    if(type==ItemType::Polygon&&points.size()>2&&same(points.first(),points.last()))points.removeLast();
    if(type==ItemType::Line&&closed&&points.size()>1&&!same(points.first(),points.last()))points<<points.first();
    if(points.size()<2)return std::nullopt;
    Item out=item;out.type=type;out.points=points;out.children.clear();
    if(type==ItemType::Polygon)out.startEnd=out.endEnd=LineEnd::None;
    if(item.type!=ItemType::Line||type!=ItemType::Line)out.electrical=false;
    return out;
}
}
