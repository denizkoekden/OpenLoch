#pragma once
#include "model.h"
#include <QLineF>
#include <QPainterPath>

// The drawing of a dimension ("Bemaßung") from its points and settings, as the reference draws it: extension lines
// from the measured points to 2 mm beyond the dimension line, arrows at its ends (outside when the text and both
// arrows do not fit between them), the text above the line, readable; an angle as an arc about the vertex with its
// legs, the text outside the arc in its middle. Lines are thin (0.1 mm).
namespace openloch::schematic {
struct DimensionDrawing {
    QList<QLineF> extensions;       // extension lines, or the legs of an angle
    QPainterPath line;              // the dimension line or arc
    QList<QPolygonF> arrows;
    QList<Item> texts;              // the value and the tolerances, as texts
};
constexpr double dimensionLineWidth=.1,dimensionOvershoot=2,dimensionTextGap=.2;
// The value measured: the length times `scale`, or the angle in degrees.
double dimensionValue(const Item &dimension,double scale);
// What the dimension shows (without tolerances).
QString dimensionText(const Item &dimension,double scale);
// A number with up to `digits` decimals, without trailing zeros, with a point or a comma.
QString dimensionNumber(double value,int digits,bool decimalPoint);
DimensionDrawing dimensionDrawing(const Item &dimension,double scale);
}
