#pragma once
#include "model.h"
#include "render.h"
#include <QDialog>
#include <QPageLayout>
#include <QRectF>
#include <array>
#include <functional>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPrinter;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QTabBar;

// Printing like the reference's print preview: for each sheet a scale (1:1 or free), an offset of the drawing on the
// paper, the paper's orientation and a banner print over several pages; the printable area is shown on the paper and
// marked where the drawing would be cut. All lengths in millimetres, the paper's top left corner at 0.
namespace openloch::schematic {
// The paper for a sheet: the printer's page in the orientation the settings ask for (automatic: like the sheet).
QPageLayout pageFor(const QPageLayout &printer,const Sheet &sheet,const PrintSettings &settings);
// The whole printed area of a banner print, its pages `step` apart, and the sheet's drawing on it.
QRectF bannerArea(const QPageLayout &page,const PrintSettings &settings);
QRectF drawingArea(const QPageLayout &page,const Sheet &sheet,const PrintSettings &settings);
// What is printed of a sheet: the elements of its title block and circuit, not its empty paper (sheet millimetres).
QRectF printedContent(const Sheet &sheet);
// Where the sheet's top left corner comes on the paper: the printed content's top left lies `offset` from the top left
// of the printable area of the first page, at any scale (as in the reference).
QPointF sheetOrigin(const QPageLayout &page,const Sheet &sheet,const PrintSettings &settings);
// Whether the drawing (the elements of the title block and the circuit) leaves the printable area: the sides it crosses
// (left, top, right, bottom).
std::array<bool,4> cutSides(const QPageLayout &page,const Sheet &sheet,const PrintSettings &settings);
// Scale and offset as the reference's "Anpassen": 98 % of the scale at which the printed content fills the printable
// area (of all banner pages less their overlaps), in whole percent, and in its middle; and the offset that centres it.
PrintSettings fitted(const QPageLayout &page,const Sheet &sheet,PrintSettings settings);
PrintSettings centred(const QPageLayout &page,const Sheet &sheet,PrintSettings settings);
// The reference's "Reset" of the offset: the sheet's or the printed content's top left corner at the paper's edge or at
// the printable area (the sheet's corner taken without the scale, as there).
enum class OffsetReset {SheetToPaper,SheetToPrintable,ContentToPaper,ContentToPrintable};
QPointF resetOffset(const QPageLayout &page,const Sheet &sheet,OffsetReset to);
// Prints the sheets (each page of a banner print a page) and returns how many pages were printed.
int printSheets(QPrinter &printer,const Document &document,const QList<int> &sheets,const QList<PrintSettings> &settings,const RenderOptions &options={});
// The sheets as a PDF file, each a page of its own size, drawn as vectors.
bool exportPdf(const Document &document,const QList<int> &sheets,const QString &file,const RenderOptions &options,QString *error=nullptr);

// "Druckvorschau": the printer and its set-up, the paper with the drawing (dragged to move it), the settings of the
// sheet (the free scale 10 to 800 % with slider and field, "Reset" of the offset in the reference's four ways), what is
// printed, and a bar of the sheets, those that would be cut marked "[!]" and counted beside it, its context menu a list
// of the sheets.
class PrintPreview : public QDialog {
public:
    PrintPreview(const Document &document,QList<PrintSettings> &settings,const QString &fileName,QWidget *parent=nullptr);
    ~PrintPreview() override;
    // The sheets the dialog would print now.
    QList<int> sheetsToPrint() const;
    void setSheet(int sheet);
    int sheet() const{return current;}
    QPrinter *printer() const{return device;}
    QRadioButton *oneToOne=nullptr,*freeScale=nullptr,*automatic=nullptr,*portrait=nullptr,*landscape=nullptr,*thisSheet=nullptr,*allSheets=nullptr,*someSheets=nullptr;
    QSlider *scaleSlider=nullptr;
    QSpinBox *scalePercent=nullptr;
    QPushButton *resetButton=nullptr;
    QLabel *cutCount=nullptr;
    QMenu *sheetMenu=nullptr;
    // The "?" button: the help pages, as the window around it shows them.
    std::function<void()> helpRequested;
    QDoubleSpinBox *offsetX=nullptr,*offsetY=nullptr,*overlap=nullptr;
    QSpinBox *bannerX=nullptr,*bannerY=nullptr;
    QLineEdit *selection=nullptr;
    QPushButton *printButton=nullptr;
    QTabBar *sheetBar=nullptr;
    void apply(const PrintSettings &s);
    void showSettings();
    void print();
private:
    friend class PaperView;
    const Document &document;
    QList<PrintSettings> &settings;
    QString fileName;
    QPrinter *device=nullptr;
    int current=0;
    bool updating=false;
    QWidget *paper=nullptr;
    QComboBox *printers=nullptr;
    void settingsChanged();
    void refreshMarks();
};
}
