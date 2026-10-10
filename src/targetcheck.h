#pragma once
#include "documents/targets.h"
#include "project.h"
#include <QLineF>
#include <QList>
#include <QPointF>
#include <QStringList>
#include <QMap>
#include <utility>
namespace openloch {
// The board shown checked against the target connections of its project's schematic (docs/file-formats.md, "Übernahme
// vom Schaltplan"). A part stands for a component by its field "component" (else its own identifier), failing that by
// the same Kennung; its pins stand for the component's pins of the same name. Pins are joined when the continuity
// tester joins the copper under them.
struct TargetCheck {
    struct Part {
        int target=-1;                // the component, by its place in Targets::components
        Project::Component part;
        QList<int> pins;              // for each pin of the part the component's pin it stands for, -1 for none
        QList<QPointF> at;            // for each pin of the part where it lies on the board
        bool byDesignator=false;      // found by its Kennung, not yet linked to the component
        bool swapped=false;           // two interchangeable pins (1 and 2 on both sides) fit the other way round
    };
    QList<Part> parts;                // parts standing for a component, their pins assigned
    QList<int> missing;               // components without a part on the board
    QList<Project::Component> extra;  // parts for no component of the schematic
    QStringList ambiguous;            // Kennungen that more than one part has
    QList<Part> unassigned;           // parts whose pin names the component does not have: "Anschlüsse zuordnen"
    // A net in pieces: the pins apart from its largest piece, and all its pieces, the largest first.
    struct Open {int net=-1;QList<documents::TargetPin> pins;QList<QList<documents::TargetPin>> pieces;};
    QList<Open> open;
    struct Joined {int first=-1,second=-1;QPointF at;};          // two nets joined on the board, and at which pin
    QList<Joined> joined;
    QList<QLineF> airwires;           // for each net the shortest lines that would join its pieces
    bool passed() const{return missing.isEmpty()&&ambiguous.isEmpty()&&unassigned.isEmpty()&&open.isEmpty()&&joined.isEmpty();}
};
TargetCheck checkTargets(const Project &board,const documents::Targets &targets);
// The targets for the board shown: a component a part on another board of the document is linked to belongs there, so
// the board shown is checked without it and its pins, and nothing on it is linked to it or set for it.
documents::Targets targetsForBoard(const Project &board,const documents::Targets &targets);
// "Aus Schaltplan übernehmen": a part found by its Kennung is linked to its component, Kennung and value follow the
// schematic, two interchangeable pins keep the way round they fit. Placing missing parts is left to the user.
struct TargetChange {
    QString uid;                 // the part, by its own identifier
    QString component;           // the component to link it to; empty: as it is
    bool text=false;             // Kennung and value from the schematic
    QString designator,value;
    QStringList pins;            // names of its pins; empty: as they are
};
QList<TargetChange> targetChanges(const TargetCheck &check,const documents::Targets &targets);
// Returns how many parts changed.
int applyTargetChanges(Project &board,const QList<TargetChange> &changes);
// Where the pins of a part of the board shown lie, in the order of its pins.
QList<QPointF> pinPositions(const Project &board,const Project::Component &part);
// For each pin of a part the component's pin of the same name regardless of case, -1 for none. The schematic numbers the
// two contacts of diodes, LEDs and electrolytics: 1 stands for a pin named A or +, 2 for K or -.
QList<int> assignedPins(const QStringList &part,const QStringList &component);
// The kind of a component of the schematic (R, C, L, D, T, IC …): its `kind`, else the letters its designator begins with.
QString kindOf(const documents::TargetComponent &component);
// Two leads numbered 1 and 2 on both sides that may be swapped: those of a resistor, capacitor or coil (kindOf R, C, L).
bool interchangeable(const Project::Component &part,const documents::TargetComponent &component);
// A part of a library page that may stand for a component: the page (a key the caller chooses, such as its file), its
// place on the page, and its name, Kennung, value and pins as the page gives them; `onGrid`: every pin lies on the
// hole grid of the perfboard, so that the part can be soldered there.
struct LibraryChoice {QString page;int index=-1;QString name,id,value;QStringList pins;bool onGrid=true;};
// The parts of an open library page (docs/open-libraries.md) as choices, in the order of the page.
QList<LibraryChoice> openLibraryChoices(const QString &page,const QJsonObject &json);
// The parts that fit a component, best first: all its pins and no more (assignedPins); parts whose pins all lie on the
// hole grid before the others; then the same kind of Kennung (R, C, L, D and LED, T and Q, IC and U, …, by kindOf),
// then its value in the part's name or value, then the library's order.
QList<LibraryChoice> fittingParts(const documents::TargetComponent &component,const QList<LibraryChoice> &library);
// Whether two Kennungen name the same kind of part ("Q1" and "T#", "U3" and "IC#", "LED2" and "D#").
bool sameKind(const QString &designator,const QString &id);
// Lays parts beside the board for missing components, to the right of it from the top, in columns as high as the
// board, each on the hole grid: a placement of its library entry with Kennung and value of its component and linked to
// it. `pages`: the bytes of each page by its key. Returns where they were added in the additions.
QList<int> placeBeside(Project &board,const QList<std::pair<documents::TargetComponent,LibraryChoice>> &parts,const QMap<QString,QByteArray> &pages);
}
