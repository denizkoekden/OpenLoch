// A small program around the schematic editor. Until the module joins the main program it serves for trying the
// editor and converting files from the command line:
//   openloch_schematic_demo [file] [--language de|en|fr] [--sheet n] [--render picture.png] [--dpi n] [--save file.olsch]
//                           [--window picture.png] [--export file.spl8|file.spl7]
// Without a file it shows OpenLoch's example.
#include "editor.h"
#include "example.h"
#include "render.h"
#include "language.h"
#include <QApplication>
#include <QCommandLineParser>
#include <cstdio>

using namespace openloch;
using namespace openloch::schematic;

int main(int argc,char **argv){
    QApplication app(argc,argv);QApplication::setApplicationName("OpenLoch");QApplication::setOrganizationName("OpenLoch");
    QCommandLineParser parser;parser.addHelpOption();parser.addPositionalArgument("file","schematic (.olsch)");
    QCommandLineOption language("language","interface language: de, en or fr","language","de"),sheet("sheet","show this sheet (counted from 1)","number"),
        render("render","draw the sheet into a picture and quit","png"),dpi("dpi","resolution of the picture (300)","dpi","300"),
        saveOwn("save","write the document in OpenLoch's format and quit","olsch"),grab("window","draw the whole window into a picture and quit","png"),
        exportSplan("export","write the document as sPlan 8 (.spl8) or 7 (.spl7) and quit","file");
    parser.addOptions({language,sheet,render,dpi,saveOwn,grab,exportSplan});parser.process(app);
    setUiLanguage(parser.value(language));
    Editor window;window.loadPreferences();
    if(!parser.positionalArguments().isEmpty()){
        QString error;if(!window.openFile(parser.positionalArguments().first(),&error)){fprintf(stderr,"%s\n",qPrintable(error));return 1;}
    }else window.setDocument(exampleDocument());
    if(parser.isSet(sheet))window.switchSheet(parser.value(sheet).toInt()-1);
    if(parser.isSet(grab)){window.resize(1400,900);window.show();QApplication::processEvents();window.view()->fitSheet();QApplication::processEvents();
        return window.grab().save(parser.value(grab))?0:1;}
    if(parser.isSet(render)||parser.isSet(saveOwn)||parser.isSet(exportSplan)){
        QString error;bool ok=true;
        if(parser.isSet(exportSplan)){const auto file=parser.value(exportSplan);ok&=window.exportSplan(file,file.endsWith(".spl7",Qt::CaseInsensitive)?70:80,&error);}
        if(parser.isSet(saveOwn))ok&=window.saveFile(parser.value(saveOwn),&error);
        if(ok&&parser.isSet(render)){
            ok&=renderSheet(window.document(),window.document().activeSheet,parser.value(dpi).toDouble()/25.4).save(parser.value(render));
            if(!ok)error="cannot write the picture";
        }
        if(!ok){fprintf(stderr,"%s\n",qPrintable(error));return 1;}
        return 0;
    }
    window.show();return app.exec();
}
