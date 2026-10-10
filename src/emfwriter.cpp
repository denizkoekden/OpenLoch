#include "emfwriter.h"
#include <QGradient>
#include <QPaintEngine>
#include <QPainterPath>
#include <QPixmap>
#include <QTextItem>
#include <QtEndian>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>

// Enhanced Metafiles from QPainter output. All coordinates are written in device units of a hundredth of a
// millimetre; the header describes a reference device with 100 units per millimetre. The files use only records
// that common readers show alike: clipping is applied to the geometry here (areas are cut, lines are cut into
// pieces), gradients become bands of solid colour.
namespace openloch {
namespace {
// Record types; in their own namespace, since QPaintEngine has names of its own (AlphaBlend among them).
namespace emr {enum Record:quint32 {Header=1,PolyBezierTo=5,PolylineTo=6,Eof=14,SetBkMode=18,SetPolyFillMode=19,SetStretchBltMode=21,MoveToEx=27,SaveDc=33,RestoreDc=34,
    SetWorldTransform=35,ModifyWorldTransform=36,IntersectClipRect=30,SelectObject=37,CreatePen=38,CreateBrushIndirect=39,DeleteObject=40,BeginPath=59,EndPath=60,
    CloseFigure=61,FillPath=62,StrokeAndFillPath=63,StrokePath=64,SelectClipPath=67,StretchDiBits=81,ExtCreatePen=95,
    AlphaBlend=114};}
constexpr quint32 whiteBrush=0x80000000,nullBrush=0x80000005,blackPen=0x80000007,nullPen=0x80000008;
// Handles of the fill and the pen.
constexpr quint32 brushHandle=1,penHandle=2;
quint32 colorRef(const QColor &c){return quint32(c.red())|quint32(c.green())<<8|quint32(c.blue())<<16;}
qint32 unit(double v){return qint32(std::clamp<double>(std::round(v),-2e9,2e9));}
struct Bytes {
    QByteArray b;
    Bytes &u32(quint32 v){char c[4];qToLittleEndian(v,c);b.append(c,4);return *this;}
    Bytes &i32(qint32 v){char c[4];qToLittleEndian(v,c);b.append(c,4);return *this;}
    Bytes &u16(quint16 v){char c[2];qToLittleEndian(v,c);b.append(c,2);return *this;}
    Bytes &f32(float f){quint32 v;std::memcpy(&v,&f,4);return u32(v);}
    Bytes &point(QPointF p){return i32(unit(p.x())).i32(unit(p.y()));}
    Bytes &rect(const QRectF &r){return i32(unit(r.left())).i32(unit(r.top())).i32(unit(r.right())).i32(unit(r.bottom()));}
    Bytes &raw(const QByteArray &x){b+=x;return *this;}
};
quint32 penStyle(Qt::PenStyle s){switch(s){case Qt::DashLine:return 1;case Qt::DotLine:return 2;case Qt::DashDotLine:return 3;case Qt::DashDotDotLine:return 4;default:return 0;}}
bool opaque(const QImage &image){
    if(!image.hasAlphaChannel())return true;
    const QImage argb=image.convertToFormat(QImage::Format_ARGB32);
    for(int y=0;y<argb.height();y++){const auto *row=reinterpret_cast<const QRgb*>(argb.constScanLine(y));for(int x=0;x<argb.width();x++)if(qAlpha(row[x])<255)return false;}
    return true;
}
// The colour of a gradient at t (0…1) between its stops.
QColor gradientColour(const QGradientStops &stops,double t){
    if(stops.isEmpty())return Qt::black;
    if(t<=stops.first().first)return stops.first().second;
    for(int i=1;i<stops.size();i++){
        if(t>stops[i].first)continue;
        const double span=stops[i].first-stops[i-1].first,f=span>0?(t-stops[i-1].first)/span:0;const QColor a=stops[i-1].second,b=stops[i].second;
        return QColor::fromRgbF(float(a.redF()+(b.redF()-a.redF())*f),float(a.greenF()+(b.greenF()-a.greenF())*f),float(a.blueF()+(b.blueF()-a.blueF())*f));
    }
    return stops.last().second;
}
}

class EmfEngine : public QPaintEngine {
public:
    EmfEngine(QSizeF size,const QString &title):QPaintEngine(QPaintEngine::AllFeatures),size(size),title(title){}
    QByteArray file;
    bool smoothImages=false;
    bool begin(QPaintDevice *) override;
    bool end() override;
    void updateState(const QPaintEngineState &state) override;
    void drawPath(const QPainterPath &path) override{paint(path,true,true);}
    void drawPolygon(const QPointF *points,int count,PolygonDrawMode mode) override;
    void drawLines(const QLineF *lines,int count) override;
    void drawRects(const QRectF *rects,int count) override;
    void drawEllipse(const QRectF &r) override{QPainterPath path;path.addEllipse(r);paint(path,true,true);}
    void drawPoints(const QPointF *points,int count) override;
    void drawPixmap(const QRectF &r,const QPixmap &pixmap,const QRectF &source) override{drawImage(r,pixmap.toImage(),source,Qt::AutoColor);}
    void drawImage(const QRectF &r,const QImage &image,const QRectF &source,Qt::ImageConversionFlags flags) override;
    void drawTextItem(const QPointF &p,const QTextItem &item) override;
    Type type() const override{return QPaintEngine::User;}
private:
    QSizeF size;QString title;QByteArray records;quint32 count=0;
    QTransform transform;QPen pen;QBrush brush;
    bool smoothHint=false;int stretchMode=0;   // the painter's SmoothPixmapTransform; the stretch mode written last (0: none)
    bool clipEnabled=true,hasClip=false;QPainterPath clip;   // the clip in device units
    QString penKey,brushKey;   // what the handles hold, so that they are written only when they change
    double scale() const{return std::sqrt(std::abs(transform.determinant()));}
    void record(quint32 type,const Bytes &payload={});
    void fillMode(Qt::FillRule rule){record(emr::SetPolyFillMode,Bytes().u32(rule==Qt::WindingFill?2:1));}
    void writePath(const QPainterPath &devicePath);
    bool clipped() const{return clipEnabled&&hasClip;}
    void selectPen(const QPen &p);
    void selectBrush(const QColor *colour);
    void paint(const QPainterPath &userPath,bool fill,bool stroke);
    void fillSolid(const QPainterPath &deviceArea,const QColor &colour);
    void strokeLines(const QPainterPath &devicePath,const QPen &p);
    void gradientBands(const QPainterPath &deviceArea,const QBrush &b);
    void hatchLines(const QPainterPath &deviceArea,const QBrush &b);
};

void EmfEngine::record(quint32 type,const Bytes &payload){
    QByteArray data=payload.b;while(data.size()%4)data.append('\0');
    records+=Bytes().u32(type).u32(quint32(8+data.size())).b;records+=data;count++;
}
bool EmfEngine::begin(QPaintDevice *){
    records.clear();count=0;penKey.clear();brushKey.clear();transform=QTransform();pen=QPen();brush=QBrush();smoothHint=false;stretchMode=0;
    clipEnabled=true;hasClip=false;clip=QPainterPath();
    record(emr::SetBkMode,Bytes().u32(1));   // hatches and dashes without background
    return true;
}
bool EmfEngine::end(){
    record(emr::Eof,Bytes().u32(0).u32(16).u32(20));
    const qint32 w=unit(size.width()*100),h=unit(size.height()*100),mmW=qint32(std::ceil(size.width())),mmH=qint32(std::ceil(size.height()));
    const QString text=QStringLiteral("OpenLoch")+QChar(0)+title+QChar(0)+QChar(0);
    Bytes description;for(QChar c:text)description.u16(c.unicode());while(description.b.size()%4)description.b.append('\0');
    const quint32 headerSize=quint32(108+description.b.size());
    Bytes h1;
    h1.u32(emr::Header).u32(headerSize).i32(0).i32(0).i32(std::max(0,w-1)).i32(std::max(0,h-1)).i32(0).i32(0).i32(w).i32(h)   // bounds (device units), frame (1/100 mm)
      .u32(0x464D4520).u32(0x10000).u32(quint32(headerSize+records.size())).u32(count+1).u16(4).u16(0)
      .u32(quint32(text.size())).u32(108).u32(0)
      .i32(mmW*100).i32(mmH*100).i32(mmW).i32(mmH)   // the reference device: 100 units per millimetre
      .u32(0).u32(0).u32(0).i32(mmW*1000).i32(mmH*1000).raw(description.b);
    file=h1.b+records;
    return true;
}
void EmfEngine::updateState(const QPaintEngineState &s){
    const auto flags=s.state();
    if(flags&DirtyTransform)transform=s.transform();
    if(flags&DirtyPen)pen=s.pen();
    if(flags&DirtyBrush)brush=s.brush();
    if(flags&DirtyHints)smoothHint=s.renderHints().testFlag(QPainter::SmoothPixmapTransform);
    if(flags&(DirtyClipPath|DirtyClipRegion)){
        // A clip comes in the coordinates of the transformation that belongs to it.
        QPainterPath area;if(flags&DirtyClipPath)area=s.clipPath();else area.addRegion(s.clipRegion());
        area=s.transform().map(area);
        switch(s.clipOperation()){
        case Qt::NoClip:hasClip=false;clip=QPainterPath();break;
        case Qt::ReplaceClip:hasClip=true;clip=area;break;
        case Qt::IntersectClip:clip=hasClip?clip.intersected(area):area;hasClip=true;break;
        }
    }
    if(flags&DirtyClipEnabled)clipEnabled=s.isClipEnabled();
}
// Figures: a move to their start, then runs of lines and Bézier curves; a figure that ends where it began is closed.
void EmfEngine::writePath(const QPainterPath &path){
    record(emr::BeginPath);
    QPolygonF lines,curves;QPointF start,last;int segments=0;
    auto flush=[&]{
        if(!lines.isEmpty()){Bytes b;b.rect(lines.boundingRect()).u32(quint32(lines.size()));for(QPointF q:lines)b.point(q);record(emr::PolylineTo,b);lines.clear();}
        if(!curves.isEmpty()){Bytes b;b.rect(curves.boundingRect()).u32(quint32(curves.size()));for(QPointF q:curves)b.point(q);record(emr::PolyBezierTo,b);curves.clear();}
    };
    auto finish=[&]{flush();if(segments>0&&unit(last.x())==unit(start.x())&&unit(last.y())==unit(start.y()))record(emr::CloseFigure);segments=0;};
    for(int i=0;i<path.elementCount();i++){
        const QPainterPath::Element e=path.elementAt(i);const QPointF q(e.x,e.y);
        if(e.isMoveTo()){finish();start=last=q;record(emr::MoveToEx,Bytes().point(q));}
        else if(e.isLineTo()){if(!curves.isEmpty())flush();lines<<q;last=q;segments++;}
        else if(e.isCurveTo()&&i+2<path.elementCount()){
            if(!lines.isEmpty())flush();
            const QPainterPath::Element c2=path.elementAt(i+1),to=path.elementAt(i+2);curves<<q<<QPointF(c2.x,c2.y)<<QPointF(to.x,to.y);last=curves.last();i+=2;segments++;
        }
    }
    finish();
    record(emr::EndPath);
}
namespace {
// The parts of the lines of a path that lie inside an area: curves are flattened, every segment is cut where it
// crosses the area's outline, and the pieces whose middle is inside are kept.
QPainterPath linesInside(const QPainterPath &lines,const QPainterPath &area){
    QList<QLineF> edges;for(const QPolygonF &poly:area.toSubpathPolygons())for(int i=0;i<poly.size();i++)edges<<QLineF(poly[i],poly[(i+1)%poly.size()]);
    QPainterPath out;QPointF end;bool open=false;
    for(const QPolygonF &poly:lines.toSubpathPolygons())for(int i=0;i+1<poly.size();i++){
        const QLineF segment(poly[i],poly[i+1]);if(segment.length()<=0)continue;
        QList<double> cuts{0,1};
        for(const QLineF &edge:edges){QPointF x;if(segment.intersects(edge,&x)==QLineF::BoundedIntersection){
            const double t=QPointF::dotProduct(x-segment.p1(),segment.p2()-segment.p1())/(segment.length()*segment.length());if(t>0&&t<1)cuts<<t;}}
        std::sort(cuts.begin(),cuts.end());
        for(int k=0;k+1<cuts.size();k++){
            if(cuts[k+1]-cuts[k]<1e-12||!area.contains(segment.pointAt((cuts[k]+cuts[k+1])/2)))continue;
            const QPointF a=segment.pointAt(cuts[k]),b=segment.pointAt(cuts[k+1]);
            if(!open||QLineF(a,end).length()>1e-9)out.moveTo(a);
            out.lineTo(b);end=b;open=true;
        }
    }
    return out;
}
}
void EmfEngine::selectPen(const QPen &p){
    const bool none=p.style()==Qt::NoPen,cosmetic=p.isCosmetic()||p.widthF()<=0;const double width=cosmetic?0:std::max(1.0,p.widthF()*scale());
    const QString key=none?QStringLiteral("none"):QString("%1 %2 %3 %4 %5").arg(p.color().rgb()).arg(penStyle(p.style())).arg(width,0,'g',12).arg(int(p.capStyle())).arg(int(p.joinStyle()));
    if(key==penKey)return;
    if(!penKey.isEmpty()&&penKey!="none"){record(emr::SelectObject,Bytes().u32(blackPen));record(emr::DeleteObject,Bytes().u32(penHandle));}
    penKey=key;
    if(none){record(emr::SelectObject,Bytes().u32(nullPen));return;}
    if(cosmetic)record(emr::CreatePen,Bytes().u32(penHandle).u32(penStyle(p.style())).i32(0).i32(0).u32(colorRef(p.color())));
    else{
        const quint32 cap=p.capStyle()==Qt::FlatCap?0x200:p.capStyle()==Qt::SquareCap?0x100:0;
        const quint32 join=p.joinStyle()==Qt::BevelJoin?0x1000:(p.joinStyle()==Qt::MiterJoin||p.joinStyle()==Qt::SvgMiterJoin)?0x2000:0;
        record(emr::ExtCreatePen,Bytes().u32(penHandle).u32(0).u32(0).u32(0).u32(0).u32(0x10000|penStyle(p.style())|cap|join).u32(quint32(std::lround(width)))
            .u32(0).u32(colorRef(p.color())).u32(0).u32(0));
    }
    record(emr::SelectObject,Bytes().u32(penHandle));
}
void EmfEngine::selectBrush(const QColor *colour){
    const QString key=colour?QString::number(colour->rgb()):QStringLiteral("none");
    if(key==brushKey)return;
    if(!brushKey.isEmpty()&&brushKey!="none"){record(emr::SelectObject,Bytes().u32(whiteBrush));record(emr::DeleteObject,Bytes().u32(brushHandle));}
    brushKey=key;
    if(!colour){record(emr::SelectObject,Bytes().u32(nullBrush));return;}
    record(emr::CreateBrushIndirect,Bytes().u32(brushHandle).u32(0).u32(colorRef(*colour)).u32(0));record(emr::SelectObject,Bytes().u32(brushHandle));
}
void EmfEngine::fillSolid(const QPainterPath &area,const QColor &colour){
    if(area.isEmpty())return;
    selectBrush(&colour);selectPen(QPen(Qt::NoPen));fillMode(area.fillRule());writePath(area);record(emr::FillPath,Bytes().rect(area.boundingRect()));
}
void EmfEngine::strokeLines(const QPainterPath &path,const QPen &p){
    if(path.isEmpty()||p.style()==Qt::NoPen)return;
    selectBrush(nullptr);selectPen(p);writePath(path);record(emr::StrokePath,Bytes().rect(path.boundingRect()));
}
void EmfEngine::paint(const QPainterPath &user,bool fill,bool stroke){
    if(user.isEmpty())return;
    const QPainterPath path=transform.map(user);const bool cut=clipped();
    const bool filled=fill&&brush.style()!=Qt::NoBrush,outline=stroke&&pen.style()!=Qt::NoPen;
    // Unclipped solid shapes with an outline keep one record for fill and stroke.
    if(!cut&&filled&&outline&&brush.style()==Qt::SolidPattern){
        const QColor colour=brush.color();selectBrush(&colour);selectPen(pen);fillMode(path.fillRule());writePath(path);
        record(emr::StrokeAndFillPath,Bytes().rect(path.boundingRect()));return;
    }
    if(filled){
        QPainterPath area=path;if(cut){area=path.intersected(clip);area.setFillRule(Qt::WindingFill);}
        switch(brush.style()){
        case Qt::SolidPattern:fillSolid(area,brush.color());break;
        case Qt::LinearGradientPattern:case Qt::RadialGradientPattern:case Qt::ConicalGradientPattern:gradientBands(area,brush);break;
        case Qt::HorPattern:case Qt::VerPattern:case Qt::CrossPattern:case Qt::BDiagPattern:case Qt::FDiagPattern:case Qt::DiagCrossPattern:hatchLines(area,brush);break;
        case Qt::TexturePattern:{
            // A picture repeated as fill: its mean colour.
            const QImage tile=brush.textureImage().scaled(1,1,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);fillSolid(area,tile.isNull()?brush.color():tile.pixelColor(0,0));break;}
        default:fillSolid(area,brush.color());break;
        }
    }
    if(outline)strokeLines(cut?linesInside(path,clip):path,pen);
}
// A gradient as bands of solid colour across its direction, each cut to the area; about one band per two steps of
// the largest colour change, at most 128. Radial and conical gradients get the colour of their middle.
void EmfEngine::gradientBands(const QPainterPath &area,const QBrush &b){
    const QRectF box=area.boundingRect();if(box.isEmpty()||!b.gradient())return;
    const QGradient *g=b.gradient();const QGradientStops stops=g->stops();
    if(g->type()!=QGradient::LinearGradient){fillSolid(area,gradientColour(stops,0.5));return;}
    const auto *lg=static_cast<const QLinearGradient*>(g);const QTransform toDevice=b.transform()*transform;
    const QPointF from=toDevice.map(lg->start()),to=toDevice.map(lg->finalStop());const double length=QLineF(from,to).length();
    if(length<1e-9){fillSolid(area,gradientColour(stops,1));return;}
    const QPointF u=(to-from)/length,n(-u.y(),u.x());
    double lo=1e300,hi=-1e300,side=0;
    for(QPointF c:{box.topLeft(),box.topRight(),box.bottomLeft(),box.bottomRight()}){const double t=QPointF::dotProduct(c-from,u);lo=std::min(lo,t);hi=std::max(hi,t);
        side=std::max(side,std::abs(QPointF::dotProduct(c-from,n)));}
    side+=1;
    int change=0;for(int i=1;i<stops.size();i++){const QColor a=stops[i-1].second,c=stops[i].second;
        change=std::max({change,std::abs(a.red()-c.red()),std::abs(a.green()-c.green()),std::abs(a.blue()-c.blue())});}
    const int bands=std::clamp(change/2+1,1,128);
    // Each band reaches to the far end and the next one covers the rest, so that no seam shows between them.
    const double c=std::max(length,hi)+1;
    for(int k=0;k<bands;k++){
        const double a=k==0?std::min(0.0,lo)-1:length*k/bands;
        if(a>hi||length*(k+1)/bands<lo)continue;
        QPainterPath strip;strip.addPolygon(QPolygonF{from+u*a-n*side,from+u*c-n*side,from+u*c+n*side,from+u*a+n*side});strip.closeSubpath();
        fillSolid(area.intersected(strip),gradientColour(stops,(k+0.5)/bands));
    }
}
// Pattern brushes are a few device pixels wide, a few hundredths of a millimetre here: lines a millimetre apart instead.
void EmfEngine::hatchLines(const QPainterPath &area,const QBrush &b){
    const QRectF r=area.boundingRect();if(r.isEmpty())return;
    const Qt::BrushStyle s=b.style();const double pitch=100;QPainterPath lines;
    auto line=[&](QPointF p,QPointF q){lines.moveTo(p);lines.lineTo(q);};
    if(s==Qt::HorPattern||s==Qt::CrossPattern)for(double y=std::floor(r.top()/pitch)*pitch;y<=r.bottom();y+=pitch)line({r.left(),y},{r.right(),y});
    if(s==Qt::VerPattern||s==Qt::CrossPattern)for(double x=std::floor(r.left()/pitch)*pitch;x<=r.right();x+=pitch)line({x,r.top()},{x,r.bottom()});
    const double step=pitch*std::sqrt(2.0);
    if(s==Qt::FDiagPattern||s==Qt::DiagCrossPattern)for(double c=std::floor((r.left()-r.bottom())/step)*step;c<=r.right()-r.top();c+=step)line({c+r.top(),r.top()},{c+r.bottom(),r.bottom()});
    if(s==Qt::BDiagPattern||s==Qt::DiagCrossPattern)for(double c=std::floor((r.left()+r.top())/step)*step;c<=r.right()+r.bottom();c+=step)line({c-r.top(),r.top()},{c-r.bottom(),r.bottom()});
    QPen hatch(b.color(),0);hatch.setCosmetic(true);strokeLines(linesInside(lines,area),hatch);
}
void EmfEngine::drawPolygon(const QPointF *points,int count,PolygonDrawMode mode){
    if(count<2)return;
    QPainterPath path;path.moveTo(points[0]);for(int i=1;i<count;i++)path.lineTo(points[i]);
    if(mode==PolylineMode){paint(path,false,true);return;}
    path.closeSubpath();path.setFillRule(mode==WindingMode?Qt::WindingFill:Qt::OddEvenFill);paint(path,true,true);
}
void EmfEngine::drawLines(const QLineF *lines,int count){
    QPainterPath path;for(int i=0;i<count;i++){path.moveTo(lines[i].p1());path.lineTo(lines[i].p2());}
    paint(path,false,true);
}
void EmfEngine::drawRects(const QRectF *rects,int count){QPainterPath path;for(int i=0;i<count;i++)path.addRect(rects[i]);paint(path,true,true);}
void EmfEngine::drawPoints(const QPointF *points,int count){
    // A point is a dot of the pen: a very short line with round ends.
    QPainterPath path;const double tiny=0.5/std::max(1e-9,scale());
    for(int i=0;i<count;i++){path.moveTo(points[i]);path.lineTo(points[i]+QPointF(tiny,0));}
    paint(path,false,true);
}
void EmfEngine::drawTextItem(const QPointF &p,const QTextItem &item){
    QPainterPath path;path.addText(p,item.font(),item.text());
    const QBrush keepBrush=brush;const QPen keepPen=pen;brush=QBrush(pen.color());pen=QPen(Qt::NoPen);paint(path,true,false);brush=keepBrush;pen=keepPen;
}
// The image under a world transformation that maps its pixels onto the target; opaque images as 24-bit bitmaps,
// others with premultiplied alpha for an alpha blend.
void EmfEngine::drawImage(const QRectF &r,const QImage &image,const QRectF &source,Qt::ImageConversionFlags){
    const QRect wanted=source.toAlignedRect().intersected(image.rect());if(image.isNull()||r.isEmpty()||wanted.isEmpty())return;
    QImage part=wanted==image.rect()?image:image.copy(wanted);
    const bool alpha=!opaque(part);
    if(smoothImages){
        if(alpha&&smoothHint){   // an alpha blend is never smoothed: enlarge it here to ten pixels per millimetre
            auto pixels=[&](QPointF a,QPointF b){return std::min(2048,int(std::ceil(QLineF(transform.map(a),transform.map(b)).length()/10)));};
            const int w=std::max(part.width(),pixels(r.topLeft(),r.topRight())),h=std::max(part.height(),pixels(r.topLeft(),r.bottomLeft()));
            if(w>part.width()||h>part.height())part=part.scaled(w,h,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        }
        const int mode=smoothHint?4:3;   // HALFTONE or COLORONCOLOR
        if(!alpha&&mode!=stretchMode){record(emr::SetStretchBltMode,Bytes().u32(quint32(mode)));stretchMode=mode;}
    }
    // Bitmaps cannot be cut here; under a clip they get a clip rectangle (or, for other shapes, a clip path).
    const bool cut=clipped();
    if(cut){
        record(emr::SaveDc);const QRectF bounds=clip.boundingRect();QPainterPath rectangle;rectangle.addRect(bounds);
        if(clip.subtracted(rectangle).isEmpty()&&rectangle.subtracted(clip).isEmpty())record(emr::IntersectClipRect,Bytes().rect(bounds));
        else{fillMode(clip.fillRule());writePath(clip);record(emr::SelectClipPath,Bytes().u32(1));}
    }
    const int w=part.width(),h=part.height();
    const QTransform map=QTransform::fromScale(r.width()/w,r.height()/h)*QTransform::fromTranslate(r.x(),r.y())*transform;
    record(emr::SetWorldTransform,Bytes().f32(float(map.m11())).f32(float(map.m12())).f32(float(map.m21())).f32(float(map.m22())).f32(float(map.dx())).f32(float(map.dy())));
    QByteArray bits;
    if(alpha){
        const QImage premultiplied=part.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        for(int y=h-1;y>=0;y--)for(int x=0;x<w;x++)bits+=Bytes().u32(reinterpret_cast<const quint32*>(premultiplied.constScanLine(y))[x]).b;
    }else{
        const QImage rgb=part.convertToFormat(QImage::Format_RGB32);const int pad=(4-(w*3)%4)%4;
        for(int y=h-1;y>=0;y--){const auto *row=reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
            for(int x=0;x<w;x++){bits.append(char(qBlue(row[x])));bits.append(char(qGreen(row[x])));bits.append(char(qRed(row[x])));}bits.append(QByteArray(pad,0));}
    }
    const quint16 depth=alpha?32:24;
    Bytes info;info.u32(40).i32(w).i32(h).u16(1).u16(depth).u32(0).u32(quint32(bits.size())).i32(0).i32(0).u32(0).u32(0);
    const QRectF bounds=transform.mapRect(r);
    if(alpha){
        Bytes b;b.rect(bounds).i32(0).i32(0).i32(w).i32(h).u32(0x01ff0000).i32(0).i32(0).f32(1).f32(0).f32(0).f32(1).f32(0).f32(0).u32(0).u32(0)
            .u32(108).u32(40).u32(148).u32(quint32(bits.size())).i32(w).i32(h).raw(info.b).raw(bits);
        record(emr::AlphaBlend,b);
    }else{
        Bytes b;b.rect(bounds).i32(0).i32(0).i32(0).i32(0).i32(w).i32(h).u32(80).u32(40).u32(120).u32(quint32(bits.size())).u32(0).u32(0x00CC0020).i32(w).i32(h).raw(info.b).raw(bits);
        record(emr::StretchDiBits,b);
    }
    record(emr::ModifyWorldTransform,Bytes().f32(1).f32(0).f32(0).f32(1).f32(0).f32(0).u32(1));
    if(cut)record(emr::RestoreDc,Bytes().i32(-1));
}

EmfDevice::EmfDevice(QSizeF s,const QString &title):size(s),engine(std::make_unique<EmfEngine>(s,title)){}
EmfDevice::~EmfDevice()=default;
QPaintEngine *EmfDevice::paintEngine() const{return engine.get();}
QByteArray EmfDevice::data() const{return engine->file;}
void EmfDevice::setSmoothImages(bool on){engine->smoothImages=on;}
int EmfDevice::metric(PaintDeviceMetric m) const{
    switch(m){
    case PdmWidth:return std::max(1,int(std::lround(size.width()*100)));
    case PdmHeight:return std::max(1,int(std::lround(size.height()*100)));
    case PdmWidthMM:return std::max(1,int(std::lround(size.width())));
    case PdmHeightMM:return std::max(1,int(std::lround(size.height())));
    case PdmDpiX:case PdmDpiY:case PdmPhysicalDpiX:case PdmPhysicalDpiY:return 2540;
    case PdmNumColors:return INT_MAX;
    case PdmDepth:return 32;
    default:return QPaintDevice::metric(m);
    }
}
}
