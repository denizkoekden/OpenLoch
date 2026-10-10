#include "model.h"
#include "text.h"
#include "dimension.h"
#include "language.h"
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QLineF>
#include <QPainterPathStroker>
#include <QRegularExpression>
#include <QSet>
#include <QTime>
#include <QUuid>
#include <QtMath>
#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace openloch::schematic {

// --- documents and ids
Document newDocument(const QString &sheetName){
    Document d;d.id=newId();d.sheets.append(newSheet(sheetName));return d;
}
Sheet newSheet(const QString &name,double width,double height,double grid){
    Sheet s;s.id=newId();s.name=name;s.width=width;s.height=height;s.grid=grid;return s;
}
QString newId(){return QUuid::createUuid().toString(QUuid::Id128);}
QString unitName(ScaleUnit unit){
    switch(unit){case ScaleUnit::Centimetre:return QStringLiteral("cm");case ScaleUnit::Metre:return QStringLiteral("m");
        case ScaleUnit::Kilometre:return QStringLiteral("km");default:return QStringLiteral("mm");}
}
double unitMillimetres(ScaleUnit unit){
    switch(unit){case ScaleUnit::Centimetre:return 10;case ScaleUnit::Metre:return 1000;case ScaleUnit::Kilometre:return 1000000;default:return 1;}
}
void assignIds(Item &item){
    item.id=newId();
    for(auto &c:item.children)assignIds(c);
}
void freshIds(const QList<QList<Item>*> &lists){
    QHash<QString,QString> renamed;
    std::function<void(Item&)> assign=[&](Item &i){
        const QString old=i.id;i.id=newId();if(i.type==ItemType::Component&&!old.isEmpty())renamed.insert(old,i.id);
        for(auto &c:i.children)assign(c);
    };
    std::function<void(Item&)> follow=[&](Item &i){
        if(!i.parentId.isEmpty()&&renamed.contains(i.parentId))i.parentId=renamed[i.parentId];
        for(auto &c:i.children)follow(c);
    };
    for(auto *l:lists)for(auto &i:*l)assign(i);
    for(auto *l:lists)for(auto &i:*l)follow(i);
}
namespace {
void complete(QList<Item> &items,QSet<QString> &seen){
    for(auto &i:items){
        if(i.id.isEmpty()||seen.contains(i.id))i.id=newId();
        seen.insert(i.id);complete(i.children,seen);
    }
}
}
void completeIds(Document &document){
    if(document.id.isEmpty())document.id=newId();
    QSet<QString> seen;
    for(auto &s:document.sheets){
        if(s.id.isEmpty()||seen.contains(s.id))s.id=newId();
        seen.insert(s.id);
    }
    for(auto &s:document.sheets){complete(s.items,seen);complete(s.titleBlock.items,seen);}
}
namespace {
Item *find(QList<Item> &items,const QString &id){
    for(auto &i:items){
        if(i.id==id)return &i;
        if(auto *c=find(i.children,id))return c;
    }
    return nullptr;
}
const Item *find(const QList<Item> &items,const QString &id){
    for(const auto &i:items){
        if(i.id==id)return &i;
        if(const auto *c=find(i.children,id))return c;
    }
    return nullptr;
}
}
Item *findItem(Sheet &sheet,const QString &id){
    if(auto *i=find(sheet.items,id))return i;
    return find(sheet.titleBlock.items,id);
}
const Item *findItem(const Sheet &sheet,const QString &id){
    if(const auto *i=find(sheet.items,id))return i;
    return find(sheet.titleBlock.items,id);
}
int sheetIndex(const Document &document,const QString &id){
    for(int i=0;i<document.sheets.size();i++)if(document.sheets[i].id==id)return i;
    return -1;
}

// --- geometry
QTransform placement(const Item &component){
    QTransform t;t.translate(component.pos.x(),component.pos.y());t.rotate(-component.rotation);
    if(component.mirrored)t.scale(-1,1);
    return t;
}
QPointF pinPosition(const Item &component,const Item &contact){return placement(component).map(contact.pin);}

namespace {
double normalized(double degrees){
    double a=std::fmod(degrees,360.);if(a<0)a+=360;
    if(std::abs(a-360)<1e-9)a=0;
    return a;
}
QPointF turned(QPointF p,QPointF pivot,double degrees){
    // Counter-clockwise on the screen (y down).
    const double a=qDegreesToRadians(degrees),c=std::cos(a),s=std::sin(a);const QPointF d=p-pivot;
    return pivot+QPointF(d.x()*c+d.y()*s,-d.x()*s+d.y()*c);
}
QPainterPath stroked(const QPainterPath &path,double width){
    QPainterPathStroker stroker;stroker.setWidth(std::max(width,.01));stroker.setCapStyle(Qt::RoundCap);stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(path);
}
QTransform frame(QPointF centre,double rotation){
    QTransform t;t.translate(centre.x(),centre.y());t.rotate(-rotation);return t;
}
// Arc angles are true angles from the centre, as the reference shows them; QPainterPath takes the angle of the circle
// the ellipse is scaled from.
double parametric(double degrees,double rx,double ry){
    const double a=qDegreesToRadians(degrees);
    return qRadiansToDegrees(std::atan2(rx*std::sin(a),ry*std::cos(a)));
}
QPainterPath ellipsePath(const Item &e){
    const QRectF r(-e.size.width()/2,-e.size.height()/2,e.size.width(),e.size.height());QPainterPath p;
    if(e.arc==ArcStyle::Full||std::abs(e.stop-e.start)>=360-1e-9)p.addEllipse(r);
    else{
        const double rx=std::abs(e.size.width()/2),ry=std::abs(e.size.height()/2);
        const double start=parametric(e.start,rx,ry);
        double span=std::fmod(parametric(e.stop,rx,ry)-start,360.);if(span<=1e-9)span+=360;
        if(e.arc==ArcStyle::Pie){p.moveTo(0,0);p.arcTo(r,start,span);p.closeSubpath();}
        else{p.arcMoveTo(r,start);p.arcTo(r,start,span);if(e.arc==ArcStyle::Chord)p.closeSubpath();}
    }
    return frame(e.centre,e.rotation).map(p);
}
QPainterPath bezierPath(const QPolygonF &points){
    QPainterPath p;if(points.isEmpty())return p;p.moveTo(points[0]);
    for(int i=1;i+2<points.size();i+=3)p.cubicTo(points[i],points[i+1],points[i+2]);
    return p;
}
QPainterPath rectanglePath(const Item &r){
    const double w=r.size.width(),h=r.size.height();const QRectF box(-w/2,-h/2,w,h);QPainterPath p;
    const double c=std::clamp(r.corner,0.,50.)/100*std::min(std::abs(w),std::abs(h));
    if(r.corners==Corners::Round&&c>0)p.addRoundedRect(box,c,c);
    else if(r.corners==Corners::Bevel&&c>0){
        p.moveTo(box.left()+c,box.top());p.lineTo(box.right()-c,box.top());p.lineTo(box.right(),box.top()+c);p.lineTo(box.right(),box.bottom()-c);
        p.lineTo(box.right()-c,box.bottom());p.lineTo(box.left()+c,box.bottom());p.lineTo(box.left(),box.bottom()-c);p.lineTo(box.left(),box.top()+c);p.closeSubpath();
    }else p.addRect(box);
    return frame(r.centre,r.rotation).map(p);
}
}
QPainterPath path(const Item &item){
    QPainterPath p;
    switch(item.type){
    case ItemType::Line:p.addPolygon(item.points);break;
    case ItemType::Polygon:p.addPolygon(item.points);p.closeSubpath();break;
    case ItemType::Bezier:return bezierPath(item.points);
    case ItemType::Rectangle:return rectanglePath(item);
    case ItemType::Ellipse:return ellipsePath(item);
    case ItemType::TextBox:case ItemType::Image:p.addPolygon(corners(item));p.closeSubpath();break;
    case ItemType::Junction:{const double r=std::max(item.size.width(),.1)/2;p.addEllipse(item.pos,r,r);break;}
    default:break;
    }
    return p;
}
QPolygonF corners(const Item &item){
    const double w=item.size.width()/2,h=item.size.height()/2;
    return frame(item.centre,item.rotation).map(QPolygonF{QPointF(-w,-h),QPointF(w,-h),QPointF(w,h),QPointF(-w,h)});
}
// The outline of an element in the coordinates it lives in.
QPainterPath shape(const Item &item){
    QPainterPath out;
    switch(item.type){
    case ItemType::Line:{QPainterPath p;p.addPolygon(item.points);return stroked(p,item.pen.width);}
    case ItemType::Polygon:{QPainterPath p;p.addPolygon(item.points);p.closeSubpath();p.setFillRule(Qt::WindingFill);return p.united(stroked(p,item.pen.width));}
    case ItemType::Bezier:return stroked(bezierPath(item.points),item.pen.width);
    case ItemType::Rectangle:{const auto p=rectanglePath(item);return p.united(stroked(p,item.pen.width));}
    case ItemType::Ellipse:{const auto p=ellipsePath(item);
        return item.arc==ArcStyle::Arc?stroked(p,item.pen.width):p.united(stroked(p,item.pen.width));}
    case ItemType::TextBox:case ItemType::Image:{QPainterPath p;p.addPolygon(corners(item));p.closeSubpath();return p;}
    case ItemType::Junction:{const double r=std::max(item.size.width(),.1)/2;out.addEllipse(item.pos,r,r);return out;}
    case ItemType::Text:case ItemType::NetLabel:case ItemType::Contact:
        out.addPolygon(textOutline(item,item.text));out.closeSubpath();
        if(item.type==ItemType::Contact&&item.hasPin)out.addEllipse(item.pin,.3,.3);
        return out;
    case ItemType::Group:
        for(const auto &c:item.children)out=out.united(shape(c));
        return out;
    case ItemType::Dimension:{
        // Lines, arrows and texts (the value as on a sheet of scale 1).
        const auto d=dimensionDrawing(item,1);
        QPainterPath lines=d.line;for(const auto &e:d.extensions){lines.moveTo(e.p1());lines.lineTo(e.p2());}
        out=stroked(lines,.3);
        for(const auto &a:d.arrows){QPainterPath p;p.addPolygon(a);p.closeSubpath();out=out.united(p);}
        for(const auto &t:d.texts){QPainterPath p;p.addPolygon(textOutline(t,t.text));p.closeSubpath();out=out.united(p);}
        return out;
    }
    case ItemType::Component:{
        const auto t=placement(item);
        for(const auto &c:item.children){
            if(isText(c)){
                const QString shown=c.role==TextRole::Designator?item.designator:c.role==TextRole::Value?item.value:c.text;
                if((c.role==TextRole::Designator&&!item.designatorVisible)||(c.role==TextRole::Value&&!item.valueVisible)||!c.visible)continue;
                QPainterPath p;p.addPolygon(textOutline(placedText(c,item),shown));p.closeSubpath();out.addPath(p);
            }else out.addPath(t.map(shape(c)));
        }
        return out;
    }
    }
    return out;
}
QRectF bounds(const Item &item){
    if(item.type==ItemType::Line||item.type==ItemType::Polygon){
        // Without the stroker: the points widened by half the line width.
        const double h=item.pen.width/2;return item.points.boundingRect().adjusted(-h,-h,h,h);
    }
    return shape(item).boundingRect();
}
QRectF bounds(const QList<Item> &items){
    QRectF r;for(const auto &i:items)r=r.united(bounds(i));return r;
}

namespace {
void transformPoints(Item &item,const std::function<QPointF(QPointF)> &map){
    for(auto &p:item.points)p=map(p);
    item.pos=map(item.pos);item.centre=map(item.centre);
    // A contact's connection point goes with it.
    if(item.type==ItemType::Contact&&item.hasPin)item.pin=map(item.pin);
}
Align swapped(Align a){return a==Align::Left?Align::Right:a==Align::Right?Align::Left:a;}
}
bool isText(const Item &item){
    return item.type==ItemType::Text||item.type==ItemType::Contact||item.type==ItemType::NetLabel;
}
void move(Item &item,QPointF delta){
    if(item.type==ItemType::Component){item.pos+=delta;return;}
    transformPoints(item,[delta](QPointF p){return p+delta;});
    for(auto &c:item.children)move(c,delta);
}
void rotate(Item &item,QPointF pivot,double degrees){
    if(item.type==ItemType::Component){item.pos=turned(item.pos,pivot,degrees);item.rotation=normalized(item.rotation+degrees);return;}
    transformPoints(item,[&](QPointF p){return turned(p,pivot,degrees);});
    if(item.type!=ItemType::Line&&item.type!=ItemType::Polygon&&item.type!=ItemType::Bezier&&item.type!=ItemType::Group&&item.type!=ItemType::Junction&&item.type!=ItemType::Dimension)
        item.rotation=normalized(item.rotation+degrees);
    for(auto &c:item.children)rotate(c,pivot,degrees);
}
void mirror(Item &item,double axis,bool textsToo){
    if(item.type==ItemType::Component){
        item.pos.setX(2*axis-item.pos.x());item.mirrored=!item.mirrored;item.rotation=normalized(-item.rotation);return;
    }
    transformPoints(item,[axis](QPointF p){return QPointF(2*axis-p.x(),p.y());});
    if(item.type==ItemType::Dimension)item.offset=-item.offset;   // the dimension line stays on its side of the points
    if(item.type==ItemType::Rectangle||item.type==ItemType::TextBox||item.type==ItemType::Image)item.rotation=normalized(-item.rotation);
    if(item.type==ItemType::Ellipse){
        item.rotation=normalized(-item.rotation);
        const double s=item.start,e=item.stop;item.start=normalized(180-e);item.stop=item.start+std::fmod(e-s+720,360.);
        if(e-s>=360)item.stop=item.start+360;
    }
    if(isText(item)){
        item.rotation=normalized(-item.rotation);
        if(textsToo)item.mirrored=!item.mirrored;else item.align=swapped(item.align);
    }
    for(auto &c:item.children)mirror(c,axis,textsToo);
}
void letter(Item &component,const Lettering &d,const Lettering &v,const Lettering &k){
    auto apply=[](Item &t,const Lettering &l){if(!l.set)return;t.font.family=l.family;t.font.height=l.height;t.font.bold=l.bold;t.font.italic=l.italic;t.font.color=l.color;};
    std::function<void(QList<Item>&)> walk=[&](QList<Item> &list){
        for(auto &c:list){
            if(c.role==TextRole::Designator)apply(c,d);else if(c.role==TextRole::Value)apply(c,v);else if(c.type==ItemType::Contact)apply(c,k);
            if(c.type==ItemType::Group)walk(c.children);
        }
    };
    walk(component.children);
}
namespace {
void partsOf(const QList<Item> &items,QList<const Item*> &parts,QList<const Item*> &found){
    for(const auto &c:items){
        if(c.type==ItemType::Contact)found<<&c;
        else if(c.type==ItemType::Group)partsOf(c.children,parts,found);
        else if(!isText(c))parts<<&c;
    }
}
// A line of no length, drawn as a dot.
bool dot(const Item &i){
    if(i.type!=ItemType::Line||i.points.isEmpty())return false;
    for(auto p:i.points)if(QLineF(p,i.points.first()).length()>1e-6)return false;
    return true;
}
void contactsOf(QList<Item> &items,QList<Item*> &out){
    for(auto &c:items){if(c.type==ItemType::Contact)out<<&c;else if(c.type==ItemType::Group)contactsOf(c.children,out);}
}
}
QList<std::optional<QPointF>> guessedPins(const Item &component){
    QList<const Item*> parts,list;partsOf(component.children,parts,list);
    QList<QPainterPath> outlines;for(const auto *p:parts)outlines<<shape(*p);
    // The free ends of lines and curves.
    QList<QPointF> ends;
    for(int k=0;k<parts.size();k++){
        const Item &p=*parts[k];
        if((p.type!=ItemType::Line&&p.type!=ItemType::Bezier)||p.points.size()<2||dot(p))continue;
        for(const QPointF e:{p.points.first(),p.points.last()}){
            const QRectF spot(e-QPointF(.05,.05),QSizeF(.1,.1));bool free=true;
            for(int j=0;j<parts.size()&&free;j++)if(j!=k&&!dot(*parts[j])&&outlines[j].intersects(spot))free=false;
            bool known=false;for(auto q:ends)known|=QLineF(q,e).length()<1e-6;
            if(free&&!known)ends<<e;
        }
    }
    // Contact and end, nearest first.
    struct Pair {double d;int contact,end;};
    QList<Pair> pairs;
    for(int c=0;c<list.size();c++){
        const Item &t=*list[c];const QPointF at=textFrame(t).map(textRect(t,t.text).center());
        for(int e=0;e<ends.size();e++){const double d=QLineF(at,ends[e]).length();if(d<=6)pairs.append({d,c,e});}
    }
    std::stable_sort(pairs.begin(),pairs.end(),[](const Pair &a,const Pair &b){return a.d<b.d;});
    QList<std::optional<QPointF>> out(list.size());QSet<int> used;
    for(const auto &p:pairs)if(!out[p.contact]&&!used.contains(p.end)){out[p.contact]=ends[p.end];used.insert(p.end);}
    return out;
}
int guessPins(Item &component){
    const auto guessed=guessedPins(component);
    QList<Item*> list;contactsOf(component.children,list);
    int count=0;
    for(int k=0;k<list.size()&&k<guessed.size();k++)if(!list[k]->hasPin&&guessed[k]){list[k]->pin=*guessed[k];list[k]->hasPin=true;count++;}
    return count;
}
QPointF gridPoint(const Item &item){
    switch(item.type){
    case ItemType::Line:case ItemType::Polygon:case ItemType::Bezier:case ItemType::Dimension:return item.points.isEmpty()?QPointF():item.points.first();
    case ItemType::Rectangle:case ItemType::Image:case ItemType::TextBox:return corners(item)[3];
    case ItemType::Ellipse:{
        // The points of the outline at half and a quarter turn, as sPlan keeps it.
        const double a=qDegreesToRadians(item.rotation);
        const QPointF u(std::cos(a),-std::sin(a)),v(-std::sin(a),-std::cos(a));
        return QPointF((item.centre-u*item.size.width()/2).x(),(item.centre+v*item.size.height()/2).y());
    }
    case ItemType::Group:{
        bool first=true;QPointF best;
        for(const auto &c:item.children){
            if(c.type==ItemType::Contact)continue;
            const QPointF p=gridPoint(c);
            if(first){best=p;first=false;continue;}
            if(std::nearbyint(p.x()*10)<=std::nearbyint(best.x()*10)&&std::nearbyint(p.y()*10)<std::nearbyint(best.y()*10))best=p;
        }
        return best;
    }
    default:return item.pos;
    }
}
void align(const QList<Item*> &items,Alignment how){
    QRectF all;for(const Item *i:items)all|=bounds(*i);
    for(Item *i:items){
        const QRectF b=bounds(*i);QPointF d;
        switch(how){
        case Alignment::Top:d.setY(all.top()-b.top());break;
        case Alignment::Bottom:d.setY(all.bottom()-b.bottom());break;
        case Alignment::Left:d.setX(all.left()-b.left());break;
        case Alignment::Right:d.setX(all.right()-b.right());break;
        case Alignment::HorizontalCentre:d.setX(all.center().x()-b.center().x());break;
        case Alignment::VerticalCentre:d.setY(all.center().y()-b.center().y());break;
        }
        move(*i,d);
    }
}
void spread(const QList<Item*> &items,bool horizontally){
    if(items.size()<3)return;
    auto middle=[&](const Item *i){const QPointF c=bounds(*i).center();return horizontally?c.x():c.y();};
    QList<Item*> order=items;std::stable_sort(order.begin(),order.end(),[&](const Item *a,const Item *b){return middle(a)<middle(b);});
    const double first=middle(order.first()),step=(middle(order.last())-first)/double(order.size()-1);
    for(int k=1;k+1<order.size();k++){const double d=first+k*step-middle(order[k]);move(*order[k],horizontally?QPointF(d,0):QPointF(0,d));}
}
void scale(Item &item,QPointF origin,double f){
    if(item.type==ItemType::Component){
        // The insertion point moves; the parts grow about it in the component's own coordinates.
        item.pos=origin+(item.pos-origin)*f;
        for(auto &c:item.children)scale(c,QPointF(),f);
        return;
    }
    transformPoints(item,[origin,f](QPointF p){return origin+(p-origin)*f;});
    item.size*=f;item.font.height*=f;item.endSize*=f;
    if(item.type==ItemType::Dimension){item.offset*=f;item.arrowLength*=f;}
    for(auto &c:item.children)scale(c,origin,f);
}
void stretch(Item &item,QPointF origin,double sx,double sy,double kx,double ky){
    const auto map=[&](QPointF p){const QPointF d=p-origin;return origin+QPointF(d.x()*sx+kx*d.y(),d.y()*sy+ky*d.x());};
    const double mean=std::sqrt(std::abs(sx*sy));
    if(item.type==ItemType::Component){
        item.pos=map(item.pos);
        if(std::abs(mean-1)>1e-12)for(auto &c:item.children)scale(c,QPointF(),mean);
        return;
    }
    transformPoints(item,map);
    if(item.type==ItemType::Rectangle||item.type==ItemType::Ellipse||item.type==ItemType::TextBox||item.type==ItemType::Image){
        if(std::abs(std::remainder(item.rotation,90.))<1e-9){
            const bool across=std::abs(std::remainder(item.rotation,180.))>1e-9;   // turned by 90° or 270°
            item.size=QSizeF(item.size.width()*std::abs(across?sy:sx),item.size.height()*std::abs(across?sx:sy));
        }else item.size*=mean;
    }else item.size*=mean;
    item.font.height*=mean;item.endSize*=mean;
    if(item.type==ItemType::Dimension){item.offset*=mean;item.arrowLength*=mean;}
    for(auto &c:item.children)stretch(c,origin,sx,sy,kx,ky);
}
void mirrorVertically(Item &item,double axis,bool textsToo){
    if(item.type==ItemType::Component){
        item.pos.setY(2*axis-item.pos.y());item.mirrored=!item.mirrored;item.rotation=normalized(180-item.rotation);return;
    }
    transformPoints(item,[axis](QPointF p){return QPointF(p.x(),2*axis-p.y());});
    if(item.type==ItemType::Dimension)item.offset=-item.offset;
    if(item.type==ItemType::Rectangle||item.type==ItemType::TextBox||item.type==ItemType::Image)item.rotation=normalized(-item.rotation);
    if(item.type==ItemType::Ellipse){
        item.rotation=normalized(-item.rotation);
        const double s=item.start,e=item.stop;item.start=normalized(-e);item.stop=item.start+std::fmod(e-s+720,360.);
        if(e-s>=360)item.stop=item.start+360;
    }
    if(isText(item)){
        item.rotation=normalized(180-item.rotation);
        if(textsToo)item.mirrored=!item.mirrored;else item.align=swapped(item.align);
    }
    for(auto &c:item.children)mirrorVertically(c,axis,textsToo);
}
Item placedText(const Item &text,const Item &component){
    // Letters inside a component stay readable: a mirrored component mirrors the place of a text and the side it
    // extends to, not its letters.
    Item t=text;t.pos=placement(component).map(text.pos);
    if(component.mirrored){t.rotation=normalized(component.rotation-text.rotation);t.align=swapped(text.align);}
    else t.rotation=normalized(component.rotation+text.rotation);
    t.pin=placement(component).map(text.pin);
    return t;
}

// --- components
namespace {
void collect(const QList<Item> &items,QList<const Item*> &out){
    for(const auto &i:items){
        if(i.type==ItemType::Component)out.append(&i);
        else if(i.type==ItemType::Group)collect(i.children,out);
    }
}
void collect(QList<Item> &items,QList<Item*> &out){
    for(auto &i:items){
        if(i.type==ItemType::Component)out.append(&i);
        else if(i.type==ItemType::Group)collect(i.children,out);
    }
}
}
QList<const Item*> components(const Sheet &sheet){QList<const Item*> out;collect(sheet.items,out);return out;}
QList<Item*> components(Sheet &sheet){QList<Item*> out;collect(sheet.items,out);return out;}
QList<const Item*> contacts(const Item &component){
    QList<const Item*> out;for(const auto &c:component.children)if(c.type==ItemType::Contact)out.append(&c);return out;
}
const Item *componentOf(const Sheet &sheet,const QString &id){
    for(const auto *c:components(sheet)){
        if(c->id==id)return c;
        for(const auto &child:c->children)if(child.id==id)return c;
    }
    return nullptr;
}
Item makeComponent(QList<Item> children,QPointF pos,const QString &designator,const QString &value,const Item *designatorPreset,const Item *valuePreset){
    Item c;c.type=ItemType::Component;c.pos=pos;c.designator=designator;c.value=value;c.extra={QString(),QString(),QString(),QString()};
    bool hasDesignator=false,hasValue=false;
    for(const auto &i:children){hasDesignator|=i.type==ItemType::Text&&i.role==TextRole::Designator;hasValue|=i.type==ItemType::Text&&i.role==TextRole::Value;}
    QRectF box;for(const auto &i:children)if(!isText(i))box=box.united(bounds(i));
    if(box.isNull())box=QRectF(-2,-2,4,4);
    if(designatorPreset&&valuePreset){
        const double hv=valuePreset->font.height,hd=designatorPreset->font.height;
        if(!hasDesignator){Item t;t.type=ItemType::Text;t.role=TextRole::Designator;t.font=designatorPreset->font;t.pos=QPointF(box.left(),box.top()-hv-hd);children.append(t);}
        if(!hasValue){Item t;t.type=ItemType::Text;t.role=TextRole::Value;t.font=valuePreset->font;t.pos=QPointF(box.left(),box.top()-hv);children.append(t);}
    }else{
        if(!hasDesignator){Item t;t.type=ItemType::Text;t.role=TextRole::Designator;t.pos=QPointF(box.right()+1,box.top());children.append(t);}
        if(!hasValue){Item t;t.type=ItemType::Text;t.role=TextRole::Value;t.pos=QPointF(box.right()+1,box.top()+3.5);children.append(t);}
    }
    c.children=children;
    return c;
}

// --- texts
QString shownText(const Item &item,const TextContext &context){
    if(context.component&&item.role==TextRole::Designator)return shownDesignator(*context.component,context);
    if(context.component&&item.role==TextRole::Value)return expandVariables(context.component->value,context);
    TextContext own=context;own.text=&item;
    return expandVariables(item.text,own);
}
namespace {
void gather(const QList<Item> &items,int sheet,QList<PlacedComponent> &out){
    for(const auto &i:items){
        if(i.type==ItemType::Component)out.append({sheet,&i});
        else if(i.type==ItemType::Group)gather(i.children,sheet,out);
    }
}
}
QList<PlacedComponent> allComponents(const Document &document){
    QList<PlacedComponent> out;for(int s=0;s<document.sheets.size();s++)gather(document.sheets[s].items,s,out);return out;
}
PlacedComponent componentWithId(const Document &document,const QString &id){
    if(id.isEmpty())return {};
    for(const auto &p:allComponents(document))if(p.item->id==id)return p;
    return {};
}
QList<PlacedComponent> childrenOf(const Document &document,const QString &parentId){
    QList<PlacedComponent> out;if(parentId.isEmpty())return out;
    for(const auto &p:allComponents(document))if(p.item->parentId==parentId)out<<p;
    return out;
}
namespace {
void gatherTexts(const QList<Item> &items,int sheet,QList<PlacedComponent> &out){
    for(const auto &i:items){
        if(i.type==ItemType::Text)out.append({sheet,&i});
        else if(i.type==ItemType::Group)gatherTexts(i.children,sheet,out);
    }
}
}
QList<PlacedComponent> allTexts(const Document &document){
    QList<PlacedComponent> out;for(int s=0;s<document.sheets.size();s++)gatherTexts(document.sheets[s].items,s,out);return out;
}
PlacedComponent textWithId(const Document &document,const QString &id){
    if(id.isEmpty())return {};
    for(const auto &p:allTexts(document))if(p.item->id==id)return p;
    return {};
}
QList<PlacedComponent> linksTo(const Document &document,const QString &id){
    QList<PlacedComponent> out;if(id.isEmpty())return out;
    for(const auto &p:allTexts(document))if(p.item->linkTarget==id)out<<p;
    return out;
}
int columnAt(const Sheet &sheet,QPointF p){
    const TitleBlock &t=sheet.titleBlock;if(t.columns<=0||t.frame.width()<=0)return 0;
    const double x=(p.x()-t.frame.left())/t.frame.width();if(x<0||x>=1)return 0;
    return int(x*t.columns)+t.columnStart;
}
int rowAt(const Sheet &sheet,QPointF p){
    const TitleBlock &t=sheet.titleBlock;if(t.rows<=0||t.frame.height()<=0)return 0;
    const double y=(p.y()-t.frame.top())/t.frame.height();if(y<0||y>=1)return 0;
    return int(y*t.rows)+t.rowStart;
}
void autoGrid(Sheet &sheet){
    TitleBlock &t=sheet.titleBlock;
    t.frame=QRectF(10,10,std::max(0.,sheet.width-20),std::max(0.,sheet.height-20));
    auto count=[](double length){return int(std::clamp(std::nearbyint(length/23),1.,999.));};
    t.columns=count(t.frame.width());t.rows=count(t.frame.height());t.columnStart=t.rowStart=1;
}
namespace {
// The letters of a column or row of the title block's grid, as the reference: A to Z, then AA, AB … ZZ, AAA … (at most
// three letters); 0, left of or above the grid, is "@". With a start of 0 for columns or rows both count one more.
QString gridLetter(const Sheet &sheet,int n){
    if(sheet.titleBlock.columnStart==0||sheet.titleBlock.rowStart==0)n++;
    QString out(QChar(char16_t(u'A'+(n-1)%26)));
    int q=(n-1)/26;
    if(q>0){out.prepend(QChar(char16_t(u'A'+(q-1)%26)));q=(q-1)/26;if(q>0)out.prepend(QChar(char16_t(u'A'+(q-1)%26)));}
    return out;
}
// A child's letter: a to z, then Aa … Az, Ba … (as the reference).
QString childLetter(int number){
    const int i=number-1;if(i<0)return {};
    return i<26?QString(QChar(char16_t(u'a'+i))):QString(QChar(char16_t(u'@'+i/26)))+QChar(char16_t(u'a'+i%26));
}
QString variable(const QString &name,const TextContext &c,int depth);
QString expand(const QString &text,const TextContext &c,int depth);
// A component's designator as shown: prefix, sheet number and designator when the document asks for it and the
// component has no parent (a child whose parent is missing counts as without one).
QString designatorAt(const Item &k,const TextContext &c,int depth){
    const QString own=expand(k.designator,c,depth);
    const Document *d=c.document;
    if(!d||!d->designatorPageNumbers||c.sheet<0||c.sheet>=d->sheets.size())return own;
    if(!k.parentId.isEmpty()&&componentWithId(*d,k.parentId).item)return own;
    return d->designatorPrefix+QString::number(c.sheet+1)+own;
}
QString expand(const QString &text,const TextContext &c,int depth){
    static const QRegularExpression pattern(QStringLiteral("<([^<>\\n]{1,64})>"));
    // Never a null string: null stands for "unknown name" in variable().
    if(depth>8||!text.contains(u'<'))return text.isNull()?QString(u""_s):text;
    QString out;qsizetype last=0;
    for(auto it=pattern.globalMatch(text);it.hasNext();){
        const auto m=it.next();out+=text.mid(last,m.capturedStart()-last);
        const QString value=variable(m.captured(1),c,depth);
        out+=value.isNull()?m.captured(0):value;last=m.capturedEnd();
    }
    return out+text.mid(last);
}
QString variable(const QString &name,const TextContext &c,int depth){
    const QString n=name.toUpper();
    const Document *d=c.document;const QFileInfo file(c.fileName);
    if(d&&n==u"PAGENO")return QString::number(c.sheet+1);
    if(d&&n==u"PAGECOUNT")return QString::number(d->sheets.size());
    if(d&&n==u"PAGENAME"&&c.sheet>=0&&c.sheet<d->sheets.size())return d->sheets[c.sheet].name;
    if(d&&n==u"PAGESCALE"&&c.sheet>=0&&c.sheet<d->sheets.size()){
        // The millimetres one millimetre of the sheet stands for, as a plain number ("2,5", "1000").
        const Sheet &s=d->sheets[c.sheet];QLocale l=uiLocale();l.setNumberOptions(QLocale::OmitGroupSeparator);
        return l.toString(s.scale*unitMillimetres(s.scaleUnit),'g',15);
    }
    if(d&&(n==u"NEXT_PAGENO"||n==u"PREVIOUS_PAGENO")&&c.sheet>=0&&c.sheet<d->sheets.size()){
        // The number of the next or previous sheet that is not a spare sheet, "-" without one.
        const int step=n==u"NEXT_PAGENO"?1:-1;int i=c.sheet+step;
        while(i>=0&&i<d->sheets.size()&&d->sheets[i].spare)i+=step;
        return i>=0&&i<d->sheets.size()?QString::number(i+1):QStringLiteral("-");
    }
    if(n==u"FILENAME")return c.fileName.isEmpty()?QString(""):file.fileName();
    if(n==u"FILENAME_PURE")return c.fileName.isEmpty()?QString(""):file.completeBaseName();
    if(n==u"FILEPATH")return c.fileName.isEmpty()?QString(""):QDir::toNativeSeparators(file.absolutePath());
    if(n==u"FILEDATE")return file.exists()?uiDate(file.lastModified().date()):QString("");
    if(n==u"FILETIME")return file.exists()?uiTime(file.lastModified().time()):QString("");
    if(n==u"DATE")return uiDate(QDate::currentDate());
    if(n==u"TIME")return uiTime(QTime::currentTime());
    if(n==u"VERSION")return QStringLiteral("OpenLoch");
    if(c.component){
        const Item &k=*c.component;
        if(n==u"BEZ"||n==u"ID")return designatorAt(k,c,depth+1);
        if(n==u"WERT"||n==u"VALUE"||n==u"VALEUR")return expand(k.value,c,depth+1);
        for(int i=0;i<4;i++)if(n==QStringLiteral("Z%1").arg(i+1))return i<k.extra.size()?expand(k.extra[i],c,depth+1):QString("");
        // The grid of the title block at a component.
        auto grid=[&](const QString &key,int sheet,QPointF at)->QString{
            if(!d||sheet<0||sheet>=d->sheets.size())return {};
            const Sheet &s=d->sheets[sheet];
            if(key==u"COLNUM")return QString::number(columnAt(s,at));if(key==u"COLCHAR")return gridLetter(s,columnAt(s,at));
            if(key==u"ROWNUM")return QString::number(rowAt(s,at));if(key==u"ROWCHAR")return gridLetter(s,rowAt(s,at));
            return {};
        };
        if(const QString g=grid(n,c.sheet,k.pos);!g.isNull())return g;
        if(d&&n.startsWith(u"PARENT_")&&!k.parentId.isEmpty()){
            // A child: the fields of its parent, in the parent's own context.
            const PlacedComponent p=componentWithId(*d,k.parentId);if(!p.item)return {};
            const TextContext pc{d,p.sheet,p.item,c.fileName};const QString rest=n.mid(7);
            if(rest==u"ID")return designatorAt(*p.item,pc,depth+1);
            if(rest==u"VALUE")return expand(p.item->value,pc,depth+1);
            if(rest==u"PAGENO")return QString::number(p.sheet+1);
            if(rest==u"ID_NUMBER"){
                // The number at the end of the parent's own designator, without leading zeros; 0 without one.
                const QString id=expand(p.item->designator,pc,depth+1);qsizetype at=id.size();
                while(at>0&&id[at-1]>=u'0'&&id[at-1]<=u'9')at--;
                return QString::number(id.mid(at).toLongLong());
            }
            if(rest==u"PAGENAME"){
                // The name of the parent's sheet with the user variables in it filled in.
                QString name=d->sheets[p.sheet].name;
                if(name.contains(u'<')&&name.contains(u'>'))
                    for(const auto &v:d->variables)name.replace(u'<'+v.name+u'>',v.value,Qt::CaseInsensitive);
                return name;
            }
            for(int i=0;i<4;i++)if(rest==QStringLiteral("Z%1").arg(i+1))return expand(p.item->extra.value(i),pc,depth+1);
            if(rest.startsWith(u"CONTACT_")){const int i=rest.mid(8).toInt();const auto list=contacts(*p.item);return i>=1&&i<=list.size()?list[i-1]->text:QString(u""_s);}
            return grid(rest,p.sheet,p.item->pos);
        }
        if(d&&(n==u"CHILDNO"||n==u"CHILDCHAR")&&!k.parentId.isEmpty()){
            const auto list=childrenOf(*d,k.parentId);int number=0;
            for(int i=0;i<list.size();i++)if(list[i].item==&k)number=i+1;
            return n==u"CHILDNO"?QString::number(number):childLetter(number);
        }
        if(d&&n.startsWith(u"CHILD_")){
            // A parent: the fields of its first child, or of the n-th with "_n" at the end.
            QString rest=n.mid(6);int index=1;
            static const QRegularExpression numbered(QStringLiteral("^(.*)_(\\d+)$"));
            if(const auto m=numbered.match(rest);m.hasMatch()){rest=m.captured(1);index=m.captured(2).toInt();}
            const auto list=childrenOf(*d,k.id);
            if(index<1||index>list.size())return QString(u""_s);
            const PlacedComponent &child=list[index-1];
            if(rest==u"PAGENO")return QString::number(child.sheet+1);
            if(rest==u"PAGENAME")return d->sheets[child.sheet].name;
            return grid(rest,child.sheet,child.item->pos);
        }
    }
    if(d&&c.text&&(n.startsWith(u"LINK_")||n.startsWith(u"LINKFROM_"))){
        // A linked text: its target's sheet, name, text and grid; a target: of the first text linking to it.
        const bool from=n.startsWith(u"LINKFROM_");const QString rest=n.mid(from?9:5);
        PlacedComponent other;
        if(from){const auto list=linksTo(*d,c.text->id);if(!list.isEmpty())other=list.first();}
        else other=textWithId(*d,c.text->linkTarget);
        if(!other.item)return QString(u""_s);
        if(rest==u"PAGENO")return QString::number(other.sheet+1);
        if(rest==u"PAGENAME")return d->sheets[other.sheet].name;
        if(rest==u"TEXT"){TextContext oc{d,other.sheet,nullptr,c.fileName,nullptr};return expand(other.item->text,oc,depth+1);}
        const Sheet &s=d->sheets[other.sheet];
        if(rest==u"COLNUM")return QString::number(columnAt(s,other.item->pos));if(rest==u"COLCHAR")return gridLetter(s,columnAt(s,other.item->pos));
        if(rest==u"ROWNUM")return QString::number(rowAt(s,other.item->pos));if(rest==u"ROWCHAR")return gridLetter(s,rowAt(s,other.item->pos));
        return {};
    }
    if(d)for(const auto &v:d->variables)if(v.name.compare(name,Qt::CaseInsensitive)==0)return expand(v.value,c,depth+1);
    return {};
}
}
QString expandVariables(const QString &text,const TextContext &context){return expand(text,context,0);}
QString shownDesignator(const Item &component,const TextContext &context){return designatorAt(component,context,0);}
}
