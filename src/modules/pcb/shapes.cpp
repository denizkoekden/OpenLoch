#include "shapes.h"
#include "font.h"
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace openloch::pcb {
QList<Element> regularPolygon(int corners,double radius,double width,bool filled,int layer,double offset,bool rays){
    corners=std::clamp(corners,3,360);
    Element e=newElement(filled?ElementType::Area:ElementType::Track);e.layer=layer;e.width=width;
    for(int i=0;i<corners;i++){const double a=qDegreesToRadians(offset+360.0*i/corners);e.points<<QPointF(radius*std::cos(a),-radius*std::sin(a));}
    QList<Element> out;
    if(rays)for(int i=0;i<corners;i++){Element ray=newElement(ElementType::Track);ray.layer=layer;ray.width=width;ray.points={{0,0},e.points[i]};out<<ray;}
    if(!filled)e.points<<e.points.first();
    out.prepend(e);return out;
}
Element spiral(const SpiralShape &shape,int layer){
    Element e=newElement(ElementType::Track);e.layer=layer;e.width=shape.width;
    const double r=shape.start,step=shape.gap+shape.width;
    if(shape.square){
        // Side k is 2 r + k step / 2 long; the sides run left, up, right and down on the screen.
        const int sides=std::max(1,int(std::lround(shape.turns*4)));const QPointF ways[4]={{-1,0},{0,-1},{1,0},{0,1}};
        QPointF at(r,r);e.points<<at;
        for(int k=0;k<sides;k++){at+=ways[k%4]*(2*r+k*step/2);e.points<<at;}
        return e;
    }
    // Half circle k has the radius r + k step / 2 around the origin (k even, over the top) or half a step to the right
    // (k odd, through the bottom); 36 segments each, a part of one for a turn that ends in between.
    const double halves=std::max(.5,shape.turns*2);
    for(int k=0;k<std::ceil(halves-1e-9);k++){
        const double part=std::min(1.0,halves-k),radius=r+k*step/2,cx=k%2?step/2:0;
        const int segments=std::max(1,int(std::ceil(36*part)));
        for(int i=k?1:0;i<=segments;i++){const double a=M_PI*(k+part*i/segments);e.points<<QPointF(cx+radius*std::cos(a),-radius*std::sin(a));}
    }
    return e;
}
double spiralDiameter(const SpiralShape &shape){return 2*shape.start+(2*shape.turns-1)*(shape.gap+shape.width)+shape.width;}

QString frameLabel(int index,bool letters){
    if(!letters)return QString::number(index+1);
    QString s;for(int n=index+1;n>0;n=(n-1)/26)s.prepend(QChar('A'+(n-1)%26));
    return s;
}
QList<Element> frame(const FrameShape &shape,int layer){
    const double w=shape.width/2,h=shape.height/2,b=frameBand;
    const bool top=shape.columnSides&FrameShape::First,bottom=shape.columnSides&FrameShape::Second;
    const bool left=shape.rowSides&FrameShape::First,right=shape.rowSides&FrameShape::Second;
    const double xl=-w+(left?b:0),xr=w-(right?b:0),yt=-h+(top?b:0),yb=h-(bottom?b:0);
    QList<Element> out;
    auto line=[&](QList<QPointF> points,double width){Element e=newElement(ElementType::Track);e.layer=layer;e.width=width;e.points=points;out<<e;};
    auto label=[&](const QString &text,QPointF centre){
        Element e=newElement(ElementType::Text);e.layer=layer;e.text=text;e.size=frameText;updateStrokes(e);
        pcb::move(e,centre-bounds(e).center());out<<e;};
    line({{-w,-h},{w,-h},{w,h},{-w,h},{-w,-h}},frameOutline);
    // The inner edge of each band; bands along the sides end where those across the ends begin.
    if(top)line({{xl,yt},{xr,yt}},frameLine);
    if(bottom)line({{xl,yb},{xr,yb}},frameLine);
    if(left)line({{xl,yt},{xl,yb}},frameLine);
    if(right)line({{xr,yt},{xr,yb}},frameLine);
    const int columns=std::max(1,shape.columns),rows=std::max(1,shape.rows);
    auto x=[&](int i){return xl+(xr-xl)*i/columns;};auto y=[&](int j){return yt+(yb-yt)*j/rows;};
    for(int i=1;i<columns;i++){if(top)line({{x(i),-h},{x(i),yt}},frameLine);if(bottom)line({{x(i),yb},{x(i),h}},frameLine);}
    for(int j=1;j<rows;j++){if(left)line({{-w,y(j)},{xl,y(j)}},frameLine);if(right)line({{xr,y(j)},{w,y(j)}},frameLine);}
    for(int i=0;i<columns;i++){const QString t=frameLabel(i,shape.columnLetters);const double cx=(x(i)+x(i+1))/2;
        if(top)label(t,{cx,(-h+yt)/2});if(bottom)label(t,{cx,(yb+h)/2});}
    for(int j=0;j<rows;j++){const QString t=frameLabel(j,shape.rowLetters);const double cy=(y(j)+y(j+1))/2;
        if(left)label(t,{(-w+xl)/2,cy});if(right)label(t,{(xr+w)/2,cy});}
    return out;
}
}
