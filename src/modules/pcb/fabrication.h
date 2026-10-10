#pragma once
#include "model.h"
#include <QByteArray>
#include <QList>
#include <QSizeF>
#include <QString>

// Fabrication data of a board: Gerber files (extended Gerber, RS-274X) for the layers and masks, Excellon drill data
// and the component data for pick and place. All of them share one frame: millimetres (or inches) from the board's
// origin, x to the right and y upwards, so the files of one board lie exactly on each other.
namespace openloch::pcb {
// Board coordinates (y down) to output coordinates (y up from the origin). A mirrored output turns the board about
// the middle of its working area first: left-right (1) or top-bottom (2). Without the origin ("fromOrigin" in the
// settings, a switch of the preferences) the outputs count from the top left corner of the working area instead, y
// still upwards, so the board lies at negative y.
struct OutputFrame {
    QPointF origin;
    QSizeF area;
    int mirror=0;
    QPointF map(QPointF p) const;
    // The point of the board that becomes (0, 0).
    QPointF zero() const;
    // Mirroring turns counter-clockwise into clockwise.
    bool reverses() const{return mirror!=0;}
};
OutputFrame outputFrame(const Board &board,int mirror=0,bool fromOrigin=true);

// The Gerber outputs: the seven layers by their numbers (Layer), then the solder masks and the SMD masks (solder paste
// stencils) of both sides.
enum GerberMask {SolderMaskTop=8,SolderMaskBottom,PasteMaskTop,PasteMaskBottom};
constexpr int gerberOutputCount=11;
// All outputs in the order of the export dialog.
QList<int> gerberOutputs();
QString gerberOutputName(int output);
// The file name ending of an output: the extensions most board makers recognise.
QString gerberSuffix(int output);
// Whether an output has anything on it (a mask needs elements with openings, a layer elements or a ground plane).
bool gerberOutputUsed(const Board &board,int output);
struct GerberSettings {
    bool mirrored=false;        // the whole output mirrored left-right
    bool fromOrigin=true;       // coordinates from the board's origin, else from the top left corner (outputFrame)
    bool frame=false;           // the outline of the working area in every file
    // Drill holes left open in the copper ("freistanzen"), or only marked by a small open dot as a centre punch.
    bool clearHoles=false,punchMarks=false;
    // Solder mask: openings for the elements marked for it, larger than their copper by an offset for each kind of
    // element (pads, SMD pads, everything else); the kinds can be left out.
    bool maskPads=true,maskSmd=true,maskOther=true;
    double padOffset=.1,smdOffset=.1,otherOffset=.1;
    bool maskInverted=false;    // the mask itself drawn instead of its openings
    // SMD mask: the SMD pads of a side, larger (or, negative, smaller) by an offset.
    double pasteOffset=0;
    bool pasteInverted=false;
};
// The size of the punch marks.
constexpr double punchMarkDiameter=.15;      // as the reference marks it
// The width of lines that have none (hairlines on silkscreen and outline layers, the frame).
constexpr double thinnestLine=.1;
// One Gerber file: millimetres with six decimals (format 4.6), leading zeros left out. Pads are flashed, tracks,
// arcs, texts and the grid lines of hatched areas drawn with round apertures, areas and other outlines are regions. A
// ground plane is the working area in dark polarity with the clearances cut out in clear polarity (hatched areas by
// their whole outline); the elements follow in dark polarity.
QByteArray gerber(const Board &board,int output,const GerberSettings &settings);

// Excellon drill data
struct DrillHole {
    QPointF at;                 // board coordinates
    double diameter=0;
    bool plated=false;          // through-plated (a via in the reference's sense)
};
// The holes of a board: 0 all, 1 only through-plated, 2 only plain holes.
QList<DrillHole> drillHoles(const Board &board,int which=0);
struct DrillSettings {
    int holes=0;                // as drillHoles()
    bool fromBelow=false;       // drilled from the bottom side: mirrored left-right
    bool fromOrigin=true;       // as GerberSettings
    bool sorted=true;           // each tool's holes in a short path instead of the board's order
    bool metric=false;          // millimetres; inches otherwise, as the reference proposes
    int integerDigits=2,decimalDigits=4;
    bool suppressLeadingZeros=false;
    bool decimalPoint=false;    // coordinates with a decimal point instead of a fixed number of digits
    // Special options of the reference: drilled from below mirrored about the middle of the holes themselves (for HPGL
    // machines; the plain mirror turns about the middle of the working area as Gerber does), the unit as M71/M72
    // instead of METRIC/INCH, no G90 and no comment lines.
    bool mirrorHoles=false,m71=false,noG90=false,noComments=false;
};
// The holes of a board for the list of holes: each diameter (to a thousandth of a millimetre) with its count, smallest
// first, of the chosen kinds.
QList<std::pair<double,int>> holeTable(const Board &board,bool plain,bool plated);
QString holeList(const Board &board,bool plain,bool plated);
// A coordinate of the drill file, `value` in the file's unit: with a decimal point, or as digits without one, the
// leading zeros left out if wanted.
QByteArray excellonNumber(double value,const DrillSettings &settings);
// The drill file: a header with the tool table (one tool per diameter, the smallest first), then the holes of each
// tool. "LZ" in the header means leading zeros are written, "TZ" that they are left out.
QByteArray excellon(const Board &board,const DrillSettings &settings);

// All Gerber files of the outputs in use and the Excellon file (ending .drl) into an existing folder, named `name`
// with the outputs' endings; files there are replaced. Returns the files written, or an empty list and `error`.
QStringList writeFabricationFiles(const Board &board,const QString &folder,const QString &name,const GerberSettings &gerberSettings={},
                                  const DrillSettings &drillSettings={},QString *error=nullptr);

// Component data ("Bauteildaten") for parts lists and pick and place machines
enum ComponentField {FieldDesignator,FieldValue,FieldLayer,FieldX,FieldY,FieldRotation,FieldPackage,FieldComment,FieldIndex};
constexpr int componentFieldCount=9;
QString componentFieldName(int field);
struct ComponentDataSettings {
    QList<int> fields{FieldDesignator,FieldValue,FieldPackage,FieldX,FieldY,FieldRotation,FieldLayer};
    QString separator=QStringLiteral(",");
    QString top=QStringLiteral("Top"),bottom=QStringLiteral("Bottom");    // the texts of the layer field
    int unit=0;                 // positions in 0 mm, 1 mil, 2 inches
    int decimals=2;
    bool rotationPrefix=false;  // "R90" instead of "90"
    // Which components, as the reference filters them: SMD ones (only SMD pads) and through-hole ones (any other), on the
    // top and the bottom side, if wanted only those with pick and place data. The reference starts with the SMD
    // components of both sides.
    bool smdParts=true,throughHoleParts=false,onTop=true,onBottom=true,pickPlaceOnly=false;
    bool stripZeros=false;      // positions without zeros at the end of their decimals
    bool header=true;           // a first line with the names of the fields
    bool fromOrigin=true;       // positions as GerberSettings counts them
};
// One line per component in the order of the component list; fields holding the separator or quotes are quoted.
QString componentData(const Board &board,const ComponentDataSettings &settings);
}
