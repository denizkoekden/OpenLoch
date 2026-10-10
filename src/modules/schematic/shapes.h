#pragma once
#include "model.h"
#include <optional>

// "Spezialformen": shapes drawn as a frame from one point to another and made of ordinary elements (lines, polygons,
// curves, texts, groups); afterwards they are edited as those. The direction the frame is drawn in turns brackets,
// arrows, triangles and the like.
namespace openloch::schematic {
enum class SpecialShape {
    RegularPolygon,Star,Grid,Wave,              // with settings ("Vieleck", "Stern", "Gitter", "Schwingung")
    CurlyBracket,RoundBracket,Triangle,RightTriangle,Square,Diamond,Parallelogram,Hexagon,Octagon,
    ArrowHorizontal,ArrowVertical,SpeechBubble,Lightning
};
enum class WaveKind {Sine,Square,Trapezoid,Triangle,Sawtooth};
// The settings of the first four shapes, as the dialog starts with them.
struct SpecialShapeOptions {
    int corners=5;double polygonOffset=0;bool polygonAsLine=false;
    int spikes=5;double spikeDepth=50;double starOffset=0;bool starAsLine=false;   // depth in per cent of the radius
    int columns=3,rows=3;bool frame=true,textFields=false;double textHeight=80;   // text height in per cent of a row
    WaveKind wave=WaveKind::Sine;int waves=1;
};
// The elements of a shape in the frame from `from` to `to`, in sheet coordinates, without ids. Closed shapes take
// outline and filling from `shape`, open ones (lines, curves) the outline of `line`; texts start from `text`.
QList<Item> specialShape(SpecialShape kind,QPointF from,QPointF to,const SpecialShapeOptions &options,
                         const Item &line,const Item &shape,const Item &text);
// The element as a line or a polygon ("Wandeln in Linie", "Wandeln in Polygon"): the points of its outline, curves and
// round corners as straight pieces at most a fortieth of a millimetre off; outline, filling and line ends kept where the
// new kind has them. A closed outline made a line ends where it began. For lines, polygons, Bézier curves, rectangles
// and ellipses (also arcs, pies and chords); nothing for other elements. As a Bézier curve ("Wandeln in Kurve") the
// outline keeps its curves and straight pieces become curves with their control points on them.
std::optional<Item> convertedTo(const Item &item,ItemType type);
}
