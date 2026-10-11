// "Alle Luftlinien routen": the order of the nets, two nets that cross in a strip joined with a via (one layer leaves
// one open), a pad behind a ring of copper reported open, the design rules kept, the airwires removed; in the editor
// one undo step and the report.
#include "language.h"
#include "modules/pcb/copper.h"
#include "modules/pcb/editor.h"
#include "modules/pcb/router.h"
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QSet>
#include <QTimer>
#include <functional>
#include <stdexcept>

using namespace openloch;
using namespace openloch::pcb;
namespace {
void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
void inDialog(const std::function<void(QDialog*)> &fill){
    auto *timer=new QTimer;timer->setInterval(5);
    QObject::connect(timer,&QTimer::timeout,[timer,fill]{
        if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){timer->stop();timer->deleteLater();fill(dialog);dialog->accept();}});
    timer->start();
}
int pad(Board &b,QPointF at){auto e=newElement(ElementType::Pad);e.pos=at;e.size=1.6;e.size2=.8;e.layer=CopperBottom;updateOutline(e);b.elements<<e;return int(b.elements.size())-1;}
void airwire(Board &b,int one,int two){b.elements[one].connections<<two;b.elements[two].connections<<one;}
bool joined(const Board &b,int one,int two){const auto c=connections(b,false);return c[one]>=0&&c[one]==c[two];}
// A strip 30 × 10 mm: A from the top left to the bottom right, B from the bottom left to the top right. The pads lie so
// near the edges that no track passes between them and the edge, so on one layer the two must cross.
Board strip(int &a1,int &a2,int &b1,int &b2){
    Board b=newBoard("Streifen",30,10);a1=pad(b,{5,2});a2=pad(b,{25,8});b1=pad(b,{5,8});b2=pad(b,{25,2});airwire(b,a1,a2);airwire(b,b1,b2);return b;
}
}

int routerTests(){
    // --- The order: ground, then supplies, then the rest, each shortest first.
    {Board b=newBoard("Reihenfolge",100,50);QList<RouteNet> nets;
        auto net=[&](const QString &name,QPointF from,QPointF to){nets<<RouteNet{name,{pad(b,from),pad(b,to)}};};
        net("SIG",{10,10},{12,10});net("VCC",{10,20},{60,20});net("GND",{10,30},{90,30});net("+5V",{10,40},{20,40});net("",{50,5},{51,5});net("Masse",{30,45},{80,45});
        require(routingOrder(b,nets)==QList<int>({5,2,3,1,4,0}),"ground first (shorter first), then supplies, then the rest");}
    // --- Two crossing nets: one via; on one layer one stays open and is reported.
    {int a1,a2,b1,b2;const Board start=strip(a1,a2,b1,b2);const auto nets=routingNets(start,nullptr);
        require(nets.size()==2,"two nets from the airwires");
        RouteSettings settings;settings.step=.2;
        Board both=start;const auto r=routeAll(both,nets,settings);
        require(r.connections==2&&r.routed==2&&r.open.isEmpty()&&r.vias>=1&&joined(both,a1,a2)&&joined(both,b1,b2),"both joined, with a via");
        bool marked=true,top=false;for(int i:r.added){const auto &e=both.elements[i];
            if(e.type==ElementType::Track){marked=marked&&e.autorouted&&e.width==.8;top=top||e.layer==CopperTop;}else marked=marked&&e.via&&e.size==1.6&&e.size2==.8;}
        require(marked&&top,"autorouted tracks on both layers, vias as through-plated pads");
        require(both.elements[a1].connections.isEmpty()&&both.elements[b2].connections.isEmpty(),"the airwires the copper joins now are gone");
        require(checkDesign(both,Rules{}).isEmpty(),"the design rules kept");
        Board again=start;routeAll(again,nets,settings);require(again==both,"the same routes twice");
        settings.twoLayers=false;Board one=start;const auto single=routeAll(one,nets,settings);
        require(single.routed==1&&single.open.size()==1&&single.vias==0,"on one layer one stays open");
        const auto o=single.open.first();const QSet<int> ends{o.from,o.to};
        require(ends==QSet<int>({a1,a2})||ends==QSet<int>({b1,b2}),"the open join named by its pads");
        require(!one.elements[o.from].connections.isEmpty(),"its airwire stays");}
    // --- A pad behind a ring of copper on both layers stays open.
    {Board b=newBoard("Ring",30,10);const int p1=pad(b,{8,5}),p2=pad(b,{24,5});airwire(b,p1,p2);
        for(int layer:{CopperBottom,CopperTop}){auto ring=newElement(ElementType::Track);ring.layer=layer;ring.width=.5;ring.points={{20,2},{28,2},{28,8},{20,8},{20,2}};b.elements<<ring;}
        RouteSettings settings;settings.step=.2;const auto r=routeAll(b,routingNets(b,nullptr),settings);
        require(r.connections==1&&r.routed==0&&r.open.size()==1&&QSet<int>({r.open[0].from,r.open[0].to})==QSet<int>({p1,p2}),"enclosed: reported open");}
    // --- The editor: one undo step, the report.
    {Editor editor;editor.resize(1000,700);editor.show();QApplication::processEvents();
        int a1,a2,b1,b2;Document d;d.boards={strip(a1,a2,b1,b2)};editor.setDocument(d);QApplication::processEvents();editor.view()->autorouteOnGrid=false;
        bool listed=false;for(const auto &[title,names]:editorMenus())if(title=="&Extras")listed=names.contains("routeAll");require(listed,"in the Extras menu");
        const Board before=editor.document().board();QString summary;
        inDialog([&](QDialog *x){if(auto *l=x->findChild<QLabel*>("routeSummary"))summary=l->text();});
        editor.routeAllAirwires();const Board after=editor.document().board();int vias=0;for(const auto &e:after.elements)vias+=e.via;
        require(summary==ui("%1 von %2 Verbindungen verlegt, %3 Durchkontaktierungen.").arg(2).arg(2).arg(vias)&&vias>=1,"the report");
        require(joined(after,a1,a2)&&joined(after,b1,b2)&&editor.canUndo(),"routed");
        editor.undo();require(editor.document().board()==before,"one undo step");
        // One layer only: the open join listed by its pads.
        Board blocked=before;for(int layer:{CopperTop}){auto wall=newElement(ElementType::Area);wall.layer=layer;wall.points={{0,0},{30,0},{30,10},{0,10}};blocked.elements<<wall;}
        Document e;e.boards={blocked};editor.setDocument(e);QApplication::processEvents();editor.view()->autorouteOnGrid=false;QStringList open;
        inDialog([&](QDialog *x){if(auto *l=x->findChild<QListWidget*>("routeOpen"))for(int k=0;k<l->count();k++)open<<l->item(k)->text();});
        editor.routeAllAirwires();
        require(open.size()==1&&open[0].contains(QStringLiteral(" – ")),"what stayed open, listed");}
    return 0;
}
