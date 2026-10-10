#include "properties.h"
#include "dialogs.h"
#include "settings.h"
#include "view.h"
#include "render.h"
#include "images.h"
#include "language.h"
#include "library.h"
#include "legacy_reader.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QTableWidget>
#include <QTextDocument>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
#include <tuple>

namespace openloch::schematic {
namespace {
// A field over several lines that reports its text when it loses the focus with a change, as the reference's memos.
class MemoField : public QPlainTextEdit {
public:
    MemoField(const QString &text,int lines,std::function<void(const QString&)> done):QPlainTextEdit(text),finished(std::move(done)){
        setTabChangesFocus(true);setFixedHeight(fontMetrics().lineSpacing()*lines+2*frameWidth()+int(document()->documentMargin()*2));
        document()->setModified(false);
    }
protected:
    void focusOutEvent(QFocusEvent *e) override{
        QPlainTextEdit::focusOutEvent(e);
        if(document()->isModified()){document()->setModified(false);if(finished)finished(toPlainText());}
    }
private:
    std::function<void(const QString&)> finished;
};
QDoubleSpinBox *numberBox(double value,double low,double high,int decimals=2,const QString &suffix={}){
    auto *b=new QDoubleSpinBox;b->setRange(low,high);b->setDecimals(decimals);b->setValue(value);b->setKeyboardTracking(false);
    b->setLocale(uiLocale());if(!suffix.isEmpty())b->setSuffix(suffix);b->setAccelerated(true);return b;
}
QToolButton *colourButton(const QColor &c){
    auto *b=new QToolButton;b->setFixedSize(40,20);b->setStyleSheet(QStringLiteral("background:%1;border:1px solid #444").arg(c.name()));b->setProperty("colour",c);return b;
}
// A choice of the thirteen line ends in the model's order (as numbered in the reference), each with its picture: a
// short line with the end at its start or its end, drawn as on the sheet.
QComboBox *lineEndBox(LineEnd current,bool atStart){
    static const char *names[]={"Ohne","Dreieck","Pfeil","Offener Pfeil","Kreis","Punkt","Raute","Raute, gefüllt","Quadrat",
                                "Quadrat, gefüllt","Querstrich","Pfeil rückwärts","Dreieck rückwärts"};
    auto *box=new QComboBox;box->setIconSize(QSize(48,16));
    const Document document=newDocument(QString());
    for(int e=0;e<=int(LineEnd::BackTriangle);e++){
        QPixmap picture(48,16);picture.fill(Qt::transparent);
        Item line;line.type=ItemType::Line;line.points={QPointF(1.5,2),QPointF(10.5,2)};line.pen.width=.35;line.endSize=3;
        (atStart?line.startEnd:line.endEnd)=LineEnd(e);
        {QPainter p(&picture);p.setRenderHint(QPainter::Antialiasing);p.scale(4,4);paintItems(p,{line},document,0);}
        box->addItem(QIcon(picture),ui(names[e]));
    }
    box->setCurrentIndex(int(current));return box;
}
}

PropertiesPanel::PropertiesPanel(QWidget *parent):QWidget(parent){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto *title=new QLabel(ui("Eigenschaften"));title->setObjectName("propertiesTitle");title->setStyleSheet(QStringLiteral("font-weight:bold;padding:4px"));layout->addWidget(title);
    scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);layout->addWidget(scroll,1);
    setMinimumWidth(230);
}
QFormLayout *PropertiesPanel::section(const QString &title){
    auto *header=new QLabel(title);header->setStyleSheet(QStringLiteral("font-weight:bold;background:palette(midlight);padding:3px"));
    sections->addWidget(header);
    auto *box=new QWidget;auto *form=new QFormLayout(box);form->setContentsMargins(6,2,4,6);form->setLabelAlignment(Qt::AlignRight);
    sections->addWidget(box);return form;
}
QList<Item*> PropertiesPanel::selection() const{
    QList<Item*> out;if(!sheetView)return out;
    if(filtered()){for(Item *i:resolved())if(typeFilter.contains(int(i->type)))out<<i;return out;}
    for(auto &i:sheetView->items())if(sheetView->selection().contains(i.id))out<<&i;
    // An element of a group chosen alone (Alt).
    if(out.isEmpty()&&sheetView->selection().size()==1)if(Item *n=sheetView->nestedItem(sheetView->selection()[0]))out<<n;
    return out;
}
bool PropertiesPanel::filtered() const{return sheetView&&!typeFilter.isEmpty()&&filterFor==sheetView->selection();}
QList<Item*> PropertiesPanel::resolved() const{
    QList<Item*> out;if(!sheetView)return out;
    std::function<void(Item&)> take=[&](Item &i){if(i.type==ItemType::Group){for(auto &k:i.children)take(k);}else out<<&i;};
    for(auto &i:sheetView->items())if(sheetView->selection().contains(i.id))take(i);
    return out;
}
QStringList PropertiesPanel::editedIds() const{
    if(!filtered())return sheetView->selection();
    QStringList ids;for(const Item *i:selection())ids<<i->id;return ids;
}
Item *PropertiesPanel::preset() const{
    if(!sheetView)return nullptr;
    switch(sheetView->tool()){
    case SheetView::Tool::Line:case SheetView::Tool::Bezier:case SheetView::Tool::Freehand:return &sheetView->linePreset;
    case SheetView::Tool::Rectangle:case SheetView::Tool::Ellipse:case SheetView::Tool::Polygon:case SheetView::Tool::Special:return &sheetView->shapePreset;
    case SheetView::Tool::Junction:return &sheetView->junctionPreset;
    case SheetView::Tool::Text:return &sheetView->textPreset;
    case SheetView::Tool::TextBox:return &sheetView->textBoxPreset;
    case SheetView::Tool::NetLabel:case SheetView::Tool::SheetReference:return &sheetView->labelPreset;
    case SheetView::Tool::Dimension:return &sheetView->dimensionPreset;
    default:return nullptr;
    }
}
void PropertiesPanel::edit(const std::function<void(Item&)> &fn){
    if(refreshing||!sheetView)return;
    // A preset changes on its own, not as a step of the document.
    if(presetItem){
        fn(*presetItem);
        if(libraryItem&&presetItem==&*libraryItem){if(libraryEntryChanged)libraryEntryChanged(*libraryItem,libraryCaption);}
        else if(presetChanged)presetChanged();
        refresh();return;
    }
    if(!change)return;
    const QStringList ids=editedIds();const QString component=sheetView->componentMode();
    change([ids,fn,component](Document &d){
        for(const auto &id:ids){
            Item *i=nullptr;
            if(!component.isEmpty()){if(Item *c=findItem(d.sheet(),component))for(auto &k:c->children)if(k.id==id)i=&k;}
            else i=findItem(d.sheet(),id);
            if(i)fn(*i);
        }
    });
}
namespace {
// Named outlines: a JSON array of names with a line carrying the outline, as in the own format, under one key.
const char *const linePresetKey="schematic/linePresets";
QList<std::pair<QString,Pen>> storedLinePresets(){
    QList<std::pair<QString,Pen>> out;
    for(const auto &v:QJsonDocument::fromJson(QSettings().value(linePresetKey).toByteArray()).array()){
        const auto o=v.toObject();
        try{const Item i=itemFromJson(o["item"],false);if(i.type==ItemType::Line&&!o["name"].toString().isEmpty())out<<std::pair{o["name"].toString(),i.pen};}
        catch(const FormatError &){}
    }
    return out;
}
void storeLinePresets(const QList<std::pair<QString,Pen>> &list){
    QJsonArray a;
    for(const auto &[name,pen]:list){Item i;i.type=ItemType::Line;i.points={QPointF(),QPointF(1,0)};i.pen=pen;a.append(QJsonObject{{"name",name},{"item",itemToJson(i)}});}
    QSettings().setValue(linePresetKey,QJsonDocument(a).toJson(QJsonDocument::Compact));
}
}
QStringList PropertiesPanel::linePresets(){QStringList out;for(const auto &p:storedLinePresets())out<<p.first;return out;}
bool PropertiesPanel::addLinePreset(const QString &name){
    // The outline of the mode's preset, or of the selection when all are alike.
    Pen pen;
    if(presetItem)pen=presetItem->pen;
    else{const auto items=selection();if(items.isEmpty())return false;pen=items[0]->pen;for(const Item *i:items)if(!(i->pen==pen))return false;}
    auto list=storedLinePresets();list.removeIf([&](const auto &p){return p.first==name;});list<<std::pair{name,pen};storeLinePresets(list);
    return true;
}
void PropertiesPanel::applyLinePreset(const QString &name){for(const auto &[n,pen]:storedLinePresets())if(n==name){applyPen(pen);return;}}
void PropertiesPanel::removeLinePreset(const QString &name){auto list=storedLinePresets();list.removeIf([&](const auto &p){return p.first==name;});storeLinePresets(list);}
void PropertiesPanel::applyPen(const Pen &pen){
    edit([pen](Item &i){
        Pen p=pen;
        if(i.type==ItemType::TextBox)p.twoColour=p.inner=p.cross=false;                                   // text frames have no stripes
        if((i.type==ItemType::Line||i.type==ItemType::Bezier)&&p.style==PenStyle::None)p.style=PenStyle::Solid;   // lines are always drawn
        i.pen=p;});
}
void PropertiesPanel::showLibraryEntry(const Item &symbol,const QString &caption,bool writable){
    libraryItem=symbol;libraryCaption=caption;libraryWritable=writable;refresh();
}
void PropertiesPanel::showPreset(Item *preset,const QString &title,bool show){
    if(!preset&&!chosenPreset)return;
    chosenPreset=preset;chosenPresetTitle=title;if(show||!preset)refresh();
}
void PropertiesPanel::clearLibraryEntry(){if(!libraryItem)return;libraryItem.reset();presetItem=nullptr;refresh();}
void PropertiesPanel::edit(const std::function<void(Document&,Item&)> &fn){
    if(refreshing||!change||!sheetView||presetItem)return;
    const QStringList ids=editedIds();const QString component=sheetView->componentMode();
    change([ids,fn,component](Document &d){
        for(const auto &id:ids){
            Item *i=nullptr;
            if(!component.isEmpty()){if(Item *c=findItem(d.sheet(),component))for(auto &k:c->children)if(k.id==id)i=&k;}
            else i=findItem(d.sheet(),id);
            if(i)fn(d,*i);
        }
    });
}
void PropertiesPanel::refresh(){
    refreshing=true;
    content=new QWidget;sections=new QVBoxLayout(content);sections->setContentsMargins(0,0,0,0);sections->setSpacing(0);
    if(sheetView&&filterFor!=sheetView->selection()){typeFilter.clear();filterFor.clear();}
    const auto items=selection();presetItem=nullptr;
    if(libraryItem){
        // A symbol of the library: its caption there and its fields.
        auto *title=new QLabel(ui("Bibliothek"));title->setObjectName("libraryTitle");title->setStyleSheet(QStringLiteral("font-weight:bold;padding:4px"));sections->addWidget(title);
        auto *form=section(ui("Unterschrift in der Bibliothek"));
        // Captions in several languages (German, English, French, apart by a carriage return) show the interface's;
        // a change replaces that one and keeps the others.
        const int language=uiLanguage()==u"en"?1:uiLanguage()==u"fr"?2:0;
        auto *caption=new QLineEdit(localized(libraryCaption));caption->setObjectName("libraryCaption");form->addRow(caption);
        connect(caption,&QLineEdit::editingFinished,this,[this,caption,language]{
            if(refreshing||caption->text()==localized(libraryCaption))return;
            QStringList parts=libraryCaption.split(u'\r');
            if(parts.size()==1)parts={caption->text()};else{while(parts.size()<=language)parts<<QString();parts[language]=caption->text();}
            libraryCaption=parts.join(u'\r');edit([this](Item &i){if(i.type==ItemType::Component)i.caption=libraryCaption;});});
        presetItem=&*libraryItem;itemSections({presetItem});
        if(!libraryWritable)for(auto *w:content->findChildren<QWidget*>())if(qobject_cast<QLineEdit*>(w)||qobject_cast<QAbstractButton*>(w)||qobject_cast<QComboBox*>(w)||qobject_cast<QAbstractSpinBox*>(w))w->setEnabled(false);
    }
    else if(chosenPreset){
        // A preset from "Voreinstellungen": what new elements of its kind get.
        auto *title=new QLabel(chosenPresetTitle+u'\n'+ui("Voreinstellungen für neue Elemente"));title->setObjectName("presetTitle");title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(QStringLiteral("font-weight:bold;padding:6px;background:#a8e693;color:black"));sections->addWidget(title);
        presetItem=chosenPreset;itemSections({presetItem});
    }
    else if(sheetView&&sheetView->document()&&!sheetView->document()->sheets.isEmpty()){
        if(items.isEmpty()&&(presetItem=preset())){
            // "Voreinstellung": what new elements of the drawing mode get.
            auto *title=new QLabel(ui("Voreinstellung"));title->setObjectName("presetTitle");title->setStyleSheet(QStringLiteral("font-weight:bold;padding:4px"));sections->addWidget(title);
            itemSections({presetItem});
        }
        else if(items.isEmpty()&&sheetView->componentMode().isEmpty())sheetSections();
        else if(!items.isEmpty()){typeList();itemSections(items);}
    }
    sections->addStretch();
    // Fields shrink with the panel instead of widening it.
    for(auto *w:content->findChildren<QAbstractSpinBox*>())w->setMinimumWidth(50);
    for(auto *w:content->findChildren<QLineEdit*>())w->setMinimumWidth(50);
    for(auto *w:content->findChildren<QComboBox*>()){w->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);w->setMinimumContentsLength(6);}
    // The old widgets may be sending the signal that led here: they go once it has been handled.
    if(QWidget *old=scroll->takeWidget())old->deleteLater();
    scroll->setWidget(content);
    refreshing=false;
}

void PropertiesPanel::typeList(){
    // The kinds in the reference's order, each with its number; with more than one kind a first row of all.
    const QList<Item*> all=resolved();
    const std::tuple<ItemType,QString,QString> kinds[]={{ItemType::Text,ui("Text"),ui("Texte")},{ItemType::NetLabel,ui("Netzname"),ui("Netznamen")},
        {ItemType::TextBox,ui("Mengentext"),ui("Mengentexte")},{ItemType::Line,ui("Linie"),ui("Linien")},{ItemType::Rectangle,ui("Rechteck"),ui("Rechtecke")},
        {ItemType::Ellipse,ui("Kreis"),ui("Kreise")},{ItemType::Polygon,ui("Polygon"),ui("Polygone")},{ItemType::Bezier,ui("Bezierkurve"),ui("Bezierkurven")},
        {ItemType::Component,ui("Bauteil"),ui("Bauteile")},{ItemType::Dimension,ui("Bemaßung"),ui("Bemaßungen")},{ItemType::Contact,ui("Kontakt"),ui("Kontakte")},
        {ItemType::Image,ui("Bild"),ui("Bilder")},{ItemType::Junction,ui("Lötpunkt"),ui("Lötpunkte")}};
    QList<std::pair<int,QString>> rows;QList<int> types;
    for(const auto &[type,one,many]:kinds){int n=0;for(const Item *i:all)n+=i->type==type;if(n){rows.append({n,n==1?one:many});types<<int(type);}}
    if(rows.size()<2)return;
    auto *list=new QTreeWidget;list->setObjectName("typeList");list->setColumnCount(2);list->setHeaderHidden(true);list->setRootIsDecorated(false);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    auto *total=new QTreeWidgetItem(list,{QString::number(all.size()),ui("Elemente insgesamt")});total->setData(0,Qt::UserRole,-1);
    for(int k=0;k<rows.size();k++){auto *row=new QTreeWidgetItem(list,{QString::number(rows[k].first),rows[k].second});row->setData(0,Qt::UserRole,types[k]);}
    list->resizeColumnToContents(0);list->setFixedHeight(list->sizeHintForRow(0)*(int(rows.size())+1)+2*list->frameWidth()+4);
    for(int k=0;k<list->topLevelItemCount();k++){auto *row=list->topLevelItem(k);const int type=row->data(0,Qt::UserRole).toInt();
        row->setSelected(typeFilter.isEmpty()?type<0:typeFilter.contains(type));}
    sections->addWidget(list);
    connect(list,&QTreeWidget::itemSelectionChanged,this,[this,list]{
        if(refreshing)return;
        QSet<int> chosen;for(auto *row:list->selectedItems()){const int type=row->data(0,Qt::UserRole).toInt();if(type>=0)chosen<<type;}
        typeFilter=chosen;filterFor=sheetView->selection();
        QMetaObject::invokeMethod(this,[this]{refresh();},Qt::QueuedConnection);});
}
void PropertiesPanel::sheetSections(){
    Document &d=*sheetView->document();const Sheet &s=d.sheet();
    auto *form=section(ui("Blatt"));
    // Name and description go to the sheet they were typed for, also when the field is left by choosing another sheet
    // (the panel is then being built anew; the change follows once that is done).
    const QString sheetId=s.id;
    auto toSheet=[this,sheetId](std::function<bool(Sheet&)> set){
        QMetaObject::invokeMethod(this,[this,sheetId,set]{
            if(!change||!sheetView->document())return;
            bool differs=false;for(auto &sh:sheetView->document()->sheets)if(sh.id==sheetId){Sheet copy=sh;differs=set(copy);}
            if(differs)change([sheetId,set](Document &d){for(auto &sh:d.sheets)if(sh.id==sheetId)set(sh);});},Qt::QueuedConnection);};
    auto *name=new QLineEdit(s.name);name->setObjectName("sheetName");form->addRow(ui("Name:"),name);
    connect(name,&QLineEdit::editingFinished,this,[name,toSheet]{const QString v=name->text();toSheet([v](Sheet &sh){const bool d=sh.name!=v;sh.name=v;return d;});});
    // Over several lines, as the reference's memo.
    auto *description=new MemoField(s.description,3,[toSheet](const QString &v){toSheet([v](Sheet &sh){const bool d=sh.description!=v;sh.description=v;return d;});});
    description->setObjectName("sheetDescription");form->addRow(ui("Beschreibung:"),description);
    // The formats as in sPlan: "Frei" (any size) first, then A0–A5, B4, B5, C3–C5, Letter and Legal.
    auto *format=new QComboBox;format->setObjectName("sheetFormat");format->addItem(ui("Frei"));
    for(const auto &p:paperFormats())format->addItem(QString::fromLatin1(p.name));
    format->setCurrentIndex(paperFormatOf(s.width,s.height)+1);
    form->addRow(ui("Format:"),format);
    auto *width=numberBox(s.width,1,10000,1,QStringLiteral(" mm"));width->setObjectName("sheetWidth");form->addRow(ui("Breite:"),width);
    auto *height=numberBox(s.height,1,10000,1,QStringLiteral(" mm"));height->setObjectName("sheetHeight");form->addRow(ui("Höhe:"),height);
    auto *landscape=new QRadioButton(ui("Querformat")),*portrait=new QRadioButton(ui("Hochformat"));
    landscape->setObjectName("landscape");portrait->setObjectName("portrait");(s.width>=s.height?landscape:portrait)->setChecked(true);
    auto *orientation=new QWidget;auto *ol=new QVBoxLayout(orientation);ol->setContentsMargins(0,0,0,0);ol->addWidget(landscape);ol->addWidget(portrait);form->addRow(QString(),orientation);
    auto *spare=new QCheckBox(ui("(nicht mitdrucken)"));spare->setObjectName("spare");spare->setChecked(s.spare);form->addRow(ui("Reserveblatt:"),spare);
    auto setSize=[this](double w,double h){if(change)change([w,h](Document &d){d.sheet().width=w;d.sheet().height=h;});};
    connect(format,&QComboBox::activated,this,[setSize,portrait](int i){
        if(i<1||i>int(paperFormats().size()))return;
        const auto &p=paperFormats()[i-1];if(portrait->isChecked())setSize(p.height,p.width);else setSize(p.width,p.height);});
    connect(width,&QDoubleSpinBox::valueChanged,this,[this,setSize](double v){if(!refreshing)setSize(v,sheetView->document()->sheet().height);});
    connect(height,&QDoubleSpinBox::valueChanged,this,[this,setSize](double v){if(!refreshing)setSize(sheetView->document()->sheet().width,v);});
    connect(landscape,&QRadioButton::toggled,this,[this,setSize](bool on){
        if(refreshing)return;const Sheet &s=sheetView->document()->sheet();
        if(on!=(s.width>=s.height))setSize(s.height,s.width);});
    connect(spare,&QCheckBox::toggled,this,[this](bool on){if(!refreshing&&change)change([on](Document &d){d.sheet().spare=on;});});

    // Title block: the frame with its divisions, generated or removed.
    {   // "Maßstab": what a millimetre of the sheet stands for; dimensions show lengths times it.
        auto *sf=section(ui("Maßstab"));
        auto *row=new QWidget;auto *h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);
        auto *factor=numberBox(s.scale,.1,1000,3);factor->setObjectName("sheetScale");h->addWidget(factor,1);
        auto *unit=new QComboBox;unit->setObjectName("scaleUnit");
        for(auto u:{ScaleUnit::Millimetre,ScaleUnit::Centimetre,ScaleUnit::Metre,ScaleUnit::Kilometre})unit->addItem(unitName(u));
        unit->setCurrentIndex(int(s.scaleUnit));h->addWidget(unit);sf->addRow(ui("1 mm ="),row);
        auto *range=new QLabel(QStringLiteral("(%1 \u2026 %2)").arg(uiLocale().toString(.1),QStringLiteral("1000")));range->setStyleSheet(QStringLiteral("color:gray"));sf->addRow(QString(),range);
        auto *reset=new QPushButton(ui("Zurücksetzen"));reset->setObjectName("resetScale");sf->addRow(QString(),reset);
        connect(factor,&QDoubleSpinBox::valueChanged,this,[this](double v){if(!refreshing&&change)change([v](Document &d){d.sheet().scale=v;});});
        connect(unit,&QComboBox::activated,this,[this](int v){if(change)change([v](Document &d){d.sheet().scaleUnit=ScaleUnit(v);});});
        connect(reset,&QPushButton::clicked,this,[this]{if(change)change([](Document &d){d.sheet().scale=1;d.sheet().scaleUnit=ScaleUnit::Millimetre;});});
    }
    auto *tb=section(ui("Formblatt"));
    const TitleBlock &t=s.titleBlock;
    auto frameBox=[&](double v,const char *name,int part){
        auto *b=numberBox(v,0,10000,1,QStringLiteral(" mm"));b->setObjectName(name);
        connect(b,&QDoubleSpinBox::valueChanged,this,[this,part](double v){
            if(refreshing||!change)return;
            change([part,v](Document &d){QRectF &f=d.sheet().titleBlock.frame;
                switch(part){case 0:f.moveLeft(v);break;case 1:f.moveTop(v);break;case 2:f.setWidth(v);break;default:f.setHeight(v);break;}});});
        return b;
    };
    // In sPlan's order: size, offset, the division with the numbers of the first column and row, the grid lines with
    // "Auto" (frame and division from the sheet's size) beside them.
    tb->addRow(ui("Breite:"),frameBox(t.frame.width(),"frameWidth",2));
    tb->addRow(ui("Höhe:"),frameBox(t.frame.height(),"frameHeight",3));
    tb->addRow(ui("X-Offset:"),frameBox(t.frame.x(),"frameX",0));
    tb->addRow(ui("Y-Offset:"),frameBox(t.frame.y(),"frameY",1));
    auto countBox=[&](int v,int low,int high,const char *name,int TitleBlock::*field){
        auto *b=new QSpinBox;b->setRange(low,high);b->setValue(v);b->setObjectName(name);b->setKeyboardTracking(false);
        connect(b,&QSpinBox::valueChanged,this,[this,field](int v){if(!refreshing&&change)change([field,v](Document &d){d.sheet().titleBlock.*field=v;});});
        return b;
    };
    tb->addRow(ui("Spalten:"),countBox(t.columns,0,999,"frameColumns",&TitleBlock::columns));
    tb->addRow(ui("Zeilen:"),countBox(t.rows,0,999,"frameRows",&TitleBlock::rows));
    tb->addRow(ui("Spalte-Start:"),countBox(t.columnStart,1,10000,"frameColumnStart",&TitleBlock::columnStart));
    tb->addRow(ui("Zeile-Start:"),countBox(t.rowStart,1,10000,"frameRowStart",&TitleBlock::rowStart));
    auto *gridRow=new QWidget;auto *gl=new QHBoxLayout(gridRow);gl->setContentsMargins(0,0,0,0);
    auto *grid=new QCheckBox;grid->setObjectName("frameGrid");grid->setChecked(t.showGrid);gl->addWidget(grid);gl->addStretch();
    auto *automatic=new QPushButton(ui("Auto"));automatic->setObjectName("autoGrid");
    automatic->setToolTip(ui("Rahmen 10 mm innerhalb des Blatts, Felder von etwa 23 mm, gezählt ab 1; die Beschriftung bleibt"));gl->addWidget(automatic);
    tb->addRow(ui("Zeige Gitter:"),gridRow);
    connect(grid,&QCheckBox::toggled,this,[this](bool on){if(!refreshing&&change)change([on](Document &d){d.sheet().titleBlock.showGrid=on;});});
    connect(automatic,&QPushButton::clicked,this,[this]{if(change)change([](Document &d){autoGrid(d.sheet());});});
    auto *generate=new QPushButton(ui("Generieren…"));generate->setObjectName("generateTitleBlock");tb->addRow(QString(),generate);
    connect(generate,&QPushButton::clicked,this,[this]{if(titleBlockRequested)titleBlockRequested();});
}

void PropertiesPanel::itemSections(const QList<Item*> &items){
    const Item &first=*items[0];
    auto all=[&](auto predicate){for(const auto *i:items)if(!predicate(*i))return false;return true;};
    auto any=[&](auto predicate){for(const auto *i:items)if(predicate(*i))return true;return false;};
    const bool inComponentEditor=!sheetView->componentMode().isEmpty();

    const bool preset=presetItem!=nullptr;
    // Position as in the reference: X and Y of the reference point in the sheet's scale from the origin of rulers and
    // coordinates; a single component by its insertion point. "Als Gruppe behandeln" (off at first, ticked for a single
    // group) takes the selection's frame as one and moves it as a block; without it each element puts its own reference
    // point on the value typed, which aligns them, and "Ausrichten" opens the list of alignments.
    if(!preset){
        auto *form=section(ui("Position"));
        const bool component=items.size()==1&&first.type==ItemType::Component,several=items.size()>1;
        const double factor=sheetView->document()&&!sheetView->document()->sheets.isEmpty()?sheetView->document()->sheet().scale:1;
        const QPointF origin=sheetView->originPoint();
        auto shown=[=](QPointF p){return (p-origin)*factor;};
        auto onSheet=[=](QPointF v){return v/factor+origin;};
        auto refPoint=[component](const QRectF &b,const Item *single,int index)->QPointF{
            if(component){if(index==0)return single->pos;index--;}
            switch(index){case 0:return b.topLeft();case 1:return b.topRight();case 2:return b.bottomLeft();case 3:return b.bottomRight();default:return b.center();}
        };
        const bool asOne=!several||positionAsGroup;
        QRectF box;if(asOne)for(const auto *i:items)box=box.united(bounds(*i));else box=bounds(first);
        const QPointF at=shown(refPoint(box,&first,0));
        auto *x=numberBox(at.x(),-1e7,1e7,2,QStringLiteral(" mm")),*y=numberBox(at.y(),-1e7,1e7,2,QStringLiteral(" mm"));
        x->setObjectName("positionX");y->setObjectName("positionY");form->addRow(ui("X:"),x);form->addRow(ui("Y:"),y);
        auto *reference=new QComboBox;reference->setObjectName("reference");
        if(component)reference->addItem(ui("Einfügepunkt"));
        reference->addItems({ui("Oben-Links"),ui("Oben-Rechts"),ui("Unten-Links"),ui("Unten-Rechts"),ui("Mittelpunkt")});
        form->addRow(ui("Referenzpunkt:"),reference);
        auto *asGroup=new QCheckBox(ui("Als Gruppe behandeln"));asGroup->setObjectName("asGroup");
        asGroup->setChecked(items.size()==1&&first.type==ItemType::Group?true:positionAsGroup);asGroup->setEnabled(several);form->addRow(QString(),asGroup);
        auto *align=new QPushButton(ui("Ausrichten"));align->setObjectName("positionAlign");align->setEnabled(several&&!asGroup->isChecked());form->addRow(QString(),align);
        connect(align,&QPushButton::clicked,this,[this,align]{if(alignMenu)alignMenu->popup(align->mapToGlobal(QPoint(0,align->height())));});
        connect(asGroup,&QCheckBox::toggled,this,[this](bool on){positionAsGroup=on;QMetaObject::invokeMethod(this,[this]{refresh();},Qt::QueuedConnection);});
        auto apply=[=,this](int axis){
            if(refreshing)return;
            const auto sel=selection();if(sel.isEmpty())return;
            const QPointF target=onSheet(QPointF(x->value(),y->value()));const int index=reference->currentIndex();
            auto along=[axis](QPointF d){return axis==0?QPointF(d.x(),0):QPointF(0,d.y());};
            if((sel.size()==1||asGroup->isChecked())&&!sheetView->nestedSelection()&&!filtered()){
                QRectF b;for(const auto *i:sel)b=b.united(bounds(*i));
                const QPointF delta=along(target-refPoint(b,sel[0],index));if(delta.manhattanLength()<1e-9)return;
                sheetView->moveSelection(delta);return;
            }
            edit([=](Item &i){schematic::move(i,along(target-refPoint(bounds(i),&i,index)));});
        };
        connect(reference,&QComboBox::currentIndexChanged,this,[=,this](int index){
            const auto sel=selection();if(sel.isEmpty())return;QRectF b;
            if(sel.size()==1||asGroup->isChecked())for(const auto *i:sel)b=b.united(bounds(*i));else b=bounds(*sel[0]);
            const QPointF p=shown(refPoint(b,sel[0],index));refreshing=true;x->setValue(p.x());y->setValue(p.y());refreshing=false;});
        connect(x,&QDoubleSpinBox::valueChanged,this,[apply]{apply(0);});connect(y,&QDoubleSpinBox::valueChanged,this,[apply]{apply(1);});
    }

    // Component: fields, switches, extra texts and the contact list.
    if(items.size()==1&&first.type==ItemType::Component){
        auto *form=section(ui("Bauteil"));
        auto field=[&](const QString &label,const QString &value,const char *name,std::function<void(Item&,const QString&)> set,bool *visible=nullptr,const char *visibleName=nullptr,std::function<void(Item&,bool)> setVisible={}){
            auto *edit=new QLineEdit(value);edit->setObjectName(name);
            connect(edit,&QLineEdit::editingFinished,this,[this,edit,set]{const QString v=edit->text();edit->setModified(false);this->edit([v,set](Item &i){set(i,v);});});
            if(visible){
                auto *row=new QWidget;auto *h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);h->addWidget(edit,1);
                // "…": the extended text input for the field, as in the reference.
                auto *more=new QToolButton;more->setText(QStringLiteral("…"));more->setObjectName(QString::fromLatin1(name)+QStringLiteral("Dialog"));h->addWidget(more);
                const QString family=first.font.family;
                // The panel may be built anew while the dialog runs (a typed text taken, variables defined): after it
                // only copies are used, never the field.
                connect(more,&QToolButton::clicked,this,[this,edit,set,family]{
                    QString v=edit->text();const auto apply=set;
                    if(!extendedText||!extendedText(v,family))return;
                    this->edit([v,apply](Item &i){apply(i,v);});});
                auto *check=new QCheckBox(ui("sichtbar"));check->setObjectName(visibleName);check->setChecked(*visible);h->addWidget(check);
                connect(check,&QCheckBox::toggled,this,[this,setVisible](bool on){this->edit([on,setVisible](Item &i){setVisible(i,on);});});
                form->addRow(label,row);
            }else form->addRow(label,edit);
        };
        bool dv=first.designatorVisible,vv=first.valueVisible;
        field(ui("Bezeichner:"),first.designator,"designator",[](Item &i,const QString &v){i.designator=v;},&dv,"designatorVisible",[](Item &i,bool on){i.designatorVisible=on;});
        field(ui("Wert:"),first.value,"value",[](Item &i,const QString &v){i.value=v;},&vv,"valueVisible",[](Item &i,bool on){i.valueVisible=on;});
        auto check=[&](const QString &text,bool value,const char *name,std::function<void(Item&,bool)> set){
            auto *c=new QCheckBox(text);c->setObjectName(name);c->setChecked(value);form->addRow(QString(),c);
            connect(c,&QCheckBox::toggled,this,[this,set](bool on){this->edit([on,set](Item &i){set(i,on);});});
        };
        check(ui("automatisch &Nummerieren"),first.autoNumber,"autoNumber",[](Item &i,bool on){i.autoNumber=on;});
        check(ui("beim &Reinziehen nachfragen"),first.askValue,"askValue",[](Item &i,bool on){i.askValue=on;});
        check(ui("In Stückliste aufnehmen"),first.inPartsList,"inPartsList",[](Item &i,bool on){i.inPartsList=on;});
        for(int k=0;k<4;k++)
            field(ui("Zusatztext %1:").arg(k+1),k<first.extra.size()?first.extra[k]:QString(),QStringLiteral("extra%1").arg(k+1).toLatin1().constData(),
                  [k](Item &i,const QString &v){while(i.extra.size()<4)i.extra<<QString();i.extra[k]=v;});
        if(!preset){
            auto *editor=new QPushButton(ui("Bauteil-Editor"));editor->setObjectName("componentEditor");form->addRow(QString(),editor);
            connect(editor,&QPushButton::clicked,this,[this]{if(componentEditorRequested)componentEditorRequested();});
        }
        // Parent and child.
        if(!preset){auto *pf=section(ui("Parent-Child"));const Document &doc=*sheetView->document();const QString file;
            auto button=[&](const QString &text,const char *name,std::function<void()> run){
                auto *b=new QPushButton(text);b->setObjectName(name);pf->addRow(b);connect(b,&QPushButton::clicked,this,std::move(run));};
            auto listOf=[&](const QList<PlacedComponent> &list,const char *name){
                auto *view=new QListWidget;view->setObjectName(name);
                for(const auto &p:list){
                    auto *row=new QListWidgetItem(QStringLiteral("%1   %2 %3").arg(expandVariables(p.item->designator,{&doc,p.sheet,p.item,file}),ui("auf Blatt")).arg(p.sheet+1));
                    row->setData(Qt::UserRole,p.item->id);view->addItem(row);
                }
                view->setMaximumHeight(std::min(160,30+int(list.size())*22));pf->addRow(view);
                connect(view,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *row){if(showElement)showElement(row->data(Qt::UserRole).toString());});
            };
            if(items.size()==1&&first.parent){
                pf->addRow(new QLabel(ui("Status: Parent")));
                const auto children=childrenOf(doc,first.id);
                if(children.isEmpty())pf->addRow(new QLabel(ui("Kein CHILD definiert")));else listOf(children,"children");
                if(!children.isEmpty())button(ui("Erstelle Childliste (Kontaktspiegel)"),"childList",[this,id=first.id]{if(childListRequested)childListRequested(id);});
                button(ui("Entferne PARENT-Status"),"removeParent",[this,id=first.id,count=children.size()]{
                    if(count&&QMessageBox::question(this,ui("Parent-Child"),ui("Sollen alle vorhandenen Child-Verknüpfungen aufgelöst werden?"))!=QMessageBox::Yes)return;
                    if(change)change([id](Document &d){
                        std::function<void(QList<Item>&)> walk=[&](QList<Item> &list){for(auto &i:list){if(i.id==id)i.parent=false;if(i.parentId==id)i.parentId.clear();walk(i.children);}};
                        for(auto &s:d.sheets)walk(s.items);});});
            }else if(items.size()==1&&!first.parentId.isEmpty()){
                pf->addRow(new QLabel(ui("Status: Child")));
                const PlacedComponent parent=componentWithId(doc,first.parentId);
                if(parent.item)listOf({parent},"parentOf");
                button(ui("Entferne PARENT-Verknüpfung"),"unlinkParent",[this]{edit([](Item &i){i.parentId.clear();});});
            }else{
                button(ui("Setze PARENT-Status"),"setParent",[this]{edit([](Item &i){if(i.type==ItemType::Component){i.parent=true;i.parentId.clear();}});});
                button(ui("Verknüpfe mit PARENT..."),"linkParent",[this]{if(linkParentRequested)linkParentRequested();});
            }
        }
        // The contacts: name and the text shown, both editable.
        const auto list=contacts(first);
        if(!list.isEmpty()){
            auto *cf=section(ui("Bauteil-Kontakte"));
            auto *table=new QTableWidget(int(list.size()),2);table->setObjectName("contactList");
            table->setHorizontalHeaderLabels({ui("Name"),ui("Text")});table->verticalHeader()->hide();table->horizontalHeader()->setStretchLastSection(true);
            for(int r=0;r<list.size();r++){
                auto *n=new QTableWidgetItem(list[r]->name);n->setData(Qt::UserRole,list[r]->id);table->setItem(r,0,n);
                table->setItem(r,1,new QTableWidgetItem(list[r]->text));
            }
            table->setMinimumHeight(std::min(240,40+int(list.size())*24));cf->addRow(table);
            connect(table,&QTableWidget::itemChanged,this,[this,table](QTableWidgetItem *cell){
                if(refreshing)return;const QString id=table->item(cell->row(),0)->data(Qt::UserRole).toString();
                const QString v=cell->text();const bool isName=cell->column()==0;
                this->edit([id,v,isName](Item &component){for(auto &k:component.children)if(k.id==id){if(isName)k.name=v;else k.text=v;}});});
        }
    }
    // A contact (in the component editor): its name and text.
    if(items.size()==1&&first.type==ItemType::Contact){
        auto *form=section(ui("Bauteil-Kontakt"));
        auto *name=new QLineEdit(first.name);name->setObjectName("contactName");form->addRow(ui("Name:"),name);
        connect(name,&QLineEdit::editingFinished,this,[this,name]{const QString v=name->text();edit([v](Item &i){i.name=v;});});
    }
    // Net label.
    if(all([](const Item &i){return i.type==ItemType::NetLabel;})){
        auto *form=section(ui("Netzname"));
        auto *global=new QCheckBox(ui("Blattverweis (verbindet alle Blätter)"));global->setObjectName("global");global->setChecked(first.global);form->addRow(QString(),global);
        connect(global,&QCheckBox::toggled,this,[this](bool on){edit([on](Item &i){if(i.type==ItemType::NetLabel)i.global=on;});});
    }
    // Text of texts, labels and contacts.
    if(!preset&&items.size()==1&&(isText(first)||first.type==ItemType::TextBox)&&first.role==TextRole::Plain){
        auto *form=section(ui("Text"));
        auto *row=new QWidget;auto *h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);
        auto *more=new QToolButton;more->setText(QStringLiteral("…"));more->setObjectName("textDialog");
        QWidget *field=nullptr;
        if(first.type==ItemType::TextBox){
            // A text box's text over several lines, as the reference's memo; Strg+Einfg gives the special characters.
            auto *text=new MemoField(first.text,4,[this](const QString &v){edit([v](Item &i){i.text=v;});});text->setObjectName("text");h->addWidget(text,1);field=text;
            const QString family=first.font.family;offerSpecialCharacters(text,[family]{return family;});
            h->addWidget(more,0,Qt::AlignTop);
        }else{
            auto *text=new QLineEdit(first.text);text->setObjectName("text");h->addWidget(text,1);h->addWidget(more);field=text;
            connect(text,&QLineEdit::editingFinished,this,[this,text]{const QString v=text->text();if(v.trimmed().isEmpty()&&selection().value(0)&&selection()[0]->type==ItemType::NetLabel)return;edit([v](Item &i){i.text=v;});});
        }
        form->addRow(row);
        // What was typed in the field is taken first, so that the extended text input starts from it.
        connect(more,&QToolButton::clicked,this,[this,field,before=first.text]{
            QString typed;if(auto *line=qobject_cast<QLineEdit*>(field))typed=line->text();else if(auto *memo=qobject_cast<QPlainTextEdit*>(field))typed=memo->toPlainText();
            if(typed!=before)edit([typed](Item &i){i.text=typed;});
            if(textDialogRequested)textDialogRequested();});
    }
    // Links of a text: an external link, the target inside the document, whether it may be a target.
    if(!preset&&items.size()==1&&first.type==ItemType::Text&&first.role==TextRole::Plain&&sheetView->document()){
        const Document &doc=*sheetView->document();
        auto *form=section(ui("Text-Verlinkung"));
        auto *row=new QWidget;auto *h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);
        auto *external=new QLineEdit(first.link);external->setObjectName("link");h->addWidget(external,1);
        auto *file=new QToolButton;file->setText(ui("Datei..."));file->setObjectName("linkFile");h->addWidget(file);
        form->addRow(ui("Externer Link:"),row);
        connect(external,&QLineEdit::editingFinished,this,[this,external]{const QString v=external->text().trimmed();edit([v](Item &i){i.link=v;});});
        connect(file,&QToolButton::clicked,this,[this]{const QString f=QFileDialog::getOpenFileName(this,ui("Externer Link"));if(!f.isEmpty())edit([f](Item &i){i.link=QUrl::fromLocalFile(f).toString();});});
        auto entry=[&](const PlacedComponent &p){
            return QStringLiteral("%1   %2 %3").arg(expandVariables(p.item->text,{&doc,p.sheet,nullptr,{},p.item}),ui("auf Blatt")).arg(p.sheet+1);};
        auto listOf=[&](const QList<PlacedComponent> &list,const char *name){
            auto *view=new QListWidget;view->setObjectName(name);
            for(const auto &p:list){auto *r=new QListWidgetItem(entry(p));r->setData(Qt::UserRole,p.item->id);view->addItem(r);}
            view->setMaximumHeight(std::min(120,30+int(list.size())*22));
            connect(view,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *r){if(showElement)showElement(r->data(Qt::UserRole).toString());});
            return view;};
        const PlacedComponent target=textWithId(doc,first.linkTarget);
        if(target.item)form->addRow(ui("Ziel:"),listOf({target},"linkTarget"));
        else form->addRow(ui("Ziel:"),new QLabel(ui("** Kein Ziel definiert **")));
        auto *buttons=new QWidget;auto *bl=new QHBoxLayout(buttons);bl->setContentsMargins(0,0,0,0);
        auto *choose=new QPushButton(target.item?ui("Neues Ziel auswählen..."):ui("Ziel auswählen..."));choose->setObjectName("chooseTarget");bl->addWidget(choose);
        auto *remove=new QPushButton(ui("Ziel entfernen"));remove->setObjectName("removeTarget");remove->setEnabled(target.item!=nullptr);bl->addWidget(remove);
        form->addRow(buttons);
        connect(choose,&QPushButton::clicked,this,[this]{if(linkTargetRequested)linkTargetRequested();});
        connect(remove,&QPushButton::clicked,this,[this]{edit([](Item &i){i.linkTarget.clear();});});
        auto *linkable=new QCheckBox(ui("Als Ziel freigeben"));linkable->setObjectName("linkable");linkable->setChecked(first.linkable);form->addRow(linkable);
        connect(linkable,&QCheckBox::toggled,this,[this](bool on){edit([on](Item &i){i.linkable=on;});});
        // The writing turned by 180° inside the same frame, as the reference's "Textrichtung umkehren".
        auto *reversed=new QCheckBox(ui("Textrichtung umkehren"));reversed->setObjectName("reversed");reversed->setChecked(first.reversed);form->addRow(reversed);
        connect(reversed,&QCheckBox::toggled,this,[this](bool on){edit([on](Item &i){if(i.type==ItemType::Text)i.reversed=on;});});
        if(first.linkable){
            const auto sources=linksTo(doc,first.id);
            if(sources.isEmpty())form->addRow(ui("Als Ziel verwendet von:"),new QLabel(ui("** Nicht als Ziel verwendet **")));
            else form->addRow(ui("Als Ziel verwendet von:"),listOf(sources,"linkSources"));
        }
    }
    // Dimensions: their text and arrows, as the reference lists them.
    if(all([](const Item &i){return i.type==ItemType::Dimension;})){
        auto *form=section(ui("Bemaßung"));
        auto check=[&](const QString &label,bool on,const char *name,std::function<void(Item&,bool)> set){
            auto *c=new QCheckBox;c->setObjectName(name);c->setChecked(on);form->addRow(label,c);
            connect(c,&QCheckBox::toggled,this,[this,set](bool v){edit([v,set](Item &i){set(i,v);});});
        };
        auto line=[&](const QString &label,const QString &value,const char *name,std::function<void(Item&,const QString&)> set){
            auto *e=new QLineEdit(value);e->setObjectName(name);form->addRow(label,e);
            connect(e,&QLineEdit::editingFinished,this,[this,e,set]{const QString v=e->text();edit([v,set](Item &i){set(i,v);});});
        };
        check(ui("Zeige Ø:"),first.showDiameter,"showDiameter",[](Item &i,bool v){i.showDiameter=v;});
        line(ui("Präfix:"),first.prefix,"prefix",[](Item &i,const QString &v){i.prefix=v;});
        line(ui("Suffix:"),first.suffix,"suffix",[](Item &i,const QString &v){i.suffix=v;});
        line(ui("Obere Toleranz:"),first.upperTolerance,"upperTolerance",[](Item &i,const QString &v){i.upperTolerance=v;});
        line(ui("Untere Toleranz:"),first.lowerTolerance,"lowerTolerance",[](Item &i,const QString &v){i.lowerTolerance=v;});
        check(ui("Auto-Maßzahl:"),first.autoValue,"autoValue",[](Item &i,bool v){i.autoValue=v;});
        line(ui("Feste Maßzahl:"),first.fixedValue,"fixedValue",[](Item &i,const QString &v){i.fixedValue=v;});
        auto *angle=numberBox(first.arrowAngle,1,170,0);angle->setObjectName("arrowAngle");form->addRow(ui("Pfeilwinkel [°]:"),angle);
        connect(angle,&QDoubleSpinBox::valueChanged,this,[this](double v){edit([v](Item &i){i.arrowAngle=v;});});
        auto *length=numberBox(first.arrowLength,0,100,2,QStringLiteral(" mm"));length->setObjectName("arrowLength");form->addRow(ui("Pfeillänge:"),length);
        connect(length,&QDoubleSpinBox::valueChanged,this,[this](double v){edit([v](Item &i){i.arrowLength=v;});});
        auto *digits=new QComboBox;digits->setObjectName("digits");digits->addItems({QStringLiteral("0"),QStringLiteral("1"),QStringLiteral("2"),QStringLiteral("3")});
        digits->setCurrentIndex(std::clamp(first.digits,0,3));form->addRow(ui("Nachkommastellen:"),digits);
        connect(digits,&QComboBox::activated,this,[this](int v){edit([v](Item &i){i.digits=v;});});
        check(ui("Punkt statt Komma:"),first.decimalPoint,"decimalPoint",[](Item &i,bool v){i.decimalPoint=v;});
        for(auto [label,colourOf,name,extension]:{std::tuple{ui("Hilfslinie:"),first.extensionColor,"extensionColour",true},std::tuple{ui("Maßlinie:"),first.lineColor,"lineColour",false}}){
            auto *b=colourButton(colourOf);b->setObjectName(name);form->addRow(label,b);
            connect(b,&QToolButton::clicked,this,[this,b,extension=extension]{
                const QColor c=QColorDialog::getColor(b->property("colour").value<QColor>(),this);
                if(c.isValid())edit([c,extension](Item &i){(extension?i.extensionColor:i.lineColor)=c.toRgb();});});
        }
    }
    // Font of texts (and of dimensions).
    if(all([](const Item &i){return isText(i)||i.type==ItemType::TextBox||i.type==ItemType::Dimension;})){
        auto *form=section(ui("Schriftart"));
        auto *family=new QFontComboBox;family->setObjectName("fontFamily");family->setCurrentFont(QFont(first.font.family));form->addRow(ui("Font:"),family);
        connect(family,&QFontComboBox::currentFontChanged,this,[this](const QFont &f){const QString v=f.family();edit([v](Item &i){i.font.family=v;});});
        if(!all([](const Item &i){return i.type==ItemType::Dimension;})){
            auto *align=new QComboBox;align->setObjectName("align");align->addItems({ui("Linksbündig"),ui("Zentriert"),ui("Rechtsbündig")});align->setCurrentIndex(int(first.align));
            form->addRow(ui("Ausrichtung:"),align);
            connect(align,&QComboBox::activated,this,[this](int v){edit([v](Item &i){i.align=Align(v);});});
        }
        auto *height=numberBox(first.font.height*10,1,10000,1);height->setObjectName("textHeight");form->addRow(ui("Texthöhe [1/10 mm]:"),height);
        connect(height,&QDoubleSpinBox::valueChanged,this,[this](double v){edit([v](Item &i){i.font.height=v/10;});});
        auto *colour=colourButton(first.font.color);colour->setObjectName("textColour");form->addRow(ui("Farbe:"),colour);
        connect(colour,&QToolButton::clicked,this,[this,colour]{const QColor c=QColorDialog::getColor(colour->property("colour").value<QColor>(),this);if(c.isValid())edit([c](Item &i){i.font.color=c.toRgb();});});
        auto *bold=new QCheckBox;bold->setObjectName("bold");bold->setChecked(first.font.bold);form->addRow(ui("Fett:"),bold);
        connect(bold,&QCheckBox::toggled,this,[this](bool on){edit([on](Item &i){i.font.bold=on;});});
        auto *italic=new QCheckBox;italic->setObjectName("italic");italic->setChecked(first.font.italic);form->addRow(ui("Kursiv:"),italic);
        connect(italic,&QCheckBox::toggled,this,[this](bool on){edit([on](Item &i){i.font.italic=on;});});
        // "Normalisieren" as the reference: the text upright again (no turn or mirror), a text kept on the sheet; font
        // and size stay.
        if(!preset&&all([](const Item &i){return i.type==ItemType::Text||i.type==ItemType::Contact||i.type==ItemType::TextBox;})){
            auto *normal=new QPushButton(ui("Normalisieren"));normal->setObjectName("normalise");form->addRow(QString(),normal);
            const QSizeF sheet=sheetView->document()&&!sheetView->document()->sheets.isEmpty()?QSizeF(sheetView->document()->sheet().width,sheetView->document()->sheet().height):QSizeF();
            connect(normal,&QPushButton::clicked,this,[this,sheet]{edit([sheet](Item &i){
                i.rotation=0;if(i.type==ItemType::TextBox)return;
                i.mirrored=false;
                if(!sheet.isEmpty()){const QRectF r=bounds(i);
                    schematic::move(i,QPointF(std::clamp(r.left(),0.,std::max(0.,sheet.width()-r.width()))-r.left(),std::clamp(r.top(),0.,std::max(0.,sheet.height()-r.height()))-r.top()));}
            });});
        }
        if(all([](const Item &i){return i.type==ItemType::Text;})){
            auto *row=new QWidget;auto *h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);
            auto *back=new QCheckBox;back->setObjectName("background");back->setChecked(first.background);h->addWidget(back);
            auto *backColour=colourButton(first.backgroundColor);h->addWidget(backColour);h->addStretch();form->addRow(ui("Hintergrund:"),row);
            connect(back,&QCheckBox::toggled,this,[this](bool on){edit([on](Item &i){i.background=on;});});
            connect(backColour,&QToolButton::clicked,this,[this,backColour]{const QColor c=QColorDialog::getColor(backColour->property("colour").value<QColor>(),this);if(c.isValid())edit([c](Item &i){i.backgroundColor=c.toRgb();});});
        }
    }
    // Rectangle and ellipse sizes.
    if(!preset&&items.size()==1&&(first.type==ItemType::Rectangle||first.type==ItemType::Ellipse||first.type==ItemType::TextBox)){
        const bool ellipse=first.type==ItemType::Ellipse;
        auto *form=section(ellipse?ui("Kreis"):first.type==ItemType::TextBox?ui("Mengentext"):ui("Rechteck"));
        auto box=[&](const QString &label,double v,const char *name,std::function<void(Item&,double)> set,double low=-10000){
            auto *b=numberBox(v,low,10000,2,QStringLiteral(" mm"));b->setObjectName(name);form->addRow(label,b);
            connect(b,&QDoubleSpinBox::valueChanged,this,[this,set](double v){edit([v,set](Item &i){set(i,v);});});
        };
        box(ellipse?ui("X-Durchmesser:"):ui("Breite:"),first.size.width(),"boxWidth",[](Item &i,double v){i.size.setWidth(v);},0);
        box(ellipse?ui("Y-Durchmesser:"):ui("Höhe:"),first.size.height(),"boxHeight",[](Item &i,double v){i.size.setHeight(v);},0);
        box(ui("X-Mitte:"),first.centre.x(),"centreX",[](Item &i,double v){i.centre.setX(v);});
        box(ui("Y-Mitte:"),first.centre.y(),"centreY",[](Item &i,double v){i.centre.setY(v);});
        if(first.type==ItemType::Rectangle){
            auto *corner=numberBox(first.corner,0,50,0);corner->setObjectName("corner");form->addRow(ui("Fase [%]:"),corner);
            connect(corner,&QDoubleSpinBox::valueChanged,this,[this](double v){edit([v](Item &i){i.corner=v;if(v>0&&i.corners==Corners::Square)i.corners=Corners::Round;});});
            auto *style=new QComboBox;style->setObjectName("corners");style->addItems({ui("Eckig"),ui("Abgerundet"),ui("Abgeschrägt")});style->setCurrentIndex(int(first.corners));form->addRow(ui("Stil:"),style);
            connect(style,&QComboBox::activated,this,[this](int v){edit([v](Item &i){i.corners=Corners(v);});});
        }
        if(ellipse){
            box(ui("Startwinkel [°]:"),first.start,"arcStart",[](Item &i,double v){i.start=v;});
            box(ui("Stopwinkel [°]:"),first.stop,"arcStop",[](Item &i,double v){i.stop=v;});
            auto *style=new QComboBox;style->setObjectName("arc");style->addItems({ui("Ellipse"),ui("Bogen"),ui("Tortenstück"),ui("Sehne")});style->setCurrentIndex(int(first.arc));form->addRow(ui("Stil:"),style);
            connect(style,&QComboBox::activated,this,[this](int v){edit([v](Item &i){i.arc=ArcStyle(v);});});
        }
        if(first.type==ItemType::TextBox){
            auto *wrap=new QCheckBox(ui("Zeilenumbruch"));wrap->setObjectName("wrap");wrap->setChecked(first.wrap);form->addRow(QString(),wrap);
            connect(wrap,&QCheckBox::toggled,this,[this](bool on){edit([on](Item &i){i.wrap=on;});});
            auto *middle=new QCheckBox(ui("Vertikal zentriert"));middle->setObjectName("middle");middle->setChecked(first.middle);form->addRow(QString(),middle);
            connect(middle,&QCheckBox::toggled,this,[this](bool on){edit([on](Item &i){i.middle=on;});});
        }
    }
    // Picture: size, its pixels and what it takes, and the changes of the reference.
    if(items.size()==1&&first.type==ItemType::Image&&sheetView->document()){
        auto *form=section(ui("Bitmap"));
        auto box=[&](const QString &label,double v,const char *name,std::function<void(Item&,double)> set,double low=-10000){
            auto *b=numberBox(v,low,10000,2,QStringLiteral(" mm"));b->setObjectName(name);form->addRow(label,b);
            connect(b,&QDoubleSpinBox::valueChanged,this,[this,set](double v){edit([v,set](Item &i){set(i,v);});});
        };
        box(ui("Breite:"),first.size.width(),"boxWidth",[](Item &i,double v){i.size.setWidth(v);},0);
        box(ui("Höhe:"),first.size.height(),"boxHeight",[](Item &i,double v){i.size.setHeight(v);},0);
        box(ui("X-Mitte:"),first.centre.x(),"centreX",[](Item &i,double v){i.centre.setX(v);});
        box(ui("Y-Mitte:"),first.centre.y(),"centreY",[](Item &i,double v){i.centre.setY(v);});
        const ImageInfo info=imageInfo(*sheetView->document(),first);
        auto *pixels=new QLabel(ui("%1 × %2 Pixel").arg(info.pixels.width()).arg(info.pixels.height()));pixels->setObjectName("imagePixels");form->addRow(ui("Bitmap:"),pixels);
        auto *memory=new QLabel(ui("%1 KB").arg(uiLocale().toString(info.bytes/1024.,'f',1)));memory->setObjectName("imageMemory");form->addRow(ui("Speicherbedarf:"),memory);
        auto *dpi=new QLabel(ui("%1 dpi").arg(std::lround(info.dpi)));dpi->setObjectName("imageDpi");form->addRow(ui("Auflösung:"),dpi);
        auto *depth=new QLabel(info.bits>0?ui("%1 Bit").arg(info.bits):QStringLiteral("–"));depth->setObjectName("imageBits");form->addRow(ui("Farbtiefe:"),depth);
        auto button=[&](const QString &text,const char *name,std::function<void(Document&,Item&)> run){
            auto *b=new QPushButton(text);b->setObjectName(name);form->addRow(QString(),b);
            connect(b,&QPushButton::clicked,this,[this,run]{edit(run);});return b;
        };
        auto *reduce=button(ui("Auflösung verringern"),"reduceResolution",[](Document &d,Item &i){reduceResolution(d,i,1.4);});
        if(info.dpi>300)reduce->setStyleSheet(QStringLiteral("background-color:#ff6060"));   // marked red, as in the reference
        button(ui("Bitmap 90° rotieren"),"rotateImage",[](Document &d,Item &i){rotateImage(d,i);});
        button(ui("Normalisieren"),"normaliseImage",[](Document &d,Item &i){normaliseImage(d,i);});
        button(ui("Aufhellen"),"lightenImage",[](Document &d,Item &i){lightenImage(d,i);});
        auto *explorer=new QPushButton(ui("Bitmap-Explorer..."));explorer->setObjectName("bitmapExplorer");form->addRow(QString(),explorer);
        connect(explorer,&QPushButton::clicked,this,[this]{if(bitmapExplorerRequested)bitmapExplorerRequested();});
    }
    // Junction.
    if(all([](const Item &i){return i.type==ItemType::Junction;})){
        auto *form=section(ui("Lötpunkt"));
        // As the reference: with "Automatik" the size follows the lines under the junction in the step chosen (XS to XL,
        // choosing one switches it on), size and colour then show what is drawn and cannot be changed; "Reset" gives
        // 1.2 mm, black, without Automatik, step L.
        const bool automaticOn=first.autoSize;
        auto *automatic=new QCheckBox;automatic->setObjectName("junctionAuto");automatic->setChecked(automaticOn);form->addRow(ui("Automatik:"),automatic);
        connect(automatic,&QCheckBox::toggled,this,[this](bool on){edit([on](Item &i){i.autoSize=on;});});
        auto *step=new QComboBox;step->setObjectName("junctionStep");step->addItems({QStringLiteral("XS"),QStringLiteral("S"),QStringLiteral("M"),QStringLiteral("L"),QStringLiteral("XL")});
        step->setCurrentIndex(std::clamp(first.sizeStep,0,4));step->setEnabled(automaticOn);form->addRow(ui("Auto-Size:"),step);
        connect(step,&QComboBox::activated,this,[this](int v){edit([v](Item &i){i.autoSize=true;i.sizeStep=v;});});
        std::pair<double,QColor> look{first.size.width(),first.pen.color};
        if(automaticOn&&!preset&&sheetView->document()&&!sheetView->document()->sheets.isEmpty())look=junctionLook(first,sheetView->document()->sheet());
        auto *size=numberBox(look.first*10,1,1000,1);size->setObjectName("junctionSize");size->setEnabled(!automaticOn);form->addRow(ui("Größe [1/10 mm]:"),size);
        connect(size,&QDoubleSpinBox::valueChanged,this,[this](double v){edit([v](Item &i){i.size=QSizeF(v/10,v/10);});});
        auto *colour=colourButton(look.second);colour->setObjectName("junctionColour");colour->setEnabled(!automaticOn);form->addRow(ui("Farbe:"),colour);
        connect(colour,&QToolButton::clicked,this,[this,colour]{const QColor c=QColorDialog::getColor(colour->property("colour").value<QColor>(),this);if(c.isValid())edit([c](Item &i){i.pen.color=c.toRgb();});});
        auto *reset=new QPushButton(ui("Reset"));reset->setObjectName("junctionReset");form->addRow(QString(),reset);
        connect(reset,&QPushButton::clicked,this,[this]{edit([](Item &i){i.size=QSizeF(1.2,1.2);i.autoSize=false;i.sizeStep=3;i.pen.color=QColor(0,0,0);});});
    }
    // Outline of lines and shapes.
    const auto drawn=[](const Item &i){return i.type==ItemType::Line||i.type==ItemType::Polygon||i.type==ItemType::Bezier||i.type==ItemType::Rectangle||i.type==ItemType::Ellipse||i.type==ItemType::TextBox;};
    if(all(drawn)){
        auto *form=section(ui("Umriss"));
        auto *style=new QComboBox;style->setObjectName("penStyle");
        style->addItems({ui("Durchgezogen"),ui("Gestrichelt"),ui("Gepunktet"),ui("Strich-Punkt"),ui("Strich-Punkt-Punkt")});
        style->setCurrentIndex(first.pen.style==PenStyle::None?0:int(first.pen.style));form->addRow(ui("Stil:"),style);
        connect(style,&QComboBox::activated,this,[this](int v){edit([v](Item &i){i.pen.style=PenStyle(v);});});
        auto *width=numberBox(first.pen.width*10,0,1000,1);width->setObjectName("penWidth");form->addRow(ui("Breite [1/10 mm]:"),width);
        connect(width,&QDoubleSpinBox::valueChanged,this,[this](double v){edit([v](Item &i){i.pen.width=v/10;});});
        auto *colour=colourButton(first.pen.color);colour->setObjectName("penColour");form->addRow(ui("Farbe:"),colour);
        connect(colour,&QToolButton::clicked,this,[this,colour]{const QColor c=QColorDialog::getColor(colour->property("colour").value<QColor>(),this);if(c.isValid())edit([c](Item &i){i.pen.color=c.toRgb();});});
        // Second colour and stripes, each switched on with its colour beside it; text frames have none.
        const bool striped=!any([](const Item &i){return i.type==ItemType::TextBox;});
        auto stripe=[&](const QString &label,bool on,const QColor &colourOf,const char *name,bool Pen::*flag,QColor Pen::*colourField){
            auto *row=new QWidget;auto *h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);
            auto *check=new QCheckBox;check->setObjectName(name);check->setChecked(on);h->addWidget(check);
            auto *button=colourButton(colourOf);button->setObjectName(QByteArray(name)+"Colour");h->addWidget(button);h->addStretch();form->addRow(label,row);
            connect(check,&QCheckBox::toggled,this,[this,flag](bool v){edit([v,flag](Item &i){i.pen.*flag=v;});});
            connect(button,&QToolButton::clicked,this,[this,button,colourField]{
                const QColor c=QColorDialog::getColor(button->property("colour").value<QColor>(),this);
                if(c.isValid())edit([c,colourField](Item &i){i.pen.*colourField=c.toRgb();});});
        };
        if(striped)stripe(ui("Zweifarbig:"),first.pen.twoColour,first.pen.color2,"twoColour",&Pen::twoColour,&Pen::color2);
        if(!any([](const Item &i){return i.type==ItemType::Line||i.type==ItemType::Bezier;})){
            auto *none=new QCheckBox;none->setObjectName("noOutline");none->setChecked(first.pen.style==PenStyle::None);form->addRow(ui("kein Umriss:"),none);
            connect(none,&QCheckBox::toggled,this,[this](bool on){edit([on](Item &i){i.pen.style=on?PenStyle::None:PenStyle::Solid;});});
        }
        if(striped){
            stripe(ui("Längsstreifen:"),first.pen.inner,first.pen.innerColor,"inner",&Pen::inner,&Pen::innerColor);
            stripe(ui("Querstreifen:"),first.pen.cross,first.pen.crossColor,"cross",&Pen::cross,&Pen::crossColor);
        }
        // Length (in the sheet's scale; a polygon all round) and nodes of a single line, curve or polygon.
        if(!preset&&items.size()==1&&(first.type==ItemType::Line||first.type==ItemType::Polygon||first.type==ItemType::Bezier)&&sheetView->document()){
            const Sheet &sheet=sheetView->document()->sheet();QPainterPath p=path(first);
            auto *length=new QLabel(QStringLiteral("%1 %2").arg(uiLocale().toString(p.length()*sheet.scale,'f',1),unitName(sheet.scaleUnit)));length->setObjectName("lineLength");
            form->addRow(ui("Länge:"),length);
            auto *nodes=new QLabel(QString::number(first.points.size()));nodes->setObjectName("nodeCount");form->addRow(ui("Knoten:"),nodes);
        }
        // "Voreinstellungen": the standard outline, the named ones from the settings, adding and removing them.
        auto *presets=new QToolButton;presets->setObjectName("linePresets");presets->setText(ui("Voreinstellungen"));presets->setPopupMode(QToolButton::InstantPopup);
        auto *menu=new QMenu(presets);presets->setMenu(menu);
        connect(menu,&QMenu::aboutToShow,this,[this,menu]{
            menu->clear();
            menu->addAction(ui("Standard"),this,[this]{applyPen(Pen());});
            menu->addSeparator();
            const QStringList names=linePresets();
            for(const QString &n:names)menu->addAction(n,this,[this,n]{applyLinePreset(n);});
            if(!names.isEmpty())menu->addSeparator();
            menu->addAction(ui("Hinzufügen..."),this,[this]{
                bool ok=false;const QString name=QInputDialog::getText(this,ui("Voreinstellungen"),ui("Name:"),QLineEdit::Normal,QString(),&ok).trimmed();
                if(ok&&!name.isEmpty()&&!addLinePreset(name))QMessageBox::information(this,ui("Voreinstellungen"),ui("Es sind nicht alle Eigenschaften definiert."));});
            auto *remove=menu->addMenu(ui("Entfernen"));remove->setEnabled(!names.isEmpty());
            for(const QString &n:names)remove->addAction(n,this,[n]{removeLinePreset(n);});
        });
        form->addRow(QString(),presets);
        if(all([](const Item &i){return i.type==ItemType::Line;})&&!inComponentEditor&&!sheetView->titleBlockMode()){
            auto *electrical=new QCheckBox(ui("Elektrische Verbindung"));electrical->setObjectName("electrical");electrical->setChecked(first.electrical);form->addRow(QString(),electrical);
            connect(electrical,&QCheckBox::toggled,this,[this](bool on){edit([on](Item &i){i.electrical=on;});});
        }
    }
    // Line ends.
    if(all([](const Item &i){return i.type==ItemType::Line||i.type==ItemType::Bezier;})){
        auto *form=section(ui("Linienenden"));
        auto *start=lineEndBox(first.startEnd,true);start->setObjectName("startEnd");form->addRow(ui("Start-Stil:"),start);
        connect(start,&QComboBox::activated,this,[this](int v){edit([v](Item &i){i.startEnd=LineEnd(v);});});
        auto *end=lineEndBox(first.endEnd,false);end->setObjectName("endEnd");form->addRow(ui("End-Stil:"),end);
        connect(end,&QComboBox::activated,this,[this](int v){edit([v](Item &i){i.endEnd=LineEnd(v);});});
        auto *size=numberBox(first.endSize,.1,100,1,QStringLiteral(" mm"));size->setObjectName("endSize");form->addRow(ui("Pfeilgröße:"),size);
        connect(size,&QDoubleSpinBox::valueChanged,this,[this](double v){edit([v](Item &i){i.endSize=v;});});
    }
    // Fill of closed shapes.
    if(all([](const Item &i){return i.type==ItemType::Polygon||i.type==ItemType::Rectangle||i.type==ItemType::Ellipse||i.type==ItemType::TextBox;})){
        auto *form=section(ui("Füllung"));
        auto *none=new QCheckBox;none->setObjectName("noFill");none->setChecked(first.fill.style==FillStyle::None);form->addRow(ui("keine Füllung:"),none);
        // The styles in the reference's order.
        static const QList<FillStyle> styles{FillStyle::Solid,FillStyle::Horizontal,FillStyle::Vertical,FillStyle::BackDiagonal,FillStyle::Diagonal,FillStyle::Cross,FillStyle::DiagonalCross};
        auto *style=new QComboBox;style->setObjectName("fillStyle");
        style->addItems({ui("Voll"),ui("Waagerecht"),ui("Senkrecht"),ui("Diagonal fallend"),ui("Diagonal steigend"),ui("Kariert"),ui("Diagonal kariert")});
        style->setCurrentIndex(std::max(0,int(styles.indexOf(first.fill.style))));form->addRow(ui("Stil:"),style);
        connect(none,&QCheckBox::toggled,this,[this,style](bool on){const FillStyle s=styles.value(style->currentIndex());edit([on,s](Item &i){i.fill.style=on?FillStyle::None:s;});});
        connect(style,&QComboBox::activated,this,[this](int v){const FillStyle s=styles.value(v);edit([s](Item &i){i.fill.style=s;});});
        auto *colour=colourButton(first.fill.color);colour->setObjectName("fillColour");form->addRow(ui("Farbe:"),colour);
        connect(colour,&QToolButton::clicked,this,[this,colour]{const QColor c=QColorDialog::getColor(colour->property("colour").value<QColor>(),this);if(c.isValid())edit([c](Item &i){i.fill.color=c.toRgb();});});
        auto *spacing=numberBox(first.fill.spacing*10,.5,10000,1);spacing->setObjectName("fillSpacing");form->addRow(ui("Linienabstand [1/10 mm]:"),spacing);
        connect(spacing,&QDoubleSpinBox::valueChanged,this,[this](double v){edit([v](Item &i){i.fill.spacing=v/10;});});
        auto *lineWidth=numberBox(first.fill.lineWidth*10,0,1000,1);lineWidth->setObjectName("fillLineWidth");form->addRow(ui("Linienstärke [1/10 mm]:"),lineWidth);
        connect(lineWidth,&QDoubleSpinBox::valueChanged,this,[this](double v){edit([v](Item &i){i.fill.lineWidth=v/10;});});
    }
}
}
