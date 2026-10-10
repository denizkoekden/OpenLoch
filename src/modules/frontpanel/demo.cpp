// A small program around the front panel editor until the module joins the main program. It also serves for converting
// and rendering files from the command line:
//   openloch_frontpanel_demo [file] [--language en] [--render picture.png] [--dpi 300] [--panel 2] [--save file.olfp|.fpl|.lib]
#include "paneleditor.h"
#include "frontpanel.h"
#include "fpl.h"
#include "panelrender.h"
#include "language.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QLocale>
#include <QSaveFile>
#include <QSettings>
#include <QTextStream>

using namespace openloch;
using namespace openloch::frontpanel;
using namespace openloch::frontdesigner;

int main(int argc,char **argv){
    QApplication app(argc,argv);app.setApplicationName("OpenLoch");app.setOrganizationName("OpenLoch");
    const auto saved=QSettings().value("ui/language").toString();
    setUiLanguage(saved.isEmpty()?(QLocale::system().language()==QLocale::German?"de":"en"):saved);
    QCommandLineParser parser;parser.setApplicationDescription(ui("OpenLoch Frontplatte"));parser.addHelpOption();
    parser.addOption({"render",ui("Frontplatte ohne Fenster als PNG exportieren"),"path"});
    parser.addOption({"dpi",ui("Auflösung des Bildexports"),"dpi","300"});
    parser.addOption({"panel",ui("Nummer der Frontplatte (ab 1)"),"number","0"});
    parser.addOption({"save",ui("Dokument ohne Fenster speichern (.olfp, .fpl oder .lib)"),"path"});
    parser.addOption({"language",ui("Sprache der Oberfläche: de oder en"),"code"});
    parser.addPositionalArgument("file",ui("Zu öffnende Datei"),"[file]");parser.process(app);
    if(parser.isSet("language"))setUiLanguage(parser.value("language"));
    const QString file=parser.positionalArguments().value(0);
    try{
        if(parser.isSet("render")||parser.isSet("save")){
            Document document;if(!file.isEmpty())document=isFrontDesignerFile(file)?loadFrontDesigner(file):Document::load(file);
            int index=parser.value("panel").toInt()-1;if(index<0)index=document.activePanel;
            if(index>=document.panels.size())throw std::runtime_error(ui("Diese Frontplatte gibt es nicht").toStdString());
            if(parser.isSet("save")){
                const QString target=parser.value("save");const QString suffix=target.section('.',-1).toLower();
                if(suffix=="fpl"||suffix=="lib"){
                    const QByteArray bytes=suffix=="lib"?writeFrontDesignerLibrary(document,index):writeFrontDesigner(document);
                    QSaveFile f(target);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())throw std::runtime_error(f.errorString().toStdString());
                }else document.save(target);
            }
            if(parser.isSet("render")){
                const QImage image=renderPanel(document,document.panels[index],parser.value("dpi").toDouble());
                if(!image.save(parser.value("render")))throw std::runtime_error(ui("Bildexport fehlgeschlagen").toStdString());
            }
            return 0;
        }
        PanelEditor editor;editor.resize(1400,900);
        if(!file.isEmpty()&&!editor.open(file))return 1;
        editor.show();return app.exec();
    }catch(const std::exception &e){QTextStream(stderr)<<e.what()<<"\n";return 1;}
}
