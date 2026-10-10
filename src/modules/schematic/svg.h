#pragma once
#include "render.h"
#include <QByteArray>
#include <QSizeF>
#include <functional>

class QPainter;

// Drawings as SVG files ("Scalable Vector Graphics"), written by a paint device of their own: whatever the renderer
// draws becomes paths (texts as outlines) and embedded PNG pictures, clipped where it clips. The picture is `size`
// millimetres big; one user unit is a hundredth of a millimetre.
namespace openloch::schematic {
// What `paint` draws in millimetres, from (0, 0) to `size`.
QByteArray svgDrawing(QSizeF size,const QString &title,const std::function<void(QPainter&)> &paint);
// A sheet as SVG.
QByteArray sheetSvg(const Document &document,int sheet,const RenderOptions &options={});
}
