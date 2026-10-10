#include "search.h"
#include "language.h"
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace openloch::schematic {
namespace {
Qt::CaseSensitivity sensitivity(const SearchOptions &o){return o.caseSensitive?Qt::CaseSensitive:Qt::CaseInsensitive;}
bool matches(const QString &field,const QString &wanted,const SearchOptions &o){return wanted.isEmpty()||field.contains(wanted,sensitivity(o));}
void gather(const QList<Item> &items,QList<const Item*> &components,QList<const Item*> &texts){
    for(const auto &i:items){
        if(i.type==ItemType::Component)components<<&i;
        else if(i.type==ItemType::Text||i.type==ItemType::TextBox||i.type==ItemType::NetLabel)texts<<&i;
        else if(i.type==ItemType::Group)gather(i.children,components,texts);
    }
}
// The element with an id on a sheet, also inside groups.
Item *find(QList<Item> &items,const QString &id){
    for(auto &i:items){if(i.id==id)return &i;if(i.type==ItemType::Group)if(Item *f=find(i.children,id))return f;}
    return nullptr;
}
}
QList<SearchHit> search(const Document &document,const SearchOptions &o){
    QList<SearchHit> out;
    for(int s=0;s<document.sheets.size();s++){
        if(!o.allSheets&&s!=o.currentSheet)continue;
        QList<const Item*> components,texts;gather(document.sheets[s].items,components,texts);
        if(o.components){
            if(o.designator.isEmpty()&&o.value.isEmpty())continue;
            for(const auto *c:components){
                if(o.partsListOnly&&!c->inPartsList)continue;
                const TextContext context{&document,s,c,o.fileName};
                // Expanded, a designator is searched as shown (with the drawing's prefix and sheet number), as in the
                // reference; as written, as entered.
                const QString d=o.expand?shownDesignator(*c,context):c->designator,v=o.expand?expandVariables(c->value,context):c->value;
                if(matches(d,o.designator,o)&&matches(v,o.value,o))out.append({s,c->id,v.isEmpty()?d:d+QStringLiteral(" / ")+v});
            }
        }else{
            if(o.text.isEmpty())continue;
            for(const auto *t:texts){
                const QString shown=o.expand?shownText(*t,{&document,s,nullptr,o.fileName}):t->text;
                if(matches(shown,o.text,o))out.append({s,t->id,QString(shown).replace(u'\n',u' ')});
            }
        }
    }
    return out;
}
int replace(Document &document,const QList<SearchHit> &hits,const SearchOptions &o,const QString &replacement,ReplaceField field){
    int changed=0;
    auto swap=[&](QString &field,const QString &wanted){
        if(wanted.isEmpty()||!field.contains(wanted,sensitivity(o)))return;
        field.replace(wanted,replacement,sensitivity(o));changed++;
    };
    for(const auto &h:hits){
        if(h.sheet<0||h.sheet>=document.sheets.size())continue;
        Item *i=find(document.sheets[h.sheet].items,h.id);if(!i)continue;
        const bool all=field==ReplaceField::All;
        if(o.components){if(all||field==ReplaceField::Designator)swap(i->designator,o.designator);if(all||field==ReplaceField::Value)swap(i->value,o.value);}
        else if(all||field==ReplaceField::Text)swap(i->text,o.text);
    }
    return changed;
}

SearchPanel::SearchPanel(QWidget *parent):QWidget(parent){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);
    auto *title=new QLabel(ui("Suchen"));title->setStyleSheet(QStringLiteral("font-weight:bold"));layout->addWidget(title);
    auto *modes=new QHBoxLayout;layout->addLayout(modes);
    componentsMode=new QRadioButton(ui("Bauteile"));componentsMode->setObjectName("searchComponents");componentsMode->setChecked(true);modes->addWidget(componentsMode);
    textsMode=new QRadioButton(ui("Texte"));textsMode->setObjectName("searchTexts");modes->addWidget(textsMode);modes->addStretch();
    // As in sPlan: at the left what is looked for, at the right what replaces it with a button for that field alone.
    auto field=[](const char *name){auto *e=new QLineEdit;e->setObjectName(name);return e;};
    auto replaceIn=[](const char *name){auto *b=new QPushButton(ui("Ersetzen ->"));b->setObjectName(name);b->setEnabled(false);return b;};
    componentsBox=new QWidget;auto *cg=new QGridLayout(componentsBox);cg->setContentsMargins(0,0,0,0);layout->addWidget(componentsBox);
    designator=field("searchDesignator");designatorReplacement=field("designatorReplacement");designatorReplace=replaceIn("designatorReplace");
    value=field("searchValue");valueReplacement=field("valueReplacement");valueReplace=replaceIn("valueReplace");
    cg->addWidget(new QLabel(ui("Bezeichner:")),0,0);cg->addWidget(new QLabel(ui("Ersetzen mit:")),0,1);
    cg->addWidget(designator,1,0);cg->addWidget(designatorReplacement,1,1);cg->addWidget(designatorReplace,2,1);
    cg->addWidget(new QLabel(ui("Wert:")),3,0);cg->addWidget(new QLabel(ui("Ersetzen mit:")),3,1);
    cg->addWidget(value,4,0);cg->addWidget(valueReplacement,4,1);cg->addWidget(valueReplace,5,1);
    partsListOnly=new QCheckBox(ui("Nur Stücklistenbauteile"));partsListOnly->setObjectName("partsListOnly");cg->addWidget(partsListOnly,6,0,1,2);
    textsBox=new QWidget;auto *tg=new QGridLayout(textsBox);tg->setContentsMargins(0,0,0,0);layout->addWidget(textsBox);
    text=field("searchText");replacement=field("replacement");replaceButton=replaceIn("replace");
    tg->addWidget(new QLabel(ui("Suchtext:")),0,0);tg->addWidget(new QLabel(ui("Ersetzen:")),0,1);
    tg->addWidget(text,1,0);tg->addWidget(replacement,1,1);tg->addWidget(replaceButton,2,1);
    caseSensitive=new QCheckBox(ui("Groß-/Kleinschreibung beachten"));caseSensitive->setObjectName("caseSensitive");layout->addWidget(caseSensitive);
    expand=new QCheckBox(ui("<Variablen> auflösen"));expand->setObjectName("expand");expand->setChecked(true);layout->addWidget(expand);
    allSheets=new QCheckBox(ui("Alle Blätter"));allSheets->setObjectName("allSheets");allSheets->setChecked(true);layout->addWidget(allSheets);
    auto *buttons=new QHBoxLayout;layout->addLayout(buttons);
    auto *update=new QPushButton(ui("Aktualisieren"));update->setObjectName("searchUpdate");buttons->addWidget(update);buttons->addStretch();
    results=new QTreeWidget;results->setObjectName("searchResults");results->setHeaderHidden(true);layout->addWidget(results,1);
    // The fields of components or of texts, as sPlan's two tabs.
    auto modeChanged=[this]{const bool c=componentsMode->isChecked();componentsBox->setVisible(c);textsBox->setVisible(!c);refresh();};
    connect(componentsMode,&QRadioButton::toggled,this,modeChanged);
    for(QLineEdit *e:{designator,value,text})connect(e,&QLineEdit::textChanged,this,[this]{refresh();});
    for(QCheckBox *c:{partsListOnly,caseSensitive,expand,allSheets})connect(c,&QCheckBox::toggled,this,[this]{refresh();});
    connect(update,&QPushButton::clicked,this,[this]{refresh();});
    connect(designatorReplace,&QPushButton::clicked,this,[this]{replaceChosen(ReplaceField::Designator);});
    connect(valueReplace,&QPushButton::clicked,this,[this]{replaceChosen(ReplaceField::Value);});
    connect(replaceButton,&QPushButton::clicked,this,[this]{replaceChosen(ReplaceField::Text);});
    connect(results,&QTreeWidget::currentItemChanged,this,[this]{chosenChanged();});
    connect(results,&QTreeWidget::itemClicked,this,[this](QTreeWidgetItem *item){
        const QString id=item->data(0,Qt::UserRole).toString();if(!id.isEmpty()&&showElement)showElement(id);});
    modeChanged();
}
SearchOptions SearchPanel::options() const{
    SearchOptions o;o.components=componentsMode->isChecked();o.designator=designator->text();o.value=value->text();o.text=text->text();
    o.partsListOnly=partsListOnly->isChecked();o.caseSensitive=caseSensitive->isChecked();o.expand=expand->isChecked();o.allSheets=allSheets->isChecked();
    const Document *d=document?document():nullptr;o.currentSheet=d?d->activeSheet:0;o.fileName=fileName?fileName():QString();
    return o;
}
void SearchPanel::refresh(){
    results->clear();found.clear();
    const Document *d=document?document():nullptr;if(!d)return;
    found=search(*d,options());
    // Everything found under "Alle Blätter", then by sheet.
    auto *all=new QTreeWidgetItem(results,{ui("Alle Blätter")});all->setData(0,Qt::UserRole+1,-1);
    QHash<int,QTreeWidgetItem*> sheets;
    for(const auto &h:found){
        QTreeWidgetItem *&s=sheets[h.sheet];
        if(!s){s=new QTreeWidgetItem(all,{QStringLiteral("%1: %2").arg(h.sheet+1).arg(d->sheets[h.sheet].name)});s->setData(0,Qt::UserRole+1,h.sheet);}
        auto *row=new QTreeWidgetItem(s,{h.shown});row->setData(0,Qt::UserRole,h.id);row->setData(0,Qt::UserRole+1,h.sheet);
    }
    results->expandAll();chosenChanged();
}
void SearchPanel::chosenChanged(){
    // Each button names what it replaces in: the element, the sheet or all sheets chosen in the results.
    const QTreeWidgetItem *item=results->currentItem();
    const bool element=item&&!item->data(0,Qt::UserRole).toString().isEmpty(),allOf=item&&item->data(0,Qt::UserRole+1).toInt()<0;
    const QString caption=!item||element?ui("Ersetzen ->"):allOf?ui("Alle Blätter ->"):ui("Ganzes Blatt ->");
    for(QPushButton *b:{designatorReplace,valueReplace,replaceButton}){b->setEnabled(item&&!found.isEmpty());b->setText(caption);}
}
void SearchPanel::replaceChosen(ReplaceField field){
    const QTreeWidgetItem *item=results->currentItem();if(!item||!change)return;
    const QString id=item->data(0,Qt::UserRole).toString();const int sheet=item->data(0,Qt::UserRole+1).toInt();
    QList<SearchHit> chosen;
    for(const auto &h:found)if((!id.isEmpty()&&h.id==id)||(id.isEmpty()&&(sheet<0||h.sheet==sheet)))chosen<<h;
    const SearchOptions o=options();
    const QString with=field==ReplaceField::Designator?designatorReplacement->text():field==ReplaceField::Value?valueReplacement->text():replacement->text();
    change([&](Document &d){replace(d,chosen,o,with,field);});
    refresh();
}
}
