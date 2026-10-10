#pragma once
#include "project.h"
#include <QDateTime>
#include <QJsonObject>
#include <QPageLayout>
#include <QSize>
#include <QSizeF>
#include <QStringList>
#include <QTransform>
#include <array>
#include <optional>
namespace openloch {
// LochMaster's print preview ("Druckvorschau"). A board carries up to ten print views ("Ansichten") in views[1..10] of
// its document: each places the same board on the sheets with its own switches, layers, scale and position, so that for
// example the component side and the mirrored solder side print on one sheet. Views that do not fit continue on further
// sheets without overlap. Lengths of the board are 1/100 mm, of the paper mm.
struct PrintView {
    // flags[0..6]: S/W, Wenden (flipped vertically in place), BMP-Rendering, Röntgen, Durchsicht, Freie Bereiche, Potenziale
    bool mono=false,flip=false,bitmaps=true,xray=true,through=false,free=false,potentials=false;
    // flags2[0..9]: Bohrungen, Trennstellen, Objekte & Symbole, Lineale, (unused), Hintergrund, Originalgröße 1:1,
    // mittig ausrichten, Leiterbahnen, Lötstellen (the solder marks at lead ends); flag: Texte
    bool holes=true,cuts=true,objects=true,rulers=true,spare=true,background=true,original=true,centred=false,copper=true,solderMarks=true,texts=true;
    int tilesX=1,tilesY=1;double gapX=0,gapY=0; // Kacheln: copies of the board in this view and the gaps between them
    double scale=1;  // the print scale, 1 with "Originalgröße 1:1"
    double zoom=1;   // the main window's zoom stored in the same record, kept unchanged
    int posX=0,posY=0; // the board point at the printable corner of the first sheet (so the board starts at -pos)
    int unit=2;      // ruler unit: 0 mm, 1 inch, 2 N (holes)
    bool backActive() const{return flip!=through;}
    // Rulers are not drawn while the view repeats the board (Kacheln).
    bool drawsRulers() const{return rulers&&tilesX==1&&tilesY==1;}
};
struct PrintSetup {
    int count=1;           // Anzahl Ansichten
    bool onlyOne=false,cutMarks=true,landscape=false,dataField=true;
    int sheet=1;           // Blatt Nr. for "nur ein Blatt drucken"
    std::array<PrintView,10> views;
};
// The defaults of a new board: view 1 centred, the others 10 mm further right and down each.
PrintView defaultPrintView(int index);
PrintSetup defaultPrintSetup();
// Reads and writes the print settings of a board document (an LM4 board or an LMB); missing records get the defaults.
PrintSetup printSetupOf(const QJsonObject &document);
void storePrintSetup(QJsonObject &document,const PrintSetup &setup);
// The print settings of a board: changed ones (Project::print) or those of its LochMaster file.
PrintSetup printSetupOf(const Project &project);
void storePrintSetup(Project &project,const PrintSetup &setup);

// A printer sheet like GetDeviceCaps reports it: the printable area in whole millimetres, the unprintable margins and
// the whole sheet in mm.
struct Paper {int width=0,height=0;double left=0,top=0,fullWidth=0,fullHeight=0;};
Paper paperOf(const QPageLayout &layout);
// "Datenfeld drucken" reserves this strip at the bottom of every sheet.
constexpr int dataFieldHeight=19;
int usableHeight(const Paper &paper,const PrintSetup &setup);
// Sheets across and down needed by the views 1..count: Int(extent·scale/size)+1, so an exact fit takes one more.
QSize sheetCount(const PrintSetup &setup,QSizeF board,const Paper &paper);
// "mittig ausrichten": the (tiled) board in the middle of all sheets.
void centreView(PrintView &view,const PrintSetup &setup,QSizeF board,const Paper &paper);
// Board coordinates of copy (tx, ty) to millimetres on the virtual paper of all sheets (origin = printable corner of
// the first sheet); Wenden mirrors the board vertically in place.
QTransform printTransform(const PrintView &view,QSizeF board,int tx=0,int ty=0);
// Links/Oben as the dialog shows them: millimetres from the edge of the paper, and back.
QPointF viewPosition(const PrintView &view,const Paper &paper);
void setViewPosition(PrintView &view,const Paper &paper,QPointF mm);
// Sheet (col, row) of "Blatt Nr." (numbered left to right, then top to bottom).
QPoint sheetCell(int number,QSize sheets);

// The data field: three lines left (project, size, editor) and three right (date, time, sheet).
struct DataField {QStringList left,right;};
DataField dataField(const QString &project,const QString &board,QSizeF size,int unit,const QString &editor,const QDateTime &when,int sheet,int sheets);
// The editor named in the data field: the registered owner and organisation on Windows, elsewhere user and computer.
QString printEditor();

// Korrekturfaktoren: 0.8 to 1.2, '.' or ',' as decimal separator; nullopt if invalid.
std::optional<double> correctionFactor(const QString &text);
}
