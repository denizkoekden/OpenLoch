#pragma once
#include "frontpanel.h"
#include <QImage>
#include <QPainter>

namespace openloch::frontpanel {
// What is drawn: the view switches of the editor (S/W outline view, element groups) and output choices.
struct RenderOptions {
    bool background=true,grid=false,outlineOnly=false;
    bool dimensions=true,milled=true,engraved=true,drills=true,texts=true,other=true;
    // Width of a hairline (pen width 0) in panel millimetres; 0 draws it one device pixel wide.
    double hairline=0;
    // Milled and engraved contours and drills as the work piece gets them (milled dark grey with the tool path light
    // grey, engraved white with the path grey); off draws them with their own pen and fill, as for print and export.
    bool machiningLook=true;
};
// Colours of the machining look.
QColor milledColor();
QColor engravedColor();
// Which view switch an element belongs to.
enum class ViewGroup {Dimension,Milled,Engraved,Drill,Text,Other};
ViewGroup viewGroup(const Element &e);
bool shown(const Element &e,const RenderOptions &options);

// Paints a panel in its own millimetre coordinates; the painter's transform places it.
void paintPanel(QPainter &p,const Document &document,const Panel &panel,const RenderOptions &options={});
void paintElement(QPainter &p,const Document &document,const Element &e,const RenderOptions &options={});
// The panel as a picture at `dpi`, or only the given elements (background off: transparent).
QImage renderPanel(const Document &document,const Panel &panel,double dpi,const RenderOptions &options={},const QList<Element> *only=nullptr);
// A picture resource as an image; empty for vector pictures that cannot be shown.
QImage resourceImage(const Document &document,const Element &e);
}
