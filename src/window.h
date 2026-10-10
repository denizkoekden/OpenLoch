#pragma once
#include <QPointer>
#include "project.h"
#include "history.h"
#include "continuity.h"
#include "targetcheck.h"
#include "assistant.h"
#include "notes.h"
#include "hpgl.h"
#include "installation.h"
#include "openlibrary.h"
#include "icons.h"
#include <QMainWindow>
#include <QList>
#include <QUrl>
#include "documents/libraryfolders.h"
#include <functional>
class QTreeWidget;
class QListWidget;
class QToolBar;
class QLabel;
class QActionGroup;
class QComboBox;
class QTabBar;
class QFormLayout;
class QTimer;
class QAction;
class QDockWidget;
class QDialog;
class QFileSystemWatcher;
namespace openloch {
class Canvas;
class NotesPanel;
class Window : public QMainWindow {
public:
    explicit Window(const QString &assets={},QWidget *parent=nullptr);
    bool openPath(const QString &path);
    // AutoSpeichern now (normally the timer does it); false when nothing changed or the backup failed.
    bool saveBackup();
    // How a file goes to the program the system has for it ("Excel erzeugen"); tests put something quieter here.
    std::function<bool(const QUrl&)> openExternally;
    // Öffnen through a host such as the suite (docs/suite.md, "Öffnen in jedem Fenster"): when set, "Öffnen…" calls it
    // with the folder it would start in instead of showing its own dialog.
    std::function<bool(const QString &folder)> openHandler;
    // The library folders changed elsewhere (the suite's overview): the library is read anew.
    void librariesChanged();
    QString path() const{return currentPath;}
    // The title of the document (the active board's name) and a call after the window has set its title, so that a
    // host can show its own (as frontpanel::PanelEditor::titleChanged).
    QString title() const{return project.title;}
    // Renames the document (its active board) as one undo step, as Platine → Umbenennen does.
    void setTitle(const QString &title){if(title!=project.title)changeBoard(8,project.activeBoard,title);}
    std::function<void()> titleChanged;
    // Later file dialogs start in this file's folder.
    void rememberDirectory(const QString &file);
    // Datei → Neue Platine… or Neuer Schaltplan… ("board" or "schematic"): asks for the size, then replaces the document;
    // false when the dialog was cancelled or unsaved changes were kept.
    bool newProject(const QString &mode);
    // Ungespeicherte Sicherung wiederherstellen…: one of the backups of unsaved projects (written a few seconds after each
    // change) opens as the unsaved project it was.
    void restoreRecovery();
    // The folder of those backups. When the program starts, files there are left over from an earlier run.
    static QString recoveryFolder();
    // Saving through a host, such as a project of the suite, as with frontpanel::PanelEditor: when set, "Speichern"
    // and "Speichern unter" (also when closing) call it instead of writing a file, `asNew` for "Speichern unter", and
    // return what it returns. The host takes the document with documentData() whenever it likes and reports a
    // finished save with markSaved(). AutoSpeichern then leaves backups to the host.
    std::function<bool(bool asNew)> saveHandler;
    QJsonObject documentData();
    void markSaved();
    // Shows a document of a project, its data as documentData() gives them; the history starts anew. Throws
    // FormatError for data it cannot read.
    void setDocumentData(const QJsonObject &data);
    // The suite, for a board whose project holds a schematic: whether it does, and the schematic's target connections
    // (docs/file-formats.md, "Übernahme vom Schaltplan"). Unset outside the suite.
    std::function<bool()> hasSchematic;
    std::function<documents::Targets()> targets;
    // The suite: the documents of the window's project changed, so a schematic may have come.
    void projectChanged();
    Canvas *canvas;
protected:
    void closeEvent(QCloseEvent *) override;
private:
    Project project;
    QString currentPath,assets,lastDirectory;
    bool dirty=false;
    History history;
    QTreeWidget *library;
    QToolBar *drawingTools;
    QActionGroup *drawingGroup=nullptr;
    QLabel *coordinates,*info;
    QMap<QString,LibrarySource> catalog;
    QComboBox *libraryPages=nullptr;QTabBar *boardList=nullptr; // boards as tabs below the drawing area, like the original
    QList<QAction*> unitActions;
    QAction *potentialsAction=nullptr;
    QColor lastPotentialColour=QColor(255,0,0);
    // The drawing style of the Farben, Breite and Füllen toolbars (LochMaster's line and fill colour, line width in 1/100 mm
    // and "Flächen füllen"), given to new objects; a new board starts silver, 0.5 mm, filled with black.
    QColor penColour=QColor(192,192,192),brushColour=QColor(Qt::black);int penWidth=50;bool fillAreas=true;QString styledTool="select";
    void setDrawStyle(int mode,const QVariant &value);
    void showDrawStyle();
    void updateToolDefaults();
    QListWidget *libraryParts=nullptr,*shortList=nullptr;
    QDockWidget *shortsDock=nullptr;
    QList<Continuity::Short> shortFindings;
    // Comparison with the schematic: the findings in a dock, the airwires on the board.
    QDockWidget *schematicDock=nullptr;
    QListWidget *schematicList=nullptr;
    QAction *airwiresAction=nullptr;
    documents::Targets schematicTargets;
    TargetCheck schematicCheck;
    bool schematicAvailable() const;
    void compareWithSchematic(bool reveal);
    void takeOverFromSchematic();
    void assignPins();
    void placeMissingParts();
    QTimer *recoveryTimer=nullptr;
    QString recoveryFile;
    bool maybeSave();
    QString startDirectory() const;
    QString savePath(const QString &caption,const QString &title,const QString &filter);
    void saveFailed(const QString &path,const QString &reason={});
    bool save(bool as=false);
    void refresh();
    void keepMainView();
    void libraryPageProperties();
    void libraryPartProperties();
    void nestedPartProperties(const QString &kind,int index,const QString &path);
    void groupProperties();
    void autoSaveSettings();
    void showHelp();
    void restartAutoSave();
    QTimer *autoSaveTimer=nullptr;bool autoSaveEnabled=true;int autoSaveMinutes=10;
    std::function<void()> addExtraRows(QFormLayout *form,QDialog &dialog,QJsonArray &fields);
    void buildTools();
    void populateLibrary();
    void undo();
    void redo();
    void exportBom();
    void keepOpenedCopy(const QString &path);
    void showLibraryPage(int index);
    void refreshLibraryPreviews();
    bool libraryBitmaps=true;   // BMP-Rendering of the library previews as they were drawn
    QString libraryDirectory() const;
    bool editableLibraryPage();
    bool changeLibraryPage(const std::function<void(Project &)> &change,int selectRow=-1);
    void createLibraryPage();
    void deleteLibraryPage();
    void moveLibraryPart(int where);
    void deleteLibraryPart();
    void addSelectionToLibrary();
    // Bauteilordner (forum wish, like Sprint-Layout's macro folder): every part its own LIB file, every folder a page,
    // so parts can be copied, sorted and shared with the file manager. The panel follows changes made outside.
    QString componentFolder() const;
    void chooseComponentFolder();
    void saveSelectionAsPartFile();
    QFileSystemWatcher *folderWatcher=nullptr;QTimer *folderRefresh=nullptr;
    void placeComponent(const QString &path,int index);
    // LochMaster's "Objekt-Assistent"; like the original's dialog it keeps type and values between calls.
    void componentAssistant();
    QString assistantPictureFolder(int style) const;
    int assistantStyle=11;
    QList<AssistantParameter> assistantValues;
    void applyTemplate(const QString &path);
    void properties();
    void projectProperties();
    // "Layout bearbeiten": the board's copper layout in its own editor; "Layout übernehmen" makes it the board's layout.
    void editLayout();
    QMap<QString,QAction*> viewSwitches;
    int xrayFillLevel=73,xrayOutlineLevel=82;
    void applyStoredView();
    // Platine → Objektbaum anzeigen: the objects topmost first, groups with their contents, texts like the original's.
    QPointer<QDockWidget> treeDock;QPointer<QTreeWidget> objectTree;bool treeChoosing=false;
    void fillObjectTree();
    // Parts like the original's "Bauteil" menu: the dialog edits a template that carries over between uses.
    QJsonObject componentTemplate{{"id",""},{"value",""},{"description",""},{"label",""},{"listed",true},{"extra",QJsonArray{}}};
    bool componentDialog(QJsonObject &values,bool position,QPointF *centre=nullptr);
    bool editExtraFields(QJsonArray &fields,QWidget *parent);
    QStringList extraFieldNames() const;
    void defineComponent();
    void dissolveComponent();
    QString billOfMaterialsTable(QChar separator) const;
    void editNodes();
    void exportDocument(const QString &kind,bool selection=false);
    void changeBoard(int operation,int index=0,const QString &name={},const Project *other=nullptr);
    // Platine → Anmerkungen: the original's non-modal RTF editor; typed text reaches the project (and undo) after a
    // pause, when the editor loses focus or closes, and before saving or switching boards.
    QPointer<QDialog> notesWindow;QPointer<NotesPanel> notesPanel;QTimer *notesTimer=nullptr;QByteArray notesShown;bool notesPending=false,notesLoading=false;
    void showNotes();void loadNotesEditor();void flushNotes();
    // Platine → Stückliste → Erstellen…: the same editor, modal, with the generated parts list.
    void showPartsList();
    // Datei → HPGL-Bearbeitungsdateien…: the original's export dialog; its choices last for the session like there.
    PlotOptions hpglOptions;void exportHpgl();
    // The LochMaster installation whose libraries, templates and samples are shown (read only); chosen in
    // Bibliothek → LochMaster-Bibliotheken einbinden…, remembered, else searched in the usual places at start.
    LochMasterInstallation lochMaster;void chooseLochMaster();
    // OpenLoch's own library pages (libraries/*.json), generated once into a cache folder as LIB files.
    QStringList openLibraryFiles;void prepareOpenLibrary();void saveOpenLibrary();
    // Pages of OpenLoch's own library are generated when first shown (or saved), so that starting stays quick.
    QMap<QString,QJsonObject> openLibraryPages;bool ensureOpenLibraryPage(const QString &file);
    QString lochMasterSummary() const;
    QList<NoteLine> partsList(int mode) const;
    bool editPotential(QString &name,QColor &colour);
    void checkShorts(bool reveal);
    void saveRecovery();
};
// The perfboard's library folders (docs/suite.md, "Bibliotheken"): its own component folder (library/componentFolder,
// else Dokumente/OpenLoch/Bauteile) and, only read, the library folder of the LochMaster installation chosen in the
// window (Bibliothek → LochMaster-Bibliotheken einbinden…). Setting them changes the own folder.
LibraryFolders componentLibraryFolders();
void setComponentLibraryFolders(const LibraryFolders &folders);
}
