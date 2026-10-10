#pragma once
#include "model.h"
#include <QColor>

class QPainter;

namespace openloch::pcb {
// Draws an element in one colour, in the painter's millimetre units, for the screen and for printing. `grow` widens
// it on every side (a clearance cut into a ground plane); pads take `padColour` when it is given. Hatched areas are
// drawn as their grid, texts from their strokes (a stroke of one point as a dot), lines without width one pixel wide.
void paintElement(QPainter &painter,const Element &element,const QColor &colour,double grow=0,const QColor &padColour={});
}
