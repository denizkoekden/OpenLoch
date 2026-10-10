#pragma once
#include <functional>
#include "model.h"
#include <QList>
#include <QPainterPath>
#include <QString>

// Copper of a board: which element has copper on which layer, the automatic ground plane ("AutoMasse"), electrical
// connections and the design rule check.
namespace openloch::pcb {
// The copper layers of a board in the order top to bottom: K1, I1, I2, K2 (the inner ones only on multilayer boards).
QList<int> copperLayers(const Board &board);
// The copper an element has on a layer, empty if none: through-plated pads are on every copper layer; keep-out areas
// and elements on silkscreen or outline layers carry no copper.
QPainterPath copperOn(const Element &element,int layer,const Board &board);
// The automatic ground plane of a copper layer: the working area without every element on that layer grown by its
// clearance, without drill holes and keep-out areas. Elements with clearance 0 merge with it; thermal pads keep their
// spokes. Hatched areas count as filled here: the plane stays out of their gaps, also at clearance 0, where it meets
// their border. Empty when the AutoMasse of that layer is off.
QPainterPath groundPlane(const Board &board,int layer);
// Spokes of a thermal pad on a layer (bars from the pad across its clearance), for drawing and the ground plane.
QPainterPath thermalSpokes(const Element &pad,int layer);
// The part of those spokes that joins the pad to the ground plane of a layer: the spokes within the pad's clearance.
// Empty for pads without copper on the layer or without clearance.
QPainterPath thermalBridges(const Element &pad,int layer,const Board &board);
// A shape grown by `distance` on every side, its corners rounded; computed in fine units, so that curves keep their form
// to a few micrometres.
QPainterPath grownShape(const QPainterPath &shape,double distance);

// Electrical connections: elements touching on a common copper layer, through-plated pads joining the layers, the
// ground plane of a layer joining the elements with clearance 0, and, if wanted, airwires. The result holds a
// component number per element, -1 for elements without copper.
QList<int> connections(const Board &board,bool airwires=false);
// The elements connected with the copper at a point of a layer (the first copper layer hit if `layer` is 0).
QList<int> connectedAt(const Board &board,QPointF at,int layer,bool airwires=false);
// Airwires whose pads are already joined by copper.
QList<std::pair<int,int>> routedAirwires(const Board &board);

// The point-to-point autorouter: the shortest track of `width` on `layer` from the centre of pad `from` to that of
// pad `to`, keeping `clearance` from all other copper on the layer and from drill holes, inside the working area,
// with nodes on a raster of `step` and as few bends as it can. Empty if there is no way.
QPolygonF autoroute(const Board &board,int from,int to,int layer,double width,double clearance,double step);

// The current a track can carry, as the reference estimates it: from the balance between the heat the current makes in
// the track's resistance and the heat its surface gives off, I = 3.675 · √(ΔT · t · b · (t + b)) with the track width b
// and the copper thickness t in millimetres, the temperature rise ΔT in kelvin and I in ampere (3.675 = 5.25 · 0.7).
// A rough guide only: air, neighbours and the board change it. widthForCurrent() solves the same for the width.
double maximumCurrent(double width,double copperMicrometres,double temperatureRise);
double widthForCurrent(double current,double copperMicrometres,double temperatureRise);

// Design rule check ("DRC"). Every check can be switched off; the limits are those the reference proposes.
struct Rules {
    bool clearanceOn=true;double clearance=.3;      // copper of different connections at least this far apart
    bool holeDistanceOn=true;double holeDistance=.8; // drill holes at least this far apart, from edge to edge
    bool minDrillOn=true;double minDrill=.3;
    bool maxDrillOn=true;double maxDrill=8;
    bool minTrackOn=true;double minTrack=.3;          // narrowest track or ring on copper
    bool minRingOn=true;double minRing=.3;            // narrowest copper ring around a drill hole
    bool minSilkOn=true;double minSilk=.15;           // narrowest line, ring or text stroke on the silkscreen
    bool silkOnPads=true;       // silkscreen must not cover solderable pads
    bool holesOnSmd=true;       // no drill hole on an SMD pad
    bool padsWithoutMask=false; // pads without their opening in the solder mask
    bool maskOutsidePads=false; // openings in the solder mask over other elements of the outer copper layers
    QRectF window;              // only the elements reaching into this part of the board; empty for the whole board
};
struct Finding {
    QString message;
    QPointF at;
    int layer=0;
    QList<int> elements;
    QRectF area;                // the part of the board the finding marks
};
// `progress` hears how far the clearance check, the long part, has come (done of total).
QList<Finding> checkDesign(const Board &board,const Rules &rules,const std::function<void(int,int)> &progress={});
}
