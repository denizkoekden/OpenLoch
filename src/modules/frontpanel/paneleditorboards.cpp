// The front panel editor and the circuit boards behind its panels (docs/modules/frontpanel.md, "Bauteile und Platinen
// dahinter"): placing boards, holes for their parts, the comparison and the underlay in the view.
#include "paneleditor.h"
#include "panelboards.h"
#include "panelgeometry.h"
#include "panelview.h"
#include "language.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

namespace openloch::frontpanel {
namespace {
// A guess for parts that usually go through a front panel, with a usual hole, by designator, value and name: LEDs (3 or
// 5 mm by their size), potentiometers, switches and jacks (BNC sockets larger).
std::optional<double> usualHole(const BoardPart &part){
    static const QRegularExpression letters(QStringLiteral("^([A-Za-z]+)"));
    const QString kind=letters.match(part.designator.trimmed()).captured(1).toUpper(),value=part.value.toUpper(),name=part.name.toUpper();
    auto named=[&](std::initializer_list<const char*> words){for(const char *w:words)if(name.contains(QLatin1String(w))||value.contains(QLatin1String(w)))return true;return false;};
    if(kind==u"LED"||named({"LED"})){const double size=std::min(part.bounds.width(),part.bounds.height());return size>0&&size<4.5?3.0:5.0;}
    if(QStringList{"P","VR","RV","POT"}.contains(kind)||named({"POTI","POTENTIOMETER"})||(kind==u"R"&&(value.contains(u"LIN")||value.contains(u"LOG"))))return 7.0;
    if(named({"BNC"}))return 9.5;
    if(QStringList{"S","SW","TA","J","BU","X"}.contains(kind)||named({"SCHALTER","TASTER","SWITCH","BUCHSE","KLINKE","JACK"}))return 6.0;
    return std::nullopt;
}
}

void PanelEditor::reloadBoards(){boardCache=boardSources?boardSources():QList<BoardSource>{};refreshUnderlay();}
QList<PanelEditor::BoardBehindNow> PanelEditor::boardsBehind() const{
    QList<BoardBehindNow> out;
    for(const auto &b:doc.panel().boards){
        BoardBehindNow now{b,std::nullopt};
        for(const auto &s:boardCache)if(s.document==b.document&&s.board==b.board)now.source=s;
        out<<now;
    }
    return out;
}
void PanelEditor::setBoardsBehind(const QList<BoardBehind> &boards,bool moveHoles){
    const QList<BoardBehind> before=doc.panel().boards;
    change([&](Document &d){
        Panel &panel=d.panel();
        if(moveHoles)for(const auto &old:before)for(const auto &now:boards){
            if(old.document!=now.document||old.board!=now.board||old==now)continue;
            const BoardSource *source=nullptr;for(const auto &s:boardCache)if(s.document==now.document&&s.board==now.board)source=&s;
            if(!source)continue;
            QSet<QString> components;for(const auto &part:source->parts)components.insert(part.component);
            // From the panel through the board's old place to its new one; an element moves with its point.
            const QTransform follow=old.toPanel().inverted()*now.toPanel();
            for(auto &e:panel.elements)if(components.contains(e.component)){
                const QPointF at=componentPoint(e),to=follow.map(at);transformElement(e,QTransform::fromTranslate(to.x()-at.x(),to.y()-at.y()));
            }
        }
        panel.boards=boards;
    });
}
void PanelEditor::addHoles(const QList<HoleFor> &holes){
    if(holes.isEmpty())return;
    QStringList ids;
    change([&](Document &d){
        for(const auto &h:holes){
            Element e=styled(ElementType::Drill);e.center=h.at;e.diameter=h.diameter;e.component=h.component;e.name=h.designator;
            d.panel().elements<<e;ids<<e.id;
        }
    });
    area->setSelection(ids);
}
QList<PanelEditor::BoardFinding> PanelEditor::compareWithBoards() const{
    QHash<QString,QPointF> parts;   // the middle of each component's part on the panel
    for(const auto &b:boardsBehind())if(b.source)for(const auto &part:b.source->parts)parts.insert(part.component,b.placement.toPanel().map(part.centre));
    QList<BoardFinding> out;
    for(const auto &e:doc.panel().elements){
        if(e.component.isEmpty())continue;
        const QString name=e.name.isEmpty()?typeTitle(e.type):e.name;
        const auto it=parts.constFind(e.component);
        if(it==parts.cend()){out<<BoardFinding{e.id,ui("%1: kein Bauteil auf den Platinen dahinter").arg(name),std::nullopt};continue;}
        const QPointF own=componentPoint(e);const double distance=QLineF(*it,own).length();
        const bool over=e.type==ElementType::Drill?distance<=e.diameter/2:elementBounds(e).contains(*it);
        if(!over)out<<BoardFinding{e.id,ui("%1 liegt %2 mm neben dem Bauteil").arg(name,uiLocale().toString(distance,'f',1)),*it};
    }
    return out;
}
void PanelEditor::moveOntoParts(){
    const auto findings=compareWithBoards();QStringList moved;
    change([&](Document &d){
        for(auto &e:d.panel().elements)for(const auto &f:findings)if(f.element==e.id&&f.part){
            const QPointF delta=*f.part-componentPoint(e);transformElement(e,QTransform::fromTranslate(delta.x(),delta.y()));moved<<e.id;
        }
    });
    if(!moved.isEmpty())area->setSelection(moved);
}
void PanelEditor::refreshUnderlay(){
    QList<PanelView::Underlay> boards;
    if(QAction *shown=action("showBoards");shown&&shown->isChecked())for(const auto &b:boardsBehind()){
        if(!b.source)continue;
        const QTransform t=b.placement.toPanel();PanelView::Underlay u;u.name=b.source->name;u.outline=t.map(QPolygonF(QRectF(QPointF(0,0),b.source->size)));
        for(const auto &part:b.source->parts)u.parts<<PanelView::Underlay::Part{t.map(QPolygonF(part.bounds)),t.map(part.centre),part.designator,part.top!=b.placement.solderSide};
        boards<<u;
    }
    area->setUnderlay(boards);
}
void PanelEditor::changeEvent(QEvent *event){
    // Back in this window: the boards may have changed in another one.
    if(event->type()==QEvent::ActivationChange&&isActiveWindow()&&boardSources&&!doc.panel().boards.isEmpty())reloadBoards();
    QMainWindow::changeEvent(event);
}

// "Platinen dahinter…": every board of the project, those behind the panel ticked, with their place.
void PanelEditor::editBoardsBehind(){
    reloadBoards();
    const Panel &panel=doc.panel();
    struct Row {BoardBehind placement;QString name;QSizeF size;bool behind=false,known=true;};
    QList<Row> rows;
    for(const auto &s:boardCache){
        // A board not yet behind the panel starts in its middle.
        Row r{BoardBehind{s.document,s.board,QPointF((panel.width-s.size.width())/2,(panel.height-s.size.height())/2),0,false},s.name,s.size,false,true};
        for(const auto &b:panel.boards)if(b.document==s.document&&b.board==s.board){r.placement=b;r.behind=true;}
        rows<<r;
    }
    for(const auto &b:panel.boards){
        bool known=false;for(const auto &r:rows)known=known||(r.placement.document==b.document&&r.placement.board==b.board);
        if(!known)rows<<Row{b,ui("Platine fehlt im Projekt"),{},true,false};
    }
    if(rows.isEmpty()){
        QMessageBox::information(this,ui("Platinen dahinter"),ui("Das Projekt hat keine Lochraster- oder Leiterplatte. Fenster → Zum Projekt hinzufügen legt eine an."));return;
    }
    QDialog dialog(this);dialog.setObjectName("boardsBehindDialog");dialog.setWindowTitle(ui("Platinen hinter „%1“").arg(panel.name));
    auto *layout=new QVBoxLayout(&dialog);
    auto *intro=new QLabel(ui("Die gewählten Platinen sitzen hinter der Frontplatte, von vorn gesehen. X und Y nennen die Lage der linken oberen Ecke der Platine (von der Bestückungsseite gesehen) auf der Frontplatte; um sie dreht sich die Platine gegen den Uhrzeigersinn."),&dialog);
    intro->setWordWrap(true);layout->addWidget(intro);
    auto *table=new QTableWidget(int(rows.size()),5,&dialog);table->setObjectName("boards");
    table->setHorizontalHeaderLabels({ui("Platine"),ui("X (mm)"),ui("Y (mm)"),ui("Drehung"),ui("Zur Frontplatte zeigt")});table->verticalHeader()->hide();
    for(int i=0;i<rows.size();i++){
        const Row &r=rows[i];
        auto *item=new QTableWidgetItem(r.name);item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsUserCheckable|Qt::ItemIsSelectable);item->setCheckState(r.behind?Qt::Checked:Qt::Unchecked);table->setItem(i,0,item);
        auto spin=[](double value){auto *s=new QDoubleSpinBox;s->setRange(-10000,10000);s->setDecimals(2);s->setValue(value);return s;};
        table->setCellWidget(i,1,spin(r.placement.offset.x()));table->setCellWidget(i,2,spin(r.placement.offset.y()));
        auto *turn=new QDoubleSpinBox;turn->setRange(-360,360);turn->setSingleStep(90);turn->setDecimals(1);turn->setSuffix(QStringLiteral("°"));turn->setValue(r.placement.rotation);table->setCellWidget(i,3,turn);
        auto *side=new QComboBox;side->addItems({ui("Bestückungsseite"),ui("Lötseite")});side->setCurrentIndex(r.placement.solderSide?1:0);table->setCellWidget(i,4,side);
    }
    table->resizeColumnsToContents();table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);layout->addWidget(table);
    auto read=[&](int i){
        BoardBehind b=rows[i].placement;
        b.offset={qobject_cast<QDoubleSpinBox*>(table->cellWidget(i,1))->value(),qobject_cast<QDoubleSpinBox*>(table->cellWidget(i,2))->value()};
        b.rotation=qobject_cast<QDoubleSpinBox*>(table->cellWidget(i,3))->value();b.solderSide=qobject_cast<QComboBox*>(table->cellWidget(i,4))->currentIndex()==1;
        return b;
    };
    auto *centre=new QPushButton(ui("Markierte Platine mittig setzen"),&dialog);centre->setObjectName("centreBoard");layout->addWidget(centre,0,Qt::AlignLeft);
    connect(centre,&QPushButton::clicked,&dialog,[&]{
        const int i=table->currentRow();if(i<0||!rows[i].known)return;
        BoardBehind b=read(i);b.offset={};const QRectF area=b.toPanel().map(QPolygonF(QRectF(QPointF(0,0),rows[i].size))).boundingRect();
        const QPointF offset=QPointF(panel.width/2,panel.height/2)-area.center();
        qobject_cast<QDoubleSpinBox*>(table->cellWidget(i,1))->setValue(offset.x());qobject_cast<QDoubleSpinBox*>(table->cellWidget(i,2))->setValue(offset.y());
        table->item(i,0)->setCheckState(Qt::Checked);
    });
    auto *move=new QCheckBox(ui("Bohrungen und Symbole der Bauteile mitnehmen"),&dialog);move->setObjectName("moveHoles");move->setChecked(true);layout->addWidget(move);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    dialog.resize(780,360);
    if(dialog.exec()!=QDialog::Accepted)return;
    QList<BoardBehind> chosen;for(int i=0;i<rows.size();i++)if(table->item(i,0)->checkState()==Qt::Checked)chosen<<read(i);
    setBoardsBehind(chosen,move->isChecked());
}
// "Bohrungen aus der Platine…": the parts of the boards behind, the usual ones ticked.
void PanelEditor::holesFromBoards(){
    reloadBoards();
    struct Offer {BoardPart part;QString board;QPointF at;bool facing=true;};
    QList<Offer> offers;
    for(const auto &b:boardsBehind())if(b.source)for(const auto &part:b.source->parts)
        offers<<Offer{part,b.source->name,b.placement.toPanel().map(part.centre),part.top!=b.placement.solderSide};
    if(offers.isEmpty()){
        QMessageBox::information(this,ui("Bohrungen aus der Platine"),ui("Hinter dieser Frontplatte sitzt keine Platine mit Bauteilen. Frontplatte → Platinen dahinter… setzt eine."));return;
    }
    QSet<QString> present;for(const auto &c:panelComponents(doc.panel()))present.insert(c.component);
    QDialog dialog(this);dialog.setObjectName("holesFromBoardsDialog");dialog.setWindowTitle(ui("Bohrungen aus der Platine"));
    auto *layout=new QVBoxLayout(&dialog);
    auto *intro=new QLabel(ui("Die gewählten Bauteile erhalten eine Bohrung über ihrer Mitte, benannt und dem Bauteil zugeordnet. Vorgewählt sind LEDs, Potentiometer, Schalter und Buchsen, die zur Frontplatte zeigen und noch keine haben."),&dialog);
    intro->setWordWrap(true);layout->addWidget(intro);
    auto *table=new QTableWidget(int(offers.size()),3,&dialog);table->setObjectName("parts");
    table->setHorizontalHeaderLabels({ui("Bauteil"),ui("Platine"),ui("Bohrung Ø (mm)")});table->verticalHeader()->hide();
    for(int i=0;i<offers.size();i++){
        const auto &o=offers[i];const auto usual=usualHole(o.part);const bool has=present.contains(o.part.component);
        QString text=o.part.designator.isEmpty()?ui("Bauteil ohne Kennung"):o.part.designator;
        if(!o.part.value.isEmpty())text+=QStringLiteral(" (%1)").arg(o.part.value);
        if(has)text+=ui(" – hat eine Bohrung");
        if(!o.facing)text+=ui(" – zeigt von der Frontplatte weg");
        auto *item=new QTableWidgetItem(text);item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsUserCheckable|Qt::ItemIsSelectable);
        item->setCheckState(usual&&o.facing&&!has?Qt::Checked:Qt::Unchecked);table->setItem(i,0,item);
        auto *board=new QTableWidgetItem(o.board);board->setFlags(Qt::ItemIsEnabled);table->setItem(i,1,board);
        auto *size=new QDoubleSpinBox;size->setRange(0.1,100);size->setDecimals(2);size->setValue(usual.value_or(3));table->setCellWidget(i,2,size);
    }
    table->resizeColumnsToContents();table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);layout->addWidget(table);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    dialog.resize(640,420);
    if(dialog.exec()!=QDialog::Accepted)return;
    QList<HoleFor> holes;
    for(int i=0;i<offers.size();i++)if(table->item(i,0)->checkState()==Qt::Checked)
        holes<<HoleFor{offers[i].part.component,offers[i].part.designator,offers[i].at,qobject_cast<QDoubleSpinBox*>(table->cellWidget(i,2))->value()};
    addHoles(holes);
}
// "Mit Platine vergleichen": the findings, a double click selects the element, "An die Bauteile setzen" moves them.
void PanelEditor::showBoardComparison(){
    reloadBoards();
    const auto findings=compareWithBoards();
    QDialog dialog(this);dialog.setObjectName("boardComparisonDialog");dialog.setWindowTitle(ui("Mit Platine vergleichen"));
    auto *layout=new QVBoxLayout(&dialog);
    const QString summary=doc.panel().boards.isEmpty()?ui("Hinter dieser Frontplatte sitzt keine Platine.")
        :findings.isEmpty()?ui("Alle Bohrungen und Symbole von Bauteilen liegen über ihren Bauteilen.")
        :ui("%1 Abweichung(en) von den Platinen dahinter:").arg(findings.size());
    auto *label=new QLabel(summary,&dialog);label->setWordWrap(true);layout->addWidget(label);
    auto *list=new QListWidget(&dialog);list->setObjectName("findings");
    for(const auto &f:findings){auto *item=new QListWidgetItem(f.text,list);item->setData(Qt::UserRole,f.element);}
    if(!findings.isEmpty())layout->addWidget(list);else list->hide();
    connect(list,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *item){area->setSelection({item->data(Qt::UserRole).toString()});});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(buttons);
    bool movable=false;for(const auto &f:findings)movable=movable||f.part.has_value();
    QPushButton *onto=nullptr;if(movable){onto=buttons->addButton(ui("An die Bauteile setzen"),QDialogButtonBox::AcceptRole);onto->setObjectName("moveOntoParts");}
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted&&onto)moveOntoParts();
}
}
