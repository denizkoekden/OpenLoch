// The suite: kinds of document, which editor opens which file, the windows of the modules with their Fenster menu,
// recently used documents, the start screen, Beenden and drawing documents without a window.
#include "fixtures.h"
#include "language.h"
#include "project.h"
#include "window.h"
#include "canvas.h"
#include "openlibrary.h"
#include "targetcheck.h"
#include "suite/documents.h"
#include "suite/pcbwindow.h"
#include "suite/startscreen.h"
#include "suite/suite.h"
#include "suite/schematictargets.h"
#include "documents/projectfile.h"
#include "legacy_reader.h"
#include "fpl.h"
#include "frontpanel.h"
#include "paneleditor.h"
#include "modules/pcb/editor.h"
#include "modules/pcb/example.h"
#include "modules/pcb/model.h"
#include "modules/pcb/netcheck.h"
#include "modules/schematic/editor.h"
#include "modules/schematic/library.h"
#include "modules/schematic/example.h"
#include "modules/schematic/model.h"
#include "formats/splan/splan.h"
#include <QAbstractButton>
#include <QTabWidget>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>
#include <functional>
#include <stdexcept>

using namespace openloch;
using namespace openloch::suite;
static void require(bool ok,const QString &message){if(!ok)throw std::runtime_error(message.toStdString());}
static void write(const QString &path,const QByteArray &bytes){QFile file(path);require(file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size(),"fixture could not be written: "+path);}
// Runs `act` and answers the modal dialog it opens: OK or Cancel.
static void answering(bool accept,const std::function<void()> &act){
    QTimer timer;timer.setInterval(20);
    QObject::connect(&timer,&QTimer::timeout,[accept]{if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){if(accept)dialog->accept();else dialog->reject();}});
    timer.start();act();timer.stop();
}
// Windows closed with WA_DeleteOnClose are deleted later.
static void settle(){for(int i=0;i<3;i++){QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QApplication::processEvents();}}
static QAction *named(QWidget *window,const QString &name){for(auto *a:window->findChildren<QAction*>())if(a->objectName()==name)return a;return nullptr;}

int main(int argc,char **argv){
    QApplication app(argc,argv);app.setApplicationName("OpenLoch");app.setOrganizationName("OpenLoch");
    fixtures::checkOwnModel(); // the own model must write every LM4 file as the program does
    setUiLanguage("de"); // the tests compare German texts; CI runners use English systems
    app.setProperty("openloch.testing",true);QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir settings,tmp;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    app.setProperty("openloch.recoveryDirectory",tmp.filePath("recovery"));app.setProperty("openloch.libraryDirectory",tmp.filePath("Bibliotheken"));
    QSettings().setValue("frontpanel/libraryFolder",tmp.filePath("Frontplattensymbole"));
    try{
        require(settings.isValid()&&tmp.isValid(),"temporary folders");
        { // the kinds in the order of the start screen, each with its editor
            const auto kinds=documentKinds();QStringList ids;for(const auto &k:kinds)ids<<k.id;
            require(ids==QStringList{"schematic","perfboard","pcb","frontpanel"},"kinds or their order changed");
            require(kinds[0].available&&kinds[1].available&&kinds[2].available&&kinds[3].available,"availability of the kinds");
            require(kinds[1].name=="Lochraster"&&kinds[2].name=="Leiterplatte"&&kinds[3].name=="Frontplatte","visible names");
            for(const auto &k:kinds)require(!kindIcon(k.kind).pixmap(64,64).isNull()&&!k.summary.isEmpty(),"symbol or summary missing: "+k.id);
        }
        // A file of every kind, written with the modules' own writers and the LochMaster fixtures.
        const QString perfboardFile=tmp.filePath("Blinker.openloch"),lm4File=tmp.filePath("Lauflicht.LM4"),lochLibrary=tmp.filePath("Bauteile.LIB");
        Project board;board.title="Blinker";board.save(perfboardFile);write(lm4File,fixtures::project());write(lochLibrary,fixtures::library());
        frontpanel::Document panel;panel.panels[0].name="Gehäuse";const QString panelFile=tmp.filePath("Gehäuse.olfp");panel.save(panelFile);
        const QString fplFile=tmp.filePath("Gehäuse.FPL"),panelLibrary=tmp.filePath("Symbole.LIB");
        write(fplFile,frontdesigner::writeFrontDesigner(panel));write(panelLibrary,frontdesigner::writeFrontDesignerLibrary(panel,0));
        pcb::Document circuit;circuit.boards={pcb::newBoard("Platine",80,50)};const QString pcbFile=tmp.filePath("Platine.olpcb");pcb::save(circuit,pcbFile);
        const QString panelBackup=tmp.filePath("Gehäuse.BAK"),pcbBackup=tmp.filePath("Platine.BAK"),lochBackup=tmp.filePath("Lauflicht.BAK"),fplBackup=tmp.filePath("Front.BAK");
        write(panelBackup,panel.encode());write(pcbBackup,QJsonDocument(pcb::toJson(circuit)).toJson());write(lochBackup,fixtures::project());write(fplBackup,frontdesigner::writeFrontDesigner(panel));
        const QString splFile=tmp.filePath("Netzteil.spl8"),splLibrary=tmp.filePath("Symbole-sPlan.LIB");
        splan::save(schematic::newDocument("Blatt 1"),splFile,80);schematic::LibraryPage symbols;symbols.name="Symbole";write(splLibrary,splan::writeLibrary(symbols,{},80));
        { // files go to the editor of their kind; LochMaster, FrontDesigner and sPlan share .LIB and .BAK, there the content decides
            auto is=[](const QString &file,std::optional<Kind> kind){return documentKind(file)==kind;};
            require(is(perfboardFile,Kind::Perfboard)&&is(lm4File,Kind::Perfboard)&&is("x.lmb",Kind::Perfboard)&&is("x.OLD",Kind::Perfboard),"Lochraster files");
            require(is(pcbFile,Kind::Pcb)&&is("x.lay6",Kind::Pcb)&&is("x.LAY",Kind::Pcb),"circuit board files");
            require(is(panelFile,Kind::FrontPanel)&&is(fplFile,Kind::FrontPanel),"front panel files");
            require(is(lochLibrary,Kind::Perfboard)&&is(panelLibrary,Kind::FrontPanel),"a .LIB goes to the program that wrote it");
            require(is(panelBackup,Kind::FrontPanel)&&is(pcbBackup,Kind::Pcb)&&is(lochBackup,Kind::Perfboard)&&is(fplBackup,Kind::FrontPanel),"a .BAK goes to the program that wrote it");
            require(is("x.lmk",std::nullopt)&&is("x.png",std::nullopt)&&is("x.txt",std::nullopt),"files no editor opens");
            require(is(splFile,Kind::Schematic)&&is("x.spl7",Kind::Schematic)&&is(splLibrary,std::nullopt),"sPlan plans go to the schematic, its library pages are no documents");
            const QString filter=documentFilter();
            require(filter.startsWith("Alle Dokumente (")&&filter.contains("*.lay6")&&filter.contains("*.FPL")&&filter.contains("*.LM4")&&filter.contains(";;Leiterplatte ("),"open dialog filter");
            require(documentFilter(Kind::Perfboard).section(";;",0,0).startsWith("Lochraster (*.openloch *.OPENLOCH *.lm4")&&documentFilter(Kind::FrontPanel).section(";;",0,0).contains("*.openloch"),
                    "the projects of the suite in the own kind's entry of the open dialog");
            {QTemporaryDir own;require(Suite::openStartFolder(own.path())==own.path()&&Suite::openStartFolder({})==QDir::homePath()&&Suite::openStartFolder(own.filePath("fehlt"))==QDir::homePath(),
                                       "the open dialog starts in the module's own folder");}
        }
        { // the project file: the modules' data unchanged, older files as one document, whatever is unknown kept
            using namespace openloch::documents;
            ProjectFile project;project.id=newId();project.title="Verstärker";
            const QString fpl=project.addResource(frontdesigner::writeFrontDesigner(panel),"fpl","Gehäuse.FPL");
            const QString unused=project.addResource("weg","png","weg.png"),named=project.addResource("bleibt","png","bleibt.png");
            project.addDocument("perfboard","Blinker",QJsonDocument::fromJson(board.encode()).object());
            const QString front=project.addDocument("frontpanel","Gehäuse",panel.toJson(),QJsonObject{{"format","fpl"},{"file","Gehäuse.FPL"},{"resource",fpl}});
            project.addDocument("pcb","Platine",pcb::toJson(circuit));
            ProjectDocument later;later.id=newId();later.kind="zukunft";later.name="Neu";later.data=QJsonObject{{"format","Zukunft"},{"version",7},{"bild",named}};
            later.rest=QJsonObject{{"neu",QJsonArray{1,"zwei"}}};project.documents.append(later);
            project.rest=QJsonObject{{"spaeter",QJsonObject{{"a",1}}}};project.active=front;
            const QString file=tmp.filePath("Verstärker.openloch");project.save(file);
            const auto back=ProjectFile::load(file);
            require(back.id==project.id&&back.title=="Verstärker"&&back.active==front&&back.documents.size()==4&&!back.converted,"project read back");
            require(Project::decode(QJsonDocument(back.documents[0].data).toJson()).title=="Blinker","perfboard data in the project");
            require(frontpanel::Document::fromJson(back.documents[1].data).panels[0].name=="Gehäuse"&&back.documents[1].origin.value("file")=="Gehäuse.FPL","front panel data in the project");
            require(pcb::fromJson(back.documents[2].data).boards[0].name=="Platine","circuit board data in the project");
            require(back.documents[3].kind=="zukunft"&&back.documents[3].data==later.data&&back.documents[3].rest==later.rest&&back.rest==project.rest,"unknown kinds and fields must come back unchanged");
            require(back.resources.contains(fpl)&&back.resources.contains(named)&&!back.resources.contains(unused),"resources named somewhere stay, the others go");
            // older files and the modules' own files: one document of their kind, which is the whole file
            auto single=[&](const QString &path,const QString &kind){const auto p=ProjectFile::load(path);return p.converted&&p.documents.size()==1&&p.documents[0].kind==kind&&isId(p.id)&&p.active==p.documents[0].id;};
            require(single(perfboardFile,"perfboard")&&single(panelFile,"frontpanel")&&single(pcbFile,"pcb"),"older files must become a project with one document");
            require(ProjectFile::load(perfboardFile).documents[0].data==QJsonDocument::fromJson([&]{QFile f(perfboardFile);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}()).object(),"the perfboard file is the document's data");
            // refused, with a reason
            auto refused=[](const QByteArray &bytes){try{ProjectFile::decode(bytes);}catch(const FormatError &e){return QString::fromUtf8(e.what()).size()>0;}return false;};
            QJsonObject newer=QJsonDocument::fromJson(project.encode()).object();newer["version"]=3;
            QJsonObject empty=QJsonDocument::fromJson(project.encode()).object();empty["documents"]=QJsonArray{};
            QJsonObject twice=QJsonDocument::fromJson(project.encode()).object();auto documents=twice["documents"].toArray();documents.append(documents[0]);twice["documents"]=documents;
            QJsonObject damaged=QJsonDocument::fromJson(project.encode()).object();auto resources=damaged["resources"].toObject();
            resources[fpl]=QJsonObject{{"kind","fpl"},{"name","x"},{"data",QString::fromLatin1(QByteArray("anders").toBase64())}};damaged["resources"]=resources;
            for(const auto &bad:{newer,empty,twice,damaged})require(refused(QJsonDocument(bad).toJson()),"a bad project file was accepted");
            require(refused("{\"format\":\"Fremd\"}")&&refused("kein JSON"),"foreign files must be refused");
        }
        { // the Lochraster window saves through a host like the front panel editor: handler, document data, saved
            Window window(tmp.path());int calls=0;bool asNew=false;
            window.saveHandler=[&](bool as){calls++;asNew=as;return true;};
            Project page;page.title="Projektplatine";window.setDocumentData(QJsonDocument::fromJson(page.encode()).object());
            require(window.path().isEmpty()&&!window.isWindowModified(),"a document from a project has no file of its own");
            auto menuAction=[&](const QString &text){for(auto *a:window.findChildren<QAction*>())if(a->text()==text)return a;throw std::runtime_error("menu action missing");};
            menuAction("&Speichern")->trigger();const bool saved=calls==1&&!asNew;menuAction("Speichern &unter…")->trigger();
            require(saved&&calls==2&&asNew&&window.path().isEmpty(),"Speichern must go to the host, without a file dialog");
            require(Project::decode(QJsonDocument(window.documentData()).toJson()).title=="Projektplatine","document data of the Lochraster window");
            window.markSaved();require(!window.isWindowModified()&&!window.saveBackup(),"no own backups while a host saves");
        }
        { // the circuit board window shows the module's menu table (pcb::editorMenus) and ends Datei with Beenden
            PcbWindow window;const auto table=pcb::editorMenus();const auto bar=window.menuBar()->actions();
            require(bar.size()==int(table.size()),"number of menus of the circuit board window");
            for(int i=0;i<int(table.size());i++){
                const auto &[title,names]=table[i];QStringList wanted,shown;
                for(const auto &name:names)if(!name.isEmpty()&&window.editor()->action(name))wanted<<name;
                for(auto *a:bar[i]->menu()->actions())if(!a->isSeparator()&&a->objectName()!="quit")shown<<a->objectName();
                require(bar[i]->text()==ui(title)&&shown==wanted,"the menu "+title+" differs from pcb::editorMenus(): "+shown.join(',')+" / "+wanted.join(','));
            }
            require(bar[0]->menu()->actions().last()==named(&window,"quit"),"Beenden must end Datei");
        }
        { // opening: the right editor, an open file comes to the front, an untouched new document gives its window
            Suite suite(tmp.path());
            auto *loch=suite.open(perfboardFile);require(dynamic_cast<Window*>(loch)&&loch->isVisible(),"Lochraster file did not open in a Lochraster window");
            QMainWindow *front=nullptr,*fresh=nullptr,*library=nullptr;
            answering(true,[&]{front=suite.open(panelFile);});require(dynamic_cast<frontpanel::PanelEditor*>(front),"front panel file");
            auto *circuitWindow=suite.open(pcbFile);require(dynamic_cast<PcbWindow*>(circuitWindow),"circuit board file");
            require(suite.windows().size()==3&&suite.open(panelFile)==front&&suite.windows().size()==3,"an open file opened a second window");
            require(suite.documentPath(loch)==QFileInfo(perfboardFile).absoluteFilePath()&&suite.documentName(circuitWindow)=="Platine","document path or name");
            fresh=suite.create(Kind::FrontPanel,false);require(fresh&&fresh!=front&&suite.windows().size()==4,"new front panel");
            answering(true,[&]{require(suite.open(fplFile)==fresh,"an untouched new document did not give its window to the file");});
            require(suite.windows().size()==4&&suite.documentName(fresh)=="Gehäuse.FPL","document name after reusing the window");
            answering(true,[&]{library=suite.open(panelLibrary);});require(dynamic_cast<frontpanel::PanelEditor*>(library),"FrontDesigner library page");
            require(dynamic_cast<Window*>(suite.open(lochLibrary)),"LochMaster library page");
            const auto recent=suite.recentDocuments();
            require(recent.size()==6&&QFileInfo(recent.first()).fileName()=="Bauteile.LIB"&&recent.contains(QFileInfo(pcbFile).absoluteFilePath()),"recently used documents: "+recent.join(", "));
            // the Fenster menu: before Hilfe in the Lochraster window, last in the others; it lists every window
            auto *menu=loch->findChild<QMenu*>("suiteMenu");require(menu,"Fenster menu missing");
            const auto bar=loch->menuBar()->actions();const int at=bar.indexOf(menu->menuAction());
            require(at>0&&at+1<bar.size()&&bar[at+1]->text()=="&Hilfe","Fenster belongs before Hilfe");
            // in the module windows before Hilfe where there is one (the front panel), else last
            for(auto *window:{front,circuitWindow}){auto *own=window->findChild<QMenu*>("suiteMenu");const auto items=window->menuBar()->actions();const int i=items.indexOf(own?own->menuAction():nullptr);
                require(own&&i>=0&&(i+1==items.size()||items[i+1]->text()=="&Hilfe"),"Fenster menu in the module windows");}
            require(front->menuBar()->actions().last()->text()=="&Hilfe","the front panel's Hilfe comes last");
            emit menu->aboutToShow();int listed=0;for(auto *a:menu->actions())listed+=a->isCheckable();
            require(listed==int(suite.windows().size()),"window list of the Fenster menu");
            require(named(loch,"suiteNew-schematic")->isEnabled()&&named(loch,"suiteNew-pcb")->isEnabled(),"every kind can be made new");
            {   // a new circuit board asks with the module's dialog as in Sprint-Layout: here a round outline with a margin
                QString title;QTimer::singleShot(0,[&]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget())){title=d->windowTitle();
                    d->findChild<QRadioButton*>("newRound")->setChecked(true);d->findChild<QLineEdit*>("newName")->setText("Gehäuse");d->accept();}});
                auto *made=dynamic_cast<PcbWindow*>(suite.create(Kind::Pcb));
                require(made&&title=="Neue Platine"&&made->editor()->document().boards.size()==1&&made->editor()->document().boards[0].name=="Gehäuse"
                        &&made->editor()->document().boards[0].width==140,"a new circuit board did not come from the module's dialog");
                QTimer::singleShot(0,[&]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget()))d->reject();});
                require(!suite.create(Kind::Pcb),"a cancelled new circuit board made a window");
                made->close();settle();
            }
            {   // with the module's preference "origin of new boards at the top left" the origin is the top left corner of the
                // outline, as the module itself makes it
                QSettings(QStringLiteral("OpenLoch"),QStringLiteral("Leiterplatte")).setValue("originTopLeft",true);
                QTimer::singleShot(0,[&]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget())){d->findChild<QRadioButton*>("newRectangle")->setChecked(true);d->accept();}});
                auto *made=dynamic_cast<PcbWindow*>(suite.create(Kind::Pcb));
                QSettings(QStringLiteral("OpenLoch"),QStringLiteral("Leiterplatte")).remove("originTopLeft");
                require(made&&made->editor()->document().boards.size()==1&&made->editor()->document().boards[0].origin==QPointF(20,20),
                        "a new circuit board's origin must be the top left corner of its outline");
                made->close();settle();
            }
            // saving under another name reaches the list
            // the front panel's own file becomes a project when saved; the project is the recent file then
            require(suite.inProject(front)&&!suite.inProject(fresh)&&suite.inProject(circuitWindow)&&circuitWindow->windowTitle().startsWith("Platine"),"which documents save through a project");
            const QString savedAs=tmp.filePath("Gehäuse-Projekt.openloch");require(suite.saveProject(front,true,savedAs),"save the project");
            require(suite.recentDocuments().first()==QFileInfo(savedAs).absoluteFilePath()&&QFileInfo::exists(panelFile),"saving did not reach the recent documents or removed the old file");
            // Öffnen in a module window goes through the suite: the window's own kind first in the filter, a file of
            // another kind to its editor, a project of the suite as a project (the windows' own readers refuse both)
            {
                auto openIn=[&](QMainWindow *w,const QString &file){
                    // The timers end with this call, so that none of them acts on a later dialog.
                    QString first;bool warned=false;QTimer pick,warn;pick.setSingleShot(true);warn.setSingleShot(true);
                    QObject::connect(&pick,&QTimer::timeout,[&]{if(auto *d=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){first=d->nameFilters().value(0);d->selectFile(file);static_cast<QDialog*>(d)->accept();}});
                    QObject::connect(&warn,&QTimer::timeout,[&]{if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){warned=true;box->accept();}});
                    pick.start(0);warn.start(300);
                    QAction *open=nullptr;
                    for(auto *a:w->findChildren<QAction*>()){bool inDialog=false;for(auto *o=a->parent();o;o=o->parent())inDialog|=qobject_cast<QDialog*>(o)!=nullptr;
                        if(!inDialog&&(a->objectName()=="open"||a->text().remove('&')=="Öffnen…"))open=a;}
                    require(open,"Öffnen missing");open->trigger();settle();QApplication::processEvents();
                    return std::pair<QString,bool>{first,warned};
                };
                const auto count=suite.windows().size();
                const auto [boardFilter,boardWarned]=openIn(loch,pcbFile);
                require(boardFilter.startsWith("Lochraster")&&!boardWarned&&suite.windows().size()==count,"a circuit board file chosen in the perfboard's Öffnen goes to its window");
                const auto [panelFilter,panelWarned]=openIn(front,savedAs);
                require(panelFilter.startsWith("Frontplatte")&&!panelWarned&&suite.windows().size()==count,"a project chosen in the front panel's Öffnen opens as a project");
                const auto [pcbFilter,pcbWarned]=openIn(circuitWindow,perfboardFile);
                require(pcbFilter.startsWith("Leiterplatte")&&!pcbWarned&&suite.windows().size()==count,"a perfboard file chosen in the circuit board's Öffnen goes to its window");
            }
            {   // Fenster → Bibliotheken…: one row per library with the folders the modules give; a further folder that is
                // gone is reported when it should be shown, not made anew
                const QString gone=QDir(tmp.path()).filePath("Weg");const auto plan=schematic::libraryFolders();schematic::setLibraryFolders({plan.own,{gone}});
                QStringList names;bool reported=false;
                QTimer::singleShot(0,[&]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());d&&d->objectName()=="librariesDialog"){
                    auto *tree=d->findChild<QTreeWidget*>("libraryFolders");for(int i=0;i<tree->topLevelItemCount();i++)if(tree->topLevelItem(i)->childCount()>0)names<<tree->topLevelItem(i)->text(0);
                    for(int i=0;i<tree->topLevelItemCount();i++)for(int k=0;k<tree->topLevelItem(i)->childCount();k++)
                        if(tree->topLevelItem(i)->child(k)->text(1)==QDir::toNativeSeparators(gone))tree->setCurrentItem(tree->topLevelItem(i)->child(k));
                    QTimer::singleShot(0,[&]{if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){reported=true;box->accept();}});
                    d->findChild<QPushButton*>("libraryShow")->click();d->reject();}});
                named(loch,"suiteLibraries")->trigger();schematic::setLibraryFolders(plan);
                require(names==QStringList{"Lochraster – Bauteile","Frontplatte – Symbole","Frontplatte – Strichschriften","Schaltplan – Bibliothek","Leiterplatte – Makros"},"the libraries overview: "+names.join(", "));
                require(reported&&!QFileInfo::exists(gone),"a further library folder that is gone must be reported, not made anew");
            }
            // Beenden in one window closes them all and does not bring back the start screen
            named(loch,"quit")->trigger();settle();
            require(suite.windows().isEmpty()&&(!suite.startScreen()||!suite.startScreen()->isVisible()),"Beenden left windows open or showed the start screen");
        }
        { // projects: new documents save as projects, documents join one, an original's file stays as it is
            Suite suite(tmp.path());
            auto *board=dynamic_cast<Window*>(suite.create(Kind::Perfboard,false));
            require(board&&suite.inProject(board)&&suite.documentPath(board).isEmpty()&&board->saveHandler,"a new document is a project without a file yet");
            const QString projectPath=tmp.filePath("Gerät.openloch");
            require(suite.saveProject(board,true,projectPath)&&suite.documentPath(board)==QFileInfo(projectPath).absoluteFilePath(),"saving a new project");
            auto *panelWindow=dynamic_cast<frontpanel::PanelEditor*>(suite.addToProject(board,Kind::FrontPanel,false));
            require(panelWindow&&suite.inProject(panelWindow)&&suite.projectFile(board)==suite.projectFile(panelWindow),"a front panel joins the project");
            require(documents::ProjectFile::load(projectPath).documents.size()==2,"adding a document saves the project");
            // a document of a project goes to LochMaster's format through Exportieren
            QTimer::singleShot(0,[&]{if(auto *dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){dialog->setDirectory(tmp.path());dialog->selectFile("Gerät-Lochraster");static_cast<QDialog*>(dialog)->accept();}});
            named(board,"exportLm4")->trigger();
            require(Project::load(tmp.filePath("Gerät-Lochraster.LM4")).sourceKind=="lm4"&&suite.documentPath(board)==QFileInfo(projectPath).absoluteFilePath(),"LM4 export of a project document");
            // a schematic joins as well; a schematic file of its own opens as a project of one document
            auto *plan=dynamic_cast<schematic::Editor*>(suite.addToProject(board,Kind::Schematic,false));
            require(plan&&suite.inProject(plan)&&documents::ProjectFile::load(projectPath).documents.size()==3,"a schematic joins the project");
            const QString planFile=tmp.filePath("Plan.olsch");write(planFile,schematic::encode(schematic::newDocument("Blatt 1")));
            auto *planWindow=suite.open(planFile);
            require(dynamic_cast<schematic::Editor*>(planWindow)&&suite.inProject(planWindow)&&suite.projectFile(planWindow)->documents.size()==1&&suite.projectFile(planWindow)->documents[0].kind=="schematic","a schematic file opens as a project");
            QString planError;require(renderDocument(planFile,tmp.filePath("plan.png"),&planError)&&!QImage(tmp.filePath("plan.png")).isNull(),"drawing a schematic: "+planError);
            planWindow->close();settle();
            auto *menu=board->findChild<QMenu*>("suiteMenu");emit menu->aboutToShow();
            require(named(board,"suiteAdd-frontpanel")->isEnabled()&&named(board,"suiteAdd-pcb")->isEnabled(),"every kind can join a project");
            auto *layout=dynamic_cast<PcbWindow*>(suite.addToProject(board,Kind::Pcb,false));
            require(layout&&suite.inProject(layout)&&layout->editor()->saveHandler&&documents::ProjectFile::load(projectPath).documents.size()==4,"a circuit board joins the project");
            // a change and Speichern in one window save the whole project and both windows
            panelWindow->change([](frontpanel::Document &d){d.panel().name="Front";});require(panelWindow->modified(),"a change in the front panel");
            named(panelWindow,"save")->trigger();
            const auto stored=documents::ProjectFile::load(projectPath);
            require(!panelWindow->modified()&&!board->isWindowModified()&&frontpanel::Document::fromJson(stored.documents[1].data).panels[0].name=="Front","Speichern in one window saves the project");
            require(Project::decode(QJsonDocument(stored.documents[0].data).toJson()).width==board->documentData()["width"].toDouble(),"the other document is saved as well");
            // a closed document stays in the project and opens again from it
            const QString panelDocument=stored.documents[1].id;panelWindow->close();settle();
            require(suite.windows().size()==3&&suite.projectFile(board)->documents.size()==4,"closing one document keeps the project");
            auto *again=dynamic_cast<frontpanel::PanelEditor*>(suite.openDocument(board,panelDocument));
            require(again&&again->document().panel().name=="Front"&&suite.inProject(again),"a closed document opens again from its project");
            for(auto *w:suite.windows())w->close();settle();
            auto *reopened=suite.open(projectPath);
            require(reopened&&suite.inProject(reopened)&&suite.projectFile(reopened)->documents.size()==4&&suite.open(projectPath)==reopened,"the project file opens again, once");
            for(auto *w:suite.windows())w->close();settle();
            // a single LM4: its module saves the LM4; joining a project leaves the LM4 as it is
            auto bytes=[](const QString &path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();};
            const QByteArray lm4=bytes(lm4File);
            auto *lauflicht=dynamic_cast<Window*>(suite.open(lm4File));
            require(lauflicht&&!suite.inProject(lauflicht)&&!lauflicht->saveHandler&&suite.documentPath(lauflicht)==QFileInfo(lm4File).absoluteFilePath(),"an original's file is saved by its module");
            const QString joined=tmp.filePath("Lauflicht-Projekt.openloch");
            require(suite.addToProject(lauflicht,Kind::FrontPanel,false,joined)&&suite.inProject(lauflicht)&&lauflicht->saveHandler,"the LM4 document joins a project");
            const auto withOrigin=documents::ProjectFile::load(joined);
            require(withOrigin.documents.size()==2&&withOrigin.documents[0].origin.value("format")=="lm4"&&withOrigin.documents[0].origin.value("file")=="Lauflicht.LM4","the original's file is named as origin");
            for(auto *a:lauflicht->findChildren<QAction*>())if(a->text()=="&Speichern")a->trigger();
            require(bytes(lm4File)==lm4&&documents::ProjectFile::load(joined).documents.size()==2,"Speichern writes the project, the LM4 stays as it was");
            for(auto *w:suite.windows())w->close();settle();
            QString error;require(renderDocument(projectPath,tmp.filePath("projekt.png"),&error)&&!QImage(tmp.filePath("projekt.png")).isNull(),"drawing a project: "+error);
        }
        { // a sPlan file opens in the schematic editor, which saves it back; sPlan's library pages are no documents
            Suite suite(tmp.path());QMainWindow *plan=nullptr;
            answering(true,[&]{plan=suite.open(splFile);});
            require(dynamic_cast<schematic::Editor*>(plan)&&!suite.inProject(plan)&&suite.documentPath(plan)==QFileInfo(splFile).absoluteFilePath()&&suite.open(splFile)==plan,"a sPlan file opens once, saved by the schematic editor");
            answering(true,[&]{require(!suite.open(splLibrary),"a sPlan library page is no document");});
            QString error;require(renderDocument(splFile,tmp.filePath("splan.png"),&error)&&!QImage(tmp.filePath("splan.png")).isNull(),"drawing a sPlan file: "+error);
            for(auto *w:suite.windows())w->close();settle();
        }
        { // the start screen: tiles and recent documents; a new document hides it, closing the last brings it back
            Suite suite(tmp.path());suite.showStartScreen();auto *start=suite.startScreen();require(start&&start->isVisible(),"start screen");
            auto *schematic=start->findChild<QAbstractButton*>("new-schematic");require(schematic&&schematic->isEnabled(),"schematic tile");
            auto *recent=start->findChild<QTreeWidget*>("recentDocuments");
            require(recent&&recent->topLevelItemCount()==int(suite.recentDocuments().size())&&recent->topLevelItemCount()>0,"recent documents on the start screen");
            answering(true,[&]{start->findChild<QAbstractButton*>("new-pcb")->click();});
            auto *made=dynamic_cast<PcbWindow*>(suite.windows().value(0));require(made&&!start->isVisible(),"the circuit board tile");
            require(made->editor()->document().board().width==160&&made->editor()->document().board().height==100,"size of a new circuit board");
            answering(false,[&]{suite.create(Kind::Perfboard);});require(suite.windows().size()==1,"a cancelled new document left a window");
            answering(true,[&]{suite.create(Kind::Perfboard);});require(suite.windows().size()==2&&dynamic_cast<Window*>(suite.windows().last()),"new Lochraster");
            answering(true,[&]{suite.create(Kind::FrontPanel);});require(suite.windows().size()==3&&dynamic_cast<frontpanel::PanelEditor*>(suite.windows().last()),"new front panel");
            auto *plan=dynamic_cast<schematic::Editor*>(suite.create(Kind::Schematic));
            require(plan&&suite.inProject(plan)&&suite.windows().size()==4,"a new schematic is a project");
            for(auto *window:suite.windows())window->close();settle();
            require(suite.windows().isEmpty()&&start->isVisible(),"closing the last document did not bring back the start screen");
            suite.forget(QFileInfo(pcbFile).absoluteFilePath());require(!suite.recentDocuments().contains(QFileInfo(pcbFile).absoluteFilePath()),"forget");
            suite.clearRecent();require(suite.recentDocuments().isEmpty()&&start->findChild<QTreeWidget*>("recentDocuments")->topLevelItemCount()==0,"clear the list");
        }
        { // backups of unsaved Lochraster projects left over from an earlier run are offered once at the start
            Suite suite(tmp.path());suite.showStartScreen();
            Project lost;lost.title="Verloren";QDir().mkpath(Window::recoveryFolder());const QString left=Window::recoveryFolder()+"/left.openloch";write(left,lost.encode());
            answering(false,[&]{suite.offerRecovery();});settle();
            require(suite.windows().isEmpty()&&suite.startScreen()->isVisible(),"declining the backups must lead back to the start screen");
            QFile::remove(left);suite.offerRecovery();require(suite.windows().isEmpty(),"no backups, no window");
        }
        { // a board in a project with a schematic compares itself with it and takes over Kennung, value and link
            using namespace openloch::documents;
            const auto plan=schematic::exampleDocument();const auto targets=schematicTargets(schematic::toJson(plan));
            require(targets.components.size()==2&&targets.components[0].designator=="U1"&&targets.components[1].designator=="U2"&&targets.components[0].pins==QStringList{"1","2","3"},"the components of a schematic as targets");
            bool wired=false;for(const auto &net:targets.nets)wired=wired||(net.pins.size()==2&&net.pins.contains(TargetPin{targets.components[0].id,"2"})&&net.pins.contains(TargetPin{targets.components[1].id,"1"}));
            require(wired,"the conductor from U1 to U2 must be a net of the targets");
            Project parts;parts.width=5080;parts.height=2540;
            parts.additions.append(QJsonObject{{"type","resistor"},{"x",762},{"y",508},{"text","U1"}});parts.additions.append(QJsonObject{{"type","resistor"},{"x",2286},{"y",508},{"text","U9"}});
            ProjectFile project;project.id=newId();project.title="Vergleich";project.addDocument("perfboard","Platine",QJsonDocument::fromJson(parts.encode()).object());
            const QString planId=project.addDocument("schematic","Schaltplan",schematic::toJson(plan));const QString file=tmp.filePath("Vergleich.openloch");project.save(file);
            Suite suite(tmp.path());auto *board=dynamic_cast<Window*>(suite.open(file));
            require(board&&board->hasSchematic&&board->hasSchematic()&&board->targets().components.size()==2&&named(board,"compareSchematic")->isEnabled(),"a board in a project with a schematic must see its targets");
            named(board,"compareSchematic")->trigger();auto *results=board->findChild<QListWidget*>("schematicResults");QStringList lines;for(int i=0;results&&i<results->count();i++)lines<<results->item(i)->text();
            require(lines.join("\n").contains("U2")&&lines.join("\n").contains("U9")&&lines.join("\n").contains("U1"),"the comparison lists the missing, extra and unassigned parts: "+lines.join(" | "));
            named(board,"showAirwires")->setChecked(true);named(board,"showAirwires")->setChecked(false);
            answering(true,[&]{named(board,"takeOverSchematic")->trigger();});
            auto linked=Project::decode(QJsonDocument(board->documentData()).toJson()).components();
            require(linked.size()==2&&linked[0].id==targets.components[0].id&&linked[1].id!=targets.components[1].id,"taking over links the part found by its Kennung, only that one");
            // An open schematic counts with its current state.
            auto *editor=dynamic_cast<schematic::Editor*>(suite.openDocument(board,planId));require(editor,"the schematic of the project");
            editor->change([](schematic::Document &d){for(auto *c:schematic::components(d.sheets[0]))if(c->designator=="U2")c->designator="U9";});
            require(board->targets().components.size()==2&&board->targets().components[1].designator=="U9","the open schematic's current state must count");
            require(suite.saveProject(board,false,file),"saving the project");for(auto *window:suite.windows())window->close();settle();
        }
        { // a parent and its children are one part of the targets; with sheet numbers the kind stays that of the part
            using namespace openloch::documents;
            const auto pages=schematic::builtInPages();const schematic::LibraryEntry *parent=nullptr,*gate1=nullptr,*gate2=nullptr,*resistor=nullptr;
            for(const auto &p:pages)for(const auto &e:p.entries){const QString c=e.caption.section(u'\r',0,0);
                if(c==u"7400 Versorgung (Parent)")parent=&e;else if(c==u"7400 NAND 1 (Child)")gate1=&e;else if(c==u"7400 NAND 2 (Child)")gate2=&e;
                else if(c==u"Widerstand"&&p.name.section(u'\r',0,0)==u"Widerstände")resistor=&e;}
            require(parent&&gate1&&gate2&&resistor,"the 7400 and a resistor in the library");
            auto plan=schematic::newDocument(QStringLiteral("x"));plan.designatorPageNumbers=true;
            auto ic=schematic::placedSymbol(*parent,plan);ic.pos={40,40};plan.sheets[0].items<<ic;
            auto a=schematic::placedSymbol(*gate1,plan);a.parentId=ic.id;a.pos={80,40};plan.sheets[0].items<<a;
            auto b=schematic::placedSymbol(*gate2,plan);b.parentId=ic.id;b.pos={120,40};plan.sheets[0].items<<b;
            auto r=schematic::placedSymbol(*resistor,plan);r.pos={80,90};plan.sheets[0].items<<r;
            auto at=[](const schematic::Item &k,const QString &name){for(const auto *c:schematic::contacts(k))if(c->name==name)return schematic::placement(k).map(c->pin);return QPointF();};
            schematic::Item wire;wire.id=newId();wire.type=schematic::ItemType::Line;wire.points={at(a,"3"),at(r,"1")};plan.sheets[0].items<<wire;   // gate 1's output to R
            const auto t=schematicTargets(schematic::toJson(plan));
            require(t.components.size()==2&&t.components[0].id==ic.id&&t.components[1].id==r.id,"the IC with its gates and the resistor are two parts");
            const auto &part=t.components[0];
            require(part.designator==u'1'+ic.designator&&QSet<QString>(part.pins.begin(),part.pins.end())==QSet<QString>{"7","14","1","2","3","4","5","6"}&&part.kind=="IC","the gates' pins belong to the IC: "+part.designator+" "+part.pins.join(','));
            require(t.components[1].designator==u'1'+r.designator&&t.components[1].kind=="R","a resistor shown as 1R… is still of kind R");
            bool wired=false;for(const auto &net:t.nets)wired=wired||(net.pins.contains(TargetPin{ic.id,"3"})&&net.pins.contains(TargetPin{r.id,"1"}));
            require(wired,"a net at a gate's pin is a net at its IC");
            require(Targets::fromJson(t.toJson()).components[0].kind=="IC"&&Targets::fromJson(t.toJson()).components[1].kind=="R","the kind survives the JSON form");
        }
        { // "Anschlüsse zuordnen" confirmed as offered keeps the names of the part's pins: + and - of an electrolytic stay
            using namespace openloch::documents;
            auto plan=schematic::exampleDocument();auto &items=plan.sheets[0].items;items.removeAt(2);items.removeAt(1);
            auto &capacitor=items[0];capacitor.designator="C1";capacitor.value="100µ";
            capacitor.children.removeIf([](const schematic::Item &i){return i.type==schematic::ItemType::Contact&&i.name=="3";});
            QFile page(QString(OPENLOCH_SOURCE_DIR)+"/libraries/11-kondensatoren-elko.json");require(page.open(QIODevice::ReadOnly),"the page of electrolytics");
            Project parts;parts.width=5080;parts.height=2540;const auto key=parts.addLibrary(openLibraryPage(QJsonDocument::fromJson(page.readAll()).object()),"LIB11.LIB","lib");
            parts.additions.append(QJsonObject{{"type","component"},{"library",key},{"index",0},{"x",2540},{"y",1270},{"id","C#"},{"group_value",1}});parts.assignIds();
            require(parts.components().size()==1&&parts.components()[0].pins==QStringList{"+","-"},"an electrolytic names its pins + and -");
            ProjectFile project;project.id=newId();project.title="Elko";project.addDocument("perfboard","Platine",QJsonDocument::fromJson(parts.encode()).object());
            project.addDocument("schematic","Schaltplan",schematic::toJson(plan));const QString file=tmp.filePath("Elko.openloch");project.save(file);
            Suite suite(tmp.path());auto *board=dynamic_cast<Window*>(suite.open(file));require(board&&named(board,"assignPins")->isEnabled(),"the board of the project");
            board->canvas->selectObjects({{"new",0}});QString seen;
            QTimer::singleShot(0,[&]{if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){seen=dialog->objectName()+"|"+dialog->windowTitle();dialog->accept();}});named(board,"assignPins")->trigger();
            const auto now=Project::decode(QJsonDocument(board->documentData()).toJson()).components();const auto wanted=board->targets().components.value(0);
            require(seen=="assignPinsDialog|Anschlüsse zuordnen – C1","the dialog for the part marked: "+seen);
            require(now.size()==1&&now[0].pins==QStringList{"+","-"}&&now[0].id==wanted.id,"the pins keep their names, the part is linked: "+now.value(0).pins.join(","));
            require(!interchangeable(now[0],wanted),"an electrolytic is never interchangeable");
            require(suite.saveProject(board,false,file),"saving the project");for(auto *window:suite.windows())window->close();settle();
        }
        { // a front panel learns the boards of its project: perfboard and circuit board, with the middle of every part
            using namespace openloch::documents;
            auto pageOf=[](const QString &name){QFile f(QString(OPENLOCH_SOURCE_DIR)+"/libraries/"+name);require(f.open(QIODevice::ReadOnly),"a library page is missing");return openLibraryPage(QJsonDocument::fromJson(f.readAll()).object());};
            Project board;board.width=5080;board.height=3810;board.title="Hauptplatine";
            const auto leds=board.addLibrary(pageOf("06-leuchtdioden.json"),"LIB6.LIB","lib"),pots=board.addLibrary(pageOf("25-potentiometer.json"),"LIB25.LIB","lib");
            board.additions.append(QJsonObject{{"type","component"},{"library",leds},{"index",1},{"x",1270},{"y",1270},{"id","D#"},{"group_value",1}});
            board.additions.append(QJsonObject{{"type","component"},{"library",pots},{"index",2},{"x",3048},{"y",2032},{"id","R#"},{"group_value",2}});
            board.additions.append(QJsonObject{{"type","resistor"},{"x",1524},{"y",3048},{"text","R1"}});board.assignIds();
            const auto parts=board.components();require(parts.size()==3,"three parts on the perfboard");
            // Stored without identifiers, as an older document: it gets them once and keeps them.
            auto stored=QJsonDocument::fromJson(board.encode()).object();stored.remove("boardId");stored.remove("uids");
            for(int i=0;i<2;i++){auto list=stored["additions"].toArray();auto o=list[i].toObject();o.remove("uid");list[i]=o;stored["additions"]=list;}
            ProjectFile project;project.id=newId();project.title="Gehäuse";
            const QString perfboardId=project.addDocument("perfboard","Platine",stored),pcbId=project.addDocument("pcb","Leiterplatte",pcb::toJson(pcb::exampleDocument()));
            frontpanel::Document front;const QString panelId=project.addDocument("frontpanel","Frontplatte",front.toJson());project.active=panelId;
            const QString file=tmp.filePath("Gehäuse.openloch");project.save(file);
            Suite suite(tmp.path());auto *panel=dynamic_cast<frontpanel::PanelEditor*>(suite.open(file));require(panel&&panel->boardSources,"the front panel of the project");
            const auto sources=panel->boardSources(),again=panel->boardSources();
            require(sources.size()==2&&sources[0].document==perfboardId&&sources[1].document==pcbId&&sources[0].board.size()==32&&sources[0].name=="Platine","the boards of the project");
            require(again[0].board==sources[0].board&&again[0].parts.size()==3&&again[0].parts[0].component==sources[0].parts[0].component&&sources[0].parts[0].component.size()==32,"an older document keeps the identifiers it got");
            require(sources[0].size==QSizeF(50.8,38.1),"the perfboard in millimetres");
            const auto led=sources[0].parts[0],pot=sources[0].parts[1],resistor=sources[0].parts[2];
            auto pinsOf=[&](int i){QList<QPointF> mm;for(auto q:pinPositions(board,parts[i]))mm<<q/100;return mm;};
            auto middle=[](const QList<QPointF> &points){QPointF m;for(auto q:points)m+=q/double(points.size());return m;};
            const auto ledPins=pinsOf(0),potPins=pinsOf(1);
            require(led.designator=="D1"&&led.top&&led.name.startsWith("LED ")&&QLineF(led.centre,middle(ledPins)).length()<0.6,"a standing LED sits between its leads and is named");
            const QLineF row(potPins.first(),potPins.last());const QPointF towards=pot.centre-row.p1();const double off=std::abs(towards.x()*row.dy()-towards.y()*row.dx())/row.length();
            require(pot.designator=="R2"&&off>1&&pot.bounds.contains(pot.centre),"a potentiometer's middle lies on its body, not on the row of its pins");
            require(resistor.designator=="R1"&&QLineF(resistor.centre,middle(pinsOf(2))).length()<1e-9,"an own resistor's middle is that of its pins");
            const auto example=pcb::exampleDocument().boards[0];const auto components=pcb::components(example);
            require(sources[1].size==QSizeF(example.width,example.height)&&sources[1].parts.size()==components.size()&&!components.isEmpty(),"the circuit board and its components");
            require(sources[1].parts[0].centre==pcb::pickPlaceCentre(example,components[0])&&sources[1].parts[0].top==pcb::componentOnTop(example,components[0]),"a component of the circuit board at its pick and place centre");
            require(suite.saveProject(panel,false,file),"saving the project");
            const auto saved=ProjectFile::load(file);const auto reread=Project::decode(QJsonDocument(saved.documents[saved.indexOf(perfboardId)].data).toJson());
            require(reread.boardId==sources[0].board,"the identifiers an older document got are saved with the project");
            for(auto *window:suite.windows())window->close();settle();
        }
        { // missing parts of the schematic set beside the board from the OpenLoch library, only those chosen
            qApp->setProperty("openloch.openLibrarySource",QString(OPENLOCH_SOURCE_DIR)+"/libraries");qApp->setProperty("openloch.openLibraryDirectory",tmp.filePath("open"));
            using namespace openloch::documents;
            ProjectFile project;project.id=newId();project.title="Setzen";Project empty;empty.width=5080;empty.height=2540;
            project.addDocument("perfboard","Platine",QJsonDocument::fromJson(empty.encode()).object());project.addDocument("schematic","Schaltplan",schematic::toJson(schematic::exampleDocument()));
            const QString file=tmp.filePath("Setzen.openloch");project.save(file);
            Suite suite(tmp.path());auto *board=dynamic_cast<Window*>(suite.open(file));require(board,"the board of the project");
            QString offered;int preselected=-1;
            QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
                if(auto *box=dialog->findChild<QComboBox*>("part-U2")){offered=box->itemText(1);preselected=box->currentIndex();box->setCurrentIndex(1);}
                if(auto *box=dialog->findChild<QComboBox*>("part-U1"))box->setCurrentIndex(0);dialog->accept();});
            named(board,"placeMissing")->trigger();
            QString wanted;for(const auto &c:board->targets().components)if(c.designator=="U2")wanted=c.id;
            const auto placed=Project::decode(QJsonDocument(board->documentData()).toJson()).components();
            require(!offered.isEmpty()&&preselected==1&&placed.size()==1&&placed[0].designator=="U2"&&placed[0].id==wanted,"a chosen part set beside the board, linked to its component: "+offered);
            require(suite.saveProject(board,false,file),"saving the project");for(auto *window:suite.windows())window->close();settle();
            qApp->setProperty("openloch.openLibrarySource",QVariant());qApp->setProperty("openloch.openLibraryDirectory",QVariant());
        }
        { // a circuit board in a project with a schematic gets its targets as the perfboard does
            using namespace openloch::documents;
            ProjectFile project;project.id=newId();project.title="Leiterplatte";project.addDocument("pcb","Platine",pcb::toJson(circuit));
            project.addDocument("schematic","Schaltplan",schematic::toJson(schematic::exampleDocument()));const QString file=tmp.filePath("Leiterplatte.openloch");project.save(file);
            Suite suite(tmp.path());auto *window=dynamic_cast<PcbWindow*>(suite.open(file));require(window,"the circuit board of the project");auto *editor=window->editor();
            require(editor->hasSchematic&&editor->hasSchematic()&&editor->targets().components.size()==2,"a circuit board in a project with a schematic must see its targets");
            QMenu *menu=nullptr;for(auto *a:window->menuBar()->actions())if(a->menu()&&a->menu()->actions().contains(editor->action("compareSchematic")))menu=a->menu();
            require(menu,"the comparison must be in a menu of the circuit board");emit menu->aboutToShow();
            require(editor->action("compareSchematic")->isEnabled()&&editor->action("placeMissing")->isEnabled(),"with a schematic the comparison must be enabled");
            for(auto *w:suite.windows())w->close();settle();
        }
        { // the project overview: the parts across all documents, the parts list, renaming and removing documents
            using namespace openloch::documents;
            const auto plan=schematic::exampleDocument();const auto targets=schematicTargets(schematic::toJson(plan));const QString u1=targets.components[0].id;
            Project board;board.width=5080;board.height=2540;board.title="Platine";
            board.additions.append(QJsonObject{{"type","resistor"},{"x",762},{"y",508},{"text","U1"},{"component",u1}});
            board.additions.append(QJsonObject{{"type","resistor"},{"x",2286},{"y",508},{"text","R9"},{"value","1k"}});board.assignIds();
            frontpanel::Document panel;panel.title="Front";frontpanel::Element hole=frontpanel::newElement(frontpanel::ElementType::Drill);hole.center={20,20};hole.diameter=5;hole.component=u1;hole.name="U1";panel.panels[0].elements<<hole;
            ProjectFile project;project.id=newId();project.title="Übersicht";
            const QString planId=project.addDocument("schematic","Schaltplan",schematic::toJson(plan)),boardId=project.addDocument("perfboard","Platine",QJsonDocument::fromJson(board.encode()).object());
            const QString circuitId=project.addDocument("pcb","Leiterplatte",pcb::toJson(pcb::exampleDocument())),panelId=project.addDocument("frontpanel","Front",panel.toJson());
            const auto parts=projectParts(project);
            auto partNamed=[&](const QString &designator){for(const auto &p:parts)if(p.designator==designator)return p;return ProjectPart{};};
            require(partNamed("U1").documents==QStringList{planId,boardId,panelId}&&partNamed("U1").component==u1,"a part of the schematic, the perfboard and the front panel is one part");
            require(partNamed("U2").documents==QStringList{planId}&&partNamed("R9").documents==QStringList{boardId}&&partNamed("R9").value=="1k","parts of one document");
            bool onCircuit=false;for(const auto &p:parts)onCircuit=onCircuit||p.documents==QStringList{circuitId};require(onCircuit,"the circuit board's parts");
            const auto rows=partsList(parts,project);int counted=0;for(const auto &r:rows)counted+=r.count;
            require(counted==int(parts.size())&&!rows.isEmpty(),"the parts list counts every part of the schematic and the boards");
            const QByteArray csv=partsListCsv(rows);
            require(csv.startsWith("\xEF\xBB\xBF")&&csv.contains("Menge;Bezeichner;Wert\r\n")&&csv.contains(";R9;1k\r\n"),"the parts list as CSV");
            project.active=planId;const QString file=tmp.filePath("Übersicht.openloch");project.save(file);
            Suite suite(tmp.path());auto *window=dynamic_cast<schematic::Editor*>(suite.open(file));require(window,"the schematic of the project");
            require(suite.projectPartsOf(window).size()==parts.size(),"the parts of an open project");
            // Renamed: a closed perfboard takes the name as its title, a closed circuit board keeps it in the project.
            require(suite.renameDocument(window,boardId,"Hauptplatine")&&suite.renameDocument(window,circuitId,"LP"),"renaming documents");
            {const auto saved=ProjectFile::load(file);require(saved.documents[saved.indexOf(boardId)].name=="Hauptplatine"&&saved.documents[saved.indexOf(boardId)].data["title"]=="Hauptplatine"&&saved.documents[saved.indexOf(circuitId)].name=="LP","renamed documents are saved");}
            // Removed: not the window's own document, the others with their window, the project saved.
            require(!suite.removeDocument(window,planId),"the window's own document stays");
            auto *front=suite.openDocument(window,panelId);require(front,"the front panel of the project");
            require(suite.removeDocument(window,panelId)&&suite.removeDocument(window,circuitId),"removing documents");settle();
            {const auto saved=ProjectFile::load(file);require(saved.documents.size()==2&&saved.indexOf(panelId)<0&&saved.indexOf(circuitId)<0&&suite.windows().size()==1,"removed documents leave the project and their windows close");}
            // The overview lists the documents and their parts.
            QMenu *menu=window->findChild<QMenu*>("suiteMenu");require(menu,"the Fenster menu");emit menu->aboutToShow();
            int listed=-1,partRows=-1;QTimer::singleShot(0,[&]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget())){
                if(auto *t=d->findChild<QTreeWidget*>("projectDocuments"))listed=t->topLevelItemCount();if(auto *t=d->findChild<QTreeWidget*>("projectParts"))partRows=t->topLevelItemCount();
                if(const QString shot=qEnvironmentVariable("OPENLOCH_SUITE_OVERVIEW_SCREENSHOT");!shot.isEmpty()){d->grab().save(shot);if(auto *tabs=d->findChild<QTabWidget*>()){tabs->setCurrentIndex(1);d->grab().save(QString(shot).replace(".png","-parts.png"));}}
                d->reject();}});
            named(window,"suiteOverview")->trigger();
            require(listed==2&&partRows==3,"the overview lists the documents and the parts across them");
            for(auto *w:suite.windows())w->close();settle();
        }
        { // the example project: one circuit in all four kinds of document, the boards matching the schematic
            using namespace openloch::documents;
            const QString example=tmp.filePath("Blinklicht.openloch");QFile::remove(example);
            require(QFile::copy(QString(OPENLOCH_SOURCE_DIR)+"/examples/Blinklicht.openloch",example),"the example project");
            const auto file=ProjectFile::load(example);QStringList kinds;for(const auto &d:file.documents)kinds<<d.kind;
            require(kinds==QStringList{"schematic","perfboard","pcb","frontpanel"},"the example has all four kinds of document");
            const auto targets=schematicTargets(file.documents[0].data);
            const auto board=Project::decode(QJsonDocument(file.documents[1].data).toJson());
            require(targets.components.size()==13&&checkTargets(board,targetsForBoard(board,targets)).passed(),"the example's perfboard matches its schematic");
            const auto circuit=pcb::fromJson(file.documents[2].data);require(pcb::checkNets(circuit.boards[0],targets).passed(),"the example's circuit board matches its schematic");
            const auto parts=projectParts(file);int everywhere=0,onPanel=0;
            for(const auto &p:parts){if(p.documents.size()>=3)everywhere++;if(p.documents.contains(file.documents[3].id))onPanel++;}
            require(parts.size()==13&&everywhere==13&&onPanel==2,"every part is in schematic and boards, the LEDs also on the front panel");
            Suite suite(tmp.path());QMainWindow *opened=suite.open(example);require(opened,"opening the example");
            auto *panel=dynamic_cast<frontpanel::PanelEditor*>(suite.openDocument(opened,file.documents[3].id));require(panel,"the example's front panel");
            panel->reloadBoards();require(panel->boardsBehind().size()==1&&panel->boardsBehind()[0].source&&panel->compareWithBoards().isEmpty(),"the front panel's holes lie over the LEDs of the board behind it");
            for(auto *w:suite.windows())w->close();settle();
        }
        { // the macOS app declares every kind of file the suite opens, so that the Finder offers OpenLoch for it
            QFile plist(QString(OPENLOCH_SOURCE_DIR)+"/assets/Info.plist.in");require(plist.open(QIODevice::ReadOnly),"the Info.plist template");const QString text=QString::fromUtf8(plist.readAll());
            for(const QString suffix:{"openloch","olsch","olpcb","olfp","spl7","spl8","lm4","lmb","lay6","lay","fpl","lib"}){
                require(text.contains("<string>"+suffix+"</string>"),"the Info.plist must declare ."+suffix);
                if(suffix!="lib")require(documentKind("x."+suffix).has_value(),"the suite must open ."+suffix);
            }
        }
        { // a circuit board learns at once when a schematic joins its project, without opening a menu
            using namespace openloch::documents;
            ProjectFile project;project.id=newId();project.title="Nachher";project.addDocument("pcb","Platine",pcb::toJson(circuit));
            const QString file=tmp.filePath("Nachher.openloch");project.save(file);
            Suite suite(tmp.path());auto *window=dynamic_cast<PcbWindow*>(suite.open(file));require(window,"the circuit board of the project");auto *editor=window->editor();
            require(!editor->action("compareSchematic")->isEnabled(),"without a schematic the comparison is off");
            require(suite.addToProject(window,Kind::Schematic,false,file)&&editor->action("compareSchematic")->isEnabled()&&editor->action("placeMissing")->isEnabled(),"a schematic joining the project switches the circuit board's comparison on");
            require(suite.saveProject(window,false,file),"saving the project");for(auto *w:suite.windows())w->close();settle();
        }
        { // every kind drawn without a window (--render)
            for(const QString &file:{perfboardFile,panelFile,pcbFile,fplFile}){
                const QString picture=tmp.filePath(QFileInfo(file).completeBaseName()+"-"+QFileInfo(file).suffix()+".png");QString error;
                require(renderDocument(file,picture,&error)&&!QImage(picture).isNull(),"drawing "+file+" failed: "+error);
            }
            QString error;require(!renderDocument(tmp.filePath("x.txt"),tmp.filePath("x.png"),&error)&&!error.isEmpty(),"unknown files must be refused");
        }
        { // English and French names of the kinds
            setUiLanguage("en");const bool english=documentKinds()[2].name=="Circuit board"&&documentKinds()[3].name=="Front panel"&&documentKinds()[1].name=="Perfboard";
            setUiLanguage("fr");const bool french=documentKinds()[1].name=="Plaque à trous";setUiLanguage("de");
            require(english&&french,"translated names of the kinds");
        }
    }catch(const std::exception &e){QTextStream(stderr)<<e.what()<<"\n";return 1;}
    QTextStream(stdout)<<"suite tests passed\n";return 0;
}
