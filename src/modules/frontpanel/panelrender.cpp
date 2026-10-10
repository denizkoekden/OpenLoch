#include "panelrender.h"
#include "panelgeometry.h"
#include "emf.h"
#include <QLinearGradient>
#include <QPainterPath>
#include <cmath>

namespace openloch::frontpanel {
namespace {
// Hatches are lines in panel millimetres, so that screen, print and export show the same pitch.
constexpr double hatchPitch=1.0;
QLinearGradient gradientOver(const QRectF &r,Gradient g,const QColor &a,const QColor &b){
    QLinearGradient lg;
    switch(g){
    case Gradient::Horizontal:lg=QLinearGradient(r.topLeft(),r.topRight());break;
    case Gradient::Vertical:lg=QLinearGradient(r.topLeft(),r.bottomLeft());break;
    case Gradient::Diagonal:lg=QLinearGradient(r.topLeft(),r.bottomRight());break;
    default:lg=QLinearGradient(r.bottomLeft(),r.topRight());break;
    }
    lg.setColorAt(0,a);lg.setColorAt(1,b);return lg;
}
void hatch(QPainter &p,const QPainterPath &area,FillStyle style,const QColor &colour,double line){
    const QRectF r=area.boundingRect();if(r.isEmpty())return;
    p.save();p.setClipPath(area,Qt::IntersectClip);QPen pen(colour,line);pen.setCosmetic(line<=0);if(line<=0)pen.setWidthF(1);p.setPen(pen);
    const bool horizontal=style==FillStyle::Horizontal||style==FillStyle::Cross,vertical=style==FillStyle::Vertical||style==FillStyle::Cross;
    const bool forward=style==FillStyle::ForwardDiagonal||style==FillStyle::DiagonalCross,backward=style==FillStyle::BackwardDiagonal||style==FillStyle::DiagonalCross;
    if(horizontal)for(double y=std::floor(r.top()/hatchPitch)*hatchPitch;y<=r.bottom();y+=hatchPitch)p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y));
    if(vertical)for(double x=std::floor(r.left()/hatchPitch)*hatchPitch;x<=r.right();x+=hatchPitch)p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));
    // Diagonals: lines x - y = c (falling to the right on screen) and x + y = c, the clip cuts them to the area.
    const double step=hatchPitch*std::sqrt(2.0);
    if(forward)for(double c=std::floor((r.left()-r.bottom())/step)*step;c<=r.right()-r.top();c+=step)p.drawLine(QPointF(c+r.top(),r.top()),QPointF(c+r.bottom(),r.bottom()));
    if(backward)for(double c=std::floor((r.left()+r.top())/step)*step;c<=r.right()+r.bottom();c+=step)p.drawLine(QPointF(c-r.top(),r.top()),QPointF(c-r.bottom(),r.bottom()));
    p.restore();
}
void fillArea(QPainter &p,const QPainterPath &area,const Fill &fill,double line){
    if(fill.style==FillStyle::None)return;
    if(fill.style==FillStyle::Solid){
        if(fill.gradient==Gradient::None)p.fillPath(area,fill.color);
        else p.fillPath(area,QBrush(gradientOver(area.boundingRect(),fill.gradient,fill.color,fill.color2)));
        return;
    }
    hatch(p,area,fill.style,fill.color,line);
}
QPen penOf(const Pen &pen,double hairline){
    QPen q(pen.color,pen.width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
    if(pen.width<=0){if(hairline>0)q.setWidthF(hairline);else{q.setCosmetic(true);q.setWidthF(1);}}
    switch(pen.style){case PenStyle::Dot:q.setStyle(Qt::DotLine);break;case PenStyle::Dash:q.setStyle(Qt::DashLine);break;case PenStyle::DashDot:q.setStyle(Qt::DashDotLine);break;default:break;}
    return q;
}
void placeholder(QPainter &p,const QPolygonF &frame,double line){
    const QPolygonF c=frameCorners(frame);if(c.size()<4)return;
    QPen pen(QColor(128,128,128),line);pen.setCosmetic(line<=0);if(line<=0)pen.setWidthF(1);p.setPen(pen);p.setBrush(QColor(235,235,235));
    p.drawPolygon(c);p.drawLine(c[0],c[2]);p.drawLine(c[1],c[3]);
}
}

QColor milledColor(){return QColor(0x60,0x60,0x60);}
QColor engravedColor(){return QColor(Qt::white);}

ViewGroup viewGroup(const Element &e){
    if(e.type==ElementType::Dimension)return ViewGroup::Dimension;
    if(e.type==ElementType::Drill)return ViewGroup::Drill;
    if(e.machining==Machining::Mill)return ViewGroup::Milled;
    if(e.machining==Machining::Engrave)return ViewGroup::Engraved;
    if(e.type==ElementType::Text)return ViewGroup::Text;
    return ViewGroup::Other;
}
bool shown(const Element &e,const RenderOptions &o){
    switch(viewGroup(e)){
    case ViewGroup::Dimension:return o.dimensions;case ViewGroup::Drill:return o.drills;case ViewGroup::Milled:return o.milled;
    case ViewGroup::Engraved:return o.engraved;case ViewGroup::Text:return o.texts;default:return o.other;
    }
}

QImage resourceImage(const Document &document,const Element &e){
    const auto it=document.resources.constFind(e.resource);if(it==document.resources.constEnd()||it->kind=="emf")return {};
    QImage image=QImage::fromData(it->data);if(image.isNull()||!e.transparent)return image;
    image=image.convertToFormat(QImage::Format_ARGB32);const QRgb hidden=e.transparentColor.rgb()&0xffffff;
    for(int y=0;y<image.height();y++){auto *row=reinterpret_cast<QRgb*>(image.scanLine(y));for(int x=0;x<image.width();x++)if((row[x]&0xffffff)==hidden)row[x]=0;}
    return image;
}

void paintElement(QPainter &p,const Document &document,const Element &e,const RenderOptions &o){
    if(e.isContainer()&&!e.combined()){if(e.type==ElementType::Dimension&&!o.dimensions)return;for(const auto &c:e.children)paintElement(p,document,c,o);return;}
    if(!shown(e,o))return;
    const QPainterPath path=elementPath(e);const double line=o.hairline;
    p.save();
    if(o.outlineOnly){
        QPen pen(Qt::black,line);if(line<=0){pen.setCosmetic(true);pen.setWidthF(1);}p.setPen(pen);p.setBrush(Qt::NoBrush);
        if(e.type==ElementType::Image||e.type==ElementType::Picture)p.drawPolygon(frameCorners(e.frame));else p.drawPath(path);
        p.restore();return;
    }
    if(e.type==ElementType::Image){
        const QImage image=resourceImage(document,e);
        if(image.isNull())placeholder(p,e.frame,line);
        else{p.setTransform(frameMap(e.frame,image.size()),true);p.setRenderHint(QPainter::SmoothPixmapTransform);p.drawImage(QPointF(),image);}
        p.restore();return;
    }
    if(e.type==ElementType::Picture){
        const auto it=document.resources.constFind(e.resource);
        if(it==document.resources.constEnd()||!paintEmf(p,it->data,e.frame))placeholder(p,e.frame,line);
        p.restore();return;
    }
    if(o.machiningLook&&(e.machining!=Machining::None||e.type==ElementType::Drill)){
        // The contour as the tool cuts it: the tool's width around its path, and the path itself as a thin line.
        const bool milled=e.machining!=Machining::Engrave||e.type==ElementType::Drill;
        QPen trace(milled?QColor(0xc8,0xc8,0xc8):QColor(0x90,0x90,0x90),0);trace.setCosmetic(true);trace.setWidthF(1);
        if(e.type==ElementType::Drill){
            p.fillPath(elementPath(e),milled?milledColor():engravedColor());p.setPen(trace);const double r=e.diameter*0.35;
            p.drawLine(e.center-QPointF(r,0),e.center+QPointF(r,0));p.drawLine(e.center-QPointF(0,r),e.center+QPointF(0,r));
        }else{
            const QPainterPath &course=path;
            QPen tool(milled?milledColor():engravedColor(),std::max(e.pen.width,0.0),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);if(e.pen.width<=0){tool.setCosmetic(true);tool.setWidthF(1);}
            p.strokePath(course,tool);p.strokePath(course,trace);
        }
        p.restore();return;
    }
    const bool strokes=strokeText(e);
    if(e.closed()||(e.type==ElementType::Text&&!strokes)||e.type==ElementType::Drill)fillArea(p,path,e.fill,line);
    if(e.pen.style!=PenStyle::None)p.strokePath(path,penOf(e.pen,line));
    // A stroke text without a visible pen keeps its fill colour as a thin line.
    else if(strokes){QPen thin(e.fill.color,0);thin.setCosmetic(true);thin.setWidthF(1);p.strokePath(path,thin);}
    if(e.type==ElementType::Drill&&e.diameter>0){
        // A small centre cross marks where the drill goes.
        QPen cross(e.pen.style==PenStyle::None?QColor(Qt::black):e.pen.color,0);cross.setCosmetic(true);p.setPen(cross);const double r=e.diameter*0.3;
        p.drawLine(e.center-QPointF(r,0),e.center+QPointF(r,0));p.drawLine(e.center-QPointF(0,r),e.center+QPointF(0,r));
    }
    p.restore();
}

void paintPanel(QPainter &p,const Document &document,const Panel &panel,const RenderOptions &o){
    const QRectF area(0,0,panel.width,panel.height);
    p.save();p.setRenderHint(QPainter::Antialiasing);
    if(o.background){
        if(o.outlineOnly)p.fillRect(area,Qt::white);
        else if(panel.gradient==Gradient::None)p.fillRect(area,panel.color);
        else p.fillRect(area,QBrush(gradientOver(area,panel.gradient,panel.color,panel.color2)));
    }
    if(o.grid&&panel.grid>0){
        const QTransform t=p.worldTransform();const double pixels=panel.grid*std::sqrt(std::abs(t.determinant()));
        int step=1;while(pixels*step<6&&step<10000)step*=10;   // too fine to show: only every tenth line
        QPen dot(panel.gridColor,0);dot.setCosmetic(true);dot.setWidthF(1);p.setPen(dot);
        const double g=panel.grid*step;
        for(double y=std::fmod(panel.origin.y(),g);y<=panel.height+1e-9;y+=g){if(y<-1e-9)continue;for(double x=std::fmod(panel.origin.x(),g);x<=panel.width+1e-9;x+=g)if(x>=-1e-9)p.drawPoint(QPointF(x,y));}
    }
    for(const auto &e:panel.elements)paintElement(p,document,e,o);
    p.restore();
}

QImage renderPanel(const Document &document,const Panel &panel,double dpi,const RenderOptions &o,const QList<Element> *only){
    const double scale=dpi/25.4;const QSize size(std::max(1,int(std::lround(panel.width*scale))),std::max(1,int(std::lround(panel.height*scale))));
    QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);image.setDotsPerMeterX(int(std::lround(dpi/0.0254)));image.setDotsPerMeterY(image.dotsPerMeterX());
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.scale(scale,scale);
    if(only){
        if(o.background){Panel bare=panel;bare.elements.clear();paintPanel(p,document,bare,o);}
        for(const auto &e:*only)paintElement(p,document,e,o);
    }else paintPanel(p,document,panel,o);
    return image;
}
}
