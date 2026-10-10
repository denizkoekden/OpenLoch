#include "targetcheck.h"
#include "continuity.h"
#include "geometry.h"
#include "openlibrary.h"
#include "legacy_reader.h"
#include <cmath>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QRegularExpression>
#include <limits>
#include <utility>
namespace openloch {
namespace {
QString keyOf(const QString &kind,int index){return kind+'/'+QString::number(index);}
// The pins of parts of the board shown, by the parts' identifiers, where the canvas and the continuity tester place them.
QHash<QString,QList<QPointF>> pinsOnBoard(const Project &board,const QList<Project::Component> &parts){
    QHash<QString,QTransform> placed;for(const auto &p:board.placedObjects())placed.insert(keyOf(p.kind,p.index),p.transform);
    QHash<QString,QList<QPointF>> result;
    for(const auto &c:parts){
        QJsonObject node;
        if(c.kind=="legacy")node=board.legacyNode(c.index,false);
        else{const auto stored=board.additions[c.index].toObject();auto plain=stored;plain.remove("reference");node=stored["type"]=="component"?board.componentNode(plain):stored;}
        const auto t=placed.value(keyOf(c.kind,c.index));QList<QPointF> at;for(const auto &p:connectionPoints(node))at<<t.map(p);
        result.insert(c.uid,at);
    }
    return result;
}
}
QList<QPointF> pinPositions(const Project &board,const Project::Component &part){return pinsOnBoard(board,{part}).value(part.uid);}
QList<int> assignedPins(const QStringList &part,const QStringList &component){
    QList<int> pins;for(const auto &name:part){int found=-1;for(int i=0;i<component.size();i++)if(component[i].compare(name,Qt::CaseInsensitive)==0)found=i;pins<<found;}
    // Two numbered contacts of the schematic against named pins: 1 is the anode or plus, 2 the cathode or minus.
    const int one=int(component.indexOf("1")),two=int(component.indexOf("2"));
    if(component.size()==2&&one>=0&&two>=0&&part.size()==2&&pins.contains(-1)){
        const QStringList upper{part[0].toUpper(),part[1].toUpper()};const std::pair<const char*,const char*> polarities[]={{"A","K"},{"+","-"}};
        for(const auto &[first,second]:polarities){const int a=int(upper.indexOf(first)),k=int(upper.indexOf(second));if(a>=0&&k>=0){pins={-1,-1};pins[a]=one;pins[k]=two;}}
    }
    return pins;
}
QString kindOf(const documents::TargetComponent &component){
    static const QRegularExpression letters(QStringLiteral("^([A-Za-z]+)"));
    return (component.kind.isEmpty()?letters.match(component.designator.trimmed()).captured(1):component.kind).toUpper();
}
bool interchangeable(const Project::Component &part,const documents::TargetComponent &component){
    return QStringList{"R","C","L"}.contains(kindOf(component))&&part.pins==QStringList{"1","2"}&&QSet<QString>(component.pins.begin(),component.pins.end())==QSet<QString>{"1","2"};
}
documents::Targets targetsForBoard(const Project &board,const documents::Targets &targets){
    QSet<QString> here,elsewhere;for(const auto &c:board.components())(c.board==board.activeBoard?here:elsewhere).insert(c.id);
    elsewhere.subtract(here);
    documents::Targets out;for(const auto &c:targets.components)if(!elsewhere.contains(c.id))out.components<<c;
    if(out.components.size()==targets.components.size())return targets;
    for(const auto &n:targets.nets){documents::TargetNet kept{n.name,{}};for(const auto &p:n.pins)if(!elsewhere.contains(p.component))kept.pins<<p;out.nets<<kept;}
    return out;
}

TargetCheck checkTargets(const Project &board,const documents::Targets &targets){
    TargetCheck check;
    QList<Project::Component> parts;for(const auto &c:board.components())if(c.board==board.activeBoard)parts<<c;
    const auto positions=pinsOnBoard(board,parts);
    // Which part stands for which component: the one naming it, else the one with its Kennung, compared regardless of
    // case as LochMaster numbers parts. A Kennung that more than one part has is not taken.
    QMap<int,int> partOf;QSet<int> used;
    for(int t=0;t<targets.components.size();t++)for(int k=0;k<parts.size();k++)
        if(!used.contains(k)&&parts[k].id==targets.components[t].id){partOf[t]=k;used.insert(k);break;}
    QHash<QString,QList<int>> byDesignator;for(int k=0;k<parts.size();k++)if(!parts[k].designator.isEmpty())byDesignator[parts[k].designator.toUpper()]<<k;
    for(int t=0;t<targets.components.size();t++){
        if(partOf.contains(t))continue;
        const auto designator=targets.components[t].designator;const auto candidates=byDesignator.value(designator.toUpper());
        if(candidates.size()>1){if(!check.ambiguous.contains(designator))check.ambiguous<<designator;check.missing<<t;continue;}
        if(candidates.size()==1&&!used.contains(candidates[0])){partOf[t]=candidates[0];used.insert(candidates[0]);}else check.missing<<t;
    }
    for(int k=0;k<parts.size();k++)if(!used.contains(k))check.extra<<parts[k];
    for(auto it=partOf.begin();it!=partOf.end();++it){
        const auto &target=targets.components[it.key()];const auto &part=parts[it.value()];
        TargetCheck::Part p;p.target=it.key();p.part=part;p.at=positions.value(part.uid);p.byDesignator=part.id!=target.id;
        p.pins=assignedPins(part.pins,target.pins);QSet<int> seen;for(int pin:p.pins)if(pin>=0)seen.insert(pin);
        if(seen.size()<target.pins.size()||p.at.size()!=p.pins.size())check.unassigned<<p;else check.parts<<p;
    }
    // The piece of copper each pin lies on: the first of its conductors; a pin off copper is a piece of its own.
    auto model=continuityModel(board);QHash<QPair<double,double>,int> cache;int lone=-1;
    auto piece=[&](QPointF at){
        const auto key=qMakePair(at.x(),at.y());if(cache.contains(key))return cache[key];
        const auto net=model.netAt(at);const int id=net.isEmpty()?lone--:net.first();cache.insert(key,id);return id;
    };
    QHash<QPair<QString,QString>,int> netOf;
    for(int n=0;n<targets.nets.size();n++)for(const auto &pin:targets.nets[n].pins)netOf.insert({pin.component,pin.pin},n);
    struct Placed {int part,piece;QPointF at;};
    auto placedPins=[&]{
        QHash<QPair<QString,QString>,Placed> result;
        for(int k=0;k<check.parts.size();k++){const auto &p=check.parts[k];const auto &target=targets.components[p.target];
            for(int i=0;i<p.pins.size();i++)if(p.pins[i]>=0)result.insert({target.id,target.pins[p.pins[i]]},{k,piece(p.at[i]),p.at[i]});}
        return result;
    };
    // The two leads of a resistor, a capacitor or a coil numbered 1 and 2 on both sides are interchangeable: they are
    // taken the other way round when that joins more of them with the rest of their nets, part by part with the turns
    // before (two resistors joined crossed to each other: one of them turns, not both). Diodes and the like never.
    {
        auto pins=placedPins();
        auto others=[&](int net,int part){QSet<int> pieces;if(net<0)return pieces;for(const auto &pin:targets.nets[net].pins){const auto it=pins.constFind({pin.component,pin.pin});if(it!=pins.cend()&&it->part!=part)pieces.insert(it->piece);}return pieces;};
        for(int k=0;k<check.parts.size();k++){
            auto &p=check.parts[k];const auto &target=targets.components[p.target];
            if(!interchangeable(p.part,target)||p.at.size()!=2)continue;
            const int first=netOf.value({target.id,target.pins[p.pins[0]]},-1),second=netOf.value({target.id,target.pins[p.pins[1]]},-1);
            const auto a=others(first,k),b=others(second,k);const int g0=piece(p.at[0]),g1=piece(p.at[1]);
            if(int(a.contains(g1))+int(b.contains(g0))>int(a.contains(g0))+int(b.contains(g1))){
                std::swap(p.pins[0],p.pins[1]);p.swapped=true;
                for(int i=0;i<2;i++)pins.insert({target.id,target.pins[p.pins[i]]},{k,piece(p.at[i]),p.at[i]});
            }
        }
    }
    // Each net's pins on the board by piece: pieces apart from the largest are open, joined by the shortest lines; a
    // piece with pins of several nets joins them.
    const auto pins=placedPins();QMap<int,QList<int>> netsOn;QMap<int,QPointF> where;
    for(int n=0;n<targets.nets.size();n++){
        QMap<int,QList<int>> pieces;QList<Placed> on;QList<documents::TargetPin> named;
        for(const auto &pin:targets.nets[n].pins){const auto it=pins.find({pin.component,pin.pin});if(it==pins.end())continue;pieces[it->piece]<<int(on.size());on<<*it;named<<pin;}
        for(auto it=pieces.begin();it!=pieces.end();++it){if(!netsOn[it.key()].contains(n))netsOn[it.key()]<<n;if(!where.contains(it.key())||netsOn[it.key()].size()==2)where[it.key()]=on[it.value().first()].at;}
        if(pieces.size()<2)continue;
        int largest=pieces.firstKey();for(auto it=pieces.begin();it!=pieces.end();++it)if(it.value().size()>pieces[largest].size())largest=it.key();
        TargetCheck::Open open{n,{},{}};
        {QList<documents::TargetPin> piece;for(int i:pieces[largest])piece<<named[i];open.pieces<<piece;}
        for(auto it=pieces.begin();it!=pieces.end();++it)if(it.key()!=largest){QList<documents::TargetPin> piece;for(int i:it.value()){piece<<named[i];open.pins<<named[i];}open.pieces<<piece;}
        check.open<<open;
        // Prim from the largest piece: each piece not reached keeps its shortest line to those reached, measured again only
        // against the pins of the piece reached last.
        const QList<QList<int>> members=pieces.values();const int m=int(members.size());
        QList<double> best(m,std::numeric_limits<double>::max());QList<QLineF> line(m);QList<bool> reached(m,false);
        auto reach=[&](int r){
            reached[r]=true;
            for(int k=0;k<m;k++)if(!reached[k])for(int i:members[r])for(int j:members[k]){const QLineF l(on[i].at,on[j].at);if(l.length()<best[k]){best[k]=l.length();line[k]=l;}}
        };
        reach(int(pieces.keys().indexOf(largest)));
        for(int step=1;step<m;step++){
            int next=-1;for(int k=0;k<m;k++)if(!reached[k]&&(next<0||best[k]<best[next]))next=k;
            check.airwires<<line[next];reach(next);
        }
    }
    QSet<QPair<int,int>> reported;
    for(auto it=netsOn.begin();it!=netsOn.end();++it)for(int i=1;i<it.value().size();i++){
        const auto pair=qMakePair(qMin(it.value()[0],it.value()[i]),qMax(it.value()[0],it.value()[i]));
        if(!reported.contains(pair)){reported.insert(pair);check.joined<<TargetCheck::Joined{pair.first,pair.second,where[it.key()]};}
    }
    return check;
}
QList<TargetChange> targetChanges(const TargetCheck &check,const documents::Targets &targets){
    QList<TargetChange> list;
    auto add=[&](const TargetCheck::Part &p,bool assigned){
        const auto &target=targets.components[p.target];TargetChange change;change.uid=p.part.uid;
        if(p.part.id!=target.id)change.component=target.id;
        if(p.part.designator!=target.designator||p.part.value!=target.value){change.text=true;change.designator=target.designator;change.value=target.value;}
        if(assigned&&p.swapped)for(int pin:p.pins)change.pins<<target.pins.value(pin);
        if(!change.component.isEmpty()||change.text||!change.pins.isEmpty())list<<change;
    };
    for(const auto &p:check.parts)add(p,true);
    for(const auto &p:check.unassigned)add(p,false);
    return list;
}
int applyTargetChanges(Project &board,const QList<TargetChange> &changes){
    int count=0;
    for(const auto &change:changes){
        bool done=false;
        if(!change.component.isEmpty())done=board.linkComponent(change.uid,change.component)||done;
        if(change.text){
            QString id;for(const auto &part:board.components())if(part.uid==change.uid)id=part.id;
            done=(!id.isEmpty()&&board.setComponent(id,change.designator,change.value))||done;
        }
        if(!change.pins.isEmpty())done=board.setPins(change.uid,change.pins)||done;
        count+=done;
    }
    return count;
}
QList<LibraryChoice> openLibraryChoices(const QString &page,const QJsonObject &json){
    QList<LibraryChoice> list;int index=0;
    for(const auto &part:openLibraryParts(json)){
        LibraryChoice c{page,index++,part["name"].toString(),part["id"].toString(),part["value"].toString(),{}};
        const auto names=part["pin_names"].toArray();const auto pins=part["pins"].toArray();const int count=int(pins.size());
        for(int k=0;k<count;k++)c.pins<<(names.size()==count?names[k].toString():QString::number(k+1));
        // Pins in holes: each at whole holes (an electrolytic with 1.5 mm pitch has one between them), and no part the
        // library marks as off the grid.
        c.onGrid=!part["off_grid"].toBool();
        for(const auto &v:pins){const auto at=v.toObject()["at"].toArray();
            for(int axis=0;axis<2;axis++){const double d=at.at(axis).toDouble();if(std::abs(d-std::round(d))>1e-6)c.onGrid=false;}}
        list<<c;
    }
    return list;
}
namespace {
// The kind of part a Kennung names, as schematics and libraries spell it.
QString family(const QString &id){
    static const QRegularExpression letters(QStringLiteral("^([A-Za-z]+)"));const auto prefix=letters.match(id).captured(1).toUpper();
    static const QMap<QString,QString> kinds{{"LED","D"},{"V","D"},{"Q","T"},{"U","IC"},{"N","IC"},{"J","X"},{"BU","X"},{"KL","X"},{"SW","S"},{"TA","S"},{"SI","F"},{"REL","K"},{"BR","B"},{"P","R"},{"RV","R"},{"VR","R"}};
    return kinds.value(prefix,prefix);
}
}
bool sameKind(const QString &designator,const QString &id){const auto kind=family(designator);return !kind.isEmpty()&&kind==family(id);}
QList<LibraryChoice> fittingParts(const documents::TargetComponent &component,const QList<LibraryChoice> &library){
    struct Ranked {int score,order;LibraryChoice choice;};QList<Ranked> ranked;
    const auto kind=family(kindOf(component));const auto value=component.value.trimmed().toUpper();
    for(int i=0;i<library.size();i++){
        const auto &c=library[i];if(c.pins.size()!=component.pins.size())continue;
        const auto pins=assignedPins(c.pins,component.pins);if(pins.contains(-1))continue;
        int score=c.onGrid?4:0;if(!kind.isEmpty()&&family(c.id)==kind)score+=2;
        if(!value.isEmpty()&&(c.name.toUpper().contains(value)||c.value.toUpper().contains(value)||(!c.value.trimmed().isEmpty()&&value.contains(c.value.trimmed().toUpper()))))score+=1;
        ranked<<Ranked{score,i,c};
    }
    std::stable_sort(ranked.begin(),ranked.end(),[](const Ranked &a,const Ranked &b){return a.score>b.score;});
    QList<LibraryChoice> result;for(const auto &r:ranked)result<<r.choice;return result;
}
QList<int> placeBeside(Project &board,const QList<std::pair<documents::TargetComponent,LibraryChoice>> &parts,const QMap<QString,QByteArray> &pages){
    QList<int> added;const double gap=254;
    // Right of everything on the board and of the board itself, from its top.
    double left=board.width+3*gap;for(const auto &p:board.placedObjects())left=qMax(left,p.transform.mapRect(legacyBounds(p.node)).right()+2*gap);
    double x=std::ceil(left/254)*254,y=0,column=0;
    for(const auto &[component,choice]:parts){
        if(!pages.contains(choice.page))continue;
        const auto key=board.addLibrary(pages[choice.page],choice.page,"lib");const auto node=board.libraryNode(key,choice.index);
        const QRectF bounds=legacyBounds(node);const QPointF anchor=componentAnchor(node);
        if(y>0&&y+bounds.height()>board.height){x=std::ceil((x+column+2*gap)/254)*254;y=0;column=0;}
        // The anchor on the hole grid, so that the pins stay on it; the part's top left at the column and row.
        const double px=std::ceil((x+anchor.x()-bounds.left())/254)*254,py=std::ceil((y+anchor.y()-bounds.top())/254)*254;
        QJsonObject placement{{"type","component"},{"library",key},{"index",choice.index},{"x",px},{"y",py},{"value",component.value},{"component",component.id}};
        // The Kennung as LochMaster keeps it: "R5" as "R#" with the number 5.
        static const QRegularExpression numbered(QStringLiteral("^(.*\\D)(\\d+)$"));const auto match=numbered.match(component.designator);
        placement["id"]=match.hasMatch()?match.captured(1)+'#':component.designator;placement["group_value"]=match.hasMatch()?match.captured(2).toInt():0;
        board.additions.append(placement);added<<int(board.additions.size())-1;
        y=py-anchor.y()+bounds.bottom()+gap;column=qMax(column,px-anchor.x()+bounds.right()-x);
    }
    board.assignIds();
    return added;
}
}
