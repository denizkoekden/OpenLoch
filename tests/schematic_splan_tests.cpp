// Tests of the sPlan files (.spl8, .spl7): files put together here byte by byte from the layout in
// docs/modules/schematic.md, independent of the writer, are read, written back and changed.
#include "language.h"
#include "legacy_reader.h"
#include "formats/splan/inflate.h"
#include "formats/splan/records.h"
#include "formats/splan/splan.h"
#include "modules/schematic/dimension.h"
#include "modules/schematic/editor.h"
#include "modules/schematic/example.h"
#include "modules/schematic/library.h"
#include "modules/schematic/nets.h"
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtEndian>
#include <cmath>
#include <numbers>
#include <cstring>
#include <functional>
#include <stdexcept>

using namespace openloch;
using namespace openloch::schematic;
using openloch::splan::Record;
using openloch::splan::readFile;

namespace {
void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
bool near(double a,double b,double eps=1e-4){return std::abs(a-b)<eps;}
bool near(QPointF a,QPointF b,double eps=1e-4){return near(a.x(),b.x(),eps)&&near(a.y(),b.y(),eps);}
QString rejection(const std::function<void()> &fn){try{fn();}catch(const FormatError &e){return QString::fromUtf8(e.what());}return {};}

// Bytes in the order the files hold them: little endian numbers, floats, strings led by their length.
struct Out {
    QByteArray b;
    void u8(int v){b.append(char(v));}
    void i32(qint32 v){char c[4];qToLittleEndian(v,c);b.append(c,4);}
    void u32(quint32 v){i32(qint32(v));}
    void f32(float f){quint32 u;std::memcpy(&u,&f,4);i32(qint32(u));}
    void pt(double x,double y){f32(float(x));f32(float(y));}
    void wstr(const QString &s){i32(qint32(s.size()));for(QChar c:s){const char16_t u=c.unicode();b.append(char(u&0xff));b.append(char(u>>8));}}
    void sstr(const QByteArray &s){i32(qint32(s.size()));u8(int(s.size()));b.append(s);}
    void raw(const QByteArray &r){b.append(r);}
    // outline off/on, fill off/on, width, colour, style or size, fill colour, fill style, title block mark
    void header(int noOutline,int noFill,int width,quint32 colour,int style,quint32 fill,int fillStyle,int mark=0){
        u8(noOutline);u8(noFill);i32(width);u32(colour);i32(style);u32(fill);i32(fillStyle);u8(mark);
    }
    void stripes(){u8(0);u8(0);u32(0xffff);u32(0xff0000);}
    void text8(double x0,double y0,double x1,double y1,const QString &text,int size,int align,bool display,const QString &shown={},int mark=0){
        header(0,1,2,0,0,0xffffff,0,mark);pt(x0,y0);pt(x1,y0);pt(x1,y1);pt(x0,y1);
        wstr(text);sstr("Arial");u8(0);u32(0);i32(size);i32(align);u8(0);wstr(QString());
        raw(QByteArray(16,'\x11'));raw(QByteArray(16,'\0'));u8(0);u32(0xffffff);u8(0);
        if(display)wstr(shown.isEmpty()?text:shown);
    }
    // A text of sPlan 8 with its own key, the key of its target and an external link.
    void linkedText8(double x0,double y0,const QString &text,const QByteArray &guid,const QByteArray &target,const QString &link){
        u8(3);header(0,1,2,0,0,0xffffff,0);pt(x0,y0);pt(x0+200,y0);pt(x0+200,y0+40);pt(x0,y0+40);
        wstr(text);sstr("Arial");u8(0);u32(0);i32(40);i32(0);u8(0);wstr(link);
        raw(guid);raw(target);u8(0);u32(0xffffff);u8(0);
        if(text.contains(u'<')&&text.contains(u'>'))wstr(text);
    }
    void text7(double x0,double y0,double x1,double y1,const QByteArray &text,bool display){
        header(0,1,2,0,0,0xffffff,0);pt(x0,y0);pt(x1,y0);pt(x1,y1);pt(x0,y1);
        sstr(text);sstr("Arial");u8(1);u32(0);i32(40);i32(0);u8(0);sstr({});sstr({});
        if(display)sstr(text);
    }
    // A picture of a BMP file between two corners.
    void picture(double x0,double y0,double x1,double y1,const QByteArray &bmp,int key){
        u8(9);header(0,1,0,0,0,0xffffff,0);pt(x0,y0);pt(x1,y0);pt(x1,y1);pt(x0,y1);
        QByteArray data(4,'\0');qToLittleEndian<qint32>(qint32(bmp.size()),data.data());data+=bmp;
        raw(qCompress(data).mid(4));i32(key);
    }
};
QByteArray bitmap(){
    QImage image(3,2,QImage::Format_RGB32);image.fill(QColor(10,200,30));
    QByteArray bmp;QBuffer buffer(&bmp);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"BMP");return bmp;
}
struct Fixture {QByteArray bytes;QByteArray line,rectangle;};
// A sPlan 8 file with every kind of object on the first of two sheets.
Fixture spl8(){
    Out o;Fixture f;
    o.raw(QByteArray("\x07SPLAN80xyz",11));
    o.i32(2);o.wstr("Autor|Jemand");o.wstr("Firma|");
    o.u8(0);o.u8(1);o.wstr({});o.wstr({});
    o.i32(0);o.i32(1);                                              // active sheet, sheets less one
    // Sheet 1
    o.wstr("Erstes");o.i32(210);o.i32(297);o.i32(10);o.i32(0);o.i32(0);o.f32(.5f);
    o.i32(11);                                                      // twelve objects
    {Out l;l.u8(4);l.header(0,1,5,0x0000ff,2,0xffffff,0);l.i32(2);l.pt(100,100);l.pt(500,100);l.pt(500,300);
        l.u32(255);l.u8(0);l.u8(0);l.u8(2);l.i32(3);l.stripes();f.line=l.b;o.raw(l.b);}
    {Out r;r.u8(2);r.header(0,0,3,0,0,0x00ff00,0);r.pt(600,400);r.pt(600,200);r.pt(1000,200);r.pt(1000,400);r.pt(650,400);
        r.u8(0);r.u32(255);r.i32(3);r.i32(0);r.stripes();r.i32(1);r.i32(20);f.rectangle=r.b;o.raw(r.b);}
    o.u8(3);o.text8(1000,500,1400,540,"Hallo <Autor>",40,1,true,"Hallo Jemand");
    {   // An arc of an ellipse 20 × 10 mm around (200, 50) from 90° to 180°.
        o.u8(1);o.header(0,1,2,0,0,0xffffff,0);o.i32(31);
        for(int k=0;k<32;k++){const double a=2*std::numbers::pi*k/32;o.pt(2000+100*std::cos(a),500-50*std::sin(a));}
        o.pt(2000,500);o.f32(float(std::numbers::pi/2));o.f32(float(std::numbers::pi));o.u8(0);o.u32(255);o.i32(0);o.i32(0);o.stripes();o.i32(1);o.i32(20);
    }
    o.u8(8);o.header(0,0,0,0,15,0,0);o.pt(500,100);
    {   // A component: a body, a pin line and a contact; designator R1, value 10k.
        o.u8(7);o.i32(2);
        o.u8(2);o.header(0,1,2,0,0,0xffffff,0);o.pt(3000,1100);o.pt(3000,1000);o.pt(3200,1000);o.pt(3200,1100);o.pt(3000,1100);o.u8(0);o.u32(255);o.i32(0);o.i32(0);o.stripes();o.i32(1);o.i32(20);
        o.u8(4);o.header(0,1,2,0,0,0xffffff,0);o.i32(1);o.pt(2900,1050);o.pt(3000,1050);o.u32(255);o.u8(0);o.u8(0);o.u8(0);o.i32(2);o.stripes();
        o.u8(11);o.text8(2920,1000,2940,1030,"1",30,0,false);o.wstr("A");
        o.text8(3000,950,3060,990,"R1",40,0,true);
        o.text8(3000,1110,3080,1150,"10k",40,0,false);
        o.raw(QByteArray("\0\0\1\1",4));o.wstr("Widerstand");o.wstr("R");o.i32(1);o.pt(0,0);o.i32(10);o.u8(1);
        o.wstr("a");o.wstr({});o.wstr({});o.wstr({});o.u8(0);o.i32(12345);o.i32(0);o.u8(0);
    }
    o.u8(4);o.header(0,1,2,0,0,0xffffff,0,1);o.i32(1);o.pt(100,2000);o.pt(2900,2000);o.u32(255);o.u8(0);o.u8(0);o.u8(0);o.i32(2);o.stripes();
    o.u8(5);o.header(0,0,2,0,0,0xff00ff,0);o.i32(2);o.pt(100,600);o.pt(300,600);o.pt(200,800);o.u8(0);o.u32(255);o.i32(0);o.stripes();o.i32(1);o.i32(20);
    o.u8(12);o.header(0,1,2,0,0,0xffffff,0);o.i32(3);o.pt(100,900);o.pt(150,850);o.pt(250,850);o.pt(300,900);o.u32(255);o.u8(0);o.u8(0);o.u8(1);o.i32(2);o.stripes();
    o.u8(10);o.header(0,1,2,0,0,0xffffff,0);o.pt(1000,1000);o.pt(1400,1000);o.pt(1400,1200);o.pt(1000,1200);o.u8(0);o.sstr("Arial");o.u8(0);o.u32(0);o.i32(30);
    o.i32(2);o.wstr("Zeile 1");o.wstr("Zeile 2");o.u8(0);o.i32(0);o.u8(0);o.u8(0);
    {   // A picture: a zlib stream of the size and a BMP file.
        o.u8(9);o.header(0,1,0,0,0,0xffffff,0);o.pt(1500,1000);o.pt(1800,1000);o.pt(1800,1200);o.pt(1500,1200);
        const QByteArray bmp=bitmap();QByteArray data(4,'\0');qToLittleEndian<qint32>(qint32(bmp.size()),data.data());data+=bmp;
        o.raw(qCompress(data).mid(4));o.i32(777);
    }
    {   // A dimension: three texts and its points.
        o.u8(14);o.header(0,1,2,0,0,0xffffff,0);
        for(const char *t:{"25,0","",""})o.text8(400,1400,500,1430,QString::fromLatin1(t),30,1,false);
        o.u8(0);o.pt(200,1500);o.pt(450,1500);o.pt(450,1450);o.pt(0,0);o.pt(0,0);o.u8(0);o.u8(1);o.wstr({});o.wstr({});o.wstr({});
        o.i32(0);o.i32(0);o.i32(0);o.i32(0);o.u8(0);o.i32(0);
    }
    o.i32(-1);o.i32(0);o.f32(1234.5f);                              // magnetic guides: no horizontal one, one vertical
    o.raw(QByteArray(8,'\0'));{const double one=1;QByteArray d(8,'\0');std::memcpy(d.data(),&one,8);o.raw(d);}o.i32(1);o.wstr("Beschreibung");
    o.raw(QByteArray(36,'\0'));o.u8(0);o.f32(2800);o.f32(1900);o.f32(100);o.f32(100);o.i32(10);o.i32(8);o.i32(1);o.i32(1);o.u8(1);
    // Sheet 2: A4 upright, no objects.
    o.wstr("Zweites");o.i32(297);o.i32(210);o.i32(25);o.i32(0);o.i32(0);o.f32(1);o.i32(-1);o.i32(-1);o.i32(-1);
    o.raw(QByteArray(8,'\0'));{const double one=1;QByteArray d(8,'\0');std::memcpy(d.data(),&one,8);o.raw(d);}o.i32(1);o.wstr({});
    o.raw(QByteArray(36,'\0'));o.u8(0);o.f32(2800);o.f32(1900);o.f32(100);o.f32(100);o.i32(10);o.i32(8);o.i32(1);o.i32(1);o.u8(0);
    f.bytes=o.b;
    return f;
}
// What follows a component's parts and texts in sPlan 8: flags, caption, designator split, insertion point, extras, keys.
void componentTail(Out &o,const QByteArray &flags,const QString &caption,const QString &prefix,int number,int key){
    o.raw(flags);o.wstr(caption);o.wstr(prefix);o.i32(number);o.pt(0,0);o.i32(10);o.u8(1);
    for(int k=0;k<4;k++)o.wstr({});o.u8(0);o.i32(key);o.i32(0);o.u8(0);
}
// The end of a sPlan 8 sheet after its objects.
void sheetTail(Out &o){
    o.i32(-1);o.i32(-1);o.raw(QByteArray(8,'\0'));{const double one=1;QByteArray d(8,'\0');std::memcpy(d.data(),&one,8);o.raw(d);}o.i32(1);o.wstr({});
    o.raw(QByteArray(36,'\0'));o.u8(0);o.f32(2800);o.f32(1900);o.f32(100);o.f32(100);o.i32(10);o.i32(8);o.i32(1);o.i32(1);o.u8(0);
}
// A sPlan 8 file with one component (a box and a relay coil K1 as a component of its own).
QByteArray nested8(){
    Out o;o.raw(QByteArray("\x07SPLAN80xyz",11));o.i32(0);o.u8(0);o.u8(1);o.wstr({});o.wstr({});o.i32(0);o.i32(0);
    o.wstr("Blatt");o.i32(210);o.i32(297);o.i32(10);o.i32(0);o.i32(0);o.f32(1);o.i32(0);
    o.u8(7);o.i32(1);
    o.u8(2);o.header(0,1,2,0,0,0xffffff,0);o.pt(1000,1400);o.pt(1000,1000);o.pt(1600,1000);o.pt(1600,1400);o.pt(1000,1400);o.u8(0);o.u32(255);o.i32(0);o.i32(0);o.stripes();o.i32(1);o.i32(20);
    {   o.u8(7);o.i32(0);
        o.u8(4);o.header(0,1,2,0,0,0xffffff,0);o.i32(1);o.pt(1100,1200);o.pt(1300,1200);o.u32(255);o.u8(0);o.u8(0);o.u8(0);o.i32(2);o.stripes();
        o.text8(1100,1100,1160,1140,"K1",40,0,true);o.text8(1100,1250,1180,1290,"24V",40,0,false);
        componentTail(o,QByteArray("\0\0\0\1",4),"Spule","K",1,4711);
    }
    o.text8(1000,940,1060,980,"A1",40,0,true);o.text8(1000,1410,1080,1450,"Modul",40,0,false);
    componentTail(o,QByteArray("\0\0\1\1",4),"Baugruppe","A",1,4712);
    sheetTail(o);
    return o.b;
}
// A sPlan 8 file with a link from one text to another and a text with an external link.
QByteArray links8(){
    Out o;o.raw(QByteArray("\x07SPLAN80xyz",11));o.i32(0);o.u8(0);o.u8(1);o.wstr({});o.wstr({});o.i32(0);o.i32(0);
    o.wstr("Blatt");o.i32(210);o.i32(297);o.i32(10);o.i32(0);o.i32(0);o.f32(1);o.i32(2);
    const QByteArray target(16,'\x42'),other(16,'\x43'),none(16,'\0');
    o.linkedText8(100,100,"Ziel",target,none,{});
    o.linkedText8(100,300,"zum Ziel",other,target,{});
    o.linkedText8(100,500,"Datenblatt",QByteArray(16,'\x44'),none,"https://example.org");
    sheetTail(o);
    return o.b;
}
// A sPlan 7 file: short strings in Windows-1252, the sheet name in a buffer with left-over bytes after it.
QByteArray spl7(){
    Out o;
    o.raw(QByteArray("\x07SPLAN70\x01\x02\x03",11));
    o.i32(1);o.sstr("Name|Wert");o.u8(0);o.u8(1);o.i32(0);o.i32(0);
    QByteArray name(31,'Z');name[0]=6;name.replace(1,6,"Blatt7");o.raw(name);
    o.i32(210);o.i32(297);o.raw(QByteArray(2,'\x05'));o.i32(10);o.i32(0);o.i32(0);o.f32(1);
    o.i32(1);
    o.u8(3);o.text7(100,100,300,140,QByteArray("Text \xe4"),false);
    o.u8(7);o.i32(0);o.u8(4);o.header(0,1,2,0,0,0xffffff,0);o.i32(1);o.pt(500,500);o.pt(600,500);o.u32(255);o.u8(0);o.u8(0);o.u8(0);o.i32(1);
    o.text7(500,440,540,480,"K1",true);o.text7(500,520,540,560,"",false);
    o.raw(QByteArray("\1\0\1\1",4));o.sstr("Relais");o.sstr("K");o.i32(1);o.pt(0,0);o.i32(10);o.u8(1);o.sstr("Zusatz");o.u8(0);o.i32(99);o.i32(0);
    o.i32(-1);o.i32(-1);o.raw(QByteArray(8,'\0'));{const double one=1;QByteArray d(8,'\0');std::memcpy(d.data(),&one,8);o.raw(d);}o.i32(1);
    o.i32(2);o.raw(QByteArray("\xc4\x37",2));
    return o.b;
}
}

// A page of a sPlan 8 library: a component with a contact and a picture, and a clip (a group).
QByteArray library8(){
    Out o;o.raw(QByteArray("\x07SPLAN80abc",11));o.i32(0);o.i32(0);
    o.wstr(QString::fromUtf8("Widerstände\rResistors\rRésistances"));o.i32(297);o.i32(210);o.i32(100);o.i32(0);o.i32(0);o.f32(.4f);o.i32(1);
    o.u8(7);o.i32(2);
    o.u8(2);o.header(0,1,2,0,0,0xffffff,0);o.pt(1000,1040);o.pt(1000,1000);o.pt(1200,1000);o.pt(1200,1040);o.pt(1000,1040);o.u8(0);o.u32(255);o.i32(0);o.i32(0);o.stripes();o.i32(1);o.i32(20);
    o.u8(11);o.text8(950,1000,970,1030,"1",30,0,false);o.wstr("1");
    o.picture(1050,1005,1150,1035,bitmap(),3);
    o.text8(1000,950,1060,990,"R?",40,0,true);o.text8(1000,1050,1080,1090,"",40,0,false);
    componentTail(o,QByteArray("\0\0\1\1",4),QString::fromUtf8("Widerstand\rResistor\rRésistance"),"R",0,77);
    o.u8(6);o.i32(0);
    o.u8(4);o.header(0,1,2,0,0,0xffffff,0);o.i32(1);o.pt(2000,500);o.pt(2200,500);o.u32(255);o.u8(0);o.u8(0);o.u8(0);o.i32(2);o.stripes();
    return o.b;
}
// A page of a sPlan 7 library: the name in a buffer, one component.
QByteArray library7(){
    Out o;o.raw(QByteArray("\x07SPLAN70abc",11));o.i32(0);o.i32(0);
    QByteArray name(31,'Q');name[0]=5;name.replace(1,5,"Seite");o.raw(name);o.i32(297);o.i32(210);o.raw(QByteArray("\5\1",2));o.i32(100);o.i32(0);o.i32(0);o.f32(.2f);o.i32(0);
    o.u8(7);o.i32(0);o.u8(4);o.header(0,1,2,0,0,0xffffff,0);o.i32(1);o.pt(500,500);o.pt(600,500);o.u32(255);o.u8(0);o.u8(0);o.u8(0);o.i32(1);
    o.text7(500,440,540,480,"S1",true);o.text7(500,520,540,560,"",false);
    o.raw(QByteArray("\0\0\1\1",4));o.sstr("Schalter");o.sstr("S");o.i32(1);o.pt(0,0);o.i32(10);o.u8(1);o.sstr({});o.u8(0);o.i32(5);o.i32(0);
    return o.b;
}
// A sPlan 8 title block: a frame line and a text, both marked for the title block.
QByteArray form8(){
    Out o;o.i32(-80);o.i32(0);o.i32(1);
    o.u8(4);o.header(0,1,5,0,0,0xffffff,0,1);o.i32(1);o.pt(100,100);o.pt(2870,100);o.u32(255);o.u8(0);o.u8(0);o.u8(0);o.i32(2);o.stripes();
    o.u8(3);o.text8(2000,1900,2600,1940,"<PAGENAME>",35,0,true,{},1);
    return o.b;
}
int schematicSplanTests(){
    // --- zlib streams with bytes after them
    {const QByteArray data=QByteArray("OpenLoch ")+QByteArray(2000,'x');const QByteArray packed=splan::deflateZlib(data);
        qsizetype used=0;require(splan::inflateZlib(packed+"tail",0,&used)==data&&used==packed.size(),"a zlib stream ends where it ends");
        QByteArray broken=packed;broken[broken.size()-1]=char(broken[broken.size()-1]^1);
        require(!rejection([&]{splan::inflateZlib(broken,0,&used);}).isEmpty(),"a wrong checksum is refused");
        require(!rejection([&]{splan::inflateZlib(packed.left(packed.size()/2),0,&used);}).isEmpty(),"half a stream is refused");}

    // --- Reading a sPlan 8 file
    const Fixture fixture=spl8();QStringList notes;
    const Document d=splan::read(fixture.bytes,&notes);
    require(splan::version(fixture.bytes)==80&&splan::sourceVersion(d)==80,"version 8");
    require(d.sheets.size()==2&&d.activeSheet==0,"two sheets");
    const Sheet &s=d.sheets[0];
    require(s.name=="Erstes"&&near(s.width,297)&&near(s.height,210)&&near(s.grid,1)&&s.description=="Beschreibung","sheet: name, width after height, grid in tenths, description");
    require(s.horizontalGuides.isEmpty()&&s.verticalGuides.size()==1&&near(s.verticalGuides[0],123.45),"a vertical guide line at x = 123.45 mm");
    require(d.sheets[1].name=="Zweites"&&near(d.sheets[1].width,210)&&near(d.sheets[1].height,297)&&near(d.sheets[1].grid,2.5),"second sheet upright");
    require(s.titleBlock.frame==QRectF(10,10,280,190)&&s.titleBlock.columns==10&&s.titleBlock.rows==8&&s.titleBlock.showGrid,"title block settings");
    require(s.titleBlock.items.size()==1&&s.titleBlock.items[0].type==ItemType::Line,"objects marked for the title block go there");
    require(d.variables.size()==2&&d.variables[0].name=="Autor"&&d.variables[0].value=="Jemand"&&d.variables[1].value.isEmpty(),"user variables");
    require(s.items.size()==11,"eleven objects on the sheet");
    const Item &line=s.items[0];
    require(line.type==ItemType::Line&&line.points==QPolygonF({QPointF(10,10),QPointF(50,10),QPointF(50,30)})&&line.electrical,"a line in millimetres, a conductor");
    require(near(line.pen.width,.5)&&line.pen.color==QColor(255,0,0)&&line.pen.style==PenStyle::Dash,"its outline: width, colour (BGR), dashes from the header");
    require(line.startEnd==LineEnd::None&&line.endEnd==LineEnd::Arrow&&near(line.endSize,6),"its ends: an arrow of size L, as long as twelve line widths");
    const Item &rect=s.items[1];
    require(rect.type==ItemType::Rectangle&&near(rect.centre,{80,30})&&near(rect.size.width(),40)&&near(rect.size.height(),20)&&near(rect.rotation,0),"a rectangle from its corners");
    require(rect.corners==Corners::Round&&near(rect.corner,25)&&rect.pen.style==PenStyle::DashDot&&rect.fill.style==FillStyle::Solid&&rect.fill.color==QColor(0,255,0),"rounded corners, dash-dot, filled");
    require(near(rect.fill.spacing,2)&&near(rect.fill.lineWidth,.1),"hatch spacing and width");
    const Item &text=s.items[2];
    require(text.type==ItemType::Text&&text.text=="Hallo <Autor>"&&text.align==Align::Centre&&near(text.pos,{120,50})&&near(text.rotation,0)&&!text.mirrored,"a centred text at the middle of its top edge");
    require(text.font.family=="Arial"&&near(text.font.height,4)&&text.font.color==QColor(0,0,0)&&!text.background&&text.backgroundColor==QColor(255,255,255),"its font, no background");
    const Item &arc=s.items[3];
    require(arc.type==ItemType::Ellipse&&near(arc.centre,{200,50})&&near(arc.size.width(),20,1e-3)&&near(arc.size.height(),10,1e-3)&&arc.arc==ArcStyle::Arc&&near(arc.start,90)&&near(arc.stop,180),"an arc of an ellipse");
    const Item &junction=s.items[4];
    require(junction.type==ItemType::Junction&&near(junction.pos,{50,10})&&near(junction.size.width(),1.5),"a junction, its size in tenths of a millimetre");
    const Item &part=s.items[5];
    require(part.type==ItemType::Component&&near(part.pos,{290,105})&&part.designator=="R1"&&part.value=="10k"&&part.caption=="Widerstand"&&part.extra.value(0)=="a","a component: its insertion point from the reference point of its parts");
    require(part.designatorVisible&&part.valueVisible,"designator and value shown");
    {const auto list=contacts(part);require(list.size()==1&&list[0]->name=="A"&&list[0]->text=="1","a contact: name and text");
        require(list[0]->hasPin&&near(list[0]->pin,{0,0}),"its connection point: the free end of the pin line next to it");
        bool body=false;for(const auto &c:part.children)if(c.type==ItemType::Rectangle)body=near(c.centre,{20,0});require(body,"parts in local coordinates");}
    require(s.items[6].type==ItemType::Polygon&&s.items[6].fill.color==QColor(255,0,255),"a polygon");
    require(s.items[7].type==ItemType::Bezier&&s.items[7].points.size()==4&&s.items[7].endEnd==LineEnd::Triangle,"a Bézier curve");
    require(s.items[8].type==ItemType::TextBox&&s.items[8].text=="Zeile 1\nZeile 2"&&near(s.items[8].font.height,3),"a text box with its lines");
    {const Item &picture=s.items[9];require(picture.type==ItemType::Image&&near(picture.centre,{165,110})&&d.resources.contains(picture.resource),"a picture");
        require(QImage::fromData(d.resources[picture.resource].data).pixelColor(0,0)==QColor(10,200,30),"the picture's BMP file");}
    {const Item &dim=s.items[10];
        require(dim.type==ItemType::Dimension&&dim.dimension==DimensionKind::Standard&&near(dim.points[0],{20,150})&&near(dim.points[1],{45,150})&&dim.offset==0,"a dimension: kind, points, offset");
        require(dim.autoValue&&!dim.showDiameter&&near(dim.font.height,3)&&dimensionText(dim,1)=="25","its settings and its value");}
    require(notes.size()==1&&notes.join(' ').contains("abgeleitet"),"the partial preview names derived connection points");

    // --- Writing back: unchanged, through the fields, through the own format
    require(splan::write(d,80)==fixture.bytes,"an unchanged file is written byte for byte");
    require(splan::write(d,80,nullptr,false)==fixture.bytes,"written from the fields as well");
    require(splan::write(decode(encode(d)),80)==fixture.bytes,"also after the own format");
    {QStringList losses;splan::write(d,80,&losses);require(losses.isEmpty(),"nothing is lost, also not the derived connection point");}
    {Document g=d;g.sheets[0].horizontalGuides={50};g.sheets[1].verticalGuides={20,30.5};const Document back=splan::read(splan::write(g,80));
        require(back.sheets[0].horizontalGuides==QList<double>{50}&&near(back.sheets[0].verticalGuides.value(0),123.45)&&back.sheets[1].verticalGuides==QList<double>{20,30.5},"guide lines written and read again");
        require(splan::read(splan::write(g,70)).sheets[1].verticalGuides==QList<double>{20,30.5},"also in sPlan 7");}
    // "Bauteile mit Seitennummer", "Blätter mit Seitennummer" and the prefix are part of the file (the prefix from
    // sPlan 8 on); a new file has sheet numbers only.
    {require(!d.designatorPageNumbers&&d.sheetNumbers&&d.designatorPrefix.isEmpty(),"the test file: sheet numbers only");
        Document g=d;g.designatorPageNumbers=true;g.sheetNumbers=false;g.designatorPrefix=QStringLiteral("=A-");
        QStringList losses;const QByteArray bytes=splan::write(g,80,&losses);const Record file=readFile(bytes);
        require(file.integer("b1")==1&&file.integer("b2")==0&&file.string("s1")=="=A-"&&losses.isEmpty(),"written where sPlan keeps them");
        const Document back=splan::read(bytes);require(back.designatorPageNumbers&&!back.sheetNumbers&&back.designatorPrefix=="=A-","read again");
        require(splan::write(back,80)==bytes,"and written back unchanged");
        const Document seven=splan::read(splan::write(g,70,&losses));
        require(seven.designatorPageNumbers&&!seven.sheetNumbers&&seven.designatorPrefix.isEmpty()&&losses.join(' ').contains("Präfix"),"sPlan 7: the switches kept, the prefix lost");
        Document fresh=newDocument(QStringLiteral("Blatt"));const Record made=readFile(splan::write(fresh,80));
        require(made.integer("b1")==0&&made.integer("b2")==1&&made.string("s1").isEmpty(),"a new file as sPlan makes it");}
    // Print settings of a sheet in its 36 bytes (sPlan 8): orientation, the offset (tenths of a millimetre, opposite
    // sign), percent, free scale, banner pages and overlap; sPlan 7 keeps none.
    {require(d.sheets[0].print==PrintSettings(),"the test file prints as standard");
        Document g=d;PrintSettings &p=g.sheets[0].print;p.free=true;p.scale=1.5;p.orientation=PrintSettings::Orientation::Landscape;p.bannerX=2;p.bannerY=3;p.overlap=10;
        p.offset=QPointF(50,30.5);
        QStringList losses;const QByteArray bytes=splan::write(g,80,&losses);
        const QByteArray b=readFile(bytes).lists["sheets"][0].raw("reserved36");
        require(b.size()==36&&qFromLittleEndian<qint32>(b.constData())==2&&qFromLittleEndian<qint32>(b.constData()+12)==150&&b[16]==1
                &&qFromLittleEndian<qint32>(b.constData()+20)==2&&qFromLittleEndian<qint32>(b.constData()+24)==3&&qFromLittleEndian<qint32>(b.constData()+28)==10,"the fields where sPlan keeps them");
        require(qFromLittleEndian<float>(b.constData()+4)==-500.f&&qFromLittleEndian<float>(b.constData()+8)==-305.f,"the offset in tenths of a millimetre, sign turned");
        require(splan::read(bytes).sheets[0].print==p&&losses.isEmpty(),"read again, nothing lost");
        require(splan::write(splan::read(bytes),80)==bytes,"and written back unchanged");
        {Record f=readFile(fixture.bytes);QByteArray r=f.lists["sheets"][0].raw("reserved36");qToLittleEndian(-1000.f,r.data()+4);qToLittleEndian(250.f,r.data()+8);
            f.lists["sheets"][0].set("reserved36",r);
            require(splan::read(writeFile(f)).sheets[0].print.offset==QPointF(100,-25),"a file of the reference: 100 mm right, 25 mm up");
            qToLittleEndian(std::numeric_limits<float>::quiet_NaN(),r.data()+4);f.lists["sheets"][0].set("reserved36",r);
            require(splan::read(writeFile(f)).sheets[0].print.offset==QPointF(0,-25),"a value that is no number: none");
            QByteArray u=readFile(fixture.bytes).lists["sheets"][0].raw("reserved36");u[33]=7;Record g2=readFile(fixture.bytes);g2.lists["sheets"][0].set("reserved36",u);
            const QByteArray withUnknown=writeFile(g2);QStringList notes;const Document kept=splan::read(withUnknown,&notes);
            require(notes.join(' ').contains("nicht angewendet")&&splan::write(kept,80)==withUnknown,"the field of unknown meaning: kept, said so, not applied");
            QStringList none;splan::read(fixture.bytes,&none);require(!none.join(' ').contains("Druckeinstellungen"),"no such note when it is empty");}
        losses.clear();splan::write(g,70,&losses);require(losses.join(' ').contains("Druckeinstellungen"),"sPlan 7 keeps none");}
    // The scale's unit: the millimetres it holds (1, 10, 1000, 1000000); the reference shows other values as millimetres.
    {Document g=d;g.sheets[0].scale=2;g.sheets[0].scaleUnit=ScaleUnit::Metre;const QByteArray bytes=splan::write(g,80);
        Record file=readFile(bytes);require(file.lists["sheets"][0].integer("scaleUnit")==1000,"metres written as 1000");
        require(splan::read(bytes).sheets[0].scaleUnit==ScaleUnit::Metre&&splan::read(splan::write(g,70)).sheets[0].scaleUnit==ScaleUnit::Metre,"and read again, also in sPlan 7");
        file.lists["sheets"][0].set("scaleUnit",7);require(splan::read(splan::writeFile(file)).sheets[0].scaleUnit==ScaleUnit::Millimetre,"another value: millimetres");
        require(d.sheets[0].scaleUnit==ScaleUnit::Millimetre,"the test file in millimetres");}
    // Fill styles in the reference's numbering: 0 solid, 1 clear, 2 horizontal, 3 vertical, 4 falling, 5 rising, 6 cross,
    // 7 diagonal cross; sPlan 7 keeps neither spacing nor line width.
    {Document own=exampleDocument();const QList<std::pair<FillStyle,int>> styles{{FillStyle::Solid,0},{FillStyle::Horizontal,2},{FillStyle::Vertical,3},
            {FillStyle::BackDiagonal,4},{FillStyle::Diagonal,5},{FillStyle::Cross,6},{FillStyle::DiagonalCross,7}};
        const int first=int(own.sheets[0].items.size());
        for(int k=0;k<styles.size();k++){Item p;p.type=ItemType::Polygon;p.points={QPointF(10*k,100),QPointF(10*k+8,100),QPointF(10*k+4,108)};p.fill.style=styles[k].first;assignIds(p);own.sheets[0].items<<p;}
        const QByteArray bytes=splan::write(own,80);const Record file=readFile(bytes);const auto objects=file.lists["sheets"][0].lists["children"];
        QList<Record> polygons;for(const auto &r:objects)if(r.type==5)polygons<<r;polygons=polygons.mid(polygons.size()-styles.size());
        bool written=polygons.size()==styles.size();for(int k=0;written&&k<styles.size();k++)written&=polygons[k].integer("fillStyle")==styles[k].second&&polygons[k].integer("noFill")==0;
        require(written,"each fill style written with the reference's number");
        const Document back=splan::read(bytes);bool read=true;for(int k=0;k<styles.size();k++)read&=back.sheets[0].items.value(first+k).fill.style==styles[k].first;
        require(read,"and read again");
        Record clear=readFile(bytes);auto &list=clear.lists["sheets"][0].lists["children"];QList<int> at;for(int k=0;k<list.size();k++)if(list[k].type==5)at<<k;
        list[at[at.size()-7]].set("fillStyle",1);list[at[at.size()-6]].set("fillStyle",9);
        const Document none=splan::read(splan::writeFile(clear));
        require(none.sheets[0].items[first].fill.style==FillStyle::None&&none.sheets[0].items[first+1].fill.style==FillStyle::None,"clear and unknown styles: no fill, as the reference shows them");
        QStringList losses;splan::write(own,70,&losses);require(!losses.join(' ').contains("Schraffuren"),"sPlan 7: hatches with the usual spacing lose nothing");
        own.sheets[0].items[first+1].fill.spacing=1;losses.clear();splan::write(own,70,&losses);require(losses.join(' ').contains("1 Schraffuren erhalten"),"another spacing is lost");}
    // Second colour, stripes and struck out texts; sPlan 7 keeps the second colour and the struck out texts only.
    {const Pen &read=d.sheets[0].items[0].pen;
        require(!read.twoColour&&!read.inner&&!read.cross&&read.color2==QColor(255,0,0)&&read.innerColor==QColor(255,255,0)&&read.crossColor==QColor(0,0,255),"read: no stripes, the colours sPlan gives new lines");
        Document g=d;Pen &line=g.sheets[0].items[0].pen;line.twoColour=true;line.color2=QColor(0,128,0);line.inner=true;line.innerColor=QColor(1,2,3);line.cross=true;line.crossColor=QColor(4,5,6);
        g.sheets[0].items[1].pen.cross=true;g.sheets[0].items[2].font.strikeOut=true;
        QStringList losses;const Document back=splan::read(splan::write(g,80,&losses));
        require(back.sheets[0].items[0].pen==g.sheets[0].items[0].pen&&back.sheets[0].items[1].pen==g.sheets[0].items[1].pen&&back.sheets[0].items[2].font.strikeOut&&losses.isEmpty(),
                "second colour, stripes and struck out text written and read again");
        {const Record file=readFile(splan::write(g,80));const Record &r=file.lists["sheets"][0].lists["children"][0];
            require(r.integer("twoColour")==1&&r.integer("colour2")==0x008000&&r.integer("inner")==1&&r.integer("innerColour")==0x030201&&r.integer("cross")==1&&r.integer("crossColour")==0x060504,"the fields of the line");
            require((file.lists["sheets"][0].lists["children"][2].integer("fontStyle")&8)==8,"struck out: bit 8 of the font style");}
        losses.clear();const Document seven=splan::read(splan::write(g,70,&losses));
        const Pen &kept=seven.sheets[0].items[0].pen;
        require(kept.twoColour&&kept.color2==QColor(0,128,0)&&!kept.inner&&!kept.cross&&seven.sheets[0].items[2].font.strikeOut,"sPlan 7: second colour and struck out text kept");
        require(losses.join(' ').contains("Querstreifen von 2 Elementen"),"its losses: the stripes of two elements");}
    {Document moved=d;Item &k=moved.sheets[0].items[5].children[2];require(k.type==ItemType::Contact,"the contact");k.pin=QPointF(5,0);
        QStringList losses;require(splan::write(moved,80,&losses)==fixture.bytes&&losses.join(' ').contains("Anschlusspunkte"),"another connection point is not written and counts as lost");}

    // --- Changes: moved, turned, retyped; the other objects keep their bytes
    {Document c=d;Sheet &t=c.sheets[0];
        move(t.items[1],QPointF(10,0));t.items[2].text=QStringLiteral("Neu");
        rotate(t.items[5],t.items[5].pos,90);t.items[5].value=QStringLiteral("22k");
        const QByteArray bytes=splan::write(c,80);
        require(bytes.contains(fixture.line)&&!bytes.contains(fixture.rectangle),"unchanged records stay, changed ones are written anew");
        const Document back=splan::read(bytes);const Sheet &b=back.sheets[0];
        require(near(b.items[1].centre,{90,30})&&b.items[1].corners==Corners::Round&&near(b.items[1].corner,25),"the moved rectangle");
        require(b.items[2].text=="Neu"&&b.items[2].align==Align::Centre&&near(b.items[2].pos,{120,50}),"the new text keeps its place");
        require(near(b.items[5].pos,{290,105},1e-3)&&b.items[5].value=="22k"&&b.items[5].designator=="R1","the turned component keeps its insertion point");
        bool body=false;for(const auto &x:b.items[5].children)if(x.type==ItemType::Rectangle)body=near(placement(b.items[5]).map(x.centre),{290,85},1e-3);
        require(body,"its parts turned about the insertion point");}

    // --- Broken and foreign files
    for(int cut:{15,40,200,int(fixture.bytes.size())-3}){
        // The place named is where the value begins that runs past the end.
        const QString message=rejection([&]{splan::read(fixture.bytes.left(cut));});
        const auto m=QRegularExpression(QStringLiteral("(\\d+)\\D*$")).match(message);
        require(m.hasMatch()&&m.captured(1).toInt()<=cut&&m.captured(1).toInt()>cut-40,"a cut file is refused with the place of the damage");
    }
    require(!rejection([&]{splan::read(fixture.bytes.left(30));}).isEmpty(),"a short file is refused");
    {QByteArray b=fixture.bytes;b[b.indexOf(fixture.line)]=char(13);require(!rejection([&]{splan::read(b);}).isEmpty(),"an unknown object is refused");}
    {QByteArray b=fixture.bytes;b.replace(6,2,"60");require(rejection([&]{splan::read(b);}).contains("6,0")||rejection([&]{splan::read(b);}).contains("6.0"),"sPlan 6 files are refused with their version");}
    require(!rejection([&]{splan::read("not a schematic");}).isEmpty(),"foreign files are refused");

    // --- A component inside a component: a group that keeps its bytes
    {const QByteArray bytes=nested8();QStringList found;const Document n=splan::read(bytes,&found);
        require(n.sheets[0].items.size()==1&&n.sheets[0].items[0].type==ItemType::Component&&n.sheets[0].items[0].designator=="A1","the outer component");
        const Item *inner=nullptr;for(const auto &c:n.sheets[0].items[0].children)if(c.type==ItemType::Group)inner=&c;
        require(inner&&inner->children.size()==3,"the inner one as a group of its parts and texts");
        require(inner->children[1].text=="K1"&&inner->children[1].visible&&inner->children[2].text=="24V"&&!inner->children[2].visible,"its designator shown, its value hidden as in the file");
        require(found.join(' ').contains("anderen Bauteilen"),"named in the partial preview");
        require(splan::write(n,80)==bytes&&splan::write(n,80,nullptr,false)==bytes,"written back byte for byte");
        const Document own=decode(encode(n));
        Document changed=n;Item &group=changed.sheets[0].items[0].children[1];require(group.type==ItemType::Group,"the group is the second part");
        schematic::move(group.children[0],QPointF(1,0));
        QStringList losses;const QByteArray written=splan::write(changed,80,&losses);
        require(losses.join(' ').contains("Gruppen"),"a changed inner component is written as a group, with a note");
        QStringList again;const Document back=splan::read(written,&again);
        require(!again.join(' ').contains("anderen Bauteilen")&&back.sheets[0].items[0].children[1].type==ItemType::Group,"read again as a group");}

    // --- Pages of sPlan libraries
    {QStringList found;const LibraryPage page=splan::readLibrary(library8(),&found);
        require(page.name==QString::fromUtf8("Widerstände\rResistors\rRésistances")&&localized(page.name)==QString::fromUtf8("Widerstände"),"the page's name in all its languages");
        require(page.entries.size()==2,"a component and a clip");
        const LibraryEntry &r=page.entries[0];
        require(r.symbol.type==ItemType::Component&&r.symbol.pos==QPointF()&&r.symbol.designator=="R?"&&localized(r.caption)=="Widerstand","the component around its insertion point, with its caption");
        const Item *image=nullptr;for(const auto &c:r.symbol.children)if(c.type==ItemType::Image)image=&c;
        require(image&&r.resources.size()==1&&r.resources.contains(image->resource),"the picture goes with the symbol");
        require(contacts(r.symbol).size()==1&&contacts(r.symbol)[0]->name=="1","its contact");
        const LibraryEntry &clip=page.entries[1];
        require(clip.symbol.type==ItemType::Group&&near(clip.symbol.children[0].points[0],{0,0})&&near(clip.symbol.children[0].points[1],{20,0}),"the clip around its reference point");
        require(found.join(' ').contains("Kontakte"),"notes as for files");
        const LibraryPage seven=splan::readLibrary(library7());
        require(seven.name=="Seite"&&seven.entries.size()==1&&seven.entries[0].symbol.designator=="S1"&&seven.entries[0].caption=="Schalter","a page of a sPlan 7 library");
        require(rejection([&]{splan::readLibrary(library8().left(200));}).contains("Byte"),"a cut page is refused with its place");
        require(!rejection([&]{splan::readLibrary("not a library");}).isEmpty(),"foreign files are refused");
        // In the library folder, next to own pages.
        QTemporaryDir dir;QDir(dir.path()).mkpath("Elektro/Bauteile");
        {QFile f(dir.filePath("Elektro/Bauteile/LIB1.LIB"));require(f.open(QIODevice::WriteOnly)&&f.write(library8())>0,"cannot write the page");}
        const auto pages=folderPages(dir.path());
        require(pages.size()==1&&pages[0].folder=="Elektro/Bauteile"&&pages[0].entries.size()==2&&pages[0].file.endsWith("LIB1.LIB"),"a sPlan page in the library folder");}

    // --- Writing pages of sPlan libraries: unchanged byte for byte (also with records that are no entries), a changed
    // entry from its fields in its place, new entries in rows; new pages of both versions
    {for(const QByteArray &bytes:{library8(),library7()})require(splan::writeLibrary(splan::readLibrary(bytes),bytes,0)==bytes,"an unchanged page written back byte for byte");
        {Record file=splan::readLibraryFile(library8());Record line=file.lists["children"][1].lists["children"][0];file.lists["children"].insert(1,line);
            const QByteArray withLine=splan::writeLibraryFile(file,80);const LibraryPage page=splan::readLibrary(withLine);
            require(page.entries.size()==2&&splan::writeLibrary(page,withLine,0)==withLine,"a line beside the entries stays where it is");}
        const QByteArray bytes=library8();const LibraryPage page=splan::readLibrary(bytes);
        LibraryPage edited=page;edited.name=QStringLiteral("Umbenannt");edited.entries[0].caption=QStringLiteral("Neu");
        edited.entries<<LibraryEntry{QStringLiteral("Beispiel"),exampleSymbol(),{}};
        QStringList losses;const LibraryPage back=splan::readLibrary(splan::writeLibrary(edited,bytes,0,&losses));
        require(back.name=="Umbenannt"&&back.entries.size()==3&&back.entries[0].caption=="Neu"&&back.entries[1].symbol.splan==page.entries[1].symbol.splan,"renamed, one entry changed, the clip kept");
        auto bare=[](Item i){std::function<void(Item&)> f=[&](Item &x){x.id.clear();x.splan.clear();x.caption.clear();for(auto &c:x.children)f(c);};f(i);return itemToJson(i);};
        require(bare(back.entries[0].symbol)==bare(page.entries[0].symbol),"the changed entry with its parts where they were");
        require(back.entries[2].symbol.type==ItemType::Component&&back.entries[2].caption=="Beispiel"&&back.entries[2].symbol.pos==QPointF(),"a new entry");
        LibraryPage fresh;fresh.name=QStringLiteral("Neue Seite");fresh.entries<<LibraryEntry{QStringLiteral("Beispiel"),exampleSymbol(),{}};
        for(int v:{80,70}){const QByteArray b=splan::writeLibrary(fresh,{},v);const LibraryPage p=splan::readLibrary(b);
            require(splan::version(b)==v&&p.name=="Neue Seite"&&p.entries.size()==1&&splan::writeLibrary(p,b,0)==b,"a new page, read and written again");}}
    // Children of a parent on a library page (as sPlan's "74xx (Parent-Child)"): hung on their parent by its key, at their
    // places relative to it; written back unchanged, changed, new and with their parent removed.
    {Item parent=exampleSymbol();parent.parent=true;parent.designator=QStringLiteral("IC?");parent.value=QStringLiteral("7400");
        Item gate=exampleSymbol();gate.designator=QStringLiteral("<PARENT_ID>-<CHILDNO>");gate.pos=QPointF(25.4,0);
        Item second=gate;second.pos=QPointF(25.4,12.7);
        LibraryPage family;family.name=QStringLiteral("Gatter");
        LibraryEntry e{QStringLiteral("7400"),parent,{}};e.children={gate,second};family.entries<<e<<LibraryEntry{QStringLiteral("Einzeln"),exampleSymbol(),{}};
        const QByteArray bytes=splan::writeLibrary(family,{},80);const LibraryPage read=splan::readLibrary(bytes);
        require(read.entries.size()==2&&read.entries[0].children.size()==2&&read.entries[1].children.isEmpty(),"the children with their parent, not as entries of their own");
        require(near(read.entries[0].children[1].pos,QPointF(25.4,12.7))&&read.entries[0].children[0].designator=="<PARENT_ID>-<CHILDNO>","at their places relative to the parent");
        require(splan::writeLibrary(read,bytes,0)==bytes,"written back byte for byte");
        {LibraryPage changed=read;changed.entries[0].children[1].value=QStringLiteral("NAND");changed.entries[0].caption=QStringLiteral("7400 neu");
            const LibraryPage back=splan::readLibrary(splan::writeLibrary(changed,bytes,0));
            require(back.entries[0].children.size()==2&&back.entries[0].children[1].value=="NAND"&&back.entries[0].children[0].splan==read.entries[0].children[0].splan&&back.entries[0].caption=="7400 neu",
                    "a changed child and parent, the other child unchanged");}
        {LibraryPage more=read;Item third=gate;third.pos=QPointF(50.8,0);more.entries[0].children<<third;
            const LibraryPage back=splan::readLibrary(splan::writeLibrary(more,bytes,0));require(back.entries[0].children.size()==3&&near(back.entries[0].children[2].pos,QPointF(50.8,0)),"a new child hung on its parent");}
        {LibraryPage fewer=read;fewer.entries.removeFirst();
            const LibraryPage back=splan::readLibrary(splan::writeLibrary(fewer,bytes,0));require(back.entries.size()==1&&back.entries[0].children.isEmpty(),"a parent removed with its children");}
        require(splan::readLibrary(splan::writeLibrary(family,{},70)).entries[0].children.size()==2,"also in sPlan 7");}

    // --- Title blocks of sPlan
    {const QByteArray form=form8();QMap<QString,Resource> pictures;
        const QList<Item> items=splan::readTitleBlock(form,&pictures);
        require(items.size()==2&&items[0].type==ItemType::Line&&items[1].text=="<PAGENAME>"&&!items[0].id.isEmpty(),"a title block's elements");
        require(splan::writeTitleBlock(items,pictures,80)==form,"written back byte for byte");
        QByteArray old=form;qToLittleEndian<qint32>(-60,old.data());require(rejection([&]{splan::readTitleBlock(old,nullptr);}).contains("6,0")||rejection([&]{splan::readTitleBlock(old,nullptr);}).contains("6.0"),"other versions are refused");
        require(!rejection([&]{splan::readTitleBlock(form.left(30),nullptr);}).isEmpty(),"a cut title block is refused");
        QTemporaryDir dir;const QString file=dir.filePath("form.sbk");{QFile f(file);require(f.open(QIODevice::WriteOnly)&&f.write(form)==form.size(),"cannot write the form");}
        Editor e;QString error;const int before=int(e.document().sheet().titleBlock.items.size());
        require(e.loadTitleBlock(file,&error)&&e.document().sheet().titleBlock.items.size()==2,"loaded into the current sheet");
        require(e.saveTitleBlock(dir.filePath("again.sbk"),&error),"saved");
        {QFile f(dir.filePath("again.sbk"));require(f.open(QIODevice::ReadOnly)&&f.readAll()==form,"saved as read");}
        e.undo();require(e.document().sheet().titleBlock.items.size()==before,"loading is undone in one step");
        // A symbol from a sPlan library brings its picture along.
        LibraryPage page=splan::readLibrary(library8());e.library()->setPages({page});
        e.library()->chosen(page.entries[0]);
        require(e.document().resources.contains(page.entries[0].resources.firstKey()),"the symbol's picture in the document");}

    // --- Keys: copies keep their original's, as in sPlan; a copy of a parent gets a key of its own
    {Document d=splan::read(nested8());Item copy=d.sheets[0].items[0];assignIds(copy);copy.id=newId();schematic::move(copy,QPointF(50,0));d.sheets[0].items<<copy;
        {const Record file=readFile(splan::write(d,80));const auto objects=file.lists["sheets"][0].lists["children"];
            require(objects.size()==2&&objects[0].integer("key")==4712&&objects[1].integer("key")==4712,"a copy keeps the key");}
        d.sheets[0].items[0].parent=true;d.sheets[0].items[1].parent=true;
        {const Record file=readFile(splan::write(d,80));const auto objects=file.lists["sheets"][0].lists["children"];
            require(objects[0].integer("key")==4712&&objects[0].integer("parent")==1&&objects[1].integer("key")!=4712&&objects[1].integer("key")!=0,"two parents, two keys");}}

    // --- Text links
    {const QByteArray bytes=links8();const Document l=splan::read(bytes);const auto &items=l.sheets[0].items;
        require(items.size()==3&&items[1].linkTarget==items[0].id&&items[0].linkable&&items[2].link=="https://example.org","a link to a text by its key, and an external link");
        require(splan::write(l,80)==bytes&&splan::write(l,80,nullptr,false)==bytes&&splan::write(decode(encode(l)),80)==bytes,"written back byte for byte");
        Document changed=l;changed.sheets[0].items[1].linkTarget.clear();
        const Document back=splan::read(splan::write(changed,80));require(back.sheets[0].items[1].linkTarget.isEmpty()&&!back.sheets[0].items[0].linkable,"a link removed");
        Document own=exampleDocument();Item to;to.type=ItemType::Text;to.text=QStringLiteral("hier");to.linkable=true;assignIds(to);
        Item from;from.type=ItemType::Text;from.text=QStringLiteral("dorthin");from.linkTarget=to.id;from.pos=QPointF(30,30);assignIds(from);
        own.sheets[1].items<<to;own.sheets[0].items<<from;
        const Document again=splan::read(splan::write(own,80));
        require(textWithId(again,again.sheets[0].items.last().linkTarget).item&&textWithId(again,again.sheets[0].items.last().linkTarget).sheet==1,"an own link written and read as sPlan 8");
        QStringList losses;splan::write(own,70,&losses);require(losses.join(' ').contains("interne Links"),"sPlan 7 has none");}

    // --- sPlan 7
    {const QByteArray seven=spl7();const Document e=splan::read(seven);
        require(e.sheets.size()==1&&e.sheets[0].name=="Blatt7"&&e.sheets[0].description==QString::fromUtf8("Ä7"),"sheet name and description in Windows-1252");
        require(e.sheets[0].items[0].text==QString::fromUtf8("Text ä")&&e.sheets[0].items[0].font.bold,"a text of sPlan 7");
        require(e.sheets[0].items[1].designator=="K1"&&e.sheets[0].items[1].caption=="Relais"&&e.sheets[0].items[1].extra.value(0)=="Zusatz","a component of sPlan 7");
        require(e.variables.size()==1&&e.variables[0].value=="Wert","variables of sPlan 7");
        require(splan::write(e,70)==seven&&splan::write(e,70,nullptr,false)==seven,"sPlan 7 written back byte for byte, with the bytes after the sheet name");
        Document renamed=e;renamed.sheets[0].name=QStringLiteral("Neu");const Document back=splan::read(splan::write(renamed,70));
        require(back.sheets[0].name=="Neu","a new sheet name in the buffer");}

    // --- Own documents written as sPlan 8 and 7, and read again
    for(int version:{80,70}){
        Document own=exampleDocument();Item label;label.type=ItemType::NetLabel;label.text=QStringLiteral("VCC");label.pos=QPointF(60,70);assignIds(label);own.sheets[0].items<<label;
        QStringList losses;const QByteArray bytes=splan::write(own,version,&losses);
        require(splan::version(bytes)==version,"written in the version asked for");
        const QString all=losses.join(' ');
        require(all.contains("Netznamen")&&all.contains("Raster")&&!all.contains("Anschlusspunkte"),"losses: net names, the grid rounded; the connection points are found again");
        const Document back=splan::read(bytes);
        require(back.sheets.size()==2&&back.sheets[1].width==420,"both sheets");
        const auto parts=components(back.sheets[0]);
        require(parts.size()==2&&parts[0]->designator=="U1"&&parts[1]->designator=="U2"&&near(parts[0]->pos,{50.8,63.5},1e-3),"the components with their insertion points");
        require(contacts(*parts[0]).size()==3&&contacts(*parts[0])[1]->name=="2","their contacts");
        {const auto was=contacts(*components(own.sheets[0])[0]),is=contacts(*parts[0]);bool same=true;
            for(int k=0;k<was.size();k++)same&=is[k]->hasPin&&near(is[k]->pin,was[k]->pin,1e-3);
            require(same,"with their connection points at the free line ends");}
        const Item *wire=nullptr;for(const auto &i:back.sheets[0].items)if(i.type==ItemType::Line&&i.points.size()==4)wire=&i;
        require(wire&&near(wire->points.first(),own.sheets[0].items[2].points.first(),1e-3),"the conductor");
        require(splan::write(back,version)==bytes,"written again, the same bytes");
    }

    // --- In the editor: open with the partial preview, save back while nothing is lost
    {QTemporaryDir dir;const QString file=dir.filePath("plan.spl8");{QFile f(file);require(f.open(QIODevice::WriteOnly)&&f.write(fixture.bytes)==fixture.bytes.size(),"cannot write the test file");}
        Editor e;QString error;
        require(e.openFile(file,&error)&&e.fileVersion()==80&&e.filePath()==file&&e.openNotes().size()==1,"opened with its notes");
        require(e.findChild<QWidget*>("noticeBar")&&!e.findChild<QWidget*>("noticeBar")->isHidden(),"the notes are shown above the sheet");
        e.change([](Document &d){d.sheets[0].items[2].text=QStringLiteral("Gespeichert");});
        require(e.save(),"saved into the sPlan file");
        {QFile f(file);require(f.open(QIODevice::ReadOnly),"cannot read the saved file");const Document back=splan::read(f.readAll());require(back.sheets[0].items[2].text=="Gespeichert","written back as sPlan 8");}
        require(e.exportSplan(dir.filePath("plan.spl7"),70,&error),"exported as sPlan 7");
        Editor other;require(other.openFile(dir.filePath("plan.spl7"),&error)&&other.fileVersion()==70,"and opened again");}
    return 0;
}
