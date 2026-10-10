#include "macropanel.h"
#include "draw.h"
#include "language.h"
#include "legacy_reader.h"
#include "view.h"
#include "formats/sprint/sprint.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDrag>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QLineEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <cmath>

namespace openloch::pcb {
namespace {
int otherSide(int layer){
    switch(layer){case CopperTop:return CopperBottom;case CopperBottom:return CopperTop;case SilkTop:return SilkBottom;case SilkBottom:return SilkTop;
        case Inner1:return Inner2;case Inner2:return Inner1;default:return layer;}
}
}

// The macro as it would be placed, scaled to the widget on the board's black; pressing and moving drags it out.
class MacroPreview : public QWidget {
public:
    QList<Element> elements;
    std::function<void()> doubleClicked;
    MacroPreview(){setMinimumHeight(150);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setObjectName("macroPreview");setCursor(Qt::OpenHandCursor);}
protected:
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);const Colours colours=Colours::standard();p.fillRect(rect(),colours.board);if(elements.isEmpty())return;
        QRectF box;for(const auto &e:elements)box=box.united(bounds(e));if(box.isEmpty())return;
        const double scale=std::min((width()-16)/std::max(box.width(),1e-3),(height()-16)/std::max(box.height(),1e-3));
        p.setRenderHint(QPainter::Antialiasing);p.translate(width()/2.0,height()/2.0);p.scale(scale,scale);p.translate(-box.center());
        for(int layer:drawingOrder(CopperTop))for(const auto &e:elements)if(e.layer==layer&&!e.cutout)paintElement(p,e,colours.layers[layer],0,e.via?colours.via:QColor());
        p.setPen(Qt::NoPen);p.setBrush(colours.board);for(const auto &e:elements)if(e.type==ElementType::Pad&&e.size2>0)p.drawEllipse(e.pos,e.size2/2,e.size2/2);
    }
    void mousePressEvent(QMouseEvent *event) override{pressAt=event->position();}
    void mouseMoveEvent(QMouseEvent *event) override{
        if(!(event->buttons()&Qt::LeftButton)||elements.isEmpty()||QLineF(event->position(),pressAt).length()<6)return;
        auto *drag=new QDrag(this);auto *data=new QMimeData;data->setData(MacroPanel::dragFormat(),QByteArray("macro"));drag->setMimeData(data);drag->exec(Qt::CopyAction);
    }
    void mouseDoubleClickEvent(QMouseEvent *) override{if(doubleClicked)doubleClicked();}
private:
    QPointF pressAt;
};

QString MacroPanel::dragFormat(){return QStringLiteral("application/x-openloch-pcb-macro");}
MacroPanel::MacroPanel(QWidget *parent):QWidget(parent){
    auto *v=new QVBoxLayout(this);
    auto *top=new QHBoxLayout;folderLabel=new QLabel;folderLabel->setWordWrap(true);folderLabel->setObjectName("macroFolderName");
    auto *choose=new QToolButton;choose->setText(ui("Ordner…"));choose->setObjectName("chooseMacroFolder");top->addWidget(folderLabel,1);top->addWidget(choose);v->addLayout(top);
    libraryBox=new QComboBox;libraryBox->setObjectName("macroLibraries");libraryBox->setVisible(false);v->addWidget(libraryBox);
    model=new QFileSystemModel(this);model->setNameFilters({QStringLiteral("*.lmk"),QStringLiteral("*.LMK")});model->setNameFilterDisables(false);
    model->setFilter(QDir::AllDirs|QDir::Files|QDir::NoDotAndDotDot);
    tree=new QTreeView;tree->setObjectName("macroTree");tree->setModel(model);tree->setHeaderHidden(true);for(int c=1;c<4;c++)tree->setColumnHidden(c,true);v->addWidget(tree,1);
    nameLabel=new QLabel;nameLabel->setObjectName("macroName");v->addWidget(nameLabel);
    // Two lines describing the macro, kept in its file.
    for(int k=0;k<2;k++){auto *line=new QLineEdit;line->setObjectName(QStringLiteral("macroText%1").arg(k+1));line->setMaxLength(50);
        line->setPlaceholderText(k?ui("Beschreibung, zweite Zeile"):ui("Beschreibung, erste Zeile"));lineEdits[k]=line;v->addWidget(line);
        connect(line,&QLineEdit::editingFinished,this,[this]{
            if(file.isEmpty()||!lineEdits[0]->isEnabled()||QStringList{lineEdits[0]->text(),lineEdits[1]->text()}==lines)return;
            // What could not be written goes back to what the file holds.
            if(!setDescription(lineEdits[0]->text(),lineEdits[1]->text())){refresh();QMessageBox::warning(this,ui("Makro"),ui("Die Beschreibung kann nicht in „%1“ geschrieben werden.").arg(QFileInfo(file).fileName()));}});}
    preview=new MacroPreview;v->addWidget(preview);
    // The options above the preview in the reference: side, through-plated pads, turning; then as a component.
    auto *options=new QHBoxLayout;sideButton=new QToolButton;sideButton->setCheckable(true);sideButton->setObjectName("macroBottom");
    viaButton=new QToolButton;viaButton->setCheckable(true);viaButton->setText(ui("DK"));viaButton->setToolTip(ui("Alle Lötaugen durchkontaktieren"));viaButton->setObjectName("macroVias");
    auto *turn=new QToolButton;turn->setText(QStringLiteral("↻"));turn->setToolTip(ui("Um 90° im Uhrzeigersinn drehen"));turn->setObjectName("macroTurn");
    removeButton=new QPushButton(ui("Löschen…"));removeButton->setObjectName("macroRemove");
    options->addWidget(sideButton);options->addWidget(viaButton);options->addWidget(turn);options->addStretch();options->addWidget(removeButton);v->addLayout(options);
    componentBox=new QCheckBox(ui("Als Bauteil einfügen"));componentBox->setObjectName("macroComponent");v->addWidget(componentBox);
    auto *buttons=new QHBoxLayout;placeButton=new QPushButton(ui("Platzieren"));placeButton->setObjectName("macroPlace");
    auto *save=new QPushButton(ui("Auswahl speichern…"));save->setObjectName("macroSave");save->setToolTip(ui("Die markierten Elemente als Makro in diesen Ordner speichern"));
    buttons->addWidget(placeButton);buttons->addWidget(save);v->addLayout(buttons);
    connect(choose,&QToolButton::clicked,this,[this]{if(chooseFolder)chooseFolder();});
    connect(libraryBox,&QComboBox::currentIndexChanged,this,[this](int i){if(i>=0)showFolder(libraryBox->itemData(i).toString());});
    connect(tree->selectionModel(),&QItemSelectionModel::currentChanged,this,[this](const QModelIndex &index){if(!model->isDir(index))pick(model->filePath(index));});
    connect(tree,&QTreeView::doubleClicked,this,[this](const QModelIndex &index){if(!model->isDir(index)&&pick(model->filePath(index))&&place)place(macro());});
    preview->doubleClicked=[this]{if(!elements.isEmpty()&&place)place(macro());};
    connect(placeButton,&QPushButton::clicked,this,[this]{if(!elements.isEmpty()&&place)place(macro());});
    connect(save,&QPushButton::clicked,this,[this]{if(saveSelection)saveSelection();});
    connect(sideButton,&QToolButton::toggled,this,[this](bool on){bottom=on;refresh();});
    connect(viaButton,&QToolButton::toggled,this,[this](bool on){vias=on;refresh();});
    connect(turn,&QToolButton::clicked,this,[this]{turns=(turns+1)%4;refresh();});
    connect(componentBox,&QCheckBox::toggled,this,[this](bool on){asComponent=on;});
    connect(removeButton,&QPushButton::clicked,this,[this]{
        if(file.isEmpty())return;
        if(QMessageBox::question(this,ui("Makro löschen"),ui("Das Makro „%1“ löschen?").arg(QFileInfo(file).fileName()))==QMessageBox::Yes)removeMacro(file);});
    setFolder({});
}
void MacroPanel::setFolder(const QString &folder){setFolders(folder,{});}
void MacroPanel::setFolders(const QString &ownFolder,const QStringList &extra){
    own=ownFolder;extras.clear();for(const auto &f:extra)if(!f.isEmpty()&&f!=own&&!extras.contains(f))extras<<f;
    {const QSignalBlocker quiet(libraryBox);libraryBox->clear();libraryBox->addItem(ui("Eigene Makros"),own);
        for(const auto &f:extras){libraryBox->addItem(ui("%1 (nur lesen)").arg(QFileInfo(f).fileName()),f);libraryBox->setItemData(libraryBox->count()-1,QDir::toNativeSeparators(f),Qt::ToolTipRole);}
        libraryBox->setCurrentIndex(0);libraryBox->setVisible(!extras.isEmpty());}
    showFolder(own);
}
void MacroPanel::showFolder(const QString &folder){
    root=folder;file.clear();elements.clear();trouble.clear();lines.clear();
    // A folder that does not exist yet (the default one before the first macro) shows nothing rather than the whole disk.
    if(root.isEmpty()){folderLabel->setText(ui("Noch kein Makroordner gewählt."));tree->setRootIndex(QModelIndex());tree->setEnabled(false);}
    else if(!QFileInfo(root).isDir()){folderLabel->setText(ui("%1 (noch leer)").arg(QDir::toNativeSeparators(root)));tree->setEnabled(false);tree->setRootIndex(model->setRootPath(QString()));tree->collapseAll();}
    else{folderLabel->setText(QDir::toNativeSeparators(root));tree->setEnabled(true);tree->setRootIndex(model->setRootPath(root));}
    refresh();
}
bool MacroPanel::pick(const QString &path){
    file=path;elements.clear();trouble.clear();
    try{
        QFile f(path);if(!f.open(QIODevice::ReadOnly))throw FormatError(f.errorString());if(f.size()>64*1024*1024)throw FormatError(ui("Datei ist zu groß"));
        const QByteArray bytes=f.readAll();elements=sprint::readMacro(bytes);lines=sprint::readMacroText(bytes);
    }catch(const std::exception &e){trouble=QString::fromUtf8(e.what());lines.clear();}
    refresh();return !elements.isEmpty();
}
QList<Element> MacroPanel::macro() const{
    // Through-plated pads (plain holes have no copper to plate), the other side: mirrored, the layers of top and bottom
    // swapped; then quarter turns clockwise about the macro's own origin as the preview shows it.
    auto els=elements;
    for(auto &e:els){if(vias&&e.type==ElementType::Pad&&e.size2<e.size)e.via=true;if(bottom){mirror(e,0);e.layer=otherSide(e.layer);}if(turns%4)rotate(e,{},-90.0*(turns%4));}
    return els;
}
void MacroPanel::setOptions(bool bottomSide,bool throughPlated,int quarterTurns,bool component){
    bottom=bottomSide;vias=throughPlated;turns=((quarterTurns%4)+4)%4;asComponent=component;
    const QSignalBlocker a(sideButton),b(viaButton),c(componentBox);sideButton->setChecked(bottom);viaButton->setChecked(vias);componentBox->setChecked(asComponent);refresh();
}
QString MacroPanel::currentFolder() const{
    if(readOnly())return own;
    if(!file.isEmpty())return QFileInfo(file).absolutePath();
    const QModelIndex index=tree->currentIndex();if(index.isValid()&&model->isDir(index))return model->filePath(index);
    return root;
}
bool MacroPanel::setDescription(const QString &first,const QString &second){
    if(file.isEmpty()||readOnly()||lines.size()!=2)return false;
    QFile in(file);if(!in.open(QIODevice::ReadOnly))return false;const QByteArray bytes=in.readAll();in.close();
    const QByteArray changed=sprint::withMacroText(bytes,first.left(50),second.left(50));if(changed==bytes){lines={first.left(50),second.left(50)};return true;}
    QSaveFile out(file);if(!out.open(QIODevice::WriteOnly)||out.write(changed)!=changed.size()||!out.commit())return false;
    lines=sprint::readMacroText(changed);refresh();return true;
}
bool MacroPanel::removeMacro(const QString &path){
    if(readOnly()||!QFile::remove(path))return false;if(path==file){file.clear();elements.clear();trouble.clear();lines.clear();}refresh();return true;
}
void MacroPanel::refresh(){
    sideButton->setText(bottom?ui("unten"):ui("oben"));sideButton->setToolTip(bottom?ui("Bestückt von unten (gespiegelt)"):ui("Bestückt von oben"));
    preview->elements=macro();preview->update();placeButton->setEnabled(!elements.isEmpty());removeButton->setEnabled(!file.isEmpty()&&!readOnly());
    nameLabel->setText(!trouble.isEmpty()?trouble:file.isEmpty()?QString():QFileInfo(file).completeBaseName());
    const bool writable=!file.isEmpty()&&lines.size()==2&&!readOnly()&&QFileInfo(file).isWritable();
    for(int k=0;k<2;k++){const QSignalBlocker quiet(lineEdits[k]);lineEdits[k]->setText(lines.value(k));lineEdits[k]->setEnabled(writable);}
}
}
