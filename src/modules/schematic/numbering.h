#pragma once
#include "model.h"

// "Bauteile neu nummerieren": components with the switch "automatisch Nummerieren" and the same letters ("R" of "R12")
// belong together and get consecutive numbers, over the chosen sheets in their order.
namespace openloch::schematic {
struct NumberingOptions {
    enum class Order {None,Columns,Rows};
    // No sorting keeps the order of the elements; by columns and by rows the sheet is scanned with a square of side
    // `raster` (mm), down the columns or along the rows, and the components inside a square come first.
    Order order=Order::None;
    double raster=20;
    QList<int> sheets;          // the sheets taken into account, empty for all
    QStringList only;           // only these components (ids), empty for all
    QString letters;            // only components with these letters, empty for all
    int start=1;
};
// The letters of a designator: what is left without the number or question mark at its end ("R12", "R?" → "R").
QString designatorLetters(const QString &designator);
// The kind of part a component is, in the letters circuit boards sort parts by (R, C, L, D, T, IC, K, S, F, X, B):
// for a symbol of OpenLoch's own library the kind of its page (its letters follow sPlan's library, K for a transistor),
// else the letters of its designator as entered, without sheet number and prefix, and of a reference designation after
// IEC 81346 the part after its last sign ("-R3", "=A1-K2" → "R", "K").
QString partKind(const Item &component);
// Renumbers and returns how many components were numbered.
int renumber(Document &document,const NumberingOptions &options);
}
