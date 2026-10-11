#pragma once
#include "documents/targets.h"
#include "project.h"
#include <QJsonArray>
#include <QPointF>
#include <QStringList>
namespace openloch {
// "Bauteile anordnen…" (docs/suite.md, "Bauteile anordnen im Lochraster"): a proposal where the parts that stand for
// components of the schematic lie on the board, so that the nets stay short and cross little. The supply net runs along
// a row above the parts and ground along a row below them; parts the schematic shows mirror-symmetric lie
// mirror-symmetric. Only quarter turns, every pin in a hole, one free hole between parts and one free hole at the edge
// of the board. Parts with a pin off the grid and parts for no component stay where they are and are kept clear of.
struct ArrangeOptions {
    unsigned seed=1;        // the same project and seed give the same proposal
    int passes=0;           // runs from different starts, the best one counts; 0: four for a few parts, else two
    int steps=0;            // changes tried in a run; 0: by the number of parts, at most 240000 for all runs
    double crossing=4;      // what a crossing of two nets costs, in holes of length
};
struct PartPlacement {
    QString uid;            // the part's own identifier (Project::Component::uid)
    QPointF at;             // where its anchor goes: "x" and "y" of a new part
    double angle=0;         // its new angle, the old one turned by a multiple of 90°
};
struct ArrangeScore {
    double length=0;        // in millimetres: per net a spanning tree along rows and columns, supply and ground nets
                            // along their row with a line down or up to each pin
    int crossings=0;        // lines of different nets that cross
    bool fits=true;         // every arranged part on the board, the rows of supply and ground on it too
};
struct Arrangement {
    QList<PartPlacement> parts;   // the parts that move or turn
    QStringList beside;           // Kennungen of parts that did not fit on the board; they stay where they are
    QStringList fixed;            // Kennungen of parts not arranged: pins off the grid
    ArrangeScore score;           // of the proposal
    int supplyRow=-1,groundRow=-1;// the rows the supply and ground nets run along, in holes from the top; -1: none
};
Arrangement arrangeParts(const Project &board,const documents::Targets &targets,const ArrangeOptions &options={});
// The score of the parts as they lie: the same measure the arrangement minimises, for the overview and for comparing
// with an arrangement by hand.
ArrangeScore scoreArrangement(const Project &board,const documents::Targets &targets);
// Moves and turns the parts as proposed; false if a part is no longer on the board.
bool applyArrangement(Project &board,const Arrangement &arrangement);
// "Verdrahten" (docs/suite.md, "Bauteile anordnen im Lochraster", rule 7): wires on the solder side along rows and
// columns that join the pins of each net, supply and ground along their rows above and below the parts; never through a
// hole of another net. Where a net cannot get past another one, a wire bridge on the component side. A wire joins only
// at its ends, so wires are split where others meet them or where they pass a pin of their net. Pins of parts not
// arranged (off the grid) stay open.
struct Routing {
    QJsonArray objects;       // wires to add to the board's additions
    int open=0;               // pins that could not be joined to their net
    int bridges=0;            // wires on the component side
    double length=0;          // of all wires, in millimetres
    QStringList unrouted;     // a pin of each net left open, as "R1.2"
};
Routing routeNets(const Project &board,const documents::Targets &targets);
// Whether every hole of the board lies on a pad of its own: OpenLoch's perfboard, or a template of single pads. On
// strips or larger islands, arranging and wiring as if every hole stood alone would join nets through the copper.
bool singleHoleGrid(const Project &board);
// The nets of supply and ground by their index in targets.nets, -1 for none: by name (VCC, +5V, V+, VDD; GND, 0V,
// Masse, VSS), else by the pins named + and - of a battery or a voltage source, as its part on the board or the
// schematic names them, else by pins 1 (plus) and 2 of a battery.
std::pair<int,int> supplyNets(const Project &board,const documents::Targets &targets);
}
