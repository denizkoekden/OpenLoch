#pragma once
#include <optional>
#include "model.h"
#include <QSet>
#include <QWidget>
#include <functional>

class QFormLayout;
class QScrollArea;
class QVBoxLayout;

// The properties panel at the right ("Eigenschaften"), as in the reference: sections for what is selected, the
// sheet and its title block when nothing is. Every change is one undo step.
class QMenu;

namespace openloch::schematic {
class SheetView;
class PropertiesPanel : public QWidget {
public:
    explicit PropertiesPanel(QWidget *parent=nullptr);
    void setView(SheetView *view){sheetView=view;}
    // Shows the properties of the view's selection (or of the sheet).
    void refresh();
    // Applies a change to the document as one undo step.
    std::function<void(const std::function<void(Document&)>&)> change;
    std::function<void()> componentEditorRequested,titleBlockRequested,textDialogRequested;
    // The extended text input for a text of the selection (designator, value), in the font `family`; false when cancelled.
    std::function<bool(QString &text,const QString &family)> extendedText;
    // Go to a component or text (on whatever sheet); link the selected components to a parent.
    std::function<void(const QString&)> showElement;
    std::function<void()> linkParentRequested;
    // Text links: choose the target of the selected text.
    std::function<void()> linkTargetRequested;
    std::function<void()> bitmapExplorerRequested;
    std::function<void(const QString&)> childListRequested;   // the parent's id
    // Called after a preset of the view changed (to keep it).
    std::function<void()> presetChanged;
    // The preset of the view's drawing mode, shown while nothing is selected ("Voreinstellungen"); none for Select.
    Item *preset() const;
    // Named outlines kept in the settings ("Voreinstellungen" of the outline section): their names, the outline of the
    // selection kept under a name (false when the selected outlines differ), one given to the selection, one removed.
    static QStringList linePresets();
    bool addLinePreset(const QString &name);
    void applyLinePreset(const QString &name);
    static void removeLinePreset(const QString &name);
    // A symbol of the library shown instead of the selection: "Unterschrift in der Bibliothek" and the component's
    // fields. Each change goes to `libraryEntryChanged`, which keeps it and may give back the symbol as stored; pages that
    // cannot be changed show it without fields to edit. It ends with clearLibraryEntry() (the editor: a new selection).
    void showLibraryEntry(const Item &symbol,const QString &caption,bool writable);
    // A preset chosen from "Voreinstellungen" (its kind as `title`), shown until cleared with nullptr; `show` refreshes.
    void showPreset(Item *preset,const QString &title,bool show=true);
    void clearLibraryEntry();
    bool showsLibraryEntry() const{return libraryItem.has_value();}
    std::function<void(Item&,QString&)> libraryEntryChanged;
private:
    Item *presetItem=nullptr;       // while the panel shows a preset or a symbol of the library
    std::optional<Item> libraryItem;
    QString libraryCaption;
    Item *chosenPreset=nullptr;
    QString chosenPresetTitle;
    bool libraryWritable=false;
    SheetView *sheetView=nullptr;
    QScrollArea *scroll=nullptr;
    QWidget *content=nullptr;
    QVBoxLayout *sections=nullptr;
    bool refreshing=false;
    bool positionAsGroup=false;     // "Als Gruppe behandeln" as last chosen (off at first, as in the reference)
    // The list of element kinds of a mixed selection (groups taken apart), as the reference's: the kinds chosen there
    // are the ones shown and changed; none chosen, all. It belongs to the selection it was chosen for.
    QSet<int> typeFilter;
    QStringList filterFor;
    bool filtered() const;
    QList<Item*> resolved() const;  // the selection with its groups taken apart
public:
    // The menu "Ausrichten" in the section Position opens (the toolbar's).
    QMenu *alignMenu=nullptr;
private:
    QFormLayout *section(const QString &title);
    // Applies `edit` to each selected element (or, in the component editor, to each selected part).
    void edit(const std::function<void(Item&)> &edit);
    // The same for changes that also change the document (pictures).
    void edit(const std::function<void(Document&,Item&)> &edit);
    void applyPen(const Pen &pen);
    QList<Item*> selection() const;
    QStringList editedIds() const;
    void sheetSections();
    void typeList();
    void itemSections(const QList<Item*> &items);
};
}
