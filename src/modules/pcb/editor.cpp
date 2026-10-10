#include "editor.h"
#include "font.h"
#include "footprints.h"
#include "macropanel.h"
#include "language.h"
#include "outputs.h"
#include "overview.h"
#include "legacy_reader.h"
#include "pcbicons.h"
#include "pictures.h"
#include "progress.h"
#include "shapes.h"
#include "formats/sprint/sprint.h"
#include "formats/sprint/textio.h"
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPageSetupDialog>
#include <QPainter>
#include <QPainterPath>
#include <QPrinter>
#include <QProcess>
#include <QProgressDialog>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSaveFile>
#include <QSettings>
#include <QSet>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <optional>
#include <algorithm>
#include <cmath>

namespace openloch::pcb {
namespace {
constexpr const char *clipboardMime="application/x-openloch-pcb";
QString preferencesPath;
// The preferences live with the user, under the module's own name, or in the INI file set for them.
QSettings preferences(){
    if(!preferencesPath.isEmpty())return QSettings(preferencesPath,QSettings::IniFormat);
    return QSettings(QStringLiteral("OpenLoch"),QStringLiteral("Leiterplatte"));
}
// A colour scheme as stored in the preferences: board, grid lines, the seven layers, through-plated pads, airwires,
// grid dots (schemes stored before the dots keep the standard colour for them).
QStringList colourNames(const Colours &c){
    QStringList out{c.board.name(),c.grid.name()};for(int l=1;l<=layerCount;l++)out<<c.layers[l].name();out<<c.via.name()<<c.airwire.name()<<c.dots.name();return out;
}
Colours coloursFrom(const QStringList &names){
    Colours c=Colours::standard();if(names.size()!=11&&names.size()!=12)return c;
    auto take=[&](int k,QColor &into){const QColor v(names[k]);if(v.isValid())into=v;};take(0,c.board);take(1,c.grid);for(int l=1;l<=layerCount;l++)take(1+l,c.layers[l]);take(9,c.via);take(10,c.airwire);
    if(names.size()==12)take(11,c.dots);
    return c;
}
// A current in ampere with three digits, as the reference shows it.
QString currentText(double a){return ui("≈ %1 A").arg(uiLocale().toString(a,'f',2));}
// Track widths for the favourites: to the hundredth of a millimetre the width field shows, sorted, each once.
QList<double> sortedWidths(QList<double> widths){
    QList<double> out;for(double w:widths)if(std::isfinite(w)&&w>=0&&w<=50)out.append(std::round(w*100)/100);
    std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());return out;
}
// Pad and SMD sizes for the favourites: to the hundredth, sorted, each once.
QList<QPointF> sortedSizes(QList<QPointF> sizes){
    QList<QPointF> out;for(auto p:sizes)if(std::isfinite(p.x())&&std::isfinite(p.y())&&p.x()>0&&p.x()<=50&&p.y()>=0&&p.y()<=50)out.append({std::round(p.x()*100)/100,std::round(p.y()*100)/100});
    std::sort(out.begin(),out.end(),[](QPointF a,QPointF b){return a.x()<b.x()||(a.x()==b.x()&&a.y()<b.y());});
    out.erase(std::unique(out.begin(),out.end()),out.end());return out;
}
QString millimetres(double v){return uiLocale().toString(v,'f',3);}
// An angle in degrees from 0 up to 360.
double wrapped(double degrees){const double a=std::fmod(degrees,360.0);return a<0?a+360:a;}
// The pad forms in the order of the reference's menu.
QStringList shapeNames(){
    return {ui("rund"),ui("achteckig"),ui("quadratisch"),ui("quer, abgerundet"),ui("quer, achteckig"),ui("quer, rechteckig"),
            ui("hoch, abgerundet"),ui("hoch, achteckig"),ui("hoch, rechteckig")};
}
QString typeName(ElementType type){
    switch(type){case ElementType::Pad:return ui("Lötauge");case ElementType::SmdPad:return ui("SMD-Pad");case ElementType::Track:return ui("Leiterbahn");
        case ElementType::Area:return ui("Fläche");case ElementType::Circle:return ui("Kreisring");case ElementType::Text:return ui("Text");}
    return {};
}
QRectF selectionBounds(const Board &b,const QList<int> &indexes){QRectF r;for(int i:indexes)r=r.united(bounds(b.elements[i]));return r;}
// The selected elements as a list of their own, airwires kept between them; an autorouted track keeps its pads only when
// they are selected too.
QList<Element> extract(const Board &b,const QList<int> &indexes){
    QList<Element> out;QMap<int,int> map;for(int k=0;k<indexes.size();k++)map[indexes[k]]=k;
    for(int i:indexes){Element e=b.elements[i];QList<int> c;for(int t:e.connections)if(map.contains(t))c.append(map[t]);e.connections=c;
        for(int &p:e.autoroutePads)p=map.value(p,-1);out.append(e);}
    return out;
}
// The selection as the parts that move as one: elements joined by outermost groups or components, single elements.
QList<QList<int>> parts(const Board &b,const QList<int> &indexes){
    QList<int> root(indexes.size());for(int k=0;k<root.size();k++)root[k]=k;
    auto find=[&](int k){while(root[k]!=k)k=root[k]=root[root[k]];return k;};
    QMap<int,int> group,part;
    for(int k=0;k<indexes.size();k++){
        const auto &e=b.elements[indexes[k]];
        if(!e.groups.isEmpty()){if(group.contains(e.groups.last()))root[find(k)]=find(group[e.groups.last()]);else group[e.groups.last()]=k;}
        if(e.part){if(part.contains(e.part))root[find(k)]=find(part[e.part]);else part[e.part]=k;}
    }
    QList<QList<int>> out;QMap<int,int> piece;
    for(int k=0;k<indexes.size();k++){const int r=find(k);if(!piece.contains(r)){piece[r]=int(out.size());out.append(QList<int>{});}out[piece[r]].append(indexes[k]);}
    return out;
}
// On some systems Ctrl+H and Ctrl+W belong to the system (hide, close); there the reference's keys use the Control key.
QKeySequence systemSafe(Qt::Key key){
#ifdef Q_OS_MACOS
    return QKeySequence(Qt::META|key);
#else
    return QKeySequence(Qt::CTRL|key);
#endif
}
// Where copy number k (1, 2, …) of the selection goes: tiles `step` apart in rows of `columns`, or around a circle.
std::function<void(QList<Element>&,int)> tilePlacement(int columns,QPointF step){
    return [columns,step](QList<Element> &els,int k){const QPointF d(step.x()*(k%columns),step.y()*(k/columns));for(auto &e:els)pcb::move(e,d);};
}
// As in the reference, the reference point starts at the top of the circle (its centre lies `radius` below) and a
// positive angle runs clockwise on the screen.
std::function<void(QList<Element>&,int)> circlePlacement(QPointF reference,double angle,double radius,bool turn){
    const QPointF centre=reference+QPointF(0,radius);
    return [=](QList<Element> &els,int k){
        if(turn){for(auto &e:els)rotate(e,centre,-angle*k);return;}
        const double a=qDegreesToRadians(angle*k);const QPointF to=centre+QPointF(radius*std::sin(a),-radius*std::cos(a));for(auto &e:els)pcb::move(e,to-reference);
    };
}
// A small picture of the selection and its copies for the arrange dialog.
class ArrangePreview : public QWidget {
public:
    QList<Element> original;int copies=0;std::function<void(QList<Element>&,int)> place;
    ArrangePreview(){setMinimumSize(260,200);}
protected:
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);p.fillRect(rect(),Qt::black);if(original.isEmpty()||!place)return;
        QList<QList<Element>> all{original};for(int k=1;k<=copies&&k<=2000;k++){auto c=original;place(c,k);all.append(c);}
        QRectF box;for(const auto &c:all)for(const auto &e:c)box=box.united(bounds(e));if(box.isEmpty())return;
        const double scale=std::min((width()-16)/std::max(box.width(),1e-3),(height()-16)/std::max(box.height(),1e-3));
        p.setRenderHint(QPainter::Antialiasing);p.translate(width()/2.0,height()/2.0);p.scale(scale,scale);p.translate(-box.center());p.setPen(Qt::NoPen);
        for(int k=0;k<all.size();k++)for(const auto &e:all[k]){
            QColor c=e.layer==CopperTop?QColor(29,106,249):e.layer==CopperBottom?QColor(0,186,1):e.layer==SilkTop?QColor(255,0,0):e.layer==SilkBottom?QColor(225,215,4):QColor(200,200,200);
            if(k==0)c=c.lighter(150);p.setBrush(c);p.drawPath(copperShape(e));
        }
    }
};
int otherLayer(int layer){
    switch(layer){case CopperTop:return CopperBottom;case CopperBottom:return CopperTop;case SilkTop:return SilkBottom;case SilkBottom:return SilkTop;
        case Inner1:return Inner2;case Inner2:return Inner1;default:return layer;}
}
QDoubleSpinBox *spin(double value,double low,double high,int decimals,const QString &suffix=QStringLiteral(" mm")){
    auto *box=new QDoubleSpinBox;box->setRange(low,high);box->setDecimals(decimals);box->setSuffix(suffix);box->setValue(value);box->setKeyboardTracking(false);
    box->setLocale(uiLocale());return box;
}
}

Editor::Editor(QWidget *parent):QWidget(parent){
    board=new BoardView;createActions();
    auto *centre=new QWidget;auto *column=new QVBoxLayout(centre);column->setContentsMargins(0,0,0,0);column->setSpacing(0);
    tabs=new QTabBar;tabs->setShape(QTabBar::RoundedSouth);tabs->setExpanding(false);tabs->setContextMenuPolicy(Qt::CustomContextMenu);
    column->addWidget(createAutorouteBar());column->addWidget(createPhotoBar());column->addWidget(board,1);column->addWidget(tabs);column->addWidget(createLayerBar());
    // What opening an older Sprint-Layout file converted or left out (openNotes), above the board until it is closed.
    noticeBar=new QFrame;noticeBar->setObjectName("noticeBar");noticeBar->setStyleSheet(QStringLiteral("#noticeBar{background:#fff4c2} #noticeBar QLabel{color:black}"));
    auto *notice=new QHBoxLayout(noticeBar);notice->setContentsMargins(8,3,3,3);noticeText=new QLabel;noticeText->setWordWrap(true);notice->addWidget(noticeText,1);
    auto *noticeClose=new QToolButton;noticeClose->setText(QStringLiteral("×"));noticeClose->setAutoRaise(true);notice->addWidget(noticeClose,0,Qt::AlignTop);
    connect(noticeClose,&QToolButton::clicked,noticeBar,&QWidget::hide);noticeBar->hide();column->insertWidget(1,noticeBar);
    auto *layout=new QHBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
    layout->addWidget(createToolPanel());layout->addWidget(centre,1);layout->addWidget(createSidePanel());

    board->beforeChange=[this]{snapshot();};
    board->changed=[this]{touched();};
    board->selectionChanged=[this]{refreshProperties();refreshActions();if(!refreshing)showSelectedComponent();};
    board->pointerMoved=[this](QPointF mm){
        // From the origin in the rulers' unit, as the reference shows it: x with its sign, y as the distance.
        const QPointF o=doc.board().origin;const QString unit=board->milUnits?QStringLiteral("mil"):QStringLiteral("mm");
        coordinates->setText(ui("X: %1 %3   Y: %2 %3").arg(millimetres(board->inUnits(mm.x()-o.x())),millimetres(board->inUnits(std::abs(o.y()-mm.y()))),unit));
    };
    // The unit switched in the rulers' corner: the coordinates follow, the preferences keep it.
    board->unitsChanged=[this]{preferences().setValue("milUnits",board->milUnits);if(board->pointerMoved)board->pointerMoved(board->shownPointer());};
    // Backups every few minutes, when wanted.
    autosaveTimer=new QTimer(this);connect(autosaveTimer,&QTimer::timeout,this,[this]{autosaveNow();});
    // A new text; with a number appended, a series follows. Its first text follows the pointer as well, as in the
    // reference: nothing is put down at the place clicked.
    board->textRequested=[this](Element &text){
        QString prefix;int start=-1;if(!editText(text,true,&prefix,&start))return false;
        if(start>=0){board->startTextSeries(text,prefix,start);return false;}
        return true;};
    board->editRequested=[this](int index){
        const auto &e=doc.board().elements[index];
        if(e.type==ElementType::Text){Element copy=e;if(editText(copy,false))editElements({index},[&](Element &t){t=copy;});}
    };
    board->toolChanged=[this](BoardView::Tool tool){if(auto *b=toolButtons->button(int(tool)))b->setChecked(true);refreshHelpLine();autorouteBar->setVisible(tool==BoardView::Tool::Autoroute);if(autorouteGrid)autorouteGrid();};
    board->shapeRequested=[this]{specialShape();};
    // Whether the schematic wants two pads connected: both belong to pins of one of its nets.
    board->schematicConnects=[this](int one,int two){
        if(!schematicAvailable())return false;
        try{
            const auto t=schematicForBoard();const auto c=checkNets(doc.board(),t);
            for(const auto &net:t.nets){bool first=false,second=false;
                for(const auto &pin:net.pins){const auto pads=c.padsFor(t,pin.component,pin.pin);first|=pads.contains(one);second|=pads.contains(two);}
                if(first&&second)return true;}
        }catch(const std::exception &){}
        return false;};
    // A double click took an element's sizes: the fields at the left show them (without changing the selection).
    board->sizesTaken=[this](const Element &){
        // The fields write back into the view as each one changes, so the taken sizes are kept aside and set again.
        const double width=board->trackWidth,pad=board->padDiameter,drill=board->padDrill,smdWidth=board->smdWidth,smdHeight=board->smdHeight;
        const PadShape shape=board->padShape;const bool via=board->padVia;
        const bool was=refreshing;refreshing=true;trackWidthBox->setValue(width);padBox->setValue(pad);drillBox->setValue(drill);
        shapeBox->setCurrentIndex(int(shape)-1);viaBox->setChecked(via);smdWidthBox->setValue(smdWidth);smdHeightBox->setValue(smdHeight);refreshing=was;
        board->trackWidth=width;board->padDiameter=pad;board->padDrill=drill;board->padShape=shape;board->padVia=via;board->smdWidth=smdWidth;board->smdHeight=smdHeight;
    };
    // The reference's popup menu on the selection.
    board->contextMenuRequested=[this](QPoint at){
        auto *menu=new QMenu(this);menu->setAttribute(Qt::WA_DeleteOnClose);menu->setObjectName("contextMenu");
        menu->addAction(ui("Eigenschaften"),this,[this]{sidePanel->setCurrentIndex(0);sidePanel->show();})->setObjectName("showProperties");menu->addSeparator();
        for(const char *name:{"copy","cut","paste","duplicate","delete","","arrange",""})if(!*name)menu->addSeparator();else menu->addAction(action(name));
        // A name for the selection, and the elements of a name.
        menu->addAction(ui("Benennen…"),this,[this]{
            QString first;for(int i:board->selection())if(!doc.board().elements[i].name.isEmpty()){first=doc.board().elements[i].name;break;}
            bool ok=false;const QString name=QInputDialog::getText(this,ui("Benennen"),ui("Name der markierten Elemente:"),QLineEdit::Normal,first,&ok);if(ok)nameSelection(name);
        })->setObjectName("nameSelection");
        {const QString wanted=nameToSelect();auto *pick=menu->addAction(wanted.isEmpty()?ui("Namen markieren"):ui("Alle mit dem Namen „%1“ markieren").arg(wanted),this,[this,wanted]{selectByName(wanted);});
            pick->setObjectName("selectByName");pick->setEnabled(!wanted.isEmpty());}
        menu->addSeparator();
        {const auto parts=components(doc.board());int designator=-1;for(const auto &c:parts)for(int i:board->selection())if(c.members.contains(i)){designator=c.designator;break;}
            auto *part=menu->addAction(ui("Bauteil…"),this,[this,designator]{editComponent(designator);});part->setObjectName("editComponent");part->setEnabled(designator>=0);}
        menu->addSeparator();
        for(const char *name:{"rotate","rotateBack","mirror","mirrorVertical","","otherSide"})if(!*name)menu->addSeparator();else menu->addAction(action(name));
        {auto *layers=menu->addMenu(ui("Auf Layer setzen"));layers->setObjectName("setLayer");layers->setEnabled(!board->selection().isEmpty());
            for(int layer:layerOrder()){if((layer==Inner1||layer==Inner2)&&!doc.board().multilayer)continue;
                layers->addAction(layerName(layer),this,[this,layer]{setSelectionLayer(layer);})->setObjectName(QStringLiteral("setLayer-%1").arg(layer));}}
        menu->addSeparator();for(const char *name:{"group","ungroup","","alignGrid"})if(!*name)menu->addSeparator();else menu->addAction(action(name));
        menu->addSeparator();
        {auto *origin=menu->addMenu(ui("Koordinatenursprung"));origin->setObjectName("originMenu");const QPointF clicked=board->originAt(board->contextPoint());
            origin->addAction(ui("Oben links"),this,[this]{setOrigin({0,0});})->setObjectName("originTopLeft");
            origin->addAction(ui("Unten links"),this,[this]{setOrigin({0,doc.board().height});})->setObjectName("originBottomLeft");
            origin->addSeparator();origin->addAction(ui("An die angeklickte Stelle"),this,[this,clicked]{setOrigin(clicked);})->setObjectName("originHere");}
        menu->popup(at);
    };
    // The popup menu of a node of the selected track or area.
    board->nodeMenuRequested=[this](int element,int node,QPoint at){
        const auto &e=doc.board().elements[element];const bool track=e.type==ElementType::Track;const int n=int(e.points.size());
        auto *menu=new QMenu(this);menu->setAttribute(Qt::WA_DeleteOnClose);menu->setObjectName("nodeMenu");
        auto *remove=menu->addAction(ui("Knoten löschen"),this,[this,element,node]{removeNode(element,node);});remove->setObjectName("nodeRemove");remove->setEnabled(n>(track?2:3));
        menu->addAction(ui("Knoten auf das Raster setzen"),this,[this,element,node]{alignNode(element,node);})->setObjectName("nodeAlign");
        menu->addAction(ui("Alle Knoten auf das Raster setzen"),this,[this,element]{alignNodes(element);})->setObjectName("nodeAlignAll");
        if(track){auto *split=menu->addAction(ui("Leiterbahn an diesem Knoten teilen"),this,[this,element,node]{splitTrack(element,node);});
            split->setObjectName("nodeSplit");split->setEnabled(node>0&&node<n-1);
            auto *join=menu->addAction(ui("Mit der Leiterbahn an diesem Knoten verbinden"),this,[this,element,node]{joinTracks(element,node);});
            join->setObjectName("nodeJoin");join->setEnabled(joiningTrack(element,node)>=0);}
        menu->popup(at);
    };
    board->photoRequested=[this]{action("photo")->trigger();};
    // The keys 1 to 9 pick their grids.
    board->gridKey=[this](int n){if(n>=1&&n<=gridValues.size())chooseGrid(gridValues[n-1]);};
    connect(tabs,&QTabBar::currentChanged,this,[this](int index){if(!refreshing&&index>=0)switchBoard(index);});
    connect(tabs,&QTabBar::tabBarDoubleClicked,this,[this](int index){if(index>=0){switchBoard(index);editBoardProperties();}});
    connect(tabs,&QTabBar::customContextMenuRequested,this,[this](QPoint at){
        const int index=tabs->tabAt(at);if(index>=0)switchBoard(index);
        QMenu menu;for(const char *name:{"addBoard","boardProperties","","copyBoard","removeBoard","","insertBoards","saveBoard","","boardLast","boardFirst"}){
            if(*name)menu.addAction(action(name));else menu.addSeparator();}
        menu.exec(tabs->mapToGlobal(at));
    });
    Document fresh;fresh.boards.append(newBoard(ui("Platine 1")));setDocument(fresh);
}

std::optional<Board> askNewBoard(QWidget *parent,const QString &name,bool originTopLeft){
    QDialog dialog(parent);dialog.setWindowTitle(ui("Neue Platine"));auto *v=new QVBoxLayout(&dialog);
    auto *plain=new QRadioButton(ui("Nur Arbeitsfläche, ohne Kontur"));auto *rectangle=new QRadioButton(ui("Rechteckige Platine mit Kontur"));auto *round=new QRadioButton(ui("Runde Platine mit Kontur"));
    plain->setObjectName("newPlain");rectangle->setObjectName("newRectangle");round->setObjectName("newRound");plain->setChecked(true);
    for(auto *b:{plain,rectangle,round})v->addWidget(b);
    auto *form=new QFormLayout;v->addLayout(form);
    auto *width=spin(160,1,500,2),*height=spin(100,1,500,2),*diameter=spin(100,1,500,2),*margin=spin(20,0,500,2);
    width->setObjectName("newWidth");height->setObjectName("newHeight");diameter->setObjectName("newDiameter");margin->setObjectName("newMargin");
    auto *area=new QLabel;area->setObjectName("newArea");auto *title=new QLineEdit(name);title->setObjectName("newName");title->setMaxLength(30);
    form->addRow(ui("Breite"),width);form->addRow(ui("Höhe"),height);form->addRow(ui("Durchmesser"),diameter);form->addRow(ui("Rand um die Kontur"),margin);
    form->addRow(ui("Arbeitsfläche"),area);form->addRow(ui("Name"),title);
    auto shape=[&]{NewBoard s;s.kind=round->isChecked()?NewBoard::Round:rectangle->isChecked()?NewBoard::Rectangle:NewBoard::Plain;s.originTopLeft=originTopLeft;
        s.width=width->value();s.height=height->value();s.diameter=diameter->value();s.margin=margin->value();return s;};
    // Only the fields of the chosen kind; the working area it makes.
    auto show=[&]{const auto s=shape();const bool outline=s.kind!=NewBoard::Plain;
        form->setRowVisible(width,s.kind!=NewBoard::Round);form->setRowVisible(height,s.kind!=NewBoard::Round);form->setRowVisible(diameter,s.kind==NewBoard::Round);
        form->setRowVisible(margin,outline);form->setRowVisible(area,outline);
        const Board b=newBoard(QString(),s);area->setText(ui("%1 × %2 mm").arg(uiLocale().toString(b.width,'f',2),uiLocale().toString(b.height,'f',2)));};
    for(auto *b:{plain,rectangle,round})QObject::connect(b,&QRadioButton::toggled,&dialog,show);
    for(auto *box:{width,height,diameter,margin})QObject::connect(box,&QDoubleSpinBox::valueChanged,&dialog,show);
    show();
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);v->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return std::nullopt;
    return newBoard(title->text().trimmed().isEmpty()?name:title->text().trimmed(),shape());
}
// The name of a kind of element for the list of a multiple selection, in the singular for one.
static QString kindName(ElementType type,int count){
    const bool one=count==1;
    switch(type){
    case ElementType::Pad:return one?ui("Lötauge"):ui("Lötaugen");case ElementType::SmdPad:return one?ui("SMD-Pad"):ui("SMD-Pads");
    case ElementType::Area:return one?ui("Fläche"):ui("Flächen");case ElementType::Circle:return one?ui("Kreisring"):ui("Kreisringe");
    case ElementType::Text:return one?ui("Text"):ui("Texte");case ElementType::Track:return one?ui("Leiterbahn"):ui("Leiterbahnen");
    }
    return {};
}
QList<std::pair<QString,QStringList>> editorMenus(){
    return {{QStringLiteral("&Datei"),{"new","open","save","saveAs","","autosave","folders","","printerSetup","print","","exportImage","exportEmf","exportLay6","exportLay4","","exportGerber","exportDrill",
                                         "exportComponents","exportMilling","","importGerber","importMacro","saveMacro","","projectInfo"}},
            {QStringLiteral("&Bearbeiten"),{"undo","redo","","cut","copy","paste","duplicate","delete","","selectAll"}},
            {QStringLiteral("&Funktionen"),{"rotate","rotateBack","rotationAngle","","mirror","mirrorVertical","","align","alignGrid","","arrange","","otherSide","setLayer","","group","ungroup"}},
            {QStringLiteral("&Ansicht"),{"zoomBoard","zoomElements","zoomSelection","zoomPrevious","","zoomIn","zoomOut","","overview","crosshair","","fromBelow","photo","transparent","","millingWide","removeMilling"}},
            {QStringLiteral("&Extras"),{"drc","","compareSchematic","schematicAirwires","takeOverSchematic","assignPins","placeMissing","","removeAirwires","resetSolderMask","","holeList","deleteOutside","","specialShape","wizard","","currentCalculator","","templates","","textIoExport","textIoImport","","definePlugins","startPlugin"}},
            {QStringLiteral("&Platine"),{"addBoard","boardProperties","","copyBoard","removeBoard","","boardLast","boardFirst","","insertBoards","saveBoard"}},
            {QStringLiteral("&Optionen"),{"preferences"}}};
}

// --- actions
void Editor::createActions(){
    auto add=[this](const QString &name,const QString &text,const QString &icon,QKeySequence key,std::function<void()> run){
        auto *a=new QAction(icon.isEmpty()?QIcon():pcbIcon(icon),text,this);a->setObjectName(name);if(!key.isEmpty())a->setShortcut(key);
        // Every command ends a drag under way first, so that it works on the document as it was before the drag.
        a->setShortcutContext(Qt::WidgetWithChildrenShortcut);addAction(a);connect(a,&QAction::triggered,this,[this,run=std::move(run)]{board->cancelDrag();run();});
        actions.insert(name,a);return a;
    };
    add("new",ui("&Neu…"),"new",QKeySequence::New,[this]{
        // As in the reference a new document starts with the dialog for its board.
        if(!maybeSave())return;const auto made=askNewBoard(this,ui("Platine 1"),originTopLeft);if(!made)return;
        Document fresh;fresh.boards.append(*made);setDocument(fresh);});
    add("open",ui("Ö&ffnen…"),"open",QKeySequence::Open,[this]{
        if(openHandler){openHandler(startFolder(Layouts));return;}
        if(!maybeSave())return;
        const auto file=QFileDialog::getOpenFileName(this,ui("Leiterplatte öffnen"),startFolder(Layouts),
            ui("Leiterplatten (*.olpcb *.lay6 *.lay);;OpenLoch-Leiterplatten (*.olpcb);;Sprint-Layout (*.lay6 *.lay)"));
        if(!file.isEmpty())usedFolder(Layouts,QFileInfo(file).absolutePath());
        QString error;if(!file.isEmpty()&&!openFile(file,&error))QMessageBox::warning(this,ui("Leiterplatte öffnen"),error);});
    add("save",ui("&Speichern"),"save",QKeySequence::Save,[this]{
        if(saveHandler){saveHandler(false);return;}
        if(path.isEmpty()){action("saveAs")->trigger();return;}QString error;if(!saveFile(path,&error))QMessageBox::warning(this,ui("Speichern"),error);});
    add("saveAs",ui("Speichern &unter…"),"",QKeySequence::SaveAs,[this]{
        if(saveHandler){saveHandler(true);return;}
        QString suggestion=path;if(suggestion.isEmpty())suggestion=QDir(startFolder(Layouts)).filePath(QFileInfo(importedFile.isEmpty()?ui("Leiterplatte"):importedFile).completeBaseName()+".olpcb");
        auto file=QFileDialog::getSaveFileName(this,ui("Leiterplatte speichern"),suggestion,ui("OpenLoch-Leiterplatten (*.olpcb)"));
        if(file.isEmpty())return;if(QFileInfo(file).suffix().isEmpty())file+=".olpcb";usedFolder(Layouts,QFileInfo(file).absolutePath());
        QString error;if(!saveFile(file,&error))QMessageBox::warning(this,ui("Speichern"),error);});
    add("print",ui("&Drucken…"),"print",QKeySequence::Print,[this]{printPreview();});
    add("exportImage",ui("Als Bild exportieren…"),"",{},[this]{exportImageDialog();});
    add("exportEmf",ui("Als EMF exportieren…"),"",{},[this]{
        auto file=QFileDialog::getSaveFileName(this,ui("Als EMF exportieren"),QDir(startFolder(Pictures)).filePath(QFileInfo(displayName()).completeBaseName()+".emf"),ui("Enhanced Metafile (*.emf)"));
        if(file.isEmpty())return;if(QFileInfo(file).suffix().isEmpty())file+=".emf";usedFolder(Pictures,QFileInfo(file).absolutePath());
        QString error;if(!exportEmf(file,&error))QMessageBox::warning(this,ui("Exportieren"),error);});
    add("exportGerber",ui("Gerber-Export…"),"",{},[this]{gerberDialog();});
    add("exportDrill",ui("Bohrdaten (Excellon)…"),"",{},[this]{drillDialog();});
    add("holeList",ui("Bohrungen &auflisten…"),"",{},[this]{holeListDialog();});
    add("deleteOutside",ui("Elemente außerhalb der Platine löschen"),"",{},[this]{
        const auto &b=doc.board();int n=0;const QRectF area(0,0,b.width,b.height);for(const auto &e:b.elements){const QRectF r=bounds(e);n+=!r.intersects(area)&&!area.contains(r.topLeft());}
        if(!n){QMessageBox::information(this,ui("Elemente außerhalb der Platine löschen"),ui("Alle Elemente liegen auf der Platine."));return;}
        if(QMessageBox::question(this,ui("Elemente außerhalb der Platine löschen"),ui("%1 Elemente liegen ganz außerhalb der Platine. Löschen?").arg(n))==QMessageBox::Yes)deleteOutside();});
    add("exportComponents",ui("Bauteildaten exportieren…"),"",{},[this]{componentDataDialog();});
    add("exportMilling",ui("Fräsdaten (HPGL)…"),"",{},[this]{millingDialog();});
    add("importGerber",ui("Gerber importieren…"),"",{},[this]{
        GerberImportDialog dialog(this);if(dialog.exec()!=QDialog::Accepted)return;
        Board board;{ProgressWindow progress(this,ui("Gerber-Dateien werden gelesen …"));QApplication::setOverrideCursor(Qt::WaitCursor);
            board=importGerber(dialog.import(),dialog.boardName(),progress.report());QApplication::restoreOverrideCursor();}
        importBoard(board,dialog.newBoard());});
    // The milling paths shown after writing them: as hairlines or as wide as the cutter, until hidden.
    add("millingWide",ui("Fräsbahnen in Fräserbreite zeigen"),"",{},[this]{board->setMillingWide(action("millingWide")->isChecked());})->setCheckable(true);
    add("removeMilling",ui("Fräsbahnen ausblenden"),"",{},[this]{board->setMillingPaths({},{},0);refreshActions();});
    add("exportLay6",ui("Als Sprint-Layout 6 exportieren…"),"",{},[this]{
        auto file=QFileDialog::getSaveFileName(this,ui("Als Sprint-Layout 6 exportieren"),QDir(startFolder(Layouts)).filePath(QFileInfo(displayName()).completeBaseName()+".lay6"),ui("Sprint-Layout 6 (*.lay6)"));
        if(file.isEmpty())return;if(QFileInfo(file).suffix().isEmpty())file+=".lay6";usedFolder(Layouts,QFileInfo(file).absolutePath());
        QString error;int skipped=0;if(!exportSprint(file,&error,6,&skipped)){QMessageBox::warning(this,ui("Exportieren"),error);return;}
        if(skipped)QMessageBox::information(this,ui("Exportieren"),ui("%1 Flächen „Nur Lötstopp“ auf anderen Layern als K1 und K2 wurden weggelassen.").arg(skipped));});
    add("exportLay4",ui("Als Sprint-Layout 4.0 exportieren…"),"",{},[this]{
        auto file=QFileDialog::getSaveFileName(this,ui("Als Sprint-Layout 4.0 exportieren"),QDir(startFolder(Layouts)).filePath(QFileInfo(displayName()).completeBaseName()+".lay"),ui("Sprint-Layout 4.0 (*.lay)"));
        if(file.isEmpty())return;if(QFileInfo(file).suffix().isEmpty())file+=".lay";usedFolder(Layouts,QFileInfo(file).absolutePath());
        QString error;int skipped=0;if(!exportSprint(file,&error,4,&skipped)){QMessageBox::warning(this,ui("Exportieren"),error);return;}
        int masks=0;for(const auto &b:doc.boards)for(const auto &e:b.elements)masks+=e.type==ElementType::Area&&e.maskOnly&&e.layer<=SilkBottom;
        if(skipped>masks)QMessageBox::information(this,ui("Exportieren"),ui("%1 Elemente auf den Layern I1, I2 und U wurden weggelassen; Sprint-Layout 4.0 kennt diese Layer nicht.").arg(skipped-masks));
        if(masks)QMessageBox::information(this,ui("Exportieren"),ui("%1 Flächen „Nur Lötstopp“ wurden weggelassen; Sprint-Layout 4.0 kennt sie nicht.").arg(masks));});
    add("importMacro",ui("Makro laden…"),"",{},[this]{importMacro();});
    // Text-IO: the selection (or the whole board) as text, and elements from such a file on the pointer.
    add("textIoExport",ui("Text-IO: Elemente exportieren…"),"",{},[this]{
        auto sel=board->selection();if(sel.isEmpty())for(int i=0;i<doc.board().elements.size();i++)sel.append(i);
        auto file=QFileDialog::getSaveFileName(this,ui("Text-IO: Elemente exportieren"),QDir(startFolder(Layouts)).filePath(QFileInfo(displayName()).completeBaseName()+".txt"),ui("Text-IO (*.txt)"));
        if(file.isEmpty())return;if(QFileInfo(file).suffix().isEmpty())file+=".txt";usedFolder(Layouts,QFileInfo(file).absolutePath());
        QSaveFile out(file);const auto bytes=sprint::textIOBytes(sprint::writeTextIO(extract(doc.board(),sel)));
        if(!out.open(QIODevice::WriteOnly)||out.write(bytes)!=bytes.size()||!out.commit())QMessageBox::warning(this,ui("Exportieren"),out.errorString());});
    add("textIoImport",ui("Text-IO: Elemente importieren…"),"",{},[this]{
        const auto file=QFileDialog::getOpenFileName(this,ui("Text-IO: Elemente importieren"),startFolder(Layouts),ui("Text-IO (*.txt);;Alle Dateien (*)"));
        if(file.isEmpty())return;usedFolder(Layouts,QFileInfo(file).absolutePath());QFile in(file);
        try{
            if(!in.open(QIODevice::ReadOnly))throw FormatError(in.errorString());
            auto els=sprint::readTextIO(sprint::textIOText(in.readAll()));if(els.isEmpty())return;
            QRectF r;for(const auto &e:els)r=r.united(bounds(e));const QPointF centre=board->snap(r.center());for(auto &e:els)pcb::move(e,-centre);
            board->setTool(BoardView::Tool::Select);board->beginPlacement(placeable(els,doc.board(),{},false));
        }catch(const std::exception &e){QMessageBox::warning(this,ui("Text-IO: Elemente importieren"),QString::fromUtf8(e.what()));}});
    add("saveMacro",ui("Auswahl als Makro speichern…"),"",{},[this]{saveMacro();});
    add("definePlugins",ui("Plugin definieren…"),"",{},[this]{pluginDialog();});
    add("startPlugin",ui("Plugin starten…"),"",{},[this]{startPluginDialog();});
    add("projectInfo",ui("Projekt-Info…"),"",{},[this]{editProjectInfo();});
    add("undo",ui("&Rückgängig"),"undo",QKeySequence::Undo,[this]{undo();});
    add("redo",ui("&Wiederholen"),"redo",QKeySequence::Redo,[this]{redo();});
    add("cut",ui("&Ausschneiden"),"cut",QKeySequence::Cut,[this]{copySelection();deleteSelection();});
    add("copy",ui("&Kopieren"),"copy",QKeySequence::Copy,[this]{copySelection();});
    add("paste",ui("E&infügen"),"paste",QKeySequence::Paste,[this]{pasteClipboard();});
    add("duplicate",ui("&Duplizieren"),"duplicate",QKeySequence(Qt::CTRL|Qt::Key_D),[this]{duplicateSelection();});
    add("delete",ui("&Löschen"),"delete",QKeySequence::Delete,[this]{deleteSelection();});
    add("selectAll",ui("Alles &markieren"),"",QKeySequence::SelectAll,[this]{board->selectAll();});
    // Rotation by the chosen angle, clockwise as in the reference; Shift turns the other way.
    add("rotate",QString(),"rotate",QKeySequence(Qt::CTRL|Qt::Key_R),[this]{rotateSelection(QApplication::keyboardModifiers()&Qt::ShiftModifier?rotation:-rotation);});
    add("rotateBack",ui("Drehen gegen den Uhrzeigersinn"),"",QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_R),[this]{rotateSelection(rotation);});
    {auto *menu=new QMenu(this);auto *group=new QActionGroup(this);
        for(double step:{90.0,45.0,30.0,15.0,5.0}){
            auto *a=menu->addAction(QString("%1°").arg(step));a->setCheckable(true);a->setChecked(step==90);a->setObjectName(QString("rotationStep%1").arg(int(step)));group->addAction(a);
            connect(a,&QAction::triggered,this,[this,step]{setRotationStep(step);});
        }
        auto *free=menu->addAction(ui("Freier Winkel…"));free->setCheckable(true);free->setObjectName("rotationStepFree");group->addAction(free);
        connect(free,&QAction::triggered,this,[this]{
            bool ok=false;const double v=QInputDialog::getDouble(this,ui("Drehwinkel"),ui("Drehwinkel in Grad (im Uhrzeigersinn):"),rotation,-360,360,2,&ok);if(ok&&v!=0)setRotationStep(v);});
        auto *a=new QAction(ui("Drehwinkel"),this);a->setObjectName("rotationAngle");a->setMenu(menu);actions.insert("rotationAngle",a);
        // A click on the tool bar's button opens the menu at once.
        connect(a,&QAction::triggered,this,[a]{a->menu()->popup(QCursor::pos());});}
    add("mirror",ui("Horizontal spiegeln"),"mirror",systemSafe(Qt::Key_H),[this]{mirrorSelection();});
    add("mirrorVertical",ui("Vertikal spiegeln"),"",QKeySequence(Qt::CTRL|Qt::Key_T),[this]{mirrorSelectionVertically();});
    {auto *menu=new QMenu(this);const QStringList names{"alignLeft","alignRight","alignTop","alignBottom","alignCentreX","alignCentreY"};
        const QStringList texts{ui("Links"),ui("Rechts"),ui("Oben"),ui("Unten"),ui("Horizontal mittig"),ui("Vertikal mittig")};
        for(int k=0;k<names.size();k++)menu->addAction(add(names[k],texts[k],"",{},[this,k]{alignSelection(k);}));
        auto *a=new QAction(ui("Ausrichten"),this);a->setObjectName("align");a->setMenu(menu);actions.insert("align",a);
        connect(a,&QAction::triggered,this,[a]{a->menu()->popup(QCursor::pos());});}
    add("alignGrid",ui("Am Raster ausrichten"),"",{},[this]{alignToGrid();});
    // Onto a layer, as the reference's Funktionen menu offers it (the inner layers on multilayer boards only).
    {auto *menu=new QMenu(this);menu->setObjectName("setLayerMenu");auto *a=new QAction(ui("Auf Layer setzen"),this);a->setObjectName("setLayer");a->setMenu(menu);actions.insert("setLayer",a);
        connect(menu,&QMenu::aboutToShow,this,[this,menu]{menu->clear();
            for(int layer:layerOrder()){if((layer==Inner1||layer==Inner2)&&!doc.board().multilayer)continue;
                menu->addAction(layerName(layer),this,[this,layer]{setSelectionLayer(layer);})->setObjectName(QStringLiteral("setLayerTo-%1").arg(layer));}});}
    add("arrange",ui("Kacheln / Kreisförmig anordnen…"),"",{},[this]{arrangeDialog();});
    add("templates",ui("Vorlage…"),"",{},[this]{templateDialog();});
    add("wizard",ui("Bauteil-Assistent…"),"",{},[this]{wizardDialog();});
    add("preferences",ui("Grundeinstellungen…"),"",{},[this]{preferencesDialog();});
    // Two pages of the preferences straight from the file menu, as the reference has them there.
    add("autosave",ui("Automatische &Sicherung…"),"",{},[this]{preferencesDialog(QStringLiteral("backups"));});
    add("folders",ui("Arbeits&ordner…"),"",{},[this]{preferencesDialog(QStringLiteral("folders"));});
    // The page of the printer, as the reference sets it up from the file menu; the print preview keeps it.
    add("printerSetup",ui("Drucker &einrichten…"),"",{},[this]{QPageSetupDialog setup(printerDevice(),this);setup.exec();});
    add("otherSide",ui("Auf andere &Platinenseite"),"pcb-other-side",systemSafe(Qt::Key_W),[this]{otherSide();});
    add("group",ui("&Gruppieren"),"group",QKeySequence(Qt::CTRL|Qt::Key_G),[this]{groupSelection();});
    add("ungroup",ui("Gruppierung &auflösen"),"ungroup",QKeySequence(Qt::CTRL|Qt::Key_U),[this]{ungroupSelection();})
        ->setShortcuts({QKeySequence(Qt::CTRL|Qt::Key_U),QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_G)});
    setRotationStep(90);
    add("zoomBoard",ui("Ganze &Platine"),"zoom-board",QKeySequence(Qt::CTRL|Qt::Key_0),[this]{board->zoomBoard();});
    add("zoomElements",ui("Alle &Elemente"),"",{},[this]{board->zoomElements();});
    add("zoomSelection",ui("&Markierung"),"",{},[this]{board->zoomSelection();});
    add("zoomPrevious",ui("&Vorherige Ansicht"),"",{},[this]{board->zoomBack();});
    // The overview below the tools, kept with the preferences.
    add("overview",ui("Ü&bersicht"),"",{},[this]{overview->setVisible(action("overview")->isChecked());preferences().setValue("showOverview",action("overview")->isChecked());})->setCheckable(true);
    // The cross hair's lines, switched as in the reference's status line; kept with the preferences.
    add("crosshair",ui("&Fadenkreuz"),"pcb-crosshair",{},[this]{board->crosshair.lines=action("crosshair")->isChecked();preferences().setValue("crosshair/lines",board->crosshair.lines);board->update();})->setCheckable(true);
    action("crosshair")->setChecked(board->crosshair.lines);
    add("zoomIn",ui("Ver&größern"),"zoom-in",QKeySequence::ZoomIn,[this]{board->zoomAt(1.5,QPointF(board->width()/2.0,board->height()/2.0));});
    add("zoomOut",ui("Ver&kleinern"),"zoom-out",QKeySequence::ZoomOut,[this]{board->zoomAt(1/1.5,QPointF(board->width()/2.0,board->height()/2.0));});
    add("addBoard",ui("Neue Platine hinzufügen…"),"",{},[this]{
        const auto made=askNewBoard(this,ui("Platine %1").arg(doc.boards.size()+1),originTopLeft);if(made)addBoard(*made);});
    add("removeBoard",ui("Platine löschen"),"",{},[this]{removeBoard();});
    add("boardProperties",ui("Platineneigenschaften…"),"properties",{},[this]{editBoardProperties();});
    add("copyBoard",ui("Platine &kopieren"),"",{},[this]{copyBoard();});
    add("boardLast",ui("Platine ans &Ende"),"",{},[this]{moveBoard(true);});
    add("boardFirst",ui("Platine an den &Anfang"),"",{},[this]{moveBoard(false);});
    add("insertBoards",ui("Platinen aus Datei &einfügen…"),"",{},[this]{
        const auto file=QFileDialog::getOpenFileName(this,ui("Platinen aus Datei einfügen"),startFolder(Layouts),
            ui("Leiterplatten (*.olpcb *.lay6 *.lay);;OpenLoch-Leiterplatten (*.olpcb);;Sprint-Layout (*.lay6 *.lay)"));
        if(file.isEmpty())return;
        // Where the boards go: after the active one or behind all.
        QDialog where(this);where.setWindowTitle(ui("Platinen einfügen"));auto *v=new QVBoxLayout(&where);
        auto *after=new QRadioButton(ui("Hinter der aktuellen Platine"));after->setObjectName("insertAfter");auto *last=new QRadioButton(ui("Hinter der letzten Platine"));last->setObjectName("insertLast");
        after->setChecked(true);v->addWidget(after);v->addWidget(last);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);v->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&where,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&where,&QDialog::reject);
        if(where.exec()!=QDialog::Accepted)return;
        QString error;if(!insertBoards(file,last->isChecked(),&error))QMessageBox::warning(this,ui("Platinen aus Datei einfügen"),error);});
    add("saveBoard",ui("Platine in neuer Datei &speichern…"),"",{},[this]{
        const auto file=QFileDialog::getSaveFileName(this,ui("Platine in neuer Datei speichern"),startFolder(Layouts),ui("OpenLoch-Leiterplatten (*.olpcb)"));
        if(file.isEmpty())return;QString error;if(!saveBoardAs(file,&error))QMessageBox::warning(this,ui("Platine in neuer Datei speichern"),error);});
    add("fromBelow",ui("Von &unten betrachten"),"view-flip",{},[this]{board->setFromBelow(action("fromBelow")->isChecked());})->setCheckable(true);
    add("photo",ui("&Fotoansicht"),"pcb-photo",{},[this]{const bool on=action("photo")->isChecked();board->setPhotoView(on);if(photoBar)photoBar->setVisible(on);})->setCheckable(true);
    add("transparent",ui("&Transparent"),"pcb-transparent",{},[this]{board->transparent=action("transparent")->isChecked();board->update();})->setCheckable(true);
    // The zoom commands in one menu, for the tool bar.
    {auto *menu=new QMenu(this);for(const char *name:{"zoomPrevious","","zoomBoard","zoomElements","zoomSelection"})if(!*name)menu->addSeparator();else menu->addAction(action(name));
        auto *a=new QAction(pcbIcon("tool-zoom"),ui("Zoom"),this);a->setObjectName("zoomMenu");a->setMenu(menu);actions.insert("zoomMenu",a);
        connect(a,&QAction::triggered,this,[this]{action("zoomBoard")->trigger();});}
    add("drc",ui("&Design Rule Check"),"pcb-drc",{},[this]{runDesignRuleCheck();});
    add("compareSchematic",ui("Mit Schaltplan vergleichen"),"",{},[this]{compareWithSchematic(true);});
    add("schematicAirwires",ui("Luftlinien aus dem Schaltplan"),"",{},[this]{if(action("schematicAirwires")->isChecked())compareWithSchematic(false);else board->setSchematicAirwires({});})
        ->setCheckable(true);
    add("takeOverSchematic",ui("Aus Schaltplan übernehmen…"),"",{},[this]{takeOverFromSchematic();});
    add("assignPins",ui("Anschlüsse zuordnen…"),"",{},[this]{assignPinsDialog();});
    add("placeMissing",ui("Fehlende Bauteile setzen…"),"",{},[this]{placeMissingParts();});
    add("removeAirwires",ui("Verbundene &Luftlinien entfernen"),"",{},[this]{
        const int left=[&]{const int before=int(board->airwires().size());const int removed=removeRoutedAirwires();return before-removed;}();
        QMessageBox::information(this,ui("Luftlinien entfernen"),ui("%1 Luftlinien verbleiben.").arg(left));});
    add("resetSolderMask",ui("Lötstoppmaske &zurücksetzen"),"",{},[this]{resetSolderMask();});
    add("specialShape",ui("&Spezialform…"),"pcb-shape",{},[this]{specialShape();});
    add("currentCalculator",ui("Strombelastbarkeit…"),"",{},[this]{currentDialog();});
    // Symbols for the commands the tool bar shows as well.
    for(const auto &[name,icon]:{std::pair{"rotationAngle","pcb-angle"},{"mirrorVertical","pcb-mirror-vertical"},{"align","pcb-align"},{"alignGrid","pcb-align-grid"},
                                  {"removeAirwires","pcb-airwires-remove"},{"projectInfo","pcb-info"},{"templates","pcb-template"}})if(auto *a=action(name))a->setIcon(pcbIcon(icon));
    // A command under the pointer, in a menu or the tool bar, shows in the help line with its key for a moment, as the
    // reference's hints do.
    helpTimer=new QTimer(this);helpTimer->setSingleShot(true);helpTimer->setInterval(4000);connect(helpTimer,&QTimer::timeout,this,[this]{refreshHelpLine();});
    for(auto *a:std::as_const(actions))connect(a,&QAction::hovered,this,[this,a]{
        if(!helpLine)return;QString text=a->text();text.remove('&');if(text.isEmpty())return;
        const QString key=a->shortcut().toString(QKeySequence::NativeText),more=commandHelp(a->objectName());
        const QString named=key.isEmpty()?text:ui("%1 (%2)").arg(text,key);helpLine->setText(more.isEmpty()?named:ui("%1: %2").arg(named,more));helpTimer->start();});
}
QList<QAction*> Editor::toolBarActions() const{
    // As the reference's bar: turning with its angles, mirroring both ways, aligning, the zoom commands, the project info,
    // the template and the transparent mode.
    return {action("new"),action("open"),action("save"),action("print"),nullptr,action("undo"),action("redo"),nullptr,action("cut"),action("copy"),action("paste"),
            action("delete"),action("duplicate"),nullptr,action("rotate"),action("rotationAngle"),action("mirror"),action("mirrorVertical"),action("otherSide"),nullptr,
            action("align"),action("alignGrid"),action("group"),action("ungroup"),nullptr,action("zoomMenu"),action("zoomBoard"),action("zoomIn"),action("zoomOut"),nullptr,
            action("fromBelow"),action("photo"),action("transparent"),nullptr,action("drc"),action("removeAirwires"),action("projectInfo"),action("templates")};
}
// --- the help line, the layer info
QString Editor::commandHelp(const QString &name){
    // What each command of the tool bar does, as the reference's long hints say it, in OpenLoch's own words.
    const QHash<QString,QString> help{
        {QStringLiteral("new"),ui("Ein neues Dokument mit einer leeren Platine beginnen.")},
        {QStringLiteral("open"),ui("Eine Leiterplatte öffnen, aus OpenLoch oder Sprint-Layout.")},
        {QStringLiteral("save"),ui("Das Dokument speichern.")},
        {QStringLiteral("print"),ui("Die Platine mit Vorschau drucken.")},
        {QStringLiteral("undo"),ui("Den letzten Schritt zurücknehmen.")},
        {QStringLiteral("redo"),ui("Den zurückgenommenen Schritt wiederholen.")},
        {QStringLiteral("cut"),ui("Die markierten Elemente in die Zwischenablage verschieben.")},
        {QStringLiteral("copy"),ui("Die markierten Elemente in die Zwischenablage kopieren.")},
        {QStringLiteral("paste"),ui("Die Elemente der Zwischenablage am Mauszeiger einfügen.")},
        {QStringLiteral("delete"),ui("Die markierten Elemente löschen.")},
        {QStringLiteral("duplicate"),ui("Die markierten Elemente verdoppeln; die Kopie hängt am Mauszeiger.")},
        {QStringLiteral("rotate"),ui("Die markierten Elemente um den gewählten Winkel im Uhrzeigersinn drehen.")},
        {QStringLiteral("rotationAngle"),ui("Den Winkel wählen, um den Drehen dreht.")},
        {QStringLiteral("mirror"),ui("Die markierten Elemente waagerecht spiegeln.")},
        {QStringLiteral("mirrorVertical"),ui("Die markierten Elemente senkrecht spiegeln.")},
        {QStringLiteral("otherSide"),ui("Die markierten Elemente auf die andere Platinenseite setzen: gespiegelt, die Layer oben und unten getauscht.")},
        {QStringLiteral("align"),ui("Die markierten Elemente aneinander ausrichten.")},
        {QStringLiteral("alignGrid"),ui("Die markierten Elemente auf das Raster setzen.")},
        {QStringLiteral("group"),ui("Die markierten Elemente zu einer Gruppe zusammenfassen.")},
        {QStringLiteral("ungroup"),ui("Die Gruppen der markierten Elemente auflösen.")},
        {QStringLiteral("zoomMenu"),ui("Die vorherige Ansicht, die ganze Platine, alle Elemente oder die Markierung zeigen.")},
        {QStringLiteral("zoomBoard"),ui("Die ganze Platine zeigen.")},
        {QStringLiteral("zoomIn"),ui("Die Ansicht vergrößern.")},
        {QStringLiteral("zoomOut"),ui("Die Ansicht verkleinern.")},
        {QStringLiteral("fromBelow"),ui("Die Platine von unten betrachten, gespiegelt.")},
        {QStringLiteral("photo"),ui("Die fertige Platine wie auf einem Foto zeigen.")},
        {QStringLiteral("transparent"),ui("Überlappende Layer gemischt zeigen, so dass verdeckte Elemente sichtbar bleiben.")},
        {QStringLiteral("drc"),ui("Die Platine nach den Regeln des Design Rule Check prüfen.")},
        {QStringLiteral("removeAirwires"),ui("Luftlinien entfernen, deren Pads schon durch Kupfer verbunden sind.")},
        {QStringLiteral("projectInfo"),ui("Titel, Autor, Firma und Kommentar des Dokuments bearbeiten.")},
        {QStringLiteral("templates"),ui("Ein eingescanntes Layout als Vorlage unter die Platine legen.")}
    };
    return help.value(name);
}
QString Editor::toolHelp(BoardView::Tool tool){
    using T=BoardView::Tool;
    switch(tool){
    case T::Select:return ui("Standard: anklicken markiert, ziehen verschiebt, ein Rahmen markiert mehrere; Rechtsklick öffnet das Kontextmenü.");
    case T::Zoom:return ui("Zoom: einen Bereich aufziehen oder klicken.");
    case T::Track:return ui("Leiterbahn: jeder Klick setzt einen Knoten, ein Rechtsklick beendet sie.");
    case T::Pad:return ui("Lötauge: ein Klick setzt ein Lötauge.");
    case T::Smd:return ui("SMD-Pad: ein Klick setzt ein SMD-Pad.");
    case T::Circle:return ui("Kreisring: vom Mittelpunkt aus aufziehen.");
    case T::Rectangle:return ui("Rechteck: von einer Ecke zur anderen aufziehen.");
    case T::Area:return ui("Fläche: jeder Klick setzt eine Ecke, ein Klick auf den Anfang oder ein Rechtsklick schließt sie.");
    case T::Keepout:return ui("Sperrfläche: wie eine Fläche zeichnen oder als Rechteck aufziehen; sie hält die AutoMasse fern.");
    case T::Text:return ui("Text: ein Klick setzt einen Text.");
    case T::SolderMask:return ui("Lötstopp: ein Klick auf ein Element öffnet oder schließt seine Lötstoppmaske.");
    case T::Airwire:return ui("Luftlinie: zwei Pads nacheinander anklicken; ein Klick auf eine Luftlinie entfernt sie.");
    case T::Autoroute:return ui("Autoroute: ein Klick auf eine Luftlinie verlegt sie, ein Klick auf die verlegte Bahn nimmt sie zurück.");
    case T::Test:return ui("Test: ein Klick auf Kupfer hebt alles hervor, was damit leitend verbunden ist.");
    case T::Measure:return ui("Messen: von einem Punkt zum anderen ziehen.");
    }
    return {};
}
QString Editor::keyHints(BoardView::Tool tool) const{
    using T=BoardView::Tool;
    if(tool==T::Select||tool==T::Zoom||tool==T::SolderMask||tool==T::Airwire||tool==T::Autoroute||tool==T::Test)return {};
    QStringList keys{ui("%1: ohne Raster").arg(QKeySequence(Qt::CTRL).toString(QKeySequence::NativeText).remove('+')),
                     ui("%1: halbes Raster").arg(QKeySequence(Qt::SHIFT).toString(QKeySequence::NativeText).remove('+'))};
    if(tool==T::Track||tool==T::Area||tool==T::Keepout)keys<<ui("Leertaste: Abknickart wechseln [%1/5]").arg(board->bendMode+1);
    return keys.join(QStringLiteral("   "));
}
// The keys of the tools for the suite's tooltips (docs/suite.md): the view keeps handling them, so they are no shortcuts
// of actions but the property the tooltips read.
void Editor::refreshToolKeys(){
    using T=BoardView::Tool;
    static const QList<std::pair<T,const char*>> tools{{T::Select,"select"},{T::Zoom,"zoom"},{T::Track,"track"},{T::Pad,"pad"},{T::Smd,"smd"},{T::Circle,"circle"},
        {T::Rectangle,"rectangle"},{T::Area,"area"},{T::Text,"text"},{T::Airwire,"airwire"},{T::Autoroute,"autoroute"},{T::Test,"test"},{T::Measure,"measure"},{T::SolderMask,"solderMask"}};
    auto key=[this](const char *mode){const int k=board->modeKeys.value(QString::fromLatin1(mode));return k?QKeySequence(k).toString(QKeySequence::NativeText):QString();};
    if(toolButtons)for(const auto &[tool,mode]:tools)if(auto *b=toolButtons->button(int(tool)))b->setProperty("toolTipShortcut",key(mode));
    for(const auto &[name,mode]:{std::pair{"specialShape","shape"},{"photo","photo"}})if(auto *a=action(name))a->setProperty("toolTipShortcut",key(mode));
}
void Editor::refreshHelpLine(){
    if(!helpLine)return;const auto tool=board->tool();const QString keys=keyHints(tool);
    helpLine->setText(keys.isEmpty()?toolHelp(tool):toolHelp(tool)+"\n"+keys);
}
void Editor::showLayerInfo(){
    QDialog dialog(this);dialog.setWindowTitle(ui("Layer"));auto *v=new QVBoxLayout(&dialog);auto *grid=new QGridLayout;v->addLayout(grid);
    const QList<std::pair<int,QString>> meaning{
        {CopperTop,ui("Kupfer oben: Leiterbahnen, Flächen und SMD-Pads der Oberseite")},
        {SilkTop,ui("Bestückungsdruck oben: Umrisse und Beschriftung der Bauteile auf der Oberseite")},
        {CopperBottom,ui("Kupfer unten: die Lötseite bedrahteter Bauteile, SMD-Pads der Unterseite")},
        {SilkBottom,ui("Bestückungsdruck unten: für Bauteile auf der Unterseite, von oben gesehen gespiegelt")},
        {Inner1,ui("Innere Kupferlage 1 (nur bei mehrlagigen Platinen)")},
        {Inner2,ui("Innere Kupferlage 2 (nur bei mehrlagigen Platinen)")},
        {Outline,ui("Umriss: Kontur der Platine und Fräskanten")}};
    int row=0;
    for(const auto &[layer,text]:meaning){
        auto *swatch=new QLabel;QPixmap colour(18,14);colour.fill(board->colours.layers[layer]);swatch->setPixmap(colour);
        auto *name=new QLabel(QStringLiteral("<b>%1</b>").arg(layerName(layer)));auto *what=new QLabel(text);what->setWordWrap(true);
        grid->addWidget(swatch,row,0);grid->addWidget(name,row,1);grid->addWidget(what,row,2);row++;
    }
    auto *uses=new QLabel(ui("Bedrahtete Bauteile stecken auf der Oberseite: ihr Druck liegt auf B1, ihre Lötaugen gehen durch alle Kupferlagen. SMD-Bauteile oben haben ihre Pads auf K1 und ihren Druck auf B1, unten auf K2 und B2."));
    uses->setWordWrap(true);uses->setObjectName("layerUses");v->addWidget(uses);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close);v->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    dialog.exec();
}

QWidget *Editor::createToolPanel(){
    auto *panel=new QFrame;panel->setFrameShape(QFrame::StyledPanel);auto *v=new QVBoxLayout(panel);v->setContentsMargins(4,4,4,4);v->setSpacing(2);
    toolButtons=new QButtonGroup(panel);toolButtons->setExclusive(true);
    const QList<std::tuple<BoardView::Tool,QString,QString>> tools{
        {BoardView::Tool::Select,ui("Standard"),"tool-select"},{BoardView::Tool::Zoom,ui("Zoom"),"tool-zoom"},
        {BoardView::Tool::Track,ui("Leiterbahn"),"pcb-track"},{BoardView::Tool::Pad,ui("Lötauge"),"pcb-pad"},{BoardView::Tool::Smd,ui("SMD-Pad"),"pcb-smd"},
        {BoardView::Tool::Circle,ui("Kreisring"),"pcb-circle"},{BoardView::Tool::Rectangle,ui("Rechteck"),"tool-rectangle"},{BoardView::Tool::Area,ui("Fläche"),"pcb-area"},
        {BoardView::Tool::Keepout,ui("Sperrfläche"),"pcb-keepout"},{BoardView::Tool::Text,ui("Text"),"tool-text"},{BoardView::Tool::SolderMask,ui("Lötstopp"),"pcb-mask"},
        {BoardView::Tool::Airwire,ui("Luftlinie"),"pcb-airwire"},{BoardView::Tool::Autoroute,ui("Autoroute"),"pcb-autoroute"},{BoardView::Tool::Test,ui("Test"),"pcb-test"},{BoardView::Tool::Measure,ui("Messen"),"pcb-measure"}};
    for(const auto &[tool,text,icon]:tools){
        auto *b=new QToolButton;b->setText(text);b->setIcon(pcbIcon(icon));b->setIconSize(QSize(20,20));b->setCheckable(true);b->setAutoRaise(true);
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);b->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);b->setObjectName("tool-"+QString::number(int(tool)));
        toolButtons->addButton(b,int(tool));v->addWidget(b);
    }
    // Special shapes and the photo view work like tools of the reference but are commands here.
    for(const char *name:{"specialShape","photo"}){
        auto *b=new QToolButton;b->setDefaultAction(action(name));b->setIconSize(QSize(20,20));b->setAutoRaise(true);
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);b->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);v->addWidget(b);
    }
    toolButtons->button(int(BoardView::Tool::Select))->setChecked(true);
    connect(toolButtons,&QButtonGroup::idClicked,this,[this](int id){board->setTool(BoardView::Tool(id));});
    // Settings for new elements, as in the reference below its tool buttons; editing them changes selected elements too.
    auto *settings=new QFormLayout;settings->setContentsMargins(0,8,0,0);settings->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    gridBox=new QComboBox;gridBox->setEditable(true);gridBox->setObjectName("grid");
    for(double g:{2.54,1.27,.635,.3175,1.0,.5,.25,.1})gridBox->addItem(uiLocale().toString(g),g);
    connect(gridBox,&QComboBox::currentTextChanged,this,[this](const QString &text){
        if(refreshing)return;bool ok;const double g=uiLocale().toDouble(text.trimmed(),&ok);if(!ok||g<=0||g>100)return;
        if(std::abs(doc.board().grid-g)>1e-12){doc.board().grid=g;modified=true;revision++;board->update();if(titleChanged)titleChanged();}});
    {auto *row=new QHBoxLayout;row->setSpacing(2);row->addWidget(gridBox,1);
        auto *keys=new QToolButton;keys->setObjectName("gridKeys");keys->setText(QStringLiteral("1–9"));keys->setToolTip(ui("Raster der Tasten 1 bis 9…"));row->addWidget(keys);
        connect(keys,&QToolButton::clicked,this,[this]{gridKeysDialog();});
        auto *more=new QToolButton;more->setObjectName("gridMenu");more->setToolTip(ui("Raster wählen und darstellen"));more->setPopupMode(QToolButton::InstantPopup);
        auto *menu=new QMenu(more);menu->setObjectName("gridMenuItems");more->setMenu(menu);row->addWidget(more);
        connect(menu,&QMenu::aboutToShow,this,[this,menu]{fillGridMenu(menu);});settings->addRow(ui("Raster"),row);}
    trackWidthBox=spin(board->trackWidth,0,50,2);trackWidthBox->setObjectName("trackWidth");
    // Favourite widths on a button before the field, as the reference offers them.
    {auto *row=new QHBoxLayout;row->setSpacing(2);favouriteButton=new QToolButton;favouriteButton->setObjectName("widthFavourites");favouriteButton->setIcon(pcbIcon("pcb-favourites"));
        favouriteButton->setToolTip(ui("Bevorzugte Breiten"));favouriteButton->setPopupMode(QToolButton::InstantPopup);favouriteButton->setMenu(new QMenu(favouriteButton));
        connect(favouriteButton->menu(),&QMenu::aboutToShow,this,[this]{showFavourites();});row->addWidget(favouriteButton);row->addWidget(trackWidthBox,1);settings->addRow(ui("Breite"),row);}
    connect(trackWidthBox,&QDoubleSpinBox::valueChanged,this,[this](double v){board->trackWidth=v;if(refreshing)return;
        QList<int> s;for(int i:board->selection()){const auto t=doc.board().elements[i].type;if(t==ElementType::Track||t==ElementType::Area||t==ElementType::Circle)s.append(i);}
        if(!s.isEmpty())editElements(s,[v](Element &e){e.width=v;});});
    shapeBox=new QComboBox;shapeBox->addItems(shapeNames());shapeBox->setObjectName("padShape");settings->addRow(ui("Form"),shapeBox);
    padBox=spin(board->padDiameter,.05,50,2);padBox->setObjectName("padDiameter");drillBox=spin(board->padDrill,0,50,2);drillBox->setObjectName("padDrill");
    // Favourite sizes on a button before the fields, as for the track widths.
    auto sizeButton=[this](SizeKind kind,const char *name,const QString &tip){
        auto *button=new QToolButton;button->setObjectName(name);button->setIcon(pcbIcon("pcb-favourites"));button->setToolTip(tip);button->setPopupMode(QToolButton::InstantPopup);
        button->setMenu(new QMenu(button));connect(button->menu(),&QMenu::aboutToShow,this,[this,kind]{showSizeFavourites(kind);});return button;};
    padFavouriteButton=sizeButton(PadSizes,"padFavourites",ui("Bevorzugte Lötaugen"));
    {auto *row=new QHBoxLayout;row->setSpacing(2);row->addWidget(padFavouriteButton);row->addWidget(padBox,1);settings->addRow(ui("Lötauge Ø"),row);}
    settings->addRow(ui("Bohrung Ø"),drillBox);
    viaBox=new QCheckBox(ui("Durchkontaktierung"));settings->addRow(viaBox);
    smdWidthBox=spin(board->smdWidth,.05,50,2);smdHeightBox=spin(board->smdHeight,.05,50,2);smdWidthBox->setObjectName("smdWidth");smdHeightBox->setObjectName("smdHeight");
    smdFavouriteButton=sizeButton(SmdSizes,"smdFavourites",ui("Bevorzugte SMD-Pads"));
    {auto *row=new QHBoxLayout;row->setSpacing(2);row->addWidget(smdFavouriteButton);row->addWidget(smdWidthBox,1);settings->addRow(ui("SMD Breite"),row);}
    {auto *row=new QHBoxLayout;row->setSpacing(2);auto *swap=new QToolButton;swap->setObjectName("smdSwap");swap->setText(QStringLiteral("⇄"));
        swap->setToolTip(ui("Breite und Höhe tauschen"));connect(swap,&QToolButton::clicked,this,[this]{swapSmdSize();});
        row->addWidget(swap);row->addWidget(smdHeightBox,1);settings->addRow(ui("SMD Höhe"),row);}
    filledBox=new QCheckBox(ui("Rechteck gefüllt"));settings->addRow(filledBox);
    auto padSettings=[this]{
        board->padShape=PadShape(shapeBox->currentIndex()+1);board->padDiameter=padBox->value();board->padDrill=drillBox->value();board->padVia=viaBox->isChecked();board->update();
        if(refreshing)return;QList<int> s;for(int i:board->selection())if(doc.board().elements[i].type==ElementType::Pad)s.append(i);
        if(!s.isEmpty())editElements(s,[this](Element &e){e.shape=board->padShape;e.size=board->padDiameter;e.size2=board->padDrill;e.via=board->padVia;updateOutline(e);});
    };
    padSettingsChanged=padSettings;
    connect(shapeBox,&QComboBox::currentIndexChanged,this,padSettings);connect(padBox,&QDoubleSpinBox::valueChanged,this,padSettings);
    connect(drillBox,&QDoubleSpinBox::valueChanged,this,padSettings);connect(viaBox,&QCheckBox::toggled,this,padSettings);
    auto smdSettings=[this]{
        board->smdWidth=smdWidthBox->value();board->smdHeight=smdHeightBox->value();board->update();if(refreshing)return;
        QList<int> s;for(int i:board->selection())if(doc.board().elements[i].type==ElementType::SmdPad)s.append(i);
        if(!s.isEmpty())editElements(s,[this](Element &e){e.size=board->smdWidth;e.size2=board->smdHeight;updateOutline(e);});
    };
    connect(smdWidthBox,&QDoubleSpinBox::valueChanged,this,smdSettings);connect(smdHeightBox,&QDoubleSpinBox::valueChanged,this,smdSettings);
    smdSettingsChanged=smdSettings;
    connect(filledBox,&QCheckBox::toggled,this,[this](bool on){board->filledRectangle=on;});
    v->addLayout(settings);
    overview=new BoardOverview(board);v->addSpacing(6);v->addWidget(overview);v->addStretch();
    action("overview")->setChecked(true);
    board->viewChanged=[this]{overview->update();if(auto *a=action("zoomPrevious"))a->setEnabled(board->canZoomBack());};
    panel->setMaximumWidth(220);refreshToolKeys();return panel;
}
// The autorouter's options above the board, as the reference shows them in its autoroute mode.
QWidget *Editor::createAutorouteBar(){
    autorouteBar=new QFrame;autorouteBar->setFrameShape(QFrame::StyledPanel);autorouteBar->setObjectName("autorouteBar");auto *h=new QHBoxLayout(autorouteBar);h->setContentsMargins(6,2,6,2);
    auto *width=spin(board->autorouteWidth,.05,20,2),*distance=spin(board->autorouteClearance,0,20,2);auto *onGrid=new QCheckBox(ui("Am Raster orientieren"));onGrid->setChecked(board->autorouteOnGrid);
    auto *gridText=new QLabel;auto *status=new QLabel;status->setObjectName("autorouteStatus");
    h->addWidget(new QLabel(ui("Breite der Leiterbahn")));h->addWidget(width);h->addSpacing(12);h->addWidget(new QLabel(ui("Mindestabstand")));h->addWidget(distance);
    h->addSpacing(12);h->addWidget(onGrid);h->addWidget(gridText);h->addSpacing(12);h->addWidget(status,1);
    connect(width,&QDoubleSpinBox::valueChanged,this,[this](double v){board->autorouteWidth=v;});
    connect(distance,&QDoubleSpinBox::valueChanged,this,[this](double v){board->autorouteClearance=v;});
    connect(onGrid,&QCheckBox::toggled,this,[this](bool on){board->autorouteOnGrid=on;});
    board->autorouteStatus=[status](const QString &text){status->setText(text);};
    // The raster the router uses, shown next to its switch.
    autorouteGrid=[this,gridText]{if(!doc.boards.isEmpty())gridText->setText(ui("Raster: %1 mm").arg(uiLocale().toString(doc.board().grid)));};
    autorouteBar->setVisible(false);return autorouteBar;
}
QWidget *Editor::createPhotoBar(){
    // The photo view's options above the board while it is on, as the reference offers them: the side looked at, the
    // silkscreen, the translucent board, the board's colour and the finish of the pads.
    photoBar=new QFrame;photoBar->setFrameShape(QFrame::StyledPanel);photoBar->setObjectName("photoBar");auto *h=new QHBoxLayout(photoBar);h->setContentsMargins(6,2,6,2);
    auto *top=new QRadioButton(ui("von oben (K1, B1)"));top->setObjectName("photoTop");top->setChecked(true);
    auto *bottom=new QRadioButton(ui("von unten (K2, B2, gespiegelt)"));bottom->setObjectName("photoBottom");
    auto *silk=new QCheckBox(ui("Bestückungsdruck zeigen"));silk->setObjectName("photoSilk");silk->setChecked(board->photoSilk);
    auto *translucent=new QCheckBox(ui("durchscheinend"));translucent->setObjectName("photoTranslucent");translucent->setChecked(board->photoTranslucent);
    auto *colour=new QComboBox;colour->setObjectName("photoBoard");colour->addItems({ui("grün"),ui("blau"),ui("kupferfarben")});
    auto *finish=new QComboBox;finish->setObjectName("photoFinish");finish->addItems({ui("goldfarben"),ui("silberfarben"),ui("blankes Kupfer")});
    h->addWidget(top);h->addWidget(bottom);h->addSpacing(12);h->addWidget(silk);h->addWidget(translucent);h->addSpacing(12);
    h->addWidget(new QLabel(ui("Platine")));h->addWidget(colour);h->addSpacing(8);h->addWidget(new QLabel(ui("Pads")));h->addWidget(finish);h->addStretch();
    connect(bottom,&QRadioButton::toggled,this,[this](bool on){if(action("fromBelow")->isChecked()!=on)action("fromBelow")->trigger();});
    connect(action("fromBelow"),&QAction::toggled,photoBar,[top,bottom](bool on){(on?bottom:top)->setChecked(true);});
    connect(silk,&QCheckBox::toggled,this,[this](bool on){board->photoSilk=on;board->update();});
    connect(translucent,&QCheckBox::toggled,this,[this](bool on){board->photoTranslucent=on;board->update();});
    connect(colour,&QComboBox::currentIndexChanged,this,[this](int i){board->photoBoard=i;board->update();});
    connect(finish,&QComboBox::currentIndexChanged,this,[this](int i){board->photoFinish=i;board->update();});
    photoBar->setVisible(false);return photoBar;
}
QWidget *Editor::createLayerBar(){
    auto *bar=new QFrame;bar->setFrameShape(QFrame::StyledPanel);auto *h=new QHBoxLayout(bar);h->setContentsMargins(6,2,6,2);
    coordinates=new QLabel;coordinates->setObjectName("coordinates");coordinates->setMinimumWidth(240);h->addWidget(coordinates);
    auto *grid=new QGridLayout;grid->setHorizontalSpacing(8);grid->setVerticalSpacing(0);
    grid->addWidget(new QLabel(ui("sichtbar")),0,0);grid->addWidget(new QLabel(ui("aktiv")),1,0);
    auto *active=new QButtonGroup(bar);active->setExclusive(true);
    const QColor colours[]={{},{29,106,249},{255,0,0},{0,186,1},{200,190,0},{194,124,21},{218,160,70},{120,120,120}};
    for(int layer:layerOrder()){
        auto *visible=new QCheckBox(layerName(layer));visible->setObjectName("visible-"+QString::number(layer));
        visible->setStyleSheet(QString("QCheckBox{color:%1;font-weight:bold;}").arg(colours[layer].name()));
        auto *on=new QRadioButton;on->setObjectName("active-"+QString::number(layer));active->addButton(on,layer);
        const int column=int(layerVisible.size())+1;grid->addWidget(visible,0,column);grid->addWidget(on,1,column,Qt::AlignHCenter);
        layerVisible[layer]=visible;layerActive[layer]=on;
        connect(visible,&QCheckBox::toggled,this,[this,layer](bool shown){
            if(refreshing)return;doc.board().visible[layer]=shown;modified=true;revision++;board->update();if(titleChanged)titleChanged();});
    }
    connect(active,&QButtonGroup::idClicked,this,[this](int layer){
        if(refreshing)return;auto &b=doc.board();b.activeLayer=layer;b.visible[layer]=true;modified=true;revision++;refreshLayers();board->update();if(titleChanged)titleChanged();});
    h->addLayout(grid);
    {auto *info=new QToolButton;info->setObjectName("layerInfo");info->setText(QStringLiteral("?"));info->setAutoRaise(true);info->setToolTip(ui("Was die Layer bedeuten"));
        connect(info,&QToolButton::clicked,this,[this]{showLayerInfo();});h->addWidget(info);}
    // AutoMasse of the active copper layer and the clearance new elements get, as the reference offers them below.
    groundBox=new QCheckBox(ui("AutoMasse"));groundBox->setObjectName("groundPlane");h->addSpacing(16);h->addWidget(groundBox);
    connect(groundBox,&QCheckBox::toggled,this,[this](bool on){if(!refreshing)setGroundPlane(on);});
    clearanceBox=spin(board->clearance,0,10,2);clearanceBox->setObjectName("clearance");clearanceBox->setToolTip(ui("Freistanzung neuer Elemente zur AutoMasse"));
    h->addWidget(new QLabel(ui("Freistanzung")));h->addWidget(clearanceBox);
    connect(clearanceBox,&QDoubleSpinBox::valueChanged,this,[this](double v){board->clearance=v;});
    // Keep-out areas as rectangles instead of polygons.
    {auto *rect=new QToolButton;rect->setObjectName("keepoutRectangle");rect->setAutoRaise(true);rect->setCheckable(true);rect->setIcon(pcbIcon("pcb-keepout-rect"));
        rect->setToolTip(ui("Sperrflächen als Rechteck aufziehen"));connect(rect,&QToolButton::toggled,this,[this](bool on){board->keepoutRectangle=on;});h->addWidget(rect);}
    // The template: its dialog, the template alone, the template hidden.
    {h->addSpacing(16);auto *open=new QToolButton;open->setObjectName("templateButton");open->setAutoRaise(true);open->setDefaultAction(action("templates"));h->addWidget(open);
        auto *alone=new QToolButton;alone->setObjectName("templateOnly");alone->setCheckable(true);alone->setAutoRaise(true);alone->setText(ui("nur Vorlage"));
        alone->setToolTip(ui("Nur die Vorlage zeigen, alle Elemente ausblenden"));
        auto *hidden=new QToolButton;hidden->setObjectName("templateHidden");hidden->setCheckable(true);hidden->setAutoRaise(true);hidden->setText(ui("Vorlage aus"));
        hidden->setToolTip(ui("Die Vorlage ausblenden"));h->addWidget(alone);h->addWidget(hidden);
        connect(alone,&QToolButton::toggled,this,[this](bool on){board->templateOnly=on;board->update();});
        connect(hidden,&QToolButton::toggled,this,[this](bool on){board->templateHidden=on;board->update();});}
    // Rubber band in three steps and automatic snapping, as switches in the bar like in the reference.
    auto *rubber=new QToolButton;rubber->setObjectName("rubberBand");rubber->setAutoRaise(true);h->addSpacing(16);h->addWidget(rubber);
    auto showRubber=[this,rubber]{
        static const char *icons[]={"pcb-rubber-off","pcb-rubber-small","pcb-rubber-large"};
        const QString tips[]={ui("Gummiband: aus"),ui("Gummiband: kleiner Fang"),ui("Gummiband: großer Fang")};
        rubber->setIcon(pcbIcon(icons[board->rubberBand]));rubber->setToolTip(tips[board->rubberBand]);
    };
    showRubber();connect(rubber,&QToolButton::clicked,this,[this,showRubber]{board->rubberBand=(board->rubberBand+2)%3;showRubber();});
    auto *snap=new QToolButton;snap->setObjectName("autoSnap");snap->setAutoRaise(true);snap->setCheckable(true);snap->setChecked(board->autoSnap);
    snap->setIcon(pcbIcon("pcb-snap"));snap->setToolTip(ui("Automatischer Fangmodus"));h->addWidget(snap);
    connect(snap,&QToolButton::toggled,this,[this](bool on){board->autoSnap=on;board->update();});
    auto *cross=new QToolButton;cross->setObjectName("crosshairButton");cross->setAutoRaise(true);cross->setDefaultAction(action("crosshair"));h->addWidget(cross);
    // What the tool does and which keys change it, as the reference's status line shows them.
    helpLine=new QLabel;helpLine->setObjectName("helpLine");helpLine->setWordWrap(true);helpLine->setMinimumWidth(200);h->addSpacing(16);h->addWidget(helpLine,1);
    board->bendChanged=[this]{refreshHelpLine();};refreshHelpLine();
    return bar;
}
QWidget *Editor::createSidePanel(){
    auto *side=new QTabWidget;side->setMinimumWidth(240);side->setMaximumWidth(320);sidePanel=side;
    auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);properties=new QWidget;propertyForm=new QFormLayout(properties);
    propertyForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);scroll->setWidget(properties);side->addTab(scroll,ui("Eigenschaften"));
    auto *parts=new QWidget;auto *v=new QVBoxLayout(parts);library=new QListWidget;library->setObjectName("library");
    for(const auto &f:footprints()){auto *item=new QListWidgetItem(f.name);item->setToolTip(f.source);library->addItem(item);}
    auto *place=new QPushButton(ui("Platzieren"));auto *macro=new QPushButton(ui("Makro laden…"));
    v->addWidget(library,1);v->addWidget(place);v->addWidget(macro);side->addTab(parts,ui("Bauteile"));
    // The macro library of the user's own folder.
    macros=new MacroPanel;side->addTab(macros,ui("Makros"));
    macros->place=[this](const QList<Element> &els){placeMacro(els,macros->asComponent);};
    macros->saveSelection=[this]{saveMacroAs(macros->currentFolder());};
    macros->chooseFolder=[this]{
        const auto chosen=QFileDialog::getExistingDirectory(this,ui("Makroordner wählen"),macroFolder.isEmpty()?startFolder(Layouts):macroFolder);if(chosen.isEmpty())return;
        macroFolder=chosen;preferences().setValue("macroFolder",macroFolder);macros->setFolders(macroFolder,extraMacroFolders);};
    board->dropElements=[this](const QMimeData *data){
        return data->hasFormat(MacroPanel::dragFormat())?macroElements(macros->macro(),macros->asComponent):QList<Element>{};};
    board->dropped=[this](const QList<int> &added){if(macros->asComponent)macroPlaced(added);};
    connect(library,&QListWidget::itemActivated,this,[this](QListWidgetItem *item){placeFootprint(library->row(item));});
    connect(place,&QPushButton::clicked,this,[this]{if(library->currentRow()>=0)placeFootprint(library->currentRow());});
    connect(macro,&QPushButton::clicked,this,[this]{importMacro();});
    // Design rule check: each check with its switch (and limit), checking the whole board or only what the view shows, the
    // findings. Picking findings in the list shows their marks on the board, a click selects the elements of one, a double
    // click zooms to it.
    auto *drc=new QWidget;auto *d=new QVBoxLayout(drc);const Rules defaults;
    auto *distances=new QGroupBox(ui("Mindestabstand"));auto *df=new QGridLayout(distances);auto *limits=new QGroupBox(ui("Grenzwerte"));auto *lf=new QGridLayout(limits);
    auto *others=new QGroupBox(ui("Weitere Prüfungen"));auto *of=new QVBoxLayout(others);
    auto rule=[this](QGridLayout *grid,const QString &key,const QString &label,bool on,double value){
        auto *c=new QCheckBox(label);c->setChecked(on);c->setObjectName("check-"+key);auto *box=spin(value,0,100,2);box->setObjectName("rule-"+key);box->setEnabled(on);box->setFixedWidth(80);
        const int row=grid->rowCount();grid->addWidget(c,row,0);grid->addWidget(box,row,1);ruleChecks[key]=c;ruleBoxes[key]=box;connect(c,&QCheckBox::toggled,box,&QWidget::setEnabled);};
    rule(df,"clearance",ui("Kupfer"),defaults.clearanceOn,defaults.clearance);rule(df,"holeDistance",ui("Bohrungen"),defaults.holeDistanceOn,defaults.holeDistance);
    rule(lf,"minDrill",ui("Bohrung ab"),defaults.minDrillOn,defaults.minDrill);rule(lf,"maxDrill",ui("Bohrung bis"),defaults.maxDrillOn,defaults.maxDrill);
    rule(lf,"minTrack",ui("Leiterbahn ab"),defaults.minTrackOn,defaults.minTrack);rule(lf,"minRing",ui("Restring ab"),defaults.minRingOn,defaults.minRing);
    rule(lf,"minSilk",ui("Bestückung ab"),defaults.minSilkOn,defaults.minSilk);
    for(const auto &[key,label,on]:QList<std::tuple<QString,QString,bool>>{{"silkOnPads",ui("Bestückung auf Pads"),defaults.silkOnPads},{"holesOnSmd",ui("Bohrungen auf SMD-Pads"),defaults.holesOnSmd},
            {"padsWithoutMask",ui("Pads ohne Lötstoppöffnung"),defaults.padsWithoutMask},{"maskOutsidePads",ui("Lötstoppöffnung neben Pads"),defaults.maskOutsidePads}}){
        auto *c=new QCheckBox(label);c->setChecked(on);c->setObjectName("check-"+key);of->addWidget(c);ruleChecks[key]=c;}
    d->addWidget(distances);d->addWidget(limits);d->addWidget(others);
    auto *starts=new QVBoxLayout;auto *whole=new QPushButton(ui("Ganze Platine…"));auto *shown=new QPushButton(ui("Sichtbarer Bereich…"));
    whole->setObjectName("drcWhole");shown->setObjectName("drcVisible");whole->setToolTip(ui("Prüft die ganze Platine"));shown->setToolTip(ui("Prüft nur, was die Ansicht gerade zeigt"));
    starts->addWidget(whole);starts->addWidget(shown);d->addLayout(starts);findingSummary=new QLabel;findingSummary->setWordWrap(true);d->addWidget(findingSummary);
    findingList=new QListWidget;findingList->setObjectName("findings");findingList->setSelectionMode(QAbstractItemView::ExtendedSelection);d->addWidget(findingList,1);
    auto *showAll=new QPushButton(ui("Alle Fundstellen zeigen"));showAll->setObjectName("findingsAll");d->addWidget(showAll);
    auto *drcScroll=new QScrollArea;drcScroll->setObjectName("drcPage");drcScroll->setWidgetResizable(true);drcScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);drcScroll->setWidget(drc);side->addTab(drcScroll,ui("DRC"));
    connect(whole,&QPushButton::clicked,this,[this]{runDesignRuleCheck(false);});connect(shown,&QPushButton::clicked,this,[this]{runDesignRuleCheck(true);});
    connect(showAll,&QPushButton::clicked,findingList,&QListWidget::selectAll);
    connect(findingList,&QListWidget::itemSelectionChanged,this,[this]{
        QList<int> rows;for(auto *item:findingList->selectedItems())rows.append(findingList->row(item));std::sort(rows.begin(),rows.end());board->setShownFindings(rows);});
    connect(findingList,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){board->showFinding(findingList->row(item));});
    connect(findingList,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *item){board->zoomToFinding(findingList->row(item));});
    // The comparison with the project's schematic, shown only when there is one: a line per finding; a click selects
    // its elements and centres them, a double click zooms to them.
    {schematicPage=new QWidget;schematicPage->setObjectName("schematicPage");auto *sl=new QVBoxLayout(schematicPage);
        auto *again=new QPushButton(ui("Mit Schaltplan vergleichen"));again->setObjectName("schematicCompare");sl->addWidget(again);
        auto *wires=new QCheckBox(ui("Luftlinien aus dem Schaltplan"));wires->setObjectName("schematicAirwiresBox");sl->addWidget(wires);
        schematicList=new QListWidget;schematicList->setObjectName("schematicResults");schematicList->setWordWrap(true);sl->addWidget(schematicList,1);
        side->addTab(schematicPage,ui("Schaltplan"));side->setTabVisible(side->indexOf(schematicPage),false);
        connect(again,&QPushButton::clicked,this,[this]{compareWithSchematic(true);});
        connect(wires,&QCheckBox::toggled,this,[this](bool on){if(action("schematicAirwires")->isChecked()!=on)action("schematicAirwires")->trigger();});
        connect(action("schematicAirwires"),&QAction::toggled,wires,[wires](bool on){const QSignalBlocker quiet(wires);wires->setChecked(on);});
        auto indexes=[this](QListWidgetItem *item){QList<int> out;for(const auto &v:item->data(Qt::UserRole+1).toList())if(v.toInt()>=0&&v.toInt()<doc.board().elements.size())out<<v.toInt();return out;};
        connect(schematicList,&QListWidget::itemClicked,this,[this,indexes](QListWidgetItem *item){
            const auto found=indexes(item);if(!found.isEmpty())board->setSelection(found);const auto at=item->data(Qt::UserRole);if(at.isValid())board->centreOn(at.toPointF());});
        connect(schematicList,&QListWidget::itemDoubleClicked,this,[this,indexes](QListWidgetItem *item){
            QRectF r;for(int i:indexes(item))r=r.united(bounds(doc.board().elements[i]));if(!r.isNull())board->showArea(r);});
        connect(side,&QTabWidget::currentChanged,this,[this]{if(sidePanel->currentWidget()==schematicPage)refreshSchematic();});}
    // The component list: a row per component; picking one marks it on the board, a double click edits it. The
    // switches below show or hide the optional columns.
    auto *listPage=new QWidget;auto *lv=new QVBoxLayout(listPage);componentTree=new QTreeWidget;componentTree->setObjectName("components");
    componentTree->setRootIsDecorated(false);componentTree->setUniformRowHeights(true);componentTree->setSelectionMode(QAbstractItemView::SingleSelection);
    componentTree->setHeaderLabels({ui("Nr."),ui("Bezeichner"),ui("Wert"),ui("Gehäuse"),ui("Kommentar"),ui("Seite"),ui("X"),ui("Y"),ui("Drehung")});
    lv->addWidget(componentTree,1);
    // The columns as wide as their contents, as the reference's bar above the switches does it.
    {auto *fit=new QPushButton(ui("Spaltenbreiten anpassen"));fit->setObjectName("fitColumns");lv->addWidget(fit);
        connect(fit,&QPushButton::clicked,this,[this]{for(int c=0;c<componentTree->columnCount();c++)if(!componentTree->isColumnHidden(c))componentTree->resizeColumnToContents(c);});}
    auto *columns=new QGridLayout;lv->addLayout(columns);
    const QList<std::pair<QString,QList<int>>> optional{{ui("Laufende Nummer"),{0}},{ui("Gehäuse"),{3}},{ui("Kommentar"),{4}},{ui("Seite"),{5}},{ui("Position"),{6,7}},{ui("Drehung"),{8}}};
    for(int k=0;k<optional.size();k++){
        auto *box=new QCheckBox(optional[k].first);box->setObjectName(QStringLiteral("componentColumn-%1").arg(k));box->setChecked(k!=0&&k!=2);columns->addWidget(box,k/3,k%3);
        for(int c:optional[k].second)componentTree->setColumnHidden(c,!box->isChecked());
        connect(box,&QCheckBox::toggled,this,[this,cols=optional[k].second](bool on){for(int c:cols)componentTree->setColumnHidden(c,!on);});
    }
    {auto *exportButton=new QPushButton(ui("Export…"));exportButton->setObjectName("exportComponents");lv->addWidget(exportButton);
        connect(exportButton,&QPushButton::clicked,this,[this]{componentDataDialog();});}
    side->addTab(listPage,ui("Bauteilliste"));
    // The selector: elements of one kind in groups by a property; a group marks all its elements and zooms to them.
    auto *selectorPage=new QWidget;auto *sv=new QVBoxLayout(selectorPage);auto *choices=new QFormLayout;sv->addLayout(choices);
    // As in the reference, through-plated pads have an entry of their own after all pads.
    selectorType=new QComboBox;selectorType->setObjectName("selectorType");
    selectorType->addItems({ui("Lötaugen"),ui("Durchkontaktierungen"),ui("SMD-Pads"),ui("Leiterbahnen"),ui("Kreisringe"),ui("Flächen"),ui("Texte")});
    selectorProperty=new QComboBox;selectorProperty->setObjectName("selectorProperty");selectorLayers=new QComboBox;selectorLayers->setObjectName("selectorLayers");
    selectorLayers->addItems({ui("alle Layer"),ui("Kupfer"),ui("Bestückung und Umriss")});
    choices->addRow(ui("Elemente"),selectorType);choices->addRow(ui("Sortierung"),selectorProperty);choices->addRow(ui("Layer"),selectorLayers);
    selectorTree=new QTreeWidget;selectorTree->setObjectName("selector");selectorTree->setHeaderHidden(true);sv->addWidget(selectorTree,1);side->addTab(selectorPage,ui("Selector"));
    auto properties=[this]{
        static const QList<QStringList> names{{ui("Durchmesser"),ui("Bohrung"),ui("Form"),ui("Durchkontaktierung")},{ui("Durchmesser"),ui("Bohrung"),ui("Form")},{ui("Größe")},{ui("Breite")},
                                              {ui("Radius"),ui("Breite")},{ui("Randbreite")},{ui("Höhe"),ui("Text")}};
        const bool was=refreshing;refreshing=true;selectorProperty->clear();selectorProperty->addItems(names.value(selectorType->currentIndex()));refreshing=was;
    };
    properties();
    connect(selectorType,&QComboBox::currentIndexChanged,this,[this,properties]{properties();refreshSelector();});
    for(auto *box:{selectorProperty,selectorLayers})connect(box,&QComboBox::currentIndexChanged,this,[this]{if(!refreshing)refreshSelector();});
    connect(selectorTree,&QTreeWidget::itemClicked,this,[this](QTreeWidgetItem *item){
        QList<int> indexes;for(const auto &v:item->data(0,Qt::UserRole).toList())indexes.append(v.toInt());if(indexes.isEmpty())return;
        board->setSelection(indexes);QRectF r;for(int i:indexes)r=r.united(bounds(doc.board().elements[i]));board->showArea(r);
    });
    connect(componentTree,&QTreeWidget::itemSelectionChanged,this,[this]{
        if(refreshing)return;const auto items=componentTree->selectedItems();if(items.isEmpty())return;
        const auto list=components(doc.board());const int row=items.first()->data(0,Qt::UserRole).toInt();if(row<0||row>=list.size())return;
        refreshing=true;board->setSelection(list[row].members);board->centreOn(componentCentre(doc.board(),list[row]));refreshing=false;
    });
    connect(componentTree,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *item){
        const auto list=components(doc.board());const int row=item->data(0,Qt::UserRole).toInt();if(row>=0&&row<list.size())editComponent(list[row].designator);
    });
    return side;
}

// --- document, undo and refresh
void Editor::setDocument(const Document &document,const QString &file){
    doc=document;if(doc.boards.isEmpty())doc.boards.append(newBoard(ui("Platine 1")));doc.activeBoard=qBound(0,doc.activeBoard,int(doc.boards.size())-1);
    assignIds(doc);
    path=file;importedFile.clear();modified=false;revision=backupRevision=0;past.clear();future.clear();board->setDocument(&doc);refresh();if(titleChanged)titleChanged();
    notes.clear();if(noticeBar)noticeBar->hide();
}
void Editor::snapshot(){board->cancelDrag();past.append(doc);while(past.size()>undoLimit)past.removeFirst();future.clear();}
void Editor::touched(){
    modified=true;revision++;
    // Rebuilt later: the change may come from a field of the property panel itself.
    QTimer::singleShot(0,this,[this]{refreshProperties();refreshActions();refreshComponents();refreshSelector();refreshSchematic();});
    if(titleChanged)titleChanged();
}
void Editor::undo(){board->cancelDrag();if(past.isEmpty())return;future.append(doc);doc=past.takeLast();modified=true;revision++;board->documentChanged();refresh();if(titleChanged)titleChanged();}
void Editor::redo(){board->cancelDrag();if(future.isEmpty())return;past.append(doc);doc=future.takeLast();modified=true;revision++;board->documentChanged();refresh();if(titleChanged)titleChanged();}
void Editor::refreshSelector(){
    if(!selectorTree)return;
    static const ElementType types[]={ElementType::Pad,ElementType::Pad,ElementType::SmdPad,ElementType::Track,ElementType::Circle,ElementType::Area,ElementType::Text};
    const int kind=std::clamp(selectorType->currentIndex(),0,6);const auto &b=doc.board();
    const auto groups=selectorGroups(b,types[kind],std::max(0,selectorProperty->currentIndex()),selectorLayers->currentIndex(),kind==1);
    selectorTree->clear();
    for(const auto &g:groups){
        QVariantList all;for(int i:g.elements)all.append(i);
        auto *group=new QTreeWidgetItem({QStringLiteral("%1  (%2)").arg(g.label).arg(g.elements.size())});group->setData(0,Qt::UserRole,all);selectorTree->addTopLevelItem(group);
        for(int i:g.elements){const auto &e=b.elements[i];const QPointF at=e.type==ElementType::Track||e.type==ElementType::Area?e.points.value(0):e.pos;
            auto *item=new QTreeWidgetItem({ui("X %1  Y %2").arg(uiLocale().toString(at.x()-b.origin.x(),'f',2),uiLocale().toString(b.origin.y()-at.y(),'f',2))});
            item->setData(0,Qt::UserRole,QVariantList{i});group->addChild(item);}
    }
}
void Editor::refreshComponents(){
    if(!componentTree)return;const bool was=refreshing;refreshing=true;
    const auto &b=doc.board();const auto list=components(b);componentTree->clear();
    auto number=[](double v){return uiLocale().toString(v,'f',2);};
    for(int k=0;k<list.size();k++){
        const auto &c=list[k];const auto &id=b.elements[c.designator];const QPointF at=pickPlaceCentre(b,c);
        const bool top=id.layer==SilkTop||id.layer==CopperTop;
        auto *item=new QTreeWidgetItem({QString::number(k+1),id.text,c.value>=0?b.elements[c.value].text:QString(),id.package,id.comment,top?ui("oben"):ui("unten"),
                                        number(at.x()-b.origin.x()),number(b.origin.y()-at.y()),number(id.componentRotation)});
        item->setData(0,Qt::UserRole,k);componentTree->addTopLevelItem(item);
    }
    refreshing=was;showSelectedComponent();
}
// The row of the component that is marked on the board, if the marking is one component.
void Editor::showSelectedComponent(){
    if(!componentTree)return;const bool was=refreshing;refreshing=true;
    const auto sel=board->selection();const auto list=components(doc.board());int row=-1;
    for(int k=0;k<list.size()&&!sel.isEmpty();k++){bool inside=true;for(int i:sel)inside&=list[k].members.contains(i);if(inside&&sel.contains(list[k].designator)){row=k;break;}}
    componentTree->clearSelection();if(row>=0)if(auto *item=componentTree->topLevelItem(row)){item->setSelected(true);componentTree->scrollToItem(item);}
    refreshing=was;
}
bool Editor::nameSelection(const QString &name){
    const auto sel=board->selection();if(sel.isEmpty())return false;
    editElements(sel,[&name](Element &e){e.name=name;});return true;
}
QString Editor::nameToSelect() const{
    const auto &els=doc.board().elements;const int hit=board->contextHit();
    if(hit>=0&&hit<els.size()&&board->selection().contains(hit)&&!els[hit].name.isEmpty())return els[hit].name;
    auto sel=board->selection();std::sort(sel.begin(),sel.end());for(int i:sel)if(i<els.size()&&!els[i].name.isEmpty())return els[i].name;
    return {};
}
bool Editor::selectByName(const QString &name){
    if(name.isEmpty())return false;const auto &b=doc.board();QList<int> found;
    for(int i=0;i<b.elements.size();i++)if(b.elements[i].name==name&&(b.visible[b.elements[i].layer]||(b.elements[i].type==ElementType::Pad&&b.elements[i].via)))found.append(i);
    board->setSelection(found);return !found.isEmpty();
}
bool Editor::setSelectionLayer(int layer){
    const auto sel=board->selection();if(sel.isEmpty()||layer<1||layer>layerCount)return false;
    editElements(sel,[layer](Element &e){
        if(e.type==ElementType::Pad&&!isCopper(layer))return;
        if(e.type==ElementType::SmdPad&&layer!=CopperTop&&layer!=CopperBottom)return;
        e.layer=layer;if(e.type==ElementType::Text)updateStrokes(e);});
    return true;
}
bool Editor::setOrigin(QPointF at){
    if(doc.board().origin==at)return false;snapshot();doc.board().origin=at;board->documentChanged();touched();return true;
}
int Editor::deleteOutside(){
    const auto &b=doc.board();const QRectF area(0,0,b.width,b.height);QList<int> outside;
    for(int i=0;i<b.elements.size();i++){const QRectF r=bounds(b.elements[i]);if(!r.intersects(area)&&!area.contains(r.topLeft()))outside.append(i);}
    if(outside.isEmpty())return 0;
    snapshot();removeElements(doc.board(),outside);board->setSelection({});board->documentChanged();touched();return int(outside.size());
}
bool Editor::editComponent(int designator){
    const auto &b=doc.board();if(designator<0||designator>=b.elements.size()||b.elements[designator].role!=TextRole::Designator)return false;
    int value=-1;for(const auto &c:components(b))if(c.designator==designator)value=c.value;
    const Element id=b.elements[designator];
    QDialog dialog(this);dialog.setWindowTitle(ui("Bauteil"));auto *form=new QFormLayout(&dialog);
    // Designator and value each with its layer and whether it shows, as the reference offers them.
    QList<int> layers;for(int l:layerOrder())if(b.multilayer||(l!=Inner1&&l!=Inner2))layers.append(l);
    // An inner layer left over from a multilayer board stays in the list, so confirming keeps it.
    auto layerBox=[&](int current,const char *objectName){auto *box=new QComboBox;box->setObjectName(objectName);auto offered=layers;if(!offered.contains(current))offered.prepend(current);
        for(int l:offered)box->addItem(layerName(l),l);box->setCurrentIndex(std::max(0,int(offered.indexOf(current))));return box;};
    auto *name=new QLineEdit(id.text);name->setObjectName("componentName");auto *nameLayer=layerBox(id.layer,"nameLayer");auto *nameShown=new QCheckBox(ui("Sichtbar"));nameShown->setChecked(id.visible);
    auto *nameRow=new QHBoxLayout;nameRow->addWidget(name,1);nameRow->addWidget(nameLayer);nameRow->addWidget(nameShown);form->addRow(ui("Bezeichner"),nameRow);
    QLineEdit *valueText=nullptr;QCheckBox *valueShown=nullptr;QComboBox *valueLayer=nullptr;
    if(value>=0){valueText=new QLineEdit(b.elements[value].text);valueLayer=layerBox(b.elements[value].layer,"valueLayer");valueShown=new QCheckBox(ui("Sichtbar"));valueShown->setChecked(b.elements[value].visible);
        auto *row=new QHBoxLayout;row->addWidget(valueText,1);row->addWidget(valueLayer);row->addWidget(valueShown);form->addRow(ui("Wert"),row);}
    // The look of both texts: height, style and stroke, and whether they are turned to read upright.
    auto *height=spin(id.size,.1,9.9,2);height->setObjectName("componentTextHeight");
    auto *style=new QComboBox;style->setObjectName("componentTextStyle");style->addItems({ui("eng"),ui("normal"),ui("weit")});style->setCurrentIndex(std::clamp(id.style,0,2));
    auto *stroke=new QComboBox;stroke->setObjectName("componentTextStroke");stroke->addItems({ui("dünn"),ui("normal"),ui("dick")});stroke->setCurrentIndex(std::clamp(id.thickness,0,2));
    {auto *row=new QHBoxLayout;row->addWidget(height);row->addWidget(style);row->addWidget(stroke);form->addRow(ui("Texte"),row);}
    auto *upright=new QCheckBox(ui("Bezeichner und Wert lesbar ausrichten"));upright->setObjectName("alignTexts");form->addRow(upright);
    auto *package=new QLineEdit(id.package);auto *comment=new QLineEdit(id.comment);package->setMaxLength(255);comment->setMaxLength(255);
    auto *angle=spin(id.componentRotation,-360,360,1,QStringLiteral("°"));angle->setObjectName("componentAngle");
    auto *pickAndPlace=new QCheckBox(ui("Pick+Place-Daten"));pickAndPlace->setChecked(id.pickAndPlace);
    {auto *row=new QHBoxLayout;row->addWidget(angle);
        for(int a:{0,90,180,270}){auto *quick=new QToolButton;quick->setText(QStringLiteral("%1°").arg(a));quick->setObjectName(QStringLiteral("angle%1").arg(a));row->addWidget(quick);
            connect(quick,&QToolButton::clicked,&dialog,[angle,a]{angle->setValue(a);});}
        form->addRow(ui("Gehäuse"),package);form->addRow(ui("Kommentar"),comment);form->addRow(ui("Bestückungswinkel"),row);form->addRow(pickAndPlace);}
    // The centre a placement machine takes: of the SMD pads, the silkscreen or both, moved by an offset.
    auto *centre=new QComboBox;centre->addItems({ui("Zentrum Kupfer"),ui("Zentrum Bestückung"),ui("Zentrum Kupfer + Bestückung")});centre->setCurrentIndex(std::clamp(id.pickCentre,0,2));
    // The offset as the reference shows it, y positive downwards (the model counts y upwards).
    auto *offsetX=spin(id.pickOffset.x(),-100,100,3),*offsetY=spin(0.0-id.pickOffset.y(),-100,100,3);auto *zero=new QPushButton(QStringLiteral("0/0"));
    offsetX->setObjectName("pickOffsetX");offsetY->setObjectName("pickOffsetY");
    auto *offsets=new QHBoxLayout;offsets->addWidget(new QLabel(QStringLiteral("X")));offsets->addWidget(offsetX);offsets->addWidget(new QLabel(QStringLiteral("Y")));
    offsets->addWidget(offsetY);offsets->addWidget(zero);form->addRow(ui("Mittelpunkt"),centre);form->addRow(ui("Offset"),offsets);
    connect(zero,&QPushButton::clicked,&dialog,[offsetX,offsetY]{offsetX->setValue(0);offsetY->setValue(0);});
    // Dissolving the component: its elements become plain ones, the texts plain texts.
    auto *dissolve=new QPushButton(ui("Bauteil auflösen"));dissolve->setObjectName("dissolveComponent");form->addRow(dissolve);
    constexpr int dissolved=2;connect(dissolve,&QPushButton::clicked,&dialog,[&dialog]{dialog.done(dissolved);});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    const int result=dialog.exec();
    if(result==dissolved)return dissolveComponent(designator);
    if(result!=QDialog::Accepted)return false;
    QList<int> indexes{designator};if(value>=0)indexes.append(value);
    const int k=stroke->currentIndex();const double size=limitTextSize?std::max(height->value(),minimumTextHeight(k)):height->value();
    editElements(indexes,[&](Element &e){
        if(e.role==TextRole::Designator){e.text=name->text();e.visible=nameShown->isChecked();e.package=package->text();e.comment=comment->text();e.layer=nameLayer->currentData().toInt();
            e.componentRotation=angle->value();e.pickAndPlace=pickAndPlace->isChecked();e.pickCentre=centre->currentIndex();e.pickOffset=QPointF(offsetX->value(),0.0-offsetY->value());}
        else if(valueText){e.text=valueText->text();e.visible=valueShown->isChecked();e.layer=valueLayer->currentData().toInt();}
        e.size=size;e.style=style->currentIndex();e.thickness=k;
        updateStrokes(e);
        // Upright as after turning, whatever the preference says.
        // Turning the text is no turn of the component: the angle and offset typed in stay.
        if(upright->isChecked()&&!e.mirrored){const double r=std::fmod(std::fmod(e.rotation,360.0)+360.0,360.0);
            if(r>90+1e-6&&r<=270+1e-6){const double turn=e.componentRotation;const QPointF offset=e.pickOffset;rotate(e,bounds(e).center(),180);e.componentRotation=turn;e.pickOffset=offset;}}
    });
    return true;
}
bool Editor::dissolveComponent(int designator){
    const auto &b=doc.board();QList<int> members;for(const auto &c:components(b))if(c.designator==designator)members=c.members;
    if(members.isEmpty())return false;
    editElements(members,[](Element &e){e.part=0;e.component.clear();e.pin.clear();if(e.type==ElementType::Text)e.role=TextRole::Plain;});
    return true;
}
void Editor::refresh(){refreshing=true;refreshTabs();refreshLayers();
    const double g=doc.board().grid;const int at=gridBox->findData(g);if(at>=0)gridBox->setCurrentIndex(at);else gridBox->setEditText(uiLocale().toString(g));
    refreshing=false;refreshProperties();refreshActions();refreshComponents();refreshSelector();refreshSchematic();board->update();}
void Editor::refreshTabs(){
    const bool was=refreshing;refreshing=true;
    while(tabs->count()>doc.boards.size())tabs->removeTab(tabs->count()-1);
    for(int i=0;i<doc.boards.size();i++){const QString name=doc.boards[i].name.isEmpty()?ui("Platine %1").arg(i+1):doc.boards[i].name;
        if(i<tabs->count())tabs->setTabText(i,name);else tabs->addTab(name);}
    tabs->setCurrentIndex(doc.activeBoard);refreshing=was;
}
void Editor::refreshLayers(){
    const bool was=refreshing;refreshing=true;const auto &b=doc.board();
    for(auto it=layerVisible.begin();it!=layerVisible.end();++it){
        it.value()->setChecked(b.visible[it.key()]);
        // The inner layers only on multilayer boards, as in the reference.
        const bool shown=b.multilayer||(it.key()!=Inner1&&it.key()!=Inner2);it.value()->setVisible(shown);layerActive[it.key()]->setVisible(shown);
    }
    if(auto *on=layerActive.value(b.activeLayer))on->setChecked(true);
    groundBox->setEnabled(isCopper(b.activeLayer));groundBox->setChecked(isCopper(b.activeLayer)&&b.groundPlane[b.activeLayer]);
    refreshing=was;
}
void Editor::refreshActions(){
    const bool schematic=schematicAvailable();
    for(const char *name:{"compareSchematic","schematicAirwires","takeOverSchematic","assignPins","placeMissing"})action(name)->setEnabled(schematic);
    if(sidePanel&&schematicPage)sidePanel->setTabVisible(sidePanel->indexOf(schematicPage),schematic);
    if(!schematic&&!board->schematicAirwires().isEmpty())board->setSchematicAirwires({});
    const bool any=!board->selection().isEmpty();
    for(const char *name:{"cut","copy","delete","duplicate","rotate","rotateBack","mirror","mirrorVertical","alignGrid","arrange","otherSide","setLayer","group","saveMacro"})action(name)->setEnabled(any);
    const bool several=board->selection().size()>1;for(const char *name:{"alignLeft","alignRight","alignTop","alignBottom","alignCentreX","alignCentreY"})action(name)->setEnabled(several);
    bool grouped=false;for(int i:board->selection())grouped|=!doc.board().elements[i].groups.isEmpty();action("ungroup")->setEnabled(grouped);
    action("undo")->setEnabled(canUndo());action("redo")->setEnabled(canRedo());action("removeBoard")->setEnabled(doc.boards.size()>1);
    for(const char *name:{"millingWide","removeMilling"})action(name)->setEnabled(board->hasMillingPaths());
    action("zoomSelection")->setEnabled(any);action("zoomPrevious")->setEnabled(board->canZoomBack());
}
void Editor::refreshProperties(){
    // Clear the form; its widgets may still be inside one of their own signals, so they go later. A field losing the focus
    // as it goes (after undo, another board) changes nothing.
    {const bool was=refreshing;refreshing=true;
        while(propertyForm->rowCount()){auto row=propertyForm->takeRow(0);for(auto *item:{row.labelItem,row.fieldItem})if(item){if(auto *w=item->widget()){w->hide();w->deleteLater();}delete item;}}
        refreshing=was;}
    const auto sel=board->selection();auto &b=doc.board();
    if(sel.isEmpty()){
        // Without a selection the board itself, as the reference shows it there: name, size and the inner layers to
        // edit at once (one undo step each change), and its area.
        const bool was=refreshing;refreshing=true;
        propertyForm->addRow(new QLabel(QStringLiteral("<b>%1</b>").arg(ui("Platine"))));
        auto *name=new QLineEdit(b.name);name->setObjectName("boardName");name->setMaxLength(30);propertyForm->addRow(ui("Name"),name);
        connect(name,&QLineEdit::editingFinished,this,[this,name]{
            const QString text=name->text();if(refreshing||text==doc.board().name)return;snapshot();doc.board().name=text;refreshTabs();touched();});
        auto *width=spin(b.width,1,2000,2),*height=spin(b.height,1,2000,2);width->setObjectName("boardWidth");height->setObjectName("boardHeight");
        propertyForm->addRow(ui("Breite"),width);propertyForm->addRow(ui("Höhe"),height);
        auto resize=[this](double w,double h){auto &now=doc.board();if(refreshing||(w==now.width&&h==now.height))return;
            snapshot();doc.board().width=w;doc.board().height=h;board->documentChanged();overview->update();touched();};
        connect(width,&QDoubleSpinBox::valueChanged,this,[this,resize](double v){resize(v,doc.board().height);});
        connect(height,&QDoubleSpinBox::valueChanged,this,[this,resize](double v){resize(doc.board().width,v);});
        const double area=std::round(b.width*b.height)/100;auto *shown=new QLabel(ui("%1 cm²").arg(uiLocale().toString(area,'g',12)));shown->setObjectName("boardArea");
        propertyForm->addRow(ui("Flächeninhalt"),shown);
        auto *inner=new QCheckBox(ui("Mehrlagig (innere Kupferlagen I1 und I2)"));inner->setObjectName("boardMultilayer");inner->setChecked(b.multilayer);propertyForm->addRow(inner);
        connect(inner,&QCheckBox::toggled,this,[this](bool on){if(refreshing||on==doc.board().multilayer)return;
            snapshot();auto &now=doc.board();now.multilayer=on;if(!on&&(now.activeLayer==Inner1||now.activeLayer==Inner2))now.activeLayer=CopperBottom;
            board->documentChanged();refreshLayers();touched();});
        propertyForm->addRow(ui("Elemente"),new QLabel(QString::number(b.elements.size())));
        refreshing=was;return;
    }
    const bool was=refreshing;refreshing=true;
    // A selection of several kinds lists them with their numbers, as the reference's multiple selection does: the
    // kind picked there shows its properties, and edits go to the selected elements of that kind only. The selection
    // stays as it is.
    static const ElementType kindOrder[]={ElementType::Pad,ElementType::SmdPad,ElementType::Text,ElementType::Track,ElementType::Circle,ElementType::Area};
    QList<ElementType> kinds;QMap<int,int> counts;for(int i:sel){const auto t=b.elements[i].type;if(!counts.contains(int(t)))kinds.append(t);counts[int(t)]++;}
    QList<int> targets=sel;
    if(kinds.size()>1){
        QList<ElementType> ordered;for(auto t:kindOrder)if(counts.contains(int(t)))ordered.append(t);
        if(!ordered.contains(ElementType(multiKind)))multiKind=int(ordered.first());
        auto *list=new QListWidget;list->setObjectName("multipleSelection");
        for(auto t:ordered){auto *item=new QListWidgetItem(QStringLiteral("%1  %2").arg(counts[int(t)]).arg(kindName(t,counts[int(t)])));item->setData(Qt::UserRole,int(t));list->addItem(item);
            if(int(t)==multiKind)list->setCurrentItem(item);}
        list->setMaximumHeight(int(ordered.size())*20+8);propertyForm->addRow(new QLabel(ui("<b>Mehrfachauswahl</b>")));propertyForm->addRow(list);
        connect(list,&QListWidget::currentItemChanged,this,[this](QListWidgetItem *item){if(item&&!refreshing){multiKind=item->data(Qt::UserRole).toInt();QTimer::singleShot(0,this,[this]{refreshProperties();});}});
        targets.clear();for(int i:sel)if(int(b.elements[i].type)==multiKind)targets.append(i);
    }
    auto apply=[this,targets](std::function<void(Element&)> set){if(!refreshing)editElements(targets,set);};
    auto addNumber=[&](const QString &label,double value,double low,double high,int decimals,const QString &suffix,std::function<void(Element&,double)> set){
        auto *box=spin(value,low,high,decimals,suffix);propertyForm->addRow(label,box);
        connect(box,&QDoubleSpinBox::valueChanged,this,[apply,set](double v){apply([&](Element &e){set(e,v);});});return box;
    };
    auto addCheck=[&](const QString &label,bool value,std::function<void(Element&,bool)> set){
        auto *box=new QCheckBox(label);box->setChecked(value);propertyForm->addRow(box);
        connect(box,&QCheckBox::toggled,this,[apply,set](bool v){apply([&](Element &e){set(e,v);});});return box;
    };
    // A switch over a flag of the elements: grey where the elements differ, as in the reference; a click sets it for all.
    auto addFlag=[&](const QString &label,bool Element::*flag,std::function<void(Element&)> after={}){
        int on=0;for(int i:targets)on+=b.elements[i].*flag;
        auto *box=addCheck(label,on>0,[flag,after](Element &x,bool v){x.*flag=v;if(after)after(x);});
        if(on>0&&on<targets.size()){const QSignalBlocker quiet(box);box->setTristate(true);box->setCheckState(Qt::PartiallyChecked);}
        connect(box,&QCheckBox::checkStateChanged,box,[box](Qt::CheckState){box->setTristate(false);});return box;
    };
    // The same over a state of the elements read by `get`.
    auto addState=[&](const QString &label,std::function<bool(const Element&)> get,std::function<void(Element&,bool)> set){
        int on=0;for(int i:targets)on+=get(b.elements[i]);
        auto *box=addCheck(label,on>0,set);
        if(on>0&&on<targets.size()){const QSignalBlocker quiet(box);box->setTristate(true);box->setCheckState(Qt::PartiallyChecked);}
        connect(box,&QCheckBox::checkStateChanged,box,[box](Qt::CheckState){box->setTristate(false);});return box;
    };
    auto addCombo=[&](const QString &label,const QStringList &items,int index,std::function<void(Element&,int)> set){
        auto *box=new QComboBox;box->addItems(items);box->setCurrentIndex(index);propertyForm->addRow(label,box);
        connect(box,&QComboBox::currentIndexChanged,this,[apply,set](int v){apply([&](Element &e){set(e,v);});});
    };
    auto addText=[&](const QString &label,const QString &value,std::function<void(Element&,const QString&)> set){
        auto *edit=new QLineEdit(value);propertyForm->addRow(label,edit);
        connect(edit,&QLineEdit::editingFinished,this,[edit,apply,set]{const QString v=edit->text();apply([&](Element &e){set(e,v);});});
        return edit;
    };
    const Element &e=b.elements[targets.first()];
    bool sameType=true;for(int i:targets)sameType&=b.elements[i].type==e.type;
    // Thermal pads: spoke width, spokes per layer (through-hole pads) and the eight directions of the spokes that hold
    // on the active copper layer, from twelve o'clock clockwise.
    auto addThermal=[&]{
        // The spoke width in percent, and in millimetres as the reference shows it: a third of the pad (of its shorter side
        // for SMD pads) times the percentage.
        {auto *width=addNumber(ui("Stegbreite"),e.thermalWidth,50,300,0," %",[](Element &x,double v){x.thermalWidth=int(std::lround(v));});
            const double size=e.type==ElementType::Pad?e.size:std::min(e.size,e.size2);
            auto *mm=new QLabel(ui("≈ %1 mm").arg(uiLocale().toString(size/3*e.thermalWidth/100,'f',2)));mm->setObjectName("thermalWidthMm");propertyForm->addRow(QString(),mm);
            connect(width,&QDoubleSpinBox::valueChanged,mm,[mm,size](double v){mm->setText(ui("≈ %1 mm").arg(uiLocale().toString(size/3*v/100,'f',2)));});}
        if(e.type==ElementType::Pad)addFlag(ui("Stege je Layer einzeln"),&Element::thermalPerLayer);
        const int layer=b.activeLayer;
        auto byteOf=[layer](const Element &x){return x.thermalPerLayer&&x.type==ElementType::Pad?(layer==CopperTop?0:layer==CopperBottom?1:layer==Inner1?2:layer==Inner2?3:0):0;};
        auto *row=new QWidget;auto *h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);h->setSpacing(1);
        static const char16_t arrows[]=u"\u2191\u2197\u2192\u2198\u2193\u2199\u2190\u2196";
        for(int bit=0;bit<8;bit++){
            auto *button=new QToolButton;button->setText(QString(QChar(arrows[bit])));button->setCheckable(true);button->setChecked((e.thermalSpokes>>(8*byteOf(e)+bit))&1);h->addWidget(button);
            connect(button,&QToolButton::toggled,this,[this,apply,byteOf,bit](bool on){
                if(refreshing)return;apply([&](Element &x){const quint32 mask=1u<<(8*byteOf(x)+bit);x.thermalSpokes=on?x.thermalSpokes|mask:x.thermalSpokes&~mask;});});
        }
        // All eight directions turned over at once, as a double click on the reference's picture does.
        {auto *turn=new QToolButton;turn->setText(ui("Umkehren"));turn->setObjectName("thermalInvert");h->addWidget(turn);
            connect(turn,&QToolButton::clicked,this,[apply,byteOf]{apply([&](Element &x){x.thermalSpokes^=0xffu<<(8*byteOf(x));});});}
        h->addStretch();propertyForm->addRow(ui("Stege"),row);
    };
    propertyForm->addRow(new QLabel(targets.size()==1?QString("<b>%1</b>").arg(typeName(e.type)):ui("<b>%1 Elemente</b>").arg(targets.size())));
    // Layer: copper for pads, any for the rest.
    QList<int> layers;const bool pads=sameType&&(e.type==ElementType::Pad||e.type==ElementType::SmdPad);
    for(int l:{CopperTop,SilkTop,CopperBottom,SilkBottom,Inner1,Inner2,Outline})if(!pads||isCopper(l))layers.append(l);
    {QStringList names;int current=0;for(int k=0;k<layers.size();k++){names<<layerName(layers[k]);if(layers[k]==e.layer)current=k;}
        addCombo(ui("Layer"),names,current,[layers](Element &x,int k){if(k>=0&&k<layers.size()){x.layer=layers[k];if(x.type==ElementType::Text)updateStrokes(x);}});}
    addNumber(ui("Abstand zur Masse"),e.clearance,0,50,2," mm",[](Element &x,double v){x.clearance=v;});
    // Positions from the origin, y upwards.
    const QPointF origin=b.origin;
    auto addPosition=[&]{
        addNumber(ui("X"),e.pos.x()-origin.x(),-10000,10000,3," mm",[origin](Element &x,double v){pcb::move(x,{v+origin.x()-x.pos.x(),0});});
        addNumber(ui("Y"),origin.y()-e.pos.y(),-10000,10000,3," mm",[origin](Element &x,double v){pcb::move(x,{0,origin.y()-v-x.pos.y()});});
    };
    if(targets.size()==1||sameType)switch(e.type){
    case ElementType::Pad:
        if(targets.size()==1)addPosition();
        addNumber(ui("Durchmesser"),e.size,.01,100,3," mm",[](Element &x,double v){x.size=v;updateOutline(x);});
        addNumber(ui("Bohrung"),e.size2,0,100,3," mm",[](Element &x,double v){x.size2=v;updateOutline(x);});
        addCombo(ui("Form"),shapeNames(),int(e.shape)-1,[](Element &x,int k){x.shape=PadShape(k+1);updateOutline(x);});
        addNumber(ui("Drehung"),e.rotation,-360,360,2,"°",[](Element &x,double v){x.rotation=v;updateOutline(x);});
        addFlag(ui("Durchkontaktiert"),&Element::via);
        addFlag(ui("Lötstopp offen"),&Element::solderMask);
        addFlag(ui("Wärmefalle (Thermal-Pad)"),&Element::thermal);
        if(e.thermal)addThermal();
        if(targets.size()==1)addText(ui("Name"),e.name,[](Element &x,const QString &v){x.name=v;});
        break;
    case ElementType::SmdPad:
        if(targets.size()==1)addPosition();
        addNumber(ui("Breite"),e.size,.01,100,3," mm",[](Element &x,double v){x.size=v;updateOutline(x);});
        addNumber(ui("Höhe"),e.size2,.01,100,3," mm",[](Element &x,double v){x.size2=v;updateOutline(x);});
        addNumber(ui("Drehung"),e.rotation,-360,360,2,"°",[](Element &x,double v){x.rotation=v;updateOutline(x);});
        addFlag(ui("Lötstopp offen"),&Element::solderMask);
        addFlag(ui("Wärmefalle (Thermal-Pad)"),&Element::thermal);
        if(e.thermal)addThermal();
        if(targets.size()==1)addText(ui("Name"),e.name,[](Element &x,const QString &v){x.name=v;});
        break;
    case ElementType::Track:case ElementType::Area:
        addNumber(e.type==ElementType::Track?ui("Breite"):ui("Randbreite"),e.width,0,100,3," mm",[](Element &x,double v){x.width=v;});
        if(targets.size()==1)propertyForm->addRow(ui("Knoten"),new QLabel(QString::number(e.points.size())));
        // The length of a track along its nodes.
        if(e.type==ElementType::Track&&targets.size()==1){double length=0;for(qsizetype k=1;k<e.points.size();k++)length+=QLineF(e.points[k-1],e.points[k]).length();
            auto *shown=new QLabel(ui("%1 mm").arg(uiLocale().toString(length,'f',3)));shown->setObjectName("trackLength");propertyForm->addRow(ui("Länge"),shown);}
        // The current a track carries, as the reference estimates it, with a calculator; a footnote names the copper and
        // the warming it holds for.
        if(e.type==ElementType::Track&&targets.size()==1&&isCopper(e.layer)){
            auto *row=new QWidget;auto *h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);
            auto *value=new QLabel(ui("%1 A").arg(uiLocale().toString(maximumCurrent(e.width,copperThickness,temperatureRise),'f',2)));value->setObjectName("imax");
            auto *more=new QToolButton;more->setText(QStringLiteral("…"));more->setToolTip(ui("Strombelastbarkeit…"));h->addWidget(value,1);h->addWidget(more);
            connect(more,&QToolButton::clicked,this,[this,w=e.width]{currentDialog(w);});propertyForm->addRow(ui("Imax*"),row);
            auto *note=new QLabel(QStringLiteral("* ")+ui("Bei %1 µm Kupfer und %2 K Erwärmung").arg(uiLocale().toString(copperThickness),uiLocale().toString(temperatureRise)));
            note->setObjectName("imaxNote");note->setWordWrap(true);propertyForm->addRow(note);
        }
        if(e.type==ElementType::Track){
            addFlag(ui("Anfang eckig"),&Element::flatStart);
            addFlag(ui("Ende eckig"),&Element::flatEnd);
        }else{
            if(e.layer==CopperTop||e.layer==CopperBottom)addFlag(ui("Nur Lötstopp"),&Element::maskOnly);
            addFlag(ui("Gerastert"),&Element::hatched);
            if(e.hatched){
                addFlag(ui("Rasterweite aus Randbreite"),&Element::hatchAuto);
                if(!e.hatchAuto)addNumber(ui("Rasterweite"),std::max(.5,e.hatchPitch),.5,100,3," mm",[](Element &x,double v){x.hatchPitch=v;});
            }
        }
        addFlag(ui("Sperrfläche"),&Element::cutout);
        addFlag(ui("Lötstopp offen"),&Element::solderMask);
        if(targets.size()==1)addText(ui("Name"),e.name,[](Element &x,const QString &v){x.name=v;});
        break;
    case ElementType::Circle:
        if(targets.size()==1)addPosition();
        // The diameter as the reference shows it; the model keeps the radius.
        addNumber(ui("Durchmesser"),2*e.size,0,2000,3," mm",[](Element &x,double v){x.size=v/2;})->setObjectName("circleDiameter");
        addNumber(ui("Breite"),e.width,0,100,3," mm",[](Element &x,double v){x.width=v;});
        addNumber(ui("Startwinkel"),e.start,0,360,1,"°",[](Element &x,double v){x.start=v;});
        addNumber(ui("Endwinkel"),e.stop,0,360,1,"°",[](Element &x,double v){x.stop=v;});
        addFlag(ui("Gefüllt"),&Element::filled);
        addFlag(ui("Sperrfläche"),&Element::cutout);
        addFlag(ui("Lötstopp offen"),&Element::solderMask);
        break;
    case ElementType::Text:
        // New input up to 50 characters, as the reference allows; a longer text (from the dialog or a file) is not cut.
        if(targets.size()==1){addPosition();addText(ui("Text"),e.text,[](Element &x,const QString &v){x.text=v;updateStrokes(x);})->setMaxLength(std::max<int>(50,int(e.text.size())));}
        addNumber(ui("Höhe"),e.size,std::min(e.size,minimumTextHeight(e.thickness)),200,2," mm",[this](Element &x,double v){x.size=std::max(v,minimumTextHeight(x.thickness));updateStrokes(x);});
        addCombo(ui("Stil"),{ui("eng"),ui("normal"),ui("weit")},e.style,[](Element &x,int k){x.style=k;updateStrokes(x);});
        addCombo(ui("Strichstärke"),{ui("dünn"),ui("normal"),ui("dick")},e.thickness,[this](Element &x,int k){x.thickness=k;x.size=std::max(x.size,minimumTextHeight(k));updateStrokes(x);});
        // The angle and both mirrors as the reference shows them: mirrored top to bottom, a text counts its angle without
        // the half turn that holds that mirror in the model. Either mirror flips the text about its start in its own
        // direction (top to bottom: about its baseline), which keeps running the same way.
        addNumber(ui("Drehung"),wrapped(e.rotation-(e.flipped?180:0)),-360,360,1,"°",[](Element &x,double v){x.rotation=wrapped(v+(x.flipped?180:0));updateStrokes(x);});
        // The four right angles at a click, as the reference offers them for texts.
        {auto *row=new QHBoxLayout;for(int a:{0,90,180,270}){auto *quick=new QToolButton;quick->setText(QStringLiteral("%1°").arg(a));quick->setObjectName(QStringLiteral("textAngle%1").arg(a));
                row->addWidget(quick);connect(quick,&QToolButton::clicked,this,[apply,a]{apply([a](Element &x){x.rotation=wrapped(a+(x.flipped?180:0));updateStrokes(x);});});}
            row->addStretch();propertyForm->addRow(QString(),row);}
        addState(ui("Gespiegelt"),[](const Element &x){return x.mirrored!=x.flipped;},[](Element &x,bool v){if((x.mirrored!=x.flipped)!=v)x.mirrored=!x.mirrored;updateStrokes(x);});
        addState(ui("Senkrecht gespiegelt"),[](const Element &x){return x.flipped;},
                 [](Element &x,bool v){if(x.flipped==v)return;x.flipped=v;x.mirrored=!x.mirrored;x.rotation=wrapped(x.rotation+180);updateStrokes(x);})->setObjectName("textFlipped");
        addFlag(ui("Sperrfläche"),&Element::cutout);
        // The width of the strokes the text gets from its height and stroke.
        {auto *width=new QLabel(ui("%1 mm").arg(uiLocale().toString(e.strokeWidth,'f',3)));width->setObjectName("textStrokeWidth");propertyForm->addRow(ui("Wirksame Strichstärke"),width);}
        if(targets.size()==1&&e.role!=TextRole::Plain){
            propertyForm->addRow(new QLabel(e.role==TextRole::Designator?ui("Bezeichner eines Bauteils"):ui("Wert eines Bauteils")));
            addFlag(ui("Sichtbar"),&Element::visible);
            if(e.role==TextRole::Designator){
                addText(ui("Gehäuse"),e.package,[](Element &x,const QString &v){x.package=v;});
                addText(ui("Kommentar"),e.comment,[](Element &x,const QString &v){x.comment=v;});
                addNumber(ui("Bestückungswinkel"),e.componentRotation,-360,360,1,"°",[](Element &x,double v){x.componentRotation=v;});
                addFlag(ui("Pick+Place-Daten"),&Element::pickAndPlace);
                addCombo(ui("Mittelpunkt"),{ui("Zentrum Kupfer"),ui("Zentrum Bestückung"),ui("Zentrum Kupfer + Bestückung")},e.pickCentre,[](Element &x,int k){x.pickCentre=k;});
                addNumber(ui("Offset X"),e.pickOffset.x(),-100,100,3," mm",[](Element &x,double v){x.pickOffset.setX(v);});
                addNumber(ui("Offset Y"),0.0-e.pickOffset.y(),-100,100,3," mm",[](Element &x,double v){x.pickOffset.setY(0.0-v);});
            }
        }
        break;
    }
    refreshing=was;
}
void Editor::editElements(const QList<int> &indexes,const std::function<void(Element&)> &edit){
    if(indexes.isEmpty())return;
    // Edited on a copy first: when nothing changes there is no undo step, and the steps to undo or redo stay as they are.
    board->cancelDrag();QList<Element> els=std::as_const(doc).board().elements;bool same=true;
    for(int i:indexes)if(i>=0&&i<els.size()){const Element was=els[i];edit(els[i]);same=same&&els[i]==was;}
    if(same)return;
    snapshot();doc.board().elements=els;board->update();touched();
}

// --- editing commands
void Editor::deleteSelection(){
    const auto sel=board->selection();if(sel.isEmpty())return;snapshot();removeElements(doc.board(),sel);board->setSelection({});board->documentChanged();touched();
}
void Editor::rotateSelection(double degrees){
    const auto sel=board->selection();if(sel.isEmpty())return;const QPointF pivot=board->snap(selectionBounds(doc.board(),sel).center());
    editElements(sel,[&](Element &e){rotate(e,pivot,degrees);keepReadable(e);});
}
void Editor::mirrorSelection(){
    // About the exact middle of the selection, as the reference mirrors: not snapped to the grid.
    const auto sel=board->selection();if(sel.isEmpty())return;const double axis=selectionBounds(doc.board(),sel).center().x();
    editElements(sel,[&](Element &e){mirror(e,axis);keepReadable(e);});
}
void Editor::mirrorSelectionVertically(){
    // Top to bottom: half a turn, then left to right, about the same middle.
    const auto sel=board->selection();if(sel.isEmpty())return;const QPointF centre=selectionBounds(doc.board(),sel).center();
    editElements(sel,[&](Element &e){rotate(e,centre,180);mirror(e,centre.x());keepReadable(e);});
}
void Editor::alignSelection(int edge){
    const auto sel=board->selection();const auto &b=doc.board();const auto pieces=parts(b,sel);if(pieces.size()<2)return;
    const QRectF all=selectionBounds(b,sel);QMap<int,QPointF> shift;
    for(const auto &piece:pieces){
        const QRectF r=selectionBounds(b,piece);QPointF d;
        switch(edge){
        case 0:d.setX(all.left()-r.left());break;case 1:d.setX(all.right()-r.right());break;
        case 2:d.setY(all.top()-r.top());break;case 3:d.setY(all.bottom()-r.bottom());break;
        case 4:d.setX(all.center().x()-r.center().x());break;default:d.setY(all.center().y()-r.center().y());break;
        }
        for(int i:piece)shift[i]=d;
    }
    int k=0;editElements(sel,[&](Element &e){pcb::move(e,shift.value(sel[k++]));});
}
void Editor::alignToGrid(){
    // The grid as drawing snaps to it, counted from the origin.
    const auto sel=board->selection();const auto &b=doc.board();if(sel.isEmpty())return;
    auto onGrid=[this](QPointF p){return board->onGrid(p);};
    QMap<int,QPointF> shift;QSet<int> nodeByNode;
    for(const auto &piece:parts(b,sel)){
        const auto &first=b.elements[piece.first()];
        if(piece.size()==1&&(first.type==ElementType::Track||first.type==ElementType::Area)){nodeByNode.insert(piece.first());continue;}
        // A group keeps its shape: its first pad (or else its first element) goes onto the grid.
        int anchor=piece.first();for(int i:piece)if(b.elements[i].type==ElementType::Pad||b.elements[i].type==ElementType::SmdPad){anchor=i;break;}
        const auto &a=b.elements[anchor];const QPointF at=a.type==ElementType::Track||a.type==ElementType::Area?a.points.value(0):a.pos;
        for(int i:piece)shift[i]=onGrid(at)-at;
    }
    int k=0;editElements(sel,[&](Element &e){const int i=sel[k++];if(nodeByNode.contains(i)){for(auto &p:e.points)p=onGrid(p);}else pcb::move(e,shift.value(i));});
}
// --- nodes of tracks and areas
namespace {
bool hasNodes(const Board &b,int element){return element>=0&&element<b.elements.size()&&(b.elements[element].type==ElementType::Track||b.elements[element].type==ElementType::Area);}
}
bool Editor::removeNode(int element,int node){
    const auto &b=std::as_const(doc).board();if(!hasNodes(b,element))return false;const auto &e=b.elements[element];
    if(node<0||node>=e.points.size()||e.points.size()<=(e.type==ElementType::Track?2:3))return false;
    editElements({element},[node](Element &x){x.points.removeAt(node);});return true;
}
bool Editor::alignNode(int element,int node){
    const auto &b=std::as_const(doc).board();if(!hasNodes(b,element)||node<0||node>=b.elements[element].points.size())return false;
    editElements({element},[this,node](Element &x){x.points[node]=board->onGrid(x.points[node]);});return true;
}
bool Editor::alignNodes(int element){
    if(!hasNodes(std::as_const(doc).board(),element))return false;
    editElements({element},[this](Element &x){for(auto &p:x.points)p=board->onGrid(p);});return true;
}
bool Editor::splitTrack(int element,int node){
    {const auto &b=std::as_const(doc).board();if(!hasNodes(b,element)||b.elements[element].type!=ElementType::Track||node<=0||node>=b.elements[element].points.size()-1)return false;}
    // Both halves share the node and keep groups, component, layer, widths and the autoroute mark with its pads (either
    // half turns back into the airwire); the square ends stay at the outer ends. The second half goes to the end of the
    // list, so no other element changes its index.
    snapshot();auto &els=doc.board().elements;Element first=els[element],second=first;
    first.points=first.points.mid(0,node+1);first.flatEnd=false;second.points=second.points.mid(node);second.flatStart=false;
    els[element]=first;els.append(second);const int added=int(els.size())-1;
    board->documentChanged();board->setSelection({element,added});touched();return true;
}
int Editor::joiningTrack(int element,int node) const{
    const auto &b=doc.board();if(!hasNodes(b,element))return -1;const auto &e=b.elements[element];
    if(e.type!=ElementType::Track||(node!=0&&node!=e.points.size()-1))return -1;
    auto same=[](QPointF a,QPointF c){return std::abs(a.x()-c.x())<1e-6&&std::abs(a.y()-c.y())<1e-6;};const QPointF at=e.points[node];
    for(int i=0;i<b.elements.size();i++){const auto &o=b.elements[i];
        if(i!=element&&o.type==ElementType::Track&&o.layer==e.layer&&o.points.size()>=2&&(same(o.points.first(),at)||same(o.points.last(),at)))return i;}
    return -1;
}
bool Editor::joinTracks(int element,int node){
    const int other=joiningTrack(element,node);if(other<0)return false;
    // The other track's nodes follow on from the shared one, the far end keeps its square or round end; the other track
    // goes, the airwires and autoroute pads of the rest keep pointing at the same pads.
    snapshot();auto &els=doc.board().elements;Element joined=els[element];const Element o=els[other];
    QPolygonF from=o.points;const bool startsThere=std::abs(from.first().x()-joined.points[node].x())<1e-6&&std::abs(from.first().y()-joined.points[node].y())<1e-6;
    if(!startsThere)std::reverse(from.begin(),from.end());const bool farFlat=startsThere?o.flatEnd:o.flatStart;
    if(node>0){joined.points+=from.mid(1);joined.flatEnd=farFlat;}
    else{std::reverse(from.begin(),from.end());joined.points=from.mid(0,from.size()-1)+joined.points;joined.flatStart=farFlat;}
    els[element]=joined;removeElements(doc.board(),{other});
    board->documentChanged();board->setSelection({other<element?element-1:element});touched();return true;
}
void Editor::addCopies(int copies,const std::function<void(QList<Element>&,int)> &place){
    const auto sel=board->selection();if(sel.isEmpty()||copies<1)return;
    const auto original=extract(doc.board(),sel);snapshot();auto &b=doc.board();QList<int> added=sel;
    // Each copy gets group numbers of its own and the next free designators, as a placed component does.
    for(int k=1;k<=copies;k++){
        auto els=placeable(original,b,{},false);place(els,k);const int base=int(b.elements.size());
        for(auto e:els){offsetLinks(e,base);b.elements.append(e);added.append(int(b.elements.size())-1);}
    }
    board->setSelection(added);board->documentChanged();touched();
}
void Editor::tileSelection(int columns,int rows,QPointF step){
    columns=std::max(1,columns);rows=std::max(1,rows);addCopies(columns*rows-1,tilePlacement(columns,step));
}
void Editor::arrangeInCircle(int count,double angle,double radius,bool turn,QPointF start){
    const auto sel=board->selection();if(sel.isEmpty())return;
    addCopies(count-1,circlePlacement(selectionBounds(doc.board(),sel).center()+start,angle,radius,turn));
}
void Editor::arrangeDialog(){
    const auto sel=board->selection();if(sel.isEmpty())return;const auto &b=doc.board();
    const QRectF box=selectionBounds(b,sel);const double g=b.grid;auto nextStep=[g](double v){return std::ceil((v+g)/g-1e-9)*g;};
    QDialog dialog(this);dialog.setWindowTitle(ui("Kacheln / Kreisförmig anordnen"));auto *v=new QVBoxLayout(&dialog);auto *tabs=new QTabWidget;v->addWidget(tabs);
    auto *tiles=new QWidget;auto *tf=new QFormLayout(tiles);
    auto *columns=new QSpinBox;columns->setRange(1,100);columns->setValue(2);auto *rows=new QSpinBox;rows->setRange(1,100);rows->setValue(1);
    auto *dx=spin(nextStep(box.width()),-1000,1000,3),*dy=spin(nextStep(box.height()),-1000,1000,3);
    tf->addRow(ui("Anzahl horizontal"),columns);tf->addRow(ui("Anzahl vertikal"),rows);tf->addRow(ui("Abstand X"),dx);tf->addRow(ui("Abstand Y"),dy);
    tabs->addTab(tiles,ui("Kacheln"));
    auto *circle=new QWidget;auto *cf=new QFormLayout(circle);
    auto *count=new QSpinBox;count->setObjectName("circleCount");count->setRange(2,360);count->setValue(8);auto *angle=spin(45,-360,360,3,QStringLiteral("°"));angle->setObjectName("circleAngle");
    auto *radius=spin(10,-1000,1000,3);radius->setObjectName("circleRadius");auto *range=new QLabel;range->setObjectName("circleRange");
    auto *turn=new QCheckBox(ui("Objekte mitdrehen"));turn->setChecked(true);auto *sx=spin(0,-1000,1000,3),*sy=spin(0,-1000,1000,3);sx->setObjectName("circleStartX");sy->setObjectName("circleStartY");
    // The start point can be put onto a pad of the selection with the arrows.
    QList<QPointF> pads;for(int i:sel)if(b.elements[i].type==ElementType::Pad||b.elements[i].type==ElementType::SmdPad)pads.append(b.elements[i].pos-box.center());
    auto *previous=new QToolButton;previous->setArrowType(Qt::LeftArrow);previous->setToolTip(ui("Voriges Pad"));
    auto *next=new QToolButton;next->setArrowType(Qt::RightArrow);next->setToolTip(ui("Nächstes Pad"));previous->setEnabled(!pads.isEmpty());next->setEnabled(!pads.isEmpty());
    // The start point back onto the middle of the selection.
    auto *middle=new QToolButton;middle->setText(ui("Mitte"));middle->setObjectName("circleMiddle");middle->setToolTip(ui("Startpunkt in die Mitte der Markierung"));
    auto *start=new QHBoxLayout;start->addWidget(sx);start->addWidget(sy);start->addWidget(middle);start->addWidget(previous);start->addWidget(next);
    cf->addRow(ui("Anzahl insgesamt"),count);cf->addRow(ui("Winkel"),angle);cf->addRow(QString(),range);cf->addRow(ui("Radius"),radius);cf->addRow(turn);cf->addRow(ui("Startpunkt"),start);
    tabs->addTab(circle,ui("Kreisförmig anordnen"));
    auto *preview=new ArrangePreview;preview->original=extract(b,sel);v->addWidget(preview,1);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);v->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    int padIndex=-1;
    auto pickPad=[&,sx,sy](int step){if(pads.isEmpty())return;padIndex=(padIndex+step+int(pads.size()))%int(pads.size());sx->setValue(pads[padIndex].x());sy->setValue(-pads[padIndex].y());};
    auto update=[&,preview]{
        if(tabs->currentIndex()==0){preview->copies=columns->value()*rows->value()-1;preview->place=tilePlacement(columns->value(),QPointF(dx->value(),dy->value()));}
        else{preview->copies=count->value()-1;preview->place=circlePlacement(box.center()+QPointF(sx->value(),-sy->value()),angle->value(),radius->value(),turn->isChecked());}
        // The angle the copies span from the first to the last.
        range->setText(ui("Überstrichener Winkel: %1°").arg(uiLocale().toString((count->value()-1)*angle->value(),'g',6)));
        preview->update();
    };
    connect(middle,&QToolButton::clicked,&dialog,[&]{padIndex=-1;sx->setValue(0);sy->setValue(0);});
    connect(previous,&QToolButton::clicked,&dialog,[&]{pickPad(-1);});connect(next,&QToolButton::clicked,&dialog,[&]{pickPad(1);});
    for(auto *spinBox:{columns,rows,count})connect(spinBox,&QSpinBox::valueChanged,&dialog,update);
    for(auto *spinBox:{dx,dy,angle,radius,sx,sy})connect(spinBox,&QDoubleSpinBox::valueChanged,&dialog,update);
    connect(turn,&QCheckBox::toggled,&dialog,update);connect(tabs,&QTabWidget::currentChanged,&dialog,update);update();
    if(dialog.exec()!=QDialog::Accepted)return;
    // The start point counts upwards on the screen, as the coordinates below do.
    if(tabs->currentIndex()==0)tileSelection(columns->value(),rows->value(),QPointF(dx->value(),dy->value()));
    else arrangeInCircle(count->value(),angle->value(),radius->value(),turn->isChecked(),QPointF(sx->value(),-sy->value()));
}
bool Editor::exportImage(const QString &file,int dpi,bool colour,QString *error){
    const auto &b=doc.board();const double perMm=dpi/25.4;const QSize size(std::max(1,int(std::lround(b.width*perMm))),std::max(1,int(std::lround(b.height*perMm))));
    if(qint64(size.width())*size.height()>200'000'000){if(error)*error=ui("Das Bild ist zu groß.");return false;}
    QImage image;
    // GIF has a palette of 256 colours: the picture without smoothed edges keeps to the layer colours.
    const bool gif=QFileInfo(file).suffix().compare(QStringLiteral("gif"),Qt::CaseInsensitive)==0;
    if(colour)image=board->renderBoard(perMm,!gif);
    else{
        // Black on white, as a printout of the visible layers with the holes open.
        PrintSettings s=defaultPrintSettings(b);for(int l=1;l<=layerCount;l++)s.layers[l]=b.visible[l];s.blackWhite=true;
        image=QImage(size,QImage::Format_RGB32);image.fill(Qt::white);
        {QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.scale(perMm,perMm);paintPrintout(p,b,s);}
        image=image.convertToFormat(QImage::Format_Mono,Qt::ThresholdDither);
        if(QStringList{"jpg","jpeg"}.contains(QFileInfo(file).suffix().toLower()))image=image.convertToFormat(QImage::Format_Grayscale8);
    }
    if(image.isNull()){if(error)*error=ui("Das Bild ist zu groß.");return false;}
    const int perMetre=int(std::lround(dpi/.0254));image.setDotsPerMeterX(perMetre);image.setDotsPerMeterY(perMetre);
    if(gif){
        const QByteArray data=gifData(image);QSaveFile out(file);
        if(data.isEmpty()||!out.open(QIODevice::WriteOnly)||out.write(data)!=data.size()||!out.commit()){if(error)*error=ui("Das Bild konnte nicht gespeichert werden.");return false;}
        return true;
    }
    if(!image.save(file)){if(error)*error=ui("Das Bild konnte nicht gespeichert werden.");return false;}
    return true;
}
bool Editor::exportEmf(const QString &file,QString *error){
    // The visible layers as on the screen, as vectors: lines with their width, areas as polygons, holes as circles.
    const auto &b=doc.board();EmfDevice device(QSizeF(b.width,b.height),QStringLiteral("Leiterplatte"));
    {QPainter p(&device);board->paintBoard(p,100);}
    const QByteArray data=device.data();QSaveFile out(file);
    if(data.isEmpty()||!out.open(QIODevice::WriteOnly)||out.write(data)!=data.size()||!out.commit()){if(error)*error=ui("Die Datei kann nicht geschrieben werden: %1").arg(file);return false;}
    return true;
}
// --- fabrication outputs, named after the document and written next to it
QString Editor::outputName() const{return QFileInfo(displayName()).completeBaseName();}
void Editor::gerberDialog(){
    // Counted from the origin or the top left corner as the preferences say.
    gerberSettings.fromOrigin=camOrigin;GerberDialog dialog(doc.board(),outputName(),startFolder(Fabrication),gerberSettings,this);dialog.exec();
    gerberSettings=dialog.settings();usedFolder(Fabrication,dialog.folder());
}
void Editor::drillDialog(){
    drillSettings.fromOrigin=camOrigin;DrillDialog dialog(doc.board(),outputName(),drillSettings,this);const int answer=dialog.exec();drillSettings=dialog.settings();
    if(answer!=QDialog::Accepted)return;
    const QString start=QDir(startFolder(Fabrication)).filePath(dialog.suggestedFile());
    auto file=QFileDialog::getSaveFileName(this,ui("Bohrdaten (Excellon)"),start,ui("Excellon-Bohrdaten (*.drl *.txt);;Alle Dateien (*)"));
    if(file.isEmpty())return;if(QFileInfo(file).suffix().isEmpty())file+=".drl";usedFolder(Fabrication,QFileInfo(file).absolutePath());
    const QByteArray data=excellon(doc.board(),drillSettings);QSaveFile f(file);
    if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit())QMessageBox::warning(this,ui("Exportieren"),ui("Die Datei kann nicht geschrieben werden: %1").arg(file));
}
void Editor::millingDialog(){
    millingSettings.fromOrigin=camOrigin;MillingDialog dialog(doc.board(),board->selection(),millingSettings,this);const int answer=dialog.exec();millingSettings=dialog.settings();
    if(answer!=QDialog::Accepted)return;
    const QString start=QDir(startFolder(Milling)).filePath(outputName()+QStringLiteral(".plt"));
    auto file=QFileDialog::getSaveFileName(this,ui("Isolationsfräsen"),start,ui("HPGL-Fräsdateien (*.plt);;Alle Dateien (*)"));
    if(file.isEmpty())return;if(QFileInfo(file).suffix().isEmpty())file+=".plt";usedFolder(Milling,QFileInfo(file).absolutePath());
    QList<MillingJob> jobs;{ProgressWindow progress(this,ui("Fräswege werden berechnet …"));QApplication::setOverrideCursor(Qt::WaitCursor);jobs=dialog.jobs(progress.report());QApplication::restoreOverrideCursor();}
    // One file, or a file per job with its pen number; the job list as text beside it if wanted.
    const QFileInfo info(file);QStringList failed;
    auto put=[&](const QString &name,const QByteArray &data){QSaveFile f(name);if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit())failed.append(name);};
    for(const auto &[name,data]:millingFiles(doc.board(),jobs,millingSettings,info.completeBaseName(),info.suffix()))put(info.dir().filePath(name),data);
    if(millingSettings.jobListFile)put(info.dir().filePath(info.completeBaseName()+QStringLiteral(".txt")),millingJobList(jobs).toUtf8());
    if(!failed.isEmpty())QMessageBox::warning(this,ui("Exportieren"),ui("Die Datei kann nicht geschrieben werden: %1").arg(failed.join(QStringLiteral(", "))));
    // The result over the layout.
    QList<QPolygonF> paths;QList<QPointF> plunges;
    for(const auto &j:jobs){
        paths+=j.paths;plunges+=j.plunges;
        for(const auto &[centre,radius]:j.circles){QPolygonF ring;for(int a=0;a<=72;a++){const double t=a*M_PI/36;ring.append(centre+QPointF(radius*std::cos(t),radius*std::sin(t)));}paths.append(ring);}
    }
    board->setMillingPaths(paths,plunges,millingSettings.toolWidth);refreshActions();
}
void Editor::componentDataDialog(){
    componentSettings.fromOrigin=camOrigin;ComponentDataDialog dialog(doc.board(),outputName(),componentSettings,this);const int answer=dialog.exec();componentSettings=dialog.settings();
    if(answer!=QDialog::Accepted)return;
    const QString start=QDir(startFolder(Fabrication)).filePath(dialog.suggestedFile());
    auto file=QFileDialog::getSaveFileName(this,ui("Export der Bauteildaten"),start,ui("Textdateien (*.csv *.txt);;Alle Dateien (*)"));
    if(file.isEmpty())return;if(QFileInfo(file).suffix().isEmpty())file+=".csv";usedFolder(Fabrication,QFileInfo(file).absolutePath());
    const QByteArray data=dialog.preview().toUtf8();QSaveFile f(file);
    if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit())QMessageBox::warning(this,ui("Exportieren"),ui("Die Datei kann nicht geschrieben werden: %1").arg(file));
}
void Editor::exportImageDialog(){
    QDialog dialog(this);dialog.setWindowTitle(ui("Als Bild exportieren"));auto *form=new QFormLayout(&dialog);
    auto *colour=new QRadioButton(ui("farbig"));auto *mono=new QRadioButton(ui("schwarz/weiß"));colour->setChecked(true);
    auto *colours=new QHBoxLayout;colours->addWidget(colour);colours->addWidget(mono);form->addRow(ui("Farbe"),colours);
    auto *dpi=new QSpinBox;dpi->setRange(50,2400);dpi->setSingleStep(50);dpi->setValue(600);dpi->setSuffix(QStringLiteral(" dpi"));form->addRow(ui("Auflösung"),dpi);
    auto *size=new QLabel;form->addRow(QString(),size);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    // The size of the picture and the memory it needs, as the reference shows them.
    auto update=[&]{
        const auto &b=doc.board();const double perMm=dpi->value()/25.4;const qint64 w=std::llround(b.width*perMm),h=std::llround(b.height*perMm);
        const double mb=double(w)*double(h)*(colour->isChecked()?4:.125)/1048576;
        size->setText(ui("%1 × %2 Pixel, etwa %3 MB").arg(w).arg(h).arg(uiLocale().toString(mb,'f',1)));buttons->button(QDialogButtonBox::Ok)->setEnabled(w*h<=200'000'000);
    };
    connect(dpi,&QSpinBox::valueChanged,&dialog,update);connect(colour,&QRadioButton::toggled,&dialog,update);update();
    if(dialog.exec()!=QDialog::Accepted)return;
    auto file=QFileDialog::getSaveFileName(this,ui("Als Bild exportieren"),QDir(startFolder(Pictures)).filePath(QFileInfo(displayName()).completeBaseName()+".png"),QStringLiteral("PNG (*.png);;Bitmap (*.bmp);;JPEG (*.jpg);;GIF (*.gif)"));
    if(file.isEmpty())return;if(QFileInfo(file).suffix().isEmpty())file+=".png";usedFolder(Pictures,QFileInfo(file).absolutePath());
    QString error;if(!exportImage(file,dpi->value(),colour->isChecked(),&error))QMessageBox::warning(this,ui("Exportieren"),error);
}
void Editor::setPreferencesFile(const QString &file){preferencesPath=file;}
void Editor::librariesChanged(){
    const auto library=libraryFolders();macroFolder=library.own;extraMacroFolders=library.extra;macros->setFolders(macroFolder,extraMacroFolders);
}
LibraryFolders libraryFolders(){
    auto s=preferences();LibraryFolders folders{s.value("macroFolder").toString(),s.value("macroFolders/extra").toStringList()};
    if(folders.own.isEmpty())folders.own=openLochDocumentsFolder(QStringLiteral("Makros"));
    return folders;
}
void setLibraryFolders(const LibraryFolders &folders){auto s=preferences();s.setValue("macroFolder",folders.own);s.setValue("macroFolders/extra",folders.extra);}
// --- the current a track carries ("Imax")
void Editor::currentDialog(double width){
    QDialog dialog(this);dialog.setWindowTitle(ui("Strombelastbarkeit"));auto *form=new QFormLayout(&dialog);
    if(width<0){width=board->trackWidth;for(int i:board->selection())if(doc.board().elements[i].type==ElementType::Track){width=doc.board().elements[i].width;break;}}
    auto *b=spin(width,.01,50,3);b->setObjectName("width");auto *t=spin(copperThickness,1,299,0,QStringLiteral(" µm"));t->setObjectName("copper");
    auto *rise=spin(temperatureRise,1,299,0,QStringLiteral(" K"));rise->setObjectName("rise");auto *result=new QLabel;result->setObjectName("current");
    auto *wanted=spin(1,.01,100,2,QStringLiteral(" A"));wanted->setObjectName("wanted");auto *needed=new QLabel;needed->setObjectName("needed");
    form->addRow(ui("Breite der Leiterbahn"),b);form->addRow(ui("Kupferstärke"),t);form->addRow(ui("Erwärmung"),rise);form->addRow(ui("Imax"),result);
    form->addRow(ui("Strom"),wanted);form->addRow(ui("Nötige Breite"),needed);
    auto *basis=new QLabel(ui("Imax = 3,675 · √(ΔT · t · b · (t + b)) mit Leiterbahnbreite b und Kupferstärke t in mm, Erwärmung ΔT in K, Strom in A. Eine grobe Näherung: Umgebung, Belüftung und Nachbarn ändern den Wert."));
    basis->setWordWrap(true);form->addRow(basis);
    auto update=[&]{result->setText(currentText(maximumCurrent(b->value(),t->value(),rise->value())));
        needed->setText(ui("%1 mm").arg(uiLocale().toString(widthForCurrent(wanted->value(),t->value(),rise->value()),'f',3)));};
    for(auto *x:{b,t,rise,wanted})connect(x,&QDoubleSpinBox::valueChanged,&dialog,update);update();
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close);auto *keep=buttons->addButton(ui("Als Vorgabe übernehmen"),QDialogButtonBox::ActionRole);keep->setObjectName("keep");
    form->addRow(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(keep,&QPushButton::clicked,&dialog,[&]{copperThickness=t->value();temperatureRise=rise->value();auto s=preferences();s.setValue("imax/copper",copperThickness);s.setValue("imax/rise",temperatureRise);refreshProperties();});
    dialog.exec();
}
// --- favourite track widths
void Editor::setWidthFavourites(QList<double> widths){
    favourites=sortedWidths(widths);QStringList stored;for(double w:favourites)stored.append(QString::number(w,'g',10));preferences().setValue("trackWidthFavourites",stored);
}
// The menu of the favourites: each width to choose (the current one ticked), the current width to add, widths to remove.
void Editor::showFavourites(){
    // The submenu of the last time goes with the actions.
    auto *menu=favouriteButton->menu();menu->clear();qDeleteAll(menu->findChildren<QMenu*>(QString(),Qt::FindDirectChildrenOnly));
    const double now=trackWidthBox->value();bool listed=false;
    auto text=[](double w){return ui("%1 mm").arg(uiLocale().toString(w,'f',2));};
    for(double w:favourites){
        auto *a=menu->addAction(text(w));a->setCheckable(true);const bool same=std::abs(w-now)<5e-4;a->setChecked(same);listed|=same;
        connect(a,&QAction::triggered,this,[this,w]{trackWidthBox->setValue(w);});
    }
    if(!favourites.isEmpty())menu->addSeparator();
    auto *add=menu->addAction(ui("Aktuelle Breite (%1) aufnehmen").arg(text(now)));add->setObjectName("favouriteAdd");add->setEnabled(!listed);
    connect(add,&QAction::triggered,this,[this,now]{auto list=favourites;list.append(now);setWidthFavourites(list);});
    auto *remove=menu->addMenu(ui("Entfernen"));remove->setObjectName("favouriteRemove");remove->setEnabled(!favourites.isEmpty());
    for(double w:favourites)connect(remove->addAction(text(w)),&QAction::triggered,this,[this,w]{
        auto list=favourites;list.removeIf([w](double v){return std::abs(v-w)<5e-4;});setWidthFavourites(list);});
}
// --- favourite pad and SMD sizes
void Editor::setSizeFavourites(SizeKind kind,QList<QPointF> sizes){
    auto &list=kind==PadSizes?padFavourites:smdFavourites;list=sortedSizes(sizes);
    QStringList stored;for(auto p:list)stored.append(QString::number(p.x(),'g',10)+"x"+QString::number(p.y(),'g',10));
    preferences().setValue(kind==PadSizes?"padFavourites":"smdFavourites",stored);
}
// The same menu as for the widths: each size to choose (the current one ticked), the current size to add, sizes to remove.
void Editor::showSizeFavourites(SizeKind kind){
    auto *button=kind==PadSizes?padFavouriteButton:smdFavouriteButton;auto *menu=button->menu();menu->clear();qDeleteAll(menu->findChildren<QMenu*>(QString(),Qt::FindDirectChildrenOnly));
    auto *first=kind==PadSizes?padBox:smdWidthBox;auto *second=kind==PadSizes?drillBox:smdHeightBox;
    const QPointF now(first->value(),second->value());const auto &list=kind==PadSizes?padFavourites:smdFavourites;bool listed=false;
    auto text=[](QPointF p){return ui("%1 × %2 mm").arg(uiLocale().toString(p.x(),'f',2),uiLocale().toString(p.y(),'f',2));};
    for(auto p:list){
        auto *a=menu->addAction(text(p));a->setCheckable(true);const bool same=std::abs(p.x()-now.x())<5e-4&&std::abs(p.y()-now.y())<5e-4;a->setChecked(same);listed|=same;
        connect(a,&QAction::triggered,this,[this,kind,p]{
            // Both fields at once: selected pads change in one undo step.
            auto *x=kind==PadSizes?padBox:smdWidthBox;auto *y=kind==PadSizes?drillBox:smdHeightBox;
            {const QSignalBlocker a(x),b(y);x->setValue(p.x());y->setValue(p.y());}
            const auto &changed=kind==PadSizes?padSettingsChanged:smdSettingsChanged;if(changed)changed();});
    }
    if(!list.isEmpty())menu->addSeparator();
    auto *add=menu->addAction(ui("Aktuelle Maße (%1) aufnehmen").arg(text(now)));add->setObjectName("sizeFavouriteAdd");add->setEnabled(!listed);
    connect(add,&QAction::triggered,this,[this,kind,now]{auto all=sizeFavourites(kind);all.append(now);setSizeFavourites(kind,all);});
    auto *remove=menu->addMenu(ui("Entfernen"));remove->setObjectName("sizeFavouriteRemove");remove->setEnabled(!list.isEmpty());
    for(auto p:list)connect(remove->addAction(text(p)),&QAction::triggered,this,[this,kind,p]{
        auto all=sizeFavourites(kind);all.removeIf([p](QPointF v){return std::abs(v.x()-p.x())<5e-4&&std::abs(v.y()-p.y())<5e-4;});setSizeFavourites(kind,all);});
}
void Editor::swapSmdSize(){
    const double w=smdWidthBox->value(),h=smdHeightBox->value();
    {const QSignalBlocker a(smdWidthBox),b(smdHeightBox);smdWidthBox->setValue(h);smdHeightBox->setValue(w);}
    if(smdSettingsChanged)smdSettingsChanged();
}
// --- grids of the keys 1 to 9
QList<double> Editor::defaultGridKeys(){return {2.54,1.27,.635,.3175,1.0,.5,.25,.1,.05};}
void Editor::setGridKeys(const QList<double> &grids){
    gridValues=defaultGridKeys();for(int k=0;k<std::min<qsizetype>(9,grids.size());k++)if(grids[k]>0&&grids[k]<=100)gridValues[k]=grids[k];
}
void Editor::chooseGrid(double grid){
    const int at=gridBox->findData(grid);if(at>=0)gridBox->setCurrentIndex(at);else gridBox->setEditText(uiLocale().toString(grid,'g',10));
    if(std::abs(doc.board().grid-grid)>1e-12){doc.board().grid=grid;modified=true;revision++;board->update();if(titleChanged)titleChanged();}
}
// --- the grid menu
QList<std::pair<double,QString>> Editor::inchGrids(){
    return {{2.54/64,QStringLiteral("1/64")},{2.54/32,QStringLiteral("1/32")},{2.54/16,QStringLiteral("1/16")},{2.54/8,QStringLiteral("1/8")},{2.54/4,QStringLiteral("1/4")},
            {2.54/2,QStringLiteral("1/2")},{2.54,QString()},{5.08,QStringLiteral("2")}};
}
QList<double> Editor::metricGrids(){return {.01,.02,.025,.05,.1,.2,.25,.5,1,2,2.5};}
void Editor::setOwnGrids(const QList<double> &grids){
    userGrids.clear();for(double g:grids)if(std::isfinite(g)&&g>0&&g<=100)userGrids.append(g);
    std::sort(userGrids.begin(),userGrids.end());userGrids.erase(std::unique(userGrids.begin(),userGrids.end(),[](double a,double b){return std::abs(a-b)<1e-12;}),userGrids.end());
    QStringList stored;for(double g:userGrids)stored.append(QString::number(g,'g',12));preferences().setValue("grid/own",stored);
}
void Editor::fillGridMenu(QMenu *menu){
    // Built anew each time it opens; the submenus of the last time go with their actions.
    const auto old=menu->findChildren<QMenu*>(QString(),Qt::FindDirectChildrenOnly);menu->clear();qDeleteAll(old);const double now=doc.board().grid;
    auto grid=[&](QMenu *into,double g,const QString &text){auto *a=into->addAction(text,this,[this,g]{chooseGrid(g);});a->setCheckable(true);a->setChecked(std::abs(now-g)<1e-9);return a;};
    auto mm=[](double g){return ui("%1 mm").arg(uiLocale().toString(g,'g',10));};
    // Fractions of the inch pitch first, as the reference lists them.
    for(const auto &[g,part]:inchGrids())grid(menu,g,part.isEmpty()?ui("%1 (Rastermaß)").arg(mm(g)):ui("%1 (%2 Rastermaß)").arg(mm(g),part))->setObjectName(QStringLiteral("grid-inch-%1").arg(QString::number(g,'g',10)));
    auto *metric=menu->addMenu(ui("Metrisches Raster"));metric->setObjectName("gridMetric");for(double g:metricGrids())grid(metric,g,mm(g));
    auto *own=menu->addMenu(ui("Eigene Raster"));own->setObjectName("gridOwn");for(double g:userGrids)grid(own,g,mm(g));
    if(!userGrids.isEmpty())own->addSeparator();
    own->addAction(ui("Eigenes Raster hinzufügen…"),this,[this]{
        // In millimetres, micrometres, mil or inches; kept in millimetres.
        QDialog dialog(this);dialog.setWindowTitle(ui("Eigenes Raster"));auto *form=new QFormLayout(&dialog);auto *value=spin(doc.board().grid,.0001,100000,4,QString());value->setObjectName("ownGrid");
        auto *unit=new QComboBox;unit->setObjectName("ownGridUnit");unit->addItems({ui("mm"),ui("µm"),ui("mil"),ui("Zoll")});
        auto *row=new QHBoxLayout;row->addWidget(value,1);row->addWidget(unit);form->addRow(ui("Raster"),row);
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()!=QDialog::Accepted)return;
        const double mm=value->value()*(unit->currentIndex()==1?.001:unit->currentIndex()==2?.0254:unit->currentIndex()==3?25.4:1);if(mm<=0||mm>100)return;
        auto grids=userGrids;grids.append(mm);setOwnGrids(grids);chooseGrid(mm);
    })->setObjectName("gridOwnAdd");
    auto *remove=own->addMenu(ui("Eigenes Raster löschen"));remove->setObjectName("gridOwnRemove");remove->setEnabled(!userGrids.isEmpty());
    for(double g:userGrids)remove->addAction(mm(g),this,[this,g]{auto grids=userGrids;grids.removeIf([g](double x){return std::abs(x-g)<1e-12;});setOwnGrids(grids);});
    menu->addSeparator();menu->addAction(ui("Raster der Tasten 1 bis 9…"),this,[this]{gridKeysDialog();});
    // How the grid shows.
    menu->addSeparator();
    auto *style=menu->addMenu(ui("Raster als"));style->setObjectName("gridStyle");auto *styles=new QActionGroup(style);
    for(const bool dots:{false,true}){auto *a=style->addAction(dots?ui("Punkte"):ui("Linien"),this,[this,dots]{board->gridDots=dots;preferences().setValue("grid/dots",dots);board->update();});
        a->setObjectName(dots?"gridDots":"gridLines");a->setCheckable(true);a->setChecked(board->gridDots==dots);styles->addAction(a);}
    auto *marking=menu->addMenu(ui("Hervorgehobene Linien"));marking->setObjectName("gridMarking");auto *marks=new QActionGroup(marking);
    for(const int every:{0,2,4,5,10}){auto *a=marking->addAction(every?ui("Jede %1. Linie").arg(every):ui("Keine"),this,[this,every]{board->gridMarking=every;preferences().setValue("grid/marking",every);board->update();});
        a->setObjectName(QStringLiteral("gridMarking-%1").arg(every));a->setCheckable(true);a->setChecked(board->gridMarking==every);marks->addAction(a);}
    auto *shown=menu->addAction(ui("Raster zeigen"),this,[this](bool on){board->gridShown=on;preferences().setValue("grid/shown",on);board->update();});
    shown->setObjectName("gridShown");shown->setCheckable(true);shown->setChecked(board->gridShown);
}
void Editor::gridKeysDialog(){
    QDialog dialog(this);dialog.setWindowTitle(ui("Raster der Tasten 1 bis 9"));auto *form=new QFormLayout(&dialog);
    QList<QDoubleSpinBox*> boxes;
    for(int k=0;k<9;k++){auto *box=spin(gridValues[k],.001,100,4);boxes.append(box);form->addRow(ui("Taste %1").arg(k+1),box);}
    auto *standard=new QPushButton(ui("Standard"));form->addRow(standard);
    connect(standard,&QPushButton::clicked,&dialog,[&]{const auto d=defaultGridKeys();for(int k=0;k<9;k++)boxes[k]->setValue(d[k]);});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QList<double> grids;QStringList stored;for(auto *box:boxes){grids.append(box->value());stored.append(QString::number(box->value(),'g',10));}
    setGridKeys(grids);preferences().setValue("gridKeys",stored);
}

void Editor::loadPreferences(){
    auto s=preferences();
    // Drill holes: earlier preferences knew only white or not.
    board->holes=std::clamp(s.value("holes",s.value("whiteHoles",false).toBool()?1:0).toInt(),0,2);
    board->darkGround=s.value("darkGround",true).toBool();board->allGrounds=s.value("allGrounds",false).toBool();
    board->testAirwires=s.value("testAirwires",false).toBool();board->takeSizes=s.value("takeSizes",true).toBool();board->optimizeNodes=s.value("optimizeNodes",true).toBool();
    board->autoSnap=s.value("autoSnap",true).toBool();board->rubberBand=std::clamp(s.value("rubberBand",2).toInt(),0,2);board->milUnits=s.value("milUnits",false).toBool();
    readableLabels=s.value("readableLabels",true).toBool();originTopLeft=s.value("originTopLeft",false).toBool();board->blinkTest=s.value("blinkTest",false).toBool();
    camOrigin=s.value("camOrigin",true).toBool();limitTextSize=s.value("limitTextSize",false).toBool();setUndoLimit(s.value("undoSteps",100).toInt());
    setAutosave(s.value("autosave",false).toBool(),s.value("autosaveMinutes",5).toInt());
    for(int k=0;k<5;k++)folders[k]=s.value(QStringLiteral("folders/%1").arg(k)).toString();oneFolder=s.value("folders/one",false).toBool();
    {const auto library=libraryFolders();macroFolder=library.own;extraMacroFolders=library.extra;macros->setFolders(macroFolder,extraMacroFolders);}copperThickness=std::clamp(s.value("imax/copper",35.0).toDouble(),1.0,299.0);temperatureRise=std::clamp(s.value("imax/rise",20.0).toDouble(),1.0,299.0);
    for(int k=0;k<3;k++)userColours[k]=coloursFrom(s.value(QStringLiteral("colours/user%1").arg(k+1)).toStringList());setColourScheme(s.value("colours/scheme",0).toInt());
    board->gridDots=s.value("grid/dots",false).toBool();board->gridShown=s.value("grid/shown",true).toBool();
    {const int every=s.value("grid/marking",4).toInt();board->gridMarking=QList<int>({0,2,4,5,10}).contains(every)?every:4;}
    {QList<double> grids;for(const auto &t:s.value("grid/own").toStringList())grids.append(t.toDouble());userGrids.clear();for(double g:grids)if(std::isfinite(g)&&g>0&&g<=100)userGrids.append(g);
        std::sort(userGrids.begin(),userGrids.end());}
    auto &c=board->crosshair;c.lines=s.value("crosshair/lines",true).toBool();c.diagonals=s.value("crosshair/diagonals",false).toBool();action("crosshair")->setChecked(c.lines);
    c.coordinates=s.value("crosshair/coordinates",false).toBool();c.bigText=s.value("crosshair/bigText",false).toBool();
    c.transparent=s.value("crosshair/transparent",false).toBool();c.whiteBox=s.value("crosshair/whiteBox",false).toBool();
    {const bool shown=s.value("showOverview",true).toBool();action("overview")->setChecked(shown);overview->setVisible(shown);}
    {QList<double> grids;for(const auto &v:s.value("gridKeys").toStringList())grids.append(v.toDouble());setGridKeys(grids);}
    favourites.clear();for(const auto &v:s.value("trackWidthFavourites").toStringList()){bool ok;const double w=v.toDouble(&ok);if(ok)favourites.append(w);}
    favourites=sortedWidths(favourites);
    for(auto kind:{PadSizes,SmdSizes}){
        QList<QPointF> sizes;for(const auto &v:s.value(kind==PadSizes?"padFavourites":"smdFavourites").toStringList()){
            const auto parts=v.split('x');bool a=false,b=false;if(parts.size()==2){const double x=parts[0].toDouble(&a),y=parts[1].toDouble(&b);if(a&&b)sizes.append({x,y});}}
        (kind==PadSizes?padFavourites:smdFavourites)=sortedSizes(sizes);}
    // Mode keys stored as key names; missing ones keep their defaults.
    for(const auto &mode:BoardView::modes())if(s.contains("keys/"+mode)){const QKeySequence key(s.value("keys/"+mode).toString(),QKeySequence::PortableText);
        board->modeKeys[mode]=key.isEmpty()?0:key[0].key();}
    refreshToolKeys();
    pluginList.clear();const auto names=s.value("pluginNames").toStringList(),programs=s.value("pluginPrograms").toStringList();
    for(int k=0;k<std::min(names.size(),programs.size());k++)pluginList.append({names[k],programs[k]});
    board->update();if(board->unitsChanged)board->unitsChanged();
}
void Editor::savePreferences(){
    auto s=preferences();
    s.setValue("holes",board->holes);s.remove("whiteHoles");s.setValue("darkGround",board->darkGround);s.setValue("allGrounds",board->allGrounds);
    s.setValue("testAirwires",board->testAirwires);s.setValue("blinkTest",board->blinkTest);s.setValue("takeSizes",board->takeSizes);s.setValue("optimizeNodes",board->optimizeNodes);
    s.setValue("readableLabels",readableLabels);s.setValue("originTopLeft",originTopLeft);s.setValue("autoSnap",board->autoSnap);s.setValue("rubberBand",board->rubberBand);
    s.setValue("milUnits",board->milUnits);s.setValue("camOrigin",camOrigin);s.setValue("limitTextSize",limitTextSize);s.setValue("undoSteps",undoLimit);
    s.setValue("autosave",autosave);s.setValue("autosaveMinutes",autosaveMinutes);
    for(int k=0;k<5;k++)s.setValue(QStringLiteral("folders/%1").arg(k),folders[k]);s.setValue("folders/one",oneFolder);s.setValue("macroFolder",macroFolder);s.setValue("macroFolders/extra",extraMacroFolders);
    s.setValue("imax/copper",copperThickness);s.setValue("imax/rise",temperatureRise);
    for(int k=0;k<3;k++)s.setValue(QStringLiteral("colours/user%1").arg(k+1),colourNames(userColours[k]));s.setValue("colours/scheme",colourScheme);
    s.setValue("grid/dots",board->gridDots);s.setValue("grid/shown",board->gridShown);s.setValue("grid/marking",board->gridMarking);
    const auto &c=board->crosshair;s.setValue("crosshair/lines",c.lines);s.setValue("crosshair/diagonals",c.diagonals);s.setValue("crosshair/coordinates",c.coordinates);
    s.setValue("crosshair/bigText",c.bigText);s.setValue("crosshair/transparent",c.transparent);s.setValue("crosshair/whiteBox",c.whiteBox);
    s.setValue("showOverview",action("overview")->isChecked());
    for(const auto &mode:BoardView::modes())s.setValue("keys/"+mode,board->modeKeys.value(mode)?QKeySequence(board->modeKeys.value(mode)).toString(QKeySequence::PortableText):QString());
}
void Editor::setUndoLimit(int steps){undoLimit=std::clamp(steps,1,500);while(past.size()>undoLimit)past.removeFirst();refreshActions();}
void Editor::setAutosave(bool on,int minutes){
    autosave=on;autosaveMinutes=std::clamp(minutes,1,60);if(on)autosaveTimer->start(autosaveMinutes*60000);else autosaveTimer->stop();
}
QString Editor::backupFile() const{
    if(!path.isEmpty())return path+QStringLiteral(".bak");if(importedFile.isEmpty())return {};
    const QFileInfo original(importedFile);return original.dir().filePath(original.completeBaseName()+QStringLiteral(".olpcb.bak"));
}
bool Editor::autosaveNow(){
    if(saveHandler||backupFile().isEmpty()||!modified||revision==backupRevision)return false;
    try{save(doc,backupFile());backupRevision=revision;return true;}catch(const std::exception &){return false;}
}
QString Editor::startFolder(Folder kind) const{
    const int k=oneFolder?Layouts:kind;
    if(!folders.value(k).isEmpty())return folders[k];if(!lastFolders.value(k).isEmpty())return lastFolders[k];
    return board->documentFolder.isEmpty()?QDir::homePath():board->documentFolder;
}
void Editor::usedFolder(Folder kind,const QString &folder){if(!folder.isEmpty())lastFolders[oneFolder?Layouts:kind]=folder;}
void Editor::setColourScheme(int scheme){colourScheme=std::clamp(scheme,0,3);applyColours();}
void Editor::applyColours(){
    board->colours=colourScheme==0?Colours::standard():userColours[colourScheme-1];board->update();
    // The layer names below the board in their colours, darker where the colour is too light to read.
    for(auto it=layerVisible.begin();it!=layerVisible.end();++it){QColor c=board->colours.layers[it.key()];if(c.lightness()>170)c=c.darker(170);
        it.value()->setStyleSheet(QString("QCheckBox{color:%1;font-weight:bold;}").arg(c.name()));}
}
double Editor::minimumTextHeight(int thickness) const{return limitTextSize?(thickness<=0?2.5:thickness>=2?1.0:1.5):.1;}
void Editor::setPlugins(const QList<Plugin> &list){
    pluginList=list;QStringList names,programs;for(const auto &p:list){names.append(p.name);programs.append(p.program);}
    auto s=preferences();s.setValue("pluginNames",names);s.setValue("pluginPrograms",programs);
}
int Editor::runPlugin(const QString &program,QString *message){
    auto tell=[message](const QString &text){if(message)*message=text;};
    QTemporaryDir dir;if(!dir.isValid()){tell(ui("Für die Übergabedatei fehlt ein Ordner."));return -1;}
    // The elements go to the plugin in a Text-IO file; its answer is the file of the same name with "_out".
    auto sel=board->selection();const bool all=sel.isEmpty();if(all)for(int i=0;i<doc.board().elements.size();i++)sel.append(i);
    const QString input=dir.filePath(QStringLiteral("elements.tmp")),output=dir.filePath(QStringLiteral("elements_out.tmp"));
    {QFile f(input);const auto bytes=sprint::textIOBytes(sprint::writeTextIO(extract(doc.board(),sel)));
        if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()){tell(ui("Die Datei kann nicht geschrieben werden: %1").arg(input));return -1;}}
    const auto &b=doc.board();auto units=[](double mm){return QString::number(qint64(std::llround(mm*10000)));};
    int ground=0;for(auto [layer,bit]:{std::pair{CopperTop,1},{CopperBottom,2},{Inner1,4},{Inner2,8}})if(b.groundPlane[layer])ground|=bit;if(b.multilayer)ground|=16;
    QStringList arguments{QDir::toNativeSeparators(input),QStringLiteral("/L:")+(uiLanguage()=="en"?"UK":uiLanguage()=="fr"?"FR":"DE"),
                          QStringLiteral("/W:")+units(b.width),QStringLiteral("/H:")+units(b.height),QStringLiteral("/X:")+units(b.origin.x()),
                          QStringLiteral("/Y:")+units(b.origin.y()),QStringLiteral("/R:")+units(b.grid),QStringLiteral("/M:%1").arg(ground)};
    if(all)arguments<<QStringLiteral("/A");
    arguments<<QStringLiteral("/P:%1").arg(QCoreApplication::applicationPid());
    // The plugin runs while a dialog offers to stop it.
    QProcess process;process.setProgram(program);process.setArguments(arguments);process.setWorkingDirectory(dir.path());
    QEventLoop loop;connect(&process,&QProcess::finished,&loop,&QEventLoop::quit);connect(&process,&QProcess::errorOccurred,&loop,&QEventLoop::quit);
    process.start();
    {QProgressDialog progress(ui("Das Plugin läuft …"),ui("Abbrechen"),0,0,this);progress.setWindowModality(Qt::WindowModal);progress.setMinimumDuration(800);
        connect(&progress,&QProgressDialog::canceled,&process,&QProcess::kill);
        if(process.state()!=QProcess::NotRunning)loop.exec();}
    if(process.error()==QProcess::FailedToStart){tell(ui("Das Plugin kann nicht gestartet werden: %1").arg(program));return -1;}
    if(process.exitStatus()!=QProcess::NormalExit){tell(ui("Das Plugin wurde abgebrochen."));return -1;}
    const int code=process.exitCode();
    if(code==0)return 0;
    if(code>=128&&code<=255){tell(ui("Das Plugin meldet den Fehler %1.").arg(code));return code;}
    if(code>4){tell(ui("Das Plugin endet mit dem unbekannten Code %1.").arg(code));return code;}
    QList<Element> els;
    {QFile f(output);if(!f.open(QIODevice::ReadOnly)){tell(ui("Das Plugin hat keine Ausgabedatei geschrieben."));return code;}
        try{els=sprint::readTextIO(sprint::textIOText(f.readAll()));}
        catch(const std::exception &e){tell(ui("Die Ausgabedatei des Plugins ist fehlerhaft: %1").arg(QString::fromUtf8(e.what())));return code;}}
    // 1 and 3 replace the elements given to the plugin; 1 and 2 put the new ones where they are, 3 and 4 on the pointer.
    const bool replace=code==1||code==3,absolute=code==1||code==2;
    els=placeable(els,doc.board(),{},false);
    if(absolute){
        snapshot();auto &elements=doc.board().elements;if(replace)removeElements(doc.board(),sel);
        const int base=int(elements.size());QList<int> added;
        for(auto e:els){offsetLinks(e,base);elements.append(e);added.append(int(elements.size())-1);}
        board->setSelection(added);board->documentChanged();touched();
    }else{
        if(replace){snapshot();removeElements(doc.board(),sel);board->setSelection({});board->documentChanged();touched();}
        if(!els.isEmpty()){QRectF r;for(const auto &e:els)r=r.united(bounds(e));const QPointF centre=board->snap(r.center());for(auto &e:els)pcb::move(e,-centre);
            board->setTool(BoardView::Tool::Select);board->beginPlacement(els);}
    }
    return code;
}
void Editor::pluginDialog(){
    QDialog dialog(this);dialog.setWindowTitle(ui("Plugin definieren"));auto *v=new QVBoxLayout(&dialog);
    v->addWidget(new QLabel(ui("Ein Plugin ist ein Programm, das die markierten Elemente als Text-IO-Datei bekommt und neue zurückgibt.")));
    auto *list=new QListWidget;v->addWidget(list);auto entries=pluginList;
    auto show=[&]{list->clear();for(const auto &p:entries)list->addItem(QStringLiteral("%1  –  %2").arg(p.name,QDir::toNativeSeparators(p.program)));};show();
    auto *row=new QHBoxLayout;v->addLayout(row);auto *add=new QPushButton(ui("Hinzufügen…"));auto *remove=new QPushButton(ui("Entfernen"));row->addWidget(add);row->addWidget(remove);row->addStretch();
    connect(add,&QPushButton::clicked,&dialog,[&]{
        const auto file=QFileDialog::getOpenFileName(&dialog,ui("Plugin definieren"));if(file.isEmpty())return;
        bool ok=false;const auto name=QInputDialog::getText(&dialog,ui("Plugin definieren"),ui("Name des Plugins"),QLineEdit::Normal,QFileInfo(file).completeBaseName(),&ok);
        if(!ok)return;entries.append({name.isEmpty()?QFileInfo(file).completeBaseName():name,file});show();
    });
    connect(remove,&QPushButton::clicked,&dialog,[&]{const int r=list->currentRow();if(r>=0){entries.removeAt(r);show();}});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);v->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted)setPlugins(entries);
}
void Editor::startPluginDialog(){
    if(pluginList.isEmpty()){QMessageBox::information(this,ui("Plugin starten"),ui("Es ist noch kein Plugin definiert."));pluginDialog();if(pluginList.isEmpty())return;}
    QDialog dialog(this);dialog.setWindowTitle(ui("Plugin starten"));auto *v=new QVBoxLayout(&dialog);
    v->addWidget(new QLabel(board->selection().isEmpty()?ui("Das Plugin bekommt alle Elemente der Platine."):ui("Das Plugin bekommt die %1 markierten Elemente.").arg(board->selection().size())));
    auto *list=new QListWidget;for(const auto &p:pluginList)list->addItem(p.name);list->setCurrentRow(0);v->addWidget(list);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);v->addWidget(buttons);buttons->button(QDialogButtonBox::Ok)->setText(ui("Starten"));
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(list,&QListWidget::itemDoubleClicked,&dialog,&QDialog::accept);
    if(dialog.exec()!=QDialog::Accepted||list->currentRow()<0)return;
    QString message;const int code=runPlugin(pluginList[list->currentRow()].program,&message);
    if(!message.isEmpty()){if(code<0||code>4)QMessageBox::warning(this,ui("Plugin starten"),message);else QMessageBox::information(this,ui("Plugin starten"),message);}
}
void Editor::preferencesDialog(const QString &shown){
    QDialog dialog(this);dialog.setWindowTitle(ui("Grundeinstellungen"));auto *v=new QVBoxLayout(&dialog);auto *pages=new QTabWidget;pages->setObjectName("pages");pages->setUsesScrollButtons(false);v->addWidget(pages);
    auto page=[&](const QString &title,const QString &name={}){auto *w=new QWidget;w->setObjectName(name);auto *l=new QVBoxLayout(w);pages->addTab(w,title);if(!name.isEmpty()&&name==shown)pages->setCurrentWidget(w);return l;};
    auto box=[](QVBoxLayout *l,const QString &name,const QString &text,bool on){auto *c=new QCheckBox(text);c->setObjectName(name);c->setChecked(on);l->addWidget(c);return c;};
    // General: units, drill holes and the switches.
    auto *general=page(ui("Allgemein"));auto *top=new QFormLayout;general->addLayout(top);
    auto *units=new QComboBox;units->setObjectName("units");units->addItems({ui("Millimeter"),ui("mil (1/1000 Zoll)")});units->setCurrentIndex(board->milUnits?1:0);top->addRow(ui("Einheit"),units);
    auto *holes=new QComboBox;holes->setObjectName("holes");holes->addItems({ui("in der Farbe der Platine"),ui("weiß"),ui("schwarz")});holes->setCurrentIndex(board->holes);top->addRow(ui("Bohrlöcher"),holes);
    auto *overviewShown=box(general,"showOverview",ui("Übersicht unter den Werkzeugen zeigen"),action("overview")->isChecked());
    auto *dark=box(general,"darkGround",ui("AutoMasse abgedunkelt darstellen"),board->darkGround);auto *all=box(general,"allGrounds",ui("AutoMasse von allen Layern gleichzeitig darstellen"),board->allGrounds);
    auto *test=box(general,"testAirwires",ui("Bei Test-Funktion die Luftlinien mit berücksichtigen"),board->testAirwires);auto *blink=box(general,"blinkTest",ui("Ergebnis des Tests blinkt"),board->blinkTest);
    auto *sizes=box(general,"takeSizes",ui("Doppelklick übernimmt Größenparameter von Elementen"),board->takeSizes);
    auto *limit=box(general,"limitTextSize",ui("Texthöhe so begrenzen, dass Striche mindestens 0,15 mm breit sind"),limitTextSize);
    auto *readable=box(general,"readableLabels",ui("Bauteilbezeichner und Wert nach Rotation immer lesbar"),readableLabels);
    auto *nodes=box(general,"optimizeNodes",ui("Knotenpunkte der Leiterbahnen automatisch optimieren"),board->optimizeNodes);
    auto *topLeft=box(general,"originTopLeft",ui("Koordinatenursprung neuer Platinen oben links"),originTopLeft);
    auto *cam=box(general,"camOrigin",ui("Fertigungsdaten (Gerber, Bohr-, Fräs- und Bauteildaten) vom Koordinatenursprung aus"),camOrigin);
    cam->setToolTip(ui("Ohne diesen Schalter zählen sie von der linken oberen Ecke der Arbeitsfläche."));general->addStretch();
    // Colours: the reference's scheme stays as it is, three schemes of one's own.
    auto *colourPage=page(ui("Farben"));auto *cf=new QFormLayout;colourPage->addLayout(cf);
    auto *scheme=new QComboBox;scheme->setObjectName("colourScheme");scheme->addItems({ui("Standard"),ui("Eigene Farben 1"),ui("Eigene Farben 2"),ui("Eigene Farben 3")});cf->addRow(ui("Farbschema"),scheme);
    auto own=userColours;int shownScheme=colourScheme;scheme->setCurrentIndex(colourScheme);
    const QStringList colourLabels{ui("Platine (Hintergrund)"),ui("Raster (Linien)"),gerberOutputName(CopperTop),gerberOutputName(SilkTop),gerberOutputName(CopperBottom),gerberOutputName(SilkBottom),
                                   gerberOutputName(Inner1),gerberOutputName(Inner2),gerberOutputName(Outline),ui("Durchkontaktierung"),ui("Luftlinie"),ui("Raster (Punkte)")};
    auto colourAt=[](Colours &c,int k)->QColor&{return k==0?c.board:k==1?c.grid:k==9?c.via:k==10?c.airwire:k==11?c.dots:c.layers[k-1];};
    QList<QToolButton*> swatches;auto *standard=new QPushButton(ui("Standardfarben übernehmen"));standard->setObjectName("standardColours");
    auto showColours=[&]{
        Colours c=shownScheme==0?Colours::standard():own[shownScheme-1];
        for(int k=0;k<swatches.size();k++){QPixmap swatch(28,14);swatch.fill(colourAt(c,k));QIcon icon;icon.addPixmap(swatch,QIcon::Normal);icon.addPixmap(swatch,QIcon::Disabled);
            swatches[k]->setIcon(icon);swatches[k]->setEnabled(shownScheme>0);}
        standard->setEnabled(shownScheme>0);
    };
    for(int k=0;k<colourLabels.size();k++){
        auto *b=new QToolButton;b->setObjectName(QStringLiteral("colour-%1").arg(k));b->setIconSize(QSize(28,14));swatches.append(b);cf->addRow(colourLabels[k],b);
        connect(b,&QToolButton::clicked,&dialog,[&,k]{if(shownScheme<1)return;QColor &c=colourAt(own[shownScheme-1],k);const QColor chosen=QColorDialog::getColor(c,&dialog);if(chosen.isValid()){c=chosen;showColours();}});
    }
    cf->addRow(standard);colourPage->addWidget(new QLabel(ui("Das Schema „Standard“ ist das des Vorbilds und bleibt unverändert.")));colourPage->addStretch();
    connect(scheme,&QComboBox::currentIndexChanged,&dialog,[&](int k){shownScheme=k;showColours();});
    connect(standard,&QPushButton::clicked,&dialog,[&]{if(shownScheme>0){own[shownScheme-1]=Colours::standard();showColours();}});
    showColours();
    // Folders: where the file dialogs start; the macro folder.
    auto *folderPage=page(ui("Ordner"),QStringLiteral("folders"));auto *ff=new QFormLayout;folderPage->addLayout(ff);
    const QStringList folderLabels{ui("Layouts"),ui("Fertigungsdaten"),ui("Bilder"),ui("Vorlagen"),ui("Fräsdaten")};QList<QLineEdit*> folderEdits;
    auto folderRow=[&](const QString &name,const QString &value){
        auto *row=new QHBoxLayout;auto *edit=new QLineEdit(value);edit->setObjectName(name);auto *pick=new QToolButton;pick->setText(QStringLiteral("…"));row->addWidget(edit,1);row->addWidget(pick);
        connect(pick,&QToolButton::clicked,&dialog,[&dialog,edit]{const auto chosen=QFileDialog::getExistingDirectory(&dialog,ui("Ordner wählen"),edit->text());if(!chosen.isEmpty())edit->setText(chosen);});
        return std::pair{edit,row};};
    for(int k=0;k<5;k++){auto [edit,row]=folderRow(QStringLiteral("folder-%1").arg(k),folders[k]);folderEdits.append(edit);ff->addRow(folderLabels[k],row);}
    auto *one=new QCheckBox(ui("Ein Ordner für alle Dateiarten (der der Layouts)"));one->setObjectName("oneFolder");one->setChecked(oneFolder);ff->addRow(one);
    auto enableFolders=[&]{for(int k=1;k<5;k++)folderEdits[k]->setEnabled(!one->isChecked());};connect(one,&QCheckBox::toggled,&dialog,enableFolders);enableFolders();
    ff->addRow(new QLabel(ui("Ein leeres Feld: der zuletzt benutzte Ordner.")));
    auto [macroEdit,macroRow]=folderRow("macroFolder",macroFolder);ff->addRow(ui("Makros"),macroRow);
    {auto *buttons=new QHBoxLayout;auto *reset=new QPushButton(ui("Zurücksetzen"));auto *open=new QPushButton(ui("Im Dateimanager zeigen"));buttons->addWidget(reset);buttons->addWidget(open);buttons->addStretch();ff->addRow(buttons);
        connect(reset,&QPushButton::clicked,&dialog,[macroEdit]{macroEdit->clear();});
        connect(open,&QPushButton::clicked,&dialog,[macroEdit]{if(!macroEdit->text().isEmpty())QDesktopServices::openUrl(QUrl::fromLocalFile(macroEdit->text()));});}
    folderPage->addStretch();
    // Undo steps.
    auto *undoPage=page(ui("Rückgängig"));auto *uf=new QFormLayout;undoPage->addLayout(uf);auto *steps=new QSpinBox;steps->setObjectName("undoSteps");steps->setRange(1,500);steps->setValue(undoLimit);
    uf->addRow(ui("Rückgängig-Schritte"),steps);auto *undoNote=new QLabel(ui("Jeder Schritt hält eine Kopie des Dokuments. Bei sehr großen Layouts spart eine kleinere Zahl Speicher."));
    undoNote->setWordWrap(true);undoPage->addWidget(undoNote);undoPage->addStretch();
    // Imax: copper thickness and temperature rise for the current a track carries.
    auto *imaxPage=page(ui("Imax"));auto *mf=new QFormLayout;imaxPage->addLayout(mf);
    auto *copper=spin(copperThickness,1,299,0,QStringLiteral(" µm"));copper->setObjectName("copperThickness");auto *rise=spin(temperatureRise,1,299,0,QStringLiteral(" K"));rise->setObjectName("temperatureRise");
    mf->addRow(ui("Kupferstärke"),copper);mf->addRow(ui("Erwärmung"),rise);
    auto *imaxNote=new QLabel(ui("Imax = 3,675 · √(ΔT · t · b · (t + b)) mit Leiterbahnbreite b und Kupferstärke t in mm, Erwärmung ΔT in K, Strom in A. Eine grobe Näherung: Umgebung, Belüftung und Nachbarn ändern den Wert."));
    imaxNote->setWordWrap(true);imaxPage->addWidget(imaxNote);imaxPage->addStretch();
    // Mode keys: pick a mode, press its new key; a key taken by another mode moves over.
    auto keys=board->modeKeys;
    auto *keyPage=page(ui("Tasten"));auto *list=new QTreeWidget;list->setObjectName("modeKeys");list->setRootIsDecorated(false);list->setHeaderLabels({ui("Modus"),ui("Taste")});keyPage->addWidget(list);
    auto keyText=[](int key){return key?QKeySequence(key).toString(QKeySequence::NativeText):QString();};
    auto show=[&]{const int row=list->currentIndex().row();list->clear();
        for(const auto &mode:BoardView::modes())list->addTopLevelItem(new QTreeWidgetItem({BoardView::modeName(mode),keyText(keys.value(mode))}));
        if(row>=0)list->setCurrentItem(list->topLevelItem(row));};
    show();
    auto *keyRow=new QHBoxLayout;keyPage->addLayout(keyRow);auto *edit=new QKeySequenceEdit;edit->setMaximumSequenceLength(1);
    auto *standardKeys=new QPushButton(ui("Standard"));keyRow->addWidget(new QLabel(ui("Neue Taste")));keyRow->addWidget(edit,1);keyRow->addWidget(standardKeys);
    connect(edit,&QKeySequenceEdit::editingFinished,&dialog,[&]{
        const int row=list->currentIndex().row();if(row<0||edit->keySequence().isEmpty())return;
        const auto combined=edit->keySequence()[0];const int key=combined.key();
        // Digits choose grids, and these keys draw and move: they cannot be mode keys.
        static const QList<int> taken{Qt::Key_Space,Qt::Key_Return,Qt::Key_Enter,Qt::Key_Left,Qt::Key_Right,Qt::Key_Up,Qt::Key_Down,Qt::Key_Delete,Qt::Key_Backspace,Qt::Key_Tab};
        if(combined.keyboardModifiers()!=Qt::NoModifier||(key>=Qt::Key_0&&key<=Qt::Key_9)||taken.contains(key)){
            QMessageBox::information(&dialog,ui("Tasten der Modi"),ui("Diese Taste kann keinen Modus wählen."));edit->clear();return;}
        for(auto it=keys.begin();it!=keys.end();++it)if(it.value()==key)it.value()=0;
        keys[BoardView::modes()[row]]=key;edit->clear();show();
    });
    connect(standardKeys,&QPushButton::clicked,&dialog,[&]{keys=BoardView::defaultModeKeys();show();});
    // The cross hair.
    auto *crossPage=page(ui("Fadenkreuz"));const auto &cross=board->crosshair;
    auto *lines=box(crossPage,"crossLines",ui("Linien über die ganze Ansicht"),cross.lines);auto *diagonals=box(crossPage,"crossDiagonals",ui("Dazu Linien unter 45°"),cross.diagonals);
    auto *coordinates=box(crossPage,"crossCoordinates",ui("Koordinaten neben dem Fadenkreuz"),cross.coordinates);auto *big=box(crossPage,"crossBigText",ui("Große Schrift"),cross.bigText);
    auto *transparent=box(crossPage,"crossTransparent",ui("Ohne Hintergrund"),cross.transparent);
    auto *backs=new QHBoxLayout;auto *black=new QRadioButton(ui("Schwarzer Hintergrund"));auto *white=new QRadioButton(ui("Weißer Hintergrund"));white->setObjectName("crossWhite");
    (cross.whiteBox?white:black)->setChecked(true);backs->addWidget(black);backs->addWidget(white);backs->addStretch();crossPage->addLayout(backs);crossPage->addStretch();
    // Backups.
    auto *savePage=page(ui("Sichern"),QStringLiteral("backups"));auto *saving=box(savePage,"autosave",ui("Regelmäßig eine Sicherung schreiben"),autosave);
    auto *sf=new QFormLayout;savePage->addLayout(sf);auto *minutes=new QSpinBox;minutes->setObjectName("autosaveMinutes");minutes->setRange(1,60);minutes->setValue(autosaveMinutes);minutes->setSuffix(ui(" min"));
    sf->addRow(ui("Alle"),minutes);auto *saveNote=new QLabel(ui("Die Sicherung liegt neben der Datei des Dokuments, mit „.bak“ hinter dem Namen, im eigenen Format; geschrieben wird nur nach Änderungen. Zum Laden die Endung entfernen."));
    saveNote->setWordWrap(true);savePage->addWidget(saveNote);savePage->addStretch();
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);v->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    board->milUnits=units->currentIndex()==1;board->holes=holes->currentIndex();board->darkGround=dark->isChecked();board->allGrounds=all->isChecked();
    board->testAirwires=test->isChecked();board->blinkTest=blink->isChecked();board->takeSizes=sizes->isChecked();board->optimizeNodes=nodes->isChecked();
    action("overview")->setChecked(overviewShown->isChecked());overview->setVisible(overviewShown->isChecked());
    limitTextSize=limit->isChecked();readableLabels=readable->isChecked();originTopLeft=topLeft->isChecked();camOrigin=cam->isChecked();
    userColours=own;setColourScheme(scheme->currentIndex());
    for(int k=0;k<5;k++)folders[k]=folderEdits[k]->text().trimmed();oneFolder=one->isChecked();
    if(macroEdit->text().trimmed()!=macroFolder){macroFolder=macroEdit->text().trimmed();macros->setFolders(macroFolder,extraMacroFolders);}
    setUndoLimit(steps->value());copperThickness=copper->value();temperatureRise=rise->value();board->modeKeys=keys;refreshToolKeys();
    board->crosshair={lines->isChecked(),diagonals->isChecked(),coordinates->isChecked(),big->isChecked(),transparent->isChecked(),white->isChecked()};action("crosshair")->setChecked(lines->isChecked());
    setAutosave(saving->isChecked(),minutes->value());board->update();if(board->unitsChanged)board->unitsChanged();refreshProperties();savePreferences();
}
// Designators and values turned upside down or reading downwards are turned round, so they read from the left or
// from below.
void Editor::keepReadable(Element &e) const{
    if(!readableLabels||e.type!=ElementType::Text||e.role==TextRole::Plain||e.mirrored)return;
    const double r=std::fmod(std::fmod(e.rotation,360.0)+360.0,360.0);if(r>90+1e-6&&r<=270+1e-6)rotate(e,bounds(e).center(),180);
}
Board Editor::freshBoard(const QString &name) const{Board b=newBoard(name);if(originTopLeft)b.origin={0,0};return b;}
void Editor::wizardDialog(){
    // The form at the left, pads, count and the form's measures, a preview; OK puts the component on the pointer.
    Wizard w=wizardDefaults(Wizard::DoubleRow);
    QDialog dialog(this);dialog.setWindowTitle(ui("Bauteil-Assistent"));auto *h=new QHBoxLayout(&dialog);
    auto *forms=new QListWidget;forms->addItems({ui("Einzelne Reihe (SIP)"),ui("Doppelte Reihe (DIP)"),ui("Quadratisch (QUAD)"),ui("Kreisform"),ui("Kreisform 2-reihig")});
    forms->setCurrentRow(int(w.form));forms->setMaximumWidth(190);h->addWidget(forms);
    auto *middle=new QVBoxLayout;h->addLayout(middle);auto *form=new QFormLayout;middle->addLayout(form);
    auto *tht=new QRadioButton(ui("Lötaugen"));auto *smdPads=new QRadioButton(ui("SMD-Pads"));auto *kinds=new QHBoxLayout;kinds->addWidget(tht);kinds->addWidget(smdPads);
    auto *diameter=spin(1,.1,20,2),*drill=spin(1,0,10,2),*length=spin(1,.05,20,2),*width=spin(1,.05,20,2);auto *count=new QSpinBox;count->setRange(1,500);
    auto *pitch=spin(1,.1,50,3),*rows=spin(1,.1,200,3),*quadWidth=spin(1,.1,200,3),*quadHeight=spin(1,.1,200,3),*circle=spin(1,.1,200,3),*inner=spin(1,.1,200,3);
    form->addRow(ui("Lötpads"),kinds);form->addRow(ui("Lötauge Ø"),diameter);form->addRow(ui("Bohrung Ø"),drill);form->addRow(ui("SMD-Länge"),length);form->addRow(ui("SMD-Breite"),width);
    form->addRow(ui("Anzahl der Pads"),count);form->addRow(ui("Rastermaß"),pitch);form->addRow(ui("Reihenabstand"),rows);form->addRow(ui("Abstand links–rechts"),quadWidth);
    form->addRow(ui("Abstand oben–unten"),quadHeight);form->addRow(ui("Kreisdurchmesser"),circle);form->addRow(ui("Innerer Kreisdurchmesser"),inner);
    auto *defaults=new QPushButton(ui("Vorgabe"));middle->addWidget(defaults);middle->addStretch();
    auto *preview=new ArrangePreview;preview->place=[](QList<Element>&,int){};h->addWidget(preview,1);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);middle->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    bool filling=false;
    auto show=[&]{
        filling=true;tht->setChecked(!w.smd);smdPads->setChecked(w.smd);diameter->setValue(w.diameter);drill->setValue(w.drill);length->setValue(w.length);width->setValue(w.width);
        count->setValue(w.count);pitch->setValue(w.pitch);rows->setValue(w.rowSpacing);quadWidth->setValue(w.quadWidth);quadHeight->setValue(w.quadHeight);circle->setValue(w.circle);inner->setValue(w.innerCircle);
        // Only the measures the form uses.
        diameter->setEnabled(!w.smd);drill->setEnabled(!w.smd);length->setEnabled(w.smd);width->setEnabled(w.smd);
        pitch->setEnabled(w.form<=Wizard::Quad);rows->setEnabled(w.form==Wizard::DoubleRow);quadWidth->setEnabled(w.form==Wizard::Quad);quadHeight->setEnabled(w.form==Wizard::Quad);
        circle->setEnabled(w.form>=Wizard::Circle);inner->setEnabled(w.form==Wizard::DoubleCircle);
        filling=false;preview->original=wizardFootprint(w).elements;preview->update();
    };
    auto read=[&]{
        if(filling)return;w.smd=smdPads->isChecked();w.diameter=diameter->value();w.drill=drill->value();w.length=length->value();w.width=width->value();w.count=count->value();
        w.pitch=pitch->value();w.rowSpacing=rows->value();w.quadWidth=quadWidth->value();w.quadHeight=quadHeight->value();w.circle=circle->value();w.innerCircle=inner->value();show();
    };
    show();
    connect(forms,&QListWidget::currentRowChanged,&dialog,[&](int row){if(row>=0){Wizard next=wizardDefaults(Wizard::Form(row));next.smd=w.smd;w=next;show();}});
    connect(defaults,&QPushButton::clicked,&dialog,[&]{w=wizardDefaults(w.form);show();});
    connect(smdPads,&QRadioButton::toggled,&dialog,read);connect(count,&QSpinBox::valueChanged,&dialog,read);
    for(auto *box:{diameter,drill,length,width,pitch,rows,quadWidth,quadHeight,circle,inner})connect(box,&QDoubleSpinBox::valueChanged,&dialog,read);
    dialog.resize(900,520);if(dialog.exec()!=QDialog::Accepted)return;
    board->setTool(BoardView::Tool::Select);board->beginPlacement(placeable(wizardFootprint(w),doc.board()));
}
void Editor::templateDialog(){
    // A tab per side: load or remove the picture, show it, its resolution and where it lies; applied as one undo step.
    auto templates=doc.board().templates;
    QDialog dialog(this);dialog.setWindowTitle(ui("Vorlage"));auto *v=new QVBoxLayout(&dialog);auto *tabs=new QTabWidget;v->addWidget(tabs);
    for(int k=0;k<2;k++){
        auto &t=templates[k];auto *page=new QWidget;auto *form=new QFormLayout(page);
        auto *file=new QLabel;file->setWordWrap(true);auto *load=new QPushButton(ui("Vorlage laden…"));auto *remove=new QPushButton(ui("Vorlage entfernen"));
        auto *shown=new QCheckBox(ui("Vorlage anzeigen"));auto *dpi=spin(t.dpi,1,100000,0,QStringLiteral(" dpi"));auto *x=spin(t.offset.x(),-10000,10000,3),*y=spin(t.offset.y(),-10000,10000,3);
        // The colour of one-bit pictures.
        auto *colour=new QToolButton;colour->setObjectName("templateColour"+QString::number(k));colour->setToolTip(ui("Farbe wählen"));
        auto show=[file,shown,dpi,colour,&t]{
            file->setText(t.file.isEmpty()?ui("keine Vorlage"):t.file);shown->setChecked(t.shown);dpi->setValue(t.dpi);QPixmap swatch(16,16);swatch.fill(t.colour);colour->setIcon(QIcon(swatch));
        };show();
        auto *row=new QHBoxLayout;row->addWidget(load);row->addWidget(remove);
        form->addRow(ui("Datei"),file);form->addRow(row);form->addRow(shown);form->addRow(ui("Auflösung"),dpi);form->addRow(ui("X-Versatz"),x);form->addRow(ui("Y-Versatz"),y);form->addRow(ui("Farbe"),colour);
        connect(colour,&QToolButton::clicked,&dialog,[this,&t,show]{const QColor c=QColorDialog::getColor(t.colour,this);if(c.isValid()){t.colour=c;show();}});
        connect(load,&QPushButton::clicked,&dialog,[this,&t,show]{
            const auto name=QFileDialog::getOpenFileName(this,ui("Vorlage laden"),startFolder(Templates),ui("Bilder (*.bmp *.jpg *.jpeg *.png)"));if(name.isEmpty())return;
            usedFolder(Templates,QFileInfo(name).absolutePath());
            // The resolution stored in the picture, if it has one.
            QImage image(name);if(image.isNull()){QMessageBox::warning(this,ui("Vorlage laden"),ui("Das Bild kann nicht gelesen werden."));return;}
            t.file=name;t.shown=true;if(image.dotsPerMeterX()>0)t.dpi=std::round(image.dotsPerMeterX()*.0254);show();
        });
        connect(remove,&QPushButton::clicked,&dialog,[&t,show]{t=Template{};show();});
        connect(shown,&QCheckBox::toggled,&dialog,[&t](bool on){t.shown=on;});
        connect(dpi,&QDoubleSpinBox::valueChanged,&dialog,[&t](double value){t.dpi=value;});
        connect(x,&QDoubleSpinBox::valueChanged,&dialog,[&t](double value){t.offset.setX(value);});connect(y,&QDoubleSpinBox::valueChanged,&dialog,[&t](double value){t.offset.setY(value);});
        tabs->addTab(page,k==0?ui("Seite 1 (Oberseite)"):ui("Seite 2 (Unterseite)"));
    }
    tabs->setCurrentIndex(doc.board().activeLayer==CopperBottom||doc.board().activeLayer==SilkBottom?1:0);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);v->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted||templates==doc.board().templates)return;
    snapshot();doc.board().templates=templates;board->update();touched();
}
void Editor::printPreview(){
    // The settings stay from one printout to the next; a new board starts with its visible layers, centred.
    if(!printSettingsSet){printSettings=defaultPrintSettings(doc.board());}
    for(int k=0;k<2;k++){const auto &t=doc.board().templates[k];printSettings.templatePictures[k]=t.file.isEmpty()?QImage():board->templatePicture(t.file);}
    PrintPreview preview(doc.board(),printSettings,path.isEmpty()?displayName():path,printerDevice(),this);if(!printSettingsSet)preview.centre();
    preview.exec();printSettings=preview.settings();printSettingsSet=true;
}
void Editor::holeListDialog(){
    // The holes of the board by diameter, plain ones and through-plated ones as chosen, to read or to copy.
    QDialog dialog(this);dialog.setWindowTitle(ui("Bohrungen"));auto *v=new QVBoxLayout(&dialog);
    auto *plain=new QCheckBox(ui("Einfache Bohrungen"));auto *plated=new QCheckBox(ui("Durchkontaktierungen"));plain->setObjectName("holesPlain");plated->setObjectName("holesPlated");
    plain->setChecked(true);plated->setChecked(true);v->addWidget(plain);v->addWidget(plated);
    auto *list=new QPlainTextEdit;list->setObjectName("holeList");list->setReadOnly(true);list->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));list->setMinimumSize(260,240);v->addWidget(list);
    auto show=[&]{list->setPlainText(holeList(doc.board(),plain->isChecked(),plated->isChecked()));};show();
    connect(plain,&QCheckBox::toggled,&dialog,show);connect(plated,&QCheckBox::toggled,&dialog,show);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok);auto *copy=buttons->addButton(ui("In die Zwischenablage"),QDialogButtonBox::ActionRole);copy->setObjectName("copyHoles");v->addWidget(buttons);
    connect(copy,&QPushButton::clicked,&dialog,[list]{QApplication::clipboard()->setText(list->toPlainText());});
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);dialog.exec();
}
QPrinter *Editor::printerDevice(){if(!printer)printer=std::make_shared<QPrinter>(QPrinter::HighResolution);return printer.get();}
void Editor::setRotationStep(double degrees){
    rotation=degrees;
    if(auto *a=action("rotate"))a->setText(ui("Drehen (%1° im Uhrzeigersinn)").arg(uiLocale().toString(degrees)));
    if(auto *a=action("rotationAngle"))for(auto *step:a->menu()->actions())if(step->isCheckable()){
        const QString name=step->objectName();const bool listed=degrees==90||degrees==45||degrees==30||degrees==15||degrees==5;
        step->setChecked(name==QString("rotationStep%1").arg(degrees)||(name=="rotationStepFree"&&!listed));}
}
void Editor::otherSide(){
    const auto sel=board->selection();if(sel.isEmpty())return;const double axis=board->snap(selectionBounds(doc.board(),sel).center()).x();
    editElements(sel,[&](Element &e){mirror(e,axis);e.layer=otherLayer(e.layer);});
}
void Editor::groupSelection(){
    const auto sel=board->selection();if(sel.size()<2)return;const int g=nextGroup(doc.board());editElements(sel,[g](Element &e){e.groups.append(g);});
}
void Editor::ungroupSelection(){editElements(board->selection(),[](Element &e){if(!e.groups.isEmpty())e.groups.removeLast();});}
void Editor::copySelection(){
    const auto sel=board->selection();if(sel.isEmpty())return;
    Document clip;Board b=newBoard(QStringLiteral("clipboard"),doc.board().width,doc.board().height);b.elements=extract(doc.board(),sel);clip.boards={b};
    auto *mime=new QMimeData;mime->setData(clipboardMime,encode(clip));QApplication::clipboard()->setMimeData(mime);
}
void Editor::pasteClipboard(){
    const auto *mime=QApplication::clipboard()->mimeData();if(!mime||!mime->hasFormat(clipboardMime))return;
    QList<Element> els;try{els=decode(mime->data(clipboardMime)).boards.value(0).elements;}catch(const FormatError &){return;}
    if(els.isEmpty())return;
    QRectF r;for(const auto &e:els)r=r.united(bounds(e));const QPointF centre=board->snap(r.center());for(auto &e:els)pcb::move(e,-centre);
    board->setTool(BoardView::Tool::Select);board->beginPlacement(placeable(els,doc.board()));
}
void Editor::duplicateSelection(){
    const auto sel=board->selection();if(sel.isEmpty())return;
    auto els=placeable(extract(doc.board(),sel),doc.board());const double g=doc.board().grid;QList<int> added;
    snapshot();const int base=int(doc.board().elements.size());
    for(auto e:els){pcb::move(e,{g*2,g*2});offsetLinks(e,base);doc.board().elements.append(e);added.append(int(doc.board().elements.size())-1);}
    board->setSelection(added);board->documentChanged();touched();
}
void Editor::importBoard(const Board &imported,bool fresh){
    snapshot();
    if(fresh){doc.boards.append(imported);doc.activeBoard=int(doc.boards.size())-1;}
    else{
        // The files' origin goes to the active board's origin.
        auto &b=doc.board();const QPointF delta=b.origin-imported.origin;const int base=int(b.elements.size());
        for(auto e:imported.elements){pcb::move(e,delta);offsetLinks(e,base);b.elements.append(e);}
        for(int l=1;l<=layerCount;l++)b.groundPlane[l]=b.groundPlane[l]||imported.groundPlane[l];b.multilayer=b.multilayer||imported.multilayer;
    }
    assignIds(doc);board->documentChanged();refresh();touched();
}
void Editor::addBoard(){addBoard(freshBoard(ui("Platine %1").arg(doc.boards.size()+1)));}
void Editor::addBoard(const Board &added){
    snapshot();doc.boards.append(added);doc.activeBoard=int(doc.boards.size())-1;assignIds(doc);
    board->setDocument(&doc);touched();refresh();
}
void Editor::removeBoard(){
    if(doc.boards.size()<2)return;
    if(QMessageBox::question(this,ui("Platine löschen"),ui("Die Platine „%1“ mit allen Elementen löschen?").arg(doc.board().name))!=QMessageBox::Yes)return;
    snapshot();doc.boards.removeAt(doc.activeBoard);doc.activeBoard=std::min<int>(doc.activeBoard,int(doc.boards.size())-1);board->setDocument(&doc);touched();refresh();
}
bool Editor::copyBoard(){
    snapshot();Board copy=doc.board();copy.name=ui("%1 (Kopie)").arg(copy.name);copy.id.clear();
    doc.boards.insert(doc.activeBoard+1,copy);doc.activeBoard++;
    // The copy's components are components of their own: identifiers anew for it only.
    for(auto &e:doc.boards[doc.activeBoard].elements)if(e.role==TextRole::Designator)e.component.clear();
    assignIds(doc);board->setDocument(&doc);touched();refresh();return true;
}
bool Editor::moveBoard(bool toEnd){
    const int from=doc.activeBoard,to=toEnd?int(doc.boards.size())-1:0;if(from==to)return false;
    snapshot();doc.boards.move(from,to);doc.activeBoard=to;board->setDocument(&doc);touched();refresh();return true;
}
bool Editor::insertBoards(const QString &file,bool last,QString *error){
    Document other;QStringList found;
    try{other=load(file,&found);}catch(const std::exception &e){if(error)*error=QString::fromUtf8(e.what());return false;}
    if(other.boards.isEmpty()){if(error)*error=ui("Die Datei enthält keine Platine");return false;}
    // Boards and components whose identifiers are here already get identifiers of their own; those here keep theirs.
    QSet<QString> boards,parts;
    for(const auto &b:std::as_const(doc).boards){boards.insert(b.id);for(const auto &e:b.elements)if(e.role==TextRole::Designator&&!e.component.isEmpty())parts.insert(e.component);}
    for(auto &b:other.boards){if(boards.contains(b.id))b.id.clear();for(auto &e:b.elements)if(e.role==TextRole::Designator&&parts.contains(e.component))e.component.clear();}
    snapshot();const int at=last?int(doc.boards.size()):doc.activeBoard+1;
    for(qsizetype k=0;k<other.boards.size();k++)doc.boards.insert(at+k,other.boards[k]);
    doc.activeBoard=at;assignIds(doc);board->setDocument(&doc);touched();refresh();
    // What converting an older file changed or left out shows above the board, as after opening it.
    if(!found.isEmpty()){notes=found;noticeText->setText(found.join('\n'));noticeBar->setVisible(true);}
    return true;
}
bool Editor::saveBoardAs(const QString &file,QString *error){
    // Never over the document's own file (or the file it was imported from): its other boards would be lost.
    const QString target=QFileInfo(file).absoluteFilePath();
    for(const QString &mine:{path,importedFile})if(!mine.isEmpty()&&QFileInfo(mine).absoluteFilePath()==target){
        if(error)*error=ui("Die Platine kann nicht in die Datei des geöffneten Dokuments gespeichert werden.");return false;}
    Document alone;alone.boards={doc.board()};alone.activeBoard=0;
    try{save(alone,file);return true;}catch(const std::exception &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
void Editor::switchBoard(int index){
    if(index<0||index>=doc.boards.size()||index==doc.activeBoard)return;
    doc.activeBoard=index;board->setDocument(&doc);refresh();
}
void Editor::setGroundPlane(bool on){
    {const auto &b=std::as_const(doc).board();if(!isCopper(b.activeLayer)||b.groundPlane[b.activeLayer]==on)return;}
    // The board only after the snapshot: a reference taken before it would change the copy kept for undo as well.
    snapshot();auto &b=doc.board();b.groundPlane[b.activeLayer]=on;board->documentChanged();touched();refreshLayers();
}
int Editor::removeRoutedAirwires(){
    const auto routed=routedAirwires(doc.board());if(routed.isEmpty())return 0;
    snapshot();auto &els=doc.board().elements;
    for(auto [a,c]:routed){els[a].connections.removeAll(c);els[c].connections.removeAll(a);}
    board->documentChanged();touched();return int(routed.size());
}
void Editor::resetSolderMask(){
    QList<int> all;for(int i=0;i<doc.board().elements.size();i++)all.append(i);
    editElements(all,[](Element &e){e.solderMask=e.type==ElementType::Pad||e.type==ElementType::SmdPad;});
}
Rules Editor::designRules() const{
    Rules r;auto on=[this](const char *key){return ruleChecks.value(key)->isChecked();};auto value=[this](const char *key){return ruleBoxes.value(key)->value();};
    r.clearanceOn=on("clearance");r.clearance=value("clearance");r.holeDistanceOn=on("holeDistance");r.holeDistance=value("holeDistance");
    r.minDrillOn=on("minDrill");r.minDrill=value("minDrill");r.maxDrillOn=on("maxDrill");r.maxDrill=value("maxDrill");r.minTrackOn=on("minTrack");r.minTrack=value("minTrack");
    r.minRingOn=on("minRing");r.minRing=value("minRing");r.minSilkOn=on("minSilk");r.minSilk=value("minSilk");
    r.silkOnPads=on("silkOnPads");r.holesOnSmd=on("holesOnSmd");r.padsWithoutMask=on("padsWithoutMask");r.maskOutsidePads=on("maskOutsidePads");
    return r;
}
QList<Finding> Editor::runDesignRuleCheck(bool visibleOnly){
    Rules rules=designRules();if(visibleOnly)rules.window=board->visibleArea();
    QList<Finding> found;{ProgressWindow progress(this,ui("Design Rule Check läuft …"));QApplication::setOverrideCursor(Qt::WaitCursor);found=checkDesign(doc.board(),rules,progress.report());QApplication::restoreOverrideCursor();}
    const bool was=refreshing;refreshing=true;findingList->clear();for(const auto &f:found)findingList->addItem(QString("%1 (%2)").arg(f.message,layerName(f.layer)));refreshing=was;
    findingSummary->setText(found.isEmpty()?ui("Keine Verstöße gefunden."):found.size()==1?ui("Ein Verstoß gefunden."):ui("%1 Verstöße gefunden.").arg(found.size()));
    board->setFindings(found);findingList->selectAll();
    for(QWidget *w=findingList;w;w=w->parentWidget())if(auto *side=qobject_cast<QTabWidget*>(w)){if(auto *page=side->findChild<QWidget*>("drcPage"))side->setCurrentWidget(page);break;}
    return found;
}
// --- the schematic of the project
void Editor::projectChanged(){refreshActions();refreshSchematic();}
documents::Targets Editor::schematicForBoard() const{
    auto t=targets();
    // A component the document has on another board belongs there: the active board is compared without it and its pins.
    QSet<QString> here,elsewhere;
    for(int k=0;k<doc.boards.size();k++)for(const auto &e:doc.boards[k].elements)if(e.role==TextRole::Designator&&!e.component.isEmpty())(k==doc.activeBoard?here:elsewhere).insert(e.component);
    elsewhere.subtract(here);if(elsewhere.isEmpty())return t;
    documents::Targets out;for(const auto &c:t.components)if(!elsewhere.contains(c.id))out.components<<c;
    for(const auto &n:t.nets){documents::TargetNet kept{n.name,{}};for(const auto &p:n.pins)if(!elsewhere.contains(p.component))kept.pins<<p;out.nets<<kept;}
    return out;
}
void Editor::refreshSchematic(){
    if(!schematicPage||!schematicList)return;
    if(!schematicAvailable()){if(!board->schematicAirwires().isEmpty())board->setSchematicAirwires({});return;}
    if(sidePanel->currentWidget()==schematicPage||action("schematicAirwires")->isChecked())compareWithSchematic(false);
}
NetCheck Editor::compareWithSchematic(bool reveal){
    if(!schematicAvailable()){board->setSchematicAirwires({});return {};}
    auto showPage=[this]{sidePanel->setTabVisible(sidePanel->indexOf(schematicPage),true);sidePanel->setCurrentWidget(schematicPage);};
    // A schematic that cannot be read: the message only when asked for, else the line alone, as this runs after changes.
    documents::Targets t;
    try{t=schematicForBoard();}catch(const std::exception &e){
        const QString text=ui("Der Schaltplan kann nicht gelesen werden: %1").arg(QString::fromUtf8(e.what()));
        board->setSchematicAirwires({});{const QSignalBlocker quiet(schematicList);schematicList->clear();schematicList->addItem(text);}
        if(reveal){showPage();QMessageBox::warning(this,ui("Mit Schaltplan vergleichen"),text);}
        return {};
    }
    const auto &b=std::as_const(doc).board();const auto c=checkNets(b,t);const auto parts=components(b);
    auto pin=[&](const documents::TargetPin &p){const auto *component=t.component(p.component);return (component?component->designator:QStringLiteral("?"))+'.'+p.pin;};
    auto net=[&](int n){const auto &x=t.nets[n];return x.name.isEmpty()?ui("Netz an %1").arg(x.pins.isEmpty()?QStringLiteral("?"):pin(x.pins.first())):x.name;};
    auto name=[&](int designator){const QString d=b.elements[designator].text.trimmed();return d.isEmpty()?ui("Bauteil ohne Bezeichner"):d;};
    auto members=[&](int designator){for(const auto &x:parts)if(x.designator==designator)return x.members;return QList<int>{designator};};
    const int previous=schematicList->currentRow();
    const int problems=int(c.missing.size()+c.ambiguous.size()+c.unassigned.size()+c.open.size()+c.joined.size());
    {const QSignalBlocker quiet(schematicList);schematicList->clear();
        // Each line keeps its elements and the place to show (the middle of its elements unless given).
        auto add=[&](const QString &text,const QList<int> &elements={},std::optional<QPointF> at={}){
            auto *item=new QListWidgetItem(text,schematicList);QVariantList list;for(int i:elements)list<<i;item->setData(Qt::UserRole+1,list);
            if(!at&&!elements.isEmpty()){QRectF r;for(int i:elements)r=r.united(bounds(b.elements[i]));at=r.center();}
            if(at)item->setData(Qt::UserRole,*at);
        };
        if(t.isEmpty())add(ui("Der Schaltplan enthält keine Bauteile."));
        else add(c.passed()?ui("Die Platine stimmt mit dem Schaltplan überein."):ui("%1 Abweichung(en) vom Schaltplan").arg(problems));
        for(int m:c.missing)add(ui("Fehlt auf der Platine: %1 (%2)").arg(t.components[m].designator,t.components[m].value));
        for(const auto &d:c.ambiguous){
            QList<int> found;for(const auto &x:parts)if(b.elements[x.designator].text.trimmed().compare(d.trimmed(),Qt::CaseInsensitive)==0)found+=x.members;
            add(ui("Bezeichner mehrfach auf der Platine: %1").arg(d),found);
        }
        for(const auto &p:c.unassigned)add(ui("Anschlüsse zuordnen: %1 (im Schaltplan %2)").arg(name(p.designator),t.components[p.target].pins.join(QStringLiteral(", "))),p.members);
        // An open net as its pieces: the pins joined on the board together, the pieces apart.
        for(const auto &o:c.open){
            QStringList pieces;QList<int> pads;
            for(const auto &piece:o.pieces){QStringList pins;for(const auto &x:piece){pins<<pin(x);pads+=c.padsFor(t,x.component,x.pin);}pieces<<pins.join(QStringLiteral(", "));}
            std::optional<QPointF> at;if(!o.pins.isEmpty()){const auto first=c.padsFor(t,o.pins.first().component,o.pins.first().pin);if(!first.isEmpty())at=b.elements[first.first()].pos;}
            const auto &called=t.nets[o.net].name;const QString list=pieces.join(QStringLiteral(" | "));
            add(called.isEmpty()?ui("Offen: %1").arg(list):ui("Offen in %1: %2").arg(called,list),pads,at);
        }
        for(const auto &j:c.joined)add(ui("Verbunden: %1 mit %2").arg(net(j.first),net(j.second)),j.elements,j.at);
        for(int d:c.extra)add(ui("Nicht im Schaltplan: %1").arg(name(d)),members(d));
        for(const auto &change:netChanges(b,c,t))if(change.text)for(const auto *list:{&c.parts,&c.unassigned})for(const auto &p:*list)if(p.designator==change.designator)
            add(ui("Im Schaltplan %1 (%2): %3 (%4) auf der Platine").arg(change.designatorText,change.value,b.elements[p.designator].text,p.value>=0?b.elements[p.value].text:QString()),p.members);
        schematicList->setCurrentRow(std::min(previous,schematicList->count()-1));
    }
    QList<std::pair<int,int>> lines;if(action("schematicAirwires")->isChecked())for(const auto &w:c.airwires)lines<<std::pair{w.from,w.to};
    board->setSchematicAirwires(lines);
    if(reveal)showPage();
    return c;
}
void Editor::takeOverFromSchematic(){
    if(!schematicAvailable())return;
    documents::Targets t;try{t=schematicForBoard();}catch(const std::exception &e){QMessageBox::warning(this,ui("Aus Schaltplan übernehmen"),QString::fromUtf8(e.what()));return;}
    const auto &b=std::as_const(doc).board();const auto check=checkNets(b,t);const auto changes=netChanges(b,check,t);
    auto name=[&](int designator){const QString d=b.elements[designator].text.trimmed();return d.isEmpty()?ui("Bauteil ohne Bezeichner"):d;};
    QStringList lines;
    for(const auto &change:changes)for(const auto *list:{&check.parts,&check.unassigned})for(const auto &p:*list)if(p.designator==change.designator){
        QStringList what;if(!change.component.isEmpty())what<<ui("gehört zu %1 im Schaltplan").arg(t.components[p.target].designator);
        if(change.text)what<<ui("Bezeichner und Wert %1 (%2)").arg(change.designatorText,change.value);
        if(change.swapPins)what<<ui("Anschlüsse getauscht");
        lines<<QStringLiteral("%1: %2").arg(name(p.designator),what.join(QStringLiteral(" · ")));
    }
    QStringList later;for(int m:check.missing)later<<ui("Fehlt auf der Platine: %1 (%2) – Extras → Fehlende Bauteile setzen…").arg(t.components[m].designator,t.components[m].value);
    for(const auto &p:check.unassigned)later<<ui("Anschlüsse zuordnen: %1 (im Schaltplan %2)").arg(name(p.designator),t.components[p.target].pins.join(QStringLiteral(", ")));
    QDialog dialog(this);dialog.setObjectName("takeOverDialog");dialog.setWindowTitle(ui("Aus Schaltplan übernehmen"));auto *layout=new QVBoxLayout(&dialog);
    auto *intro=new QLabel(changes.isEmpty()?ui("Auf der Platine ist nichts zu übernehmen."):ui("Diese Bauteile der Platine übernehmen Bezeichner, Wert und Zuordnung aus dem Schaltplan:"));
    intro->setWordWrap(true);layout->addWidget(intro);
    if(!lines.isEmpty()){auto *list=new QListWidget;list->setObjectName("takeOverChanges");list->addItems(lines);layout->addWidget(list);}
    if(!later.isEmpty()){auto *rest=new QLabel(later.join('\n'));rest->setObjectName("takeOverLater");rest->setWordWrap(true);layout->addWidget(rest);}
    auto *buttons=new QDialogButtonBox(changes.isEmpty()?QDialogButtonBox::Close:QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted||changes.isEmpty())return;
    Document probe=doc;if(!applyNetChanges(probe.board(),changes))return;
    snapshot();doc=probe;board->documentChanged();touched();
}
void Editor::assignPinsDialog(){
    if(!schematicAvailable())return;
    documents::Targets t;try{t=schematicForBoard();}catch(const std::exception &e){QMessageBox::warning(this,ui("Anschlüsse zuordnen"),QString::fromUtf8(e.what()));return;}
    const auto &b=std::as_const(doc).board();const auto check=checkNets(b,t);
    // The component selected, else the first one whose pins need assigning.
    const NetCheck::Part *chosen=nullptr;const auto sel=board->selection();
    if(!sel.isEmpty())for(const auto *list:{&check.parts,&check.unassigned})for(const auto &p:*list)if(!chosen&&p.members.contains(sel.first()))chosen=&p;
    if(!chosen&&!check.unassigned.isEmpty())chosen=&check.unassigned.first();
    if(!chosen){QMessageBox::information(this,ui("Anschlüsse zuordnen"),ui("Ein Bauteil markieren, das zu einem Bauteil des Schaltplans gehört."));return;}
    const auto part=*chosen;const auto &target=t.components[part.target];const auto pads=padsOf(b,part.designator);
    QDialog dialog(this);dialog.setObjectName("assignPinsDialog");dialog.setWindowTitle(ui("Anschlüsse zuordnen – %1").arg(b.elements[part.designator].text));
    auto *form=new QFormLayout(&dialog);QList<QComboBox*> boxes;QList<std::pair<int,QString>> labels;QList<int> standing;   // the pin each pad stands for now
    // Three through-hole pads of a transistor: the lead order of its type in a TO-92 (usualLeads), lead 1 at the pad
    // numbered 1, else at the first.
    bool leaded=pads.size()==3;for(int i:pads)leaded=leaded&&b.elements[i].type==ElementType::Pad;
    const auto usual=leaded?usualLeads(QStringLiteral("to-92"),target):QStringList();
    auto lead=[&](int k){bool numbered=false;const int n=b.elements[pads[k]].name.trimmed().toInt(&numbered);return numbered&&n>=1&&n<=3?n-1:k;};
    auto pinIndex=[&](const QString &pin){for(int n=0;n<target.pins.size();n++)if(target.pins[n].trimmed()==pin)return n;return -1;};
    for(int k=0;k<pads.size();k++){
        auto *box=new QComboBox;box->setObjectName(QStringLiteral("pin%1").arg(k+1));box->addItem(ui("kein Anschluss"));box->addItems(target.pins);
        // What the pad stands for now; a pad named for no pin of the schematic offers the pin at its place, a
        // transistor's the pin of its lead, or none when the type's order is not known.
        int current=-1;for(int n=0;n<part.pads.size();n++)if(part.pads[n].contains(pads[k]))current=n;standing<<current;
        if(current<0&&!b.elements[pads[k]].pin.trimmed().isEmpty()&&k<target.pins.size())
            current=!usual.isEmpty()?pinIndex(usual[lead(k)]):transistorPins(target.pins)?-1:k;
        box->setCurrentIndex(current+1);boxes<<box;
        const QString shown=b.elements[pads[k]].name.trimmed();const QString label=ui("Pad %1").arg(k+1);
        form->addRow(shown.isEmpty()?label:QStringLiteral("%1 (%2)").arg(label,shown),box);labels<<std::pair{pads[k],QString::number(k+1)};
    }
    if(!usual.isEmpty()){
        auto *take=new QPushButton(ui("Übernehmen"));take->setObjectName("usualLeads");
        form->addRow(new QLabel(ui("Übliche Anschlussfolge für %1 im TO-92: %2").arg(target.value.trimmed(),usual.join(u'-'))),take);
        connect(take,&QPushButton::clicked,&dialog,[&]{for(int k=0;k<boxes.size();k++)boxes[k]->setCurrentIndex(pinIndex(usual[lead(k)])+1);});
    }
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    board->setPadLabels(labels);const bool accepted=dialog.exec()==QDialog::Accepted;board->setPadLabels({});
    if(!accepted)return;
    // A pad left at the pin it stands for keeps its own name (A, K, +, -), so that the shared rules still read it.
    QStringList pins;
    for(int k=0;k<boxes.size();k++)pins<<(standing[k]>=0&&boxes[k]->currentIndex()==standing[k]+1?b.elements[pads[k]].pin:boxes[k]->currentIndex()>0?boxes[k]->currentText():QString());
    Document probe=doc;assignPins(probe.board(),part.designator,pins,b.elements[part.designator].component!=target.id?target.id:QString());
    if(probe==doc)return;
    snapshot();doc=probe;board->documentChanged();touched();
}
void Editor::placeMissingParts(){
    if(!schematicAvailable())return;
    documents::Targets t;try{t=schematicForBoard();}catch(const std::exception &e){QMessageBox::warning(this,ui("Fehlende Bauteile setzen"),QString::fromUtf8(e.what()));return;}
    const auto check=checkNets(doc.board(),t);
    // A component the document has on another board already is not placed twice.
    QSet<QString> placed;for(const auto &b:doc.boards)for(const auto &e:b.elements)if(e.role==TextRole::Designator&&!e.component.isEmpty())placed.insert(e.component);
    QList<int> missing;for(int m:check.missing)if(!placed.contains(t.components[m].id))missing<<m;
    if(missing.isEmpty()){QMessageBox::information(this,ui("Fehlende Bauteile setzen"),ui("Auf der Platine fehlt kein Bauteil des Schaltplans."));return;}
    QDialog dialog(this);dialog.setObjectName("placeMissingDialog");dialog.setWindowTitle(ui("Fehlende Bauteile setzen"));auto *layout=new QVBoxLayout(&dialog);
    auto *intro=new QLabel(ui("Diese Bauteile des Schaltplans fehlen auf der Platine. OpenLoch legt die gewählten Footprints neben die Platine, benannt und zugeordnet:"));
    intro->setWordWrap(true);layout->addWidget(intro);auto *form=new QFormLayout;layout->addLayout(form);
    QList<std::pair<int,QComboBox*>> rows;QList<QList<PartChoice>> offers;
    for(int m:missing){
        const auto &component=t.components[m];const auto choices=partChoices(component,doc.board().grid);offers<<choices;
        auto *box=new QComboBox;box->setObjectName("part-"+component.designator);box->addItem(ui("Nicht setzen"));for(const auto &c:choices)box->addItem(c.label);
        // Preselected only a sure choice: pads that fit do not make a resistor a diode, a transistor's leads follow its type.
        box->setCurrentIndex(!choices.isEmpty()&&choices.first().sure?1:0);
        form->addRow(QStringLiteral("%1 %2 (%3)").arg(component.designator,component.value,component.pins.join(QStringLiteral(", "))),box);rows<<std::pair{m,box};
    }
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    Board probe=doc.board(),scratch=probe;QList<QList<Element>> parts;
    for(int i=0;i<rows.size();i++){
        const int pick=rows[i].second->currentIndex()-1;if(pick<0||pick>=offers[i].size())continue;
        const auto els=missingPart(offers[i][pick],t.components[rows[i].first],scratch);if(els.isEmpty())continue;scratch.elements+=els;parts<<els;
    }
    if(parts.isEmpty())return;
    const auto added=layBeside(probe,parts);
    snapshot();doc.board()=probe;board->documentChanged();touched();board->setSelection(added);
    QRectF area(0,0,probe.width,probe.height);for(int i:added)area=area.united(bounds(probe.elements[i]));board->showArea(area);
    if(!action("schematicAirwires")->isChecked())action("schematicAirwires")->trigger();else compareWithSchematic(false);
}
void Editor::specialShape(){
    // The reference's "Spezialformen": a regular polygon, a spiral or a drawing frame, shown in a preview and placed
    // with the pointer.
    QDialog dialog(this);dialog.setWindowTitle(ui("Spezialform"));auto *outer=new QHBoxLayout(&dialog);auto *v=new QVBoxLayout;outer->addLayout(v);
    auto *kinds=new QTabWidget;kinds->setObjectName("shapeKinds");v->addWidget(kinds);const auto &b=doc.board();
    auto *polygon=new QWidget;auto *pf=new QFormLayout(polygon);auto *corners=new QSpinBox;corners->setObjectName("polygonCorners");corners->setRange(3,99);corners->setValue(6);
    auto *radius=spin(12,.1,1000,2);radius->setObjectName("polygonRadius");auto *polyLine=spin(.4,0,99.99,2);polyLine->setObjectName("polygonWidth");
    auto *offset=spin(0,-180,180,1,QStringLiteral("°"));offset->setObjectName("polygonOffset");
    auto *rays=new QCheckBox(ui("Strahlen von der Mitte zu den Ecken"));rays->setObjectName("polygonRays");auto *filled=new QCheckBox(ui("Gefüllt"));filled->setObjectName("polygonFilled");
    pf->addRow(ui("Radius"),radius);pf->addRow(ui("Breite"),polyLine);pf->addRow(ui("Ecken"),corners);pf->addRow(ui("Winkelversatz"),offset);pf->addRow(rays);pf->addRow(filled);
    kinds->addTab(polygon,ui("Vieleck"));
    auto *spiralPage=new QWidget;auto *sf=new QFormLayout(spiralPage);
    auto *start=spin(2,.1,249.9,2);start->setObjectName("spiralStart");auto *gap=spin(2,.1,99.9,2);gap->setObjectName("spiralGap");
    auto *spiralLine=spin(.4,0,99.99,2);spiralLine->setObjectName("spiralWidth");auto *turns=spin(6,1,100,2,QString());turns->setSingleStep(.25);turns->setObjectName("spiralTurns");
    auto *round=new QRadioButton(ui("rund"));round->setObjectName("spiralRound");round->setChecked(true);auto *square=new QRadioButton(ui("eckig"));square->setObjectName("spiralSquare");
    auto *diameter=new QLabel;diameter->setObjectName("spiralDiameter");
    sf->addRow(ui("Startradius"),start);sf->addRow(ui("Lücke zwischen den Windungen"),gap);sf->addRow(ui("Breite"),spiralLine);sf->addRow(ui("Windungen"),turns);
    {auto *h=new QHBoxLayout;h->addWidget(round);h->addWidget(square);h->addStretch();sf->addRow(ui("Form"),h);}
    sf->addRow(ui("Durchmesser"),diameter);kinds->addTab(spiralPage,ui("Spirale"));
    auto *framePage=new QWidget;auto *ff=new QFormLayout(framePage);
    auto *columns=new QSpinBox;columns->setObjectName("frameColumns");columns->setRange(1,99);columns->setValue(8);
    auto *rows=new QSpinBox;rows->setObjectName("frameRows");rows->setRange(1,99);rows->setValue(8);
    auto *columnKind=new QComboBox;columnKind->setObjectName("frameColumnKind");columnKind->addItems({ui("A, B, C …"),ui("1, 2, 3 …")});
    auto *rowKind=new QComboBox;rowKind->setObjectName("frameRowKind");rowKind->addItems({ui("A, B, C …"),ui("1, 2, 3 …")});rowKind->setCurrentIndex(1);
    auto *columnSides=new QComboBox;columnSides->setObjectName("frameColumnSides");columnSides->addItems({ui("ohne Beschriftung"),ui("oben"),ui("unten"),ui("oben und unten")});columnSides->setCurrentIndex(3);
    auto *rowSides=new QComboBox;rowSides->setObjectName("frameRowSides");rowSides->addItems({ui("ohne Beschriftung"),ui("links"),ui("rechts"),ui("links und rechts")});rowSides->setCurrentIndex(3);
    auto *frameWidth=spin(std::clamp(b.width,10.0,2000.0),10,2000,2),*frameHeight=spin(std::clamp(b.height,10.0,2000.0),10,2000,2);frameWidth->setObjectName("frameWidth");frameHeight->setObjectName("frameHeight");
    auto *autosize=new QPushButton(ui("Wie die Platine"));
    auto pair=[](QWidget *x,QWidget *y){auto *h=new QHBoxLayout;h->addWidget(x);h->addWidget(y,1);return h;};
    ff->addRow(ui("Spalten"),pair(columns,columnKind));ff->addRow(QString(),columnSides);ff->addRow(ui("Zeilen"),pair(rows,rowKind));ff->addRow(QString(),rowSides);
    ff->addRow(ui("Breite"),frameWidth);ff->addRow(ui("Höhe"),frameHeight);ff->addRow(autosize);kinds->addTab(framePage,ui("Rahmen"));
    connect(autosize,&QPushButton::clicked,&dialog,[&]{frameWidth->setValue(b.width);frameHeight->setValue(b.height);});
    auto *preview=new ArrangePreview;preview->setObjectName("shapePreview");preview->place=[](QList<Element>&,int){};outer->addWidget(preview,1);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);v->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    const int layer=b.activeLayer;
    auto spiralShape=[&]{SpiralShape s;s.start=start->value();s.gap=gap->value();s.width=spiralLine->value();s.turns=turns->value();s.square=square->isChecked();return s;};
    auto frameShape=[&]{FrameShape f;f.width=frameWidth->value();f.height=frameHeight->value();f.columns=columns->value();f.rows=rows->value();
        f.columnSides=columnSides->currentIndex();f.rowSides=rowSides->currentIndex();f.columnLetters=columnKind->currentIndex()==0;f.rowLetters=rowKind->currentIndex()==0;return f;};
    auto make=[&]()->QList<Element>{
        switch(kinds->currentIndex()){
        case 0:return regularPolygon(corners->value(),radius->value(),polyLine->value(),filled->isChecked(),layer,offset->value(),rays->isChecked());
        case 1:return {spiral(spiralShape(),layer)};
        default:return frame(frameShape(),layer);
        }};
    auto update=[&]{diameter->setText(ui("%1 mm").arg(uiLocale().toString(spiralDiameter(spiralShape()),'f',3)));preview->original=make();preview->update();};
    for(auto *box:{corners,columns,rows})connect(box,&QSpinBox::valueChanged,&dialog,update);
    for(auto *box:{radius,polyLine,offset,start,gap,spiralLine,turns,frameWidth,frameHeight})connect(box,&QDoubleSpinBox::valueChanged,&dialog,update);
    for(auto *box:{rays,filled})connect(box,&QCheckBox::toggled,&dialog,update);
    connect(square,&QRadioButton::toggled,&dialog,update);
    for(auto *box:{columnKind,rowKind,columnSides,rowSides})connect(box,&QComboBox::currentIndexChanged,&dialog,update);
    connect(kinds,&QTabWidget::currentChanged,&dialog,update);update();
    if(dialog.exec()!=QDialog::Accepted)return;
    QList<Element> shape=make();for(auto &e:shape)e.clearance=board->clearance;
    if(kinds->currentIndex()==2){
        // The frame is one group. One of the working area's size goes straight around it; any other follows the pointer.
        const int g=nextGroup(doc.board());for(auto &e:shape)e.groups.append(g);
        if(std::abs(frameWidth->value()-b.width)<1e-9&&std::abs(frameHeight->value()-b.height)<1e-9){
            snapshot();QList<int> added;for(auto e:shape){pcb::move(e,{b.width/2,b.height/2});doc.board().elements.append(e);added<<int(doc.board().elements.size())-1;}
            board->setSelection(added);board->documentChanged();touched();return;}
    }
    board->setTool(BoardView::Tool::Select);board->beginPlacement(shape);board->setFocus();
}
void Editor::placeFootprint(int row){
    const auto all=footprints();if(row<0||row>=all.size())return;
    board->setTool(BoardView::Tool::Select);board->beginPlacement(placeable(all[row],doc.board()));board->setFocus();
}
void Editor::importMacro(){
    const auto file=QFileDialog::getOpenFileName(this,ui("Makro laden"),macroFolder.isEmpty()?startFolder(Layouts):macroFolder,ui("Sprint-Layout-Makros (*.lmk)"));if(file.isEmpty())return;
    try{
        QFile f(file);if(!f.open(QIODevice::ReadOnly))throw FormatError(f.errorString());if(f.size()>64*1024*1024)throw FormatError(ui("Datei ist zu groß"));
        QStringList found;auto els=sprint::readMacro(f.readAll(),&found);if(!found.isEmpty())QMessageBox::information(this,ui("Makro laden"),found.join('\n'));
        placeMacro(els,false);
    }catch(const std::exception &e){QMessageBox::warning(this,ui("Makro laden"),QString::fromUtf8(e.what()));}
}
QList<Element> Editor::macroElements(QList<Element> els,bool asComponent) const{
    if(els.isEmpty())return {};
    // As a component: a macro without a designator gets one and a value, on its silkscreen side at its top left corner;
    // the component dialog opens once it is placed, as in the reference.
    const auto isDesignator=[](const Element &e){return e.type==ElementType::Text&&e.role==TextRole::Designator;};
    if(asComponent&&std::none_of(els.begin(),els.end(),isDesignator)){
        QRectF r;bool bottom=false,top=false;for(const auto &e:els){r=r.united(bounds(e));bottom|=e.layer==SilkBottom;top|=e.layer==SilkTop;}
        Element id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text=QStringLiteral("?");id.layer=bottom&&!top?SilkBottom:SilkTop;id.size=1.5;
        id.mirrored=id.layer==SilkBottom;id.pos=QPointF(r.left(),r.top()-.5);updateStrokes(id);
        Element value=id;value.role=TextRole::Value;value.pos=QPointF(r.left(),r.bottom()+2);updateStrokes(value);for(auto *e:{&id,&value})e->part=0;
        els<<id<<value;
    }
    // A macro with a designator and at most one component becomes one component, as the reference places macros;
    // several components stay apart.
    els=macroComponent(els);
    QRectF r;for(const auto &e:els)r=r.united(bounds(e));const QPointF centre=board->snap(r.center());for(auto &e:els)pcb::move(e,-centre);
    return placeable(els,doc.board());
}
void Editor::macroPlaced(const QList<int> &added){
    const auto &els=doc.board().elements;
    for(int i:added)if(i>=0&&i<els.size()&&els[i].type==ElementType::Text&&els[i].role==TextRole::Designator){editComponent(i);break;}
}
void Editor::placeMacro(QList<Element> els,bool asComponent){
    els=macroElements(els,asComponent);if(els.isEmpty())return;
    board->setTool(BoardView::Tool::Select);
    board->beginPlacement(els,[this,asComponent](QList<int> added){if(asComponent)macroPlaced(added);});
    board->setFocus();
}
void Editor::saveMacro(){saveMacroAs({});}
bool Editor::saveMacroAs(const QString &folder,QString file){
    const auto sel=board->selection();if(sel.isEmpty())return false;
    if(file.isEmpty()){
        file=QFileDialog::getSaveFileName(this,ui("Auswahl als Makro speichern"),!folder.isEmpty()?folder:macroFolder.isEmpty()?startFolder(Layouts):macroFolder,ui("Sprint-Layout-Makros (*.lmk)"));
        if(file.isEmpty())return false;if(QFileInfo(file).suffix().isEmpty())file+=".lmk";
    }
    auto els=extract(doc.board(),sel);QRectF r;for(const auto &e:els)r=r.united(bounds(e));const QPointF centre=board->snap(r.center());
    for(auto &e:els){pcb::move(e,-centre);e.groups.clear();}
    QSaveFile out(file);const auto bytes=sprint::writeMacro(els);
    if(!out.open(QIODevice::WriteOnly)||out.write(bytes)!=bytes.size()||!out.commit()){QMessageBox::warning(this,ui("Auswahl als Makro speichern"),out.errorString());return false;}
    return true;
}

// --- files
bool Editor::openFile(const QString &file,QString *error){
    try{
        QStringList found;auto d=load(file,&found);const bool own=sprint::fileVersion([&]{QFile f(file);return f.open(QIODevice::ReadOnly)?f.read(8):QByteArray();}())<0;
        setDocument(d,own?file:QString());if(!own){importedFile=QFileInfo(file).absoluteFilePath();if(titleChanged)titleChanged();}
        notes=found;noticeText->setText(found.join('\n'));noticeBar->setVisible(!found.isEmpty());
        board->documentFolder=QFileInfo(file).absolutePath();board->update();
        return true;
    }catch(const std::exception &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
bool Editor::saveFile(const QString &file,QString *error){
    try{save(doc,file);path=file;importedFile.clear();modified=false;backupRevision=revision;board->documentFolder=QFileInfo(file).absolutePath();if(titleChanged)titleChanged();return true;}
    catch(const std::exception &e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
bool Editor::exportSprint(const QString &file,QString *error,int version,int *skipped){
    QSaveFile out(file);const auto bytes=sprint::writeLayout(doc,version,skipped);
    if(!out.open(QIODevice::WriteOnly)||out.write(bytes)!=bytes.size()||!out.commit()){if(error)*error=out.errorString();return false;}
    return true;
}
void Editor::markSaved(){modified=false;if(titleChanged)titleChanged();}
bool Editor::setComponent(const QString &id,const QString &designator,const QString &value){
    Document probe=doc;bool found=false;for(auto &b:probe.boards)found=setComponentText(b,id,designator,value)||found;
    if(!found)return false;if(probe==doc)return true;
    snapshot();doc=probe;board->documentChanged();touched();return true;
}
bool Editor::placeComponent(const Footprint &footprint,const QString &id,const QString &designator,const QString &value){
    if(id.isEmpty())return false;
    for(const auto &b:doc.boards)for(const auto &e:b.elements)if(e.role==TextRole::Designator&&e.component==id)return false;
    Board parts;parts.elements=placeable(footprint,doc.board());for(auto &e:parts.elements)if(e.role==TextRole::Designator)e.component=id;
    if(!setComponentText(parts,id,designator,value))return false;
    board->setTool(BoardView::Tool::Select);board->beginPlacement(parts.elements);board->setFocus();return true;
}
QString Editor::displayName() const{
    if(!path.isEmpty())return QFileInfo(path).fileName();if(!importedFile.isEmpty())return QFileInfo(importedFile).fileName();return ui("Neue Leiterplatte");
}
bool Editor::maybeSave(){
    if(!modified)return true;
    const auto answer=QMessageBox::question(this,ui("Leiterplatte"),ui("Die Änderungen an „%1“ speichern?").arg(displayName()),QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);
    if(answer==QMessageBox::Cancel)return false;if(answer==QMessageBox::Discard)return true;
    if(saveHandler)return saveHandler(false);
    action("save")->trigger();return !modified;
}

// --- dialogs
bool Editor::editText(Element &text,bool isNew,QString *prefix,int *start){
    QDialog dialog(this);dialog.setWindowTitle(isNew?ui("Neuer Text"):ui("Text bearbeiten"));auto *form=new QFormLayout(&dialog);
    auto *line=new QLineEdit(text.text);line->setObjectName("text");line->setMaxLength(std::max<int>(99,int(text.text.size())));auto *height=spin(text.size,.1,200,2);height->setObjectName("height");
    auto *style=new QComboBox;style->addItems({ui("eng"),ui("normal"),ui("weit")});style->setCurrentIndex(text.style);
    auto *thickness=new QComboBox;thickness->addItems({ui("dünn"),ui("normal"),ui("dick")});thickness->setCurrentIndex(text.thickness);
    // The angle and the mirrors as in the properties.
    auto *rotation=spin(wrapped(text.rotation-(text.flipped?180:0)),-360,360,1,"°");auto *mirrored=new QCheckBox(ui("Gespiegelt"));mirrored->setChecked(text.mirrored!=text.flipped);
    auto *flipped=new QCheckBox(ui("Senkrecht gespiegelt"));flipped->setObjectName("textDialogFlipped");flipped->setChecked(text.flipped);
    form->addRow(ui("Text"),line);form->addRow(ui("Höhe"),height);form->addRow(ui("Stil"),style);form->addRow(ui("Strichstärke"),thickness);
    {auto *row=new QHBoxLayout;row->addWidget(rotation,1);
        for(int a:{0,90,180,270}){auto *quick=new QToolButton;quick->setText(QStringLiteral("%1°").arg(a));quick->setObjectName(QStringLiteral("textDialogAngle%1").arg(a));row->addWidget(quick);
            connect(quick,&QToolButton::clicked,&dialog,[rotation,a]{rotation->setValue(a);});}
        form->addRow(ui("Drehung"),row);}
    form->addRow(mirrored);form->addRow(flipped);
    // With the limit of the preferences no height below what keeps the strokes 0.15 mm wide.
    // A text keeps its height while that and its stroke stay as they were: one below the limit, as in the properties, and
    // one of more decimals than the field shows.
    const double was=text.size,shown=height->value();const int wasThickness=text.thickness;
    auto kept=[=](int k,double h){return !isNew&&k==wasThickness&&std::abs(h-shown)<1e-9;};
    auto lowest=[=,this](int k){height->setMinimum(kept(k,shown)?std::min(shown,minimumTextHeight(k)):minimumTextHeight(k));};
    lowest(text.thickness);connect(thickness,&QComboBox::currentIndexChanged,&dialog,lowest);
    // A series: the text is the beginning, a number follows it and counts up with each text put down.
    QCheckBox *numbered=nullptr;QSpinBox *first=nullptr;
    if(start){
        auto *group=new QGroupBox(ui("Fortlaufende Nummer"));auto *g=new QFormLayout(group);numbered=new QCheckBox(ui("Nummer anhängen und weiterzählen"));numbered->setObjectName("series");
        first=new QSpinBox;first->setRange(0,9999);first->setValue(0);first->setObjectName("seriesStart");g->addRow(numbered);g->addRow(ui("Erste Nummer"),first);form->addRow(group);
        *start=-1;
    }
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return false;
    const bool series=numbered&&numbered->isChecked();if(line->text().isEmpty()&&!series)return false;
    if(series){*start=first->value();if(prefix)*prefix=line->text();}
    const int k=thickness->currentIndex();text.text=line->text();text.size=kept(k,height->value())?was:std::max(height->value(),minimumTextHeight(k));text.style=style->currentIndex();text.thickness=thickness->currentIndex();
    text.flipped=flipped->isChecked();text.mirrored=mirrored->isChecked()!=text.flipped;text.rotation=wrapped(rotation->value()+(text.flipped?180:0));updateStrokes(text);
    if(isNew){board->textHeight=text.size;board->textStyle=text.style;board->textThickness=text.thickness;}
    return true;
}
void Editor::editBoardProperties(){
    const auto &shown=std::as_const(doc).board();QDialog dialog(this);dialog.setWindowTitle(ui("Platineneigenschaften"));auto *form=new QFormLayout(&dialog);
    auto *name=new QLineEdit(shown.name);name->setMaxLength(30);auto *width=spin(shown.width,1,2000,2),*height=spin(shown.height,1,2000,2),*grid=spin(shown.grid,.001,100,4);
    const double shownWidth=width->value(),shownHeight=height->value(),shownGrid=grid->value();     // as the fields show them
    auto *inner=new QCheckBox(ui("Mehrlagig (innere Kupferlagen I1 und I2)"));inner->setChecked(shown.multilayer);
    form->addRow(ui("Name"),name);form->addRow(ui("Breite"),width);form->addRow(ui("Höhe"),height);form->addRow(ui("Raster"),grid);form->addRow(inner);
    form->addRow(new QLabel(ui("Elemente bleiben in gleichem Abstand zur oberen Kante.")));
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    // Fields left as they were keep the exact values; nothing changed, no undo step.
    const double w=width->value()==shownWidth?shown.width:width->value(),h=height->value()==shownHeight?shown.height:height->value(),g=grid->value()==shownGrid?shown.grid:grid->value();
    if(name->text()==shown.name&&w==shown.width&&h==shown.height&&g==shown.grid&&inner->isChecked()==shown.multilayer)return;
    snapshot();auto &b=doc.board();b.name=name->text();b.width=w;b.height=h;b.grid=g;b.multilayer=inner->isChecked();
    if(!b.multilayer&&(b.activeLayer==Inner1||b.activeLayer==Inner2))b.activeLayer=CopperBottom;
    board->documentChanged();touched();refresh();
}
void Editor::editProjectInfo(){
    QDialog dialog(this);dialog.setWindowTitle(ui("Projekt-Info"));auto *form=new QFormLayout(&dialog);
    auto *title=new QLineEdit(doc.title),*author=new QLineEdit(doc.author),*company=new QLineEdit(doc.company);auto *comment=new QPlainTextEdit(doc.comment);
    for(auto *e:{title,author,company})e->setMaxLength(100);
    // The comment holds at most 1900 characters, as in the reference.
    comment->setObjectName("projectComment");
    connect(comment,&QPlainTextEdit::textChanged,&dialog,[comment]{if(comment->toPlainText().size()>1900){const QSignalBlocker quiet(comment);comment->setPlainText(comment->toPlainText().left(1900));comment->moveCursor(QTextCursor::End);}});
    form->addRow(ui("Titel"),title);form->addRow(ui("Autor"),author);form->addRow(ui("Firma"),company);form->addRow(ui("Kommentar"),comment);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(title->text()==doc.title&&author->text()==doc.author&&company->text()==doc.company&&comment->toPlainText()==doc.comment)return;
    snapshot();doc.title=title->text();doc.author=author->text();doc.company=company->text();doc.comment=comment->toPlainText();touched();
}
}
