#include "example.h"
#include "language.h"

namespace openloch::schematic {
namespace {
Item line(QPolygonF points,double width=.25){Item i;i.type=ItemType::Line;i.points=points;i.pen.width=width;i.electrical=false;return i;}
Item contact(const QString &number,QPointF label,QPointF pin,Align align=Align::Left){
    Item c;c.type=ItemType::Contact;c.name=number;c.text=number;c.pos=label;c.pin=pin;c.hasPin=true;c.align=align;c.font.height=1.8;return c;
}
Item text(const QString &content,QPointF pos,double height,Align align=Align::Left){
    Item t;t.type=ItemType::Text;t.text=content;t.pos=pos;t.font.height=height;t.align=align;return t;
}
}
Item exampleSymbol(){
    QList<Item> parts;
    parts<<line({QPointF(0,0),QPointF(2.54,0)});
    Item body;body.type=ItemType::Rectangle;body.centre=QPointF(7.62,0);body.size=QSizeF(10.16,5.08);body.pen.width=.35;parts<<body;
    Item mark;mark.type=ItemType::Polygon;mark.points={QPointF(3.3,-1.6),QPointF(4.8,-1.6),QPointF(3.3,-.1)};mark.fill.style=FillStyle::Solid;mark.fill.color=QColor(0,0,0);parts<<mark;
    parts<<line({QPointF(12.7,0),QPointF(15.24,0)});
    parts<<line({QPointF(10.16,2.54),QPointF(10.16,5.08)});
    parts<<contact(QStringLiteral("1"),QPointF(.4,-2.2),QPointF(0,0));
    parts<<contact(QStringLiteral("2"),QPointF(14.8,-2.2),QPointF(15.24,0),Align::Right);
    parts<<contact(QStringLiteral("3"),QPointF(10.8,3.2),QPointF(10.16,5.08));
    Item designator;designator.type=ItemType::Text;designator.role=TextRole::Designator;designator.pos=QPointF(2.54,-6);designator.font.height=2.5;parts<<designator;
    Item value;value.type=ItemType::Text;value.role=TextRole::Value;value.pos=QPointF(2.54,3);value.font.height=2.5;parts<<value;
    Item c=makeComponent(parts,{},QStringLiteral("U?"),QString());
    c.caption=ui("Beispiel");c.libraryEntry=QStringLiteral("OpenLoch/Beispiel");
    return c;
}
namespace {
// A label of the frame: the number, or the letters A…Z, AA…ZZ, … of the position counted from 1.
QString frameLabel(FrameLabels kind,int n){
    if(kind==FrameLabels::Numbers)return QString::number(n);
    QString letters;for(int k=std::max(1,n);k>0;k=(k-1)/26)letters.prepend(QChar(u'A'+(k-1)%26));return letters;
}
}
TitleBlock generateTitleBlock(QRectF frame,int columns,int rows,const TitleBlockStyle &style){
    TitleBlock t;t.name=QStringLiteral("OpenLoch");t.frame=frame;t.columns=columns;t.rows=rows;t.columnStart=style.columnStart;t.rowStart=style.rowStart;
    const QRectF f=t.frame;const double h=style.textHeight,strip=std::max(3.,2*h);
    Item outer;outer.type=ItemType::Rectangle;outer.centre=f.center();outer.size=f.size();outer.pen.width=.5;t.items<<outer;
    auto label=[&](const QString &content,QPointF at,Align align){Item i=text(content,at,h,align);i.font.family=style.font;t.items<<i;};
    // A strip on each labelled side, with the division marks across it and a label in each field.
    const bool top=columns>0&&style.top!=FrameLabels::None,bottom=columns>0&&style.bottom!=FrameLabels::None;
    const bool left=rows>0&&style.left!=FrameLabels::None,right=rows>0&&style.right!=FrameLabels::None;
    const QRectF inner=f.adjusted(left?strip:0,top?strip:0,right?-strip:0,bottom?-strip:0);
    if(top||bottom||left||right){Item box;box.type=ItemType::Rectangle;box.centre=inner.center();box.size=inner.size();box.pen.width=.25;t.items<<box;}
    for(int i=0;i<columns&&(top||bottom);i++){
        const double x0=inner.left()+inner.width()*i/columns,x1=inner.left()+inner.width()*(i+1)/columns;
        if(top){if(i)t.items<<line({QPointF(x0,f.top()),QPointF(x0,inner.top())});label(frameLabel(style.top,style.columnStart+i),QPointF((x0+x1)/2,f.top()+(strip-h)/2),Align::Centre);}
        if(bottom){if(i)t.items<<line({QPointF(x0,inner.bottom()),QPointF(x0,f.bottom())});label(frameLabel(style.bottom,style.columnStart+i),QPointF((x0+x1)/2,inner.bottom()+(strip-h)/2),Align::Centre);}
    }
    for(int i=0;i<rows&&(left||right);i++){
        const double y0=inner.top()+inner.height()*i/rows,y1=inner.top()+inner.height()*(i+1)/rows;
        if(left){if(i)t.items<<line({QPointF(f.left(),y0),QPointF(inner.left(),y0)});label(frameLabel(style.left,style.rowStart+i),QPointF(f.left()+strip/2,(y0+y1)/2-h/2),Align::Centre);}
        if(right){if(i)t.items<<line({QPointF(inner.right(),y0),QPointF(f.right(),y0)});label(frameLabel(style.right,style.rowStart+i),QPointF(inner.right()+strip/2,(y0+y1)/2-h/2),Align::Centre);}
    }
    if(style.field){
        // The field with the sheet's name, number and the file in the bottom right corner.
        const double right=inner.right(),bottom=inner.bottom();
        Item box;box.type=ItemType::Rectangle;box.size=QSizeF(80,16);box.centre=QPointF(right-40,bottom-8);box.pen.width=.35;t.items<<box;
        t.items<<line({QPointF(right-80,bottom-8),QPointF(right,bottom-8)});
        t.items<<text(QStringLiteral("<PAGENAME>"),QPointF(right-78,bottom-14.5),3.5);
        t.items<<text(QStringLiteral("<PAGENO> / <PAGECOUNT>"),QPointF(right-2,bottom-6.5),3,Align::Right);
        t.items<<text(QStringLiteral("<FILENAME>"),QPointF(right-78,bottom-6.5),2.5);
    }
    for(auto &i:t.items)assignIds(i);
    return t;
}
TitleBlock generateTitleBlock(double width,double height,double margin,int columns,int rows,bool field){
    TitleBlockStyle style;style.bottom=FrameLabels::Numbers;style.right=FrameLabels::Letters;style.textHeight=2.5;style.field=field;
    return generateTitleBlock(QRectF(margin,margin,std::max(1.,width-2*margin),std::max(1.,height-2*margin)),columns,rows,style);
}
TitleBlock simpleTitleBlock(double width,double height){return generateTitleBlock(width,height,10,0,0,true);}
Document exampleDocument(){
    Document d=newDocument(ui("Blatt %1").arg(1));
    d.sheets.append(newSheet(ui("Blatt %1").arg(2),420,297));
    for(auto &s:d.sheets)s.titleBlock=simpleTitleBlock(s.width,s.height);
    Sheet &first=d.sheets[0];
    Item u1=exampleSymbol();u1.pos=QPointF(50.8,63.5);u1.designator=QStringLiteral("U1");u1.value=QStringLiteral("A");
    Item u2=exampleSymbol();u2.pos=QPointF(101.6,76.2);u2.designator=QStringLiteral("U2");u2.value=QStringLiteral("B");
    assignIds(u1);assignIds(u2);
    const Item *pin2=nullptr,*pin1=nullptr;
    for(const auto *c:contacts(u1))if(c->name==u"2")pin2=c;
    for(const auto *c:contacts(u2))if(c->name==u"1")pin1=c;
    const QPointF from=pinPosition(u1,*pin2),to=pinPosition(u2,*pin1);
    Item wire;wire.type=ItemType::Line;wire.points={from,QPointF((from.x()+to.x())/2,from.y()),QPointF((from.x()+to.x())/2,to.y()),to};assignIds(wire);
    Item note=text(QStringLiteral("OpenLoch"),QPointF(50.8,40.64),5);note.font.bold=true;assignIds(note);
    first.items<<u1<<u2<<wire<<note;
    return d;
}
}
