#pragma once
#include <memory>
#include <optional>
#include "copper.h"
#include "fabrication.h"
#include "milling.h"
#include "model.h"
#include "netcheck.h"
#include "printing.h"
#include "view.h"
#include "documents/libraryfolders.h"
#include <QMap>
#include <QWidget>
#include <functional>

class QAction;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QFormLayout;
class QPrinter;
class QLabel;
class QListWidget;
class QRadioButton;
class QTabBar;
class QTabWidget;
class QToolButton;
class QTimer;
class QTreeWidget;
class QWidget;

namespace openloch::pcb {
struct Footprint;
class BoardOverview;
class MacroPanel;
// The menus of a window around the editor in the reference's order: each menu's German title (shown through ui())
// and the names of its actions (Editor::action), an empty name for a separator. A window adds its own quit action to
// "&Datei". Meant for the demo program and the suite's circuit board window alike.
QList<std::pair<QString,QStringList>> editorMenus();
// The dialog for a new board, as the reference asks for one: only a working area, or a rectangular or round outline with
// a margin round it, and the name (`name` offered). The board it sets up (see newBoard; the origin at the bottom left
// corner of the outline, or at the top left one with `originTopLeft`), nothing when cancelled. The editor uses it for a
// new document and a new board; a host such as the suite may use it for a board of a project.
std::optional<Board> askNewBoard(QWidget *parent,const QString &name,bool originTopLeft=false);

// The PCB editor ("Leiterplatte") as one widget: tools and their settings at the left as in the reference, the board
// with its tabs and the layer switches below, properties, the component library and the design rule check at the
// right. It owns the document, its undo steps and file name; a window around it shows the actions in its menus and
// toolbars.
class Editor : public QWidget {
public:
    explicit Editor(QWidget *parent=nullptr);
    const Document &document() const{return doc;}
    // Replaces the document (no undo step); `path` is its own-format file, empty for new or imported documents.
    void setDocument(const Document &document,const QString &path={});
    BoardView *view() const{return board;}

    // Files. Opening reads the own format and Sprint-Layout files; saving writes the own format, exporting Sprint-Layout.
    // What converting an older Sprint-Layout file changed or left out is listed in a bar above the board (openNotes).
    bool openFile(const QString &path,QString *error=nullptr);
    QStringList openNotes() const{return notes;}
    bool saveFile(const QString &path,QString *error=nullptr);
    // The board as a picture (.png, .bmp, .jpg or .gif by the suffix) with its visible layers: in colour as on screen,
    // or black on white; `dpi` pixels per inch, stored in the file (not in GIF, which has no place for it). A GIF has
    // hard edges, so that its palette holds the layer colours.
    bool exportImage(const QString &path,int dpi,bool colour,QString *error=nullptr);
    // The board as an Enhanced Metafile (vectors, the visible layers as on the screen, a hundredth of a millimetre per unit).
    bool exportEmf(const QString &path,QString *error=nullptr);
    // Version 6 (.lay6) or 4 (.lay of Sprint-Layout 4.0, which leaves out I1, I2 and U; `skipped` counts those elements).
    bool exportSprint(const QString &path,QString *error=nullptr,int version=6,int *skipped=nullptr);
    QString filePath() const{return path;}
    QString displayName() const;
    bool isModified() const{return modified;}
    // Asks to save a modified document; false if the user cancels.
    bool maybeSave();
    // Saving through a host, such as a project of the suite, as with frontpanel::PanelEditor: when set, "Speichern"
    // and "Speichern unter" (also when closing) call it instead of writing a file, `asNew` for "Speichern unter", and
    // return what it returns. The host takes the document with documentData() whenever it likes and reports a
    // finished save with markSaved().
    std::function<bool(bool asNew)> saveHandler;
    // Opening through a host such as the suite (docs/suite.md, "Öffnen in jedem Fenster"): when set, "Öffnen…" calls it
    // with the folder it would start in instead of showing its own dialog.
    std::function<bool(const QString &folder)> openHandler;
    // The library folders changed elsewhere (the suite's overview): the macro library reads them anew.
    void librariesChanged();
    QJsonObject documentData() const{return toJson(doc);}
    void markSaved();

    // The component interface for projects. components(board) lists the components of a board with identifier,
    // designator, value and pins; componentOf() names the component of an element of the active board.
    QString componentOf(int element) const{return pcb::componentOf(doc.board(),element);}
    // Designator and value from the project for the component with this identifier, on whichever board it sits, as one
    // undo step; false if the document has no such component.
    bool setComponent(const QString &id,const QString &designator,const QString &value);
    // A footprint (a library entry's) on the pointer as the component with this identifier, designator and value (a
    // value text comes new if the footprint has none); false if the document has that component already or the
    // footprint has no designator.
    bool placeComponent(const Footprint &footprint,const QString &id,const QString &designator,const QString &value);

    // The target connections of the project's schematic, set by the suite as for the perfboard: whether the project has
    // a schematic, and its components and nets (may throw when the schematic cannot be read). Without them the
    // comparison's actions are disabled and its side tab hidden.
    std::function<bool()> hasSchematic;
    std::function<documents::Targets()> targets;
    bool schematicAvailable() const{return hasSchematic&&targets&&hasSchematic();}
    // The suite: the documents of the project changed, so a schematic may have come, gone or changed. The comparison's
    // actions and side tab follow at once, and the comparison runs again while its tab or airwires are shown (as
    // Window::projectChanged() for the perfboard).
    void projectChanged();
    // "Mit Schaltplan vergleichen": the active board against the schematic (checkNets), listed in the side tab
    // "Schaltplan" (missing, ambiguous and extra components, pins to assign, open nets as their pieces, joined nets,
    // designators and values that differ; a click on a line selects its elements and centres them, a double click zooms
    // to them), and the airwires of open nets on the board while "Luftlinien aus dem Schaltplan" is on. The comparison
    // follows every change, undo and redo while its tab or the airwires are shown. `reveal` brings the tab forward and
    // reports a schematic that cannot be read in a message as well.
    NetCheck compareWithSchematic(bool reveal=true);
    // "Aus Schaltplan übernehmen…": links components found by their designator, takes designator and value from the
    // schematic and names pins 1 and 2 of resistors, capacitors and coils the way round they fit, after a list of
    // what changes, as one undo step.
    void takeOverFromSchematic();
    // "Anschlüsse zuordnen…": the pads of the component selected (else the first one whose pins need it) named after
    // the pins of its component in the schematic, the pads numbered on the board meanwhile; one undo step.
    void assignPinsDialog();
    // "Fehlende Bauteile setzen…": for each component of the schematic the board lacks (and the document has nowhere) a
    // choice from the module's library (partChoices; a fitting footprint of the same kind preselected, else none); the
    // chosen ones laid beside the board (layBeside), named, valued and linked, as one undo step, selected and shown
    // with the board, the airwires from the schematic switched on to show where they belong.
    void placeMissingParts();

    // Editing (all with one undo step each)
    void undo();
    void redo();
    bool canUndo() const{return !past.isEmpty();}
    bool canRedo() const{return !future.isEmpty();}
    void deleteSelection();
    void rotateSelection(double degrees);           // counter-clockwise about the middle of the selection
    void mirrorSelection();                         // left-right about the middle of the selection
    void mirrorSelectionVertically();               // top-bottom about the middle of the selection
    // Lines up the selected elements (groups as a whole) with the selection's edge or middle: 0 left, 1 right, 2 top,
    // 3 bottom, 4 the same middle from left to right, 5 the same middle from top to bottom.
    void alignSelection(int edge);
    // Puts the selection on the grid: single elements each (tracks and areas node by node), groups as a whole.
    void alignToGrid();
    // Copies of the selection in a grid of `columns` × `rows` (the selection itself is the first), `step` apart.
    void tileSelection(int columns,int rows,QPointF step);
    // Copies of the selection around a circle, `count` in all (the selection is the first), `angle` degrees apart
    // counter-clockwise. The circle of `radius` runs through the reference point (the selection's middle moved by
    // `start`) and has its centre to the left of it; `turn` turns the copies with the circle.
    void arrangeInCircle(int count,double angle,double radius,bool turn,QPointF start={});
    // The settings of the reference's "Grundeinstellungen" from the user's preferences (a window around the editor
    // calls this; without it the defaults hold): the view's switches, designators and values kept readable after
    // turning, new boards with their origin at the top left.
    void loadPreferences();
    bool readableLabels=true,originTopLeft=false;
    // More settings of the reference's "Grundeinstellungen": fabrication data (Gerber, Excellon, milling, component data)
    // counted from the origin, else from the top left corner of the working area; texts no lower than their strokes
    // allow (at least 0.15 mm wide: thin from 2.5, normal from 1.5, thick from 1 mm); how many undo steps are kept (1 to
    // 500); a backup every few minutes (1 to 60); the folders the file dialogs start in (empty: the one used last), one
    // for all if wanted, and the macro folder; copper thickness (µm) and temperature rise (K) for the current a track
    // carries; the colour scheme (0 the reference's, 1 to 3 the user's own).
    bool camOrigin=true,limitTextSize=false;
    int undoLimit=100;
    void setUndoLimit(int steps);
    bool autosave=false;int autosaveMinutes=5;
    void setAutosave(bool on,int minutes);
    // The backup of the document's own file: its name with ".bak" added, written in the own format when the document has
    // a file, is not saved through a host and changed since the last save or backup; true if it wrote. A layout opened
    // from a Sprint-Layout file backs up next to it, under the name it would be saved as (name.olpcb.bak).
    QString backupFile() const;
    bool autosaveNow();
    enum Folder {Layouts,Fabrication,Pictures,Templates,Milling};
    QStringList folders{QString(),QString(),QString(),QString(),QString()};
    bool oneFolder=false;
    QString macroFolder;
    QStringList extraMacroFolders;      // further macro folders, only read
    QString startFolder(Folder kind) const;
    double copperThickness=35,temperatureRise=20;
    int colourScheme=0;
    std::array<Colours,3> userColours{Colours::standard(),Colours::standard(),Colours::standard()};
    void setColourScheme(int scheme);
    // The lowest height a text with this thickness may have (0.1 mm without the limit).
    double minimumTextHeight(int thickness) const;
    // Macros on the pointer, as the macro library and "Makro laden" place them: a macro with a designator and at most one
    // component number becomes one component; `asComponent` makes one of a macro without designator too (a designator and
    // a value at its top left corner) and opens the component dialog once it is down.
    void placeMacro(QList<Element> elements,bool asComponent);
    // The macro's elements as they follow the pointer (made a component, centred on the origin, ready for the board).
    QList<Element> macroElements(QList<Element> elements,bool asComponent) const;
    // The selection as a macro: into `file`, or asked for, starting in `folder` (else the macro folder).
    bool saveMacroAs(const QString &folder,QString file={});
    MacroPanel *macroPanel() const{return macros;}
    // A calculator of the current a track carries and the width a current needs, with the preferences' copper and
    // temperature rise; `width` starts it with a track's width.
    void currentDialog(double width=-1);
    // Where the preferences live: empty for the system's place of the module's settings ("OpenLoch/Leiterplatte"), else
    // an INI file of that name (tests keep the user's settings untouched this way).
    static void setPreferencesFile(const QString &file);
    // Favourite track widths, as the menu of the button next to the width offers them: kept in the preferences, sorted.
    QList<double> widthFavourites() const{return favourites;}
    void setWidthFavourites(QList<double> widths);
    // Favourite sizes of pads (outer diameter, drill) and SMD pads (width, height) on the buttons next to their fields,
    // with the same menu; kept in the preferences, sorted.
    enum SizeKind {PadSizes,SmdSizes};
    QList<QPointF> sizeFavourites(SizeKind kind) const{return kind==PadSizes?padFavourites:smdFavourites;}
    void setSizeFavourites(SizeKind kind,QList<QPointF> sizes);
    // Swaps width and height of the SMD fields, as the button below them does; selected SMD pads follow (one undo step).
    void swapSmdSize();
    // The meaning of the layers and which of them components use, in a small window (the "?" beside the layers).
    void showLayerInfo();
    // The help line under the board for a tool: what it does, and which keys change it (a modifier for no grid or half
    // the grid, Space for the bend while drawing tracks and areas).
    static QString toolHelp(BoardView::Tool tool);
    // What a command of the tool bar does, for the help line (empty for others).
    static QString commandHelp(const QString &name);
    QString keyHints(BoardView::Tool tool) const;
    // Nodes of tracks and areas as the popup menu of a node offers them, each one undo step; false where it does not
    // apply: removing a node (a track keeps at least two, an area three), putting one or all nodes of the element onto
    // the grid (counted from the origin, as drawing snaps), splitting a track at an inner node into two tracks that
    // share it and keep everything else (groups, component, autoroute mark and pads; the square ends stay outside);
    // joining a track at an end node with another track on its layer ending there into one track that keeps the first
    // one's settings and both outer ends (joiningTrack names that other track, -1 for none).
    bool removeNode(int element,int node);
    bool alignNode(int element,int node);
    bool alignNodes(int element);
    bool splitTrack(int element,int node);
    int joiningTrack(int element,int node) const;
    bool joinTracks(int element,int node);
    // The grids the keys 1 to 9 choose, in millimetres; chosen freely as in the reference (kept in the preferences
    // when set in their dialog).
    static QList<double> defaultGridKeys();
    QList<double> gridKeys() const{return gridValues;}
    void setGridKeys(const QList<double> &grids);
    // The grid menu beside the grid field, as the reference's grid popup: the grids of the inch pitch (1/64 to 2 of
    // 2.54 mm) and metric ones, own grids to add and remove (kept in the preferences, sorted, each once), the grid
    // keys, lines or dots, which lines are stronger, and whether the grid shows.
    static QList<std::pair<double,QString>> inchGrids();
    static QList<double> metricGrids();
    QList<double> ownGrids() const{return userGrids;}
    void setOwnGrids(const QList<double> &grids);
    void chooseGrid(double grid);
    // The angle of the rotate action in degrees (clockwise); Shift or the second action turn the other way.
    void setRotationStep(double degrees);
    double rotationStep() const{return rotation;}
    void otherSide();                               // to the other side of the board: mirrored, top and bottom layers swapped
    void groupSelection();
    void ungroupSelection();
    void copySelection();
    void pasteClipboard();
    void duplicateSelection();
    void addBoard();
    void addBoard(const Board &board);              // behind the others and active, one undo step
    void removeBoard();
    void switchBoard(int index);
    // More board commands, as the reference's board menu and tab menu offer them: a copy of the active board right after
    // it (its components get identifiers of their own), the active board to the first or the last place, the boards of
    // another file after the active board or at the end (`last`), and the active board alone in a file of its own. Each
    // change is one undo step; false where nothing happened.
    bool copyBoard();
    bool moveBoard(bool toEnd);
    bool insertBoards(const QString &file,bool last,QString *error=nullptr);
    bool saveBoardAs(const QString &file,QString *error=nullptr);
    // AutoMasse of the active copper layer on or off.
    void setGroundPlane(bool on);
    // Removes airwires whose pads copper already joins; returns how many were removed.
    int removeRoutedAirwires();
    // Solder mask openings back to all pads and nothing else.
    void resetSolderMask();
    // Runs the design rule check with the rules of the DRC panel, on the whole board or only on what the view shows, and
    // lists the findings (all of them marked on the board at first).
    QList<Finding> runDesignRuleCheck(bool visibleOnly=false);
    // The rules as the DRC panel sets them.
    Rules designRules() const;
    // Edits the designator, value and component data of the component with this designator text in a dialog.
    bool editComponent(int designator);
    // Its elements as plain elements: no component number any more, designator and value plain texts (one undo step).
    bool dissolveComponent(int designator);
    // Commands of the reference's popup menu: a name for all selected elements (one undo step; "Benennen"), the
    // selection replaced by all elements on visible layers with exactly that name ("Namen markieren"; the name of the
    // element clicked, else of the first selected element that has one), the selection onto a layer (pads stay on
    // copper, SMD pads on the outer copper), the origin to the top left or bottom left corner of the working area or to
    // a point (on the grid counted from that corner, as the key 0 puts it). Extras: the elements lying wholly outside the
    // working area deleted (their number, one undo step).
    bool nameSelection(const QString &name);
    QString nameToSelect() const;
    bool selectByName(const QString &name);
    bool setSelectionLayer(int layer);
    bool setOrigin(QPointF at);
    int deleteOutside();
    // The component list ("Bauteilliste") and the selector at the right.
    QTreeWidget *componentList() const{return componentTree;}
    QTreeWidget *selector() const{return selectorTree;}
    // Plugins: external programs that get the selection (all elements when nothing is selected) as a Text-IO file
    // with the board's data as parameters, and answer with a Text-IO file and their exit code: 0 nothing, 1 replace the
    // elements, 2 add the new ones, 3 and 4 the same with the new elements on the pointer, 128 to 255 an error.
    struct Plugin {QString name,program;};
    QList<Plugin> plugins() const{return pluginList;}
    void setPlugins(const QList<Plugin> &list);     // kept in the preferences
    // Runs a plugin and applies its answer; returns its exit code, -1 when it could not run. `message` tells what
    // went wrong or what the plugin reported.
    int runPlugin(const QString &program,QString *message=nullptr);
    // A board made by the Gerber import, as a new board (one undo step) or, keeping the files' origin at the active
    // board's, its elements on the active board.
    void importBoard(const Board &imported,bool newBoard);
    // Applies a change to the elements at `indexes` as one undo step.
    void editElements(const QList<int> &indexes,const std::function<void(Element&)> &edit);

    // Actions for menus and toolbars: new, open, save, saveAs, print, exportImage, exportLay6, exportLay4, importMacro, saveMacro,
    // projectInfo, undo, redo, cut, copy, paste, duplicate, delete, selectAll, rotate, rotateBack, rotationAngle (a
    // menu), mirror, mirrorVertical, align (a menu of alignLeft, alignRight, alignTop, alignBottom, alignCentreX,
    // alignCentreY), alignGrid, arrange, templates, wizard, preferences, otherSide, group, ungroup, zoomBoard, zoomElements, zoomSelection,
    // zoomPrevious, zoomIn, zoomOut, overview (the overview below the tools shown), fromBelow, photo, drc,
    // compareSchematic, schematicAirwires, takeOverSchematic, assignPins, placeMissing, removeAirwires, resetSolderMask, specialShape, addBoard, removeBoard, boardProperties, exportGerber, exportDrill,
    // exportComponents, exportMilling, millingWide (milling paths as wide as the cutter), removeMilling, importGerber,
    // definePlugins, startPlugin.
    QAction *action(const QString &name) const{return actions.value(name);}
    // The actions of the reference's main toolbar, in order, with nullptr for separators.
    QList<QAction*> toolBarActions() const;
    std::function<void()> titleChanged;
private:
    Document doc;
    QString path,importedFile;     // the document's own file; the Sprint-Layout file it was opened from
    bool modified=false;
    QList<Document> past,future;
    QStringList notes;
    QFrame *noticeBar=nullptr;
    QLabel *noticeText=nullptr;
    BoardView *board=nullptr;
    BoardOverview *overview=nullptr;
    MacroPanel *macros=nullptr;
    QTabBar *tabs=nullptr;
    QLabel *coordinates=nullptr;
    QWidget *properties=nullptr;
    QFormLayout *propertyForm=nullptr;
    int multiKind=0;                                // the kind a multiple selection shows (ElementType)
    QListWidget *library=nullptr,*findingList=nullptr,*schematicList=nullptr;
    QTabWidget *sidePanel=nullptr;
    QWidget *schematicPage=nullptr;
    // The comparison again, when its tab or its airwires are shown.
    void refreshSchematic();
    // The schematic's components and nets for the active board (targets(), may throw): without the components another
    // board of the document has.
    documents::Targets schematicForBoard() const;
    QTreeWidget *componentTree=nullptr,*selectorTree=nullptr;
    QFrame *autorouteBar=nullptr,*photoBar=nullptr;
    QLabel *helpLine=nullptr;
    QTimer *helpTimer=nullptr;
    void refreshHelpLine();
    void refreshToolKeys();
    QComboBox *selectorType=nullptr,*selectorProperty=nullptr,*selectorLayers=nullptr;
    QLabel *findingSummary=nullptr;
    QButtonGroup *toolButtons=nullptr;
    QMap<int,QCheckBox*> layerVisible;
    QMap<int,QRadioButton*> layerActive;
    QCheckBox *groundBox=nullptr;
    QComboBox *gridBox=nullptr,*shapeBox=nullptr;
    QDoubleSpinBox *trackWidthBox=nullptr,*padBox=nullptr,*drillBox=nullptr,*smdWidthBox=nullptr,*smdHeightBox=nullptr,*clearanceBox=nullptr;
    QCheckBox *viaBox=nullptr,*filledBox=nullptr;
    QMap<QString,QDoubleSpinBox*> ruleBoxes;
    QMap<QString,QCheckBox*> ruleChecks;
    QMap<QString,QAction*> actions;
    bool refreshing=false;
    double rotation=90;
    PrintSettings printSettings;
    bool printSettingsSet=false;
    std::shared_ptr<QPrinter> printer;              // set up once ("Drucker einrichten"), used by every printout
    QPrinter *printerDevice();
    GerberSettings gerberSettings;
    DrillSettings drillSettings;
    ComponentDataSettings componentSettings;
    MillingSettings millingSettings;
    void millingDialog();
    QList<Plugin> pluginList;
    QList<double> gridValues=defaultGridKeys();
    QList<double> userGrids;
    void fillGridMenu(QMenu *menu);
    QList<double> favourites;
    QList<QPointF> padFavourites,smdFavourites;
    QToolButton *padFavouriteButton=nullptr,*smdFavouriteButton=nullptr;
    void showSizeFavourites(SizeKind kind);
    std::function<void()> padSettingsChanged,smdSettingsChanged;
    QStringList lastFolders{QString(),QString(),QString(),QString(),QString()};
    void usedFolder(Folder kind,const QString &folder);
    QTimer *autosaveTimer=nullptr;
    int revision=0,backupRevision=0;
    void applyColours();
    void savePreferences();
    QToolButton *favouriteButton=nullptr;
    void showFavourites();
    void gridKeysDialog();
    void pluginDialog();
    void startPluginDialog();
    void gerberDialog();
    void drillDialog();
    void holeListDialog();
    void componentDataDialog();
    QString outputName() const;
    void printPreview();
    void exportImageDialog();
    void templateDialog();
    void wizardDialog();
    // The preferences, at the page of that name ("folders", "backups") if given.
    void preferencesDialog(const QString &shown={});
    void keepReadable(Element &e) const;
    Board freshBoard(const QString &name) const;
    void createActions();
    QWidget *createToolPanel();
    QWidget *createLayerBar();
    QWidget *createAutorouteBar();
    QWidget *createPhotoBar();
    std::function<void()> autorouteGrid;
    QWidget *createSidePanel();
    void snapshot();
    // After a macro went down as a component: its component dialog.
    void macroPlaced(const QList<int> &added);
    void touched();
    void refresh();
    void refreshLayers();
    void refreshTabs();
    void refreshProperties();
    void refreshActions();
    void refreshComponents();
    void refreshSelector();
    void showSelectedComponent();
    // A new text may start a series: then `start` gets its first number (else -1) and `prefix` the text before it.
    bool editText(Element &text,bool isNew,QString *prefix=nullptr,int *start=nullptr);
    void editBoardProperties();
    void editProjectInfo();
    void specialShape();
    void arrangeDialog();
    void addCopies(int copies,const std::function<void(QList<Element>&,int)> &place);
    void importMacro();
    void saveMacro();
    void placeFootprint(int row);
};
// The module's library folders (docs/suite.md, "Bibliotheken"): its own macro folder (macroFolder in the module's
// preferences, else Dokumente/OpenLoch/Makros) and further folders whose macros are only read (macroFolders/extra).
LibraryFolders libraryFolders();
void setLibraryFolders(const LibraryFolders &folders);
}
