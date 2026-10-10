#pragma once
#include "model.h"
#include "documents/libraryfolders.h"
#include <QDialog>
#include <QWidget>
#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QMenu;
class QToolButton;
class QTreeWidget;

// The symbol library ("Bauteilbibliothek"): pages of symbols, each a component or a plain group ("Clip") in local
// coordinates around its insertion point. Pages come with the program (libraries/schematic, read-only) or lie in the
// user's library folder as files of their own (.olschlib, or pages of a sPlan library, .LIB). Names and captions may
// hold several languages like sPlan's: German, English and French, separated by line breaks (CR).
namespace openloch::schematic {
struct LibraryEntry {
    QString caption;                // shown under the symbol ("Unterschrift")
    Item symbol;
    QMap<QString,Resource> resources;   // the pictures the symbol and its children show
    // A parent's children as the reference keeps them with it (say the gates of a 7400): components at their places
    // relative to the parent's insertion point, placed together with it.
    QList<Item> children;
    bool operator==(const LibraryEntry &) const=default;
};
struct LibraryPage {
    QString name;                   // the page as listed
    QString folder;                 // categories above it, "/" between levels
    QString file;                   // where it was read from; empty for pages made in memory
    bool builtIn=false;
    bool readOnly=false;            // from a further library folder that is only read
    QList<LibraryEntry> entries;
    bool operator==(const LibraryPage &) const=default;
};
// The part of a name or caption in the interface language (the German one where there is no other).
QString localized(const QString &text);
// Own library format: JSON with format "OpenLoch Schematic Library", version 2. Throws FormatError.
QJsonObject pageToJson(const LibraryPage &page);
LibraryPage pageFromJson(const QJsonObject &json);
LibraryPage loadPage(const QString &file);
void savePage(const LibraryPage &page,const QString &file);
// The pages that come with OpenLoch, and the pages of `folder` and its subfolders, each sorted like sPlan's library:
// level by level, folders before pages, both by their names in the interface language.
void sortPages(QList<LibraryPage> &pages);
QList<LibraryPage> builtInPages();
QList<LibraryPage> folderPages(const QString &folder);
// The user's library folder ("schematic/libraryFolder" in the settings; Documents/OpenLoch/Schaltplan-Bibliothek).
QString libraryFolder();
QString standardLibraryFolder();
// The library folders for the suite (docs/suite.md, "Bibliotheken"): the own one above, where pages are written, and
// further ones that are only read ("schematic/extraLibraryFolders"). Setting them writes the settings; open editors
// read them again with Editor::librariesChanged().
openloch::LibraryFolders libraryFolders();
void setLibraryFolders(const openloch::LibraryFolders &folders);
// The folder of templates ("schematic/templateFolder"; Documents/OpenLoch/Schaltplan-Vorlagen).
QString templateFolder();
QString standardTemplateFolder();
// A symbol ready to be placed: new ids, and with automatic numbering the next free number for its designator in the
// document ("R?" becomes "R3" when R1 and R2 exist).
Item placedSymbol(const LibraryEntry &entry,const Document &document);
// What placing an entry puts down: the symbol as placedSymbol makes it and, for a parent with children kept in the
// library, the children at their places relative to it, with new ids and linked to it.
QList<Item> placedItems(const LibraryEntry &entry,const Document &document);
QString nextDesignator(const Document &document,const QString &designator);

// The library at the left of the editor, like sPlan's: above, the chosen page as a path of folders ("Elektro > Antennen")
// with a menu of the folder's level ("<---" one level up, the folders, then the pages) and a tree of all pages;
// the symbols of the page as pictures at one scale for the whole page; below, the page's file and the buttons for the
// columns, the captions, the previous and next page and the search. Symbols are dragged onto the sheet or clicked
// and then placed.
class LibraryPanel : public QWidget {
public:
    explicit LibraryPanel(QWidget *parent=nullptr);
    void setPages(const QList<LibraryPage> &pages);
    const QList<LibraryPage> &pages() const{return list;}
    int currentPage() const{return page;}
    void setCurrentPage(int index);
    // A page by its folder and name, to be remembered across the order of pages; showPage returns false if none fits.
    QString pageKey(int index) const;
    bool showPage(const QString &key);
    void setColumns(int columns);
    int columns() const{return columnCount;}
    void setCaptions(bool on);
    bool captionsShown() const{return captions;}
    // The entry for a drag key ("page/entry").
    bool entry(const QString &key,LibraryEntry *out) const;
    // The menu of a folder's level, as the page button shows it.
    QMenu *levelMenu(const QString &folder);
    // The pages as a tree instead of the symbols, as in sPlan with a filter field and "Bibliothek neu einlesen" below it.
    // A filter shows the pages whose name and those with entries whose caption contains it (in any language); such an
    // entry is listed under its page, and a click on it shows the page with the entry chosen.
    void showTree(bool on);
    // The entries whose caption (in any language) or designator contains `text`: page and entry.
    QList<std::pair<int,int>> find(const QString &text) const;
    std::function<void(const LibraryEntry&)> chosen;
    QListWidget *symbolList() const{return symbols;}
    // A parent with children kept in the library shows a small triangle at the bottom right of its cell, as in sPlan;
    // over the triangle its children appear beside the cell. The triangle of an entry of the current page (in the
    // list's viewport; empty without children), and the pointer at a place of the viewport.
    QRect childrenMark(int entry) const;
    void hoverAt(QPoint viewport);
    QLabel *childrenPopup() const{return popup;}
    QTreeWidget *pageTree() const{return tree;}

    // Own pages (.olschlib) and sPlan library pages (.lib) lie in `folder` and its subfolders and show under the top folder
    // `root`; only they can be changed. A sPlan page is written back into its file: unchanged entries keep their bytes. The functions work on the current page and save it at once; they return false with a message in `error`.
    void setOwnFolder(const QString &folder,const QString &root);
    bool writable(int index) const;
    bool newPage(const QString &name,QString *error=nullptr);
    // A subfolder beside the current own page (or at the top of the library folder) with a first, empty page in it.
    bool newFolder(const QString &name,QString *error=nullptr);
    // "Datensicherung": every file of a library folder (the own one unless `folder` is given) in a ZIP file, and back into
    // a folder. Restoring replaces files of the same name and keeps the others, or with `clearFirst` deletes the library
    // pages there first (.olschlib, .lib, also in subfolders); restored into the own folder, the library is read again.
    bool backup(const QString &zipFile,QString *error=nullptr,const QString &folder={}) const;
    bool restore(const QString &zipFile,QString *error=nullptr,const QString &folder={},bool clearFirst=false);
    // The files a backup of `folder` takes, the library pages restoring with `clearFirst` deletes there, and the files in
    // a backup (paths relative to the folder, sorted; for an unreadable backup none and `error` set).
    static QStringList backupFiles(const QString &folder);
    static QStringList libraryPageFiles(const QString &folder);
    static QStringList backupContents(const QString &zipFile,QString *error=nullptr);
    bool copyPage(const QString &name,QString *error=nullptr);
    bool renamePage(const QString &name,QString *error=nullptr);
    bool deletePage(QString *error=nullptr);
    bool emptyPage(QString *error=nullptr);
    bool addEntries(const QList<LibraryEntry> &entries,QString *error=nullptr);
    bool replaceEntry(int index,const LibraryEntry &entry,QString *error=nullptr);
    bool newEntry(QString *error=nullptr);
    bool duplicateEntry(int index,QString *error=nullptr);
    bool deleteEntry(int index,QString *error=nullptr);
    bool moveEntry(int from,int to,QString *error=nullptr);
    // The context menu over the symbols, for the entry under the pointer (or -1), as sPlan's.
    QMenu *contextMenu(int entry);
    // Asked of the editor: new sheets with the symbols of these pages, reading the library again.
    std::function<void(const QList<int>&)> copyToSheets;
    std::function<void()> reload;
    // Asked of the editor: the properties of entry `index` of the current page in its panel (without it, in a dialog).
    std::function<void(int)> entryPropertiesRequested;
    // "Eigenschaften": of one entry (in the panel, see above), or for all components of the page at once (a dialog whose
    // changed fields go to each of them).
    void editEntry(int index,bool all);
    // What writing a sPlan page could not keep (by default shown in a message).
    std::function<void(const QStringList&)> lossesReported;
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    QList<LibraryPage> list;
    int page=-1;
    int filledWidth=0;
    int columnCount=2;
    bool captions=true;
    QToolButton *pageButton=nullptr,*treeButton=nullptr;
    QListWidget *symbols=nullptr;
    QTreeWidget *tree=nullptr;
    QWidget *treeBox=nullptr;           // the tree with the filter row below it
    QLineEdit *filter=nullptr;
    QToolButton *reloadButton=nullptr;
    QLabel *folderLabel=nullptr;
    QMenu *menu=nullptr;
    QHash<QString,QPixmap> pictures;    // by page, entry, size and scale
    double pageScale=1;                 // pixels per millimetre of the page shown
    QLabel *popup=nullptr;              // the children of a parent while the pointer is over its triangle
    int popupEntry=-1;
    QString ownFolder,ownRoot;
    bool store(int index,QString *error);
    void refresh(const QString &key);
    bool readOnly(QString *error) const;
    void fill();
    void showPath();
    void fillTree();
    void openLevel(const QString &folder);
    void searchDialog();
};
// A picture of a symbol `size` pixels wide and high, fitted into it or at `pixelsPerMm` around the centre of its bounds.
QImage symbolPicture(const Item &symbol,int size,const QMap<QString,Resource> &resources={},double pixelsPerMm=0);
// "Datensicherung" as in sPlan: creating one shows the folder whose files go into it and those files; restoring shows the
// files of the backup, the folder they go to and "delete before restoring". "..." chooses another folder.
class LibraryBackupDialog : public QDialog {
public:
    LibraryBackupDialog(bool restoring,const QString &folder,const QStringList &files,QWidget *parent=nullptr);
    QString folder() const;
    bool clearFirst() const;
    QLineEdit *folderField=nullptr;
    QPlainTextEdit *fileList=nullptr;
    QCheckBox *clear=nullptr;
    QPushButton *chooseFolder=nullptr,*ok=nullptr;
private:
    bool restoring=false;
};
}
