#pragma once
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QString>
namespace openloch::parts {
// Building blocks of LochMaster components in the reader's JSON form, shared by the object assistant and the
// open libraries. Coordinates in 1/100 mm; a grid hole is 254.
constexpr double pi=3.14159265358979323846;
double round(double v);                      // Delphi's Round: halves go to the even neighbour
QJsonArray pair(QPointF p);
// LochMaster turns a point about `o` with the distance rounded to whole units; positive angles turn counter-clockwise.
QPointF turn(QPointF p,QPointF o,double angle);
// A TDraht of `kind` (1 wire, 4 line, 6 rectangle, 7 polygon, 9 lead soldered at its first point, 11 pin).
QJsonObject wire(int kind,int width,int pen,const QList<QPointF> &points);
QJsonObject outline(int kind,const QList<QPointF> &points,QPointF o,double angle);
// Corner smoothing: style 0 B-spline, 1 chamfer, 2 rounding, `size` in 1/100 mm.
void smooth(QJsonObject &o,int style,double size);
// A filled body; a picture is stretched over the bounding box of the outline (anchors top left, top right, bottom left).
QJsonObject body(QJsonObject o,int brush,const QByteArray &bitmap);
// A text label; `angle` in radians counter-clockwise, the assistant's labels run downwards (3π/2).
QJsonObject label(const QString &text,QPointF at,int height,int pen,double angle=3*pi/2);
QList<QPointF> rectangle(QPointF centre,double w,double h);
QList<QPointF> shapeI(QPointF o,double s,double c,double l1,double l2);
QList<QPointF> shapeElko(QPointF o,double d,double l);
QJsonArray leads(QPointF o,double length,double thickness);
}
