#include "render.h"
#include "dimension.h"
#include "text.h"
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <cmath>
#include <numbers>

namespace openloch::schematic {
namespace {
struct Context {
    const Document &document;
    int sheet;
    const RenderOptions &options;
    const Sheet *sheetData;
    bool highlight=false;
    QColor colour(const QColor &c) const{
        if(highlight)return options.highlightColor;
        return options.blackAndWhite?QColor(0,0,0):c;
    }
    QColor fillColour(const QColor &c) const{
        if(options.blackAndWhite)return c.lightness()>200?QColor(255,255,255):QColor(0,0,0);
        return c;
    }
    TextContext text(const Item *component=nullptr) const{return {&document,sheet,component,options.fileName};}
    QColor paper() const{return options.paperColour;}
};

// The dash patterns of the reference, in pen widths; each dash also gets its round ends, the first starts the line.
QList<qreal> dashes(PenStyle s){
    switch(s){
    case PenStyle::Dot:return {2,4};
    case PenStyle::Dash:return {8,5};
    case PenStyle::DashDot:return {2,4,8,4};
    case PenStyle::DashDotDot:return {2,4,2,4,8,4};
    default:return {};
    }
}
// Cross stripes: a dash of this length (in pen widths, without its round ends) at the start of every period.
constexpr double crossMark=.87,crossPeriod=6.4;
QPen solidPen(const QColor &colour,double width){
    QPen pen(colour,width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
    if(width<=0)pen.setCosmetic(true);
    return pen;
}
QPen qtPen(const Pen &p,const Context &c){
    QPen pen=solidPen(c.colour(p.color),p.width);
    if(p.style==PenStyle::None)pen.setStyle(Qt::NoPen);
    else if(p.style!=PenStyle::Solid)pen.setDashPattern(dashes(p.style));
    return pen;
}
// An outline as the reference draws it: the second colour under a dashed line, the line, the stripe along its middle
// and the stripes across it. In black and white only the line itself.
void stroke(QPainter &painter,const QPainterPath &path,const Pen &p,const Context &c){
    if(p.style==PenStyle::None||path.isEmpty())return;
    painter.setBrush(Qt::NoBrush);
    const bool extras=!c.options.blackAndWhite&&p.width>0;
    if(extras&&p.twoColour&&p.style!=PenStyle::Solid){painter.setPen(solidPen(c.colour(p.color2),p.width));painter.drawPath(path);}
    painter.setPen(qtPen(p,c));painter.drawPath(path);
    if(extras&&p.inner){painter.setPen(solidPen(c.colour(p.innerColor),p.width/2));painter.drawPath(path);}
    if(extras&&p.cross){QPen q=solidPen(c.colour(p.crossColor),p.width);q.setDashPattern({crossMark,crossPeriod-crossMark});painter.setPen(q);painter.drawPath(path);}
}
// Hatches as the reference draws them: lines `lineWidth` wide with `spacing` between them, laid out in the upright
// `box`: the first a full step below its top or right of its left edge, diagonals through its top left ("/") or top
// right ("\") corner. Drawn as lines, so that their spacing and width stay true at every scale.
void paintFill(QPainter &painter,const QPainterPath &area,const Fill &fill,const QRectF &box,const Context &c){
    if(fill.style==FillStyle::None||area.isEmpty())return;
    if(fill.style==FillStyle::Solid){painter.fillPath(area,c.fillColour(fill.color));return;}
    painter.save();painter.setClipPath(area,Qt::IntersectClip);
    QPen pen(c.colour(fill.color),fill.lineWidth,Qt::SolidLine,Qt::FlatCap);if(fill.lineWidth<=0)pen.setCosmetic(true);painter.setPen(pen);
    const QRectF &r=box;const double step=std::max(fill.spacing+fill.lineWidth,.05),diagonal=step*std::numbers::sqrt2,beyond=fill.lineWidth;
    auto horizontal=[&]{for(double y=r.top()+step;y<r.bottom()+beyond;y+=step)painter.drawLine(QPointF(r.left(),y),QPointF(r.right(),y));};
    auto vertical=[&]{for(double x=r.left()+step;x<r.right()+beyond;x+=step)painter.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));};
    // "/" on x + y = s, "\" on y − x = s.
    auto rising=[&]{for(double s=r.left()+r.top()+diagonal;s<r.right()+r.bottom()+2*beyond;s+=diagonal)painter.drawLine(QPointF(s-r.top(),r.top()),QPointF(s-r.bottom(),r.bottom()));};
    auto falling=[&]{for(double s=r.top()-r.right()+diagonal;s<r.bottom()-r.left()+2*beyond;s+=diagonal)painter.drawLine(QPointF(r.top()-s,r.top()),QPointF(r.bottom()-s,r.bottom()));};
    switch(fill.style){
    case FillStyle::Horizontal:horizontal();break;
    case FillStyle::Vertical:vertical();break;
    case FillStyle::Cross:horizontal();vertical();break;
    case FillStyle::Diagonal:rising();break;
    case FillStyle::BackDiagonal:falling();break;
    case FillStyle::DiagonalCross:rising();falling();break;
    default:break;
    }
    painter.restore();
}
// The box hatches are laid out in: the bounds of the element (of the whole ellipse also for an arc), grown by half its
// outline; upright also when the element is turned.
QRectF hatchBox(const Item &item){
    QRectF r;
    if(item.type==ItemType::Ellipse&&item.arc!=ArcStyle::Full){Item whole=item;whole.arc=ArcStyle::Full;r=path(whole).boundingRect();}
    else r=path(item).boundingRect();
    const double o=item.pen.style==PenStyle::None?0:std::max(0.,item.pen.width)/2;
    return r.adjusted(-o,-o,o,o);
}
void paintEnd(QPainter &painter,LineEnd end,QPointF tip,QPointF from,double size,const QPen &pen,const QColor &paper){
    if(end==LineEnd::None)return;
    QPointF d=tip-from;const double l=std::hypot(d.x(),d.y());if(l<1e-9)return;d/=l;
    // Arrows are `size` long and a little more than half as wide; marks are half as big, centred on the end.
    const QPointF n(-d.y(),d.x());const double w=size*.28,m=size/4;
    painter.save();QPen p=pen;p.setStyle(Qt::SolidLine);p.setJoinStyle(Qt::MiterJoin);painter.setPen(p);
    const QBrush filled(pen.color()),hollow(paper);
    const QPolygonF arrow{tip,tip-d*size+n*w,tip-d*size-n*w},back{tip-d*size,tip+n*w,tip-n*w};
    const QPolygonF diamond{tip+d*m,tip+n*m,tip-d*m,tip-n*m},square{tip+(n-d)*m,tip+(n+d)*m,tip+(-n+d)*m,tip+(-n-d)*m};
    switch(end){
    case LineEnd::Triangle:painter.setBrush(hollow);painter.drawPolygon(arrow);break;
    case LineEnd::Arrow:painter.setBrush(filled);painter.drawPolygon(arrow);break;
    case LineEnd::OpenArrow:painter.drawPolyline(QPolygonF{tip-d*size+n*w,tip,tip-d*size-n*w});break;
    case LineEnd::Circle:painter.setBrush(hollow);painter.drawEllipse(tip,m,m);break;
    case LineEnd::Dot:painter.setBrush(filled);painter.drawEllipse(tip,m,m);break;
    case LineEnd::Diamond:painter.setBrush(hollow);painter.drawPolygon(diamond);break;
    case LineEnd::FilledDiamond:painter.setBrush(filled);painter.drawPolygon(diamond);break;
    case LineEnd::Square:painter.setBrush(hollow);painter.drawPolygon(square);break;
    case LineEnd::FilledSquare:painter.setBrush(filled);painter.drawPolygon(square);break;
    case LineEnd::Bar:painter.drawLine(tip+n*w,tip-n*w);break;
    case LineEnd::BackArrow:painter.setBrush(filled);painter.drawPolygon(back);break;
    case LineEnd::BackTriangle:painter.setBrush(hollow);painter.drawPolygon(back);break;
    case LineEnd::None:break;
    }
    painter.restore();
}
void paintText(QPainter &painter,const Item &t,const QString &shown,const Context &c){
    if(shown.isEmpty())return;
    // An external link is underlined.
    Font font=t.font;if(!t.link.isEmpty())font.underline=true;
    const auto layout=layoutText(shown,font);const double scale=textScale(font);
    painter.save();painter.setTransform(textFrame(t),true);
    const QRectF box=textRect(t,shown);
    if(t.background)painter.fillRect(box,c.fillColour(t.backgroundColor));
    if(c.options.linkMarks&&(t.linkable||!t.linkTarget.isEmpty())){
        painter.setPen(Qt::NoPen);painter.setBrush(QColor(30,90,200));
        if(t.linkable)painter.drawPolygon(linkArrow(box,false));
        if(!t.linkTarget.isEmpty())painter.drawPolygon(linkArrow(box,true));
    }
    // "Textrichtung umkehren": the writing turned by 180° inside the same frame.
    if(t.reversed&&t.type==ItemType::Text){painter.translate(box.center());painter.rotate(180);painter.translate(-box.center());}
    painter.setFont(qtFont(font));painter.setPen(c.colour(t.font.color));painter.scale(scale,scale);
    for(int i=0;i<layout.lines.size();i++){
        const double w=layout.widths[i];
        const double x=t.align==Align::Left?0:t.align==Align::Centre?-w/2:-w;
        painter.drawText(QPointF(x/scale,(i*layout.lineHeight+layout.ascent)/scale),layout.lines[i]);
    }
    painter.restore();
}
QImage picture(const Document &document,const QString &key){
    static QHash<QString,QImage> cache;
    if(!cache.contains(key)){
        if(cache.size()>64)cache.clear();
        const auto it=document.resources.constFind(key);
        cache.insert(key,it==document.resources.cend()?QImage():QImage::fromData(it->data));
    }
    return cache.value(key);
}
void paintItem(QPainter &painter,const Item &item,const Context &parent,const Item *component=nullptr);
void paintLabel(QPainter &painter,const Item &label,const Context &c){
    paintText(painter,label,label.text,c);
    if(label.global){
        // A sheet reference: the name in a frame that points away from the connection.
        const QRectF r=textRect(label,label.text).adjusted(-.6,-.3,.6,.3);const double h=r.height()/2;
        QPolygonF frame=label.align==Align::Right?QPolygonF{r.topLeft(),QPointF(r.right()+h,r.top()),QPointF(r.right()+h*2,r.center().y()),QPointF(r.right()+h,r.bottom()),r.bottomLeft()}
                                                 :QPolygonF{QPointF(r.left()-h*2,r.center().y()),QPointF(r.left()-h,r.top()),r.topRight(),r.bottomRight(),QPointF(r.left()-h,r.bottom())};
        painter.save();painter.setTransform(textFrame(label),true);painter.setPen(QPen(c.colour(label.font.color),label.font.height*.06));painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(frame);painter.restore();
    }
}
void paintItem(QPainter &painter,const Item &item,const Context &parent,const Item *component){
    Context c=parent;c.highlight=parent.highlight||parent.options.highlighted.contains(item.id);
    switch(item.type){
    case ItemType::Line:{
        QPainterPath line;line.addPolygon(item.points);stroke(painter,line,item.pen,c);
        const auto pen=qtPen(item.pen,c);
        if(item.points.size()>=2){
            paintEnd(painter,item.startEnd,item.points.first(),item.points[1],item.endSize,pen,c.paper());
            paintEnd(painter,item.endEnd,item.points.last(),item.points[item.points.size()-2],item.endSize,pen,c.paper());
        }
        break;
    }
    case ItemType::Bezier:{
        stroke(painter,path(item),item.pen,c);const auto pen=qtPen(item.pen,c);
        const int n=int(item.points.size());
        if(n>=4){paintEnd(painter,item.startEnd,item.points[0],item.points[1],item.endSize,pen,c.paper());paintEnd(painter,item.endEnd,item.points[n-1],item.points[n-2],item.endSize,pen,c.paper());}
        break;
    }
    case ItemType::Polygon:case ItemType::Rectangle:case ItemType::Ellipse:{
        const auto p=path(item);
        if(item.type==ItemType::Ellipse&&item.arc==ArcStyle::Arc){
            // A filled arc fills the segment up to its chord; only the arc has an outline.
            QPainterPath segment=p;segment.closeSubpath();paintFill(painter,segment,item.fill,hatchBox(item),c);
        }else paintFill(painter,p,item.fill,hatchBox(item),c);
        stroke(painter,p,item.pen,c);
        break;
    }
    case ItemType::TextBox:{
        const auto p=path(item);paintFill(painter,p,item.fill,hatchBox(item),c);stroke(painter,p,item.pen,c);
        const QString shown=shownText(item,c.text(component));const double scale=textScale(item.font);
        painter.save();painter.translate(item.centre);painter.rotate(-item.rotation);painter.scale(scale,scale);
        painter.setFont(qtFont(item.font));painter.setPen(c.colour(item.font.color));
        const double w=item.size.width()/scale,h=item.size.height()/scale,margin=item.font.height*.3/scale;
        const int align=(item.align==Align::Left?Qt::AlignLeft:item.align==Align::Centre?Qt::AlignHCenter:Qt::AlignRight)|(item.middle?Qt::AlignVCenter:Qt::AlignTop)|(item.wrap?Qt::TextWordWrap:0);
        painter.drawText(QRectF(-w/2+margin,-h/2+margin,w-2*margin,h-2*margin),align,shown);
        painter.restore();
        break;
    }
    case ItemType::Junction:{
        const auto [size,colour]=c.sheetData?junctionLook(item,*c.sheetData):std::pair<double,QColor>(item.size.width(),item.pen.color);
        painter.setPen(Qt::NoPen);painter.setBrush(c.colour(colour));painter.drawEllipse(item.pos,size/2,size/2);
        break;
    }
    case ItemType::Image:{
        const QImage image=picture(c.document,item.resource);
        painter.save();painter.translate(item.centre);painter.rotate(-item.rotation);
        const QRectF r(-item.size.width()/2,-item.size.height()/2,item.size.width(),item.size.height());
        if(image.isNull()){painter.setPen(QPen(c.colour(QColor(128,128,128)),.2));painter.setBrush(Qt::NoBrush);painter.drawRect(r);painter.drawLine(r.topLeft(),r.bottomRight());painter.drawLine(r.topRight(),r.bottomLeft());}
        else painter.drawImage(r,c.options.blackAndWhite?image.convertToFormat(QImage::Format_Grayscale8):image);
        painter.restore();
        break;
    }
    case ItemType::Text:case ItemType::Contact:{
        if(!item.visible)break;
        if(component){
            if(item.role==TextRole::Designator&&!component->designatorVisible)break;
            if(item.role==TextRole::Value&&!component->valueVisible)break;
        }
        const Item placed=component?placedText(item,*component):item;
        paintText(painter,placed,shownText(item,c.text(component)),c);
        if(item.type==ItemType::Contact&&item.hasPin&&c.options.pins){
            painter.save();painter.setPen(QPen(c.colour(QColor(255,0,0)),.1));painter.setBrush(Qt::NoBrush);
            const QPointF p=placed.pin;painter.drawLine(p-QPointF(.5,.5),p+QPointF(.5,.5));painter.drawLine(p-QPointF(.5,-.5),p+QPointF(.5,-.5));
            painter.restore();
        }
        break;
    }
    case ItemType::NetLabel:paintLabel(painter,item,c);break;
    case ItemType::Dimension:{
        const auto d=dimensionDrawing(item,c.sheetData?c.sheetData->scale:1);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(c.colour(item.extensionColor),dimensionLineWidth,Qt::SolidLine,Qt::FlatCap));for(const auto &e:d.extensions)painter.drawLine(e);
        painter.setPen(QPen(c.colour(item.lineColor),dimensionLineWidth,Qt::SolidLine,Qt::FlatCap));painter.drawPath(d.line);
        painter.setPen(Qt::NoPen);painter.setBrush(c.colour(item.lineColor));for(const auto &a:d.arrows)painter.drawPolygon(a);
        for(const auto &t:d.texts)paintText(painter,t,t.text,c);
        break;
    }
    case ItemType::Group:for(const auto &child:item.children)paintItem(painter,child,c,component);break;
    case ItemType::Component:{
        if(c.options.parentChild&&(item.parent||!item.parentId.isEmpty())&&!c.options.blackAndWhite){
            const bool strong=c.options.related.contains(item.id);
            QColor fill=item.parent?QColor(150,190,255):QColor(255,160,160);fill.setAlpha(strong?170:70);
            painter.fillRect(bounds(item).adjusted(-.6,-.6,.6,.6),fill);
        }
        for(const auto &child:item.children){
            if(isText(child)){paintItem(painter,child,c,&item);continue;}
            painter.save();painter.setTransform(placement(item),true);paintItem(painter,child,c);painter.restore();
        }
        break;
    }
    }
}
// The grid of a title block's columns and rows, when it is to be shown.
void paintTitleGrid(QPainter &painter,const TitleBlock &t,const Context &c){
    if(!t.showGrid||t.frame.isEmpty()||(t.columns<=0&&t.rows<=0))return;
    painter.save();painter.setBrush(Qt::NoBrush);painter.setPen(QPen(c.colour(QColor(170,170,170)),0,Qt::DotLine));
    const QRectF f=t.frame;
    for(int i=1;i<t.columns;i++){const double x=f.left()+f.width()*i/t.columns;painter.drawLine(QPointF(x,f.top()),QPointF(x,f.bottom()));}
    for(int i=1;i<t.rows;i++){const double y=f.top()+f.height()*i/t.rows;painter.drawLine(QPointF(f.left(),y),QPointF(f.right(),y));}
    painter.restore();
}
}

QPolygonF linkArrow(const QRectF &box,bool outgoing){
    const double h=std::min(box.height(),3.),y=box.top()+box.height()/2;
    if(outgoing)return QPolygonF{QPointF(box.right()+.2,y-h/2),QPointF(box.right()+h*.9,y),QPointF(box.right()+.2,y+h/2)};
    return QPolygonF{QPointF(box.left()-h*.9,y-h/2),QPointF(box.left()-.2,y),QPointF(box.left()-h*.9,y+h/2)};
}
std::pair<double,QColor> junctionLook(const Item &junction,const Sheet &sheet){
    if(!junction.autoSize)return {junction.size.width(),junction.pen.color};
    // As the reference: the lines of the sheet (not those in groups or components, no curves) whose stroke covers the
    // junction's centre (on a tenth of a millimetre); the widest gives the size, the last the colour; without one 0.1 mm
    // and black. The size is that width times 3 to 7 for XS to XL.
    const QPointF at(std::round(junction.pos.x()*10)/10,std::round(junction.pos.y()*10)/10);
    double width=0;QColor colour(0,0,0);
    for(const auto &i:sheet.items){
        if(i.type!=ItemType::Line)continue;
        for(int k=0;k+1<i.points.size();k++){
            const QPointF a=i.points[k],d=i.points[k+1]-a;const double l=QPointF::dotProduct(d,d);
            const double t=l>0?std::clamp(QPointF::dotProduct(at-a,d)/l,0.,1.):0;const QPointF q=a+t*d;
            if(std::hypot(q.x()-at.x(),q.y()-at.y())<=i.pen.width/2+1e-9){width=std::max(width,i.pen.width);colour=i.pen.color;break;}
        }
    }
    if(width<=0)width=.1;
    return {width*(3+std::clamp(junction.sizeStep,0,4)),colour};
}
void paintItems(QPainter &painter,const QList<Item> &items,const Document &document,int sheet,const RenderOptions &options){
    const Sheet *s=sheet>=0&&sheet<document.sheets.size()?&document.sheets[sheet]:nullptr;
    Context c{document,sheet,options,s};
    painter.save();painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::TextAntialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for(const auto &i:items)paintItem(painter,i,c);
    painter.restore();
}
void paintSheet(QPainter &painter,const Document &document,int sheet,const RenderOptions &options){
    if(sheet<0||sheet>=document.sheets.size())return;
    const Sheet &s=document.sheets[sheet];
    Context c{document,sheet,options,&s};
    painter.save();painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::TextAntialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if(options.paper)painter.fillRect(QRectF(0,0,s.width,s.height),options.paperColour);
    if(options.titleBlock){for(const auto &i:s.titleBlock.items)paintItem(painter,i,c);paintTitleGrid(painter,s.titleBlock,c);}
    if(options.circuit)for(const auto &i:s.items)paintItem(painter,i,c);
    painter.restore();
}
QImage renderSheet(const Document &document,int sheet,double pixelsPerMm,const RenderOptions &options){
    if(sheet<0||sheet>=document.sheets.size())return {};
    const Sheet &s=document.sheets[sheet];
    QImage image(std::max(1,int(std::lround(s.width*pixelsPerMm))),std::max(1,int(std::lround(s.height*pixelsPerMm))),QImage::Format_ARGB32_Premultiplied);
    image.fill(options.paper?options.paperColour:QColor(0,0,0,0));
    image.setDotsPerMeterX(int(std::lround(pixelsPerMm*1000)));image.setDotsPerMeterY(int(std::lround(pixelsPerMm*1000)));
    QPainter painter(&image);painter.scale(pixelsPerMm,pixelsPerMm);paintSheet(painter,document,sheet,options);
    return image;
}
}
