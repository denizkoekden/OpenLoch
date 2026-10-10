// The board against the schematic of its project (docs/suite.md, "Soll-Verbindungen aus dem Schaltplan"): matching of
// components and pins, open and joined nets, airwires, the lookup of pads by component and pin, the take-over.
#include "language.h"
#include "modules/pcb/editor.h"
#include "modules/pcb/font.h"
#include "modules/pcb/footprints.h"
#include "modules/pcb/netcheck.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineF>
#include <QListWidget>
#include <QMap>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <cmath>
#include <functional>
#include <stdexcept>

using namespace openloch;
using namespace openloch::pcb;
namespace {
void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
Footprint footprint(const QString &id){for(const auto &f:footprints())if(f.id==id)return f;throw std::runtime_error("no such footprint");}
// A footprint of the module's library put on the board at `at` as a component: its designator text, its value, its
// identifier (empty: none, so that only the designator finds it). Returns the index of its designator.
int put(Board &b,const QString &id,QPointF at,const QString &designator,const QString &value,const QString &component={}){
    auto els=placeable(footprint(id),b);int d=-1;
    for(auto &e:els){pcb::move(e,at);if(e.role==TextRole::Designator){e.text=designator;e.component=component;updateStrokes(e);}if(e.role==TextRole::Value){e.text=value;updateStrokes(e);}}
    for(int k=0;k<els.size();k++)if(els[k].role==TextRole::Designator)d=int(b.elements.size())+k;
    b.elements+=els;return d;
}
// The pad of a component named so.
int pad(const Board &b,int designator,const QString &name){for(int i:padsOf(b,designator))if(b.elements[i].name==name)return i;throw std::runtime_error("no such pad");}
int track(Board &b,QPointF from,QPointF to,int layer=CopperBottom){
    auto e=newElement(ElementType::Track);e.layer=layer;e.points={from,to};e.width=.5;b.elements<<e;return int(b.elements.size())-1;
}
int joinPads(Board &b,int p,int q,int layer=CopperBottom){return track(b,b.elements[p].pos,b.elements[q].pos,layer);}
void path(Board &b,const QPolygonF &points){auto e=newElement(ElementType::Track);e.layer=CopperBottom;e.points=points;e.width=.5;b.elements<<e;}
documents::TargetComponent part(const QString &id,const QString &designator,const QString &value,const QStringList &pins){return {id,designator,value,pins};}
documents::TargetNet net(const QString &name,const QList<documents::TargetPin> &pins){return {name,pins};}
// Fills in the next modal dialog with `fill` and accepts it (never throws inside: what it finds is checked afterwards).
void inDialog(const std::function<void(QDialog*)> &fill){
    auto *timer=new QTimer;timer->setInterval(5);
    QObject::connect(timer,&QTimer::timeout,[timer,fill]{
        if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){timer->stop();timer->deleteLater();fill(dialog);dialog->accept();}});
    timer->start();
}
QStringList lines(QListWidget *list){QStringList out;for(int k=0;k<list->count();k++)out<<list->item(k)->text();return out;}
}

int schematicTests(){
    // --- Components found by identifier, else by designator regardless of case; a designator two components have is
    // not taken; what the schematic lacks is extra.
    {Board b=newBoard("Abgleich",100,60);
        const int r1=put(b,"res-0207-10",{20,10},"R7","1k","r1");          // the identifier wins over the designator
        const int r2=put(b,"res-0207-10",{20,20},"r2","2k");                // lower case, no identifier
        const int c1=put(b,"cap-508",{20,30},"C1","100n"),c1b=put(b,"cap-508",{20,40},"c1","100n");
        const int x9=put(b,"header-2",{60,10},"X9","");
        documents::Targets t;t.components={part("r1","R1","1k",{"1","2"}),part("r2","R2","2k",{"1","2"}),part("c1","C1","100n",{"1","2"}),part("ic1","IC1","NE555",{"1","2","3","4","5","6","7","8"})};
        const auto c=checkNets(b,t);
        require(c.parts.size()==2&&c.parts[0].target==0&&c.parts[0].designator==r1&&!c.parts[0].byDesignator&&c.parts[1].designator==r2&&c.parts[1].byDesignator,
                "by identifier, else by designator regardless of case");
        require(c.missing==QList<int>{3}&&c.ambiguous==QStringList{"C1"},"a designator two components have is not taken, nor missing");
        require(c.extra.size()==3&&c.extra.contains(c1)&&c.extra.contains(c1b)&&c.extra.contains(x9),"components for none of the schematic");
        require(!c.passed()&&c.open.isEmpty()&&c.joined.isEmpty()&&c.airwires.isEmpty(),"no nets, no lines");}

    // --- Pins by name regardless of case; a diode's numbered contacts find its pads A and K; a mounting hole is no pin;
    // a pin without a pad wants the pins assigned.
    {Board b=newBoard("Anschlüsse",100,60);
        const int d1=put(b,"do-41",{20,10},"D1","1N4148","d1"),t1=put(b,"to-92",{20,30},"T1","BC547","t1"),j1=put(b,"header-2",{20,45},"J1","","j1");
        for(int i:padsOf(b,j1))b.elements[i].pin=b.elements[i].pin=="1"?"a":"b";
        auto hole=newElement(ElementType::Pad);hole.pos={30,45};hole.size=3;hole.size2=3;hole.part=b.elements[j1].part;b.elements<<hole;
        documents::Targets t;t.components={part("d1","D1","1N4148",{"1","2"}),part("t1","T1","BC547",{"E","B","C"}),part("j1","J1","",{"A","B"})};
        const auto c=checkNets(b,t);
        require(c.parts.size()==2&&c.unassigned.size()==1&&c.unassigned[0].designator==t1,"a transistor with numbered pads against E, B, C wants its pins assigned");
        require(c.padsFor(t,"d1","1")==QList<int>{pad(b,d1,"A")}&&c.padsFor(t,"d1","2")==QList<int>{pad(b,d1,"K")},"the anode is contact 1, the cathode 2");
        require(c.padsFor(t,"j1","A").size()==1&&c.padsFor(t,"j1","B").size()==1&&!c.padsFor(t,"j1","A").contains(int(b.elements.size())-1),"pins regardless of case, the mounting hole none");
        require(c.padsFor(t,"t1","E").isEmpty()&&c.padsFor(t,"x","1").isEmpty()&&c.padsFor(t,"d1","3").isEmpty(),"nothing for unknown pins");
        // Assigned in the order of the pads, the names follow.
        require(assignPins(b,t1,{"E","B","C"})&&!assignPins(b,t1,{"E","B"}),"three pads, three names");
        require(b.elements[pad(b,t1,"B")].pin=="B"&&checkNets(b,t).unassigned.isEmpty(),"assigned, the transistor counts");}

    // --- Open and joined nets: pieces, airwires from pad to pad, a via and the ground plane joining nets, the pads of one
    // pin joined inside the component, a pad without copper as a piece of its own, a net of one pin.
    {Board b=newBoard("Netze",120,80);
        const int r1=put(b,"res-0207-10",{20,10},"R1","1k","r1"),r2=put(b,"res-0207-10",{20,30},"R2","1k","r2"),r3=put(b,"res-0207-10",{20,50},"R3","1k","r3");
        const int d1=put(b,"do-41",{70,40},"D1","1N4148","d1");
        documents::Targets t;
        t.components={part("r1","R1","1k",{"1","2"}),part("r2","R2","1k",{"1","2"}),part("r3","R3","1k",{"1","2"}),part("d1","D1","1N4148",{"1","2"})};
        // VCC: R1.1, R2.1, R3.1; OUT: R1.2, D1.1 (anode); GND: D1.2, R2.2; R3.2 alone.
        t.nets={net("VCC",{{"r1","1"},{"r2","1"},{"r3","1"}}),net("",{{"r1","2"},{"d1","1"}}),net("GND",{{"d1","2"},{"r2","2"}}),net("",{{"r3","2"}})};
        joinPads(b,pad(b,r1,"1"),pad(b,r2,"1"));
        auto c=checkNets(b,t);
        require(c.parts.size()==4&&c.unassigned.isEmpty()&&c.joined.isEmpty(),"all found, nothing joined");
        require(c.open.size()==3&&c.open[0].net==0&&c.open[0].pins==QList<documents::TargetPin>{{"r3","1"}}
                &&c.open[0].pieces==QList<QList<documents::TargetPin>>({{{"r1","1"},{"r2","1"}},{{"r3","1"}}}),"an open net as its pieces, the largest first");
        require(c.open[1].net==1&&c.open[2].net==2,"each net open apart");
        bool vccLine=false;for(const auto &w:c.airwires)if(w.net==0)vccLine=w.to==pad(b,r3,"1")&&(w.from==pad(b,r2,"1")||w.from==pad(b,r1,"1"));
        require(c.airwires.size()==3&&vccLine,"one airwire per missing join, to the pad of the piece apart");
        bool vccNearest=false;for(const auto &w:c.airwires)if(w.net==0)vccNearest=w.from==pad(b,r2,"1");
        require(vccNearest,"from the nearest pad of the larger piece");
        // Joined by copper: OUT and GND, the diode's anode and cathode by one track.
        joinPads(b,pad(b,r1,"2"),pad(b,d1,"A"));joinPads(b,pad(b,d1,"K"),pad(b,r2,"2"));joinPads(b,pad(b,r3,"1"),pad(b,r2,"1"));
        c=checkNets(b,t);require(c.passed()&&c.airwires.isEmpty(),"all joined as the schematic wants");
        // The diode turned round is never taken the other way: two nets open, not swapped.
        {Board turned=b;for(int i:padsOf(turned,d1))turned.elements[i].name=turned.elements[i].pin=turned.elements[i].pin=="A"?"K":"A";
            const auto x=checkNets(turned,t);bool swapped=false;for(const auto &p:x.parts)swapped=swapped||p.swapped;
            require(x.open.size()==2&&!swapped,"a diode the wrong way round is open on both sides, never swapped");}
        // Two vias joining GND with the net of one pin through the top layer; without the top track nothing joins.
        {Board via=b;auto v=newElement(ElementType::Pad);v.size=1.2;v.size2=.6;v.via=true;v.pos={35,50};updateOutline(v);via.elements<<v;const int at=int(via.elements.size())-1;
            v.pos={35,30};updateOutline(v);via.elements<<v;
            track(via,via.elements[pad(via,r3,"2")].pos,{35,50});track(via,via.elements[pad(via,r2,"2")].pos,{35,30});
            require(checkNets(via,t).joined.isEmpty(),"vias alone join nothing");
            track(via,{35,50},{35,30},CopperTop);const auto x=checkNets(via,t);
            require(x.joined.size()==1&&x.joined[0].first==2&&x.joined[0].second==3&&x.joined[0].elements.contains(at)&&x.joined[0].elements.contains(pad(via,r3,"2"))
                    &&!x.joined[0].elements.contains(pad(via,r1,"1")),"two vias join two nets, their copper named and nothing else");}
        // The ground plane: pads without clearance on it join their nets.
        {Board ground=b;ground.groundPlane[CopperBottom]=true;for(int i:{pad(ground,r3,"2"),pad(ground,r2,"2")})ground.elements[i].clearance=0;
            const auto x=checkNets(ground,t);bool found=false;for(const auto &j:x.joined)found=found||(j.first==2&&j.second==3);
            require(found,"the ground plane joins GND with the net of one pin");}
        // A net of one pin wants no join.
        require(checkNets(b,t).open.size()==0,"a net of one pin is never open");
        // A pin pad without copper is a piece of its own.
        {Board bare=b;const int p=pad(bare,r3,"2");bare.elements[p].size2=bare.elements[p].size;
            documents::Targets more=t;more.nets[3].pins<<documents::TargetPin{"r1","2"};more.nets[1].pins.removeAll(documents::TargetPin{"r1","2"});
            const auto x=checkNets(bare,more);bool line=false;for(const auto &w:x.airwires)line=line||w.from==p||w.to==p;
            require(line,"a pad without copper stands apart");}}

    // --- A net of many pieces: the airwires join all of them along a shortest tree (the same length as a plain search).
    {Board b=newBoard("Viele",200,200);documents::Targets t;documents::TargetNet gnd{"GND",{}};
        for(int k=0;k<40;k++){const QString id=QStringLiteral("r%1").arg(k);put(b,"res-0207-10",{10.0+(k%8)*22+(k%3),10.0+(k/8)*30+(k%5)},QStringLiteral("R%1").arg(k+1),"1k",id);
            t.components<<part(id,QStringLiteral("R%1").arg(k+1),"1k",{"1","2"});gnd.pins<<documents::TargetPin{id,"1"};}
        t.nets={gnd};const auto c=checkNets(b,t);
        QList<int> pads;for(const auto &p:gnd.pins)pads<<c.padsFor(t,p.component,p.pin).first();
        QHash<int,int> root;std::function<int(int)> find=[&](int x){return root.value(x,x)==x?x:find(root[x]);};double total=0;
        for(const auto &w:c.airwires){root[find(w.from)]=find(w.to);total+=QLineF(b.elements[w.from].pos,b.elements[w.to].pos).length();}
        bool joined=true;for(int p:pads)joined=joined&&find(p)==find(pads.first());
        // Prim over all pairs, done plainly.
        QList<bool> in(pads.size(),false);in[0]=true;double plain=0;
        for(int step=1;step<pads.size();step++){double best=1e300;int next=-1;
            for(int i=0;i<pads.size();i++)if(in[i])for(int j=0;j<pads.size();j++)if(!in[j]){const double l=QLineF(b.elements[pads[i]].pos,b.elements[pads[j]].pos).length();if(l<best){best=l;next=j;}}
            in[next]=true;plain+=best;}
        require(c.airwires.size()==39&&joined&&std::abs(total-plain)<1e-9,"39 lines join 40 pieces along a shortest tree");}

    // --- Pins 1 and 2 of resistors may be the other way round: one of two resistors joined crossed to each other turns,
    // not both; the take-over names its pads that way.
    {Board b=newBoard("Tausch",100,60);
        const int r1=put(b,"res-0207-10",{20,10},"R1","1k","r1"),r2=put(b,"res-0207-10",{20,30},"R2","1k","r2");
        documents::Targets t;t.components={part("r1","R1","1k",{"1","2"}),part("r2","R2","1k",{"1","2"})};
        t.nets={net("A",{{"r1","1"},{"r2","1"}}),net("B",{{"r1","2"},{"r2","2"}})};
        // R1's pin 1 round the right to R2's pin 2, R1's pin 2 down the middle to R2's pin 1: crossed, without touching.
        require(QLineF(b.elements[pad(b,r1,"1")].pos,{14.92,10}).length()<1e-6&&QLineF(b.elements[pad(b,r2,"2")].pos,{25.08,30}).length()<1e-6,"pads where the routes expect them");
        path(b,QPolygonF(QList<QPointF>{{14.92,10},{14.92,5},{35,5},{35,30},{25.08,30}}));path(b,QPolygonF(QList<QPointF>{{25.08,10},{25.08,20},{14.92,20},{14.92,30}}));
        auto c=checkNets(b,t);int turned=0;for(const auto &p:c.parts)turned+=p.swapped;
        require(c.passed()&&turned==1,"one of the two turns, and all is joined");
        {documents::Targets sheets=t;sheets.components[0].designator="2R1";sheets.components[1].designator="=A-2R2";for(auto &c:sheets.components)c.kind="R";
            int turnedHere=0;const auto x=checkNets(b,sheets);for(const auto &p:x.parts)turnedHere+=p.swapped;
            require(x.passed()&&turnedHere==1,"the kind decides, not the designator with its sheet number or prefix");}
        {documents::Targets diodes=t;diodes.components[0].designator="D1";diodes.components[1].designator="D2";
            const auto x=checkNets(b,diodes);int any=0;for(const auto &p:x.parts)any+=p.swapped;
            require(any==0&&x.open.size()==2&&x.joined.size()==1,"diodes numbered 1 and 2 never turn: both nets open and joined crossed");}
        const auto changes=netChanges(b,c,t);require(changes.size()==1&&changes[0].swapPins&&!changes[0].text&&changes[0].component.isEmpty(),"the change: pins swapped");
        require(applyNetChanges(b,changes)==1,"one component changed");
        c=checkNets(b,t);turned=0;for(const auto &p:c.parts)turned+=p.swapped;require(c.passed()&&turned==0,"after the take-over nothing is turned any more");
        for(const auto &p:c.parts)for(int i:p.pads.value(0))require(b.elements[i].name==b.elements[i].pin,"the pad's name follows its pin");
    }

    // --- The take-over: link by identifier, designator and value from the schematic, one change per component.
    {Board b=newBoard("Übernahme",100,60);
        const int r=put(b,"res-0207-10",{20,10},"r5","4k7");put(b,"res-0207-10",{20,30},"R6","1k","r6");
        documents::Targets t;t.components={part("r5","R5","4,7k",{"1","2"}),part("r6","R6","1k",{"1","2"})};
        const auto c=checkNets(b,t);const auto changes=netChanges(b,c,t);
        require(changes.size()==1&&changes[0].designator==r&&changes[0].component=="r5"&&changes[0].text&&changes[0].designatorText=="R5"&&changes[0].value=="4,7k","link, designator and value");
        require(applyNetChanges(b,changes)==1&&b.elements[r].component=="r5"&&b.elements[r].text=="R5","applied");
        const auto again=checkNets(b,t);require(netChanges(b,again,t).isEmpty()&&!again.parts[0].byDesignator,"nothing left to take over");}
    // --- Missing parts: what the library offers, the part made for the component, laid beside the board in columns.
    {auto sheet=part("r9","=A-2R9","4k7",{"1","2"});sheet.kind="R";const auto bySheet=partChoices(sheet);
        require(!bySheet.isEmpty()&&bySheet[0].sameKind&&footprints()[bySheet[0].footprint].prefix=="R","a resistor by its kind, though its designator has a sheet number and prefix");}
    {const auto r=partChoices(part("r5","R5","4k7",{"1","2"}));
        require(!r.isEmpty()&&!r[0].inOrder&&r[0].sameKind&&footprints()[r[0].footprint].prefix=="R","a resistor's footprint first, a sure choice");
        require(r.last().footprint==-1&&r.last().inOrder&&!r.last().sameKind,"the wizard's row last");
        require(r[0].sure,"and offered as chosen");
        const auto q=partChoices(part("q1","Q1","BC547",{"E","B","C"}));
        require(!q.isEmpty()&&footprints()[q[0].footprint].id=="to-92"&&!q[0].inOrder&&q[0].sure&&q[0].leads==QStringList({"C","B","E"}),
            "a transistor named E, B, C: none fits by name, the TO-92 in the lead order of a BC547");
        const auto d=partChoices(part("d1","D1","1N4148",{"1","2"}));const QString first=d.isEmpty()?QString():footprints()[d[0].footprint].id;
        require(!d.isEmpty()&&!d[0].inOrder&&d[0].sameKind&&(first=="do-41"||first.startsWith("led")),"a diode's footprint fits its numbered contacts");
        const auto ic=partChoices(part("ic1","IC1","NE555",{"1","2","3","4","5","6","7","8"}));
        require(!ic.isEmpty()&&footprints()[ic[0].footprint].prefix=="IC"&&footprints()[ic[0].footprint].elements.size()>8,"an eight-pin IC: an IC package first");
        // With the board's grid: among those of the same kind, footprints with their pads on it first.
        const auto cap=part("c1","C1","100n",{"1","2"});const auto plain=partChoices(cap),gridded=partChoices(cap,1.27);
        auto onGrid=[](const PartChoice &c){return c.footprint>=0&&padsOnGrid(footprints()[c.footprint].elements,1.27);};
        require(!gridded.isEmpty()&&gridded[0].sameKind&&onGrid(gridded[0])&&gridded.size()==plain.size(),"a capacitor with its pads on the grid first");
        bool sorted=true,seenOff=false;for(const auto &c:gridded){if(!c.sameKind)break;if(!onGrid(c))seenOff=true;else if(seenOff)sorted=false;}
        require(sorted,"on the grid before off it, among the same kind");
        {int elko=-1;for(int i=0;i<footprints().size();i++)if(footprints()[i].id.startsWith("elko")){elko=i;break;}
            require(elko>=0&&!padsOnGrid(footprints()[elko].elements,1.27),"the electrolytics' pitches are off a 1.27 mm grid");}}
    // --- A transistor from the schematic library, its contacts B, C, E: the pads named in the lead order of its type
    // (a BC548: collector at lead 1, the base in the middle), never in the order of the contacts.
    {QFile file(QString(OPENLOCH_SOURCE_DIR)+"/libraries/schematic/elektro-elektronik-bauteile--transistoren.json");
        require(file.open(QIODevice::ReadOnly),"the schematic library of transistors");
        QStringList contacts;std::function<void(const QJsonValue&)> walk=[&](const QJsonValue &v){
            if(v.isArray())for(const auto &x:v.toArray())walk(x);
            else if(v.isObject()){const auto o=v.toObject();if(o["type"].toString()=="contact")contacts<<o["name"].toString();if(o.contains("children"))walk(o["children"]);}};
        for(const auto &x:QJsonDocument::fromJson(file.readAll()).object()["symbols"].toArray())
            if(x.toObject()["caption"].toString().section(u'\r',0,0)=="NPN-Transistor")walk(x.toObject()["item"]);
        require(contacts==QStringList({"B","C","E"}),"the library's NPN transistor: contacts B, C, E");
        {double flat=0,leads=0,first=0,last=0;
            for(const auto &e:footprint("to-92").elements){
                if(e.type==ElementType::Track&&e.layer==SilkTop&&e.points.size()==2&&e.points[0].y()==e.points[1].y())flat=e.points[0].y();
                if(e.type==ElementType::Pad){leads=e.pos.y();if(e.name=="1")first=e.pos.x();if(e.name=="3")last=e.pos.x();}}
            require(flat>leads&&first<last,"the TO-92 as the data sheets draw it: the flat side below the leads, lead 1 on the left");}
        const auto bc548=part("t1","T1","BC548",contacts);const auto offers=partChoices(bc548,1.27);
        require(!offers.isEmpty()&&offers[0].footprint>=0&&footprints()[offers[0].footprint].id=="to-92"&&offers[0].sure&&offers[0].leads==QStringList({"C","B","E"}),
            "a BC548: the TO-92 first and sure, its leads C, B, E");
        bool blind=false,other=false;
        for(const auto &c:offers)if(c.footprint>=0&&footprints()[c.footprint].id=="to-92"){blind=blind||c.inOrder;other=other||(!c.sure&&c.leads==QStringList({"E","B","C"}));}
        require(!blind&&other,"no TO-92 named in the order of the contacts, the other orders offered");
        auto leadsOf=[](const QList<Element> &els){QMap<double,QString> byX;for(const auto &e:els)if(e.type==ElementType::Pad)byX[e.pos.x()]=e.pin;return byX.values();};
        const Board scratch=newBoard("T",40,30);
        require(leadsOf(missingPart(offers[0],bc548,scratch))==QStringList({"C","B","E"}),"from the left: collector, base, emitter");
        auto first=[&](const QString &value,const QStringList &pins){const auto c=partChoices(part("t2","T2",value,pins));return c.isEmpty()?PartChoice{}:c[0];};
        require(first("BC 547 B",contacts).leads==QStringList({"C","B","E"})&&first("bc337-40",contacts).sure,"gain groups, spaces and small letters");
        const auto n3904=first("2N3904",contacts);
        require(n3904.sure&&leadsOf(missingPart(n3904,part("t2","T2","2N3904",contacts),scratch))==QStringList({"E","B","C"}),"a 2N3904: emitter, base, collector");
        require(first("BC639",contacts).leads==QStringList({"E","C","B"})&&first("MPSA42",contacts).leads==QStringList({"E","B","C"}),"a BC639 and an MPSA42");
        require(first("2N7000",{"G","D","S"}).leads==QStringList({"S","G","D"})&&first("BS170",{"G","D","S"}).leads==QStringList({"D","G","S"}),"MOSFETs: 2N7000, BS170");
        require(!first("BC5480",contacts).sure&&!first("2N2222",contacts).sure&&!first("2N7000",contacts).sure,
            "not for another number, a type whose makers differ, or a FET's order for B, C, E");
        // A type not in the table: nothing sure, the usual orders of a TO-92 offered, the SOT-23 in the order of its kind.
        const auto unknown=partChoices(part("t3","T3","",contacts));QList<QStringList> orders;bool sure=false,sot=false;
        for(const auto &c:unknown){sure=sure||c.sure;if(c.footprint<0)continue;const QString id=footprints()[c.footprint].id;
            if(id=="to-92")orders<<c.leads;if(id=="sot-23")sot=c.leads==QStringList({"B","E","C"});}
        require(!sure&&orders==QList<QStringList>({{"C","B","E"},{"E","B","C"},{"E","C","B"}})&&sot,"a type not known: the usual orders offered, none chosen");}
    {Board b=newBoard("Daneben",60,30);b.origin={0,30};b.grid=1.27;put(b,"res-0207-10",{20,10},"R1","1k","r1");
        auto beside=newElement(ElementType::Track);beside.layer=SilkTop;beside.points={{50,5},{70,5}};beside.width=.2;b.elements<<beside;
        documents::Targets t;t.components={part("r1","R1","1k",{"1","2"})};
        for(int k=1;k<=6;k++)t.components<<part(QStringLiteral("ic%1").arg(k),QStringLiteral("IC%1").arg(k),"NE555",{"1","2","3","4","5","6","7","8"});
        t.components<<part("q1","Q1","BC547",{"E","B","C"});
        Board scratch=b;QList<QList<Element>> parts;
        for(int k=1;k<t.components.size();k++){const auto els=missingPart(partChoices(t.components[k]).first(),t.components[k],scratch);scratch.elements+=els;parts<<els;}
        const int before=int(b.elements.size());const auto added=layBeside(b,parts);
        require(added.size()==int(b.elements.size())-before&&added.first()==before,"the parts appended");
        bool right=true;for(int i:added)right=right&&bounds(b.elements[i]).left()>70;require(right,"right of the board and of what lies beside it");
        const auto c=checkNets(b,t);require(c.missing.isEmpty()&&c.parts.size()==8&&c.unassigned.isEmpty(),"each found by its identifier, its pins assigned");
        QList<QRectF> boxes;bool grid=true,inside=true;
        for(const auto &x:components(b)){
            if(x.designator<before)continue;QRectF r;for(int i:x.members)r=r.united(bounds(b.elements[i]));boxes<<r;inside=inside&&r.top()>=-1e-9;
            const auto pads=padsOf(b,x.designator);const QPointF p=b.elements[pads.first()].pos-b.origin;
            grid=grid&&std::abs(p.x()/1.27-std::round(p.x()/1.27))<1e-6&&std::abs(p.y()/1.27-std::round(p.y()/1.27))<1e-6;
        }
        bool apart=true;for(int i=0;i<boxes.size();i++)for(int j=i+1;j<boxes.size();j++)apart=apart&&!boxes[i].intersects(boxes[j]);
        double lowest=0;for(const auto &r:boxes)lowest=std::max(lowest,r.bottom());
        require(boxes.size()==7&&apart&&grid&&inside,"apart from each other, the first pad of each on the grid");
        require(lowest<=30+1.27+1e-9,"in columns as high as the working area");
        Component named;for(const auto &x:components(b))if(b.elements[x.designator].text=="Q1")named=x;
        require(named.designator>=0&&named.value>=0&&b.elements[named.value].text=="BC547"
            &&b.elements[padsOf(b,named.designator)[1]].name=="B","named, valued, the transistor's pads after its pins");}

    // --- The editor: the comparison only with a schematic, its list follows changes, airwires on the board, a schematic
    // that cannot be read, the take-over and the pin assignment as one undo step each.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Board b=newBoard("Projekt",100,60);b.origin={0,0};
        const int r1=put(b,"res-0207-10",{20,10},"R1","1k","r1"),r2=put(b,"res-0207-10",{20,30},"r2","2k");
        const int t1=put(b,"to-92",{60,30},"T1","BC547","t1"),c2=put(b,"elko-50",{60,10},"C2","10µ","c2");
        const int vcc=joinPads(b,pad(b,r1,"1"),pad(b,r2,"1"));
        Document d;d.boards={b};editor.setDocument(d);QApplication::processEvents();
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};
        documents::Targets t;t.components={part("r1","R1","1k",{"1","2"}),part("r2","R2","2,2k",{"1","2"}),part("t1","T1","BC547",{"E","B","C"}),part("c1","C1","100n",{"1","2"}),
            part("c2","C2","10µ",{"1","2"})};
        t.nets={net("VCC",{{"r1","1"},{"r2","1"}}),net("",{{"r1","2"},{"r2","2"}})};
        auto *tabs=editor.findChild<QTabWidget*>();QWidget *page=editor.findChild<QWidget*>("schematicPage");auto *list=editor.findChild<QListWidget*>("schematicResults");
        while(tabs&&!tabs->findChild<QWidget*>("schematicPage",Qt::FindDirectChildrenOnly)&&tabs->indexOf(page)<0)tabs=tabs->parentWidget()?tabs->parentWidget()->findChild<QTabWidget*>():nullptr;
        require(page&&list&&tabs&&tabs->indexOf(page)>=0,"the schematic's side tab");
        require(!editor.action("compareSchematic")->isEnabled()&&!tabs->isTabVisible(tabs->indexOf(page))&&!editor.schematicAvailable(),"no schematic: the actions disabled, the tab hidden");
        bool extras=false;for(const auto &[title,names]:editorMenus())if(title=="&Extras")extras=names.contains("compareSchematic")&&names.contains("schematicAirwires")&&names.contains("takeOverSchematic")&&names.contains("assignPins");
        require(extras,"the actions in the Extras menu");
        bool schematic=true;bool broken=false;editor.hasSchematic=[&]{return schematic;};editor.targets=[&]{if(broken)throw std::runtime_error("kaputt");return t;};
        view->setSelection({r1});QApplication::processEvents();
        require(editor.action("compareSchematic")->isEnabled()&&tabs->isTabVisible(tabs->indexOf(page)),"with a schematic: enabled, the tab shown");
        auto c=editor.compareWithSchematic();
        require(tabs->currentWidget()==page&&c.missing==QList<int>{3}&&c.unassigned.size()==1&&c.open.size()==1,"compared: C1 missing, T1 unassigned, the second net open");
        const auto shown=lines(list);
        require(shown.size()==5&&shown[0]==ui("%1 Abweichung(en) vom Schaltplan").arg(3)&&shown.contains(ui("Fehlt auf der Platine: %1 (%2)").arg("C1","100n"))
                &&shown.contains(ui("Anschlüsse zuordnen: %1 (im Schaltplan %2)").arg("T1","E, B, C"))&&shown.contains(ui("Offen: %1").arg("R1.2 | R2.2"))
                &&shown.contains(ui("Im Schaltplan %1 (%2): %3 (%4) auf der Platine").arg("R2","2,2k","r2","2k")),"the lines of the comparison");
        int open=-1;for(int k=0;k<list->count();k++)if(list->item(k)->text().startsWith(ui("Offen: %1").arg("")))open=k;
        Q_EMIT list->itemClicked(list->item(open));const auto picked=view->selection();
        require(picked.size()==2&&picked.contains(pad(b,r1,"2"))&&picked.contains(pad(b,r2,"2")),"a click on the open net selects its pads");
        // Airwires: switched on, from the schematic only on the screen; an edit that opens VCC brings its line.
        editor.action("schematicAirwires")->trigger();require(view->schematicAirwires()==QList<std::pair<int,int>>{{pad(b,r2,"2"),pad(b,r1,"2")}}||view->schematicAirwires()==QList<std::pair<int,int>>{{pad(b,r1,"2"),pad(b,r2,"2")}},
            "the airwire of the open net");
        require(editor.findChild<QCheckBox*>("schematicAirwiresBox")->isChecked(),"the switch in the tab follows the action");
        const QImage picture=view->renderBoard(10);
        view->setSelection({vcc});editor.deleteSelection();QApplication::processEvents();QApplication::processEvents();
        require(view->schematicAirwires().size()==2&&lines(list)[0]==ui("%1 Abweichung(en) vom Schaltplan").arg(4),"the comparison follows the change");
        editor.undo();QApplication::processEvents();require(view->schematicAirwires().size()==1,"and undo");
        require(view->renderBoard(10)==picture,"pictures of the board never show them");
        editor.action("schematicAirwires")->trigger();require(view->schematicAirwires().isEmpty(),"switched off");
        // A schematic that cannot be read: the line alone after changes, the message when asked.
        broken=true;editor.compareWithSchematic(false);require(lines(list)==QStringList{ui("Der Schaltplan kann nicht gelesen werden: %1").arg("kaputt")},"a schematic that cannot be read");
        QString warned;inDialog([&](QDialog *x){for(auto *l:x->findChildren<QLabel*>())if(l->text().contains("kaputt"))warned=l->text();});editor.compareWithSchematic(true);
        require(warned.contains("kaputt"),"and a message when asked for");broken=false;
        // The take-over: R2 linked, named and valued as in the schematic, one undo step; what is left is named.
        QStringList changes,later;const QString ownId=els()[r2].component;require(!ownId.isEmpty()&&ownId!="r2","the editor gave R2 an identifier of its own");
        inDialog([&](QDialog *x){if(auto *l=x->findChild<QListWidget*>("takeOverChanges"))changes=lines(l);if(auto *l=x->findChild<QLabel*>("takeOverLater"))later=l->text().split('\n');});
        editor.takeOverFromSchematic();
        require(changes==QStringList{QStringLiteral("r2: %1 · %2").arg(ui("gehört zu %1 im Schaltplan").arg("R2"),ui("Bezeichner und Wert %1 (%2)").arg("R2","2,2k"))},"the list of changes");
        require(later.size()==2&&later[0]==ui("Fehlt auf der Platine: %1 (%2) – Extras → Fehlende Bauteile setzen…").arg("C1","100n"),"what is left to do");
        require(els()[r2].text=="R2"&&els()[r2].component=="r2"&&editor.canUndo(),"taken over");
        {bool valued=false;for(const auto &x:components(editor.document().board()))if(x.designator==r2&&x.value>=0)valued=els()[x.value].text=="2,2k";require(valued,"the value as well");}
        editor.undo();require(els()[r2].text=="r2"&&els()[r2].component==ownId,"one undo step for the take-over");
        // Assigning pins: the transistor's pads numbered on the board meanwhile, named E, B, C.
        view->setSelection({t1});QList<std::pair<int,QString>> labels;QStringList offered;
        inDialog([&](QDialog *x){labels=view->shownPadLabels();for(int k=1;k<=3;k++)if(auto *box=x->findChild<QComboBox*>(QStringLiteral("pin%1").arg(k))){
            offered<<box->currentText();box->setCurrentIndex(k);}});
        editor.assignPinsDialog();
        require(labels.size()==3&&labels[0].second=="1"&&view->shownPadLabels().isEmpty(),"the pads numbered while the dialog is open");
        require(offered==QStringList({"C","B","E"}),"each pad offered the pin of its lead in a BC547");
        const auto tPads=padsOf(editor.document().board(),t1);
        require(els()[tPads[0]].pin=="E"&&els()[tPads[1]].name=="B"&&els()[tPads[2]].pin=="C"&&editor.compareWithSchematic(false).unassigned.isEmpty(),"assigned, the names follow");
        editor.undo();require(els()[tPads[0]].pin=="1","one undo step for the pins");
        // Pads named B, C, E from lead 1 on, as when they were named in the order of the contacts: the dialog shows the
        // type's order and takes it over.
        view->setSelection({t1});
        inDialog([&](QDialog *x){for(int k=1;k<=3;k++)if(auto *box=x->findChild<QComboBox*>(QStringLiteral("pin%1").arg(k)))box->setCurrentText(QStringList({"B","C","E"})[k-1]);});
        editor.assignPinsDialog();require(els()[tPads[0]].pin=="B"&&els()[tPads[2]].pin=="E","the base at lead 1");
        QString hint;view->setSelection({t1});
        inDialog([&](QDialog *x){for(auto *l:x->findChildren<QLabel*>())if(l->text().contains("C-B-E"))hint=l->text();if(auto *take=x->findChild<QPushButton*>("usualLeads"))take->click();});
        editor.assignPinsDialog();
        require(hint==ui("Übliche Anschlussfolge für %1 im TO-92: %2").arg("BC547","C-B-E"),"the type's order shown");
        require(els()[tPads[0]].pin=="C"&&els()[tPads[1]].pin=="B"&&els()[tPads[2]].pin=="E","taken over: collector, base, emitter");
        editor.undo();editor.undo();require(els()[tPads[0]].pin=="1","an undo step each");
        // A type not in the table: numbered pads offered no pin.
        {t.components[2].value="XY1";QStringList none;view->setSelection({t1});const Document before=editor.document();
            inDialog([&](QDialog *x){for(int k=1;k<=3;k++)if(auto *box=x->findChild<QComboBox*>(QStringLiteral("pin%1").arg(k)))none<<box->currentText();});
            editor.assignPinsDialog();if(editor.document()!=before)editor.undo();t.components[2].value="BC547";
            require(none==QStringList(3,ui("kein Anschluss"))&&editor.document()==before,"a transistor of a type not known: no pin offered");}
        // An electrolytic left as it stands keeps its pads + and -: nothing changes, no undo step.
        view->setSelection({c2});inDialog([](QDialog *){});const Document unassigned=editor.document();editor.assignPinsDialog();
        require(editor.document()==unassigned&&els()[padsOf(editor.document().board(),c2)[0]].pin=="+","an electrolytic confirmed as it is keeps + and -");
        // The pad numbers show in the photo view as well.
        view->setPhotoView(true);QApplication::processEvents();const QImage plain=view->grab().toImage();
        view->setPadLabels({{pad(b,r1,"1"),"1"}});QApplication::processEvents();const QImage labelled=view->grab().toImage();view->setPadLabels({});view->setPhotoView(false);
        require(plain!=labelled,"pad numbers in the photo view");
        // Missing parts: C1 offered with a capacitor preselected, laid beside the board, one undo step.
        QString offeredC1;int choices=0;
        inDialog([&](QDialog *x){if(auto *box=x->findChild<QComboBox*>("part-C1")){offeredC1=box->currentText();choices=box->count();}});
        const int count=int(els().size());editor.placeMissingParts();
        const auto after=editor.compareWithSchematic(false);
        require(!offeredC1.isEmpty()&&offeredC1!=ui("Nicht setzen")&&choices>2&&after.missing.isEmpty()&&int(els().size())>count,"C1 offered, placed and found");
        bool besides=true;for(int i=count;i<els().size();i++)besides=besides&&bounds(els()[i]).left()>100;
        require(besides&&view->selection().size()==int(els().size())-count&&editor.action("schematicAirwires")->isChecked(),"beside the board, selected, the airwires on");
        editor.undo();require(int(els().size())==count,"one undo step");editor.action("schematicAirwires")->trigger();
        QString nothing;inDialog([&](QDialog *x){for(auto *l:x->findChildren<QLabel*>())if(!l->text().isEmpty())nothing=l->text();});
        {Board full=editor.document().board();const auto made=missingPart(partChoices(t.components[3]).first(),t.components[3],full);full.elements+=made;
            Document withC1=editor.document();withC1.boards[withC1.activeBoard]=full;editor.setDocument(withC1);}
        editor.placeMissingParts();require(nothing==ui("Auf der Platine fehlt kein Bauteil des Schaltplans."),"nothing missing: said so");
        // A component another board of the document has belongs there: left out here, never linked here.
        {Document two=editor.document();Board other=newBoard("Zweite",50,30);const int r9=put(other,"res-0207-10",{20,10},"R9","1k","r1");Q_UNUSED(r9);
            const int r1here=r1;two.boards.prepend(other);two.activeBoard=1;editor.setDocument(two);QApplication::processEvents();
            const auto x=editor.compareWithSchematic(false);bool listed=false;
            require(!listed&&x.extra.contains(r1here)&&x.missing.isEmpty(),"R1 linked on the other board: here it is not the schematic's R1, nor missing");
            inDialog([](QDialog *){});editor.takeOverFromSchematic();require(els()[r1here].component!="r1"||els()[r1here].component.isEmpty(),"and the take-over never links it here");
            editor.setDocument(d);QApplication::processEvents();}
        // The suite reports a changed project: the schematic goes and comes back without any edit in between.
        schematic=false;editor.projectChanged();require(!tabs->isTabVisible(tabs->indexOf(page))&&!editor.action("compareSchematic")->isEnabled(),"projectChanged: the schematic gone");
        schematic=true;editor.projectChanged();require(tabs->isTabVisible(tabs->indexOf(page))&&editor.action("compareSchematic")->isEnabled(),"projectChanged: the schematic back");
        // Without the schematic again, the tab goes.
        schematic=false;view->setSelection({0});view->setSelection({});QApplication::processEvents();require(!tabs->isTabVisible(tabs->indexOf(page))&&!editor.action("assignPins")->isEnabled(),"the schematic gone");}
    return 0;
}
