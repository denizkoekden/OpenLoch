#pragma once
#include "frontpanel.h"
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QToolButton;
namespace openloch::frontpanel {
class PanelEditor;
class ColorButton;
struct PenPreset {QString name;Pen pen;Machining machining=Machining::None;};
struct FillPreset {QString name;Fill fill;};
struct FontPreset {QString name,family="Arial",strokeFont;double height=3.5;bool bold=false,italic=false;};
QList<PenPreset> defaultPens();
QList<FillPreset> defaultFills();
QList<FontPreset> defaultFonts();
// The original's lists of pens, fills and fonts: Stifte.INI, FUELLUNG.INI and Fonts.ini in its Settings folder.
QList<PenPreset> frontDesignerPens(const QByteArray &ini);
QList<FillPreset> frontDesignerFills(const QByteArray &ini);
QList<FontPreset> frontDesignerFonts(const QByteArray &ini);
// A page of the symbol library: a FrontDesigner library file; pages in the own folder can be changed.
struct LibraryPage {QString name,path;bool writable=false;};

// The library on the right of the editor, with the pages Symbols, Pen, Fill, View and Font as in the original.
class PanelSidebar : public QWidget {
public:
    explicit PanelSidebar(PanelEditor *editor);
    void showPage(int page);              // 0 symbols, 1 pens, 2 fills, 3 views, 4 fonts
    int page() const;
    // The controls follow the editor's current pen, fill and font (after the selection changed).
    void syncFromEditor();
    void refreshViews();
    void refreshLibrary();
    // The installed stroke fonts changed.
    void refreshStrokeFonts();
    QList<LibraryPage> libraryPages() const{return pages;}
    QString ownLibraryFolder() const;
    QStringList extraLibraryFolders() const;
    void setExtraLibraryFolders(const QStringList &folders);
    // Adds a symbol to the current page, or to a new own page when the current one cannot be changed.
    bool addSymbol(const Element &symbol);
    void newPage();
    void renamePage();
    void deletePage();
    QList<PenPreset> pens;
    QList<FillPreset> fills;
    QList<FontPreset> fonts;
    void loadPresets();
    void savePresets() const;
    // Takes over the entries of the original's settings files (any of the three, recognised by their content).
    // `replace` empties a list before its entries come in; otherwise an entry replaces an own one of the same name
    // or is added. Returns the number of entries taken over; `notes` names files without any.
    int importFrontDesignerPresets(const QStringList &files,bool replace,QStringList *notes=nullptr);
private:
    PanelEditor *editor;
    QStackedWidget *stack=nullptr;
    QList<QToolButton*> pageButtons;
    // Symbols
    QComboBox *pageCombo=nullptr;QListWidget *symbols=nullptr;QList<LibraryPage> pages;Document pageDocument;
    // Pens
    QToolButton *toolButtons[3]{};ColorButton *penColor=nullptr;QDoubleSpinBox *penWidth=nullptr;QComboBox *penStyle=nullptr;QListWidget *penList=nullptr;
    // Fills
    QComboBox *fillStyle=nullptr;ColorButton *fillColor=nullptr,*fillColor2=nullptr;QListWidget *fillList=nullptr;
    // Views
    QListWidget *viewList=nullptr;
    // Fonts
    QToolButton *ttfButton=nullptr,*shxButton=nullptr,*boldButton=nullptr,*italicButton=nullptr;QFontComboBox *fontCombo=nullptr;QComboBox *strokeEdit=nullptr;
    QDoubleSpinBox *fontHeight=nullptr;QListWidget *fontList=nullptr;QWidget *fontPreview=nullptr;
    bool syncing=false;
    QWidget *symbolPage();
    QWidget *penPage();
    QWidget *fillPage();
    QWidget *viewPage();
    QWidget *fontPage();
    void fillPenList();
    void fillFillList();
    void fillFontList();
    void loadPage(int index);
    void savePage();
    void penControlsChanged();
    void fillControlsChanged();
    void fontControlsChanged();
};
}
