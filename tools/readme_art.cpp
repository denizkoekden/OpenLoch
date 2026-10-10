// Renders the pictures of the README from OpenLoch's own material: the toolbar with the program's pixel symbols, parts
// of the open library side by side, and one project in all four kinds of document (the two-transistor flasher of
// examples/perfboard as schematic, perfboard, circuit board and front panel), each drawn by its module. Used by
// scripts/make-readme-art.py:
//   openloch_readme_art <libraries folder> <output folder>
// With --example it writes that project as an OpenLoch project instead (examples/Blinklicht.openloch), its boards
// linked to the schematic and the perfboard behind the front panel with holes over its LEDs:
//   openloch_readme_art --example <libraries folder> <project file>
#include "canvas.h"
#include "geometry.h"
#include "icons.h"
#include "legacy_reader.h"
#include "language.h"
#include "openlibrary.h"
#include "printing.h"
#include "project.h"
#include "modules/pcb/editor.h"
#include "modules/pcb/font.h"
#include "modules/pcb/footprints.h"
#include "modules/pcb/netcheck.h"
#include "modules/schematic/library.h"
#include "modules/schematic/model.h"
#include "modules/schematic/render.h"
#include "frontpanel.h"
#include "panelrender.h"
#include "suite/documents.h"
#include "suite/boardsources.h"
#include "suite/schematictargets.h"
#include "documents/projectfile.h"
#include "targetcheck.h"
#include <QApplication>
#include <QDir>
#include <QTemporaryDir>
#include <cmath>
#include <stdexcept>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QTextStream>
using namespace openloch;
namespace {
// A row or column of symbols at twice their size, groups separated like the original's toolbars.
QImage strip(const QList<QStringList> &groups,int size,bool vertical){
    const int scale=2,cell=(size+6)*scale,gap=8*scale;int length=0;for(const auto &g:groups)length+=int(g.size())*cell+gap;length-=gap;
    QImage image(vertical?QSize(cell,length):QSize(length,cell),QImage::Format_ARGB32);image.fill(Qt::transparent);QPainter p(&image);int at=0;
    for(const auto &g:groups){
        for(const auto &name:g){const QPixmap icon=openLochIcon(name).pixmap(QSize(size,size)*scale);p.drawPixmap(vertical?QPoint(3*scale,at+3*scale):QPoint(at+3*scale,3*scale),icon);at+=cell;}
        // An etched separator like Windows 95.
        if(&g!=&groups.last()){const int m=at+gap/2-scale;p.fillRect(vertical?QRect(scale*2,m,cell-scale*4,scale):QRect(m,scale*2,scale,cell-scale*4),QColor(128,128,128));
            p.fillRect(vertical?QRect(scale*2,m+scale,cell-scale*4,scale):QRect(m+scale,scale*2,scale,cell-scale*4),Qt::white);}
        at+=gap;
    }
    return image;
}
QJsonObject page(const QString &folder,const QString &name){QFile f(folder+"/"+name);f.open(QIODevice::ReadOnly);return QJsonDocument::fromJson(f.readAll()).object();}
// The flasher's schematic: +5 V and ground rails, Q1 (mirrored) and Q2 with their LEDs and base resistors, the
// coupling capacitors crossed in the middle, the supply at the left and its capacitors at the right.
schematic::Document flasherSchematic(){
    using namespace openloch::schematic;
    auto document=newDocument(QStringLiteral("Blinklicht"));auto &sheet=document.sheets[0];sheet.width=132;sheet.height=92;sheet.titleBlock=TitleBlock{};
    const auto pages=builtInPages();
    auto entry=[&](const QString &page,const QString &caption){
        for(const auto &p:pages)if(localized(p.name)==page)for(const auto &e:p.entries)if(localized(e.caption)==caption)return e;
        throw std::runtime_error(("no symbol "+caption).toStdString());
    };
    auto place=[&](const LibraryEntry &e,QPointF at,const QString &designator,const QString &value,double rotation=0,bool mirrored=false){
        auto item=placedSymbol(e,document);item.pos=at;item.rotation=rotation;item.mirrored=mirrored;item.designator=designator;item.value=value;sheet.items.append(item);return item;};
    auto pin=[&](const Item &component,const QString &name){for(const auto *c:contacts(component))if(c->name==name)return pinPosition(component,*c);throw std::runtime_error("no pin");};
    auto line=[&](const QList<QPointF> &points){Item l;l.type=ItemType::Line;l.points=QPolygonF(points);sheet.items.append(l);};
    auto dot=[&](QPointF at){Item j;j.type=ItemType::Junction;j.pos=at;j.size=QSizeF(1,1);sheet.items.append(j);};
    const double vcc=15.24,gnd=81.28;
    const auto resistor=entry("Widerstände","Widerstand vertikal"),led=entry("LED","Leuchtdiode vertikal"),npn=entry("Transistoren","NPN-Transistor");
    const auto capacitor=entry("Kondensatoren","Kondensator"),upright=entry("Kondensatoren","Kondensator vertikal"),electrolytic=entry("Kondensatoren","Elektrolytkondensator vertikal");
    const auto j1=place(entry("Stecker, Buchsen, Klemmen","Stiftleiste 1×2"),{10.16,40.64},"J1","5 V");
    line({pin(j1,"1"),{15.24,40.64},{15.24,vcc},{119.38,vcc}});line({pin(j1,"2"),{17.78,43.18},{17.78,gnd},{119.38,gnd}});
    // Each half: resistor and LED to the collector, the base resistor to the base, the emitter to ground.
    for(const bool left:{true,false}){
        const double x=left?30.48:91.44,base=left?45.72:76.2;
        const auto r=place(resistor,{x,20.32},left?"R1":"R2","470R"),d=place(led,{x,33.02},left?"D1":"D2","LED rot");
        const auto q=place(npn,{left?35.56:86.36,60.96},left?"Q1":"Q2","BC547B",0,left);const auto rb=place(resistor,{base,20.32},left?"R3":"R4","47k");
        line({pin(r,"1"),{x,vcc}});line({pin(r,"2"),pin(d,"1")});line({pin(d,"2"),pin(q,"C")});line({pin(q,"E"),{x,gnd}});
        line({pin(rb,"1"),{base,vcc}});line({pin(rb,"2"),{base,pin(q,"B").y()},pin(q,"B")});
        dot({x,vcc});dot({base,vcc});dot({x,gnd});
    }
    // The coupling capacitors crossed over: C1 from Q1's collector to Q2's base, C2 from Q1's base to Q2's collector.
    const auto c1=place(capacitor,{50.8,45.72},"C1","10µ"),c2=place(capacitor,{60.96,53.34},"C2","10µ");
    line({{30.48,45.72},pin(c1,"1")});line({pin(c1,"2"),{76.2,45.72}});dot({30.48,45.72});dot({76.2,45.72});
    line({{45.72,53.34},pin(c2,"1")});line({pin(c2,"2"),{91.44,53.34}});dot({45.72,53.34});dot({91.44,53.34});
    const auto c3=place(upright,{106.68,40.64},"C3","100n"),c4=place(electrolytic,{119.38,40.64},"C4","47µ");
    for(const auto &c:{c3,c4}){line({pin(c,"1"),{pin(c,"1").x(),vcc}});line({pin(c,"2"),{pin(c,"2").x(),gnd}});}
    dot({pin(c3,"1").x(),vcc});dot({pin(c3,"2").x(),gnd});   // C4 closes the rails at their ends
    completeIds(document);
    return document;
}
QImage schematicPicture(double pixelsPerMm){
    using namespace openloch::schematic;
    const auto document=flasherSchematic();
    // Only the drawing, with a small white margin.
    const QImage sheetPicture=renderSheet(document,0,pixelsPerMm).convertToFormat(QImage::Format_RGB32);QRect used;
    for(int y=0;y<sheetPicture.height();y++){const auto *row=reinterpret_cast<const QRgb*>(sheetPicture.constScanLine(y));for(int x=0;x<sheetPicture.width();x++)if(qGray(row[x])<235)used|=QRect(x,y,1,1);}
    return sheetPicture.copy(used.adjusted(-12,-12,12,12).intersected(sheetPicture.rect()));
}
// The same circuit as circuit board: the footprints at the pins of the perfboard example, its wires as tracks (one
// bridge on the top side between two vias).
pcb::Document flasherBoard(const QJsonObject &netlist){
    using namespace openloch::pcb;
    const double hole=2.54;Board board;board.name=QStringLiteral("Blinklicht");board.width=30*hole;board.height=26*hole;
    {Element outline;outline.type=ElementType::Track;outline.layer=Outline;outline.width=.3;outline.points=QPolygonF(QList<QPointF>{{1,1},{board.width-1,1},{board.width-1,board.height-1},{1,board.height-1},{1,1}});board.elements<<outline;}
    const auto library=footprints();
    auto footprint=[&](const QString &id){for(const auto &f:library)if(f.id==id)return f;throw std::runtime_error(("no footprint "+id).toStdString());};
    const QMap<QString,QString> kinds{{"J","header-2"},{"Q","to-92"},{"R","res-0207-10"},{"D","led-5"}};
    for(const auto &value:netlist["parts"].toArray()){
        const auto part=value.toObject();const auto ref=part["ref"].toString();const auto pins=part["pins"].toObject();
        const QString id=ref=="C3"?"cap-508":ref=="C4"?"elko-63":ref.startsWith('C')?"cap-254":kinds.value(ref.left(1));
        auto elements=placeable(footprint(id),board);
        // Pad names as the perfboard names its pins (TO-92: 1, 2, 3 are C, B, E of the BC547).
        const QMap<QString,QString> alias{{"C","1"},{"B","2"},{"E","3"}};
        QMap<QString,QPointF> want;for(auto it=pins.begin();it!=pins.end();++it){const auto a=it.value().toArray();want[ref.startsWith('Q')?alias[it.key()]:it.key()]=QPointF(a[0].toDouble(),a[1].toDouble())*hole;}
        QMap<QString,QPointF> pads;for(const auto &e:elements)if(e.type==ElementType::Pad&&want.contains(e.name))pads[e.name]=e.pos;
        if(pads.size()<2)throw std::runtime_error(("pads of "+ref).toStdString());
        // Upright on the perfboard, lying in the library: turned a quarter (x, y → −y, x).
        // Turned in quarters so that the pads point as the perfboard's pins do.
        const auto names=pads.keys();const QPointF from=pads[names[1]]-pads[names[0]],to=want[names[1]]-want[names[0]];
        const double turn=std::round(qRadiansToDegrees(std::atan2(to.y(),to.x())-std::atan2(from.y(),from.x()))/90)*90;
        QTransform t;t.rotate(turn);const QPointF shift=want[names[0]]-t.map(pads[names[0]]);t=t*QTransform::fromTranslate(shift.x(),shift.y());
        for(auto &e:elements){
            e.points=t.map(e.points);e.pos=t.map(e.pos);
            if(e.type==ElementType::Text){if(e.role==TextRole::Designator)e.text=ref;if(e.role==TextRole::Value)e.text=ref=="J1"?"5V":ref.startsWith('D')?"rot":part["value"].toString().section(' ',0,0);updateStrokes(e);}
        }
        board.elements+=elements;
    }
    for(const auto &value:netlist["wires"].toArray()){
        const auto w=value.toObject();const auto a=w["start"].toArray(),b=w["end"].toArray();
        Element track;track.type=ElementType::Track;track.layer=a[2].toInt()?CopperTop:CopperBottom;track.width=.8;
        track.points=QPolygonF(QList<QPointF>{QPointF(a[0].toDouble(),a[1].toDouble())*hole,QPointF(b[0].toDouble(),b[1].toDouble())*hole});board.elements<<track;
    }
    for(const auto &value:netlist["vias"].toArray()){const auto a=value.toArray();Element via;via.type=ElementType::Pad;via.via=true;via.layer=CopperBottom;via.pos=QPointF(a[0].toDouble(),a[1].toDouble())*hole;via.size=1.4;via.size2=.6;
        via.points=padOutline(via.shape,via.pos,via.size,0);board.elements<<via;}
    Document document;document.title=QStringLiteral("Blinklicht");document.boards<<board;
    return document;
}
QImage circuitBoardPicture(const QJsonObject &netlist,int dpi){
    using namespace openloch::pcb;
    Editor editor;editor.setDocument(flasherBoard(netlist));QTemporaryDir folder;const QString file=folder.filePath("board.png");
    if(!editor.exportImage(file,dpi,true))throw std::runtime_error("board picture");
    return QImage(file);
}
// Its front panel: brushed aluminium with the two LEDs, the supply socket, engraved names and four mounting holes.
QImage frontPanelPicture(double dpi){
    using namespace openloch::frontpanel;
    Panel panel;panel.name=QStringLiteral("Blinklicht");panel.width=80;panel.height=40;panel.color=QColor("#c9ccd1");panel.gridVisible=false;
    auto drill=[&](QPointF at,double diameter){Element e;e.type=ElementType::Drill;e.center=at;e.diameter=diameter;panel.elements<<e;};
    auto ring=[&](QPointF at,double radius){Element e;e.type=ElementType::Ellipse;e.center=at;e.radiusX=e.radiusY=radius;e.pen.color=QColor("#4a4c50");e.pen.width=.6;e.fill.style=FillStyle::None;panel.elements<<e;};
    auto text=[&](const QString &value,QRectF frame,bool bold){Element e;e.type=ElementType::Text;e.text=value;e.bold=bold;e.frame=QPolygonF(QList<QPointF>{frame.topLeft(),frame.topRight(),frame.bottomLeft()});
        e.pen.color=QColor("#1d1d1f");e.fill.color=QColor("#1d1d1f");panel.elements<<e;};
    for(const QPointF at:{QPointF(4,4),QPointF(76,4),QPointF(4,36),QPointF(76,36)})drill(at,3.2);
    for(const double x:{22.0,40.0}){ring({x,20},4.2);drill({x,20},5.2);}
    ring({62,20},6.2);drill({62,20},8);
    text(QStringLiteral("BLINKLICHT"),QRectF(24,4.5,32,6),true);
    text(QStringLiteral("LED 1"),QRectF(18,27,8,3.4),false);text(QStringLiteral("LED 2"),QRectF(36,27,8,3.4),false);text(QStringLiteral("5 V"),QRectF(59.5,29,5,3.4),false);
    Document document;document.panels={panel};
    return renderPanel(document,document.panels[0],dpi);
}
QJsonObject flasherNetlist(const QDir &examples){
    QFile list(examples.filePath("01-Zweitransistor-Blinklicht-Netzliste.json"));if(!list.open(QIODevice::ReadOnly))throw std::runtime_error("netlist");
    return QJsonDocument::fromJson(list.readAll()).object();
}
// The flasher as one project of four documents: the schematic; the perfboard example and the circuit board, their parts
// linked to the schematic's components; and a front panel with the perfboard behind it and holes over its LEDs.
void exampleProject(const QString &libraries,const QString &file){
    using namespace openloch::documents;
    const QDir examples(QDir(libraries).filePath("../examples/perfboard"));
    const auto plan=flasherSchematic();const auto targets=suite::schematicTargets(schematic::toJson(plan));
    ProjectFile project;project.id=newId();project.title=QStringLiteral("Blinklicht");
    const QString planId=project.addDocument("schematic",QStringLiteral("Schaltplan"),schematic::toJson(plan));
    // The perfboard: its parts found by their designators and linked; the pins name themselves (C, B, E; A, K; +, -).
    auto board=Project::load(examples.filePath("01-Zweitransistor-Blinklicht.openloch"));board.assignIds();
    // Its transistors number their leads; the example's netlist says which hole is C, B and E, so they are named as
    // "Anschlüsse zuordnen" names them.
    const auto netlist=flasherNetlist(examples);
    for(const auto &part:board.components()){
        if(!part.designator.startsWith('Q'))continue;
        QJsonObject pins;for(const auto &v:netlist["parts"].toArray())if(v.toObject()["ref"]==part.designator)pins=v.toObject()["pins"].toObject();
        QStringList names;
        for(const QPointF at:pinPositions(board,part)){
            QString nearest;double best=1e9;
            for(auto it=pins.begin();it!=pins.end();++it){const auto a=it.value().toArray();const double d=QLineF(at,QPointF(a[0].toDouble(),a[1].toDouble())*254).length();if(d<best){best=d;nearest=it.key();}}
            if(best>60)throw std::runtime_error(("a lead of "+part.designator+" is in no hole of the netlist").toStdString());
            names<<nearest;
        }
        board.setPins(part.uid,names);
    }
    const auto before=checkTargets(board,targets);applyTargetChanges(board,targetChanges(before,targets));
    const auto linked=checkTargets(board,targets);
    if(!linked.missing.isEmpty()||!linked.unassigned.isEmpty()||!linked.ambiguous.isEmpty()){
        QStringList why;for(int m:linked.missing)why<<"missing "+targets.components[m].designator;
        for(const auto &u:linked.unassigned)why<<"pins of "+u.part.designator+": "+u.part.pins.join(",")+" against "+targets.components[u.target].pins.join(",");
        why+=linked.ambiguous;throw std::runtime_error(("the perfboard does not match the schematic: "+why.join("; ")).toStdString());
    }
    QTextStream(stdout)<<"perfboard against the schematic: "<<(linked.passed()?"passed":"open nets or joined nets left")<<"\n";
    QJsonObject perfboard=QJsonDocument::fromJson(board.encode()).object();
    const QString boardId=project.addDocument("perfboard",QStringLiteral("Lochraster"),perfboard);
    // The circuit board: each component linked to the schematic's component of its designator.
    auto circuit=flasherBoard(flasherNetlist(examples));
    // The TO-92 pads 1, 2, 3 are C, B and E of the BC547 (as the picture places them); they stand for those pins.
    const QMap<QString,QString> transistor{{"1","C"},{"2","B"},{"3","E"}};
    for(auto &b:circuit.boards)for(const auto &c:pcb::components(b)){
        const QString designator=b.elements[c.designator].text;
        for(const auto &t:targets.components)if(t.designator==designator)b.elements[c.designator].component=t.id;
        if(designator.startsWith('Q'))for(int i:c.members)if(b.elements[i].type==pcb::ElementType::Pad)b.elements[i].pin=transistor.value(b.elements[i].name,b.elements[i].pin);
    }
    {const auto check=pcb::checkNets(circuit.boards[0],targets);
     QTextStream(stdout)<<"circuit board against the schematic: "<<(check.passed()?"passed":QString("%1 missing, %2 to assign, %3 open, %4 joined").arg(check.missing.size()).arg(check.unassigned.size()).arg(check.open.size()).arg(check.joined.size()))<<"\n";
     for(const auto &o:check.open){QStringList pieces;for(const auto &piece:o.pieces){QStringList pins;for(const auto &pin:piece)pins<<targets.component(pin.component)->designator+"."+pin.pin;pieces<<pins.join(",");}QTextStream(stdout)<<"  open: "<<pieces.join(" | ")<<"\n";}}
    project.addDocument("pcb",QStringLiteral("Leiterplatte"),pcb::toJson(circuit));
    // The front panel: the perfboard behind it in the middle, holes over the two LEDs as "Bohrungen aus der Platine"
    // puts them, their names below, the title above and four mounting holes.
    using namespace openloch::frontpanel;
    const auto sources=suite::perfboardSources(boardId,QStringLiteral("Lochraster"),perfboard);
    Panel panel=newPanel(QStringLiteral("Blinklicht"),120,100);panel.color=QColor("#c9ccd1");panel.gridVisible=false;
    const QSizeF size=sources.value(0).size;const BoardBehind behind{boardId,sources.value(0).board,QPointF((panel.width-size.width())/2,(panel.height-size.height())/2),0,false};
    panel.boards={behind};
    auto text=[&](const QString &value,QRectF frame,bool bold){Element e=newElement(ElementType::Text);e.text=value;e.bold=bold;e.frame=QPolygonF(QList<QPointF>{frame.topLeft(),frame.topRight(),frame.bottomLeft()});
        e.fill.color=QColor("#1d1d1f");panel.elements<<e;};
    for(const QPointF at:{QPointF(5,5),QPointF(115,5),QPointF(5,95),QPointF(115,95)}){Element e=newElement(ElementType::Drill);e.center=at;e.diameter=3.2;panel.elements<<e;}
    int led=0;
    for(const auto &part:sources.value(0).parts){
        if(part.designator!="D1"&&part.designator!="D2")continue;
        const QPointF at=behind.toPanel().map(part.centre);
        Element hole=newElement(ElementType::Drill);hole.center=at;hole.diameter=5;hole.component=part.component;hole.name=part.designator;panel.elements<<hole;
        text(QStringLiteral("LED %1").arg(++led),QRectF(at.x()-4,at.y()+5,8,3.4),false);
    }
    if(led!=2)throw std::runtime_error("the LEDs of the perfboard");
    text(QStringLiteral("BLINKLICHT"),QRectF(44,6,32,6),true);
    frontpanel::Document front;front.title=QStringLiteral("Frontplatte");front.panels={panel};
    project.addDocument("frontpanel",QStringLiteral("Frontplatte"),front.toJson());
    project.active=planId;project.save(file);
    QTextStream(stdout)<<"example project written to "<<file<<"\n";
}
void suitePictures(const QString &libraries,const QString &out){
    const QDir examples(QDir(libraries).filePath("../examples/perfboard"));const auto netlist=flasherNetlist(examples);
    schematicPicture(6).save(out+"/suite-schematic.png");
    circuitBoardPicture(netlist,220).save(out+"/suite-pcb.png");
    frontPanelPicture(190).save(out+"/suite-panel.png");
    auto board=Project::load(examples.filePath("01-Zweitransistor-Blinklicht.openloch"));PrintView view;view.rulers=false;const double scale=.1;
    QImage scene(int(board.width*scale),int(board.height*scale),QImage::Format_ARGB32);scene.fill(Qt::transparent);
    {QPainter p(&scene);p.setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);p.scale(scale,scale);paintPrintBoard(p,board,view,false);}
    // Only the part of the board in use, with a row of holes around it.
    QRectF used;for(const auto &placed:board.placedObjects())used|=placed.transform.mapRect(legacyBounds(placed.node));
    const QRect crop=QRectF(used.adjusted(-254,-254,254,254).topLeft()*scale,used.adjusted(-254,-254,254,254).bottomRight()*scale).toAlignedRect().intersected(scene.rect());
    // A JPEG on the canvas colour, much smaller than the PNG.
    {const QImage part=scene.copy(crop);QImage flat(part.size(),QImage::Format_RGB32);flat.fill(QColor("#ece9da"));QPainter p(&flat);p.drawImage(0,0,part);p.end();flat.save(out+"/suite-perfboard.jpg","JPG",86);}
    for(const auto &info:suite::documentKinds())suite::kindIcon(info.kind).pixmap(QSize(32,32)).save(out+"/kind-"+info.id+".png");
}
}

int main(int argc,char **argv){
    QApplication app(argc,argv);setUiLanguage("de");
    if(argc==4&&QString::fromLocal8Bit(argv[1])=="--example"){
        try{exampleProject(QString::fromLocal8Bit(argv[2]),QString::fromLocal8Bit(argv[3]));}catch(const std::exception &e){QTextStream(stderr)<<"example project: "<<e.what()<<"\n";return 1;}
        return 0;
    }
    if(argc<3){QTextStream(stderr)<<"usage: openloch_readme_art <libraries> <output>\n       openloch_readme_art --example <libraries> <project file>\n";return 2;}
    const QString libraries=QString::fromLocal8Bit(argv[1]),out=QString::fromLocal8Bit(argv[2]);
    strip({{"new","open","save","print"},{"group","ungroup","front","back"},{"copy","cut","paste","duplicate","delete"},{"undo","redo"},
           {"align-left","align-hcenter","align-right","align-top","align-vcenter","align-bottom"},{"view-mono","view-flip","view-through","view-xray","view-bitmaps","view-free","view-potentials"}},16,false).save(out+"/toolbar.png");
    // Single symbols for the README buttons, at twice their size.
    for(const char *name:{"save","undolist"})openLochIcon(name).pixmap(QSize(32,32)).save(out+"/icon-"+name+".png");
    openLochCursor("kolben").pixmap().save(out+"/icon-kolben.png"); // the soldering iron pointer
    // Parts of the open library side by side at the same scale (7 pixels per mm), classic first, then modern ones.
    {
        const QList<std::tuple<QString,int,QString,QString>> parts{{"09-widerstaende.json",1,"Widerstand","R1"},{"06-leuchtdioden.json",3,"LED 5 mm","D1"},{"11-kondensatoren-elko.json",3,"Elko","C1"},
            {"08-transistoren.json",11,"BC546","T1"},{"14-ic.json",1,"DIL 8","IC1"},{"101-mikrocontroller-module.json",0,"ESP32-DevKitC","M1"},{"101-mikrocontroller-module.json",9,"Raspberry Pi Pico","M2"},
            {"103-anzeigemodule.json",0,"OLED 0,96″","M3"},{"102-sensormodule.json",0,"BME280","M4"},{"106-treiber-relais.json",4,"A4988","M5"}};
        QJsonArray manifest;int k=0;
        for(const auto &[file,index,caption,id]:parts){
            const auto bytes=openLibraryPage(page(libraries,file));auto node=LegacyReader(bytes).read(false)["objects"].toArray().at(index).toObject();node["id"]=id;
            const QImage picture=renderNode(node,bytes,.07);const QString name=QString("part-%1.png").arg(k++);picture.save(out+"/"+name);
            manifest.append(QJsonObject{{"file",name},{"caption",caption},{"width",picture.width()},{"height",picture.height()}});
        }
        QFile f(out+"/parts.json");f.open(QIODevice::WriteOnly);f.write(QJsonDocument(manifest).toJson());
    }
    try{suitePictures(libraries,out);}catch(const std::exception &e){QTextStream(stderr)<<"suite pictures: "<<e.what()<<"\n";return 1;}
    QTextStream(stdout)<<"toolbar, symbols, parts and the suite pictures written to "<<out<<"\n";return 0;
}
