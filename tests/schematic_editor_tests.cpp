// Tests of the schematic editor window: the first example worked through the interface as a user would.
#include "language.h"
#include "modules/schematic/editor.h"
#include "modules/schematic/example.h"
#include "modules/schematic/dialogs.h"
#include "formats/splan/splan.h"
#include "modules/schematic/dimension.h"
#include "modules/schematic/emf.h"
#include "modules/schematic/images.h"
#include "modules/schematic/library.h"
#include "modules/schematic/nets.h"
#include "modules/schematic/print.h"
#include "modules/schematic/properties.h"
#include "modules/schematic/render.h"
#include "modules/schematic/text.h"
#include "modules/schematic/zip.h"
#include "modules/schematic/search.h"
#include <QNativeGestureEvent>
#include <QWheelEvent>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QClipboard>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFontComboBox>
#include <QFileInfo>
#include <QImage>
#include <QGuiApplication>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QBuffer>
#include <QCryptographicHash>
#include <QLabel>
#include <QSettings>
#include <QTimer>
#include <QKeyEvent>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QPushButton>
#include <QRadioButton>
#include <QTabBar>
#include <QTreeWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QKeySequenceEdit>
#include <QToolButton>
#include <QToolBar>
#include <QTextBrowser>
#include <QHeaderView>
#include <QDockWidget>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <tuple>

using namespace openloch;
using namespace openloch::schematic;

namespace {
void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
bool near(QPointF a,QPointF b,double eps=1e-6){return std::abs(a.x()-b.x())<eps&&std::abs(a.y()-b.y())<eps;}
// A click on the sheet at a position in millimetres.
void click(SheetView *view,QPointF mm,Qt::MouseButton button=Qt::LeftButton,Qt::KeyboardModifiers modifiers=Qt::NoModifier){
    const QPointF p=view->toPixel(mm);
    QMouseEvent press(QEvent::MouseButtonPress,p,view->mapToGlobal(p),button,button,modifiers);QApplication::sendEvent(view,&press);
    QMouseEvent release(QEvent::MouseButtonRelease,p,view->mapToGlobal(p),button,Qt::NoButton,modifiers);QApplication::sendEvent(view,&release);
}
void dragOn(SheetView *view,QPointF from,QPointF to,Qt::KeyboardModifiers modifiers=Qt::NoModifier){
    const QPointF a=view->toPixel(from),b=view->toPixel(to);
    QMouseEvent press(QEvent::MouseButtonPress,a,view->mapToGlobal(a),Qt::LeftButton,Qt::LeftButton,modifiers);QApplication::sendEvent(view,&press);
    for(int i=1;i<=4;i++){const QPointF p=a+(b-a)*i/4.;QMouseEvent m(QEvent::MouseMove,p,view->mapToGlobal(p),Qt::NoButton,Qt::LeftButton,modifiers);QApplication::sendEvent(view,&m);}
    QMouseEvent release(QEvent::MouseButtonRelease,b,view->mapToGlobal(b),Qt::LeftButton,Qt::NoButton,modifiers);QApplication::sendEvent(view,&release);
}
// The pointer moved without a button held.
void hover(SheetView *view,QPointF mm){
    const QPointF p=view->toPixel(mm);QMouseEvent m(QEvent::MouseMove,p,view->mapToGlobal(p),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(view,&m);
}
// A drag between widget positions in pixels (from the rulers).
void dragPixels(SheetView *view,QPointF a,QPointF b){
    QMouseEvent press(QEvent::MouseButtonPress,a,view->mapToGlobal(a),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view,&press);
    for(int i=1;i<=4;i++){const QPointF p=a+(b-a)*i/4.;QMouseEvent m(QEvent::MouseMove,p,view->mapToGlobal(p),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view,&m);}
    QMouseEvent release(QEvent::MouseButtonRelease,b,view->mapToGlobal(b),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(view,&release);
}
const Item *contactNamed(const Item &component,const QString &name){for(const auto *c:contacts(component))if(c->name==name)return c;return nullptr;}
QList<const Item*> parts(const Editor &e){return components(e.document().sheet());}
}

int schematicEditorTests(){
    // --- The window and its menus come from the module's table.
    {Editor e;e.resize(1400,900);e.show();
        const auto table=editorMenus();const auto menus=e.menuBar()->actions();
        require(menus.size()==table.size(),"one menu per entry of the table");
        for(int i=0;i<table.size();i++){
            require(menus[i]->text()==ui(table[i].first),"menu titles in the table's order");
            QStringList names;for(auto *a:menus[i]->menu()->actions())if(!a->isSeparator())names<<a->objectName();
            QStringList expected;for(const auto &n:table[i].second)if(!n.isEmpty())expected<<n;
            require(names==expected,"menu actions as in the table");
        }
        require(e.action("quit")&&e.action("quit")->objectName()=="quit","a quit action named quit");
        require(!menus[1]->isVisible(),"the component editor's menu only while a component is edited");
        require(e.windowTitle().contains(ui("Neuer Schaltplan"))&&!e.isModified(),"a new document");}

    // --- The first example through the interface.
    Editor e;e.resize(1400,900);e.show();QApplication::processEvents();
    SheetView *view=e.view();
    view->textRequested=[](Item &i){if(i.type==ItemType::NetLabel)i.text=QStringLiteral("SIG");else if(i.type==ItemType::Contact){i.name=QStringLiteral("4");i.text=QStringLiteral("4");}else i.text=QStringLiteral("Hallo");return true;};
    require(e.document().sheets.size()==1,"one sheet to start");
    // Two sheets of different sizes.
    e.insertSheets(1,1,false);
    require(e.document().sheets.size()==2&&e.document().activeSheet==1&&e.sheetTabs()->count()==2,"a second sheet");
    e.change([](Document &d){d.sheet().width=420;d.sheet().height=297;});
    e.switchSheet(0);require(e.document().activeSheet==0&&e.sheetTabs()->currentIndex()==0,"back on the first sheet");
    e.setTitleBlock(simpleTitleBlock(297,210));
    require(!e.document().sheets[0].titleBlock.items.isEmpty(),"a simple title block");
    // The symbol from the library, twice: once by clicking it and putting it down, once more.
    LibraryPanel *library=e.library();
    require(library->pages().size()>=30,"the built-in library pages");
    // The example symbol, asymmetric with numbered contacts, on a page of its own.
    {LibraryPage page;page.name=QStringLiteral("Beispiele");page.entries.append({QStringLiteral("Beispiel"),exampleSymbol(),{}});library->setPages({page});}
    auto place=[&](QPointF at){
        library->chosen(library->pages()[0].entries[0]);require(view->placing(),"the symbol follows the pointer");
        click(view,at);require(!view->placing(),"a click puts it down");
    };
    place(QPointF(50.8,63.5));place(QPointF(101.6,76.2));
    auto list=parts(e);
    require(list.size()==2&&near(list[0]->pos,{50.8,63.5})&&near(list[1]->pos,{101.6,76.2}),"two instances at their insertion points");
    require(list[0]->designator!=list[1]->designator&&list[0]->id!=list[1]->id,"numbered and with ids of their own");
    const QString u1=list[0]->id,u2=list[1]->id;
    // A conductor from contact 2 of the first to contact 1 of the second, drawn with the line tool.
    const QPointF from=pinPosition(*list[0],*contactNamed(*list[0],"2")),to=pinPosition(*list[1],*contactNamed(*list[1],"1"));
    e.action("toolLine")->trigger();require(view->tool()==SheetView::Tool::Line,"line tool");
    click(view,from);click(view,QPointF(to.x(),from.y()));click(view,to);click(view,to,Qt::RightButton);
    const Item &wire=e.document().sheet().items.last();
    require(wire.type==ItemType::Line&&wire.electrical&&wire.points.size()==3&&near(wire.points.first(),from)&&near(wire.points.last(),to),"the conductor ends at the contacts");
    click(view,to,Qt::RightButton);require(view->tool()==SheetView::Tool::Select,"a second right click goes back to the standard mode");
    const QString wireId=wire.id;
    // A text.
    e.action("toolText")->trigger();click(view,QPointF(50.8,40.64));e.action("toolSelect")->trigger();
    require(e.document().sheet().items.last().type==ItemType::Text&&e.document().sheet().items.last().text=="Hallo","a text");
    {const auto nets=deriveNets(e.document());const Net *n=netOf(nets,{0,u1,contactNamed(*parts(e)[0],"2")->id});
        require(n&&n->pins.size()==2,"the drawn conductor joins the two contacts");}

    // Change the second instance numerically in the properties: the first stays as it was.
    const Item firstBefore=*parts(e)[0];
    view->setSelection({u2});
    auto *x=e.properties()->findChild<QDoubleSpinBox*>("positionX");auto *y=e.properties()->findChild<QDoubleSpinBox*>("positionY");
    require(x&&y&&std::abs(x->value()-101.6)<1e-6,"the insertion point in the properties");
    x->setValue(114.3);
    require(near(parts(e)[1]->pos,{114.3,76.2}),"moved numerically");
    {const Item *w=findItem(e.document().sheet(),wireId);require(w&&near(w->points.last(),{114.3,76.2})&&near(w->points.first(),from),"the conductor follows on the rubber band");}
    // Turn and mirror it, as a user would with Ctrl+R and Ctrl+M.
    e.action("rotate")->trigger();e.action("mirror")->trigger();
    const Item &edited=*parts(e)[1];
    require(edited.mirrored&&std::abs(edited.rotation-270)<1e-9&&near(edited.pos,{114.3,76.2}),"turned about and mirrored at its insertion point");
    require(*parts(e)[0]==firstBefore,"the first instance is unchanged");
    {QStringList ids;for(const auto &k:edited.children)ids<<k.id+u'/'+k.name;QStringList start;for(const auto &k:library->pages()[0].entries[0].symbol.children)start<<k.name;
        require(ids.size()==start.size(),"contacts and texts stay with the instance");}
    // Undo and redo.
    const Document afterEdits=e.document();
    e.undo();e.undo();e.undo();
    require(near(parts(e)[1]->pos,{101.6,76.2})&&!parts(e)[1]->mirrored&&std::abs(parts(e)[1]->rotation)<1e-9,"three steps back");
    e.redo();e.redo();e.redo();
    require(e.document()==afterEdits,"and forward again");
    // Sheets: title, size and order.
    e.switchSheet(1);view->setSelection({});
    auto *name=e.properties()->findChild<QLineEdit*>("sheetName");require(name,"the sheet in the properties when nothing is selected");
    name->setText(QStringLiteral("Netzteil"));emit name->editingFinished();QApplication::processEvents();
    require(e.document().sheets[1].name=="Netzteil"&&e.sheetTabs()->tabText(1)=="2: Netzteil","sheet renamed");
    auto *height=e.properties()->findChild<QDoubleSpinBox*>("sheetHeight");height->setValue(420);
    require(std::abs(e.document().sheets[1].height-420)<1e-9,"sheet resized");
    const QString firstSheet=e.document().sheets[0].id;
    e.reorderSheets({1,0});
    require(e.document().sheets[0].name=="Netzteil"&&e.document().sheets[1].id==firstSheet&&e.document().activeSheet==0,"sheets reordered, the active one stays active");
    // Save, close and open: nothing lost.
    QTemporaryDir dir;const QString file=dir.filePath("Beispiel.olsch");QString error;
    require(e.saveFile(file,&error)&&!e.isModified()&&e.windowTitle().startsWith("Beispiel.olsch"),"saved");
    const Document saved=e.document();
    {Editor other;require(other.openFile(file,&error)&&other.document()==saved&&other.filePath()==file,"opened again without loss");}

    // The component editor: a contact added and the change undone by "Abbrechen", then kept.
    e.switchSheet(1);view->setSelection({u1});e.editComponent();
    require(e.editingComponent()&&e.action("toolContact")->isVisible()&&e.menuBar()->actions()[1]->isVisible(),"the component editor");
    const int before=int(view->items().size());
    e.action("toolContact")->trigger();click(view,QPointF(-2.54,2.54));
    require(view->items().size()==before+1&&view->items().last().type==ItemType::Contact&&view->items().last().hasPin,"a contact with its connection point");
    e.leaveComponentEditor(false);
    require(!e.editingComponent()&&*parts(e)[0]==firstBefore,"cancelled: the component as it was");
    view->setSelection({u1});e.editComponent();e.action("toolContact")->trigger();click(view,QPointF(-2.54,2.54));e.leaveComponentEditor(true);
    require(contacts(*parts(e)[0]).size()==4&&contacts(*parts(e)[1]).size()==3,"kept: only this instance has the new contact");

    // A component made from drawn elements, and dissolved again.
    e.action("toolRectangle")->trigger();dragOn(view,QPointF(200,100),QPointF(210,105));
    e.action("toolSelect")->trigger();
    const QString box=view->selection().value(0);require(!box.isEmpty()&&view->items().last().type==ItemType::Rectangle,"a rectangle drawn");
    const QPointF corner=bounds(view->items().last()).topLeft();
    e.makeComponent();
    const Item *made=nullptr;for(const auto *c:parts(e))if(c->id==view->selection().value(0))made=c;
    require(made&&made->children.size()==3&&near(made->pos,view->onGrid(corner)),"a component with designator and value texts, inserted at the grid");
    e.dissolveComponents();
    require(view->selection().size()==2,"dissolved into the rectangle and its designator");

    // Copy and paste: new ids, the original untouched.
    view->setSelection({u2});e.copySelection();e.pasteClipboard();require(view->placing(),"pasted elements follow the pointer");
    click(view,QPointF(150,150));
    require(parts(e).size()==3&&parts(e)[2]->id!=u2&&parts(e)[2]->designator==parts(e)[1]->designator,"a copy with ids of its own");

    // Title block mode edits the title block, not the circuit.
    e.setTitleBlockMode(true);
    const int titleItems=int(e.document().sheet().titleBlock.items.size());
    e.action("toolText")->trigger();click(view,QPointF(20,20));e.action("toolSelect")->trigger();
    require(e.document().sheet().titleBlock.items.size()==titleItems+1,"a text in the title block");
    e.setTitleBlockMode(false);
    require(!e.action("editTitleBlock")->isChecked(),"title block mode ended");

    // Net labels from the tools.
    e.action("toolNetLabel")->trigger();click(view,from);e.action("toolSelect")->trigger();
    require(e.document().sheet().items.last().type==ItemType::NetLabel&&e.document().sheet().items.last().text=="SIG","a net label");
    {const auto nets=deriveNets(e.document());require(!nets.isEmpty()&&nets[0].name=="SIG","the net takes the label's name");}

    // A host can take over opening: "Öffnen" calls it with the drawing folder of the settings instead of a dialog.
    {QString asked=QStringLiteral("none");int calls=0;GeneralSettings g;g.drawingFolder=QStringLiteral("/tmp/zeichnungen");e.applySettings(g);
        e.openHandler=[&](const QString &folder){calls++;asked=folder;return true;};
        e.action("open")->trigger();require(calls==1&&asked=="/tmp/zeichnungen","the host opens, from the drawing folder");
        g.drawingFolder.clear();e.applySettings(g);e.action("open")->trigger();require(calls==2&&asked.isEmpty(),"without a drawing folder an empty one");
        e.openHandler=nullptr;}
    // A host can take over saving: the editor calls it instead of writing a file.
    {int calls=0;bool asNew=false;QJsonObject taken;
        e.saveHandler=[&](bool fresh){calls++;asNew=fresh;taken=e.documentData();e.markSaved();return true;};
        e.change([](Document &d){d.sheet().description=QStringLiteral("Im Projekt");});require(e.isModified(),"changed before the host saves");
        require(e.save()&&calls==1&&!asNew&&!e.isModified()&&fromJson(taken).sheet().description=="Im Projekt","saving through the host");
        require(e.saveAs()&&calls==2&&asNew,"saving under a new name through the host");
        e.saveHandler=nullptr;}
    // Settings are not written without loadPreferences.
    e.setDocument(exampleDocument());
    // --- Own library pages: made, filled, ordered and emptied; the pages that come with OpenLoch stay as they are.
    {QTemporaryDir dir;LibraryPanel panel;panel.setOwnFolder(dir.filePath("Bibliothek"),QStringLiteral("Bibliothek"));panel.setPages(builtInPages());
        QString error;LibraryEntry sample{QStringLiteral("Beispiel"),exampleSymbol(),{}};
        require(!panel.writable(panel.currentPage())&&!panel.addEntries({sample},&error)&&!error.isEmpty(),"built-in pages cannot be changed");
        require(!panel.contextMenu(-1)->findChild<QAction*>("entryNew")->isEnabled(),"nor offered for it");
        require(panel.newPage(QStringLiteral("Meine Teile"),&error),"a new page");
        const QString file=panel.pages()[panel.currentPage()].file;
        require(panel.writable(panel.currentPage())&&QFileInfo(file).dir().dirName()=="Bibliothek"&&panel.pages()[panel.currentPage()].folder=="Bibliothek"&&QFileInfo::exists(file),"in the library folder, under its top folder");
        LibraryEntry other=sample;other.caption=QStringLiteral("Zweites");
        require(panel.addEntries({sample,other},&error)&&loadPage(file).entries.size()==2,"symbols added and saved");
        require(panel.moveEntry(0,1,&error)&&loadPage(file).entries[0].caption=="Zweites","moved");
        require(panel.duplicateEntry(0,&error)&&loadPage(file).entries.size()==3&&loadPage(file).entries[2].caption=="Zweites","duplicated to the end");
        require(panel.deleteEntry(2,&error)&&loadPage(file).entries.size()==2,"deleted");
        require(panel.newEntry(&error)&&loadPage(file).entries.last().symbol.type==ItemType::Component,"a new stand-in component");
        require(panel.renamePage(QStringLiteral("Umbenannt"),&error)&&loadPage(file).name=="Umbenannt"&&localized(panel.pages()[panel.currentPage()].name)=="Umbenannt","renamed");
        require(panel.contextMenu(0)->findChild<QAction*>("entryDuplicate")->isEnabled(),"the context menu on an own page");
        require(panel.copyPage(QStringLiteral("Kopie"),&error)&&panel.pages()[panel.currentPage()].entries.size()==3&&panel.pages()[panel.currentPage()].file!=file,"a page copied beside it");
        const QString copy=panel.pages()[panel.currentPage()].file;
        require(panel.emptyPage(&error)&&loadPage(copy).entries.isEmpty(),"emptied");
        require(panel.deletePage(&error)&&!QFileInfo::exists(copy),"deleted");
        // A subfolder with its first page, beside the current own page; a second of that name is refused.
        auto original=[&]{panel.showPage(panel.pageKey(int(std::find_if(panel.pages().begin(),panel.pages().end(),[&](const LibraryPage &p){return p.file==file;})-panel.pages().begin())));};
        original();
        require(panel.newFolder(QStringLiteral("Relais"),&error)&&QFileInfo(dir.filePath("Bibliothek/Relais")).isDir()&&panel.pages()[panel.currentPage()].folder=="Bibliothek/Relais"
                &&QFileInfo::exists(panel.pages()[panel.currentPage()].file),"a subfolder with a first page");
        {const auto again=folderPages(dir.filePath("Bibliothek"));bool found=false;for(const auto &p:again)found|=p.folder=="Relais";require(found,"read again from the folder");}
        original();require(!panel.newFolder(QStringLiteral("Relais"),&error)&&error.contains("Unterordner"),"not twice on one level");
        require(panel.contextMenu(-1)->findChild<QAction*>("folderNew")->isEnabled(),"offered in the menu");
        // Backup and restore as ZIP: the files of the library folder, read back after a page is lost.
        {const QString zip=dir.filePath("sicherung.zip");require(panel.backup(zip,&error)&&QFile(zip).size()>100,"the library folder backed up");
            QFile f(zip);require(f.open(QIODevice::ReadOnly),"the backup");const auto entries=readZip(f.readAll());
            bool page=false;for(const auto &en:entries)page|=en.path=="Relais/Neue Seite.olschlib";
            require(entries.size()>=2&&page,"with its files and folders");
            QFile::remove(file);int reloads=0;panel.reload=[&]{reloads++;};
            require(panel.restore(zip,&error)&&QFileInfo::exists(file)&&loadPage(file).name=="Umbenannt"&&reloads==1,"restored, the library read again");
            panel.reload=nullptr;
            QFile bad(dir.filePath("kaputt.zip"));require(bad.open(QIODevice::WriteOnly)&&bad.write("PK no zip")>0,"a broken file");bad.close();
            require(!panel.restore(dir.filePath("kaputt.zip"),&error)&&!error.isEmpty(),"a broken backup refused");}
        // A page of a sPlan library in the library folder: changed and written back into its file.
        {LibraryPage lib;lib.name=QStringLiteral("sPlan-Seite");lib.entries<<sample;
            const QString libFile=dir.filePath("Bibliothek/SEITE.LIB");
            {QFile f(libFile);require(f.open(QIODevice::WriteOnly)&&f.write(splan::writeLibrary(lib,{},80))>0,"cannot write the sPlan page");}
            LibraryPanel own;own.setOwnFolder(dir.filePath("Bibliothek"),QStringLiteral("Bibliothek"));
            QList<LibraryPage> pages=folderPages(dir.filePath("Bibliothek"));for(auto &p:pages)p.folder=p.folder.isEmpty()?QStringLiteral("Bibliothek"):QStringLiteral("Bibliothek/")+p.folder;
            own.setPages(pages);QStringList reported;own.lossesReported=[&](const QStringList &l){reported<<l;};
            int index=-1;for(int k=0;k<own.pages().size();k++)if(own.pages()[k].file==libFile)index=k;
            require(index>=0&&own.showPage(own.pageKey(index))&&own.writable(own.currentPage()),"a sPlan page can be changed");
            const QByteArray before=[&]{QFile f(libFile);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}();
            require(own.renamePage(QStringLiteral("Umbenannt"),&error)&&own.duplicateEntry(0,&error),"renamed, an entry duplicated");
            QFile f(libFile);require(f.open(QIODevice::ReadOnly),"the page");const LibraryPage back=splan::readLibrary(f.readAll());
            require(back.name=="Umbenannt"&&back.entries.size()==2&&back.entries[0].symbol.splan==own.pages()[own.currentPage()].entries[0].symbol.splan&&!before.isEmpty(),
                    "written into the sPlan file, the entries read again from it");}
        // A symbol's properties in the panel: its caption and fields kept on its page at once; a new selection ends it.
        {Editor y;y.library()->setOwnFolder(dir.filePath("Bibliothek"),QStringLiteral("Bibliothek"));y.library()->setPages(folderPages(dir.filePath("Bibliothek")));
            int own=-1;for(int k=0;k<y.library()->pages().size();k++)if(y.library()->pages()[k].file==file)own=k;
            require(own>=0&&y.library()->showPage(y.library()->pageKey(own)),"the own page");
            int index=-1;const auto &entries=y.library()->pages()[y.library()->currentPage()].entries;for(int k=0;k<entries.size();k++)if(entries[k].symbol.type==ItemType::Component)index=k;
            require(index>=0,"a component on the page");
            y.library()->editEntry(index,false);QApplication::processEvents();
            auto *caption=y.findChild<QLineEdit*>("libraryCaption");require(caption&&y.findChild<QLabel*>("libraryTitle"),"the symbol in the panel");
            caption->setText(QStringLiteral("Neue Unterschrift"));emit caption->editingFinished();QApplication::processEvents();
            require(loadPage(file).entries[index].caption=="Neue Unterschrift","the caption kept on the page");
            auto *value=y.findChild<QLineEdit*>("value");require(value&&value->isEnabled(),"its fields");value->setText(QStringLiteral("4k7"));emit value->editingFinished();QApplication::processEvents();
            require(loadPage(file).entries[index].symbol.value=="4k7"&&!y.isModified(),"a field kept on the page, the document unchanged");
            Item mark;mark.type=ItemType::Line;mark.points={QPointF(10,10),QPointF(20,10)};assignIds(mark);y.change([&](Document &d){d.sheet().items<<mark;});y.markSaved();
            y.view()->setSelection({mark.id});QApplication::processEvents();
            require(!y.findChild<QLabel*>("libraryTitle"),"a selection on the sheet ends it");
            y.library()->setPages(builtInPages());int builtIn=-1;
            for(int k=0;k<y.library()->pages().size()&&builtIn<0;k++)for(const auto &e:y.library()->pages()[k].entries)if(e.symbol.type==ItemType::Component){builtIn=k;break;}
            y.library()->showPage(y.library()->pageKey(builtIn));int first=0;while(y.library()->pages()[builtIn].entries[first].symbol.type!=ItemType::Component)first++;
            y.library()->editEntry(first,false);QApplication::processEvents();
            require(y.findChild<QLineEdit*>("libraryCaption")&&!y.findChild<QLineEdit*>("libraryCaption")->isEnabled(),"a page that cannot be changed only shows it");
            y.markSaved();}
        // From the sheet into the library, and the page onto new sheets.
        Editor x;x.library()->setOwnFolder(dir.filePath("Bibliothek"),QStringLiteral("Bibliothek"));x.library()->setPages(folderPages(dir.filePath("Bibliothek")));
        require(x.library()->showPage(x.library()->pageKey(0)),"an own page");
        const int before=int(x.library()->pages()[x.library()->currentPage()].entries.size());
        Item part=exampleSymbol();part.designator=QStringLiteral("U7");part.pos=QPointF(50.8,50.8);assignIds(part);
        x.change([&](Document &d){d.sheet().items<<part;});x.view()->setSelection({part.id});
        x.copyToLibrary(false);
        const auto &now=x.library()->pages()[x.library()->currentPage()].entries;
        require(now.size()==before+1&&now.last().symbol.designator=="U?"&&now.last().symbol.pos==QPointF(),"a component into the library, numbered anew");
        x.copyToLibrary(true);
        require(x.library()->pages()[x.library()->currentPage()].entries.last().symbol.type==ItemType::Group,"the selection as a clip");
        const int sheets=int(x.document().sheets.size());
        x.librarySheets({x.library()->currentPage()});
        require(x.document().sheets.size()==sheets+1&&x.document().sheets.last().items.size()==before+2&&components(x.document().sheets.last()).size()==before+2,"the page on a new sheet, the clip with its component");}
    // --- The parts list
    {require(designatorLess("R2","R10")&&!designatorLess("R10","R2")&&designatorLess("C9","R1")&&!designatorLess("R1","R1"),"designators with their numbers as numbers");
        Document d=newDocument(QStringLiteral("x"));d.sheets<<newSheet(QStringLiteral("Zwei"));
        auto part=[](const char *designator,const char *value,const char *extra=""){Item c=exampleSymbol();c.designator=QString::fromLatin1(designator);c.value=QString::fromLatin1(value);c.extra={QString::fromLatin1(extra)};assignIds(c);return c;};
        Item group;group.type=ItemType::Group;group.children<<part("R4","1k");assignIds(group);
        Item hidden=part("U1","LM358");hidden.inPartsList=false;
        d.sheets[0].items<<part("R2","1k")<<part("R10","10k")<<part("R1","1k")<<part("R3","1k","1%")<<part("C2","100n")<<hidden<<group;
        d.sheets[1].items<<part("C1","100n");
        auto show=[](const QList<PartsRow> &rows){QStringList out;for(const auto &r:rows)out<<r.designators.join(',')+'='+QString::number(r.count())+'x'+r.value;return out.join(' ');};
        // Merged as in the reference: the groups by their letters, in a group the single parts first, then the merged
        // rows, each in the order of their designators (not of their counts).
        require(show(partsList(d))=="C1,C2=2x100n R3=1x1k R10=1x10k R1,R2,R4=3x1k","merged, the groups by their letters, single parts first, in the order of their designators");
        {PartsListOptions o;o.sort=PartsListOptions::Sort::Frequency;require(show(partsList(d,o)).startsWith("R3=1x1k R10=1x10k R1,R2,R4=3x1k"),"the largest group first");}
        {PartsListOptions o;o.merge=false;o.sheets={0};require(show(partsList(d,o))=="C2=1x100n R1=1x1k R2=1x1k R3=1x1k R4=1x1k R10=1x10k","not merged, the chosen sheets only");}
        const PartsTable t=partsTable(partsList(d),{0});
        require(t.header.size()==4&&t.rows.size()==4&&t.rows[3]==QStringList({"R1,R2,R4","3","1k",""})&&t.rows[1][3]=="1%","a table with the chosen additional text");
        require(partsText(t,{0,1},u';',true,true)==QStringLiteral("%1;%2;%3;%4\nC1,C2;2;100n;\n\nR3;1;1k;1%\nR10;1;10k;\nR1,R2,R4;3;1k;\n").arg(ui("Bezeichner"),ui("Anzahl"),ui("Wert"),ui("Zusatztext %1").arg(1)),"as text with field names and empty lines between groups");
        // With sheet numbers and prefix (as in the reference): listed as shown, grouped and merged as entered.
        {Document q=newDocument(QStringLiteral("Eins"));q.sheets<<newSheet(QStringLiteral("Zwei"));q.designatorPageNumbers=true;q.designatorPrefix=QStringLiteral("X-");
            q.sheets[0].items<<part("R1","10k")<<part("R2","4k7")<<part("C1","100n");q.sheets[1].items<<part("R1","10k")<<part("R3","4k7");
            {PartsListOptions o;o.merge=false;require(show(partsList(q,o))=="X-1C1=1x100n X-1R1=1x10k X-1R2=1x4k7 X-2R1=1x10k X-2R3=1x4k7","not merged: as shown, in that order");}
            require(show(partsList(q))=="X-1C1=1x100n X-1R1,X-2R1=2x10k X-1R2,X-2R3=2x4k7","merged by value across sheets");
            {PartsListOptions o;o.sort=PartsListOptions::Sort::Frequency;require(show(partsList(q,o)).endsWith("X-1C1=1x100n"),"the groups by the letters as entered: R before C");}
            Document one=q;one.sheets[0].items.removeLast();one.sheets[1].items.removeLast();
            require(show(partsList(one))=="X-1R2=1x4k7 X-1R1,X-2R1=2x10k","the single part first, as in the reference");}
        const Item drawn=partsGroup(t,QPointF(20,20));
        int texts=0;for(const auto &c:drawn.children)texts+=c.type==ItemType::Text;
        require(drawn.type==ItemType::Group&&drawn.children[0].type==ItemType::Rectangle&&texts==4+4*3+1&&near(bounds(drawn).topLeft(),{20,20},.3),"drawn as a group of texts and lines with a frame");
        PartsListDialog dialog(d,{},QString());
        require(dialog.grid->rowCount()==4,"the dialog shows the list");
        dialog.merge->setChecked(false);require(dialog.grid->rowCount()==7,"made anew without merging");
        dialog.grid->item(0,2)->setText(QStringLiteral("220n"));require(dialog.table().rows[0][2]=="220n","changes by hand count");
        dialog.copyToClipboard();require(QGuiApplication::clipboard()->text().contains("220n"),"copied to the clipboard");
        {   // As Rich Text Format: written and read back, also from another program's table.
            PartsTable odd=t;odd.rows[0][2]=QString::fromUtf8("1µF {x} a\\b Ä");
            const QByteArray rtf=partsRtf(odd,{"Kopf","Zeile"});
            require(rtf.startsWith("{\\rtf1")&&partsFromRtf(rtf).header==odd.header&&partsFromRtf(rtf).rows==odd.rows,"the table written and read back, with special characters");
            const QByteArray foreign="{\\rtf1\\ansi{\\fonttbl{\\f0 Times;}}{\\colortbl;\\red0\\green0\\blue0;}{\\*\\generator x;}Text\\par\n"
                                     "\\trowd\\cellx1000\\cellx2000\\pard\\intbl A\\cell B\\cell\\row\\trowd\\cellx1000\\cellx2000\\pard\\intbl R1\\cell 4,7k \\'e4\\cell\\row}";
            const PartsTable f=partsFromRtf(foreign);
            require(f.header==QStringList({"A","B"})&&f.rows.size()==1&&f.rows[0]==QStringList({"R1",QString::fromUtf8("4,7k ä")}),"a table of another program");
            QTemporaryDir dir;const QString file=dir.filePath("liste.rtf");PartsListDialog other(d,{},QString());
            require(dialog.saveRtf(file)&&other.openRtf(file)&&other.table().rows==dialog.table().rows&&other.grid->rowCount()==dialog.grid->rowCount(),"saved and opened in the dialog");
            QFile empty(dir.filePath("leer.rtf"));require(empty.open(QIODevice::WriteOnly),"cannot write the RTF file");empty.write("{\\rtf1 nichts}");empty.close();
            QString error;require(!other.openRtf(dir.filePath("leer.rtf"),&error)&&!error.isEmpty(),"a file without a table is refused");}
        {   // On the sheet: sPlan's options, broken into tables side by side.
            PartsDrawing got;QString breaks;
            dialog.insert=[&](const PartsTable &,const PartsDrawing &dd){got=dd;};
            dialog.placementOptions=[&](QDialog *o){
                o->findChild<QRadioButton*>("manual")->setChecked(true);o->findChild<QSpinBox*>("rows")->setValue(3);o->findChild<QCheckBox*>("shadow")->setChecked(true);
                breaks=o->findChild<QLabel*>("breaks")->text();QTimer::singleShot(0,o,&QDialog::accept);};
            dialog.insertOnSheet();
            require(got.rowsPerColumn==3&&got.shadow&&std::abs(got.height-1.6)<1e-9&&breaks==ui("%1 Umbrüche").arg(2),"manual breaks after 3 of 7 rows, the options, text height 1,6 mm");
            PartsDrawing br;br.rowsPerColumn=2;const Item side=partsGroup(t,QPointF(),br);int frames=0;for(const auto &c:side.children)frames+=c.type==ItemType::Rectangle;
            require(frames==2&&bounds(side).width()>bounds(partsGroup(t,QPointF())).width(),"two tables side by side");
            require(fittingRows(210,PartsDrawing{})==int((210-20)/3.75)-1,"as many rows as fit into the sheet");}
        Editor x;const int before=int(x.document().sheet().items.size());
        x.insertPartsList(t,{});
        require(x.document().sheet().items.size()==before+1&&x.document().sheet().items.last().type==ItemType::Group&&x.view()->selection()==QStringList{x.document().sheet().items.last().id},"put onto the sheet and selected");}
    // --- Printing and PDF
    {const Document d=exampleDocument();   // a sheet A4 landscape and one A3 landscape
        const QPageLayout a4(QPageSize(QPageSize::A4),QPageLayout::Portrait,QMarginsF(5,5,5,5),QPageLayout::Millimeter);
        PrintSettings plain;
        require(pageFor(a4,d.sheets[0],plain).orientation()==QPageLayout::Landscape,"automatic orientation: like the sheet");
        {PrintSettings s;s.orientation=PrintSettings::Orientation::Portrait;require(pageFor(a4,d.sheets[0],s).orientation()==QPageLayout::Portrait,"or as asked for");}
        const QPageLayout page=pageFor(a4,d.sheets[0],plain);
        auto anyCut=[](std::array<bool,4> c){return c[0]||c[1]||c[2]||c[3];};
        require(!anyCut(cutSides(page,d.sheets[0],plain)),"an A4 sheet at 1:1 on A4 paper: its drawing, 10 mm from the edge, fits");
        {PrintSettings s;s.offset=QPointF(0,-8);require(cutSides(page,d.sheets[0],s)[1],"moved up, the drawing is cut at the top");}
        // As in the reference: the printed content (title block and circuit), not the sheet, lies `offset` from the
        // printable area's corner, at any scale.
        {const QRectF content=printedContent(d.sheets[0]),printableArea(5,5,287,200);
            require(!content.isNull()&&near(sheetOrigin(page,d.sheets[0],plain),printableArea.topLeft()-content.topLeft()),"no offset: the content at the printable corner");
            PrintSettings s;s.offset=QPointF(50,30);require(near(sheetOrigin(page,d.sheets[0],s)+content.topLeft(),QPointF(55,35)),"50 mm right, 30 mm down");
            s.free=true;s.scale=.5;require(near(sheetOrigin(page,d.sheets[0],s)+content.topLeft()*.5,QPointF(55,35)),"the same at half the size");
            {const QSizeF z=drawingArea(page,d.sheets[0],s).size();require(near(QPointF(z.width(),z.height()),QPointF(d.sheets[0].width,d.sheets[0].height)*.5),"the sheet halved");}
            Sheet withBlock=d.sheets[0];Item corner;corner.type=ItemType::Rectangle;corner.centre=QPointF(2,2);corner.size=QSizeF(2,2);assignIds(corner);
            withBlock.titleBlock.items<<corner;require(printedContent(withBlock).topLeft().x()<content.left()&&near(sheetOrigin(page,withBlock,plain),QPointF(5,5)-printedContent(withBlock).topLeft()),"the title block is part of it");
            const PrintSettings middle=centred(page,d.sheets[0],plain);
            require(near(printableArea.topLeft()+middle.offset+QPointF(content.width(),content.height())/2,printableArea.center(),.05+1e-9),"centred: the content in the middle, to a tenth of a millimetre");
            const PrintSettings fill=fitted(page,d.sheets[0],plain);
            const double best=std::min(printableArea.width()/content.width(),printableArea.height()/content.height());
            require(fill.free&&std::abs(fill.scale-std::nearbyint(best*98)/100)<1e-9,"fitted as the reference: 98 % of the scale that fits, in whole percent");
            {PrintSettings banner=plain;banner.bannerX=2;banner.overlap=10;const QSizeF one=printableArea.size();
                const double wide=std::min((2*one.width()-10)/content.width(),one.height()/content.height());
                require(std::abs(fitted(page,d.sheets[0],banner).scale-std::nearbyint(wide*98)/100)<1e-9,"over the banner pages less their overlap");}
            require(std::abs(middle.offset.x()*10-std::round(middle.offset.x()*10))<1e-9,"centred to a tenth of a millimetre");
            // Margins that make the exact scale end in a fraction of a percent: still not cut once the preview keeps it.
            {const QPageLayout odd(QPageSize(QPageSize::A4),QPageLayout::Landscape,QMarginsF(6.35,6.35,6.35,6.35),QPageLayout::Millimeter,QMarginsF());
                PrintSettings kept=fitted(odd,d.sheets[0],plain);kept.scale=std::lround(kept.scale*100)/100.;
                kept.offset=QPointF(std::round(kept.offset.x()*10)/10,std::round(kept.offset.y()*10)/10);
                const auto c=cutSides(odd,d.sheets[0],kept);require(!c[0]&&!c[1]&&!c[2]&&!c[3],"fitted and kept as the preview keeps it, not cut");}
            {const QPageLayout full(QPageSize(QPageSize::A4),QPageLayout::Landscape,QMarginsF(5,5,5,5),QPageLayout::Millimeter,QMarginsF());QPageLayout f=full;f.setMode(QPageLayout::FullPageMode);
                require(near(sheetOrigin(f,d.sheets[0],plain),sheetOrigin(page,d.sheets[0],plain)),"also when the printer paints the full page");}}
        const PrintSettings fit=fitted(page,d.sheets[0],plain);
        require(fit.free&&!anyCut(cutSides(page,d.sheets[0],fit)),"fitted, it is not");
        {PrintSettings s=fit;s.offset+=QPointF(-20,0);const auto c=cutSides(page,d.sheets[0],s);require(c[0]&&!c[1]&&!c[2]&&!c[3],"moved left, cut on the left only");}
        {PrintSettings s;s.bannerX=2;s.overlap=10;require(std::abs(bannerArea(page,s).width()-(2*297-10))<1e-3&&std::abs(bannerArea(page,s).height()-210)<1e-3,"a banner of two pages overlapping");}
        auto pages=[](const QString &file){QFile f(file);if(!f.open(QIODevice::ReadOnly))return -1;const QByteArray b=f.readAll();
            int n=0;for(qsizetype at=b.indexOf("/Type /Page");at>=0;at=b.indexOf("/Type /Page",at+1))if(b.mid(at,12)!="/Type /Pages")n++;return n;};
        QTemporaryDir dir;
        {QPrinter printer(QPrinter::HighResolution);printer.setOutputFormat(QPrinter::PdfFormat);printer.setOutputFileName(dir.filePath("print.pdf"));
            QList<PrintSettings> settings{fit,plain};settings[0].bannerX=2;
            require(printSheets(printer,d,{0,1},settings)==3&&pages(dir.filePath("print.pdf"))==3,"printed: a banner of two pages and one more sheet");}
        QString error;
        require(exportPdf(d,{0,1},dir.filePath("export.pdf"),{},&error)&&pages(dir.filePath("export.pdf"))==2,"exported as PDF, a page per sheet");
        Editor x;x.setDocument(d);require(x.exportPdfFile(dir.filePath("one.pdf"),false,false,&error)&&pages(dir.filePath("one.pdf"))==1,"the current sheet from the editor");
        // The preview: what it would print, sheets that would be cut marked.
        QList<PrintSettings> settings;PrintPreview preview(d,settings,QString());
        require(settings.size()==2&&preview.sheetsToPrint()==QList<int>{d.activeSheet},"the current sheet at first");
        preview.allSheets->setChecked(true);require(preview.sheetsToPrint()==QList<int>({0,1}),"all sheets");
        preview.someSheets->setChecked(true);preview.selection->setText(QStringLiteral("2, 1-1"));require(preview.sheetsToPrint()==QList<int>({1,0}),"a choice of sheets");
        {Document withSpare=d;withSpare.sheets[1].spare=true;QList<PrintSettings> spareSettings;PrintPreview spare(withSpare,spareSettings,QString());
            spare.allSheets->setChecked(true);require(spare.sheetsToPrint()==QList<int>({0}),"a spare sheet left out of all sheets");
            spare.someSheets->setChecked(true);spare.selection->setText(QStringLiteral("1-2"));require(spare.sheetsToPrint()==QList<int>({0}),"and out of a choice");}
        preview.setSheet(0);PrintSettings far;far.offset=QPointF(-50,-50);preview.apply(far);
        require(preview.sheetBar->tabText(0).endsWith("[!]")&&settings[0].offset==QPointF(-50,-50),"a sheet that would be cut is marked");
        preview.apply(fitted(pageFor(preview.printer()->pageLayout(),d.sheets[0],settings[0]),d.sheets[0],settings[0]));
        require(!preview.sheetBar->tabText(0).endsWith("[!]")&&preview.freeScale->isChecked(),"fitted, no longer");}
    // --- Parent and child in the editor
    {Document d=exampleDocument();d.sheets[0].items[0].parent=true;
        Item child=exampleSymbol();child.parentId=d.sheets[0].items[0].id;child.designator=QStringLiteral("<PARENT_ID>");child.pos=QPointF(60,60);assignIds(child);
        d.sheets[1].items<<child;
        Editor x;x.resize(1400,900);x.setDocument(d);
        auto panel=[&]{return x.findChild<QWidget*>("properties");};
        x.view()->setSelection({d.sheets[0].items[0].id});QApplication::processEvents();
        require(panel()->findChild<QListWidget*>("children")&&panel()->findChild<QListWidget*>("children")->count()==1&&panel()->findChild<QPushButton*>("removeParent"),"a parent lists its children");
        x.showElement(child.id);
        require(x.document().activeSheet==1&&x.view()->selection()==QStringList{child.id},"going to a child on another sheet");
        require(panel()->findChild<QListWidget*>("parentOf")&&panel()->findChild<QPushButton*>("unlinkParent"),"a child shows its parent");
        panel()->findChild<QPushButton*>("unlinkParent")->click();
        require(x.document().sheets[1].items.last().parentId.isEmpty(),"the link removed");
        x.view()->setSelection({child.id});QApplication::processEvents();panel()->findChild<QPushButton*>("setParent")->click();
        require(x.document().sheets[1].items.last().parent,"a parent made");
        ParentChildListDialog list(x.document(),QString());
        require(list.tree->topLevelItemCount()==2&&list.text().contains("U1"),"the parent-child list");
        RenderOptions o;o.parentChild=true;o.titleBlock=false;
        const QImage plain=renderSheet(d,0,4),coloured=renderSheet(d,0,4,o);
        const QRectF b=bounds(d.sheets[0].items[0]);const QPoint at(int((b.left()+.3)*4),int((b.top()+.3)*4));
        require(coloured.pixelColor(at)!=plain.pixelColor(at)&&coloured.pixelColor(at).blue()>coloured.pixelColor(at).red(),"parents coloured blue");}
    // --- Contact list and table of contents
    {const Document d=exampleDocument();int count=0;for(const auto &p:allComponents(d))count+=int(contacts(*p.item).size());
        ContactListDialog contactsDialog(d,QString());
        require(contactsDialog.list->topLevelItemCount()==count&&contactsDialog.text().contains("U1\t"),"every contact in the list");
        Editor x;x.setDocument(d);const int before=int(x.document().sheet().items.size());
        x.insertContents({0,1},{});
        const Item &table=x.document().sheet().items.last();int texts=0;for(const auto &c:table.children)texts+=c.type==ItemType::Text;
        require(x.document().sheet().items.size()==before+1&&texts==2+2*2,"a table of contents: two columns, two sheets");}
    // --- The component editor: designator and value stay, the insertion point moves
    {Document d=newDocument(QStringLiteral("x"));Item part=exampleSymbol();part.pos=QPointF(50.8,50.8);part.designator=QStringLiteral("U1");assignIds(part);d.sheets[0].items<<part;
        Editor x;x.resize(1400,900);x.show();x.setDocument(d);QApplication::processEvents();
        x.view()->setSelection({part.id});x.editComponent();
        QStringList placeholders;for(const auto &c:x.view()->items())if(c.role!=TextRole::Plain)placeholders<<c.id;
        x.view()->setSelection(placeholders);x.action("delete")->trigger();
        int left=0;for(const auto &c:x.view()->items())left+=c.role!=TextRole::Plain;
        require(placeholders.size()==2&&left==2,"designator and value cannot be deleted in the component editor");
        auto partsOnSheet=[&]{QList<QPointF> out;const Item &c=x.document().sheet().items[0];for(const auto *k:contacts(c))out<<placement(c).map(k->pin);return out;};
        const QList<QPointF> pinsBefore=partsOnSheet();
        dragOn(x.view(),QPointF(0,0),QPointF(15.24,0));
        const Item &moved=x.document().sheet().items[0];
        const QList<QPointF> pinsAfter=partsOnSheet();bool same=pinsAfter.size()==pinsBefore.size();
        for(int k=0;same&&k<pinsAfter.size();k++)same=near(pinsAfter[k],pinsBefore[k],1e-9);
        require(near(moved.pos,{50.8+15.24,50.8},1e-6)&&same,"the insertion point moved, the parts stayed where they are");
        x.leaveComponentEditor(true);}

    // --- Picture, scale, colours and line widths
    {Document d=newDocument(QStringLiteral("x"));
        Item box;box.type=ItemType::Rectangle;box.centre=QPointF(50,50);box.size=QSizeF(20,10);box.pen.width=.5;assignIds(box);
        Item label;label.type=ItemType::Text;label.text=QStringLiteral("T");label.pos=QPointF(70,50);label.font.height=2.5;assignIds(label);
        Item part=exampleSymbol();part.pos=QPointF(100,50);assignIds(part);
        Item white=box;white.centre=QPointF(50,80);white.fill.style=FillStyle::Solid;white.fill.color=QColor(255,255,255);
        Item red=white;red.centre=QPointF(80,80);red.fill.color=QColor(255,0,0);assignIds(white);assignIds(red);
        d.sheets[0].items<<box<<label<<part<<white<<red;
        Editor x;x.resize(1400,900);x.show();x.setDocument(d);QApplication::processEvents();
        x.view()->setSelection({box.id,label.id,part.id,white.id,red.id});
        x.colourizeSelection(QColor(0,0,255));
        // The list again after each change: a change copies it for undo.
        auto items=[&]{return x.document().sheet().items;};
        // A copy first: a loop over a member of the returned list itself would outlive the list.
        bool partBlue=true;const QList<Item> now=items();for(const auto &c:now[2].children)if(c.type!=ItemType::Contact&&!isText(c))partBlue&=c.pen.color==QColor(0,0,255);
        require(items()[0].pen.color==QColor(0,0,255)&&items()[1].font.color==QColor(0,0,255)&&partBlue,"elements coloured, also inside components");
        require(items()[3].fill.color==QColor(255,255,255)&&items()[3].pen.color==QColor(0,0,255)&&items()[4].fill.color==QColor(0,0,255),"a white filling stays white, others take the colour");
        x.changePenWidths(true,.7);require(std::abs(items()[0].pen.width-.7)<1e-9,"a fixed line width");
        x.changePenWidths(false,50);require(std::abs(items()[0].pen.width-.35)<1e-9,"a line width in per cent");
        x.view()->setSelection({box.id});x.scaleSelection(2);
        require(near(items()[0].centre,{50,50})&&std::abs(items()[0].size.width()-40)<1e-9,"scaled about its middle");
        x.view()->setSelection({label.id});x.scaleSelection(2);require(std::abs(items()[1].font.height-5)<1e-9,"a text grows with it");
        Item scaled=part;schematic::scale(scaled,QPointF(),2);
        require(near(scaled.pos,{200,100})&&std::abs(contacts(scaled)[1]->pin.x()-2*contacts(part)[1]->pin.x())<1e-9,"a component's parts grow about its insertion point");
        QTemporaryDir dir;QImage picture(40,20,QImage::Format_RGB32);picture.fill(Qt::green);picture.setDotsPerMeterX(4000);picture.setDotsPerMeterY(4000);
        const QString file=dir.filePath("bild.png");require(picture.save(file),"cannot write the picture");
        QString error;require(x.insertImage(file,&error)&&x.view()->placing(),"a picture to put down");
        click(x.view(),QPointF(127,101.6));
        const Item &placed=x.document().sheet().items.last();
        require(placed.type==ItemType::Image&&x.document().resources.contains(placed.resource)&&std::abs(placed.size.width()-10)<1e-6&&near(placed.centre,{127,101.6},.01),"put down at its size of 10 mm");}

    // --- Aligning and spreading
    {Document d=newDocument(QStringLiteral("x"));
        for(auto [c,w,h]:{std::tuple{QPointF(10,10),4.,2.},std::tuple{QPointF(70,15),2.,2.},std::tuple{QPointF(30,20),6.,4.}}){
            Item r;r.type=ItemType::Rectangle;r.centre=c;r.size=QSizeF(w,h);assignIds(r);d.sheets[0].items<<r;}
        Editor x;x.resize(1400,900);x.show();x.setDocument(d);QApplication::processEvents();
        auto items=[&]{return x.document().sheet().items;};
        auto all=[&](auto edge){const double v=edge(bounds(items()[0]));for(const auto &i:items())if(std::abs(edge(bounds(i))-v)>1e-9)return false;return true;};
        const QRectF before=bounds(items());
        const auto *button=x.findChild<QToolButton*>("alignButton");
        require(button&&button->menu()&&button->menu()->actions().size()==9,"the button with six alignments and two ways of spreading");
        x.view()->setSelection({items()[0].id});x.alignSelection(Alignment::Top);require(near(items()[0].centre,{10,10}),"one element stays");
        x.view()->setSelection({items()[0].id,items()[1].id,items()[2].id});
        x.alignSelection(Alignment::Top);
        require(all([](QRectF b){return b.top();})&&std::abs(bounds(items()[0]).top()-before.top())<1e-9,"tops on the top of the selection");
        require(std::abs(items()[1].centre.x()-70)<1e-9&&std::abs(items()[2].centre.x()-30)<1e-9,"only up and down");
        x.undo();require(near(items()[2].centre,{30,20}),"one undo step");
        x.alignSelection(Alignment::Bottom);require(all([](QRectF b){return b.bottom();})&&std::abs(bounds(items()[2]).bottom()-before.bottom())<1e-9,"bottoms");x.undo();
        x.alignSelection(Alignment::Left);require(all([](QRectF b){return b.left();})&&std::abs(bounds(items()[0]).left()-before.left())<1e-9,"left edges");x.undo();
        x.alignSelection(Alignment::Right);require(all([](QRectF b){return b.right();})&&std::abs(bounds(items()[1]).right()-before.right())<1e-9,"right edges");x.undo();
        x.alignSelection(Alignment::HorizontalCentre);
        require(all([](QRectF b){return b.center().x();})&&std::abs(items()[0].centre.x()-before.center().x())<1e-9&&near(items()[0].centre,{before.center().x(),10}),"middles above one another, in the middle of the selection");x.undo();
        x.alignSelection(Alignment::VerticalCentre);require(all([](QRectF b){return b.center().y();})&&std::abs(items()[1].centre.y()-before.center().y())<1e-9,"middles beside one another");x.undo();
        x.spreadSelection(true);
        require(near(items()[0].centre,{10,10})&&near(items()[1].centre,{70,15})&&near(items()[2].centre,{40,20}),"spread by their middles: first and last stay, in the order on the sheet");x.undo();
        x.spreadSelection(false);
        require(near(items()[0].centre,{10,10})&&near(items()[2].centre,{30,20})&&near(items()[1].centre,{70,15}),"vertically by the middles: 10, 15, 20 are already even");
        x.view()->setSelection({items()[0].id,items()[2].id});x.spreadSelection(true);require(near(items()[2].centre,{30,20}),"two elements are not spread");
        // On the grid: the whole element by its point, or nodes.
        Item line;line.type=ItemType::Line;line.points={QPointF(1.3,2.6),QPointF(5.1,2.6),QPointF(5.1,7.7)};assignIds(line);
        x.change([&](Document &d){d.sheet().grid=1;d.sheet().items<<line;});
        x.view()->setSelection({line.id});x.alignToGrid();
        require(near(items()[3].points[0],{1,3})&&near(items()[3].points[1],{4.8,3})&&near(items()[3].points[2],{4.8,8.1}),"a line moved as a whole, its first point on the grid");
        x.undo();x.nodeToGrid(line.id,1);require(near(items()[3].points[0],{1.3,2.6})&&near(items()[3].points[1],{5,3}),"one node");
        x.undo();x.nodesToGrid(line.id);require(near(items()[3].points[0],{1,3})&&near(items()[3].points[1],{5,3})&&near(items()[3].points[2],{5,8}),"all nodes");}

    // --- Magnetic guide lines
    {Document d=newDocument(QStringLiteral("x"));d.sheets[0].grid=1;
        Item r;r.type=ItemType::Rectangle;r.centre=QPointF(50,50);r.size=QSizeF(10,10);assignIds(r);d.sheets[0].items<<r;
        Editor x;x.resize(1400,900);x.show();x.setDocument(d);QApplication::processEvents();
        SheetView *v=x.view();auto sheet=[&]{return x.document().sheet();};
        dragPixels(v,QPointF(v->toPixel({150,0}).x(),SheetView::rulerSize/2.),v->toPixel({150,80.4}));
        require(sheet().horizontalGuides.size()==1&&std::abs(sheet().horizontalGuides[0]-80.4)<1e-6,"a horizontal guide line dragged from the top ruler");
        dragPixels(v,QPointF(SheetView::rulerSize/2.,v->toPixel({0,120}).y()),v->toPixel({120.2,120}));
        require(sheet().verticalGuides.size()==1&&std::abs(sheet().verticalGuides[0]-120.2)<1e-6,"a vertical one from the left ruler");
        x.undo();require(sheet().verticalGuides.isEmpty()&&sheet().horizontalGuides.size()==1,"one undo step each");
        // The rectangle moved near the line is caught by it.
        const double near=.5*SheetView::magnetPixels/v->scale();
        dragOn(v,{45,50},{45,80.4+near});require(std::abs(x.document().sheet().items[0].centre.y()-80.4)<1e-6&&std::abs(x.document().sheet().items[0].centre.x()-50)<1e-6,"the guide line attracts");
        x.undo();dragOn(v,{45,50},{45,80.4+3*near});require(std::abs(x.document().sheet().items[0].centre.y()-80.4)>.1,"not from further away");x.undo();
        // Moved, chosen and deleted, dragged onto a ruler.
        dragOn(v,{150,80.4},{150,90});require(std::abs(sheet().horizontalGuides.value(0)-90)<1e-6,"a guide line moved");
        click(v,{160,90});require(v->guideChosen(),"chosen by a click");
        x.action("delete")->trigger();require(sheet().horizontalGuides.isEmpty()&&x.document().sheet().items.size()==1,"deleted with Löschen, the elements stay");
        x.newGuide(true);require(sheet().verticalGuides.size()==1,"a new vertical guide line from the menu");
        const double at=sheet().verticalGuides[0];
        dragPixels(v,v->toPixel({at,100}),QPointF(SheetView::rulerSize/2.,v->toPixel({0,100}).y()));require(sheet().verticalGuides.isEmpty(),"dragged onto the ruler, it goes away");
        // Fixed and hidden.
        x.newGuide(false);const double y=sheet().horizontalGuides.value(0);
        x.action("fixGuides")->trigger();require(!v->addGuide(true,10),"fixed: none added");
        dragOn(v,{150,y},{150,y+20});require(sheet().horizontalGuides.value(0)==y,"fixed: not moved");
        x.action("fixGuides")->trigger();x.action("hideGuides")->trigger();
        require(!v->guidesShown()&&!v->addGuide(true,10)&&std::abs(v->snapped({50,y+near/2},{}).y()-y)>1e-6,"hidden: none added, none attracts");
        x.action("hideGuides")->trigger();require(std::abs(v->snapped({50,y+near/2},{}).y()-y)<1e-9,"shown again");
        // The keys that free from snapping, as in the reference: Strg the grid, Umschalt the 45° steps, Alt the terminals.
        {const QPointF from(240,180);const auto loose=Qt::KeyboardModifiers(Qt::ControlModifier);
            require(std::abs(v->snapped({250,181.2},loose,&from).y()-180)<1e-9&&std::abs(v->snapped({250,181.2},loose|Qt::ShiftModifier,&from).y()-181.2)<1e-9,
                    "Umschalt frees from the 45° steps");
            Item dot;dot.type=ItemType::Junction;dot.pos=QPointF(230,170);dot.size=QSizeF(.8,.8);assignIds(dot);x.change([&](Document &dd){dd.sheet().items<<dot;});
            auto at=[](QPointF p,QPointF q){return std::hypot(p.x()-q.x(),p.y()-q.y())<1e-9;};
            require(at(v->snapped({230.05,170.05},loose),{230,170})&&at(v->snapped({230.05,170.05},loose|Qt::AltModifier),{230.05,170.05}),"Alt frees from the terminals");
            x.undo();}
        QTemporaryDir dir;const QString file=dir.filePath("plan.olsch");require(x.saveFile(file),"saved");
        Editor other;QString error;require(other.openFile(file,&error)&&other.document().sheet().horizontalGuides==sheet().horizontalGuides,"guide lines in the file");}

    // --- The drawing modes of the reference: curves, freehand, special shapes, measuring; keys
    {Document d=newDocument(QStringLiteral("x"));d.sheets[0].grid=1;
        Editor x;x.resize(1400,900);x.show();x.setDocument(d);QApplication::processEvents();SheetView *v=x.view();
        auto items=[&]{return x.document().sheet().items;};
        require(x.action("toolLine")->shortcut()==QKeySequence(Qt::Key_L)&&x.action("toolBezier")->shortcut()==QKeySequence(Qt::Key_U)&&x.action("toolMeasure")->shortcut()==QKeySequence(Qt::Key_M),"the keys of the modes");
        x.action("toolBezier")->trigger();for(QPointF p:{QPointF(10,10),QPointF(15,5),QPointF(25,5),QPointF(30,10),QPointF(35,15),QPointF(40,15)})click(v,p);
        click(v,{50,50},Qt::RightButton);
        require(items().size()==1&&items()[0].type==ItemType::Bezier&&items()[0].points.size()==4&&!items()[0].electrical,"a Bézier curve of four points; points left over are dropped");
        x.action("toolFreehand")->trigger();dragOn(v,{10,30},{40,45});
        require(items().size()==2&&items()[1].type==ItemType::Line&&items()[1].points.size()>=3&&!items()[1].electrical,"a freehand line with its nodes, drawing only");
        v->special=SpecialShape::Triangle;v->setTool(SheetView::Tool::Special);dragOn(v,{50,50},{70,60});
        require(items().size()==3&&items()[2].type==ItemType::Polygon&&items()[2].points.size()==3&&v->selection()==QStringList{items()[2].id},"a triangle in the frame, selected");
        x.undo();require(items().size()==2,"one undo step");
        x.action("toolMeasure")->trigger();dragOn(v,{0,0},{30,40});require(items().size()==2,"measuring changes nothing");
        {SpecialShapeOptions o;o.corners=7;o.wave=WaveKind::Sawtooth;SpecialShapeDialog dialog(o,2);
            require(dialog.pages->currentIndex()==2&&dialog.options().corners==7&&dialog.options().wave==WaveKind::Sawtooth&&!dialog.textHeight->isEnabled(),"the settings dialog on the page of its shape");
            dialog.textFields->setChecked(true);require(dialog.textHeight->isEnabled(),"text height with text fields");}
        x.action("toolSelect")->trigger();v->setFocus();
        QKeyEvent tab(QEvent::KeyPress,Qt::Key_Tab,Qt::NoModifier);QApplication::sendEvent(v,&tab);require(v->selection()==QStringList{items()[0].id},"Tab chooses the first element");
        QApplication::sendEvent(v,&tab);require(v->selection()==QStringList{items()[1].id},"and then the next");
        QKeyEvent back(QEvent::KeyPress,Qt::Key_Backtab,Qt::ShiftModifier);QApplication::sendEvent(v,&back);require(v->selection()==QStringList{items()[0].id},"Shift+Tab the one before");
        const QPointF was=items()[0].points[0];QKeyEvent fine(QEvent::KeyPress,Qt::Key_Right,Qt::ControlModifier);QApplication::sendEvent(v,&fine);
        require(std::abs(items()[0].points[0].x()-was.x()-.1)<1e-9,"Ctrl and an arrow move by a tenth of a millimetre");}

    // --- Pictures in the editor: panel, Bitmap-Explorer, pictures no element shows are dropped
    {Document d=newDocument(QStringLiteral("x"));
        QImage image(600,300,QImage::Format_RGB32);image.fill(Qt::red);QByteArray data;{QBuffer b(&data);b.open(QIODevice::WriteOnly);image.save(&b,"PNG");}
        const QString key=QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex());d.resources.insert(key,{QStringLiteral("png"),data});
        Item pic;pic.type=ItemType::Image;pic.resource=key;pic.centre=QPointF(60,60);pic.size=QSizeF(25.4,12.7);assignIds(pic);d.sheets[0].items<<pic;
        Editor x;x.resize(1400,900);x.show();x.setDocument(d);QApplication::processEvents();
        x.view()->setSelection({pic.id});QApplication::processEvents();
        auto *dpi=x.findChild<QLabel*>("imageDpi");require(dpi&&dpi->text().startsWith("600"),"the resolution in the panel");
        auto *reduce=x.findChild<QPushButton*>("reduceResolution");require(reduce&&!reduce->styleSheet().isEmpty(),"above 300 dpi the button is red");
        reduce->click();const QString reduced=x.document().sheet().items[0].resource;
        require(reduced!=key&&x.document().resources.size()==1&&imageInfo(x.document(),x.document().sheet().items[0]).pixels==QSize(429,214),"reduced by 1.4, the old picture gone");
        x.undo();require(x.document().sheet().items[0].resource==key&&x.document().resources.contains(key),"one undo step");
        BitmapExplorerDialog explorer;explorer.document=[&]{return &x.document();};explorer.change=[&](const std::function<void(Document&)> &f){x.change(f);};explorer.refresh();
        require(explorer.count->text()=="1"&&explorer.list->topLevelItemCount()==1,"the explorer lists the picture by sheet");
        explorer.maximum->setCurrentIndex(0);require(explorer.applyMaximum(true)==1&&std::abs(imageInfo(x.document(),x.document().sheet().items[0]).dpi-100)<1,"reduced to 100 dpi");
        require(explorer.applyMaximum(true)==0,"nothing above the maximum any more");
        x.view()->setSelection({pic.id});x.action("delete")->trigger();require(x.document().resources.isEmpty(),"a deleted picture leaves the document");
        require(x.action("toolBitmap")&&x.action("toolBitmap")->shortcut()==QKeySequence(Qt::Key_F),"the mode Bitmap");}

    // --- Dimensions in the editor: drawing each kind, the panel, the handle of the line, the sheet's scale
    {Document d=newDocument(QStringLiteral("x"));d.sheets[0].grid=1;d.sheets[0].titleBlock=TitleBlock();
        Editor x;x.resize(1400,900);x.show();x.setDocument(d);QApplication::processEvents();SheetView *v=x.view();
        auto items=[&]{return x.document().sheet().items;};
        require(x.action("toolDimension")->shortcut()==QKeySequence(Qt::Key_B),"the key of the mode");
        x.action("toolDimension")->trigger();click(v,{20,40});click(v,{50,40});click(v,{35,30});
        require(items().size()==1&&items()[0].type==ItemType::Dimension&&items()[0].dimension==DimensionKind::Standard&&near(items()[0].points[1],{50,40})&&std::abs(items()[0].offset-10)<1e-9,
                "a length: two points and the place of the line");
        click(v,{20,60});click(v,{50,60});click(v,{60,80},Qt::RightButton);require(items().size()==1,"the right mouse button cancels");
        x.setDimensionKind(DimensionKind::Radial);click(v,{20,60});click(v,{40,60});
        require(items().size()==2&&items()[1].dimension==DimensionKind::Radial&&items()[1].offset==0,"a radius: two points");
        x.action("dimensionAngle")->trigger();require(v->dimensionPreset.suffix==QString::fromUtf8("°")&&v->tool()==SheetView::Tool::Dimension,"an angle shows degrees");
        click(v,{100,100});click(v,{120,100});click(v,{100,80});
        require(items().size()==3&&items()[2].dimension==DimensionKind::Angle&&near(items()[2].points[2],{100,100})&&near(items()[2].points[1],{100,80})&&dimensionText(items()[2],1)==QString::fromUtf8("90°"),
                "an angle: vertex, first leg, second leg");
        x.action("dimensionStandard")->trigger();require(v->dimensionPreset.suffix.isEmpty(),"lengths again without °");
        // The panel and the handle of the line.
        x.action("toolSelect")->trigger();v->setSelection({items()[0].id});QApplication::processEvents();
        auto *prefix=x.findChild<QLineEdit*>("prefix");require(prefix,"the dimension's fields in the panel");
        prefix->setText("L=");emit prefix->editingFinished();require(dimensionText(items()[0],1)=="L=30","the prefix");
        dragOn(v,{35,30},{35,25});require(std::abs(items()[0].offset-15)<1e-9,"the line moved with its handle");
        x.undo();require(std::abs(items()[0].offset-10)<1e-9,"and back in one step");
        x.change([](Document &dd){dd.sheet().scale=2;});require(dimensionText(items()[0],x.document().sheet().scale)=="L=60","the sheet's scale");
        // "Bemaßung fixieren": the value shown kept as the fixed value; "Fixierung aufheben" measures again.
        v->setSelection({items()[0].id});x.fixDimensions(true);
        require(!items()[0].autoValue&&items()[0].fixedValue=="60","fixed: the value shown is kept");
        x.change([](Document &dd){dd.sheet().scale=1;});require(dimensionText(items()[0],1)=="L=60","also at another scale");
        x.fixDimensions(false);require(items()[0].autoValue&&dimensionText(items()[0],1)=="L=30","measured again");
        x.undo();x.undo();x.undo();require(items()[0].autoValue&&x.document().sheet().scale==2,"undone step by step");
        {QMouseEvent move(QEvent::MouseMove,v->toPixel({10,20}),v->mapToGlobal(v->toPixel({10,20})),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(v,&move);
            require(x.findChild<QLabel*>("coordinates")->text().contains(uiLocale().toString(20.,'f',2))&&x.findChild<QLabel*>("scaleLabel")->text().startsWith("1:2"),
                    "coordinates and the status bar in the sheet's scale");}
        // The scale's unit from the panel: the status bar names it, "Zurücksetzen" goes back to 1 mm.
        {v->setSelection({});QApplication::processEvents();auto *unit=x.findChild<QComboBox*>("scaleUnit");require(unit&&unit->count()==4,"the units in the panel");
            unit->setCurrentIndex(2);emit unit->activated(2);QApplication::processEvents();
            require(x.document().sheet().scaleUnit==ScaleUnit::Metre&&x.findChild<QLabel*>("scaleLabel")->text().endsWith("\nm"),"metres, shown in the status bar");
            x.findChild<QPushButton*>("resetScale")->click();QApplication::processEvents();
            require(x.document().sheet().scale==1&&x.document().sheet().scaleUnit==ScaleUnit::Millimetre,"reset to 1 mm");}
        // The origin between the rulers: coordinates count from the chosen corner, the axes keep their directions.
        {QMouseEvent corner(QEvent::MouseButtonPress,QPointF(8,8),v->mapToGlobal(QPoint(8,8)),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(v,&corner);
            auto *menu=v->findChild<QMenu*>("originMenu");require(menu&&menu->isVisible()&&menu->actions().size()==4,"the four corners offered");
            menu->actions()[1]->trigger();menu->close();require(v->origin==SheetView::Origin::BottomLeft&&near(v->originPoint(),{0,x.document().sheet().height}),"bottom left chosen");
            const QPointF at=v->toPixel({10,x.document().sheet().height-20});QMouseEvent move(QEvent::MouseMove,at,v->mapToGlobal(at),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(v,&move);
            const QString shown=x.findChild<QLabel*>("coordinates")->text();
            require(shown.contains(uiLocale().toString(10.,'f',2))&&shown.contains(uiLocale().toString(-20.,'f',2)),"the coordinates from that corner");
            v->origin=SheetView::Origin::TopLeft;}
        // The fields of the status bar answer a click, as in the reference: the scale shows the sheet's properties, grid and
        // zoom open their lists.
        {const auto click=[](QWidget *w){const QPointF p(2,2);QMouseEvent e(QEvent::MouseButtonPress,p,w->mapToGlobal(p),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(w,&e);};
            v->setSelection({items()[0].id});click(x.findChild<QLabel*>("scaleLabel"));QApplication::processEvents();
            require(v->selection().isEmpty()&&x.findChild<QComboBox*>("scaleUnit"),"the scale: the sheet's properties");
            auto *grid=x.findChild<QToolButton*>("gridButton")->menu();click(x.findChild<QLabel*>("gridLabel"));
            require(grid->isVisible(),"the grid: its list");grid->hide();
            auto *zoom=x.findChild<QToolButton*>("zoomButton")->menu();click(x.findChild<QLabel*>("zoomLabel"));
            require(zoom->isVisible(),"the zoom: its list");zoom->hide();}
        // A node dragged and undone (the undo step keeps the node where it was).
        Item line;line.type=ItemType::Line;line.points={QPointF(10,150),QPointF(40,150)};assignIds(line);x.change([&](Document &dd){dd.sheet().items<<line;});
        v->setSelection({line.id});dragOn(v,{40,150},{40,180});require(near(items().last().points[1],{40,180}),"a node dragged (45° from the other end)");
        x.undo();require(near(items().last().points[1],{40,150}),"undone, the node is back");
        // Rectangles and circles from a corner or from their centre, Shift for a square.
        {const auto mode=v->tool();v->setTool(SheetView::Tool::Rectangle);
            require(v->shapeFrame({10,10},{13,12},{})==QRectF(10,10,3,2)&&v->shapeFrame({10,10},{13,12},Qt::ShiftModifier)==QRectF(10,10,3,3),"from a corner, Shift a square");
            x.findChild<QAction*>("rectangleFromCentre")->trigger();
            require(v->tool()==SheetView::Tool::Rectangle&&v->shapeFrame({10,10},{13,12},{})==QRectF(7,8,6,4)&&v->shapeFrame({10,10},{13,12},Qt::ShiftModifier)==QRectF(7,7,6,6),
                    "from the centre, chosen at the tool's button");
            const int before=int(items().size());dragOn(v,{100,100},{110,105});
            require(items().size()==before+1&&items().last().type==ItemType::Rectangle&&near(items().last().centre,{100,100})&&near(QPointF(items().last().size.width(),items().last().size.height()),{20,10}),
                    "drawn around the point pressed");
            x.undo();v->rectanglesFromCentre=false;v->setTool(mode);}
        // The selection frame takes every element inside it or only partly inside it (as in the reference), not one whose
        // bounds it only touches.
        {Item slant;slant.type=ItemType::Line;slant.points={QPointF(230,120),QPointF(250,140)};assignIds(slant);x.change([&](Document &dd){dd.sheet().items<<slant;});
            v->setTool(SheetView::Tool::Select);v->setSelection({});dragOn(v,{242,120},{260,128});
            require(!v->selection().contains(slant.id),"a frame inside the bounds of the line but off the line");
            v->setSelection({});dragOn(v,{225,115},{235,128});require(v->selection().contains(slant.id),"a frame over a part of the line");
            x.undo();v->setSelection({});}
        // Zooming as in the reference: a click in the zoom mode zooms in and brings the point to the middle, the right
        // button zooms out; the wheel zooms in about the middle; a double click on an empty spot pans while held.
        {const auto send=[&](QEvent::Type t,QPointF p,Qt::MouseButton b,Qt::MouseButtons held){QMouseEvent e(t,p,v->mapToGlobal(p),b,held,Qt::NoModifier);QApplication::sendEvent(v,&e);};
            const QPointF middle(SheetView::rulerSize+(v->width()-SheetView::rulerSize)/2.,SheetView::rulerSize+(v->height()-SheetView::rulerSize)/2.);
            const auto mode=v->tool();v->setTool(SheetView::Tool::Zoom);const double before=v->scale();const QPointF at=v->toPixel({60,60}),spot=v->toSheet(at);
            send(QEvent::MouseButtonPress,at,Qt::LeftButton,Qt::LeftButton);send(QEvent::MouseButtonRelease,at,Qt::LeftButton,Qt::NoButton);
            const QPointF now=v->toPixel(spot);
            require(std::abs(v->scale()-before*1.4)<1e-9&&std::hypot(now.x()-middle.x(),now.y()-middle.y())<1e-6,"a click zooms in, the point in the middle");
            send(QEvent::MouseButtonPress,middle,Qt::RightButton,Qt::RightButton);send(QEvent::MouseButtonRelease,middle,Qt::RightButton,Qt::NoButton);
            require(std::abs(v->scale()-before)<1e-9&&v->tool()==SheetView::Tool::Zoom,"the right button zooms out, the mode stays");
            v->setTool(SheetView::Tool::Select);v->fitSheet();QApplication::processEvents();
            const QPointF empty=v->toPixel({285,200}),corner=v->toPixel({0,0});
            send(QEvent::MouseButtonPress,empty,Qt::LeftButton,Qt::LeftButton);send(QEvent::MouseButtonRelease,empty,Qt::LeftButton,Qt::NoButton);
            send(QEvent::MouseButtonDblClick,empty,Qt::LeftButton,Qt::LeftButton);
            QMouseEvent move(QEvent::MouseMove,empty+QPointF(30,20),v->mapToGlobal(empty+QPointF(30,20)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(v,&move);
            send(QEvent::MouseButtonRelease,empty+QPointF(30,20),Qt::LeftButton,Qt::NoButton);
            const QPointF shifted=v->toPixel({0,0})-corner;
            require(std::abs(shifted.x()-30)<1e-6&&std::abs(shifted.y()-20)<1e-6,"a double click on an empty spot pans");
            v->setTool(mode);v->fitSheet();}
        // Alt on a segment of a selected line moves that segment alone, one undo step (as in the reference).
        {Item bent;bent.type=ItemType::Line;bent.points={QPointF(100,150),QPointF(120,150),QPointF(120,170)};assignIds(bent);
            x.change([&](Document &dd){dd.sheet().items<<bent;});v->setTool(SheetView::Tool::Select);v->setSelection({bent.id});
            dragOn(v,{120,160},{125,160},Qt::AltModifier|Qt::ControlModifier);
            const QPolygonF p=items().last().points;
            require(near(p[0],{100,150})&&near(p[1],{125,150})&&near(p[2],{125,170}),"the segment moved, the line's other node stayed");
            x.undo();require(near(items().last().points[1],{120,150}),"undone in one step");x.undo();}
        // The changers of the reference: a rectangle's rounding on its top edge, an arc's start and end on the curve.
        {Item box;box.type=ItemType::Rectangle;box.centre=QPointF(60,120);box.size=QSizeF(20,10);assignIds(box);
            Item arc;arc.type=ItemType::Ellipse;arc.centre=QPointF(100,120);arc.size=QSizeF(20,20);arc.arc=ArcStyle::Arc;arc.start=0;arc.stop=90;assignIds(arc);
            x.change([&](Document &dd){dd.sheet().items<<box<<arc;});v->setTool(SheetView::Tool::Select);
            // The changer of a square corner stands 10 pixels from it, the corner's own handle stays free.
            v->setSelection({box.id});const QPointF changer(50+10/v->scale(),115);
            dragOn(v,{50,115},{48,113});require(items()[items().size()-2].corner==0&&items()[items().size()-2].size.width()>20,"the corner's handle sizes the rectangle");
            x.undo();v->setSelection({box.id});
            dragOn(v,changer,changer+QPointF(2.5,0));
            const Item rounded=items()[items().size()-2];
            require(rounded.corners==Corners::Round&&std::abs(rounded.corner-25)<1e-6&&near(rounded.centre,{60,120}),"the changer 10 pixels beside the corner rounds from where it was taken");
            v->setSelection({arc.id});dragOn(v,{110,120},{100,130});
            require(std::abs(items().last().start-270)<1e-9&&std::abs(items().last().stop-90)<1e-9,"dragging the start of the arc");
            x.undo();x.undo();x.undo();}
        // The eight sizers around the selection stretch it; a second click on a selected element makes them arrows that
        // turn (corners, in steps of the rotation snap) and shear (sides), as in the reference.
        {Item top;top.type=ItemType::Line;top.points={QPointF(200,100),QPointF(220,100)};assignIds(top);
            Item slope;slope.type=ItemType::Line;slope.points={QPointF(200,110),QPointF(220,120)};assignIds(slope);
            x.change([&](Document &dd){dd.sheet().items<<top<<slope;});v->setTool(SheetView::Tool::Select);v->setSelection({top.id,slope.id});
            require(v->sizers().size()==8&&near(v->sizers()[4],{220,120}),"eight sizers around the selection");
            auto line=[&](const QString &id){for(const auto &i:items())if(i.id==id)return i.points;return QPolygonF();};
            dragOn(v,{220,120},{240,140},Qt::ControlModifier);
            require(near(line(top.id)[1],{240,100})&&near(line(slope.id)[0],{200,120})&&near(line(slope.id)[1],{240,140}),"a corner stretches both ways from the opposite one");
            x.undo();dragOn(v,{220,110},{230,110},Qt::ControlModifier);
            require(near(line(top.id)[1],{230,100})&&near(line(slope.id)[1],{230,120}),"a side one way");x.undo();
            const auto click=[&](QPointF at){const QPointF p=v->toPixel(at);
                QMouseEvent press(QEvent::MouseButtonPress,p,v->mapToGlobal(p),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(v,&press);
                QMouseEvent release(QEvent::MouseButtonRelease,p,v->mapToGlobal(p),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(v,&release);};
            click({204,100});require(v->turning()&&v->selection().size()==2,"a click on a selected element (beside the sizers): arrows");
            v->rotationSnap=30;dragOn(v,{200,100},{220,100});
            require(near(line(top.id)[0],{220,100})&&near(line(top.id)[1],{220,120}),"a corner arrow turns about the middle");x.undo();
            dragOn(v,{210,100},{220,100},Qt::ControlModifier);
            require(near(line(top.id)[0],{210,100})&&near(line(top.id)[1],{230,100})&&near(line(slope.id)[1],{220,120}),"a side arrow shears, the opposite side stays");x.undo();
            click({204,100});require(!v->turning(),"and back to sizers");
            x.undo();v->setSelection({});}
        // "Wandeln in Polygon" from the editor, one undo step.
        x.convertTo(line.id,ItemType::Polygon);require(items().last().type==ItemType::Polygon&&items().last().id==line.id,"the line made a polygon");
        x.undo();require(items().last().type==ItemType::Line,"and back");
        // The line ends in the panel: all thirteen in the model's order, each with its picture.
        {v->setSelection({line.id});QApplication::processEvents();auto *ends=x.findChild<QComboBox*>("endEnd");
            require(ends&&ends->count()==13&&!ends->itemIcon(2).isNull(),"the thirteen line ends with their pictures");
            ends->setCurrentIndex(int(LineEnd::FilledSquare));emit ends->activated(int(LineEnd::FilledSquare));QApplication::processEvents();
            require(items().last().endEnd==LineEnd::FilledSquare,"the end chosen is the end set");x.undo();}
        // A text of a component that is not selected is dragged on its own, in grid steps; a click alone selects the
        // component; with "only with a modifier key" a drag moves the whole component unless exactly that key is held;
        // from the context menu the text follows the pointer without any key until a click.
        {Item part=exampleSymbol();part.pos=QPointF(120,120);assignIds(part);x.change([&](Document &dd){dd.sheet().items<<part;});
            auto now=[&]{for(const auto &i:items())if(i.id==part.id)return i;return Item();};
            int k=-1;for(int c=0;c<part.children.size();c++)if(part.children[c].role==TextRole::Designator)k=c;require(k>=0,"its designator");
            const QPointF was=part.children[k].pos;const QRectF r=textOutline(placedText(part.children[k],part),part.designator).boundingRect();
            v->setSelection({});dragOn(v,r.center(),r.center()+QPointF(5,0));
            require(now().pos==part.pos&&near(now().children[k].pos,was+QPointF(5,0))&&v->selection().isEmpty(),"the designator moved, the component stayed");
            x.undo();require(near(now().children[k].pos,was),"undone in one step");
            click(v,r.center());require(v->selection()==QStringList{part.id}&&near(now().children[k].pos,was),"a click selects the component");
            v->setSelection({});v->componentTextsWithKey=true;v->componentTextKey=Qt::AltModifier;dragOn(v,r.center(),r.center()+QPointF(5,0));
            require(now().pos!=part.pos&&near(now().children[k].pos,was),"without Alt the whole component moves");
            x.undo();v->setSelection({});dragOn(v,r.center(),r.center()+QPointF(5,0),Qt::AltModifier);
            require(now().pos==part.pos&&near(now().children[k].pos,was+QPointF(5,0)),"with Alt the designator alone");
            x.undo();v->componentTextKey=Qt::ControlModifier|Qt::AltModifier;v->setSelection({});dragOn(v,r.center(),r.center()+QPointF(5,0),Qt::AltModifier);
            require(now().pos!=part.pos&&near(now().children[k].pos,was),"another key chosen: Alt alone moves the component");
            x.undo();v->setSelection({});dragOn(v,r.center(),r.center()+QPointF(5,0),Qt::ControlModifier|Qt::AltModifier);
            require(now().pos==part.pos&&near(now().children[k].pos,was+QPointF(5,0)),"the chosen keys move the designator");
            x.undo();v->setSelection({});
            // Without a key: "Bauteiltext verschieben" from the context menu.
            {const auto menu=v->contextMenuRequested;QString menuFor;v->contextMenuRequested=[&](QPoint,const QString &id,int){menuFor=id;};
                click(v,r.center(),Qt::RightButton);v->contextMenuRequested=menu;
                require(menuFor==part.id&&v->contextComponentText==k,"the text under the context menu");}
            v->startComponentTextMove(part.id,k);require(v->componentTextFollows(),"the text follows the pointer");
            const QPointF at=placement(part).map(was);hover(v,at+QPointF(2.5,0));hover(v,at+QPointF(5,0));
            require(near(now().children[k].pos,was+QPointF(5,0))&&now().pos==part.pos,"moved with the pointer, no button held");
            click(v,at+QPointF(5,0));require(!v->componentTextFollows()&&near(now().children[k].pos,was+QPointF(5,0)),"a click puts it down");
            x.undo();require(near(now().children[k].pos,was),"undone in one step");
            v->startComponentTextMove(part.id,k);hover(v,at+QPointF(5,0));
            QKeyEvent esc(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(v,&esc);
            require(!v->componentTextFollows()&&near(now().children[k].pos,was),"Esc puts it back");
            v->startComponentTextMove(part.id,k);hover(v,at+QPointF(5,0));click(v,at+QPointF(5,0),Qt::RightButton);
            require(!v->componentTextFollows()&&near(now().children[k].pos,was),"so does the right button");
            v->componentTextsWithKey=false;x.undo();v->setSelection({line.id});}
        // A virtual node in the middle of a segment becomes a node when dragged; a polygon has one on its closing side too.
        dragOn(v,{25,150},{25,165});require(items().last().points.size()==3&&near(items().last().points[1],{25,165}),"a node inserted from the middle of the segment (45° from the node before)");
        x.undo();require(items().last().points.size()==2,"undone in one step");
        Item poly;poly.type=ItemType::Polygon;poly.points={QPointF(60,140),QPointF(80,140),QPointF(80,160)};assignIds(poly);x.change([&](Document &dd){dd.sheet().items<<poly;});
        v->setSelection({poly.id});dragOn(v,{70,150},{70,170});
        require(items().last().points.size()==4&&near(items().last().points[3],{70,170}),"a polygon's closing side");}

    // --- Search and replace
    {Document d=exampleDocument();d.sheets[1].items<<d.sheets[0].items[0];d.sheets[1].items.last().id=newId();d.sheets[1].items.last().designator=QStringLiteral("U3");
        SearchOptions o;o.designator=QStringLiteral("u");
        require(search(d,o).size()==3,"components by designator on all sheets, case ignored");
        o.caseSensitive=true;require(search(d,o).isEmpty(),"case kept");
        o.caseSensitive=false;o.allSheets=false;o.currentSheet=1;require(search(d,o).size()==1,"the current sheet only");
        SearchOptions t;t.components=false;t.text=QStringLiteral("Loch");
        const auto texts=search(d,t);require(texts.size()==1&&texts[0].sheet==0,"texts");
        Document changed=d;require(replace(changed,texts,t,QStringLiteral("Bau"))==1&&changed.sheets[0].items[3].text=="OpenBau","replaced in the text");
        SearchOptions v;v.components=false;v.text=QStringLiteral("Blatt 1");Item page;page.type=ItemType::Text;page.text=QStringLiteral("Blatt <PAGENO>");assignIds(page);d.sheets[0].items<<page;
        require(search(d,v).size()==1,"with variables expanded");v.expand=false;require(search(d,v).isEmpty(),"or as written");
        // With sheet numbers and prefix: expanded, designators are searched as shown, part of them is enough; as
        // written, as entered (as in the reference).
        {Document q=newDocument(QStringLiteral("Eins"));q.sheets<<newSheet(QStringLiteral("Zwei"));q.designatorPageNumbers=true;q.designatorPrefix=QStringLiteral("X-");
            auto part=[](const char *designator,const char *value){Item c=exampleSymbol();c.designator=QString::fromLatin1(designator);c.value=QString::fromLatin1(value);assignIds(c);return c;};
            q.sheets[0].items<<part("R1","10k")<<part("R2","4k7");q.sheets[1].items<<part("R1","10k");
            auto found=[&](const char *what,bool expand){SearchOptions s;s.designator=QString::fromLatin1(what);s.expand=expand;QStringList out;for(const auto &h:search(q,s))out<<h.shown;return out.join(u'|');};
            require(found("X-1R1",true)=="X-1R1 / 10k"&&found("R1",true)=="X-1R1 / 10k|X-2R1 / 10k"&&found("X-2",true)=="X-2R1 / 10k"&&found("1R",true)=="X-1R1 / 10k|X-1R2 / 4k7","as shown");
            require(found("X-1R1",false).isEmpty()&&found("R1",false)=="R1 / 10k|R1 / 10k","as entered");
            // Replacing works in the designators as entered, of the hits found as shown (as in the reference): "R1" by
            // "R5" changes both R1; a part of the shown designator that is not in the entered one changes nothing.
            Document r=q;SearchOptions s;s.designator=QStringLiteral("R1");
            require(replace(r,search(r,s),s,QStringLiteral("R5"))==2&&r.sheets[0].items[0].designator=="R5"&&r.sheets[1].items[0].designator=="R5"&&r.sheets[0].items[1].designator=="R2",
                    "replaced in the entered designators of the hits");
            s.designator=QStringLiteral("2R5");
            require(search(r,s).size()==1&&replace(r,search(r,s),s,QStringLiteral("9"))==0&&r.sheets[1].items[0].designator=="R5","a part of the shown designator only: nothing replaced");}
        Editor x;x.resize(1400,900);x.show();x.setDocument(d);QApplication::processEvents();
        x.action("search")->trigger();auto *panel=static_cast<SearchPanel*>(x.findChild<QWidget*>("search"));
        require(panel&&panel->isVisible(),"the search panel shown");
        panel->designator->setText(QStringLiteral("U"));
        require(panel->results->topLevelItemCount()==1&&panel->results->topLevelItem(0)->childCount()==2,"results by sheet under all sheets");
        panel->value->setText(QStringLiteral("B"));panel->valueReplacement->setText(QStringLiteral("C"));
        panel->results->setCurrentItem(panel->results->topLevelItem(0));panel->replaceChosen(ReplaceField::Value);
        int cs=0;for(const auto &p:allComponents(x.document()))cs+=p.item->value==QStringLiteral("C");
        require(cs==1&&x.canUndo(),"replaced on all sheets as one step");}

    // --- Sheets saved and loaded on their own, text constants in the text input
    {QTemporaryDir dir;Document d=exampleDocument();d.variables={{"Autor","Jemand"}};
        Editor x;x.setDocument(d);x.switchSheet(1);QString error;const QString file=dir.filePath("blatt.olsch");
        require(x.saveSheet(file,&error)&&load(file).sheets.size()==1&&load(file).sheets[0].name==d.sheets[1].name&&load(file).variables.size()==1,"the current sheet as a file of its own");
        Editor y;y.setDocument(exampleDocument());const int before=int(y.document().sheets.size());
        require(y.loadSheets(file,&error)&&y.document().sheets.size()==before+1&&y.document().activeSheet==1&&y.document().sheets[1].name==d.sheets[1].name,"loaded after the current sheet");
        require(y.document().sheets[1].id!=d.sheets[1].id&&y.document().variables.size()==1,"with new ids and the variable it brings");
        require(!y.loadSheets(dir.filePath("fehlt.olsch"),&error)&&!error.isEmpty(),"a missing file is reported");
        TextDialog dialog(QStringLiteral("a"),{},nullptr,{QStringLiteral("Netzteil 5 V")});
        auto *constant=dialog.findChild<QPushButton*>("constant");require(constant&&constant->isEnabled()&&constant->menu()&&constant->menu()->actions().size()==1,"text constants offered");
        constant->menu()->actions()[0]->trigger();require(dialog.text().contains(QStringLiteral("Netzteil 5 V")),"and put in");
        TextConstantsDialog constants({QStringLiteral("eins"),QString(),QStringLiteral("zwei")});require(constants.constants()==QStringList({"eins","zwei"}),"empty rows dropped");
        GeneralSettings g;g.libraryFolder=QStringLiteral("/a/b");g.templateFolder=QStringLiteral("/t");g.formFolder=QStringLiteral("/f");g.gridMarks=3;g.gridLines=true;
        SettingsDialog settings(g,nullptr);require(settings.settings()==g,"the settings as given");
        {QStringList titles;for(int k=0;k<settings.pages->count();k++)titles<<settings.pages->item(k)->text();
            require(titles==QStringList{ui("Grundeinstellungen"),ui("Anzeige"),ui("Verzeichnisse"),ui("Bibliothek"),ui("Raster"),ui("Autospeichern"),ui("Neues Blatt"),ui("Hotkeys")},"its pages in the reference's order");}
        require(settings.sheetNumbers->isChecked()&&!settings.designatorPageNumbers->isChecked()&&!settings.designatorPrefix->isEnabled()&&settings.drawing()==DrawingSettings{},
                "the drawing's sheet numbers on, designators without them, the prefix only with them");
        require(!settings.componentTextsWithKey->isChecked()&&!settings.componentTextKey->isEnabled()&&settings.componentTextKey->count()==int(componentTextKeys().size()),
                "component texts without a key; the keys offered");
        settings.designatorPageNumbers->setChecked(true);settings.designatorPrefix->setText(QStringLiteral("=A-"));
        require(settings.designatorPrefix->isEnabled()&&settings.findChild<QLabel*>("designatorExample")->text().endsWith("=A-2R1"),"the prefix with an example");
        require(settings.drawing()==DrawingSettings{true,true,QStringLiteral("=A-")},"the drawing's settings");
        settings.componentTextsWithKey->setChecked(true);settings.componentTextKey->setCurrentIndex(2);
        require(settings.componentTextKey->isEnabled()&&settings.settings().componentTextsWithKey&&settings.settings().componentTextKey==(Qt::ControlModifier|Qt::AltModifier),"another key chosen");
        settings.componentTextsWithKey->setChecked(false);settings.designatorPageNumbers->setChecked(false);settings.designatorPrefix->clear();
        settings.exportFolder->setText(QStringLiteral("/bilder"));require(settings.settings().exportFolder=="/bilder"&&settings.settings().drawingFolder.isEmpty(),"working folders, empty for the last used");
        settings.findChild<QPushButton*>("resetFolder")->click();require(settings.settings().libraryFolder==standardLibraryFolder(),"reset to the standard library folder");
        settings.findChild<QPushButton*>("resetFolders")->click();
        require(settings.settings().templateFolder==standardTemplateFolder()&&settings.settings().formFolder==standardFormFolder(),"the fixed folders reset");
        settings.gridDots->setChecked(true);settings.gridMarks->setValue(0);settings.gridOverTitleBlock->setChecked(true);settings.gridContrast->setCurrentIndex(1);
        require(!settings.settings().gridLines&&settings.settings().gridMarks==0&&settings.settings().gridOverTitleBlock&&settings.settings().gridContrast==35,"the grid's look");
        settings.portrait->setChecked(true);require(settings.settings().newSheetWidth==210&&settings.settings().newSheetHeight==297,"portrait turns the new sheet");
        {SettingsDialog keys(GeneralSettings{},nullptr,{HotkeyMode{QStringLiteral("toolLine"),QStringLiteral("Linie"),QKeySequence(Qt::Key_L)}});
            auto *edit=keys.findChild<QKeySequenceEdit*>("toolLine");require(edit&&edit->keySequence()==QKeySequence(Qt::Key_L)&&keys.settings().hotkeys.isEmpty(),"the standard key, nothing to keep");
            edit->setKeySequence(QKeySequence(Qt::Key_Q));require(keys.settings().hotkeys.value("toolLine")=="Q","another key kept");
            keys.findChild<QPushButton*>("resetHotkeys")->click();require(keys.settings().hotkeys.isEmpty(),"back to the standard");}}

    // --- Lettering of components
    {Document d=exampleDocument();Item second=d.sheets[0].items[0];second.id=newId();second.designator=QStringLiteral("U9");d.sheets[1].items<<second;
        Editor x;x.setDocument(d);
        Lettering big;big.set=true;big.family=QStringLiteral("Courier");big.height=4;big.bold=true;Lettering none;
        auto fontOf=[&](int sheet,int item,TextRole role){for(const auto &c:x.document().sheets[sheet].items[item].children)if(c.role==role)return c.font;return Font();};
        x.applyLettering(big,none,none,LetteringDialog::Sheet);
        require(fontOf(0,0,TextRole::Designator).family=="Courier"&&fontOf(0,0,TextRole::Designator).bold&&std::abs(fontOf(0,0,TextRole::Designator).height-4)<1e-9,"the designators of the current sheet");
        require(fontOf(0,0,TextRole::Value).family!="Courier"&&fontOf(1,int(x.document().sheets[1].items.size())-1,TextRole::Designator).family!="Courier","values and other sheets stay");
        x.applyLettering(none,big,none,LetteringDialog::Project);require(fontOf(1,int(x.document().sheets[1].items.size())-1,TextRole::Value).family=="Courier","the whole project");
        x.view()->setSelection({x.document().sheets[0].items[1].id});x.applyLettering(none,none,big,LetteringDialog::Selection);
        bool contactsBig=true,othersSmall=true;
        for(const auto *c:contacts(x.document().sheets[0].items[1]))contactsBig&=c->font.family=="Courier";
        for(const auto *c:contacts(x.document().sheets[0].items[0]))othersSmall&=c->font.family!="Courier";
        require(contactsBig&&othersSmall,"the contacts of the selected components");
        LetteringDialog dialog(false,false,nullptr);require(!dialog.selection->isEnabled()&&!dialog.lettering(2).set&&dialog.lettering(0).set,"the dialog's start");}

    // --- Text links in the editor
    {Document d=exampleDocument();Item to;to.type=ItemType::Text;to.text=QStringLiteral("Ziel");to.linkable=true;to.pos=QPointF(50,50);assignIds(to);
        Item from;from.type=ItemType::Text;from.text=QStringLiteral("zum Ziel");from.linkTarget=to.id;from.pos=QPointF(20,20);assignIds(from);
        d.sheets[1].items<<to;d.sheets[0].items<<from;
        Editor x;x.resize(1400,900);x.setDocument(d);auto panel=[&]{return x.findChild<QWidget*>("properties");};
        x.view()->setSelection({from.id});QApplication::processEvents();
        require(panel()->findChild<QListWidget*>("linkTarget")&&panel()->findChild<QPushButton*>("removeTarget")->isEnabled(),"the panel shows the target");
        x.action("followLink")->trigger();require(x.document().activeSheet==1&&x.view()->selection()==QStringList{to.id},"F8 follows the selected text's link");
        x.switchSheet(0);
        require(x.followLink(from.id)&&x.document().activeSheet==1&&x.view()->selection()==QStringList{to.id},"following a link to another sheet");
        require(panel()->findChild<QListWidget*>("linkSources")&&panel()->findChild<QListWidget*>("linkSources")->count()==1,"a target lists where it is linked from");
        panel()->findChild<QCheckBox*>("linkable")->setChecked(false);
        require(!x.document().sheets[1].items.last().linkable,"no longer a target");
        LinkListDialog list(x.document());require(list.list->topLevelItemCount()==1&&list.text().contains("zum Ziel"),"the link list");}

    // --- Second colour and stripes in the panel; text frames have none
    {Document d=exampleDocument();Item tb;tb.type=ItemType::TextBox;tb.centre=QPointF(80,80);tb.size=QSizeF(30,10);assignIds(tb);d.sheets[0].items<<tb;
        Editor x;x.resize(1400,900);x.setDocument(d);auto panel=[&]{return x.findChild<QWidget*>("properties");};
        const QString wire=d.sheets[0].items[2].id;require(d.sheets[0].items[2].type==ItemType::Line,"a line");
        x.view()->setSelection({wire});QApplication::processEvents();
        require(panel()->findChild<QCheckBox*>("twoColour")&&panel()->findChild<QToolButton*>("twoColourColour")&&panel()->findChild<QCheckBox*>("cross"),"a line has a second colour and stripes");
        panel()->findChild<QCheckBox*>("inner")->setChecked(true);QApplication::processEvents();
        auto line=[&]{for(const auto &i:x.document().sheets[0].items)if(i.id==wire)return i;return Item();};
        require(line().pen.inner&&!line().pen.cross,"the lengthwise stripe switched on");
        {double l=0;for(int k=0;k+1<line().points.size();k++)l+=std::hypot(line().points[k+1].x()-line().points[k].x(),line().points[k+1].y()-line().points[k].y());
            require(panel()->findChild<QLabel*>("lineLength")->text()==uiLocale().toString(l,'f',1)+" mm"&&panel()->findChild<QLabel*>("nodeCount")->text()==QString::number(line().points.size()),
                    "the line's length and nodes");}
        panel()->findChild<QCheckBox*>("twoColour")->setChecked(true);QApplication::processEvents();
        require(line().pen.twoColour,"two colours switched on");
        x.undo();x.undo();require(!line().pen.inner&&!line().pen.twoColour,"each an undo step");
        x.view()->setSelection({tb.id});QApplication::processEvents();
        require(panel()->findChild<QDoubleSpinBox*>("penWidth")&&!panel()->findChild<QCheckBox*>("inner"),"no stripes for a text frame");
        x.markSaved();}

    // --- The child list dialog
    {Document d=exampleDocument();auto &items=d.sheets[0].items;items[0].parent=true;items[1].parentId=items[0].id;
        ChildListDialog dialog(d,items[0].id,QString());
        require(dialog.order->count()==3&&dialog.preview->toPlainText().contains("U2"),"the chosen columns and a preview");
        dialog.columns[int(ChildColumn::Value)]->setChecked(true);dialog.columns[int(ChildColumn::Designator)]->setChecked(false);
        require(dialog.options().columns==QList<ChildColumn>({ChildColumn::Contacts,ChildColumn::PageColumn,ChildColumn::Value}),"checked columns come last, unchecked ones go");
        dialog.size->setValue(25);dialog.shadow->setChecked(true);const PartsDrawing dr=dialog.drawing();
        require(std::abs(dr.height-2.5)<1e-9&&dr.shadow&&!dr.header,"text height in tenths of a millimetre, the options, no header");
        Editor x;x.setDocument(d);x.view()->setSelection({items[0].id});QApplication::processEvents();
        require(x.findChild<QPushButton*>("childList"),"the parent's panel offers it");}

    // --- Presets of the drawing modes in the panel, kept in the settings (a settings file of the test, not the user's)
    {static QTemporaryDir settings;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        // The library folders for the suite: the own one and further ones, only read; open editors read them again.
        {QTemporaryDir own,extra;LibraryPage page;page.name=QStringLiteral("Fremd");page.entries.append({QStringLiteral("Teil"),exampleSymbol()});
            savePage(page,QDir(extra.path()).filePath(QStringLiteral("fremd.olschlib")));
            Editor x;x.loadPreferences();
            const openloch::LibraryFolders folders{own.path(),{extra.path()}};setLibraryFolders(folders);
            require(libraryFolders()==folders&&libraryFolder()==own.path(),"kept in the settings");
            x.librariesChanged();int found=-1;const auto pages=x.library()->pages();
            for(int k=0;k<pages.size();k++)if(pages[k].name==QStringLiteral("Fremd"))found=k;
            require(found>=0&&pages[found].readOnly&&pages[found].folder==QDir(extra.path()).dirName()&&!x.library()->writable(found),"a further folder's page, only read");
            setLibraryFolders({});require(libraryFolders()==openloch::LibraryFolders{standardLibraryFolder(),{}},"back to the standard folder, none further");}
        {Editor x;x.resize(1400,900);x.show();x.loadPreferences();QApplication::processEvents();
            x.action("toolLine")->trigger();QApplication::processEvents();
            require(x.findChild<QLabel*>("presetTitle")&&x.findChild<QDoubleSpinBox*>("penWidth"),"the preset of the mode in the panel");
            x.findChild<QDoubleSpinBox*>("penWidth")->setValue(7);
            require(std::abs(x.view()->linePreset.pen.width-.7)<1e-9&&!x.isModified(),"the preset changes, the document does not");
            click(x.view(),{20,20});click(x.view(),{50,20});click(x.view(),{60,60},Qt::RightButton);
            require(std::abs(x.document().sheet().items.last().pen.width-.7)<1e-9,"a new line gets it");
            x.undo();x.action("toolDimension")->trigger();QApplication::processEvents();require(x.findChild<QDoubleSpinBox*>("arrowLength"),"the preset of dimensions");
            x.findChild<QDoubleSpinBox*>("arrowLength")->setValue(4);x.action("toolSelect")->trigger();QApplication::processEvents();
            require(!x.findChild<QLabel*>("presetTitle"),"no preset for Standard");
            x.markSaved();x.close();}
        Editor y;y.loadPreferences();require(std::abs(y.view()->linePreset.pen.width-.7)<1e-9&&std::abs(y.view()->dimensionPreset.arrowLength-4)<1e-9&&y.view()->linePreset.points.isEmpty(),"kept for the next start");
        // The "Grundeinstellungen": standard folders kept as absent entries, the grid given to the view.
        {GeneralSettings g=GeneralSettings::load();require(g.formFolder==standardFormFolder()&&g.gridMarks==5&&!g.gridLines,"the defaults");
            g.gridMarks=10;g.gridLines=true;g.gridContrast=40;g.componentTextsWithKey=true;g.componentTextKey=Qt::MetaModifier;g.store();
            require(!QSettings().contains("schematic/formFolder")&&GeneralSettings::load()==g,"stored and read again");
            Editor z;z.loadPreferences();require(z.view()->gridMarks==10&&z.view()->gridLines&&z.view()->gridContrast==40,"the view draws the grid so");
            require(z.view()->componentTextsWithKey&&z.view()->componentTextKey==Qt::MetaModifier,"component texts only with the Meta key");
            // Sheet numbers on the tabs come from the drawing.
            {Document nd=exampleDocument();nd.sheetNumbers=false;Editor t;t.setDocument(nd);require(t.sheetTabs()->tabText(0)==nd.sheets[0].name,"a drawing without sheet numbers: plain tabs");
                nd.sheetNumbers=true;t.setDocument(nd);require(t.sheetTabs()->tabText(0)==QStringLiteral("1: ")+nd.sheets[0].name,"with them: numbered");t.markSaved();}
            // The older setting "only with Alt" is taken over.
            {QSettings s;s.remove("schematic/componentTextsWithKey");s.remove("schematic/componentTextKey");s.setValue("schematic/componentTextsWithAlt",true);}
            require(GeneralSettings::load().componentTextsWithKey&&GeneralSettings::load().componentTextKey==Qt::AltModifier,"Alt as before");
            g.store();require(!QSettings().contains("schematic/componentTextsWithAlt"),"the old entry goes");
            // Working folders: set ones first, else the last used (kept with the preferences), else the fallback.
            {Editor w;w.loadPreferences();w.applySettings(GeneralSettings{});
                require(w.startFolder(Editor::Work::Exports,QStringLiteral("/fallback"))=="/fallback","nothing used yet: the fallback");
                const QString used=QFileInfo(QDir::temp().filePath("bild.png")).absolutePath();
                w.usedFolder(Editor::Work::Exports,QDir::temp().filePath("bild.png"));require(w.startFolder(Editor::Work::Exports)==used,"the last used");
                GeneralSettings set;set.exportFolder=QStringLiteral("/fest");w.applySettings(set);require(w.startFolder(Editor::Work::Exports)=="/fest","the one set");
                w.applySettings(GeneralSettings{});w.markSaved();w.close();}
            {Editor w;w.loadPreferences();require(w.startFolder(Editor::Work::Exports)==QFileInfo(QDir::temp().filePath("bild.png")).absolutePath()&&w.startFolder(Editor::Work::Drawings,QStringLiteral("x"))=="x","kept for the next start");w.markSaved();}
            GeneralSettings reset;reset.libraryFolder=standardLibraryFolder();reset.templateFolder=standardTemplateFolder();reset.formFolder=standardFormFolder();reset.store();}}

    // --- Export as pictures and SVG, of the sheet or of the selected elements only
    {Document d=exampleDocument();Item l;l.type=ItemType::Line;l.points={QPointF(100,100),QPointF(140,120)};l.pen.width=1;assignIds(l);d.sheets[0].items<<l;
        Editor x;x.setDocument(d);QTemporaryDir dir;QString error;
        require(x.exportImage(dir.filePath("plan.svg"),300,false,false,false,&error)&&QFile(dir.filePath("plan.svg")).size()>1000,"the sheet as SVG");
        require(!x.exportImage(dir.filePath("none.png"),100,false,false,false,&error,ExportArea::Selection)&&!error.isEmpty(),"nothing selected, nothing exported");
        x.view()->setSelection({l.id});
        require(x.exportImage(dir.filePath("line.png"),254,false,false,false,&error,ExportArea::Selection),"the selection as PNG");
        const QImage image(dir.filePath("line.png"));require(image.width()==490&&image.height()==290,"its bounds and 4 mm around them as in the reference (10 pixels per millimetre)");
        require(x.exportImage(dir.filePath("line.svg"),300,false,false,true,&error,ExportArea::Selection),"the selection as SVG");
        {QFile f(dir.filePath("line.svg"));require(f.open(QIODevice::ReadOnly)&&f.readAll().contains("width=\"49mm\" height=\"29mm\""),"in its size");}
        require(x.exportImage(dir.filePath("plan.emf"),300,false,false,false,&error)&&QFile(dir.filePath("plan.emf")).size()>1000,"the sheet as EMF");
        require(x.exportImage(dir.filePath("line.emf"),300,false,false,true,&error,ExportArea::Selection),"the selection as EMF");
        {QFile f(dir.filePath("line.emf"));const QByteArray emf=f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();
            require(emf.size()>40&&emf.mid(32,8)==QByteArray::fromHex("24130000540b0000"),"in its size (49 by 29 mm)");}
        // "Alle Elemente": the frame of all elements, drawn with the title block.
        {const QRectF all=bounds(x.document().sheet().items).adjusted(-4,-4,4,4);require(near(x.exportFrame(ExportArea::Elements).topLeft(),all.topLeft())&&near(x.exportFrame(ExportArea::Elements).bottomRight(),all.bottomRight()),"the frame of all elements");
            require(x.exportImage(dir.filePath("all.png"),127,false,false,false,&error,ExportArea::Elements),"all elements as PNG");
            const QImage picture(dir.filePath("all.png"));require(picture.width()==int(std::lround(all.width()*5))&&picture.height()==int(std::lround(all.height()*5)),"in that frame");}
        {Editor empty;Document e=exampleDocument();e.sheets[0].items.clear();empty.setDocument(e);
            require(!empty.exportImage(dir.filePath("empty.png"),100,false,false,false,&error,ExportArea::Elements)&&!error.isEmpty(),"no elements, nothing exported");empty.markSaved();}
        // All sheets: a file each, the spare ones left out.
        {Document three=x.document();three.sheets<<three.sheets[0]<<three.sheets[0];three.sheets[1].spare=true;Editor y;y.setDocument(three);
            require(y.exportImage(dir.filePath("blatt.png"),50,false,true,false,&error),"all sheets");
            require(QFileInfo::exists(dir.filePath("blatt_1.png"))&&!QFileInfo::exists(dir.filePath("blatt_2.png"))&&QFileInfo::exists(dir.filePath("blatt_3.png")),"each in a file of its own, spare sheets left out");
            y.markSaved();}
        // The dialog: format, resolution with the sizes, options, what is exported.
        {ExportDialog dialog(QSizeF(297,210),QSizeF(120,80),QSizeF(),1);
            require(!dialog.selectedElements->isEnabled()&&!dialog.allSheets->isEnabled()&&dialog.currentSheet->isChecked(),"without a selection and with one sheet: the sheet or its elements");
            dialog.formats[ExportDialog::Png]->setChecked(true);dialog.dpi->setValue(254);
            require(dialog.original->text()==QStringLiteral("297,0 mm x 210,0 mm").replace(u',',uiLocale().decimalPoint())&&dialog.pixels->text()=="2970 x 2100","the original size and that of the picture");
            dialog.allElements->setChecked(true);require(dialog.pixels->text()=="1200 x 800"&&dialog.area()==ExportArea::Elements,"of the elements' frame");
            require(dialog.resolution->value()==254&&dialog.transparent->isEnabled(),"slider and field together, transparency for PNG");
            dialog.dpi->setValue(600);require(dialog.resolution->value()==300&&dialog.dpi->value()==600,"the field beyond the slider's 300 dpi");
            dialog.formats[ExportDialog::Jpg]->setChecked(true);require(!dialog.transparent->isEnabled(),"no transparency for JPEG");
            dialog.formats[ExportDialog::Svg]->setChecked(true);require(!dialog.dpi->isEnabled()&&dialog.pixels->text()==QStringLiteral("–")&&dialog.suffix()=="svg","no resolution for vectors");
            dialog.formats[ExportDialog::Pdf]->setChecked(true);require(!dialog.allElements->isEnabled()&&dialog.currentSheet->isChecked(),"PDF: whole sheets only");}
        {ExportDialog dialog(QSizeF(297,210),QSizeF(120,80),QSizeF(49,29),3);require(dialog.selectedElements->isEnabled()&&dialog.allSheets->isEnabled(),"with a selection and several sheets, every choice");
            dialog.selectedElements->setChecked(true);require(dialog.area()==ExportArea::Selection,"the selection");}
        x.markSaved();}

    // --- Templates: a document of the template folder opened as a new, unsaved one with ids of its own; the folder in
    // "Grundeinstellungen"
    {QTemporaryDir dir;Document t=exampleDocument();t.sheets[0].name=QStringLiteral("Vorlage A4");
        save(t,dir.filePath("A4.olsch"));{QFile f(dir.filePath("A4.spl8"));require(f.open(QIODevice::WriteOnly)&&f.write(splan::write(t,80))>0,"cannot write the sPlan template");}
        Editor x;QString error;
        for(const char *name:{"A4.olsch","A4.spl8"}){
            require(x.newFromTemplate(dir.filePath(QString::fromLatin1(name)),&error),"a new document from a template");
            const Document &d=x.document();
            require(d.sheets.size()==t.sheets.size()&&d.sheets[0].name=="Vorlage A4"&&components(d.sheets[0]).size()==components(t.sheets[0]).size(),"with the template's content");
            require(d.id!=t.id&&d.sheets[0].id!=t.sheets[0].id&&d.sheets[0].items[0].id!=t.sheets[0].items[0].id&&x.filePath().isEmpty()&&!x.isModified(),"new ids, not saved anywhere");}
        require(!x.newFromTemplate(dir.filePath("fehlt.olsch"),&error)&&!error.isEmpty(),"a missing template");
        QStringList file;for(const auto &[title,names]:editorMenus())if(title=="&Datei")file=names;
        require(file.contains("templateNew")&&file.contains("templateSave")&&file.contains("explorer")&&x.action("templateNew")&&x.action("explorer"),"in the file menu");
        GeneralSettings g;g.templateFolder=dir.filePath("eigene");SettingsDialog dialog(g);
        require(dialog.settings().templateFolder==dir.filePath("eigene")&&dialog.findChild<QLineEdit*>("templateFolder"),"the template folder in the settings");
        x.markSaved();}

    // --- Settings at work: hotkeys, presets of a new sheet, autosave and the copy made on opening
    {QTemporaryDir dir;Editor x;GeneralSettings g;g.hotkeys.insert(QStringLiteral("toolLine"),QStringLiteral("Q"));
        g.newSheetWidth=420;g.newSheetHeight=297;g.newSheetGrid=2.54;g.newSheetForm=dir.filePath("A3.sbk");g.backupOnOpen=true;
        {QFile f(g.newSheetForm);require(f.open(QIODevice::WriteOnly)&&f.write(splan::writeTitleBlock(simpleTitleBlock(420,297).items,{},80))>0,"cannot write the title block");}
        x.applySettings(g);
        require(x.action("toolLine")->shortcut()==QKeySequence(Qt::Key_Q)&&x.action("toolRectangle")->shortcut()==QKeySequence(Qt::Key_R),"a key changed, the others standard");
        x.newDocument();const Sheet &s=x.document().sheets[0];
        require(s.width==420&&s.height==297&&s.grid==2.54&&!s.titleBlock.items.isEmpty()&&s.titleBlock.name=="A3","a new document as preset");
        x.insertSheets(1,1,false);require(x.document().sheets.size()==2&&x.document().sheets[1].width==420&&!x.document().sheets[1].titleBlock.items.isEmpty(),"a new sheet as preset");
        save(exampleDocument(),dir.filePath("plan.olsch"));require(x.openFile(dir.filePath("plan.olsch")),"opened");
        require(QFile(dir.filePath("Backup_of_plan.olsch")).size()==QFile(dir.filePath("plan.olsch")).size(),"a copy as it was opened");
        x.autosave();require(!QFileInfo::exists(dir.filePath("plan.bak")),"nothing to keep while unchanged");
        x.change([](Document &d){d.sheet().name=QStringLiteral("geändert");});x.autosave();
        require(QFileInfo::exists(dir.filePath("plan.bak"))&&load(dir.filePath("plan.bak")).sheets[0].name=="geändert","the changed document beside it");
        x.applySettings(GeneralSettings{});require(x.action("toolLine")->shortcut()==QKeySequence(Qt::Key_L),"the standard key again");
        x.markSaved();}

    // --- Paper formats as in sPlan: "Frei", A0–A5, B4, B5, C3–C5 (then Letter, Legal) for a sheet and for a new sheet
    {require(paperFormatOf(353,250)>=0&&paperFormatOf(250,353)==paperFormatOf(353,250)&&QString::fromLatin1(paperFormats()[paperFormatOf(229,162)].name)=="C5"
             &&paperFormatOf(100,100)<0,"the paper formats of sPlan");
     Editor x;x.resize(1400,900);x.setDocument(exampleDocument());
     auto *format=x.properties()->findChild<QComboBox*>("sheetFormat");require(format&&format->itemText(0)=="Frei"&&format->findText("B4")>0,"the sheet's formats, Frei first");
     const int b5=format->findText("B5");format->setCurrentIndex(b5);emit format->activated(b5);
     const Sheet &sheet=x.document().sheet();require(std::abs(std::max(sheet.width,sheet.height)-250)<1e-9&&std::abs(std::min(sheet.width,sheet.height)-176)<1e-9,"a sheet in B5");
     SettingsDialog settings(GeneralSettings{},nullptr,{});auto *fresh=settings.findChild<QComboBox*>("newSheetFormat");
     require(fresh&&fresh->itemText(fresh->currentIndex())=="A4","a new sheet in A4 by default");
     const int c4=fresh->findText("C4");fresh->setCurrentIndex(c4);emit fresh->activated(c4);
     require(settings.settings().newSheetWidth==324&&settings.settings().newSheetHeight==229,"a new sheet in C4, landscape");
     settings.newSheetWidth->setValue(100);require(fresh->currentIndex()==0,"another size is Frei");
     x.markSaved();}
    // --- "Weißer Hintergrund" (Grundeinstellungen › Anzeige): the screen's paper slightly tinted as in sPlan, else white
    {SettingsDialog settings(GeneralSettings{},nullptr,{});require(settings.whiteBackground&&!settings.whiteBackground->isChecked(),"the tinted paper by default");
     settings.whiteBackground->setChecked(true);require(settings.settings().whiteBackground,"white chosen");
     Document d=newDocument(QString());RenderOptions tinted;tinted.paperColour=QColor(255,255,250);
     require(renderSheet(d,0,2,tinted).pixelColor(1,1)==QColor(255,255,250)&&renderSheet(d,0,2,RenderOptions{}).pixelColor(1,1)==QColor(Qt::white),"the paper colour of the screen and of exports");
     Editor x;x.applySettings(settings.settings());require(x.view()->whiteBackground,"the view takes the white background");
     x.applySettings(GeneralSettings{});require(!x.view()->whiteBackground,"and the tint again");x.markSaved();}

    // --- "Formblatt generieren" as in sPlan: each side ---, NUM or CHAR, font and height; frame and division from the sheet
    {auto texts=[](const TitleBlock &t){QStringList out;for(const auto &i:t.items)if(i.type==ItemType::Text&&!i.text.startsWith('<'))out<<i.text;return out;};
     const QRectF frame(10,10,277,190);TitleBlockStyle style;style.font=QStringLiteral("Courier New");style.field=false;
     const TitleBlock standard=generateTitleBlock(frame,10,8,style);
     require(texts(standard)==QStringList{"1","2","3","4","5","6","7","8","9","10","A","B","C","D","E","F","G","H"},"numbers above and letters left by default");
     bool inner=false;for(const auto &i:standard.items)inner|=i.type==ItemType::Rectangle&&std::abs(i.centre.x()-(18+287)/2.)<1e-9&&std::abs(i.centre.y()-(18+200)/2.)<1e-9;
     bool font=true;for(const auto &i:standard.items)if(i.type==ItemType::Text)font&=i.font.family=="Courier New"&&std::abs(i.font.height-4)<1e-9;
     require(inner&&font,"strips only on the labelled sides, 8 mm for 4 mm text, in the chosen font");
     style.top=FrameLabels::Letters;style.left=FrameLabels::None;style.columnStart=26;
     require(texts(generateTitleBlock(frame,3,8,style))==QStringList{"Z","AA","AB"},"letters go on after Z, from the start column");
     TitleBlock current;current.frame=frame;current.columns=4;current.rows=2;current.showGrid=true;
     TitleBlockDialog dialog(current,297,210,nullptr);dialog.bottom->setCurrentIndex(1);dialog.size->setValue(25);dialog.field->setChecked(false);
     const TitleBlock made=dialog.titleBlock();
     require(made.frame==frame&&made.columns==4&&made.rows==2&&made.showGrid&&texts(made)==QStringList{"1","1","2","2","3","3","4","4","A","B"},"the dialog keeps the sheet's frame and division, labels above, below and left");}

    // --- "Spalte-Start", "Zeile-Start" and "Auto" in the Formblatt section, in sPlan's order; the generated labels count
    //     from the starts
    {Document d=newDocument(QString());d.sheets[0].width=297;d.sheets[0].height=210;
     d.sheets[0].titleBlock.frame=QRectF(10,10,277,190);d.sheets[0].titleBlock.columns=4;d.sheets[0].titleBlock.rows=2;
     Editor x;x.resize(1400,900);x.setDocument(d);
     auto spin=[&](const char *name){return x.properties()->findChild<QSpinBox*>(name);};
     require(spin("frameColumnStart")&&spin("frameRowStart")&&spin("frameColumnStart")->value()==1&&spin("frameColumnStart")->minimum()==1
             &&spin("frameRowStart")->maximum()==10000&&spin("frameColumns")->maximum()==999,"the starts in the panel, from 1");
     QStringList labels;for(const auto *l:x.properties()->findChildren<QLabel*>())labels<<l->text();
     const qsizetype at=labels.indexOf("Spalten:");
     require(at>=4&&labels.mid(at-4,9)==QStringList{"Breite:","Höhe:","X-Offset:","Y-Offset:","Spalten:","Zeilen:","Spalte-Start:","Zeile-Start:","Zeige Gitter:"},"the order of sPlan");
     spin("frameColumnStart")->setValue(5);spin("frameRowStart")->setValue(3);
     require(x.document().sheet().titleBlock.columnStart==5&&x.document().sheet().titleBlock.rowStart==3,"the starts of the sheet changed");
     auto texts=[](const TitleBlock &t){QStringList out;for(const auto &i:t.items)if(i.type==ItemType::Text&&!i.text.startsWith('<'))out<<i.text;return out;};
     {TitleBlockDialog dialog(x.document().sheet().titleBlock,297,210,nullptr);dialog.field->setChecked(false);const TitleBlock made=dialog.titleBlock();
      require(texts(made)==QStringList{"5","6","7","8","C","D"}&&made.columnStart==5&&made.rowStart==3,"Formblatt generieren labels from the starts and keeps them");}
     auto *automatic=x.properties()->findChild<QPushButton*>("autoGrid");require(automatic&&!automatic->text().endsWith(QChar(0x2026)),"Auto acts at once");
     automatic->click();
     {const TitleBlock t=x.document().sheet().titleBlock;
      require(t.frame==QRectF(10,10,277,190)&&t.columns==12&&t.rows==8&&t.columnStart==1&&t.rowStart==1&&spin("frameColumns")->value()==12&&spin("frameColumnStart")->value()==1,
              "Auto: the frame 10 mm inside the sheet, fields of about 23 mm, from 1");}
     x.undo();require(x.document().sheet().titleBlock.columnStart==5&&x.document().sheet().titleBlock.columns==4,"Auto is one step to undo");
     x.markSaved();}

    // --- "Datensicherung" as in sPlan: the folder and its files; a backup restored into another folder, deleting only the
    //     library pages there first; the two dialogs with their files, folder and "delete before restoring"
    {QTemporaryDir dir;const QString own=dir.filePath("Eigene"),other=dir.filePath("Andere");QDir().mkpath(own+"/Relais");QDir().mkpath(other+"/Alt");
     auto put=[](const QString &f,const QByteArray &b){QFile out(f);return out.open(QIODevice::WriteOnly)&&out.write(b)==b.size();};
     require(put(own+"/Seite.olschlib","{}")&&put(own+"/Relais/Zweite.lib","x")&&put(own+"/Liesmich.txt","t")&&put(other+"/Alt/Weg.olschlib","{}")&&put(other+"/Notiz.txt","n"),"library files");
     LibraryPanel panel;panel.setOwnFolder(own,QStringLiteral("USER"));QString error;
     require(LibraryPanel::backupFiles(own)==QStringList{"Liesmich.txt","Relais/Zweite.lib","Seite.olschlib"}&&LibraryPanel::libraryPageFiles(own)==QStringList{"Relais/Zweite.lib","Seite.olschlib"},
             "the files of a backup and the library pages");
     const QString zip=dir.filePath("b.zip");require(panel.backup(zip,&error)&&LibraryPanel::backupContents(zip)==LibraryPanel::backupFiles(own),"a backup of the own folder");
     int reloads=0;panel.reload=[&]{reloads++;};
     require(panel.restore(zip,&error,other,true)&&!QFileInfo::exists(other+"/Alt/Weg.olschlib")&&QFileInfo::exists(other+"/Notiz.txt")&&QFileInfo::exists(other+"/Relais/Zweite.lib")&&reloads==0,
             "restored into another folder after deleting its library pages, other files kept, the library not read again");
     require(panel.restore(zip,&error)&&reloads==1,"restored into the own folder, the library read again");
     QString bad;require(LibraryPanel::backupContents(dir.filePath("fehlt.zip"),&bad).isEmpty()&&!bad.isEmpty(),"an unreadable backup");
     LibraryBackupDialog create(false,own,LibraryPanel::backupFiles(own));
     require(create.windowTitle()=="Bibliothek-Backup erstellen"&&create.fileList->toPlainText().split('\n').size()==3&&create.clear->isHidden()&&create.ok->text()=="Erstelle Backupdatei..."
             &&!create.clearFirst()&&create.folder()==own,"the dialog for a backup");
     LibraryBackupDialog restoring(true,own,LibraryPanel::backupContents(zip));restoring.clear->setChecked(true);
     require(restoring.windowTitle()=="Bibliothek-Backup einlesen"&&!restoring.clear->isHidden()&&restoring.clearFirst()&&restoring.ok->text()=="Backupdatei einspielen...","the dialog for restoring");}

    // --- The parts list window as in sPlan: the buttons above the list, the font of the list, the printout and RTF files,
    //     the printout's margins and orientation, "?" for the help on lists, the export with a preview; in the child list
    //     letters go on after Z with AA
    {Document d=exampleDocument();Item r=exampleSymbol();r.designator=QStringLiteral("R1");r.value=QStringLiteral("1k");r.pos=QPointF(120,120);assignIds(r);d.sheets[0].items<<r;
     PartsListDialog dialog(d,{},QString());
     QStringList missing;for(const char *name:{"export","insert","clipboard","saveRtf","openRtf","help","print","close"})if(!dialog.findChild<QPushButton*>(name))missing<<name;
     require(missing.isEmpty()&&dialog.findChild<QPushButton*>("openRtf")->text()=="Laden..."&&dialog.fontSize->value()==8&&dialog.topMargin->value()==10&&dialog.leftMargin->value()==20
             &&dialog.portrait->isChecked()&&dialog.grid->rowCount()==3,"the window of sPlan");
     dialog.fontSize->setValue(12);require(dialog.grid->font().pointSize()==12,"the list in the size chosen");
     QTemporaryDir dir;const QString file=dir.filePath("liste.rtf");require(dialog.saveRtf(file),"the list saved");
     {QFile f(file);require(f.open(QIODevice::ReadOnly),"the saved list");const QByteArray rtf=f.readAll();
      require(rtf.contains("\\fs24")&&rtf.contains(dialog.font->currentFont().family().toUtf8()),"the RTF file in the font chosen");}
     int helped=0;dialog.help=[&]{helped++;};dialog.findChild<QPushButton*>("help")->click();require(helped==1,"? asks for the help");
     dialog.landscape->setChecked(true);dialog.topMargin->setValue(15);dialog.leftMargin->setValue(25);
     const QPageLayout page=dialog.printLayout(QPageLayout(QPageSize(QPageSize::A4),QPageLayout::Portrait,QMarginsF(5,5,5,5),QPageLayout::Millimeter));
     const QMarginsF m=page.margins(QPageLayout::Millimeter);
     require(page.orientation()==QPageLayout::Landscape&&std::abs(m.left()-25)<1e-6&&std::abs(m.top()-15)<1e-6&&std::abs(m.right()-5)<1e-6,"the printout's orientation and margins");
     int rows=0,withEmpty=0,withoutNames=0;
     dialog.exportOptions=[&](QDialog *o){auto *preview=o->findChild<QTableWidget*>("exportPreview");rows=preview->rowCount();
         o->findChild<QCheckBox*>("emptyLines")->setChecked(true);withEmpty=preview->rowCount();o->findChild<QCheckBox*>("fieldNames")->setChecked(false);withoutNames=preview->rowCount();
         QTimer::singleShot(0,o,&QDialog::reject);};
     dialog.exportText();
     require(rows==4&&withEmpty==5&&withoutNames==4,"the export's preview: field names, rows and an empty line between the groups");
     Document p=newDocument(QString());Item parent=exampleSymbol();parent.parent=true;parent.designator=QStringLiteral("K1");parent.pos=QPointF(20,20);assignIds(parent);
     Item child=exampleSymbol();child.parentId=parent.id;child.designator=QStringLiteral("K1.1");child.pos=QPointF(150,20);assignIds(child);p.sheets[0].items<<parent<<child;
     TitleBlock &t=p.sheets[0].titleBlock;t.frame=QRectF(0,0,297,210);t.columns=3;t.rows=1;t.columnStart=26;
     ChildListOptions o;o.columns={ChildColumn::RowColumn};o.columnLetters=true;o.rowLetters=false;
     require(childList(p,parent.id,o).rows.value(0)==QStringList{"1AA"},"letters go on with AA after Z in the child list");}

    // --- Suchen/Ersetzen as in sPlan: designator and value each with a replacement and a button of their own, which
    //     replaces in that field alone; texts with theirs; the fields of components or of texts shown
    {Editor x;x.resize(1400,900);x.show();x.setDocument(exampleDocument());QApplication::processEvents();
     x.action("search")->trigger();auto *panel=static_cast<SearchPanel*>(x.findChild<QWidget*>("search"));
     require(panel&&panel->designatorReplace&&panel->valueReplace&&panel->replaceButton&&panel->designatorReplacement->isVisible()&&!panel->replacement->isVisible(),"a replacement beside each field");
     auto components=[&]{QStringList out;for(const auto &p:allComponents(x.document()))out<<p.item->designator+u'='+p.item->value;out.sort();return out;};
     panel->value->setText(QStringLiteral("A"));panel->valueReplacement->setText(QStringLiteral("Z"));panel->designatorReplacement->setText(QStringLiteral("IC"));
     panel->results->setCurrentItem(panel->results->topLevelItem(0));
     require(panel->valueReplace->isEnabled()&&panel->valueReplace->text()==ui("Alle Blätter ->"),"the buttons name what they replace in");
     panel->valueReplace->click();require(components()==QStringList{"U1=Z","U2=B"},"the value replaced, the designator not");
     panel->value->clear();panel->designator->setText(QStringLiteral("U"));panel->results->setCurrentItem(panel->results->topLevelItem(0));
     panel->designatorReplace->click();require(components()==QStringList{"IC1=Z","IC2=B"},"the designators replaced, the values not");
     panel->textsMode->setChecked(true);require(panel->replacement->isVisible()&&!panel->designatorReplacement->isVisible(),"the fields of texts");
     x.markSaved();}

    // --- The arrows of text links as in sPlan: over the outer arrow of a link its target, over the inner arrow of a
    //     target the texts linking to it; a double click on an arrow jumps there
    {Document d=exampleDocument();
     Item target;target.type=ItemType::Text;target.text=QStringLiteral("Netzteil");target.pos=QPointF(60,40);target.linkable=true;assignIds(target);
     Item link;link.type=ItemType::Text;link.text=QStringLiteral("siehe Netzteil");link.pos=QPointF(60,60);link.linkTarget=target.id;assignIds(link);
     d.sheets[1].items<<target;d.sheets[0].items<<link;d.activeSheet=0;
     Editor x;x.resize(1400,900);x.setDocument(d);SheetView *view=x.view();
     auto arrowAt=[&](const Item &t,bool outgoing){return textFrame(t).map(linkArrow(textRect(t,t.text),outgoing)).boundingRect().center();};
     bool outgoing=false;
     require(view->linkArrowAt(arrowAt(link,true),&outgoing)==link.id&&outgoing&&view->linkArrowAt(QPointF(5,5)).isEmpty(),"the outer arrow of a link");
     const QString window=view->linkWindowText(link.id,true);
     require(window.contains(QStringLiteral("Netzteil"))&&window.contains(QStringLiteral("2: ")),"its window names the target and its sheet");
     view->linkArrowActivated(link.id,true);
     require(x.document().activeSheet==1&&x.view()->selection()==QStringList{target.id},"a double click on the outer arrow goes to the target");
     require(view->linkArrowAt(arrowAt(target,false),&outgoing)==target.id&&!outgoing&&view->linkWindowText(target.id,false).contains(QStringLiteral("siehe Netzteil")),
             "the inner arrow of a target names the texts linking to it");
     view->linkArrowActivated(target.id,false);
     require(x.document().activeSheet==0&&x.view()->selection()==QStringList{link.id},"a double click on the inner arrow goes to the link");
     x.markSaved();}

    // --- A parent with children kept in the library, as in sPlan: a triangle on its cell, the children beside it while
    //     the pointer is over the triangle, placed together and linked to it; a selected parent copied into the library
    //     takes its selected children along, and the command says so
    {QTemporaryDir dir;Editor x;x.resize(1400,900);x.show();x.setDocument(exampleDocument());QApplication::processEvents();
     LibraryPanel *library=x.library();
     Item parent=exampleSymbol();parent.parent=true;parent.designator=QStringLiteral("IC?");parent.autoNumber=true;
     Item gate=exampleSymbol();gate.designator=QStringLiteral("<PARENT_ID>-<CHILDNO>");gate.autoNumber=false;gate.pos=QPointF(30,0);
     Item second=gate;second.pos=QPointF(60,0);
     {LibraryPage page;page.name=QStringLiteral("Parents");
      page.entries<<LibraryEntry{QStringLiteral("7400"),parent,{},{gate,second}}<<LibraryEntry{QStringLiteral("Beispiel"),exampleSymbol(),{},{}};library->setPages({page});}
     library->symbolList()->doItemsLayout();QApplication::processEvents();
     const QRect mark=library->childrenMark(0);
     require(!mark.isEmpty()&&library->childrenMark(1).isEmpty(),"a triangle only at a parent with children");
     library->hoverAt(mark.center());
     require(library->childrenPopup()&&library->childrenPopup()->isVisible()&&library->childrenPopup()->pixmap().width()>library->symbolList()->iconSize().width(),
             "its children beside the cell");
     library->hoverAt(QPoint(-1,-1));require(!library->childrenPopup()->isVisible(),"gone when the pointer leaves");
     library->chosen(library->pages()[0].entries[0]);require(x.view()->placing(),"the parent follows the pointer");
     click(x.view(),QPointF(40,150));
     QString parentId;QPointF at;QStringList kids;
     for(const auto *c:components(x.document().sheet()))if(c->parent){parentId=c->id;at=c->pos;}
     for(const auto *c:components(x.document().sheet()))if(!parentId.isEmpty()&&c->parentId==parentId){
         require(near(c->pos,at+QPointF(30,0))||near(c->pos,at+QPointF(60,0)),"a child at its place beside the parent");kids<<c->id;}
     require(kids.size()==2,"placed with its two children, linked to it");
     x.view()->setSelection(QStringList{parentId}+kids);
     require(x.action("copyToLibrary")->text()==ui("Markiertes Bauteil (mit Children) in die Bibliothek &kopieren"),"the command names the children, as in sPlan");
     library->setOwnFolder(dir.filePath("Bibliothek"),QStringLiteral("Bibliothek"));QString error;
     require(library->newPage(QStringLiteral("Eigene"),&error),"an own page");
     x.copyToLibrary(false);
     const auto &stored=library->pages()[library->currentPage()].entries;
     require(stored.size()==1&&stored[0].symbol.parent&&stored[0].children.size()==2&&stored[0].children[0].parentId.isEmpty()
             &&(near(stored[0].children[0].pos,QPointF(30,0))||near(stored[0].children[0].pos,QPointF(60,0))),"one entry, the parent with its children relative to it");
     x.markSaved();}

    // --- Named outlines ("Voreinstellungen" in the outline section), in the test's settings file as well
    {Document d=exampleDocument();Item a;a.type=ItemType::Line;a.points={QPointF(10,10),QPointF(30,10)};a.pen={QColor(0,0,255),.5,PenStyle::Dash};a.pen.inner=true;assignIds(a);
        Item b=a;b.pen.color=QColor(255,0,0);b.points={QPointF(10,20),QPointF(30,20)};assignIds(b);
        Item r;r.type=ItemType::Rectangle;r.centre=QPointF(50,50);r.size=QSizeF(10,10);r.pen.style=PenStyle::None;assignIds(r);
        Item tb;tb.type=ItemType::TextBox;tb.centre=QPointF(80,50);tb.size=QSizeF(20,10);assignIds(tb);
        d.sheets[0].items<<a<<b<<r<<tb;
        Editor x;x.resize(1400,900);x.setDocument(d);auto *panel=static_cast<PropertiesPanel*>(x.findChild<QWidget*>("properties"));
        auto find=[&](const QString &id){for(const auto &i:x.document().sheet().items)if(i.id==id)return i;return Item();};
        for(const QString &n:PropertiesPanel::linePresets())PropertiesPanel::removeLinePreset(n);
        x.view()->setSelection({a.id});QApplication::processEvents();
        require(panel->addLinePreset(QStringLiteral("Blau gestrichelt"))&&PropertiesPanel::linePresets()==QStringList{QStringLiteral("Blau gestrichelt")}&&!QSettings().value("schematic/linePresets").toByteArray().isEmpty(),
                "an outline kept under its name");
        x.view()->setSelection({a.id,b.id});QApplication::processEvents();require(!panel->addLinePreset(QStringLiteral("Gemischt"))&&PropertiesPanel::linePresets().size()==1,"not from outlines that differ");
        x.view()->setSelection({r.id});QApplication::processEvents();panel->applyLinePreset(QStringLiteral("Blau gestrichelt"));
        require(find(r.id).pen==a.pen,"given to a rectangle");
        x.view()->setSelection({tb.id});QApplication::processEvents();panel->applyLinePreset(QStringLiteral("Blau gestrichelt"));
        require(find(tb.id).pen.color==QColor(0,0,255)&&!find(tb.id).pen.inner,"a text frame without the stripe");
        x.view()->setSelection({b.id});QApplication::processEvents();
        {auto *menu=x.findChild<QToolButton*>("linePresets")->menu();emit menu->aboutToShow();
            QAction *standard=nullptr;for(auto *act:menu->actions())if(act->text()==ui("Standard"))standard=act;require(standard,"the standard outline in the menu");
            standard->trigger();require(find(b.id).pen==Pen(),"the standard outline given");}
        x.undo();require(find(b.id).pen.color==QColor(255,0,0),"one undo step");
        PropertiesPanel::removeLinePreset(QStringLiteral("Blau gestrichelt"));require(PropertiesPanel::linePresets().isEmpty(),"removed");
        x.markSaved();}

    // --- The extended text input: fixed variables as in sPlan, user variables and text constants with "Definieren…",
    // names twice refused, special characters by "Ω" and Strg+Einfg (in the test's settings file as well)
    {// Every fixed variable of the menu is one the texts know.
        std::unique_ptr<QMenu> menu(TextDialog::fixedVariables(nullptr));QStringList names;
        for(auto *a:menu->findChildren<QAction*>())if(a->data().isValid())names<<a->data().toString();
        require(names.size()==116&&names.contains("<PAGESCALE>")&&names.contains("<PARENT_ID_NUMBER>")&&names.contains("<PARENT_CONTACT_9>")&&names.contains("<CHILD_ROWCHAR_9>")
                &&names.contains("<LINKFROM_ROWCHAR>"),"the fixed variables in the reference's groups");
        Document d=exampleDocument();auto &items=d.sheets[0].items;items[1].parentId=items[0].id;
        Item t;t.type=ItemType::Text;t.text=QStringLiteral("x");assignIds(t);
        const TextContext c{&d,0,&items[1],QStringLiteral("/tmp/Plan.olsch"),&t};QStringList unknown;
        for(const auto &n:names)if(expandVariables(n,c)==n)unknown<<n;
        require(unknown.isEmpty(),("variables of the menu the texts do not know: "+unknown.join(' ')).toStdString().c_str());}
    {TextDialog dialog(QString(),{QStringLiteral("Autor")},nullptr,{},QStringLiteral("Arial"));
        require(dialog.user->isEnabled()&&dialog.user->menu()->actions().size()==1&&!dialog.constant->isEnabled(),"user variables offered, no text constants");
        dialog.user->menu()->actions()[0]->trigger();require(dialog.text()=="<Autor>","a user variable put in");
        dialog.defineVariables=[]{return QStringList{"A","B"};};dialog.defineConstants=[]{return QStringList{"5 V"};};
        dialog.defineUser->click();dialog.defineConstant->click();
        require(dialog.user->menu()->actions().size()==2&&dialog.constant->isEnabled()&&dialog.constant->menu()->actions().size()==1,"the lists anew after \"Definieren…\"");}
    {// In the editor "Definieren…" changes the document (one undo step) and the settings; text boxes get the user variables too.
        Document d=exampleDocument();Item t;t.type=ItemType::Text;t.text=QStringLiteral("alt");assignIds(t);
        Item box;box.type=ItemType::TextBox;box.centre=QPointF(80,50);box.size=QSizeF(20,10);box.text=QStringLiteral("b");assignIds(box);
        d.sheets[0].items<<t<<box;d.variables.clear();QSettings().remove("schematic/textConstants");
        Editor x;x.setDocument(d);QStringList offered,constants;
        x.variablesShown=[](VariablesDialog *v){v->table->setItem(0,0,new QTableWidgetItem(QStringLiteral("Projekt")));v->table->setItem(0,1,new QTableWidgetItem(QStringLiteral("OpenLoch")));QTimer::singleShot(0,v,&QDialog::accept);};
        x.textConstantsShown=[](TextConstantsDialog *k){k->table->insertRow(0);k->table->setItem(0,0,new QTableWidgetItem(QStringLiteral("Netzteil")));QTimer::singleShot(0,k,&QDialog::accept);};
        x.textDialogShown=[&](TextDialog *dialog){
            dialog->defineUser->click();dialog->defineConstant->click();
            for(auto *a:dialog->user->menu()->actions())offered<<a->text();
            for(auto *a:dialog->constant->menu()->actions())constants<<a->text();
            QTimer::singleShot(0,dialog,&QDialog::reject);};
        x.editText(t.id);
        require(offered==QStringList{"<Projekt>"}&&constants==QStringList{"Netzteil"},"defined from the text input and offered at once");
        require(x.document().variables.size()==1&&x.document().variables[0].value=="OpenLoch"&&QSettings().value("schematic/textConstants").toStringList()==QStringList{"Netzteil"},"kept in the document and the settings");
        x.undo();require(x.document().variables.isEmpty(),"the variables one undo step");
        x.variablesShown=nullptr;x.textConstantsShown=nullptr;offered.clear();bool userOn=false;
        x.textDialogShown=[&](TextDialog *dialog){userOn=dialog->user->isEnabled();QTimer::singleShot(0,dialog,&QDialog::reject);};
        x.redo();x.editText(box.id);require(userOn,"user variables in text boxes as well");
        QSettings().remove("schematic/textConstants");x.markSaved();}
    {// A name twice, in any case, is refused with a message.
        auto closeBox=[]{auto *timer=new QTimer;timer->setInterval(10);
            QObject::connect(timer,&QTimer::timeout,[timer]{if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){timer->stop();timer->deleteLater();box->accept();}});timer->start();};
        VariablesDialog v({{QStringLiteral("Autor"),QStringLiteral("a")},{QStringLiteral("AUTOR"),QStringLiteral("b")}});
        closeBox();v.accept();require(v.result()!=QDialog::Accepted,"a name twice is refused");
        v.table->item(1,0)->setText(QStringLiteral("Firma"));v.accept();require(v.result()==QDialog::Accepted,"different names are taken");
        VariablesDialog w({});w.findChild<QPushButton*>("newRow")->click();require(w.table->rowCount()==1,"the empty row is used for a new one");
        w.table->item(0,0)->setText(QStringLiteral("A"));w.findChild<QPushButton*>("newRow")->click();require(w.table->rowCount()==2,"a new row after a filled one");
        w.table->setCurrentCell(0,0);w.findChild<QPushButton*>("deleteRow")->click();require(w.variables().isEmpty()&&w.table->rowCount()==1,"a row deleted");
    // Special characters: the font's characters twenty in a row, the jumps, the last ones taken.
        CharacterDialog::recent().clear();
        {CharacterDialog c(QStringLiteral("Arial"));require(c.grid->model()->columnCount()==20&&c.grid->model()->rowCount()>3,"the characters of the font twenty in a row");
            require(c.jumpTo(U'A')&&c.grid->currentIndex().data().toString()=="A"&&!c.jumpTo(0x7F),"a jump to a character, none to a control character");
            closeBox();require(!c.take(0x7F)&&c.chosen().isEmpty(),"a character the font lacks is not taken");
            require(c.take(U'B')&&c.chosen()=="B"&&c.result()==QDialog::Accepted,"a character taken");}
        for(char32_t k:{U'C',U'B'}){CharacterDialog c{QString()};c.take(k);}
        require(CharacterDialog::recent()==QList<char32_t>({U'B',U'C'}),"the last characters, newest first and each once");
        for(char32_t k=U'a';k<=U'y';k++){CharacterDialog c(QStringLiteral("Arial"));c.take(k);}
        {require(CharacterDialog::recent().size()==20&&CharacterDialog::recent().first()==U'y',"twenty kept");
            CharacterDialog c(QStringLiteral("Arial"));require(c.last->item(0,0)->text()=="y"&&c.last->item(0,19)->text()=="f","shown below the table");}
        CharacterDialog::shown=[](CharacterDialog *c){QTimer::singleShot(0,c,[c]{c->take(U'Q');});};
        {TextDialog dialog(QStringLiteral("a"),{},nullptr,{},QStringLiteral("Arial"));dialog.special->click();
            require(dialog.text()=="aQ","\"Ω\" puts a special character in");
            QKeyEvent over(QEvent::ShortcutOverride,Qt::Key_Insert,Qt::ControlModifier);QApplication::sendEvent(dialog.edit,&over);
            require(over.isAccepted(),"Strg+Einfg taken from the shortcuts");
            QKeyEvent press(QEvent::KeyPress,Qt::Key_Insert,Qt::ControlModifier);QApplication::sendEvent(dialog.edit,&press);
            require(dialog.text()=="aQQ","and asks for a special character");}
        {Document d=exampleDocument();Item box;box.type=ItemType::TextBox;box.centre=QPointF(80,50);box.size=QSizeF(20,10);box.text=QStringLiteral("b");assignIds(box);
            d.sheets[0].items<<box;Editor x;x.resize(1400,900);x.setDocument(d);x.view()->setSelection({box.id});QApplication::processEvents();
            auto *field=x.findChild<QWidget*>("properties")->findChild<QPlainTextEdit*>("text");require(field,"the text field of a text box, over several lines");
            field->moveCursor(QTextCursor::End);QKeyEvent press(QEvent::KeyPress,Qt::Key_Insert,Qt::ControlModifier);QApplication::sendEvent(field,&press);
            require(field->toPlainText()=="bQ","Strg+Einfg in the field of a text box");
            field->setPlainText(QStringLiteral("eins\nzwei"));field->document()->setModified(true);QFocusEvent out(QEvent::FocusOut);QApplication::sendEvent(field,&out);
            {Item now;for(const auto &i:x.document().sheet().items)if(i.id==box.id)now=i;require(now.text=="eins\nzwei","taken over several lines when the field is left");}
            x.markSaved();}
        CharacterDialog::shown=nullptr;CharacterDialog::recent().clear();}

    // --- Clipboard, print preview and sPlan's sheet files: the reference's choices of resolution and area, the scale as
    // a field, the four ways of resetting the offset, the count of cut sheets, the sheets in a menu, ".blt"
    {Document d=exampleDocument();Item l;l.type=ItemType::Line;l.points={QPointF(100,100),QPointF(140,120)};l.pen.width=1;assignIds(l);d.sheets[0].items<<l;
        Editor x;x.setDocument(d);
        {ClipboardDialog dialog(false);require(dialog.all->isChecked()&&!dialog.selected->isEnabled()&&dialog.resolution()==300,"without a selection: all, 300 dpi");
            dialog.dpi150->setChecked(true);dialog.accept();}
        {ClipboardDialog dialog(true);require(dialog.selected->isChecked()&&dialog.selectionOnly()&&dialog.resolution()==150,"with one the selection, the resolution kept");
            dialog.dpi300->setChecked(true);dialog.accept();}
        x.view()->setSelection({l.id});
        {const QImage image=x.clipboardImage(254,true);require(image.width()==490&&image.height()==290,"the selection with 4 mm around it");
            const QRectF all=bounds(x.document().sheet().items).adjusted(-4,-4,4,4);const QImage whole=x.clipboardImage(127,false);
            require(whole.width()==int(std::lround(all.width()*5))&&whole.height()==int(std::lround(all.height()*5)),"all elements in their frame");}
        bool asked=false;x.clipboardShown=[&](ClipboardDialog *dialog){asked=dialog->selected->isChecked();QTimer::singleShot(0,dialog,&QDialog::reject);};
        x.action("copyImage")->trigger();require(asked,"\"Zwischenablage...\" asks first");x.clipboardShown=nullptr;
        // Print preview.
        QList<PrintSettings> settings;PrintPreview preview(x.document(),settings,QString());preview.setSheet(0);
        preview.scalePercent->setValue(250);require(preview.freeScale->isChecked()&&std::abs(settings[0].scale-2.5)<1e-9&&preview.scaleSlider->value()==250,"the scale typed in, the slider with it");
        preview.scaleSlider->setValue(10);require(preview.scalePercent->value()==10&&std::abs(settings[0].scale-.1)<1e-9,"and the other way, down to 10 %");
        require(preview.scalePercent->maximum()==800&&preview.scaleSlider->minimum()==10,"10 to 800 % as in the reference");
        {const QPageLayout page=pageFor(preview.printer()->pageLayout(),x.document().sheets[0],settings[0]);
            const QPointF margin=page.paintRect(QPageLayout::Millimeter).topLeft(),content=printedContent(x.document().sheets[0]).topLeft();
            auto round=[](QPointF p){return QPointF(std::round(p.x()*10)/10,std::round(p.y()*10)/10);};
            const QList<QAction*> ways=preview.resetButton->menu()->actions();require(ways.size()==4,"four ways to reset the offset");
            const QPointF expected[]={round(content-margin),round(content),round(-margin),QPointF()};
            for(int i=0;i<4;i++){ways[i]->trigger();require(near(settings[0].offset,expected[i]),("offset reset way "+std::to_string(i+1)).c_str());}
            require(near(sheetOrigin(page,x.document().sheets[0],[&]{PrintSettings s;s.offset=content;return s;}()),margin),"the sheet's corner then at the printable corner (1:1)");}
        PrintSettings far;far.offset=QPointF(-80,-80);preview.apply(far);
        {int marked=0;for(int i=0;i<preview.sheetBar->count();i++)marked+=preview.sheetBar->tabText(i).endsWith("[!]");
            require(marked>0&&!preview.cutCount->isHidden()&&preview.cutCount->text()==ui("Blätter abgeschnitten:")+QStringLiteral(" %1").arg(marked),"cut sheets counted");}
        {Document one=x.document();one.sheets.resize(1);QList<PrintSettings> fine;PrintPreview whole(one,fine,QString());
            require(whole.cutCount->isHidden(),"and the count hidden without them");}
        emit preview.sheetMenu->aboutToShow();require(preview.sheetMenu->actions().size()==2&&preview.sheetMenu->actions()[0]->isChecked(),"the sheets in the bar's menu, the current one ticked");
        preview.sheetMenu->actions()[1]->trigger();require(preview.sheet()==1,"chosen from it");
        // sPlan's sheet files.
        QTemporaryDir dir;QString error;
        require(x.saveSheet(dir.filePath("blatt.blt"),&error),"the sheet as sPlan's sheet file");
        {QFile f(dir.filePath("blatt.blt"));require(f.open(QIODevice::ReadOnly)&&splan::version(f.readAll())==80,"an sPlan 8 file");}
        const int before=int(x.document().sheets.size());
        require(x.loadSheets(dir.filePath("blatt.blt"),&error)&&x.document().sheets.size()==before+1,"read back as a sheet");
        x.markSaved();}

    // --- Window and panels: help, the second toolbar's panels and problems, the list of sheets, quick sheets, recent
    // files (in the test's settings file)
    {Document d=exampleDocument();Editor x;x.resize(1400,900);x.setDocument(d);x.show();QApplication::processEvents();
        QMenu *help=nullptr;for(auto *a:x.menuBar()->actions())if(a->menu()&&a->text()==ui("&Hilfe"))help=a->menu();
        require(help&&help->actions().contains(x.action("helpTopics"))&&help->actions().contains(x.action("about"))&&x.action("helpTopics")->shortcuts().contains(QKeySequence(Qt::Key_F1)),
                "a Hilfe menu with Hilfethemen (F1) and Info");
        x.showHelp();{auto *page=x.findChild<QTextBrowser*>("helpPage");require(page&&page->toPlainText().size()>500,"the help pages");}
        {auto *bar=x.findChild<QToolBar*>("mainToolBar");
            for(const char *n:{"renumber","partsList","showSheetList","showProperties","problems"})require(bar->actions().contains(x.action(n)),(std::string("on the toolbar: ")+n).c_str());}
        auto *properties=x.findChild<QDockWidget*>("propertiesDock");auto *sheets=x.findChild<QDockWidget*>("sheetsDock");
        require(properties&&sheets&&!properties->isHidden()&&sheets->isHidden()&&(properties->features()&QDockWidget::DockWidgetFloatable),"the properties shown, the list of sheets hidden, both can be set free");
        x.action("showProperties")->trigger();x.action("showSheetList")->trigger();require(properties->isHidden()&&!sheets->isHidden(),"switched from the toolbar");
        x.action("showProperties")->trigger();require(!properties->isHidden(),"and back");
        // The list of sheets.
        auto *list=x.findChild<QTableWidget*>("sheetList");
        require(list->rowCount()==2&&list->item(1,0)->text()==d.sheets[1].name&&list->currentRow()==x.document().activeSheet,"a row per sheet, the current one chosen");
        list->setCurrentCell(1,0);require(x.document().activeSheet==1,"a row chosen, its sheet shown");
        const QString first=x.document().sheets[0].id;list->verticalHeader()->moveSection(0,1);
        require(x.document().sheets[1].id==first&&list->item(1,0)->text()==d.sheets[0].name,"dragged to reorder the sheets");
        x.undo();require(x.document().sheets[0].id==first,"one undo step");
        // "+": an empty sheet or a copy of one.
        {auto *quick=x.findChild<QToolButton*>("quickSheet");emit quick->menu()->aboutToShow();
            QMenu *copies=nullptr;for(auto *a:quick->menu()->actions())if(a->menu())copies=a->menu();
            require(copies&&copies->actions().size()==2,"\"Kopie von\" lists the sheets");
            copies->actions()[0]->trigger();require(x.document().sheets.size()==3&&x.document().sheets[2].name==d.sheets[0].name&&x.document().activeSheet==2,"a copy of the first sheet at the end");
            emit quick->menu()->aboutToShow();quick->menu()->actions()[0]->trigger();require(x.document().sheets.size()==4,"an empty one");}
        // Problems: elements wholly outside the sheet and pictures above 300 dpi.
        {Document p=exampleDocument();Item left;left.type=ItemType::Line;left.points={QPointF(-50,20),QPointF(-30,20)};assignIds(left);
            Item below;below.type=ItemType::Rectangle;below.centre=QPointF(100,p.sheets[0].height+30);below.size=QSizeF(10,10);assignIds(below);
            Item edge;edge.type=ItemType::Line;edge.points={QPointF(-5,20),QPointF(5,20)};assignIds(edge);
            QImage big(1000,800,QImage::Format_RGB32);big.fill(Qt::white);QByteArray bytes;{QBuffer b(&bytes);b.open(QIODevice::WriteOnly);big.save(&b,"PNG");}
            p.resources.insert(QStringLiteral("big"),Resource{QStringLiteral("png"),bytes});
            Item picture;picture.type=ItemType::Image;picture.resource=QStringLiteral("big");picture.centre=QPointF(50,50);picture.size=QSizeF(20,16);assignIds(picture);
            p.sheets[0].items<<left<<below<<edge<<picture;Editor y;y.setDocument(p);
            const auto found=y.problems();require(found.outside==QStringList({left.id,below.id})&&found.pictures==1&&y.action("problems")->isVisible(),"two elements outside, one picture of 1270 dpi, the button shown");
            y.moveOntoSheet(found.outside);
            auto get=[&](const QString &id){for(const auto &i:y.document().sheet().items)if(i.id==id)return i;return Item();};
            require(std::abs(bounds(get(left.id)).left())<1e-9&&std::abs(bounds(get(below.id)).bottom()-p.sheets[0].height)<1e-9,"moved onto the sheet's edge as the reference");
            require(y.problems().outside.isEmpty()&&y.view()->selection()==found.outside,"none left outside, the moved ones selected");
            y.undo();bool chosen=false;
            y.problemShown=[&](ProblemDialog *dialog){chosen=dialog->deleteButton&&dialog->picturesButton;QTimer::singleShot(0,dialog->deleteButton,&QPushButton::click);};
            y.problemDialog();require(chosen&&y.problems().outside.isEmpty()&&y.document().sheet().items.size()==p.sheets[0].items.size()-2,"or deleted from the problem dialog");
            y.markSaved();}
        // The print preview's help button.
        {QList<PrintSettings> settings;PrintPreview preview(x.document(),settings,QString());bool asked=false;preview.helpRequested=[&]{asked=true;};
            preview.findChild<QPushButton*>("help")->click();require(asked,"\"?\" in the print preview");}
        // Recent files, newest first, eight at most; not with a host's own Öffnen.
        {QSettings().remove("schematic/recentFiles");Editor r;r.loadPreferences();QTemporaryDir dir;QString error;
            for(int i=0;i<10;i++){r.setDocument(exampleDocument());require(r.saveFile(dir.filePath(QStringLiteral("plan%1.olsch").arg(i)),&error),"saved");}
            require(r.openFile(dir.filePath("plan3.olsch"),&error),"opened");
            const QStringList recent=r.recentFiles();
            require(recent.size()==8&&QFileInfo(recent[0]).fileName()=="plan3.olsch"&&QFileInfo(recent[1]).fileName()=="plan9.olsch","the last eight, newest first");
            QMenu *file=nullptr;for(auto *a:r.menuBar()->actions())if(a->menu()&&a->text()==ui("&Datei"))file=a->menu();
            emit file->aboutToShow();int shown=0;for(auto *a:file->actions())shown+=a->objectName()=="recentFile";require(shown==8,"in the Datei menu");
            r.openHandler=[](const QString &){return true;};emit file->aboutToShow();shown=0;for(auto *a:file->actions())shown+=a->objectName()=="recentFile";
            require(shown==0,"a host keeps its own list");
            QSettings().remove("schematic/recentFiles");r.markSaved();}
        x.markSaved();}

    // --- Properties as in the reference: memo fields, "…" at designator and value, colour depth of pictures
    {auto encoded=[](const QImage &image,const char *format){QByteArray bytes;QBuffer b(&bytes);b.open(QIODevice::WriteOnly);image.save(&b,format);return bytes;};
        QImage rgb(8,8,QImage::Format_RGB32);rgb.fill(Qt::red);
        require(bitsPerPixel(encoded(rgb,"PNG"))==24&&bitsPerPixel(encoded(rgb,"BMP"))==24&&bitsPerPixel(encoded(rgb,"JPG"))==24,"24 bit as PNG, BMP and JPEG");
        QImage alpha(8,8,QImage::Format_ARGB32);alpha.fill(QColor(0,0,255,100));require(bitsPerPixel(encoded(alpha,"PNG"))==32,"32 bit with alpha");
        require(bitsPerPixel(encoded(rgb.convertToFormat(QImage::Format_Indexed8),"PNG"))==8&&bitsPerPixel("nichts")==0,"8 bit with a palette, unknown: 0");
        Document d=exampleDocument();d.resources.insert(QStringLiteral("p"),Resource{QStringLiteral("png"),encoded(rgb,"PNG")});
        Item picture;picture.type=ItemType::Image;picture.resource=QStringLiteral("p");picture.centre=QPointF(50,50);picture.size=QSizeF(20,20);assignIds(picture);d.sheets[0].items<<picture;
        Editor x;x.resize(1400,900);x.setDocument(d);auto *panel=x.findChild<QWidget*>("properties");
        x.view()->setSelection({picture.id});QApplication::processEvents();
        require(panel->findChild<QLabel*>("imageBits")&&panel->findChild<QLabel*>("imageBits")->text()==ui("%1 Bit").arg(24),"the colour depth in the panel");
        // The sheet's description over several lines.
        x.view()->setSelection({});QApplication::processEvents();
        {auto *description=panel->findChild<QPlainTextEdit*>("sheetDescription");require(description,"the sheet's description over several lines");
            description->setPlainText(QStringLiteral("Zeile 1\nZeile 2"));description->document()->setModified(true);QFocusEvent out(QEvent::FocusOut);QApplication::sendEvent(description,&out);
            QApplication::processEvents();require(x.document().sheet().description=="Zeile 1\nZeile 2","taken when the field is left");x.undo();require(x.document().sheet().description!="Zeile 1\nZeile 2","one undo step");}
        // "…" at a component's designator: the extended text input.
        {const QString id=x.document().sheet().items[0].id;x.view()->setSelection({id});QApplication::processEvents();
            x.textDialogShown=[](TextDialog *dialog){dialog->edit->setPlainText(QStringLiteral("R<PAGENO>"));QTimer::singleShot(0,dialog,&QDialog::accept);};
            auto *more=panel->findChild<QToolButton*>("designatorDialog");require(more&&panel->findChild<QToolButton*>("valueDialog"),"\"…\" at designator and value");
            more->click();Item now;for(const auto &i:x.document().sheet().items)if(i.id==id)now=i;
            require(now.designator=="R<PAGENO>","the designator from the extended text input");x.textDialogShown=nullptr;}
        x.markSaved();}

    // --- Alt on an element of a group chooses it alone, as in the reference: its properties change without ungrouping,
    // it is not moved or deleted on its own
    {Document d=exampleDocument();d.sheets[0].items.clear();
        Item a;a.type=ItemType::Rectangle;a.centre=QPointF(50,50);a.size=QSizeF(20,20);
        Item b=a;b.centre=QPointF(90,50);Item c=a;c.centre=QPointF(130,50);
        Item inner;inner.type=ItemType::Group;inner.children={b,c};
        Item outer;outer.type=ItemType::Group;outer.children={a,inner};assignIds(outer);d.sheets[0].items<<outer;
        Editor x;x.resize(1400,900);x.setDocument(d);x.show();QApplication::processEvents();auto *v=x.view();auto *panel=x.findChild<QWidget*>("properties");
        const Item &g=x.document().sheet().items[0];const QString groupId=g.id,aId=g.children[0].id,bId=g.children[1].children[0].id;
        click(v,{40,44});require(v->selection()==QStringList{groupId},"a click takes the whole group");
        click(v,{40,44},Qt::LeftButton,Qt::AltModifier);require(v->selection()==QStringList{aId}&&v->nestedSelection(),"with Alt the element alone");
        require(!x.action("delete")->isEnabled()&&!x.action("cut")->isEnabled(),"not deleted or cut on its own");
        QApplication::processEvents();
        {auto *width=panel->findChild<QDoubleSpinBox*>("boxWidth");require(width,"its properties in the panel");width->setValue(30);}
        require(std::abs(x.document().sheet().items[0].children[0].size.width()-30)<1e-9&&x.document().sheet().items[0].type==ItemType::Group,"changed inside the group, which stays");
        require(v->selection()==QStringList{aId},"still chosen after the change");
        x.undo();require(std::abs(x.document().sheet().items[0].children[0].size.width()-20)<1e-9,"one undo step");
        click(v,{80,44},Qt::LeftButton,Qt::AltModifier);require(v->selection()==QStringList{bId},"in nested groups the innermost element");
        x.deleteSelection();require(x.document().sheet().items.size()==1&&x.document().sheet().items[0].children.size()==2,"delete leaves the group whole");
        click(v,{200,150});require(v->selection().isEmpty(),"a click beside it ends the choice");
        x.markSaved();}

    // --- Presets: text boxes have their own, as in the reference, kept for the next start (in the test's settings file)
    {QSettings().remove("schematic/presets/textBox");QSettings().remove("schematic/presets/text");
        {Editor x;x.resize(1400,900);x.show();x.loadPreferences();QApplication::processEvents();
            x.view()->setTool(SheetView::Tool::TextBox);QApplication::processEvents();auto *panel=x.findChild<QWidget*>("properties");
            auto *height=panel->findChild<QDoubleSpinBox*>("textHeight");require(height,"the text box preset in the panel");height->setValue(70);
            require(std::abs(x.view()->textBoxPreset.font.height-7)<1e-9&&std::abs(x.view()->textPreset.font.height-3)<1e-9,"changed apart from the text preset");
            x.view()->setTool(SheetView::Tool::Text);QApplication::processEvents();require(std::abs(panel->findChild<QDoubleSpinBox*>("textHeight")->value()-30)<1e-9,"texts keep theirs");
            x.markSaved();x.close();}
        {Editor y;y.loadPreferences();require(std::abs(y.view()->textBoxPreset.font.height-7)<1e-9&&y.view()->textBoxPreset.type==ItemType::TextBox,"kept for the next start");
            QSettings().remove("schematic/presets/textBox");QSettings().remove("schematic/presets/text");y.markSaved();}}

    // --- Bauteilbeschriftung with a colour for each group and a preview
    {LetteringDialog dialog(false,false);
        require(dialog.colour.size()==3&&dialog.lettering(0).color==QColor(0,0,0)&&dialog.preview&&!dialog.preview->pixmap().isNull(),"a colour for each group and a preview");
        const QImage before=dialog.preview->pixmap().toImage();
        dialog.colours[0]=QColor(200,0,0);dialog.showPreview();require(dialog.preview->pixmap().toImage()!=before&&dialog.lettering(0).color==QColor(200,0,0),"the preview in the colour");
        Item c=exampleSymbol();Lettering red=dialog.lettering(0);red.set=true;letter(c,red,{},{});
        bool coloured=false;for(const auto &k:c.children)if(k.role==TextRole::Designator)coloured=k.font.color==QColor(200,0,0);
        require(coloured,"the designator takes the colour");}

    // --- Position as in the reference: each element by its own reference point, or the selection as one block
    {Document d=exampleDocument();d.sheets[0].items.clear();
        Item a;a.type=ItemType::Rectangle;a.centre=QPointF(50,50);a.size=QSizeF(20,10);a.pen.width=0;assignIds(a);
        Item b=a;b.centre=QPointF(100,80);b.size=QSizeF(10,10);assignIds(b);d.sheets[0].items<<a<<b;
        Editor x;x.resize(1400,900);x.setDocument(d);auto *panel=x.findChild<QWidget*>("properties");
        auto get=[&](const QString &id){for(const auto &i:x.document().sheet().items)if(i.id==id)return i;return Item();};
        x.view()->setSelection({a.id,b.id});QApplication::processEvents();
        auto *asGroup=panel->findChild<QCheckBox*>("asGroup");auto *align=panel->findChild<QPushButton*>("positionAlign");
        require(asGroup&&!asGroup->isChecked()&&asGroup->isEnabled()&&align&&align->isEnabled(),"off at first, \"Ausrichten\" offered");
        panel->findChild<QDoubleSpinBox*>("positionX")->setValue(10);
        require(std::abs(bounds(get(a.id)).left()-10)<1e-9&&std::abs(bounds(get(b.id)).left()-10)<1e-9&&std::abs(get(b.id).centre.y()-80)<1e-9,"each element's left side to X, Y unchanged");
        x.undo();QApplication::processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        // The panel is built anew after the switch; its old fields go with the next deferred deletion.
        panel->findChild<QCheckBox*>("asGroup")->setChecked(true);QApplication::processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        const double gap=bounds(get(b.id)).left()-bounds(get(a.id)).left();
        require(!panel->findChild<QPushButton*>("positionAlign")->isEnabled()&&std::abs(panel->findChild<QDoubleSpinBox*>("positionX")->value()-bounds(get(a.id)).left())<.006,"as a group: the frame of both (to the field's hundredths), no aligning");
        panel->findChild<QDoubleSpinBox*>("positionX")->setValue(10);
        require(std::abs(bounds(get(a.id)).left()-10)<1e-9&&std::abs(bounds(get(b.id)).left()-10-gap)<1e-9,"moved as one block");
        // In the sheet's scale.
        x.change([](Document &dd){dd.sheet().scale=2;});x.view()->setSelection({b.id});QApplication::processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        require(std::abs(panel->findChild<QDoubleSpinBox*>("positionX")->value()-bounds(get(b.id)).left()*2)<.006,"X in the sheet's scale");
        x.markSaved();}

    // --- Junction (Automatik, Auto-Size, Reset), Normalisieren, Textrichtung umkehren in the panel
    {Document d=exampleDocument();d.sheets[0].items.clear();
        Item line;line.type=ItemType::Line;line.points={QPointF(20,40),QPointF(120,40)};line.pen.width=.5;line.pen.color=QColor(0,0,200);assignIds(line);
        Item j;j.type=ItemType::Junction;j.pos=QPointF(60,40);j.size=QSizeF(1,1);j.autoSize=false;assignIds(j);
        Item t;t.type=ItemType::Text;t.text=QStringLiteral("Schräg");t.pos=QPointF(-5,80);t.rotation=30;t.mirrored=true;assignIds(t);
        d.sheets[0].items<<line<<j<<t;
        Editor x;x.resize(1400,900);x.setDocument(d);auto *panel=x.findChild<QWidget*>("properties");
        auto get=[&](const QString &id){for(const auto &i:x.document().sheet().items)if(i.id==id)return i;return Item();};
        auto fresh=[]{QApplication::processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);};
        x.view()->setSelection({j.id});fresh();
        require(!panel->findChild<QComboBox*>("junctionStep")->isEnabled()&&panel->findChild<QDoubleSpinBox*>("junctionSize")->isEnabled(),"without Automatik the size is typed");
        panel->findChild<QComboBox*>("junctionStep")->setEnabled(true);panel->findChild<QComboBox*>("junctionStep")->activated(4);fresh();
        require(get(j.id).autoSize&&get(j.id).sizeStep==4,"a step chosen switches Automatik on");
        require(!panel->findChild<QDoubleSpinBox*>("junctionSize")->isEnabled()&&std::abs(panel->findChild<QDoubleSpinBox*>("junctionSize")->value()-.5*7*10)<1e-6,"the size shown as drawn (XL: 7 times 0.5 mm)");
        panel->findChild<QPushButton*>("junctionReset")->click();
        require(!get(j.id).autoSize&&get(j.id).sizeStep==3&&std::abs(get(j.id).size.width()-1.2)<1e-9&&get(j.id).pen.color==QColor(0,0,0),"Reset: 1.2 mm, black, step L, Automatik off");
        x.view()->setSelection({t.id});fresh();
        panel->findChild<QCheckBox*>("reversed")->setChecked(true);require(get(t.id).reversed,"the writing direction reversed");
        fresh();panel->findChild<QPushButton*>("normalise")->click();
        {const Item n=get(t.id);require(n.rotation==0&&!n.mirrored&&bounds(n).left()>=-1e-9&&n.reversed,"upright again and on the sheet, the direction kept");}
        x.markSaved();}

    // --- The list of kinds of a mixed selection: groups taken apart, the kinds chosen are shown and changed
    {Document d=exampleDocument();d.sheets[0].items.clear();
        Item line;line.type=ItemType::Line;line.points={QPointF(10,10),QPointF(50,10)};assignIds(line);
        Item rect;rect.type=ItemType::Rectangle;rect.centre=QPointF(80,40);rect.size=QSizeF(10,10);assignIds(rect);
        Item t1;t1.type=ItemType::Text;t1.text=QStringLiteral("A");t1.pos=QPointF(20,60);Item t2=t1;t2.text=QStringLiteral("B");t2.pos=QPointF(40,60);
        Item g;g.type=ItemType::Group;g.children={t1,t2};assignIds(g);d.sheets[0].items<<line<<rect<<g;
        Editor x;x.resize(1400,900);x.setDocument(d);auto *panel=x.findChild<QWidget*>("properties");
        auto fresh=[]{QApplication::processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);};
        x.view()->setSelection({line.id,rect.id,g.id});fresh();
        auto *list=panel->findChild<QTreeWidget*>("typeList");require(list&&list->topLevelItemCount()==4,"all, texts, line, rectangle");
        require(list->topLevelItem(0)->text(0)=="4"&&list->topLevelItem(1)->text(0)=="2"&&list->topLevelItem(1)->text(1)==ui("Texte")&&list->topLevelItem(0)->isSelected(),"the counts, all chosen at first");
        require(!panel->findChild<QDoubleSpinBox*>("textHeight"),"mixed: no font");
        list->topLevelItem(0)->setSelected(false);list->topLevelItem(1)->setSelected(true);fresh();
        auto *height=panel->findChild<QDoubleSpinBox*>("textHeight");require(height&&panel->findChild<QTreeWidget*>("typeList"),"the texts chosen: their font, the list stays");
        height->setValue(55);
        {const Item &gg=x.document().sheet().items[2];require(std::abs(gg.children[0].font.height-5.5)<1e-9&&std::abs(gg.children[1].font.height-5.5)<1e-9&&gg.type==ItemType::Group,"both texts in the group changed, the group stays");}
        x.undo();fresh();x.view()->setSelection({line.id});fresh();require(!panel->findChild<QTreeWidget*>("typeList"),"one kind: no list");
        x.markSaved();}
    // A caption in several languages: the interface's shown, the others kept.
    {Item symbol=exampleSymbol();LibraryPage page;page.name=QStringLiteral("Seite");page.entries.append({QStringLiteral("Widerstand\rResistor\rRésistance"),symbol});
        QTemporaryDir dir;savePage(page,QDir(dir.path()).filePath(QStringLiteral("seite.olschlib")));
        PropertiesPanel panel;SheetView view;panel.setView(&view);QString stored;
        panel.libraryEntryChanged=[&](Item &,QString &caption){stored=caption;};
        panel.showLibraryEntry(symbol,QStringLiteral("Widerstand\rResistor\rRésistance"),true);
        auto *caption=panel.findChild<QLineEdit*>("libraryCaption");require(caption&&caption->text()==localized(QStringLiteral("Widerstand\rResistor\rRésistance")),"the interface's language shown");
        caption->setText(QStringLiteral("Neu"));emit caption->editingFinished();
        const int language=uiLanguage()==u"en"?1:uiLanguage()==u"fr"?2:0;QStringList parts=stored.split(u'\r');
        require(parts.size()==3&&parts[language]=="Neu"&&parts[(language+1)%3]!="Neu","that language changed, the others kept");}

    // --- "Voreinstellungen": a preset in the panel without changing the mode; the presets of component texts
    {Document d=exampleDocument();Item r;r.type=ItemType::Rectangle;r.centre=QPointF(150,150);r.size=QSizeF(20,10);assignIds(r);d.sheets[0].items<<r;
        Editor x;x.resize(1400,900);x.setDocument(d);auto *panel=x.findChild<QWidget*>("properties");
        auto fresh=[]{QApplication::processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);};
        auto *entry=x.findChild<QAction*>("presetDesignator");require(entry&&x.action("presets")->menu()&&x.action("presets")->menu()->actions().size()==9,"nine presets in the menu");
        entry->trigger();fresh();
        {auto *title=panel->findChild<QLabel*>("presetTitle");require(title&&title->text().startsWith(ui("Bauteil - Bezeichner"))&&x.view()->tool()==SheetView::Tool::Select,"shown in the panel, the mode unchanged");
            panel->findChild<QDoubleSpinBox*>("textHeight")->setValue(40);require(std::abs(x.view()->designatorPreset.font.height-4)<1e-9&&!x.document().sheet().items.isEmpty(),"changed as a preset, not in the document");}
        x.view()->setSelection({r.id});fresh();require(!panel->findChild<QLabel*>("presetTitle"),"a selection ends it");
        x.makeComponent();
        const Item &c=x.document().sheet().items.last();Item designator,value;for(const auto &k:c.children){if(k.role==TextRole::Designator)designator=k;if(k.role==TextRole::Value)value=k;}
        require(c.type==ItemType::Component&&std::abs(designator.font.height-4)<1e-9&&std::abs(value.font.height-2.5)<1e-9,"a component made from the selection takes the presets' fonts");
        require(value.pos.y()<-1e-9&&designator.pos.y()<value.pos.y(),"value just above it, designator above the value");
        LetteringDialog dialog(true,false);Font f;f.family=QStringLiteral("Arial");f.height=3.3;f.bold=true;f.color=QColor(0,0,200);dialog.start(1,f);
        require(std::abs(dialog.lettering(1).height-3.3)<1e-9&&dialog.bold[1]->isChecked()&&dialog.lettering(1).color==QColor(0,0,200),"the lettering dialog starts from given fonts");
        x.markSaved();}

    // --- Findings of the review of the night's changes
    {Document d=exampleDocument();Item box;box.type=ItemType::TextBox;box.centre=QPointF(80,50);box.size=QSizeF(20,10);box.text=QStringLiteral("b");assignIds(box);
        d.sheets[0].items<<box;Editor x;x.resize(1400,900);x.setDocument(d);auto *panel=static_cast<PropertiesPanel*>(x.findChild<QWidget*>("properties"));
        auto flush=[]{QApplication::processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);};
        const QString part=x.document().sheet().items[0].id;
        // "…" at a designator while the panel is built anew during the dialog (variables defined there): no stale field.
        x.view()->setSelection({part});flush();
        x.textDialogShown=[&](TextDialog *dialog){x.change([](Document &dd){dd.variables<<Variable{QStringLiteral("V"),QStringLiteral("1")};});flush();
            dialog->edit->setPlainText(QStringLiteral("U9"));QTimer::singleShot(0,dialog,&QDialog::accept);};
        panel->findChild<QToolButton*>("designatorDialog")->click();x.textDialogShown=nullptr;flush();
        require(x.document().sheet().items[0].designator=="U9","the designator taken although the panel was built anew");
        // Strg+Einfg in a text box's field while the panel is built anew: the dialog belongs to the window.
        x.view()->setSelection({box.id});flush();
        {auto *memo=panel->findChild<QPlainTextEdit*>("text");memo->setPlainText(QStringLiteral("getippt"));memo->document()->setModified(true);
            CharacterDialog::shown=[&](CharacterDialog *c){require(c->parent()==memo->window(),"the dialog belongs to the window");
                panel->refresh();flush();QTimer::singleShot(0,c,[c]{c->take(U'Q');});};
            QKeyEvent press(QEvent::KeyPress,Qt::Key_Insert,Qt::ControlModifier);QApplication::sendEvent(memo,&press);CharacterDialog::shown=nullptr;flush();}
        // "…" at a text box takes what was typed first.
        x.view()->setSelection({box.id});flush();
        {auto *memo=panel->findChild<QPlainTextEdit*>("text");memo->setPlainText(QStringLiteral("neu getippt"));QString offered;
            x.textDialogShown=[&](TextDialog *dialog){offered=dialog->text();QTimer::singleShot(0,dialog,&QDialog::reject);};
            panel->findChild<QToolButton*>("textDialog")->click();x.textDialogShown=nullptr;flush();
            require(offered=="neu getippt","the extended text input starts from the typed text");}
        // A sheet's description typed and left by choosing another sheet stays with its own sheet.
        x.view()->setSelection({});flush();
        {auto *description=panel->findChild<QPlainTextEdit*>("sheetDescription");description->setPlainText(QStringLiteral("Erstes"));description->document()->setModified(true);
            x.switchSheet(1);QFocusEvent out(QEvent::FocusOut);QApplication::sendEvent(description,&out);flush();
            require(x.document().sheets[0].description=="Erstes"&&x.document().sheets[1].description!="Erstes","the description stays with its sheet");}
        // Arrow keys with an element of a group chosen alone make no step.
        {Item g;g.type=ItemType::Group;Item a;a.type=ItemType::Line;a.points={QPointF(10,10),QPointF(20,10)};Item b=a;b.points={QPointF(10,20),QPointF(20,20)};g.children={a,b};assignIds(g);
            // A step to redo stays: an empty step would drop it.
            x.switchSheet(0);x.change([&](Document &dd){dd.sheet().items<<g;});x.change([](Document &dd){dd.sheet().name=QStringLiteral("anders");});x.undo();
            x.view()->setSelection({x.document().sheet().items.last().children[0].id});QKeyEvent right(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);QApplication::sendEvent(x.view(),&right);
            require(x.canRedo(),"no empty step");}
        // "+" keeps one submenu.
        {auto *quick=x.findChild<QToolButton*>("quickSheet");emit quick->menu()->aboutToShow();emit quick->menu()->aboutToShow();
            require(quick->menu()->actions().size()==2&&quick->menu()->findChildren<QMenu*>().size()==1,"one list of copies however often it opens");}
        // Pictures too large are refused; elements far off the sheet.
        {Item far;far.type=ItemType::Line;far.points={QPointF(10000,10000),QPointF(10010,10000)};assignIds(far);x.change([&](Document &dd){dd.sheet().items<<far;});
            QTemporaryDir dir;QString error;require(x.clipboardImage(300,false).isNull(),"the clipboard refuses a picture of several metres");
            require(!x.exportImage(dir.filePath("gross.png"),300,false,false,false,&error,ExportArea::Elements)&&!error.isEmpty(),"so does the export");
            require(!x.exportImage(dir.filePath("blatt.png"),2400,false,false,false,&error)||QImage(dir.filePath("blatt.png")).width()<=16384,"and a sheet at 2400 dpi beyond the limit");
            const QByteArray emf=emfDrawing(QSizeF(5e5,10),[](QPainter &){});require(!emf.isEmpty(),"an EMF of half a kilometre without overflow");}
        // The problem dialog deletes also in the title block mode.
        {x.action("editTitleBlock")->trigger();const int before=int(x.document().sheet().items.size());
            x.problemShown=[&](ProblemDialog *dialog){QTimer::singleShot(0,dialog->deleteButton,&QPushButton::click);};x.problemDialog();x.problemShown=nullptr;
            require(x.document().sheet().items.size()==before-1,"the element outside deleted in the title block mode");x.action("editTitleBlock")->trigger();}
        // .blt keeps the sheet's sPlan bytes, the own format not.
        {Document fromSplan=splan::read(splan::write(exampleDocument(),80));Editor y;y.setDocument(fromSplan);
            require(!y.sheetDocument(true).sheets[0].splan.isEmpty()&&y.sheetDocument().sheets[0].splan.isEmpty(),"the sheet's bytes for an sPlan sheet file only");y.markSaved();}
        // The print preview keeps the scale within 10 to 800 %.
        {QList<PrintSettings> settings;PrintSettings tiny;tiny.free=true;tiny.scale=.05;settings<<tiny;PrintPreview preview(x.document(),settings,QString());
            require(std::abs(settings[0].scale-.1)<1e-9,"a stored 5 % shown and printed as 10 %");
            preview.someSheets->setChecked(true);preview.selection->setText(QStringLiteral("1-2147483647"));require(preview.sheetsToPrint().size()==x.document().sheets.size(),"a huge range only to the last sheet");}
        // Names of styles out of range.
        {Item odd;odd.type=ItemType::Line;odd.points={QPointF(),QPointF(1,0)};odd.pen.style=PenStyle(9);assignIds(odd);
            require(itemToJson(odd)["pen"].toObject()["style"].toString()=="solid","a style out of range written as the first");}
        // The keys of the reference's menus are shortcuts of their actions, so that tooltips and menus show them.
        {const QList<std::pair<const char*,QKeySequence>> keys{{"new",QKeySequence(Qt::CTRL|Qt::Key_N)},{"open",QKeySequence(Qt::CTRL|Qt::Key_O)},{"save",QKeySequence(Qt::CTRL|Qt::Key_S)},
                {"copyImage",QKeySequence(Qt::CTRL|Qt::Key_B)},{"fullScreen",QKeySequence(Qt::CTRL|Qt::Key_F11)},{"print",QKeySequence(Qt::CTRL|Qt::Key_P)},
                {"undo",QKeySequence(Qt::CTRL|Qt::Key_Z)},{"redo",QKeySequence(Qt::CTRL|Qt::Key_Y)},{"cut",QKeySequence(Qt::CTRL|Qt::Key_X)},{"copy",QKeySequence(Qt::CTRL|Qt::Key_C)},
                {"paste",QKeySequence(Qt::CTRL|Qt::Key_V)},{"duplicate",QKeySequence(Qt::CTRL|Qt::Key_D)},{"delete",QKeySequence(Qt::Key_Delete)},{"selectAll",QKeySequence(Qt::CTRL|Qt::Key_A)},
                {"search",QKeySequence(Qt::CTRL|Qt::Key_F)},{"alignGrid",QKeySequence(Qt::CTRL|Qt::ALT|Qt::Key_G)},{"rotate",QKeySequence(Qt::CTRL|Qt::Key_R)},
                {"mirror",QKeySequence(Qt::CTRL|Qt::Key_M)},{"mirrorVertical",QKeySequence(Qt::CTRL|Qt::ALT|Qt::Key_M)},{"group",QKeySequence(Qt::CTRL|Qt::Key_G)},
                {"ungroup",QKeySequence(Qt::CTRL|Qt::Key_U)},{"colourize",QKeySequence(Qt::CTRL|Qt::ALT|Qt::Key_C)},{"helpTopics",QKeySequence(Qt::Key_F1)},
                {"zoomSheet",QKeySequence(Qt::Key_F5)},{"zoomItems",QKeySequence(Qt::Key_F6)},{"zoomSelected",QKeySequence(Qt::Key_F7)},
                {"toolLine",QKeySequence(Qt::Key_L)},{"toolJunction",QKeySequence(Qt::Key_P)},{"toolRectangle",QKeySequence(Qt::Key_R)},{"toolEllipse",QKeySequence(Qt::Key_K)},
                {"toolPolygon",QKeySequence(Qt::Key_O)},{"toolText",QKeySequence(Qt::Key_T)},{"toolTextBox",QKeySequence(Qt::Key_E)},{"toolZoom",QKeySequence(Qt::Key_Z)}};
            for(const auto &[name,key]:keys){QAction *a=x.action(QString::fromLatin1(name));
                require(a&&a->shortcuts().contains(key),(std::string("the key of the reference as a shortcut: ")+name).c_str());}}
        // Zoom and touchpad as in every drawing view of the suite.
        {SheetView *v=x.view();const QPointF at(300,250);QPointF under=v->toSheet(at);double s=v->scale();
            auto wheel=[&](QPoint pixels,QPoint angle,Qt::KeyboardModifiers mods,Qt::ScrollPhase phase){
                QWheelEvent e(at,v->mapToGlobal(at),pixels,angle,Qt::NoButton,mods,phase,false);QApplication::sendEvent(v,&e);};
            auto same=[](QPointF a,QPointF b){return std::abs(a.x()-b.x())<1e-6&&std::abs(a.y()-b.y())<1e-6;};
            QNativeGestureEvent pinch(Qt::ZoomNativeGesture,QPointingDevice::primaryPointingDevice(),2,at,at,v->mapToGlobal(at),.1,QPointF());QApplication::sendEvent(v,&pinch);
            require(std::abs(v->scale()-s*1.1)<1e-9&&same(v->toSheet(at),under),"a pinch zooms about the pointer");
            s=v->scale();wheel(QPoint(0,30),QPoint(0,90),Qt::NoModifier,Qt::ScrollUpdate);
            require(v->scale()==s&&same(v->toSheet(at),under-QPointF(0,30)/s),"two fingers pan");
            under=v->toSheet(at);wheel(QPoint(),QPoint(0,120),Qt::NoModifier,Qt::ScrollUpdate);
            require(v->scale()==s&&!same(v->toSheet(at),under),"a touchpad without steps in pixels pans too");
            under=v->toSheet(at);wheel(QPoint(0,60),QPoint(0,120),Qt::ControlModifier,Qt::ScrollUpdate);
            require(std::abs(v->scale()-s*1.2)<1e-9&&same(v->toSheet(at),under),"Ctrl and two fingers zoom about the pointer, by sPlan's step");
            s=v->scale();wheel(QPoint(),QPoint(0,120),Qt::ControlModifier,Qt::NoScrollPhase);
            require(std::abs(v->scale()-s*1.2)<1e-9&&same(v->toSheet(at),under),"Ctrl and the wheel of a mouse too");
            s=v->scale();wheel(QPoint(),QPoint(0,-120),Qt::NoModifier,Qt::NoScrollPhase);
            require(std::abs(v->scale()-s/1.2)<1e-9&&same(v->toSheet(at),under),"the wheel alone zooms out about the pointer");
            // sPlan's limits and its zoom shown: pixels per tenth of a millimetre.
            for(int k=0;k<60;k++)v->zoomAt(2,at);
            require(v->scale()==SheetView::maxScale&&std::abs(v->zoom()-30)<1e-9,"at most sPlan's zoom 30");
            for(int k=0;k<60;k++)v->zoomAt(.5,at);
            require(v->scale()==SheetView::minScale&&std::abs(v->zoom()-.05)<1e-9,"at least sPlan's zoom 0.05");
            v->zoomAt(7.6,at);x.action("zoomIn")->trigger();require(std::abs(v->zoom()-.05*7.6*1.2)<1e-9,"Vergrößern by sPlan's step");
            QCoreApplication::processEvents();require(x.findChild<QLabel*>("zoomLabel")->text().contains(uiLocale().toString(v->zoom(),'f',2)),"the zoom shown as in sPlan");}
        x.markSaved();}
    return 0;
}
