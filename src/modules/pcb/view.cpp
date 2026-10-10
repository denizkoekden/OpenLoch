#include "view.h"
#include "draw.h"
#include "font.h"
#include "language.h"
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QHash>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QSet>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace openloch::pcb {
namespace {
// Colours of the marks: around the working area, node handles, test results, solder mask openings, findings.
const QColor outsideColour(255,255,255),handleColour(80,140,255),testColour(255,170,255),maskColour(255,105,180),findingColour(255,40,40);
// Photo view: solder mask, copper under it, tinned pads, silkscreen.
const QColor photoSilkColour(230,230,230),photoHole(0,0,0);
double distanceToSegment(QPointF p,QPointF a,QPointF b){
    const QPointF d=b-a;const double l=QPointF::dotProduct(d,d);
    const double t=l>0?std::clamp(QPointF::dotProduct(p-a,d)/l,0.0,1.0):0;const QPointF q=a+t*d-p;return std::hypot(q.x(),q.y());
}
double distanceToPolyline(QPointF p,const QPolygonF &line,bool closed){
    double best=1e300;for(qsizetype i=1;i<line.size();i++)best=std::min(best,distanceToSegment(p,line[i-1],line[i]));
    if(closed&&line.size()>2)best=std::min(best,distanceToSegment(p,line.last(),line.first()));
    if(line.size()==1)best=std::hypot(p.x()-line[0].x(),p.y()-line[0].y());
    return best;
}
double normalized(double a){a=std::fmod(a,360.0);return a<0?a+360:a;}
bool fullCircle(const Element &e){return std::abs(normalized(e.stop-e.start))<1e-9;}
bool autoroutedTrack(const Element &e){return e.type==ElementType::Track&&e.autorouted;}
// Whether an angle (counter-clockwise, screen y down) lies on the arc of a circle element.
bool onArc(const Element &e,QPointF d){
    if(fullCircle(e))return true;
    const double a=normalized(qRadiansToDegrees(std::atan2(-d.y(),d.x()))-e.start);return a<=normalized(e.stop-e.start)+1e-9;
}
}

BoardView::BoardView(QWidget *parent):QWidget(parent){
    setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);setMinimumSize(200,150);setAttribute(Qt::WA_OpaquePaintEvent);setAcceptDrops(true);
    // Test results blink while there are any.
    blinker=new QTimer(this);blinker->setInterval(500);
    connect(blinker,&QTimer::timeout,this,[this]{if(testMarks.isEmpty()||!blinkTest){blinker->stop();blinkOn=true;return;}blinkOn=!blinkOn;update();});
}
void BoardView::setDocument(Document *document){
    doc=document;selected.clear();testMarks.clear();findings.clear();drawing.clear();floating.clear();airwireStart=-1;drag=Drag::None;series=false;views.clear();fitted=false;
    moving.clear();rubberTracks.clear();rubberNodes.clear();schematicLines.clear();schematicCount=-1;padLabels.clear();fitBoard();update();
}
void BoardView::setSchematicAirwires(const QList<std::pair<int,int>> &pads){
    schematicLines=pads;schematicCount=doc&&!doc->boards.isEmpty()?board().elements.size():-1;update();
}
Board &BoardView::board(){return doc->board();}
void BoardView::documentChanged(){
    // A drag under way no longer fits the document; its copies are dropped, not put back.
    drag=Drag::None;moving.clear();rubberTracks.clear();rubberNodes.clear();
    // Undoing texts of a series takes their numbers back, redoing them takes them again: the series' own texts count,
    // as they went down, not other texts that read the same.
    if(series&&doc){
        auto has=[&](int n){const QString text=seriesPrefix+QString::number(n);const auto &els=board().elements;
            return std::any_of(seriesTexts.begin(),seriesTexts.end(),[&](const Element &own){return own.text==text&&els.contains(own);});};
        while(seriesNext>seriesFirst&&!has(seriesNext-1))seriesNext--;
        while(seriesNext<seriesLast&&has(seriesNext))seriesNext++;
    }
    QList<int> keep;for(int i:selected)if(doc&&i<board().elements.size())keep.append(i);
    testMarks.clear();findings.clear();
    const bool changedSelection=keep!=selected;selected=keep;update();if(changedSelection&&selectionChanged)selectionChanged();
}
void BoardView::setTool(Tool tool){
    if(current==tool)return;
    cancelDrag();if(!drawing.isEmpty())finishDrawing();
    current=tool;floating.clear();airwireStart=-1;series=false;if(tool!=Tool::Test)testMarks.clear();
    setCursor(tool==Tool::Select?Qt::ArrowCursor:tool==Tool::Test?Qt::PointingHandCursor:Qt::CrossCursor);
    update();if(toolChanged)toolChanged(tool);
}
void BoardView::setFromBelow(bool on){
    if(below==on)return;
    // Keep the board where it is on the screen.
    const QPointF centre=toBoard(QPointF(width()/2.0,height()/2.0));below=on;centreOn(centre);
}
void BoardView::setPhotoView(bool on){photo=on;update();}
void BoardView::setFindings(const QList<Finding> &list){findings=list;findingsShown.clear();for(int k=0;k<list.size();k++)findingsShown.append(k);update();}
void BoardView::setShownFindings(const QList<int> &indexes){findingsShown.clear();for(int k:indexes)if(k>=0&&k<findings.size())findingsShown.append(k);update();}
void BoardView::showFinding(int index){
    if(index<0||index>=findings.size())return;setSelection(findings[index].elements);centreOn(findings[index].at);
}
void BoardView::zoomToFinding(int index){
    if(index<0||index>=findings.size())return;const auto &f=findings[index];showArea(f.area.isEmpty()?QRectF(f.at-QPointF(1,1),QSizeF(2,2)):f.area);
}
QRectF BoardView::visibleArea() const{return QRectF(toBoard(QPointF(rulerSize,rulerSize)),toBoard(QPointF(width(),height()))).normalized();}
void BoardView::setSelection(QList<int> indexes){
    std::sort(indexes.begin(),indexes.end());indexes.erase(std::unique(indexes.begin(),indexes.end()),indexes.end());
    QList<int> valid;for(int i:indexes)if(doc&&i>=0&&i<board().elements.size())valid.append(i);
    if(valid==selected)return;
    // A move or a node dragged belongs to the selection it began with: it ends first, nothing stays half moved.
    if(drag==Drag::Move||drag==Drag::Node)cancelDrag();
    selected=valid;update();if(selectionChanged)selectionChanged();
}
void BoardView::selectAll(){QList<int> all;if(doc)for(int i=0;i<board().elements.size();i++)if(board().visible[board().elements[i].layer])all.append(i);setSelection(all);}
const QList<std::pair<QPointF,int>> &BoardView::pickPlaceMarks(){
    // A fingerprint of everything the centres depend on is far cheaper than the components themselves; boards without
    // pick and place data need neither.
    const auto &b=doc->board();size_t key=qHash(doc->activeBoard);bool any=false;
    for(const auto &e:b.elements){
        any|=e.pickAndPlace&&e.role==TextRole::Designator;
        key=qHashMulti(key,int(e.type),e.layer,e.pos.x(),e.pos.y(),e.size,e.size2,e.width,e.rotation,e.visible,e.pickAndPlace,e.pickCentre,e.pickOffset.x(),e.pickOffset.y());
        key=qHashMulti(key,e.part,int(e.role),e.componentRotation,e.text);for(const auto &q:e.points)key=qHashMulti(key,q.x(),q.y());
    }
    if(!any){pickMarks.clear();pickKey=0;return pickMarks;}
    if(key!=pickKey){
        pickMarks.clear();pickKey=key;
        for(const auto &c:components(b)){const auto &id=b.elements[c.designator];if(id.pickAndPlace)pickMarks.append({pickPlaceCentre(b,c),id.layer});}
    }
    return pickMarks;
}
void BoardView::setMillingPaths(const QList<QPolygonF> &paths,const QList<QPointF> &plunges,double cutter){
    millingPaths=paths;millingPlunges=plunges;millingCutter=cutter;update();
}
void BoardView::beginPlacement(const QList<Element> &elements,std::function<void(QList<int>)> placed){
    cancelDrag();if(!drawing.isEmpty())finishDrawing();
    floating=elements;floatingPlaced=std::move(placed);update();
}

// --- view transform: seen from below, x runs from the right edge of the working area
QPointF BoardView::mirrored(QPointF mm) const{return below&&doc?QPointF(doc->board().width-mm.x(),mm.y()):mm;}
void BoardView::fitBoard(){
    if(!doc||doc->boards.isEmpty()||width()<10||height()<10)return;
    const auto &b=board();const double margin=24;
    pixelsPerMm=std::max(.05,std::min((width()-2*margin)/b.width,(height()-2*margin)/b.height));
    offset=QPointF((width()-b.width*pixelsPerMm)/2,(height()-b.height*pixelsPerMm)/2);fitted=true;viewMoved();
}
void BoardView::zoomAt(double factor,QPointF pixel){rememberView();zoomBy(factor,pixel);}
void BoardView::zoomBy(double factor,QPointF pixel){
    const QPointF mm=toBoard(pixel);pixelsPerMm=std::clamp(pixelsPerMm*factor,.05,4000.0);offset=pixel-mirrored(mm)*pixelsPerMm;viewMoved();
}
void BoardView::showArea(const QRectF &r){
    if(r.isNull())return;rememberView();const double room=std::max({r.width(),r.height(),2.0})*.5;
    pixelsPerMm=std::clamp(std::min((width()-2.0*rulerSize)/(r.width()+2*room),(height()-2.0*rulerSize)/(r.height()+2*room)),.05,4000.0);centreOn(r.center());
}
void BoardView::centreOn(QPointF mm){offset=QPointF(width()/2.0,height()/2.0)-mirrored(mm)*pixelsPerMm;viewMoved();}
void BoardView::viewMoved(){update();if(viewChanged)viewChanged();}
void BoardView::rememberView(){
    // The centre in millimetres, not the offset: seen from below the same offset shows another part of the board.
    const QPointF centre=toBoard(QPointF(width()/2.0,height()/2.0));
    if(!views.isEmpty()&&views.last().first==pixelsPerMm&&QLineF(views.last().second,centre).length()*pixelsPerMm<.01)return;
    views.append({pixelsPerMm,centre});if(views.size()>50)views.removeFirst();
}
bool BoardView::zoomBack(){if(views.isEmpty())return false;const auto [s,c]=views.takeLast();pixelsPerMm=s;centreOn(c);return true;}
void BoardView::zoomBoard(){rememberView();fitBoard();}
void BoardView::zoomElements(){
    if(!doc)return;QRectF r;for(const auto &e:board().elements)if(board().visible[e.layer]||(e.type==ElementType::Pad&&e.via))r=r.united(bounds(e));
    if(r.isNull())zoomBoard();else showArea(r);
}
bool BoardView::zoomSelection(){
    if(!doc||selected.isEmpty())return false;QRectF r;for(int i:selected)r=r.united(bounds(board().elements[i]));showArea(r);return true;
}
QPointF BoardView::toBoard(QPointF pixel) const{return mirrored((pixel-offset)/pixelsPerMm);}
QPointF BoardView::toPixel(QPointF mm) const{return offset+mirrored(mm)*pixelsPerMm;}
QPointF BoardView::snap(QPointF mm) const{
    const auto keys=QGuiApplication::keyboardModifiers();
    if(!doc||keys&Qt::ControlModifier)return mm;
    QPointF at;if(caught(mm,&at))return at;
    if(!snapToGrid)return mm;
    return keys&Qt::ShiftModifier?onGrid(mm,doc->board().grid/2):onGrid(mm);
}
QPointF BoardView::onGrid(QPointF mm) const{return doc?onGrid(mm,doc->board().grid):mm;}
QPointF BoardView::onGrid(QPointF mm,double g) const{
    // The grid counts from the origin, as the reference counts it.
    if(!doc||g<=0)return mm;const QPointF o=doc->board().origin;return {o.x()+std::round((mm.x()-o.x())/g)*g,o.y()+std::round((mm.y()-o.y())/g)*g};
}
QPointF BoardView::shownPointer() const{return current==Tool::Select&&drag==Drag::None?pointerMm:snap(pointerMm);}
QPointF BoardView::originAt(QPointF mm) const{
    // The origin itself goes onto the grid counted from the top left corner.
    if(!doc||QGuiApplication::keyboardModifiers()&Qt::ControlModifier||!snapToGrid)return mm;
    const double g=doc->board().grid;return {std::round(mm.x()/g)*g,std::round(mm.y()/g)*g};
}
bool BoardView::caught(QPointF mm,QPointF *at) const{
    if(!doc||!autoSnap||QGuiApplication::keyboardModifiers()&Qt::ControlModifier)return false;
    const auto &b=doc->board();double best=8/pixelsPerMm;bool found=false;
    // What is being moved does not catch itself.
    const QList<int> skip=drag==Drag::Move?selected:drag==Drag::Node?QList<int>{nodeElement}:QList<int>{};
    auto consider=[&](QPointF p){const double d=std::hypot(p.x()-mm.x(),p.y()-mm.y());if(d<=best){best=d;found=true;if(at)*at=p;}};
    for(int i=0;i<b.elements.size();i++){
        const auto &e=b.elements[i];if(skip.contains(i)||!(b.visible[e.layer]||(e.type==ElementType::Pad&&e.via)))continue;
        if(e.type==ElementType::Pad||e.type==ElementType::SmdPad||e.type==ElementType::Circle)consider(e.pos);
        else if(e.type==ElementType::Track||e.type==ElementType::Area)for(auto p:e.points)consider(p);
    }
    return found;
}
void BoardView::resizeEvent(QResizeEvent *){if(!fitted)fitBoard();else if(viewChanged)viewChanged();}

// --- drawing
// The reference's default colours: a black board with grey grid lines, copper top blue, bottom green, silkscreen red
// and yellow, inner layers brown and ochre, through-plated pads cyan, the outline white, airwires light grey.
Colours Colours::standard(){
    Colours c;c.board=QColor(0,0,0);c.grid=QColor(70,70,70);c.dots=QColor(170,170,170);c.via=QColor(82,227,253);c.airwire=QColor(215,215,215);
    c.layers={QColor(),QColor(29,106,249),QColor(255,0,0),QColor(0,186,1),QColor(225,215,4),QColor(194,124,21),QColor(238,182,98),QColor(255,255,255)};
    return c;
}
QColor BoardView::layerColour(int layer) const{return layer>=1&&layer<=layerCount?colours.layers[layer]:colours.layers[Outline];}
// An element in one colour; `grow` widens it on every side (the clearance cut into a ground plane). Through-plated
// pads show in their own colour where they are drawn in their layer's.
void BoardView::drawElement(QPainter &p,const Element &e,const QColor &colour,double grow) const{
    const bool own=grow==0&&colour==layerColour(e.layer);
    paintElement(p,e,colour,grow,e.via&&own?colours.via:QColor());
}
QList<std::pair<int,int>> BoardView::airwires() const{
    QList<std::pair<int,int>> out;if(!doc)return out;const auto &els=doc->board().elements;
    for(int i=0;i<els.size();i++)for(int t:els[i].connections)if(t>i&&t<els.size())out.append({i,t});
    // Connections are stored on both pads; one-sided ones are drawn too.
    for(int i=0;i<els.size();i++)for(int t:els[i].connections)if(t<i&&t>=0&&!els[t].connections.contains(i))out.append({t,i});
    return out;
}
// The ground plane ("AutoMasse") of a copper layer, painted as the reference shows it below everything else: the
// working area in a darker shade of the layer, every element of the layer cut out with its clearance, keep-out
// elements cut out in their own shape (areas filled with their border, also when hatched), thermal spokes kept.
void BoardView::paintGround(QPainter &p,int layer,const QColor &colour){
    const auto &b=doc->board();if(!b.groundPlane[layer]||!copperLayers(b).contains(layer))return;
    const double shade=darkGround?.6:1;const QColor ground(qRound(colour.red()*shade),qRound(colour.green()*shade),qRound(colour.blue()*shade));
    p.fillRect(QRectF(0,0,b.width,b.height),ground);
    for(const auto &e:b.elements){
        if(e.cutout&&e.layer==layer){if(e.type==ElementType::Area){p.setPen(Qt::NoPen);p.setBrush(colours.board);p.drawPath(copperShape(e));}else drawElement(p,e,colours.board);continue;}
        const bool hatched=e.type==ElementType::Area&&e.hatched;
        if((e.clearance<=0&&!hatched)||copperOn(e,layer,b).isEmpty())continue;
        // A hatched area keeps the plane out of its whole outline, also at clearance 0.
        if(e.clearance<=0){p.setPen(Qt::NoPen);p.setBrush(colours.board);p.drawPath(copperShape(e));continue;}
        drawElement(p,e,colours.board,e.clearance);
    }
    for(const auto &e:b.elements)if(e.thermal&&e.clearance>0&&!copperOn(e,layer,b).isEmpty()){p.setPen(Qt::NoPen);p.setBrush(ground);p.drawPath(thermalSpokes(e,layer));}
}
BoardView::PhotoColours BoardView::photoColours() const{
    // The reference's colours: the board, its copper under the mask lighter, the far side's copper 20 darker than the board.
    static const QColor boards[]={{22,117,22},{36,82,200},{156,116,22}},coppers[]={{0,190,0},{132,164,228},{231,183,118}};
    const int k=std::clamp(photoBoard,0,2);const QColor board=boards[k];
    const QColor far(std::max(0,board.red()-20),std::max(0,board.green()-20),std::max(0,board.blue()-20));
    const QColor finish=photoFinish==0?QColor(220,170,20):photoFinish==1?QColor(210,210,210):coppers[k];
    return {board,coppers[k],far,finish};
}
void BoardView::paintPhoto(QPainter &p){
    // The finished board from the side looked at: solder mask over the copper of that side, the finish on pads and copper
    // without mask, silkscreen, drill holes. Translucent, the far side's copper shows through and the near copper covers
    // it to three quarters.
    const auto &b=doc->board();const int copper=below?CopperBottom:CopperTop,silk=below?SilkBottom:SilkTop,far=below?CopperTop:CopperBottom;
    const auto c=photoColours();
    p.fillRect(QRectF(0,0,b.width,b.height),c.board);
    if(photoTranslucent)for(const auto &e:b.elements)if(!copperOn(e,far,b).isEmpty())drawElement(p,e,c.farCopper);
    {QColor near=c.copper;if(photoTranslucent)near.setAlphaF(.75);for(const auto &e:b.elements)if(!copperOn(e,copper,b).isEmpty())drawElement(p,e,near);}
    for(const auto &e:b.elements)if(e.solderMask&&!copperOn(e,copper,b).isEmpty())drawElement(p,e,c.finish);
    // Openings of the mask without pads: the bare board, its copper with the finish.
    for(const auto &o:b.elements){
        if(o.type!=ElementType::Area||!o.maskOnly||o.layer!=copper)continue;
        QPainterPath opening;opening.addPolygon(o.points);opening.closeSubpath();p.save();p.setClipPath(opening,Qt::IntersectClip);
        p.fillRect(o.points.boundingRect(),QColor(196,178,124));for(const auto &e:b.elements)if(!copperOn(e,copper,b).isEmpty())drawElement(p,e,c.finish);p.restore();
    }
    if(photoSilk)for(const auto &e:b.elements)if(e.layer==silk)drawElement(p,e,photoSilkColour);
    p.setPen(Qt::NoPen);p.setBrush(photoHole);
    for(const auto &e:b.elements)if(e.type==ElementType::Pad&&e.size2>0)p.drawEllipse(e.pos,e.size2/2,e.size2/2);
    for(const auto &e:b.elements)if(e.layer==Outline){QColor edge(170,170,170);drawElement(p,e,edge);}
}
void BoardView::paint(QPainter &p,const QRectF &target,bool marks){
    p.fillRect(target,outsideColour);if(!doc||doc->boards.isEmpty())return;
    const auto &b=doc->board();
    p.save();p.translate(offset);p.scale(pixelsPerMm,pixelsPerMm);if(below){p.translate(b.width,0);p.scale(-1,1);}
    p.setRenderHint(QPainter::Antialiasing,smoothing);
    if(photo){paintPhoto(p);p.restore();if(marks)paintPadLabels(p);return;}
    p.fillRect(QRectF(0,0,b.width,b.height),colours.board);
    // The scanned template of the active layer's side, at its resolution: as in the reference the top one under K1 and
    // B1, the bottom one under K2 and B2, none under the inner layers and the outline. One-bit pictures take the side's
    // colour for their dark pixels and the board's for the light ones.
    if(const int side=b.activeLayer==CopperTop||b.activeLayer==SilkTop?0:b.activeLayer==CopperBottom||b.activeLayer==SilkBottom?1:-1;side>=0&&!templateHidden){
        const auto &t=b.templates[side];QImage image=t.shown&&!t.file.isEmpty()?templatePicture(t.file):QImage();
        if(image.depth()==1){auto table=image.colorTable();if(table.size()<2)table={qRgb(0,0,0),qRgb(255,255,255)};for(auto &c:table)c=qGray(c)<128?t.colour.rgb():colours.board.rgb();image.setColorTable(table);}
        if(!image.isNull()){const double perPixel=25.4/std::max(1.0,t.dpi);p.drawImage(QRectF(t.offset,QSizeF(image.width()*perPixel,image.height()*perPixel)),image);}
    }
    // One ground plane, below everything: the reference shows only that of the topmost copper layer that has one.
    // With the template alone, the elements stay hidden.
    const auto order=templateOnly?QList<int>():drawingOrder(b.activeLayer,below);
    if(!allGrounds)for(qsizetype k=order.size()-1;k>=0;k--)if(b.visible[order[k]]&&b.groundPlane[order[k]]&&copperLayers(b).contains(order[k])){paintGround(p,order[k],layerColour(order[k]));break;}
    // The grid once its lines are at least 8 pixels apart, counted from the origin as in the reference, every so many
    // lines from it stronger: lines, or dots where they cross.
    if(marks&&gridShown&&b.grid*pixelsPerMm>=8){
        const QRectF view=QRectF(toBoard(target.topLeft()),toBoard(target.bottomRight())).normalized().intersected(QRectF(0,0,b.width,b.height));
        const QColor colour=gridDots?colours.dots:colours.grid;QPen thin(colour,1),strong(colour,2);thin.setCosmetic(true);strong.setCosmetic(true);
        p.save();p.setRenderHint(QPainter::Antialiasing,false);const QPointF o=b.origin;
        const qint64 left=qint64(std::ceil((view.left()-o.x())/b.grid-1e-9)),right=qint64(std::floor((view.right()-o.x())/b.grid+1e-9));
        const qint64 top=qint64(std::ceil((view.top()-o.y())/b.grid-1e-9)),bottom=qint64(std::floor((view.bottom()-o.y())/b.grid+1e-9));
        auto marked=[this](qint64 n){return gridMarking>0&&n%gridMarking==0;};
        if(gridDots&&(right-left+1)*(bottom-top+1)<4000000){
            QPolygonF small,large;
            for(qint64 i=left;i<=right;i++)for(qint64 j=top;j<=bottom;j++)(marked(i)&&marked(j)?large:small)<<QPointF(o.x()+i*b.grid,o.y()+j*b.grid);
            p.setPen(thin);p.drawPoints(small);p.setPen(strong);p.drawPoints(large);
        }
        else if(!gridDots&&right-left<20000&&bottom-top<20000){
            for(qint64 i=left;i<=right;i++){const double x=o.x()+i*b.grid;p.setPen(marked(i)?strong:thin);p.drawLine(QPointF(x,view.top()),QPointF(x,view.bottom()));}
            for(qint64 j=top;j<=bottom;j++){const double y=o.y()+j*b.grid;p.setPen(marked(j)?strong:thin);p.drawLine(QPointF(view.left(),y),QPointF(view.right(),y));}
        }
        p.restore();
    }
    // In the autoroute tool, autorouted tracks wider than two pixels show a light grey stripe along their middle, a
    // quarter of their width (at least a pixel), as in the reference.
    const bool stripes=marks&&current==Tool::Autoroute;
    auto stripe=[&](const Element &e){
        if(!autoroutedTrack(e)||e.width*pixelsPerMm<=2)return;
        p.setPen(QPen(QColor(215,215,215),std::max(e.width/4,1/pixelsPerMm),Qt::SolidLine,Qt::FlatCap,Qt::MiterJoin));p.setBrush(Qt::NoBrush);p.drawPolyline(e.points);
    };
    // The layers; keep-out elements show only as their cut in the ground plane.
    const bool maskMode=marks&&current==Tool::SolderMask;
    // Transparent: each layer mixes bit by bit with what lies below, OR on a dark board and AND on a light one.
    if(transparent)p.setCompositionMode(colours.board.lightness()<128?QPainter::RasterOp_SourceOrDestination:QPainter::RasterOp_SourceAndDestination);
    for(int layer:order){
        if(!b.visible[layer])continue;const QColor c=layerColour(layer);
        // With all ground planes shown, each lies right under its own layer.
        if(allGrounds&&b.groundPlane[layer]&&copperLayers(b).contains(layer))paintGround(p,layer,c);
        for(const auto &e:b.elements)if(e.layer==layer&&!e.cutout)drawElement(p,e,maskMode&&e.solderMask?maskColour:c);
        if(stripes)for(const auto &e:b.elements)if(e.layer==layer&&!e.cutout)stripe(e);
    }
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    // Drill holes are always open, on top of everything: in the board's colour or white.
    const bool elementsShown=!templateOnly;
    p.setPen(Qt::NoPen);p.setBrush(holes==1?QColor(Qt::white):holes==2?QColor(Qt::black):colours.board);
    for(const auto &e:b.elements)if(elementsShown&&e.type==ElementType::Pad&&e.size2>0&&(b.visible[e.layer]||e.via))p.drawEllipse(e.pos,e.size2/2,e.size2/2);
    // Holes without copper (drill diameter as large as the pad) show a cross like the reference.
    for(const auto &e:b.elements)if(elementsShown&&e.type==ElementType::Pad&&e.size2>=e.size-1e-9&&b.visible[e.layer]){
        QPen q(layerColour(e.layer),1);q.setCosmetic(true);p.setPen(q);p.setBrush(Qt::NoBrush);const double r=e.size2/2;
        p.drawEllipse(e.pos,r,r);p.drawLine(e.pos+QPointF(-r,-r)*.7,e.pos+QPointF(r,r)*.7);p.drawLine(e.pos+QPointF(-r,r)*.7,e.pos+QPointF(r,-r)*.7);}
    if(elementsShown){QPen wire(colours.airwire,1);wire.setCosmetic(true);p.setPen(wire);for(auto [a,c]:airwires())p.drawLine(b.elements[a].pos,b.elements[c].pos);}
    if(marks&&!schematicLines.isEmpty()&&schematicCount==b.elements.size()){
        QPen wire(colours.airwire,1,Qt::DashLine);wire.setCosmetic(true);p.setPen(wire);
        for(auto [a,c]:schematicLines)if(a>=0&&c>=0&&a<b.elements.size()&&c<b.elements.size())p.drawLine(b.elements[a].pos,b.elements[c].pos);
    }
    if(marks&&hasMillingPaths()){
        // Milling paths over everything, as thin lines or as wide as the cutter.
        const QColor mill(255,0,255);QPen pen(millingWide&&millingCutter>0?QColor(mill.red(),mill.green(),mill.blue(),160):mill,millingWide&&millingCutter>0?millingCutter:1,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
        if(!(millingWide&&millingCutter>0))pen.setCosmetic(true);p.setPen(pen);p.setBrush(Qt::NoBrush);for(const auto &path:millingPaths)p.drawPolyline(path);
        QPen cross(mill,1);cross.setCosmetic(true);p.setPen(cross);const double r=4/pixelsPerMm;
        for(auto at:millingPlunges){p.drawLine(at-QPointF(r,r),at+QPointF(r,r));p.drawLine(at-QPointF(r,-r),at+QPointF(r,-r));}
    }
    if(marks){
        // Components with pick and place data show a small cross at their centre, in the colour of their designator.
        for(const auto &[at,layer]:pickPlaceMarks()){
            if(!b.visible[layer])continue;const double r=.5;QPen cross(QColor(255,255,255),1);cross.setCosmetic(true);p.setPen(cross);
            p.drawLine(at-QPointF(r,0),at+QPointF(r,0));p.drawLine(at-QPointF(0,r),at+QPointF(0,r));
        }
        if(testShown())for(int i:testMarks)if(i<b.elements.size())drawElement(p,b.elements[i],testColour);
        const QColor highlight(255,255,255,150);
        auto showSelected=[&](const Element &e){drawElement(p,e,highlight);
            QPen frame(QColor(255,255,255),1,Qt::DashLine);frame.setCosmetic(true);p.setPen(frame);p.setBrush(Qt::NoBrush);p.drawRect(bounds(e));};
        for(int i:selected)if(i<b.elements.size())showSelected(b.elements[i]);
        for(int n:nodeHandles()){const auto &e=b.elements[selected.first()];const double r=4/pixelsPerMm;
            p.setPen(Qt::NoPen);p.setBrush(handleColour);p.drawEllipse(e.points[n],r,r);}
        // Virtual nodes halfway along the segments: hollow, smaller.
        if(drag!=Drag::Node)for(const auto &[index,at]:virtualNodes()){const double r=3/pixelsPerMm;QPen ring(handleColour,1);ring.setCosmetic(true);
            p.setPen(ring);p.setBrush(Qt::NoBrush);p.drawEllipse(at,r,r);}
        // Whatever the current tool is drawing or placing.
        const QColor live=layerColour(b.activeLayer).lighter(130);
        if(!drawing.isEmpty()&&hasPointer){
            Element e=newElement(current==Tool::Track?ElementType::Track:ElementType::Area);e.width=trackWidth;e.points=drawing;
            const auto bend=bentPath(drawing.last(),snap(pointerMm));for(qsizetype i=1;i<bend.size();i++)e.points<<bend[i];
            if(current!=Tool::Track&&e.points.size()>2){Element outline=e;outline.type=ElementType::Track;outline.points<<outline.points.first();QColor c=live;c.setAlpha(110);drawElement(p,e,c);drawElement(p,outline,live);}
            else{e.type=ElementType::Track;drawElement(p,e,live);}
        }
        // Before the first click the track tool shows its round end at the pointer.
        if(drawing.isEmpty()&&hasPointer&&drag==Drag::None&&current==Tool::Track){p.setPen(Qt::NoPen);p.setBrush(live);p.drawEllipse(snap(pointerMm),trackWidth/2,trackWidth/2);}
        if(drag==Drag::Circle){Element e=newElement(ElementType::Circle);e.pos=snap(pressMm);const QPointF d=snap(pointerMm)-e.pos;e.size=std::hypot(d.x(),d.y());e.width=trackWidth;drawElement(p,e,live);}
        if(drag==Drag::Rectangle){const QRectF r=QRectF(snap(pressMm),snap(pointerMm)).normalized();const bool filled=filledRectangle||current==Tool::Keepout;Element e=newElement(filled?ElementType::Area:ElementType::Track);e.width=trackWidth;
            e.points={r.topLeft(),r.topRight(),r.bottomRight(),r.bottomLeft()};if(!filledRectangle)e.points<<r.topLeft();drawElement(p,e,live);}
        if(hasPointer&&drag==Drag::None&&(current==Tool::Pad||current==Tool::Smd)){
            Element e=newElement(current==Tool::Pad?ElementType::Pad:ElementType::SmdPad);e.pos=snap(pointerMm);
            if(current==Tool::Pad){e.size=padDiameter;e.size2=padDrill;e.shape=padShape;e.via=padVia;}else{e.size=smdWidth;e.size2=smdHeight;}
            updateOutline(e);drawElement(p,e,live);if(current==Tool::Pad){p.setPen(Qt::NoPen);p.setBrush(colours.board);p.drawEllipse(e.pos,padDrill/2,padDrill/2);}
        }
        if(current==Tool::Autoroute&&hasPointer){
            // The airwire under the pointer lights up with its pads; without one, an autorouted track under it shows
            // selected (its stripe on top), as a click turns it back.
            if(const auto routable=routableAt(pointerMm)){const auto pair=*routable;drawElement(p,b.elements[pair.first],testColour);drawElement(p,b.elements[pair.second],testColour);
                QPen wire(testColour,2);wire.setCosmetic(true);p.setPen(wire);p.drawLine(b.elements[pair.first].pos,b.elements[pair.second].pos);}
            else if(const int t=hit(pointerMm,autoroutedTrack);t>=0){showSelected(b.elements[t]);stripe(b.elements[t]);}
        }
        if(current==Tool::Airwire&&hasPointer){
            const int pad=padAt(pointerMm);if(pad>=0)drawElement(p,b.elements[pad],testColour);
            QPen wire(testColour,1);wire.setCosmetic(true);p.setPen(wire);
            if(airwireStart>=0&&airwireStart<b.elements.size())p.drawLine(b.elements[airwireStart].pos,pointerMm);
            else if(pad<0){const int w=airwireAt(pointerMm);if(w>=0){const auto pair=airwires()[w];QPen x(findingColour,2);x.setCosmetic(true);p.setPen(x);p.drawLine(b.elements[pair.first].pos,b.elements[pair.second].pos);}}
        }
        if(hasPointer&&!floating.isEmpty()){const QPointF at=snap(pointerMm);for(auto e:floating){pcb::move(e,at);drawElement(p,e,layerColour(e.layer).lighter(130));}}
        // The next text of a series waits at the pointer.
        if(hasPointer&&series&&current==Tool::Text){Element e=seriesText;e.pos=snap(pointerMm);e.text=seriesPrefix+QString::number(seriesNext);updateStrokes(e);drawElement(p,e,layerColour(e.layer).lighter(130));}
        if(drag==Drag::Measure){QPen m(QColor(255,255,255),1,Qt::DashLine);m.setCosmetic(true);p.setPen(m);p.drawLine(snap(pressMm),snap(pointerMm));}
        // The cross hair through the whole view where the pointer is (snapped as the tool snaps), as the reference shows it
        // in the tools that draw or place: white lines, grey ones at 45° if wanted. Where it caught a point of an element
        // the lines turn red, apart from a gap around the point; that also shows while dragging in the standard tool.
        crossOn=false;
        if(hasPointer){
            crossAt=shownPointer();const bool tool=crossTool(),red=(tool||drag==Drag::Move||drag==Drag::Node)&&caught(pointerMm,&crossAt);
            crossOn=red||(tool&&crosshair.lines);
            if(crossOn){
                // Sharp lines of one pixel.
                const QRectF view=QRectF(toBoard(target.topLeft()),toBoard(target.bottomRight())).normalized();
                auto pen=[](QColor c,int width){QPen pen(c,width);pen.setCosmetic(true);return pen;};
                auto lines=[&](double gap){
                    p.drawLine(QPointF(view.left(),crossAt.y()),QPointF(crossAt.x()-gap,crossAt.y()));p.drawLine(QPointF(crossAt.x()+gap,crossAt.y()),QPointF(view.right(),crossAt.y()));
                    p.drawLine(QPointF(crossAt.x(),view.top()),QPointF(crossAt.x(),crossAt.y()-gap));p.drawLine(QPointF(crossAt.x(),crossAt.y()+gap),QPointF(crossAt.x(),view.bottom()));};
                p.save();p.setRenderHint(QPainter::Antialiasing,false);
                if(crosshair.diagonals){const double r=view.width()+view.height();p.setPen(pen(QColor(150,150,150),1));
                    p.drawLine(crossAt-QPointF(r,r),crossAt+QPointF(r,r));p.drawLine(crossAt-QPointF(r,-r),crossAt+QPointF(r,-r));}
                p.setPen(pen(QColor(255,255,255),1));lines(0);
                if(red){const double gap=20/pixelsPerMm;p.setPen(pen(QColor(255,255,255),3));lines(gap);p.setPen(pen(QColor(255,0,0),1));lines(gap);}
                p.restore();
            }
        }
    }
    if(marks){
        // The origin of the coordinates: a small cross.
        QPen cross(QColor(255,255,255),1);cross.setCosmetic(true);p.setPen(cross);const double r=6/pixelsPerMm;const QPointF o=drag==Drag::Origin?originAt(pointerMm):b.origin;
        p.drawLine(o-QPointF(r,0),o+QPointF(r,0));p.drawLine(o-QPointF(0,r),o+QPointF(0,r));p.setBrush(Qt::NoBrush);p.drawEllipse(o,r/2,r/2);
    }
    p.restore();
    if(marks)paintRulers(p);
    // The coordinates at the cross hair, as the status line counts them: x with its sign, y as the distance.
    if(marks&&hasPointer&&crossOn&&crosshair.coordinates){
        const QPointF o=board().origin;const QString unit=milUnits?QStringLiteral("mil"):QStringLiteral("mm");
        const QString text=ui("X: %1 %3\nY: %2 %3").arg(uiLocale().toString(inUnits(crossAt.x()-o.x()),'f',3),uiLocale().toString(inUnits(std::abs(o.y()-crossAt.y())),'f',3),unit);
        QFont f=font();f.setPixelSize(crosshair.bigText?16:11);p.setFont(f);
        const QRectF box=p.boundingRect(QRectF(toPixel(crossAt)+QPointF(20,20),QSizeF(400,100)),Qt::AlignLeft|Qt::AlignTop,text).adjusted(-4,-3,4,3);
        if(!crosshair.transparent)p.fillRect(box,crosshair.whiteBox?QColor(Qt::white):QColor(Qt::black));
        p.setPen(crosshair.whiteBox&&!crosshair.transparent?QColor(Qt::black):QColor(Qt::white));p.drawText(box.adjusted(4,3,-4,-3),Qt::AlignLeft|Qt::AlignTop,text);
    }
    if(marks)paintPadLabels(p);
    // Findings of the design rule check shown: white hatched frames, at least 16 pixels large.
    if(marks)for(int k:findingsShown){
        if(k>=findings.size())continue;const auto &f=findings[k];const QRectF r=f.area.isEmpty()?QRectF(f.at,f.at):f.area;
        QRectF px=QRectF(toPixel(r.topLeft()),toPixel(r.bottomRight())).normalized().adjusted(-4,-4,4,4);
        if(px.width()<16)px.adjust(-(16-px.width())/2,0,(16-px.width())/2,0);if(px.height()<16)px.adjust(0,-(16-px.height())/2,0,(16-px.height())/2);
        p.setPen(QPen(QColor(255,255,255),1));p.setBrush(QBrush(QColor(255,255,255,190),Qt::BDiagPattern));p.drawRect(px);
    }
    if(marks&&drag==Drag::Band){QPen band(QColor(255,255,255),1,Qt::DashLine);p.setPen(band);p.setBrush(QColor(255,255,255,30));p.drawRect(QRectF(toPixel(pressMm),toPixel(pointerMm)).normalized());}
    if(marks&&drag==Drag::Zoom){QPen band(QColor(60,108,244),1,Qt::DashLine);p.setPen(band);p.setBrush(Qt::NoBrush);p.drawRect(QRectF(toPixel(pressMm),toPixel(pointerMm)).normalized());}
    if(marks&&drag==Drag::Measure){
        const QPointF d=snap(pointerMm)-snap(pressMm);const double l=std::hypot(d.x(),d.y()),angle=qRadiansToDegrees(std::atan2(-d.y(),d.x()));
        p.setPen(QColor(255,255,255));p.drawText(toPixel(snap(pointerMm))+QPointF(12,-8),ui("%1 mm  dx %2  dy %3  %4°").arg(uiLocale().toString(l,'f',3),uiLocale().toString(d.x(),'f',3),uiLocale().toString(-d.y(),'f',3),uiLocale().toString(angle,'f',1)));
    }
}
void BoardView::paintRulers(QPainter &p){
    const auto &b=board();const double unit=milUnits?.0254:1,perUnit=pixelsPerMm*unit;
    // Labels at 1, 2 or 5 times a power of ten units, at least 50 pixels apart; five ticks between them.
    double step=1e6;for(double base=1e-3;base<1e6;base*=10){bool found=false;for(double n:{1.0,2.0,5.0})if(n*base*perUnit>=50){step=n*base;found=true;break;}if(found)break;}
    const double minor=step/5;const QColor back(240,240,240),ink(70,70,70);
    p.save();p.setRenderHint(QPainter::Antialiasing,false);p.fillRect(QRectF(0,0,width(),rulerSize),back);p.fillRect(QRectF(0,0,rulerSize,height()),back);
    QFont f=font();f.setPixelSize(9);p.setFont(f);p.setPen(ink);
    // The upper ruler counts with its sign, the left one as the distance from the origin, as in the reference.
    auto label=[&](double v,bool sign){return uiLocale().toString(sign&&v!=0?v:std::abs(v),'g',6);};
    {const double a=(toBoard(QPointF(rulerSize,0)).x()-b.origin.x())/unit,c=(toBoard(QPointF(width(),0)).x()-b.origin.x())/unit;
        const qint64 from=qint64(std::floor(std::min(a,c)/minor)),to=qint64(std::ceil(std::max(a,c)/minor));
        for(qint64 i=from;i<=to&&to-from<5000;i++){const double x=toPixel(QPointF(b.origin.x()+i*minor*unit,0)).x();if(x<rulerSize)continue;const bool major=i%5==0;
            p.drawLine(QPointF(x,rulerSize-1),QPointF(x,rulerSize-(major?9:4)));if(major)p.drawText(QPointF(x+2,9),label(i*minor,true));}}
    {const double a=(b.origin.y()-toBoard(QPointF(0,rulerSize)).y())/unit,c=(b.origin.y()-toBoard(QPointF(0,height())).y())/unit;
        const qint64 from=qint64(std::floor(std::min(a,c)/minor)),to=qint64(std::ceil(std::max(a,c)/minor));
        for(qint64 i=from;i<=to&&to-from<5000;i++){const double y=toPixel(QPointF(0,b.origin.y()-i*minor*unit)).y();if(y<rulerSize)continue;const bool major=i%5==0;
            p.drawLine(QPointF(rulerSize-1,y),QPointF(rulerSize-(major?9:4),y));
            if(major){p.save();p.translate(9,y-2);p.rotate(-90);p.drawText(QPointF(0,0),label(i*minor,false));p.restore();}}}
    // Where the pointer is, and the unit in the corner.
    if(hasPointer){QPen mark(QColor(255,0,0),1);p.setPen(mark);const QPointF at=toPixel(shownPointer());
        if(at.x()>=rulerSize)p.drawLine(QPointF(at.x(),0),QPointF(at.x(),rulerSize));if(at.y()>=rulerSize)p.drawLine(QPointF(0,at.y()),QPointF(rulerSize,at.y()));}
    p.fillRect(QRectF(0,0,rulerSize,rulerSize),back);p.setPen(ink);p.drawText(QRectF(0,0,rulerSize,rulerSize),Qt::AlignCenter,milUnits?QStringLiteral("mil"):QStringLiteral("mm"));
    p.setPen(QColor(160,160,160));p.drawLine(QPointF(0,rulerSize),QPointF(width(),rulerSize));p.drawLine(QPointF(rulerSize,0),QPointF(rulerSize,height()));
    p.restore();
}
QImage BoardView::templatePicture(const QString &file) const{
    if(pictures.contains(file))return pictures[file];
    QImage image(file);
    // Files from other systems: the name alone, next to the document.
    if(image.isNull()&&!documentFolder.isEmpty()){const QString name=file.section(QRegularExpression(QStringLiteral("[\\\\/]")),-1);image=QImage(QDir(documentFolder).filePath(name));}
    if(!image.isNull()&&image.width()*qint64(image.height())>60000000)image={};
    pictures.insert(file,image);return image;
}
bool BoardView::nearOrigin(QPointF pixel) const{const QPointF d=toPixel(doc->board().origin)-pixel;return std::hypot(d.x(),d.y())<=7;}
// Labels on pads (in the photo view as well): white on black beside each pad.
void BoardView::paintPadLabels(QPainter &p){
    if(padLabels.isEmpty()||!doc)return;
    QFont f=font();f.setPixelSize(11);f.setBold(true);p.setFont(f);
    for(const auto &[index,text]:padLabels){
        if(index<0||index>=board().elements.size())continue;
        const QRectF box=p.boundingRect(QRectF(toPixel(board().elements[index].pos)+QPointF(6,-18),QSizeF(200,40)),Qt::AlignLeft|Qt::AlignTop,text).adjusted(-3,-1,3,1);
        p.fillRect(box,QColor(0,0,0,200));p.setPen(QColor(Qt::white));p.drawText(box.adjusted(3,1,-3,-1),Qt::AlignLeft|Qt::AlignTop,text);
    }
}
void BoardView::paintEvent(QPaintEvent *){QPainter p(this);paint(p,rect(),true);}
QImage BoardView::render(QSize size){
    const auto keepScale=pixelsPerMm;const auto keepOffset=offset;const auto keepSize=this->size();
    resize(size);fitBoard();QImage image(size,QImage::Format_ARGB32_Premultiplied);
    {QPainter p(&image);paint(p,QRectF(QPointF(),QSizeF(size)),false);}
    resize(keepSize);pixelsPerMm=keepScale;offset=keepOffset;return image;
}

QImage BoardView::renderBoard(double perMm,bool smooth){
    if(!doc)return {};const auto &b=board();
    const QSize size(std::max(1,int(std::lround(b.width*perMm))),std::max(1,int(std::lround(b.height*perMm))));
    QImage image(size,QImage::Format_RGB32);{QPainter p(&image);paintBoard(p,perMm,smooth);}return image;
}
void BoardView::paintBoard(QPainter &p,double perMm,bool smooth){
    if(!doc)return;const auto keepScale=pixelsPerMm;const auto keepOffset=offset;const bool keepSmooth=smoothing;const auto &b=board();
    pixelsPerMm=perMm;offset=QPointF();smoothing=smooth;paint(p,QRectF(0,0,b.width*perMm,b.height*perMm),false);
    pixelsPerMm=keepScale;offset=keepOffset;smoothing=keepSmooth;
}

// --- hit test and editing helpers
bool BoardView::hits(const Element &e,QPointF mm) const{
    if(!doc)return false;const auto &b=doc->board();const double tolerance=3/pixelsPerMm;
    if(!b.visible[e.layer]&&!(e.type==ElementType::Pad&&e.via))return false;
    if(!bounds(e).adjusted(-tolerance,-tolerance,tolerance,tolerance).contains(mm))return false;
    switch(e.type){
    case ElementType::Track:return distanceToPolyline(mm,e.points,false)<=e.width/2+tolerance;
    case ElementType::Area:return e.points.containsPoint(mm,Qt::OddEvenFill)||distanceToPolyline(mm,e.points,true)<=e.width/2+tolerance;
    case ElementType::Circle:{const QPointF d=mm-e.pos;const double r=std::hypot(d.x(),d.y());
        return onArc(e,d)&&(e.filled?r<=e.size+e.width/2+tolerance:std::abs(r-e.size)<=e.width/2+tolerance);}
    case ElementType::Text:return e.visible||e.role==TextRole::Plain;
    default:{
        // Pads: their copper, or near it when they are only a few pixels large.
        const auto shape=copperShape(e);if(shape.contains(mm))return true;const QRectF r=shape.boundingRect();
        return (r.width()*pixelsPerMm<8||r.height()*pixelsPerMm<8)&&r.adjusted(-tolerance,-tolerance,tolerance,tolerance).contains(mm);}
    }
}
int BoardView::hit(QPointF mm,const std::function<bool(const Element&)> &only) const{
    if(!doc)return -1;const auto &b=doc->board();
    // On one layer the smallest element at the point wins (a pad over the track ending on it), then the one drawn last;
    // the active layer comes first, then the others from the top of the drawing order.
    auto pick=[&](int layer){
        int best=-1;double size=0;
        for(int i=int(b.elements.size())-1;i>=0;i--){
            const auto &e=b.elements[i];if(e.layer!=layer||(only&&!only(e))||!hits(e,mm))continue;
            const QRectF r=bounds(e);const double s=r.width()*r.height();if(best<0||s<size-1e-12){best=i;size=s;}
        }
        return best;
    };
    if(const int i=pick(b.activeLayer);i>=0)return i;
    const auto order=drawingOrder(b.activeLayer,below);
    for(int k=int(order.size())-1;k>=0;k--)if(order[k]!=b.activeLayer)if(const int i=pick(order[k]);i>=0)return i;
    return -1;
}
int BoardView::padAt(QPointF mm) const{
    if(!doc)return -1;const auto &b=doc->board();const double tolerance=3/pixelsPerMm;
    for(int i=int(b.elements.size())-1;i>=0;i--){
        const auto &e=b.elements[i];if((e.type!=ElementType::Pad&&e.type!=ElementType::SmdPad)||(!b.visible[e.layer]&&!e.via))continue;
        if(copperShape(e).boundingRect().adjusted(-tolerance,-tolerance,tolerance,tolerance).contains(mm))return i;
    }
    return -1;
}
int BoardView::airwireAt(QPointF mm) const{
    const auto wires=airwires();const auto &b=doc->board();const double tolerance=4/pixelsPerMm;
    for(int k=0;k<wires.size();k++)if(distanceToSegment(mm,b.elements[wires[k].first].pos,b.elements[wires[k].second].pos)<=tolerance)return k;
    return -1;
}
int BoardView::schematicAirwireAt(QPointF mm) const{
    if(!doc||schematicLines.isEmpty()||schematicCount!=doc->board().elements.size())return -1;
    const auto &b=doc->board();const double tolerance=4/pixelsPerMm;const int n=int(b.elements.size());
    for(int k=0;k<schematicLines.size();k++){const auto [a,c]=schematicLines[k];
        if(a>=0&&c>=0&&a<n&&c<n&&distanceToSegment(mm,b.elements[a].pos,b.elements[c].pos)<=tolerance)return k;}
    return -1;
}
std::optional<std::pair<int,int>> BoardView::routableAt(QPointF mm,bool *fromSchematic) const{
    if(fromSchematic)*fromSchematic=false;
    if(const int w=airwireAt(mm);w>=0)return airwires()[w];
    if(const int s=schematicAirwireAt(mm);s>=0){if(fromSchematic)*fromSchematic=true;return schematicLines[s];}
    return std::nullopt;
}
QList<int> BoardView::nodeHandles() const{
    QList<int> out;if(!doc||selected.size()!=1||current!=Tool::Select)return out;const auto &e=doc->board().elements.value(selected.first());
    if(e.type==ElementType::Track||e.type==ElementType::Area)for(int i=0;i<e.points.size();i++)out.append(i);
    return out;
}
QList<std::pair<int,QPointF>> BoardView::virtualNodes() const{
    QList<std::pair<int,QPointF>> out;if(nodeHandles().isEmpty())return out;const auto &e=doc->board().elements[selected.first()];
    // Segment k runs from node k to node k+1; an area's last segment closes it, its new node goes to the end.
    const int n=int(e.points.size()),segments=e.type==ElementType::Area&&n>2?n:n-1;
    for(int k=0;k<segments;k++){const QPointF a=e.points[k],c=e.points[(k+1)%n],d=(c-a)*pixelsPerMm;if(std::hypot(d.x(),d.y())>=16)out.append({k+1,(a+c)/2});}
    return out;
}
void BoardView::startTextSeries(const Element &text,const QString &prefix,int next){
    seriesText=text;seriesPrefix=prefix;seriesNext=seriesFirst=seriesLast=next;seriesTexts.clear();series=true;update();
}
bool BoardView::crossTool() const{
    switch(current){
    case Tool::Track:case Tool::Pad:case Tool::Smd:case Tool::Circle:case Tool::Rectangle:case Tool::Area:case Tool::Keepout:case Tool::Text:case Tool::Measure:return true;
    default:return !floating.isEmpty();
    }
}
void BoardView::cancelDrag(){
    if(drag==Drag::None)return;
    if(doc){
        auto &els=board().elements;auto fits=[&](int i){return i>=0&&i<els.size();};
        if(drag==Drag::Move){
            for(int k=0;k<selected.size()&&k<moving.size();k++)if(fits(selected[k]))els[selected[k]]=moving[k];
            for(auto it=rubberTracks.cbegin();it!=rubberTracks.cend();++it)if(fits(it.key()))els[it.key()]=it.value();
        }
        else if(drag==Drag::Node&&fits(nodeElement)&&!moving.isEmpty())els[nodeElement]=moving.first();
    }
    moving.clear();rubberTracks.clear();rubberNodes.clear();drag=Drag::None;update();
}
void BoardView::catchRubber(){
    rubberTracks.clear();rubberNodes.clear();if(!doc||rubberBand==0)return;const auto &b=board();
    QList<int> pads;for(int i:selected)if(b.elements[i].type==ElementType::Pad||b.elements[i].type==ElementType::SmdPad)pads.append(i);
    if(pads.isEmpty())return;
    QHash<qint64,QPainterPath> copper;
    auto padCopper=[&](int pad,int layer)->const QPainterPath&{const qint64 key=qint64(pad)*16+layer;if(!copper.contains(key))copper.insert(key,copperOn(b.elements[pad],layer,b));return copper[key];};
    for(int t=0;t<b.elements.size();t++){
        const auto &e=b.elements[t];if(e.type!=ElementType::Track||selected.contains(t)||!isCopper(e.layer))continue;
        for(int n=0;n<e.points.size();n++)for(int pad:pads){
            const auto &shape=padCopper(pad,e.layer);if(shape.isEmpty())continue;const QPointF d=e.points[n]-b.elements[pad].pos;
            // Small catch: the node on the pad's centre; large catch: anywhere on its copper.
            if(rubberBand==1?std::hypot(d.x(),d.y())<=.01:shape.contains(e.points[n])){rubberNodes.append({t,n});rubberTracks.insert(t,e);break;}
        }
    }
}
void BoardView::moveSelection(QPointF delta){
    cancelDrag();if(!doc||selected.isEmpty())return;catchRubber();
    change([&]{for(int i:selected)pcb::move(board().elements[i],delta);for(auto [t,n]:rubberNodes)board().elements[t].points[n]+=delta;});
    rubberTracks.clear();rubberNodes.clear();
}
void BoardView::change(const std::function<void()> &edit){cancelDrag();if(beforeChange)beforeChange();edit();testMarks.clear();findings.clear();update();if(changed)changed();}
void BoardView::addElement(Element e){
    change([&]{board().elements.append(e);});setSelection({int(board().elements.size())-1});
}
QPolygonF BoardView::bentPath(QPointF from,QPointF to) const{
    const QPointF d=to-from;QPolygonF out{from};
    const double ax=std::abs(d.x()),ay=std::abs(d.y()),sx=d.x()<0?-1:1,sy=d.y()<0?-1:1;
    switch(bendMode){
    case 1:{QPointF k=ax>ay?QPointF(from.x()+sx*(ax-ay),from.y()):QPointF(from.x(),from.y()+sy*(ay-ax));if(k!=from&&k!=to)out<<k;break;}
    case 2:{QPointF k=ax>ay?QPointF(from.x()+sx*ay,to.y()):QPointF(to.x(),from.y()+sy*ax);if(k!=from&&k!=to)out<<k;break;}
    case 3:{QPointF k(to.x(),from.y());if(k!=from&&k!=to)out<<k;break;}
    case 4:{QPointF k(from.x(),to.y());if(k!=from&&k!=to)out<<k;break;}
    default:break;
    }
    out<<to;return out;
}
void BoardView::finishDrawing(){
    const QPolygonF nodes=drawing;drawing.clear();
    QPolygonF clean;for(auto p:nodes)if(clean.isEmpty()||clean.last()!=p)clean<<p;
    if(optimizeNodes)clean=withoutRedundantNodes(clean,current!=Tool::Track);
    if(current==Tool::Area||current==Tool::Keepout){
        if(clean.size()>1&&clean.last()==clean.first())clean.removeLast();
        if(clean.size()<3){update();return;}
        Element e=newElement(ElementType::Area);e.layer=board().activeLayer;e.width=current==Tool::Keepout?0:trackWidth;e.points=clean;e.clearance=clearance;
        e.cutout=current==Tool::Keepout;addElement(e);
    }else{
        if(clean.size()<2){update();return;}
        Element e=newElement(ElementType::Track);e.layer=board().activeLayer;e.width=trackWidth;e.points=clean;e.clearance=clearance;addElement(e);
    }
}

void BoardView::placeFloating(QPointF at){
    QList<int> added;
    // Airwires among the placed elements count from the first of them. A component whose identifier the document or
    // whose number the board has already (a copy from another board) becomes a new one.
    change([&]{
        QSet<QString> taken;for(const auto &b:doc->boards)for(const auto &e:b.elements)if(e.role==TextRole::Designator&&!e.component.isEmpty())taken.insert(e.component);
        const int base=int(board().elements.size());auto placing=floating;freshParts(placing,board());
        for(auto e:placing){
            if(e.role==TextRole::Designator&&!e.component.isEmpty()&&taken.contains(e.component))e.component=newId();
            pcb::move(e,at);offsetLinks(e,base);board().elements.append(e);added.append(int(board().elements.size())-1);
        }});
    floating.clear();setSelection(added);if(floatingPlaced){auto done=std::move(floatingPlaced);floatingPlaced={};done(added);}
}
// A macro dragged from the library follows the pointer over the board; dropping it puts it down, leaving the board
// lets it go and brings back what waited at the pointer before. Only the drop changes the tool and the document.
void BoardView::dragEnterEvent(QDragEnterEvent *event){
    if(!doc||dropping||!dropElements)return;const auto elements=dropElements(event->mimeData());if(elements.isEmpty())return;
    stashed=floating;stashedPlaced=std::move(floatingPlaced);floating=elements;floatingPlaced={};dropping=true;
    pointerMm=toBoard(event->position());hasPointer=true;event->acceptProposedAction();update();
}
void BoardView::dragMoveEvent(QDragMoveEvent *event){if(!dropping)return;pointerMm=toBoard(event->position());hasPointer=true;event->acceptProposedAction();update();}
void BoardView::dragLeaveEvent(QDragLeaveEvent *){
    if(!dropping)return;floating=stashed;floatingPlaced=std::move(stashedPlaced);stashed.clear();stashedPlaced={};dropping=false;hasPointer=false;update();
}
void BoardView::dropEvent(QDropEvent *event){
    if(!dropping)return;const auto elements=floating;dropping=false;stashed.clear();stashedPlaced={};
    pointerMm=toBoard(event->position());event->acceptProposedAction();
    // Dropping is putting down: what is being drawn ends, the standard tool takes over. What follows the drop (the
    // component dialog) waits until the drag has ended.
    floating.clear();setTool(Tool::Select);if(!drawing.isEmpty())finishDrawing();
    floating=elements;floatingPlaced=[this](QList<int> added){QTimer::singleShot(0,this,[this,added]{if(dropped)dropped(added);});};
    placeFloating(snap(pointerMm));setFocus();
}
// --- input
void BoardView::mousePressEvent(QMouseEvent *event){
    if(!doc)return;setFocus();
    const QPointF mm=toBoard(event->position());pressMm=pointerMm=mm;moved=false;
    // The rulers' corner switches between millimetres and mil.
    if(event->button()==Qt::LeftButton&&event->position().x()<rulerSize&&event->position().y()<rulerSize){milUnits=!milUnits;update();if(unitsChanged)unitsChanged();return;}
    if(event->button()==Qt::MiddleButton||(event->button()==Qt::LeftButton&&event->modifiers()&Qt::AltModifier&&current!=Tool::Select)){cancelDrag();drag=Drag::Pan;lastPan=event->position();return;}
    if(event->button()==Qt::RightButton){
        // A drag under way is called off, nothing changes.
        if(drag!=Drag::None){cancelDrag();return;}
        // Right click ends what is being drawn; a second one returns to the standard tool, as in the reference.
        if(!floating.isEmpty()){floating.clear();update();return;}
        if(!drawing.isEmpty()){if(drawing.size()>1)finishDrawing();else drawing.clear();update();return;}
        if(series){endTextSeries();return;}
        if(current==Tool::Airwire&&airwireStart>=0){airwireStart=-1;update();return;}
        if(current==Tool::Zoom){zoomAt(1/zoomStep,event->position());return;}
        if(current!=Tool::Select){setTool(Tool::Select);return;}
        // A node of the selected track or area has a menu of its own.
        if(nodeMenuRequested)for(int n:nodeHandles()){const QPointF d=toPixel(board().elements[selected.first()].points[n])-event->position();
            if(std::hypot(d.x(),d.y())<=6){nodeMenuRequested(selected.first(),n,event->globalPosition().toPoint());return;}}
        // The popup menu works on the element under the pointer unless it belongs to the selection.
        {const int i=hit(mm);contextAt=mm;contextElement=i;if(i>=0&&!selected.contains(i))setSelection(withGroups(board(),{i}));}
        if(contextMenuRequested)contextMenuRequested(event->globalPosition().toPoint());
        return;
    }
    if(event->button()!=Qt::LeftButton)return;
    if(!floating.isEmpty()){placeFloating(snap(mm));return;}
    const auto &b=board();
    switch(current){
    case Tool::Select:{
        if(nearOrigin(event->position())){drag=Drag::Origin;return;}
        const auto handles=nodeHandles();
        for(int n:handles){const QPointF d=toPixel(b.elements[selected.first()].points[n])-event->position();if(std::hypot(d.x(),d.y())<=6){
            drag=Drag::Node;nodeElement=selected.first();nodeIndex=n;nodeInsert=false;moving={b.elements[nodeElement]};return;}}
        // A virtual node halfway along a segment becomes a node of its own once it is dragged.
        for(const auto &[index,at]:virtualNodes()){const QPointF d=toPixel(at)-event->position();if(std::hypot(d.x(),d.y())<=5){
            drag=Drag::Node;nodeElement=selected.first();nodeIndex=index;nodeInsert=true;moving={b.elements[nodeElement]};return;}}
        const int i=hit(mm);
        if(i<0){if(!(event->modifiers()&Qt::ShiftModifier))setSelection({});drag=Drag::Band;return;}
        // Alt picks a single element out of its group, Shift adds to or takes from the selection.
        const auto members=event->modifiers()&Qt::AltModifier?QList<int>{i}:withGroups(b,{i});
        if(event->modifiers()&Qt::ShiftModifier){auto s=selected;
            if(s.contains(i))for(int k:members)s.removeAll(k);else s.append(members);setSelection(s);return;}
        if(!selected.contains(i)||(event->modifiers()&Qt::AltModifier))setSelection(members);
        pressSnapped=snap(mm);drag=Drag::Move;moving.clear();for(int k:selected)moving.append(b.elements[k]);catchRubber();
        break;}
    case Tool::Zoom:drag=Drag::Zoom;break;
    case Tool::Keepout:if(keepoutRectangle){drag=Drag::Rectangle;break;}[[fallthrough]];
    case Tool::Track:case Tool::Area:{
        const QPointF at=snap(mm);
        if(drawing.isEmpty())drawing<<at;
        else{const auto bend=bentPath(drawing.last(),at);for(qsizetype k=1;k<bend.size();k++)drawing<<bend[k];
            if(current!=Tool::Track&&drawing.size()>3&&drawing.last()==drawing.first())finishDrawing();}
        update();break;}
    case Tool::Pad:{
        Element e=newElement(ElementType::Pad);e.layer=isCopper(b.activeLayer)?b.activeLayer:CopperBottom;e.pos=snap(mm);
        e.size=padDiameter;e.size2=padDrill;e.shape=padShape;e.via=padVia;e.clearance=clearance;updateOutline(e);addElement(e);break;}
    case Tool::Smd:{
        Element e=newElement(ElementType::SmdPad);e.layer=b.activeLayer==CopperBottom||b.activeLayer==CopperTop?b.activeLayer:CopperTop;e.pos=snap(mm);
        e.size=smdWidth;e.size2=smdHeight;e.clearance=clearance;updateOutline(e);addElement(e);break;}
    case Tool::Circle:drag=Drag::Circle;break;
    case Tool::Rectangle:drag=Drag::Rectangle;break;
    case Tool::Measure:drag=Drag::Measure;break;
    case Tool::Text:{
        // In a series the next number goes down without asking; one undo step each.
        if(series){Element e=seriesText;e.pos=snap(mm);e.text=seriesPrefix+QString::number(seriesNext++);seriesLast=std::max(seriesLast,seriesNext);updateStrokes(e);seriesTexts<<e;addElement(e);break;}
        Element e=newElement(ElementType::Text);e.layer=b.activeLayer;e.pos=snap(mm);e.size=textHeight;e.style=textStyle;e.thickness=textThickness;e.clearance=clearance;
        e.mirrored=e.layer==CopperBottom||e.layer==SilkBottom;
        if(textRequested&&!textRequested(e))return;if(e.text.isEmpty())return;
        updateStrokes(e);addElement(e);break;}
    case Tool::SolderMask:{
        // Clicking an element adds it to the solder mask openings or takes it away.
        const int i=hit(mm);if(i<0)return;change([&]{auto &e=board().elements[i];e.solderMask=!e.solderMask;});break;}
    case Tool::Test:
        // The copper of the active layer first, then any copper layer.
        testMarks=isCopper(b.activeLayer)?connectedAt(b,mm,b.activeLayer,testAirwires):QList<int>{};if(testMarks.isEmpty())testMarks=connectedAt(b,mm,0,testAirwires);
        blinkOn=true;if(blinkTest&&!testMarks.isEmpty())blinker->start();update();break;
    case Tool::Autoroute:{
        auto report=[this](const QString &text){if(autorouteStatus)autorouteStatus(text);};
        // An airwire under the pointer comes first. Otherwise a click on an autorouted track turns it back into the
        // airwire between its two pads; without them (one deleted, or none known) the track only loses its mark, as in
        // the reference.
        // Airwires of the schematic route the same way; they are not stored, so routing them removes none.
        bool schematic=false;const auto routable=routableAt(mm,&schematic);
        if(!routable){
            const int t=hit(mm,autoroutedTrack);if(t<0)return;
            const auto pads=b.elements[t].autoroutePads;const int n=int(b.elements.size());
            const bool known=pads[0]>=0&&pads[1]>=0&&pads[0]<n&&pads[1]<n;
            // A route the schematic asks for comes back as the schematic's airwire, not as a stored one.
            const bool wanted=known&&schematicConnects&&schematicConnects(pads[0],pads[1]);
            change([&]{auto &els=board().elements;
                if(!known){els[t].autorouted=false;els[t].autoroutePads={-1,-1};return;}
                const int one=pads[0],two=pads[1];
                if(one!=two&&!wanted){if(!els[one].connections.contains(two))els[one].connections.append(two);if(!els[two].connections.contains(one))els[two].connections.append(one);}
                removeElements(board(),{t});});
            setSelection({});report(ui("Autoroute aufgelöst"));return;
        }
        if(!isCopper(b.activeLayer)){report(ui("Der aktive Layer ist kein Kupferlayer"));return;}
        const auto pair=*routable;const double step=autorouteOnGrid?b.grid:std::max(.05,std::min(autorouteWidth,autorouteClearance)/2);
        const auto path=autoroute(b,pair.first,pair.second,b.activeLayer,autorouteWidth,autorouteClearance,step);
        if(path.size()<2){report(ui("Keine Verbindung gefunden"));return;}
        Element track=newElement(ElementType::Track);track.layer=b.activeLayer;track.width=autorouteWidth;track.points=path;track.autorouted=true;track.clearance=clearance;
        track.autoroutePads={pair.first,pair.second};     // the pad at the first node first
        change([&]{auto &els=board().elements;if(!schematic){els[pair.first].connections.removeAll(pair.second);els[pair.second].connections.removeAll(pair.first);}els.append(track);});
        report(ui("Verbindung verlegt"));break;}
    case Tool::Airwire:{
        const int pad=padAt(mm);
        if(pad<0){
            if(airwireStart<0){const int w=airwireAt(mm);if(w>=0){const auto pair=airwires()[w];
                change([&]{board().elements[pair.first].connections.removeAll(pair.second);board().elements[pair.second].connections.removeAll(pair.first);});}}
            return;
        }
        if(airwireStart<0||airwireStart>=b.elements.size()){airwireStart=pad;update();return;}
        if(pad!=airwireStart&&!b.elements[airwireStart].connections.contains(pad)){
            const int from=airwireStart;change([&]{board().elements[from].connections.append(pad);board().elements[pad].connections.append(from);});
        }
        airwireStart=pad;update();break;}
    }
}
void BoardView::mouseMoveEvent(QMouseEvent *event){
    if(!doc)return;
    pointerMm=toBoard(event->position());hasPointer=true;if(pointerMoved)pointerMoved(shownPointer());
    const QPointF d=event->position()-toPixel(pressMm);if(std::hypot(d.x(),d.y())>3)moved=true;
    switch(drag){
    case Drag::Pan:offset+=event->position()-lastPan;lastPan=event->position();if(viewChanged)viewChanged();break;
    case Drag::Move:if(moved){
        const QPointF delta=snap(pointerMm)-pressSnapped;auto &els=board().elements;
        for(int k=0;k<selected.size();k++){auto e=moving[k];pcb::move(e,delta);els[selected[k]]=e;}
        for(auto it=rubberTracks.cbegin();it!=rubberTracks.cend();++it)els[it.key()]=it.value();
        for(auto [t,n]:rubberNodes)els[t].points[n]+=delta;}
        break;
    case Drag::Node:if(moved){auto e=moving.first();if(nodeInsert)e.points.insert(nodeIndex,snap(pointerMm));else e.points[nodeIndex]=snap(pointerMm);board().elements[nodeElement]=e;}break;
    case Drag::Origin:break;
    default:break;
    }
    update();
}
void BoardView::mouseReleaseEvent(QMouseEvent *event){
    if(!doc)return;const auto mode=drag;drag=Drag::None;pointerMm=toBoard(event->position());
    switch(mode){
    case Drag::Move:case Drag::Node:{
        if(!moved){rubberTracks.clear();rubberNodes.clear();update();break;}
        // The elements already moved while dragging; put the old state back for the undo step, then apply.
        QList<int> indexes=mode==Drag::Move?selected:QList<int>{nodeElement};QList<Element> before=moving;
        // A node dragged onto the straight line between its neighbours goes.
        if(mode==Drag::Node&&optimizeNodes){auto &e=board().elements[nodeElement];const bool area=e.type==ElementType::Area;
            const auto fewer=withoutRedundantNodes(e.points,area);if(fewer.size()>=(area?3:2))e.points=fewer;}
        if(mode==Drag::Move)for(auto it=rubberTracks.cbegin();it!=rubberTracks.cend();++it){indexes.append(it.key());before.append(it.value());}
        QList<Element> now;for(int i:indexes)now.append(board().elements[i]);
        for(int k=0;k<indexes.size();k++)board().elements[indexes[k]]=before[k];
        // Back where it started (or a new node taken out again as redundant): no undo step.
        if(now!=before)change([&]{for(int k=0;k<indexes.size();k++)board().elements[indexes[k]]=now[k];});else update();
        rubberTracks.clear();rubberNodes.clear();
        break;}
    case Drag::Origin:{const QPointF at=originAt(pointerMm);if(moved&&at!=board().origin)change([&]{board().origin=at;});else update();break;}
    case Drag::Band:{
        const QRectF r=QRectF(pressMm,pointerMm).normalized();QList<int> s=(event->modifiers()&Qt::ShiftModifier)?selected:QList<int>{};
        const auto &b=board();for(int i=0;i<b.elements.size();i++)if(b.visible[b.elements[i].layer]&&r.contains(bounds(b.elements[i])))s.append(i);
        setSelection(withGroups(b,s));update();break;}
    case Drag::Zoom:{
        const QRectF r=QRectF(toPixel(pressMm),event->position()).normalized();
        if(moved&&r.width()>4&&r.height()>4){rememberView();const QPointF c=toBoard(r.center());pixelsPerMm=std::clamp(std::min(width()/r.width(),height()/r.height())*pixelsPerMm,.05,4000.0);centreOn(c);}
        else zoomAt(zoomStep,event->position());
        break;}
    case Drag::Circle:{
        Element e=newElement(ElementType::Circle);e.layer=board().activeLayer;e.pos=snap(pressMm);const QPointF d=snap(pointerMm)-e.pos;
        e.size=std::hypot(d.x(),d.y());e.width=trackWidth;e.clearance=clearance;if(e.size>0)addElement(e);else update();break;}
    case Drag::Rectangle:{
        const QRectF r=QRectF(snap(pressMm),snap(pointerMm)).normalized();if(r.width()<=0||r.height()<=0){update();break;}
        // A keep-out rectangle is an area without width that only cuts the ground plane.
        const bool keepout=current==Tool::Keepout,filled=filledRectangle||keepout;
        Element e=newElement(filled?ElementType::Area:ElementType::Track);e.layer=board().activeLayer;e.width=keepout?0:trackWidth;e.clearance=clearance;e.cutout=keepout;
        e.points={r.topLeft(),r.topRight(),r.bottomRight(),r.bottomLeft()};if(!filled)e.points<<r.topLeft();addElement(e);break;}
    default:update();break;
    }
}
void BoardView::mouseDoubleClickEvent(QMouseEvent *event){
    if(!doc)return;
    if((current==Tool::Track||current==Tool::Area||current==Tool::Keepout)&&!drawing.isEmpty()){finishDrawing();return;}
    if(current!=Tool::Select)return;const int i=hit(toBoard(event->position()));if(i<0)return;
    // Texts open for editing; other elements give their sizes to the next new elements.
    const auto &e=board().elements[i];
    if(e.type==ElementType::Text||!takeSizes){if(editRequested)editRequested(i);return;}
    switch(e.type){
    case ElementType::Track:case ElementType::Area:case ElementType::Circle:if(e.width>0)trackWidth=e.width;break;
    case ElementType::Pad:padDiameter=e.size;padDrill=e.size2;padShape=e.shape;padVia=e.via;break;
    case ElementType::SmdPad:smdWidth=e.size;smdHeight=e.size2;break;
    default:break;
    }
    if(sizesTaken)sizesTaken(e);
}
void BoardView::wheelEvent(QWheelEvent *event){
    // A mouse wheel zooms about the pointer as in the reference; a trackpad scrolls, and zooms with Ctrl (Cmd).
    const QPoint pixels=event->pixelDelta();
    if(!pixels.isNull()&&!(event->modifiers()&Qt::ControlModifier)){offset+=QPointF(pixels);viewMoved();return;}
    const double steps=(pixels.isNull()?event->angleDelta().y()/120.0:pixels.y()/60.0);if(steps==0)return;
    // Steps of the wheel in quick succession are one zoom for going back.
    const qint64 now=QDateTime::currentMSecsSinceEpoch();if(now-lastWheel>600)rememberView();lastWheel=now;
    zoomBy(std::pow(1.25,steps),event->position());
}
void BoardView::keyPressEvent(QKeyEvent *event){
    if(!doc)return;
    switch(event->key()){
    case Qt::Key_Escape:
        if(drag!=Drag::None){cancelDrag();return;}
        if(!floating.isEmpty()){floating.clear();update();return;}
        if(!drawing.isEmpty()){if(drawing.size()>1)finishDrawing();else drawing.clear();update();return;}
        if(series){endTextSeries();return;}
        if(current==Tool::Airwire&&airwireStart>=0){airwireStart=-1;update();return;}
        if(current!=Tool::Select){setTool(Tool::Select);return;}
        setSelection({});return;
    case Qt::Key_Space:if(current==Tool::Track||current==Tool::Area||current==Tool::Keepout){bendMode=(bendMode+1)%5;update();if(bendChanged)bendChanged();return;}break;
    case Qt::Key_0:if(hasPointer&&!(event->modifiers()&Qt::ControlModifier)){const QPointF at=originAt(pointerMm);if(at!=board().origin)change([&]{board().origin=at;});if(pointerMoved)pointerMoved(at);return;}break;
    case Qt::Key_Return:case Qt::Key_Enter:if(!drawing.isEmpty()){finishDrawing();return;}break;
    case Qt::Key_Left:case Qt::Key_Right:case Qt::Key_Up:case Qt::Key_Down:if(!selected.isEmpty()&&current==Tool::Select){
        // One grid step; as in the reference a tenth of it with Ctrl, half of it with Shift.
        const double g=board().grid/(event->modifiers()&Qt::ControlModifier?10:event->modifiers()&Qt::ShiftModifier?2:1);double dx=event->key()==Qt::Key_Left?-g:event->key()==Qt::Key_Right?g:0;if(below)dx=-dx;
        moveSelection(QPointF(dx,event->key()==Qt::Key_Up?-g:event->key()==Qt::Key_Down?g:0));return;}
        break;
    default:break;
    }
    // The mode keys.
    if(!(event->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier|Qt::ShiftModifier))){
        static const QMap<QString,Tool> tools{{"select",Tool::Select},{"zoom",Tool::Zoom},{"track",Tool::Track},{"pad",Tool::Pad},{"smd",Tool::Smd},{"circle",Tool::Circle},
            {"rectangle",Tool::Rectangle},{"area",Tool::Area},{"text",Tool::Text},{"airwire",Tool::Airwire},{"autoroute",Tool::Autoroute},{"test",Tool::Test},
            {"measure",Tool::Measure},{"solderMask",Tool::SolderMask}};
        const QString mode=event->key()?modeKeys.key(event->key()):QString();
        if(tools.contains(mode)){setTool(tools[mode]);return;}
        if(mode=="shape"&&shapeRequested){shapeRequested();return;}
        if(mode=="photo"&&photoRequested){photoRequested();return;}
        if(event->key()>=Qt::Key_1&&event->key()<=Qt::Key_9&&gridKey){gridKey(event->key()-Qt::Key_0);return;}
    }
    QWidget::keyPressEvent(event);
}
void BoardView::leaveEvent(QEvent *){hasPointer=false;update();}
QStringList BoardView::modes(){
    return {"select","zoom","track","pad","smd","circle","rectangle","area","shape","text","airwire","autoroute","test","measure","photo","solderMask"};
}
QString BoardView::modeName(const QString &mode){
    const QMap<QString,QString> names{{"select",ui("Standard")},{"zoom",ui("Zoom")},{"track",ui("Leiterbahn")},{"pad",ui("Lötauge")},{"smd",ui("SMD-Pad")},
        {"circle",ui("Kreisring")},{"rectangle",ui("Rechteck")},{"area",ui("Fläche")},{"shape",ui("Spezialform")},{"text",ui("Text")},{"airwire",ui("Luftlinie")},
        {"autoroute",ui("Autoroute")},{"test",ui("Test")},{"measure",ui("Messen")},{"photo",ui("Fotoansicht")},{"solderMask",ui("Lötstopp")}};
    return names.value(mode,mode);
}
QMap<QString,int> BoardView::defaultModeKeys(){
    return {{"select",Qt::Key_Escape},{"zoom",Qt::Key_Z},{"track",Qt::Key_L},{"pad",Qt::Key_P},{"smd",Qt::Key_S},{"circle",Qt::Key_R},{"rectangle",Qt::Key_Q},
            {"area",Qt::Key_F},{"shape",Qt::Key_N},{"text",Qt::Key_T},{"airwire",Qt::Key_C},{"autoroute",Qt::Key_A},{"test",Qt::Key_X},{"measure",Qt::Key_M},
            {"photo",Qt::Key_V},{"solderMask",Qt::Key_O}};
}
}
