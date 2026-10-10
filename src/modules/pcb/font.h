#pragma once
#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QString>

// OpenLoch's own single-stroke font for texts on a board: every character is a few polylines drawn with a round pen,
// so the same strokes serve the screen, the copper and the Sprint-Layout files (which store texts as strokes).
// Glyphs sit on a grid with the capital height 8, x-height 5.5 and descenders down to -2.5; most are 4 wide.
namespace openloch::pcb {
struct Element;
// The stroke width for a text height: thin, normal and thick (thickness 0, 1, 2).
double textStrokeWidth(double height,int thickness);
// The polylines of a text in board coordinates (millimetres, y down). `start` is the left end of the baseline, as the
// centre of the strokes; the outer edge of a capital reaches `height` above the baseline minus half a stroke.
// Style 0 is narrow, 1 normal, 2 wide; mirrored texts run to the left; rotation is counter-clockwise about `start`.
QList<QPolygonF> textStrokes(const QString &text,QPointF start,double height,int style,int thickness,double rotation,bool mirrored);
// The advance of a text along its baseline in millimetres.
double textAdvance(const QString &text,double height,int style,int thickness);
// Rebuilds the strokes of a text element from its text and settings.
void updateStrokes(Element &text);
// Characters the font draws (others are drawn as a box), for tests.
QString fontCharacters();
}
