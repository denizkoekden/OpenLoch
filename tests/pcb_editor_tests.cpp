// Editor tests of the PCB module: tools, selection, undo and files, driven through the widgets offscreen.
#include "language.h"
#include "modules/pcb/editor.h"
#include "modules/pcb/example.h"
#include "modules/pcb/font.h"
#include "modules/pcb/footprints.h"
#include "formats/sprint/sprint.h"
#include <QAbstractButton>
#include <QAction>
#include <QCheckBox>
#include <QPushButton>
#include <QApplication>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QTreeWidget>
#include <QTemporaryDir>
#include <cmath>
#include <stdexcept>

using namespace openloch;
using namespace openloch::pcb;
namespace {
void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
void send(BoardView *view,QEvent::Type type,QPointF mm,Qt::MouseButton button=Qt::LeftButton,Qt::KeyboardModifiers modifiers=Qt::NoModifier){
    const QPointF at=view->toPixel(mm);const auto buttons=type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::MouseButtons(button);
    QMouseEvent event(type,at,view->mapToGlobal(at),type==QEvent::MouseMove?Qt::NoButton:button,type==QEvent::MouseMove?(button==Qt::NoButton?Qt::NoButton:buttons):buttons,modifiers);
    QApplication::sendEvent(view,&event);
}
void click(BoardView *view,QPointF mm,Qt::MouseButton button=Qt::LeftButton,Qt::KeyboardModifiers modifiers=Qt::NoModifier){
    send(view,QEvent::MouseMove,mm,Qt::NoButton);send(view,QEvent::MouseButtonPress,mm,button,modifiers);send(view,QEvent::MouseButtonRelease,mm,button,modifiers);
}
void dragTo(BoardView *view,QPointF from,QPointF to){
    send(view,QEvent::MouseMove,from,Qt::NoButton);send(view,QEvent::MouseButtonPress,from);
    for(int i=1;i<=4;i++)send(view,QEvent::MouseMove,from+(to-from)*i/4.0,Qt::LeftButton);send(view,QEvent::MouseButtonRelease,to);
}
bool near(QPointF a,QPointF b){return std::abs(a.x()-b.x())<1e-6&&std::abs(a.y()-b.y())<1e-6;}
// Answers the next message box with `button`.
void answer(QMessageBox::StandardButton button){
    auto *timer=new QTimer;timer->setInterval(5);
    QObject::connect(timer,&QTimer::timeout,[timer,button]{
        if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){timer->stop();timer->deleteLater();box->button(button)->click();}});
    timer->start();
}
// Fills in the next modal dialog with `fill` and accepts it.
void inDialog(const std::function<void(QDialog*)> &fill){
    auto *timer=new QTimer;timer->setInterval(5);
    QObject::connect(timer,&QTimer::timeout,[timer,fill]{
        if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){timer->stop();timer->deleteLater();fill(dialog);dialog->accept();}});
    timer->start();
}
}

int editorTests(){
    Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();
    auto *view=editor.view();
    // A fresh document: one empty board of 160 × 100 mm.
    require(editor.document().boards.size()==1&&editor.document().board().elements.isEmpty()&&!editor.isModified(),"new document");
    // The grid counts from the origin; with the origin in the top left corner the grid points are multiples of 1.27 mm.
    Document blank;blank.boards={newBoard("Test",50,40)};blank.boards[0].grid=1.27;blank.boards[0].origin={0,0};editor.setDocument(blank);QApplication::processEvents();view->fitBoard();
    auto elements=[&]()->const QList<Element>&{return editor.document().board().elements;};

    // Track: two clicks and a right click; nodes snap to the 1.27 mm grid, width and layer from the settings.
    view->setTool(BoardView::Tool::Track);view->trackWidth=.5;
    click(view,{10.1,10.0});click(view,{20.4,10.2});click(view,{20.4,10.2},Qt::RightButton);
    require(elements().size()==1&&elements()[0].type==ElementType::Track&&elements()[0].points.size()==2,"track tool");
    require(near(elements()[0].points[0],{10.16,10.16})&&near(elements()[0].points[1],{20.32,10.16})&&elements()[0].width==.5&&elements()[0].layer==CopperBottom,"track nodes and settings");
    require(editor.isModified()&&editor.canUndo(),"a new element is an undo step");
    // Bend mode 3: horizontal then vertical, so a diagonal click gives a corner.
    view->bendMode=3;click(view,{5.08,30.48});click(view,{12.7,35.56});click(view,{12.7,35.56},Qt::RightButton);
    require(elements().size()==2&&elements()[1].points.size()==3&&near(elements()[1].points[1],{12.7,30.48}),"bend mode corner");
    view->bendMode=0;

    // Pad and SMD pad tools.
    view->setTool(BoardView::Tool::Pad);view->padShape=PadShape::Octagon;view->padDiameter=2;view->padDrill=1;click(view,{30.48,20.32});
    require(elements().size()==3&&elements()[2].type==ElementType::Pad&&elements()[2].shape==PadShape::Octagon&&elements()[2].points.size()==8&&near(elements()[2].pos,{30.48,20.32}),"pad tool");
    // SMD pads go to the active copper side, here the top.
    view->board().activeLayer=CopperTop;view->setTool(BoardView::Tool::Smd);click(view,{40.64,20.32});view->board().activeLayer=CopperBottom;
    require(elements().size()==4&&elements()[3].type==ElementType::SmdPad&&elements()[3].layer==CopperTop,"SMD pads go to the active copper side");

    // Circle and rectangle by dragging, area by clicks.
    view->setTool(BoardView::Tool::Circle);dragTo(view,{25.4,30.48},{27.94,30.48});
    require(elements().size()==5&&elements()[4].type==ElementType::Circle&&std::abs(elements()[4].size-2.54)<1e-9,"circle tool");
    view->setTool(BoardView::Tool::Rectangle);dragTo(view,{33.02,27.94},{38.1,33.02});
    require(elements().size()==6&&elements()[5].type==ElementType::Track&&elements()[5].points.size()==5,"rectangle tool");
    view->setTool(BoardView::Tool::Area);for(QPointF p:{QPointF(2.54,2.54),QPointF(7.62,2.54),QPointF(7.62,7.62)})click(view,p);click(view,{7.62,7.62},Qt::RightButton);
    require(elements().size()==7&&elements()[6].type==ElementType::Area&&elements()[6].points.size()==3,"area tool");

    // Text tool: the editor asks for the text; here a stand-in answers.
    view->setTool(BoardView::Tool::Text);view->textRequested=[](Element &e){e.text="R1 10k";return true;};click(view,{2.54,38.1});
    require(elements().size()==8&&elements()[7].type==ElementType::Text&&elements()[7].text=="R1 10k"&&!elements()[7].strokes.isEmpty(),"text tool");

    // Select and move the pad by dragging: snapped, and one undo step puts it back.
    view->setTool(BoardView::Tool::Select);
    dragTo(view,{30.48,20.32},{33.02,22.86});
    require(view->selection()==QList<int>{2}&&near(elements()[2].pos,{33.02,22.86})&&near(elements()[2].points[0],{33.02-1,22.86+.585-1+.0}),"moving a pad moves its outline");
    editor.undo();require(near(elements()[2].pos,{30.48,20.32}),"undo puts the moved pad back");
    editor.redo();require(near(elements()[2].pos,{33.02,22.86}),"redo moves it again");

    // Rotating 90° clockwise, deleting and undoing.
    view->setSelection({3});editor.rotateSelection(-90);require(std::abs(elements()[3].rotation-270)<1e-9,"rotated SMD pad");
    const auto before=elements().size();view->setSelection({3});editor.deleteSelection();require(elements().size()==before-1,"delete");
    editor.undo();require(elements().size()==before,"undo of delete");

    // The reference's keys: mode keys, arrows by one grid step, with Ctrl by a tenth of it.
    auto key=[&](int k,Qt::KeyboardModifiers modifiers=Qt::NoModifier){QKeyEvent press(QEvent::KeyPress,k,modifiers);QApplication::sendEvent(view,&press);};
    key(Qt::Key_L);require(view->tool()==BoardView::Tool::Track,"L: track tool");key(Qt::Key_O);require(view->tool()==BoardView::Tool::SolderMask,"O: solder mask tool");
    key(Qt::Key_Escape);require(view->tool()==BoardView::Tool::Select,"Esc: standard tool");
    // Mode keys can be chosen: the old key does nothing then.
    view->modeKeys["track"]=Qt::Key_K;view->modeKeys["solderMask"]=0;key(Qt::Key_L);require(view->tool()==BoardView::Tool::Select,"L no longer chooses the track tool");
    key(Qt::Key_K);require(view->tool()==BoardView::Tool::Track,"K: track tool");key(Qt::Key_O);require(view->tool()==BoardView::Tool::Track,"a mode without key");
    view->modeKeys=BoardView::defaultModeKeys();key(Qt::Key_Escape);
    require(BoardView::modes().size()==16&&BoardView::modeName("photo")=="Fotoansicht","modes of the key settings");
    // Keys 1 to 9: the grids of the list (this board keeps 1.27 mm afterwards, as before).
    key(Qt::Key_1);require(std::abs(editor.document().board().grid-2.54)<1e-9,"key 1: first grid of the list");
    // Grids chosen for the keys: one not in the list stands in the grid field as text.
    editor.setGridKeys({2.54,1.27,.635,.3175,1.0,.5,.25,.1,.2});key(Qt::Key_9);require(std::abs(editor.document().board().grid-.2)<1e-9,"key 9: a chosen grid");
    editor.setGridKeys({});require(editor.gridKeys()==Editor::defaultGridKeys(),"default grids of the keys");
    key(Qt::Key_2);require(std::abs(editor.document().board().grid-1.27)<1e-9,"key 2: second grid");
    {const QPointF at=elements()[2].pos;view->setSelection({2});key(Qt::Key_Right);require(near(elements()[2].pos,at+QPointF(1.27,0)),"arrow: one grid step");
        key(Qt::Key_Left,Qt::ControlModifier);require(near(elements()[2].pos,at+QPointF(1.27-.127,0)),"Ctrl+arrow: a tenth of the grid");editor.undo();editor.undo();
        require(near(elements()[2].pos,at),"arrow moves undone");}
    // Rotating by the chosen angle, mirroring top to bottom, aligning edges and to the grid.
    {editor.setRotationStep(45);view->setSelection({3});const double r=elements()[3].rotation;editor.action("rotate")->trigger();
        require(std::abs(std::fmod(elements()[3].rotation-r+720,360)-315)<1e-9,"rotated 45° clockwise");editor.undo();editor.setRotationStep(90);
        const auto bend=elements()[1].points;view->setSelection({1});editor.mirrorSelectionVertically();const double axis=bend[0].y()+bend[2].y();
        require(near(elements()[1].points[0],{bend[0].x(),axis-bend[0].y()})&&near(elements()[1].points[2],{bend[2].x(),axis-bend[2].y()}),"mirrored top to bottom");editor.undo();
        view->setSelection({2,3});editor.alignSelection(0);require(std::abs(bounds(elements()[3]).left()-bounds(elements()[2]).left())<1e-9,"aligned left");editor.undo();
        const QPointF at=elements()[2].pos;editor.editElements({2},[](Element &e){pcb::move(e,{.3,-.2});});view->setSelection({2});editor.alignToGrid();
        require(near(elements()[2].pos,at),"back on the grid");editor.undo();editor.undo();}

    // Groups: clicking one member selects all, Alt picks the member alone; ungrouping undoes it.
    view->setSelection({2,3});editor.groupSelection();view->setSelection({});click(view,elements()[3].pos);
    require(view->selection()==QList<int>({2,3}),"a click selects the whole group");
    view->setSelection({});click(view,elements()[3].pos,Qt::LeftButton,Qt::AltModifier);require(view->selection()==QList<int>{3},"Alt picks one member of a group");
    view->setSelection({2,3});editor.ungroupSelection();view->setSelection({});click(view,elements()[3].pos);require(view->selection()==QList<int>{3},"ungrouped");

    // Copy and paste: the copy follows the pointer until a click puts it down.
    view->setSelection({2});editor.copySelection();editor.pasteClipboard();require(view->placing(),"pasted elements are placed with the pointer");
    click(view,{10.16,25.4});require(elements().size()==before+1&&near(elements().last().pos,{10.16,25.4}),"pasted pad");

    // A footprint: a component with a numbered designator; a second one gets the next number.
    const auto all=footprints();Footprint dil8;for(const auto &f:all)if(f.id=="dil-8")dil8=f;
    require(dil8.elements.size()>=10,"DIL-8 footprint");
    view->beginPlacement(placeable(dil8,editor.document().board()));click(view,{15.24,20.32});
    view->beginPlacement(placeable(dil8,editor.document().board()));click(view,{35.56,20.32});
    QStringList designators;for(const auto &e:elements())if(e.role==TextRole::Designator)designators<<e.text;
    require(designators==QStringList({"IC1","IC2"}),"designators numbered on placement");
    // The component list: a row per component; picking a row marks the component, marking it on the board picks the row.
    {QApplication::processEvents();auto *list=editor.componentList();
        require(list->topLevelItemCount()==2&&list->topLevelItem(0)->text(1)=="IC1"&&list->topLevelItem(1)->text(1)=="IC2","component list");
        require(list->isColumnHidden(0)&&list->topLevelItem(1)->text(0)=="2","the running number, hidden at first as in the reference");
        editor.findChild<QCheckBox*>("componentColumn-0")->setChecked(true);require(!list->isColumnHidden(0),"shown by its switch");
        editor.findChild<QPushButton*>("fitColumns")->click();require(list->columnWidth(1)>0,"columns fitted to their contents");
        list->topLevelItem(1)->setSelected(true);require(view->selection()==components(editor.document().board())[1].members,"a row marks its component");
        view->setSelection(components(editor.document().board())[0].members);
        require(list->topLevelItem(0)->isSelected()&&!list->topLevelItem(1)->isSelected(),"a marked component picks its row");view->setSelection({});}

    // Files: own format and Sprint-Layout 6 give the same elements back.
    QTemporaryDir dir;QString error;
    require(editor.saveFile(dir.filePath("probe.olpcb"),&error)&&!editor.isModified(),"save");
    const Document saved=editor.document();
    require(editor.openFile(dir.filePath("probe.olpcb"),&error),"open own format");
    require(editor.document()==saved&&!editor.canUndo(),"open own format gives the saved document");
    require(editor.exportSprint(dir.filePath("probe.lay6"),&error),"export");
    // Texts without strokes (empty ones) are not written: the reference deletes them when it reads the file.
    int writable=0;for(auto e:saved.board().elements){if(e.type==ElementType::Text&&e.strokes.isEmpty())updateStrokes(e);writable+=e.type!=ElementType::Text||!e.strokes.isEmpty();}
    require(editor.openFile(dir.filePath("probe.lay6"),&error)&&editor.filePath().isEmpty()&&editor.document().board().elements.size()==writable,"open exported Sprint-Layout file");
    require(editor.displayName()=="probe.lay6","imported documents are named after their file");
    // A file older than Sprint-Layout 4.0 (version 0: board size, one pad) opens converted, without asking, as an
    // imported document; what the conversion did stands in a bar above the board until another file is opened.
    {QFile old(dir.filePath("alt.lay"));require(old.open(QIODevice::WriteOnly),"cannot write the old file");
        old.write(QByteArray::fromHex("0033aaff70170000a00f0000010002e80318fc640028000000000101")+QByteArray(35,'\0'));old.close();
        auto *bar=editor.findChild<QWidget*>("noticeBar");
        require(editor.openFile(dir.filePath("alt.lay"),&error)&&editor.filePath().isEmpty()&&editor.document().board().elements.size()==1,"open a version 0 file");
        require(editor.openNotes().size()==1&&bar&&bar->isVisible(),"the notes of an old file are shown");
        require(editor.openFile(dir.filePath("probe.lay6"),&error)&&editor.openNotes().isEmpty()&&!bar->isVisible(),"a newer file has no notes");}
    // Pictures: exactly the working area at the chosen resolution, in colour or black on white.
    {const auto &b=editor.document().board();require(editor.exportImage(dir.filePath("board.png"),254,true,&error),"picture export");QImage picture(dir.filePath("board.png"));
        require(picture.width()==int(std::lround(b.width*10))&&picture.height()==int(std::lround(b.height*10))&&std::abs(picture.dotsPerMeterX()-10000)<=1,"picture size and resolution");
        require(editor.exportImage(dir.filePath("board.bmp"),254,false,&error),"black and white picture");QImage mono(dir.filePath("board.bmp"));
        int black=0;for(int y=0;y<mono.height();y+=2)for(int x=0;x<mono.width();x+=2)black+=mono.pixelColor(x,y)==QColor(Qt::black);
        require(mono.width()==picture.width()&&black>0&&black<mono.width()*mono.height()/8,"black elements on white");}

    // Airwires, test, solder mask and keep-out tools.
    {Document small;small.boards={newBoard("Werkzeuge",40,30)};small.boards[0].grid=1.27;editor.setDocument(small);QApplication::processEvents();view->fitBoard();
        view->setTool(BoardView::Tool::Pad);view->padShape=PadShape::Round;view->padDiameter=1.8;view->padDrill=.8;click(view,{5.08,5.08});click(view,{15.24,5.08});
        view->setTool(BoardView::Tool::Airwire);click(view,{5.08,5.08});click(view,{15.24,5.08});click(view,{15.24,5.08},Qt::RightButton);
        require(elements()[0].connections==QList<int>{1}&&elements()[1].connections==QList<int>{0}&&view->airwires().size()==1,"airwire tool");
        view->setTool(BoardView::Tool::Track);view->trackWidth=.5;click(view,{5.08,5.08});click(view,{15.24,5.08});click(view,{15.24,5.08},Qt::RightButton);
        require(editor.removeRoutedAirwires()==1&&view->airwires().isEmpty(),"routed airwires are removed");
        editor.undo();require(view->airwires().size()==1,"undo brings the airwire back");
        view->setTool(BoardView::Tool::Airwire);click(view,{10.16,5.08});require(view->airwires().isEmpty(),"a click on an airwire removes it");
        view->setTool(BoardView::Tool::Test);click(view,{10.16,5.08});require(view->tested().size()==3,"the test tool finds both pads and the track");
        view->setTool(BoardView::Tool::SolderMask);click(view,{10.16,5.08});require(elements()[2].solderMask,"the solder mask tool opens the mask over a track");
        editor.resetSolderMask();require(!elements()[2].solderMask&&elements()[0].solderMask,"solder mask reset to the pads");
        view->setTool(BoardView::Tool::Keepout);for(QPointF p:{QPointF(25.4,15.24),QPointF(35.56,15.24),QPointF(35.56,25.4)})click(view,p);click(view,{35.56,25.4},Qt::RightButton);
        require(elements().last().type==ElementType::Area&&elements().last().cutout,"keep-out tool");
        editor.setGroundPlane(true);require(editor.document().board().groundPlane[CopperBottom],"AutoMasse on the active layer");
        // Seen from below, the board is mirrored on the screen but clicks still hit the same element.
        view->setTool(BoardView::Tool::Select);view->setFromBelow(true);require(near(view->toBoard(view->toPixel({5.08,5.08})),{5.08,5.08}),"mirrored view maps back");
        require(view->toPixel({5,5}).x()>view->toPixel({15,5}).x(),"seen from below, left and right swap");
        view->setSelection({});click(view,{15.24,5.85});require(view->selection()==QList<int>{1},"a click from below selects the pad under the pointer");view->setFromBelow(false);
        // Design rule check from the panel: the two pads are 10.16 mm apart, nothing to report at first.
        require(editor.runDesignRuleCheck().isEmpty(),"no findings on a clean board");}

    // Rubber band: a track with a node on a moved pad follows it; automatic snapping catches a pad's centre off the grid.
    {Document d;Board b=newBoard("Gummiband",40,30);b.grid=1.27;
        auto pad=newElement(ElementType::Pad);pad.pos={10.16,10.16};pad.size=2;pad.size2=.8;updateOutline(pad);b.elements<<pad;
        auto onCentre=newElement(ElementType::Track);onCentre.points={{10.16,10.16},{20.32,10.16}};b.elements<<onCentre;
        auto offCentre=newElement(ElementType::Track);offCentre.points={{10.16,10.66},{10.16,20.32}};b.elements<<offCentre;
        auto offGrid=newElement(ElementType::Pad);offGrid.pos={30.3,20.3};updateOutline(offGrid);b.elements<<offGrid;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();view->setTool(BoardView::Tool::Select);
        require(view->hit({10.16,10.16})==0,"the pad wins over the tracks ending on it");
        view->rubberBand=2;view->setSelection({0});dragTo(view,{10.16,10.16},{12.7,10.16});
        require(near(elements()[0].pos,{12.7,10.16})&&near(elements()[1].points[0],{12.7,10.16})&&near(elements()[2].points[0],{12.7,10.66}),"large catch: tracks on the pad follow it");
        require(near(elements()[1].points[1],{20.32,10.16}),"the far end of a track stays");
        editor.undo();require(near(elements()[1].points[0],{10.16,10.16})&&near(elements()[2].points[0],{10.16,10.66}),"one undo step for pad and tracks");
        view->rubberBand=1;view->setSelection({0});dragTo(view,{10.16,10.16},{12.7,10.16});
        require(near(elements()[1].points[0],{12.7,10.16})&&near(elements()[2].points[0],{10.16,10.66}),"small catch: only nodes on the centre follow");
        editor.undo();view->rubberBand=0;view->setSelection({0});QKeyEvent right(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);QApplication::sendEvent(view,&right);
        require(near(elements()[0].pos,{11.43,10.16})&&near(elements()[1].points[0],{10.16,10.16}),"rubber band off: tracks stay");editor.undo();view->rubberBand=2;
        view->setTool(BoardView::Tool::Track);click(view,{30.4,20.4});click(view,{35.56,20.32});click(view,{35.56,20.32},Qt::RightButton);
        require(elements().size()==5&&near(elements()[4].points[0],{30.3,20.3}),"the track starts on the pad's centre");
        // The origin: new boards count from the bottom left corner; the key 0 puts it under the pointer, one undo step.
        require(near(editor.document().board().origin,{0,30}),"new boards count from the bottom left corner");
        send(view,QEvent::MouseMove,{10.16,10.16},Qt::NoButton);{QKeyEvent zero(QEvent::KeyPress,Qt::Key_0,Qt::NoModifier);QApplication::sendEvent(view,&zero);}
        require(near(editor.document().board().origin,{10.16,10.16})&&editor.canUndo(),"key 0 sets the origin");editor.undo();
        view->autoSnap=false;click(view,{30.4,20.4});click(view,{35.56,25.4});click(view,{35.56,25.4},Qt::RightButton);
        // The grid counts from the origin in the bottom left corner (30 mm down): 30 - 8 × 1.27 = 19.84.
        require(near(elements()[5].points[0],{30.48,19.84}),"without automatic snapping the grid catches, counted from the origin");view->autoSnap=true;view->setTool(BoardView::Tool::Select);}

    // A component without groups, as Sprint-Layout keeps them: a click marks all of it, Alt one element; grouping and
    // ungrouping leave it whole, and aligning moves it as one.
    {Document d;Board b=newBoard("Bauteil",40,30);b.grid=1.27;
        auto pad=newElement(ElementType::Pad);pad.pos={10.16,10.16};pad.part=4;updateOutline(pad);auto second=pad;second.pos={15.24,10.16};updateOutline(second);
        auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="R1";id.pos={10,7};id.part=4;updateStrokes(id);
        auto other=newElement(ElementType::Pad);other.pos={25.4,20.32};updateOutline(other);b.elements<<pad<<second<<id<<other;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();view->setTool(BoardView::Tool::Select);
        click(view,{15.24,10.16});require(view->selection()==QList<int>({0,1,2}),"a click marks the whole component");
        view->setSelection({});click(view,{15.24,10.16},Qt::LeftButton,Qt::AltModifier);require(view->selection()==QList<int>{1},"Alt marks one element of it");
        view->setSelection({0,1,2,3});editor.groupSelection();editor.ungroupSelection();
        require(components(editor.document().board()).size()==1&&components(editor.document().board())[0].members==QList<int>({0,1,2}),"grouping and ungrouping leave the component");
        const QPointF apart=elements()[2].pos-elements()[0].pos;view->setSelection({0,1,2,3});editor.alignSelection(3);
        require(near(elements()[2].pos-elements()[0].pos,apart)&&near(elements()[1].pos-elements()[0].pos,{5.08,0})&&std::abs(elements()[0].pos.y()-10.16)>1,"the component aligned as one");}

    // Tiles and a circle of copies: each a component of its own with the next designator, one undo step each.
    {Document d;Board b=newBoard("Kacheln",60,60);b.grid=1.27;
        auto pad=newElement(ElementType::Pad);pad.pos={30,30};pad.size=2;pad.size2=.8;updateOutline(pad);pad.part=1;b.elements<<pad;
        auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="D1";id.pos={29,27};id.layer=SilkTop;id.part=1;updateStrokes(id);b.elements<<id;
        d.boards={b};editor.setDocument(d);view->setSelection({0,1});
        editor.tileSelection(3,2,{5,7});require(elements().size()==12,"3 × 2 tiles");
        require(near(elements()[2].pos,{35,30})&&near(elements()[10].pos,{40,37}),"tile positions");
        require(elements()[2].part!=elements()[0].part&&elements()[2].part==elements()[3].part&&elements()[3].text=="D2"&&elements()[11].text=="D6"&&components(editor.document().board()).size()==6,
                "tiles are components of their own");
        editor.undo();require(elements().size()==2,"tiles undone");
        // As in the reference: the circle's centre lies the radius below the start point, a positive angle runs clockwise.
        view->setSelection({0});editor.arrangeInCircle(4,90,10,true);
        require(elements().size()==5&&near(elements()[2].pos,{40,40})&&near(elements()[3].pos,{30,50})&&near(elements()[4].pos,{20,40}),"four around a circle");
        editor.undo();view->setSelection({0});editor.arrangeInCircle(3,-90,5,false,{0,5});
        // The reference point 5 mm below the pad runs counter-clockwise around (30, 40); the copies keep their offset to it.
        require(near(elements()[2].pos,{25,35})&&near(elements()[3].pos,{30,40}),"circle through a moved start point, counter-clockwise");editor.undo();}

    // Autoroute mode: a click on an airwire lays a track on the active layer, a click on that track brings the airwire back.
    {Document d;Board b=newBoard("Router",30,20);b.grid=1.27;b.activeLayer=CopperBottom;
        auto a=newElement(ElementType::Pad);a.pos={5.08,10.16};a.connections={1};updateOutline(a);auto z=a;z.pos={25.4,10.16};z.connections={0};updateOutline(z);b.elements<<a<<z;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();view->setTool(BoardView::Tool::Autoroute);
        click(view,{15.24,10.16});
        require(elements().size()==3&&elements()[2].autorouted&&elements()[0].connections.isEmpty()&&view->airwires().isEmpty(),"autoroute lays the track");
        require(elements()[2].autoroutePads==std::array<int,2>{0,1},"the autorouted track keeps the pads of its airwire");
        // Only in the autoroute tool a light stripe runs along its middle (0.8 mm wide, the stripe a quarter of it), and
        // under the pointer the track shows selected.
        // (The cross hair would run through the pixels looked at.)
        {view->crosshair.lines=false;auto shown=[&](QPointF mm){const QImage image=view->grab().toImage();return image.pixelColor((view->toPixel(mm)*image.devicePixelRatio()).toPoint());};
            const QPointF middle(15.24,10.16),side(15.24,10.16+.3);send(view,QEvent::MouseMove,{2,2},Qt::NoButton);
            view->setTool(BoardView::Tool::Select);const QColor plain=shown(side);require(shown(middle)==plain,"no stripe outside the autoroute tool");
            view->setTool(BoardView::Tool::Autoroute);require(shown(middle)==QColor(215,215,215)&&shown(side)==plain,"a light stripe along an autorouted track");
            send(view,QEvent::MouseMove,middle,Qt::NoButton);require(shown(middle)==QColor(215,215,215)&&shown(side).lightness()>plain.lightness(),"an autorouted track under the pointer shows selected");
            view->crosshair.lines=true;}
        click(view,{15.24,10.16});require(elements().size()==2&&view->airwires().size()==1,"a click on the autoroute brings the airwire back");
        // An airwire under the pointer comes first: the click lays a track, it does not take the autoroute back.
        click(view,{15.24,10.16});editor.editElements({0},[](Element &e){e.connections={1};});editor.editElements({1},[](Element &e){e.connections={0};});
        click(view,{15.24,10.16});require(elements().size()>=3&&elements()[2].autorouted&&elements()[2].autoroutePads==std::array<int,2>{0,1},"the airwire under the pointer before the autoroute");
        // Undoing edits of elements gives the elements back as they were.
        while(elements().size()>3)editor.undo();editor.undo();editor.undo();require(elements().size()==3&&view->airwires().isEmpty(),"airwire and second track undone");
        // Without its pads (unknown, or one deleted) the track only loses its mark.
        editor.editElements({2},[](Element &e){e.autoroutePads={-1,-1};});click(view,{15.24,10.16});
        require(elements().size()==3&&!elements()[2].autorouted&&view->airwires().isEmpty(),"an autoroute without pads becomes an ordinary track");
        editor.undo();editor.undo();view->setTool(BoardView::Tool::Select);view->setSelection({0});editor.deleteSelection();
        require(elements().size()==2&&elements()[1].autoroutePads==std::array<int,2>{-1,0},"a deleted pad leaves the autoroute without it");
        view->setTool(BoardView::Tool::Autoroute);click(view,{15.24,10.16});require(elements().size()==2&&!elements()[1].autorouted,"then a click only takes the mark away");
        editor.undo();editor.undo();require(elements().size()==3&&elements()[2].autorouted&&elements()[2].autoroutePads==std::array<int,2>{0,1},"undone");
        // Copies keep the pads only together with them; the mark stays.
        view->setTool(BoardView::Tool::Select);view->setSelection({2});editor.copySelection();editor.pasteClipboard();click(view,{15.24,15.24});
        require(elements().size()==4&&elements()[3].autorouted&&elements()[3].autoroutePads==std::array<int,2>{-1,-1},"a copied autoroute without its pads");
        view->setSelection({0,1,2});editor.copySelection();editor.pasteClipboard();click(view,{15.24,5.08});
        require(elements().size()==7&&elements()[6].autoroutePads==std::array<int,2>{4,5},"a copied autoroute with its pads");
        view->setTool(BoardView::Tool::Select);}

    // Autoroute along the schematic's airwires: no stored airwire is used up, and taking the route back leaves none, as
    // the schematic shows its line again.
    {Document d;Board b=newBoard("Schaltplan",30,20);b.grid=1.27;b.activeLayer=CopperBottom;
        auto a=newElement(ElementType::Pad);a.pos={5.08,10.16};updateOutline(a);auto z=a;z.pos={25.4,10.16};updateOutline(z);b.elements<<a<<z;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();view->setTool(BoardView::Tool::Autoroute);
        const auto keep=view->schematicConnects;view->schematicConnects=[](int one,int two){return std::min(one,two)==0&&std::max(one,two)==1;};
        view->setSchematicAirwires({{0,1}});click(view,{15.24,10.16});
        require(elements().size()==3&&elements()[2].autorouted&&elements()[2].autoroutePads==std::array<int,2>{0,1}&&elements()[0].connections.isEmpty(),"a schematic airwire routed");
        click(view,{15.24,10.16});
        require(elements().size()==2&&elements()[0].connections.isEmpty()&&elements()[1].connections.isEmpty(),"taken back without a stored airwire");
        view->schematicConnects=keep;view->setTool(BoardView::Tool::Select);}
    // Preferences: a double click takes a pad's sizes, designators stay readable after turning, holes can be white.
    {Document d;Board b=newBoard("Optionen",30,20);auto pad=newElement(ElementType::Pad);pad.pos={10.16,10.16};pad.size=2.4;pad.size2=1.1;pad.shape=PadShape::Octagon;updateOutline(pad);pad.groups={1};
        auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="R5";id.pos={8,7};id.layer=SilkTop;id.groups={1};updateStrokes(id);b.elements<<pad<<id;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();view->setTool(BoardView::Tool::Select);
        send(view,QEvent::MouseButtonDblClick,{10.16,10.16});
        require(std::abs(view->padDiameter-2.4)<1e-9&&std::abs(view->padDrill-1.1)<1e-9&&view->padShape==PadShape::Octagon,"a double click takes the pad's sizes");
        view->setSelection({0,1});editor.rotateSelection(180);require(std::abs(elements()[1].rotation)<1e-9||std::abs(elements()[1].rotation-360)<1e-9,"the designator stays readable");
        view->holes=1;const QImage holes=view->renderBoard(10);view->holes=0;
        require(holes.pixelColor(QPoint(int(std::lround(elements()[0].pos.x()*10)),int(std::lround(elements()[0].pos.y()*10))))==QColor(Qt::white),"white drill holes");}

    // The selector lists the groups; a click on one marks its elements.
    {QApplication::processEvents();auto *tree=editor.selector();require(tree->topLevelItemCount()>0,"selector groups");
        auto *first=tree->topLevelItem(0);QList<int> expected;for(const auto &v:first->data(0,Qt::UserRole).toList())expected.append(v.toInt());
        const QRect at=tree->visualItemRect(first);QMouseEvent press(QEvent::MouseButtonPress,at.center(),tree->viewport()->mapToGlobal(at.center()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease,at.center(),tree->viewport()->mapToGlobal(at.center()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(),&press);QApplication::sendEvent(tree->viewport(),&release);
        require(!expected.isEmpty()&&view->selection()==expected,"a selector group marks its elements");view->setSelection({});}

    // A template: the picture of the active side under the board, at its resolution and offset.
    {QTemporaryDir pictures;QImage white(20,20,QImage::Format_RGB32);white.fill(Qt::white);require(white.save(pictures.filePath("scan.png")),"template picture");
        Document d;Board b=newBoard("Vorlage",10,10);b.activeLayer=CopperTop;b.templates[0]={pictures.filePath("scan.png"),254,{1,1},true};d.boards={b};editor.setDocument(d);
        const QImage shown=view->renderBoard(10);require(shown.pixelColor(15,15)==QColor(Qt::white)&&shown.pixelColor(50,50)==QColor(Qt::black),"template under the board");
        editor.editElements({},[](Element&){});view->board().activeLayer=CopperBottom;require(view->renderBoard(10).pixelColor(15,15)==QColor(Qt::black),"only the active side's template");}
    // A one-bit picture: its black pixels in the side's colour, its white ones in the board's; inner layers and the
    // outline show no template.
    {QTemporaryDir pictures;QImage mono(20,20,QImage::Format_Mono);mono.setColorTable({qRgb(0,0,0),qRgb(255,255,255)});mono.fill(0);
        for(int y=0;y<10;y++)for(int x=0;x<20;x++)mono.setPixel(x,y,1);require(mono.save(pictures.filePath("mono.bmp")),"one-bit template picture");
        Document d;Board b=newBoard("Vorlage",10,10);b.activeLayer=SilkBottom;b.multilayer=true;
        b.templates[0]={pictures.filePath("mono.bmp"),254,{1,1},true,QColor(255,0,0)};b.templates[1]={pictures.filePath("mono.bmp"),254,{1,1},true,QColor(0,0,255)};d.boards={b};editor.setDocument(d);
        const QImage shown=view->renderBoard(10);require(shown.pixelColor(15,15)==QColor(Qt::black)&&shown.pixelColor(15,25)==QColor(0,0,255),"one-bit template in the side's colour");
        view->board().activeLayer=SilkTop;require(view->renderBoard(10).pixelColor(15,25)==QColor(255,0,0),"the top side's template in its colour");
        for(int layer:{Inner1,Inner2,Outline}){view->board().activeLayer=layer;require(view->renderBoard(10).pixelColor(15,25)==QColor(Qt::black),"no template under inner layers and the outline");}}

    // On the screen the ground plane keeps out of a hatched area's whole outline at clearance 0 and meets its border; the
    // round ends of the lines lie on the plane.
    {Document d;Board b=newBoard("Raster",20,16);b.activeLayer=CopperBottom;b.groundPlane[CopperBottom]=true;
        auto e=newElement(ElementType::Area);e.layer=CopperBottom;e.points={{3,3},{13,3},{13,13},{3,13}};e.width=.2;e.hatched=true;e.hatchAuto=false;e.hatchPitch=2;e.clearance=0;
        b.elements<<e;d.boards={b};editor.setDocument(d);const QImage shown=view->renderBoard(20);
        auto green=[&](double x,double y){return shown.pixelColor(int(x*20),int(y*20)).green();};
        require(green(5,5)==0&&green(3.25,5)==0&&green(2.8,5)>100&&green(2.8,5)<130&&green(4,2.6)>170,"ground plane round a hatched area on the screen");}
    // A hatched keep-out cuts its whole outline out of the plane on the screen, as everywhere else: no plane in its gaps,
    // plane just outside its border, where the end of a grid line would reach.
    {Document d;Board b=newBoard("Sperr",20,16);b.activeLayer=CopperBottom;b.groundPlane[CopperBottom]=true;
        auto e=newElement(ElementType::Area);e.layer=CopperBottom;e.points={{3,3},{13,3},{13,13},{3,13}};e.width=.2;e.hatched=true;e.hatchAuto=false;e.hatchPitch=2;e.cutout=true;
        b.elements<<e;d.boards={b};editor.setDocument(d);const QImage shown=view->renderBoard(20);
        auto green=[&](double x,double y){return shown.pixelColor(int(x*20),int(y*20)).green();};
        require(green(5,5)==0&&green(2.75,6)>100&&green(2.75,6)<130,"a hatched keep-out on the screen");}

    // The example board draws: copper colours on the black board.
    editor.setDocument(exampleDocument());const QImage image=view->render(QSize(800,500));
    int green=0,blue=0,cyan=0;
    for(int y=0;y<image.height();y++)for(int x=0;x<image.width();x++){const QColor c=image.pixelColor(x,y);
        green+=c.green()>150&&c.red()<120&&c.blue()<100;blue+=c.blue()>200&&c.red()<100&&c.green()<140;cyan+=c.green()>200&&c.blue()>200&&c.red()<140;}
    require(green>100&&blue>100&&cyan>10,"the example board shows bottom copper, top copper and the via");
    // Every PCB action has a translated text.
    setUiLanguage("en");Editor english;setUiLanguage("de");
    require(english.action("exportLay6")->text()=="Export as Sprint-Layout 6…"&&english.action("otherSide")->text()=="To the other board &side","English action texts");
    // --- The menu table names only actions the editor has; the demo builds its menus from it.
    {const auto menus=editorMenus();require(menus.size()==7,"seven menus");
        for(const auto &[title,names]:menus)for(const auto &name:names)require(name.isEmpty()||editor.action(name),("menu action "+name).toUtf8().constData());
        QFile demo(QString(OPENLOCH_SOURCE_DIR)+"/src/modules/pcb/demo.cpp");require(demo.open(QIODevice::ReadOnly)&&demo.readAll().contains("editorMenus()"),"demo menus from the table");}
    // --- Pick and place crosses follow the component.
    {Editor placed;Document d;Board b=newBoard("Kreuz",40,30);
        auto smd=newElement(ElementType::SmdPad);smd.pos={10,10};smd.part=1;updateOutline(smd);b.elements<<smd;
        auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="U1";id.pos={8,6};id.part=1;id.pickAndPlace=true;updateStrokes(id);b.elements<<id;
        d.boards={b};placed.setDocument(d);auto *v=placed.view();
        require(v->pickPlaceMarks().size()==1&&v->pickPlaceMarks()[0].first==QPointF(10,10)&&v->pickPlaceMarks()[0].second==SilkTop,"a cross at the centre of the SMD pads");
        placed.editElements({0,1},[](Element &e){pcb::move(e,{5,0});});
        require(v->pickPlaceMarks().size()==1&&v->pickPlaceMarks()[0].first==QPointF(15,10),"the cross follows a move");
        placed.editElements({1},[](Element &e){e.pickAndPlace=false;});require(v->pickPlaceMarks().isEmpty(),"no cross without pick and place data");}
    // --- The grid counts from the origin, the origin itself goes onto the grid counted from the top left corner, the
    // standard tool shows the pointer as it is (x with its sign, y as the distance), and mirroring turns about the exact
    // middle of the selection.
    {Editor ge;Document d;d.boards={newBoard("Raster",50,40)};d.boards[0].grid=1.27;ge.setDocument(d);auto *v=ge.view();
        require(near(v->snap({10.1,10.0}),{10.16,9.52}),"the grid counts from the origin in the bottom left corner");
        Document middle=ge.document();middle.boards[0].origin={25,20};ge.setDocument(middle);v->setTool(BoardView::Tool::Select);
        send(v,QEvent::MouseMove,{10.1,30.0},Qt::NoButton);const QPointF shown=v->shownPointer();const QString text=ge.findChild<QLabel*>("coordinates")->text();
        require(std::abs(shown.x()-10.1)<.05&&std::abs(shown.y()-30)<.05,"the standard tool shows the pointer as it is");
        require(text.contains("-14")&&text.contains("10")&&!text.contains("-10"),"x with its sign, y as the distance from the origin");
        v->setTool(BoardView::Tool::Track);send(v,QEvent::MouseMove,{10.1,30.0},Qt::NoButton);v->setTool(BoardView::Tool::Select);
        send(v,QEvent::MouseMove,{10.3,10.3},Qt::NoButton);{QKeyEvent zero(QEvent::KeyPress,Qt::Key_0,Qt::NoModifier);QApplication::sendEvent(v,&zero);}
        require(near(ge.document().board().origin,{10.16,10.16}),"the origin goes onto the grid counted from the top left corner");
        Document pads=ge.document();auto pad=newElement(ElementType::Pad);pad.pos={10.3,10.3};updateOutline(pad);pads.boards[0].elements={pad};ge.setDocument(pads);
        v->setSelection({0});ge.mirrorSelection();ge.mirrorSelectionVertically();
        require(near(ge.document().board().elements[0].pos,{10.3,10.3}),"mirrored about the exact middle, not about a grid point");}
    // --- The component dialog shows the pick and place offset as the reference does, y positive downwards.
    {Editor oe;Document d;d.boards={newBoard("Offset",40,30)};auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="U1";id.part=1;
        id.pickOffset={1,2};updateStrokes(id);d.boards[0].elements={id};oe.setDocument(d);double shown=0;
        inDialog([&](QDialog *dialog){auto *y=dialog->findChild<QDoubleSpinBox*>("pickOffsetY");shown=y->value();y->setValue(3);});
        require(oe.editComponent(0)&&std::abs(shown+2)<1e-9&&std::abs(oe.document().board().elements[0].pickOffset.y()+3)<1e-9,"pick and place offset y shown downwards");}
    // --- Undo restores the ground plane switch and the board properties (both once changed the copy kept for undo too).
    {Editor ue;Document d;d.boards={newBoard("Alt",40,30)};ue.setDocument(d);
        const auto shown=[&]()->const Board&{return ue.document().board();};
        ue.setGroundPlane(true);require(shown().groundPlane[shown().activeLayer],"ground plane switched on");
        ue.undo();require(!shown().groundPlane[shown().activeLayer],"undo switches the ground plane off again");
        inDialog([](QDialog *dialog){dialog->findChild<QLineEdit*>()->setText("Neu");});ue.action("boardProperties")->trigger();
        require(shown().name=="Neu","board properties changed");ue.undo();require(shown().name=="Alt","undo restores the board properties");}
    // --- Identifiers and the component interface for projects
    {Editor ided;Document d;d.boards={newBoard("Kennungen",60,40),newBoard("Zweite",60,40)};ided.setDocument(d);auto *v=ided.view();
        const auto board=[&]()->const Board&{return ided.document().board();};
        const auto byId=[&](const QString &id){for(const auto &c:components(board()))if(c.id==id)return c;return Component{};};
        require(board().id.size()==32&&ided.document().boards[1].id.size()==32&&ided.document().boards[1].id!=board().id,"boards have identifiers of their own");
        Footprint dil;for(const auto &f:footprints())if(f.id=="dil-8")dil=f;require(!dil.elements.isEmpty(),"DIL 8 footprint");
        const QString chip="0123456789abcdef0123456789abcdef";
        require(ided.placeComponent(dil,chip,"IC7","NE555")&&v->placing(),"the component hangs on the pointer");click(v,{20,20});
        auto c=byId(chip);
        require(components(board()).size()==1&&c.designator>=0&&board().elements[c.designator].text=="IC7"&&c.value>=0&&board().elements[c.value].text=="NE555",
                "placed with identifier, designator and value");
        require(c.pins==QStringList{"1","2","3","4","5","6","7","8"},"pins of the pads in natural order");
        require(!ided.placeComponent(dil,chip,"IC8","NE555")&&!v->placing(),"a component the document has already is not placed again");
        {Footprint bare=dil;bare.elements.removeIf([](const Element &e){return e.role==TextRole::Value;});
            require(ided.placeComponent(bare,"ffffffffffffffffffffffffffffffff","IC9","TL071")&&v->placing(),"a footprint without a value text");
            click(v,{50,30},Qt::RightButton);require(!v->placing(),"placing called off");
            bare.elements.removeIf([](const Element &e){return e.role==TextRole::Designator;});require(!ided.placeComponent(bare,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","IC9","")&&!v->placing(),"a footprint without designator is no component");}
        int pad=-1;for(int i:c.members)if(board().elements[i].type==ElementType::Pad)pad=i;
        require(pad>=0&&ided.componentOf(pad)==chip&&ided.componentOf(-1).isEmpty(),"the component of an element");
        // Designator and value from the project, as one undo step; the same texts once more change nothing.
        require(ided.setComponent(chip,"U1","LM555")&&board().elements[c.designator].text=="U1"&&board().elements[c.value].text=="LM555","designator and value taken over");
        ided.markSaved();require(ided.setComponent(chip,"U1","LM555")&&!ided.isModified(),"the same texts change nothing");
        require(!ided.setComponent("missing","x","y"),"no such component");
        ided.undo();require(board().elements[c.designator].text=="IC7"&&board().elements[c.value].text=="NE555","taking over undone");ided.redo();
        {Board b=board();removeElements(b,{c.value});require(setComponentText(b,chip,"U1","10k"),"value for a component without a value text");
            const auto again=components(b).value(0);require(again.value>=0&&b.elements[again.value].text=="10k"&&b.elements[again.value].role==TextRole::Value,"a value text comes new");}
        // A copy is a new component, also on another board of the document; cut out and put back it stays the same one.
        v->setSelection(c.members);ided.action("duplicate")->trigger();
        {QSet<QString> ids;QSet<int> numbers;for(const auto &k:components(board())){ids<<k.id;numbers<<k.part;}
            require(ids.size()==2&&ids.contains(chip)&&numbers.size()==2,"a duplicate gets a new identifier and component number");}
        v->setSelection(byId(chip).members);ided.action("copy")->trigger();ided.switchBoard(1);ided.action("paste")->trigger();click(v,{30,20});
        require(components(board()).size()==1&&components(board())[0].id!=chip&&components(board())[0].id.size()==32,"a copy on another board is a new component");
        ided.switchBoard(0);const int number=byId(chip).part;v->setSelection(byId(chip).members);ided.action("cut")->trigger();require(byId(chip).designator<0,"cut out");
        ided.action("paste")->trigger();click(v,{40,20});
        require(byId(chip).designator>=0&&byId(chip).pins.size()==8&&byId(chip).part==number,"cut out and pasted, the component keeps its identifier and number");
        // Saving through a host: the handler takes over "Speichern" and "Speichern unter", and closing asks it too.
        QList<bool> calls;bool saves=true;
        ided.saveHandler=[&](bool asNew){calls.append(asNew);if(saves&&fromJson(ided.documentData())==ided.document())ided.markSaved();return saves;};
        require(ided.isModified(),"changed before saving");ided.action("save")->trigger();ided.action("saveAs")->trigger();
        require(calls==QList<bool>{false,true}&&!ided.isModified(),"the handler saves the document data");
        ided.setComponent(chip,"X1","");answer(QMessageBox::Save);require(ided.maybeSave()&&calls.size()==3&&!ided.isModified(),"asked when closing, the handler saves");
        saves=false;ided.setComponent(chip,"X2","");answer(QMessageBox::Save);require(!ided.maybeSave()&&calls.size()==4&&ided.isModified(),"a failed save keeps the window open");}
    // --- Copies of elements without component numbers (from older files): the innermost group of each designator becomes
    // a component of its own; elements in no such group stay without a number.
    {Editor copier;auto *v=copier.view();const auto els=[&]()->const QList<Element>&{return copier.document().board().elements;};
        const auto parts=[&]{return components(copier.document().board());};
        auto pad=newElement(ElementType::Pad);pad.pos={10.16,10.16};pad.groups={1};updateOutline(pad);
        auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="R1";id.pos={10,7};id.layer=SilkTop;id.groups={1};updateStrokes(id);
        auto pad2=pad;pad2.pos={30.48,10.16};pad2.groups={2};updateOutline(pad2);auto id2=id;id2.text="R2";id2.pos={30,7};id2.groups={2};updateStrokes(id2);
        auto show=[&](const QList<Element> &elements){Document d;Board b=newBoard("Kopien",60,40);b.grid=1.27;b.elements=elements;d.boards={b};copier.setDocument(d);
            QApplication::processEvents();v->fitBoard();v->setTool(BoardView::Tool::Select);};
        // Two designator groups, each with its pad: two components with numbers of their own.
        auto two=[&](int first,const char *message){const auto cs=parts();
            require(cs.size()==2&&cs[0].part!=cs[1].part&&cs[0].members==QList<int>({first,first+1})&&cs[1].members==QList<int>({first+2,first+3})
                    &&near(pickPlaceCentre(copier.document().board(),cs[0]),els()[first].pos)&&near(pickPlaceCentre(copier.document().board(),cs[1]),els()[first+2].pos),message);};
        show({pad,id,pad2,id2});require(parts().isEmpty(),"groups without numbers are no components");
        v->setSelection({0,1,2,3});copier.duplicateSelection();two(4,"duplicated designator groups: a component each");
        require(els()[5].text=="R3"&&els()[7].text=="R4"&&copier.componentOf(4)==els()[5].component&&copier.componentOf(6)==els()[7].component&&els()[5].component!=els()[7].component,
                "each duplicated group with an identifier of its own");
        require(copier.setComponent(els()[5].component,"R3",""),"the first copy is a component");
        copier.undo();v->setSelection({0,1,2,3});copier.action("cut")->trigger();require(els().isEmpty(),"cut out");
        copier.action("paste")->trigger();click(v,{30.48,25.4});two(0,"designator groups cut out and pasted: a component each");
        // A loose line copied together with a designator group stays out of the component.
        auto line=newElement(ElementType::Track);line.layer=SilkTop;line.width=.2;line.points={{5,15},{20,15}};
        show({pad,id,line});v->setSelection({0,1,2});copier.action("copy")->trigger();copier.action("paste")->trigger();click(v,{30.48,25.4});
        require(els().size()==6&&parts().size()==1&&parts()[0].members==QList<int>({3,4})&&els()[5].part==0,"a loose line stays out of the pasted component");
        // Without any groups the copies get one group: a designator with its pad becomes a component, two designators do not
        // (nothing tells which pad is whose).
        for(auto *e:{&pad,&id,&pad2,&id2})e->groups.clear();
        show({pad,id,pad2,id2});v->setSelection({0,1});copier.duplicateSelection();
        require(parts().size()==1&&parts()[0].members==QList<int>({4,5}),"a loose designator copied with its pad: one component");
        copier.undo();v->setSelection({0,1,2,3});copier.duplicateSelection();
        require(els().size()==8&&parts().isEmpty()&&!els()[4].groups.isEmpty()&&els()[4].groups==els()[7].groups,"two loose designators copied: one group, no component");}
    // --- Plugins: this test program answers as a plugin (see testPlugin in pcb_tests.cpp).
    {Editor plugged;Document d;Board b=newBoard("Plugin",50,40);b.groundPlane[CopperBottom]=true;
        auto pad=newElement(ElementType::Pad);pad.pos={10,10};updateOutline(pad);b.elements<<pad;
        auto track=newElement(ElementType::Track);track.points={{10,10},{20,10}};b.elements<<track;d.boards={b};plugged.setDocument(d);
        QTemporaryDir dir;const QString log=dir.filePath("arguments.txt");qputenv("OPENLOCH_PCB_TEST_PLUGIN_LOG",QFile::encodeName(log));
        const QString self=QCoreApplication::applicationFilePath();QString message;
        // The elements as they are now: an undo step copies the document, so a kept reference would go stale.
        auto els=[&]()->const QList<Element>&{return plugged.document().board().elements;};
        auto run=[&](int code){qputenv("OPENLOCH_PCB_TEST_PLUGIN",QByteArray::number(code));message.clear();return plugged.runPlugin(self,&message);};
        auto arguments=[&]{QFile f(log);require(f.open(QIODevice::ReadOnly),"plugin parameters written");return QString::fromUtf8(f.readAll()).split('\n');};
        plugged.view()->setSelection({0});
        require(run(0)==0&&els().size()==2&&!plugged.canUndo(),"exit code 0 changes nothing");
        const auto given=arguments();
        require(given.size()==9&&given[0].endsWith("elements.tmp")&&given[1]=="/L:DE"&&given[2]=="/W:500000"&&given[3]=="/H:400000"&&given[4]=="/X:0"
                &&given[5]=="/Y:400000"&&given[6]=="/R:12700"&&given[7]=="/M:2"&&given[8].startsWith("/P:"),"plugin parameters as documented");
        require(run(2)==2&&els().size()==4&&els()[2].type==ElementType::Pad&&std::abs(els()[2].pos.x()-15)<1e-9&&plugged.view()->selection()==QList<int>{2,3},"exit code 2 adds");
        plugged.undo();plugged.view()->setSelection({0});
        require(run(1)==1&&els().size()==3&&els()[0].type==ElementType::Track&&std::abs(els()[1].pos.x()-15)<1e-9,"exit code 1 replaces");
        plugged.undo();plugged.view()->setSelection({});
        require(run(4)==4&&els().size()==2&&plugged.view()->placing()&&arguments().contains("/A"),"exit code 4: all elements, the answer on the pointer");
        plugged.view()->beginPlacement({});plugged.view()->setSelection({1});
        require(run(3)==3&&els().size()==1&&plugged.view()->placing(),"exit code 3 removes and puts the answer on the pointer");
        plugged.view()->beginPlacement({});plugged.undo();
        require(run(130)==130&&message.contains("130")&&els().size()==2,"exit codes from 128 report an error");
        require(run(7)==7&&message.contains("7"),"unknown exit code");
        require(plugged.runPlugin(dir.filePath("missing-plugin"),&message)==-1&&!message.isEmpty(),"a plugin that does not start");
        qunsetenv("OPENLOCH_PCB_TEST_PLUGIN");qunsetenv("OPENLOCH_PCB_TEST_PLUGIN_LOG");}
    return 0;
}
