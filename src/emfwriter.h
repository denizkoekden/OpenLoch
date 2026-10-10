#pragma once
#include <QByteArray>
#include <QPaintDevice>
#include <QSizeF>
#include <QString>
#include <memory>

namespace openloch {
// A device that writes what is painted on it as an Enhanced Metafile (EMF) of `size` millimetres, for every module. One
// unit of the painter is a hundredth of a millimetre; the header describes a reference device with 100 units per
// millimetre and names OpenLoch and `title` (the kind of document, such as "Frontplatte"). Shapes become path records:
// pens geometric with their width, caps and joins, cosmetic pens hairlines, solid brushes with their fill rule.
// Clipping is applied to the geometry (areas are cut, lines cut into pieces), linear gradients become bands of solid
// colour, hatches lines a millimetre apart, images device-independent bitmaps (with transparency as an alpha blend) and
// texts their outlines. Dashed pens become the dashes of the EMF pen styles; whoever needs exact dashes splits the lines
// first. The file is complete when the painter has ended.
class EmfEngine;
class EmfDevice : public QPaintDevice {
public:
    explicit EmfDevice(QSizeF size,const QString &title={});
    ~EmfDevice() override;
    QPaintEngine *paintEngine() const override;
    QByteArray data() const;
    // Off by default. On, an image painted with QPainter::SmoothPixmapTransform stays smooth in readers that show scaled
    // bitmaps as GDI does, with their pixels: an opaque one gets the halftone stretch mode, one with transparency (an
    // alpha blend, which GDI never smooths) is scaled up to ten pixels per millimetre before it is written; other images
    // get the stretch mode that keeps their pixels. Adds SetStretchBltMode records. Set before painting.
    void setSmoothImages(bool on);
protected:
    int metric(PaintDeviceMetric metric) const override;
private:
    QSizeF size;
    std::unique_ptr<EmfEngine> engine;
};
}
