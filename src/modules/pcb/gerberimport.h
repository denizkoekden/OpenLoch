#pragma once
#include <functional>
#include "model.h"
#include <QByteArray>
#include <QColor>
#include <QList>
#include <QMap>
#include <QPainterPath>
#include <QPolygonF>
#include <QString>

class QPainter;

// Gerber import: board layers rebuilt from extended Gerber files (RS-274X) and Excellon drill data. A Gerber file only
// holds the picture of a layer, so the import guesses what made it: draws become tracks and arcs, flashes become pads
// (with a drill hole from the drill data where one lies under them), regions become areas, and a large dark region
// with clear features cut into it becomes the layer's ground plane.
namespace openloch::pcb {
// A Gerber aperture: a standard circle, rectangle, obround or polygon, or a macro, with its shape about the origin.
struct GerberAperture {
    char kind='C';                      // C, R, O, P, or M for a macro
    QList<double> parameters;           // in millimetres (polygon: outer diameter, vertices, rotation)
    QPainterPath shape;                 // the flashed shape, millimetres, y upwards
};
// One object of a Gerber file in millimetres with y upwards, as the file draws it.
struct GerberObject {
    enum Kind {Draw,Arc,Flash,Region} kind=Draw;
    bool dark=true;                     // polarity
    int aperture=0;                     // draws and flashes: the D code
    QPointF from,to;                    // draws and arcs: start and end; flashes: position in `to`
    QPointF centre;                     // arcs
    bool clockwise=false;               // arcs
    double rotation=0;bool mirrorX=false,mirrorY=false;double scale=1;   // flashes: the loaded transformation
    QList<QPolygonF> contours;          // regions, arcs flattened
};
struct GerberData {
    QMap<int,GerberAperture> apertures;
    QList<GerberObject> objects;
    QRectF bounds() const;
    // Paints the file in millimetres (the caller turns y upwards): dark objects in `dark`, clear ones in `clear`.
    void paint(QPainter &painter,const QColor &dark,const QColor &clear) const;
    // The shape an object covers.
    QPainterPath shape(const GerberObject &object) const;
};
// Reads extended Gerber: format, unit, apertures with macros, polarity, linear and circular interpolation (single and
// multi quadrant), regions, step and repeat, the loaded mirroring, rotation and scaling of flashes, and the older forms
// (G54, G70/G71, combined commands). Throws FormatError with the line for anything it cannot read.
GerberData readGerber(const QByteArray &data);

// Excellon drill data
struct DrillFormat {
    bool metric=false;
    int integerDigits=2,decimalDigits=4;
    bool trailingZeros=false;           // trailing zeros written, leading ones left out ("TZ")
    // The other two ways of the reference: all digits written, or numbers with a decimal point (a number without one
    // then counts whole units).
    bool allDigits=false,decimalPoint=false;
};
struct DrillHit {
    QPointF at;                         // millimetres, y upwards
    double diameter=0;
};
// The format a drill file states (unit line with LZ/TZ and digit pattern, FILE_FORMAT comment), else the usual one of
// its unit: inches 2.4, millimetres 3.3, leading zeros written.
DrillFormat drillFormat(const QByteArray &data);
// Holes of a drill file; numbers without a decimal point take `format`. Throws FormatError for a broken file.
QList<DrillHit> readExcellon(const QByteArray &data,const DrillFormat &format);

struct GerberImport {
    QMap<int,GerberData> layers;        // by layer number
    QList<DrillHit> drills;
    bool vias=true;                     // pads on both outer sides over one hole become one through-plated pad
    bool joinTracks=true;               // draws that continue each other become one track
};
// The board the files describe: a working area around everything with a small margin, its origin where the files have
// theirs, the elements per layer and recognised ground planes. `progress` hears of each layer begun (done of total).
Board importGerber(const GerberImport &import,const QString &name,const std::function<void(int,int)> &progress={});
}
