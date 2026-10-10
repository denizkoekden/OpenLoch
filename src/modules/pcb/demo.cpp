// A small program around the PCB editor: a window with its menus and toolbar. Until the module joins the main program
// it serves for trying the editor and converting files from the command line:
//   openloch_pcb_demo [file] [--language en|fr] [--render picture.png] [--export file.lay6|file.lay] [--save file.olpcb]
//                     [--fabrication folder]
//                     [--board n] [--photo] [--below]
// Without a file it shows OpenLoch's example board.
#include "editor.h"
#include "fabrication.h"
#include "example.h"
#include "language.h"
#include "formats/sprint/sprint.h"
#include <QApplication>
#include <QCloseEvent>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QMainWindow>
#include <QMenuBar>
#include <QSaveFile>
#include <QToolBar>
#include <cstdio>

using namespace openloch;
using namespace openloch::pcb;
namespace {
class Window : public QMainWindow {
public:
    Window(){
        editor=new Editor;setCentralWidget(editor);editor->loadPreferences();
        // The menus of the module's table; "&Datei" ends with Beenden.
        for(const auto &[title,names]:editorMenus()){
            auto *m=menuBar()->addMenu(ui(title));for(const auto &n:names){if(n.isEmpty())m->addSeparator();else m->addAction(editor->action(n));}
            if(title==QStringLiteral("&Datei")){m->addSeparator();m->addAction(ui("&Beenden"),QKeySequence::Quit,this,&QWidget::close);}
        }
        auto *bar=addToolBar(ui("Werkzeuge"));bar->setIconSize(QSize(16,16));
        for(auto *a:editor->toolBarActions()){if(a)bar->addAction(a);else bar->addSeparator();}
        editor->titleChanged=[this]{setWindowTitle(QString("%1%2 – OpenLoch").arg(editor->displayName(),editor->isModified()?" *":""));};
        editor->titleChanged();resize(1400,900);
    }
    Editor *editor;
protected:
    void closeEvent(QCloseEvent *event) override{if(editor->maybeSave())event->accept();else event->ignore();}
};
}

int main(int argc,char **argv){
    QApplication app(argc,argv);QApplication::setApplicationName("OpenLoch");
    QCommandLineParser parser;parser.addHelpOption();parser.addPositionalArgument("file","circuit board (.olpcb, .lay6, .lay)");
    QCommandLineOption language("language","interface language: de, en or fr","language","de"),render("render","draw the board into a PNG file and quit","png"),
        exportLay("export","write the document as Sprint-Layout 6 (.lay6) or 4.0 (.lay) and quit","file"),saveOwn("save","write the document in OpenLoch's format and quit","olpcb"),
        boardOption("board","show this board (counted from 1)","number"),photo("photo","photo view"),below("below","view from below"),
        fabrication("fabrication","write the Gerber files of the layers in use and the Excellon drill file into an existing folder and quit","folder");
    parser.addOptions({language,render,exportLay,saveOwn,boardOption,photo,below,fabrication});parser.process(app);
    setUiLanguage(parser.value(language));
    Window window;
    if(!parser.positionalArguments().isEmpty()){
        QString error;if(!window.editor->openFile(parser.positionalArguments().first(),&error)){fprintf(stderr,"%s\n",qPrintable(error));return 1;}
    }else window.editor->setDocument(exampleDocument());
    if(parser.isSet(boardOption))window.editor->switchBoard(parser.value(boardOption).toInt()-1);
    if(parser.isSet(photo))window.editor->action("photo")->trigger();
    if(parser.isSet(below))window.editor->action("fromBelow")->trigger();
    if(parser.isSet(render)||parser.isSet(exportLay)||parser.isSet(saveOwn)||parser.isSet(fabrication)){
        QString error;bool ok=true;
        if(parser.isSet(exportLay)){const auto file=parser.value(exportLay);ok&=window.editor->exportSprint(file,&error,file.endsWith(".lay",Qt::CaseInsensitive)?4:6);}
        if(ok&&parser.isSet(saveOwn))ok&=window.editor->saveFile(parser.value(saveOwn),&error);
        if(ok&&parser.isSet(fabrication)){
            const auto written=writeFabricationFiles(window.editor->document().board(),parser.value(fabrication),QFileInfo(window.editor->displayName()).completeBaseName(),{},{},&error);
            ok=!written.isEmpty();for(const auto &f:written)printf("%s\n",qPrintable(f));
        }
        if(ok&&parser.isSet(render)){const auto &b=window.editor->document().board();const double ratio=b.height/b.width;
            ok&=window.editor->view()->render(QSize(1600,std::max(200,int(1600*ratio)))).save(parser.value(render));if(!ok)error="cannot write the picture";}
        if(!ok){fprintf(stderr,"%s\n",qPrintable(error));return 1;}
        return 0;
    }
    window.show();return app.exec();
}
