// Tests of the front panel module: model, native format, geometry, rendering and history.
#include "frontpanel.h"
#include "panelboards.h"
#include "panelgeometry.h"
#include "panelhistory.h"
#include "panelgenerators.h"
#include "fpl.h"
#include "panelrender.h"
#include "language.h"
#include "legacy_reader.h"
#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <functional>
#include <numbers>

using namespace openloch;
using namespace openloch::frontpanel;

void runFormatTests();  // tests/frontpanel_fpl.cpp
void runEditorTests();  // tests/frontpanel_editor.cpp
void runStrokeFontTests();  // tests/frontpanel_strokefont.cpp
void runScaleTests();  // tests/frontpanel_scale.cpp
void runEmfTests();  // tests/frontpanel_emf.cpp
void runHpglTests();  // tests/frontpanel_hpgl.cpp
namespace fptest {
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
void rejects(const std::function<void()> &fn,const char *message){bool rejected=false;try{fn();}catch(const FormatError &){rejected=true;}require(rejected,message);}
bool near(double a,double b,double tolerance){return std::abs(a-b)<=tolerance;}
bool near(QPointF a,QPointF b,double tolerance){return QLineF(a,b).length()<=tolerance;}

Document sampleDocument(){
    Document d;d.title="Muster Ω";
    Panel &p=d.panels[0];p.name="Vorderseite";p.width=120;p.height=60;p.color=QColor("#304050");p.color2=QColor("#a0b0c0");p.gradient=Gradient::Vertical;p.origin={5,55};
    Element line=newElement(ElementType::Line);line.points={{10,10},{30,12},{40,30}};line.pen={QColor("#ff0000"),0.35,PenStyle::Dash};line.contour={Corners::Round,2.5};line.name="Linie 1";
    Element rect=newElement(ElementType::Rectangle);rect.points={{50,10},{70,10},{70,20},{50,20}};rect.fill={FillStyle::Solid,QColor("#ffff00"),QColor("#00ff00"),Gradient::Diagonal};rect.machining=Machining::Mill;
    Element ellipse=newElement(ElementType::Ellipse);ellipse.center={90,30};ellipse.radiusX=12;ellipse.radiusY=6;ellipse.rotation=30;ellipse.fill.style=FillStyle::Cross;
    Element arc=newElement(ElementType::Arc);arc.center={20,45};arc.radiusX=8;arc.radiusY=8;arc.startAngle=10;arc.spanAngle=200;arc.arcStyle=ArcStyle::Pie;
    Element text=newElement(ElementType::Text);text.text="Lautstärke";text.frame=rectFrame(QRectF(40,40,30,6));text.bold=true;text.strokeFont="Einlinig";
    Element drill=newElement(ElementType::Drill);drill.center={100,50};drill.diameter=6.5;
    Element image=newElement(ElementType::Image);QImage pixels(4,3,QImage::Format_RGB32);pixels.fill(Qt::blue);QByteArray bmp;{QBuffer b(&bmp);b.open(QIODevice::WriteOnly);pixels.save(&b,"BMP");}
    image.resource=d.addResource(bmp,"bmp");image.frame=rectFrame(QRectF(2,2,8,6));image.transparent=true;image.transparentColor=QColor("#123456");
    Element group=newElement(ElementType::Group);group.children={line,drill};group.name="Gruppe";group.foreign=QJsonObject{{"kept",42}};
    p.elements={group,rect,ellipse,arc,text,image};
    Panel back=newPanel("Rückseite",80,40);back.gridVisible=false;back.snap=false;back.grid=2.54;d.panels.append(back);d.activePanel=1;
    return d;
}

QStringList untranslatedTexts(){
    QStringList missing;QSet<QString> table;for(const auto &[german,english]:translationTable())table.insert(german);
    const QRegularExpression literal(R"re((?<![A-Za-z_])ui\("((?:[^"\\]|\\.)*)"\))re");
    for(const QString folder:{"/src/modules/frontpanel","/src/formats/frontdesigner"}){
        QDirIterator it(QStringLiteral(OPENLOCH_SOURCE_DIR)+folder,{"*.cpp","*.h"},QDir::Files,QDirIterator::Subdirectories);
        while(it.hasNext()){
            QFile f(it.next());if(!f.open(QIODevice::ReadOnly))continue;const auto text=QString::fromUtf8(f.readAll());
            for(auto m=literal.globalMatch(text);m.hasNext();){auto key=m.next().captured(1);key.replace("\\n","\n").replace("\\\"","\"").replace("\\t","\t").replace("\\\\","\\");if(!table.contains(key))missing.append(QFileInfo(f.fileName()).fileName()+": "+key);}
        }
    }
    return missing;
}
}
using namespace fptest;

static void modelTests(){
    // Native format: everything survives encode → decode, and encoding the result gives the same bytes.
    const Document d=sampleDocument();const QByteArray bytes=d.encode();const Document back=Document::decode(bytes);
    require(back.encode()==bytes,"native round trip changed the document");
    require(back.panels.size()==2&&back.activePanel==1&&back.panels[0].elements.size()==6,"panels or elements lost");
    const auto &g=back.panels[0].elements[0];require(g.type==ElementType::Group&&g.children.size()==2&&g.foreign["kept"].toInt()==42,"group, children or foreign values lost");
    require(g.children[0].contour.corners==Corners::Round&&near(g.children[0].contour.size,2.5,1e-12)&&g.children[0].pen.style==PenStyle::Dash,"contour or pen lost");
    require(back.panels[0].elements[4].strokeFont=="Einlinig"&&back.panels[0].elements[4].text=="Lautstärke","text lost");
    require(back.resources.size()==1&&back.panels[0].elements[5].transparentColor==QColor("#123456"),"resource or transparency lost");
    // Unused resources are not stored.
    Document unused=d;unused.addResource("other","png");require(Document::decode(unused.encode()).resources.size()==1,"an unused resource was stored");
    // Invalid input is refused with a readable error.
    rejects([]{Document::decode("{bad");},"broken JSON accepted");
    rejects([]{Document::decode(R"({"format":"OpenLoch","version":1})");},"a perfboard project was accepted as a front panel");
    rejects([]{Document::decode(R"({"format":"OpenLoch-Frontplatte","version":99,"panels":[{}]})");},"a newer format version was accepted");
    rejects([]{Document::decode(R"({"format":"OpenLoch-Frontplatte","version":1,"panels":[]})");},"a document without panels was accepted");
    auto broken=[&](const std::function<void(QJsonObject&)> &change){auto root=QJsonDocument::fromJson(bytes).object();auto panels=root["panels"].toArray();auto panel=panels[0].toObject();auto elements=panel["elements"].toArray();auto e=elements[1].toObject();
        change(e);elements[1]=e;panel["elements"]=elements;panels[0]=panel;root["panels"]=panels;return QJsonDocument(root).toJson();};
    rejects([&]{Document::decode(broken([](QJsonObject &e){e["type"]="hologram";}));},"an unknown element type was accepted");
    rejects([&]{Document::decode(broken([](QJsonObject &e){e["points"]=QJsonArray{QJsonArray{0,0},QJsonArray{1,1}};}));},"a rectangle with two corners was accepted");
    rejects([&]{Document::decode(broken([](QJsonObject &e){auto pen=e["pen"].toObject();pen["width"]=-1;e["pen"]=pen;}));},"a negative pen width was accepted");
    rejects([&]{Document::decode(broken([](QJsonObject &e){e["fill"]=QJsonObject{{"color","nocolour"}};}));},"an invalid colour was accepted");
    rejects([&]{Document::decode(broken([](QJsonObject &e){e["points"]=QJsonArray{QJsonArray{0,0},QJsonArray{1,"x"},QJsonArray{1,1},QJsonArray{0,1}};}));},"an invalid point was accepted");
    rejects([&]{Document::decode(broken([](QJsonObject &e){e["type"]="image";e["frame"]=QJsonArray{QJsonArray{0,0},QJsonArray{1,0},QJsonArray{0,1}};e["resource"]="missing";}));},"an image without its resource was accepted");
    // Saving writes the file atomically and can be read back; an unwritable target fails without leaving a file.
    QTemporaryDir dir;const QString path=dir.filePath("platte.olfp");d.save(path);require(Document::load(path).encode()==bytes,"saved file differs");
    rejects([&]{d.save(dir.filePath("fehlt/platte.olfp"));},"saving into a missing folder succeeded");
    require(!QFile::exists(dir.filePath("fehlt/platte.olfp")),"a failed save left a file");
}

static void geometryTests(){
    // An ellipse turned, stretched and mirrored stays the image of the original: compare sampled points.
    Element e=newElement(ElementType::Arc);e.center={10,20};e.radiusX=8;e.radiusY=3;e.rotation=25;e.startAngle=30;e.spanAngle=120;
    for(const QTransform &map:{rotationAbout({4,4},37),QTransform::fromScale(2,0.5),QTransform(1,0.4,0,1,3,-2),QTransform::fromScale(-1,1),QTransform(0.3,-1.2,0.9,0.7,5,5)}){
        Element t=e;transformElement(t,map);
        const QPointF start=map.map(ellipsePoint(e,e.startAngle)),end=map.map(ellipsePoint(e,e.startAngle+e.spanAngle)),middle=map.map(ellipsePoint(e,e.startAngle+e.spanAngle/2));
        const QPointF s2=ellipsePoint(t,t.startAngle),e2=ellipsePoint(t,t.startAngle+t.spanAngle),m2=ellipsePoint(t,t.startAngle+t.spanAngle/2);
        const bool sameWay=near(start,s2,1e-6)&&near(end,e2,1e-6),reversed=near(start,e2,1e-6)&&near(end,s2,1e-6);
        require((sameWay||reversed)&&near(middle,m2,1e-6),"a transformed arc does not cover the mapped points");
        for(int k=0;k<12;k++){const QPointF q=map.map(ellipsePoint(e,k*30));const auto mapped=ellipseMap(t).inverted().map(q);require(near(std::hypot(mapped.x(),mapped.y()),1,1e-9),"a mapped ellipse point is off the new ellipse");}
    }
    // Turning about the centre keeps the radii and adds the angle.
    Element r=newElement(ElementType::Ellipse);r.center={50,50};r.radiusX=10;r.radiusY=4;r.rotation=10;transformElement(r,rotationAbout(r.center,30));
    require(near(r.radiusX,10,1e-9)&&near(r.radiusY,4,1e-9)&&near(r.rotation,40,1e-9)&&near(r.center,{50,50},1e-9),"turning an ellipse changed its size or angle wrongly");
    // Contours: a spline square stays inside, chamfers cut the corners, a sharp contour is the polygon itself.
    const QPolygonF square{{0,0},{10,0},{10,10},{0,10}};
    // The spline runs through the edge midpoints and cuts the corners.
    // An open B-spline runs straight from the first point to the middle of the first edge, through the middles of the
    // edges, and straight from the middle of the last edge to the last point.
    {const QPainterPath open=contourPath(QPolygonF{{0,0},{10,0},{10,10}},false,{Corners::Spline,0});
     const int n=open.elementCount();
     require(n==6&&open.elementAt(0)==QPointF(0,0)&&open.elementAt(1)==QPointF(5,0)&&open.elementAt(n-2)==QPointF(10,5)&&open.currentPosition()==QPointF(10,10),"open spline ends");
     // The spline of the original's arcs leaves the two straight pieces out; closed, both are the same.
     const QPainterPath arc=contourPath(QPolygonF{{0,0},{10,0},{10,10}},false,{Corners::ArcSpline,0});
     require(arc.elementAt(0)==QPointF(5,0)&&arc.currentPosition()==QPointF(10,5)&&arc.boundingRect()==QRectF(5,0,5,5),"open arc spline ends");
     require(contourPath(square,true,{Corners::ArcSpline,0})==contourPath(square,true,{Corners::Spline,0}),"closed arc spline");
     Document k;Element bent=newElement(ElementType::Line);bent.points=square;bent.contour={Corners::ArcSpline,0};k.panels[0].elements={bent};
     require(Document::decode(k.encode()).panels[0].elements[0].contour.corners==Corners::ArcSpline,"arc spline in the native format");}
    const auto spline=contourPath(square,true,{Corners::Spline,0});require(spline.boundingRect()==QRectF(0,0,10,10)&&spline.contains(QPointF(5,5))&&!spline.contains(QPointF(0.5,0.5)),"spline corners are not inside the polygon");
    const auto chamfer=contourPoints(square,true,{Corners::Chamfer,2});require(chamfer.size()==10&&chamfer.first()==chamfer.last()&&near(chamfer[0],{0,2},1e-9)&&near(chamfer[1],{2,0},1e-9),"chamfer points differ");
    require(contourPoints(square,false,{Corners::Sharp,2})==square,"a sharp open contour must keep its nodes");
    {const auto round=contourPoints(square,true,{Corners::Round,20});require(round.boundingRect()==QRectF(0,0,10,10)&&!round.containsPoint(QPointF(0.5,0.5),Qt::OddEvenFill),"a large rounding must stay within the edges");}
    // Text fills its frame; a mirrored frame mirrors the text.
    Element text=newElement(ElementType::Text);text.text="ABC";text.frame=rectFrame(QRectF(10,10,30,8));
    const QRectF tb=textPath(text).boundingRect();require(tb.left()>=10-0.5&&tb.right()<=40+0.5&&tb.top()>=10-0.5&&tb.bottom()<=18+0.5&&tb.width()>20,"text does not fill its frame");
    require(hitElement(text,{25,14},0.1)&&!hitElement(text,{25,30},0.1),"text hit test is wrong");
    require(naturalTextWidth(text,8)>8,"the natural text width is too small");
    Element drill=newElement(ElementType::Drill);drill.center={5,5};drill.diameter=4;require(near(elementBounds(drill).width(),4.2,1e-9)&&hitElement(drill,{5,6.5},0),"drill size or hit test is wrong");
    transformElement(drill,QTransform::fromScale(2,2));require(near(drill.diameter,8,1e-12)&&near(drill.center,{10,10},1e-12),"scaling a drill");
    Element line=newElement(ElementType::Line);line.points={{0,0},{10,0}};line.pen.width=1;require(hitElement(line,{5,0.7},0.3)&&!hitElement(line,{5,2},0.3),"line hit test is wrong");
    Element group=newElement(ElementType::Group);group.children={line,drill};{QJsonArray anchors;anchors.append(QJsonArray{1,1});group.parameters["anchors"]=anchors;} // one nested array: some compilers copy instead of nesting
    transformElement(group,QTransform::fromTranslate(3,4));require(group.children[0].points[0]==QPointF(3,4)&&group.parameters["anchors"].toArray()[0].toArray()[0].toDouble()==4,"group transform misses children or anchors");
}

static void renderTests(){
    Document d;Panel &p=d.panels[0];p.width=100;p.height=50;p.color=QColor(200,10,10);
    Element e=newElement(ElementType::Ellipse);e.center={50,25};e.radiusX=10;e.radiusY=10;e.fill={FillStyle::Solid,QColor(0,0,255),Qt::white,Gradient::None};e.pen.style=PenStyle::None;p.elements={e};
    const QImage image=renderPanel(d,p,254);
    require(image.size()==QSize(1000,500),"render size differs from the panel size");
    require(image.pixelColor(10,10)==QColor(200,10,10)&&image.pixelColor(500,250)==QColor(0,0,255),"background or fill colour wrong");
    RenderOptions hide;hide.other=false;require(renderPanel(d,p,254,hide).pixelColor(500,250)==QColor(200,10,10),"a hidden view group was drawn");
    RenderOptions outline;outline.outlineOnly=true;require(renderPanel(d,p,254,outline).pixelColor(500,250)==QColor(Qt::white),"outline view filled an area");
    // A gradient panel runs from colour to colour2.
    p.gradient=Gradient::Horizontal;p.color=Qt::black;p.color2=Qt::white;p.elements.clear();const QImage g=renderPanel(d,p,50.8);
    require(g.pixelColor(0,50).value()<20&&g.pixelColor(g.width()-1,50).value()>235,"panel gradient wrong");
    // A transparent colour of an image is left out.
    QImage pixels(2,1,QImage::Format_RGB32);pixels.setPixelColor(0,0,Qt::white);pixels.setPixelColor(1,0,Qt::red);QByteArray png;{QBuffer b(&png);b.open(QIODevice::WriteOnly);pixels.save(&b,"PNG");}
    Element img=newElement(ElementType::Image);img.resource=d.addResource(png,"png");img.transparent=true;img.transparentColor=Qt::white;
    const QImage shown=resourceImage(d,img);require(shown.pixelColor(0,0).alpha()==0&&shown.pixelColor(1,0)==QColor(Qt::red),"image transparency wrong");
    // A vector picture (EMF) is played into its frame: a header for a 100 × 100 unit picture, an anisotropic mapping,
    // a green brush and a triangle covering the lower left half.
    QByteArray emf;auto u32=[&](quint32 v){char b[4];qToLittleEndian(v,b);emf.append(b,4);};auto rec=[&](quint32 type,const QList<quint32> &values){u32(type);u32(8+4*values.size());for(auto v:values)u32(v);};
    rec(1,{0,0,99,99, 0,0,1000,1000, 0x464D4520,0x10000,0,0,0,0,0,0, 100,100, 10,10, 0,0,0});  // bounds, frame (1/100 mm), signature, …, device 100 px over 10 mm
    rec(17,{8});rec(9,{1000,1000});rec(11,{100,100});rec(39,{1,0,0x00ff00,0});rec(37,{1});rec(37,{0x80000008});
    {u32(86);u32(8+16+4+12);u32(0);u32(0);u32(0);u32(0);u32(3);auto p16=[&](qint16 x,qint16 y){char b[2];qToLittleEndian(x,b);emf.append(b,2);qToLittleEndian(y,b);emf.append(b,2);};p16(0,0);p16(0,1000);p16(1000,1000);}
    rec(14,{0,0,0});
    QByteArray header=emf.left(88);qToLittleEndian(quint32(emf.size()),emf.data()+48);
    Element pic=newElement(ElementType::Picture);pic.resource=d.addResource(emf,"emf");pic.frame=rectFrame(QRectF(10,10,20,20));pic.pen.style=PenStyle::None;
    Panel plain=p;plain.gradient=Gradient::None;plain.color=Qt::white;plain.elements={pic};const QImage vector=renderPanel(d,plain,254);
    require(vector.pixelColor(130,270)==QColor(0,255,0)&&vector.pixelColor(270,130)==QColor(Qt::white),"vector picture not drawn into its frame");
    // Clipping, a stretched bitmap and text in a vector picture of the same size.
    {
        QByteArray e2;auto w32=[&](quint32 v){char c[4];qToLittleEndian(v,c);e2.append(c,4);};auto record=[&](quint32 type,const QList<quint32> &values){w32(type);w32(8+4*values.size());for(auto v:values)w32(v);};
        record(1,{0,0,99,99, 0,0,1000,1000, 0x464D4520,0x10000,0,0,0,0,0,0, 100,100, 10,10, 0,0,0});
        record(17,{8});record(9,{1000,1000});record(11,{100,100});
        // A green rectangle over everything, clipped to the left half; then clipping is switched off again.
        record(30,{0,0,500,1000});record(39,{1,0,0x00ff00,0});record(37,{1});record(37,{0x80000008});record(43,{0,0,1000,1000});record(75,{0,5});
        // A blue 2 × 2 bitmap stretched over the lower right quarter.
        QByteArray info(40,0);qToLittleEndian<quint32>(40,info.data());qToLittleEndian<qint32>(2,info.data()+4);qToLittleEndian<qint32>(2,info.data()+8);
        qToLittleEndian<quint16>(1,info.data()+12);qToLittleEndian<quint16>(24,info.data()+14);
        QByteArray bits;for(int row=0;row<2;row++){for(int x=0;x<2;x++)bits+=QByteArray("\xff\x00\x00",3);bits+=QByteArray(2,0);}
        w32(81);w32(80+info.size()+bits.size());
        for(quint32 v:QList<quint32>{0,0,0,0, 500,500, 0,0,2,2, 80,quint32(info.size()),80+quint32(info.size()),quint32(bits.size()), 0,0x00CC0020, 500,500})w32(v);
        e2+=info;e2+=bits;
        // Red text in bold Arial, 300 units high.
        QByteArray font(104,0);qToLittleEndian<quint32>(82,font.data());qToLittleEndian<quint32>(104,font.data()+4);qToLittleEndian<quint32>(2,font.data()+8);
        qToLittleEndian<qint32>(-300,font.data()+12);qToLittleEndian<qint32>(700,font.data()+28);
        const QString face="Arial";for(int i=0;i<face.size();i++)qToLittleEndian<quint16>(face[i].unicode(),font.data()+40+2*i);e2+=font;
        record(37,{2});record(24,{0x0000ff});record(22,{0});
        const QString text="HH";QByteArray chars;for(QChar c:text){char cc[2];qToLittleEndian<quint16>(c.unicode(),cc);chars.append(cc,2);}
        w32(84);w32(76+chars.size());for(quint32 v:QList<quint32>{0,0,0,0, 1,0,0, 50,50, quint32(text.size()),76,0, 0,0,0,0, 0})w32(v);e2+=chars;
        record(14,{0,0,0});qToLittleEndian(quint32(e2.size()),e2.data()+48);
        Element picture=newElement(ElementType::Picture);picture.resource=d.addResource(e2,"emf");picture.frame=rectFrame(QRectF(10,10,20,20));picture.pen.style=PenStyle::None;
        Panel plain2=p;plain2.gradient=Gradient::None;plain2.color=Qt::white;plain2.elements={picture};const QImage drawn=renderPanel(d,plain2,254);
        require(drawn.pixelColor(150,190)==QColor(0,255,0)&&drawn.pixelColor(250,150)==QColor(Qt::white),"clipping in a vector picture");
        require(drawn.pixelColor(275,275)==QColor(0,0,255),"bitmap in a vector picture");
        int red=0;for(int y=100;y<180;y++)for(int x=100;x<220;x++){const QColor c=drawn.pixelColor(x,y);if(c.red()>200&&c.green()<80&&c.blue()<80)red++;}
        require(red>100,"text in a vector picture");
    }
    // A path bracket under a world transformation, and a pie.
    {
        QByteArray e3;auto w32=[&](quint32 v){char c[4];qToLittleEndian(v,c);e3.append(c,4);};auto record=[&](quint32 type,const QList<quint32> &values){w32(type);w32(8+4*values.size());for(auto v:values)w32(v);};
        auto real=[](float f){quint32 v;std::memcpy(&v,&f,4);return v;};
        record(1,{0,0,99,99, 0,0,1000,1000, 0x464D4520,0x10000,0,0,0,0,0,0, 100,100, 10,10, 0,0,0});
        record(17,{8});record(9,{1000,1000});record(11,{100,100});record(37,{0x80000008});
        record(35,{real(1),real(0),real(0),real(1),real(500),real(0)});   // moved 500 units to the right
        record(59,{});record(27,{0,0});record(54,{400,0});record(54,{400,400});record(54,{0,400});record(61,{});record(60,{});
        record(39,{1,0,0x0000ff,0});record(37,{1});record(62,{0,0,0,0});
        record(36,{real(1),real(0),real(0),real(1),real(0),real(0),1});   // back to no transformation
        record(39,{2,0,0xff0000,0});record(37,{2});record(47,{0,500,400,900, 400,700, 200,500});   // the upper right quarter
        record(14,{0,0,0});qToLittleEndian(quint32(e3.size()),e3.data()+48);
        Element picture=newElement(ElementType::Picture);picture.resource=d.addResource(e3,"emf");picture.frame=rectFrame(QRectF(10,10,20,20));picture.pen.style=PenStyle::None;
        Panel plain3=p;plain3.gradient=Gradient::None;plain3.color=Qt::white;plain3.elements={picture};const QImage drawn=renderPanel(d,plain3,254);
        require(drawn.pixelColor(240,140)==QColor(255,0,0)&&drawn.pixelColor(160,140)==QColor(Qt::white),"filled path under a world transformation");
        require(drawn.pixelColor(165,215)==QColor(0,0,255)&&drawn.pixelColor(125,255)==QColor(Qt::white),"pie in a vector picture");
    }
}

static void generatorTests(){
    // Regular polygon: corners on the radius, or edge midpoints on it.
    const Element hexagon=regularPolygon({10,10},6,5,false,0);require(hexagon.points.size()==6&&near(hexagon.points[0],{15,10},1e-12)&&near(hexagon.points[1],{12.5,10-5*std::sqrt(3.0)/2},1e-9),"regular polygon corners");
    const Element inner=regularPolygon({0,0},4,5,true,45);require(near(QLineF(inner.points[0],inner.points[1]).pointAt(0.5),{0,-5},1e-9),"inner radius of a regular polygon");
    // Dimension: five parts in the order of the original (two arrows, value, two extension lines), value with comma.
    const Element dim=dimension({10,20},{40,20},{10,12});
    require(dim.type==ElementType::Dimension&&dim.children.size()==5&&dim.children[0].children.size()==3&&dim.children[2].type==ElementType::Text&&dim.children[2].text=="30,00","dimension parts");
    require(near(dim.children[0].children[0].points[0],{10,12},1e-9)&&near(dim.children[1].children[0].points[0],{40,12},1e-9),"dimension line not through the third point");
    require(elementBounds(dim.children[2]).bottom()<=12+1e-6&&elementBounds(dim.children[2]).center().x()>20&&elementBounds(dim.children[2]).center().x()<30,"dimension value not above the middle of the line");
    require(near(dimensionValue(dim),30,1e-12)&&dimensionWithValue(dim,25).children[2].text=="25,00"&&near(dimensionWithValue(dim,25).children[1].children[0].points[0],{35,12},1e-9),"setting the dimension value");
    const Element shortDim=dimension({0,0},{3,0},{0,-5});require(shortDim.children[0].children[0].points[1]==QPointF(0,-5)&&elementBounds(shortDim.children[2]).left()>3,"a short dimension puts arrows and value outside");
    Element moved=dim;transformElement(moved,QTransform::fromScale(2,1));const Element rebuilt=regenerate(moved);require(rebuilt.children[2].text=="60,00"&&rebuilt.id==dim.id,"a stretched dimension does not measure again");
    // Scales: a round scale placed at (50, 40) and rebuilt after it was moved; the styles: tests/frontpanel_scale.cpp.
    const ScaleParameters sp(ScaleParameters::RoundLinear);
    const Element sc=scale(sp,QTransform::fromTranslate(50,40));QMap<QString,int> parts;for(const auto &c:sc.children)parts[c.name]=c.children.size();
    require(sc.type==ElementType::Scale&&parts.value("1. Teilung")==11&&parts.value("2. Teilung")==40&&parts.value("Beschriftung")==11&&parts.value("Mittelpunkt")==2&&parts.contains("Lauflinie"),"scale parts");
    Element turned=sc;transformElement(turned,rotationAbout({50,40},30)*QTransform::fromTranslate(10,0));const Element again=regenerate(turned);
    require(near(elementBounds(again).center(),elementBounds(turned).center(),1e-6),"a moved scale is rebuilt elsewhere");
    // Cut-out: DIN frame 96 × 96 gives a 92 × 92 opening, milled inside with the tool.
    CutoutParameters cp;cp.din=true;cp.frameWidth=96;cp.frameHeight=96;cp.tool=3;const Element cut=cutout(cp,QTransform::fromTranslate(60,60));
    const Element &opening=cut.children[1];require(opening.machining==Machining::Mill&&near(opening.pen.width,3,1e-12)&&near(elementBounds(opening).width(),92,1e-9),"DIN cut-out size");
    CutoutParameters own;own.din=false;own.frame=CutoutParameters::Round;own.cut=CutoutParameters::Round;own.cutWidth=20;own.cutHeight=20;own.holesSides=true;own.holesSidesDistance=30;own.holesSidesDiameter=3;own.name="Schalter";
    const Element c2=cutout(own,QTransform());int drills=0;for(const auto &c:c2.children)drills+=c.type==ElementType::Drill;require(drills==2&&c2.children[0].type==ElementType::Ellipse&&c2.name=="Schalter","own cut-out parts");
    const CutoutParameters reread=CutoutParameters::fromCut(own.toCut());require(reread.toJson()==own.toJson(),"cut-out definition files do not round-trip");
    const CutoutParameters ini=CutoutParameters::fromCut("[Name]\r\nName=Test\r\n[Frame]\r\nStyle=1\r\nUserWidth=53\r\nUserHeight=30\r\nDIN=0\r\n[Cut]\r\nStyle=0\r\nUserWidth=27,5\r\nUserHeight=31,5\r\nMilling=5\r\n[Holes]\r\nVertical=0\r\nHorizontal=1\r\nDistanceX=36\r\nDiameterX=3.6\r\n");
    require(ini.name=="Test"&&ini.frame==CutoutParameters::Rectangular&&ini.cutWidth==27.5&&ini.tool==5&&ini.holesSides&&ini.holesSidesDistance==36&&ini.holesSidesDiameter==3.6,"reading a cut-out definition");
    // Generated objects through the FrontDesigner format: a dimension stays a dimension, a scale a group of its parts.
    Document d;d.panels[0].elements={dim,sc,cut};const Document fd=frontdesigner::readFrontDesigner(frontdesigner::writeFrontDesigner(d));
    require(fd.panels[0].elements[0].type==ElementType::Dimension&&fd.panels[0].elements[0].children.size()==5&&near(dimensionValue(fd.panels[0].elements[0]),30,1e-9),"a dimension written as FPL is no dimension");
    require(fd.panels[0].elements[1].type==ElementType::Group&&fd.panels[0].elements[2].type==ElementType::Cutout,"scale or cut-out written as FPL");
}

static void boardTests(){
    // Format version 2: the component of an element and the circuit boards behind a panel survive; version 1 reads.
    Document d=sampleDocument();Panel &p=d.panels[0];
    const QString pot=newId(),board=newId(),other=newId(),plan=newId();
    p.elements[0].children[1].component=pot;p.elements[1].component=other;
    p.boards={BoardBehind{plan,board,{20,15},90,true},BoardBehind{plan,newId(),{0,0},0,false}};
    const QByteArray bytes=d.encode();const Document back=Document::decode(bytes);const auto root=QJsonDocument::fromJson(bytes).object();
    require(back.encode()==bytes&&root["version"].toInt()==2,"version 2 must survive its round trip");
    require(back.panels[0].elements[0].children[1].component==pot&&back.panels[0].elements[1].component==other&&back.panels[0].boards==p.boards,"components or boards behind lost");
    require(!elementToJson(p.elements[1],false).contains("component"),"the component is part of an element's identity, not of its geometry");
    {auto old=QJsonDocument::fromJson(sampleDocument().encode()).object();old["version"]=1;require(Document::fromJson(old).panels[0].boards.isEmpty(),"version 1 must read");}
    auto panelOf=[&](const std::function<void(QJsonObject&)> &change){auto r=root;auto panels=r["panels"].toArray();auto o=panels[0].toObject();change(o);panels[0]=o;r["panels"]=panels;return r;};
    auto boardWith=[&](const std::function<void(QJsonObject&)> &change){return panelOf([&](QJsonObject &o){auto list=o["boards"].toArray();auto b=list[0].toObject();change(b);list[0]=b;o["boards"]=list;});};
    rejects([&]{Document::fromJson(boardWith([](QJsonObject &b){b["board"]="Platine 1";}));},"a board named by something else than its identifier was accepted");
    rejects([&]{Document::fromJson(boardWith([](QJsonObject &b){b["side"]="left";}));},"an unknown side was accepted");
    rejects([&]{Document::fromJson(boardWith([](QJsonObject &b){b["rotation"]=720;}));},"a rotation out of range was accepted");
    rejects([&]{Document::fromJson(boardWith([](QJsonObject &b){b["offset"]=QJsonArray{1};}));},"an offset without y was accepted");
    rejects([&]{Document::fromJson(panelOf([](QJsonObject &o){auto list=o["boards"].toArray();list.append(list[0]);o["boards"]=list;}));},"the same board twice behind a panel was accepted");
    rejects([&]{Document::fromJson(panelOf([](QJsonObject &o){auto list=o["elements"].toArray();auto e=list[1].toObject();e["component"]="R1";list[1]=e;o["elements"]=list;}));},"a component named by something else than its identifier was accepted");
    {auto newer=root;newer["version"]=3;rejects([&]{Document::fromJson(newer);},"a newer version was accepted");}
    // A copy is a new component: the same new one for the elements that shared it.
    Element hole=newElement(ElementType::Drill);hole.component=pot;Element pin=hole;pin.id=newId();Element group=newElement(ElementType::Group);group.children={hole,pin};
    Element copy=group;renewIds(copy);
    require(copy.children[0].component.size()==32&&copy.children[0].component!=pot&&copy.children[1].component==copy.children[0].component,"a copy must be a new component, one for both elements");
    QHash<QString,QString> renamed;Element first=hole,second=hole;renewIds(first,&renamed);renewIds(second,&renamed);
    require(first.component==second.component&&first.component!=pot&&first.id!=second.id,"elements copied together must stay one component");
    // From the board to the panel: as it is, turned by 90° (the board's x axis points up), the solder side towards the
    // panel (mirrored left to right), both.
    require(near(BoardBehind{plan,board,{0,0},0,false}.toPanel().map(QPointF(10,5)),QPointF(10,5),1e-9),"a board as it is");
    require(near(BoardBehind{plan,board,{50,40},90,false}.toPanel().map(QPointF(10,0)),QPointF(50,30),1e-9),"turned counter-clockwise, the board's x axis points up");
    require(near(BoardBehind{plan,board,{50,40},0,true}.toPanel().map(QPointF(10,5)),QPointF(40,45),1e-9),"with its solder side towards the panel the board is mirrored");
    require(near(BoardBehind{plan,board,{50,40},90,true}.toPanel().map(QPointF(10,5)),QPointF(55,50),1e-9),"mirrored first, then turned");
    // The component interface: elements standing for components, the component of an element inside one.
    const auto found=panelComponents(back.panels[0]);
    require(found.size()==2&&found[0].component==pot&&found[0].at==QPointF(100,50)&&found[1].component==other&&found[1].at==QPointF(60,15),"the components of a panel and where they sit");
    require(componentOf(back.panels[0],back.panels[0].elements[0].children[1].id)==pot&&componentOf(back.panels[0],back.panels[0].elements[0].children[0].id).isEmpty(),"the component of an element");
    Panel nested;Element symbol=newElement(ElementType::Group);symbol.component=pot;symbol.hasAnchor=true;symbol.anchor={7,8};Element text=newElement(ElementType::Text);text.frame=rectFrame(QRectF(0,0,10,4));symbol.children={text};nested.elements={symbol};
    require(componentOf(nested,text.id)==pot&&componentPoint(symbol)==QPointF(7,8),"a part of a symbol belongs to the symbol's component");
}

static void historyTests(){
    Document d;History h;h.begin(d);require(!h.commit(d),"an unchanged document added an undo step");
    h.begin(d);d.panels[0].elements.append(newElement(ElementType::Drill));require(h.commit(d),"a change added no undo step");
    h.begin(d);d.panels.append(newPanel("Zwei",50,50));d.activePanel=1;require(h.commit(d),"adding a panel added no undo step");
    require(h.undo(d)&&d.panels.size()==1&&d.activePanel==0,"undo did not remove the panel");
    require(h.undo(d)&&d.panels[0].elements.isEmpty(),"undo did not remove the drill");
    require(h.redo(d)&&d.panels[0].elements.size()==1,"redo did not restore the drill");
    h.begin(d);d.title="Anders";h.commit(d);require(!h.redo(d),"a new change kept the redo steps");
}

int main(int argc,char **argv){
    QApplication app(argc,argv);setUiLanguage("de"); // the tests compare German texts
    try{
        const auto missing=untranslatedTexts();
        if(!missing.isEmpty())throw std::runtime_error(("texts without English translation: "+missing.join(" | ")).toStdString());
        modelTests();boardTests();geometryTests();renderTests();generatorTests();historyTests();runFormatTests();runEditorTests();runStrokeFontTests();runScaleTests();runEmfTests();runHpglTests();
        // The interface in English: the code above must not have asked for a text without translation.
        setUiLanguage("en");sampleDocument();runFormatTests();
        require(typeTitle(ElementType::Drill)=="Hole"&&missingTranslations().isEmpty(),"English texts are missing");
        setUiLanguage("de");
        QTextStream(stdout)<<"Front panel tests passed\n";return 0;
    }catch(const std::exception &e){QTextStream(stderr)<<e.what()<<"\n";return 1;}
}
