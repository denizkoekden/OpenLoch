#include "view.h"
#include "dimension.h"
#include "nets.h"
#include "render.h"
#include "text.h"
#include "language.h"
#include "icons.h"
#include <QDragEnterEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QActionGroup>
#include <QCursor>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QToolTip>
#include <QPainter>
#include <QPainterPathStroker>
#include <QWheelEvent>
#include <QtMath>
#include <cmath>
#include <numbers>

namespace openloch::schematic {
namespace {
// Colours of the desk around the sheet, of the sheet while its title block is edited and of the component editor.
const QColor desk(150,150,150),componentDesk(214,234,255),markColour(0,90,255),guideColour(0,150,190);
constexpr double catchPixels=7;
double distance(QPointF a,QPointF b){return std::hypot(a.x()-b.x(),a.y()-b.y());}
QPainterPath widened(const QPainterPath &path,double width){
    QPainterPathStroker s;s.setWidth(width);s.setCapStyle(Qt::RoundCap);s.setJoinStyle(Qt::RoundJoin);return s.createStroke(path);
}
bool isOpen(const Item &i){return i.type==ItemType::Line||i.type==ItemType::Bezier||(i.type==ItemType::Ellipse&&i.arc==ArcStyle::Arc);}
}

SheetView::SheetView(QWidget *parent):QWidget(parent){
    setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);setAcceptDrops(true);setMinimumSize(200,150);
    setAttribute(Qt::WA_OpaquePaintEvent);
    linePreset.type=ItemType::Line;
    shapePreset.type=ItemType::Rectangle;
    junctionPreset.type=ItemType::Junction;junctionPreset.size=QSizeF(1,1);
    textPreset.type=ItemType::Text;textPreset.font.height=3;
    textBoxPreset.type=ItemType::TextBox;textBoxPreset.font.height=3;
    designatorPreset.type=ItemType::Text;designatorPreset.font.height=2.5;valuePreset=designatorPreset;
    contactPreset.type=ItemType::Contact;contactPreset.font.height=1.8;
    dimensionPreset.type=ItemType::Dimension;dimensionPreset.points={QPointF(),QPointF(),QPointF()};
    labelPreset.type=ItemType::NetLabel;labelPreset.font.height=2.5;
}
void SheetView::setDocument(Document *document){doc=document;editedId.clear();selected.clear();chosenGuide=-1;fitted=false;documentChanged();}
void SheetView::chooseGuide(int index,bool vertical){
    chosenGuide=index;chosenVertical=vertical;chosenSheet=doc&&!doc->sheets.isEmpty()?doc->sheet().id:QString();
    selected.clear();update();if(selectionChanged)selectionChanged();
}
Item *SheetView::editedComponent() const{
    if(editedId.isEmpty()||!doc||doc->sheets.isEmpty())return nullptr;
    Item *c=findItem(doc->sheet(),editedId);return c&&c->type==ItemType::Component?c:nullptr;
}
QList<Item> &SheetView::items(){
    if(Item *c=editedComponent())return c->children;
    if(!doc||doc->sheets.isEmpty()){emptyItems.clear();return emptyItems;}
    return titleMode?doc->sheet().titleBlock.items:doc->sheet().items;
}
const QList<Item> &SheetView::items() const{return const_cast<SheetView*>(this)->items();}
void SheetView::setTitleBlockMode(bool on){titleMode=on;selected.clear();drawing.clear();update();if(selectionChanged)selectionChanged();}
void SheetView::setComponentMode(const QString &componentId){editedId=componentId;selected.clear();drawing.clear();fitted=false;if(isVisible())fitSheet();update();if(selectionChanged)selectionChanged();}
void SheetView::documentChanged(){
    QStringList keep;for(const auto &id:selected)if(itemById(id)||(selected.size()==1&&nestedItem(id)))keep<<id;
    const bool differs=keep!=selected;selected=keep;
    if(chosenGuide>=0&&(!doc||doc->sheets.isEmpty()||doc->sheet().id!=chosenSheet||chosenGuide>=(chosenVertical?doc->sheet().verticalGuides:doc->sheet().horizontalGuides).size()))chosenGuide=-1;
    if(!fitted&&width()>0&&isVisible())fitSheet();
    update();if(differs&&selectionChanged)selectionChanged();
}
Item *SheetView::itemById(const QString &id){for(auto &i:items())if(i.id==id)return &i;return nullptr;}
Item *SheetView::nestedItem(const QString &id){
    std::function<Item*(QList<Item>&)> inGroups=[&](QList<Item> &list)->Item*{
        for(auto &i:list)if(i.type==ItemType::Group){for(auto &k:i.children)if(k.id==id)return &k;if(Item *deeper=inGroups(i.children))return deeper;}
        return nullptr;};
    return inGroups(items());
}

void SheetView::setTool(Tool tool){
    drawing.clear();current=tool;
    setCursor(tool==Tool::Select?Qt::ArrowCursor:tool==Tool::Zoom?openLochCursor("lupe"):Qt::CrossCursor);
    switch(tool){
    case Tool::Line:case Tool::Polygon:setHint(ui("Klicken setzt Eckpunkte, die rechte Maustaste beendet."));break;
    case Tool::Bezier:setHint(ui("Klicken setzt Startpunkt, zwei Kontrollpunkte und Endpunkt, je weitere Kurve zwei Kontrollpunkte und einen Endpunkt; die rechte Maustaste beendet."));break;
    case Tool::Freehand:setHint(ui("Bei gedrückter Maustaste zeichnen."));break;
    case Tool::Special:setHint(ui("Einen Rahmen für die Spezialform aufziehen."));break;
    case Tool::Measure:setHint(ui("Bei gedrückter Maustaste messen."));break;
    case Tool::Dimension:setHint(dimensionPreset.dimension==DimensionKind::Angle?ui("Klicken setzt den Scheitel, einen Punkt auf dem ersten und einen auf dem zweiten Schenkel."):
        dimensionPreset.dimension==DimensionKind::Standard?ui("Klicken setzt die beiden Punkte und die Lage der Maßlinie."):ui("Klicken setzt die beiden Punkte."));break;
    // A hint for every mode, also the standard one, as in the reference.
    case Tool::Select:setHint(ui("Elemente anklicken oder mit einem Rahmen markieren, ziehen, doppelt anklicken zum Bearbeiten; die rechte Maustaste öffnet das Kontextmenü."));break;
    case Tool::Junction:setHint(ui("Klicken setzt einen Lötpunkt, die rechte Maustaste beendet den Modus."));break;
    case Tool::Rectangle:case Tool::Ellipse:setHint(ui("Einen Rahmen aufziehen (von der Ecke oder vom Mittelpunkt, wie am Werkzeugknopf gewählt); Umschalt ergibt Quadrat oder Kreis."));break;
    case Tool::TextBox:setHint(ui("Einen Rahmen für den Mengentext aufziehen, danach den Text eingeben."));break;
    case Tool::Text:setHint(ui("Klicken setzt einen Text, danach den Text eingeben."));break;
    case Tool::NetLabel:setHint(ui("Klicken setzt einen Netznamen."));break;
    case Tool::SheetReference:setHint(ui("Klicken setzt einen Blattverweis."));break;
    case Tool::Contact:setHint(ui("Klicken setzt einen Kontakt des Bauteils."));break;
    case Tool::Zoom:setHint(ui("Klicken vergrößert, ein Rahmen zoomt auf seinen Bereich, die rechte Maustaste verkleinert."));break;
    default:setHint(ui("Klicken setzt das Element, die rechte Maustaste beendet den Modus."));break;
    }
    update();if(toolChanged)toolChanged(tool);
}
void SheetView::setHint(const QString &hint){if(hintChanged)hintChanged(hint);}
void SheetView::setSelection(const QStringList &ids){
    selected.clear();for(const auto &id:ids)if(itemById(id)&&!selected.contains(id))selected<<id;
    // One element of a group alone.
    if(selected.isEmpty()&&ids.size()==1&&nestedItem(ids[0]))selected=ids;
    update();if(selectionChanged)selectionChanged();
}
void SheetView::selectAll(){QStringList ids;for(const auto &i:items())ids<<i.id;setSelection(ids);}

void SheetView::beginPlacement(const QList<Item> &list,std::function<void(QStringList)> placed){
    floating=list;floatingPlaced=std::move(placed);setFocus();update();
    setHint(ui("Klicken setzt die Elemente, Strg+R dreht sie, die rechte Maustaste bricht ab."));
}
void SheetView::turnPlacement(double degrees){for(auto &i:floating)rotate(i,QPointF(),degrees);update();}

// --- view
QRectF SheetView::sheetRect() const{
    if(const Item *edited=editedComponent()){QRectF r=bounds(edited->children);r=r.united(QRectF(-5,-5,10,10));return r.adjusted(-10,-10,10,10);}
    if(!doc||doc->sheets.isEmpty())return QRectF(0,0,297,210);
    return QRectF(0,0,doc->sheet().width,doc->sheet().height);
}
void SheetView::fitSheet(){
    const QRectF r=sheetRect();const double w=width()-rulerSize-20,h=height()-rulerSize-20;
    if(r.isEmpty()||w<=0||h<=0)return;
    setScale(std::min(w/r.width(),h/r.height()));
    offset=QPointF(rulerSize+10+(w-r.width()*pixelsPerMm)/2-r.left()*pixelsPerMm,rulerSize+10+(h-r.height()*pixelsPerMm)/2-r.top()*pixelsPerMm);
    fitted=true;update();
}
void SheetView::centreOn(QPointF p){
    offset=QPointF(rulerSize+(width()-rulerSize)/2.,rulerSize+(height()-rulerSize)/2.)-p*pixelsPerMm;update();
}
void SheetView::fitItems(bool selectedOnly){
    QRectF r;for(const auto &i:items())if(!selectedOnly||selected.contains(i.id))r=r.united(bounds(i));
    if(r.isEmpty())return;
    r=r.adjusted(-r.width()*.05-2,-r.height()*.05-2,r.width()*.05+2,r.height()*.05+2);
    const double w=width()-rulerSize-20,h=height()-rulerSize-20;
    setScale(std::min(w/r.width(),h/r.height()));
    offset=QPointF(rulerSize+10+(w-r.width()*pixelsPerMm)/2-r.left()*pixelsPerMm,rulerSize+10+(h-r.height()*pixelsPerMm)/2-r.top()*pixelsPerMm);
    update();
}
void SheetView::setScale(double pixels){
    const double before=pixelsPerMm;pixelsPerMm=std::clamp(pixels,minScale,maxScale);
    if(pixelsPerMm!=before&&zoomChanged)zoomChanged();
}
void SheetView::zoomAt(double factor,QPointF pixel){
    const QPointF at=toSheet(pixel);setScale(pixelsPerMm*factor);
    offset=pixel-at*pixelsPerMm;fitted=true;update();
}
void SheetView::zoomCentred(double factor,QPointF pixel){
    const QPointF at=toSheet(pixel);setScale(pixelsPerMm*factor);
    const QPointF middle(rulerSize+(width()-rulerSize)/2.,rulerSize+(height()-rulerSize)/2.);
    offset=middle-at*pixelsPerMm;fitted=true;update();
}
QPointF SheetView::toSheet(QPointF pixel) const{return (pixel-offset)/pixelsPerMm;}
QPointF SheetView::toPixel(QPointF mm) const{return mm*pixelsPerMm+offset;}
QPointF SheetView::onGrid(QPointF mm) const{
    const double g=doc&&!doc->sheets.isEmpty()&&doc->sheet().grid>0?doc->sheet().grid:1;
    return QPointF(std::round(mm.x()/g)*g,std::round(mm.y()/g)*g);
}
bool SheetView::terminalAt(QPointF mm,QPointF *at) const{
    double best=catchPixels/pixelsPerMm;bool found=false;
    auto consider=[&](QPointF p){const double d=distance(p,mm);if(d<best){best=d;found=true;if(at)*at=p;}};
    std::function<void(const QList<Item>&)> scan=[&](const QList<Item> &list){
        for(const auto &i:list){
            switch(i.type){
            case ItemType::Line:for(auto p:i.points)consider(p);break;
            case ItemType::Bezier:if(!i.points.isEmpty()){consider(i.points.first());consider(i.points.last());}break;
            case ItemType::Junction:case ItemType::NetLabel:consider(i.pos);break;
            case ItemType::Contact:if(i.hasPin)consider(i.pin);break;
            case ItemType::Group:scan(i.children);break;
            case ItemType::Component:for(auto p:connectionPoints(i))consider(p);break;
            default:break;
            }
        }
    };
    scan(items());
    return found;
}
Item SheetView::dimensionDraft(const QPolygonF &clicks,QPointF pointer) const{
    Item d=dimensionPreset;d.type=ItemType::Dimension;d.points.clear();d.offset=0;
    QPolygonF all=clicks;all<<pointer;
    if(d.dimension==DimensionKind::Angle){
        // Vertex, first leg (its radius), second leg in the direction of the third point.
        if(all.size()<2){d.points={all[0]};return d;}
        const QPointF v=all[0],a=all[1];const double r=QLineF(v,a).length();
        QPointF b=all.size()>2?all[2]:a+QPointF(0,-r);
        const double l=QLineF(v,b).length();if(l>1e-9)b=v+(b-v)*(r/l);
        d.points={a,b,v};return d;
    }
    if(all.size()<2){d.points={all[0]};return d;}
    d.points={all[0],all[1],all[0]};
    if(d.dimension==DimensionKind::Standard&&all.size()>2){
        // The third click: the dimension line through it, parallel to the points.
        const QPointF u=all[1]-all[0];const double l=std::hypot(u.x(),u.y());
        if(l>1e-9)d.offset=QPointF::dotProduct(all[2]-all[0],QPointF(u.y(),-u.x())/l);
    }
    return d;
}
bool SheetView::guidesShown() const{return doc&&!doc->sheets.isEmpty()&&!guidesHidden&&editedId.isEmpty();}
int SheetView::guideAtPixel(QPointF pixel,bool *vertical) const{
    if(!guidesShown())return -1;
    int best=-1;double nearest=4;
    const Sheet &s=doc->sheet();
    for(int k=0;k<s.verticalGuides.size();k++){const double d=std::abs(toPixel(QPointF(s.verticalGuides[k],0)).x()-pixel.x());if(d<=nearest){nearest=d;best=k;*vertical=true;}}
    for(int k=0;k<s.horizontalGuides.size();k++){const double d=std::abs(toPixel(QPointF(0,s.horizontalGuides[k])).y()-pixel.y());if(d<=nearest){nearest=d;best=k;*vertical=false;}}
    return best;
}
// The guide lines of the sheet, taken after startChange(): the undo step shares the lists until they change.
QList<double> &SheetView::guides(bool vertical){return vertical?doc->sheet().verticalGuides:doc->sheet().horizontalGuides;}
bool SheetView::addGuide(bool vertical,double at){
    if(!guidesShown()||guidesFixed)return false;
    startChange();guides(vertical)<<at;finishChange();
    return true;
}
bool SheetView::deleteChosenGuide(){
    if(chosenGuide<0||!guidesShown()||guidesFixed)return false;
    if(chosenGuide<guides(chosenVertical).size()){startChange();guides(chosenVertical).removeAt(chosenGuide);finishChange();}
    chosenGuide=-1;update();if(selectionChanged)selectionChanged();return true;
}
QList<QPointF> SheetView::sizers() const{
    if(!doc||selected.isEmpty()||current!=Tool::Select||!drawing.isEmpty()||!floating.isEmpty())return {};
    // Around the geometry without line widths, so that stretching and turning go exactly through the points.
    QRectF r;for(const auto &i:items())if(selected.contains(i.id)){const QPainterPath p=path(i);r|=p.isEmpty()?bounds(i):p.boundingRect();}
    if(r.isNull())return {};
    const QPointF c=r.center();
    return {r.topLeft(),QPointF(c.x(),r.top()),r.topRight(),QPointF(r.right(),c.y()),r.bottomRight(),QPointF(c.x(),r.bottom()),r.bottomLeft(),QPointF(r.left(),c.y())};
}
QPointF SheetView::originPoint() const{
    if(!doc||doc->sheets.isEmpty())return {};
    const Sheet &s=doc->sheet();
    const bool right=origin==Origin::TopRight||origin==Origin::BottomRight,bottom=origin==Origin::BottomLeft||origin==Origin::BottomRight;
    return QPointF(right?s.width:0,bottom?s.height:0);
}
QRectF SheetView::shapeFrame(QPointF from,QPointF to,Qt::KeyboardModifiers modifiers) const{
    QPointF d=to-from;
    if(modifiers&Qt::ShiftModifier){const double m=std::max(std::abs(d.x()),std::abs(d.y()));d=QPointF(d.x()<0?-m:m,d.y()<0?-m:m);}
    const bool centred=(current==Tool::Rectangle&&rectanglesFromCentre)||(current==Tool::Ellipse&&ellipsesFromCentre);
    if(centred){const QPointF h(std::abs(d.x()),std::abs(d.y()));return QRectF(from-h,from+h);}
    return QRectF(from,from+d).normalized();
}
QPointF SheetView::snapped(QPointF mm,Qt::KeyboardModifiers modifiers,const QPointF *from) const{
    QPointF p=mm;
    // As in the reference: Strg frees from the grid, Alt from terminals, Umschalt from the 45° steps.
    if(terminalSnap&&!(modifiers&Qt::AltModifier)){QPointF t;if(terminalAt(mm,&t))return t;}
    if(gridSnap&&!(modifiers&Qt::ControlModifier))p=onGrid(p);
    if(guidesShown()){
        // The nearest guide line within reach attracts.
        const double reach=magnetPixels/pixelsPerMm;const Sheet &s=doc->sheet();
        double nx=reach,ny=reach;
        for(double x:s.verticalGuides)if(std::abs(mm.x()-x)<=nx){nx=std::abs(mm.x()-x);p.setX(x);}
        for(double y:s.horizontalGuides)if(std::abs(mm.y()-y)<=ny){ny=std::abs(mm.y()-y);p.setY(y);}
    }
    if(from&&angleSnap&&!(modifiers&Qt::ShiftModifier)){
        const QPointF d=mm-*from;if(std::hypot(d.x(),d.y())<1e-9)return p;
        const double step=std::numbers::pi/4,a=std::round(std::atan2(d.y(),d.x())/step)*step;const QPointF dir(std::cos(a),std::sin(a));
        if(std::abs(dir.y())<1e-9)return QPointF(p.x(),from->y());
        if(std::abs(dir.x())<1e-9)return QPointF(from->x(),p.y());
        const double t=QPointF::dotProduct(p-*from,dir);return *from+t*dir;
    }
    return p;
}
QList<QPointF> SheetView::connectionPoints(const Item &component) const{
    QList<QPointF> out;const QTransform t=placement(component);
    for(const auto &c:component.children){
        if(c.type==ItemType::Contact&&c.hasPin)out<<t.map(c.pin);
        else if(c.type==ItemType::Line&&c.points.size()>=2){out<<t.map(c.points.first())<<t.map(c.points.last());}
    }
    return out;
}
bool SheetView::hits(const Item &item,QPointF mm) const{
    const double tol=4/pixelsPerMm;
    switch(item.type){
    case ItemType::Line:case ItemType::Bezier:return widened(path(item),std::max(item.pen.width,2*tol)).contains(mm);
    case ItemType::Polygon:case ItemType::Rectangle:case ItemType::Ellipse:{
        const auto p=path(item);
        if(item.fill.style!=FillStyle::None&&!(item.type==ItemType::Ellipse&&item.arc==ArcStyle::Arc)&&p.contains(mm))return true;
        return widened(p,std::max(item.pen.width,2*tol)).contains(mm);
    }
    case ItemType::TextBox:case ItemType::Image:return corners(item).containsPoint(mm,Qt::OddEvenFill);
    case ItemType::Junction:return distance(item.pos,mm)<=std::max(item.size.width()/2,tol);
    case ItemType::Text:case ItemType::NetLabel:case ItemType::Contact:
        return textOutline(item,item.text.isEmpty()?QStringLiteral("  "):item.text).containsPoint(mm,Qt::OddEvenFill)||(item.type==ItemType::Contact&&item.hasPin&&distance(item.pin,mm)<tol);
    case ItemType::Group:for(const auto &c:item.children)if(hits(c,mm))return true;return false;
    case ItemType::Dimension:{
        const auto d=dimensionDrawing(item,doc&&!doc->sheets.isEmpty()?doc->sheet().scale:1);
        QPainterPath lines=d.line;for(const auto &e:d.extensions){lines.moveTo(e.p1());lines.lineTo(e.p2());}
        if(widened(lines,2*tol).contains(mm))return true;
        for(const auto &a:d.arrows)if(a.containsPoint(mm,Qt::OddEvenFill))return true;
        for(const auto &t:d.texts)if(textOutline(t,t.text).containsPoint(mm,Qt::OddEvenFill))return true;
        return false;
    }
    case ItemType::Component:{
        const QPointF local=placement(item).inverted().map(mm);
        for(const auto &c:item.children){
            if(isText(c)){
                if(!c.visible||(c.role==TextRole::Designator&&!item.designatorVisible)||(c.role==TextRole::Value&&!item.valueVisible))continue;
                if(textOutline(placedText(c,item),componentText(item,c)).containsPoint(mm,Qt::OddEvenFill))return true;
            }else if(hits(c,local))return true;
        }
        return false;
    }
    }
    return false;
}
int SheetView::hit(QPointF mm) const{
    const auto &list=items();
    for(int i=int(list.size())-1;i>=0;i--)if(hits(list[i],mm))return i;
    return -1;
}

// --- changes
void SheetView::startChange(){if(!changedDuringDrag){changedDuringDrag=true;if(beforeChange)beforeChange();}}
void SheetView::finishChange(){if(changedDuringDrag){changedDuringDrag=false;if(changed)changed();}update();}
void SheetView::cancelChange(){
    if(changedDuringDrag){changedDuringDrag=false;if(changeCancelled)changeCancelled();else if(changed)changed();}
    update();
}
void SheetView::addItem(Item item){
    if(item.id.isEmpty())assignIds(item);
    startChange();items().append(item);finishChange();
    selected={item.id};if(selectionChanged)selectionChanged();
}
namespace {
// Moves the selected elements; conductors ending at the connection points of moved components follow them.
void applyMove(QList<Item> &list,const QStringList &ids,QPointF delta,bool rubber,const std::function<QList<QPointF>(const Item&)> &points){
    QList<QPointF> attached;
    if(rubber){
        std::function<void(const Item&)> collect=[&](const Item &i){
            if(i.type==ItemType::Component)attached<<points(i);
            else if(i.type==ItemType::Group)for(const auto &c:i.children)collect(c);
        };
        for(const auto &i:list)if(ids.contains(i.id))collect(i);
    }
    for(auto &i:list){
        if(ids.contains(i.id)){schematic::move(i,delta);continue;}
        if(!rubber||i.type!=ItemType::Line||!i.electrical||i.points.size()<2)continue;
        for(int end:{0,int(i.points.size())-1})
            for(auto p:attached)if(distance(i.points[end],p)<tolerance){i.points[end]+=delta;break;}
    }
}
}
void SheetView::moveSelection(QPointF delta){
    // Only elements of the top level move (an element of a group chosen alone with Alt does not).
    if(selected.isEmpty()||delta.isNull()||std::none_of(selected.cbegin(),selected.cend(),[this](const QString &id){return itemById(id)!=nullptr;}))return;
    startChange();applyMove(items(),selected,delta,rubberBand&&!titleMode&&editedId.isEmpty(),[this](const Item &i){return connectionPoints(i);});finishChange();
}
void SheetView::removeNode(const QString &id,int node){
    Item *i=itemById(id);if(!i||node<0||node>=i->points.size())return;
    const int least=i->type==ItemType::Polygon?3:2;if(i->points.size()<=least)return;
    startChange();i->points.remove(node);finishChange();
}
void SheetView::splitLine(const QString &id,int node){
    Item *i=itemById(id);if(!i||i->type!=ItemType::Line||node<=0||node>=i->points.size()-1)return;
    startChange();
    Item second=*i;second.points=i->points.mid(node);assignIds(second);second.startEnd=LineEnd::None;
    i->points=i->points.mid(0,node+1);i->endEnd=LineEnd::None;
    const int at=int(i-items().data());items().insert(at+1,second);
    finishChange();
}
void SheetView::joinLines(){
    // Two selected lines with a common end become one.
    if(selected.size()!=2)return;
    Item *a=itemById(selected[0]),*b=itemById(selected[1]);
    if(!a||!b||a->type!=ItemType::Line||b->type!=ItemType::Line)return;
    QPolygonF p=a->points,q=b->points;
    if(distance(p.first(),q.first())<tolerance)std::reverse(p.begin(),p.end());
    else if(distance(p.first(),q.last())<tolerance){std::swap(p,q);}
    else if(distance(p.last(),q.last())<tolerance)std::reverse(q.begin(),q.end());
    else if(!(distance(p.last(),q.first())<tolerance))return;
    startChange();
    a->points=p+q.mid(1);const QString gone=b->id;
    for(int k=0;k<items().size();k++)if(items()[k].id==gone){items().removeAt(k);break;}
    selected={a->id};finishChange();if(selectionChanged)selectionChanged();
}
void SheetView::finishDrawing(bool close){
    if(current==Tool::Bezier){
        // A start point and three points for each curve; points left over are dropped.
        QPolygonF nodes=drawing;drawing.clear();
        if(nodes.size()>=4){nodes.resize((nodes.size()-1)/3*3+1);Item b=linePreset;b.type=ItemType::Bezier;b.points=nodes;b.electrical=false;b.children.clear();addItem(b);}
        update();return;
    }
    QPolygonF nodes;for(auto p:drawing)if(nodes.isEmpty()||distance(nodes.last(),p)>1e-9)nodes<<p;
    drawing.clear();
    if(current==Tool::Polygon||close){
        if(nodes.size()>=3){Item p=shapePreset;p.type=ItemType::Polygon;p.points=nodes;p.children.clear();addItem(p);}
    }else if(nodes.size()>=2){Item l=linePreset;l.type=ItemType::Line;l.points=nodes;l.electrical=!titleMode&&editedId.isEmpty();addItem(l);}
    update();
}

// --- handles
QList<QPointF> SheetView::handles(const Item &item) const{
    switch(item.type){
    case ItemType::Line:case ItemType::Polygon:case ItemType::Bezier:return QList<QPointF>(item.points.begin(),item.points.end());
    case ItemType::Rectangle:case ItemType::Ellipse:case ItemType::TextBox:case ItemType::Image:{
        const auto c=corners(item);QList<QPointF> out{c[0],c[1],c[2],c[3]};
        // The changers of the reference: a rectangle's rounding on its top edge, an arc's start and end on the curve.
        const QTransform f=QTransform().translate(item.centre.x(),item.centre.y()).rotate(-item.rotation);   // as the model's frame
        const double w=item.size.width()/2,h=item.size.height()/2;
        if(item.type==ItemType::Rectangle){const double r=std::clamp(item.corner,0.,50.)/100*std::min(std::abs(2*w),std::abs(2*h));out<<f.map(QPointF(-w+roundingShown(r,2*w),-h));}
        if(item.type==ItemType::Ellipse&&item.arc!=ArcStyle::Full)
            for(double a:{item.start,item.stop}){
                const double t=qDegreesToRadians(a),cx=std::cos(t),sy=std::sin(t),rx=std::max(std::abs(w),1e-9),ry=std::max(std::abs(h),1e-9);
                const double r=1/std::sqrt(cx*cx/(rx*rx)+sy*sy/(ry*ry));out<<f.map(QPointF(r*cx,-r*sy));
            }
        return out;
    }
    case ItemType::Dimension:{
        // Its points, and for lengths the middle of the dimension line, which moves the line.
        QList<QPointF> out(item.points.begin(),item.points.end());
        if(item.dimension!=DimensionKind::Angle&&item.points.size()>=2){
            const QPointF d=item.points[1]-item.points[0];const double l=std::hypot(d.x(),d.y());
            const QPointF n=l>1e-9?QPointF(d.y(),-d.x())/l:QPointF(0,-1);
            out<<(item.points[0]+item.points[1])/2+n*item.offset;
        }
        return out;
    }
    default:return {};
    }
}
int SheetView::handleAt(const Item &item,QPointF pixel) const{
    const auto list=handles(item);
    for(int i=int(list.size())-1;i>=0;i--)if(distance(toPixel(list[i]),pixel)<=5)return i;
    return -1;
}
QList<QPointF> SheetView::virtualNodes(const Item &item) const{
    if(item.type!=ItemType::Line&&item.type!=ItemType::Polygon)return {};
    QList<QPointF> out;const auto &p=item.points;
    for(int k=0;k+1<p.size();k++)out<<(p[k]+p[k+1])/2;
    if(item.type==ItemType::Polygon&&p.size()>2)out<<(p.last()+p.first())/2;
    return out;
}
QString SheetView::linkArrowAt(QPointF mm,bool *outgoing) const{
    if(!doc||doc->sheets.isEmpty())return {};
    // A few pixels around the arrow count, as small as it is drawn.
    const double slack=4/std::max(1e-9,pixelsPerMm);QString found;
    std::function<void(const QList<Item>&)> walk=[&](const QList<Item> &list){
        for(qsizetype k=list.size()-1;k>=0&&found.isEmpty();k--){
            const Item &t=list[k];
            if(t.type==ItemType::Group){walk(t.children);continue;}
            if(t.type!=ItemType::Text||(!t.linkable&&t.linkTarget.isEmpty()))continue;
            const QString shown=shownText(t,{doc,doc->activeSheet,nullptr,fileName});if(shown.isEmpty())continue;
            const QRectF box=textRect(t,shown);const QTransform frame=textFrame(t);
            auto at=[&](bool out){return frame.map(linkArrow(box,out)).boundingRect().adjusted(-slack,-slack,slack,slack).contains(mm);};
            if(!t.linkTarget.isEmpty()&&at(true)){found=t.id;if(outgoing)*outgoing=true;}
            else if(t.linkable&&at(false)){found=t.id;if(outgoing)*outgoing=false;}
        }
    };
    walk(items());return found;
}
QString SheetView::linkWindowText(const QString &id,bool outgoing) const{
    if(!doc)return {};
    auto line=[&](const PlacedComponent &p){return QStringLiteral("%1: %2 – %3").arg(p.sheet+1).arg(doc->sheets[p.sheet].name,shownText(*p.item,{doc,p.sheet,nullptr,fileName}));};
    const PlacedComponent self=textWithId(*doc,id);if(!self.item)return {};
    if(outgoing){const PlacedComponent target=textWithId(*doc,self.item->linkTarget);return ui("Linkziel:")+u'\n'+(target.item?line(target):ui("fehlt"));}
    QStringList lines{ui("Verlinkt von:")};for(const auto &p:linksTo(*doc,id))lines<<line(p);
    return lines.join(u'\n');
}
QString SheetView::componentText(const Item &component,const Item &text) const{
    if(!doc||doc->sheets.isEmpty())return text.role==TextRole::Designator?component.designator:text.role==TextRole::Value?component.value:text.text;
    return shownText(text,{doc,doc->activeSheet,&component,fileName});
}
int SheetView::componentTextAt(const Item &item,QPointF mm) const{
    if(item.type!=ItemType::Component)return -1;
    for(int k=int(item.children.size())-1;k>=0;k--){
        const Item &c=item.children[k];
        if(!isText(c)||!c.visible||(c.role==TextRole::Designator&&!item.designatorVisible)||(c.role==TextRole::Value&&!item.valueVisible))continue;
        if(textOutline(placedText(c,item),componentText(item,c)).containsPoint(mm,Qt::OddEvenFill))return k;
    }
    return -1;
}
int SheetView::virtualNodeAt(const Item &item,QPointF pixel) const{
    const auto list=virtualNodes(item);
    for(int i=0;i<list.size();i++)if(distance(toPixel(list[i]),pixel)<=5)return i;
    return -1;
}

// --- painting
void SheetView::paintRulers(QPainter &painter){
    const QColor back(236,236,236);
    painter.fillRect(QRect(0,0,width(),rulerSize),back);painter.fillRect(QRect(0,0,rulerSize,height()),back);
    painter.setPen(QColor(90,90,90));painter.drawLine(rulerSize,rulerSize-1,width(),rulerSize-1);painter.drawLine(rulerSize-1,rulerSize,rulerSize-1,height());
    // Ticks every 1, 5 or 10 mm and so on, numbers where there is room; in the sheet's scale.
    const double f=doc&&!doc->sheets.isEmpty()&&doc->sheet().scale>0?doc->sheet().scale:1;
    double step=1;while(step/f*pixelsPerMm<6)step*=step==1?5:2;
    double label=step*10;while(label/f*pixelsPerMm<50)label*=2;
    QFont small=font();small.setPixelSize(9);painter.setFont(small);
    const QRectF visible(toSheet(QPointF(rulerSize,rulerSize)),toSheet(QPointF(width(),height())));
    // Counted from the chosen origin.
    const QPointF o=originPoint()*f;
    for(double x=std::floor((visible.left()*f-o.x())/step)*step;x<=visible.right()*f-o.x();x+=step){
        const double px=toPixel(QPointF((x+o.x())/f,0)).x();if(px<rulerSize)continue;
        const bool big=std::abs(std::remainder(x,label))<step/2;const bool mid=std::abs(std::remainder(x,step*5))<step/2;
        painter.drawLine(QPointF(px,rulerSize-1),QPointF(px,rulerSize-(big?9:mid?6:3)));
        if(big)painter.drawText(QPointF(px+2,9),QString::number(std::lround(x)));
    }
    for(double y=std::floor((visible.top()*f-o.y())/step)*step;y<=visible.bottom()*f-o.y();y+=step){
        const double py=toPixel(QPointF(0,(y+o.y())/f)).y();if(py<rulerSize)continue;
        const bool big=std::abs(std::remainder(y,label))<step/2;const bool mid=std::abs(std::remainder(y,step*5))<step/2;
        painter.drawLine(QPointF(rulerSize-1,py),QPointF(rulerSize-(big?9:mid?6:3),py));
        if(big){painter.save();painter.translate(9,py+2);painter.rotate(-90);painter.drawText(QPointF(-30,0),QString::number(std::lround(y)).rightJustified(5));painter.restore();}
    }
    // The origin button between the rulers: a small sheet with a dot at the chosen corner.
    {const QRectF sheet(5,5,rulerSize-11,rulerSize-11);painter.setBrush(Qt::white);painter.setPen(QColor(90,90,90));painter.drawRect(sheet);
        const bool right=origin==Origin::TopRight||origin==Origin::BottomRight,bottom=origin==Origin::BottomLeft||origin==Origin::BottomRight;
        painter.setBrush(QColor(200,0,0));painter.setPen(Qt::NoPen);painter.drawEllipse(QPointF(right?sheet.right():sheet.left(),bottom?sheet.bottom():sheet.top()),2.2,2.2);painter.setBrush(Qt::NoBrush);}
    // The unit at the far end of each ruler.
    {const QString unit=doc&&!doc->sheets.isEmpty()?unitName(doc->sheet().scaleUnit):unitName(ScaleUnit::Millimetre);
        const int w=painter.fontMetrics().horizontalAdvance(unit)+6;painter.setPen(QColor(90,90,90));
        painter.fillRect(QRect(width()-w,0,w,rulerSize-1),back);painter.drawText(QRect(width()-w,0,w,rulerSize-1),Qt::AlignCenter,unit);
        painter.fillRect(QRect(0,height()-w,rulerSize-1,w),back);
        painter.save();painter.translate(0,height());painter.rotate(-90);painter.drawText(QRect(0,0,w,rulerSize-1),Qt::AlignCenter,unit);painter.restore();}
    if(pointerInside){
        painter.setPen(QColor(255,0,0));const QPointF p=toPixel(pointer);
        painter.drawLine(QPointF(p.x(),0),QPointF(p.x(),rulerSize-1));painter.drawLine(QPointF(0,p.y()),QPointF(rulerSize-1,p.y()));
    }
    painter.fillRect(QRect(0,0,rulerSize,rulerSize),back);
}
// The snap grid as the reference shows it: a dot on every point, a grey cross on every `gridMarks`-th in both directions
// (counted from the sheet's corner), or thin lines; hidden where its points come closer than seven pixels.
void SheetView::paintGrid(QPainter &painter,const QRectF &area){
    if(!gridSnap||gridContrast<=0||!doc||doc->sheets.isEmpty())return;
    const double g=doc->sheet().grid;if(g<=0||g*pixelsPerMm<7)return;
    const QRectF visible=area.intersected(QRectF(toSheet(QPointF(0,0)),toSheet(QPointF(width(),height()))));
    if(visible.isEmpty()||visible.width()/g*visible.height()/g>400000)return;
    const int level=int(std::lround(255*(1-std::clamp(gridContrast,0,100)/100.)));
    const QColor dot(level,level,level),mark(std::max(level,128),std::max(level,128),std::max(level,128));
    const long x0=long(std::ceil(visible.left()/g-1e-9)),x1=long(std::floor(visible.right()/g+1e-9)),y0=long(std::ceil(visible.top()/g-1e-9)),y1=long(std::floor(visible.bottom()/g+1e-9));
    auto marked=[&](long i,long j){return gridMarks>1&&i%gridMarks==0&&j%gridMarks==0;};
    // Crisp, as the reference draws it: in device pixels, without smoothing.
    painter.save();painter.resetTransform();painter.setRenderHint(QPainter::Antialiasing,false);
    auto at=[&](long i,long j){const QPointF p=toPixel(QPointF(i*g,j*g));return QPoint(int(std::floor(p.x())),int(std::floor(p.y())));};
    if(gridLines){
        // Lines a little lighter than dots; every marked one as dark as a mark.
        const int l=std::min(255,level+(255-level)*3/4);const QColor light(l,l,l);
        const QPoint top=at(x0,y0),bottom=at(x1,y1);
        for(long i=x0;i<=x1;i++){const int x=at(i,y0).x();painter.setPen(gridMarks>1&&i%gridMarks==0?mark:light);painter.drawLine(x,top.y(),x,bottom.y());}
        for(long j=y0;j<=y1;j++){const int y=at(x0,j).y();painter.setPen(gridMarks>1&&j%gridMarks==0?mark:light);painter.drawLine(top.x(),y,bottom.x(),y);}
    }else{
        painter.setPen(dot);
        for(long i=x0;i<=x1;i++)for(long j=y0;j<=y1;j++)if(!marked(i,j))painter.drawPoint(at(i,j));
        if(gridMarks>1){
            painter.setPen(mark);
            for(long i=x0;i<=x1;i++)for(long j=y0;j<=y1;j++)if(marked(i,j)){const QPoint p=at(i,j);painter.drawLine(p.x()-2,p.y(),p.x()+2,p.y());painter.drawLine(p.x(),p.y()-2,p.x(),p.y()+2);}
        }
    }
    painter.restore();
}
void SheetView::paintEvent(QPaintEvent *){
    const Item *edited=editedComponent();
    QPainter painter(this);painter.fillRect(rect(),edited?componentDesk:desk);
    if(!doc||doc->sheets.isEmpty()){paintRulers(painter);return;}
    const int sheetIndex=doc->activeSheet;
    painter.save();painter.translate(offset);painter.scale(pixelsPerMm,pixelsPerMm);
    RenderOptions options;options.fileName=fileName;options.pins=edited!=nullptr;options.linkMarks=true;
    if(!whiteBackground)options.paperColour=QColor(255,255,250);   // sPlan's screen paper, measured in its window
    if(parentChildColours&&!edited){
        // The selected parents and children with their relatives.
        options.parentChild=true;
        for(const auto &p:allComponents(*doc)){
            if(!selected.contains(p.item->id))continue;
            const QString parent=p.item->parent?p.item->id:p.item->parentId;if(parent.isEmpty())continue;
            options.related.insert(parent);for(const auto &c:childrenOf(*doc,parent))options.related.insert(c.item->id);
        }
    }
    const Sheet &sheet=doc->sheet();
    if(edited){
        // The component in its own coordinates; designator and value show as placeholders.
        Item shown=*edited;shown.pos=QPointF();shown.rotation=0;shown.mirrored=false;
        shown.designator=QStringLiteral("[")+ui("Bezeichner")+QStringLiteral("]");shown.value=QStringLiteral("[")+ui("Wert")+QStringLiteral("]");
        shown.designatorVisible=shown.valueVisible=true;
        paintGrid(painter,sheetRect());
        // The placeholders without the drawing's prefix and sheet number (the lists stay shared, nothing is copied).
        Document plain=*doc;plain.designatorPageNumbers=false;
        paintItems(painter,{shown},plain,sheetIndex,options);
        // The insertion point ("roter Punkt"), dragged to another grid point to move it.
        painter.setPen(Qt::NoPen);painter.setBrush(QColor(255,0,0));painter.drawEllipse(drag==Drag::Anchor?anchorAt:QPointF(),4/pixelsPerMm,4/pixelsPerMm);
    }else{
        painter.fillRect(QRectF(QPointF(1.5,1.5),QSizeF(sheet.width,sheet.height)),QColor(90,90,90));
        if(titleMode){
            RenderOptions faint=options;faint.titleBlock=false;
            paintSheet(painter,*doc,sheetIndex,faint);
            painter.fillRect(QRectF(0,0,sheet.width,sheet.height),QColor(255,255,230,190));
            RenderOptions block=options;block.paper=false;block.circuit=false;paintSheet(painter,*doc,sheetIndex,block);
            paintGrid(painter,QRectF(0,0,sheet.width,sheet.height));
        }else{
            // Paper, the grid, the title block and the circuit; the grid over the title block on request.
            RenderOptions paper=options;paper.titleBlock=gridOverTitleBlock;paper.circuit=false;paintSheet(painter,*doc,sheetIndex,paper);
            paintGrid(painter,QRectF(0,0,sheet.width,sheet.height));
            RenderOptions rest=options;rest.paper=false;rest.titleBlock=!gridOverTitleBlock;paintSheet(painter,*doc,sheetIndex,rest);
        }
    }
    const double px=1/pixelsPerMm;
    // Guide lines across the view; the chosen one red.
    if(guidesShown()){
        const QRectF visible(toSheet(QPointF(0,0)),toSheet(QPointF(width(),height())));
        const Sheet &s=doc->sheet();
        auto guide=[&](bool vertical,int k,double at){
            const bool chosen=(chosenGuide==k&&chosenVertical==vertical)||(drag==Drag::Guide&&guideVertical==vertical&&guideIndex==k);
            painter.setPen(QPen(chosen?QColor(255,0,0):guideColour,px,Qt::DashLine));
            if(vertical)painter.drawLine(QPointF(at,visible.top()),QPointF(at,visible.bottom()));
            else painter.drawLine(QPointF(visible.left(),at),QPointF(visible.right(),at));
        };
        for(int k=0;k<s.verticalGuides.size();k++)guide(true,k,s.verticalGuides[k]);
        for(int k=0;k<s.horizontalGuides.size();k++)guide(false,k,s.horizontalGuides[k]);
        if(drag==Drag::Guide&&guideIndex<0)guide(guideVertical,-2,guideAt);
    }
    // An element of a group chosen alone: a dashed frame only.
    if(selected.size()==1&&!itemById(selected[0]))if(const Item *n=nestedItem(selected[0])){
        painter.setBrush(Qt::NoBrush);painter.setPen(QPen(markColour,px,Qt::DashLine));painter.drawRect(bounds(*n).adjusted(-2*px,-2*px,2*px,2*px));}
    // Selection: a dashed frame around each element, handles for a single one.
    for(const auto &i:items()){
        if(!selected.contains(i.id))continue;
        painter.setBrush(Qt::NoBrush);painter.setPen(QPen(markColour,px,Qt::DashLine));
        painter.drawRect(bounds(i).adjusted(-2*px,-2*px,2*px,2*px));
        if(selected.size()==1){
            painter.setPen(QPen(markColour,px));painter.setBrush(QColor(255,255,255));
            const auto list=handles(i);
            for(int k=0;k<list.size();k++){
                // Changers (rounding, arc angles) round, the other handles square.
                if(k>=4&&(i.type==ItemType::Rectangle||i.type==ItemType::Ellipse))painter.drawEllipse(list[k],3.5*px,3.5*px);
                else painter.drawRect(QRectF(list[k]-QPointF(3,3)*px,QSizeF(6,6)*px));
            }
            if(current==Tool::Select){painter.setPen(QPen(QColor(0,0,255),px));painter.setBrush(Qt::NoBrush);for(auto v:virtualNodes(i))painter.drawEllipse(v,3*px,3*px);}
            if(i.type==ItemType::Component){painter.setPen(Qt::NoPen);painter.setBrush(QColor(255,0,0));painter.drawEllipse(i.pos,3*px,3*px);}
        }
    }
    // What is being drawn.
    if(!drawing.isEmpty()&&current==Tool::Dimension){
        const Item d=dimensionDraft(drawing,snapped(pointer,QGuiApplication::keyboardModifiers()));
        if(d.points.size()>=2)paintItems(painter,{d},*doc,sheetIndex,options);
        else{painter.setPen(QPen(markColour,px,Qt::DashLine));painter.drawLine(drawing.first(),snapped(pointer,QGuiApplication::keyboardModifiers()));}
    }else if(!drawing.isEmpty()&&current==Tool::Bezier){
        // The control polygon to the pointer, and the curves that are complete.
        QPolygonF control=drawing;control<<snapped(pointer,QGuiApplication::keyboardModifiers());
        painter.setPen(QPen(markColour,px,Qt::DashLine));painter.setBrush(Qt::NoBrush);painter.drawPolyline(control);
        if(control.size()>=4){Item curve=linePreset;curve.type=ItemType::Bezier;curve.points=control;curve.points.resize((control.size()-1)/3*3+1);paintItems(painter,{curve},*doc,sheetIndex,options);}
    }else if(!drawing.isEmpty()&&current==Tool::Freehand){
        Item path=linePreset;path.type=ItemType::Line;path.points=drawing;path.points<<pointer;paintItems(painter,{path},*doc,sheetIndex,options);
    }else if(!drawing.isEmpty()){
        Item preview=linePreset;preview.type=current==Tool::Polygon?ItemType::Polygon:ItemType::Line;preview.points=drawing;
        preview.points<<snapped(pointer,QGuiApplication::keyboardModifiers(),&drawing.last());
        if(preview.type==ItemType::Polygon){preview.pen=shapePreset.pen;preview.fill=shapePreset.fill;}
        if(preview.points.size()>=(preview.type==ItemType::Polygon?3:2))paintItems(painter,{preview},*doc,sheetIndex,options);
        else{painter.setPen(QPen(QColor(0,0,0),px));painter.drawPolyline(preview.points);}
    }
    if(drag==Drag::Special)paintItems(painter,specialShape(special,pressSheet,lastSheet,specialOptions,linePreset,shapePreset,textPreset),*doc,sheetIndex,options);
    if(drag==Drag::Measure){
        // The distance measured, written beside the pointer.
        // In the sheet's scale.
        const QPointF d=(lastSheet-pressSheet)*doc->sheet().scale;const double length=std::hypot(d.x(),d.y());
        painter.setPen(QPen(QColor(255,0,0),px));painter.drawLine(pressSheet,lastSheet);
        painter.drawEllipse(pressSheet,3*px,3*px);painter.drawEllipse(lastSheet,3*px,3*px);
        const QString label=ui("Länge: %1 %5, dX: %2 %5, dY: %3 %5, Winkel: %4°").arg(uiLocale().toString(length,'f',2),uiLocale().toString(d.x(),'f',2),
            uiLocale().toString(-d.y(),'f',2),uiLocale().toString(qRadiansToDegrees(std::atan2(-d.y(),d.x())),'f',1)).arg(unitName(doc->sheet().scaleUnit));
        painter.save();painter.resetTransform();
        const QPointF at=toPixel(lastSheet)+QPointF(12,-12);QFontMetrics fm(painter.font());QRectF box=fm.boundingRect(label);box.moveTopLeft(at-QPointF(0,box.height()));
        painter.fillRect(box.adjusted(-3,-2,3,2),QColor(255,255,225));painter.setPen(QColor(0,0,0));painter.drawText(box,Qt::AlignLeft|Qt::AlignVCenter,label);
        painter.restore();
    }
    {// The sizers: small black and green boxes, or arrows for turning (corners) and shearing (sides).
        const auto list=sizers();const bool arrows=turning();
        for(int k=0;k<list.size();k++){
            const QPointF h=list[k];painter.setPen(QPen(QColor(0,0,0),px));
            if(!arrows){painter.setBrush(QColor(0,160,0));painter.drawRect(QRectF(h-QPointF(2.5,2.5)*px,QSizeF(5,5)*px));continue;}
            painter.setBrush(Qt::NoBrush);
            if(k%2==0)painter.drawArc(QRectF(h-QPointF(4,4)*px,QSizeF(8,8)*px),0,270*16);
            else if(k==1||k==5)painter.drawLine(h-QPointF(5,0)*px,h+QPointF(5,0)*px);
            else painter.drawLine(h-QPointF(0,5)*px,h+QPointF(0,5)*px);
        }
        painter.setBrush(Qt::NoBrush);}
    if(drag==Drag::Rubber||drag==Drag::Shape||(drag==Drag::Pan&&current==Tool::Zoom)){
        const QRectF frame=drag==Drag::Shape?shapeFrame(pressSheet,lastSheet,QGuiApplication::keyboardModifiers()):QRectF(pressSheet,lastSheet).normalized();
        painter.setPen(QPen(markColour,px,Qt::DashLine));painter.setBrush(Qt::NoBrush);painter.drawRect(frame);
        if(drag==Drag::Shape&&current==Tool::Ellipse)painter.drawEllipse(frame);
    }
    if(!floating.isEmpty()&&pointerInside){
        painter.save();painter.translate(snapped(pointer,QGuiApplication::keyboardModifiers()));paintItems(painter,floating,*doc,sheetIndex,options);painter.restore();
    }
    if(caughtTerminal){painter.setPen(QPen(QColor(255,0,0),2*px));painter.setBrush(Qt::NoBrush);painter.drawEllipse(caughtAt,6*px,6*px);}
    painter.restore();
    paintRulers(painter);
}
QImage SheetView::render(QSize size){
    QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);
    if(!doc||doc->sheets.isEmpty())return image;
    const Sheet &s=doc->sheet();const double scale=std::min(size.width()/s.width,size.height()/s.height);
    QPainter p(&image);p.scale(scale,scale);RenderOptions o;o.fileName=fileName;paintSheet(p,*doc,doc->activeSheet,o);
    return image;
}

// --- mouse and keys
void SheetView::startComponentTextMove(const QString &componentId,int text){
    const Item *c=itemById(componentId);if(!c||text<0||text>=c->children.size()||!isText(c->children[text]))return;
    // The text's own point is the origin: it moves by the pointer's grid steps from there.
    drag=Drag::ComponentText;dragId=componentId;dragNode=text;textStart=c->children[text].pos;before=items();
    pressSheet=placement(*c).map(textStart);changedDuringDrag=false;textFollows=true;
    setHint(ui("Bauteiltext verschieben: Klicken legt ihn ab, Esc bricht ab"));update();
}
void SheetView::mousePressEvent(QMouseEvent *event){
    setFocus();if(!doc)return;
    if(textFollows){
        // A component text moved from the context menu: put down with the left button, put back with any other.
        textFollows=false;drag=Drag::None;setHint({});
        if(event->button()==Qt::LeftButton){before.clear();finishChange();}
        else{items()=before;before.clear();cancelChange();}
        update();return;
    }
    // The origin button between the rulers.
    if(event->position().x()<rulerSize&&event->position().y()<rulerSize){
        auto *menu=new QMenu(this);menu->setObjectName("originMenu");menu->setAttribute(Qt::WA_DeleteOnClose);auto *group=new QActionGroup(menu);
        const std::pair<const char*,Origin> corners[]={{"Oben links",Origin::TopLeft},{"Unten links",Origin::BottomLeft},{"Oben rechts",Origin::TopRight},{"Unten rechts",Origin::BottomRight}};
        for(const auto &[name,corner]:corners){
            auto *a=menu->addAction(ui(name));a->setCheckable(true);a->setChecked(origin==corner);group->addAction(a);
            connect(a,&QAction::triggered,this,[this,c=corner]{origin=c;update();if(pointerMoved)pointerMoved(toSheet(mapFromGlobal(QCursor::pos())));});
        }
        menu->popup(mapToGlobal(QPoint(0,rulerSize)));return;
    }
    const QPointF mm=toSheet(event->position());const auto mods=event->modifiers();
    pressPixel=event->position();pressSheet=mm;lastSheet=mm;changedDuringDrag=false;
    if(event->button()==Qt::MiddleButton){drag=Drag::Pan;setCursor(Qt::ClosedHandCursor);return;}
    if(event->button()==Qt::RightButton){
        if(!floating.isEmpty()){floating.clear();setHint({});update();return;}
        if(!drawing.isEmpty()&&current==Tool::Dimension){drawing.clear();update();return;}   // cancelled
        if(!drawing.isEmpty()){
            if(current==Tool::Line)drawing<<snapped(mm,mods,&drawing.last());
            finishDrawing(current==Tool::Polygon);return;
        }
        // In the zoom mode the right button zooms out, as in the reference; other modes end with it.
        if(current==Tool::Zoom){zoomAt(1/clickStep,event->position());return;}
        if(current!=Tool::Select){setTool(Tool::Select);return;}
        const int index=hit(mm);QString id;int node=-1;contextComponentText=-1;
        if(index>=0){
            id=items()[index].id;
            if(!editedComponent())contextComponentText=componentTextAt(items()[index],mm);
            if(!selected.contains(id)){selected={id};if(selectionChanged)selectionChanged();update();}
            node=handleAt(items()[index],event->position());
        }else if(!selected.isEmpty()){selected.clear();if(selectionChanged)selectionChanged();update();}
        if(contextMenuRequested)contextMenuRequested(event->globalPosition().toPoint(),id,node);
        return;
    }
    if(event->button()!=Qt::LeftButton)return;
    if(chosenGuide>=0){chosenGuide=-1;update();if(selectionChanged)selectionChanged();}
    // A new guide line dragged from a ruler.
    if(floating.isEmpty()&&drawing.isEmpty()&&(event->position().x()<rulerSize)!=(event->position().y()<rulerSize)){
        if(guidesShown()&&!guidesFixed){drag=Drag::Guide;guideIndex=-1;guideVertical=event->position().x()<rulerSize;guideAt=guideVertical?mm.x():mm.y();update();}
        return;
    }
    if(!floating.isEmpty()){
        const QPointF at=snapped(mm,mods);QStringList ids;
        startChange();
        QList<Item> placed=floating;freshIds(placed);
        for(auto i:placed){schematic::move(i,at);items().append(i);ids<<i.id;}
        finishChange();floating.clear();setHint({});
        selected=ids;if(selectionChanged)selectionChanged();
        if(floatingPlaced){auto f=std::move(floatingPlaced);floatingPlaced=nullptr;f(ids);}
        update();return;
    }
    switch(current){
    case Tool::Select:{
        if(editedComponent()&&distance(event->position(),offset)<6){drag=Drag::Anchor;anchorAt=QPointF();setHint(ui("Ankerpunkt: Bestimmt den Rasterpunkt des Bauteiles"));return;}
        const auto pressSizer=[&]{
            const auto list=sizers();
            for(int k=0;k<list.size();k++)if(distance(toPixel(list[k]),event->position())<=5){
                drag=Drag::Sizer;sizer=k;sizerFrame=QRectF(list[0],list[4]).normalized();before=items();return true;}
            return false;};
        if(turning()&&pressSizer())return;
        if(selected.size()==1)if(Item *s=itemById(selected[0])){
            const int h=handleAt(*s,event->position());
            if(h>=0){
                drag=(s->type==ItemType::Line||s->type==ItemType::Polygon||s->type==ItemType::Bezier||s->type==ItemType::Dimension)?Drag::Node:Drag::Corner;
                if(h>=4&&s->type==ItemType::Rectangle){
                    drag=Drag::Rounding;const double r=std::clamp(s->corner,0.,50.)/100*std::min(std::abs(s->size.width()),std::abs(s->size.height()));
                    roundingGrab=roundingShown(r,s->size.width())-r;}
                if(h>=4&&s->type==ItemType::Ellipse)drag=Drag::ArcAngle;
                dragId=s->id;dragNode=h;insertNode=false;before=items();return;}
            // Alt on a segment of a selected line or polygon moves that segment alone, as in the reference.
            if((mods&Qt::AltModifier)&&(s->type==ItemType::Line||s->type==ItemType::Polygon)&&s->points.size()>=2){
                const int n=int(s->points.size()),segments=s->type==ItemType::Polygon?n:n-1;const double reach=4/pixelsPerMm;
                for(int k=0;k<segments;k++){
                    const QPointF a=s->points[k],b=s->points[(k+1)%n],d=b-a;const double l2=QPointF::dotProduct(d,d);
                    const double t=l2>0?std::clamp(QPointF::dotProduct(mm-a,d)/l2,0.,1.):0;const QPointF q=a+t*d;
                    if(std::hypot(mm.x()-q.x(),mm.y()-q.y())<=reach){drag=Drag::Segment;dragId=s->id;dragNode=k;segmentFrom=a;segmentTo=b;before=items();return;}
                }
            }
            // A virtual node: the node after the segment's start, once moved.
            const int v=virtualNodeAt(*s,event->position());
            if(v>=0){drag=Drag::Node;dragId=s->id;dragNode=v+1;insertNode=true;before=items();return;}
        }
        if(pressSizer())return;
        const int index=hit(mm);
        if(index<0){
            // A guide line: chosen, and moved while dragged.
            bool vertical=false;const int g=guideAtPixel(event->position(),&vertical);
            if(g>=0){
                chooseGuide(g,vertical);
                if(!guidesFixed){drag=Drag::Guide;guideIndex=g;guideVertical=vertical;guideAt=vertical?doc->sheet().verticalGuides[g]:doc->sheet().horizontalGuides[g];}
                update();return;
            }
            if(!(mods&(Qt::ShiftModifier|Qt::ControlModifier))&&!selected.isEmpty()){selected.clear();if(selectionChanged)selectionChanged();}
            drag=Drag::Rubber;update();return;
        }
        const Item &item=items()[index];
        // Alt on an element of a group chooses that element alone (in nested groups the innermost one), as in the
        // reference: its properties then change without ungrouping.
        if((mods&Qt::AltModifier)&&!(mods&(Qt::ShiftModifier|Qt::ControlModifier))&&item.type==ItemType::Group){
            const Item *inner=&item;
            while(inner->type==ItemType::Group){
                const Item *next=nullptr;for(int k=int(inner->children.size())-1;k>=0&&!next;k--)if(hits(inner->children[k],mm))next=&inner->children[k];
                if(!next)break;inner=next;
            }
            if(inner!=&item){selected={inner->id};if(selectionChanged)selectionChanged();update();return;}
        }
        // A text of a component that is not selected: dragged on its own (a click alone selects the component).
        const Qt::KeyboardModifiers held=mods&(Qt::ShiftModifier|Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier);
        if(!editedComponent()&&item.type==ItemType::Component&&!selected.contains(item.id)
           &&(componentTextsWithKey?held==componentTextKey:!(mods&(Qt::ShiftModifier|Qt::ControlModifier)))){
            const int t=componentTextAt(item,mm);
            if(t>=0){drag=Drag::ComponentText;dragId=item.id;dragNode=t;textStart=item.children[t].pos;before=items();return;}
        }
        if(mods&(Qt::ShiftModifier|Qt::ControlModifier)){
            if(selected.contains(item.id))selected.removeAll(item.id);else selected<<item.id;
            if(selectionChanged)selectionChanged();update();return;
        }
        clickedSelected=selected.contains(item.id);
        if(!selected.contains(item.id)){selected={item.id};if(selectionChanged)selectionChanged();}
        // The point that snaps while moving: a component's insertion point, the nearest node of a line, a corner.
        anchor=bounds(item).topLeft();
        if(item.type==ItemType::Component||isText(item)||item.type==ItemType::Junction)anchor=item.pos;
        else if(!item.points.isEmpty()){anchor=item.points[0];for(auto p:item.points)if(distance(p,mm)<distance(anchor,mm))anchor=p;}
        else if(item.type==ItemType::Rectangle||item.type==ItemType::Ellipse||item.type==ItemType::TextBox||item.type==ItemType::Image)anchor=item.centre;
        drag=Drag::Move;before=items();moved=QPointF();update();return;
    }
    case Tool::Zoom:
        if(mods&Qt::ShiftModifier){zoomAt(1/clickStep,event->position());return;}
        drag=Drag::Pan;return;
    case Tool::Line:case Tool::Polygon:{
        const QPointF p=drawing.isEmpty()?snapped(mm,mods):snapped(mm,mods,&drawing.last());
        if(!drawing.isEmpty()&&distance(p,drawing.last())<1e-9&&event->type()!=QEvent::MouseButtonDblClick)return;
        drawing<<p;update();return;
    }
    case Tool::Bezier:drawing<<snapped(mm,mods);update();return;
    case Tool::Dimension:{
        drawing<<snapped(mm,mods);
        const int needed=dimensionPreset.dimension==DimensionKind::Radial||dimensionPreset.dimension==DimensionKind::Diameter?2:3;
        if(drawing.size()>=needed){
            Item d=dimensionDraft(QPolygonF(drawing.mid(0,needed-1)),drawing[needed-1]);drawing.clear();
            if(d.points.size()>=2&&QLineF(d.points[0],d.dimension==DimensionKind::Angle?d.points[2]:d.points[1]).length()>1e-6)addItem(d);
        }
        update();return;
    }
    case Tool::Freehand:drawing={snapped(mm,mods)};drag=Drag::Freehand;update();return;
    case Tool::Measure:pressSheet=snapped(mm,mods);lastSheet=pressSheet;drag=Drag::Measure;update();return;
    case Tool::Special:pressSheet=snapped(mm,mods);lastSheet=pressSheet;drag=Drag::Special;return;
    case Tool::Junction:{Item j=junctionPreset;j.pos=snapped(mm,mods);addItem(j);return;}
    case Tool::Rectangle:case Tool::Ellipse:case Tool::TextBox:
        pressSheet=snapped(mm,mods);lastSheet=pressSheet;drag=Drag::Shape;return;
    case Tool::Text:case Tool::NetLabel:case Tool::SheetReference:case Tool::Contact:{
        Item t=current==Tool::Text?textPreset:current==Tool::Contact?contactPreset:labelPreset;t.pos=snapped(mm,mods);
        if(current==Tool::Text)t.type=ItemType::Text;
        else if(current==Tool::Contact){t.type=ItemType::Contact;t.pin=t.pos;t.hasPin=true;t.pos+=QPointF(.5,-t.font.height*1.4);}
        else{t.type=ItemType::NetLabel;t.global=current==Tool::SheetReference;}
        if(textRequested&&!textRequested(t))return;
        addItem(t);return;
    }
    }
}
void SheetView::mouseMoveEvent(QMouseEvent *event){
    const QPointF mm=toSheet(event->position());const auto mods=event->modifiers();
    pointer=mm;pointerInside=true;
    caughtTerminal=false;
    const bool catching=drag==Drag::Move||drag==Drag::Node||!floating.isEmpty()||current==Tool::Line||current==Tool::Polygon||current==Tool::Junction||current==Tool::NetLabel||current==Tool::SheetReference;
    if(catching&&terminalSnap&&!(mods&Qt::ShiftModifier)&&doc)caughtTerminal=terminalAt(mm,&caughtAt);
    if(pointerMoved)pointerMoved(doc?snapped(mm,mods):mm);
    // Over the arrow of a text link a small window names its target or the texts linking here.
    if(drag==Drag::None&&current==Tool::Select){
        bool outgoing=false;const QString id=linkArrowAt(mm,&outgoing);
        if(!id.isEmpty()){QToolTip::showText(event->globalPosition().toPoint(),linkWindowText(id,outgoing),this);linkTipShown=true;}
        else if(linkTipShown){QToolTip::hideText();linkTipShown=false;}
    }
    switch(drag){
    case Drag::Anchor:anchorAt=snapped(mm,mods);update();return;
    case Drag::Guide:{
        guideAt=guideVertical?mm.x():mm.y();
        if(guideIndex>=0&&guideIndex<guides(guideVertical).size()&&guides(guideVertical)[guideIndex]!=guideAt){startChange();guides(guideVertical)[guideIndex]=guideAt;}
        update();return;
    }
    case Drag::Pan:
        if(current==Tool::Zoom&&event->buttons()&Qt::LeftButton){lastSheet=mm;update();return;}
        offset+=event->position()-pressPixel;pressPixel=event->position();fitted=true;update();return;
    case Drag::Move:{
        const QPointF delta=snapped(anchor+(mm-pressSheet),mods)-anchor;
        if(delta==moved){update();return;}
        if(distance(event->position(),pressPixel)<3&&moved.isNull())return;
        startChange();items()=before;
        applyMove(items(),selected,delta,rubberBand&&!titleMode&&editedId.isEmpty(),[this](const Item &i){return connectionPoints(i);});
        moved=delta;update();return;
    }
    case Drag::ComponentText:{
        // Moved in grid steps, in the component's own coordinates.
        const Item *was=nullptr;for(const auto &b:std::as_const(items()))if(b.id==dragId)was=&b;if(!was||dragNode>=was->children.size())return;
        const QTransform back=placement(*was).inverted();
        const QPointF delta=back.map(snapped(mm,mods))-back.map(snapped(pressSheet,mods)),p=textStart+delta;
        if(distance(p,was->children[dragNode].pos)<1e-12)return;
        startChange();if(Item *i=itemById(dragId))i->children[dragNode].pos=p;update();return;
    }
    case Drag::Segment:{
        const QPointF delta=snapped(segmentFrom+(mm-pressSheet),mods)-segmentFrom;
        startChange();
        if(Item *i=itemById(dragId))if(dragNode<i->points.size()){const int n=int(i->points.size());i->points[dragNode]=segmentFrom+delta;i->points[(dragNode+1)%n]=segmentTo+delta;}
        update();return;
    }
    case Drag::Node:{
        // The element is looked up after startChange(): the undo step shares the lists until they change.
        const Item *was=nullptr;for(const auto &b:std::as_const(items()))if(b.id==dragId)was=&b;if(!was)return;
        if(was->type==ItemType::Dimension&&dragNode==was->points.size()&&was->points.size()>=2){
            // The middle of the dimension line: its distance from the measured points.
            const QPointF d=was->points[1]-was->points[0];const double l=std::hypot(d.x(),d.y());
            const QPointF n=l>1e-9?QPointF(d.y(),-d.x())/l:QPointF(0,-1);
            const double offset=QPointF::dotProduct(snapped(mm,mods)-was->points[0],n);if(std::abs(offset-was->offset)<1e-12)return;
            startChange();if(Item *i=itemById(dragId))i->offset=offset;update();return;
        }
        if(insertNode){
            if(dragNode<1||dragNode>was->points.size())return;
            const QPointF from=was->points[dragNode-1],p=snapped(mm,mods,&from);
            startChange();if(Item *i=itemById(dragId))i->points.insert(dragNode,p);insertNode=false;update();return;
        }
        if(dragNode>=was->points.size())return;
        const QPointF from=dragNode>0?was->points[dragNode-1]:was->points.size()>1?was->points[1]:QPointF();
        const QPointF p=snapped(mm,mods,dragNode>0||was->points.size()>1?&from:nullptr);if(p==was->points[dragNode])return;
        startChange();if(Item *i=itemById(dragId))i->points[dragNode]=p;update();return;
    }
    case Drag::Sizer:{
        const QRectF r=sizerFrame;const QPointF c=r.center();
        const QPointF points[8]={r.topLeft(),QPointF(c.x(),r.top()),r.topRight(),QPointF(r.right(),c.y()),r.bottomRight(),QPointF(c.x(),r.bottom()),r.bottomLeft(),QPointF(r.left(),c.y())};
        const QPointF from=points[sizer],anchor=points[(sizer+4)%8];
        startChange();
        // Each time from the elements as they were.
        for(const auto &b:std::as_const(before))if(selected.contains(b.id))if(Item *i=itemById(b.id))*i=b;
        const bool corner=sizer%2==0;
        if(!turning()){
            // Stretched about the opposite sizer; corners both ways, the sides one way.
            const QPointF p=snapped(mm,mods);double sx=1,sy=1;
            if((corner||sizer==3||sizer==7)&&std::abs(from.x()-anchor.x())>1e-9)sx=std::max(.01,(p.x()-anchor.x())/(from.x()-anchor.x()));
            if((corner||sizer==1||sizer==5)&&std::abs(from.y()-anchor.y())>1e-9)sy=std::max(.01,(p.y()-anchor.y())/(from.y()-anchor.y()));
            for(auto &i:items())if(selected.contains(i.id))stretch(i,anchor,sx,sy);
        }else if(corner){
            // Turned about the middle, in steps of the rotation snap (Ctrl turns freely).
            const double screen=qRadiansToDegrees(std::atan2(mm.y()-c.y(),mm.x()-c.x())-std::atan2(pressSheet.y()-c.y(),pressSheet.x()-c.x()));
            double degrees=-screen;   // clockwise on the screen is negative for the model
            if(rotationSnap>0&&!(mods&Qt::ControlModifier))degrees=std::round(degrees/rotationSnap)*rotationSnap;
            for(auto &i:items())if(selected.contains(i.id))rotate(i,c,degrees);
        }else{
            // Sheared along the side, the opposite side staying.
            const QPointF p=snapped(mm,mods);double kx=0,ky=0;
            if(sizer==1||sizer==5){if(std::abs(from.y()-anchor.y())>1e-9)kx=(p.x()-from.x())/(from.y()-anchor.y());}
            else if(std::abs(from.x()-anchor.x())>1e-9)ky=(p.y()-from.y())/(from.x()-anchor.x());
            for(auto &i:items())if(selected.contains(i.id))stretch(i,anchor,1,1,kx,ky);
        }
        update();return;
    }
    case Drag::Rounding:case Drag::ArcAngle:{
        const Item *original=nullptr;for(const auto &b:before)if(b.id==dragId)original=&b;if(!original)return;
        const QPointF local=QTransform().translate(original->centre.x(),original->centre.y()).rotate(-original->rotation).inverted().map(mm);
        startChange();Item *i=itemById(dragId);if(!i)return;
        if(drag==Drag::Rounding){
            // The distance of the changer from the left corner is the rounding, up to half the shorter side.
            const double w=std::abs(original->size.width()),h=std::abs(original->size.height()),shorter=std::min(w,h);
            if(shorter>0){i->corner=std::clamp((local.x()+w/2-roundingGrab)/shorter*100,0.,50.);if(i->corners==Corners::Square&&i->corner>0)i->corners=Corners::Round;}
        }else{
            // The true angle of the pointer from the centre, counter-clockwise, in whole degrees.
            double a=std::round(qRadiansToDegrees(std::atan2(-local.y(),local.x())));if(a<0)a+=360;
            (dragNode==4?i->start:i->stop)=a;
        }
        update();return;
    }
    case Drag::Corner:{
        const Item *original=nullptr;for(const auto &b:before)if(b.id==dragId)original=&b;if(!original)return;
        // The opposite corner stays; the box is resized in its own frame.
        const auto c=corners(*original);const QPointF fixed=c[(dragNode+2)%4],p=snapped(mm,mods);
        QTransform frame;frame.rotate(-original->rotation);const QTransform back=frame.inverted();
        const QPointF a=back.map(fixed),b=back.map(p);
        startChange();if(Item *i=itemById(dragId)){i->size=QSizeF(std::abs(b.x()-a.x()),std::abs(b.y()-a.y()));i->centre=frame.map((a+b)/2);}
        update();return;
    }
    case Drag::Rubber:case Drag::Shape:
        lastSheet=drag==Drag::Shape?snapped(mm,mods):mm;update();return;
    case Drag::Measure:case Drag::Special:lastSheet=snapped(mm,mods);update();return;
    case Drag::Freehand:
        // A node whenever the pointer has gone a few pixels.
        if(drawing.isEmpty()||distance(toPixel(drawing.last()),event->position())>=3)drawing<<mm;
        update();return;
    case Drag::None:break;
    }
    if(current==Tool::Select&&floating.isEmpty()&&drawing.isEmpty()){
        bool vertical=false;const bool onGuide=!guidesFixed&&hit(mm)<0&&guideAtPixel(event->position(),&vertical)>=0;
        setCursor(onGuide?(vertical?Qt::SplitHCursor:Qt::SplitVCursor):Qt::ArrowCursor);
    }
    if(!drawing.isEmpty()||!floating.isEmpty()||current!=Tool::Select||caughtTerminal)update();
    else update(QRect(0,0,width(),rulerSize)),update(QRect(0,0,rulerSize,height()));
}
void SheetView::mouseReleaseEvent(QMouseEvent *event){
    if(textFollows)return;
    const Drag was=drag;drag=Drag::None;
    if(current==Tool::Select||current==Tool::Zoom)setCursor(current==Tool::Zoom?openLochCursor("lupe"):Qt::ArrowCursor);
    switch(was){
    case Drag::Anchor:
        setHint({});
        if(Item *c=editedComponent();c&&anchorAt!=QPointF()){
            // The parts stay where they are on the sheet; the component's own coordinates start at the new point.
            startChange();
            const QPointF at=placement(*c).map(anchorAt);
            for(auto &k:c->children)schematic::move(k,-anchorAt);
            c->pos=at;finishChange();
        }
        update();break;
    case Drag::Move:
        // A click on an element that was selected already turns the sizers into arrows and back, as in the reference.
        if(clickedSelected&&moved.isNull()){turnMode=!turning();turnFor=selected;}
        before.clear();finishChange();break;
    case Drag::Node:case Drag::Corner:case Drag::Segment:case Drag::Rounding:case Drag::ArcAngle:case Drag::Sizer:before.clear();finishChange();break;
    case Drag::ComponentText:
        before.clear();
        if(changedDuringDrag)finishChange();else{selected={dragId};if(selectionChanged)selectionChanged();}
        break;
    case Drag::Freehand:{
        QPolygonF nodes=drawing;drawing.clear();nodes<<toSheet(event->position());
        if(nodes.size()>=2&&distance(nodes.first(),nodes.last())+nodes.boundingRect().width()+nodes.boundingRect().height()>1e-6){
            Item l=linePreset;l.type=ItemType::Line;l.points=nodes;l.electrical=false;l.children.clear();addItem(l);
        }
        update();break;
    }
    case Drag::Measure:update();break;
    case Drag::Special:{
        const QRectF r=QRectF(pressSheet,lastSheet).normalized();
        if(r.width()*pixelsPerMm<2&&r.height()*pixelsPerMm<2){update();break;}
        QList<Item> made=specialShape(special,pressSheet,lastSheet,specialOptions,linePreset,shapePreset,textPreset);
        if(made.isEmpty()){update();break;}
        startChange();QStringList ids;for(auto &i:made){assignIds(i);items().append(i);ids<<i.id;}
        finishChange();selected=ids;if(selectionChanged)selectionChanged();update();break;
    }
    case Drag::Guide:{
        // Dropped onto a ruler, a guide line goes away; a new one comes onto the sheet.
        const bool onRuler=guideVertical?event->position().x()<rulerSize:event->position().y()<rulerSize;
        if(guideIndex<0){if(!onRuler){startChange();guides(guideVertical)<<guideAt;chooseGuide(int(guides(guideVertical).size())-1,guideVertical);}}
        else if(onRuler&&guideIndex<guides(guideVertical).size()){startChange();guides(guideVertical).removeAt(guideIndex);chosenGuide=-1;}
        finishChange();guideIndex=-1;update();break;
    }
    case Drag::Rubber:{
        const QRectF r=QRectF(pressSheet,lastSheet).normalized();
        if(r.width()*pixelsPerMm>2||r.height()*pixelsPerMm>2){
            // As in the reference: every element inside the frame or only partly inside it.
            QPainterPath frame;frame.addRect(r);
            for(const auto &i:items()){
                if(selected.contains(i.id))continue;
                const bool simple=i.type!=ItemType::Component&&i.type!=ItemType::Group&&i.type!=ItemType::Dimension;
                if(simple?shape(i).intersects(frame):r.intersects(bounds(i)))selected<<i.id;
            }
            if(selectionChanged)selectionChanged();
        }
        update();break;
    }
    case Drag::Shape:{
        QRectF r=shapeFrame(pressSheet,lastSheet,event->modifiers());
        if(r.width()<1e-6||r.height()<1e-6)r=QRectF(pressSheet,QSizeF(10,current==Tool::TextBox?5:10));
        Item s=current==Tool::TextBox?textBoxPreset:shapePreset;s.type=current==Tool::Ellipse?ItemType::Ellipse:current==Tool::TextBox?ItemType::TextBox:ItemType::Rectangle;
        s.centre=r.center();s.size=r.size();s.rotation=0;
        if(s.type==ItemType::TextBox){s.text=QString();if(textRequested&&!textRequested(s)){update();break;}}
        addItem(s);break;
    }
    case Drag::Pan:
        if(current==Tool::Zoom){
            const QRectF r=QRectF(pressSheet,toSheet(event->position())).normalized();
            if(r.width()*pixelsPerMm>8&&r.height()*pixelsPerMm>8){
                const double w=width()-rulerSize,h=height()-rulerSize;setScale(std::min(w/r.width(),h/r.height()));
                offset=QPointF(rulerSize+(w-r.width()*pixelsPerMm)/2-r.left()*pixelsPerMm,rulerSize+(h-r.height()*pixelsPerMm)/2-r.top()*pixelsPerMm);update();
            }else zoomCentred(clickStep,event->position());
        }
        break;
    case Drag::None:break;
    }
}
void SheetView::mouseDoubleClickEvent(QMouseEvent *event){
    if(event->button()!=Qt::LeftButton||!doc)return;
    // A double click on the arrow of a text link jumps to its target or to the text linking here.
    if(current==Tool::Select&&linkArrowActivated){bool outgoing=false;const QString id=linkArrowAt(toSheet(event->position()),&outgoing);if(!id.isEmpty()){linkArrowActivated(id,outgoing);return;}}
    if(current==Tool::Line||current==Tool::Polygon||current==Tool::Bezier){if(!drawing.isEmpty())finishDrawing(current==Tool::Polygon);return;}
    if(current==Tool::Dimension)return;
    if(current!=Tool::Select)return;
    const int index=hit(toSheet(event->position()));
    if(index>=0&&editRequested)editRequested(items()[index].id);
    // A double click on an empty spot pans while the button stays down, as in the reference.
    if(index<0){drag=Drag::Pan;pressPixel=event->position();setCursor(Qt::ClosedHandCursor);}
}
void SheetView::wheelEvent(QWheelEvent *event){
    // As in every drawing view of the suite: a touchpad (steps in pixels or scroll phases) pans; Ctrl (macOS Cmd) and
    // scrolling zooms about the pointer, with a mouse too; the wheel alone zooms.
    const QPoint pixels=event->pixelDelta();const bool control=event->modifiers()&Qt::ControlModifier;
    if((!pixels.isNull()||event->phase()!=Qt::NoScrollPhase)&&!control){offset+=pixels.isNull()?QPointF(event->angleDelta())/3:QPointF(pixels);fitted=true;update();return;}
    const double steps=pixels.isNull()?event->angleDelta().y()/120.0:pixels.y()/60.0;
    // As in the reference, zooming in with the wheel alone brings the point under the pointer to the middle.
    if(steps>0&&!control)zoomCentred(std::pow(wheelStep,steps),event->position());
    else if(steps!=0)zoomAt(std::pow(wheelStep,steps),event->position());
}
bool SheetView::event(QEvent *event){
    if(event->type()==QEvent::NativeGesture){
        const auto *gesture=static_cast<QNativeGestureEvent*>(event);
        if(gesture->gestureType()==Qt::ZoomNativeGesture){zoomAt(std::clamp(1+gesture->value(),.5,2.),gesture->position());return true;}
    }
    return QWidget::event(event);
}
void SheetView::keyPressEvent(QKeyEvent *event){
    if(!doc)return;
    switch(event->key()){
    case Qt::Key_Escape:
        if(!floating.isEmpty()){floating.clear();setHint({});update();return;}
        if(!drawing.isEmpty()){drawing.clear();update();return;}
        if(drag==Drag::Move||drag==Drag::Node||drag==Drag::Corner||drag==Drag::Segment||drag==Drag::Rounding||drag==Drag::ArcAngle||drag==Drag::Sizer||drag==Drag::ComponentText){
            if(textFollows){textFollows=false;setHint({});}
            items()=before;drag=Drag::None;cancelChange();return;}
        if(current!=Tool::Select){setTool(Tool::Select);return;}
        if(!selected.isEmpty()){selected.clear();if(selectionChanged)selectionChanged();update();}
        return;
    case Qt::Key_Return:case Qt::Key_Enter:if(!drawing.isEmpty())finishDrawing(current==Tool::Polygon);return;
    case Qt::Key_Space:if(!floating.isEmpty()){turnPlacement(90);return;}break;
    case Qt::Key_Tab:case Qt::Key_Backtab:{
        // The next element (the previous one with Shift) becomes the selection.
        const int n=int(items().size());if(!n)return;
        const bool back=event->key()==Qt::Key_Backtab||(event->modifiers()&Qt::ShiftModifier);
        int k=-1;if(selected.size()==1)for(int i=0;i<n;i++)if(items()[i].id==selected[0])k=i;
        k=k<0?(back?n-1:0):(k+(back?n-1:1))%n;
        selected={items()[k].id};if(selectionChanged)selectionChanged();update();return;
    }
    case Qt::Key_Left:case Qt::Key_Right:case Qt::Key_Up:case Qt::Key_Down:{
        if(selected.isEmpty())break;
        // One grid step; with Ctrl a tenth of a millimetre, with Shift ten grid steps.
        const double g=event->modifiers()&Qt::ControlModifier?.1:(doc->sheet().grid>0?doc->sheet().grid:1)*(event->modifiers()&Qt::ShiftModifier?10:1);
        const QPointF d=event->key()==Qt::Key_Left?QPointF(-g,0):event->key()==Qt::Key_Right?QPointF(g,0):event->key()==Qt::Key_Up?QPointF(0,-g):QPointF(0,g);
        moveSelection(d);return;
    }
    default:break;
    }
    QWidget::keyPressEvent(event);
}
void SheetView::resizeEvent(QResizeEvent *){if(!fitted)fitSheet();}
bool SheetView::focusNextPrevChild(bool){return false;}
void SheetView::leaveEvent(QEvent *){pointerInside=false;caughtTerminal=false;update();}
void SheetView::dragEnterEvent(QDragEnterEvent *event){if(event->mimeData()->hasFormat("application/x-openloch-schematic-symbol"))event->acceptProposedAction();}
void SheetView::dragMoveEvent(QDragMoveEvent *event){
    if(!event->mimeData()->hasFormat("application/x-openloch-schematic-symbol"))return;
    pointer=toSheet(event->position());pointerInside=true;event->acceptProposedAction();
}
void SheetView::dropEvent(QDropEvent *event){
    const QString key=QString::fromUtf8(event->mimeData()->data("application/x-openloch-schematic-symbol"));
    QList<Item> list;if(!libraryItems||!libraryItems(key,&list)||list.isEmpty())return;
    // At the drop point in one step; a parent's children keep their places beside it and stay linked to it.
    const QPointF at=snapped(toSheet(event->position()),event->modifiers());freshIds(list);
    startChange();QStringList ids;for(auto &i:list){schematic::move(i,at);items().append(i);ids<<i.id;}finishChange();
    selected=ids;if(selectionChanged)selectionChanged();
    event->acceptProposedAction();setFocus();update();
}
}
