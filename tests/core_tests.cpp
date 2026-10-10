#include "language.h"
#include "notes.h"
#include "project.h"
#include "legacy_reader.h"
#include "history.h"
#include "geometry.h"
#include "fixtures.h"
#include "continuity.h"
#include "hpgl.h"
#include "installation.h"
#include "openlibrary.h"
#include "printing.h"
#include "picturefill.h"
#include "legacy_writer.h"
#include "targetcheck.h"
#include "documents/projectfile.h"
#include "documents/libraryfolders.h"
#include "emfwriter.h"
#include <QJsonArray>
#include <QPainter>
#include <QtEndian>
#include <QMap>
#include <QPolygonF>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QSet>
#include <QRegularExpression>
#include <QDir>
#include <QDirIterator>
#include <QTextStream>
#include <QJsonDocument>
#include <functional>
#include <stdexcept>
using namespace openloch;
static void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
static void rejects(const std::function<void()> &fn){bool rejected=false;try{fn();}catch(const FormatError &){rejected=true;}require(rejected,"invalid input was accepted");}
int main(int argc,char **argv) {
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen"); // fonts for the printing on library pictures
    fixtures::checkOwnModel(); // the own model must write every LM4 file as the program does
    QGuiApplication app(argc,argv);openloch::setUiLanguage("de"); // the tests compare German texts; CI runners use English systems
    try {
        {   // the library folders of a kind of document, and the program's fixed folders that do not follow the language
            const openloch::LibraryFolders a{"/a",{"/b"}},b{"/a",{"/b"}};require(a==b&&!(a==openloch::LibraryFolders{"/a",{}}),"library folders compare");
            require(QDir::fromNativeSeparators(openloch::openLochDocumentsFolder("Bauteile")).endsWith("/OpenLoch/Bauteile"),"fixed folder under Dokumente/OpenLoch");
        }
        rejects([]{LegacyReader(QByteArray("MZ")).read(true);});
        rejects([]{Project::decode("{bad json");});
        rejects([]{Project::decode("{\"format\":\"Other\",\"version\":1}");});
        Project project;project.mode="schematic";project.title="Schaltplan Ω · µ";
        project.additions.append(QJsonObject{{"type","resistor"},{"x",254},{"y",508},{"text","R1 · 10 kΩ"}});
        project.additions.append(QJsonObject{{"type","wire"},{"x",0},{"y",508},{"x2",254},{"y2",508}});
        project.assignIds();auto recovered=Project::decode(project.encode());require(recovered.encode()==project.encode(),"project round trip changed data");
        QTemporaryDir tmp;auto path=tmp.filePath("test.openloch");project.save(path);require(Project::load(path).encode()==project.encode(),"disk round trip changed data");
        rejects([&]{project.save(tmp.filePath("wrong.LM4"));});
        project.moves["999"]=QJsonArray{0,0};rejects([&]{Project::decode(project.encode());});project.moves={};
        project.width=-1;rejects([&]{Project::decode(project.encode());});
        Project imported;imported.original=fixtures::project();imported.sourceKind="lm4";imported.legacy=LegacyReader(imported.original).read(true);
        require(imported.legacy["objects"].toArray()[0].toObject()["group_value"].toInt()==4,"legacy component number was not read");
        require(imported.legacyNode(0)["id"]=="R4"&&imported.billOfMaterials()[0].toObject()["id"]=="R4","legacy component number was not resolved");
        // Parts list as in LochMaster: the second group flag ("Erscheint in Stückliste") hides a component; nested components count.
        {auto unlisted=imported;auto edit=unlisted.edits["0"].toObject();edit["group_flags"]=QJsonArray{true,false};unlisted.edits["0"]=edit;require(unlisted.billOfMaterials().isEmpty(),"a component not marked for the parts list was listed");
         auto nested=imported;auto outer=nested.legacyNode(0);QJsonObject wrapper{{"type","TGruppe"},{"group_flags",QJsonArray{false,true}},{"children",QJsonArray{outer}}};
         QJsonObject assembly=wrapper;assembly["children"]=QJsonArray{wrapper};QJsonArray parts;std::function<void(const QJsonObject&)> list;Project probe;probe.legacy=QJsonObject{{"objects",QJsonArray{assembly}}};
         require(probe.billOfMaterials().size()==1&&probe.billOfMaterials()[0].toObject()["id"]=="R4","nested components must be listed, plain groups must not");}
        for(auto id:{"R1","R2","R3"})imported.additions.append(QJsonObject{{"type","resistor"},{"x",0},{"y",0},{"id",id}});
        require(imported.nextId("R#")=="R5","numbering collided with an imported component");imported.additions={};
        auto key=imported.addLibrary(fixtures::library(),"test.lib");
        require(Project::decode(imported.encode()).libraries.isEmpty(),"unused sources were retained");
        imported.additions.append(QJsonObject{{"type","component"},{"library",key},{"index",0},{"x",2540},{"y",2540},{"id","R1"},{"value","4,7 kΩ"}});
        imported.edits["0"]=QJsonObject{{"id","R2"},{"value","100 kΩ"},{"angle",90},{"mirrorX",true}};
        imported.boardSource=imported.addLibrary(fixtures::board(),"test.lmb","lmb");
        auto loaded=Project::decode(imported.encode());
        require(loaded.original==imported.original,"original bytes changed");
        require(loaded.libraries[key].bytes==fixtures::library(),"library bytes changed");
        imported.assignIds();require(Project::decode(imported.encode()).encode()==imported.encode(),"component/template round trip changed data");
        require(loaded.nextId("R#")=="R3","component identifiers are not unique");
        require(loaded.billOfMaterials().size()==2,"components missing from bill of materials");
        require(loaded.billOfMaterials()[0].toObject()["value"]=="100 kΩ","edited component value was lost");
        require(componentText("<BauteilWertTyp>",loaded.componentNode(loaded.additions[0].toObject()))=="4,7 kΩ","component placeholder not resolved");
        require(componentText("Mein Wert",{})=="Mein Wert","literal text was changed");
        auto valid=loaded.encode();auto invalid=[&](const std::function<void(Project&)> &change){auto p=Project::decode(valid);change(p);rejects([&]{Project::decode(p.encode());});};
        invalid([](Project &p){auto n=p.additions[0].toObject();n["index"]=.5;p.additions[0]=n;});
        invalid([](Project &p){auto n=p.additions[0].toObject();n["library"]="missing";p.additions[0]=n;});
        invalid([](Project &p){auto n=p.additions[0].toObject();n["mirrorX"]="yes";p.additions[0]=n;});
        invalid([](Project &p){p.edits["0"]=QJsonObject{{"type","TBohrung"}};});
        invalid([](Project &p){p.edits["0"]=QJsonObject{{"width",-20}};});
        invalid([](Project &p){p.boardSource="missing";});
        auto json=QJsonDocument::fromJson(valid).object();auto libraries=json["libraries"].toObject();auto source=libraries[key].toObject();source["data"]=QString::fromLatin1(fixtures::board().toBase64());libraries[key]=source;json["libraries"]=libraries;
        rejects([&]{Project::decode(QJsonDocument(json).toJson());});
        History history;history.begin(loaded);require(!history.commit(loaded),"a no-op created undo history");
        history.begin(loaded);loaded.edits["0"]=QJsonObject{{"deleted",true}};require(history.commit(loaded),"deletion did not create history");
        require(loaded.billOfMaterials().size()==1,"deleted component remains in the bill of materials");
        require(history.undo(loaded)&&loaded.encode()==valid,"undo did not restore imported component");
        require(history.redo(loaded)&&loaded.billOfMaterials().size()==1,"redo did not delete imported component");
        require(history.undo(loaded),"second undo failed");history.begin(loaded);loaded.title="Andere Platine";history.commit(loaded);require(!history.redo(loaded),"branching edit kept stale redo history");
        { // "Rückgängig-Aktionen": only the chosen number of steps is kept, older ones go at once when the number shrinks
            History limited;auto steps=Project::decode(valid);for(const char *name:{"A","B","C","D"}){limited.begin(steps);steps.title=name;limited.commit(steps);}
            limited.setLimit(2);require(limited.limit()==2&&limited.undo(steps)&&limited.undo(steps)&&steps.title=="B"&&!limited.undo(steps),"the undo limit was not applied");
            limited.setLimit(99);require(limited.limit()==50,"the undo limit must stay between 1 and 50");
        }
        auto multi=Project::decode(valid);multi.title="Erste";multi.addBoard(true);require(multi.boards.size()==2&&multi.activeBoard==1,"duplicating a board failed");multi.title="Zweite";multi.notes="Eigene Notiz Ω";multi.notesRtf.clear();multi.notesEdited=true;
        multi.switchBoard(0);require(multi.title=="Erste"&&multi.original==imported.original,"switching boards lost the first document");multi.switchBoard(1);require(multi.title=="Zweite"&&multi.notes=="Eigene Notiz Ω"&&plainNotes(multi.notesRtf)=="Eigene Notiz Ω","switching boards lost edited notes");
        multi=Project::decode(multi.encode());multi.addBoard();require(multi.boards.size()==3&&multi.original.isEmpty(),"new board inherited source geometry");multi.removeBoard();require(multi.boards.size()==2,"removing a board failed");multi.removeBoard();require(multi.boards.isEmpty()&&multi.title=="Erste","last remaining board was lost");
        // Continuity tester, synthetic: wire ends join pads, a wire passing over a pad does not.
        auto copperAt=[](const QList<Conductor> &found,QPointF p){for(const auto &c:found)if(c.copper&&c.shape.contains(p))return true;return false;};
        auto wiresIn=[](const QList<Conductor> &found){int n=0;for(const auto &c:found)n+=!c.copper;return n;};
        Continuity bridge;for(double x:{1000,1500,2000})bridge.addPad({x,1000},1.8);bridge.addWire(1,true,QPolygonF{{1000,1000},{2000,1000}},45);
        auto net=bridge.trace({1000,1000});require(copperAt(net,{2000,1000})&&!copperAt(net,{1500,1000})&&wiresIn(net)==1,"bridge must join its end pads only");
        require(bridge.trace({1500,1000}).size()==1,"a pad under a bridge is isolated");require(bridge.trace({1200,1200}).isEmpty(),"clicking off copper found a net");
        Continuity lead;lead.addPad({1000,2000},1.8);lead.addPad({1500,2000},1.8);lead.addWire(9,false,QPolygonF{{1000,2000},{1500,2000}},40);
        require(wiresIn(lead.trace({1000,2000}))==1&&!copperAt(lead.trace({1000,2000}),{1500,2000}),"a lead connects through its soldered first point only");
        require(wiresIn(lead.trace({1500,2000}))==0,"a lead's free end must not connect");
        // A cut splits a strip only when it spans the strip's full width; drills above 1.5 mm cut with a cross.
        auto stripWith=[](std::function<void(Continuity&)> extra){Continuity c;c.addStrip({500,3000},{4000,3000},100);extra(c);return c;};
        require(!copperAt(stripWith([](Continuity &c){c.addCut(QRectF(QPointF(1900,2850),QPointF(2100,3150)));}).trace({1000,3000}),{3000,3000}),"spanning cut did not split the strip");
        require(copperAt(stripWith([](Continuity &c){c.addCut(QRectF(QPointF(1900,2950),QPointF(2100,3150)));}).trace({1000,3000}),{3000,3000}),"partial cut split the strip");
        auto drilled=stripWith([](Continuity &c){c.addDrill({2000,3000},2.5);});require(!copperAt(drilled.trace({1000,3000}),{3000,3000}),"2.5 mm drill did not cut the strip");
        require(drilled.trace({2000,3000}).isEmpty(),"clicking inside a large drill must not trace");
        require(copperAt(stripWith([](Continuity &c){c.addDrill({2000,3000},.9);}).trace({1000,3000}),{3000,3000}),"a component hole cut the strip");
        require(copperAt(stripWith([](Continuity &c){c.addDrill({2000,3000},2.0);}).trace({1000,3000}),{3000,3000}),"a drill exactly as wide as the strip must not cut it");
        // Solder blob on the copper side joins an overlapping copper-side wire and component-side wire ends inside it.
        Continuity blob;blob.addPad({1000,4000},1.8);blob.addPad({3000,4000},1.8);blob.addPad({1000,5000},1.8);
        blob.addWire(19,true,QPolygonF{{1000,4000},{1000,4000}},260);blob.addWire(1,true,QPolygonF{{1120,4000},{3000,4000}},45);blob.addWire(1,false,QPolygonF{{1000,4110},{1000,5000}},45);
        Continuity bare;for(QPointF at:{QPointF(1000,4000),QPointF(3000,4000),QPointF(1000,5000)})bare.addPad(at,1.8);bare.addWire(1,true,QPolygonF{{1120,4000},{3000,4000}},45);bare.addWire(1,false,QPolygonF{{1000,4110},{1000,5000}},45);
        require(bare.trace({1000,4000}).size()==1,"wires near a pad joined it without a solder blob");
        auto blobbed=blob.trace({1000,4000});require(copperAt(blobbed,{3000,4000})&&copperAt(blobbed,{1000,5000})&&wiresIn(blobbed)==3,"solder blob rules not applied");
        // Project model: OpenLoch's perfboard has isolated pads on the grid; native wires join them at their ends.
        Project perfboard;perfboard.width=2540;perfboard.height=2540;perfboard.additions.append(QJsonObject{{"type","wire"},{"x",254},{"y",254},{"x2",762},{"y2",254}});
        auto board=continuityModel(perfboard);auto joined=board.trace({254,254});
        require(copperAt(joined,{762,254})&&!copperAt(joined,{508,254})&&wiresIn(joined)==1,"native wire on the generated perfboard joined the wrong pads");
        // Native lead and solder blob on OpenLoch's perfboard: a lead joins at its first point only; a copper-side blob joins
        // an overlapping copper-side wire to the pad under it.
        Project tools;tools.width=2540;tools.height=2540;
        tools.additions.append(QJsonObject{{"type","lead"},{"x",254},{"y",254},{"points",QJsonArray{QJsonArray{0,0},QJsonArray{0,762}}},{"back",false},{"width",45}});
        tools.additions.append(QJsonObject{{"type","solder"},{"x",508},{"y",508},{"width",300},{"back",true}});
        tools.additions.append(QJsonObject{{"type","wire"},{"x",640},{"y",508},{"x2",1016},{"y2",508},{"back",true}});
        auto toolModel=continuityModel(tools);const auto leadNet=toolModel.trace({254,254});
        require(wiresIn(leadNet)==1&&!copperAt(leadNet,{254,1016}),"a lead must connect at its soldered first point only");
        const auto blobNet=toolModel.trace({508,508});require(copperAt(blobNet,{1016,508})&&!copperAt(blobNet,{762,508}),"a copper-side solder blob must join the overlapping wire");
        // Both published examples: every net traced from one pin reaches exactly its own pins.
        for(const QString name:{"01-Zweitransistor-Blinklicht","02-Zehnkanal-Lauflicht"}){
            const QString base=QStringLiteral(OPENLOCH_SOURCE_DIR "/examples/perfboard/")+name;QFile list(base+"-Netzliste.json");require(list.open(QIODevice::ReadOnly),"example netlist missing");
            QMap<QString,QList<QPointF>> pins;QList<QPointF> unconnected;
            for(const auto &part:QJsonDocument::fromJson(list.readAll()).object()["parts"].toArray()){
                const auto o=part.toObject();for(const auto &pin:o["nets"].toObject().keys()){
                    const auto grid=o["pins"].toObject()[pin].toArray();const QPointF at(grid[0].toDouble()*254,grid[1].toDouble()*254);
                    const auto value=o["nets"].toObject()[pin];if(value.isNull())unconnected.append(at);else pins[value.toString()].append(at);
                }
            }
            auto example=continuityModel(Project::load(base+".openloch"));
            for(auto it=pins.begin();it!=pins.end();++it){
                const auto found=example.trace(it.value().first());
                for(auto p:it.value())if(!copperAt(found,p))throw std::runtime_error(QString("%1: net %2 misses a pin").arg(name,it.key()).toStdString());
                for(auto other=pins.begin();other!=pins.end();++other)if(other.key()!=it.key())for(auto p:other.value())if(copperAt(found,p))throw std::runtime_error(QString("%1: net %2 reaches net %3").arg(name,it.key(),other.key()).toStdString());
                for(auto p:unconnected)if(copperAt(found,p))throw std::runtime_error(QString("%1: net %2 reaches an unconnected pin").arg(name,it.key()).toStdString());
            }
            // Potentials: a VCC and a GND marker colour exactly the pins of their nets.
            auto marked=Project::load(base+".openloch");
            for(const auto &[net,colour]:{std::pair{"VCC","#ff0000"},std::pair{"GND","#0000ff"}})marked.additions.append(QJsonObject{{"type","potential"},{"x",pins[net].first().x()},{"y",pins[net].first().y()},{"color",colour},{"name",net}});
            const auto coloured=continuityModel(marked).potentials();require(coloured.conflicts==0,"example potentials reported a conflict");
            auto colourOf=[&](QPointF p){for(const auto &c:coloured.coloured)if(c.copper&&c.shape.contains(p))return c.potential;return QColor();};
            for(auto it=pins.begin();it!=pins.end();++it){const QColor expected=it.key()=="VCC"?QColor("#ff0000"):it.key()=="GND"?QColor("#0000ff"):QColor();
                for(auto p:it.value())if(colourOf(p)!=expected)throw std::runtime_error(QString("%1: pin of net %2 has the wrong potential colour").arg(name,it.key()).toStdString());}
            for(auto p:unconnected)if(colourOf(p).isValid())throw std::runtime_error("an unconnected pin received a potential");
            require(continuityModel(marked).shorts().isEmpty(),"example reported a short between VCC and GND");
            // A bridge from a VCC pin to a GND pin must be found as the short, and the chain must run through it.
            marked.additions.append(QJsonObject{{"type","wire"},{"x",pins["VCC"].first().x()},{"y",pins["VCC"].first().y()},{"x2",pins["GND"].first().x()},{"y2",pins["GND"].first().y()},{"back",false}});
            const auto bridged=continuityModel(marked).shorts();require(bridged.size()==1&&bridged[0].first=="VCC"&&bridged[0].second=="GND","bridged example must report one VCC/GND short");
            require(bridged[0].chain.size()==3&&!bridged[0].chain[1].copper,"the short must be located at the added bridge");
        }
        // Potentials, synthetic: the first marker colours its net, later markers there only count as conflicts.
        Continuity nets;for(double x:{1000,2000,4000})nets.addPad({x,1000},1.8);nets.addWire(1,true,QPolygonF{{1000,1000},{2000,1000}},45);
        nets.addMarker({1000,1000},Qt::red);nets.addMarker({2000,1000},Qt::blue);nets.addMarker({4000,1000},Qt::black);nets.addMarker({3000,3000},Qt::green);
        const auto netColours=nets.potentials();auto netColour=[&](QPointF p){for(const auto &c:netColours.coloured)if(c.copper&&c.shape.contains(p))return c.potential;return QColor();};
        require(netColour({1000,1000})==QColor(Qt::red)&&netColour({2000,1000})==QColor(Qt::red)&&netColours.conflicts==1,"first marker must colour the whole net");
        require(!netColour({4000,1000}).isValid()&&wiresIn(netColours.coloured)==1,"black markers or markers off copper must not colour anything");
        // Free areas: copper without terminals; occupation spreads through overlapping copper, not through wires.
        Continuity spare;spare.addStrip({500,6000},{3050,6000},100);spare.addStrip({500,6254},{3050,6254},100);
        spare.addPad({3100,6000},1.8);spare.addPad({5000,6000},1.8);spare.addPad({7000,6000},1.8);
        spare.addWire(9,false,QPolygonF{{3150,6000},{3150,7000}},40);spare.addMarker({5000,6000},Qt::black);
        const auto freeParts=spare.freeCopper();auto isFree=[&](QPointF at){for(const auto &c:freeParts)if(c.shape.contains(at))return true;return false;};
        require(!isFree({3150,6000})&&!isFree({1000,6000}),"a lead must occupy its pad and the strip overlapping it");
        require(isFree({1000,6254})&&isFree({7000,6000})&&!isFree({5000,6000}),"unused copper must stay free; potential markers occupy copper");
        // Shorts (OpenLoch extension): a net joining different potentials, with the conducting chain between them.
        Continuity shorted;for(double x:{1000,2000,3000,5000})shorted.addPad({x,8000},1.8);
        shorted.addWire(1,true,QPolygonF{{1000,8000},{2000,8000}},45);shorted.addWire(1,true,QPolygonF{{2000,8000},{3000,8000}},45);
        shorted.addMarker({1000,8000},Qt::red,"VCC");shorted.addMarker({3000,8000},Qt::blue,"GND");shorted.addMarker({2000,8000},Qt::green,"vcc");shorted.addMarker({5000,8000},Qt::blue,"GND");
        const auto found=shorted.shorts();require(found.size()==1&&found[0].first=="VCC"&&found[0].second=="GND","shorts must report one VCC/GND short; same names are one potential");
        int chainCopper=0,chainWires=0;for(const auto &c:found[0].chain)(c.copper?chainCopper:chainWires)++;
        require(chainCopper==3&&chainWires==2&&found[0].chain.first().shape.contains(QPointF(1000,8000))&&found[0].chain.last().shape.contains(QPointF(3000,8000)),"short chain must run pad-wire-pad-wire-pad from VCC to GND");
        Continuity unnamed;unnamed.addPad({1000,9000},1.8);unnamed.addMarker({1000,9000},Qt::red);unnamed.addMarker({1000,9000},Qt::red);unnamed.addMarker({1000,9000},Qt::black,"GND");
        require(unnamed.shorts().isEmpty(),"equal colours or black markers must not count as different potentials");
        QTextStream(stdout)<<"Continuity tester, potentials, free areas and shorts passed\n";
        { // outlines as LochMaster draws them: kinds 6/7 closed, lines open, corner smoothing styles 0-2
            auto wire=[](int kind,QJsonArray path,bool smooth=false,int style=0,double size=200){return QJsonObject{{"kind",kind},{"path",path},{"flag2",smooth},{"style",style},{"rotation",size}};};
            const QJsonArray square{QJsonArray{0,0},QJsonArray{1000,0},QJsonArray{1000,1000},QJsonArray{0,1000}};
            auto closed=drawnPath(wire(7,square));require(closed.size()==5&&closed.last()==QPointF(0,0),"an outline of kind 7 must be closed");
            require(drawnPath(wire(4,square)).size()==4,"a line of kind 4 must stay open");
            auto chamfer=drawnPath(wire(6,square,true,1,100));
            require(chamfer.size()==11&&chamfer[0]==QPointF(900,0)&&chamfer[1]==QPointF(1000,100)&&chamfer.last()==chamfer.first(),"chamfered corners differ from the original");
            auto round=drawnPath(wire(7,square,true,2,100));
            require(round.size()==4*21+1&&round[0]==QPointF(900,0)&&round[20]==QPointF(1000,100)&&round.last()==round.first(),"rounded corners differ from the original");
            auto curve=drawnPath(wire(7,square,true,0));require(curve.size()==4*21&&curve[0]==QPointF(500,0)&&curve[20]==QPointF(1000,500),"midpoint curve differs from the original");
            auto open=drawnPath(wire(4,QJsonArray{QJsonArray{0,0},QJsonArray{1000,0},QJsonArray{1000,1000}},true,2,100));
            require(open.size()==1+21+1&&open.first()==QPointF(0,0)&&open.last()==QPointF(1000,1000)&&open[1]==QPointF(900,0),"open smoothed line differs from the original");
            require(drawnPath(wire(7,QJsonArray{QJsonArray{0,0},QJsonArray{-3,-3}},true,0)).at(0)==QPointF(-1,-1),"midpoints must truncate towards zero");
        }
        { // placeholders anywhere in a label; a part's extra fields win over built-ins of the same name
            const QJsonObject part{{"id","R#"},{"group_value",3},{"value","4,7k"},{"label","Pull-up"},{"description","Widerstand"},{"extra",QJsonArray{QJsonArray{"URL",true,"https://example.org"},QJsonArray{"BauteilWertTyp",true,"überschrieben"}}}};
            require(componentText("<BauteilKennung> = <BauteilWertTyp> (<BauteilName>) <URL>",part)=="R3 = überschrieben (Pull-up) https://example.org","placeholders were not replaced like in the original");
            require(componentText("ohne Platzhalter",part)=="ohne Platzhalter","plain text changed");
        }
        { // HPGL export like the original's: the worked example of the drill, cut and outer-rectangle files
            PlotBoard board;board.width=board.height=1000;
            board.objects={QJsonObject{{"type","TTrenner"},{"kind",3},{"width",25},{"rect",QJsonArray{470,230,530,280}}},QJsonObject{{"type","TBohrung"},{"kind",14},{"diameter",.9},{"center",QJsonArray{254,254}}}};
            PlotOptions options;options.outline=true;auto jobs=plotJobs(board,options);
            QStringList names;for(const auto &j:jobs)names.append(j.name);
            require(names==QStringList{"Bohren mit 0,9 mm","Trennstellen","Aussenrechteck mit 2 mm"},"HPGL job names differ from the original's");
            auto file=[&](int i){return QString::fromLatin1(plotFile(jobs[i],board,options)).split("\r\n",Qt::SkipEmptyParts).join(' ');};
            require(file(0)=="IN; SP1; PT0; PU; PA102,102; PD; PA102,102; PU; PA0,0;","HPGL drill file differs");
            require(file(1)=="IN; SP1; PT0; PU; PA188,92; PD; PA188,112; PA212,112; PA212,92; PA188,92; PU; PA0,0;","HPGL cut file differs");
            require(file(2)=="IN; SP1; PT0; PU; PA-40,440; PD; PA440,440; PA440,-40; PA-40,-40; PU; PA0,0;","HPGL outer rectangle differs (three sides)");
            require(plotFile(jobs[0],board,options).endsWith("PU;\r\nPA0,0;\r\n"),"HPGL lines must end with CR LF");
            options.layer2=true;jobs=plotJobs(board,options);require(file(0).startsWith("IN; SP2;")&&file(1).startsWith("IN; SP1;")&&file(2).startsWith("IN; SP2;"),"layer 2 must not apply to cuts");
            options.layer2=false;options.commonOrigin=true;jobs=plotJobs(board,options);require(file(0)=="IN; SP1; PT0; PU; PA-40,-40; PD; PU; PA102,102; PD; PA102,102; PU; PA0,0;","common origin with outer rectangle differs");
            options.outline=false;jobs=plotJobs(board,options);require(file(0)=="IN; SP1; PT0; PU; PA0,0; PD; PU; PA102,102; PD; PA102,102; PU; PA0,0;","common origin mark differs");
            options.commonOrigin=false;options.millDrills=true;options.tool=1.0f;jobs=plotJobs(board,options);
            const auto milled=QString::fromLatin1(plotFile(jobs[0],board,options)).split("\r\n",Qt::SkipEmptyParts);
            require(jobs[0].name=="Bohrungen fräsen mit 0,90 mm"&&options.tool==0.9f&&milled.size()==343&&milled[4]=="PA116,112;"&&milled[341]=="PU;"&&milled[340]=="PA116,112;","milling drills differs from the original's");
            const auto corners=plotDrillCircle(board.objects[1],0.9f);require(corners.size()==336,"a milled drill has 16 corners of 21 points");
            // Exchange sort and names: one job per diameter, ascending; ausfräsen keeps the smaller tool
            board.objects={QJsonObject{{"type","TBohrung"},{"kind",14},{"diameter",1.0},{"center",QJsonArray{0,0}}},QJsonObject{{"type","TBohrung"},{"kind",14},{"diameter",.8},{"center",QJsonArray{254,0}}},QJsonObject{{"type","TBohrung"},{"kind",14},{"diameter",.8},{"center",QJsonArray{508,0}}},
                QJsonObject{{"type","TDraht"},{"kind",7},{"width",25},{"flag3",true},{"path",QJsonArray{QJsonArray{0,0},QJsonArray{1000,0},QJsonArray{1000,1000}}}}};
            PlotOptions plain;jobs=plotJobs(board,plain);names.clear();for(const auto &j:jobs)names.append(j.name);
            require(names==QStringList{"Bohren mit 0,8 mm","Bohren mit 1 mm","Fräsen mit 0,25 mm"}&&jobs[0].objects.size()==2,"HPGL jobs are not grouped and sorted like the original's");
            require(QString::fromLatin1(plotFile(jobs[2],board,plain)).split("\r\n",Qt::SkipEmptyParts).join(' ')=="IN; SP1; PT0; PU; PA0,0; PD; PA400,0; PA400,400; PA0,0; PU; PA0,0;","milled outline differs");
            // From a project: the board is written and read back; drills and cuts of OpenLoch are found by their kind
            Project p;p.additions={QJsonObject{{"type","drill"},{"x",254},{"y",508},{"diameter",1.2}},QJsonObject{{"type","cut"},{"x",1016},{"y",1016}}};
            const auto copy=plotBoard(p);int drills=0,cuts=0;for(const auto &o:copy.objects){drills+=o["type"]=="TBohrung";cuts+=o["type"]=="TTrenner";}
            require(drills>=1&&cuts==1&&copy.width==qRound(p.width)&&copy.objects.first()["type"]=="TTrenner","the HPGL board copy misses objects or their order");
        }
        { // identifiers of the objects and the parts of a board for projects
            const auto p=Project::fromLegacyBytes(fixtures::project(),"lm4","Test.LM4");
            const QString part=p.uidOf("legacy",0);
            require(p.uids.size()==p.legacy["objects"].toArray().size()&&part.size()==32,"imported objects did not get identifiers");
            auto back=Project::decode(p.encode());require(back.uidOf("legacy",0)==part,"identifiers did not survive saving");
            const auto parts=back.components();
            require(parts.size()==1&&parts[0].id==part&&parts[0].designator=="R4"&&parts[0].value=="10k"&&parts[0].pins==QStringList{"1"}&&back.componentOf("legacy",0)==part,"the part of the board for projects");
            require(back.setComponent(part,"R12","4k7")&&back.components()[0].designator=="R12"&&back.components()[0].value=="4k7"&&back.edits["0"].toObject()["id"]=="R#","Kennung and value from a project");
            // own objects: an identifier each, a copy gets a new one; the LM4 writer does not see them
            back.additions.append(QJsonObject{{"type","resistor"},{"x",254},{"y",254},{"text","R1"}});back.assignIds();
            const QString own=back.uidOf("new",0);back.additions.append(back.additions[0]);back.assignIds();
            require(own.size()==32&&back.uidOf("new",0)==own&&back.uidOf("new",1).size()==32&&back.uidOf("new",1)!=own,"a copy must get an identifier of its own");
            require(back.components().size()==3&&back.setComponent(own,"R2","1k")&&back.additions[0].toObject()["text"]=="R2","own parts for projects");
            auto plain=Project::fromLegacyBytes(fixtures::project(),"lm4","Test.LM4");plain.uids={};
            require(writeLegacyProject(plain)==writeLegacyProject(p),"identifiers must not change the LM4 file");
            // several boards: identifiers unique in the document, copied boards with their own, the parts of every board
            auto multi=Project::fromLegacyBytes(fixtures::project(),"lm4","Test.LM4");const QString first=multi.uidOf("legacy",0);multi.addBoard(true);
            require(multi.boards.size()==2&&multi.activeBoard==1&&multi.uidOf("legacy",0).size()==32&&multi.uidOf("legacy",0)!=first,"a duplicated board must get identifiers of its own");
            const auto both=multi.components();
            require(both.size()==2&&both[0].board==0&&both[0].id==first&&both[1].board==1&&both[1].id==multi.uidOf("legacy",0),"the parts of every board for projects");
            require(multi.setComponent(first,"R7","22k")&&multi.components()[0].designator=="R7"&&multi.components()[1].designator=="R4","Kennung of a part on another board");
            auto reread=Project::decode(multi.encode());require(reread.encode()==multi.encode(),"identifiers of several boards changed on saving");
            reread.switchBoard(0);require(reread.uidOf("legacy",0)==first&&reread.components()[0].designator=="R7","a board's identifiers changed on switching");
            auto clash=QJsonDocument::fromJson(multi.encode()).object();auto pages=clash["boards"].toArray();auto page=pages[0].toObject();auto ids=page["uids"].toObject();
            ids["0"]=multi.uidOf("legacy",0);page["uids"]=ids;pages[0]=page;clash["boards"]=pages;const auto fixed=Project::decode(QJsonDocument(clash).toJson());
            require(fixed.components()[0].id==multi.uidOf("legacy",0)&&fixed.uidOf("legacy",0).size()==32&&fixed.uidOf("legacy",0)!=multi.uidOf("legacy",0),"an identifier two boards share must be renewed on the later one");
            auto joined=Project::fromLegacyBytes(fixtures::project(),"lm4","Test.LM4");const auto same=joined;joined.appendBoards(same);
            const auto added=joined.components();require(added.size()==2&&added[0].id!=added[1].id&&added[1].id.size()==32,"added boards are copies with identifiers of their own");
            QTemporaryDir folder;const auto lm4=folder.filePath("Boards.LM4");{QFile f(lm4);require(f.open(QIODevice::WriteOnly)&&f.write(writeLegacyProject(multi))>0,"writing the boards failed");}
            const auto loaded=Project::load(lm4);const auto onBoards=loaded.components();
            require(loaded.boards.size()==2&&onBoards.size()==2&&onBoards[0].id.size()==32&&onBoards[1].id.size()==32&&onBoards[0].id!=onBoards[1].id,"every board of an LM4 file needs identifiers");
            require(Project::decode(loaded.encode()).encode()==loaded.encode(),"identifiers of an LM4 file's boards changed on saving");
            // the boards themselves: an identifier each, kept on saving and switching, new for copies, unique in the document
            require(p.boardId.size()==32&&Project::decode(p.encode()).boardId==p.boardId,"a board's identifier must survive saving");
            QStringList boardIds;for(const auto &page:QJsonDocument::fromJson(multi.encode()).object()["boards"].toArray())boardIds<<page.toObject()["boardId"].toString();
            require(boardIds.size()==2&&boardIds[0].size()==32&&boardIds[1].size()==32&&boardIds[0]!=boardIds[1]&&boardIds[1]==multi.boardId,"a duplicated board must get an identifier of its own");
            auto back0=Project::decode(multi.encode());back0.switchBoard(0);require(back0.boardId==boardIds[0],"a board's identifier changed on switching");
            require(joined.boardId.size()==32&&joined.boardId!=same.boardId,"an added board is a copy with an identifier of its own");
            require(loaded.boardId.size()==32&&QJsonDocument::fromJson(loaded.encode()).object()["boards"].toArray()[0].toObject()["boardId"].toString()!=loaded.boardId,"the boards of an LM4 file need identifiers of their own");
            auto twin=QJsonDocument::fromJson(multi.encode()).object();auto sheets=twin["boards"].toArray();auto sheet=sheets[0].toObject();sheet["boardId"]=multi.boardId;sheets[0]=sheet;twin["boards"]=sheets;
            const auto untwinned=Project::decode(QJsonDocument(twin).toJson());require(untwinned.boardId.size()==32&&untwinned.boardId!=multi.boardId,"a board's identifier two boards share must be renewed on the later one");
            rejects([&]{auto bad=QJsonDocument::fromJson(p.encode()).object();bad["boardId"]="Platine 1";Project::decode(QJsonDocument(bad).toJson());});
        }
        { // the own model of a perfboard (board.h): built from a project, the same LM4 file, its own format
            auto p=Project::fromLegacyBytes(fixtures::project(),"lm4","Modell.LM4");const auto key=p.addLibrary(fixtures::library(),"test.lib");
            p.additions.append(QJsonObject{{"type","component"},{"library",key},{"index",0},{"x",2540},{"y",2540},{"id","R1"},{"value","4,7 kΩ"}});
            p.additions.append(QJsonObject{{"type","wire"},{"x",254},{"y",508},{"x2",1016},{"y2",508}});p.assignIds();
            const auto model=BoardDocument::fromProject(p);const auto objects=model.boards[0].objects;
            require(model.boards.size()==1&&objects.size()==3,"the own model lost objects");
            const auto read=objects[0].toObject(),placed=objects[1].toObject(),wire=objects[2].toObject();
            require(read["type"]=="TGruppe"&&read["uid"]==p.uidOf("legacy",0)&&read["id"]=="R#"&&read.contains("start"),"a read object in the own model");
            require(placed["type"]=="TGruppe"&&placed["uid"]==p.uidOf("new",0)&&placed["part"].toObject()["library"]==key&&placed["id"]=="R1"&&!placed.contains("start"),"a library placement must be a resolved group naming its entry");
            require(wire["type"]=="wire"&&wire["uid"]==p.uidOf("new",1)&&!wire.contains("z"),"own objects stay as they are");
            require(model.writeLm4()==writeLegacyProject(p),"the own model wrote another LM4 file");
            const auto stored=model.encode();const auto back=BoardDocument::decode(stored);
            require(back.encode()==stored&&back.writeLm4()==model.writeLm4(),"the own format changed the model");
            require(model.boards[0].settings.boardId==p.boardId&&QJsonDocument::fromJson(stored).object()["boards"].toArray()[0].toObject()["id"]==p.boardId&&back.boards[0].settings.boardId==p.boardId,"the own model keeps the board's identifier");
            const auto plain=Project::fromLegacyBytes(fixtures::project(),"lm4","Modell.LM4");
            require(BoardDocument::fromProject(plain).writeLm4()==plain.original,"an unchanged board must be written as read");
            // a changed read object no longer names its record; turned and mirrored, it is drawn into its node
            auto turned=plain;turned.edits["0"]=QJsonObject{{"angle",90},{"mirrorX",true},{"value","22k"}};
            const auto node=BoardDocument::fromProject(turned).boards[0].objects[0].toObject();
            require(!node.contains("start")&&!node.contains("angle")&&!node.contains("mirrorX")&&node["value"]=="22k","a changed object must carry its changes in its node");
            // rejected: another format, a newer version, an identifier of the wrong form, unknown objects
            auto broken=[&](const std::function<void(QJsonObject&)> &change){auto root=QJsonDocument::fromJson(stored).object();change(root);rejects([&]{BoardDocument::decode(QJsonDocument(root).toJson());});};
            auto object=[](QJsonObject &root,int index,const std::function<void(QJsonObject&)> &change){
                auto pages=root["boards"].toArray();auto page=pages[0].toObject();auto list=page["objects"].toArray();auto n=list[index].toObject();change(n);list[index]=n;page["objects"]=list;pages[0]=page;root["boards"]=pages;};
            broken([](QJsonObject &r){r["format"]="OpenLoch";});
            broken([](QJsonObject &r){r["version"]=3;});
            broken([](QJsonObject &r){r["active"]=1;});
            broken([&](QJsonObject &r){object(r,0,[](QJsonObject &n){n["uid"]="xyz";});});
            broken([&](QJsonObject &r){object(r,0,[](QJsonObject &n){n["type"]="TUnbekannt";});});
            broken([&](QJsonObject &r){object(r,2,[](QJsonObject &n){n["type"]="laser";});});
            broken([&](QJsonObject &r){object(r,2,[](QJsonObject &n){n.remove("x");});});
            // an identifier that comes twice is renewed on the later object
            auto twice=QJsonDocument::fromJson(stored).object();object(twice,2,[&](QJsonObject &n){n["uid"]=read["uid"];});
            const auto renewed=BoardDocument::decode(QJsonDocument(twice).toJson()).boards[0].objects;
            require(renewed[0].toObject()["uid"]==read["uid"]&&renewed[2].toObject()["uid"].toString().size()==32&&renewed[2].toObject()["uid"]!=read["uid"],"an identifier that comes twice must be renewed");
        }
        { // target connections of a schematic and the board checked against them
            using documents::Targets;using documents::TargetComponent;using documents::TargetNet;
            const QString r1=documents::newId(),r2=documents::newId(),d1=documents::newId(),c1=documents::newId();
            Targets targets;
            targets.components={TargetComponent{r1,"R1","10k",{"1","2"}},TargetComponent{r2,"R2","4k7",{"1","2"}},TargetComponent{d1,"D1","1N4148",{"A","K"}}};
            targets.nets={TargetNet{"",{{r1,"2"},{r2,"1"}}},TargetNet{"IN",{{r1,"1"}}},TargetNet{"OUT",{{r2,"2"},{d1,"A"}}},TargetNet{"",{{d1,"K"}}}};
            require(Targets::fromJson(targets.toJson()).toJson()==targets.toJson(),"target connections changed in their JSON form");
            {auto json=targets.toJson();auto nets=json["nets"].toArray();auto net=nets[0].toObject();net["pins"]=QJsonArray{QJsonArray{r1,"9"}};nets[0]=net;json["nets"]=nets;rejects([&]{Targets::fromJson(json);});}
            // A perfboard without template: isolated pads every 2.54 mm. R1 at holes 1–3, R2 at 5–7, D1 at 9–11 of the first row.
            Project board;board.width=3302;board.height=1016;
            board.additions.append(QJsonObject{{"type","resistor"},{"x",508},{"y",254},{"text","R1"},{"value","10k"}});
            board.additions.append(QJsonObject{{"type","resistor"},{"x",1524},{"y",254},{"text","R2"},{"value","4k7"}});
            board.additions.append(QJsonObject{{"type","diode"},{"x",2540},{"y",254},{"text","D1"},{"value","1N4148"}});
            board.assignIds();const auto parts=board.components();
            require(parts.size()==3&&pinPositions(board,parts[0])==QList<QPointF>{{254,254},{762,254}},"the pins of a part on the board");
            auto check=checkTargets(board,targets);
            require(check.parts.size()==2&&check.parts[0].byDesignator&&check.missing.isEmpty()&&check.unassigned.size()==1&&check.unassigned[0].part.designator=="D1","parts are found by their Kennung, pins named otherwise need assigning");
            require(check.open.size()==1&&check.open[0].net==0&&check.airwires==QList<QLineF>{QLineF({762,254},{1270,254})},"a net not yet wired is open, with its airwire");
            board.additions.append(QJsonObject{{"type","wire"},{"x",762},{"y",254},{"x2",1270},{"y2",254}});
            require(board.setPins(parts[2].uid,{"A","K"})&&board.components()[2].pins==QStringList{"A","K"},"named pins of a part");
            board.additions.append(QJsonObject{{"type","wire"},{"x",1778},{"y",254},{"x2",2286},{"y2",254}});
            check=checkTargets(board,targets);require(check.passed()&&check.open.isEmpty()&&check.airwires.isEmpty(),"a board wired as its schematic passes");
            // The leads of a resistor are interchangeable: R1 turned round still fits.
            auto turned=targets;turned.nets[0].pins[0].pin="1";turned.nets[1].pins[0].pin="2";
            check=checkTargets(board,turned);require(check.passed()&&check.parts[0].swapped,"two pins 1 and 2 must fit either way round");
            // A wire too many joins two nets; a part too many and one missing are listed.
            board.additions.append(QJsonObject{{"type","wire"},{"x",254},{"y",508},{"x2",254},{"y2",254}});
            board.additions.append(QJsonObject{{"type","wire"},{"x",254},{"y",508},{"x2",762},{"y2",508}});
            board.additions.append(QJsonObject{{"type","wire"},{"x",762},{"y",508},{"x2",762},{"y2",254}});
            check=checkTargets(board,targets);require(check.joined.size()==1&&check.joined[0].first==0&&check.joined[0].second==1,"two nets joined on the board");
            auto more=targets;more.components<<TargetComponent{c1,"C1","100n",{"1","2"}};
            board.additions.append(QJsonObject{{"type","resistor"},{"x",508},{"y",762},{"text","R9"}});board.assignIds();
            check=checkTargets(board,more);require(check.missing==QList<int>{3}&&check.extra.size()==1&&check.extra[0].designator=="R9","missing and extra parts");
            // Linked: the part names its component and keeps it when its Kennung changes; two parts with one Kennung are not taken.
            require(board.linkComponent(parts[0].uid,r1)&&board.components()[0].id==r1&&board.components()[0].uid==parts[0].uid,"a part linked to its component");
            require(board.setComponent(r1,"R11","10k")&&!checkTargets(board,targets).parts[0].byDesignator,"a linked part is found by its component");
            board.additions.append(QJsonObject{{"type","resistor"},{"x",1524},{"y",762},{"text","R2"}});board.assignIds();
            check=checkTargets(board,targets);require(check.ambiguous==QStringList{"R2"}&&check.missing.contains(1),"a Kennung of two parts is not taken");
            // Linking and pin names are the project's: the LM4 file stays as read, the own model keeps them.
            auto read=Project::fromLegacyBytes(fixtures::project(),"lm4","Bezug.LM4");const auto part=read.components()[0];
            require(read.linkComponent(part.uid,r1)&&read.setPins(part.uid,{"A"})&&writeLegacyProject(read)==read.original,"project fields of a part must not change the LM4 file");
            const auto node=BoardDocument::fromProject(read).boards[0].objects[0].toObject();
            require(node["component"]==r1&&node["pins"]==QJsonArray{"A"}&&node.contains("start"),"the own model keeps the project fields of a part");
            require(Project::decode(read.encode()).components()[0].id==r1&&read.setPins(part.uid,{})&&read.linkComponent(part.uid,{})&&read.edits.isEmpty(),"removed project fields leave no edit");
            rejects([&]{auto bad=read;bad.edits["0"]=QJsonObject{{"pins",QJsonArray{"1","1"}}};Project::decode(bad.encode());});
            // Parts of the open libraries name their pins: a transistor fits E, B and C of the schematic by itself, a diode
            // the numbered contacts (1 the anode); a diode the wrong way round is never taken the other way round.
            require(assignedPins({"+","-"},{"2","1"})==QList<int>{1,0}&&assignedPins({"b","e","c"},{"E","B","C"})==QList<int>{1,0,2}&&assignedPins({"1","2"},{"A","K"})==QList<int>{-1,-1},"pins by name regardless of case, numbered contacts by polarity");
            auto pageOf=[](const QString &name){QFile f(QString(OPENLOCH_SOURCE_DIR)+"/libraries/"+name);require(f.open(QIODevice::ReadOnly),"a library page is missing");return QJsonDocument::fromJson(f.readAll()).object();};
            Project named;named.width=5080;named.height=2540;
            const auto transistors=named.addLibrary(openLibraryPage(pageOf("08-transistoren.json")),"LIB8.LIB","lib"),diodes=named.addLibrary(openLibraryPage(pageOf("02-dioden.json")),"LIB2.LIB","lib");
            named.additions.append(QJsonObject{{"type","component"},{"library",transistors},{"index",3},{"x",1270},{"y",1270},{"id","T1"}});
            named.additions.append(QJsonObject{{"type","component"},{"library",diodes},{"index",0},{"x",3048},{"y",1270},{"id","D1"}});
            named.assignIds();const auto semis=named.components();
            require(semis.size()==2&&semis[0].pins==QStringList{"B","E","C"}&&semis[1].pins==QStringList{"A","K"},"pins named by the library");
            const QString t1=documents::newId(),dd=documents::newId();
            Targets wanted;wanted.components={TargetComponent{t1,"T1","BC547",{"E","B","C"}},TargetComponent{dd,"D1","1N4148",{"1","2"}}};
            wanted.nets={TargetNet{"",{{t1,"C"},{dd,"1"}}}};
            check=checkTargets(named,wanted);
            require(check.unassigned.isEmpty()&&check.parts.size()==2&&check.parts[0].pins==QList<int>{1,0,2}&&check.parts[1].pins==QList<int>{0,1}&&check.open.size()==1,"named pins are assigned by themselves");
            named.additions.append(QJsonObject{{"type","wire"},{"x",check.parts[0].at[2].x()},{"y",check.parts[0].at[2].y()},{"x2",check.parts[1].at[0].x()},{"y2",check.parts[1].at[0].y()}});
            require(checkTargets(named,wanted).passed(),"the collector wired to the anode as in the schematic");
            auto reversed=wanted;reversed.nets[0].pins[1].pin="2";require(!checkTargets(named,reversed).passed(),"a diode the wrong way round must not pass");
            Project plain;plain.width=2540;plain.height=1016;
            plain.additions.append(QJsonObject{{"type","diode"},{"x",508},{"y",254},{"text","D2"}});plain.additions.append(QJsonObject{{"type","resistor"},{"x",1524},{"y",254},{"text","R5"}});
            plain.additions.append(QJsonObject{{"type","wire"},{"x",762},{"y",254},{"x2",1270},{"y2",254}});plain.assignIds();
            const QString d2=documents::newId(),r5=documents::newId();Targets polar;polar.components={TargetComponent{d2,"D2","",{"1","2"}},TargetComponent{r5,"R5","",{"1","2"}}};
            polar.nets={TargetNet{"",{{d2,"1"},{r5,"1"}}}};check=checkTargets(plain,polar);
            require(!check.passed()&&!check.parts[0].swapped,"a diode with numbered pins is never taken the other way round");
            // Two resistors joined crossed to each other: one of them turns, not both; with the number of their sheet in the
            // designator (1R1) they keep their kind.
            for(const bool paged:{false,true}){
                Project crossed;crossed.width=3302;crossed.height=1016;const QString one=paged?"1R1":"R1",two=paged?"1R2":"R2";
                crossed.additions.append(QJsonObject{{"type","resistor"},{"x",508},{"y",254},{"text",one}});
                crossed.additions.append(QJsonObject{{"type","resistor"},{"x",2032},{"y",254},{"text",two}});
                for(const QList<int> &w:QList<QList<int>>{{762,254,1778,254},{254,254,254,762},{254,762,2286,762},{2286,762,2286,254}})
                    crossed.additions.append(QJsonObject{{"type","wire"},{"x",w[0]},{"y",w[1]},{"x2",w[2]},{"y2",w[3]}});
                crossed.assignIds();
                const QString a=documents::newId(),b=documents::newId();Targets pair;
                pair.components={TargetComponent{a,one,"1k",{"1","2"},paged?"R":""},TargetComponent{b,two,"1k",{"1","2"},paged?"R":""}};
                pair.nets={TargetNet{"",{{a,"1"},{b,"1"}}},TargetNet{"",{{a,"2"},{b,"2"}}}};
                check=checkTargets(crossed,pair);
                require(check.passed()&&check.parts.size()==2&&int(check.parts[0].swapped)+int(check.parts[1].swapped)==1,"of two resistors joined crossed exactly one turns");
            }
            {   // a component linked on another board of the document belongs there
                Project two;two.width=2540;two.height=1016;
                two.additions.append(QJsonObject{{"type","resistor"},{"x",508},{"y",254},{"text","R1"}});two.assignIds();
                const QString r=documents::newId(),s=documents::newId();require(two.linkComponent(two.components()[0].uid,r),"linking a part");
                two.addBoard();
                Targets split;split.components={TargetComponent{r,"R1","1k",{"1","2"}},TargetComponent{s,"R2","1k",{"1","2"}}};
                split.nets={TargetNet{"",{{r,"2"},{s,"1"}}}};
                const auto here=targetsForBoard(two,split);
                require(here.components.size()==1&&here.components[0].id==s&&here.nets.size()==1&&here.nets[0].pins==QList<documents::TargetPin>{{s,"1"}},"a component on another board is left out with its pins");
                require(checkTargets(two,here).missing==QList<int>{0},"only the component the document has nowhere is missing");
                two.switchBoard(0);require(targetsForBoard(two,split).components.size()==2,"on its own board a component is compared");
            }
            // Pins named after the part's markings: DIL by the notch (pin 1 top left, counter-clockwise from above), a standing
            // diode by its band, a metal can by its tab (the emitter next to it), the second TO-92 as e-b-c.
            {
                Project marked;marked.width=5080;marked.height=2540;
                const auto ics=marked.addLibrary(openLibraryPage(pageOf("14-ic.json")),"LIB14.LIB","lib");const auto cans=marked.addLibrary(openLibraryPage(pageOf("08-transistoren.json")),"LIB8.LIB","lib");
                marked.additions.append(QJsonObject{{"type","component"},{"library",ics},{"index",2},{"x",2540},{"y",2540}});
                marked.additions.append(QJsonObject{{"type","component"},{"library",cans},{"index",17},{"x",7620},{"y",2540}});
                marked.additions.append(QJsonObject{{"type","component"},{"library",cans},{"index",4},{"x",10160},{"y",2540}});
                const auto standing=marked.addLibrary(openLibraryPage(pageOf("02-dioden.json")),"LIB2.LIB","lib");
                marked.additions.append(QJsonObject{{"type","component"},{"library",standing},{"index",12},{"x",12700},{"y",2540}});
                marked.assignIds();const auto parts=marked.components();require(parts.size()==4,"four marked parts");
                const auto dil=pinPositions(marked,parts[0]);auto at=[&](const QString &name){return dil.value(int(parts[0].pins.indexOf(name)));};
                double left=dil[0].x(),top=dil[0].y(),right=left,bottom=top;for(auto q:dil){left=qMin(left,q.x());right=qMax(right,q.x());top=qMin(top,q.y());bottom=qMax(bottom,q.y());}
                require(parts[0].pins.size()==14&&at("1")==QPointF(left,top)&&at("7")==QPointF(left,bottom)&&at("8")==QPointF(right,bottom)&&at("14")==QPointF(right,top),"DIL pins are numbered from the notch");
                require(QSet<QString>(parts[1].pins.begin(),parts[1].pins.end())==QSet<QString>{"E","B","C"}&&parts[2].pins==QStringList{"B","C","E"}&&parts[3].pins==QStringList{"A","K"},"metal can, TO-92 and standing diode name their pins");
            }
            for(const QString name:{"05-ttl.json","07-fassungen-hq.json","14-ic.json","35-fassungen-low-cost.json","42-atmel.json"}){   // every DIL IC and socket
                Project board;board.width=5080;board.height=5080;const auto key=board.addLibrary(openLibraryPage(pageOf(name)),name,"lib");
                const int count=int(board.libraries[key].document["objects"].toArray().size());
                for(int i=0;i<count;i++)board.additions.append(QJsonObject{{"type","component"},{"library",key},{"index",i},{"x",2540},{"y",2540}});
                board.assignIds();
                for(const auto &part:board.components()){
                    const auto at=pinPositions(board,part);const int n=int(at.size());require(n>=4&&n%2==0,"a DIL part with an odd number of pins");
                    double left=at[0].x(),top=at[0].y(),right=left,bottom=top;for(auto q:at){left=qMin(left,q.x());right=qMax(right,q.x());top=qMin(top,q.y());bottom=qMax(bottom,q.y());}
                    auto pin=[&](int number){return at.value(int(part.pins.indexOf(QString::number(number))),QPointF(-1e9,-1e9));};
                    require(pin(1)==QPointF(left,top)&&pin(n/2)==QPointF(left,bottom)&&pin(n/2+1)==QPointF(right,bottom)&&pin(n)==QPointF(right,top),"a DIL part is not numbered from its notch");
                }
            }
            // Missing parts beside the board: the library's best fit for each, named and linked like its component.
            const auto choices=openLibraryChoices("LIB8.LIB",pageOf("08-transistoren.json"))+openLibraryChoices("LIB2.LIB",pageOf("02-dioden.json"))+openLibraryChoices("LIB9.LIB",pageOf("09-widerstaende.json"));
            const TargetComponent transistor{documents::newId(),"Q1","BC546",{"E","B","C"}},diode{documents::newId(),"D7","1N4148",{"1","2"}},resistor{documents::newId(),"R3","10k",{"1","2"}};
            const auto forTransistor=fittingParts(transistor,choices),forDiode=fittingParts(diode,choices),forResistor=fittingParts(resistor,choices);
            require(!forTransistor.isEmpty()&&forTransistor[0].page=="LIB8.LIB"&&forTransistor[0].name.contains("BC546")&&sameKind("Q1",forTransistor[0].id),"a transistor of the library with its name first");
            require(!forDiode.isEmpty()&&forDiode[0].page=="LIB2.LIB"&&forDiode[0].pins==QStringList{"A","K"}&&!forResistor.isEmpty()&&forResistor[0].page=="LIB9.LIB","a diode and a resistor of the library first");
            require(fittingParts(TargetComponent{documents::newId(),"=A-2R3","10k",{"1","2"},"R"},choices).value(0).page=="LIB9.LIB"&&kindOf(TargetComponent{documents::newId(),"T1","",{}})=="T","the kind of a component by its kind, else by its designator");
            Project beside;beside.width=5080;beside.height=2540;
            const QMap<QString,QByteArray> bytes{{"LIB8.LIB",openLibraryPage(pageOf("08-transistoren.json"))},{"LIB2.LIB",openLibraryPage(pageOf("02-dioden.json"))}};
            const auto placed=placeBeside(beside,{{transistor,forTransistor[0]},{diode,forDiode[0]}},bytes);const auto now=beside.components();
            require(placed.size()==2&&now.size()==2&&now[0].id==transistor.id&&now[0].designator=="Q1"&&now[0].value=="BC546"&&now[1].designator=="D7"&&now[1].pins==QStringList{"A","K"},"parts set beside the board, named and linked");
            for(const auto &p:beside.placedObjects())require(p.transform.mapRect(legacyBounds(p.node)).left()>beside.width,"a part was set onto the board");
            for(const auto &c:now)for(auto at:pinPositions(beside,c))require(std::fmod(at.x(),254)==0&&std::fmod(at.y(),254)==0,"a part beside the board is off the hole grid");
            Targets both;both.components={transistor,diode};both.nets={TargetNet{"",{{transistor.id,"C"},{diode.id,"1"}}}};
            check=checkTargets(beside,both);require(check.missing.isEmpty()&&check.unassigned.isEmpty()&&check.airwires.size()==1,"set beside the board, the parts show their airwires");
            {   // a column of parts with their Kennung above them: none over another
                Project column;column.width=2540;column.height=2540;QList<std::pair<TargetComponent,LibraryChoice>> row;
                for(const QString designator:{"R3","R4","R5","R6"})row<<std::pair{TargetComponent{documents::newId(),designator,"10k",{"1","2"}},forResistor[0]};
                placeBeside(column,row,{{"LIB9.LIB",openLibraryPage(pageOf("09-widerstaende.json"))}});QList<QRectF> boxes;for(const auto &p:column.placedObjects())boxes<<p.transform.mapRect(legacyBounds(p.node));
                require(boxes.size()==4,"four parts set beside the board");
                for(int i=0;i<boxes.size();i++)for(int j=i+1;j<boxes.size();j++)require(!boxes[i].intersects(boxes[j]),"parts set beside the board overlap");
            }
        }
        { // Qt's standard buttons in the interface language
            installStandardTexts();
            setUiLanguage("en");const QString english=QCoreApplication::translate("QPlatformTheme","Cancel");
            setUiLanguage("fr");const QString french=QCoreApplication::translate("QPlatformTheme","&Yes");
            setUiLanguage("de");
            require(QCoreApplication::translate("QPlatformTheme","Cancel")=="Abbrechen"&&QCoreApplication::translate("QPlatformTheme","Close")=="Schließen"&&english=="Cancel"&&french=="&Oui","standard buttons in the interface language");
            require(QCoreApplication::translate("QPlatformTheme","Something else")=="Something else"&&QCoreApplication::translate("QFileDialog","Cancel")=="Cancel","other texts of Qt stay as they are");
        }
        { // the shared EMF writer: header with the frame in hundredths of a millimetre and the title, path records, end
            EmfDevice device(QSizeF(80,40),"Schaltplan");
            {QPainter p(&device);p.setPen(QPen(Qt::red,50,Qt::SolidLine,Qt::RoundCap));p.drawLine(QPointF(1000,1000),QPointF(7000,3000));p.fillRect(QRectF(100,100,500,500),Qt::blue);}
            const QByteArray emf=device.data();auto u32=[&](int at){return qFromLittleEndian<quint32>(emf.constData()+at);};auto i32=[&](int at){return qFromLittleEndian<qint32>(emf.constData()+at);};
            require(emf.size()>108&&u32(0)==1&&u32(40)==0x464D4520&&u32(48)==quint32(emf.size()),"an EMF begins with its header and knows its size");
            require(i32(24)==0&&i32(28)==0&&i32(32)==8000&&i32(36)==4000,"the frame of an EMF in hundredths of a millimetre");
            const int length=int(u32(60)),offset=int(u32(64));const QString title=QString::fromUtf16(reinterpret_cast<const char16_t*>(emf.constData()+offset),length);
            require(title.startsWith(QStringLiteral("OpenLoch"))&&title.contains(QStringLiteral("Schaltplan")),"the description names OpenLoch and the document");
            bool stroked=false,ended=false;for(int at=int(u32(4));at+8<=emf.size();at+=int(u32(at+4))){const quint32 type=u32(at);stroked=stroked||type==64||type==63;ended=type==14;if(u32(at+4)<8)break;}
            require(stroked&&ended,"an EMF strokes its lines and ends with its end record");
        }
        { // English interface: every ui("…") text of the sources has a translation; numbers and file names like the original's EN edition
            QSet<QString> table;for(const auto &[german,english]:translationTable()){require(!english.isEmpty()&&!table.contains(german),"empty or duplicate English entry");table.insert(german);}
            QStringList untranslated;const QRegularExpression literal(R"re((?<![A-Za-z_])ui\("((?:[^"\\]|\\.)*)"\))re");
            for(QDirIterator files(QString(OPENLOCH_SOURCE_DIR)+"/src",{"*.cpp"},QDir::Files,QDirIterator::Subdirectories);files.hasNext();){ // the modules and the suite too
                const QFileInfo info(files.next());if(info.fileName()=="language_en.cpp")continue;QFile f(info.filePath());require(f.open(QIODevice::ReadOnly),"a source file cannot be read");const auto text=QString::fromUtf8(f.readAll());
                for(auto it=literal.globalMatch(text);it.hasNext();){auto key=it.next().captured(1);key.replace("\\n","\n").replace("\\\"","\"").replace("\\t","\t").replace("\\\\","\\");if(!table.contains(key))untranslated.append(info.fileName()+": "+key);}
            }
            if(!untranslated.isEmpty())throw std::runtime_error(("texts without English translation: "+untranslated.join(" | ")).toStdString());
            setUiLanguage("en");
            PlotBoard board;board.width=board.height=1000;board.objects={QJsonObject{{"type","TTrenner"},{"kind",3},{"width",25},{"rect",QJsonArray{470,230,530,280}}},QJsonObject{{"type","TBohrung"},{"kind",14},{"diameter",.9},{"center",QJsonArray{254,254}}}};
            PlotOptions options;options.outline=true;QStringList names;for(const auto &j:plotJobs(board,options))names.append(j.name);
            const auto wire=objectDescription(QJsonObject{{"type","wire"},{"x",0},{"y",0},{"x2",762},{"y2",0}});
            const bool english=names==QStringList{"Drilling 0.9 mm","Spitters","Contour rectangle 2 mm"}&&wire.first=="Wire"&&wire.second=="(L=7.62 mm pitch 3)"&&ui("&Datei")=="&File"&&missingTranslations().isEmpty();
            setUiLanguage("de");require(english,"English job names, object texts or menu texts differ");
            require(objectDescription(QJsonObject{{"type","wire"},{"x",0},{"y",0},{"x2",762},{"y2",0}}).second=="(L=7,62 mm Lochabstand 3)","German object texts changed");
        }
        { // finding a LochMaster installation from the program, the library folder, a library file or the bottle
            QTemporaryDir dir;auto touch=[&](const QString &path){QDir().mkpath(QFileInfo(dir.filePath(path)).absolutePath());QFile f(dir.filePath(path));require(f.open(QIODevice::WriteOnly),"a test file cannot be written");f.write("x");};
            const QString drive="Flasche/drive_c/";touch(drive+"Program Files (x86)/LochMaster40/LochMaster40.exe");touch(drive+"ProgramData/LochMaster40/DE/LIB/LIB1.LIB");touch(drive+"ProgramData/LochMaster40/DE/LIB/LIB2.LIB");
            touch(drive+"ProgramData/LochMaster40/EN/LIB/LIB1.LIB");QDir().mkpath(dir.filePath(drive+"ProgramData/LochMaster40/DE/Bitmaps"));
            touch(drive+"users/Public/Documents/LochMaster40/Boards/LED.LM4");touch(drive+"users/Public/Documents/LochMaster40/Board Layouts/Raster.LMB");
            for(const QString &chosen:QStringList{drive+"Program Files (x86)/LochMaster40/LochMaster40.exe","Flasche",drive+"ProgramData/LochMaster40/DE/LIB",drive+"ProgramData/LochMaster40/DE/LIB/LIB2.LIB"}){
                const auto found=findLochMaster(dir.filePath(chosen));
                require(found.valid()&&found.pages==2&&found.projectCount==1&&found.templateCount==1&&found.bitmaps.endsWith("DE/Bitmaps")&&QDir(found.root)==QDir(dir.filePath("Flasche/drive_c")),"the installation was not found from what the user chose");
            }
            require(findLochMaster(dir.filePath("Flasche"),"EN").libraries.endsWith("EN/LIB"),"the library folder of the interface language is not preferred");
            touch("Kopie/LIB7.LIB");const auto bare=findLochMaster(dir.filePath("Kopie"));require(bare.valid()&&bare.pages==1&&bare.projects.isEmpty()&&bare.root==bare.libraries,"a folder with library files alone was not accepted");
            QDir().mkpath(dir.filePath("Leer"));require(!findLochMaster(dir.filePath("Leer")).valid()&&!findLochMaster(dir.filePath("fehlt")).valid(),"an empty folder must not count as installation");
            require(lochMasterCandidates({dir.filePath(drive+"Program Files (x86)")}).contains(dir.filePath("Flasche")),"a bottle above OpenLoch's folder is not a candidate");
        }
        { // OpenLoch's own libraries: every page expands without problems, writes a LIB page that reads back with all its parts
            int pages=0;
            for(const auto &info:QDir(QString(OPENLOCH_SOURCE_DIR)+"/libraries").entryInfoList({"*.json"},QDir::Files,QDir::Name)){
                QFile f(info.filePath());require(f.open(QIODevice::ReadOnly),"a source file cannot be read");QJsonParseError error;const auto page=QJsonDocument::fromJson(f.readAll(),&error).object();
                if(error.error!=QJsonParseError::NoError)throw std::runtime_error(("invalid library file "+info.fileName()+": "+error.errorString()).toStdString());
                QStringList problems;const auto parts=openLibraryParts(page,&problems);
                if(!problems.isEmpty())throw std::runtime_error(("library problems: "+problems.join(" | ")).toStdString());
                require(!parts.isEmpty()&&page["position"].toInt()>0&&!page["page"].toString().isEmpty()&&!page["page_en"].toString().isEmpty(),"a library page lacks parts, position or titles");
                const auto bytes=openLibraryPage(page);const auto document=LegacyReader(bytes).read(false);int groups=0,pictures=0,looks=0;
                for(const auto &o:document["objects"].toArray()){const auto g=o.toObject();if(!g.contains("children"))continue;++groups;for(const auto &c:g["children"].toArray())pictures+=c.toObject()["bitmap_size"].toInteger()>0;}
                for(const auto &part:parts)for(const auto &s:part["shapes"].toArray())looks+=!s.toObject()["look"].toObject().isEmpty();
                require(groups==parts.size()&&pictures==looks&&document["description"]==page["page"].toString(),"a library page does not read back with all its parts and pictures");
                const auto english=LegacyReader(openLibraryPage(page,true)).read(false);require(english["description"]==page["page_en"].toString(),"the English library page has the wrong title");
                ++pages;
            }
            require(pages>=2,"the open libraries are missing");
        }
        { // Bezugspunkt: a chosen terminal becomes the part's first, which LochMaster turns about; also for library parts and in the LM4
            auto lead=[](double x){return QJsonObject{{"type","TDraht"},{"kind",9},{"path",QJsonArray{QJsonArray{x,0},QJsonArray{x,300}}}};};
            const QJsonObject part{{"children",QJsonArray{QJsonObject{{"type","TDraht"},{"kind",6},{"path",QJsonArray{QJsonArray{0,0},QJsonArray{508,0},QJsonArray{508,200}}}},lead(0),lead(254),lead(508)}}};
            require(partTerminals(part)==QList<QPointF>{{0,0},{254,0},{508,0}},"the terminals of a part are not found in order");
            const auto turned=withReferenceTerminal(part,2);require(partTerminals(turned).first()==QPointF(508,0)&&turned["children"].toArray().size()==4,"the chosen terminal is not first");
            require(withReferenceTerminal(part,7)==part&&withReferenceTerminal(part,-1)==part,"an invalid Bezugspunkt changed the part");
            const QJsonObject wire{{"children",QJsonArray{QJsonObject{{"type","TDraht"},{"kind",1},{"path",QJsonArray{QJsonArray{0,0},QJsonArray{0,500},QJsonArray{762,500}}}}}}};
            require(partTerminals(wire)==QList<QPointF>{{0,0},{762,500}}&&partTerminals(withReferenceTerminal(wire,1)).first()==QPointF(762,500),"the far end of a wire cannot be the Bezugspunkt");
            QFile f(QString(OPENLOCH_SOURCE_DIR)+"/libraries/08-transistoren.json");require(f.open(QIODevice::ReadOnly),"a source file cannot be read");const auto page=QJsonDocument::fromJson(f.readAll()).object();
            Project board;const auto key=board.addLibrary(openLibraryPage(page),"LIB8.LIB","lib");board.additions.append(QJsonObject{{"type","component"},{"library",key},{"index",3},{"x",1270},{"y",1270}});
            const auto plain=partTerminals(board.componentNode(board.additions[0].toObject()));require(plain.size()==3,"the transistor has no three terminals");
            auto placement=board.additions[0].toObject();placement["reference"]=2;board.additions[0]=placement;
            require(partTerminals(board.componentNode(placement)).first()==plain[2],"the Bezugspunkt is not applied to a library part");
            const auto saved=Project::fromLegacyBytes(writeLegacyProject(board),"lm4","Bezug.LM4");QJsonObject group;for(const auto &v:saved.legacy["objects"].toArray())if(v.toObject().contains("children"))group=v.toObject();
            const auto stored=partTerminals(group);require(stored.size()==3&&stored[0]-stored[1]==plain[2]-plain[0],"the LM4 does not keep the Bezugspunkt as the first terminal");
        }
        { // Fill pictures with transparency: the outline of the opaque part, the picture flattened to a BMP over white
            QImage picture(300,120,QImage::Format_ARGB32);picture.fill(Qt::transparent);{QPainter p(&picture);p.setPen(Qt::NoPen);p.setBrush(Qt::red);p.drawRect(50,20,200,80);p.drawRect(260,50,20,20);}
            const auto fill=pictureFill(picture);const QImage bmp=QImage::fromData(fill.bmp,"BMP");
            require(bmp.size()==picture.size()&&!bmp.hasAlphaChannel()&&bmp.pixelColor(0,0)==QColor(Qt::white)&&bmp.pixelColor(100,50)==QColor(Qt::red),"the fill picture was not flattened over white");
            const auto r=fill.outline.boundingRect();
            require(fill.outline.size()>=4&&fill.outline.size()<=8&&std::abs(r.left()-50/300.0)<.01&&std::abs(r.right()-250/300.0)<.01&&std::abs(r.top()-20/120.0)<.01&&std::abs(r.bottom()-100/120.0)<.01,"the outline does not follow the largest opaque part");
            QImage solid(40,40,QImage::Format_RGB32);solid.fill(Qt::blue);require(pictureFill(solid).outline.isEmpty(),"an opaque picture must keep the area's outline");
        }
        { // Printing like LochMaster's print preview: defaults, storage, sheet counts with the original's rules, centring
            auto setup=defaultPrintSetup();
            require(setup.count==1&&setup.cutMarks&&setup.dataField&&!setup.landscape&&setup.views[0].centred&&!setup.views[1].centred&&setup.views[3].posX==-3000&&setup.views[9].posY==-9000&&setup.views[0].unit==2,"the print defaults differ from the original");
            QJsonObject document;setup.count=3;setup.onlyOne=true;setup.sheet=2;setup.views[1].flip=true;setup.views[1].scale=1.5;setup.views[1].original=false;setup.views[2].tilesX=3;setup.views[2].gapX=200;setup.views[2].texts=false;setup.views[2].solderMarks=false;
            storePrintSetup(document,setup);const auto back=printSetupOf(document);
            require(document["views"].toArray().size()==11&&back.count==3&&back.onlyOne&&back.sheet==2&&back.views[1].flip&&back.views[1].scale==1.5&&!back.views[1].original&&back.views[2].tilesX==3&&back.views[2].gapX==200&&!back.views[2].texts&&!back.views[2].solderMarks&&back.views[3].posX==-3000,"print settings do not survive storing");
            const Paper a4{200,287,5,5,210,297};PrintSetup one=defaultPrintSetup();
            require(sheetCount(one,{10000,8000},a4)==QSize(1,1),"a small board needs more than one sheet");
            require(sheetCount(one,{20000,8000},a4)==QSize(2,1),"an exact fit does not take one more sheet like in the original");
            one.views[0].rulers=false;
            require(sheetCount(one,{10000,27000},a4)==QSize(1,2),"the data field does not take 19 mm off the sheet");
            one.dataField=false;require(sheetCount(one,{10000,27000},a4)==QSize(1,1),"without the data field the board does not fit");
            one.dataField=true;require(sheetCount(one,{10000,26500},a4)==QSize(1,1),"the board does not fit below the data field");
            one.views[0].rulers=true;require(sheetCount(one,{10000,26500},a4)==QSize(1,2),"the rulers do not add 5 mm downwards");
            one.views[0].tilesX=2;require(!one.views[0].drawsRulers()&&sheetCount(one,{10000,26500},a4)==QSize(2,1),"tiles do not switch the rulers off or do not widen the view");
            PrintSetup centred=defaultPrintSetup();centreView(centred.views[0],centred,{10000,8000},a4);
            require(centred.views[0].posX==-5000&&centred.views[0].posY==-(26800/2)+4000,"centring does not put the board in the middle of the sheet");
            const auto at=printTransform(centred.views[0],{10000,8000}).map(QPointF(0,0));require(qAbs(at.x()-50)<1e-9&&qAbs(at.y()-94)<1e-9,"the board does not start where the view puts it");
            auto flipped=centred.views[0];flipped.flip=true;const auto top=printTransform(flipped,{10000,8000}).map(QPointF(0,0));require(qAbs(top.x()-50)<1e-9&&qAbs(top.y()-174)<1e-9,"Wenden does not mirror the board vertically in place");
            auto moved=centred.views[0];moved.scale=2;setViewPosition(moved,a4,{25.5,40});const auto shown=viewPosition(moved,a4);require(qAbs(shown.x()-25.5)<.02&&qAbs(shown.y()-40)<.02,"Links/Oben do not round-trip");
            require(sheetCell(5,{2,3})==QPoint(0,2)&&sheetCell(2,{2,3})==QPoint(1,0),"sheets are not numbered row by row");
            require(correctionFactor("1,05")==1.05&&correctionFactor(" 0.8 ")==.8&&!correctionFactor("1.3")&&!correctionFactor("abc"),"correction factors are not checked like in the original");
            const auto field=dataField("Blinker","Platine",{10000,8000},0,"Anna [Werkstatt]",QDateTime(QDate(2026,10,8),QTime(14,5,9)),2,4);
            require(field.left.size()==3&&field.left[0]=="Projekt: Blinker [Platine]"&&field.left[1]=="Abmessungen: 100,00 x 80,00 mm"&&field.right[2]=="Blatt 2 / 4","the data field lines differ");
            // Changed print settings are saved with the board: in a file of the original (its tail is otherwise copied
            // unchanged) and on a new board, and they survive the OpenLoch format.
            for(const bool original:{true,false}){
                Project board=original?Project::load(QString(OPENLOCH_SOURCE_DIR)+"/examples/perfboard/01-Zweitransistor-Blinklicht.LM4"):Project();
                auto changed=printSetupOf(board);const int untouched=changed.views[2].posX;
                changed.count=2;changed.landscape=true;changed.views[1].flip=true;changed.views[1].posX=-1234;changed.views[0].potentials=true;storePrintSetup(board,changed);
                const auto bytes=writeLegacyProject(board);const auto reread=Project::fromLegacyBytes(bytes,"lm4","Druck.LM4");const auto back=printSetupOf(reread);
                require(back.count==2&&back.landscape&&back.views[1].flip&&back.views[1].posX==-1234&&back.views[0].potentials&&back.views[2].posX==untouched&&(original||untouched==-2000),original?"print settings are not saved in the LM4 file of the original":"print settings are not saved in the LM4 file of a new board");
                require(reread.legacy["objects"].toArray().size()==board.legacy["objects"].toArray().size(),"saving the print settings changed the objects");
                const auto decoded=Project::decode(board.encode());require(printSetupOf(decoded).views[1].posX==-1234,"print settings are lost in the OpenLoch format");
            }
        }
        QTextStream(stdout)<<"Core tests passed\n";return 0;
    }catch(const std::exception &e){QTextStream(stderr)<<e.what()<<"\n";return 1;}
}
