#pragma once
#include "frontpanel.h"
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>

namespace openloch::frontpanel {
// The component interface of the front panel (docs/modules/frontpanel.md, "Bauteile und Platinen dahinter"): the
// elements that stand for components of a project, carrying the component's identifier in `component`.
struct PanelComponent {
    QString component,element;   // the component's identifier and the element standing for it
    QPointF at;                  // where it sits on the panel (componentPoint)
};
// The elements of a panel that stand for components, also inside groups, in panel order (a group before its parts).
QList<PanelComponent> panelComponents(const Panel &panel);
// The component of an element: that of the nearest element with a component on the way from it to its outermost group;
// empty when there is none.
QString componentOf(const Panel &panel,const QString &element);
// Where an element of a component sits: a hole's centre, a symbol's insertion point, else the middle of its bounds.
QPointF componentPoint(const Element &element);

// A circuit board of the project as the host (the suite) describes it to the editor, in the board's own millimetres
// (BoardBehind: as seen from its component side, origin in its top left corner, x to the right, y down).
struct BoardPart {
    QString component,designator,value;   // the component's identifier, designator and value as the board shows them
    QPointF centre;                       // the middle of its drawing without texts
    QRectF bounds;                        // its drawing without texts
    bool top=true;                        // on the component side
    QString name;                         // its name in the library (perfboard) or its package (circuit board)
};
struct BoardSource {
    QString document,board;   // the identifiers a BoardBehind names it by
    QString name;             // shown in lists: the document's name, with the board's when the document has several
    QSizeF size;              // the board from its origin
    QList<BoardPart> parts;
};
}
