#pragma once
#include "model.h"
#include <QList>
#include <QString>

// OpenLoch's own footprints, built from the package dimensions of the JEDEC outlines and the IPC-7351 land pattern
// recommendations named at each one. Each is a component: its elements around the origin, a designator text
// (role Designator, e.g. "IC?") and a value text. Through-hole pads lie on the bottom copper and SMD pads on the top
// copper, silkscreen on the top side, as on a single-sided hobby board.
namespace openloch::pcb {
struct Footprint {
    QString id;         // stable key, e.g. "dil-8"
    QString name;       // shown in the library (German, translated with ui())
    QString prefix;     // designator prefix, e.g. "IC"
    QString source;     // where the dimensions come from
    QList<Element> elements;
};
QList<Footprint> footprints();
// The leads of a transistor package from lead 1, as the data sheets number them (a TO-92 seen on its flat side with the
// leads down, from the left), by the usual pin names B, C, E or G, D, S; `fet` for a field-effect transistor. A TO-92
// has no order of its own: the order is the type's, read from the value (BC548B: C, B, E; 2N3904: E, B, C), and empty
// for a type the table does not hold. SOT-23 transistors share B, E, C (G, S, D), TO-220 ones B, C, E (G, D, S).
QStringList transistorLeads(const QString &footprint,const QString &value,bool fet);
// The orders a TO-92 is offered with when its type is not known, the most common first.
QList<QStringList> to92Orders(bool fet);
// The footprint's elements made a component on `board`: a new component number and one new group, the designator
// numbered with the next free number for its prefix.
QList<Element> placeable(const Footprint &footprint,const Board &board);
// Same for elements from a macro file or the clipboard: fresh group numbers, new component numbers where the board has
// them already, designators renumbered. Unless `groupLoose` is false, loose elements get one new group when none has a
// group, and elements without any component numbers get one for the innermost group of each designator (one that is
// the innermost group of several designators stays without).
QList<Element> placeable(QList<Element> elements,const Board &board,const QString &prefix={},bool groupLoose=true);
// The elements of a macro made a component, as the reference drops a macro as a component: with a designator and at
// most one component number, all of them become that one component; several components stay apart.
QList<Element> macroComponent(QList<Element> elements);

// The component wizard ("Bauteil-Assistent"): footprints of five basic forms from a few numbers. Pads are numbered
// from pin 1 (square for through-hole pads): along a single row, down the left and up the right row, counter-clockwise
// round the four sides from the top of the left one, clockwise round a circle from twelve o'clock (in two rows
// alternating between the outer and the inner circle).
struct Wizard {
    enum Form {SingleRow,DoubleRow,Quad,Circle,DoubleCircle};
    Form form=DoubleRow;
    bool smd=false;
    double diameter=1.6,drill=.8;       // through-hole pads
    double length=1.8,width=.6;         // SMD pads: across and along their row
    int count=8;
    double pitch=2.54,rowSpacing=7.62;  // single and double rows, sides of a quad
    double quadWidth=10,quadHeight=10;  // distance of the left and right rows, of the top and bottom rows
    double circle=10,innerCircle=6;     // diameters through the pad centres
};
Wizard wizardDefaults(Wizard::Form form);
Footprint wizardFootprint(const Wizard &wizard);
}
