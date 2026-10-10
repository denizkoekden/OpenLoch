#pragma once
#include "frontpanel.h"
#include "strokefont.h"
#include "x87.h"
#include <QList>
#include <QPoint>
#include <QPolygon>

// The outlines the original plots for the HPGL export. Its outline builders turn the points of an object into a
// polygon of whole file units (1/50 mm, y down from the panel's top left corner): every point is rounded on its own,
// corners become quadratic curves of 21 points each, cut chamfers or rounded corners. The arithmetic follows the
// original's x87 code step by step, so that points landing exactly between two units go the same way.
namespace openloch::frontdesigner {
struct ExtendedPoint {Extended x,y;};

// What the builders need of an object: its points in file units, the closed flag, the corner switch, form and size,
// and for arcs how the end is closed (pie or chord, through the centre).
struct PlotShape {
    QList<ExtendedPoint> points;
    bool closed=false;
    bool smooth=false;
    int cornerStyle=0;             // 0 B-spline, 1 chamfer, 2 round, 3 B-spline of arcs
    Extended cornerSize;
    int arcMode=-1;                // arcs: 0 open, 1 pie, 2 chord; -1 for other objects
    ExtendedPoint arcCentre;
};
QPolygon plotPolygon(const PlotShape &shape);
// The 16 support points the original gives a circle: on rays at multiples of 22.5 degrees, pushed out by
// 1/cos(11.25 degrees) so that the B-spline through them touches the circle.
QList<ExtendedPoint> circlePoints(const ExtendedPoint &centre,const Extended &radius);

// A text in a stroke font of the original's own format (FHX) as it plots it: per character the strokes of the glyph,
// lines point by point and arcs in equal steps (their number grows with the square root of the radius), stretched to
// the width of the frame, sheared for slanted frames, mirrored and turned like the frame.
struct StrokeText {
    ExtendedPoint corners[4];      // top left, top right, bottom right, bottom left (file units)
    int height=0;                  // font height in file units
    bool flipX=false,flipY=false;  // the first and second mirror flag of the file: across and upside down
    QByteArray text;               // Windows-1252
    const frontpanel::StrokeFont *font=nullptr;
};
QList<QPolygon> plotStrokeText(const StrokeText &text);

// The same for the elements of a document, in whole file units. An element that is unchanged since it was read from
// an FPL file uses the values stored there; any other element the values the FPL writer would store for it.
// The outline of a line, polygon, rectangle, ellipse or arc (empty for other elements):
QPolygon plotOutline(const frontpanel::Element &e);
// The strokes of a text whose stroke font is installed in the original's own format; false for other texts.
bool plotText(const frontpanel::Element &e,QList<QPolygon> &strokes);
// The plunge point of a drill, and the circle it is milled out on with a smaller tool (mm). A diameter changed in the
// job list (mm, not negative) counts in whole file units, as the original keeps it.
QPoint plotDrill(const frontpanel::Element &drill);
QPolygon plotMilledDrill(const frontpanel::Element &drill,double tool,double diameter=-1);
// The outer rectangle: around the panel, half the tool (mm) outside its edge, from the bottom left corner to the
// right, up, left and down again.
QPolygon plotPanelOutline(const frontpanel::Panel &panel,double tool);
}
