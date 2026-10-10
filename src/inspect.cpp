#include "legacy_reader.h"
#include "project.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
#include <QFileInfo>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QTextStream out(stdout),err(stderr);
    if(argc<2){err<<"Usage: openloch_inspect FILE [--json] [--export-lm4 OUTPUT.LM4]\n";return 2;}
    try {
        auto input=app.arguments().at(1);auto p=openloch::Project::load(input);
        auto exportIndex=app.arguments().indexOf("--export-lm4");
        if(exportIndex>=0){if(exportIndex+1>=app.arguments().size())throw openloch::FormatError("Ausgabedatei fehlt");auto output=app.arguments()[exportIndex+1];if(QFileInfo(output).suffix().toLower()!="lm4")throw openloch::FormatError("Ausgabedatei benötigt die Endung .LM4");if(QFileInfo(output).absoluteFilePath()==QFileInfo(input).absoluteFilePath())throw openloch::FormatError("Export benötigt einen anderen Dateinamen");p.save(output);}
        if(app.arguments().contains("--json"))out<<QJsonDocument(p.legacy).toJson();
        else out<<QJsonDocument(QJsonObject{{"file",QFileInfo(input).fileName()},{"title",p.title},{"objects",p.legacy["object_count"]},{"width_mm",p.width/100},{"height_mm",p.height/100},{"tail_size",p.legacy["tail_size"]}}).toJson(QJsonDocument::Compact)<<"\n";
        return 0;
    } catch(const std::exception &e){err<<e.what()<<"\n";return 1;}
}
