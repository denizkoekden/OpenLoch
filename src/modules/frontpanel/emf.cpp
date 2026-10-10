#include "emf.h"
#include "panelgeometry.h"
#include <QFontMetricsF>
#include <QHash>
#include <QImage>
#include <QLinearGradient>
#include <QPainterPathStroker>
#include <QPaintEngine>
#include <QPainterPath>
#include <QRegion>
#include <QtEndian>
#include <array>
#include <cmath>
#include <cstring>
#include <optional>

namespace openloch::frontpanel {
namespace {
enum Record:quint32 {Header=1,PolyBezier=2,Polygon=3,Polyline=4,PolyBezierTo=5,PolylineTo=6,PolyPolyline=7,PolyPolygon=8,SetWindowExt=9,
    SetWindowOrg=10,SetViewportExt=11,SetViewportOrg=12,Eof=14,SetMapMode=17,SetBkMode=18,SetPolyFillMode=19,SetStretchBltMode=21,SetTextAlign=22,SetTextColor=24,
    SetBkColor=25,MoveTo=27,ExcludeClipRect=29,IntersectClipRect=30,SaveDc=33,RestoreDc=34,SetWorldTransform=35,ModifyWorldTransform=36,
    SelectObject=37,CreatePen=38,CreateBrush=39,DeleteObject=40,AngleArc=41,Ellipse=42,Rectangle=43,RoundRect=44,Arc=45,Chord=46,Pie=47,
    LineTo=54,ArcTo=55,SetArcDirection=57,BeginPath=59,EndPath=60,CloseFigure=61,FillPath=62,StrokeAndFillPath=63,StrokePath=64,
    SelectClipPath=67,AbortPath=68,FillRgn=71,FrameRgn=72,InvertRgn=73,PaintRgn=74,ExtSelectClipRgn=75,BitBlt=76,StretchBlt=77,MaskBlt=78,PlgBlt=79,SetDiBitsToDevice=80,StretchDiBits=81,ExtCreateFontW=82,ExtTextOutA=83,
    ExtTextOutW=84,PolyBezier16=85,Polygon16=86,Polyline16=87,PolyBezierTo16=88,PolylineTo16=89,PolyPolyline16=90,PolyPolygon16=91,
    CreateDibPatternBrush=94,ExtCreatePen=95,SmallTextOut=108,AlphaBlend=114,TransparentBlt=116,GradientFill=118};
struct Font {QString face;double height=0,escapement=0;int weight=400;bool italic=false,underline=false,strikeOut=false;};
struct Object {enum Kind {Pen,Brush,Typeface} kind=Brush;QPen pen;QBrush brush;Font font;};
struct State {
    int mapMode=1;QPointF windowOrg,viewportOrg;QSizeF windowExt{1,1},viewportExt{1,1};Qt::FillRule fill=Qt::OddEvenFill;
    QPen pen{Qt::black,0};QBrush brush{Qt::white};QPointF position;
    Font font;QColor textColor{Qt::black},background{Qt::white};int textAlign=0;bool opaque=true;
    int stretchMode=1;   // SetStretchBltMode: BLACKONWHITE by default, HALFTONE (4) the only one that smooths
    bool clipped=false;QPainterPath clip;   // in device units
    QTransform world;bool clockwise=false;  // world transformation before the window mapping; direction of arcs
};
QColor colorRef(quint32 c){return QColor(c&0xff,(c>>8)&0xff,(c>>16)&0xff);}
// A raster operation as one of the 16 ways to combine what is drawn (a bitmap or the brush) with what lies below: bit
// (2 × drawn + below) of the table is the result. Operations that need bitmap and brush together count as copying the
// bitmap.
int combination(quint32 rop,bool *withBrush){
    const int i=int((rop>>16)&0xff);const bool pattern=((i>>4)&0x0f)!=(i&0x0f),source=((i>>2)&0x33)!=(i&0x33);
    *withBrush=pattern&&!source;
    if(pattern&&source)return 0xc;
    return pattern?(i&0x3)|((i>>2)&0xc):i&0xf;
}
const QPainter::CompositionMode rasterModes[16]={QPainter::RasterOp_ClearDestination,QPainter::RasterOp_NotSourceAndNotDestination,
    QPainter::RasterOp_NotSourceAndDestination,QPainter::RasterOp_NotSource,QPainter::RasterOp_SourceAndNotDestination,QPainter::RasterOp_NotDestination,
    QPainter::RasterOp_SourceXorDestination,QPainter::RasterOp_NotSourceOrNotDestination,QPainter::RasterOp_SourceAndDestination,
    QPainter::RasterOp_NotSourceXorDestination,QPainter::CompositionMode_DestinationOver,QPainter::RasterOp_NotSourceOrDestination,
    QPainter::CompositionMode_SourceOver,QPainter::RasterOp_SourceOrNotDestination,QPainter::RasterOp_SourceOrDestination,QPainter::RasterOp_SetDestination};
// The set (or clear) bits of a monochrome mask as a region in its pixels. As in a monochrome bitmap of Windows, white
// pixels are set, whatever the order of the mask's colour table.
QRegion maskRegion(const QImage &mask,bool set){
    QList<QRect> runs;
    for(int y=0;y<mask.height();y++)for(int x=0;x<mask.width();){
        auto on=[&](int at){return (qGray(mask.pixel(at,y))>127)==set;};
        if(!on(x)){x++;continue;}
        int end=x+1;while(end<mask.width()&&on(end))end++;runs.append(QRect(x,y,end-x,1));x=end;
    }
    QRegion region;if(!runs.isEmpty())region.setRects(runs.constData(),int(runs.size()));return region;
}
Qt::BrushStyle hatch(quint32 h){switch(h){case 0:return Qt::HorPattern;case 1:return Qt::VerPattern;case 2:return Qt::FDiagPattern;case 3:return Qt::BDiagPattern;case 4:return Qt::CrossPattern;default:return Qt::DiagCrossPattern;}}
std::optional<Object> stock(quint32 index){
    Object o;
    switch(index&0x7fffffff){
    case 0:o.brush=QBrush(Qt::white);break;case 1:o.brush=QBrush(QColor(192,192,192));break;case 2:o.brush=QBrush(QColor(128,128,128));break;
    case 3:o.brush=QBrush(QColor(64,64,64));break;case 4:o.brush=QBrush(Qt::black);break;case 5:o.brush=QBrush(Qt::NoBrush);break;
    case 6:o.kind=Object::Pen;o.pen=QPen(Qt::white,0);break;case 7:o.kind=Object::Pen;o.pen=QPen(Qt::black,0);break;case 8:o.kind=Object::Pen;o.pen=QPen(Qt::NoPen);break;
    case 10:case 11:case 12:case 13:case 14:case 16:case 17:o.kind=Object::Typeface;o.font.face=QStringLiteral("Arial");break;
    default:return std::nullopt;
    }
    return o;
}
}

bool isEmf(const QByteArray &b){return b.size()>=88&&qFromLittleEndian<quint32>(b.constData())==Header&&b.mid(40,4)==" EMF";}

bool paintEmf(QPainter &p,const QByteArray &b,const QPolygonF &frame){
    if(!isEmf(b)||frame.size()<3)return false;
    auto u32=[&](qsizetype at){return qFromLittleEndian<quint32>(b.constData()+at);};
    auto i32=[&](qsizetype at){return qFromLittleEndian<qint32>(b.constData()+at);};
    auto i16=[&](qsizetype at){return qFromLittleEndian<qint16>(b.constData()+at);};
    // The picture's nominal area: its frame in hundredths of a millimetre, converted to device units; else its bounds.
    const QRectF bounds(QPointF(i32(8),i32(12)),QPointF(i32(16)+1,i32(20)+1));
    const double devicePerMmX=i32(80)>0?double(i32(72))/i32(80):1,devicePerMmY=i32(84)>0?double(i32(76))/i32(84):1;
    QRectF area(QPointF(i32(24)*devicePerMmX/100,i32(28)*devicePerMmY/100),QPointF(i32(32)*devicePerMmX/100,i32(36)*devicePerMmY/100));
    if(area.width()<=0||area.height()<=0)area=bounds;
    if(area.width()<=0||area.height()<=0)return true;
    p.save();
    const QTransform base=QTransform::fromTranslate(-area.left(),-area.top())*QTransform::fromScale(1/area.width(),1/area.height())*frameMap(frame,QSizeF(1,1))*p.transform();
    p.setTransform(base);p.setRenderHint(QPainter::Antialiasing);
    State s;QList<State> saved;QHash<quint32,Object> objects;
    // A path bracket collects the drawing between BEGINPATH and ENDPATH instead of painting it.
    bool inPath=false;QPainterPath figure;
    auto device=[&](QPointF logical)->QPointF{
        const QPointF l=s.world.map(logical);
        if(s.mapMode==7||s.mapMode==8){const double sx=s.windowExt.width()?s.viewportExt.width()/s.windowExt.width():1,sy=s.windowExt.height()?s.viewportExt.height()/s.windowExt.height():1;
            return QPointF((l.x()-s.windowOrg.x())*sx+s.viewportOrg.x(),(l.y()-s.windowOrg.y())*sy+s.viewportOrg.y());}
        if(s.mapMode>=2&&s.mapMode<=6){static const double perUnit[]={0,0,0.1,0.01,0.254,0.0254,0.0254/1.44};const double mm=perUnit[s.mapMode];
            return QPointF((l.x()-s.windowOrg.x())*mm*devicePerMmX+s.viewportOrg.x(),-(l.y()-s.windowOrg.y())*mm*devicePerMmY+s.viewportOrg.y());}
        return l-s.windowOrg+s.viewportOrg;
    };
    auto length=[&](double logical,bool vertical){return std::abs(QLineF(device(QPointF()),device(vertical?QPointF(0,logical):QPointF(logical,0))).length());};
    auto points=[&](qsizetype at,quint32 n,bool small,qsizetype end){
        QPolygonF poly;const qsizetype step=small?4:8;if(n>quint32((end-at)/step))return poly;
        for(quint32 i=0;i<n;i++){const qsizetype q=at+i*step;poly<<device(small?QPointF(i16(q),i16(q+2)):QPointF(i32(q),i32(q+4)));}
        return poly;
    };
    auto rectAt=[&](qsizetype at){return QRectF(device(QPointF(i32(at),i32(at+4))),device(QPointF(i32(at+8),i32(at+12)))).normalized();};
    auto stroke=[&](const QPainterPath &path){if(inPath){figure.addPath(path);return;}if(s.pen.style()!=Qt::NoPen)p.strokePath(path,s.pen);};
    auto fillAndStroke=[&](QPainterPath path){if(inPath){figure.addPath(path);return;}path.setFillRule(s.fill);if(s.brush.style()!=Qt::NoBrush)p.fillPath(path,s.brush);stroke(path);};
    auto pointAt=[&](qsizetype at){return device(QPointF(i32(at),i32(at+4)));};
    auto real=[&](qsizetype at){const quint32 bits=u32(at);float f;std::memcpy(&f,&bits,4);return double(f);};
    // Lines that go on from the current position: in a path bracket they continue the figure. They are added one by
    // one, since QPainterPath::connectPath drops a figure's opening move and would join it to the figure before.
    auto onwards=[&](const QPainterPath &tail){
        if(!inPath){stroke(tail);return;}
        if(figure.elementCount()==0||figure.currentPosition()!=s.position)figure.moveTo(s.position);
        for(int i=1;i<tail.elementCount();i++){const QPainterPath::Element e=tail.elementAt(i);
            if(e.isCurveTo()&&i+2<tail.elementCount()){figure.cubicTo(e,tail.elementAt(i+1),tail.elementAt(i+2));i+=2;}
            else if(e.isMoveTo())figure.moveTo(e);
            else figure.lineTo(e);}
    };
    // An arc of the ellipse in `box` from the radial through `from` to the radial through `to`, counter-clockwise on the
    // screen unless the arc direction says otherwise; equal radials give the whole ellipse.
    auto arc=[&](const QRectF &box,QPointF from,QPointF to,double *start,double *sweep){
        const QPointF c=box.center();const double rx=std::max(box.width()/2,1e-9),ry=std::max(box.height()/2,1e-9);
        auto angle=[&](QPointF q){return std::atan2(-(q.y()-c.y())/ry,(q.x()-c.x())/rx)*180/3.14159265358979323846;};
        *start=angle(from);double span=angle(to)-*start;
        if(s.clockwise){if(span>=0)span-=360;}else if(span<=0)span+=360;
        *sweep=span;
    };
    // Clipping lives in the painter state; it is set up again from the outer state whenever it changes.
    auto applyClip=[&]{p.restore();p.save();p.setTransform(base);p.setRenderHint(QPainter::Antialiasing);if(s.clipped)p.setClipPath(s.clip,Qt::IntersectClip);};
    auto clipWith=[&](const QPainterPath &region,int mode){
        // Modes as in the format: 1 and, 2 or, 3 xor, 4 difference, 5 copy.
        QPainterPath everything;everything.addRect(QRectF(-1e7,-1e7,2e7,2e7));const QPainterPath current=s.clipped?s.clip:everything;
        switch(mode){case 1:s.clip=current.intersected(region);break;case 2:s.clip=current.united(region);break;case 3:s.clip=current.united(region).subtracted(current.intersected(region));break;
            case 4:s.clip=current.subtracted(region);break;default:s.clip=region;break;}
        s.clipped=true;applyClip();
    };
    auto drawText=[&](QPointF reference,const QString &text,const QList<double> &advances){
        if(text.isEmpty())return;
        QFont font(s.font.face.isEmpty()?QStringLiteral("Arial"):s.font.face);font.setPixelSize(100);font.setBold(s.font.weight>=600);font.setItalic(s.font.italic);
        font.setUnderline(s.font.underline);font.setStrikeOut(s.font.strikeOut);
        const QFontMetricsF m(font);
        // A negative height is the character height, a positive one the cell height with internal leading.
        const double h=s.font.height!=0?length(s.font.height,true):16,scale=s.font.height<0?h/100:h/std::max(1.0,m.ascent()+m.descent());
        const double natural=std::max(m.horizontalAdvance(text),1.0);double stretch=1;
        if(!advances.isEmpty()){double total=0;for(double a:advances)total+=a;const double wanted=length(total,false)/scale;if(wanted>0)stretch=wanted/natural;}
        const int horizontal=s.textAlign&6,vertical=s.textAlign&24;
        const double dx=horizontal==2?-natural*stretch:horizontal==6?-natural*stretch/2:0,dy=vertical==0?m.ascent():vertical==8?-m.descent():0;
        QPainterPath path;path.addText(0,0,font,text);
        const QTransform t=QTransform::fromScale(stretch,1)*QTransform::fromTranslate(dx,dy)*QTransform::fromScale(scale,scale)*QTransform().rotate(-s.font.escapement/10)
            *QTransform::fromTranslate(reference.x(),reference.y());
        // Inside a path bracket the outlines join the figure.
        if(inPath)figure.addPath(t.map(path));else p.fillPath(t.map(path),s.textColor);
        if(s.textAlign&1)s.position=t.map(QPointF(dx+natural*stretch,0));
    };
    // A device independent bitmap of the record as an image: its header and bits with a file header in front.
    // `rows` replaces the height of the header for records that hold only some scan lines.
    auto bitmap=[&](qsizetype at,quint32 offHeader,quint32 headerSize,quint32 offBits,quint32 bitsSize,qsizetype end,qint32 rows=0)->QImage{
        if(headerSize<12||at+offHeader+headerSize>end||at+offBits+bitsSize>end)return {};
        QByteArray file("BM");char word[4];
        qToLittleEndian(quint32(14+headerSize+bitsSize),word);file.append(word,4);file.append(QByteArray(4,0));qToLittleEndian(quint32(14+headerSize),word);file.append(word,4);
        QByteArray header=b.mid(at+offHeader,headerSize);
        if(rows!=0&&headerSize>=40){qToLittleEndian(rows,header.data()+8);qToLittleEndian(quint32(0),header.data()+20);}
        file+=header;file+=b.mid(at+offBits,bitsSize);
        return QImage::fromData(file,"BMP");
    };
    // Whether a device independent bitmap stores its rows from the top (a negative height); rows count from the bottom
    // otherwise.
    auto topDown=[&](qsizetype at,quint32 offHeader,quint32 headerSize,qsizetype end){return headerSize>=40&&at+offHeader+12<=end&&i32(at+offHeader+8)<0;};
    // Raster operations act as on Windows where the device has them (screen, images); elsewhere (printers, PDF) AND
    // multiplies, OR lightens, XOR takes the difference, an inverted bitmap is inverted first, the rest is copied.
    const bool rasterOps=p.paintEngine()&&p.paintEngine()->hasFeature(QPaintEngine::RasterOpModes);
    auto combineWith=[&](int table){
        if(rasterOps)p.setCompositionMode(rasterModes[table]);
        else if(table==0x8)p.setCompositionMode(QPainter::CompositionMode_Multiply);
        else if(table==0xe)p.setCompositionMode(QPainter::CompositionMode_Lighten);
        else if(table==0x6)p.setCompositionMode(QPainter::CompositionMode_Difference);
    };
    // An area treated by a raster operation that leaves the bitmap out: black, white, inverted or with the brush.
    auto fillWith=[&](const QPainterPath &area,int table,QBrush brush){
        if(table==0xa)return;
        p.save();
        if(table==0x0||table==0xf)p.fillPath(area,table?Qt::white:Qt::black);
        else if(table==0x5){p.setCompositionMode(QPainter::CompositionMode_Difference);p.fillPath(area,Qt::white);}
        else if(brush.style()!=Qt::NoBrush){
            if(table==0x3&&!rasterOps){const QColor c=brush.color();brush.setColor(QColor(255-c.red(),255-c.green(),255-c.blue()));table=0xc;}
            combineWith(table);p.fillPath(area,brush);
        }
        p.restore();
    };
    // A bitmap onto the parallelogram of its destination corners (a world transformation may turn or slant it), combined
    // with what lies below by the raster operation. A monochrome mask of the source's size limits it to its set bits
    // (or, with `clear`, to the others). Like GDI, only the halftone stretch mode smooths a scaled bitmap, AlphaBlend
    // never: the other modes show its pixels.
    auto blit=[&](const QImage &image,QPointF topLeft,QPointF topRight,QPointF bottomLeft,QRect source,quint32 rop,double opacity=1,const QImage &mask={},bool clear=false,bool alphaBlend=false){
        bool withBrush=false;const int table=combination(rop,&withBrush);
        if(table==0xa)return;
        if(table==0x0||table==0xf||table==0x5||withBrush){
            if(!mask.isNull())return;
            QPainterPath area;area.addPolygon(QPolygonF{topLeft,topRight,topRight+bottomLeft-topLeft,bottomLeft});area.closeSubpath();fillWith(area,table,s.brush);return;
        }
        if(image.isNull())return;
        QImage part=source.isValid()?image.copy(source):image;if(part.isNull())return;
        const QTransform onto(topRight.x()-topLeft.x(),topRight.y()-topLeft.y(),bottomLeft.x()-topLeft.x(),bottomLeft.y()-topLeft.y(),topLeft.x(),topLeft.y());
        if(std::abs(onto.determinant())<1e-12)return;
        if(table==0x3&&!rasterOps){part=part.convertToFormat(QImage::Format_RGB32);part.invertPixels();}
        p.save();
        p.setTransform(QTransform::fromScale(1.0/part.width(),1.0/part.height())*onto*p.transform());
        if(!mask.isNull()){const QRegion where=maskRegion(mask.scaled(part.size()),!clear);if(where.isEmpty()){p.restore();return;}p.setClipRegion(where,Qt::IntersectClip);}
        if(!(table==0x3&&!rasterOps))combineWith(table);
        p.setOpacity(opacity);p.setRenderHint(QPainter::SmoothPixmapTransform,s.stretchMode==4&&!alphaBlend);p.drawImage(0,0,part);p.restore();
    };
    // Regions of the drawing records: a header of 32 bytes, then rectangles in logical units.
    auto regionAt=[&](qsizetype region,quint32 dataSize,qsizetype end)->QPainterPath{
        QPainterPath path;if(dataSize<32||region+dataSize>end)return path;const quint32 count=u32(region+8);if(count>(dataSize-32)/16)return path;
        for(quint32 k=0;k<count&&k<2000;k++){const qsizetype r=region+32+k*16;path.addPolygon(QPolygonF{device(QPointF(i32(r),i32(r+4))),device(QPointF(i32(r+8),i32(r+4))),
            device(QPointF(i32(r+8),i32(r+12))),device(QPointF(i32(r),i32(r+12)))});path.closeSubpath();}
        return path.simplified();
    };
    auto brushOf=[&](quint32 index)->QBrush{
        if(index&0x80000000){const auto o=stock(index);return o&&o->kind==Object::Brush?o->brush:QBrush();}
        const auto it=objects.constFind(index);return it!=objects.constEnd()&&it->kind==Object::Brush?it->brush:QBrush();
    };
    // A triangle whose corners have their own colours. When all colours lie on one line from the first, the colour
    // changes along one direction and a linear gradient shows it exactly; otherwise small triangles approximate it.
    auto shade=[&](const std::array<QPointF,3> &corner,const std::array<QColor,3> &colour){
        const QPolygonF triangle{corner[0],corner[1],corner[2]};
        const double ux=corner[1].x()-corner[0].x(),uy=corner[1].y()-corner[0].y(),vx=corner[2].x()-corner[0].x(),vy=corner[2].y()-corner[0].y(),det=ux*vy-uy*vx;
        if(std::abs(det)<1e-12)return;
        // The spatial gradient of each channel.
        double g[3][2];const double c0[3]{colour[0].redF(),colour[0].greenF(),colour[0].blueF()};
        for(int k=0;k<3;k++){const double c1=k==0?colour[1].redF():k==1?colour[1].greenF():colour[1].blueF(),c2=k==0?colour[2].redF():k==1?colour[2].greenF():colour[2].blueF();
            const double d1=c1-c0[k],d2=c2-c0[k];g[k][0]=(d1*vy-d2*uy)/det;g[k][1]=(ux*d2-vx*d1)/det;}
        int main=0;for(int k=1;k<3;k++)if(std::hypot(g[k][0],g[k][1])>std::hypot(g[main][0],g[main][1]))main=k;
        const double wx=g[main][0],wy=g[main][1],w2=wx*wx+wy*wy;bool linear=true;
        // Channels change along the same direction (within rounding of the eight-bit colours).
        for(int k=0;k<3&&linear;k++)linear=std::abs(g[k][0]*wy-g[k][1]*wx)<=0.02*std::hypot(g[k][0],g[k][1])*std::sqrt(w2)+1e-15;
        p.save();p.setRenderHint(QPainter::Antialiasing,false);p.setPen(Qt::NoPen);
        if(w2<1e-24){p.setBrush(colour[0]);p.drawPolygon(triangle);}
        else if(linear){
            double t[3],lo=0,hi=0;for(int i=0;i<3;i++){t[i]=wx*(corner[i].x()-corner[0].x())+wy*(corner[i].y()-corner[0].y());lo=std::min(lo,t[i]);hi=std::max(hi,t[i]);}
            auto at=[&](double tt){double c[3];for(int k=0;k<3;k++){const double lambda=(g[k][0]*wx+g[k][1]*wy)/w2;c[k]=std::clamp(c0[k]+lambda*tt,0.0,1.0);}
                return QColor::fromRgbF(float(c[0]),float(c[1]),float(c[2]));};
            QLinearGradient lg(corner[0]+QPointF(wx,wy)*(lo/w2),corner[0]+QPointF(wx,wy)*(hi/w2));lg.setColorAt(0,at(lo));lg.setColorAt(1,at(hi));
            p.setBrush(lg);p.drawPolygon(triangle);
        }else{
            const int n=32;
            auto pointAt=[&](double a,double b){return corner[0]+QPointF(ux,uy)*a+QPointF(vx,vy)*b;};
            auto colourAt=[&](double a,double b){double c[3];for(int k=0;k<3;k++)c[k]=std::clamp(c0[k]+g[k][0]*(ux*a+vx*b)+g[k][1]*(uy*a+vy*b),0.0,1.0);return QColor::fromRgbF(float(c[0]),float(c[1]),float(c[2]));};
            for(int i=0;i<n;i++)for(int j=0;i+j<n;j++){
                const double a=double(i)/n,b=double(j)/n,d=1.0/n;
                p.setBrush(colourAt(a+d/3,b+d/3));p.drawPolygon(QPolygonF{pointAt(a,b),pointAt(a+d,b),pointAt(a,b+d)});
                if(i+j+1<n){p.setBrush(colourAt(a+2*d/3,b+2*d/3));p.drawPolygon(QPolygonF{pointAt(a+d,b),pointAt(a+d,b+d),pointAt(a,b+d)});}
            }
        }
        p.restore();
    };
    qsizetype at=0;
    while(at+8<=b.size()){
        const quint32 type=u32(at),size=u32(at+4);
        if(size<8||size%4||at+size>b.size())break;
        const qsizetype end=at+size;
        switch(type){
        case SetMapMode:if(size>=12)s.mapMode=int(u32(at+8));break;
        case SetWindowExt:if(size>=16)s.windowExt=QSizeF(i32(at+8),i32(at+12));break;
        case SetWindowOrg:if(size>=16)s.windowOrg=QPointF(i32(at+8),i32(at+12));break;
        case SetViewportExt:if(size>=16)s.viewportExt=QSizeF(i32(at+8),i32(at+12));break;
        case SetViewportOrg:if(size>=16)s.viewportOrg=QPointF(i32(at+8),i32(at+12));break;
        case SetPolyFillMode:if(size>=12)s.fill=u32(at+8)==2?Qt::WindingFill:Qt::OddEvenFill;break;
        case SetStretchBltMode:if(size>=12)s.stretchMode=int(u32(at+8));break;
        case SetBkMode:if(size>=12)s.opaque=u32(at+8)==2;break;
        case SetTextAlign:if(size>=12)s.textAlign=int(u32(at+8));break;
        case SetTextColor:if(size>=12)s.textColor=colorRef(u32(at+8));break;
        case SetBkColor:if(size>=12)s.background=colorRef(u32(at+8));break;
        case SaveDc:saved.append(s);break;
        case RestoreDc:if(!saved.isEmpty()){const bool clipChanged=s.clipped||saved.last().clipped;s=saved.takeLast();if(clipChanged)applyClip();}break;
        case CreatePen:if(size>=28){
            Object o;o.kind=Object::Pen;const quint32 style=u32(at+12);const QColor c=colorRef(u32(at+24));
            o.pen=QPen(c,i32(at+16)<=0?0.0:length(i32(at+16),false),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);if(o.pen.widthF()<=0){o.pen.setCosmetic(true);o.pen.setWidthF(1);}
            switch(style&0xff){case 1:o.pen.setStyle(Qt::DashLine);break;case 2:o.pen.setStyle(Qt::DotLine);break;case 3:o.pen.setStyle(Qt::DashDotLine);break;case 4:o.pen.setStyle(Qt::DashDotDotLine);break;case 5:o.pen=QPen(Qt::NoPen);break;default:break;}
            objects.insert(u32(at+8),o);}break;
        case CreateBrush:if(size>=24){
            Object o;const quint32 style=u32(at+12);const QColor c=colorRef(u32(at+16));
            o.brush=style==1?QBrush(Qt::NoBrush):style==2?QBrush(c,hatch(u32(at+20))):QBrush(c);objects.insert(u32(at+8),o);}break;
        case CreateDibPatternBrush:if(size>=32){
            // A brush that repeats a small bitmap.
            Object o;const QImage image=bitmap(at,u32(at+16),u32(at+20),u32(at+24),u32(at+28),end);o.brush=image.isNull()?QBrush(Qt::gray):QBrush(image);objects.insert(u32(at+8),o);}break;
        case ExtCreatePen:if(size>=52){
            // Style, width, colour; geometric pens have a width in logical units, cosmetic ones are one pixel wide.
            Object o;o.kind=Object::Pen;const quint32 style=u32(at+28);const QColor c=colorRef(u32(at+40));const bool geometric=style&0x10000;
            o.pen=QPen(c,geometric?length(u32(at+32),false):0.0);if(o.pen.widthF()<=0){o.pen.setCosmetic(true);o.pen.setWidthF(1);}
            switch(style&0xf00){case 0x100:o.pen.setCapStyle(Qt::SquareCap);break;case 0x200:o.pen.setCapStyle(Qt::FlatCap);break;default:o.pen.setCapStyle(Qt::RoundCap);}
            switch(style&0xf000){case 0x1000:o.pen.setJoinStyle(Qt::BevelJoin);break;case 0x2000:o.pen.setJoinStyle(Qt::MiterJoin);break;default:o.pen.setJoinStyle(Qt::RoundJoin);}
            switch(style&0xff){case 1:o.pen.setStyle(Qt::DashLine);break;case 2:o.pen.setStyle(Qt::DotLine);break;case 3:o.pen.setStyle(Qt::DashDotLine);break;case 4:o.pen.setStyle(Qt::DashDotDotLine);break;case 5:o.pen=QPen(Qt::NoPen);break;default:break;}
            objects.insert(u32(at+8),o);}break;
        case ExtCreateFontW:if(size>=104){
            // The logical font: height, width, escapement, orientation, weight, italic, underline, strike-out, …, face name.
            Object o;o.kind=Object::Typeface;o.font.height=i32(at+12);o.font.escapement=i32(at+20);o.font.weight=i32(at+28);
            o.font.italic=b[at+32]!=0;o.font.underline=b[at+33]!=0;o.font.strikeOut=b[at+34]!=0;
            QByteArray face=b.mid(at+40,64);o.font.face=QString::fromUtf16(reinterpret_cast<const char16_t*>(face.constData()),32);
            if(const int zero=o.font.face.indexOf(QChar(0));zero>=0)o.font.face.truncate(zero);
            objects.insert(u32(at+8),o);}break;
        case SelectObject:if(size>=12){const quint32 index=u32(at+8);std::optional<Object> o;if(index&0x80000000)o=stock(index);else if(objects.contains(index))o=objects.value(index);
            if(o){if(o->kind==Object::Pen)s.pen=o->pen;else if(o->kind==Object::Brush)s.brush=o->brush;else s.font=o->font;}}break;
        case DeleteObject:if(size>=12)objects.remove(u32(at+8));break;
        case MoveTo:if(size>=16){s.position=pointAt(at+8);if(inPath)figure.moveTo(s.position);}break;
        case LineTo:if(size>=16){const QPointF to=pointAt(at+8);QPainterPath tail(s.position);tail.lineTo(to);onwards(tail);s.position=to;}break;
        case Rectangle:case Ellipse:if(size>=24){const QRectF r=rectAt(at+8);QPainterPath path;
            if(type==Rectangle)path.addRect(r);else path.addEllipse(r);fillAndStroke(path);}break;
        case RoundRect:if(size>=32){QPainterPath path;path.addRoundedRect(rectAt(at+8),length(i32(at+24),false)/2,length(i32(at+28),true)/2);fillAndStroke(path);}break;
        case Arc:case Chord:case Pie:case ArcTo:if(size>=40){
            const QRectF box=rectAt(at+8);double start=0,sweep=0;arc(box,pointAt(at+24),pointAt(at+32),&start,&sweep);QPainterPath path;
            if(type==Pie){path.moveTo(box.center());path.arcTo(box,start,sweep);path.closeSubpath();fillAndStroke(path);}
            else if(type==Chord){path.arcMoveTo(box,start);path.arcTo(box,start,sweep);path.closeSubpath();fillAndStroke(path);}
            else if(type==Arc){path.arcMoveTo(box,start);path.arcTo(box,start,sweep);stroke(path);}
            else{QPainterPath tail(s.position);tail.arcTo(box,start,sweep);onwards(tail);s.position=tail.currentPosition();}}break;
        case AngleArc:if(size>=28){
            // A line from the current position to the arc's start, then the arc; angles in degrees, counter-clockwise.
            const QPointF c=pointAt(at+8);const double r=length(u32(at+16),false),start=real(at+20),sweep=real(at+24);
            QPainterPath tail(s.position);tail.arcTo(QRectF(c.x()-r,c.y()-r,2*r,2*r),start,sweep);onwards(tail);s.position=tail.currentPosition();}break;
        case PolylineTo:case PolyBezierTo:case PolylineTo16:case PolyBezierTo16:if(size>=28){
            const bool small=type==PolylineTo16||type==PolyBezierTo16;const QPolygonF poly=points(at+28,u32(at+24),small,end);if(poly.isEmpty())break;
            QPainterPath tail(s.position);
            if(type==PolyBezierTo||type==PolyBezierTo16){for(int i=0;i+2<poly.size();i+=3)tail.cubicTo(poly[i],poly[i+1],poly[i+2]);}else for(auto q:poly)tail.lineTo(q);
            onwards(tail);s.position=tail.currentPosition();}break;
        case SetArcDirection:if(size>=12)s.clockwise=u32(at+8)==2;break;
        case SetWorldTransform:if(size>=32)s.world=QTransform(real(at+8),real(at+12),real(at+16),real(at+20),real(at+24),real(at+28));break;
        case ModifyWorldTransform:if(size>=36){
            const QTransform t(real(at+8),real(at+12),real(at+16),real(at+20),real(at+24),real(at+28));
            switch(u32(at+32)){case 1:s.world=QTransform();break;case 2:s.world=t*s.world;break;case 3:s.world=s.world*t;break;case 4:s.world=t;break;default:break;}}break;
        case BeginPath:inPath=true;figure=QPainterPath();break;
        case EndPath:inPath=false;break;
        case CloseFigure:if(inPath)figure.closeSubpath();break;
        case AbortPath:inPath=false;figure=QPainterPath();break;
        case FillPath:case StrokeAndFillPath:case StrokePath:{
            QPainterPath path=figure;figure=QPainterPath();inPath=false;path.setFillRule(s.fill);
            if(type!=StrokePath&&s.brush.style()!=Qt::NoBrush)p.fillPath(path,s.brush);
            if(type!=FillPath&&s.pen.style()!=Qt::NoPen)p.strokePath(path,s.pen);}break;
        case SelectClipPath:if(size>=12){QPainterPath path=figure;figure=QPainterPath();inPath=false;path.setFillRule(s.fill);clipWith(path,int(u32(at+8)));}break;
        case Polygon:case Polyline:case Polygon16:case Polyline16:case PolyBezier:case PolyBezier16:if(size>=28){
            const bool small=type==Polygon16||type==Polyline16||type==PolyBezier16;const QPolygonF poly=points(at+28,u32(at+24),small,end);if(poly.size()<2)break;
            QPainterPath path;
            if(type==PolyBezier||type==PolyBezier16){path.moveTo(poly[0]);for(int i=1;i+2<poly.size();i+=3)path.cubicTo(poly[i],poly[i+1],poly[i+2]);stroke(path);}
            else if(type==Polygon||type==Polygon16){path.addPolygon(poly);path.closeSubpath();fillAndStroke(path);}
            else{path.addPolygon(poly);stroke(path);}}break;
        case PolyPolygon:case PolyPolyline:case PolyPolygon16:case PolyPolyline16:if(size>=32){
            const bool small=type==PolyPolygon16||type==PolyPolyline16,closed=type==PolyPolygon||type==PolyPolygon16;const quint32 polygons=u32(at+24),total=u32(at+28);
            if(polygons>quint32((end-at-32)/4))break;
            qsizetype data=at+32+qsizetype(polygons)*4;const QPolygonF all=points(data,total,small,end);if(all.size()!=int(total))break;
            QPainterPath path;int first=0;
            for(quint32 k=0;k<polygons;k++){const int n=int(u32(at+32+k*4));if(n<0||first+n>all.size())break;QPolygonF part=all.mid(first,n);first+=n;
                path.addPolygon(part);if(closed)path.closeSubpath();}
            if(closed)fillAndStroke(path);else stroke(path);}break;
        case IntersectClipRect:case ExcludeClipRect:if(size>=24){QPainterPath r;r.addRect(rectAt(at+8));clipWith(r,type==IntersectClipRect?1:4);}break;
        case ExtSelectClipRgn:if(size>=16){
            // The region: a header of 32 bytes and its rectangles in device units; without data, copying resets the clipping.
            const quint32 dataSize=u32(at+8);const int mode=int(u32(at+12));
            if(dataSize<32){if(mode==5&&s.clipped){s.clipped=false;s.clip=QPainterPath();applyClip();}break;}
            const qsizetype region=at+16;if(region+dataSize>end)break;const quint32 count=u32(region+8);if(count>(dataSize-32)/16)break;
            QPainterPath path;
            // Regions of many rectangles are clipped to their bounds; joining them all could take very long.
            if(count>200)path.addRect(QRectF(QPointF(i32(region+16),i32(region+20)),QPointF(i32(region+24),i32(region+28))).normalized());
            else{for(quint32 k=0;k<count;k++){const qsizetype r=region+32+k*16;path.addRect(QRectF(QPointF(i32(r),i32(r+4)),QPointF(i32(r+8),i32(r+12))));}path=path.simplified();}
            clipWith(path,mode);}break;
        case ExtTextOutA:case ExtTextOutW:if(size>=76){
            const qsizetype text=at+36;const quint32 count=u32(text+8),offset=u32(text+12),options=u32(text+16),offDx=u32(text+36);
            const bool wide=type==ExtTextOutW;const qsizetype bytes=qsizetype(count)*(wide?2:1);if(count==0||count>100000||offset+bytes>size)break;
            const QByteArray raw=b.mid(at+offset,bytes);
            const QString string=wide?QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData()),qsizetype(count)):QString::fromLatin1(raw);
            if(options&2)p.fillRect(rectAt(text+20),s.background);   // opaque rectangle behind the text
            // Spacing of the characters; with ETO_PDY each entry is followed by a vertical one.
            QList<double> advances;const int stride=options&0x2000?8:4;
            if(offDx&&offDx+qsizetype(count)*stride<=size)for(quint32 k=0;k<count;k++)advances<<i32(at+offDx+k*stride);
            drawText(s.textAlign&1?s.position:device(QPointF(i32(text),i32(text+4))),string,advances);}break;
        case SmallTextOut:if(size>=36){
            const quint32 count=u32(at+16),options=u32(at+20);const qsizetype text=at+36+(options&0x100?0:16);const bool narrow=options&0x200;
            const qsizetype bytes=qsizetype(count)*(narrow?1:2);if(count==0||count>100000||text+bytes>end)break;
            const QByteArray raw=b.mid(text,bytes);
            drawText(device(QPointF(i32(at+8),i32(at+12))),narrow?QString::fromLatin1(raw):QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData()),qsizetype(count)),{});}break;
        case StretchDiBits:if(size>=80){
            const int x=i32(at+24),y=i32(at+28),w=i32(at+72),h=i32(at+76);
            const QImage image=bitmap(at,u32(at+48),u32(at+52),u32(at+56),u32(at+60),end);
            const int sx=i32(at+32),sy=i32(at+36),sw=i32(at+40),sh=i32(at+44);
            const bool fromTop=topDown(at,u32(at+48),u32(at+52),end);
            blit(image,device(QPointF(x,y)),device(QPointF(x+w,y)),device(QPointF(x,y+h)),image.isNull()?QRect():QRect(sx,fromTop?sy:image.height()-sy-sh,sw,sh),u32(at+68));}break;
        case SetDiBitsToDevice:if(size>=76){
            // A bitmap at its own size in logical units; the record may hold only a band of its scan lines.
            const int x=i32(at+24),y=i32(at+28),sx=i32(at+32),sy=i32(at+36),w=i32(at+40),h=i32(at+44);
            const quint32 offInfo=u32(at+48),infoSize=u32(at+52),start=u32(at+68),scans=u32(at+72);
            if(w<=0||h<=0||scans==0||scans>100000||start>100000)break;
            const bool fromTop=topDown(at,offInfo,infoSize,end);
            const QImage band=bitmap(at,offInfo,infoSize,u32(at+56),u32(at+60),end,fromTop?-qint32(scans):qint32(scans));if(band.isNull())break;
            // The rows of the source rectangle that the band holds, counted like the bitmap's rows.
            const int first=std::max(sy,int(start)),last=std::min(sy+h,int(start+scans));if(first>=last)break;
            const int top=fromTop?first-sy:sy+h-last,bottom=fromTop?last-sy:sy+h-first;
            const QRect source(sx,fromTop?first-int(start):int(start+scans)-last,w,last-first);
            blit(band,device(QPointF(x,y+top)),device(QPointF(x+w,y+top)),device(QPointF(x,y+bottom)),source,0x00CC0020);}break;
        case BitBlt:case StretchBlt:case TransparentBlt:if(size>=100){
            // Without a bitmap only the brush and what lies below take part (a pattern fill).
            const int dx=i32(at+24),dy=i32(at+28),dw=i32(at+32),dh=i32(at+36);
            const quint32 headerSize=u32(at+88);const bool stretched=type!=BitBlt&&size>=108;
            QImage image=headerSize?bitmap(at,u32(at+84),headerSize,u32(at+92),u32(at+96),end):QImage();
            // TRANSPARENTBLT leaves out the pixels of one colour, given in place of the raster operation.
            if(type==TransparentBlt&&!image.isNull()){
                image=image.convertToFormat(QImage::Format_ARGB32);const QRgb hidden=colorRef(u32(at+40)).rgb()&0xffffff;
                for(int row=0;row<image.height();row++){auto *line=reinterpret_cast<QRgb*>(image.scanLine(row));for(int k=0;k<image.width();k++)if((line[k]&0xffffff)==hidden)line[k]=0;}
            }
            const int sx=i32(at+44),sy=i32(at+48);const int sw=stretched?i32(at+100):dw,sh=stretched?i32(at+104):dh;
            blit(image,device(QPointF(dx,dy)),device(QPointF(dx+dw,dy)),device(QPointF(dx,dy+dh)),QRect(sx,sy,sw,sh),type==TransparentBlt?0x00CC0020:u32(at+40));}break;
        case MaskBlt:if(size>=128){
            // Where the mask is set the foreground operation applies, elsewhere the background one (the high byte).
            const int dx=i32(at+24),dy=i32(at+28),dw=i32(at+32),dh=i32(at+36);const quint32 rop4=u32(at+40);
            const QImage image=u32(at+88)?bitmap(at,u32(at+84),u32(at+88),u32(at+92),u32(at+96),end):QImage();
            const QImage mask=u32(at+116)?bitmap(at,u32(at+112),u32(at+116),u32(at+120),u32(at+124),end):QImage();
            const QRect source(i32(at+44),i32(at+48),dw,dh);const QPointF a=device(QPointF(dx,dy)),r=device(QPointF(dx+dw,dy)),l=device(QPointF(dx,dy+dh));
            if(mask.isNull()){blit(image,a,r,l,source,rop4&0xffffff);break;}
            const QImage part=mask.copy(i32(at+100),i32(at+104),dw,dh);
            blit(image,a,r,l,source,rop4&0xffffff,1,part);blit(image,a,r,l,source,(rop4>>8)&0xff0000,1,part,true);}break;
        case PlgBlt:if(size>=140){
            // A bitmap onto a parallelogram given by three corners, copied where its mask is set.
            const QPointF a=device(QPointF(i32(at+24),i32(at+28))),r=device(QPointF(i32(at+32),i32(at+36))),l=device(QPointF(i32(at+40),i32(at+44)));
            const QRect source(i32(at+48),i32(at+52),i32(at+56),i32(at+60));
            const QImage image=bitmap(at,u32(at+96),u32(at+100),u32(at+104),u32(at+108),end);
            const QImage mask=u32(at+128)?bitmap(at,u32(at+124),u32(at+128),u32(at+132),u32(at+136),end):QImage();
            blit(image,a,r,l,source,0x00CC0020,1,mask.isNull()?QImage():mask.copy(i32(at+112),i32(at+116),source.width(),source.height()));}break;
        case AlphaBlend:if(size>=108){
            // Destination, blend function (constant alpha, per-pixel alpha), source and a bitmap with premultiplied alpha.
            const int dx=i32(at+24),dy=i32(at+28),dw=i32(at+32),dh=i32(at+36);const double constant=quint8(b[at+42])/255.0;const bool perPixel=quint8(b[at+43])&1;
            const int sx=i32(at+44),sy=i32(at+48),sw=i32(at+100),sh=i32(at+104);const quint32 offInfo=u32(at+84),infoSize=u32(at+88),offBits=u32(at+92),bitsSize=u32(at+96);
            if(infoSize<40||at+offInfo+infoSize>end||at+offBits+bitsSize>end)break;
            const qint32 width=i32(at+offInfo+4),height=i32(at+offInfo+8);const int depth=qFromLittleEndian<quint16>(b.constData()+at+offInfo+14);
            QImage image;
            if(depth==32&&perPixel&&width>0&&width<=20000&&height!=0&&std::abs(height)<=20000&&bitsSize>=quint32(width)*quint32(std::abs(height))*4){
                image=QImage(width,std::abs(height),QImage::Format_ARGB32_Premultiplied);
                for(int row=0;row<image.height();row++){const qsizetype from=at+offBits+qsizetype(height>0?image.height()-1-row:row)*width*4;std::memcpy(image.scanLine(row),b.constData()+from,size_t(width)*4);}
            }else image=bitmap(at,offInfo,infoSize,offBits,bitsSize,end);
            if(image.isNull())break;
            blit(image,device(QPointF(dx,dy)),device(QPointF(dx+dw,dy)),device(QPointF(dx,dy+dh)),QRect(sx,sy,sw,sh),0x00CC0020,constant,QImage(),false,true);}break;
        case GradientFill:if(size>=36){
            // Vertices with colours of 16 bits per channel, then rectangles (horizontal or vertical) or triangles.
            const quint32 vertices=u32(at+24),objectsCount=u32(at+28),mode=u32(at+32);
            if(vertices>100000||objectsCount>100000||at+36+qsizetype(vertices)*16+qsizetype(objectsCount)*(mode==2?12:8)>end)break;
            auto vertex=[&](quint32 i){return device(QPointF(i32(at+36+i*16),i32(at+40+i*16)));};
            auto colour=[&](quint32 i){const qsizetype c=at+44+i*16;return QColor(qFromLittleEndian<quint16>(b.constData()+c)>>8,qFromLittleEndian<quint16>(b.constData()+c+2)>>8,qFromLittleEndian<quint16>(b.constData()+c+4)>>8);};
            const qsizetype list=at+36+qsizetype(vertices)*16;
            for(quint32 k=0;k<objectsCount;k++){
                if(mode==2){const quint32 i0=u32(list+k*12),i1=u32(list+k*12+4),i2=u32(list+k*12+8);if(i0>=vertices||i1>=vertices||i2>=vertices)continue;
                    shade({vertex(i0),vertex(i1),vertex(i2)},{colour(i0),colour(i1),colour(i2)});}
                else{const quint32 i0=u32(list+k*8),i1=u32(list+k*8+4);if(i0>=vertices||i1>=vertices)continue;
                    const QPointF a=device(QPointF(i32(at+36+i0*16),i32(at+40+i0*16))),c=device(QPointF(i32(at+36+i1*16),i32(at+40+i1*16)));
                    const QRectF r=QRectF(a,c).normalized();QLinearGradient lg(mode==0?QPointF(a.x(),r.top()):QPointF(r.left(),a.y()),mode==0?QPointF(c.x(),r.top()):QPointF(r.left(),c.y()));
                    lg.setColorAt(0,colour(i0));lg.setColorAt(1,colour(i1));p.fillRect(r,lg);}
            }}break;
        case FillRgn:if(size>=32){const QPainterPath area=regionAt(at+32,u32(at+24),end);p.fillPath(area,brushOf(u32(at+28)));}break;
        case PaintRgn:if(size>=28){const QPainterPath area=regionAt(at+28,u32(at+24),end);if(s.brush.style()!=Qt::NoBrush)p.fillPath(area,s.brush);}break;
        case FrameRgn:if(size>=40){
            // A border inside the region, as wide as the record says.
            const QPainterPath area=regionAt(at+40,u32(at+24),end);QPainterPathStroker border;border.setWidth(2*std::max(length(i32(at+32),false),length(i32(at+36),true)));
            border.setJoinStyle(Qt::MiterJoin);p.fillPath(border.createStroke(area).intersected(area),brushOf(u32(at+28)));}break;
        case InvertRgn:if(size>=28){const QPainterPath area=regionAt(at+28,u32(at+24),end);p.save();p.setCompositionMode(QPainter::CompositionMode_Difference);p.fillPath(area,Qt::white);p.restore();}break;
        default:break;
        }
        if(type==Eof)break;
        at=end;
    }
    p.restore();
    return true;
}
}
