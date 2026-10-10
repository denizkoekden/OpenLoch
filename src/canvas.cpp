#include "canvas.h"
#include "icons.h"
#include "language.h"
#include "geometry.h"
#include "legacy_reader.h"
#include "legacy_writer.h"
#include "continuity.h"
#include "printing.h"
#include <QPainterPathStroker>
#include <QFontMetricsF>
#include <QGraphicsItem>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QShowEvent>
#include <QTimer>
#include <QSignalBlocker>
#include <QPaintEngine>
#include <QMenu>
#include <QBuffer>
#include <QScreen>
#include <QPdfWriter>
#include <QPageSize>
#include <QInputDialog>
#include <QJsonArray>
#include <QImageReader>
#include <QImageWriter>
#include <QStyleOptionGraphicsItem>
#include <QPrinter>
#include <cmath>
#include <functional>
#include <optional>
#include <array>
#include <limits>

namespace openloch {
// The original's board colours: board area with light bevel lines at the top and left, copper (orange while the solder
// side is active), pads and black holes.
namespace board {const QColor fill("#efcc05"),bevel("#fef5cf"),copper("#cfac03"),copperBack("#d07d02"),cut("#ffdc15"),hole("#ece9d8"),holeRim("#c0c0c0");}
static QPointF point(const QJsonArray &a,int at=0){return {a[at].toDouble(),a[at+1].toDouble()};}
static QPolygonF polygon(const QJsonValue &v){QPolygonF p;for(const auto &x:v.toArray())p<<point(x.toArray());return p;}
static QColor delphiColor(const QJsonValue &v) {quint32 c=quint32(v.toInteger());return QColor(c&255,(c>>8)&255,(c>>16)&255);}
// Potential marker rings ignore the stored width; keep them inside the item without moving component anchors.
static QRectF contactBounds(const QJsonObject &o) {
    QRectF r;for(const auto &v:o["children"].toArray())r=r.united(contactBounds(v.toObject()));
    auto path=o["path"].toArray();if(o["type"]=="TDraht"&&o["kind"].toInt()==18&&path.size()>1)r=r.united(QRectF(point(path[0].toArray())-QPointF(102,102),QSizeF(204,204)));
    return r;
}
static QRectF nodeBounds(const QJsonObject &o) {
    if(!o.contains("x"))return legacyBounds(o).united(contactBounds(o)).adjusted(-10,-10,10,10);
    QPointF at(o["x"].toDouble(),o["y"].toDouble());QRectF r(at-QPointF(280,280),QSizeF(560,560));
    if(o.contains("x2")){const double grow=o["type"]=="track"?o["width"].toDouble(200)/2+10:50;r=QRectF(at,QPointF(o["x2"].toDouble(),o["y2"].toDouble())).normalized().adjusted(-grow,-grow,grow,grow);}
    if(o.contains("points")){auto poly=polygon(o["points"]);r=poly.boundingRect().translated(at).adjusted(-50,-50,50,50);}
    if(o.contains("text")){double height=qBound(20.0,o["textSize"].toDouble(120),2000.0);r=r.united(QRectF(at+QPointF(-130,-130-height*1.2),QSizeF(qMax(1,o["text"].toString().size())*height,height*1.5)));}
    return objectTransform(o,at).mapRect(r);
}
// A LochMaster text label lives in a box P0 (top left), P1 (top right), P2, P3 (bottom left). Without stored corners
// (old files, fresh assistant labels) the original lays the box out from the stored angle (counter-clockwise), the
// cell height and the measured width; the "turned by 180°" flag starts it at the far corner.
static std::array<QPointF,4> labelCorners(const QJsonObject &o,const QString &text,QFont f){
    if(const auto c=labelStoredCorners(o))return *c;
    const QPointF p0=point(o["text_position"].toArray());
    const double h=qMax(1,o["text_kind"].toInt());f.setPixelSize(100);const QFontMetricsF m(f);const double w=m.horizontalAdvance(text)*h/(m.ascent()+m.descent());
    double angle=o["text_height"].toDouble();QPointF start=p0;auto along=[&]{return QPointF(std::cos(angle),-std::sin(angle));};auto down=[&]{return QPointF(std::sin(angle),std::cos(angle));};
    if(o["text_flags"].toArray().at(1).toBool()){start=p0+w*along()+h*down();angle+=3.14159265358979323846;}
    return {start,start+w*along(),start+w*along()+h*down(),start+h*down()};
}
// Paints the text into its box like the original: em = box height, baseline at P3 raised by the descent, stretched to
// the box width. Glyphs are never mirrored: a mirrored part only swaps corners (bit 0 horizontal, bit 1 vertical).
static void paintLabel(QPainter &p,const QString &text,QFont f,std::array<QPointF,4> c,int mirror,const QColor &pen,const QColor *box){
    if(mirror&1)c={c[1],c[0],c[3],c[2]};if(mirror&2)c={c[3],c[2],c[1],c[0]};
    const QLineF bottom(c[3],c[2]),side(c[3],c[0]);const double height=side.length()*std::abs(std::sin(bottom.angleTo(side)*3.14159265358979323846/180));
    if(box){p.setPen(Qt::NoPen);p.setBrush(*box);p.drawPolygon(QPolygonF{c[0],c[1],c[2],c[3]});}
    if(height<1||bottom.length()<1||text.isEmpty())return;
    f.setPixelSize(100);const QFontMetricsF m(f);const double width=m.horizontalAdvance(text);if(width<=0)return;
    const QPointF up=(c[0]-c[3])/side.length(),origin=c[3]+up*(height*m.descent()/(m.ascent()+m.descent())),across=(c[2]-c[3])/width,downwards=-up*(height/100);
    p.save();p.setTransform(QTransform(across.x(),across.y(),downwards.x(),downwards.y(),origin.x(),origin.y()),true);p.setFont(f);p.setPen(pen);p.drawText(QPointF(),text);p.restore();
}
// A fill picture stretched over the corners in `anchors` (top left, top right, bottom left), clipped to the outline.
static void paintPicture(QPainter &p,const QImage &img,const QPolygonF &poly,const QJsonArray &a){
    if(img.isNull())return;p.save();QPainterPath clip;clip.addPolygon(poly);p.setClipPath(clip,Qt::IntersectClip);const QPointF tl=point(a),tr=point(a,2),bl=point(a,4);
    if(a.size()==6&&QLineF(tl,tr).length()>1&&QLineF(tl,bl).length()>1){p.setTransform(QTransform((tr.x()-tl.x())/img.width(),(tr.y()-tl.y())/img.width(),(bl.x()-tl.x())/img.height(),(bl.y()-tl.y())/img.height(),tl.x(),tl.y()),true);p.drawImage(QPointF(),img);}
    else p.drawImage(poly.boundingRect(),img);
    p.restore();
}
// Röntgenkontrast grey levels, program-wide as in the original (fill "Füllung", outline "Rand").
static QColor xrayFill(0xB9,0xB9,0xB9),xrayOutline(0xD0,0xD0,0xD0);
using ViewState=Canvas::ViewState;
// The original's copper parts: pads (TAuge) in the copper colour of the active side (light grey in S/W), holes (TBohrung)
// light with a grey rim and, over 2 mm, a grey cross of half their radius, cuts (TTrenner) light yellow.
static void paintPad(QPainter &p,QPointF centre,double r,bool mono,bool back){const QColor c=mono?QColor("#eaeaea"):back?board::copperBack:board::copper;p.setPen(QPen(c,0));p.setBrush(c);p.drawEllipse(centre,r,r);}
static void paintHole(QPainter &p,QPointF centre,double diameter,bool mono){
    const double r=std::round(diameter*50);p.setPen(QPen(board::holeRim,0));p.setBrush(mono?QColor(Qt::white):board::hole);p.drawEllipse(centre,r,r);
    if(diameter>2){const double h=std::floor(r/2);p.drawLine(centre-QPointF(h,0),centre+QPointF(h,0));p.drawLine(centre-QPointF(0,h),centre+QPointF(0,h));}
}
static void paintCut(QPainter &p,const QRectF &r,bool mono){p.setPen(QPen(mono?QColor(Qt::black):board::cut,0));p.setBrush(mono?QColor(Qt::white):board::cut);p.drawRect(r);}
// A circle's own picture corners stay unused; a copy takes the picture with the corners of its inner outline.
static void circlePicture(const QJsonObject &source,QJsonObject &n){const auto inner=source.value("inner").toObject();if(inner.contains("bitmap")){n["bitmap"]=inner["bitmap"];n["anchors"]=inner["anchors"];}}
// A solder joint as the original shows it with BMP-Rendering: a shiny tin dome, here drawn by OpenLoch (light from the
// upper left, a darker rim where it meets the copper).
static void paintSolderBlob(QPainter &p,QPointF centre,double r){
    QRadialGradient tin(centre,r,centre+QPointF(-r*.35,-r*.4));
    tin.setColorAt(0,QColor(252,252,250));tin.setColorAt(.35,QColor(214,216,218));tin.setColorAt(.8,QColor(150,153,158));tin.setColorAt(1,QColor(110,112,116));
    p.setPen(QPen(QColor(96,98,102),qMax(1.0,r*.06)));p.setBrush(tin);p.drawEllipse(centre,r,r);
}
static void drawNode(QPainter &p,const QJsonObject &o,const QByteArray &source,QJsonObject group={},const QString &title={},const ViewState &view={},int mirror=0,bool selected=false,bool sideless=false);
// An object of the inactive side seen through the board: its shape darkens what lies below, like the original's x-ray
// pen mode. Drawn as a layer in device pixels and composited onto the board.
static void paintXRay(QPainter &p,const QJsonObject &o,const QByteArray &source,const QJsonObject &group,const QString &title,const ViewState &view,int mirror){
    const QRect device(0,0,p.device()->width(),p.device()->height());const QRect area=p.worldTransform().mapRect(nodeBounds(o).adjusted(-60,-60,60,60)).toAlignedRect().intersected(device);
    if(area.isEmpty()||qint64(area.width())*area.height()>40000000)return;
    QImage layer(area.size(),QImage::Format_ARGB32_Premultiplied);layer.fill(Qt::transparent);
    {QPainter q(&layer);q.setRenderHints(p.renderHints());q.setWorldTransform(p.worldTransform()*QTransform::fromTranslate(-area.left(),-area.top()));ViewState plain=view;plain.mono=view.printer;drawNode(q,o,source,group,title,plain,mirror,false,true);
     q.setCompositionMode(QPainter::CompositionMode_SourceIn);q.resetTransform();q.fillRect(layer.rect(),view.mono?xrayFill:xrayFill);}
    // The original combines dest AND NOT grey, tuned for its yellow board; on OpenLoch's palette that turns copper
    // purple, so the grey multiplies instead: a shadow of the same grey on white, darker on copper.
    p.save();p.resetTransform();p.setCompositionMode(QPainter::CompositionMode_Multiply);
    p.drawImage(area.topLeft(),layer);p.restore();
}
// A fill picture's colour when bitmaps are off: the average of five pixels (centre and the four edge middles).
static QColor pictureAverage(const QImage &img){
    if(img.isNull())return {};const int w=img.width(),h=img.height();int r=0,g=0,b=0;
    for(QPoint at:{QPoint(w/2,h/2),QPoint(qMin(2,w-1),h/2),QPoint(w/2,qMin(2,h-1)),QPoint(qMax(0,w-2),h/2),QPoint(w/2,qMax(0,h-2))}){const QColor c=img.pixelColor(at);r+=c.red();g+=c.green();b+=c.blue();}
    return QColor(r/5,g/5,b/5);
}
// The print passes: 1 holes, 2 cuts, 3 everything else.
static int printCategory(const QJsonObject &o){const auto t=o["type"].toString();if(t=="TBohrung"||t=="drill")return 1;if(t=="TTrenner"||t=="TTrennerFest"||t=="cut")return 2;return 3;}
static void drawNode(QPainter &p,const QJsonObject &o,const QByteArray &source,QJsonObject group,const QString &title,const ViewState &view,int mirror,bool selected,bool sideless) {
    if(o.contains("children")){
        QJsonObject context=group;for(auto key:{"id","value","description"})if(group.isEmpty()||!o[key].toString().isEmpty())context[key]=QString(key)=="id"?QJsonValue(componentId(o)):o[key];
        for(auto key:{"label","extra"})if(o.contains(key))context[key]=o[key];
        for(const auto &v:o["children"].toArray())drawNode(p,v.toObject(),source,context,title,view,mirror,selected,sideless);return;
    }
    if(view.pass&&printCategory(o)!=view.pass)return;
    if(!view.potentialMarkers&&((o["type"]=="TDraht"&&o["kind"].toInt()==18)||o["type"]=="potential"))return;
    QString type=o["type"].toString();p.save();const int kind=o["kind"].toInt();
    // Copper, cuts, pads, drills, potential markers, solder blobs and milled outlines look the same from both sides.
    const bool always=type=="TLeiterbahn"||type=="TAuge"||type=="TBohrung"||type=="TTrenner"||type=="TTrennerFest"||QStringList{"cut","pad","eye","drill","track","potential","solder"}.contains(type)||(type=="TDraht"&&(kind==18||kind==19))||o["flag3"].toBool();
    const bool back=type=="wire"?o.value("back").toBool(true):o["back"].toBool();
    if(view.sides&&!sideless&&!always&&back!=view.backActive()){
        // The inactive side: x-rayed with Röntgenblick or when selected, hidden otherwise. With the solder side active the
        // soldered ends of front objects show as feet.
        if(view.xray||selected)paintXRay(p,o,source,group,title,view,mirror);
        if(view.backActive())for(auto at:connectionPoints(o)){
            p.setPen(QPen(view.mono?QColor(Qt::black):QColor("#604824"),8));p.setBrush(view.mono?QColor("#eaeaea"):QColor("#d0a261"));p.drawEllipse(at,58,58);
            p.setPen(Qt::NoPen);p.setBrush(view.mono?QColor(Qt::white):QColor("#352d23"));p.drawEllipse(at,24,24);
        }
        p.restore();return;
    }
    QPen pen(delphiColor(o["pen"]),qMax(2.0,o["width"].toDouble()),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
    // S/W: lines black and 0.1 mm wide (milled outlines keep their width). In colour an outline of width 0 ("unsichtbar")
    // has no line at all.
    if(view.mono){pen.setColor(Qt::black);if(!o["flag3"].toBool())pen.setWidthF(10);}
    else if(o["width"].toDouble()==0&&(type=="TDraht"||type=="TDrahtFest"||type=="TKreis"))pen.setStyle(Qt::NoPen);
    p.setPen(pen);p.setBrush(delphiColor(o["brush"]));
    if(type=="TBohrung"||type=="TAuge") {
        // A pad is only the copper disc; its hole is a TBohrung of its own, drawn above it.
        const auto center=point(o["center"].toArray());
        if(type=="TAuge")paintPad(p,center,std::round(o["diameter"].toDouble()*50),view.mono,view.backActive());else paintHole(p,center,o["diameter"].toDouble(),view.mono);
    } else if(type=="TTrenner"||type=="TTrennerFest") {
        auto a=o["rect"].toArray();paintCut(p,QRectF(point(a),point(a,2)).normalized(),view.mono);
    } else if(type=="TTextLabel"&&view.texts) {
        QString text=componentText(o["text"].toString(),group,title);QFont f(o["font"].toString("Arial"));
        auto style=o["font_style"].toArray();f.setBold(style[0].toBool());f.setItalic(style[1].toBool());f.setUnderline(style[2].toBool());f.setStrikeOut(style[3].toBool());
        // A filled label sits on its box in the brush colour.
        const QColor brush=delphiColor(o["brush"]);paintLabel(p,text,f,labelCorners(o,text,f),mirror^view.textMirror,view.mono?QColor(Qt::black):delphiColor(o["pen"]),o["transparent"].toBool()&&!view.mono?&brush:nullptr);
    } else if(type=="TDraht"||type=="TDrahtFest"||type=="TLeiterbahn"||type=="TKreis") {
        auto shape=type=="TKreis"?o["inner"].toObject():o;auto poly=polygon(shape["path"]);
        // LochMaster paints a TDraht only from two path points on; kind 18 is a potential marker, 11 a pin, 19 a solder blob.
        if(type=="TDraht"&&poly.size()<2){p.restore();return;}
        if(type=="TDraht"&&o["kind"].toInt()==18){
            p.setPen(QPen(QColor(255,0,0),0));p.setBrush(Qt::white);p.drawEllipse(poly[0],100,100);
            QColor core=delphiColor(o["pen"]);if(poly[0]==poly[1]){p.setPen(Qt::NoPen);p.setBrush(core);p.drawEllipse(poly[0],50,50);}else{p.setPen(QPen(core,100,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.drawLine(poly[0],poly[1]);}
            p.restore();return;
        }
        if(type=="TDraht"&&o["kind"].toInt()==11){
            // Pins: a pen-coloured stroke of the stored width, i.e. a dot of that diameter for two equal points.
            const double w=qMax(1.0,o["width"].toDouble());QColor colour=view.mono?QColor(Qt::black):delphiColor(o["pen"]);
            if(poly[0]==poly[1]){p.setPen(Qt::NoPen);p.setBrush(colour);p.drawEllipse(poly[0],w/2,w/2);}else{p.setPen(QPen(colour,w,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.drawLine(poly[0],poly[1]);}
            p.restore();return;
        }
        if(type=="TDraht"&&o["kind"].toInt()==19){
            double r=o["width"].toInt()/2;p.setPen(QPen(QColor(192,192,192),0));p.setBrush(Qt::white);p.drawLine(poly[0],poly[1]);
            if(view.bitmaps&&!view.mono)paintSolderBlob(p,poly[0],r);else p.drawEllipse(poly[0],r,r);
            p.restore();return;
        }
        // Milled outlines ("Kontur fräsen", lines and closed outlines only) look grey in the original: the line in its own
        // width in 0x808080, a closed outline filled with it, then a thin light grey line on top; no fill picture.
        const int shapeKind=shape["kind"].toInt();
        if((type=="TDraht"||type=="TDrahtFest"||type=="TKreis")&&(o["flag3"].toBool()||shape["flag3"].toBool())&&(shapeKind==4||shapeKind==6||shapeKind==7)){
            const QColor grey(0x80,0x80,0x80);QPen wide=p.pen();wide.setColor(grey);p.setPen(wide);poly=drawnPath(shape);const bool closed=shapeKind!=4&&poly.size()>2;
            p.setBrush(closed?QBrush(grey):QBrush(Qt::NoBrush));
            for(const QPen &pass:{wide,QPen(QColor(0xc0,0xc0,0xc0),0)}){p.setPen(pass);if(closed)p.drawPolygon(poly);else p.drawPolyline(poly);}
            p.restore();return;
        }
        // The picture comes from the file (bitmap_offset) or from a fill loaded in OpenLoch (Base64 "bitmap"). A circle shows
        // the one of its inner outline, as the original paints the circle through it.
        auto bitmap=shape.contains("bitmap_offset")||shape.contains("bitmap")?shape:o;const bool hasPicture=bitmap.contains("bitmap_offset")||bitmap.contains("bitmap");
        const QImage picture=hasPicture?QImage::fromData(legacyBitmapData(bitmap.contains("bitmap")?QByteArray::fromBase64(bitmap["bitmap"].toString().toLatin1()):source.mid(bitmap["bitmap_offset"].toInteger(),bitmap["bitmap_size"].toInteger())),"BMP"):QImage();
        // S/W draws closed shapes as outlines only; without BMP-Rendering a picture's area takes its average colour.
        bool filled=poly.size()>2&&shape["transparent"].toBool()&&(!view.mono||o["flag3"].toBool());
        if(hasPicture&&!view.bitmaps&&!view.mono&&!picture.isNull())p.setBrush(pictureAverage(picture));
        if(type!="TLeiterbahn")poly=drawnPath(shape);
        // A strip extends its stored width to both sides of the centre line and ends flat at the path ends.
        if(type=="TLeiterbahn"){p.setPen(QPen(view.mono?QColor("#eaeaea"):view.backActive()?board::copperBack:board::copper,qMax(20.0,2*o["width"].toDouble()),Qt::SolidLine,Qt::FlatCap,Qt::MiterJoin));p.setBrush(Qt::NoBrush);}
        else if(!filled)p.setBrush(Qt::NoBrush);
        if(poly.size()==1||(poly.size()==2&&poly[0]==poly[1])){double r=qMax(12.0,o["width"].toDouble()/2);p.setBrush(p.pen().color());p.drawEllipse(poly[0],r,r);}
        else if(filled)p.drawPolygon(poly);
        else p.drawPolyline(poly);
        if(hasPicture&&!poly.isEmpty()&&view.bitmaps&&!view.mono)paintPicture(p,picture,poly,bitmap["anchors"].toArray());
    } else if(o.contains("x")) {
        QPointF at(o["x"].toDouble(),o["y"].toDouble());p.translate(at);p.rotate(o["angle"].toDouble());p.scale(o["mirrorX"].toBool()?-1:1,o["mirrorY"].toBool()?-1:1);
        // Milled outlines are grey like the original's (see the LM4 objects above).
        const bool milled=o["flag3"].toBool()&&(type=="rectangle"||type=="ellipse"||type=="polygon"||type=="polyline");
        const QColor line=milled?QColor(0x80,0x80,0x80):view.mono?QColor(Qt::black):QColor(o["color"].toString("#28624d"));const double width=o["width"].toDouble(25);
        p.setPen(!view.mono&&width==0?QPen(Qt::NoPen):QPen(line,view.mono&&!o["flag3"].toBool()?10:width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
        const bool fill=milled?type!="polyline":o["filled"].toBool()&&(!view.mono||o["flag3"].toBool());const QImage picture=o.contains("bitmap")?QImage::fromData(legacyBitmapData(QByteArray::fromBase64(o["bitmap"].toString().toLatin1())),"BMP"):QImage();
        auto fillBrush=[&]{p.setBrush(milled?line:!picture.isNull()&&!view.bitmaps?pictureAverage(picture):o.contains("fill")&&!view.mono?QColor(o["fill"].toString()):line);};
        if(type=="wire")p.drawLine(QPointF(),QPointF(o["x2"].toDouble(),o["y2"].toDouble())-at);
        // With the outline buttons' smoothing, contours are drawn through the same path rules as LochMaster's outlines.
        auto smoothed=[&](const QPolygonF &points,int kind){QJsonArray path;for(auto q:points)path.append(QJsonArray{q.x(),q.y()});return drawnPath(QJsonObject{{"kind",kind},{"path",path},{"flag2",true},{"style",o["style"].toInt()},{"rotation",o["rotation"].toDouble(200)}});};
        auto shapes=[&]{
            if(type=="rectangle"||type=="ellipse"){QRectF r(QPointF(),QPointF(o["x2"].toDouble(),o["y2"].toDouble())-at);r=r.normalized();if(fill)fillBrush();
                if(type=="rectangle"&&o["flag2"].toBool())p.drawPolygon(smoothed(QPolygonF{r.topLeft(),r.topRight(),r.bottomRight(),r.bottomLeft()},6));else if(type=="rectangle")p.drawRect(r);else p.drawEllipse(r);}
            if(type=="polygon"||type=="polyline"||type=="lead"){auto poly=polygon(o["points"]);if(o["flag2"].toBool())poly=smoothed(poly,type=="polygon"?6:type=="lead"?9:4);if(fill)fillBrush();if(type=="polygon")p.drawPolygon(poly);else p.drawPolyline(poly);}
        };
        shapes();if(milled){p.setPen(QPen(QColor(0xc0,0xc0,0xc0),0));shapes();}
        if(!milled&&(type=="polygon"||type=="rectangle"||type=="ellipse")&&o.contains("bitmap")&&view.bitmaps&&!view.mono){
            const QRectF bounds=QRectF(QPointF(),QPointF(o["x2"].toDouble(),o["y2"].toDouble())-at).normalized();QPainterPath round;round.addEllipse(bounds);
            QPolygonF outline=type=="polygon"?polygon(o["points"]):type=="ellipse"?round.toFillPolygon():QPolygonF(bounds);
            paintPicture(p,QImage::fromData(legacyBitmapData(QByteArray::fromBase64(o["bitmap"].toString().toLatin1())),"BMP"),outline,o["anchors"].toArray());
        }
        if(type=="drill")paintHole(p,{},o["diameter"].toDouble(.9),view.mono);
        // Pins look like LochMaster's kind 11: a dot in their own colour with their width as diameter.
        if(type=="pin"){const double w=qMax(1.0,o["width"].toDouble(45));p.setPen(Qt::NoPen);p.setBrush(view.mono?QColor(Qt::black):QColor(o["color"].toString("#b53831")));p.drawEllipse(QPointF(),w/2,w/2);}
        if(type=="solder"){const double r=qMax(1.0,o["width"].toDouble(150))/2;if(view.bitmaps&&!view.mono)paintSolderBlob(p,{},r);else{p.setPen(QPen(QColor(o["color"].toString("#c0c0c0")),0));p.setBrush(Qt::white);p.drawEllipse(QPointF(),r,r);}}
        if(type=="potential"){p.setPen(QPen(QColor(255,0,0),0));p.setBrush(Qt::white);p.drawEllipse(QPointF(),100,100);p.setPen(Qt::NoPen);p.setBrush(QColor(o["color"].toString()));p.drawEllipse(QPointF(),50,50);}
        // Cuts and pads look like the LochMaster objects they are saved as: a gap (TTrenner) and a pad with hole (TAuge).
        if(type=="cut")paintCut(p,QRectF(-85,-85,170,170),view.mono);
        if(type=="pad"||type=="eye"){paintPad(p,{},std::round(o["diameter"].toDouble(type=="pad"?1.8:2)*50),view.mono,view.backActive());if(type=="pad")paintHole(p,{},o["drill"].toDouble(.8),view.mono);}
        if(type=="track"){p.setPen(QPen(view.mono?QColor("#eaeaea"):view.backActive()?board::copperBack:board::copper,qMax(20.0,o["width"].toDouble(200)),Qt::SolidLine,Qt::FlatCap,Qt::MiterJoin));p.drawLine(QPointF(),QPointF(o["x2"].toDouble(),o["y2"].toDouble())-at);}
        if(type=="resistor"){p.drawLine(-254,0,-127,0);p.drawRect(QRectF(-127,-75,254,150));p.drawLine(127,0,254,0);}
        if(type=="capacitor"){p.drawLine(-254,0,-40,0);p.drawLine(-40,-100,-40,100);p.drawLine(40,-100,40,100);p.drawLine(40,0,254,0);}
        if(type=="diode"){p.drawLine(-254,0,-100,0);p.drawPolygon(QPolygonF{QPointF(-100,-100),QPointF(-100,100),QPointF(100,0)});p.drawLine(100,-100,100,100);p.drawLine(100,0,254,0);}
        if(type=="ground"){p.drawLine(0,-254,0,0);p.drawLine(-150,0,150,0);p.drawLine(-100,65,100,65);p.drawLine(-50,130,50,130);}
        // Text sits on a baseline 130 units above the anchor and is drawn like the label it is saved as (Arial, cell height = size).
        if(view.texts&&!o["text"].toString().isEmpty()){
            QFont f("Arial");f.setPixelSize(100);const QFontMetricsF m(f);const double h=qBound(20.0,o["textSize"].toDouble(120),2000.0);
            const QJsonObject label{{"text_position",QJsonArray{type=="text"?0:-127,-130-h*m.ascent()/(m.ascent()+m.descent())}},{"text_kind",h}};
            paintLabel(p,o["text"].toString(),f,labelCorners(label,o["text"].toString(),f),((o["mirrorX"].toBool()?1:0)|(o["mirrorY"].toBool()?2:0))^view.textMirror,p.pen().color(),nullptr);
        }
    }
    p.restore();
}
QImage renderNode(const QJsonObject &node,const QByteArray &source,double scale,qreal ratio,bool bitmaps) {
    auto r=nodeBounds(node);const double pixels=scale*ratio;if(!r.isValid()||pixels<=0||r.width()*pixels>4096||r.height()*pixels>4096)return {};
    QImage image(qMax(1,int(std::ceil(r.width()*pixels))),qMax(1,int(std::ceil(r.height()*pixels))),QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter p(&image);p.setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform|QPainter::TextAntialiasing);p.scale(pixels,pixels);p.translate(-r.topLeft());ViewState view;view.bitmaps=bitmaps;drawNode(p,node,source,{},{},view);p.end();
    image.setDevicePixelRatio(ratio);return image;
}
class DrawingItem final:public QGraphicsItem {
public:
    QJsonObject node;QByteArray source;QRectF bounds;QString title;ViewState view;int mirror=0; // mirror flags of the placement
    DrawingItem(QJsonObject n,const QByteArray &s,const QString &t={}):node(std::move(n)),source(s),bounds(nodeBounds(node)),title(t){}
    QRectF boundingRect() const override{return bounds;}
    void paint(QPainter *p,const QStyleOptionGraphicsItem *,QWidget *) override {
        drawNode(*p,node,source,{},title,view,mirror,isSelected());
        if(isSelected()){p->setPen(QPen(QColor("#257bca"),0,Qt::DashLine));p->setBrush(Qt::NoBrush);p->drawRect(bounds);}
    }
};
class BoardItem final:public QGraphicsItem {
public:
    QRectF board;bool holes,schematic,mono=false,back=false; // S/W: white board, light grey pads, white holes; back: solder side active
    BoardItem(QRectF rect,bool drawHoles,bool isSchematic):board(rect),holes(drawHoles),schematic(isSchematic){setFlags(ItemClipsChildrenToShape|ItemUsesExtendedStyleOption);}
    QRectF boundingRect() const override{return board;}
    void paint(QPainter *p,const QStyleOptionGraphicsItem *option,QWidget *) override {
        p->setPen(mono?QPen(QColor(Qt::black),0):QPen(Qt::NoPen));p->setBrush(schematic||mono?QColor(Qt::white):board::fill);p->drawRect(this->board);
        if(!schematic&&!mono){QPen bevel(board::bevel,2);bevel.setCosmetic(true);p->setPen(bevel);p->drawLine(this->board.topLeft(),this->board.topRight());p->drawLine(this->board.topLeft(),this->board.bottomLeft());}
        if(!holes||QStyleOptionGraphicsItem::levelOfDetailFromTransform(p->worldTransform())*254<3)return;
        auto area=option->exposedRect.intersected(board);
        for(double x=qMax(254.0,std::ceil((area.left()-90)/254)*254);x<qMin(board.right(),area.right()+90);x+=254)
            for(double y=qMax(254.0,std::ceil((area.top()-90)/254)*254);y<qMin(board.bottom(),area.bottom()+90);y+=254){
                // The same pad with hole that the LM4 export writes for a generated board (TAuge 1.8 mm, TBohrung 0.9 mm).
                paintPad(*p,QPointF(x,y),90,mono,back);paintHole(*p,QPointF(x,y),.9,mono);
            }
    }
};
Canvas::Canvas(QWidget *parent):QGraphicsView(parent) {
    setScene(&content);setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
    setBackgroundBrush(QColor("#ece9da"));setDragMode(RubberBandDrag);setTransformationAnchor(AnchorUnderMouse);setMouseTracking(true);setAcceptDrops(true);
    connect(&content,&QGraphicsScene::selectionChanged,this,[this]{viewport()->update();}); // outline handles follow the selection
}
void Canvas::setProject(Project *p){content.clearSelection();dragging=false;project=p;placement={};lastPaste.clear();pasteStep=0;rebuild();setTool("select");fit();}
void Canvas::rebuild() {
    QList<QPair<QString,int>> selected;for(auto *item:content.selectedItems())selected.append({item->data(1).toString(),item->data(0).toInt()});
    hasStart=false;preview=nullptr;placementPreview=nullptr;continuityMarks.clear();potentialMarks.clear();freeMarks.clear();shortMarks.clear();airwireMarks.clear();boardItem=nullptr;content.clear();boardHoles.clear();generatedBoard=false;if(!project)return;
    QRectF board(0,0,project->width,project->height);
    bool templatePresent=!project->boardSource.isEmpty()||!project->legacy.value("board").toObject().isEmpty()||!project->legacy.isEmpty();
    generatedBoard=project->mode=="board"&&!templatePresent;
    auto *base=new BoardItem(board,project->mode=="board"&&!templatePresent,project->mode=="schematic");base->mono=views.mono;base->back=views.backActive();content.addItem(base);boardItem=base;base->setZValue(-100);
    // Like the original, the inactive side is drawn first so that the active side lies on top of its x-ray image.
    auto layer=[&](const QJsonObject &n){const auto s=solderSide(n);return project->mode=="board"&&s&&*s!=views.backActive()?-60.0:0.0;}; // above the board (-100), below the active side
    QPointF shift=-project->offset();
    auto boardDoc=project->legacy.value("board").toObject();QByteArray boardBytes=project->original;QPointF boardShift=shift;
    if(!project->boardSource.isEmpty()){auto source=project->libraries[project->boardSource];boardDoc=source.document;boardBytes=source.bytes;boardShift=-point(boardDoc["origin"].toArray());}
    if(!boardDoc.isEmpty()) {
        QJsonObject group{{"children",boardDoc["objects"]}};
        for(auto p:boardHolePoints(group)){
            p+=boardShift;if(board.contains(p))boardHoles.append(p);
        }
        // Like the original, the layout's drill holes stay visible above its copper: copper first, holes in a second pass.
        std::function<QJsonObject(QJsonObject,bool)> only=[&](QJsonObject n,bool drills){
            if(n.contains("children")){QJsonArray kept;for(auto v:n["children"].toArray()){auto c=only(v.toObject(),drills);if(!c.isEmpty())kept.append(c);}if(kept.isEmpty()&&!n.contains("type"))return QJsonObject{{"children",kept}};if(kept.isEmpty())return QJsonObject{};n["children"]=kept;return n;}
            return (n["type"]=="TBohrung")==drills?n:QJsonObject{};
        };
        for(bool drills:{false,true}){auto *underlay=new DrawingItem(only(group,drills),boardBytes,project->title);underlay->view=effectiveView();underlay->setParentItem(base);underlay->setPos(boardShift);underlay->setZValue(drills?2:1);}
    }
    auto setup=[&](DrawingItem *item,const QString &kind,int index,QPointF pos){
        content.addItem(item);item->view=effectiveView();item->setFlags(QGraphicsItem::ItemIsSelectable|QGraphicsItem::ItemIsMovable);
        item->setData(0,index);item->setData(1,kind);item->setData(2,pos);item->setPos(pos);
        if(selected.contains({kind,index}))item->setSelected(true);
    };
    for(int i=0;i<project->legacy.value("objects").toArray().size();i++) {
        auto node=project->legacyNode(i);if(node["deleted"].toBool()||(!showComponents&&!views.backActive()&&node.contains("children")))continue;
        auto *item=new DrawingItem(node,project->original,project->title);
        setup(item,"legacy",i,shift+point(project->moves.value(QString::number(i)).toArray()));
        item->setTransform(objectTransform(node,componentAnchor(project->legacy.value("objects").toArray()[i].toObject())));item->setZValue(node["z"].toDouble(10+i*.001)+layer(node));
        item->mirror=(node["mirrorX"].toBool()?1:0)|(node["mirrorY"].toBool()?2:0);
        if(project->sourceKind=="lmb"&&project->boardSource.isEmpty())for(auto p:boardHolePoints(node)){
            p=item->mapToScene(p);if(board.contains(p))boardHoles.append(p);
        }
    }
    for(int i=0;i<project->additions.size();i++) {
        auto node=project->additions[i].toObject();DrawingItem *item;
        if(node["type"]=="component") {
            auto geometry=project->componentNode(node);if(!showComponents&&!views.backActive()&&geometry.contains("children"))continue;auto anchor=componentAnchor(project->libraryNode(node["library"].toString(),node["index"].toInt()));
            item=new DrawingItem(geometry,project->libraries[node["library"].toString()].bytes,project->title);
            setup(item,"new",i,QPointF(node["x"].toDouble(),node["y"].toDouble())-anchor);item->setTransform(objectTransform(node,anchor));
            item->mirror=(node["mirrorX"].toBool()?1:0)|(node["mirrorY"].toBool()?2:0);
        }else {item=new DrawingItem(node,{},project->title);setup(item,"new",i,{});}
        item->setZValue(node["z"].toDouble(50+i*.001)+layer(node["type"]=="component"?project->componentNode(node):node));
    }
    // The board and whatever lies beside it, such as parts set down for a schematic.
    content.setSceneRect(board.adjusted(-254,-254,254,254).united(content.itemsBoundingRect().adjusted(-254,-254,254,254)));
    if(project->mode=="board"&&(showPotentialsEnabled||showFreeEnabled||!shortChain.isEmpty()||(activeTool=="continuity"&&continuityPoint))){
        auto model=continuityModel(*project);markedHoles=model.holes();
        if(showPotentialsEnabled){auto result=model.potentials();showPotentials(result.coloured);if(potentialsComputed)potentialsComputed(result.conflicts);}
        // Free areas override potential colours in LochMaster (clGreen); the continuity tester's cyan stays on top.
        if(showFreeEnabled)freeMarks.append(paintConductors(model.freeCopper(),QColor(0,128,0),2.5,149.5));
        if(activeTool=="continuity"&&continuityPoint)showContinuity(model.trace(*continuityPoint));
        if(!shortChain.isEmpty())shortMarks=paintConductors(shortChain,QColor(220,0,0),3.5,150.5);
    }
    paintAirwires();
    viewport()->update();
}
void Canvas::setAirwires(const QList<QLineF> &lines){airwireLines=lines;paintAirwires();}
void Canvas::setPinNames(const QList<QPair<QPointF,QString>> &names){pinNameMarks=names;paintAirwires();}
// Thin lines straight from pin to pin, as layout programs show connections still to be made; pin names upright at
// screen size whatever the zoom and the view.
void Canvas::paintAirwires(){
    for(auto *item:airwireMarks){content.removeItem(item);delete item;}airwireMarks.clear();
    QPen line(QColor(0,110,255),1.5);line.setCosmetic(true);
    for(const auto &l:airwireLines){auto *item=content.addLine(l,line);item->setZValue(160);airwireMarks<<item;}
    const QColor mark(200,0,120);QFont font=this->font();font.setPixelSize(12);font.setBold(true);
    for(const auto &[at,name]:pinNameMarks){
        auto *dot=content.addEllipse(QRectF(at-QPointF(45,45),QSizeF(90,90)),QPen(Qt::NoPen),QBrush(mark));dot->setZValue(161);airwireMarks<<dot;
        auto *text=content.addSimpleText(name,font);text->setBrush(mark);text->setPos(at);text->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        text->setTransform(QTransform::fromTranslate(6,-18));text->setZValue(161);airwireMarks<<text;
    }
    viewport()->update();
}
QList<Conductor> Canvas::traceContinuity(QPointF at){
    clearContinuity();if(!project)return {};
    auto model=continuityModel(*project);markedHoles=model.holes();auto found=model.trace(at);continuityPoint=at;showContinuity(found);
    if(continuityTraced){int copper=0;for(const auto &c:found)copper+=c.copper;continuityTraced(copper,int(found.size())-copper);}
    return found;
}
void Canvas::clearContinuity(){for(auto *item:continuityMarks){content.removeItem(item);delete item;}continuityMarks.clear();continuityPoint.reset();}
// LochMaster repaints connected objects cyan: copper below the parts, wires and solder blobs above them.
void Canvas::showContinuity(const QList<Conductor> &found){continuityMarks.append(paintConductors(found,QColor(0,255,255),3,150));}
void Canvas::showPotentials(const QList<Conductor> &coloured){
    QMap<QRgb,QList<Conductor>> nets;for(const auto &c:coloured)nets[c.potential.rgb()].append(c);
    for(auto it=nets.begin();it!=nets.end();++it)potentialMarks.append(paintConductors(it.value(),QColor::fromRgb(it.key()),2,149));
}
// Repaints conductors in one colour: copper inside the board below the parts, wires and solder blobs above them.
QList<QGraphicsItem*> Canvas::paintConductors(const QList<Conductor> &conductors,const QColor &colour,double copperZ,double wireZ){
    QPainterPath copper,wires;copper.setFillRule(Qt::WindingFill);wires.setFillRule(Qt::WindingFill);
    for(const auto &c:conductors){
        if(c.copper){copper.addPath(c.shape);continue;}
        // Solder blobs show their area; pins and other zero-length leads a dot of their width.
        if(c.kind==19&&!c.shape.isEmpty()){wires.addPath(c.shape);continue;}
        if(c.path.boundingRect().isNull()){if(!c.path.isEmpty())wires.addEllipse(c.path.first(),qMax(10.0,c.width/2),qMax(10.0,c.width/2));continue;}
        QPainterPath line;line.addPolygon(c.path);QPainterPathStroker stroke;stroke.setWidth(qMax(20.0,c.width));stroke.setCapStyle(Qt::RoundCap);stroke.setJoinStyle(Qt::RoundJoin);wires.addPath(stroke.createStroke(line));
    }
    // Drill holes lie above the copper in LochMaster, so they stay visible inside coloured copper.
    QPainterPath holes;
    for(const auto &[centre,diameter]:markedHoles)for(const auto &c:conductors)if(c.copper&&c.bounds.contains(centre)&&c.shape.contains(centre)){holes.addEllipse(centre,diameter*50,diameter*50);break;}
    QList<QGraphicsItem*> items;
    for(int layer=0;layer<3;layer++){
        auto *mark=new QGraphicsPathItem(layer==1?wires:layer==2?holes:copper);mark->setPen(Qt::NoPen);mark->setBrush(layer==2?QColor("#342e26"):colour);mark->setAcceptedMouseButtons(Qt::NoButton);
        if(layer==1||!boardItem){content.addItem(mark);mark->setZValue(layer==1?wireZ:wireZ-.5);}else{mark->setParentItem(boardItem);mark->setZValue(copperZ+(layer==2?.05:0));}
        items.append(mark);
    }
    return items;
}
void Canvas::setShowPotentials(bool shown){showPotentialsEnabled=shown;rebuild();}
void Canvas::setShowFreeAreas(bool shown){showFreeEnabled=shown;rebuild();}
// Short check: the conducting chain between two potentials in red, centred in the view.
void Canvas::showShort(const QList<Conductor> &chain){
    clearShort();shortChain=chain;if(chain.isEmpty()||!project)return;
    markedHoles=continuityModel(*project).holes();shortMarks=paintConductors(chain,QColor(220,0,0),3.5,150.5);
    QRectF area;for(const auto &c:chain)area=area.united(c.bounds);centerOn(area.center());
}
void Canvas::clearShort(){for(auto *item:shortMarks){content.removeItem(item);delete item;}shortMarks.clear();shortChain.clear();}
// Fitting before the first show uses a placeholder viewport; repeat it once the real size is known.
void Canvas::fit(){if(project){if(!isVisible())fitPending=true;resetTransform();fitInView(QRectF(0,0,project->width,project->height).adjusted(-100,-100,100,100),Qt::KeepAspectRatio);if(views.flip)scale(1,-1);}}
void Canvas::fitArea(const QRectF &area){if(area.isEmpty())return;resetTransform();fitInView(area.adjusted(-100,-100,100,100),Qt::KeepAspectRatio);if(views.flip)scale(1,-1);}
void Canvas::fitObjects(bool selectedOnly){
    QRectF r;for(auto *item:content.items())if(item->flags().testFlag(QGraphicsItem::ItemIsSelectable)&&(!selectedOnly||item->isSelected()))r=r.united(item->sceneBoundingRect());
    if(r.isEmpty())return;resetTransform();fitInView(r.adjusted(-100,-100,100,100),Qt::KeepAspectRatio);if(views.flip)scale(1,-1);
}
// One document unit is 1/100 mm; the screen's logical pixels per inch come from its physical size.
void Canvas::realSize(){
    auto *s=screen();if(!s||s->physicalSize().width()<=0)return;const double perInch=s->geometry().width()/(s->physicalSize().width()/25.4);
    const auto centre=mapToScene(viewport()->rect().center());resetTransform();scale(perInch/2540,perInch/2540);if(views.flip)scale(1,-1);centerOn(centre);
}
void Canvas::showEvent(QShowEvent *e){QGraphicsView::showEvent(e);if(fitPending){fitPending=false;QTimer::singleShot(0,this,[this]{fit();});}}
// The original's pointers per tool: soldering irons for wiring, pencils for drawing, magnifier, cross hairs.
static QCursor toolCursor(const QString &tool){
    static const QHash<QString,QString> pointers{{"zoom","lupe"},{"polyline","stift"},{"ellipse","stiftcirc"},{"rectangle","stiftrect"},{"polygon","stiftpoly"},{"text","stifttext"},
        {"wire","kolbendraht"},{"pin","kolbenpin"},{"lead","kolbenanschluss"},{"solder","loeten"},{"pad","kolben"},{"track","kolben"},{"continuity","kolbentest"},{"cut","kreuz"},{"drill","kreuz"},
        {"origin","kreuz"},{"potential","finger"},{"resistor","stift"},{"capacitor","stift"},{"diode","stift"},{"ground","stift"}};
    if(tool=="select"||!pointers.contains(tool))return QCursor(Qt::ArrowCursor);
    return openLochCursor(pointers.value(tool));
}
void Canvas::setTool(const QString &name){
    if(name!="continuity")clearContinuity();activeTool=name;hasStart=false;contour={};
    if(preview){content.removeItem(preview);delete preview;preview=nullptr;}
    if(placementPreview){content.removeItem(placementPreview);delete placementPreview;placementPreview=nullptr;}
    setDragMode(name=="select"?RubberBandDrag:NoDrag);setCursor(toolCursor(name));if(toolChanged)toolChanged(name);
}
void Canvas::beginPlacement(const QString &library,int index){
    auto node=project->libraryNode(library,index);
    placement=QJsonObject{{"type","component"},{"library",library},{"index",index},{"id",node["id"]},{"value",node["value"]},{"description",node["description"]}};setTool("component");
}
void Canvas::setGrid(double units){fixedGrid=units;viewport()->update();}
void Canvas::setShowComponents(bool b){showComponents=b;rebuild();}
void Canvas::setViewMode(const QString &mode){
    if(!QStringList{"front","back","xray","outline"}.contains(mode))return;ViewState state=views;
    state.flip=mode=="back";state.through=mode=="xray";state.mono=mode=="outline";setViewState(state);
}
void Canvas::setViewState(const ViewState &state){const bool refit=state.flip!=views.flip;views=state;rebuild();if(refit)fit();}
void Canvas::setXRayGrey(int fill,int outline){xrayFill=QColor(fill,fill,fill);xrayOutline=QColor(outline,outline,outline);viewport()->update();}
void Canvas::setRulers(bool enabled){rulers=enabled;viewport()->update();}
void Canvas::setUnit(int unit){units=qBound(0,unit,2);viewport()->update();}
QList<QPointF> Canvas::connectionTargets() const {
    QList<QPointF> result;
    for(auto *item:content.items())if(item->isVisible()&&item->flags().testFlag(QGraphicsItem::ItemIsSelectable)){
        if(auto *drawing=dynamic_cast<DrawingItem*>(item))for(auto p:connectionPoints(drawing->node)){
            auto at=item->mapToScene(p);if(!result.contains(at))result.append(at);
        }
    }
    return result;
}
QPointF Canvas::snap(QPointF p) const {
    // Shift places exactly; otherwise terminals and holes come first for wires, then the grid if it is not too fine.
    if(pointerModifiers&Qt::ShiftModifier)return p;
    if(activeTool=="wire"){
        double nearest=12/qMax(.00001,std::hypot(transform().m11(),transform().m12()));QPointF target=p;bool found=false;
        for(auto at:connectionTargets()){double distance=QLineF(p,at).length();if(distance<=nearest){nearest=distance;target=at;found=true;}}
        if(found)return target;
        if(project&&project->mode=="board"){auto hole=nearestBoardHole(p);if(hole&&QLineF(p,*hole).length()<=nearest)return *hole;}
    }
    // The grid counts from the user origin, as in the original; tracks and pads with drill snap to the hole pitch.
    const double step=activeTool=="track"||activeTool=="pad"?254:gridStep();if(step<1)return p;
    const QPointF o=project?project->origin():QPointF();return {o.x()+std::round((p.x()-o.x())/step)*step,o.y()+std::round((p.y()-o.y())/step)*step};
}
std::optional<QPointF> Canvas::nearestBoardHole(QPointF p) const {
    if(generatedBoard){
        const QPointF hole(std::round(p.x()/254)*254,std::round(p.y()/254)*254);
        if(hole.x()>=254&&hole.y()>=254&&hole.x()<project->width&&hole.y()<project->height)return hole;
    }
    // Avoid jumping across large unperforated areas of a template.
    double nearest=508;std::optional<QPointF> target;
    for(auto hole:boardHoles){double distance=QLineF(p,hole).length();if(distance<nearest){nearest=distance;target=hole;}}
    return target;
}
static QPointF referenceConnection(const QJsonObject &node,QPointF center){
    // A chosen Bezugspunkt is the first terminal; otherwise the terminal nearest to the middle.
    if(node["reference"].toInt(-1)>=0){const auto terminals=partTerminals(node);if(!terminals.isEmpty())return terminals.first();}
    const auto pins=connectionPoints(node);QPointF result=center;double nearest=std::numeric_limits<double>::max();
    for(auto pin:pins){double distance=QLineF(center,pin).length();if(distance<nearest){nearest=distance;result=pin;}}
    return result;
}
QPointF Canvas::placementPosition(QPointF p) const {
    if(!project||project->mode!="board"||!snapping())return p;
    const auto node=project->componentNode(placement);
    const auto anchor=componentAnchor(project->libraryNode(placement["library"].toString(),placement["index"].toInt()));
    const auto pin=p-anchor+objectTransform(placement,anchor).map(referenceConnection(node,anchor));
    const auto hole=nearestBoardHole(pin);return hole?p+*hole-pin:p;
}
bool Canvas::alignComponentItems(){
    if(!project||project->mode!="board")return false;bool edited=false;QList<QPair<QPointF,QPointF>> connections;
    for(auto *item:content.selectedItems())if(auto *drawing=dynamic_cast<DrawingItem*>(item)){
        if(!drawing->node.contains("children")&&!(item->data(1)=="new"&&project->additions[item->data(0).toInt()].toObject()["type"]=="component"))continue;
        if(connectionPoints(drawing->node).isEmpty())continue;
        const auto pin=item->mapToScene(referenceConnection(drawing->node,drawing->boundingRect().center()));
        if(const auto hole=nearestBoardHole(pin)){
            const auto delta=*hole-pin;if(delta.isNull())continue;
            for(auto p:connectionPoints(drawing->node)){p=item->mapToScene(p);connections.append({p,p+delta});}
            edited|=moveItem(item,delta);
        }
    }
    // Keep native wires attached when correcting an already placed footprint.
    for(int i=0;i<project->additions.size()&&!connections.isEmpty();i++){
        auto n=project->additions[i].toObject();if(n.value("type")!="wire"||n.value("angle").toDouble()!=0||n.value("mirrorX").toBool()||n.value("mirrorY").toBool())continue;
        for(auto keys:{std::pair{"x","y"},std::pair{"x2","y2"}}){
            const QPointF at(n[keys.first].toDouble(),n[keys.second].toDouble());
            for(const auto &move:connections)if(QLineF(at,move.first).length()<.001){n[keys.first]=move.second.x();n[keys.second]=move.second.y();break;}
        }
        project->additions[i]=n;
    }
    return edited;
}
void Canvas::alignSelectedToBoard(){
    if(!project||project->mode!="board"||selectionCount()==0)return;
    if(beforeChange)beforeChange();bool edited=alignComponentItems();rebuild();if(edited&&changed)changed();
}
// A Kennung like "R#" stays and the part gets the next free number, as in the original; a literal one counts on.
static void numberPart(const Project &project,QJsonObject &n){const auto id=n.value("id").toString();if(id.contains('#'))n["group_value"]=project.nextNumber(id);else if(!id.isEmpty())n["id"]=project.nextId(id);}
void Canvas::placeAt(QPointF at){at=placementPosition(at);auto o=placement;o["x"]=at.x();o["y"]=at.y();numberPart(*project,o);addElement(o);}
void Canvas::movePlacementPreview(QPointF at){
    auto geometry=project->componentNode(placement);auto anchor=componentAnchor(project->libraryNode(placement["library"].toString(),placement["index"].toInt()));
    if(!placementPreview){auto *ghost=new DrawingItem(geometry,project->libraries[placement["library"].toString()].bytes,project->title);content.addItem(ghost);ghost->setOpacity(.55);ghost->setZValue(200);ghost->setAcceptedMouseButtons(Qt::NoButton);placementPreview=ghost;}
    placementPreview->setPos(placementPosition(at)-anchor);placementPreview->setTransform(objectTransform(placement,anchor));
}
// A part dragged from the library panel arrives with placement already armed by the window.
bool Canvas::acceptsPart(const QMimeData *data) const{return project&&activeTool=="component"&&!placement.isEmpty()&&data&&data->hasFormat(libraryPartMime);}
// Always copy: a proposed move (e.g. with ⌘) would make the list remove the part from the library panel.
void Canvas::dragEnterEvent(QDragEnterEvent *e){if(acceptsPart(e->mimeData())){e->setDropAction(Qt::CopyAction);e->accept();}else QGraphicsView::dragEnterEvent(e);}
void Canvas::dragMoveEvent(QDragMoveEvent *e){pointerModifiers=e->modifiers();if(!acceptsPart(e->mimeData())){QGraphicsView::dragMoveEvent(e);return;}movePlacementPreview(snap(mapToScene(e->position().toPoint())));e->setDropAction(Qt::CopyAction);e->accept();}
void Canvas::dropEvent(QDropEvent *e){
    pointerModifiers=e->modifiers();if(!acceptsPart(e->mimeData())){QGraphicsView::dropEvent(e);return;}
    auto at=snap(mapToScene(e->position().toPoint()));if(QRectF(0,0,project->width,project->height).contains(at))placeAt(at);e->setDropAction(Qt::CopyAction);e->accept();
}
void Canvas::addElement(const QJsonObject &o){if(beforeChange)beforeChange();project->additions.append(o);rebuild();selectObject("new",project->additions.size()-1);if(changed)changed();}
void Canvas::mousePressEvent(QMouseEvent *e) {
    pointerModifiers=e->modifiers();if(!project)return;
    if(e->button()==Qt::RightButton){
        // Right click on an outline handle: add or delete that point; the zoom loupe zooms out; otherwise end the tool.
        if(activeTool=="select"){const auto handles=nodeHandles();for(int i=0;i<handles.size();i++)if(QLineF(QPointF(mapFromScene(handles[i])),e->position()).length()<=6){
            QMenu menu(this);auto *add=menu.addAction(ui("Knoten hinzufügen"));auto *remove=menu.addAction(ui("Knoten löschen"));auto *chosen=menu.exec(e->globalPosition().toPoint());
            if(chosen==add)addNode(i);else if(chosen==remove)deleteNode(i);return;}
            if(contextRequested){auto *item=itemAt(e->pos());while(item&&!item->flags().testFlag(QGraphicsItem::ItemIsSelectable))item=item->parentItem();
                if(item&&!item->isSelected()){content.clearSelection();item->setSelected(true);}contextRequested(e->globalPosition().toPoint());return;}}
        if(activeTool=="zoom"){const auto at=mapToScene(e->pos());scale(1/1.2,1/1.2);centerOn(at);return;}
        setTool("select");return;
    }
    if(activeTool=="zoom"){zoomStart=e->pos();return;}
    if(e->button()!=Qt::LeftButton){QGraphicsView::mousePressEvent(e);return;}
    if(activeTool=="select"){
        // A handle of the selected outline takes the click: drag that point.
        const auto handles=nodeHandles();for(int i=0;i<handles.size();i++)if(QLineF(QPointF(mapFromScene(handles[i])),e->position()).length()<=6){nodeDrag=i;nodeTarget=handles[i];return;}
        if(beforeChange)beforeChange();dragging=true;QGraphicsView::mousePressEvent(e);return;
    }
    if(activeTool=="continuity"){traceContinuity(mapToScene(e->pos()));return;}
    // "Ursprung setzen": the clicked grid point (Shift: exactly the pointer) becomes the origin; then back to selecting.
    if(activeTool=="origin"){const auto at=snap(mapToScene(e->pos()));if(beforeChange)beforeChange();
        project->userOrigin=QJsonArray{std::round(at.x()),std::round(at.y())};setTool("select");viewport()->update();if(changed)changed();return;}
    auto at=snap(mapToScene(e->pos()));if(!QRectF(0,0,project->width,project->height).contains(at))return;
    if(activeTool=="component"){placeAt(at);return;}
    QJsonObject o{{"type",activeTool},{"x",at.x()},{"y",at.y()},{"color",project->mode=="board"?"#375f9b":"#253b35"},{"width",project->mode=="board"?45:20}};
    const auto defaults=toolDefaults.value(activeTool);for(auto it=defaults.begin();it!=defaults.end();++it)o[it.key()]=it.value();
    if(activeTool=="wire") {
        if(!hasStart){start=at;hasStart=true;return;}
        o["x"]=start.x();o["y"]=start.y();o["x2"]=at.x();o["y2"]=at.y();addElement(o);start=at;hasStart=true;return;
    }
    // A track is drawn from one click to the next; each track stands alone.
    if(activeTool=="track") {
        if(!hasStart){start=at;hasStart=true;return;}if(at==start)return;
        o["x"]=start.x();o["y"]=start.y();o["x2"]=at.x();o["y2"]=at.y();o.remove("color");hasStart=false;if(preview){content.removeItem(preview);delete preview;preview=nullptr;}
        if(!confirmNew||confirmNew(o))addElement(o);return;
    }
    if(activeTool=="rectangle"||activeTool=="ellipse"){
        if(!hasStart){start=at;hasStart=true;return;}if(at==start)return;o["x"]=start.x();o["y"]=start.y();o["x2"]=at.x();o["y2"]=at.y();addElement(o);return;
    }
    if(activeTool=="polygon"||activeTool=="polyline"||activeTool=="lead"){
        if(contour.isEmpty())start=at;
        auto p=at-start;QJsonArray pair{p.x(),p.y()};if(contour.isEmpty()||contour.last()!=pair)contour.append(pair);hasStart=true;return;
    }
    if(activeTool=="text"||activeTool=="resistor"||activeTool=="capacitor"||activeTool=="diode") {
        bool ok=false;auto text=QInputDialog::getText(this,ui("Beschriftung"),ui("Text oder Bauteilwert"),QLineEdit::Normal,activeTool=="resistor"?"R1 · 10 kΩ":activeTool=="capacitor"?"C1 · 100 nF":activeTool=="diode"?"D1":"",&ok);
        if(!ok)return;o["text"]=text;
    }
    // A solder blob (LochMaster's "Lötstelle"): 1.5 mm, on the side being edited.
    if(activeTool=="solder"){o["width"]=150;o["color"]="#c0c0c0";o["back"]=views.backActive();}
    if(activeTool=="potential"){QString name;QColor colour;if(!potentialRequested||!potentialRequested(name,colour))return;o["name"]=name;o["color"]=colour.name();o.remove("width");}
    if(activeTool=="eye"||activeTool=="pad"||activeTool=="drill")o.remove("width");
    if(confirmNew&&!confirmNew(o))return;
    addElement(o);
}
void Canvas::mouseMoveEvent(QMouseEvent *e) {
    pointerModifiers=e->modifiers();auto at=snap(mapToScene(e->pos()));if(pointerMoved)pointerMoved(at-(project?project->origin():QPointF()));
    if(nodeDrag>=0){nodeTarget=at;viewport()->update();return;}
    if(activeTool=="component"&&!placement.isEmpty())movePlacementPreview(at);
    if(hasStart&&(activeTool=="wire"||activeTool=="track")) {
        if(!preview)preview=content.addPath({},QPen(QColor("#3386b7"),15,Qt::DashLine));
        QPainterPath p(start);p.lineTo(at);preview->setPath(p);preview->setZValue(100);
    }
    if(hasStart&&QStringList{"rectangle","ellipse","polygon","polyline","lead"}.contains(activeTool)){
        if(!preview)preview=content.addPath({},QPen(QColor("#3386b7"),15,Qt::DashLine));QPainterPath p;
        if(activeTool=="rectangle")p.addRect(QRectF(start,at).normalized());else if(activeTool=="ellipse")p.addEllipse(QRectF(start,at).normalized());
        else{p.moveTo(start);for(auto value:contour)p.lineTo(start+point(value.toArray()));p.lineTo(at);if(activeTool=="polygon")p.closeSubpath();}preview->setPath(p);preview->setZValue(100);
    }
    QGraphicsView::mouseMoveEvent(e);
}
void Canvas::mouseReleaseEvent(QMouseEvent *e) {
    pointerModifiers=e->modifiers();
    // Zoom loupe: a click zooms in by 1.2 at the pointer, a drag of 5 pixels or more fits the dragged rectangle.
    if(activeTool=="zoom"&&e->button()==Qt::LeftButton){
        const QRect drag=QRect(zoomStart,e->pos()).normalized();
        if(drag.width()<5&&drag.height()<5){const auto at=mapToScene(e->pos());scale(1.2,1.2);centerOn(at);}
        else{const auto area=mapToScene(drag).boundingRect();resetTransform();fitInView(area,Qt::KeepAspectRatio);if(views.flip)scale(1,-1);}
        return;
    }
    if(nodeDrag>=0){
        QGraphicsItem *item;const auto t=nodeTransform(&item);auto path=selectedPath();const int index=nodeDrag;nodeDrag=-1;
        if(item&&index<path.size()){
            const bool closed=path.size()>2&&path.first()==path.last();const auto p=t.inverted().map(item->mapFromScene(nodeTarget));
            const bool absolute=selectedNode()["type"]=="component"||item->data(1)=="legacy";const QJsonArray moved{absolute?std::round(p.x()):p.x(),absolute?std::round(p.y()):p.y()};
            if(moved!=path[index].toArray()){path[index]=moved;if(closed&&index==0)path[path.size()-1]=moved;try{editPath(path);}catch(const std::exception &){}}
        }
        viewport()->update();return;
    }
    QGraphicsView::mouseReleaseEvent(e);if(!project||!dragging)return;dragging=false;bool edited=false;
    for(auto *item:content.selectedItems()) {
        int index=item->data(0).toInt();QString type=item->data(1).toString();auto delta=item->pos()-item->data(2).toPointF();
        // Moved by whole grid steps, so that objects on the grid stay on it.
        if(snapping()){const double g=gridStep();delta={std::round(delta.x()/g)*g,std::round(delta.y()/g)*g};}if(delta.isNull())continue;
        if(type=="legacy") {auto old=point(project->moves.value(QString::number(index)).toArray());auto at=old+delta;project->moves[QString::number(index)]=QJsonArray{at.x(),at.y()};edited=true;}
        else if(type=="new"&&index>=0&&index<project->additions.size()) {
            auto o=project->additions[index].toObject();
            for(auto pair:{std::pair{"x","y"},std::pair{"x2","y2"}})if(o.contains(pair.first)){o[pair.first]=o[pair.first].toDouble()+delta.x();o[pair.second]=o[pair.second].toDouble()+delta.y();}
            project->additions[index]=o;edited=true;
        }
    }
    // Reset the visual position even when a short drag snaps back to zero.
    rebuild();if(edited&&snapping()&&alignComponentItems())rebuild();if(edited&&changed)changed();
}
void Canvas::wheelEvent(QWheelEvent *e){if(e->modifiers()&Qt::ControlModifier||e->modifiers()&Qt::MetaModifier){double f=std::pow(1.0015,e->angleDelta().y());double s=transform().m11()*f;if(s>.001&&s<50)scale(f,f);e->accept();}else QGraphicsView::wheelEvent(e);}
void Canvas::keyPressEvent(QKeyEvent *e){
    if((e->key()==Qt::Key_Return||e->key()==Qt::Key_Enter)&&!contour.isEmpty()){finishContour();return;}
    if(e->key()==Qt::Key_Escape){setTool("select");return;}
    if(e->key()==Qt::Key_Delete||e->key()==Qt::Key_Backspace){removeSelected();return;}
    if(selectionCount()&&!(e->modifiers()&(Qt::ControlModifier|Qt::MetaModifier|Qt::AltModifier))){
        double step=(gridStep()>=1?gridStep():1)*(e->modifiers().testFlag(Qt::ShiftModifier)?10:1);QPointF delta;
        if(e->key()==Qt::Key_Left)delta={-step,0};if(e->key()==Qt::Key_Right)delta={step,0};
        if(e->key()==Qt::Key_Up)delta={0,-step};if(e->key()==Qt::Key_Down)delta={0,step};if(views.flip)delta.setY(-delta.y()); // up stays up on screen
        if(!delta.isNull()){moveSelected(delta);e->accept();return;}
    }
    QGraphicsView::keyPressEvent(e);
}
int Canvas::selectionCount() const{return content.selectedItems().size();}
void Canvas::selectObjects(const QList<std::pair<QString,int>> &objects){
    {QSignalBlocker quiet(&content);content.clearSelection();
    for(auto *item:content.items())if(item->flags().testFlag(QGraphicsItem::ItemIsSelectable))for(const auto &[kind,index]:objects)if(item->data(1)==kind&&item->data(0).toInt()==index)item->setSelected(true);}
    emit content.selectionChanged();viewport()->update();
}
QList<std::pair<QString,int>> Canvas::selectedObjects() const{QList<std::pair<QString,int>> out;for(auto *item:content.selectedItems())out.append({item->data(1).toString(),item->data(0).toInt()});return out;}
void Canvas::selectObject(const QString &kind,int index){content.clearSelection();for(auto *item:content.items())if(item->data(1)==kind&&item->data(0).toInt()==index){item->setSelected(true);break;}}
QPointF Canvas::selectedCentre() const{auto items=content.selectedItems();return items.size()==1?items.first()->sceneBoundingRect().center():QPointF();}
QJsonObject Canvas::selectedNode() const {
    auto items=content.selectedItems();if(items.size()!=1)return {};auto *item=items.first();int index=item->data(0).toInt();
    return item->data(1)=="legacy"?project->legacyNode(index):project->additions[index].toObject();
}
void Canvas::editSelected(const QJsonObject &properties,QPointF move) {
    auto selected=content.selectedItems();if(selected.isEmpty())return;if(beforeChange)beforeChange();
    for(auto *item:selected){int i=item->data(0).toInt();bool legacy=item->data(1)=="legacy";auto n=legacy?project->edits[QString::number(i)].toObject():project->additions[i].toObject();
        for(auto it=properties.begin();it!=properties.end();++it)n[it.key()]=it.value();
        if(legacy)project->edits[QString::number(i)]=n;else project->additions[i]=n;
        moveItem(item,move); // one history step for the whole dialog
    }
    rebuild();if(changed)changed();
}
// The selected top-level outlines as (item, effective node, legacy?) for the outline buttons.
static bool smoothable(const QJsonObject &n){
    const auto type=n["type"].toString();if(n.contains("children"))return false;
    return type=="TDraht"||type=="TKreis"||type=="polygon"||type=="polyline"||type=="lead"||type=="rectangle"||type=="ellipse";
}
void Canvas::setSmoothing(int style,double size){
    auto items=content.selectedItems();if(!project||items.isEmpty())return;bool changed_=false;
    for(auto *item:items){
        const int i=item->data(0).toInt();const bool old=item->data(1)=="legacy";auto stored=old?project->edits[QString::number(i)].toObject():project->additions[i].toObject();
        const auto effective=old?project->legacyNode(i):stored["type"]=="component"?project->componentNode(stored):stored;if(!smoothable(effective))continue;
        if(!changed_&&beforeChange)beforeChange();changed_=true;
        if(style<0)stored["flag2"]=false;
        else{stored["flag2"]=true;if(effective["type"]!="TKreis")stored["style"]=style;if(style>0)stored["rotation"]=size;} // circles always stay B-spline
        if(old)project->edits[QString::number(i)]=stored;else project->additions[i]=stored;
    }
    if(changed_){rebuild();if(changed)changed();}
}
void Canvas::setMilling(bool on){
    auto items=content.selectedItems();if(!project||items.isEmpty())return;bool changed_=false;
    for(auto *item:items){
        const int i=item->data(0).toInt();const bool old=item->data(1)=="legacy";auto stored=old?project->edits[QString::number(i)].toObject():project->additions[i].toObject();
        const auto effective=old?project->legacyNode(i):stored["type"]=="component"?project->componentNode(stored):stored;const int kind=effective["kind"].toInt();const auto type=effective["type"].toString();
        if(!((type=="TDraht"&&(kind==4||kind==6||kind==7))||type=="TKreis"||type=="polygon"||type=="polyline"||type=="rectangle"||type=="ellipse"))continue;
        if(!changed_&&beforeChange)beforeChange();changed_=true;stored["flag3"]=on;
        if(old)project->edits[QString::number(i)]=stored;else project->additions[i]=stored;
    }
    if(changed_){rebuild();if(changed)changed();}
}
void Canvas::applyStyle(int mode,const QVariant &value){
    auto items=content.selectedItems();if(!project||items.isEmpty())return;bool changed_=false;
    const QColor colour=value.value<QColor>();const qint64 delphi=colour.red()|(colour.green()<<8)|(colour.blue()<<16);
    for(auto *item:items){
        const int i=item->data(0).toInt();const bool old=item->data(1)=="legacy";auto stored=old?project->edits[QString::number(i)].toObject():project->additions[i].toObject();
        const auto effective=old?project->legacyNode(i):stored;const auto type=effective["type"].toString();const int kind=effective["kind"].toInt();
        const bool outline=type=="TDraht"||type=="TDrahtFest"||type=="TKreis",label=type=="TTextLabel",drawn=QStringList{"wire","lead","polyline","polygon","rectangle","ellipse","pin","solder"}.contains(type);
        if(effective.contains("children")||!(outline||label||drawn||(type=="text"&&mode==1))||(label&&mode==0))continue;
        if(!changed_&&beforeChange)beforeChange();changed_=true;
        if(mode==0)stored["width"]=value.toInt();
        else if(mode==1){if(outline||label)stored["pen"]=delphi;else stored["color"]=colour.name();}
        else if(mode==2){
            const bool never=(type!="TKreis"&&outline&&(kind==1||kind==4||kind==9||kind==11||kind==18))||type=="wire"||type=="lead"||type=="polyline"||type=="pin";
            const bool always=(type=="TDraht"&&kind==19)||type=="solder";stored["filled"]=always||(!never&&value.toBool());
        }
        else{if(outline||label)stored["brush"]=delphi;else stored["fill"]=colour.name();
            // A new fill colour replaces the picture; an empty picture also hides one stored in the file.
            if(old&&(effective.contains("bitmap")||effective.contains("bitmap_offset"))){stored["bitmap"]=QString();stored.remove("anchors");}else if(!old){stored.remove("bitmap");stored.remove("anchors");}}
        if(old)project->edits[QString::number(i)]=stored;else project->additions[i]=stored;
    }
    if(changed_){rebuild();if(changed)changed();}
}
void Canvas::switchSide(){
    auto items=content.selectedItems();if(!project||items.isEmpty())return;if(beforeChange)beforeChange();
    for(auto *item:items){
        const int i=item->data(0).toInt();const bool old=item->data(1)=="legacy";auto stored=old?project->edits[QString::number(i)].toObject():project->additions[i].toObject();
        const auto effective=old?project->legacyNode(i):stored["type"]=="component"?project->componentNode(stored):stored;
        if(effective.contains("children")||stored.value("type")=="component")stored["otherSide"]=!stored["otherSide"].toBool();else stored["back"]=!effective["back"].toBool();
        if(old)project->edits[QString::number(i)]=stored;else project->additions[i]=stored;
    }
    rebuild();if(changed)changed();
}
void Canvas::addNode(int index){
    auto path=selectedPath();if(index<0||index>=path.size()||path.size()<2)return;
    const int at=index==0?1:index;const auto a=point(path[at-1].toArray()),b=point(path[at].toArray());
    path.insert(at,QJsonArray{a.x()+std::trunc((b.x()-a.x())/2),a.y()+std::trunc((b.y()-a.y())/2)});
    try{editPath(path);}catch(const std::exception &){}
}
void Canvas::deleteNode(int index){
    auto path=selectedPath();if(index<0||index>=path.size())return;const bool closed=path.size()>2&&path.first()==path.last();
    const int points=int(path.size())-(closed?1:0);if(points<=2){removeSelected();return;}
    path.removeAt(index);if(closed&&index==0)path[path.size()-1]=path[0];
    try{editPath(path);}catch(const std::exception &){}
}
void Canvas::setBitmapFill(const QByteArray &bmp,const QPolygonF &outline){
    auto items=content.selectedItems();if(!project||items.isEmpty()||bmp.isEmpty())return;bool changed_=false;
    for(auto *item:items){
        const int i=item->data(0).toInt();const bool old=item->data(1)=="legacy";auto stored=old?project->edits[QString::number(i)].toObject():project->additions[i].toObject();
        const auto effective=old?project->legacyNode(i):stored["type"]=="component"?project->componentNode(stored):stored;const auto type=effective["type"].toString();const int kind=effective["kind"].toInt();
        QRectF box;
        if(type=="TDraht"&&(kind==6||kind==7)&&!effective.contains("children"))box=polygon(effective["path"]).boundingRect();
        else if(type=="TKreis")box=polygon(effective["inner"].toObject()["path"]).boundingRect(); // the circle's inner outline takes the picture
        else if(type=="rectangle"||type=="ellipse")box=QRectF(QPointF(),QPointF(effective["x2"].toDouble()-effective["x"].toDouble(),effective["y2"].toDouble()-effective["y"].toDouble())).normalized();
        else if(type=="polygon")box=polygon(effective["points"]).boundingRect();else continue;
        const double half=effective["width"].toDouble()/2;box.adjust(-half,-half,half,half);
        if(!changed_&&beforeChange)beforeChange();changed_=true;
        stored["bitmap"]=QString::fromLatin1(bmp.toBase64());stored["anchors"]=QJsonArray{qRound(box.left()),qRound(box.top()),qRound(box.right()),qRound(box.top()),qRound(box.left()),qRound(box.bottom())};
        stored["filled"]=true;stored["flag3"]=false;
        if(outline.size()>=3&&type!="TKreis"){
            // The outline follows the opaque part of the picture over the same box, so the picture keeps its place.
            QJsonArray points;for(const auto &f:outline)points.append(QJsonArray{qRound(box.left()+f.x()*box.width()),qRound(box.top()+f.y()*box.height())});
            if(type=="TDraht"){points.append(points.first());stored["path"]=points;}
            else{stored["type"]="polygon";stored["points"]=points;stored.remove("x2");stored.remove("y2");}
        }
        if(old)project->edits[QString::number(i)]=stored;else project->additions[i]=stored;
    }
    if(changed_){rebuild();if(changed)changed();}
}
void Canvas::rotateBitmapFill(){
    auto items=content.selectedItems();if(!project||items.isEmpty())return;bool changed_=false;
    for(auto *item:items){
        const int i=item->data(0).toInt();const bool old=item->data(1)=="legacy";auto stored=old?project->edits[QString::number(i)].toObject():project->additions[i].toObject();
        auto effective=old?project->legacyNode(i):stored;QByteArray bytes;
        if(effective.contains("bitmap"))bytes=QByteArray::fromBase64(effective["bitmap"].toString().toLatin1());
        else if(old&&effective.contains("bitmap_offset"))bytes=project->original.mid(effective["bitmap_offset"].toInteger(),effective["bitmap_size"].toInteger());
        const QImage img=QImage::fromData(legacyBitmapData(bytes),"BMP");if(img.isNull())continue;
        QByteArray turned;QBuffer buffer(&turned);buffer.open(QIODevice::WriteOnly);img.transformed(QTransform().rotate(90)).convertToFormat(QImage::Format_RGB888).save(&buffer,"BMP");
        if(!changed_&&beforeChange)beforeChange();changed_=true;stored["bitmap"]=QString::fromLatin1(turned.toBase64());
        if(old&&!stored.contains("anchors"))stored["anchors"]=effective["anchors"];if(old)project->edits[QString::number(i)]=stored;else project->additions[i]=stored;
    }
    if(changed_){rebuild();if(changed)changed();}
}
// The original turns and mirrors a selection about one pivot: the first terminal (soldered end of a wire, lead or pin,
// also inside a group) of the selected objects, otherwise the centre of the selection.
QPointF Canvas::selectionPivot() const{
    std::function<std::optional<QPointF>(const QJsonObject&)> terminal=[&](const QJsonObject &n)->std::optional<QPointF>{
        if(n.contains("children")){for(auto v:n["children"].toArray())if(auto t=terminal(v.toObject()))return t;return std::nullopt;}
        const auto path=n["path"].toArray();if(n["type"]=="TDraht"&&(n["kind"]==1||n["kind"]==9||n["kind"]==11)&&!path.isEmpty())return point(path[0].toArray());
        return std::nullopt;
    };
    QRectF box;
    for(auto *item:content.selectedItems()){
        const int i=item->data(0).toInt();const bool old=item->data(1)=="legacy";const auto stored=old?project->legacyNode(i):project->additions[i].toObject();
        if(!old&&stored["type"]!="component"){const auto t=stored["type"].toString();if(t=="wire"||t=="pin"||t=="lead")return QPointF(stored["x"].toDouble(),stored["y"].toDouble());}
        else if(auto t=terminal(old?stored:project->componentNode(stored)))return item->mapToScene(*t);
        box=box.united(item->sceneBoundingRect());
    }
    return {std::round(box.center().x()),std::round(box.center().y())};
}
// Each object turns or mirrors about its own anchor; moving that anchor with the same map about the pivot makes the
// whole selection turn or mirror about the pivot.
void Canvas::transformSelection(const QTransform &map,const std::function<void(QJsonObject&)> &change){
    auto items=content.selectedItems();if(!project||items.isEmpty())return;if(beforeChange)beforeChange();
    for(auto *item:items){
        int i=item->data(0).toInt();bool old=item->data(1)=="legacy";auto n=old?project->edits[QString::number(i)].toObject():project->additions[i].toObject();
        const QPointF anchor=old?item->mapToScene(componentAnchor(project->legacy.value("objects").toArray()[i].toObject())):QPointF(n["x"].toDouble(),n["y"].toDouble());
        const QPointF delta=map.map(anchor)-anchor;change(n);
        if(old){auto m=point(project->moves.value(QString::number(i)).toArray())+delta;project->moves[QString::number(i)]=QJsonArray{m.x(),m.y()};project->edits[QString::number(i)]=n;}
        else{for(auto pair:{std::pair{"x","y"},std::pair{"x2","y2"}})if(n.contains(pair.first)){n[pair.first]=n[pair.first].toDouble()+delta.x();n[pair.second]=n[pair.second].toDouble()+delta.y();}project->additions[i]=n;}
    }
    rebuild();if(gridStep()>=1&&alignComponentItems())rebuild();if(changed)changed();
}
// In the flipped board view the direction is reversed so that "rechts" stays clockwise on screen.
void Canvas::rotateSelected(double degrees){
    if(!project||content.selectedItems().isEmpty())return;if(views.flip)degrees=-degrees;
    const auto c=selectionPivot();QTransform turn;turn.translate(c.x(),c.y());turn.rotate(degrees);turn.translate(-c.x(),-c.y());
    transformSelection(turn,[degrees](QJsonObject &n){n["angle"]=std::remainder(n["angle"].toDouble()+degrees,360.0);});
}
void Canvas::mirrorSelected(bool horizontal){
    if(!project||content.selectedItems().isEmpty())return;const auto c=selectionPivot();
    QTransform flip;flip.translate(c.x(),c.y());flip.scale(horizontal?-1:1,horizontal?1:-1);flip.translate(-c.x(),-c.y());const QString key=horizontal?"mirrorX":"mirrorY";
    // Mirror flags act before the turn (in the object's own frame); mirroring on the board also reverses the angle.
    transformSelection(flip,[key](QJsonObject &n){n[key]=!n[key].toBool();n["angle"]=std::remainder(-n["angle"].toDouble(),360.0);});
}
void Canvas::duplicateSelected(){auto items=content.selectedItems();if(items.isEmpty())return;if(beforeChange)beforeChange();QJsonArray copies;
    for(auto *item:items){int i=item->data(0).toInt();QJsonObject n;
        if(item->data(1)=="legacy"){
            auto key=project->addLibrary(project->original,project->sourceName,project->sourceKind);auto source=project->legacyNode(i);auto anchor=componentAnchor(project->legacy.value("objects").toArray()[i].toObject());auto origin=project->offset();auto move=point(project->moves.value(QString::number(i)).toArray());
            n=QJsonObject{{"type","component"},{"library",key},{"index",i},{"x",anchor.x()-origin.x()+move.x()},{"y",anchor.y()-origin.y()+move.y()}};
            for(auto key:{"id","value","description","angle","mirrorX","mirrorY","text","width","pen","brush","diameter","text_kind","font","path","filled","back","group_flags","otherSide","flag2","style","rotation","flag3","bitmap","anchors"})if(source.contains(key))n[key]=source[key];circlePicture(source,n);
        }else n=project->additions[i].toObject();
        for(auto key:{"x","y","x2","y2"})if(n.contains(key))n[key]=n[key].toDouble()+project->pitch()*100;
        if(item->data(1)=="legacy"&&n.contains("id"))n["id"]=project->rawId("legacy",i);
        if(item->data(1)=="legacy"){const auto edit=project->edits.value(QString::number(i)).toObject();if(edit.contains("nested"))n["nested"]=edit["nested"];}
        n.remove("z");n.remove("uid");n.remove("component");numberPart(*project,n);project->additions.append(n);copies.append(project->additions.size()-1);
    }
    rebuild();content.clearSelection();for(auto v:copies)for(auto *item:content.items())if(item->data(1)=="new"&&item->data(0).toInt()==v.toInt())item->setSelected(true);if(changed)changed();
}
void Canvas::selectAll(){for(auto *item:content.items())if(item->isVisible()&&item->flags().testFlag(QGraphicsItem::ItemIsSelectable))item->setSelected(true);}
void Canvas::clearSelection(){content.clearSelection();}
bool Canvas::moveItem(QGraphicsItem *item,QPointF delta){
    if(delta.isNull())return false;int i=item->data(0).toInt();
    if(item->data(1)=="legacy"){
        const auto key=QString::number(i);auto p=point(project->moves.value(key).toArray())+delta;
        project->moves[key]=QJsonArray{p.x(),p.y()};return true;
    }
    if(item->data(1)=="new"){
        auto n=project->additions[i].toObject();for(auto key:{"x","x2"})if(n.contains(key))n[key]=n[key].toDouble()+delta.x();
        for(auto key:{"y","y2"})if(n.contains(key))n[key]=n[key].toDouble()+delta.y();project->additions[i]=n;return true;
    }return false;
}
void Canvas::moveSelected(QPointF delta){
    if(delta.isNull()||!project||content.selectedItems().isEmpty())return;
    if(beforeChange)beforeChange();bool edited=false;for(auto *item:content.selectedItems())edited|=moveItem(item,delta);
    rebuild();if(edited&&changed)changed();
}
void Canvas::alignSelected(const QString &edge){
    const auto items=content.selectedItems();if(items.size()<2)return;
    if(!QStringList{"left","right","top","bottom","horizontal","vertical"}.contains(edge))return;
    QRectF bounds;bool first=true;for(auto *item:items){bounds=first?item->sceneBoundingRect():bounds.united(item->sceneBoundingRect());first=false;}
    if(beforeChange)beforeChange();bool edited=false;
    for(auto *item:items){auto r=item->sceneBoundingRect();QPointF d;
        if(edge=="left")d.setX(bounds.left()-r.left());if(edge=="right")d.setX(bounds.right()-r.right());
        if(edge=="top")d.setY(bounds.top()-r.top());if(edge=="bottom")d.setY(bounds.bottom()-r.bottom());
        if(edge=="horizontal")d.setY(bounds.center().y()-r.center().y());if(edge=="vertical")d.setX(bounds.center().x()-r.center().x());
        edited|=moveItem(item,d);
    }
    rebuild();if(edited&&changed)changed();
}
void Canvas::finishContour(){
    if(!project||contour.size()<(activeTool=="polygon"?3:2))return;
    QJsonObject n{{"type",activeTool},{"x",start.x()},{"y",start.y()},{"points",contour},{"color","#28624d"},{"width",25},{"filled",false}};
    // A lead (LochMaster's "Bauteilanschlußdraht") is soldered at its first point and lies on the side being edited.
    if(activeTool=="lead"){n.remove("filled");n["color"]="#375f9b";n["width"]=45;n["back"]=views.backActive();}
    const auto defaults=toolDefaults.value(activeTool);for(auto it=defaults.begin();it!=defaults.end();++it)n[it.key()]=it.value();
    addElement(n);setTool(activeTool);
}
void Canvas::reorderSelected(bool front){
    if(!project||content.selectedItems().isEmpty())return;auto items=content.items(Qt::AscendingOrder);QList<QGraphicsItem*> ordered,selected;
    for(auto *item:items)if(item->flags().testFlag(QGraphicsItem::ItemIsSelectable)){if(item->isSelected())selected.append(item);else ordered.append(item);}
    if(front)ordered.append(selected);else ordered=selected+ordered;if(beforeChange)beforeChange();int z=1;
    for(auto *item:ordered){auto i=item->data(0).toInt();auto key=QString::number(i);bool old=item->data(1)=="legacy";auto n=old?project->edits[key].toObject():project->additions[i].toObject();n["z"]=z++;if(old)project->edits[key]=n;else project->additions[i]=n;}
    rebuild();if(changed)changed();
}
void Canvas::renumber(){if(!project)return;if(beforeChange)beforeChange();project->renumber();rebuild();if(changed)changed();}
void Canvas::groupSelected(bool dissolve){
    if(!project||selectionCount()<(dissolve?1:2))return;auto fragment=Project::decode(selectionData());
    auto bytes=writeLegacyDocument(fragment,!dissolve,dissolve);auto document=LegacyReader(bytes).read(false);auto objects=document["objects"].toArray();
    Project next=*project;auto key=next.addLibrary(bytes,dissolve?"Aufgelöste Gruppe.lib":"Eigene Gruppe.lib");QList<int> removed;
    for(auto *item:content.selectedItems())if(item->data(1)=="new")removed.append(item->data(0).toInt());else{auto id=QString::number(item->data(0).toInt());auto edit=next.edits[id].toObject();edit["deleted"]=true;next.edits[id]=edit;}
    std::sort(removed.begin(),removed.end(),std::greater<>());for(int i:removed)next.additions.removeAt(i);int first=next.additions.size();
    for(int i=0;i<objects.size();i++){auto anchor=componentAnchor(objects[i].toObject());next.additions.append(QJsonObject{{"type","component"},{"library",key},{"index",i},{"x",anchor.x()},{"y",anchor.y()}});}
    Project::decode(next.encode());if(beforeChange)beforeChange();*project=std::move(next);rebuild();content.clearSelection();
    for(auto *item:content.items())if(item->data(1)=="new"&&item->data(0).toInt()>=first)item->setSelected(true);if(changed)changed();
}
void Canvas::replaceSelected(const QByteArray &bytes,const QString &name,QPointF move,const QJsonObject &properties){
    if(!project||selectionCount()!=1)return;const auto objects=LegacyReader(bytes).read(false)["objects"].toArray();if(objects.isEmpty())return;
    Project next=*project;const auto key=next.addLibrary(bytes,name);auto *item=content.selectedItems().first();
    if(item->data(1)=="new")next.additions.removeAt(item->data(0).toInt());else{const auto id=QString::number(item->data(0).toInt());auto edit=next.edits[id].toObject();edit["deleted"]=true;next.edits[id]=edit;}
    const auto anchor=componentAnchor(objects[0].toObject());QJsonObject placement{{"type","component"},{"library",key},{"index",0},{"x",anchor.x()+move.x()},{"y",anchor.y()+move.y()}};
    for(auto it=properties.begin();it!=properties.end();++it)placement[it.key()]=it.value();next.additions.append(placement);
    Project::decode(next.encode());if(beforeChange)beforeChange();*project=std::move(next);rebuild();selectObject("new",project->additions.size()-1);if(changed)changed();
}
// Maps the selected outline's stored points (selectedPath) into the item's coordinates: library and imported paths
// are absolute, OpenLoch contours relative to their position and turned/mirrored around it while painting.
QTransform Canvas::nodeTransform(QGraphicsItem **item) const{
    const auto items=content.selectedItems();*item=items.size()==1?items.first():nullptr;if(!*item)return {};
    const auto n=selectedNode();if(n["type"]=="component"||(*item)->data(1)=="legacy")return {};
    const QPointF at(n["x"].toDouble(),n["y"].toDouble());QTransform t;t.translate(at.x(),at.y());t.rotate(n["angle"].toDouble());t.scale(n["mirrorX"].toBool()?-1:1,n["mirrorY"].toBool()?-1:1);
    if(n["type"]=="wire")t.translate(-at.x(),-at.y());return t;
}
QList<QPointF> Canvas::nodeHandles() const{
    if(!project||activeTool!="select")return {};QGraphicsItem *item;const auto t=nodeTransform(&item);if(!item)return {};
    auto path=selectedPath();if(path.size()<2)return {};if(path.size()>2&&path.first()==path.last())path.removeLast(); // a stored closing point moves with the first
    QList<QPointF> handles;for(auto v:path)handles<<item->mapToScene(t.map(point(v.toArray())));return handles;
}
QJsonArray Canvas::selectedPath() const{
    auto n=selectedNode();if(n["type"]=="component")n=project->componentNode(n);
    if(n.contains("path"))return n["path"].toArray();if(n.contains("points")&&n.contains("x"))return n["points"].toArray();
    if(n["type"]=="wire")return {QJsonArray{n["x"],n["y"]},QJsonArray{n["x2"],n["y2"]}};return {};
}
void Canvas::editPath(const QJsonArray &points){
    auto n=selectedNode();if(n.isEmpty())return;auto effective=n["type"]=="component"?project->componentNode(n):n;
    for(auto p:points){auto a=p.toArray();if(a.size()!=2)throw FormatError(ui("Knoten benötigt X und Y"));for(auto v:a)if(!v.isDouble()||!std::isfinite(v.toDouble())||std::abs(v.toDouble())>1000000)throw FormatError(ui("Ungültiger Knoten"));}
    if(effective.contains("path")){if(points.isEmpty())throw FormatError(ui("Kontur ist leer"));editSelected({{"path",points}});}
    else if(n["type"]=="wire"){if(points.size()!=2)throw FormatError(ui("Eine Leitung benötigt zwei Endpunkte"));auto a=points[0].toArray(),b=points[1].toArray();editSelected({{"x",a[0]},{"y",a[1]},{"x2",b[0]},{"y2",b[1]}});}
    else if(n.contains("points")){if(points.size()<(n["type"]=="polygon"?3:2))throw FormatError(ui("Zu wenige Konturknoten"));editSelected({{"points",points}});}
}
QByteArray Canvas::selectionData() const{
    auto items=content.selectedItems();if(!project||items.isEmpty())return {};
    std::sort(items.begin(),items.end(),[](auto *a,auto *b){return a->zValue()<b->zValue();});
    Project copied;copied.title=ui("Auswahl");copied.mode=project->mode;copied.width=project->width;copied.height=project->height;
    for(auto *item:items){int index=item->data(0).toInt();QJsonObject n;
        if(item->data(1)=="legacy"){
            const auto key=copied.addLibrary(project->original,project->sourceName,project->sourceKind);
            const auto source=project->legacyNode(index);auto anchor=componentAnchor(project->legacy.value("objects").toArray()[index].toObject());
            auto at=anchor-project->offset()+point(project->moves.value(QString::number(index)).toArray());
            n={{"type","component"},{"library",key},{"index",index},{"x",at.x()},{"y",at.y()}};
            for(auto key:{"id","value","description","angle","mirrorX","mirrorY","text","width","pen","brush","diameter","text_kind","font","path","filled","back","group_flags","otherSide","flag2","style","rotation","flag3","bitmap","anchors"})if(source.contains(key))n[key]=source[key];circlePicture(source,n);
            if(n.contains("id"))n["id"]=project->rawId("legacy",index);
            {const auto edit=project->edits.value(QString::number(index)).toObject();if(edit.contains("nested"))n["nested"]=edit["nested"];}
        }else{
            n=project->additions[index].toObject();if(n["type"]=="component"){
                const auto source=project->libraries.value(n["library"].toString());copied.addLibrary(source.bytes,source.name,source.kind);
            }
        }copied.additions.append(n);
    }
    return copied.encode();
}
void Canvas::pasteData(const QByteArray &data){
    if(!project||data.isEmpty())return;const auto copied=Project::decode(data);
    if(copied.additions.isEmpty())return;
    if(!copied.original.isEmpty()||!copied.moves.isEmpty()||!copied.edits.isEmpty()||!copied.boardSource.isEmpty())throw FormatError(ui("Die Zwischenablage enthält keine gültige Auswahl."));
    Project next=*project;for(auto it=copied.libraries.begin();it!=copied.libraries.end();++it)next.addLibrary(it->bytes,it->name,it->kind);
    const int step=data==lastPaste?pasteStep+1:1;const double offset=project->pitch()*100*step;const int startIndex=next.additions.size();
    for(auto value:copied.additions){auto n=value.toObject();
        for(auto key:{"x","y","x2","y2"})if(n.contains(key))n[key]=n[key].toDouble()+offset;
        n.remove("z");n.remove("uid");n.remove("component");numberPart(next,n);next.additions.append(n);   // a copy gets an identifier of its own and stands for no component
    }
    Project::decode(next.encode()); // Reject the entire paste before changing anything.
    if(beforeChange)beforeChange();*project=std::move(next);lastPaste=data;pasteStep=step;setTool("select");rebuild();content.clearSelection();
    for(auto *item:content.items())if(item->data(1)=="new"&&item->data(0).toInt()>=startIndex)item->setSelected(true);
    if(changed)changed();
}
void Canvas::removeSelected() {
    if(!project)return;QList<int> ids;bool any=false;for(auto *item:content.selectedItems())if(item->data(1)=="new")ids.append(item->data(0).toInt());else if(item->data(1)=="legacy")any=true;
    if(ids.isEmpty()&&!any)return;if(beforeChange)beforeChange();
    for(auto *item:content.selectedItems())if(item->data(1)=="legacy"){auto key=QString::number(item->data(0).toInt());auto edit=project->edits[key].toObject();edit["deleted"]=true;project->edits[key]=edit;}
    content.clearSelection();std::sort(ids.begin(),ids.end(),std::greater<>());for(int i:ids)project->additions.removeAt(i);rebuild();if(changed)changed();
}
void Canvas::mouseDoubleClickEvent(QMouseEvent *e){if(activeTool=="polygon"||activeTool=="polyline"||activeTool=="lead"){finishContour();e->accept();return;}if(activeTool!="select"){mousePressEvent(e);return;}auto *item=itemAt(e->pos());if(item&&item->flags().testFlag(QGraphicsItem::ItemIsSelectable)){content.clearSelection();item->setSelected(true);if(propertiesRequested)propertiesRequested();e->accept();return;}QGraphicsView::mouseDoubleClickEvent(e);}
void Canvas::drawBackground(QPainter *p,const QRectF &r) {
    QGraphicsView::drawBackground(p,r);
}
void Canvas::drawForeground(QPainter *p,const QRectF &exposed) {
    if(const auto handles=nodeHandles();!handles.isEmpty()){
        const double s=4/qMax(.00001,std::hypot(transform().m11(),transform().m12()));p->save();p->setPen(QPen(QColor("#1f6fd1"),0));p->setBrush(Qt::white);
        for(int i=0;i<handles.size();i++){const auto at=i==nodeDrag?nodeTarget:handles[i];p->drawRect(QRectF(at-QPointF(s,s),QSizeF(2*s,2*s)));}
        if(nodeDrag>=0&&nodeDrag<handles.size()){p->setPen(QPen(QColor("#1f6fd1"),0,Qt::DashLine));for(int n:{nodeDrag-1,nodeDrag+1})if(n>=0&&n<handles.size())p->drawLine(handles[n],nodeTarget);}
        p->restore();
    }
    if(rulers&&project){
        p->save();p->resetTransform();QRect r=viewport()->rect();p->fillRect(0,0,r.width(),22,QColor("#f5f5f2"));p->fillRect(0,0,42,r.height(),QColor("#f5f5f2"));p->setPen(QColor("#4b514d"));QFont f=p->font();f.setPixelSize(10);p->setFont(f);
        double unit=units==1?2540:units==2?project->pitch()*100:100,desired=60/qMax(.0001,std::abs(transform().m11()))/unit,power=std::pow(10,std::floor(std::log10(desired)));double step=(desired/power<=1?1:desired/power<=2?2:desired/power<=5?5:10)*power*unit;
        // Ruler values count from the user origin.
        const QPointF o=project->origin();auto visible=mapToScene(r).boundingRect();
        for(double x=o.x()+std::ceil((visible.left()-o.x())/step)*step;x<=visible.right();x+=step){int pixel=mapFromScene(QPointF(x,0)).x();p->drawLine(pixel,16,pixel,22);p->drawText(pixel+3,12,QString::number((x-o.x())/unit,'g',5));}
        for(double y=o.y()+std::ceil((visible.top()-o.y())/step)*step;y<=visible.bottom();y+=step){int pixel=mapFromScene(QPointF(0,y)).y();p->drawLine(36,pixel,42,pixel);p->drawText(2,pixel-3,QString::number((y-o.y())/unit,'g',5));}p->drawText(3,13,units==1?"in":units==2?"N":"mm");p->restore();
    }
    const double grid=gridStep();if(!project||!project->legacy.isEmpty()||!project->boardSource.isEmpty()||grid<=0||transform().m11()*grid<7)return;
    auto r=exposed.intersected(QRectF(0,0,project->width,project->height));
    if((r.width()/grid)*(r.height()/grid)>25000)return;
    p->save();p->setClipRect(QRectF(0,0,project->width,project->height));
    QPen pen(project->mode=="board"?QColor(80,65,40,110):QColor(70,90,80,65));pen.setCosmetic(true);p->setPen(pen);
    for(double x=std::ceil(r.left()/grid)*grid;x<=r.right();x+=grid)
        for(double y=std::ceil(r.top()/grid)*grid;y<=r.bottom();y+=grid)p->drawPoint(QPointF(x,y));
    p->restore();
}
void Canvas::renderTo(QPainter &p,const QRectF &target){
    const auto selected=content.selectedItems();content.clearSelection();
    if(preview)preview->hide();if(placementPreview)placementPreview->hide();for(auto *mark:continuityMarks)mark->hide();for(auto *mark:potentialMarks)mark->hide();for(auto *mark:freeMarks)mark->hide();for(auto *mark:shortMarks)mark->hide();for(auto *mark:airwireMarks)mark->hide();
    p.save();if(views.flip){p.translate(0,target.top()*2+target.height());p.scale(1,-1);}p.setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);content.render(&p,target,QRectF(0,0,project->width,project->height),Qt::KeepAspectRatio);p.restore();
    for(auto *item:selected)item->setSelected(true);if(preview)preview->show();if(placementPreview)placementPreview->show();for(auto *mark:continuityMarks)mark->show();for(auto *mark:potentialMarks)mark->show();for(auto *mark:freeMarks)mark->show();for(auto *mark:shortMarks)mark->show();for(auto *mark:airwireMarks)mark->show();
}
bool Canvas::exportImage(const QString &path,QString *error) {
    if(!project)return false;double ratio=project->height/project->width;int w=2400,h=qBound(100,int(w*ratio),6000);if(w*ratio>6000)w=int(6000/ratio);
    QImage image(w,h,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter p(&image);renderTo(p,QRectF(0,0,w,h));p.end();
    QImageWriter writer(path);if(writer.write(image))return true;if(error)*error=writer.errorString();return false;
}
bool Canvas::exportPdf(const QString &path) {
    if(!project)return false;QPdfWriter writer(path);writer.setTitle(project->title);writer.setCreator("OpenLoch");writer.setResolution(300);
    writer.setPageSize(QPageSize(QSizeF(project->width/100,project->height/100),QPageSize::Millimeter,"OpenLoch board",QPageSize::ExactMatch));writer.setPageMargins(QMarginsF());
    QPainter p(&writer);if(!p.isActive())return false;renderTo(p,QRectF(0,0,writer.width(),writer.height()));return p.end();
}
bool Canvas::printTo(QPrinter &printer){
    if(!project)return false;QPainter p(&printer);if(!p.isActive())return false;
    auto page=printer.pageLayout().paintRectPixels(printer.resolution());double units=printer.resolution()/2540.0;
    double width=page.width()/units,height=page.height()/units;int columns=std::ceil(project->width/width),rows=std::ceil(project->height/height);
    if(qint64(columns)*rows>10000){p.end();return false;}
    auto selected=content.selectedItems();content.clearSelection();if(preview)preview->hide();if(placementPreview)placementPreview->hide();for(auto *mark:continuityMarks)mark->hide();for(auto *mark:potentialMarks)mark->hide();for(auto *mark:freeMarks)mark->hide();for(auto *mark:shortMarks)mark->hide();for(auto *mark:airwireMarks)mark->hide();
    bool ok=true;for(int y=0;y<rows&&ok;y++)for(int x=0;x<columns&&ok;x++){
        if(x||y)ok=printer.newPage();if(!ok)break;
        QRectF source(x*width,y*height,qMin(width,project->width-x*width),qMin(height,project->height-y*height));
        p.save();if(views.flip){source.moveTop(project->height-source.bottom());p.translate(0,source.height()*units);p.scale(1,-1);}content.render(&p,QRectF(0,0,source.width()*units,source.height()*units),source,Qt::IgnoreAspectRatio);p.restore();
    }
    for(auto *item:selected)item->setSelected(true);if(preview)preview->show();if(placementPreview)placementPreview->show();for(auto *mark:continuityMarks)mark->show();for(auto *mark:potentialMarks)mark->show();for(auto *mark:freeMarks)mark->show();for(auto *mark:shortMarks)mark->show();for(auto *mark:airwireMarks)mark->show();return p.end()&&ok;
}

// ---------------------------------------------------------------- printing
// The order of LochMaster's print path (RenderBoardView): board fill, copper layer, cuts and lead-end solder marks (marks
// first with the solder side active), holes, objects (inactive side first, x-rayed with Röntgen), the board outline and
// the rulers. Objects are placed exactly like the main window places them.
void paintPrintBoard(QPainter &p,const Project &project,const PrintView &print,bool printer){
    const QRectF board(0,0,project.width,project.height);
    ViewState view;view.flip=print.flip;view.through=print.through;view.xray=print.xray;view.bitmaps=print.bitmaps;view.mono=print.mono;view.sides=project.mode=="board";
    view.texts=print.texts;view.textMirror=print.flip?2:0;view.printer=printer;view.potentialMarkers=print.potentials;
    p.save();p.setClipRect(board.adjusted(0,0,1,1),Qt::IntersectClip);
    // The objects with their placement, in drawing order.
    struct Placed {double z;QTransform transform;QJsonObject node;QByteArray source;int mirror;};
    QList<Placed> placed;
    auto layer=[&](const QJsonObject &n){const auto s=solderSide(n);return project.mode=="board"&&s&&*s!=view.backActive()?-60.0:0.0;};
    const QPointF shift=-project.offset();
    const auto objects=project.legacy.value("objects").toArray();
    for(int i=0;i<objects.size();i++){
        auto node=project.legacyNode(i);if(node["deleted"].toBool())continue;
        const QPointF at=shift+point(project.moves.value(QString::number(i)).toArray());
        const QTransform t=objectTransform(node,componentAnchor(objects[i].toObject()))*QTransform::fromTranslate(at.x(),at.y());
        placed.append({node["z"].toDouble(10+i*.001)+layer(node),t,node,project.original,(node["mirrorX"].toBool()?1:0)|(node["mirrorY"].toBool()?2:0)});
    }
    for(int i=0;i<project.additions.size();i++){
        auto node=project.additions[i].toObject();
        if(node["type"]=="component"){
            const auto geometry=project.componentNode(node);const auto anchor=componentAnchor(project.libraryNode(node["library"].toString(),node["index"].toInt()));
            const QPointF at=QPointF(node["x"].toDouble(),node["y"].toDouble())-anchor;
            placed.append({node["z"].toDouble(50+i*.001)+layer(geometry),objectTransform(node,anchor)*QTransform::fromTranslate(at.x(),at.y()),geometry,project.libraries.value(node["library"].toString()).bytes,(node["mirrorX"].toBool()?1:0)|(node["mirrorY"].toBool()?2:0)});
        }else placed.append({node["z"].toDouble(50+i*.001)+layer(node),QTransform(),node,{},0});
    }
    std::stable_sort(placed.begin(),placed.end(),[](const Placed &a,const Placed &b){return a.z<b.z;});
    auto pass=[&](int which){ViewState v=view;v.pass=which;for(const auto &o:placed){p.save();p.setTransform(o.transform,true);drawNode(p,o.node,o.source,{},project.title,v,o.mirror,false,false);p.restore();}};
    // The board document (template or layout copper) and the hole grid of a generated perfboard.
    QJsonObject boardDoc=project.legacy.value("board").toObject();QByteArray boardBytes=project.original;QPointF boardShift=shift;
    if(!project.boardSource.isEmpty()){const auto source=project.libraries.value(project.boardSource);boardDoc=source.document;boardBytes=source.bytes;boardShift=-point(boardDoc["origin"].toArray());}
    const bool templatePresent=!project.boardSource.isEmpty()||!project.legacy.value("board").toObject().isEmpty()||!project.legacy.isEmpty();
    const bool perfboard=project.mode=="board"&&!templatePresent;
    auto boardPass=[&](int which){if(boardDoc.isEmpty())return;ViewState v=view;v.pass=which;p.save();p.translate(boardShift);drawNode(p,QJsonObject{{"children",boardDoc["objects"]}},boardBytes,{},project.title,v,0,false,false);p.restore();};
    auto gridHoles=[&](bool pads){if(!perfboard)return;
        for(double x=254;x<board.right();x+=254)for(double y=254;y<board.bottom();y+=254){
            if(pads)paintPad(p,QPointF(x,y),90,view.mono,view.backActive());else paintHole(p,QPointF(x,y),.9,view.mono);}};
    // Potentials and free areas repaint conductors: copper below the parts, wires and solder blobs above them.
    QPainterPath markedCopper,markedWires,markedHoles;QList<std::pair<QPainterPath,QColor>> copperColours,wireColours;
    if(project.mode=="board"&&(print.potentials||print.free)){
        auto model=continuityModel(project);const auto holes=model.holes();
        auto add=[&](const QList<Conductor> &conductors,const QColor &colour){
            QPainterPath copper,wires;copper.setFillRule(Qt::WindingFill);wires.setFillRule(Qt::WindingFill);
            for(const auto &c:conductors){
                if(c.copper){copper.addPath(c.shape);for(const auto &[centre,diameter]:holes)if(c.bounds.contains(centre)&&c.shape.contains(centre))markedHoles.addEllipse(centre,diameter*50,diameter*50);continue;}
                if(c.kind==19&&!c.shape.isEmpty()){wires.addPath(c.shape);continue;}
                if(c.path.boundingRect().isNull()){if(!c.path.isEmpty())wires.addEllipse(c.path.first(),qMax(10.0,c.width/2),qMax(10.0,c.width/2));continue;}
                QPainterPath line;line.addPolygon(c.path);QPainterPathStroker stroke;stroke.setWidth(qMax(20.0,c.width));stroke.setCapStyle(Qt::RoundCap);stroke.setJoinStyle(Qt::RoundJoin);wires.addPath(stroke.createStroke(line));
            }
            copperColours.append({copper,colour});wireColours.append({wires,colour});
        };
        if(print.potentials){QMap<QRgb,QList<Conductor>> nets;for(const auto &c:model.potentials().coloured)nets[c.potential.rgb()].append(c);for(auto it=nets.begin();it!=nets.end();++it)add(it.value(),QColor::fromRgb(it.key()));}
        if(print.free)add(model.freeCopper(),QColor(0,128,0));
    }
    auto paintMarks=[&](const QList<std::pair<QPainterPath,QColor>> &list){p.setPen(Qt::NoPen);for(const auto &[path,colour]:list){p.setBrush(colour);p.drawPath(path);}};
    // Lead-end solder marks of the top-level wires, leads, pins and groups.
    auto leadEnds=[&]{
        if(!print.solderMarks)return;
        for(const auto &o:placed){
            const auto t=o.node["type"].toString();const int kind=o.node["kind"].toInt();
            if(!(o.node.contains("children")||((t=="TDraht"||t=="TDrahtFest")&&(kind==1||kind==9||kind==11))||t=="wire"||t=="lead"||t=="pin"))continue;
            for(auto at:connectionPoints(o.node)){
                at=o.transform.map(at);
                if(view.mono){const double h=116/2.0/3;p.setPen(QPen(Qt::black,10));p.drawLine(at-QPointF(h,h),at+QPointF(h,h));p.drawLine(at+QPointF(-h,h),at+QPointF(h,-h));}
                else if(view.backActive()&&view.bitmaps){p.setPen(QPen(QColor("#604824"),8));p.setBrush(QColor("#d0a261"));p.drawEllipse(at,58,58);p.setPen(Qt::NoPen);p.setBrush(QColor("#352d23"));p.drawEllipse(at,24,24);}
                else if(view.backActive()){p.setPen(Qt::NoPen);p.setBrush(QColor(0xC0,0xC0,0xC0));p.drawEllipse(at,58,58);}
                else{p.setPen(Qt::NoPen);p.setBrush(Qt::black);p.drawEllipse(at,58/3.0,58/3.0);}
            }
        }
    };
    if(print.background&&project.mode=="board"){
        p.setPen(view.mono?QPen(QColor(Qt::black),0):QPen(Qt::NoPen));p.setBrush(view.mono?QColor(Qt::white):board::fill);p.drawRect(board);
        if(!view.mono){QPen bevel(board::bevel,0);p.setPen(bevel);p.drawLine(board.topLeft(),board.topRight());p.drawLine(board.topLeft(),board.bottomLeft());}
    }else if(print.background){p.setPen(Qt::NoPen);p.setBrush(Qt::white);p.drawRect(board);}
    // "Freie Bereiche" always shows the copper it marks.
    if(print.copper||print.free){gridHoles(true);boardPass(3);paintMarks(copperColours);}
    auto cuts=[&]{if(print.cuts){boardPass(2);pass(2);}};
    if(view.backActive()){leadEnds();cuts();}else{cuts();leadEnds();}
    if(print.holes){gridHoles(false);boardPass(1);pass(1);}
    if(!markedHoles.isEmpty()&&(print.copper||print.free)){p.setPen(Qt::NoPen);p.setBrush(QColor("#342e26"));p.drawPath(markedHoles);}
    if(print.objects){pass(3);paintMarks(wireColours);}
    p.setPen(QPen(Qt::black,0));p.setBrush(Qt::NoBrush);p.drawRect(board);
    p.restore();
    // Rulers (outside the board clip): ticks every unit from the origin, a label every ten, 2.2 mm Arial.
    if(print.drawsRulers()){
        const double unit=print.unit==1?254:print.unit==2?254:100,divisor=print.unit==1?10:1;const QPointF o=project.origin();
        p.save();p.setPen(QPen(Qt::black,10));QFont f("Arial");f.setPixelSize(220);p.setFont(f);const QFontMetricsF m(f);
        // Labels stay upright on paper when the view is flipped.
        auto text=[&](QPointF at,const QString &s){p.save();p.translate(at);if(print.flip)p.scale(1,-1);p.drawText(QPointF(0,m.ascent()),s);p.restore();};
        const double y0=print.flip?board.bottom()+700:-700,dir=print.flip?-1:1;
        for(int i=int(std::lround((0-o.x())/unit));i<=int(std::lround((board.right()-o.x())/unit));i++){
            const double x=std::round(o.x()+i*unit);
            if(i%10==0){p.drawLine(QPointF(x,y0+500*dir),QPointF(x,y0));text({x+50,print.flip?y0+220:y0},QString::number(i/int(divisor)));}
            else p.drawLine(QPointF(x,y0+500*dir),QPointF(x,y0+300*dir));
        }
        const double x0=-750;
        for(int i=int(std::lround((0-o.y())/unit));i<=int(std::lround((board.bottom()-o.y())/unit));i++){
            const double y=std::round(o.y()+i*unit);
            if(i%10==0){p.drawLine(QPointF(x0+550,y),QPointF(x0,y));text({x0,y+(print.flip?270:50)},QString::number(i/int(divisor)));}
            else p.drawLine(QPointF(x0+550,y),QPointF(x0+350,y));
        }
        text({x0,print.flip?y0+220:y0},print.unit==1?"[inch]":print.unit==2?"[N]":"[mm]");
        p.restore();
    }
}
}
