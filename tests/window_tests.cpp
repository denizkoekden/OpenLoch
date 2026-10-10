#include "language.h"
#include "icons.h"
#include "window.h"
#include "geometry.h"
#include "canvas.h"
#include "fixtures.h"
#include "openlibrary.h"
#include "printdialog.h"
#include "printing.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QClipboard>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QScreen>
#include <QMessageBox>
#include <QAbstractButton>
#include <functional>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QLineEdit>
#include <QMouseEvent>
#include <QMenu>
#include <QTextBrowser>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QTabBar>
#include <QMenuBar>
#include <QThread>
#include <QElapsedTimer>
#include <QRadioButton>
#include <QToolButton>
#include <QPrinter>
#include <QSpinBox>
#include <QTextBlock>
#include <QToolBar>
#include <QTextEdit>
#include <QTreeWidget>
#include <QListWidget>
#include <QToolButton>
#include <QDockWidget>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <cmath>
#include <QMimeData>
#include <QDropEvent>
#include <QImage>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QStandardPaths>
#include <QJsonDocument>
#include <stdexcept>

using namespace openloch;
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static QAction *action(Window &window,const QString &name){
    for(auto *a:window.findChildren<QAction*>()){bool dialog=false;for(auto *o=a->parent();o;o=o->parent())dialog|=qobject_cast<QDialog*>(o)!=nullptr;if(!dialog&&(a->objectName()==name||a->text().remove('&')==name))return a;}
    throw std::runtime_error("menu action missing");
}
static void write(const QString &path,const QByteArray &bytes){QDir().mkpath(QFileInfo(path).absolutePath());QFile file(path);require(file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size(),"fixture could not be written");}
int main(int argc,char **argv){
    QApplication app(argc,argv);openloch::setUiLanguage("de"); // the tests compare German texts; CI runners use English systems
    fixtures::checkOwnModel(); // the own model must write every LM4 file as the program does
    app.setProperty("openloch.testing",true);QStandardPaths::setTestModeEnabled(true);
    try{
        QTemporaryDir tmp;app.setProperty("openloch.recoveryDirectory",tmp.filePath("recovery"));app.setProperty("openloch.libraryDirectory",tmp.filePath("Bibliotheken"));write(tmp.filePath("drive_c/ProgramData/LochMaster40/DE/LIB/Test.LIB"),fixtures::library());
        write(tmp.filePath("drive_c/users/Public/Documents/LochMaster40/Board Layouts/Test.LMB"),fixtures::board());
        Window window(tmp.path());window.show();QApplication::processEvents();
        const auto fitted=window.canvas->mapFromScene(window.canvas->sceneRect()).boundingRect();require(fitted.width()>window.canvas->viewport()->width()/2||fitted.height()>window.canvas->viewport()->height()/2,"board was not fitted once the window appeared");
        auto *tree=window.findChild<QTreeWidget*>("projectBrowser");require(tree&&tree->topLevelItemCount()==2,"project browser missing");
        auto imagePath=tmp.filePath("empty-board.png");require(window.canvas->exportImage(imagePath),"empty board export failed");QImage image(imagePath);
        require(image.pixelColor(61,61).blue()>150&&image.pixelColor(31,31).blue()<100,"new board is missing its physical 2.54 mm hole pattern (light holes like the original)");
        { // save dialogs start in the home folder, then in the folder used last; a name typed without suffix gets the filter's
            QDir(tmp.path()).mkdir("Exporte"); // the home folder itself comes from the system (Windows ignores HOME), so only the start is compared
            app.setAttribute(Qt::AA_DontUseNativeDialogs);QString started;
            auto answer=[&](const QString &folder,const QString &name){QTimer::singleShot(0,[&started,folder,name]{auto *dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());if(!dialog)return;
                started=dialog->directory().canonicalPath();if(!folder.isEmpty())dialog->setDirectory(folder);dialog->selectFile(name);static_cast<QDialog*>(dialog)->accept();});};
            const auto exports=QDir(tmp.filePath("Exporte")).canonicalPath();
            answer(exports,"Platine");action(window,"exportPng")->trigger();
            require(started==QDir(QDir::homePath()).canonicalPath(),"export dialog did not start in the home folder");
            require(!QImage(tmp.filePath("Exporte/Platine.png")).isNull(),"PNG export without typed suffix failed");
            answer({},"Platine");action(window,"exportPdf")->trigger();
            require(started==exports&&QFileInfo(tmp.filePath("Exporte/Platine.pdf")).size()>0,"PDF export did not reopen the folder used last");
            answer({},"Platine");action(window,"exportImage")->trigger();
            require(started==exports&&!QImage(tmp.filePath("Exporte/Platine.bmp")).isNull(),"BMP export failed");
        }
        action(window,"Durchgang");action(window,"Potenzial");action(window,"Potenziale anzeigen"); // electrical tools on boards
        auto *freeAreas=window.findChild<QToolButton*>("freeAreas");require(freeAreas,"free areas button missing");
        const int idleItems=window.canvas->scene()->items().size();freeAreas->pressed();const int heldItems=window.canvas->scene()->items().size();freeAreas->released();
        require(heldItems==idleItems+3&&window.canvas->scene()->items().size()==idleItems,"free areas must show only while the button is held");
        action(window,"Kurzschlüsse prüfen")->trigger();auto *shortsDock=window.findChild<QDockWidget*>("shortsDock");auto *shortResults=window.findChild<QListWidget*>("shortResults");
        require(shortsDock&&shortsDock->isVisible()&&shortResults&&shortResults->count()>=1&&shortResults->item(0)->text().startsWith("Keine Kurzschlüsse"),"short check results missing");
        shortsDock->hide();
        auto *pages=window.findChild<QComboBox*>("libraryPages");auto *parts=window.findChild<QListWidget*>("libraryParts");
        require(pages&&parts&&pages->count()==1&&pages->currentText()=="Test","library page selector missing");
        require(parts->count()==1&&parts->item(0)->text().startsWith("Widerstand"),"library page did not list its components");
        auto preview=parts->item(0)->data(Qt::DecorationRole).value<QImage>();bool drawn=false;for(int y=0;y<preview.height()&&!drawn;y++)for(int x=0;x<preview.width();x++)if(preview.pixelColor(x,y).alpha()>0){drawn=true;break;}
        require(drawn,"library component preview was not drawn");
        {   // The previews follow BMP-Rendering as in the original: a solder joint is shaded with it and a plain circle without
            // it; the library draws its page anew when the switch changes.
            const QJsonObject joint{{"type","TDraht"},{"kind",19},{"width",150},{"pen",0},{"path",QJsonArray{QJsonArray{0,0},QJsonArray{0,0}}}};
            require(renderNode(joint,{},.2,1,true)!=renderNode(joint,{},.2,1,false),"a preview with and without BMP-Rendering looks the same");
            // A mark on the old item, not its address: a new item may well be made at the address of the old one.
            parts->item(0)->setData(Qt::UserRole+42,true);action(window,"BMP-Rendering")->trigger();
            require(parts->count()==1&&!parts->item(0)->data(Qt::UserRole+42).toBool()&&!window.canvas->viewState().bitmaps,"the library did not follow BMP-Rendering");
            action(window,"BMP-Rendering")->trigger();require(window.canvas->viewState().bitmaps,"BMP-Rendering back on");
        }
        parts->itemClicked(parts->item(0));require(window.canvas->activeTool=="component","clicking a library part did not arm placement");
        auto *canvas=window.canvas;const auto local=QPointF(canvas->mapFromScene(QPointF(2540,2540))),global=QPointF(canvas->viewport()->mapToGlobal(local.toPoint()));
        QMouseEvent down(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(),&down);QApplication::sendEvent(canvas->viewport(),&up);canvas->setTool("select");
        require(window.canvas->document()->componentNode(canvas->selectedNode())["id"]=="R1"&&canvas->selectedNode()["id"]=="R#","placed library component has incorrect identifier");action(window,"rotateRight")->trigger();require(canvas->selectedNode()["angle"].toDouble()==90,"rotation action did not edit component");
        bool edited=false;QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;auto *id=dialog->findChild<QLineEdit*>("componentId");auto *value=dialog->findChild<QLineEdit*>("componentValue");if(id&&value){id->setText("R9");value->setText("4,7 kΩ");edited=true;}dialog->accept();});
        action(window,"properties")->trigger();require(edited&&canvas->selectedNode()["value"]=="4,7 kΩ","properties dialog did not save component value");
        action(window,"Rückgängig")->trigger();canvas->selectObject("new",0);require(canvas->selectedNode()["value"]=="10k"&&canvas->selectedNode()["angle"].toDouble()==90,"window undo did not restore previous properties");
        action(window,"Wiederholen")->trigger();canvas->selectObject("new",0);require(canvas->selectedNode()["id"]=="R9","window redo did not restore properties");
        { // Bezugspunkt (forum wish): the Bauteil dialog offers the part's terminals; the choice is stored and undone
            int offered=0;
            QTimer::singleShot(0,[&]{auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d)return;if(auto *r=d->findChild<QComboBox*>("componentReference")){offered=r->count();r->setCurrentIndex(r->count()-1);}d->accept();});
            action(window,"properties")->trigger();canvas->selectObject("new",0);
            require(offered>=2&&canvas->selectedNode()["reference"].toInt(-1)==offered-2,"the Bezugspunkt was not chosen in the Bauteil dialog");
            action(window,"Rückgängig")->trigger();canvas->selectObject("new",0);require(!canvas->selectedNode().contains("reference"),"undo did not restore the Bezugspunkt");
        }
        const int count=canvas->scene()->items().size();tree->itemDoubleClicked(tree->topLevelItem(1)->child(0),0);
        require(canvas->scene()->items().size()==count+2,"template was not applied as board underlay (copper and holes)");action(window,"Rückgängig")->trigger();require(canvas->scene()->items().size()==count,"template undo failed");
        action(window,"Wiederholen")->trigger();canvas->selectObject("new",0);action(window,"delete")->trigger();canvas->selectObject("new",0);require(canvas->selectedNode().isEmpty(),"delete action did not remove component");
        action(window,"Rückgängig")->trigger();canvas->selectObject("new",0);require(canvas->selectedNode()["id"]=="R9","delete undo lost component properties");
        action(window,"Kopieren")->trigger();action(window,"Einfügen")->trigger();require(canvas->selectedNode()["id"]=="R1","paste did not allocate an unused identifier");
        action(window,"Alles auswählen")->trigger();require(canvas->selectionCount()==2,"select-all menu missed pasted component");action(window,"Auswahl aufheben")->trigger();require(canvas->selectionCount()==0,"deselect menu failed");
        canvas->selectObject("new",1);action(window,"Ausschneiden")->trigger();canvas->selectObject("new",1);require(canvas->selectedNode().isEmpty(),"cut menu did not remove selection");
        action(window,"Rückgängig")->trigger();canvas->selectObject("new",1);require(canvas->selectedNode()["id"]=="R1","cut was not undoable");
        action(window,"boardDuplicate")->trigger();auto *boards=window.findChild<QTabBar*>("boardTabs");require(boards&&boards->count()==2&&boards->currentIndex()==1,"board duplication menu failed");
        action(window,"boardAdd")->trigger();require(boards->count()==3&&canvas->scene()->items().size()==1,"adding a blank board failed");boards->setCurrentIndex(0);canvas->selectObject("new",0);require(canvas->selectedNode()["id"]=="R9","board selector lost the first board");
        {   // a right click on a board's tab chooses that board for its menu
            QTimer::singleShot(0,[]{if(auto *m=qobject_cast<QMenu*>(QApplication::activePopupWidget()))m->close();});
            emit boards->customContextMenuRequested(boards->tabRect(2).center());
            require(boards->currentIndex()==2,"a right click on a board tab must choose that board");boards->setCurrentIndex(0);canvas->selectObject("new",0);
            auto *closeWindow=window.findChild<QAction*>("closeWindow");
            require(closeWindow&&closeWindow->shortcuts()==QKeySequence::keyBindings(QKeySequence::Close),"the platform's key closes the window");
        }
        action(window,"Anmerkungen…")->trigger();auto *notes=window.findChild<QTextEdit*>("notesText");require(notes&&notes->isVisible(),"notes editor did not open");
        notes->setPlainText("Prüfung Ω");
        { // The editor's ruler like the original: dragging the lower marker indents the paragraph's other lines, the upper one
          // its first line; the indents go into the RTF
            auto *ruler=window.findChild<QWidget*>("notesRuler");require(ruler&&ruler->isVisible(),"the notes ruler is missing");
            auto drag=[&](QPointF from,QPointF to){QMouseEvent down(QEvent::MouseButtonPress,from,ruler->mapToGlobal(from),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(ruler,&down);
                QMouseEvent move(QEvent::MouseMove,to,ruler->mapToGlobal(to),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(ruler,&move);
                QMouseEvent up(QEvent::MouseButtonRelease,to,ruler->mapToGlobal(to),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(ruler,&up);};
            const double start=notes->viewport()->x()+notes->document()->documentMargin();
            drag({start,20},{start+40,20});require(std::abs(notes->textCursor().blockFormat().leftMargin()-40)<1.5&&std::abs(notes->textCursor().blockFormat().textIndent()+40)<1.5,"the lower marker did not indent the other lines");
            drag({start,4},{start+20,4});require(std::abs(notes->textCursor().blockFormat().textIndent()+20)<1.5,"the upper marker did not indent the first line");
            require(writeRtf(*notes->document()).contains("\\li600"),"the indent did not reach the RTF");
        }
        window.findChild<QDialog*>("notesWindow")->close();
        auto *recovery=window.findChild<QTimer*>("recoveryTimer");require(recovery,"recovery timer missing");recovery->start(0);QApplication::processEvents();QDir saved(tmp.filePath("recovery"));auto snapshots=saved.entryList({"*.openloch"},QDir::Files);require(snapshots.size()==1,"automatic recovery snapshot missing");
        auto snapshot=Project::load(saved.filePath(snapshots.first()));require(snapshot.boards.size()==3&&snapshot.notes=="Prüfung Ω","recovery lost boards or notes");
        parts->itemClicked(parts->item(0));QMimeData part;part.setData(Canvas::libraryPartMime,"Test.LIB");const QPoint drop=canvas->mapFromScene(QPointF(5080,5080));
        QDragEnterEvent enter(drop,Qt::CopyAction,&part,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(canvas->viewport(),&enter);require(enter.isAccepted(),"canvas refused a dragged library part");
        QDropEvent dropped(QPointF(drop),Qt::CopyAction|Qt::MoveAction,&part,Qt::LeftButton,Qt::ShiftModifier);QApplication::sendEvent(canvas->viewport(),&dropped);
        require(dropped.isAccepted()&&canvas->selectedNode()["type"]=="component","dropping a library part did not place it");require(dropped.dropAction()==Qt::CopyAction,"a dragged library part must be copied, never moved out of the panel");
        // Component dialog: parts-list flag and centre position change in one undoable step.
        canvas->selectObject("new",0);const QPointF centre=canvas->selectedCentre();bool positioned=false;
        QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            for(auto *box:dialog->findChildren<QCheckBox*>())if(box->text()=="Erscheint in Stückliste")box->setChecked(false);
            if(auto *x=dialog->findChild<QDoubleSpinBox*>("centreX")){x->setValue(centre.x()/100+2.54);positioned=true;}dialog->accept();});
        action(window,"properties")->trigger();canvas->selectObject("new",0);
        require(positioned&&std::abs(canvas->selectedCentre().x()-centre.x()-254)<.5&&!canvas->selectedNode()["group_flags"].toArray().at(1).toBool(),"component dialog did not apply parts-list flag and centre");
        action(window,"Rückgängig")->trigger();canvas->selectObject("new",0);
        const auto restored=canvas->selectedNode()["group_flags"].toArray();
        require(std::abs(canvas->selectedCentre().x()-centre.x())<.5&&(restored.size()<2||restored.at(1).toBool()),"component dialog changes must undo in one step");
        { // library editor: own pages are created, filled from the board, reordered, renamed and deleted; installation pages stay untouched
            auto respond=[](std::function<void(QDialog*)> act){QTimer::singleShot(0,[act]{if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))act(dialog);});};
            auto answer=[&](const QString &text){respond([text](QDialog *dialog){if(auto *field=dialog->findChild<QLineEdit*>())field->setText(text);dialog->accept();});};
            auto confirm=[&]{respond([](QDialog *dialog){if(auto *box=qobject_cast<QMessageBox*>(dialog))box->button(QMessageBox::Yes)->click();});};
            const auto installed=tmp.filePath("drive_c/ProgramData/LochMaster40/DE/LIB/Test.LIB");bool refused=false;
            respond([&refused](QDialog *dialog){refused=qobject_cast<QMessageBox*>(dialog)!=nullptr;dialog->accept();});canvas->selectObject("new",0);action(window,"Zur Bibliothek hinzufügen")->trigger();
            QFile original(installed);require(refused&&original.open(QIODevice::ReadOnly)&&original.readAll()==fixtures::library(),"a page of the LochMaster installation was changed");
            answer("Eigene Teile");action(window,"Seite anlegen…")->trigger();const auto own=QDir(tmp.filePath("Bibliotheken")).filePath("LIB1.LIB");
            require(QFile::exists(own)&&pages->count()==2&&pages->currentIndex()==0&&pages->currentText()=="Eigene Teile"&&parts->count()==0,"new own library page missing");
            // As in the original the Bauteil dialog comes first.
            auto partDialog=[&](const QString &name){respond([name](QDialog *dialog){if(auto *field=dialog->findChild<QLineEdit*>("componentName"))field->setText(name);dialog->accept();});};
            partDialog("Erstes Teil");canvas->selectObject("new",0);action(window,"Zur Bibliothek hinzufügen")->trigger();partDialog("Zweites Teil");canvas->selectObject("new",1);action(window,"Zur Bibliothek hinzufügen")->trigger();
            require(parts->count()==2&&parts->currentRow()==1,"marked components were not added to the own page");
            const auto first=parts->item(0)->toolTip(),second=parts->item(1)->toolTip();require(first!=second,"added parts cannot be told apart");
            action(window,"Bauteil eins nach oben setzen")->trigger();require(parts->count()==2&&parts->currentRow()==0&&parts->item(0)->toolTip()==second&&parts->item(1)->toolTip()==first,"moving a library part up failed");
            respond([](QDialog *dialog){if(auto *field=dialog->findChild<QLineEdit*>("libraryPageName"))field->setText("Umbenannt");dialog->accept();});
            action(window,"libraryProperties")->trigger();require(pages->currentText()=="Umbenannt"&&Project::load(own).title=="Umbenannt","renaming the library page in its properties failed");
            // The part's own properties from the library list: the Bauteil dialog on the marked part
            QTimer::singleShot(0,[]{auto *menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());if(!menu)return;QAction *properties=nullptr;for(auto *a:menu->actions())if(a->text()=="Eigenschaften…")properties=a;menu->close();
                if(!properties)return;QTimer::singleShot(0,[]{if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){if(auto *field=dialog->findChild<QLineEdit*>("componentValue"))field->setText("4,7k");dialog->accept();}});properties->trigger();});
            emit parts->customContextMenuRequested(parts->visualItemRect(parts->item(0)).center());
            require(Project::load(own).legacyNode(0)["value"]=="4,7k","the part's properties from the library list were not saved");const auto secondChanged=parts->item(0)->toolTip();
            parts->setCurrentRow(1);confirm();action(window,"Bauteil aus Bibliothek löschen")->trigger();require(parts->count()==1&&parts->item(0)->toolTip()==secondChanged,"deleting a library part failed");
            parts->itemClicked(parts->item(0));require(canvas->activeTool=="component","a part of the own page cannot be placed");canvas->setTool("select");
            confirm();action(window,"Seite löschen…")->trigger();require(!QFile::exists(own)&&pages->count()==1&&pages->currentText()=="Test","deleting the library page failed");
        }
        { // object assistant: opens on the lying resistor, corrects values out of range, places the part with the next free number
            QString first;bool corrected=false,shown=false;
            QTimer::singleShot(0,[&]{
                auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
                auto *styles=dialog->findChild<QComboBox*>("assistantStyle");auto *table=dialog->findChild<QTableWidget*>("assistantParameters");auto *preview=dialog->findChild<QLabel*>("assistantPreview");
                if(!styles||!table||!preview){dialog->reject();return;}
                first=styles->currentText();styles->setCurrentIndex(13);
                QTimer::singleShot(0,[]{if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->accept();});
                table->item(0,1)->setText("300");corrected=table->item(0,1)->text()=="200";table->item(0,1)->setText("6");table->item(5,1)->setText("47nF");
                shown=!preview->pixmap().isNull()&&table->rowCount()==8;dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            });
            action(window,"Assistent…")->trigger();
            require(first=="Bauteil: Widerstand, liegend"&&corrected&&shown&&canvas->activeTool=="component","object assistant dialog failed");
            const auto spot=QPointF(canvas->mapFromScene(QPointF(7620,5080))),screen=QPointF(canvas->viewport()->mapToGlobal(spot.toPoint()));
            QMouseEvent press(QEvent::MouseButtonPress,spot,screen,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),release(QEvent::MouseButtonRelease,spot,screen,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(canvas->viewport(),&press);QApplication::sendEvent(canvas->viewport(),&release);canvas->setTool("select");
            const auto placed=canvas->selectedNode();
            require(placed["type"]=="component"&&window.canvas->document()->componentNode(placed)["id"]=="C1"&&placed["value"]=="47nF"&&placed["description"]=="Kondensator","assistant part was not placed with its number and value");
        }
        { // quick parity commands: 180°, zoom onto objects and real size, board rename/order, boards added from a file
            canvas->selectObject("new",0);const double before=canvas->selectedNode()["angle"].toDouble();action(window,"rotate180")->trigger();canvas->selectObject("new",0);
            require(std::abs(std::remainder(canvas->selectedNode()["angle"].toDouble()-before-180,360.0))<1e-9,"180° rotation failed");
            action(window,"Einpassen")->trigger();const double whole=canvas->transform().m11();action(window,"Markierte Objekte zoomen")->trigger();
            require(canvas->transform().m11()>whole,"zoom onto the marked part did not enlarge the view");
            action(window,"Reale Größe 1:1")->trigger();auto *screen=canvas->screen();
            if(screen&&screen->physicalSize().width()>0)require(std::abs(canvas->transform().m11()-screen->geometry().width()/(screen->physicalSize().width()/25.4)/2540)<1e-9,"real size zoom is not 1:1");
            auto *boards=window.findChild<QTabBar*>("boardTabs");const int count=boards->count();
            QTimer::singleShot(0,[]{if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){if(auto *field=dialog->findChild<QLineEdit*>())field->setText("Hauptplatine");dialog->accept();}});
            action(window,"Platine umbenennen…")->trigger();require(boards->tabText(boards->currentIndex())=="Hauptplatine","board rename failed");
            action(window,"Platine eins nach rechts")->trigger();require(boards->currentIndex()==1&&boards->tabText(1)=="Hauptplatine"&&boards->count()==count,"moving the board failed");
            action(window,"boardFirst")->trigger();require(boards->currentIndex()==0&&boards->tabText(0)=="Hauptplatine","moving the board to the front failed");
            const auto extra=tmp.filePath("Zusatz.openloch");{Project other;other.title="Zusatzplatine";other.save(extra);}
            QTimer::singleShot(0,[extra]{if(auto *dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){dialog->selectFile(extra);static_cast<QDialog*>(dialog)->accept();}});
            action(window,"Datei hinzufügen…")->trigger();require(boards->count()==count+1&&boards->tabText(boards->currentIndex())=="Zusatzplatine","adding boards from a file failed");
            boards->setCurrentIndex(0);
        }
        { // "rotateAngle" (clockwise as chosen) and "Auf andere Platinenseite setzen" on a part with all it contains
            canvas->selectObject("new",0);const double start=canvas->selectedNode()["angle"].toDouble();
            QTimer::singleShot(0,[]{if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){dialog->findChild<QDoubleSpinBox*>("rotationAngle")->setValue(45);dialog->findChild<QCheckBox*>("rotationClockwise")->setChecked(true);dialog->accept();}});
            action(window,"rotateAngle")->trigger();canvas->selectObject("new",0);require(std::abs(std::remainder(canvas->selectedNode()["angle"].toDouble()-start-45,360.0))<1e-9,"rotation by an angle failed");
            action(window,"Rückgängig")->trigger();canvas->selectObject("new",0);action(window,"Auf andere Platinenseite setzen")->trigger();canvas->selectObject("new",0);
            require(canvas->selectedNode()["otherSide"].toBool(),"switching the side of a part failed");action(window,"Rückgängig")->trigger();
        }
        { // "Layout bearbeiten": draw a track in the layout editor and apply it to the board
            bool drawn=false;const QString previous=window.canvas->document()->boardSource;
            QTimer::singleShot(0,[&]{
                auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog||dialog->objectName()!="layoutEditor")return;
                auto *editor=static_cast<Canvas*>(dialog->findChild<QGraphicsView*>("layoutCanvas"));editor->toolDefaults["track"]=QJsonObject{{"width",200}};editor->setTool("track");
                for(QPointF at:{QPointF(254,508),QPointF(2286,508)}){const QPointF v=editor->mapFromScene(at);QMouseEvent press(QEvent::MouseButtonPress,v,QPointF(editor->viewport()->mapToGlobal(v.toPoint())),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(editor->viewport(),&press);}
                drawn=editor->document()->additions.size()==1;dialog->findChild<QPushButton*>("applyLayout")->click();
            });
            action(window,"Layout bearbeiten…")->trigger();const auto *board=window.canvas->document();
            require(drawn&&!board->boardSource.isEmpty()&&board->libraries[board->boardSource].document["objects"].toArray().last().toObject()["type"]=="TLeiterbahn","the layout editor did not apply the track");
            action(window,"Rückgängig")->trigger();require(window.canvas->document()->boardSource==previous,"applying a layout could not be undone");
        }
        { // the original's independent view switches: Röntgenblick and BMP-Rendering on by default, Wenden mirrors, Durchsicht does not
            require(canvas->viewState().xray&&canvas->viewState().bitmaps&&!canvas->viewState().flip,"view defaults differ from the original");
            action(window,"Durchsicht")->trigger();require(canvas->backActive()&&!canvas->viewState().flip,"Durchsicht must activate the other side without mirroring");
            action(window,"Wenden")->trigger();require(!canvas->backActive()&&canvas->viewState().flip,"Wenden with Durchsicht must make the component side active again, mirrored");
            require(canvas->transform().m11()>0&&canvas->transform().m22()<0,"Wenden must turn the board over top to bottom like the original");
            action(window,"Wenden")->trigger();action(window,"Durchsicht")->trigger();action(window,"S/W-Darstellung")->trigger();require(canvas->viewState().mono,"S/W display did not switch");
            action(window,"S/W-Darstellung")->trigger();
        }
        { // boards are saved as LM4 by default, with all their boards; afterwards "Speichern" writes the LM4 again in place
            auto *boards=window.findChild<QTabBar*>("boardTabs");const int count=boards->count();QString offered;
            QTimer::singleShot(0,[&]{if(auto *dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){offered=dialog->selectedNameFilter();dialog->setDirectory(tmp.path());dialog->selectFile("Gespeichert");static_cast<QDialog*>(dialog)->accept();}});
            action(window,"Speichern unter…")->trigger();const auto saved=tmp.filePath("Gespeichert.LM4");
            require(offered.startsWith("Lochraster-Projekt")&&QFile::exists(saved)&&Project::load(saved).boards.size()==count,"board was not saved as LM4 with all its boards");
            canvas->selectObject("new",0);action(window,"rotate180")->trigger();QFile::remove(saved);
            bool asked=false;QTimer::singleShot(0,[&]{if(QApplication::activeModalWidget()){asked=true;QApplication::activeModalWidget()->close();}});
            action(window,"Speichern")->trigger();QApplication::processEvents();require(!asked&&QFile::exists(saved),"Speichern did not write the LM4 in place");
        }
        { // Bauteil menu: plain groups, "Bauteileinheit bilden" with an extra field, the Excel table, "Bauteileinheit aufheben"
            canvas->selectAll();action(window,"group")->trigger();const auto group=window.canvas->document()->componentNode(canvas->selectedNode());
            require(group["group_flags"].toArray()==QJsonArray({false,true}),"Gruppieren must create a plain group");
            { // A plain group's properties like the original: name, Abmessungen and position; a new width scales the group about
              // its centre, one undo step brings it back
                const auto before=window.canvas->document()->additions;const QPointF centre=canvas->selectedCentre();double asked=0;
                QTimer::singleShot(0,[&asked]{auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d||d->objectName()!="groupDialog")return;
                    auto *w=d->findChild<QDoubleSpinBox*>("groupWidth");asked=w->value()*1.5;w->setValue(asked);d->findChild<QLineEdit*>("groupName")->setText("Halter");d->accept();});
                action(window,"properties")->trigger();const auto scaled=window.canvas->document()->componentNode(canvas->selectedNode());const QRectF r=legacyBounds(scaled);
                require(asked>0&&std::abs(r.width()-asked*100)<2&&scaled["label"]=="Halter"&&QLineF(canvas->selectedCentre(),centre).length()<60,"the group was not scaled to the new width about its centre");
                action(window,"Rückgängig")->trigger();require(window.canvas->document()->additions==before,"one undo did not restore the group");canvas->selectAll();
            }
            QString seen;QTimer::singleShot(0,[&]{
                auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());seen+=d?d->objectName()+";":"none;";if(!d||d->objectName()!="componentDialog")return;
                d->findChild<QLineEdit*>("componentName")->setText("Netzteil");d->findChild<QLineEdit*>("componentId")->setText("PS#");d->findChild<QLineEdit*>("componentValue")->setText("5 V");
                QTimer::singleShot(0,[&]{
                    auto *x=qobject_cast<QDialog*>(QApplication::activeModalWidget());seen+=x?x->objectName()+";":"none;";if(!x||x->objectName()!="extraFieldsDialog")return;
                    QTimer::singleShot(0,[&]{auto *input=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());seen+=input?"input;":"no input;";if(input){input->setTextValue("URL");input->accept();}});
                    x->findChild<QPushButton*>("addExtraField")->click();x->accept();
                });
                d->findChild<QLineEdit*>("componentDescription")->clear(); // the dialog starts with the values used last, as in the original
                d->findChild<QPushButton*>("extraFieldsButton")->click();if(auto *e=d->findChild<QLineEdit*>("extra:URL")){seen+="field;";e->setText("https://example.org");}d->accept();
            });
            action(window,"Bauteileinheit bilden…")->trigger();const auto part=window.canvas->document()->componentNode(canvas->selectedNode());
            if(!(part["id"]=="PS1"&&part["label"]=="Netzteil"&&part["group_flags"].toArray()==QJsonArray({true,true})&&part["extra"].toArray().size()==1&&part["extra"].toArray()[0].toArray()==QJsonArray{"URL",true,"https://example.org"}))
                throw std::runtime_error(("Bauteileinheit bilden failed; dialogs: "+seen+" part: "+QString::fromUtf8(QJsonDocument(QJsonObject{{"id",part["id"]},{"label",part["label"]},{"group_flags",part["group_flags"]},{"extra",part["extra"]}}).toJson(QJsonDocument::Compact))).toStdString());
            // "Excel erzeugen…": the table on the clipboard and as CSV for the spreadsheet program (here not started).
            QUrl opened;window.openExternally=[&](const QUrl &url){opened=url;return true;};
            action(window,"Excel erzeugen…")->trigger();const auto table=QApplication::clipboard()->text();
            {QFile csv(opened.toLocalFile());require(opened.isLocalFile()&&csv.open(QIODevice::ReadOnly),"Excel erzeugen wrote no CSV file");const QByteArray bytes=csv.readAll();
             require(bytes.startsWith("\xEF\xBB\xBF\"Kennung\";\"Name\";")&&bytes.contains("\"PS1\";\"Netzteil\";\"5 V\""),"the CSV of Excel erzeugen");csv.close();QFile::remove(opened.toLocalFile());}
            if(!(table.startsWith("Kennung\tName\tWert/Typ\tBeschreibung\tTeil von\tURL\n")&&table.contains("\nPS1\tNetzteil\t5 V\t\t\thttps://example.org\n")&&table.contains("\tPS1\t")))throw std::runtime_error(("the Excel table differs from the original's:\n"+table).toStdString());
            QTimer::singleShot(0,[]{if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->accept();});action(window,"ungroup")->trigger();
            require(canvas->selectionCount()==1,"Gruppe auflösen must not dissolve a part");
            action(window,"Bauteileinheit aufheben")->trigger();require(canvas->selectionCount()>1,"Bauteileinheit aufheben did not dissolve the part");
        }
        { // Anmerkungen: bold text and the inserted parts list reach the LM4 as RTF and survive switching boards; Stückliste → Erstellen
            action(window,"Anmerkungen…")->trigger();auto *notes=window.findChild<QTextEdit*>("notesText");notes->clear();notes->insertPlainText("Wichtig");notes->selectAll();
            for(auto *a:window.findChild<QToolBar*>("notesFormat")->actions())if(a->text()=="Fett")a->trigger();
            notes->moveCursor(QTextCursor::End);notes->insertPlainText("\n");window.findChild<QAction*>("insertPartsList")->trigger();
            require(notes->toPlainText().contains("Stückliste für ")&&notes->toPlainText().contains("Drahtbrücken:"),"the parts list was not inserted");
            window.findChild<QDialog*>("notesWindow")->close();
            QTimer::singleShot(0,[&]{if(auto *dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){dialog->setDirectory(tmp.path());dialog->selectFile("Notiert");static_cast<QDialog*>(dialog)->accept();}});
            action(window,"Speichern unter…")->trigger();auto saved=Project::load(tmp.filePath("Notiert.LM4"));saved.switchBoard(window.findChild<QTabBar*>("boardTabs")->currentIndex());
            require(saved.notesRtf.contains("\\b")&&saved.notesRtf.contains("Wichtig")&&saved.notes.contains("Stückliste für ")&&saved.notes.contains("Drahtbrücken:"),"formatted notes were not saved in the LM4");
            auto *boards=window.findChild<QTabBar*>("boardTabs");const int current=boards->currentIndex();boards->setCurrentIndex(current==0?1:0);boards->setCurrentIndex(current);
            action(window,"Anmerkungen…")->trigger();require(notes->toPlainText().startsWith("Wichtig")&&notes->document()->begin().begin().fragment().charFormat().fontWeight()==QFont::Bold,"notes were lost when switching boards");
            // the notes' own Speichern unter… asks for a file every time, also when the notes have one
            {auto *saveAs=window.findChild<QDialog*>("notesWindow")->findChild<QAction*>("notesSaveAs");int asked=0;
             for(const QString name:{"Erste.rtf","Zweite.rtf"}){QTimer::singleShot(0,[&,name]{if(auto *d=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){asked++;d->setDirectory(tmp.path());d->selectFile(name);static_cast<QDialog*>(d)->accept();}});saveAs->trigger();}
             require(asked==2&&QFile::exists(tmp.filePath("Erste.rtf"))&&QFile::exists(tmp.filePath("Zweite.rtf")),"Speichern unter of the notes");}
            window.findChild<QDialog*>("notesWindow")->close();
            require(window.findChild<QAction*>("helpTopics")->shortcuts().contains(QKeySequence(Qt::Key_F1)),"F1 opens the help topics on every system");
            QString listed;QTimer::singleShot(0,[&]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());d&&d->objectName()=="partsListWindow"){listed=d->findChild<QTextEdit*>("notesText")->toPlainText();d->reject();}});
            action(window,"Erstellen…")->trigger();require(listed.startsWith("Stückliste für ")&&listed.contains("Drahtbrücken:"),"Stückliste → Erstellen did not show the parts list");
        }
        { // Datei → HPGL-Bearbeitungsdateien: the outer rectangle job written into a new folder as <job>.PLT
            const auto folder=tmp.filePath("plots")+"/";QStringList jobNames;
            QTimer::singleShot(0,[&]{
                auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d||d->objectName()!="hpglDialog")return;
                d->findChild<QCheckBox*>("hpglOutline")->setChecked(true);d->findChild<QLineEdit*>("hpglPath")->setText(folder);
                auto *jobs=d->findChild<QTreeWidget*>("hpglJobs");for(int i=0;i<jobs->topLevelItemCount();i++)jobNames.append(jobs->topLevelItem(i)->text(0));
                d->findChild<QPushButton*>("hpglExport")->click();
            });
            action(window,"exportHpgl")->trigger();QFile plot(folder+"Aussenrechteck mit 2 mm.PLT");
            require(jobNames.contains("Aussenrechteck mit 2 mm")&&plot.open(QIODevice::ReadOnly)&&plot.readAll().startsWith("IN;\r\nSP1;\r\nPT0;\r\nPU;\r\nPA"),"the HPGL export did not write the outer rectangle file");
        }
        { // Ansicht → Einheit (N sets the 2.54 mm grid) and Platine → Objektbaum anzeigen with selection both ways
            action(window,"Einheit inch")->trigger();require(canvas->unit()==1&&std::abs(canvas->gridStep()-25.4)<1e-9,"Einheit inch must snap to 0.254 mm");
            action(window,"Einheit mm")->trigger();require(canvas->unit()==0&&canvas->gridStep()==10,"Einheit mm must snap to 0.1 mm");
            action(window,"Einheit N")->trigger();require(canvas->unit()==2&&canvas->gridStep()==254,"Einheit N must snap to the hole pitch");action(window,"Einheit mm")->trigger();
            action(window,"Objektbaum anzeigen")->trigger();auto *tree=window.findChild<QTreeWidget*>("objectTree");
            require(tree&&tree->isVisible()&&tree->topLevelItemCount()>0,"the object tree is empty");
            canvas->selectObjects({});tree->topLevelItem(0)->setSelected(true);require(canvas->selectionCount()==1,"choosing in the object tree did not select the object");
            canvas->selectAll();require(tree->selectedItems().size()==tree->topLevelItemCount(),"the canvas selection is not shown in the object tree");
            bool part=false;for(int i=0;i<tree->topLevelItemCount();i++)part|=tree->topLevelItem(i)->childCount()>0;require(part,"groups are shown without their contents");
            action(window,"Objektbaum anzeigen")->trigger();require(!tree->isVisible(),"the object tree could not be hidden");
        }
        { // Bibliothek → LochMaster-Bibliotheken einbinden: choosing LochMaster40.exe shows its pages and says what was found
            const auto other=tmp.filePath("Andere/drive_c/");write(other+"Program Files (x86)/LochMaster40/LochMaster40.exe",QByteArray("MZ"));write(other+"ProgramData/LochMaster40/DE/LIB/Fremd.LIB",fixtures::library());
            QTemporaryDir elsewhere;QString shown,afterForget;bool wrongRefused=false,listed=false;
            QTimer::singleShot(0,[&]{
                auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d||d->objectName()!="lochmasterDialog")return;auto *status=d->findChild<QLabel*>("lochmasterStatus");
                QTimer::singleShot(0,[&]{if(auto *f=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){f->selectFile(elsewhere.path());static_cast<QDialog*>(f)->accept();}});
                d->findChild<QPushButton*>("lochmasterFolder")->click();wrongRefused=status->text().startsWith("✗");
                QTimer::singleShot(0,[&]{if(auto *f=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){f->selectFile(other+"Program Files (x86)/LochMaster40/LochMaster40.exe");static_cast<QDialog*>(f)->accept();}});
                d->findChild<QPushButton*>("lochmasterExe")->click();shown=status->text();
                auto *pages=window.findChild<QComboBox*>("libraryPages");for(int i=0;i<pages->count();i++)listed|=pages->itemData(i).toString().endsWith("Fremd.LIB");
                d->findChild<QPushButton*>("lochmasterForget")->click();afterForget=status->text();d->reject();
            });
            action(window,"LochMaster-Bibliotheken einbinden…")->trigger();
            require(wrongRefused&&listed&&shown.startsWith("✓ Eingebunden")&&shown.contains("1 Bibliotheksseiten")&&afterForget.startsWith("Noch keine"),"choosing the LochMaster installation gives no clear answer");
        }
        { // OpenLoch's own library pages appear read-only in the panel and offer their parts
            app.setProperty("openloch.openLibrarySource",QString(OPENLOCH_SOURCE_DIR)+"/libraries");app.setProperty("openloch.openLibraryDirectory",tmp.filePath("open"));
            {
                Window open(tmp.path());auto *pages=open.findChild<QComboBox*>("libraryPages");const int index=pages->findText("OpenLoch · Widerstände");
                require(index>=0&&pages->findText("OpenLoch · Leuchtdioden")>=0&&!pages->itemData(index,Qt::UserRole+1).toBool(),"the OpenLoch library pages are missing or editable");
                pages->setCurrentIndex(index);QApplication::processEvents();auto *parts=open.findChild<QListWidget*>("libraryParts");
                require(parts&&parts->count()==10,"the OpenLoch resistor page does not list its parts");
                require(QDir(tmp.filePath("open")).entryList(QDir::Dirs|QDir::NoDotAndDotDot).size()==1,"the generated library pages are not kept in one folder");
            }
            app.setProperty("openloch.openLibrarySource",QVariant());app.setProperty("openloch.openLibraryDirectory",QVariant());
        }
        { // Bauteilordner (forum wish): every LIB file a part, every folder a page; saved parts and files copied in appear
            const QString folder=tmp.filePath("Bauteile");QDir().mkpath(folder+"/Sensoren");
            QFile f(QString(OPENLOCH_SOURCE_DIR)+"/libraries/09-widerstaende.json");require(f.open(QIODevice::ReadOnly),"a library page cannot be read");const auto page=QJsonDocument::fromJson(f.readAll()).object();
            {QFile out(folder+"/Sensoren/Widerstaende.LIB");require(out.open(QIODevice::WriteOnly),"a library file cannot be written");out.write(openLibraryPage(page));}
            app.setProperty("openloch.componentFolder",folder);
            {
                Window w(tmp.path());w.resize(1200,800);w.show();QApplication::processEvents();auto *pages=w.findChild<QComboBox*>("libraryPages");auto *parts=w.findChild<QListWidget*>("libraryParts");
                const int root=pages->findText("Ordner · Bauteile"),sub=pages->findText("Ordner · Sensoren");require(root>=0&&sub>=0,"the component folder pages are missing");
                pages->setCurrentIndex(sub);QApplication::processEvents();
                require(parts->count()==10&&parts->item(0)->data(Qt::UserRole).toString().endsWith("Widerstaende.LIB"),"a folder page does not list the parts of its files");
                parts->itemClicked(parts->item(0));auto *c=w.canvas;const auto local=QPointF(c->mapFromScene(QPointF(2540,2540))),global=QPointF(c->viewport()->mapToGlobal(local.toPoint()));
                QMouseEvent down(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(c->viewport(),&down);QApplication::sendEvent(c->viewport(),&up);c->setTool("select");require(c->selectionCount()==1,"the part from the folder was not placed");
                pages->setCurrentIndex(root);QApplication::processEvents();
                QTimer::singleShot(0,[]{if(auto *d=qobject_cast<QInputDialog*>(QApplication::activeModalWidget())){d->setTextValue("Widerstand Test");d->accept();}});
                action(w,"Als Bauteildatei speichern…")->trigger();QApplication::processEvents();
                require(QFileInfo::exists(folder+"/Widerstand Test.LIB")&&pages->currentText()=="Ordner · Bauteile"&&parts->count()==1,"the part was not saved as its own file in the folder");
                require(QFile::copy(folder+"/Sensoren/Widerstaende.LIB",folder+"/Kopie.LIB"),"copying a part file failed");
                QElapsedTimer waited;waited.start();while(parts->count()!=11&&waited.elapsed()<8000){QApplication::processEvents(QEventLoop::AllEvents,50);QThread::msleep(20);}
                require(parts->count()==11,"a part file copied into the folder did not appear");
                // The library folders as the suite's overview reads and sets them; the window reads its library anew.
                require(componentLibraryFolders().own==folder,"the own component folder");
                const QString other=tmp.filePath("Andere Bauteile");QDir().mkpath(other);require(QFile::copy(folder+"/Kopie.LIB",other+"/Einzeln.LIB"),"copying a part file failed");
                setComponentLibraryFolders({other,{}});w.librariesChanged();
                require(componentLibraryFolders().own==other&&pages->findText("Ordner · Andere Bauteile")>=0,"a changed component folder reaches the window");
                // Öffnen through a host: no own dialog, the host gets the folder the window would start in.
                QString asked;bool called=false;w.openHandler=[&](const QString &folder){called=true;asked=folder;return true;};
                QTimer::singleShot(0,[]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget()))d->reject();});
                action(w,"Öffnen…")->trigger();require(called&&!asked.isEmpty(),"Öffnen goes to the host");
            }
            app.setProperty("openloch.componentFolder",QVariant());
        }
        { // the original's toolbars with OpenLoch's own symbols, the tool palette beside the drawing area, pointers per tool
            for(const auto &name:openLochIconNames())require(!openLochIcon(name).pixmap(16,16).isNull(),"a toolbar symbol is not drawn");
            for(const auto &name:openLochCursorNames())require(!openLochCursor(name).pixmap().isNull(),"a pointer is not drawn");
            auto *palette=window.findChild<QToolBar*>("toolPalette");require(palette&&palette->parentWidget()==window.canvas->parentWidget()&&palette->orientation()==Qt::Vertical,"the tool palette is not beside the drawing area");
            bool icons=true;for(auto *a:palette->actions())icons&=!a->icon().isNull();require(icons,"a drawing tool has no symbol");
            for(const char *bar:{"fileBar","arrangeBar","editBar","alignBar","viewBar","outlineBar","zoomBar"})require(window.findChild<QToolBar*>(bar)&&!window.findChild<QToolBar*>(bar)->actions().isEmpty(),"a toolbar is missing");
            canvas->setTool("wire");require(!canvas->cursor().pixmap().isNull(),"the wire tool has no soldering iron pointer");canvas->setTool("select");require(canvas->cursor().shape()==Qt::ArrowCursor,"the select tool has no arrow");
        }
        { // Unit and main view belong to the board as in the original: a new board starts in N, a file brings its own
          // unit and switches, and each board of a project keeps its own
            Window fresh(tmp.path());require(fresh.canvas->unit()==2&&fresh.canvas->gridStep()==254,"a new board must start in the unit N");
            Project stored;stored.width=5080;stored.height=5080;stored.boardSettings=QJsonObject{{"unit",0},{"view",QJsonObject{{"flip",true},{"bitmaps",true},{"xray",true},{"through",false},{"potentials",false}}}};
            stored.save(tmp.filePath("Einheit.LM4"));require(fresh.openPath(tmp.filePath("Einheit.LM4")),"the board with its own unit did not open");
            require(fresh.canvas->unit()==0&&fresh.canvas->viewState().flip&&!fresh.canvas->potentialsShown(),"the board's unit or main view was not applied");
            action(fresh,"Einheit inch")->trigger();action(fresh,"boardAdd")->trigger();require(fresh.canvas->unit()==2&&!fresh.canvas->viewState().flip,"a new board did not start with the original's view");
            fresh.findChild<QTabBar*>("boardTabs")->setCurrentIndex(0);require(fresh.canvas->unit()==1&&fresh.canvas->viewState().flip,"the first board lost its unit or view");
        }
        { // Hilfe → Hilfethemen (F1): OpenLoch's own help in the language of the interface
            action(window,"helpTopics")->trigger();auto *helpWindow=window.findChild<QDialog*>("helpWindow");auto *page=helpWindow?helpWindow->findChild<QTextBrowser*>("helpPage"):nullptr;
            require(helpWindow&&helpWindow->isVisible()&&page&&page->toPlainText().contains("Erste Schritte")&&page->toPlainText().contains("Tastenkürzel"),"the help did not open");helpWindow->close();
        }
        { // AutoSpeichern like the original: the interval, the whole project as <name>.BAK next to its file, and an opened
          // backup that becomes the LM4 again
            Window backup(tmp.path());backup.show();QApplication::processEvents();
            QTimer::singleShot(0,[]{auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d)return;d->findChild<QSpinBox*>("autoSaveMinutes")->setValue(5);d->accept();});
            action(backup,"autoSave")->trigger();auto *timer=backup.findChild<QTimer*>("autoSaveTimer");
            require(timer&&timer->isActive()&&timer->interval()==5*60000,"AutoSpeichern did not take the interval");
            Project small;small.width=2540;small.height=2540;small.save(tmp.filePath("Sicherung.LM4"));require(backup.openPath(tmp.filePath("Sicherung.LM4")),"the project for the backup did not open");
            require(!backup.saveBackup(),"an unchanged project must not be backed up");
            auto *canvas=backup.canvas;auto press=[&](QPointF at){const QPointF local=canvas->mapFromScene(at);const QPointF global=canvas->viewport()->mapToGlobal(local.toPoint());
                QMouseEvent down(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(canvas->viewport(),&down);QApplication::sendEvent(canvas->viewport(),&up);};
            canvas->setTool("rectangle");press({508,508});press({1524,1524});
            require(backup.saveBackup()&&QFileInfo::exists(tmp.filePath("Sicherung.BAK")),"the backup was not written next to the file");
            require(Project::load(tmp.filePath("Sicherung.BAK")).legacy["objects"].toArray().size()==1,"the backup does not hold the changed project");
            QTimer::singleShot(0,[]{if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->button(QMessageBox::Discard)?box->button(QMessageBox::Discard)->click():box->reject();});
            require(backup.openPath(tmp.filePath("Sicherung.BAK"))&&backup.path()==QDir(tmp.path()).filePath("Sicherung.LM4"),"an opened backup did not become the LM4 again");
        }
        { // Platine → Eigenschaften like the original: name, size, the pitch and grids, origin and offset; one undo step
            Window props(tmp.path());props.show();QApplication::processEvents();
            QTimer::singleShot(0,[]{auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d)return;
                d->findChild<QLineEdit*>("boardName")->setText("Eigene Platine");d->findChild<QDoubleSpinBox*>("boardWidth")->setValue(60);
                d->findChild<QDoubleSpinBox*>("boardPitch")->setValue(2);d->findChild<QDoubleSpinBox*>("boardGridMm")->setValue(.5);d->findChild<QDoubleSpinBox*>("boardOffsetX")->setValue(1.27);d->accept();});
            action(props,"boardProperties")->trigger();const auto *board=props.canvas->document();
            require(board->title=="Eigene Platine"&&board->width==6000&&board->pitch()==2&&board->gridMm()==.5&&board->offset().x()==127&&props.canvas->gridStep()==200,"the board properties were not applied");
            QTimer::singleShot(0,[]{auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d)return;d->findChild<QDoubleSpinBox*>("boardPitch")->setValue(.05);d->accept();});
            QTimer::singleShot(50,[]{if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->accept();});
            action(props,"boardProperties")->trigger();require(props.canvas->document()->pitch()==.1,"a pitch below 0.1 mm must become 0.1 mm");
            action(props,"Rückgängig")->trigger();action(props,"Rückgängig")->trigger();board=props.canvas->document();
            require(board->pitch()==2.54&&board->width==10000&&board->offset().isNull()&&board->title!="Eigene Platine","undoing the board properties did not restore them");
        }
        { // Farben, Breite, Füllen and Fräsen as in the original: a drawing tool brings its colour and width, the toolbars
          // change the selection and the style of new objects; Werkzeuge anzeigen switches every toolbar
            auto *canvas=window.canvas;auto press=[&](QPointF at){const QPointF local=canvas->mapFromScene(at);const QPointF global=canvas->viewport()->mapToGlobal(local.toPoint());
                QMouseEvent down(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(canvas->viewport(),&down);QApplication::sendEvent(canvas->viewport(),&up);};
            for(const char *bar:{"fillBar","millBar","colourBar","widthBar"})require(window.findChild<QToolBar*>(bar)&&window.toolBarArea(window.findChild<QToolBar*>(bar))==Qt::BottomToolBarArea,"a bottom toolbar is missing");
            require(window.toolBarBreak(window.findChild<QToolBar*>("fillBar"))&&window.findChild<QToolBar*>("fillBar")->y()<window.findChild<QToolBar*>("colourBar")->y(),"Farben and Breite must sit below Füllen, Fräsen, Zoom and Kontur");
            auto *lineWidth=window.findChild<QComboBox*>("lineWidth");auto *grid=window.findChild<QWidget*>("colourGrid");require(lineWidth&&grid&&lineWidth->itemText(0)=="unsichtbar","the Breite toolbar is incomplete");
            canvas->setTool("wire");require(lineWidth->currentData().toInt()==40,"the wire tool must choose 0.4 mm");press({762,762});press({1524,762});
            auto drawn=canvas->document()->additions.last().toObject();require(drawn["type"]=="wire"&&drawn["color"]=="#c0c0c0"&&drawn["width"]==40,"a new wire is not silver and 0.4 mm");
            canvas->setTool("rectangle");require(lineWidth->currentData().toInt()==10,"the rectangle tool must choose 0.1 mm");
            {const QPointF at(3*18+9,14+7);QMouseEvent down(QEvent::MouseButtonPress,at,grid->mapToGlobal(at),Qt::RightButton,Qt::RightButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,at,grid->mapToGlobal(at),Qt::RightButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(grid,&down);QApplication::sendEvent(grid,&up);} // a right click on yellow chooses the fill colour
            press({762,1524});press({2032,2286});drawn=canvas->document()->additions.last().toObject();
            require(drawn["type"]=="rectangle"&&drawn["fill"]=="#ffff00"&&drawn["filled"].toBool()&&drawn["color"]=="#000000"&&drawn["width"]==10,"a new rectangle does not take the toolbar style");
            canvas->setTool("select");canvas->selectObject("new",canvas->document()->additions.size()-1);lineWidth->setCurrentIndex(lineWidth->findData(50));emit lineWidth->activated(lineWidth->currentIndex());
            window.findChild<QAction*>("fillAreas")->trigger();drawn=canvas->document()->additions.last().toObject();require(drawn["width"]==50&&!drawn["filled"].toBool(),"width and filling did not reach the selection");
            window.findChild<QAction*>("milled")->trigger();require(canvas->document()->additions.last().toObject()["flag3"].toBool(),"Kontur fräsen did not mill the selection");
            window.findChild<QAction*>("unmilled")->trigger();require(!canvas->document()->additions.last().toObject()["flag3"].toBool(),"Normale Kontur did not end milling");
            auto *colours=window.findChild<QToolBar*>("colourBar");colours->toggleViewAction()->trigger();require(colours->isHidden(),"Werkzeuge anzeigen did not hide a toolbar");
            action(window,"showAllToolbars")->trigger();require(colours->isVisible(),"Alle anzeigen did not show the toolbars");
        }
        { // English interface: menus and the main dialogs without a single missing translation
            setUiLanguage("en");
            {
                Window english(tmp.path());english.show();QApplication::processEvents();
                require(english.menuBar()->actions().first()->text()=="&File","the English menus are missing");
                QStringList titles;for(auto *m:english.menuBar()->actions())titles.append(m->text().remove('&'));
                require(titles.join(",")=="File,Edit,Arrange,Component,Library,Board,View,Check,Help","the menus are not in the original's order");
                auto find=[&](const QString &name){for(auto *a:english.findChildren<QAction*>()){bool dialog=false;for(auto *o=a->parent();o;o=o->parent())dialog|=qobject_cast<QDialog*>(o)!=nullptr;if(!dialog&&(a->objectName()==name||a->text().remove('&')==name))return a;}throw std::runtime_error(("English action missing: "+name).toStdString());};
                for(const char *name:{"Wizard…","exportHpgl","boardProperties","rotateAngle","X-ray contrast…","Edit layout…","Create…"}){
                    QTimer::singleShot(0,[]{if(auto *d=QApplication::activeModalWidget())d->close();});find(name)->trigger();QApplication::processEvents();
                }
                QTimer::singleShot(0,[]{if(auto *d=QApplication::activeModalWidget())d->close();});find("Print...")->trigger();QApplication::processEvents();
                find("Notes…")->trigger();english.findChild<QDialog*>("notesWindow")->close();
                find("Show object tree")->trigger();
                find("helpTopics")->trigger();require(english.findChild<QTextBrowser*>("helpPage")->toPlainText().contains("Getting started"),"the English help did not open");
            }
            const auto missing=missingTranslations();
            if(!missing.isEmpty())throw std::runtime_error(("English texts missing: "+QStringList(missing.begin(),missing.end()).join(" | ")).toStdString());
            setUiLanguage("fr");
            { // French like the original's language dialog: menus, the main dialogs and the help
                Window french(tmp.path());french.show();QApplication::processEvents();
                QStringList titles;for(auto *m:french.menuBar()->actions())titles.append(m->text().remove('&'));
                require(titles.join(",")=="Fichier,Édition,Disposer,Composant,Bibliothèque,Carte,Affichage,Vérifier,Aide","the French menus are missing");
                auto find=[&](const QString &name){for(auto *a:french.findChildren<QAction*>()){bool dialog=false;for(auto *o=a->parent();o;o=o->parent())dialog|=qobject_cast<QDialog*>(o)!=nullptr;if(!dialog&&(a->objectName()==name||a->text().remove('&')==name))return a;}throw std::runtime_error(("French action missing: "+name).toStdString());};
                for(const char *name:{"Assistant…","exportHpgl","boardProperties","rotateAngle","Contraste rayons X…","Modifier le tracé…","Créer…","autoSave","libraryProperties"}){
                    QTimer::singleShot(0,[]{if(auto *d=QApplication::activeModalWidget())d->close();});find(name)->trigger();QApplication::processEvents();
                }
                find("Remarques…")->trigger();french.findChild<QDialog*>("notesWindow")->close();
                find("helpTopics")->trigger();require(french.findChild<QTextBrowser*>("helpPage")->toPlainText().contains("Premiers pas"),"the French help did not open");
            }
            const auto missingFrench=missingTranslations();setUiLanguage("de");
            if(!missingFrench.isEmpty())throw std::runtime_error(("French texts missing: "+QStringList(missingFrench.begin(),missingFrench.end()).join(" | ")).toStdString());
        }
        { // Print preview like the original: sheets and their numbers, views, the data field, printing to PDF, settings kept
            QTemporaryDir tmp;Project board;board.width=10000;board.height=8000;
            QPrinter printer(QPrinter::HighResolution);printer.setOutputFormat(QPrinter::PdfFormat);printer.setOutputFileName(tmp.filePath("druck.pdf"));
            printer.setPageSize(QPageSize(QPageSize::A4));printer.setPageMargins(QMarginsF(5,5,5,5),QPageLayout::Millimeter);
            auto pdfPages=[&]{QFile f(printer.outputFileName());require(f.open(QIODevice::ReadOnly),"the PDF cannot be read");const auto b=f.readAll();return int(b.count("/Type /Page"))-int(b.count("/Type /Pages"));};
            {
                PrintDialog dialog(board,printer,"Druck",0);auto status=[&](int i){return dialog.findChild<QLabel*>(QString("printStatus%1").arg(i))->text();};
                require(dialog.sheets()==QSize(1,1)&&status(1)=="1 Blatt"&&status(2)=="1 Blatt zu drucken","a small board does not fit one sheet in the preview");
                require(!dialog.findChild<QCheckBox*>("cutMarks")->isEnabled()&&!dialog.findChild<QCheckBox*>("onlyOne")->isEnabled(),"cut marks or single sheets are offered for one sheet");
                dialog.findChild<QSpinBox*>("viewCount")->setValue(2);
                require(dialog.findChild<QComboBox*>("activeView")->count()==2&&dialog.findChild<QComboBox*>("activeView")->currentIndex()==1,"a new view count does not activate the last view");
                require(dialog.findChild<QLineEdit*>("positionLeft")->isEnabled()&&dialog.findChild<QLineEdit*>("positionLeft")->text()=="15,0","the second view is not cascaded 10 mm from the paper corner");
                dialog.findChild<QToolButton*>("print-view-flip")->setChecked(true);dialog.findChild<QCheckBox*>("layerRulers")->setChecked(false);
                dialog.findChild<QWidget*>("printPreview")->grab();
                require(dialog.print(printer)==1&&pdfPages()==1,"two views of a small board did not print on one page");
                dialog.findChild<QSpinBox*>("copies")->setValue(2);require(status(2)=="2 Blätter zu drucken","copies are not counted");
                require(dialog.print(printer)==2&&pdfPages()==2,"copies did not print");
                dialog.findChild<QSpinBox*>("copies")->setValue(1);dialog.accept();
            }
            const auto kept=printSetupOf(board);require(kept.count==2&&kept.views[1].flip&&!kept.views[1].rulers,"print settings are not kept with the board");
            Project wide;wide.width=25000;wide.height=8000;
            {
                PrintDialog dialog(wide,printer,"Druck",0);auto status=[&](int i){return dialog.findChild<QLabel*>(QString("printStatus%1").arg(i))->text();};
                require(dialog.sheets()==QSize(2,1)&&status(1)=="2 Blätter"&&dialog.findChild<QCheckBox*>("cutMarks")->isEnabled(),"a wide board does not continue on a second sheet");
                require(dialog.print(printer)==2&&pdfPages()==2,"both sheets were not printed");
                dialog.findChild<QCheckBox*>("onlyOne")->setChecked(true);dialog.findChild<QSpinBox*>("sheetNumber")->setValue(2);
                require(status(2)=="1 Blatt zu drucken"&&dialog.print(printer)==1&&pdfPages()==1,"only one sheet was not printed");
                dialog.findChild<QRadioButton*>("landscape")->setChecked(true);require(dialog.sheets()==QSize(1,1),"landscape does not fit the wide board on one sheet");
                dialog.findChild<QWidget*>("printPreview")->grab();dialog.reject();
            }
        }
        if(argc==3&&QString::fromLocal8Bit(argv[1])=="--screenshot"){QApplication::processEvents();require(window.grab().save(QString::fromLocal8Bit(argv[2])),"window screenshot failed");}
        QTextStream(stdout)<<"Window library panel, drag placement, properties, template and undo/redo workflows passed\n";return 0;
    }catch(const std::exception &e){QTextStream(stderr)<<e.what()<<"\n";return 1;}
}
