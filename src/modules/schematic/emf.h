#pragma once
#include "render.h"
#include <QByteArray>
#include <QList>
#include <QPolygonF>
#include <QSizeF>
#include <functional>

class QPainter;
class QPainterPath;

// Drawings as EMF files ("Enhanced Metafile"), written by openloch::EmfDevice (src/emfwriter.h): one unit a hundredth of a
// millimetre, the frame the picture's size. Dashed lines are cut into their dashes first, each a line with round ends,
// so that the picture shows the dashes OpenLoch draws and not the pen styles of EMF.
namespace openloch::schematic {
// What `paint` draws in millimetres, from (0, 0) to `size`.
QByteArray emfDrawing(QSizeF size,const std::function<void(QPainter&)> &paint);
// A sheet as EMF.
QByteArray sheetEmf(const Document &document,int sheet,const RenderOptions &options={});
// The dashes of `path` for a dash pattern counted in `unit`s (Qt's pen pattern, dash and gap in turn, starting `offset`
// units into it): the pieces that are drawn, as open polylines; a dash of no length is a single point twice.
QList<QPolygonF> dashPieces(const QPainterPath &path,const QList<qreal> &pattern,double unit,double offset=0);
}
