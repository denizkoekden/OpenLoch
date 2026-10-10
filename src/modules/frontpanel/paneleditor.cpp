#include "paneleditor.h"
#include "emf.h"
#include "panelgeometry.h"
#include "panelicons.h"
#include "panelmachining.h"
#include "panelprint.h"
#include "panelrender.h"
#include "panelsidebar.h"
#include "panelview.h"
#include "panelwizards.h"
#include "strokefont.h"
#include "fpl.h"
#include "language.h"
#include "legacy_reader.h"
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTabBar>
#include <QtEndian>
#include <QTimer>
#include <QTextBrowser>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace openloch::frontpanel {
namespace {
constexpr const char *clipboardMime="application/x-openloch-frontpanel";
Element *findIn(QList<Element> &list,const QString &id){for(auto &e:list){if(e.id==id)return &e;if(Element *inside=findIn(e.children,id))return inside;}return nullptr;}
// The selected elements in drawing order, without those inside another selected element.
QList<Element*> rootsOf(QList<Element> &list,const QStringList &ids){QList<Element*> out;for(auto &e:list){if(ids.contains(e.id))out<<&e;else out+=rootsOf(e.children,ids);}return out;}
// The geometry without pen width (the outline the original aligns and measures).
QRectF shapeOf(const Element &e){
    if(e.isContainer()&&!e.combined()){QRectF r;for(const auto &c:e.children){const QRectF b=shapeOf(c);if(b.isNull())continue;r=r.isNull()?b:r.united(b);}return r;}
    QRectF r=elementPath(e).boundingRect();if(e.type==ElementType::Text&&r.isEmpty())r=frameCorners(e.frame).boundingRect();return r;
}
QRectF shapeOf(const QList<Element*> &list){QRectF r;for(const auto *e:list){const QRectF b=shapeOf(*e);if(b.isNull())continue;r=r.isNull()?b:r.united(b);}return r;}
void usedResources(const QList<Element> &list,QStringList &keys){for(const auto &e:list){if(!e.resource.isEmpty()&&!keys.contains(e.resource))keys<<e.resource;usedResources(e.children,keys);}}
QString number(double v,int decimals){return uiLocale().toString(v,'f',decimals);}
QDoubleSpinBox *field(double low,double high,int decimals,const QString &tip,const QString &suffix=QStringLiteral(" mm")){
    auto *s=new QDoubleSpinBox;s->setRange(low,high);s->setDecimals(decimals);s->setSuffix(suffix);s->setToolTip(tip);s->setKeyboardTracking(false);s->setLocale(uiLocale());s->setMinimumWidth(96);return s;
}
// A text keeps its corner and direction; width and height follow the text height.
void refit(Element &t,double height){
    if(t.frame.size()!=3)return;const QPointF p0=t.frame[0];QPointF along=t.frame[1]-p0,down=t.frame[2]-p0;
    const double la=std::hypot(along.x(),along.y()),ld=std::hypot(down.x(),down.y());along=la>0?along/la:QPointF(1,0);down=ld>0?down/ld:QPointF(0,1);
    t.frame={p0,p0+along*naturalTextWidth(t,height),p0+down*height};
}
double textHeight(const Element &t){const QPolygonF c=frameCorners(t.frame);return c.size()==4?QLineF(c[0],c[3]).length():0;}
void putOnClipboard(const Document &document,const QList<Element> &list){
    if(list.isEmpty())return;
    Document clip;clip.panels[0].width=document.panel().width;clip.panels[0].height=document.panel().height;clip.panels[0].elements=list;
    QStringList keys;usedResources(list,keys);for(const auto &k:keys)clip.resources.insert(k,document.resources.value(k));
    auto *mime=new QMimeData;mime->setData(clipboardMime,clip.encode());
    // A picture of the elements for other programs: 300 dpi, but at most 4000 pixels on the longer side.
    const QRectF b=elementsBounds(list);
    if(b.isValid()){
        const double s=std::min(300/25.4,4000/std::max(b.width(),b.height()));QImage image(QSize(std::max(1,int(std::ceil(b.width()*s))),std::max(1,int(std::ceil(b.height()*s)))),QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);image.setDotsPerMeterX(int(std::lround(s*1000)));image.setDotsPerMeterY(image.dotsPerMeterX());
        {QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.scale(s,s);p.translate(-b.topLeft());for(const auto &e:list)paintElement(p,document,e);}
        mime->setImageData(image);
    }
    QApplication::clipboard()->setMimeData(mime);
}
// Projects, libraries and backups: the native format is JSON, everything else is read as FrontDesigner file.
Document readAny(const QString &file,QStringList *notes,QString *format){
    QFile f(file);if(!f.open(QIODevice::ReadOnly))throw FormatError(f.errorString());
    if(f.size()>256LL*1024*1024)throw FormatError(ui("Datei ist zu groß"));
    const QByteArray data=f.readAll();const QString suffix=QFileInfo(file).suffix().toLower();
    if(data.trimmed().startsWith('{')){*format="native";return Document::decode(data);}
    Document d;
    if(suffix=="bak"){try{d=frontdesigner::readFrontDesigner(data,false,notes);*format="fpl";}catch(const std::exception&){d=frontdesigner::readFrontDesigner(data,true,notes);*format="lib";}}
    else{const bool library=suffix=="lib";d=frontdesigner::readFrontDesigner(data,library,notes);*format=library?"lib":"fpl";}
    d.title=QFileInfo(file).completeBaseName();return d;
}
}

PanelEditor::PanelEditor(QWidget *parent):QMainWindow(parent){
    setObjectName("frontPanelEditor");setDockNestingEnabled(true);
    area=new PanelView;tabs=new QTabBar;tabs->setShape(QTabBar::RoundedSouth);tabs->setExpanding(false);tabs->setContextMenuPolicy(Qt::CustomContextMenu);
    tabs->setToolTip(ui("Frontplatten des Projekts"));
    auto *centre=new QWidget;auto *column=new QVBoxLayout(centre);column->setContentsMargins(0,0,0,0);column->setSpacing(0);column->addWidget(area,1);column->addWidget(tabs);setCentralWidget(centre);
    area->setDocument(&doc);
    side=new PanelSidebar(this);sideDock=new QDockWidget(ui("Bibliothek"),this);sideDock->setObjectName("library");sideDock->setWidget(side);addDockWidget(Qt::RightDockWidgetArea,sideDock);
    tree=new QTreeWidget;tree->setHeaderHidden(true);tree->setSelectionMode(QAbstractItemView::ExtendedSelection);tree->setContextMenuPolicy(Qt::CustomContextMenu);
    treeDock=new QDockWidget(ui("Objektbaum"),this);treeDock->setObjectName("objectTree");treeDock->setWidget(tree);addDockWidget(Qt::RightDockWidgetArea,treeDock);treeDock->hide();
    autosave=new QTimer(this);connect(autosave,&QTimer::timeout,this,[this]{writeBackup();});
    buildActions();buildToolbars();buildMenus();

    area->beforeChange=[this]{begin();};
    area->afterChange=[this]{commit();};
    area->selectionChanged=[this]{adoptStyle();updateSelectionTools();updateTreeSelection();updateActions();};
    area->toolChanged=[this](PanelView::Tool tool){
        static const QMap<int,QString> names{{PanelView::Select,"tool-select"},{PanelView::Rotate,"tool-rotate"},{PanelView::Zoom,"tool-zoom"},{PanelView::Line,"tool-line"},
            {PanelView::Polygon,"tool-polygon"},{PanelView::Rectangle,"tool-rectangle"},{PanelView::Circle,"tool-circle"},{PanelView::Arc,"tool-arc"},{PanelView::Text,"tool-text"},
            {PanelView::Drill,"tool-drill"},{PanelView::Dimension,"tool-dimension"},{PanelView::Origin,"tool-origin"}};
        if(QAction *a=action(names.value(int(tool))))a->setChecked(true);else if(QAction *a=toolGroup->checkedAction())a->setChecked(false);
    };
    area->pointerMoved=[this](QPointF p){const Panel &pl=doc.panel();const QPointF d=p-pl.origin;
        const double unit=pl.inch?25.4:1;const int decimals=pl.inch?3:2;
        position->setText(ui("X: %1 %3   Y: %2 %3").arg(number(d.x()/unit,decimals),number(d.y()/unit,decimals),pl.inch?QStringLiteral("inch"):QStringLiteral("mm")));};
    area->newElement=[this](ElementType type){return styled(type);};
    area->textRequested=[this](QPointF at){addText(at);};
    area->drillRequested=[this](QPointF at){addDrill(at);};
    area->propertiesRequested=[this](const QString &id){showProperties(id);};
    area->contextMenuRequested=[this](QPoint global,int node){showContextMenu(global,node);};
    area->dimensionRequested=[this](QPointF a,QPointF b,QPointF at){addDimension(a,b,at);};
    connect(tabs,&QTabBar::currentChanged,this,[this](int i){if(!refreshing&&i>=0)selectPanel(i);});
    connect(tabs,&QTabBar::tabBarDoubleClicked,this,[this](int i){if(i>=0){selectPanel(i);action("panelProperties")->trigger();}});
    connect(tabs,&QTabBar::customContextMenuRequested,this,[this](QPoint at){
        const int i=tabs->tabAt(at);if(i>=0)selectPanel(i);
        QMenu menu;for(const char *name:{"addPanel","duplicatePanel","removePanel","","panelProperties","","panelsFromFile","","panelLeft","panelRight"}){if(*name)menu.addAction(action(name));else menu.addSeparator();}
        menu.exec(tabs->mapToGlobal(at));
    });
    connect(tree,&QTreeWidget::itemSelectionChanged,this,[this]{
        if(treeSync)return;QStringList ids;for(auto *item:tree->selectedItems())ids<<item->data(0,Qt::UserRole).toString();
        treeSync=true;area->setSelection(ids);treeSync=false;
    });
    connect(tree,&QTreeWidget::customContextMenuRequested,this,[this](QPoint at){showTreeMenu(at);});
    connect(tree,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *item){if(item)showProperties(item->data(0,Qt::UserRole).toString());});
    connect(QApplication::clipboard(),&QClipboard::dataChanged,this,[this]{updateActions();});
    setAcceptDrops(true);
    loadSettings();
    setDocument(Document());
}
PanelEditor::~PanelEditor()=default;

// ------------------------------------------------------------------ actions, menus, toolbars
QAction *PanelEditor::add(const QString &name,const QString &text,const QString &icon,const std::function<void()> &run,const QList<QKeySequence> &keys){
    auto *a=new QAction(icon.isEmpty()?QIcon():panelIcon(icon),text,this);a->setObjectName(name);if(!keys.isEmpty())a->setShortcuts(keys);
    QString tip=QString(text).remove('&');if(tip.endsWith(QStringLiteral("…")))tip.chop(1);if(!keys.isEmpty())tip+=QString(" (%1)").arg(keys.first().toString(QKeySequence::NativeText));a->setToolTip(tip);
    addAction(a);if(run)connect(a,&QAction::triggered,this,[run]{run();});actions.insert(name,a);return a;
}
void PanelEditor::buildActions(){
    // File
    add("new",ui("&Neu…"),"new",[this]{newDocument();},{QKeySequence::New});
    add("open",ui("Ö&ffnen…"),"open",[this]{
        if(openHandler){openHandler(startFolder());return;}
        if(!maybeSave())return;
        const QString file=QFileDialog::getOpenFileName(this,ui("Frontplatte öffnen"),startFolder(),ui("Frontplatten (*.olfp *.fpl *.FPL *.lib *.LIB *.bak *.BAK);;OpenLoch-Frontplatten (*.olfp);;FrontDesigner (*.fpl *.FPL *.lib *.LIB);;Sicherungen (*.bak *.BAK)"));
        if(!file.isEmpty())open(file);
    },{QKeySequence::Open,QKeySequence(Qt::Key_F3)});
    add("save",ui("&Speichern"),"save",[this]{save();},{QKeySequence::Save,QKeySequence(Qt::Key_F2)});
    add("saveAs",ui("Speichern &unter…"),"",[this]{saveAs();},{QKeySequence::SaveAs,QKeySequence(Qt::Key_F4)});
    add("autosave",ui("AutoSpeichern…"),"",[this]{if(!askAutosave(this,autosaveMinutes))return;if(autosaveMinutes>0)autosave->start(autosaveMinutes*60000);else autosave->stop();saveSettings();},{QKeySequence(Qt::CTRL|Qt::Key_B)});
    add("importImage",ui("&Importieren…"),"tool-image",[this]{importImage();},{QKeySequence(Qt::CTRL|Qt::Key_I)});
    add("exportImage",ui("&Grafik…"),"export-image",[this]{exportImage();},{QKeySequence(Qt::CTRL|Qt::Key_E)});
    add("exportHpgl",ui("&HPGL-Bearbeitungsdateien…"),"export-hpgl",[this]{exportHpgl();});
    add("print",ui("&Drucken…"),"print",[this]{print();},{QKeySequence::Print});
    add("quit",ui("&Beenden"),"",[this]{close();});
    // Edit
    add("undo",ui("&Rückgängig"),"undo",[this]{undo();},{QKeySequence::Undo});
    add("redo",ui("&Wiederherstellen"),"redo",[this]{redo();},{QKeySequence::Redo,QKeySequence(Qt::CTRL|Qt::Key_Y)});
    add("cut",ui("A&usschneiden"),"cut",[this]{cut();},{QKeySequence::Cut});
    add("copy",ui("&Kopieren"),"copy",[this]{copy();},{QKeySequence::Copy});
    add("paste",ui("&Einfügen"),"paste",[this]{paste();},{QKeySequence::Paste});
    add("duplicate",ui("&Duplizieren"),"duplicate",[this]{duplicate();},{QKeySequence(Qt::CTRL|Qt::Key_D)});
    add("selectAll",ui("&Alles auswählen"),"",[this]{area->selectAll();},{QKeySequence::SelectAll});
    add("delete",ui("&Löschen"),"delete",[this]{removeSelected();},{QKeySequence::Delete,QKeySequence(Qt::Key_Backspace)});
    add("adoptPen",ui("&Stift"),"",[this]{applyPen(currentPen,currentMachining);});
    add("adoptFill",ui("&Füllung"),"",[this]{applyFill(currentFill);});
    add("adoptFont",ui("Sch&rift"),"",[this]{applyFont(currentFont,currentTextHeight,currentBold,currentItalic,currentStrokeFont);});
    add("properties",ui("&Eigenschaften…"),"properties",[this]{showProperties(area->lastSelected());});
    add("rename",ui("&Umbenennen…"),"",[this]{
        const QString id=area->lastSelected();Element *e=findIn(doc.panel().elements,id);if(!e)return;bool ok=false;
        const QString name=QInputDialog::getText(this,ui("Umbenennen"),ui("Name:"),QLineEdit::Normal,e->name,&ok);
        if(ok)change([&](Document &d){if(Element *t=findIn(d.panel().elements,id))t->name=name;});
    });
    // Arrange
    add("front",ui("Nach &vorne setzen"),"front",[this]{toFront();});
    add("back",ui("Nach &hinten setzen"),"back",[this]{toBack();});
    add("group",ui("&Gruppe bilden"),"group",[this]{group();},{QKeySequence(Qt::CTRL|Qt::Key_G)});
    add("ungroup",ui("Gruppe auf&lösen"),"ungroup",[this]{ungroup();},{QKeySequence(Qt::CTRL|Qt::Key_U)});
    add("combine",ui("&Kombination bilden"),"combine",[this]{combine();},{QKeySequence(Qt::CTRL|Qt::Key_K)});
    add("uncombine",ui("Kombination a&uflösen"),"uncombine",[this]{uncombine();},{QKeySequence(Qt::CTRL|Qt::Key_L)});
    add("distribute",ui("&Verteilen…"),"distribute",[this]{if(askDistribute(this,distributeOptions))distribute(distributeOptions);},{QKeySequence(Qt::CTRL|Qt::ALT|Qt::Key_V)});
    add("alignGrid",ui("Am &Raster ausrichten…"),"align-grid",[this]{if(askAlignToGrid(this,gridOptions))alignToGrid(gridOptions);},{QKeySequence(Qt::CTRL|Qt::ALT|Qt::Key_R)});
    add("mirrorVertical",ui("Vertikal spiegeln"),"mirror-vertical",[this]{mirror(false);});
    add("mirrorHorizontal",ui("Horizontal spiegeln"),"mirror-horizontal",[this]{mirror(true);});
    const QStringList alignNames{"alignLeft","alignCentre","alignRight","alignTop","alignMiddle","alignBottom"};
    const QStringList alignTexts{ui("Links ausrichten"),ui("Vertikal mittig ausrichten"),ui("Rechts ausrichten"),ui("Oben ausrichten"),ui("Horizontal mittig ausrichten"),ui("Unten ausrichten")};
    const QStringList alignIcons{"align-left","align-hcenter","align-right","align-top","align-vcenter","align-bottom"};
    for(int i=0;i<6;i++)add(alignNames[i],alignTexts[i],alignIcons[i],[this,i]{align(i);});
    add("rotateLeft",ui("Gegen den Uhrzeigersinn drehen"),"rotate-left",[this]{rotateSelected(angleEdit->value());});
    add("rotateRight",ui("Im Uhrzeigersinn drehen"),"rotate-right",[this]{rotateSelected(-angleEdit->value());});
    add("proportional",ui("Proportionen beibehalten"),"proportional",[this]{area->proportional=action("proportional")->isChecked();})->setCheckable(true);
    add("contourSharp",ui("Ursprüngliche Kontur"),"contour-normal",[this]{setContour(Corners::Sharp);});
    add("contourSpline",ui("Kontur mit B-Splines glätten"),"contour-spline",[this]{setContour(Corners::Spline);});
    add("contourChamfer",ui("Ecken mit Fasen versehen"),"contour-chamfer",[this]{setContour(Corners::Chamfer);});
    add("contourRound",ui("Ecken abrunden"),"contour-round",[this]{setContour(Corners::Round);});
    // Library
    add("addToLibrary",ui("zur Bibliothek &hinzufügen…"),"",[this]{addToLibrary();});
    add("newPage",ui("Seite &anlegen…"),"",[this]{sideDock->show();side->showPage(0);side->newPage();},{QKeySequence(Qt::CTRL|Qt::ALT|Qt::Key_S)});
    add("deletePage",ui("Seite &löschen"),"",[this]{side->deletePage();},{QKeySequence(Qt::CTRL|Qt::Key_Delete)});
    add("renamePage",ui("Seite &umbenennen…"),"",[this]{side->renamePage();},{QKeySequence(Qt::CTRL|Qt::ALT|Qt::Key_N)});
    add("libraryFolders",ui("Weitere Bibliotheksordner…"),"",[this]{
        QStringList folders=side->extraLibraryFolders();
        const QString folder=QFileDialog::getExistingDirectory(this,ui("Ordner mit Bibliotheksseiten (*.LIB) wählen"),folders.value(0,startFolder()));
        if(folder.isEmpty())return;if(folders.contains(folder))folders.removeAll(folder);else folders<<folder;side->setExtraLibraryFolders(folders);
    });
    // Panel
    add("panelProperties",ui("&Eigenschaften…"),"panel",[this]{Panel p=doc.panel();if(editPanelProperties(this,p,false))change([&](Document &d){d.panel()=p;});},{QKeySequence(Qt::CTRL|Qt::Key_F)});
    add("addPanel",ui("&Hinzufügen…"),"",[this]{addPanel(false);},{QKeySequence(Qt::Key_F5)});
    add("duplicatePanel",ui("&Duplizieren"),"",[this]{addPanel(true);},{QKeySequence(Qt::Key_F6)});
    add("removePanel",ui("&Löschen"),"",[this]{removePanel();},{QKeySequence(Qt::Key_F7)});
    add("panelsFromFile",ui("Aus Da&tei hinzufügen…"),"",[this]{addPanelsFromFile();},{QKeySequence(Qt::Key_F8)});
    add("panelLeft",ui("Frontplatte nach &links"),"",[this]{movePanel(-1);});
    add("panelRight",ui("Frontplatte nach &rechts"),"",[this]{movePanel(1);});
    // Circuit boards behind the panel, in a project.
    add("boardsBehind",ui("Pla&tinen dahinter…"),"",[this]{editBoardsBehind();});
    add("showBoards",ui("Platinen an&zeigen"),"",[this]{refreshUnderlay();})->setCheckable(true);action("showBoards")->setChecked(true);
    add("holesFromBoards",ui("&Bohrungen aus der Platine…"),"",[this]{holesFromBoards();});
    add("compareBoards",ui("Mit Platine &vergleichen"),"",[this]{showBoardComparison();});
    // Options
    add("objectTree",ui("&Objektbaum"),"object-tree",[this]{treeDock->setVisible(action("objectTree")->isChecked());},{QKeySequence(Qt::CTRL|Qt::ALT|Qt::Key_O)})->setCheckable(true);
    connect(treeDock,&QDockWidget::visibilityChanged,this,[this](bool on){action("objectTree")->setChecked(on);if(on)updateTree();});
    add("grid",ui("&Raster einrichten…"),"grid",[this]{Panel p=doc.panel();if(editGrid(this,p))change([&](Document &d){d.panel()=p;});},{QKeySequence(Qt::CTRL|Qt::Key_R)});
    add("origin",ui("&Ursprung setzen"),"tool-origin",[this]{area->setTool(PanelView::Origin);});
    add("scaleWizard",ui("&Skalen-Assistent…"),"tool-scale",[this]{scaleWizard();});
    add("cutout",ui("&Frontplattenausschnitt…"),"tool-cutout",[this]{cutoutWizard();},{QKeySequence(Qt::CTRL|Qt::Key_T)});
    add("regularPolygon",ui("Gleichmäßiges &Vieleck…"),"tool-regular",[this]{regularPolygon();});
    add("dimensionStyle",ui("Vorgaben &Bemaßung…"),"",[this]{if(editDimensionStyle(this,dimensionStyle,dimensionAutoColor,dimensionAsk))saveSettings();});
    add("usedFonts",ui("Verwendete Schriftarten…"),"",[this]{showUsedFonts(this,doc);});
    add("importPresets",ui("Vorgaben aus FrontDesigner übernehmen…"),"",[this]{
        const QString title=ui("Vorgaben aus FrontDesigner übernehmen");
        const QStringList files=QFileDialog::getOpenFileNames(this,title,startFolder(),ui("Einstellungen von FrontDesigner (Stifte.INI FUELLUNG.INI Fonts.ini *.ini *.INI)"));
        if(files.isEmpty())return;
        QMessageBox box(QMessageBox::Question,title,ui("Sollen die Vorgaben aus FrontDesigner die eigenen Listen ersetzen oder ergänzen?"),QMessageBox::NoButton,this);
        auto *replace=box.addButton(ui("Ersetzen"),QMessageBox::DestructiveRole);auto *extend=box.addButton(ui("Ergänzen"),QMessageBox::AcceptRole);
        box.addButton(ui("Abbrechen"),QMessageBox::RejectRole);box.exec();
        if(box.clickedButton()!=replace&&box.clickedButton()!=extend)return;
        QStringList notes;const int taken=side->importFrontDesignerPresets(files,box.clickedButton()==replace,&notes);
        QMessageBox::information(this,title,ui("%1 Vorgaben übernommen.").arg(taken)+(notes.isEmpty()?QString():"\n\n"+notes.join("\n")));
    });
    add("installStrokeFont",ui("Strichschrift installieren…"),"",[this]{
        // Like the original, the chosen files are copied into the program's own folder of stroke fonts.
        const QStringList files=QFileDialog::getOpenFileNames(this,ui("Strichschrift installieren"),startFolder(),ui("Strichschriften (*.shx *.SHX *.shp *.SHP *.fhx *.FHX)"));
        if(files.isEmpty())return;const QString folder=ownStrokeFontFolder();QDir().mkpath(folder);QStringList failed;
        for(const auto &file:files){
            try{StrokeFont::load(file);}catch(const std::exception &e){failed<<QString("%1: %2").arg(QFileInfo(file).fileName(),QString::fromUtf8(e.what()));continue;}
            const QString target=QDir(folder).filePath(QFileInfo(file).fileName());if(QFileInfo(target)==QFileInfo(file))continue;
            QFile::remove(target);if(!QFile::copy(file,target))failed<<QFileInfo(file).fileName();
        }
        forgetStrokeFonts();side->refreshStrokeFonts();area->refresh();rememberFolder(files.first());
        if(!failed.isEmpty())QMessageBox::warning(this,ui("Strichschrift installieren"),ui("Nicht installiert:\n%1").arg(failed.join("\n")));
    });
    // Shape fonts usually number their letters in the DOS code page; FrontDesigner moves them to Windows-1252. Off, the
    // numbers count as Windows-1252, for the screen and the engraving alike.
    QAction *dosOrder=add("strokeFontsDosOrder",ui("Strichschriften in DOS-Zeichenordnung (wie FrontDesigner)"),"",[this]{
        setStrokeFontsInDosOrder(action("strokeFontsDosOrder")->isChecked());side->refreshStrokeFonts();area->refresh();});
    dosOrder->setCheckable(true);dosOrder->setChecked(strokeFontsInDosOrder());
    add("strokeFontOrders",ui("Zeichenordnung der Strichschriften…"),"",[this]{if(editStrokeFontOrders(this)){side->refreshStrokeFonts();area->refresh();}});
    // View
    auto toggle=[this](const QString &name,const QString &text,const QString &icon,bool on,const std::function<void(bool)> &set){
        QAction *a=add(name,text,icon,{});a->setCheckable(true);a->setChecked(on);connect(a,&QAction::toggled,this,[this,set](bool v){set(v);area->refresh();});return a;};
    toggle("showGrid",ui("Raster anzeigen"),"grid",true,[this](bool v){doc.panel().gridVisible=v;});
    toggle("mono",ui("S/W-Darstellung"),"view-mono",false,[this](bool v){area->options.outlineOnly=v;});
    toggle("showDimensions",ui("Bemaßungen zeigen oder ausblenden"),"view-dimensions",true,[this](bool v){area->options.dimensions=v;});
    toggle("showMachining",ui("Bohrungen und Fräsungen zeigen oder ausblenden"),"view-milled",true,[this](bool v){area->options.milled=area->options.drills=v;});
    toggle("showEngraved",ui("Gravuren zeigen oder ausblenden"),"view-engraved",true,[this](bool v){area->options.engraved=v;});
    toggle("showObjects",ui("Übrige Objekte und Symbole zeigen oder ausblenden"),"view-objects",true,[this](bool v){area->options.other=v;});
    toggle("showTexts",ui("Texte zeigen oder ausblenden"),"view-texts",true,[this](bool v){area->options.texts=v;});
    add("zoomIn",ui("Vergrößern"),"zoom-in",[this]{area->zoomBy(1.25);},{QKeySequence::ZoomIn});
    add("zoomOut",ui("Verkleinern"),"zoom-out",[this]{area->zoomBy(0.8);},{QKeySequence::ZoomOut});
    add("zoomAll",ui("Alle Objekte zeigen"),"zoom-all",[this]{area->fitElements(false);});
    add("zoomSelection",ui("Markierte Objekte zeigen"),"zoom-marked",[this]{area->fitElements(true);});
    add("zoomPanel",ui("Ganze Frontplatte zeigen"),"zoom-board",[this]{area->fitPanel();},{QKeySequence(Qt::CTRL|Qt::Key_0)});
    // Tools: the mode switches on the left, in the original's order.
    toolGroup=new QActionGroup(this);toolGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    struct Tool {const char *name;PanelView::Tool tool;QString text;const char *icon;};
    const QList<Tool> tools{{"tool-select",PanelView::Select,ui("Markieren, verschieben und bearbeiten"),"tool-select"},{"tool-rotate",PanelView::Rotate,ui("Drehen"),"tool-rotate"},
        {"tool-zoom",PanelView::Zoom,ui("Zoom (Lupe)"),"tool-zoom"},{"tool-line",PanelView::Line,ui("Linien zeichnen"),"tool-line"},{"tool-polygon",PanelView::Polygon,ui("Fläche zeichnen"),"tool-polygon"},
        {"tool-rectangle",PanelView::Rectangle,ui("Rechteck zeichnen"),"tool-rectangle"},{"tool-circle",PanelView::Circle,ui("Kreis zeichnen"),"tool-circle"},{"tool-arc",PanelView::Arc,ui("Bogen zeichnen"),"tool-arc"},
        {"tool-text",PanelView::Text,ui("Frontplatte beschriften"),"tool-text"},{"tool-drill",PanelView::Drill,ui("Bohrung setzen"),"tool-drill-fp"},
        {"tool-dimension",PanelView::Dimension,ui("Bemaßung zeichnen"),"tool-dimension"},{"tool-origin",PanelView::Origin,ui("Ursprung setzen"),"tool-origin"}};
    for(const auto &t:tools){
        const PanelView::Tool tool=t.tool;QAction *a=add(t.name,t.text,t.icon,[this,tool]{
            if(tool==PanelView::Dimension&&dimensionAsk&&area->tool()!=PanelView::Dimension)editDimensionStyle(this,dimensionStyle,dimensionAutoColor,dimensionAsk);
            area->setTool(tool);});
        a->setCheckable(true);toolGroup->addAction(a);
    }
    action("tool-select")->setChecked(true);
}

void PanelEditor::buildToolbars(){
    auto bar=[this](const QString &name,const QString &title,Qt::ToolBarArea where,const QStringList &items){
        auto *b=new QToolBar(title,this);b->setObjectName(name);b->setIconSize(QSize(16,16));addToolBar(where,b);
        for(const auto &item:items){if(item.isEmpty())b->addSeparator();else b->addAction(action(item));}
        return b;
    };
    bar("fileBar",ui("Datei"),Qt::TopToolBarArea,{"new","open","save","print"});
    bar("editBar",ui("Bearbeiten"),Qt::TopToolBarArea,{"copy","cut","paste","duplicate","","delete","","undo","redo"});
    bar("arrangeBar",ui("Anordnen"),Qt::TopToolBarArea,{"group","ungroup","front","back","mirrorVertical","mirrorHorizontal","distribute","alignGrid"});
    bar("viewBar",ui("Ansicht"),Qt::TopToolBarArea,{"showGrid","mono","showDimensions","showMachining","showEngraved","showObjects","showTexts"});
    auto *tools=new QToolBar(ui("Werkzeuge"),this);tools->setObjectName("toolsBar");tools->setIconSize(QSize(20,20));addToolBar(Qt::LeftToolBarArea,tools);
    for(const char *name:{"tool-select","tool-rotate","tool-zoom","tool-line","tool-polygon","tool-rectangle","tool-circle","tool-arc","regularPolygon","tool-text","tool-drill","scaleWizard","cutout","tool-dimension","importImage","tool-origin"})
        tools->addAction(action(name));
    bar("alignBar",ui("Ausrichten"),Qt::BottomToolBarArea,{"alignLeft","alignCentre","alignRight","alignTop","alignMiddle","alignBottom"});
    auto *rotate=bar("rotateBar",ui("Drehen"),Qt::BottomToolBarArea,{"rotateLeft","rotateRight"});
    angleEdit=field(0.01,360,2,ui("Winkelschritt")," °");angleEdit->setValue(45);rotate->addWidget(angleEdit);
    auto *size=bar("sizeBar",ui("Breite/Höhe"),Qt::BottomToolBarArea,{"proportional"});
    widthEdit=field(0,100000,3,ui("Breite"));heightEdit=field(0,100000,3,ui("Höhe"));size->addWidget(widthEdit);size->addWidget(heightEdit);
    auto *contour=bar("contourBar",ui("Kontur"),Qt::BottomToolBarArea,{"contourSharp","contourSpline","contourChamfer","contourRound"});
    contourEdit=field(0,1000,2,ui("Größe der Fasen und Rundungen"));contourEdit->setValue(2);contour->addWidget(contourEdit);
    auto *display=new QToolBar(ui("Anzeige"),this);display->setObjectName("infoBar");addToolBar(Qt::BottomToolBarArea,display);
    auto *box=new QWidget;auto *lines=new QVBoxLayout(box);lines->setContentsMargins(4,0,4,0);lines->setSpacing(0);info=new QLabel;position=new QLabel;
    for(auto *l:{info,position}){l->setMinimumWidth(240);QFont f=l->font();f.setPointSizeF(f.pointSizeF()*0.9);l->setFont(f);lines->addWidget(l);}
    box->setToolTip(ui("Informationsanzeige"));display->addWidget(box);
    auto *place=new QToolBar(ui("Position"),this);place->setObjectName("positionBar");addToolBar(Qt::BottomToolBarArea,place);
    xEdit=field(-100000,100000,3,ui("X (vom Ursprung)"));yEdit=field(-100000,100000,3,ui("Y (vom Ursprung)"));
    place->addWidget(new QLabel(" X "));place->addWidget(xEdit);place->addWidget(new QLabel(" Y "));place->addWidget(yEdit);
    connect(widthEdit,&QDoubleSpinBox::editingFinished,this,[this]{
        if(refreshing)return;const QRectF b=shapeOf(area->selectedElements());const double w=widthEdit->value();
        const double h=area->proportional&&b.width()>1e-9?b.height()*w/b.width():heightEdit->value();resizeSelection(w,h);});
    connect(heightEdit,&QDoubleSpinBox::editingFinished,this,[this]{
        if(refreshing)return;const QRectF b=shapeOf(area->selectedElements());const double h=heightEdit->value();
        const double w=area->proportional&&b.height()>1e-9?b.width()*h/b.height():widthEdit->value();resizeSelection(w,h);});
    connect(xEdit,&QDoubleSpinBox::editingFinished,this,[this]{if(!refreshing)moveSelection(xEdit->value(),yEdit->value());});
    connect(yEdit,&QDoubleSpinBox::editingFinished,this,[this]{if(!refreshing)moveSelection(xEdit->value(),yEdit->value());});
}

void PanelEditor::buildMenus(){
    auto menu=[this](const QString &title,const QStringList &items){
        QMenu *m=menuBar()->addMenu(title);for(const auto &item:items){if(item.isEmpty())m->addSeparator();else m->addAction(action(item));}return m;};
    QMenu *file=menu(ui("&Datei"),{"new","","open","save","saveAs","","autosave","","importImage"});
    QMenu *exports=file->addMenu(ui("E&xportieren"));exports->addAction(action("exportImage"));exports->addAction(action("exportHpgl"));
    file->addSeparator();file->addAction(action("print"));file->addSeparator();file->addAction(action("quit"));
    QMenu *edit=menu(ui("&Bearbeiten"),{"undo","redo","","cut","copy","paste","duplicate","","selectAll","","delete",""});
    QMenu *adopt=edit->addMenu(ui("Eigenschaften &übernehmen"));adopt->addAction(action("adoptPen"));adopt->addAction(action("adoptFill"));adopt->addAction(action("adoptFont"));
    edit->addSeparator();edit->addAction(action("properties"));edit->addAction(action("rename"));
    menu(ui("&Ansicht"),{"zoomIn","zoomOut","zoomAll","zoomSelection","zoomPanel","","showGrid","mono","showDimensions","showMachining","showEngraved","showObjects","showTexts"});
    QMenu *arrange=menu(ui("&Anordnen"),{"front","back","","group","ungroup","","combine","uncombine","","distribute","alignGrid","","mirrorVertical","mirrorHorizontal"});
    QMenu *aligning=arrange->addMenu(ui("A&usrichten"));for(const char *name:{"alignLeft","alignCentre","alignRight","alignTop","alignMiddle","alignBottom"})aligning->addAction(action(name));
    menu(ui("B&ibliothek"),{"addToLibrary","","newPage","deletePage","renamePage","","libraryFolders"});
    menu(ui("&Frontplatte"),{"panelProperties","","addPanel","duplicatePanel","removePanel","","panelsFromFile","","panelLeft","panelRight","","boardsBehind","showBoards","holesFromBoards","compareBoards"});
    QMenu *options=menu(ui("&Optionen"),{"objectTree","grid","origin","scaleWizard","cutout","regularPolygon","dimensionStyle",""});
    QMenu *shown=options->addMenu(ui("Werkzeuge anzeigen"));
    for(auto *b:findChildren<QToolBar*>())shown->addAction(b->toggleViewAction());
    shown->addAction(sideDock->toggleViewAction());shown->addSeparator();
    shown->addAction(ui("Alle anzeigen"),this,[this]{for(auto *b:findChildren<QToolBar*>())b->show();sideDock->show();});
    options->addSeparator();options->addAction(action("installStrokeFont"));options->addAction(action("strokeFontsDosOrder"));options->addAction(action("strokeFontOrders"));options->addAction(action("usedFonts"));
    options->addAction(action("importPresets"));
    // The interface language as in FrontDesigner's language dialog (German, English, French); it takes effect at the
    // next start, for every window of the program.
    options->addSeparator();auto *languages=options->addMenu(ui("Sprache"));auto *languageGroup=new QActionGroup(languages);
    for(auto [label,code]:std::initializer_list<std::pair<const char*,const char*>>{{"Deutsch","de"},{"English","en"},{"Français","fr"}}){
        auto *a=languages->addAction(label);a->setCheckable(true);a->setChecked(uiLanguage()==code);a->setObjectName(QString("language-")+code);languageGroup->addAction(a);const QString chosen=code;
        connect(a,&QAction::triggered,this,[this,chosen]{QSettings().setValue("ui/language",chosen);if(chosen!=uiLanguage())QMessageBox::information(this,ui("Sprache"),ui("Die Sprache wird beim nächsten Start übernommen."));});
    }
    // Hilfe as FrontDesigner's "?": the module's help pages, with F1 on every system (macOS gives Hilfethemen ⌘?).
    auto keys=QKeySequence::keyBindings(QKeySequence::HelpContents);if(!keys.contains(QKeySequence(Qt::Key_F1)))keys<<QKeySequence(Qt::Key_F1);
    add("helpTopics",ui("&Hilfethemen…"),"",[this]{showHelp();},keys);menu(ui("&Hilfe"),{"helpTopics"});
}
void PanelEditor::librariesChanged(){forgetStrokeFonts();side->refreshStrokeFonts();side->refreshLibrary();area->refresh();}
void PanelEditor::showHelp(){
    auto *window=findChild<QDialog*>("helpWindow");
    if(!window){window=new QDialog(this);window->setObjectName("helpWindow");window->setWindowTitle(ui("Frontplatte – Hilfe"));window->resize(760,640);
        auto *layout=new QVBoxLayout(window);layout->setContentsMargins(0,0,0,0);auto *page=new QTextBrowser(window);page->setObjectName("helpPage");page->setOpenExternalLinks(true);layout->addWidget(page);
        page->setSource(QUrl(QString("qrc:/help/frontpanel/%1.html").arg(uiLanguage())));}
    window->show();window->raise();window->activateWindow();
}

// ------------------------------------------------------------------ document and history
void PanelEditor::setDocument(const Document &document,const QString &file){
    doc=document;if(doc.panels.isEmpty())doc.panels<<newPanel(ui("Neue Frontplatte"),100,100);doc.activePanel=std::clamp(doc.activePanel,0,int(doc.panels.size())-1);
    history.clear();historyOpen=false;path=file;dirty=false;
    area->setDocument(&doc);action("showGrid")->setChecked(doc.panel().gridVisible);
    refreshAll();updateTitle();reloadBoards();
}
void PanelEditor::begin(){if(!historyOpen){history.begin(doc);historyOpen=true;}}
void PanelEditor::commit(){if(!historyOpen)return;historyOpen=false;if(history.commit(doc))markChanged();refreshAll();}
void PanelEditor::change(const std::function<void(Document&)> &edit){begin();edit(doc);commit();}
void PanelEditor::markChanged(){dirty=true;updateTitle();}
void PanelEditor::refreshAll(){area->refresh();updateTabs();updateTree();updateSelectionTools();updateActions();side->refreshViews();refreshUnderlay();}
void PanelEditor::undo(){if(history.undo(doc)){markChanged();refreshAll();}}
void PanelEditor::redo(){if(history.redo(doc)){markChanged();refreshAll();}}
void PanelEditor::updateTitle(){
    const QString name=path.isEmpty()?ui("Unbenannt"):QFileInfo(path).fileName();
    setWindowTitle(QString("%1[*] – %2").arg(name,ui("Frontplatte")));setWindowModified(dirty);if(titleChanged)titleChanged();
}
void PanelEditor::updateTabs(){
    refreshing=true;
    while(tabs->count()>doc.panels.size())tabs->removeTab(tabs->count()-1);
    for(int i=0;i<doc.panels.size();i++){if(i<tabs->count())tabs->setTabText(i,doc.panels[i].name);else tabs->addTab(doc.panels[i].name);}
    tabs->setCurrentIndex(doc.activePanel);refreshing=false;
}
void PanelEditor::updateTree(){
    if(!treeDock->isVisible())return;
    treeSync=true;tree->clear();
    std::function<void(QTreeWidgetItem*,const Element&)> addItem=[&](QTreeWidgetItem *parent,const Element &e){
        const QString title=e.name.isEmpty()?typeTitle(e.type):QString("%1 (%2)").arg(e.name,typeTitle(e.type));
        auto *item=parent?new QTreeWidgetItem(parent,{title}):new QTreeWidgetItem(tree,{title});item->setData(0,Qt::UserRole,e.id);
        for(const auto &c:e.children)addItem(item,c);
    };
    // The front element first, as it lies on top.
    const auto &list=doc.panel().elements;for(int i=list.size()-1;i>=0;i--)addItem(nullptr,list[i]);
    treeSync=false;updateTreeSelection();
}
void PanelEditor::updateTreeSelection(){
    if(treeSync||!treeDock->isVisible())return;treeSync=true;const QStringList ids=area->selection();
    std::function<void(QTreeWidgetItem*)> visit=[&](QTreeWidgetItem *item){const bool on=ids.contains(item->data(0,Qt::UserRole).toString());item->setSelected(on);
        if(on)for(QTreeWidgetItem *p=item->parent();p;p=p->parent())p->setExpanded(true);for(int i=0;i<item->childCount();i++)visit(item->child(i));};
    for(int i=0;i<tree->topLevelItemCount();i++)visit(tree->topLevelItem(i));
    treeSync=false;
}
QStringList PanelEditor::selectedTopLevel() const{QStringList out;const QStringList ids=area->selection();for(const auto &e:doc.panel().elements)if(ids.contains(e.id))out<<e.id;return out;}
QList<Element> PanelEditor::selectedCopies(){QList<Element> out;for(auto *e:rootsOf(doc.panel().elements,area->selection()))out<<*e;return out;}

void PanelEditor::updateSelectionTools(){
    refreshing=true;const auto selection=area->selectedElements();const Panel &p=doc.panel();
    if(selection.isEmpty()){
        // Without a selection, width and height are those of the panel.
        widthEdit->setValue(p.width);heightEdit->setValue(p.height);xEdit->setEnabled(false);yEdit->setEnabled(false);
        info->setText(ui("„%1“: %2 × %3 mm").arg(p.name,number(p.width,1),number(p.height,1)));
    }else{
        const QRectF b=shapeOf(selection);widthEdit->setValue(b.width());heightEdit->setValue(b.height());xEdit->setEnabled(true);yEdit->setEnabled(true);
        xEdit->setValue(b.left()-p.origin.x());yEdit->setValue(b.top()-p.origin.y());
        const Element &e=*selection.first();
        info->setText(selection.size()>1?ui("%1 Objekte markiert").arg(selection.size()):e.name.isEmpty()?typeTitle(e.type):QString("%1: %2").arg(typeTitle(e.type),e.name));
    }
    refreshing=false;
}
void PanelEditor::updateActions(){
    const auto selection=area->selectedElements();const QStringList top=selectedTopLevel();const bool any=!selection.isEmpty();
    for(const char *name:{"copy","duplicate","mirrorVertical","mirrorHorizontal","rotateLeft","rotateRight","alignGrid","addToLibrary","adoptPen","adoptFill","adoptFont","zoomSelection",
                          "alignLeft","alignCentre","alignRight","alignTop","alignMiddle","alignBottom"})action(name)->setEnabled(any);
    for(const char *name:{"cut","delete","front","back"})action(name)->setEnabled(!top.isEmpty());
    int containers=0,closed=0,combinations=0;
    for(const auto &e:doc.panel().elements){if(!top.contains(e.id))continue;if(e.isContainer()&&!e.combined())containers++;if(e.combined())combinations++;if(e.closed()||e.type==ElementType::Text)closed++;}
    action("group")->setEnabled(top.size()>=2);action("ungroup")->setEnabled(containers>0);action("combine")->setEnabled(closed>=2);action("uncombine")->setEnabled(combinations>0);
    action("distribute")->setEnabled(top.size()>=2);action("properties")->setEnabled(selection.size()==1);action("rename")->setEnabled(selection.size()==1);
    bool contours=false;std::function<void(const Element&)> scan=[&](const Element &e){if(e.hasContour())contours=true;if(!e.combined())for(const auto &c:e.children)scan(c);};for(auto *e:selection)scan(*e);
    for(const char *name:{"contourSharp","contourSpline","contourChamfer","contourRound"})action(name)->setEnabled(contours);
    action("undo")->setEnabled(history.canUndo());action("redo")->setEnabled(history.canRedo());
    const QMimeData *mime=QApplication::clipboard()->mimeData();action("paste")->setEnabled(mime&&mime->hasFormat(clipboardMime));
    action("removePanel")->setEnabled(doc.panels.size()>1);action("panelLeft")->setEnabled(doc.activePanel>0);action("panelRight")->setEnabled(doc.activePanel<doc.panels.size()-1);
    // Boards behind the panel need a project; holes and the comparison a board behind it.
    const bool behind=!doc.panel().boards.isEmpty();
    action("boardsBehind")->setEnabled(bool(boardSources)||behind);action("showBoards")->setEnabled(behind);
    action("holesFromBoards")->setEnabled(behind&&bool(boardSources));action("compareBoards")->setEnabled(behind);
}

// The current pen, fill and font take those of a single selected element, so that the next one is drawn alike.
void PanelEditor::adoptStyle(){
    const auto selection=area->selectedElements();if(selection.size()!=1)return;const Element &e=*selection.first();
    if(e.type==ElementType::Text){currentFont=e.font;currentStrokeFont=e.strokeFont;currentBold=e.bold;currentItalic=e.italic;const double h=textHeight(e);if(h>0)currentTextHeight=h;}
    else if(e.type==ElementType::Drill){drillDiameter=e.diameter;}
    else if(!e.isContainer()||e.combined()){
        if(e.type==ElementType::Image||e.type==ElementType::Picture)return;
        currentPen=e.pen;currentMachining=e.machining;if(e.closed())currentFill=e.fill;
    }
    side->syncFromEditor();
}
Element PanelEditor::styled(ElementType type) const{
    Element e=newElement(type);
    switch(type){
    case ElementType::Line:e.pen=currentPen;e.machining=currentMachining;break;
    case ElementType::Polygon:case ElementType::Rectangle:case ElementType::Ellipse:e.pen=currentPen;e.machining=currentMachining;e.fill=currentFill;break;
    case ElementType::Arc:e.pen=currentPen;e.machining=currentMachining;break;
    case ElementType::Text:
        e.font=currentFont;e.strokeFont=currentStrokeFont;e.bold=currentBold;e.italic=currentItalic;e.machining=currentMachining;
        // Letters are filled; without a fill they take the pen colour.
        e.fill=currentFill.style==FillStyle::Solid?currentFill:Fill{FillStyle::Solid,currentPen.color,currentPen.color,Gradient::None};
        e.pen=currentMachining==Machining::None?Pen{currentPen.color,0,PenStyle::None}:currentPen;
        if(!currentStrokeFont.isEmpty()){e.pen=currentPen;e.fill.style=FillStyle::None;}
        break;
    default:break;
    }
    return e;
}

// ------------------------------------------------------------------ files
QString PanelEditor::startFolder() const{
    const QString saved=QSettings().value("frontpanel/folder").toString();
    return !path.isEmpty()?QFileInfo(path).absolutePath():!saved.isEmpty()?saved:QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}
void PanelEditor::rememberFolder(const QString &file){QSettings().setValue("frontpanel/folder",QFileInfo(file).absolutePath());}
void PanelEditor::newDocument(){
    if(!maybeSave())return;Document d;
    if(!editPanelProperties(this,d.panels[0],true))return;
    setDocument(d);
}
bool PanelEditor::open(const QString &file){
    QStringList notes;QString format;Document d;
    try{d=readAny(file,&notes,&format);}
    catch(const std::exception &e){QMessageBox::warning(this,ui("Öffnen"),ui("%1 kann nicht geöffnet werden:\n%2").arg(QFileInfo(file).fileName(),QString::fromUtf8(e.what())));return false;}
    QString target=file;const QFileInfo info(file);const bool backup=info.suffix().compare("bak",Qt::CaseInsensitive)==0;
    // A backup continues as the project it belongs to.
    if(backup)target=info.absoluteDir().filePath(info.completeBaseName()+(format=="native"?".olfp":format=="lib"?".LIB":".FPL"));
    setDocument(d,target);rememberFolder(file);
    if(backup){dirty=true;updateTitle();}
    if(!notes.isEmpty())QMessageBox::information(this,ui("Öffnen"),ui("Einiges wurde nur angenähert übernommen:\n\n%1").arg(notes.mid(0,20).join("\n")));
    reportMissingFonts();
    return true;
}
void PanelEditor::reportMissingFonts(){
    if(missingFonts(doc).isEmpty())return;
    QTimer::singleShot(0,this,[this]{usedFontsWindow(this,doc)->show();});
}
bool PanelEditor::save(){
    if(saveHandler)return saveHandler(false);
    if(path.isEmpty())return saveAs();
    // Pictures that nothing uses any more are not saved.
    QStringList used;for(const auto &p:doc.panels)usedResources(p.elements,used);for(const auto &key:doc.resources.keys())if(!used.contains(key))doc.resources.remove(key);
    try{
        const QString suffix=QFileInfo(path).suffix().toLower();
        if(suffix=="fpl"||suffix=="lib"){
            const QByteArray bytes=suffix=="lib"?frontdesigner::writeFrontDesignerLibrary(doc,doc.activePanel):frontdesigner::writeFrontDesigner(doc);
            frontdesigner::readFrontDesigner(bytes,suffix=="lib");   // never write what could not be read back
            QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())throw FormatError(f.errorString());
        }else doc.save(path);
    }catch(const std::exception &e){
        QMessageBox::warning(this,ui("Speichern"),ui("%1 kann nicht gespeichert werden:\n%2").arg(QFileInfo(path).fileName(),QString::fromUtf8(e.what())));return false;
    }
    dirty=false;updateTitle();return true;
}
bool PanelEditor::saveAs(const QString &given){
    if(saveHandler&&given.isEmpty())return saveHandler(true);
    QString file=given;
    if(file.isEmpty()){
        QString filter;const QString base=path.isEmpty()?(doc.title.isEmpty()?ui("Frontplatte"):doc.title):QFileInfo(path).completeBaseName();
        file=QFileDialog::getSaveFileName(this,ui("Frontplatte speichern unter"),QDir(startFolder()).filePath(base+".olfp"),
            ui("OpenLoch-Frontplatte (*.olfp);;FrontDesigner-Projekt (*.fpl);;FrontDesigner-Bibliothek (*.lib)"),&filter);
        if(file.isEmpty())return false;
        if(QFileInfo(file).suffix().isEmpty())file+=filter.contains("*.fpl")?".FPL":filter.contains("*.lib")?".LIB":".olfp";
    }
    if(QFileInfo(file).suffix().compare("lib",Qt::CaseInsensitive)==0&&doc.panels.size()>1)
        QMessageBox::information(this,ui("Speichern"),ui("Eine Bibliotheksseite nimmt nur eine Frontplatte auf; gespeichert wird „%1“.").arg(doc.panel().name));
    const QString old=path;path=file;
    if(!save()){path=old;updateTitle();return false;}
    rememberFolder(file);return true;
}
void PanelEditor::markSaved(){dirty=false;updateTitle();}
bool PanelEditor::maybeSave(){
    if(!dirty)return true;
    QMessageBox box(QMessageBox::Warning,ui("Frontplatte"),ui("Die Frontplatte wurde geändert. Sollen die Änderungen gespeichert werden?"),QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,this);
    box.button(QMessageBox::Save)->setText(ui("Speichern"));box.button(QMessageBox::Discard)->setText(ui("Verwerfen"));box.button(QMessageBox::Cancel)->setText(ui("Abbrechen"));
    const int answer=box.exec();if(answer==QMessageBox::Save)return save();return answer==QMessageBox::Discard;
}
void PanelEditor::writeBackup(){
    if(!dirty)return;
    try{
        QString file;QByteArray bytes;
        if(path.isEmpty()){const QString folder=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);QDir().mkpath(folder);file=QDir(folder).filePath("Frontplatte.BAK");bytes=doc.encode();}
        else{const QFileInfo info(path);const QString suffix=info.suffix().toLower();file=info.absoluteDir().filePath(info.completeBaseName()+".BAK");
            bytes=suffix=="fpl"?frontdesigner::writeFrontDesigner(doc):suffix=="lib"?frontdesigner::writeFrontDesignerLibrary(doc,doc.activePanel):doc.encode();}
        QSaveFile f(file);if(f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size())f.commit();
    }catch(const std::exception&){}
}
void PanelEditor::closeEvent(QCloseEvent *event){if(!maybeSave()){event->ignore();return;}saveSettings();event->accept();}
namespace {
QString droppedFile(const QMimeData *mime){
    if(!mime||!mime->hasUrls())return {};
    for(const QUrl &url:mime->urls())if(url.isLocalFile()){
        const QString file=url.toLocalFile(),suffix=QFileInfo(file).suffix().toLower();
        if(QStringList{"olfp","fpl","lib","bak","png","jpg","jpeg","bmp","emf"}.contains(suffix))return file;
    }
    return {};
}
}
void PanelEditor::dragEnterEvent(QDragEnterEvent *event){if(!droppedFile(event->mimeData()).isEmpty())event->acceptProposedAction();}
void PanelEditor::dropEvent(QDropEvent *event){
    const QString file=droppedFile(event->mimeData());if(file.isEmpty())return;event->acceptProposedAction();
    const QString suffix=QFileInfo(file).suffix().toLower();
    if(QStringList{"png","jpg","jpeg","bmp","emf"}.contains(suffix))importImageFile(file);else if(maybeSave())open(file);
}

void PanelEditor::loadSettings(){
    QSettings s;restoreGeometry(s.value("frontpanel/geometry").toByteArray());restoreState(s.value("frontpanel/state").toByteArray());
    autosaveMinutes=std::clamp(s.value("frontpanel/autosave",0).toInt(),0,240);if(autosaveMinutes>0)autosave->start(autosaveMinutes*60000);
    const QJsonObject style=QJsonDocument::fromJson(s.value("frontpanel/style").toByteArray()).object();
    if(!style.isEmpty()){
        currentPen.color=QColor::fromString(style["penColor"].toString("#000000"));currentPen.width=std::clamp(style["penWidth"].toDouble(0.5),0.0,100.0);
        currentPen.style=PenStyle(std::clamp(style["penStyle"].toInt(),0,4));currentMachining=Machining(std::clamp(style["tool"].toInt(),0,2));
        setFillChoice(currentFill,std::clamp(style["fill"].toInt(1),0,11));currentFill.color=QColor::fromString(style["fillColor"].toString("#ffffff"));currentFill.color2=QColor::fromString(style["fillColor2"].toString("#ffffff"));
        currentFont=style["font"].toString("Arial");currentStrokeFont=style["strokeFont"].toString();currentTextHeight=std::clamp(style["textHeight"].toDouble(3.5),0.2,500.0);
        currentBold=style["bold"].toBool();currentItalic=style["italic"].toBool();drillDiameter=std::clamp(style["drill"].toDouble(3),0.01,100.0);
    }
    dimensionStyle=DimensionStyle::fromJson(QJsonDocument::fromJson(s.value("frontpanel/dimension").toByteArray()).object());
    dimensionAutoColor=s.value("frontpanel/dimensionAutoColor",true).toBool();dimensionAsk=s.value("frontpanel/dimensionAsk",false).toBool();
    angleEdit->setValue(s.value("frontpanel/angle",45.0).toDouble());contourEdit->setValue(s.value("frontpanel/contourSize",2.0).toDouble());
    action("proportional")->setChecked(s.value("frontpanel/proportional",false).toBool());area->proportional=action("proportional")->isChecked();
    polygonCorners=std::clamp(s.value("frontpanel/polygonCorners",6).toInt(),3,360);polygonRadius=s.value("frontpanel/polygonRadius",10.0).toDouble();
    polygonStart=s.value("frontpanel/polygonStart",90.0).toDouble();polygonInner=s.value("frontpanel/polygonInner",false).toBool();
    side->syncFromEditor();
}
void PanelEditor::saveSettings() const{
    QSettings s;s.setValue("frontpanel/geometry",saveGeometry());s.setValue("frontpanel/state",saveState());s.setValue("frontpanel/autosave",autosaveMinutes);
    const QJsonObject style{{"penColor",currentPen.color.name()},{"penWidth",currentPen.width},{"penStyle",int(currentPen.style)},{"tool",int(currentMachining)},
        {"fill",fillChoice(currentFill)},{"fillColor",currentFill.color.name()},{"fillColor2",currentFill.color2.name()},{"font",currentFont},{"strokeFont",currentStrokeFont},
        {"textHeight",currentTextHeight},{"bold",currentBold},{"italic",currentItalic},{"drill",drillDiameter}};
    s.setValue("frontpanel/style",QJsonDocument(style).toJson(QJsonDocument::Compact));
    s.setValue("frontpanel/dimension",QJsonDocument(dimensionStyle.toJson()).toJson(QJsonDocument::Compact));
    s.setValue("frontpanel/dimensionAutoColor",dimensionAutoColor);s.setValue("frontpanel/dimensionAsk",dimensionAsk);
    s.setValue("frontpanel/angle",angleEdit->value());s.setValue("frontpanel/contourSize",contourEdit->value());s.setValue("frontpanel/proportional",area->proportional);
    s.setValue("frontpanel/polygonCorners",polygonCorners);s.setValue("frontpanel/polygonRadius",polygonRadius);s.setValue("frontpanel/polygonStart",polygonStart);s.setValue("frontpanel/polygonInner",polygonInner);
}

// ------------------------------------------------------------------ editing
void PanelEditor::copy(){putOnClipboard(doc,selectedCopies());updateActions();}
void PanelEditor::cut(){copy();removeSelected();}
void PanelEditor::paste(){
    const QMimeData *mime=QApplication::clipboard()->mimeData();if(!mime||!mime->hasFormat(clipboardMime))return;
    try{
        Document clip=Document::decode(mime->data(clipboardMime));if(clip.panels.isEmpty())return;
        QList<Element> list=clip.panels[0].elements;QHash<QString,QString> renamed;for(auto &e:list)renewIds(e,&renamed);
        for(auto it=clip.resources.constBegin();it!=clip.resources.constEnd();++it)doc.resources.insert(it.key(),it.value());
        place(list);
    }catch(const std::exception&){}
}
void PanelEditor::duplicate(){QList<Element> list=selectedCopies();QHash<QString,QString> renamed;for(auto &e:list)renewIds(e,&renamed);place(list);}
void PanelEditor::place(const QList<Element> &elements){if(elements.isEmpty())return;area->beginPlacement(elements);area->setFocus();}
void PanelEditor::removeSelected(){
    // Parts of a group cannot be deleted on their own.
    const QStringList ids=selectedTopLevel();if(ids.isEmpty())return;
    area->clearSelection();change([&](Document &d){auto &list=d.panel().elements;list.erase(std::remove_if(list.begin(),list.end(),[&](const Element &e){return ids.contains(e.id);}),list.end());});
}
void PanelEditor::group(){
    const QStringList ids=selectedTopLevel();if(ids.size()<2)return;
    Element g=newElement(ElementType::Group);g.name=ui("Gruppe");g.pen.style=PenStyle::None;
    change([&](Document &d){
        auto &list=d.panel().elements;int top=-1;
        for(int i=0;i<list.size();i++)if(ids.contains(list[i].id)){g.children<<list[i];top=i;}
        list.erase(std::remove_if(list.begin(),list.end(),[&](const Element &e){return ids.contains(e.id);}),list.end());
        list.insert(std::clamp(top-int(ids.size())+1,0,int(list.size())),g);
    });
    area->setSelection({g.id});
}
void PanelEditor::ungroup(){
    const QStringList ids=selectedTopLevel();QStringList parts;
    change([&](Document &d){
        auto &list=d.panel().elements;
        for(int i=list.size()-1;i>=0;i--){
            if(!ids.contains(list[i].id)||!list[i].isContainer()||list[i].combined())continue;
            const QList<Element> children=list[i].children;list.removeAt(i);for(int k=0;k<children.size();k++){list.insert(i+k,children[k]);parts<<children[k].id;}
        }
    });
    if(!parts.isEmpty())area->setSelection(parts);
}
void PanelEditor::combine(){
    const QStringList ids=selectedTopLevel();QStringList members;
    for(const auto &e:doc.panel().elements)if(ids.contains(e.id)&&(e.closed()||e.type==ElementType::Text))members<<e.id;
    if(members.size()<2)return;
    Element c=newElement(ElementType::Group);c.name=ui("Kombination");c.parameters["combine"]=true;
    change([&](Document &d){
        auto &list=d.panel().elements;int top=-1;
        for(int i=0;i<list.size();i++)if(members.contains(list[i].id)){
            // The combination takes pen, fill and tool of its lowest part.
            if(c.children.isEmpty()){const Element &first=list[i];c.pen=first.pen;c.fill=first.fill;c.machining=first.machining;
                if(first.type==ElementType::Text&&c.pen.style==PenStyle::None&&c.fill.style==FillStyle::None)c.fill.style=FillStyle::Solid;}
            // The parts take pen and tool of the combination too: they are machined each on its own.
            Element part=list[i];part.pen=c.pen;if(part.type!=ElementType::Image&&part.type!=ElementType::Picture)part.machining=c.machining;
            c.children<<part;top=i;
        }
        list.erase(std::remove_if(list.begin(),list.end(),[&](const Element &e){return members.contains(e.id);}),list.end());
        list.insert(std::clamp(top-int(members.size())+1,0,int(list.size())),c);
    });
    area->setSelection({c.id});
}
void PanelEditor::uncombine(){
    const QStringList ids=selectedTopLevel();QStringList parts;
    change([&](Document &d){
        auto &list=d.panel().elements;
        for(int i=list.size()-1;i>=0;i--){
            if(!ids.contains(list[i].id)||!list[i].combined())continue;
            const QList<Element> children=list[i].children;list.removeAt(i);for(int k=0;k<children.size();k++){list.insert(i+k,children[k]);parts<<children[k].id;}
        }
    });
    if(!parts.isEmpty())area->setSelection(parts);
}
void PanelEditor::toFront(){
    const QStringList ids=selectedTopLevel();if(ids.isEmpty())return;
    change([&](Document &d){QList<Element> moving,rest;for(const auto &e:d.panel().elements)(ids.contains(e.id)?moving:rest)<<e;d.panel().elements=rest+moving;});
}
void PanelEditor::toBack(){
    const QStringList ids=selectedTopLevel();if(ids.isEmpty())return;
    change([&](Document &d){QList<Element> moving,rest;for(const auto &e:d.panel().elements)(ids.contains(e.id)?moving:rest)<<e;d.panel().elements=moving+rest;});
}
namespace {
void transformAll(const QList<Element*> &list,const QTransform &map){
    for(auto *e:list){transformElement(*e,map);if(e->type==ElementType::Dimension&&e->parameters["generator"]=="dimension")*e=regenerate(*e);}
}
}
void PanelEditor::mirror(bool horizontal){
    const QStringList ids=area->selection();const QRectF b=shapeOf(area->selectedElements());if(ids.isEmpty()||b.isNull())return;const QPointF c=b.center();
    const QTransform map=QTransform::fromTranslate(-c.x(),-c.y())*QTransform::fromScale(horizontal?-1:1,horizontal?1:-1)*QTransform::fromTranslate(c.x(),c.y());
    change([&](Document &d){transformAll(rootsOf(d.panel().elements,ids),map);});
}
void PanelEditor::rotateSelected(double degrees){
    const QStringList ids=area->selection();const QRectF b=shapeOf(area->selectedElements());if(ids.isEmpty()||b.isNull())return;
    change([&](Document &d){transformAll(rootsOf(d.panel().elements,ids),rotationAbout(b.center(),degrees));});
}
void PanelEditor::resizeSelection(double width,double height){
    const QStringList ids=area->selection();
    if(ids.isEmpty()){
        width=std::clamp(width,10.0,600.0);height=std::clamp(height,10.0,600.0);   // the original's limits
        if(std::abs(width-doc.panel().width)<1e-9&&std::abs(height-doc.panel().height)<1e-9){updateSelectionTools();return;}
        change([&](Document &d){d.panel().width=width;d.panel().height=height;});return;
    }
    const QRectF b=shapeOf(area->selectedElements());if(b.isNull())return;
    const double sx=b.width()>1e-9?width/b.width():1,sy=b.height()>1e-9?height/b.height():1;if(std::abs(sx-1)<1e-12&&std::abs(sy-1)<1e-12)return;
    if(sx<=0||sy<=0)return;
    const QTransform map=QTransform::fromTranslate(-b.left(),-b.top())*QTransform::fromScale(sx,sy)*QTransform::fromTranslate(b.left(),b.top());
    change([&](Document &d){transformAll(rootsOf(d.panel().elements,ids),map);});
}
void PanelEditor::moveSelection(double x,double y){
    const QStringList ids=area->selection();const QRectF b=shapeOf(area->selectedElements());if(ids.isEmpty()||b.isNull())return;
    const QPointF o=doc.panel().origin;const double dx=x+o.x()-b.left(),dy=y+o.y()-b.top();if(std::abs(dx)<1e-9&&std::abs(dy)<1e-9)return;
    change([&](Document &d){transformAll(rootsOf(d.panel().elements,ids),QTransform::fromTranslate(dx,dy));});
}
void PanelEditor::align(int edge){
    const QStringList ids=area->selection();const auto selection=area->selectedElements();if(selection.isEmpty())return;
    // The element marked last is the reference; a single element is aligned on the panel.
    const QString last=area->lastSelected();QRectF ref(0,0,doc.panel().width,doc.panel().height);
    if(selection.size()>1)if(Element *r=findIn(doc.panel().elements,last))ref=shapeOf(*r);
    change([&](Document &d){
        for(auto *e:rootsOf(d.panel().elements,ids)){
            if(selection.size()>1&&e->id==last)continue;const QRectF b=shapeOf(*e);double dx=0,dy=0;
            switch(edge){case 0:dx=ref.left()-b.left();break;case 1:dx=ref.center().x()-b.center().x();break;case 2:dx=ref.right()-b.right();break;
                case 3:dy=ref.top()-b.top();break;case 4:dy=ref.center().y()-b.center().y();break;default:dy=ref.bottom()-b.bottom();break;}
            transformAll({e},QTransform::fromTranslate(dx,dy));
        }
    });
}
void PanelEditor::setContour(Corners corners){
    const double size=contourEdit->value();
    applyToSelection([&](Element &e){if(e.hasContour())e.contour=Contour{corners,size};},false);
}
void PanelEditor::applyToSelection(const std::function<void(Element&)> &apply,bool includeTexts){
    const QStringList ids=area->selection();if(ids.isEmpty())return;
    // Texts take pen and fill only when nothing but texts is selected, so that a scale's lines change without its labels.
    bool onlyTexts=true;std::function<void(const Element&)> scan=[&](const Element &e){if(e.isContainer()&&!e.combined()){for(const auto &c:e.children)scan(c);}else if(e.type!=ElementType::Text)onlyTexts=false;};
    for(auto *e:area->selectedElements())scan(*e);
    const bool texts=includeTexts||onlyTexts;
    change([&](Document &d){
        std::function<void(Element&)> visit=[&](Element &e){
            if(e.isContainer()&&!e.combined()){for(auto &c:e.children)visit(c);return;}
            if(e.type==ElementType::Text&&!texts)return;apply(e);
            // A combination passes it on to its parts, like the original: they are machined each with its own tool.
            if(e.combined()){std::function<void(Element&)> parts=[&](Element &c){apply(c);for(auto &g:c.children)parts(g);};for(auto &c:e.children)parts(c);}
        };
        for(auto *e:rootsOf(d.panel().elements,ids))visit(*e);
    });
}
void PanelEditor::applyPen(const Pen &pen,Machining machining){
    currentPen=pen;currentMachining=machining;
    applyToSelection([&](Element &e){if(e.type==ElementType::Image||e.type==ElementType::Picture)return;e.pen=pen;if(e.type!=ElementType::Drill)e.machining=machining;},false);
}
void PanelEditor::applyFill(const Fill &fill){
    currentFill=fill;applyToSelection([&](Element &e){if(e.closed()||e.type==ElementType::Text||e.type==ElementType::Drill)e.fill=fill;},false);
}
void PanelEditor::applyFont(const QString &family,double height,bool bold,bool italic,const QString &strokeFont){
    currentFont=family;currentTextHeight=height;currentBold=bold;currentItalic=italic;currentStrokeFont=strokeFont;
    applyToSelection([&](Element &e){if(e.type!=ElementType::Text)return;e.font=family;e.bold=bold;e.italic=italic;e.strokeFont=strokeFont;refit(e,height);},true);
}
void PanelEditor::distribute(const DistributeOptions &o){
    const QStringList ids=area->selection();if(rootsOf(doc.panel().elements,ids).size()<2)return;
    change([&](Document &d){
        const auto list=rootsOf(d.panel().elements,ids);
        for(int axis=0;axis<2;axis++){
            if(!(axis==0?o.horizontal:o.vertical))continue;
            const int reference=axis==0?o.horizontalReference:o.verticalReference,mode=axis==0?o.horizontalMode:o.verticalMode;
            const double value=axis==0?o.horizontalValue:o.verticalValue;const bool pen=axis==0?o.horizontalPen:o.verticalPen;
            struct Item{Element *e;double low,high;};QList<Item> items;
            for(auto *e:list){const QRectF b=pen?elementBounds(*e):shapeOf(*e);items<<Item{e,axis==0?b.left():b.top(),axis==0?b.right():b.bottom()};}
            auto key=[&](const Item &i){return reference==0?(i.low+i.high)/2:reference==2?i.high:i.low;};
            std::sort(items.begin(),items.end(),[&](const Item &a,const Item &b){return key(a)<key(b);});
            const int n=items.size();
            auto shift=[&](Element *e,double by){transformAll({e},axis==0?QTransform::fromTranslate(by,0):QTransform::fromTranslate(0,by));};
            if(reference==3){
                // Equal gaps between neighbouring sides.
                double widths=0;for(const auto &i:items)widths+=i.high-i.low;
                const double span=mode==1?value:items.last().high-items.first().low,gap=mode==2?value:(span-widths)/(n-1);
                double at=items.first().low;for(const auto &i:items){shift(i.e,at-i.low);at+=i.high-i.low+gap;}
            }else{
                const double first=key(items.first()),last=key(items.last());
                const double step=mode==0?(last-first)/(n-1):mode==1?(value-(first-items.first().low)-(items.last().high-last))/(n-1):value;
                for(int k=0;k<n;k++)shift(items[k].e,first+k*step-key(items[k]));
            }
        }
    });
}
void PanelEditor::alignToGrid(const GridAlignOptions &o){
    const QStringList ids=area->selection();const Panel &p=doc.panel();if(ids.isEmpty()||p.grid<=0)return;
    change([&](Document &d){
        const QPointF origin=d.panel().origin;const double grid=d.panel().grid;
        auto snap=[&](double v,double o0){return o0+std::round((v-o0)/grid)*grid;};
        for(auto *e:rootsOf(d.panel().elements,ids)){
            double dx=0,dy=0;
            if(o.horizontal){const QRectF b=o.horizontalPen?elementBounds(*e):shapeOf(*e);const double x=o.horizontalReference==0?b.center().x():o.horizontalReference==1?b.left():b.right();dx=snap(x,origin.x())-x;}
            if(o.vertical){const QRectF b=o.verticalPen?elementBounds(*e):shapeOf(*e);const double y=o.verticalReference==0?b.center().y():o.verticalReference==1?b.top():b.bottom();dy=snap(y,origin.y())-y;}
            if(dx!=0||dy!=0)transformAll({e},QTransform::fromTranslate(dx,dy));
        }
    });
}

// ------------------------------------------------------------------ panels
void PanelEditor::selectPanel(int index){
    if(index<0||index>=doc.panels.size()||index==doc.activePanel){updateTabs();return;}
    area->clearSelection();doc.activePanel=index;action("showGrid")->setChecked(doc.panel().gridVisible);
    area->refresh();area->fitPanel();updateTabs();updateTree();updateSelectionTools();updateActions();
}
void PanelEditor::addPanel(bool duplicate){
    Panel p;
    if(duplicate){p=doc.panel();p.id=newId();p.name=ui("%1 (Kopie)").arg(p.name);QHash<QString,QString> renamed;for(auto &e:p.elements)renewIds(e,&renamed);}
    else{p=newPanel(ui("Frontplatte %1").arg(doc.panels.size()+1),doc.panel().width,doc.panel().height);if(!editPanelProperties(this,p,true))return;}
    area->clearSelection();change([&](Document &d){d.panels.insert(d.activePanel+1,p);d.activePanel++;});
    action("showGrid")->setChecked(doc.panel().gridVisible);area->fitPanel();
}
void PanelEditor::removePanel(){
    if(doc.panels.size()<=1)return;
    if(QMessageBox::question(this,ui("Frontplatte löschen"),ui("Die Frontplatte „%1“ mit allen Objekten löschen?").arg(doc.panel().name))!=QMessageBox::Yes)return;
    area->clearSelection();change([&](Document &d){d.panels.removeAt(d.activePanel);d.activePanel=std::min<int>(d.activePanel,d.panels.size()-1);
        for(auto &v:d.views)if(v.panel>=d.panels.size())v.panel=d.panels.size()-1;});
    area->fitPanel();
}
void PanelEditor::movePanel(int direction){
    const int j=doc.activePanel+direction;if(j<0||j>=doc.panels.size())return;
    change([&](Document &d){d.panels.swapItemsAt(d.activePanel,j);for(auto &v:d.views){if(v.panel==d.activePanel)v.panel=j;else if(v.panel==j)v.panel=d.activePanel;}d.activePanel=j;});
}
void PanelEditor::addPanelsFromFile(){
    const QString file=QFileDialog::getOpenFileName(this,ui("Frontplatten aus Datei hinzufügen"),startFolder(),ui("Frontplatten (*.olfp *.fpl *.FPL *.lib *.LIB)"));if(file.isEmpty())return;
    QStringList notes;QString format;Document other;
    try{other=readAny(file,&notes,&format);}catch(const std::exception &e){QMessageBox::warning(this,ui("Öffnen"),ui("%1 kann nicht geöffnet werden:\n%2").arg(QFileInfo(file).fileName(),QString::fromUtf8(e.what())));return;}
    rememberFolder(file);area->clearSelection();
    change([&](Document &d){
        const int first=d.panels.size();
        for(Panel p:other.panels){p.id=newId();p.boards.clear();QHash<QString,QString> renamed;for(auto &e:p.elements)renewIds(e,&renamed);d.panels<<p;}   // the boards behind belong to another project
        for(auto it=other.resources.constBegin();it!=other.resources.constEnd();++it)d.resources.insert(it.key(),it.value());
        if(d.panels.size()>first)d.activePanel=first;
    });
    area->fitPanel();
}

// ------------------------------------------------------------------ inserting
void PanelEditor::addText(QPointF at){
    Element t=styled(ElementType::Text);const double h=currentTextHeight;t.frame={at,at+QPointF(1,0),at+QPointF(0,h)};
    if(!editText(this,t,true))return;
    change([&](Document &d){d.panel().elements<<t;});area->setSelection({t.id});area->setTool(PanelView::Select);
}
void PanelEditor::addDrill(QPointF at){
    double diameter=drillDiameter;if(!askDrillDiameter(this,diameter))return;drillDiameter=diameter;
    Element e=styled(ElementType::Drill);e.center=at;e.diameter=diameter;
    change([&](Document &d){d.panel().elements<<e;});area->setSelection({e.id});
}
void PanelEditor::addDimension(QPointF a,QPointF b,QPointF at){
    if(QLineF(a,b).length()<1e-6)return;
    DimensionStyle style=dimensionStyle;
    // Automatic colour: dark on a light panel, light on a dark one.
    if(dimensionAutoColor)style.color=qGray(doc.panel().color.rgb())<128?QColor(Qt::white):QColor(Qt::black);
    const Element e=dimension(a,b,at,style);change([&](Document &d){d.panel().elements<<e;});area->setSelection({e.id});
}
void PanelEditor::importImage(){
    const QString file=QFileDialog::getOpenFileName(this,ui("Bild importieren"),startFolder(),ui("Bilder (*.bmp *.BMP *.jpg *.JPG *.jpeg *.png *.PNG *.emf *.EMF);;Alle Dateien (*)"));
    if(!file.isEmpty())importImageFile(file);
}
void PanelEditor::importImageFile(const QString &file){
    rememberFolder(file);
    QFile f(file);if(!f.open(QIODevice::ReadOnly)){QMessageBox::warning(this,ui("Bild importieren"),f.errorString());return;}
    const qint64 limit=64LL*1024*1024;if(f.size()>limit){QMessageBox::warning(this,ui("Bild importieren"),ui("Datei ist zu groß"));return;}
    const QByteArray data=f.readAll();Element e;QSizeF size;
    if(isEmf(data)&&data.size()>=40){
        // The frame of the picture in hundredths of a millimetre.
        auto at=[&](int offset){return qFromLittleEndian<qint32>(data.constData()+offset);};const double w=(at(32)-at(24))/100.0,h=(at(36)-at(28))/100.0;
        e=newElement(ElementType::Picture);e.resource=doc.addResource(data,"emf");size=QSizeF(w>0.1?w:50,h>0.1?h:50);
    }else{
        const QImage image=QImage::fromData(data);if(image.isNull()){QMessageBox::warning(this,ui("Bild importieren"),ui("Das Bild kann nicht gelesen werden."));return;}
        const QString kind=data.startsWith("BM")?"bmp":data.startsWith("\x89PNG")?"png":"jpg";e=newElement(ElementType::Image);e.resource=doc.addResource(data,kind);
        const double perMetre=image.dotsPerMeterX()>0?image.dotsPerMeterX():3780;size=QSizeF(image.width()/perMetre*1000,image.height()/perMetre*1000);
    }
    // The picture is placed with its bottom left corner.
    e.name=QFileInfo(file).completeBaseName();e.frame=rectFrame(QRectF(QPointF(),size));e.hasAnchor=true;e.anchor=QPointF(0,size.height());
    place({e});
}
void PanelEditor::regularPolygon(){
    if(!askRegularPolygon(this,polygonCorners,polygonRadius,polygonInner,polygonStart))return;
    Element p=openloch::frontpanel::regularPolygon(QPointF(),polygonCorners,polygonRadius,polygonInner,polygonStart);
    const Element s=styled(ElementType::Polygon);p.pen=s.pen;p.fill=s.fill;p.machining=s.machining;p.hasAnchor=true;p.anchor=QPointF();
    place({p});
}
void PanelEditor::scaleWizard(const QString &existing){
    QSettings settings;ScaleParameters p=ScaleParameters::fromJson(QJsonDocument::fromJson(settings.value("frontpanel/scale").toByteArray()).object());
    const Element *old=existing.isEmpty()?nullptr:findIn(doc.panel().elements,existing);
    if(old)p=ScaleParameters::fromJson(old->parameters["scale"].toObject());
    if(!openloch::frontpanel::scaleWizard(this,p,[this](const Element &e){putOnClipboard(doc,{e});updateActions();}))return;
    settings.setValue("frontpanel/scale",QJsonDocument(p.toJson()).toJson(QJsonDocument::Compact));
    if(old){
        const QString id=old->id,name=old->name;const QTransform placement=placementOf(*old);
        change([&](Document &d){if(Element *t=findIn(d.panel().elements,id)){Element s=scale(p,placement);s.id=id;s.name=name;*t=s;}});return;
    }
    // A scale is placed with its reference point: the centre, or the middle of a straight scale.
    Element s=scale(p,QTransform());s.hasAnchor=true;s.anchor=QPointF();place({s});
}
void PanelEditor::cutoutWizard(const QString &existing){
    QSettings settings;CutoutParameters p=CutoutParameters::fromJson(QJsonDocument::fromJson(settings.value("frontpanel/cutout").toByteArray()).object());
    const Element *old=existing.isEmpty()?nullptr:findIn(doc.panel().elements,existing);
    if(old)p=CutoutParameters::fromJson(old->parameters["cutout"].toObject());
    if(!cutoutDialog(this,p))return;
    settings.setValue("frontpanel/cutout",QJsonDocument(p.toJson()).toJson(QJsonDocument::Compact));
    if(old){
        const QString id=old->id,name=old->name;const QTransform placement=placementOf(*old);
        change([&](Document &d){if(Element *t=findIn(d.panel().elements,id)){Element c=cutout(p,placement);c.id=id;c.name=p.name.isEmpty()?name:p.name;*t=c;}});return;
    }
    Element c=cutout(p,QTransform());c.hasAnchor=true;c.anchor=QPointF();place({c});
}
void PanelEditor::addToLibrary(){
    const QList<Element> list=selectedCopies();if(list.isEmpty())return;
    Element symbol;if(list.size()==1)symbol=list[0];else{symbol=newElement(ElementType::Group);symbol.children=list;symbol.pen.style=PenStyle::None;}
    if(symbol.name.isEmpty())symbol.name=ui("Symbol");
    // A symbol of the library stands for no component of a project.
    std::function<void(Element&)> plain=[&](Element &e){e.component.clear();for(auto &c:e.children)plain(c);};plain(symbol);
    if(!editSymbol(this,doc,symbol,doc.panel().grid))return;
    sideDock->show();side->showPage(0);side->addSymbol(symbol);
}

// ------------------------------------------------------------------ menus, properties, output
void PanelEditor::showContextMenu(QPoint global,int node){
    QMenu menu;
    if(node>=0){menu.addAction(ui("Knoten hinzufügen"),this,[this,node]{area->addNode(node);});menu.addAction(ui("Knoten löschen"),this,[this,node]{area->deleteNode(node);});menu.exec(global);return;}
    for(const char *name:{"cut","copy","paste","duplicate","delete","","front","back","","properties","","addToLibrary","","rename"}){if(*name)menu.addAction(action(name));else menu.addSeparator();}
    menu.exec(global);
}
void PanelEditor::showTreeMenu(QPoint at){
    if(QTreeWidgetItem *item=tree->itemAt(at);item&&!item->isSelected()){tree->clearSelection();item->setSelected(true);}
    showContextMenu(tree->viewport()->mapToGlobal(at),-1);
}
void PanelEditor::showProperties(const QString &id){
    Element *found=findIn(doc.panel().elements,id);if(!found)return;Element copy=*found;
    if(copy.type==ElementType::Scale&&copy.parameters["generator"]=="scale"){scaleWizard(id);return;}
    if(copy.type==ElementType::Cutout&&copy.parameters["generator"]=="cutout"){cutoutWizard(id);return;}
    const bool accepted=copy.type==ElementType::Text?editText(this,copy,false):editElementProperties(this,doc,copy);
    if(accepted)change([&](Document &d){if(Element *t=findIn(d.panel().elements,id))*t=copy;});
}
void PanelEditor::exportImage(){
    const QList<Element> selection=selectedCopies();const Panel &panel=doc.panel();
    if(!askImageExport(this,imageOptions,QSizeF(panel.width,panel.height),!selection.isEmpty()))return;
    const QString ending=imageOptions.format.toLower();const QString base=path.isEmpty()?panel.name:QFileInfo(path).completeBaseName();
    QString file=QFileDialog::getSaveFileName(this,ui("Grafik exportieren"),QDir(startFolder()).filePath(base+"."+ending),QString("%1 (*.%2)").arg(imageOptions.format,ending));
    if(file.isEmpty())return;if(QFileInfo(file).suffix().isEmpty())file+="."+ending;rememberFolder(file);
    RenderOptions o=area->options;o.background=imageOptions.background;o.grid=false;
    const bool only=imageOptions.selectionOnly&&!selection.isEmpty();const QRectF crop=only?elementsBounds(selection):QRectF(0,0,panel.width,panel.height);
    // Vector formats: the panel or the marked objects at their size, one painter unit a hundredth of a millimetre in EMF.
    auto paintVector=[&](QPainter &p){
        if(only){if(o.background){Panel bare=panel;bare.elements.clear();paintPanel(p,doc,bare,o);}for(const auto &e:selection)paintElement(p,doc,e,o);}else paintPanel(p,doc,panel,o);
    };
    if(ending=="emf"){
        EmfDevice emf(crop.size(),QStringLiteral("Frontplatte"));emf.setSmoothImages(true);QPainter p;
        if(!p.begin(&emf)){QMessageBox::warning(this,ui("Grafik exportieren"),ui("%1 kann nicht geschrieben werden.").arg(file));return;}
        p.scale(100,100);p.translate(-crop.topLeft());paintVector(p);p.end();
        QSaveFile out(file);if(!out.open(QIODevice::WriteOnly)||out.write(emf.data())<0||!out.commit())QMessageBox::warning(this,ui("Grafik exportieren"),ui("%1 kann nicht geschrieben werden.").arg(file));
        return;
    }
    if(ending=="pdf"){
        QPdfWriter pdf(file);pdf.setResolution(1200);pdf.setPageSize(QPageSize(crop.size(),QPageSize::Millimeter,QString(),QPageSize::ExactMatch));pdf.setPageMargins(QMarginsF());
        QPainter p;if(!p.begin(&pdf)){QMessageBox::warning(this,ui("Grafik exportieren"),ui("%1 kann nicht geschrieben werden.").arg(file));return;}
        p.scale(pdf.resolution()/25.4,pdf.resolution()/25.4);p.translate(-crop.topLeft());paintVector(p);
        p.end();return;
    }
    QImage image=renderPanel(doc,panel,imageOptions.dpi,o,only?&selection:nullptr);
    if(only){const double s=imageOptions.dpi/25.4;image=image.copy(QRectF(crop.left()*s,crop.top()*s,crop.width()*s,crop.height()*s).toAlignedRect().intersected(image.rect()));}
    if(ending!="png"){
        // Formats without transparency get a white ground.
        QImage flat(image.size(),QImage::Format_RGB32);flat.fill(Qt::white);flat.setDotsPerMeterX(image.dotsPerMeterX());flat.setDotsPerMeterY(image.dotsPerMeterY());
        {QPainter p(&flat);p.drawImage(0,0,image);}image=flat;
    }
    if(!image.save(file,ending=="jpg"?"JPG":ending.toUpper().toLatin1().constData()))QMessageBox::warning(this,ui("Grafik exportieren"),ui("%1 kann nicht geschrieben werden.").arg(file));
}
void PanelEditor::exportHpgl(){exportMachining(this,doc,doc.panel(),path);}
// The print settings belong to the panels and are saved with them; a change in the preview is one step of the history.
void PanelEditor::print(){
    QList<PrintSettings> settings;printPanels(this,doc,path.isEmpty()?(doc.title.isEmpty()?ui("Frontplatte"):doc.title):QFileInfo(path).completeBaseName(),&settings);
    bool changed=false;for(int i=0;i<settings.size()&&i<doc.panels.size();i++)changed|=!(settings[i]==doc.panels[i].print);
    if(changed)change([&](Document &d){for(int i=0;i<settings.size()&&i<d.panels.size();i++)d.panels[i].print=settings[i];});
}
}
