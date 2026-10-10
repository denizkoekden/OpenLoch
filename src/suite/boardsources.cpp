#include "boardsources.h"
#include "geometry.h"
#include "project.h"
#include "targetcheck.h"
#include "modules/pcb/model.h"
#include <QHash>
#include <QJsonDocument>
namespace openloch::suite {
namespace {
QString boardName(const QString &document,const QString &board,qsizetype count){return count>1&&!board.isEmpty()?QString("%1 – %2").arg(document,board):document;}
}
QList<frontpanel::BoardSource> perfboardSources(const QString &document,const QString &name,QJsonObject &data){
    const Project p=Project::decode(QJsonDocument(data).toJson(QJsonDocument::Compact));
    if(const auto stored=QJsonDocument::fromJson(p.encode()).object();stored!=data)data=stored;
    QList<Project> boards;
    if(p.boards.isEmpty())boards<<p;
    else for(int i=0;i<p.boards.size();i++)boards<<(i==p.activeBoard?p:Project::decode(QJsonDocument(p.boards[i].toObject()).toJson(QJsonDocument::Compact)));
    QList<frontpanel::BoardSource> out;
    for(const auto &b:boards){
        frontpanel::BoardSource source{document,b.boardId,boardName(name,b.title,boards.size()),QSizeF(b.width/100,b.height/100),{}};
        QHash<QString,Project::Placed> placed;for(const auto &o:b.placedObjects())placed.insert(o.kind+'/'+QString::number(o.index),o);
        for(const auto &c:b.components()){
            const auto o=placed.value(c.kind+'/'+QString::number(c.index));
            QRectF area=o.transform.mapRect(drawingBounds(o.node));
            if(area.isEmpty()){
                // Own parts such as resistors draw no outline here: the middle of their pins.
                const auto pins=pinPositions(b,c);QPointF middle;for(auto q:pins)middle+=q/double(pins.size());area=QRectF(middle,QSizeF(0,0));
            }
            const QString label=o.node["label"].toString().trimmed();
            source.parts<<frontpanel::BoardPart{c.id,c.designator,c.value,area.center()/100,QRectF(area.topLeft()/100,area.size()/100),!solderSide(o.node).value_or(false),
                                                label.isEmpty()?o.node["description"].toString().trimmed():label};
        }
        out<<source;
    }
    return out;
}
QList<frontpanel::BoardSource> circuitBoardSources(const QString &document,const QString &name,QJsonObject &data){
    const auto d=pcb::fromJson(data);
    if(const auto stored=pcb::toJson(d);stored!=data)data=stored;
    QList<frontpanel::BoardSource> out;
    for(const auto &b:d.boards){
        frontpanel::BoardSource source{document,b.id,boardName(name,b.name,d.boards.size()),QSizeF(b.width,b.height),{}};
        for(const auto &c:pcb::components(b)){
            QRectF area;for(int m:c.members)if(b.elements[m].type!=pcb::ElementType::Text)area|=pcb::bounds(b.elements[m]);
            const QString designator=c.designator>=0?b.elements[c.designator].text:QString(),value=c.value>=0?b.elements[c.value].text:QString();
            source.parts<<frontpanel::BoardPart{c.id,designator,value,pcb::pickPlaceCentre(b,c),area,pcb::componentOnTop(b,c),c.designator>=0?b.elements[c.designator].package:QString()};
        }
        out<<source;
    }
    return out;
}
}
