// Automatic placement ("Bauteile automatisch anordnen"): the region from the outline, the nets from the airwires, the
// arrangement inside the region without overlaps and with shorter airwires than beside the board, the same result
// twice, components without room or already wired left where they are; in the editor the dialog, one undo step and the
// report, and the message when nothing lies outside the board.
#include "language.h"
#include "modules/pcb/editor.h"
#include "modules/pcb/footprints.h"
#include "modules/pcb/netcheck.h"
#include "modules/pcb/placement.h"
#include <QApplication>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineF>
#include <QTimer>
#include <cmath>
#include <functional>
#include <stdexcept>

using namespace openloch;
using namespace openloch::pcb;
namespace {
void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
Footprint footprint(const QString &id){for(const auto &f:footprints())if(f.id==id)return f;throw std::runtime_error("no such footprint");}
// Fills in the next modal dialog with `fill` and accepts it (never throws inside: what it finds is checked afterwards).
void inDialog(const std::function<void(QDialog*)> &fill){
    auto *timer=new QTimer;timer->setInterval(5);
    QObject::connect(timer,&QTimer::timeout,[timer,fill]{
        if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){timer->stop();timer->deleteLater();fill(dialog);dialog->accept();}});
    timer->start();
}
int designator(const Board &b,const QString &text){for(const auto &c:components(b))if(b.elements[c.designator].text==text)return c.designator;throw std::runtime_error("no such component");}
int pad(const Board &b,const QString &component,const QString &name){
    for(int i:padsOf(b,designator(b,component)))if(b.elements[i].name==name)return i;
    throw std::runtime_error("no such pad");
}
// The pads of a net joined by airwires in a chain: "IC1.8" and so on.
void net(Board &b,const QStringList &pins){
    QList<int> pads;for(const auto &p:pins)pads<<pad(b,p.section('.',0,0),p.section('.',1));
    for(int k=1;k<pads.size();k++)b.elements[pads[k-1]].connections<<pads[k];
}
// A board with an outline of 60 × 40 mm (from 10, 10 to 70, 50) and the parts of a small circuit laid beside it.
Board sample(){
    NewBoard shape;shape.kind=NewBoard::Rectangle;shape.width=60;shape.height=40;shape.margin=10;Board b=newBoard("Anordnen",shape);b.grid=1.27;
    Board scratch=b;QList<QList<Element>> parts;
    for(const char *id:{"dil-8","res-0207-10","res-0207-10","res-0207-10","res-0207-10","cap-254","cap-254","to-92"}){
        auto els=placeable(footprint(QString::fromLatin1(id)),scratch);scratch.elements+=els;parts<<els;}
    layBeside(b,parts);
    net(b,{"IC1.8","IC1.4","R1.1","R2.1","C1.1","R4.2"});net(b,{"IC1.1","C1.2","C2.2","T1.3"});net(b,{"IC1.2","IC1.6","C2.1","R2.2"});
    net(b,{"IC1.7","R1.2","R2.2"});net(b,{"IC1.3","R3.1"});net(b,{"R3.2","T1.2"});net(b,{"T1.1","R4.1"});
    return b;
}
QRectF box(const Board &b,const Component &c){QRectF r;for(int m:c.members)r=r.united(bounds(b.elements[m]));return r;}
QList<int> designators(const Board &b){QList<int> out;for(const auto &c:components(b))out<<c.designator;return out;}
}

int placementTests(){
    // --- The region: lines meeting end to end make a closed outline, a round one a circle; without one the working area.
    {Board b=newBoard("L",80,60);
        auto first=newElement(ElementType::Track);first.layer=Outline;first.points={{10,10},{70,10},{70,30}};
        auto rest=newElement(ElementType::Track);rest.layer=Outline;rest.points={{10,10},{10,50},{35,50},{35,30},{70,30}};
        b.elements<<first<<rest;const auto l=arrangeRegion(b);
        require(l.contains(QPointF(20,40))&&l.contains(QPointF(60,20))&&!l.contains(QPointF(50,40)),"an L joined from two lines");
        NewBoard shape;shape.kind=NewBoard::Round;shape.diameter=50;shape.margin=5;const Board round=newBoard("Rund",shape);const auto r=arrangeRegion(round);
        require(r.contains(QPointF(30,30))&&!r.contains(QPointF(7,7)),"a round outline");
        require(arrangeRegion(newBoard("Fläche",40,30)).boundingRect()==QRectF(0,0,40,30),"no outline: the working area");}
    // --- The nets from the airwires; the parts arranged inside, apart, on the grid, shorter, the same twice.
    {const Board b=sample();const auto nets=arrangementNets(b,nullptr);
        require(nets.size()==6,"the airwires joined into six nets (R2.2 joins two chains)");
        Board first=b;const auto r=arrangeParts(first,designators(b),nets);
        require(r.placed.size()==8&&r.left.isEmpty()&&r.wired.isEmpty(),"all eight arranged");
        const auto region=arrangeRegion(first);const auto parts=components(first);bool inside=true,grid=true,apart=true;
        for(int i=0;i<parts.size();i++){
            const QRectF x=box(first,parts[i]);inside=inside&&region.contains(x.adjusted(-.9,-.9,.9,.9));
            const QPointF p=first.elements[padsOf(first,parts[i].designator).first()].pos-first.origin;
            grid=grid&&std::abs(p.x()/1.27-std::round(p.x()/1.27))<1e-6&&std::abs(p.y()/1.27-std::round(p.y()/1.27))<1e-6;
            for(int j=i+1;j<parts.size();j++)apart=apart&&!x.adjusted(-.49,-.49,.49,.49).intersects(box(first,parts[j]).adjusted(-.49,-.49,.49,.49));
        }
        require(inside,"inside the outline, a gap from its edge");require(apart,"a gap between any two");require(grid,"the first pad of each on the grid");
        require(r.lengthAfter<r.lengthBefore/2&&r.crossingsAfter<=r.crossingsBefore,"far shorter airwires than beside the board, no more crossings");
        Board second=b;const auto again=arrangeParts(second,designators(b),nets);
        require(second==first&&again.lengthAfter==r.lengthAfter,"the same result twice");
        // Without turning every element keeps its angle; turned or not, the pads of a part keep their distances.
        {Board flat=b;ArrangeSettings still;still.rotate=false;const auto kept=arrangeParts(flat,designators(b),nets,still);bool same=true,whole=true;
            for(int i=0;i<b.elements.size();i++)same=same&&flat.elements[i].rotation==b.elements[i].rotation;
            for(const auto &c:components(b)){const auto pads=padsOf(b,c.designator);for(int k=1;k<pads.size();k++)
                whole=whole&&std::abs(QLineF(first.elements[pads[k]].pos,first.elements[pads[0]].pos).length()-QLineF(b.elements[pads[k]].pos,b.elements[pads[0]].pos).length())<1e-9;}
            require(kept.placed.size()==8&&same&&whole,"without turning no angle changes; the parts stay whole");}
        // Only some: the others stay and are in the way.
        Board some=b;const int ic=designator(some,"IC1"),r1=designator(some,"R1");const auto partly=arrangeParts(some,{ic,r1},nets);
        require(partly.placed.size()==2&&some.elements[designator(some,"R2")]==b.elements[designator(b,"R2")],"only those asked for move");}
    // --- An L-shaped outline: nothing in the notch.
    {Board b=sample();QList<int> outline;for(int i=0;i<b.elements.size();i++)if(b.elements[i].layer==Outline)outline<<i;removeElements(b,outline);
        auto first=newElement(ElementType::Track);first.layer=Outline;first.points={{10,10},{70,10},{70,30}};
        auto rest=newElement(ElementType::Track);rest.layer=Outline;rest.points={{70,30},{35,30},{35,50},{10,50},{10,10}};
        b.elements<<first<<rest;const auto r=arrangeParts(b,designators(b),arrangementNets(b,nullptr));const auto region=arrangeRegion(b);
        bool inside=true;for(const auto &c:components(b))if(r.placed.contains(c.designator))inside=inside&&region.contains(box(b,c));
        require(r.placed.size()+r.left.size()==8&&r.placed.size()>=6&&inside,"inside the L, none in its notch");}
    // --- No room: the part stays where it was and is named; copper at a pad keeps a part where it is.
    {Board b=newBoard("Klein",12,12);Board scratch=b;auto els=placeable(footprint("dil-40"),scratch);layBeside(b,{els});const Board before=b;
        const auto r=arrangeParts(b,designators(b),{});
        require(r.placed.isEmpty()&&r.left==designators(before)&&b==before,"a DIL-40 has no room on 12 × 12 mm and stays");}
    {Board b=sample();const int p=pad(b,"R1","1");auto wire=newElement(ElementType::Track);wire.layer=CopperBottom;wire.width=.5;
        wire.points={b.elements[p].pos,b.elements[p].pos+QPointF(-3,0)};b.elements<<wire;const Element r1=b.elements[designator(b,"R1")];
        require(padWired(b,p)&&!padWired(b,pad(b,"R2","1")),"a track ending on a pad wires it");
        const auto r=arrangeParts(b,designators(b),arrangementNets(b,nullptr));
        require(r.wired==QList<int>{designator(b,"R1")}&&b.elements[designator(b,"R1")]==r1&&r.placed.size()==7,"a wired part stays where it is");}
    // --- The editor: the parts outside arranged after the dialog, one undo step, the report in the help line.
    {Editor editor;editor.resize(1000,700);editor.show();QApplication::processEvents();
        Document d;d.boards={sample()};editor.setDocument(d);QApplication::processEvents();const Board before=editor.document().board();
        double gap=0;QString told;inDialog([&](QDialog *x){if(auto *g=x->findChild<QDoubleSpinBox*>("arrangeGap"))gap=g->value();
            for(auto *l:x->findChildren<QLabel*>())if(l->text().contains(QStringLiteral("8")))told=l->text();});
        editor.arrangeComponents();const Board after=editor.document().board();
        const auto region=arrangeRegion(after);bool inside=true;for(const auto &c:components(after))inside=inside&&region.contains(componentCentre(after,c));
        require(gap==2&&told==ui("Die %1 Bauteile außerhalb der Platine werden auf ihr angeordnet, nach ihren Netzen gruppiert.").arg(8),"the dialog: gap and what moves");
        require(inside&&editor.canUndo()&&editor.arrangeReport().startsWith(ui("%1 Bauteile angeordnet: Luftlinien %2 mm statt %3 mm, %4 Kreuzungen statt %5.").arg(8).section(':',0,0)),
            "arranged, reported");
        require(int(editor.view()->selection().size())==[&]{int n=0;for(const auto &c:components(after))n+=int(c.members.size());return n;}(),"the arranged parts selected");
        editor.undo();require(editor.document().board()==before,"one undo step");
        editor.redo();editor.view()->setSelection({});QString nothing;
        inDialog([&](QDialog *x){for(auto *l:x->findChildren<QLabel*>())if(l->text().contains(ui("Kein Bauteil liegt außerhalb der Platine.").left(20)))nothing=l->text();});
        const Board placed=editor.document().board();editor.arrangeComponents();
        require(nothing==ui("Kein Bauteil liegt außerhalb der Platine. Die Bauteile markieren, die angeordnet werden sollen.")&&editor.document().board()==placed,
            "nothing outside, nothing selected: said so, nothing moves");}
    return 0;
}
