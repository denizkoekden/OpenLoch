#include "overview.h"
#include "view.h"
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace openloch::pcb {
namespace {
const QColor boardGreen(18,82,30),shownGreen(120,220,120,170),shownEdge(170,255,170);
}

BoardOverview::BoardOverview(BoardView *v,QWidget *parent):QWidget(parent),view(v){
    setObjectName("overview");setMinimumHeight(80);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setCursor(Qt::PointingHandCursor);
}
QRectF BoardOverview::boardRect() const{
    if(!view->document()||view->document()->boards.isEmpty())return {};
    const auto &b=view->document()->board();const double margin=6,scale=std::min((width()-2*margin)/b.width,(height()-2*margin)/b.height);
    return QRectF((width()-b.width*scale)/2,(height()-b.height*scale)/2,b.width*scale,b.height*scale);
}
QPointF BoardOverview::toPixel(QPointF mm) const{
    const QRectF r=boardRect();if(r.isEmpty())return {};const auto &b=view->document()->board();const double scale=r.width()/b.width;
    return QPointF(view->fromBelow()?r.right()-mm.x()*scale:r.left()+mm.x()*scale,r.top()+mm.y()*scale);
}
QPointF BoardOverview::toBoard(QPointF pixel) const{
    const QRectF r=boardRect();if(r.isEmpty())return {};const auto &b=view->document()->board();const double scale=r.width()/b.width;
    return QPointF(view->fromBelow()?(r.right()-pixel.x())/scale:(pixel.x()-r.left())/scale,(pixel.y()-r.top())/scale);
}
QRectF BoardOverview::shownRect() const{const QRectF a=view->visibleArea();return QRectF(toPixel(a.topLeft()),toPixel(a.bottomRight())).normalized();}
void BoardOverview::paintEvent(QPaintEvent *){
    QPainter p(this);p.fillRect(rect(),palette().window());const QRectF r=boardRect();if(r.isEmpty())return;
    p.fillRect(r,boardGreen);p.setClipRect(rect().adjusted(1,1,-1,-1));
    p.setPen(QPen(shownEdge,1));p.setBrush(shownGreen);p.drawRect(shownRect());
}
void BoardOverview::mousePressEvent(QMouseEvent *event){
    if(boardRect().isEmpty())return;pressed=true;dragged=false;pressAt=event->position();onShown=shownRect().contains(pressAt);
    grab=view->toBoard(QPointF(view->width()/2.0,view->height()/2.0))-toBoard(pressAt);
}
void BoardOverview::mouseMoveEvent(QMouseEvent *event){
    if(!pressed||!(event->buttons()&Qt::LeftButton))return;
    const QPointF d=event->position()-pressAt;if(!dragged&&std::hypot(d.x(),d.y())<3)return;
    // The shown part follows the pointer as it was grabbed; beside it, the view's middle goes under the pointer.
    if(!dragged)view->rememberView();dragged=true;view->centreOn(toBoard(event->position())+(onShown?grab:QPointF()));update();
}
void BoardOverview::mouseReleaseEvent(QMouseEvent *event){
    if(!pressed)return;pressed=false;if(dragged){update();return;}
    const QPointF middle(view->width()/2.0,view->height()/2.0);
    if(onShown)view->zoomAt(event->button()==Qt::RightButton?1/BoardView::zoomStep:BoardView::zoomStep,middle);
    else if(event->button()==Qt::LeftButton){view->rememberView();view->centreOn(toBoard(event->position()));}
    update();
}
}
