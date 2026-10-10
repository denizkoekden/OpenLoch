#include "draw.h"
#include "font.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace openloch::pcb {
namespace {
double normalized(double a){a=std::fmod(a,360.0);return a<0?a+360:a;}
bool fullCircle(const Element &e){return std::abs(normalized(e.stop-e.start))<1e-9;}
}

void paintElement(QPainter &p,const Element &e,const QColor &colour,double grow,const QColor &pad){
    auto pen=[&](double width,Qt::PenCapStyle cap=Qt::RoundCap){QPen q(colour,width,Qt::SolidLine,cap,Qt::RoundJoin);if(width<=0)q.setCosmetic(true),q.setWidthF(1);return q;};
    const QColor padColour=pad.isValid()?pad:colour;
    switch(e.type){
    case ElementType::Pad:
        p.setPen(grow>0?pen(2*grow):Qt::NoPen);p.setBrush(padColour);
        if(e.points.size()==2&&e.shape==PadShape::Round){p.setPen(Qt::NoPen);p.drawEllipse(e.pos,e.size/2+grow,e.size/2+grow);}
        else if(e.points.size()==2){p.setPen(QPen(padColour,e.size+2*grow,Qt::SolidLine,Qt::RoundCap));p.drawLine(e.points[0],e.points[1]);}
        else p.drawPolygon(e.points);
        break;
    case ElementType::SmdPad:p.setPen(grow>0?pen(2*grow):Qt::NoPen);p.setBrush(colour);p.drawPolygon(e.points);break;
    case ElementType::Track:{
        const double w=e.width+2*grow;p.setBrush(Qt::NoBrush);p.setPen(pen(w));p.drawPolyline(e.points);
        if(e.points.size()>1&&w>0&&(e.flatStart||e.flatEnd)){
            p.setPen(Qt::NoPen);p.setBrush(colour);
            if(e.flatStart)p.drawPolygon(squareEnd(e.points.first(),e.points[1],w));
            if(e.flatEnd)p.drawPolygon(squareEnd(e.points.last(),e.points[e.points.size()-2],w));
        }
        break;}
    case ElementType::Area:
        if(e.maskOnly&&grow==0){
            // An opening of the solder mask: hatched at 45° with its outline.
            QPainterPath inside;inside.addPolygon(e.points);inside.closeSubpath();const QRectF box=e.points.boundingRect();
            p.save();p.setClipPath(inside,Qt::IntersectClip);QPen hatch(colour,1);hatch.setCosmetic(true);p.setPen(hatch);
            const double step=.5;const double span=box.width()+box.height();
            if(span/step<200000)for(double d=0;d<=span;d+=step)p.drawLine(QPointF(box.left()+d,box.top()),QPointF(box.left()+d-box.height(),box.bottom()));
            p.restore();p.setBrush(Qt::NoBrush);p.setPen(pen(e.width));p.drawPolygon(e.points);
            break;
        }
        if(e.hatched&&grow==0){
            // The lines of the grid with round ends, not cut to the outline, then the border.
            p.setPen(QPen(colour,hatchLineWidth(e),Qt::SolidLine,Qt::RoundCap));for(const auto &l:hatchLines(e))p.drawLine(l);
            p.setBrush(Qt::NoBrush);p.setPen(e.width>0?pen(e.width):Qt::NoPen);p.drawPolygon(e.points);
            break;
        }
        p.setBrush(colour);p.setPen(e.width+2*grow>0?pen(e.width+2*grow):Qt::NoPen);p.drawPolygon(e.points);break;
    case ElementType::Circle:{
        const double outer=e.size+e.width/2+grow;
        if(e.filled){p.setPen(grow>0?pen(2*grow):Qt::NoPen);p.setBrush(colour);const QRectF r(e.pos-QPointF(outer-grow,outer-grow),QSizeF(2*(outer-grow),2*(outer-grow)));
            if(fullCircle(e)){p.setPen(Qt::NoPen);p.drawEllipse(e.pos,outer,outer);}else p.drawPie(r,qRound(e.start*16),qRound(normalized(e.stop-e.start)*16));}
        else{p.setBrush(Qt::NoBrush);p.setPen(pen(e.width+2*grow));const QRectF r(e.pos-QPointF(e.size,e.size),QSizeF(2*e.size,2*e.size));
            if(fullCircle(e))p.drawEllipse(r);else p.drawArc(r,qRound(e.start*16),qRound(normalized(e.stop-e.start)*16));}
        break;}
    case ElementType::Text:{
        if(!e.visible&&e.role!=TextRole::Plain)break;
        p.setBrush(Qt::NoBrush);
        const auto strokes=e.strokes.isEmpty()?textStrokes(e.text,e.pos,e.size,e.style,e.thickness,e.rotation,e.mirrored):e.strokes;
        const QPen line=pen((e.strokes.isEmpty()?textStrokeWidth(e.size,e.thickness):e.strokeWidth)+2*grow);p.setPen(line);
        for(const auto &s:strokes){
            // A stroke of one point (the dot of an i or an umlaut) is a round dot as wide as the line.
            if(s.size()>1&&s.boundingRect().size()!=QSizeF(0,0)){p.drawPolyline(s);continue;}
            if(s.isEmpty())continue;
            if(line.isCosmetic()){p.drawPoint(s[0]);continue;}
            const double r=line.widthF()/2;p.setPen(Qt::NoPen);p.setBrush(colour);p.drawEllipse(s[0],r,r);p.setPen(line);p.setBrush(Qt::NoBrush);
        }
        break;}
    }
}
}
