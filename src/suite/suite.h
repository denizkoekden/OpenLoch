#pragma once
#include "documents.h"
#include "documents/projectfile.h"
#include "panelboards.h"
#include "projectparts.h"
#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QJsonObject>
#include <optional>
#include <memory>
class QMainWindow;
class QMenu;
class QWidget;
namespace openloch::suite {
class StartScreen;
// OpenLoch as one program for the four kinds of document (docs/suite.md): the start screen and one window per
// document, each the editor of its module, Lochraster (openloch::Window), Leiterplatte (PcbWindow) and Frontplatte
// (frontpanel::PanelEditor). Every document belongs to a project (docs/file-formats.md): new documents and project
// files are saved as OpenLoch projects through the editors' save handlers; a single document opened from a file of an
// original stays "transparent", its module saving that file, until a second document joins. The suite routes files
// to their editor, keeps the recently used files and gives every window a Fenster menu.
//
// Closing the last document brings back the start screen; closing the start screen or Beenden ends the program.
class Suite : public QObject {
public:
    // `lochMasterAssets`: the LochMaster installation shown in Lochraster windows (--assets); empty to search for it.
    explicit Suite(const QString &lochMasterAssets={},QObject *parent=nullptr);
    ~Suite() override;
    // Opens a file and shows the window of its document: a project (.openloch, or a module's own .olfp) with its
    // active document, a file of an original as one document that its module saves back into that file. A file that
    // is open already comes to the front; an untitled, unchanged document of the same kind gives its window to an
    // original's file. Returns the window, or nullptr if the file could not be opened (the suite or editor says why).
    QMainWindow *open(const QString &path);
    // A new project with a new document of this kind, after the kind's short dialog unless `ask` is false; nullptr
    // when the dialog was cancelled or the kind has no editor yet.
    QMainWindow *create(Kind kind,bool ask=true);
    // A new document of this kind in the project of `window`, saved with the project at once. A project without a
    // file asks for one (or takes `projectFile`); a single document of an original becomes a project then, and its
    // file stays as it is. nullptr when cancelled or when the kind cannot share a project yet.
    QMainWindow *addToProject(QMainWindow *window,Kind kind,bool ask=true,const QString &projectFile={});
    // Opens a document of the project of `window` that has no window of its own yet.
    QMainWindow *openDocument(QMainWindow *window,const QString &document);
    // The project overview (Fenster → Projektübersicht…, docs/suite.md): the documents of the project of `window` and the
    // parts across them, with commands to open, add, rename and remove documents and to export the parts list.
    void showProjectOverview(QMainWindow *window);
    // Renames a document of the project of `window`: an editor with a title of its own (perfboard, front panel) takes
    // the name as an edit, the others keep it in the project. The project is saved at once. False without such a
    // document, for an empty name, or when saving failed or was cancelled.
    bool renameDocument(QMainWindow *window,const QString &document,const QString &name);
    // Removes a document from the project of `window`: its window closes without asking (its changes go with it), and
    // the project is saved at once. Never the last document, nor the one `window` shows. False when nothing was removed.
    bool removeDocument(QMainWindow *window,const QString &document);
    // The parts of the project of `window` across its documents, open ones in their current state.
    QList<ProjectPart> projectPartsOf(const QWidget *window) const;
    // Saves the project of `window` with the state of all its open documents (documents without window as stored);
    // as new, or without file yet, it asks for one (or takes `projectFile`). False when cancelled or failed.
    bool saveProject(QMainWindow *window,bool asNew=false,const QString &projectFile={});
    // The open dialog for every window (docs/suite.md, "Öffnen in jedem Fenster"): the kind of the asking window first in
    // the filter, starting in openStartFolder(folder); the chosen file opens as open() does, its window comes back.
    QMainWindow *openDialog(QWidget *parent=nullptr,std::optional<Kind> kind=std::nullopt,const QString &folder={});
    // The folder the open dialog starts in: `given` (the module's own folder) when it exists, else the one used last,
    // else Dokumente/OpenLoch, else the user's folder; in tests only `given` or the user's folder.
    static QString openStartFolder(const QString &given);
    // The library folders of all kinds in one overview (Fenster → Bibliotheken…).
    void showLibraries(QMainWindow *window);
    void showStartScreen();
    StartScreen *startScreen() const;
    // At the start of the program: backups of unsaved Lochraster projects left over from an earlier run (a crash or a
    // forced end) are offered in a Lochraster window, as the program did before it had the start screen.
    void offerRecovery();
    // The open document windows, oldest first.
    QList<QMainWindow*> windows() const;
    static std::optional<Kind> kindOf(const QWidget *window);
    // Whether documents of this kind can share a project yet: their editor saves through the suite.
    static bool projectCapable(Kind kind);
    // Whether the window's document is saved through its project rather than into a file of its own.
    bool inProject(const QWidget *window) const;
    // The project of a window, and the file it stands for: its project's file, or the file its module saves.
    const documents::ProjectFile *projectFile(const QWidget *window) const;
    QString documentPath(const QWidget *window) const;
    // The document's name for lists: its own title within a project, else its file name or new title.
    QString documentName(const QWidget *window) const;
    // Recently used files, newest first (at most ten).
    QStringList recentDocuments() const;
    void remember(const QString &path);
    void forget(const QString &path);
    void clearRecent();
    // Closes the documents, newest first, each asking about unsaved changes, then the start screen. Stops and returns
    // false when a document stays open.
    bool closeAll();
    // Beenden: closes everything and ends the program; nothing happens when a document stays open.
    void quit();
    // Set while the system ends the program (macOS: Quit in the program menu), so that closing the last document
    // does not bring back the start screen.
    void setQuitting(bool on){quitting=on;}
private:
    // An open project and the windows of its documents.
    struct OpenProject {
        QString path,source;                           // the project's file (empty until saved); the file it came from
        documents::ProjectFile file;                   // as stored; open windows give their documents when saving
        QMap<QString,QPointer<QMainWindow>> windows;   // document id → window
        bool transparent=false;                        // one document whose module saves its own file
    };
    QString assets;
    QList<QPointer<QMainWindow>> documents;
    QList<std::shared_ptr<OpenProject>> projects;
    QPointer<StartScreen> start;
    bool quitting=false,finished=false;
    OpenProject *projectOf(const QWidget *window) const;
    void newProject(QMainWindow *window,Kind kind,bool transparent);
    void bind(QMainWindow *window,OpenProject *project,const QString &document);
    // The project's documents changed: its boards learn whether a schematic is there now.
    void notifyBoards(OpenProject *project);
    void release(QMainWindow *window);
    void updateTitle(QMainWindow *window);
    // The name of a window's document: its own title, or (circuit board, schematic) the name its project gives it.
    QString nameOf(const QWidget *window) const;
    QMainWindow *makeWindow(Kind kind);
    QMainWindow *newDocumentWindow(Kind kind,bool ask);
    QMainWindow *openProjectFile(const QString &path);
    QMainWindow *openDocumentWindow(OpenProject *project,const QString &document);
    QMainWindow *windowFor(const QString &path) const;
    QMainWindow *reusableWindow(Kind kind) const;
    bool openIn(QMainWindow *window,Kind kind,const QString &path);
    void present(QMainWindow *window);
    void attach(QMainWindow *window,Kind kind);
    void fillWindowMenu(QMenu *menu,QMainWindow *window);
    // The first schematic of a window's project: its data from its window when that is open (the current state), else
    // as stored; with `data` false only whether there is one. nullopt without a schematic.
    std::optional<QJsonObject> schematicData(const QWidget *window,bool data) const;
    // The boards of a window's project for its front panel: every board of the perfboard and circuit board documents,
    // from their windows when open (the current state), else as stored, where an older document gets its identifiers
    // once. A document that cannot be read offers none.
    QList<frontpanel::BoardSource> boardSources(const QWidget *window);
    void documentClosed(QObject *gone);
    void startScreenClosed();
};
// Draws a document of any kind into a picture without a window (the command line's --render); for a project, its
// active document.
bool renderDocument(const QString &path,const QString &picture,QString *error=nullptr);
}
