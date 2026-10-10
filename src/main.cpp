#include "language.h"
#include "suite/suite.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileOpenEvent>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QTextStream>
namespace {
// Requests of the system reach the suite: files to open (macOS: Finder, Dock, `open -a`) and the end of the program
// (macOS: Quit in the program menu), so that closing the last document then does not bring back the start screen.
class Application : public QApplication {
public:
    using QApplication::QApplication;
    openloch::suite::Suite *suite=nullptr;
protected:
    bool event(QEvent *e) override{
        if(e->type()==QEvent::FileOpen){const QString file=static_cast<QFileOpenEvent*>(e)->file();if(suite)suite->open(file);return true;}
        if(e->type()==QEvent::Quit&&suite){suite->setQuitting(true);const bool done=QApplication::event(e);if(!e->isAccepted())suite->setQuitting(false);return done;}
        return QApplication::event(e);
    }
};
}
int main(int argc,char **argv) {
    Application app(argc,argv);app.setApplicationName("OpenLoch");app.setOrganizationName("OpenLoch");app.setApplicationVersion("0.1.0");
    app.setWindowIcon(QIcon(":/openloch.png"));
    // The interface language: saved choice, else German on a German system and English otherwise; --language overrides.
    const auto saved=QSettings().value("ui/language").toString();
    openloch::setUiLanguage(saved.isEmpty()?(QLocale::system().language()==QLocale::German?"de":QLocale::system().language()==QLocale::French?"fr":"en"):saved);
    QCommandLineParser parser;parser.setApplicationDescription(openloch::ui("OpenLoch – Lochraster, Leiterplatte und Frontplatte"));parser.addHelpOption();parser.addVersionOption();
    parser.addOption({"assets",openloch::ui("Optionaler Ordner der vorhandenen LochMaster-Installation"),"path"});
    parser.addOption({"render",openloch::ui("Projekt ohne Fenster als PNG exportieren"),"path"});
    parser.addOption({"language",openloch::ui("Sprache der Oberfläche: de, en oder fr"),"code"});
    parser.addPositionalArgument("files",openloch::ui("Zu öffnende Dokumente"),"[file...]");parser.process(app);
    if(parser.isSet("language"))openloch::setUiLanguage(parser.value("language"));
    openloch::installStandardTexts();
    const QStringList files=parser.positionalArguments();
    if(parser.isSet("render")) {
        QString error;
        if(files.size()!=1)error=openloch::ui("Für den Export ist genau eine Projektdatei erforderlich");
        else if(openloch::suite::renderDocument(files.first(),parser.value("render"),&error))return 0;
        QTextStream(stderr)<<error<<"\n";return 1;
    }
    QString assets=parser.value("assets");if(assets.isEmpty())assets=qEnvironmentVariable("OPENLOCH_ASSET_ROOT");
    // Without one, Lochraster windows use the saved choice or search the usual places (Bibliothek → LochMaster-Bibliotheken einbinden).
    openloch::suite::Suite suite(assets);app.suite=&suite;
    // Files on the command line open straight in their editors; without any, the start screen offers the kinds of document.
    if(!files.isEmpty()){
        int opened=0;for(const auto &file:files)opened+=suite.open(file)!=nullptr;
        if(!opened)return 1;
    }else{suite.showStartScreen();suite.offerRecovery();}
    return app.exec();
}
