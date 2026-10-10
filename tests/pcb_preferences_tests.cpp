// Preference tests of the PCB module: the dialog and what each setting does (units, drill holes, colour schemes, the
// cross hair, folders, undo steps, backups, fabrication data from the origin or the corner, the text limit, Imax).
// The preferences go to a file of the test (Editor::setPreferencesFile), never to the user's.
#include "language.h"
#include "modules/pcb/editor.h"
#include "modules/pcb/fabrication.h"
#include "modules/pcb/font.h"
#include "modules/pcb/milling.h"
#include "modules/pcb/outputs.h"
#include "modules/pcb/macropanel.h"
#include "formats/sprint/sprint.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <cmath>
#include <stdexcept>

using namespace openloch;
using namespace openloch::pcb;
namespace {
void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
void send(BoardView *view,QEvent::Type type,QPointF mm,Qt::MouseButton button=Qt::LeftButton){
    const QPointF at=view->toPixel(mm);const auto buttons=type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::MouseButtons(button);
    QMouseEvent event(type,at,view->mapToGlobal(at),type==QEvent::MouseMove?Qt::NoButton:button,type==QEvent::MouseMove?Qt::NoButton:buttons,Qt::NoModifier);
    QApplication::sendEvent(view,&event);
}
void click(BoardView *view,QPointF mm){send(view,QEvent::MouseMove,mm,Qt::NoButton);send(view,QEvent::MouseButtonPress,mm);send(view,QEvent::MouseButtonRelease,mm);}
// Fills in the next modal dialog with `fill` and accepts it.
void inDialog(const std::function<void(QDialog*)> &fill,bool accept=true){
    auto *timer=new QTimer;timer->setInterval(5);
    QObject::connect(timer,&QTimer::timeout,[timer,fill,accept]{
        if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){timer->stop();timer->deleteLater();fill(dialog);if(accept)dialog->accept();else dialog->reject();}});
    timer->start();
}
template<class W> W *child(QWidget *in,const char *name){auto *w=in->findChild<W*>(name);if(!w)throw std::runtime_error(std::string("missing widget ")+name);return w;}
}

int preferencesTests(const QString &preferencesFile){
    // --- The dialog: every page set, applied, kept and read back by another editor.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        QTemporaryDir folder;editor.userColours[0].layers[CopperBottom]=QColor(1,2,3);
        inDialog([&](QDialog *d){
            child<QComboBox>(d,"units")->setCurrentIndex(1);child<QComboBox>(d,"holes")->setCurrentIndex(2);child<QCheckBox>(d,"showOverview")->setChecked(false);
            child<QCheckBox>(d,"blinkTest")->setChecked(true);child<QCheckBox>(d,"limitTextSize")->setChecked(true);child<QCheckBox>(d,"camOrigin")->setChecked(false);
            child<QComboBox>(d,"colourScheme")->setCurrentIndex(1);
            child<QLineEdit>(d,"folder-0")->setText(folder.path());child<QCheckBox>(d,"oneFolder")->setChecked(true);child<QLineEdit>(d,"macroFolder")->setText(folder.path());
            child<QSpinBox>(d,"undoSteps")->setValue(3);child<QDoubleSpinBox>(d,"copperThickness")->setValue(70);child<QDoubleSpinBox>(d,"temperatureRise")->setValue(10);
            child<QCheckBox>(d,"crossDiagonals")->setChecked(true);child<QCheckBox>(d,"crossCoordinates")->setChecked(true);child<QRadioButton>(d,"crossWhite")->setChecked(true);
            child<QCheckBox>(d,"autosave")->setChecked(true);child<QSpinBox>(d,"autosaveMinutes")->setValue(2);});
        editor.action("preferences")->trigger();
        require(view->milUnits&&view->holes==2&&!editor.action("overview")->isChecked()&&view->blinkTest&&editor.limitTextSize&&!editor.camOrigin,"the general settings applied");
        require(editor.colourScheme==1&&view->colours.layers[CopperBottom]==QColor(1,2,3)&&view->colours.layers[CopperTop]==Colours::standard().layers[CopperTop],"a scheme of one's own");
        require(editor.startFolder(Editor::Pictures)==folder.path()&&editor.macroFolder==folder.path(),"one folder for all kinds of files");
        require(editor.undoLimit==3&&editor.copperThickness==70&&editor.temperatureRise==10,"undo steps and Imax settings");
        require(view->crosshair.lines&&view->crosshair.diagonals&&view->crosshair.coordinates&&view->crosshair.whiteBox&&editor.autosave&&editor.autosaveMinutes==2,"cross hair and backups");
        Editor other;other.loadPreferences();
        require(other.view()->milUnits&&other.view()->holes==2&&!other.action("overview")->isChecked()&&other.view()->blinkTest&&other.limitTextSize&&!other.camOrigin,"read back: general");
        require(other.colourScheme==1&&other.view()->colours==view->colours&&other.userColours==editor.userColours,"read back: colours");
        require(other.folders==editor.folders&&other.oneFolder&&other.macroFolder==folder.path()&&other.undoLimit==3&&other.copperThickness==70&&other.temperatureRise==10,"read back: folders, undo, Imax");
        require(other.view()->crosshair==view->crosshair&&other.autosave&&other.autosaveMinutes==2,"read back: cross hair and backups");
        // The standard scheme stays as it is; "standard colours" puts them into a scheme of one's own.
        bool fixed=false;
        inDialog([&](QDialog *d){auto *scheme=child<QComboBox>(d,"colourScheme");scheme->setCurrentIndex(0);
            fixed=!child<QPushButton>(d,"standardColours")->isEnabled()&&!d->findChild<QWidget*>("colour-2")->isEnabled();
            scheme->setCurrentIndex(1);child<QPushButton>(d,"standardColours")->click();});
        editor.action("preferences")->trigger();require(fixed,"the standard scheme cannot be changed");require(editor.colourScheme==1&&view->colours==Colours::standard()&&editor.userColours[0]==Colours::standard(),"standard colours taken into one's own scheme");
        // Cancel changes nothing.
        inDialog([&](QDialog *d){child<QComboBox>(d,"units")->setCurrentIndex(0);child<QSpinBox>(d,"undoSteps")->setValue(50);},false);editor.action("preferences")->trigger();
        require(view->milUnits&&editor.undoLimit==3,"cancelling the dialog changes nothing");
        // Back to the defaults for the tests after this one.
        QFile::remove(preferencesFile);}

    // --- Undo steps: no more than set, fewer at once when lowered.
    {Editor editor;Document d;Board b=newBoard("Rückgängig",40,30);d.boards={b};editor.setDocument(d);
        for(int k=0;k<6;k++)editor.setGroundPlane(k%2==0);
        int steps=0;while(editor.canUndo()){editor.undo();steps++;}require(steps==6,"six steps kept");
        for(int k=0;k<6;k++)editor.setGroundPlane(k%2==0);editor.setUndoLimit(4);steps=0;while(editor.canUndo()){editor.undo();steps++;}require(steps==4,"lowering the limit drops the oldest steps");
        editor.setUndoLimit(0);require(editor.undoLimit==1,"at least one step");}

    // --- Backups: next to the document's own file, only after changes, never through a host.
    {QTemporaryDir dir;Editor editor;Document d;Board b=newBoard("Sicherung",40,30);d.boards={b};editor.setDocument(d);
        const QString file=dir.filePath("platine.olpcb");QString error;require(editor.saveFile(file,&error),"saved");
        require(editor.backupFile()==file+".bak"&&!editor.autosaveNow(),"no backup without a change");
        editor.setGroundPlane(true);require(editor.autosaveNow()&&QFile::exists(file+".bak"),"a backup after a change");
        require(load(file+".bak")==editor.document()&&editor.isModified(),"the backup holds the document, which stays modified");
        require(!editor.autosaveNow(),"no second backup without a further change");
        editor.setGroundPlane(false);require(editor.saveFile(file,&error)&&!editor.autosaveNow(),"saving makes a backup unnecessary");
        editor.setGroundPlane(true);editor.saveHandler=[](bool){return true;};require(!editor.autosaveNow(),"no backup when a host saves");editor.saveHandler={};
        Editor fresh;fresh.setGroundPlane(true);require(fresh.backupFile().isEmpty()&&!fresh.autosaveNow(),"no backup without a file");
        // Grid and layers are changes as well.
        require(editor.saveFile(file,&error)&&!editor.autosaveNow(),"saved again");
        auto *grid=editor.findChild<QComboBox*>("grid");require(grid&&grid->findData(.5)>=0&&std::abs(editor.document().board().grid-.5)>1e-9,"another grid to choose");
        grid->setCurrentIndex(grid->findData(.5));require(editor.isModified()&&editor.autosaveNow(),"a new grid backs up");
        // A layout opened from Sprint-Layout backs up next to its file, under the name it would be saved as.
        const QString sprintFile=dir.filePath("sprint.lay6");require(editor.exportSprint(sprintFile,&error),"Sprint-Layout file written");
        Editor imported;require(imported.openFile(sprintFile,&error)&&imported.backupFile()==dir.filePath("sprint.olpcb.bak")&&!imported.autosaveNow(),"opened, no backup yet");
        {const auto &shown=imported.document().board();imported.setGroundPlane(!shown.groundPlane[shown.activeLayer]);}
        require(imported.isModified()&&imported.autosaveNow(),"a backup beside the Sprint-Layout file");
        require(load(dir.filePath("sprint.olpcb.bak"))==imported.document(),"the backup holds the layout");
        editor.setAutosave(true,0);require(editor.autosaveMinutes==1,"at least a minute");editor.setAutosave(false,90);require(editor.autosaveMinutes==60,"at most an hour");}

    // --- Fabrication data from the origin or from the top left corner of the working area, the same in every output.
    {Board b=newBoard("Ursprung",40,30);b.origin={10,20};
        auto pad=newElement(ElementType::Pad);pad.pos={15,15};pad.size=1.6;pad.size2=.8;updateOutline(pad);b.elements<<pad;
        auto id=newElement(ElementType::Text);id.role=TextRole::Designator;id.text="J1";id.layer=SilkTop;id.pos={14,12};id.part=1;updateStrokes(id);b.elements[0].part=1;b.elements<<id;
        GerberSettings g;require(gerber(b,CopperBottom,g).contains("X5000000Y5000000D03"),"Gerber from the origin");
        g.fromOrigin=false;const QByteArray corner=gerber(b,CopperBottom,g);require(corner.contains("X15000000Y-15000000D03")&&corner.contains("top left corner"),"Gerber from the top left corner");
        DrillSettings x;x.metric=true;x.integerDigits=x.decimalDigits=3;x.decimalPoint=true;require(excellon(b,x).contains("X5.000Y5.000"),"drill data from the origin");
        x.fromOrigin=false;require(excellon(b,x).contains("X15.000Y-15.000"),"drill data from the top left corner");
        ComponentDataSettings c;c.fields={FieldDesignator,FieldX,FieldY};c.header=false;c.throughHoleParts=true;require(componentData(b,c).startsWith("J1,5.00,5.00"),"component data from the origin");
        c.fromOrigin=false;require(componentData(b,c).startsWith("J1,15.00,-15.00"),"component data from the top left corner");
        MillingSettings m;m.drillSide=2;m.drillMode=1;m.bottomMirror=0;m.bottom=false;const auto jobs=millingJobs(b,m);
        const double unit=1/.0254;auto at=[&](double mm){return QByteArray::number(qint64(std::llround(mm*unit)));};
        require(hpgl(b,jobs,m).contains("PU"+at(5)+","+at(5)),"milling from the origin");m.fromOrigin=false;
        require(hpgl(b,millingJobs(b,m),m).contains("PU"+at(15)+","+at(-15)),"milling from the top left corner");
        // The holes are taken in an order that starts at the machine's zero: the origin, or the corner.
        {Board two=b;auto corner=newElement(ElementType::Pad);corner.pos={2,2};corner.size=1.6;corner.size2=.8;updateOutline(corner);two.elements<<corner;
            MillingSettings n;n.drillSide=2;n.drillMode=1;n.bottomMirror=0;n.bottom=false;
            auto first=[&](const MillingSettings &x){for(const auto &j:millingJobs(two,x))if(j.kind==MillingJobKind::Drill&&!j.plunges.isEmpty())return j.plunges.first();return QPointF(-1,-1);};
            require(QLineF(first(n),{15,15}).length()<1e-9,"from the origin the hole nearest to it first");
            n.fromOrigin=false;require(QLineF(first(n),{2,2}).length()<1e-9,"from the corner the hole nearest to that");}
        // The editor hands its switch to the output dialogs.
        Editor editor;Document d;d.boards={b};editor.setDocument(d);editor.camOrigin=false;bool seen=false;
        inDialog([&](QDialog *dialog){if(auto *drill=dynamic_cast<DrillDialog*>(dialog))seen=!drill->settings().fromOrigin;},false);editor.action("exportDrill")->trigger();
        require(seen,"the drill dialog counts from the corner when the preferences say so");seen=false;
        inDialog([&](QDialog *dialog){if(auto *parts=dynamic_cast<ComponentDataDialog*>(dialog))seen=!parts->settings().fromOrigin&&parts->findChildren<QWidget*>().size()>0;},false);
        editor.action("exportComponents")->trigger();require(seen,"so does the component data dialog");}

    // --- The text limit: no text lower than its strokes allow; Imax.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Texte",60,40);b.origin={0,0};d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();
        auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};
        editor.limitTextSize=true;require(editor.minimumTextHeight(0)==2.5&&editor.minimumTextHeight(1)==1.5&&editor.minimumTextHeight(2)==1,"the limits per thickness");
        view->setTool(BoardView::Tool::Text);
        double normalLowest=0,thinLowest=0;
        inDialog([&](QDialog *x){x->findChild<QLineEdit*>("text")->setText("A");auto *h=x->findChild<QDoubleSpinBox*>("height");h->setValue(1);normalLowest=h->value();
            for(auto *c:x->findChildren<QComboBox*>())if(c->count()==3&&c->currentIndex()==1&&c->itemText(0)==ui("dünn")){c->setCurrentIndex(0);break;}thinLowest=h->value();});
        click(view,{10,10});require(std::abs(normalLowest-1.5)<1e-9&&std::abs(thinLowest-2.5)<1e-9,"the text dialog keeps the height above what the stroke allows");require(els().size()==1&&std::abs(els()[0].size-2.5)<1e-9&&els()[0].thickness==0&&textStrokeWidth(els()[0].size,0)>=.15,"a thin text from 2.5 mm");
        editor.editElements({0},[](Element &e){e.size=1;e.thickness=2;updateStrokes(e);});view->setTool(BoardView::Tool::Select);view->setSelection({0});QApplication::processEvents();QApplication::processEvents();
        QComboBox *thick=nullptr;for(auto *c:editor.findChildren<QComboBox*>())if(!c->isHidden()&&c->count()==3&&c->itemText(0)==ui("dünn")&&c->currentIndex()==2)thick=c;
        require(thick,"the thickness in the properties");thick->setCurrentIndex(1);require(std::abs(els()[0].size-1.5)<1e-9,"a thinner stroke raises the height");
        // In the text dialog an existing text keeps a height below the limit while height and stroke stay.
        editor.editElements({0},[](Element &e){e.size=1;e.thickness=0;updateStrokes(e);});
        inDialog([](QDialog *x){x->findChild<QLineEdit*>("text")->setText("B");});view->editRequested(0);
        require(els()[0].text=="B"&&std::abs(els()[0].size-1)<1e-9,"an existing low text keeps its height in the text dialog");
        inDialog([](QDialog *x){for(auto *c:x->findChildren<QComboBox*>())if(c->count()==3&&c->itemText(0)==ui("dünn")&&c->currentIndex()==0){c->setCurrentIndex(1);break;}});view->editRequested(0);
        require(std::abs(els()[0].size-1.5)<1e-9&&els()[0].thickness==1,"another stroke in the dialog brings the limit back");
        editor.limitTextSize=false;require(editor.minimumTextHeight(0)==.1,"without the limit");
        // Imax as the reference estimates it, in the properties of a track and the calculator.
        require(std::abs(maximumCurrent(1,35,20)-3.1281)<1e-3&&std::abs(widthForCurrent(maximumCurrent(1,35,20),35,20)-1)<1e-9&&maximumCurrent(0,35,20)==0,"Imax and its inverse");
        auto t=newElement(ElementType::Track);t.points={{5,20},{25,20}};t.width=1;Document withTrack=editor.document();withTrack.boards[0].elements<<t;editor.setDocument(withTrack);
        view->setSelection({1});QApplication::processEvents();QApplication::processEvents();QLabel *imax=nullptr;for(auto *l:editor.findChildren<QLabel*>("imax"))if(!l->isHidden())imax=l;
        require(imax&&imax->text()==ui("%1 A").arg(uiLocale().toString(3.13,'f',2)),"Imax of the selected track, with two decimals");
        {QLabel *note=nullptr,*length=nullptr;for(auto *l:editor.findChildren<QLabel*>("imaxNote"))if(!l->isHidden())note=l;for(auto *l:editor.findChildren<QLabel*>("trackLength"))if(!l->isHidden())length=l;
            require(note&&note->text()==QStringLiteral("* ")+ui("Bei %1 µm Kupfer und %2 K Erwärmung").arg(uiLocale().toString(35.0),uiLocale().toString(20.0)),"a footnote names copper and warming");
            require(length&&length->text()==ui("%1 mm").arg(uiLocale().toString(20.0,'f',3)),"the length of the track");}
        QString current,needed;
        inDialog([&](QDialog *x){current=x->findChild<QLabel*>("current")->text();x->findChild<QDoubleSpinBox*>("wanted")->setValue(3.13);needed=x->findChild<QLabel*>("needed")->text();},false);
        editor.action("currentCalculator")->trigger();
        require(current==ui("≈ %1 A").arg(uiLocale().toString(3.13,'f',2)),"the calculator starts with the track");
        require(needed==ui("%1 mm").arg(uiLocale().toString(widthForCurrent(3.13,35,20),'f',3))&&std::abs(widthForCurrent(3.13,35,20)-1)<1e-3,"the width a current needs");}

    // --- What the screen settings do: colours, drill holes, the cross hair.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Farben",60,40);b.origin={0,0};auto pad=newElement(ElementType::Pad);pad.pos={20,20};pad.size=4;pad.size2=2;updateOutline(pad);b.elements<<pad;
        d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();
        editor.userColours[1].board=QColor(255,255,255);editor.userColours[1].layers[CopperBottom]=QColor(10,20,200);editor.setColourScheme(2);
        const QImage image=view->renderBoard(10);require(image.pixelColor(5,5)==QColor(255,255,255)&&image.pixelColor(200+15,200)==QColor(10,20,200),"a scheme colours board and layers");
        require(image.pixelColor(200,200)==QColor(255,255,255),"holes in the board's colour");view->holes=2;require(view->renderBoard(10).pixelColor(200,200)==QColor(0,0,0),"black holes");
        view->holes=0;editor.setColourScheme(0);
        auto grab=[&]{return view->grab().toImage();};const QPointF at(40.64,30.48);view->setTool(BoardView::Tool::Track);send(view,QEvent::MouseMove,at,Qt::NoButton);
        const QPoint p=view->toPixel(at).toPoint();
        // A pixel of a colour at or next to a place (the lines lie on whole pixels).
        auto near=[](const QImage &image,QPoint q,QColor c){for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(image.pixelColor(q+QPoint(dx,dy))==c)return true;return false;};
        const QColor white(255,255,255),grey(150,150,150),red(255,0,0);
        view->crosshair=Crosshair{};QImage shot=grab();require(near(shot,p+QPoint(-200,0),white)&&near(shot,p+QPoint(0,-150),white),"in a drawing tool white lines run through the whole view");
        require(!near(shot,p+QPoint(-80,-80),grey),"no lines at 45° at first");
        view->crosshair.lines=false;shot=grab();require(!near(shot,p+QPoint(-200,0),white),"without lines");
        view->crosshair.diagonals=true;view->crosshair.lines=true;shot=grab();require(near(shot,p+QPoint(-80,-80),grey)&&near(shot,p+QPoint(80,-80),grey),"grey lines at 45°");
        const QPoint box=p+QPoint(18,30);require(shot.pixelColor(box)!=white,"no box without coordinates");
        view->crosshair.coordinates=true;view->crosshair.whiteBox=true;shot=grab();require(shot.pixelColor(box)==white,"the coordinates in a box beside the pointer");
        view->crosshair.transparent=true;shot=grab();require(shot.pixelColor(box)!=white,"or without a box");
        // The standard tool shows neither the cross hair nor its coordinates.
        view->crosshair=Crosshair{};view->crosshair.coordinates=true;view->crosshair.whiteBox=true;view->setTool(BoardView::Tool::Select);send(view,QEvent::MouseMove,at,Qt::NoButton);
        shot=grab();require(!near(shot,p+QPoint(-200,0),white)&&shot.pixelColor(box)!=white,"no cross hair in the standard tool");
        // Caught on the pad: red lines, apart from a gap around the point caught where the white line goes on.
        view->crosshair.coordinates=false;view->setTool(BoardView::Tool::Track);send(view,QEvent::MouseMove,{20.3,20.2},Qt::NoButton);const QPoint q=view->toPixel({20,20}).toPoint();shot=grab();
        require(near(shot,q+QPoint(-120,0),red)&&near(shot,q+QPoint(0,-120),red),"red lines where the pointer caught a point");
        require(!near(shot,q+QPoint(-12,0),red)&&near(shot,q+QPoint(-12,0),white),"with a gap around the point");
        // The switch in the view menu and the bar below the board.
        editor.action("crosshair")->trigger();require(!view->crosshair.lines&&!editor.action("crosshair")->isChecked(),"the cross hair switched off");
        {QSettings s(preferencesFile,QSettings::IniFormat);require(!s.value("crosshair/lines").toBool(),"kept with the preferences");}
        auto *button=editor.findChild<QToolButton*>("crosshairButton");require(button&&button->defaultAction()==editor.action("crosshair"),"a switch below the board");
        editor.action("crosshair")->trigger();require(view->crosshair.lines,"and on again");}

    // --- The grid menu: fractions of the inch pitch, metric and own grids, lines or dots, the stronger lines, shown or not.
    {Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();
        Document d;Board b=newBoard("Raster",60,40);b.origin={0,0};b.grid=2.54;d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();
        auto *button=editor.findChild<QToolButton*>("gridMenu");require(button&&button->menu(),"a grid menu beside the grid field");auto *menu=button->menu();
        auto show=[&]{Q_EMIT menu->aboutToShow();};auto act=[&](const QString &name){return menu->findChild<QAction*>(name);};
        show();require(act("grid-inch-0.0396875")&&act("grid-inch-5.08")&&act("grid-inch-2.54")->isChecked()&&menu->findChild<QMenu*>("gridMetric")->actions().size()==11,
                       "the fractions of the inch pitch and the metric grids, the current one ticked");
        act("grid-inch-0.0396875")->trigger();require(editor.document().board().grid==2.54/64,"a fraction of the inch pitch chosen exactly");
        show();require(act("grid-inch-0.0396875")->isChecked()&&!act("grid-inch-2.54")->isChecked(),"the menu ticks the new grid");
        // An own grid, added through its dialog and chosen at once, kept in the preferences, deleted again.
        inDialog([](QDialog *x){x->findChild<QDoubleSpinBox*>("ownGrid")->setValue(.7);});act("gridOwnAdd")->trigger();
        require(editor.ownGrids()==QList<double>{.7}&&editor.document().board().grid==.7,"an own grid added and chosen");
        {QSettings s(preferencesFile,QSettings::IniFormat);require(s.value("grid/own").toStringList()==QStringList{"0.7"},"own grids kept with the preferences");}
        {Editor other;other.loadPreferences();require(other.ownGrids()==QList<double>{.7},"and read back");}
        show();auto *remove=menu->findChild<QMenu*>("gridOwnRemove");require(remove&&remove->isEnabled()&&remove->actions().size()==1,"an own grid to delete");
        remove->actions()[0]->trigger();require(editor.ownGrids().isEmpty(),"the own grid deleted");
        // Also in micrometres or mil, kept in millimetres.
        inDialog([](QDialog *x){x->findChild<QDoubleSpinBox*>("ownGrid")->setValue(500);x->findChild<QComboBox*>("ownGridUnit")->setCurrentIndex(1);});act("gridOwnAdd")->trigger();
        inDialog([](QDialog *x){x->findChild<QDoubleSpinBox*>("ownGrid")->setValue(25);x->findChild<QComboBox*>("ownGridUnit")->setCurrentIndex(2);});act("gridOwnAdd")->trigger();
        {const auto own=editor.ownGrids();require(own.size()==2&&std::abs(own[0]-.5)<1e-9&&std::abs(own[1]-.635)<1e-9,"own grids in micrometres and mil");}
        editor.setOwnGrids({});
        // Lines, or dots in their own colour where the lines cross.
        editor.chooseGrid(2.54);view->fitBoard();QApplication::processEvents();
        auto near=[](const QImage &image,QPoint q,QColor c){for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(image.pixelColor(q+QPoint(dx,dy))==c)return true;return false;};
        const QColor lines(70,70,70),dots(170,170,170);const QPoint onLine=view->toPixel({5.08,6.35}).toPoint(),crossing=view->toPixel({5.08,5.08}).toPoint();
        QImage image=view->grab().toImage();require(near(image,onLine,lines)&&!near(image,crossing,dots),"grid lines at first");
        show();act("gridDots")->trigger();image=view->grab().toImage();
        require(view->gridDots&&near(image,crossing,dots)&&!near(image,onLine,lines)&&!near(image,onLine,dots),"dots where the lines cross, in their own colour");
        {QSettings s(preferencesFile,QSettings::IniFormat);require(s.value("grid/dots").toBool(),"the dots kept with the preferences");}
        show();act("gridShown")->trigger();image=view->grab().toImage();require(!view->gridShown&&!near(image,crossing,dots),"the grid hidden");
        show();act("gridShown")->trigger();show();act("gridLines")->trigger();require(view->gridShown&&!view->gridDots,"shown again, as lines");
        // Every fourth line stronger at first; none, or every second.
        auto count=[&](int y){const QImage i=view->grab().toImage();int n=0;for(int x=0;x<i.width();x++)if(i.pixelColor(x,y)==lines)n++;return n;};
        const int row=view->toPixel({30,6.35}).toPoint().y(),four=count(row);
        show();act("gridMarking-0")->trigger();const int none=count(row);show();act("gridMarking-2")->trigger();const int two=count(row);
        require(view->gridMarking==2&&none<four&&four<two,"every second, every fourth or no line stronger");
        {QSettings s(preferencesFile,QSettings::IniFormat);require(s.value("grid/marking").toInt()==2,"kept with the preferences");}
        show();act("gridMarking-4")->trigger();
        // The dots have a colour in each scheme, kept with the schemes.
        require(Colours::standard().dots==dots&&Colours::standard().airwire==QColor(215,215,215),"the reference's colours of dots and airwires");}
    // --- For the suite: Öffnen through a host with the folder it would start in, and the library folders (the own macro
    // folder, by default under Dokumente/OpenLoch, and further folders whose macros are only read).
    {{QSettings s(preferencesFile,QSettings::IniFormat);s.remove("macroFolder");s.remove("macroFolders/extra");}
        Editor editor;editor.loadPreferences();editor.resize(1000,700);editor.show();QApplication::processEvents();
        QString asked;bool called=false;editor.openHandler=[&](const QString &f){called=true;asked=f;return true;};
        editor.action("open")->trigger();require(called&&asked==editor.startFolder(Editor::Layouts),"Öffnen goes to the host with its start folder");editor.openHandler=nullptr;
        require(libraryFolders().own==openLochDocumentsFolder("Makros")&&libraryFolders().extra.isEmpty()&&editor.macroFolder==libraryFolders().own,"the own macro folder by default");
        QTemporaryDir own,extra;const QString foreign=QDir(extra.path()).filePath("fremd.lmk");
        {auto pad=newElement(ElementType::Pad);pad.size=1.6;pad.size2=.8;updateOutline(pad);QFile f(foreign);require(f.open(QIODevice::WriteOnly)&&f.write(sprint::writeMacro({pad}))>0,"a macro elsewhere");}
        setLibraryFolders({own.path(),{extra.path()}});editor.librariesChanged();
        auto *panel=editor.macroPanel();auto *list=panel->findChild<QComboBox*>("macroLibraries");
        require(libraryFolders()==LibraryFolders{own.path(),{extra.path()}}&&panel->ownFolder()==own.path()&&list&&list->isVisibleTo(panel)&&list->count()==2,"the folders reach the macro library");
        list->setCurrentIndex(1);
        require(panel->folder()==extra.path()&&panel->readOnly()&&panel->currentFolder()==own.path()&&panel->pick(foreign),"a further folder is read; macros are saved into the own one");
        require(!panel->removeMacro(foreign)&&QFile::exists(foreign),"nothing is removed there");
        {QSettings s(preferencesFile,QSettings::IniFormat);s.remove("macroFolder");s.remove("macroFolders/extra");}}
    // --- The keys of the tools for the suite's tooltips: a property of the buttons, no second binding; a changed key
    // follows.
    {Editor editor;QApplication::processEvents();
        auto *track=editor.findChild<QToolButton*>(QStringLiteral("tool-%1").arg(int(BoardView::Tool::Track)));
        require(track&&track->property("toolTipShortcut").toString()==QKeySequence(Qt::Key_L).toString(QKeySequence::NativeText),"the track tool's key for the tooltip");
        require(editor.action("photo")->property("toolTipShortcut").toString()==QKeySequence(Qt::Key_V).toString(QKeySequence::NativeText)&&editor.action("photo")->shortcut().isEmpty(),
                "the photo view's, without a shortcut of its own");
        {QSettings s(preferencesFile,QSettings::IniFormat);s.setValue("keys/track","K");}
        editor.loadPreferences();require(track->property("toolTipShortcut").toString()==QKeySequence(Qt::Key_K).toString(QKeySequence::NativeText),"a changed key follows");
        {QSettings s(preferencesFile,QSettings::IniFormat);s.remove("keys/track");}}
    return 0;
}
