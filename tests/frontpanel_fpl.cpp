// Tests of the FrontDesigner format: files written here and hand-built byte streams, no files of the vendor.
// With OPENLOCH_FRONTDESIGNER_CORPUS set to a folder of local .FPL/.LIB files, these are read and written back too.
#include "fpl.h"
#include "delphistream.h"
#include "frontpanel.h"
#include "panelgeometry.h"
#include "panelgenerators.h"
#include "language.h"
#include "legacy_reader.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>
#include <QtEndian>
#include <cmath>
#include <functional>

using namespace openloch;
using namespace openloch::frontpanel;
using namespace openloch::frontdesigner;

namespace fptest {
void require(bool ok,const char *message);
void rejects(const std::function<void()> &fn,const char *message);
bool near(double a,double b,double tolerance);
bool near(QPointF a,QPointF b,double tolerance);
Document sampleDocument();
}
using namespace fptest;

namespace {
// Own stream builder, independent of the module's writer: typed values, raw blocks, 80-bit numbers.
struct Bytes {
    QByteArray b;
    void raw(const QByteArray &x){b+=x;}
    void byte(int v){b+=char(v);}
    void integer(int v){char t[4];if(v>=-128&&v<=127){byte(2);byte(qint8(v));}else if(v>=-32768&&v<=32767){byte(3);qToLittleEndian(qint16(v),t);b.append(t,2);}else{byte(4);qToLittleEndian(qint32(v),t);b.append(t,4);}}
    QByteArray ten(double v){QByteArray out(10,0);if(v!=0){int e=0;const double f=std::frexp(std::abs(v),&e);qToLittleEndian(quint64(std::ldexp(f,64)),out.data());qToLittleEndian(quint16((e-1+16383)|(v<0?0x8000:0)),out.data()+8);}return out;}
    void real(double v){byte(5);raw(ten(v));}
    void boolean(bool v){byte(v?9:8);}
    void text(const QByteArray &s){byte(6);byte(s.size());raw(s);}
    void colour(int r,int g,int bl){byte(r);byte(g);byte(bl);byte(0);}
    // The common values of every object in the layout of a version: stroke, fill and "filled", from 2.0 the machining
    // values, corner form, short name and insertion point, from 3.0 the name, from 3.02 the gradient, from 3.03 engraving.
    void base(double version,int kind,int width,bool filled){
        integer(kind);integer(width);colour(10,20,30);byte(0);colour(200,100,50);byte(0);boolean(filled);boolean(false);
        if(version>=2.0){boolean(false);boolean(false);integer(1);real(0);raw(QByteArray(41,0));real(0);real(0);}
        if(version>=3.0)text("Teil");
        if(version>=3.02){colour(255,255,255);integer(0);}
        if(version>=3.03)boolean(false);
    }
    void base(int kind,int width){base(3.04,kind,width,true);}
    void point(double x,double y){raw(ten(x)+ten(y));}
};
QByteArray versionThreeFour(){
    Bytes s;QByteArray header(41,0);header[0]=5;header.replace(1,5,"Alt 1");header.replace(37,4,"3,04");s.raw(header);
    s.integer(2);
    s.text("TLinie");s.base(0,25);s.integer(2);for(double v:{100.0,200.0,600.0,200.0,600.0,700.0})s.real(v);
    s.text("TBohrung");s.base(8,10);s.real(2500);s.real(1500);s.real(300);
    s.integer(0);s.real(5000);s.real(4000);s.real(50);s.integer(1);s.raw(s.ten(100)+s.ten(150));s.boolean(true);s.colour(230,230,230);
    s.colour(192,192,192);s.integer(0);for(int i=0;i<5;i++)s.integer(0);s.colour(255,255,255);s.integer(2);
    s.integer(0);s.integer(1);
    return s.b;
}
// A file in the layout of an older version, built from the format description: line, drill, text (style from 1.01),
// group, and in files before 2.0 the oldest circle and arc; the panel values of each version; a view (with the panel
// number from 2.0) and from 2.0 the number of panels. An empty version text stands for 1,00.
QByteArray olderVersion(const QByteArray &versionText,double v){
    Bytes s;QByteArray header(41,0);header[0]=3;header.replace(1,3,"Alt");header.replace(37,versionText.size(),versionText);s.raw(header);
    const bool oldest=v<2.0;
    s.integer(oldest?6:4);
    s.text("TLinie");s.base(v,0,25,true);s.integer(1);s.real(100);s.real(200);s.real(600);s.real(200);
    s.text("TBohrung");s.base(v,8,10,false);s.real(2500);s.real(1500);s.real(300);
    s.text("TTextLabel");s.base(v,5,0,true);s.point(1000,500);s.point(2000,500);s.point(2000,1000);s.point(1000,1000);
    s.text("Arial");s.integer(250);s.real(0);s.text("Alt");s.boolean(false);s.boolean(false);if(v>=1.01)s.byte(1);
    s.text("TGruppe");s.base(v,9,0,false);s.integer(0);s.raw(QByteArray(41,0));s.text("TBohrung");s.base(v,8,0,false);s.real(4000);s.real(1000);s.real(150);
    if(oldest){
        s.text("TKreis");s.base(v,1,5,false);s.point(3000,3000);s.point(3500,3000);
        // Stroke and fill only, then centre, radius point, start and end ray, and whether it is a pie.
        s.text("TBogen");s.integer(2);s.integer(5);s.colour(0,0,0);s.byte(0);s.colour(255,255,255);s.byte(1);s.boolean(false);s.boolean(false);
        s.point(1000,3000);s.point(1500,3000);s.point(1500,3000);s.point(1000,2500);s.boolean(true);
    }
    s.integer(0);s.real(5000);s.real(4000);if(v>=2.02)s.real(50);else s.integer(50);s.integer(1);s.point(100,150);s.boolean(true);s.colour(230,230,230);
    if(v>=3.01){s.colour(192,192,192);s.integer(0);for(int i=0;i<5;i++)s.integer(0);}
    if(v>=3.02){s.colour(255,255,255);s.integer(2);}
    s.integer(1);s.point(0,0);s.point(2500,2000);if(v>=2.0){char panel[4];qToLittleEndian<qint32>(0,panel);s.raw(QByteArray(panel,4));}s.text("Links oben");
    if(v>=2.0)s.integer(1);
    return s.b;
}
// A file of version 3,16 with one empty panel and its print settings: three by ten tiles 3,17 mm apart across, sheet
// 2, of the options only landscape and "only one sheet", 150 %, the first tile 1,24 mm left of and 8,46 mm below the
// corner of the printable area, texts left out. With `inch` the panel's unit is inch, and so are the origin fields.
QByteArray printedPanel(bool inch=false){
    Bytes s;QByteArray header(41,0);header[0]=5;header.replace(1,5,"Druck");header.replace(37,4,"3,16");s.raw(header);
    s.integer(0);
    s.integer(0);s.real(5000);s.real(2500);s.real(50);s.integer(1);s.point(0,0);s.boolean(true);s.colour(230,230,230);
    s.colour(192,192,192);s.integer(inch);for(int i=0;i<5;i++)s.integer(inch&&i>=3);
    s.colour(255,255,255);s.integer(0);
    s.integer(3);s.integer(10);s.integer(2);s.point(158.5,0);for(int i=0;i<13;i++)s.boolean(i==7||i==12);s.real(1.5);s.integer(62);s.integer(-423);
    s.text("Druck");s.boolean(false);
    s.integer(0);s.integer(1);
    return s.b;
}
// The arc's points against the original, sampled along both.
bool sameArc(const Element &a,const Element &b,double tolerance){
    for(int k=0;k<=8;k++){const QPointF p=ellipsePoint(a,a.startAngle+a.spanAngle*k/8);bool found=false;
        for(int j=0;j<=400&&!found;j++)found=near(p,ellipsePoint(b,b.startAngle+b.spanAngle*j/400),tolerance);if(!found)return false;}
    return true;
}
QJsonArray canonical(const Panel &p){QJsonArray a;for(const auto &e:p.elements)a.append(elementToJson(e,false));return a;}
// Equal up to the rounding of numbers that were fitted again (circles and arcs from their points).
bool same(const QJsonValue &a,const QJsonValue &b){
    if(a.isDouble()&&b.isDouble())return std::abs(a.toDouble()-b.toDouble())<=1e-6*std::max(1.0,std::abs(a.toDouble()));
    if(a.isArray()&&b.isArray()){const auto x=a.toArray(),y=b.toArray();if(x.size()!=y.size())return false;for(int i=0;i<x.size();i++)if(!same(x[i],y[i]))return false;return true;}
    if(a.isObject()&&b.isObject()){const auto x=a.toObject(),y=b.toObject();if(x.keys()!=y.keys())return false;for(const auto &k:x.keys())if(!same(x[k],y[k]))return false;return true;}
    return a==b;
}
bool samePanels(const Document &a,const Document &b){
    if(a.panels.size()!=b.panels.size())return false;
    for(int i=0;i<a.panels.size();i++){const auto &x=a.panels[i],&y=b.panels[i];
        if(!same(canonical(x),canonical(y))||x.name!=y.name||x.width!=y.width||x.height!=y.height||x.color!=y.color||x.gradient!=y.gradient||x.origin!=y.origin)return false;}
    return true;
}
void corpus(){
    const QString folder=qEnvironmentVariable("OPENLOCH_FRONTDESIGNER_CORPUS");if(folder.isEmpty())return;
    // With OPENLOCH_FRONTDESIGNER_WRITE set to a folder, every file is also written there rebuilt from the model, and a
    // sample with objects made here, for a check with other readers.
    const QString written=qEnvironmentVariable("OPENLOCH_FRONTDESIGNER_WRITE");
    if(!written.isEmpty()){
        Document sample=fptest::sampleDocument();Panel &p=sample.panels[0];
        p.elements<<dimension({10,50},{60,50},{35,56});ScaleParameters sp;p.elements<<scale(sp,QTransform::fromTranslate(80,30));
        CutoutParameters cp;cp.frameWidth=cp.frameHeight=48;p.elements<<cutout(cp,QTransform::fromTranslate(30,30));
        Element ring=newElement(ElementType::Group);ring.parameters["combine"]=true;Element outer=newElement(ElementType::Ellipse);outer.center={100,20};outer.radiusX=outer.radiusY=8;
        Element inner=outer;inner.id=newId();inner.radiusX=inner.radiusY=4;ring.children={outer,inner};ring.fill.style=FillStyle::Solid;p.elements<<ring;
        p.elements<<regularPolygon({20,20},6,8,false,90);
        QFile out(QDir(written).filePath("openloch-sample.FPL"));if(out.open(QIODevice::WriteOnly))out.write(writeFrontDesigner(sample));
    }
    int files=0,exact=0;
    QDirIterator it(folder,{"*.fpl","*.FPL","*.lib","*.LIB","*.Lib"},QDir::Files,QDirIterator::Subdirectories|QDirIterator::FollowSymlinks);
    while(it.hasNext()){
        const QString path=it.next();QFile f(path);if(!f.open(QIODevice::ReadOnly))continue;const QByteArray bytes=f.readAll();
        const bool library=QFileInfo(path).suffix().compare("lib",Qt::CaseInsensitive)==0;
        const Document d=readFrontDesigner(bytes,library);files++;
        const QByteArray again=library?writeFrontDesignerLibrary(d,0):writeFrontDesigner(d);
        if(bytes.mid(37,4)=="3,16"){if(again!=bytes)throw std::runtime_error(("not written back unchanged: "+path).toStdString());exact++;}
        if(!samePanels(readFrontDesigner(again,library),d))throw std::runtime_error(("writing changed the content: "+path).toStdString());
        WriteOptions fresh;fresh.keepUnchanged=false;const QByteArray fromModel=library?writeFrontDesignerLibrary(d,0,fresh):writeFrontDesigner(d,fresh);const Document rebuilt=readFrontDesigner(fromModel,library);
        if(!written.isEmpty()){QFile out(QDir(written).filePath(QFileInfo(path).fileName()));if(out.open(QIODevice::WriteOnly))out.write(fromModel);}
        for(int i=0;i<d.panels.size();i++)if(rebuilt.panels[i].elements.size()!=d.panels[i].elements.size())throw std::runtime_error(("rebuilt file lost objects: "+path).toStdString());
    }
    QTextStream(stdout)<<"FrontDesigner corpus: "<<files<<" files read and written, "<<exact<<" byte for byte\n";
}
}

void runFormatTests(){
    // Extended numbers: exact for doubles, negative zero and large values.
    for(double v:{0.0,1.0,-2.5,3.14159265358979,1e-300,123456789.125,-0.0}){const QByteArray t=toExtended(v);require(t.size()==10&&fromExtended(t.constData())==v&&std::signbit(fromExtended(t.constData()))==std::signbit(v),"extended conversion is not exact");}
    {DelphiWriter w;w.string("Abc");w.string("Größe");w.string(QString());w.string(QString::fromUtf8("日本"));w.integer(127);w.integer(128);w.integer(-40000);
     require(w.bytes.left(5)==QByteArray("\x06\x03""Abc",5)&&quint8(w.bytes[5])==20&&quint8(w.bytes[17])==18,"strings are not tagged like the original writes them");
     DelphiReader r(w.bytes);require(r.string()=="Abc"&&r.string()=="Größe"&&r.string().isEmpty()&&r.string()==QString::fromUtf8("日本")&&r.integer()==127&&r.integer()==128&&r.integer()==-40000&&r.atEnd(),"stream values do not read back");
     require(fromWindows1252(QByteArray("\x80\xe4",2))==QString::fromUtf8("€ä")&&toWindows1252(QString::fromUtf8("€ä"))==QByteArray("\x80\xe4",2),"Windows code page conversion is wrong");}

    // A file of version 3,04, built byte by byte: units, version branches, colours, drill and line.
    const QByteArray old=versionThreeFour();QStringList notes;const Document o=readFrontDesigner(old,false,&notes);
    require(o.panels.size()==1&&o.panels[0].name=="Alt 1"&&near(o.panels[0].width,100,1e-12)&&near(o.panels[0].height,80,1e-12)&&near(o.panels[0].grid,1,1e-12)&&o.panels[0].origin==QPointF(2,3),"panel values of an old file");
    require(o.panels[0].gridVisible&&o.panels[0].color==QColor(230,230,230)&&o.panels[0].gradient==Gradient::Vertical,"panel colours of an old file");
    const auto &line=o.panels[0].elements[0],&drill=o.panels[0].elements[1];
    require(line.type==ElementType::Line&&line.points==QPolygonF({{2,4},{12,4},{12,14}})&&near(line.pen.width,0.5,1e-12)&&line.pen.color==QColor(10,20,30)&&line.name=="Teil","line of an old file");
    require(line.fill.style==FillStyle::Solid&&line.fill.color==QColor(200,100,50)&&line.machining==Machining::None,"fill of an old file");
    require(drill.type==ElementType::Drill&&drill.center==QPointF(50,30)&&near(drill.diameter,6,1e-12),"drill of an old file");
    // Files of older versions: each version branch of objects, panel, views and panel count.
    for(const auto &[text,version]:{std::pair<QByteArray,double>{"",1.0},{"1,00",1.0},{"1,01",1.01},{"2,00",2.0},{"2,02",2.02},{"3,00",3.0},{"3,01",3.01},{"3,02",3.02},{"3,03",3.03}}){
        const Document a=readFrontDesigner(olderVersion(text,version));const Panel &p=a.panels[0];const auto &e=p.elements;
        require(a.panels.size()==1&&p.name=="Alt"&&near(p.width,100,1e-12)&&near(p.height,80,1e-12)&&near(p.grid,1,1e-12)&&p.origin==QPointF(2,3)&&p.color==QColor(230,230,230)
            &&p.gradient==(version>=3.02?Gradient::Vertical:Gradient::None),"panel of an older version");
        require(e.size()==(version<2.0?6:4)&&e[0].type==ElementType::Line&&e[0].points==QPolygonF{QPointF(2,4),QPointF(12,4)}&&e[0].fill.style==FillStyle::Solid
            &&e[0].name==(version>=3.0?"Teil":""),"line of an older version");
        require(e[1].type==ElementType::Drill&&e[1].center==QPointF(50,30)&&near(e[1].diameter,6,1e-12)&&e[1].fill.style==FillStyle::None,"drill of an older version, not filled");
        require(e[2].type==ElementType::Text&&e[2].text=="Alt"&&e[2].font=="Arial"&&e[2].bold==(version>=1.01)&&near(e[2].frame[0],{20,10},1e-12)&&near(e[2].frame[2],{20,20},1e-12),"text of an older version");
        require(e[3].type==ElementType::Group&&e[3].children.size()==1&&e[3].children[0].center==QPointF(80,20),"group of an older version");
        if(version<2.0){
            require(e[4].type==ElementType::Ellipse&&e[4].center==QPointF(60,60)&&near(e[4].radiusX,10,1e-12),"circle of the oldest layout");
            require(e[5].type==ElementType::Arc&&e[5].center==QPointF(20,60)&&near(e[5].radiusX,10,1e-12)&&near(e[5].startAngle,90,1e-9)&&near(e[5].spanAngle,270,1e-9)
                &&e[5].arcStyle==ArcStyle::Pie,"arc of the oldest layout");
        }
        require(a.views.size()==1&&a.views[0].name=="Links oben"&&a.views[0].panel==0&&a.views[0].area==QRectF(0,0,50,40),"view of an older version");
        require(samePanels(readFrontDesigner(writeFrontDesigner(a)),a),"an older file written as 3,16");
    }
    // Print settings of a panel: read into the model, written back unchanged, a change written anew.
    {
        const QByteArray file=printedPanel();const Document a=readFrontDesigner(file);const PrintSettings &s=a.panels[0].print;
        require(s.tilesX==3&&s.tilesY==10&&s.sheet==2&&near(s.gapX,3.17,1e-12)&&s.gapY==0,"tiles of the print settings");
        require(s.landscape&&s.onlyOne&&!s.machining&&!s.objects&&!s.data&&!s.rulers&&!s.frame&&!s.background&&!s.mirror&&!s.original&&!s.centred&&!s.dimensions
            &&!s.cutMarks&&!s.texts,"options of the print settings");
        require(near(s.zoom,1.5,1e-12)&&near(s.left,-1.24,1e-12)&&near(s.top,8.46,1e-12),"scale and place of the print settings");
        require(writeFrontDesigner(a)==file,"print settings not written back unchanged");
        require(writeFrontDesigner(Document::decode(a.encode()))==file,"print settings lost in the native format");
        Document changed=a;PrintSettings &c=changed.panels[0].print;c.tilesY=8;c.texts=true;c.left=4;c.mirror=true;
        const PrintSettings back=readFrontDesigner(writeFrontDesigner(changed)).panels[0].print;
        require(back==c,"changed print settings not written");
        // Before 3,05 a panel has the defaults; before 3,16 its texts are printed.
        require(readFrontDesigner(olderVersion("3,03",3.03)).panels[0].print==PrintSettings(),"print settings of an older file");
        QByteArray noTexts=file;noTexts.replace(37,4,"3,15");noTexts.remove(noTexts.size()-5,1);
        require(readFrontDesigner(noTexts).panels[0].print.texts,"texts printed in a file before 3,16");
        // The unit of the panel: read, kept, and switched together with the origin fields.
        const Document b=readFrontDesigner(printedPanel(true));require(b.panels[0].inch&&!a.panels[0].inch,"unit of a panel");
        require(writeFrontDesigner(b)==printedPanel(true)&&Document::decode(b.encode()).panels[0].inch,"unit of a panel kept");
        Document turned=a;turned.panels[0].inch=true;require(writeFrontDesigner(turned)==printedPanel(true),"unit switched as in the original");
    }
    // A mirrored text gets the original's mirror flag; its corners keep the mirroring.
    {
        Document m;m.panels[0]=newPanel("Spiegel",60,40);Element t=newElement(ElementType::Text);t.text="AB";t.frame=rectFrame(QRectF(10,10,20,8));
        Element u=t;transformElement(u,QTransform::fromScale(-1,1)*QTransform::fromTranslate(60,0));m.panels[0].elements={t,u};
        const Document back=readFrontDesigner(writeFrontDesigner(m));const auto &e=back.panels[0].elements;
        auto flag=[](const Element &x,const char *k){return x.foreign["frontDesigner"].toObject()[k].toBool();};
        require(e.size()==2&&!flag(e[0],"flagA")&&!flag(e[0],"flagB")&&flag(e[1],"flagA")&&!flag(e[1],"flagB"),"mirror flags of texts");
        require(near(e[1].frame[0],u.frame[0],1e-9)&&near(e[1].frame[1],u.frame[1],1e-9)&&near(e[1].frame[2],u.frame[2],1e-9),"frame of a mirrored text");
    }
    // Every shorter file is refused (the last panel count is optional, as in the original reader).
    for(int n=0;n<old.size()-2;n++)rejects([&]{readFrontDesigner(old.left(n));},"a truncated file was accepted");
    rejects([&]{QByteArray b=old;b.replace(37,4,"9,99");readFrontDesigner(b);},"a file of a newer version was accepted");
    rejects([&]{QByteArray b=old;b.replace(b.indexOf("TLinie"),6,"TXXXXX");readFrontDesigner(b);},"an unknown object class was accepted");

    // The sample document written as FPL and read again: every element type, units and colours.
    Document d=sampleDocument();
    Element ellipse=newElement(ElementType::Ellipse);ellipse.center={40,30};ellipse.radiusX=12;ellipse.radiusY=5;ellipse.rotation=20;
    Element arc=newElement(ElementType::Arc);arc.center={20,20};arc.radiusX=arc.radiusY=10;arc.startAngle=30;arc.spanAngle=100;arc.arcStyle=ArcStyle::Pie;
    Element mirrored=arc;transformElement(mirrored,QTransform::fromScale(-1,1)*QTransform::fromTranslate(80,0));mirrored.arcStyle=ArcStyle::Open;
    Element stretched=arc;transformElement(stretched,QTransform(2,0.3,0,1,0,0));stretched.arcStyle=ArcStyle::Chord;
    d.panels[1].elements={ellipse,arc,mirrored,stretched};
    const QByteArray bytes=writeFrontDesigner(d);require(bytes.mid(37,4)=="3,16","the file does not carry version 3,16");
    const Document back=readFrontDesigner(bytes);
    require(back.panels.size()==2&&back.panels[0].elements.size()==6&&back.panels[1].elements.size()==4,"panels or objects lost in FPL");
    require(back.panels[0].name=="Vorderseite"&&near(back.panels[0].width,120,1e-9)&&back.panels[0].gradient==Gradient::Vertical&&back.panels[0].color==QColor("#304050"),"panel values lost in FPL");
    const auto &group=back.panels[0].elements[0];
    require(group.type==ElementType::Group&&group.children.size()==2&&group.children[0].contour.corners==Corners::Round&&near(group.children[0].contour.size,2.5,1e-9)&&group.children[0].pen.style==PenStyle::Dash,"group or contour lost in FPL");
    require(back.panels[0].elements[1].type==ElementType::Rectangle&&back.panels[0].elements[1].machining==Machining::Mill&&back.panels[0].elements[1].fill.gradient==Gradient::Diagonal,"rectangle lost in FPL");
    const auto &text=back.panels[0].elements[4];require(text.text=="Lautstärke"&&text.bold&&text.strokeFont=="Einlinig"&&near(text.frame[1],d.panels[0].elements[4].frame[1],1e-9),"text lost in FPL");
    const auto &image=back.panels[0].elements[5];require(image.type==ElementType::Image&&image.transparent&&image.transparentColor==QColor("#123456")&&!QImage::fromData(back.resources.value(image.resource).data).isNull(),"image lost in FPL");
    const auto &e2=back.panels[1].elements[0];require(e2.type==ElementType::Ellipse&&near(e2.center,{40,30},1e-6)&&near(e2.radiusX,12,1e-6)&&near(e2.radiusY,5,1e-6)&&near(std::fmod(e2.rotation+360,180),20,1e-6),"ellipse not recovered from its points");
    for(int i=1;i<4;i++){const auto &a=back.panels[1].elements[i];require(a.type==ElementType::Arc&&a.arcStyle==d.panels[1].elements[i].arcStyle&&sameArc(d.panels[1].elements[i],a,1e-6)&&sameArc(a,d.panels[1].elements[i],1e-6),"arc not recovered from its points");}
    // Arcs carry the original's arc spline (corner form 3), which begins and ends in the middle of the outer edges.
    for(int i=1;i<4;i++)require(back.panels[1].elements[i].foreign["frontDesigner"].toObject()["cornerStyle"].toInt()==3,"arc written with the arc spline");
    // Read and written again, unchanged objects keep their bytes: the second file equals the first.
    require(writeFrontDesigner(back)==bytes,"writing a read file changed it");
    // Also after the detour through the native format.
    require(writeFrontDesigner(Document::decode(back.encode()))==bytes,"the native format lost values of the FPL file");
    // A changed object is written anew; the others stay as they were.
    Document changed=back;changed.panels[0].elements[1].points[0]+=QPointF(1,0);const Document reread=readFrontDesigner(writeFrontDesigner(changed));
    require(near(reread.panels[0].elements[1].points[0],changed.panels[0].elements[1].points[0],1e-9)&&reread.panels[0].elements[1].type==ElementType::Rectangle,"a changed rectangle was not written anew");
    // Without keeping bytes the file is built completely from the model and reads back the same.
    WriteOptions fresh;fresh.keepUnchanged=false;const Document rebuilt=readFrontDesigner(writeFrontDesigner(back,fresh));
    require(rebuilt.panels[0].elements.size()==6&&near(rebuilt.panels[0].elements[0].children[0].points[2],back.panels[0].elements[0].children[0].points[2],1e-9),"a file built anew differs");
    // One panel as a library page.
    const Document library=readFrontDesigner(writeFrontDesignerLibrary(back,1),true);require(library.panels.size()==1&&library.panels[0].elements.size()==4,"library page differs");
    corpus();
}
