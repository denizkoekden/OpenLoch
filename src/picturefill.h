#pragma once
#include <QByteArray>
#include <QImage>
#include <QPolygonF>
namespace openloch {
// A fill picture from any image file OpenLoch reads (BMP, PNG, GIF, JPEG). LM4 stores fill pictures as BMP without
// transparency, and LochMaster clips a fill picture to the area's outline. So a picture with transparent parts brings
// the outline of its opaque part: the area takes it as its outline, and the picture shows only there, in OpenLoch, in
// LochMaster and in the outline and S/W views alike, without covering copper and holes around it.
struct PictureFill {
    QByteArray bmp;    // 24 bit, transparent pixels over white
    QPolygonF outline; // the largest opaque part in fractions of the picture (0..1), empty if nothing is transparent
};
PictureFill pictureFill(const QImage &image);
}
