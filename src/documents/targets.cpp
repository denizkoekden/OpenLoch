#include "targets.h"
#include "language.h"
#include "legacy_reader.h"
#include <QJsonArray>
#include <QSet>
namespace openloch::documents {
const TargetComponent *Targets::component(const QString &id) const{
    for(const auto &c:components)if(c.id==id)return &c;
    return nullptr;
}
QJsonObject Targets::toJson() const{
    QJsonArray parts,list;
    for(const auto &c:components){QJsonObject o{{"id",c.id},{"designator",c.designator},{"value",c.value},{"pins",QJsonArray::fromStringList(c.pins)}};if(!c.kind.isEmpty())o["kind"]=c.kind;parts.append(o);}
    for(const auto &n:nets){QJsonArray pins;for(const auto &p:n.pins)pins.append(QJsonArray{p.component,p.pin});list.append(QJsonObject{{"name",n.name},{"pins",pins}});}
    return {{"components",parts},{"nets",list}};
}
Targets Targets::fromJson(const QJsonObject &json){
    auto invalid=[]{return FormatError(ui("Ungültige Soll-Verbindungen"));};
    if(!json["components"].isArray()||!json["nets"].isArray())throw invalid();
    Targets t;QSet<QString> ids;
    for(const auto &value:json["components"].toArray()){
        const auto o=value.toObject();if(!value.isObject()||!o["id"].isString()||o["id"].toString().isEmpty()||ids.contains(o["id"].toString())||!o["pins"].isArray())throw invalid();
        TargetComponent c{o["id"].toString(),o["designator"].toString(),o["value"].toString(),{},o["kind"].toString()};
        for(const auto &pin:o["pins"].toArray()){if(!pin.isString()||c.pins.contains(pin.toString()))throw invalid();c.pins<<pin.toString();}
        ids.insert(c.id);t.components.append(c);
    }
    for(const auto &value:json["nets"].toArray()){
        const auto o=value.toObject();if(!value.isObject()||!o["pins"].isArray())throw invalid();
        TargetNet n{o["name"].toString(),{}};
        for(const auto &pin:o["pins"].toArray()){
            const auto a=pin.toArray();const auto *c=t.component(a.at(0).toString());
            if(a.size()!=2||!c||!c->pins.contains(a.at(1).toString()))throw invalid();
            n.pins.append({a.at(0).toString(),a.at(1).toString()});
        }
        t.nets.append(n);
    }
    return t;
}
}
