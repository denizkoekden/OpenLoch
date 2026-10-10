#include "library.h"
#include "dialogs.h"
#include "example.h"
#include "render.h"
#include "zip.h"
#include "language.h"
#include "legacy_reader.h"
#include "formats/splan/splan.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QDrag>
#include <QUrl>
#include <QMessageBox>
#include <QInputDialog>
#include <QDragEnterEvent>
#include <QDesktopServices>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidgetItemIterator>
#include <QTreeWidget>
#include <QTimer>
#include <QStyle>
#include <QMenu>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

namespace openloch::schematic {
namespace {
[[noreturn]] void invalid(const QString &message){throw FormatError(message);}
constexpr int libraryVersion=3;
}
QString localized(const QString &text){
    const auto parts=text.split(u'\r');
    const int index=uiLanguage()==u"en"?1:uiLanguage()==u"fr"?2:0;
    return index<parts.size()&&!parts[index].trimmed().isEmpty()?parts[index]:parts[0];
}
QJsonObject pageToJson(const LibraryPage &page){
    QJsonArray symbols;
    for(const auto &e:page.entries){
        QJsonObject o{{"caption",e.caption},{"item",itemToJson(e.symbol)}};
        if(!e.children.isEmpty()){QJsonArray children;for(const auto &c:e.children)children.append(itemToJson(c));o["children"]=children;}
        if(!e.resources.isEmpty()){
            QJsonObject resources;
            for(auto it=e.resources.cbegin();it!=e.resources.cend();++it)resources[it.key()]=QJsonObject{{"kind",it->kind},{"data",QString::fromLatin1(it->data.toBase64())}};
            o["resources"]=resources;
        }
        symbols.append(o);
    }
    QJsonObject o{{"format","OpenLoch Schematic Library"},{"version",libraryVersion},{"unit","mm"},{"name",page.name},{"symbols",symbols}};
    if(!page.folder.isEmpty())o["folder"]=page.folder;
    return o;
}
LibraryPage pageFromJson(const QJsonObject &json){
    if(json["format"]!="OpenLoch Schematic Library")invalid(ui("Keine OpenLoch-Schaltplanbibliothek"));
    if(!json["version"].isDouble()||json["version"].toInt()<1)invalid(ui("Diese Version der Schaltplanbibliothek wird nicht unterstützt"));
    if(json["version"].toInt()>libraryVersion)invalid(ui("Die Bibliothek stammt von einer neueren Version von OpenLoch und kann nicht gelesen werden"));
    if(json.contains("unit")&&json["unit"]!="mm")invalid(ui("Unbekannte Einheit im Schaltplan"));
    LibraryPage page;
    if(!json["name"].isString())invalid(ui("Die Bibliotheksseite hat keinen Namen"));
    page.name=json["name"].toString();page.folder=json["folder"].toString();
    if(!json["symbols"].isArray())invalid(ui("Ungültiges Element im Schaltplan"));
    for(const auto &v:json["symbols"].toArray()){
        const auto o=v.toObject();if(!v.isObject()||(o.contains("caption")&&!o["caption"].isString()))invalid(ui("Ungültiges Element im Schaltplan"));
        LibraryEntry e;
        // Version 2: the pictures of a symbol, by the SHA-256 value of their data.
        if(o.contains("resources")){
            if(!o["resources"].isObject())invalid(ui("Ein Bild des Schaltplans fehlt"));
            const auto r=o["resources"].toObject();
            for(auto it=r.begin();it!=r.end();++it){
                const auto x=it.value().toObject();const QString kind=x["kind"].toString();
                const auto data=QByteArray::fromBase64Encoding(x["data"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
                if(!it.value().isObject()||(kind!=u"png"&&kind!=u"jpg"&&kind!=u"bmp")||!data||QCryptographicHash::hash(*data,QCryptographicHash::Sha256).toHex()!=it.key().toLatin1())
                    invalid(ui("Ein Bild des Schaltplans fehlt"));
                e.resources.insert(it.key(),{kind,*data});
            }
        }
        e.symbol=itemFromJson(o["item"],false);e.caption=o["caption"].toString(e.symbol.caption);
        if(e.symbol.type!=ItemType::Component&&e.symbol.type!=ItemType::Group)invalid(ui("Eine Bibliothek enthält nur Bauteile und Gruppen"));
        // Version 3: the children of a parent.
        if(json["version"].toInt()>=3&&o.contains("children")){
            if(!o["children"].isArray())invalid(ui("Ungültiges Element im Schaltplan"));
            for(const auto &c:o["children"].toArray()){Item child=itemFromJson(c,false);if(child.type!=ItemType::Component)invalid(ui("Ungültiges Element im Schaltplan"));e.children<<child;}
        }
        page.entries.append(e);
    }
    return page;
}
LibraryPage loadPage(const QString &file){
    QFile f(file);if(!f.open(QIODevice::ReadOnly))throw FormatError(f.errorString());
    if(f.size()>64*1024*1024)invalid(ui("Datei ist zu groß"));
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(f.readAll(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())invalid(ui("Die Bibliotheksdatei ist beschädigt"));
    LibraryPage page=pageFromJson(doc.object());page.file=file;return page;
}
void savePage(const LibraryPage &page,const QString &file){
    const auto data=QJsonDocument(pageToJson(page)).toJson(QJsonDocument::Indented);
    pageFromJson(QJsonDocument::fromJson(data).object());
    QSaveFile out(file);if(!out.open(QIODevice::WriteOnly))throw FormatError(out.errorString());
    if(out.write(data)!=data.size()||!out.commit())throw FormatError(out.errorString());
}
void sortPages(QList<LibraryPage> &pages){
    // Level by level, folders before pages, both by their names as shown.
    auto key=[](const LibraryPage &p){QStringList k=p.folder.split(u'/',Qt::SkipEmptyParts);k<<localized(p.name);return k;};
    std::stable_sort(pages.begin(),pages.end(),[&](const LibraryPage &a,const LibraryPage &b){
        const QStringList ka=key(a),kb=key(b);
        for(qsizetype i=0;i<std::min(ka.size(),kb.size());i++){
            const bool pageA=i==ka.size()-1,pageB=i==kb.size()-1;
            if(pageA!=pageB)return pageB;
            const int c=QString::compare(ka[i],kb[i],Qt::CaseInsensitive);
            if(c)return c<0;
        }
        return false;
    });
}
QList<LibraryPage> builtInPages(){
    QList<LibraryPage> out;
    for(QDirIterator it(QStringLiteral(":/libraries/schematic"),{"*.json"},QDir::Files);it.hasNext();){
        const QString f=it.next();try{LibraryPage p=loadPage(f);p.builtIn=true;out.append(p);}catch(const FormatError &){}
    }
    sortPages(out);
    if(out.isEmpty()){
        // Without the library files: the example symbol.
        LibraryPage p;p.name=ui("Beispiele");p.builtIn=true;p.entries.append({ui("Beispiel"),exampleSymbol()});out.append(p);
    }
    return out;
}
QList<LibraryPage> folderPages(const QString &folder){
    QList<LibraryPage> out;if(folder.isEmpty()||!QDir(folder).exists())return out;
    QStringList files;
    for(QDirIterator it(folder,{"*.olschlib","*.lib"},QDir::Files,QDirIterator::Subdirectories);it.hasNext();)files<<it.next();
    std::sort(files.begin(),files.end());
    for(const auto &f:files){
        try{
            LibraryPage p;
            if(f.endsWith(u".olschlib",Qt::CaseInsensitive))p=loadPage(f);
            else{
                // A page of a sPlan library.
                QFile in(f);if(!in.open(QIODevice::ReadOnly)||in.size()>64*1024*1024)continue;
                p=splan::readLibrary(in.readAll());p.file=f;
            }
            const QString relative=QDir(folder).relativeFilePath(QFileInfo(f).absolutePath());
            if(p.folder.isEmpty()&&relative!=u".")p.folder=relative;out.append(p);
        }catch(const FormatError &){}
    }
    sortPages(out);
    return out;
}
QString standardLibraryFolder(){
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("OpenLoch/Schaltplan-Bibliothek"));
}
QString libraryFolder(){return QSettings().value("schematic/libraryFolder",standardLibraryFolder()).toString();}
openloch::LibraryFolders libraryFolders(){return {libraryFolder(),QSettings().value("schematic/extraLibraryFolders").toStringList()};}
void setLibraryFolders(const openloch::LibraryFolders &folders){
    QSettings s;
    if(folders.own.isEmpty()||folders.own==standardLibraryFolder())s.remove("schematic/libraryFolder");else s.setValue("schematic/libraryFolder",folders.own);
    if(folders.extra.isEmpty())s.remove("schematic/extraLibraryFolders");else s.setValue("schematic/extraLibraryFolders",folders.extra);
}
QString standardTemplateFolder(){
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("OpenLoch/Schaltplan-Vorlagen"));
}
QString templateFolder(){return QSettings().value("schematic/templateFolder",standardTemplateFolder()).toString();}
QString nextDesignator(const Document &document,const QString &designator){
    static const QRegularExpression pattern(QStringLiteral("^(.*?)(\\d+|\\?)?$"));
    const QString prefix=pattern.match(designator).captured(1);
    if(prefix.isEmpty())return designator;
    int highest=0;
    const QRegularExpression used(QStringLiteral("^")+QRegularExpression::escape(prefix)+QStringLiteral("(\\d+)$"));
    for(const auto &s:document.sheets)for(const auto *c:components(s)){const auto m=used.match(c->designator);if(m.hasMatch())highest=std::max(highest,m.captured(1).toInt());}
    return prefix+QString::number(highest+1);
}
Item placedSymbol(const LibraryEntry &entry,const Document &document){
    QList<Item> one{entry.symbol};freshIds(one);Item item=one[0];item.pos=QPointF();
    if(item.type==ItemType::Component&&item.autoNumber)item.designator=nextDesignator(document,item.designator);
    // A clip of a parent with its children: each parent the next free number, the children follow it.
    if(item.type==ItemType::Group){
        Document taken=document;
        std::function<void(Item&)> number=[&](Item &i){
            if(i.type==ItemType::Component&&i.parent&&i.autoNumber){i.designator=nextDesignator(taken,i.designator);if(!taken.sheets.isEmpty())taken.sheets[0].items<<i;}
            for(auto &c:i.children)number(c);
        };
        number(item);
    }
    if(item.caption.isEmpty())item.caption=entry.caption;
    return item;
}
QList<Item> placedItems(const LibraryEntry &entry,const Document &document){
    QList<Item> out{placedSymbol(entry,document)};
    for(const Item &child:entry.children){QList<Item> one{child};freshIds(one);one[0].parentId=out[0].id;out<<one[0];}
    return out;
}
QImage symbolPicture(const Item &symbol,int size,const QMap<QString,Resource> &resources,double pixelsPerMm){
    QImage image(size,size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QRectF r=bounds(symbol);if(r.isEmpty())r=QRectF(-1,-1,2,2);
    const double fit=std::min((size-6)/r.width(),(size-6)/r.height());
    const double scale=pixelsPerMm>0?std::min(pixelsPerMm,fit):fit;
    Document none=newDocument(QString());none.resources=resources;
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.translate(size/2.,size/2.);p.scale(scale,scale);p.translate(-r.center());
    RenderOptions o;o.paper=false;paintItems(p,{symbol},none,0,o);
    return image;
}

namespace {
// The list of symbols: dragging one starts a drag with its key, clicking chooses it. Dropped on the list itself, on a
// page that can be changed, it moves there.
class SymbolList : public QListWidget {
public:
    using QListWidget::QListWidget;
    std::function<bool()> canMove;
    std::function<void(int,int)> moved;
    std::function<void(QPoint)> hovered;   // the pointer over the viewport; outside it QPoint(-1,-1)
protected:
    bool viewportEvent(QEvent *event) override{
        if(hovered&&event->type()==QEvent::MouseMove)hovered(static_cast<QMouseEvent*>(event)->position().toPoint());
        if(hovered&&event->type()==QEvent::HoverMove)hovered(static_cast<QHoverEvent*>(event)->position().toPoint());
        if(hovered&&event->type()==QEvent::Leave)hovered(QPoint(-1,-1));
        return QListWidget::viewportEvent(event);
    }
    void startDrag(Qt::DropActions) override{
        auto *item=currentItem();if(!item)return;
        auto *mime=new QMimeData;mime->setData("application/x-openloch-schematic-symbol",item->data(Qt::UserRole).toString().toUtf8());
        auto *drag=new QDrag(this);drag->setMimeData(mime);drag->setPixmap(item->icon().pixmap(iconSize()));drag->exec(Qt::CopyAction|Qt::MoveAction);
    }
    void dragEnterEvent(QDragEnterEvent *event) override{
        if(event->source()==this&&canMove&&canMove())event->acceptProposedAction();else event->ignore();
    }
    void dragMoveEvent(QDragMoveEvent *event) override{
        if(event->source()==this&&canMove&&canMove())event->acceptProposedAction();else event->ignore();
    }
    void dropEvent(QDropEvent *event) override{
        if(event->source()!=this||!canMove||!canMove()||!currentItem())return;
        const int from=row(currentItem());QListWidgetItem *target=itemAt(event->position().toPoint());
        const int to=target?row(target):count()-1;
        event->acceptProposedAction();
        if(from!=to&&moved)moved(from,to);
    }
};
// A file name from a page name.
QString fileNameOf(const QString &name){
    QString n=name.section(u'\r',0,0).trimmed();
    static const QRegularExpression bad(QStringLiteral("[\\\\/:*?\"<>|]+"));n.replace(bad,QStringLiteral("_"));
    return n.isEmpty()?QStringLiteral("Seite"):n;
}
// A library entry without ids.
void clearIds(Item &i){i.id.clear();for(auto &c:i.children)clearIds(c);}
QStringList folderParts(const QString &folder){return folder.split(u'/',Qt::SkipEmptyParts);}
// The largest scale at which the pictures of a page are drawn.
constexpr double maxPixelsPerMm=7;
}
LibraryPanel::LibraryPanel(QWidget *parent):QWidget(parent){
    lossesReported=[this](const QStringList &l){
        QMessageBox::information(this,ui("Bibliothek"),ui("Beim Schreiben der sPlan-Bibliotheksseite geht verloren:")+QStringLiteral("\n• ")+l.join(QStringLiteral("\n• ")));};
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(2);
    auto *top=new QHBoxLayout;top->setSpacing(1);layout->addLayout(top);
    pageButton=new QToolButton;pageButton->setObjectName("libraryPage");pageButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    pageButton->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);pageButton->setToolTip(ui("Bibliotheks-Seite"));
    pageButton->setStyleSheet(QStringLiteral("QToolButton{text-align:left;padding:2px 4px}"));
    connect(pageButton,&QToolButton::clicked,this,[this]{openLevel(page>=0?list[page].folder:QString());});
    top->addWidget(pageButton,1);
    treeButton=new QToolButton;treeButton->setObjectName("libraryTree");treeButton->setCheckable(true);treeButton->setAutoRaise(true);
    treeButton->setText(QStringLiteral("☰"));treeButton->setToolTip(ui("Bibliothek als Baumstruktur anzeigen"));
    connect(treeButton,&QToolButton::toggled,this,[this](bool on){showTree(on);});
    top->addWidget(treeButton);
    symbols=new SymbolList;symbols->setObjectName("librarySymbols");symbols->setViewMode(QListView::IconMode);symbols->setResizeMode(QListView::Adjust);
    symbols->setMovement(QListView::Static);symbols->setDragEnabled(true);symbols->setSelectionMode(QAbstractItemView::SingleSelection);symbols->setWordWrap(true);
    symbols->setUniformItemSizes(true);symbols->setSpacing(0);
    // Light cells with the captions in blue, as in sPlan.
    symbols->setStyleSheet(QStringLiteral("QListWidget#librarySymbols::item{background:#eaf0fb;border:1px solid #c9d3e8;color:#1c3fc2}"
                                          "QListWidget#librarySymbols::item:selected{background:#cbdaf6;border:1px solid #6d8fd8;color:#1c3fc2}"));
    layout->addWidget(symbols,1);
    // The tree of pages with the filter field and "Bibliothek neu einlesen" below it, as in sPlan.
    treeBox=new QWidget;treeBox->setObjectName("libraryTreeBox");auto *treeLayout=new QVBoxLayout(treeBox);treeLayout->setContentsMargins(0,0,0,0);treeLayout->setSpacing(2);
    tree=new QTreeWidget;tree->setObjectName("libraryPageTree");tree->setHeaderHidden(true);treeLayout->addWidget(tree,1);
    auto *filterRow=new QHBoxLayout;filterRow->setSpacing(1);treeLayout->addLayout(filterRow);
    filter=new QLineEdit;filter->setObjectName("libraryFilter");filter->setPlaceholderText(ui("Seiten und Bauteile filtern"));filter->setClearButtonEnabled(true);filterRow->addWidget(filter,1);
    reloadButton=new QToolButton;reloadButton->setObjectName("libraryReloadTree");reloadButton->setText(QStringLiteral("⟳"));reloadButton->setAutoRaise(true);
    reloadButton->setToolTip(ui("Bibliothek neu einlesen"));filterRow->addWidget(reloadButton);
    treeBox->hide();layout->addWidget(treeBox,1);
    connect(filter,&QLineEdit::textChanged,this,[this]{fillTree();});
    connect(reloadButton,&QToolButton::clicked,this,[this]{if(reload)reload();});
    connect(tree,&QTreeWidget::itemClicked,this,[this](QTreeWidgetItem *item){
        const QVariant v=item->data(0,Qt::UserRole);if(!v.isValid())return;
        setCurrentPage(v.toInt());treeButton->setChecked(false);
        // An entry the filter found: chosen on its page.
        if(const QVariant e=item->data(0,Qt::UserRole+1);e.isValid())if(auto *x=symbols->item(e.toInt())){symbols->setCurrentItem(x);symbols->scrollToItem(x);}});
    folderLabel=new QLabel;folderLabel->setObjectName("libraryFolder");folderLabel->setMinimumWidth(10);layout->addWidget(folderLabel);
    auto *buttons=new QHBoxLayout;buttons->setSpacing(1);layout->addLayout(buttons);
    auto button=[&](const QString &text,const QString &tip,const QString &name,std::function<void()> run){
        auto *b=new QToolButton;b->setText(text);b->setToolTip(tip);b->setObjectName(name);b->setAutoRaise(true);buttons->addWidget(b);
        connect(b,&QToolButton::clicked,this,std::move(run));return b;
    };
    button(QStringLiteral("−"),ui("Spalte minus"),"columnsMinus",[this]{setColumns(columnCount-1);});
    button(QStringLiteral("+"),ui("Spalte plus"),"columnsPlus",[this]{setColumns(columnCount+1);});
    auto *captionButton=button(QStringLiteral("Abc"),ui("Bauteil-Unterschriften"),"captions",[]{});captionButton->setCheckable(true);captionButton->setChecked(true);
    connect(captionButton,&QToolButton::toggled,this,[this](bool on){setCaptions(on);});
    button(QStringLiteral("▲"),ui("Vorherige Bibliothek"),"previousPage",[this]{setCurrentPage(std::max(0,page-1));});
    button(QStringLiteral("▼"),ui("Nächste Bibliothek"),"nextPage",[this]{setCurrentPage(std::min(int(list.size())-1,page+1));});
    buttons->addStretch();
    button(QStringLiteral("🔍"),ui("Bibliothek durchsuchen"),"librarySearch",[this]{searchDialog();});
    connect(symbols,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){
        LibraryEntry e;if(chosen&&entry(item->data(Qt::UserRole).toString(),&e))chosen(e);});
    symbols->setAcceptDrops(true);symbols->setDropIndicatorShown(true);symbols->viewport()->setMouseTracking(true);
    auto *list=static_cast<SymbolList*>(symbols);
    list->hovered=[this](QPoint at){hoverAt(at);};
    list->canMove=[this]{return writable(page);};
    list->moved=[this](int from,int to){QString error;if(!moveEntry(from,to,&error))QMessageBox::warning(this,ui("Bibliothek"),error);};
    symbols->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(symbols,&QWidget::customContextMenuRequested,this,[this](QPoint at){
        QListWidgetItem *item=symbols->itemAt(at);
        contextMenu(item?symbols->row(item):-1)->popup(symbols->viewport()->mapToGlobal(at));});
    connect(symbols,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *item){editEntry(symbols->row(item),false);});
}
void LibraryPanel::setOwnFolder(const QString &folder,const QString &root){ownFolder=folder;ownRoot=root;}
bool LibraryPanel::writable(int index) const{
    return index>=0&&index<list.size()&&!list[index].builtIn&&!list[index].readOnly&&(list[index].file.endsWith(u".olschlib",Qt::CaseInsensitive)||list[index].file.endsWith(u".lib",Qt::CaseInsensitive));
}
bool LibraryPanel::readOnly(QString *error) const{
    if(writable(page))return false;
    if(error)*error=ui("Diese Bibliotheks-Seite kann nicht geändert werden. Eigene Seiten liegen im Bibliotheksordner; legen Sie dort eine neue Seite an.");
    return true;
}
bool LibraryPanel::store(int index,QString *error){
    try{
        const QString file=list[index].file;
        if(file.endsWith(u".lib",Qt::CaseInsensitive)){
            // A page of a sPlan library, written into its file as read; its entries are read again for their new bytes.
            QFile in(file);const QByteArray original=in.open(QIODevice::ReadOnly)?in.readAll():QByteArray();in.close();
            QStringList losses;const QByteArray bytes=splan::writeLibrary(list[index],original,80,&losses);
            QSaveFile out(file);
            if(!out.open(QIODevice::WriteOnly)||out.write(bytes)!=bytes.size()||!out.commit())throw FormatError(ui("Die Datei kann nicht geschrieben werden: %1").arg(file));
            list[index].entries=splan::readLibrary(bytes).entries;pictures.clear();
            losses.removeDuplicates();if(!losses.isEmpty()&&lossesReported)lossesReported(losses);
            return true;
        }LibraryPage copy=list[index];copy.folder.clear();savePage(copy,list[index].file);pictures.clear();return true;}
    catch(const FormatError &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
void LibraryPanel::refresh(const QString &key){
    sortPages(list);pictures.clear();fillTree();
    page=list.isEmpty()?-1:std::clamp(page,0,int(list.size())-1);
    if(!showPage(key)){showPath();fill();}
}
bool LibraryPanel::newPage(const QString &name,QString *error){
    // Next to the current page if it is an own one, otherwise at the top of the library folder.
    const bool beside=writable(page);
    const QString dir=beside?QFileInfo(list[page].file).absolutePath():ownFolder;
    if(dir.isEmpty()||!QDir().mkpath(dir)){if(error)*error=ui("Das Bibliotheks-Verzeichnis ist nicht vorhanden.");return false;}
    QString file=QDir(dir).filePath(fileNameOf(name)+QStringLiteral(".olschlib"));
    for(int k=2;QFileInfo::exists(file);k++)file=QDir(dir).filePath(fileNameOf(name)+QStringLiteral("-%1.olschlib").arg(k));
    LibraryPage p;p.name=name.trimmed().isEmpty()?ui("Neue Seite"):name.trimmed();p.file=file;
    p.folder=beside?list[page].folder:ownRoot;
    list.append(p);
    if(!store(int(list.size())-1,error)){list.removeLast();return false;}
    refresh(pageKey(int(list.size())-1));
    return true;
}
bool LibraryPanel::newFolder(const QString &name,QString *error){
    const bool beside=writable(page);
    const QString base=beside?QFileInfo(list[page].file).absolutePath():ownFolder,folder=fileNameOf(name);
    if(base.isEmpty()||QFileInfo::exists(QDir(base).filePath(folder))||!QDir(base).mkpath(folder)){if(error)*error=ui("Der neue Unterordner kann nicht erstellt werden.");return false;}
    LibraryPage p;p.name=ui("Neue Seite");p.file=QDir(QDir(base).filePath(folder)).filePath(fileNameOf(p.name)+QStringLiteral(".olschlib"));
    const QString parent=beside?list[page].folder:ownRoot;p.folder=parent.isEmpty()?folder:parent+u'/'+folder;
    list.append(p);
    if(!store(int(list.size())-1,error)){list.removeLast();return false;}
    refresh(pageKey(int(list.size())-1));
    return true;
}
QStringList LibraryPanel::backupFiles(const QString &folder){
    QStringList out;const QDir root(folder);if(folder.isEmpty()||!root.exists())return out;
    for(QDirIterator it(folder,QDir::Files,QDirIterator::Subdirectories);it.hasNext();)out<<root.relativeFilePath(it.next());
    std::sort(out.begin(),out.end());return out;
}
QStringList LibraryPanel::libraryPageFiles(const QString &folder){
    QStringList out;const QDir root(folder);if(folder.isEmpty()||!root.exists())return out;
    for(QDirIterator it(folder,{"*.olschlib","*.lib"},QDir::Files,QDirIterator::Subdirectories);it.hasNext();)out<<root.relativeFilePath(it.next());
    std::sort(out.begin(),out.end());return out;
}
namespace {
QList<ZipEntry> readBackup(const QString &zipFile,QString *error){
    QFile in(zipFile);
    if(!in.open(QIODevice::ReadOnly)||in.size()>1024LL*1024*1024){if(error)*error=ui("Die Datei kann nicht gelesen werden: %1").arg(zipFile);return {};}
    try{return readZip(in.readAll());}catch(const FormatError &e){if(error)*error=QString::fromUtf8(e.what());return {};}
}
}
QStringList LibraryPanel::backupContents(const QString &zipFile,QString *error){
    QString e;const auto entries=readBackup(zipFile,&e);if(!e.isEmpty()){if(error)*error=e;return {};}
    QStringList out;for(const auto &x:entries)out<<x.path;std::sort(out.begin(),out.end());return out;
}
bool LibraryPanel::backup(const QString &zipFile,QString *error,const QString &folder) const{
    const QString source=folder.isEmpty()?ownFolder:folder;
    if(source.isEmpty()||!QDir(source).exists()){if(error)*error=ui("Das Bibliotheks-Verzeichnis ist nicht vorhanden.");return false;}
    QList<ZipEntry> entries;const QDir root(source);
    for(const QString &name:backupFiles(source)){
        const QString f=root.filePath(name);if(QFileInfo(f)==QFileInfo(zipFile))continue;
        QFile in(f);if(!in.open(QIODevice::ReadOnly)){if(error)*error=ui("Die Datei kann nicht gelesen werden: %1").arg(f);return false;}
        entries.append({name,in.readAll(),QFileInfo(f).lastModified()});
    }
    QSaveFile out(zipFile);
    if(!out.open(QIODevice::WriteOnly)||out.write(writeZip(entries))<0||!out.commit()){if(error)*error=ui("Die Datensicherung kann nicht geschrieben werden: %1").arg(zipFile);return false;}
    return true;
}
bool LibraryPanel::restore(const QString &zipFile,QString *error,const QString &folder,bool clearFirst){
    const QString target=folder.isEmpty()?ownFolder:folder;
    if(target.isEmpty()||!QDir().mkpath(target)){if(error)*error=ui("Das Bibliotheks-Verzeichnis ist nicht vorhanden.");return false;}
    QString e;const QList<ZipEntry> entries=readBackup(zipFile,&e);if(!e.isEmpty()){if(error)*error=e;return false;}
    if(clearFirst)for(const QString &name:libraryPageFiles(target))if(!QFile::remove(QDir(target).filePath(name))){if(error)*error=ui("Die Datei kann nicht gelöscht werden: %1").arg(QDir(target).filePath(name));return false;}
    for(const auto &x:entries){
        const QString file=QDir(target).filePath(x.path);QDir().mkpath(QFileInfo(file).absolutePath());
        QSaveFile out(file);
        if(!out.open(QIODevice::WriteOnly)||out.write(x.data)<0||!out.commit()){if(error)*error=ui("Die Datei kann nicht geschrieben werden: %1").arg(file);return false;}
    }
    if(reload&&!ownFolder.isEmpty()&&QFileInfo(target)==QFileInfo(ownFolder))reload();
    return true;
}
LibraryBackupDialog::LibraryBackupDialog(bool restoring_,const QString &folder,const QStringList &files,QWidget *parent):QDialog(parent),restoring(restoring_){
    setObjectName("libraryBackupDialog");setWindowTitle(restoring?ui("Bibliothek-Backup einlesen"):ui("Bibliothek-Backup erstellen"));
    auto *layout=new QVBoxLayout(this);
    layout->addWidget(new QLabel(restoring?ui("Verzeichnis für die Wiederherstellung:"):ui("Verzeichnis:")));
    auto *row=new QHBoxLayout;row->setSpacing(1);layout->addLayout(row);
    folderField=new QLineEdit(QDir::toNativeSeparators(folder));folderField->setObjectName("backupFolder");folderField->setReadOnly(true);row->addWidget(folderField,1);
    chooseFolder=new QPushButton(QStringLiteral("..."));chooseFolder->setObjectName("backupChooseFolder");chooseFolder->setMaximumWidth(32);row->addWidget(chooseFolder);
    layout->addWidget(new QLabel(restoring?ui("Dateien in der Backupdatei:"):ui("Bibliotheksdateien:")));
    fileList=new QPlainTextEdit(files.join(u'\n'));fileList->setObjectName("backupFiles");fileList->setReadOnly(true);fileList->setLineWrapMode(QPlainTextEdit::NoWrap);layout->addWidget(fileList,1);
    clear=new QCheckBox(ui("Alle vorhandenen Bibliotheksdateien im Verzeichnis vor dem Einlesen löschen"));clear->setObjectName("backupClear");clear->setVisible(restoring);layout->addWidget(clear);
    auto *buttons=new QDialogButtonBox;layout->addWidget(buttons);
    ok=buttons->addButton(restoring?ui("Backupdatei einspielen..."):ui("Erstelle Backupdatei..."),QDialogButtonBox::AcceptRole);ok->setObjectName("backupOk");
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    // A backup lists the files of the folder chosen; restoring keeps the list of the backup.
    connect(chooseFolder,&QPushButton::clicked,this,[this]{
        const QString chosen=QFileDialog::getExistingDirectory(this,ui("Verzeichnis:"),this->folder());if(chosen.isEmpty())return;
        folderField->setText(QDir::toNativeSeparators(chosen));if(!restoring)fileList->setPlainText(LibraryPanel::backupFiles(chosen).join(u'\n'));});
    resize(420,520);
}
QString LibraryBackupDialog::folder() const{return QDir::fromNativeSeparators(folderField->text());}
bool LibraryBackupDialog::clearFirst() const{return restoring&&clear->isChecked();}
bool LibraryPanel::copyPage(const QString &name,QString *error){
    if(page<0)return false;
    const QList<LibraryEntry> entries=list[page].entries;
    if(!newPage(name,error))return false;
    list[page].entries=entries;
    if(!store(page,error))return false;
    fill();return true;
}
bool LibraryPanel::renamePage(const QString &name,QString *error){
    if(readOnly(error))return false;
    const QString before=list[page].name;list[page].name=name.trimmed();
    if(!store(page,error)){list[page].name=before;return false;}
    refresh(pageKey(page));return true;
}
bool LibraryPanel::deletePage(QString *error){
    if(readOnly(error))return false;
    if(!QFile::remove(list[page].file)){if(error)*error=ui("Die Datei kann nicht gelöscht werden: %1").arg(QDir::toNativeSeparators(list[page].file));return false;}
    list.removeAt(page);page=std::min(page,int(list.size())-1);
    refresh(pageKey(page));return true;
}
bool LibraryPanel::emptyPage(QString *error){
    if(readOnly(error))return false;
    const auto before=list[page].entries;list[page].entries.clear();
    if(!store(page,error)){list[page].entries=before;return false;}
    fill();return true;
}
bool LibraryPanel::addEntries(const QList<LibraryEntry> &entries,QString *error){
    if(readOnly(error))return false;
    for(auto e:entries){clearIds(e.symbol);for(auto &c:e.children)clearIds(c);list[page].entries.append(e);}
    if(!store(page,error)){list[page].entries.resize(list[page].entries.size()-entries.size());return false;}
    fill();if(symbols->count())symbols->scrollToItem(symbols->item(symbols->count()-1));
    return true;
}
bool LibraryPanel::replaceEntry(int index,const LibraryEntry &entry,QString *error){
    if(readOnly(error)||index<0||index>=list[page].entries.size())return false;
    const LibraryEntry before=list[page].entries[index];list[page].entries[index]=entry;
    if(!store(page,error)){list[page].entries[index]=before;return false;}
    fill();return true;
}
bool LibraryPanel::newEntry(QString *error){
    // As in sPlan: a square of 20 mm as a stand-in, to be drawn in the component editor.
    Item square;square.type=ItemType::Rectangle;square.centre=QPointF(10,-10);square.size=QSizeF(20,20);
    Item designator;designator.type=ItemType::Text;designator.role=TextRole::Designator;designator.pos=QPointF(0,-24);
    Item value;value.type=ItemType::Text;value.role=TextRole::Value;value.pos=QPointF(0,1);
    Item c=makeComponent({square,designator,value},{},QStringLiteral("X?"),QString());
    return addEntries({{ui("Neues Bauteil"),c,{}}},error);
}
bool LibraryPanel::duplicateEntry(int index,QString *error){
    if(page<0||index<0||index>=list[page].entries.size())return false;
    return addEntries({list[page].entries[index]},error);
}
bool LibraryPanel::deleteEntry(int index,QString *error){
    if(readOnly(error)||index<0||index>=list[page].entries.size())return false;
    const LibraryEntry before=list[page].entries[index];list[page].entries.removeAt(index);
    if(!store(page,error)){list[page].entries.insert(index,before);return false;}
    fill();return true;
}
bool LibraryPanel::moveEntry(int from,int to,QString *error){
    if(readOnly(error))return false;
    auto &e=list[page].entries;if(from<0||to<0||from>=e.size()||to>=e.size())return false;
    e.move(from,to);
    if(!store(page,error)){e.move(to,from);return false;}
    fill();symbols->setCurrentRow(to);return true;
}
void LibraryPanel::editEntry(int index,bool all){
    if(page<0||index<0||index>=list[page].entries.size())return;
    const LibraryEntry e=list[page].entries[index];
    if(e.symbol.type!=ItemType::Component)return;
    if(!all&&entryPropertiesRequested){entryPropertiesRequested(index);return;}
    ComponentDialog dialog(e.symbol,e.caption,this);
    if(!writable(page))for(auto *w:dialog.findChildren<QWidget*>())if(qobject_cast<QLineEdit*>(w)||qobject_cast<QCheckBox*>(w))w->setEnabled(false);
    if(dialog.exec()!=QDialog::Accepted||!writable(page))return;
    QString error;
    if(all){
        for(auto &x:list[page].entries)if(x.symbol.type==ItemType::Component)dialog.apply(x.symbol,x.caption,true);
        if(!store(page,&error))QMessageBox::warning(this,ui("Bibliothek"),error);
        fill();
    }else{
        LibraryEntry changed=e;dialog.apply(changed.symbol,changed.caption);
        if(!replaceEntry(index,changed,&error))QMessageBox::warning(this,ui("Bibliothek"),error);
    }
}
QMenu *LibraryPanel::contextMenu(int index){
    delete menu;menu=new QMenu(this);menu->setObjectName("libraryMenu");
    const bool own=writable(page),onEntry=page>=0&&index>=0&&index<list[page].entries.size();
    auto warn=[this](bool ok,const QString &error){if(!ok&&!error.isEmpty())QMessageBox::warning(this,ui("Bibliothek"),error);};
    auto add=[&](QMenu *m,const QString &text,const char *name,bool enabled,std::function<void()> run){
        auto *a=m->addAction(text,this,std::move(run));a->setObjectName(QString::fromLatin1(name));a->setEnabled(enabled);return a;};
    add(menu,ui("Eigenschaften..."),"entryProperties",onEntry,[this,index]{editEntry(index,false);});
    add(menu,ui("Eigenschaften (Alle)..."),"entryPropertiesAll",onEntry&&own,[this,index]{editEntry(index,true);});
    menu->addSeparator();
    add(menu,ui("Neues Bauteil"),"entryNew",own,[this,warn]{QString e;warn(newEntry(&e),e);});
    add(menu,ui("Bauteil duplizieren"),"entryDuplicate",onEntry&&own,[this,index,warn]{QString e;warn(duplicateEntry(index,&e),e);});
    add(menu,ui("Bauteil löschen..."),"entryDelete",onEntry&&own,[this,index,warn]{
        if(QMessageBox::question(this,ui("Bibliothek"),ui("Soll das Bauteil aus der Bibliothek gelöscht werden?"))!=QMessageBox::Yes)return;
        QString e;warn(deleteEntry(index,&e),e);});
    menu->addSeparator();
    QMenu *pages=menu->addMenu(ui("Bibliotheks-Seite"));pages->setObjectName("libraryPageMenu");
    auto askName=[this](const QString &title,const QString &start,QString *out){
        bool ok=false;*out=QInputDialog::getText(this,title,ui("Name:"),QLineEdit::Normal,start,&ok);return ok&&!out->trimmed().isEmpty();};
    add(pages,ui("Neu..."),"pageNew",!ownFolder.isEmpty(),[this,askName,warn]{QString n,e;if(askName(ui("Bibliotheksseite"),ui("Neue Seite"),&n))warn(newPage(n,&e),e);});
    add(pages,ui("Neuer Unterordner..."),"folderNew",!ownFolder.isEmpty(),[this,askName,warn]{QString n,e;if(askName(ui("Neuer Unterordner"),ui("Neuer Ordner"),&n))warn(newFolder(n,&e),e);});
    add(pages,ui("Kopieren..."),"pageCopy",page>=0&&!ownFolder.isEmpty(),[this,askName,warn]{
        QString n,e;if(askName(ui("Bibliotheksseite"),ui("Kopie von")+u' '+localized(list[page].name),&n))warn(copyPage(n,&e),e);});
    add(pages,ui("Umbenennen..."),"pageRename",own,[this,askName,warn]{QString n,e;if(askName(ui("Bibliotheksseite"),localized(list[page].name),&n))warn(renamePage(n,&e),e);});
    add(pages,ui("Löschen..."),"pageDelete",own,[this,warn]{
        if(QMessageBox::question(this,ui("Bibliothek"),ui("Möchten Sie wirklich die komplette Seite aus der Bibliothek löschen?"))!=QMessageBox::Yes)return;
        QString e;warn(deletePage(&e),e);});
    add(pages,ui("Leeren..."),"pageEmpty",own,[this,warn]{
        if(QMessageBox::question(this,ui("Bibliothek"),ui("Sollen ALLE Bauteile aus dieser Bibliothek gelöscht werden?"))!=QMessageBox::Yes)return;
        QString e;warn(emptyPage(&e),e);});
    QMenu *sheets=menu->addMenu(ui("Auf Blatt kopieren"));sheets->setObjectName("librarySheetMenu");
    auto copy=[this](const QList<int> &pagesToCopy){
        if(!copyToSheets||pagesToCopy.isEmpty())return;
        if(QMessageBox::question(this,ui("Auf Blatt kopieren"),ui("%1 neue Blätter werden dem Plan hinzugefügt. Fortfahren?").arg(pagesToCopy.size()))!=QMessageBox::Yes)return;
        copyToSheets(pagesToCopy);};
    add(sheets,ui("Nur diese Bibliotheks-Seite"),"sheetsThis",page>=0,[this,copy]{copy({page});});
    add(sheets,ui("Alle Bibliotheks-Seiten dieser Ebene"),"sheetsLevel",page>=0,[this,copy]{
        QList<int> level;for(int i=0;i<list.size();i++)if(list[i].folder==list[page].folder)level<<i;copy(level);});
    QMenu *extras=menu->addMenu(ui("Extras"));extras->setObjectName("libraryExtras");
    const bool folder=!ownFolder.isEmpty()&&QDir(ownFolder).exists();
    // As in sPlan: the folder and its files first, then the backup file; restoring asks for the backup first, then shows
    // its files and where they go.
    add(extras,ui("Datensicherung erstellen..."),"libraryBackup",folder,[this,warn]{
        LibraryBackupDialog dialog(false,ownFolder,backupFiles(ownFolder),this);if(dialog.exec()!=QDialog::Accepted)return;
        const QString file=QFileDialog::getSaveFileName(this,ui("Erstelle Backupdatei..."),
            QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(ui("Schaltplan-Bibliothek")+QDate::currentDate().toString(QStringLiteral(" yyyy-MM-dd"))+QStringLiteral(".zip")),
            ui("ZIP-Archiv (*.zip)"));
        if(file.isEmpty())return;QString e;
        if(backup(file,&e,dialog.folder()))QMessageBox::information(this,ui("Datensicherung erstellen"),ui("Backupdatei erfolgreich erstellt."));else warn(false,e);});
    add(extras,ui("Datensicherung einspielen..."),"libraryRestore",!ownFolder.isEmpty(),[this,warn]{
        const QString file=QFileDialog::getOpenFileName(this,ui("Backupdatei (*.zip) auswählen..."),QString(),ui("ZIP-Archiv (*.zip)"));
        if(file.isEmpty())return;QString e;const QStringList files=backupContents(file,&e);
        if(!e.isEmpty()){QMessageBox::warning(this,ui("Datensicherung einspielen"),ui("Fehler beim Einlesen der Backupdatei.")+u'\n'+e);return;}
        LibraryBackupDialog dialog(true,ownFolder,files,this);if(dialog.exec()!=QDialog::Accepted)return;
        // Deleting is asked for once more, with the number of pages it takes.
        if(const int n=int(libraryPageFiles(dialog.folder()).size());dialog.clearFirst()&&n>0
           &&QMessageBox::question(this,ui("Datensicherung einspielen"),ui("%1 Bibliotheksdateien im Verzeichnis werden gelöscht. Fortfahren?").arg(n))!=QMessageBox::Yes)return;
        if(restore(file,&e,dialog.folder(),dialog.clearFirst()))QMessageBox::information(this,ui("Datensicherung einspielen"),ui("Backupdatei erfolgreich eingespielt."));else warn(false,e);});
    extras->addSeparator();
    add(extras,ui("Bibliothek neu einlesen"),"libraryReload",bool(reload),[this]{if(reload)reload();});
    add(extras,ui("Explorer..."),"libraryExplorer",page>=0&&(!list[page].file.isEmpty()&&!list[page].builtIn||!ownFolder.isEmpty()),[this]{
        const QString dir=page>=0&&!list[page].builtIn&&!list[page].file.isEmpty()?QFileInfo(list[page].file).absolutePath():ownFolder;
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));});
    return menu;
}
void LibraryPanel::setPages(const QList<LibraryPage> &pages){
    const QString was=page>=0?pageKey(page):QString();
    list=pages;pictures.clear();page=list.isEmpty()?-1:0;
    if(!was.isEmpty())showPage(was);
    fillTree();showPath();fill();
}
void LibraryPanel::setCurrentPage(int index){
    if(index<0||index>=list.size()||index==page)return;
    page=index;showPath();fill();
}
QString LibraryPanel::pageKey(int index) const{
    if(index<0||index>=list.size())return {};
    return list[index].folder+u'|'+list[index].name.section(u'\r',0,0);
}
bool LibraryPanel::showPage(const QString &key){
    for(int i=0;i<list.size();i++)if(pageKey(i)==key){
        if(i!=page){page=i;showPath();fill();}
        return true;
    }
    return false;
}
void LibraryPanel::setColumns(int columns){columnCount=std::clamp(columns,1,8);fill();}
void LibraryPanel::setCaptions(bool on){
    captions=on;
    if(auto *b=findChild<QToolButton*>("captions");b&&b->isChecked()!=on)b->setChecked(on);
    fill();
}
bool LibraryPanel::entry(const QString &key,LibraryEntry *out) const{
    const auto parts=key.split(u'/');if(parts.size()!=2)return false;
    bool ok1,ok2;const int p=parts[0].toInt(&ok1),e=parts[1].toInt(&ok2);
    if(!ok1||!ok2||p<0||p>=list.size()||e<0||e>=list[p].entries.size())return false;
    if(out)*out=list[p].entries[e];
    return true;
}
void LibraryPanel::resizeEvent(QResizeEvent *event){
    QWidget::resizeEvent(event);
    // The pictures follow the width of the panel.
    if(std::abs(width()-filledWidth)>8)fill();
    showPath();
}
void LibraryPanel::showPath(){
    if(page<0){pageButton->setText(QString());folderLabel->clear();return;}
    const auto &p=list[page];
    QStringList path=folderParts(p.folder);path<<localized(p.name);
    const QString full=path.join(QStringLiteral(" > "));
    // Too long, the path loses whole folders at its start, as in sPlan ("... Bauteile > Dioden").
    const int room=std::max(40,pageButton->width()-12);const QFontMetrics fm=pageButton->fontMetrics();
    QString shown=full;
    while(fm.horizontalAdvance(shown)>room&&path.size()>1){path.removeFirst();shown=QStringLiteral("... ")+path.join(QStringLiteral(" > "));}
    pageButton->setText(fm.elidedText(shown,Qt::ElideRight,room));pageButton->setToolTip(full);
    const QString file=p.builtIn?ui("OpenLoch-Bibliothek"):QDir::toNativeSeparators(p.file);
    folderLabel->setText(folderLabel->fontMetrics().elidedText(file,Qt::ElideMiddle,std::max(40,width()-4)));folderLabel->setToolTip(file);
}
QMenu *LibraryPanel::levelMenu(const QString &folder){
    delete menu;menu=new QMenu(this);menu->setObjectName("libraryLevel");
    const QStringList here=folderParts(folder);
    if(!here.isEmpty()){
        const QString up=here.mid(0,here.size()-1).join(u'/');
        menu->addAction(QStringLiteral("<---"),this,[this,up]{QTimer::singleShot(0,this,[this,up]{openLevel(up);});});
    }
    QStringList folders;QList<int> pagesHere;
    for(int i=0;i<list.size();i++){
        const QStringList parts=folderParts(list[i].folder);
        if(parts.size()>here.size()&&parts.mid(0,here.size())==here){if(!folders.contains(parts[here.size()]))folders<<parts[here.size()];}
        else if(parts==here)pagesHere<<i;
    }
    std::sort(folders.begin(),folders.end(),[](const QString &a,const QString &b){return QString::compare(a,b,Qt::CaseInsensitive)<0;});
    for(const auto &f:folders){
        const QString inside=(here+QStringList{f}).join(u'/');
        auto *a=menu->addAction(style()->standardIcon(QStyle::SP_DirIcon),f,this,[this,inside]{QTimer::singleShot(0,this,[this,inside]{openLevel(inside);});});
        a->setData(inside);
    }
    if(!folders.isEmpty()&&!pagesHere.isEmpty())menu->addSeparator();
    for(int i:pagesHere){
        auto *a=menu->addAction(localized(list[i].name),this,[this,i]{setCurrentPage(i);});
        a->setCheckable(true);a->setChecked(i==page);a->setData(i);
    }
    return menu;
}
void LibraryPanel::openLevel(const QString &folder){
    QMenu *m=levelMenu(folder);
    m->popup(pageButton->mapToGlobal(QPoint(0,pageButton->height())));
}
void LibraryPanel::showTree(bool on){
    treeBox->setVisible(on);symbols->setVisible(!on);reloadButton->setEnabled(bool(reload));
    if(treeButton->isChecked()!=on)treeButton->setChecked(on);
    if(on&&page>=0)for(QTreeWidgetItemIterator it(tree);*it;++it)
        if((*it)->data(0,Qt::UserRole)==QVariant(page)&&!(*it)->data(0,Qt::UserRole+1).isValid()){tree->setCurrentItem(*it);tree->scrollToItem(*it);break;}
}
void LibraryPanel::fillTree(){
    tree->clear();QHash<QString,QTreeWidgetItem*> folders;
    // The pages shown: all, or with a filter those whose name or an entry's caption contains it (in any language).
    const QString text=filter?filter->text().trimmed():QString();
    QList<int> shown;QHash<int,QList<int>> found;
    for(int i=0;i<list.size();i++){
        if(text.isEmpty()){shown<<i;continue;}
        for(int e=0;e<list[i].entries.size();e++)if(list[i].entries[e].caption.contains(text,Qt::CaseInsensitive))found[i]<<e;
        if(list[i].name.contains(text,Qt::CaseInsensitive)||found.contains(i))shown<<i;
    }
    std::function<QTreeWidgetItem*(const QStringList&)> node=[&](const QStringList &parts)->QTreeWidgetItem*{
        if(parts.isEmpty())return nullptr;
        const QString key=parts.join(u'/');
        if(auto *n=folders.value(key))return n;
        QTreeWidgetItem *parent=node(parts.mid(0,parts.size()-1));
        auto *n=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(tree);
        n->setText(0,parts.last());n->setIcon(0,style()->standardIcon(QStyle::SP_DirIcon));folders.insert(key,n);return n;
    };
    // Folders first on each level, as in the menu.
    QStringList keys;for(int i:std::as_const(shown))if(!list[i].folder.isEmpty())keys<<list[i].folder;
    std::sort(keys.begin(),keys.end(),[](const QString &a,const QString &b){return QString::compare(a,b,Qt::CaseInsensitive)<0;});
    for(const auto &k:std::as_const(keys))node(folderParts(k));
    for(int i:std::as_const(shown)){
        QTreeWidgetItem *parent=node(folderParts(list[i].folder));
        auto *leaf=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(tree);leaf->setText(0,localized(list[i].name));leaf->setData(0,Qt::UserRole,i);
        for(int e:found.value(i)){auto *hit=new QTreeWidgetItem(leaf);hit->setText(0,localized(list[i].entries[e].caption));hit->setData(0,Qt::UserRole,i);hit->setData(0,Qt::UserRole+1,e);}
    }
    if(!text.isEmpty())tree->expandAll();
}
QList<std::pair<int,int>> LibraryPanel::find(const QString &text) const{
    QList<std::pair<int,int>> out;const QString t=text.trimmed();if(t.isEmpty())return out;
    for(int p=0;p<list.size();p++)for(int e=0;e<list[p].entries.size();e++){
        const auto &x=list[p].entries[e];
        if(x.caption.contains(t,Qt::CaseInsensitive)||x.symbol.designator.contains(t,Qt::CaseInsensitive)||x.symbol.value.contains(t,Qt::CaseInsensitive))out<<std::pair{p,e};
    }
    return out;
}
void LibraryPanel::searchDialog(){
    QDialog dialog(this);dialog.setWindowTitle(ui("Bibliothek durchsuchen"));
    auto *l=new QVBoxLayout(&dialog);auto *field=new QLineEdit;field->setObjectName("searchText");field->setPlaceholderText(ui("Suchbegriff"));l->addWidget(field);
    auto *results=new QListWidget;results->setObjectName("searchResults");l->addWidget(results,1);
    auto *close=new QDialogButtonBox(QDialogButtonBox::Close);l->addWidget(close);
    connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(field,&QLineEdit::textChanged,&dialog,[this,results](const QString &t){
        results->clear();
        for(auto [p,e]:find(t)){
            QStringList path=folderParts(list[p].folder);path<<localized(list[p].name);
            auto *item=new QListWidgetItem(localized(list[p].entries[e].caption)+QStringLiteral("   (")+path.join(QStringLiteral(" > "))+u')');
            item->setData(Qt::UserRole,QStringLiteral("%1/%2").arg(p).arg(e));results->addItem(item);
        }
    });
    connect(results,&QListWidget::itemDoubleClicked,&dialog,[this,&dialog](QListWidgetItem *item){
        const auto parts=item->data(Qt::UserRole).toString().split(u'/');
        treeButton->setChecked(false);setCurrentPage(parts[0].toInt());
        if(auto *x=symbols->item(parts[1].toInt()))symbols->setCurrentItem(x),symbols->scrollToItem(x);
        dialog.accept();
    });
    dialog.resize(420,360);dialog.exec();
}
QRect LibraryPanel::childrenMark(int entry) const{
    if(page<0||page>=list.size()||entry<0||entry>=list[page].entries.size()||list[page].entries[entry].children.isEmpty())return {};
    const QListWidgetItem *item=symbols->item(entry);if(!item)return {};
    // The picture lies centred at the top of the cell; its triangle at the picture's bottom right.
    const QRect cell=symbols->visualItemRect(item);const QSize icon=symbols->iconSize();
    const QRect picture(cell.left()+(cell.width()-icon.width())/2,cell.top()+(symbols->gridSize().height()>icon.height()+20?3:(cell.height()-icon.height())/2),icon.width(),icon.height());
    const int t=std::max(10,icon.width()/5);
    return QRect(picture.right()-t,picture.bottom()-t,t+1,t+1);
}
void LibraryPanel::hoverAt(QPoint at){
    int over=-1;
    if(at.x()>=0&&page>=0&&page<list.size())for(int i=0;i<list[page].entries.size();i++)if(childrenMark(i).adjusted(-3,-3,3,3).contains(at)){over=i;break;}
    if(over<0){if(popup)popup->hide();popupEntry=-1;return;}
    if(over==popupEntry&&popup&&popup->isVisible())return;
    // The children side by side at the scale of the page, beside the cell.
    const LibraryEntry &e=list[page].entries[over];const int size=symbols->iconSize().width();
    QList<QImage> images;int width=0,height=0;
    for(const auto &c:e.children){images<<symbolPicture(c,size*2,e.resources,pageScale*2);width+=images.last().width()+12;height=std::max(height,int(images.last().height()));}
    QImage sheet(std::max(1,width+12),std::max(1,height+24),QImage::Format_ARGB32_Premultiplied);sheet.fill(Qt::white);
    {QPainter painter(&sheet);int x=12;for(const auto &image:images){painter.drawImage(x,12,image);x+=image.width()+12;}
     painter.setPen(QColor(109,143,216));painter.drawRect(sheet.rect().adjusted(0,0,-1,-1));}
    QPixmap picture=QPixmap::fromImage(sheet);picture.setDevicePixelRatio(2);
    if(!popup){popup=new QLabel(this,Qt::ToolTip);popup->setObjectName("libraryChildren");}
    popup->setPixmap(picture);popup->adjustSize();
    const QRect cell=symbols->visualItemRect(symbols->item(over));
    popup->move(symbols->viewport()->mapToGlobal(QPoint(cell.right()+2,cell.top())));popup->show();popupEntry=over;
}
void LibraryPanel::fill(){
    if(popup)popup->hide();popupEntry=-1;
    filledWidth=width();symbols->clear();if(page<0||page>=list.size())return;
    const auto &p=list[page];
    // From the panel's width: the list's own size follows only after this.
    const int available=std::max(60,width()-symbols->verticalScrollBar()->sizeHint().width()-12);const int size=std::max(32,available/columnCount-8);
    symbols->setIconSize(QSize(size,size));symbols->setGridSize(QSize(size+6,size+(captions?34:8)));
    // One scale for the page: the largest at which every symbol fits, at most a few pixels per millimetre.
    double scale=maxPixelsPerMm;
    for(const auto &e:p.entries){QRectF r=bounds(e.symbol);if(r.isEmpty())continue;scale=std::min(scale,(size-6)/std::max(r.width(),r.height()));}
    pageScale=scale;
    for(int i=0;i<p.entries.size();i++){
        const QString key=QStringLiteral("%1/%2/%3/%4").arg(page).arg(i).arg(size).arg(scale);
        QPixmap picture=pictures.value(key);
        if(picture.isNull()){
            QImage image=symbolPicture(p.entries[i].symbol,size*2,p.entries[i].resources,scale*2);
            if(!p.entries[i].children.isEmpty()){
                // A parent with children: a small triangle at the bottom right, as in sPlan.
                const double t=std::max(10.,size/5.)*2;QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(Qt::NoPen);painter.setBrush(QColor(30,90,200));
                painter.drawPolygon(QPolygonF{QPointF(image.width()-2,image.height()-2-t),QPointF(image.width()-2,image.height()-2),QPointF(image.width()-2-t,image.height()-2)});
            }
            picture=QPixmap::fromImage(image);picture.setDevicePixelRatio(2);
            pictures.insert(key,picture);
        }
        const QString caption=localized(p.entries[i].caption);
        auto *item=new QListWidgetItem(QIcon(picture),captions?caption:QString());
        item->setData(Qt::UserRole,QStringLiteral("%1/%2").arg(page).arg(i));item->setToolTip(caption);item->setTextAlignment(Qt::AlignHCenter|Qt::AlignTop);
        symbols->addItem(item);
    }
}
}
