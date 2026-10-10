#pragma once
#include "frontpanel.h"
#include <QPainter>
#include <QSize>
#include <QSizeF>

class QWidget;
namespace openloch::frontpanel {
// What the print preview works with: the settings of the panel shown and what holds for the whole print job.
struct PrintOptions : PrintSettings {
    int copies=1;bool allPanels=false;
    double correctionX=1,correctionY=1;    // calibration: factors for printers that print slightly too large or too small
};
// The printout of one panel in paper millimetres: rulers, cut marks, the panel (repeated as tiles) and the data line.
QSizeF printExtent(const Panel &panel,const PrintOptions &options);
// Where the panel's top left corner lies in the printout.
QPointF printPanelOrigin(const PrintOptions &options);
// Where the printout starts, from the top left corner of the printable area of the first sheet (of size `area`).
// Negative values are allowed: the printer leaves out what lies outside its area.
QPointF printOffset(const PrintOptions &options,QSizeF extent,QSizeF area);
void paintPrintout(QPainter &p,const Document &document,const Panel &panel,const PrintOptions &options,const QString &title);
// Sheets across and down for a printout of `extent` placed at `offset` within printable areas of size `area`.
QSize sheetsFor(QSizeF extent,QPointF offset,QSizeF area);
// The print preview, starting with the active panel; prints it or all panels. Each panel brings its own print
// settings; `settings` receives them as left in the preview, also when it is cancelled. False when cancelled.
bool printPanels(QWidget *parent,const Document &document,const QString &title,QList<PrintSettings> *settings=nullptr);
}
