#include "pcbwindow.h"
#include "language.h"
#include "modules/pcb/editor.h"
#include <QAction>
#include <QCloseEvent>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
namespace openloch::suite {
PcbWindow::PcbWindow(QWidget *parent):QMainWindow(parent){
    setObjectName("pcbWindow");pcb=new pcb::Editor(this);setCentralWidget(pcb);pcb->loadPreferences();
    // The module's menu table (the same as in its own program); names the editor does not have are left out, Beenden ends Datei.
    for(const auto &[title,names]:pcb::editorMenus()){
        auto *menu=menuBar()->addMenu(ui(title));bool separate=false;
        // The comparison with the schematic needs the project's schematic, which can come after this window opened (the
        // suite gives the editor the project's targets): a menu asks the editor each time it opens.
        connect(menu,&QMenu::aboutToShow,this,[this]{
            const bool on=pcb->schematicAvailable();
            for(const char *name:{"compareSchematic","schematicAirwires","takeOverSchematic","assignPins","placeMissing"})if(auto *a=pcb->action(name))a->setEnabled(on);
        });
        for(const auto &name:names){
            if(name.isEmpty()){separate=!menu->isEmpty();continue;}
            if(auto *a=pcb->action(name)){if(separate)menu->addSeparator();separate=false;menu->addAction(a);}
        }
        if(title!=QStringLiteral("&Datei"))continue;
        auto *quit=new QAction(ui("&Beenden"),this);quit->setObjectName("quit");quit->setShortcut(QKeySequence::Quit);quit->setMenuRole(QAction::QuitRole);
        connect(quit,&QAction::triggered,this,&QWidget::close);menu->addSeparator();menu->addAction(quit);
    }
    auto *bar=addToolBar(ui("Werkzeuge"));bar->setObjectName("pcbToolBar");bar->setIconSize(QSize(16,16));
    for(auto *a:pcb->toolBarActions()){if(a)bar->addAction(a);else bar->addSeparator();}
    pcb->titleChanged=[this]{setWindowTitle(QString("%1[*] – %2").arg(pcb->displayName(),ui("Leiterplatte")));setWindowModified(pcb->isModified());if(titleChanged)titleChanged();};
    pcb->titleChanged();resize(1400,900);
}
QString PcbWindow::documentPath() const{
    if(!pcb->filePath().isEmpty())return pcb->filePath();
    return !source.isEmpty()&&pcb->displayName()==QFileInfo(source).fileName()?source:QString();
}
void PcbWindow::closeEvent(QCloseEvent *event){if(pcb->maybeSave())event->accept();else event->ignore();}
}
