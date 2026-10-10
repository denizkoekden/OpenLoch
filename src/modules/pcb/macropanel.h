#pragma once
#include "model.h"
#include <QWidget>
#include <functional>

class QCheckBox;
class QComboBox;
class QFileSystemModel;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QTreeView;

namespace openloch::pcb {
// The macro library at the right, as the reference offers it for its own folders: the macro folder chosen in the
// preferences with its subfolders and Sprint-Layout macros (.lmk), a preview of the macro picked, its side (from the
// top or, mirrored with the layers swapped, from the bottom), through-plated pads, turns by 90° clockwise and "as a
// component". A double click, the place button or dragging the preview onto the board puts the macro there. OpenLoch
// ships no macros; the folder is the user's.
class MacroPreview;
class MacroPanel : public QWidget {
public:
    explicit MacroPanel(QWidget *parent=nullptr);
    // The folder shown (empty: none chosen yet).
    void setFolder(const QString &folder);
    QString folder() const{return root;}
    // The own folder and further folders whose macros are only read (the suite's library folders): a list above the
    // tree picks the folder shown; macros are saved into the own folder and removed only there.
    void setFolders(const QString &own,const QStringList &extra);
    QString ownFolder() const{return own;}
    QStringList extraFolders() const{return extras;}
    bool readOnly() const{return !extras.isEmpty()&&root!=own;}
    // The macro picked: its file, and its elements as they would be placed (side, vias and turns applied, centred on
    // the origin). Empty when the file cannot be read; `problem` tells why.
    bool pick(const QString &file);
    QString picked() const{return file;}
    QList<Element> macro() const;
    QString problem() const{return trouble;}
    // The options of the reference's panel.
    bool bottom=false,vias=false,asComponent=false;
    int turns=0;                // quarter turns clockwise
    void setOptions(bool bottomSide,bool throughPlated,int quarterTurns,bool component);
    // The folder the next macro is saved into: that of the macro picked or the one selected, else the root.
    QString currentFolder() const;
    // Removes a macro file (the panel's delete button asks first); true if it went.
    bool removeMacro(const QString &file);
    // The two lines describing the macro picked, as the fields above the preview show and change them (written into the
    // macro file; not for macros older than Sprint-Layout 4.0 or in folders that are only read).
    QStringList description() const{return lines;}
    bool setDescription(const QString &first,const QString &second);
    // What the editor does: place the elements (on the pointer), save the selection as a macro, choose the folder.
    std::function<void(const QList<Element>&)> place;
    std::function<void()> saveSelection,chooseFolder;
    // The format of a macro dragged from the preview onto the board.
    static QString dragFormat();
private:
    QString root,file,trouble,own;
    QStringList lines;
    QLineEdit *lineEdits[2]{};
    QStringList extras;
    QComboBox *libraryBox=nullptr;
    void showFolder(const QString &folder);
    QList<Element> elements;    // as read
    QFileSystemModel *model=nullptr;
    QTreeView *tree=nullptr;
    QLabel *folderLabel=nullptr,*nameLabel=nullptr;
    MacroPreview *preview=nullptr;
    QToolButton *sideButton=nullptr,*viaButton=nullptr;
    QCheckBox *componentBox=nullptr;
    QPushButton *placeButton=nullptr,*removeButton=nullptr;
    void refresh();
};
}
