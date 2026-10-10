#pragma once
#include "model.h"
#include <QColor>
#include <QDialog>
#include <QImage>
#include <QPageLayout>
#include <array>
#include <memory>

class QPainter;
class QPrinter;

namespace openloch::pcb {
// The settings of the print preview, as the reference offers them.
struct PrintSettings {
    std::array<bool,layerCount+1> layers{};     // index = layer
    std::array<QColor,layerCount+1> colours{};
    // 0 the top side over the bottom side, 1 the bottom side over the top side, 2 the bottom side below the top side,
    // 3 the bottom side to the right of the top side.
    int order=0;
    // Special layers, as the reference offers them: the openings of the solder mask of the top side and of the bottom side,
    // each in its colour, and a drill plan with the diameters written next to the holes. An opening is larger than the
    // copper by the offset of its kind on every side (through-hole pads 0.3 mm, SMD pads 0.1 mm, everything else 0.1 mm;
    // negative makes it smaller); a kind switched off has no openings in the print.
    bool maskTop=false,maskBottom=false,drillPlan=false;
    QColor maskTopColour=QColor(0,140,0),maskBottomColour=QColor(0,140,0),drillColour=QColor(0,0,0);
    bool maskPads=true,maskSmd=true,maskOther=true;
    double padMask=.3,smdMask=.1,otherMask=.1,drillTextHeight=1;
    bool blackWhite=false,mirrored=false,marks=false,frame=false,negative=false,helpGrid=false,infoLine=false;
    // The scanned templates (top side, bottom side) printed under the layers: under an image of one side's layers the
    // template of that side, under an image of both sides that of the side on top.
    bool withTemplate=false;
    std::array<QImage,2> templatePictures;
    double scale=1;                         // 1 is 1:1
    // Copies of the printout side by side and one below the other (Kacheln), 1 to 20 each way, `tileGap` mm apart.
    int tilesX=1,tilesY=1;double tileGap=0;
    QPageLayout::Orientation orientation=QPageLayout::Portrait;
    QPointF position;                       // top left corner of the printout on the printable area, in mm
};
// Settings for a board: its visible layers with elements, in the layers' colours (the outline in black).
PrintSettings defaultPrintSettings(const Board &board);
// The layers of each printed image in drawing order: one image for orders 0 and 1, the top side and then the bottom
// side for orders 2 and 3. Only the selected layers.
QList<QList<int>> printImages(const PrintSettings &settings);
// The size of the printout on paper in millimetres, all images and registration marks included.
QSizeF printSize(const Board &board,const PrintSettings &settings);
// Draws the printout with its top left corner at the painter's origin, in millimetres on paper.
void paintPrintout(QPainter &painter,const Board &board,const PrintSettings &settings);
// The info line: file, board, scale, date and time.
QString printInfo(const QString &file,const Board &board,const PrintSettings &settings);
// Prints on `printer`, the printout placed on its printable area as the settings say, stretched by the correction
// factors (horizontal, vertical) for printers that print slightly too large or too small.
void print(QPrinter &printer,const Board &board,const PrintSettings &settings,const QString &info,QPointF correction={1,1});
// The printout as a picture of `dpi` dots per inch on white, for the clipboard.
QImage printPicture(const Board &board,const PrintSettings &settings,double dpi=300);

class PrintSheet;
// The print preview: the settings at the left, the sheet with the printout at the right. The printout can be dragged
// on the sheet; the red frame is the printable area of the printer.
class PrintPreview : public QDialog {
public:
    // `printer` is the editor's printer (its page set up by "Drucker einrichten"); without one the preview has its own.
    PrintPreview(const Board &board,const PrintSettings &settings,const QString &file,QPrinter *printer=nullptr,QWidget *parent=nullptr);
    ~PrintPreview() override;
    PrintSettings settings() const{return current;}
    // Puts the printout in the middle of the printable area.
    void centre();
    QPrinter *printer() const{return device;}
    // The correction factors of the print (horizontal, vertical; 0.8 to 1.2), kept for the session as in the reference.
    static QPointF correction;
private:
    const Board &board;
    PrintSettings current;
    QString file;
    std::unique_ptr<QPrinter> own;QPrinter *device=nullptr;
    PrintSheet *sheet=nullptr;
    void changed();
    void specialSettings();
    void tilesDialog();
    void correctionDialog();
};
}
