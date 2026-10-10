#include "language.h"
#include "canvas.h"
#include "picturefill.h"
#include "project.h"
#include "history.h"
#include "geometry.h"
#include "fixtures.h"
#include "documents/projectfile.h"
#include <QJsonArray>
#include <QDateTime>
#include <QTextList>
#include <QTextBlock>
#include <QTextDocument>
#include "notes.h"
#include "legacy_reader.h"
#include <QLineF>
#include <QApplication>
#include <QFile>
#include <QDirIterator>
#include <QImage>
#include <QBuffer>
#include <QPainter>
#include <QGraphicsItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QPrinter>
#include "legacy_writer.h"
#include <QTextStream>
#include <stdexcept>
#include <cmath>

using namespace openloch;
static void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
static void click(Canvas &canvas,QPointF position) {
    const auto local=QPointF(canvas.mapFromScene(position));
    const auto global=QPointF(canvas.viewport()->mapToGlobal(local.toPoint()));
    QMouseEvent down(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(),&down);
    QMouseEvent up(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(),&up);
    QApplication::processEvents();
}
static void drag(Canvas &canvas,QPointF from,QPointF to) {
    auto send=[&](QEvent::Type type,QPointF position,Qt::MouseButton button,Qt::MouseButtons buttons){
        const auto local=QPointF(canvas.mapFromScene(position)),global=QPointF(canvas.viewport()->mapToGlobal(local.toPoint()));
        QMouseEvent event(type,local,global,button,buttons,Qt::NoModifier);QApplication::sendEvent(canvas.viewport(),&event);
    };
    send(QEvent::MouseButtonPress,from,Qt::LeftButton,Qt::LeftButton);
    send(QEvent::MouseMove,to,Qt::NoButton,Qt::LeftButton);
    send(QEvent::MouseButtonRelease,to,Qt::LeftButton,Qt::NoButton);QApplication::processEvents();
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);openloch::setUiLanguage("de"); // the tests compare German texts; CI runners use English systems
    fixtures::checkOwnModel(); // the own model must write every LM4 file as the program does
    try {
        Project project;project.mode="schematic";project.width=10000;project.height=8000;
        Canvas canvas;canvas.resize(900,720);canvas.show();canvas.setProject(&project);
        require(project.legacy.isEmpty(),"displaying a new project created legacy data");
        QApplication::processEvents();canvas.fit();
        int changes=0;canvas.changed=[&]{changes++;};
        canvas.setTool("ground");click(canvas,{2540,2540});
        require(project.additions.size()==1,"placing a symbol did not update the project");
        require(project.additions[0].toObject()["x"].toDouble()==2540,"symbol did not snap to the grid");
        canvas.setTool("wire");click(canvas,{2540,2286});click(canvas,{5080,2286});
        require(project.additions.size()==2,"drawing a wire did not update the project");
        auto wire=project.additions[1].toObject();
        require(wire["x"].toDouble()==2540&&wire["x2"].toDouble()==5080,"wire endpoints changed");
        canvas.setTool("select");click(canvas,{3810,2286});canvas.removeSelected();
        require(project.additions.size()==1,"deleting the selected wire failed");
        require(changes==3,"edit notifications were lost");
        QTemporaryDir tmp;auto path=tmp.filePath("edited.openloch");project.assignIds();project.save(path); // the window gives identifiers with every change
        auto restored=Project::load(path);canvas.setProject(&restored);
        require(restored.encode()==project.encode(),"saved drawing changed on reopen");
        auto png=tmp.filePath("drawing.png");require(canvas.exportImage(png),"PNG export failed");
        QImage image(png);require(!image.isNull()&&image.width()==2400,"PNG has incorrect dimensions");
        bool ink=false;for(int y=600;y<625;y++)for(int x=600;x<625;x++)ink|=image.pixelColor(x,y)!=Qt::white;
        require(ink,"exported drawing is blank at its symbol");
        auto pdf=tmp.filePath("drawing.pdf");require(canvas.exportPdf(pdf),"PDF export failed");
        QFile f(pdf);require(f.open(QIODevice::ReadOnly)&&f.read(5)=="%PDF-","PDF output is invalid");
        Project board;board.original=fixtures::project();board.sourceKind="lm4";board.sourceName="original.LM4";board.legacy=LegacyReader(board.original).read(true);
        History history;canvas.beforeChange=[&]{history.begin(board);};canvas.changed=[&]{history.commit(board);};canvas.setProject(&board);
        require(board.moves.isEmpty(),"displaying an imported board changed saved offsets");
        // Turning and mirroring keep the first terminal in place, as the original turns and mirrors about it.
        canvas.selectObject("legacy",0);canvas.editSelected({{"id","R1"},{"value","100 kΩ"}});const QPointF pinOne=canvas.selectionPivot();canvas.rotateSelected(90);canvas.mirrorSelected(true);
        // Mirroring on the board after a 90° turn reverses the angle (mirror flags act in the object's own frame).
        require(board.legacyNode(0)["angle"].toDouble()==-90&&board.legacyNode(0)["mirrorX"].toBool(),"imported component transform was lost");
        require(QLineF(canvas.selectionPivot(),pinOne).length()<.5,"turning and mirroring moved the first terminal");
        auto *originalItem=canvas.scene()->selectedItems().first();auto pivot=originalItem->mapToScene(componentAnchor(board.legacy["objects"].toArray()[0].toObject()));
        auto beforeMove=board.encode();const auto offset=board.moves["0"].toArray();drag(canvas,pivot,pivot+QPointF(508,0));
        const auto moved=board.moves["0"].toArray();
        require(moved[0].toDouble()-offset.at(0).toDouble()==508&&moved[1].toDouble()==offset.at(1).toDouble(),"moving an imported component did not snap its offset");
        require(history.undo(board)&&board.encode()==beforeMove,"undo did not restore the imported component position");canvas.setProject(&board);
        auto library=board.addLibrary(fixtures::library(),"Widerstand.lib");canvas.beginPlacement(library,0);click(canvas,{2540,2540});click(canvas,{5080,2540});
        require(board.additions.size()==2,"library components were not placed");
        require(board.componentNode(board.additions[0].toObject())["id"]=="R2"&&board.componentNode(board.additions[1].toObject())["id"]=="R3"&&board.additions[0].toObject()["id"]=="R#","repeated placement reused a component identifier");
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(&canvas,&escape);require(canvas.activeTool=="select","Escape did not end component placement");
        canvas.selectObject("new",0);canvas.editSelected({{"value","4,7 kΩ"}});const QPointF firstPin=canvas.selectionPivot();canvas.rotateSelected(-90);canvas.mirrorSelected(false);
        require(QLineF(canvas.selectionPivot(),firstPin).length()<.5,"rotating or editing a library component moved its first terminal");
        canvas.duplicateSelected();require(board.additions.size()==3&&board.componentNode(board.additions[2].toObject())["id"]=="R4","duplicating a component did not allocate a new identifier");
        require(board.additions[2].toObject()["value"]=="4,7 kΩ"&&board.additions[2].toObject()["mirrorY"].toBool(),"duplication lost edited component properties");
        canvas.removeSelected();require(board.additions.size()==2,"deleting a library component failed");
        require(history.undo(board)&&board.additions.size()==3,"undo did not restore library component");canvas.setProject(&board);
        canvas.selectObject("legacy",0);canvas.removeSelected();require(board.billOfMaterials().size()==3,"deleted imported component remains in the parts list");
        require(history.undo(board)&&board.billOfMaterials().size()==4,"undo did not restore imported component");canvas.setProject(&board);
        canvas.selectObject("legacy",0);canvas.duplicateSelected();require(board.additions.size()==4,"duplicating an imported component failed");
        auto copy=board.additions[3].toObject();require(copy["value"]=="100 kΩ"&&copy["angle"].toDouble()==-90&&copy["mirrorX"].toBool(),"duplicating an imported component lost edits");
        path=tmp.filePath("board.openloch");board.save(path);auto reopened=Project::load(path);require(reopened.encode()==board.encode(),"edited library components changed on reopen");
        require(reopened.original==fixtures::project(),"editing overwrote the imported original");canvas.setProject(&reopened);
        canvas.selectObject("new",0);
        require(canvas.exportImage(tmp.filePath("components.png")),"component image export failed");
        require(canvas.selectionCount()==1,"export cleared the editor selection");
        canvas.beginPlacement(library,0);
        const auto local=QPointF(canvas.mapFromScene(QPointF(7620,5080)));QMouseEvent move(QEvent::MouseMove,local,QPointF(canvas.viewport()->mapToGlobal(local.toPoint())),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(canvas.viewport(),&move);
        auto previewFile=tmp.filePath("preview.png");canvas.exportImage(previewFile);canvas.setTool("select");
        require(QImage(previewFile)==QImage(tmp.filePath("components.png")),"an unplaced preview appeared in the export");
        canvas.selectAll();require(canvas.selectionCount()==5,"select-all included the board or missed components");
        {auto linked=reopened.additions[0].toObject();linked["component"]=documents::newId();reopened.additions[0]=linked;}
        auto sourceState=reopened.encode();auto copied=canvas.selectionData();require(reopened.encode()==sourceState,"copy changed the source project");
        auto fragment=Project::decode(copied);require(fragment.additions.size()==5&&fragment.original.isEmpty(),"clipboard contains an incomplete selection");
        Project destination;Canvas pasted;pasted.setProject(&destination);History pasteHistory;
        pasted.beforeChange=[&]{pasteHistory.begin(destination);};pasted.changed=[&]{pasteHistory.commit(destination);};
        pasted.pasteData(copied);require(destination.additions.size()==5&&pasted.selectionCount()==5,"cross-project paste lost components");
        {QSet<QString> source;for(auto v:reopened.additions){const auto uid=v.toObject()["uid"].toString();require(uid.size()==32,"a copied object had no identifier");source.insert(uid);}
         for(auto v:destination.additions)require(!source.contains(v.toObject()["uid"].toString())&&!v.toObject().contains("component"),"a pasted copy kept the identifier or the component of its original");}
        require(destination.libraries.size()==fragment.libraries.size(),"cross-project paste lost source libraries");
        auto firstPosition=destination.additions[0].toObject()["x"].toDouble();pasted.pasteData(copied);
        require(destination.additions.size()==10&&destination.additions[5].toObject()["x"].toDouble()==firstPosition+254,"repeated paste did not advance its offset");
        QSet<QString> identifiers;for(auto v:destination.additions){const auto n=v.toObject();const auto id=n["type"]=="component"?destination.componentNode(n)["id"].toString():n["id"].toString();require(!identifiers.contains(id),"paste reused an identifier");identifiers.insert(id);}
        require(pasteHistory.undo(destination)&&destination.additions.size()==5,"paste was not a single undo step");pasted.setProject(&destination);pasted.selectAll();
        pasted.alignSelected("left");double left=0;bool first=true;for(auto *item:pasted.scene()->selectedItems()){
            if(first){left=item->sceneBoundingRect().left();first=false;}require(std::abs(item->sceneBoundingRect().left()-left)<.001,"alignment did not align transformed bounds");
        }
        // "Mitte senkrecht" (vertical) lines the centres up on one vertical line, "Mitte waagerecht" (horizontal) on one horizontal line.
        for(const QString edge:{"vertical","horizontal"}){pasted.selectAll();pasted.alignSelected(edge);double centre=0;first=true;
            for(auto *item:pasted.scene()->selectedItems()){const auto c=item->sceneBoundingRect().center();const double v=edge=="vertical"?c.x():c.y();if(first){centre=v;first=false;}require(std::abs(v-centre)<.001,"centre alignment used the wrong axis");}}
        auto beforeArrow=destination.additions[0].toObject();QKeyEvent arrow(QEvent::KeyPress,Qt::Key_Right,Qt::ShiftModifier);QApplication::sendEvent(&pasted,&arrow);
        require(destination.additions[0].toObject()["x"].toDouble()==beforeArrow["x"].toDouble()+2540,"keyboard movement did not use the selected grid");
        auto beforeInvalid=destination.encode();bool rejected=false;try{pasted.pasteData("invalid");}catch(const FormatError &){rejected=true;}
        require(rejected&&destination.encode()==beforeInvalid,"invalid clipboard partially changed the project");pasted.clearSelection();require(pasted.selectionCount()==0,"deselect failed");
        QTextStream(stdout)<<"Clipboard workflows passed"<<Qt::endl;
        Project drawing;drawing.width=2540;drawing.height=2540;Canvas shapes;shapes.resize(600,600);shapes.show();shapes.setProject(&drawing);QApplication::processEvents();shapes.fit();
        for(auto type:{"rectangle","ellipse"}){shapes.setTool(type);click(shapes,{254,254});click(shapes,{1016,762});}
        for(auto type:{"polygon","polyline"}){shapes.setTool(type);click(shapes,{1270,254});click(shapes,{1778,762});click(shapes,{1270,1016});shapes.finishContour();}
        for(auto type:{"drill","pin"}){shapes.setTool(type);click(shapes,{2032,2032});}
        require(drawing.additions.size()==6,"new contour, drilling or pin tool failed");
        shapes.selectObject("new",2);auto contour=shapes.selectedPath();contour[1]=QJsonArray{762,508};shapes.editPath(contour);shapes.editSelected({{"filled",true}});
        require(drawing.additions[2].toObject()["points"].toArray()[1].toArray()==QJsonArray{762,508},"editing a polygon node failed");
        { // outline buttons: rounding with a size, milling, smoothing off keeps style and size; the export carries them
            shapes.setTool("select");shapes.selectObject("new",2);shapes.setSmoothing(2,100);shapes.setMilling(true);
            auto n=drawing.additions[2].toObject();require(n["flag2"].toBool()&&n["style"]==2&&n["rotation"].toDouble()==100&&n["flag3"].toBool(),"outline rounding or milling was not set");
            const auto out=LegacyReader(writeLegacyProject(drawing)).read(true)["objects"].toArray();bool found=false;
            for(auto v:out){auto o=v.toObject();if(o["type"]=="TDraht"&&o["kind"]==6&&o["flag2"].toBool()&&o["style"]==2&&o["rotation"].toDouble()==100&&o["flag3"].toBool())found=true;}
            require(found,"smoothing and milling were not exported");
            shapes.setSmoothing(-1,0);n=drawing.additions[2].toObject();require(!n["flag2"].toBool()&&n["style"]==2&&n["rotation"].toDouble()==100,"switching smoothing off must keep style and size");
        }
        { // fill pictures: loaded onto an area over its grown bounding box, exported with board corners, turned 90° clockwise
            QImage picture(4,2,QImage::Format_RGB888);picture.fill(Qt::red);picture.setPixelColor(0,0,Qt::blue);QByteArray bmp;{QBuffer buffer(&bmp);buffer.open(QIODevice::WriteOnly);picture.save(&buffer,"BMP");}
            shapes.selectObject("new",2);shapes.setBitmapFill(bmp);auto n=drawing.additions[2].toObject();
            require(n.contains("bitmap")&&n["filled"].toBool()&&!n["flag3"].toBool()&&n["anchors"].toArray().size()==6,"fill picture was not loaded");
            const auto out=LegacyReader(writeLegacyProject(drawing)).read(true)["objects"].toArray();bool exported=false;
            for(auto v:out){auto o=v.toObject();if(o["type"]=="TDraht"&&o["kind"]==6&&o["bitmap_size"].toInteger()>0)exported=true;}require(exported,"fill picture was not exported");
            shapes.selectObject("new",2);shapes.rotateBitmapFill();const QImage turned=QImage::fromData(QByteArray::fromBase64(drawing.additions[2].toObject()["bitmap"].toString().toLatin1()),"BMP");
            require(turned.width()==2&&turned.height()==4&&turned.pixelColor(1,0)==QColor(Qt::blue),"fill picture was not turned 90° clockwise");
            // A PNG with a transparent background: the area takes the outline of the opaque part, also in the LM4.
            QImage cutout(200,100,QImage::Format_ARGB32);cutout.fill(Qt::transparent);{QPainter p(&cutout);p.setRenderHint(QPainter::Antialiasing);p.setPen(Qt::NoPen);p.setBrush(Qt::darkGreen);p.drawEllipse(QRectF(20,10,160,80));}
            const auto fill=pictureFill(cutout);shapes.selectObject("new",2);const auto anchors=drawing.additions[2].toObject()["anchors"].toArray();shapes.setBitmapFill(fill.bmp,fill.outline);n=drawing.additions[2].toObject();
            const QRectF box(QPointF(anchors[0].toDouble(),anchors[1].toDouble()),QPointF(anchors[2].toDouble(),anchors[5].toDouble()));QRectF shape;for(auto v:n["points"].toArray())shape|=QRectF(QPointF(v.toArray()[0].toDouble(),v.toArray()[1].toDouble()),QSizeF(.01,.01));
            require(n["type"]=="polygon"&&n["points"].toArray().size()>=8&&box.adjusted(-1,-1,1,1).contains(shape)&&shape.width()>box.width()*.7&&shape.width()<box.width()*.9,"the area did not take the outline of the opaque part of the picture");
            bool cut=false;for(auto v:LegacyReader(writeLegacyProject(drawing)).read(true)["objects"].toArray()){auto o=v.toObject();if(o["type"]=="TDraht"&&o["kind"]==6&&o["bitmap_size"].toInteger()>0&&o["path"].toArray().size()>=9)cut=true;}
            require(cut,"the outline of the picture was not exported");
        }
        { // "Ursprung setzen": the clicked grid point becomes the origin, the grid counts from it, the LM4 stores it
            shapes.setTool("origin");click(shapes,{508,508});require(drawing.origin()==QPointF(508,508)&&shapes.activeTool=="select","origin tool did not set the origin");
            Project shifted;shifted.mode="board";shifted.width=2540;shifted.height=2540;shifted.userOrigin=QJsonArray{127,127};
            const auto stored=LegacyReader(writeLegacyProject(shifted)).read(true);Project reread;reread.legacy=stored;require(reread.origin()==QPointF(127,127),"the LM4 did not store the origin");
            Canvas grid;grid.setProject(&shifted);grid.setTool("drill");grid.resize(400,400);grid.show();QApplication::processEvents();grid.fit();click(grid,{1290,1290});
            require(shifted.additions.size()==1&&shifted.additions[0].toObject()["x"].toDouble()==1397,"the grid does not count from the origin");
            drawing.userOrigin={};
        }
        { // points: insert halfway to the previous one, delete; zoom loupe; other board side
            shapes.setTool("select");shapes.selectObject("new",3);auto before=drawing.additions[3].toObject()["points"].toArray();
            shapes.selectObject("new",3);shapes.addNode(0);auto after=drawing.additions[3].toObject()["points"].toArray();
            const auto a=before[0].toArray(),b=before[1].toArray();
            require(after.size()==before.size()+1&&after[1].toArray()==QJsonArray{a[0].toDouble()+std::trunc((b[0].toDouble()-a[0].toDouble())/2),a[1].toDouble()+std::trunc((b[1].toDouble()-a[1].toDouble())/2)},"inserting an outline point failed");
            shapes.selectObject("new",3);shapes.deleteNode(1);require(drawing.additions[3].toObject()["points"].toArray()==before,"deleting an outline point failed");
            shapes.selectObject("new",3);const bool side=drawing.additions[3].toObject()["back"].toBool();shapes.switchSide();require(drawing.additions[3].toObject()["back"].toBool()!=side,"switching the board side failed");
            shapes.switchSide();const double zoom=shapes.transform().m11();shapes.setTool("zoom");click(shapes,{1270,1270});require(std::abs(shapes.transform().m11()/zoom-1.2)<1e-6,"zoom loupe did not zoom in");
            shapes.setTool("select");shapes.fit();
        }
        { // a handle of the selected outline drags that point, snapped to the grid
            shapes.setTool("select");shapes.selectObject("new",3);const auto handles=shapes.nodeHandles();require(handles.size()==3&&handles[1]==QPointF(1778,762),"outline handles missing");
            auto send=[&](QEvent::Type type,QPointF scene,Qt::MouseButtons buttons){const QPointF v=shapes.mapFromScene(scene);QMouseEvent event(type,v,QPointF(shapes.viewport()->mapToGlobal(v.toPoint())),Qt::LeftButton,buttons,Qt::NoModifier);QApplication::sendEvent(shapes.viewport(),&event);};
            send(QEvent::MouseButtonPress,handles[1],Qt::LeftButton);send(QEvent::MouseMove,{1790,260},Qt::LeftButton);send(QEvent::MouseButtonRelease,{1790,260},Qt::NoButton);
            require(drawing.additions[3].toObject()["points"].toArray()[1].toArray()==QJsonArray{508,0},"dragging an outline handle did not move that point");
        }
        shapes.selectObject("new",4);shapes.editSelected({{"diameter",.8}});Project::decode(drawing.encode());
        shapes.selectObject("new",0);shapes.reorderSelected(true);auto top=shapes.scene()->selectedItems().first();require(top->zValue()==6,"bring-to-front failed");
        auto exported=LegacyReader(writeLegacyProject(drawing)).read(true)["objects"].toArray();require(exported.last().toObject()["type"]=="TDraht","export lost layer order");
        require(exported[0].toObject()["type"]=="TKreis","ellipse export changed its class");
        require(exported[3].toObject()["diameter"].toDouble()==.8,"sub-millimetre drill diameter was lost");
        QTextStream(stdout)<<"Drawing tools and layer order passed"<<Qt::endl;
        drawing.assignIds();auto drawingState=drawing.encode();History groupHistory;shapes.beforeChange=[&]{groupHistory.begin(drawing);};shapes.changed=[&]{groupHistory.commit(drawing);};
        shapes.selectAll();shapes.groupSelected();require(drawing.additions.size()==1&&shapes.selectionCount()==1,"grouping failed");
        auto grouped=drawing.componentNode(drawing.additions[0].toObject());require(grouped["children"].toArray().size()==6,"grouping lost geometry");
        require(groupHistory.undo(drawing)&&drawing.encode()==drawingState,"grouping was not an atomic undo step");shapes.setProject(&drawing);shapes.selectAll();shapes.groupSelected();shapes.groupSelected(true);
        require(drawing.additions.size()==6&&shapes.selectionCount()==6,"ungrouping lost contours");
        require(Project::decode(drawing.encode()).additions.size()==6,"grouped geometry did not survive save/reopen");
        auto ownLibrary=writeLegacyDocument(drawing);auto ownBoard=tmp.filePath("own.LMB");drawing.save(ownBoard);
        require(LegacyReader(ownLibrary).read(false)["objects"].toArray().size()==6&&Project::load(ownBoard).legacy["objects"].toArray().size()==6,"library/template export lost edited contours");
        QTextStream(stdout)<<"Grouping and document export passed"<<Qt::endl;
        Project numbered;auto source=numbered.addLibrary(fixtures::library(),"own.lib");
        // "Neu nummerieren" as in the original: Kennungen with "#" in the order of the document (not by position), also inside
        // groups and parts; literal Kennungen stay
        numbered.additions={QJsonObject{{"type","component"},{"library",source},{"index",0},{"x",2540},{"y",508},{"id","R#"},{"group_value",7}},QJsonObject{{"type","component"},{"library",source},{"index",0},{"x",508},{"y",508},{"id","R#"},{"group_value",7}}};numbered.renumber();
        auto shown=[&](int i){return numbered.componentNode(numbered.additions[i].toObject())["id"].toString();};
        require(shown(0)=="R1"&&shown(1)=="R2"&&numbered.additions[0].toObject()["id"]=="R#","renumbering must follow the order of the document");
        Canvas assembly;assembly.setProject(&numbered);assembly.selectAll();assembly.groupSelected();
        require(numbered.billOfMaterials().size()==2&&numbered.nextId("R#")=="R3","selection grouping lost parts or reused a nested identifier");
        {auto twice=numbered;twice.renumber();QStringList ids;for(auto v:twice.billOfMaterials())ids<<v.toObject()["id"].toString();
         require(ids==QStringList({"R1","R2"})&&!twice.additions.last().toObject()["nested"].toObject().isEmpty(),"parts inside a group were not renumbered");
         Project literal;const auto key=literal.addLibrary(fixtures::library(),"own.lib");literal.additions={QJsonObject{{"type","component"},{"library",key},{"index",0},{"x",508},{"y",508},{"id","IC7"}}};
         literal.renumber();require(literal.componentNode(literal.additions[0].toObject())["id"]=="IC7","a literal Kennung was renumbered");}
        numbered=Project::decode(numbered.encode());require(numbered.billOfMaterials().size()==2,"grouped parts disappeared after reopening");
        QTextStream(stdout)<<"Renumbering and grouped parts passed"<<Qt::endl;
        QPrinter printer(QPrinter::HighResolution);printer.setOutputFormat(QPrinter::PdfFormat);printer.setOutputFileName(tmp.filePath("print.pdf"));printer.setPageSize(QPageSize(QPageSize::A4));
        require(shapes.printTo(printer),"actual-size print failed");QFile printed(printer.outputFileName());require(printed.open(QIODevice::ReadOnly)&&printed.readAll().contains("/MediaBox [0 0 595"),"print page is not A4");
        require(shapes.exportImage(tmp.filePath("drawing.bmp"))&&shapes.exportImage(tmp.filePath("drawing.jpg")),"BMP/JPEG export failed");
        Project views;views.mode="schematic";views.width=2000;views.height=1000;views.additions={QJsonObject{{"type","rectangle"},{"x",200},{"y",200},{"x2",400},{"y2",400},{"filled",true},{"color","#cc0000"},{"back",true}}};Canvas projection;projection.setProject(&views);projection.exportImage(tmp.filePath("front.png"));projection.setViewMode("back");projection.exportImage(tmp.filePath("back.png"));
        QImage front(tmp.filePath("front.png")),back(tmp.filePath("back.png"));require(front.pixelColor(360,360).red()>150&&front.pixelColor(360,360).green()<50,"front-side drawing missing");require(back.pixelColor(360,840).green()<50&&back.pixelColor(360,360)==Qt::white,"back-side export was not turned over top to bottom");
        projection.setViewMode("outline");projection.exportImage(tmp.filePath("outline.png"));require(QImage(tmp.filePath("outline.png")).pixelColor(360,360)==Qt::white,"outline view still filled contours");
        QTextStream(stdout)<<"Printing and views passed"<<Qt::endl;
        Project bridges;bridges.mode="board";bridges.width=2000;bridges.height=1000;
        bridges.additions={QJsonObject{{"type","wire"},{"x",200},{"y",200},{"x2",800},{"y2",200},{"width",35},{"color","#008000"},{"back",false}},QJsonObject{{"type","wire"},{"x",200},{"y",600},{"x2",800},{"y2",600},{"width",35},{"color","#0000ff"}}};
        Canvas bridgeView;bridgeView.setProject(&bridges);bridgeView.exportImage(tmp.filePath("bridges-front.png"));bridgeView.setViewMode("back");bridgeView.exportImage(tmp.filePath("bridges-back.png"));
        QImage bridgeFront(tmp.filePath("bridges-front.png")),bridgeBack(tmp.filePath("bridges-back.png"));
        require(bridgeFront.pixelColor(600,240).green()>100&&bridgeFront.pixelColor(600,240).red()<20,"top-side wire bridge is missing");
        // From the solder side the component-side bridge is x-rayed (darkened, not in its own colour), as in the original.
        const QColor seen=bridgeBack.pixelColor(780,960);const QColor beside=bridgeBack.pixelColor(780,870);require(!(seen.green()>100&&seen.red()<20)&&seen.lightness()<beside.lightness()-20,"top-side bridge was not x-rayed from the solder side");
        require(bridgeBack.pixelColor(600,480).blue()>200&&bridgeBack.pixelColor(600,480).red()<20,"older wire without an explicit side disappeared from the solder side");
        require(bridgeBack.pixelColor(240,960)!=Qt::white&&bridgeBack.pixelColor(960,960)!=Qt::white,"top-side bridge solder terminals disappeared");
        Project terminals;terminals.mode="schematic";auto terminalLibrary=terminals.addLibrary(fixtures::library(),"pins.lib");
        terminals.additions={QJsonObject{{"type","component"},{"library",terminalLibrary},{"index",0},{"x",2540},{"y",2540},{"angle",90},{"mirrorX",true},{"id","R1"}}};
        Canvas copper;copper.resize(700,600);copper.show();copper.setProject(&terminals);QApplication::processEvents();copper.setViewMode("back");
        const auto anchorPoint=componentAnchor(terminals.libraryNode(terminalLibrary,0));
        const QPointF terminal=objectTransform(terminals.additions[0].toObject(),anchorPoint).map(QPointF(762,1016))+QPointF(2540,2540)-anchorPoint;
        require(copper.connectionTargets().contains(terminal),"rotated mirrored component terminal is not available for wiring");
        copper.exportImage(tmp.filePath("terminals.png"));const QImage terminalImage(tmp.filePath("terminals.png"));
        const QPoint pixel(qRound(terminal.x()*terminalImage.width()/terminals.width),qRound((terminals.height-terminal.y())*terminalImage.height()/terminals.height));
        require(terminalImage.pixelColor(pixel).red()<100,"component foot is missing from the copper-side export");
        copper.setShowComponents(false);require(copper.connectionTargets().contains(terminal),"hiding front-side component bodies hid copper-side feet");
        copper.setGrid(0);copper.setTool("wire");click(copper,terminal+QPointF(12,8));click(copper,terminal+QPointF(900,500));
        const auto connected=terminals.additions.last().toObject();require(QLineF(QPointF(connected["x"].toDouble(),connected["y"].toDouble()),terminal).length()<.001,"copper-side wiring did not snap to the physical component foot");
        copper.setTool("select");copper.exportImage(tmp.filePath("connected.png"));const QImage connectedImage(tmp.filePath("connected.png"));
        const QPointF midpoint(connected["x"].toDouble()*.5+connected["x2"].toDouble()*.5,connected["y"].toDouble()*.5+connected["y2"].toDouble()*.5);
        const QPoint wirePixel(qRound(midpoint.x()*connectedImage.width()/terminals.width),qRound((terminals.height-midpoint.y())*connectedImage.height()/terminals.height));
        require(connectedImage.pixelColor(wirePixel)!=QColor(Qt::white),"a new copper-side wire disappeared from the export");
        Project placementBoard;placementBoard.width=2540;placementBoard.height=2540;
        const auto halfPitch=placementBoard.addLibrary(fixtures::halfPitchLibrary(),"half-pitch.lib");
        Canvas insertion;insertion.resize(650,650);insertion.show();insertion.setProject(&placementBoard);QApplication::processEvents();insertion.fit();
        insertion.beginPlacement(halfPitch,0);click(insertion,{1270,1270});
        auto onDefaultHoles=[&]{const auto points=insertion.connectionTargets();require(points.size()==2,"footprint terminals were lost");for(auto p:points)require(std::abs(std::remainder(p.x(),254))<.001&&std::abs(std::remainder(p.y(),254))<.001,"component feet do not coincide with default perfboard holes");};
        onDefaultHoles();insertion.setTool("select");insertion.rotateSelected(90);onDefaultHoles();insertion.mirrorSelected(true);onDefaultHoles();
        const auto alignedState=placementBoard.encode();const auto alignedPoints=insertion.connectionTargets();placementBoard=Project::decode(alignedState);insertion.setProject(&placementBoard);
        require(insertion.connectionTargets()==alignedPoints,"reopening moved aligned terminals");
        const auto exportedPins=connectionPoints(LegacyReader(writeLegacyProject(placementBoard)).read(true)["objects"].toArray().first().toObject());
        require(exportedPins==alignedPoints,"LM4 export moved aligned terminals");
        // Old projects keep their positions until the user requests alignment.
        placementBoard.additions[0]=QJsonObject{{"type","component"},{"library",halfPitch},{"index",0},{"x",1270},{"y",1270}};insertion.setProject(&placementBoard);
        const auto oldPin=insertion.connectionTargets().first();require(std::abs(std::remainder(oldPin.y(),254))==127,"half-pitch regression fixture is already aligned");
        placementBoard.additions.append(QJsonObject{{"type","wire"},{"x",oldPin.x()},{"y",oldPin.y()},{"x2",2032},{"y2",2032}});insertion.setProject(&placementBoard);
        History alignmentHistory;insertion.beforeChange=[&]{alignmentHistory.begin(placementBoard);};insertion.changed=[&]{alignmentHistory.commit(placementBoard);};
        placementBoard.assignIds();const auto oldState=placementBoard.encode();insertion.selectObject("new",0);insertion.alignSelectedToBoard();
        const auto repairedWire=placementBoard.additions[1].toObject();require(repairedWire["y"].toDouble()!=oldPin.y()&&std::remainder(repairedWire["y"].toDouble(),254)==0,"alignment detached a native wire from its terminal");
        require(Project::decode(placementBoard.encode()).encode()==placementBoard.encode(),"aligned components and attached wires cannot be saved and reopened");
        require(alignmentHistory.undo(placementBoard)&&placementBoard.encode()==oldState,"terminal alignment cannot be undone");
        insertion.beforeChange={};insertion.changed={};
        Project templateBoard;templateBoard.width=2540;templateBoard.height=2540;templateBoard.boardSource=templateBoard.addLibrary(fixtures::offsetBoard(),"offset.lmb","lmb");
        const auto templatePart=templateBoard.addLibrary(fixtures::halfPitchLibrary(),"half-pitch.lib");insertion.setProject(&templateBoard);insertion.beginPlacement(templatePart,0);click(insertion,{635,430});
        const auto holes=boardHolePoints(QJsonObject{{"children",templateBoard.libraries[templateBoard.boardSource].document["objects"]}});
        const QPointF templateOrigin(127,205);for(auto p:insertion.connectionTargets())require(holes.contains(p+templateOrigin),"placement ignored template holes or their origin");
        insertion.setViewMode("back");const QPointF unusedHole(635,430);insertion.setTool("wire");click(insertion,unusedHole);click(insertion,{635,900});
        const auto templateWire=templateBoard.additions.last().toObject();require(templateWire["x"].toDouble()==unusedHole.x()&&templateWire["y"].toDouble()==unusedHole.y(),"back-side wire missed an offset template hole");
        Project directTemplate;directTemplate.width=2540;directTemplate.height=2540;directTemplate.original=fixtures::offsetBoard();directTemplate.sourceKind="lmb";directTemplate.legacy=LegacyReader(directTemplate.original).read(false);
        const auto directPart=directTemplate.addLibrary(fixtures::halfPitchLibrary(),"half-pitch.lib");insertion.setProject(&directTemplate);insertion.beginPlacement(directPart,0);click(insertion,{635,430});
        for(auto p:insertion.connectionTargets())if(p.y()!=430)throw std::runtime_error("placement in an opened LMB ignored its editable holes");
        insertion.setGrid(0);insertion.beginPlacement(directPart,0);click(insertion,{1200,1200});
        require(std::abs(std::remainder(directTemplate.additions.last().toObject()["y"].toDouble(),254))>1,"free placement was forced onto the hole grid");
        QTextStream(stdout)<<"Physical board-hole placement and alignment passed"<<Qt::endl;
        // Potential markers (kind 18): a 2 mm ring with a 1 mm core in the pen colour, independent of the stored width.
        auto contact=[](int kind,int width,QJsonArray path){return QJsonObject{{"type","TDraht"},{"kind",kind},{"width",width},{"pen",qint64(0x8080ff)},{"brush",0},{"path",path}};};
        const QJsonArray dot{QJsonArray{1000,1000},QJsonArray{1000,1000}};
        for(int width:{200,35}){
            auto ring=renderNode(contact(18,width,dot),{},1);require(!ring.isNull(),"potential marker was not rendered");
            const QPoint centre(ring.width()/2,ring.height()/2);auto at=[&](int dx){return ring.pixelColor(centre+QPoint(dx,0));};
            require(at(0)==QColor(255,128,128),"potential marker core lost the pen colour");require(at(75)==Qt::white,"potential marker ring is not white at 0.75 mm");
            require(at(-75)==Qt::white&&at(106).alpha()==0&&at(-106).alpha()==0,"potential marker ring does not have a 2 mm diameter");
        }
        QJsonArray onePoint;onePoint.append(QJsonArray{1000,1000}); // QJsonArray{QJsonArray{…}} would copy on older compilers
        auto single=renderNode(contact(18,200,onePoint),{},1);
        for(int y=0;y<single.height();y++)for(int x=0;x<single.width();x++)require(single.pixelColor(x,y).alpha()==0,"a single-point wire must stay invisible like in LochMaster");
        auto solder=renderNode(contact(19,120,dot),{},1);const QPoint solderCentre(solder.width()/2,solder.height()/2);
        require(solder.pixelColor(solderCentre+QPoint(30,0)).alpha()==255&&solder.pixelColor(solderCentre+QPoint(66,0)).alpha()==0,"kind 19 contact radius must follow half the stored width");
        // Stripboard strips: the stored width lies on both sides of the centre line, ends are flat.
        QJsonObject strip{{"type","TLeiterbahn"},{"kind",16},{"width",100},{"path",QJsonArray{QJsonArray{1000,1000},QJsonArray{3000,1000}}}};
        auto stripImage=renderNode(strip,{},1);const QPointF origin(1000-100-10,1000-100-10);auto stripAt=[&](double x,double y){return stripImage.pixelColor((QPointF(x,y)-origin).toPoint());};
        require(stripAt(2000,1090).alpha()==255&&stripAt(2000,910).alpha()==255,"strip copper does not reach its stored width on both sides");
        require(stripAt(2000,1104).alpha()==0&&stripAt(990,1000).alpha()==0&&stripAt(3010,1000).alpha()==0,"strip copper is wider than stored or has rounded ends");
        // Pins (kind 11): a dot whose diameter is the stored width.
        auto pinDot=renderNode(contact(11,60,dot),{},1);const QPoint pinCentre(pinDot.width()/2,pinDot.height()/2);
        require(pinDot.pixelColor(pinCentre+QPoint(25,0))==QColor(255,128,128)&&pinDot.pixelColor(pinCentre+QPoint(34,0)).alpha()==0,"pin dot must have the stored width as diameter");
        auto nativePin=renderNode(QJsonObject{{"type","pin"},{"x",1000},{"y",1000},{"width",60},{"color","#ff8080"}},{},1);const QPoint nativeCentre(nativePin.width()/2,nativePin.height()/2);
        require(nativePin.pixelColor(nativeCentre+QPoint(25,0))==QColor(255,128,128)&&nativePin.pixelColor(nativeCentre+QPoint(34,0)).alpha()==0,"native pins must look like LochMaster pins");
        QTextStream(stdout)<<"Legacy potential markers, pins and strips passed"<<Qt::endl;
        // Continuity tool: marks the net in cyan without touching the project or its history.
        Project traced;traced.width=2540;traced.height=2540;traced.additions.append(QJsonObject{{"type","wire"},{"x",254},{"y",254},{"x2",762},{"y2",254}});
        Canvas tester;tester.resize(600,600);tester.show();tester.setProject(&traced);QApplication::processEvents();tester.fit();
        const auto tracedState=traced.encode();int edits=0,copperFound=-1;tester.beforeChange=[&]{edits++;};tester.changed=[&]{edits++;};tester.continuityTraced=[&](int copper,int){copperFound=copper;};
        const int sceneItems=tester.scene()->items().size();tester.setTool("continuity");click(tester,{254,254});
        require(copperFound==2&&tester.scene()->items().size()==sceneItems+3,"continuity tool did not mark the net");
        require(traced.encode()==tracedState&&edits==0,"continuity tool changed the project");
        tester.setViewMode("back");require(tester.scene()->items().size()==sceneItems+3,"continuity marks were lost when switching the view");
        tester.setTool("select");require(tester.scene()->items().size()==sceneItems,"continuity marks survived a tool change");
        QTextStream(stdout)<<"Continuity tool passed"<<Qt::endl;
        // Potentials: a marker placed with the tool colours its net once potentials are shown; showing them changes nothing.
        tester.potentialRequested=[](QString &name,QColor &colour){name="VCC";colour=QColor("#ff0000");return true;};
        int conflicts=-1;tester.potentialsComputed=[&](int n){conflicts=n;};tester.beforeChange={};tester.changed={};
        tester.setViewMode("front");tester.setTool("potential");click(tester,{254,254});
        require(traced.additions.size()==2&&traced.additions[1].toObject()["type"]=="potential"&&traced.additions[1].toObject()["color"]=="#ff0000"&&traced.additions[1].toObject()["name"]=="VCC","potential tool did not place a named marker");
        const auto markedState=traced.encode();const int plainItems=tester.scene()->items().size();tester.setShowPotentials(true);
        require(conflicts==0&&tester.scene()->items().size()==plainItems+3&&traced.encode()==markedState,"showing potentials must colour the net without changing the project");
        auto colouredModel=continuityModel(traced).potentials();int red=0;for(const auto &c:colouredModel.coloured)red+=c.potential==QColor("#ff0000");
        require(red==3,"marker net must contain both pads and the wire");
        tester.setShowPotentials(false);require(tester.scene()->items().size()==plainItems,"hiding potentials left coloured marks");
        QTemporaryDir markerDir;traced.save(markerDir.filePath("marker.openloch"));require(Project::load(markerDir.filePath("marker.openloch")).additions[1].toObject()["name"]=="VCC","potential marker did not survive saving");
        const auto beforeFree=traced.encode();tester.setShowFreeAreas(true);
        require(tester.scene()->items().size()==plainItems+3&&traced.encode()==beforeFree,"free areas must be shown without changing the project");
        auto spareCopper=continuityModel(traced).freeCopper();bool padFree=false,usedFree=false;for(const auto &c:spareCopper){padFree|=c.shape.contains(QPointF(1016,1016));usedFree|=c.shape.contains(QPointF(254,254));}
        require(padFree&&!usedFree,"unused pads must be free and pads under a wire end occupied");
        tester.setShowFreeAreas(false);require(tester.scene()->items().size()==plainItems,"free areas stayed visible after release");
        // Short check: a GND marker on the VCC net is a short; its chain is shown in red without changing the project.
        Project shortedBoard=traced;shortedBoard.additions.append(QJsonObject{{"type","potential"},{"x",762},{"y",254},{"color","#0000ff"},{"name","GND"}});
        const auto shortsFound=continuityModel(shortedBoard).shorts();require(shortsFound.size()==1&&shortsFound[0].chain.size()==3,"short between VCC and GND not found");
        tester.setProject(&shortedBoard);QApplication::processEvents();const auto shortState=shortedBoard.encode();const int unmarked=tester.scene()->items().size();
        tester.showShort(shortsFound[0].chain);require(tester.markedShortItems()==3&&tester.scene()->items().size()==unmarked+3&&shortedBoard.encode()==shortState,"short chain was not highlighted or changed the project");
        tester.setViewMode("back");require(tester.markedShortItems()==3,"short highlight lost when switching the view");
        tester.clearShort();require(tester.scene()->items().size()==unmarked,"short highlight not removed");
        // Lead and solder tools create LochMaster-like objects on the side being edited.
        Project toolBoard;toolBoard.width=2540;toolBoard.height=2540;tester.setViewMode("front");tester.setProject(&toolBoard);QApplication::processEvents();tester.fit();
        tester.setTool("lead");click(tester,{254,254});click(tester,{254,762});tester.finishContour();
        tester.setViewMode("back");tester.setTool("solder");click(tester,{1016,1016});
        require(toolBoard.additions.size()==2&&toolBoard.additions[0].toObject()["type"]=="lead"&&!toolBoard.additions[0].toObject()["back"].toBool()&&toolBoard.additions[0].toObject()["points"].toArray().size()==2,"lead tool did not create a lead");
        require(toolBoard.additions[1].toObject()["type"]=="solder"&&toolBoard.additions[1].toObject()["back"].toBool()&&toolBoard.additions[1].toObject()["width"].toInt()==150,"solder tool did not create a copper-side blob");
        QTextStream(stdout)<<"Potentials, free areas, short check and electrical tools passed"<<Qt::endl;
        { // legacy drawing as in the original: labels turn counter-clockwise (270° runs downwards), filled labels sit on their brush colour, outlines of kind 7 close
            auto label=legacyLabel("MMMM",QPointF(1500,1000),300);label["text_height"]=3*M_PI/2;label["transparent"]=true;label["brush"]=0x00FFFF;label["pen"]=0;
            auto outline=legacyObject("TDraht",7,40);outline["pen"]=0xFF0000;outline["path"]=QJsonArray{QJsonArray{200,2200},QJsonArray{1200,2200},QJsonArray{1200,2800}};
            const auto bytes=writeLegacyObjects({label,outline},"Darstellung");const auto objects=LegacyReader(bytes).read(false)["objects"].toArray();
            Project drawing;drawing.mode="board";drawing.width=3000;drawing.height=3000;const auto key=drawing.addLibrary(bytes,"Darstellung.lib");
            for(int i=0;i<objects.size();i++){const auto anchor=componentAnchor(objects[i].toObject());drawing.additions.append(QJsonObject{{"type","component"},{"library",key},{"index",i},{"x",anchor.x()},{"y",anchor.y()}});}
            Canvas view;view.setProject(&drawing);const auto file=tmp.filePath("legacy-drawing.png");require(view.exportImage(file),"legacy drawing export failed");const QImage image(file);
            const double scale=image.width()/3000.0;auto at=[&](double x,double y){return image.pixelColor(qRound(x*scale),qRound(y*scale));};
            const QColor yellow(255,255,0);
            require(at(1500-300,1010)==yellow&&at(1500+300,990)!=yellow,"a label at 270° must run downwards with its box to the left");
            require(at(700,2500).blue()>200&&at(700,2500).red()<80,"an outline of kind 7 must be drawn closed");
            // Milled outlines are grey and filled like the original's, an LM4 outline as well as one drawn in OpenLoch.
            auto milledOutline=legacyObject("TDraht",6,40);milledOutline["pen"]=0xFF0000;milledOutline["flag3"]=true;
            milledOutline["path"]=QJsonArray{QJsonArray{1800,2200},QJsonArray{2800,2200},QJsonArray{2800,2800},QJsonArray{1800,2800},QJsonArray{1800,2200}};
            const auto milledBytes=writeLegacyObjects({milledOutline},"Fräsen");const auto milledKey=drawing.addLibrary(milledBytes,"Fräsen.lib");const auto milledAnchor=componentAnchor(LegacyReader(milledBytes).read(false)["objects"].toArray()[0].toObject());
            drawing.additions.append(QJsonObject{{"type","component"},{"library",milledKey},{"index",0},{"x",milledAnchor.x()},{"y",milledAnchor.y()}});
            drawing.additions.append(QJsonObject{{"type","rectangle"},{"x",1800},{"y",200},{"x2",2800},{"y2",700},{"color","#0000ff"},{"width",40},{"filled",false},{"flag3",true}});
            view.setProject(&drawing);require(view.exportImage(file),"milled drawing export failed");const QImage milled(file);auto grey=[&](double x,double y){const QColor c=milled.pixelColor(qRound(x*scale),qRound(y*scale));return std::abs(c.red()-128)<3&&std::abs(c.green()-128)<3&&std::abs(c.blue()-128)<3;};
            require(grey(2300,2500)&&grey(2300,450),"milled outlines must be filled grey like the original's");
        }
        { // Side, outline buttons, milling and fill pictures on the objects of an LM4 file survive saving, undo and redo;
          // a circle takes them into its inner outline
            Project source;source.mode="board";source.width=3000;source.height=3000;
            source.additions.append(QJsonObject{{"type","polygon"},{"x",500},{"y",500},{"points",QJsonArray{QJsonArray{0,0},QJsonArray{1000,0},QJsonArray{1000,800}}},{"color","#28624d"},{"width",25},{"filled",true}});
            source.additions.append(QJsonObject{{"type","ellipse"},{"x",500},{"y",1800},{"x2",1500},{"y2",2600},{"color","#28624d"},{"width",25},{"filled",false}});
            const auto file=tmp.filePath("legacy-edits.LM4");{QFile f(file);require(f.open(QIODevice::WriteOnly)&&f.write(writeLegacyProject(source))>0,"writing the LM4 fixture failed");}
            auto lm4=Project::load(file);Canvas edit;edit.setProject(&lm4);History steps;edit.beforeChange=[&]{steps.begin(lm4);};edit.changed=[&]{steps.commit(lm4);};
            QImage picture(4,2,QImage::Format_RGB888);picture.fill(Qt::red);QByteArray bmp;{QBuffer buffer(&bmp);buffer.open(QIODevice::WriteOnly);picture.save(&buffer,"BMP");}
            edit.selectObject("legacy",0);edit.switchSide();edit.setSmoothing(1,50);edit.setMilling(true);edit.setBitmapFill(bmp);
            edit.selectObject("legacy",1);edit.setSmoothing(2,70);edit.setMilling(true);
            const auto saved=Project::decode(lm4.encode());require(saved.edits.size()==2,"edits of LM4 objects did not survive reopening");
            const auto out=LegacyReader(writeLegacyProject(lm4)).read(true)["objects"].toArray();const auto outline=out[0].toObject(),circle=out[1].toObject();
            require(outline["back"].toBool()&&outline["flag2"].toBool()&&outline["style"]==1&&outline["rotation"].toDouble()==50&&!outline["flag3"].toBool()&&outline["bitmap_size"].toInteger()>0,"side, outline or picture of an LM4 object were not saved");
            require(circle["type"]=="TKreis"&&circle["flag3"].toBool()&&circle["inner"].toObject()["flag3"].toBool()&&circle["inner"].toObject()["rotation"].toDouble()==70,"a circle did not take milling and size into its inner outline");
            require(steps.undo(lm4)&&steps.redo(lm4)&&lm4.edits.size()==2,"redo of edited LM4 objects failed");
            // A duplicate takes the side, outline, milling and picture along, also into the file.
            edit.setProject(&lm4);edit.selectObject("legacy",0);edit.duplicateSelected();const auto copy=lm4.componentNode(lm4.additions.last().toObject());
            require(copy["back"].toBool()&&copy["flag2"].toBool()&&copy["style"]==1&&copy["rotation"].toDouble()==50&&copy.contains("bitmap"),"a duplicate lost the side, outline or picture");
            const auto copied=LegacyReader(writeLegacyProject(lm4)).read(true)["objects"].toArray().last().toObject();
            require(copied["flag2"].toBool()&&copied["style"]==1&&copied["bitmap_size"].toInteger()>0,"the duplicate's outline or picture was not saved");
            lm4.additions.removeLast();
            // The Breite, Farben and Füllen toolbars on an LM4 circle reach its inner outline; a fill colour replaces the picture.
            edit.setProject(&lm4);edit.selectObject("legacy",1);edit.applyStyle(2,true);edit.applyStyle(3,QColor(Qt::yellow));edit.applyStyle(0,0);
            const auto styled=LegacyReader(writeLegacyProject(lm4)).read(true)["objects"].toArray()[1].toObject();const auto inner=styled["inner"].toObject();
            require(styled["width"]==0&&inner["width"]==0&&inner["transparent"].toBool()&&inner["brush"].toInteger()==0x00ffff,"style changes did not reach a circle's inner outline");
            const QJsonValue circleAnchors=lm4.legacyNode(1).value("anchors");edit.selectObject("legacy",1);edit.setBitmapFill(bmp);
            { // as the original writes it: the same picture on the inner outline with its corners and on the circle itself
                const auto written=writeLegacyProject(lm4);const auto ring=LegacyReader(written).read(true)["objects"].toArray()[1].toObject();const auto inner=ring["inner"].toObject();
                auto bytesOf=[&](const QJsonObject &n){return written.mid(n["bitmap_offset"].toInteger(),n["bitmap_size"].toInteger());};
                require(inner["bitmap_size"].toInteger()>0&&bytesOf(ring)==bytesOf(inner)&&ring["anchors"]==circleAnchors&&inner["anchors"]!=circleAnchors,"a circle's fill picture is not written like the original's");
                // a copy of the circle keeps the picture's corners
                edit.selectObject("legacy",1);edit.duplicateSelected();const auto copy=lm4.componentNode(lm4.additions.last().toObject());
                const auto corners=lm4.legacyNode(1).value("inner").toObject().value("anchors");
                require(corners!=circleAnchors&&copy.value("inner").toObject().value("anchors")==corners&&copy.value("anchors")==circleAnchors,"a copied circle lost its picture's corners");
                lm4.additions.removeLast();
            }
            edit.selectObject("legacy",0);edit.applyStyle(3,QColor(Qt::blue));require(!lm4.legacyNode(0).contains("bitmap")&&LegacyReader(writeLegacyProject(lm4)).read(true)["objects"].toArray()[0].toObject()["bitmap_size"].toInteger()==0,"a fill colour did not replace the picture");
        }
        { // Wenden turns the board over top to bottom like the original; texts move with it but stay upright (an "L" keeps its foot down)
            auto label=legacyLabel("L",QPointF(1000,500),400);label["pen"]=0;const auto bytes=writeLegacyObjects({label},"Text");const auto objects=LegacyReader(bytes).read(false)["objects"].toArray();
            Project page;page.mode="schematic";page.width=2000;page.height=2000;const auto key=page.addLibrary(bytes,"Text.lib");const auto anchor=componentAnchor(objects[0].toObject());
            page.additions.append(QJsonObject{{"type","component"},{"library",key},{"index",0},{"x",anchor.x()},{"y",anchor.y()}});Canvas turned;turned.setProject(&page);
            auto glyph=[&](const QString &name,QRect &box){const QImage image=QImage(tmp.filePath(name)).convertToFormat(QImage::Format_Grayscale8);box={};
                for(int y=0;y<image.height();y++){const uchar *line=image.constScanLine(y);for(int x=0;x<image.width();x++)if(line[x]<128)box|=QRect(x,y,1,1);}return image;};
            turned.exportImage(tmp.filePath("text-front.png"));turned.setViewMode("back");turned.exportImage(tmp.filePath("text-back.png"));QRect front,back;
            const QImage frontImage=glyph("text-front.png",front),backImage=glyph("text-back.png",back);
            // The same glyph upright in both views: the cut-outs match as they are, not when one is turned upside down. Without
            // fonts (the offscreen platform on Windows) glyphs are plain boxes and only the position can be checked.
            const QImage a=frontImage.copy(front),b=backImage.copy(back).scaled(a.size());qint64 same=0,turnedOver=0;
            for(int y=0;y<a.height();y++)for(int x=0;x<a.width();x++){same+=std::abs(a.constScanLine(y)[x]-b.constScanLine(y)[x]);turnedOver+=std::abs(a.constScanLine(y)[x]-b.constScanLine(a.height()-1-y)[x]);}
            if(!(front.center().y()<frontImage.height()/2&&back.center().y()>backImage.height()/2&&(same<turnedOver||turnedOver==0)))
                throw std::runtime_error(QString("Wenden must move texts top to bottom and keep them upright (front %1,%2 %3×%4, back %5,%6 %7×%8, image height %9, difference %10 against %11)")
                    .arg(front.x()).arg(front.y()).arg(front.width()).arg(front.height()).arg(back.x()).arg(back.y()).arg(back.width()).arg(back.height()).arg(frontImage.height()).arg(same).arg(turnedOver).toStdString());
        }
        { // Snapping as in the original: the unit's grid from the origin, Shift places exactly
            Project grid;grid.mode="board";grid.width=5000;grid.height=5000;Canvas snapped;snapped.resize(600,600);snapped.show();snapped.setProject(&grid);QApplication::processEvents();snapped.resetTransform(); // one pixel per 1/100 mm
            auto press=[&](QPointF at,Qt::KeyboardModifiers modifiers){const QPointF local=snapped.mapFromScene(at);const QPointF global=snapped.viewport()->mapToGlobal(local.toPoint());
                QMouseEvent down(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,modifiers),up(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,modifiers);
                QApplication::sendEvent(snapped.viewport(),&down);QApplication::sendEvent(snapped.viewport(),&up);};
            snapped.setTool("rectangle");press({1013,1007},Qt::NoModifier);press({2013,2007},Qt::NoModifier);auto box=grid.additions.last().toObject();
            require(box["x"].toDouble()==1016&&box["y"].toDouble()==1016,"the unit N did not snap to the hole pitch");
            snapped.setUnit(0);press({1013,1007},Qt::NoModifier);press({2013,2007},Qt::NoModifier);box=grid.additions.last().toObject();
            require(box["x"].toDouble()==1010&&box["y"].toDouble()==1010,"the unit mm did not snap to 0.1 mm");
            press({1013,1007},Qt::ShiftModifier);press({2013,2007},Qt::ShiftModifier);box=grid.additions.last().toObject();
            require(std::abs(box["x"].toDouble()-1013)<1&&std::abs(box["y"].toDouble()-1007)<1,"Shift must place without snapping");
        }
        { // Width 0 ("unsichtbar") draws no line in colour, 0.1 mm in S/W; areas fill with their own fill colour; wires never fill
            Project styled;styled.mode="board";styled.width=2000;styled.height=2000;
            styled.additions.append(QJsonObject{{"type","polyline"},{"x",200},{"y",1000},{"points",QJsonArray{QJsonArray{0,0},QJsonArray{1600,0}}},{"color","#ff0000"},{"width",0}});
            styled.additions.append(QJsonObject{{"type","rectangle"},{"x",200},{"y",200},{"x2",1800},{"y2",700},{"color","#000000"},{"width",10},{"fill","#0000ff"},{"filled",true}});
            styled.additions.append(QJsonObject{{"type","wire"},{"x",200},{"y",1500},{"x2",1800},{"y2",1500},{"color","#c0c0c0"},{"width",40}});
            Canvas view;view.setProject(&styled);const auto file=tmp.filePath("styled.png");require(view.exportImage(file),"style export failed");QImage image(file);
            const double scale=image.width()/2000.0;auto at=[&](double x,double y){return image.pixelColor(qRound(x*scale),qRound(y*scale));};
            require(at(1000,1000).red()<200||at(1000,1000).green()>100,"a line of width 0 is visible");require(at(1000,450)==QColor(Qt::blue),"an area is not filled with its fill colour");
            view.selectObject("new",2);view.applyStyle(2,true);require(!styled.additions[2].toObject()["filled"].toBool(),"a wire must never be filled");
            // Solder joints: a shaded tin dome with BMP-Rendering, a white disc without, as in the original.
            styled.additions.append(QJsonObject{{"type","solder"},{"x",1000},{"y",1700},{"width",150},{"color","#c0c0c0"}});view.rebuild();
            require(view.exportImage(file),"solder export failed");QImage shaded(file);auto state=view.viewState();state.bitmaps=false;view.setViewState(state);require(view.exportImage(file),"solder export failed");QImage plain(file);
            const QColor dome=shaded.pixelColor(qRound(1000*scale),qRound(1730*scale)),disc=plain.pixelColor(qRound(1000*scale),qRound(1730*scale));
            require(disc==QColor(Qt::white)&&dome!=QColor(Qt::white)&&dome.lightness()>150&&std::abs(dome.red()-dome.blue())<12,"a solder joint is not shaded with BMP-Rendering");
            const auto out=LegacyReader(writeLegacyProject(styled)).read(true)["objects"].toArray();require(out[1].toObject()["brush"].toInteger()==0xff0000&&out[1].toObject()["pen"].toInteger()==0,"the fill colour was not saved as the brush");
        }
        if(argc==3&&QString::fromLocal8Bit(argv[1])=="--assets"){
            QString root=QString::fromLocal8Bit(argv[2]);int count=0,lm4Files=0,unwritable=0;
            for(const auto &dir:{root+"/drive_c/users/Public/Documents/LochMaster40",root+"/drive_c/ProgramData/LochMaster40/DE/LIB"}){
                QDirIterator it(dir,QDir::Files,QDirIterator::Subdirectories);while(it.hasNext()){
                    auto path=it.next();if(!QStringList{"lm4","lmb","lib"}.contains(QFileInfo(path).suffix().toLower()))continue;
                    auto imported=Project::load(path);auto state=imported.encode();canvas.setProject(&imported);
                    require(imported.encode()==state,"displaying a corpus file changed the project");
                    require(Project::decode(imported.encode()).original==imported.original,"corpus project lost original bytes on reopen");count++;
                    // The own model: its format keeps everything; LM4 is written as the program writes it (the check of writeLegacyProject).
                    const auto model=BoardDocument::fromProject(imported);require(BoardDocument::decode(model.encode()).encode()==model.encode(),"the own format changed a corpus file");
                    try{const auto lm4=writeLegacyProject(imported);if(imported.sourceKind=="lm4"&&imported.boards.isEmpty())require(lm4==imported.original,"an unchanged corpus file was not written as read");lm4Files++;}catch(const FormatError &){unwritable++;}
                }
            }
            require(count>0,"no corpus files found");QTextStream(stdout)<<count<<" corpus files displayed and reopened without mutation\n";
            QTextStream(stdout)<<lm4Files<<" corpus files written as LM4 from the own model as by the program, "<<unwritable<<" not writable as LM4\n";
        }
        { // formatted notes: RichEdit's RTF into a document and back
            const QByteArray rtf("{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1031{\\fonttbl{\\f0\\fnil Arial;}{\\f1\\fnil\\fcharset2 Symbol;}}\r\n{\\colortbl ;\\red255\\green0\\blue0;}\r\n"
                "\\viewkind4\\uc1\\pard\\qc\\b\\f0\\fs40 Titel\\par\r\n\\pard\\b0\\fs20 Gr\\'f6\\'dfe \\i kursiv\\i0  \\ul unter\\ulnone  \\cf1 rot\\cf0\\tab \\u937?\\par\r\n"
                "\\pard{\\pntext\\f1\\'B7\\tab}{\\*\\pn\\pnlvlblt\\pnf1\\pnindent0{\\pntxtb\\'B7}}\\fi-200\\li200 Punkt\\par\r\n}\r\n");
            QTextDocument document;readRtf(rtf+QByteArray(1,'\0'),document);
            auto check=[](const QTextDocument &d,const char *message){
                require(d.blockCount()==3&&d.toPlainText()==QString::fromUtf8("Titel\nGröße kursiv unter rot\tΩ\nPunkt"),message);
                const auto title=d.begin();const auto line=title.next();QList<QTextCharFormat> parts;for(auto it=line.begin();!it.atEnd();++it)parts.append(it.fragment().charFormat());
                require(title.blockFormat().alignment()&Qt::AlignHCenter&&title.begin().fragment().charFormat().fontWeight()==QFont::Bold&&title.begin().fragment().charFormat().fontPointSize()==20,message);
                bool italic=false,underline=false,red=false;for(const auto &f:parts){italic|=f.fontItalic();underline|=f.fontUnderline();red|=f.foreground().color()==QColor(255,0,0);}
                require(italic&&underline&&red&&parts.first().fontPointSize()==10&&parts.first().fontWeight()==QFont::Normal&&d.lastBlock().textList()&&!line.textList(),message);
            };
            check(document,"RTF notes were read wrongly");
            const auto written=writeRtf(document);QTextDocument again;readRtf(written,again);check(again,"RTF notes did not survive writing");
            require(written.startsWith("{\\rtf1\\ansi\\ansicpg1252")&&written.endsWith(QByteArray("\\par\r\n}\r\n",9)+QByteArray(1,'\0'))&&written.contains("\\'f6"),"RTF notes are not written like RichEdit");
            QTextDocument empty;readRtf("{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1031{\\fonttbl{\\f0\\fnil Tahoma;}}\r\n\\viewkind4\\uc1\\pard\\f0\\fs16\\par\r\n}\r\n",empty);
            require(empty.blockCount()==1&&empty.toPlainText().isEmpty(),"the original's empty notes are not one empty paragraph");
        }
        { // "Stückliste einfügen" / "Einkaufsliste einfügen": order R, C, D, T, others; units; wire bridges in holes
            Project p;p.title="Testplatine";
            auto part=[](const char *id,int number,const char *value,const char *description,QJsonArray children={}){
                if(children.isEmpty()){QJsonArray pin;pin.append(QJsonArray{0,0});children={QJsonObject{{"type","TDraht"},{"kind",11},{"path",pin}}};}
                return QJsonObject{{"id",id},{"group_value",number},{"value",value},{"description",description},{"group_flags",QJsonArray{true,true}},{"children",children}};
            };
            p.additions={part("R#",2,"10k","Widerstand"),part("R#",1,"10k","Widerstand"),part("X#",1,"","Modul",{part("R#",3,"10k","Widerstand")}),part("C#",1,"100n","Kondensator"),
                part("T#",1,"BC547","Transistor"),part("",0,"1 mm","Lötnagel"),QJsonObject{{"type","wire"},{"x",254},{"y",254},{"x2",1016},{"y2",254}},QJsonObject{{"type","wire"},{"x",254},{"y",254},{"x2",300},{"y2",254}}};
            const QDateTime when(QDate(2026,10,8),QTime(16,29,41));
            auto texts=[&](int mode){QStringList out;for(const auto &l:partsListNotes(p,mode,"Test.LM4",when,"Prüfer"))out.append(l.text);return out;};
            const auto list=partsListNotes(p,0,"Test.LM4",when,"Prüfer");
            require(list.first().text=="Stückliste für Testplatine"&&list.first().bold&&list.first().size==20&&list[2].text=="Projekt: Test.LM4"&&list[3].text=="Erstellt am 08.10.2026 um 16:29:41"&&list[4].text=="von Prüfer","parts list header differs from the original's");
            const QStringList parts{"R1\tWiderstand, 10k","R2\tWiderstand, 10k","C1\tKondensator, 100n","T1\tTransistor, BC547","-\tLötnagel, 1 mm","",">>> X1 - Modul <<<","\tR3\tWiderstand, 10k",
                "","Drahtbrücken:","","(1/1)\t(4/1)\tDraht; (L=7,62 mm Lochabstand 3)"};
            require(texts(0).mid(6)==parts,"parts list differs from the original's");
            const QStringList order{"R1,R2,R3","3x\tWiderstand, 10k","","C1","1x\tKondensator, 100n","","T1","1x\tTransistor, BC547","","1x\tLötnagel, 1 mm","",
                "","Drahtbrücken:","","(1/1)\t(4/1)\tDraht; (L=7,62 mm Lochabstand 3)"};
            require(texts(1).first()=="Einkaufsliste für Testplatine"&&texts(1).mid(6)==order,"order list differs from the original's");
        }
        QTextStream(stdout)<<"Editor placement, component editing, transforms, duplication, deletion, undo, save/reopen and export passed\n";return 0;
    }catch(const std::exception &e){QTextStream(stderr)<<e.what()<<"\n";return 1;}
}
