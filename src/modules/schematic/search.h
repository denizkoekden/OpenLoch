#pragma once
#include "model.h"
#include <QWidget>
#include <functional>

class QCheckBox;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTreeWidget;
class QTreeWidgetItem;

// "Suchen / Ersetzen": components by designator and value, or texts, on the current sheet or all sheets; what is found
// is replaced in one element, on one sheet or on all sheets.
namespace openloch::schematic {
struct SearchOptions {
    bool components=true;           // components (designator, value), otherwise texts
    QString designator,value,text;  // what is looked for; empty fields of components match anything
    bool partsListOnly=false,caseSensitive=false,expand=true,allSheets=true;
    int currentSheet=0;
    QString fileName;               // for <FILENAME> when variables are expanded
};
struct SearchHit {
    int sheet=0;
    QString id;                     // the component or text
    QString shown;                  // what the list shows
};
QList<SearchHit> search(const Document &document,const SearchOptions &options);
// The field a replacement goes to: as sPlan's buttons, the designator or the value of components or the text of texts;
// or every field looked for.
enum class ReplaceField {Designator,Value,Text,All};
// Replaces what was looked for by `replacement` in the fields of the hits; returns how many fields changed.
int replace(Document &document,const QList<SearchHit> &hits,const SearchOptions &options,const QString &replacement,ReplaceField field=ReplaceField::All);

// The panel at the right of the editor: search, results by sheet, replace.
class SearchPanel : public QWidget {
public:
    explicit SearchPanel(QWidget *parent=nullptr);
    std::function<const Document*()> document;
    std::function<void(const QString&)> showElement;
    // Applies a change to the document as one undo step.
    std::function<void(const std::function<void(Document&)>&)> change;
    std::function<QString()> fileName;
    void refresh();
    SearchOptions options() const;
    QList<SearchHit> hits() const{return found;}
    QTreeWidget *results=nullptr;
    QRadioButton *componentsMode=nullptr,*textsMode=nullptr;
    // What is looked for and, beside it as in sPlan, what replaces it: designator and value of components, or texts.
    QLineEdit *designator=nullptr,*value=nullptr,*text=nullptr;
    QLineEdit *designatorReplacement=nullptr,*valueReplacement=nullptr,*replacement=nullptr;
    QCheckBox *partsListOnly=nullptr,*caseSensitive=nullptr,*expand=nullptr,*allSheets=nullptr;
    QPushButton *designatorReplace=nullptr,*valueReplace=nullptr,*replaceButton=nullptr;
    // Replaces one field with its replacement in what is chosen in the results: one element, a sheet, or all.
    void replaceChosen(ReplaceField field);
private:
    QList<SearchHit> found;
    QWidget *componentsBox=nullptr,*textsBox=nullptr;
    void chosenChanged();
};
}
