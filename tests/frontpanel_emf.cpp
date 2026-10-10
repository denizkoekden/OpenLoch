// Tests of vector pictures: a panel written as EMF and played back looks like the panel itself; records for gradients,
// regions, alpha bitmaps, raster operations and the other bitmap records, paths of several figures and text in paths
// are played.
#include "emf.h"
#include "frontpanel.h"
#include "panelgeometry.h"
#include "panelrender.h"
#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace openloch::frontpanel;

namespace fptest {
void require(bool ok,const char *message);
bool near(double a,double b,double tolerance);
}
using fptest::require;using fptest::near;

namespace {
// Records of a hand-made picture: 1000 × 1000 logical units on 100 × 100 device pixels over 10 × 10 mm.
struct Picture {
    QByteArray b;
    Picture(){record(1,{0,0,99,99, 0,0,1000,1000, 0x464D4520,0x10000,0,0,0,0,0,0, 100,100, 10,10, 0,0,0});record(17,{8});record(9,{1000,1000});record(11,{100,100});}
    void u32(quint32 v){char c[4];qToLittleEndian(v,c);b.append(c,4);}
    void record(quint32 type,const QList<quint32> &values){u32(type);u32(8+4*values.size());for(auto v:values)u32(v);}
    void raw(quint32 type,const QByteArray &payload){u32(type);u32(quint32(8+payload.size()));b+=payload;}
    QByteArray done(){record(14,{0,0,20});qToLittleEndian(quint32(b.size()),b.data()+48);return b;}
};
QByteArray words(const QList<quint32> &values){QByteArray out;for(auto v:values){char c[4];qToLittleEndian(v,c);out.append(c,4);}return out;}
QByteArray halves(const QList<quint16> &values){QByteArray out;for(auto v:values){char c[2];qToLittleEndian(v,c);out.append(c,2);}return out;}
quint32 real(float f){quint32 v;std::memcpy(&v,&f,4);return v;}
// The picture in a frame from 10 to 30 mm on a white panel, at ten pixels per millimetre: logical 0…1000 lies on
// pixels 100…300.
QImage shown(const QByteArray &emf){
    Document d;Element pic=newElement(ElementType::Picture);pic.resource=d.addResource(emf,"emf");pic.frame=rectFrame(QRectF(10,10,20,20));pic.pen.style=PenStyle::None;
    Panel plain=newPanel("P",40,40);plain.color=Qt::white;plain.elements={pic};return renderPanel(d,plain,254);
}
QPoint at(int x,int y){return QPoint(100+x/5,100+y/5);}   // pixel of a logical point
bool close(const QColor &a,const QColor &b,int tolerance){return std::abs(a.red()-b.red())<=tolerance&&std::abs(a.green()-b.green())<=tolerance&&std::abs(a.blue()-b.blue())<=tolerance;}
QList<quint32> recordTypes(const QByteArray &emf){
    QList<quint32> out;qsizetype at=0;
    while(at+8<=emf.size()){const quint32 type=qFromLittleEndian<quint32>(emf.constData()+at),size=qFromLittleEndian<quint32>(emf.constData()+at+4);out<<type;if(size<8)break;at+=size;}
    return out;
}

void exportRoundTrip(){
    // A panel with outlines, fills, a gradient, a hatch, dashes, text and images, turned and transparent ones included.
    Document d;Panel &panel=d.panels[0];panel=newPanel("Muster",60,40);panel.color=QColor(0xf0,0xf0,0xe0);
    Element frame=newElement(ElementType::Rectangle);frame.points={{5,5},{55,5},{55,35},{5,35}};frame.pen=Pen{Qt::red,0.8,PenStyle::Solid};frame.fill.style=FillStyle::None;
    Element ellipse=newElement(ElementType::Ellipse);ellipse.center={15,15};ellipse.radiusX=6;ellipse.radiusY=4;ellipse.rotation=20;ellipse.pen=Pen{Qt::black,0.3,PenStyle::Solid};
    ellipse.fill=Fill{FillStyle::Solid,QColor(40,80,200),Qt::white,Gradient::None};
    Element ramp=newElement(ElementType::Polygon);ramp.points={{25,8},{45,8},{50,20},{25,20}};ramp.pen.style=PenStyle::None;ramp.fill=Fill{FillStyle::Solid,QColor(250,220,0),QColor(0,120,60),Gradient::Diagonal};
    Element hatch=newElement(ElementType::Rectangle);hatch.points={{8,24},{20,24},{20,32},{8,32}};hatch.pen=Pen{Qt::black,0,PenStyle::Solid};hatch.fill=Fill{FillStyle::Horizontal,Qt::darkGreen,Qt::white,Gradient::None};
    Element dash=newElement(ElementType::Line);dash.points={{24,30},{54,26}};dash.pen=Pen{Qt::blue,0.5,PenStyle::Dash};
    Element text=newElement(ElementType::Text);text.text="EMF";text.frame=rectFrame(QRectF(26,22,14,6));text.fill=Fill{FillStyle::Solid,Qt::black,Qt::black,Gradient::None};text.pen.style=PenStyle::None;
    QImage pixels(2,2,QImage::Format_ARGB32);pixels.setPixelColor(0,0,Qt::red);pixels.setPixelColor(1,0,Qt::green);pixels.setPixelColor(0,1,Qt::blue);pixels.setPixelColor(1,1,Qt::yellow);
    QByteArray png;{QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);pixels.save(&buffer,"PNG");}
    Element image=newElement(ElementType::Image);image.resource=d.addResource(png,"png");image.frame=rectFrame(QRectF(44,24,8,8));transformElement(image,rotationAbout({48,28},30));
    QImage holes(2,1,QImage::Format_RGB32);holes.setPixelColor(0,0,Qt::white);holes.setPixelColor(1,0,QColor(120,0,120));QByteArray png2;{QBuffer buffer(&png2);buffer.open(QIODevice::WriteOnly);holes.save(&buffer,"PNG");}
    Element see=newElement(ElementType::Image);see.resource=d.addResource(png2,"png");see.frame=rectFrame(QRectF(42,10,10,5));see.transparent=true;see.transparentColor=Qt::white;
    panel.elements={ellipse,ramp,hatch,dash,text,image,see,frame};
    RenderOptions o;o.machiningLook=false;
    EmfDevice device(QSizeF(panel.width,panel.height));device.setSmoothImages(true);{QPainter p;require(p.begin(&device),"painting into an EMF");p.scale(100,100);paintPanel(p,d,panel,o);p.end();}
    const QByteArray emf=device.data();
    require(isEmf(emf)&&qFromLittleEndian<quint32>(emf.constData()+48)==quint32(emf.size()),"EMF header and size");
    auto i32=[&](int at){return qFromLittleEndian<qint32>(emf.constData()+at);};
    require(i32(32)==6000&&i32(36)==4000&&i32(72)==i32(80)*100&&i32(76)==i32(84)*100,"EMF frame and reference device");
    const QList<quint32> types=recordTypes(emf);
    require(quint32(types.size())==qFromLittleEndian<quint32>(emf.constData()+52)&&types.last()==14,"EMF record count");
    for(quint32 type:{59u,60u,62u,64u,95u,81u,114u,21u})require(types.contains(type),"EMF record kinds");
    // Clipping and gradients are resolved into geometry, which every reader shows alike.
    for(quint32 type:{30u,67u,75u,118u})require(!types.contains(type),"EMF export without clip and gradient records");
    // Played back over the whole panel, the picture looks like the panel itself.
    Document back;Element pic=newElement(ElementType::Picture);pic.resource=back.addResource(emf,"emf");pic.frame=rectFrame(QRectF(0,0,panel.width,panel.height));pic.pen.style=PenStyle::None;
    Panel replay=newPanel("P",panel.width,panel.height);replay.color=Qt::white;replay.elements={pic};
    const QImage original=renderPanel(d,panel,254,o),copy=renderPanel(back,replay,254,o);require(original.size()==copy.size(),"EMF picture size");
    double sum=0;int far=0;QRect where;
    for(int y=0;y<original.height();y++)for(int x=0;x<original.width();x++){const QColor a=original.pixelColor(x,y),b=copy.pixelColor(x,y);
        const int diff=std::abs(a.red()-b.red())+std::abs(a.green()-b.green())+std::abs(a.blue()-b.blue());sum+=diff;if(diff>120){far++;where|=QRect(x,y,1,1);}}
    const double mean=sum/(original.width()*original.height());
    if(!(mean<1.5&&far<original.width()*original.height()/200))
        throw std::runtime_error(QString("EMF export does not look like the panel: mean difference %1, %2 pixels far off within x %3…%4, y %5…%6")
            .arg(mean).arg(far).arg(where.left()).arg(where.right()).arg(where.top()).arg(where.bottom()).toStdString());
    // Spots: the gradient's ends, the turned image, the transparent pixel.
    require(close(copy.pixelColor(260,90),original.pixelColor(260,90),12)&&close(copy.pixelColor(480,190),original.pixelColor(480,190),12),"gradient in the EMF export");
    require(close(copy.pixelColor(480,280),original.pixelColor(480,280),20)&&close(copy.pixelColor(445,125),original.pixelColor(445,125),12),"images in the EMF export");
}

void playback(){
    // Gradient fill: a horizontal rectangle from red to blue, a triangle with three colours.
    {
        Picture e;QByteArray g=words({0,0,0,0, 2,1,0});g+=words({0,0})+halves({0xff00,0,0,0xff00});g+=words({1000,500})+halves({0,0,0xff00,0xff00});g+=words({0,1});
        e.raw(118,g);
        QByteArray t=words({0,0,0,0, 3,1,2});t+=words({0,600})+halves({0xff00,0,0,0});t+=words({1000,600})+halves({0,0xff00,0,0});t+=words({0,1000})+halves({0,0,0xff00,0});t+=words({0,1,2});
        e.raw(118,t);
        const QImage img=shown(e.done());
        require(close(img.pixelColor(at(10,250)),QColor(255,0,0),12)&&close(img.pixelColor(at(990,250)),QColor(0,0,255),12)&&close(img.pixelColor(at(500,250)),QColor(128,0,128),12),"horizontal gradient fill");
        require(close(img.pixelColor(at(15,615)),QColor(255,0,0),24)&&close(img.pixelColor(at(950,605)),QColor(0,255,0),24)&&close(img.pixelColor(at(15,980)),QColor(0,0,255),24),"triangle gradient fill");
    }
    // Regions: filled with a brush, with the current brush, framed and inverted.
    {
        Picture e;e.record(39,{1,0,0x00ff00,0});e.record(39,{2,0,0x0000ff,0});e.record(37,{2});
        auto region=[](qint32 l,qint32 t,qint32 r,qint32 b){return words({32,1,1,16,quint32(l),quint32(t),quint32(r),quint32(b),quint32(l),quint32(t),quint32(r),quint32(b)});};
        e.raw(71,words({0,0,0,0,48,1})+region(0,0,500,500));                 // green, top left
        e.raw(74,words({0,0,0,0,48})+region(500,0,1000,500));                 // red (the selected brush), top right
        e.raw(72,words({0,0,0,0,48,1,50,50})+region(0,500,500,1000));         // a green border, bottom left
        e.raw(73,words({0,0,0,0,48})+region(500,500,1000,1000));              // white inverted to black, bottom right
        const QImage img=shown(e.done());
        require(img.pixelColor(at(250,250))==QColor(0,255,0)&&img.pixelColor(at(750,250))==QColor(255,0,0),"filled regions");
        require(img.pixelColor(at(20,750))==QColor(0,255,0)&&img.pixelColor(at(250,750))==QColor(Qt::white),"framed region");
        require(img.pixelColor(at(750,750))==QColor(Qt::black),"inverted region");
    }
    // Figures in one path stay apart: three squares, each opened with a move. Joined into one figure they would also
    // fill the triangle between their first corners.
    {
        Picture e;e.record(39,{1,0,0x000000,0});e.record(37,{1});e.record(59,{});
        for(const QPoint &corner:{QPoint(100,900),QPoint(800,900),QPoint(800,100)}){
            const quint32 x=quint32(corner.x()),y=quint32(corner.y()),down=corner.y()>500?y-100:y+100;
            e.record(27,{x,y});e.raw(6,words({x,std::min(y,down),x+100,std::max(y,down), 4, x+100,y, x+100,down, x,down, x,y}));e.record(61,{});
        }
        e.record(60,{});e.record(62,{0,0,0,0});
        const QImage img=shown(e.done());
        require(img.pixelColor(at(150,850))==QColor(Qt::black)&&img.pixelColor(at(850,850))==QColor(Qt::black)&&img.pixelColor(at(850,150))==QColor(Qt::black),"figures of a path");
        require(img.pixelColor(at(600,700))==QColor(Qt::white)&&img.pixelColor(at(700,500))==QColor(Qt::white),"figures of a path stay apart");
    }
    // Raster operations as Windows applies them, and the other bitmap records: a mask, a colour left out, a band of
    // scan lines, a parallelogram.
    {
        Picture e;e.record(39,{1,0,0x0000ff,0});e.record(37,{1});   // a red brush
        const QByteArray none;
        auto dib=[](int w,int h,const QList<quint32> &rgb){   // 24 bits, rows from the bottom
            QByteArray bits;for(int row=0;row<h;row++){QByteArray line;for(int x=0;x<w;x++){const quint32 c=rgb[row*w+x];line+=char(c&0xff);line+=char((c>>8)&0xff);line+=char((c>>16)&0xff);}
                while(line.size()%4)line+=char(0);bits+=line;}
            return std::pair{words({40,quint32(w),quint32(h)})+halves({1,24})+words({0,quint32(bits.size()),0,0,0,0}),bits};};
        auto mono=[](int w,const QList<int> &set){   // one row of 1 bit, black and white
            QByteArray bits(4,0);for(int x=0;x<w;x++)if(set[x])bits[x/8]=char(quint8(bits[x/8])|(0x80>>(x%8)));
            return std::pair{words({40,quint32(w),1})+halves({1,1})+words({0,4,0,0,2,0})+words({0x000000,0xffffff}),bits};};
        const QList<quint32> plain{real(1),real(0),real(0),real(1),real(0),real(0)};
        // STRETCHBLT of a 1 × 1 bitmap over the destination, or a BITBLT without a bitmap.
        auto blt=[&](quint32 x,quint32 y,quint32 w,quint32 h,quint32 rop,const std::pair<QByteArray,QByteArray> &bitmap){
            const bool with=!bitmap.first.isEmpty();const quint32 at=with?108:100;
            e.raw(with?77:76,words(QList<quint32>{0,0,0,0, x,y,w,h, rop, 0,0}+plain+QList<quint32>{0,0, with?at:0u,quint32(bitmap.first.size()),with?at+quint32(bitmap.first.size()):0u,
                quint32(bitmap.second.size())}+(with?QList<quint32>{1,1}:QList<quint32>{}))+bitmap.first+bitmap.second);};
        const auto white=dib(1,1,{0xffffff}),black=dib(1,1,{0x000000}),blue=dib(1,1,{0x0000ff});
        blt(0,0,250,250,0x00F00021,{none,none});blt(0,0,250,250,0x00660046,white);   // red, then white XOR: cyan
        blt(250,0,250,250,0x008800C6,black);                                         // black AND white: black
        blt(500,0,250,250,0x00F00021,{none,none});blt(500,0,250,250,0x00550009,{none,none});   // red inverted: cyan
        blt(750,0,250,250,0x005A0049,{none,none});                                  // red brush XOR white: cyan
        blt(0,250,250,250,0x00000042,{none,none});                                  // blackness
        blt(250,250,250,250,0x00F00021,{none,none});blt(250,250,250,250,0x00FF0062,{none,none});   // whiteness over red
        blt(500,250,250,250,0x00F00021,{none,none});blt(500,250,250,250,0x00330008,black);   // inverted black over red: white
        blt(750,250,250,250,0x00F00021,{none,none});blt(750,250,250,250,0x00EE0086,blue);    // blue OR red: magenta
        // MASKBLT, at the bitmap's own size (blown up by the world transformation): copied where the mask is set, left
        // as it is elsewhere.
        e.record(35,{real(250),real(0),real(0),real(250),real(0),real(500)});
        {const auto pair=dib(2,1,{0x0000ff,0x00ff00});const auto mask=mono(2,{1,0});const quint32 a=128,c=a+quint32(pair.first.size()+pair.second.size());
         e.raw(78,words(QList<quint32>{0,0,0,0, 0,0,2,1, 0xAACC0020, 0,0}+plain+QList<quint32>{0,0, a,quint32(pair.first.size()),a+quint32(pair.first.size()),quint32(pair.second.size()),
             0,0,0, c,quint32(mask.first.size()),c+quint32(mask.first.size()),quint32(mask.second.size())})+pair.first+pair.second+mask.first+mask.second);}
        e.record(36,{real(1),real(0),real(0),real(1),real(0),real(0),1});
        // TRANSPARENTBLT: white left out over red.
        {const auto pair=dib(2,1,{0xffffff,0x0000ff});blt(500,500,500,250,0x00F00021,{none,none});
         e.raw(116,words(QList<quint32>{0,0,0,0, 500,500,500,250, 0xffffff, 0,0}+plain+QList<quint32>{0,0, 108,quint32(pair.first.size()),108+quint32(pair.first.size()),quint32(pair.second.size()),2,1})
             +pair.first+pair.second);}
        // SETDIBITSTODEVICE in two bands of one scan line (a 2 × 2 bitmap, blown up by the world transformation).
        e.record(35,{real(100),real(0),real(0),real(100),real(0),real(750)});
        for(int band=0;band<2;band++){
            const auto rows=dib(2,1,{band?0x0000ffu:0x00ff00u,band?0x0000ffu:0x00ff00u});QByteArray info=rows.first;qToLittleEndian<qint32>(2,info.data()+8);
            e.raw(80,words({0,0,0,0, 0,0, 0,0,2,2, 76,quint32(info.size()),76+quint32(info.size()),quint32(rows.second.size()),0,quint32(band),1})+info+rows.second);
        }
        e.record(36,{real(1),real(0),real(0),real(1),real(0),real(0),1});
        // PLGBLT with a mask: only the blue half, onto a parallelogram.
        {const auto pair=dib(2,1,{0xff0000,0x0000ff});const auto mask=mono(2,{0,1});const quint32 a=140,c=a+quint32(pair.first.size()+pair.second.size());
         e.raw(79,words(QList<quint32>{0,0,0,0, 500,750,1000,750,500,1000, 0,0,2,1}+plain+QList<quint32>{0,0, a,quint32(pair.first.size()),a+quint32(pair.first.size()),quint32(pair.second.size()),
             0,0,0, c,quint32(mask.first.size()),c+quint32(mask.first.size()),quint32(mask.second.size())})+pair.first+pair.second+mask.first+mask.second);}
        const QImage img=shown(e.done());const QColor cyan(0,255,255);
        require(img.pixelColor(at(125,125))==cyan&&img.pixelColor(at(375,125))==QColor(Qt::black)&&img.pixelColor(at(625,125))==cyan&&img.pixelColor(at(875,125))==cyan,"raster operations");
        require(img.pixelColor(at(125,375))==QColor(Qt::black)&&img.pixelColor(at(375,375))==QColor(Qt::white)&&img.pixelColor(at(625,375))==QColor(Qt::white)
            &&img.pixelColor(at(875,375))==QColor(255,0,255),"raster operations without and with a bitmap");
        // Enlarged bitmaps are smoothed: their colours hold in the middle of their pixels.
        auto shows=[&](int x,int y,const QColor &c){return close(img.pixelColor(at(x,y)),c,16);};
        require(shows(125,625,QColor(0,0,255))&&shows(375,625,Qt::white),"bitmap through a mask");
        require(shows(625,625,QColor(255,0,0))&&shows(875,625,QColor(0,0,255)),"bitmap with a colour left out");
        require(shows(50,800,QColor(0,0,255))&&shows(150,900,QColor(0,255,0)),"bitmap in bands of scan lines");
        require(shows(625,875,Qt::white)&&shows(875,875,QColor(0,0,255)),"bitmap onto a parallelogram through a mask");
    }
    // Text in a path bracket becomes part of the path: filled with the brush, not with the text colour.
    {
        Picture e;
        QByteArray font(104,0);qToLittleEndian<quint32>(82,font.data());qToLittleEndian<quint32>(104,font.data()+4);qToLittleEndian<quint32>(1,font.data()+8);
        qToLittleEndian<qint32>(-600,font.data()+12);qToLittleEndian<qint32>(700,font.data()+28);const QString face="Arial";for(int i=0;i<face.size();i++)qToLittleEndian<quint16>(face[i].unicode(),font.data()+40+2*i);
        e.b+=font;e.record(37,{1});e.record(24,{0x0000ff});e.record(22,{0});
        e.record(39,{2,0,0x00ff00,0});e.record(37,{2});e.record(37,{0x80000008});e.record(59,{});
        const QByteArray chars=halves({'H'});e.raw(84,words({0,0,0,0, 1,0,0, 200,200, 1,76,0, 0,0,0,0, 0})+chars+QByteArray(2,0));
        e.record(60,{});e.record(62,{0,0,0,0});
        const QImage img=shown(e.done());int green=0,red=0;
        for(int y=100;y<300;y++)for(int x=100;x<300;x++){const QColor c=img.pixelColor(x,y);if(c.green()>200&&c.red()<60)green++;if(c.red()>200&&c.green()<60)red++;}
        require(green>200&&red==0,"text in a path bracket");
    }
    // Alpha blend: an opaque red pixel and a transparent one; a bitmap under a turned world transformation.
    {
        Picture e;
        const QByteArray info=words({40,2,1})+halves({1,32})+words({0,8,0,0,0,0});const QByteArray bits=words({0xffff0000,0x00000000});
        e.raw(114,words({0,0,0,0, 0,0,1000,500, 0x01ff0000, 0,0, real(1),real(0),real(0),real(1),real(0),real(0), 0,0, 108,40,148,8, 2,1})+info+bits);
        // A 1 × 1 blue bitmap drawn over 0…400 × 0…100 under a quarter turn about (700, 600): it covers x 600…700, y 600…1000.
        e.record(35,{real(0),real(1),real(-1),real(0),real(700),real(600)});
        const QByteArray info24=words({40,1,1})+halves({1,24})+words({0,4,0,0,0,0});const QByteArray blue=QByteArray("\xff\x00\x00\x00",4);
        e.raw(81,words({0,0,0,0, 0,0, 0,0,1,1, 80,40,120,4, 0,0x00CC0020, 400,100})+info24+blue);
        const QImage img=shown(e.done());
        require(close(img.pixelColor(at(250,250)),QColor(255,0,0),8)&&img.pixelColor(at(750,250))==QColor(Qt::white),"alpha blend");
        require(img.pixelColor(at(650,800))==QColor(0,0,255)&&img.pixelColor(at(800,650))==QColor(Qt::white),"bitmap under a world transformation");
    }
    // Stretch modes: a black and a white pixel over the width of the picture show sharp pixels, as GDI shows them in its
    // default mode; the halftone mode (4) blends them.
    {
        Picture e;const QByteArray info=words({40,2,1})+halves({1,24})+words({0,8,0,0,0,0});const QByteArray bits("\x00\x00\x00\xff\xff\xff\x00\x00",8);
        e.raw(81,words({0,0,0,0, 0,0, 0,0,2,1, 80,40,120,8, 0,0x00CC0020, 1000,500})+info+bits);
        e.record(21,{4});e.raw(81,words({0,0,0,0, 0,500, 0,0,2,1, 80,40,120,8, 0,0x00CC0020, 1000,500})+info+bits);
        const QImage img=shown(e.done());
        require(img.pixelColor(at(480,250))==QColor(Qt::black)&&img.pixelColor(at(520,250))==QColor(Qt::white),"an enlarged bitmap shows its pixels");
        const int grey=img.pixelColor(at(480,750)).red();require(grey>60&&grey<200,"the halftone mode blends an enlarged bitmap");
    }
}
}

void runEmfTests(){exportRoundTrip();playback();}
