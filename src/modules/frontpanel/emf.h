#pragma once
#include "emfwriter.h"
#include <QByteArray>
#include <QPainter>
#include <QPolygonF>

namespace openloch::frontpanel {
// Plays an Enhanced Metafile (EMF) into a frame (top left, top right, bottom left corner). Records that draw nothing
// this module can show (EMF+ comments, palettes, …) are skipped; docs/modules/frontpanel.md lists what is played.
// Returns false when the bytes are no EMF.
bool paintEmf(QPainter &p,const QByteArray &emf,const QPolygonF &frame);
// True when the bytes start with an EMF header.
bool isEmf(const QByteArray &bytes);

// Writing EMF is shared code (src/emfwriter.h); the module's export uses it under its old name.
using openloch::EmfDevice;
}
