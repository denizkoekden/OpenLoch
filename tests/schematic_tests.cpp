#include "language.h"
#include "legacy_reader.h"
#include "modules/schematic/model.h"
#include "modules/schematic/example.h"
#include "modules/schematic/library.h"
#include "formats/splan/splan.h"
#include "modules/schematic/nets.h"
#include "modules/schematic/numbering.h"
#include "modules/schematic/partslist.h"
#include "modules/schematic/shapes.h"
#include "modules/schematic/images.h"
#include "modules/schematic/dimension.h"
#include "modules/schematic/render.h"
#include "modules/schematic/svg.h"
#include "modules/schematic/emf.h"
#include "modules/schematic/zip.h"
#include "modules/schematic/text.h"
#include <QApplication>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QToolButton>
#include <QLineEdit>
#include <QListWidget>
#include <QAction>
#include <QMenu>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QtEndian>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <cmath>
#include <functional>
#include <stdexcept>

using namespace openloch;
using namespace openloch::schematic;
int schematicEditorTests();
int schematicSplanTests();

static void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
static void rejects(const std::function<void()> &fn,const char *message){bool rejected=false;try{fn();}catch(const FormatError &){rejected=true;}require(rejected,message);}
static QString rejection(const std::function<void()> &fn){try{fn();}catch(const FormatError &e){return QString::fromUtf8(e.what());}return {};}
static bool near(double a,double b,double eps=1e-6){return std::abs(a-b)<eps;}
static bool near(QPointF a,QPointF b,double eps=1e-6){return near(a.x(),b.x(),eps)&&near(a.y(),b.y(),eps);}

namespace {
const Item *contactNamed(const Item &component,const QString &name){
    for(const auto *c:contacts(component))if(c->name==name)return c;
    return nullptr;
}
Item conductor(QPolygonF points){Item i;i.type=ItemType::Line;i.points=points;return i;}
Item junction(QPointF at){Item i;i.type=ItemType::Junction;i.pos=at;i.size=QSizeF(1,1);return i;}
Item label(const QString &name,QPointF at,bool global=false){Item i;i.type=ItemType::NetLabel;i.text=name;i.pos=at;i.global=global;return i;}
// A sheet with the given items, all with fresh ids.
Document documentWith(QList<QList<Item>> sheets){
    Document d=newDocument(QStringLiteral("1"));
    for(int s=1;s<sheets.size();s++)d.sheets.append(newSheet(QString::number(s+1)));
    for(int s=0;s<sheets.size();s++)for(auto i:sheets[s]){assignIds(i);d.sheets[s].items.append(i);}
    return d;
}
// Whether two items are in one net (by their ids, conductors, junctions or labels).
bool together(const QList<Net> &nets,const QString &a,const QString &b){
    for(const auto &n:nets){
        QSet<QString> ids;for(const auto &r:n.conductors)ids.insert(r.id);for(const auto &r:n.junctions)ids.insert(r.id);
        for(const auto &r:n.labels)ids.insert(r.id);for(const auto &p:n.pins)ids.insert(p.contact);
        if(ids.contains(a)&&ids.contains(b))return true;
    }
    return false;
}
// Every kind of element once, for the file format tests.
Document everything(){
    Document d=exampleDocument();
    Sheet &s=d.sheets[1];
    Item l=conductor({QPointF(10,10),QPointF(20,10),QPointF(20,30)});l.startEnd=LineEnd::Arrow;l.endEnd=LineEnd::Bar;l.endSize=3;
    l.pen={QColor(10,20,30),.5,PenStyle::DashDot};l.electrical=false;s.items<<l;
    Item p;p.type=ItemType::Polygon;p.points={QPointF(1,1),QPointF(5,1),QPointF(3,4)};p.fill={FillStyle::DiagonalCross,QColor(200,0,0),.7,.1};s.items<<p;
    Item b;b.type=ItemType::Bezier;b.points={QPointF(0,0),QPointF(1,2),QPointF(3,2),QPointF(4,0),QPointF(5,-2),QPointF(7,-2),QPointF(8,0)};b.endEnd=LineEnd::OpenArrow;s.items<<b;
    Item r;r.type=ItemType::Rectangle;r.centre=QPointF(40,40);r.size=QSizeF(10,6);r.rotation=30;r.corners=Corners::Round;r.corner=20;r.fill.style=FillStyle::Solid;s.items<<r;
    Item e;e.type=ItemType::Ellipse;e.centre=QPointF(60,40);e.size=QSizeF(10,6);e.start=30;e.stop=200;e.arc=ArcStyle::Pie;s.items<<e;
    Item t;t.type=ItemType::Text;t.text=QStringLiteral("Ä <PAGENO>\nzweite Zeile");t.pos=QPointF(70,70);t.rotation=90;t.mirrored=true;t.align=Align::Centre;
    t.font={QStringLiteral("Courier New"),3,true,true,true,false,QColor(0,0,255)};t.background=true;t.backgroundColor=QColor(255,255,0);s.items<<t;
    Item tb;tb.type=ItemType::TextBox;tb.centre=QPointF(100,100);tb.size=QSizeF(40,20);tb.text=QStringLiteral("Ein längerer Text");tb.wrap=false;tb.middle=true;s.items<<tb;
    s.items<<junction(QPointF(20,10));
    QByteArray png;{QImage img(2,2,QImage::Format_RGB32);img.fill(Qt::red);QBuffer buf(&png);buf.open(QIODevice::WriteOnly);img.save(&buf,"PNG");}
    const QString key=QString::fromLatin1(QCryptographicHash::hash(png,QCryptographicHash::Sha256).toHex());d.resources.insert(key,{QStringLiteral("png"),png});
    Item img;img.type=ItemType::Image;img.centre=QPointF(150,150);img.size=QSizeF(20,20);img.resource=key;s.items<<img;
    Item g;g.type=ItemType::Group;g.children<<conductor({QPointF(0,50),QPointF(10,50)})<<label(QStringLiteral("VCC"),QPointF(0,50),true);s.items<<g;
    Item c=exampleSymbol();c.pos=QPointF(200,100);c.rotation=270;c.mirrored=true;c.designator=QStringLiteral("U7");c.extra={"a","b","c","d"};
    c.designatorVisible=false;c.askValue=true;c.inPartsList=false;s.items<<c;
    for(auto &i:s.items)if(i.id.isEmpty())assignIds(i);
    d.variables={{QStringLiteral("Autor"),QStringLiteral("Jemand")},{QStringLiteral("Firma"),QString()}};
    d.activeSheet=1;
    return d;
}
}

int main(int argc,char **argv){
    QApplication app(argc,argv);setUiLanguage("de");
    try{
        // --- Ids
        {Document d=newDocument(QStringLiteral("A"));
            const QRegularExpression hex(QStringLiteral("^[0-9a-f]{32}$"));
            require(d.sheets.size()==1&&hex.match(d.sheets[0].id).hasMatch()&&hex.match(d.id).hasMatch()&&d.id!=d.sheets[0].id,"new document with ids of 32 hexadecimal digits");
            Item c=exampleSymbol();assignIds(c);
            QSet<QString> ids{c.id};for(const auto &k:c.children)ids.insert(k.id);
            require(ids.size()==c.children.size()+1&&!ids.contains(QString()),"every element of a component has its own id");
            d.sheets[0].items<<c<<c;completeIds(d);
            QSet<QString> all;for(const auto &i:d.sheets[0].items){all.insert(i.id);for(const auto &k:i.children)all.insert(k.id);}
            require(all.size()==2*(c.children.size()+1),"a copied component gets new ids when they repeat");
            require(d.sheets[0].items[0].id==c.id,"the first of two equal ids stays");
            Document e;e.sheets.append(Sheet());e.sheets[0].items<<conductor({QPointF(),QPointF(1,1)});completeIds(e);
            require(hex.match(e.id).hasMatch()&&hex.match(e.sheets[0].id).hasMatch()&&hex.match(e.sheets[0].items[0].id).hasMatch(),"missing ids are given");}

        // --- Placement of a component and its contacts
        {Item c=exampleSymbol();c.pos=QPointF(100,50);
            const Item *k2=contactNamed(c,"2"),*k3=contactNamed(c,"3");
            require(near(pinPosition(c,*k2),{115.24,50})&&near(pinPosition(c,*k3),{110.16,55.08}),"contacts at their place");
            c.rotation=90;require(near(pinPosition(c,*k2),{100,50-15.24})&&near(pinPosition(c,*k3),{105.08,50-10.16}),"turned counter-clockwise on the screen");
            c.rotation=0;c.mirrored=true;require(near(pinPosition(c,*k2),{100-15.24,50})&&near(pinPosition(c,*k3),{100-10.16,55.08}),"mirrored left to right");
            c.rotation=90;require(near(pinPosition(c,*k2),{100,50+15.24}),"mirrored, then turned");
            // Turning and mirroring the component as an element.
            Item d=exampleSymbol();d.pos=QPointF(10,10);
            rotate(d,QPointF(0,0),90);require(near(d.pos,{10,-10})&&near(d.rotation,90),"turning moves the insertion point and turns the component");
            for(int i=0;i<3;i++)rotate(d,QPointF(0,0),90);require(near(d.pos,{10,10})&&near(d.rotation,0),"four quarter turns");
            mirror(d,20);require(near(d.pos,{30,10})&&d.mirrored&&near(d.rotation,0),"mirrored about x = 20");
            const QPointF before=pinPosition(exampleSymbol(),*contactNamed(exampleSymbol(),"3"));(void)before;
            Item e=exampleSymbol();e.pos=QPointF(5,5);e.rotation=90;const QPointF p3=pinPosition(e,*contactNamed(e,"3"));
            mirror(e,0);require(near(pinPosition(e,*contactNamed(e,"3")),{-p3.x(),p3.y()}),"a mirrored component keeps its contacts on the mirror image");
            mirrorVertically(e,0);require(near(pinPosition(e,*contactNamed(e,"3")),{-p3.x(),-p3.y()}),"top to bottom as well");
            mirror(e,0);mirrorVertically(e,0);require(near(e.pos,{5,5})&&!e.mirrored&&near(e.rotation,90),"mirroring twice gives the start");}

        // --- Connection points guessed for contacts from files that keep none
        {Item c;c.type=ItemType::Component;
            Item body;body.type=ItemType::Rectangle;body.centre=QPointF(10,0);body.size=QSizeF(10,4);c.children<<body;
            c.children<<conductor({QPointF(0,0),QPointF(5,0)})<<conductor({QPointF(15,0),QPointF(20,0)})<<conductor({QPointF(20,0),QPointF(20,0)});
            auto contact=[&](const QString &name,QPointF at){Item k;k.type=ItemType::Contact;k.name=name;k.text=name;k.pos=at;c.children<<k;};
            contact("1",{3.5,-3});contact("2",{19,-3});contact("3",{50,50});contact("4",{0,4});
            const auto g=guessedPins(c);
            require(g.size()==4&&g[0]&&near(*g[0],{0,0}),"the free end of the line next to the contact, not the end at the body");
            require(g[1]&&near(*g[1],{20,0}),"a dot on a line end does not take it");
            require(!g[2],"no line end near a contact: none");
            require(!g[3],"each end serves one contact, the nearest");
            c.children[5].hasPin=true;c.children[5].pin=QPointF(7,7);
            require(guessPins(c)==1&&c.children[4].hasPin&&near(c.children[4].pin,{0,0})&&near(c.children[5].pin,{7,7})&&!c.children[6].hasPin,
                    "only contacts without a connection point get the guessed one");}

        // --- The point "Am Raster ausrichten" puts on the grid
        {Item l=conductor({QPointF(3,4),QPointF(9,9)});require(near(gridPoint(l),{3,4}),"a line: its first point");
            Item r;r.type=ItemType::Rectangle;r.centre=QPointF(10,20);r.size=QSizeF(4,2);require(near(gridPoint(r),{8,21}),"a rectangle: bottom left");
            Item e;e.type=ItemType::Ellipse;e.centre=QPointF(10,20);e.size=QSizeF(4,2);require(near(gridPoint(e),{8,19}),"an ellipse: left and top");
            Item t;t.type=ItemType::Text;t.text=QStringLiteral("x");t.pos=QPointF(5,6);t.align=Align::Centre;require(near(gridPoint(t),{5,6}),"a text: its anchor");
            Item g;g.type=ItemType::Group;g.children<<conductor({QPointF(5,5),QPointF(6,6)})<<conductor({QPointF(5.02,2),QPointF(1,1)})<<conductor({QPointF(7,1),QPointF(1,1)});
            require(near(gridPoint(g),{5.02,2}),"a group: a later element not further right and higher");
            Item c=exampleSymbol();c.pos=QPointF(12,13);require(near(gridPoint(c),{12,13}),"a component: its insertion point");}

        // --- Dimensions: numbers, text, the drawing of each kind
        {require(dimensionNumber(10,1,false)=="10"&&dimensionNumber(11.18,1,false)=="11,2"&&dimensionNumber(11.1,2,true)=="11.1"&&dimensionNumber(-.01,1,false)=="0"
                &&dimensionNumber(std::nan(""),1,false)=="NAN"&&dimensionNumber(2.5,0,false)=="3","numbers: rounded, without trailing zeros, point or comma");
            Item d;d.type=ItemType::Dimension;d.points={QPointF(10,20),QPointF(20,20),QPointF(10,20)};d.offset=10;d.font.height=2.4;
            require(dimensionText(d,1)=="10"&&dimensionText(d,2.5)=="25","the length times the sheet's scale");
            Item t=d;t.prefix="P";t.suffix="S";t.showDiameter=true;require(dimensionText(t,1)==QString::fromUtf8("PØ10S"),"prefix, Ø, value, suffix");
            t.autoValue=false;t.fixedValue="FIX";require(dimensionText(t,1)==QString::fromUtf8("PØFIXS"),"a fixed value");
            // Inside or outside is decided with the text's width, which differs a little between systems: far from it.
            Item wide=d;wide.points[1]=QPointF(40,20);auto a=dimensionDrawing(wide,1);
            require(a.extensions.size()==2&&near(a.extensions[0].p1(),{10,20})&&near(a.extensions[0].p2(),{10,8})&&near(a.extensions[1].p2(),{40,8}),"extension lines 2 mm beyond the line above");
            require(a.arrows.size()==2&&near(a.arrows[0][0],{10,10})&&a.arrows[0][1].x()>10&&near(a.arrows[1][0],{40,10})&&a.arrows[1][1].x()<40,"arrows inside, tips at the ends");
            require(a.texts.size()==1&&a.texts[0].text=="30"&&a.texts[0].pos.y()<10&&std::abs(a.texts[0].pos.x()-25)<1e-9&&std::abs(a.texts[0].rotation)<1e-9,"the text above the middle");
            Item below=d;below.offset=-10;require(near(dimensionDrawing(below,1).extensions[0].p2(),{10,32}),"a negative offset: below");
            Item outside=d;outside.arrowLength=8;const auto o=dimensionDrawing(outside,1);
            require(near(o.arrows[0][0],{10,10})&&o.arrows[0][1].x()<10&&o.line.boundingRect().left()<-1,"arrows that do not fit go outside");
            Item up=d;up.points={QPointF(10,20),QPointF(10,10),QPointF(10,20)};up.offset=5;const auto v=dimensionDrawing(up,1);
            require(near(v.extensions[0].p2(),{3,20})&&std::abs(v.texts[0].rotation-90)<1e-9&&v.texts[0].pos.x()<5,"measured upwards: line on the left, text reading upwards");
            Item r=d;r.dimension=DimensionKind::Radial;const auto rd=dimensionDrawing(r,1);require(rd.extensions.isEmpty()&&rd.arrows.size()==1&&near(rd.arrows[0][0],{20,10}),"a radius: one arrow at the second point");
            Item dia=d;dia.dimension=DimensionKind::Diameter;const auto dd=dimensionDrawing(dia,1);require(dd.extensions.isEmpty()&&dd.arrows.size()==2,"a diameter: two arrows, no extension lines");
            Item an=d;an.dimension=DimensionKind::Angle;an.points={QPointF(10,0),QPointF(0,-10),QPointF(0,0)};an.suffix=QString::fromUtf8("°");
            const auto ad=dimensionDrawing(an,1);
            require(dimensionText(an,1)==QString::fromUtf8("90°")&&ad.extensions.size()==2&&ad.arrows.size()==2&&near(ad.arrows[0][0],{10,0})&&near(ad.line.boundingRect().bottomRight(),{10,0},1e-6),"an angle: arc, legs, arrows, degrees");
            Item tol=d;tol.upperTolerance="+0,1";tol.lowerTolerance="-0,2";const auto td=dimensionDrawing(tol,1);
            require(td.texts.size()==3&&td.texts[1].font.height==1.2&&td.texts[1].pos.x()>td.texts[0].pos.x()&&td.texts[2].pos.y()>td.texts[1].pos.y(),"tolerances small after the value, upper above lower");
            Item m=d;mirror(m,0);require(near(m.points[0],{-10,20})&&m.offset==-10,"mirrored: the line stays above");
            Item s=d;scale(s,QPointF(),2);require(near(s.points[1],{40,40})&&s.offset==20&&std::abs(s.arrowLength-5.6)<1e-9,"scaled: offset and arrows grow");
            require(near(gridPoint(d),{10,20}),"on the grid by its first point");}

        // --- Pictures: resolution, turning, lightening, proportions, unused ones dropped
        {Document d=newDocument(QStringLiteral("x"));
            QImage image(400,200,QImage::Format_RGB32);image.fill(Qt::black);QByteArray data;{QBuffer b(&data);b.open(QIODevice::WriteOnly);image.save(&b,"PNG");}
            const QString key=QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex());d.resources.insert(key,{QStringLiteral("png"),data});
            Item pic;pic.type=ItemType::Image;pic.resource=key;pic.centre=QPointF(50,50);pic.size=QSizeF(50.8,25.4);assignIds(pic);
            Item group;group.type=ItemType::Group;group.children<<pic;assignIds(group);pic.id=group.children[0].id;d.sheets[0].titleBlock.items<<group;
            require(placedImages(d).size()==1&&imageWithId(d,0,pic.id),"a picture in a group of the title block is found");
            Item &p=*imageWithId(d,0,pic.id);
            const ImageInfo info=imageInfo(d,p);require(info.pixels==QSize(400,200)&&std::abs(info.dpi-200)<1e-9&&info.bytes==data.size(),"pixels, resolution and memory");
            require(reduceResolution(d,p,2)&&imageInfo(d,p).pixels==QSize(200,100)&&p.resource!=key&&d.resources[p.resource].kind=="png","half the pixels, a new picture of the same kind");
            require(dropUnusedResources(d)==1&&!d.resources.contains(key),"the old picture is dropped");
            require(rotateImage(d,p)&&imageInfo(d,p).pixels==QSize(100,200)&&near(p.size.width(),25.4)&&near(p.size.height(),50.8),"turned by 90°, the element as well");
            p.size=QSizeF(50,10);require(normaliseImage(d,p)&&near(p.size.height(),100),"proportions from the pixels");
            require(lightenImage(d,p)&&QImage::fromData(d.resources[p.resource].data).pixelColor(5,5).red()==63,"a quarter of the way to white");}

        // --- "Wandeln in Linie / Polygon": the outline's points, curves as short pieces, outline and filling kept.
        {Item r;r.type=ItemType::Rectangle;r.centre=QPointF(20,10);r.size=QSizeF(10,4);r.fill.style=FillStyle::Solid;r.pen.width=.5;
            const auto polygon=convertedTo(r,ItemType::Polygon);
            require(polygon&&polygon->type==ItemType::Polygon&&polygon->points.size()==4&&polygon->fill.style==FillStyle::Solid&&near(polygon->pen.width,.5)&&
                    polygon->points.boundingRect()==QRectF(15,8,10,4),"a rectangle as a polygon of its corners");
            const auto line=convertedTo(r,ItemType::Line);
            require(line&&line->points.size()==5&&line->points.first()==line->points.last(),"as a line that ends where it began");
            Item e;e.type=ItemType::Ellipse;e.centre=QPointF(0,0);e.size=QSizeF(20,20);
            const auto round=convertedTo(e,ItemType::Polygon);bool onCircle=round&&round->points.size()>16;
            if(round)for(const QPointF p:round->points)onCircle&=std::abs(std::hypot(p.x(),p.y())-10)<.03;
            require(onCircle,"a circle in short pieces on it");
            e.arc=ArcStyle::Arc;e.start=0;e.stop=90;const auto arc=convertedTo(e,ItemType::Line);
            require(arc&&arc->points.first()!=arc->points.last(),"an arc stays open");
            Item l;l.type=ItemType::Line;l.points={QPointF(0,0),QPointF(10,0),QPointF(10,5)};l.endEnd=LineEnd::Arrow;l.electrical=true;
            const auto closed=convertedTo(l,ItemType::Polygon);
            require(closed&&closed->points==l.points&&closed->endEnd==LineEnd::None&&!closed->electrical,"a line as a polygon, without line ends");
            Item t;t.type=ItemType::Text;require(!convertedTo(t,ItemType::Line)&&!convertedTo(l,ItemType::Rectangle),"not for texts, only to lines and polygons");
            // "Wandeln in Kurve": straight pieces with their control points on them, curves kept, closed outlines closed.
            const auto curve=convertedTo(l,ItemType::Bezier);
            require(curve&&curve->type==ItemType::Bezier&&curve->points.size()==7&&near(curve->points[1],{10./3,0})&&near(curve->points[3],{10,0})&&curve->endEnd==LineEnd::Arrow,
                    "a line as a curve of straight pieces, its ends kept");
            const auto box=convertedTo(r,ItemType::Bezier);require(box&&box->points.size()==13&&near(box->points.first(),box->points.last()),"a rectangle: four pieces, closed");
            e.arc=ArcStyle::Full;const auto ring=convertedTo(e,ItemType::Bezier);bool onRing=ring&&ring->points.size()>=13;
            if(ring)for(int k=0;k<ring->points.size();k+=3)onRing&=std::abs(std::hypot(ring->points[k].x(),ring->points[k].y())-10)<1e-6;
            require(onRing,"a circle keeps its curves");}

        // --- Special shapes from a frame
        {Item line;line.type=ItemType::Line;Item shape;shape.type=ItemType::Polygon;Item text;text.type=ItemType::Text;SpecialShapeOptions o;
            auto make=[&](SpecialShape k,QPointF a,QPointF b){return specialShape(k,a,b,o,line,shape,text);};
            auto one=[&](SpecialShape k,QPointF a,QPointF b){const auto l=make(k,a,b);require(l.size()==1,"one element");return l[0];};
            const Item pentagon=one(SpecialShape::RegularPolygon,{0,0},{20,20});
            require(pentagon.type==ItemType::Polygon&&pentagon.points.size()==5&&near(pentagon.points[0],{10,0}),"a polygon of five corners, the first at the top");
            o.polygonAsLine=true;const Item ring=one(SpecialShape::RegularPolygon,{0,0},{20,20});require(ring.type==ItemType::Line&&ring.points.size()==6&&ring.points.first()==ring.points.last()&&!ring.electrical,"as a closed line");
            const Item star=one(SpecialShape::Star,{0,0},{20,20});require(star.type==ItemType::Polygon&&star.points.size()==10&&std::abs(QLineF(star.points[1],{10,10}).length()-5)<1e-9,"a star: inner corners at half the radius");
            o.textFields=true;const Item grid=one(SpecialShape::Grid,{0,0},{30,30});int lines=0,texts=0,frames=0;
            for(const auto &c:grid.children){lines+=c.type==ItemType::Line;texts+=c.type==ItemType::Text;frames+=c.type==ItemType::Rectangle;}
            require(grid.type==ItemType::Group&&lines==4&&texts==9&&frames==1&&std::abs(grid.children.last().font.height-8)<1e-9,"a grid of 3 × 3 with frame and text fields of 80 % of a row");
            const Item sine=one(SpecialShape::Wave,{0,10},{20,30});require(sine.type==ItemType::Bezier&&sine.points.size()==7&&sine.points[1].y()<20&&sine.points[4].y()>20,"one sine period: two curves, up then down");
            o.wave=WaveKind::Square;o.waves=2;const Item square=one(SpecialShape::Wave,{0,10},{20,30});require(square.type==ItemType::Line&&square.points.size()==9&&near(square.points.last(),{20,30}),"a square wave of two periods");
            const Item right=one(SpecialShape::ArrowHorizontal,{0,0},{30,10}),left=one(SpecialShape::ArrowHorizontal,{30,0},{0,10});
            require(right.points.size()==7&&near(right.points[3],{30,5})&&near(left.points[3],{0,5}),"an arrow points where the frame was drawn to");
            require(make(SpecialShape::CurlyBracket,{0,0},{5,20})[0].children.size()==2&&one(SpecialShape::RoundBracket,{0,0},{5,20}).type==ItemType::Bezier,"brackets are curves");
            for(auto [k,n]:{std::pair{SpecialShape::Triangle,3},{SpecialShape::RightTriangle,3},{SpecialShape::Square,4},{SpecialShape::Diamond,4},{SpecialShape::Parallelogram,4},
                {SpecialShape::Hexagon,6},{SpecialShape::Octagon,8},{SpecialShape::ArrowVertical,7},{SpecialShape::SpeechBubble,7},{SpecialShape::Lightning,11}}){
                const Item p=one(k,{0,0},{20,10});require(p.type==ItemType::Polygon&&p.points.size()==n&&QRectF(-1e-9,-1e-9,20+2e-9,10+2e-9).contains(p.points.boundingRect()),"a closed shape inside its frame");}}

        // --- Geometry of the other elements
        {Item r;r.type=ItemType::Rectangle;r.centre=QPointF(10,0);r.size=QSizeF(4,2);
            rotate(r,QPointF(0,0),90);require(near(r.centre,{0,-10})&&near(r.rotation,90),"rectangle turned");
            require(near(path(r).boundingRect().width(),2,1e-3)&&near(path(r).boundingRect().height(),4,1e-3)&&near(bounds(r).width(),2.25,1e-3),"bounds of a turned rectangle");
            Item e;e.type=ItemType::Ellipse;e.centre=QPointF(0,0);e.size=QSizeF(4,4);e.start=0;e.stop=90;e.arc=ArcStyle::Arc;
            mirror(e,0);require(near(e.start,90)&&near(e.stop,180),"a mirrored arc runs on the mirror image");
            mirrorVertically(e,0);require(near(e.start,180)&&near(e.stop,270),"and top to bottom");
            Item t;t.type=ItemType::Text;t.text=QStringLiteral("Abc");t.pos=QPointF(5,0);
            mirror(t,0);require(near(t.pos,{-5,0})&&t.align==Align::Right&&!t.mirrored,"a mirrored text stays readable and extends to the other side");
            mirror(t,0,true);require(t.mirrored&&t.align==Align::Right,"with mirrored letters on request");
            Item lt;lt.type=ItemType::Text;lt.text=QStringLiteral("X");lt.rotation=30;mirrorVertically(lt,0);require(near(lt.rotation,150)&&lt.align==Align::Right,"vertical mirror of a text");
            // Texts of a mirrored component keep their letters readable.
            Item c=exampleSymbol();c.mirrored=true;Item designator;
            for(const auto &k:c.children)if(k.role==TextRole::Designator)designator=placedText(k,c);
            require(near(designator.pos,{-2.54,-6})&&designator.align==Align::Right&&!designator.mirrored,"designator of a mirrored component");
            const auto layout=layoutText(QStringLiteral("Wy\nWy"),Font());require(layout.lines.size()==2&&layout.width>1&&near(layout.lineHeight*2,textRect(t,"Wy\nWy").height()),"text layout in millimetres");}

        // --- Variables
        {Document d=everything();const TextContext c{&d,1,nullptr,QStringLiteral("/tmp/Plan.olsch")};
            require(expandVariables("<PAGENO>/<PAGECOUNT> <PAGENAME>",c)=="2/2 "+d.sheets[1].name,"page variables");
            require(expandVariables("<FILENAME> <FILENAME_PURE> <filepath>",c)=="Plan.olsch Plan "+QDir::toNativeSeparators(QFileInfo("/tmp/Plan.olsch").absolutePath()),"file variables");
            require(expandVariables("<Autor>, <AUTOR>, <Firma>!",c)=="Jemand, Jemand, !","user variables");
            require(expandVariables("<unbekannt> a<b",c)=="<unbekannt> a<b","unknown names stay");
            {Document s=d;s.sheets<<s.sheets[0]<<s.sheets[0];s.sheets[1].spare=true;s.sheets[2].spare=true;  // 1, spare 2 and 3, 4
                auto at=[&](int sheet){return expandVariables("<PREVIOUS_PAGENO>|<NEXT_PAGENO>",{&s,sheet,nullptr,{}});};
                require(at(0)=="-|4"&&at(1)=="1|4"&&at(3)=="1|-","previous and next sheet without the spare ones");
                s.sheets[0].scale=2.5;s.sheets[3].scale=2;s.sheets[3].scaleUnit=ScaleUnit::Metre;
                require(expandVariables("<PAGESCALE>",{&s,0,nullptr,{}})==QStringLiteral("2")+uiLocale().decimalPoint()+u'5'&&expandVariables("<PAGESCALE>",{&s,3,nullptr,{}})=="2000",
                        "<PAGESCALE>: the millimetres of one on the sheet, without grouping");}
            d.variables={{"A","<B>"},{"B","<A>"}};require(expandVariables("<A>",{&d,0,nullptr,{}})=="<B>"||expandVariables("<A>",{&d,0,nullptr,{}}).size()<64,"recursion ends");
            Item comp=exampleSymbol();comp.designator=QStringLiteral("R<PAGENO>");comp.value=QStringLiteral("10k");comp.extra={"x","y","",""};
            const TextContext cc{&d,0,&comp,{}};
            require(expandVariables("<BEZ>=<WERT> <Z1><Z2><Z4>",cc)=="R1=10k xy","component variables");
            for(const auto &k:comp.children)if(k.role==TextRole::Value)require(shownText(k,cc)=="10k","a value text shows the component's value");
            // "Bauteile mit Seitennummer" and "Präfix": prefix, sheet number and designator, only as shown.
            {Document q=d;q.designatorPageNumbers=true;q.designatorPrefix=QStringLiteral("=A-");Item r=exampleSymbol();r.designator=QStringLiteral("R1");r.value=QStringLiteral("10k");
                const TextContext rc{&q,1,&r,{}};
                for(const auto &k:r.children)if(k.role==TextRole::Designator)require(shownText(k,rc)=="=A-2R1","prefix, sheet number and designator, no separator");
                require(r.designator=="R1"&&expandVariables("<BEZ> <ID> <WERT> <VALUE> <VALEUR>",rc)=="=A-2R1 =A-2R1 10k 10k 10k","<BEZ> and <ID> as shown");
                require(shownDesignator(r,{&q,0,&r,{}})=="=A-1R1","the number of the component's sheet");
                r.designator.clear();require(shownDesignator(r,rc)=="=A-2","an empty designator still gets them");r.designator=QStringLiteral("R1");
                require(shownDesignator(r,{nullptr,1,&r,{}})=="R1"&&shownDesignator(r,{&q,5,&r,{}})=="R1","not outside a document's sheets");
                q.designatorPageNumbers=false;require(shownDesignator(r,rc)=="R1","switched off: the designator as entered, the prefix unused");
                q.designatorPageNumbers=true;q.designatorPrefix.clear();require(shownDesignator(r,rc)=="2R1","without prefix the sheet number alone");}}

        // --- Parents and children, and the grid of the title block
        {Document p=exampleDocument();auto &items=p.sheets[0].items;   // U1, U2, a conductor, a text
            items[0].parent=true;items[0].value=QStringLiteral("7400");items[0].extra={QStringLiteral("DIL14")};
            items[1].parentId=items[0].id;items[1].designator=QStringLiteral("<PARENT_ID>-<CHILDCHAR>");items[1].value=QStringLiteral("<PARENT_VALUE>");
            Item third=exampleSymbol();third.parentId=items[0].id;third.designator=QStringLiteral("<PARENT_ID>.<CHILDNO>");third.pos=QPointF(80,120);assignIds(third);
            p.sheets[1].items<<third;
            p.sheets[1].titleBlock=generateTitleBlock(420,297,10,4,3,false);
            const Item &parent=p.sheets[0].items[0],&child=p.sheets[0].items[1],&other=p.sheets[1].items.last();
            require(childrenOf(p,parent.id).size()==2&&componentWithId(p,parent.id).item==&parent,"a parent knows its children");
            auto shown=[&](const Item &c,int sheet,const QString &text){return expandVariables(text,{&p,sheet,&c,{}});};
            require(shown(child,0,child.designator)=="U1-a"&&shown(child,0,child.value)=="7400"&&shown(other,1,other.designator)=="U1.2","children show their parent's designator and value, and their number");
            require(shown(child,0,"<PARENT_PAGENO> <PARENT_Z1> <PARENT_CONTACT_2>")=="1 DIL14 2","the parent's sheet, additional text and contacts");
            // With sheet numbers in designators the children take their parent's designator as shown and get none of
            // their own; a child whose parent is missing counts as without one.
            {Document q=p;q.designatorPageNumbers=true;q.designatorPrefix=QStringLiteral("X");
                const Item &qp=q.sheets[0].items[0],&qc=q.sheets[0].items[1],&qo=q.sheets[1].items.last();
                auto as=[&](const Item &c,int sheet){return shownDesignator(c,{&q,sheet,&c,{}});};
                require(as(qp,0)=="X1U1"&&as(qc,0)=="X1U1-a"&&as(qo,1)=="X1U1.2","children with their parent's prefix and sheet");
                require(expandVariables("<PARENT_ID>",{&q,1,&qo,{}})=="X1U1","<PARENT_ID> as shown");
                Item orphan=qo;orphan.parentId=newId();require(as(orphan,1)=="X2<PARENT_ID>.0","a missing parent: prefixed like any component");}
            require(shown(parent,0,"<CHILD_PAGENO> <CHILD_PAGENO_2> <CHILD_PAGENAME_2> <CHILD_PAGENO_3>")==QStringLiteral("1 2 %1 ").arg(p.sheets[1].name),"a parent shows its children's sheets");
            require(columnAt(p.sheets[1],QPointF(80,120))==1&&rowAt(p.sheets[1],QPointF(80,120))==2&&columnAt(p.sheets[1],QPointF(5,5))==0,"column and row of the title block's grid");
            require(shown(other,1,"<COLNUM><COLCHAR> <ROWNUM><ROWCHAR> <PARENT_COLNUM>")=="1A 2B 0","the grid at a component and at its parent");
            // The number of the parent's designator and the name of its sheet with user variables filled in.
            {Document q=p;q.sheets[0].name=QStringLiteral("Netzteil <rev> <PAGENO>");q.variables={{QStringLiteral("REV"),QStringLiteral("B")}};
                const Item &qo=q.sheets[1].items.last();auto with=[&](const QString &id){q.sheets[0].items[0].designator=id;return expandVariables("<PARENT_ID_NUMBER>",{&q,1,&qo,{}});};
                require(with("U1")=="1"&&with("R007")=="7"&&with("IC1A")=="0"&&with("R")=="0","<PARENT_ID_NUMBER>: the number at the end of the parent's designator");
                require(expandVariables("<PARENT_PAGENAME>",{&q,1,&qo,{}})=="Netzteil B <PAGENO>","<PARENT_PAGENAME> with user variables only");}
            // Letters of the grid and of children beyond Z as in the reference: AA, AB … for columns, Aa, Ab … for children.
            {Document g=p;Sheet &gs=g.sheets[0];gs.titleBlock.frame=QRectF(0,0,300,100);gs.titleBlock.columns=60;gs.titleBlock.rows=1;gs.titleBlock.columnStart=gs.titleBlock.rowStart=1;
                Item &k=gs.items[0];auto at=[&](double x){k.pos=QPointF(x,50);return expandVariables("<COLCHAR>",{&g,0,&k,{}});};
                require(at(2)=="A"&&at(127.5)=="Z"&&at(132.5)=="AA"&&at(137.5)=="AB"&&at(262.5)=="BA"&&at(-5)=="@","A to Z, then AA, AB … BA; outside the grid @");
                gs.titleBlock.columnStart=0;require(at(132.5)=="AA"&&at(2)=="A","a start of 0 counts one more");
                Document many=exampleDocument();auto &list=many.sheets[0].items;list[0].parent=true;
                for(int i=0;i<28;i++){Item c=exampleSymbol();c.parentId=list[0].id;c.pos=QPointF(20+i,150);assignIds(c);list<<c;}
                auto child=[&](int n){const Item &c=many.sheets[0].items[int(many.sheets[0].items.size())-28+n-1];return expandVariables("<CHILDCHAR>",{&many,0,&c,{}});};
                require(child(1)=="a"&&child(26)=="z"&&child(27)=="Aa"&&child(28)=="Ab","children a to z, then Aa, Ab");}
            // Copies: children among them follow their copied parent, others keep theirs.
            {QList<Item> copies{parent,child};freshIds(copies);QList<Item> alone{child};freshIds(alone);
                require(copies[0].id!=parent.id&&copies[1].parentId==copies[0].id&&alone[0].id!=child.id&&alone[0].parentId==parent.id,"copied children follow their copied parent, others keep theirs");
                Item clip;clip.type=ItemType::Group;clip.children<<parent<<child;clip.children[0].designator="U?";
                const Item placed=placedSymbol({QStringLiteral("7400"),clip,{}},p);
                require(placed.children[0].designator=="U2"&&placed.children[1].parentId==placed.children[0].id&&placed.children[0].id!=parent.id,"a parent structure from the library: the next number, the children with it");}
            // The child list ("Kontaktspiegel").
            {ChildListOptions o;o.columns={ChildColumn::Designator,ChildColumn::Contacts,ChildColumn::Value,ChildColumn::PageNumber,ChildColumn::PageName,ChildColumn::RowColumn,ChildColumn::PageColumn,ChildColumn::Reference};
                QStringList pins;for(const auto *k:contacts(other))pins<<k->text;const QString joined=pins.join(u'-');
                const PartsTable t=childList(p,parent.id,o);
                require(t.rows.size()==2&&t.header.size()==8&&t.rows[0][0]=="U1-a","one row per child, in their order");
                require(t.rows[1]==QStringList{"U1.2",joined,other.value,"2",p.sheets[1].name,"B1","2.1","/2.B1-U1.2:"+joined},"its columns: designator, contacts, value, sheet, grid, reference");
                o.rowLetters=false;o.columnLetters=true;o.slash=true;o.columns={ChildColumn::RowColumn,ChildColumn::PageColumn};
                require(childList(p,parent.id,o).rows[1]==QStringList{"2A","/2.A"},"rows and columns as numbers or letters, the sheet with a slash");
                PartsDrawing plain;plain.header=false;plain.frame=false;plain.verticalLines=false;
                PartsDrawing rich=plain;rich.frame=true;rich.shadow=true;rich.alternate=true;rich.verticalLines=true;
                const Item a=partsGroup(t,QPointF(),plain),b=partsGroup(t,QPointF(),rich);
                int texts=0;for(const auto &c:a.children)texts+=c.type==ItemType::Text&&!c.font.bold;
                int boxes=0;for(const auto &c:b.children)boxes+=c.type==ItemType::Rectangle;
                int cells=0;for(const auto &r:t.rows)for(const auto &c:r)cells+=!c.isEmpty();
                require(texts==cells&&boxes==4,"without a header; frame, shadow and its ground, a grey second row");}
            require(decode(encode(p))==p,"kept in the own format");
            Document lost=p;lost.sheets[0].items.removeAt(0);
            require(decode(encode(lost)).sheets[0].items[0].parentId.isEmpty(),"a link to a deleted parent is not written");
            QJsonObject o=toJson(p);auto sheets=o["sheets"].toArray();auto s0=sheets[0].toObject();auto list=s0["items"].toArray();auto c1=list[1].toObject();
            c1["parentId"]=QStringLiteral("0123456789abcdef0123456789abcdef");list[1]=c1;s0["items"]=list;sheets[0]=s0;o["sheets"]=sheets;
            rejects([&]{decode(QJsonDocument(o).toJson());},"a link to a parent that is not there");}

        // --- Text links
        {Document p=exampleDocument();Item target;target.type=ItemType::Text;target.text=QStringLiteral("Ziel");target.pos=QPointF(100,100);target.linkable=true;assignIds(target);
            p.sheets[1].titleBlock=generateTitleBlock(420,297,10,4,3,false);p.sheets[1].items<<target;
            Item link;link.type=ItemType::Text;link.text=QStringLiteral("siehe Blatt <LINK_PAGENO> (<LINK_PAGENAME>): <LINK_TEXT> <LINK_COLNUM><LINK_ROWCHAR>");link.linkTarget=target.id;link.pos=QPointF(20,20);assignIds(link);
            Item web;web.type=ItemType::Text;web.text=QStringLiteral("Datenblatt");web.link=QStringLiteral("https://example.org/datenblatt.pdf");assignIds(web);
            p.sheets[0].items<<link<<web;
            const TextContext c0{&p,0,nullptr,{}};const TextContext c1{&p,1,nullptr,{}};
            require(shownText(p.sheets[0].items[4],c0)==QStringLiteral("siehe Blatt 2 (%1): Ziel 1A").arg(p.sheets[1].name),"a link shows its target's sheet, text and grid");
            Item back=target;back.text=QStringLiteral("von Blatt <LINKFROM_PAGENO>: <LINKFROM_TEXT>");
            require(shownText(back,c1).startsWith("von Blatt 1: siehe Blatt"),"a target shows where it is linked from");
            require(linksTo(p,target.id).size()==1&&textWithId(p,target.id).sheet==1,"links found");
            require(decode(encode(p))==p,"links kept in the own format");
            Document lost=p;lost.sheets[1].items.removeLast();require(decode(encode(lost)).sheets[0].items[4].linkTarget.isEmpty(),"a link to a deleted text is not written");
            QJsonObject o=toJson(p);auto sheets=o["sheets"].toArray();auto s0=sheets[0].toObject();auto list=s0["items"].toArray();auto t=list[4].toObject();
            t["linkTarget"]=QStringLiteral("0123456789abcdef0123456789abcdef");list[4]=t;s0["items"]=list;sheets[0]=s0;o["sheets"]=sheets;
            rejects([&]{decode(QJsonDocument(o).toJson());},"a link to a text that is not there");
            const QImage plain=renderSheet(p,0,4);RenderOptions marks;marks.linkMarks=true;const QImage marked=renderSheet(p,0,4,marks);
            require(plain!=marked,"links marked on demand");}

        // --- Version 13: the step of a junction's automatic size and the writing direction of a text, also in sPlan 8
        {Document d=exampleDocument();Item j;j.type=ItemType::Junction;j.pos=QPointF(40,40);j.size=QSizeF(1.2,1.2);j.sizeStep=1;assignIds(j);
            Item t;t.type=ItemType::Text;t.text=QStringLiteral("Umgekehrt");t.pos=QPointF(60,60);t.reversed=true;assignIds(t);d.sheets[0].items<<j<<t;
            const QJsonObject json=toJson(d);require(json["version"].toInt()==13&&fromJson(json)==d,"both in the own format, version 13");
            QJsonObject old=json;old["version"]=12;
            {const Document o=fromJson(old);const Item &oj=o.sheets[0].items[o.sheets[0].items.size()-2],&ot=o.sheets[0].items.last();
                require(oj.sizeStep==3&&!ot.reversed,"version 12: step L, the usual direction");}
            QStringList losses;const Document back=splan::read(splan::write(d,80,&losses));
            const Item &bj=back.sheets[0].items[back.sheets[0].items.size()-2],&bt=back.sheets[0].items.last();
            require(bj.autoSize&&bj.sizeStep==1&&bt.reversed,"in sPlan 8 as well");
            Document off=back;Item &oj=off.sheets[0].items[off.sheets[0].items.size()-2];oj.autoSize=false;oj.sizeStep=4;oj.pen.color=QColor(0,0,255);
            const Document again=splan::read(splan::write(off,80,&losses));const Item &aj=again.sheets[0].items[again.sheets[0].items.size()-2];
            require(!aj.autoSize&&aj.sizeStep==4&&aj.pen.color==QColor(0,0,255),"switched off, step and colour kept");
            // The size as the reference: the widest line under the centre times 3 to 7, its colour; else 0.1 mm and black.
            Sheet s;Item thin;thin.type=ItemType::Line;thin.points={QPointF(0,40),QPointF(80,40)};thin.pen.width=.3;thin.pen.color=QColor(255,0,0);
            Item wide=thin;wide.points={QPointF(40,0),QPointF(40,80)};wide.pen.width=.5;wide.pen.color=QColor(0,128,0);
            s.items<<thin;Item jj=j;jj.autoSize=true;
            for(int step=0;step<5;step++){jj.sizeStep=step;require(std::abs(junctionLook(jj,s).first-.3*(3+step))<1e-9,"XS to XL: 3 to 7 times the line's width");}
            require(junctionLook(jj,s).second==QColor(255,0,0),"the line's colour");
            s.items<<wide;jj.sizeStep=3;require(std::abs(junctionLook(jj,s).first-.5*6)<1e-9&&junctionLook(jj,s).second==QColor(0,128,0),"the widest line, the last one's colour");
            jj.pos=QPointF(40,40.2);require(std::abs(junctionLook(jj,s).first-.5*6)<1e-9,"within the stroke");
            jj.pos=QPointF(10,10);require(std::abs(junctionLook(jj,s).first-.6)<1e-9&&junctionLook(jj,s).second==QColor(0,0,0),"none: 0.1 mm and black");
            Item grouped;grouped.type=ItemType::Group;grouped.children={thin};s.items={grouped};jj.pos=QPointF(40,40);
            require(std::abs(junctionLook(jj,s).first-.6)<1e-9,"lines in groups do not count");}

        // --- Own file format
        {const Document d=everything();const QByteArray bytes=encode(d);
            require(bytes.contains("\"format\": \"OpenLoch Schematic\"")&&bytes.contains("\"version\": 13")&&!bytes.contains('\r'),"format, version, LF");
            const Document back=decode(bytes);
            require(back==d,"everything read back as written");
            require(encode(back)==bytes,"written again byte for byte");
            QTemporaryDir dir;const QString file=dir.filePath("plan.olsch");save(d,file);require(load(file)==d,"saved and loaded");
            // Broken files are refused with a message; an existing file stays as it was.
            auto edited=[&](const std::function<void(QJsonObject&)> &change){QJsonObject o=toJson(d);change(o);return QJsonDocument(o).toJson();};
            auto sheetItem=[](QJsonObject &o,int sheet,int item,const std::function<void(QJsonObject&)> &change){
                auto sheets=o["sheets"].toArray();auto s=sheets[sheet].toObject();auto items=s["items"].toArray();auto i=items[item].toObject();
                change(i);items[item]=i;s["items"]=items;sheets[sheet]=s;o["sheets"]=sheets;};
            rejects([&]{decode("{");},"truncated JSON");
            rejects([&]{decode(bytes.left(bytes.size()/2));},"half a file");
            rejects([&]{decode(edited([](QJsonObject &o){o["format"]="OpenLoch PCB";}));},"another format");
            require(rejection([&]{decode(edited([](QJsonObject &o){o["version"]=formatVersion+1;}));}).contains("neueren Version"),"a newer version is refused with a message");
            // Version 1: one grid for the document and one, filled, square line end.
            {QJsonObject o=toJson(d);o["version"]=1;o["grid"]=2.54;
                auto sheets=o["sheets"].toArray();for(int k=0;k<sheets.size();k++){auto x=sheets[k].toObject();x.remove("grid");sheets[k]=x;}o["sheets"]=sheets;
                sheetItem(o,1,0,[](QJsonObject &i){i["endEnd"]="square";});
                const Document old=decode(QJsonDocument(o).toJson());
                require(old.sheets[0].grid==2.54&&old.sheets[1].grid==2.54,"version 1: the document's grid for each sheet");
                require(old.sheets[1].items[0].endEnd==LineEnd::FilledSquare,"version 1: the square line end was filled");}
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,0,[](QJsonObject &i){i["splan"]="not base64!";});}));},"kept sPlan bytes that are no Base64");
            // Version 2 knows no parents and children.
            {QJsonObject o=toJson(d);o["version"]=2;require(decode(QJsonDocument(o).toJson())==d,"version 2 is read");}
            {QJsonObject o=toJson(d);o["version"]=3;require(decode(QJsonDocument(o).toJson())==d,"version 3 (no links) is read");}
            // Guide lines (version 5).
            {Document g=d;g.sheets[0].horizontalGuides={12.5};g.sheets[1].verticalGuides={30,40.25};
                require(decode(encode(g))==g&&encode(decode(encode(g)))==encode(g),"guide lines kept");
                QJsonObject o=toJson(g);o["version"]=4;require(decode(QJsonDocument(o).toJson())==d,"version 4 has no guide lines");
                auto withGuides=[&](QJsonValue v){QJsonObject x=toJson(g);auto sheets=x["sheets"].toArray();auto s=sheets[0].toObject();s["guides"]=v;sheets[0]=s;x["sheets"]=sheets;return QJsonDocument(x).toJson();};
                rejects([&]{decode(withGuides(QJsonArray{1,2}));},"guide lines that are no object");
                rejects([&]{decode(withGuides(QJsonObject{{"vertical",QJsonArray{QStringLiteral("x")}}}));},"a guide line that is no number");
                rejects([&]{decode(withGuides(QJsonObject{{"horizontal",QJsonArray{1e9}}}));},"a guide line too far away");}
            // Dimensions and the sheet's scale (version 6).
            {Document g=d;g.sheets[0].scale=2.5;
                Item dim;dim.type=ItemType::Dimension;dim.dimension=DimensionKind::Angle;dim.points={QPointF(10,0),QPointF(0,-10),QPointF(0,0)};dim.offset=3;dim.arrowAngle=20;dim.arrowLength=3;
                dim.extensionColor=QColor(255,0,0);dim.lineColor=QColor(0,0,255);dim.digits=2;dim.decimalPoint=true;dim.showDiameter=true;dim.autoValue=false;dim.fixedValue="F";
                dim.prefix="P";dim.suffix="S";dim.upperTolerance="+1";dim.lowerTolerance="-1";dim.font.family="Courier New";assignIds(dim);g.sheets[0].items<<dim;
                require(decode(encode(g))==g&&encode(decode(encode(g)))==encode(g),"a dimension and the scale kept");
                QJsonObject o=toJson(g);o["version"]=5;require(rejection([&]{decode(QJsonDocument(o).toJson());}).size()>0,"version 5 knows no dimensions");
                Document s=d;s.sheets[0].scale=2.5;QJsonObject p=toJson(s);p["version"]=5;require(decode(QJsonDocument(p).toJson())==d,"version 5 has no scale");
                auto withPoints=[&](QJsonArray pts){QJsonObject x=toJson(g);sheetItem(x,0,int(g.sheets[0].items.size())-1,[&](QJsonObject &i){i["points"]=pts;});return QJsonDocument(x).toJson();};
                rejects([&]{decode(withPoints(QJsonArray{QJsonArray{0,0},QJsonArray{1,1}}));},"an angle with two points");}
            // Sheet numbers on the tabs, designators with sheet number and prefix (version 10).
            {Document g=d;g.sheetNumbers=false;g.designatorPageNumbers=true;g.designatorPrefix=QStringLiteral("=A-");
                require(decode(encode(g))==g&&encode(decode(encode(g)))==encode(g),"kept");
                require(!encode(d).contains("sheetNumbers")&&!encode(d).contains("designatorP"),"nothing written when standard");
                QJsonObject o=toJson(g);o["version"]=9;Document expected=g;expected.sheetNumbers=true;expected.designatorPageNumbers=false;expected.designatorPrefix.clear();
                require(decode(QJsonDocument(o).toJson())==expected,"version 9: tabs numbered, designators as entered");
                QJsonObject x=toJson(g);x["designatorPrefix"]=3;rejects([&]{decode(QJsonDocument(x).toJson());},"a prefix that is no text");}
            // The print offset from the printable area (version 11); before, the sheet's corner from the paper's.
            {Document g=d;g.sheets[0].print.offset=QPointF(5,-3);
                QRectF content;for(const auto &i:g.sheets[0].titleBlock.items)content|=bounds(i);for(const auto &i:g.sheets[0].items)content|=bounds(i);
                require(!content.isNull()&&decode(encode(g))==g,"kept");
                QJsonObject o=toJson(g);o["version"]=10;Document expected=g;expected.sheets[0].print.offset+=content.topLeft();
                require(decode(QJsonDocument(o).toJson())==expected,"version 10: moved by the content's corner");
                g.sheets[0].print.offset=QPointF();QJsonObject z=toJson(g);z["version"]=10;require(decode(QJsonDocument(z).toJson())==g,"no offset stays none");}
            // How sheets are printed (version 9).
            {Document g=d;PrintSettings &p=g.sheets[0].print;p.free=true;p.scale=1.5;p.offset=QPointF(5,-3);p.orientation=PrintSettings::Orientation::Portrait;p.bannerX=2;p.bannerY=3;p.overlap=10;
                require(decode(encode(g))==g&&encode(decode(encode(g)))==encode(g)&&!encode(d).contains("\"print\""),"print settings kept, none written when standard");
                QJsonObject o=toJson(g);o["version"]=8;Document expected=g;expected.sheets[0].print=PrintSettings();
                require(decode(QJsonDocument(o).toJson())==expected,"version 8 has none");
                QJsonObject x=toJson(g);auto sheets=x["sheets"].toArray();auto s0=sheets[0].toObject();auto pr=s0["print"].toObject();pr["pages"]=QJsonArray{0,1};s0["print"]=pr;sheets[0]=s0;x["sheets"]=sheets;
                rejects([&]{decode(QJsonDocument(x).toJson());},"no pages across");}
            // The unit of the scale (version 8).
            {Document g=d;g.sheets[1].scale=2;g.sheets[1].scaleUnit=ScaleUnit::Kilometre;
                require(decode(encode(g))==g&&encode(decode(encode(g)))==encode(g),"the scale's unit kept");
                QJsonObject o=toJson(g);o["version"]=7;Document expected=g;expected.sheets[1].scaleUnit=ScaleUnit::Millimetre;
                require(decode(QJsonDocument(o).toJson())==expected,"version 7 has no unit");
                QJsonObject x=toJson(g);auto sheets=x["sheets"].toArray();auto s1=sheets[1].toObject();s1["scaleUnit"]="mile";sheets[1]=s1;x["sheets"]=sheets;
                rejects([&]{decode(QJsonDocument(x).toJson());},"an unknown unit");}
            // Second colour, stripes and struck out texts (version 7).
            {Document g=d;Item line;line.type=ItemType::Line;line.points={QPointF(0,0),QPointF(20,0)};line.pen.style=PenStyle::Dash;
                line.pen.twoColour=true;line.pen.color2=QColor(0,128,0);line.pen.inner=true;line.pen.innerColor=QColor(1,2,3);line.pen.cross=true;line.pen.crossColor=QColor(4,5,6);
                Item text;text.type=ItemType::Text;text.text=QStringLiteral("X");text.font.strikeOut=true;
                assignIds(line);assignIds(text);g.sheets[0].items<<line<<text;const int n=int(g.sheets[0].items.size());
                require(decode(encode(g))==g&&encode(decode(encode(g)))==encode(g),"stripes and struck out texts kept");
                QJsonObject o=toJson(g);o["version"]=6;Document expected=g;
                {Item &l=expected.sheets[0].items[n-2];const Pen plain;l.pen.twoColour=false;l.pen.color2=plain.color2;l.pen.inner=false;l.pen.innerColor=plain.innerColor;
                    l.pen.cross=false;l.pen.crossColor=plain.crossColor;expected.sheets[0].items[n-1].font.strikeOut=false;}
                require(decode(QJsonDocument(o).toJson())==expected,"version 6 has no stripes and no struck out texts");
                auto withPen=[&](const char *key,QJsonValue v){QJsonObject x=toJson(g);sheetItem(x,0,n-2,[&](QJsonObject &i){auto p=i["pen"].toObject();p[key]=v;i["pen"]=p;});return QJsonDocument(x).toJson();};
                rejects([&]{decode(withPen("color2",QStringLiteral("rot")));},"a second colour that is no colour");
                rejects([&]{decode(withPen("cross",1));},"cross stripes that are no switch");}
            rejects([&]{decode(edited([](QJsonObject &o){o["version"]=0;}));},"version 0");
            rejects([&]{decode(edited([](QJsonObject &o){o["unit"]="inch";}));},"another unit");
            rejects([&]{decode(edited([](QJsonObject &o){o["sheets"]=QJsonArray();}));},"no sheet");
            rejects([&]{decode(edited([](QJsonObject &o){o["activeSheet"]=5;}));},"active sheet out of range");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,0,[](QJsonObject &i){i["points"]=QJsonArray{QJsonArray{1,2}};});}));},"a line with one point");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,0,[](QJsonObject &i){i["points"]=QJsonArray{QJsonArray{1,2},QJsonArray{1e9,0}};});}));},"a point far away");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,0,[](QJsonObject &i){auto p=i["pen"].toObject();p["color"]="red";i["pen"]=p;});}));},"a colour by name");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,0,[](QJsonObject &i){auto p=i["pen"].toObject();p["style"]="wavy";i["pen"]=p;});}));},"an unknown pen style");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,0,[](QJsonObject &i){i["type"]="spline";});}));},"an unknown element");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,0,[](QJsonObject &i){i["electrical"]="yes";});}));},"a switch as text");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,1,[&](QJsonObject &i){i["id"]=d.sheets[1].items[0].id;});}));},"a duplicate id");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,1,[&](QJsonObject &i){i.remove("id");});}));},"a missing id");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,2,[](QJsonObject &i){i["points"]=QJsonArray{QJsonArray{0,0},QJsonArray{1,1},QJsonArray{2,2}};});}));},"a Bézier curve with three points");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,8,[](QJsonObject &i){i["resource"]="00";});}));},"a missing picture");
            rejects([&]{decode(edited([&](QJsonObject &o){auto r=o["resources"].toObject();for(const auto &k:r.keys()){auto x=r[k].toObject();x["data"]="AAAA";r[k]=x;}o["resources"]=r;}));},"a picture that does not match its key");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,9,[](QJsonObject &i){auto c=i["children"].toArray();auto g=c[1].toObject();g["text"]="  ";c[1]=g;i["children"]=c;});}));},"an empty net label");
            // A contact or a designator text outside a component.
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,0,[](QJsonObject &i){i=QJsonObject{{"id","x1"},{"type","contact"},{"text","1"}};});}));},"a contact outside a component");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,0,[](QJsonObject &i){i=QJsonObject{{"id","x1"},{"type","text"},{"role","value"}};});}));},"a value text outside a component");
            rejects([&]{decode(edited([&](QJsonObject &o){sheetItem(o,1,10,[](QJsonObject &i){auto c=i["children"].toArray();c.append(QJsonObject{{"id","x2"},{"type","component"}});i["children"]=c;});}));},"a component inside a component");
            // Saving over an existing file with an invalid document leaves it unchanged.
            Document broken=d;broken.sheets[1].items[0].points={QPointF(1,1)};
            rejects([&]{save(broken,file);},"a document the reader would refuse was saved");
            require(load(file)==d,"the file is unchanged after a failed save");}

        // --- Nets: the rules of nets.h, one by one
        {Item a=exampleSymbol(),b=exampleSymbol();a.pos=QPointF(0,0);b.pos=QPointF(50,0);
            const QString a2=contactNamed(a,"2")->id,b1=contactNamed(b,"1")->id;
            Document d=documentWith({{a,b,conductor({QPointF(15.24,0),QPointF(50,0)})}});
            const auto &items=d.sheets[0].items;
            auto nets=deriveNets(d);
            const Net *n=netOf(nets,{0,items[0].id,contactNamed(items[0],"2")->id});
            require(n&&n->pins.size()==2&&n->conductors.size()==1&&n->pins.contains(PinRef{0,items[1].id,contactNamed(items[1],"1")->id}),"rule 3: a conductor's ends at contacts join them");
            require(netOf(nets,{0,items[0].id,contactNamed(items[0],"1")->id})->pins.size()==1,"an open contact is a net of its own");
            (void)a2;(void)b1;
            // Rule 2: end to end and T.
            d=documentWith({{conductor({QPointF(0,0),QPointF(10,0)}),conductor({QPointF(10,0),QPointF(10,10)}),conductor({QPointF(5,0),QPointF(5,-10)}),conductor({QPointF(20,0),QPointF(30,0)})}});
            nets=deriveNets(d);const auto &l=d.sheets[0].items;
            require(together(nets,l[0].id,l[1].id)&&together(nets,l[0].id,l[2].id)&&!together(nets,l[0].id,l[3].id),"rule 2: ends meet, a T joins, a gap does not");
            // Rules 4 and 5: crossing with and without a junction.
            d=documentWith({{conductor({QPointF(0,0),QPointF(10,0)}),conductor({QPointF(5,-5),QPointF(5,5)})}});
            require(!together(deriveNets(d),d.sheets[0].items[0].id,d.sheets[0].items[1].id),"rule 5: crossing without a junction");
            d=documentWith({{conductor({QPointF(0,0),QPointF(10,0)}),conductor({QPointF(5,-5),QPointF(5,5)}),junction(QPointF(5,0))}});
            require(together(deriveNets(d),d.sheets[0].items[0].id,d.sheets[0].items[1].id),"rule 4: a junction joins crossing conductors");
            d=documentWith({{conductor({QPointF(0,0),QPointF(10,0)}),conductor({QPointF(5,-5),QPointF(5,5)}),junction(QPointF(5.2,0))}});
            require(!together(deriveNets(d),d.sheets[0].items[0].id,d.sheets[0].items[1].id),"a junction beside the crossing joins nothing");
            d=documentWith({{conductor({QPointF(0,0),QPointF(10,0)}),conductor({QPointF(10.005,0),QPointF(20,0)}),conductor({QPointF(30,0),QPointF(40,0)}),conductor({QPointF(40.02,0),QPointF(50,0)})}});
            require(together(deriveNets(d),d.sheets[0].items[0].id,d.sheets[0].items[1].id)&&!together(deriveNets(d),d.sheets[0].items[2].id,d.sheets[0].items[3].id),"points meet closer than the tolerance only");
            // Rule 1: drawings take no part.
            Item drawing=conductor({QPointF(15.24,0),QPointF(50,0)});drawing.electrical=false;
            d=documentWith({{a,b,drawing}});
            require(netOf(deriveNets(d),{0,d.sheets[0].items[0].id,contactNamed(d.sheets[0].items[0],"2")->id})->pins.size()==1,"rule 1: a line marked as drawing connects nothing");
            Item bez;bez.type=ItemType::Bezier;bez.points={QPointF(15.24,0),QPointF(20,5),QPointF(40,5),QPointF(50,0)};
            d=documentWith({{a,b,bez}});
            require(netOf(deriveNets(d),{0,d.sheets[0].items[0].id,contactNamed(d.sheets[0].items[0],"2")->id})->pins.size()==1,"rule 1: a Bézier curve connects nothing");
            Item touching=b;touching.pos=QPointF(15.24,0);
            d=documentWith({{a,touching}});
            require(netOf(deriveNets(d),{0,d.sheets[0].items[0].id,contactNamed(d.sheets[0].items[0],"2")->id})->pins.size()==2,"rule 3: contact on contact");
            // A component's own lines are its drawing: a conductor crossing the symbol does not reach other contacts.
            d=documentWith({{a,conductor({QPointF(10.16,-5),QPointF(10.16,10)})}});
            nets=deriveNets(d);
            require(netOf(nets,{0,d.sheets[0].items[0].id,contactNamed(d.sheets[0].items[0],"3")->id})->conductors.size()==1,"rule 3: a contact anywhere along a conductor");
            require(netOf(nets,{0,d.sheets[0].items[0].id,contactNamed(d.sheets[0].items[0],"1")->id})->conductors.isEmpty(),"the symbol's lines are no conductors");
            // Contacts without a connection point (as read from sPlan) take no part.
            Item c=a;for(auto &k:c.children)k.hasPin=false;
            d=documentWith({{c,conductor({QPointF(15.24,0),QPointF(50,0)})}});
            nets=deriveNets(d);require(nets.size()==1&&nets[0].pins.isEmpty(),"contacts without connection points");
            // Rules 6 and 7: names.
            d=documentWith({{conductor({QPointF(0,0),QPointF(10,0)}),label("A",QPointF(5,0)),conductor({QPointF(0,20),QPointF(10,20)}),label("A",QPointF(0,20)),label("B",QPointF(30,30))},
                            {conductor({QPointF(0,0),QPointF(10,0)}),label("A",QPointF(10,0))}});
            nets=deriveNets(d);const auto &s0=d.sheets[0].items,&s1=d.sheets[1].items;
            require(together(nets,s0[0].id,s0[2].id),"rule 6: equal names join on a sheet");
            require(!together(nets,s0[0].id,s1[0].id),"rule 6: plain names do not reach another sheet");
            require(nets[0].name=="A"&&nets[0].conductors.size()==2,"named nets first");
            require(!together(nets,s0[4].id,s0[0].id),"a label beside every conductor names a net of its own");
            d.sheets[1].items<<label("A",QPointF(0,0),true);assignIds(d.sheets[1].items.last());
            d.sheets[0].items<<label("A",QPointF(100,100),true);assignIds(d.sheets[0].items.last());
            nets=deriveNets(d);
            require(together(nets,d.sheets[0].items[0].id,d.sheets[1].items[0].id),"rule 7: a sheet reference joins sheets, with the plain labels of its name");}

        // --- The first example: two sheets, a symbol twice, a conductor, a text; editing one instance
        {Document d=exampleDocument();
            require(d.sheets.size()==2&&d.sheets[0].width==297&&d.sheets[1].width==420&&!d.sheets[0].titleBlock.items.isEmpty(),"two sheets of different sizes with a title block");
            const auto parts=components(d.sheets[0]);require(parts.size()==2&&parts[0]->designator=="U1"&&parts[1]->designator=="U2","two instances");
            const Item u1=*parts[0],u2=*parts[1];
            auto nets=deriveNets(d);
            const Net *n=netOf(nets,{0,u1.id,contactNamed(u1,"2")->id});
            require(n&&n->pins.size()==2&&n->pins.contains(PinRef{0,u2.id,contactNamed(u2,"1")->id})&&n->conductors.size()==1,"the conductor joins contact 2 of U1 and contact 1 of U2");
            require(nets.size()==5,"five nets: the joined one and four open contacts");
            // Change U2 numerically, turn and mirror it: U1 stays exactly as it was, ids and contacts of U2 stay.
            Item &edit=*components(d.sheets[0])[1];
            edit.pos+=QPointF(12.7,-6.35);rotate(edit,edit.pos,90);mirror(edit,edit.pos.x());
            require(*components(d.sheets[0])[0]==u1,"the other instance is unchanged");
            QStringList before,after;for(const auto &k:u2.children)before<<k.id+k.name+k.text;for(const auto &k:edit.children)after<<k.id+k.name+k.text;
            require(before==after&&edit.id==u2.id,"ids and contact texts stay");
            require(near(edit.rotation,270)&&edit.mirrored,"turned and mirrored");
            nets=deriveNets(d);require(netOf(nets,{0,u1.id,contactNamed(u1,"2")->id})->pins.size()==1,"moved away, the contact is open");
            // Sheets: title, size and order; ids and the active sheet survive saving.
            const QString firstId=d.sheets[0].id;
            d.sheets[1].name=QStringLiteral("Netzteil");d.sheets[1].width=594;d.sheets[1].height=420;d.sheets.move(1,0);d.activeSheet=1;
            QTemporaryDir dir;save(d,dir.filePath("a.olsch"));const Document back=load(dir.filePath("a.olsch"));
            require(back==d&&back.sheets[0].name=="Netzteil"&&back.sheets[1].id==firstId&&back.activeSheet==1,"sheets reordered, renamed and resized survive saving");
            require(deriveNets(back).size()==nets.size(),"nets derived again after loading");}

        // --- Drawing without a window
        {const Document d=exampleDocument();RenderOptions o;
            const QImage image=renderSheet(d,0,4,o);
            require(image.width()==1188&&image.height()==840&&std::lround(image.dotsPerMeterX()/1000.)==4,"picture size from the sheet");
            auto dark=[&](QPointF mm){const QColor c=image.pixelColor(int(mm.x()*4),int(mm.y()*4));return c.lightness()<200;};
            const Item &wire=d.sheets[0].items[2];
            require(dark((wire.points[0]+wire.points[1])/2),"the conductor is drawn");
            require(dark(QPointF(10,100))&&!dark(QPointF(5,100)),"the title block frame is drawn");
            o.titleBlock=false;require(renderSheet(d,0,4,o).pixelColor(40,400)==QColor(255,255,255),"no frame without the title block");}
        // Lines as the reference draws them: dashes in pen widths with round ends, the second colour in the gaps, a stripe
        // of half the width along the middle, short stripes across every 6.4 widths.
        {Document d=exampleDocument();d.sheets[0].items.clear();RenderOptions o;o.titleBlock=false;
            Item line;line.type=ItemType::Line;line.points={QPointF(20,50),QPointF(80,50)};line.pen={QColor(0,0,0),2,PenStyle::Dash};assignIds(line);d.sheets[0].items<<line;
            auto at=[&](const Document &x,double along,double across,const RenderOptions &options){return renderSheet(x,0,4,options).pixelColor(int((20+along)*4),int((50+across)*4));};
            auto close=[](QColor a,QColor b){return std::abs(a.red()-b.red())<40&&std::abs(a.green()-b.green())<40&&std::abs(a.blue()-b.blue())<40;};
            const QColor black(0,0,0),white(255,255,255),red(255,0,0),yellow(255,255,0),blue(0,0,255);
            require(close(at(d,8,.7,o),black)&&close(at(d,21,.7,o),white)&&close(at(d,29,.7,o),black),"a dash of 8 widths, a gap of 5");
            Document s=d;Pen &p=s.sheets[0].items[0].pen;p.twoColour=true;p.inner=true;p.cross=true;
            require(close(at(s,21,.7,o),red),"the second colour in the gap");
            require(close(at(s,8,0,o),yellow)&&close(at(s,21,0,o),yellow)&&close(at(s,8,.7,o),black),"the stripe along the middle, half as wide, also over the gap");
            require(close(at(s,.8,.7,o),blue)&&close(at(s,13.6,.7,o),blue)&&!close(at(s,7,.7,o),blue),"stripes across at the start and every 6.4 widths");
            RenderOptions bw=o;bw.blackAndWhite=true;
            require(close(at(s,21,.7,bw),white)&&close(at(s,8,0,bw),black),"black and white: the line alone");}
        // ZIP: files with folders, packed or stored, back as they were; broken ones and unsafe paths refused.
        {QByteArray big(5000,'a');for(int i=0;i<big.size();i+=7)big[i]='b';
            const QDateTime t(QDate(2026,10,9),QTime(17,30,24));
            const QByteArray zip=writeZip({{QStringLiteral("Seite.olschlib"),big,t},{QStringLiteral("Ordner/Ä.txt"),QByteArray("x"),t},{QStringLiteral("leer"),QByteArray(),t}});
            const auto back=readZip(zip);
            require(back.size()==3&&back[0].data==big&&back[1].path==QString::fromUtf8("Ordner/Ä.txt")&&back[1].data=="x"&&back[2].data.isEmpty()&&back[0].modified==t,"files back with names, contents and times");
            require(zip.size()<big.size(),"packed");
            QByteArray broken=zip;broken[60]=char(broken[60]^0x55);require(rejection([&]{readZip(broken);}).size()>0,"a changed byte of the packed data is noticed");
            require(rejection([&]{readZip(QByteArray("no zip"));}).size()>0,"no ZIP");
            require(rejection([&]{readZip(writeZip({{QStringLiteral("../evil"),QByteArray("x"),t}}));}).size()>0&&rejection([&]{readZip(writeZip({{QStringLiteral("/abs"),QByteArray("x"),t}}));}).size()>0,
                    "paths leaving the folder");}
        // SVG: the sheet in millimetres (a hundredth of one per unit), lines as paths with their pens, hatches clipped,
        // texts as outlines, pictures embedded.
        {Document d=exampleDocument();d.sheets[0].items.clear();d.sheets[0].name=QStringLiteral("A & B");RenderOptions o;o.titleBlock=false;
            Item l;l.type=ItemType::Line;l.points={QPointF(10,10),QPointF(50,10)};l.pen={QColor(255,0,0),.5,PenStyle::Dash};assignIds(l);
            Item h;h.type=ItemType::Ellipse;h.centre=QPointF(80,40);h.size=QSizeF(20,10);h.fill={FillStyle::Cross,QColor(0,0,255),2,.1};assignIds(h);
            Item t;t.type=ItemType::Text;t.text=QStringLiteral("Hallo");t.pos=QPointF(20,60);assignIds(t);
            QImage pic(2,2,QImage::Format_RGB32);pic.fill(Qt::green);QByteArray png;{QBuffer b(&png);b.open(QIODevice::WriteOnly);pic.save(&b,"PNG");}
            const QString key=QString::fromLatin1(QCryptographicHash::hash(png,QCryptographicHash::Sha256).toHex());d.resources.insert(key,{QStringLiteral("png"),png});
            Item im;im.type=ItemType::Image;im.centre=QPointF(100,100);im.size=QSizeF(10,10);im.resource=key;assignIds(im);
            d.sheets[0].items<<l<<h<<t<<im;
            const QString svg=QString::fromUtf8(sheetSvg(d,0,o));
            require(svg.startsWith("<?xml")&&svg.contains("width=\"297mm\" height=\"210mm\" viewBox=\"0 0 29700 21000\"")&&svg.contains("<title>A &amp; B</title>")&&svg.trimmed().endsWith("</svg>"),
                    "an SVG file of the sheet's size, its name as title");
            require(svg.contains("d=\"M1000 1000L5000 1000\"")&&svg.contains("stroke=\"#ff0000\" stroke-width=\"50\"")&&svg.contains("stroke-dasharray=\"400 250\""),
                    "the line in hundredths of a millimetre, its width and dashes");
            require(svg.contains("<clipPath id=\"clip1\">")&&svg.contains("clip-path=\"url(#clip1)\"")&&svg.contains("stroke=\"#0000ff\""),"the hatch clipped to the ellipse");
            require(svg.count("<path")>8&&svg.contains("data:image/png;base64,"),"the text as outlines, the picture embedded");
            o.titleBlock=true;require(QString::fromUtf8(sheetSvg(d,0,o)).count("<path")>svg.count("<path"),"with the title block");
            require(sheetSvg(d,5,o).isEmpty(),"no sheet, no file");}
        // EMF: the sheet in hundredths of a millimetre (header and frame), dashed lines cut into their dashes with round ends.
        {QPainterPath line;line.moveTo(0,0);line.lineTo(10,0);
            const auto pieces=dashPieces(line,{2,1},1);
            require(pieces.size()==4&&pieces[0]==QPolygonF({QPointF(0,0),QPointF(2,0)})&&pieces[3]==QPolygonF({QPointF(9,0),QPointF(10,0)}),"dashes and gaps along a line");
            require(dashPieces(line,{2,1},1,1).first()==QPolygonF({QPointF(0,0),QPointF(1,0)}),"the pattern started at an offset");
            QPainterPath corner;corner.moveTo(0,0);corner.lineTo(3,0);corner.lineTo(3,3);
            require(dashPieces(corner,{4,1},1).first()==QPolygonF({QPointF(0,0),QPointF(3,0),QPointF(3,1)}),"a dash around a corner");
            const auto dots=dashPieces(line,{0,5},1);require(dots.size()==3&&dots[1]==QPolygonF({QPointF(5,0),QPointF(5,0)}),"dots of no length");}
        {Document d=exampleDocument();d.sheets[0].items.clear();RenderOptions o;o.titleBlock=false;
            Item l;l.type=ItemType::Line;l.points={QPointF(10,10),QPointF(50,10)};l.pen={QColor(255,0,0),.5,PenStyle::Dash};assignIds(l);
            d.sheets[0].items<<l;
            auto u32=[](const QByteArray &b,qsizetype at){return qFromLittleEndian<quint32>(b.constData()+at);};
            auto records=[&](const QByteArray &b,quint32 type){int n=0;for(qsizetype at=0;at+8<=b.size();){const quint32 t=u32(b,at),size=u32(b,at+4);if(size<8)break;n+=t==type;at+=size;}return n;};
            const QByteArray dashed=sheetEmf(d,0,o);
            require(dashed.size()>108&&u32(dashed,0)==1&&u32(dashed,40)==0x464D4520,"an EMF file, its header first");
            require(qint32(u32(dashed,24))==0&&qint32(u32(dashed,28))==0&&qint32(u32(dashed,32))==29700&&qint32(u32(dashed,36))==21000,"the frame of the sheet in hundredths of a millimetre");
            d.sheets[0].items[0].pen.style=PenStyle::Solid;const QByteArray solid=sheetEmf(d,0,o);
            // 40 mm in periods of 13 pen widths (6.5 mm): seven dashes where the solid line has one.
            require(records(dashed,27)==records(solid,27)+6,"the dashed line as its dashes");
            require(sheetEmf(d,5,o).isEmpty(),"no sheet, no file");}
        // Hatches as the reference lays them out: lines `lineWidth` wide, `spacing` between them, a full step from the top
        // or left of the upright bounds grown by half the outline, diagonals through its top left or top right corner.
        {Document d=exampleDocument();d.sheets[0].items.clear();RenderOptions o;o.titleBlock=false;
            Item r;r.type=ItemType::Rectangle;r.centre=QPointF(35,30);r.size=QSizeF(30,20);r.pen={QColor(0,0,0),.2,PenStyle::Solid};
            r.fill={FillStyle::Horizontal,QColor(0,0,255),5,1};assignIds(r);d.sheets[0].items<<r;
            auto blue=[&](const Document &x,double mmX,double mmY){const QColor c=renderSheet(x,0,8,o).pixelColor(int(mmX*8),int(mmY*8));return c.blue()>200&&c.red()<80;};
            auto with=[&](FillStyle style,double pen){Document x=d;x.sheets[0].items[0].fill.style=style;x.sheets[0].items[0].pen.width=pen;
                if(pen==0)x.sheets[0].items[0].pen.style=PenStyle::None;return x;};
            const Document h=with(FillStyle::Horizontal,.2);
            require(blue(h,35,25.9)&&blue(h,35,31.9)&&blue(h,35,37.9)&&!blue(h,35,22.9)&&!blue(h,35,28.9)&&!blue(h,35,20.9),"horizontal: every 6 mm from the top");
            const Document v=with(FillStyle::Vertical,.2);require(blue(v,25.9,30)&&blue(v,31.9,30)&&!blue(v,28.9,30),"vertical: every 6 mm from the left");
            require(blue(with(FillStyle::Horizontal,2),35,24.8)&&!blue(with(FillStyle::Horizontal,0),35,24.8),"grown by half the outline, not without one");
            // "/" on x + y = 39.8 + 8.49 k, "\" on y − x = −30.2 + 8.49 k.
            const Document rise=with(FillStyle::Diagonal,.2),fall=with(FillStyle::BackDiagonal,.2);
            require(blue(rise,23.29,25)&&!blue(rise,22.53,30),"rising lines through the top left corner");
            require(blue(fall,43.23,30)&&!blue(fall,38.99,30),"falling lines through the top right corner");
            Document turned=h;turned.sheets[0].items[0].rotation=90;turned.sheets[0].items[0].size=QSizeF(20,30);
            require(blue(turned,35,25.9)&&!blue(turned,35,28.9),"upright also when turned");}

        // --- Own library pages: version 2 carries the pictures of a symbol
        {LibraryPage page;page.name=QString::fromUtf8("Bauteile\rComponents\rComposants");page.folder=QStringLiteral("Elektro");
            LibraryEntry e;e.caption=QStringLiteral("Symbol");e.symbol=exampleSymbol();
            QImage picture(2,2,QImage::Format_RGB32);picture.fill(Qt::red);QByteArray png;{QBuffer b(&png);b.open(QIODevice::WriteOnly);picture.save(&b,"PNG");}
            const QString key=QString::fromLatin1(QCryptographicHash::hash(png,QCryptographicHash::Sha256).toHex());
            Item image;image.type=ItemType::Image;image.centre=QPointF(1,1);image.size=QSizeF(2,2);image.resource=key;e.symbol.children<<image;e.resources.insert(key,{QStringLiteral("png"),png});
            page.entries<<e;
            const QJsonObject json=pageToJson(page);
            require(json["version"].toInt()>=2&&json["symbols"].toArray()[0].toObject().contains("resources"),"from version 2 with the pictures of a symbol");
            const LibraryPage back=pageFromJson(json);
            require(back.name==page.name&&back.folder==page.folder&&back.entries.size()==1&&back.entries[0].resources==e.resources&&back.entries[0].caption=="Symbol","read back");
            QJsonObject old=json;old["version"]=1;auto symbols=old["symbols"].toArray();auto first=symbols[0].toObject();first.remove("resources");
            auto item=first["item"].toObject();auto children=item["children"].toArray();children.removeLast();item["children"]=children;first["item"]=item;symbols[0]=first;old["symbols"]=symbols;
            require(pageFromJson(old).entries.size()==1,"version 1 is read");
            QJsonObject newer=json;newer["version"]=4;require(rejection([&]{pageFromJson(newer);}).contains("neueren Version"),"a newer version is refused");
            // Version 3: a parent's children with the entry; read only from version 3 on, only components.
            {LibraryPage family=page;Item gate=exampleSymbol();gate.designator=QStringLiteral("<PARENT_ID>-<CHILDNO>");gate.pos=QPointF(25.4,0);
                family.entries[0].symbol.parent=true;family.entries[0].children={gate,gate};
                const QJsonObject j=pageToJson(family);{const LibraryEntry back=pageFromJson(j).entries[0];
                    require(j["version"].toInt()==3&&back.children.size()==2&&back.children[1].designator==gate.designator&&near(back.children[1].pos,gate.pos)&&back.symbol.parent,"children of a parent in version 3");}
                QJsonObject two=j;two["version"]=2;require(pageFromJson(two).entries[0].children.isEmpty(),"version 2 knows none");
                QJsonObject bad=j;auto list=bad["symbols"].toArray();auto entry=list[0].toObject();auto kids=entry["children"].toArray();
                Item line;line.type=ItemType::Line;line.points={QPointF(),QPointF(1,0)};kids.append(itemToJson(line));entry["children"]=kids;list[0]=entry;bad["symbols"]=list;
                rejects([&]{pageFromJson(bad);},"a child that is no component");}
            QJsonObject wrong=json;symbols=wrong["symbols"].toArray();first=symbols[0].toObject();auto res=first["resources"].toObject();
            res[key]=QJsonObject{{"kind","png"},{"data",QString::fromLatin1(QByteArray("other").toBase64())}};first["resources"]=res;symbols[0]=first;wrong["symbols"]=symbols;
            rejects([&]{pageFromJson(wrong);},"a picture that does not match its key");
            // Names in several languages, the German one where the language has none.
            require(localized(page.name)=="Bauteile","German name");
            setUiLanguage("en");const QString english=localized(page.name),fallback=localized(QStringLiteral("Nur deutsch"));
            setUiLanguage("fr");const QString french=localized(page.name);setUiLanguage("de");
            require(english=="Components"&&french=="Composants"&&fallback=="Nur deutsch","English and French names");
            QTemporaryDir dir;QDir(dir.path()).mkpath("Elektro/Bauteile");const QString file=dir.filePath("Elektro/Bauteile/seite.olschlib");LibraryPage plain=page;plain.folder.clear();savePage(plain,file);
            const auto pages=folderPages(dir.path());
            require(pages.size()==1&&pages[0].folder=="Elektro/Bauteile"&&pages[0].file==file&&pages[0].entries[0].resources.size()==1,"a page in a subfolder");}

        // --- Numbering the components
        {require(designatorLetters("R12")=="R"&&designatorLetters("R?")=="R"&&designatorLetters("IC")=="IC"&&designatorLetters("T1A2")=="T1A","the letters of designators");
            Document d=newDocument(QStringLiteral("x"));d.sheets<<newSheet(QStringLiteral("Zwei"));
            auto part=[](const char *designator,QPointF at,bool automatic=true){Item c=exampleSymbol();c.designator=QString::fromLatin1(designator);c.pos=at;c.autoNumber=automatic;assignIds(c);return c;};
            Item group;group.type=ItemType::Group;group.children<<part("R9",{60,90});assignIds(group);
            d.sheets[0].items<<part("R5",{100,10})<<part("R2",{10,50})<<part("R7",{10,10})<<part("C4",{50,50})<<part("X1",{80,80},false)<<group;
            d.sheets[1].items<<part("R3",{10,10});
            auto names=[](const Document &x){QStringList out;for(const auto &s:x.sheets)for(const auto *c:components(s))out<<c->designator;return out.join(' ');};
            {Document a=d;require(renumber(a,{})==6&&names(a)=="R1 R2 R3 C1 X1 R4 R5","in the order of the elements, over the sheets, per letters");}
            {Document a=d;NumberingOptions o;o.order=NumberingOptions::Order::Columns;o.raster=20;renumber(a,o);
                require(names(a)=="R4 R2 R1 C1 X1 R3 R5","by columns: down the first column, then the next");}
            {Document a=d;NumberingOptions o;o.order=NumberingOptions::Order::Rows;o.raster=20;renumber(a,o);
                require(names(a)=="R2 R3 R1 C1 X1 R4 R5","by rows: along the first row, then the next");}
            {Document a=d;NumberingOptions o;o.letters=QStringLiteral("C");o.start=10;renumber(a,o);require(names(a)=="R5 R2 R7 C10 X1 R9 R3","only one kind, from a start");}
            {Document a=d;NumberingOptions o;o.sheets={1};o.only={d.sheets[1].items[0].id};renumber(a,o);require(names(a)=="R5 R2 R7 C4 X1 R9 R1","only the chosen sheets and components");}}

        // --- The pages that come with OpenLoch
        {const auto pages=builtInPages();
            require(pages.size()>=30,"the built-in pages");
            // Every page file of the source folder loads (a page that does not read would otherwise just be missing).
            require(pages.size()==QDir(QString(OPENLOCH_SOURCE_DIR)+"/libraries/schematic").entryList({"*.json"},QDir::Files).size(),"every page file of the library loads");
            int symbols=0;Document all=newDocument(QStringLiteral("x"));double x=0;
            for(const auto &p:pages){
                require(p.builtIn&&!localized(p.name).isEmpty()&&!p.folder.isEmpty()&&p.name.count(u'\r')==2,"a page in a folder, named in three languages");
                QSet<QString> captions;
                for(const auto &e:p.entries){
                    require(!e.caption.isEmpty()&&!captions.contains(e.caption)&&e.caption.count(u'\r')==2,"a caption of its own in three languages");captions.insert(e.caption);
                    require(e.children.isEmpty()||e.symbol.parent,"children only with a parent");
                    // The symbol and the children kept with it, these at their places relative to it.
                    for(const Item *symbol:QList<const Item*>{&e.symbol}+[&]{QList<const Item*> l;for(const auto &c:e.children)l<<&c;return l;}()){
                        symbols++;
                        require(symbol->type==ItemType::Component&&!symbol->libraryEntry.isEmpty()&&symbol->parentId.isEmpty(),"each symbol a component");
                        if(symbol!=&e.symbol){require(!captions.contains(symbol->caption)&&symbol->caption.count(u'\r')==2&&!symbol->parent,"a child with a caption of its own");captions.insert(symbol->caption);}
                        QSet<QString> names;
                        for(const auto *c:contacts(*symbol)){
                            require(!names.contains(c->name),"contact names unique within a symbol");names.insert(c->name);
                            require(c->hasPin,"each contact with a connection point");
                            const QPointF at=placement(*symbol).map(c->pin);
                            require(std::abs(at.x()/2.54-std::round(at.x()/2.54))<1e-6&&std::abs(at.y()/2.54-std::round(at.y()/2.54))<1e-6,"connection points on the 2.54 mm pitch");
                        }
                    }
                    // In rows of fifty, within the coordinates a file allows; a parent with its children.
                    QList<Item> placed=placedItems(e,all);const QPointF at(10+std::fmod(x,1000),100+30*std::floor(x/1000));x+=20;
                    for(auto &i:placed){i.pos+=at;all.sheets[0].items<<i;}
                }
            }
            require(symbols>=1100,"more than a thousand symbols");
            // The kind of part boards sort by: from the library page, else the letters as entered.
            {const LibraryEntry *npn=nullptr,*bridge=nullptr;for(const auto &p:pages)for(const auto &e:p.entries){const QString c=e.caption.section(u'\r',0,0);
                    if(c==u"NPN-Transistor")npn=&e;if(c==u"Brückengleichrichter"&&p.name.startsWith(u"Dioden\r"))bridge=&e;}
                require(npn&&bridge&&npn->symbol.designator.startsWith(u'K')&&partKind(npn->symbol)=="T"&&partKind(bridge->symbol)=="B","library parts of their page's kind");
                Item own;own.type=ItemType::Component;own.designator=QStringLiteral("RV3");require(partKind(own)=="RV","own parts by their letters");
                own.designator=QStringLiteral("-R3");const QString iec=partKind(own);own.designator=QStringLiteral("=A1-K2");
                require(iec=="R"&&partKind(own)=="K","reference designations after IEC 81346 by the letters after their last sign");}
            // The 74xx: for every IC the parent and its children use each pin number once, all of them pins of the
            // DIL box with the same number, whose package has 14, 16, 20 or 24 pins numbered from 1.
            {const LibraryPage *dil=nullptr,*pc=nullptr;
                for(const auto &p:pages){const QString n=p.name.section(u'\r',0,0);if(n==u"TTL-74xx")dil=&p;if(n==u"TTL-74xx (Parent/Child)")pc=&p;}
                require(dil&&pc&&dil->entries.size()>=90&&pc->entries.size()==dil->entries.size(),"the two pages of the 74xx");
                // As in the reference every entry of the parent/child page is a parent carrying its children.
                {bool all=true;int children=0;for(const auto &e:pc->entries){all&=e.symbol.parent&&!e.children.isEmpty();children+=int(e.children.size());}
                    require(all&&children>=250,"each IC a parent with its children");}
                auto chipOf=[](const LibraryEntry &e){return e.caption.section(u'\r',0,0).section(u' ',0,0);};
                QHash<QString,QSet<int>> package;
                for(const auto &e:dil->entries){QSet<int> n;for(const auto *c:contacts(e.symbol))n<<c->name.toInt();package.insert(chipOf(e),n);
                    bool numbered=QList<int>{14,16,20,24}.contains(int(n.size()));for(int k=1;k<=int(n.size());k++)numbered&=n.contains(k);
                    require(numbered,"a DIL box with its package's pins");}
                QHash<QString,QList<int>> used;QSet<QString> parents;
                for(const auto &e:pc->entries){if(e.symbol.parent)parents<<chipOf(e);for(const auto *c:contacts(e.symbol))used[chipOf(e)]<<c->name.toInt();
                    for(const auto &child:e.children)for(const auto *c:contacts(child))used[chipOf(e)]<<c->name.toInt();}
                for(auto it=used.cbegin();it!=used.cend();++it){
                    const QSet<int> set(it->begin(),it->end());
                    require(set.size()==it->size(),"each pin of an IC once among its parent and children");
                    require(package.contains(it.key())&&package[it.key()].contains(set),"the pins of the package with the same number");}
                require(parents.size()==package.size()&&QSet<QString>(package.keyBegin(),package.keyEnd())==parents,"a parent for every IC of the DIL page");
                // As in the reference a child shows its parent's designator and its number once placed with it, linked.
                const LibraryEntry *parent=nullptr;
                for(const auto &e:pc->entries)if(e.caption.section(u'\r',0,0)==u"7400 Versorgung (Parent)")parent=&e;
                require(parent&&parent->children.size()==4&&parent->children[1].caption.section(u'\r',0,0)==u"7400 NAND 2 (Child)"&&parent->children[1].designator==u"<PARENT_ID>-<CHILDNO>","a parent and its children of the 7400");
                Document q=newDocument(QStringLiteral("x"));for(const auto &i:placedItems(*parent,q))q.sheets[0].items<<i;
                const Item &pa=q.sheets[0].items[0];
                require(q.sheets[0].items.size()==5&&q.sheets[0].items[2].parentId==pa.id&&q.sheets[0].items[2].pos.x()>bounds(pa).right(),"placed with its children, beside it");
                require(shownDesignator(q.sheets[0].items[2],{&q,0,&q.sheets[0].items[2],{}})==pa.designator+QStringLiteral("-2"),"the second child of IC1 shows IC1-2");}
            // The microcontrollers: each package with its pins numbered from 1, the header of the Raspberry Pi two rows; board
            // outlines have no contacts.
            {int parts=0;
                for(const auto &p:pages){if(!p.folder.endsWith(QStringLiteral("µP")))continue;
                    for(const auto &e:p.entries){parts++;QSet<int> n;for(const auto *c:contacts(e.symbol))n<<c->name.toInt();
                        bool numbered=n.size()==contacts(e.symbol).size();for(int k=1;k<=int(n.size());k++)numbered&=n.contains(k);require(numbered,"a package with its pins numbered from 1");}}
                require(parts>=13,"the microcontroller pages");}
            require(decode(encode(all))==all,"all of them in the own format");
            {QStringList losses;const Document back=splan::read(splan::write(all,80,&losses));require(components(back.sheets[0]).size()==symbols,"and as a sPlan file");}
            // Level by level, folders before pages, by name.
            auto made=[](const char *folder,const char *name){LibraryPage p;p.folder=QString::fromUtf8(folder);p.name=QString::fromUtf8(name);return p;};
            QList<LibraryPage> list{made("B","Zeta"),made("","Beta"),made("B/C","Alpha"),made("B","alpha"),made("A","Omega"),made("","Alpha")};
            sortPages(list);QStringList order;for(const auto &p:list)order<<p.folder+u'|'+p.name;
            require(order.join(' ')=="A|Omega B/C|Alpha B|alpha B|Zeta |Alpha |Beta","the order of sPlan's library");
            // The panel: a menu for each level of folders, the tree, the search, a page kept by its folder and name.
            LibraryPanel panel;panel.resize(220,600);panel.setPages(pages);
            auto texts=[](QMenu *m){QStringList t;for(auto *a:m->actions())if(!a->isSeparator())t<<a->text();return t;};
            const QStringList top=texts(panel.levelMenu(QString()));
            require(top==QStringList({"Elektro","Hydraulik + Pneumatik","Sonstiges","USER"}),"the top level: its folders");
            const QStringList elektro=texts(panel.levelMenu(QStringLiteral("Elektro")));
            require(elektro.value(0)=="<---"&&elektro.value(1)=="Elektroinstallation"&&elektro.value(2)=="Elektronik"&&elektro.contains("Antennen"),"one level up, the folders, then the pages");
            QAction *antennas=nullptr;for(auto *a:panel.levelMenu(QStringLiteral("Elektro"))->actions())if(a->text()=="Antennen")antennas=a;
            antennas->trigger();
            require(localized(panel.pages()[panel.currentPage()].name)=="Antennen"&&panel.symbolList()->count()==panel.pages()[panel.currentPage()].entries.size(),"a page chosen from the menu");
            const QString key=panel.pageKey(panel.currentPage());panel.setCurrentPage(0);
            require(panel.showPage(key)&&localized(panel.pages()[panel.currentPage()].name)=="Antennen"&&!panel.showPage(QStringLiteral("Nirgends|Nichts")),"a page found again by its key");
            panel.showTree(true);require(panel.pageTree()->topLevelItemCount()==4&&panel.pageTree()->currentItem()&&panel.pageTree()->currentItem()->text(0)=="Antennen","the tree of pages");
            panel.showTree(false);
            const auto found=panel.find(QStringLiteral("Z-Diode"));
            bool zener=false;for(const auto &f:found)zener|=localized(panel.pages()[f.first].entries[f.second].caption)=="Z-Diode";
            require(zener,"the search");
            require(!panel.entry(QStringLiteral("999/0"),nullptr)&&!panel.entry(QStringLiteral("x"),nullptr),"no entry for keys that do not fit");}

        // --- Interface texts of the module have English translations
        {QSet<QString> table;for(const auto &[german,english]:translationTable())table.insert(german);
            QStringList untranslated;const QRegularExpression literal(R"re((?<![A-Za-z_])ui\("((?:[^"\\]|\\.)*)"\))re");
            for(const auto *dir:{"/src/modules/schematic","/src/formats/splan"}){
                if(!QFileInfo::exists(QString(OPENLOCH_SOURCE_DIR)+dir))continue;
                for(QDirIterator it(QString(OPENLOCH_SOURCE_DIR)+dir,{"*.cpp"},QDir::Files,QDirIterator::Subdirectories);it.hasNext();){
                    QFile f(it.next());require(f.open(QIODevice::ReadOnly),"cannot read a source file");const auto text=QString::fromUtf8(f.readAll());
                    for(auto m=literal.globalMatch(text);m.hasNext();){auto key=m.next().captured(1);key.replace("\\n","\n").replace("\\\"","\"").replace("\\t","\t").replace("\\\\","\\");
                        if(!table.contains(key))untranslated.append(QFileInfo(f.fileName()).fileName()+": "+key);}
                }
            }
            if(!untranslated.isEmpty())throw std::runtime_error(("texts without English translation: "+untranslated.join(" | ")).toStdString());}
        // --- The title block's grid numbered from "Spalte-Start" and "Zeile-Start"; "Auto" from the sheet's size; the starts
        //     in the own format since version 12 and in sPlan 8
        {Document d=newDocument(QString());Sheet &s=d.sheets[0];s.width=297;s.height=210;
            s.titleBlock.frame=QRectF(10,10,200,100);s.titleBlock.columns=10;s.titleBlock.rows=5;
            require(s.titleBlock.columnStart==1&&s.titleBlock.rowStart==1&&columnAt(s,QPointF(15,15))==1&&rowAt(s,QPointF(15,105))==5,"numbered from 1 by default");
            s.titleBlock.columnStart=25;s.titleBlock.rowStart=3;
            require(columnAt(s,QPointF(15,15))==25&&columnAt(s,QPointF(205,15))==34&&rowAt(s,QPointF(15,105))==7&&columnAt(s,QPointF(5,15))==0&&rowAt(s,QPointF(15,111))==0,
                    "from the starts, 0 outside");
            Item k;k.type=ItemType::Component;k.pos=QPointF(35,35);
            require(expandVariables(QStringLiteral("<COLNUM><COLCHAR> <ROWNUM><ROWCHAR>"),TextContext{&d,0,&k,{}})=="26Z 4D","the variables of the grid count from the starts");
            Sheet a=s;autoGrid(a);
            require(a.titleBlock.frame==QRectF(10,10,277,190)&&a.titleBlock.columns==12&&a.titleBlock.rows==8&&a.titleBlock.columnStart==1&&a.titleBlock.rowStart==1
                    &&a.titleBlock.items==s.titleBlock.items,"Auto: 10 mm inside the sheet, fields of about 23 mm, from 1, the labels kept");
            a.width=77.5;autoGrid(a);const int two=a.titleBlock.columns;a.width=100.5;autoGrid(a);
            require(two==2&&a.titleBlock.columns==4,"a half rounded to the even count, as Delphi's Round");
            s.titleBlock.columnStart=7;s.titleBlock.rowStart=2;
            const QJsonObject json=toJson(d);require(json["version"].toInt()>=12&&fromJson(json)==d,"the starts in the own format, from version 12");
            auto edited=[&](int version,const char *key,int value){
                QJsonObject o=json;o["version"]=version;QJsonArray sheets=o["sheets"].toArray();QJsonObject sheet=sheets[0].toObject();QJsonObject title=sheet["titleBlock"].toObject();
                title[key]=value;sheet["titleBlock"]=title;sheets[0]=sheet;o["sheets"]=sheets;return o;};
            {Document expected=d;expected.sheets[0].titleBlock.columnStart=expected.sheets[0].titleBlock.rowStart=1;
             require(fromJson(edited(11,"columnStart",7))==expected,"version 11 has no starts");}
            for(int bad:{0,10001}){bool refused=false;try{fromJson(edited(12,"rowStart",bad));}catch(const FormatError &){refused=true;}require(refused,"a start out of range is refused");}
            QStringList losses;const Document back=splan::read(splan::write(d,80,&losses));
            require(back.sheets[0].titleBlock.columnStart==7&&back.sheets[0].titleBlock.rowStart==2,"the starts in sPlan 8");
            Document again=back;again.sheets[0].titleBlock.rowStart=9;
            require(splan::read(splan::write(again,80,&losses)).sheets[0].titleBlock.rowStart==9&&splan::read(splan::write(back,80,&losses)).sheets[0].titleBlock.columnStart==7,"changed and kept");}
        // --- The tree of library pages filtered as in sPlan: pages by name and entries by caption, in any language; an entry
        //     found lies under its page and a click chooses it there; no folder without pages; all again without a filter;
        //     the filter stays when the library is read again
        {LibraryPanel panel;panel.resize(220,600);panel.setPages(builtInPages());panel.showTree(true);
            QTreeWidget *tree=panel.pageTree();auto *filter=panel.findChild<QLineEdit*>("libraryFilter");auto *again=panel.findChild<QToolButton*>("libraryReloadTree");
            auto count=[&]{int n=0;for(QTreeWidgetItemIterator it(tree);*it;++it)n++;return n;};
            auto entryItem=[&](const QString &caption)->QTreeWidgetItem*{for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole+1).isValid()&&(*it)->text(0)==caption)return *it;return nullptr;};
            const int all=count();
            require(filter&&again&&!again->isEnabled()&&tree->topLevelItemCount()==4,"the filter row below the tree, reloading only with a library to read");
            filter->setText(QStringLiteral("z-diode"));
            QTreeWidgetItem *hit=entryItem(QStringLiteral("Z-Diode"));bool empty=false;
            for(QTreeWidgetItemIterator it(tree);*it;++it)empty|=!(*it)->data(0,Qt::UserRole).isValid()&&(*it)->childCount()==0;
            require(hit&&hit->parent()&&!hit->parent()->data(0,Qt::UserRole+1).isValid()&&count()<all&&!empty,"an entry found by its caption under its page, no other pages, no empty folders");
            const int page=hit->data(0,Qt::UserRole).toInt(),entry=hit->data(0,Qt::UserRole+1).toInt();
            emit tree->itemClicked(hit,0);
            require(panel.currentPage()==page&&panel.symbolList()->currentRow()==entry&&panel.findChild<QWidget*>("libraryTreeBox")->isHidden(),"a click shows the page with the entry chosen");
            panel.showTree(true);filter->setText(QStringLiteral("antennen"));bool antennas=false;
            for(QTreeWidgetItemIterator it(tree);*it;++it)antennas|=(*it)->text(0)=="Antennen"&&!(*it)->data(0,Qt::UserRole+1).isValid();
            require(antennas,"a page found by its name");
            // An entry with a caption in three languages is found by its English one and shown in the language of the interface.
            QString english,shown;for(const auto &p:panel.pages())for(const auto &e:p.entries){const auto parts=e.caption.split(u'\r');
                if(english.isEmpty()&&parts.size()==3&&parts[1].size()>5&&!parts[0].contains(parts[1],Qt::CaseInsensitive)){english=parts[1];shown=localized(e.caption);}}
            filter->setText(english);require(!english.isEmpty()&&entryItem(shown),"an entry found by its caption in another language");
            filter->clear();require(count()==all,"all pages again without a filter");
            panel.reload=[&]{panel.setPages(builtInPages());};panel.showTree(true);require(again->isEnabled(),"reloading with a library to read");
            filter->setText(QStringLiteral("z-diode"));again->click();
            require(filter->text()=="z-diode"&&entryItem(QStringLiteral("Z-Diode")),"the filter stays when the library is read again");}
        if(schematicEditorTests())return 1;
        if(schematicSplanTests())return 1;
    }catch(const std::exception &e){fprintf(stderr,"schematic test failed: %s\n",e.what());return 1;}
    puts("schematic tests passed");
    return 0;
}
