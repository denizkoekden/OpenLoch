#include "fdplot.h"
#include "fdshapes.h"
#include <QByteArray>

namespace openloch::frontdesigner {
namespace {
const Extended two=Extended::fromInt(2);
// Delphi's Round of both coordinates, kept to 32 bits like the original's TPoint.
QPoint whole(const ExtendedPoint &p){return QPoint(int(qint32(p.x.rounded())),int(qint32(p.y.rounded())));}
// The middle of an edge, computed as (b - a) / 2 + a.
ExtendedPoint middle(const ExtendedPoint &a,const ExtendedPoint &b){return {(b.x-a.x)/two+a.x,(b.y-a.y)/two+a.y};}
// The length of an edge; zero when both differences are below 1e-9.
Extended distance(const ExtendedPoint &a,const ExtendedPoint &b){
    static const Extended tiny=Extended::fromBytes(QByteArray::fromHex("97a5b436415f7089e13f").constData());
    const Extended dx=a.x-b.x,dy=a.y-b.y;
    if(dx.abs()<tiny&&dy.abs()<tiny)return {};
    return (dx*dx+dy*dy).sqrt();
}
Extended constant(const char *hex){return Extended::fromBytes(QByteArray::fromHex(hex).constData());}
const Extended pi=constant("35c26821a2da0fc90040"),halfPi=constant("35c26821a2da0fc9ff3f"),twoPi=constant("35c26821a2da0fc90140"),
               threeHalvesPi=constant("a8910e99f9e3cb960140");
// The direction from p to q in [0, 2 pi), counted counter-clockwise on the screen (y downwards), with the original's
// exact values along the axes.
Extended direction(const ExtendedPoint &p,const ExtendedPoint &q){
    if(p.y==q.y)return q.x<=p.x?pi:Extended();
    if(p.x==q.x)return q.y<=p.y?halfPi:threeHalvesPi;
    const Extended dx=q.x-p.x,dy=p.y-q.y,zero;
    if(zero<dx&&zero<dy)return (dy/dx).atan();
    if(zero<dx&&dy<zero)return twoPi-((-dy)/dx).atan();
    if(dx<zero&&zero<dy)return pi-(dy/(-dx)).atan();
    if(dx<zero&&dy<zero)return pi+((-dy)/(-dx)).atan();
    return {};
}
// The point `size` away from the corner towards a neighbour, or the neighbour itself when the edge is not longer.
ExtendedPoint cut(const ExtendedPoint &corner,const ExtendedPoint &neighbour,const Extended &size){
    const Extended d=distance(neighbour,corner);
    if(!(size<d))return neighbour;
    const Extended f=size/d;
    return {corner.x+f*(neighbour.x-corner.x),corner.y+f*(neighbour.y-corner.y)};
}
// A quadratic curve from p0 with control point p1 to p2: t runs from 0 in steps of 0.05 (the double nearest to it)
// while it is at most 1, which gives 20 points, and the end point follows.
void segment(QPolygon &out,QPoint p0,QPoint p1,QPoint p2){
    static const Extended step=Extended::fromDouble(0.05),one=Extended::fromInt(1);
    const Extended ax=Extended::fromInt(qint32(p0.x()-2*p1.x()+p2.x())),bx=Extended::fromInt(qint32(2*p1.x()-2*p0.x())),x0=Extended::fromInt(p0.x());
    const Extended ay=Extended::fromInt(qint32(p0.y()-2*p1.y()+p2.y())),by=Extended::fromInt(qint32(2*p1.y()-2*p0.y())),y0=Extended::fromInt(p0.y());
    for(Extended t;t<=one;t=t+step){
        const Extended t2=t*t;
        out<<whole({ax*t2+bx*t+x0,ay*t2+by*t+y0});
    }
    out<<p2;
}
struct Builder {
    const PlotShape &s;QPolygon &out;
    const ExtendedPoint &at(int i) const{return s.points[i%s.points.size()];}
    // Corner i is the point after the i-th one.
    void spline(int i){segment(out,whole(middle(at(i),at(i+1))),whole(at(i+1)),whole(middle(at(i+1),at(i+2))));}
    void chamfer(int i){const ExtendedPoint &c=at(i+1);out<<whole(cut(c,at(i),s.cornerSize))<<whole(cut(c,at(i+2),s.cornerSize));}
    void round(int i){const ExtendedPoint &c=at(i+1);segment(out,whole(cut(c,at(i),s.cornerSize)),whole(c),whole(cut(c,at(i+2),s.cornerSize)));}
    void open(){
        const int n=s.points.size();if(!n)return;
        if(!s.smooth){for(const auto &p:s.points)out<<whole(p);return;}
        switch(s.cornerStyle){
        case 0:out<<whole(s.points[0]);for(int i=0;i<n-2;i++)spline(i);out<<whole(s.points[n-1]);break;
        case 1:out<<whole(s.points[0]);for(int i=0;i<n-2;i++)chamfer(i);out<<whole(s.points[n-1]);break;
        case 2:out<<whole(s.points[0]);for(int i=0;i<n-2;i++)round(i);out<<whole(s.points[n-1]);break;
        // The B-spline of arcs runs only from the middle of the first edge to the middle of the last one.
        case 3:for(int i=0;i<n-2;i++)spline(i);break;
        default:break;
        }
    }
    void closed(){
        const int n=s.points.size();if(!n)return;
        if(!s.smooth){for(const auto &p:s.points)out<<whole(p);out<<whole(s.points[0]);return;}
        const qsizetype first=out.size();
        switch(s.cornerStyle){
        case 0:for(int i=0;i<n;i++)spline(i);break;
        // Chamfers go round once more to the first corner (n + 1 corners), then back to the first point.
        case 1:for(int i=0;i<=n;i++)chamfer(i);out<<QPoint(out[first]);break;
        case 2:for(int i=0;i<n;i++)round(i);out<<QPoint(out[first]);break;
        // A closed contour with the corners of arcs has no outline in the original.
        default:break;
        }
    }
};
}

QPolygon plotPolygon(const PlotShape &shape){
    QPolygon out;Builder b{shape,out};
    if(shape.arcMode>=0){
        // An arc: its open B-spline, a pie on through the centre to the first point, a chord to the first point.
        b.open();
        if(!shape.points.isEmpty()){
            if(shape.arcMode==1)out<<whole(shape.arcCentre)<<whole(shape.points[0]);
            else if(shape.arcMode==2)out<<whole(shape.points[0]);
        }
        return out;
    }
    if(shape.closed)b.closed();else b.open();
    return out;
}

QList<ExtendedPoint> circlePoints(const ExtendedPoint &centre,const Extended &radius){
    const Extended step=twoPi/Extended::fromInt(16),outer=radius/(step/two).cos();
    QList<ExtendedPoint> out;
    for(int k=1;k<=16;k++){const Extended a=step*Extended::fromInt(k);out<<ExtendedPoint{centre.x+outer*a.cos(),centre.y+outer*a.sin()};}
    return out;
}

QList<QPolygon> plotStrokeText(const StrokeText &t){
    using frontpanel::StrokeElement;using frontpanel::StrokeGlyph;
    const QHash<int,StrokeGlyph> *letters=t.font?originalLetters(*t.font):nullptr;
    if(!letters||t.text.isEmpty())return {};
    const frontpanel::StrokeFont &f=*t.font;const ExtendedPoint &c0=t.corners[0],&c1=t.corners[1],&c2=t.corners[2],&c3=t.corners[3];
    // The scale of the font, kept as a double: font height over capital height plus descender depth.
    const Extended scale=Extended::fromDouble((Extended::fromInt(std::abs(t.height))/Extended::fromInt(f.above+f.below)).toDouble());
    auto scaled=[&](double v){return Extended::fromDouble((Extended::fromDouble(v)*scale).toDouble());};
    // A character without strokes is drawn as the space; codes outside 1…255 as the question mark.
    auto glyphOf=[&](int code)->const StrokeGlyph*{
        if(code<1||code>255)code='?';
        auto it=letters->constFind(code);if(it==letters->constEnd()||it->elements.isEmpty())it=letters->constFind(' ');
        return it==letters->constEnd()?nullptr:&*it;
    };
    auto advance=[&](int code){const StrokeGlyph *g=glyphOf(code);const int a=g?int(qint32((Extended::fromInt(qint64(g->advance))*scale).rounded())):0;return a==0?10:a;};
    int width=0;for(char c:t.text)width+=advance(quint8(c));
    if(width<1)return {};
    // Stretched to the bottom edge of the frame; the base line lies the descender depth above it.
    const Extended stretch=distance(c3,c2)/Extended::fromInt(width);
    const Extended theta=t.flipX?direction(c1,c0):direction(c0,c1);
    const Extended depth=Extended::fromInt(qint32((Extended::fromInt(f.below)*scale).rounded()));
    const qint32 ox=qint32(c3.x.rounded()),oy=qint32(c3.y.rounded());
    // Slanted frames shear the strokes by the angle between the bottom and the left edge beyond a right angle.
    Extended a1=direction(c3,c2),a2=direction(c3,c0);while(a2<a1)a2=a2+twoPi;
    Extended shear=(a2-a1)-halfPi;if(t.flipY)shear=-shear;
    QList<QPolygon> out;int prefix=0;
    for(char ch:t.text){
        const int code=quint8(ch);
        const Extended d=Extended::fromInt((Extended::fromInt(prefix)*stretch).rounded());prefix+=advance(code);
        const Extended sinT=theta.sin(),cosN=(-theta).cos(),sinN=(-theta).sin();
        const Extended tx=t.flipY?Extended::fromInt(ox)+sinT*depth:Extended::fromInt(ox)-sinT*depth,ty=t.flipY?Extended::fromInt(oy)+cosN*depth:Extended::fromInt(oy)-cosN*depth;
        const ExtendedPoint origin{Extended::fromInt(qint32((t.flipX?tx-cosN*d:tx+cosN*d).rounded())),Extended::fromInt(qint32((t.flipX?ty-sinN*d:ty+sinN*d).rounded()))};
        // A point of the glyph (font height units, y downwards) on the panel.
        auto place=[&](const ExtendedPoint &p){
            Extended x=p.x*stretch;
            if(!(shear==Extended())){const Extended tangent=shear.tan();x=t.flipX?x-tangent*p.y:x+tangent*p.y;}
            const ExtendedPoint q{t.flipX?origin.x-x:x+origin.x,t.flipY?origin.y-p.y:p.y+origin.y};
            if(theta==Extended())return whole(q);
            const Extended beta=direction(origin,q)+theta,r=distance(origin,q);
            return whole({origin.x+beta.cos()*r,origin.y-beta.sin()*r});
        };
        const StrokeGlyph *g=glyphOf(code);if(!g)continue;
        // Only strokes ended by lifting the pen are plotted.
        bool up=true;QPolygon stroke;
        for(const StrokeElement &e:g->elements){
            if(e.kind==StrokeElement::Line){
                if(up){up=false;stroke<<place({scaled(e.x1),scaled(e.y1)});}
                stroke<<place({scaled(e.x2),scaled(e.y2)});
            }else if(e.kind==StrokeElement::Arc){
                const ExtendedPoint c{scaled(e.cx),scaled(e.cy)};
                const Extended start=direction(c,{scaled(e.x1),scaled(e.y1)});Extended end=direction(c,{scaled(e.x2),scaled(e.y2)});
                if(e.counterClockwise){while(end<start)end=end+twoPi;}else while(start<end)end=end-twoPi;
                const Extended radius=scaled(e.radius);
                const qint64 steps=((((radius*Extended::fromInt(4)).sqrt()*(end-start).abs())/twoPi)+Extended::fromInt(6)).rounded();
                const Extended step=(end-start)/Extended::fromInt(steps);Extended a=start;
                auto at=[&](const Extended &angle){return place({c.x+angle.cos()*radius,c.y-angle.sin()*radius});};
                if(up){up=false;stroke<<at(a);}
                for(qint64 k=0;k<steps;k++){a=a+step;stroke<<at(a);}
            }else if(!up){out<<stroke;stroke.clear();up=true;}
        }
    }
    return out;
}
}
