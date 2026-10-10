#include "suite.h"
#include "documents/libraryfolders.h"
#include "boardsources.h"
#include "schematictargets.h"
#include "pcbwindow.h"
#include "startscreen.h"
#include "canvas.h"
#include "language.h"
#include "project.h"
#include "window.h"
#include "fpl.h"
#include "paneldialogs.h"
#include "paneleditor.h"
#include "panelrender.h"
#include "modules/pcb/editor.h"
#include "modules/pcb/model.h"
#include "modules/schematic/editor.h"
#include "modules/schematic/render.h"
#include "formats/splan/splan.h"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <algorithm>
#include <functional>
#include <stdexcept>
namespace openloch::suite {
namespace {
const char *const kindProperty="openloch.documentKind";
const char *const recentKey="suite/recentDocuments";
constexpr int recentLimit=10;
bool testing(){return qApp->property("openloch.testing").toBool();}
QString cleanPath(const QString &path){return QDir::cleanPath(QFileInfo(path).absoluteFilePath());}
bool samePath(const QString &a,const QString &b){
    if(a.isEmpty()||b.isEmpty())return false;
    const QFileInfo x(a),y(b);
    return x.exists()&&y.exists()?x.canonicalFilePath()==y.canonicalFilePath():cleanPath(a)==cleanPath(b);
}
// Project files open as projects: OpenLoch's own and the own files of modules that save through projects.
bool isProjectFile(const QString &path){
    const QString suffix=QFileInfo(path).suffix().toLower();
    return suffix=="openloch"||(suffix=="olsch"&&Suite::projectCapable(Kind::Schematic))||(suffix=="olfp"&&Suite::projectCapable(Kind::FrontPanel))
        ||(suffix=="olpcb"&&Suite::projectCapable(Kind::Pcb));
}
std::optional<Kind> kindNamed(const QString &id){for(const auto &info:documentKinds())if(info.id==id)return info.kind;return std::nullopt;}
// The file a window's own editor saves; empty for new documents and documents of a project.
QString modulePath(const QWidget *window){
    if(auto *w=dynamic_cast<const Window*>(window))return w->path();
    if(auto *w=dynamic_cast<const frontpanel::PanelEditor*>(window))return w->filePath();
    if(auto *w=dynamic_cast<const PcbWindow*>(window))return w->documentPath();
    if(auto *w=dynamic_cast<const schematic::Editor*>(window))return w->filePath();
    return {};
}
// The title of a window's document as its editor keeps it (a schematic has none of its own: its file or kind).
QString moduleTitle(const QWidget *window){
    if(auto *w=dynamic_cast<const Window*>(window))return w->title();
    if(auto *w=dynamic_cast<const frontpanel::PanelEditor*>(window))return const_cast<frontpanel::PanelEditor*>(w)->document().title;
    if(auto *w=dynamic_cast<const PcbWindow*>(window))return w->editor()->displayName();
    if(auto *w=dynamic_cast<const schematic::Editor*>(window))return w->filePath().isEmpty()?ui("Schaltplan"):w->displayName();
    return {};
}
// The title an editor gives its window when it saves its own file (the same text as the editors write).
QString moduleWindowTitle(const QWidget *window){
    if(auto *w=dynamic_cast<const Window*>(window))return w->title()+ui("[*] — OpenLoch");
    if(auto *w=dynamic_cast<const frontpanel::PanelEditor*>(window))
        return QString("%1[*] – %2").arg(w->filePath().isEmpty()?ui("Unbenannt"):QFileInfo(w->filePath()).fileName(),ui("Frontplatte"));
    if(auto *w=dynamic_cast<const schematic::Editor*>(window))return QString("%1[*] – %2").arg(w->displayName(),ui("Schaltplan"));
    if(auto *w=dynamic_cast<const PcbWindow*>(window))return QString("%1[*] – %2").arg(w->editor()->displayName(),ui("Leiterplatte"));
    return window->windowTitle();
}
// The editors' part of the project contract: the document as data, the saved mark and the save handler.
QJsonObject dataOf(QMainWindow *window){
    if(auto *w=dynamic_cast<Window*>(window))return w->documentData();
    if(auto *w=dynamic_cast<frontpanel::PanelEditor*>(window))return w->documentData();
    if(auto *w=dynamic_cast<schematic::Editor*>(window))return w->documentData();
    if(auto *w=dynamic_cast<PcbWindow*>(window))return w->editor()->documentData();
    return {};
}
void markSavedOf(QMainWindow *window){
    if(auto *w=dynamic_cast<Window*>(window))w->markSaved();
    else if(auto *w=dynamic_cast<frontpanel::PanelEditor*>(window))w->markSaved();
    else if(auto *w=dynamic_cast<schematic::Editor*>(window))w->markSaved();
    else if(auto *w=dynamic_cast<PcbWindow*>(window))w->editor()->markSaved();
}
void saveThrough(QMainWindow *window,const std::function<bool(bool)> &handler){
    if(auto *w=dynamic_cast<Window*>(window))w->saveHandler=handler;
    else if(auto *w=dynamic_cast<frontpanel::PanelEditor*>(window))w->saveHandler=handler;
    else if(auto *w=dynamic_cast<schematic::Editor*>(window))w->saveHandler=handler;
    else if(auto *w=dynamic_cast<PcbWindow*>(window))w->editor()->saveHandler=handler;
}
// Öffnen in a module window goes through the suite's dialog, with that window's kind first.
void openThrough(QMainWindow *window,const std::function<bool(const QString&)> &handler){
    if(auto *w=dynamic_cast<Window*>(window))w->openHandler=handler;
    else if(auto *w=dynamic_cast<frontpanel::PanelEditor*>(window))w->openHandler=handler;
    else if(auto *w=dynamic_cast<schematic::Editor*>(window))w->openHandler=handler;
    else if(auto *w=dynamic_cast<PcbWindow*>(window))w->editor()->openHandler=handler;
}
// Editors whose document has a title of its own; the others keep the name the project gives them.
bool ownTitle(const QWidget *window){return dynamic_cast<const Window*>(window)||dynamic_cast<const frontpanel::PanelEditor*>(window);}
// A front panel file of any kind the module reads: its own format, FrontDesigner projects and library pages, backups.
frontpanel::Document readPanelFile(const QString &path){
    QFile file(path);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error(file.errorString().toStdString());
    const QByteArray bytes=file.readAll();const QString suffix=QFileInfo(path).suffix().toLower();
    if(bytes.trimmed().startsWith('{'))return frontpanel::Document::decode(bytes);
    if(suffix!="bak")return frontdesigner::readFrontDesigner(bytes,suffix=="lib");
    try{return frontdesigner::readFrontDesigner(bytes,false);}catch(const std::exception&){return frontdesigner::readFrontDesigner(bytes,true);}
}
QImage boardPicture(pcb::Editor &editor){
    const auto &board=editor.document().board();
    return editor.view()->render(QSize(1600,std::max(200,int(1600*board.height/board.width))));
}
}

Suite::Suite(const QString &lochMasterAssets,QObject *parent):QObject(parent),assets(lochMasterAssets){
    // The program ends with the start screen or Beenden, not when the last document closes (that shows the start screen).
    QApplication::setQuitOnLastWindowClosed(false);
}
Suite::~Suite(){
    finished=true;
    for(auto *window:windows())delete window;
    delete start.data();
}

StartScreen *Suite::startScreen() const{return start.data();}
QList<QMainWindow*> Suite::windows() const{
    QList<QMainWindow*> list;for(const auto &window:documents)if(window)list.append(window.data());return list;
}
std::optional<Kind> Suite::kindOf(const QWidget *window){
    const QVariant kind=window?window->property(kindProperty):QVariant();
    if(!kind.isValid())return std::nullopt;
    return Kind(kind.toInt());
}
// Every editor saves through a project; a kind without editor (none at present) could not be part of one.
bool Suite::projectCapable(Kind kind){return kindInfo(kind).available;}
Suite::OpenProject *Suite::projectOf(const QWidget *window) const{
    if(!window)return nullptr;
    for(const auto &project:projects)for(const auto &w:project->windows)if(w.data()==window)return project.get();
    return nullptr;
}
bool Suite::inProject(const QWidget *window) const{const OpenProject *project=projectOf(window);return project&&!project->transparent;}
const documents::ProjectFile *Suite::projectFile(const QWidget *window) const{const OpenProject *project=projectOf(window);return project?&project->file:nullptr;}
QString Suite::documentPath(const QWidget *window) const{
    const OpenProject *project=projectOf(window);
    return project&&!project->transparent?project->path:modulePath(window);
}
QString Suite::documentName(const QWidget *window) const{
    if(inProject(window))return nameOf(window);
    const QString path=modulePath(window);if(!path.isEmpty())return QFileInfo(path).fileName();
    // A new document: the name the window shows before the dash of the program or module name.
    QString title=window->windowTitle();title.remove("[*]");
    for(const QString &dash:{QStringLiteral(" — "),QStringLiteral(" – ")}){const int at=title.lastIndexOf(dash);if(at>0){title.truncate(at);break;}}
    return title.trimmed();
}

QMainWindow *Suite::open(const QString &given){
    if(given.isEmpty())return nullptr;
    const QString path=cleanPath(given);QWidget *parent=QApplication::activeWindow();
    if(!QFileInfo(path).isFile()){QMessageBox::warning(parent,ui("Öffnen"),ui("„%1“ wurde nicht gefunden.").arg(QDir::toNativeSeparators(path)));return nullptr;}
    if(auto *window=windowFor(path)){present(window);return window;}
    if(isProjectFile(path))return openProjectFile(path);
    const auto kind=documentKind(path);
    if(!kind){
        QFile file(path);const bool splanPage=QFileInfo(path).suffix().compare("lib",Qt::CaseInsensitive)==0&&file.open(QIODevice::ReadOnly)&&splan::version(file.read(16));
        QMessageBox::warning(parent,ui("Öffnen"),splanPage?ui("„%1“ ist eine Bibliotheksseite von sPlan. Der Schaltplan zeigt solche Seiten in seiner Bibliothek, wenn sie im Bibliotheksordner liegen.").arg(QFileInfo(path).fileName())
            :ui("Dieses Dateiformat kann OpenLoch nicht öffnen:\n%1").arg(QDir::toNativeSeparators(path)));
        return nullptr;
    }
    // A file of an original: one document whose editor saves back into that file.
    QMainWindow *window=reusableWindow(*kind);const bool fresh=!window;
    if(fresh)window=makeWindow(*kind);else release(window);
    if(!openIn(window,*kind,path)){if(fresh)delete window;else newProject(window,*kind,!projectCapable(*kind));return nullptr;}
    newProject(window,*kind,true);
    // A backup continues as its project; that is the file to remember.
    const QString shown=modulePath(window);remember(shown.isEmpty()?path:shown);
    present(window);return window;
}
QMainWindow *Suite::create(Kind kind,bool ask){
    QMainWindow *window=newDocumentWindow(kind,ask);if(!window)return nullptr;
    // An editor that cannot save through a project yet saves its own file, as before.
    newProject(window,kind,!projectCapable(kind));
    present(window);return window;
}
QMainWindow *Suite::addToProject(QMainWindow *window,Kind kind,bool ask,const QString &projectFile){
    OpenProject *project=projectOf(window);
    if(!project||!kindInfo(kind).available||!projectCapable(kind))return nullptr;
    QMainWindow *first=project->transparent?project->windows.value(project->file.documents.first().id).data():nullptr;
    if(project->transparent&&(!first||!kindOf(first)||!projectCapable(*kindOf(first)))){
        QMessageBox::information(window,ui("Zum Projekt hinzufügen"),ui("Dieses Dokument kann noch nicht Teil eines Projekts werden."));return nullptr;
    }
    QMainWindow *added=newDocumentWindow(kind,ask);if(!added)return nullptr;
    const documents::ProjectFile before=project->file;
    if(first){
        // The document of an original joins the project; its file stays as it is.
        auto &own=project->file.documents.first();const QString file=modulePath(first);
        if(!file.isEmpty()){
            own.origin=QJsonObject{{"format",QFileInfo(file).suffix().toLower()},{"file",QFileInfo(file).fileName()}};
            if(project->file.title.isEmpty())project->file.title=QFileInfo(file).completeBaseName();
        }
        own.name=moduleTitle(first);project->transparent=false;bind(first,project,own.id);
    }
    const QString id=project->file.addDocument(kindInfo(kind).id,moduleTitle(added),dataOf(added));
    bind(added,project,id);
    if(!saveProject(added,project->path.isEmpty(),projectFile)){
        // Without a project file everything stays as it was.
        release(added);delete added;project->file=before;
        if(first){project->transparent=true;bind(first,project,before.documents.first().id);first->setWindowTitle(moduleWindowTitle(first));}
        return nullptr;
    }
    project->file.active=id;present(added);return added;
}
QMainWindow *Suite::openDocument(QMainWindow *window,const QString &document){
    OpenProject *project=projectOf(window);return project?openDocumentWindow(project,document):nullptr;
}
bool Suite::saveProject(QMainWindow *window,bool asNew,const QString &projectFile){
    OpenProject *project=projectOf(window);if(!project||project->transparent)return false;
    QString path=projectFile.isEmpty()?project->path:projectFile;
    if(projectFile.isEmpty()&&(asNew||path.isEmpty())){
        QString folder=!project->source.isEmpty()?QFileInfo(project->source).absolutePath():testing()?QString():QSettings().value("dialogs/lastDirectory").toString();
        if(folder.isEmpty()||!QFileInfo(folder).isDir())folder=QDir::homePath();
        const QString name=project->file.title.isEmpty()?moduleTitle(window):project->file.title;
        path=QFileDialog::getSaveFileName(window,ui("Projekt speichern"),QDir(folder).filePath(name+".openloch"),ui("OpenLoch-Projekt (*.openloch)"));
        if(path.isEmpty())return false;
    }
    if(QFileInfo(path).suffix().compare("openloch",Qt::CaseInsensitive)!=0)path+=".openloch";
    path=cleanPath(path);
    // Open windows give the state of their documents; documents without window stay as stored.
    documents::ProjectFile file=project->file;
    for(auto it=project->windows.constBegin();it!=project->windows.constEnd();++it){
        const int i=file.indexOf(it.key());if(i<0||!it.value())continue;
        file.documents[i].data=dataOf(it.value());if(ownTitle(it.value()))file.documents[i].name=moduleTitle(it.value());
    }
    if(path!=project->path||file.title.isEmpty())file.title=QFileInfo(path).completeBaseName();
    try{file.save(path);}
    catch(const std::exception &e){
        QMessageBox::warning(window,ui("Speichern"),ui("%1 kann nicht gespeichert werden:\n%2").arg(QFileInfo(path).fileName(),QString::fromUtf8(e.what())));return false;
    }
    file.converted=false;project->file=file;project->path=path;
    for(const auto &w:project->windows)if(w){markSavedOf(w);updateTitle(w);}
    remember(path);if(!testing())QSettings().setValue("dialogs/lastDirectory",QFileInfo(path).absolutePath());
    return true;
}
QString Suite::openStartFolder(const QString &given){
    QString folder=given;
    if(!testing()){
        if(folder.isEmpty()||!QFileInfo(folder).isDir())folder=QSettings().value("dialogs/lastDirectory").toString();
        if(folder.isEmpty()||!QFileInfo(folder).isDir())folder=openLochDocumentsFolder(QString());
    }
    return !folder.isEmpty()&&QFileInfo(folder).isDir()?folder:QDir::homePath();
}
QMainWindow *Suite::openDialog(QWidget *parent,std::optional<Kind> kind,const QString &given){
    const QString path=QFileDialog::getOpenFileName(parent,ui("Dokument öffnen"),openStartFolder(given),documentFilter(kind));
    if(path.isEmpty())return nullptr;
    if(!testing())QSettings().setValue("dialogs/lastDirectory",QFileInfo(path).absolutePath());   // shared with the Lochraster window's dialogs
    return open(path);
}

void Suite::newProject(QMainWindow *window,Kind kind,bool transparent){
    auto project=std::make_shared<OpenProject>();project->transparent=transparent;project->file.id=documents::newId();
    const QString id=project->file.addDocument(kindInfo(kind).id,moduleTitle(window),transparent?QJsonObject{}:dataOf(window));
    projects.append(project);bind(window,project.get(),id);
}
// A window of a project saves through it; a transparent one through its own editor. Either way the suite hears of
// its title, to show the project in it.
void Suite::bind(QMainWindow *window,OpenProject *project,const QString &document){
    project->windows[document]=window;
    saveThrough(window,project->transparent?std::function<bool(bool)>():[this,window](bool asNew){return saveProject(window,asNew);});
    if(auto *w=dynamic_cast<Window*>(window))w->titleChanged=[this,window]{updateTitle(window);};
    else if(auto *w=dynamic_cast<frontpanel::PanelEditor*>(window))w->titleChanged=[this,window]{updateTitle(window);};
    else if(auto *w=dynamic_cast<schematic::Editor*>(window))w->titleChanged=[this,window]{updateTitle(window);};
    else if(auto *w=dynamic_cast<PcbWindow*>(window))w->titleChanged=[this,window]{updateTitle(window);};
    updateTitle(window);
    notifyBoards(project);
}
void Suite::notifyBoards(OpenProject *project){
    for(const auto &open:project->windows){
        if(auto *board=dynamic_cast<Window*>(open.data()))board->projectChanged();
        else if(auto *circuit=dynamic_cast<PcbWindow*>(open.data()))circuit->editor()->projectChanged();
    }
}
bool Suite::renameDocument(QMainWindow *window,const QString &document,const QString &name){
    OpenProject *project=projectOf(window);const QString title=name.trimmed();if(!project||title.isEmpty())return false;
    const int i=project->file.indexOf(document);if(i<0)return false;
    auto &d=project->file.documents[i];QMainWindow *open=project->windows.value(document).data();
    // Editors with a title of their own take it as an edit; closed documents of their kind keep it in their data.
    if(auto *w=dynamic_cast<Window*>(open))w->setTitle(title);
    else if(auto *w=dynamic_cast<frontpanel::PanelEditor*>(open))w->change([&](frontpanel::Document &doc){doc.title=title;});
    else if(!open&&(d.kind==kindInfo(Kind::Perfboard).id||d.kind==kindInfo(Kind::FrontPanel).id))d.data["title"]=title;
    d.name=title;if(open)updateTitle(open);
    return project->transparent||saveProject(window,false);
}
bool Suite::removeDocument(QMainWindow *window,const QString &document){
    OpenProject *project=projectOf(window);if(!project||project->transparent||project->file.documents.size()<2)return false;
    const int i=project->file.indexOf(document);QMainWindow *open=project->windows.value(document).data();
    if(i<0||open==window)return false;
    if(open){release(open);markSavedOf(open);open->close();}
    project->file.documents.removeAt(i);
    if(project->file.indexOf(project->file.active)<0)project->file.active=project->file.documents.first().id;
    notifyBoards(project);
    return saveProject(window,false);
}
QList<ProjectPart> Suite::projectPartsOf(const QWidget *window) const{
    const OpenProject *project=projectOf(window);if(!project)return {};
    QMap<QString,QJsonObject> current;for(auto it=project->windows.constBegin();it!=project->windows.constEnd();++it)if(it.value())current.insert(it.key(),dataOf(it.value()));
    return projectParts(project->file,current);
}
void Suite::release(QMainWindow *window){
    for(const auto &project:projects)for(auto it=project->windows.begin();it!=project->windows.end();)it=it.value()==window?project->windows.erase(it):std::next(it);
    projects.removeIf([](const std::shared_ptr<OpenProject> &project){return project->windows.isEmpty();});
    saveThrough(window,nullptr);
}
QString Suite::nameOf(const QWidget *window) const{
    const OpenProject *project=projectOf(window);
    if(project&&!ownTitle(window))for(auto it=project->windows.constBegin();it!=project->windows.constEnd();++it)if(it.value().data()==window){
        const int i=project->file.indexOf(it.key());if(i>=0&&!project->file.documents[i].name.isEmpty())return project->file.documents[i].name;
    }
    return moduleTitle(window);
}
void Suite::updateTitle(QMainWindow *window){
    const OpenProject *project=projectOf(window);if(!project||project->transparent)return;   // the editor's own title
    const QString kind=kindInfo(kindOf(window).value_or(Kind::Perfboard)).name,file=QFileInfo(project->path).completeBaseName();
    QString name=nameOf(window);if(name==kind)name.clear();
    const QString label=file.isEmpty()?(name.isEmpty()?kind:name):name.isEmpty()||file==name?file:QString("%1 – %2").arg(file,name);
    window->setWindowTitle(QString("%1[*] – %2").arg(label,kind));
}
QMainWindow *Suite::makeWindow(Kind kind){
    QMainWindow *window=nullptr;
    switch(kind){
    case Kind::Perfboard:window=new Window(assets);break;
    case Kind::FrontPanel:window=new frontpanel::PanelEditor;window->resize(1400,900);break;
    case Kind::Pcb:window=new PcbWindow;break;
    case Kind::Schematic:{auto *editor=new schematic::Editor;editor->loadPreferences();editor->resize(1400,900);window=editor;break;}
    }
    window->setAttribute(Qt::WA_DeleteOnClose);attach(window,kind);return window;
}
QMainWindow *Suite::newDocumentWindow(Kind kind,bool ask){
    if(!kindInfo(kind).available)return nullptr;
    QWidget *parent=QApplication::activeWindow();
    QMainWindow *window=makeWindow(kind);bool made=false;
    switch(kind){
    case Kind::Perfboard:made=!ask||static_cast<Window*>(window)->newProject("board");break;
    case Kind::FrontPanel:{
        frontpanel::Document document;made=!ask||frontpanel::editPanelProperties(parent?parent:window,document.panels[0],true);
        if(made)static_cast<frontpanel::PanelEditor*>(window)->setDocument(document);
        break;}
    case Kind::Pcb:{
        // The module's dialog for new boards as in Sprint-Layout: working area, rectangular or round outline with margin.
        auto *editor=static_cast<PcbWindow*>(window)->editor();
        // The origin follows the module's preference: the top left corner of the outline, or of a plain working area.
        auto board=ask?pcb::askNewBoard(parent?parent:window,ui("Platine 1"),editor->originTopLeft):std::optional<pcb::Board>(pcb::newBoard(ui("Platine 1")));
        made=board.has_value();
        if(made){if(!ask&&editor->originTopLeft)board->origin={0,0};pcb::Document document;document.boards={*board};editor->setDocument(document);}
        break;}
    case Kind::Schematic:made=true;break;   // a new schematic window shows a new document (one sheet A4 landscape)
    }
    if(!made){delete window;return nullptr;}
    return window;
}
QMainWindow *Suite::openProjectFile(const QString &path){
    QWidget *parent=QApplication::activeWindow();documents::ProjectFile file;
    try{file=documents::ProjectFile::load(path);}
    catch(const std::exception &e){QMessageBox::warning(parent,ui("Öffnen"),ui("%1 kann nicht geöffnet werden:\n%2").arg(QFileInfo(path).fileName(),QString::fromUtf8(e.what())));return nullptr;}
    auto project=std::make_shared<OpenProject>();project->file=file;project->source=path;
    // A project file is saved in place (an older version then as version 2); a module's own file becomes a new
    // project file and stays as it is.
    if(QFileInfo(path).suffix().compare("openloch",Qt::CaseInsensitive)==0)project->path=path;
    projects.append(project);
    QMainWindow *window=openDocumentWindow(project.get(),file.active);
    for(int i=0;!window&&i<file.documents.size();i++)if(file.documents[i].id!=file.active){
        const auto kind=kindNamed(file.documents[i].kind);if(kind&&projectCapable(*kind))window=openDocumentWindow(project.get(),file.documents[i].id);
    }
    if(!window){projects.removeOne(project);return nullptr;}
    remember(path);return window;
}
QMainWindow *Suite::openDocumentWindow(OpenProject *project,const QString &document){
    if(auto window=project->windows.value(document);window){present(window);return window;}
    const int i=project->file.indexOf(document);if(i<0)return nullptr;
    const documents::ProjectDocument d=project->file.documents[i];const auto kind=kindNamed(d.kind);
    QWidget *parent=QApplication::activeWindow();
    if(!kind||!projectCapable(*kind)){
        QMessageBox::information(parent,ui("Öffnen"),ui("„%1“ kann in dieser Fassung von OpenLoch noch nicht geöffnet werden.").arg(d.name.isEmpty()?d.kind:d.name));return nullptr;
    }
    QMainWindow *window=makeWindow(*kind);
    try{
        if(auto *w=dynamic_cast<Window*>(window))w->setDocumentData(d.data);
        else if(auto *w=dynamic_cast<frontpanel::PanelEditor*>(window)){w->setDocument(frontpanel::Document::fromJson(d.data));w->reportMissingFonts();}
        else if(auto *w=dynamic_cast<schematic::Editor*>(window))w->setDocument(schematic::fromJson(d.data));
        else if(auto *w=dynamic_cast<PcbWindow*>(window))w->editor()->setDocument(pcb::fromJson(d.data));
    }catch(const std::exception &e){
        delete window;QMessageBox::warning(parent,ui("Öffnen"),ui("%1 kann nicht geöffnet werden:\n%2").arg(d.name,QString::fromUtf8(e.what())));return nullptr;
    }
    bind(window,project,d.id);project->file.active=d.id;present(window);return window;
}
QMainWindow *Suite::windowFor(const QString &path) const{
    for(const auto &project:projects){
        if(project->transparent||!(samePath(project->path,path)||samePath(project->source,path)))continue;
        if(auto window=project->windows.value(project->file.active);window)return window;
        for(const auto &window:project->windows)if(window)return window;
    }
    for(auto *window:windows())if(!inProject(window)&&samePath(modulePath(window),path))return window;
    return nullptr;
}
QMainWindow *Suite::reusableWindow(Kind kind) const{
    for(auto *window:windows()){
        if(kindOf(window)!=kind||!modulePath(window).isEmpty())continue;
        // Only a new, single document that came from no file.
        const OpenProject *project=projectOf(window);
        if(project&&(project->file.documents.size()>1||!project->path.isEmpty()||!project->source.isEmpty()))continue;
        if(auto *w=dynamic_cast<frontpanel::PanelEditor*>(window);w&&!w->modified())return window;
        if(auto *w=dynamic_cast<PcbWindow*>(window);w&&!w->editor()->isModified())return window;
        if(auto *w=dynamic_cast<schematic::Editor*>(window);w&&!w->isModified())return window;
        if(dynamic_cast<Window*>(window)&&!window->isWindowModified())return window;
    }
    return nullptr;
}
bool Suite::openIn(QMainWindow *window,Kind kind,const QString &path){
    switch(kind){
    case Kind::Perfboard:{auto *w=static_cast<Window*>(window);if(!w->openPath(path))return false;w->rememberDirectory(path);return true;}
    case Kind::FrontPanel:return static_cast<frontpanel::PanelEditor*>(window)->open(path);
    case Kind::Pcb:{
        auto *w=static_cast<PcbWindow*>(window);QString error;
        if(!w->editor()->openFile(path,&error)){QMessageBox::warning(window->isVisible()?window:QApplication::activeWindow(),ui("Öffnen"),ui("%1 kann nicht geöffnet werden:\n%2").arg(QFileInfo(path).fileName(),error));return false;}
        w->source=path;return true;}
    case Kind::Schematic:{
        auto *w=static_cast<schematic::Editor*>(window);QString error;
        if(!w->openFile(path,&error)){QMessageBox::warning(window->isVisible()?window:QApplication::activeWindow(),ui("Öffnen"),ui("%1 kann nicht geöffnet werden:\n%2").arg(QFileInfo(path).fileName(),error));return false;}
        return true;}
    }
    return false;
}
void Suite::present(QMainWindow *window){
    // A second window of a kind would cover the first exactly; it moves a little.
    if(!window->isVisible())for(auto *other:windows())if(other!=window&&other->isVisible()&&other->pos()==window->pos())window->move(window->pos()+QPoint(28,28));
    window->show();window->raise();window->activateWindow();
    if(start&&start->isVisible())start->hide();
}

void Suite::attach(QMainWindow *window,Kind kind){
    window->setProperty(kindProperty,int(kind));documents.append(window);
    connect(window,&QObject::destroyed,this,[this](QObject *gone){documentClosed(gone);});
    // The window shows another file after opening or saving, and its title changes with it.
    connect(window,&QWidget::windowTitleChanged,this,[this,window]{
        const QString path=documentPath(window);
        if(!path.isEmpty()&&window->property("openloch.recentPath").toString()!=path){window->setProperty("openloch.recentPath",path);remember(path);}
    });
    openThrough(window,[this,guard=QPointer<QMainWindow>(window),kind](const QString &folder){return openDialog(guard,kind,folder)!=nullptr;});
    // The Fenster menu goes before Hilfe, where the window has one.
    auto *menu=new QMenu(ui("&Fenster"),window);menu->setObjectName("suiteMenu");
    connect(menu,&QMenu::aboutToShow,this,[this,menu,window]{fillWindowMenu(menu,window);});fillWindowMenu(menu,window);
    QAction *help=nullptr;for(auto *a:window->menuBar()->actions())if(a->menu()&&a->text()==ui("&Hilfe"))help=a;
    window->menuBar()->insertMenu(help,menu);
    // Beenden in the window's own menu closes the window as before; then the other windows follow and the program ends.
    if(auto *quitAction=window->findChild<QAction*>("quit"))
        connect(quitAction,&QAction::triggered,this,[this,guard=QPointer<QMainWindow>(window)]{if(!guard||!guard->isVisible())quit();});
    // A board compares itself with the schematic of its project (docs/file-formats.md, "Übernahme vom Schaltplan"); the
    // project is looked up at each call, as a window can join another one.
    const QPointer<QMainWindow> guard(window);
    auto hasSchematic=[this,guard]{return guard&&schematicData(guard,false).has_value();};
    auto targets=[this,guard]{const auto data=guard?schematicData(guard,true):std::nullopt;return data?schematicTargets(*data):documents::Targets{};};
    if(kind==Kind::Perfboard){auto *board=static_cast<Window*>(window);board->hasSchematic=hasSchematic;board->targets=targets;}
    else if(kind==Kind::Pcb){auto *circuit=static_cast<PcbWindow*>(window)->editor();circuit->hasSchematic=hasSchematic;circuit->targets=targets;}
    // A front panel learns the boards of its project that can sit behind it, also looked up at each call.
    else if(kind==Kind::FrontPanel)static_cast<frontpanel::PanelEditor*>(window)->boardSources=[this,guard]{return guard?boardSources(guard):QList<frontpanel::BoardSource>{};};
}
QList<frontpanel::BoardSource> Suite::boardSources(const QWidget *window){
    OpenProject *project=projectOf(window);if(!project)return {};
    QList<frontpanel::BoardSource> out;
    for(auto &d:project->file.documents){
        const bool perfboard=d.kind==kindInfo(Kind::Perfboard).id;if(!perfboard&&d.kind!=kindInfo(Kind::Pcb).id)continue;
        QMainWindow *open=project->windows.value(d.id).data();QJsonObject current;QJsonObject &data=open?current:d.data;if(open)current=dataOf(open);
        const QString name=open?documentName(open):d.name;
        try{out+=perfboard?perfboardSources(d.id,name,data):circuitBoardSources(d.id,name,data);}catch(const std::exception &){}
    }
    return out;
}
std::optional<QJsonObject> Suite::schematicData(const QWidget *window,bool data) const{
    const auto *project=projectOf(window);if(!project)return std::nullopt;
    for(const auto &d:project->file.documents){
        if(d.kind!=kindInfo(Kind::Schematic).id)continue;
        if(!data)return QJsonObject{};
        if(auto *open=project->windows.value(d.id).data())return dataOf(open);
        return d.data;
    }
    return std::nullopt;
}
void Suite::fillWindowMenu(QMenu *menu,QMainWindow *window){
    menu->clear();
    auto add=[this](QMenu *into,const QString &text,const QString &name,auto run){auto *a=into->addAction(text);a->setObjectName(name);connect(a,&QAction::triggered,this,run);return a;};
    auto later=[](const KindInfo &info){return QString("%1 (%2)").arg(info.name,ui("folgt"));};
    auto *fresh=menu->addMenu(ui("&Neues Dokument"));fresh->setObjectName("suiteNew");
    for(const auto &info:documentKinds())
        add(fresh,info.available?info.name+QStringLiteral("…"):later(info),"suiteNew-"+info.id,[this,kind=info.kind]{create(kind);})->setEnabled(info.available);
    // A further document in this window's project; kinds whose editor cannot save through a project yet follow.
    auto *join=menu->addMenu(ui("Zum &Projekt hinzufügen"));join->setObjectName("suiteAdd");
    const auto own=kindOf(window);const bool joinable=own&&projectCapable(*own);
    for(const auto &info:documentKinds()){
        const bool can=joinable&&info.available&&projectCapable(info.kind);
        add(join,can?info.name+QStringLiteral("…"):later(info),"suiteAdd-"+info.id,[this,window,kind=info.kind]{addToProject(window,kind);})->setEnabled(can);
    }
    add(menu,ui("Projekt&übersicht…"),"suiteOverview",[this,window]{showProjectOverview(window);})->setEnabled(projectOf(window)!=nullptr);
    add(menu,ui("Bi&bliotheken…"),"suiteLibraries",[this,window]{showLibraries(window);});
    add(menu,ui("Dokument ö&ffnen…"),"suiteOpen",[this,window]{openDialog(window);});
    auto *recent=menu->addMenu(ui("&Zuletzt verwendet"));recent->setObjectName("suiteRecent");
    const QStringList files=recentDocuments();
    for(const auto &file:files)add(recent,QFileInfo(file).fileName().replace('&',"&&"),"",[this,file]{open(file);})->setToolTip(QDir::toNativeSeparators(file));
    if(files.isEmpty())recent->addAction(ui("Keine"))->setEnabled(false);
    else{recent->addSeparator();add(recent,ui("Liste leeren"),"suiteClearRecent",[this]{clearRecent();});}
    menu->addSeparator();add(menu,ui("&Startbildschirm"),"suiteStart",[this]{showStartScreen();});
    // The documents, by project: a project with a file or several documents under its name, documents without window
    // open from here.
    if(!projects.isEmpty())menu->addSeparator();
    for(const auto &project:projects){
        const bool group=!project->transparent&&(project->file.documents.size()>1||!project->path.isEmpty());
        if(group)menu->addAction(QFileInfo(project->path).completeBaseName().isEmpty()?project->file.title:QFileInfo(project->path).completeBaseName())->setEnabled(false);
        for(const auto &d:project->file.documents){
            QMainWindow *w=project->windows.value(d.id);const auto kind=kindNamed(d.kind);
            const QString name=w?documentName(w):d.name.isEmpty()?d.kind:d.name;
            QString text=QString("%1 – %2").arg(name,kind?kindInfo(*kind).name:d.kind)+(w&&w->isWindowModified()?" *":"");
            if(group)text.prepend(QStringLiteral("    "));
            if(w){auto *a=add(menu,text.replace('&',"&&"),"",[this,guard=QPointer<QMainWindow>(w)]{if(guard)present(guard);});a->setCheckable(true);a->setChecked(w==window);}
            else add(menu,text.replace('&',"&&"),"",[this,window,id=d.id]{openDocument(window,id);})->setEnabled(kind&&projectCapable(*kind));
        }
    }
}
void Suite::documentClosed(QObject *gone){
    documents.removeIf([gone](const QPointer<QMainWindow> &window){return !window||window.data()==gone;});
    for(const auto &project:projects)for(auto it=project->windows.begin();it!=project->windows.end();)it=!it.value()||it.value().data()==gone?project->windows.erase(it):std::next(it);
    projects.removeIf([](const std::shared_ptr<OpenProject> &project){return project->windows.isEmpty();});
    if(!documents.isEmpty()||quitting||finished)return;
    showStartScreen();   // the last document closed: back to the start, the program goes on
}

void Suite::showStartScreen(){
    if(finished)return;
    if(!start){start=new StartScreen(this);start->closed=[this]{startScreenClosed();};}
    start->refresh();start->show();start->raise();start->activateWindow();
}
void Suite::offerRecovery(){
    if(QDir(Window::recoveryFolder()).entryList({"*.openloch"},QDir::Files).isEmpty())return;
    auto *window=static_cast<Window*>(makeWindow(Kind::Perfboard));present(window);window->restoreRecovery();
    if(window->path().isEmpty()&&!window->isWindowModified()){window->close();return;}   // nothing restored: back to the start screen
    // A restored document with its own file continues there; one never saved continues as a new project.
    newProject(window,Kind::Perfboard,!window->path().isEmpty());
}
void Suite::startScreenClosed(){
    // With no document open, the start screen is the program.
    if(windows().isEmpty()&&!finished){finished=true;QTimer::singleShot(0,qApp,[]{QCoreApplication::quit();});}
}
bool Suite::closeAll(){
    const auto list=windows();
    for(auto it=list.crbegin();it!=list.crend();++it){
        QMainWindow *window=*it;if(!window->isVisible())continue;
        window->raise();window->activateWindow();if(!window->close())return false;
    }
    if(start)start->hide();
    return true;
}
void Suite::quit(){
    quitting=true;
    if(!closeAll()){quitting=false;return;}
    finished=true;QTimer::singleShot(0,qApp,[]{QCoreApplication::quit();});
}

QStringList Suite::recentDocuments() const{return QSettings().value(recentKey).toStringList();}
void Suite::remember(const QString &path){
    const QString file=cleanPath(path);QStringList list=recentDocuments();
    list.erase(std::remove_if(list.begin(),list.end(),[&](const QString &other){return other==file||samePath(other,file);}),list.end());
    list.prepend(file);while(list.size()>recentLimit)list.removeLast();
    QSettings().setValue(recentKey,list);if(start)start->refresh();
}
void Suite::forget(const QString &path){QStringList list=recentDocuments();list.removeAll(path);QSettings().setValue(recentKey,list);if(start)start->refresh();}
void Suite::clearRecent(){QSettings().remove(recentKey);if(start)start->refresh();}

bool renderDocument(const QString &path,const QString &picture,QString *error){
    auto fail=[error](const QString &why){if(error)*error=why;return false;};
    auto perfboard=[&](Project board){Canvas canvas;canvas.setProject(&board);QString why;return canvas.exportImage(picture,&why)||fail(why.isEmpty()?ui("Bildexport fehlgeschlagen"):why);};
    try{
        QImage image;
        if(isProjectFile(path)){
            // A project: its active document.
            const auto project=documents::ProjectFile::load(path);const auto &d=project.documents[project.indexOf(project.active)];
            if(d.kind=="perfboard")return perfboard(Project::decode(QJsonDocument(d.data).toJson(QJsonDocument::Compact)));
            if(d.kind=="frontpanel"){const auto document=frontpanel::Document::fromJson(d.data);image=frontpanel::renderPanel(document,document.panel(),300);}
            else if(d.kind=="pcb"){pcb::Editor editor;editor.setDocument(pcb::fromJson(d.data));image=boardPicture(editor);}
            else if(d.kind=="schematic"){const auto document=schematic::fromJson(d.data);image=schematic::renderSheet(document,document.activeSheet,300/25.4);}
            else return fail(ui("„%1“ kann in dieser Fassung von OpenLoch noch nicht geöffnet werden.").arg(d.name.isEmpty()?d.kind:d.name));
        }else{
            const auto kind=documentKind(path);
            if(!kind)return fail(ui("Dieses Dateiformat kann OpenLoch nicht öffnen:\n%1").arg(QDir::toNativeSeparators(path)));
            switch(*kind){
            case Kind::Perfboard:return perfboard(Project::load(path));
            case Kind::FrontPanel:{const auto document=readPanelFile(path);image=frontpanel::renderPanel(document,document.panel(),300);break;}
            case Kind::Pcb:{pcb::Editor editor;QString why;if(!editor.openFile(path,&why))return fail(why);image=boardPicture(editor);break;}
            case Kind::Schematic:{const auto document=schematic::load(path);image=schematic::renderSheet(document,document.activeSheet,300/25.4);break;}
            }
        }
        return (!image.isNull()&&image.save(picture))||fail(ui("Bildexport fehlgeschlagen"));
    }catch(const std::exception &e){return fail(QString::fromUtf8(e.what()));}
}
}
