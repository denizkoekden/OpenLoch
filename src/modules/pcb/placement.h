#pragma once
#include "model.h"
#include "documents/targets.h"
#include <QList>
#include <QPainterPath>
#include <functional>

// "Bauteile automatisch anordnen" (docs/modules/pcb.md): components distributed on the board as a proposal, grouped by
// their nets, with short airwires and few crossings, inside the board's outline.
namespace openloch::pcb {
// Where components may lie: inside the closed outlines of the outline layer (U), lines and arcs meeting end to end
// joined, cut-outs left out (even-odd); without a closed outline the working area.
QPainterPath arrangeRegion(const Board &board);
// The nets the arrangement keeps short, as lists of pads (element indexes), only those with two pads or more: from the
// schematic's targets when given (the first pad standing for each pin), else the board's airwires joined into nets.
QList<QList<int>> arrangementNets(const Board &board,const documents::Targets *targets);
// Whether copper joins a pad already: a track on a copper layer with a node on the pad, an area over its centre or an
// autorouted track ending there, none of them part of the pad's own component.
bool padWired(const Board &board,int pad);

struct ArrangeSettings {
    double gap=1;           // between the components' outlines, and from the edge of the region, in mm
    double crossing=5;      // what a crossing of two airwires of different nets costs, in mm of airwire
    bool rotate=true;       // components may also lie turned by 90° (counter-clockwise, texts with them as "Drehen" turns them)
};
struct ArrangeResult {
    QList<int> placed;      // the components arranged, by their designators (element indexes)
    QList<int> left;        // those that found no room and stayed where they were
    QList<int> wired;       // those left where they were because copper joins their pads already
    double lengthBefore=0,lengthAfter=0;    // the airwires' length as shortest trees per net, in mm
    int crossingsBefore=0,crossingsAfter=0; // crossings of airwires of different nets
};
// Arranges the components with these designators (element indexes) inside the region: the first pad of each on the
// grid counted from the origin, the outlines of all their elements apart from each other and from everything else on
// the board by the gap (tracks and areas aside). Everything else stays where it is. A first placement takes the
// components in the order of their connections, each where its nets are shortest; then moves and swaps follow while
// they shorten the airwires, crossings counted with their price. A component keeps its turn or, with `rotate`, is
// turned by 90° about its first pad. Deterministic.
ArrangeResult arrangeParts(Board &board,const QList<int> &designators,const QList<QList<int>> &nets,const ArrangeSettings &settings={},
                           const std::function<void(int,int)> &progress={});
}
