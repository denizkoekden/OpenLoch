#include "startscreen.h"
#include "suite.h"
#include "language.h"
#include <QAbstractButton>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
namespace openloch::suite {
namespace {
// One kind of document as a large button: its symbol, its name and what one makes with it. A kind whose editor is
// still missing says so and cannot be chosen.
class KindTile : public QAbstractButton {
public:
    KindTile(const KindInfo &info,QWidget *parent):QAbstractButton(parent),summary(info.summary){
        setObjectName("new-"+info.id);setText(info.name);setIcon(kindIcon(info.kind));setEnabled(info.available);
        if(!info.available)note=ui("Folgt in einer späteren Version");
        setFocusPolicy(Qt::StrongFocus);setAttribute(Qt::WA_Hover);if(info.available)setCursor(Qt::PointingHandCursor);
        setToolTip(info.available?summary:note);setAccessibleName(info.name);setAccessibleDescription(info.available?summary:summary+". "+note);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    }
    QSize sizeHint() const override{return {180,200};}
    QSize minimumSizeHint() const override{return {150,200};}
protected:
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const QPalette &colours=palette();const bool on=isEnabled(),hot=on&&(underMouse()||hasFocus());
        QColor back=colours.color(QPalette::Base);if(on&&isDown())back=colours.color(QPalette::AlternateBase);
        p.setPen(QPen(hot?colours.color(QPalette::Highlight):colours.color(QPalette::Mid),hot?2:1));p.setBrush(back);
        p.drawRoundedRect(QRectF(rect()).adjusted(1.5,1.5,-1.5,-1.5),8,8);
        const int size=64,top=18;
        p.drawPixmap(QRect((width()-size)/2,top,size,size),icon().pixmap(QSize(size,size),devicePixelRatioF(),on?QIcon::Normal:QIcon::Disabled));
        QFont name=font();name.setBold(true);name.setPointSizeF(name.pointSizeF()*1.2);p.setFont(name);
        p.setPen(colours.color(on?QPalette::Active:QPalette::Disabled,QPalette::Text));
        const int nameTop=top+size+12,nameHeight=QFontMetrics(name).height();
        p.drawText(QRect(8,nameTop,width()-16,nameHeight),Qt::AlignHCenter|Qt::AlignTop,text());
        QFont small=font();small.setPointSizeF(small.pointSizeF()*0.92);p.setFont(small);p.setPen(colours.color(QPalette::PlaceholderText));
        p.drawText(QRect(12,nameTop+nameHeight+4,width()-24,height()-nameTop-nameHeight-10),Qt::AlignHCenter|Qt::AlignTop|Qt::TextWordWrap,on?summary:note);
    }
    void keyPressEvent(QKeyEvent *event) override{
        if(event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter)click();else QAbstractButton::keyPressEvent(event);
    }
private:
    QString summary,note;
};
QLabel *heading(const QString &text){auto *label=new QLabel(text);QFont f=label->font();f.setBold(true);f.setPointSizeF(f.pointSizeF()*1.1);label->setFont(f);return label;}
// The local files of a drag that some editor opens.
QStringList documentFiles(const QMimeData *mime){
    QStringList files;if(!mime)return files;
    for(const QUrl &url:mime->urls())if(url.isLocalFile()&&documentKind(url.toLocalFile()))files<<url.toLocalFile();
    return files;
}
}

StartScreen::StartScreen(Suite *s):suite(s){
    setObjectName("startScreen");setWindowTitle("OpenLoch");setAcceptDrops(true);
    auto *outer=new QVBoxLayout(this);outer->setContentsMargins(28,24,28,24);outer->setSpacing(10);
    auto *header=new QHBoxLayout;header->setSpacing(16);
    auto *logo=new QLabel;logo->setPixmap(QIcon(":/openloch.png").pixmap(64,64));header->addWidget(logo,0,Qt::AlignTop);
    auto *titles=new QVBoxLayout;titles->setSpacing(2);
    auto *title=new QLabel("OpenLoch");QFont big=title->font();big.setPointSizeF(big.pointSizeF()*2);big.setBold(true);title->setFont(big);titles->addWidget(title);
    auto *subtitle=new QLabel(ui("Elektronik vom Schaltplan bis zur Frontplatte"));subtitle->setForegroundRole(QPalette::PlaceholderText);titles->addWidget(subtitle);
    header->addLayout(titles,1);outer->addLayout(header);outer->addSpacing(14);
    outer->addWidget(heading(ui("Neues Dokument")));
    auto *tiles=new QHBoxLayout;tiles->setSpacing(12);
    for(const auto &info:documentKinds()){
        auto *tile=new KindTile(info,this);tiles->addWidget(tile);
        connect(tile,&QAbstractButton::clicked,this,[this,kind=info.kind]{suite->create(kind);});
    }
    outer->addLayout(tiles);outer->addSpacing(16);
    auto *recentRow=new QHBoxLayout;recentRow->addWidget(heading(ui("Zuletzt verwendet")));recentRow->addStretch();
    auto *openButton=new QPushButton(ui("Öffnen…"));openButton->setObjectName("openDocument");recentRow->addWidget(openButton);
    connect(openButton,&QPushButton::clicked,this,[this]{suite->openDialog(this);});
    outer->addLayout(recentRow);
    recent=new QTreeWidget;recent->setObjectName("recentDocuments");recent->setHeaderLabels({ui("Dokument"),ui("Art"),ui("Ordner")});
    recent->setRootIsDecorated(false);recent->setUniformRowHeights(true);recent->setAllColumnsShowFocus(true);recent->setIconSize(QSize(32,32));
    recent->setContextMenuPolicy(Qt::CustomContextMenu);recent->header()->setStretchLastSection(true);
    connect(recent,&QTreeWidget::itemActivated,this,[this](QTreeWidgetItem *item){openRecent(item->data(0,Qt::UserRole).toString());});
    connect(recent,&QWidget::customContextMenuRequested,this,[this](QPoint at){
        auto *item=recent->itemAt(at);if(!item)return;const QString path=item->data(0,Qt::UserRole).toString();QMenu menu(this);
        connect(menu.addAction(ui("Öffnen")),&QAction::triggered,this,[this,path]{openRecent(path);});
        connect(menu.addAction(ui("Aus der Liste entfernen")),&QAction::triggered,this,[this,path]{suite->forget(path);});
        menu.addSeparator();connect(menu.addAction(ui("Liste leeren")),&QAction::triggered,this,[this]{suite->clearRecent();});
        menu.exec(recent->viewport()->mapToGlobal(at));
    });
    auto *empty=new QLabel(ui("Hier erscheinen die zuletzt geöffneten und gespeicherten Dokumente."));empty->setObjectName("noRecentDocuments");
    empty->setAlignment(Qt::AlignCenter);empty->setWordWrap(true);empty->setForegroundRole(QPalette::PlaceholderText);
    recentPages=new QStackedWidget;recentPages->addWidget(recent);recentPages->addWidget(empty);outer->addWidget(recentPages,1);
    resize(860,640);refresh();
}
void StartScreen::refresh(){
    recent->clear();
    for(const QString &file:suite->recentDocuments()){
        const QFileInfo info(file);const auto kind=documentKind(file);
        auto *item=new QTreeWidgetItem(recent,{info.fileName(),kind?kindInfo(*kind).name:QString(),QDir::toNativeSeparators(info.absolutePath())});
        item->setData(0,Qt::UserRole,file);item->setToolTip(0,QDir::toNativeSeparators(file));if(kind)item->setIcon(0,kindIcon(*kind));
        if(!info.isFile()){
            item->setToolTip(0,ui("Nicht gefunden: %1").arg(QDir::toNativeSeparators(file)));
            for(int column=0;column<3;column++)item->setForeground(column,palette().brush(QPalette::Disabled,QPalette::Text));
        }
    }
    for(int column=0;column<2;column++)recent->resizeColumnToContents(column);
    recentPages->setCurrentIndex(recent->topLevelItemCount()?0:1);
}
void StartScreen::openRecent(const QString &path){
    if(!QFileInfo(path).isFile()){if(askToForget(path))suite->forget(path);return;}
    suite->open(path);
}
bool StartScreen::askToForget(const QString &path){
    QMessageBox box(QMessageBox::Question,ui("Zuletzt verwendet"),ui("„%1“ wurde nicht gefunden. Aus der Liste entfernen?").arg(QDir::toNativeSeparators(path)),QMessageBox::NoButton,this);
    auto *remove=box.addButton(ui("Entfernen"),QMessageBox::AcceptRole);box.addButton(ui("Abbrechen"),QMessageBox::RejectRole);box.exec();
    return box.clickedButton()==remove;
}
void StartScreen::closeEvent(QCloseEvent *event){event->accept();if(closed)closed();}
void StartScreen::dragEnterEvent(QDragEnterEvent *event){if(!documentFiles(event->mimeData()).isEmpty())event->acceptProposedAction();}
void StartScreen::dropEvent(QDropEvent *event){
    const QStringList files=documentFiles(event->mimeData());if(files.isEmpty())return;
    event->acceptProposedAction();for(const QString &file:files)suite->open(file);
}
}
