#pragma once
#include "model.h"
#include <QList>

// The reference's "Spezialformen": regular polygons, spirals and drawing frames, each made of plain elements around
// the origin, ready to be placed.
namespace openloch::pcb {
// A regular polygon with `corners` corners on a circle of `radius`, the first corner `offset` degrees counter-clockwise
// from three o'clock: an area when filled, else a closed track of `width`. `rays` adds a track of the same width from
// the centre to each corner.
QList<Element> regularPolygon(int corners,double radius,double width,bool filled,int layer,double offset=0,bool rays=false);

// A spiral of `turns` (counted in quarters) from `start` radius outwards, `gap` between neighbouring turns, so that
// their centre lines lie `gap + width` apart. Round: half circles whose centres alternate between the origin and half
// a step to the right, starting at three o'clock and running counter-clockwise. Square: sides growing by half a step
// each, starting below the centre and running clockwise, as the reference draws it.
struct SpiralShape {
    double start=2,gap=2,width=.4,turns=6;
    bool square=false;
};
Element spiral(const SpiralShape &shape,int layer);
// The width the spiral covers from edge to edge, as the reference shows it: 2 start + (2 turns − 1)(gap + width) + width.
double spiralDiameter(const SpiralShape &shape);

// A drawing frame: a closed outline of `width` × `height` (its centre line), with a labelled band along the chosen
// sides; columns and rows divide the inside, the labels sit in the middle of their fields. All elements share one
// group when placed.
struct FrameShape {
    double width=90,height=50;
    int columns=8,rows=8;
    enum Sides {None=0,First=1,Second=2,Both=3};     // columns: top, bottom; rows: left, right
    int columnSides=Both,rowSides=Both;
    bool columnLetters=true,rowLetters=false;      // A, B, … or 1, 2, …
};
constexpr double frameBand=3.81,frameText=2.286,frameOutline=.4,frameLine=.2;
QList<Element> frame(const FrameShape &shape,int layer);
// The label of field `index` (from 0): A … Z, AA, AB, … or 1, 2, ….
QString frameLabel(int index,bool letters);
}
