#pragma once
#include "emfwriter.h"
#include <QByteArray>
#include <QImage>

// Picture formats Qt does not write, for exporting the board as in the reference: GIF here, EMF through the shared writer.
namespace openloch::pcb {
using openloch::EmfDevice;
// A picture as a GIF file (GIF89a): one image with a global colour table of up to 256 colours, LZW compressed. A
// picture of more colours is reduced first (Qt's palette with dithering); an empty one or one larger than 65535 pixels
// a side gives nothing.
QByteArray gifData(const QImage &picture);

// Enhanced Metafiles (EMF) are written by the writer all modules share (src/emfwriter.h): one unit of the painter a
// hundredth of a millimetre, the header naming the kind of document.
}
