#include "panelview.h"
#include "panelgeometry.h"
#include "panelgenerators.h"
#include "panelicons.h"
#include "language.h"
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace openloch::frontpanel {
namespace {
constexpr int rulerSize=20,margin=24;
constexpr double handleSize=7;
Element *findElement(QList<Element> &list,const QString &id){
    for(auto &e:list){if(e.id==id)return &e;if(Element *inside=findElement(e.children,id))return inside;}
    return nullptr;
}
// The geometry without pen width: where a symbol's corner or a snapped point really is.
QRectF shapeBounds(const QList<Element> &list){
    QRectF r;for(const auto &e:list){const QRectF b=e.isContainer()?shapeBounds(e.children):elementPath(e).boundingRect();if(b.isNull())continue;r=r.isNull()?b:r.united(b);}
    return r;
}
}

// Rulers along the top and left edge, in mm or inch from the panel's origin.
class Ruler : public QWidget {
public:
    Ruler(PanelView *view,Qt::Orientation orientation):QWidget(view),view(view),orientation(orientation){setMouseTracking(true);}
    double mark=std::numeric_limits<double>::quiet_NaN();
protected:
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);p.fillRect(rect(),palette().color(QPalette::Button));p.setPen(palette().color(QPalette::ButtonText));
        if(!view->doc)return;
        const frontpanel::Panel &panel=view->doc->panel();const bool horizontal=orientation==Qt::Horizontal;
        const double unit=panel.inch?25.4:1,origin=horizontal?panel.origin.x():panel.origin.y();
        // Steps of 1, 2, 5 × 10^n units so that labels have at least 50 pixels between them.
        double step=1;const double pixels=view->scale*unit;while(step*pixels<50)step*=step>=1&&QString::number(step).startsWith('2')?2.5:2;
        while(step*pixels>120&&step>1e-3)step/=2;
        const QPointF a=view->toPanel(QPointF(0,0)),b=view->toPanel(QPointF(view->viewport()->width(),view->viewport()->height()));
        const double from=((horizontal?a.x():a.y())-origin)/unit,to=((horizontal?b.x():b.y())-origin)/unit;
        QFont f=font();f.setPixelSize(9);p.setFont(f);
        const int length=horizontal?width():height();
        for(double v=std::floor(from/step)*step;v<=to;v+=step){
            for(int k=0;k<10;k++){
                const double w=v+k*step/10;const double at=horizontal?view->toView(QPointF(origin+w*unit,0)).x():view->toView(QPointF(0,origin+w*unit)).y();
                if(at<0||at>length)continue;const int tick=k==0?rulerSize-2:k==5?7:4;
                if(horizontal)p.drawLine(QPointF(at,rulerSize),QPointF(at,rulerSize-tick));else p.drawLine(QPointF(rulerSize,at),QPointF(rulerSize-tick,at));
                if(k==0){const QString label=uiLocale().toString(v,'g',6);
                    if(horizontal)p.drawText(QPointF(at+2,9),label);else{p.save();p.translate(9,at-2);p.rotate(-90);p.drawText(QPointF(0,0),label);p.restore();}}
            }
        }
        if(!std::isnan(mark)){const double at=horizontal?view->toView(QPointF(mark,0)).x():view->toView(QPointF(0,mark)).y();p.setPen(Qt::red);
            if(horizontal)p.drawLine(QPointF(at,0),QPointF(at,rulerSize));else p.drawLine(QPointF(0,at),QPointF(rulerSize,at));}
    }
private:
    PanelView *view;Qt::Orientation orientation;
};

PanelView::PanelView(QWidget *parent):QAbstractScrollArea(parent){
    setViewportMargins(rulerSize,rulerSize,0,0);viewport()->setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);
    top=new Ruler(this,Qt::Horizontal);left=new Ruler(this,Qt::Vertical);
    unitButton=new QToolButton(this);unitButton->setText("mm");unitButton->setAutoRaise(true);unitButton->setToolTip(ui("Einheit der Lineale: mm oder inch"));
    QFont f=unitButton->font();f.setPixelSize(9);unitButton->setFont(f);
    connect(unitButton,&QToolButton::clicked,this,[this]{setInch(!inch());});
    scroller=new QTimer(this);scroller->setInterval(30);connect(scroller,&QTimer::timeout,this,[this]{autoScroll();});
}
void PanelView::autoScroll(){
    constexpr double edge=14,step=12;const QSizeF size=viewport()->size();
    const double dx=lastView.x()<edge?-step:lastView.x()>size.width()-edge?step:0,dy=lastView.y()<edge?-step:lastView.y()>size.height()-edge?step:0;
    const int h=horizontalScrollBar()->value(),v=verticalScrollBar()->value();
    horizontalScrollBar()->setValue(h+int(dx));verticalScrollBar()->setValue(v+int(dy));
    if(horizontalScrollBar()->value()==h&&verticalScrollBar()->value()==v){scroller->stop();return;}
    cursor=toPanel(lastView);top->mark=cursor.x();left->mark=cursor.y();
    if(drag==Move||drag==Stretch||drag==Turn||drag==NodeDrag)applyDrag(cursor,lastModifiers);
    viewport()->update();top->update();left->update();
}
void PanelView::setInch(bool inch){
    frontpanel::Panel *p=panel();if(!p||p->inch==inch)return;
    if(beforeChange)beforeChange();p->inch=inch;if(afterChange)afterChange();
    refresh();
}
frontpanel::Panel *PanelView::panel() const{return doc&&!doc->panels.isEmpty()?&doc->panel():nullptr;}
void PanelView::setDocument(Document *document){doc=document;selected.clear();points.clear();placement.clear();if(current==Place)current=Select;refresh();fitPanel();}
void PanelView::refresh(){
    if(doc){QStringList kept;for(const auto &id:selected)if(findElement(doc->panel().elements,id))kept<<id;if(kept!=selected){selected=kept;if(selectionChanged)selectionChanged();}}
    unitButton->setText(inch()?"inch":"mm");updateScrollBars();viewport()->update();top->update();left->update();
}

// ------------------------------------------------------------------ coordinates, zoom and scrolling
QPointF PanelView::origin() const{
    const frontpanel::Panel *p=panel();if(!p)return {};
    const double w=p->width*scale+2*margin,h=p->height*scale+2*margin;
    const double x=w<=viewport()->width()?(viewport()->width()-w)/2:-horizontalScrollBar()->value();
    const double y=h<=viewport()->height()?(viewport()->height()-h)/2:-verticalScrollBar()->value();
    return {x+margin,y+margin};
}
QPointF PanelView::toPanel(QPointF v) const{const QPointF o=origin();return {(v.x()-o.x())/scale,(v.y()-o.y())/scale};}
QPointF PanelView::toView(QPointF p) const{const QPointF o=origin();return {o.x()+p.x()*scale,o.y()+p.y()*scale};}
QPointF PanelView::snap(QPointF p,bool free) const{
    const frontpanel::Panel *pl=panel();if(!pl||free||!pl->snap||pl->grid<=0)return p;
    const QPointF o=pl->origin;return {o.x()+std::round((p.x()-o.x())/pl->grid)*pl->grid,o.y()+std::round((p.y()-o.y())/pl->grid)*pl->grid};
}
void PanelView::updateScrollBars(){
    const frontpanel::Panel *p=panel();if(!p)return;
    const int w=int(std::ceil(p->width*scale+2*margin)),h=int(std::ceil(p->height*scale+2*margin));
    horizontalScrollBar()->setRange(0,std::max(0,w-viewport()->width()));horizontalScrollBar()->setPageStep(viewport()->width());horizontalScrollBar()->setSingleStep(20);
    verticalScrollBar()->setRange(0,std::max(0,h-viewport()->height()));verticalScrollBar()->setPageStep(viewport()->height());verticalScrollBar()->setSingleStep(20);
}
void PanelView::setZoom(double pixelsPerMm,std::optional<QPointF> keep){
    const QPointF view=keep?*keep:QPointF(viewport()->width()/2.0,viewport()->height()/2.0);const QPointF at=toPanel(view);
    scale=std::clamp(pixelsPerMm,0.05,400.0);updateScrollBars();
    const QPointF now=toView(at);horizontalScrollBar()->setValue(horizontalScrollBar()->value()+int(std::lround(now.x()-view.x())));verticalScrollBar()->setValue(verticalScrollBar()->value()+int(std::lround(now.y()-view.y())));
    viewport()->update();top->update();left->update();if(zoomChanged)zoomChanged();
}
void PanelView::zoomBy(double factor){setZoom(scale*factor);}
void PanelView::showArea(const QRectF &area){
    if(area.isEmpty()||!panel())return;
    const double s=std::min((viewport()->width()-2.0*margin)/area.width(),(viewport()->height()-2.0*margin)/area.height());
    scale=std::clamp(s,0.05,400.0);updateScrollBars();
    const QPointF c=toView(area.center());horizontalScrollBar()->setValue(horizontalScrollBar()->value()+int(std::lround(c.x()-viewport()->width()/2.0)));
    verticalScrollBar()->setValue(verticalScrollBar()->value()+int(std::lround(c.y()-viewport()->height()/2.0)));
    viewport()->update();top->update();left->update();if(zoomChanged)zoomChanged();
}
void PanelView::fitPanel(){if(const frontpanel::Panel *p=panel())showArea(QRectF(0,0,p->width,p->height));}
void PanelView::fitElements(bool selectedOnly){
    if(!panel())return;QList<Element> list;
    if(selectedOnly){for(auto *e:selectedElements())list<<*e;}else list=panel()->elements;
    const QRectF r=elementsBounds(list);if(r.isValid())showArea(r.adjusted(-2,-2,2,2));
}
QRectF PanelView::visibleArea() const{return QRectF(toPanel(QPointF(0,0)),toPanel(QPointF(viewport()->width(),viewport()->height())));}
void PanelView::scrollStep(int dx,int dy){horizontalScrollBar()->setValue(horizontalScrollBar()->value()+dx*viewport()->width()/4);verticalScrollBar()->setValue(verticalScrollBar()->value()+dy*viewport()->height()/4);}
void PanelView::scrollContentsBy(int,int){viewport()->update();top->update();left->update();}
void PanelView::resizeEvent(QResizeEvent *event){
    QAbstractScrollArea::resizeEvent(event);
    const QRect r=viewport()->geometry();top->setGeometry(r.left(),r.top()-rulerSize,r.width(),rulerSize);left->setGeometry(r.left()-rulerSize,r.top(),rulerSize,r.height());
    unitButton->setGeometry(r.left()-rulerSize,r.top()-rulerSize,rulerSize,rulerSize);updateScrollBars();
}

// ------------------------------------------------------------------ selection
void PanelView::setSelection(const QStringList &ids){selected=ids;viewport()->update();if(selectionChanged)selectionChanged();}
void PanelView::selectAll(){if(!panel())return;QStringList ids;for(const auto &e:panel()->elements)if(shown(e,options)||e.isContainer())ids<<e.id;setSelection(ids);}
void PanelView::clearSelection(){if(!selected.isEmpty())setSelection({});}
QList<Element*> PanelView::selectedElements(){
    QList<Element*> out;if(!panel())return out;
    for(const auto &id:selected)if(Element *e=findElement(panel()->elements,id))out<<e;
    return out;
}
QString PanelView::hit(QPointF p) const{
    if(!panel())return {};const double tolerance=4/scale;
    const auto &list=panel()->elements;
    for(int i=list.size()-1;i>=0;i--){const Element &e=list[i];if(!e.isContainer()&&!shown(e,options))continue;if(hitElement(e,p,tolerance))return e.id;}
    return {};
}
QRectF PanelView::selectionBounds(){QList<Element> list;for(auto *e:selectedElements())list<<*e;return shapeBounds(list);}
QList<QPointF> PanelView::handlePoints(const QRectF &b) const{
    return {b.topLeft(),QPointF(b.center().x(),b.top()),b.topRight(),QPointF(b.right(),b.center().y()),b.bottomRight(),QPointF(b.center().x(),b.bottom()),b.bottomLeft(),QPointF(b.left(),b.center().y())};
}
int PanelView::handleAt(QPointF view) const{
    auto *self=const_cast<PanelView*>(this);if(selected.isEmpty())return -1;const QRectF b=self->selectionBounds();if(b.isNull())return -1;
    const auto list=handlePoints(b);
    for(int i=0;i<list.size();i++){if(rotateHandles&&i%2)continue;if(QLineF(toView(list[i]),view).length()<=handleSize)return i;}
    return -1;
}
Element *PanelView::singleContour(){
    if(selected.size()!=1)return nullptr;Element *e=selectedElements().value(0);
    return e&&e->hasContour()?e:nullptr;
}
int PanelView::nodeAt(QPointF view) const{
    auto *e=const_cast<PanelView*>(this)->singleContour();if(!e||rotateHandles)return -1;
    for(int i=0;i<e->points.size();i++)if(QLineF(toView(e->points[i]),view).length()<=handleSize-1)return i;
    return -1;
}

// ------------------------------------------------------------------ tools
void PanelView::setTool(Tool tool){
    // Turning is the select tool with turning handles.
    const bool turn=tool==Rotate;if(turn)tool=Select;
    if(current==tool&&(tool!=Select||rotateHandles==turn))return;
    cancelShape();current=tool;if(tool!=Place)placement.clear();if(tool==Select)rotateHandles=turn;
    viewport()->update();setCursorFor(viewport()->mapFromGlobal(QCursor::pos()));if(toolChanged)toolChanged(turn?Rotate:tool);
}
void PanelView::beginPlacement(const QList<Element> &elements){
    if(elements.isEmpty())return;cancelShape();placement=elements;current=Place;
    // The point that sticks to the cursor: a single symbol's insertion point, otherwise the top left corner.
    placementReference=elements.size()==1&&elements[0].hasAnchor?elements[0].anchor:shapeBounds(elements).topLeft();
    viewport()->update();setCursorFor(viewport()->mapFromGlobal(QCursor::pos()));if(toolChanged)toolChanged(Place);
}
QList<Element> PanelView::placed(QPointF at,bool free) const{
    QList<Element> out=placement;const QPointF to=snap(at,free);const QTransform shift=QTransform::fromTranslate(to.x()-placementReference.x(),to.y()-placementReference.y());
    for(auto &e:out)transformElement(e,shift);return out;
}
void PanelView::cancelShape(){points.clear();drag=None;viewport()->update();}
void PanelView::add(const Element &e,bool select){
    if(!panel())return;if(beforeChange)beforeChange();panel()->elements.append(e);if(afterChange)afterChange();
    if(select)setSelection({e.id});viewport()->update();
}
void PanelView::changed(){if(afterChange)afterChange();viewport()->update();}
void PanelView::finishShape(){
    if(!newElement){points.clear();return;}
    if(current==Line&&points.size()>=2){Element e=newElement(ElementType::Line);e.points=points;points.clear();add(e);}
    else if(current==Polygon&&points.size()>=3){Element e=newElement(ElementType::Polygon);e.points=points;points.clear();add(e);}
    else points.clear();
    viewport()->update();
}

void PanelView::setCursorFor(QPointF view){
    QCursor c(Qt::ArrowCursor);
    switch(current){
    case Select:{
        const int h=handleAt(view);
        if(h>=0){if(rotateHandles)c=QCursor(Qt::CrossCursor);else c=QCursor(h%4==0?Qt::SizeFDiagCursor:h%4==2?Qt::SizeBDiagCursor:h%4==1?Qt::SizeVerCursor:Qt::SizeHorCursor);}
        else if(nodeAt(view)>=0)c=panelCursor("kreuz");
        break;}
    case Zoom:c=panelCursor("lupe");break;
    case Line:case Arc:case Dimension:c=panelCursor("stift");break;
    case Polygon:c=panelCursor("stiftpoly");break;
    case Rectangle:c=panelCursor("stiftrect");break;
    case Circle:c=panelCursor("stiftcirc");break;
    case Text:c=panelCursor("stifttext");break;
    case Drill:case Origin:c=panelCursor("kreuz");break;
    case Place:c=panelCursor("hand");break;
    default:break;
    }
    viewport()->setCursor(c);
}

// ------------------------------------------------------------------ dragging
void PanelView::beginDrag(Drag kind){
    drag=kind;moved=false;originals.clear();
    for(auto *e:selectedElements())originals.insert(e->id,*e);
    QList<Element> list;for(const auto &e:originals)list<<e;originalBounds=shapeBounds(list);
    if(kind==Move||kind==Stretch||kind==Turn||kind==NodeDrag){if(beforeChange)beforeChange();}
}
void PanelView::applyDrag(QPointF to,Qt::KeyboardModifiers modifiers){
    const bool free=modifiers&Qt::ShiftModifier;
    auto apply=[&](const QTransform &map){
        for(auto it=originals.begin();it!=originals.end();++it)if(Element *e=findElement(panel()->elements,it.key())){
            Element moved=it.value();transformElement(moved,map);
            if(moved.type==ElementType::Dimension&&moved.parameters["generator"]=="dimension")moved=regenerate(moved);
            *e=moved;
        }
    };
    if(drag==Move){
        // Moved by whole grid steps, so that what lies on the grid stays on it (Shift: freely).
        QPointF delta=to-pressAt;const frontpanel::Panel *p=panel();
        if(!free&&p->snap&&p->grid>0)delta=QPointF(std::round(delta.x()/p->grid)*p->grid,std::round(delta.y()/p->grid)*p->grid);
        apply(QTransform::fromTranslate(delta.x(),delta.y()));
    }else if(drag==Stretch&&!originalBounds.isNull()){
        const auto list=handlePoints(originalBounds);const QPointF fixed=list[(handle+4)%8];const QPointF target=snap(to,free);
        double sx=1,sy=1;const double w=originalBounds.width(),h=originalBounds.height();
        if(handle%4!=1&&w>1e-9)sx=(target.x()-fixed.x())/(list[handle].x()-fixed.x());
        if(handle%4!=3&&h>1e-9)sy=(target.y()-fixed.y())/(list[handle].y()-fixed.y());
        if(proportional||(handle%2==0&&(modifiers&Qt::ControlModifier))){const double s=handle%4==1?sy:handle%4==3?sx:(std::abs(sx)>std::abs(sy)?sx:sy);sx=sy=s;}
        if(std::abs(sx)<1e-6)sx=1e-6;if(std::abs(sy)<1e-6)sy=1e-6;
        apply(QTransform::fromTranslate(-fixed.x(),-fixed.y())*QTransform::fromScale(sx,sy)*QTransform::fromTranslate(fixed.x(),fixed.y()));
    }else if(drag==Turn&&!originalBounds.isNull()){
        // Turning about the middle, in steps of 45° (Shift: freely).
        const QPointF c=originalBounds.center();double a0=std::atan2(-(pressAt.y()-c.y()),pressAt.x()-c.x()),a1=std::atan2(-(to.y()-c.y()),to.x()-c.x());
        double angle=(a1-a0)*180/std::numbers::pi;if(!free)angle=std::round(angle/45)*45;
        apply(rotationAbout(c,angle));
    }else if(drag==NodeDrag){
        if(Element *e=singleContour()){const Element &o=originals.value(e->id);if(node>=0&&node<o.points.size()){Element changed=o;changed.points[node]=snap(to,free);*e=changed;}}
    }
    viewport()->update();
}
void PanelView::addNode(int index){
    Element *e=singleContour();if(!e||index<0||index>=e->points.size())return;if(beforeChange)beforeChange();
    const int next=(index+1)%e->points.size();if(e->type==ElementType::Line&&index==e->points.size()-1){e->points.insert(index,(e->points[index]+e->points[index-1])/2);}
    else e->points.insert(index+1,(e->points[index]+e->points[next])/2);
    if(e->type==ElementType::Rectangle)e->type=ElementType::Polygon;
    changed();
}
void PanelView::deleteNode(int index){
    Element *e=singleContour();if(!e||index<0||index>=e->points.size())return;const int minimum=e->type==ElementType::Line?2:3;
    if(beforeChange)beforeChange();
    if(e->points.size()<=minimum){const QString id=e->id;auto &list=panel()->elements;for(int i=0;i<list.size();i++)if(list[i].id==id){list.removeAt(i);break;}selected.clear();if(selectionChanged)selectionChanged();}
    else{e->points.removeAt(index);if(e->type==ElementType::Rectangle)e->type=ElementType::Polygon;}
    changed();
}

// ------------------------------------------------------------------ mouse
void PanelView::mousePressEvent(QMouseEvent *event){
    if(!panel())return;setFocus();
    const QPointF view=event->position(),p=toPanel(view);const bool free=event->modifiers()&Qt::ShiftModifier;
    pressAt=p;pressView=event->pos();cursor=p;moved=false;
    if(event->button()==Qt::MiddleButton){drag=Pan;return;}
    if(event->button()==Qt::RightButton){
        // The right button ends a contour, leaves a drawing tool, or opens the context menu.
        if(current==Line||current==Polygon){if(!points.isEmpty()){finishShape();return;}setTool(Select);return;}
        if(current==Zoom){setZoom(scale/2,view);return;}
        if(current==Place){placement.clear();setTool(Select);return;}
        if(current!=Select){cancelShape();setTool(Select);return;}
        const int n=nodeAt(view);
        if(n<0){const QString id=hit(p);if(!id.isEmpty()&&!selected.contains(id))setSelection({id});}
        if(contextMenuRequested)contextMenuRequested(event->globalPosition().toPoint(),n);
        return;
    }
    if(event->button()!=Qt::LeftButton)return;
    switch(current){
    case Select:{
        if(const int n=nodeAt(view);n>=0){node=n;beginDrag(NodeDrag);return;}
        if(const int h=handleAt(view);h>=0){handle=h;beginDrag(rotateHandles?Turn:Stretch);return;}
        const QString id=hit(p);
        if(id.isEmpty()){if(!free)clearSelection();drag=Band;return;}
        pressedSelected=selected.contains(id);
        if(free){QStringList ids=selected;if(pressedSelected)ids.removeAll(id);else ids<<id;setSelection(ids);if(pressedSelected){drag=None;return;}}
        else if(!pressedSelected)setSelection({id});
        beginDrag(Move);return;}
    case Zoom:drag=ZoomBand;return;
    case Line:case Polygon:points<<snap(p,free);viewport()->update();return;
    case Rectangle:case Circle:if(points.isEmpty()){points<<snap(p,free);drag=Shape;}return;
    case Arc:points<<snap(p,free);
        if(points.size()==3){
            // Centre, start (which sets the radius) and end; the arc runs clockwise from start to end.
            const QPointF c=points[0],s=points[1],f=points[2];const double r=QLineF(c,s).length();
            if(r>1e-6&&newElement){Element e=newElement(ElementType::Arc);e.center=c;e.radiusX=e.radiusY=r;
                const double a=std::atan2(-(s.y()-c.y()),s.x()-c.x())*180/std::numbers::pi,b=std::atan2(-(f.y()-c.y()),f.x()-c.x())*180/std::numbers::pi;
                double sweep=std::fmod(a-b+720,360);if(sweep<=1e-9)sweep=360;e.startAngle=b;e.spanAngle=sweep;points.clear();add(e);setTool(Select);}
            else points.clear();
        }
        viewport()->update();return;
    case Text:if(textRequested)textRequested(snap(p,free));return;
    case Drill:if(drillRequested)drillRequested(snap(p,free));return;
    case Dimension:points<<snap(p,free);
        if(points.size()==3){const QPolygonF clicks=points;points.clear();if(dimensionRequested)dimensionRequested(clicks[0],clicks[1],clicks[2]);}
        viewport()->update();return;
    case Origin:if(beforeChange)beforeChange();panel()->origin=snap(p,free);changed();top->update();left->update();setTool(Select);return;
    case Place:{const QList<Element> list=placed(p,free);if(beforeChange)beforeChange();QStringList ids;for(const auto &e:list){panel()->elements.append(e);ids<<e.id;}
        if(afterChange)afterChange();placement.clear();current=Select;setSelection(ids);if(toolChanged)toolChanged(Select);setCursorFor(view);return;}
    default:return;
    }
}
void PanelView::mouseMoveEvent(QMouseEvent *event){
    if(!panel())return;const QPointF view=event->position(),p=toPanel(view);cursor=p;
    if(pointerMoved)pointerMoved(p);
    top->mark=p.x();left->mark=p.y();top->update();left->update();
    if(drag==Pan){const QPoint d=event->pos()-pressView;pressView=event->pos();horizontalScrollBar()->setValue(horizontalScrollBar()->value()-d.x());verticalScrollBar()->setValue(verticalScrollBar()->value()-d.y());return;}
    lastView=view;lastModifiers=event->modifiers();
    {
        // At the edge of the view the content scrolls, while dragging and while something waits to be placed.
        const bool busy=(drag!=None&&drag!=Pan&&moved)||current==Place||((current==Line||current==Polygon||current==Arc||current==Dimension)&&!points.isEmpty());
        const bool atEdge=view.x()<14||view.y()<14||view.x()>viewport()->width()-14||view.y()>viewport()->height()-14;
        if(busy&&atEdge){if(!scroller->isActive())scroller->start();}else scroller->stop();
    }
    if(drag!=None&&(event->buttons()&Qt::LeftButton)){
        if(!moved&&(event->pos()-pressView).manhattanLength()<QApplication::startDragDistance()){return;}
        moved=true;
        if(drag==Move||drag==Stretch||drag==Turn||drag==NodeDrag)applyDrag(p,event->modifiers());
        viewport()->update();return;
    }
    setCursorFor(view);
    if(current!=Select)viewport()->update();
}
void PanelView::mouseReleaseEvent(QMouseEvent *event){
    scroller->stop();if(!panel())return;const QPointF p=toPanel(event->position());const bool free=event->modifiers()&Qt::ShiftModifier;
    const Drag was=drag;drag=None;
    switch(was){
    case Move:case Stretch:case Turn:case NodeDrag:
        if(moved)changed();else{
            if(afterChange)afterChange();
            // A click on what was already selected switches between stretching and turning handles.
            if(was==Move&&pressedSelected&&!free){rotateHandles=!rotateHandles;viewport()->update();if(toolChanged)toolChanged(rotateHandles?Rotate:Select);}
        }
        originals.clear();break;
    case Band:{
        if(!moved)break;const QRectF band=QRectF(pressAt,p).normalized();QStringList ids=free?selected:QStringList();
        for(const auto &e:panel()->elements){if(!e.isContainer()&&!shown(e,options))continue;if(elementBounds(e).intersects(band)&&!ids.contains(e.id))ids<<e.id;}
        setSelection(ids);break;}
    case ZoomBand:
        if(moved&&QRectF(pressAt,p).normalized().width()>1e-3)showArea(QRectF(pressAt,p).normalized());else setZoom(scale*2,event->position());
        break;
    case Shape:{
        const QPointF a=points.value(0),b=snap(p,free);
        if(QLineF(a,b).length()<1e-6&&!moved){viewport()->update();drag=Shape;return;}   // first click of two: wait for the second
        points.clear();if(!newElement)break;
        if(current==Rectangle){const QRectF r=QRectF(a,b).normalized();if(r.width()>0&&r.height()>0){Element e=newElement(ElementType::Rectangle);e.points={r.topLeft(),r.topRight(),r.bottomRight(),r.bottomLeft()};add(e);}}
        else if(current==Circle){const double r=QLineF(a,b).length();if(r>0){Element e=newElement(ElementType::Ellipse);e.center=a;e.radiusX=e.radiusY=r;add(e);}}
        break;}
    default:break;
    }
    viewport()->update();
}
void PanelView::mouseDoubleClickEvent(QMouseEvent *event){
    if(!panel())return;
    if(current==Line||current==Polygon){if(!points.isEmpty())points.removeLast();finishShape();return;}
    if(current==Select&&event->button()==Qt::LeftButton){const QString id=hit(toPanel(event->position()));if(!id.isEmpty()&&propertiesRequested){setSelection({id});rotateHandles=false;propertiesRequested(id);}}
}
void PanelView::wheelEvent(QWheelEvent *event){
    if(event->modifiers()&(Qt::ControlModifier|Qt::MetaModifier)){const double steps=event->angleDelta().y()/120.0;if(steps)setZoom(scale*std::pow(1.25,steps),event->position());event->accept();return;}
    QAbstractScrollArea::wheelEvent(event);
}
bool PanelView::viewportEvent(QEvent *event){
    if(event->type()==QEvent::NativeGesture){auto *g=static_cast<QNativeGestureEvent*>(event);
        if(g->gestureType()==Qt::ZoomNativeGesture){setZoom(scale*(1+g->value()),g->position());return true;}}
    return QAbstractScrollArea::viewportEvent(event);
}
void PanelView::keyPressEvent(QKeyEvent *event){
    if(!panel())return;
    switch(event->key()){
    case Qt::Key_Escape:if(!points.isEmpty()){cancelShape();return;}if(current!=Select){setTool(Select);return;}clearSelection();return;
    case Qt::Key_Return:case Qt::Key_Enter:if(current==Line||current==Polygon){finishShape();return;}break;
    case Qt::Key_Left:case Qt::Key_Right:case Qt::Key_Up:case Qt::Key_Down:{
        if(selected.isEmpty())break;const double step=(event->modifiers()&Qt::ShiftModifier)?0.1:(panel()->grid>0?panel()->grid:1);
        const QPointF d=event->key()==Qt::Key_Left?QPointF(-step,0):event->key()==Qt::Key_Right?QPointF(step,0):event->key()==Qt::Key_Up?QPointF(0,-step):QPointF(0,step);
        if(beforeChange)beforeChange();for(auto *e:selectedElements())transformElement(*e,QTransform::fromTranslate(d.x(),d.y()));changed();return;}
    default:break;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

// ------------------------------------------------------------------ painting
void PanelView::setUnderlay(const QList<Underlay> &boards){underlay=boards;viewport()->update();}
void PanelView::paintEvent(QPaintEvent *){
    QPainter p(viewport());p.fillRect(viewport()->rect(),milledColor());
    const frontpanel::Panel *pl=panel();if(!pl)return;
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF o=origin();const QTransform map(scale,0,0,scale,o.x(),o.y());
    // A shadow under the panel.
    p.fillRect(QRectF(o+QPointF(4,4),QSizeF(pl->width*scale,pl->height*scale)),QColor(0,0,0,70));
    p.setTransform(map);RenderOptions view=options;view.grid=pl->gridVisible;paintPanel(p,*doc,*pl,view);
    // Hidden kinds of objects stay visible while they are selected, so that they can be worked on.
    if(!(options.dimensions&&options.milled&&options.engraved&&options.drills&&options.texts&&options.other)){
        RenderOptions all=options;all.dimensions=all.milled=all.engraved=all.drills=all.texts=all.other=true;
        for(auto *e:const_cast<PanelView*>(this)->selectedElements())paintElement(p,*doc,*e,all);
    }
    // Circuit boards behind the panel, seen through it: in the view only.
    const QColor boardInk(0,130,60),awayInk(130,160,140);
    for(const auto &b:underlay){
        QPen edge(boardInk,1.5,Qt::DashLine);edge.setCosmetic(true);p.setPen(edge);p.setBrush(QColor(0,140,70,26));p.drawPolygon(b.outline);
        for(const auto &part:b.parts){
            if(part.outline.size()<3)continue;
            QPen line(part.facing?boardInk:awayInk,1,part.facing?Qt::SolidLine:Qt::DotLine);line.setCosmetic(true);p.setPen(line);p.setBrush(Qt::NoBrush);p.drawPolygon(part.outline);
        }
    }
    p.setBrush(Qt::NoBrush);
    p.resetTransform();
    if(!underlay.isEmpty()){
        QFont small=font();small.setPointSizeF(8);p.setFont(small);
        for(const auto &b:underlay)for(const auto &part:b.parts){
            const QPointF c=toView(part.centre);p.setPen(QPen(part.facing?boardInk:awayInk,1));
            p.drawLine(c-QPointF(4,0),c+QPointF(4,0));p.drawLine(c-QPointF(0,4),c+QPointF(0,4));
            if(!part.label.isEmpty())p.drawText(c+QPointF(5,-5),part.label);
        }
    }
    // Placement and previews of the drawing tools.
    const QColor ink(0,90,255);QPen dash(ink,1,Qt::DashLine);
    if(current==Place&&!placement.isEmpty()){
        p.setOpacity(0.6);p.setTransform(map);for(const auto &e:placed(cursor,QApplication::keyboardModifiers()&Qt::ShiftModifier))paintElement(p,*doc,e,options);
        p.resetTransform();p.setOpacity(1);
    }
    const QPointF c=snap(cursor,QApplication::keyboardModifiers()&Qt::ShiftModifier);
    if((current==Line||current==Polygon||current==Dimension||current==Arc)&&!points.isEmpty()){
        p.setPen(QPen(ink,1));QPolygonF line;for(auto q:points)line<<toView(q);line<<toView(c);if(current==Polygon&&points.size()>1)line<<toView(points[0]);
        if(current==Arc){p.setPen(dash);const double r=QLineF(toView(points[0]),toView(points.size()>1?points[1]:c)).length();p.drawEllipse(toView(points[0]),r,r);p.setPen(QPen(ink,1));}
        p.drawPolyline(line);for(auto q:points)p.drawRect(QRectF(toView(q)-QPointF(2,2),QSizeF(4,4)));
    }
    if(current==Rectangle&&!points.isEmpty()){p.setPen(QPen(ink,1));p.drawRect(QRectF(toView(points[0]),toView(c)).normalized());}
    if(current==Circle&&!points.isEmpty()){p.setPen(QPen(ink,1));const double r=QLineF(points[0],c).length()*scale;p.drawEllipse(toView(points[0]),r,r);}
    if((drag==Band||drag==ZoomBand)&&moved){p.setPen(dash);p.setBrush(QColor(0,90,255,30));p.drawRect(QRectF(toView(pressAt),toView(cursor)).normalized());p.setBrush(Qt::NoBrush);}
    // The user origin.
    {const QPointF u=toView(pl->origin);p.setPen(QPen(QColor(220,0,0),1));p.drawLine(u-QPointF(6,0),u+QPointF(6,0));p.drawLine(u-QPointF(0,6),u+QPointF(0,6));}
    // Selection: the outline of each selected element, the handles around all, and the nodes of a single contour.
    if(!selected.isEmpty()){
        p.setPen(QPen(QColor(0,90,255),1,Qt::DotLine));
        for(auto *e:const_cast<PanelView*>(this)->selectedElements())p.drawRect(map.mapRect(shapeBounds({*e})));
        const QRectF b=const_cast<PanelView*>(this)->selectionBounds();
        if(!b.isNull()){
            const auto list=handlePoints(b);p.setPen(QPen(Qt::black,1));p.setBrush(rotateHandles?QColor(255,255,255):QColor(0,90,255));
            for(int i=0;i<list.size();i++){if(rotateHandles&&i%2)continue;const QRectF h(toView(list[i])-QPointF(handleSize/2,handleSize/2),QSizeF(handleSize,handleSize));if(rotateHandles)p.drawEllipse(h);else p.drawRect(h);}
            if(rotateHandles){p.setPen(QPen(Qt::black,1));const QPointF m=toView(b.center());p.drawLine(m-QPointF(4,0),m+QPointF(4,0));p.drawLine(m-QPointF(0,4),m+QPointF(0,4));}
        }
        if(Element *e=const_cast<PanelView*>(this)->singleContour();e&&!rotateHandles){p.setBrush(Qt::white);p.setPen(QPen(Qt::black,1));for(auto q:e->points)p.drawRect(QRectF(toView(q)-QPointF(3,3),QSizeF(6,6)));}
    }
}
}
