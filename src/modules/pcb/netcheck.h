#pragma once
#include "model.h"
#include "documents/targets.h"
#include <QList>
#include <QPointF>
#include <QStringList>

// The board checked against the target connections of its project's schematic (docs/suite.md, "Soll-Verbindungen aus
// dem Schaltplan"). A component of the board stands for a component of the schematic by the identifier on its
// designator, failing that by the same designator regardless of case (a designator more than one component has is not
// taken). Its pads stand for the pins of the same name regardless of case; the numbered contacts 1 and 2 of diodes,
// LEDs and electrolytics stand for pads named A and K or + and - (openloch::assignedPins). Several pads may stand for
// one pin, pads without a pin (mounting holes) for none. Pins are joined where the copper joins their pads: tracks,
// areas, through-plated pads and the ground plane, as the test tool joins them; the pads of one pin count as joined
// inside the component.
namespace openloch::pcb {
struct NetCheck {
    struct Part {
        int target=-1;              // the component of the schematic, by its place in Targets::components
        int designator=-1,value=-1; // the board component's designator and value texts (element indexes, -1 for none)
        QList<int> members;         // all its elements
        QList<QList<int>> pads;     // for each pin of the schematic's component the pads standing for it (element indexes)
        bool byDesignator=false;    // found by its designator, not yet linked by identifier
        bool swapped=false;         // pins 1 and 2 of a resistor, capacitor or coil fit the other way round
    };
    QList<Part> parts;              // components standing for one of the schematic, every pin with a pad
    QList<int> missing;             // components of the schematic without any on the board
    QList<int> extra;               // board components for none of the schematic, by their designator texts
    QStringList ambiguous;          // designators of the schematic more than one board component has
    QList<Part> unassigned;         // components with a pin of the schematic no pad stands for: "Anschlüsse zuordnen"
    // A net in pieces: the pins apart from its largest piece, and all its pieces, the largest first.
    struct Open {int net=-1;QList<documents::TargetPin> pins;QList<QList<documents::TargetPin>> pieces;};
    QList<Open> open;
    // Two nets joined on the board: where (a pad of the piece joining them) and the copper elements of that piece.
    struct Joined {int first=-1,second=-1;QPointF at;QList<int> elements;};
    QList<Joined> joined;
    // For each open net the shortest lines that would join its pieces, from pad to pad (element indexes).
    struct Airwire {int from=-1,to=-1,net=-1;};
    QList<Airwire> airwires;
    bool passed() const{return missing.isEmpty()&&ambiguous.isEmpty()&&unassigned.isEmpty()&&open.isEmpty()&&joined.isEmpty();}
    // The pads standing for a pin of a component of the schematic (by its identifier and the pin's name), empty if none.
    QList<int> padsFor(const documents::Targets &targets,const QString &component,const QString &pin) const;
};
NetCheck checkNets(const Board &board,const documents::Targets &targets);

// "Aus Schaltplan übernehmen": a component found by its designator is linked to the schematic's component (its
// identifier on the designator), designator and value follow the schematic, and the pins 1 and 2 of a resistor,
// capacitor or coil (openloch::kindOf) are named the way round they fit.
struct NetChange {
    int designator=-1;              // the board component, by its designator text
    QString component;              // the identifier to link it to; empty: as it is
    bool text=false;                // designator and value from the schematic
    QString designatorText,value;
    bool swapPins=false;            // pins 1 and 2 exchanged on its pads
};
QList<NetChange> netChanges(const Board &board,const NetCheck &check,const documents::Targets &targets);
// Applies the changes to the board; returns how many components changed.
int applyNetChanges(Board &board,const QList<NetChange> &changes);
// The pads of a board component, in the order of the elements.
QList<int> padsOf(const Board &board,int designator);
// Names the pads of a board component after pins of the schematic: `pins` for its pads in the order of padsOf, an empty
// name for a pad that stands for no pin. The name of a pad whose pin changes follows, so that Sprint-Layout files keep
// it; a pad keeping its pin keeps its name. Links the component to `component` when that is not empty. False if the
// numbers of pads and names differ.
bool assignPins(Board &board,int designator,const QStringList &pins,const QString &component={});

// "Fehlende Bauteile setzen": what the module's library offers for a component of the schematic the board lacks, best
// first: the footprints whose pads fit its pins (openloch::fittingParts: the same kind of designator first, then its
// value in the footprint's), then the footprints with as many pads, their pads named after its pins in order, and a row
// of as many pads made by the component wizard. Only a fitting footprint of the same kind is a sure choice.
struct PartChoice {
    int footprint=-1;           // the footprint, by its place in footprints(); -1: the wizard's row of pads
    bool inOrder=false;         // its pads named after the component's pins in their order
    bool sameKind=false;        // a footprint for the same kind of part (R, C, D and LED, T and Q, IC and U, …)
    QString label;              // as the choice is offered
};
QList<PartChoice> partChoices(const documents::TargetComponent &component);
// The elements of a choice for the component: a component with its designator, value and identifier, around the origin,
// numbered for `board`.
QList<Element> missingPart(const PartChoice &choice,const documents::TargetComponent &component,const Board &board);
// Lays parts beside the board: right of the working area and of everything on and beside it, from the top of the
// working area down in columns as high as it, the first pad of each on the grid counted from the origin. Returns the
// indexes of the new elements.
QList<int> layBeside(Board &board,const QList<QList<Element>> &parts);
}
