#include "projectparts.h"
#include "documents.h"
#include "language.h"
#include "project.h"
#include "schematictargets.h"
#include "frontpanel.h"
#include "panelboards.h"
#include "modules/pcb/model.h"
#include <QCollator>
#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <algorithm>
namespace openloch::suite {
namespace {
struct Found {QString component,designator,value;};
QList<Found> partsOf(const QString &kind,const QJsonObject &data){
    QList<Found> out;
    if(kind==kindInfo(Kind::Schematic).id){for(const auto &c:schematicTargets(data).components)out<<Found{c.id,c.designator,c.value};}
    else if(kind==kindInfo(Kind::Perfboard).id){for(const auto &c:Project::decode(QJsonDocument(data).toJson(QJsonDocument::Compact)).components())out<<Found{c.id,c.designator,c.value};}
    else if(kind==kindInfo(Kind::Pcb).id){
        for(const auto &b:pcb::fromJson(data).boards)for(const auto &c:pcb::components(b))
            out<<Found{c.id,c.designator>=0?b.elements[c.designator].text:QString(),c.value>=0?b.elements[c.value].text:QString()};
    }else if(kind==kindInfo(Kind::FrontPanel).id){
        const auto panels=frontpanel::Document::fromJson(data).panels;
        for(const auto &panel:panels){
            QHash<QString,QString> names;std::function<void(const QList<frontpanel::Element>&)> walk=[&](const QList<frontpanel::Element> &list){for(const auto &e:list){names.insert(e.id,e.name);walk(e.children);}};
            walk(panel.elements);
            for(const auto &c:frontpanel::panelComponents(panel))out<<Found{c.component,names.value(c.element),{}};
        }
    }
    return out;
}
QString lettersOf(const QString &designator){static const QRegularExpression letters(QStringLiteral("^([A-Za-z]*)"));return letters.match(designator.trimmed()).captured(1).toUpper();}
}
QList<ProjectPart> projectParts(const documents::ProjectFile &project,const QMap<QString,QJsonObject> &current){
    QList<ProjectPart> parts;QHash<QString,int> index;
    // The schematic names its parts first; the boards and panels add theirs.
    QList<int> order;for(int i=0;i<project.documents.size();i++)if(project.documents[i].kind==kindInfo(Kind::Schematic).id)order<<i;
    for(int i=0;i<project.documents.size();i++)if(!order.contains(i))order<<i;
    for(int i:order){
        const auto &d=project.documents[i];
        QList<Found> found;try{found=partsOf(d.kind,current.value(d.id,d.data));}catch(const std::exception &){continue;}
        for(const auto &f:found){
            if(f.component.isEmpty())continue;
            auto it=index.find(f.component);
            if(it==index.end()){it=index.insert(f.component,int(parts.size()));parts<<ProjectPart{f.component,f.designator.trimmed(),f.value.trimmed(),{}};}
            ProjectPart &p=parts[*it];if(!p.documents.contains(d.id))p.documents<<d.id;
            if(p.designator.isEmpty())p.designator=f.designator.trimmed();
            if(p.value.isEmpty())p.value=f.value.trimmed();
        }
    }
    for(auto &p:parts)std::sort(p.documents.begin(),p.documents.end(),[&](const QString &a,const QString &b){return project.indexOf(a)<project.indexOf(b);});
    QCollator natural;natural.setNumericMode(true);natural.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(parts.begin(),parts.end(),[&](const ProjectPart &a,const ProjectPart &b){return natural.compare(a.designator,b.designator)<0;});
    return parts;
}
QList<PartsRow> partsList(const QList<ProjectPart> &parts,const documents::ProjectFile &project){
    QList<PartsRow> rows;QMap<QPair<QString,QString>,int> index;   // (letters, value) → row
    for(const auto &p:parts){
        bool buy=false;for(const auto &d:p.documents){const int i=project.indexOf(d);if(i>=0&&project.documents[i].kind!=kindInfo(Kind::FrontPanel).id)buy=true;}
        if(!buy)continue;
        const auto key=qMakePair(lettersOf(p.designator),p.value);
        auto it=index.find(key);if(it==index.end()){it=index.insert(key,int(rows.size()));rows<<PartsRow{0,{},p.value};}
        rows[*it].count++;rows[*it].designators<<(p.designator.isEmpty()?ui("ohne Kennung"):p.designator);
    }
    return rows;
}
QByteArray partsListCsv(const QList<PartsRow> &rows){
    auto cell=[](QString text){if(text.contains(';')||text.contains('"')||text.contains('\n'))text='"'+text.replace('"',"\"\"")+'"';return text;};
    QString out=cell(ui("Menge"))+';'+cell(ui("Bezeichner"))+';'+cell(ui("Wert"))+"\r\n";
    for(const auto &r:rows)out+=QString::number(r.count)+';'+cell(r.designators.join(", "))+';'+cell(r.value)+"\r\n";
    return "\xEF\xBB\xBF"+out.toUtf8();
}
}
