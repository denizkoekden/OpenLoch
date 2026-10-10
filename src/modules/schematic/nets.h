#pragma once
#include "model.h"
#include <utility>

// Nets of a schematic, derived from the drawing each time and never stored. The rules:
//  1. Conductors are the lines at sheet level (also inside groups) that are `electrical`. Lines of a component's
//     symbol, of a title block and Bézier curves are drawings.
//  2. A conductor's end point on another conductor (on its end, a node or anywhere along it, a T) connects both.
//  3. A contact's connection point (`pin`) on a conductor (at its end or anywhere along it) or on another contact's
//     connection point connects them. Contacts without a connection point take no part.
//  4. A junction ("Lötpunkt") on conductors joins all of them, also where they cross.
//  5. Conductors that cross without a junction are not connected.
//  6. A net label lying on a conductor or a connection point names that net; labels with the same name join their
//     nets on the same sheet.
//  7. A net label marked `global` ("Blattverweis") joins the nets of its name on all sheets, and on each sheet the
//     nets of plain labels with that name.
// Two points meet when they are less than `tolerance` apart; the zoom, line widths or pixels play no part.
namespace openloch::schematic {
constexpr double tolerance=.01;

// A place in the document: sheet index and item id.
struct ItemRef {
    int sheet=0;
    QString id;
    bool operator==(const ItemRef &) const=default;
};
// A contact of a component: sheet, component id and contact id.
struct PinRef {
    int sheet=0;
    QString component,contact;
    bool operator==(const PinRef &) const=default;
};
struct Net {
    QString name;               // the first of `names` in alphabetical order, empty without labels
    QStringList names;          // all label names
    QList<PinRef> pins;
    QList<ItemRef> conductors,junctions,labels;
};
// All nets with at least one pin, conductor or label. Named nets come first (by name), then the others in the order
// of their first element on the sheets.
QList<Net> deriveNets(const Document &document);
// The net holding a contact; nullptr if it has none.
const Net *netOf(const QList<Net> &nets,const PinRef &pin);
}
