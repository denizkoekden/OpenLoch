#pragma once
#include "model.h"
#include "documents/targets.h"
#include <QList>
#include <QString>
#include <functional>

// "Alle Luftlinien routen" (docs/modules/pcb.md): every net of the board routed at once on the two outer copper layers,
// beyond what the reference's point-to-point router (autoroute() in copper.h) does.
namespace openloch::pcb {
struct RouteNet {
    QString name;               // empty for a net without a name (from airwires)
    QList<int> pads;            // element indexes
};
// The nets to route: from the schematic's targets when given (every pad standing for a pin), else the board's airwires
// joined into nets. Only nets with two pads or more.
QList<RouteNet> routingNets(const Board &board,const documents::Targets *targets);
// The order nets are routed in: ground (GND, AGND, Masse, VSS, 0V …) first, then supplies (VCC, VDD, VEE, VBAT, +5V,
// -12V, 3V3 …), then the rest, each group by the length of its shortest tree, shortest first.
QList<int> routingOrder(const Board &board,const QList<RouteNet> &nets);

struct RouteSettings {
    double width=.8,clearance=.4;       // tracks, and copper of different nets at least this far apart
    double step=.4;                     // the raster of track nodes
    double viaDiameter=1.6,viaDrill=.8; // through-plated pads where a route changes layer
    double padClearance=.4;             // the clearance new tracks and vias carry for the ground plane
    bool twoLayers=true;                // also the top copper (K1), with vias; else the bottom one only
};
struct RouteResult {
    int connections=0;                  // joins the nets asked for (islands of copper to join)
    int routed=0,vias=0;
    QList<int> added;                   // the new tracks and vias (element indexes)
    // What stayed open: two pads (element indexes) the router could not join, with the net's name.
    struct Open {int from=-1,to=-1;QString net;};
    QList<Open> open;
};
// Routes the nets in routingOrder: within each net its islands of copper are joined one by one from the largest, each
// by the cheapest way on a raster of `step` from what the net has so far (pads and new tracks) to the nearest other
// island, keeping `clearance` from the copper of other nets and from drill holes, inside the board's outline (else the
// working area). The bottom layer is preferred, the top one costs half as much again, a change of layer costs a via.
// When something stays open, the whole routing is tried again with the open nets moved to the front, up to four tries;
// the try with the fewest open joins (then the fewest vias) counts.
// The new tracks are marked autorouted with the two pads they join; stored airwires the copper now joins are removed.
RouteResult routeAll(Board &board,const QList<RouteNet> &nets,const RouteSettings &settings={},const std::function<void(int,int)> &progress={});
}
