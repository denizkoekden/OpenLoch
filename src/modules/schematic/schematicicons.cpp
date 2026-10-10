#include "schematicicons.h"
#include "icons.h"
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPolygon>
#include <QStyleHints>
#include <functional>

namespace openloch::schematic {
namespace {
// The Windows 16-colour palette, as in OpenLoch's other symbols.
const QColor K(0,0,0),W(255,255,255),G(192,192,192),D(128,128,128),B(0,0,255),N(0,0,128),E(0,128,0),L(0,255,0),R(255,0,0),M(128,0,0),C(0,255,255),T(0,128,128),Y(255,255,0),O(128,128,0),X;
struct Pix {
    QImage image;QPainter p;
    Pix():image(20,20,QImage::Format_ARGB32){image.fill(Qt::transparent);p.begin(&image);p.setRenderHint(QPainter::Antialiasing,false);}
    void fill(int x,int y,int w,int h,QColor c){p.fillRect(x,y,w,h,c);}
    void dot(int x,int y,QColor c){p.fillRect(x,y,1,1,c);}
    void line(int x1,int y1,int x2,int y2,QColor c){p.setPen(QPen(c,1));p.drawLine(x1,y1,x2,y2);}
    void polyline(const QPolygon &points,QColor c){p.setPen(QPen(c,1));p.drawPolyline(points);}
    void poly(const QPolygon &points,QColor inside,QColor border=K){p.setPen(border.isValid()?QPen(border,1):QPen(Qt::NoPen));p.setBrush(inside.isValid()?QBrush(inside):QBrush(Qt::NoBrush));p.drawPolygon(points);}
    void rect(int x,int y,int w,int h,QColor inside,QColor border=K){p.setPen(border.isValid()?QPen(border,1):QPen(Qt::NoPen));p.setBrush(inside.isValid()?QBrush(inside):QBrush(Qt::NoBrush));p.drawRect(x,y,w-1,h-1);}
    void ellipse(int x,int y,int w,int h,QColor inside,QColor border=K){p.setPen(border.isValid()?QPen(border,1):QPen(Qt::NoPen));p.setBrush(inside.isValid()?QBrush(inside):QBrush(Qt::NoBrush));p.drawEllipse(x,y,w-1,h-1);}
    // Small letters from 3 × 5 pixel patterns.
    void letters(int x,int y,const char *text,QColor c){
        static const QHash<char,const char*> glyphs{{'A',"010101111101101"},{'B',"110101110101110"},{'C',"011100100100011"},{'1',"010110010010111"},{'2',"110001010100111"},
            {'3',"110001010001110"},{'R',"110101110101101"},{'?',"110001010000010"},{'x',"000101010101000"},{'N',"101111111111101"},{'a',"000011101101011"}};
        for(int k=0;text[k];k++){const char *g=glyphs.value(text[k],"000000000000000");
            for(int i=0;i<15;i++)if(g[i]=='1')dot(x+k*4+i%3,y+i/3,c);}
    }
    QImage done(){p.end();return image;}
};
const QHash<QString,std::function<void(Pix&)>> &drawings(){
    static const QHash<QString,std::function<void(Pix&)>> d{
        // Drawing modes
        {"sch-line",[](Pix &q){q.polyline(QPolygon({{2,16},{2,9},{11,9},{11,3},{17,3}}),K);q.polyline(QPolygon({{3,16},{3,10},{12,10},{12,4},{17,4}}),B);q.rect(1,15,4,4,W,K);q.rect(15,2,4,4,W,K);}},
        {"sch-junction",[](Pix &q){q.line(1,10,18,10,K);q.line(10,10,10,18,K);q.line(1,11,18,11,K);q.line(11,11,11,18,K);q.ellipse(7,7,7,7,K,K);q.dot(8,8,D);}},
        {"sch-bezier",[](Pix &q){q.polyline(QPolygon({{2,16},{4,11},{6,8},{9,6},{12,7},{14,10},{16,14},{18,16}}),K);q.line(2,16,6,4,D);q.line(18,16,14,4,D);q.rect(5,3,3,3,W,B);q.rect(13,3,3,3,W,B);}},
        {"sch-textbox",[](Pix &q){q.rect(1,2,18,16,W,K);for(int y:{5,8,11,14})q.line(3,y,y==14?11:16,y,D);q.letters(3,4,"Abc",K);}},
        {"sch-netlabel",[](Pix &q){q.line(1,13,18,13,B);q.line(1,14,18,14,B);q.letters(3,5,"N1",K);q.line(3,11,9,11,K);q.ellipse(1,12,4,4,K,K);}},
        {"sch-sheetref",[](Pix &q){q.line(1,10,6,10,B);q.line(1,11,6,11,B);q.poly(QPolygon({{6,6},{14,6},{18,10},{14,14},{6,14}}),Y,K);q.letters(8,8,"A",K);}},
        {"sch-contact",[](Pix &q){q.rect(8,2,10,16,W,K);q.line(1,6,7,6,K);q.line(1,13,7,13,K);q.letters(10,4,"1",R);q.letters(10,11,"2",R);q.dot(1,6,R);q.dot(1,13,R);}},
        {"sch-freehand",[](Pix &q){q.polyline(QPolygon({{2,15},{4,12},{5,13},{7,9},{9,11},{11,6},{13,8},{15,4},{17,6}}),K);}},
        {"sch-special",[](Pix &q){q.poly(QPolygon({{10,1},{12,7},{18,7},{13,11},{15,18},{10,14},{5,18},{7,11},{2,7},{8,7}}),Y,K);}},
        {"sch-dimension",[](Pix &q){q.line(2,4,2,16,K);q.line(17,4,17,16,K);q.line(3,10,16,10,K);q.poly(QPolygon({{3,10},{6,8},{6,12}}),K,K);q.poly(QPolygon({{16,10},{13,8},{13,12}}),K,K);q.letters(7,3,"12",B);}},
        {"sch-bitmap",[](Pix &q){q.rect(1,3,18,14,W,K);q.poly(QPolygon({{2,15},{7,8},{11,12},{13,10},{17,15}}),E,E);q.ellipse(12,5,4,4,Y,O);}},
        {"sch-measure",[](Pix &q){q.poly(QPolygon({{1,12},{18,5},{19,8},{2,15}}),Y,K);for(int i=0;i<5;i++){const int x=4+3*i;q.line(x,12-int(1.25*i),x+1,14-int(1.25*i),K);}}},
        // Toolbar
        {"sch-mirror-vertical",[](Pix &q){q.line(1,10,18,10,D);q.poly(QPolygon({{4,8},{15,8},{9,2}}),B,N);q.poly(QPolygon({{4,12},{15,12},{9,18}}),W,N);}},
        {"sch-autonum",[](Pix &q){q.rect(1,3,8,14,W,K);q.letters(2,5,"R1",K);q.letters(2,11,"R2",K);q.poly(QPolygon({{11,10},{16,10},{16,7},{19,11},{16,15},{16,12},{11,12}}),L,E);}},
        {"sch-partslist",[](Pix &q){q.rect(2,1,15,18,W,K);for(int y:{4,8,12,16}){q.line(4,y,6,y,K);q.line(8,y,14,y,D);}}},
        // The list of sheets, the properties panel and the problem button of the second toolbar.
        {"sch-pages",[](Pix &q){q.rect(5,1,13,15,W,K);q.rect(3,3,13,15,W,K);q.rect(1,5,13,14,W,K);for(int y:{9,12,15})q.line(3,y,11,y,D);}},
        {"sch-properties",[](Pix &q){q.rect(1,1,18,18,W,K);q.fill(2,2,16,3,B);for(int y:{8,12,16}){q.line(3,y,7,y,D);q.rect(9,y-2,8,4,W,D);}}},
        {"sch-problem",[](Pix &q){q.poly(QPolygon({{10,1},{19,18},{1,18}}),Y,K);q.fill(9,6,2,7,K);q.fill(9,15,2,2,K);}},
        {"sch-grid",[](Pix &q){for(int x=2;x<20;x+=4)for(int y=2;y<20;y+=4)q.dot(x,y,K);q.line(2,18,18,2,B);q.rect(1,17,3,3,R,R);}},
        {"sch-parentchild",[](Pix &q){q.rect(1,1,9,7,QColor(190,210,255),K);q.rect(10,12,9,7,QColor(255,200,200),K);q.polyline(QPolygon({{5,8},{5,15},{9,15}}),K);q.letters(3,2,"x",K);}},
        {"sch-titleblock",[](Pix &q){q.rect(1,1,18,18,W,K);q.rect(9,12,10,7,G,K);q.line(9,15,18,15,K);q.line(13,12,13,15,K);}},
        // Binoculars, for searching.
        {"sch-spread-horizontal",[](Pix &q){q.rect(1,6,4,8,B,K);q.rect(8,3,4,14,B,K);q.rect(15,6,4,8,B,K);q.line(3,18,17,18,K);q.line(3,17,3,19,K);q.line(10,17,10,19,K);q.line(17,17,17,19,K);}},
        {"sch-spread-vertical",[](Pix &q){q.rect(6,1,8,4,B,K);q.rect(3,8,14,4,B,K);q.rect(6,15,8,4,B,K);q.line(18,3,18,17,K);q.line(17,3,19,3,K);q.line(17,10,19,10,K);q.line(17,17,19,17,K);}},
        {"sch-search",[](Pix &q){q.rect(2,3,5,5,D,K);q.rect(13,3,5,5,D,K);q.ellipse(1,8,8,9,N,K);q.ellipse(11,8,8,9,N,K);q.rect(8,8,4,4,D,K);q.dot(4,11,C);q.dot(14,11,C);}},
        // A parent (blue) and its two children (red), joined by dotted lines.
        {"sch-parent-child",[](Pix &q){q.rect(6,1,8,6,QColor(150,190,255),K);q.rect(1,13,6,6,QColor(255,170,170),K);q.rect(13,13,6,6,QColor(255,170,170),K);
            for(int y=7;y<13;y+=2){q.dot(4,y,N);q.dot(15,y,N);}q.line(4,7,15,7,N);}},
        {"sch-component-editor",[](Pix &q){q.rect(4,4,12,12,QColor(214,234,255),K);q.line(1,10,4,10,K);q.line(16,10,18,10,K);q.ellipse(8,8,4,4,R,R);}},
        // Status bar switches
        {"sch-snap-grid",[](Pix &q){for(int x=2;x<20;x+=4)for(int y=2;y<20;y+=4)q.dot(x,y,D);q.rect(8,8,5,5,R,K);}},
        {"sch-snap-angle",[](Pix &q){q.line(2,17,18,17,K);q.line(2,17,14,5,K);q.polyline(QPolygon({{9,17},{9,14},{8,12}}),R);}},
        {"sch-snap-terminal",[](Pix &q){q.line(1,10,10,10,K);q.ellipse(6,6,9,9,X,R);q.ellipse(9,9,3,3,K,K);}},
        {"sch-rubberband",[](Pix &q){q.rect(11,6,8,8,W,K);q.polyline(QPolygon({{1,4},{6,4},{8,10},{11,10}}),B);q.dot(1,4,K);}},
        {"sch-text-rotate",[](Pix &q){q.letters(2,3,"A",K);q.p.save();q.p.translate(13,17);q.p.rotate(-90);q.p.restore();q.line(12,4,12,14,K);q.letters(14,8,"a",B);q.polyline(QPolygon({{4,12},{4,16},{8,16}}),R);}},
        {"sch-text-mirror",[](Pix &q){q.line(10,1,10,18,D);q.letters(3,7,"R",K);q.dot(14,7,B);q.dot(15,7,B);q.dot(13,8,B);q.dot(15,8,B);q.dot(14,9,B);q.dot(15,9,B);q.dot(13,10,B);q.dot(15,10,B);q.dot(13,11,B);q.dot(15,11,B);}},
    };
    return d;
}
bool darkTheme(){
    if(QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark)return true;
    return qobject_cast<QGuiApplication*>(QCoreApplication::instance())&&QGuiApplication::palette().color(QPalette::Window).lightness()<110;
}
// On dark bars, dark pixels are brightened keeping their hue.
QImage forDark(QImage image){
    for(int y=0;y<image.height();y++){auto *row=reinterpret_cast<QRgb*>(image.scanLine(y));
        for(int x=0;x<image.width();x++){if(!qAlpha(row[x]))continue;QColor c=QColor::fromRgba(row[x]);
            if(c.lightnessF()<.4){float h,s,l,a;c.getHslF(&h,&s,&l,&a);row[x]=QColor::fromHslF(h,s,.72f+l*.4f,a).rgba();}}}
    return image;
}
}

QIcon schematicIcon(const QString &name){
    if(!drawings().contains(name))return openLochIcon(name);
    Pix q;drawings()[name](q);QImage image=q.done();if(darkTheme())image=forDark(image);
    QIcon icon;for(int f:{1,2,3})icon.addPixmap(QPixmap::fromImage(image.scaled(image.size()*f,Qt::IgnoreAspectRatio,Qt::FastTransformation)));
    return icon;
}
QStringList schematicIconNames(){return drawings().keys();}
}
