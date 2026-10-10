#pragma once
#include "documents/libraryfolders.h"
#include "frontpanel.h"
#include "panelboards.h"
#include "panelhistory.h"
#include "panelgenerators.h"
#include "paneldialogs.h"
#include <QMainWindow>
#include <QMap>
#include <optional>

class QAction;
class QActionGroup;
class QDockWidget;
class QDoubleSpinBox;
class QLabel;
class QTabBar;
class QTimer;
class QTreeWidget;
namespace openloch::frontpanel {
class PanelView;
class PanelSidebar;
// The front panel editor: working area with rulers, the tools on the left, toolbars at the top and bottom, the panel
// register below and the library on the right (symbols, pens, fills, views, fonts), as the original arranges them.
// It is a main window so that its toolbars can be moved and hidden; a host can embed it as a widget.
class PanelEditor : public QMainWindow {
public:
    explicit PanelEditor(QWidget *parent=nullptr);
    ~PanelEditor() override;
    Document &document(){return doc;}
    PanelView *view() const{return area;}
    PanelSidebar *sidebar() const{return side;}
    QAction *action(const QString &name) const{return actions.value(name);}
    QString filePath() const{return path;}
    bool modified() const{return dirty;}

    // File functions. open() reads the native format, FrontDesigner projects and libraries and backup files (.BAK).
    void newDocument();
    bool open(const QString &file);
    bool save();
    bool saveAs(const QString &file={});
    bool maybeSave();
    // Shows another document; the history starts anew.
    void setDocument(const Document &document,const QString &file={});
    // After opening a document: when its texts use fonts that are not installed, the overview of the used fonts opens
    // by itself, without blocking, as in FrontDesigner. open() does this; whoever sets an opened document calls it.
    void reportMissingFonts();
    // Hilfe → Hilfethemen… (F1): the module's help page in the interface language.
    void showHelp();
    // Öffnen through a host such as the suite (docs/suite.md, "Öffnen in jedem Fenster"): when set, "Öffnen…" calls it
    // with the folder it would start in instead of showing its own dialog.
    std::function<bool(const QString &folder)> openHandler;
    // The library or stroke font folders changed elsewhere (the suite's overview): pages and fonts are read anew.
    void librariesChanged();
    // Saving through a host, such as a project of the suite: when set, "Speichern" and "Speichern unter" (also when
    // closing) call it instead of writing a file, `asNew` for "Speichern unter", and return what it returns. The host
    // takes the current document with documentData() whenever it likes and reports a finished save with markSaved().
    std::function<bool(bool asNew)> saveHandler;
    QJsonObject documentData() const{return doc.toJson();}
    void markSaved();
    // The host's circuit boards that can sit behind the panels (in the suite: every board of the project's perfboard
    // and circuit board documents); unset outside a project.
    std::function<QList<BoardSource>()> boardSources;
    // Asks boardSources again, as the boards may have changed in another window (the editor does so itself when its
    // window becomes active and before its dialogs).
    void reloadBoards();
    // Circuit boards behind the panel shown (docs/modules/frontpanel.md, "Bauteile und Platinen dahinter"): its
    // placements with the boards as boardSources last described them; a board that is gone has no source.
    struct BoardBehindNow {BoardBehind placement;std::optional<BoardSource> source;};
    QList<BoardBehindNow> boardsBehind() const;
    // The panel shown gets these boards behind it (they replace its list), as one undo step. With `moveHoles`, the
    // elements of the components of a board whose place changes move with it, so that holes and parts stay together.
    void setBoardsBehind(const QList<BoardBehind> &boards,bool moveHoles);
    // Holes for parts of the boards behind, each at the part's middle on the panel, named after it and standing for its
    // component; one undo step, selected afterwards.
    struct HoleFor {QString component,designator;QPointF at;double diameter=3;};
    void addHoles(const QList<HoleFor> &holes);
    // The elements of components on the panel shown, checked against the boards behind: one off its part (the part's
    // middle outside the hole, or outside the element), or one whose component no board behind has.
    struct BoardFinding {QString element,text;std::optional<QPointF> part;};
    QList<BoardFinding> compareWithBoards() const;
    // Moves the elements off their parts onto them (their point to the part's middle), one undo step.
    void moveOntoParts();

    // Editing commands; they work on the selection of the view and record one undo step each.
    void change(const std::function<void(Document&)> &edit);
    void copy();
    void cut();
    void paste();
    void duplicate();
    void removeSelected();
    void group();
    void ungroup();
    void combine();
    void uncombine();
    void toFront();
    void toBack();
    void mirror(bool horizontal);         // horizontal: left and right change places
    void rotateSelected(double degrees);  // counter-clockwise
    void align(int edge);                 // 0 left, 1 centre, 2 right, 3 top, 4 middle, 5 bottom
    void setContour(Corners corners);
    void resizeSelection(double width,double height);
    void moveSelection(double x,double y);
    void applyPen(const Pen &pen,Machining machining);
    void applyFill(const Fill &fill);
    void applyFont(const QString &family,double height,bool bold,bool italic,const QString &strokeFont);
    void distribute(const DistributeOptions &options);
    void alignToGrid(const GridAlignOptions &options);
    void undo();
    void redo();
    void selectPanel(int index);
    void addPanel(bool duplicate);
    void removePanel();
    void movePanel(int direction);
    // Inserts generated or imported elements: they stick to the cursor until placed.
    void place(const QList<Element> &elements);
    void addDimension(QPointF a,QPointF b,QPointF at);
    // Writes the backup (.BAK) beside the project now; AutoSpeichern calls it regularly.
    void writeBackup();
    // Current pen, fill and font for new elements; a single selected element passes its own on, as in the original.
    Pen currentPen;
    Machining currentMachining=Machining::None;
    Fill currentFill;
    QString currentFont="Arial",currentStrokeFont;
    double currentTextHeight=3.5,drillDiameter=3;
    bool currentBold=false,currentItalic=false;
    DimensionStyle dimensionStyle;
    bool dimensionAutoColor=true,dimensionAsk=false;
    std::function<void()> titleChanged;
protected:
    void closeEvent(QCloseEvent *) override;
    void changeEvent(QEvent *) override;
    // Files dropped on the window: front panels open, pictures are imported.
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;
private:
    Document doc;
    History history;
    QString path;
    bool dirty=false,historyOpen=false,refreshing=false,treeSync=false;   // refreshing: fields and tabs; treeSync: object tree
    PanelView *area=nullptr;
    PanelSidebar *side=nullptr;
    QTabBar *tabs=nullptr;
    QTreeWidget *tree=nullptr;
    QDockWidget *treeDock=nullptr,*sideDock=nullptr;
    QMap<QString,QAction*> actions;
    QActionGroup *toolGroup=nullptr;
    QDoubleSpinBox *angleEdit=nullptr,*widthEdit=nullptr,*heightEdit=nullptr,*xEdit=nullptr,*yEdit=nullptr,*contourEdit=nullptr;
    QLabel *info=nullptr,*position=nullptr;
    QTimer *autosave=nullptr;
    int autosaveMinutes=0;
    int polygonCorners=6;double polygonRadius=10,polygonStart=90;bool polygonInner=false;
    DistributeOptions distributeOptions;
    GridAlignOptions gridOptions;
    ImageExportOptions imageOptions;
    QAction *add(const QString &name,const QString &text,const QString &icon,const std::function<void()> &run,const QList<QKeySequence> &keys={});
    void buildActions();
    void buildMenus();
    void buildToolbars();
    void begin();
    void commit();
    void markChanged();
    void refreshAll();
    void updateTitle();
    void updateTabs();
    void updateTree();
    void updateTreeSelection();
    void updateSelectionTools();
    void updateActions();
    void adoptStyle();
    void loadSettings();
    void saveSettings() const;
    void showContextMenu(QPoint global,int node);
    void showTreeMenu(QPoint at);
    void showProperties(const QString &id);
    Element styled(ElementType type) const;
    void addText(QPointF at);
    void addDrill(QPointF at);
    void importImage();
    void importImageFile(const QString &file);
    void exportImage();
    void exportHpgl();
    void print();
    void scaleWizard(const QString &existing={});
    void cutoutWizard(const QString &existing={});
    void regularPolygon();
    void addToLibrary();
    void addPanelsFromFile();
    // Circuit boards behind the panel: the dialogs and the underlay in the view.
    QList<BoardSource> boardCache;
    void editBoardsBehind();
    void holesFromBoards();
    void showBoardComparison();
    void refreshUnderlay();
    void applyToSelection(const std::function<void(Element&)> &apply,bool includeTexts);
    QList<Element> selectedCopies();
    QStringList selectedTopLevel() const;
    QString startFolder() const;
    void rememberFolder(const QString &file);
};
// The front panel's library folders (docs/suite.md, "Bibliotheken"): its own symbol pages (frontpanel/libraryFolder,
// else Dokumente/OpenLoch/Frontplattensymbole) and further folders that are only read (frontpanel/extraLibraries). The
// stroke fonts have their own pair, strokeFontFolders() and setStrokeFontFolders() (strokefont.h).
LibraryFolders symbolLibraryFolders();
void setSymbolLibraryFolders(const LibraryFolders &folders);
}
