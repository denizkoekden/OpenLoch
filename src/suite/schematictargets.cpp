#include "schematictargets.h"
#include "modules/schematic/model.h"
#include "modules/schematic/nets.h"
#include "modules/schematic/numbering.h"
#include <QHash>
#include <QSet>
namespace openloch::suite {
documents::Targets schematicTargets(const QJsonObject &data){
    const auto document=schematic::fromJson(data);
    // A child (a gate of an IC, a contact of a relay) belongs to its parent's part: its contacts count as the parent's
    // pins. A child whose parent is missing stands for itself.
    QHash<QString,QString> partOf;   // component id → id of the part it belongs to
    QHash<QString,QList<schematic::PlacedComponent>> members;QStringList order;
    for(const auto &placed:schematic::allComponents(document)){
        const auto &component=*placed.item;
        const bool child=!component.parentId.isEmpty()&&schematic::componentWithId(document,component.parentId).item;
        const QString part=child?component.parentId:component.id;
        partOf.insert(component.id,part);
        if(!members.contains(part))order<<part;
        if(child)members[part].append(placed);else members[part].prepend(placed);
    }
    documents::Targets targets;QHash<QString,QString> pinOf;QSet<QString> parts;   // contact id → pin; parts with designator
    for(const auto &part:order){
        const auto &list=members[part];
        if(list.first().item->id!=part)continue;   // only children whose parent is not a component (cannot happen)
        const auto &component=*list.first().item;
        schematic::TextContext context;context.document=&document;context.sheet=list.first().sheet;context.component=&component;
        QList<const schematic::Item*> contacts;for(const auto &member:list)contacts<<schematic::contacts(*member.item);
        QStringList names;for(const auto *contact:contacts)names<<contact->name.trimmed();
        const bool named=!names.contains(QString())&&QSet<QString>(names.begin(),names.end()).size()==names.size();
        documents::TargetComponent target{component.id,schematic::shownDesignator(component,context).trimmed(),component.value,{},schematic::partKind(component)};
        for(int k=0;k<contacts.size();k++){
            if(!contacts[k]->hasPin)continue;
            const QString pin=named?names[k]:QString::number(k+1);target.pins<<pin;pinOf.insert(contacts[k]->id,pin);
        }
        if(target.designator.isEmpty())continue;
        parts.insert(component.id);targets.components.append(target);
    }
    for(const auto &net:schematic::deriveNets(document)){
        documents::TargetNet target{net.name,{}};
        for(const auto &pin:net.pins){
            const QString part=partOf.value(pin.component,pin.component);
            if(parts.contains(part)&&pinOf.contains(pin.contact))target.pins.append({part,pinOf[pin.contact]});
        }
        if(!target.pins.isEmpty())targets.nets.append(target);
    }
    return targets;
}
}
