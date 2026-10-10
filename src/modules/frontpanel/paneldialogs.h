#pragma once
#include "frontpanel.h"
#include "panelgenerators.h"
#include <QToolButton>
#include <functional>

class QComboBox;
class QDoubleSpinBox;
namespace openloch::frontpanel {
// A button showing a colour; a click opens the colour dialog.
class ColorButton : public QToolButton {
public:
    explicit ColorButton(QWidget *parent=nullptr);
    void setColor(const QColor &c);
    QColor color() const{return value;}
    std::function<void(const QColor&)> changed;
private:
    QColor value{Qt::black};
};
// A length in mm, shown and entered in mm or inch.
class LengthEdit : public QWidget {
public:
    explicit LengthEdit(double mm=0,double minimum=0,double maximum=10000,QWidget *parent=nullptr);
    double value() const;
    void setValue(double mm);
    std::function<void(double)> changed;
private:
    QDoubleSpinBox *spin;QComboBox *unit;double minimum,maximum;
    void show(double mm);
};

// The fill choices as in the fill list: solid, none, six hatches and four gradients.
QStringList fillChoices();
int fillChoice(const Fill &fill);
void setFillChoice(Fill &fill,int choice);
QStringList penStyleNames();
QStringList machiningNames();

bool editPanelProperties(QWidget *parent,Panel &panel,bool create);
bool editGrid(QWidget *parent,Panel &panel);
bool editText(QWidget *parent,Element &text,bool create);
// The characters a stroke font draws, in a table as FrontDesigner's "SHX FONT": a click picks one. Nothing when the
// font is not installed or the table was closed.
QString pickStrokeFontCharacter(QWidget *parent,const QString &font);
bool askDrillDiameter(QWidget *parent,double &diameter);
bool editElementProperties(QWidget *parent,Document &document,Element &element);
bool askRegularPolygon(QWidget *parent,int &corners,double &radius,bool &inner,double &startAngle);
bool editDimensionStyle(QWidget *parent,DimensionStyle &style,bool &automaticColor,bool &askEveryTime);

struct DistributeOptions {
    bool horizontal=true,vertical=false;
    int horizontalReference=0,verticalReference=0;   // centres, left/top, right/bottom, neighbouring sides
    int horizontalMode=0,verticalMode=0;             // keep the overall size, new overall size, given gap
    double horizontalValue=10,verticalValue=10;
    bool horizontalPen=false,verticalPen=false;      // with the pen width (outer edge)
};
bool askDistribute(QWidget *parent,DistributeOptions &options);
struct GridAlignOptions {
    bool horizontal=true,vertical=false;
    int horizontalReference=0,verticalReference=0;   // centre, left/top, right/bottom
    bool horizontalPen=false,verticalPen=false;
};
bool askAlignToGrid(QWidget *parent,GridAlignOptions &options);
// Name and insertion point of a symbol for the library; `symbol` is a group or a single element. `title` for a symbol
// that is in the library already ("Eigenschaften Symbol" as in FrontDesigner).
bool editSymbol(QWidget *parent,const Document &document,Element &symbol,double grid,const QString &title={});
struct ImageExportOptions {
    QString format="PNG";
    int dpi=300;
    bool selectionOnly=false,background=true;
};
bool askImageExport(QWidget *parent,ImageExportOptions &options,QSizeF panelSize,bool hasSelection);
bool askAutosave(QWidget *parent,int &minutes);
// The character order of each installed shape font: as set for all, DOS or Windows, with where its umlauts lie.
bool editStrokeFontOrders(QWidget *parent);
void showUsedFonts(QWidget *parent,const Document &document);
// The fonts of the document's texts that are not installed (stroke fonts with " (Strichschrift)").
QStringList missingFonts(const Document &document);
// The overview of the used fonts as a window that does not block, as FrontDesigner shows it by itself when a document
// with missing fonts was opened. It deletes itself when closed.
QDialog *usedFontsWindow(QWidget *parent,const Document &document);
}
