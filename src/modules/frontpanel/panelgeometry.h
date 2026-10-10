#pragma once
#include "frontpanel.h"
#include <QFont>
#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <QTransform>

namespace openloch::frontpanel {
// The drawn course of a contour with its corner style: smoothed corners are quadratic Bézier curves (a B-spline through
// the edge midpoints, or a curve between the two points `size` away from the corner), chamfers are straight cuts.
// An open B-spline starts in the middle of the first edge and ends in the middle of the last one.
QPainterPath contourPath(const QPolygonF &points,bool closed,const Contour &contour);
// The same course as points, each curve sampled with `steps` segments (milling, hit tests).
QPolygonF contourPoints(const QPolygonF &points,bool closed,const Contour &contour,int steps=16);

// Ellipses and arcs: the affine map from the unit circle (cos t, -sin t) to the element, and back from such a map.
// A point at parameter angle t (degrees) lies at ellipseMap(e).map(QPointF(cos t, -sin t)).
QTransform ellipseMap(const Element &e);
void setEllipseMap(Element &e,const QTransform &map,bool keepArc=true);
QPointF ellipsePoint(const Element &e,double degrees);

// Text, images and pictures fill a parallelogram given by three corners; this maps the rectangle (0,0)-(w,h) onto it.
QTransform frameMap(const QPolygonF &frame,QSizeF size);
QPolygonF frameCorners(const QPolygonF &frame);  // all four corners
QPolygonF rectFrame(const QRectF &r);           // top left, top right, bottom left
// The font of a text element at a pixel size of 100, and its natural width for a given text height in mm.
QFont textFont(const Element &e);
double naturalTextWidth(const Element &e,double height);
// The outline of the text, fitted into its frame; with an installed stroke font its strokes instead.
QPainterPath textPath(const Element &e);
// True for a text in a stroke font that is installed: it is drawn with the pen and has no fill.
bool strokeText(const Element &e);

// The geometric shape of an element (closed shapes closed, open ones open), without pen width.
QPainterPath elementPath(const Element &e);
// Area covered on the panel including half the pen width.
QRectF elementBounds(const Element &e);
QRectF elementsBounds(const QList<Element> &elements);
// Moves, turns, mirrors or stretches an element and everything inside it.
void transformElement(Element &e,const QTransform &map);
// True when `p` is on the element: within `tolerance` mm of its line, or inside a filled or closed area.
bool hitElement(const Element &e,QPointF p,double tolerance);
// Rotation about a point, counter-clockwise on screen for positive degrees (y points down).
QTransform rotationAbout(QPointF center,double degrees);
}
