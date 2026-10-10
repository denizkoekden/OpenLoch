#include "window.h"
#include <QFileSystemWatcher>
#include "language.h"
#include "canvas.h"
#include "printdialog.h"
#include "picturefill.h"
#include "geometry.h"
#include "legacy_reader.h"
#include "legacy_writer.h"
#include <QActionGroup>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QCollator>
#include <QComboBox>
#include <QInputDialog>
#include <QListWidget>
#include <QPainter>
#include <QStyledItemDelegate>
#include <cmath>
#include <functional>
#include <QColorDialog>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSaveFile>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QPrinter>
#include <QPrintDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPixmap>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QTabBar>
#include <QCryptographicHash>
#include <QGridLayout>
#include <QUrl>
#include <QDesktopServices>
#ifndef Q_OS_WIN
#include <pwd.h>
#include <unistd.h>
#endif
#include <QSpinBox>
#include <QDateTime>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextList>
#include <QFontComboBox>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTimer>
#include <QStandardPaths>
#include <QSettings>
#include <QSlider>
#include <QBuffer>
#include <QRegularExpression>
#include <QDir>
#include <QUuid>
#include <QJsonDocument>
#include <QSignalBlocker>
#include <memory>

namespace openloch {
// LochMaster draws library previews at roughly 4.7 px per millimetre and shrinks wider parts to the panel.
static constexpr double previewScale=.047;
class LibraryList final:public QListWidget {
public:
    using QListWidget::QListWidget;
    std::function<void(QListWidgetItem*)> dragStarted;
protected:
    QStringList mimeTypes() const override{return {Canvas::libraryPartMime};}
    QMimeData *mimeData(const QList<QListWidgetItem*> &items) const override{auto *data=new QMimeData;if(!items.isEmpty())data->setData(Canvas::libraryPartMime,items.first()->data(Qt::UserRole).toString().toUtf8());return data;}
    void startDrag(Qt::DropActions actions) override{if(dragStarted&&currentItem())dragStarted(currentItem());QListWidget::startDrag(actions);}
};
class LibraryDelegate final:public QStyledItemDelegate {
public:
    explicit LibraryDelegate(QListWidget *view):QStyledItemDelegate(view),view(view){}
    void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        p->save();const QRect r=option.rect;if(option.state&QStyle::State_Selected)p->fillRect(r,QColor(198,219,245));
        const auto image=index.data(Qt::DecorationRole).value<QImage>();const auto target=imageRect(image,r);if(!image.isNull())p->drawImage(target,image);
        QFont f=p->font();f.setPixelSize(10);p->setFont(f);p->setPen(index.flags()&Qt::ItemIsEnabled?QColor(128,128,120):QColor(170,60,50));
        p->drawText(QRectF(r.left()+2,target.bottom()+3,r.width()-4,14),Qt::AlignHCenter|Qt::AlignTop|Qt::TextSingleLine,QFontMetrics(f).elidedText(index.data().toString(),Qt::ElideRight,r.width()-4));
        p->setPen(QColor(150,150,140));p->drawLine(r.bottomLeft(),r.bottomRight());p->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem &,const QModelIndex &index) const override {
        const int width=view->viewport()->width();return {width,int(std::ceil(imageRect(index.data(Qt::DecorationRole).value<QImage>(),QRect(0,0,width,0)).height()))+24};
    }
private:
    QListWidget *view;
    static QRectF imageRect(const QImage &image,const QRect &cell){
        QSizeF size=image.isNull()?QSizeF():image.deviceIndependentSize();size*=qMin(1.0,(cell.width()-12)/qMax(1.0,size.width()));
        return QRectF(cell.left()+(cell.width()-size.width())/2,cell.top()+5,size.width(),size.height());
    }
};
// The original's colour grid (8 × 2 colours of the Windows standard palette): a left click chooses the line colour (VG),
// a right click the fill colour (HG); the letters mark both.
class ColourGrid final:public QWidget {
public:
    static QColor colour(int index){static const QRgb colours[16]={0x000000,0x800000,0x008000,0x808000,0x000080,0x800080,0x008080,0xc0c0c0,0x808080,0xff0000,0x00ff00,0xffff00,0x0000ff,0xff00ff,0x00ffff,0xffffff};return QColor(colours[qBound(0,index,15)]);}
    int foreground=7,background=0;std::function<void(bool fill,QColor colour)> chosen;
    explicit ColourGrid(QWidget *parent):QWidget(parent){setFixedSize(145,29);setContextMenuPolicy(Qt::PreventContextMenu);setToolTip(ui("Stiftfarbe (VG) einstellen: linke Maustaste; Füllfarbe (HG) einstellen: rechte Maustaste"));}
protected:
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);QFont f=font();f.setPixelSize(8);f.setBold(true);p.setFont(f);
        for(int i=0;i<16;i++){const QRect cell((i%8)*18,(i/8)*14,18,14);p.fillRect(cell,colour(i));p.setPen(QColor(110,110,110));p.drawRect(cell);
            const QString mark=i==foreground&&i==background?ui("VH"):i==foreground?ui("VG"):i==background?ui("HG"):QString();
            if(!mark.isEmpty()){p.setPen(qGray(colour(i).rgb())<128?Qt::white:Qt::black);p.drawText(cell,Qt::AlignCenter,mark);}}
    }
    void mouseReleaseEvent(QMouseEvent *e) override{
        const int i=qBound(0,int(e->position().y())/14,1)*8+qBound(0,int(e->position().x())/18,7);
        if(e->button()==Qt::LeftButton)foreground=i;else if(e->button()==Qt::RightButton)background=i;else return;update();if(chosen)chosen(e->button()==Qt::RightButton,colour(i));
    }
};
Window::Window(const QString &a,QWidget *parent):QMainWindow(parent),assets(a) {
    openExternally=[](const QUrl &url){return QDesktopServices::openUrl(url);};
    setWindowTitle("OpenLoch");resize(1280,820);canvas=new Canvas(this);
    // As in the original, the tool palette sits between the parts panel and the drawing area.
    {auto *centre=new QWidget(this);auto *row=new QHBoxLayout(centre);row->setContentsMargins(0,0,0,0);row->setSpacing(0);drawingTools=new QToolBar(ui("Zeichnen"),centre);drawingTools->setObjectName("toolPalette");
     drawingTools->setOrientation(Qt::Vertical);drawingTools->setIconSize(QSize(20,20));drawingTools->setToolButtonStyle(Qt::ToolButtonIconOnly);row->addWidget(drawingTools);
     auto *column=new QVBoxLayout;column->setContentsMargins(0,0,0,0);column->setSpacing(0);column->addWidget(canvas,1);
     boardList=new QTabBar(centre);boardList->setObjectName("boardTabs");boardList->setShape(QTabBar::RoundedSouth);boardList->setExpanding(false);boardList->setDrawBase(false);column->addWidget(boardList);
     row->addLayout(column,1);setCentralWidget(centre);
     connect(boardList,&QTabBar::currentChanged,this,[this](int index){if(index>=0&&index!=project.activeBoard)changeBoard(3,index);});}
    if(!qApp->property("openloch.testing").toBool()){lastDirectory=QSettings().value("dialogs/lastDirectory").toString();xrayFillLevel=QSettings().value("view/xrayFill",73).toInt();xrayOutlineLevel=QSettings().value("view/xrayOutline",82).toInt();
        if(QSettings().contains("view/xrayFill"))canvas->setXRayGrey(qRound(xrayFillLevel*2.55),qRound(xrayOutlineLevel*2.55));}
    auto *file=menuBar()->addMenu(ui("&Datei"));
    auto action=[&](QMenu *menu,QString text,QKeySequence shortcut,auto fn){auto *a=menu->addAction(text);a->setShortcut(shortcut);connect(a,&QAction::triggered,this,fn);return a;};
    action(file,ui("Neue &Platine…"),QKeySequence::New,[this]{newProject("board");});
    action(file,ui("Neuer &Schaltplan…"),QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_N),[this]{newProject("schematic");});
    // Datei like the original: open (also backups), save, print, Exportieren, AutoSpeichern and Beenden.
    file->addSeparator();action(file,ui("&Öffnen…"),QKeySequence::Open,[this]{if(openHandler){openHandler(startDirectory());return;}auto p=QFileDialog::getOpenFileName(this,ui("Projekt öffnen"),startDirectory(),ui("OpenLoch / LochMaster (*.openloch *.LM4 *.lm4 *.LMB *.lmb *.LIB *.Lib *.lib);;Sicherungsdatei (*.bak *.BAK *.old *.OLD)"));if(!p.isEmpty()){rememberDirectory(p);openPath(p);}});
    action(file,ui("&Speichern"),QKeySequence::Save,[this]{save();});action(file,ui("Speichern &unter…"),QKeySequence::SaveAs,[this]{save(true);});
    // The print preview like the original's: views, layers, scale, paper, Kacheln and Korrekturfaktoren.
    file->addSeparator();action(file,ui("&Drucken..."),QKeySequence::Print,[this]{
        canvas->clearSelection();canvas->setTool("select");QPrinter printer(QPrinter::HighResolution);
        PrintDialog dialog(project,printer,currentPath.isEmpty()?project.title:QFileInfo(currentPath).completeBaseName(),canvas->unit(),this);dialog.exec();});
    // Exportieren: the original's picture and HPGL files, then OpenLoch's further formats.
    file->addSeparator();auto *exports=file->addMenu(ui("E&xportieren"));
    action(exports,ui("&Grafik… (*.bmp, *.jpg)"),{},[this]{auto p=savePath(ui("Bild exportieren"),project.title,ui("Bitmap (*.bmp);;JPEG (*.jpg *.jpeg)"));QString error;if(!p.isEmpty()&&!canvas->exportImage(p,&error))saveFailed(p,error);})->setObjectName("exportImage");
    action(exports,ui("&HPGL-Bearbeitungsdateien… (*.plt)"),{},[this]{exportHpgl();})->setObjectName("exportHpgl");exports->addSeparator();
    // A document of a project goes to LochMaster's own format here (Speichern writes the project).
    action(exports,ui("LochMaster-Projekt… (*.LM4)"),{},[this]{exportDocument("lm4");})->setObjectName("exportLm4");
    action(exports,ui("PNG-Bild…"),{},[this]{auto p=savePath(ui("Bild exportieren"),project.title,"PNG (*.png)");QString error;if(!p.isEmpty()&&!canvas->exportImage(p,&error))saveFailed(p,error);})->setObjectName("exportPng");
    action(exports,ui("PDF…"),{},[this]{auto p=savePath(ui("PDF exportieren"),project.title,"PDF (*.pdf)");if(!p.isEmpty()&&!canvas->exportPdf(p))saveFailed(p);})->setObjectName("exportPdf");exports->addSeparator();
    action(exports,ui("Bauteilbibliothek…"),{},[this]{exportDocument("lib");})->setObjectName("exportLibrary");
    action(exports,ui("Platinenvorlage…"),{},[this]{exportDocument("lmb");})->setObjectName("exportTemplate");
    action(exports,ui("Auswahl als Bibliotheksbauteil…"),{},[this]{exportDocument("lib",true);})->setObjectName("exportSelection");
    action(exports,ui("Stückliste (CSV)…"),{},[this]{exportBom();})->setObjectName("exportBom");
    file->addSeparator();action(file,ui("A&utoSpeichern…"),QKeySequence(Qt::CTRL|Qt::Key_B),[this]{autoSaveSettings();})->setObjectName("autoSave");
    action(file,ui("Ungespeicherte Sicherung wiederherstellen…"),{},[this]{restoreRecovery();});
    file->addSeparator();auto *quit=action(file,ui("&Beenden"),QKeySequence::Quit,[this]{close();});quit->setMenuRole(QAction::QuitRole);quit->setObjectName("quit");
    // The platform's key for closing a window (Strg+W, macOS ⌘W) closes it; the original's command on Strg+W has none.
    {auto *closeWindow=new QAction(this);closeWindow->setObjectName("closeWindow");closeWindow->setShortcuts(QKeySequence::Close);
     connect(closeWindow,&QAction::triggered,this,[this]{close();});addAction(closeWindow);}
    // Bearbeiten, Anordnen, Bauteil, Bibliothek and Platine as in the original; the shortcuts stay those of the platform
    // (the original's Strg+Z, Strg+H and Strg+M would undo, hide or minimise on macOS). Actions the toolbars, the
    // context menus or the tests look up carry an object name, as some texts appear in more than one menu.
    auto *edit=menuBar()->addMenu(ui("&Bearbeiten"));action(edit,ui("&Rückgängig"),QKeySequence::Undo,[this]{undo();});action(edit,ui("&Wiederholen"),QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_Z),[this]{redo();}); // Strg+Y is "Layout bearbeiten"
    // "Rückgängig-Aktionen…": the number of undo steps, kept in the settings.
    if(!qApp->property("openloch.testing").toBool())history.setLimit(QSettings().value("edit/undoSteps",50).toInt());
    action(edit,ui("Rückgängig-Aktionen…"),{},[this]{bool ok=false;
        const int steps=QInputDialog::getInt(this,ui("Rückgängig-Aktionen"),ui("Höchstzahl der Arbeitsschritte, die rückgängig gemacht werden können (1 bis 50).\nWird die Arbeit an großen Platinen langsam, hilft ein kleinerer Wert."),history.limit(),1,50,1,&ok);
        if(!ok)return;history.setLimit(steps);if(!qApp->property("openloch.testing").toBool())QSettings().setValue("edit/undoSteps",steps);})->setObjectName("undoSteps");
    edit->addSeparator();auto clipboardCopy=[this](bool cut){
        try{auto data=canvas->selectionData();if(data.isEmpty())return;auto *mime=new QMimeData;mime->setData("application/x-openloch-selection",data);QApplication::clipboard()->setMimeData(mime);if(cut)canvas->removeSelected();}
        catch(const std::exception &e){QMessageBox::warning(this,ui("Kopieren fehlgeschlagen"),QString::fromUtf8(e.what()));}
    };
    action(edit,ui("Ausschneiden"),QKeySequence::Cut,[clipboardCopy]{clipboardCopy(true);})->setObjectName("cut");action(edit,ui("Kopieren"),QKeySequence::Copy,[clipboardCopy]{clipboardCopy(false);})->setObjectName("copy");
    action(edit,ui("Einfügen"),QKeySequence::Paste,[this]{
        auto *mime=QApplication::clipboard()->mimeData();if(!mime||!mime->hasFormat("application/x-openloch-selection"))return;
        try{canvas->pasteData(mime->data("application/x-openloch-selection"));}catch(const std::exception &e){QMessageBox::warning(this,ui("Einfügen fehlgeschlagen"),QString::fromUtf8(e.what()));}
    })->setObjectName("paste");
    action(edit,ui("Duplizieren"),QKeySequence(Qt::CTRL|Qt::Key_D),[this]{canvas->duplicateSelected();})->setObjectName("duplicate");
    edit->addSeparator();action(edit,ui("Alles auswählen"),QKeySequence::SelectAll,[this]{canvas->selectAll();});action(edit,ui("Auswahl aufheben"),QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_A),[this]{canvas->clearSelection();});
    edit->addSeparator();action(edit,ui("Löschen"),QKeySequence::Delete,[this]{canvas->removeSelected();})->setObjectName("delete");
    edit->addSeparator();action(edit,ui("Eigenschaften…"),QKeySequence(Qt::ALT|Qt::Key_Return),[this]{properties();})->setObjectName("properties");
    action(edit,ui("Konturknoten bearbeiten…"),{},[this]{editNodes();});
    auto *arrange=menuBar()->addMenu(ui("&Anordnen"));
    action(arrange,ui("Nach vorne setzen"),QKeySequence(Qt::CTRL|Qt::Key_BracketRight),[this]{canvas->reorderSelected(true);})->setObjectName("front");
    action(arrange,ui("Nach hinten setzen"),QKeySequence(Qt::CTRL|Qt::Key_BracketLeft),[this]{canvas->reorderSelected(false);})->setObjectName("back");
    arrange->addSeparator();action(arrange,ui("Auf andere Platinenseite setzen"),{},[this]{canvas->switchSide();})->setObjectName("otherSide");
    arrange->addSeparator();
    action(arrange,ui("Gruppe bilden"),QKeySequence(Qt::CTRL|Qt::Key_G),[this]{try{canvas->groupSelected();}catch(const std::exception &e){QMessageBox::warning(this,ui("Gruppieren fehlgeschlagen"),QString::fromUtf8(e.what()));}})->setObjectName("group");
    action(arrange,ui("Gruppe aufheben"),QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_G),[this]{
        // As in the original a part is dissolved with "Bauteileinheit aufheben", not as a plain group.
        const auto n=canvas->selectedNode();const auto e=n["type"]=="component"?project.componentNode(n):n;
        if(canvas->selectionCount()==1&&e.contains("children")&&e["group_flags"].toArray().at(0).toBool()){QMessageBox::information(this,ui("Gruppe aufheben"),ui("Bauteile werden mit „Bauteil → Bauteileinheit aufheben“ aufgelöst."));return;}
        try{canvas->groupSelected(true);}catch(const std::exception &e){QMessageBox::warning(this,ui("Auflösen fehlgeschlagen"),QString::fromUtf8(e.what()));}})->setObjectName("ungroup");
    findChild<QAction*>("ungroup")->setShortcuts({QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_G),QKeySequence(Qt::CTRL|Qt::Key_U)});
    arrange->addSeparator();auto *align=arrange->addMenu(ui("Ausrichten"));
    // "Mitte senkrecht" lines the centres up on a vertical line (same x), "Mitte waagerecht" on a horizontal one.
    for(const auto &[text,edge]:{std::pair{"Links","left"},std::pair{"Rechts","right"},std::pair{"Oben","top"},std::pair{"Unten","bottom"},std::pair{"Mitte senkrecht","vertical"},std::pair{"Mitte waagerecht","horizontal"}}){
        const QString chosen=edge;action(align,ui(text),{},[this,chosen]{canvas->alignSelected(chosen);})->setObjectName("align-"+chosen);}
    align->addSeparator();action(align,ui("Bauteilanschlüsse an Platinenlöchern"),{},[this]{canvas->alignSelectedToBoard();});
    auto *turn=arrange->addMenu(ui("Drehen"));
    action(turn,ui("90° rechts"),QKeySequence(Qt::Key_R),[this]{canvas->rotateSelected(90);})->setObjectName("rotateRight");
    action(turn,ui("90° links"),QKeySequence(Qt::SHIFT|Qt::Key_R),[this]{canvas->rotateSelected(-90);})->setObjectName("rotateLeft");
    action(turn,ui("180°"),{},[this]{canvas->rotateSelected(180);})->setObjectName("rotate180");turn->addSeparator();
    // "Winkel…" like the original: positive angles turn counter-clockwise unless "Im Uhrzeigersinn" is chosen.
    action(turn,ui("Winkel…"),{},[this]{
        QDialog dialog(this);dialog.setWindowTitle(ui("Drehen"));QFormLayout layout(&dialog);QDoubleSpinBox angle;angle.setObjectName("rotationAngle");angle.setRange(-360,360);angle.setDecimals(1);angle.setSuffix(" °");layout.addRow(ui("Winkel"),&angle);
        QCheckBox clockwise(ui("Im Uhrzeigersinn"));clockwise.setObjectName("rotationClockwise");layout.addRow(ui("Richtung"),&clockwise);
        QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()==QDialog::Accepted&&angle.value()!=0)canvas->rotateSelected(clockwise.isChecked()?angle.value():-angle.value());
    })->setObjectName("rotateAngle");
    auto *flip=arrange->addMenu(ui("Spiegeln"));
    action(flip,ui("Horizontal"),QKeySequence(Qt::Key_M),[this]{canvas->mirrorSelected(true);})->setObjectName("mirrorHorizontal");
    action(flip,ui("Vertikal"),QKeySequence(Qt::SHIFT|Qt::Key_M),[this]{canvas->mirrorSelected(false);})->setObjectName("mirrorVertical");
    // Library menu as in LochMaster. Own pages live in OpenLoch's library folder; pages of a LochMaster installation stay read-only.
    auto *componentMenu=menuBar()->addMenu(ui("Bau&teil"));
    action(componentMenu,ui("Bauteileinheit bilden…"),QKeySequence(Qt::CTRL|Qt::Key_K),[this]{defineComponent();});action(componentMenu,ui("Bauteileinheit aufheben"),QKeySequence(Qt::CTRL|Qt::Key_J),[this]{dissolveComponent();});
    componentMenu->addSeparator();action(componentMenu,ui("Zur Bibliothek hinzufügen"),QKeySequence(Qt::CTRL|Qt::Key_I),[this]{addSelectionToLibrary();})->setObjectName("addToLibrary");
    componentMenu->addSeparator();action(componentMenu,ui("Neu nummerieren"),{},[this]{canvas->renumber();});
    componentMenu->addSeparator();action(componentMenu,ui("Assis&tent…"),{},[this]{componentAssistant();});
    auto *libraryMenu=menuBar()->addMenu(ui("B&ibliothek"));
    action(libraryMenu,ui("&Eigenschaften…"),{},[this]{libraryPageProperties();})->setObjectName("libraryProperties");libraryMenu->addSeparator();
    action(libraryMenu,ui("Seite anlegen…"),{},[this]{createLibraryPage();});action(libraryMenu,ui("Seite löschen…"),{},[this]{deleteLibraryPage();});
    libraryMenu->addSeparator();auto *order=libraryMenu->addMenu(ui("Bauteilreihenfolge ändern"));
    action(order,ui("Bauteil nach unten setzen"),{},[this]{moveLibraryPart(1);});action(order,ui("Bauteil nach oben setzen"),{},[this]{moveLibraryPart(0);});order->addSeparator();
    action(order,ui("Bauteil eins nach oben setzen"),{},[this]{moveLibraryPart(2);});action(order,ui("Bauteil eins nach unten setzen"),{},[this]{moveLibraryPart(3);});
    libraryMenu->addSeparator();action(libraryMenu,ui("Bauteil aus Bibliothek löschen"),{},[this]{deleteLibraryPart();});
    libraryMenu->addSeparator();action(libraryMenu,ui("Als Bauteildatei speichern…"),{},[this]{saveSelectionAsPartFile();});
    action(libraryMenu,ui("Bauteilordner im Dateimanager zeigen"),{},[this]{QDir().mkpath(componentFolder());QDesktopServices::openUrl(QUrl::fromLocalFile(componentFolder()));});
    action(libraryMenu,ui("Bauteilordner wählen…"),{},[this]{chooseComponentFolder();});
    libraryMenu->addSeparator();action(libraryMenu,ui("LochMaster-Bibliotheken einbinden…"),{},[this]{chooseLochMaster();});
    action(libraryMenu,ui("OpenLoch-Bibliothek als LIB-Dateien speichern…"),{},[this]{saveOpenLibrary();});
    auto *boards=menuBar()->addMenu(ui("&Platine"));
    action(boards,ui("Eigenschaften…"),QKeySequence(Qt::CTRL|Qt::Key_E),[this]{projectProperties();})->setObjectName("boardProperties");boards->addSeparator();
    action(boards,ui("Layout bearbeiten…"),QKeySequence(Qt::CTRL|Qt::Key_Y),[this]{editLayout();});action(boards,ui("Anmerkungen…"),QKeySequence(Qt::CTRL|Qt::Key_T),[this]{showNotes();});
    {   // The object tree, shown and hidden from the menu; a click selects the objects (no zoom), as in the original.
        treeDock=new QDockWidget(ui("Objektbaum"),this);treeDock->setObjectName("objectTreeDock");objectTree=new QTreeWidget(treeDock);objectTree->setObjectName("objectTree");objectTree->setHeaderHidden(true);
        objectTree->setSelectionMode(QAbstractItemView::ExtendedSelection);treeDock->setWidget(objectTree);addDockWidget(Qt::RightDockWidgetArea,treeDock);treeDock->hide();
        // A double click opens the properties, as in the original: of the object, or the Bauteil dialog of a part inside a part.
        connect(objectTree,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *item){const auto path=item->data(0,Qt::UserRole+2).toString();
            if(path.isEmpty()){properties();return;}nestedPartProperties(item->data(0,Qt::UserRole).toString(),item->data(0,Qt::UserRole+1).toInt(),path);});
        connect(objectTree,&QTreeWidget::itemSelectionChanged,this,[this]{QList<std::pair<QString,int>> chosen;for(auto *item:objectTree->selectedItems()){auto *top=item;while(top->parent())top=top->parent();chosen.append({top->data(0,Qt::UserRole).toString(),top->data(0,Qt::UserRole+1).toInt()});}treeChoosing=true;canvas->selectObjects(chosen);treeChoosing=false;});
        auto *show=boards->addAction(ui("Objektbaum anzeigen"));show->setCheckable(true);connect(show,&QAction::toggled,this,[this](bool on){treeDock->setVisible(on);fillObjectTree();});
        connect(treeDock,&QDockWidget::visibilityChanged,show,[show](bool on){QSignalBlocker quiet(show);show->setChecked(on);});
        connect(canvas->scene(),&QGraphicsScene::selectionChanged,treeDock,[this]{fillObjectTree();});
    }
    auto *partsList=boards->addMenu(ui("Stückliste"));action(partsList,ui("Erstellen…"),QKeySequence(Qt::CTRL|Qt::Key_L),[this]{showPartsList();});
    // "Excel erzeugen" like the original, which starts Excel and pastes the table into it: the table goes to the clipboard
    // and, as a CSV file in the temporary folder, to the program the system opens such files with.
    action(partsList,ui("Excel erzeugen…"),{},[this]{
        QApplication::clipboard()->setText(billOfMaterialsTable('\t'));
        QString name=project.title.trimmed();name.replace(QRegularExpression(R"([\\/:*?"<>|])"),"_");
        const QString path=QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(ui("Stückliste")+(name.isEmpty()?QString():" "+name)+".csv");
        const QByteArray bytes="\xEF\xBB\xBF"+billOfMaterialsTable(';').toUtf8();QSaveFile f(path);
        if(f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit()&&openExternally(QUrl::fromLocalFile(path)))statusBar()->showMessage(ui("Stückliste in der Tabellenkalkulation geöffnet und in der Zwischenablage."),6000);
        else statusBar()->showMessage(ui("Stückliste in der Zwischenablage – in Excel einfügen."),6000);
    })->setObjectName("partsListExcel");
    boards->addSeparator();action(boards,ui("Hinzufügen"),{},[this]{changeBoard(0);})->setObjectName("boardAdd");
    action(boards,ui("Datei hinzufügen…"),{},[this]{
        const auto path=QFileDialog::getOpenFileName(this,ui("Platinen aus Datei hinzufügen"),startDirectory(),ui("OpenLoch / LochMaster (*.openloch *.LM4 *.lm4)"));if(path.isEmpty())return;rememberDirectory(path);
        try{const auto other=Project::load(path);if(other.mode!="board")throw FormatError(ui("Die Datei enthält keine Platine."));changeBoard(9,0,{},&other);}
        catch(const std::exception &e){QMessageBox::warning(this,ui("Datei hinzufügen"),QString::fromUtf8(e.what()));}
    })->setObjectName("boardAddFile");
    action(boards,ui("Duplizieren"),{},[this]{changeBoard(1);})->setObjectName("boardDuplicate");
    action(boards,ui("Platine umbenennen…"),{},[this]{bool ok=false;const auto name=QInputDialog::getText(this,ui("Platine umbenennen"),ui("Name der Platine"),QLineEdit::Normal,project.title,&ok).trimmed();if(ok&&!name.isEmpty()&&name!=project.title)changeBoard(8,0,name);})->setObjectName("boardRename");
    auto *boardOrder=boards->addMenu(ui("Reihenfolge ändern"));
    action(boardOrder,ui("Platine nach links"),{},[this]{changeBoard(7,0);})->setObjectName("boardFirst");action(boardOrder,ui("Platine nach rechts"),{},[this]{changeBoard(7,int(project.boards.size())-1);})->setObjectName("boardLast");boardOrder->addSeparator();
    action(boardOrder,ui("Platine eins nach links"),{},[this]{changeBoard(7,project.activeBoard-1);})->setObjectName("boardLeft");action(boardOrder,ui("Platine eins nach rechts"),{},[this]{changeBoard(7,project.activeBoard+1);})->setObjectName("boardRight");
    auto *boardTurn=boards->addMenu(ui("Drehen"));
    action(boardTurn,ui("90° rechts"),{},[this]{changeBoard(4);})->setObjectName("boardRotateRight");action(boardTurn,ui("90° links"),{},[this]{changeBoard(5);})->setObjectName("boardRotateLeft");
    action(boardTurn,ui("180°"),{},[this]{changeBoard(6);})->setObjectName("boardRotate180");
    boards->addSeparator();action(boards,ui("Löschen…"),{},[this]{
        if(project.boards.size()<2){QMessageBox::information(this,ui("Platine löschen"),ui("Die einzige Platine eines Projekts kann nicht gelöscht werden."));return;}
        if(qApp->property("openloch.testing").toBool()||QMessageBox::question(this,ui("Platine löschen"),QString(ui("Platine „%1“ löschen?")).arg(project.title))==QMessageBox::Yes)changeBoard(2);})->setObjectName("boardDelete");
    // The context menus of the drawing area and of the board tabs, as in the original, from the same actions.
    auto popup=[this](QMenu &menu,std::initializer_list<const char*> names){for(const char *name:names){if(!*name){menu.addSeparator();continue;}if(auto *a=menuBar()->findChild<QAction*>(name))menu.addAction(a);}};
    canvas->contextRequested=[this,popup,turn](QPoint at){QMenu menu(this);popup(menu,{"properties","","cut","copy","paste","duplicate","","delete",""});menu.addMenu(turn);
        popup(menu,{"","front","back","","group","ungroup","","addToLibrary"});menu.exec(at);};
    boardList->setContextMenuPolicy(Qt::CustomContextMenu);
    // A right click chooses the board under it first, so that its menu acts on that board.
    connect(boardList,&QWidget::customContextMenuRequested,this,[this,popup,boardOrder,boardTurn](QPoint at){
        if(const int tab=boardList->tabAt(at);tab>=0&&tab!=boardList->currentIndex())boardList->setCurrentIndex(tab);
        QMenu menu(this);popup(menu,{"boardProperties","","boardAdd","boardDuplicate","boardRename","","boardDelete","","boardAddFile",""});
        menu.addMenu(boardOrder);menu.addSeparator();menu.addMenu(boardTurn);menu.exec(boardList->mapToGlobal(at));});
    // Ansicht like the original: Optionen (the view switches of the board), Lineale / Raster, Zoom and Werkzeuge anzeigen.
    auto *view=menuBar()->addMenu(ui("&Ansicht"));auto *options=view->addMenu(ui("Optionen"));
    // The original's independent view switches (per board); Umriss is its S/W display.
    auto toggle=[&](const QString &text,bool on,std::function<void(Canvas::ViewState&,bool)> set){auto *a=options->addAction(ui(text));a->setCheckable(true);a->setChecked(on);
        connect(a,&QAction::toggled,this,[this,set](bool enabled){auto state=canvas->viewState();set(state,enabled);canvas->setViewState(state);refreshLibraryPreviews();});viewSwitches[text]=a;return a;};
    toggle("S/W-Darstellung",false,[](Canvas::ViewState &v,bool on){v.mono=on;});toggle("Wenden",false,[](Canvas::ViewState &v,bool on){v.flip=on;});
    toggle("Durchsicht",false,[](Canvas::ViewState &v,bool on){v.through=on;});toggle("Röntgenblick",true,[](Canvas::ViewState &v,bool on){v.xray=on;});
    toggle("BMP-Rendering",true,[](Canvas::ViewState &v,bool on){v.bitmaps=on;});
    potentialsAction=options->addAction(ui("Potenziale anzeigen"));potentialsAction->setCheckable(true);connect(potentialsAction,&QAction::toggled,canvas,&Canvas::setShowPotentials);
    auto *smooth=options->addAction(ui("Fonts glätten"));smooth->setCheckable(true);smooth->setChecked(true);connect(smooth,&QAction::toggled,canvas,[this](bool on){canvas->setRenderHint(QPainter::TextAntialiasing,on);canvas->viewport()->update();});
    options->addSeparator();auto *components=options->addAction(ui("Bestückung anzeigen"));components->setCheckable(true);components->setChecked(true);connect(components,&QAction::toggled,canvas,&Canvas::setShowComponents);
    action(options,ui("Röntgenkontrast…"),{},[this]{
        QDialog d(this);d.setWindowTitle(ui("Röntgenkontrast"));QFormLayout f(&d);QSlider fill(Qt::Horizontal),outline(Qt::Horizontal);fill.setObjectName("xrayFill");outline.setObjectName("xrayOutline");
        for(auto *s:{&fill,&outline})s->setRange(0,100);fill.setValue(xrayFillLevel);outline.setValue(xrayOutlineLevel);f.addRow(ui("Füllung"),&fill);f.addRow(ui("Rand"),&outline);
        // Grey = Round(position · 255 / 100), shown at once.
        auto apply=[&]{xrayFillLevel=fill.value();xrayOutlineLevel=outline.value();canvas->setXRayGrey(qRound(xrayFillLevel*2.55),qRound(xrayOutlineLevel*2.55));canvas->setViewState(canvas->viewState());};
        connect(&fill,&QSlider::valueChanged,&d,apply);connect(&outline,&QSlider::valueChanged,&d,apply);QDialogButtonBox b(QDialogButtonBox::Close);f.addRow(&b);connect(&b,&QDialogButtonBox::rejected,&d,&QDialog::accept);
        d.exec();if(!qApp->property("openloch.testing").toBool()){QSettings().setValue("view/xrayFill",xrayFillLevel);QSettings().setValue("view/xrayOutline",xrayOutlineLevel);}
    });
    // Ansicht → Lineale / Raster like the original: the unit of rulers and coordinates, which also chooses the snap grid
    // (N: holes of the board's pitch counted from the origin, mm and inch: the board's grids for them), and the rulers.
    auto *units=view->addMenu(ui("Lineale / Raster"));auto *unitGroup=new QActionGroup(this);unitActions={nullptr,nullptr,nullptr};
    for(auto [text,unit]:std::initializer_list<std::pair<QString,int>>{{ui("Einheit N"),2},{ui("Einheit inch"),1},{ui("Einheit mm"),0}}){
        auto *a=units->addAction(text);a->setCheckable(true);a->setChecked(unit==2);unitGroup->addAction(a);unitActions[unit]=a;const int chosen=unit;
        connect(a,&QAction::triggered,this,[this,chosen]{canvas->setUnit(chosen);});
    }
    units->addSeparator();auto *rulers=units->addAction(ui("Lineale anzeigen"));rulers->setCheckable(true);connect(rulers,&QAction::toggled,canvas,&Canvas::setRulers);
    auto *zoom=view->addMenu(ui("Zoom"));action(zoom,ui("Vergrößern"),QKeySequence::ZoomIn,[this]{canvas->scale(1.25,1.25);})->setShortcuts({QKeySequence::ZoomIn,QKeySequence(Qt::Key_F5)});
    action(zoom,ui("Verkleinern"),QKeySequence::ZoomOut,[this]{canvas->scale(.8,.8);})->setShortcuts({QKeySequence::ZoomOut,QKeySequence(Qt::Key_F6)});
    action(zoom,ui("&Einpassen"),QKeySequence(Qt::Key_F),[this]{canvas->fit();});action(zoom,ui("Zoom Objekte"),{},[this]{canvas->fitObjects(false);});
    action(zoom,ui("Markierte Objekte zoomen"),{},[this]{canvas->fitObjects(true);});action(zoom,ui("Reale Größe 1:1"),{},[this]{canvas->realSize();});
    auto *check=menuBar()->addMenu(ui("&Prüfen"));action(check,ui("Kurzschlüsse prüfen"),QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_K),[this]{checkShorts(true);});
    // With a schematic in the same project (the suite): its components and nets as target connections of the board.
    check->addSeparator();
    action(check,ui("Mit Schaltplan vergleichen"),{},[this]{compareWithSchematic(true);})->setObjectName("compareSchematic");
    airwiresAction=check->addAction(ui("Luftlinien anzeigen"));airwiresAction->setObjectName("showAirwires");airwiresAction->setCheckable(true);
    connect(airwiresAction,&QAction::toggled,this,[this](bool on){if(on)compareWithSchematic(false);else canvas->setAirwires({});});
    action(check,ui("Aus Schaltplan übernehmen…"),{},[this]{takeOverFromSchematic();})->setObjectName("takeOverSchematic");
    action(check,ui("Anschlüsse zuordnen…"),{},[this]{assignPins();})->setObjectName("assignPins");
    action(check,ui("Fehlende Bauteile setzen…"),{},[this]{placeMissingParts();})->setObjectName("placeMissing");
    connect(check,&QMenu::aboutToShow,this,[this]{projectChanged();});
    // The original's unit button in the corner of the rulers ("Raster umschalten", Ctrl+<): mm, inch, N in turn.
    {
        auto *unitButton=new QToolButton(canvas->viewport());unitButton->setObjectName("unitButton");unitButton->setToolTip(ui("Raster umschalten (Strg+<)"));unitButton->setAutoRaise(true);unitButton->setGeometry(0,0,42,22);unitButton->setText("N");unitButton->hide();
        connect(rulers,&QAction::toggled,unitButton,&QWidget::setVisible);
        auto next=[this,unitButton]{int current=0;for(int i=0;i<unitActions.size();i++)if(unitActions[i]->isChecked())current=i;auto *a=unitActions.value((current+1)%unitActions.size());if(a)a->trigger();};
        auto *cycle=units->addAction(ui("Raster umschalten"));cycle->setShortcut(QKeySequence(Qt::CTRL|Qt::Key_Less));connect(cycle,&QAction::triggered,this,next);connect(unitButton,&QToolButton::clicked,this,next);
        for(int i=0;i<unitActions.size();i++){const QString label=i==0?"mm":i==1?"inch":"N";connect(unitActions[i],&QAction::toggled,unitButton,[unitButton,label](bool on){if(on)unitButton->setText(label);});}
    }
    // Sprache like the original's language dialog (Deutsch, English, Français there); the choice takes effect at the next start.
    auto *languages=view->addMenu(ui("Sprache"));auto *languageGroup=new QActionGroup(this);
    for(auto [label,code]:std::initializer_list<std::pair<const char*,const char*>>{{"Deutsch","de"},{"English","en"},{"Français","fr"}}){
        auto *a=languages->addAction(label);a->setCheckable(true);a->setChecked(uiLanguage()==code);a->setObjectName(QString("language-")+code);languageGroup->addAction(a);const QString chosen=code;
        connect(a,&QAction::triggered,this,[this,chosen]{QSettings().setValue("ui/language",chosen);if(chosen!=uiLanguage())QMessageBox::information(this,ui("Sprache"),ui("Die Sprache wird beim nächsten Start übernommen."));});
    }
    auto *help=menuBar()->addMenu(ui("&Hilfe"));
    // "Hilfethemen…" like the original's F1, with OpenLoch's own help pages.
    {auto *topics=action(help,ui("&Hilfethemen…"),QKeySequence::HelpContents,[this]{showHelp();});topics->setObjectName("helpTopics");
     // F1 as in the original on every system; macOS gives Hilfethemen ⌘? instead.
     auto keys=QKeySequence::keyBindings(QKeySequence::HelpContents);if(!keys.contains(QKeySequence(Qt::Key_F1)))keys<<QKeySequence(Qt::Key_F1);topics->setShortcuts(keys);}help->addSeparator();action(help,ui("Über OpenLoch"),{},[this]{QMessageBox::about(this,ui("OpenLoch 0.1"),ui("<b>OpenLoch</b><br>Native Werkzeuge für Lochraster und Schaltpläne.<br><br>Experimenteller Entwicklungsstand. LochMaster-Dateien der Versionen 3.08 bis 4.07 werden gelesen; Platinen werden wie im Original als LM4 gespeichert, wahlweise als .openloch. Schaltpläne können neu gezeichnet werden; sPlan-Import folgt.<br><br>Darstellung und Druck sind noch nicht vollständig mit dem Original abgeglichen."));});
    // The original's toolbars with OpenLoch's own symbols in its style: file, arrange, edit, alignment and view at the top,
    // the drawing tools on the left, outline, milling, pictures and zoom at the bottom. They use the menu's actions.
    auto find=[this](const QString &german)->QAction*{if(auto *named=menuBar()->findChild<QAction*>(german))return named;const auto wanted=ui(german);for(auto *a:menuBar()->findChildren<QAction*>())if(a->text()==wanted&&!a->menu())return a;return nullptr;};
    auto iconBar=[&](const QString &title,const QString &name,Qt::ToolBarArea area,std::initializer_list<std::pair<const char*,const char*>> items){
        auto *iconBarWidget=new QToolBar(ui(title),this);iconBarWidget->setObjectName(name);iconBarWidget->setIconSize(QSize(16,16));iconBarWidget->setToolButtonStyle(Qt::ToolButtonIconOnly);addToolBar(area,iconBarWidget);
        for(const auto &[text,icon]:items){if(!*text){iconBarWidget->addSeparator();continue;}if(auto *a=find(QString::fromUtf8(text))){a->setIcon(openLochIcon(icon));iconBarWidget->addAction(a);}}
        return iconBarWidget;
    };
    iconBar("Datei","fileBar",Qt::TopToolBarArea,{{"Neue &Platine…","new"},{"&Öffnen…","open"},{"&Speichern","save"},{"&Drucken...","print"}});
    iconBar("Anordnen","arrangeBar",Qt::TopToolBarArea,{{"group","group"},{"ungroup","ungroup"},{"front","front"},{"back","back"}});
    iconBar("Bearbeiten","editBar",Qt::TopToolBarArea,{{"Kopieren","copy"},{"Ausschneiden","cut"},{"Einfügen","paste"},{"duplicate","duplicate"},{"",""},{"delete","delete"},{"",""},
        {"&Rückgängig","undo"},{"undoSteps","undolist"},{"&Wiederholen","redo"},{"",""},{"rotateRight","rotate"},{"mirrorHorizontal","mirror"},{"properties","properties"}});
    iconBar("Ausrichten","alignBar",Qt::TopToolBarArea,{{"align-left","align-left"},{"align-vertical","align-hcenter"},{"align-right","align-right"},{"align-top","align-top"},{"align-horizontal","align-vcenter"},{"align-bottom","align-bottom"}});
    auto *viewBar=iconBar("Ansicht","viewBar",Qt::TopToolBarArea,{{"S/W-Darstellung","view-mono"},{"Wenden","view-flip"},{"Durchsicht","view-through"},{"Röntgenblick","view-xray"},{"BMP-Rendering","view-bitmaps"}});
    // Like LochMaster's toolbar button: free areas are shown only while the button is held down.
    auto *freeAreas=new QToolButton(viewBar);freeAreas->setObjectName("freeAreas");freeAreas->setText(ui("Freie Bereiche"));freeAreas->setIcon(openLochIcon("view-free"));freeAreas->setToolTip(ui("Freie Bereiche anzeigen (gedrückt halten)"));viewBar->addWidget(freeAreas);
    connect(freeAreas,&QToolButton::pressed,this,[this]{canvas->setShowFreeAreas(true);});connect(freeAreas,&QToolButton::released,this,[this]{canvas->setShowFreeAreas(false);});
    if(auto *a=find("Potenziale anzeigen")){a->setIcon(openLochIcon("view-potentials"));viewBar->addAction(a);}
    // Component panel as in LochMaster: page selector, drawn parts with their names, page buttons.
    auto *parts=new QDockWidget(ui("Bauteile"),this);parts->setObjectName("partsDock");parts->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);auto *panel=new QWidget(parts);auto *column=new QVBoxLayout(panel);column->setContentsMargins(2,2,2,2);column->setSpacing(2);
    libraryPages=new QComboBox(panel);libraryPages->setObjectName("libraryPages");column->addWidget(libraryPages);
    auto *list=new LibraryList(panel);libraryParts=list;list->setObjectName("libraryParts");list->setItemDelegate(new LibraryDelegate(list));list->setResizeMode(QListView::Adjust);list->setUniformItemSizes(false);list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);list->setDragEnabled(true);list->setDragDropMode(QAbstractItemView::DragOnly);list->setMinimumWidth(180);QPalette cream=list->palette();cream.setColor(QPalette::Base,QColor("#fef5cf"));list->setPalette(cream);column->addWidget(list,1);
    auto *pageButtons=new QHBoxLayout;auto *previous=new QPushButton("<<<",panel),*next=new QPushButton(">>>",panel);for(auto *b:{previous,next}){b->setToolTip(ui("Blättern"));b->setAutoDefault(false);}pageButtons->addWidget(previous);pageButtons->addStretch();pageButtons->addWidget(next);column->addLayout(pageButtons);
    parts->setWidget(panel);addDockWidget(Qt::LeftDockWidgetArea,parts);
    connect(libraryPages,&QComboBox::currentIndexChanged,this,&Window::showLibraryPage);
    connect(previous,&QPushButton::clicked,this,[this]{if(libraryPages->currentIndex()>0)libraryPages->setCurrentIndex(libraryPages->currentIndex()-1);});
    connect(next,&QPushButton::clicked,this,[this]{if(libraryPages->currentIndex()+1<libraryPages->count())libraryPages->setCurrentIndex(libraryPages->currentIndex()+1);});
    auto arm=[this](QListWidgetItem *item){auto path=item->data(Qt::UserRole).toString();if(!path.isEmpty())placeComponent(path,item->data(Qt::UserRole+1).toInt());};
    connect(list,&QListWidget::itemClicked,this,arm);connect(list,&QListWidget::itemActivated,this,arm);list->dragStarted=arm;
    list->setContextMenuPolicy(Qt::CustomContextMenu);connect(list,&QWidget::customContextMenuRequested,this,[this,list,arm](QPoint at){auto *item=list->itemAt(at);QString path=libraryPages->currentData().toString();if(path.isEmpty())return;QMenu menu(this);
        // As in the original "Eigenschaften…" comes first: the part's on an own page, else the page's.
        if(item)libraryParts->setCurrentItem(item);else libraryParts->setCurrentRow(-1);menu.addAction(ui("Eigenschaften…"),[this]{libraryPartProperties();});menu.addSeparator();
        if(item&&!item->data(Qt::UserRole).toString().isEmpty())menu.addAction(ui("Bauteil platzieren"),[item,arm]{arm(item);});
        if(libraryPages->currentData(Qt::UserRole+1).toBool()){
            menu.addSeparator();menu.addAction(ui("Nach oben setzen"),[this]{moveLibraryPart(0);});menu.addAction(ui("Nach unten setzen"),[this]{moveLibraryPart(1);});
            menu.addAction(ui("Eins nach oben setzen"),[this]{moveLibraryPart(2);});menu.addAction(ui("Eins nach unten setzen"),[this]{moveLibraryPart(3);});
            menu.addSeparator();menu.addAction(ui("Löschen"),[this]{deleteLibraryPart();});menu.addSeparator();
            menu.addAction(ui("Seite anlegen"),[this]{createLibraryPage();});menu.addAction(ui("Seite umbenennen"),[this]{libraryPageProperties();});menu.addAction(ui("Seite löschen"),[this]{deleteLibraryPage();});
        }
        if(libraryPages->currentData(Qt::UserRole+2).toBool()){
            // Folder pages: files instead of records.
            menu.addSeparator();menu.addAction(ui("Markiertes Bauteil hier speichern…"),[this]{saveSelectionAsPartFile();});
            if(item&&!item->data(Qt::UserRole).toString().isEmpty()){const auto file=item->data(Qt::UserRole).toString();
                menu.addAction(ui("Bauteildatei löschen"),[this,file]{
                    if(QMessageBox::question(this,ui("Bauteildatei löschen"),ui("„")+QFileInfo(file).fileName()+ui("“ in den Papierkorb legen?"))!=QMessageBox::Yes)return;
                    if(!(qApp->property("openloch.testing").toBool()?QFile::remove(file):QFile::moveToTrash(file)))QMessageBox::warning(this,ui("Bauteildatei löschen"),ui("Die Datei konnte nicht entfernt werden."));
                    catalog.remove(file);populateLibrary();});
                menu.addAction(ui("Bauteildatei als Dokument öffnen"),[this,file]{openPath(file);});}
            menu.addAction(ui("Unterordner anlegen…"),[this,path]{bool ok=false;const auto name=QInputDialog::getText(this,ui("Unterordner anlegen"),ui("Name des Ordners"),QLineEdit::Normal,{},&ok).trimmed();
                if(ok&&!name.isEmpty()&&QDir(path).mkpath(name)){populateLibrary();}});
            menu.addAction(ui("Ordner im Dateimanager zeigen"),[path]{QDesktopServices::openUrl(QUrl::fromLocalFile(path));});
            menu.exec(list->viewport()->mapToGlobal(at));return;
        }
        menu.addSeparator();menu.addAction(ui("Bibliotheksseite als Dokument öffnen"),[this,path]{ensureOpenLibraryPage(path);openPath(path);});menu.exec(list->viewport()->mapToGlobal(at));
    });
    auto *dock=new QDockWidget(ui("Projekte und Vorlagen"),this);dock->setObjectName("projectsDock");dock->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);library=new QTreeWidget(dock);library->setObjectName("projectBrowser");library->setHeaderHidden(true);library->setMinimumWidth(180);dock->setWidget(library);addDockWidget(Qt::LeftDockWidgetArea,dock);tabifyDockWidget(parts,dock);parts->raise();
    connect(library,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *item,int){QString p=item->data(0,Qt::UserRole).toString();if(p.isEmpty())return;if(item->data(0,Qt::UserRole+2)=="template")applyTemplate(p);else openPath(p);});
    library->setContextMenuPolicy(Qt::CustomContextMenu);connect(library,&QWidget::customContextMenuRequested,this,[this](QPoint at){auto *item=library->itemAt(at);if(!item)return;QString path=item->data(0,Qt::UserRole).toString();if(path.isEmpty())return;QMenu menu(this);
        if(item->data(0,Qt::UserRole+2)=="template")menu.addAction(ui("Platinenvorlage übernehmen"),[this,path]{applyTemplate(path);});
        menu.addAction(ui("Datei als Dokument öffnen"),[this,path]{openPath(path);});menu.exec(library->viewport()->mapToGlobal(at));
    });
    // The LochMaster installation: given at start (--assets), else the saved choice ("-" = none), else the usual places.
    {
        const QString language=uiLanguage().toUpper();
        if(!assets.isEmpty())lochMaster=findLochMaster(assets,language);
        else if(!qApp->property("openloch.testing").toBool()){
            const auto saved=QSettings().value("lochmaster/path").toString();
            if(saved!="-"){
                if(!saved.isEmpty())lochMaster=findLochMaster(saved,language);
                if(!lochMaster.valid())for(const auto &place:lochMasterCandidates({QDir::currentPath(),QCoreApplication::applicationDirPath()})){const auto found=findLochMaster(place,language);if(found.valid()){lochMaster=found;break;}}
            }
        }
    }
    // Files copied into or out of the component folder show up without restarting.
    folderWatcher=new QFileSystemWatcher(this);folderRefresh=new QTimer(this);folderRefresh->setSingleShot(true);folderRefresh->setInterval(400);
    connect(folderWatcher,&QFileSystemWatcher::directoryChanged,folderRefresh,qOverload<>(&QTimer::start));
    connect(folderRefresh,&QTimer::timeout,this,[this]{const auto page=libraryPages->currentData().toString();for(auto it=catalog.begin();it!=catalog.end();)it=it.key().startsWith(componentFolder())?catalog.erase(it):std::next(it);
        populateLibrary();const int at=libraryPages->findData(page);if(at>=0)libraryPages->setCurrentIndex(at);});
    prepareOpenLibrary();populateLibrary();
    // Short check results: one entry per short; selecting it shows where the potentials are joined.
    shortsDock=new QDockWidget(ui("Prüfergebnisse"),this);shortsDock->setObjectName("shortsDock");shortList=new QListWidget(shortsDock);shortList->setObjectName("shortResults");shortsDock->setWidget(shortList);addDockWidget(Qt::RightDockWidgetArea,shortsDock);shortsDock->hide();
    connect(shortList,&QListWidget::currentRowChanged,this,[this](int row){if(row>=0&&row<shortFindings.size())canvas->showShort(shortFindings[row].chain);else canvas->clearShort();});
    connect(shortsDock,&QDockWidget::visibilityChanged,this,[this](bool visible){if(!visible&&!shortsDock->isVisible())canvas->clearShort();});
    // Comparison with the schematic: selecting a finding shows its part or place on the board.
    schematicDock=new QDockWidget(ui("Vergleich mit Schaltplan"),this);schematicDock->setObjectName("schematicDock");schematicList=new QListWidget(schematicDock);schematicList->setObjectName("schematicResults");
    schematicDock->setWidget(schematicList);addDockWidget(Qt::RightDockWidgetArea,schematicDock);schematicDock->hide();
    connect(schematicList,&QListWidget::currentRowChanged,this,[this](int row){
        const auto *item=schematicList->item(row);if(!item)return;
        const auto part=item->data(Qt::UserRole+1).toString().split('/');if(part.size()==2)canvas->selectObject(part[0],part[1].toInt());
        if(item->data(Qt::UserRole).isValid())canvas->centerOn(item->data(Qt::UserRole).toPointF());
    });
    coordinates=new QLabel(this);info=new QLabel(this);statusBar()->addWidget(info,1);statusBar()->addPermanentWidget(coordinates);
    canvas->pointerMoved=[this](QPointF p){const int u=canvas->unit();const double unit=u==1?2540:u==2?project.pitch()*100:100;
        coordinates->setText(QString("%1 / %2 %3").arg(p.x()/unit,0,'f',u==0?2:3).arg(p.y()/unit,0,'f',u==0?2:3).arg(u==1?"inch":u==2?"N":"mm"));};
    canvas->beforeChange=[this]{history.begin(project);};
    canvas->changed=[this]{if(history.commit(project)){dirty=true;refresh();if(recoveryTimer)recoveryTimer->start();}};
    canvas->propertiesRequested=[this]{properties();};
    canvas->potentialRequested=[this](QString &name,QColor &colour){colour=lastPotentialColour;if(!editPotential(name,colour))return false;lastPotentialColour=colour;return true;};
    canvas->potentialsComputed=[this](int conflicts){if(conflicts)statusBar()->showMessage(QString(ui("Potenziale: %1 Marke(n) auf einem Netz, das bereits ein anderes Potential trägt")).arg(conflicts),10000);};
    canvas->continuityTraced=[this](int copper,int wires){statusBar()->showMessage(copper?QString(ui("Durchgang: %1 Kupferstücke und %2 Drähte verbunden")).arg(copper).arg(wires):QString(ui("Keine Kupferfläche an dieser Stelle")),10000);};
    canvas->toolChanged=[this](const QString &name){if(drawingGroup)drawingGroup->setExclusive(false);for(auto *a:drawingTools->actions())a->setChecked(a->data().toString()==name);if(drawingGroup)drawingGroup->setExclusive(true);
        // Choosing a drawing tool sets the original's colour and width for it: wires and leads silver 0.4 mm, lines and areas
        // black 0.1 mm, pins white 0.6 mm, texts black on yellow.
        if(name==styledTool)return;styledTool=name;if(project.mode!="board")return;int line=-1,fill=-1;
        if(name=="wire"||name=="lead"){line=7;penWidth=40;}else if(name=="polyline"||name=="rectangle"||name=="ellipse"||name=="polygon"){line=0;penWidth=10;}
        else if(name=="pin"){line=15;penWidth=60;}else if(name=="text"){line=0;fill=11;}else return;
        auto *grid=static_cast<ColourGrid*>(findChild<QWidget*>("colourGrid"));penColour=ColourGrid::colour(line);if(fill>=0)brushColour=ColourGrid::colour(fill);
        if(grid){grid->foreground=line;if(fill>=0)grid->background=fill;grid->update();}showDrawStyle();updateToolDefaults();};
    canvas->setProject(&project);buildTools();refresh();
    // The original's bottom toolbars: Füllen, Fräsen, Zoom and Kontur in the upper row, Farben and Breite in the lower one.
    // Colours, width and filling change the selected objects and the style of new ones (setDrawStyle).
    auto bottomBar=[this](const QString &title,const QString &name){auto *bar=new QToolBar(ui(title),this);bar->setObjectName(name);bar->setIconSize(QSize(16,16));bar->setToolButtonStyle(Qt::ToolButtonIconOnly);addToolBar(Qt::BottomToolBarArea,bar);return bar;};
    auto swatchButton=[](QToolBar *bar,const QString &name,const QString &tip){auto *b=new QToolButton(bar);b->setObjectName(name);b->setToolTip(tip);b->setIconSize(QSize(18,18));b->setAutoRaise(true);bar->addWidget(b);return b;};
    // Kontur: smoothing off, B-spline, chamfer, rounding (size in mm); Fräsen: normal or milled outline. The Kontur menu repeats them.
    auto *outlineMenu=edit->addMenu(ui("Kontur"));
    auto *corner=new QDoubleSpinBox(this);corner->setObjectName("contourSize");corner->setRange(.01,100);corner->setDecimals(2);corner->setValue(1);corner->setSuffix(" mm");corner->setToolTip(ui("Größe von Fasen und Rundungen"));
    QList<QAction*> smoothing,milling;
    for(auto [text,style,icon]:std::initializer_list<std::tuple<QString,int,const char*>>{{ui("Ursprüngliche Kontur"),-1,"contour-normal"},{ui("Kontur mit B-Splines"),0,"contour-spline"},{ui("Kontur mit Fasen"),1,"contour-chamfer"},{ui("Kontur mit Rundungen"),2,"contour-round"}}){
        auto *a=outlineMenu->addAction(openLochIcon(icon),text);const int chosen=style;connect(a,&QAction::triggered,this,[this,corner,chosen]{canvas->setSmoothing(chosen,corner->value()*100);});smoothing.append(a);
    }
    outlineMenu->addSeparator();
    for(auto [text,on,icon]:std::initializer_list<std::tuple<QString,bool,const char*>>{{ui("Normale Kontur"),false,"mill-normal"},{ui("Kontur fräsen"),true,"mill"}}){
        auto *a=outlineMenu->addAction(openLochIcon(icon),text);a->setObjectName(on?"milled":"unmilled");const bool chosen=on;connect(a,&QAction::triggered,this,[this,chosen]{canvas->setMilling(chosen);});milling.append(a);
    }
    // Fill pictures; LM4 embeds BMP, so JPEG, PNG and GIF are converted. In the toolbar the original's text buttons BMP and ROT.
    outlineMenu->addSeparator();auto *picture=outlineMenu->addAction(ui("Bitmapfüllung laden…"));picture->setIconText("BMP");
    connect(picture,&QAction::triggered,this,[this]{
        const auto path=QFileDialog::getOpenFileName(this,ui("Bitmapfüllung laden"),startDirectory(),ui("Bilder (*.bmp *.BMP *.png *.PNG *.gif *.GIF *.jpg *.JPG *.jpeg)"));if(path.isEmpty())return;rememberDirectory(path);
        const QImage image(path);if(image.isNull()){QMessageBox::warning(this,ui("Bitmapfüllung laden"),ui("Das Bild konnte nicht gelesen werden."));return;}
        // PNG and GIF with transparency: the area takes the outline of the opaque part (LM4 stores BMP without transparency).
        const auto fill=pictureFill(image);canvas->setBitmapFill(fill.bmp,fill.outline);
        if(!fill.outline.isEmpty())statusBar()->showMessage(ui("Umriss an die durchsichtigen Stellen des Bildes angepasst"),8000);
    });
    auto *turnPicture=outlineMenu->addAction(ui("Bitmapfüllung drehen"));turnPicture->setIconText("ROT");connect(turnPicture,&QAction::triggered,this,[this]{canvas->rotateBitmapFill();});
    auto *colourBar=bottomBar("Farben","colourBar");auto *colourGrid=new ColourGrid(colourBar);colourGrid->setObjectName("colourGrid");colourBar->addWidget(colourGrid);
    colourGrid->chosen=[this](bool fill,QColor colour){setDrawStyle(fill?3:1,colour);};
    auto *widthBar=bottomBar("Breite","widthBar");
    auto *penSwatch=swatchButton(widthBar,"penSwatch",ui("Stiftfarbe zuweisen"));
    connect(penSwatch,&QToolButton::clicked,this,[this]{const auto c=QColorDialog::getColor(penColour,this,ui("Stiftfarbe"));if(c.isValid())setDrawStyle(1,c);});
    auto *lineWidth=new QComboBox(widthBar);lineWidth->setObjectName("lineWidth");lineWidth->setToolTip(ui("Breite zuweisen"));lineWidth->addItem(ui("unsichtbar"),0);
    for(const int w:{10,20,30,40,50,60,70,80,90,100,150,200})lineWidth->addItem(uiLocale().toString(w/100.0,'f',1)+" mm",w);
    widthBar->addWidget(lineWidth);connect(lineWidth,&QComboBox::activated,this,[this,lineWidth](int index){setDrawStyle(0,lineWidth->itemData(index).toInt());});
    // Rows added later sit nearer the drawing area: Farben and Breite go first so that they end up in the lower row.
    addToolBarBreak(Qt::BottomToolBarArea);
    auto *fillBar=bottomBar("Füllen","fillBar");
    auto *brushSwatch=swatchButton(fillBar,"brushSwatch",ui("Farbe für Füllungen zuweisen"));
    connect(brushSwatch,&QToolButton::clicked,this,[this]{const auto c=QColorDialog::getColor(brushColour,this,ui("Füllfarbe"));if(c.isValid())setDrawStyle(3,c);});
    auto *fillToggle=fillBar->addAction(openLochIcon("fill"),ui("Flächen füllen"));fillToggle->setObjectName("fillAreas");fillToggle->setCheckable(true);fillToggle->setChecked(true);
    connect(fillToggle,&QAction::triggered,this,[this](bool on){setDrawStyle(2,on);});fillBar->addAction(picture);fillBar->addAction(turnPicture);
    bottomBar("Fräsen","millBar")->addActions(milling);
    iconBar("Zoom","zoomBar",Qt::BottomToolBarArea,{{"Vergrößern","zoom-in"},{"Verkleinern","zoom-out"},{"&Einpassen","zoom-board"},{"Zoom Objekte","zoom-all"},{"Markierte Objekte zoomen","zoom-marked"},{"Reale Größe 1:1","zoom-real"}});
    auto *outline=bottomBar("Kontur","outlineBar");outline->addActions(smoothing);outline->addWidget(corner);
    showDrawStyle();
    // Ansicht → Werkzeuge anzeigen: each toolbar on and off, in alphabetical order as in the original, or all at once.
    {   auto *bars=view->addMenu(ui("Werkzeuge anzeigen"));QList<QToolBar*> all;for(auto *t:findChildren<QToolBar*>())if(t->parent()==this)all.append(t);
        std::sort(all.begin(),all.end(),[](QToolBar *a,QToolBar *b){return QString::localeAwareCompare(a->windowTitle(),b->windowTitle())<0;});
        for(auto *t:all)bars->addAction(t->toggleViewAction());bars->addSeparator();action(bars,ui("Alle anzeigen"),{},[all]{for(auto *t:all)t->show();})->setObjectName("showAllToolbars");
    }
    recoveryFile=recoveryFolder()+'/'+QUuid::createUuid().toString(QUuid::WithoutBraces)+".openloch";
    // AutoSpeichern as in the original: on, every 10 minutes, kept in the settings.
    autoSaveTimer=new QTimer(this);autoSaveTimer->setObjectName("autoSaveTimer");connect(autoSaveTimer,&QTimer::timeout,this,[this]{saveBackup();});
    if(!qApp->property("openloch.testing").toBool()){autoSaveEnabled=QSettings().value("backup/active",true).toBool();autoSaveMinutes=qBound(1,QSettings().value("backup/interval",10).toInt(),60);}
    restartAutoSave();
    recoveryTimer=new QTimer(this);recoveryTimer->setObjectName("recoveryTimer");recoveryTimer->setSingleShot(true);recoveryTimer->setInterval(10000);connect(recoveryTimer,&QTimer::timeout,this,&Window::saveRecovery);
}
QString Window::recoveryFolder(){
    const QString root=qApp->property("openloch.recoveryDirectory").toString();
    return root.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/recovery":root;
}
// 0 add, 1 duplicate, 2 remove, 3 switch to `index`, 4/5/6 turn 90° right/left/180°, 7 move to `index`, 8 rename, 9 append boards of `other`.
void Window::changeBoard(int operation,int index,const QString &name,const Project *other){
    flushNotes();if(canvas->viewState().flip&&(operation==4||operation==5))operation=9-operation; // "rechts" stays clockwise on screen when turned over
    keepMainView();
    try{auto next=project;
        if(operation==0)next.addBoard();else if(operation==1)next.addBoard(true);else if(operation==2)next.removeBoard();
        else if(operation>=4&&operation<=6){for(int turns=operation==4?1:operation==5?3:2;turns>0;--turns)next.rotateBoard();}
        else if(operation==7){if(index<0||index>=next.boards.size())return;next.moveBoard(index);}
        else if(operation==8)next.title=name;else if(operation==9&&other)next.appendBoards(*other);else next.switchBoard(index);
        Project::decode(next.encode());canvas->beforeChange();project=std::move(next);canvas->setProject(&project);applyStoredView();buildTools();canvas->changed();refresh();
    }catch(const std::exception &e){QMessageBox::warning(this,ui("Platine konnte nicht geändert werden"),QString::fromUtf8(e.what()));}
}
// Name and colour of a potential marker; LochMaster ignores black markers, so black is not offered.
bool Window::editPotential(QString &name,QColor &colour){
    QDialog dialog(this);dialog.setWindowTitle(ui("Potenzial"));QFormLayout layout(&dialog);QLineEdit text(name);text.setPlaceholderText(ui("z. B. VCC"));layout.addRow(ui("Name"),&text);
    QPushButton choose(colour.name());layout.addRow(ui("Farbe"),&choose);
    connect(&choose,&QPushButton::clicked,&dialog,[&]{auto chosen=QColorDialog::getColor(colour,&dialog,ui("Potentialfarbe wählen"));if(chosen.isValid()&&chosen.rgb()!=QColor(Qt::black).rgb()){colour=chosen;choose.setText(colour.name());}});
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return false;name=text.text();return true;
}
// The original's notes editor (TEditorForm): a RichEdit with file, clipboard, font, size, bold/italic/underline,
// alignment and bullet buttons, plus "Stückliste einfügen" and "Einkaufsliste einfügen" at the caret.
// The original editor's ruler: a centimetre scale over the text with the paragraph's indents, the first line (triangle at
// the top), the other lines (triangle at the bottom) and the right edge. Dragging a marker changes the marked paragraphs.
class NotesRuler final:public QWidget {
public:
    QTextEdit *text;
    explicit NotesRuler(QTextEdit *edit,QWidget *parent):QWidget(parent),text(edit){
        setObjectName("notesRuler");setFixedHeight(24);setToolTip(ui("Einzüge: erste Zeile (oben), weitere Zeilen (unten), rechts"));
        connect(text,&QTextEdit::cursorPositionChanged,this,qOverload<>(&QWidget::update));connect(text->horizontalScrollBar(),&QScrollBar::valueChanged,this,qOverload<>(&QWidget::update));
    }
    // 0 first line, 1 left, 2 right: the marker's position in the ruler's pixels.
    double markerX(int which) const{
        const auto f=text->textCursor().blockFormat();if(which==2)return start()+text->viewport()->width()-2*margin()-f.rightMargin();
        return start()+f.leftMargin()+(which==0?f.textIndent():0);
    }
    // The marked paragraphs get the indents of the three markers (in pixels of the ruler).
    void apply(double first,double left,double right){
        QTextBlockFormat f;f.setLeftMargin(qMax(0.0,left-start()));f.setTextIndent(first-left);f.setRightMargin(qMax(0.0,start()+text->viewport()->width()-2*margin()-right));
        auto c=text->textCursor();c.mergeBlockFormat(f);text->setTextCursor(c);update();
    }
protected:
    double margin() const{return text->document()->documentMargin();}
    double start() const{return text->viewport()->x()+margin()-text->horizontalScrollBar()->value();}
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);p.fillRect(rect(),palette().window());const double cm=logicalDpiX()/2.54;p.setPen(palette().color(QPalette::Mid));
        p.fillRect(QRectF(start(),4,text->viewport()->width()-2*margin(),14),palette().base());QFont f=font();f.setPixelSize(9);p.setFont(f);
        for(int i=0;start()+i*cm/4<width();i++){const double x=start()+i*cm/4;if(i%4==0){if(i)p.drawText(QRectF(x-10,4,20,14),Qt::AlignCenter,QString::number(i/4));}else p.drawLine(QPointF(x,i%2?10:8),QPointF(x,i%2?12:14));}
        p.setPen(palette().color(QPalette::Text));p.setBrush(palette().color(QPalette::Button));
        const double first=dragging==0?dragX:markerX(0),left=dragging==1?dragX:markerX(1),right=dragging==2?dragX:markerX(2);
        p.drawPolygon(QPolygonF{{first-4,2},{first+4,2},{first,8}});p.drawPolygon(QPolygonF{{left-4,22},{left+4,22},{left,16}});p.drawPolygon(QPolygonF{{right-4,22},{right+4,22},{right,16}});
    }
    void mousePressEvent(QMouseEvent *e) override{
        const double x=e->position().x();dragging=-1;double best=7;
        for(int which:{0,1,2}){const bool upper=which==0;if((e->position().y()<12)!=upper&&which!=2)continue;const double d=std::abs(markerX(which)-x);if(d<best){best=d;dragging=which;}}
        dragX=x;update();
    }
    void mouseMoveEvent(QMouseEvent *e) override{if(dragging>=0){dragX=e->position().x();update();}}
    void mouseReleaseEvent(QMouseEvent *e) override{
        if(dragging<0)return;dragX=e->position().x();double first=markerX(0),left=markerX(1),right=markerX(2);
        if(dragging==0)first=dragX;else if(dragging==1)left=dragX;else right=dragX;dragging=-1;apply(first,left,right);
    }
private:
    int dragging=-1;double dragX=0;
};
class NotesPanel : public QWidget {
public:
    QTextEdit *text;QString file;std::function<QList<NoteLine>(int)> listSource;
    explicit NotesPanel(QWidget *parent):QWidget(parent){
        auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
        auto *bar=new QToolBar(this);bar->setObjectName("notesToolbar");layout->addWidget(bar);auto *format=new QToolBar(this);format->setObjectName("notesFormat");layout->addWidget(format);
        text=new QTextEdit(this);text->setObjectName("notesText");text->setAcceptRichText(true);text->document()->setDefaultFont(QFont("Arial",10));
        layout->addWidget(new NotesRuler(text,this));layout->addWidget(text);
        auto add=[&](QToolBar *t,const QString &name,QKeySequence key,auto fn){auto *a=t->addAction(name);a->setShortcut(key);connect(a,&QAction::triggered,this,fn);return a;};
        add(bar,ui("Neu"),{},[this]{text->clear();file.clear();});
        add(bar,ui("Öffnen…"),{},[this]{
            const auto path=QFileDialog::getOpenFileName(this,ui("Öffnen"),file.isEmpty()?QDir::homePath():QFileInfo(file).absolutePath(),ui("Rich Text Files (*.rtf *.RTF);;Text Files (*.txt *.TXT)"));if(path.isEmpty())return;
            QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>4000000){QMessageBox::warning(this,ui("Öffnen fehlgeschlagen"),f.errorString());return;}
            const auto bytes=f.readAll();if(bytes.startsWith("{\\rtf"))readRtf(bytes,*text->document());else text->setPlainText(plainNotes(bytes));file=path;
        });
        // Speichern writes to the file of the notes, Speichern unter always asks, as in the original's editor.
        auto save=[this](bool asNew){
            QString path=asNew?QString():file;if(path.isEmpty())path=QFileDialog::getSaveFileName(this,ui("Speichern unter"),file.isEmpty()?QDir::homePath():file,ui("Rich Text Files (*.RTF);;Text Files (*.TXT)"));if(path.isEmpty())return;
            auto bytes=QFileInfo(path).suffix().toLower()=="txt"?text->toPlainText().toUtf8():writeRtf(*text->document());if(bytes.endsWith('\0'))bytes.chop(1);
            QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit()){QMessageBox::warning(this,ui("Speichern fehlgeschlagen"),f.errorString());return;}file=path;
        };
        add(bar,ui("Speichern…"),{},[save]{save(false);});
        add(bar,ui("Speichern unter…"),{},[save]{save(true);})->setObjectName("notesSaveAs");
        add(bar,ui("Drucken…"),{},[this]{QPrinter printer(QPrinter::HighResolution);QPrintDialog dialog(&printer,this);if(dialog.exec()==QDialog::Accepted)text->print(&printer);});
        bar->addSeparator();
        add(bar,ui("Rückgängig"),{},[this]{text->undo();});add(bar,ui("Ausschneiden"),{},[this]{text->cut();});add(bar,ui("Kopieren"),{},[this]{text->copy();});add(bar,ui("Einfügen"),{},[this]{text->paste();});
        bar->addSeparator();
        add(bar,ui("Stückliste einfügen"),{},[this]{insertList(0);})->setObjectName("insertPartsList");add(bar,ui("Einkaufsliste einfügen"),{},[this]{insertList(1);})->setObjectName("insertOrderList");
        auto *font=new QFontComboBox(format);font->setObjectName("notesFont");font->setCurrentFont(QFont("Arial"));format->addWidget(font);
        auto *size=new QSpinBox(format);size->setObjectName("notesSize");size->setRange(1,200);size->setValue(10);format->addWidget(size);
        auto merge=[this](const QTextCharFormat &f){auto c=text->textCursor();if(c.hasSelection())c.mergeCharFormat(f);text->mergeCurrentCharFormat(f);};
        connect(font,&QFontComboBox::currentFontChanged,this,[merge](const QFont &chosen){QTextCharFormat f;f.setFontFamilies(QStringList{chosen.family()});merge(f);});
        connect(size,&QSpinBox::valueChanged,this,[merge](int points){QTextCharFormat f;f.setFontPointSize(points);merge(f);});
        auto *bold=add(format,ui("Fett"),QKeySequence::Bold,[merge](bool on){QTextCharFormat f;f.setFontWeight(on?QFont::Bold:QFont::Normal);merge(f);});
        auto *italic=add(format,ui("Kursiv"),QKeySequence::Italic,[merge](bool on){QTextCharFormat f;f.setFontItalic(on);merge(f);});
        auto *underline=add(format,ui("Unterstrichen"),QKeySequence::Underline,[merge](bool on){QTextCharFormat f;f.setFontUnderline(on);merge(f);});
        for(auto *a:{bold,italic,underline})a->setCheckable(true);
        format->addSeparator();auto *alignment=new QActionGroup(this);QList<QAction*> aligns;
        for(auto [name,flag]:std::initializer_list<std::pair<QString,Qt::Alignment>>{{ui("Links"),Qt::AlignLeft|Qt::AlignAbsolute},{ui("Zentriert"),Qt::AlignHCenter},{ui("Rechts"),Qt::AlignRight|Qt::AlignAbsolute}}){
            const Qt::Alignment chosen=flag;auto *a=add(format,name,{},[this,chosen]{text->setAlignment(chosen);});a->setCheckable(true);alignment->addAction(a);aligns.append(a);
        }
        auto *bullets=add(format,ui("Aufzählung"),{},[this](bool on){
            auto c=text->textCursor();c.beginEditBlock();
            if(on)c.createList(QTextListFormat::ListDisc);
            else for(auto block=text->document()->findBlock(c.selectionStart());block.isValid()&&block.position()<=c.selectionEnd();block=block.next())if(auto *l=block.textList()){l->remove(block);QTextCursor b(block);auto f=b.blockFormat();f.setIndent(0);b.setBlockFormat(f);}
            c.endEditBlock();
        });bullets->setCheckable(true);
        // The buttons follow the text at the caret.
        auto follow=[=,this]{
            const auto f=text->currentCharFormat();QSignalBlocker a(font),b(size);
            const auto families=f.fontFamilies().toStringList();font->setCurrentFont(QFont(families.isEmpty()?text->document()->defaultFont().family():families.first()));
            size->setValue(qRound(f.fontPointSize()>0?f.fontPointSize():text->document()->defaultFont().pointSizeF()));
            bold->setChecked(f.fontWeight()>=QFont::DemiBold);italic->setChecked(f.fontItalic());underline->setChecked(f.fontUnderline());
            const auto align=text->alignment();aligns[align&Qt::AlignHCenter?1:align&Qt::AlignRight?2:0]->setChecked(true);bullets->setChecked(text->textCursor().currentList()!=nullptr);
        };
        connect(text,&QTextEdit::cursorPositionChanged,this,follow);connect(text,&QTextEdit::currentCharFormatChanged,this,follow);
    }
    void load(const QByteArray &rtf){readRtf(rtf,*text->document());text->document()->setModified(false);text->moveCursor(QTextCursor::Start);}
    // Inserted at the caret, replacing a selection, like the original's paste from a hidden RichEdit; then back to the top.
    void insertList(int mode){
        if(!listSource)return;const auto lines=listSource(mode);auto c=text->textCursor();c.beginEditBlock();
        for(const auto &line:lines){QTextCharFormat f;f.setFontFamilies(QStringList{text->document()->defaultFont().family()});f.setFontPointSize(line.size);f.setFontWeight(line.bold?QFont::Bold:QFont::Normal);c.insertText(line.text,f);c.insertBlock();}
        c.endEditBlock();text->setTextCursor(c);text->verticalScrollBar()->setValue(0);
    }
};
QString ownerName(){
#ifdef Q_OS_WIN
    QSettings registry("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",QSettings::NativeFormat);
    return (registry.value("RegisteredOwner").toString()+" "+registry.value("RegisteredOrganization").toString()).trimmed();
#else
    if(const auto *entry=getpwuid(getuid());entry&&entry->pw_gecos&&*entry->pw_gecos)return QString::fromLocal8Bit(entry->pw_gecos).section(',',0,0).trimmed();
    return qEnvironmentVariable("USER");
#endif
}
QList<NoteLine> Window::partsList(int mode) const{return partsListNotes(project,mode,QFileInfo(currentPath).fileName(),QDateTime::currentDateTime(),ownerName());}
void Window::showNotes(){
    if(!notesWindow){
        notesWindow=new QDialog(this);notesWindow->setObjectName("notesWindow");auto *layout=new QVBoxLayout(notesWindow);layout->setContentsMargins(0,0,0,0);
        notesPanel=new NotesPanel(notesWindow);notesPanel->setObjectName("notesPanel");layout->addWidget(notesPanel);notesWindow->resize(680,500);
        notesPanel->listSource=[this](int mode){return partsList(mode);};
        notesTimer=new QTimer(notesWindow);notesTimer->setSingleShot(true);notesTimer->setInterval(2000);connect(notesTimer,&QTimer::timeout,this,[this]{flushNotes();});
        connect(notesPanel->text->document(),&QTextDocument::contentsChanged,notesWindow,[this]{if(notesLoading)return;notesPending=true;notesTimer->start();});
        connect(notesWindow,&QDialog::finished,this,[this]{flushNotes();});
        connect(qApp,&QApplication::focusChanged,notesWindow,[this](QWidget *old,QWidget *){if(notesPanel&&old==notesPanel->text)flushNotes();});
    }
    loadNotesEditor();notesWindow->show();notesWindow->raise();notesWindow->activateWindow();
}
void Window::loadNotesEditor(){
    if(!notesPanel)return;notesLoading=true;notesShown=project.notesRtf.isEmpty()?rtfNotes(project.notes):project.notesRtf;notesPanel->load(notesShown);notesPending=false;notesLoading=false;
    notesWindow->setWindowTitle(ui("Anmerkungen – ")+project.title);
}
void Window::flushNotes(){
    if(!notesPanel||!notesPending)return;notesPending=false;if(notesTimer)notesTimer->stop();
    const auto bytes=writeRtf(*notesPanel->text->document());if(bytes==notesShown)return;
    canvas->beforeChange();project.notesRtf=bytes;project.notes=notesPanel->text->toPlainText();project.notesEdited=true;notesShown=bytes;canvas->changed();
}
void Window::showPartsList(){
    QDialog dialog(this);dialog.setObjectName("partsListWindow");dialog.setWindowTitle(ui("Bauteilliste"));auto *layout=new QVBoxLayout(&dialog);layout->setContentsMargins(0,0,0,0);
    auto *panel=new NotesPanel(&dialog);panel->listSource=[this](int mode){return partsList(mode);};layout->addWidget(panel);dialog.resize(680,500);
    panel->insertList(0);panel->text->document()->setModified(false);dialog.exec();
}
// The original's "HPGL-Bearbeitungsdateien exportieren": jobs of a copy of the board, options, preview and output
// folder; "Ändern…" changes tool diameters of that copy only. One .PLT file per job, then the folder is shown.
void Window::exportHpgl(){
    if(project.mode!="board")return;PlotBoard board;
    try{board=plotBoard(project);}catch(const std::exception &e){QMessageBox::warning(this,"HPGL",QString::fromUtf8(e.what()));return;}
    QDialog dialog(this);dialog.setObjectName("hpglDialog");dialog.setWindowTitle(ui("HPGL-Bearbeitungsdateien exportieren..."));dialog.setMinimumSize(588,455);
    auto *columns=new QHBoxLayout(&dialog);auto *tree=new QTreeWidget(&dialog);tree->setObjectName("hpglJobs");tree->setHeaderLabel(ui("Jobs"));tree->setContextMenuPolicy(Qt::CustomContextMenu);tree->setMinimumWidth(200);columns->addWidget(tree);
    auto *right=new QVBoxLayout;columns->addLayout(right,1);auto *preview=new QLabel(&dialog);preview->setObjectName("hpglPreview");preview->setMinimumSize(320,220);preview->setAlignment(Qt::AlignCenter);preview->setToolTip(ui("Vorschau"));right->addWidget(preview,1);
    auto box=[&](const QString &text,const char *name,bool checked){auto *c=new QCheckBox(text,&dialog);c->setObjectName(name);c->setChecked(checked);return c;};
    auto *drills=box(ui("&Bohrungen"),"hpglDrills",hpglOptions.drills),*mills=box(ui("&Fräsungen"),"hpglMills",hpglOptions.mills),*cuts=box(ui("&Trennstellen"),"hpglCuts",hpglOptions.cuts),*outline=box(ui("Aussenrechteck"),"hpglOutline",hpglOptions.outline);
    auto *millDrills=box(ui("Bohrungen ausfräsen mit:"),"hpglMillDrills",hpglOptions.millDrills),*origin=box(ui("Gemeinsamer Ursprung"),"hpglOrigin",hpglOptions.commonOrigin),*layer2=box(ui("Bohren/Fräsen auf Layer &2"),"hpglLayer2",hpglOptions.layer2);
    auto *tool=new QDoubleSpinBox(&dialog);tool->setObjectName("hpglTool");tool->setDecimals(2);tool->setSingleStep(.05);tool->setRange(0,10);tool->setValue(hpglOptions.tool);tool->setSuffix(" mm");
    auto *grid=new QGridLayout;grid->addWidget(drills,0,0);grid->addWidget(mills,1,0);grid->addWidget(cuts,2,0);grid->addWidget(outline,3,0);
    auto *milling=new QHBoxLayout;milling->addWidget(millDrills);milling->addWidget(tool);milling->addStretch();grid->addLayout(milling,0,1);grid->addWidget(origin,1,1);grid->addWidget(layer2,2,1);right->addLayout(grid);
    right->addWidget(new QLabel(ui("Ausgabeverzeichnis für Plotdateien:"),&dialog));auto *row=new QHBoxLayout;auto *path=new QLineEdit(&dialog);path->setObjectName("hpglPath");row->addWidget(path,1);
    auto *browse=new QToolButton(&dialog);browse->setText("…");row->addWidget(browse);right->addLayout(row);
    QString folder=currentPath.isEmpty()?startDirectory():QFileInfo(currentPath).absolutePath();path->setText(QDir(folder).filePath((ui("Plotdateien_")+project.title).toLower())+"/");
    connect(browse,&QToolButton::clicked,&dialog,[&]{const auto chosen=QFileDialog::getExistingDirectory(&dialog,ui("Ausgabeverzeichnis"),path->text());if(!chosen.isEmpty())path->setText(chosen+"/");});
    auto *buttons=new QDialogButtonBox(&dialog);auto *exporting=buttons->addButton(ui("&Exportieren"),QDialogButtonBox::AcceptRole);exporting->setObjectName("hpglExport");buttons->addButton(ui("&Abbrechen"),QDialogButtonBox::RejectRole);
    right->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    bool hasDrills=false,hasMills=false,hasCuts=false;
    for(const auto &o:board.objects){const auto type=o["type"].toString();hasDrills|=type=="TBohrung";hasCuts|=type.startsWith("TTrenner");hasMills|=type!="TBohrung"&&!type.startsWith("TTrenner")&&o["flag3"].toBool();}
    drills->setEnabled(hasDrills);mills->setEnabled(hasMills);cuts->setEnabled(hasCuts);
    QList<PlotJob> jobs;PlotOptions options=hpglOptions;
    auto draw=[&]{
        QPixmap picture(preview->size().expandedTo(QSize(320,220)));picture.fill(QColor("#f5f5f2"));QPainter p(&picture);p.setRenderHint(QPainter::Antialiasing);
        const QRectF area(-300,-300,board.width+600,board.height+600);const double scale=std::min(picture.width()/area.width(),picture.height()/area.height());
        p.translate(picture.width()/2.0,picture.height()/2.0);p.scale(scale,scale);p.translate(-area.center());
        p.setPen(QPen(QColor("#9a9a90"),0));p.setBrush(QColor("#e4dcc0"));p.drawRect(QRectF(0,0,board.width,board.height));p.setBrush(Qt::NoBrush);
        QSet<int> marked;bool outlineMarked=false;for(auto *item:tree->selectedItems()){auto *top=item->parent()?item->parent():item;const int j=top->data(0,Qt::UserRole).toInt();if(j<0||j>=jobs.size())continue;if(item->parent())marked.insert(item->data(0,Qt::UserRole+1).toInt());else{marked.unite(QSet<int>(jobs[j].objects.begin(),jobs[j].objects.end()));outlineMarked|=jobs[j].type==PlotJob::Outline;}}
        for(const auto &job:jobs){
            if(job.type==PlotJob::Outline){p.setPen(QPen(outlineMarked?Qt::red:QColor("#2a62c9"),0));QPolygon line;for(auto q:plotOutline(board,options.outlineWidth))line<<q;p.drawPolyline(line);continue;}
            for(int i:job.objects){
                p.setPen(QPen(marked.contains(i)?Qt::red:job.type==PlotJob::Cut?QColor("#b5651d"):QColor("#2a62c9"),0));const auto &o=board.objects[i];
                if(job.type==PlotJob::Drill){const auto c=o["center"].toArray();const double r=std::max(o["diameter"].toDouble()*50,10.0);p.drawEllipse(QPointF(c[0].toDouble(),c[1].toDouble()),r,r);}
                else if(job.type==PlotJob::Cut){const auto r=o["rect"].toArray();p.drawRect(QRectF(QPointF(r[0].toDouble(),r[1].toDouble()),QPointF(r[2].toDouble(),r[3].toDouble())));}
                else{QPolygon line;for(auto q:job.type==PlotJob::MillDrill?plotDrillCircle(o,options.tool):plotPath(o))line<<q;p.drawPolyline(line);}
            }
        }
        p.end();preview->setPixmap(picture);
    };
    auto rebuild=[&]{
        options.drills=drills->isChecked()&&drills->isEnabled();options.mills=mills->isChecked()&&mills->isEnabled();options.cuts=cuts->isChecked()&&cuts->isEnabled();options.outline=outline->isChecked();
        millDrills->setEnabled(drills->isEnabled()&&drills->isChecked());tool->setEnabled(millDrills->isEnabled()&&millDrills->isChecked());
        options.millDrills=millDrills->isEnabled()&&millDrills->isChecked();options.commonOrigin=origin->isChecked();options.layer2=layer2->isChecked();
        options.tool=float(tool->value());jobs=plotJobs(board,options);{QSignalBlocker quiet(tool);tool->setValue(options.tool);}
        QSignalBlocker quiet(tree);tree->clear();
        static const QMap<QString,const char*> names{{"TBohrung","Bohrung"},{"TTrenner","Trenner"},{"TTrennerFest","Trenner"},{"TKreis","Kreis"},{"TDraht","Linie"}}; // translated where used
        for(int j=0;j<jobs.size();j++){
            auto *node=new QTreeWidgetItem(tree,{jobs[j].name});node->setData(0,Qt::UserRole,j);
            if(jobs[j].type==PlotJob::Outline)new QTreeWidgetItem(node,{ui("Aussenrechteck")});
            for(int i:jobs[j].objects){const auto &o=board.objects[i];const auto label=o["label"].toString();auto *child=new QTreeWidgetItem(node,{label.isEmpty()?(names.contains(o["type"].toString())?ui(names.value(o["type"].toString())):o["type"].toString()):label});child->setData(0,Qt::UserRole+1,i);}
            node->setExpanded(true);
        }
        exporting->setEnabled(!jobs.isEmpty());draw();
    };
    for(auto *c:{drills,mills,cuts,outline,millDrills,origin,layer2})connect(c,&QCheckBox::toggled,&dialog,rebuild);
    connect(tool,&QDoubleSpinBox::valueChanged,&dialog,rebuild);connect(tree,&QTreeWidget::itemSelectionChanged,&dialog,draw);
    // "Ändern…": the tool of an object or of all objects of a job (not for cuts), in mm with up to four digits.
    connect(tree,&QTreeWidget::customContextMenuRequested,&dialog,[&](QPoint at){
        auto *item=tree->itemAt(at);if(!item)return;auto *top=item->parent()?item->parent():item;const int j=top->data(0,Qt::UserRole).toInt();if(j<0||j>=jobs.size()||jobs[j].type==PlotJob::Cut)return;
        QMenu menu;auto *change=menu.addAction(ui("Ä&ndern..."));if(menu.exec(tree->viewport()->mapToGlobal(at))!=change)return;
        const bool single=item->parent()!=nullptr;const int index=single?item->data(0,Qt::UserRole+1).toInt():-1;
        double value=jobs[j].type==PlotJob::Outline?options.outlineWidth:single?(board.objects[index]["type"]=="TBohrung"?board.objects[index]["diameter"].toDouble()*100:board.objects[index]["width"].toDouble()):jobs[j].tool*100;
        bool ok=false;const auto text=QInputDialog::getText(&dialog,ui("Werkzeug"),ui("Durchmesser [mm]"),QLineEdit::Normal,QString::number(value/100,'g',4).replace('.',','),&ok);if(!ok)return;
        const double parsed=text.trimmed().replace(',','.').toDouble(&ok);if(!ok||parsed<0)return;const int v=int(std::nearbyint(parsed*100));
        auto apply=[&](int i){auto &o=board.objects[i];if(o["type"]=="TBohrung")o["diameter"]=v/100.0;else o["width"]=v;};
        if(jobs[j].type==PlotJob::Outline)options.outlineWidth=hpglOptions.outlineWidth=v;else if(jobs[j].type==PlotJob::MillDrill&&!single){QSignalBlocker quiet(tool);tool->setValue(v/100.0);}
        else if(single)apply(index);else for(int i:jobs[j].objects)apply(i);
        rebuild();
    });
    rebuild();
    if(dialog.exec()!=QDialog::Accepted)return;
    hpglOptions.drills=drills->isChecked();hpglOptions.mills=mills->isChecked();hpglOptions.cuts=cuts->isChecked();hpglOptions.outline=outline->isChecked();hpglOptions.millDrills=millDrills->isChecked();
    hpglOptions.commonOrigin=origin->isChecked();hpglOptions.layer2=layer2->isChecked();hpglOptions.tool=float(tool->value());
    QString target=path->text().trimmed();if(target.isEmpty())return;if(!target.endsWith('/'))target+='/';
    if(QDir(target).exists()){
        if(QMessageBox::question(this,"HPGL",ui("Das Zielverzeichnis existiert schon. Darin enthaltene Dateien werden überschrieben.\n\n")+QDir::toNativeSeparators(target)+ui("\n\nMöchten Sie das Projekt trotzdem exportieren?"),QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return;
    }else if(!QDir().mkpath(target)){QMessageBox::critical(this,"HPGL",ui("Zielverzeichnis konnte nicht angelegt werden."));return;}
    rememberDirectory(target+"x");QStringList failed;
    for(const auto &job:jobs){QSaveFile f(target+job.name+".PLT");const auto bytes=plotFile(job,board,options);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())failed.append(job.name+".PLT");}
    if(!failed.isEmpty()){QMessageBox::warning(this,"HPGL",ui("Nicht geschrieben:\n")+failed.join('\n'));return;}
    statusBar()->showMessage(QString(ui("%1 Plotdatei(en) geschrieben: %2")).arg(jobs.size()).arg(QDir::toNativeSeparators(target)),8000);
    if(!qApp->property("openloch.testing").toBool())QDesktopServices::openUrl(QUrl::fromLocalFile(target));
}
// What was found, for the dialog and the status bar.
QString Window::lochMasterSummary() const{
    if(lochMaster.root.isEmpty())return ui("Noch keine LochMaster-Installation eingebunden.");
    return ui("✓ Eingebunden: %1\n%2 Bibliotheksseiten · %3 Platinenvorlagen · %4 Beispielprojekte").arg(QDir::toNativeSeparators(lochMaster.root)).arg(lochMaster.pages).arg(lochMaster.templateCount).arg(lochMaster.projectCount);
}
// Bibliothek → LochMaster-Bibliotheken einbinden: what to choose, the usual places, an automatic search and what was found.
void Window::chooseLochMaster(){
    QDialog dialog(this);dialog.setObjectName("lochmasterDialog");dialog.setWindowTitle(ui("LochMaster-Bibliotheken einbinden"));auto *layout=new QVBoxLayout(&dialog);
    auto *help=new QLabel(ui("OpenLoch verwendet die Bauteilbibliotheken, Platinenvorlagen und Beispielprojekte einer vorhandenen LochMaster-4-Installation. Sie werden nur gelesen, nie verändert.<br><br>Wählen Sie das Programm <b>LochMaster40.exe</b> oder den Ordner, in dem die <b>.LIB-Dateien</b> liegen. Üblich sind:<ul><li>Windows: C:\\Program Files (x86)\\LochMaster40\\LochMaster40.exe (die Bibliotheken liegen unter C:\\ProgramData\\LochMaster40\\DE\\LIB)</li><li>macOS mit CrossOver: ~/Library/Application Support/CrossOver/Bottles/&lt;Flasche&gt;/drive_c/Program Files (x86)/LochMaster40/LochMaster40.exe (im Dateidialog mit Cmd+Umschalt+G den Pfad eingeben)</li><li>Linux mit Wine: ~/.wine/drive_c/Program Files (x86)/LochMaster40/LochMaster40.exe</li></ul>„Automatisch suchen“ prüft diese Orte."),&dialog);
    help->setWordWrap(true);help->setTextFormat(Qt::RichText);layout->addWidget(help);
    auto *status=new QLabel(lochMasterSummary(),&dialog);status->setObjectName("lochmasterStatus");status->setWordWrap(true);status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status->setStyleSheet("padding:8px;border:1px solid palette(mid);");layout->addWidget(status);
    auto *buttons=new QHBoxLayout;layout->addLayout(buttons);
    auto *exe=new QPushButton(ui("LochMaster40.exe wählen…"),&dialog);exe->setObjectName("lochmasterExe");auto *folder=new QPushButton(ui("Ordner mit .LIB-Dateien wählen…"),&dialog);folder->setObjectName("lochmasterFolder");
    auto *search=new QPushButton(ui("Automatisch suchen"),&dialog);search->setObjectName("lochmasterSearch");auto *forget=new QPushButton(ui("Nicht mehr verwenden"),&dialog);forget->setObjectName("lochmasterForget");
    for(auto *b:{exe,folder,search,forget})buttons->addWidget(b);
    auto *close=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    const QString language=uiLanguage().toUpper();
    // A choice takes effect at once and is remembered; one without libraries is reported and changes nothing.
    auto use=[&,language](const QString &chosen){
        const auto found=findLochMaster(chosen,language);
        if(!found.valid()){status->setText(ui("✗ In „%1“ wurde keine LochMaster-Installation gefunden: weder LochMaster40.exe mit den Bibliotheken unter ProgramData noch .LIB-Dateien. Bitte LochMaster40.exe oder den Ordner mit den .LIB-Dateien wählen.").arg(QDir::toNativeSeparators(chosen)));return false;}
        lochMaster=found;assets=chosen;if(!qApp->property("openloch.testing").toBool())QSettings().setValue("lochmaster/path",chosen);
        populateLibrary();status->setText(lochMasterSummary());statusBar()->showMessage(lochMasterSummary().section('\n',0,0),6000);return true;
    };
    const QString from=lochMaster.root.isEmpty()?QDir::homePath():lochMaster.root;
    connect(exe,&QPushButton::clicked,&dialog,[&]{const auto p=QFileDialog::getOpenFileName(&dialog,ui("LochMaster40.exe wählen"),from,ui("LochMaster (LochMaster40.exe);;Programme (*.exe);;Alle Dateien (*)"));if(!p.isEmpty())use(p);});
    connect(folder,&QPushButton::clicked,&dialog,[&]{const auto p=QFileDialog::getExistingDirectory(&dialog,ui("Ordner mit .LIB-Dateien wählen"),from);if(!p.isEmpty())use(p);});
    connect(search,&QPushButton::clicked,&dialog,[&]{
        for(const auto &place:lochMasterCandidates({QDir::currentPath(),QCoreApplication::applicationDirPath()}))if(findLochMaster(place,language).valid()){use(place);return;}
        status->setText(ui("✗ An den üblichen Orten wurde keine LochMaster-Installation gefunden. Bitte LochMaster40.exe oder den Ordner mit den .LIB-Dateien wählen."));
    });
    connect(forget,&QPushButton::clicked,&dialog,[&]{lochMaster={};assets.clear();if(!qApp->property("openloch.testing").toBool())QSettings().setValue("lochmaster/path","-");populateLibrary();status->setText(lochMasterSummary());});
    dialog.resize(680,420);dialog.exec();
}
// OpenLoch's own library: the definitions embedded in the program (or a folder given for tests) are turned into LIB
// pages once; the folder name changes with the definitions, the generator and the language, older ones are removed.
void Window::prepareOpenLibrary(){
    openLibraryFiles.clear();const auto given=qApp->property("openloch.openLibrarySource").toString();const QDir source(given.isEmpty()?":/libraries":given);
    QList<QJsonObject> pages;QCryptographicHash hash(QCryptographicHash::Sha1);
    for(const auto &name:source.entryList({"*.json"},QDir::Files,QDir::Name)){
        QFile f(source.filePath(name));if(!f.open(QIODevice::ReadOnly))continue;const auto bytes=f.readAll();hash.addData(bytes);
        const auto document=QJsonDocument::fromJson(bytes);if(document.isObject())pages.append(document.object());
    }
    if(pages.isEmpty())return;
    hash.addData(QByteArray::number(openLibraryGeneratorVersion())+uiLanguage().toLatin1());
    const auto configured=qApp->property("openloch.openLibraryDirectory").toString();
    const QString base=configured.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/OpenLoch-Bibliothek":configured;
    const QString dir=base+"/"+QString::fromLatin1(hash.result().toHex().left(12));
    for(const auto &old:QDir(base).entryList(QDir::Dirs|QDir::NoDotAndDotDot))if(QDir(base).filePath(old)!=dir)QDir(QDir(base).filePath(old)).removeRecursively();
    QDir().mkpath(dir);
    std::stable_sort(pages.begin(),pages.end(),[](const QJsonObject &a,const QJsonObject &b){return a["position"].toInt()<b["position"].toInt();});
    openLibraryPages.clear();
    for(const auto &page:pages){const QString file=dir+QString("/LIB%1.LIB").arg(page["position"].toInt());openLibraryFiles.append(file);openLibraryPages.insert(file,page);}
}
bool Window::ensureOpenLibraryPage(const QString &file){
    if(QFile::exists(file)||!openLibraryPages.contains(file))return QFile::exists(file);
    QApplication::setOverrideCursor(Qt::WaitCursor);bool written=false;
    try{const auto bytes=openLibraryPage(openLibraryPages[file],uiLanguage()!="de");QSaveFile out(file);written=out.open(QIODevice::WriteOnly)&&out.write(bytes)==bytes.size()&&out.commit();}catch(const std::exception &){}
    QApplication::restoreOverrideCursor();return written;
}
// Bibliothek → OpenLoch-Bibliothek als LIB-Dateien speichern: the generated pages for LochMaster or other uses (CC0).
void Window::saveOpenLibrary(){
    if(openLibraryFiles.isEmpty())return;
    const auto target=QFileDialog::getExistingDirectory(this,ui("Ordner für die OpenLoch-Bibliothek wählen"),startDirectory());if(target.isEmpty())return;
    bool exists=false;for(const auto &f:openLibraryFiles)exists|=QFile::exists(QDir(target).filePath(QFileInfo(f).fileName()));
    if(exists&&QMessageBox::question(this,ui("Bibliothek"),ui("Im Ordner liegen schon gleichnamige LIB-Dateien. Überschreiben?"))!=QMessageBox::Yes)return;
    int written=0;for(const auto &f:openLibraryFiles){if(!ensureOpenLibraryPage(f))continue;const auto to=QDir(target).filePath(QFileInfo(f).fileName());QFile::remove(to);written+=QFile::copy(f,to);}
    rememberDirectory(QDir(target).filePath("x"));statusBar()->showMessage(ui("%1 Bibliotheksseiten gespeichert: %2").arg(written).arg(QDir::toNativeSeparators(target)),8000);
}
void Window::saveRecovery(){
    if(!dirty)return;QDir().mkpath(QFileInfo(recoveryFile).absolutePath());project.assignIds();auto root=QJsonDocument::fromJson(project.encode()).object();root["recoverySource"]=currentPath;auto data=QJsonDocument(root).toJson();QSaveFile file(recoveryFile);
    if(data.size()>128*1024*1024||!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit())statusBar()->showMessage(ui("Automatische Sicherung fehlgeschlagen."),10000);
    else statusBar()->showMessage(ui("Ungespeicherte Änderungen automatisch gesichert."),3000);
}
void Window::restoreRecovery(){
    auto dir=QFileInfo(recoveryFile).absolutePath();auto path=QFileDialog::getOpenFileName(this,ui("Ungespeicherte OpenLoch-Sicherung wiederherstellen"),dir,ui("OpenLoch-Sicherung (*.openloch)"));if(path.isEmpty())return;
    if(openPath(path)){QFile file(path);if(file.open(QIODevice::ReadOnly)){currentPath=QJsonDocument::fromJson(file.readAll()).object()["recoverySource"].toString();recoveryFile=path;dirty=true;refresh();}}
}
QString Window::libraryDirectory() const{
    const auto configured=qApp->property("openloch.libraryDirectory").toString();
    return configured.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/Bibliotheken":configured;
}
void Window::populateLibrary() {
    // The pages are listed without signals (an entry is complete only after its data is set); the current one is shown at the end.
    struct ShowAtEnd{Window *w;~ShowAtEnd(){w->showLibraryPage(w->libraryPages->currentIndex());}} showAtEnd{this};
    QSignalBlocker blocked(libraryPages);
    library->clear();catalog.clear();libraryPages->clear();libraryParts->clear();
    QCollator numeric;numeric.setNumericMode(true);numeric.setCaseSensitivity(Qt::CaseInsensitive);
    auto files=[&numeric](QString dir,QStringList filters){QDirIterator it(dir,filters,QDir::Files,QDirIterator::Subdirectories);QStringList paths;while(it.hasNext())paths.append(it.next());std::sort(paths.begin(),paths.end(),[&numeric](const QString &a,const QString &b){return numeric.compare(a,b)<0;});return paths;};
    // Pages keep the numeric LIB1…LIBn order of the original selector; the title comes from the file header.
    auto addPage=[this](const QString &p,bool editable,const QString &prefix={}){
        QString title=QFileInfo(p).completeBaseName();QFile f(p);if(f.open(QIODevice::ReadOnly)){auto h=f.read(41);if(h.size()==41&&quint8(h[0])<=35)title=QString::fromLatin1(h.mid(1,quint8(h[0])));}
        libraryPages->addItem(prefix+title,p);const int i=libraryPages->count()-1;libraryPages->setItemData(i,p,Qt::ToolTipRole);libraryPages->setItemData(i,editable,Qt::UserRole+1);
    };
    for(const auto &p:files(libraryDirectory(),{"*.LIB","*.Lib","*.lib"}))addPage(p,true);
    // The component folder: the folder itself and every subfolder with part files, each a page of single-part files.
    {
        const QString root=componentFolder();QStringList folders;
        if(!root.isEmpty()&&QFileInfo(root).isDir()){folders.append(root);QDirIterator it(root,QDir::Dirs|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);while(it.hasNext())folders.append(it.next());}
        std::sort(folders.begin()+qMin<qsizetype>(1,folders.size()),folders.end(),[&numeric](const QString &a,const QString &b){return numeric.compare(a,b)<0;});
        if(folderWatcher){const auto watched=folderWatcher->directories();if(!watched.isEmpty())folderWatcher->removePaths(watched);}
        for(const auto &dir:folders){
            if(folderWatcher)folderWatcher->addPath(QFileInfo(dir).canonicalFilePath()); // FSEvents reports real paths (no symlinks)
            if(dir!=root&&QDir(dir).entryList({"*.LIB","*.Lib","*.lib"},QDir::Files).isEmpty())continue;
            const QString name=dir==root?QDir(root).dirName():QDir(root).relativeFilePath(dir);
            libraryPages->addItem(ui("Ordner · ")+name,dir);const int i=libraryPages->count()-1;libraryPages->setItemData(i,dir,Qt::ToolTipRole);libraryPages->setItemData(i,false,Qt::UserRole+1);libraryPages->setItemData(i,true,Qt::UserRole+2);
        }
    }
    // OpenLoch's own pages, read-only like those of an installation; the prefix keeps them apart from same-named ones.
    for(const auto &p:openLibraryFiles){addPage(p,false,"OpenLoch · ");libraryPages->setItemText(libraryPages->count()-1,"OpenLoch · "+openLibraryTitle(openLibraryPages.value(p),uiLanguage()!="de"));}
    if(lochMaster.root.isEmpty()){
        auto *item=new QTreeWidgetItem(library,{ui("LochMaster über „Bibliothek → LochMaster-Bibliotheken einbinden…“")});item->setDisabled(true);
        if(libraryPages->count()==0)libraryPages->addItem(ui("Keine Seite: „Bibliothek → LochMaster-Bibliotheken einbinden…“ oder „Seite anlegen…“"));
        return;
    }
    auto add=[this,&files](QString label,QString dir,QStringList filters,QString kind){auto *root=new QTreeWidgetItem(library,{label});
        if(!dir.isEmpty())for(const auto &p:files(dir,filters)){auto *item=new QTreeWidgetItem(root,{QFileInfo(p).completeBaseName()});item->setData(0,Qt::UserRole,p);item->setData(0,Qt::UserRole+2,kind);item->setToolTip(0,p);}
        root->setExpanded(label=="Beispielprojekte");};
    add(ui("Beispielprojekte"),lochMaster.projects,{"*.LM4","*.lm4"},"project");
    add(ui("Platinenvorlagen"),lochMaster.templates,{"*.LMB","*.lmb"},"template");
    if(!lochMaster.libraries.isEmpty())for(const auto &p:files(lochMaster.libraries,{"*.LIB","*.Lib","*.lib"}))addPage(p,false);
}
bool Window::editableLibraryPage(){
    if(libraryPages->currentData(Qt::UserRole+1).toBool())return true;
    QMessageBox::information(this,ui("Bibliothek"),ui("Seiten einer LochMaster-Installation werden nicht verändert. Eine eigene Seite wählen oder über „Bibliothek → Seite anlegen…“ anlegen."));return false;
}
// Loads the current own page, applies a change and writes it back as LIB, then shows it again.
bool Window::changeLibraryPage(const std::function<void(Project &)> &change,int selectRow){
    if(!editableLibraryPage())return false;const int index=libraryPages->currentIndex();const auto path=libraryPages->currentData().toString();
    try{auto page=Project::load(path);change(page);page.save(path);catalog.remove(path);libraryPages->setItemText(index,page.title);showLibraryPage(index);
        if(selectRow>=0&&selectRow<libraryParts->count())libraryParts->setCurrentRow(selectRow);return true;}
    catch(const std::exception &e){QMessageBox::warning(this,ui("Bibliothek konnte nicht geändert werden"),QString::fromUtf8(e.what()));return false;}
}
void Window::createLibraryPage(){
    bool ok=false;const auto name=QInputDialog::getText(this,ui("Seite anlegen"),ui("Name der Bibliotheksseite"),QLineEdit::Normal,ui("Eigene Bauteile"),&ok).trimmed();if(!ok||name.isEmpty())return;
    const QDir dir(libraryDirectory());QDir().mkpath(dir.path());int number=1;while(dir.exists(QString("LIB%1.LIB").arg(number)))number++;
    const auto path=dir.filePath(QString("LIB%1.LIB").arg(number));
    try{Project page;page.title=name;page.save(path);}catch(const std::exception &e){QMessageBox::warning(this,ui("Seite konnte nicht angelegt werden"),QString::fromUtf8(e.what()));return;}
    populateLibrary();libraryPages->setCurrentIndex(libraryPages->findData(path));
}
void Window::deleteLibraryPage(){
    if(!editableLibraryPage())return;const auto path=libraryPages->currentData().toString();
    if(QMessageBox::question(this,ui("Seite löschen"),ui("Die Bibliotheksseite „")+libraryPages->currentText()+ui("“ in den Papierkorb legen?"))!=QMessageBox::Yes)return;
    if(!(qApp->property("openloch.testing").toBool()?QFile::remove(path):QFile::moveToTrash(path))){QMessageBox::warning(this,ui("Seite löschen"),ui("Die Datei konnte nicht entfernt werden."));return;}
    populateLibrary();
}
// 0 to the top, 1 to the bottom, 2 one up, 3 one down; the page order is written through the record order.
void Window::moveLibraryPart(int where){
    const int row=libraryParts->currentRow(),count=libraryParts->count();if(row<0||row>=count){if(editableLibraryPage())QMessageBox::information(this,ui("Bibliothek"),ui("Ein Bauteil in der Liste auswählen."));return;}
    const int target=where==0?0:where==1?count-1:where==2?qMax(0,row-1):qMin(count-1,row+1);if(target==row)return;
    changeLibraryPage([&](Project &page){QList<int> order;for(int i=0;i<count;i++)order<<i;order.move(row,target);
        for(int k=0;k<order.size();k++){const auto key=QString::number(order[k]);auto edit=page.edits.value(key).toObject();edit["z"]=10+k*.001;page.edits[key]=edit;}},target);
}
void Window::deleteLibraryPart(){
    const int row=libraryParts->currentRow();if(row<0||row>=libraryParts->count()){if(editableLibraryPage())QMessageBox::information(this,ui("Bibliothek"),ui("Ein Bauteil in der Liste auswählen."));return;}
    if(!editableLibraryPage()||QMessageBox::question(this,ui("Bauteil löschen"),ui("„")+libraryParts->item(row)->text()+ui("“ aus der Bibliotheksseite löschen?"))!=QMessageBox::Yes)return;
    changeLibraryPage([&](Project &page){const auto key=QString::number(row);auto edit=page.edits.value(key).toObject();edit["deleted"]=true;page.edits[key]=edit;},qMin(row,libraryParts->count()-2));
}
// "Zur Bibliothek hinzufügen" like the original: the Bauteil dialog comes first (a marked part brings its values, else
// those used last), then the marked objects become one part on the current page. Seen from the solder side the part is
// turned back to the component side and mirrored vertically, as the original does.
void Window::addSelectionToLibrary(){
    const auto data=canvas->selectionData();if(data.isEmpty()){QMessageBox::information(this,ui("Zur Bibliothek hinzufügen"),ui("Ein Bauteil oder mehrere Objekte markieren."));return;}
    if(!editableLibraryPage())return;
    const auto n=canvas->selectedNode();const auto e=n["type"]=="component"?project.componentNode(n):n;const bool group=canvas->selectionCount()==1&&e.contains("children");
    auto values=componentTemplate;
    if(group&&e["group_flags"].toArray().at(0).toBool())values={{"id",e["id"]},{"value",e["value"]},{"description",e["description"]},{"label",e["label"]},{"listed",e["group_flags"].toArray().at(1).toBool(true)},{"extra",e["extra"]}};
    if(!componentDialog(values,false))return;componentTemplate=values;
    try{
        const auto bytes=writeLegacyDocument(Project::decode(data),!group);const auto objects=LegacyReader(bytes).read(false)["objects"].toArray();
        QJsonObject part{{"type","component"},{"label",values["label"]},{"description",values["description"]},{"id",values["id"]},{"value",values["value"]},{"group_flags",QJsonArray{true,values["listed"].toBool()}},{"extra",values["extra"]}};
        if(canvas->backActive()){part["otherSide"]=true;part["mirrorY"]=true;}
        changeLibraryPage([&](Project &page){const auto key=page.addLibrary(bytes,"Hinzugefügt.lib");
            for(int i=0;i<objects.size();i++){const auto anchor=componentAnchor(objects[i].toObject());auto placement=part;placement["library"]=key;placement["index"]=i;placement["x"]=anchor.x();placement["y"]=anchor.y();page.additions.append(placement);}},libraryParts->count()+int(objects.size())-1);
    }catch(const std::exception &e){QMessageBox::warning(this,ui("Zur Bibliothek hinzufügen"),QString::fromUtf8(e.what()));}
}
// "Bibliothek → Eigenschaften…" like the original: the page's name, its file and the page's extra fields. Pages of a
// LochMaster installation, OpenLoch's library and folders are only shown.
void Window::libraryPageProperties(){
    const auto path=libraryPages->currentData().toString();if(path.isEmpty())return;const bool editable=libraryPages->currentData(Qt::UserRole+1).toBool();
    Project page;if(!QFileInfo(path).isDir()){try{ensureOpenLibraryPage(path);page=Project::load(path);}catch(const std::exception &e){QMessageBox::warning(this,ui("Eigenschaften"),QString::fromUtf8(e.what()));return;}}
    QDialog d(this);d.setObjectName("libraryPageDialog");d.setWindowTitle(ui("Eigenschaften"));auto *form=new QFormLayout(&d);
    form->addRow(new QLabel("<b>"+libraryPages->currentText().toHtmlEscaped()+"</b>",&d));
    auto *name=new QLineEdit(QFileInfo(path).isDir()?libraryPages->currentText():page.title,&d);name->setObjectName("libraryPageName");name->setReadOnly(!editable);form->addRow(ui("Name"),name);
    auto *file=new QLineEdit(QDir::toNativeSeparators(path),&d);file->setObjectName("libraryPagePath");file->setReadOnly(true);form->addRow(ui("Pfad"),file);
    auto fields=page.boardExtra();std::function<void()> sync;if(editable)sync=addExtraRows(form,d,fields);
    QDialogButtonBox buttons(editable?QDialogButtonBox::Ok|QDialogButtonBox::Cancel:QDialogButtonBox::Close);form->addRow(&buttons);
    connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
    if(d.exec()!=QDialog::Accepted||!editable)return;sync();const auto title=name->text().trimmed();
    if(!title.isEmpty())changeLibraryPage([&](Project &p){p.title=title;if(fields!=p.boardExtra())p.boardSettings["extra"]=fields;});
}
// "Eigenschaften…" in the library list: the marked part's Bauteil dialog on an own page, otherwise the page's properties.
void Window::libraryPartProperties(){
    const int row=libraryParts->currentRow();const auto path=libraryPages->currentData().toString();
    if(row<0||row>=libraryParts->count()||!libraryPages->currentData(Qt::UserRole+1).toBool()||QFileInfo(path).isDir()){libraryPageProperties();return;}
    try{
        const auto page=Project::load(path);const auto key=QString::number(row);const auto node=page.legacyNode(row);if(!node.contains("children")){libraryPageProperties();return;}
        const auto raw=page.edits.value(key).toObject().contains("id")?page.edits.value(key).toObject()["id"]:page.legacy["objects"].toArray()[row].toObject()["id"];
        QJsonObject values{{"id",raw},{"value",node["value"]},{"description",node["description"]},{"label",node["label"]},{"listed",node["group_flags"].toArray().at(1).toBool(true)},{"extra",node["extra"]}};
        if(!componentDialog(values,false))return;
        changeLibraryPage([&](Project &p){auto edit=p.edits.value(key).toObject();
            for(auto field:{"label","description","id","value","extra"})edit[field]=values[field];edit["group_flags"]=QJsonArray{node["group_flags"].toArray().at(0).toBool(true),values["listed"].toBool()};p.edits[key]=edit;},row);
    }catch(const std::exception &e){QMessageBox::warning(this,ui("Eigenschaften"),QString::fromUtf8(e.what()));}
}
namespace {
// Dokumente/OpenLoch/Bauteile. Before the name was fixed it followed the interface language ("Components",
// "Composants"); such a folder stays in use as long as the fixed one is missing.
QString defaultComponentFolder(){
    // While "Bauteile" does not exist, a folder under a translated name of it (the language set first) is the own one.
    const QString fixed=openLochDocumentsFolder(QStringLiteral("Bauteile"));if(QFileInfo::exists(fixed))return fixed;
    for(const QString &name:{ui("Bauteile"),QStringLiteral("Components"),QStringLiteral("Composants")})
        if(const QString translated=openLochDocumentsFolder(name);translated!=fixed&&QFileInfo(translated).isDir())return translated;
    return fixed;
}
QString ownComponentFolder(){
    const auto configured=qApp->property("openloch.componentFolder").toString();if(!configured.isEmpty())return configured;
    if(qApp->property("openloch.testing").toBool())return {}; // tests never see the user's folder
    const auto chosen=QSettings().value("library/componentFolder").toString();
    return chosen.isEmpty()?defaultComponentFolder():chosen;
}
}
QString Window::componentFolder() const{return ownComponentFolder();}
LibraryFolders componentLibraryFolders(){
    LibraryFolders folders{ownComponentFolder(),{}};
    const QString lochMaster=QSettings().value("lochmaster/path").toString();
    if(!lochMaster.isEmpty()&&lochMaster!="-")if(const auto found=findLochMaster(lochMaster,uiLanguage().toUpper());!found.libraries.isEmpty())folders.extra<<found.libraries;
    return folders;
}
void setComponentLibraryFolders(const LibraryFolders &folders){
    if(qApp->property("openloch.testing").toBool()&&qApp->property("openloch.componentFolder").isValid()){qApp->setProperty("openloch.componentFolder",folders.own);return;}
    QSettings().setValue("library/componentFolder",folders.own==defaultComponentFolder()?QString():folders.own);
}
void Window::librariesChanged(){populateLibrary();}
void Window::chooseComponentFolder(){
    const auto dir=QFileDialog::getExistingDirectory(this,ui("Bauteilordner wählen"),componentFolder());if(dir.isEmpty())return;
    QSettings().setValue("library/componentFolder",dir);populateLibrary();
}
// The marked part (several marked objects become one) as a single-part LIB file in the current folder page or the
// component folder; the file and page are named after the part.
void Window::saveSelectionAsPartFile(){
    const auto data=canvas->selectionData();if(data.isEmpty()){QMessageBox::information(this,ui("Als Bauteildatei speichern"),ui("Ein Bauteil oder mehrere Objekte markieren."));return;}
    try{
        const auto bytes=writeLegacyDocument(Project::decode(data),canvas->selectionCount()>1);const auto objects=LegacyReader(bytes).read(false)["objects"].toArray();
        const auto first=objects.isEmpty()?QJsonObject{}:objects.first().toObject();QString name=first["label"].toString();if(name.isEmpty())name=first["description"].toString();if(name.isEmpty())name=ui("Bauteil");
        const QString folder=libraryPages->currentData(Qt::UserRole+2).toBool()?libraryPages->currentData().toString():componentFolder();if(folder.isEmpty())return;
        bool ok=false;name=QInputDialog::getText(this,ui("Als Bauteildatei speichern"),ui("Name des Bauteils (auch Dateiname)"),QLineEdit::Normal,name,&ok).trimmed();if(!ok||name.isEmpty())return;
        QString file=name;file.replace(QRegularExpression(R"([\\/:*?"<>|])"),"_");QDir().mkpath(folder);QString path=QDir(folder).filePath(file+".LIB");
        if(QFileInfo::exists(path)&&QMessageBox::question(this,ui("Als Bauteildatei speichern"),ui("„")+QFileInfo(path).fileName()+ui("“ gibt es schon. Ersetzen?"))!=QMessageBox::Yes)return;
        Project page;page.title=name.left(35);const auto key=page.addLibrary(bytes,"Bauteil.lib");
        for(int i=0;i<objects.size();i++){const auto anchor=componentAnchor(objects[i].toObject());page.additions.append(QJsonObject{{"type","component"},{"library",key},{"index",i},{"x",anchor.x()},{"y",anchor.y()}});}
        page.save(path);catalog.remove(path);populateLibrary();const int at=libraryPages->findData(folder);if(at>=0)libraryPages->setCurrentIndex(at);
        statusBar()->showMessage(ui("Bauteil gespeichert: ")+path,8000);
    }catch(const std::exception &e){QMessageBox::warning(this,ui("Als Bauteildatei speichern"),QString::fromUtf8(e.what()));}
}
void Window::showLibraryPage(int index) {
    libraryParts->clear();libraryBitmaps=canvas->viewState().bitmaps;auto path=libraryPages->itemData(index).toString();if(path.isEmpty())return;
    if(libraryPages->itemData(index,Qt::UserRole+2).toBool()){
        // A folder page: the parts of every LIB file in it, each placed from its own file.
        QCollator numeric;numeric.setNumericMode(true);numeric.setCaseSensitivity(Qt::CaseInsensitive);auto names=QDir(path).entryList({"*.LIB","*.Lib","*.lib"},QDir::Files);
        std::sort(names.begin(),names.end(),[&numeric](const QString &a,const QString &b){return numeric.compare(a,b)<0;});const double ratio=qMax(2.0,libraryParts->devicePixelRatioF());
        for(const auto &name:names){const auto file=QDir(path).filePath(name);
            try{if(!catalog.contains(file)){auto p=Project::load(file);catalog[file]={name,"lib",p.original,p.legacy};}
                const auto &source=catalog[file];const auto nodes=source.document["objects"].toArray();
                for(int i=0;i<nodes.size();i++){auto n=nodes[i].toObject();QString label=n["description"].toString();if(label.isEmpty())label=n["label"].toString(QFileInfo(name).completeBaseName());
                    auto *item=new QListWidgetItem(label,libraryParts);item->setData(Qt::DecorationRole,renderNode(n,source.bytes,previewScale,ratio,libraryBitmaps));item->setData(Qt::UserRole,file);item->setData(Qt::UserRole+1,i);
                    item->setToolTip(name+"\n"+componentId(n)+" · "+n["value"].toString()+ui("\nKlicken oder auf die Platine ziehen"));}}
            catch(const std::exception &e){auto *item=new QListWidgetItem(name+": "+QString::fromUtf8(e.what()),libraryParts);item->setFlags(Qt::NoItemFlags);}
        }
        if(names.isEmpty()){auto *item=new QListWidgetItem(ui("Noch keine Bauteile: ein Bauteil markieren und „Bibliothek → Als Bauteildatei speichern…“ wählen oder LIB-Dateien in den Ordner kopieren"),libraryParts);item->setFlags(Qt::NoItemFlags);}
        return;
    }
    try {
        ensureOpenLibraryPage(path);
        if(!catalog.contains(path)){auto p=Project::load(path);catalog[path]={QFileInfo(path).fileName(),"lib",p.original,p.legacy};}
        const auto &source=catalog[path];auto nodes=source.document["objects"].toArray();const double ratio=qMax(2.0,libraryParts->devicePixelRatioF());
        for(int i=0;i<nodes.size();i++){auto n=nodes[i].toObject();QString label=n["description"].toString();if(label.isEmpty())label=n["label"].toString(n["type"].toString());
            auto *item=new QListWidgetItem(label,libraryParts);item->setData(Qt::DecorationRole,renderNode(n,source.bytes,previewScale,ratio,libraryBitmaps));item->setData(Qt::UserRole,path);item->setData(Qt::UserRole+1,i);
            item->setToolTip(componentId(n)+" · "+n["value"].toString()+ui("\nKlicken oder auf die Platine ziehen"));}
    }catch(const std::exception &e){auto *item=new QListWidgetItem(ui("Bibliothek konnte nicht geladen werden: ")+QString::fromUtf8(e.what()),libraryParts);item->setFlags(Qt::NoItemFlags);}
}
// Folder of the original's body pictures for a component style, when a LochMaster installation is connected.
// The installation spells some folders in lower case, so the lookup ignores case.
QString Window::assistantPictureFolder(int style) const{
    const QString wanted=style==11?"Farbverlauf-Diagonal":style==13?"Farbverlauf-Quadratisch":style==12||style==14?"Farbverlauf-Rund":QString();
    if(lochMaster.bitmaps.isEmpty()||wanted.isEmpty())return {};
    const QDir dir(lochMaster.bitmaps);
    for(const auto &name:dir.entryList(QDir::Dirs|QDir::NoDotAndDotDot))if(name.compare(wanted,Qt::CaseInsensitive)==0)return dir.filePath(name);
    return {};
}
void Window::componentAssistant(){
    if(project.mode!="board"){QMessageBox::information(this,ui("Objekt-Assistent"),ui("Der Assistent erzeugt Konturen und Bauteile für Lochrasterplatinen."));return;}
    // Body colours: the installation's pictures when present (names up to the first dot), otherwise OpenLoch's own gradients.
    auto pictures=[this](int style){
        QMap<QString,QString> files;const auto folder=assistantPictureFolder(style);
        if(!folder.isEmpty())for(const auto &info:QDir(folder).entryInfoList(QDir::Files))if(info.suffix().compare("bmp",Qt::CaseInsensitive)==0)files.insert(info.fileName().section('.',0,0),info.filePath());
        return files;
    };
    auto colours=[&](int style){auto files=pictures(style);QStringList names=files.keys();std::sort(names.begin(),names.end(),[](const QString &a,const QString &b){return a.compare(b,Qt::CaseInsensitive)<0;});return names;};
    auto picture=[&](int style,const QList<AssistantParameter> &values)->QByteArray{
        if(style<11)return {};const auto &colour=values.value(style==11||style==13?6:5);const auto name=colour.choices.value(colour.choice);
        const auto files=pictures(style);if(files.contains(name)){QFile file(files[name]);if(file.open(QIODevice::ReadOnly))return file.readAll();}
        return gradientBitmap(style,name);
    };
    if(assistantValues.isEmpty())assistantValues=assistantParameters(assistantStyle,colours(assistantStyle));
    int style=assistantStyle;auto values=assistantValues;
    QDialog dlg(this);dlg.setWindowTitle(ui("Objekt-Assistent"));
    auto *preview=new QLabel;preview->setObjectName("assistantPreview");preview->setFixedSize(302,344);preview->setAlignment(Qt::AlignCenter);
    preview->setStyleSheet("background:#efe6cf;border:1px solid #b9ab88;");
    auto *group=new QGroupBox(ui("Objekt"));auto *styles=new QComboBox;styles->setObjectName("assistantStyle");for(const auto &style:assistantStyles())styles->addItem(ui(style));styles->setMaxVisibleItems(20);
    (new QVBoxLayout(group))->addWidget(styles);
    auto *table=new QTableWidget(0,2);table->setObjectName("assistantParameters");table->setHorizontalHeaderLabels({ui("Parameter"),ui("Wert")});
    table->verticalHeader()->hide();table->setColumnWidth(0,140);table->horizontalHeader()->setStretchLastSection(true);table->setMinimumWidth(265);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Cancel)->setText(ui("Abbrechen"));
    // Moving the current cell commits a value still being edited, as the original's OK commits the current row.
    connect(buttons,&QDialogButtonBox::accepted,&dlg,[&]{table->setCurrentIndex({});dlg.accept();});connect(buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
    auto *side=new QVBoxLayout;side->addWidget(group);side->addWidget(table,1);side->addWidget(buttons);
    auto *layout=new QHBoxLayout(&dlg);layout->addWidget(preview);layout->addLayout(side);
    // Shown values and preview follow every change; the resistor's value snaps to the chosen series as in the original.
    auto update=[&]{
        if(style==11)values[4].value=standardValue(values[4].value,values[5].choice);
        {QSignalBlocker quiet(table);for(int i=0;i<values.size();i++)if(auto *cell=table->item(i,1))cell->setText(assistantValueText(values[i]));}
        try{
            const auto bytes=writeLegacyObjects({assistantObject(style,values,picture(style,values))},ui("Assistent"));const auto node=LegacyReader(bytes).read(false)["objects"].toArray().at(0).toObject();
            const auto r=legacyBounds(node);const double scale=qMin(.2,qMin(280/qMax(1.0,r.width()),320/qMax(1.0,r.height())));
            preview->setPixmap(QPixmap::fromImage(renderNode(node,bytes,scale,preview->devicePixelRatioF())));
        }catch(const std::exception &e){preview->setText(QString::fromUtf8(e.what()));}
    };
    auto fill=[&]{
        QSignalBlocker quiet(table);table->clearContents();table->setRowCount(values.size());
        for(int i=0;i<values.size();i++){
            const auto &v=values[i];auto *name=new QTableWidgetItem(v.unit.isEmpty()?ui(v.name):ui(v.name)+" ["+ui(v.unit)+"]");name->setFlags(Qt::ItemIsEnabled);table->setItem(i,0,name);
            if(v.type==AssistantParameter::Choice){
                auto *box=new QComboBox;for(const auto &c:v.choices)box->addItem(ui(c));box->setCurrentIndex(v.choice);table->setCellWidget(i,1,box);
                connect(box,&QComboBox::currentIndexChanged,&dlg,[&,i](int index){if(index>=0){values[i].choice=index;update();}});
            }else table->setItem(i,1,new QTableWidgetItem(assistantValueText(v)));
        }
    };
    connect(table,&QTableWidget::itemChanged,&dlg,[&](QTableWidgetItem *cell){
        if(cell->column()!=1||cell->row()>=values.size())return;const auto text=cell->text();
        const auto result=assistantSetValue(values[cell->row()],text);
        if(result==AssistantInput::Corrected)QMessageBox::information(&dlg,ui("Hinweis"),ui("Der eingegebene Wert überschreitet die zulässigen Grenzen und wird korrigiert!"));
        if(result==AssistantInput::Invalid)QMessageBox::information(&dlg,ui("Hinweis"),"„"+text+ui("“ ist kein gültiger Wert."));
        update();
    });
    connect(styles,&QComboBox::currentIndexChanged,&dlg,[&](int index){if(index<0)return;style=index;values=assistantParameters(style,colours(style));fill();update();});
    {QSignalBlocker quiet(styles);styles->setCurrentIndex(style);}fill();update();
    const bool accepted=dlg.exec()==QDialog::Accepted;assistantStyle=style;assistantValues=values;if(!accepted)return;
    try{
        // Like the original's paste: the part follows the mouse; parts snap with the soldered end of lead 1, contours with their centre.
        const auto bytes=writeLegacyObjects({assistantObject(style,values,picture(style,values),canvas->backActive())},ui("Assistent"));
        const auto key=project.addLibrary(bytes,"Assistent.lib");canvas->beginPlacement(key,0);info->setText(ui("Objekt mit Klick platzieren · Esc beendet das Platzieren"));
    }catch(const std::exception &e){QMessageBox::warning(this,ui("Objekt-Assistent"),QString::fromUtf8(e.what()));}
}
void Window::placeComponent(const QString &path,int index) {
    if(project.mode!="board"){QMessageBox::information(this,ui("Lochrasterbauteil"),ui("LochMaster-Bauteile werden auf einer Platine platziert."));return;}
    try {ensureOpenLibraryPage(path);LibrarySource source;if(catalog.contains(path))source=catalog[path];else{auto p=Project::load(path);source={QFileInfo(path).fileName(),"lib",p.original,p.legacy};}
        auto key=project.addLibrary(source.bytes,source.name);canvas->beginPlacement(key,index);info->setText(ui("Bauteil mit Klick platzieren · Esc beendet das Platzieren"));
    }catch(const std::exception &e){QMessageBox::warning(this,ui("Bauteil konnte nicht geladen werden"),QString::fromUtf8(e.what()));}
}
void Window::applyTemplate(const QString &path) {
    if(project.mode!="board")return;
    try {auto p=Project::load(path);canvas->beforeChange();auto key=project.addLibrary(p.original,QFileInfo(path).fileName(),"lmb");project.boardSource=key;project.width=p.width;project.height=p.height;canvas->rebuild();canvas->fit();canvas->changed();}
    catch(const std::exception &e){QMessageBox::warning(this,ui("Vorlage konnte nicht geladen werden"),QString::fromUtf8(e.what()));}
}
// Every extra field name used in the document, in first-seen order: board fields, objects and everything inside them.
QStringList Window::extraFieldNames() const{
    QStringList names;std::function<void(const QJsonObject&)> collect=[&](const QJsonObject &n){
        for(auto f:n["extra"].toArray()){const auto name=f.toArray().at(0).toString();if(!name.isEmpty()&&!names.contains(name))names<<name;}
        for(auto v:n["children"].toArray())collect(v.toObject());
    };
    collect(QJsonObject{{"extra",project.boardExtra()}});for(int i=0;i<project.legacy["objects"].toArray().size();i++)collect(project.legacyNode(i));
    for(auto v:project.additions){const auto n=v.toObject();collect(n["type"]=="component"?project.componentNode(n):n);}
    return names;
}
// LochMaster's "Extrafelder": all field names of the document, checked for the fields the object has. Unchecking a
// field with content asks before deleting it; new names are added checked.
bool Window::editExtraFields(QJsonArray &fields,QWidget *parent){
    QDialog d(parent);d.setObjectName("extraFieldsDialog");d.setWindowTitle(ui("Extrafelder"));QVBoxLayout box(&d);QListWidget list;list.setObjectName("extraFieldList");box.addWidget(&list);
    auto names=extraFieldNames();for(auto f:fields){const auto n=f.toArray().at(0).toString();if(!names.contains(n))names<<n;}
    auto active=[&](const QString &name){for(auto f:fields)if(f.toArray().at(0).toString()==name)return f.toArray().at(1).toBool();return false;};
    for(const auto &name:names){auto *item=new QListWidgetItem(name,&list);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(active(name)?Qt::Checked:Qt::Unchecked);}
    QCheckBox all(ui("Alle Felder markieren/demarkieren"));all.setChecked(true);box.addWidget(&all);
    connect(&all,&QCheckBox::toggled,&d,[&](bool on){for(int i=0;i<list.count();i++)list.item(i)->setCheckState(on?Qt::Checked:Qt::Unchecked);});
    QPushButton add(ui("Neues Feld hinzufügen…"));add.setObjectName("addExtraField");box.addWidget(&add);
    connect(&add,&QPushButton::clicked,&d,[&]{bool ok=false;const auto name=QInputDialog::getText(&d,ui("Feld hinzufügen"),ui("Neuer Feldname"),QLineEdit::Normal,{},&ok).left(80);
        if(ok&&!name.isEmpty()){auto *item=new QListWidgetItem(name,&list);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Checked);}});
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);box.addWidget(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
    if(d.exec()!=QDialog::Accepted)return false;
    QStringList listed,checked;for(int i=0;i<list.count();i++){listed<<list.item(i)->text();if(list.item(i)->checkState()==Qt::Checked)checked<<list.item(i)->text();}
    QJsonArray result;
    for(auto v:fields){const auto f=v.toArray();const auto name=f.at(0).toString(),value=f.at(2).toString();
        if(!listed.contains(name))continue;
        if(!checked.contains(name)){
            if(value.isEmpty())continue;
            if(QMessageBox::warning(parent,ui("Löschen bestätigen"),QString(ui("Sie haben das Feld \"%1\" deaktiviert.\nDas Feld wird gelöscht.\nDer Feldinhalt \"%2\" geht dabei verloren.")).arg(name,value),QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes)continue;
        }
        result.append(f);
    }
    for(const auto &name:checked){bool present=false;for(auto v:result)present|=v.toArray().at(0).toString()==name;if(!present)result.append(QJsonArray{name,true,""});}
    fields=result;return true;
}
// The active extra fields as rows under "Extra" and the button "Zusatzangaben…" that chooses them, as in the original's
// Bauteil and Platine dialogs. The returned function writes the edited values into `fields`.
std::function<void()> Window::addExtraRows(QFormLayout *form,QDialog &dialog,QJsonArray &fields){
    auto *box=new QWidget(&dialog);auto *rows=new QFormLayout(box);rows->setContentsMargins(0,0,0,0);auto *header=new QLabel("<b>Extra</b>",&dialog);
    auto edits=std::make_shared<QList<std::pair<int,QLineEdit*>>>();
    auto sync=[&fields,edits]{for(auto [i,e]:*edits){auto f=fields[i].toArray();f[2]=e->text();fields[i]=f;}};
    auto rebuild=[&fields,edits,rows,header]{while(rows->rowCount())rows->removeRow(0);edits->clear();
        for(int i=0;i<fields.size();i++){const auto f=fields[i].toArray();if(!f.at(1).toBool())continue;auto *e=new QLineEdit(f.at(2).toString());e->setObjectName("extra:"+f.at(0).toString());rows->addRow(f.at(0).toString(),e);edits->append({i,e});}
        header->setVisible(!edits->isEmpty());};
    form->addRow(header);form->addRow(box);rebuild();
    auto *more=new QPushButton(ui("Zusatzangaben…"),&dialog);more->setObjectName("extraFieldsButton");form->addRow(more);
    connect(more,&QPushButton::clicked,&dialog,[this,&fields,&dialog,sync,rebuild]{sync();if(editExtraFields(fields,&dialog))rebuild();});
    return sync;
}
// The original's "Bauteil" dialog: Name, Beschreibung, Kennung, Wert/Typ, "Erscheint in Stückliste", optionally the
// centre, then the active extra fields and "Zusatzangaben…".
bool Window::componentDialog(QJsonObject &values,bool position,QPointF *centre){
    QDialog dialog(this);dialog.setObjectName("componentDialog");dialog.setWindowTitle(ui("Bauteil"));auto *form=new QFormLayout(&dialog);
    QLineEdit name(values["label"].toString()),description(values["description"].toString()),id(values["id"].toString()),value(values["value"].toString());
    name.setObjectName("componentName");description.setObjectName("componentDescription");id.setObjectName("componentId");value.setObjectName("componentValue");id.setMaximumWidth(90);
    form->addRow(ui("Name"),&name);form->addRow(ui("Beschreibung"),&description);form->addRow(ui("Kennung"),&id);form->addRow(ui("Wert/Typ"),&value);
    QCheckBox listed(ui("Erscheint in Stückliste"));listed.setChecked(values["listed"].toBool(true));form->addRow("",&listed);
    // Bezugspunkt: the terminal the part turns about and that OpenLoch places on a hole (OpenLoch extension).
    QComboBox reference;reference.setObjectName("componentReference");const auto terminals=values["terminals"].toArray();
    if(!terminals.isEmpty()){
        reference.addItem(ui("Automatisch (Anschluss nahe der Mitte)"));const QPointF first(terminals[0].toArray()[0].toDouble(),terminals[0].toArray()[1].toDouble());
        for(int i=0;i<terminals.size();i++){const QPointF at=QPointF(terminals[i].toArray()[0].toDouble(),terminals[i].toArray()[1].toDouble())-first;
            reference.addItem(ui("Anschluss %1 (%2 / %3 mm)").arg(i+1).arg(uiLocale().toString(at.x()/100,'f',2),uiLocale().toString(at.y()/100,'f',2)));}
        reference.setCurrentIndex(qBound(0,values["reference"].toInt(-1)+1,reference.count()-1));form->addRow(ui("Bezugspunkt"),&reference);
    }
    QDoubleSpinBox x,y;x.setObjectName("centreX");y.setObjectName("centreY");
    if(position&&centre){for(auto *box:{&x,&y}){box->setRange(-10000,10000);box->setDecimals(2);box->setSuffix(" mm");}x.setValue(centre->x()/100);y.setValue(centre->y()/100);form->addRow(ui("Position: Mitte X"),&x);form->addRow(ui("Mitte Y"),&y);}
    auto fields=values["extra"].toArray();const auto sync=addExtraRows(form,dialog,fields);
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return false;sync();
    values["label"]=name.text();values["description"]=description.text();values["id"]=id.text();values["value"]=value.text();values["listed"]=listed.isChecked();values["extra"]=fields;
    if(!terminals.isEmpty())values["reference"]=reference.currentIndex()-1;
    if(position&&centre)*centre=QPointF(std::round(x.value()*100),std::round(y.value()*100));return true;
}
// The original's dialog of a plain group: its name, "Abmessungen" (a new width or height scales the group about its
// centre) and the position of its centre; one undo step.
void Window::groupProperties(){
    try{
        const auto piece=writeLegacyDocument(Project::decode(canvas->selectionData()),false,false);const auto object=LegacyReader(piece).read(false)["objects"].toArray().at(0).toObject();
        const QRectF box=legacyBounds(object);const auto n=canvas->selectedNode();const auto effective=n["type"]=="component"?project.componentNode(n):n;
        QDialog d(this);d.setObjectName("groupDialog");d.setWindowTitle(ui("Gruppe"));auto *form=new QFormLayout(&d);
        auto *name=new QLineEdit(effective["label"].toString(),&d);name->setObjectName("groupName");form->addRow(ui("Name"),name);
        auto spin=[&](const char *objectName,double value,double low){auto *s=new QDoubleSpinBox(&d);s->setObjectName(objectName);s->setRange(low,10000);s->setDecimals(2);s->setSuffix(" mm");s->setValue(value);return s;};
        form->addRow(new QLabel("<b>"+ui("Abmessungen")+"</b>",&d));auto *width=spin("groupWidth",box.width()/100,0),*height=spin("groupHeight",box.height()/100,0);form->addRow(ui("Breite"),width);form->addRow(ui("Höhe"),height);
        form->addRow(new QLabel("<b>"+ui("Position")+"</b>",&d));auto *x=spin("groupCentreX",box.center().x()/100,-10000),*y=spin("groupCentreY",box.center().y()/100,-10000);form->addRow(ui("Mitte X"),x);form->addRow(ui("Mitte Y"),y);
        QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
        if(d.exec()!=QDialog::Accepted)return;
        const QPointF move=QPointF(std::round(x->value()*100),std::round(y->value()*100))-QPointF(std::round(box.center().x()),std::round(box.center().y()));
        const bool resized=std::abs(width->value()*100-box.width())>=1||std::abs(height->value()*100-box.height())>=1;
        if(!resized){canvas->editSelected({{"label",name->text()}},QLineF(QPointF(),move).length()>=1?move:QPointF());return;}
        canvas->replaceSelected(writeScaledObject(piece,0,width->value()*100,height->value()*100),"Skalierte Gruppe.lib",move,{{"label",name->text()}});
    }catch(const std::exception &e){QMessageBox::warning(this,ui("Gruppe"),QString::fromUtf8(e.what()));}
}
// The Bauteil dialog of a part inside another part; its values are kept with the top-level object.
void Window::nestedPartProperties(const QString &kind,int index,const QString &path){
    QJsonObject part=kind=="legacy"?project.legacyNode(index):project.componentNode(project.additions.at(index).toObject());
    for(const auto &step:path.split('/'))part=part["children"].toArray().at(step.toInt()).toObject();
    if(!part.contains("children")||!part["group_flags"].toArray().at(0).toBool())return;
    QJsonObject values{{"id",part["id"]},{"value",part["value"]},{"description",part["description"]},{"label",part["label"]},{"listed",part["group_flags"].toArray().at(1).toBool(true)},{"extra",part["extra"]}};
    if(!componentDialog(values,false))return;const auto id=values["id"].toString();
    QJsonObject changes{{"label",values["label"]},{"description",values["description"]},{"id",id},{"value",values["value"]},{"group_flags",QJsonArray{true,values["listed"].toBool()}},{"extra",values["extra"]}};
    if(id!=part["id"].toString()&&id.contains('#'))changes["group_value"]=project.nextNumber(id);
    canvas->beforeChange();auto stored=kind=="legacy"?project.edits.value(QString::number(index)).toObject():project.additions.at(index).toObject();
    auto nested=stored["nested"].toObject();auto entry=nested[path].toObject();for(auto it=changes.begin();it!=changes.end();++it)entry[it.key()]=it.value();nested[path]=entry;stored["nested"]=nested;
    if(kind=="legacy")project.edits[QString::number(index)]=stored;else project.additions[index]=stored;canvas->rebuild();canvas->changed();
}
// "Bauteileinheit bilden": several objects are grouped first; a plain group becomes a part. A Kennung with "#" gets the
// next free number.
void Window::defineComponent(){
    const int count=canvas->selectionCount();const auto n=canvas->selectedNode();const auto e=n["type"]=="component"?project.componentNode(n):n;
    const bool plainGroup=count==1&&e.contains("children")&&!e["group_flags"].toArray().at(0).toBool();
    if(count<2&&!plainGroup){QMessageBox::information(this,ui("Bauteileinheit bilden"),ui("Mehrere Objekte oder eine Gruppe markieren."));return;}
    auto values=componentTemplate;if(!componentDialog(values,false))return;componentTemplate=values;
    try{if(count>1)canvas->groupSelected();}catch(const std::exception &ex){QMessageBox::warning(this,ui("Bauteileinheit bilden"),QString::fromUtf8(ex.what()));return;}
    const auto id=values["id"].toString();QJsonObject changes{{"label",values["label"]},{"description",values["description"]},{"id",id},{"value",values["value"]},
        {"group_flags",QJsonArray{true,values["listed"].toBool()}},{"extra",values["extra"]}};
    if(id.contains('#'))changes["group_value"]=project.nextNumber(id);canvas->editSelected(changes);
}
// "Bauteileinheit aufheben": the part's values become the dialog's template, then the part is dissolved.
void Window::dissolveComponent(){
    const auto n=canvas->selectedNode();const auto e=n["type"]=="component"?project.componentNode(n):n;
    if(canvas->selectionCount()!=1||!e.contains("children")||!e["group_flags"].toArray().at(0).toBool()){QMessageBox::information(this,ui("Bauteileinheit aufheben"),ui("Ein Bauteil markieren."));return;}
    componentTemplate={{"id",e["id"]},{"value",e["value"]},{"description",e["description"]},{"label",e["label"]},{"listed",e["group_flags"].toArray().at(1).toBool(true)},{"extra",e["extra"]}};
    try{canvas->groupSelected(true);}catch(const std::exception &ex){QMessageBox::warning(this,ui("Bauteileinheit aufheben"),QString::fromUtf8(ex.what()));}
}
// Like the original, a loaded board brings its main view's switches (Wenden, BMP, Röntgen, Durchsicht); S/W stays a
// program setting.
void Window::applyStoredView(){
    const auto stored=project.mainView();auto state=canvas->viewState();state.flip=stored.flip;state.bitmaps=stored.bitmaps;state.xray=stored.xray;state.through=stored.through;
    for(auto [name,on]:std::initializer_list<std::pair<const char*,bool>>{{"Wenden",state.flip},{"Durchsicht",state.through},{"Röntgenblick",state.xray},{"BMP-Rendering",state.bitmaps}})
        if(auto *a=viewSwitches.value(name)){QSignalBlocker quiet(a);a->setChecked(on);}
    if(potentialsAction){QSignalBlocker quiet(potentialsAction);potentialsAction->setChecked(stored.potentials);}canvas->setShowPotentials(stored.potentials);
    canvas->setViewState(state);if(auto *a=unitActions.value(project.unit()))a->trigger();
    refreshLibraryPreviews();
}
// The library shows its parts as the board does: with BMP-Rendering or in average colours, as in the original.
void Window::refreshLibraryPreviews(){
    if(!libraryParts||!libraryPages||canvas->viewState().bitmaps==libraryBitmaps)return;
    const int row=libraryParts->currentRow(),scroll=libraryParts->verticalScrollBar()->value();
    showLibraryPage(libraryPages->currentIndex());libraryParts->setCurrentRow(row);libraryParts->verticalScrollBar()->setValue(scroll);
}
// The main view and unit belong to the board; they are kept with it for saving and when changing boards.
void Window::keepMainView(){
    const auto state=canvas->viewState();Project::MainView view;view.flip=state.flip;view.bitmaps=state.bitmaps;view.xray=state.xray;view.through=state.through;view.potentials=canvas->potentialsShown();
    project.storeMainView(view,canvas->unit());
}
void Window::fillObjectTree(){
    if(!objectTree||!treeDock||!treeDock->isVisible()||treeChoosing)return;QSignalBlocker quiet(objectTree);objectTree->clear();
    struct Entry{double z;QString kind;int index;QJsonObject node;};QList<Entry> entries;
    for(int i=0;i<project.legacy["objects"].toArray().size();i++){const auto n=project.legacyNode(i);if(n["deleted"].toBool())continue;entries.append({n["z"].toDouble(10+i*.001),"legacy",i,n});}
    for(int i=0;i<project.additions.size();i++){const auto stored=project.additions[i].toObject();entries.append({stored["z"].toDouble(50+i*.001),"new",i,stored["type"]=="component"?project.componentNode(stored):stored});}
    std::stable_sort(entries.begin(),entries.end(),[](const Entry &a,const Entry &b){return a.z>b.z;}); // topmost first, like the original
    // Each item knows its top-level object and, inside it, the path of child indices ("2/0").
    std::function<void(QTreeWidgetItem*,const QJsonObject&,const Entry&,const QString&)> add=[&](QTreeWidgetItem *parent,const QJsonObject &n,const Entry &e,const QString &path){
        const auto [name,detail]=objectDescription(n);const QString text=detail.isEmpty()?name:name+" "+detail;auto *item=parent?new QTreeWidgetItem(parent,{text}):new QTreeWidgetItem(objectTree,{text});
        item->setData(0,Qt::UserRole,e.kind);item->setData(0,Qt::UserRole+1,e.index);item->setData(0,Qt::UserRole+2,path);
        const auto children=n["children"].toArray();for(int k=children.size()-1;k>=0;k--)add(item,children[k].toObject(),e,path.isEmpty()?QString::number(k):path+'/'+QString::number(k));
    };
    for(const auto &e:entries)add(nullptr,e.node,e,{});
    const auto selected=canvas->selectedObjects();
    for(int i=0;i<objectTree->topLevelItemCount();i++){auto *item=objectTree->topLevelItem(i);for(const auto &[kind,index]:selected)if(item->data(0,Qt::UserRole)==kind&&item->data(0,Qt::UserRole+1).toInt()==index)item->setSelected(true);}
}
void Window::properties() {
    auto n=canvas->selectedNode();if(n.isEmpty()){QMessageBox::information(this,ui("Eigenschaften"),ui("Ein einzelnes Element auswählen."));return;}
    if(n["type"]=="potential"){QString name=n["name"].toString();QColor colour(n["color"].toString());if(editPotential(name,colour))canvas->editSelected({{"name",name},{"color",colour.name()}});return;}
    auto effective=n["type"]=="component"?project.componentNode(n):n;
    // Parts open the original's "Bauteil" dialog; a changed Kennung with "#" is numbered anew.
    if(effective.contains("children")&&(effective["group_flags"].toArray().isEmpty()||effective["group_flags"].toArray().at(0).toBool())){
        const QPointF before=canvas->selectedCentre();QPointF centre=before;const auto flags=effective["group_flags"].toArray();
        // The dialog shows the Kennung as stored, like the original ("R#" for R4).
        const auto selected=canvas->selectedObjects();const QString raw=selected.isEmpty()?effective["id"].toString():project.rawId(selected.first().first,selected.first().second);
        QJsonObject values{{"id",raw},{"value",effective["value"]},{"description",effective["description"]},{"label",effective["label"]},{"listed",flags.size()<2||flags.at(1).toBool()},{"extra",effective["extra"]}};
        // The terminals in the part's own order, without a Bezugspunkt chosen before.
        QJsonObject base;
        if(n["type"]=="component"){auto placement=n;placement.remove("reference");base=project.componentNode(placement);}else if(!selected.isEmpty()&&selected.first().first=="legacy")base=project.legacyNode(selected.first().second,false);
        QJsonArray terminals;for(const auto &t:partTerminals(base))terminals.append(QJsonArray{t.x(),t.y()});values["terminals"]=terminals;values["reference"]=effective["reference"].toInt(-1);
        if(!componentDialog(values,true,&centre))return;const auto id=values["id"].toString();
        QJsonObject changes{{"label",values["label"]},{"description",values["description"]},{"id",id},{"value",values["value"]},{"group_flags",QJsonArray{true,values["listed"].toBool()}},{"extra",values["extra"]}};
        if(id!=raw&&id.contains('#'))changes["group_value"]=project.nextNumber(id);
        if(!terminals.isEmpty()&&values["reference"].toInt(-1)!=effective["reference"].toInt(-1))changes["reference"]=values["reference"].toInt(-1);
        canvas->editSelected(changes,QLineF(centre,before).length()>=1?centre-before:QPointF());
        return;
    }
    if(effective.contains("children")){groupProperties();return;}
    QDialog dlg(this);dlg.setWindowTitle(ui("Eigenschaften"));QFormLayout layout(&dlg);
    bool component=effective.contains("children"),legacy=effective["type"].toString().startsWith('T');
    QLineEdit id(effective["id"].toString()),value(effective["value"].toString()),description(effective["description"].toString()),text(effective["text"].toString());
    // LochMaster's component dialog: identifier, value, description, parts-list flag and the centre position.
    auto flags=effective["group_flags"].toArray();QCheckBox listed(ui("Erscheint in Stückliste"));listed.setChecked(flags.size()<2||flags.at(1).toBool());
    if(component){layout.addRow(ui("Kennung"),&id);layout.addRow(ui("Wert / Typ"),&value);layout.addRow(ui("Beschreibung"),&description);layout.addRow("",&listed);}else if(effective.contains("text"))layout.addRow(ui("Text"),&text);
    const QPointF centre=canvas->selectedCentre();QDoubleSpinBox centreX,centreY;
    for(auto *box:{&centreX,&centreY}){box->setRange(-10000,10000);box->setDecimals(2);box->setSuffix(" mm");}centreX.setObjectName("centreX");centreY.setObjectName("centreY");
    centreX.setValue(centre.x()/100);centreY.setValue(centre.y()/100);if(component){layout.addRow(ui("Mitte X"),&centreX);layout.addRow(ui("Mitte Y"),&centreY);}
    bool hasText=!component&&effective.contains("text"),hasWidth=!component&&(effective.contains("path")||QStringList{"wire","rectangle","ellipse","polygon","polyline","pin","lead","solder"}.contains(effective["type"].toString())),hasDiameter=effective.contains("diameter")||effective["type"]=="drill";
    QDoubleSpinBox width,diameter,textSize;
    width.setRange(0,100);width.setDecimals(2);width.setSuffix(" mm");width.setValue(effective["width"].toDouble()/100);if(hasWidth)layout.addRow(ui("Linienbreite"),&width);
    diameter.setRange(.01,100);diameter.setDecimals(2);diameter.setSuffix(" mm");diameter.setValue(effective["diameter"].toDouble(.9));if(hasDiameter)layout.addRow(ui("Durchmesser"),&diameter);
    textSize.setRange(.2,20);textSize.setDecimals(2);textSize.setSuffix(" mm");textSize.setValue(legacy?effective["text_kind"].toDouble()/100:effective["textSize"].toDouble(120)/100);if(hasText)layout.addRow(ui("Schriftgröße"),&textSize);
    quint32 pen=quint32(effective["pen"].toInteger());QColor color=legacy?QColor(pen&255,(pen>>8)&255,(pen>>16)&255):QColor(effective["color"].toString("#28624d"));
    QPushButton colorButton(color.name());if(hasText||hasWidth)layout.addRow(ui("Farbe"),&colorButton);
    connect(&colorButton,&QPushButton::clicked,&dlg,[&]{auto chosen=QColorDialog::getColor(color,&dlg,ui("Farbe wählen"));if(chosen.isValid()){color=chosen;colorButton.setText(color.name());}});
    QCheckBox filled(ui("Fläche füllen")),back(ui("Kupferseite"));filled.setChecked(effective["filled"].toBool(effective["transparent"].toBool()));back.setChecked(effective.value("back").toBool(effective["type"]=="wire"));
    if(hasWidth)layout.addRow(ui("Füllung"),&filled);layout.addRow(ui("Seite"),&back);
    QDoubleSpinBox angle;angle.setRange(-360,360);angle.setSuffix("°");angle.setValue(n["angle"].toDouble());layout.addRow(ui("Drehung"),&angle);
    QCheckBox horizontal(ui("Horizontal")),vertical(ui("Vertikal"));horizontal.setChecked(n["mirrorX"].toBool());vertical.setChecked(n["mirrorY"].toBool());layout.addRow(ui("Spiegeln"),&horizontal);layout.addRow("",&vertical);
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&dlg,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
    if(dlg.exec()!=QDialog::Accepted)return;QJsonObject changes{{"angle",angle.value()},{"mirrorX",horizontal.isChecked()},{"mirrorY",vertical.isChecked()}};
    QPointF move;
    if(component){changes["id"]=id.text();changes["value"]=value.text();changes["description"]=description.text();
        changes["group_flags"]=QJsonArray{flags.isEmpty()||flags.at(0).toBool(),listed.isChecked()};
        const QPointF target(std::round(centreX.value()*100),std::round(centreY.value()*100));if(QLineF(target,centre).length()>=1)move=target-centre;}
    if(hasText){changes["text"]=text.text();changes[legacy?"text_kind":"textSize"]=textSize.value()*100;}
    if(hasWidth)changes["width"]=width.value()*100;if(hasDiameter)changes["diameter"]=diameter.value();
    if(hasWidth)changes["filled"]=filled.isChecked();changes["back"]=back.isChecked();
    if(hasText||hasWidth){if(legacy)changes["pen"]=qint64(color.red()|(color.green()<<8)|(color.blue()<<16));else changes["color"]=color.name();}
    canvas->editSelected(changes,move);
}
void Window::editNodes(){
    auto points=canvas->selectedPath();if(points.isEmpty()){QMessageBox::information(this,ui("Konturknoten"),ui("Eine Leitung oder Kontur auswählen. Bibliotheksgruppen können über ihre einzelnen Konturen bearbeitet werden."));return;}
    QDialog dialog(this);dialog.setWindowTitle(ui("Konturknoten · Objektkoordinaten in mm"));QVBoxLayout layout(&dialog);QTableWidget table(points.size(),2);table.setHorizontalHeaderLabels({"X (mm)","Y (mm)"});table.horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for(int row=0;row<points.size();row++)for(int col=0;col<2;col++)table.setItem(row,col,new QTableWidgetItem(QString::number(points[row].toArray()[col].toDouble()/100,'f',3)));layout.addWidget(&table);
    QHBoxLayout operations;QPushButton add(ui("Knoten hinzufügen")),remove(ui("Knoten löschen"));operations.addWidget(&add);operations.addWidget(&remove);layout.addLayout(&operations);
    connect(&add,&QPushButton::clicked,&dialog,[&]{int row=table.rowCount();table.insertRow(row);for(int c=0;c<2;c++)table.setItem(row,c,new QTableWidgetItem(row?table.item(row-1,c)->text():"0"));});
    connect(&remove,&QPushButton::clicked,&dialog,[&]{if(table.currentRow()>=0)table.removeRow(table.currentRow());});
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addWidget(&buttons);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{try{QJsonArray path;for(int r=0;r<table.rowCount();r++){QJsonArray p;for(int c=0;c<2;c++){bool ok=false;double value=table.item(r,c)->text().toDouble(&ok);if(!ok)throw FormatError(ui("Koordinaten als Zahlen eingeben."));p.append(value*100);}path.append(p);}canvas->editPath(path);dialog.accept();}catch(const std::exception &e){QMessageBox::warning(&dialog,ui("Kontur ungültig"),QString::fromUtf8(e.what()));}});dialog.resize(380,480);dialog.exec();
}
namespace {
// Closing or "Layout verwerfen" asks first, like the original.
class LayoutDialog final:public QDialog{
public:
    using QDialog::QDialog;std::function<bool()> mayClose;
    void reject() override{if(mayClose&&!mayClose())return;QDialog::reject();}
};
}
void Window::editLayout(){
    if(project.mode!="board"){QMessageBox::information(this,ui("Layout bearbeiten"),ui("Das Layout gehört zu einer Lochrasterplatine."));return;}
    Project layout;try{layout=project.layoutProject();}catch(const std::exception &e){QMessageBox::warning(this,ui("Layout bearbeiten"),QString::fromUtf8(e.what()));return;}
    LayoutDialog dialog(this);dialog.setObjectName("layoutEditor");dialog.setWindowTitle(ui("Layout bearbeiten"));dialog.resize(1000,720);
    auto *editor=new Canvas(&dialog);editor->setObjectName("layoutCanvas");History history;bool modified=false;
    editor->beforeChange=[&]{history.begin(layout);modified=true;};editor->changed=[&]{history.commit(layout);};
    auto show=[&]{editor->setProject(&layout);editor->fit();};
    auto *bar=new QToolBar(&dialog);auto *layoutBox=new QVBoxLayout(&dialog);layoutBox->addWidget(bar);layoutBox->addWidget(editor,1);
    // Tool values asked once when a tool is chosen, as in the original; the drill dialog follows every drill.
    double padDiameter=2,padDrill=.8,trackWidth=2,drillDiameter=.8;
    editor->toolDefaults["rectangle"]=QJsonObject{{"width",50},{"filled",true},{"color","#c19c66"}};editor->toolDefaults["polygon"]=editor->toolDefaults["rectangle"];
    editor->confirmNew=[&](QJsonObject &o){
        if(o["type"]!="drill")return true;
        QDialog d(&dialog);d.setWindowTitle(ui("Bohrung"));QFormLayout f(&d);QDoubleSpinBox size,x,y;size.setObjectName("drillDiameter");size.setRange(.1,50);size.setDecimals(2);size.setSuffix(" mm");size.setValue(drillDiameter);
        for(auto *v:{&x,&y}){v->setRange(-10000,10000);v->setDecimals(2);v->setSuffix(" mm");}x.setValue(o["x"].toDouble()/100);y.setValue(o["y"].toDouble()/100);
        f.addRow(ui("Durchmesser"),&size);f.addRow(ui("Mitte X"),&x);f.addRow(ui("Mitte Y"),&y);QDialogButtonBox b(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);f.addRow(&b);
        connect(&b,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&b,&QDialogButtonBox::rejected,&d,&QDialog::reject);
        if(d.exec()!=QDialog::Accepted)return false;drillDiameter=size.value();o["diameter"]=drillDiameter;o["x"]=x.value()*100;o["y"]=y.value()*100;return true;
    };
    auto *tools=new QActionGroup(&dialog);
    const QList<std::pair<QString,QString>> entries{{ui("Auswahl"),"select"},{ui("Rechteckige Kupferfläche"),"rectangle"},{ui("Geschlossene Kupferfläche (Polygon)"),"polygon"},{ui("Bohrung zeichnen"),"drill"},
        {ui("Rundes Lötauge zeichnen"),"eye"},{ui("Leiterbahn zeichnen"),"track"},{ui("Rundes Lötauge mit Bohrung zeichnen"),"pad"}};
    for(const auto &[label,type]:entries){
        auto *a=bar->addAction(label);a->setCheckable(true);a->setChecked(type=="select");tools->addAction(a);const QString chosen=type;
        connect(a,&QAction::triggered,&dialog,[&,chosen]{
            bool ok=false;
            if(chosen=="eye"){const double v=QInputDialog::getDouble(&dialog,ui("Rundes Lötauge"),ui("Durchmesser [mm]"),padDiameter,.1,50,2,&ok);if(ok)padDiameter=v;editor->toolDefaults["eye"]=QJsonObject{{"diameter",padDiameter}};}
            if(chosen=="track"){const double v=QInputDialog::getDouble(&dialog,ui("Leiterbahn"),ui("Breite [mm]"),trackWidth,.1,50,2,&ok);if(ok)trackWidth=v;editor->toolDefaults["track"]=QJsonObject{{"width",trackWidth*100}};}
            if(chosen=="pad"){
                QDialog d(&dialog);d.setWindowTitle(ui("Lötauge mit Bohrung"));QFormLayout f(&d);QDoubleSpinBox eye,hole;for(auto *v:{&eye,&hole}){v->setRange(.1,50);v->setDecimals(2);v->setSuffix(" mm");}
                eye.setValue(padDiameter);hole.setValue(padDrill);f.addRow(ui("Lötauge"),&eye);f.addRow(ui("Bohrung"),&hole);QDialogButtonBox b(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);f.addRow(&b);
                connect(&b,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&b,&QDialogButtonBox::rejected,&d,&QDialog::reject);
                if(d.exec()==QDialog::Accepted){padDiameter=eye.value();padDrill=hole.value();}editor->toolDefaults["pad"]=QJsonObject{{"diameter",padDiameter},{"drill",padDrill}};
            }
            editor->setTool(chosen);
        });
    }
    bar->addSeparator();
    auto *undoAction=bar->addAction(ui("Rückgängig"));connect(undoAction,&QAction::triggered,&dialog,[&]{if(history.undo(layout))show();});
    auto *redoAction=bar->addAction(ui("Wiederholen"));connect(redoAction,&QAction::triggered,&dialog,[&]{if(history.redo(layout))show();});
    bar->addSeparator();
    // Template files: a new empty board of 160 × 100 mm, LMB files to open and to save.
    connect(bar->addAction(ui("Neu")),&QAction::triggered,&dialog,[&]{
        Project blank;blank.mode="board";blank.width=16000;blank.height=10000;blank.title=ui("Neues Layout");
        layout=Project::fromLegacyBytes(writeLegacyDocument(blank,false,false,true),"lmb",ui("Neues Layout.LMB"));history.clear();modified=true;show();
    });
    connect(bar->addAction(ui("Öffnen…")),&QAction::triggered,&dialog,[&]{
        const QString templates=lochMaster.templates;
        const auto path=QFileDialog::getOpenFileName(&dialog,ui("Platinenvorlage öffnen"),QFileInfo(templates).isDir()?templates:startDirectory(),ui("LochMaster-Platine (*.LMB *.lmb)"));if(path.isEmpty())return;
        try{layout=Project::load(path);history.clear();modified=true;show();}catch(const std::exception &e){QMessageBox::warning(&dialog,ui("Platinenvorlage öffnen"),QString::fromUtf8(e.what()));}
    });
    connect(bar->addAction(ui("Speichern unter…")),&QAction::triggered,&dialog,[&]{
        const auto path=savePath(ui("Platinenvorlage speichern"),ui("NONAME"),ui("LochMaster-Platine (*.LMB)"));if(path.isEmpty())return;
        try{layout.title=QFileInfo(path).completeBaseName().left(35);layout.save(path);}catch(const std::exception &e){saveFailed(path,QString::fromUtf8(e.what()));}
    });
    bar->addSeparator();
    auto apply=[&]{canvas->beforeChange();project.applyLayout(layout);canvas->setProject(&project);canvas->changed();refresh();};
    auto *take=new QPushButton(ui("Layout übernehmen"));take->setObjectName("applyLayout");auto *drop=new QPushButton(ui("Layout verwerfen"));bar->addWidget(take);bar->addWidget(drop);
    connect(take,&QPushButton::clicked,&dialog,[&]{apply();modified=false;dialog.accept();});connect(drop,&QPushButton::clicked,&dialog,[&]{dialog.reject();});
    dialog.mayClose=[&]{
        if(!modified)return true;const auto answer=QMessageBox::question(&dialog,ui("Layout bearbeiten"),ui("Layout für Platine übernehmen?"),QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);
        if(answer==QMessageBox::Cancel)return false;if(answer==QMessageBox::Yes)apply();return true;
    };
    show();dialog.exec();
}
// The original's "Platine → Eigenschaften": name, size, the pitch of the unit N and the grids of mm and inch, the origin
// (shown relative to the offset), the offset between board and copper, and the board's extra fields. One undo step;
// afterwards the whole board is shown, as the original zooms to it.
void Window::projectProperties() {
    QDialog dlg(this);dlg.setObjectName("boardDialog");dlg.setWindowTitle(ui("Eigenschaften"));auto *form=new QFormLayout(&dlg);
    QLineEdit title(project.title);title.setObjectName("boardName");form->addRow(ui("Name"),&title);
    auto spin=[&](const char *name,double value,double low,double high,int decimals){auto *box=new QDoubleSpinBox(&dlg);box->setObjectName(name);box->setRange(low,high);box->setDecimals(decimals);box->setValue(value);return box;};
    auto section=[&](const QString &text){form->addRow(new QLabel("<b>"+text+"</b>",&dlg));};
    section(ui("Abmessungen"));
    auto *width=spin("boardWidth",project.width/100,.5,1000,2),*height=spin("boardHeight",project.height/100,.5,1000,2);form->addRow(ui("Breite"),width);form->addRow(ui("Höhe"),height);
    section(ui("Rastermaße / Lineale"));
    auto *pitch=spin("boardPitch",project.pitch(),0,1000,3),*gridMm=spin("boardGridMm",project.gridMm(),0,1000,3),*gridInch=spin("boardGridInch",project.gridInch(),0,1000,3);
    form->addRow(ui("Lochabstand \"N\""),pitch);form->addRow(ui("Raster \"mm\""),gridMm);form->addRow(ui("Raster \"inch\""),gridInch);
    const QPointF origin=project.origin(),offset=project.offset();
    section(ui("Ursprung"));auto *originX=spin("boardOriginX",origin.x()/100,-10000,10000,2),*originY=spin("boardOriginY",origin.y()/100,-10000,10000,2);form->addRow("X",originX);form->addRow("Y",originY);
    section(ui("Versatz (Bord<->Kupfer)"));auto *offsetX=spin("boardOffsetX",offset.x()/100,0,5.08,2),*offsetY=spin("boardOffsetY",offset.y()/100,0,5.08,2);form->addRow("X",offsetX);form->addRow("Y",offsetY);
    auto fields=project.boardExtra();const auto sync=addExtraRows(form,dlg,fields);
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&dlg,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
    const QPointF shownOrigin(originX->value(),originY->value());
    if(dlg.exec()!=QDialog::Accepted)return;sync();
    double hole=pitch->value();if(hole<.1){QMessageBox::warning(this,ui("Raster"),ui("Das Rastermaß muss mindestens 0,1 mm sein."));hole=.1;}
    try{
        auto next=project;next.title=title.text();next.width=std::round(width->value()*100);next.height=std::round(height->value()*100);
        if(hole!=next.pitch())next.boardSettings["pitch"]=hole;
        if(gridMm->value()!=next.gridMm())next.boardSettings["gridMm"]=gridMm->value();if(gridInch->value()!=next.gridInch())next.boardSettings["gridInch"]=gridInch->value();
        if(fields!=next.boardExtra())next.boardSettings["extra"]=fields;
        // The origin stays where it was on the board unless it was changed here; then it counts from the old offset.
        const QPointF nextOffset(std::round(offsetX->value()*100),std::round(offsetY->value()*100)),delta=nextOffset-offset;next.setOffset(nextOffset);
        if(QPointF(originX->value(),originY->value())!=shownOrigin)next.userOrigin=QJsonArray{std::round(originX->value()*100)-delta.x(),std::round(originY->value()*100)-delta.y()};
        Project::decode(next.encode());canvas->beforeChange();project=std::move(next);canvas->setProject(&project);buildTools();canvas->changed();refresh();
    }catch(const std::exception &e){QMessageBox::warning(this,ui("Eigenschaften"),QString::fromUtf8(e.what()));}
}
void Window::buildTools() {
    auto old=drawingTools->actions();drawingTools->clear();qDeleteAll(old);delete drawingGroup;drawingGroup=new QActionGroup(drawingTools);auto *group=drawingGroup;group->setExclusive(true);
    // The original's tool palette from top to bottom, with its tool tips; OpenLoch's solder pad tool comes last.
    struct Tool {const char *label,*type,*tip;};
    QList<Tool> tools{{"Auswahl","select","Markieren, Bearbeiten, Verschieben, Ändern"},{"Lupe","zoom","Zoom (Lupe): Klick vergrößert, Rahmen ziehen zoomt auf den Bereich, Rechtsklick verkleinert"}};
    if(project.mode=="board")tools.append({{"Kontur","polyline","Linien zeichnen"},{"Ellipse","ellipse","Kreis zeichnen"},{"Rechteck","rectangle","Rechteck zeichnen"},{"Polygon","polygon","Geschlossenes Polygon zeichnen"},
        {"Text","text","Schaltung beschriften"},{"Leitung","wire","Drähte ziehen (Brücke mit beiden Enden eingelötet)"},{"Anschluss","pin","Anschluß-Pins setzen (z. B. IC-Pin)"},
        {"Anschlussdraht","lead","Bauteilanschlußdraht definieren (erstes Ende eingelötet)"},{"Lötstelle","solder","Lötstelle hinzufügen"},{"Trennstelle","cut","Leiterbahn auftrennen"},{"Bohrung","drill","Bohren"},
        {"Potenzial","potential","Potenzial hinzufügen"},{"Durchgang","continuity","Elektrische Verbindung prüfen (Durchgangstester)"},{"Ursprung","origin","Ursprung setzen (Umschalt: ohne Raster)"},{"Lötpunkt","pad","Lötpunkt setzen"}});
    else tools.append({{"Leitung","wire","Leitungen zeichnen"},{"Text","text","Schaltung beschriften"},{"Widerstand","resistor","Widerstand setzen"},{"Kondensator","capacitor","Kondensator setzen"},{"Diode","diode","Diode setzen"},{"Masse","ground","Masse setzen"}});
    int key=1;
    for(const auto &tool:tools){
        const QString type=tool.type;auto *a=drawingTools->addAction(openLochIcon("tool-"+type),ui(tool.label));a->setCheckable(true);a->setData(type);a->setChecked(type=="select");a->setToolTip(ui(tool.tip));
        if(key<=9)a->setShortcut(QKeySequence(QString::number(key)));key++;group->addAction(a);connect(a,&QAction::triggered,this,[this,type]{canvas->setTool(type);});
    }
    canvas->setTool("select");
}
void Window::checkShorts(bool reveal){
    if(project.mode!="board"){if(reveal)QMessageBox::information(this,ui("Kurzschlüsse prüfen"),ui("Die Prüfung ist für Platinen vorgesehen."));return;}
    const int previous=shortList->currentRow();auto model=continuityModel(project);shortFindings=model.shorts();
    {QSignalBlocker blocked(shortList);shortList->clear();
    for(const auto &s:shortFindings)new QListWidgetItem(QString(ui("Kurzschluss: %1 ↔ %2 · %3 leitende Teile")).arg(s.first,s.second).arg(s.chain.size()),shortList);
    if(shortFindings.isEmpty()){
        auto *none=new QListWidgetItem(ui("Keine Kurzschlüsse zwischen verschiedenen Potentialen."),shortList);none->setFlags(Qt::ItemIsEnabled);
        if(model.potentialMarkers()<2){auto *hint=new QListWidgetItem(ui("Geprüft werden Netze mit Potentialmarken. Mit dem Werkzeug „Potenzial“ mindestens zwei verschiedene Potentiale setzen."),shortList);hint->setFlags(Qt::ItemIsEnabled);}
    }}
    if(reveal){shortsDock->show();shortsDock->raise();}
    const int row=shortFindings.isEmpty()?-1:qBound(0,previous,int(shortFindings.size())-1);
    {QSignalBlocker blocked(shortList);shortList->setCurrentRow(row);}
    if(row<0)canvas->clearShort();else canvas->showShort(shortFindings[row].chain);
    statusBar()->showMessage(shortFindings.isEmpty()?QString(ui("Keine Kurzschlüsse gefunden")):QString(ui("%1 Kurzschluss/Kurzschlüsse zwischen Potentialen gefunden")).arg(shortFindings.size()),8000);
}
bool Window::schematicAvailable() const{return project.mode=="board"&&targets&&hasSchematic&&hasSchematic();}
// The comparison follows every change, undo and redo while its findings or the airwires are shown.
void Window::projectChanged(){
    const bool schematic=schematicAvailable();for(const char *name:{"compareSchematic","showAirwires","takeOverSchematic","assignPins","placeMissing"})if(auto *a=findChild<QAction*>(name))a->setEnabled(schematic);
    if(schematic&&((schematicDock&&schematicDock->isVisible())||(airwiresAction&&airwiresAction->isChecked())))compareWithSchematic(false);
    else if(!schematic&&canvas)canvas->setAirwires({});
}
// The board against the schematic of its project: parts missing or extra, pins to assign, nets open or joined.
void Window::compareWithSchematic(bool reveal){
    if(!schematicAvailable()){canvas->setAirwires({});return;}
    // A schematic the module cannot read: the dialog only when asked for, else the finding alone, as this runs after every change.
    try{schematicTargets=targetsForBoard(project,targets());}catch(const std::exception &e){
        const QString text=QString(ui("Der Schaltplan kann nicht gelesen werden: %1")).arg(QString::fromUtf8(e.what()));
        schematicCheck={};canvas->setAirwires({});{QSignalBlocker blocked(schematicList);schematicList->clear();schematicList->addItem(text);}
        if(reveal){schematicDock->show();schematicDock->raise();QMessageBox::warning(this,ui("Mit Schaltplan vergleichen"),text);}
        return;
    }
    schematicCheck=checkTargets(project,schematicTargets);const auto &c=schematicCheck;const auto &t=schematicTargets;
    auto pin=[&](const documents::TargetPin &p){const auto *component=t.component(p.component);return (component?component->designator:QString("?"))+'.'+p.pin;};
    auto net=[&](int n){const auto &x=t.nets[n];return x.name.isEmpty()?QString(ui("Netz an %1")).arg(x.pins.isEmpty()?QString("?"):pin(x.pins.first())):x.name;};
    auto at=[&](const documents::TargetPin &p)->std::optional<QPointF>{
        for(const auto &part:c.parts){const auto &target=t.components[part.target];if(target.id!=p.component)continue;
            for(int k=0;k<part.pins.size();k++)if(part.pins[k]>=0&&target.pins[part.pins[k]]==p.pin)return part.at[k];}
        return std::nullopt;
    };
    const int previous=schematicList->currentRow();const int problems=int(c.missing.size()+c.ambiguous.size()+c.unassigned.size()+c.open.size()+c.joined.size());
    {QSignalBlocker blocked(schematicList);schematicList->clear();
    auto add=[&](const QString &text,std::optional<QPointF> where={},const Project::Component *part=nullptr){
        auto *item=new QListWidgetItem(text,schematicList);if(where)item->setData(Qt::UserRole,*where);if(part)item->setData(Qt::UserRole+1,part->kind+'/'+QString::number(part->index));return item;};
    if(t.isEmpty())add(ui("Der Schaltplan enthält keine Bauteile."));
    else add(c.passed()?ui("Die Platine stimmt mit dem Schaltplan überein."):QString(ui("%1 Abweichung(en) vom Schaltplan")).arg(problems));
    for(int m:c.missing)add(QString(ui("Fehlt auf der Platine: %1 (%2)")).arg(t.components[m].designator,t.components[m].value));
    for(const auto &d:c.ambiguous)add(QString(ui("Kennung mehrfach auf der Platine: %1")).arg(d));
    for(const auto &p:c.unassigned)add(QString(ui("Anschlüsse zuordnen: %1 (im Schaltplan %2)")).arg(p.part.designator,t.components[p.target].pins.join(", ")),p.at.value(0),&p.part);
    // An open net as its pieces: pins joined on the board together, the pieces apart.
    for(const auto &o:c.open){
        QStringList pieces;for(const auto &piece:o.pieces){QStringList pins;for(const auto &p:piece)pins<<pin(p);pieces<<pins.join(", ");}
        const auto &name=t.nets[o.net].name;add(name.isEmpty()?QString(ui("Offen: %1")).arg(pieces.join(" | ")):QString(ui("Offen in %1: %2")).arg(name,pieces.join(" | ")),at(o.pins.first()));
    }
    for(const auto &j:c.joined)add(QString(ui("Verbunden: %1 mit %2")).arg(net(j.first),net(j.second)),j.at);
    for(const auto &p:c.extra)add(QString(ui("Nicht im Schaltplan: %1")).arg(p.designator.isEmpty()?ui("Bauteil ohne Kennung"):p.designator),pinPositions(project,p).value(0),&p);
    for(const auto &change:targetChanges(c,t))if(change.text)for(const auto &p:c.parts+c.unassigned)if(p.part.uid==change.uid)
        add(QString(ui("Im Schaltplan %1 (%2): %3 (%4) auf der Platine")).arg(change.designator,change.value,p.part.designator,p.part.value),p.at.value(0),&p.part);
    }
    {QSignalBlocker blocked(schematicList);schematicList->setCurrentRow(qMin(previous,schematicList->count()-1));}
    if(reveal){schematicDock->show();schematicDock->raise();}
    canvas->setAirwires(airwiresAction&&airwiresAction->isChecked()?c.airwires:QList<QLineF>{});
    if(reveal)statusBar()->showMessage(c.passed()?QString(ui("Die Platine stimmt mit dem Schaltplan überein.")):QString(ui("%1 Abweichung(en) vom Schaltplan")).arg(problems),8000);
}
// "Aus Schaltplan übernehmen": after a list of what changes, one undo step. Missing parts are placed by the user.
void Window::takeOverFromSchematic(){
    if(!schematicAvailable())return;
    documents::Targets t;try{t=targetsForBoard(project,targets());}catch(const std::exception &e){QMessageBox::warning(this,ui("Aus Schaltplan übernehmen"),QString::fromUtf8(e.what()));return;}
    const auto check=checkTargets(project,t);const auto changes=targetChanges(check,t);
    QStringList lines;
    for(const auto &change:changes)for(const auto &p:check.parts+check.unassigned)if(p.part.uid==change.uid){
        QStringList what;if(!change.component.isEmpty())what<<QString(ui("gehört zu %1 im Schaltplan")).arg(t.components[p.target].designator);
        if(change.text)what<<QString(ui("Kennung und Wert %1 (%2)")).arg(change.designator,change.value);
        if(!change.pins.isEmpty())what<<ui("Anschlüsse getauscht");
        lines<<QString("%1: %2").arg(p.part.designator.isEmpty()?ui("Bauteil ohne Kennung"):p.part.designator,what.join(" · "));
    }
    QStringList later;for(int m:check.missing)later<<QString(ui("Fehlt auf der Platine: %1 (%2) – Prüfen → Fehlende Bauteile setzen…")).arg(t.components[m].designator,t.components[m].value);
    for(const auto &p:check.unassigned)later<<QString(ui("Anschlüsse zuordnen: %1 (im Schaltplan %2)")).arg(p.part.designator,t.components[p.target].pins.join(", "));
    QDialog dialog(this);dialog.setObjectName("takeOverDialog");dialog.setWindowTitle(ui("Aus Schaltplan übernehmen"));QVBoxLayout layout(&dialog);
    QLabel intro(changes.isEmpty()?ui("Auf der Platine ist nichts zu übernehmen."):ui("Diese Bauteile der Platine übernehmen Kennung, Wert und Zuordnung aus dem Schaltplan:"));intro.setWordWrap(true);layout.addWidget(&intro);
    QListWidget list;list.setObjectName("takeOverChanges");list.addItems(lines);if(!lines.isEmpty())layout.addWidget(&list);
    QLabel rest(later.join("\n"));rest.setWordWrap(true);if(!later.isEmpty())layout.addWidget(&rest);
    QDialogButtonBox buttons(changes.isEmpty()?QDialogButtonBox::Close:QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addWidget(&buttons);
    connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted||changes.isEmpty())return;
    canvas->beforeChange();applyTargetChanges(project,changes);canvas->rebuild();canvas->changed();
    statusBar()->showMessage(QString(ui("%1 Bauteil(e) aus dem Schaltplan übernommen")).arg(changes.size()),8000);
}
// Names the pins of a part after its component's pins: the part marked, else the first one whose pins need it. The
// board shows the numbers of the part's connection points meanwhile.
void Window::assignPins(){
    if(!schematicAvailable())return;
    try{schematicTargets=targetsForBoard(project,targets());}catch(const std::exception &e){QMessageBox::warning(this,ui("Anschlüsse zuordnen"),QString::fromUtf8(e.what()));return;}
    schematicCheck=checkTargets(project,schematicTargets);
    const auto chosen=canvas->selectedObjects();const auto all=schematicCheck.parts+schematicCheck.unassigned;const TargetCheck::Part *part=nullptr;
    for(const auto &p:all)if(chosen.size()==1&&chosen[0].first==p.part.kind&&chosen[0].second==p.part.index){part=&p;break;}
    if(!part&&!schematicCheck.unassigned.isEmpty())for(const auto &p:all)if(p.part.uid==schematicCheck.unassigned.first().part.uid)part=&p;
    if(!part){QMessageBox::information(this,ui("Anschlüsse zuordnen"),ui("Ein Bauteil markieren, das zu einem Bauteil des Schaltplans gehört."));return;}
    const auto &target=schematicTargets.components[part->target];const auto p=*part;
    QDialog dialog(this);dialog.setObjectName("assignPinsDialog");dialog.setWindowTitle(QString(ui("Anschlüsse zuordnen – %1")).arg(p.part.designator));QFormLayout form(&dialog);
    QList<QComboBox*> boxes;QList<QPair<QPointF,QString>> names;
    for(int k=0;k<p.at.size();k++){
        auto *box=new QComboBox(&dialog);box->setObjectName(QString("pin%1").arg(k+1));box->addItems(target.pins);
        box->setCurrentIndex(p.pins.value(k,-1)>=0?p.pins[k]:qMin(k,int(target.pins.size())-1));boxes<<box;
        form.addRow(QString(ui("Anschluss %1")).arg(k+1),box);names<<qMakePair(p.at[k],QString::number(k+1));
    }
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form.addRow(&buttons);
    connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    canvas->setPinNames(names);const bool accepted=dialog.exec()==QDialog::Accepted;canvas->setPinNames({});
    if(!accepted)return;
    // A pin left at the one its own name stands for keeps that name: + and - of an electrolytic stay.
    QStringList pins;QSet<int> picked;const auto byName=assignedPins(p.part.pins,target.pins);
    for(int k=0;k<boxes.size();k++){
        const int index=boxes[k]->currentIndex();picked.insert(index);
        pins<<(k<p.part.pins.size()&&byName.value(k,-1)==index?p.part.pins[k]:target.pins.value(index));
    }
    if(picked.size()!=boxes.size()){QMessageBox::warning(this,ui("Anschlüsse zuordnen"),ui("Jeder Anschluss des Schaltplans darf nur einmal vorkommen."));return;}
    const bool renamed=pins!=p.part.pins,link=p.part.id!=target.id;if(!renamed&&!link)return;
    canvas->beforeChange();if(renamed)project.setPins(p.part.uid,pins);if(link)project.linkComponent(p.part.uid,target.id);canvas->rebuild();canvas->changed();
}
// "Fehlende Bauteile setzen": for each component missing on the board a part of the OpenLoch library whose pins fit,
// best first and changeable; the chosen ones beside the board, named and linked, in one undo step.
void Window::placeMissingParts(){
    if(!schematicAvailable())return;
    documents::Targets t;try{t=targetsForBoard(project,targets());}catch(const std::exception &e){QMessageBox::warning(this,ui("Fehlende Bauteile setzen"),QString::fromUtf8(e.what()));return;}
    const auto check=checkTargets(project,t);
    if(check.missing.isEmpty()){QMessageBox::information(this,ui("Fehlende Bauteile setzen"),ui("Auf der Platine fehlt kein Bauteil des Schaltplans."));return;}
    QList<LibraryChoice> library;for(const auto &file:openLibraryFiles)library+=openLibraryChoices(QFileInfo(file).fileName(),openLibraryPages.value(file));
    QMap<QString,QString> files;QMap<QString,QString> titles;
    for(const auto &file:openLibraryFiles){files.insert(QFileInfo(file).fileName(),file);titles.insert(QFileInfo(file).fileName(),openLibraryTitle(openLibraryPages.value(file),uiLanguage()!="de"));}
    QDialog dialog(this);dialog.setObjectName("placeMissingDialog");dialog.setWindowTitle(ui("Fehlende Bauteile setzen"));QVBoxLayout layout(&dialog);
    QLabel intro(ui("Diese Bauteile des Schaltplans fehlen auf der Platine. OpenLoch legt die gewählten Teile der OpenLoch-Bibliothek neben die Platine, benannt und zugeordnet:"));intro.setWordWrap(true);layout.addWidget(&intro);
    QFormLayout form;layout.addLayout(&form);QList<std::pair<int,QComboBox*>> rows;QList<QList<LibraryChoice>> offers;
    for(int m:check.missing){
        const auto &component=t.components[m];const auto fitting=fittingParts(component,library);offers<<fitting;
        auto *box=new QComboBox(&dialog);box->setObjectName("part-"+component.designator);box->addItem(fitting.isEmpty()?ui("Kein passendes Bauteil"):ui("Nicht setzen"));
        for(const auto &c:fitting)box->addItem(QString("%1 · %2%3").arg(titles.value(c.page),c.name,c.value.isEmpty()||c.value=="?"?QString():" ("+c.value+")"));
        // Preselected only a part of the same kind: pins that fit do not make a resistor a diode.
        box->setCurrentIndex(!fitting.isEmpty()&&sameKind(kindOf(component),fitting.first().id)?1:0);box->setEnabled(!fitting.isEmpty());
        form.addRow(QString("%1 %2 (%3)").arg(component.designator,component.value,component.pins.join(", ")),box);rows<<std::pair{m,box};
    }
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addWidget(&buttons);
    connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QList<std::pair<documents::TargetComponent,LibraryChoice>> chosen;QMap<QString,QByteArray> pages;
    for(int i=0;i<rows.size();i++){
        const int pick=rows[i].second->currentIndex()-1;if(pick<0||pick>=offers[i].size())continue;const auto &choice=offers[i][pick];
        if(!pages.contains(choice.page)){const auto file=files.value(choice.page);if(!ensureOpenLibraryPage(file))continue;QFile f(file);if(!f.open(QIODevice::ReadOnly))continue;pages.insert(choice.page,f.readAll());}
        chosen<<std::pair{t.components[rows[i].first],choice};
    }
    if(chosen.isEmpty())return;
    canvas->beforeChange();const auto added=placeBeside(project,chosen,pages);canvas->rebuild();canvas->changed();
    QList<std::pair<QString,int>> placed;QRectF area(0,0,project.width,project.height);
    for(int index:added)placed<<std::pair{QString("new"),index};canvas->selectObjects(placed);
    for(const auto &p:project.placedObjects())if(p.kind=="new"&&added.contains(p.index))area|=p.transform.mapRect(legacyBounds(p.node));
    canvas->fitArea(area);statusBar()->showMessage(QString(ui("%1 Bauteil(e) neben die Platine gelegt")).arg(added.size()),8000);
}
// A change in the Farben, Breite or Füllen toolbar: applied to the selection (one undo step) and to new objects.
void Window::setDrawStyle(int mode,const QVariant &value){
    if(mode==0)penWidth=value.toInt();else if(mode==1)penColour=value.value<QColor>();else if(mode==2)fillAreas=value.toBool();else brushColour=value.value<QColor>();
    canvas->applyStyle(mode,value);showDrawStyle();updateToolDefaults();
}
void Window::showDrawStyle(){
    auto swatch=[](const QColor &c){QPixmap pix(18,18);pix.fill(c);QPainter p(&pix);p.setPen(QColor(90,90,90));p.drawRect(0,0,17,17);return QIcon(pix);};
    if(auto *b=findChild<QToolButton*>("penSwatch"))b->setIcon(swatch(penColour));if(auto *b=findChild<QToolButton*>("brushSwatch"))b->setIcon(swatch(brushColour));
    if(auto *c=findChild<QComboBox*>("lineWidth")){QSignalBlocker quiet(c);c->setCurrentIndex(c->findData(penWidth));}
    if(auto *a=findChild<QAction*>("fillAreas")){QSignalBlocker quiet(a);a->setChecked(fillAreas);}
}
// New objects of the board tools take the toolbar style; schematics keep their own.
void Window::updateToolDefaults(){
    for(const char *tool:{"wire","lead","polyline","pin","rectangle","ellipse","polygon","text"})canvas->toolDefaults.remove(tool);if(project.mode!="board")return;
    const QJsonObject line{{"color",penColour.name()},{"width",penWidth}};auto area=line;area["fill"]=brushColour.name();area["filled"]=fillAreas;
    for(const char *tool:{"wire","lead","polyline","pin"})canvas->toolDefaults[tool]=line;for(const char *tool:{"rectangle","ellipse","polygon"})canvas->toolDefaults[tool]=area;
    canvas->toolDefaults["text"]=QJsonObject{{"color",penColour.name()}};
}
void Window::refresh() {
    fillObjectTree();updateToolDefaults();
    if(notesWindow&&notesWindow->isVisible()&&!notesPending&&!notesLoading&&(project.notesRtf.isEmpty()?rtfNotes(project.notes):project.notesRtf)!=notesShown)loadNotesEditor();
    setWindowTitle(project.title+ui("[*] — OpenLoch"));setWindowModified(dirty);if(titleChanged)titleChanged();
    if(shortsDock&&shortsDock->isVisible())checkShorts(false); // re-check after every change, undo and redo
    projectChanged();
    info->setText(QString(ui("%1  ·  %2 × %3 mm  ·  %4 neue Elemente%5")).arg(project.mode=="board"?ui("Lochraster"):ui("Schaltplan")).arg(project.width/100).arg(project.height/100).arg(project.additions.size()).arg(project.original.isEmpty()?"":ui("  ·  Import: ")+project.sourceName));
    if(boardList){QSignalBlocker blocked(boardList);while(boardList->count())boardList->removeTab(0);if(project.boards.isEmpty())boardList->addTab(project.title);else for(int i=0;i<project.boards.size();i++)boardList->addTab(i==project.activeBoard?project.title:project.boards[i].toObject()["title"].toString());boardList->setCurrentIndex(project.activeBoard);}
}
bool Window::maybeSave() {
    flushNotes();if(!dirty)return true;auto answer=QMessageBox::question(this,ui("Änderungen speichern?"),ui("Änderungen an „")+project.title+ui("“ speichern?"),QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);
    if(answer==QMessageBox::Cancel)return false;if(answer==QMessageBox::Discard){if(recoveryTimer)recoveryTimer->stop();QFile::remove(recoveryFile);return true;}return save();
}
// Boards are saved as LochMaster projects like in the original, OpenLoch's own format stays available; schematics
// only exist in that format. A file opened as LM4 or .openloch is saved back in place.
bool Window::save(bool as) {
    flushNotes();keepMainView();project.assignIds();   // changes go through the history, which gives them; this is the last guard
    if(saveHandler)return saveHandler(as);
    QString p=currentPath;const auto suffix=QFileInfo(p).suffix().toLower();const bool board=project.mode=="board";
    if(as||p.isEmpty()||!(suffix=="openloch"||(suffix=="lm4"&&board)))
        p=savePath(ui("Projekt speichern"),project.title,board?ui("Lochraster-Projekt (*.LM4);;OpenLoch-Projekt (*.openloch)"):ui("OpenLoch-Projekt (*.openloch)"));
    if(p.isEmpty())return false;
    try{project.save(p);currentPath=p;dirty=false;if(recoveryTimer)recoveryTimer->stop();QFile::remove(recoveryFile);refresh();return true;}catch(const std::exception &e){saveFailed(p,QString::fromUtf8(e.what()));return false;}
}
void Window::exportDocument(const QString &kind,bool selection){
    auto data=selection?canvas->selectionData():QByteArray{};if(selection&&data.isEmpty())return;
    flushNotes();keepMainView();
    auto path=kind=="lm4"?savePath(ui("LochMaster-Projekt exportieren"),project.title,ui("LochMaster-Projekt (*.LM4)"))
        :savePath(kind=="lib"?ui("Bauteilbibliothek speichern"):ui("Platinenvorlage speichern"),project.title+ui("-Export"),kind=="lib"?ui("LochMaster-Bibliothek (*.LIB)"):ui("LochMaster-Platine (*.LMB)"));
    if(path.isEmpty())return;
    if(QFileInfo(path).absoluteFilePath()==QFileInfo(currentPath).absoluteFilePath()){QMessageBox::warning(this,ui("Anderen Dateinamen wählen"),ui("Für den Export einen eigenen Dateinamen wählen."));return;}
    try{if(selection){QSaveFile file(path);auto bytes=writeLegacyDocument(Project::decode(data),true);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())throw FormatError(file.errorString());}else project.save(path);statusBar()->showMessage(ui("Datei exportiert: ")+QFileInfo(path).fileName(),8000);}
    catch(const std::exception &e){saveFailed(path,QString::fromUtf8(e.what()));}
}
// Like the original, opening an LM4 project writes what was loaded as <name>.OLD next to it, a copy to fall back on.
// Not in folders without write access, not inside a connected LochMaster installation, which stays untouched.
void Window::keepOpenedCopy(const QString &path){
    const QFileInfo info(path);if(info.suffix().compare("lm4",Qt::CaseInsensitive)!=0||qApp->property("openloch.testing").toBool())return;
    if(!lochMaster.root.isEmpty()&&info.absoluteFilePath().startsWith(QFileInfo(lochMaster.root).absoluteFilePath()+'/'))return;
    if(!QFileInfo(info.absolutePath()).isWritable())return;
    try{const auto bytes=writeLegacyProject(project);QSaveFile file(info.absoluteDir().filePath(info.completeBaseName()+".OLD"));if(file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size())file.commit();}
    catch(const std::exception &){}
}
// File dialogs open in the folder used last, at first in the home folder. A bare file name made Qt start them in the
// working directory, which is "/" for apps launched from the Finder, and saving into the disk root failed.
QString Window::startDirectory() const{return !lastDirectory.isEmpty()&&QFileInfo(lastDirectory).isDir()?lastDirectory:QDir::homePath();}
void Window::rememberDirectory(const QString &file){
    lastDirectory=QFileInfo(file).absolutePath();
    if(!qApp->property("openloch.testing").toBool())QSettings().setValue("dialogs/lastDirectory",lastDirectory);
}
static QString fileTitle(QString title){
    for(auto &c:title)if(c.unicode()<32||QStringLiteral("/\\:*?\"<>|").contains(c))c='-';
    title=title.trimmed();while(title.startsWith('.'))title.remove(0,1);
    return title.isEmpty()?QStringLiteral("OpenLoch"):title;
}
QString Window::savePath(const QString &caption,const QString &title,const QString &filter){
    static const QRegularExpression pattern("\\*\\.(\\w+)");
    auto suffix=[](const QString &f){return pattern.match(f).captured(1);};
    QString chosen;auto path=QFileDialog::getSaveFileName(this,caption,QDir(startDirectory()).filePath(fileTitle(title)+'.'+suffix(filter)),filter,&chosen);
    if(path.isEmpty())return {};
    // The image writer picks the format from the suffix, so a name typed without one has to get the filter's.
    QStringList known;for(auto m=pattern.globalMatch(filter);m.hasNext();)known<<m.next().captured(1).toLower();
    if(!known.contains(QFileInfo(path).suffix().toLower()))path+='.'+suffix(chosen.isEmpty()?filter:chosen);
    rememberDirectory(path);return path;
}
void Window::saveFailed(const QString &path,const QString &reason){
    auto text=QString(ui("„%1“ konnte nicht gespeichert werden.")).arg(QDir::toNativeSeparators(path));
    if(!reason.isEmpty())text+="\n\n"+reason;
    if(!QFileInfo(QFileInfo(path).absolutePath()).isWritable())text+=ui("\n\nIn diesen Ordner darf OpenLoch nicht schreiben. Bitte einen Ordner im Benutzerordner wählen, etwa Dokumente.");
    QMessageBox::warning(this,ui("Speichern fehlgeschlagen"),text);
}
bool Window::openPath(const QString &path) {
    try {
        auto loaded=Project::load(path);if(!maybeSave())return false;
        project=std::move(loaded);currentPath=path;dirty=false;history.clear();
        // An opened backup becomes its project again, as in the original (BAK → LM4).
        const QFileInfo info(path);if(info.suffix().compare("bak",Qt::CaseInsensitive)==0)currentPath=info.absoluteDir().filePath(info.completeBaseName()+(project.sourceKind=="lm4"?".LM4":".openloch"));
        canvas->setProject(&project);applyStoredView();buildTools();refresh();keepOpenedCopy(currentPath);return true;
    }catch(const std::exception &e){QMessageBox::warning(this,ui("Datei konnte nicht geöffnet werden"),QString::fromUtf8(e.what()));return false;}
}
// "AutoSpeichern": on or off and the interval in minutes, as in the original.
// The help in the language of the interface (English for other languages), in a window of its own.
void Window::showHelp(){
    auto *window=findChild<QDialog*>("helpWindow");
    if(!window){window=new QDialog(this);window->setObjectName("helpWindow");window->setWindowTitle(ui("OpenLoch – Hilfe"));window->resize(760,640);
        auto *layout=new QVBoxLayout(window);layout->setContentsMargins(0,0,0,0);auto *page=new QTextBrowser(window);page->setObjectName("helpPage");page->setOpenExternalLinks(true);layout->addWidget(page);
        page->setSource(QUrl(QString("qrc:/help/%1.html").arg(uiLanguage())));}
    window->show();window->raise();window->activateWindow();
}
void Window::autoSaveSettings(){
    QDialog d(this);d.setObjectName("autoSaveDialog");d.setWindowTitle(ui("AutoSpeichern"));QFormLayout form(&d);
    QCheckBox active(ui("Aktivieren"));active.setObjectName("autoSaveActive");active.setChecked(autoSaveEnabled);form.addRow(&active);
    QSpinBox minutes;minutes.setObjectName("autoSaveMinutes");minutes.setRange(1,60);minutes.setValue(autoSaveMinutes);form.addRow(ui("Intervall [min]"),&minutes);
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form.addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
    if(d.exec()!=QDialog::Accepted)return;autoSaveEnabled=active.isChecked();autoSaveMinutes=minutes.value();
    if(!qApp->property("openloch.testing").toBool()){QSettings().setValue("backup/active",autoSaveEnabled);QSettings().setValue("backup/interval",autoSaveMinutes);}
    restartAutoSave();
}
void Window::restartAutoSave(){autoSaveTimer->setInterval(autoSaveMinutes*60000);if(autoSaveEnabled)autoSaveTimer->start();else autoSaveTimer->stop();}
// The whole project as <Name>.BAK next to its file, like the original; a project never saved goes to NewProject.BAK in
// OpenLoch's documents folder. Only when something changed since; the project itself stays unsaved.
bool Window::saveBackup(){
    if(!dirty||saveHandler)return false;flushNotes();keepMainView();project.assignIds();
    QString target;
    if(!currentPath.isEmpty()){const QFileInfo info(currentPath);target=info.absoluteDir().filePath(info.completeBaseName()+".BAK");}
    else{auto folder=qApp->property("openloch.documentsDirectory").toString();if(folder.isEmpty()&&!qApp->property("openloch.testing").toBool())folder=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/OpenLoch";
        if(folder.isEmpty())return false;target=folder+"/NewProject.BAK";}
    try{
        QDir().mkpath(QFileInfo(target).absolutePath());const auto bytes=project.mode=="board"?writeLegacyProject(project):project.encode();
        QSaveFile f(target);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())throw FormatError(f.errorString());
        statusBar()->showMessage(QString(ui("Gesichert: %1")).arg(QDir::toNativeSeparators(target)),6000);return true;
    }catch(const std::exception &e){statusBar()->showMessage(QString(ui("AutoSpeichern fehlgeschlagen: %1")).arg(QString::fromUtf8(e.what())),10000);return false;}
}
bool Window::newProject(const QString &mode) {
    QDialog dlg(this);dlg.setWindowTitle(mode=="board"?ui("Neue Platine"):ui("Neuer Schaltplan"));QFormLayout layout(&dlg);
    QDoubleSpinBox width,height;width.setRange(5,1000);height.setRange(5,1000);width.setValue(mode=="board"?100:297);height.setValue(mode=="board"?80:210);width.setSuffix(" mm");height.setSuffix(" mm");layout.addRow(ui("Breite"),&width);layout.addRow(ui("Höhe"),&height);
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&dlg,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
    if(dlg.exec()!=QDialog::Accepted||!maybeSave())return false;project=Project();project.mode=mode;project.title=mode=="board"?ui("Neue Platine"):ui("Neuer Schaltplan");project.width=width.value()*100;project.height=height.value()*100;
    dirty=false;currentPath.clear();history.clear();canvas->setProject(&project);applyStoredView();buildTools();refresh();return true;
}
QJsonObject Window::documentData(){flushNotes();keepMainView();project.assignIds();return QJsonDocument::fromJson(project.encode()).object();}
void Window::markSaved(){dirty=false;if(recoveryTimer)recoveryTimer->stop();QFile::remove(recoveryFile);refresh();}
void Window::setDocumentData(const QJsonObject &data){
    auto loaded=Project::decode(QJsonDocument(data).toJson(QJsonDocument::Compact));
    project=std::move(loaded);currentPath.clear();dirty=false;history.clear();
    canvas->setProject(&project);applyStoredView();buildTools();refresh();
}
void Window::undo(){if(history.undo(project)){dirty=true;canvas->setProject(&project);buildTools();refresh();recoveryTimer->start();}}
void Window::redo(){if(history.redo(project)){dirty=true;canvas->setProject(&project);buildTools();refresh();recoveryTimer->start();}}
void Window::exportBom() {
    auto p=savePath(ui("Stückliste speichern"),project.title,"CSV (*.csv)");if(p.isEmpty())return;
    QSaveFile f(p);auto data=billOfMaterialsTable(';').toUtf8();if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit())saveFailed(p,f.errorString());
}
// The original's Excel table: the board first (PCB, Platine, size), then every listed part in document order with
// Kennung, Name, Wert/Typ, Beschreibung, "Teil von" and one column per extra field name. CSV cells are quoted.
QString Window::billOfMaterialsTable(QChar separator) const{
    const auto parts=project.billOfMaterials();QStringList fields;
    for(auto v:parts)for(auto f:v.toObject()["extra"].toArray()){const auto name=f.toArray().at(0).toString();if(!fields.contains(name))fields<<name;}
    auto cell=[separator](QString s){if(separator=='\t')return s.replace('\t',' ').replace('\n',' ');s.replace('"',"\"\"");if(s.startsWith('=')||s.startsWith('+')||s.startsWith('-')||s.startsWith('@'))s.prepend('\'');return '"'+s+'"';};
    QStringList header{ui("Kennung"),ui("Name"),ui("Wert/Typ"),ui("Beschreibung"),ui("Teil von")};header+=fields;QString out;
    auto row=[&](const QStringList &cells){QStringList quoted;for(const auto &c:cells)quoted<<cell(c);out+=quoted.join(separator)+'\n';};row(header);
    const auto size=QLocale(QLocale::German).toString(project.width/100,'g',6)+" x "+QLocale(QLocale::German).toString(project.height/100,'g',6)+" mm";
    QStringList board{"PCB",ui("Platine"),size,project.title,""};for(int i=0;i<fields.size();i++)board<<"";row(board);
    for(auto v:parts){const auto o=v.toObject();QStringList cells{o["id"].toString(),o["name"].toString(),o["value"].toString(),o["description"].toString(),o["partOf"].toString()};
        for(const auto &name:fields){QString value;for(auto f:o["extra"].toArray())if(f.toArray().at(0).toString()==name)value=f.toArray().at(2).toString();cells<<value;}row(cells);}
    return out;
}
void Window::closeEvent(QCloseEvent *e){if(maybeSave())e->accept();else e->ignore();}
}
