#pragma once
#include "model.h"
#include <QSize>

// Pictures of a document: what they take, and the changes the properties panel and the "Bitmap-Explorer" offer.
// A changed picture becomes a new resource (its key is the hash of its bytes); pictures no element shows any more are
// dropped from the document.
namespace openloch::schematic {
struct ImageInfo {
    QSize pixels;
    double dpi=0;           // pixels per inch along the picture's width as it is placed
    qint64 bytes=0;         // the size of its file
    int bits=0;             // bits per pixel as the file stores them ("Farbtiefe"), 0 when unknown
};
// The bits per pixel a PNG, JPEG or BMP file stores, 0 when unknown.
int bitsPerPixel(const QByteArray &file);
ImageInfo imageInfo(const Document &document,const Item &image);
// The picture with fewer pixels: by `factor` (> 1, "Auflösung verringern" uses 1.4).
bool reduceResolution(Document &document,Item &image,double factor);
// Turned by 90° clockwise in its pixels; the element turns its width and height along.
bool rotateImage(Document &document,Item &image);
// A little brighter ("Aufhellen").
bool lightenImage(Document &document,Item &image);
// The height of the element from its width in the proportions of the picture ("Normalisieren").
bool normaliseImage(const Document &document,Item &image);
// The pictures the document's elements show, also in groups, components and title blocks: sheet and element id.
struct PlacedImage {int sheet=0;QString id;};
QList<PlacedImage> placedImages(const Document &document);
Item *imageWithId(Document &document,int sheet,const QString &id);
const Item *imageWithId(const Document &document,int sheet,const QString &id);
// Removes resources that no element shows; returns how many.
int dropUnusedResources(Document &document);
}
