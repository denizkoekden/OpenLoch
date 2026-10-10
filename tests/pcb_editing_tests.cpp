// Editing tests of the PCB module: nodes of tracks and areas, favourite widths and text series, driven through the
// widgets offscreen. The preferences go to a file of the test (Editor::setPreferencesFile), never to the user's.
#include "language.h"
#include "modules/pcb/editor.h"
#include "modules/pcb/font.h"
#include "modules/pcb/overview.h"
#include "modules/pcb/printing.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QTabWidget>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QFrame>
#include <QLabel>
#include <QLineF>
#include <QWheelEvent>
#include <QEventLoop>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPrinter>
#include <QRadioButton>
#include <QSet>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <cmath>
#include <functional>
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
void click(BoardView *view,QPointF mm,Qt::MouseButton button=Qt::LeftButton){
    send(view,QEvent::MouseMove,mm,Qt::NoButton);send(view,QEvent::MouseButtonPress,mm,button);send(view,QEvent::MouseButtonRelease,mm,button);
}
void dragTo(BoardView *view,QPointF from,QPointF to){
    send(view,QEvent::MouseMove,from,Qt::NoButton);send(view,QEvent::MouseButtonPress,from);
    for(int i=1;i<=4;i++)send(view,QEvent::MouseMove,from+(to-from)*i/4.0,Qt::LeftButton);send(view,QEvent::MouseButtonRelease,to);
}
void key(QWidget *w,int k){QKeyEvent press(QEvent::KeyPress,k,Qt::NoModifier);QApplication::sendEvent(w,&press);}
bool near(QPointF a,QPointF b){return std::abs(a.x()-b.x())<1e-6&&std::abs(a.y()-b.y())<1e-6;}
// Fills in the next modal dialog with `fill` and accepts it.
void inDialog(const std::function<void(QDialog*)> &fill){
    auto *timer=new QTimer;timer->setInterval(5);
    QObject::connect(timer,&QTimer::timeout,[timer,fill]{
        if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){timer->stop();timer->deleteLater();fill(dialog);dialog->accept();}});
    timer->start();
}
QAction *named(QMenu *menu,const QString &name){for(auto *a:menu->findChildren<QAction*>())if(a->objectName()==name)return a;return nullptr;}
}

int editingTests(const QString &preferencesFile){
    // --- Virtual nodes: halfway along each segment of the selected track or area a node that becomes real when dragged.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Knoten",60,40);b.grid=1.27;b.origin={0,0};
        auto track=newElement(ElementType::Track);track.points={{10.16,10.16},{30.48,10.16}};track.width=.5;b.elements<<track;
        auto area=newElement(ElementType::Area);area.points={{10.16,20.32},{20.32,20.32},{20.32,30.48},{10.16,30.48}};area.width=.4;b.elements<<area;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();view->setTool(BoardView::Tool::Select);
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};
        view->setSelection({0});
        const auto halfway=view->virtualNodes();
        require(halfway.size()==1&&halfway[0].first==1&&near(halfway[0].second,{20.32,10.16}),"a track's virtual node lies halfway along its segment");
        click(view,{20.32,10.16});require(els()[0]==track&&!editor.canUndo()&&view->selection()==QList<int>{0},"a mere click on a virtual node changes nothing");
        dragTo(view,{20.32,10.16},{20.32,15.24});
        require(els()[0].points.size()==3&&near(els()[0].points[1],{20.32,15.24})&&near(els()[0].points[2],{30.48,10.16})&&editor.canUndo(),"dragging a virtual node puts a node there");
        editor.undo();require(els()[0]==track,"undo takes the new node away");editor.redo();require(els()[0].points.size()==3,"redo puts it back");editor.undo();
        // An area has one on the edge that closes it as well; its new node comes last.
        view->setSelection({1});const auto edges=view->virtualNodes();
        require(edges.size()==4&&edges.last().first==4&&near(edges.last().second,{10.16,25.4}),"an area's closing edge has a virtual node");
        dragTo(view,{10.16,25.4},{5.08,25.4});
        require(els()[1].points.size()==5&&near(els()[1].points[4],{5.08,25.4})&&near(els()[1].points[0],{10.16,20.32}),"the closing edge's new node comes last");
        editor.undo();require(els()[1]==area,"undo of the area's new node");
        // A node dragged onto the straight line between its neighbours goes again: no change, no undo step.
        editor.setDocument(d);QApplication::processEvents();view->fitBoard();view->setSelection({0});
        dragTo(view,{20.32,10.16},{22.86,10.16});require(els()[0]==track&&!editor.canUndo(),"a new node on the straight line is no change");
        // Calling a drag off: Esc or a right click while dragging; nothing changes, no undo step.
        send(view,QEvent::MouseMove,{20.32,10.16},Qt::NoButton);send(view,QEvent::MouseButtonPress,{20.32,10.16});send(view,QEvent::MouseMove,{20.32,17.78},Qt::LeftButton);
        require(els()[0].points.size()==3,"the new node follows the pointer while dragging");
        key(view,Qt::Key_Escape);require(els()[0]==track&&!editor.canUndo(),"Esc calls the drag off");
        send(view,QEvent::MouseButtonRelease,{20.32,17.78});require(els()[0]==track&&!editor.canUndo()&&view->selection()==QList<int>{0},"releasing after Esc changes nothing");
        send(view,QEvent::MouseMove,{10.16,10.16},Qt::NoButton);send(view,QEvent::MouseButtonPress,{10.16,10.16});send(view,QEvent::MouseMove,{5.08,5.08},Qt::LeftButton);
        require(near(els()[0].points[0],{5.08,5.08}),"a node follows the pointer");
        send(view,QEvent::MouseButtonPress,{5.08,5.08},Qt::RightButton);require(els()[0]==track&&!editor.canUndo(),"a right click calls the drag off");
        send(view,QEvent::MouseButtonRelease,{5.08,5.08},Qt::RightButton);send(view,QEvent::MouseButtonRelease,{5.08,5.08});require(els()[0]==track&&!editor.canUndo(),"nothing after the call-off");
        // Moving the whole track and calling it off with Esc.
        send(view,QEvent::MouseMove,{15,10.16},Qt::NoButton);send(view,QEvent::MouseButtonPress,{15,10.16});send(view,QEvent::MouseMove,{15,20},Qt::LeftButton);
        require(!near(els()[0].points[0],{10.16,10.16}),"the track moves with the pointer");key(view,Qt::Key_Escape);send(view,QEvent::MouseButtonRelease,{15,20});
        require(els()[0]==track&&!editor.canUndo(),"Esc calls a move off");
        // A right click on a node asks for its menu, with what applies to it.
        int menuElement=-1,menuNode=-1;const auto own=view->nodeMenuRequested;
        view->nodeMenuRequested=[&](int e,int n,QPoint){menuElement=e;menuNode=n;};click(view,{30.48,10.16},Qt::RightButton);
        require(menuElement==0&&menuNode==1,"a right click on a node asks for the node's menu");view->nodeMenuRequested=own;
        click(view,{30.48,10.16},Qt::RightButton);QApplication::processEvents();
        auto *menu=editor.findChild<QMenu*>("nodeMenu");
        require(menu&&named(menu,"nodeRemove")&&!named(menu,"nodeRemove")->isEnabled()&&named(menu,"nodeSplit")&&!named(menu,"nodeSplit")->isEnabled()&&named(menu,"nodeAlign")->isEnabled(),
                "a track of two nodes: its end can neither go nor split it");
        menu->close();QApplication::processEvents();}

    // --- A drag under way when undo, another tool, panning, Delete or an arrow key comes: it ends first, nothing stays
    // half moved and no index of the drag outlives the document it was taken from.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Ziehen",60,40);b.grid=1.27;b.origin={0,0};
        auto pad=newElement(ElementType::Pad);pad.pos={30.48,10.16};pad.size=2;pad.size2=.8;updateOutline(pad);b.elements<<pad;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();view->rubberBand=1;
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};
        // A track drawn onto the pad, the last element: dragging the pad takes the track's end along.
        view->setTool(BoardView::Tool::Track);click(view,{10.16,10.16});click(view,{30.48,10.16});key(view,Qt::Key_Return);view->setTool(BoardView::Tool::Select);
        require(els().size()==2&&near(els()[1].points.last(),{30.48,10.16}),"a track onto the pad");const Document before=editor.document();
        auto grab=[&]{view->setSelection({});send(view,QEvent::MouseMove,{31.2,10.16},Qt::NoButton);send(view,QEvent::MouseButtonPress,{31.2,10.16});
            send(view,QEvent::MouseMove,{31.2,20.32},Qt::LeftButton);};
        grab();require(near(els()[0].pos,{30.48,20.32})&&near(els()[1].points.last(),{30.48,20.32}),"the pad moves and takes the track's end along");
        editor.undo();require(els().size()==1&&els()[0]==pad,"undo while dragging: the drag ends, then the track drawn goes");
        send(view,QEvent::MouseMove,{35.56,20.32},Qt::LeftButton);send(view,QEvent::MouseButtonRelease,{35.56,20.32});
        require(els().size()==1&&els()[0]==pad&&editor.canRedo(),"moving on and releasing change nothing then");
        editor.redo();require(editor.document()==before,"redo brings the track back, no half moved pad with it");
        grab();view->setTool(BoardView::Tool::Pad);require(editor.document()==before,"another tool calls the drag off");
        send(view,QEvent::MouseButtonRelease,{31.2,20.32});require(editor.document()==before,"and the release after it changes nothing");
        view->setTool(BoardView::Tool::Select);grab();send(view,QEvent::MouseButtonPress,{31.2,20.32},Qt::MiddleButton);require(editor.document()==before,"panning calls the drag off");
        send(view,QEvent::MouseButtonRelease,{31.2,20.32},Qt::MiddleButton);send(view,QEvent::MouseButtonRelease,{31.2,20.32});require(editor.document()==before,"nothing after panning");
        grab();editor.deleteSelection();send(view,QEvent::MouseButtonRelease,{31.2,20.32});
        require(els().size()==1&&els()[0]==before.board().elements[1],"Delete while dragging takes the pad where it was, the track stays as it was");
        editor.undo();require(editor.document()==before,"one undo step for the Delete");
        grab();key(view,Qt::Key_Down);send(view,QEvent::MouseButtonRelease,{31.2,20.32});
        require(near(els()[0].pos,{30.48,11.43})&&near(els()[1].points.last(),{30.48,11.43}),"an arrow key while dragging moves from where the drag began");
        editor.undo();require(editor.document()==before,"one undo step for the arrow key");
        // Commands work on the document as it was before the drag: Ctrl+R turns the pad where it was, select all ends it.
        grab();editor.action("rotate")->trigger();send(view,QEvent::MouseButtonRelease,{31.2,20.32});
        require(near(els()[0].pos,{30.48,10.16})&&std::abs(els()[0].rotation-270)<1e-9&&near(els()[1].points.last(),{30.48,10.16}),"a turn while dragging turns the pad in place");
        editor.undo();require(editor.document()==before,"one undo step for the turn");
        grab();editor.action("selectAll")->trigger();send(view,QEvent::MouseMove,{35.56,25.4},Qt::LeftButton);send(view,QEvent::MouseButtonRelease,{35.56,25.4});
        require(editor.document()==before&&view->selection().size()==2,"select all while dragging ends the drag first, nothing moves after");
        // The origin set again where it is: no undo step, redo stays.
        editor.undo();require(editor.canRedo(),"a step to redo");send(view,QEvent::MouseMove,{.3,.3},Qt::NoButton);key(view,Qt::Key_0);
        require(editor.canRedo()&&editor.document().board().origin==QPointF(0,0),"the origin set where it is: no step");editor.redo();}

    // --- An edit that changes nothing leaves the steps to undo and redo as they are, however few are kept.
    {Editor editor;Document d;Board b=newBoard("Schritte",40,30);b.grid=1;b.origin={0,0};
        auto t=newElement(ElementType::Track);t.points={{5,5},{15,5}};b.elements<<t;d.boards={b};editor.setDocument(d);editor.setUndoLimit(1);
        editor.editElements({0},[](Element &e){pcb::move(e,{0,5});});require(editor.canUndo(),"one step");
        require(editor.alignNode(0,0)&&editor.canUndo(),"a node already on the grid: no step, the one before stays");
        editor.editElements({0},[](Element &){});require(editor.canUndo(),"an edit without change keeps it too");
        editor.undo();require(editor.document()==d&&editor.canRedo(),"the move undone");
        editor.editElements({0},[](Element &e){e.width=e.width;});require(editor.canRedo(),"redo stays after an edit without change");
        inDialog([](QDialog *){});editor.action("boardProperties")->trigger();require(editor.canRedo(),"board properties confirmed unchanged: redo stays");
        inDialog([](QDialog *){});editor.action("projectInfo")->trigger();require(editor.canRedo(),"so does the project info");
        editor.redo();require(near(editor.document().board().elements[0].points[0],{5,10}),"and works");}

    // --- The node commands: remove (keeping two nodes of a track, three of an area), put one or all nodes on the grid
    // counted from the origin, split a track at an inner node.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Knoten",60,40);b.grid=1;b.origin={.5,.25};
        auto t=newElement(ElementType::Track);t.points={{10,10},{20.3,10.1},{30,20},{40,20}};t.width=.6;t.groups={4};t.part=7;t.flatStart=t.flatEnd=true;
        t.autorouted=true;t.autoroutePads={2,3};t.clearance=.25;t.name="Netz";b.elements<<t;
        auto a=newElement(ElementType::Area);a.points={{10,30},{20,30},{15,35}};b.elements<<a;
        auto p1=newElement(ElementType::Pad);p1.pos={10,10};updateOutline(p1);auto p2=p1;p2.pos={40,20};updateOutline(p2);b.elements<<p1<<p2;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();view->setTool(BoardView::Tool::Select);
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};
        require(editor.alignNode(0,1)&&near(els()[0].points[1],{20.5,10.25})&&near(els()[0].points[0],{10,10}),"one node onto the grid counted from the origin");editor.undo();
        require(editor.alignNodes(0)&&near(els()[0].points[0],{10.5,10.25})&&near(els()[0].points[3],{40.5,20.25})&&near(els()[0].points[1],{20.5,10.25}),"all nodes onto the grid");editor.undo();
        require(els()[0]==t&&!editor.alignNode(2,0)&&!editor.alignNode(0,9),"undo; nodes only of tracks and areas");
        require(!editor.removeNode(1,0)&&els()[1].points.size()==3,"an area keeps three nodes");
        require(editor.removeNode(0,1)&&els()[0].points.size()==3&&near(els()[0].points[1],{30,20}),"a node removed");editor.undo();
        require(!editor.splitTrack(0,0)&&!editor.splitTrack(0,3)&&!editor.splitTrack(1,1),"no split at the ends of a track, nor of an area");
        // Splitting through the node's menu.
        view->setSelection({0});click(view,{30,20},Qt::RightButton);QApplication::processEvents();
        auto *menu=editor.findChild<QMenu*>("nodeMenu");require(menu&&named(menu,"nodeSplit")&&named(menu,"nodeSplit")->isEnabled()&&named(menu,"nodeRemove")->isEnabled(),"an inner node can split the track");
        named(menu,"nodeSplit")->trigger();menu->close();QApplication::processEvents();
        require(els().size()==5,"split at an inner node: a second track at the end of the list");
        const Element x=els()[0],y=els()[4];
        require(x.points==QPolygonF({QPointF(10,10),QPointF(20.3,10.1),QPointF(30,20)})&&y.points==QPolygonF({QPointF(30,20),QPointF(40,20)}),"both halves share the node");
        require(x.flatStart&&!x.flatEnd&&!y.flatStart&&y.flatEnd,"the square ends stay at the outer ends");
        require(x.groups==QList<int>{4}&&y.groups==QList<int>{4}&&x.part==7&&y.part==7&&y.width==.6&&y.clearance==.25&&y.name=="Netz"&&y.layer==t.layer,"both halves keep group, component and settings");
        require(x.autorouted&&y.autorouted&&x.autoroutePads==t.autoroutePads&&y.autoroutePads==t.autoroutePads,"both halves keep the autoroute mark with its pads");
        require(view->selection()==QList<int>({0,4})&&els()[2]==p1&&els()[3]==p2,"both halves selected, the pads keep their places");
        // Either half turns back into the airwire between the pads.
        view->setTool(BoardView::Tool::Autoroute);click(view,{35,20});
        require(els().size()==4&&els()[2].connections==QList<int>{3}&&els()[3].connections==QList<int>{2},"a split autoroute half turns back into its airwire");
        editor.undo();editor.undo();require(els().size()==4&&els()[0]==t,"undo joins the track again");view->setTool(BoardView::Tool::Select);
        require(editor.removeNode(0,1)&&editor.removeNode(0,1)&&!editor.removeNode(0,0)&&els()[0].points.size()==2,"a track keeps two nodes");}
    // --- Joining two tracks that meet at an end node into one, through the node's menu.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Verbinden",60,40);b.grid=1.27;b.origin={0,0};
        auto p1=newElement(ElementType::Pad);p1.pos={5,30};p1.connections={1};updateOutline(p1);auto p2=p1;p2.pos={55,30};p2.connections={0};updateOutline(p2);
        auto t=newElement(ElementType::Track);t.points={{10,10},{20,10}};t.width=.8;t.flatStart=true;t.groups={3};t.name="Netz";
        auto u=newElement(ElementType::Track);u.points={{20,10},{30,20}};u.width=.4;u.flatEnd=true;
        auto w=newElement(ElementType::Track);w.points={{20,10},{20,25}};w.layer=CopperTop;
        b.elements<<p1<<u<<t<<w<<p2;b.elements[0].connections={4};b.elements[4].connections={0};d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};
        require(editor.joiningTrack(2,1)==1&&editor.joiningTrack(2,0)<0&&editor.joiningTrack(3,0)<0,"a track of the same layer ending at an end node joins, one on another layer does not");
        view->setSelection({2});click(view,{20,10},Qt::RightButton);QApplication::processEvents();
        auto *menu=editor.findChild<QMenu*>("nodeMenu");require(menu&&named(menu,"nodeJoin")&&named(menu,"nodeJoin")->isEnabled()&&!named(menu,"nodeSplit")->isEnabled()&&!named(menu,"nodeRemove")->isEnabled(),
                                                                  "at a shared end node the tracks can be joined");
        named(menu,"nodeJoin")->trigger();menu->close();QApplication::processEvents();
        require(els().size()==4&&els()[1].points==QPolygonF({QPointF(10,10),QPointF(20,10),QPointF(30,20)}),"one track of three nodes, the shared one once");
        require(els()[1].width==.8&&els()[1].groups==QList<int>{3}&&els()[1].name=="Netz"&&els()[1].flatStart&&els()[1].flatEnd,"it keeps the first track's settings and both outer ends");
        require(view->selection()==QList<int>{1}&&els()[0].connections==QList<int>{3}&&els()[3].connections==QList<int>{0},"the joined track selected, airwires still between the pads");
        editor.undo();require(els().size()==5&&els()[1]==u&&els()[2]==t,"one undo step");
        view->setSelection({2});click(view,{10,10},Qt::RightButton);QApplication::processEvents();
        menu=nullptr;for(auto *m:editor.findChildren<QMenu*>("nodeMenu"))if(m->isVisible())menu=m;
        require(menu&&named(menu,"nodeJoin")&&!named(menu,"nodeJoin")->isEnabled(),"not at an end where no track ends");menu->close();QApplication::processEvents();
        // From a first node the other track goes in front.
        require(editor.joinTracks(1,0)&&els().size()==4&&els()[1].points==QPolygonF({QPointF(10,10),QPointF(20,10),QPointF(30,20)})&&els()[1].width==.4&&els()[1].flatStart&&els()[1].flatEnd,
                "joined at the first node, the other track in front");}

    // --- Favourite track widths: added from the current width, chosen, removed; kept in the preferences.
    {Editor editor;editor.show();QApplication::processEvents();
        auto *button=editor.findChild<QToolButton*>("widthFavourites");auto *box=editor.findChild<QDoubleSpinBox*>("trackWidth");
        require(button&&button->menu()&&box&&editor.widthFavourites().isEmpty(),"no favourite widths at first");
        auto show=[&]{Q_EMIT button->menu()->aboutToShow();};
        box->setValue(.8);show();require(named(button->menu(),"favouriteAdd")&&named(button->menu(),"favouriteAdd")->isEnabled(),"the current width can be added");
        named(button->menu(),"favouriteAdd")->trigger();box->setValue(.4);show();named(button->menu(),"favouriteAdd")->trigger();
        require(editor.widthFavourites()==QList<double>({.4,.8}),"favourites sorted");
        {QSettings s(preferencesFile,QSettings::IniFormat);require(s.value("trackWidthFavourites").toStringList()==QStringList({"0.4","0.8"}),"favourites kept in the preferences");}
        show();QList<QAction*> widths;for(auto *a:button->menu()->actions())if(a->isCheckable())widths.append(a);
        require(widths.size()==2&&widths[0]->isChecked()&&!widths[1]->isChecked()&&!named(button->menu(),"favouriteAdd")->isEnabled(),"the current width ticked, not offered again");
        widths[1]->trigger();require(std::abs(box->value()-.8)<1e-9&&std::abs(editor.view()->trackWidth-.8)<1e-9,"choosing a favourite sets the width");
        show();auto *remove=button->menu()->findChild<QMenu*>("favouriteRemove");require(remove&&remove->actions().size()==2,"each favourite can be removed");
        remove->actions()[0]->trigger();require(editor.widthFavourites()==QList<double>{.8},"a favourite removed");
        Editor other;other.loadPreferences();require(other.widthFavourites()==QList<double>{.8},"favourites read back from the preferences");}

    // --- The view: the photo view's options with the reference's colours, the transparent mode, the template switches,
    // keep-out rectangles, the help line with its keys and the layer info.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Ansicht",40,30);b.grid=1.27;
        auto far=newElement(ElementType::Track);far.layer=CopperBottom;far.points={{5,10},{35,10}};far.width=2;b.elements<<far;
        auto near=newElement(ElementType::Track);near.layer=CopperTop;near.points={{5,20},{35,20}};near.width=2;b.elements<<near;
        auto smd=newElement(ElementType::SmdPad);smd.layer=CopperTop;smd.pos={20,26};smd.size=4;smd.size2=2;smd.solderMask=true;updateOutline(smd);b.elements<<smd;
        auto cross=newElement(ElementType::Track);cross.layer=CopperBottom;cross.points={{30,5},{30,25}};cross.width=2;b.elements<<cross;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();
        auto pixel=[&](QPointF mm){const QImage image=view->renderBoard(10,false);return QColor(image.pixel(int(mm.x()*10),int(mm.y()*10)));};
        editor.action("photo")->trigger();QApplication::processEvents();
        auto *bar=editor.findChild<QFrame*>("photoBar");require(bar&&bar->isVisible(),"the photo view's bar");
        require(pixel({2,2})==QColor(22,117,22)&&pixel({20,10})==QColor(2,97,2)&&pixel({20,26})==QColor(220,170,20),"green board, the far copper through it, gold pads");
        {const QColor c=pixel({10,20});require(c.green()>165&&c.green()<180&&c.red()<10,"the near copper covers three quarters");}
        editor.findChild<QCheckBox*>("photoTranslucent")->setChecked(false);require(pixel({20,10})==QColor(22,117,22),"not translucent: the far side hidden");
        editor.findChild<QComboBox*>("photoBoard")->setCurrentIndex(1);editor.findChild<QComboBox*>("photoFinish")->setCurrentIndex(1);
        require(pixel({2,2})==QColor(36,82,200)&&pixel({20,26})==QColor(210,210,210),"blue board, silver pads");
        editor.findChild<QComboBox*>("photoFinish")->setCurrentIndex(2);require(pixel({20,26})==QColor(132,164,228),"without a finish the copper's colour");
        editor.findChild<QRadioButton*>("photoBottom")->setChecked(true);require(editor.action("fromBelow")->isChecked()&&view->fromBelow(),"seen from below");
        editor.action("fromBelow")->trigger();require(editor.findChild<QRadioButton*>("photoTop")->isChecked(),"the bar follows the view");
        editor.action("photo")->trigger();require(!bar->isVisible(),"the bar goes with the photo view");
        // Transparent: the layers mix bit by bit where they overlap (OR on the dark board).
        const QColor k1=view->colours.layers[CopperTop],k2=view->colours.layers[CopperBottom];
        editor.action("transparent")->trigger();require(view->transparent&&pixel({30,20})==QColor::fromRgb(k1.rgb()|k2.rgb()),"overlapping layers mixed");
        editor.action("transparent")->trigger();require(pixel({30,20})!=QColor::fromRgb(k1.rgb()|k2.rgb())||(k1.rgb()|k2.rgb())==k1.rgb(),"and covered again");
        // The template alone hides the elements.
        editor.findChild<QToolButton*>("templateOnly")->setChecked(true);require(view->templateOnly&&pixel({10,20})==view->colours.board,"the template alone");
        editor.findChild<QToolButton*>("templateOnly")->setChecked(false);editor.findChild<QToolButton*>("templateHidden")->setChecked(true);require(view->templateHidden,"the template hidden");
        editor.findChild<QToolButton*>("templateHidden")->setChecked(false);
        // Keep-out rectangles.
        editor.findChild<QToolButton*>("keepoutRectangle")->setChecked(true);view->setTool(BoardView::Tool::Keepout);view->fitBoard();
        const int before=int(editor.document().board().elements.size());dragTo(view,{6.35,3.81},{16.51,8.89});
        {const auto &els=editor.document().board().elements;
         require(els.size()==before+1&&els.last().type==ElementType::Area&&els.last().cutout&&els.last().width==0&&els.last().points.size()==4,"a keep-out drawn as a rectangle");}
        // The help line: what the tool does and the keys; Space counts the bend.
        auto *help=editor.findChild<QLabel*>("helpLine");view->setTool(BoardView::Tool::Track);
        require(help&&help->text().contains(Editor::toolHelp(BoardView::Tool::Track))&&help->text().contains("[1/5]"),"the help line of the track tool");
        key(view,Qt::Key_Space);require(help->text().contains("[2/5]"),"Space counts the bend");
        view->setTool(BoardView::Tool::Select);require(help->text()==Editor::toolHelp(BoardView::Tool::Select),"no keys for the standard tool");
        bool explained=false;inDialog([&](QDialog *x){explained=x->findChild<QLabel*>("layerUses")!=nullptr;});
        editor.findChild<QToolButton*>("layerInfo")->click();require(explained,"the layer info");}

    // --- Details of the reference's dialogs and menus: 5° ticked, onto a layer from the Funktionen menu, right angles for
    // texts, the length of texts and of the project comment, Shift for half a grid step, commands in the help line.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Details",40,30);b.grid=1.27;
        auto label=newElement(ElementType::Text);label.text="A";label.layer=SilkTop;label.pos={10,10};label.size=2;updateStrokes(label);b.elements<<label;
        auto pad=newElement(ElementType::Pad);pad.pos={20.32,10.16};updateOutline(pad);b.elements<<pad;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};
        auto *five=editor.findChild<QAction*>("rotationStep5");five->trigger();
        require(five->isChecked()&&!editor.findChild<QAction*>("rotationStepFree")->isChecked(),"5° ticked, not the free angle");
        editor.findChild<QAction*>("rotationStep90")->trigger();
        view->setSelection({0});QApplication::processEvents();QApplication::processEvents();
        auto *layers=editor.action("setLayer");require(layers&&layers->isEnabled()&&layers->menu(),"onto a layer in the Funktionen menu");
        Q_EMIT layers->menu()->aboutToShow();QAction *bottom=nullptr;for(auto *a:layers->menu()->actions())if(a->objectName()==QStringLiteral("setLayerTo-%1").arg(SilkBottom))bottom=a;
        require(bottom,"the layers offered");bottom->trigger();require(els()[0].layer==SilkBottom,"the text onto B2");editor.undo();
        QToolButton *right=nullptr;for(auto *t:editor.findChildren<QToolButton*>("textAngle90"))if(!t->isHidden())right=t;
        require(right,"right angles for a text");right->click();require(els()[0].rotation==90,"the text turned to 90°");
        // The comment of the project at most 1900 characters.
        int kept=-1;inDialog([&](QDialog *x){auto *c=x->findChild<QPlainTextEdit*>("projectComment");c->setPlainText(QString(2000,'x'));kept=int(c->toPlainText().size());});
        editor.action("projectInfo")->trigger();require(kept==1900&&editor.document().comment.size()==1900,"the project comment limited");
        // Shift moves by half a grid step.
        view->setSelection({1});const QPointF was=els()[1].pos;
        {QKeyEvent press(QEvent::KeyPress,Qt::Key_Right,Qt::ShiftModifier);QApplication::sendEvent(view,&press);}
        require(std::abs(els()[1].pos.x()-was.x()-1.27/2)<1e-9,"Shift: half a grid step");
        // A command under the pointer in the help line.
        auto *help=editor.findChild<QLabel*>("helpLine");Q_EMIT editor.action("mirror")->hovered();
        require(help->text().startsWith(ui("Horizontal spiegeln"))&&help->text().contains(Editor::commandHelp("mirror")),"a command in the help line, with what it does");
        for(auto *a:editor.toolBarActions())if(a)require(!Editor::commandHelp(a->objectName()).isEmpty(),"every command of the tool bar explained");}

    // --- Found in the review: the printer's orientation reaches the preview, a single board never over the document's
    // own file, the vertical mirror of a text, upright texts keep the component data typed in, long texts are not cut by
    // their field, an inner layer survives the component dialog, a field going with the form changes nothing, a macro
    // that cannot be written offers no description to edit.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Befunde",40,30);b.multilayer=true;b.grid=1.27;
        auto label=newElement(ElementType::Text);label.text="A";label.layer=SilkTop;label.pos={10,10};label.size=2;label.rotation=90;updateStrokes(label);b.elements<<label;
        auto lead=newElement(ElementType::Pad);lead.pos={25,20};lead.part=1;updateOutline(lead);b.elements<<lead;
        auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="R1";id.part=1;id.layer=Inner1;id.pos={25,24};id.size=1.5;id.rotation=180;updateStrokes(id);b.elements<<id;
        d.boards={b,newBoard("Zweite",20,20)};editor.setDocument(d);QApplication::processEvents();view->fitBoard();
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};
        {QPrinter printer;printer.setPageOrientation(QPageLayout::Landscape);PrintPreview preview(editor.document().board(),defaultPrintSettings(editor.document().board()),"x.olpcb",&printer);
            require(preview.settings().orientation==QPageLayout::Landscape,"the printer's orientation in the preview");}
        QTemporaryDir dir;const QString own=dir.filePath("befunde.olpcb");require(editor.saveFile(own),"saved");QString error;
        require(!editor.saveBoardAs(own,&error)&&!error.isEmpty()&&load(own).boards.size()==2,"one board never over the document's own file");
        view->setSelection({0});QApplication::processEvents();QApplication::processEvents();
        // Mirrored top to bottom as the reference's switch does it: about the baseline, the text keeps its direction; the
        // angle shown stays 90°, the model holds the half turn.
        QCheckBox *flip=nullptr;for(auto *x:editor.findChildren<QCheckBox*>("textFlipped"))if(!x->isHidden())flip=x;
        require(flip&&!flip->isChecked(),"the vertical mirror as a switch");const auto before=els()[0].strokes;flip->click();
        require(els()[0].flipped&&els()[0].mirrored&&std::abs(els()[0].rotation-270)<1e-9&&els()[0].pos==QPointF(10,10),"mirrored top to bottom, the half turn in the model");
        {QRectF was,now;for(const auto &l:before)was|=l.boundingRect();for(const auto &l:els()[0].strokes)now|=l.boundingRect();
         require(std::abs(was.top()-now.top())<1e-6&&std::abs(was.bottom()-now.bottom())<1e-6&&std::abs(now.left()-(2*10-was.right()))<1e-6,"about its baseline, running the same way");}
        QApplication::processEvents();QApplication::processEvents();
        {QDoubleSpinBox *angle=nullptr;for(auto *x:editor.findChildren<QDoubleSpinBox*>())if(!x->isHidden()&&x->suffix()==QStringLiteral("°")&&!angle)angle=x;
         require(angle&&std::abs(angle->value()-90)<1e-9,"the angle shown as the reference shows it");}
        editor.undo();
        // Upright and the component data typed in.
        inDialog([](QDialog *x){x->findChild<QCheckBox*>("alignTexts")->setChecked(true);x->findChild<QDoubleSpinBox*>("componentAngle")->setValue(0);});
        require(editor.editComponent(2)&&std::abs(els()[2].componentRotation)<1e-9&&els()[2].rotation==0,"upright, the angle typed in kept");
        require(els()[2].layer==Inner1,"the inner layer kept");
        // A long text keeps its characters when its field is confirmed.
        editor.editElements({0},[](Element &x){x.text=QString(60,'L');updateStrokes(x);});view->setSelection({0});QApplication::processEvents();QApplication::processEvents();
        QLineEdit *field=nullptr;for(auto *x:editor.findChildren<QLineEdit*>())if(!x->isHidden()&&x->text()==QString(60,'L'))field=x;
        require(field&&field->maxLength()>=60,"the field holds the long text");Q_EMIT field->editingFinished();require(els()[0].text.size()==60,"not cut");
        // A name typed into the board's field and an undo: the undo wins, redo stays.
        view->setSelection({});QApplication::processEvents();QApplication::processEvents();
        QLineEdit *name=nullptr;for(auto *x:editor.findChildren<QLineEdit*>("boardName"))if(!x->isHidden())name=x;
        require(name&&editor.canUndo(),"the board's name field");name->setFocus();name->setText("Getippt");editor.undo();QApplication::processEvents();
        require(editor.canRedo()&&editor.document().board().name!="Getippt","the undo not undone by the field");}

    // --- The selector: through-plated pads as a kind of their own after all pads.
    {Editor editor;editor.show();QApplication::processEvents();
        Document d;Board b=newBoard("Selector",40,30);auto pad=newElement(ElementType::Pad);pad.pos={10,10};updateOutline(pad);b.elements<<pad;
        pad.pos={20,10};pad.via=true;updateOutline(pad);b.elements<<pad;d.boards={b};editor.setDocument(d);
        auto *type=editor.findChild<QComboBox*>("selectorType");auto *tree=editor.findChild<QTreeWidget*>("selector");
        type->setCurrentIndex(0);require(tree&&tree->topLevelItemCount()==1&&tree->topLevelItem(0)->childCount()==2,"all pads");
        type->setCurrentIndex(1);require(tree->topLevelItemCount()==1&&tree->topLevelItem(0)->childCount()==1,"the through-plated one only");}

    // --- Favourite pad and SMD sizes, the same way; choosing one changes the selected pads in one step. The button below
    // the SMD fields swaps width and height, selected SMD pads follow.
    {Editor editor;editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Favoriten",40,30);auto pad=newElement(ElementType::Pad);pad.pos={10,10};updateOutline(pad);b.elements<<pad;
        auto smd=newElement(ElementType::SmdPad);smd.pos={20,10};smd.size=.9;smd.size2=1.8;updateOutline(smd);b.elements<<smd;d.boards={b};editor.setDocument(d);
        auto *padButton=editor.findChild<QToolButton*>("padFavourites");auto *outer=editor.findChild<QDoubleSpinBox*>("padDiameter");auto *drill=editor.findChild<QDoubleSpinBox*>("padDrill");
        require(padButton&&padButton->menu()&&editor.sizeFavourites(Editor::PadSizes).isEmpty()&&editor.sizeFavourites(Editor::SmdSizes).isEmpty(),"no favourite sizes at first");
        auto show=[](QToolButton *button){Q_EMIT button->menu()->aboutToShow();};
        outer->setValue(2);drill->setValue(1);show(padButton);named(padButton->menu(),"sizeFavouriteAdd")->trigger();
        outer->setValue(1.6);drill->setValue(.8);show(padButton);named(padButton->menu(),"sizeFavouriteAdd")->trigger();
        require(editor.sizeFavourites(Editor::PadSizes)==QList<QPointF>({{1.6,.8},{2,1}}),"pad sizes sorted");
        {QSettings s(preferencesFile,QSettings::IniFormat);require(s.value("padFavourites").toStringList()==QStringList({"1.6x0.8","2x1"}),"kept in the preferences");}
        show(padButton);QList<QAction*> sizes;for(auto *a:padButton->menu()->actions())if(a->isCheckable())sizes.append(a);
        require(sizes.size()==2&&sizes[0]->isChecked()&&!named(padButton->menu(),"sizeFavouriteAdd")->isEnabled(),"the current size ticked, not offered again");
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};view->setSelection({0});const double was=els()[0].size,wasDrill=els()[0].size2;sizes[1]->trigger();
        require(std::abs(els()[0].size-2)<1e-9&&std::abs(els()[0].size2-1)<1e-9&&std::abs(view->padDiameter-2)<1e-9,"a favourite chosen: the selected pad follows");
        editor.undo();require(std::abs(els()[0].size-was)<1e-9&&std::abs(els()[0].size2-wasDrill)<1e-9,"in one undo step");
        auto *smdButton=editor.findChild<QToolButton*>("smdFavourites");auto *width=editor.findChild<QDoubleSpinBox*>("smdWidth");auto *height=editor.findChild<QDoubleSpinBox*>("smdHeight");
        width->setValue(.9);height->setValue(1.8);show(smdButton);named(smdButton->menu(),"sizeFavouriteAdd")->trigger();
        require(editor.sizeFavourites(Editor::SmdSizes)==QList<QPointF>({{.9,1.8}}),"an SMD size added");
        view->setSelection({1});editor.findChild<QToolButton*>("smdSwap")->click();
        require(std::abs(width->value()-1.8)<1e-9&&std::abs(height->value()-.9)<1e-9&&std::abs(els()[1].size-1.8)<1e-9&&std::abs(els()[1].size2-.9)<1e-9,"width and height swapped, the SMD pad follows");
        editor.undo();require(std::abs(els()[1].size-.9)<1e-9&&std::abs(els()[1].size2-1.8)<1e-9,"one undo step");
        show(smdButton);auto *remove=smdButton->menu()->findChild<QMenu*>("sizeFavouriteRemove");require(remove&&remove->actions().size()==1,"removable");
        remove->actions()[0]->trigger();require(editor.sizeFavourites(Editor::SmdSizes).isEmpty(),"removed");
        Editor other;other.loadPreferences();require(other.sizeFavourites(Editor::PadSizes)==QList<QPointF>({{1.6,.8},{2,1}}),"read back from the preferences");}

    // --- Text series: the text typed with a number that counts up; each text waits at the pointer, the first one too.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Texte",60,40);b.grid=1.27;b.origin={0,0};d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};
        auto dialog=[](const QString &text,bool numbered,int start){inDialog([=](QDialog *x){x->findChild<QLineEdit*>("text")->setText(text);
            x->findChild<QCheckBox*>("series")->setChecked(numbered);x->findChild<QSpinBox*>("seriesStart")->setValue(start);});};
        // The number the dialog offers first is 0; without a text and a number nothing follows.
        int offered=-1;view->setTool(BoardView::Tool::Text);
        inDialog([&](QDialog *x){offered=x->findChild<QSpinBox*>("seriesStart")->value();x->findChild<QLineEdit*>("text")->setText("");});click(view,{10.16,10.16});
        require(offered==0&&els().isEmpty()&&!view->textSeries(),"a series starts at 0 unless another number is chosen");
        dialog("R",true,5);click(view,{10.16,10.16});
        require(els().isEmpty()&&view->textSeries()&&view->nextSeriesNumber()==5,"the first text of a series waits at the pointer, nothing goes down at the place clicked");
        // The text shows at the pointer.
        send(view,QEvent::MouseMove,{30.48,25.4},Qt::NoButton);QApplication::processEvents();const QImage waiting=view->grab().toImage();
        view->endTextSeries();QApplication::processEvents();const QImage without=view->grab().toImage();
        const QPoint at=view->toPixel({30.48,25.4}).toPoint();int differ=0;
        for(int y=at.y()-30;y<at.y();y++)for(int x=at.x();x<at.x()+40;x++)if(waiting.rect().contains(x,y)&&waiting.pixel(x,y)!=without.pixel(x,y))differ++;
        require(differ>20,"the text of the series waits at the pointer");
        dialog("R",true,5);click(view,{10.16,10.16});
        click(view,{10.16,10.16});click(view,{20.32,10.16});click(view,{30.48,10.16});
        require(els().size()==3&&els()[0].text=="R5"&&near(els()[0].pos,{10.16,10.16})&&els()[1].text=="R6"&&els()[2].text=="R7"&&near(els()[2].pos,{30.48,10.16})
                &&els()[2].size==els()[0].size&&!els()[2].strokes.isEmpty(),"each click puts the next number down");
        editor.undo();require(view->nextSeriesNumber()==7&&view->textSeries(),"undoing the last text of a series takes its number back");
        editor.redo();require(view->nextSeriesNumber()==8&&els().size()==3,"redo takes it again");
        click(view,{40,20},Qt::RightButton);require(!view->textSeries()&&view->tool()==BoardView::Tool::Text&&els().size()==3,"a right click ends the series, the text tool stays");
        editor.undo();require(els().size()==2,"each text of the series is one undo step");editor.undo();editor.undo();require(els().isEmpty(),"three undo steps");
        dialog("C",true,1);click(view,{10.16,20.32});click(view,{10.16,20.32});require(els().last().text=="C1"&&view->textSeries(),"a series from 1");
        key(view,Qt::Key_Escape);require(!view->textSeries()&&view->tool()==BoardView::Tool::Text,"Esc ends a series, the text tool stays");
        dialog("X",false,3);click(view,{10.16,30.48});require(els().last().text=="X"&&!view->textSeries(),"without the number no series follows");
        dialog("",true,0);click(view,{20.32,30.48});click(view,{20.32,30.48});require(els().last().text=="0"&&view->textSeries(),"a series of numbers alone");
        view->setTool(BoardView::Tool::Select);require(!view->textSeries(),"another tool ends the series");}
    // A text elsewhere that reads like a number of a series does not hold that number back after undo.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Folge",60,40);b.grid=1.27;b.origin={0,0};
        auto other=newElement(ElementType::Text);other.text="R6";other.pos={50.8,35.56};other.size=1.5;updateStrokes(other);b.elements<<other;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};view->setTool(BoardView::Tool::Text);
        inDialog([](QDialog *x){x->findChild<QLineEdit*>("text")->setText("R");x->findChild<QCheckBox*>("series")->setChecked(true);x->findChild<QSpinBox*>("seriesStart")->setValue(5);});
        click(view,{10.16,10.16});click(view,{10.16,10.16});click(view,{20.32,10.16});click(view,{30.48,10.16});
        require(els().size()==4&&els()[3].text=="R7"&&view->nextSeriesNumber()==8,"a series beside another text R6");
        editor.undo();editor.undo();require(view->nextSeriesNumber()==6,"the series' own R6 undone frees 6");
        editor.undo();require(view->nextSeriesNumber()==5,"and its first text 5");}

    // --- Design rule check: the checks the reference adds, each switchable, and the check of the visible part.
    {Board b=newBoard("DRC",80,40);b.multilayer=true;
        auto pad=[&](QPointF at,double size,double drill,int layer=CopperBottom){auto e=newElement(ElementType::Pad);e.pos=at;e.size=size;e.size2=drill;e.layer=layer;e.solderMask=true;updateOutline(e);b.elements<<e;return int(b.elements.size())-1;};
        auto line=[&](int layer,QPolygonF points,double width){auto e=newElement(ElementType::Track);e.layer=layer;e.points=points;e.width=width;b.elements<<e;return int(b.elements.size())-1;};
        const int a=pad({10,10},2,1),c=pad({11.5,10},2,1),far=pad({20,10},1.6,.8);                     // a and c: holes 0.5 mm apart from edge to edge
        const int thin=line(SilkTop,{{5,20},{15,20}},.1),hair=line(SilkBottom,{{5,25},{15,25}},0);line(SilkTop,{{5,30},{15,30}},.2);
        auto text=newElement(ElementType::Text);text.layer=SilkTop;text.text="T";text.pos={30,30};text.size=1;text.thickness=0;updateStrokes(text);b.elements<<text;const int small=int(b.elements.size())-1;
        auto hidden=text;hidden.role=TextRole::Designator;hidden.visible=false;hidden.pos={40,30};b.elements<<hidden;
        auto ring=newElement(ElementType::Circle);ring.layer=SilkBottom;ring.pos={50,30};ring.size=2;ring.width=.1;b.elements<<ring;const int circle=int(b.elements.size())-1;
        auto outline=newElement(ElementType::Area);outline.layer=SilkTop;outline.points={{60,25},{65,25},{65,30}};outline.width=0;b.elements<<outline;
        auto smd=newElement(ElementType::SmdPad);smd.layer=CopperTop;smd.pos={40,10};smd.size=2;smd.size2=2;smd.solderMask=true;updateOutline(smd);b.elements<<smd;const int smdPad=int(b.elements.size())-1;
        const int onSmd=pad({40.8,10},1.2,.6),beside=pad({45,10},1.2,.6);
        const int bare=pad({55,10},1.6,.8);b.elements[bare].solderMask=false;const int plain=pad({60,10},1,1);b.elements[plain].solderMask=false;
        const int topMask=line(CopperTop,{{5,35},{15,35}},.5),bottomMask=line(CopperBottom,{{20,35},{30,35}},.5),innerMask=line(Inner1,{{35,35},{45,35}},.5);
        for(int i:{topMask,bottomMask,innerMask})b.elements[i].solderMask=true;
        auto opening=newElement(ElementType::Area);opening.layer=CopperTop;opening.maskOnly=true;opening.points={{70,30},{75,30},{75,35}};b.elements<<opening;const int maskOnly=int(b.elements.size())-1;
        auto kinds=[](const QList<Finding> &found,const QString &part){QList<QList<int>> out;for(const auto &f:found)if(f.message.contains(part))out.append(f.elements);return out;};
        Rules rules;const auto found=checkDesign(b,rules);
        require(kinds(found,"Bohrungen näher")==QList<QList<int>>{{a,c}},"two holes 0.5 mm apart from edge to edge, the others far enough");
        for(const auto &f:found)if(f.message.contains("Bohrungen näher"))require(std::abs(f.area.left()-9.5)<1e-9&&std::abs(f.area.right()-12)<1e-9&&std::abs(f.area.top()-9.5)<1e-9,"the mark covers both holes");
        require(kinds(found,"Bestückungsdruck schmaler")==QList<QList<int>>({{thin},{hair},{small},{circle}}),"narrow silkscreen lines, a hairline, thin text strokes and a narrow ring; not a hidden designator or an area");
        require(kinds(found,"SMD-Pad")==QList<QList<int>>{{smdPad,onSmd}},"a hole on an SMD pad (of the other side too), not one beside it");
        require(kinds(found,"Pad ohne").isEmpty()&&kinds(found,"außerhalb").isEmpty(),"the solder mask checks are off at first");
        Q_UNUSED(far);Q_UNUSED(beside);
        rules.padsWithoutMask=rules.maskOutsidePads=true;auto masks=checkDesign(b,rules);
        require(kinds(masks,"Pad ohne")==QList<QList<int>>{{bare}},"a pad without its opening; a plain hole needs none");
        require(kinds(masks,"außerhalb")==QList<QList<int>>({{topMask},{bottomMask}}),"mask openings over tracks of both outer layers; not on an inner layer, not a mask-only area by itself");
        b.elements[maskOnly].solderMask=true;require(kinds(checkDesign(b,rules),"außerhalb").size()==3,"a mask-only area counts by its own opening switch");b.elements[maskOnly].solderMask=false;
        Rules off;off.holeDistanceOn=off.minSilkOn=off.holesOnSmd=off.silkOnPads=false;off.clearanceOn=off.minDrillOn=off.maxDrillOn=off.minTrackOn=off.minRingOn=false;
        require(checkDesign(b,off).isEmpty(),"every check switched off");
        off.minRingOn=true;off.minRing=.6;require(kinds(checkDesign(b,off),"Restring").size()==6,"one check alone");
        // Only what reaches into a window counts.
        Rules part;part.window=QRectF(35,5,15,10);const auto inWindow=checkDesign(b,part);
        require(kinds(inWindow,"SMD-Pad").size()==1&&kinds(inWindow,"Bohrungen näher").isEmpty()&&kinds(inWindow,"Bestückungsdruck schmaler").isEmpty(),"the check of a window");
        // A filled circle on the silkscreen is a disc without a line to check; a hidden designator is not printed over a
        // pad; silkscreen only touching a pad is marked where it touches.
        {Board s=newBoard("Druck",40,30);
            auto disc=newElement(ElementType::Circle);disc.layer=SilkTop;disc.pos={10,10};disc.size=2;disc.width=.1;disc.filled=true;s.elements<<disc;
            auto smd=newElement(ElementType::SmdPad);smd.layer=CopperTop;smd.pos={20,10};smd.size=2;smd.size2=2;smd.solderMask=true;updateOutline(smd);s.elements<<smd;
            auto name=newElement(ElementType::Text);name.layer=SilkTop;name.role=TextRole::Designator;name.visible=false;name.text="R1";name.pos={19.5,10.5};name.size=1.5;updateStrokes(name);s.elements<<name;
            auto edge=newElement(ElementType::Track);edge.layer=SilkTop;edge.points={{18,8.9},{22,8.9}};edge.width=.2;s.elements<<edge;
            const auto found=checkDesign(s,Rules{});
            require(kinds(found,"Bestückungsdruck schmaler").isEmpty(),"a filled disc has no line to be too narrow");
            for(const auto &f:found)if(f.message.contains("auf einem Pad"))
                require(!f.elements.contains(2)&&QRectF(17,7,6,6).contains(f.area.center())&&QRectF(17,7,6,6).contains(f.at),"no hidden designator, the mark at the pad");}
        // The panel: switches and limits, the whole board or the visible part, the findings marked and zoomed to.
        Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();Document d;d.boards={b};editor.setDocument(d);
        QApplication::processEvents();view->fitBoard();
        auto *list=editor.findChild<QListWidget*>("findings");auto *whole=editor.findChild<QPushButton*>("drcWhole"),*visible=editor.findChild<QPushButton*>("drcVisible");
        require(list&&whole&&visible&&editor.findChild<QCheckBox*>("check-holeDistance")->isChecked()&&std::abs(editor.findChild<QDoubleSpinBox*>("rule-holeDistance")->value()-.8)<1e-9
                &&!editor.findChild<QCheckBox*>("check-padsWithoutMask")->isChecked(),"the panel with the reference's switches and limits");
        whole->click();const int all=list->count();
        require(all==int(checkDesign(b,editor.designRules()).size())&&all==6&&view->shownFindings().size()==all&&list->selectedItems().size()==all,"all findings listed and marked");
        editor.findChild<QCheckBox*>("check-minSilk")->setChecked(false);whole->click();require(list->count()==all-4,"a check switched off in the panel");
        editor.findChild<QDoubleSpinBox*>("rule-holeDistance")->setValue(.4);whole->click();require(list->count()==all-5,"a limit changed in the panel");
        editor.findChild<QDoubleSpinBox*>("rule-holeDistance")->setValue(.8);whole->click();
        int row=-1;for(int k=0;k<list->count();k++)if(list->item(k)->text().contains("SMD-Pad"))row=k;
        list->setCurrentRow(row);Q_EMIT list->itemClicked(list->item(row));
        require(view->selection()==QList<int>({smdPad,onSmd})&&view->shownFindings()==QList<int>{row},"a click selects the finding's elements and shows its mark alone");
        const double before=view->scale();Q_EMIT list->itemDoubleClicked(list->item(row));require(view->scale()>before*2,"a double click zooms to the finding");
        visible->click();require(list->count()>=1&&list->count()<all&&list->item(0)->text().contains("SMD-Pad"),"only the visible part checked");
        editor.findChild<QPushButton*>("findingsAll")->click();require(view->shownFindings().size()==list->count(),"all findings shown again");}

    // --- View: zooming back, onto all elements and onto the selection; the overview; blinking test results.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Ansicht",100,80);b.grid=1.27;b.origin={0,0};
        auto p1=newElement(ElementType::Pad);p1.pos={70,60};updateOutline(p1);auto p2=p1;p2.pos={80,65};updateOutline(p2);
        auto t=newElement(ElementType::Track);t.points={{70,60},{80,65}};b.elements<<p1<<p2<<t;d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();
        const double fitted=view->scale();auto contains=[&](const QRectF &r){const QRectF a=view->visibleArea();return a.contains(r);};
        require(!view->canZoomBack()&&!editor.action("zoomPrevious")->isEnabled()&&!editor.action("zoomSelection")->isEnabled(),"nothing to go back to, nothing selected");
        const QPointF middle(view->width()/2.0,view->height()/2.0);
        editor.action("zoomIn")->trigger();const double once=view->scale();editor.action("zoomIn")->trigger();
        require(view->scale()>once&&once>fitted&&editor.action("zoomPrevious")->isEnabled(),"zooming in keeps the views before");
        editor.action("zoomPrevious")->trigger();require(std::abs(view->scale()-once)<1e-9,"back to the view before the last zoom");
        editor.action("zoomPrevious")->trigger();require(std::abs(view->scale()-fitted)<1e-9&&!view->canZoomBack(),"and to the first one");
        editor.action("zoomElements")->trigger();
        require(contains(QRectF(69,59,12,7))&&view->scale()>fitted*3&&!contains(QRectF(0,0,10,10)),"zoomed onto all elements");
        editor.action("zoomBoard")->trigger();require(std::abs(view->scale()-fitted)<1e-9,"the whole board");
        view->setSelection({0});require(editor.action("zoomSelection")->isEnabled(),"a selection to zoom to");editor.action("zoomSelection")->trigger();
        require(contains(bounds(p1))&&!contains(bounds(p2))&&view->scale()>fitted*5,"zoomed onto the selection");
        editor.action("zoomPrevious")->trigger();require(std::abs(view->scale()-fitted)<1e-9,"back from the selection to the whole board");
        editor.action("zoomPrevious")->trigger();require(contains(QRectF(69,59,12,7))&&view->scale()>fitted*3,"and to all elements: the views kept in order");
        // A run of wheel steps is one zoom to go back from.
        while(view->canZoomBack())view->zoomBack();view->fitBoard();
        auto wheel=[&](int delta){QWheelEvent e(middle,view->mapToGlobal(middle),QPoint(),QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(view,&e);};
        wheel(120);wheel(120);wheel(120);require(view->scale()>fitted*1.9,"the wheel zooms");view->zoomBack();
        require(std::abs(view->scale()-fitted)<1e-9&&!view->canZoomBack(),"three quick wheel steps go back at once");
        // Seen from below, going back shows the same part of the board.
        view->setSelection({0});editor.action("zoomSelection")->trigger();const QPointF zoomed=view->toBoard(middle);
        editor.action("zoomIn")->trigger();view->setFromBelow(true);editor.action("zoomPrevious")->trigger();
        require(QLineF(view->toBoard(middle),zoomed).length()<1e-6,"going back from below shows the part shown before");
        view->setFromBelow(false);view->setSelection({});while(view->canZoomBack())view->zoomBack();view->fitBoard();
        // The overview: the board and the part shown; dragging moves the view, clicks zoom or move it.
        auto *overview=dynamic_cast<BoardOverview*>(editor.findChild<QWidget*>("overview"));require(overview&&overview->isVisible(),"the overview below the tools");
        require(overview->shownRect().contains(overview->boardRect().adjusted(1,1,-1,-1)),"the whole board shown at first");
        auto mouse=[&](QEvent::Type type,QPointF at,Qt::MouseButton button=Qt::LeftButton){
            QMouseEvent e(type,at,overview->mapToGlobal(at),type==QEvent::MouseMove?Qt::NoButton:button,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::MouseButtons(button),Qt::NoModifier);
            QApplication::sendEvent(overview,&e);};
        auto clickAt=[&](QPointF at,Qt::MouseButton button=Qt::LeftButton){mouse(QEvent::MouseButtonPress,at,button);mouse(QEvent::MouseButtonRelease,at,button);};
        clickAt(overview->shownRect().center());require(std::abs(view->scale()-1.4*fitted)<1e-9,"a click into the shown part zooms in by 1.4");
        clickAt(overview->shownRect().center(),Qt::RightButton);require(std::abs(view->scale()-fitted)<1e-9,"a right click zooms out");
        clickAt(overview->shownRect().center());clickAt(overview->shownRect().center());
        auto centre=[&]{return view->toBoard(QPointF(view->width()/2.0,view->height()/2.0));};
        const QRectF shown=overview->shownRect();const QPointF before=centre();
        mouse(QEvent::MouseButtonPress,shown.center());mouse(QEvent::MouseMove,shown.center()+QPointF(15,0));mouse(QEvent::MouseMove,shown.center()+QPointF(20,0));
        mouse(QEvent::MouseButtonRelease,shown.center()+QPointF(20,0));
        const double perMm=overview->boardRect().width()/100;const QPointF after=centre();
        require(std::abs(after.x()-before.x()-20/perMm)<1e-6&&std::abs(after.y()-before.y())<1e-6&&std::abs(view->scale()-1.96*fitted)<1e-9,"dragging the shown part moves the view");
        view->zoomBack();require(std::abs(centre().x()-before.x())<1e-6,"the drag goes back as one view");
        const QPointF corner=overview->boardRect().topLeft()+QPointF(5,5);clickAt(corner);
        require(QLineF(centre(),QPointF(5/perMm,5/perMm)).length()<1e-6,"a click beside the shown part moves the view there");
        editor.action("overview")->trigger();require(!overview->isVisible()&&!editor.action("overview")->isChecked(),"the overview hidden");
        {QSettings s(preferencesFile,QSettings::IniFormat);require(!s.value("showOverview").toBool(),"kept with the preferences");}
        editor.action("overview")->trigger();require(overview->isVisible(),"and shown again");
        // The zoom tool zooms in by 1.4 as well, a right click out.
        view->fitBoard();view->setTool(BoardView::Tool::Zoom);click(view,{50,40});require(std::abs(view->scale()-1.4*fitted)<1e-9,"the zoom tool zooms in by 1.4");
        click(view,{50,40},Qt::RightButton);require(std::abs(view->scale()-fitted)<1e-9,"and out again");view->setTool(BoardView::Tool::Select);
        // Blinking test results.
        view->blinkTest=true;view->setTool(BoardView::Tool::Test);click(view,{75,62.5});require(view->tested().size()==3&&view->testShown(),"the test finds pad, track and pad");
        bool changed=false;for(int k=0;k<12&&!changed;k++){QEventLoop wait;QTimer::singleShot(100,&wait,&QEventLoop::quit);wait.exec();changed=!view->testShown();}
        require(changed,"test results blink");
        click(view,{20,20});require(view->tested().isEmpty()&&view->testShown(),"no test result, nothing blinks");
        view->blinkTest=false;click(view,{75,62.5});for(int k=0;k<6;k++){QEventLoop wait;QTimer::singleShot(100,&wait,&QEventLoop::quit);wait.exec();require(view->testShown(),"steady test results");}}

    // --- New boards as the reference sets them up: a working area only, or a rectangular or round outline with a margin.
    {auto pick=[](const QString &kind,std::function<void(QDialog*)> more={}){inDialog([=](QDialog *x){x->findChild<QRadioButton*>(kind)->setChecked(true);if(more)more(x);});};
        pick("newPlain",[](QDialog *x){x->findChild<QDoubleSpinBox*>("newWidth")->setValue(80);x->findChild<QDoubleSpinBox*>("newHeight")->setValue(50);});
        const auto plain=askNewBoard(nullptr,"P");require(plain&&plain->width==80&&plain->height==50&&plain->elements.isEmpty()&&plain->name=="P","only a working area");
        QString area;
        pick("newRectangle",[&area](QDialog *x){x->findChild<QDoubleSpinBox*>("newMargin")->setValue(5);area=x->findChild<QLabel*>("newArea")->text();x->findChild<QLineEdit*>("newName")->setText("Rechteck");});
        const auto rect=askNewBoard(nullptr,"P");
        require(rect&&rect->width==170&&rect->height==110&&rect->name=="Rechteck"&&rect->elements.size()==1,"a rectangle of 160 × 100 mm with a margin of 5 mm");
        const auto &o=rect->elements[0];
        require(o.type==ElementType::Track&&o.layer==Outline&&o.points.size()==5&&o.points[0]==QPointF(5,5)&&o.points[2]==QPointF(165,105)&&o.points[4]==o.points[0]&&o.width==0,
                "its closed outline on the outline layer, without width as in the reference");
        require(rect->origin==QPointF(5,105),"the origin at the outline's bottom left corner");
        require(area==ui("%1 × %2 mm").arg(uiLocale().toString(170.0,'f',2),uiLocale().toString(110.0,'f',2)),"the dialog shows the working area");
        pick("newRound",[](QDialog *x){x->findChild<QDoubleSpinBox*>("newDiameter")->setValue(60);});
        const auto round=askNewBoard(nullptr,"P");
        require(round&&round->width==100&&round->height==100&&round->elements.size()==1&&round->elements[0].type==ElementType::Circle&&round->elements[0].layer==Outline
                &&round->elements[0].pos==QPointF(50,50)&&round->elements[0].size==30&&round->elements[0].width==0&&round->origin==QPointF(20,80),
                "a round outline of 60 mm with the margin of 20 mm, the origin at the bottom left corner of the square round it");
        {NewBoard spec;spec.kind=NewBoard::Rectangle;spec.margin=5;spec.originTopLeft=true;require(newBoard("T",spec).origin==QPointF(5,5),"or at its top left corner");
            spec.kind=NewBoard::Plain;require(newBoard("T",spec).origin==QPointF(0,0)&&newBoard("T",NewBoard{}).origin==QPointF(0,100),"a plain working area at its own corner");}
        {auto *timer=new QTimer;timer->setInterval(5);
            QObject::connect(timer,&QTimer::timeout,[timer]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget())){timer->stop();timer->deleteLater();d->reject();}});timer->start();}
        require(!askNewBoard(nullptr,"P"),"cancelled: no board");
        // The editor asks for the board of a new document and of a board added.
        Editor editor;editor.show();QApplication::processEvents();pick("newRectangle");editor.action("new")->trigger();
        require(editor.document().boards.size()==1&&editor.document().board().elements.size()==1&&editor.document().board().width==200,"a new document with its outline");
        pick("newRound");editor.action("addBoard")->trigger();
        require(editor.document().boards.size()==2&&editor.document().board().elements[0].type==ElementType::Circle,"a board added with a round outline");
        editor.undo();require(editor.document().boards.size()==1,"adding is one undo step");}

    // --- Commands of the reference's popup menu and of the properties: names, layers, the origin, elements outside,
    // the component dialog, dissolving, the multiple selection and switches for differing elements.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Bearbeiten",60,40);b.grid=1.27;
        auto pad=[&](QPointF at,int part){auto e=newElement(ElementType::Pad);e.pos=at;e.part=part;updateOutline(e);b.elements<<e;};
        pad({10,10},1);pad({15,10},1);pad({30,10},0);                                                     // 0, 1, 2
        auto t=newElement(ElementType::Track);t.points={{5,30},{25,30}};b.elements<<t;                      // 3
        auto outside=newElement(ElementType::Track);outside.points={{70,5},{80,5}};b.elements<<outside;     // 4 wholly outside
        auto label=[&](const QString &text,TextRole role,QPointF at){auto e=newElement(ElementType::Text);e.text=text;e.role=role;e.part=1;e.pos=at;e.size=1.5;e.layer=SilkTop;updateStrokes(e);b.elements<<e;};
        label("R1",TextRole::Designator,{10,7});label("10k",TextRole::Value,{10,14});                       // 5, 6
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};
        // A name for the selection and the elements of a name.
        view->setSelection({0,2});require(editor.nameSelection("GND")&&els()[0].name=="GND"&&els()[2].name=="GND"&&editor.canUndo(),"a name for the selection, one undo step");
        view->setSelection({2});require(editor.nameToSelect()=="GND","the name of the selection");
        require(editor.selectByName("GND")&&view->selection()==QList<int>({0,2}),"all elements of that name selected");
        require(!editor.selectByName("gnd")&&view->selection().isEmpty(),"the name exactly");
        // Onto a layer: pads stay on copper; the origin into a corner.
        view->setSelection({2,3});require(editor.setSelectionLayer(SilkTop)&&els()[2].layer==CopperBottom&&els()[3].layer==SilkTop,"onto a layer, pads stay on copper");
        require(editor.setOrigin({0,0})&&editor.document().board().origin==QPointF(0,0)&&!editor.setOrigin({0,0}),"the origin top left, no step without a change");
        // Elements wholly outside the working area.
        require(editor.deleteOutside()==1&&els().size()==6&&editor.deleteOutside()==0,"the element outside deleted");
        require(editor.findChild<QAction*>("rotationStep5"),"a rotation step of 5°");
        // The component dialog: the layers of designator and value, the look of both texts, a quick angle.
        inDialog([](QDialog *x){auto *layer=x->findChild<QComboBox*>("nameLayer");layer->setCurrentIndex(layer->findData(SilkBottom));
            x->findChild<QDoubleSpinBox*>("componentTextHeight")->setValue(2.5);x->findChild<QComboBox*>("componentTextStyle")->setCurrentIndex(2);
            for(auto *button:x->findChildren<QToolButton*>())if(button->objectName()=="angle270")button->click();});
        require(editor.editComponent(4),"the component dialog");
        require(els()[4].layer==SilkBottom&&els()[4].size==2.5&&els()[4].style==2&&els()[5].size==2.5&&els()[4].componentRotation==270,"layer, height, style and angle");
        require(editor.dissolveComponent(4)&&els()[4].role==TextRole::Plain&&els()[5].role==TextRole::Plain&&els()[0].part==0&&components(editor.document().board()).isEmpty(),
                "the component dissolved into plain elements");
        editor.undo();require(components(editor.document().board()).size()==1,"one undo step");
        // A selection of several kinds: the kinds with their numbers; edits go to the kind shown.
        view->setSelection({0,1,3});QApplication::processEvents();QApplication::processEvents();
        auto shownList=[&]{QListWidget *found=nullptr;for(auto *l:editor.findChildren<QListWidget*>("multipleSelection"))if(!l->isHidden())found=l;return found;};
        auto *kinds=shownList();require(kinds&&kinds->count()==2&&kinds->item(0)->text().contains(ui("Lötaugen"))&&kinds->item(1)->text().contains(ui("Leiterbahn")),"the kinds of a multiple selection");
        kinds->setCurrentRow(1);QApplication::processEvents();QApplication::processEvents();kinds=shownList();require(kinds&&kinds->currentRow()==1,"the tracks picked");
        QDoubleSpinBox *first=nullptr;for(auto *w:editor.findChildren<QDoubleSpinBox*>())if(!w->isHidden()&&w->parentWidget()==kinds->parentWidget()&&!first)first=w;
        require(first,"a field of the tracks");first->setValue(.7);
        require(els()[3].clearance==.7&&els()[0].clearance!=.7&&view->selection()==QList<int>({0,1,3}),"only the tracks changed, the selection stays");
        // A switch for elements that differ shows grey.
        editor.editElements({1},[](Element &x){x.solderMask=false;});view->setSelection({0,1});QApplication::processEvents();QApplication::processEvents();
        QCheckBox *mask=nullptr;for(auto *c:editor.findChildren<QCheckBox*>())if(!c->isHidden()&&c->text()==ui("Lötstopp offen"))mask=c;
        require(mask&&mask->checkState()==Qt::PartiallyChecked,"grey where the elements differ");}

    // --- Special shapes from their dialog: a frame of the board's size goes straight around it as one group on the active
    // layer. The circle arrangement shows the angle its copies span; its middle button puts the start point back.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Formen",100,80);b.activeLayer=SilkTop;d.boards={b};editor.setDocument(d);QApplication::processEvents();
        inDialog([](QDialog *x){x->findChild<QTabWidget*>("shapeKinds")->setCurrentIndex(2);
            for(auto *button:x->findChildren<QPushButton*>())if(button->text()==ui("Wie die Platine"))button->click();
            x->findChild<QSpinBox*>("frameColumns")->setValue(4);x->findChild<QComboBox*>("frameRowSides")->setCurrentIndex(0);});
        editor.action("specialShape")->trigger();
        const auto &els=editor.document().board().elements;int tracks=0,texts=0;QSet<int> groups;
        for(const auto &e:els){(e.type==ElementType::Track?tracks:texts)++;for(int g:e.groups)groups.insert(g);}
        require(tracks==1+2+3*2&&texts==8&&groups.size()==1&&std::all_of(els.begin(),els.end(),[](const Element &e){return e.groups.size()==1&&e.layer==SilkTop;}),
                "a frame from the dialog: one group on the active layer");
        require(near(els[0].points[0],{0,0})&&near(els[0].points[2],{100,80})&&editor.canUndo(),"around the working area, one undo step");
        // The polygon tab: rays to the corners come along.
        int before=int(els.size());
        inDialog([](QDialog *x){x->findChild<QTabWidget*>("shapeKinds")->setCurrentIndex(0);x->findChild<QSpinBox*>("polygonCorners")->setValue(5);
            x->findChild<QCheckBox*>("polygonRays")->setChecked(true);});
        editor.action("specialShape")->trigger();click(view,{50,40});QApplication::processEvents();
        require(editor.document().board().elements.size()==before+6,"a pentagon with five rays placed");
        QString range;double startAfter=-1;view->setSelection({before});
        inDialog([&](QDialog *x){x->findChild<QTabWidget*>()->setCurrentIndex(1);x->findChild<QSpinBox*>("circleCount")->setValue(4);x->findChild<QDoubleSpinBox*>("circleAngle")->setValue(30);
            range=x->findChild<QLabel*>("circleRange")->text();x->findChild<QDoubleSpinBox*>("circleStartX")->setValue(5);
            x->findChild<QToolButton*>("circleMiddle")->click();startAfter=x->findChild<QDoubleSpinBox*>("circleStartX")->value();});
        editor.action("arrange")->trigger();
        require(range.contains("90")&&startAfter==0&&editor.document().board().elements.size()==before+6+3,"the angle spanned, the middle again, three copies");}

    // --- The print preview: tiles, correction factors for the session, settings of the special layers, the clipboard.
    {Board b=newBoard("Druck",20,10);auto pad=newElement(ElementType::Pad);pad.pos={5,5};pad.size=3;pad.size2=1;updateOutline(pad);b.elements<<pad;
        PrintPreview preview(b,defaultPrintSettings(b),"druck.olpcb");
        inDialog([](QDialog *x){x->findChild<QSpinBox*>("tilesX")->setValue(3);x->findChild<QSpinBox*>("tilesY")->setValue(2);x->findChild<QDoubleSpinBox*>("tileGap")->setValue(4);});
        preview.findChild<QPushButton*>("tiles")->click();
        require(preview.settings().tilesX==3&&preview.settings().tilesY==2&&preview.settings().tileGap==4,"tiles chosen in their dialog");
        inDialog([](QDialog *x){x->findChild<QLineEdit*>("correctionX")->setText("1,1");x->findChild<QLineEdit*>("correctionY")->setText("0.9");});
        preview.findChild<QPushButton*>("correction")->click();require(PrintPreview::correction==QPointF(1.1,.9),"correction factors with comma or point");
        {PrintPreview again(b,defaultPrintSettings(b),"druck.olpcb");QString shown;
            inDialog([&shown](QDialog *x){shown=x->findChild<QLineEdit*>("correctionX")->text();});again.findChild<QPushButton*>("correction")->click();
            require(shown==uiLocale().toString(1.1,'f',5),"they hold for the session");}
        PrintPreview::correction={1,1};
        inDialog([](QDialog *x){x->findChild<QCheckBox*>("padMaskOn")->setChecked(false);x->findChild<QDoubleSpinBox*>("smdMask")->setValue(.2);x->findChild<QDoubleSpinBox*>("drillTextHeight")->setValue(2);});
        preview.findChild<QPushButton*>("specialSettings")->click();
        require(!preview.settings().maskPads&&preview.settings().smdMask==.2&&preview.settings().drillTextHeight==2&&preview.settings().padMask==.3,"settings of the special layers");
        preview.findChild<QPushButton*>("clipboard")->click();
        require(QApplication::clipboard()->image().size()==printPicture(b,preview.settings()).size(),"the printout on the clipboard");}

    // --- Board commands: a copy, the first and the last place, boards of a file after the active one or at the end, the
    // active board alone in a file.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();
        Document d;Board a=newBoard("A",50,40),b=newBoard("B",60,40);
        auto pad=newElement(ElementType::Pad);pad.pos={10,10};pad.part=1;updateOutline(pad);
        auto name=newElement(ElementType::Text);name.text="R1";name.role=TextRole::Designator;name.part=1;name.pos={10,14};name.size=1.5;updateStrokes(name);
        a.elements<<pad<<name;d.boards={a,b};editor.setDocument(d);const auto &doc=editor.document();
        const QString part=doc.boards[0].elements[1].component;require(!part.isEmpty(),"the component has an identifier");
        require(editor.copyBoard()&&doc.boards.size()==3&&doc.activeBoard==1&&doc.boards[1].name==ui("%1 (Kopie)").arg("A")&&doc.boards[1].elements.size()==2,"a copy right after the active board");
        require(doc.boards[1].id!=doc.boards[0].id&&doc.boards[1].elements[1].component!=part&&doc.boards[0].elements[1].component==part,"the copy and its component get identifiers of their own");
        editor.undo();require(doc.boards.size()==2,"one undo step");
        editor.switchBoard(0);require(editor.moveBoard(true)&&doc.boards[1].name=="A"&&doc.activeBoard==1,"to the last place");
        require(!editor.moveBoard(true),"nothing to move at the end");require(editor.moveBoard(false)&&doc.boards[0].name=="A"&&doc.activeBoard==0,"to the first place");
        QTemporaryDir dir;const QString file=dir.filePath("andere.olpcb");Document other;other.boards={newBoard("X",30,20),newBoard("Y",30,20)};save(other,file);
        require(editor.insertBoards(file,false)&&doc.boards.size()==4&&doc.boards[1].name=="X"&&doc.boards[2].name=="Y"&&doc.boards[3].name=="B"&&doc.activeBoard==1,"boards of a file after the active one");
        const QString idB=doc.boards[3].id;
        require(editor.insertBoards(file,true)&&doc.boards.size()==6&&doc.boards[4].name=="X"&&doc.activeBoard==4&&doc.boards[3].id==idB,"at the end; the boards here keep their identifiers");
        {QSet<QString> ids;for(const auto &x:doc.boards)ids.insert(x.id);require(ids.size()==6,"every board an identifier of its own");}
        QString error;require(!editor.insertBoards(dir.filePath("fehlt.olpcb"),true,&error)&&!error.isEmpty()&&doc.boards.size()==6,"a missing file changes nothing");
        editor.switchBoard(0);const QString alone=dir.filePath("allein.olpcb");require(editor.saveBoardAs(alone),"the active board saved alone");
        const Document read=load(alone);require(read.boards.size()==1&&read.boards[0].name=="A"&&read.boards[0].elements.size()==2&&doc.boards.size()==6,"a file with that board only, the document as it was");}

    // --- Without a selection the properties show the board: name, size and inner layers edited there, its area.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Panel",60,40);d.boards={b};editor.setDocument(d);view->setSelection({});QApplication::processEvents();QApplication::processEvents();
        auto shown=[&](const QString &name)->QWidget*{for(auto *w:editor.findChildren<QWidget*>(name))if(!w->isHidden())return w;return nullptr;};
        auto settle=[]{QApplication::processEvents();QApplication::processEvents();};
        auto *area=qobject_cast<QLabel*>(shown("boardArea"));require(area&&area->text()==ui("%1 cm²").arg(uiLocale().toString(24.0,'g',12)),"the board's area in square centimetres");
        auto *width=qobject_cast<QDoubleSpinBox*>(shown("boardWidth"));require(width&&width->value()==60&&qobject_cast<QDoubleSpinBox*>(shown("boardHeight"))->value()==40,"width and height to edit");
        width->setValue(80.5);settle();require(editor.document().board().width==80.5&&editor.canUndo(),"the width edited in the panel");
        area=qobject_cast<QLabel*>(shown("boardArea"));require(area&&area->text()==ui("%1 cm²").arg(uiLocale().toString(32.2,'g',12)),"the area follows");
        auto *name=qobject_cast<QLineEdit*>(shown("boardName"));require(name&&name->text()=="Panel","the name to edit");
        name->setText("Neu");Q_EMIT name->editingFinished();settle();require(editor.document().board().name=="Neu","the name edited in the panel");
        auto *inner=qobject_cast<QCheckBox*>(shown("boardMultilayer"));require(inner&&!inner->isChecked(),"the inner layers to switch");
        inner->setChecked(true);settle();require(editor.document().board().multilayer,"the inner layers switched on in the panel");
        name=qobject_cast<QLineEdit*>(shown("boardName"));Q_EMIT name->editingFinished();settle();
        editor.undo();editor.undo();editor.undo();const auto &now=editor.document().board();
        require(now.width==60&&now.name=="Panel"&&!now.multilayer&&!editor.canUndo(),"each change one undo step, a name left as it was none");}
    return 0;
}
