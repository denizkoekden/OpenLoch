#include "svg.h"
#include <QBuffer>
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

// Numbers with at most two decimals, without trailing zeros.
QString number(double v){
    QString s=QString::number(v,'f',2);
    if(s.contains(u'.')){while(s.endsWith(u'0'))s.chop(1);if(s.endsWith(u'.'))s.chop(1);}
    if(s==u"-0")s=QStringLiteral("0");
    return s;
}
QString escaped(QString s){
    return s.replace(u'&',QStringLiteral("&amp;")).replace(u'<',QStringLiteral("&lt;")).replace(u'>',QStringLiteral("&gt;")).replace(u'"',QStringLiteral("&quot;"));
}
// A path's data; a subpath that ends where it began is closed, as Qt strokes it.
QString pathData(const QPainterPath &p){
    QString d;QPointF start;int first=-1;
    auto close=[&](int last){if(first>=0&&last>first&&QPointF(p.elementAt(last))==start)d+=u'Z';};
    for(int i=0;i<p.elementCount();i++){
        const QPainterPath::Element e=p.elementAt(i);
        switch(e.type){
        case QPainterPath::MoveToElement:
            close(i-1);start=QPointF(e);first=i;d+=u'M'+number(e.x)+u' '+number(e.y);break;
        case QPainterPath::LineToElement:d+=u'L'+number(e.x)+u' '+number(e.y);break;
        case QPainterPath::CurveToElement:
            if(i+2<p.elementCount()){
                const QPainterPath::Element c=p.elementAt(i+1),t=p.elementAt(i+2);
                d+=u'C'+number(e.x)+u' '+number(e.y)+u' '+number(c.x)+u' '+number(c.y)+u' '+number(t.x)+u' '+number(t.y);i+=2;
            }
            break;
        default:break;
        }
    }
    close(p.elementCount()-1);
    return d;
}
QString paint(const char *what,const QColor &c,double opacity){
    QString s=QStringLiteral(" %1=\"%2\"").arg(QLatin1String(what),c.name(QColor::HexRgb));
    const double a=c.alphaF()*opacity;if(a<1)s+=QStringLiteral(" %1-opacity=\"%2\"").arg(QLatin1String(what),number(a));
    return s;
}

class SvgEngine : public QPaintEngine {
public:
    SvgEngine():QPaintEngine(QPaintEngine::AllFeatures){}
    QString defs,body;
    bool begin(QPaintDevice *) override{setActive(true);return true;}
    bool end() override{if(grouped)body+=QStringLiteral("</g>\n");grouped=false;setActive(false);return true;}
    Type type() const override{return QPaintEngine::User;}
    void updateState(const QPaintEngineState &s) override{
        const DirtyFlags f=s.state();
        if(f&DirtyTransform)transform=s.transform();
        if(f&DirtyPen)pen=s.pen();
        if(f&DirtyBrush)brush=s.brush();
        if(f&DirtyOpacity)opacity=s.opacity();
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
    void drawImage(const QRectF &r,const QImage &image,const QRectF &source,Qt::ImageConversionFlags) override{
        if(image.isNull()||r.isEmpty())return;
        const QImage part=source.toAlignedRect()==image.rect()?image:image.copy(source.toAlignedRect());
        QByteArray png;{QBuffer b(&png);b.open(QIODevice::WriteOnly);part.save(&b,"PNG");}
        const QTransform &t=transform;
        element(QStringLiteral("<image x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" preserveAspectRatio=\"none\" transform=\"matrix(%5 %6 %7 %8 %9 %10)\"%11 xlink:href=\"data:image/png;base64,%12\"/>")
                .arg(number(r.x()),number(r.y()),number(r.width()),number(r.height()))
                .arg(QString::number(t.m11(),'g',10),QString::number(t.m12(),'g',10),QString::number(t.m21(),'g',10),QString::number(t.m22(),'g',10),number(t.dx()),number(t.dy()))
                .arg(opacity<1?QStringLiteral(" opacity=\"%1\"").arg(number(opacity)):QString(),QString::fromLatin1(png.toBase64())));
    }
private:
    QTransform transform;
    QPen pen;
    QBrush brush;
    double opacity=1;
    QPainterPath clip;              // in the file's units; empty with `clipped` false: none
    bool clipped=false,clipChanged=false,grouped=false;
    int clips=0;
    void clipTo(Qt::ClipOperation op,const QPainterPath &path){
        if(op==Qt::NoClip){clipped=false;clip=QPainterPath();}
        else if(op==Qt::IntersectClip&&clipped)clip=clip.intersected(path);
        else{clip=path;clipped=true;}
        clipChanged=true;
    }
    // An element, inside the group of the clip it is drawn with.
    void element(const QString &e){
        if(clipChanged){
            clipChanged=false;
            if(grouped){body+=QStringLiteral("</g>\n");grouped=false;}
            if(clipped){
                const QString id=QStringLiteral("clip%1").arg(++clips);
                defs+=QStringLiteral("<clipPath id=\"%1\"><path d=\"%2\"%3/></clipPath>\n").arg(id,pathData(clip),clip.fillRule()==Qt::WindingFill?QString():QStringLiteral(" clip-rule=\"evenodd\""));
                body+=QStringLiteral("<g clip-path=\"url(#%1)\">\n").arg(id);grouped=true;
            }
        }
        body+=e+u'\n';
    }
    void shape(const QPainterPath &logical,bool fillable){
        const bool filled=fillable&&brush.style()==Qt::SolidPattern&&brush.color().alpha()>0;
        const bool stroked=pen.style()!=Qt::NoPen&&pen.brush().style()!=Qt::NoBrush&&pen.color().alpha()>0;
        if(!filled&&!stroked)return;
        const QPainterPath p=transform.map(logical);
        QString a=filled?paint("fill",brush.color(),opacity):QStringLiteral(" fill=\"none\"");
        if(filled&&logical.fillRule()==Qt::OddEvenFill)a+=QStringLiteral(" fill-rule=\"evenodd\"");
        if(stroked){
            const bool cosmetic=pen.isCosmetic()||pen.widthF()<=0;
            const double scale=std::sqrt(std::abs(transform.determinant())),width=cosmetic?1:pen.widthF()*scale;
            a+=paint("stroke",pen.color(),opacity)+QStringLiteral(" stroke-width=\"%1\"").arg(number(width));
            if(cosmetic)a+=QStringLiteral(" vector-effect=\"non-scaling-stroke\"");
            a+=pen.capStyle()==Qt::RoundCap?QStringLiteral(" stroke-linecap=\"round\""):pen.capStyle()==Qt::SquareCap?QStringLiteral(" stroke-linecap=\"square\""):QString();
            if(pen.joinStyle()==Qt::RoundJoin)a+=QStringLiteral(" stroke-linejoin=\"round\"");
            else if(pen.joinStyle()==Qt::BevelJoin)a+=QStringLiteral(" stroke-linejoin=\"bevel\"");
            else a+=QStringLiteral(" stroke-miterlimit=\"%1\"").arg(number(std::max(1.,pen.miterLimit())));
            if(pen.style()!=Qt::SolidLine){
                // Dashes are given in pen widths (one device unit for hairlines).
                const double unit=std::max(width,cosmetic?1.:width);QStringList dashes;
                for(double d:pen.dashPattern())dashes<<number(d*unit);
                if(!dashes.isEmpty()){
                    a+=QStringLiteral(" stroke-dasharray=\"%1\"").arg(dashes.join(u' '));
                    if(pen.dashOffset()!=0)a+=QStringLiteral(" stroke-dashoffset=\"%1\"").arg(number(pen.dashOffset()*unit));
                }
            }
        }
        element(QStringLiteral("<path d=\"%1\"%2/>").arg(pathData(p),a));
    }
};

class SvgDevice : public QPaintDevice {
public:
    explicit SvgDevice(QSizeF mm):size(mm){}
    QPaintEngine *paintEngine() const override{return &engine;}
    mutable SvgEngine engine;
protected:
    int metric(PaintDeviceMetric m) const override{
        switch(m){
        case PdmWidth:return std::max(1,int(std::ceil(size.width()*unitsPerMm)));
        case PdmHeight:return std::max(1,int(std::ceil(size.height()*unitsPerMm)));
        case PdmWidthMM:return std::max(1,int(std::ceil(size.width())));
        case PdmHeightMM:return std::max(1,int(std::ceil(size.height())));
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
};
}

QByteArray svgDrawing(QSizeF size,const QString &title,const std::function<void(QPainter&)> &draw){
    SvgDevice device(size);
    {QPainter painter(&device);painter.scale(unitsPerMm,unitsPerMm);draw(painter);}
    QString out=QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                               "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" version=\"1.1\" "
                               "width=\"%1mm\" height=\"%2mm\" viewBox=\"0 0 %3 %4\">\n")
                    .arg(number(size.width()),number(size.height()),number(size.width()*unitsPerMm),number(size.height()*unitsPerMm));
    if(!title.isEmpty())out+=QStringLiteral("<title>%1</title>\n").arg(escaped(title));
    if(!device.engine.defs.isEmpty())out+=QStringLiteral("<defs>\n")+device.engine.defs+QStringLiteral("</defs>\n");
    out+=device.engine.body+QStringLiteral("</svg>\n");
    return out.toUtf8();
}
QByteArray sheetSvg(const Document &document,int sheet,const RenderOptions &options){
    if(sheet<0||sheet>=document.sheets.size())return {};
    const Sheet &s=document.sheets[sheet];
    return svgDrawing(QSizeF(s.width,s.height),s.name,[&](QPainter &p){paintSheet(p,document,sheet,options);});
}
}
