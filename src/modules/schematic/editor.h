#pragma once
#include "model.h"
#include "partslist.h"
#include "print.h"
#include "render.h"
#include "settings.h"
#include "view.h"
#include <QMainWindow>
#include <QHash>
#include <QKeySequence>
#include <QMap>
#include <functional>

class QAction;
class QActionGroup;
class QDockWidget;
class QLabel;
class QMenu;
class QPushButton;
class QTabBar;
class QTableWidget;
class QToolBar;
class QToolButton;
class QWidget;

namespace openloch::schematic {
class ClipboardDialog;
class ExportDialog;
class ProblemDialog;
class TextConstantsDialog;
class TextDialog;
class VariablesDialog;
class SearchPanel;
class LibraryPanel;
class PropertiesPanel;
// The menus of the editor window in the reference's order: each menu's German title (shown through ui()) and the
// names of its actions (Editor::action), an empty name for a separator. "B&auteileditor" is shown only while a
// component is edited. The editor builds its menu bar from this table; new actions go only here.
QList<std::pair<QString,QStringList>> editorMenus();

// The schematic editor ("Schaltplan") as a main window: the library at the left, the drawing modes beside it, the
// sheet with its tabs below, the properties at the right, the switches for snapping in the status bar. It owns the
// document, its undo steps and file name.
class Editor : public QMainWindow {
public:
    explicit Editor(QWidget *parent=nullptr);
    const Document &document() const{return doc;}
    // Replaces the document (no undo step); `path` is its own-format file, empty for new or imported documents.
    void setDocument(const Document &document,const QString &path={});
    // "Neu von Vorlage": the document of `file` (own or sPlan) as a new, unsaved one with ids of its own.
    bool newFromTemplate(const QString &file,QString *error=nullptr);
    // The "Grundeinstellungen" that change how the window works (the grid; the library is read again by the caller).
    void applySettings(const GeneralSettings &settings);
    // "Autospeichern" now: a changed document with a file also beside it as "<name>.bak" (own format).
    void autosave();
    // Where a file dialog of drawings, title blocks or exports starts ("Arbeitsverzeichnisse"): the folder set in the
    // settings, else the last one used for that kind (kept with the other preferences), else `fallback`.
    enum class Work {Drawings,Forms,Exports};
    QString startFolder(Work kind,const QString &fallback={}) const;
    void usedFolder(Work kind,const QString &file);
    void newDocument();

    // Files: opening reads the own format and sPlan files (.spl7, .spl8); what a sPlan file holds that OpenLoch shows
    // differently is listed in a bar above the sheet (openNotes). Saving writes the own format, or a sPlan file by its
    // suffix. "Speichern" writes an opened sPlan file back only when nothing is lost, otherwise it asks for an own
    // file. Exporting writes the current sheet (or all) as pictures, or the document as a sPlan file.
    bool openFile(const QString &path,QString *error=nullptr);
    bool saveFile(const QString &path,QString *error=nullptr);
    bool exportSplan(const QString &path,int version,QString *error=nullptr);
    // The title block of the current sheet from and to a sPlan title block file (.sbk); loading replaces its elements.
    bool loadTitleBlock(const QString &path,QString *error=nullptr);
    bool saveTitleBlock(const QString &path,QString *error=nullptr);
    QStringList openNotes() const{return notes;}
    // The sPlan version of the file the document is kept in (70, 80), 0 for the own format.
    int fileVersion() const{return pathVersion;}
    // A picture file by its ending: PNG, JPEG, BMP, SVG or EMF (`dpi` only for the first three); of the current sheet all
    // of it, all its elements or the selected ones (see ExportArea), or every sheet but the spare ones in a file of its
    // own, the sheet's number appended ("Name_2.png").
    bool exportImage(const QString &path,int dpi,bool blackAndWhite,bool allSheets,bool transparent,QString *error=nullptr,ExportArea area=ExportArea::Sheet);
    // The current sheet or all sheets but the spare ones as a PDF file, each sheet a page of its size.
    bool exportPdfFile(const QString &path,bool blackAndWhite,bool allSheets,QString *error=nullptr);
    // What an area of the current sheet takes in millimetres: the sheet, or the bounds of its elements or of the selected
    // ones and 4 mm around them (empty without such elements).
    QRectF exportFrame(ExportArea area) const;
    // "Zwischenablage...": the elements of the current sheet (with the title block) or only the selected ones in their
    // frame as a picture with `dpi`.
    QImage clipboardImage(int dpi,bool selectionOnly) const;
    QString filePath() const{return path;}
    QString displayName() const;
    bool isModified() const{return modified;}
    // Asks to save a modified document; false if the user cancels.
    bool maybeSave();
    // "Speichern" and "Speichern unter" (also when closing): own file, or through a host such as a project of the
    // suite. When `saveHandler` is set the editor calls it instead of writing a file, `asNew` for "Speichern unter",
    // and returns what it returns; the host takes the document with documentData() and reports the save with
    // markSaved().
    bool save();
    bool saveAs();
    std::function<bool(bool asNew)> saveHandler;
    // "Öffnen" through a host such as the suite: when set, the editor calls it with the folder to start in (the drawing
    // folder of the settings, schematic/drawingFolder, else empty) instead of showing its own dialog, and the host opens
    // what is chosen. Without it the editor opens the file itself.
    std::function<bool(const QString &folder)> openHandler;
    QJsonObject documentData() const{return toJson(doc);}
    void markSaved();
    // The settings kept between runs ("schematic/…"): snapping switches, library columns, the library folder.
    // A window around the editor calls this; without it the defaults hold and nothing is written.
    void loadPreferences();

    // Editing, each one undo step.
    void change(const std::function<void(Document&)> &edit);
    void undo();
    void redo();
    bool canUndo() const{return !past.isEmpty();}
    bool canRedo() const{return !future.isEmpty();}
    void switchSheet(int index);
    // New empty sheets (or copies of the current one, or of `source`) at `position`.
    void insertSheets(int position,int count,bool copies,int source=-1);
    void removeSheet(int index);
    void reorderSheets(const QList<int> &order);
    void deleteSelection();
    void copySelection();
    void cutSelection();
    void pasteClipboard();
    void duplicateSelection();
    void rotateSelection(double degrees);           // counter-clockwise
    void mirrorSelection(bool vertically);
    void groupSelection();
    void ungroupSelection();
    // Drawing order: to the front or back, or one step.
    void reorderSelection(int how);                 // 0 front, 1 back, 2 one forward, 3 one back
    // "Am Raster ausrichten": each selected element moved so that its gridPoint() lies on the grid.
    void alignToGrid();
    // A node, or all nodes, of a line, polygon or curve on the grid.
    void nodeToGrid(const QString &id,int node);
    void nodesToGrid(const QString &id);
    // "Wandeln in Linie", "Wandeln in Polygon" (see convertedTo).
    void convertTo(const QString &id,ItemType type);
    // "Ausrichten" and "Gleichmäßig verteilen" for the selected elements (at least two, three for spreading).
    void alignSelection(Alignment how);
    // A special shape for the mode "Spezialformen", after its settings where it has some.
    void chooseSpecialShape(SpecialShape shape);
    // The kind of new dimensions ("Standard", "Radial", "Durchmesser", "Winkel"); the mode becomes "Bemaßung".
    void setDimensionKind(DimensionKind kind);
    // The presets of the drawing modes by their key in the settings.
    QList<std::pair<QString,Item*>> presets();
    // "Bitmap-Explorer": the pictures of the document, reduced to a maximum resolution.
    void bitmapExplorer();
    // "Childliste (Kontaktspiegel)" of a parent, put on the sheet.
    void childListDialog(const QString &parentId);
    // "Neue vertikale/horizontale Magnetlinie": in the middle of the visible part of the sheet.
    void newGuide(bool vertical);
    void spreadSelection(bool horizontally);
    // A component from the selected elements, its insertion point on the grid at their top left corner.
    void makeComponent();
    // The selected components (or the selection as one clip) onto the current library page, if it is an own one.
    void copyToLibrary(bool clip);
    // New sheets with the symbols of library pages.
    void librarySheets(const QList<int> &pages);
    void reloadLibrary();
    // The library folders changed (setLibraryFolders, the suite's overview): the pages are read again.
    void librariesChanged();
    // "Bauteile neu nummerieren": the sheets, then the order and the components.
    void renumberDialog();
    // "Bauteilbeschriftung": the lettering of designators, values and contacts in a scope of LetteringDialog::Scope.
    void letteringDialog();
    void applyLettering(const Lettering &designator,const Lettering &value,const Lettering &contacts,int scope);
    // "Blatt speichern": the current sheet as a file of its own (".blt" as sPlan's sheet file, an sPlan 8 file); "Blatt
    // laden": the sheets of a file (also sPlan's ".blt") after it.
    bool saveSheet(const QString &path,QString *error=nullptr);
    // The current sheet as a document of its own; for an sPlan sheet file it keeps what the sheet was read from.
    Document sheetDocument(bool forSplan=false) const;
    bool loadSheets(const QString &path,QString *error=nullptr);
    // The text constants of the settings ("Textkonstanten"), changed in their dialog.
    QStringList textConstants() const;
    void editTextConstants();
    // The reference's problems of the current sheet: its elements wholly outside it, and the document's pictures of more
    // than 300 dpi (in either direction as placed). The button "Probleme" shows only while there are some.
    struct Problems {
        QStringList outside;
        int pictures=0;
        bool any() const{return !outside.isEmpty()||pictures>0;}
    };
    Problems problems() const;
    // Moves elements wholly outside the sheet onto its edge, as the reference: one left of it with its left side to the
    // sheet's left edge, one right of it with its right side to the right edge, the same up and down.
    void moveOntoSheet(const QStringList &ids);
    void problemDialog();
    std::function<void(ProblemDialog*)> problemShown;
    // "Zuletzt geöffnete Dateien" in the Datei menu: the last eight, newest first (kept with the preferences).
    QStringList recentFiles() const;
    void rememberFile(const QString &file);
    // "Hilfethemen…" (F1): the module's help pages, at a section (an anchor of the page) if one is given.
    // Over a modal dialog (`over`) the pages open as its own window, which that dialog does not block.
    void showHelp(const QString &section={},QWidget *over=nullptr);
    // Called with the dialog before it is shown (for tests).
    std::function<void(TextDialog*)> textDialogShown;
    std::function<void(TextConstantsDialog*)> textConstantsShown;
    std::function<void(VariablesDialog*)> variablesShown;
    std::function<void(ExportDialog*)> exportShown;
    std::function<void(ClipboardDialog*)> clipboardShown;
    // Changes the text of a text, text box, net label or contact in the extended text input.
    void editText(const QString &id);
    // "Bild aus Datei einfügen": a picture to be put down with a click; "Skalieren", "Elemente einfärben" and
    // "Strichstärke verändern" for the selection (with what its groups and components hold).
    bool insertImage(const QString &path,QString *error=nullptr);
    void scaleSelection(double factor);
    void colourizeSelection(const QColor &colour);
    // "Bemaßung fixieren" (true) and "Bemaßung Fixierung aufheben" (false) for the selected dimensions, also in groups.
    void fixDimensions(bool fix);
    void changePenWidths(bool fixed,double value);
    void penWidthDialog();
    // "Stückliste erstellen": the sheets, then the list; put onto the current sheet as a group of texts and lines.
    void partsListDialog();
    // Goes to a component or text (by id) on whatever sheet and selects it (or the group it is in).
    void showElement(const QString &id);
    // Parent and child: link the selected components to a parent.
    void linkToParent();
    // Text links: choose the target of the selected text; follow the link of a text (false if it has none).
    void chooseLinkTarget();
    bool followLink(const QString &id);
    void insertPartsList(const PartsTable &table,const PartsDrawing &drawing);
    // "Inhaltsverzeichnis einfügen": the chosen sheets with number and name as a table on the current sheet.
    void contentsDialog();
    void insertContents(const QList<int> &sheets,const PartsDrawing &drawing);
    // The selected components back into plain elements.
    void dissolveComponents();
    // The component editor ("Bauteileditor") for the selected component; leaving keeps or undoes the changes.
    void editComponent();
    void leaveComponentEditor(bool keep);
    bool editingComponent() const;
    void setTitleBlockMode(bool on);
    void setTitleBlock(const TitleBlock &block);

    QAction *action(const QString &name) const{return actions.value(name);}
    SheetView *view() const{return sheetView;}
    LibraryPanel *library() const{return libraryPanel;}
    PropertiesPanel *properties() const{return propertiesPanel;}
    QTabBar *sheetTabs() const{return tabs;}
    std::function<void()> titleChanged;
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    Document doc;
    QString path,importedName;
    int pathVersion=0;
    QStringList notes;
    QWidget *noticeBar=nullptr;
    SearchPanel *searchPanel=nullptr;
    QLabel *noticeText=nullptr;
    void showNotes(const QStringList &notes);
    void exportSplanDialog(int version);
    bool modified=false,preferences=false;
    GeneralSettings general;        // the "Grundeinstellungen" in use (standard ones until loadPreferences or the dialog)
    class QTimer *autosaveTimer=nullptr;
    QHash<QString,QKeySequence> standardKeys;   // of the drawing modes, for "Hotkeys"
    QHash<int,QString> lastFolders;             // by Work
    QList<QAction*> modeActions() const;
    // A new sheet as "Neues Blatt" sets it; the pictures of its title block go into `document`.
    Sheet presetSheet(const QString &name,Document &document) const;
    // The print settings of the sheets for this session, as the print preview left them.
    QList<Document> past,future;
    SheetView *sheetView=nullptr;
    LibraryPanel *libraryPanel=nullptr;
    PropertiesPanel *propertiesPanel=nullptr;
    QTabBar *tabs=nullptr;
    // The panels at the right: the properties and the list of sheets, each shown, hidden or set free (docks).
    QDockWidget *propertiesDock=nullptr,*sheetsDock=nullptr;
    QTableWidget *sheetList=nullptr;
    QList<QAction*> recentActions;
    mutable QHash<QString,QSize> pictureSizes;      // the pixels of a picture resource, for problems()
    void refreshSheetList();
    void refreshRecent();
    void refreshProblems();
    QWidget *modeBar=nullptr;
    QLabel *modeTitle=nullptr,*coordinates=nullptr,*relative=nullptr,*gridLabel=nullptr,*zoomLabel=nullptr,*hint=nullptr,*scaleLabel=nullptr;
    QPushButton *modeOk=nullptr,*modeCancel=nullptr;
    QToolBar *modes=nullptr;
    QActionGroup *toolGroup=nullptr;
    QMap<QString,QAction*> actions;
    QMap<QString,QMenu*> menus;
    Item componentBefore;           // the component as it was when its editor opened
    bool refreshing=false;
    void createActions();
    void createMenus();
    void createToolBars();
    QWidget *createStatusBar();
    void snapshot();
    void touched();
    void refresh();
    void refreshTabs();
    void refreshActions();
    void refreshTitle();
    QList<Item> selectedItems() const;
    void showContextMenu(QPoint at,const QString &id,int node);
    void showSheetMenu(QPoint at,int index);
    void editItem(const QString &id);
    bool askText(Item &item);
    // The extended text input with the document's user variables and the text constants.
    bool extendedText(QString &text,const QString &family);
    void chooseGrid();
    void exportDialog();
    void insertSheetDialog(bool copies);
    void sortSheetsDialog();
    void titleBlockDialog();
    void variablesDialog();
    void textDialog();
};
}
