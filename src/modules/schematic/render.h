#pragma once
#include "model.h"
#include <QImage>
#include <QSet>

class QPainter;

// Drawing a sheet with QPainter in millimetres: the same for the editor, pictures, print and the command line.
namespace openloch::schematic {
struct RenderOptions {
    bool titleBlock=true;           // draw the title block behind the circuit
    bool circuit=true;              // draw the sheet's elements
    bool paper=true;                // fill the sheet with the paper colour
    // The paper: white in print and export; on screen sPlan tints it very slightly unless "Weißer Hintergrund" is set.
    QColor paperColour=QColor(255,255,255);
    bool blackAndWhite=false;       // all lines and texts black, fills white or black
    bool pins=false;                // mark the connection points of contacts
    QString fileName;               // for <FILENAME> and the like
    // Elements drawn in the selection colour (by id).
    QSet<QString> highlighted;
    QColor highlightColor=QColor(255,0,0);
    // Parents on light blue, children on light red ("Parent/Child-Bauteile einfärben"); those in `related` stronger.
    bool parentChild=false;
    QSet<QString> related;
    // Texts that are link targets get an arrow pointing in at the left, links an arrow pointing out at the right.
    bool linkMarks=false;
};
// What a picture shows of the current sheet: all of it, all its elements (drawn with the title block) or only the
// selected ones; the elements, as in the reference, in their bounds and 4 mm around them.
enum class ExportArea {Sheet,Elements,Selection};
// Draws one sheet. The painter's coordinates must be millimetres with the origin at the sheet's top left corner.
void paintSheet(QPainter &painter,const Document &document,int sheet,const RenderOptions &options={});
// Draws a list of elements (a library symbol, elements on the pointer).
void paintItems(QPainter &painter,const QList<Item> &items,const Document &document,int sheet,const RenderOptions &options={});
// A sheet as a picture, `pixelsPerMm` fine.
QImage renderSheet(const Document &document,int sheet,double pixelsPerMm,const RenderOptions &options={});
// The size a junction is drawn with and its colour: with `autoSize` they follow the conductor it sits on.
std::pair<double,QColor> junctionLook(const Item &junction,const Sheet &sheet);
// The marks of text links ("Text-Verlinkungen") in a text's frame (textFrame), around its box (textRect): the inner arrow
// at the left of a text others may link to, the outer one at the right of a link with a target (`outgoing`).
QPolygonF linkArrow(const QRectF &box,bool outgoing);
}
