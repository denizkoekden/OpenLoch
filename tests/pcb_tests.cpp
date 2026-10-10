#include "language.h"
#include "legacy_reader.h"
#include "modules/pcb/model.h"
#include "modules/pcb/copper.h"
#include "modules/pcb/font.h"
#include "modules/pcb/shapes.h"
#include "modules/pcb/printing.h"
#include "modules/pcb/footprints.h"
#include "modules/pcb/editor.h"
#include "formats/sprint/sprint.h"
#include "formats/sprint/textio.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>

using namespace openloch;
using namespace openloch::pcb;
int editorTests();
int editingTests(const QString &preferencesFile);
int preferencesTests(const QString &preferencesFile);
int picturesTests();
int macroTests();
int fabricationTests();
int schematicTests();

static void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
static void rejects(const std::function<void()> &fn,const char *message){bool rejected=false;try{fn();}catch(const FormatError &){rejected=true;}require(rejected,message);}
static bool near(double a,double b,double eps=1e-6){return std::abs(a-b)<eps;}
static bool near(QPointF a,QPointF b,double eps=1e-6){return near(a.x(),b.x(),eps)&&near(a.y(),b.y(),eps);}
static bool near(const QRectF &a,const QRectF &b,double eps=1e-6){return near(a.topLeft(),b.topLeft(),eps)&&near(a.bottomRight(),b.bottomRight(),eps);}

// Sprint-Layout 6 bytes written straight from the record layout of docs/modules/pcb.md, independent of the writer.
namespace {
struct Bytes {
    QByteArray b;
    void u8(int v){b.append(char(v));}
    void u32(quint32 v){char c[4];qToLittleEndian(v,c);b.append(c,4);}
    void f32(float v){quint32 u;std::memcpy(&u,&v,4);u32(u);}
    void f64(double v){quint64 u;std::memcpy(&u,&v,8);char c[8];qToLittleEndian(u,c);b.append(c,8);}
    void str(const QByteArray &s){u32(quint32(s.size()));b.append(s);}
    void shortStr(const QByteArray &s,int capacity){u8(int(s.size()));b.append(s);b.append(QByteArray(capacity-s.size(),'\x5a'));}  // garbage after the text
};
// A record: type, x, y, outer, inner, the 32-bit field at 0x11, layer, form; `fields` sets further bytes by offset.
struct Rec {
    int type;float x=0,y=0,outer=0,inner=0;quint32 width=0;int layer=3,form=0;QList<std::pair<int,QByteArray>> fields;
    QByteArray text;QList<quint32> groups;QList<QPointF> points;QList<QList<QPointF>> strokes;quint32 strokeWidth=0,strokeClearance=4000;
    QByteArray component;QByteArray package;
};
QByteArray le32(quint32 v){char c[4];qToLittleEndian(v,c);return QByteArray(c,4);}
QByteArray le16(quint16 v){char c[2];qToLittleEndian(v,c);return QByteArray(c,2);}
QByteArray f32le(float v){quint32 u;std::memcpy(&u,&v,4);return le32(u);}
void record(Bytes &o,const Rec &r){
    Bytes f;f.f32(r.x);f.f32(r.y);f.f32(r.outer);f.f32(r.inner);f.u32(r.width);f.u8(0);f.u8(r.layer);f.u8(r.form);
    f.b.append(QByteArray(0x4c-f.b.size(),'\0'));for(const auto &[at,v]:r.fields)f.b.replace(at-1,v.size(),v);
    o.u8(r.type);o.b.append(f.b);o.str(r.text);o.str({});o.u32(quint32(r.groups.size()));for(auto g:r.groups)o.u32(g);
    if(r.type==7){
        o.u32(quint32(r.strokes.size()));
        for(const auto &s:r.strokes){Bytes sub;sub.b=QByteArray(0x4c,'\0');sub.b.replace(0x10,4,le32(r.strokeWidth));sub.b[0x15]=char(r.layer);sub.b.replace(0x28,4,le32(r.strokeClearance));
            o.u8(6);o.b.append(sub.b);o.u32(quint32(s.size()));for(auto p:s){o.f32(float(p.x()));o.f32(float(p.y()));}}
        if(!r.component.isEmpty()){o.b.append(r.component);o.str(r.package);o.str("");o.u8(1);}
    }else if(r.type!=5){o.u32(quint32(r.points.size()));for(auto p:r.points){o.f32(float(p.x()));o.f32(float(p.y()));}}
}
// Lengths given in tenths of a micrometre (version 6), as version 5 stores them: in hundredths of a millimetre.
struct Units {
    bool five;
    float f(float v) const{return five?v/100:v;}
    quint32 i(qint32 v) const{return quint32(five?v/100:v);}
    QList<QPointF> points(QList<QPointF> list) const{if(five)for(auto &p:list)p/=100;return list;}
};
// One board of 80 × 50 mm with one element of every kind, an airwire between the two pads and a component: the pads and
// the designator carry component number 5. Version 5 (Sprint-Layout 5) has the same layout with lengths in hundredths
// of a millimetre and no component data after the designator; the arc angles, the text's thickness, style and angle
// and the hatch pitch are stored as in version 6.
QByteArray fixtureLayout(int version=6){
    const Units u{version==5};
    Bytes o;o.b=QByteArray::fromHex(version==5?"0533aaff":"0633aaff");o.u32(1);
    Bytes h;h.shortStr("Test",30);h.u32(0x01020304);h.u32(u.i(800000));h.u32(u.i(500000));h.b.append(QByteArray::fromHex("00000100000000"));
    h.f64(version==5?254:25400);h.f64(version==5?.04:.0004);h.u32(0);h.u32(0);h.u8(1);h.b.append(QByteArray(3,'\0'));h.b.append(QByteArray::fromHex("01010101000001"));
    h.b.append(QByteArray(0x212-h.b.size(),'\0'));o.b.append(h.b);
    QList<Rec> recs;
    // Square pad at (10, 10) mm, outer diameter 1.6 mm, drill 0.8 mm, through-plated, 0.3 mm from the ground plane, in group 7;
    // a thermal pad with spokes per layer: one at twelve o'clock on K1, four on K2.
    Rec pad{2,u.f(100000),u.f(-100000),u.f(8000),u.f(4000),0,3,3};pad.fields={{0x1c,le16(5)},{0x1f,le32(0x5501)},{0x28,QByteArray("\x01",1)},{0x29,le32(u.i(3000))},{0x32,QByteArray("\x01",1)},{0x35,le32(100)},{0x39,QByteArray("\x01\x01",2)},{0x3e,le32(123456789)}};pad.groups={7};
    pad.points=u.points({{92000,-92000},{108000,-92000},{108000,-108000},{92000,-108000}});recs<<pad;
    // SMD pad at (20, 10) mm, 2 × 1 mm, turned by 90°.
    Rec smd{8,u.f(200000),u.f(-100000),u.f(20000),u.f(10000),0,1,0};smd.fields={{0x1c,le16(5)},{0x1f,le32(0x55)},{0x35,le32(100)},{0x3a,QByteArray("\x01",1)},{0x3e,le32(123456789)}};smd.points=u.points({{195000,-90000},{195000,-110000},{205000,-110000},{205000,-90000}});recs<<smd;
    // Track from the pad to (30, 20) mm, 0.8 mm wide, named "GND", with a square end.
    Rec track{6,0,0,0,0,u.i(8000),3,2};track.text="GND";track.points=u.points({{100000,-100000},{300000,-200000}});recs<<track;
    // Arc on B1 around (40, 25) mm: outer radius 5.5, inner 4.5 mm, from 90° to 180°; a value below 1000 is whole degrees.
    Rec arc{5,u.f(400000),u.f(-250000),u.f(55000),u.f(45000),180,2,0};arc.fields={{0x1f,le32(90000)}};recs<<arc;
    // Area with three corners and a 0.4 mm outline, hatched with a set pitch of 1 mm.
    Rec area{4,0,0,0,0,u.i(4000),3,0};area.fields={{0x32,QByteArray("\x01",1)},{0x35,le32(10000)}};area.points=u.points({{500000,-100000},{600000,-100000},{550000,-200000}});recs<<area;
    // Designator "R1" at (10, 40) mm, 2 mm high, wide style, thick strokes, turned (clockwise 270° = counter-clockwise 90°).
    // Its component data: pick and place offset 1 mm to the right and 2.5 mm down, centre 3 (counts as both), rotation 45°.
    Rec text{7,u.f(100000),u.f(-400000),u.f(20000),2,2,2,1};text.text="R1";text.fields={{0x1c,le16(5)},{0x32,QByteArray("\x01",1)},{0x35,le32(270)}};
    text.strokes={u.points({{100000,-400000},{100000,-380000}}),u.points({{110000,-400000},{110000,-380000}})};text.strokeWidth=u.i(3200);text.strokeClearance=u.i(4000);
    if(version>=6){Bytes c;c.f32(10000);c.f32(25000);c.u8(3);c.f64(45);text.component=c.b;}text.package="0805";recs<<text;
    o.u32(quint32(recs.size()));for(const auto &r:recs)record(o,r);
    o.u32(1);o.u32(1);   // pad 0 → element 1
    o.u32(1);o.u32(0);   // pad 1 → element 0
    o.u32(0);o.shortStr("Titel",100);o.shortStr("Autor",100);o.shortStr("Firma",100);o.str("Kommentar \xe4\x80");
    return o.b;
}
// The same in the version 4 layout (Sprint-Layout 4.0): integers in 1/100 mm, a shorter fixed part, one string, a
// through-plated pad as two pads naming each other.
void record4(Bytes &o,int type,qint32 x,qint32 y,qint32 outer,qint32 inner,qint32 width,int layer,int form,const QList<std::pair<int,QByteArray>> &fields={}){
    Bytes f;f.u32(quint32(x));f.u32(quint32(y));f.u32(quint32(outer));f.u32(quint32(inner));f.u32(quint32(width));f.u8(0);f.u8(layer);f.u8(form);
    f.b.append(QByteArray(0x4a-f.b.size(),'\0'));for(const auto &[at,v]:fields)f.b.replace(at-1,v.size(),v);
    o.u8(type);o.b.append(f.b);
}
QByteArray fixtureLayout4(){
    Bytes o;o.b=QByteArray::fromHex("0433aaff");o.u32(1);
    Bytes h;h.shortStr("Alt",30);h.u32(0);h.u32(8000);h.u32(5000);h.b.append(QByteArray::fromHex("0001"));h.f64(254);h.f64(.05);h.u32(0);h.u32(0);
    h.u8(3);h.b.append(QByteArray(3,'\0'));h.b.append(QByteArray::fromHex("0101010000"));h.b.append(QByteArray(0x209-h.b.size(),'\0'));o.b.append(h.b);
    o.u32(6);
    const QByteArray clearance=le32(40);
    record4(o,2,1000,-1000,80,40,0,3,1,{{0x1c,le16(0x1234)},{0x29,clearance}});o.str("");o.u32(0);o.u32(1);    // pad on K2 (0x1c is no component number), twin element 1
    record4(o,2,1000,-1000,80,40,0,1,1,{{0x29,clearance}});o.str("");o.u32(0);o.u32(0);    // the twin on K1
    record4(o,8,2000,-1000,200,100,0,1,0,{{0x29,clearance}});o.str("");o.u32(0);             // SMD pad 2 x 1 mm
    record4(o,6,0,0,0,0,50,3,0,{{0x29,clearance}});o.str("GND");o.u32(1);o.u32(4);o.u32(2);o.u32(1000);o.u32(quint32(-1000));o.u32(3000);o.u32(quint32(-2000));
    record4(o,5,4000,-2500,550,450,180,2,0,{{0x1f,le32(90)}});o.str("");o.u32(0);             // arc 90° to 180°, whole degrees
    record4(o,7,1000,-4000,200,2,2,2,0,{{0x1f,QByteArray("\x01",1)},{0x36,QByteArray("\x01",1)}});o.str("R1");o.u32(0);
    o.u32(2);for(int k=0;k<2;k++){record4(o,6,0,0,0,0,32,2,0,{{0x29,clearance}});o.u32(2);o.u32(quint32(1000+100*k));o.u32(quint32(-4000));o.u32(quint32(1000+100*k));o.u32(quint32(-3800));}
    o.u32(1);o.u32(2);o.u32(0);o.u32(1);o.u32(0);    // airwires: pad 0 to the SMD pad (element 2) and back
    o.u32(0);o.shortStr("Alt",100);o.shortStr("",100);o.shortStr("",100);o.str("");
    return o.b;
}
// The project data at the end of a layout: the board shown, title, empty author and company (zeros after the text, as
// the writer pads them) and an empty comment.
void footer(Bytes &o,quint32 shown){o.u32(shown);for(const QByteArray &s:{QByteArray("Titel"),QByteArray(),QByteArray()}){o.u8(int(s.size()));o.b.append(s);o.b.append(QByteArray(100-s.size(),'\0'));}o.str("");}
// Boards of 50 × 30 mm with two round pads on K2 at (10, 20) and (40, 20) mm and a track between them (before the pads
// with `trackFirst`), the track's autoroute mark `mark` and the indexes `block` after the airwires of every board; the
// last board is shown. Version 5 stores the same in hundredths of a millimetre.
QByteArray routedLayout(int mark,const QList<qint32> &block,bool trackFirst=false,int boards=1,int version=6){
    const Units u{version==5};
    Bytes o;o.b=QByteArray::fromHex(version==5?"0533aaff":"0633aaff");o.u32(quint32(boards));
    for(int k=0;k<boards;k++){
        Bytes h;h.shortStr(k?"Zweite":"Erste",30);h.u32(0);h.u32(u.i(500000));h.u32(u.i(300000));h.b.append(QByteArray(7,'\0'));h.f64(version==5?127:12700);h.f64(version==5?.04:.0004);h.u32(0);h.u32(0);
        h.u8(3);h.b.append(QByteArray(3,'\0'));h.b.append(QByteArray::fromHex("01010101000001"));h.b.append(QByteArray(0x212-h.b.size(),'\0'));o.b.append(h.b);
        QList<Rec> recs;
        for(float x:{100000.f,400000.f}){Rec pad{2,u.f(x),u.f(-200000),u.f(9000),u.f(4000),0,3,1};pad.fields={{0x1f,le32(0x55)},{0x35,le32(100)},{0x3a,QByteArray("\x01",1)},{0x3e,le32(123456789)}};
            pad.points=u.points({{x-9000,-200000},{x+9000,-200000}});recs<<pad;}
        Rec track{6,0,0,0,0,u.i(8000),3,0};track.fields={{0x31,QByteArray(1,char(mark))}};track.points=u.points({{100000,-200000},{100000,-80000},{400000,-80000},{400000,-200000}});
        if(trackFirst)recs.prepend(track);else recs<<track;
        o.u32(quint32(recs.size()));for(const auto &r:recs)record(o,r);
        o.u32(0);o.u32(0);for(auto i:block)o.u32(quint32(i));
    }
    footer(o,quint32(boards-1));return o.b;
}
// Version 4: a via (the pad on K2, then its twin on K1), a pad at (40, 20) mm and a track with the autoroute mark
// (position 0x35) between them, then the indexes `block`.
QByteArray routedLayout4(const QList<qint32> &block){
    Bytes o;o.b=QByteArray::fromHex("0433aaff");o.u32(1);
    Bytes h;h.shortStr("Vier",30);h.u32(0);h.u32(5000);h.u32(3000);h.b.append(QByteArray::fromHex("0001"));h.f64(127);h.f64(.05);h.u32(0);h.u32(0);
    h.u8(3);h.b.append(QByteArray(3,'\0'));h.b.append(QByteArray::fromHex("0101010000"));h.b.append(QByteArray(0x209-h.b.size(),'\0'));o.b.append(h.b);
    o.u32(4);
    record4(o,2,1000,-2000,90,40,0,3,1);o.str("");o.u32(0);o.u32(1);
    record4(o,2,1000,-2000,90,40,0,1,1);o.str("");o.u32(0);o.u32(0);
    record4(o,2,4000,-2000,90,40,0,3,1);o.str("");o.u32(0);o.u32(quint32(-1));
    record4(o,6,0,0,0,0,80,3,0,{{0x35,QByteArray("\x01",1)}});o.str("");o.u32(0);o.u32(2);o.u32(1000);o.u32(quint32(-2000));o.u32(4000);o.u32(quint32(-2000));
    o.u32(0);o.u32(0);o.u32(0);for(auto i:block)o.u32(quint32(i));
    footer(o,0);return o.b;
}
// Files older than Sprint-Layout 4.0: records of version 3 (74 bytes with the type byte, 32-bit numbers) or of versions 0
// to 2 (49 bytes, 16-bit numbers), lengths in 1/100 mm; `c` is the width, a circle's stop angle or a text's font size.
struct OldRec {
    int type,layer;qint32 x=0,y=0,a=0,b=0,c=0,d=0;int shape=1;QByteArray text;QList<qint32> groups;QList<QPoint> points;
    qint32 partner=-1;QList<quint32> wires;qint32 clearance=40;bool filled=false;int key=0;
};
// The element count, the records and, in version 3, the airwires of every pad.
QByteArray oldRecords(const QList<OldRec> &recs,int version){
    Bytes o;const bool wide=version==3;auto u16=[&](int v){char c[2];qToLittleEndian(quint16(v),c);o.b.append(c,2);};
    if(wide)o.u32(quint32(recs.size()));else u16(int(recs.size()));
    for(const auto &r:recs){
        QByteArray f(wide?73:48,'\0');auto put=[&](int at,qint32 v){if(wide)qToLittleEndian(v,f.data()+at-1);else qToLittleEndian(qint16(v),f.data()+at-1);};
        put(1,r.x);put(wide?5:3,r.y);put(wide?9:5,r.a);put(wide?13:7,r.b);put(wide?17:9,r.c);put(wide?47:37,r.d);
        f[wide?21:11]=char(r.layer);f[wide?22:12]=char(r.shape);const int text=wide?23:13;f[text]=char(r.text.size());f.replace(text+1,r.text.size(),r.text);
        if(wide){f[55]=char(r.filled);put(57,r.clearance);}else put(34,r.key);
        o.u8(r.type);o.b.append(f);
        if(wide){o.u32(quint32(r.groups.size()));for(auto g:r.groups)o.u32(quint32(g));}
        else if(version==2){u16(int(r.groups.size()));for(auto g:r.groups)u16(g);}
        if(r.type==4||r.type==6){if(wide)o.u32(quint32(r.points.size()));else u16(int(r.points.size()));for(auto p:r.points)for(int v:{p.x(),p.y()}){if(wide)o.u32(quint32(v));else u16(v);}}
        if(wide&&r.type==2)o.u32(quint32(r.partner));
    }
    if(wide)for(const auto &r:recs)if(r.type==2){o.u32(quint32(r.wires.size()));for(auto w:r.wires)o.u32(w);}
    return o.b;
}
// A version 3 board header: name, size, the two ground plane switches, grid (1/100 mm) and active layer.
QByteArray oldBoard(const QByteArray &name,qint32 width,qint32 height,int ground1,int ground3,double grid,qint32 active){
    Bytes h;h.shortStr(name,30);h.u32(0);h.u32(quint32(width));h.u32(quint32(height));h.u8(ground1);h.u8(ground3);h.f64(grid);h.f64(.1);h.u32(0);h.u32(0);h.u32(quint32(active));
    h.b.append(QByteArray(513-h.b.size(),'\0'));return h.b;
}
Document sampleDocument(){
    Document d;d.title="Probe";d.author="OpenLoch";d.comment="zwei Platinen";
    Board a=newBoard("Erste",80,50);a.grid=1.27;a.origin={5,45};a.templates[1]={QStringLiteral("C:\\Scans\\Unterseite.bmp"),300,{},true};a.groundPlane[CopperBottom]=true;a.visible[SilkBottom]=false;
    auto pad=newElement(ElementType::Pad);pad.pos={10,10};pad.shape=PadShape::OctagonWide;pad.rotation=30;pad.size=2;pad.size2=1;pad.via=true;updateOutline(pad);pad.groups={1};pad.part=1;pad.connections={1};a.elements<<pad;
    auto smd=newElement(ElementType::SmdPad);smd.pos={20,12.5};smd.size=1.5;smd.size2=.75;smd.rotation=45;updateOutline(smd);smd.connections={0};a.elements<<smd;
    auto track=newElement(ElementType::Track);track.points={{10,10},{15,10},{20,12.5}};track.width=.5;track.name="Netz 1";track.flatStart=true;a.elements<<track;
    auto circle=newElement(ElementType::Circle);circle.pos={30,30};circle.size=3;circle.width=.25;circle.start=45;circle.stop=315.5;a.elements<<circle;
    auto disc=newElement(ElementType::Circle);disc.pos={40,30};disc.size=1;disc.width=.5;disc.filled=true;disc.layer=CopperTop;a.elements<<disc;
    auto area=newElement(ElementType::Area);area.points={{50,5},{70,5},{70,20},{50,20}};area.width=.3;area.layer=CopperTop;area.hatched=true;area.hatchAuto=false;area.hatchPitch=1.25;a.elements<<area;
    auto text=newElement(ElementType::Text);text.pos={5,45};text.text="Grüße €";text.size=1.5;text.style=0;text.thickness=2;text.rotation=90;text.mirrored=true;
    text.role=TextRole::Designator;text.package="DIL8";text.comment="Opamp";text.componentRotation=180;text.pickAndPlace=true;text.groups={1};text.part=1;
    text.pickCentre=2;text.pickOffset={1.5,-.25};
    text.strokes={QPolygonF(QList<QPointF>{{5,45},{5,43.5}}),QPolygonF(QList<QPointF>{{6,45},{6.5,43.5},{7,45}})};text.strokeWidth=.2;text.visible=false;a.elements<<text;
    Board b=newBoard("Zweite",30,20);b.activeLayer=CopperTop;
    auto t2=newElement(ElementType::Track);t2.points={{1,1},{29,19}};t2.layer=Outline;t2.width=0;b.elements<<t2;
    d.boards={a,b};d.activeBoard=1;return d;
}
}

// Started by the plugin test with OPENLOCH_PCB_TEST_PLUGIN set to an exit code, the program answers as a plugin: it
// writes its parameters to the file OPENLOCH_PCB_TEST_PLUGIN_LOG names and, as its output, the elements it got moved
// 5 mm to the right and one more track.
static int testPlugin(int argc,char **argv){
    const int code=qEnvironmentVariableIntValue("OPENLOCH_PCB_TEST_PLUGIN");
    QStringList arguments;for(int i=1;i<argc;i++)arguments.append(QString::fromLocal8Bit(argv[i]));
    {QFile log(qEnvironmentVariable("OPENLOCH_PCB_TEST_PLUGIN_LOG"));if(log.open(QIODevice::WriteOnly))log.write(arguments.join('\n').toUtf8());}
    if(arguments.isEmpty())return 200;
    try{
        QFile in(arguments[0]);if(!in.open(QIODevice::ReadOnly))return 201;
        auto els=sprint::readTextIO(sprint::textIOText(in.readAll()));for(auto &e:els)pcb::move(e,{5,0});
        auto t=newElement(ElementType::Track);t.points={{1,1},{3,1}};t.width=.5;els.append(t);
        const QFileInfo info(arguments[0]);QFile out(info.absolutePath()+"/"+info.completeBaseName()+"_out."+info.suffix());
        if(!out.open(QIODevice::WriteOnly))return 202;out.write(sprint::textIOBytes(sprint::writeTextIO(els)));
    }catch(const std::exception &){return 203;}
    return code;
}

int main(int argc,char **argv){
    if(qEnvironmentVariableIsSet("OPENLOCH_PCB_TEST_PLUGIN"))return testPlugin(argc,argv);
    QApplication app(argc,argv);setUiLanguage("de");
    // The editor's preferences go to a file of the test run, never to the user's settings.
    QTemporaryDir settings;const QString preferences=settings.filePath("preferences.ini");Editor::setPreferencesFile(preferences);
    try{
        // --- Sprint-Layout: reading the documented layout
        const auto fixture=fixtureLayout();
        require(sprint::fileVersion(fixture)==6&&sprint::fileVersion("{}")<0&&sprint::fileVersion(QByteArray::fromHex("0433aaff01000000"))==4,"file version detection");
        const Document d=sprint::readLayout(fixture);
        require(d.boards.size()==1&&d.title=="Titel"&&d.author=="Autor"&&d.company=="Firma"&&d.comment=="Kommentar ä€","project data");
        const Board &b=d.boards[0];
        require(b.name=="Test"&&near(b.width,80)&&near(b.height,50)&&near(b.grid,2.54)&&b.activeLayer==1&&b.groundPlane[CopperBottom]&&!b.groundPlane[CopperTop],"board header");
        require(near(b.origin,{0,0}),"an origin stored as zero lies in the top left corner");
        require(b.visible[CopperTop]&&b.visible[SilkBottom]&&!b.visible[Inner1]&&!b.visible[Inner2]&&b.visible[Outline],"layer visibility");
        require(b.elements.size()==6,"element count");
        const auto &pad=b.elements[0];
        require(pad.type==ElementType::Pad&&near(pad.pos,{10,10})&&near(pad.size,1.6)&&near(pad.size2,.8)&&pad.shape==PadShape::Square&&pad.via&&pad.solderMask,"through-hole pad");
        require(pad.points.size()==4&&near(pad.points[0],{9.2,9.2})&&near(pad.rotation,0)&&pad.groups==QList<int>{7}&&pad.connections==QList<int>{1},"pad outline, group and airwire");
        require(near(pad.clearance,.3)&&near(b.elements[2].clearance,0),"clearance (an integer field)");
        require(pad.thermal&&pad.thermalPerLayer&&pad.thermalSpokes==0x5501&&pad.thermalWidth==100,"thermal pad with spokes per layer");
        const auto &smd=b.elements[1];
        require(smd.type==ElementType::SmdPad&&near(smd.size,2)&&near(smd.size2,1)&&near(smd.rotation,270)&&smd.layer==CopperTop&&smd.connections==QList<int>{0},"SMD pad");
        const auto &track=b.elements[2];
        require(track.type==ElementType::Track&&near(track.width,.8)&&track.name=="GND"&&track.points.size()==2&&near(track.points[1],{30,20}),"track");
        require(!track.flatStart&&track.flatEnd,"square track end");
        const auto &arc=b.elements[3];
        require(arc.type==ElementType::Circle&&near(arc.pos,{40,25})&&near(arc.size,5)&&near(arc.width,1)&&near(arc.start,90)&&near(arc.stop,180)&&arc.layer==SilkTop,"arc");
        const auto &area=b.elements[4];
        require(area.type==ElementType::Area&&area.points.size()==3&&near(area.width,.4)&&near(area.points[2],{55,20}),"area");
        require(area.hatched&&!area.hatchAuto&&near(area.hatchPitch,1)&&near(hatchSpacing(area),1),"hatched area");
        const auto &text=b.elements[5];
        require(text.type==ElementType::Text&&text.text=="R1"&&near(text.size,2)&&text.style==2&&text.thickness==2&&near(text.rotation,90)&&text.mirrored,"text");
        require(!text.visible,"a designator with its visibility switch off is hidden");
        require(text.role==TextRole::Designator&&text.package=="0805"&&near(text.componentRotation,45)&&text.pickAndPlace&&text.strokes.size()==2&&near(text.strokeWidth,.32),"designator");
        // The component: the elements with its number; the offset counts y downwards in the file, upwards in the model.
        {const auto list=components(b);
            require(list.size()==1&&list[0].part==5&&list[0].designator==5&&list[0].value<0&&list[0].members==QList<int>({0,1,5})&&b.elements[2].part==0,"a component by its number");
            require(near(text.pickOffset,{1,-2.5})&&text.pickCentre==2,"pick and place offset and centre (3 counts as both)");}
        // Unchanged elements are written back byte for byte, with the fields OpenLoch does not know.
        // (The project strings at the end are written with zeros after the text.)
        {auto expected=fixture;for(const char *s:{"Titel","Autor","Firma"})expected.replace(QByteArray(s)+QByteArray(95,'\x5a'),QByteArray(s)+QByteArray(95,'\0'));
            require(sprint::writeLayout(d)==expected,"an unchanged layout is not written back as read");}
        // Truncated and foreign files are refused with a message.
        for(int cut:{5,20,0x212,0x230,int(fixture.size())-10})rejects([&]{sprint::readLayout(fixture.left(cut));},"a truncated layout was accepted");
        rejects([]{sprint::readLayout(QByteArray::fromHex("0333aaff0100000000"));},"a truncated version 3 layout was accepted");
        rejects([]{sprint::readLayout("{\"format\":1}");},"a JSON file was read as Sprint-Layout");
        {auto bad=fixture;bad[0x21e + 0x16]=9;rejects([&]{sprint::readLayout(bad);},"a layer outside 1..7 was accepted");}
        // A point that is no finite number breaks the file: a node, a corner of a pad outline or a point of a text stroke;
        // in version 5 also one that becomes none times 100. A large finite one is read.
        {const float inf=INFINITY;auto patched=[](QByteArray f,const QByteArray &point,float x,float y){const auto at=f.indexOf(point);require(at>0,"the point in the fixture");f.replace(at,8,f32le(x)+f32le(y));return f;};
            const auto node=f32le(500000)+f32le(-100000),corner=f32le(92000)+f32le(-92000),stroke=f32le(100000)+f32le(-380000);
            for(const auto &f:{patched(fixture,node,std::nanf(""),-100000),patched(fixture,node,500000,-inf),patched(fixture,corner,inf,-92000),patched(fixture,stroke,100000,std::nanf("")),
                               patched(fixtureLayout(5),f32le(5000)+f32le(-1000),1e37f,-1000)})rejects([&]{sprint::readLayout(f);},"a point that is no finite number was read");
            require(sprint::readLayout(patched(fixture,node,1e37f,-100000)).boards[0].elements[4].points[0].x()>1e32,"a large finite point is read");}
        // Version 5 (Sprint-Layout 5): the layout of version 6 with lengths in hundredths of a millimetre, converted to
        // version 6 when read. The version 5 fixture holds the board and elements of the version 6 one in that unit and
        // reads alike, apart from what only version 6 has.
        {const auto d5=sprint::readLayout(fixtureLayout(5));const auto &b5=d5.boards[0];
            require(near(b5.width,80)&&near(b5.height,50)&&near(b5.grid,2.54)&&b5.elements.size()==6,"version 5 board header in hundredths of a millimetre");
            for(int k=0;k<6;k++){const auto &x=b5.elements[k],&y=b.elements[k];
                require(x.type==y.type&&x.layer==y.layer&&near(x.pos,y.pos)&&near(x.size,y.size)&&near(x.size2,y.size2)&&near(x.width,y.width)&&near(x.clearance,y.clearance)
                        &&x.points==y.points&&near(x.rotation,y.rotation)&&x.strokes==y.strokes&&near(x.strokeWidth,y.strokeWidth)&&near(x.start,y.start)&&near(x.stop,y.stop)
                        &&x.thickness==y.thickness&&x.style==y.style&&x.mirrored==y.mirrored,"version 5 elements in hundredths of a millimetre");}
            // The thermal fields count with the mark, spokes per layer came with version 6; the hatch fields of areas are
            // taken as stored, the pitch in tenths of a micrometre as in version 6.
            require(b5.elements[0].thermal&&b5.elements[0].thermalSpokes==0x5501&&!b5.elements[0].thermalPerLayer,"version 5 thermal pad without spokes per layer");
            require(b5.elements[4].hatched&&!b5.elements[4].hatchAuto&&near(b5.elements[4].hatchPitch,1),"version 5 hatch fields as stored");
            // Written as version 6 in its unit: the board header as in the version 6 fixture, the pad but for its component
            // number and spokes per layer, the arc and the area byte for byte, the text as a plain one.
            const auto six=sprint::writeLayout(d5);const int arc=int(fixture.indexOf(QByteArray(1,'\x05')+f32le(400000))),text=int(fixture.indexOf(QByteArray("\x02\0\0\0R1",6)))-0x4c-1;
            auto pad6=fixture.mid(0x21e,0x4d);pad6.replace(0x1c,2,QByteArray(2,'\0'));pad6[0x28]=0;
            auto text6=fixture.mid(text,0x4d);text6.replace(0x1c,2,QByteArray(2,'\0'));text6[0x17]=0;text6[0x28]=1;
            require(six.mid(8,0x212)==fixture.mid(8,0x212),"version 5 board header written as version 6");
            require(six.mid(0x21e,0x4d)==pad6,"version 5 pad written in tenths of a micrometre");
            require(arc>0&&text>arc&&six.mid(arc,text-arc)==fixture.mid(arc,text-arc)&&six.mid(text,0x4d)==text6,"version 5 arc, area and text written as version 6");
            require(sprint::readLayout(six).boards[0].elements==b5.elements,"version 5 elements read again from version 6");}
        // A version 5 macro: a resistor with two round pads 10.16 mm apart (1.6 mm, drill 0.8 mm), its body outline on B1
        // (0.15 mm wide), a value text 1.5 mm high with normal strokes, turned by 90°, and half a ring; lengths in
        // hundredths of a millimetre, as Sprint-Layout 5 stores them. The outline holds anything in the fields a track does
        // not use, as such files do: lengths that are no number or beyond 100 m become 0.
        {Bytes m;m.b=QByteArray::fromHex("0533aaff");m.u32(5);
            for(float x:{0.f,1016.f}){Rec pad{2,x,0,80,40,0,3,1};pad.fields={{0x29,le32(40)},{0x3a,QByteArray("\x01",1)}};pad.points={{x-80,0},{x+80,0}};record(m,pad);}
            Rec body{6,1e30f,std::nanf(""),7e20f,0,15,2,0};body.fields={{0x29,le32(40)}};body.points={{150,100},{866,100},{866,-100},{150,-100},{150,100}};record(m,body);
            Rec value{7,300,300,150,1,1,2,0};value.text="10k";value.fields={{0x35,le32(90)}};value.strokes={{{300,300},{300,450}}};value.strokeWidth=15;value.strokeClearance=40;record(m,value);
            Rec ring{5,508,0,60,50,180000,2,0};record(m,ring);
            m.u32(0);m.u32(0);m.b.append(QByteArray(102,'\0'));
            const auto e=sprint::readMacro(m.b);
            require(e.size()==5&&near(e[0].pos,{0,0})&&near(e[1].pos,{10.16,0})&&near(e[1].size,1.6)&&near(e[1].size2,.8)&&near(e[1].clearance,.4)&&near(e[1].points[1],{10.96,0}),
                    "version 5 macro: pads in hundredths of a millimetre");
            require(near(e[2].width,.15)&&near(e[2].clearance,.4)&&e[2].points.size()==5&&near(e[2].points[1],{8.66,-1})&&near(e[2].points[2],{8.66,1}),"version 5 macro: outline");
            require(near(e[3].pos,{3,-3})&&near(e[3].size,1.5)&&e[3].thickness==1&&e[3].style==1&&near(e[3].rotation,270)&&near(e[3].strokeWidth,.15)&&near(e[3].strokes[0][1],{3,-4.5}),
                    "version 5 macro: text height and strokes converted, thickness and angle not");
            require(near(e[4].pos,{5.08,0})&&near(e[4].size,.55)&&near(e[4].width,.1)&&near(e[4].start,0)&&near(e[4].stop,180),"version 5 macro: ring converted, its angles not");
            const auto six=sprint::writeMacro(e,6);auto has=[&](const QByteArray &part){return six.contains(part);};
            require(has(QByteArray(1,'\x02')+f32le(101600)+f32le(0)+f32le(8000)+f32le(4000))&&has(QByteArray(1,'\x06')+QByteArray(16,'\0')+le32(1500))
                    &&has(QByteArray(1,'\x07')+f32le(30000)+f32le(30000)+f32le(15000)+f32le(1)+le32(1))&&has(QByteArray(1,'\x05')+f32le(50800)+f32le(0)+f32le(6000)+f32le(5000)+le32(180000)),
                    "version 5 macro written in tenths of a micrometre");
            auto withoutRecords=[](QList<Element> list){for(auto &x:list)x.sprint={};return list;};
            require(withoutRecords(sprint::readMacro(six))==withoutRecords(e),"version 5 macro read again from version 6");}
        // Thermal fields count only with the mark of version 6: a version 5 pad without it gets the defaults, and written
        // as version 6 the mark and clean values. Track ends and roles of texts are not read from version 5, and it has
        // neither component numbers nor component data.
        {auto old5=fixtureLayout(5);const int pad=0x21e;old5[pad+0x38]=char(108);old5[pad+0x32]=char(48);for(int k=0;k<4;k++)old5[pad+0x3e + k]='0';
            const int track=int(old5.indexOf(QByteArray("\x03\0\0\0GND",7)))-0x4c-1;old5[track+0x17]=char(105);
            const auto d5=sprint::readLayout(old5);require(!d5.boards[0].elements[0].thermal&&d5.boards[0].elements[0].thermalWidth==100,"version 5 thermal bytes ignored");
            require(!d5.boards[0].elements[2].flatStart&&!d5.boards[0].elements[2].flatEnd,"version 5 track ends ignored");
            require(d5.boards[0].elements[0].part==0&&d5.boards[0].elements[5].role==TextRole::Plain&&components(d5.boards[0]).isEmpty(),"version 5 has no components");
            const auto again=sprint::writeLayout(d5);
            require(again[pad+0x38]==0&&again[pad+0x32]==0&&again[pad+0x28]==0&&qFromLittleEndian<quint32>(again.constData()+pad+0x3e)==123456789&&qFromLittleEndian<quint32>(again.constData()+pad+0x1f)==0x55,"version 5 pads written with the version 6 mark");
            require(again[track+0x17]==0&&again[pad+0x1c]==0,"version 5 track written with round ends, pads without component number");}
        // --- Board header: the coordinate origin and the offsets and colours of the templates (the first board header
        // starts at file offset 8); unchanged values are written back as read.
        {auto asWritten=[](QByteArray f){for(const char *s:{"Titel","Autor","Firma"})f.replace(QByteArray(s)+QByteArray(95,'\x5a'),QByteArray(s)+QByteArray(95,'\0'));return f;};
            auto patched=[&](const QList<std::pair<int,qint32>> &fields){auto f=fixture;for(const auto &[at,v]:fields)f.replace(8+at,4,le32(quint32(v)));return f;};
            auto at=[](const QByteArray &bytes,int position){return qFromLittleEndian<qint32>(bytes.constData()+8+position);};
            // The origin: x and y from the top left corner of the working area in tenths of a micrometre, y up, off the
            // grid or outside the working area as stored.
            {const auto f=patched({{0x209,123456},{0x20d,-234567}});const auto o=sprint::readLayout(f);
                require(near(o.boards[0].origin,{12.3456,23.4567})&&sprint::writeLayout(o)==asWritten(f),"origin read from the board header and written back");
                require(near(sprint::readLayout(patched({{0x20d,200000}})).boards[0].origin,{0,-20}),"origin above the working area");}
            // Beyond the model's 10 m the board counts from the top left corner; the bytes stay until the origin moves.
            {const auto f=patched({{0x209,2147483647}});auto o=sprint::readLayout(f);
                require(near(o.boards[0].origin,{0,0})&&sprint::writeLayout(o)==asWritten(f),"an origin out of range reads as the top left corner and stays in the file");
                o.boards[0].origin={1,2};const auto moved=sprint::writeLayout(o);require(at(moved,0x209)==10000&&at(moved,0x20d)==-20000,"a moved origin is written");}
            // Templates: offsets in tenths of a millimetre (x top, x bottom, y top, y bottom; y down), colours as 0x00bbggrr.
            {const auto f=patched({{0x1f1,100},{0x1f5,380},{0x1f9,50},{0x1fd,150},{0x201,0x0000ff00},{0x205,0x00336699}});auto o=sprint::readLayout(f);
                const auto &t=o.boards[0].templates;
                require(near(t[0].offset,{10,5})&&near(t[1].offset,{38,15})&&t[0].colour==QColor(0,255,0)&&t[1].colour==QColor(0x99,0x66,0x33),"template offsets and colours");
                require(sprint::writeLayout(o)==asWritten(f),"templates written back as read");
                // Changed: rounded to the unit and limited to ±300 mm, as the reference reads them.
                auto &w=o.boards[0].templates;w[0].offset={12.34,450};w[1].offset={-1.24,7.06};w[1].colour=QColor(255,0,0);const auto changed=sprint::writeLayout(o);
                require(at(changed,0x1f1)==123&&at(changed,0x1f5)==-12&&at(changed,0x1f9)==3000&&at(changed,0x1fd)==71&&at(changed,0x201)==0x0000ff00&&at(changed,0x205)==0xff,"template offsets and colours written");}
            // Resolutions as the reference shows them: 20 to 2400 dpi (0 is none, the usual 600); unchanged they stay in the
            // file, changed ones are written within the limits.
            {const auto f=patched({{0x1e9,5000},{0x1ed,10}});auto o=sprint::readLayout(f);auto &t=o.boards[0].templates;
                require(near(t[0].dpi,2400)&&near(t[1].dpi,20)&&near(sprint::readLayout(patched({{0x1e9,0}})).boards[0].templates[0].dpi,600),"template resolutions limited to 20 to 2400 dpi");
                t[0].file="oben.bmp";t[1].file="unten.bmp";const auto kept=sprint::writeLayout(o);require(at(kept,0x1e9)==5000&&at(kept,0x1ed)==10,"limited resolutions stay in the file while unchanged");
                t[0].dpi=3000;t[1].dpi=300;const auto changed=sprint::writeLayout(o);require(at(changed,0x1e9)==2400&&at(changed,0x1ed)==300,"changed resolutions written within the limits");}
            // The reference shows offsets beyond ±300 mm at that limit; unchanged they stay in the file.
            {const auto f=patched({{0x1f1,3500},{0x1fd,-3500}});const auto o=sprint::readLayout(f);
                require(near(o.boards[0].templates[0].offset.x(),300)&&near(o.boards[0].templates[1].offset.y(),-300)&&sprint::writeLayout(o)==asWritten(f),"template offsets beyond the limit");}
            // New boards: no offsets, the reference's template colour (lime) and the origin in the bottom left corner.
            {Document fresh;fresh.boards={newBoard("Neu",60,40)};
                require(sprint::writeLayout(fresh).mid(8+0x1f1,32)==QByteArray(16,'\0')+QByteArray::fromHex("00ff000000ff00000000000080e5f9ff"),"new board: offsets, colours and origin");
                fresh.boards[0].origin={0,0};require(sprint::writeLayout(fresh).mid(8+0x209,8)==QByteArray(8,'\0'),"origin in the top left corner");}
            // Version 5: the same places, the origin in hundredths of a millimetre; written as version 6 in that unit.
            {auto f=fixtureLayout(5);for(const auto &[at,v]:QList<std::pair<int,qint32>>{{0x1f1,123},{0x1fd,77},{0x209,2000},{0x20d,-1500}})f.replace(8+at,4,le32(quint32(v)));
                const auto o=sprint::readLayout(f);
                require(near(o.boards[0].origin,{20,15})&&near(o.boards[0].templates[0].offset,{12.3,0})&&near(o.boards[0].templates[1].offset,{0,7.7}),"version 5 origin and template offsets");
                const auto six=sprint::writeLayout(o);require(at(six,0x209)==200000&&at(six,0x20d)==-150000,"version 5 origin written as version 6");}}
        // Component numbers come from the model: an element out of the component gets 0, a number beyond 16 bits (or
        // 0xFFFF) a free one; two designators with one number are one component, the last one counts.
        {Document numbers=d;auto &els=numbers.boards[0].elements;els[1].part=0;els[2].part=70000;els[4].part=65535;
            auto second=els[5];second.text="R2";second.sprint={};els.append(second);
            const auto back=sprint::readLayout(sprint::writeLayout(numbers)).boards[0];
            require(back.elements[0].part==5&&back.elements[1].part==0&&back.elements[2].part>5&&back.elements[2].part<65535&&back.elements[4].part>5&&back.elements[4].part<65535
                    &&back.elements[2].part!=back.elements[4].part,"component numbers written from the model");
            const auto list=components(back);require(list.size()==1&&list[0].designator==6&&back.elements[6].text=="R2"&&list[0].members==QList<int>({0,5,6}),"the last designator counts");}
        // The component data keeps its bytes while unchanged (also a denormal offset, as an integer would read); changed
        // fields are written, a zero offset as +0. A designator of role 3 (designator and value bit) has component data.
        {const int at=int(fixture.indexOf(QByteArray("\x02\0\0\0R1",6)))-0x4c-1;
            const int block=int(fixture.indexOf(QByteArray("\x04\0\0\0" "0805",8)))-17;require(at>0&&block>at&&fixture.mid(block,4)==le32(0x461c4000),"component data in the fixture");
            auto odd=fixture;odd.replace(block,4,le32(10000));odd[at+0x17]=3;const auto expected=sprint::writeLayout(sprint::readLayout(odd));
            const auto t=sprint::readLayout(odd).boards[0].elements[5];
            require(t.role==TextRole::Designator&&std::abs(t.pickOffset.x())<1e-30&&expected.mid(at,0x60)==odd.mid(at,0x60)&&expected.mid(block,17)==odd.mid(block,17),"odd component data kept");
            // Component data the model cannot hold (not finite, an offset beyond 1 m, a rotation beyond 1e6°) reads as 0 and
            // stays in the file; the own format reads the document again.
            for(double bad:{std::nan(""),1e20}){Bytes c;c.f32(float(bad));c.f32(float(bad));c.f64(bad);auto f=fixture;f.replace(block,8,c.b.left(8));f.replace(block+9,8,c.b.mid(8));
                Document o=sprint::readLayout(f);const auto &x=o.boards[0].elements[5];
                require(x.componentRotation==0&&x.pickOffset==QPointF()&&sprint::writeLayout(o).mid(block,17)==f.mid(block,17),"component data out of range reads as 0 and stays");
                assignIds(o);require(decode(encode(o))==o,"component data out of range: the own format reads it again");}
            Document moved=d;auto &e=moved.boards[0].elements[5];e.pickOffset={0,0};e.pickCentre=1;
            const auto bytes=sprint::writeLayout(moved);require(bytes.mid(block,9)==QByteArray::fromHex("000000000000000001")&&bytes.mid(block+9,8)==fixture.mid(block+9,8),"changed component data");
            Document fresh;fresh.boards={newBoard("x",20,20)};auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="U1";id.part=1;updateStrokes(id);
            fresh.boards[0].elements={id};const auto one=sprint::writeLayout(fresh);
            // The component data follows the record, the text "U1", the second string, the groups and the strokes.
            const int start=int(one.indexOf(QByteArray("\x02\0\0\0U1",6)))-0x4c-1;int data=start+1+0x4c+6+4+4+4;for(const auto &s:id.strokes)data+=1+0x4c+4+8*int(s.size());
            const auto read=sprint::readLayout(one).boards[0].elements.value(0);
            require(start>0&&one[start]==7&&one.mid(data,17)==QByteArray(17,'\0')&&one.mid(data+17,8)==le32(0)+le32(0)&&read.part==1&&read.pickOffset==QPointF()&&read.pickCentre==0,
                    "a new designator: zero component data, its number");
            fresh.boards[0].elements[0].pickCentre=1;require(sprint::writeLayout(fresh).mid(data,17)==QByteArray::fromHex("000000000000000001")+QByteArray(8,'\0'),"the centre at position 8");}
        // A text mirrored top to bottom (0x33) is mirrored left to right and turned by half a turn; the flag stays in the
        // record, and a changed text is written with a horizontal flag and an angle that give the same with it.
        {const int at=int(fixture.indexOf(QByteArray("\x02\0\0\0R1",6)))-0x4c-1;auto both=fixture;both[at+0x33]=1;
            const auto t=sprint::readLayout(both).boards[0].elements[5];require(!t.mirrored&&near(t.rotation,270),"both mirror flags: half a turn");
            require(sprint::writeLayout(sprint::readLayout(both)).mid(at,0x4d)==both.mid(at,0x4d),"a vertically mirrored text written back unchanged");
            Document d2=sprint::readLayout(both);d2.boards[0].elements[5].mirrored=true;auto bytes=sprint::writeLayout(d2);
            require(bytes[at+0x32]==0&&bytes[at+0x33]==1&&qFromLittleEndian<qint32>(bytes.constData()+at+0x35)==270,"mirrored again: the horizontal flag goes");
            auto upright=fixture;upright[at+0x32]=0;upright[at+0x33]=1;upright.replace(at+0x35,4,le32(0));
            Document d3=sprint::readLayout(upright);auto &u=d3.boards[0].elements[5];require(u.mirrored&&near(u.rotation,180),"vertical flag alone: mirrored, half a turn");
            const auto io=sprint::writeTextIO({u});const auto line=io.split('\n').filter("ID_TEXT").value(0);
            require(line.contains("MIRROR_VERT=true")&&!line.contains("MIRROR_HORZ")&&!line.contains("ROTATION"),"Text-IO keeps the vertical mirror");
            {const auto r=sprint::readTextIO(io).value(0);require(r.mirrored&&near(r.rotation,180),"Text-IO vertical mirror read back");}
            u.mirrored=false;bytes=sprint::writeLayout(d3);
            require(bytes[at+0x32]==1&&bytes[at+0x33]==1&&qFromLittleEndian<qint32>(bytes.constData()+at+0x35)==0,"not mirrored, turned by half: both flags");
            require(u.flipped&&sprint::mirroredVertically(u),"the vertical flag kept in the model");
            // A text mirrored top to bottom in OpenLoch: the flag goes into the record, Text-IO and the own format.
            {Document mine;Board mb=newBoard("Spiegel",20,20);auto t=newElement(ElementType::Text);t.text="T";t.pos={5,5};t.size=2;
                t.flipped=true;t.mirrored=true;t.rotation=180;updateStrokes(t);mb.elements<<t;mine.boards={mb};
                const auto back=sprint::readLayout(sprint::writeLayout(mine)).boards[0].elements.value(0);
                require(back.flipped&&back.mirrored&&near(back.rotation,180),"a new vertical mirror written as the reference's flag");
                const auto line=sprint::writeTextIO({t});require(line.contains("MIRROR_VERT=true")&&!line.contains("MIRROR_HORZ")&&!line.contains("ROTATION"),"and as Text-IO's");
                require(fromJson(toJson(mine)).boards[0].elements.value(0).flipped,"and in the own format");}}

        // --- Sprint-Layout: writing, then reading back
        const Document sample=sampleDocument();
        auto sample_=[&]{return sample;};
        const auto written=sprint::writeLayout(sample);
        Document back=sprint::readLayout(written);
        require(back.boards.size()==2&&back.activeBoard==1&&back.title=="Probe"&&back.comment=="zwei Platinen","written project data");
        for(auto &board:back.boards){board.sprintHeader.clear();for(auto &e:board.elements)e.sprint={};}
        for(int i=0;i<2;i++){
            const auto &x=sample.boards[i],&y=back.boards[i];
            require(x.name==y.name&&near(x.width,y.width)&&near(x.height,y.height)&&near(x.grid,y.grid)&&x.activeLayer==y.activeLayer&&x.visible==y.visible&&x.groundPlane==y.groundPlane,"written board header");
            require(x.elements.size()==y.elements.size(),"written element count");
            for(int k=0;k<2;k++)require(x.templates[k].file==y.templates[k].file&&x.templates[k].shown==y.templates[k].shown&&(x.templates[k].file.isEmpty()||near(x.templates[k].dpi,y.templates[k].dpi)),"written templates");
            require(near(x.origin,y.origin)&&x.templates==y.templates,"written origin and templates");
            for(int k=0;k<x.elements.size();k++){
                const auto &p=x.elements[k],&q=y.elements[k];
                require(p.type==q.type&&p.layer==q.layer&&near(p.pos,q.pos,1e-5)&&near(p.size,q.size,1e-5)&&near(p.size2,q.size2,1e-5)&&near(p.width,q.width,1e-5),"written element geometry");
                require(p.points.size()==q.points.size()&&p.groups==q.groups&&p.part==q.part&&p.connections==q.connections&&p.name==q.name,"written element lists");
                for(int n=0;n<p.points.size();n++)require(near(p.points[n],q.points[n],1e-4),"written element nodes");
                if(!(p.shape==q.shape&&p.via==q.via&&near(p.rotation,q.rotation,1e-3)&&near(p.start,q.start,1e-3)&&near(p.stop,q.stop,1e-3)&&p.filled==q.filled))
                    fprintf(stderr,"element %d: shape %d/%d via %d/%d rotation %g/%g start %g/%g stop %g/%g filled %d/%d\n",k,int(p.shape),int(q.shape),p.via,q.via,p.rotation,q.rotation,p.start,q.start,p.stop,q.stop,p.filled,q.filled);
                require(p.shape==q.shape&&p.via==q.via&&near(p.rotation,q.rotation,1e-3)&&near(p.start,q.start,1e-3)&&near(p.stop,q.stop,1e-3)&&p.filled==q.filled,"written element details");
                require(near(p.clearance,q.clearance,1e-6),"written clearance");
                require(p.text==q.text&&p.style==q.style&&p.thickness==q.thickness&&p.mirrored==q.mirrored&&p.role==q.role&&p.package==q.package&&p.comment==q.comment
                        &&near(p.componentRotation,q.componentRotation)&&p.pickAndPlace==q.pickAndPlace&&p.strokes.size()==q.strokes.size()&&near(p.strokeWidth,q.strokeWidth,1e-5)
                        &&p.pickCentre==q.pickCentre&&near(p.pickOffset,q.pickOffset,1e-5),"written text");
                require(p.flatStart==q.flatStart&&p.flatEnd==q.flatEnd&&p.hatched==q.hatched&&p.hatchAuto==q.hatchAuto&&near(p.hatchPitch,q.hatchPitch,1e-4)&&p.visible==q.visible,"written switches");
            }
        }
        // The reference draws pads from their outline: it is always written complete for the form.
        {Document one;Board bb=newBoard("P",20,20);for(int s=1;s<=9;s++){auto p=newElement(ElementType::Pad);p.shape=PadShape(s);p.pos={2.0*s,5};p.points.clear();bb.elements<<p;}
            one.boards={bb};const auto again=sprint::readLayout(sprint::writeLayout(one)).boards[0].elements;
            const int counts[]={2,8,4,2,8,4,2,8,4};for(int s=0;s<9;s++)require(again[s].points.size()==counts[s]&&near(again[s].points[0],padOutline(PadShape(s+1),{2.0*(s+1),5},1.8,0)[0],1e-5),"pad outline written for every form");}
        // Arc angles: thousandths of a degree on file, below 1000 read as whole degrees.
        require(near(back.boards[0].elements[3].stop,315.5),"arc angle precision");

        // --- Text-IO: every kind of element and back, groups and components as blocks, airwires by pad ids
        {// Group blocks follow the loose elements: the pad and the designator of group 1 come last.
            const auto &els=sample.boards[0].elements;const auto again=sprint::readTextIO(sprint::writeTextIO(els));
            const QList<int> order{1,2,3,4,5,0,6};QMap<int,int> place;for(int k=0;k<order.size();k++)place[order[k]]=k;
            require(again.size()==els.size(),"Text-IO element count");
            for(int k=0;k<order.size();k++){
                const auto &p=els[order[k]],&q=again[k];
                require(p.type==q.type&&p.layer==q.layer&&near(p.pos,q.pos,1e-4)&&near(p.size,q.size,1e-4)&&near(p.size2,q.size2,1e-4)&&near(p.width,q.width,1e-4),"Text-IO geometry");
                require(p.points.size()==q.points.size(),"Text-IO nodes");
                require(p.shape==q.shape&&p.via==q.via&&near(p.rotation,q.rotation,1e-2)&&near(p.start,q.start,1e-3)&&near(p.stop,q.stop,1e-3)&&p.filled==q.filled,"Text-IO details");
                require(p.text==q.text&&p.role==q.role&&p.mirrored==q.mirrored&&p.package==q.package&&p.comment==q.comment&&p.pickAndPlace==q.pickAndPlace&&p.visible==q.visible,"Text-IO texts");
                QList<int> wires;for(int c:p.connections)wires.append(place[c]);
                require(wires==q.connections&&p.groups.size()==q.groups.size()&&p.name==q.name&&p.hatched==q.hatched&&p.flatStart==q.flatStart,"Text-IO lists");
            }
            require(again[5].groups==again[6].groups&&!again[5].groups.isEmpty()&&again[5].part==again[6].part&&again[5].part!=0,"Text-IO keeps the component together");
            require(sprint::writeTextIO(els).contains("ROTATION=270000")&&sprint::writeTextIO(els).contains("GROUP;\n   BEGIN_COMPONENT"),"Text-IO: text angles in thousandths of a degree, components inside groups");
            require(sprint::writeTextIO(again)==sprint::writeTextIO(sprint::readTextIO(sprint::writeTextIO(again))),"Text-IO written again unchanged");}
        {// Written by hand: any case, spaces, commas and semicolons inside bars, nested groups, a component, airwires.
            const auto els=sprint::readTextIO(QStringLiteral(
                "group;\n group;\n  pad,layer=3, pos=100000 / 200000,size=18000,drill=8000,form=3,pad_id=7,con0=8;\n end_group;\n"
                "  smdpad, LAYER=1, POS=300000/200000, SIZE_X=10000, SIZE_Y=20000, ROTATION=9000, PAD_ID=8;\nEND_GROUP;\n"
                "BEGIN_COMPONENT, PACKAGE=|0805|, COMMENT=|a, b; c|;\n ID_TEXT, LAYER=2, POS=0/0, HEIGHT=15000, TEXT=|R1|;\n VALUE_TEXT, LAYER=2, POS=0/30000, HEIGHT=15000, TEXT=|4k7|, VISIBLE=false;\nEND_COMPONENT;\n"
                "ZONE, LAYER=3, WIDTH=4000, HATCH=true, HATCH_AUTO=false, HATCH_WIDTH=10000, P0=0/0, P1=100000/0, P2=0/100000;\n"
                "CIRCLE, LAYER=2, WIDTH=2000, CENTER=50000/50000, RADIUS=20000, START=90000, STOP=180000;\n"
                "TRACK, LAYER=3, WIDTH=8000, P0=0/0, P1=50000/0, FLATEND=true, CUTOUT=true, NAME=|Masse|;\n"));
            require(els.size()==7&&els[0].type==ElementType::Pad&&els[0].shape==PadShape::Square&&near(els[0].pos,{10,20})&&near(els[0].size,1.8),"Text-IO pad");
            require(els[0].groups.size()==2&&els[1].groups.size()==1&&els[0].groups.last()==els[1].groups.last()&&els[0].groups.first()!=els[1].groups.first(),"Text-IO nested groups");
            require(els[0].connections==QList<int>{1}&&els[1].connections==QList<int>{0},"Text-IO airwires");
            require(els[1].type==ElementType::SmdPad&&near(els[1].rotation,270)&&els[1].layer==CopperTop,"Text-IO SMD pad turned clockwise");
            require(els[2].role==TextRole::Designator&&els[2].text=="R1"&&els[2].package=="0805"&&els[2].comment=="a, b; c"&&els[3].role==TextRole::Value&&!els[3].visible,"Text-IO component");
            require(els[2].part!=0&&els[2].part==els[3].part&&els[2].groups.isEmpty()&&els[0].part==0,"Text-IO component as a component number");
            require(near(sprint::readTextIO("TEXT, LAYER=2, POS=0/0, HEIGHT=10000, TEXT=|A|, ROTATION=90000;").value(0).rotation,270),"Text-IO text angle: thousandths of a degree, clockwise");
            require(els[4].hatched&&!els[4].hatchAuto&&near(els[4].hatchPitch,1)&&near(els[5].start,90)&&near(els[5].stop,180)&&els[6].flatEnd&&els[6].cutout&&els[6].name=="Masse","Text-IO zone, arc and track");
            rejects([]{sprint::readTextIO("TRACK, LAYER=3, WIDTH=8000, P0=0/0, P1=1/1");},"Text-IO without semicolon accepted");
            rejects([]{sprint::readTextIO("WIRE, LAYER=3;");},"unknown Text-IO entry accepted");
            rejects([]{sprint::readTextIO("END_GROUP;");},"Text-IO block end without start accepted");
            rejects([]{sprint::readTextIO("PAD, LAYER=9, POS=0/0, SIZE=1, DRILL=1, FORM=1;");},"Text-IO layer 9 accepted");
            // Code page 1252 as the reference writes it; files in UTF-8 are read as well.
            require(sprint::textIOBytes(QStringLiteral("Grüße 5€"))==QByteArray("Gr\xfc\xdf" "e 5\x80")&&sprint::textIOText(QByteArray("Gr\xfc\xdf" "e 5\x80"))==QStringLiteral("Grüße 5€")
                    &&sprint::textIOText(QByteArray("\xc3\x84"))==QStringLiteral("Ä"),"Text-IO code page");}
        {// A component whose members sit in different groups stays one block: pads in a group and the texts without one (as
         // in many macros), and pads in a group of their own inside the component's group (as components of version 1 to 3
         // files). The groups only some members have stand inside the component's block; written again, nothing changes.
            auto member=[](ElementType type,QPointF at,const QList<int> &groups,TextRole role){
                auto e=newElement(type);e.pos=at;e.groups=groups;e.part=4;e.role=role;if(type==ElementType::Text){e.text=role==TextRole::Designator?"R1":"4k7";updateStrokes(e);}else updateOutline(e);return e;};
            auto blocks=[](const QString &io){QStringList b;for(const auto &line:io.split('\n')){const QString head=line.section(QRegularExpression("[,;]"),0,0);
                if(QRegularExpression("^(GROUP|END_GROUP|BEGIN_COMPONENT|END_COMPONENT)$").match(head.trimmed()).hasMatch())b<<head;}return b.join('|');};
            struct Case {QList<int> inner,outer;QString nesting;};
            for(const auto &[inner,outer,nesting]:{Case{{1},{},"BEGIN_COMPONENT|   GROUP|   END_GROUP|END_COMPONENT"},
                                                   Case{{7,5},{5},"GROUP|   BEGIN_COMPONENT|      GROUP|      END_GROUP|   END_COMPONENT|END_GROUP"}}){
                const QList<Element> els{member(ElementType::Pad,{0,0},inner,TextRole::Plain),member(ElementType::Pad,{2.54,0},inner,TextRole::Plain),
                                         member(ElementType::Text,{0,2},outer,TextRole::Designator),member(ElementType::Text,{0,4},outer,TextRole::Value)};
                const QString io=sprint::writeTextIO(els);Board b=newBoard("t",50,50);b.elements=sprint::readTextIO(io);const auto cs=components(b);
                require(b.elements.size()==4&&cs.size()==1&&cs[0].members.size()==4,"Text-IO keeps a component whose members sit in different groups");
                for(const auto &e:b.elements)require(e.groups.size()==(e.type==ElementType::Text?outer:inner).size()&&(outer.isEmpty()||e.groups.last()==b.elements[cs[0].designator].groups.last()),
                                                     "Text-IO keeps the groups of a component's members");
                require(blocks(io)==nesting&&sprint::writeTextIO(b.elements)==io,"Text-IO: one block per component, its inner groups inside, written again unchanged");
            }
            // Two components in one group stay in it, one block each.
            QList<Element> two;for(int k=0;k<2;k++){auto pad=member(ElementType::Pad,{5.0*k,0},k?QList<int>{8,3}:QList<int>{3},TextRole::Plain);
                auto id=member(ElementType::Text,{5.0*k,2},{3},TextRole::Designator);pad.part=id.part=k+1;two<<pad<<id;}
            const QString io=sprint::writeTextIO(two);Board b=newBoard("t",50,50);b.elements=sprint::readTextIO(io);
            require(components(b).size()==2&&blocks(io)=="GROUP|   BEGIN_COMPONENT|   END_COMPONENT|   BEGIN_COMPONENT|      GROUP|      END_GROUP|   END_COMPONENT|END_GROUP"
                    &&std::all_of(b.elements.begin(),b.elements.end(),[&](const Element &e){return e.groups.last()==b.elements[0].groups.last();}),"Text-IO: two components in one group");}

        // --- Version 4 (Sprint-Layout 4.0)
        {const auto old=fixtureLayout4();const Document d4=sprint::readLayout(old);const auto &b4=d4.boards[0];
            require(b4.name=="Alt"&&near(b4.width,80)&&near(b4.height,50)&&near(b4.grid,2.54)&&b4.activeLayer==CopperBottom&&b4.groundPlane[CopperBottom]&&!b4.groundPlane[CopperTop],"version 4 board header");
            require(b4.visible[CopperTop]&&b4.visible[CopperBottom]&&!b4.visible[SilkBottom],"version 4 layer visibility");
            require(b4.elements.size()==5,"the two pads of a version 4 via become one");
            const auto &via=b4.elements[0];
            require(via.type==ElementType::Pad&&via.via&&via.layer==CopperBottom&&near(via.pos,{10,10})&&near(via.size,1.6)&&near(via.size2,.8)&&via.points.size()==2&&via.connections==QList<int>{1},"version 4 via");
            require(b4.elements[1].type==ElementType::SmdPad&&near(b4.elements[1].size,2)&&near(b4.elements[1].size2,1)&&b4.elements[1].connections==QList<int>{0},"version 4 SMD pad");
            require(b4.elements[2].name=="GND"&&near(b4.elements[2].width,.5)&&b4.elements[2].groups==QList<int>{4}&&near(b4.elements[2].points[1],{30,20}),"version 4 track");
            require(near(b4.elements[3].start,90)&&near(b4.elements[3].stop,180)&&near(b4.elements[3].size,5)&&near(b4.elements[3].width,1),"version 4 arc in whole degrees");
            const auto &t4=b4.elements[4];
            require(t4.text=="R1"&&near(t4.size,2)&&t4.style==2&&t4.thickness==2&&near(t4.rotation,270)&&t4.mirrored&&t4.strokes.size()==2&&near(t4.strokeWidth,.32),"version 4 text");
            require(b4.elements[0].part==0,"version 4 has no component numbers");
            // The vertical flag at 0x37; the quarter turns are 32 bits, other values than 0 to 3 no turn.
            {const int at=int(old.indexOf(QByteArray("\x02\0\0\0R1",6)))-0x4a-1;auto v4=old;v4[at+0x37]=1;
                const auto t=sprint::readLayout(v4).boards[0].elements[4];require(!t.mirrored&&near(t.rotation,90),"version 4 vertical flag");
                require(sprint::writeLayout(sprint::readLayout(v4),4).mid(at,0x4b)==v4.mid(at,0x4b),"version 4 vertical flag written back");
                v4[at+0x37]=0;v4.replace(at+0x1f,4,le32(0x101));require(near(sprint::readLayout(v4).boards[0].elements[4].rotation,0),"version 4: no quarter turn beyond 3");
                // Written back, an unchanged value beyond 3 stays; a text turned by three more quarters (from 0 to 270, or with
                // the vertical flag from 180 to 90) is written as one quarter turn in all 32 bits.
                for(int vertical:{0,1}){auto w=v4;w[at+0x37]=char(vertical);const Document read=sprint::readLayout(w);require(sprint::writeLayout(read,4).mid(at,0x4b)==w.mid(at,0x4b),"version 4: a quarter turn beyond 3 kept");
                    Document turned=read;auto &e=turned.boards[0].elements[4];e.rotation=std::fmod(e.rotation+270,360);updateStrokes(e);const auto bytes=sprint::writeLayout(turned,4);
                    require(bytes.mid(at+0x1f,4)==le32(1)&&near(sprint::readLayout(bytes).boards[0].elements[4].rotation,e.rotation),"version 4: quarter turns written as 32 bits");}}
            // Written as version 4: the via becomes two pads again, and the file reads back the same.
            int skipped=-1;const auto again=sprint::writeLayout(d4,4,&skipped);
            require(skipped==0&&sprint::fileVersion(again)==4,"version 4 writing");
            require(sprint::readLayout(again).boards[0].elements.size()==5,"version 4 round trip");
            {auto expected=old;for(const char *s:{"Alt"})expected.replace(expected.lastIndexOf(QByteArray(s)+QByteArray(97,'\x5a')),100,QByteArray(s)+QByteArray(97,'\0'));
                expected.replace(expected.size()-4-101*2,101,QByteArray(101,'\0'));expected.replace(expected.size()-4-101,101,QByteArray(101,'\0'));
                require(again==expected,"an unchanged version 4 layout is not written back as read");}
            // The sample: elements on U are left out; turned pads lie across or stand upright.
            const auto old4=sprint::writeLayout(sample_(),4,&skipped);const auto back4=sprint::readLayout(old4);
            require(skipped==1&&back4.boards[0].elements.size()==sample.boards[0].elements.size()&&back4.boards[1].elements.isEmpty(),"version 4 leaves out what it cannot hold");
            // 45° rounds to a quarter turn: the sides swap.
            const auto &smd4=back4.boards[0].elements[1];require(near(smd4.size,.75)&&near(smd4.size2,1.5)&&near(smd4.rotation,0),"version 4 SMD pad turned to the next quarter");}
        // Version 4 keeps templates and origin 8 bytes earlier than version 6, the origin in hundredths of a millimetre.
        {auto f=fixtureLayout4();auto put=[&](int at,qint32 v){f.replace(8+at,4,le32(quint32(v)));};
            f[8+0x4e]=1;f.replace(8+0x118,12,QByteArray("\x0b" "C:\\scan.bmp",12));put(0x1e5,300);put(0x1ed,-50);put(0x1f5,120);put(0x1f9,0x0000ff00);put(0x1fd,0x000000ff);put(0x201,2000);put(0x205,-1500);
            const Document o=sprint::readLayout(f);const auto &b=o.boards[0];const auto &t=b.templates[1];
            require(near(b.origin,{20,15}),"version 4 origin");
            require(t.shown&&t.file=="C:\\scan.bmp"&&near(t.dpi,300)&&near(t.offset,{-5,12})&&t.colour==QColor(255,0,0)&&b.templates[0].colour==QColor(0,255,0)&&!b.templates[0].shown,"version 4 templates");
            auto expected=f;expected.replace(expected.lastIndexOf(QByteArray("Alt")+QByteArray(97,'\x5a')),100,QByteArray("Alt")+QByteArray(97,'\0'));
            expected.replace(expected.size()-4-101*2,101,QByteArray(101,'\0'));expected.replace(expected.size()-4-101,101,QByteArray(101,'\0'));
            require(sprint::writeLayout(o,4)==expected,"version 4 templates and origin written back as read");
            Document m=o;m.boards[0].origin={20,20};m.boards[0].sprintHeader.clear();const auto bytes=sprint::writeLayout(m,4);
            require(bytes.mid(8+0x1f9,16)==QByteArray::fromHex("00ff0000ff000000d007000030f8ffff")&&sprint::readLayout(bytes).boards[0].templates==b.templates,"version 4 templates and origin written");}

        // --- Macros
        {QList<Element> parts=sample.boards[0].elements.mid(0,3);const auto bytes=sprint::writeMacro(parts);
            require(bytes.endsWith(QByteArray(102,'\0'))&&sprint::fileVersion(bytes)==6,"macro framing");
            const auto read=sprint::readMacro(bytes);require(read.size()==3&&read[0].connections==QList<int>{1}&&read[2].name=="Netz 1","macro round trip");
            rejects([&]{sprint::readMacro(bytes.left(40));},"a truncated macro was accepted");
            // Two lines describing the macro in the two short strings at its end, at most 50 characters each.
            require(sprint::readMacroText(bytes)==QStringList({"",""}),"no description yet");
            const auto described=sprint::withMacroText(bytes,"Transistor TO-92",QString(60,'x'));
            require(sprint::readMacroText(described)==QStringList({"Transistor TO-92",QString(50,'x')})&&described.size()==bytes.size()&&sprint::readMacro(described).size()==3,
                    "a description written and read");
            // Component data come along with the designator: package, comment, pick and place.
            auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="T1";id.part=1;id.package="TO-92";id.comment="NPN";id.pickAndPlace=true;updateStrokes(id);
            auto lead=newElement(ElementType::Pad);lead.part=1;updateOutline(lead);
            const auto part=sprint::readMacro(sprint::writeMacro({lead,id}));
            require(part.size()==2&&part[1].role==TextRole::Designator&&part[1].package=="TO-92"&&part[1].comment=="NPN"&&part[1].pickAndPlace,"component data kept in a macro");}
        // A placed macro: with a designator and at most one component number it is one component as a whole; several
        // components stay apart, each with a number the board does not have yet.
        {auto footprint=[](const QString &id){for(const auto &f:footprints())if(f.id==id)return f;return Footprint{};};
            auto put=[](Board &b,QList<Element> els,QPointF at){for(auto &e:els)pcb::move(e,at);b.elements<<els;};
            auto placed=[](const Board &b,const QList<Element> &macro){Board out=b;out.elements<<placeable(macroComponent(macro),b);return out;};
            auto listed=[](const Board &b){QStringList out;for(const auto &c:components(b))out<<QString("%1:%2:%3").arg(b.elements[c.designator].text).arg(c.members.size()).arg(c.pins.join(','));return out.join(' ');};
            Board board=newBoard("Makro",80,60);put(board,placeable(footprint("res-0207-10"),board),{20,20});put(board,placeable(footprint("cap-254"),board),{50,20});
            // Saved as the editor saves a selection: without groups, with the component numbers.
            QList<Element> selection=board.elements;for(auto &e:selection)e.groups.clear();const auto two=sprint::readMacro(sprint::writeMacro(selection));
            // The resistor's empty value text is not written: the reference deletes a text without strokes when it reads it.
            require(listed(board)=="C1:5:1,2 R1:7:1,2"&&listed(placed(newBoard("leer",80,60),two))=="C1:5:1,2 R1:6:1,2","a macro with two components keeps them apart");
            {const Board again=placed(board,two);QSet<int> before,copies;for(int i=0;i<again.elements.size();i++)(i<board.elements.size()?before:copies)<<again.elements[i].part;
                require(listed(again)=="C1:5:1,2 C2:5:1,2 R1:7:1,2 R2:6:1,2"&&copies.size()==2&&!copies.contains(0)&&!copies.intersects(before),"the macro placed on its own board: new numbers and designators");}
            // One component with a loose silkscreen line: the line joins it. Without any numbers the whole macro is one.
            QList<Element> one;for(const auto &e:two)if(e.part==two.first().part)one<<e;auto line=newElement(ElementType::Track);line.layer=SilkTop;line.points={{-3,3},{3,3}};one<<line;
            {const Board b=placed(newBoard("leer",80,60),one);require(listed(b)=="R1:7:1,2"&&b.elements.last().part==b.elements.first().part,"a component with a loose line: one component");}
            QList<Element> bare=two;for(auto &e:bare)e.part=0;require(listed(placed(newBoard("leer",80,60),bare))=="C1:11:1,2","a macro without numbers: one component");
            // As many vendor macros: pad and outline in a group, the texts in none. One component, also after Text-IO.
            QList<Element> vendor;for(const auto &e:footprint("cap-254").elements)if(e.type!=ElementType::Pad||e.name=="1")vendor<<e;
            for(auto &e:vendor)if(e.type!=ElementType::Text)e.groups={1};
            {Board b=placed(newBoard("leer",80,60),sprint::readMacro(sprint::writeMacro(vendor)));require(listed(b)=="C1:4:1"&&b.elements.last().groups.isEmpty(),"a vendor macro: one component");
                b.elements=sprint::readTextIO(sprint::writeTextIO(b.elements));require(listed(b)=="C1:4:","a vendor macro after Text-IO: one component");}}

        // --- Autorouted tracks: the mark at 0x31 of the track record and, after the airwires of the board, two element
        // indexes per marked track for the pads whose airwire it replaced.
        {using Pads=std::array<int,2>;const auto marked=routedLayout(1,{0,1});const auto d=sprint::readLayout(marked);const auto &t=d.boards[0].elements[2];
            require(t.autorouted&&t.autoroutePads==Pads{0,1}&&!sprint::readLayout(routedLayout(0,{})).boards[0].elements[2].autorouted,"autoroute mark and pads");
            // Before, the indexes were taken for the project data at the end: the title came out as NUL characters.
            require(d.title=="Titel"&&d.activeBoard==0&&d.author.isEmpty(),"the project data after the pads of autorouted tracks");
            require(sprint::writeLayout(d)==marked&&sprint::writeLayout(sprint::readLayout(routedLayout(0,{})))==routedLayout(0,{}),"autorouted tracks written back as read");
            // Any value other than 0 marks; it is kept as long as the track stays autorouted.
            {const auto other=routedLayout(0x80,{0,1});require(sprint::readLayout(other).boards[0].elements[2].autorouted&&sprint::writeLayout(sprint::readLayout(other))==other,"a mark of 0x80 kept");}
            // Negative indexes name no pad (written as -1), an index of an element that is no pad neither; one beyond the
            // elements, or a mark without its indexes (the file then ends early), is refused.
            {const auto none=routedLayout(1,{-1,-1});const auto n=sprint::readLayout(none);require(n.boards[0].elements[2].autoroutePads==Pads{-1,-1}&&sprint::writeLayout(n)==none,"an autorouted track without pads");
                require(sprint::readLayout(routedLayout(1,{-100,1})).boards[0].elements[2].autoroutePads==Pads{-1,1}&&sprint::writeLayout(sprint::readLayout(routedLayout(1,{-100,1})))==routedLayout(1,{-1,1}),"any negative index is none");
                require(sprint::readLayout(routedLayout(1,{0,2})).boards[0].elements[2].autoroutePads==Pads{0,-1},"an index of a track is none");
                rejects([]{sprint::readLayout(routedLayout(1,{0,3}));},"an index beyond the elements was accepted");
                rejects([]{sprint::readLayout(routedLayout(1,{}));},"a mark without its pads was accepted");}
            // The indexes count all elements of their own board; each board has its own block (before, a second board
            // was refused).
            {const auto first=routedLayout(1,{1,2},true);require(sprint::readLayout(first).boards[0].elements[0].autoroutePads==Pads{1,2}&&sprint::writeLayout(sprint::readLayout(first))==first,"the track before its pads");
                const auto two=routedLayout(1,{0,1},false,2);const auto d2=sprint::readLayout(two);
                require(d2.boards.size()==2&&d2.boards[1].name=="Zweite"&&d2.activeBoard==1&&d2.boards[1].elements[2].autoroutePads==Pads{0,1}&&sprint::writeLayout(d2)==two,"two boards with autorouted tracks");}
            // Version 5 has the mark and the pads as version 6 (the same board in hundredths of a millimetre is written as
            // the version 6 one).
            {const auto d5=sprint::readLayout(routedLayout(1,{0,1},false,1,5));const auto again=sprint::writeLayout(d5);
                require(d5.boards[0].elements[2].autoroutePads==Pads{0,1}&&again==marked,"version 5 autorouted tracks");}
            // Version 4 keeps the mark at 0x35 and counts the records before the two pads of a via are joined: either of
            // them is the via. Written, the via is its first pad.
            {const auto four=routedLayout4({0,2});const auto d4=sprint::readLayout(four);const auto &t4=d4.boards[0].elements[2];
                require(d4.boards[0].elements.size()==3&&t4.autorouted&&t4.autoroutePads==Pads{0,1}&&sprint::writeLayout(d4,4)==four,"version 4 autorouted track");
                require(sprint::readLayout(routedLayout4({1,2})).boards[0].elements[2].autoroutePads==Pads{0,1}&&sprint::writeLayout(sprint::readLayout(routedLayout4({1,2})),4)==four,"version 4: the second pad of a via");}
            // Written from the model: indexes of the elements as written, after leaving out what version 4 cannot hold
            // and splitting vias; the strokes of texts never carry the mark.
            {Document m;Board b=newBoard("Neu",40,30);
                auto inner=newElement(ElementType::Track);inner.layer=Inner1;inner.points={{1,1},{5,1}};b.elements<<inner;
                auto via=newElement(ElementType::Pad);via.pos={10,10};via.via=true;updateOutline(via);auto pad=newElement(ElementType::Pad);pad.pos={30,10};updateOutline(pad);
                auto routed=newElement(ElementType::Track);routed.points={{10,10},{30,10}};routed.autorouted=true;routed.autoroutePads={1,2};
                auto lost=routed;lost.autoroutePads={0,2};    // a link to a track counts as none
                b.elements<<via<<pad<<routed<<lost;m.boards={b};
                const int end=311;   // the project data: board shown, three strings of 101 bytes, empty comment
                const auto six=sprint::writeLayout(m);require(six.mid(six.size()-end-16,16)==le32(1)+le32(2)+le32(quint32(-1))+le32(2),"pads written as element indexes");
                const auto back=sprint::readLayout(six).boards[0].elements;require(back[3].autoroutePads==Pads{1,2}&&back[4].autoroutePads==Pads{-1,2},"pads read back");
                const auto old=sprint::writeLayout(m,4);require(old.mid(old.size()-end-16,16)==le32(0)+le32(2)+le32(quint32(-1))+le32(2),"version 4: indexes of the written elements");
                require(sprint::readLayout(old).boards[0].elements[2].autoroutePads==Pads{0,1},"version 4 read back");
                Document text;Board tb=newBoard("Text",20,20);auto label=newElement(ElementType::Text);label.text="I";updateStrokes(label);tb.elements<<label;text.boards={tb};
                auto bytes=sprint::writeLayout(text);const int stroke=8+0x212+4+1+0x4c+4+1+4+4+4;
                require(bytes[stroke]==6&&bytes[stroke+0x31]==0,"text strokes without the mark");bytes[stroke+0x31]=1;
                const auto read=sprint::readLayout(bytes);require(read.title.isEmpty()&&read.boards[0].elements.size()==1&&sprint::writeLayout(read)[stroke+0x31]==0,"a mark on a stroke takes no pads and is not written");}
            // Macros: the pads between the airwires and the closing bytes.
            {const auto els=sprint::readLayout(marked).boards[0].elements;const auto bytes=sprint::writeMacro(els);
                require(bytes.mid(bytes.size()-102-8,8)==le32(0)+le32(1)&&sprint::readMacro(bytes)[2].autoroutePads==Pads{0,1},"macro with an autorouted track");}}
        // --- Files older than Sprint-Layout 4.0, converted as Sprint-Layout 6 converts them
        {// Version 3: copper layers 1 and 3 change places, old texts become stroke texts after all other elements.
            OldRec a{2,1,1000,-1000,100,40};a.clearance=30;a.groups={5};a.wires={1,77,4};     // round pad; airwires to B, to nothing and to the second pad of the via
            OldRec b{2,3,2500,-1000,125,50};b.shape=3;b.groups={5};b.wires={0};              // square pad in the same group
            OldRec text{3,2,500,-300,0,0,-400};text.text="AB12";text.clearance=150;           // 2.7 mm high, the clearance is not taken
            OldRec via1{2,3,4000,-1000,90,35};via1.partner=4;via1.wires={4};OldRec via2{2,1,4000,-1000,150,35};via2.shape=2;via2.partner=3;via2.wires={0};
            OldRec track{6,1,0,0,0,0,50};track.points={{500,-2500},{2000,-2500},{2000,-3500}};track.groups={70000};
            OldRec area{4,3,0,0,0,0,20};area.points={{3000,-2500},{4500,-2500},{4500,-3500},{3000,-3500}};
            OldRec arc{5,2,5200,-3000,400,380,180,0};                                         // 0° to 180° in whole degrees
            OldRec line{1,3,1000,-3500,3000,-3900,40};                                        // a line from (10, 35) to (30, 39) mm
            OldRec seven{7,2},none{0,2},nine{9,2};OldRec dot{6,1,0,0,0,0,50};dot.points={{100,-100}};
            OldRec smd{8,1,5200,-1000,200,100};
            QList<OldRec> turned;for(int k=1;k<4;k++){OldRec t{3,4,1000+1000*k,-2000,0,0,-400,90*k};t.text="9";turned<<t;}
            OldRec blank{3,4,100,-100,0,0,-400};blank.text="  ";
            OldRec mixed{5,2,5200,-1500,200,180,180000,900};mixed.filled=true;                 // one angle above 1000: both in thousandths
            OldRec far{2,3,10000001,-1000,100,40};                                            // beyond 100 m
            OldRec silk1{2,2,1000,-1500,100,40};silk1.partner=21;OldRec silk2{2,4,1000,-1500,100,40};silk2.partner=20;
            Bytes o;o.b=QByteArray::fromHex("0333aaff");o.u32(2);
            QByteArray h=oldBoard("Alt3",6000,4000,1,0,127,1);h[77]=1;h[79]=char(17);h.replace(80,17,"C:\\Scans\\oben.bmp");qToLittleEndian<qint32>(300,h.data()+481);
            // Template offsets in tenths of a millimetre (x top, x bottom, y top, y bottom; y down) and colours (0x00bbggrr).
            for(const auto &[at,v]:QList<std::pair<int,quint32>>{{489,123},{493,quint32(-12)},{497,77},{501,quint32(-50)},{505,0x00336699},{509,0x00ff0000}})qToLittleEndian<quint32>(v,h.data()+at);
            o.b+=h+oldRecords({a,b,text,via1,via2,track,area,arc,line,seven,none,dot,smd,turned[0],turned[1],turned[2],blank,mixed,far,nine,silk1,silk2},3);
            OldRec other{6,1,0,0,0,0,30};other.points={{0,-1000},{5000,-1000}};
            o.b+=oldBoard("Zwei",5000,3000,2,0,254,9)+oldRecords({other},3);o.u32(1);o.b+=QByteArray::fromHex("e803000018fcffff");
            const QByteArray layout3=o.b;
            QStringList notes;const Document d3=sprint::readLayout(layout3,&notes);
            require(d3.boards.size()==2&&d3.activeBoard==1&&d3.title.isEmpty(),"version 3 boards");
            const Board &b3=d3.boards[0];const auto &e=b3.elements;
            require(b3.name=="Alt3"&&near(b3.width,60)&&near(b3.height,40)&&near(b3.grid,1.27)&&b3.activeLayer==CopperTop&&near(b3.origin,{0,0})&&!b3.multilayer&&b3.sprintHeader.isEmpty(),"version 3 board header");
            require(b3.groundPlane[CopperBottom]&&!b3.groundPlane[CopperTop]&&std::all_of(b3.visible.begin()+1,b3.visible.end(),[](bool v){return v;}),"version 3 ground planes swapped, all layers visible");
            require(b3.templates[0].shown&&b3.templates[0].file=="C:\\Scans\\oben.bmp"&&near(b3.templates[0].dpi,300)&&!b3.templates[1].shown&&near(b3.templates[1].dpi,600),"version 3 templates");
            require(near(b3.templates[0].offset,{12.3,7.7})&&near(b3.templates[1].offset,{-1.2,-5})&&b3.templates[0].colour==QColor(0x99,0x66,0x33)&&b3.templates[1].colour==QColor(0,0,255),
                    "version 3 template offsets and colours");
            require(e.size()==15,"version 3 element count");
            require(e[0].type==ElementType::Pad&&e[0].layer==CopperBottom&&near(e[0].pos,{10,10})&&near(e[0].size,2)&&near(e[0].size2,.8)&&e[0].shape==PadShape::Round&&!e[0].via,"version 3 pad");
            require(e[0].solderMask&&!e[0].thermal&&e[0].thermalSpokes==0x55&&e[0].thermalWidth==100&&e[0].points.size()==2&&near(e[0].clearance,.3)&&e[0].sprint.record.isEmpty(),"version 3 pad defaults");
            require(e[1].layer==CopperTop&&e[1].shape==PadShape::Square&&near(e[1].size,2.5)&&!e[0].groups.isEmpty()&&e[1].groups==e[0].groups&&e[0].groups[0]>0,"version 3 group");
            require(e[2].via&&e[2].layer==CopperTop&&near(e[2].size,1.8)&&e[2].shape==PadShape::Round,"the first pad of a version 3 pair stays, through-plated");
            // The airwire from A to the second pad of the via goes to the first; the one of the via to its own twin is dropped.
            require(e[0].connections==QList<int>({1,2})&&e[1].connections==QList<int>{0}&&e[2].connections==QList<int>{0},"version 3 airwires: those of the second pad and those to it on the first");
            require(e[3].type==ElementType::Track&&e[3].layer==CopperBottom&&near(e[3].width,.5)&&e[3].points.size()==3&&near(e[3].points[2],{20,35})&&e[3].groups.size()==1&&e[3].groups!=e[0].groups,"version 3 track");
            require(e[4].type==ElementType::Area&&e[4].layer==CopperTop&&near(e[4].width,.2)&&e[4].points.size()==4&&!e[4].hatched&&e[4].hatchAuto,"version 3 area");
            require(e[5].type==ElementType::Circle&&e[5].layer==SilkTop&&near(e[5].pos,{52,30})&&near(e[5].size,3.9)&&near(e[5].width,.2)&&near(e[5].start,0)&&near(e[5].stop,180)&&!e[5].filled,"version 3 arc");
            require(e[6].type==ElementType::Track&&e[6].layer==CopperTop&&near(e[6].width,.4)&&e[6].points==QPolygonF({QPointF(10,35),QPointF(30,39)}),"a line becomes a track");
            require(e[7].type==ElementType::SmdPad&&e[7].layer==CopperBottom&&near(e[7].size,2)&&near(e[7].size2,1)&&e[7].points.size()==4&&e[7].connections.isEmpty(),"version 3 SMD pad");
            require(near(e[8].start,.9)&&near(e[8].stop,180)&&e[8].filled,"version 3 arc angles in thousandths");
            require(e[9].type==ElementType::Pad&&near(e[9].pos,{0,10}),"lengths beyond 100 m count as 0");
            require(e[10].layer==SilkTop&&!e[10].via,"a pair on the silkscreen is no via");
            // Texts: height from the font size, thick and normal; the box around the strokes, 0.4 mm larger, has the corner
            // that was top left before turning on the stored point.
            auto corner=[](const Element &t,int quarter){const QRectF box=bounds(t).adjusted(-.4,-.4,.4,.4);return quarter==0?box.topLeft():quarter==1?box.topRight():quarter==2?box.bottomRight():box.bottomLeft();};
            require(e[11].type==ElementType::Text&&e[11].text=="AB12"&&e[11].layer==SilkTop&&near(e[11].size,2.7)&&e[11].thickness==2&&e[11].style==1&&!e[11].mirrored&&e[11].role==TextRole::Plain,"old text");
            require(near(e[11].clearance,.4)&&!e[11].strokes.isEmpty()&&near(e[11].rotation,0)&&near(corner(e[11],0),{5,3}),"old text placed by its top left corner");
            for(int k=1;k<4;k++)require(e[11+k].text=="9"&&e[11+k].layer==SilkBottom&&near(e[11+k].rotation,360-90*k)&&near(corner(e[11+k],k),{10.0+10*k,20}),"old text turned clockwise, placed by the turned corner");
            require(notes.size()==4&&notes[1].startsWith("4 ")&&notes[2].startsWith("2 ")&&notes[3].startsWith("1 "),"notes of a version 3 file");
            const Board &c3=d3.boards[1];
            require(c3.name=="Zwei"&&c3.groundPlane[CopperTop]&&c3.groundPlane[CopperBottom]&&near(c3.grid,2.54)&&c3.activeLayer==CopperBottom&&c3.elements.size()==1&&c3.elements[0].layer==CopperBottom,"second version 3 board");
            // Written as version 6 and read again, nothing is swapped twice; the own format keeps it all.
            {const auto written=sprint::writeLayout(d3);const Document again=sprint::readLayout(written);
                require(sprint::fileVersion(written)==6&&again.boards[0].elements.size()==e.size()&&again.boards[0].templates==b3.templates,"version 3 written as version 6");
                for(int k=0;k<e.size();k++)require(again.boards[0].elements[k].layer==e[k].layer&&again.boards[0].elements[k].via==e[k].via&&again.boards[0].elements[k].groups==e[k].groups,"version 3 layers written as read");
                Document own=d3;assignIds(own);require(decode(encode(own))==own,"version 3 document in the own format");}
            // Broken files are refused.
            for(int cut:{6,100,600,int(layout3.size())-12,int(layout3.size())-1})rejects([&]{sprint::readLayout(layout3.left(cut));},"a truncated version 3 layout was accepted");
            {Bytes bad;bad.b=QByteArray::fromHex("0333aaff");bad.u32(1);OldRec wrong{2,9,0,0,100,40};bad.b+=oldBoard("X",1000,1000,0,0,127,1)+oldRecords({wrong},3);bad.u32(0);bad.u32(0);bad.u32(0);
                rejects([&]{sprint::readLayout(bad.b);},"a version 3 layer outside 1..7 was accepted");}
            {Bytes bad;bad.b=QByteArray::fromHex("0333aaff");bad.u32(1);bad.b+=oldBoard("X",0,1000,0,0,127,1)+oldRecords({},3);bad.u32(0);bad.u32(0);bad.u32(0);
                rejects([&]{sprint::readLayout(bad.b);},"a version 3 board without width was accepted");}
            rejects([]{sprint::readLayout(QByteArray::fromHex("0333aaff00000000"));},"a version 3 layout without boards was accepted");
            // Versions 7 to 11 come from newer Sprint-Layout versions; from 12 on the bytes are no Sprint-Layout file.
            require(sprint::fileVersion(QByteArray::fromHex("0733aaff"))==7&&sprint::fileVersion(QByteArray::fromHex("0c33aaff"))<0&&sprint::fileVersion(QByteArray::fromHex("0033aafe"))<0,"newer versions recognised");
            for(int v=7;v<=11;v++){auto newer=layout3;newer[0]=char(v);bool told=false;
                try{sprint::readLayout(newer);}catch(const FormatError &error){told=QString::fromUtf8(error.what()).contains(QStringLiteral("(Version %1)").arg(v));}
                require(told,"a file of a newer version is refused with its version");}
            {auto twelve=layout3;twelve[0]=12;rejects([&]{sprint::readLayout(twelve);},"version 12 was read");twelve[0]=9;rejects([&]{sprint::readMacro(twelve);},"a version 9 macro was read");
                // Opened as a file, version 12 is named as no Sprint-Layout file (not taken for a broken own file).
                twelve[0]=12;QTemporaryDir dir;const auto path=dir.filePath("zwoelf.lay");{QFile f(path);require(f.open(QIODevice::WriteOnly)&&f.write(twelve)==twelve.size(),"cannot write the test file");}
                QString message;try{load(path);}catch(const FormatError &error){message=QString::fromUtf8(error.what());}
                require(message.contains(ui("Keine Sprint-Layout-Datei")),"version 12 named as no Sprint-Layout file");}
            // A version 3 pad that names itself as its partner is removed with its airwires, as Sprint-Layout 6 removes it.
            {OldRec self{2,1,1000,-1000,100,40};self.partner=0;self.wires={1};OldRec other{2,1,3000,-1000,100,40};other.wires={0};
                Bytes o;o.b=QByteArray::fromHex("0333aaff");o.u32(1);o.b+=oldBoard("Selbst",5000,3000,0,0,127,1)+oldRecords({self,other},3);o.u32(0);o.b+=QByteArray(8,'\0');
                const auto e=sprint::readLayout(o.b).boards[0].elements;require(e.size()==1&&e[0].connections.isEmpty()&&near(e[0].pos,{30,10}),"a version 3 pad naming itself is removed");}}
        {// Versions 0 to 2: one board of the default name with its origin at the bottom left corner, 16-bit numbers.
            OldRec pad{2,3,800,-1500,90,35};pad.shape=2;pad.groups={-7};
            OldRec smd{8,1,2000,-1500,150,60};smd.groups={-7};
            OldRec track{6,1,0,0,0,0,60};track.points={{500,-3000},{2000,-3500}};
            OldRec ring{5,2,4500,-2500,300,250,180,90};                                       // the angles are not read: a full ring
            OldRec text{3,4,3000,-800,0,0,-350};text.text="V2";OldRec big{3,4,100,-3900,0,0,-10000};big.text="x";OldRec tiny{3,4,100,-3900,0,0,-5};tiny.text="y";
            OldRec line{1,3,1000,-2000,4000,-3800,40};
            auto file=[](int version,const QList<OldRec> &recs){Bytes o;o.b=QByteArray::fromHex("0033aaff");o.b[0]=char(version);o.u32(6000);o.u32(4000);return o.b+oldRecords(recs,version);};
            const auto layout2=file(2,{pad,smd,track,ring,text,big,tiny,line});
            QStringList notes;const Document d2=sprint::readLayout(layout2,&notes);const Board &b2=d2.boards[0];const auto &e=b2.elements;
            require(d2.boards.size()==1&&b2.name==ui("Platine 1")&&near(b2.width,60)&&near(b2.height,40)&&near(b2.origin,{0,40})&&near(b2.grid,1.27)&&b2.activeLayer==CopperBottom,"version 2 board");
            require(std::none_of(b2.groundPlane.begin(),b2.groundPlane.end(),[](bool g){return g;})&&std::all_of(b2.visible.begin()+1,b2.visible.end(),[](bool v){return v;}),"version 2 board defaults");
            require(e.size()==8&&e[0].type==ElementType::Pad&&e[0].layer==CopperTop&&e[0].shape==PadShape::Octagon&&near(e[0].pos,{8,15})&&near(e[0].size,1.8),"version 2 pad");
            require(e[1].type==ElementType::SmdPad&&e[1].layer==CopperBottom&&near(e[1].size,1.5)&&near(e[1].size2,.6)&&e[0].groups.size()==1&&e[0].groups==e[1].groups&&e[0].groups[0]>0,"version 2 group with a negative number");
            require(e[2].type==ElementType::Track&&e[2].layer==CopperBottom&&near(e[2].width,.6)&&near(e[2].points[1],{20,35}),"version 2 track");
            require(e[3].type==ElementType::Circle&&near(e[3].start,0)&&near(e[3].stop,0)&&!e[3].filled&&near(e[3].size,2.75)&&near(e[3].width,.5),"version 2 full ring");
            require(e[4].type==ElementType::Track&&e[4].layer==CopperTop&&near(e[4].points[1],{40,38}),"version 2 line");
            require(std::all_of(e.begin(),e.end(),[](const Element &x){return near(x.clearance,.4);}),"versions 0 to 2 keep 0.4 mm to the ground plane");
            require(e[5].text=="V2"&&near(e[5].size,2.4)&&near(e[6].size,50)&&near(e[7].size,.1),"old text heights to 0.1 mm, at most 50 mm and at least 0.1 mm");
            require(notes.size()==2&&notes[1].startsWith("3 "),"notes of a version 2 file");
            {Document own=d2;assignIds(own);require(decode(encode(own))==own,"version 2 document in the own format");}
            // Version 1 has no group lists: elements with the same key form a group. Version 0 has neither.
            OldRec k1{2,3,1000,-1500,200,50},k2{2,3,3000,-1500,200,50},k3{2,3,5000,-1500,200,50};k1.key=9;k2.key=9;k3.key=4;
            const auto e1=sprint::readLayout(file(1,{k1,k2,k3})).boards[0].elements;
            require(e1.size()==3&&e1[0].groups.isEmpty()&&e1[1].groups.isEmpty()&&e1[2].groups.isEmpty(),"version 1 keys make no group");
            const auto layout0=file(0,{pad,track});
            require(sprint::fileVersion(layout0)==0&&sprint::readLayout(layout0).boards[0].elements.size()==2&&sprint::readLayout(layout0).boards[0].elements[0].groups.isEmpty(),"version 0");
            for(int cut:{10,13,30,int(layout2.size())-1})rejects([&]{sprint::readLayout(layout2.left(cut));},"a truncated version 2 layout was accepted");
            {OldRec wrong=pad;wrong.layer=8;rejects([&]{sprint::readLayout(file(2,{wrong}));},"a version 2 layer outside 1..7 was accepted");}
            // Macros of these versions: the elements alone; old macros keep pads on layer 5 and the silkscreen on 6.
            OldRec p1{2,5,0,0,90,35};p1.wires={1};OldRec p2{2,1,254,0,90,35};p2.wires={0};OldRec outline{6,6,0,0,0,0,20};outline.points={{-200,200},{454,200}};
            OldRec label{3,6,0,500,0,0,-300};label.text="J1";
            QStringList macroNotes;const auto m3=sprint::readMacro(QByteArray::fromHex("0333aaff")+oldRecords({p1,p2,outline,label},3),&macroNotes);
            require(m3.size()==4&&m3[0].layer==CopperBottom&&m3[1].layer==CopperBottom&&m3[2].layer==SilkTop&&m3[3].type==ElementType::Text&&m3[3].layer==SilkTop,"version 3 macro layers");
            require(m3[0].connections==QList<int>{1}&&m3[1].connections==QList<int>{0}&&macroNotes.size()==1&&macroNotes[0].startsWith("1 "),"version 3 macro airwires and notes");
            const auto m2=sprint::readMacro(QByteArray::fromHex("0233aaff")+oldRecords({p1,outline},2));
            require(m2.size()==2&&m2[0].layer==CopperBottom&&m2[1].layer==SilkTop&&m2[0].connections.isEmpty(),"version 2 macro");
            rejects([&]{sprint::readMacro(QByteArray::fromHex("0233aaff")+oldRecords({p1,outline},2).left(30));},"a truncated version 2 macro was accepted");}

        // Texts without strokes (an own-format file may leave them out) are written with the strokes of the font; an empty
        // text is not written, as the reference deletes texts without strokes when it reads them.
        {Document t;Board tb=newBoard("Striche",30,20);auto bare=newElement(ElementType::Text);bare.text="AB";bare.pos={5,10};auto empty=bare;empty.text="";
            tb.elements={bare,empty};t.boards={tb};require(bare.strokes.isEmpty(),"a text without strokes");
            const auto back=sprint::readLayout(sprint::writeLayout(t)).boards[0].elements;
            require(back.size()==1&&back[0].text=="AB"&&!back[0].strokes.isEmpty(),"texts written with strokes, empty ones left out");
            const auto old=sprint::readLayout(sprint::writeLayout(t,4)).boards[0].elements;require(old.size()==1&&!old[0].strokes.isEmpty(),"also in version 4");}
        // --- Own format
        // A document as the editor holds it: with identifiers for boards and components.
        {Document own=sample;assignIds(own);
            require(own.boards[0].id.size()==32&&own.boards[0].id!=own.boards[1].id&&QRegularExpression("^[0-9a-f]{32}$").match(own.boards[0].id).hasMatch(),"board identifiers");
            const auto json=encode(own);const auto again=decode(json);require(again==own,"own format round trip changed the document");
            QTemporaryDir dir;const auto path=dir.filePath("probe.olpcb");save(own,path);require(load(path)==own,"disk round trip changed the document");
            const auto lay=dir.filePath("probe.lay6");{QFile f(lay);require(f.open(QIODevice::WriteOnly)&&f.write(written)==written.size(),"cannot write the test file");}require(load(lay).boards.size()==2,"a Sprint-Layout file is not opened by content");
            auto o=QJsonDocument::fromJson(json).object();
            auto broken=[&](const std::function<void(QJsonObject&)> &change,const char *message){auto c=o;change(c);rejects([&]{fromJson(c);},message);};
            broken([](QJsonObject &c){c["format"]="Other";},"foreign format accepted");
            broken([](QJsonObject &c){c["version"]=5;},"unknown version accepted");
            broken([](QJsonObject &c){c["version"]="2";},"version as text accepted");
            // Versions 1 and 2 lack the newer fields; they read with their defaults and new identifiers.
            {require(o["version"]==4,"files are written as version 4");auto one=o;one["version"]=1;require(fromJson(one)==own,"version 1 still reads");
                auto two=o;two["version"]=2;auto bs=two["boards"].toArray();auto bo=bs[0].toObject();bo.remove("id");bs[0]=bo;two["boards"]=bs;
                const auto migrated=fromJson(two);require(migrated.boards[0].id.size()==32&&migrated.boards[0].id!=own.boards[0].id&&migrated.boards[1].id==own.boards[1].id,"identifiers come new where they are missing");
                // Before version 4 a component was the innermost group of its designator: its members get one number.
                auto three=o;three["version"]=3;auto b3=three["boards"].toArray();auto o3=b3[0].toObject();auto e3=o3["elements"].toArray();
                for(int k=0;k<e3.size();k++){auto e=e3[k].toObject();e.remove("part");if(k==2)e["groups"]=QJsonArray{4,1};e3[k]=e;}
                o3["elements"]=e3;b3[0]=o3;three["boards"]=b3;const auto old3=fromJson(three).boards[0];
                require(old3.elements[0].part==1&&old3.elements[6].part==1&&old3.elements[2].part==1&&old3.elements[1].part==0&&components(old3).size()==1,"version 3 components by groups");
                auto four=o;auto b4=four["boards"].toArray();auto o4=b4[0].toObject();auto e4=o4["elements"].toArray();auto e=e4[2].toObject();e["groups"]=QJsonArray{4,1};e4[2]=e;
                o4["elements"]=e4;b4[0]=o4;four["boards"]=b4;require(fromJson(four).boards[0].elements[2].part==0,"version 4: groups make no components");}
            // An identifier used twice in a document is renewed where it comes again: a board copied as a whole is a new
            // board with new components.
            {Document twice=own;twice.boards[1]=twice.boards[0];assignIds(twice);const auto first=components(own.boards[0]),second=components(twice.boards[1]);
                bool fresh=second.size()==first.size()&&!first.isEmpty();for(int i=0;fresh&&i<first.size();i++)fresh=second[i].id.size()==32&&second[i].id!=first[i].id;
                require(twice.boards[0]==own.boards[0]&&twice.boards[1].id!=own.boards[0].id&&twice.boards[1].id.size()==32&&fresh,"identifiers used twice are renewed");}
            // The boards of a Sprint-Layout file get identifiers when it is opened.
            require(load(lay).boards[0].id.size()==32,"identifiers for a Sprint-Layout file");
            // Template colours as #rrggbb; offsets and colours stay with a board also without a picture.
            {Document c=own;c.boards[1].templates[0].colour=QColor(0x12,0x34,0x56);c.boards[1].templates[1].offset={1.5,-2};const auto j=encode(c);
                const auto list=QJsonDocument::fromJson(j).object()["boards"].toArray()[1].toObject()["templates"].toArray();
                require(decode(j)==c&&list.size()==2&&list[0].toObject()["colour"]=="#123456","template colour and offset in the own format");
                broken([](QJsonObject &c){auto bs=c["boards"].toArray();auto bo=bs[0].toObject();auto ts=bo["templates"].toArray();auto t=ts[1].toObject();
                    t["colour"]="lime";ts[1]=t;bo["templates"]=ts;bs[0]=bo;c["boards"]=bs;},"template colour other than #rrggbb accepted");}
            // Solder mask openings: no copper, written and read back, as Text-IO SOLDERMASK_CUTOUT.
            {Document m=own;auto a=newElement(ElementType::Area);a.layer=CopperBottom;a.points={{1,1},{4,1},{4,3}};a.maskOnly=true;m.boards[0].elements<<a;
                require(decode(encode(m))==m&&copperOn(a,CopperBottom,m.boards[0]).isEmpty(),"solder mask opening in the own format, without copper");
                const auto text=sprint::writeTextIO({a});require(text.contains("SOLDERMASK_CUTOUT=true")&&sprint::readTextIO(text).value(0).maskOnly,"solder mask opening in Text-IO");
                int left=0;const auto back=sprint::readLayout(sprint::writeLayout(m,6,&left)).boards[0].elements;
                require(left==0&&back.size()==m.boards[0].elements.size()&&back.last().maskOnly&&back.last().points==a.points,"Sprint-Layout 6 keeps solder mask openings");
                int beyond=0;for(const auto &b:m.boards)for(const auto &e:b.elements)beyond+=e.layer>SilkBottom;
                int left4=0;sprint::writeLayout(m,4,&left4);require(left4==beyond+1,"Sprint-Layout 4.0 leaves out solder mask openings");
                // An opening before a via is left out before the two pads of the via are numbered: they name each other as
                // written, and an autorouted track still names the via.
                {Document v;Board vb=newBoard("v",20,20);auto through=newElement(ElementType::Pad);through.pos={5,10};through.via=true;updateOutline(through);
                    auto pad=newElement(ElementType::Pad);pad.pos={15,10};updateOutline(pad);auto routed=newElement(ElementType::Track);routed.points={{5,10},{15,10}};routed.autorouted=true;routed.autoroutePads={1,2};
                    vb.elements={a,through,pad,routed};v.boards={vb};int l=0;const auto e=sprint::readLayout(sprint::writeLayout(v,4,&l)).boards[0].elements;
                    require(l==1&&e.size()==3&&e[0].via&&!e[1].via&&e[2].autoroutePads==std::array<int,2>{0,1},"version 4: a via after a left-out solder mask opening");}
                // Version 6 marks an opening on a copper side, and its stored point count is one more than its points.
                Document plain;plain.boards={newBoard("x",20,20)};
                auto z=newElement(ElementType::Area);z.layer=CopperBottom;z.points={{1,1},{4,1},{4,3},{1,3}};z.width=.3;z.maskOnly=true;
                auto t=newElement(ElementType::Track);t.layer=CopperBottom;t.points={{5,5},{9,5}};t.width=.5;
                auto pad=newElement(ElementType::Pad);pad.layer=CopperBottom;pad.pos={12,12};updateOutline(pad);
                plain.boards[0].elements={z,t,pad};
                const auto areaAt=[](const QByteArray &bytes,int layer){
                    for(int i=0;i+80<bytes.size();i++)if(bytes[i]==4&&bytes[i+0x16]==layer&&qFromLittleEndian<qint32>(bytes.constData()+i+0x11)==3000)return i;return -1;};
                const QByteArray marked=sprint::writeLayout(plain);const int at=areaAt(marked,CopperBottom),count=at+1+0x4c+12;
                require(at>0&&marked[at+0x4a]==1&&qFromLittleEndian<quint32>(marked.constData()+count)==5,"an opening is marked, its point count one more");
                {const auto e=sprint::readLayout(marked).boards[0].elements;
                    require(e.size()==3&&e[0].maskOnly&&e[0].points==z.points&&e[1].type==ElementType::Track&&e[1].points==t.points&&e[2].type==ElementType::Pad&&e[2].pos==pad.pos,
                            "an opening read with its points, the elements after it intact");
                    require(sprint::writeLayout(sprint::readLayout(marked))==marked,"an opening written back unchanged");}
                // As the last element, where a wrong count would run past the end.
                {Document last=plain;last.boards[0].elements={t,z};const auto bytes=sprint::writeLayout(last);const auto e=sprint::readLayout(bytes).boards[0].elements;
                    require(e.size()==2&&e[1].maskOnly&&e[1].points==z.points&&sprint::writeLayout(sprint::readLayout(bytes))==bytes,"an opening as the last element");}
                {auto broken=marked;qToLittleEndian<quint32>(0,broken.data()+count);rejects([&]{sprint::readLayout(broken);},"a marked area without points was read");}
                // The mark means nothing on other layers, and version 5 does not know it: the count is the number of points
                // (the bytes of version 6 taken as version 5 read as hundredths of a millimetre, 100 times as large).
                {Document silk=plain;silk.boards[0].elements[0].layer=SilkBottom;int dropped=0;sprint::writeLayout(silk,6,&dropped);require(dropped==1,"an opening off the copper sides is left out");
                    silk.boards[0].elements[0].maskOnly=false;auto bytes=sprint::writeLayout(silk);const int s=areaAt(bytes,SilkBottom);
                    require(s>0&&bytes[s+0x4a]==0&&qFromLittleEndian<quint32>(bytes.constData()+s+1+0x4c+12)==4,"no mark off the copper sides");bytes[s+0x4a]=1;
                    const auto e=sprint::readLayout(bytes).boards[0].elements;require(e.size()==3&&!e[0].maskOnly&&e[0].points==z.points&&e[1].points==t.points,"a mark on the silkscreen changes nothing");}
                {Document cu=plain;cu.boards[0].elements[0].maskOnly=false;auto bytes=sprint::writeLayout(cu);const int s=areaAt(bytes,CopperBottom);bytes[0]=5;bytes[s+0x4a]=1;
                    auto hundredfold=[](QPolygonF p){for(auto &q:p)q*=100;return p;};
                    const auto e=sprint::readLayout(bytes).boards[0].elements;
                    require(e.size()==3&&!e[0].maskOnly&&e[0].points==hundredfold(z.points)&&e[1].points==hundredfold(t.points),"version 5 does not know the mark");
                    require(sprint::writeLayout(sprint::readLayout(bytes))[s+0x4a]==0,"written as version 6 without the mark");}}
            // Autorouted tracks keep their pads; older files without them take the pads under the track's ends.
            {using Pads=std::array<int,2>;Document r;Board rb=newBoard("Pads",40,30);
                auto p0=newElement(ElementType::Pad);p0.pos={10,10};updateOutline(p0);auto p1=p0;p1.pos={30,10};updateOutline(p1);
                auto t=newElement(ElementType::Track);t.points={{10,10},{20,5},{30,10}};t.autorouted=true;t.autoroutePads={1,0};auto none=t;none.autoroutePads={-1,-1};
                rb.elements={p0,p1,t,none};r.boards={rb};assignIds(r);require(decode(encode(r))==r,"pads of autorouted tracks in the own format");
                auto changed=[&](int index,const std::function<void(QJsonObject&)> &change){auto o=toJson(r);auto bs=o["boards"].toArray();auto bo=bs[0].toObject();auto es=bo["elements"].toArray();
                    auto e=es[index].toObject();change(e);es[index]=e;bo["elements"]=es;bs[0]=bo;o["boards"]=bs;return o;};
                const auto older=fromJson(changed(3,[](QJsonObject &e){e.remove("autoroutePads");})).boards[0].elements;
                require(older[3].autoroutePads==Pads{0,1}&&older[2].autoroutePads==Pads{1,0},"older files: the pads under the track's ends");
                for(const QJsonArray &bad:{QJsonArray{0,4},QJsonArray{-2,0},QJsonArray{0,2},QJsonArray{0}})
                    rejects([&]{fromJson(changed(2,[&](QJsonObject &e){e["autoroutePads"]=bad;}));},"broken pads of an autorouted track accepted");
                Board cut=rb;removeElements(cut,{0});require(cut.elements[1].autoroutePads==Pads{0,-1},"pads follow removed elements");}
            broken([](QJsonObject &c){c["boards"]=QJsonArray();},"document without boards accepted");
            auto element=[&](int index,const std::function<void(QJsonObject&)> &change){return [=](QJsonObject &c){auto bs=c["boards"].toArray();auto bo=bs[0].toObject();auto es=bo["elements"].toArray();
                auto e=es[index].toObject();change(e);es[index]=e;bo["elements"]=es;bs[0]=bo;c["boards"]=bs;};};
            broken(element(0,[](QJsonObject &e){e["x"]=std::nan("");}),"NaN coordinate accepted");
            broken(element(0,[](QJsonObject &e){e["layer"]=8;}),"layer 8 accepted");
            broken(element(0,[](QJsonObject &e){e["part"]=-1;}),"negative component number accepted");
            broken(element(0,[](QJsonObject &e){e["shape"]="star";}),"unknown pad form accepted");
            broken(element(0,[](QJsonObject &e){e["outline"]=QJsonArray{QJsonArray{0,0},QJsonArray{1,1},QJsonArray{2,2}};}),"pad outline with the wrong corner count accepted");
            broken(element(0,[](QJsonObject &e){e["connections"]=QJsonArray{99};}),"airwire to a missing element accepted");
            broken(element(2,[](QJsonObject &e){e["points"]=QJsonArray{QJsonArray{0,0}};}),"track with one node accepted");
            broken(element(2,[](QJsonObject &e){e["width"]=-1;}),"negative width accepted");
            broken(element(6,[](QJsonObject &e){e["style"]=3;}),"text style 3 accepted");
            broken(element(0,[](QJsonObject &e){e["sprint"]=QJsonObject{{"record","*not base64*"}};}),"broken source data accepted");
            rejects([]{decode("{broken");},"broken JSON accepted");}

        // --- Geometry
        {auto p=newElement(ElementType::Pad);p.pos={5,5};p.shape=PadShape::SquareWide;p.size=2;updateOutline(p);
            require(near(p.points[0],{3,4})&&near(p.points[2],{7,6}),"wide square pad outline");
            rotate(p,{5,5},90);require(near(p.rotation,90)&&near(p.points[0],{4,7})&&near(bounds(p),QRectF(4,3,2,4)),"turned pad");
            mirror(p,0);require(near(p.pos,{-5,5})&&near(p.rotation,270),"mirrored pad");
            auto c=newElement(ElementType::Circle);c.pos={0,0};c.size=2;c.width=.5;c.start=0;c.stop=90;
            const auto r=bounds(c);require(near(r.left(),-.25,1e-3)&&near(r.top(),-2.25,1e-3)&&near(r.right(),2.25,1e-3)&&near(r.bottom(),.25,1e-3),"quarter arc lies top right");
            Board board=sample.boards[0];removeElements(board,{0});require(board.elements.size()==6&&board.elements[0].connections.isEmpty(),"airwires to removed pads are dropped");
            // Redundant nodes: repeated points and points in the middle of a straight run go, a reversal stays.
            require(withoutRedundantNodes(QPolygonF({{0,0},{1,0},{1,0},{2,0},{2,1}}))==QPolygonF({{0,0},{2,0},{2,1}})&&withoutRedundantNodes(QPolygonF({{0,0},{2,0},{1,0}})).size()==3,"redundant nodes");
            // A square end reaches half the width beyond the node, as the round one does, but with corners.
            auto t=newElement(ElementType::Track);t.points={{0,0},{10,0}};t.width=2;t.flatEnd=true;
            require(near(bounds(t),QRectF(-1,-1,12,2),1e-3)&&copperShape(t).contains(QPointF(10.9,.9))&&!copperShape(t).contains(QPointF(-.9,.9)),"square track end");
            // The component wizard: pads where the forms put them, pin 1 square, numbered as the wizard says.
            {auto pads=[](const Footprint &f){QList<Element> out;for(const auto &e:f.elements)if(e.type==ElementType::Pad||e.type==ElementType::SmdPad)out<<e;return out;};
                auto dip=pads(wizardFootprint(wizardDefaults(Wizard::DoubleRow)));
                require(dip.size()==8&&near(dip[0].pos,{-3.81,-3.81})&&dip[0].shape==PadShape::Square&&near(dip[4].pos,{3.81,3.81})&&dip[7].name=="8","wizard: double row");
                Wizard w=wizardDefaults(Wizard::SingleRow);w.count=5;auto sip=pads(wizardFootprint(w));require(sip.size()==5&&near(sip[0].pos,{-5.08,0})&&near(sip[4].pos,{5.08,0}),"wizard: single row");
                auto quad=pads(wizardFootprint(wizardDefaults(Wizard::Quad)));
                require(quad.size()==16&&quad[0].type==ElementType::SmdPad&&near(quad[0].pos,{-5,-1.905})&&near(quad[4].pos,{-1.905,5})&&near(quad[8].pos,{5,1.905}),"wizard: quad, counter-clockwise");
                auto ring=pads(wizardFootprint(wizardDefaults(Wizard::DoubleCircle)));
                require(ring.size()==12&&near(ring[0].pos,{0,-6})&&near(std::hypot(ring[1].pos.x(),ring[1].pos.y()),4),"wizard: two circles");
                require(wizardFootprint(wizardDefaults(Wizard::Circle)).elements.last().role==TextRole::Value,"wizard footprints are components");}
            // The selector: pads in groups by diameter (ascending), by form, and only those on the chosen layers.
            {Board pads=newBoard("Selector",50,50);for(double size:{2.0,1.8,1.8}){auto p=newElement(ElementType::Pad);p.size=size;p.pos={size*10,5};updateOutline(p);pads.elements<<p;}
                auto t=newElement(ElementType::Track);t.points={{1,1},{5,1}};t.width=.5;t.layer=SilkTop;pads.elements<<t;
                const auto groups=selectorGroups(pads,ElementType::Pad,0);
                require(groups.size()==2&&groups[0].elements==QList<int>({1,2})&&groups[1].elements==QList<int>{0},"selector groups by diameter");
                require(selectorGroups(pads,ElementType::Pad,2).size()==1&&selectorGroups(pads,ElementType::Track,0,1).isEmpty()&&selectorGroups(pads,ElementType::Track,0,2).size()==1,"selector by form and layers");
                // Through-plated pads as a kind of their own; silkscreen and outline together, as in the reference.
                pads.elements[1].via=true;auto edge=t;edge.layer=Outline;pads.elements<<edge;
                require(selectorGroups(pads,ElementType::Pad,0,0,true).size()==1&&selectorGroups(pads,ElementType::Pad,0,0,true)[0].elements==QList<int>{1},"vias only");
                require(selectorGroups(pads,ElementType::Track,0,2).value(0).elements==QList<int>({3,4})&&selectorGroups(pads,ElementType::Track,0,1).isEmpty(),"silkscreen and outline");}
            // Components sort by the letters of their designators, then by number.
            {Board parts=newBoard("Teile",50,50);int part=1;
                for(const char *name:{"R10","R2","C1"}){auto t=newElement(ElementType::Text);t.role=TextRole::Designator;t.text=name;t.part=part;
                    auto p=newElement(ElementType::Pad);p.pos={10.0*part,10};p.part=part++;updateOutline(p);parts.elements<<t<<p;}
                const auto list=components(parts);QStringList names;for(const auto &c:list)names<<parts.elements[c.designator].text;
                require(names==QStringList({"C1","R2","R10"})&&list[0].members==QList<int>({4,5})&&near(componentCentre(parts,list[0]),{30,10}),"component list order and centre");
                // A group around two components and a click on one: the group with both; groups alone make no component.
                parts.elements[0].groups={1};parts.elements[2].groups={1};
                require(withGroups(parts,{1})==QList<int>({0,1,2,3})&&withGroups(parts,{5})==QList<int>({4,5}),"selection by group and component");
                auto loose=newElement(ElementType::Text);loose.role=TextRole::Designator;loose.text="X1";loose.groups={9};parts.elements<<loose;require(components(parts).size()==3,"a group makes no component");}
            // The pick and place centre as the reference finds it: copper, silkscreen without texts, or both; with no such
            // element all elements but hidden texts; then the offset (x to the right, y upwards).
            {Board pp=newBoard("PnP",100,100);auto smd=newElement(ElementType::SmdPad);smd.size=2;smd.size2=1.5;smd.part=1;
                for(double x:{20.0,30.0}){smd.pos={x,40};updateOutline(smd);pp.elements<<smd;}
                auto frame=newElement(ElementType::Track);frame.layer=SilkTop;frame.width=.2;frame.part=1;frame.points={{26,34},{46,34},{46,42},{26,42},{26,34}};pp.elements<<frame;
                auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="R1";id.pos={10,30};id.part=1;updateStrokes(id);pp.elements<<id;
                auto value=id;value.role=TextRole::Value;value.text="10k";value.pos={60,70};updateStrokes(value);pp.elements<<value;
                auto centre=[&](int mode,QPointF offset={}){pp.elements[3].pickCentre=mode;pp.elements[3].pickOffset=offset;return pickPlaceCentre(pp,components(pp).value(0));};
                require(near(centre(0),{25,40},1e-3)&&near(centre(1),{36,38},1e-3)&&near(centre(2),{32.55,38},1e-3),"pick and place centres: copper, silkscreen, both");
                require(near(centre(0,{1,-1}),{26,41},1e-3),"pick and place offset");
                pp.elements[2].layer=Inner1;require(near(centre(0),{32.55,38},1e-3),"all copper counts, inner layers too");
                pp.elements[2].layer=Outline;require(near(centre(2),{25,40},1e-3),"the outline never counts");
                pp.elements[4].visible=false;QRectF all;for(int k:{0,1,2,3})all=all.united(bounds(pp.elements[k]));
                require(near(centre(1),all.center(),1e-3),"nothing on the silkscreen: all elements but hidden texts");}
            // A rotation that is no quarter turn: the rectangle lies in a frame turned counter-clockwise by the rotation
            // (whole degrees, modulo 90), as the reference showed for an L of three SMD pads at 30°, 40° and 45°.
            {Board rf=newBoard("Rahmen",100,100);auto smd=newElement(ElementType::SmdPad);smd.layer=CopperTop;smd.part=1;
                const QPointF at[]={{60,70},{66,70},{60,76}};const double w[]={2.4,2.4,1.2},h[]={1.2,1.2,2.4};
                for(int k=0;k<3;k++){smd.pos=at[k];smd.size=w[k];smd.size2=h[k];updateOutline(smd);rf.elements<<smd;}
                auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="U7";id.pos={57,64};id.part=1;updateStrokes(id);rf.elements<<id;
                auto centre=[&](double rotation){rf.elements[3].componentRotation=rotation;return pickPlaceCentre(rf,components(rf).value(0));};
                require(near(centre(0),{63,73.3})&&near(centre(90),{63,73.3},1e-9)&&near(centre(270),{63,73.3},1e-9),"no frame at quarter turns");
                require(near(centre(30),{61.6461,72.3451},1e-4)&&near(centre(40),{61.4990,71.7888},1e-4)&&near(centre(45),{61.5,71.5},1e-9),"frame turned counter-clockwise");
                require(!near(centre(30),centre(-30),1e-3)&&near(centre(-30),centre(60),1e-9)&&near(centre(330),centre(60),1e-9),"turned one way: -30° is 60°, not 30°");
                require(near(centre(30.4),centre(30),1e-12)&&near(centre(30.5),centre(30),1e-12)&&near(centre(31.5),centre(32),1e-12),"whole degrees, halves to even");
                rf.elements[3].pickOffset={2,0};require(near(centre(30),QPointF(63.6461,72.3451),1e-4),"the offset along the board's axes");}
            auto h=newElement(ElementType::Area);h.width=.2;h.hatched=true;require(near(hatchSpacing(h),.5),"hatch pitch at least 0.5 mm");
            h.width=.8;require(near(hatchSpacing(h),1.6),"hatch pitch twice the outline");h.hatchAuto=false;h.hatchPitch=1.2;require(near(hatchSpacing(h),1.2),"hatch pitch as set");
            // The lines of the grid: at multiples of the pitch from the top left corner of the working area (not of the
            // outline), from crossing to crossing with the outline, half the pitch wide. Columns run downwards, rows to
            // the right.
            auto grid=[&](QList<QLineF> &columns,QList<QLineF> &rows){columns.clear();rows.clear();
                for(auto l:hatchLines(h)){if(l.y1()>l.y2()||l.x1()>l.x2())l=QLineF(l.p2(),l.p1());(l.x1()==l.x2()?columns:rows).append(l);}};
            QList<QLineF> columns,rows;
            h.points={{3,3},{13,3},{13,13},{3,13}};h.width=.2;h.hatchPitch=2;grid(columns,rows);
            require(near(hatchLineWidth(h),1)&&columns.size()==5&&rows.size()==5&&near(columns[0].p1(),{4,3})&&near(columns[0].p2(),{4,13})&&near(rows[4].p1(),{3,12})&&near(rows[4].p2(),{13,12}),"grid lines end on the outline");
            h.points.translate(.7,.3);grid(columns,rows);
            require(columns.size()==5&&near(columns[0].p1(),{4,3.3})&&rows.size()==5&&near(rows[0].p1(),{3.7,4}),"grid lines at multiples of the pitch");
            // An edge exactly on a grid line: its nodes count as lying 0.2 µm further right and up, so the right and top
            // edges carry a line, the left and bottom ones none.
            h.points={{3,4.5},{12,4.5},{12,10.5},{3,10.5}};h.width=.3;h.hatchPitch=1.5;grid(columns,rows);
            require(columns.size()==6&&near(columns.first().x1(),4.5)&&near(columns.last().x1(),12)&&rows.size()==4&&near(rows.first().y1(),4.5)&&near(rows.last().y1(),9),"lines on the right and top edges only");
            require(near(columns[0].y1(),4.4998,1e-9)&&near(columns[0].y2(),10.4998,1e-9)&&near(rows[0].x1(),3.0002,1e-9)&&near(rows[0].x2(),12.0002,1e-9),"nodes on grid lines move right and up");
            // An outline with a notch: the crossings along a line pair up, so a line across the notch has two pieces.
            h.points={{2.9,3.3},{14.1,3.3},{14.1,12.7},{10.7,12.7},{10.7,7.1},{6.2,7.1},{6.2,12.7},{2.9,12.7}};grid(columns,rows);
            {QList<QLineF> across,above;for(const auto &l:rows)if(near(l.y1(),9))across.append(l);for(const auto &l:columns)if(near(l.x1(),9))above.append(l);
                require(columns.size()==8&&rows.size()==10&&across.size()==2&&near(across[0].x2(),6.2)&&near(across[1].x1(),10.7)&&above.size()==1&&near(above[0].y2(),7.1),"pieces of a line across a notch");}
            // A node beyond the 10 m of the own format or no number at all (a damaged Sprint-Layout file) gives no grid,
            // also where each node fits the units and only the span between two does not.
            {const double inf=std::numeric_limits<double>::infinity();
                h.points={{5,5},{20000,5},{15,15},{5,15}};require(hatchLines(h).isEmpty(),"no grid with a node beyond 10 m");
                for(const QPolygonF &outline:{QPolygonF{{5,5},{5e14,5},{15,15},{-5e14,15}},QPolygonF{{5,5},{inf,5},{15,15},{-inf,15}},QPolygonF{{5,5},{1e30,5},{15,15},{-1e30,15}},
                        QPolygonF{{5,5},{std::nan(""),5},{15,15},{5,15}}}){h.points=outline;require(hatchLines(h).isEmpty(),"no grid with nodes out of range or not numbers");}}}

        // --- Printing: which layers, how the images are arranged, what lands on the paper
        {Board b=newBoard("Druck",20,10);
            auto pad=newElement(ElementType::Pad);pad.pos={5,5};pad.size=3;pad.size2=1;updateOutline(pad);b.elements<<pad;
            auto t=newElement(ElementType::Track);t.points={{5,5},{15,5}};t.width=1;b.elements<<t;
            auto silk=newElement(ElementType::Track);silk.layer=SilkTop;silk.points={{2,8},{18,8}};silk.width=.5;b.elements<<silk;
            PrintSettings s=defaultPrintSettings(b);
            require(s.layers[CopperBottom]&&s.layers[SilkTop]&&!s.layers[CopperTop],"printed layers are those with elements");
            require(printImages(s)==QList<QList<int>>{{CopperBottom,SilkTop}},"one image, the bottom side first");
            s.order=3;require(printImages(s).size()==2&&near(printSize(b,s).width(),2*20+5),"the bottom side right of the top side");s.order=0;
            s.marks=true;require(near(printSize(b,s).width(),20+16),"registration marks around the board");s.marks=false;
            auto render=[&](const PrintSettings &settings){QImage image(200,100,QImage::Format_RGB32);image.fill(Qt::white);QPainter p(&image);p.scale(10,10);paintPrintout(p,b,settings);p.end();return image;};
            s.blackWhite=true;s.layers[SilkTop]=false;QImage image=render(s);
            require(image.pixelColor(100,50)==QColor(Qt::black)&&image.pixelColor(50,50)==QColor(Qt::white)&&image.pixelColor(50,38)==QColor(Qt::black),"black track and pad, open hole");
            s.mirrored=true;image=render(s);require(image.pixelColor(150,50)==QColor(Qt::white)&&image.pixelColor(150,38)==QColor(Qt::black),"mirrored, the pad is on the right");
            s.mirrored=false;s.negative=true;image=render(s);require(image.pixelColor(100,50)==QColor(Qt::white)&&image.pixelColor(100,90)==QColor(Qt::black),"negative");
            // The template of the side under the layers: 100 dpi, 2 mm from the left edge.
            s.negative=false;b.templates[1].file="unten.png";b.templates[1].dpi=100;b.templates[1].offset={2,0};
            QImage scan(20,20,QImage::Format_RGB32);scan.fill(QColor(0,0,255));s.templatePictures[1]=scan;s.withTemplate=true;
            image=render(s);require(image.pixelColor(30,20)==QColor(0,0,255)&&image.pixelColor(10,20)==QColor(Qt::white),"bottom template under the bottom side");
            s.order=1;s.layers[SilkTop]=true;image=render(s);require(image.pixelColor(30,20)==QColor(0,0,255),"both sides: the template of the side on top");
            s.order=0;image=render(s);require(image.pixelColor(30,20)==QColor(Qt::white),"no template for the top side");
            // Tiles: copies side by side and one below the other, a gap apart.
            {PrintSettings t=defaultPrintSettings(b);t.tilesX=3;t.tilesY=2;t.tileGap=5;require(near(printSize(b,t).width(),3*20+2*5)&&near(printSize(b,t).height(),2*10+5),"tiles and their gap");
                t.tilesX=99;require(near(printSize(b,t).width(),20*20+19*5),"at most 20 tiles");
                const QImage picture=printPicture(b,t,254);require(picture.width()==4950&&picture.height()==250,"the printout as a picture of 254 dpi for the clipboard");}
            // The solder mask of each side, each kind of opening with its offset and switch.
            {PrintSettings m=defaultPrintSettings(b);for(int l=1;l<=layerCount;l++)m.layers[l]=false;m.withTemplate=false;m.blackWhite=false;
                m.maskBottom=true;m.maskBottomColour=QColor(255,0,0);m.maskTopColour=QColor(0,0,255);
                auto at=[&](const PrintSettings &x,double mm){return render(x).pixelColor(int(std::lround(mm*10)),50);};
                require(at(m,5+1.7)==QColor(255,0,0)&&at(m,5+1.9)==QColor(Qt::white),"a pad's opening 0.3 mm larger, in the bottom side's colour");
                m.padMask=-.5;require(at(m,5+.8)==QColor(255,0,0)&&at(m,5+1.2)==QColor(Qt::white),"or smaller");m.padMask=.3;
                m.maskPads=false;require(at(m,5+1.7)==QColor(Qt::white),"pads switched off");m.maskPads=true;
                m.maskBottom=false;m.maskTop=true;require(at(m,5+1.7)==QColor(Qt::white),"the pad has no opening on the top side");
                m.order=2;m.maskBottom=true;require(printImages(m).size()==2,"with special layers both images stay");
                const QImage two=render(m);require(two.pixelColor(67,50)==QColor(Qt::white),"the bottom side's opening only in the bottom image");}}

        // --- Copper: ground plane, connections, design rule check
        {Board b=newBoard("Kupfer",40,30);b.groundPlane[CopperBottom]=true;
            auto pad=newElement(ElementType::Pad);pad.pos={10,10};pad.size=2;pad.size2=1;pad.clearance=.5;updateOutline(pad);b.elements<<pad;            // 0
            auto t1=newElement(ElementType::Track);t1.points={{10,10},{20,10}};t1.width=.5;b.elements<<t1;                                          // 1 joins the pad
            auto t2=newElement(ElementType::Track);t2.points={{20,10.7},{30,10.7}};t2.width=.5;b.elements<<t2;                                      // 2 0.2 mm away
            auto via=newElement(ElementType::Pad);via.pos={30,20};via.size=1.2;via.size2=.6;via.via=true;updateOutline(via);b.elements<<via;      // 3
            auto top=newElement(ElementType::Track);top.layer=CopperTop;top.points={{30,20},{35,25}};top.width=.3;b.elements<<top;                  // 4 via joins it
            auto gnd=newElement(ElementType::Track);gnd.points={{2,28},{8,28}};gnd.width=.5;gnd.clearance=0;b.elements<<gnd;                       // 5 joins the ground plane
            auto keep=newElement(ElementType::Area);keep.points={{30,2},{38,2},{38,8},{30,8}};keep.cutout=true;b.elements<<keep;                   // 6 keep-out
            auto silk=newElement(ElementType::Track);silk.layer=SilkBottom;silk.points={{8,10},{12,10}};silk.width=.2;b.elements<<silk;           // 7 over the pad
            require(copperLayers(b)==QList<int>({CopperTop,CopperBottom})&&!copperOn(via,CopperTop,b).isEmpty()&&copperOn(pad,CopperTop,b).isEmpty()&&copperOn(keep,CopperBottom,b).isEmpty(),"copper per layer");
            const auto ground=groundPlane(b,CopperBottom);
            require(ground.contains(QPointF(5,25))&&!ground.contains(QPointF(10,11.3))&&!ground.contains(QPointF(34,5))&&!ground.contains(QPointF(15,10.6)),"ground plane leaves clearance and keep-out free");
            require(groundPlane(b,CopperTop).isEmpty(),"no ground plane where it is off");
            const auto c=connections(b);
            require(c[0]==c[1]&&c[0]!=c[2]&&c[3]==c[4]&&c[6]==-1&&c[7]==-1,"connections by touching copper and vias");
            b.elements[0].connections={2};b.elements[2].connections={0};require(connections(b,true)[0]==connections(b,true)[2],"airwires join when asked");
            require(routedAirwires(b).isEmpty(),"an open airwire is not routed");
            require(connectedAt(b,{15,10},0).size()==2,"test point finds pad and track");
            Rules rules;rules.clearance=.3;rules.minTrack=.4;
            int reports=0,lastTotal=0;const auto found=checkDesign(b,rules,[&](int done,int total){reports++;lastTotal=total;require(done>=0&&done<total,"progress within its total");});
            require(reports>0&&lastTotal>0,"the check reports its progress");QStringList kinds;for(const auto &f:found)kinds<<f.message;
            require(kinds.filter("Abstand").size()==1&&kinds.filter("Leiterbahn").size()==1&&kinds.filter("Restring").isEmpty()&&kinds.filter("Bestückungsdruck").size()==1,"design rule findings");
            rules.minRing=.6;rules.minDrill=.8;rules.silkOnPads=false;const auto more=checkDesign(b,rules);QStringList k2;for(const auto &f:more)k2<<f.message;
            require(k2.filter("Restring").size()==2&&k2.filter("Bohrung kleiner").size()==1&&k2.filter("Bestückungsdruck").isEmpty(),"ring and drill rules");
            // Thermal pads keep spokes across their clearance.
            b.elements[0].thermal=true;require(groundPlane(b,CopperBottom).contains(QPointF(10,8.8))&&!groundPlane(b,CopperBottom).contains(QPointF(9.2,8.9)),"thermal spokes");
            // The autorouter goes round a wall at the distance asked for, and finds nothing when the wall closes the way.
            {Board r=newBoard("Router",30,20);r.grid=1.27;
                auto a=newElement(ElementType::Pad);a.pos={5.08,10.16};updateOutline(a);auto z=a;z.pos={25.4,10.16};updateOutline(z);
                auto wall=newElement(ElementType::Track);wall.points={{15.24,0},{15.24,15.24}};wall.width=.5;r.elements<<a<<z<<wall;
                const auto path=autoroute(r,0,1,CopperBottom,.5,.4,1.27);
                require(path.size()>2&&near(path.first(),a.pos)&&near(path.last(),z.pos),"autoroute from pad to pad");
                auto routed=newElement(ElementType::Track);routed.points=path;routed.width=.5;
                QPainterPathStroker grow;grow.setWidth(2*.35);const auto around=copperShape(routed).united(grow.createStroke(copperShape(routed)));
                require(!around.intersects(copperShape(wall)),"autoroute keeps its distance");
                r.elements[2].points={{15.24,0},{15.24,20}};require(autoroute(r,0,1,CopperBottom,.5,.4,1.27).isEmpty(),"no way through a closed wall");}
            // Spokes per layer: without the switch the first byte holds for every layer.
            {auto t=b.elements[0];t.thermalSpokes=0x5501;auto bars=[&](int layer){return thermalSpokes(t,layer).toSubpathPolygons().size();};
                require(bars(CopperTop)==1&&bars(CopperBottom)==1,"first byte for all layers");t.thermalPerLayer=true;require(bars(CopperTop)==1&&bars(CopperBottom)==4,"spokes per layer");}
            // The new switches and the multilayer mark survive Sprint-Layout 6.
            Document one;b.multilayer=true;one.boards={b};const auto again=sprint::readLayout(sprint::writeLayout(one)).boards[0];
            require(again.multilayer&&again.elements[0].thermal&&again.elements[6].cutout&&!again.elements[1].cutout&&again.groundPlane[CopperBottom],"thermal, keep-out and multilayer in Sprint-Layout files");}
        // --- Hatched areas in the ground plane: cut out by their whole outline, also at clearance 0, where the plane meets
        // the border and joins the area; the gaps never get plane. Keep-outs and mask openings stay as they were.
        {Board b=newBoard("Raster",30,16);b.groundPlane[CopperBottom]=true;
            auto area=newElement(ElementType::Area);area.layer=CopperBottom;area.points={{3,3},{13,3},{13,13},{3,13}};area.width=.2;area.hatched=true;area.hatchAuto=false;area.hatchPitch=2;area.clearance=0;b.elements<<area;
            auto pad=newElement(ElementType::SmdPad);pad.layer=CopperBottom;pad.pos={20,7};pad.size=pad.size2=.4;pad.clearance=0;updateOutline(pad);b.elements<<pad;     // on the plane
            auto ground=groundPlane(b,CopperBottom);auto c=connections(b);
            require(!ground.contains(QPointF(5,5))&&!ground.contains(QPointF(3.2,5))&&!ground.contains(QPointF(2.95,5))&&ground.contains(QPointF(2.85,5))&&c[0]==c[1],"plane round a hatched area at clearance 0, joined");
            // The round ends of the lines lie on the plane: 0.4 mm beyond the border.
            require(ground.contains(QPointF(8,2.85))&&copperShape(b.elements[0]).contains(QPointF(5,5)),"line ends over the plane, the area filled for the test");
            b.elements[0].clearance=.6;ground=groundPlane(b,CopperBottom);c=connections(b);
            require(!ground.contains(QPointF(5,5))&&!ground.contains(QPointF(2.35,5))&&ground.contains(QPointF(2.25,5))&&c[0]!=c[1],"plane round a hatched area with clearance");
            b.elements[0].cutout=true;ground=groundPlane(b,CopperBottom);
            require(!ground.contains(QPointF(5,5))&&!ground.contains(QPointF(2.95,5))&&ground.contains(QPointF(2.85,5)),"a hatched keep-out cuts its own shape, without clearance");
            b.elements[0].cutout=false;b.elements[0].maskOnly=true;require(groundPlane(b,CopperBottom).contains(QPointF(5,5)),"an opening of the mask leaves the plane");
            // The test and the design rule check take the outline as filled, as the reference does: a pad in a gap belongs
            // to the area and is not checked against its lines.
            b.elements[0].maskOnly=false;auto inGap=newElement(ElementType::SmdPad);inGap.layer=CopperBottom;inGap.pos={5,5};inGap.size=inGap.size2=.6;updateOutline(inGap);b.elements<<inGap;
            c=connections(b);require(c[0]==c[2]&&c[0]!=c[1]&&checkDesign(b,Rules()).isEmpty(),"a pad in a gap belongs to the area");}
        // --- Special shapes, against what the reference's dialog shows
        {// Polygon: the first corner at three o'clock, the offset counter-clockwise; rays from the centre to each corner.
            const auto hex=regularPolygon(6,5,.5,true,CopperTop);require(hex.size()==1&&hex[0].type==ElementType::Area&&hex[0].points.size()==6&&near(hex[0].points[0],{5,0}),"regular polygon");
            const auto turned=regularPolygon(5,10,.4,false,SilkTop,90);
            require(turned.size()==1&&turned[0].type==ElementType::Track&&turned[0].points.size()==6&&near(turned[0].points[0],{0,-10})&&turned[0].points.first()==turned[0].points.last(),
                    "offset counter-clockwise, closed outline");
            const auto star=regularPolygon(5,10,.4,false,SilkTop,30,true);
            require(star.size()==6&&star[1].type==ElementType::Track&&near(star[1].points[0],{0,0})&&near(star[3].points[1],star[0].points[2])&&star[1].width==.4,"rays to the corners");
            // Spiral: the width the reference shows, for seven sets of values read from its dialog.
            struct Case{double r,a,w,u,d;};
            for(const Case &c:{Case{2,2,.4,6,30.8},Case{2,2,1,6,38},Case{2,2,1,16,98},Case{5,2,1,16,104},Case{5,3,1,16,135},Case{5,3,1,100,807},Case{5,3,1,3,31}}){
                SpiralShape s;s.start=c.r;s.gap=c.a;s.width=c.w;s.turns=c.u;require(near(spiralDiameter(s),c.d,1e-9),"spiral width as the reference shows it");}
            auto extent=[](const Element &e){double l=1e9,r=-1e9;for(auto p:e.points){l=std::min(l,p.x());r=std::max(r,p.x());}return r-l+e.width;};
            // Round: half circles from three o'clock, counter-clockwise on the screen; square: sides growing by half a step.
            SpiralShape s;s.start=5;s.gap=3;s.width=1;s.turns=3;const auto coil=spiral(s,CopperTop);
            require(coil.points.size()==217&&near(coil.points.first(),{5,0})&&coil.points[1].y()<0&&near(coil.points.last(),{17,0},1e-9)&&near(extent(coil),spiralDiameter(s),1e-9),"round spiral");
            s.square=true;const auto box=spiral(s,CopperTop);
            require(box.points.size()==13&&near(box.points[0],{5,5})&&near(box.points[1],{-5,5})&&near(box.points[2],{-5,-7})&&near(box.points[3],{9,-7})&&near(extent(box),spiralDiameter(s),1e-9),
                    "square spiral: left, up, right, down");
            // Frame: as the reference builds it, 33 lines and 32 labels for eight columns and rows labelled on all sides.
            FrameShape f;const auto drawing=frame(f,SilkTop);int tracks=0,texts=0;for(const auto &e:drawing)(e.type==ElementType::Track?tracks:texts)++;
            require(tracks==33&&texts==32,"frame: 33 lines and 32 labels");
            auto length=[](const Element &e){double l=0;for(int i=1;i<e.points.size();i++)l+=QLineF(e.points[i-1],e.points[i]).length();return l;};
            require(near(length(drawing[0]),280)&&drawing[0].points.size()==5&&drawing[0].width==.4,"outline along its centre line");
            require(near(length(drawing[1]),90-2*3.81)&&drawing[1].width==.2&&near(length(drawing[5]),3.81)&&drawing[5].width==.2,"inner edge and a divider");
            const Element *a=nullptr,*one=nullptr;for(const auto &e:drawing)if(e.type==ElementType::Text){if(e.text=="A"&&!a)a=&e;if(e.text=="1"&&!one)one=&e;}
            const double column=(90-2*3.81)/8,row=(50-2*3.81)/8;
            require(a&&one&&near(bounds(*a).center(),{-45+3.81+column/2,-25+3.81/2},1e-9)&&near(bounds(*one).center(),{-45+3.81/2,-25+3.81+row/2},1e-9)&&near(a->size,2.286),
                    "labels in the middle of their fields");
            f.columnSides=FrameShape::None;f.rowSides=FrameShape::First;f.rowLetters=true;const auto side=frame(f,SilkTop);tracks=texts=0;for(const auto &e:side)(e.type==ElementType::Track?tracks:texts)++;
            require(tracks==9&&texts==8&&near(length(side[1]),50),"rows on the left only: the band runs the full height");
            require(frameLabel(0,true)=="A"&&frameLabel(25,true)=="Z"&&frameLabel(26,true)=="AA"&&frameLabel(9,false)=="10","labels");}

        // --- Interface texts of the module have English translations
        {QSet<QString> table;for(const auto &[german,english]:translationTable())table.insert(german);
            QStringList untranslated;const QRegularExpression literal(R"re((?<![A-Za-z_])ui\("((?:[^"\\]|\\.)*)"\))re");
            for(const auto *dir:{"/src/modules/pcb","/src/formats/sprint"})
                for(QDirIterator it(QString(OPENLOCH_SOURCE_DIR)+dir,{"*.cpp"},QDir::Files,QDirIterator::Subdirectories);it.hasNext();){
                    QFile f(it.next());require(f.open(QIODevice::ReadOnly),"cannot read a source file");const auto text=QString::fromUtf8(f.readAll());
                    for(auto m=literal.globalMatch(text);m.hasNext();){auto key=m.next().captured(1);key.replace("\\n","\n").replace("\\\"","\"").replace("\\t","\t").replace("\\\\","\\");
                        if(!table.contains(key))untranslated.append(QFileInfo(f.fileName()).fileName()+": "+key);}
                }
            if(!untranslated.isEmpty())throw std::runtime_error(("texts without English translation: "+untranslated.join(" | ")).toStdString());}
        editorTests();
        editingTests(preferences);
        preferencesTests(preferences);
        picturesTests();
        macroTests();
        fabricationTests();
        schematicTests();
    }catch(const std::exception &e){fprintf(stderr,"pcb test failed: %s\n",e.what());return 1;}
    puts("pcb tests passed");
    return 0;
}
