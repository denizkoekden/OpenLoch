// Tests of the front panel editor on an offscreen window: drawing with the mouse, editing commands, panels, files,
// the library page, HPGL machining jobs and the print layout. Settings go to a temporary folder.
#include "paneleditor.h"
#include "panelboards.h"
#include "panelgeometry.h"
#include "panelmachining.h"
#include "panelprint.h"
#include "panelrender.h"
#include "panelsidebar.h"
#include "panelview.h"
#include "panelwizards.h"
#include "strokefont.h"
#include "fpl.h"
#include "language.h"
#include <QAction>
#include <QLabel>
#include <QTextBrowser>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QJsonObject>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPrinter>
#include <QRadioButton>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QTabBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <algorithm>
#include <cmath>

using namespace openloch;
using namespace openloch::frontpanel;

namespace fptest {
void require(bool ok,const char *message);
bool near(double a,double b,double tolerance);
bool near(QPointF a,QPointF b,double tolerance);
void rejects(const std::function<void()> &fn,const char *message);
}
using fptest::require;using fptest::near;using fptest::rejects;

namespace {
void mouse(PanelView *view,QEvent::Type type,QPointF panelPoint,Qt::MouseButton button=Qt::LeftButton,Qt::KeyboardModifiers modifiers=Qt::NoModifier){
    QWidget *w=view->viewport();const QPointF at=view->toView(panelPoint);
    QMouseEvent e(type,at,w->mapToGlobal(at),button,type==QEvent::MouseButtonRelease?Qt::NoButton:button,modifiers);QApplication::sendEvent(w,&e);
}
void click(PanelView *view,QPointF at,Qt::KeyboardModifiers modifiers=Qt::NoModifier){mouse(view,QEvent::MouseButtonPress,at,Qt::LeftButton,modifiers);mouse(view,QEvent::MouseButtonRelease,at,Qt::LeftButton,modifiers);}
void dragTo(PanelView *view,QPointF from,QPointF to){
    mouse(view,QEvent::MouseButtonPress,from);mouse(view,QEvent::MouseMove,(from+to)/2);mouse(view,QEvent::MouseMove,to);mouse(view,QEvent::MouseButtonRelease,to);
}
Element rectangle(QRectF r){Element e=newElement(ElementType::Rectangle);e.points={r.topLeft(),r.topRight(),r.bottomRight(),r.bottomLeft()};e.fill=Fill{FillStyle::Solid,Qt::red,Qt::red,Gradient::None};return e;}
QRectF shape(const Element &e){return e.isContainer()&&!e.combined()?elementsBounds(e.children):elementPath(e).boundingRect();}
const Element *byId(const Panel &p,const QString &id){for(const auto &e:p.elements)if(e.id==id)return &e;return nullptr;}
bool sameRect(const QRectF &a,const QRectF &b){return near(a.topLeft(),b.topLeft(),1e-6)&&near(a.bottomRight(),b.bottomRight(),1e-6);}

void drawingTests(PanelEditor &editor){
    Document d;d.panels[0]=newPanel("Test",100,60);editor.setDocument(d);
    PanelView *view=editor.view();view->fitPanel();auto P=[&]()->Panel&{return editor.document().panel();};
    // A rectangle drawn by dragging, its corners on the grid.
    view->setTool(PanelView::Rectangle);dragTo(view,{10.2,10.1},{30.3,19.8});
    require(P().elements.size()==1&&P().elements[0].type==ElementType::Rectangle,"rectangle drawn");
    require(shape(P().elements[0])==QRectF(10,10,20,10),"rectangle snapped to the grid");
    // A line with three clicks, finished with the right button.
    view->setTool(PanelView::Line);click(view,{5,40});click(view,{20,40});click(view,{20,50});
    mouse(view,QEvent::MouseButtonPress,{20,50},Qt::RightButton);mouse(view,QEvent::MouseButtonRelease,{20,50},Qt::RightButton);
    require(P().elements.size()==2&&P().elements[1].type==ElementType::Line&&P().elements[1].points.size()==3,"line drawn");
    // A circle: centre, then radius.
    view->setTool(PanelView::Circle);dragTo(view,{70,30},{75,30});
    require(P().elements.size()==3&&P().elements[2].type==ElementType::Ellipse&&near(P().elements[2].radiusX,5,1e-9),"circle drawn");
    // Undo takes the circle away again, redo brings it back.
    editor.undo();require(P().elements.size()==2,"undo drawing");editor.redo();require(P().elements.size()==3,"redo drawing");
    // Moving with the mouse goes by whole grid steps.
    view->setTool(PanelView::Select);const QString id=P().elements[0].id;click(view,{10,13});
    require(view->selection()==QStringList{id},"click selects");
    dragTo(view,{10,13},{14.6,15.2});require(shape(*byId(P(),id)).topLeft()==QPointF(15,12),"moved on the grid");
    // A click on the selected element switches to turning handles.
    click(view,{15,14});require(view->rotateHandles&&editor.action("tool-rotate")->isChecked(),"second click turns");
    view->setTool(PanelView::Select);require(!view->rotateHandles,"select tool leaves turning");
    // At the edge of the view the content scrolls by itself while an element is dragged.
    view->showArea(shape(*byId(P(),id)).adjusted(-5,-5,5,5));
    {const QRectF r=shape(*byId(P(),id));const QPointF grip(r.left(),r.top()+2);mouse(view,QEvent::MouseButtonPress,grip);
     const QPointF edge=view->toPanel(QPointF(view->viewport()->width()-3,view->toView(grip).y()));mouse(view,QEvent::MouseMove,(grip+edge)/2);mouse(view,QEvent::MouseMove,edge);
     const int before=view->horizontalScrollBar()->value();QElapsedTimer clock;clock.start();while(clock.elapsed()<250)QApplication::processEvents();
     require(view->horizontalScrollBar()->value()>before,"the view scrolls at its edge");mouse(view,QEvent::MouseButtonRelease,edge);}
    editor.undo();view->fitPanel();require(shape(*byId(P(),id)).topLeft()==QPointF(15,12),"undo the scrolled move");
    // Size and position from the toolbar functions.
    editor.resizeSelection(40,20);require(near(shape(*byId(P(),id)).width(),40,1e-9)&&near(shape(*byId(P(),id)).height(),20,1e-9),"resize selection");
    editor.moveSelection(0,0);require(near(shape(*byId(P(),id)).topLeft(),QPointF(0,0),1e-9),"move selection");
    view->clearSelection();editor.resizeSelection(120,70);require(P().width==120&&P().height==70,"without a selection the panel is resized");
}

void arrangeTests(PanelEditor &editor){
    Document d;d.panels[0]=newPanel("Anordnen",200,100);
    const Element a=rectangle({10,10,10,10}),b=rectangle({40,30,20,10}),c=rectangle({100,50,10,20});d.panels[0].elements={a,b,c};
    editor.setDocument(d);PanelView *view=editor.view();auto P=[&]()->Panel&{return editor.document().panel();};
    // Aligning: the element selected last is the reference.
    view->setSelection({a.id,c.id,b.id});editor.align(0);
    require(near(shape(*byId(P(),a.id)).left(),40,1e-9)&&near(shape(*byId(P(),c.id)).left(),40,1e-9)&&near(shape(*byId(P(),b.id)).left(),40,1e-9),"align left");
    editor.align(5);require(near(shape(*byId(P(),a.id)).bottom(),40,1e-9)&&near(shape(*byId(P(),c.id)).bottom(),40,1e-9),"align bottom");
    editor.undo();editor.undo();require(near(shape(*byId(P(),a.id)).left(),10,1e-9),"undo align");
    // Distributing the centres evenly keeps the outer ones.
    DistributeOptions o;o.horizontal=true;o.horizontalReference=0;o.horizontalMode=0;editor.distribute(o);
    require(near(shape(*byId(P(),b.id)).center().x(),(15+105)/2.0,1e-9)&&near(shape(*byId(P(),a.id)).center().x(),15,1e-9),"distribute centres");
    // Equal gaps of 5 mm between neighbouring sides.
    o.horizontalReference=3;o.horizontalMode=2;o.horizontalValue=5;editor.distribute(o);
    require(near(shape(*byId(P(),b.id)).left(),25,1e-9)&&near(shape(*byId(P(),c.id)).left(),50,1e-9),"distribute with gaps");
    // Grid alignment of the left sides.
    editor.change([&](Document &doc){transformElement(doc.panel().elements[0],QTransform::fromTranslate(0.4,0));});
    GridAlignOptions g;g.horizontal=true;g.horizontalReference=1;view->setSelection({a.id});editor.alignToGrid(g);require(near(shape(*byId(P(),a.id)).left(),10,1e-9),"align to grid");
    // Order: to the back and to the front.
    view->setSelection({c.id});editor.toBack();require(P().elements.first().id==c.id,"to back");editor.toFront();require(P().elements.last().id==c.id,"to front");
    // Mirroring and turning about the middle of the selection.
    view->setSelection({b.id});const QRectF before=shape(*byId(P(),b.id));editor.rotateSelected(90);
    const QRectF turned=shape(*byId(P(),b.id));require(near(turned.width(),before.height(),1e-9)&&near(turned.center(),before.center(),1e-9),"rotate");
    editor.mirror(true);require(near(shape(*byId(P(),b.id)).center(),before.center(),1e-9),"mirror keeps the middle");
}

void groupTests(PanelEditor &editor){
    Document d;d.panels[0]=newPanel("Gruppen",100,100);
    Element a=rectangle({10,10,40,40}),b=newElement(ElementType::Ellipse);b.center={30,30};b.radiusX=b.radiusY=10;Element t=newElement(ElementType::Text);t.text="A";t.frame=rectFrame({60,60,5,5});
    d.panels[0].elements={a,b,t};editor.setDocument(d);PanelView *view=editor.view();auto P=[&]()->Panel&{return editor.document().panel();};
    view->setSelection({a.id,b.id});editor.group();
    require(P().elements.size()==2&&P().elements[0].type==ElementType::Group&&P().elements[0].children.size()==2&&P().elements[1].id==t.id,"group keeps the drawing order");
    const QString group=P().elements[0].id;require(view->selection()==QStringList{group},"group is selected");
    // A part chosen inside the group (object tree) cannot be deleted on its own.
    view->setSelection({a.id});editor.removeSelected();require(P().elements[0].children.size()==2,"parts of a group stay");
    view->setSelection({group});editor.ungroup();require(P().elements.size()==3&&P().elements[0].id==a.id&&P().elements[1].id==b.id,"ungroup");
    // A combination: one shape with a hole, filled even-odd, written as TKombination.
    view->setSelection({a.id,b.id});editor.combine();
    require(P().elements.size()==2&&P().elements[0].combined()&&P().elements[0].children.size()==2,"combine");
    const Element &c=P().elements[0];require(!elementPath(c).contains(QPointF(30,30))&&elementPath(c).contains(QPointF(15,15)),"combination has a hole");
    require(hitElement(c,QPointF(15,15),0.1)&&!hitElement(c,QPointF(30,30),0.1),"combination is hit on its area only");
    const Document back=frontdesigner::readFrontDesigner(frontdesigner::writeFrontDesigner(editor.document()));
    require(back.panels[0].elements[0].combined()&&back.panels[0].elements[0].children.size()==2,"combination survives FrontDesigner files");
    // Like the original, pen and tool given to a combination reach its parts, which are machined each on its own.
    require(c.children[1].pen==c.pen&&c.children[1].machining==c.machining,"parts take pen and tool of the combination");
    view->setSelection({c.id});editor.applyPen(Pen{Qt::darkGreen,0.6,PenStyle::Solid},Machining::Mill);
    require(P().elements[0].machining==Machining::Mill&&std::all_of(P().elements[0].children.begin(),P().elements[0].children.end(),
            [](const Element &part){return part.machining==Machining::Mill&&near(part.pen.width,0.6,1e-12);}),"pen and tool of a combination passed on to its parts");
    view->setSelection({P().elements[0].id});editor.uncombine();require(P().elements.size()==3&&!P().elements[0].combined(),"uncombine");
    // Pen and fill: texts change only when nothing but texts is selected.
    view->setSelection({a.id,t.id});editor.applyPen(Pen{Qt::blue,0.8,PenStyle::Solid},Machining::None);
    require(byId(P(),a.id)->pen.color==QColor(Qt::blue)&&byId(P(),t.id)->pen!=Pen({Qt::blue,0.8,PenStyle::Solid}),"pen skips texts in a mixed selection");
    view->setSelection({t.id});editor.applyFill(Fill{FillStyle::Solid,Qt::green,Qt::green,Gradient::None});require(byId(P(),t.id)->fill.color==QColor(Qt::green),"fill of a text alone");
    editor.applyFont("Courier",5,true,false,{});require(near(QLineF(frameCorners(byId(P(),t.id)->frame)[0],frameCorners(byId(P(),t.id)->frame)[3]).length(),5,1e-6)&&byId(P(),t.id)->bold,"font height");
    // Contours only for lines, polygons and rectangles.
    view->setSelection({a.id,b.id});editor.setContour(Corners::Round);require(byId(P(),a.id)->contour.corners==Corners::Round,"contour rounded");
}

void treeTests(PanelEditor &editor){
    // The object tree shows groups with their parts; choosing a part there selects it inside its group.
    Document d;d.panels[0]=newPanel("Baum",100,100);const Element a=rectangle({10,10,10,10}),b=rectangle({30,10,10,10});
    Element pair=newElement(ElementType::Group);pair.children={a,b};pair.name="Paar";d.panels[0].elements={pair};editor.setDocument(d);
    if(!editor.action("objectTree")->isChecked())editor.action("objectTree")->trigger();QApplication::processEvents();
    auto *tree=editor.findChild<QTreeWidget*>();require(tree&&tree->topLevelItemCount()==1&&tree->topLevelItem(0)->childCount()==2,"object tree lists groups and parts");
    tree->topLevelItem(0)->child(1)->setSelected(true);require(editor.view()->selection()==QStringList{b.id},"choosing a part in the tree selects it");
    editor.view()->setSelection({pair.id});require(tree->topLevelItem(0)->isSelected()&&!tree->topLevelItem(0)->child(1)->isSelected(),"the tree follows the selection");
    editor.action("objectTree")->trigger();QApplication::processEvents();
}

// Answers the next modal dialog as soon as it is shown.
void respond(const std::function<void(QDialog*)> &act){QTimer::singleShot(0,[act]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget()))act(d);});}
void accept(QDialog *d){d->accept();}
void reject(QDialog *d){d->reject();}
void dialogTests(PanelEditor &editor){
    // Every style of the scale assistant builds and comes back as chosen.
    auto styleBox=[](QDialog *d)->QComboBox*{
        for(auto *c:d->findChildren<QComboBox*>())if(c->count()==int(ScaleParameters::Sine)+1&&c->itemText(0)==scaleStyleTitle(ScaleParameters::StraightLinear))return c;return nullptr;};
    for(int style=0;style<=int(ScaleParameters::Sine);style++){
        ScaleParameters q;
        respond([&,style](QDialog *d){if(auto *c=styleBox(d))c->setCurrentIndex(style);d->accept();});
        require(scaleWizard(&editor,q)&&int(q.style)==style&&q.values.size()==ScaleParameters::info(q.style).size(),"scale assistant styles");
    }
    // Values from the parameter list, texts from the labels page, the look of the part chosen in the list.
    ScaleParameters q(ScaleParameters::RoundLinear);
    respond([](QDialog *d){
        QTableWidget *grid=nullptr,*texts=nullptr;for(auto *t:d->findChildren<QTableWidget*>())(t->columnCount()==3?grid:texts)=t;
        qobject_cast<QDoubleSpinBox*>(grid->cellWidget(ScaleParameters::RoundRange,1))->setValue(200);
        qobject_cast<QCheckBox*>(grid->cellWidget(ScaleParameters::RoundCentre,1))->setChecked(false);
        qobject_cast<QSpinBox*>(grid->cellWidget(ScaleParameters::RoundDivisions1,1))->setValue(4);
        if(texts->rowCount()==5)texts->item(0,0)->setText("Min");
        auto *list=d->findChild<QListWidget*>();list->clearSelection();list->item(1)->setSelected(true);
        for(auto *s:d->findChildren<QDoubleSpinBox*>())if(s->suffix()==" mm")s->setValue(0.35);
        d->accept();});
    require(scaleWizard(&editor,q)&&q.values[ScaleParameters::RoundRange]==200&&q.values[ScaleParameters::RoundCentre]==0&&q.values[ScaleParameters::RoundDivisions1]==4
        &&q.labelTexts()==QStringList{"Min","1","2","3","4"}&&near(q.design[1].pen.width,0.35,1e-9)&&q.design[2].pen.width==0,"scale assistant values, texts and look");
    CutoutParameters cut;respond(accept);require(cutoutDialog(&editor,cut)&&cut.din&&cut.frameWidth==96,"cut-out dialog");
    Document &doc=editor.document();
    respond(reject);require(!exportMachining(&editor,doc,doc.panel(),{}),"machining export cancelled");
    respond(reject);require(!printPanels(&editor,doc,"Test"),"print preview cancelled");
    // The preview works on the settings of each panel and hands them back, also when it is cancelled. A margin typed
    // in is the panel's distance from the edge of the paper.
    {
        Document two=doc;two.panels<<newPanel("Rückseite",80,40);two.panels[1].print.tilesX=2;two.activePanel=0;
        QPrinter printer(QPrinter::HighResolution);const double printable=printer.pageLayout().paintRect(QPageLayout::Millimeter).left();
        respond([](QDialog *d){
            for(auto *b:d->findChildren<QRadioButton*>())if(b->text()==ui("&Spiegeln"))b->setChecked(true);
            for(auto *b:d->findChildren<QCheckBox*>())if(b->text()==ui("&mittig ausrichten"))b->setChecked(false);
            for(auto *s:d->findChildren<QDoubleSpinBox*>())if(s->isEnabled()&&s->suffix()==" mm"){s->setValue(30);break;}
            d->findChild<QTabBar*>()->setCurrentIndex(1);
            for(auto *b:d->findChildren<QCheckBox*>())if(b->text()==ui("Sch&nittmarken"))b->setChecked(false);
            d->reject();});
        QList<PrintSettings> back;require(!printPanels(&editor,two,"Test",&back)&&back.size()==2,"print settings handed back");
        require(back[0].mirror&&!back[0].centred&&back[0].cutMarks&&near(back[0].left,30-printable,1e-9),"print settings of the first panel");
        require(back[1].tilesX==2&&!back[1].cutMarks&&!back[1].mirror&&back[1].centred,"print settings of the second panel");
        // In the editor they go into the document as a step that can be undone.
        respond([](QDialog *d){for(auto *b:d->findChildren<QCheckBox*>())if(b->text()==ui("Da&ten"))b->setChecked(false);d->reject();});
        editor.action("print")->trigger();require(!editor.document().panel().print.data&&editor.modified(),"print settings kept with the panel");
        editor.undo();require(editor.document().panel().print.data,"print settings undone");
    }
    // A host can take over saving: the editor calls it instead of writing a file, the host takes the document data and
    // reports the save.
    {
        int calls=0;bool asNew=false;QJsonObject taken;
        editor.saveHandler=[&](bool fresh){calls++;asNew=fresh;taken=editor.documentData();editor.markSaved();return true;};
        editor.change([](Document &d){d.title="Im Projekt";});require(editor.modified(),"changed before the host saves");
        require(editor.save()&&calls==1&&!asNew&&!editor.modified()&&Document::fromJson(taken).title=="Im Projekt","saving through the host");
        require(editor.saveAs()&&calls==2&&asNew,"saving under a new name through the host");
        editor.saveHandler=nullptr;
    }
    // The unit of rulers and coordinates belongs to the panel; switching it can be undone.
    {
        QToolButton *unit=nullptr;for(auto *b:editor.findChildren<QToolButton*>())if(b->text()=="mm")unit=b;
        require(unit&&!editor.document().panel().inch,"unit button of the rulers");
        unit->click();require(editor.document().panel().inch&&unit->text()=="inch","unit of the panel switched");
        editor.undo();require(!editor.document().panel().inch&&unit->text()=="mm","unit of the panel switched back");
    }
    Panel panel=doc.panel();respond(accept);require(editPanelProperties(&editor,panel,false),"panel properties");
    respond(accept);require(editGrid(&editor,panel),"grid dialog");
    Element text=newElement(ElementType::Text);text.text="Abc";text.frame=rectFrame({0,0,10,3});respond(accept);require(editText(&editor,text,false)&&text.text=="Abc","text dialog");
    for(ElementType type:{ElementType::Rectangle,ElementType::Ellipse,ElementType::Arc,ElementType::Drill}){
        Element e=type==ElementType::Rectangle?rectangle({0,0,10,10}):newElement(type);if(type!=ElementType::Rectangle){e.center={20,20};e.radiusX=e.radiusY=5;e.diameter=3;}
        respond(accept);require(editElementProperties(&editor,doc,e),"properties dialog");
    }
    Element dim=dimension({0,0},{30,0},{15,5});respond(accept);require(editElementProperties(&editor,doc,dim)&&near(dimensionValue(dim),30,1e-9),"dimension properties");
    // A line with the arc spline of the original keeps it through the properties dialog.
    {Element bent=newElement(ElementType::Line);bent.points={{0,0},{10,0},{10,10}};bent.contour={Corners::ArcSpline,0};respond(accept);
     require(editElementProperties(&editor,doc,bent)&&bent.contour.corners==Corners::ArcSpline,"arc spline in the properties dialog");}
    DistributeOptions distributeOptions;respond(accept);require(askDistribute(&editor,distributeOptions),"distribute dialog");
    GridAlignOptions gridOptions;respond(accept);require(askAlignToGrid(&editor,gridOptions),"grid align dialog");
    int corners=6;double radius=10,start=90;bool inner=false;respond(accept);require(askRegularPolygon(&editor,corners,radius,inner,start)&&corners==6,"polygon dialog");
    DimensionStyle style;bool automatic=true,ask=false;respond(accept);require(editDimensionStyle(&editor,style,automatic,ask),"dimension defaults");
    // The export dialog names the memory of the picture as FrontDesigner does: 100 × 50 mm at 300 dpi are 1181 × 591 pixels.
    ImageExportOptions image;QString memory;respond([&](QDialog *d){if(auto *l=d->findChild<QLabel*>("exportMemory"))memory=l->text();d->accept();});
    require(askImageExport(&editor,image,QSizeF(100,50),false)&&image.format=="PNG"&&memory.contains("MB"),"image export dialog");
    Element symbol=rectangle({0,0,10,10});respond(accept);require(editSymbol(&editor,doc,symbol,1)&&symbol.hasAnchor,"symbol dialog");
    respond(accept);require(editStrokeFontOrders(&editor),"character order of the stroke fonts");
    // A symbol of the library keeps its dialog under FrontDesigner's title; the language as in its language dialog.
    QString title;respond([&](QDialog *d){title=d->windowTitle();d->accept();});
    require(editSymbol(&editor,doc,symbol,1,ui("Eigenschaften Symbol"))&&title=="Eigenschaften Symbol","symbol properties of a library symbol");
    int languages=0,chosen=0;for(const char *code:{"de","en","fr"})if(auto *a=editor.findChild<QAction*>(QString("language-")+code)){languages++;chosen+=a->isChecked();}
    require(languages==3&&chosen==1,"the language in the front panel window");
    // Hilfe → Hilfethemen… (F1 on every system) shows the module's own help page.
    {auto *topics=editor.findChild<QAction*>("helpTopics");require(topics&&topics->shortcuts().contains(QKeySequence(Qt::Key_F1)),"F1 opens the front panel's help");
     topics->trigger();auto *page=editor.findChild<QTextBrowser*>("helpPage");require(page&&page->toPlainText().contains("Frontplatte – Hilfe")&&page->toPlainText().contains("Strichschriften"),"the front panel's help page");
     editor.findChild<QDialog*>("helpWindow")->close();}
}

void clipboardTests(PanelEditor &editor){
    Document d;d.panels[0]=newPanel("Ablage",100,100);const Element a=rectangle({10,10,10,10});d.panels[0].elements={a};editor.setDocument(d);
    PanelView *view=editor.view();auto P=[&]()->Panel&{return editor.document().panel();};view->fitPanel();
    view->setSelection({a.id});editor.copy();
    const QMimeData *mime=QApplication::clipboard()->mimeData();require(mime&&mime->hasFormat("application/x-openloch-frontpanel")&&mime->hasImage(),"copy puts elements and a picture on the clipboard");
    // Pasted elements stick to the cursor until a click places them with their corner on the grid.
    editor.paste();require(view->placing(),"paste waits for a place");click(view,{50.3,60.2});
    require(P().elements.size()==2&&P().elements[1].id!=a.id&&shape(P().elements[1]).topLeft()==QPointF(50,60),"pasted at the click");
    editor.undo();require(P().elements.size()==1,"undo paste");
    // Duplicate works the same way; the right button cancels.
    view->setSelection({a.id});editor.duplicate();mouse(view,QEvent::MouseButtonPress,{50,50},Qt::RightButton);require(!view->placing()&&P().elements.size()==1,"cancel placing");
    view->setSelection({a.id});editor.cut();require(P().elements.isEmpty()&&QApplication::clipboard()->mimeData()->hasFormat("application/x-openloch-frontpanel"),"cut");
}

void panelTests(PanelEditor &editor,const QString &folder){
    Document d;d.panels[0]=newPanel("Vorne",100,50);d.panels[0].elements={rectangle({10,10,10,10})};editor.setDocument(d);Document &doc=editor.document();
    editor.addPanel(true);require(doc.panels.size()==2&&doc.activePanel==1&&doc.panels[1].elements.size()==1&&doc.panels[1].elements[0].id!=doc.panels[0].elements[0].id,"duplicate panel");
    editor.movePanel(-1);require(doc.activePanel==0&&doc.panels[0].name.endsWith("(Kopie)"),"move panel left");
    editor.selectPanel(1);require(doc.activePanel==1&&doc.panel().name=="Vorne","select panel");
    editor.undo();editor.undo();require(doc.panels.size()==1,"undo panel changes");
    // Saving and opening in the native format and as FrontDesigner project, with a saved view of the second panel.
    editor.addPanel(true);editor.change([](Document &doc){doc.views<<View{"Ausschnitt",1,QRectF(5,5,40,20)};});
    for(const QString name:{"panel.olfp","panel.FPL"}){
        const QString file=QDir(folder).filePath(name);require(editor.saveAs(file)&&!editor.modified(),"save");
        PanelEditor other;require(other.open(file),"open again");
        require(other.document().panels.size()==2&&other.document().panels[0].elements.size()==1&&near(other.document().panels[1].width,100,1e-6),"saved panels come back");
        const auto &views=other.document().views;require(views.size()==1&&views[0].name=="Ausschnitt"&&views[0].panel==1&&sameRect(views[0].area,QRectF(5,5,40,20)),"saved views come back");
    }
    // A front panel file dropped on the window opens.
    {PanelEditor target;QMimeData mime;mime.setUrls({QUrl::fromLocalFile(QDir(folder).filePath("panel.olfp"))});
     QDragEnterEvent enter(QPoint(10,10),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&target,&enter);require(enter.isAccepted(),"dropping a file is accepted");
     QDropEvent drop(QPointF(10,10),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&target,&drop);
     require(target.filePath().endsWith("panel.olfp")&&target.document().panels.size()==2,"a dropped file opens");}
    // A backup is written beside the project and opens as the project it belongs to.
    editor.change([](Document &doc){doc.panel().name="Geändert";});editor.saveAs(QDir(folder).filePath("backup.olfp"));
    editor.change([](Document &doc){doc.panel().name="Nach dem Speichern";});editor.writeBackup();
    const QString backup=QDir(folder).filePath("backup.BAK");require(QFile::exists(backup),"backup written");
    PanelEditor restored;require(restored.open(backup)&&restored.filePath()==QDir(folder).filePath("backup.olfp")&&restored.modified()&&restored.document().panel().name=="Nach dem Speichern","backup opens as its project");
    // A document with a font that is not installed opens with the overview of the used fonts, which does not block.
    {
        Document missing;Element text=newElement(ElementType::Text);text.text="Ω";text.font="Keine Schrift dieses Namens";text.frame=rectFrame(QRectF(5,5,20,5));missing.panel().elements={text};
        require(missingFonts(missing)==QStringList{"Keine Schrift dieses Namens"}&&missingFonts(d).isEmpty(),"missing fonts of a document");
        const QString file=QDir(folder).filePath("fonts.olfp");{PanelEditor writer;writer.setDocument(missing);require(writer.saveAs(file),"save a document with a missing font");}
        PanelEditor reader;require(reader.open(file),"open a document with a missing font");QApplication::processEvents();
        QDialog *overview=reader.findChild<QDialog*>("usedFonts");require(overview&&overview->isVisible()&&!overview->isModal(),"the used fonts open by themselves when one is missing");
        overview->close();
    }
}

// The pen-down strokes of an HPGL file in panel millimetres (y down again); false for anything this module does not
// write or for a broken sequence.
bool readHpgl(const QByteArray &file,double panelHeight,QList<QPolygonF> &strokes){
    if(!file.endsWith("PU;\r\nPA0,0;\r\n"))return false;
    bool down=false;QPointF at;QPolygonF current;
    for(QByteArray command:file.split(';')){
        command=command.trimmed();if(command.isEmpty()||command=="IN"||command=="PT0"||command=="SP1"||command=="SP2")continue;
        if(command=="PU"){if(down)strokes<<current;current.clear();down=false;continue;}
        if(command=="PD"){if(down)return false;down=true;current={at};continue;}
        if(!command.startsWith("PA"))return false;
        const QList<QByteArray> xy=command.mid(2).split(',');bool okX=false,okY=false;if(xy.size()!=2)return false;
        const int x=xy[0].toInt(&okX),y=xy[1].toInt(&okY);if(!okX||!okY)return false;
        at=QPointF(x/40.0,panelHeight-y/40.0);if(down)current<<at;
    }
    return !down;
}
// Read back, every job gives one stroke per path, through the same points within the rounding to fiftieths of a
// millimetre and plotter units.
void hpglReadBack(){
    Document d;Panel &p=d.panels[0];p.width=120;p.height=80;
    Element hole=newElement(ElementType::Drill);hole.center={12.34,56.78};hole.diameter=3;
    Element big=newElement(ElementType::Drill);big.center={30,20};big.diameter=8;
    Element ring=newElement(ElementType::Ellipse);ring.center={60,40};ring.radiusX=15;ring.radiusY=9;ring.rotation=25;ring.machining=Machining::Mill;ring.pen.width=2;
    Element arc=newElement(ElementType::Arc);arc.center={90,60};arc.radiusX=arc.radiusY=10;arc.startAngle=30;arc.spanAngle=200;arc.machining=Machining::Engrave;arc.pen.width=0.4;
    Element curve=newElement(ElementType::Polygon);curve.points={{70,10},{110,12},{105,30},{80,25}};curve.contour={Corners::Round,3};curve.machining=Machining::Mill;curve.pen.width=1;
    Element text=newElement(ElementType::Text);text.text="OL";text.frame=rectFrame(QRectF(10,62,16,8));text.machining=Machining::Engrave;text.pen.width=0.2;
    p.elements={hole,big,ring,arc,curve,text};
    for(const MachiningOptions &o:{MachiningOptions(),[]{MachiningOptions m;m.millDrills=true;m.tool=2;m.outline=true;m.commonOrigin=true;return m;}()}){
        const auto jobs=machiningJobs(d,p,o);require(!jobs.isEmpty(),"jobs for the read back");
        for(const MachiningJob &job:jobs){
            QList<QPolygonF> strokes;require(readHpgl(hpgl(job,p,o),p.height,strokes),"HPGL file readable");
            if(o.commonOrigin){require(!strokes.isEmpty()&&strokes.first().size()==1&&strokes.first().first()==QPointF(0,p.height),"common origin");strokes.removeFirst();}
            require(strokes.size()==job.paths.size(),"one stroke per path");
            for(int i=0;i<strokes.size();i++){
                const QPolygonF &want=job.paths[i].points,&got=strokes[i];
                // A drill is a plunge: down and the same point again.
                const QPolygonF expected=want.size()==1?QPolygonF{want[0],want[0]}:want;
                require(got.size()==expected.size(),"stroke with the points of its path");
                for(int k=0;k<got.size();k++)require(std::abs(got[k].x()-expected[k].x())<=0.026&&std::abs(got[k].y()-expected[k].y())<=0.026,"stroke points within the rounding");
            }
        }
    }
}
void machiningTests(){
    hpglReadBack();
    Document d;Panel &p=d.panels[0];p.width=100;p.height=50;
    Element d3=newElement(ElementType::Drill);d3.center={10,10};d3.diameter=3;Element d5=newElement(ElementType::Drill);d5.center={20,10};d5.diameter=5;
    Element d3b=d3;d3b.id=newId();d3b.center={30,10};
    Element milled=rectangle({40,20,10,10});milled.machining=Machining::Mill;milled.pen.width=1;
    Element engraved=newElement(ElementType::Line);engraved.points={{60,30},{80,30}};engraved.machining=Machining::Engrave;engraved.pen.width=0.3;
    Element drawn=rectangle({5,30,5,5});
    Element group=newElement(ElementType::Group);group.children={d5,drawn};
    p.elements={d3,group,d3b,milled,engraved};
    MachiningOptions o;auto jobs=machiningJobs(d,p,o);
    require(jobs.size()==4&&jobs[0].kind==MachiningJob::Drill&&near(jobs[0].tool,3,1e-9)&&jobs[0].paths.size()==2&&near(jobs[1].tool,5,1e-9)
            &&jobs[2].kind==MachiningJob::Mill&&near(jobs[2].tool,1,1e-9)&&jobs[3].kind==MachiningJob::Engrave,"jobs by tool, drills inside groups too, plain pen not machined");
    require(jobs[0].title()=="Bohren mit 3 mm"&&jobs[2].fileName()=="Fräsen mit 1 mm.PLT"&&jobs[3].title()=="Gravieren mit 0,3 mm","job names");
    const QByteArray drill=hpgl(jobs[0],p,o);
    require(drill=="IN;\r\nSP1;\r\nPT0;\r\nPU;\r\nPA400,1600;\r\nPD;\r\nPA400,1600;\r\nPU;\r\nPA1200,1600;\r\nPD;\r\nPA1200,1600;\r\nPU;\r\nPA0,0;\r\n","drill job in plotter units from the bottom left");
    // Coordinates are rounded to whole fiftieths of a millimetre first, then to plotter units: 25.9656 mm from the bottom
    // would be 1038.62 units, but 1298 fiftieths give 1038.4.
    {Document r;r.panels[0].width=100;r.panels[0].height=100;Element hole=newElement(ElementType::Drill);hole.center={70.7275,74.0344};hole.diameter=1;r.panels[0].elements={hole};
     require(hpgl(machiningJobs(r,r.panels[0],MachiningOptions()).value(0),r.panels[0],MachiningOptions()).contains("PA2829,1038;"),"rounding of plotter coordinates");}
    // The milled rectangle runs around its line and comes back to the start.
    const QPolygonF &course=jobs[2].paths[0].points;require(course.size()==5&&course.first()==course.last()&&sameRect(course.boundingRect(),QRectF(40,20,10,10)),"milled course");
    // Options: common origin, layer 2 for drilling and milling only, outer rectangle, holes milled out.
    o.commonOrigin=true;o.layer2=true;o.outline=true;o.tool=2;o.millDrills=true;jobs=machiningJobs(d,p,o);
    require(hpgl(jobs[0],p,o).startsWith("IN;\r\nSP2;\r\nPT0;\r\nPU;\r\nPA0,0;\r\nPD;\r\nPU;\r\n"),"common origin and layer 2");
    const auto engraving=std::find_if(jobs.begin(),jobs.end(),[](const MachiningJob &j){return j.kind==MachiningJob::Engrave;});
    require(engraving!=jobs.end()&&hpgl(*engraving,p,o).contains("SP1;"),"engraving stays on pen 1");
    require(jobs.first().kind==MachiningJob::DrillMill&&near(jobs.first().tool,2,1e-9)&&jobs.first().paths.size()==3,"holes milled out in one job");
    // The milled circle is the original's: 16 B-spline pieces of 21 points in whole fiftieths of a millimetre.
    const QPolygonF hole=jobs.first().paths[0].points;
    require(hole.size()==16*21&&hole.first()==hole.last()&&std::all_of(hole.begin(),hole.end(),[](QPointF q){return near(QLineF(QPointF(10,10),q).length(),0.5,0.03);}),
            "3 mm hole milled with 2 mm on a 0.5 mm radius");
    require(jobs.last().kind==MachiningJob::Outline&&sameRect(jobs.last().paths[0].points.boundingRect(),QRectF(-1,-1,102,52)),"outer rectangle outside by half the tool");
    require(jobs.first().title()=="Bohrungen fräsen mit 2 mm"&&jobs.last().fileName()=="Aussenrechteck mit 2 mm.PLT","job names of the original");
    // As in the original, the parts of a combination are machined each with its own tool.
    Element combination=newElement(ElementType::Group);combination.parameters["combine"]=true;combination.machining=Machining::Mill;combination.pen.width=2;
    Element outer=rectangle({40,20,10,10});outer.machining=Machining::Mill;outer.pen.width=2;
    Element inner=newElement(ElementType::Ellipse);inner.center={45,25};inner.radiusX=inner.radiusY=2;inner.machining=Machining::Mill;inner.pen.width=2;
    Element mark=newElement(ElementType::Ellipse);mark.center={45,25};mark.radiusX=mark.radiusY=0.5;mark.machining=Machining::Engrave;mark.pen.width=0.2;
    combination.children={outer,inner,mark};
    p.elements={combination};jobs=machiningJobs(d,p,MachiningOptions());
    require(jobs.size()==2&&near(jobs[0].tool,2,1e-9)&&jobs[0].paths.size()==2&&jobs[1].kind==MachiningJob::Engrave&&jobs[1].paths.size()==1,"parts of a combination with their own tools");
}

void printTests(){
    Document d;Panel &p=d.panels[0];p.width=100;p.height=50;p.elements={rectangle({10,10,20,10})};
    PrintOptions o;o.cutMarks=false;o.rulers=false;o.data=false;require(printExtent(p,o)==QSizeF(100,50),"bare printout");
    o.tilesX=3;o.gapX=5;require(printExtent(p,o)==QSizeF(310,50),"tiles side by side");
    require(sheetsFor(printExtent(p,o),QPointF(0,0),QSizeF(200,280))==QSize(2,1),"two sheets across");
    // Gaps and the place of the first tile are panel millimetres: at twice the size they double too. The place is that
    // of the panel, also left of the printable area (label sheets), not that of the cut marks.
    o.original=false;o.zoom=2;require(printExtent(p,o)==QSizeF(620,100),"tiles enlarged with their gaps");
    o.centred=false;o.left=-1.24;o.top=8.46;require(near(printOffset(o,printExtent(p,o),QSizeF(200,280)),QPointF(-2.48,16.92),1e-9),"place of the first tile at twice the size");
    o.original=true;o.cutMarks=true;require(near(printOffset(o,printExtent(p,o),QSizeF(200,280)),QPointF(-7.24,2.46),1e-9),"place of the panel, not of the cut marks");
    o.centred=true;o.tilesX=1;o.cutMarks=false;require(near(printOffset(o,printExtent(p,o),QSizeF(200,280)),QPointF(50,115),1e-9),"printout centred on the sheet");
    o=PrintOptions();require(printPanelOrigin(o)==QPointF(14,14),"rulers and cut marks before the panel");
    o.original=false;o.zoom=2;require(printExtent(p,o).width()>200,"enlarged");
    QImage image(400,300,QImage::Format_ARGB32);image.fill(Qt::white);{QPainter painter(&image);painter.scale(2,2);o.mirror=true;paintPrintout(painter,d,p,o,"Test");}
    require(image.pixelColor(2*(14+2*85),2*(14+2*15))==QColor(Qt::red),"mirrored print shows the rectangle on the other side");
    // Print settings belong to the panel and are kept in the native format; the defaults are not written.
    p.print.mirror=true;p.print.tilesX=3;p.print.gapX=3.17;p.print.left=-1.24;p.print.texts=false;p.inch=true;
    const Document back=Document::decode(d.encode());require(back.panels[0].print==p.print&&back.panels[0].inch,"print settings and unit kept");
    p.print=PrintSettings();p.inch=false;require(!d.encode().contains("\"print\"")&&!d.encode().contains("\"inch\""),"default print settings or unit written");
    for(const char *bad:{R"("tiles":[0,1])",R"("tiles":[1.5,1])",R"("gap":[-1,0])",R"("zoom":0)"}){
        QByteArray json=d.encode();json.replace("\"elements\"",QByteArray("\"print\":{")+bad+"},\"elements\"");
        rejects([&]{Document::decode(json);},"invalid print settings accepted");
    }
}

void libraryTests(PanelEditor &editor,const QString &folder){
    PanelSidebar *side=editor.sidebar();side->refreshLibrary();
    require(QFile::exists(QDir(folder).filePath("Grundsymbole.LIB"))&&!side->libraryPages().isEmpty(),"own library page with starter symbols");
    const Document page=frontdesigner::loadFrontDesigner(QDir(folder).filePath("Grundsymbole.LIB"));require(page.panels[0].elements.size()>=8,"starter symbols readable");
    // The library folders as the suite's overview reads and sets them; the editor reads them anew.
    {const LibraryFolders before=symbolLibraryFolders();require(QDir(before.own)==QDir(folder),"the own symbol folder");
     QTemporaryDir extra;require(QFile::copy(QDir(folder).filePath("Grundsymbole.LIB"),QDir(extra.path()).filePath("Fremd.LIB")),"copying a library page");
     setSymbolLibraryFolders({before.own,{extra.path()}});editor.librariesChanged();
     bool listed=false;for(const auto &p:side->libraryPages())listed|=p.path.endsWith("Fremd.LIB")&&!p.writable;
     require(symbolLibraryFolders().extra==QStringList{extra.path()}&&listed,"a further library folder reaches the editor, only read");
     setSymbolLibraryFolders(before);editor.librariesChanged();}
    // Öffnen through a host: no own dialog, the host gets the folder the editor would start in.
    {QString asked;bool called=false;editor.openHandler=[&](const QString &f){called=true;asked=f;return true;};
     respond([](QDialog *d){d->reject();});editor.findChild<QAction*>("open")->trigger();require(called&&!asked.isEmpty(),"Öffnen goes to the host");
     editor.openHandler=nullptr;QApplication::processEvents();}
    // A selected element joins the current page.
    Document d;d.panels[0]=newPanel("Symbole",100,100);Element e=rectangle({10,10,10,10});e.name="Quadrat";d.panels[0].elements={e};editor.setDocument(d);
    Element symbol=e;symbol.hasAnchor=true;symbol.anchor={15,15};require(side->addSymbol(symbol),"add symbol");
    const Document after=frontdesigner::loadFrontDesigner(side->libraryPages().first().path);
    require(after.panels[0].elements.size()==page.panels[0].elements.size()+1&&after.panels[0].elements.last().name=="Quadrat"&&after.panels[0].elements.last().hasAnchor,"symbol with insertion point on the page");
    // Pen presets are kept between sessions.
    side->pens.append(PenPreset{"Test",Pen{Qt::magenta,0.7,PenStyle::Dash},Machining::None});side->savePresets();side->loadPresets();
    require(side->pens.last().name=="Test"&&side->pens.last().pen.color==QColor(Qt::magenta),"pen presets kept");
    // The original's lists of pens, fills and fonts.
    const QByteArray pens="[Stifte]\r\nAnzahl=2\r\n\r\n[Stift0]\r\nBreite=10\r\nPattern=0\r\nName=Gravierstichel 0,2\r\nFarbe=16777215\r\nTool=2\r\n\r\n"
        "[Stift1]\r\nName=Fr\xe4se 1 mm\r\nFarbe=8421504\r\nBreite=50\r\nPattern=2\r\nTool=1\r\n";
    const QByteArray fills="[Fuellung]\r\nAnzahl=2\r\n[Fuellung0]\r\nName=Leer\r\nFarbe=16777215\r\nStyle=1\r\nVerlauf=0\r\nVerlaufFarbe=12632256\r\nVerlaufStyle=0\r\n"
        "[Fuellung1]\r\nName=Kunststoff dunkel\r\nFarbe=0\r\nStyle=0\r\nVerlauf=0\r\nVerlaufFarbe=15066597\r\nVerlaufStyle=3\r\n";
    const QByteArray fonts="[Fonts]\r\nAnzahl=2\r\n[Font0]\r\nName=Arial\r\nHoehe=250\r\nBeschreibung=Mittel\r\nItalic=1\r\nBold=0\r\nFarbe=255\r\nSHX=1\r\nSHXName=DIN1451\r\n"
        "[Font1]\r\nName=Arial\r\nHoehe=300\r\nBeschreibung=Mittel\r\nSHX=0\r\nSHXName=DIN1451\r\n";
    const auto p=frontDesignerPens(pens);const auto f=frontDesignerFills(fills);const auto t=frontDesignerFonts(fonts);
    require(p.size()==2&&p[0].name=="Gravierstichel 0,2"&&p[0].machining==Machining::Engrave&&near(p[0].pen.width,0.2,1e-12)&&p[0].pen.color==QColor(Qt::white)
        &&p[1].name==QString::fromUtf8("Fräse 1 mm")&&p[1].machining==Machining::Mill&&p[1].pen.style==PenStyle::Dash&&p[1].pen.color==QColor(128,128,128),"the original's pens");
    require(f.size()==2&&f[0].fill.style==FillStyle::None&&f[1].fill.style==FillStyle::Solid&&f[1].fill.gradient==Gradient::Diagonal&&f[1].fill.color==QColor(Qt::black)
        &&f[1].fill.color2==QColor(229,229,229),"the original's fills");
    require(t.size()==2&&t[1].strokeFont.isEmpty()&&near(t[1].height,6,1e-12)&&t[0].name=="Mittel"&&t[0].family=="Arial"&&near(t[0].height,5,1e-12)&&t[0].italic&&!t[0].bold&&t[0].strokeFont=="DIN1451","the original's fonts");
    QTemporaryDir dir;QStringList paths;
    for(const auto &[name,bytes]:{std::pair<const char*,QByteArray>{"Stifte.INI",pens},{"FUELLUNG.INI",fills},{"Fonts.ini",fonts},{"Leer.ini","[Anders]\r\nX=1\r\n"}}){
        QFile out(dir.filePath(name));require(out.open(QIODevice::WriteOnly)&&out.write(bytes)==bytes.size(),"writing a settings file");paths<<out.fileName();}
    const int before=side->pens.size();QStringList notes;
    require(side->importFrontDesignerPresets(paths,false,&notes)==6&&side->pens.size()==before+2&&side->pens.last().name==QString::fromUtf8("Fräse 1 mm")&&notes.size()==1,"taking over the original's presets");
    const int fontCount=side->fonts.size();
    require(side->importFrontDesignerPresets(paths,false)==6&&side->pens.size()==before+2&&side->fonts.size()==fontCount,"taking over the same presets again");
    require(side->importFrontDesignerPresets(paths,true)==6&&side->pens.size()==2&&side->fills.size()==2&&side->fonts.size()==2,"replacing the presets");
    side->loadPresets();require(side->fonts.size()==2&&side->fonts[0].name=="Mittel"&&side->fonts[1].name=="Mittel","taken over presets kept");
}
}

// Circuit boards behind the panel: placed, shown, holes from their parts, the comparison, holes moving with the board.
void boardTests(PanelEditor &editor){
    const QString document=newId(),board=newId(),led=newId(),pot=newId(),resistor=newId();
    BoardSource source{document,board,"Platine",QSizeF(60,40),{
        BoardPart{led,"LED1","rot",{10,10},QRectF(7,7,6,6),true},BoardPart{pot,"P1","10k lin",{30,20},QRectF(22,12,16,16),true},
        BoardPart{resistor,"R1","4k7",{45,30},QRectF(40,29,10,2),true},BoardPart{newId(),"D3","",{10,30},QRectF(8.1,28.1,3.8,3.8),true,"LED 3mm"}}};
    editor.boardSources=[&]{return QList<BoardSource>{source};};
    Document fresh;fresh.panels[0].width=120;fresh.panels[0].height=60;editor.setDocument(fresh);
    require(editor.action("boardsBehind")->isEnabled()&&!editor.action("holesFromBoards")->isEnabled(),"boards can be put behind a panel of a project");
    editor.setBoardsBehind({BoardBehind{document,board,{20,10},0,false}},true);
    const auto behind=editor.boardsBehind();
    require(behind.size()==1&&behind[0].source&&behind[0].source->parts.size()==4&&editor.document().panel().boards.size()==1,"a board behind the panel");
    const auto &shown=editor.view()->shownUnderlay();
    require(shown.size()==1&&shown[0].parts.size()==4&&shown[0].parts[0].centre==QPointF(30,20)&&shown[0].parts[0].facing,"the board shows through the panel");
    // Holes: LEDs and potentiometers facing the panel are ticked, the resistor not.
    QStringList ticked;
    respond([&](QDialog *d){auto *table=d->findChild<QTableWidget*>("parts");
        for(int i=0;table&&i<table->rowCount();i++)if(table->item(i,0)->checkState()==Qt::Checked)ticked<<table->item(i,0)->text().section(' ',0,0);d->accept();});
    editor.action("holesFromBoards")->trigger();
    require(ticked==QStringList{"LED1","P1","D3"},"LEDs and potentiometers are offered first, also by their names");
    const auto &elements=editor.document().panel().elements;
    require(elements.size()==3&&elements[0].type==ElementType::Drill&&elements[0].component==led&&elements[0].name=="LED1"&&elements[0].center==QPointF(30,20)&&elements[0].diameter==5
            &&elements[1].component==pot&&elements[1].center==QPointF(50,30)&&elements[1].diameter==7&&elements[2].diameter==3,"holes over the parts, linked to their components");
    require(editor.compareWithBoards().isEmpty(),"holes over their parts pass the comparison");
    if(const QString shot=qEnvironmentVariable("OPENLOCH_FRONTPANEL_BOARDS_SCREENSHOT");!shot.isEmpty()){editor.view()->fitPanel();QApplication::processEvents();editor.grab().save(shot);}
    // The board moves without its holes: they are off; moved onto the parts they pass again.
    editor.setBoardsBehind({BoardBehind{document,board,{25,10},0,false}},false);
    const auto off=editor.compareWithBoards();
    require(off.size()==3&&off[0].part==QPointF(35,20),"holes off their parts after the board moved");
    editor.moveOntoParts();
    require(editor.compareWithBoards().isEmpty()&&editor.document().panel().elements[0].center==QPointF(35,20),"holes moved onto their parts");
    // With the holes taken along: moved, turned and turned over, the holes stay on their parts.
    editor.setBoardsBehind({BoardBehind{document,board,{25,15},0,false}},true);
    require(editor.compareWithBoards().isEmpty()&&editor.document().panel().elements[0].center==QPointF(35,25),"holes taken along with the board");
    editor.setBoardsBehind({BoardBehind{document,board,{90,50},90,true}},true);
    require(editor.compareWithBoards().isEmpty(),"holes stay on their parts when the board turns and turns over");
    require(!editor.view()->shownUnderlay()[0].parts[0].facing,"parts on the component side face away when the solder side faces the panel");
    editor.undo();require(editor.document().panel().boards[0].offset==QPointF(25,15)&&editor.document().panel().elements[0].center==QPointF(35,25),"one undo step for the board and its holes");
    // An element of a component no board behind has.
    Element stray=newElement(ElementType::Drill);stray.component=newId();stray.center={5,5};stray.diameter=3;stray.name="X9";
    editor.change([&](Document &d){d.panel().elements<<stray;});
    const auto findings=editor.compareWithBoards();
    require(findings.size()==1&&!findings[0].part&&findings[0].text.startsWith("X9"),"an element whose part is on no board behind");
    // The dialogs: the board's place from the table; the comparison lists what is off.
    respond([](QDialog *d){auto *table=d->findChild<QTableWidget*>("boards");if(table)qobject_cast<QDoubleSpinBox*>(table->cellWidget(0,1))->setValue(30);d->accept();});
    editor.action("boardsBehind")->trigger();
    require(editor.document().panel().boards.size()==1&&editor.document().panel().boards[0].offset.x()==30&&editor.compareWithBoards().size()==1,"the board placed in the dialog takes its holes along");
    int listed=-1;respond([&](QDialog *d){if(auto *list=d->findChild<QListWidget*>("findings"))listed=list->count();d->reject();});
    editor.action("compareBoards")->trigger();require(listed==1,"the comparison lists its findings");
    editor.action("showBoards")->trigger();require(editor.view()->shownUnderlay().isEmpty(),"the boards can be hidden");
    editor.action("showBoards")->trigger();require(editor.view()->shownUnderlay().size()==1,"and shown again");
    editor.boardSources=nullptr;editor.reloadBoards();
}

void runEditorTests(){
    QTemporaryDir settings,library,files;require(settings.isValid()&&library.isValid()&&files.isValid(),"temporary folders");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    const QString symbols=QDir(library.path()).filePath("Symbole");QSettings().setValue("frontpanel/libraryFolder",symbols);
    {
        PanelEditor editor;editor.resize(1400,900);editor.show();QApplication::processEvents();
        drawingTests(editor);arrangeTests(editor);groupTests(editor);treeTests(editor);dialogTests(editor);clipboardTests(editor);panelTests(editor,files.path());libraryTests(editor,symbols);boardTests(editor);
        // The character order of shape fonts: on like the original, switched in the options.
        QAction *order=editor.action("strokeFontsDosOrder");
        require(order&&order->isCheckable()&&order->isChecked()&&strokeFontsInDosOrder(),"shape fonts in DOS order by default");
        order->trigger();require(!strokeFontsInDosOrder(),"switched to Windows order");order->trigger();require(strokeFontsInDosOrder(),"switched back");
        // A picture of the window for a look at the layout.
        if(const QString shot=qEnvironmentVariable("OPENLOCH_FRONTPANEL_SCREENSHOT");!shot.isEmpty()){
            Document d=frontdesigner::loadFrontDesigner(QDir(symbols).filePath("Grundsymbole.LIB"));d.panels[0].name="Grundsymbole";editor.setDocument(d);
            editor.view()->fitPanel();QApplication::processEvents();editor.grab().save(shot);
        }
        editor.setDocument(Document());
    }
    machiningTests();printTests();
}
