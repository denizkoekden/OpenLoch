#include "emf.h"
#include "emfwriter.h"
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <climits>
#include <cmath>

namespace openloch::schematic {
namespace {
constexpr double unitsPerMm=100;

// Passes what is painted on it to `target`, a painter on the EMF device, with dashed lines cut into their dashes.
class DashEngine : public QPaintEngine {
public:
    explicit DashEngine(QPainter &target):QPaintEngine(QPaintEngine::AllFeatures),target(&target){}
    bool begin(QPaintDevice *) override{setActive(true);return true;}
    bool end() override{setActive(false);return true;}
    Type type() const override{return QPaintEngine::User;}
    void updateState(const QPaintEngineState &s) override{
        const DirtyFlags f=s.state();
        if(f&DirtyTransform)transform=s.transform();
        if(f&DirtyPen)pen=s.pen();
        if(f&DirtyBrush)brush=s.brush();
        if(f&DirtyOpacity)target->setOpacity(s.opacity());
        if(f&DirtyHints)target->setRenderHints(s.renderHints());
        if(f&DirtyClipEnabled&&!s.isClipEnabled())clipTo(Qt::NoClip,{});
        if(f&DirtyClipPath)clipTo(s.clipOperation(),transform.map(s.clipPath()));
        if(f&DirtyClipRegion){QPainterPath p;p.addRegion(s.clipRegion());clipTo(s.clipOperation(),transform.map(p));}
    }
    void drawPath(const QPainterPath &path) override{shape(path,true);}
    void drawPolygon(const QPointF *points,int count,PolygonDrawMode mode) override{
        QPainterPath p;if(count<=0)return;p.moveTo(points[0]);for(int i=1;i<count;i++)p.lineTo(points[i]);
        if(mode!=PolylineMode)p.closeSubpath();
        p.setFillRule(mode==WindingMode?Qt::WindingFill:Qt::OddEvenFill);shape(p,mode!=PolylineMode);
    }
    void drawPolygon(const QPoint *points,int count,PolygonDrawMode mode) override{
        QList<QPointF> f;f.reserve(count);for(int i=0;i<count;i++)f<<QPointF(points[i]);drawPolygon(f.constData(),count,mode);
    }
    void drawLines(const QLineF *lines,int count) override{
        QPainterPath p;for(int i=0;i<count;i++){p.moveTo(lines[i].p1());p.lineTo(lines[i].p2());}shape(p,false);
    }
    void drawLines(const QLine *lines,int count) override{for(int i=0;i<count;i++){const QLineF l(lines[i]);drawLines(&l,1);}}
    void drawRects(const QRectF *rects,int count) override{QPainterPath p;for(int i=0;i<count;i++)p.addRect(rects[i]);shape(p,true);}
    void drawRects(const QRect *rects,int count) override{for(int i=0;i<count;i++){const QRectF r(rects[i]);drawRects(&r,1);}}
    void drawEllipse(const QRectF &r) override{QPainterPath p;p.addEllipse(r);shape(p,true);}
    void drawEllipse(const QRect &r) override{drawEllipse(QRectF(r));}
    void drawPoints(const QPointF *points,int count) override{
        QPainterPath p;for(int i=0;i<count;i++){p.moveTo(points[i]);p.lineTo(points[i]);}shape(p,false);
    }
    void drawPoints(const QPoint *points,int count) override{for(int i=0;i<count;i++){const QPointF q(points[i]);drawPoints(&q,1);}}
    void drawPixmap(const QRectF &r,const QPixmap &pixmap,const QRectF &source) override{drawImage(r,pixmap.toImage(),source,Qt::AutoColor);}
    void drawImage(const QRectF &r,const QImage &image,const QRectF &source,Qt::ImageConversionFlags flags) override{
        applyClip();target->setTransform(transform);target->drawImage(r,image,source,flags);
    }
private:
    QPainter *target;
    QTransform transform;
    QPen pen;
    QBrush brush;
    QPainterPath clip;              // in device units; empty with `clipped` false: none
    bool clipped=false,clipChanged=false;
    void clipTo(Qt::ClipOperation op,const QPainterPath &path){
        if(op==Qt::NoClip){clipped=false;clip=QPainterPath();}
        else if(op==Qt::IntersectClip&&clipped)clip=clip.intersected(path);
        else{clip=path;clipped=true;}
        clipChanged=true;
    }
    void applyClip(){
        if(!clipChanged)return;
        clipChanged=false;target->setTransform(QTransform());
        if(clipped)target->setClipPath(clip);else target->setClipping(false);
    }
    void shape(const QPainterPath &logical,bool fillable){
        applyClip();target->setTransform(transform);
        const QBrush fill=fillable?brush:QBrush(Qt::NoBrush);
        if(pen.style()==Qt::NoPen||pen.style()==Qt::SolidLine){target->setPen(pen);target->setBrush(fill);target->drawPath(logical);return;}
        if(fill.style()!=Qt::NoBrush){target->setPen(Qt::NoPen);target->setBrush(fill);target->drawPath(logical);}
        // Qt counts dashes in pen widths (one device unit for hairlines).
        const bool cosmetic=pen.isCosmetic()||pen.widthF()<=0;
        const double width=cosmetic?std::max(1.,pen.widthF()):pen.widthF()*std::sqrt(std::abs(transform.determinant()));
        QPen solid(pen);solid.setStyle(Qt::SolidLine);solid.setCapStyle(Qt::RoundCap);solid.setJoinStyle(Qt::RoundJoin);
        if(!cosmetic)solid.setWidthF(width);
        target->setTransform(QTransform());target->setPen(solid);target->setBrush(Qt::NoBrush);
        for(const QPolygonF &d:dashPieces(transform.map(logical),pen.dashPattern(),width,pen.dashOffset()))target->drawPolyline(d);
    }
};

class DashDevice : public QPaintDevice {
public:
    DashDevice(QSizeF mm,QPainter &target):size(mm),engine(target){}
    QPaintEngine *paintEngine() const override{return &engine;}
protected:
    int metric(PaintDeviceMetric m) const override{
        switch(m){
        // Within int before the conversion (a frame beyond about 200 m would not fit).
        case PdmWidth:return int(std::clamp(std::ceil(size.width()*unitsPerMm),1.,2e9));
        case PdmHeight:return int(std::clamp(std::ceil(size.height()*unitsPerMm),1.,2e9));
        case PdmWidthMM:return int(std::clamp(std::ceil(size.width()),1.,2e9));
        case PdmHeightMM:return int(std::clamp(std::ceil(size.height()),1.,2e9));
        case PdmDpiX:case PdmDpiY:case PdmPhysicalDpiX:case PdmPhysicalDpiY:return int(std::lround(unitsPerMm*25.4));
        case PdmDepth:return 32;
        case PdmNumColors:return INT_MAX;
        case PdmDevicePixelRatio:return 1;
        case PdmDevicePixelRatioScaled:return int(devicePixelRatioFScale());
        default:return QPaintDevice::metric(m);
        }
    }
private:
    QSizeF size;
    mutable DashEngine engine;
};
}

QList<QPolygonF> dashPieces(const QPainterPath &path,const QList<qreal> &pattern,double unit,double offset){
    QList<QPolygonF> out;
    double period=0;for(double d:pattern)period+=std::max(0.,d)*unit;
    const auto lines=path.toSubpathPolygons();
    if(pattern.size()<2||period<=0){for(const auto &line:lines)out<<line;return out;}
    const auto step=[&](int k){return std::max(0.,pattern[k%pattern.size()])*unit;};
    for(const QPolygonF &line:lines){
        if(line.isEmpty())continue;
        // Where the pattern stands at the start: entry k with `left` of it to go.
        int k=0;double left=step(0),skip=std::fmod(offset*unit,period);if(skip<0)skip+=period;
        while(skip>0&&skip>=left){skip-=left;k=(k+1)%int(pattern.size());left=step(k);}
        left-=skip;
        QPolygonF piece;bool on=k%2==0;if(on)piece<<line.first();
        for(int i=1;i<line.size();i++){
            QPointF a=line[i-1];const QPointF b=line[i];double length=QLineF(a,b).length();
            while(left<=length){
                const QPointF at=length>0?a+(b-a)*(left/length):a;
                if(on){piece<<at;out<<piece;piece.clear();}
                length-=left;a=at;k=(k+1)%int(pattern.size());left=step(k);on=k%2==0;
                if(on)piece<<at;
            }
            left-=length;
            if(on)piece<<b;
        }
        if(on&&!piece.isEmpty()){if(piece.size()==1)piece<<piece.first();out<<piece;}
    }
    return out;
}
QByteArray emfDrawing(QSizeF size,const std::function<void(QPainter&)> &draw){
    EmfDevice emf(size,QStringLiteral("Schaltplan"));
    {
        QPainter target(&emf);
        DashDevice device(size,target);
        QPainter painter(&device);painter.scale(unitsPerMm,unitsPerMm);draw(painter);
    }
    return emf.data();
}
QByteArray sheetEmf(const Document &document,int sheet,const RenderOptions &options){
    if(sheet<0||sheet>=document.sheets.size())return {};
    const Sheet &s=document.sheets[sheet];
    return emfDrawing(QSizeF(s.width,s.height),[&](QPainter &p){paintSheet(p,document,sheet,options);});
}
}
