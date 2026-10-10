#include "pcbicons.h"
#include "icons.h"
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPolygon>
#include <QRegion>
#include <QStyleHints>
#include <algorithm>
#include <functional>

namespace openloch::pcb {
namespace {
// The Windows 16-colour palette, as in OpenLoch's other symbols.
const QColor K(0,0,0),W(255,255,255),G(192,192,192),D(128,128,128),B(0,0,255),N(0,0,128),E(0,128,0),L(0,255,0),R(255,0,0),M(128,0,0),C(0,255,255),T(0,128,128),Y(255,255,0),X;
struct Pix {
    QImage image;QPainter p;
    Pix():image(20,20,QImage::Format_ARGB32){image.fill(Qt::transparent);p.begin(&image);p.setRenderHint(QPainter::Antialiasing,false);}
    void fill(int x,int y,int w,int h,QColor c){p.fillRect(x,y,w,h,c);}
    void line(int x1,int y1,int x2,int y2,QColor c){p.setPen(QPen(c,1));p.drawLine(x1,y1,x2,y2);}
    void polyline(const QPolygon &points,QColor c){p.setPen(QPen(c,1));p.drawPolyline(points);}
    void poly(const QPolygon &points,QColor inside,QColor border=K){p.setPen(border.isValid()?QPen(border,1):QPen(Qt::NoPen));p.setBrush(inside.isValid()?QBrush(inside):QBrush(Qt::NoBrush));p.drawPolygon(points);}
    void ellipse(int x,int y,int w,int h,QColor inside,QColor border=K){p.setPen(border.isValid()?QPen(border,1):QPen(Qt::NoPen));p.setBrush(inside.isValid()?QBrush(inside):QBrush(Qt::NoBrush));p.drawEllipse(x,y,w-1,h-1);}
    QImage done(){p.end();return image;}
};
// A thick track: the centre line drawn three pixels wide.
void track(Pix &q,const QPolygon &points,QColor c){for(int d=-1;d<=1;d++){q.polyline(points.translated(0,d),c);q.polyline(points.translated(d,0),c);}}
const QHash<QString,std::function<void(Pix&)>> &drawings(){
    static const QHash<QString,std::function<void(Pix&)>> d{
        {"pcb-track",[](Pix &q){track(q,QPolygon({{2,16},{7,11},{12,11},{17,6}}),E);q.ellipse(1,13,5,5,L,K);q.ellipse(14,2,5,5,L,K);}},
        {"pcb-pad",[](Pix &q){q.ellipse(3,3,15,15,L,K);q.ellipse(8,8,5,5,K,K);}},
        {"pcb-smd",[](Pix &q){q.poly(QPolygon({{4,6},{15,6},{15,13},{4,13}}),B,N);q.line(5,7,13,7,C);}},
        {"pcb-circle",[](Pix &q){q.ellipse(2,2,17,17,X,E);q.ellipse(3,3,15,15,X,E);q.ellipse(4,4,13,13,X,K);}},
        {"pcb-area",[](Pix &q){q.poly(QPolygon({{2,15},{5,4},{11,7},{17,3},{17,16}}),E,K);q.line(4,13,7,7,L);}},
        {"pcb-measure",[](Pix &q){q.poly(QPolygon({{1,12},{18,5},{19,8},{2,15}}),Y,K);for(int i=0;i<5;i++){const int x=4+3*i;q.line(x,12-int(1.25*i),x+1,14-int(1.25*i),K);}}},
        {"pcb-via",[](Pix &q){q.ellipse(3,3,15,15,C,T);q.ellipse(8,8,5,5,K,K);}},
        {"pcb-keepout",[](Pix &q){const QPolygon shape({{2,15},{5,4},{11,7},{17,3},{17,16}});q.p.setClipRegion(QRegion(shape));
            for(int i=-20;i<20;i+=3)q.line(i,19,i+19,0,D);q.p.setClipping(false);q.poly(shape,X,K);}},
        {"pcb-mask",[](Pix &q){q.ellipse(3,3,15,15,R,M);q.ellipse(7,6,5,4,QColor(255,140,140),QColor(255,140,140));}},
        {"pcb-airwire",[](Pix &q){q.line(4,15,16,4,W);q.line(4,16,17,4,K);q.ellipse(1,12,6,6,L,K);q.ellipse(13,1,6,6,L,K);}},
        {"pcb-test",[](Pix &q){q.poly(QPolygon({{1,19},{4,13},{14,3},{17,6},{7,16}}),R,K);q.line(1,19,4,16,K);q.poly(QPolygon({{14,3},{16,1},{19,4},{17,6}}),G,K);}},
        {"pcb-shape",[](Pix &q){q.poly(QPolygon({{10,1},{18,6},{18,14},{10,19},{2,14},{2,6}}),X,E);q.poly(QPolygon({{10,4},{15,7},{15,13},{10,16},{5,13},{5,7}}),L,E);}},
        {"pcb-photo",[](Pix &q){q.fill(2,3,16,14,QColor(28,96,46));q.p.setPen(QPen(K,1));q.p.setBrush(Qt::NoBrush);q.p.drawRect(2,3,15,13);
            q.ellipse(4,6,5,5,G,D);q.ellipse(11,9,5,5,G,D);q.line(7,13,14,6,W);}},
        {"pcb-drc",[](Pix &q){q.fill(2,2,16,16,K);q.fill(4,6,12,2,L);q.fill(4,12,12,2,L);q.ellipse(6,5,8,10,X,R);q.ellipse(7,6,6,8,X,R);}},
        {"pcb-rubber-large",[](Pix &q){track(q,QPolygon({{6,13},{10,9},{14,9},{18,3}}),E);q.ellipse(2,9,9,9,L,K);q.ellipse(5,12,3,3,K,K);q.ellipse(0,7,13,13,X,R);}},
        {"pcb-rubber-small",[](Pix &q){track(q,QPolygon({{6,13},{10,9},{14,9},{18,3}}),E);q.ellipse(2,9,9,9,L,K);q.ellipse(4,11,5,5,X,R);}},
        {"pcb-rubber-off",[](Pix &q){track(q,QPolygon({{12,8},{14,8},{18,3}}),E);q.ellipse(2,9,9,9,L,K);q.ellipse(5,12,3,3,K,K);}},
        {"pcb-autoroute",[](Pix &q){q.ellipse(1,13,6,6,L,K);q.ellipse(13,1,6,6,L,K);track(q,QPolygon({{4,16},{4,10},{10,4},{16,4}}),E);q.line(3,19,9,19,R);q.line(11,0,17,0,R);}},
        {"pcb-crosshair",[](Pix &q){q.line(10,0,10,6,K);q.line(10,14,10,19,K);q.line(0,10,6,10,K);q.line(14,10,19,10,K);q.ellipse(8,8,5,5,X,R);}},
        {"pcb-snap",[](Pix &q){q.ellipse(5,5,11,11,L,K);q.ellipse(8,8,5,5,K,K);q.line(10,0,10,19,R);q.line(0,10,19,10,R);}},
        {"pcb-favourites",[](Pix &q){q.poly(QPolygon({{10,1},{12,7},{18,7},{13,11},{15,18},{10,14},{5,18},{7,11},{2,7},{8,7}}),Y,K);q.line(10,4,11,7,W);}},
        // Two layers mixed where they overlap.
        {"pcb-transparent",[](Pix &q){q.fill(2,2,11,11,B);q.fill(7,7,11,11,R);q.fill(7,7,6,6,QColor(255,0,255));q.p.setPen(QPen(K,1));q.p.setBrush(Qt::NoBrush);q.p.drawRect(2,2,10,10);q.p.drawRect(7,7,10,10);}},
        // A keep-out drawn as a rectangle.
        {"pcb-keepout-rect",[](Pix &q){q.p.setClipRect(3,5,14,10);for(int i=-20;i<20;i+=3)q.line(i,19,i+19,0,D);q.p.setClipping(false);q.p.setPen(QPen(K,1));q.p.setBrush(Qt::NoBrush);q.p.drawRect(3,5,13,9);}},
        // Mirrored top to bottom about a horizontal axis.
        {"pcb-mirror-vertical",[](Pix &q){q.poly(QPolygon({{4,8},{10,2},{16,8}}),B,N);q.poly(QPolygon({{4,12},{10,18},{16,12}}),X,N);q.line(1,10,18,10,R);}},
        // Bars aligned at their left ends.
        {"pcb-align",[](Pix &q){q.line(2,1,2,18,R);q.fill(3,3,13,4,E);q.fill(3,9,8,4,E);q.fill(3,15,11,3,E);}},
        // An element pulled onto the grid.
        {"pcb-align-grid",[](Pix &q){for(int i=2;i<20;i+=5){q.line(i,0,i,19,G);q.line(0,i,19,i,G);}q.ellipse(4,4,7,7,L,K);q.line(13,13,10,10,R);q.line(13,13,13,9,R);q.line(13,13,9,13,R);}},
        // Airwires taken away.
        {"pcb-airwires-remove",[](Pix &q){q.line(4,15,16,4,K);q.ellipse(1,12,6,6,L,K);q.ellipse(13,1,6,6,L,K);q.line(9,13,17,19,R);q.line(9,19,17,13,R);}},
        // Information about the project.
        {"pcb-info",[](Pix &q){q.ellipse(2,2,17,17,B,N);q.fill(9,5,3,2,W);q.fill(9,8,3,7,W);q.fill(8,14,5,1,W);}},
        // A scanned template under the board.
        {"pcb-template",[](Pix &q){q.fill(2,3,16,14,W);q.p.setPen(QPen(K,1));q.p.setBrush(Qt::NoBrush);q.p.drawRect(2,3,15,13);q.line(4,13,8,8,D);q.line(8,8,11,11,D);q.line(11,11,15,6,D);q.ellipse(5,5,3,3,D,D);}},
        // Angles to turn by.
        {"pcb-angle",[](Pix &q){q.line(3,16,17,16,K);q.line(3,16,14,5,K);q.p.setPen(QPen(R,1));q.p.setBrush(Qt::NoBrush);q.p.drawArc(-5,8,16,16,0,45*16);q.poly(QPolygon({{13,1},{18,3},{15,7}}),R,R);}},
        {"pcb-other-side",[](Pix &q){q.fill(2,8,16,4,E);q.line(2,8,17,8,K);q.line(2,11,17,11,K);q.poly(QPolygon({{6,1},{10,5},{2,5}}),B,N);q.poly(QPolygon({{14,19},{18,15},{10,15}}),L,E);}},
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

QIcon pcbIcon(const QString &name){
    if(!drawings().contains(name))return openLochIcon(name);
    Pix q;drawings()[name](q);QImage image=q.done();if(darkTheme())image=forDark(image);
    QIcon icon;for(int f:{1,2,3})icon.addPixmap(QPixmap::fromImage(image.scaled(image.size()*f,Qt::IgnoreAspectRatio,Qt::FastTransformation)));
    return icon;
}
QStringList pcbIconNames(){return drawings().keys();}
}
