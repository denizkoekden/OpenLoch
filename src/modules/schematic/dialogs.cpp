#include "dialogs.h"
#include "library.h"
#include "example.h"
#include "language.h"
#include "images.h"
#include <QAbstractTableModel>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDate>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRadioButton>
#include <QRawFont>
#include <QSaveFile>
#include <QScrollBar>
#include <QSet>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextDocument>
#include <QTime>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace openloch::schematic {
namespace {
QDialogButtonBox *okCancel(QDialog *dialog){
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(ui("&OK"));buttons->button(QDialogButtonBox::Cancel)->setText(ui("&Abbrechen"));
    QObject::connect(buttons,&QDialogButtonBox::accepted,dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    return buttons;
}
}

SheetInsertDialog::SheetInsertDialog(const QString &title,const QStringList &sheets,int current,QWidget *parent):QDialog(parent),sheetCount(int(sheets.size())){
    setWindowTitle(title);auto *layout=new QVBoxLayout(this);
    auto *where=new QGroupBox(ui("Position"));auto *wl=new QFormLayout(where);
    sheetBox=new QComboBox;sheetBox->setObjectName("sheet");wl->addRow(ui("Blatt:"),sheetBox);
    before=new QRadioButton(ui("&Vor"));after=new QRadioButton(ui("&Nach"));first=new QRadioButton(ui("&Erstes Blatt"));last=new QRadioButton(ui("&Letztes Blatt"));
    before->setObjectName("before");after->setObjectName("after");first->setObjectName("first");last->setObjectName("last");after->setChecked(true);
    for(auto *b:{before,after,first,last})wl->addRow(QString(),b);
    layout->addWidget(where);
    auto *count=new QGroupBox(ui("Anzahl:"));auto *cl=new QHBoxLayout(count);countBox=new QSpinBox;countBox->setRange(1,100);countBox->setValue(1);countBox->setObjectName("count");cl->addWidget(countBox);cl->addStretch();
    layout->addWidget(count);layout->addWidget(okCancel(this));
    sheetBox->addItems(sheets);sheetBox->setCurrentIndex(current);
}
int SheetInsertDialog::position() const{
    if(first->isChecked())return 0;
    if(last->isChecked())return sheetCount;
    return sheetBox->currentIndex()+(after->isChecked()?1:0);
}
int SheetInsertDialog::count() const{return countBox->value();}

SheetSortDialog::SheetSortDialog(const QStringList &sheets,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Blätter sortieren"));auto *layout=new QHBoxLayout(this);
    list=new QListWidget;list->setObjectName("sheetOrder");list->setDragDropMode(QAbstractItemView::InternalMove);
    for(int i=0;i<sheets.size();i++){auto *item=new QListWidgetItem(sheets[i]);item->setData(Qt::UserRole,i);list->addItem(item);}
    list->setCurrentRow(0);layout->addWidget(list,1);
    auto *side=new QVBoxLayout;layout->addLayout(side);
    auto shift=[this](int by){const int r=list->currentRow(),to=r+by;if(r<0||to<0||to>=list->count())return;auto *item=list->takeItem(r);list->insertItem(to,item);list->setCurrentRow(to);};
    auto *up=new QPushButton(ui("Nach oben schieben"));up->setObjectName("up");side->addWidget(up);connect(up,&QPushButton::clicked,this,[shift]{shift(-1);});
    auto *down=new QPushButton(ui("Nach unten schieben"));down->setObjectName("down");side->addWidget(down);connect(down,&QPushButton::clicked,this,[shift]{shift(1);});
    side->addStretch();side->addWidget(okCancel(this));
}
QList<int> SheetSortDialog::order() const{QList<int> out;for(int i=0;i<list->count();i++)out<<list->item(i)->data(Qt::UserRole).toInt();return out;}

QMenu *TextDialog::fixedVariables(QWidget *parent){
    auto *menu=new QMenu(parent);menu->setObjectName("fixedVariables");
    auto add=[](QMenu *m,const QStringList &names){for(const auto &n:names){if(n.isEmpty()){m->addSeparator();continue;}m->addAction(u'<'+n+u'>')->setData(u'<'+n+u'>');}};
    // A submenu with the forms _1 to _9 of a variable.
    auto numbered=[&](QMenu *m,const QString &name){auto *sub=m->addMenu(u'<'+name+QStringLiteral("_…>"));for(int i=1;i<=9;i++)add(sub,{QStringLiteral("%1_%2").arg(name).arg(i)});};
    add(menu,{"PAGENO","PAGECOUNT","PAGENAME","PAGESCALE","PREVIOUS_PAGENO","NEXT_PAGENO","","FILENAME","FILENAME_PURE","FILEPATH","FILEDATE","FILETIME","",
              "VERSION","DATE","TIME","","COLNUM","COLCHAR","ROWNUM","ROWCHAR",""});
    add(menu->addMenu(ui("Bauteil")),{"BEZ","WERT","Z1","Z2","Z3","Z4"});
    menu->addSeparator();
    auto *family=menu->addMenu(ui("Parent/Child"));
    add(family,{"PARENT_ID","PARENT_VALUE","PARENT_ID_NUMBER","PARENT_PAGENO","PARENT_PAGENAME","PARENT_Z1","PARENT_Z2","PARENT_Z3","PARENT_Z4"});
    numbered(family,"PARENT_CONTACT");
    add(family,{"PARENT_COLNUM","PARENT_COLCHAR","PARENT_ROWNUM","PARENT_ROWCHAR","","CHILDNO","CHILDCHAR",""});
    for(const char *n:{"CHILD_PAGENO","CHILD_PAGENAME","CHILD_COLNUM","CHILD_COLCHAR","CHILD_ROWNUM","CHILD_ROWCHAR"})numbered(family,n);
    menu->addSeparator();
    add(menu->addMenu(ui("Text-Verlinkungen")),{"LINK_PAGENO","LINK_PAGENAME","LINK_TEXT","LINK_COLNUM","LINK_COLCHAR","LINK_ROWNUM","LINK_ROWCHAR","",
                                                "LINKFROM_PAGENO","LINKFROM_PAGENAME","LINKFROM_TEXT","LINKFROM_COLNUM","LINKFROM_COLCHAR","LINKFROM_ROWNUM","LINKFROM_ROWCHAR"});
    return menu;
}
TextDialog::TextDialog(const QString &text,const QStringList &variables,QWidget *parent,const QStringList &constants,const QString &family)
    :QDialog(parent),userNames(variables),constantTexts(constants){
    setWindowTitle(ui("Erweiterte Texteingabe"));auto *layout=new QVBoxLayout(this);
    auto *group=new QGroupBox(ui("Text"));auto *grid=new QGridLayout(group);layout->addWidget(group,1);
    edit=new QPlainTextEdit(text);edit->setObjectName("text");edit->moveCursor(QTextCursor::End);grid->addWidget(edit,0,0,1,2);
    special=new QPushButton(QStringLiteral("Ω"));special->setObjectName("specialCharacter");special->setToolTip(ui("Sonderzeichen"));special->setFixedWidth(28);
    grid->addWidget(special,0,2,Qt::AlignTop);
    fixed=new QPushButton(ui("Feste Variable einfügen"));fixed->setObjectName("fixedVariable");grid->addWidget(fixed,1,0);
    user=new QPushButton(ui("Anwender-Variable einfügen"));user->setObjectName("userVariable");grid->addWidget(user,2,0);
    defineUser=new QPushButton(ui("Definieren..."));defineUser->setObjectName("defineVariables");grid->addWidget(defineUser,2,1);
    constant=new QPushButton(ui("Textkonstante einfügen"));constant->setObjectName("constant");grid->addWidget(constant,3,0);
    defineConstant=new QPushButton(ui("Definieren..."));defineConstant->setObjectName("defineConstants");grid->addWidget(defineConstant,3,1);
    grid->setColumnStretch(0,1);
    auto *menu=fixedVariables(this);fixed->setMenu(menu);
    for(auto *a:menu->findChildren<QAction*>())if(a->data().isValid())connect(a,&QAction::triggered,this,[this,a]{put(a->data().toString());});
    user->setMenu(new QMenu(user));constant->setMenu(new QMenu(constant));
    connect(defineUser,&QPushButton::clicked,this,[this]{if(defineVariables){userNames=defineVariables();refresh();}});
    connect(defineConstant,&QPushButton::clicked,this,[this]{if(defineConstants){constantTexts=defineConstants();refresh();}});
    auto font=[family]{return family;};
    connect(special,&QPushButton::clicked,this,[this,font]{put(CharacterDialog::ask(font(),this));});
    offerSpecialCharacters(edit,font);
    refresh();
    layout->addWidget(okCancel(this));resize(440,280);
}
void TextDialog::put(const QString &t){if(t.isEmpty())return;edit->insertPlainText(t);edit->setFocus();}
void TextDialog::refresh(){
    // The menus of the user variables and the text constants, built anew after "Definieren…".
    auto fill=[this](QMenu *menu,const QStringList &captions,const QStringList &texts){
        menu->clear();
        for(int i=0;i<captions.size();i++){auto *a=menu->addAction(QString(captions[i]).replace(u'&',QStringLiteral("&&")));const QString t=texts[i];connect(a,&QAction::triggered,this,[this,t]{put(t);});}
    };
    QStringList names;for(const auto &n:userNames)names<<u'<'+n+u'>';
    fill(user->menu(),names,names);fill(constant->menu(),constantTexts,constantTexts);
    user->setEnabled(!userNames.isEmpty());constant->setEnabled(!constantTexts.isEmpty());
    constant->setToolTip(constantTexts.isEmpty()?ui("Es sind keine Textkonstanten definiert."):QString());
}
QString TextDialog::text() const{return edit->toPlainText();}

VariablesDialog::VariablesDialog(const QList<Variable> &variables,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Anwender-Variablen"));auto *layout=new QVBoxLayout(this);
    layout->addWidget(new QLabel(ui("In Texten als <Name> verwendbar, gespeichert mit der Zeichnung.")));
    table=new QTableWidget(int(variables.size())+1,2);table->setObjectName("variables");table->setHorizontalHeaderLabels({ui("Name"),ui("Wert")});
    table->horizontalHeader()->setStretchLastSection(true);table->verticalHeader()->hide();
    for(int r=0;r<variables.size();r++){table->setItem(r,0,new QTableWidgetItem(variables[r].name));table->setItem(r,1,new QTableWidgetItem(variables[r].value));}
    connect(table,&QTableWidget::cellChanged,this,[this](int row){if(row==table->rowCount()-1&&table->item(row,0)&&!table->item(row,0)->text().isEmpty())table->insertRow(table->rowCount());});
    layout->addWidget(table,1);
    auto *row=new QHBoxLayout;layout->addLayout(row);
    auto *add=new QPushButton(ui("Neue Zeile"));add->setObjectName("newRow");row->addWidget(add);
    auto *remove=new QPushButton(ui("Zeile löschen"));remove->setObjectName("deleteRow");row->addWidget(remove);row->addStretch();
    connect(add,&QPushButton::clicked,this,[this]{
        int r=table->rowCount()-1;   // the empty last row, else a new one
        if(r<0||(table->item(r,0)&&!table->item(r,0)->text().isEmpty())){r=table->rowCount();table->insertRow(r);}
        if(!table->item(r,0))table->setItem(r,0,new QTableWidgetItem);
        table->setCurrentCell(r,0);table->editItem(table->item(r,0));});
    connect(remove,&QPushButton::clicked,this,[this]{
        const int r=table->currentRow();if(r<0)return;
        table->removeRow(r);if(table->rowCount()==0)table->insertRow(0);});
    layout->addWidget(okCancel(this));resize(420,340);
}
QList<Variable> VariablesDialog::variables() const{
    QList<Variable> out;
    for(int r=0;r<table->rowCount();r++){
        const QString name=table->item(r,0)?table->item(r,0)->text().trimmed():QString();if(name.isEmpty())continue;
        out.append({name,table->item(r,1)?table->item(r,1)->text():QString()});
    }
    return out;
}
void VariablesDialog::accept(){
    // Names are compared without regard to case, as texts use them.
    const auto list=variables();
    for(int i=0;i<list.size();i++)for(int k=0;k<i;k++)if(list[i].name.compare(list[k].name,Qt::CaseInsensitive)==0){
        QMessageBox::warning(this,ui("Anwender-Variablen"),ui("Der Name „%1“ kommt mehrfach vor (Groß- und Kleinschreibung zählen dabei nicht). Bitte eine der Variablen löschen oder umbenennen.").arg(list[i].name));
        return;
    }
    QDialog::accept();
}

namespace {
// The characters of a font twenty in a row.
class CharacterModel : public QAbstractTableModel {
public:
    CharacterModel(const QList<char32_t> &codes,const QFont &font,QObject *parent):QAbstractTableModel(parent),codes(codes),font(font){}
    static constexpr int columns=20;
    int rowCount(const QModelIndex &parent) const override{return parent.isValid()?0:int((codes.size()+columns-1)/columns);}
    int columnCount(const QModelIndex &parent) const override{return parent.isValid()?0:columns;}
    QVariant data(const QModelIndex &index,int role) const override{
        const qsizetype i=qsizetype(index.row())*columns+index.column();if(i>=codes.size())return {};
        if(role==Qt::DisplayRole)return QString::fromUcs4(&codes[i],1);
        if(role==Qt::FontRole)return font;
        if(role==Qt::TextAlignmentRole)return Qt::AlignCenter;
        if(role==Qt::ToolTipRole)return QStringLiteral("U+%1").arg(uint(codes[i]),4,16,QLatin1Char('0')).toUpper();
        if(role==Qt::UserRole)return uint(codes[i]);
        return {};
    }
    Qt::ItemFlags flags(const QModelIndex &index) const override{
        return qsizetype(index.row())*columns+index.column()<codes.size()?Qt::ItemIsEnabled|Qt::ItemIsSelectable:Qt::NoItemFlags;}
private:
    QList<char32_t> codes;
    QFont font;
};
}
QList<char32_t> &CharacterDialog::recent(){static QList<char32_t> list;return list;}
CharacterDialog::CharacterDialog(const QString &family,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Sonderzeichen"));auto *layout=new QVBoxLayout(this);
    QFont font(family.isEmpty()?QStringLiteral("Arial"):family);font.setPointSize(12);
    // The characters the font itself has (no fallback), without control characters and surrogates.
    const QRawFont raw=QRawFont::fromFont(font);const QFontMetrics metrics(font);
    for(char32_t c=0x20;c<=0xFFFF;c++){
        if((c>=0x7F&&c<0xA0)||(c>=0xD800&&c<0xE000))continue;
        if(raw.isValid()?raw.supportsCharacter(c):metrics.inFontUcs4(c))codes<<c;
    }
    grid=new QTableView;grid->setObjectName("characters");grid->setModel(new CharacterModel(codes,font,grid));
    grid->horizontalHeader()->hide();grid->verticalHeader()->hide();
    grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);grid->horizontalHeader()->setDefaultSectionSize(26);
    grid->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);grid->verticalHeader()->setDefaultSectionSize(26);
    grid->setSelectionMode(QAbstractItemView::SingleSelection);grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
    grid->setMinimumSize(20*26+grid->verticalScrollBar()->sizeHint().width()+4,10*26);
    layout->addWidget(grid,1);
    auto *jumps=new QHBoxLayout;layout->addLayout(jumps);jumps->addStretch();
    auto jump=[&](const QString &caption,const char *name,QList<char32_t> targets){
        auto *b=new QPushButton(caption);b->setObjectName(name);jumps->addWidget(b);
        connect(b,&QPushButton::clicked,this,[this,targets]{for(char32_t t:targets)if(jumpTo(t))return;QApplication::beep();});};
    jump(ui("Indizes"),"subscripts",{0x2080,0x2070});jump(ui("Griechisch"),"greek",{0x391});jump(ui("Mathematisch"),"mathematical",{0x2202});
    layout->addWidget(new QLabel(ui("Zuletzt benutzte Zeichen:")));
    last=new QTableWidget(1,20);last->setObjectName("recent");last->horizontalHeader()->hide();last->verticalHeader()->hide();
    last->horizontalHeader()->setDefaultSectionSize(26);last->verticalHeader()->setDefaultSectionSize(26);
    last->setEditTriggers(QAbstractItemView::NoEditTriggers);last->setSelectionMode(QAbstractItemView::SingleSelection);
    last->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);last->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    last->setFixedHeight(26+2*last->frameWidth());
    QFont shown(QStringLiteral("Arial"));shown.setPointSize(12);last->setFont(shown);
    layout->addWidget(last);showRecent();
    connect(grid,&QTableView::doubleClicked,this,[this](const QModelIndex &i){if(i.data(Qt::UserRole).isValid())take(i.data(Qt::UserRole).toUInt());});
    auto codeAt=[this](int column)->char32_t{const auto *item=last->item(0,column);if(!item||item->text().isEmpty())return 0;return char32_t(item->text().toUcs4().value(0));};
    connect(last,&QTableWidget::cellClicked,this,[this,codeAt](int,int column){if(const char32_t c=codeAt(column))jumpTo(c);});
    connect(last,&QTableWidget::cellDoubleClicked,this,[this,codeAt](int,int column){if(const char32_t c=codeAt(column))take(c);});
    resize(sizeHint());
}
void CharacterDialog::showRecent(){
    const auto &list=recent();
    for(int i=0;i<20;i++){auto *item=new QTableWidgetItem(i<list.size()?QString::fromUcs4(&list[i],1):QString());item->setTextAlignment(Qt::AlignCenter);last->setItem(0,i,item);}
}
bool CharacterDialog::jumpTo(char32_t code){
    const qsizetype i=codes.indexOf(code);if(i<0)return false;
    const QModelIndex at=grid->model()->index(int(i/CharacterModel::columns),int(i%CharacterModel::columns));
    grid->setCurrentIndex(at);grid->scrollTo(at,QAbstractItemView::PositionAtCenter);return true;
}
bool CharacterDialog::take(char32_t code){
    if(!codes.contains(code)){QMessageBox::information(this,ui("Sonderzeichen"),ui("Die Schrift enthält dieses Zeichen nicht."));return false;}
    taken=QString::fromUcs4(&code,1);
    // Newest first, each once, at most twenty.
    auto &list=recent();list.removeAll(code);list.prepend(code);if(list.size()>20)list.resize(20);
    QDialog::accept();return true;
}
QString CharacterDialog::ask(const QString &family,QWidget *parent){
    CharacterDialog dialog(family,parent);
    if(shown)shown(&dialog);
    return dialog.exec()==QDialog::Accepted?dialog.chosen():QString();
}
namespace {
// Strg+Einfg in a text field: the special characters.
class SpecialCharacterKey : public QObject {
public:
    SpecialCharacterKey(QWidget *field,std::function<QString()> family):QObject(field),family(std::move(family)){field->installEventFilter(this);}
    bool eventFilter(QObject *object,QEvent *event) override{
        if(event->type()!=QEvent::ShortcutOverride&&event->type()!=QEvent::KeyPress)return false;
        const auto *key=static_cast<QKeyEvent*>(event);
        if(key->key()!=Qt::Key_Insert||!(key->modifiers()&Qt::ControlModifier))return false;
        // Taken from the shortcuts (Strg+Einfg copies elsewhere) and handled as a key here.
        if(event->type()==QEvent::ShortcutOverride){event->accept();return true;}
        // The field (and this filter with it) may go while the dialog runs, when leaving it takes its text and the panel is
        // built anew: the dialog belongs to the window, and the field is only touched while it is there.
        QPointer<QWidget> field=static_cast<QWidget*>(object);
        const QString font=family();
        const QString c=CharacterDialog::ask(font,field->window());
        if(!c.isEmpty()&&field){
            if(auto *line=qobject_cast<QLineEdit*>(field.data()))line->insert(c);
            else if(auto *plain=qobject_cast<QPlainTextEdit*>(field.data()))plain->insertPlainText(c);
        }
        return true;
    }
private:
    std::function<QString()> family;
};
}
void offerSpecialCharacters(QWidget *field,std::function<QString()> family){new SpecialCharacterKey(field,std::move(family));}

ComponentDialog::ComponentDialog(const Item &component,const QString &caption_,QWidget *parent):QDialog(parent),before(component),captionBefore(caption_){
    setWindowTitle(ui("Bauteileigenschaften"));auto *layout=new QVBoxLayout(this);
    auto *names=new QGroupBox;auto *form=new QFormLayout(names);layout->addWidget(names);
    designator=new QLineEdit(component.designator);designator->setObjectName("designator");
    designatorVisible=new QCheckBox(ui("sichtbar"));designatorVisible->setObjectName("designatorVisible");designatorVisible->setChecked(component.designatorVisible);
    autoNumber=new QCheckBox(ui("automatisch Nummerieren"));autoNumber->setObjectName("autoNumber");autoNumber->setChecked(component.autoNumber);
    {auto *row=new QHBoxLayout;row->addWidget(designator,1);row->addWidget(designatorVisible);row->addWidget(autoNumber);form->addRow(ui("Bezeichner:"),row);}
    value=new QLineEdit(component.value);value->setObjectName("value");
    valueVisible=new QCheckBox(ui("sichtbar"));valueVisible->setObjectName("valueVisible");valueVisible->setChecked(component.valueVisible);
    askValue=new QCheckBox(ui("beim Reinziehen nachfragen"));askValue->setObjectName("askValue");askValue->setChecked(component.askValue);
    {auto *row=new QHBoxLayout;row->addWidget(value,1);row->addWidget(valueVisible);row->addWidget(askValue);form->addRow(ui("Wert:"),row);}
    auto *options=new QGroupBox(ui("Optionen"));auto *of=new QFormLayout(options);layout->addWidget(options);
    partsList=new QCheckBox(ui("Bauteil in Stückliste aufnehmen"));partsList->setObjectName("partsList");partsList->setChecked(component.inPartsList);of->addRow(partsList);
    extra=new QLineEdit(component.extra.value(0));extra->setObjectName("extra");of->addRow(ui("Zusatztext für Stückliste:"),extra);
    caption=new QLineEdit(caption_);caption->setObjectName("caption");of->addRow(ui("Unterschrift in der Bibliothek:"),caption);
    auto *pins=new QGroupBox(ui("Kontakte"));auto *pl=new QVBoxLayout(pins);layout->addWidget(pins,1);
    auto *table=new QTableWidget(0,2);table->setObjectName("contacts");table->setHorizontalHeaderLabels({ui("Name"),ui("Text")});
    table->horizontalHeader()->setStretchLastSection(true);table->verticalHeader()->hide();table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for(const auto *c:contacts(component)){const int r=table->rowCount();table->insertRow(r);table->setItem(r,0,new QTableWidgetItem(c->name));table->setItem(r,1,new QTableWidgetItem(c->text));}
    pl->addWidget(table);
    layout->addWidget(okCancel(this));resize(460,420);
}
void ComponentDialog::apply(Item &c,QString &cap,bool changedOnly) const{
    auto take=[&](auto &field,const auto &now,const auto &was){if(!changedOnly||now!=was)field=now;};
    take(c.designator,designator->text(),before.designator);take(c.value,value->text(),before.value);
    take(c.designatorVisible,designatorVisible->isChecked(),before.designatorVisible);take(c.autoNumber,autoNumber->isChecked(),before.autoNumber);
    take(c.valueVisible,valueVisible->isChecked(),before.valueVisible);take(c.askValue,askValue->isChecked(),before.askValue);
    take(c.inPartsList,partsList->isChecked(),before.inPartsList);
    if(!changedOnly||extra->text()!=before.extra.value(0)){if(c.extra.isEmpty())c.extra.append(QString());c.extra[0]=extra->text();}
    take(cap,caption->text(),captionBefore);
}

SheetChoiceDialog::SheetChoiceDialog(const QString &title,const QStringList &sheets,QWidget *parent):QDialog(parent){
    setWindowTitle(title);auto *layout=new QVBoxLayout(this);
    layout->addWidget(new QLabel(ui("Blätter auswählen:")));
    list=new QListWidget;list->setObjectName("sheets");
    for(int i=0;i<sheets.size();i++){auto *item=new QListWidgetItem(QStringLiteral("%1  %2").arg(i+1).arg(sheets[i]));item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Checked);list->addItem(item);}
    layout->addWidget(list,1);
    auto *row=new QHBoxLayout;layout->addLayout(row);
    auto *all=new QPushButton(ui("Alle auswählen"));all->setObjectName("all");row->addWidget(all);
    auto *nothing=new QPushButton(ui("Auswahl entfernen"));nothing->setObjectName("nothing");row->addWidget(nothing);row->addStretch();
    auto set=[this](Qt::CheckState state){for(int i=0;i<list->count();i++)list->item(i)->setCheckState(state);};
    connect(all,&QPushButton::clicked,this,[set]{set(Qt::Checked);});connect(nothing,&QPushButton::clicked,this,[set]{set(Qt::Unchecked);});
    layout->addWidget(okCancel(this));resize(360,360);
}
QList<int> SheetChoiceDialog::sheets() const{
    QList<int> out;for(int i=0;i<list->count();i++)if(list->item(i)->checkState()==Qt::Checked)out<<i;return out;
}
NumberingDialog::NumberingDialog(bool selection,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Bauteilnummerierung"));auto *layout=new QVBoxLayout(this);
    auto *order=new QGroupBox(ui("Geometrische Sortierung"));auto *ol=new QVBoxLayout(order);layout->addWidget(order);
    none=new QRadioButton(ui("Keine Sortierung"));none->setObjectName("none");none->setChecked(true);ol->addWidget(none);
    columns=new QRadioButton(ui("Spaltenweise"));columns->setObjectName("columns");ol->addWidget(columns);
    rows=new QRadioButton(ui("Zeilenweise"));rows->setObjectName("rows");ol->addWidget(rows);
    auto *rf=new QFormLayout;ol->addLayout(rf);
    raster=new QDoubleSpinBox;raster->setObjectName("raster");raster->setRange(1,1000);raster->setValue(20);raster->setSuffix(QStringLiteral(" mm"));raster->setLocale(uiLocale());
    rf->addRow(ui("Geometrisches Raster:"),raster);
    auto *options=new QGroupBox(ui("Optionen"));auto *of=new QFormLayout(options);layout->addWidget(options);
    selectedOnly=new QCheckBox(ui("Nur markierte Bauteile"));selectedOnly->setObjectName("selectedOnly");selectedOnly->setEnabled(selection);of->addRow(selectedOnly);
    lettersOnly=new QCheckBox(ui("Nur Bauteile mit Bezeichner:"));lettersOnly->setObjectName("lettersOnly");
    letters=new QLineEdit;letters->setObjectName("letters");letters->setMaximumWidth(80);of->addRow(lettersOnly,letters);
    start=new QSpinBox;start->setObjectName("start");start->setRange(0,999999);start->setValue(1);of->addRow(ui("Start bei:"),start);
    layout->addWidget(okCancel(this));
}

PartsListDialog::PartsListDialog(const Document &doc,const QList<int> &chosen,const QString &file,QWidget *parent)
    :QDialog(parent),document(doc),sheets(chosen),fileName(file){
    setWindowTitle(ui("Stückliste"));auto *outer=new QVBoxLayout(this);
    auto *bar=new QHBoxLayout;outer->addLayout(bar);auto *body=new QHBoxLayout;outer->addLayout(body,1);
    grid=new QTableWidget;grid->setObjectName("parts");grid->verticalHeader()->hide();grid->horizontalHeader()->setStretchLastSection(true);
    body->addWidget(grid,1);
    auto *side=new QVBoxLayout;body->addLayout(side);
    auto button=[&](QBoxLayout *l,const QString &text,const char *name,std::function<void()> run){auto *b=new QPushButton(text);b->setObjectName(name);l->addWidget(b);connect(b,&QPushButton::clicked,this,std::move(run));return b;};
    // The buttons above the list, as in sPlan.
    button(bar,ui("Exportieren..."),"export",[this]{exportText();});
    button(bar,ui("Auf Blatt einfügen..."),"insert",[this]{insertOnSheet();});
    bar->addStretch();
    button(bar,ui("Zwischenablage"),"clipboard",[this]{copyToClipboard();});
    button(bar,ui("Speichern..."),"saveRtf",[this]{
        const QString file=QFileDialog::getSaveFileName(this,ui("Stückliste speichern"),QStringLiteral("Stückliste.rtf"),ui("Rich-Text-Format (*.rtf)"));
        QString error;if(!file.isEmpty()&&!saveRtf(file,&error))QMessageBox::warning(this,ui("Stückliste speichern"),error);});
    button(bar,ui("Laden..."),"openRtf",[this]{
        const QString file=QFileDialog::getOpenFileName(this,ui("Stückliste öffnen"),QString(),ui("Rich-Text-Format (*.rtf)"));
        QString error;if(!file.isEmpty()&&!openRtf(file,&error))QMessageBox::warning(this,ui("Stückliste öffnen"),error);});
    button(bar,QStringLiteral("?"),"help",[this]{if(help)help();})->setToolTip(ui("Hilfe"));
    // At the right: the font, the options, the sorting and printing.
    auto *fontBox=new QGroupBox(ui("Schriftart"));auto *fl=new QHBoxLayout(fontBox);side->addWidget(fontBox);
    font=new QFontComboBox;font->setObjectName("listFont");font->setCurrentFont(QFont(QStringLiteral("Arial")));fl->addWidget(font,1);
    fontSize=new QSpinBox;fontSize->setObjectName("listFontSize");fontSize->setRange(6,48);fontSize->setValue(8);fl->addWidget(fontSize);
    auto *options=new QGroupBox(ui("Optionen"));auto *ol=new QVBoxLayout(options);side->addWidget(options);
    fileData=new QCheckBox(ui("Dateidaten"));fileData->setObjectName("fileData");ol->addWidget(fileData);
    merge=new QCheckBox(ui("Gleiche Bauteile zusammenf."));merge->setObjectName("merge");merge->setChecked(true);ol->addWidget(merge);
    verticalLines=new QCheckBox(ui("Vertikale Trennlinien"));verticalLines->setObjectName("verticalLines");verticalLines->setChecked(true);ol->addWidget(verticalLines);
    horizontalLines=new QCheckBox(ui("Horizontale Trennlinien"));horizontalLines->setObjectName("horizontalLines");ol->addWidget(horizontalLines);
    for(int k=0;k<4;k++){auto *c=new QCheckBox(ui("Zusatztext %1").arg(k+1));c->setObjectName(QStringLiteral("extra%1").arg(k+1));ol->addWidget(c);extras<<c;}
    auto *sorting=new QGroupBox(ui("Gruppen sortieren"));auto *sl=new QVBoxLayout(sorting);side->addWidget(sorting);
    frequency=new QRadioButton(ui("Häufigkeit"));frequency->setObjectName("frequency");sl->addWidget(frequency);
    alphabetical=new QRadioButton(ui("Alphabetisch"));alphabetical->setObjectName("alphabetical");alphabetical->setChecked(true);sl->addWidget(alphabetical);
    auto *printing=new QGroupBox(ui("Drucken"));auto *pl=new QFormLayout(printing);side->addWidget(printing);
    auto margin=[](const char *name,int value){auto *b=new QSpinBox;b->setObjectName(name);b->setRange(0,99);b->setValue(value);b->setSuffix(QStringLiteral(" mm"));return b;};
    topMargin=margin("topMargin",10);leftMargin=margin("leftMargin",20);pl->addRow(ui("Oberer Rand:"),topMargin);pl->addRow(ui("Linker Rand:"),leftMargin);
    portrait=new QRadioButton(ui("Hochformat"));portrait->setObjectName("portrait");portrait->setChecked(true);pl->addRow(portrait);
    landscape=new QRadioButton(ui("Querformat"));landscape->setObjectName("landscape");pl->addRow(landscape);
    {auto *b=new QPushButton(ui("Drucken..."));b->setObjectName("print");pl->addRow(b);connect(b,&QPushButton::clicked,this,[this]{print();});}
    side->addStretch();
    button(side,ui("Schließen"),"close",[this]{reject();});
    // A change of these options makes the list anew; changes made by hand are lost, as in the reference.
    for(QCheckBox *c:QList<QCheckBox*>{merge,fileData}+extras)connect(c,&QCheckBox::toggled,this,[this]{rebuild();});
    connect(frequency,&QRadioButton::toggled,this,[this]{rebuild();});
    connect(font,&QFontComboBox::currentFontChanged,this,[this]{applyFont();});
    connect(fontSize,&QSpinBox::valueChanged,this,[this]{applyFont();});
    rebuild();applyFont();resize(940,640);
}
void PartsListDialog::applyFont(){
    QFont f=font->currentFont();f.setPointSize(fontSize->value());grid->setFont(f);grid->resizeColumnsToContents();
}
QPageLayout PartsListDialog::printLayout(QPageLayout layout) const{
    layout.setOrientation(landscape->isChecked()?QPageLayout::Landscape:QPageLayout::Portrait);
    const QMarginsF m=layout.margins(QPageLayout::Millimeter);layout.setUnits(QPageLayout::Millimeter);
    layout.setMargins(QMarginsF(leftMargin->value(),topMargin->value(),m.right(),m.bottom()),QPageLayout::OutOfBoundsPolicy::Clamp);
    return layout;
}
void PartsListDialog::rebuild(){
    PartsListOptions o;o.sheets=sheets;o.fileName=fileName;o.merge=merge->isChecked();o.sort=frequency->isChecked()?PartsListOptions::Sort::Frequency:PartsListOptions::Sort::Alphabetical;
    rows=partsList(document,o);
    QList<int> chosen;for(int k=0;k<4;k++)if(extras[k]->isChecked())chosen<<k;
    const PartsTable t=partsTable(rows,chosen);
    grid->clear();grid->setColumnCount(int(t.header.size()));grid->setRowCount(int(t.rows.size()));grid->setHorizontalHeaderLabels(t.header);
    for(int r=0;r<t.rows.size();r++)for(int c=0;c<t.rows[r].size();c++)grid->setItem(r,c,new QTableWidgetItem(t.rows[r][c]));
    grid->resizeColumnsToContents();
}
bool PartsListDialog::saveRtf(const QString &file,QString *error) const{
    QSaveFile out(file);
    if(!out.open(QIODevice::WriteOnly)||out.write(partsRtf(table(),fileLines(),font->currentFont().family(),fontSize->value()))<0||!out.commit()){if(error)*error=out.errorString();return false;}
    return true;
}
bool PartsListDialog::openRtf(const QString &file,QString *error){
    QFile in(file);if(!in.open(QIODevice::ReadOnly)){if(error)*error=in.errorString();return false;}
    const PartsTable t=partsFromRtf(in.readAll());
    if(t.header.isEmpty()){if(error)*error=ui("Die Datei enthält keine Stückliste");return false;}
    // The list as saved; the options make it anew from the schematic.
    grid->clear();int columns=int(t.header.size());for(const auto &r:t.rows)columns=std::max(columns,int(r.size()));
    grid->setColumnCount(columns);grid->setRowCount(int(t.rows.size()));grid->setHorizontalHeaderLabels(t.header);
    for(int r=0;r<t.rows.size();r++)for(int c=0;c<t.rows[r].size();c++)grid->setItem(r,c,new QTableWidgetItem(t.rows[r][c]));
    grid->resizeColumnsToContents();rows.clear();
    return true;
}
PartsTable PartsListDialog::table() const{
    PartsTable t;
    for(int c=0;c<grid->columnCount();c++)t.header<<(grid->horizontalHeaderItem(c)?grid->horizontalHeaderItem(c)->text():QString());
    for(int r=0;r<grid->rowCount();r++){QStringList cells;for(int c=0;c<grid->columnCount();c++)cells<<(grid->item(r,c)?grid->item(r,c)->text():QString());t.rows<<cells;}
    return t;
}
QStringList PartsListDialog::fileLines() const{
    if(!fileData->isChecked())return {};
    QStringList lines{uiDate(QDate::currentDate())+u' '+uiTime(QTime::currentTime())};
    if(!fileName.isEmpty())lines<<QFileInfo(fileName).fileName()<<QDir::toNativeSeparators(QFileInfo(fileName).absolutePath());
    QStringList names;for(int i=0;i<document.sheets.size();i++)if(sheets.isEmpty()||sheets.contains(i))names<<document.sheets[i].name;
    lines<<names.join(QStringLiteral(", "));
    return lines;
}
QList<int> PartsListDialog::groupStarts() const{
    QList<int> out;for(int i=0;i<rows.size();i++)if(i==0||rows[i].letters!=rows[i-1].letters)out<<i;return out;
}
void PartsListDialog::copyToClipboard() const{
    const PartsTable t=table();QString text;for(const auto &l:fileLines())text+=l+u'\n';
    text+=partsText(t,groupStarts(),u'\t',true,false);
    QString html=QStringLiteral("<table border=1 cellspacing=0 cellpadding=3><tr>");
    for(const auto &h:t.header)html+=QStringLiteral("<th>")+h.toHtmlEscaped()+QStringLiteral("</th>");
    html+=QStringLiteral("</tr>");
    for(const auto &r:t.rows){html+=QStringLiteral("<tr>");for(const auto &c:r)html+=QStringLiteral("<td>")+c.toHtmlEscaped()+QStringLiteral("</td>");html+=QStringLiteral("</tr>");}
    html+=QStringLiteral("</table>");
    auto *mime=new QMimeData;mime->setText(text);mime->setHtml(html);QGuiApplication::clipboard()->setMimeData(mime);
}
void PartsListDialog::print(){
    QPrinter printer;printer.setPageLayout(printLayout(printer.pageLayout()));QPrintDialog dialog(&printer,this);if(dialog.exec()!=QDialog::Accepted)return;
    const PartsTable t=table();
    QString html;for(const auto &l:fileLines())html+=l.toHtmlEscaped()+QStringLiteral("<br>");
    const QString border=verticalLines->isChecked()||horizontalLines->isChecked()?QStringLiteral("1"):QStringLiteral("0");
    html+=QStringLiteral("<table border=%1 cellspacing=0 cellpadding=4><tr>").arg(border);
    for(const auto &h:t.header)html+=QStringLiteral("<th align=left>")+h.toHtmlEscaped()+QStringLiteral("</th>");
    html+=QStringLiteral("</tr>");
    for(const auto &r:t.rows){html+=QStringLiteral("<tr>");for(const auto &c:r)html+=QStringLiteral("<td>")+c.toHtmlEscaped()+QStringLiteral("</td>");html+=QStringLiteral("</tr>");}
    html+=QStringLiteral("</table>");
    QFont f=font->currentFont();f.setPointSize(fontSize->value());
    QTextDocument text;text.setDefaultFont(f);text.setHtml(html);text.print(&printer);
}
void PartsListDialog::exportText(){
    // "Export" as in sPlan: separator, the field names as the first line, empty lines between the groups, and the lines
    // of the file in a table below.
    QDialog options(this);options.setObjectName("exportDialog");options.setWindowTitle(ui("Export"));auto *layout=new QVBoxLayout(&options);
    auto *top=new QHBoxLayout;layout->addLayout(top);
    auto *separatorBox=new QGroupBox(ui("Feld-Trennzeichen"));auto *sl=new QVBoxLayout(separatorBox);top->addWidget(separatorBox);
    auto *semicolon=new QRadioButton(ui("Semikolon"));semicolon->setObjectName("semicolon");semicolon->setChecked(true);sl->addWidget(semicolon);
    auto *tab=new QRadioButton(ui("Tabulator"));tab->setObjectName("tab");sl->addWidget(tab);
    auto *optionBox=new QGroupBox(ui("Optionen"));auto *ol=new QVBoxLayout(optionBox);top->addWidget(optionBox);
    auto *names=new QCheckBox(ui("Feldnamen als 1. Zeile"));names->setObjectName("fieldNames");names->setChecked(true);ol->addWidget(names);
    auto *empty=new QCheckBox(ui("Leerzeilen einfügen"));empty->setObjectName("emptyLines");ol->addWidget(empty);
    top->addStretch();
    auto *buttons=new QDialogButtonBox(Qt::Vertical);buttons->addButton(ui("Speichern..."),QDialogButtonBox::AcceptRole);buttons->addButton(QDialogButtonBox::Cancel);top->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&options,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&options,&QDialog::reject);
    auto *preview=new QTableWidget;preview->setObjectName("exportPreview");preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    preview->verticalHeader()->hide();preview->horizontalHeader()->hide();layout->addWidget(preview,1);
    // The lines as the file has them: the file's data, the field names, the rows with empty lines between the groups.
    const QStringList head=fileLines();const PartsTable t=table();const QList<int> starts=groupStarts();
    auto fill=[=]{
        QList<QStringList> lines;for(const auto &l:head)lines<<QStringList{l};
        if(names->isChecked())lines<<t.header;
        for(int i=0;i<t.rows.size();i++){if(empty->isChecked()&&i>0&&starts.contains(i))lines<<QStringList{};lines<<t.rows[i];}
        int columns=1;for(const auto &l:lines)columns=std::max(columns,int(l.size()));
        preview->clear();preview->setRowCount(int(lines.size()));preview->setColumnCount(columns);
        for(int r=0;r<lines.size();r++)for(int c=0;c<lines[r].size();c++)preview->setItem(r,c,new QTableWidgetItem(lines[r][c]));
        preview->resizeColumnsToContents();
    };
    connect(names,&QCheckBox::toggled,&options,fill);connect(empty,&QCheckBox::toggled,&options,fill);fill();
    options.resize(780,420);
    if(exportOptions)exportOptions(&options);
    if(options.exec()!=QDialog::Accepted)return;
    const QString file=QFileDialog::getSaveFileName(this,ui("Exportieren"),QStringLiteral("Stückliste.csv"),ui("Textdateien (*.csv *.txt)"));
    if(file.isEmpty())return;
    QString text;for(const auto &l:head)text+=l+u'\n';
    text+=partsText(t,starts,semicolon->isChecked()?u';':u'\t',names->isChecked(),empty->isChecked());
    QSaveFile out(file);
    if(!out.open(QIODevice::WriteOnly)||out.write(text.toUtf8())<0||!out.commit())QMessageBox::warning(this,ui("Exportieren"),out.errorString());
}
void PartsListDialog::insertOnSheet(){
    // "Stückliste auf Blatt einfügen": font, frame and lines, and where the list breaks into tables side by side.
    QDialog options(this);options.setWindowTitle(ui("Stückliste auf Blatt einfügen"));auto *layout=new QVBoxLayout(&options);
    auto *top=new QHBoxLayout;layout->addLayout(top);
    auto *fontBox=new QGroupBox(ui("Schriftart"));auto *fl=new QVBoxLayout(fontBox);top->addWidget(fontBox);
    auto *family=new QFontComboBox;family->setObjectName("family");family->setCurrentFont(QFont(QStringLiteral("Arial")));fl->addWidget(family);
    auto *sizeRow=new QHBoxLayout;auto *size=new QSpinBox;size->setObjectName("size");size->setRange(6,96);size->setValue(16);sizeRow->addWidget(size);
    sizeRow->addWidget(new QLabel(ui("[1/10 mm]")));sizeRow->addStretch();fl->addLayout(sizeRow);
    auto *optionBox=new QGroupBox(ui("Optionen"));auto *ol=new QGridLayout(optionBox);top->addWidget(optionBox);
    auto *frame=new QCheckBox(ui("Rahmen"));frame->setObjectName("frame");frame->setChecked(true);ol->addWidget(frame,0,0);
    auto *shadow=new QCheckBox(ui("Schatten"));shadow->setObjectName("shadow");ol->addWidget(shadow,0,1);
    auto *vertical=new QCheckBox(ui("Senkrechte Trennlinien"));vertical->setObjectName("vertical");vertical->setChecked(true);ol->addWidget(vertical,1,0,1,2);
    auto *alternate=new QCheckBox(ui("Zeilen alternierend"));alternate->setObjectName("alternate");ol->addWidget(alternate,2,0,1,2);
    auto *heightBox=new QGroupBox(ui("Höhe der Stückliste"));auto *hl=new QGridLayout(heightBox);layout->addWidget(heightBox);
    auto *fit=new QRadioButton(ui("Maximal (an Seitenhöhe angepasst)"));fit->setObjectName("fit");fit->setChecked(true);hl->addWidget(fit,0,0,1,3);
    auto *manual=new QRadioButton(ui("Manuell umbrechen nach jeweils"));manual->setObjectName("manual");hl->addWidget(manual,1,0);
    auto *rows=new QSpinBox;rows->setObjectName("rows");rows->setRange(3,999);rows->setValue(16);hl->addWidget(rows,1,1);hl->addWidget(new QLabel(ui("Zeilen")),1,2);
    auto *info=new QLabel;info->setObjectName("breaks");info->setStyleSheet(QStringLiteral("color:gray"));hl->addWidget(info,2,0,1,3);
    const double sheetHeight=document.sheets.isEmpty()?210:document.sheet().height;const int count=int(table().rows.size());
    auto drawingNow=[=]{
        PartsDrawing d;d.height=size->value()/10.;d.family=family->currentFont().family();d.frame=frame->isChecked();d.shadow=shadow->isChecked();
        d.verticalLines=vertical->isChecked();d.horizontalLines=false;d.alternate=alternate->isChecked();
        d.rowsPerColumn=manual->isChecked()?rows->value():fittingRows(sheetHeight,d);
        return d;
    };
    auto updateInfo=[=]{
        const int per=drawingNow().rowsPerColumn,breaks=per>0&&count>per?(count-1)/per:0;
        info->setText(breaks==0?ui("Kein Umbruch"):breaks==1?ui("1 Umbruch"):ui("%1 Umbrüche").arg(breaks));
    };
    for(QAbstractButton *b:{static_cast<QAbstractButton*>(fit),static_cast<QAbstractButton*>(manual)})connect(b,&QAbstractButton::toggled,&options,updateInfo);
    connect(rows,&QSpinBox::valueChanged,&options,updateInfo);connect(size,&QSpinBox::valueChanged,&options,updateInfo);
    updateInfo();
    auto *buttons=new QDialogButtonBox;layout->addWidget(buttons);
    buttons->addButton(ui("Einfügen"),QDialogButtonBox::AcceptRole)->setDefault(true);buttons->addButton(ui("Abbrechen"),QDialogButtonBox::RejectRole);
    connect(buttons,&QDialogButtonBox::accepted,&options,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&options,&QDialog::reject);
    if(placementOptions)placementOptions(&options);
    if(options.exec()!=QDialog::Accepted||!insert)return;
    insert(table(),drawingNow());accept();
}

namespace {
// Designator and value of a component as shown, in its own context.
QString shownDesignator(const Document &d,const PlacedComponent &p,const QString &file){return expandVariables(p.item->designator,{&d,p.sheet,p.item,file});}
QString shownValue(const Document &d,const PlacedComponent &p,const QString &file){return expandVariables(p.item->value,{&d,p.sheet,p.item,file});}
QString sheetLabel(const Document &d,int sheet){return QStringLiteral("%1: %2").arg(sheet+1).arg(d.sheets.value(sheet).name);}
}
ParentChoiceDialog::ParentChoiceDialog(const Document &d,const QString &file,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Verknüpfe mit PARENT..."));auto *layout=new QVBoxLayout(this);
    list=new QTreeWidget;list->setObjectName("parents");list->setRootIsDecorated(false);
    list->setHeaderLabels({ui("Bezeichner"),ui("Wert"),ui("auf Blatt")});
    for(const auto &p:allComponents(d)){
        if(!p.item->parent)continue;
        auto *row=new QTreeWidgetItem(list,{shownDesignator(d,p,file),shownValue(d,p,file),sheetLabel(d,p.sheet)});row->setData(0,Qt::UserRole,p.item->id);
    }
    if(list->topLevelItemCount())list->setCurrentItem(list->topLevelItem(0));
    connect(list,&QTreeWidget::itemDoubleClicked,this,&QDialog::accept);
    layout->addWidget(list,1);layout->addWidget(okCancel(this));resize(420,320);
}
QString ParentChoiceDialog::parentId() const{return list->currentItem()?list->currentItem()->data(0,Qt::UserRole).toString():QString();}
ParentChildListDialog::ParentChildListDialog(const Document &d,const QString &file,QWidget *parent):QDialog(parent),document(d),fileName(file){
    setWindowTitle(ui("Parent-Child-Liste"));auto *layout=new QVBoxLayout(this);
    auto *options=new QHBoxLayout;layout->addLayout(options);
    currentOnly=new QCheckBox(ui("Nur aktuelle Seite"));currentOnly->setObjectName("currentOnly");options->addWidget(currentOnly);
    showContacts=new QCheckBox(ui("Kontakte anzeigen"));showContacts->setObjectName("showContacts");options->addWidget(showContacts);
    expanded=new QCheckBox(ui("Ausgeklappt"));expanded->setObjectName("expanded");expanded->setChecked(true);options->addWidget(expanded);options->addStretch();
    tree=new QTreeWidget;tree->setObjectName("parentChild");tree->setHeaderLabels({ui("Bezeichner"),ui("Wert"),ui("auf Blatt"),ui("Kontakte")});layout->addWidget(tree,1);
    auto *buttons=new QHBoxLayout;layout->addLayout(buttons);
    auto *copy=new QPushButton(ui("Zwischenablage..."));copy->setObjectName("clipboard");buttons->addWidget(copy);buttons->addStretch();
    auto *close=new QPushButton(ui("Schließen"));buttons->addWidget(close);
    connect(copy,&QPushButton::clicked,this,[this]{QGuiApplication::clipboard()->setText(text());QMessageBox::information(this,windowTitle(),ui("Die Daten wurden in die Zwischenablage kopiert"));});
    connect(close,&QPushButton::clicked,this,&QDialog::reject);
    for(QCheckBox *c:{currentOnly,showContacts,expanded})connect(c,&QCheckBox::toggled,this,[this]{fill();});
    connect(tree,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *item){chosen=item->data(0,Qt::UserRole).toString();if(!chosen.isEmpty())accept();});
    fill();resize(560,420);
}
void ParentChildListDialog::fill(){
    tree->clear();tree->setColumnHidden(3,!showContacts->isChecked());
    auto contactsOf=[](const Item &c){QStringList names;for(const auto *k:contacts(c))names<<(k->text.isEmpty()?k->name:k->text);return names.join(QStringLiteral(", "));};
    for(const auto &p:allComponents(document)){
        if(!p.item->parent)continue;
        const auto children=childrenOf(document,p.item->id);
        if(currentOnly->isChecked()&&p.sheet!=document.activeSheet&&std::none_of(children.begin(),children.end(),[&](const PlacedComponent &c){return c.sheet==document.activeSheet;}))continue;
        auto *row=new QTreeWidgetItem(tree,{shownDesignator(document,p,fileName),shownValue(document,p,fileName),sheetLabel(document,p.sheet),contactsOf(*p.item)});
        row->setData(0,Qt::UserRole,p.item->id);
        if(children.isEmpty())new QTreeWidgetItem(row,{ui("Kein CHILD definiert")});
        for(const auto &c:children){
            auto *child=new QTreeWidgetItem(row,{shownDesignator(document,c,fileName),shownValue(document,c,fileName),sheetLabel(document,c.sheet),contactsOf(*c.item)});
            child->setData(0,Qt::UserRole,c.item->id);
        }
        row->setExpanded(expanded->isChecked());
    }
    for(int k=0;k<4;k++)tree->resizeColumnToContents(k);
}
QString ParentChildListDialog::text() const{
    QStringList lines;
    for(int i=0;i<tree->topLevelItemCount();i++){
        const auto *row=tree->topLevelItem(i);QStringList cells;for(int k=0;k<tree->columnCount();k++)if(!tree->isColumnHidden(k))cells<<row->text(k);
        lines<<cells.join(u'\t');
        for(int j=0;j<row->childCount();j++){QStringList c;for(int k=0;k<tree->columnCount();k++)if(!tree->isColumnHidden(k))c<<row->child(j)->text(k);lines<<u'\t'+c.join(u'\t');}
    }
    return lines.join(u'\n')+u'\n';
}

ContactListDialog::ContactListDialog(const Document &d,const QString &file,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Kontaktliste"));auto *layout=new QVBoxLayout(this);
    list=new QTreeWidget;list->setObjectName("contacts");list->setRootIsDecorated(false);list->setSortingEnabled(false);
    list->setHeaderLabels({ui("Blatt"),ui("Bauteil"),ui("Kontakt"),ui("Name")});
    for(const auto &p:allComponents(d)){
        const QString designator=expandVariables(p.item->designator,{&d,p.sheet,p.item,file});
        for(const auto *c:contacts(*p.item)){
            auto *row=new QTreeWidgetItem(list,{QString::number(p.sheet+1),designator,c->text.isEmpty()?ui("Ohne Kontaktbezeichnung"):c->text,c->name});
            row->setData(0,Qt::UserRole,p.item->id);
        }
    }
    for(int k=0;k<4;k++)list->resizeColumnToContents(k);
    connect(list,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *row){chosen=row->data(0,Qt::UserRole).toString();accept();});
    layout->addWidget(list,1);
    auto *buttons=new QHBoxLayout;layout->addLayout(buttons);
    auto *copy=new QPushButton(ui("Zwischenablage..."));copy->setObjectName("clipboard");buttons->addWidget(copy);buttons->addStretch();
    auto *close=new QPushButton(ui("Schließen"));buttons->addWidget(close);
    connect(copy,&QPushButton::clicked,this,[this]{QGuiApplication::clipboard()->setText(text());QMessageBox::information(this,windowTitle(),ui("Die Kontaktdaten wurden in die Zwischenablage kopiert"));});
    connect(close,&QPushButton::clicked,this,&QDialog::reject);
    resize(520,420);
}
QString ContactListDialog::text() const{
    QStringList lines{QStringList{ui("Blatt"),ui("Bauteil"),ui("Kontakt"),ui("Name")}.join(u'\t')};
    for(int i=0;i<list->topLevelItemCount();i++){const auto *r=list->topLevelItem(i);lines<<QStringList{r->text(0),r->text(1),r->text(2),r->text(3)}.join(u'\t');}
    return lines.join(u'\n')+u'\n';
}
ContentsDialog::ContentsDialog(QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Inhaltsverzeichnis einfügen"));auto *form=new QFormLayout(this);
    height=new QDoubleSpinBox;height->setObjectName("height");height->setRange(.5,50);height->setValue(3.5);height->setSuffix(QStringLiteral(" mm"));height->setLocale(uiLocale());
    form->addRow(ui("Texthöhe:"),height);
    frame=new QCheckBox(ui("Rahmen"));frame->setChecked(true);form->addRow(frame);
    verticalLines=new QCheckBox(ui("Senkrechte Trennlinien"));verticalLines->setChecked(true);form->addRow(verticalLines);
    horizontalLines=new QCheckBox(ui("Horizontale Trennlinien"));horizontalLines->setChecked(true);form->addRow(horizontalLines);
    form->addRow(okCancel(this));
}
PartsDrawing ContentsDialog::drawing() const{
    PartsDrawing d;d.height=height->value();d.frame=frame->isChecked();d.verticalLines=verticalLines->isChecked();d.horizontalLines=horizontalLines->isChecked();return d;
}

namespace {
QString shownTextOf(const Document &d,const PlacedComponent &p){return expandVariables(p.item->text,{&d,p.sheet,nullptr,{},p.item});}
}
TargetChoiceDialog::TargetChoiceDialog(const Document &d,const QString &except,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Ziel auswählen..."));auto *layout=new QVBoxLayout(this);
    list=new QTreeWidget;list->setObjectName("targets");list->setRootIsDecorated(false);list->setHeaderLabels({ui("Text"),ui("auf Blatt")});
    for(const auto &p:allTexts(d)){
        if(!p.item->linkable||p.item->id==except)continue;
        auto *row=new QTreeWidgetItem(list,{shownTextOf(d,p),sheetLabel(d,p.sheet)});row->setData(0,Qt::UserRole,p.item->id);
    }
    if(list->topLevelItemCount())list->setCurrentItem(list->topLevelItem(0));
    else layout->addWidget(new QLabel(ui("Es sind keine Texte als Ziel freigegeben.")));
    connect(list,&QTreeWidget::itemDoubleClicked,this,&QDialog::accept);
    layout->addWidget(list,1);layout->addWidget(okCancel(this));resize(420,320);
}
QString TargetChoiceDialog::targetId() const{return list->currentItem()?list->currentItem()->data(0,Qt::UserRole).toString():QString();}
LinkListDialog::LinkListDialog(const Document &d,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Linkliste"));auto *layout=new QVBoxLayout(this);
    list=new QTreeWidget;list->setObjectName("links");list->setRootIsDecorated(false);list->setHeaderLabels({ui("Blatt"),ui("Linktext"),ui("Ziel:")});
    for(const auto &p:allTexts(d)){
        if(p.item->linkTarget.isEmpty()&&p.item->link.isEmpty())continue;
        const PlacedComponent to=textWithId(d,p.item->linkTarget);
        const QString target=to.item?QStringLiteral("%1 (%2)").arg(shownTextOf(d,to),sheetLabel(d,to.sheet)):p.item->link;
        auto *row=new QTreeWidgetItem(list,{QString::number(p.sheet+1),shownTextOf(d,p),target});row->setData(0,Qt::UserRole,p.item->id);
    }
    for(int k=0;k<3;k++)list->resizeColumnToContents(k);
    connect(list,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *row){chosen=row->data(0,Qt::UserRole).toString();accept();});
    layout->addWidget(list,1);
    auto *buttons=new QHBoxLayout;layout->addLayout(buttons);
    auto *copy=new QPushButton(ui("Zwischenablage..."));copy->setObjectName("clipboard");buttons->addWidget(copy);buttons->addStretch();
    auto *close=new QPushButton(ui("Schließen"));buttons->addWidget(close);
    connect(copy,&QPushButton::clicked,this,[this]{QGuiApplication::clipboard()->setText(text());QMessageBox::information(this,windowTitle(),ui("Die Linkliste wurde in die Zwischenablage kopiert"));});
    connect(close,&QPushButton::clicked,this,&QDialog::reject);
    resize(560,400);
}
QString LinkListDialog::text() const{
    QStringList lines{QStringList{ui("Blatt"),ui("Linktext"),ui("Ziel:")}.join(u'\t')};
    for(int i=0;i<list->topLevelItemCount();i++){const auto *r=list->topLevelItem(i);lines<<QStringList{r->text(0),r->text(1),r->text(2)}.join(u'\t');}
    return lines.join(u'\n')+u'\n';
}

TextConstantsDialog::TextConstantsDialog(const QStringList &constants,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Textkonstanten"));auto *layout=new QVBoxLayout(this);
    layout->addWidget(new QLabel(ui("Textkonstanten zum schnellen Einfügen in Texten:")));
    table=new QTableWidget(int(constants.size()),1);table->setObjectName("constants");table->horizontalHeader()->hide();table->horizontalHeader()->setStretchLastSection(true);
    for(int r=0;r<constants.size();r++)table->setItem(r,0,new QTableWidgetItem(constants[r]));
    layout->addWidget(table,1);
    auto *row=new QHBoxLayout;layout->addLayout(row);
    auto *add=new QPushButton(ui("Neue Zeile"));add->setObjectName("newRow");row->addWidget(add);
    auto *remove=new QPushButton(ui("Zeile löschen"));remove->setObjectName("deleteRow");row->addWidget(remove);row->addStretch();
    connect(add,&QPushButton::clicked,this,[this]{const int r=table->rowCount();table->insertRow(r);table->setItem(r,0,new QTableWidgetItem);table->editItem(table->item(r,0));});
    connect(remove,&QPushButton::clicked,this,[this]{if(table->currentRow()>=0)table->removeRow(table->currentRow());});
    layout->addWidget(okCancel(this));resize(420,320);
}
QStringList TextConstantsDialog::constants() const{
    QStringList out;for(int r=0;r<table->rowCount();r++)if(table->item(r,0)&&!table->item(r,0)->text().trimmed().isEmpty())out<<table->item(r,0)->text();return out;
}
namespace {
// A modifier combination as the keyboard names it (on macOS Qt's Control is the Command key, Meta the Control key).
QString modifierName(Qt::KeyboardModifiers m){
#ifdef Q_OS_MACOS
    QString out;
    // In Apple's order: Control, Option, Shift, Command.
    if(m&Qt::MetaModifier)out+=QStringLiteral("\u2303");if(m&Qt::AltModifier)out+=QStringLiteral("\u2325");
    if(m&Qt::ShiftModifier)out+=QStringLiteral("\u21e7");if(m&Qt::ControlModifier)out+=QStringLiteral("\u2318");
    if(m==Qt::AltModifier)out+=QStringLiteral(" (")+ui("Wahltaste")+u')';
    if(m==Qt::MetaModifier)out+=QStringLiteral(" (Control)");
    return out;
#else
    QStringList parts;
    if(m&Qt::ControlModifier)parts<<ui("Strg");if(m&Qt::ShiftModifier)parts<<ui("Umschalt");if(m&Qt::AltModifier)parts<<QStringLiteral("Alt");
    if(m&Qt::MetaModifier)parts<<ui("Meta (Windows-/Super-Taste)");
    return parts.join(u'+');
#endif
}
}
SettingsDialog::SettingsDialog(const GeneralSettings &g,QWidget *parent,const QList<HotkeyMode> &modes_,const DrawingSettings &drawing):QDialog(parent),modes(modes_){
    setWindowTitle(ui("Grundeinstellungen"));auto *layout=new QVBoxLayout(this);
    auto *body=new QHBoxLayout;layout->addLayout(body,1);
    pages=new QListWidget;pages->setObjectName("pages");pages->setMaximumWidth(170);body->addWidget(pages);
    auto *stack=new QStackedWidget;body->addWidget(stack,1);
    auto page=[&](const QString &title){auto *w=new QWidget;auto *l=new QVBoxLayout(w);l->setContentsMargins(6,0,0,0);pages->addItem(title);stack->addWidget(w);return l;};
    auto folderRow=[&](QGridLayout *grid,int row,const QString &label,const QString &value,const char *name){
        grid->addWidget(new QLabel(label),row,0);auto *edit=new QLineEdit(QDir::toNativeSeparators(value));edit->setObjectName(name);edit->setReadOnly(true);grid->addWidget(edit,row,1);
        auto *change=new QPushButton(ui("Ändern..."));change->setObjectName(QByteArray(name)+"Change");grid->addWidget(change,row,2);
        connect(change,&QPushButton::clicked,this,[this,edit]{
            const QString d=QFileDialog::getExistingDirectory(this,ui("Verzeichnis wählen"),QDir::fromNativeSeparators(edit->text()));if(!d.isEmpty())edit->setText(QDir::toNativeSeparators(d));});
        return edit;};
    {   // Grundeinstellungen
        auto *l=page(ui("Grundeinstellungen"));
        auto *box=new QGroupBox(ui("Diese Zeichnung (wird mit ihr gespeichert)"));auto *b=new QGridLayout(box);l->addWidget(box);
        sheetNumbers=new QCheckBox(ui("Blätter mit Seitennummer"));sheetNumbers->setObjectName("sheetNumbers");sheetNumbers->setChecked(drawing.sheetNumbers);b->addWidget(sheetNumbers,0,0,1,3);
        designatorPageNumbers=new QCheckBox(ui("Bauteile mit Seitennummer"));designatorPageNumbers->setObjectName("designatorPageNumbers");
        designatorPageNumbers->setChecked(drawing.designatorPageNumbers);b->addWidget(designatorPageNumbers,1,0,1,3);
        auto *prefixLabel=new QLabel(ui("Präfix:"));b->addWidget(prefixLabel,2,1);
        designatorPrefix=new QLineEdit(drawing.designatorPrefix);designatorPrefix->setObjectName("designatorPrefix");designatorPrefix->setMaxLength(256);b->addWidget(designatorPrefix,2,2);
        b->setColumnMinimumWidth(0,18);b->setColumnStretch(2,1);
        auto *example=new QLabel;example->setObjectName("designatorExample");example->setWordWrap(true);b->addWidget(example,3,1,1,2);
        auto prefixState=[this,prefixLabel,example]{
            const bool on=designatorPageNumbers->isChecked();prefixLabel->setEnabled(on);designatorPrefix->setEnabled(on);example->setEnabled(on);
            example->setText(ui("Bezeichner R1 auf Blatt 2 erscheint als: %1").arg(on?designatorPrefix->text()+QStringLiteral("2R1"):QStringLiteral("R1")));};
        connect(designatorPageNumbers,&QCheckBox::toggled,this,prefixState);connect(designatorPrefix,&QLineEdit::textChanged,this,prefixState);prefixState();
        auto *keyRow=new QHBoxLayout;l->addLayout(keyRow);
        componentTextsWithKey=new QCheckBox(ui("Bauteiltexte nur mit gedrückter Zusatztaste verschiebbar:"));componentTextsWithKey->setObjectName("componentTextsWithKey");
        componentTextsWithKey->setChecked(g.componentTextsWithKey);keyRow->addWidget(componentTextsWithKey);
        componentTextKey=new QComboBox;componentTextKey->setObjectName("componentTextKey");
        for(auto m:componentTextKeys())componentTextKey->addItem(modifierName(m),m.toInt());
        componentTextKey->setCurrentIndex(std::max(0,componentTextKey->findData(g.componentTextKey.toInt())));keyRow->addWidget(componentTextKey);keyRow->addStretch();
        componentTextKey->setEnabled(g.componentTextsWithKey);connect(componentTextsWithKey,&QCheckBox::toggled,componentTextKey,&QWidget::setEnabled);
        auto *keyHint=new QLabel(ui("Ohne Zusatztaste: Rechtsklick auf den Text, dann „Bauteiltext verschieben“; der Text folgt dem Mauszeiger bis zum nächsten Klick."));
        keyHint->setWordWrap(true);l->addWidget(keyHint);l->addStretch();
    }
    {   // Anzeige as in sPlan; scaling and the graphics engine are the system's, so only the background stays.
        auto *l=page(ui("Anzeige"));whiteBackground=new QCheckBox(ui("Weißer Hintergrund"));whiteBackground->setObjectName("whiteBackground");
        whiteBackground->setChecked(g.whiteBackground);l->addWidget(whiteBackground);
        auto *hint=new QLabel(ui("Das Blatt erscheint dann in reinem Weiß statt leicht eingefärbt."));hint->setWordWrap(true);l->addWidget(hint);l->addStretch();
    }
    {   // Verzeichnisse
        auto *l=page(ui("Verzeichnisse"));l->addWidget(new QLabel(ui("Arbeitsverzeichnisse:")));
        {auto *work=new QGridLayout;l->addLayout(work);
            drawingFolder=folderRow(work,0,ui("Zeichnungen:"),g.drawingFolder,"drawingFolder");formWorkFolder=folderRow(work,1,ui("Formblätter:"),g.formWorkFolder,"formWorkFolder");
            exportFolder=folderRow(work,2,ui("Grafik-Export:"),g.exportFolder,"exportFolder");
            for(auto *e:{drawingFolder,formWorkFolder,exportFolder}){e->setReadOnly(false);e->setClearButtonEnabled(true);}
            auto *hint=new QLabel(ui("Bleiben die Felder leer, beginnen die Dateidialoge in den jeweils zuletzt benutzten Verzeichnissen."));
            hint->setWordWrap(true);l->addWidget(hint);}
        l->addWidget(new QLabel(ui("Feste Verzeichnisse:")));
        auto *grid=new QGridLayout;l->addLayout(grid);
        templates=folderRow(grid,0,ui("Vorlagen:"),g.templateFolder,"templateFolder");
        forms=folderRow(grid,1,ui("Formblätter:"),g.formFolder,"formFolder");
        auto *standard=new QPushButton(ui("Standardwerte setzen"));standard->setObjectName("resetFolders");l->addWidget(standard,0,Qt::AlignLeft);
        connect(standard,&QPushButton::clicked,this,[this]{templates->setText(QDir::toNativeSeparators(standardTemplateFolder()));forms->setText(QDir::toNativeSeparators(standardFormFolder()));});
        l->addStretch();
    }
    {   // Bibliothek
        auto *l=page(ui("Bibliothek"));l->addWidget(new QLabel(ui("Bibliothek-Stammverzeichnis:")));
        folder=new QLineEdit(QDir::toNativeSeparators(g.libraryFolder));folder->setObjectName("libraryFolder");folder->setReadOnly(true);l->addWidget(folder);
        auto *row=new QHBoxLayout;l->addLayout(row);
        auto *change=new QPushButton(ui("Ändern..."));change->setObjectName("changeFolder");row->addWidget(change);
        auto *reset=new QPushButton(ui("Reset"));reset->setObjectName("resetFolder");reset->setToolTip(ui("Hiermit setzen Sie das Bibliotheks-Verzeichnis wieder auf die Standard-Einstellung zurück."));row->addWidget(reset);
        auto *show=new QPushButton(ui("Explorer"));show->setObjectName("showFolder");show->setToolTip(ui("Öffnet das Bibliotheks-Verzeichnis in einem Dateifenster."));row->addWidget(show);row->addStretch();
        auto *hint=new QLabel(ui("Ändern Sie diese Einstellung nur, wenn Sie Ihre Bibliothek auf ein anderes Laufwerk oder Verzeichnis ausgelagert haben."));hint->setWordWrap(true);l->addWidget(hint);
        connect(change,&QPushButton::clicked,this,[this]{const QString d=QFileDialog::getExistingDirectory(this,ui("Verzeichnis wählen"),libraryFolder());if(!d.isEmpty())folder->setText(QDir::toNativeSeparators(d));});
        connect(reset,&QPushButton::clicked,this,[this]{folder->setText(QDir::toNativeSeparators(standardLibraryFolder()));});
        connect(show,&QPushButton::clicked,this,[this]{QDir().mkpath(libraryFolder());QDesktopServices::openUrl(QUrl::fromLocalFile(libraryFolder()));});
        l->addStretch();
    }
    {   // Raster
        auto *l=page(ui("Raster"));auto *form=new QFormLayout;l->addLayout(form);
        gridContrast=new QComboBox;gridContrast->setObjectName("gridContrast");
        for(auto [text,value]:{std::pair{ui("Unsichtbar"),0},std::pair{ui("Leicht"),35},std::pair{ui("Mittel"),65},std::pair{ui("Stark"),100}})gridContrast->addItem(text,value);
        {int best=0;for(int k=1;k<gridContrast->count();k++)if(std::abs(gridContrast->itemData(k).toInt()-g.gridContrast)<std::abs(gridContrast->itemData(best).toInt()-g.gridContrast))best=k;gridContrast->setCurrentIndex(best);}
        form->addRow(ui("Kontrast:"),gridContrast);
        gridMarks=new QSpinBox;gridMarks->setObjectName("gridMarks");gridMarks->setRange(1,100);gridMarks->setValue(std::max(1,g.gridMarks));gridMarks->setSpecialValueText(ui("Aus"));form->addRow(ui("Rastermarkierung:"),gridMarks);
        gridDots=new QRadioButton(ui("Punktraster"));gridDots->setObjectName("gridDots");gridLines=new QRadioButton(ui("Linienraster"));gridLines->setObjectName("gridLines");
        (g.gridLines?gridLines:gridDots)->setChecked(true);form->addRow(QString(),gridDots);form->addRow(QString(),gridLines);
        gridOverTitleBlock=new QCheckBox(ui("Raster ÜBER der Formblatt-Ebene darstellen"));gridOverTitleBlock->setObjectName("gridOverTitleBlock");gridOverTitleBlock->setChecked(g.gridOverTitleBlock);
        l->addWidget(gridOverTitleBlock);l->addStretch();
    }
    {   // Autospeichern
        auto *l=page(ui("Autospeichern"));
        auto *text=new QLabel(ui("Mit dieser Funktion wird Ihr Schaltplan in regelmäßigen Abständen automatisch mit der Endung *.BAK gespeichert."));text->setWordWrap(true);l->addWidget(text);
        auto *form=new QFormLayout;l->addLayout(form);
        autosaveMinutes=new QSpinBox;autosaveMinutes->setObjectName("autosaveMinutes");autosaveMinutes->setRange(0,999);autosaveMinutes->setSuffix(QStringLiteral(" min"));
        autosaveMinutes->setSpecialValueText(ui("Aus"));autosaveMinutes->setValue(g.autosaveMinutes);form->addRow(ui("Speicherintervall:"),autosaveMinutes);
        backupOnOpen=new QCheckBox(ui("Datei beim Öffnen automatisch sichern."));backupOnOpen->setObjectName("backupOnOpen");backupOnOpen->setChecked(g.backupOnOpen);l->addWidget(backupOnOpen);
        l->addWidget(new QLabel(ui("Die Sicherungsdatei beginnt mit \"Backup_of_\"")));l->addStretch();
    }
    {   // Neues Blatt
        auto *l=page(ui("Neues Blatt"));l->addWidget(new QLabel(ui("Vorgaben für ein neues Blatt:")));auto *form=new QFormLayout;l->addLayout(form);
        auto box=[&](double v,double low,double high,const char *name){auto *b=new QDoubleSpinBox;b->setObjectName(name);b->setRange(low,high);b->setDecimals(2);b->setSuffix(QStringLiteral(" mm"));b->setValue(v);return b;};
        // The format as in sPlan ("Frei", A0–A5, B4, B5, C3–C5, then Letter and Legal) sets width and height in the
        // chosen orientation; other sizes show as "Frei".
        auto *format=new QComboBox;format->setObjectName("newSheetFormat");format->addItem(ui("Frei"));
        for(const auto &f:paperFormats())format->addItem(QString::fromLatin1(f.name));
        format->setCurrentIndex(paperFormatOf(g.newSheetWidth,g.newSheetHeight)+1);form->addRow(ui("Format:"),format);
        newSheetWidth=box(g.newSheetWidth,10,10000,"newSheetWidth");form->addRow(ui("Breite:"),newSheetWidth);
        newSheetHeight=box(g.newSheetHeight,10,10000,"newSheetHeight");form->addRow(ui("Höhe:"),newSheetHeight);
        connect(format,&QComboBox::activated,this,[this](int i){
            if(i<1||i>int(paperFormats().size()))return;const auto &f=paperFormats()[i-1];const bool upright=newSheetHeight->value()>newSheetWidth->value();
            newSheetWidth->setValue(upright?f.height:f.width);newSheetHeight->setValue(upright?f.width:f.height);});
        auto follow=[this,format]{const QSignalBlocker quiet(format);format->setCurrentIndex(paperFormatOf(newSheetWidth->value(),newSheetHeight->value())+1);};
        connect(newSheetWidth,&QDoubleSpinBox::valueChanged,this,follow);connect(newSheetHeight,&QDoubleSpinBox::valueChanged,this,follow);
        landscape=new QRadioButton(ui("Querformat"));landscape->setObjectName("landscape");portrait=new QRadioButton(ui("Hochformat"));portrait->setObjectName("portrait");
        (g.newSheetWidth>=g.newSheetHeight?landscape:portrait)->setChecked(true);form->addRow(QString(),landscape);form->addRow(QString(),portrait);
        // The orientation turns the sheet: width and height change places.
        auto turn=[this](bool wide){const double w=newSheetWidth->value(),h=newSheetHeight->value();if((w>=h)!=wide){newSheetWidth->setValue(h);newSheetHeight->setValue(w);}};
        connect(landscape,&QRadioButton::toggled,this,[turn](bool on){if(on)turn(true);});
        connect(portrait,&QRadioButton::toggled,this,[turn](bool on){if(on)turn(false);});
        newSheetGrid=box(g.newSheetGrid,.01,100,"newSheetGrid");newSheetGrid->setDecimals(3);form->addRow(ui("Raster:"),newSheetGrid);
        auto *row=new QHBoxLayout;newSheetForm=new QLineEdit(QDir::toNativeSeparators(g.newSheetForm));newSheetForm->setObjectName("newSheetForm");newSheetForm->setReadOnly(true);row->addWidget(newSheetForm,1);
        auto *choose=new QPushButton(ui("Ändern..."));choose->setObjectName("newSheetFormChange");row->addWidget(choose);
        auto *none=new QPushButton(ui("Ohne"));none->setObjectName("newSheetFormNone");row->addWidget(none);form->addRow(ui("Formblatt:"),row);
        connect(choose,&QPushButton::clicked,this,[this]{
            const QString f=QFileDialog::getOpenFileName(this,ui("Formblatt laden"),QDir::fromNativeSeparators(forms->text()),ui("sPlan-Formblätter (*.sbk);;Alle Dateien (*)"));
            if(!f.isEmpty())newSheetForm->setText(QDir::toNativeSeparators(f));});
        connect(none,&QPushButton::clicked,newSheetForm,&QLineEdit::clear);
        l->addStretch();
    }
    {   // Hotkeys
        auto *l=page(ui("Hotkeys"));
        hotkeys=new QTableWidget(int(modes.size()),2);hotkeys->setObjectName("hotkeys");hotkeys->setHorizontalHeaderLabels({ui("Modus"),ui("Hotkey")});
        hotkeys->horizontalHeader()->setStretchLastSection(true);hotkeys->verticalHeader()->hide();
        for(int r=0;r<modes.size();r++){
            auto *name=new QTableWidgetItem(modes[r].label);name->setFlags(Qt::ItemIsEnabled);hotkeys->setItem(r,0,name);
            auto *key=new QKeySequenceEdit(g.hotkeys.contains(modes[r].action)?QKeySequence(g.hotkeys[modes[r].action]):modes[r].standard);
            key->setObjectName(modes[r].action);hotkeys->setCellWidget(r,1,key);
        }
        hotkeys->resizeColumnToContents(0);l->addWidget(hotkeys,1);
        auto *standard=new QPushButton(ui("Standardwerte setzen"));standard->setObjectName("resetHotkeys");l->addWidget(standard,0,Qt::AlignLeft);
        connect(standard,&QPushButton::clicked,this,[this]{for(int r=0;r<this->modes.size();r++)static_cast<QKeySequenceEdit*>(hotkeys->cellWidget(r,1))->setKeySequence(this->modes[r].standard);});
    }
    connect(pages,&QListWidget::currentRowChanged,stack,&QStackedWidget::setCurrentIndex);pages->setCurrentRow(0);
    layout->addWidget(okCancel(this));resize(620,360);
}
QString SettingsDialog::libraryFolder() const{return QDir::fromNativeSeparators(folder->text());}
DrawingSettings SettingsDialog::drawing() const{
    return {sheetNumbers->isChecked(),designatorPageNumbers->isChecked(),designatorPrefix->text()};
}
GeneralSettings SettingsDialog::settings() const{
    GeneralSettings g;g.libraryFolder=libraryFolder();
    g.drawingFolder=QDir::fromNativeSeparators(drawingFolder->text().trimmed());g.formWorkFolder=QDir::fromNativeSeparators(formWorkFolder->text().trimmed());
    g.exportFolder=QDir::fromNativeSeparators(exportFolder->text().trimmed());g.templateFolder=QDir::fromNativeSeparators(templates->text());g.formFolder=QDir::fromNativeSeparators(forms->text());
    g.componentTextsWithKey=componentTextsWithKey->isChecked();g.componentTextKey=Qt::KeyboardModifiers::fromInt(componentTextKey->currentData().toInt());
    g.gridContrast=gridContrast->currentData().toInt();g.gridMarks=gridMarks->value()<2?0:gridMarks->value();g.whiteBackground=whiteBackground->isChecked();
    g.autosaveMinutes=autosaveMinutes->value();g.backupOnOpen=backupOnOpen->isChecked();
    g.newSheetWidth=newSheetWidth->value();g.newSheetHeight=newSheetHeight->value();g.newSheetGrid=newSheetGrid->value();g.newSheetForm=QDir::fromNativeSeparators(newSheetForm->text());
    for(int r=0;r<modes.size();r++){
        const QKeySequence k=static_cast<QKeySequenceEdit*>(hotkeys->cellWidget(r,1))->keySequence();
        if(k!=modes[r].standard)g.hotkeys.insert(modes[r].action,k.toString(QKeySequence::PortableText));
    }g.gridLines=gridLines->isChecked();g.gridOverTitleBlock=gridOverTitleBlock->isChecked();
    return g;
}

ChildListDialog::ChildListDialog(const Document &d,const QString &parentId,const QString &fileName,QWidget *parent)
    :QDialog(parent),document(d),parentId(parentId),fileName(fileName){
    setWindowTitle(ui("Childliste (Kontaktspiegel)"));auto *layout=new QVBoxLayout(this);
    auto *top=new QHBoxLayout;layout->addLayout(top);
    auto *fontBox=new QGroupBox(ui("Schriftart"));auto *fl=new QVBoxLayout(fontBox);top->addWidget(fontBox);
    family=new QFontComboBox;family->setObjectName("family");family->setCurrentFont(QFont(QStringLiteral("Arial")));fl->addWidget(family);
    auto *sizeRow=new QHBoxLayout;size=new QSpinBox;size->setObjectName("size");size->setRange(6,96);size->setValue(16);sizeRow->addWidget(size);
    sizeRow->addWidget(new QLabel(ui("[1/10 mm]")));sizeRow->addStretch();fl->addLayout(sizeRow);
    auto *optionBox=new QGroupBox(ui("Optionen"));auto *ol=new QGridLayout(optionBox);top->addWidget(optionBox);
    frame=new QCheckBox(ui("Rahmen"));frame->setObjectName("frame");frame->setChecked(true);ol->addWidget(frame,0,0);
    shadow=new QCheckBox(ui("Schatten"));shadow->setObjectName("shadow");ol->addWidget(shadow,0,1);
    verticalLines=new QCheckBox(ui("Senkrechte Trennlinien"));verticalLines->setObjectName("verticalLines");verticalLines->setChecked(true);ol->addWidget(verticalLines,1,0,1,2);
    alternate=new QCheckBox(ui("Zeilen alternierend"));alternate->setObjectName("alternate");ol->addWidget(alternate,2,0,1,2);
    auto *dataBox=new QGroupBox(ui("Daten"));auto *dl=new QGridLayout(dataBox);layout->addWidget(dataBox);
    const ChildListOptions defaults;
    for(int k=0;k<=int(ChildColumn::Reference);k++){
        auto *c=new QCheckBox(childColumnName(ChildColumn(k)));c->setObjectName(QStringLiteral("column%1").arg(k));c->setChecked(defaults.columns.contains(ChildColumn(k)));
        dl->addWidget(c,k,0);columns<<c;
    }
    auto *hint=new QLabel(ui("Reihenfolge per Drag&Drop festlegen"));hint->setStyleSheet(QStringLiteral("color:navy"));hint->setWordWrap(true);dl->addWidget(hint,0,1);
    order=new QListWidget;order->setObjectName("order");order->setDragDropMode(QAbstractItemView::InternalMove);dl->addWidget(order,1,1,7,1);
    for(auto c:defaults.columns){auto *i=new QListWidgetItem(childColumnName(c),order);i->setData(Qt::UserRole,int(c));}
    dl->addWidget(new QLabel(ui("Zeile:")),1,2);
    rowFormat=new QComboBox;rowFormat->setObjectName("rowFormat");rowFormat->addItems({ui("NUM (1,2,3,...)"),ui("CHAR (A,B,C,...)")});rowFormat->setCurrentIndex(1);dl->addWidget(rowFormat,2,2);
    dl->addWidget(new QLabel(ui("Spalte:")),3,2);
    columnFormat=new QComboBox;columnFormat->setObjectName("columnFormat");columnFormat->addItems({ui("NUM (1,2,3,...)"),ui("CHAR (A,B,C,...)")});dl->addWidget(columnFormat,4,2);
    slash=new QCheckBox(ui("Blatt.Spalte mit \"/\""));slash->setObjectName("slash");dl->addWidget(slash,6,2);
    layout->addWidget(new QLabel(ui("Vorschau:")));
    preview=new QPlainTextEdit;preview->setObjectName("preview");preview->setReadOnly(true);preview->setLineWrapMode(QPlainTextEdit::NoWrap);preview->setMaximumHeight(110);layout->addWidget(preview);
    auto *buttons=new QDialogButtonBox;layout->addWidget(buttons);
    auto *insert=buttons->addButton(ui("Einfügen..."),QDialogButtonBox::AcceptRole);insert->setObjectName("insert");insert->setDefault(true);
    buttons->addButton(ui("Abbrechen"),QDialogButtonBox::RejectRole);
    connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    // A checked column comes last in the order; an unchecked one leaves it.
    for(int k=0;k<columns.size();k++)connect(columns[k],&QCheckBox::toggled,this,[this,k](bool on){
        for(int r=order->count()-1;r>=0;r--)if(order->item(r)->data(Qt::UserRole).toInt()==k)delete order->takeItem(r);
        if(on){auto *i=new QListWidgetItem(childColumnName(ChildColumn(k)),order);i->setData(Qt::UserRole,k);}
        updatePreview();});
    connect(order->model(),&QAbstractItemModel::rowsMoved,this,[this]{updatePreview();});
    for(auto *c:{rowFormat,columnFormat})connect(c,&QComboBox::currentIndexChanged,this,[this]{updatePreview();});
    connect(slash,&QCheckBox::toggled,this,[this]{updatePreview();});
    updatePreview();
}
ChildListOptions ChildListDialog::options() const{
    ChildListOptions o;o.columns.clear();
    for(int r=0;r<order->count();r++)o.columns<<ChildColumn(order->item(r)->data(Qt::UserRole).toInt());
    o.rowLetters=rowFormat->currentIndex()==1;o.columnLetters=columnFormat->currentIndex()==1;o.slash=slash->isChecked();
    return o;
}
PartsDrawing ChildListDialog::drawing() const{
    PartsDrawing d;d.height=size->value()/10.;d.family=family->currentFont().family();d.frame=frame->isChecked();d.shadow=shadow->isChecked();
    d.verticalLines=verticalLines->isChecked();d.alternate=alternate->isChecked();d.horizontalLines=false;d.header=false;
    return d;
}
PartsTable ChildListDialog::table() const{return childList(document,parentId,options(),fileName);}
void ChildListDialog::updatePreview(){
    const PartsTable t=table();QStringList lines;for(const auto &r:t.rows)lines<<r.join(QStringLiteral("  |  "));
    preview->setPlainText(lines.join(u'\n'));
}
BitmapExplorerDialog::BitmapExplorerDialog(QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Bitmap-Explorer"));resize(560,480);
    auto *layout=new QHBoxLayout(this);
    list=new QTreeWidget;list->setObjectName("bitmaps");list->setHeaderHidden(true);layout->addWidget(list,1);
    auto *side=new QVBoxLayout;layout->addLayout(side);
    auto *sorting=new QGroupBox(ui("Sortierung"));auto *sl=new QVBoxLayout(sorting);side->addWidget(sorting);
    bySheet=new QRadioButton(ui("Blatt"));bySheet->setObjectName("bySheet");bySheet->setChecked(true);sl->addWidget(bySheet);
    byMemory=new QRadioButton(ui("Speicherbedarf"));byMemory->setObjectName("byMemory");sl->addWidget(byMemory);
    byDpi=new QRadioButton(ui("DPI"));byDpi->setObjectName("byDpi");sl->addWidget(byDpi);
    for(auto *r:{bySheet,byMemory,byDpi})connect(r,&QRadioButton::toggled,this,[this](bool on){if(on)refresh();});
    auto *most=new QGroupBox(ui("Maximale Auflösung für Bitmaps"));auto *ml=new QVBoxLayout(most);side->addWidget(most);
    auto *row=new QHBoxLayout;ml->addLayout(row);row->addWidget(new QLabel(ui("DPI:")));
    maximum=new QComboBox;maximum->setObjectName("maximum");for(int v:{100,150,200,250,300,400,500,600,700,800,900})maximum->addItem(QString::number(v),v);
    maximum->setCurrentIndex(4);row->addWidget(maximum);row->addStretch();connect(maximum,&QComboBox::currentIndexChanged,this,[this]{refresh();});
    auto *one=new QPushButton(ui(">  Auf gewählte Bitmap anwenden"));one->setObjectName("applySelected");ml->addWidget(one);
    auto *every=new QPushButton(ui(">>>  Auf alle Bitmaps anwenden"));every->setObjectName("applyAll");ml->addWidget(every);
    connect(one,&QPushButton::clicked,this,[this]{applyMaximum(false);});
    connect(every,&QPushButton::clicked,this,[this]{
        const int n=applyMaximum(true);
        QMessageBox::information(this,ui("Bitmap-Explorer"),n?ui("%1 Bitmaps angepasst.").arg(n):ui("Keine Bitmaps mit höherer DPI als das Maximum vorhanden."));});
    auto *total=new QGroupBox(ui("Speicherbedarf insgesamt"));auto *tf=new QFormLayout(total);side->addWidget(total);
    count=new QLabel;count->setObjectName("count");memory=new QLabel;memory->setObjectName("memory");
    for(auto *l:{count,memory})l->setStyleSheet(QStringLiteral("color:#0000ff"));
    tf->addRow(ui("Bitmaps:"),count);tf->addRow(ui("Speicherbedarf:"),memory);
    side->addStretch();
    auto *close=new QDialogButtonBox(QDialogButtonBox::Close);side->addWidget(close);connect(close,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(list,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem *item){
        if(item&&showImage&&!item->data(0,Qt::UserRole).toString().isEmpty())showImage(item->data(0,Qt::UserRole+1).toInt(),item->data(0,Qt::UserRole).toString());});
}
void BitmapExplorerDialog::refresh(){
    const Document *d=document?document():nullptr;list->clear();if(!d)return;
    const QString chosen=list->currentItem()?list->currentItem()->data(0,Qt::UserRole).toString():QString();
    struct Row {int sheet;QString id;ImageInfo info;};
    QList<Row> rows;qint64 bytes=0;QSet<QString> counted;
    for(const auto &p:placedImages(*d)){
        const Item *i=imageWithId(*d,p.sheet,p.id);if(!i)continue;
        rows.append({p.sheet,p.id,imageInfo(*d,*i)});
        if(!counted.contains(i->resource)){counted.insert(i->resource);bytes+=rows.last().info.bytes;}
    }
    if(byMemory->isChecked())std::stable_sort(rows.begin(),rows.end(),[](const Row &a,const Row &b){return a.info.bytes>b.info.bytes;});
    if(byDpi->isChecked())std::stable_sort(rows.begin(),rows.end(),[](const Row &a,const Row &b){return a.info.dpi>b.info.dpi;});
    const double most=maximum->currentData().toDouble();
    QHash<int,QTreeWidgetItem*> sheets;
    for(const auto &r:rows){
        QTreeWidgetItem *parent=nullptr;
        if(bySheet->isChecked()){QTreeWidgetItem *&s=sheets[r.sheet];if(!s)s=new QTreeWidgetItem(list,{QStringLiteral("%1: %2").arg(r.sheet+1).arg(d->sheets[r.sheet].name)});parent=s;}
        const QString text=ui("%1 × %2 Pixel, %3 dpi, %4 KB").arg(r.info.pixels.width()).arg(r.info.pixels.height()).arg(std::lround(r.info.dpi)).arg(uiLocale().toString(r.info.bytes/1024.,'f',1));
        auto *item=parent?new QTreeWidgetItem(parent,{text}):new QTreeWidgetItem(list,{QStringLiteral("%1: ").arg(r.sheet+1)+text});
        item->setData(0,Qt::UserRole,r.id);item->setData(0,Qt::UserRole+1,r.sheet);
        if(r.info.dpi>most)item->setForeground(0,QColor(220,0,0));
        if(r.id==chosen)list->setCurrentItem(item);
    }
    list->expandAll();
    count->setText(QString::number(rows.size()));memory->setText(ui("%1 KB").arg(uiLocale().toString(bytes/1024.,'f',1)));
}
int BitmapExplorerDialog::applyMaximum(bool all){
    if(!change||!document)return 0;
    const double most=maximum->currentData().toDouble();
    QList<PlacedImage> chosen;
    if(all)chosen=placedImages(*document());
    else if(auto *item=list->currentItem();item&&!item->data(0,Qt::UserRole).toString().isEmpty())chosen.append({item->data(0,Qt::UserRole+1).toInt(),item->data(0,Qt::UserRole).toString()});
    int changed=0;
    {   // Only those above the maximum.
        const Document &d=*document();QList<PlacedImage> above;
        for(const auto &p:chosen)if(const Item *i=imageWithId(d,p.sheet,p.id);i&&imageInfo(d,*i).dpi>most+.5)above<<p;
        chosen=above;
    }
    if(chosen.isEmpty()){refresh();return 0;}
    change([&](Document &d){
        for(const auto &p:chosen)if(Item *i=imageWithId(d,p.sheet,p.id)){const double dpi=imageInfo(d,*i).dpi;if(dpi>most&&reduceResolution(d,*i,dpi/most))changed++;}
    });
    refresh();return changed;
}
SpecialShapeDialog::SpecialShapeDialog(const SpecialShapeOptions &o,int page,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Spezialformen"));auto *layout=new QVBoxLayout(this);
    pages=new QTabWidget;pages->setObjectName("pages");layout->addWidget(pages);
    auto spin=[](const char *name,int low,int high,int value){auto *s=new QSpinBox;s->setObjectName(name);s->setRange(low,high);s->setValue(value);return s;};
    auto choice=[](QWidget *page,QFormLayout *form,QRadioButton *&line,QRadioButton *&polygon,bool asLine){
        line=new QRadioButton(ui("Als Linie"));polygon=new QRadioButton(ui("Als Polygon"));(asLine?line:polygon)->setChecked(true);
        auto *group=new QVBoxLayout;group->addWidget(line);group->addWidget(polygon);form->addRow(QString(),group);(void)page;
    };
    {auto *p=new QWidget;auto *f=new QFormLayout(p);
        corners=spin("corners",3,360,o.corners);f->addRow(ui("Ecken:"),corners);
        polygonOffset=spin("polygonOffset",0,360,int(std::lround(o.polygonOffset)));f->addRow(ui("Winkeloffset:"),polygonOffset);
        choice(p,f,polygonAsLine,polygonAsPolygon,o.polygonAsLine);polygonAsLine->setObjectName("polygonAsLine");pages->addTab(p,ui("Vieleck"));}
    {auto *p=new QWidget;auto *f=new QFormLayout(p);
        spikes=spin("spikes",3,360,o.spikes);f->addRow(ui("Zacken:"),spikes);
        spikeDepth=spin("spikeDepth",0,100,int(std::lround(o.spikeDepth)));spikeDepth->setSuffix(QStringLiteral(" %"));f->addRow(ui("Zackentiefe:"),spikeDepth);
        starOffset=spin("starOffset",0,360,int(std::lround(o.starOffset)));f->addRow(ui("Winkeloffset:"),starOffset);
        choice(p,f,starAsLine,starAsPolygon,o.starAsLine);starAsLine->setObjectName("starAsLine");pages->addTab(p,ui("Stern"));}
    {auto *p=new QWidget;auto *f=new QFormLayout(p);
        columns=spin("columns",1,50,o.columns);f->addRow(ui("Spalten:"),columns);
        rows=spin("rows",1,50,o.rows);f->addRow(ui("Zeilen:"),rows);
        frame=new QCheckBox(ui("Rahmen"));frame->setObjectName("frame");frame->setChecked(o.frame);f->addRow(QString(),frame);
        textFields=new QCheckBox(ui("Textfelder hinterlegen"));textFields->setObjectName("textFields");textFields->setChecked(o.textFields);f->addRow(QString(),textFields);
        textHeight=spin("textHeight",5,100,int(std::lround(o.textHeight)));textHeight->setSuffix(QStringLiteral(" ")+ui("% der Zellenhöhe"));textHeight->setEnabled(o.textFields);
        connect(textFields,&QCheckBox::toggled,textHeight,&QWidget::setEnabled);f->addRow(ui("Texthöhe:"),textHeight);pages->addTab(p,ui("Gitter"));}
    {auto *p=new QWidget;auto *f=new QFormLayout(p);
        wave=new QComboBox;wave->setObjectName("wave");wave->addItems({ui("Sinus"),ui("Rechteck"),ui("Trapez"),ui("Dreieck"),ui("Sägezahn")});wave->setCurrentIndex(int(o.wave));f->addRow(wave);
        waves=spin("waves",1,20,o.waves);f->addRow(ui("Schwingungen:"),waves);pages->addTab(p,ui("Schwingung"));}
    pages->setCurrentIndex(std::clamp(page,0,3));
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
}
SpecialShapeOptions SpecialShapeDialog::options() const{
    SpecialShapeOptions o;
    o.corners=corners->value();o.polygonOffset=polygonOffset->value();o.polygonAsLine=polygonAsLine->isChecked();
    o.spikes=spikes->value();o.spikeDepth=spikeDepth->value();o.starOffset=starOffset->value();o.starAsLine=starAsLine->isChecked();
    o.columns=columns->value();o.rows=rows->value();o.frame=frame->isChecked();o.textFields=textFields->isChecked();o.textHeight=textHeight->value();
    o.wave=WaveKind(wave->currentIndex());o.waves=waves->value();
    return o;
}
LetteringDialog::LetteringDialog(bool hasSelection,bool hasLibraryPage,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Bauteilbeschriftung"));auto *layout=new QVBoxLayout(this);
    const QStringList titles{ui("Bezeichner"),ui("Wert"),ui("Kontakt")};
    for(int k=0;k<3;k++){
        auto *box=new QGroupBox(titles[k]);auto *form=new QFormLayout(box);layout->addWidget(box);
        auto *on=new QCheckBox(ui("Ändern"));on->setObjectName(QStringLiteral("use%1").arg(k));on->setChecked(k<2);form->addRow(on);use<<on;
        auto *f=new QFontComboBox;f->setObjectName(QStringLiteral("family%1").arg(k));f->setCurrentFont(QFont(QStringLiteral("Arial")));form->addRow(ui("Schriftart:"),f);family<<f;
        auto *h=new QDoubleSpinBox;h->setObjectName(QStringLiteral("height%1").arg(k));h->setRange(.5,50);h->setValue(k<2?2.5:1.8);h->setSuffix(QStringLiteral(" mm"));h->setLocale(uiLocale());
        form->addRow(ui("Höhe:"),h);height<<h;
        auto *b=new QCheckBox(ui("Fett"));auto *i=new QCheckBox(ui("Kursiv"));auto *row=new QHBoxLayout;row->addWidget(b);row->addWidget(i);row->addStretch();form->addRow(row);bold<<b;italic<<i;
        // The colour of the group's texts.
        auto *c=new QToolButton;c->setObjectName(QStringLiteral("colour%1").arg(k));c->setFixedSize(40,20);form->addRow(ui("Farbe:"),c);colour<<c;colours<<QColor(0,0,0);
        connect(c,&QToolButton::clicked,this,[this,k]{const QColor chosen=QColorDialog::getColor(colours[k],this);if(chosen.isValid()){colours[k]=chosen.toRgb();showPreview();}});
        for(QWidget *w:QList<QWidget*>{f,h,b,i,c}){w->setEnabled(on->isChecked());connect(on,&QCheckBox::toggled,w,&QWidget::setEnabled);}
        connect(on,&QCheckBox::toggled,this,[this]{showPreview();});connect(f,&QFontComboBox::currentFontChanged,this,[this]{showPreview();});
        connect(h,&QDoubleSpinBox::valueChanged,this,[this]{showPreview();});connect(b,&QCheckBox::toggled,this,[this]{showPreview();});connect(i,&QCheckBox::toggled,this,[this]{showPreview();});
    }
    preview=new QLabel;preview->setObjectName("preview");preview->setAlignment(Qt::AlignCenter);preview->setFrameShape(QFrame::StyledPanel);layout->addWidget(preview);
    auto *scopeBox=new QGroupBox(ui("Anwenden auf"));auto *sl=new QVBoxLayout(scopeBox);layout->addWidget(scopeBox);
    project=new QRadioButton(ui("Gesamtes Projekt"));project->setObjectName("project");sl->addWidget(project);
    sheet=new QRadioButton(ui("Aktuelles Blatt"));sheet->setObjectName("sheet");sheet->setChecked(true);sl->addWidget(sheet);
    selection=new QRadioButton(ui("Markierte Bauteile"));selection->setObjectName("selection");selection->setEnabled(hasSelection);sl->addWidget(selection);
    libraryPage=new QRadioButton(ui("Bibliotheks-Seite"));libraryPage->setObjectName("libraryPage");libraryPage->setEnabled(hasLibraryPage);sl->addWidget(libraryPage);
    layout->addWidget(okCancel(this));
    showPreview();
}
Lettering LetteringDialog::lettering(int k) const{
    Lettering l;l.set=use[k]->isChecked();l.family=family[k]->currentFont().family();l.height=height[k]->value();l.bold=bold[k]->isChecked();l.italic=italic[k]->isChecked();
    l.color=colours.value(k,QColor(0,0,0));
    return l;
}
void LetteringDialog::start(int k,const Font &font){
    if(k<0||k>2)return;
    family[k]->setCurrentFont(QFont(font.family));height[k]->setValue(font.height);bold[k]->setChecked(font.bold);italic[k]->setChecked(font.italic);
    colours[k]=font.color;showPreview();
}
void LetteringDialog::showPreview(){
    if(!preview)return;
    for(int k=0;k<colour.size();k++)colour[k]->setStyleSheet(QStringLiteral("background:%1;border:1px solid #444").arg(colours[k].name()));
    // A resistor drawn at 4 pixels per millimetre with its designator, value and two contacts; unchanged groups in black.
    constexpr double scale=4;QPixmap picture(QSize(300,110));picture.fill(Qt::white);
    QPainter p(&picture);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(QPen(Qt::black,1.5));p.drawLine(QPointF(40,60),QPointF(110,60));p.drawLine(QPointF(190,60),QPointF(260,60));p.setBrush(Qt::NoBrush);p.drawRect(QRectF(110,48,80,24));
    auto text=[&](int k,const QString &t,QPointF at,Qt::Alignment align){
        QFont f(QStringLiteral("Arial"));double h=k<2?2.5:1.8;QColor c(0,0,0);
        if(use[k]->isChecked()){f=family[k]->currentFont();h=height[k]->value();f.setBold(bold[k]->isChecked());f.setItalic(italic[k]->isChecked());c=colours[k];}
        f.setPixelSize(std::max(1,int(std::lround(h*scale))));p.setFont(f);p.setPen(c);
        const QRectF r(at.x()-100,at.y()-50,200,100);p.drawText(r,int(align),t);};
    text(0,QStringLiteral("R1"),QPointF(150,22),Qt::AlignHCenter|Qt::AlignVCenter);
    text(1,QStringLiteral("4k7"),QPointF(150,90),Qt::AlignHCenter|Qt::AlignVCenter);
    text(2,QStringLiteral("1"),QPointF(70,50),Qt::AlignHCenter|Qt::AlignVCenter);
    text(2,QStringLiteral("2"),QPointF(230,50),Qt::AlignHCenter|Qt::AlignVCenter);
    p.end();preview->setPixmap(picture);
}
LetteringDialog::Scope LetteringDialog::scope() const{
    return project->isChecked()?Project:selection->isChecked()?Selection:libraryPage->isChecked()?LibraryPage:Sheet;
}

TitleBlockDialog::TitleBlockDialog(const TitleBlock &t,double width,double height,QWidget *parent):QDialog(parent),current(t),sheetWidth(width),sheetHeight(height){
    setWindowTitle(ui("Formblatt generieren"));setObjectName("titleBlockDialog");auto *layout=new QVBoxLayout(this);
    auto *frame=new QGroupBox(ui("Rahmen"));auto *ff=new QFormLayout(frame);layout->addWidget(frame);
    auto side=[&](const char *name,int choice){auto *b=new QComboBox;b->setObjectName(name);b->addItems({QStringLiteral("---"),QStringLiteral("NUM (1,2,3,...)"),QStringLiteral("CHAR (A,B,C,...)")});b->setCurrentIndex(choice);return b;};
    top=side("labelsTop",1);bottom=side("labelsBottom",0);left=side("labelsLeft",2);right=side("labelsRight",0);
    ff->addRow(ui("Oben:"),top);ff->addRow(ui("Unten:"),bottom);ff->addRow(ui("Links:"),left);ff->addRow(ui("Rechts:"),right);
    auto *letters=new QGroupBox(ui("Schriftart"));auto *lf=new QHBoxLayout(letters);layout->addWidget(letters);
    font=new QFontComboBox;font->setObjectName("titleFont");font->setCurrentFont(QFont(QStringLiteral("Arial")));lf->addWidget(font,1);
    size=new QSpinBox;size->setObjectName("titleTextSize");size->setRange(6,96);size->setValue(40);lf->addWidget(size);lf->addWidget(new QLabel(QStringLiteral("[1/10 mm]")));
    field=new QCheckBox(ui("Schriftfeld mit Blattname und Blattnummer"));field->setObjectName("field");field->setChecked(true);layout->addWidget(field);
    auto *buttons=new QDialogButtonBox;buttons->addButton(ui("Erstellen"),QDialogButtonBox::AcceptRole);buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);layout->addWidget(buttons);
}
TitleBlockStyle TitleBlockDialog::style() const{
    TitleBlockStyle st;auto kind=[](const QComboBox *b){return FrameLabels(std::clamp(b->currentIndex(),0,2));};
    st.top=kind(top);st.bottom=kind(bottom);st.left=kind(left);st.right=kind(right);
    st.font=font->currentFont().family();st.textHeight=size->value()/10.;st.field=field->isChecked();
    st.columnStart=current.columnStart;st.rowStart=current.rowStart;
    return st;
}
TitleBlock TitleBlockDialog::titleBlock() const{
    const QRectF frame=current.frame.isEmpty()?QRectF(10,10,std::max(1.,sheetWidth-20),std::max(1.,sheetHeight-20)):current.frame;
    TitleBlock t=generateTitleBlock(frame,current.columns,current.rows,style());t.showGrid=current.showGrid;
    return t;
}
ExportDialog::ExportDialog(QSizeF sheet,QSizeF elements,QSizeF selection,int sheets,QWidget *parent):QDialog(parent),sizes{sheet,elements,selection}{
    setWindowTitle(ui("Schaltplan exportieren"));auto *layout=new QGridLayout(this);
    // The choices of the last export in this session.
    static int lastFormat=Jpg,lastDpi=150;static bool lastBlack=false;
    auto *formatBox=new QGroupBox(ui("Dateiformat"));auto *fl=new QVBoxLayout(formatBox);layout->addWidget(formatBox,0,0);
    const char *names[]={"jpg","png","bmp","emf","svg","pdf"};const char *captions[]={"JPG","PNG","BMP","EMF","SVG","PDF"};
    for(int i=0;i<6;i++){formats[i]=new QRadioButton(QString::fromLatin1(captions[i]));formats[i]->setObjectName(QString::fromLatin1(names[i]));fl->addWidget(formats[i]);
        connect(formats[i],&QRadioButton::toggled,this,[this](bool on){if(on)update();});}
    fl->addStretch();
    auto *resolutionBox=new QGroupBox(ui("Auflösung"));auto *rl=new QGridLayout(resolutionBox);layout->addWidget(resolutionBox,0,1);
    resolution=new QSlider(Qt::Horizontal);resolution->setObjectName("resolution");resolution->setRange(20,300);resolution->setTickInterval(20);resolution->setTickPosition(QSlider::TicksBelow);
    rl->addWidget(resolution,0,0,1,2);
    original=new QLabel;original->setObjectName("original");rl->addWidget(new QLabel(ui("Originalgröße:")),1,0,Qt::AlignRight);rl->addWidget(original,1,1);
    dpi=new QSpinBox;dpi->setObjectName("dpi");dpi->setRange(20,2400);rl->addWidget(new QLabel(ui("Auflösung (dpi):")),2,0,Qt::AlignRight);rl->addWidget(dpi,2,1,Qt::AlignLeft);
    pixels=new QLabel;pixels->setObjectName("pixels");rl->addWidget(new QLabel(ui("Größe der Bitmap in Pixel:")),3,0,Qt::AlignRight);rl->addWidget(pixels,3,1);
    connect(resolution,&QSlider::valueChanged,this,[this](int v){if(dpi->value()!=v&&(v<300||dpi->value()<300))dpi->setValue(v);});
    connect(dpi,&QSpinBox::valueChanged,this,[this](int v){resolution->blockSignals(true);resolution->setValue(std::min(v,300));resolution->blockSignals(false);update();});
    auto *optionBox=new QGroupBox(ui("Optionen"));auto *ol=new QVBoxLayout(optionBox);layout->addWidget(optionBox,1,0);
    colour=new QRadioButton(ui("&Farbe"));colour->setObjectName("colour");blackAndWhite=new QRadioButton(ui("&S/W"));blackAndWhite->setObjectName("blackAndWhite");
    transparent=new QCheckBox(ui("Transparent"));transparent->setObjectName("transparent");
    ol->addWidget(blackAndWhite);ol->addWidget(colour);ol->addWidget(transparent);ol->addStretch();
    auto *areaBox=new QGroupBox(ui("Auswahl"));auto *al=new QVBoxLayout(areaBox);layout->addWidget(areaBox,1,1);
    currentSheet=new QRadioButton(ui("Aktuelles Blatt"));currentSheet->setObjectName("currentSheet");
    allElements=new QRadioButton(ui("Alle Elemente"));allElements->setObjectName("allElements");
    selectedElements=new QRadioButton(ui("Nur markierte Elemente"));selectedElements->setObjectName("selectedElements");
    allSheets=new QRadioButton(ui("Alle Blätter"));allSheets->setObjectName("allSheets");allSheets->setToolTip(ui("Jedes Blatt in eine eigene Datei, die Blattnummer angehängt (Name_1, Name_2 …); Reserveblätter nicht."));
    for(auto *r:{currentSheet,allElements,selectedElements,allSheets}){al->addWidget(r);connect(r,&QRadioButton::toggled,this,[this](bool on){if(on)update();});}
    al->addStretch();
    allSheets->setEnabled(sheets>1);selectedElements->setEnabled(!selection.isEmpty());currentSheet->setChecked(true);
    formats[std::clamp(lastFormat,0,5)]->setChecked(true);dpi->setValue(lastDpi);(lastBlack?blackAndWhite:colour)->setChecked(true);
    layout->addWidget(okCancel(this),2,0,1,2);
    connect(this,&QDialog::accepted,this,[this]{lastFormat=format();lastDpi=dpi->value();lastBlack=blackAndWhite->isChecked();});
    update();
}
ExportDialog::Format ExportDialog::format() const{for(int i=0;i<6;i++)if(formats[i]&&formats[i]->isChecked())return Format(i);return Jpg;}
QString ExportDialog::suffix() const{static const char *names[]={"jpg","png","bmp","emf","svg","pdf"};return QString::fromLatin1(names[format()]);}
ExportArea ExportDialog::area() const{return allElements->isChecked()?ExportArea::Elements:selectedElements->isChecked()?ExportArea::Selection:ExportArea::Sheet;}
void ExportDialog::update(){
    if(!currentSheet)return;
    // PDF only of whole sheets; transparency for PNG and the vector formats; a resolution only for pictures.
    const bool pdf=format()==Pdf;
    allElements->setEnabled(!pdf);selectedElements->setEnabled(!pdf&&!sizes[2].isEmpty());
    if((allElements->isChecked()||selectedElements->isChecked())&&!allElements->isEnabled())currentSheet->setChecked(true);
    if(selectedElements->isChecked()&&!selectedElements->isEnabled())currentSheet->setChecked(true);
    transparent->setEnabled(format()==Png||format()==Emf||format()==Svg);
    resolution->setEnabled(raster());dpi->setEnabled(raster());
    const QSizeF size=sizes[allElements->isChecked()?1:selectedElements->isChecked()?2:0];
    const QLocale l=uiLocale();
    original->setText(QStringLiteral("%1 mm x %2 mm").arg(l.toString(size.width(),'f',1),l.toString(size.height(),'f',1)));
    pixels->setText(raster()?QStringLiteral("%1 x %2").arg(std::lround(size.width()*dpi->value()/25.4)).arg(std::lround(size.height()*dpi->value()/25.4)):QStringLiteral("–"));
}

ProblemDialog::ProblemDialog(int outside,int pictures,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Probleme"));auto *layout=new QVBoxLayout(this);
    auto choose=[this](Choice c){choice=c;accept();};
    if(outside>0){
        auto *box=new QGroupBox;auto *bl=new QVBoxLayout(box);layout->addWidget(box);
        auto *text=new QLabel((outside==1?ui("Ein Element liegt ganz außerhalb des Blatts."):ui("%1 Elemente liegen ganz außerhalb des Blatts.").arg(outside))+u'\n'+ui("Was soll mit ihnen geschehen?"));
        text->setObjectName("outsideText");bl->addWidget(text);
        deleteButton=new QPushButton(ui("Elemente löschen"));deleteButton->setObjectName("deleteOutside");bl->addWidget(deleteButton);
        moveButton=new QPushButton(ui("Elemente auf das Blatt verschieben"));moveButton->setObjectName("moveOutside");bl->addWidget(moveButton);
        connect(deleteButton,&QPushButton::clicked,this,[choose]{choose(Delete);});connect(moveButton,&QPushButton::clicked,this,[choose]{choose(Move);});
    }
    if(pictures>0){
        auto *box=new QGroupBox;auto *bl=new QVBoxLayout(box);layout->addWidget(box);
        auto *text=new QLabel(ui("%1 Bilder haben mehr als 300 dpi. Das kostet Speicher; eine geringere Auflösung genügt meist.").arg(pictures));
        text->setObjectName("picturesText");text->setWordWrap(true);bl->addWidget(text);
        picturesButton=new QPushButton(ui("Bitmap-Explorer..."));picturesButton->setObjectName("bitmapExplorer");bl->addWidget(picturesButton);
        connect(picturesButton,&QPushButton::clicked,this,[choose]{choose(Pictures);});
    }
    auto *cancel=new QPushButton(ui("Abbrechen"));cancel->setObjectName("cancel");layout->addWidget(cancel);connect(cancel,&QPushButton::clicked,this,&QDialog::reject);
}

namespace {
// The resolution of the last copy in this session.
int &clipboardResolution(){static int value=300;return value;}
}
ClipboardDialog::ClipboardDialog(bool selection,QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Kopieren in die Zwischenablage"));auto *layout=new QHBoxLayout(this);auto *left=new QVBoxLayout;layout->addLayout(left);
    auto *resolutionBox=new QGroupBox(ui("Auflösung"));auto *rl=new QVBoxLayout(resolutionBox);left->addWidget(resolutionBox);
    dpi75=new QRadioButton(QStringLiteral("75 dpi"));dpi75->setObjectName("dpi75");dpi150=new QRadioButton(QStringLiteral("150 dpi"));dpi150->setObjectName("dpi150");
    dpi300=new QRadioButton(QStringLiteral("300 dpi"));dpi300->setObjectName("dpi300");
    for(auto *r:{dpi75,dpi150,dpi300})rl->addWidget(r);
    auto *choiceBox=new QGroupBox(ui("Auswahl"));auto *cl=new QVBoxLayout(choiceBox);left->addWidget(choiceBox);
    all=new QRadioButton(ui("Alles"));all->setObjectName("all");selected=new QRadioButton(ui("Markierung"));selected->setObjectName("selected");
    cl->addWidget(all);cl->addWidget(selected);
    selected->setEnabled(selection);(selection?selected:all)->setChecked(true);
    {const int last=clipboardResolution();(last==75?dpi75:last==150?dpi150:dpi300)->setChecked(true);}
    auto *buttons=okCancel(this);buttons->setOrientation(Qt::Vertical);layout->addWidget(buttons,0,Qt::AlignBottom);
}
int ClipboardDialog::resolution() const{return dpi75->isChecked()?75:dpi150->isChecked()?150:300;}
bool ClipboardDialog::selectionOnly() const{return selected->isEnabled()&&selected->isChecked();}
void ClipboardDialog::accept(){clipboardResolution()=resolution();QDialog::accept();}
}
