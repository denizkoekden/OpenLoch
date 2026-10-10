#include "editor.h"
#include "dialogs.h"
#include "images.h"
#include "library.h"
#include "numbering.h"
#include "search.h"
#include "print.h"
#include "properties.h"
#include "render.h"
#include "svg.h"
#include "emf.h"
#include "dimension.h"
#include "schematicicons.h"
#include "language.h"
#include "legacy_reader.h"
#include "formats/splan/splan.h"
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QImageReader>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
#include <tuple>

namespace openloch::schematic {
namespace {
// A label of the status bar that does something when clicked, as the reference's fields do.
class ClickableLabel : public QLabel {
public:
    using QLabel::QLabel;
    std::function<void()> clicked;
protected:
    void mousePressEvent(QMouseEvent *event) override{if(event->button()==Qt::LeftButton&&clicked)clicked();else QLabel::mousePressEvent(event);}
};
constexpr int undoSteps=200;
const char *const clipboardType="application/x-openloch-schematic";
}

QList<std::pair<QString,QStringList>> editorMenus(){
    return {{QStringLiteral("&Datei"),{"new","open","save","saveAs","","templateNew","templateSave","","explorer","","insertImage","exportImage","copyImage","exportSplan8","exportSplan7","","print","","fullScreen","","quit"}},
            {QStringLiteral("B&auteileditor"),{"componentEditorSave","componentEditorCancel"}},
            {QStringLiteral("&Bearbeiten"),{"undo","redo","","cut","copy","paste","duplicate","delete","","selectAll","search"}},
            {QStringLiteral("B&latt"),{"sheetProperties","","newSheet","copySheet","deleteSheet","","sortSheets","","gotoSheet","","loadSheet","saveSheet"}},
            {QStringLiteral("&Formblatt"),{"editTitleBlock","","loadTitleBlock","saveTitleBlock","","generateTitleBlock","removeTitleBlock"}},
            {QStringLiteral("&Funktionen"),{"toFront","toBack","oneForward","oneBack","","alignGrid","","rotate","rotateBy","scale","mirror","mirrorVertical","","group","ungroup","","magnetLines","","joinLines","","fixDimensions","unfixDimensions","","renumber","partsList","parentChildList","contactList","linkList","","colourize","penWidth","contents","bitmapExplorer"}},
            {QStringLiteral("B&auteil"),{"makeComponent","dissolveComponent","copyToLibrary","lettering","","componentEditor"}},
            {QStringLiteral("&Einstellungen"),{"settings","grid","","textConstants","variables","","presets"}},
            {QStringLiteral("&Hilfe"),{"helpTopics","","about"}}};
}

Editor::Editor(QWidget *parent):QMainWindow(parent){
    doc=schematic::newDocument(ui("Blatt %1").arg(1));
    sheetView=new SheetView;sheetView->setObjectName("sheetView");
    libraryPanel=new LibraryPanel;libraryPanel->setObjectName("library");
    propertiesPanel=new PropertiesPanel;propertiesPanel->setObjectName("properties");propertiesPanel->setView(sheetView);
    tabs=new QTabBar;tabs->setObjectName("sheetTabs");tabs->setShape(QTabBar::RoundedSouth);tabs->setMovable(true);tabs->setDocumentMode(true);tabs->setExpanding(false);
    tabs->setContextMenuPolicy(Qt::CustomContextMenu);tabs->setMinimumHeight(tabs->fontMetrics().height()+10);tabs->setDrawBase(false);
    createActions();
    // The panels at the right as in the reference, each shown or hidden from the second toolbar and set free with its
    // title bar: the properties, and the sheets as a list (hidden at first).
    propertiesDock=new QDockWidget(ui("Eigenschaften"),this);propertiesDock->setObjectName("propertiesDock");propertiesDock->setWidget(propertiesPanel);
    addDockWidget(Qt::RightDockWidgetArea,propertiesDock);
    sheetsDock=new QDockWidget(ui("Blätter"),this);sheetsDock->setObjectName("sheetsDock");
    {auto *w=new QWidget;auto *l=new QVBoxLayout(w);l->setContentsMargins(0,0,0,0);
        sheetList=new QTableWidget(0,1);sheetList->setObjectName("sheetList");sheetList->horizontalHeader()->hide();sheetList->horizontalHeader()->setStretchLastSection(true);
        sheetList->setSelectionBehavior(QAbstractItemView::SelectRows);sheetList->setSelectionMode(QAbstractItemView::SingleSelection);sheetList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        // The numbers at the left are dragged to reorder the sheets.
        sheetList->verticalHeader()->setSectionsMovable(true);sheetList->setContextMenuPolicy(Qt::CustomContextMenu);
        l->addWidget(sheetList,1);
        auto *contents=new QPushButton(ui("Inhaltsverzeichnis einfügen..."));contents->setObjectName("sheetListContents");l->addWidget(contents);
        connect(contents,&QPushButton::clicked,this,[this]{action("contents")->trigger();});
        sheetsDock->setWidget(w);}
    addDockWidget(Qt::RightDockWidgetArea,sheetsDock);sheetsDock->hide();
    for(auto [dock,name,icon,tip]:{std::tuple{propertiesDock,"showProperties","sch-properties",ui("Eigenschaften ein/aus")},std::tuple{sheetsDock,"showSheetList","sch-pages",ui("Blattliste ein/aus")}}){
        auto *a=dock->toggleViewAction();a->setObjectName(name);a->setIcon(schematicIcon(icon));a->setToolTip(tip);actions.insert(name,a);}
    connect(sheetList,&QTableWidget::currentCellChanged,this,[this](int row){if(!refreshing&&row>=0)switchSheet(row);});
    connect(sheetList->verticalHeader(),&QHeaderView::sectionMoved,this,[this]{
        if(refreshing)return;
        QList<int> order;for(int v=0;v<sheetList->rowCount();v++)order<<sheetList->verticalHeader()->logicalIndex(v);
        reorderSheets(order);});
    connect(sheetList,&QTableWidget::customContextMenuRequested,this,[this](QPoint at){
        // As the reference's list: properties, delete, a new sheet or a copy here, first or last.
        const int row=sheetList->rowAt(at.y());if(row>=0)switchSheet(row);
        const int here=doc.activeSheet;QMenu menu(this);
        menu.addAction(action("sheetProperties"));menu.addSeparator();menu.addAction(action("deleteSheet"));
        auto *fresh=menu.addMenu(ui("Neues Blatt"));
        fresh->addAction(ui("Hier einfügen"),this,[this,here]{insertSheets(here,1,false);});
        fresh->addAction(ui("Als erstes Blatt"),this,[this]{insertSheets(0,1,false);});
        fresh->addAction(ui("Als letztes Blatt"),this,[this]{insertSheets(int(doc.sheets.size()),1,false);});
        auto *copy=menu.addMenu(ui("Blatt kopieren"));
        copy->addAction(ui("Hier einfügen"),this,[this,here]{insertSheets(here,1,true,here);});
        copy->addAction(ui("Als letztes Blatt"),this,[this,here]{insertSheets(int(doc.sheets.size()),1,true,here);});
        copy->addAction(ui("Mehr..."),this,[this]{insertSheetDialog(true);});
        menu.exec(sheetList->viewport()->mapToGlobal(at));});

    // The bar shown while the title block or a component is edited, with its buttons.
    modeBar=new QWidget;modeBar->setObjectName("modeBar");modeBar->setStyleSheet(QStringLiteral("#modeBar{background:#cfe3ff}"));
    auto *mb=new QHBoxLayout(modeBar);mb->setContentsMargins(8,3,3,3);
    modeTitle=new QLabel;mb->addWidget(modeTitle,1);
    modeOk=new QPushButton;modeOk->setObjectName("modeOk");mb->addWidget(modeOk);
    modeCancel=new QPushButton(ui("Abbrechen"));modeCancel->setObjectName("modeCancel");mb->addWidget(modeCancel);
    modeBar->hide();
    connect(modeOk,&QPushButton::clicked,this,[this]{if(editingComponent())leaveComponentEditor(true);else setTitleBlockMode(false);});
    connect(modeCancel,&QPushButton::clicked,this,[this]{leaveComponentEditor(false);});
    // The partial preview of a sPlan file: what OpenLoch shows differently or leaves out.
    noticeBar=new QWidget;noticeBar->setObjectName("noticeBar");noticeBar->setStyleSheet(QStringLiteral("#noticeBar{background:#fff4c2}"));
    auto *nb=new QHBoxLayout(noticeBar);nb->setContentsMargins(8,3,3,3);
    noticeText=new QLabel;noticeText->setObjectName("noticeText");noticeText->setWordWrap(true);nb->addWidget(noticeText,1);
    auto *noticeClose=new QToolButton;noticeClose->setText(QStringLiteral("×"));noticeClose->setAutoRaise(true);nb->addWidget(noticeClose,0,Qt::AlignTop);
    connect(noticeClose,&QToolButton::clicked,noticeBar,&QWidget::hide);
    noticeBar->hide();

    // Library | drawing modes | sheet with tabs | properties
    auto *centre=new QWidget;auto *cl=new QVBoxLayout(centre);cl->setContentsMargins(0,0,0,0);cl->setSpacing(0);
    cl->addWidget(modeBar);cl->addWidget(noticeBar);cl->addWidget(sheetView,1);
    auto *tabRow=new QWidget;auto *tl=new QHBoxLayout(tabRow);tl->setContentsMargins(0,0,0,0);tl->setSpacing(0);
    auto *gotoButton=new QToolButton;gotoButton->setObjectName("gotoSheetButton");gotoButton->setText(QStringLiteral("▴"));gotoButton->setToolTip(ui("Gehe zu Blatt"));gotoButton->setAutoRaise(true);
    gotoButton->setMenu(new QMenu(gotoButton));gotoButton->setPopupMode(QToolButton::InstantPopup);
    connect(gotoButton->menu(),&QMenu::aboutToShow,this,[this,gotoButton]{
        auto *m=gotoButton->menu();m->clear();for(int i=0;i<doc.sheets.size();i++)m->addAction(QStringLiteral("%1  %2").arg(i+1).arg(doc.sheets[i].name),this,[this,i]{switchSheet(i);});});
    // "Neues Blatt" quickly, as in the reference: an empty one or a copy of a sheet, at the end.
    auto *quickSheet=new QToolButton;quickSheet->setObjectName("quickSheet");quickSheet->setText(QStringLiteral("+"));quickSheet->setToolTip(ui("Blatt schnell hinzufügen"));quickSheet->setAutoRaise(true);
    quickSheet->setMenu(new QMenu(quickSheet));quickSheet->setPopupMode(QToolButton::InstantPopup);
    quickSheet->menu()->addAction(ui("Leeres Blatt"),this,[this]{insertSheets(int(doc.sheets.size()),1,false);})->setObjectName("emptySheet");
    {auto *copies=quickSheet->menu()->addMenu(ui("Kopie von"));copies->setObjectName("copyOf");
        // The list of sheets anew each time; the menus themselves stay.
        connect(quickSheet->menu(),&QMenu::aboutToShow,this,[this,copies]{
            copies->clear();
            for(int i=0;i<doc.sheets.size();i++)copies->addAction(QStringLiteral("%1  %2").arg(i+1).arg(doc.sheets[i].name),this,[this,i]{insertSheets(int(doc.sheets.size()),1,true,i);});});}
    tl->addWidget(gotoButton);tl->addWidget(quickSheet);tl->addWidget(tabs,1);
    cl->addWidget(tabRow);
    createToolBars();
    auto *middle=new QWidget;auto *ml=new QHBoxLayout(middle);ml->setContentsMargins(0,0,0,0);ml->setSpacing(0);
    ml->addWidget(modes);ml->addWidget(centre,1);
    auto *split=new QSplitter;split->addWidget(libraryPanel);split->addWidget(middle);
    searchPanel=new SearchPanel;searchPanel->setObjectName("search");searchPanel->hide();split->addWidget(searchPanel);
    split->setStretchFactor(0,0);split->setStretchFactor(1,1);split->setStretchFactor(2,0);split->setSizes({200,900,260});
    resizeDocks({propertiesDock},{260},Qt::Horizontal);
    setCentralWidget(split);
    createMenus();
    statusBar()->addWidget(createStatusBar(),1);

    // The view and the panels.
    sheetView->beforeChange=[this]{snapshot();};
    sheetView->changed=[this]{touched();};
    // A drag taken back with Esc leaves no step to undo.
    sheetView->changeCancelled=[this]{if(!past.isEmpty())doc=past.takeLast();refresh();};
    sheetView->selectionChanged=[this]{propertiesPanel->showPreset(nullptr,{},false);if(propertiesPanel->showsLibraryEntry())propertiesPanel->clearLibraryEntry();else propertiesPanel->refresh();refreshActions();};
    sheetView->toolChanged=[this](SheetView::Tool t){
        for(auto *a:toolGroup->actions())if(a->data().toInt()==int(t))a->setChecked(true);
        propertiesPanel->showPreset(nullptr,{},false);
        if(propertiesPanel->showsLibraryEntry())propertiesPanel->clearLibraryEntry();
        else if(sheetView->selection().isEmpty())propertiesPanel->refresh();};
    sheetView->textRequested=[this](Item &i){return askText(i);};
    sheetView->editRequested=[this](const QString &id){editItem(id);};
    sheetView->contextMenuRequested=[this](QPoint at,const QString &id,int node){showContextMenu(at,id,node);};
    sheetView->hintChanged=[this](const QString &h){hint->setText(h);};
    // A double click on a link's arrow follows it; on a target's arrow it goes to the first text linking there.
    sheetView->linkArrowActivated=[this](const QString &id,bool outgoing){
        if(outgoing){followLink(id);return;}
        if(const auto origins=linksTo(doc,id);!origins.isEmpty())showElement(origins.first().item->id);};
    sheetView->pointerMoved=[this](QPointF p){
        // In the sheet's scale.
        const auto l=uiLocale();const double f=doc.sheet().scale;const QPointF d=(p-sheetView->pressPoint())*f;p=(p-sheetView->originPoint())*f;
        coordinates->setText(QStringLiteral("X: %1\nY: %2").arg(l.toString(p.x(),'f',2),l.toString(p.y(),'f',2)));
        relative->setText(QStringLiteral("dX: %1\ndY: %2").arg(l.toString(d.x(),'f',2),l.toString(d.y(),'f',2)));
        zoomLabel->setText(ui("Zoom: %1").arg(l.toString(sheetView->zoom(),'f',2)));};
    sheetView->zoomChanged=[this]{if(zoomLabel)zoomLabel->setText(ui("Zoom: %1").arg(uiLocale().toString(sheetView->zoom(),'f',2)));};
    // A symbol brings its pictures into the document.
    auto pictures=[this](const LibraryEntry &e){for(auto it=e.resources.cbegin();it!=e.resources.cend();++it)if(!doc.resources.contains(it.key()))doc.resources.insert(it.key(),it.value());};
    // A symbol, and a parent with the children the library keeps with it (as in sPlan, placed together and linked).
    sheetView->libraryItems=[this,pictures](const QString &key,QList<Item> *out){
        LibraryEntry e;if(!libraryPanel->entry(key,&e))return false;
        *out=placedItems(e,doc);Item &first=(*out)[0];
        if(first.type==ItemType::Component&&first.askValue){bool ok;const QString v=QInputDialog::getText(this,ui("Wert"),ui("Wert:"),QLineEdit::Normal,first.value,&ok);if(!ok)return false;first.value=v;}
        pictures(e);return true;};
    libraryPanel->chosen=[this,pictures](const LibraryEntry &e){
        QList<Item> items=placedItems(e,doc);Item &first=items[0];
        if(first.type==ItemType::Component&&first.askValue){bool ok;const QString v=QInputDialog::getText(this,ui("Wert"),ui("Wert:"),QLineEdit::Normal,first.value,&ok);if(!ok)return;first.value=v;}
        pictures(e);
        sheetView->setTool(SheetView::Tool::Select);sheetView->beginPlacement(items);};
    libraryPanel->setPages(builtInPages());libraryPanel->showPage(QStringLiteral("Elektro/Elektronik/Bauteile|Widerstände"));
    libraryPanel->copyToSheets=[this](const QList<int> &pages){librarySheets(pages);};
    // A symbol's properties in the panel; each change is kept on its page at once.
    libraryPanel->entryPropertiesRequested=[this](int index){
        const int p=libraryPanel->currentPage();if(p<0||index<0||index>=libraryPanel->pages()[p].entries.size())return;
        const LibraryEntry e=libraryPanel->pages()[p].entries[index];const QString key=libraryPanel->pageKey(p);
        propertiesPanel->libraryEntryChanged=[this,key,index](Item &symbol,QString &caption){
            if(libraryPanel->pageKey(libraryPanel->currentPage())!=key&&!libraryPanel->showPage(key))return;
            const auto &entries=libraryPanel->pages()[libraryPanel->currentPage()].entries;if(index>=entries.size())return;
            LibraryEntry changed=entries[index];changed.symbol=symbol;changed.caption=caption;
            QString error;if(!libraryPanel->replaceEntry(index,changed,&error)){QMessageBox::warning(this,ui("Bibliothek"),error);return;}
            // As stored (a sPlan page gives its entries new bytes).
            const LibraryEntry &now=libraryPanel->pages()[libraryPanel->currentPage()].entries[index];symbol=now.symbol;caption=now.caption;};
        propertiesPanel->showLibraryEntry(e.symbol,e.caption,libraryPanel->writable(p));};
    libraryPanel->reload=[this]{const QString key=libraryPanel->pageKey(libraryPanel->currentPage());if(preferences)reloadLibrary();else libraryPanel->setPages(builtInPages());libraryPanel->showPage(key);};
    propertiesPanel->change=[this](const std::function<void(Document&)> &edit){change(edit);};
    propertiesPanel->componentEditorRequested=[this]{editComponent();};
    propertiesPanel->showElement=[this](const QString &id){showElement(id);};
    propertiesPanel->linkParentRequested=[this]{linkToParent();};
    propertiesPanel->linkTargetRequested=[this]{chooseLinkTarget();};
    propertiesPanel->bitmapExplorerRequested=[this]{bitmapExplorer();};
    propertiesPanel->childListRequested=[this](const QString &id){childListDialog(id);};
    searchPanel->document=[this]{return &doc;};searchPanel->fileName=[this]{return path;};
    searchPanel->showElement=[this](const QString &id){showElement(id);};
    searchPanel->change=[this](const std::function<void(Document&)> &edit){change(edit);};
    propertiesPanel->titleBlockRequested=[this]{titleBlockDialog();};
    propertiesPanel->textDialogRequested=[this]{textDialog();};
    propertiesPanel->extendedText=[this](QString &text,const QString &family){return extendedText(text,family);};
    connect(tabs,&QTabBar::currentChanged,this,[this](int i){if(!refreshing&&i>=0)switchSheet(i);});
    connect(tabs,&QTabBar::tabMoved,this,[this](int from,int to){
        if(refreshing)return;
        QList<int> order;for(int i=0;i<doc.sheets.size();i++)order<<i;order.move(from,to);reorderSheets(order);});
    connect(tabs,&QTabBar::customContextMenuRequested,this,[this](QPoint at){showSheetMenu(tabs->mapToGlobal(at),tabs->tabAt(at));});

    setDocument(doc);resize(1400,900);
}

// --- actions, menus and bars
void Editor::createActions(){
    auto add=[this](const QString &name,const QString &text,const QString &icon,QKeySequence key,std::function<void()> run){
        auto *a=new QAction(icon.isEmpty()?QIcon():schematicIcon(icon),text,this);a->setObjectName(name);if(!key.isEmpty())a->setShortcut(key);
        a->setShortcutContext(Qt::WindowShortcut);addAction(a);connect(a,&QAction::triggered,this,std::move(run));actions.insert(name,a);return a;
    };
    // Datei
    add("new",ui("&Neu"),"new",QKeySequence::New,[this]{if(maybeSave())newDocument();});
    add("open",ui("&Öffnen..."),"open",QKeySequence::Open,[this]{
        if(openHandler){openHandler(general.drawingFolder);return;}
        if(!maybeSave())return;
        const auto file=QFileDialog::getOpenFileName(this,ui("Schaltplan öffnen"),startFolder(Work::Drawings,path.isEmpty()?QString():QFileInfo(path).absolutePath()),
            ui("Schaltpläne (*.olsch *.spl8 *.spl7);;OpenLoch-Schaltpläne (*.olsch);;sPlan (*.spl8 *.spl7);;Alle Dateien (*)"));usedFolder(Work::Drawings,file);
        QString error;if(!file.isEmpty()&&!openFile(file,&error))QMessageBox::warning(this,ui("Schaltplan öffnen"),error);});
    add("save",ui("&Speichern"),"save",QKeySequence::Save,[this]{save();});
    add("saveAs",ui("Speichern &unter..."),"",QKeySequence::SaveAs,[this]{saveAs();});
    add("insertImage",ui("Bild aus Datei einfügen..."),"",{},[this]{
        const QString file=QFileDialog::getOpenFileName(this,ui("Bild aus Datei einfügen..."),QString(),ui("Bilder (*.png *.jpg *.jpeg *.bmp)"));
        QString error;if(!file.isEmpty()&&!insertImage(file,&error))QMessageBox::warning(this,ui("Bild aus Datei einfügen..."),error);});
    add("exportImage",ui("&Exportieren..."),"",{},[this]{exportDialog();});
    add("print",ui("&Drucken..."),"print",QKeySequence::Print,[this]{
        // The settings of each sheet, kept with it: changed in the preview, one undo step.
        QList<PrintSettings> list;for(const auto &s:doc.sheets)list<<s.print;
        {PrintPreview preview(doc,list,path,this);preview.helpRequested=[this,&preview]{showHelp({},&preview);};preview.exec();}
        bool same=true;for(int i=0;i<doc.sheets.size();i++)same&=list.value(i)==doc.sheets[i].print;
        if(!same)change([&](Document &d){for(int i=0;i<d.sheets.size();i++)d.sheets[i].print=list.value(i);});});
    add("copyImage",ui("Zwischenablage..."),"",QKeySequence(Qt::CTRL|Qt::Key_B),[this]{
        // All elements or the selected ones as a picture on the clipboard, as in the reference.
        ClipboardDialog dialog(!selectedItems().isEmpty()&&!editingComponent(),this);
        if(clipboardShown)clipboardShown(&dialog);
        if(dialog.exec()!=QDialog::Accepted)return;
        const QImage image=clipboardImage(dialog.resolution(),dialog.selectionOnly());
        if(image.isNull()){QMessageBox::warning(this,ui("Kopieren in die Zwischenablage"),ui("Das Bild wäre zu groß. Bitte eine kleinere Auflösung wählen oder weniger auswählen."));return;}
        QApplication::clipboard()->setImage(image);});
    add("exportSplan8",ui("Als sPlan 8 exportieren..."),"",{},[this]{exportSplanDialog(80);});
    add("exportSplan7",ui("Als sPlan 7 exportieren..."),"",{},[this]{exportSplanDialog(70);});
    add("quit",ui("&Beenden"),"",{},[this]{close();});
    // Hilfe; F1 as in the reference on every system (macOS offers ⌘? as well).
    {auto *topics=add("helpTopics",ui("&Hilfethemen…"),"",QKeySequence::HelpContents,[this]{showHelp();});
        auto keys=QKeySequence::keyBindings(QKeySequence::HelpContents);if(!keys.contains(QKeySequence(Qt::Key_F1)))keys<<QKeySequence(Qt::Key_F1);topics->setShortcuts(keys);}
    add("about",ui("Info..."),"",{},[this]{QMessageBox::about(this,ui("Schaltplan"),ui("<b>OpenLoch – Schaltplan</b><br>Schaltpläne über mehrere Blätter mit Bauteilbibliothek, Parent/Child-Bauteilen, Formblättern und Stückliste; liest und schreibt sPlan-Dateien (Versionen 7 und 8)."));});
    add("problems",ui("Probleme..."),"sch-problem",{},[this]{problemDialog();})->setVisible(false);
    // Bauteileditor
    add("componentEditorSave",ui("Speichern und Beenden"),"",{},[this]{leaveComponentEditor(true);});
    add("componentEditorCancel",ui("Abbrechen"),"",{},[this]{leaveComponentEditor(false);});
    // Bearbeiten
    add("undo",ui("&Rückgängig"),"undo",QKeySequence::Undo,[this]{undo();});
    add("redo",ui("&Wiederherstellen"),"redo",QKeySequence(Qt::CTRL|Qt::Key_Y),[this]{redo();});
    add("cut",ui("&Ausschneiden"),"cut",QKeySequence::Cut,[this]{cutSelection();});
    add("copy",ui("&Kopieren"),"copy",QKeySequence::Copy,[this]{copySelection();});
    add("paste",ui("&Einfügen"),"paste",QKeySequence::Paste,[this]{pasteClipboard();});
    add("duplicate",ui("&Duplizieren"),"duplicate",QKeySequence(Qt::CTRL|Qt::Key_D),[this]{duplicateSelection();});
    add("delete",ui("Löschen"),"delete",QKeySequence::Delete,[this]{if(!sheetView->deleteChosenGuide())deleteSelection();});
    add("selectAll",ui("Alles &markieren"),"",QKeySequence::SelectAll,[this]{sheetView->selectAll();});
    // Blatt
    add("sheetProperties",ui("Eigenschaften..."),"",{},[this]{sheetView->setSelection({});propertiesPanel->setFocus();});
    add("newSheet",ui("&Neues Blatt..."),"",{},[this]{insertSheetDialog(false);});
    add("copySheet",ui("Blatt kopieren..."),"",{},[this]{insertSheetDialog(true);});
    add("deleteSheet",ui("Blatt &löschen..."),"",{},[this]{
        if(doc.sheets.size()<2){QMessageBox::information(this,ui("Blatt löschen"),ui("Ein Schaltplan hat mindestens ein Blatt."));return;}
        if(QMessageBox::question(this,ui("Blatt löschen"),ui("Das Blatt „%1“ löschen?").arg(doc.sheet().name))==QMessageBox::Yes)removeSheet(doc.activeSheet);});
    add("sortSheets",ui("Sortieren..."),"",{},[this]{sortSheetsDialog();});
    {auto *a=add("gotoSheet",ui("Gehe zu Blatt"),"",{},[]{});auto *m=new QMenu(this);a->setMenu(m);
        connect(m,&QMenu::aboutToShow,this,[this,m]{m->clear();for(int i=0;i<doc.sheets.size();i++)m->addAction(QStringLiteral("%1  %2").arg(i+1).arg(doc.sheets[i].name),this,[this,i]{switchSheet(i);});});}
    // Formblatt
    add("editTitleBlock",ui("&Formblatt bearbeiten"),"sch-titleblock",{},[this]{setTitleBlockMode(action("editTitleBlock")->isChecked());})->setCheckable(true);
    add("loadTitleBlock",ui("Formblatt &laden..."),"",{},[this]{
        const QString file=QFileDialog::getOpenFileName(this,ui("Formblatt laden"),startFolder(Work::Forms,formFolder()),ui("sPlan-Formblätter (*.sbk);;Alle Dateien (*)"));usedFolder(Work::Forms,file);
        QString error;if(!file.isEmpty()&&!loadTitleBlock(file,&error))QMessageBox::warning(this,ui("Formblatt laden"),error);});
    add("saveTitleBlock",ui("Formblatt &speichern..."),"",{},[this]{
        QDir().mkpath(formFolder());
        const QString file=QFileDialog::getSaveFileName(this,ui("Formblatt speichern"),QDir(startFolder(Work::Forms,formFolder())).filePath(doc.sheet().titleBlock.name.isEmpty()?ui("Formblatt"):doc.sheet().titleBlock.name),ui("sPlan-Formblätter (*.sbk)"));
        usedFolder(Work::Forms,file);
        QString error;if(!file.isEmpty()&&!saveTitleBlock(file,&error))QMessageBox::warning(this,ui("Formblatt speichern"),error);});
    add("generateTitleBlock",ui("Formblatt generieren..."),"",{},[this]{titleBlockDialog();});
    add("removeTitleBlock",ui("Formblatt entfernen..."),"",{},[this]{
        if(QMessageBox::question(this,ui("Formblatt entfernen"),ui("Das Formblatt dieses Blattes entfernen?"))==QMessageBox::Yes)setTitleBlock(TitleBlock());});
    // Funktionen
    add("toFront",ui("Nach v&orne stellen"),"front",{},[this]{reorderSelection(0);});
    add("toBack",ui("Nach h&inten stellen"),"back",{},[this]{reorderSelection(1);});
    add("oneForward",ui("Eins nach vorne stellen"),"",{},[this]{reorderSelection(2);});
    add("oneBack",ui("Eins nach hinten stellen"),"",{},[this]{reorderSelection(3);});
    add("alignGrid",ui("Am Raster ausrichten"),"",QKeySequence(Qt::CTRL|Qt::ALT|Qt::Key_G),[this]{alignToGrid();});
    add("alignTop",ui("Oben ausrichten"),"align-top",{},[this]{alignSelection(Alignment::Top);});
    add("alignBottom",ui("Unten ausrichten"),"align-bottom",{},[this]{alignSelection(Alignment::Bottom);});
    add("alignLeft",ui("Links ausrichten"),"align-left",{},[this]{alignSelection(Alignment::Left);});
    add("alignRight",ui("Rechts ausrichten"),"align-right",{},[this]{alignSelection(Alignment::Right);});
    add("alignHorizontalCentre",ui("Horizontal-Mittig ausrichten"),"align-hcenter",{},[this]{alignSelection(Alignment::HorizontalCentre);});
    add("alignVerticalCentre",ui("Vertikal-Mittig ausrichten"),"align-vcenter",{},[this]{alignSelection(Alignment::VerticalCentre);});
    add("spreadHorizontally",ui("Horizontal gleichmäßig verteilen"),"sch-spread-horizontal",{},[this]{spreadSelection(true);});
    add("spreadVertically",ui("Vertikal gleichmäßig verteilen"),"sch-spread-vertical",{},[this]{spreadSelection(false);});
    add("rotate",ui("Rotieren"),"rotate",QKeySequence(Qt::CTRL|Qt::Key_R),[this]{if(sheetView->placing())sheetView->turnPlacement(90);else rotateSelection(90);});
    add("rotateBy",ui("&Drehen..."),"",{},[this]{
        bool ok;const double a=QInputDialog::getDouble(this,ui("Drehen"),ui("Winkel [°] (gegen den Uhrzeigersinn):"),90,-360,360,2,&ok);if(ok)rotateSelection(a);});
    add("mirror",ui("&Horizontal spiegeln"),"mirror",QKeySequence(Qt::CTRL|Qt::Key_M),[this]{mirrorSelection(false);});
    add("mirrorVertical",ui("&Vertikal spiegeln"),"sch-mirror-vertical",QKeySequence(Qt::CTRL|Qt::ALT|Qt::Key_M),[this]{mirrorSelection(true);});
    // Magnetic guide lines.
    add("newVerticalGuide",ui("Neue vertikale &Magnetlinie"),"",{},[this]{newGuide(true);});
    add("newHorizontalGuide",ui("Neue hori&zontale Magnetlinie"),"",{},[this]{newGuide(false);});
    add("fixGuides",ui("Magnetlinien fixieren"),"",{},[this]{sheetView->guidesFixed=action("fixGuides")->isChecked();sheetView->update();})->setCheckable(true);
    add("hideGuides",ui("Magnetlinien ausblenden"),"",{},[this]{sheetView->guidesHidden=action("hideGuides")->isChecked();sheetView->update();})->setCheckable(true);
    {auto *a=add("magnetLines",ui("Magnetlinien"),"",{},[]{});auto *m=new QMenu(this);a->setMenu(m);
        for(const char *n:{"newVerticalGuide","newHorizontalGuide","","fixGuides","hideGuides"}){if(!*n)m->addSeparator();else m->addAction(action(n));}}
    add("group",ui("&Gruppe bilden"),"group",QKeySequence(Qt::CTRL|Qt::Key_G),[this]{groupSelection();});
    add("ungroup",ui("Gr&uppe auflösen"),"ungroup",QKeySequence(Qt::CTRL|Qt::Key_U),[this]{ungroupSelection();});
    add("joinLines",ui("Linien verbinden"),"",{},[this]{sheetView->joinLines();});
    // Bauteil
    add("dissolveComponent",ui("Bauteil &auflösen"),"",{},[this]{dissolveComponents();});
    add("makeComponent",ui("&Bauteil aus Markierung erstellen..."),"",{},[this]{makeComponent();});
    add("componentEditor",ui("Bauteil-Editor"),"sch-component-editor",QKeySequence(Qt::ALT|Qt::Key_Return),[this]{editComponent();});
    add("renumber",ui("&Bauteile neu nummerieren..."),"sch-autonum",{},[this]{renumberDialog();});
    add("search",ui("Suchen/Ersetzen..."),"sch-search",QKeySequence::Find,[this]{
        const bool on=action("search")->isChecked();searchPanel->setVisible(on);if(on)searchPanel->refresh();})->setCheckable(true);
    add("scale",ui("Skalieren..."),"",{},[this]{
        bool ok=false;const double percent=QInputDialog::getDouble(this,ui("Skalieren..."),ui("Skalieren auf")+QStringLiteral(" [%]:"),100,1,10000,1,&ok);
        if(ok&&percent!=100)scaleSelection(percent/100);});
    add("colourize",ui("&Elemente einfärben..."),"",QKeySequence(Qt::CTRL|Qt::ALT|Qt::Key_C),[this]{
        const QColor colour=QColorDialog::getColor(Qt::black,this,ui("&Elemente einfärben..."));if(colour.isValid())colourizeSelection(colour);});
    add("penWidth",ui("Strichstärke verändern..."),"",{},[this]{penWidthDialog();});
    add("partsList",ui("&Stückliste erstellen..."),"sch-partslist",{},[this]{partsListDialog();});
    add("parentChildList",ui("Parent-Child-Liste erstellen..."),"",{},[this]{
        ParentChildListDialog dialog(doc,path,this);if(dialog.exec()==QDialog::Accepted&&!dialog.chosen.isEmpty())showElement(dialog.chosen);});
    add("contactList",ui("Kontaktliste erstellen..."),"",{},[this]{
        ContactListDialog dialog(doc,path,this);if(dialog.exec()==QDialog::Accepted&&!dialog.chosen.isEmpty())showElement(dialog.chosen);});
    add("contents",ui("Inhaltsverzeichnis einfügen..."),"",{},[this]{contentsDialog();});
    add("bitmapExplorer",ui("Bitmap-Explorer..."),"",{},[this]{bitmapExplorer();});
    add("linkList",ui("Linkliste erstellen..."),"",{},[this]{
        LinkListDialog dialog(doc,this);if(dialog.exec()==QDialog::Accepted&&!dialog.chosen.isEmpty())showElement(dialog.chosen);});
    // "Bemaßung fixieren": the value shown becomes the fixed value; "Fixierung aufheben" measures again (as the reference).
    add("fixDimensions",ui("Bemaßung fixieren"),"",{},[this]{fixDimensions(true);});
    add("unfixDimensions",ui("Bemaßung Fixierung aufheben"),"",{},[this]{fixDimensions(false);});
    // F8 follows the link of the selected text, as a double click does.
    add("followLink",ui("Textverlinkung folgen"),"",QKeySequence(Qt::Key_F8),[this]{
        const QStringList ids=sheetView->selection();if(ids.size()==1)followLink(ids.first());});
    add("parentChildColours",ui("Parent/Child-Bauteile einfärben"),"sch-parent-child",{},[this]{sheetView->parentChildColours=action("parentChildColours")->isChecked();sheetView->update();})->setCheckable(true);
    add("lettering",ui("Bauteilbeschriftung..."),"",{},[this]{letteringDialog();});
    add("copyToLibrary",ui("Bauteil(e) in die Bibliothek &kopieren"),"",{},[this]{copyToLibrary(false);});
    add("copyClipToLibrary",ui("Als Clip in die Bibliothek kopieren"),"",{},[this]{copyToLibrary(true);});
    // Einstellungen
    add("grid",ui("Raster..."),"sch-grid",{},[this]{chooseGrid();});
    add("variables",ui("Anwender-Variablen..."),"",{},[this]{variablesDialog();});
    // "Voreinstellungen" as in the reference: each entry shows its preset in the properties panel, without changing the
    // drawing mode, until something is selected or another mode is chosen.
    {auto *presetsAction=add("presets",ui("Voreinstellungen"),"",{},[]{});auto *menu=new QMenu(this);presetsAction->setMenu(menu);
        const std::tuple<const char*,QString,Item SheetView::*> entries[]={
            {"presetLine",ui("Linie"),&SheetView::linePreset},{"presetShape",ui("Fläche"),&SheetView::shapePreset},{"presetDimension",ui("Bemaßung"),&SheetView::dimensionPreset},
            {"presetJunction",ui("Lötpunkt"),&SheetView::junctionPreset},{"presetText",ui("Text"),&SheetView::textPreset},{"presetTextBox",ui("Mengentext"),&SheetView::textBoxPreset},
            {"presetDesignator",ui("Bauteil - Bezeichner"),&SheetView::designatorPreset},{"presetValue",ui("Bauteil - Wert"),&SheetView::valuePreset},
            {"presetContact",ui("Bauteil - Kontakt"),&SheetView::contactPreset}};
        for(const auto &[name,title,member]:entries){
            auto *a=menu->addAction(title);a->setObjectName(QString::fromLatin1(name));
            connect(a,&QAction::triggered,this,[this,title,member]{
                propertiesPanel->showPreset(&(sheetView->*member),title);if(propertiesDock)propertiesDock->show();});
        }}
    add("settings",ui("Grundeinstellungen..."),"",{},[this]{
        QList<HotkeyMode> modes;
        for(auto *a:modeActions())modes<<HotkeyMode{a->objectName(),a->text().remove(u'&'),standardKeys.value(a->objectName(),a->shortcut())};
        const DrawingSettings drawing{doc.sheetNumbers,doc.designatorPageNumbers,doc.designatorPrefix};
        SettingsDialog dialog(GeneralSettings::load(),this,modes,drawing);if(dialog.exec()!=QDialog::Accepted)return;
        const GeneralSettings g=dialog.settings();g.store();applySettings(g);
        // Sheet numbers and the prefix belong to the drawing: one step to undo.
        if(const DrawingSettings d=dialog.drawing();d!=drawing)
            change([d](Document &x){x.sheetNumbers=d.sheetNumbers;x.designatorPageNumbers=d.designatorPageNumbers;x.designatorPrefix=d.designatorPrefix;});
        const QString key=libraryPanel->pageKey(libraryPanel->currentPage());reloadLibrary();libraryPanel->showPage(key);});
    add("textConstants",ui("Textkonstanten..."),"",{},[this]{editTextConstants();});
    // Templates: a document of the template folder opened as a new one, the document kept there as one.
    add("templateNew",ui("Neu von Vorlage..."),"",{},[this]{
        if(!maybeSave())return;
        const QString file=QFileDialog::getOpenFileName(this,ui("Neu von Vorlage"),templateFolder(),ui("Schaltpläne (*.olsch *.spl8 *.spl7);;OpenLoch-Schaltpläne (*.olsch);;sPlan (*.spl8 *.spl7);;Alle Dateien (*)"));
        QString error;if(!file.isEmpty()&&!newFromTemplate(file,&error))QMessageBox::warning(this,ui("Öffnen"),error);});
    add("templateSave",ui("Als Vorlage speichern..."),"",{},[this]{
        QDir().mkpath(templateFolder());
        const QString file=QFileDialog::getSaveFileName(this,ui("Als Vorlage speichern"),QDir(templateFolder()).filePath(QFileInfo(displayName()).completeBaseName()+QStringLiteral(".olsch")),ui("OpenLoch-Schaltpläne (*.olsch)"));
        if(file.isEmpty())return;
        try{schematic::save(doc,file);}catch(const FormatError &e){QMessageBox::warning(this,ui("Speichern"),QString::fromUtf8(e.what()));}});
    add("explorer",ui("Explorer..."),"",{},[this]{
        const QString dir=path.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation):QFileInfo(path).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));});
    add("loadSheet",ui("Blatt la&den..."),"",{},[this]{
        const QString file=QFileDialog::getOpenFileName(this,ui("Blatt la&den...").remove(u'&'),startFolder(Work::Drawings),ui("Schaltpläne und Blätter (*.olsch *.spl8 *.spl7 *.blt);;OpenLoch-Schaltpläne (*.olsch);;sPlan (*.spl8 *.spl7);;sPlan-Blätter (*.blt);;Alle Dateien (*)"));
        usedFolder(Work::Drawings,file);
        QString error;if(!file.isEmpty()&&!loadSheets(file,&error))QMessageBox::warning(this,ui("Öffnen"),error);});
    add("saveSheet",ui("Blatt &speichern..."),"",{},[this]{
        const QString file=QFileDialog::getSaveFileName(this,ui("Blatt &speichern...").remove(u'&'),QDir(startFolder(Work::Drawings,QStringLiteral("."))).filePath(doc.sheet().name+QStringLiteral(".olsch")),
                                                        ui("OpenLoch-Schaltpläne (*.olsch);;sPlan-Blätter (*.blt)"));
        usedFolder(Work::Drawings,file);
        if(!file.isEmpty()&&QFileInfo(file).suffix().compare("blt",Qt::CaseInsensitive)==0){
            // As an sPlan file: what would be lost is shown first.
            QStringList lost;try{lost=splan::losses(sheetDocument(true),80);}catch(const FormatError &e){QMessageBox::warning(this,ui("Speichern"),QString::fromUtf8(e.what()));return;}
            if(!lost.isEmpty()&&QMessageBox::question(this,ui("Speichern"),ui("Beim Schreiben als sPlan-Datei geht verloren:")+QStringLiteral("\n• ")+lost.join(QStringLiteral("\n• "))+QStringLiteral("\n\n")+ui("Trotzdem speichern?"))!=QMessageBox::Yes)return;
        }
        QString error;if(!file.isEmpty()&&!saveSheet(file,&error))QMessageBox::warning(this,ui("Speichern"),error);});
    // The system's key and, as in the reference, Ctrl+F11.
    {auto *full=add("fullScreen",ui("Vollbildansicht"),"",QKeySequence::FullScreen,[this]{if(action("fullScreen")->isChecked())showFullScreen();else showNormal();});full->setCheckable(true);
        auto keys=QKeySequence::keyBindings(QKeySequence::FullScreen);if(!keys.contains(QKeySequence(Qt::CTRL|Qt::Key_F11)))keys<<QKeySequence(Qt::CTRL|Qt::Key_F11);full->setShortcuts(keys);}
    // View
    add("zoomSheet",ui("Zoom Blatt"),"zoom-board",QKeySequence(Qt::Key_F5),[this]{sheetView->fitSheet();});
    add("zoomItems",ui("Zoom Elemente"),"zoom-all",QKeySequence(Qt::Key_F6),[this]{sheetView->fitItems(false);});
    add("zoomSelected",ui("Zoom markierte Elemente"),"zoom-marked",QKeySequence(Qt::Key_F7),[this]{sheetView->fitItems(true);});
    add("zoomIn",ui("Vergrößern"),"zoom-in",QKeySequence::ZoomIn,[this]{sheetView->zoomAt(SheetView::wheelStep,QPointF(sheetView->width()/2.,sheetView->height()/2.));});
    add("zoomOut",ui("Verkleinern"),"zoom-out",QKeySequence::ZoomOut,[this]{sheetView->zoomAt(1/SheetView::wheelStep,QPointF(sheetView->width()/2.,sheetView->height()/2.));});
    // The drawing modes.
    toolGroup=new QActionGroup(this);
    // The keys of the reference choose them; a key goes to a text field first when one has the focus.
    auto tool=[&](const QString &name,const QString &text,const QString &icon,SheetView::Tool t,QKeySequence key={}){
        auto *a=add(name,text,icon,key,[this,t]{sheetView->setTool(t);});a->setCheckable(true);a->setData(int(t));toolGroup->addAction(a);return a;
    };
    tool("toolSelect",ui("Standard"),"tool-select",SheetView::Tool::Select)->setChecked(true);
    tool("toolLine",ui("Linie"),"sch-line",SheetView::Tool::Line,Qt::Key_L);
    tool("toolJunction",ui("Lötpunkt"),"sch-junction",SheetView::Tool::Junction,Qt::Key_P);
    // As in the reference, the small triangle at the button chooses how the frame is drawn.
    auto fromCorner=[&](QAction *a,bool SheetView::*centred,const char *name){
        auto *m=new QMenu(this);auto *how=new QActionGroup(m);
        auto *corner=m->addAction(ui("Rahmen aufziehen"));auto *centre=m->addAction(ui("Vom Mittelpunkt aus"));
        corner->setCheckable(true);centre->setCheckable(true);corner->setChecked(true);how->addAction(corner);how->addAction(centre);
        centre->setObjectName(QString::fromLatin1(name));
        connect(corner,&QAction::triggered,this,[this,centred,a]{sheetView->*centred=false;a->trigger();});
        connect(centre,&QAction::triggered,this,[this,centred,a]{sheetView->*centred=true;a->trigger();});
        a->setMenu(m);
    };
    fromCorner(tool("toolRectangle",ui("Rechteck"),"tool-rectangle",SheetView::Tool::Rectangle,Qt::Key_R),&SheetView::rectanglesFromCentre,"rectangleFromCentre");
    fromCorner(tool("toolEllipse",ui("Kreis"),"tool-ellipse",SheetView::Tool::Ellipse,Qt::Key_K),&SheetView::ellipsesFromCentre,"ellipseFromCentre");
    tool("toolPolygon",ui("Polygon"),"tool-polygon",SheetView::Tool::Polygon,Qt::Key_O);
    tool("toolBezier",ui("Bezierkurve"),"sch-bezier",SheetView::Tool::Bezier,Qt::Key_U);
    tool("toolFreehand",ui("Freihandlinie"),"sch-freehand",SheetView::Tool::Freehand,Qt::Key_H);
    {   // "Spezialformen": the mode button shows a list of shapes, the first four with their settings.
        auto *m=new QMenu(this);m->setObjectName("specialShapes");
        const QList<std::pair<SpecialShape,QString>> shapes{{SpecialShape::RegularPolygon,ui("Vieleck...")},{SpecialShape::Star,ui("Stern...")},{SpecialShape::Grid,ui("Gitter...")},
            {SpecialShape::Wave,ui("Schwingung...")},{SpecialShape::Wave,QString()},{SpecialShape::CurlyBracket,ui("Geschweifte Klammer")},{SpecialShape::RoundBracket,ui("Runde Klammer")},
            {SpecialShape::Triangle,ui("Dreieck")},{SpecialShape::RightTriangle,ui("Dreieck, rechtwinklig")},{SpecialShape::Square,ui("4-Eck")},{SpecialShape::Diamond,ui("Raute")},
            {SpecialShape::Parallelogram,ui("Parallelogramm")},{SpecialShape::Hexagon,ui("6-Eck")},{SpecialShape::Octagon,ui("8-Eck")},
            {SpecialShape::ArrowHorizontal,ui("Pfeil links oder rechts")},{SpecialShape::ArrowVertical,ui("Pfeil hoch oder runter")},{SpecialShape::SpeechBubble,ui("Sprechblase")},{SpecialShape::Lightning,ui("Blitz")}};
        for(const auto &[shape,label]:shapes){
            if(label.isEmpty()){m->addSeparator();continue;}
            m->addAction(label,this,[this,shape=shape]{chooseSpecialShape(shape);});
        }
        auto *a=add("toolSpecial",ui("Spezialformen"),"sch-special",Qt::Key_S,[this,m]{
            // Until a shape is chosen the mode stays as it was.
            for(auto *t:toolGroup->actions())if(t->data().toInt()==int(sheetView->tool()))t->setChecked(true);
            QWidget *w=modes?modes->widgetForAction(action("toolSpecial")):nullptr;
            m->popup(w&&w->isVisible()?w->mapToGlobal(QPoint(w->width(),0)):QCursor::pos());
        });
        a->setCheckable(true);a->setData(int(SheetView::Tool::Special));toolGroup->addAction(a);
    }
    {   // "Bemaßung": the kind from the list at the mode button.
        auto *a=tool("toolDimension",ui("Bemaßung"),"sch-dimension",SheetView::Tool::Dimension,Qt::Key_B);
        auto *m=new QMenu(this);m->setObjectName("dimensionKinds");auto *kinds=new QActionGroup(m);
        for(auto [kind,label,name]:{std::tuple{DimensionKind::Standard,ui("Standard"),"dimensionStandard"},std::tuple{DimensionKind::Radial,ui("Radial"),"dimensionRadial"},
                                    std::tuple{DimensionKind::Diameter,ui("Durchmesser"),"dimensionDiameter"},std::tuple{DimensionKind::Angle,ui("Winkel"),"dimensionAngle"}}){
            auto *k=m->addAction(label);k->setObjectName(name);k->setCheckable(true);k->setChecked(kind==DimensionKind::Standard);kinds->addAction(k);actions.insert(name,k);
            connect(k,&QAction::triggered,this,[this,kind=kind]{setDimensionKind(kind);});
        }
        a->setMenu(m);
    }
    tool("toolText",ui("Text"),"tool-text",SheetView::Tool::Text,Qt::Key_T);
    tool("toolTextBox",ui("Mengentext"),"sch-textbox",SheetView::Tool::TextBox,Qt::Key_E);
    // "Bitmap" asks for a picture file and puts the picture down, as "Bild aus Datei einfügen".
    add("toolBitmap",ui("Bitmap"),"sch-bitmap",Qt::Key_F,[this]{action("insertImage")->trigger();});
    tool("toolNetLabel",ui("Netzname"),"sch-netlabel",SheetView::Tool::NetLabel);
    tool("toolSheetReference",ui("Blattverweis"),"sch-sheetref",SheetView::Tool::SheetReference);
    tool("toolZoom",ui("Zoom"),"tool-zoom",SheetView::Tool::Zoom,Qt::Key_Z);
    tool("toolMeasure",ui("Messen"),"sch-measure",SheetView::Tool::Measure,Qt::Key_M);
    tool("toolContact",ui("Kontakt"),"sch-contact",SheetView::Tool::Contact)->setVisible(false);
}
void Editor::createMenus(){
    for(const auto &[title,names]:editorMenus()){
        auto *m=menuBar()->addMenu(ui(title));menus.insert(title,m);
        for(const auto &n:names){if(n.isEmpty())m->addSeparator();else if(auto *a=action(n))m->addAction(a);}
    }
    menus.value(QStringLiteral("B&auteileditor"))->menuAction()->setVisible(false);
    connect(menus.value(QStringLiteral("&Datei")),&QMenu::aboutToShow,this,[this]{refreshRecent();});
}
QStringList Editor::recentFiles() const{return preferences?QSettings().value("schematic/recentFiles").toStringList():QStringList();}
void Editor::rememberFile(const QString &file){
    if(!preferences||file.isEmpty())return;
    QStringList list=recentFiles();const QString full=QFileInfo(file).absoluteFilePath();
    list.removeAll(full);list.prepend(full);while(list.size()>8)list.removeLast();
    QSettings().setValue("schematic/recentFiles",list);
}
void Editor::refreshRecent(){
    // "Zuletzt geöffnete Dateien" before "Beenden"; a host such as the suite keeps its own list.
    QMenu *file=menus.value(QStringLiteral("&Datei"));if(!file)return;
    for(auto *a:std::as_const(recentActions)){file->removeAction(a);a->deleteLater();}
    recentActions.clear();
    const QStringList list=openHandler?QStringList():recentFiles();if(list.isEmpty())return;
    QAction *quit=action("quit");
    for(int i=0;i<list.size();i++){
        auto *a=new QAction(QStringLiteral("&%1 %2").arg(i+1).arg(QDir::toNativeSeparators(list[i])),file);a->setObjectName("recentFile");a->setData(list[i]);
        connect(a,&QAction::triggered,this,[this,f=list[i]]{if(!maybeSave())return;QString error;if(!openFile(f,&error))QMessageBox::warning(this,ui("Schaltplan öffnen"),error);});
        file->insertAction(quit,a);recentActions<<a;
    }
    auto *line=new QAction(file);line->setSeparator(true);file->insertAction(quit,line);recentActions<<line;
}
void Editor::createToolBars(){
    auto *bar=addToolBar(ui("Werkzeuge"));bar->setObjectName("mainToolBar");bar->setIconSize(QSize(20,20));bar->setMovable(false);
    for(const char *n:{"new","open","save","print","","undo","redo","","cut","copy","paste","duplicate","","delete","","toFront","toBack","","rotate","mirror","mirrorVertical","","group","ungroup"}){
        if(!*n)bar->addSeparator();else bar->addAction(action(n));
    }
    // "Ausrichten" as a button with its list, as in the reference.
    auto *align=new QToolButton;align->setObjectName("alignButton");align->setIcon(schematicIcon("align-left"));align->setToolTip(ui("Ausrichten"));align->setPopupMode(QToolButton::InstantPopup);
    auto *alignMenu=new QMenu(align);align->setMenu(alignMenu);propertiesPanel->alignMenu=alignMenu;
    for(const char *n:{"alignTop","alignBottom","alignLeft","alignRight","alignHorizontalCentre","alignVerticalCentre","","spreadHorizontally","spreadVertically"}){if(!*n)alignMenu->addSeparator();else alignMenu->addAction(action(n));}
    bar->addWidget(align);bar->addSeparator();
    // Grid and zoom as buttons with their lists, as in the reference.
    auto *grid=new QToolButton;grid->setObjectName("gridButton");grid->setIcon(schematicIcon("sch-grid"));grid->setToolTip(ui("Raster"));grid->setPopupMode(QToolButton::InstantPopup);
    auto *gridMenu=new QMenu(grid);grid->setMenu(gridMenu);
    for(double g:{.1,.2,.5,1.,1.27,2.,2.5,2.54,5.,10.})gridMenu->addAction(uiLocale().toString(g)+QStringLiteral(" mm"),this,[this,g]{change([g](Document &d){d.sheet().grid=g;});});
    gridMenu->addSeparator();gridMenu->addAction(QStringLiteral("..."),this,[this]{chooseGrid();});
    bar->addWidget(grid);
    auto *zoom=new QToolButton;zoom->setObjectName("zoomButton");zoom->setIcon(schematicIcon("zoom-board"));zoom->setToolTip(ui("Zoom"));zoom->setPopupMode(QToolButton::InstantPopup);
    auto *zoomMenu=new QMenu(zoom);zoom->setMenu(zoomMenu);for(const char *n:{"zoomSheet","zoomItems","zoomSelected","","zoomIn","zoomOut"}){if(!*n)zoomMenu->addSeparator();else zoomMenu->addAction(action(n));}
    bar->addWidget(zoom);bar->addSeparator();
    bar->addAction(action("editTitleBlock"));bar->addAction(action("componentEditor"));bar->addAction(action("parentChildColours"));bar->addAction(action("search"));
    bar->addSeparator();bar->addAction(action("renumber"));bar->addAction(action("partsList"));
    // The reference's second toolbar: the list of sheets, the properties panel and, while there are some, the problems.
    bar->addSeparator();bar->addAction(action("showSheetList"));bar->addAction(action("showProperties"));bar->addAction(action("problems"));
    // The drawing modes at the left of the sheet.
    auto *m=new QToolBar(ui("Modus"));m->setObjectName("modeToolBar");m->setOrientation(Qt::Vertical);m->setIconSize(QSize(20,20));m->setMovable(false);
    for(auto *a:toolGroup->actions()){m->addAction(a);if(a==action("toolTextBox"))m->addAction(action("toolBitmap"));}
    if(auto *b=qobject_cast<QToolButton*>(m->widgetForAction(action("toolDimension"))))b->setPopupMode(QToolButton::MenuButtonPopup);
    modes=m;
}
QWidget *Editor::createStatusBar(){
    auto *w=new QWidget;auto *l=new QHBoxLayout(w);l->setContentsMargins(2,0,2,0);
    coordinates=new QLabel(QStringLiteral("X:\nY:"));coordinates->setObjectName("coordinates");coordinates->setMinimumWidth(90);l->addWidget(coordinates);
    relative=new QLabel(QStringLiteral("dX:\ndY:"));relative->setObjectName("relative");relative->setMinimumWidth(90);l->addWidget(relative);
    // As in the reference, a click on the scale shows the sheet's properties, on the grid or the zoom their lists.
    auto *scale=new ClickableLabel(QStringLiteral("1:1\nmm"));scaleLabel=scale;scaleLabel->setObjectName("scaleLabel");l->addWidget(scaleLabel);
    scale->clicked=[this]{if(!editingComponent())sheetView->setSelection({});};
    auto *gz=new QWidget;auto *gzl=new QVBoxLayout(gz);gzl->setContentsMargins(0,0,0,0);gzl->setSpacing(0);
    auto *gridField=new ClickableLabel,*zoomField=new ClickableLabel;gridLabel=gridField;zoomLabel=zoomField;
    gridLabel->setObjectName("gridLabel");zoomLabel->setObjectName("zoomLabel");gzl->addWidget(gridLabel);gzl->addWidget(zoomLabel);l->addWidget(gz);
    auto listOf=[this](const char *button,QLabel *label){return [this,button,label]{
        if(auto *b=findChild<QToolButton*>(QString::fromLatin1(button));b&&b->menu())b->menu()->popup(label->mapToGlobal(QPoint(0,0)));};};
    gridField->clicked=listOf("gridButton",gridLabel);zoomField->clicked=listOf("zoomButton",zoomLabel);
    auto toggle=[&](const QString &name,const QString &icon,const QString &tip,bool on,std::function<void(bool)> set){
        auto *b=new QToolButton;b->setObjectName(name);b->setIcon(schematicIcon(icon));b->setToolTip(tip);b->setCheckable(true);b->setChecked(on);b->setAutoRaise(true);
        connect(b,&QToolButton::toggled,this,std::move(set));l->addWidget(b);return b;
    };
    toggle("gridSnap","sch-snap-grid",ui("Rasterfang ein/aus"),true,[this](bool on){sheetView->gridSnap=on;sheetView->update();});
    toggle("angleSnap","sch-snap-angle",ui("Winkelfang ein/aus"),true,[this](bool on){sheetView->angleSnap=on;});
    toggle("terminalSnap","sch-snap-terminal",ui("Anschlussfang ein/aus"),true,[this](bool on){sheetView->terminalSnap=on;});
    toggle("rubberBand","sch-rubberband",ui("Gummiband ein/aus"),true,[this](bool on){sheetView->rubberBand=on;});
    auto *angle=new QToolButton;angle->setObjectName("rotationSnap");angle->setText(QStringLiteral("30°"));angle->setToolTip(ui("Drehwinkelfang"));angle->setPopupMode(QToolButton::InstantPopup);
    auto *am=new QMenu(angle);angle->setMenu(am);
    for(int a:{5,10,15,30,45,90})am->addAction(QStringLiteral("%1°").arg(a),this,[this,angle,a]{sheetView->rotationSnap=a;angle->setText(QStringLiteral("%1°").arg(a));});
    am->addSeparator();am->addAction(ui("Aus"),this,[this,angle]{sheetView->rotationSnap=0;angle->setText(ui("Aus"));});
    // Any other step, as the reference's "...".
    am->addAction(QStringLiteral("..."),this,[this,angle]{
        bool ok=false;const double a=QInputDialog::getDouble(this,ui("Drehwinkelfang"),ui("Drehwinkelfang")+QStringLiteral(" [°]:"),sheetView->rotationSnap>0?sheetView->rotationSnap:30,.1,360,1,&ok);
        if(ok){sheetView->rotationSnap=a;angle->setText(uiLocale().toString(a)+QStringLiteral("°"));}});
    l->addWidget(angle);
    toggle("textsTurned","sch-text-rotate",ui("Texte beim Drehen komplett mitdrehen"),true,[this](bool on){sheetView->textsTurned=on;});
    toggle("textsMirrored","sch-text-mirror",ui("Texte beim Spiegeln komplett mitspiegeln"),false,[this](bool on){sheetView->textsMirrored=on;});
    hint=new QLabel;hint->setObjectName("hint");l->addWidget(hint,1);
    auto *keys=new QLabel(ui("STRG = Rasterfang aus   SHIFT = Winkelfang aus   ALT = Anschlussfang aus"));keys->setObjectName("keys");keys->setStyleSheet(QStringLiteral("color:palette(dark)"));l->addWidget(keys);
    return w;
}
void Editor::reloadLibrary(){
    // The pages of the library folder under a top folder of their own, named like the folder.
    const QString folder=libraryFolder();QList<LibraryPage> own=folderPages(folder);
    const QString root=QDir(folder).dirName();for(auto &p:own)p.folder=p.folder.isEmpty()?root:root+u'/'+p.folder;
    // Further folders, only read, each under a top folder of its name too.
    for(const QString &extra:libraryFolders().extra){
        if(extra.isEmpty()||QDir(extra)==QDir(folder))continue;
        QList<LibraryPage> more=folderPages(extra);const QString name=QDir(extra).dirName();
        for(auto &p:more){p.folder=p.folder.isEmpty()?name:name+u'/'+p.folder;p.readOnly=true;}
        own+=more;
    }
    libraryPanel->setOwnFolder(folder,root);libraryPanel->setPages(builtInPages()+own);
}
void Editor::librariesChanged(){if(libraryPanel&&libraryPanel->reload)libraryPanel->reload();}
void Editor::copyToLibrary(bool clip){
    QList<LibraryEntry> entries;
    QMap<QString,Resource> pictures;
    auto collect=[&](const Item &i,auto &self)->void{
        if(i.type==ItemType::Image&&doc.resources.contains(i.resource))pictures.insert(i.resource,doc.resources[i.resource]);
        for(const auto &c:i.children)self(c,self);};
    const QList<Item> sel=selectedItems();
    if(clip){
        // A clip: the selection as a group around the grid point nearest the middle of its bounds.
        Item group;group.type=ItemType::Group;QRectF r;
        static const QRegularExpression trailing(QStringLiteral("\\d+$"));
        for(auto i:sel){
            // Numbered components get theirs anew when placed; a parent's children follow it.
            std::function<void(Item&)> unnumber=[&](Item &c){if(c.type==ItemType::Component&&c.autoNumber&&c.designator.contains(trailing))c.designator=c.designator.remove(trailing)+u'?';for(auto &k:c.children)unnumber(k);};
            unnumber(i);group.children<<i;r|=bounds(i);collect(i,collect);
        }
        const double g=doc.sheet().grid>0?doc.sheet().grid:1.27;
        const QPointF origin(std::round(r.center().x()/g)*g,std::round(r.center().y()/g)*g);
        schematic::move(group,-origin);
        entries.append({ui("Clip"),group,pictures});
    }else{
        // As in sPlan, a selected parent takes its selected children along, kept with it at their places relative to
        // its insertion point; they are no entries of their own.
        QSet<QString> parents;for(const auto &i:sel)if(i.type==ItemType::Component&&i.parent)parents.insert(i.id);
        for(const auto &i:sel){
            if(i.type!=ItemType::Component||parents.contains(i.parentId))continue;
            Item c=i;c.pos=QPointF();pictures.clear();collect(c,collect);
            static const QRegularExpression number(QStringLiteral("\\d+$"));
            if(c.autoNumber)c.designator=c.designator.remove(number)+u'?';
            QList<Item> children;
            for(const auto &k:sel)if(k.type==ItemType::Component&&c.parent&&k.parentId==i.id){
                Item child=k;child.parentId.clear();schematic::move(child,-i.pos);collect(child,collect);children<<child;}
            entries.append({c.caption.isEmpty()?i.designator:c.caption,c,pictures,children});
        }
    }
    if(entries.isEmpty())return;
    QString error;if(!libraryPanel->addEntries(entries,&error))QMessageBox::warning(this,ui("Bibliothek"),error);
}
bool Editor::insertImage(const QString &file,QString *error){
    QFile in(file);if(!in.open(QIODevice::ReadOnly)){if(error)*error=in.errorString();return false;}
    const QByteArray data=in.readAll();const QImage image=QImage::fromData(data);
    if(image.isNull()){if(error)*error=ui("Die Bitmap kann nicht eingelesen werden");return false;}
    QString kind=QFileInfo(file).suffix().toLower();if(kind==u"jpeg")kind=QStringLiteral("jpg");
    if(kind!=u"png"&&kind!=u"jpg"&&kind!=u"bmp"){if(error)*error=ui("Die Bitmap kann nicht eingelesen werden");return false;}
    const QString key=QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex());
    // Its size from the resolution stored with it (96 dpi without one), at most half the sheet.
    const double dpm=image.dotsPerMeterX()>0?image.dotsPerMeterX()/1000.:96/25.4;
    QSizeF size(image.width()/dpm,image.height()/dpm);
    const double fit=std::min({1.,doc.sheet().width/2/size.width(),doc.sheet().height/2/size.height()});size*=fit;
    Item picture;picture.type=ItemType::Image;picture.resource=key;picture.size=size;picture.centre=QPointF();
    doc.resources.insert(key,{kind,data});
    sheetView->setTool(SheetView::Tool::Select);sheetView->beginPlacement({picture});
    return true;
}
namespace {
// The selected elements and what they hold.
void eachIn(QList<Item> &items,const QStringList &ids,const std::function<void(Item&)> &f){
    std::function<void(Item&)> all=[&](Item &i){f(i);for(auto &c:i.children)all(c);};
    for(auto &i:items)if(ids.contains(i.id))all(i);
}
}
void Editor::scaleSelection(double factor){
    const QStringList ids=sheetView->selection();if(ids.isEmpty())return;
    QRectF r;for(const auto &i:sheetView->items())if(ids.contains(i.id))r|=bounds(i);
    const QPointF origin=r.center();
    change([&](Document &){for(auto &i:sheetView->items())if(ids.contains(i.id))schematic::scale(i,origin,factor);});
}
void Editor::colourizeSelection(const QColor &colour){
    const QStringList ids=sheetView->selection();if(ids.isEmpty())return;
    change([&](Document &){eachIn(sheetView->items(),ids,[&](Item &i){
        // A white filling stays white, as in the reference (boxes with texts inside stay readable).
        i.pen.color=colour;if(i.fill.style!=FillStyle::None&&i.fill.color.rgb()!=qRgb(255,255,255))i.fill.color=colour;
        if(isText(i)||i.type==ItemType::TextBox||i.type==ItemType::Dimension)i.font.color=colour;
        if(i.type==ItemType::Dimension)i.lineColor=i.extensionColor=colour;});});
}
void Editor::fixDimensions(bool fix){
    const QStringList ids=sheetView->selection();if(ids.isEmpty())return;
    const double scale=doc.sheet().scale;bool any=false;
    eachIn(sheetView->items(),ids,[&](Item &i){any|=i.type==ItemType::Dimension&&i.autoValue==fix;});
    if(!any)return;
    change([&](Document &){eachIn(sheetView->items(),ids,[&](Item &i){
        if(i.type!=ItemType::Dimension||i.autoValue!=fix)return;
        if(fix)i.fixedValue=dimensionNumber(dimensionValue(i,scale),i.digits,i.decimalPoint);
        i.autoValue=!fix;});});
}
void Editor::changePenWidths(bool fixed,double value){
    const QStringList ids=sheetView->selection();if(ids.isEmpty())return;
    change([&](Document &){eachIn(sheetView->items(),ids,[&](Item &i){
        if(i.type==ItemType::Line||i.type==ItemType::Polygon||i.type==ItemType::Bezier||i.type==ItemType::Rectangle||i.type==ItemType::Ellipse||i.type==ItemType::TextBox)
            i.pen.width=fixed?value:i.pen.width*value/100;});});
}
void Editor::penWidthDialog(){
    // "Strichstärke verändern": a new width, or a change in per cent.
    QDialog dialog(this);dialog.setWindowTitle(ui("Strichstärke verändern"));auto *form=new QFormLayout(&dialog);
    auto *fixed=new QRadioButton(ui("Auf festen Wert setzen"));fixed->setChecked(true);
    auto *width=new QDoubleSpinBox;width->setRange(0,10);width->setDecimals(2);width->setValue(.25);width->setSuffix(QStringLiteral(" mm"));width->setLocale(uiLocale());
    auto *relative=new QRadioButton(ui("Prozentual ändern"));
    auto *percent=new QDoubleSpinBox;percent->setRange(1,1000);percent->setValue(100);percent->setSuffix(QStringLiteral(" %"));percent->setLocale(uiLocale());
    form->addRow(new QLabel(ui("Neue Strichstärke")));form->addRow(fixed,width);form->addRow(relative,percent);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    changePenWidths(fixed->isChecked(),fixed->isChecked()?width->value():percent->value());
}
QStringList Editor::textConstants() const{return QSettings().value("schematic/textConstants").toStringList();}
void Editor::editTextConstants(){
    TextConstantsDialog dialog(textConstants(),this);
    if(textConstantsShown)textConstantsShown(&dialog);
    if(dialog.exec()==QDialog::Accepted)QSettings().setValue("schematic/textConstants",dialog.constants());
}
Document Editor::sheetDocument(bool forSplan) const{
    // The current sheet as a document of its own, with the pictures it shows and the user variables; the sheet's own
    // sPlan bytes stay for an sPlan sheet file (the sheet record stands for itself), the file's never.
    Document one=schematic::newDocument(doc.sheet().name);one.sheets={doc.sheet()};one.variables=doc.variables;
    for(auto it=doc.resources.cbegin();it!=doc.resources.cend();++it)one.resources.insert(it.key(),it.value());
    one.splan.clear();if(!forSplan)one.sheets[0].splan.clear();
    return one;
}
bool Editor::saveSheet(const QString &file,QString *error){
    // ".blt" as sPlan's sheet file: an sPlan 8 file with this one sheet.
    const bool blt=QFileInfo(file).suffix().compare("blt",Qt::CaseInsensitive)==0;
    try{if(blt)splan::save(sheetDocument(true),file,80);else schematic::save(sheetDocument(),file);return true;}
    catch(const FormatError &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
bool Editor::loadSheets(const QString &file,QString *error){
    // The sheets of a file after the current one, with new ids; its pictures and the variables the document lacks.
    Document other;
    try{other=load(file);}catch(const FormatError &e){if(error)*error=QString::fromUtf8(e.what());return false;}
    change([&](Document &d){
        int at=d.activeSheet+1;
        QList<QList<Item>*> lists;for(auto &s:other.sheets){s.id=newId();lists<<&s.items<<&s.titleBlock.items;}
        freshIds(lists);
        for(const Sheet &s:other.sheets)d.sheets.insert(at++,s);
        for(auto it=other.resources.cbegin();it!=other.resources.cend();++it)if(!d.resources.contains(it.key()))d.resources.insert(it.key(),it.value());
        for(const auto &v:other.variables){bool known=false;for(const auto &w:d.variables)known|=w.name.compare(v.name,Qt::CaseInsensitive)==0;if(!known)d.variables<<v;}
        d.activeSheet=d.activeSheet+1;
    });
    sheetView->fitSheet();
    return true;
}
void Editor::letteringDialog(){
    bool component=false;for(const auto &i:selectedItems())component|=i.type==ItemType::Component;
    LetteringDialog dialog(component,libraryPanel->writable(libraryPanel->currentPage()),this);
    // As the reference: the fonts of the selected components (the last one's), else the presets of component texts.
    Font fonts[3]={sheetView->designatorPreset.font,sheetView->valuePreset.font,sheetView->contactPreset.font};
    for(const auto &i:selectedItems())if(i.type==ItemType::Component){
        bool contact=false;
        for(const auto &k:i.children){
            if(k.role==TextRole::Designator)fonts[0]=k.font;else if(k.role==TextRole::Value)fonts[1]=k.font;
            else if(k.type==ItemType::Contact&&!contact){fonts[2]=k.font;contact=true;}
        }
    }
    for(int k=0;k<3;k++)dialog.start(k,fonts[k]);
    if(dialog.exec()!=QDialog::Accepted)return;
    applyLettering(dialog.lettering(0),dialog.lettering(1),dialog.lettering(2),dialog.scope());
}
void Editor::applyLettering(const Lettering &d,const Lettering &v,const Lettering &k,int scope){
    if(scope==LetteringDialog::LibraryPage){
        // The components of the current library page, if it is an own one.
        const int page=libraryPanel->currentPage();if(!libraryPanel->writable(page))return;
        LibraryPage p=libraryPanel->pages()[page];
        for(auto &e:p.entries)if(e.symbol.type==ItemType::Component)letter(e.symbol,d,v,k);
        for(int i=0;i<p.entries.size();i++){QString error;if(!libraryPanel->replaceEntry(i,p.entries[i],&error)){QMessageBox::warning(this,ui("Bibliothek"),error);return;}}
        return;
    }
    const QStringList ids=sheetView->selection();
    change([&](Document &doc){
        for(int s=0;s<doc.sheets.size();s++){
            if(scope!=LetteringDialog::Project&&s!=doc.activeSheet)continue;
            std::function<void(QList<Item>&)> walk=[&](QList<Item> &items){
                for(auto &i:items){
                    if(i.type==ItemType::Component&&(scope!=LetteringDialog::Selection||ids.contains(i.id)))letter(i,d,v,k);
                    else if(i.type==ItemType::Group)walk(i.children);
                }
            };
            walk(doc.sheets[s].items);
        }
    });
}
void Editor::renumberDialog(){
    QStringList names;for(const auto &s:doc.sheets)names<<s.name;
    QList<int> sheets;
    if(doc.sheets.size()>1){SheetChoiceDialog choice(ui("Bauteile neu nummerieren"),names,this);if(choice.exec()!=QDialog::Accepted)return;sheets=choice.sheets();if(sheets.isEmpty())return;}
    NumberingDialog dialog(!sheetView->selection().isEmpty(),this);if(dialog.exec()!=QDialog::Accepted)return;
    NumberingOptions o;o.sheets=sheets;o.raster=dialog.raster->value();o.start=dialog.start->value();
    o.order=dialog.columns->isChecked()?NumberingOptions::Order::Columns:dialog.rows->isChecked()?NumberingOptions::Order::Rows:NumberingOptions::Order::None;
    if(dialog.selectedOnly->isChecked())o.only=sheetView->selection();
    if(dialog.lettersOnly->isChecked())o.letters=dialog.letters->text().trimmed();
    change([&](Document &d){renumber(d,o);});
}
void Editor::showElement(const QString &id){
    PlacedComponent p=componentWithId(doc,id);if(!p.item)p=textWithId(doc,id);if(!p.item)return;
    if(editingComponent())leaveComponentEditor(true);
    if(p.sheet!=doc.activeSheet)switchSheet(p.sheet);
    // The element of the sheet that holds it (itself, or the group it is in).
    std::function<bool(const Item&)> holds=[&](const Item &i){if(i.id==id)return true;for(const auto &c:i.children)if(holds(c))return true;return false;};
    for(const auto &i:doc.sheet().items)if(holds(i)){sheetView->setSelection({i.id});break;}
    sheetView->centreOn(p.item->type==ItemType::Component||p.item->type==ItemType::Text?p.item->pos:bounds(*p.item).center());refresh();
}
void Editor::chooseLinkTarget(){
    const QStringList ids=sheetView->selection();if(ids.size()!=1)return;
    TargetChoiceDialog dialog(doc,ids[0],this);if(dialog.exec()!=QDialog::Accepted||dialog.targetId().isEmpty())return;
    const QString target=dialog.targetId(),id=ids[0];
    change([&](Document &d){for(auto &i:d.sheet().items)if(i.id==id&&i.type==ItemType::Text)i.linkTarget=target;});
}
bool Editor::followLink(const QString &id){
    const PlacedComponent t=textWithId(doc,id);if(!t.item)return false;
    if(textWithId(doc,t.item->linkTarget).item){showElement(t.item->linkTarget);return true;}
    if(t.item->link.isEmpty())return false;
    if(QMessageBox::question(this,ui("Externen Link ausführen"),t.item->link)==QMessageBox::Yes&&!QDesktopServices::openUrl(QUrl::fromUserInput(t.item->link)))
        QMessageBox::warning(this,ui("Externen Link ausführen"),ui("Fehler beim Ausführen des externen Links:")+u'\n'+t.item->link);
    return true;
}
void Editor::linkToParent(){
    bool any=false;for(const auto &p:allComponents(doc))any|=p.item->parent;
    if(!any){QMessageBox::information(this,ui("Verknüpfe mit PARENT..."),ui("Keine Parent-Bauteile definiert"));return;}
    ParentChoiceDialog dialog(doc,path,this);if(dialog.exec()!=QDialog::Accepted||dialog.parentId().isEmpty())return;
    const QString parent=dialog.parentId();const QStringList ids=sheetView->selection();
    // As in the reference: a child shows its parent's designator and value.
    change([&](Document &d){
        for(auto &i:d.sheet().items){
            if(!ids.contains(i.id)||i.type!=ItemType::Component||i.id==parent)continue;
            i.parentId=parent;i.parent=false;i.autoNumber=false;
            if(!i.designator.contains(QStringLiteral("<PARENT_ID>"),Qt::CaseInsensitive))i.designator=QStringLiteral("<PARENT_ID>");
            if(!i.value.contains(QStringLiteral("<PARENT_"),Qt::CaseInsensitive))i.value=QStringLiteral("<PARENT_VALUE>");
        }
    });
}
void Editor::contentsDialog(){
    QStringList names;for(const auto &s:doc.sheets)names<<s.name;
    SheetChoiceDialog choice(ui("Inhaltsverzeichnis einfügen"),names,this);if(choice.exec()!=QDialog::Accepted||choice.sheets().isEmpty())return;
    ContentsDialog options(this);if(options.exec()!=QDialog::Accepted)return;
    insertContents(choice.sheets(),options.drawing());
}
void Editor::insertContents(const QList<int> &sheets,const PartsDrawing &drawing){
    // A table of the sheets: number and name, put where a parts list would go.
    PartsTable table;table.header<<ui("Blatt")<<ui("Name");
    for(int s:sheets)if(s>=0&&s<doc.sheets.size())table.rows<<QStringList{QString::number(s+1),doc.sheets[s].name};
    insertPartsList(table,drawing);
}
void Editor::partsListDialog(){
    QStringList names;for(const auto &s:doc.sheets)names<<s.name;
    QList<int> sheets;
    if(doc.sheets.size()>1){SheetChoiceDialog choice(ui("Stückliste erstellen"),names,this);if(choice.exec()!=QDialog::Accepted)return;sheets=choice.sheets();if(sheets.isEmpty())return;}
    PartsListDialog dialog(doc,sheets,path,this);
    dialog.insert=[this](const PartsTable &table,const PartsDrawing &drawing){insertPartsList(table,drawing);};
    dialog.help=[this,&dialog]{showHelp(QStringLiteral("listen"),&dialog);};
    dialog.exec();
}
void Editor::insertPartsList(const PartsTable &table,const PartsDrawing &drawing){
    // At the top left of the title block's frame, or of the sheet; selected, to be moved where it belongs.
    const QRectF frame=doc.sheet().titleBlock.frame;
    const QPointF at=frame.isEmpty()?QPointF(15,15):frame.topLeft()+QPointF(5,5);
    const Item group=partsGroup(table,at,drawing);
    change([&](Document &d){d.sheet().items<<group;});
    sheetView->setSelection({group.id});
}
void Editor::librarySheets(const QList<int> &pages){
    // Each page on a new sheet of the current size, its symbols in rows.
    const auto &all=libraryPanel->pages();
    change([&](Document &d){
        const Sheet source=d.sheet();
        for(int p:pages){
            if(p<0||p>=all.size())continue;
            Sheet s=newSheet(localized(all[p].name),source.width,source.height,source.grid);
            double x=15,y=25,row=0;
            for(const auto &e:all[p].entries){
                Item item=placedSymbol(e,d);const QRectF b=bounds(item);
                if(x+b.width()>s.width-15&&x>15){x=15;y+=row+12;row=0;}
                const double g=std::max(1.27,s.grid);
                item.pos=QPointF(std::round((x-b.left())/g)*g,std::round((y-b.top())/g)*g);
                for(auto it=e.resources.cbegin();it!=e.resources.cend();++it)d.resources.insert(it.key(),it.value());
                s.items<<item;x+=b.width()+10;row=std::max(row,b.height());
            }
            d.sheets.append(s);
        }
        d.activeSheet=int(d.sheets.size())-1;
    });
    sheetView->fitSheet();
}
QList<QAction*> Editor::modeActions() const{
    QList<QAction*> out;for(auto *a:toolGroup->actions()){out<<a;if(a->objectName()==u"toolTextBox")if(auto *b=findChild<QAction*>("toolBitmap"))out<<b;}
    return out;
}
void Editor::applySettings(const GeneralSettings &g){
    general=g;
    // The keys of the drawing modes, standard where none is set.
    for(auto *a:modeActions()){
        if(!standardKeys.contains(a->objectName()))standardKeys.insert(a->objectName(),a->shortcut());
        a->setShortcut(g.hotkeys.contains(a->objectName())?QKeySequence(g.hotkeys[a->objectName()]):standardKeys[a->objectName()]);
    }
    if(!autosaveTimer){autosaveTimer=new QTimer(this);connect(autosaveTimer,&QTimer::timeout,this,[this]{autosave();});}
    if(g.autosaveMinutes>0)autosaveTimer->start(g.autosaveMinutes*60000);else autosaveTimer->stop();
    sheetView->componentTextsWithKey=g.componentTextsWithKey;sheetView->componentTextKey=g.componentTextKey;
    sheetView->gridContrast=g.gridContrast;sheetView->gridMarks=g.gridMarks;sheetView->gridLines=g.gridLines;sheetView->gridOverTitleBlock=g.gridOverTitleBlock;sheetView->whiteBackground=g.whiteBackground;
    sheetView->update();
}
void Editor::loadPreferences(){
    preferences=true;QSettings s;applySettings(GeneralSettings::load());
    auto apply=[&](const char *name,const char *key,bool fallback){
        if(auto *b=findChild<QToolButton*>(name))b->setChecked(s.value(QStringLiteral("schematic/")+key,fallback).toBool());};
    apply("gridSnap","gridSnap",true);apply("angleSnap","angleSnap",true);apply("terminalSnap","terminalSnap",true);apply("rubberBand","rubberBand",true);
    apply("textsTurned","textsTurned",true);apply("textsMirrored","textsMirrored",false);
    libraryPanel->setColumns(s.value("schematic/libraryColumns",2).toInt());
    libraryPanel->setCaptions(s.value("schematic/libraryCaptions",true).toBool());
    reloadLibrary();
    libraryPanel->showPage(s.value("schematic/libraryPage").toString());
    for(auto [kind,key]:{std::pair{Work::Drawings,"schematic/lastDrawingFolder"},std::pair{Work::Forms,"schematic/lastFormFolder"},std::pair{Work::Exports,"schematic/lastExportFolder"}})
        if(s.contains(QLatin1String(key)))lastFolders.insert(int(kind),s.value(QLatin1String(key)).toString());
    // The presets of the drawing modes, as they were left.
    for(auto [key,preset]:presets()){
        const QByteArray json=s.value(QStringLiteral("schematic/presets/")+key).toByteArray();if(json.isEmpty())continue;
        try{Item i=itemFromJson(QJsonDocument::fromJson(json).object(),false);if(i.type==preset->type){i.points=preset->points;i.text=preset->text;i.id.clear();*preset=i;}}
        catch(const std::exception &){}
    }
}
QList<std::pair<QString,Item*>> Editor::presets(){
    return {{QStringLiteral("line"),&sheetView->linePreset},{QStringLiteral("shape"),&sheetView->shapePreset},{QStringLiteral("junction"),&sheetView->junctionPreset},
            {QStringLiteral("text"),&sheetView->textPreset},{QStringLiteral("textBox"),&sheetView->textBoxPreset},{QStringLiteral("designator"),&sheetView->designatorPreset},{QStringLiteral("value"),&sheetView->valuePreset},{QStringLiteral("contact"),&sheetView->contactPreset},{QStringLiteral("label"),&sheetView->labelPreset},{QStringLiteral("dimension"),&sheetView->dimensionPreset}};
}

// --- document and history
void Editor::setDocument(const Document &document,const QString &file){
    doc=document;if(doc.sheets.isEmpty())doc.sheets.append(newSheet(ui("Blatt %1").arg(1)));
    doc.activeSheet=qBound(0,doc.activeSheet,int(doc.sheets.size())-1);
    path=file;pathVersion=0;importedName.clear();modified=false;past.clear();future.clear();notes.clear();if(noticeBar)noticeBar->hide();
    if(editingComponent()){sheetView->setComponentMode({});modeBar->hide();}
    if(sheetView->titleBlockMode())setTitleBlockMode(false);
    sheetView->setDocument(&doc);sheetView->fileName=path;
    refresh();sheetView->fitSheet();
}
void Editor::newDocument(){Document d=schematic::newDocument(ui("Blatt %1").arg(1));d.sheets[0]=presetSheet(d.sheets[0].name,d);setDocument(d);}
Sheet Editor::presetSheet(const QString &name,Document &document) const{
    Sheet s=newSheet(name,general.newSheetWidth,general.newSheetHeight,general.newSheetGrid);
    if(!general.newSheetForm.isEmpty()){
        QFile f(general.newSheetForm);
        if(f.open(QIODevice::ReadOnly)&&f.size()<64*1024*1024)
            try{QMap<QString,Resource> pictures;s.titleBlock.items=splan::readTitleBlock(f.readAll(),&pictures);s.titleBlock.name=QFileInfo(general.newSheetForm).completeBaseName();
                for(auto it=pictures.cbegin();it!=pictures.cend();++it)document.resources.insert(it.key(),it.value());}catch(const FormatError &){}
    }
    return s;
}
QString Editor::startFolder(Work kind,const QString &fallback) const{
    const QString set=kind==Work::Drawings?general.drawingFolder:kind==Work::Forms?general.formWorkFolder:general.exportFolder;
    if(!set.isEmpty())return set;
    if(!lastFolders.value(int(kind)).isEmpty())return lastFolders.value(int(kind));
    return fallback;
}
void Editor::usedFolder(Work kind,const QString &file){if(!file.isEmpty())lastFolders.insert(int(kind),QFileInfo(file).absolutePath());}
// "Autospeichern": the document beside its file as "<name>.bak", in the own format.
void Editor::autosave(){
    if(path.isEmpty()||!modified)return;
    const QFileInfo info(path);QSaveFile f(info.dir().filePath(info.completeBaseName()+QStringLiteral(".bak")));
    if(f.open(QIODevice::WriteOnly)&&f.write(encode(doc))>0)f.commit();
}
bool Editor::newFromTemplate(const QString &file,QString *error){
    try{
        // A new document with the template's content and ids of its own, not yet saved anywhere.
        Document d=load(file);d.id=newId();
        QList<QList<Item>*> lists;for(auto &s:d.sheets){s.id=newId();lists<<&s.items<<&s.titleBlock.items;}
        freshIds(lists);setDocument(d);return true;
    }catch(const FormatError &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
void Editor::snapshot(){past.append(doc);if(past.size()>undoSteps)past.removeFirst();future.clear();}
void Editor::touched(){modified=true;dropUnusedResources(doc);refresh();}
void Editor::change(const std::function<void(Document&)> &edit){snapshot();edit(doc);touched();}
void Editor::undo(){if(past.isEmpty())return;future.append(doc);doc=past.takeLast();modified=true;refresh();}
void Editor::redo(){if(future.isEmpty())return;past.append(doc);doc=future.takeLast();modified=true;refresh();}
void Editor::refresh(){
    if(editingComponent()&&!sheetView->editedComponent()){sheetView->setComponentMode({});modeBar->hide();refreshActions();}
    sheetView->documentChanged();refreshTabs();refreshSheetList();propertiesPanel->refresh();refreshActions();refreshTitle();refreshProblems();
}
void Editor::refreshSheetList(){
    if(!sheetList)return;
    refreshing=true;
    // Anew, so that moved rows are in order again.
    sheetList->setRowCount(0);sheetList->setRowCount(int(doc.sheets.size()));
    for(int i=0;i<doc.sheets.size();i++){auto *item=new QTableWidgetItem(doc.sheets[i].name);item->setToolTip(doc.sheets[i].description);sheetList->setItem(i,0,item);}
    sheetList->setCurrentCell(doc.activeSheet,0);
    refreshing=false;
}
void Editor::refreshProblems(){if(auto *a=action("problems"))a->setVisible(problems().any());}
Editor::Problems Editor::problems() const{
    Problems out;
    if(!editingComponent()){
        const Sheet &s=doc.sheet();
        for(const auto &i:s.items){const QRectF b=bounds(i);if(b.right()<0||b.bottom()<0||b.left()>s.width||b.top()>s.height)out.outside<<i.id;}
    }
    for(const auto &p:placedImages(doc)){
        const Item *i=imageWithId(doc,p.sheet,p.id);if(!i||i->size.width()<=0||i->size.height()<=0)continue;
        auto it=pictureSizes.find(i->resource);
        if(it==pictureSizes.end()){
            // Only the picture's header is read.
            QBuffer buffer;buffer.setData(doc.resources.value(i->resource).data);buffer.open(QIODevice::ReadOnly);
            it=pictureSizes.insert(i->resource,QImageReader(&buffer).size());
        }
        if(!it->isValid())continue;
        if(std::min(it->width()/(i->size.width()/25.4),it->height()/(i->size.height()/25.4))>300)out.pictures++;
    }
    return out;
}
void Editor::moveOntoSheet(const QStringList &ids){
    if(ids.isEmpty()||editingComponent())return;
    change([&](Document &d){
        Sheet &s=d.sheet();
        for(auto &i:s.items){
            if(!ids.contains(i.id))continue;
            const QRectF b=bounds(i);QPointF delta;
            if(b.right()<0)delta.rx()=-b.left();
            if(b.bottom()<0)delta.ry()=-b.top();
            if(b.left()>s.width)delta.rx()=s.width-b.right();
            if(b.top()>s.height)delta.ry()=s.height-b.bottom();
            schematic::move(i,delta);
        }
    });
    sheetView->setSelection(ids);
}
void Editor::problemDialog(){
    const Problems p=problems();
    ProblemDialog dialog(int(p.outside.size()),p.pictures,this);
    if(problemShown)problemShown(&dialog);
    if(dialog.exec()!=QDialog::Accepted)return;
    // Straight in the sheet's elements: in the title block mode the view does not select them.
    if(dialog.choice==ProblemDialog::Delete){const QStringList ids=p.outside;sheetView->setSelection({});change([ids](Document &d){d.sheet().items.removeIf([&](const Item &i){return ids.contains(i.id);});});}
    else if(dialog.choice==ProblemDialog::Move)moveOntoSheet(p.outside);
    else if(dialog.choice==ProblemDialog::Pictures)bitmapExplorer();
}
void Editor::showHelp(const QString &section,QWidget *over){
    QWidget *owner=over?over:this;
    auto *window=owner->findChild<QDialog*>("helpWindow",Qt::FindDirectChildrenOnly);
    if(!window){window=new QDialog(owner);window->setObjectName("helpWindow");window->setWindowTitle(ui("Schaltplan – Hilfe"));window->resize(760,640);
        auto *layout=new QVBoxLayout(window);layout->setContentsMargins(0,0,0,0);auto *page=new QTextBrowser(window);page->setObjectName("helpPage");page->setOpenExternalLinks(true);layout->addWidget(page);
        page->setSource(QUrl(QStringLiteral("qrc:/help/schematic/%1.html").arg(uiLanguage())));}
    if(auto *page=window->findChild<QTextBrowser*>("helpPage");page&&!section.isEmpty())page->scrollToAnchor(section);
    window->show();window->raise();window->activateWindow();
}
void Editor::refreshTabs(){
    refreshing=true;
    while(tabs->count()>doc.sheets.size())tabs->removeTab(tabs->count()-1);
    while(tabs->count()<doc.sheets.size())tabs->addTab(QString());
    // As the reference: with their numbers ("Blätter mit Seitennummer").
    for(int i=0;i<doc.sheets.size();i++){tabs->setTabText(i,doc.sheetNumbers?QStringLiteral("%1: %2").arg(i+1).arg(doc.sheets[i].name):doc.sheets[i].name);tabs->setTabToolTip(i,doc.sheets[i].description);}
    tabs->setCurrentIndex(doc.activeSheet);
    refreshing=false;
}
void Editor::refreshActions(){
    // An element of a group chosen alone counts as no selection here: it is neither moved nor deleted on its own.
    const auto sel=selectedItems();
    const bool any=!sel.isEmpty();
    bool component=false,group=false;for(const auto &i:sel){component|=i.type==ItemType::Component;group|=i.type==ItemType::Group;}
    const bool sheetMode=!editingComponent()&&!sheetView->titleBlockMode();
    for(const char *n:{"cut","copy","duplicate","delete","toFront","toBack","oneForward","oneBack","alignGrid","rotate","rotateBy","mirror","mirrorVertical"})action(n)->setEnabled(any);
    if(sheetView->guideChosen())action("delete")->setEnabled(true);
    action("group")->setEnabled(sel.size()>1);action("ungroup")->setEnabled(group);
    action("dissolveComponent")->setEnabled(component&&sheetMode);action("componentEditor")->setEnabled(sel.size()==1&&component&&sheetMode);
    action("makeComponent")->setEnabled(any&&sheetMode);
    action("copyToLibrary")->setEnabled(component&&sheetMode);
    {   // Named for what it copies, as in sPlan: one component, several, or a parent with its children.
        int components=0;bool children=false;QSet<QString> parents;
        for(const auto &i:sel)if(i.type==ItemType::Component){if(i.parent)parents.insert(i.id);}
        for(const auto &i:sel)if(i.type==ItemType::Component){if(parents.contains(i.parentId))children=true;else components++;}
        action("copyToLibrary")->setText(components>1?ui("Markierte Bauteile in die Bibliothek &kopieren"):children?ui("Markiertes Bauteil (mit Children) in die Bibliothek &kopieren"):ui("Markiertes Bauteil in die Bibliothek &kopieren"));
    }
    for(const char *n:{"scale","colourize","penWidth"})action(n)->setEnabled(any);action("copyClipToLibrary")->setEnabled(any&&sheetMode);
    action("joinLines")->setEnabled(sel.size()==2&&sel[0].type==ItemType::Line&&sel[1].type==ItemType::Line);
    action("undo")->setEnabled(canUndo());action("redo")->setEnabled(canRedo());
    action("deleteSheet")->setEnabled(doc.sheets.size()>1&&sheetMode);
    for(const char *n:{"newSheet","copySheet","sortSheets","loadTitleBlock","saveTitleBlock","generateTitleBlock","removeTitleBlock"})action(n)->setEnabled(sheetMode);
    action("editTitleBlock")->setEnabled(!editingComponent());action("editTitleBlock")->setChecked(sheetView->titleBlockMode());
    action("toolContact")->setVisible(editingComponent());
    action("toolNetLabel")->setEnabled(sheetMode);action("toolSheetReference")->setEnabled(sheetMode);
    gridLabel->setText(ui("Raster: %1 %2").arg(uiLocale().toString(doc.sheet().grid*doc.sheet().scale),unitName(doc.sheet().scaleUnit)));
    zoomLabel->setText(ui("Zoom: %1").arg(uiLocale().toString(sheetView->zoom(),'f',2)));
    scaleLabel->setText(QStringLiteral("1:%1\n%2").arg(uiLocale().toString(doc.sheet().scale),unitName(doc.sheet().scaleUnit)));
}
QString Editor::displayName() const{
    if(!path.isEmpty())return QFileInfo(path).fileName();
    if(!importedName.isEmpty())return importedName;
    return ui("Neuer Schaltplan");
}
void Editor::refreshTitle(){
    setWindowTitle(QStringLiteral("%1[*] – %2").arg(displayName(),ui("Schaltplan")));setWindowModified(modified);
    if(titleChanged)titleChanged();
}
QList<Item> Editor::selectedItems() const{
    QList<Item> out;for(const auto &i:sheetView->items())if(sheetView->selection().contains(i.id))out<<i;return out;
}

// --- files
bool Editor::openFile(const QString &file,QString *error){
    try{
        QFile f(file);if(!f.open(QIODevice::ReadOnly))throw FormatError(f.errorString());
        if(f.size()>128*1024*1024)throw FormatError(ui("Datei ist zu groß"));
        const QByteArray bytes=f.readAll();
        const int version=splan::version(bytes);
        if(version){
            QStringList found;const Document d=splan::read(bytes,&found);
            setDocument(d,file);pathVersion=version;showNotes(found);
        }else setDocument(decode(bytes),file);
        refreshTitle();rememberFile(file);
        // "Datei beim Öffnen automatisch sichern": a copy beside it, as it was opened.
        if(general.backupOnOpen){
            const QFileInfo info(file);QSaveFile copy(info.dir().filePath(QStringLiteral("Backup_of_")+info.fileName()));
            if(copy.open(QIODevice::WriteOnly)&&copy.write(bytes)==bytes.size())copy.commit();
        }
        return true;
    }catch(const FormatError &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
bool Editor::saveFile(const QString &file,QString *error){
    const QString suffix=QFileInfo(file).suffix().toLower();
    const int version=suffix==u"spl8"?80:suffix==u"spl7"?70:0;
    try{if(version)splan::save(doc,file,version);else schematic::save(doc,file);}
    catch(const FormatError &e){if(error)*error=QString::fromUtf8(e.what());return false;}
    path=file;pathVersion=version;importedName.clear();modified=false;sheetView->fileName=path;refreshTitle();rememberFile(file);return true;
}
bool Editor::exportSplan(const QString &file,int version,QString *error){
    try{splan::save(doc,file,version);return true;}catch(const FormatError &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
void Editor::showNotes(const QStringList &list){
    notes=list;
    if(list.isEmpty()){noticeBar->hide();return;}
    noticeText->setText(ui("Teilvorschau der sPlan-Datei:")+QStringLiteral("\n• ")+list.join(QStringLiteral("\n• ")));noticeBar->show();
}
void Editor::exportSplanDialog(int version){
    const QString suffix=version==80?QStringLiteral(".spl8"):QStringLiteral(".spl7");
    auto file=QFileDialog::getSaveFileName(this,version==80?ui("Als sPlan 8 exportieren"):ui("Als sPlan 7 exportieren"),
        QDir(startFolder(Work::Drawings,QStringLiteral("."))).filePath(QFileInfo(displayName()).completeBaseName()+suffix),version==80?ui("sPlan 8 (*.spl8)"):ui("sPlan 7 (*.spl7)"));
    if(file.isEmpty())return;usedFolder(Work::Drawings,file);if(QFileInfo(file).suffix().isEmpty())file+=suffix;
    QStringList lost;
    try{lost=splan::losses(doc,version);}catch(const FormatError &e){QMessageBox::warning(this,ui("Exportieren"),QString::fromUtf8(e.what()));return;}
    if(!lost.isEmpty()&&QMessageBox::question(this,ui("Exportieren"),ui("Beim Schreiben als sPlan-Datei geht verloren:")+QStringLiteral("\n• ")+lost.join(QStringLiteral("\n• "))+QStringLiteral("\n\n")+ui("Trotzdem exportieren?"))!=QMessageBox::Yes)return;
    QString error;if(!exportSplan(file,version,&error))QMessageBox::warning(this,ui("Exportieren"),error);
}
bool Editor::save(){
    if(saveHandler)return saveHandler(false);
    if(path.isEmpty())return saveAs();
    if(pathVersion){
        // Back into the sPlan file only while nothing is lost.
        QStringList lost;try{lost=splan::losses(doc,pathVersion);}catch(const FormatError &e){lost<<QString::fromUtf8(e.what());}
        if(!lost.isEmpty()){
            QMessageBox::information(this,ui("Speichern"),ui("Der Schaltplan lässt sich nicht verlustfrei als sPlan-Datei speichern:")+QStringLiteral("\n• ")+lost.join(QStringLiteral("\n• "))
                                     +QStringLiteral("\n\n")+ui("Er wird als OpenLoch-Schaltplan gespeichert; die sPlan-Datei bleibt unverändert."));
            return saveAs();
        }
    }
    QString error;if(saveFile(path,&error))return true;
    QMessageBox::warning(this,ui("Speichern"),error);return false;
}
bool Editor::saveAs(){
    if(saveHandler)return saveHandler(true);
    QString suggestion=path;if(suggestion.isEmpty()||pathVersion)suggestion=QFileInfo(suggestion.isEmpty()?(importedName.isEmpty()?ui("Schaltplan"):importedName):suggestion).completeBaseName()+".olsch";
    if(QFileInfo(suggestion).isRelative()&&!startFolder(Work::Drawings).isEmpty())suggestion=QDir(startFolder(Work::Drawings)).filePath(suggestion);
    auto file=QFileDialog::getSaveFileName(this,ui("Schaltplan speichern"),suggestion,ui("OpenLoch-Schaltpläne (*.olsch)"));
    if(file.isEmpty())return false;usedFolder(Work::Drawings,file);if(QFileInfo(file).suffix().isEmpty())file+=".olsch";
    QString error;if(saveFile(file,&error))return true;
    QMessageBox::warning(this,ui("Speichern"),error);return false;
}
void Editor::markSaved(){modified=false;refreshTitle();}
bool Editor::maybeSave(){
    if(!modified)return true;
    const auto answer=QMessageBox::question(this,ui("Schaltplan"),ui("Der Schaltplan „%1“ wurde geändert. Änderungen speichern?").arg(displayName()),
                                            QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);
    if(answer==QMessageBox::Cancel)return false;
    if(answer==QMessageBox::Save)return save()&&!modified;
    return true;
}
void Editor::closeEvent(QCloseEvent *event){
    if(editingComponent())leaveComponentEditor(true);
    if(!maybeSave()){event->ignore();return;}
    if(preferences){
        QSettings s;
        for(const char *n:{"gridSnap","angleSnap","terminalSnap","rubberBand","textsTurned","textsMirrored"})
            if(auto *b=findChild<QToolButton*>(n))s.setValue(QStringLiteral("schematic/")+n,b->isChecked());
        s.setValue("schematic/libraryColumns",libraryPanel->columns());
        s.setValue("schematic/libraryCaptions",libraryPanel->captionsShown());
        s.setValue("schematic/libraryPage",libraryPanel->pageKey(libraryPanel->currentPage()));
        for(auto [kind,key]:{std::pair{Work::Drawings,"schematic/lastDrawingFolder"},std::pair{Work::Forms,"schematic/lastFormFolder"},std::pair{Work::Exports,"schematic/lastExportFolder"}})
            if(lastFolders.contains(int(kind)))s.setValue(QLatin1String(key),lastFolders[int(kind)]);
        // Stored as elements; a line needs two points and a net label a name to be read again, which loading leaves out.
        for(auto [key,preset]:presets()){
            Item i=*preset;i.id.clear();if(i.points.size()<2)i.points={QPointF(),QPointF(1,0),QPointF()};if(i.type==ItemType::NetLabel)i.text=QStringLiteral("N");
            if(i.type==ItemType::Line)i.points.resize(2);
            if(i.type==ItemType::TextBox&&i.size.isEmpty())i.size=QSizeF(10,5);
            s.setValue(QStringLiteral("schematic/presets/")+key,QJsonDocument(itemToJson(i)).toJson(QJsonDocument::Compact));
        }
    }
    event->accept();
}
bool Editor::exportPdfFile(const QString &file,bool blackAndWhite,bool allSheets,QString *error){
    RenderOptions o;o.blackAndWhite=blackAndWhite;o.fileName=path;
    QList<int> sheets;if(allSheets){for(int i=0;i<doc.sheets.size();i++)if(!doc.sheets[i].spare)sheets<<i;}else sheets<<doc.activeSheet;
    return exportPdf(doc,sheets,file,o,error);
}
QRectF Editor::exportFrame(ExportArea area) const{
    const Sheet &sheet=doc.sheet();
    if(area==ExportArea::Sheet)return QRectF(0,0,sheet.width,sheet.height);
    // In the component editor the component as it lies on the sheet (its parts are in its own coordinates).
    if(const Item *c=sheetView->editedComponent()){
        const QRectF r=placement(*c).mapRect(bounds(c->children));
        return r.isEmpty()?QRectF():r.adjusted(-4,-4,4,4);
    }
    const QList<Item> items=area==ExportArea::Selection?selectedItems():sheetView->items();
    return items.isEmpty()?QRectF():bounds(items).adjusted(-4,-4,4,4);
}
namespace {
// Whether a picture of `mm` at `dpi` stays within 16384 pixels a side and 512 MB.
bool pictureFits(QSizeF mm,double dpi){
    const double w=mm.width()*dpi/25.4,h=mm.height()*dpi/25.4;
    return w<=16384&&h<=16384&&w*h*4<=512.*1024*1024;
}
QString tooLarge(QSizeF mm,double dpi){
    return ui("Das Bild wäre mit %1 × %2 Pixel zu groß. Bitte eine kleinere Auflösung wählen oder weniger auswählen.").arg(std::lround(mm.width()*dpi/25.4)).arg(std::lround(mm.height()*dpi/25.4));
}
// Draws the current sheet's frame or the selected elements in it, the painter in millimetres at the frame's corner.
void paintFrame(QPainter &p,const Document &doc,const QRectF &frame,const QList<Item> &chosen,bool selectionOnly,const RenderOptions &o){
    p.translate(-frame.topLeft());if(o.paper)p.fillRect(frame,QColor(255,255,255));
    if(selectionOnly)paintItems(p,chosen,doc,doc.activeSheet,o);else paintSheet(p,doc,doc.activeSheet,o);
}
}
QImage Editor::clipboardImage(int dpi,bool selectionOnly) const{
    if(sheetView->editedComponent())selectionOnly=false;
    const QRectF frame=exportFrame(selectionOnly?ExportArea::Selection:ExportArea::Elements);
    if(frame.isEmpty())return pictureFits(QSizeF(doc.sheet().width,doc.sheet().height),dpi)?renderSheet(doc,doc.activeSheet,dpi/25.4):QImage();
    if(!pictureFits(frame.size(),dpi))return {};
    const double ppm=dpi/25.4;
    QImage image(std::max(1,int(std::lround(frame.width()*ppm))),std::max(1,int(std::lround(frame.height()*ppm))),QImage::Format_RGB32);image.fill(Qt::white);
    image.setDotsPerMeterX(int(std::lround(ppm*1000)));image.setDotsPerMeterY(int(std::lround(ppm*1000)));
    RenderOptions o;o.fileName=path;
    {QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.scale(ppm,ppm);paintFrame(p,doc,frame,selectedItems(),selectionOnly,o);}
    return image;
}
bool Editor::exportImage(const QString &file,int dpi,bool blackAndWhite,bool allSheets,bool transparent,QString *error,ExportArea area){
    if(sheetView->editedComponent()&&area==ExportArea::Selection)area=ExportArea::Elements;
    RenderOptions o;o.blackAndWhite=blackAndWhite;o.paper=!transparent;o.fileName=path;
    const QFileInfo info(file);const bool svg=info.suffix().compare("svg",Qt::CaseInsensitive)==0,emf=info.suffix().compare("emf",Qt::CaseInsensitive)==0;
    const bool png=info.suffix().compare("png",Qt::CaseInsensitive)==0;
    if(!allSheets&&area!=ExportArea::Sheet){
        // All elements (with the title block) or only the selected ones, in their frame.
        const QRectF frame=exportFrame(area);const QList<Item> chosen=area==ExportArea::Selection?selectedItems():QList<Item>();
        if(frame.isEmpty()){if(error)*error=area==ExportArea::Selection?ui("Es sind keine Elemente markiert."):ui("Das Blatt enthält keine Elemente.");return false;}
        auto draw=[&](QPainter &p){paintFrame(p,doc,frame,chosen,area==ExportArea::Selection,o);};
        if(!svg&&!emf&&!pictureFits(frame.size(),dpi)){if(error)*error=tooLarge(frame.size(),dpi);return false;}
        bool ok=false;
        if(svg||emf){QSaveFile f(file);ok=f.open(QIODevice::WriteOnly)&&f.write(svg?svgDrawing(frame.size(),doc.sheet().name,draw):emfDrawing(frame.size(),draw))>0&&f.commit();}
        else{
            const double ppm=dpi/25.4;QImage image(std::max(1,int(std::lround(frame.width()*ppm))),std::max(1,int(std::lround(frame.height()*ppm))),QImage::Format_ARGB32_Premultiplied);
            image.fill(o.paper||!png?QColor(255,255,255):QColor(0,0,0,0));
            image.setDotsPerMeterX(int(std::lround(ppm*1000)));image.setDotsPerMeterY(int(std::lround(ppm*1000)));
            {QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.scale(ppm,ppm);draw(p);}
            ok=image.save(file);
        }
        if(!ok&&error)*error=ui("Das Bild konnte nicht geschrieben werden: %1").arg(file);
        return ok;
    }
    // Whole sheets; with all of them each but the spare ones in a file of its own.
    QList<int> sheets;if(allSheets){for(int i=0;i<doc.sheets.size();i++)if(!doc.sheets[i].spare)sheets<<i;}else sheets<<doc.activeSheet;
    for(int s:sheets){
        QString target=file;
        if(allSheets)target=info.dir().filePath(QStringLiteral("%1_%2.%3").arg(info.completeBaseName()).arg(s+1).arg(info.suffix()));
        if(svg||emf){
            QSaveFile f(target);
            if(!f.open(QIODevice::WriteOnly)||f.write(svg?sheetSvg(doc,s,o):sheetEmf(doc,s,o))<=0||!f.commit()){if(error)*error=ui("Das Bild konnte nicht geschrieben werden: %1").arg(target);return false;}
            continue;
        }
        const QSizeF size(doc.sheets[s].width,doc.sheets[s].height);
        if(!pictureFits(size,dpi)){if(error)*error=tooLarge(size,dpi);return false;}
        QImage image=renderSheet(doc,s,dpi/25.4,o);
        if(!png&&transparent){QImage flat(image.size(),QImage::Format_RGB32);flat.fill(Qt::white);QPainter p(&flat);p.drawImage(0,0,image);image=flat;}
        if(!image.save(target)){if(error)*error=ui("Das Bild konnte nicht geschrieben werden: %1").arg(target);return false;}
    }
    return true;
}

// --- sheets
void Editor::switchSheet(int index){
    if(index<0||index>=doc.sheets.size()||index==doc.activeSheet)return;
    if(editingComponent())leaveComponentEditor(true);
    doc.activeSheet=index;sheetView->setSelection({});sheetView->fitSheet();refresh();
}
void Editor::insertSheets(int position,int count,bool copies,int from){
    position=qBound(0,position,int(doc.sheets.size()));
    change([&](Document &d){
        const Sheet source=from>=0&&from<d.sheets.size()?d.sheets[from]:d.sheet();
        for(int k=0;k<count;k++){
            Sheet s;
            if(copies){s=source;s.id=newId();freshIds(QList<QList<Item>*>{&s.items,&s.titleBlock.items});}
            else s=presetSheet(ui("Blatt %1").arg(d.sheets.size()+1),d);
            d.sheets.insert(position+k,s);
        }
        d.activeSheet=position;
    });
    sheetView->fitSheet();
}
void Editor::removeSheet(int index){
    if(doc.sheets.size()<2||index<0||index>=doc.sheets.size())return;
    change([index](Document &d){d.sheets.removeAt(index);d.activeSheet=std::min(index,int(d.sheets.size())-1);});
}
void Editor::reorderSheets(const QList<int> &order){
    if(order.size()!=doc.sheets.size())return;
    change([order](Document &d){
        QList<Sheet> sorted;for(int i:order)sorted<<d.sheets[i];
        const QString active=d.sheet().id;d.sheets=sorted;d.activeSheet=std::max(0,sheetIndex(d,active));
    });
}
void Editor::insertSheetDialog(bool copies){
    QStringList names;for(const auto &s:doc.sheets)names<<s.name;
    SheetInsertDialog dialog(copies?ui("Blatt kopieren"):ui("Blatt einfügen"),names,doc.activeSheet,this);
    if(dialog.exec()==QDialog::Accepted)insertSheets(dialog.position(),dialog.count(),copies);
}
void Editor::sortSheetsDialog(){
    QStringList names;for(const auto &s:doc.sheets)names<<s.name;
    SheetSortDialog dialog(names,this);
    if(dialog.exec()==QDialog::Accepted){
        const auto order=dialog.order();bool same=true;for(int i=0;i<order.size();i++)same&=order[i]==i;
        if(!same)reorderSheets(order);
    }
}
void Editor::showSheetMenu(QPoint at,int index){
    if(index>=0)switchSheet(index);
    QMenu menu(this);
    for(const char *n:{"sheetProperties","","newSheet","copySheet","deleteSheet","","sortSheets"}){if(!*n)menu.addSeparator();else menu.addAction(action(n));}
    menu.exec(at);
}

// --- title block and component editor
void Editor::setTitleBlockMode(bool on){
    if(on&&editingComponent())return;
    sheetView->setTitleBlockMode(on);
    modeTitle->setText(QStringLiteral("   ")+ui("Formblatt bearbeiten"));modeOk->setText(ui("Beenden"));modeCancel->hide();modeBar->setVisible(on);
    refreshActions();propertiesPanel->refresh();
}
void Editor::setTitleBlock(const TitleBlock &block){
    TitleBlock b=block;for(auto &i:b.items)assignIds(i);
    change([b](Document &d){d.sheet().titleBlock=b;});
}
bool Editor::loadTitleBlock(const QString &file,QString *error){
    try{
        QFile in(file);if(!in.open(QIODevice::ReadOnly))throw FormatError(in.errorString());
        if(in.size()>64*1024*1024)throw FormatError(ui("Datei ist zu groß"));
        QMap<QString,Resource> pictures;const QList<Item> items=splan::readTitleBlock(in.readAll(),&pictures);
        change([&](Document &d){
            d.sheet().titleBlock.items=items;
            for(auto it=pictures.cbegin();it!=pictures.cend();++it)if(!d.resources.contains(it.key()))d.resources.insert(it.key(),it.value());
        });
        return true;
    }catch(const FormatError &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
bool Editor::saveTitleBlock(const QString &file,QString *error){
    try{
        const QByteArray bytes=splan::writeTitleBlock(doc.sheet().titleBlock.items,doc.resources,80);
        QSaveFile out(file);if(!out.open(QIODevice::WriteOnly))throw FormatError(out.errorString());
        if(out.write(bytes)!=bytes.size()||!out.commit())throw FormatError(out.errorString());
        return true;
    }catch(const FormatError &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
void Editor::titleBlockDialog(){
    TitleBlockDialog dialog(doc.sheet().titleBlock,doc.sheet().width,doc.sheet().height,this);
    if(dialog.exec()==QDialog::Accepted)setTitleBlock(dialog.titleBlock());
}
bool Editor::editingComponent() const{return !sheetView->componentMode().isEmpty();}
void Editor::editComponent(){
    if(editingComponent()||sheetView->titleBlockMode())return;
    const auto sel=selectedItems();if(sel.size()!=1||sel[0].type!=ItemType::Component)return;
    componentBefore=sel[0];sheetView->setTool(SheetView::Tool::Select);sheetView->setComponentMode(sel[0].id);
    modeTitle->setText(QStringLiteral("   ")+ui("Bauteileditor"));modeOk->setText(ui("Speichern und Beenden"));modeCancel->show();modeBar->show();
    menus.value(QStringLiteral("B&auteileditor"))->menuAction()->setVisible(true);
    refreshActions();propertiesPanel->refresh();
}
void Editor::leaveComponentEditor(bool keep){
    if(!editingComponent())return;
    const QString id=sheetView->componentMode();
    sheetView->setTool(SheetView::Tool::Select);sheetView->setComponentMode({});
    modeBar->hide();menus.value(QStringLiteral("B&auteileditor"))->menuAction()->setVisible(false);
    if(!keep){
        const Item before=componentBefore;const Item *now=findItem(doc.sheet(),id);
        if(now&&!(*now==before))change([before,id](Document &d){if(Item *c=findItem(d.sheet(),id))*c=before;});
    }
    sheetView->setSelection({id});sheetView->fitSheet();refreshActions();propertiesPanel->refresh();
}

// --- editing the selection
void Editor::deleteSelection(){
    const QStringList ids=sheetView->selection();if(ids.isEmpty())return;
    snapshot();QList<Item> &list=sheetView->items();
    // In the component editor, designator and value stay: they can be hidden, not deleted.
    for(int i=int(list.size())-1;i>=0;i--)if(ids.contains(list[i].id)&&!(editingComponent()&&list[i].role!=TextRole::Plain))list.removeAt(i);
    touched();
}
void Editor::copySelection(){
    const auto sel=selectedItems();if(sel.isEmpty())return;
    QJsonArray items;QJsonObject resources;
    std::function<void(const Item&)> pictures=[&](const Item &i){
        if(i.type==ItemType::Image&&doc.resources.contains(i.resource)){const auto &r=doc.resources[i.resource];resources[i.resource]=QJsonObject{{"kind",r.kind},{"data",QString::fromLatin1(r.data.toBase64())}};}
        for(const auto &c:i.children)pictures(c);};
    for(const auto &i:sel){items.append(itemToJson(i));pictures(i);}
    auto *mime=new QMimeData;mime->setData(clipboardType,QJsonDocument(QJsonObject{{"items",items},{"resources",resources}}).toJson(QJsonDocument::Compact));
    QApplication::clipboard()->setMimeData(mime);
}
void Editor::cutSelection(){copySelection();deleteSelection();}
void Editor::pasteClipboard(){
    const QMimeData *mime=QApplication::clipboard()->mimeData();if(!mime||!mime->hasFormat(clipboardType))return;
    const auto o=QJsonDocument::fromJson(mime->data(clipboardType)).object();
    QList<Item> items;
    try{for(const auto &v:o["items"].toArray())items<<itemFromJson(v,false);}catch(const FormatError &){return;}
    freshIds(items);
    if(items.isEmpty())return;
    const auto res=o["resources"].toObject();
    for(auto it=res.begin();it!=res.end();++it)if(!doc.resources.contains(it.key())){const auto r=it.value().toObject();
        const QByteArray data=QByteArray::fromBase64(r["data"].toString().toLatin1());
        if(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex()==it.key().toLatin1())doc.resources.insert(it.key(),{r["kind"].toString(),data});}
    const QPointF anchor=sheetView->onGrid(bounds(items).topLeft());
    for(auto &i:items)schematic::move(i,-anchor);
    sheetView->setTool(SheetView::Tool::Select);sheetView->beginPlacement(items);
}
void Editor::duplicateSelection(){
    auto sel=selectedItems();if(sel.isEmpty())return;
    const QPointF shift=sheetView->onGrid(QPointF(5,5));
    freshIds(sel);QStringList ids;for(auto &i:sel){schematic::move(i,shift);ids<<i.id;}
    change([this,sel](Document &){sheetView->items().append(sel);});
    sheetView->setSelection(ids);
}
namespace {
// Keeps the rotation of texts (only their place turns) when texts are not to be turned completely.
void keepTextAngles(Item &now,const Item &before){
    if(isText(now))now.rotation=before.rotation;
    if(now.type==ItemType::Group)for(int i=0;i<now.children.size()&&i<before.children.size();i++)keepTextAngles(now.children[i],before.children[i]);
}
}
void Editor::rotateSelection(double degrees){
    const auto sel=selectedItems();if(sel.isEmpty())return;
    QRectF box;for(const auto &i:sel)box=box.united(bounds(i));
    QPointF pivot=sheetView->onGrid(box.center());
    if(sel.size()==1&&(sel[0].type==ItemType::Component||isText(sel[0])||sel[0].type==ItemType::Junction))pivot=sel[0].pos;
    const QStringList ids=sheetView->selection();const bool whole=sheetView->textsTurned;
    change([&](Document &){for(auto &i:sheetView->items())if(ids.contains(i.id)){const Item was=i;rotate(i,pivot,degrees);if(!whole)keepTextAngles(i,was);}});
}
void Editor::mirrorSelection(bool vertically){
    const auto sel=selectedItems();if(sel.isEmpty())return;
    QRectF box;for(const auto &i:sel)box=box.united(bounds(i));
    QPointF axis=sheetView->onGrid(box.center());if(sel.size()==1&&sel[0].type==ItemType::Component)axis=sel[0].pos;
    const QStringList ids=sheetView->selection();const bool texts=sheetView->textsMirrored;
    change([&](Document &){for(auto &i:sheetView->items())if(ids.contains(i.id)){if(vertically)mirrorVertically(i,axis.y(),texts);else mirror(i,axis.x(),texts);}});
}
void Editor::groupSelection(){
    const QStringList ids=sheetView->selection();if(ids.size()<2)return;
    Item group;group.type=ItemType::Group;group.id=newId();
    change([&](Document &){
        QList<Item> &list=sheetView->items();int at=-1;
        for(int i=0;i<list.size();){if(ids.contains(list[i].id)){if(at<0)at=i;group.children<<list.takeAt(i);}else i++;}
        list.insert(std::max(0,at),group);
    });
    sheetView->setSelection({group.id});
}
void Editor::ungroupSelection(){
    QStringList ids;
    change([&](Document &){
        QList<Item> &list=sheetView->items();
        for(int i=0;i<list.size();i++){
            if(list[i].type!=ItemType::Group||!sheetView->selection().contains(list[i].id))continue;
            const auto children=list.takeAt(i).children;for(int k=0;k<children.size();k++){list.insert(i+k,children[k]);ids<<children[k].id;}
            i+=int(children.size())-1;
        }
    });
    sheetView->setSelection(ids);
}
void Editor::reorderSelection(int how){
    const QStringList ids=sheetView->selection();if(ids.isEmpty())return;
    change([&](Document &){
        QList<Item> &list=sheetView->items();
        if(how<2){
            QList<Item> moved,rest;for(const auto &i:list)(ids.contains(i.id)?moved:rest)<<i;
            list=how==0?rest+moved:moved+rest;
        }else if(how==2){for(int i=int(list.size())-2;i>=0;i--)if(ids.contains(list[i].id)&&!ids.contains(list[i+1].id))list.swapItemsAt(i,i+1);}
        else for(int i=1;i<list.size();i++)if(ids.contains(list[i].id)&&!ids.contains(list[i-1].id))list.swapItemsAt(i,i-1);
    });
}
void Editor::alignToGrid(){
    const QStringList ids=sheetView->selection();if(ids.isEmpty())return;
    change([&](Document &){
        for(auto &i:sheetView->items())if(ids.contains(i.id)){const QPointF p=gridPoint(i);schematic::move(i,sheetView->onGrid(p)-p);}
    });
}
void Editor::nodeToGrid(const QString &id,int node){
    change([&](Document &){for(auto &i:sheetView->items())if(i.id==id&&node>=0&&node<i.points.size())i.points[node]=sheetView->onGrid(i.points[node]);});
}
void Editor::nodesToGrid(const QString &id){
    change([&](Document &){for(auto &i:sheetView->items())if(i.id==id)for(auto &p:i.points)p=sheetView->onGrid(p);});
}
void Editor::convertTo(const QString &id,ItemType type){
    const Item *item=nullptr;for(const auto &i:sheetView->items())if(i.id==id)item=&i;
    if(!item||!convertedTo(*item,type))return;
    change([&](Document &){for(auto &i:sheetView->items())if(i.id==id)if(const auto c=convertedTo(i,type))i=*c;});
}
void Editor::newGuide(bool vertical){
    // In the middle of the visible part of the sheet.
    const QRectF visible=QRectF(sheetView->toSheet(QPointF(SheetView::rulerSize,SheetView::rulerSize)),sheetView->toSheet(QPointF(sheetView->width(),sheetView->height())))
                         .intersected(QRectF(0,0,doc.sheet().width,doc.sheet().height));
    const QPointF middle=sheetView->onGrid(visible.isEmpty()?QPointF(doc.sheet().width/2,doc.sheet().height/2):visible.center());
    if(!sheetView->addGuide(vertical,vertical?middle.x():middle.y()))
        QMessageBox::information(this,ui("Magnetlinien"),ui("Es können zur Zeit keine Magnetlinien hinzugefügt werden.")+QStringLiteral("\n")+ui("Die Magnetlinien sind momentan fixiert oder ausgeblendet."));
}
void Editor::setDimensionKind(DimensionKind kind){
    // An angle shows degrees: its suffix is ° unless one was set.
    Item &p=sheetView->dimensionPreset;const QString degrees=QString::fromUtf8("°");
    if(kind==DimensionKind::Angle&&p.suffix.isEmpty())p.suffix=degrees;
    else if(kind!=DimensionKind::Angle&&p.suffix==degrees)p.suffix.clear();
    p.dimension=kind;sheetView->setTool(SheetView::Tool::Dimension);
}
void Editor::chooseSpecialShape(SpecialShape shape){
    // The first four shapes ask for their settings first; cancelled, the mode stays as it was.
    if(shape==SpecialShape::RegularPolygon||shape==SpecialShape::Star||shape==SpecialShape::Grid||shape==SpecialShape::Wave){
        SpecialShapeDialog dialog(sheetView->specialOptions,int(shape),this);
        if(dialog.exec()!=QDialog::Accepted){for(auto *t:toolGroup->actions())if(t->data().toInt()==int(sheetView->tool()))t->setChecked(true);return;}
        sheetView->specialOptions=dialog.options();
    }
    sheetView->special=shape;sheetView->setTool(SheetView::Tool::Special);
}
void Editor::childListDialog(const QString &parentId){
    // The table follows the pointer until a click puts it down.
    ChildListDialog dialog(doc,parentId,path,this);
    if(dialog.exec()!=QDialog::Accepted)return;
    const PartsTable table=dialog.table();if(table.rows.isEmpty())return;
    sheetView->setTool(SheetView::Tool::Select);sheetView->beginPlacement({partsGroup(table,QPointF(),dialog.drawing())});
}
void Editor::bitmapExplorer(){
    BitmapExplorerDialog dialog(this);
    dialog.document=[this]{return &doc;};
    dialog.change=[this](const std::function<void(Document&)> &f){change(f);};
    dialog.showImage=[this](int sheet,const QString &id){switchSheet(sheet);sheetView->setSelection({id});};
    dialog.refresh();dialog.exec();
}
void Editor::alignSelection(Alignment how){
    const QStringList ids=sheetView->selection();if(ids.size()<2)return;
    change([&](Document &){QList<Item*> list;for(auto &i:sheetView->items())if(ids.contains(i.id))list<<&i;align(list,how);});
}
void Editor::spreadSelection(bool horizontally){
    const QStringList ids=sheetView->selection();if(ids.size()<3)return;
    change([&](Document &){QList<Item*> list;for(auto &i:sheetView->items())if(ids.contains(i.id))list<<&i;spread(list,horizontally);});
}
void Editor::makeComponent(){
    const auto sel=selectedItems();if(sel.isEmpty()||editingComponent()||sheetView->titleBlockMode())return;
    const QPointF insertion=sheetView->onGrid(bounds(sel).topLeft());
    QList<Item> children;for(auto i:sel){schematic::move(i,-insertion);if(i.type==ItemType::Line)i.electrical=false;children<<i;}
    Item c=schematic::makeComponent(children,insertion,QStringLiteral("X?"),QString(),&sheetView->designatorPreset,&sheetView->valuePreset);
    for(auto &k:c.children)if(k.id.isEmpty())k.id=newId();
    c.id=newId();c.designator=nextDesignator(doc,c.designator);c.extra={QString(),QString(),QString(),QString()};
    const QStringList ids=sheetView->selection();
    change([&](Document &){
        QList<Item> &list=sheetView->items();int at=-1;
        for(int i=0;i<list.size();){if(ids.contains(list[i].id)){if(at<0)at=i;list.removeAt(i);}else i++;}
        list.insert(std::max(0,at),c);
    });
    sheetView->setSelection({c.id});
}
void Editor::dissolveComponents(){
    QStringList ids;
    change([&](Document &){
        QList<Item> &list=sheetView->items();
        for(int i=0;i<list.size();i++){
            if(list[i].type!=ItemType::Component||!sheetView->selection().contains(list[i].id))continue;
            const Item c=list.takeAt(i);QList<Item> parts;
            for(Item child:c.children){
                if(isText(child)){
                    Item t=placedText(child,c);t.type=ItemType::Text;
                    t.text=child.role==TextRole::Designator?c.designator:child.role==TextRole::Value?c.value:child.text;
                    t.role=TextRole::Plain;t.name.clear();t.hasPin=false;t.pin=QPointF();
                    if((child.role==TextRole::Designator&&!c.designatorVisible)||(child.role==TextRole::Value&&!c.valueVisible)||t.text.isEmpty())continue;
                    parts<<t;continue;
                }
                if(c.mirrored)mirror(child,0,true);
                rotate(child,QPointF(),c.rotation);schematic::move(child,c.pos);parts<<child;
            }
            for(int k=0;k<parts.size();k++){list.insert(i+k,parts[k]);ids<<parts[k].id;}
            i+=int(parts.size())-1;
        }
    });
    sheetView->setSelection(ids);
}

// --- dialogs and menus
void Editor::chooseGrid(){
    bool ok;const double g=QInputDialog::getDouble(this,ui("Raster"),ui("Raster [mm]:"),doc.sheet().grid,.001,1000,3,&ok);
    if(ok&&g!=doc.sheet().grid)change([g](Document &d){d.sheet().grid=g;});
}
void Editor::variablesDialog(){
    VariablesDialog dialog(doc.variables,this);
    if(variablesShown)variablesShown(&dialog);
    if(dialog.exec()==QDialog::Accepted){const auto v=dialog.variables();if(!(v==doc.variables))change([v](Document &d){d.variables=v;});}
}
bool Editor::extendedText(QString &text,const QString &family){
    auto names=[this]{QStringList out;for(const auto &v:doc.variables)out<<v.name;return out;};
    TextDialog dialog(text,names(),this,textConstants(),family);
    dialog.defineVariables=[this,names]{variablesDialog();return names();};
    dialog.defineConstants=[this]{editTextConstants();return textConstants();};
    if(textDialogShown)textDialogShown(&dialog);
    if(dialog.exec()!=QDialog::Accepted)return false;
    text=dialog.text();return true;
}
bool Editor::askText(Item &item){
    if(item.type==ItemType::Text||item.type==ItemType::TextBox){
        QString text=item.text;
        if(!extendedText(text,item.font.family)||(item.type==ItemType::Text&&text.isEmpty()))return false;
        item.text=text;return true;
    }
    bool ok;
    if(item.type==ItemType::Contact){
        int next=1;if(const Item *c=sheetView->editedComponent())next=int(contacts(*c).size())+1;
        const QString name=QInputDialog::getText(this,ui("Kontakt"),ui("Name des Kontakts (etwa die Anschlussnummer):"),QLineEdit::Normal,QString::number(next),&ok);
        if(!ok||name.isEmpty())return false;
        item.name=name;item.text=name;return true;
    }
    const QString name=QInputDialog::getText(this,item.global?ui("Blattverweis"):ui("Netzname"),ui("Name des Netzes:"),QLineEdit::Normal,item.text,&ok).trimmed();
    if(!ok||name.isEmpty())return false;
    item.text=name;return true;
}
void Editor::textDialog(){
    const auto sel=selectedItems();if(sel.size()!=1)return;
    if(sel[0].type==ItemType::Component)editItem(sel[0].id);else editText(sel[0].id);
}
void Editor::editItem(const QString &id){
    Item *item=nullptr;for(auto &i:sheetView->items())if(i.id==id)item=&i;
    if(!item)return;
    if(item->type==ItemType::Component&&!editingComponent()){sheetView->setSelection({id});editComponent();return;}
    // A double click on a linked text follows the link; its text is changed in the properties panel.
    if(item->type==ItemType::Text&&followLink(id))return;
    editText(id);
}
void Editor::editText(const QString &id){
    Item *item=nullptr;for(auto &i:sheetView->items())if(i.id==id)item=&i;
    if(!item)return;
    if(item->type==ItemType::Text||item->type==ItemType::TextBox||item->type==ItemType::NetLabel||item->type==ItemType::Contact){
        if(item->role!=TextRole::Plain)return;
        Item edited=*item;
        if(item->type==ItemType::Contact){bool ok;const QString t=QInputDialog::getText(this,ui("Kontakt"),ui("Text:"),QLineEdit::Normal,item->text,&ok);if(!ok)return;edited.text=t;}
        else if(!askText(edited))return;
        if(!(edited==*item)){const QString text=edited.text;change([this,id,text](Document &){for(auto &i:sheetView->items())if(i.id==id)i.text=text;});}
    }
}
void Editor::showContextMenu(QPoint at,const QString &id,int node){
    QMenu menu(this);
    const Item *item=nullptr;for(const auto &i:sheetView->items())if(i.id==id)item=&i;
    if(item&&node>=0&&(item->type==ItemType::Line||item->type==ItemType::Polygon||item->type==ItemType::Bezier)){
        menu.addAction(ui("Diesen Knoten am Raster ausrichten"),this,[this,id,node]{nodeToGrid(id,node);});
        menu.addAction(ui("Alle Knoten am Raster ausrichten"),this,[this,id]{nodesToGrid(id);});
        if(item->type!=ItemType::Bezier){
            menu.addAction(ui("Knoten entfernen"),this,[this,id,node]{sheetView->removeNode(id,node);});
            if(item->type==ItemType::Line&&node>0&&node<item->points.size()-1)menu.addAction(ui("Linie auftrennen"),this,[this,id,node]{sheetView->splitLine(id,node);});
        }
        // "Linien verbinden" on the end of a line that another line's end touches.
        if(item->type==ItemType::Line&&(node==0||node==item->points.size()-1)){
            const QPointF at=item->points[node];QString other;
            for(const auto &i:sheetView->items())if(i.id!=id&&i.type==ItemType::Line&&!i.points.isEmpty()&&
                (std::hypot(i.points.first().x()-at.x(),i.points.first().y()-at.y())<1e-6||std::hypot(i.points.last().x()-at.x(),i.points.last().y()-at.y())<1e-6)){other=i.id;break;}
            if(!other.isEmpty())menu.addAction(ui("Linien verbinden"),this,[this,id,other]{sheetView->setSelection({id,other});sheetView->joinLines();});
        }
        menu.addSeparator();
    }
    if(item){
        for(const char *n:{"cut","copy","paste","duplicate","delete","","toFront","toBack","","alignGrid","","rotateBy","scale","mirror","mirrorVertical","","group","ungroup"}){if(!*n)menu.addSeparator();else menu.addAction(action(n));}
        if(item->type==ItemType::Dimension){menu.addSeparator();menu.addAction(action(item->autoValue?"fixDimensions":"unfixDimensions"));}
        if(convertedTo(*item,ItemType::Line)){
            menu.addSeparator();
            if(item->type!=ItemType::Line)menu.addAction(ui("Wandeln in Linie"),this,[this,id]{convertTo(id,ItemType::Line);});
            if(item->type!=ItemType::Polygon)menu.addAction(ui("Wandeln in Polygon"),this,[this,id]{convertTo(id,ItemType::Polygon);});
            if(convertedTo(*item,ItemType::Bezier))menu.addAction(ui("Wandeln in Kurve"),this,[this,id]{convertTo(id,ItemType::Bezier);});
        }
        if(item->type==ItemType::Component){
            menu.addSeparator();
            // Moving a text of the component without a modifier key: it follows the pointer until a click.
            if(const int t=sheetView->contextComponentText;t>=0)menu.addAction(ui("Bauteiltext verschieben"),this,[this,id,t]{sheetView->startComponentTextMove(id,t);});
            menu.addAction(action("componentEditor"));menu.addAction(action("dissolveComponent"));
        }
        if(item->type==ItemType::Component&&item->parent&&!childrenOf(doc,item->id).isEmpty())
            menu.addAction(ui("Childliste (Kontaktspiegel) einfügen"),this,[this,id]{childListDialog(id);});
        menu.addSeparator();if(item->type==ItemType::Component)menu.addAction(action("copyToLibrary"));menu.addAction(action("copyClipToLibrary"));
    }else{menu.addAction(action("paste"));menu.addAction(action("selectAll"));menu.addSeparator();menu.addAction(action("sheetProperties"));}
    menu.exec(at);
}
void Editor::exportDialog(){
    // As in the reference first the choices, then the file.
    const bool component=editingComponent();
    ExportDialog dialog(QSizeF(doc.sheet().width,doc.sheet().height),exportFrame(ExportArea::Elements).size(),component?QSizeF():exportFrame(ExportArea::Selection).size(),int(doc.sheets.size()),this);
    if(exportShown)exportShown(&dialog);
    if(dialog.exec()!=QDialog::Accepted)return;
    const QString suffix=dialog.suffix();const bool all=dialog.allSheets->isChecked();
    const QHash<QString,QString> filters{{"jpg",ui("JPEG-Bild (*.jpg)")},{"png",ui("PNG-Bild (*.png)")},{"bmp",ui("Windows-Bitmap (*.bmp)")},
                                         {"emf",ui("EMF-Grafik (*.emf)")},{"svg",ui("SVG-Grafik (*.svg)")},{"pdf",ui("PDF-Dokument (*.pdf)")}};
    if(all&&suffix!=u"pdf"&&QMessageBox::information(this,ui("Exportieren"),ui("Jedes Blatt wird in eine eigene Datei geschrieben, die Blattnummer angehängt (Name_1, Name_2 …); Reserveblätter nicht."),
                                                     QMessageBox::Ok|QMessageBox::Cancel)!=QMessageBox::Ok)return;
    auto file=QFileDialog::getSaveFileName(this,ui("Exportieren"),QDir(startFolder(Work::Exports,QStringLiteral("."))).filePath(QFileInfo(displayName()).completeBaseName()+u'.'+suffix),filters.value(suffix));
    if(file.isEmpty())return;usedFolder(Work::Exports,file);if(QFileInfo(file).suffix().isEmpty())file+=u'.'+suffix;
    if(all&&suffix!=u"pdf"){
        // The numbered files that are there already, asked about once.
        const QFileInfo info(file);QStringList there;
        for(int i=0;i<doc.sheets.size();i++)if(!doc.sheets[i].spare){const QString t=QStringLiteral("%1_%2.%3").arg(info.completeBaseName()).arg(i+1).arg(info.suffix());if(info.dir().exists(t))there<<t;}
        if(!there.isEmpty()&&QMessageBox::question(this,ui("Exportieren"),ui("Diese Dateien gibt es schon. Überschreiben?")+QStringLiteral("\n• ")+there.join(QStringLiteral("\n• ")))!=QMessageBox::Yes)return;
    }
    QString error;
    const bool ok=suffix==u"pdf"?exportPdfFile(file,dialog.blackAndWhite->isChecked(),all,&error)
                               :exportImage(file,dialog.dpi->value(),dialog.blackAndWhite->isChecked(),all,dialog.transparent->isEnabled()&&dialog.transparent->isChecked(),&error,dialog.area());
    if(!ok)QMessageBox::warning(this,ui("Exportieren"),error);
}
}
