#include "icons.h"
#include <QHash>
#include <QStyleHints>
#include <QPalette>
#include <QIconEngine>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygon>
#include <functional>
namespace openloch {
namespace {
// The Windows 16-colour palette of the original's bitmaps.
const QColor K(0,0,0),W(255,255,255),G(192,192,192),D(128,128,128),N(0,0,128),B(0,0,255),Y(255,255,0),O(128,128,0),R(255,0,0),M(128,0,0),
    E(0,128,0),L(0,255,0),T(0,128,128),C(0,255,255),P(128,0,128),X;  // X: none
// A small pixel canvas without smoothing; coordinates are whole pixels.
struct Pix {
    QImage image;QPainter p;
    explicit Pix(int size):image(size,size,QImage::Format_ARGB32){image.fill(Qt::transparent);p.begin(&image);p.setRenderHint(QPainter::Antialiasing,false);}
    void dot(int x,int y,QColor c){p.fillRect(x,y,1,1,c);}
    void fill(int x,int y,int w,int h,QColor c){p.fillRect(x,y,w,h,c);}
    void line(int x1,int y1,int x2,int y2,QColor c){p.setPen(QPen(c,1));p.drawLine(x1,y1,x2,y2);}
    void box(int x,int y,int w,int h,QColor inside,QColor border=K){p.fillRect(x,y,w,h,border);if(inside.isValid())p.fillRect(x+1,y+1,w-2,h-2,inside);else p.fillRect(x+1,y+1,w-2,h-2,Qt::transparent);}
    void frame(int x,int y,int w,int h,QColor c){line(x,y,x+w-1,y,c);line(x,y+h-1,x+w-1,y+h-1,c);line(x,y,x,y+h-1,c);line(x+w-1,y,x+w-1,y+h-1,c);}
    void ellipse(int x,int y,int w,int h,QColor inside,QColor border=K){p.setPen(border.isValid()?QPen(border,1):QPen(Qt::NoPen));p.setBrush(inside.isValid()?QBrush(inside):QBrush(Qt::NoBrush));p.drawEllipse(x,y,w-1,h-1);}
    void poly(const QPolygon &points,QColor inside,QColor border=K){p.setPen(border.isValid()?QPen(border,1):QPen(Qt::NoPen));p.setBrush(inside.isValid()?QBrush(inside):QBrush(Qt::NoBrush));p.drawPolygon(points);}
    void polyline(const QPolygon &points,QColor c){p.setPen(QPen(c,1));p.drawPolyline(points);}
    void text(int x,int y,int w,int h,const QString &t,QColor c,int pixels,bool bold=true){QFont f("Arial");f.setPixelSize(pixels);f.setBold(bold);f.setStyleStrategy(QFont::NoAntialias);p.setFont(f);p.setPen(c);p.drawText(QRect(x,y,w,h),Qt::AlignCenter,t);}
    QImage done(){p.end();return image;}
};
using Drawing=std::function<void(Pix&)>;
// --- small parts shared by several drawings
void page(Pix &q,int x,int y,int w,int h){q.poly(QPolygon({{x,y},{x+w-4,y},{x+w-1,y+3},{x+w-1,y+h-1},{x,y+h-1}}),W);q.polyline(QPolygon({{x+w-4,y},{x+w-4,y+3},{x+w-1,y+3}}),K);}
void magnifier(Pix &q,int x,int y,int r,QColor glass=C){q.ellipse(x,y,2*r+1,2*r+1,glass);q.ellipse(x+1,y+1,2*r-1,2*r-1,X,W);q.line(x+2*r-1,y+2*r-1,x+2*r+3,y+2*r+3,K);q.line(x+2*r,y+2*r-1,x+2*r+4,y+2*r+3,K);q.line(x+2*r-1,y+2*r,x+2*r+3,y+2*r+4,K);}
void board(Pix &q,int x,int y,int w,int h){q.fill(x,y,w,h,QColor(232,200,40));for(int i=y+1;i<y+h;i+=3)q.line(x,i,x+w-1,i,QColor(206,160,30));}
// --- toolbar icons, 16 × 16
const QHash<QString,Drawing> &icons16(){
    static const QHash<QString,Drawing> d{
    {"new",[](Pix &q){page(q,3,1,10,14);}},
    {"open",[](Pix &q){q.poly(QPolygon({{1,4},{4,4},{5,3},{9,3},{10,4},{12,4},{12,6},{1,6}}),O);q.poly(QPolygon({{1,6},{12,6},{12,13},{1,13}}),O);q.poly(QPolygon({{3,8},{15,8},{12,13},{1,13}}),Y);
        q.polyline(QPolygon({{9,1},{11,0},{13,1},{14,3}}),K);q.line(13,3,15,3,K);q.line(14,2,14,4,K);}},
    {"save",[](Pix &q){q.box(1,1,14,14,N);q.box(4,1,8,6,W);q.line(5,3,10,3,D);q.line(5,5,10,5,D);q.box(4,9,8,6,G);q.fill(9,10,2,4,N);}},
    {"print",[](Pix &q){q.box(4,1,8,6,W);q.box(1,6,14,7,G);q.line(2,12,13,12,D);q.fill(11,8,2,1,E);q.box(3,10,10,5,W);q.line(5,12,10,12,D);}},
    {"group",[](Pix &q){q.frame(1,1,14,14,D);for(int i=1;i<15;i+=2){q.dot(i,1,W);q.dot(i,14,W);q.dot(1,i,W);q.dot(14,i,W);}q.box(3,3,6,6,R);q.ellipse(7,7,7,7,B);}},
    {"ungroup",[](Pix &q){q.box(1,1,6,6,R);q.ellipse(9,9,6,6,B);q.line(10,2,14,6,D);q.line(14,2,10,6,D);}},
    {"front",[](Pix &q){q.box(1,1,9,9,W,D);q.box(6,6,9,9,B);}},
    {"back",[](Pix &q){q.box(6,6,9,9,B);q.box(1,1,9,9,W,D);}},
    {"copy",[](Pix &q){page(q,1,1,9,11);page(q,6,4,9,11);q.line(8,8,12,8,D);q.line(8,10,12,10,D);q.line(8,12,11,12,D);}},
    {"cut",[](Pix &q){q.line(5,1,10,9,K);q.line(10,1,5,9,K);q.line(6,1,10,8,D);q.ellipse(2,9,5,5,X,B);q.ellipse(9,9,5,5,X,B);q.dot(4,11,B);q.dot(11,11,B);}},
    {"paste",[](Pix &q){q.box(1,2,11,13,QColor(160,110,40));q.box(4,1,5,3,G);page(q,6,6,9,9);q.line(8,10,12,10,D);q.line(8,12,12,12,D);}},
    {"duplicate",[](Pix &q){q.box(1,1,8,8,G);q.box(4,4,8,8,Y);q.fill(12,10,3,1,E);q.fill(13,9,1,3,E);}},
    {"delete",[](Pix &q){q.box(4,4,8,11,G);q.box(3,2,10,2,D,K);q.fill(6,1,4,1,K);q.line(6,6,6,12,D);q.line(8,6,8,12,D);q.line(10,6,10,12,D);}},
    {"undo",[](Pix &q){q.polyline(QPolygon({{4,6},{8,4},{11,5},{13,8},{12,11},{9,13}}),N);q.polyline(QPolygon({{4,7},{8,5},{11,6},{12,8},{11,11},{9,12}}),N);q.poly(QPolygon({{1,7},{6,3},{6,10}}),N,N);}},
    {"redo",[](Pix &q){q.polyline(QPolygon({{11,6},{7,4},{4,5},{2,8},{3,11},{6,13}}),N);q.polyline(QPolygon({{11,7},{7,5},{4,6},{3,8},{4,11},{6,12}}),N);q.poly(QPolygon({{14,7},{9,3},{9,10}}),N,N);}},
    {"undolist",[](Pix &q){for(int i=0;i<3;i++){q.fill(1,3+i*4,2,2,B);q.line(5,4+i*4,14,4+i*4,K);}}},
    {"align-left",[](Pix &q){q.line(1,0,1,15,K);q.box(3,2,9,4,B);q.box(3,9,12,4,B);}},
    {"align-hcenter",[](Pix &q){q.line(7,0,7,15,K);q.box(3,2,9,4,B);q.box(1,9,13,4,B);}},
    {"align-right",[](Pix &q){q.line(14,0,14,15,K);q.box(4,2,9,4,B);q.box(1,9,12,4,B);}},
    {"align-top",[](Pix &q){q.line(0,1,15,1,K);q.box(2,3,4,9,B);q.box(9,3,4,12,B);}},
    {"align-vcenter",[](Pix &q){q.line(0,7,15,7,K);q.box(2,3,4,9,B);q.box(9,1,4,13,B);}},
    {"align-bottom",[](Pix &q){q.line(0,14,15,14,K);q.box(2,4,4,9,B);q.box(9,1,4,12,B);}},
    {"view-mono",[](Pix &q){q.ellipse(1,1,14,14,W);q.p.setPen(Qt::NoPen);q.p.setBrush(K);q.p.drawPie(1,1,13,13,90*16,180*16);q.ellipse(1,1,14,14,X);}},
    {"view-flip",[](Pix &q){board(q,4,5,8,6);q.frame(4,5,8,6,K);q.polyline(QPolygon({{2,9},{1,6},{2,3},{5,1},{9,1}}),B);q.poly(QPolygon({{9,0},{11,1},{9,2}}),B,B);q.polyline(QPolygon({{13,6},{14,9},{13,12},{10,14},{6,14}}),B);q.poly(QPolygon({{6,13},{4,14},{6,15}}),B,B);}},
    {"view-through",[](Pix &q){board(q,1,3,10,8);q.frame(1,3,10,8,K);q.fill(5,6,10,8,QColor(255,255,255,170));q.frame(5,6,10,8,D);}},
    {"view-xray",[](Pix &q){q.fill(1,1,14,14,QColor(40,40,60));q.ellipse(4,2,8,8,X,W);q.line(8,10,8,14,W);q.line(5,12,11,12,W);q.dot(6,5,W);q.dot(9,5,W);}},
    {"view-bitmaps",[](Pix &q){q.box(1,2,14,12,C);q.poly(QPolygon({{2,12},{6,6},{9,10},{11,8},{13,12}}),E,E);q.ellipse(10,3,3,3,Y,Y);}},
    {"view-free",[](Pix &q){board(q,1,1,14,14);q.fill(4,4,8,8,L);q.frame(1,1,14,14,K);}},
    {"view-potentials",[](Pix &q){q.ellipse(1,1,14,14,X,R);q.ellipse(3,3,10,10,X,R);q.fill(7,7,2,2,R);}},
    {"contour-normal",[](Pix &q){q.polyline(QPolygon({{2,14},{2,2},{14,2}}),K);}},
    {"contour-spline",[](Pix &q){q.polyline(QPolygon({{2,14},{2,8},{3,5},{5,3},{8,2},{14,2}}),K);}},
    {"contour-chamfer",[](Pix &q){q.polyline(QPolygon({{2,14},{2,7},{7,2},{14,2}}),K);}},
    {"contour-round",[](Pix &q){q.polyline(QPolygon({{2,14},{2,6},{3,4},{4,3},{6,2},{14,2}}),K);q.dot(3,5,D);q.dot(5,3,D);}},
    {"fill",[](Pix &q){q.poly(QPolygon({{3,6},{8,1},{13,6},{8,11}}),G);q.fill(9,8,4,1,B);q.fill(12,9,2,5,B);}},
    {"mill-normal",[](Pix &q){q.poly(QPolygon({{2,3},{13,3},{13,12},{2,12}}),G,D);}},
    {"mill",[](Pix &q){q.poly(QPolygon({{2,3},{13,3},{13,12},{2,12}}),X,R);q.box(9,0,4,9,G);q.poly(QPolygon({{9,9},{12,9},{10,13}}),D);}},
    {"zoom-in",[](Pix &q){magnifier(q,0,0,5);q.line(3,5,7,5,B);q.line(5,3,5,7,B);}},
    {"zoom-out",[](Pix &q){magnifier(q,0,0,5);q.line(3,5,7,5,B);}},
    {"zoom-board",[](Pix &q){magnifier(q,0,0,5);q.fill(3,3,5,5,QColor(232,200,40));}},
    {"zoom-all",[](Pix &q){magnifier(q,0,0,5);q.frame(2,2,7,7,B);}},
    {"zoom-marked",[](Pix &q){magnifier(q,0,0,5);for(int i=2;i<9;i+=2){q.dot(i,2,B);q.dot(i,8,B);q.dot(2,i,B);q.dot(8,i,B);}}},
    {"zoom-real",[](Pix &q){magnifier(q,0,0,5);q.text(1,1,9,9,"1:1",B,6,false);}},
    {"properties",[](Pix &q){q.box(1,1,14,14,W);q.fill(2,2,12,3,N);for(int y=7;y<14;y+=3){q.line(3,y,7,y,K);q.box(9,y-1,4,3,W,D);}}},
    {"rotate",[](Pix &q){q.polyline(QPolygon({{3,11},{2,8},{3,5},{6,2},{10,2},{13,5}}),B);q.poly(QPolygon({{11,5},{15,5},{13,8}}),B,B);q.box(5,7,7,7,Y);}},
    {"mirror",[](Pix &q){q.line(7,0,7,15,D);q.poly(QPolygon({{1,12},{6,3},{6,12}}),B);q.poly(QPolygon({{9,3},{14,12},{9,12}}),W,B);}},
    {"fit",[](Pix &q){q.frame(1,1,14,14,K);q.box(4,4,8,8,QColor(232,200,40));q.dot(2,2,K);q.dot(13,13,K);}},
    };
    return d;
}
// --- drawing tools, 20 × 20, named "tool-" and the tool
const QHash<QString,Drawing> &icons20(){
    static const QHash<QString,Drawing> d{
    {"tool-select",[](Pix &q){q.poly(QPolygon({{5,2},{5,16},{8,13},{11,19},{13,18},{10,12},{14,12}}),W);}},
    {"tool-zoom",[](Pix &q){magnifier(q,2,2,6);}},
    {"tool-polyline",[](Pix &q){q.polyline(QPolygon({{4,15},{8,6},{16,13}}),K);q.box(3,14,3,3,B,B);q.box(7,5,3,3,B,B);q.box(15,12,3,3,B,B);}},
    {"tool-ellipse",[](Pix &q){q.ellipse(2,5,17,11,X);}},
    {"tool-rectangle",[](Pix &q){q.frame(3,5,15,11,K);}},
    {"tool-polygon",[](Pix &q){q.poly(QPolygon({{4,15},{6,5},{15,4},{12,10},{16,16}}),X);for(QPoint c:{QPoint(4,15),QPoint(6,5),QPoint(15,4),QPoint(12,10),QPoint(16,16)})q.box(c.x()-1,c.y()-1,3,3,B,B);}},
    {"tool-text",[](Pix &q){q.text(1,1,18,18,"T",K,17);}},
    {"tool-wire",[](Pix &q){q.fill(2,2,16,16,K);q.fill(4,7,2,9,W);q.fill(14,7,2,9,W);q.fill(4,6,12,2,W);q.dot(5,15,G);q.dot(15,15,G);}},
    {"tool-pin",[](Pix &q){board(q,2,8,16,5);q.ellipse(6,6,9,9,G);q.ellipse(8,8,5,5,D,D);q.dot(9,9,W);}},
    {"tool-lead",[](Pix &q){q.fill(2,2,16,16,K);board(q,2,12,16,6);q.polyline(QPolygon({{6,16},{6,8},{9,5},{17,5}}),Y);q.polyline(QPolygon({{7,16},{7,8},{10,6},{17,6}}),Y);q.ellipse(4,14,5,4,W,G);}},
    {"tool-solder",[](Pix &q){board(q,2,6,16,9);q.ellipse(5,5,11,10,G,D);q.ellipse(7,7,4,3,W,W);}},
    {"tool-cut",[](Pix &q){board(q,2,5,16,10);q.fill(9,4,3,12,QColor(120,90,20));q.line(10,2,10,17,K);q.dot(9,3,K);q.dot(11,3,K);}},
    {"tool-drill",[](Pix &q){q.ellipse(3,3,15,15,W);q.p.setPen(Qt::NoPen);q.p.setBrush(K);q.p.drawPie(3,3,14,14,0,90*16);q.p.drawPie(3,3,14,14,180*16,90*16);q.ellipse(3,3,15,15,X);}},
    {"tool-potential",[](Pix &q){q.ellipse(3,3,15,15,X,R);q.ellipse(7,7,7,7,X,R);q.fill(10,10,1,1,R);}},
    {"tool-continuity",[](Pix &q){board(q,2,12,16,6);q.line(4,11,14,1,K);q.line(5,11,15,1,D);q.box(12,1,5,4,R);q.fill(3,11,3,2,G);q.polyline(QPolygon({{1,2},{4,5},{2,6},{6,9}}),Y);}},
    {"tool-origin",[](Pix &q){q.line(4,3,4,16,K);q.line(4,16,17,16,K);q.poly(QPolygon({{2,5},{4,1},{6,5}}),K,K);q.poly(QPolygon({{15,14},{19,16},{15,18}}),K,K);q.ellipse(9,4,3,3,R,R);q.ellipse(13,4,3,3,R,R);}},
    {"tool-pad",[](Pix &q){q.ellipse(3,3,15,15,QColor(232,200,40),O);q.ellipse(8,8,5,5,K,K);}},
    {"tool-resistor",[](Pix &q){q.line(1,10,4,10,K);q.box(4,7,12,7,W);q.line(16,10,19,10,K);}},
    {"tool-capacitor",[](Pix &q){q.line(1,10,8,10,K);q.fill(8,4,2,13,K);q.fill(11,4,2,13,K);q.line(13,10,19,10,K);}},
    {"tool-diode",[](Pix &q){q.line(1,10,6,10,K);q.poly(QPolygon({{6,4},{6,16},{13,10}}),K,K);q.fill(13,4,2,13,K);q.line(15,10,19,10,K);}},
    {"tool-ground",[](Pix &q){q.line(10,2,10,10,K);q.fill(3,10,15,2,K);q.fill(6,13,9,2,K);q.fill(8,16,5,2,K);}},
    };
    return d;
}
// --- pointers, 32 × 32, with their hot spots
struct Pointer {Drawing draw;QPoint hot;};
// A soldering iron from the hot spot at the lower left up to the upper right, outlined for every background.
void iron(Pix &q){
    q.p.setRenderHint(QPainter::Antialiasing,false);
    q.poly(QPolygon({{0,31},{2,26},{6,22},{9,25},{5,29}}),G,K);                           // tip
    q.poly(QPolygon({{6,22},{15,13},{18,16},{9,25}}),D,K);                                 // shaft
    q.poly(QPolygon({{15,13},{19,9},{22,12},{18,16}}),W,K);                                // collar
    q.poly(QPolygon({{19,9},{27,1},{30,4},{22,12}}),R,K);                                  // handle
    q.line(21,9,27,3,QColor(255,140,140));
}
void pencil(Pix &q){
    q.poly(QPolygon({{0,31},{2,25},{6,29}}),QColor(240,210,160),K);q.fill(1,29,2,2,K);   // point
    q.poly(QPolygon({{2,25},{22,5},{26,9},{6,29}}),Y,K);q.line(4,26,23,7,O);               // wood
    q.poly(QPolygon({{22,5},{25,2},{29,6},{26,9}}),QColor(240,150,170),K);                // eraser
}
const QHash<QString,Pointer> &pointers(){
    static const QHash<QString,Pointer> d{
    {"lupe",{[](Pix &q){q.ellipse(1,1,17,17,W);q.ellipse(3,3,13,13,QColor(190,230,255),K);q.dot(6,6,W);q.dot(7,5,W);
        q.poly(QPolygon({{15,17},{17,15},{27,25},{25,27}}),K,K);q.poly(QPolygon({{16,18},{18,16},{29,27},{27,29}}),D,K);}, {9,9}}},
    {"kolben",{[](Pix &q){iron(q);}, {0,31}}},
    {"kolbendraht",{[](Pix &q){iron(q);q.fill(2,2,12,3,W);q.frame(2,2,12,3,K);q.fill(2,4,3,7,W);q.frame(2,4,3,7,K);q.fill(11,4,3,7,W);q.frame(11,4,3,7,K);}, {0,31}}},
    {"kolbenanschluss",{[](Pix &q){iron(q);q.polyline(QPolygon({{3,13},{3,6},{6,3},{14,3}}),K);q.polyline(QPolygon({{4,13},{4,6},{7,4},{14,4}}),Y);q.ellipse(1,11,6,5,W,K);}, {0,31}}},
    {"kolbenpin",{[](Pix &q){iron(q);q.ellipse(2,2,10,10,G,K);q.ellipse(5,5,4,4,K,K);}, {0,31}}},
    {"loeten",{[](Pix &q){iron(q);q.ellipse(1,1,11,9,G,K);q.ellipse(3,3,4,3,W,W);}, {0,31}}},
    {"kolben2pol",{[](Pix &q){iron(q);q.ellipse(1,1,7,7,G,K);q.ellipse(9,1,7,7,G,K);}, {0,31}}},
    {"kolbentest",{[](Pix &q){iron(q);q.polyline(QPolygon({{2,1},{6,6},{3,7},{8,12}}),K);q.polyline(QPolygon({{3,1},{7,6},{4,7},{9,12}}),Y);}, {0,31}}},
    {"stift",{[](Pix &q){pencil(q);}, {0,31}}},
    {"stiftcirc",{[](Pix &q){pencil(q);q.ellipse(1,1,12,9,X,K);}, {0,31}}},
    {"stiftrect",{[](Pix &q){pencil(q);q.frame(1,1,12,9,K);}, {0,31}}},
    {"stiftpoly",{[](Pix &q){pencil(q);q.poly(QPolygon({{1,9},{3,1},{12,2},{9,6},{12,10}}),X,K);}, {0,31}}},
    {"stifttext",{[](Pix &q){pencil(q);q.text(0,0,14,12,"T",K,12);}, {0,31}}},
    {"kreuz",{[](Pix &q){for(int w:{0,1}){const QColor c=w?K:W;const int o=w?0:1;q.fill(15-o,1,1+2*o,12,c);q.fill(15-o,19,1+2*o,12,c);q.fill(1,15-o,12,1+2*o,c);q.fill(19,15-o,12,1+2*o,c);}}, {15,15}}},
    {"hand",{[](Pix &q){q.poly(QPolygon({{9,30},{5,22},{3,14},{6,13},{9,18},{9,5},{12,5},{12,15},{13,3},{16,3},{16,15},{17,4},{20,4},{20,16},{21,7},{24,7},{24,22},{20,30}}),W,K);}, {14,16}}},
    {"finger",{[](Pix &q){q.poly(QPolygon({{10,31},{6,23},{3,16},{6,15},{10,20},{10,2},{14,2},{14,15},{15,12},{18,12},{18,16},{19,14},{22,14},{22,17},{23,16},{26,16},{26,24},{22,31}}),W,K);}, {12,2}}},
    };
    return d;
}
// Whole-pixel scaling keeps the pixel look on screens with 2 or 3 device pixels per point.
QIcon iconOf(const QImage &image){QIcon icon;for(int f:{1,2,3})icon.addPixmap(QPixmap::fromImage(image.scaled(image.size()*f,Qt::IgnoreAspectRatio,Qt::FastTransformation)));return icon;}
// For dark toolbars: dark pixels (black outlines, navy, maroon, dark green …) are brightened keeping their hue, so the
// symbols stay as readable as the original's on its light grey bars.
QImage forDarkBackground(QImage image){
    for(int y=0;y<image.height();y++){auto *line=reinterpret_cast<QRgb*>(image.scanLine(y));
        for(int x=0;x<image.width();x++){if(qAlpha(line[x])==0)continue;QColor c=QColor::fromRgba(line[x]);
            if(c.lightnessF()<.4){float h,s,l,a;c.getHslF(&h,&s,&l,&a);c=QColor::fromHslF(h,s,.72f+l*.4f,a);line[x]=c.rgba();}}}
    return image;
}
bool darkTheme(){
    if(QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark)return true;
    return qobject_cast<QGuiApplication*>(QCoreApplication::instance())&&QGuiApplication::palette().color(QPalette::Window).lightness()<110;
}
// Chooses the light or dark drawing whenever the icon is painted, so the symbols follow a change of the system appearance.
class PixelIconEngine final:public QIconEngine {
public:
    PixelIconEngine(QIcon light,QIcon dark):light(std::move(light)),dark(std::move(dark)){}
    void paint(QPainter *painter,const QRect &rect,QIcon::Mode mode,QIcon::State state) override{current().paint(painter,rect,Qt::AlignCenter,mode,state);}
    QPixmap pixmap(const QSize &size,QIcon::Mode mode,QIcon::State state) override{return current().pixmap(size,mode,state);}
    QPixmap scaledPixmap(const QSize &size,QIcon::Mode mode,QIcon::State state,qreal scale) override{return current().pixmap(size,scale,mode,state);}
    QSize actualSize(const QSize &size,QIcon::Mode mode,QIcon::State state) override{return current().actualSize(size,mode,state);}
    QList<QSize> availableSizes(QIcon::Mode mode,QIcon::State state) override{return light.availableSizes(mode,state);}
    QIconEngine *clone() const override{return new PixelIconEngine(light,dark);}
    QString key() const override{return QStringLiteral("openloch-pixel");}
private:
    QIcon light,dark;
    const QIcon &current() const{return darkTheme()?dark:light;}
};
}
QIcon openLochIcon(const QString &name){
    static QHash<QString,QIcon> cache;const auto it=cache.constFind(name);if(it!=cache.constEnd())return *it;
    QImage image;
    if(icons16().contains(name)){Pix q(16);icons16()[name](q);image=q.done();}
    else if(icons20().contains(name)){Pix q(20);icons20()[name](q);image=q.done();}
    const QIcon icon=image.isNull()?QIcon():QIcon(new PixelIconEngine(iconOf(image),iconOf(forDarkBackground(image))));
    cache.insert(name,icon);return icon;
}
QCursor openLochCursor(const QString &name){
    if(!pointers().contains(name))return QCursor(Qt::ArrowCursor);
    const auto &pointer=pointers()[name];Pix q(32);pointer.draw(q);const QImage image=q.done();
    // Pointers are drawn at 32 device-independent pixels; a 2× version keeps them sharp on high-density screens.
    QPixmap pixmap=QPixmap::fromImage(image.scaled(64,64,Qt::IgnoreAspectRatio,Qt::FastTransformation));pixmap.setDevicePixelRatio(2);
    return QCursor(pixmap,pointer.hot.x(),pointer.hot.y());
}
QStringList openLochIconNames(){return icons16().keys()+icons20().keys();}
QStringList openLochCursorNames(){return pointers().keys();}
}
