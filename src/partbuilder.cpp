#include "partbuilder.h"
#include "legacy_writer.h"
#include <QJsonArray>
#include <cmath>
#include <limits>
namespace openloch::parts {
double round(double v){return std::nearbyint(v);} // Delphi's Round: halves go to the even neighbour
QJsonArray pair(QPointF p){return {qRound(p.x()),qRound(p.y())};}
// LochMaster turns a point about the origin with the distance rounded to whole units; positive angles turn
// counter-clockwise on screen.
QPointF turn(QPointF p,QPointF o,double angle){
    if(angle==0)return p;
    const double r=round(std::hypot(p.x()-o.x(),p.y()-o.y()));double phi=std::atan2(o.y()-p.y(),p.x()-o.x());if(phi<0)phi+=2*pi;
    return {round(o.x()+r*std::cos(phi+angle)),round(o.y()-r*std::sin(phi+angle))};
}
QJsonObject wire(int kind,int width,int pen,const QList<QPointF> &points){
    auto o=legacyObject("TDraht",kind,width);o["pen"]=pen;o["brush"]=0;QJsonArray path;for(auto p:points)path.append(pair(p));o["path"]=path;
    if(kind==9){QJsonArray first;first.append(pair(points.first()));o["points"]=first;} // not QJsonArray{pair(…)}, which older compilers copy
    return o;
}
// Contour vertices are rounded first and then turned, as in the original.
QJsonObject outline(int kind,const QList<QPointF> &points,QPointF o,double angle){
    QList<QPointF> turned;for(auto p:points)turned<<turn({round(p.x()),round(p.y())},o,angle);return wire(kind,10,0,turned);
}
void smooth(QJsonObject &o,int style,double size){o["flag2"]=true;o["style"]=style;o["rotation"]=size;}
// A filled body; the original stretches its picture over the corners stored in "anchors" (top left, top right,
// bottom left of the outline's bounding box) and draws nothing without them.
QJsonObject body(QJsonObject o,int brush,const QByteArray &bitmap){
    o["width"]=0;o["transparent"]=true;o["brush"]=brush;o["flag"]=true;if(bitmap.isEmpty())return o;
    double left=std::numeric_limits<double>::max(),top=left,right=-left,bottom=-left;
    for(auto v:o["path"].toArray()){const auto a=v.toArray();left=qMin(left,a[0].toDouble());right=qMax(right,a[0].toDouble());top=qMin(top,a[1].toDouble());bottom=qMax(bottom,a[1].toDouble());}
    o["bitmap"]=QString::fromLatin1(bitmap.toBase64());o["anchors"]=QJsonArray{left,top,right,top,left,bottom};return o;
}
// Labels carry the placeholders that the original's paste leaves in the document, so the allocated number shows.
// They run downwards (270°, counter-clockwise on screen) and keep collapsed corners like the original's fresh labels.
QJsonObject label(const QString &text,QPointF at,int height,int pen,double angle){
    auto l=legacyLabel(text,at,height);l["pen"]=pen;l["brush"]=0x808080;l["width"]=0;l["transparent"]=false;l["flag"]=true;
    l["font"]="ARIAL";l["text_flags"]=QJsonArray{true,false};l["text_height"]=angle;l["text_width"]=angle;
    return l;
}
QList<QPointF> rectangle(QPointF c,double w,double h){return {{c.x()-w/2,c.y()-h/2},{c.x()+w/2,c.y()-h/2},{c.x()+w/2,c.y()+h/2},{c.x()-w/2,c.y()+h/2}};}
// The "I": a shaft of width s and length l1 between two caps of width c and length l2.
QList<QPointF> shapeI(QPointF o,double s,double c,double l1,double l2){
    const double x=o.x(),y=o.y();
    return {{x-c/2,y-l1/2-l2},{x-c/2,y-l1/2},{x-s/2,y-l1/2},{x-s/2,y+l1/2},{x-c/2,y+l1/2},{x-c/2,y+l1/2+l2},
            {x+c/2,y+l1/2+l2},{x+c/2,y+l1/2},{x+s/2,y+l1/2},{x+s/2,y-l1/2},{x+c/2,y-l1/2},{x+c/2,y-l1/2-l2}};
}
// Side view of an electrolytic capacitor with the rolled-in groove near the bottom.
QList<QPointF> shapeElko(QPointF o,double d,double l){
    const double x=o.x(),y=o.y(),r=d/10;
    return {{x-d/2,y-l/2},{x+d/2,y-l/2},{x+d/2,y+l/2-4*r},{x+d/2-r,y+l/2-3*r},{x+d/2,y+l/2-2*r},{x+d/2,y+l/2},
            {x-d/2,y+l/2},{x-d/2,y+l/2-2*r},{x-d/2+r,y+l/2-3*r},{x-d/2,y+l/2-4*r}};
}
// Two leads, soldered at their outer end on the 2.54 mm grid: n grid steps from the centre.
QJsonArray leads(QPointF o,double length,double thickness){
    const double n=round(length/2/254+1);const int width=int(round(thickness));
    return {wire(9,width,0xC0C0C0,{{o.x(),o.y()-254*n},{o.x(),round(o.y()-length/2)}}),wire(9,width,0xC0C0C0,{{o.x(),o.y()+254*n},{o.x(),round(o.y()+length/2)}})};
}
}
