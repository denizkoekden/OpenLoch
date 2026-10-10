#include "dimension.h"
#include "text.h"
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace openloch::schematic {
namespace {
QPointF unit(QPointF v){const double l=std::hypot(v.x(),v.y());return l>1e-12?v/l:QPointF(1,0);}
// An arrow with its tip at `tip`, its base `length` away in the direction `back`, opening `angle` degrees.
QPolygonF arrow(QPointF tip,QPointF back,double length,double angle){
    const QPointF b=unit(back),side(-b.y(),b.x());const double half=length*std::tan(qDegreesToRadians(std::clamp(angle,1.,170.))/2);
    return {tip,tip+b*length+side*half,tip+b*length-side*half};
}
// A text turn that reads from the left or from below: within (−90°, 90°].
double readable(double degrees){
    while(degrees>90)degrees-=180;
    while(degrees<=-90)degrees+=180;
    return degrees;
}
struct Block {Item main,upper,lower;bool tolerances=false;double width=0,height=0;};
// The texts of a dimension, not yet placed: the value and the tolerances at half its height.
Block block(const Item &d,double scale){
    Block b;
    b.main.type=ItemType::Text;b.main.font=d.font;b.main.align=Align::Centre;b.main.text=dimensionText(d,scale);
    const QRectF m=textRect(b.main,b.main.text);b.height=m.height();
    double tolerance=0;
    b.tolerances=!d.upperTolerance.isEmpty()||!d.lowerTolerance.isEmpty();
    for(auto [t,s]:{std::pair{&b.upper,d.upperTolerance},std::pair{&b.lower,d.lowerTolerance}}){
        t->type=ItemType::Text;t->font=d.font;t->font.height=d.font.height/2;t->align=Align::Left;t->text=s;
        if(!s.isEmpty())tolerance=std::max(tolerance,textRect(*t,s).width());
    }
    b.width=(b.main.text.isEmpty()?0:m.width())+(b.tolerances?d.font.height*.15+tolerance:0);
    return b;
}
// Places the texts with the middle of the whole block at `centre`, turned by `degrees`.
QList<Item> place(Block b,QPointF centre,double degrees){
    const double a=qDegreesToRadians(degrees);const QPointF along(std::cos(a),-std::sin(a)),up(-std::sin(a),-std::cos(a));
    const double mainWidth=b.main.text.isEmpty()?0:textRect(b.main,b.main.text).width();
    QList<Item> out;
    b.main.rotation=degrees;b.main.pos=centre+along*(-b.width/2+mainWidth/2)+up*(b.height/2);out<<b.main;
    if(b.tolerances){
        const double left=-b.width/2+mainWidth+b.main.font.height*.15;
        b.upper.rotation=b.lower.rotation=degrees;
        b.upper.pos=centre+along*left+up*(b.height/2);b.lower.pos=centre+along*left;
        if(!b.upper.text.isEmpty())out<<b.upper;
        if(!b.lower.text.isEmpty())out<<b.lower;
    }
    return out;
}
}
QString dimensionNumber(double value,int digits,bool point){
    if(!std::isfinite(value))return QStringLiteral("NAN");
    QString s=QString::number(value,'f',std::clamp(digits,0,6));
    if(s.contains(u'.')){while(s.endsWith(u'0'))s.chop(1);if(s.endsWith(u'.'))s.chop(1);}
    if(s==u"-0")s=QStringLiteral("0");
    if(!point)s.replace(u'.',u',');
    return s;
}
double dimensionValue(const Item &d,double scale){
    const QPointF a=d.points.value(0),b=d.points.value(1);
    if(d.dimension==DimensionKind::Angle){
        const QPointF v=d.points.value(2);
        const double a0=std::atan2(-(a-v).y(),(a-v).x()),a1=std::atan2(-(b-v).y(),(b-v).x());
        double sweep=std::remainder(a1-a0,2*std::numbers::pi);
        return std::abs(qRadiansToDegrees(sweep));
    }
    return std::hypot(b.x()-a.x(),b.y()-a.y())*scale;
}
QString dimensionText(const Item &d,double scale){
    const QString value=d.autoValue?dimensionNumber(dimensionValue(d,scale),d.digits,d.decimalPoint):d.fixedValue;
    return d.prefix+(d.showDiameter?QStringLiteral("Ø"):QString())+value+d.suffix;
}
DimensionDrawing dimensionDrawing(const Item &d,double scale){
    DimensionDrawing out;
    if(d.points.size()<2)return out;
    const Block texts=block(d,scale);
    const double length=std::max(0.,d.arrowLength);
    if(d.dimension==DimensionKind::Angle&&d.points.size()>=3){
        const QPointF v=d.points[2],v0=d.points[0]-v,v1=d.points[1]-v;
        const double r0=std::hypot(v0.x(),v0.y()),r1=std::hypot(v1.x(),v1.y()),radius=r0>1e-9?r0:r1;
        const double a0=std::atan2(-v0.y(),v0.x()),a1=std::atan2(-v1.y(),v1.x());
        double sweep=std::remainder(a1-a0,2*std::numbers::pi);
        if(std::abs(sweep+std::numbers::pi)<1e-9)sweep=std::numbers::pi;   // a straight angle turns counter-clockwise, as in the reference
        auto at=[&](double a,double r){return v+QPointF(std::cos(a),-std::sin(a))*r;};
        // The legs from the vertex through both points, 2 mm beyond them (or beyond the arc).
        out.extensions<<QLineF(v,at(a0,r0+dimensionOvershoot))<<QLineF(v,at(a1,std::max(r1,radius)+dimensionOvershoot));
        const QRectF circle(v-QPointF(radius,radius),QSizeF(2*radius,2*radius));
        out.line.arcMoveTo(circle,qRadiansToDegrees(a0));out.line.arcTo(circle,qRadiansToDegrees(a0),qRadiansToDegrees(sweep));
        if(radius>1e-9){
            const double step=(sweep>=0?1:-1)*length/radius;
            out.arrows<<arrow(at(a0,radius),at(a0+step,radius)-at(a0,radius),length,d.arrowAngle)
                      <<arrow(at(a1,radius),at(a1-step,radius)-at(a1,radius),length,d.arrowAngle);
        }
        // The text outside the middle of the arc, along it.
        const double middle=a0+sweep/2;
        const QPointF outward(std::cos(middle),-std::sin(middle));
        out.texts=place(texts,v+outward*(radius+dimensionTextGap+texts.height/2),readable(qRadiansToDegrees(middle)-90));
        return out;
    }
    const QPointF p0=d.points[0],p1=d.points[1],u=unit(p1-p0),n(u.y(),-u.x());
    const double l=std::hypot(p1.x()-p0.x(),p1.y()-p0.y());
    const QPointF a=p0+n*d.offset,b=p1+n*d.offset;
    if(d.dimension==DimensionKind::Standard){
        const QPointF beyond=n*((d.offset>=0?1:-1)*dimensionOvershoot);
        out.extensions<<QLineF(p0,a+beyond)<<QLineF(p1,b+beyond);
    }
    const bool radial=d.dimension==DimensionKind::Radial;
    // The arrows fit between the ends with the text, or go outside pointing in.
    const bool inside=texts.width+(radial?1:2)*length<=l+1e-9;
    if(inside){
        out.line.moveTo(a);out.line.lineTo(b);
        if(!radial)out.arrows<<arrow(a,u,length,d.arrowAngle);
        out.arrows<<arrow(b,-u,length,d.arrowAngle);
    }else{
        out.line.moveTo(radial?a:a-u*(2*length));out.line.lineTo(b+u*(2*length));
        if(!radial)out.arrows<<arrow(a,-u,length,d.arrowAngle);
        out.arrows<<arrow(b,u,length,d.arrowAngle);
    }
    // The text above the middle of the line, readable.
    const double turn=readable(qRadiansToDegrees(std::atan2(-u.y(),u.x())));
    const double t=qDegreesToRadians(turn);const QPointF up(-std::sin(t),-std::cos(t));
    out.texts=place(texts,(a+b)/2+up*(dimensionTextGap+texts.height/2),turn);
    return out;
}
}
