#include "panelicons.h"
#include "icons.h"
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPolygon>
#include <functional>

namespace openloch::frontpanel {
namespace {
// The Windows 16-colour palette the program's symbols use; X means no colour.
const QColor K(0,0,0),W(255,255,255),G(192,192,192),D(128,128,128),N(0,0,128),B(0,0,255),Y(255,255,0),O(128,128,0),R(255,0,0),M(128,0,0),
    E(0,128,0),L(0,255,0),T(0,128,128),C(0,255,255),P(128,0,128),X;
const QColor panel(0xd8,0xd8,0xd8),mill(0x60,0x60,0x60);
struct Pix {
    QImage image;QPainter p;
    explicit Pix(int size):image(size,size,QImage::Format_ARGB32){image.fill(Qt::transparent);p.begin(&image);p.setRenderHint(QPainter::Antialiasing,false);}
    void dot(int x,int y,QColor c){p.fillRect(x,y,1,1,c);}
    void fill(int x,int y,int w,int h,QColor c){p.fillRect(x,y,w,h,c);}
    void line(int x1,int y1,int x2,int y2,QColor c){p.setPen(QPen(c,1));p.drawLine(x1,y1,x2,y2);}
    void box(int x,int y,int w,int h,QColor inside,QColor border=K){p.fillRect(x,y,w,h,border);p.fillRect(x+1,y+1,w-2,h-2,inside.isValid()?inside:QColor(Qt::transparent));}
    void frame(int x,int y,int w,int h,QColor c){line(x,y,x+w-1,y,c);line(x,y+h-1,x+w-1,y+h-1,c);line(x,y,x,y+h-1,c);line(x+w-1,y,x+w-1,y+h-1,c);}
    void ellipse(int x,int y,int w,int h,QColor inside,QColor border=K){p.setPen(border.isValid()?QPen(border,1):QPen(Qt::NoPen));p.setBrush(inside.isValid()?QBrush(inside):QBrush(Qt::NoBrush));p.drawEllipse(x,y,w-1,h-1);}
    void arc(int x,int y,int w,int h,int start,int span,QColor c){p.setPen(QPen(c,1));p.setBrush(Qt::NoBrush);p.drawArc(x,y,w-1,h-1,start*16,span*16);}
    void poly(const QPolygon &points,QColor inside,QColor border=K){p.setPen(border.isValid()?QPen(border,1):QPen(Qt::NoPen));p.setBrush(inside.isValid()?QBrush(inside):QBrush(Qt::NoBrush));p.drawPolygon(points);}
    void polyline(const QPolygon &points,QColor c){p.setPen(QPen(c,1));p.drawPolyline(points);}
    void text(int x,int y,int w,int h,const QString &t,QColor c,int pixels,bool bold=true,bool italic=false){QFont f("Arial");f.setPixelSize(pixels);f.setBold(bold);f.setItalic(italic);f.setStyleStrategy(QFont::NoAntialias);p.setFont(f);p.setPen(c);p.drawText(QRect(x,y,w,h),Qt::AlignCenter,t);}
    QImage done(){p.end();return image;}
};
using Drawing=std::function<void(Pix&)>;
void arrow(Pix &q,int x,int y,int dx,int dy,QColor c){q.line(x,y,x+dx*5,y+dy*5,c);if(dx){q.line(x+dx*5,y+dy*5,x+dx*3,y-2,c);q.line(x+dx*5,y,x+dx*3,y+2,c);}else{q.line(x,y+dy*5,x-2,y+dy*3,c);q.line(x,y+dy*5,x+2,y+dy*3,c);}}
void curl(Pix &q,bool clockwise){
    q.arc(3,3,11,11,clockwise?-30:210,clockwise?240:-240,B);q.arc(4,4,9,9,clockwise?-30:210,clockwise?240:-240,B);
    if(clockwise)q.poly(QPolygon({{11,2},{15,6},{10,7}}),B,B);else q.poly(QPolygon({{5,2},{1,6},{6,7}}),B,B);
}
const QHash<QString,Drawing> &icons16(){
    static const QHash<QString,Drawing> d{
    {"mirror-vertical",[](Pix &q){q.line(0,7,15,7,D);q.poly(QPolygon({{3,1},{12,6},{3,6}}),B);q.poly(QPolygon({{3,9},{12,9},{3,14}}),W,B);}},
    {"mirror-horizontal",[](Pix &q){q.line(7,0,7,15,D);q.poly(QPolygon({{1,12},{6,3},{6,12}}),B);q.poly(QPolygon({{9,3},{14,12},{9,12}}),W,B);}},
    {"distribute",[](Pix &q){for(int x:{1,6,11})q.box(x,4,4,8,B);q.line(0,14,15,14,K);q.line(0,13,0,15,K);q.line(15,13,15,15,K);}},
    {"align-grid",[](Pix &q){for(int i=1;i<16;i+=4)for(int j=1;j<16;j+=4)q.dot(i,j,D);q.box(5,5,7,7,B);q.dot(5,5,R);q.dot(4,5,R);q.dot(5,4,R);}},
    {"grid",[](Pix &q){q.fill(1,1,14,14,panel);for(int i=2;i<15;i+=3)for(int j=2;j<15;j+=3)q.dot(i,j,K);}},
    {"view-dimensions",[](Pix &q){q.line(1,3,1,13,K);q.line(14,3,14,13,K);q.line(2,8,13,8,K);q.line(2,8,4,6,K);q.line(2,8,4,10,K);q.line(13,8,11,6,K);q.line(13,8,11,10,K);q.text(3,0,10,7,"10",B,6,false);}},
    {"view-milled",[](Pix &q){q.fill(1,1,14,14,panel);q.box(3,3,10,10,X,mill);q.box(4,4,8,8,X,mill);q.frame(5,5,6,6,G);q.ellipse(11,0,5,5,mill,mill);}},
    {"view-engraved",[](Pix &q){q.box(1,1,14,14,D,K);q.polyline(QPolygon({{2,12},{5,3},{8,12}}),W);q.line(3,9,7,9,W);q.polyline(QPolygon({{9,12},{9,3},{12,3},{13,5},{12,7},{9,7}}),W);q.line(12,8,13,12,W);}},
    {"view-objects",[](Pix &q){q.box(1,6,8,8,Y);q.ellipse(6,1,9,9,R);q.poly(QPolygon({{10,15},{15,9},{15,15}}),B);}},
    {"view-texts",[](Pix &q){q.text(0,0,16,16,"Aa",K,11,true,true);}},
    {"view-mono",[](Pix &q){q.box(1,1,14,14,W);q.ellipse(3,3,10,10,X,K);q.line(3,12,12,3,K);}},
    {"rotate-left",[](Pix &q){curl(q,false);}},
    {"rotate-right",[](Pix &q){curl(q,true);}},
    {"proportional",[](Pix &q){q.box(1,3,14,10,W);q.text(1,3,14,10,"1:1",K,8);}},
    {"combine",[](Pix &q){q.box(1,1,14,14,Y);q.ellipse(4,4,8,8,W,K);}},
    {"uncombine",[](Pix &q){q.box(1,1,9,9,Y);q.ellipse(8,8,8,8,W,K);}},
    {"pen-draw",[](Pix &q){q.poly(QPolygon({{2,14},{3,11},{12,2},{14,4},{5,13}}),Y);q.line(2,14,4,12,K);q.fill(11,3,2,2,R);}},
    {"pen-mill",[](Pix &q){q.box(5,0,6,8,G);q.line(6,2,9,5,D);q.line(6,5,9,8,D);q.poly(QPolygon({{5,8},{10,8},{8,13}}),D);q.line(1,14,14,14,mill);q.line(1,15,14,15,mill);}},
    {"pen-engrave",[](Pix &q){q.poly(QPolygon({{9,1},{13,5},{6,12},{3,13},{4,10}}),G);q.line(3,13,2,14,K);q.polyline(QPolygon({{1,15},{5,15},{8,14}}),D);}},
    {"page-symbols",[](Pix &q){q.box(1,1,14,14,W);q.ellipse(3,3,5,5,R);q.poly(QPolygon({{9,7},{12,2},{15,7}}),Y);q.box(3,9,10,5,B);}},
    {"page-pens",[](Pix &q){for(int i=0;i<4;i++)q.fill(1,2+i*4,14,1+i,i==2?R:K);}},
    {"page-fills",[](Pix &q){q.box(1,1,7,7,R);q.box(8,1,7,7,X);for(int i=9;i<14;i+=2)q.line(i,2,i,6,B);q.box(1,8,7,7,Y);q.box(8,8,7,7,L);}},
    {"page-views",[](Pix &q){q.box(1,2,14,12,W);q.fill(2,3,12,2,N);q.frame(4,7,6,5,B);q.line(10,12,13,15,K);}},
    {"page-fonts",[](Pix &q){q.text(0,0,16,16,"F",K,14,true,false);q.line(10,13,15,13,B);}},
    {"list-add",[](Pix &q){q.fill(7,2,2,12,E);q.fill(2,7,12,2,E);}},
    {"list-remove",[](Pix &q){q.fill(2,7,12,2,R);}},
    {"list-up",[](Pix &q){q.poly(QPolygon({{8,2},{14,9},{2,9}}),B,B);q.fill(6,9,4,5,B);}},
    {"list-down",[](Pix &q){q.poly(QPolygon({{8,14},{14,7},{2,7}}),B,B);q.fill(6,2,4,5,B);}},
    {"scroll-left",[](Pix &q){q.poly(QPolygon({{3,8},{10,2},{10,14}}),B,B);}},
    {"scroll-right",[](Pix &q){q.poly(QPolygon({{13,8},{6,2},{6,14}}),B,B);}},
    {"scroll-up",[](Pix &q){q.poly(QPolygon({{8,3},{2,10},{14,10}}),B,B);}},
    {"scroll-down",[](Pix &q){q.poly(QPolygon({{8,13},{2,6},{14,6}}),B,B);}},
    {"swap",[](Pix &q){arrow(q,2,5,1,0,K);q.line(13,10,3,10,K);q.line(3,10,5,8,K);q.line(3,10,5,12,K);}},
    {"export-hpgl",[](Pix &q){q.box(1,1,14,14,W);q.text(1,1,14,7,"PLT",B,6);q.polyline(QPolygon({{3,13},{6,9},{9,12},{13,8}}),R);}},
    {"export-image",[](Pix &q){q.box(1,2,14,12,C);q.poly(QPolygon({{2,12},{6,6},{9,10},{11,8},{13,12}}),E,E);q.ellipse(10,3,3,3,Y,Y);}},
    {"panel",[](Pix &q){q.box(1,2,14,12,panel,D);q.ellipse(3,4,5,5,X,K);q.fill(10,5,3,1,K);q.fill(10,8,3,1,K);q.dot(4,11,K);q.dot(7,11,K);}},
    {"object-tree",[](Pix &q){q.box(1,1,5,4,Y);q.line(3,5,3,13,K);q.line(3,9,7,9,K);q.line(3,13,7,13,K);q.box(8,7,6,4,B);q.box(8,11,6,4,R);}},
    };
    return d;
}
const QHash<QString,Drawing> &icons20(){
    static const QHash<QString,Drawing> d{
    {"tool-rotate",[](Pix &q){q.box(6,6,9,9,Y);q.arc(2,2,16,16,100,250,B);q.arc(3,3,14,14,100,250,B);q.poly(QPolygon({{9,0},{13,3},{8,5}}),B,B);}},
    {"tool-arc",[](Pix &q){q.arc(2,4,17,17,0,180,K);q.box(1,11,3,3,B,B);q.box(16,11,3,3,B,B);q.dot(10,12,R);}},
    {"tool-circle",[](Pix &q){q.ellipse(2,2,17,17,X);q.dot(10,10,R);q.line(10,10,17,6,D);}},
    {"tool-regular",[](Pix &q){q.poly(QPolygon({{10,2},{17,6},{17,14},{10,18},{3,14},{3,6}}),X);q.dot(10,10,R);}},
    {"tool-scale",[](Pix &q){q.arc(2,4,17,17,20,140,K);for(int a=0;a<7;a++){const double r=a*140.0/6+20,s=std::sin(r*3.14159/180),c=std::cos(r*3.14159/180);q.line(10+int(8*c),12-int(8*s),10+int(10.5*c),12-int(10.5*s),K);}q.line(10,12,15,6,R);q.ellipse(9,11,3,3,K,K);}},
    {"tool-cutout",[](Pix &q){q.box(1,3,18,15,panel,D);q.box(5,6,10,9,mill,mill);q.ellipse(2,9,3,3,K,K);q.ellipse(15,9,3,3,K,K);}},
    {"tool-dimension",[](Pix &q){q.line(2,4,2,16,K);q.line(17,4,17,16,K);q.line(3,12,16,12,K);q.line(3,12,6,9,K);q.line(3,12,6,15,K);q.line(16,12,13,9,K);q.line(16,12,13,15,K);q.text(4,2,12,8,"mm",B,7,false);}},
    {"tool-image",[](Pix &q){q.box(2,3,16,14,C);q.poly(QPolygon({{3,16},{8,8},{12,13},{14,11},{17,16}}),E,E);q.ellipse(13,5,4,4,Y,Y);}},
    {"tool-drill-fp",[](Pix &q){q.ellipse(3,3,15,15,mill,K);q.line(6,10,14,10,G);q.line(10,6,10,14,G);}},
    {"tool-line",[](Pix &q){q.line(3,16,16,3,K);q.box(2,15,3,3,B,B);q.box(15,2,3,3,B,B);}},
    };
    return d;
}
QIcon iconOf(const QImage &image){QIcon icon;for(int f:{1,2,3})icon.addPixmap(QPixmap::fromImage(image.scaled(image.size()*f,Qt::IgnoreAspectRatio,Qt::FastTransformation)));return icon;}
}

QIcon panelIcon(const QString &name){
    static QHash<QString,QIcon> cache;const auto it=cache.constFind(name);if(it!=cache.constEnd())return *it;
    QIcon icon;
    if(icons16().contains(name)){Pix q(16);icons16()[name](q);icon=iconOf(q.done());}
    else if(icons20().contains(name)){Pix q(20);icons20()[name](q);icon=iconOf(q.done());}
    else icon=openLochIcon(name);
    cache.insert(name,icon);return icon;
}
QCursor panelCursor(const QString &name){return openLochCursor(name);}
QStringList panelIconNames(){return icons16().keys()+icons20().keys();}
}
