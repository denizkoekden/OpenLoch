#include "netcheck.h"
#include "copper.h"
#include "footprints.h"
#include "language.h"
#include "targetcheck.h"
#include <QHash>
#include <optional>
#include <QLineF>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <limits>

namespace openloch::pcb {
namespace {
bool isPad(const Element &e){return e.type==ElementType::Pad||e.type==ElementType::SmdPad;}
// Pieces of copper joined further, by any numbers standing for them.
struct Pieces {
    QHash<int,int> parent;
    int find(int x){int r=x;while(parent.contains(r)&&parent[r]!=r)r=parent[r];while(x!=r){const int next=parent.value(x,x);parent[x]=r;x=next;}return r;}
    void join(int a,int b){a=find(a);b=find(b);if(a!=b)parent[std::max(a,b)]=std::min(a,b);}
};
}

QList<int> padsOf(const Board &b,int designator){
    QList<int> out;if(designator<0||designator>=b.elements.size()||!b.elements[designator].part)return out;const int part=b.elements[designator].part;
    for(int i=0;i<b.elements.size();i++)if(b.elements[i].part==part&&isPad(b.elements[i]))out<<i;
    return out;
}
QList<int> NetCheck::padsFor(const documents::Targets &targets,const QString &component,const QString &pin) const{
    for(const auto *list:{&parts,&unassigned})for(const auto &p:*list){
        if(p.target<0||p.target>=targets.components.size())continue;const auto &t=targets.components[p.target];if(t.id!=component)continue;
        const int i=int(t.pins.indexOf(pin));return i>=0&&i<p.pads.size()?p.pads[i]:QList<int>{};
    }
    return {};
}

NetCheck checkNets(const Board &board,const documents::Targets &targets){
    NetCheck check;const auto parts=components(board);const auto &els=board.elements;
    // Which board component stands for which of the schematic: the one with its identifier, else the one with its
    // designator, regardless of case; a designator more than one component has is not taken.
    QMap<int,int> partOf;QSet<int> used;
    for(int t=0;t<targets.components.size();t++)for(int k=0;k<parts.size();k++)
        if(!used.contains(k)&&!parts[k].id.isEmpty()&&parts[k].id==targets.components[t].id){partOf[t]=k;used.insert(k);break;}
    QHash<QString,QList<int>> byDesignator;
    for(int k=0;k<parts.size();k++){const QString d=els[parts[k].designator].text.trimmed().toUpper();if(!d.isEmpty())byDesignator[d]<<k;}
    for(int t=0;t<targets.components.size();t++){
        if(partOf.contains(t))continue;
        const QString designator=targets.components[t].designator;const auto candidates=byDesignator.value(designator.trimmed().toUpper());
        if(candidates.size()>1){if(!check.ambiguous.contains(designator))check.ambiguous<<designator;continue;}
        if(candidates.size()==1&&!used.contains(candidates[0])){partOf[t]=candidates[0];used.insert(candidates[0]);}else check.missing<<t;
    }
    for(int k=0;k<parts.size();k++)if(!used.contains(k))check.extra<<parts[k].designator;
    // The pads of each component by the pins of the schematic, as the shared rules name them; a component with a pin no
    // pad stands for wants its pins assigned and stays out of the nets.
    for(auto it=partOf.begin();it!=partOf.end();++it){
        const auto &target=targets.components[it.key()];const auto &c=parts[it.value()];
        NetCheck::Part p;p.target=it.key();p.designator=c.designator;p.value=c.value;p.members=c.members;p.byDesignator=c.id!=target.id;
        for(int k=0;k<target.pins.size();k++)p.pads.append(QList<int>{});
        QStringList names;for(int i:c.members){const auto &e=els[i];const QString pin=e.pin.trimmed();if(isPad(e)&&!pin.isEmpty()&&!names.contains(pin))names<<pin;}
        const auto pins=assignedPins(names,target.pins);
        for(int i:c.members){const auto &e=els[i];if(!isPad(e))continue;const int k=int(names.indexOf(e.pin.trimmed()));if(k>=0&&pins.value(k,-1)>=0)p.pads[pins[k]]<<i;}
        const bool complete=std::all_of(p.pads.begin(),p.pads.end(),[](const QList<int> &l){return !l.isEmpty();});
        // Pins 1 and 2 of a resistor, capacitor or coil (by its kind, whatever sheet number or prefix the designator
        // carries), so numbered on both sides, may be the other way round.
        p.swapped=complete&&QStringList({"R","C","L"}).contains(kindOf(target))&&QSet<QString>(names.begin(),names.end())==QSet<QString>{"1","2"}
            &&target.pins.size()==2&&QSet<QString>(target.pins.begin(),target.pins.end())==QSet<QString>{"1","2"};
        (complete?check.parts:check.unassigned)<<p;
    }
    // The piece of copper under each pad as the test tool finds it; a pad without copper (a plain hole) is a piece of its
    // own. The pads of one pin are joined inside the component, as the two pads of a push button are.
    const auto copper=connections(board,false);Pieces pieces;
    auto raw=[&](int pad){const int c=copper.value(pad,-1);return c>=0?c:-1-pad;};
    for(const auto &p:check.parts)for(const auto &pads:p.pads)for(int k=1;k<pads.size();k++)pieces.join(raw(pads[0]),raw(pads[k]));
    auto piece=[&](int pad){return pieces.find(raw(pad));};
    QHash<QPair<QString,QString>,int> netOf;
    for(int n=0;n<targets.nets.size();n++)for(const auto &pin:targets.nets[n].pins)netOf.insert({pin.component,pin.pin},n);
    struct Placed {int part=-1;QList<int> pads;};
    auto placed=[&]{
        QHash<QPair<QString,QString>,Placed> out;
        for(int k=0;k<check.parts.size();k++){const auto &p=check.parts[k];const auto &target=targets.components[p.target];
            for(int i=0;i<p.pads.size();i++)out.insert({target.id,target.pins[i]},{k,p.pads[i]});}
        return out;
    };
    // The way round of interchangeable pins: the one that joins more of them with the rest of their nets, taken component
    // by component with the turns before (two resistors joined crossed to each other: one of them turns, not both).
    {
        auto pins=placed();
        auto others=[&](int net,int part){QSet<int> found;if(net<0)return found;
            for(const auto &pin:targets.nets[net].pins){const auto it=pins.find({pin.component,pin.pin});if(it!=pins.end()&&it->part!=part)for(int pad:it->pads)found.insert(piece(pad));}
            return found;};
        auto onAny=[&](const QSet<int> &found,const QList<int> &pads){return std::any_of(pads.begin(),pads.end(),[&](int pad){return found.contains(piece(pad));});};
        for(int k=0;k<check.parts.size();k++){
            auto &p=check.parts[k];if(!p.swapped)continue;p.swapped=false;const auto &target=targets.components[p.target];
            const int one=int(target.pins.indexOf(QStringLiteral("1"))),two=int(target.pins.indexOf(QStringLiteral("2")));
            const auto a=others(netOf.value({target.id,QStringLiteral("1")},-1),k),b=others(netOf.value({target.id,QStringLiteral("2")},-1),k);
            const int straight=int(onAny(a,p.pads[one]))+int(onAny(b,p.pads[two])),crossed=int(onAny(a,p.pads[two]))+int(onAny(b,p.pads[one]));
            if(crossed>straight){std::swap(p.pads[one],p.pads[two]);p.swapped=true;pins[{target.id,QStringLiteral("1")}].pads=p.pads[one];pins[{target.id,QStringLiteral("2")}].pads=p.pads[two];}
        }
    }
    // Each net's pins by piece: pieces apart from the one with the most pins are open, joined by the shortest lines from
    // pad to pad; a piece with pins of several nets joins them.
    const auto pins=placed();QMap<int,QList<int>> netsOn;QMap<int,QPointF> where;
    for(int n=0;n<targets.nets.size();n++){
        QList<documents::TargetPin> named;QList<int> pads,pinOf;QMap<int,QList<int>> found;    // found: piece → places in `pads`
        for(const auto &pin:targets.nets[n].pins){
            const auto it=pins.find({pin.component,pin.pin});if(it==pins.end())continue;const int k=int(named.size());named<<pin;
            for(int pad:it->pads){found[piece(pad)]<<int(pads.size());pads<<pad;pinOf<<k;}
        }
        for(auto it=found.begin();it!=found.end();++it){
            if(!netsOn[it.key()].contains(n))netsOn[it.key()]<<n;
            if(!where.contains(it.key())||netsOn[it.key()].size()==2)where[it.key()]=els[pads[it.value().first()]].pos;
        }
        if(found.size()<2)continue;
        // The pieces in order with the pins on each; the one with the most pins is the net's body.
        const QList<int> keys=found.keys();const int m=int(keys.size());QList<QList<int>> pinsOf(m);
        for(int k=0;k<m;k++)for(int i:found[keys[k]])if(!pinsOf[k].contains(pinOf[i]))pinsOf[k]<<pinOf[i];
        int largest=0;for(int k=1;k<m;k++)if(pinsOf[k].size()>pinsOf[largest].size())largest=k;
        NetCheck::Open open;open.net=n;
        {QList<documents::TargetPin> first;for(int k:pinsOf[largest])first<<named[k];open.pieces<<first;}
        for(int k=0;k<m;k++)if(k!=largest){QList<documents::TargetPin> apart;for(int i:pinsOf[k]){apart<<named[i];open.pins<<named[i];}open.pieces<<apart;}
        check.open<<open;
        // Prim from the largest piece: each piece not reached keeps its shortest line to those reached, measured again only
        // against the pads of the piece reached last.
        QList<double> best(m,std::numeric_limits<double>::max());QList<int> from(m,-1),to(m,-1);QList<bool> reached(m,false);
        auto reach=[&](int r){
            reached[r]=true;
            for(int k=0;k<m;k++)if(!reached[k])for(int i:found[keys[r]])for(int j:found[keys[k]]){
                const double l=QLineF(els[pads[i]].pos,els[pads[j]].pos).length();if(l<best[k]){best[k]=l;from[k]=pads[i];to[k]=pads[j];}}
        };
        reach(largest);
        for(int step=1;step<m;step++){
            int next=-1;for(int k=0;k<m;k++)if(!reached[k]&&(next<0||best[k]<best[next]))next=k;
            if(next<0||from[next]<0)break;check.airwires<<NetCheck::Airwire{from[next],to[next],n};reach(next);
        }
    }
    // Nets joined, with the copper joining them.
    QSet<QPair<int,int>> reported;
    for(auto it=netsOn.begin();it!=netsOn.end();++it)for(int i=1;i<it.value().size();i++){
        const auto pair=qMakePair(std::min(it.value()[0],it.value()[i]),std::max(it.value()[0],it.value()[i]));
        if(reported.contains(pair))continue;reported.insert(pair);
        QList<int> elements;for(int e=0;e<els.size();e++)if((copper[e]>=0||isPad(els[e]))&&piece(e)==it.key())elements<<e;
        check.joined<<NetCheck::Joined{pair.first,pair.second,where[it.key()],elements};
    }
    return check;
}

QList<NetChange> netChanges(const Board &board,const NetCheck &check,const documents::Targets &targets){
    QList<NetChange> list;
    auto add=[&](const NetCheck::Part &p,bool assigned){
        const auto &target=targets.components[p.target];const auto &d=board.elements[p.designator];NetChange change;change.designator=p.designator;
        if(d.component!=target.id)change.component=target.id;
        const QString value=p.value>=0?board.elements[p.value].text:QString();
        if(d.text!=target.designator||value!=target.value){change.text=true;change.designatorText=target.designator;change.value=target.value;}
        change.swapPins=assigned&&p.swapped;
        if(!change.component.isEmpty()||change.text||change.swapPins)list<<change;
    };
    for(const auto &p:check.parts)add(p,true);
    for(const auto &p:check.unassigned)add(p,false);
    return list;
}
int applyNetChanges(Board &board,const QList<NetChange> &changes){
    int count=0;
    for(const auto &c:changes){
        if(c.designator<0||c.designator>=board.elements.size()||board.elements[c.designator].role!=TextRole::Designator)continue;
        const QList<Element> before=board.elements;
        if(!c.component.isEmpty())board.elements[c.designator].component=c.component;
        if(c.text)setComponentText(board,board.elements[c.designator].component,c.designatorText,c.value);
        if(c.swapPins)for(int i:padsOf(board,c.designator)){
            auto &e=board.elements[i];const QString pin=e.pin.trimmed();if(pin!=QStringLiteral("1")&&pin!=QStringLiteral("2"))continue;
            const QString to=pin==QStringLiteral("1")?QStringLiteral("2"):QStringLiteral("1");if(e.name.trimmed()==pin)e.name=to;e.pin=to;
        }
        count+=board.elements!=before;
    }
    return count;
}
bool padsOnGrid(const QList<Element> &elements,double grid){
    if(grid<=0)return true;std::optional<QPointF> first;
    auto whole=[grid](double v){const double k=v/grid;return std::abs(k-std::round(k))<1e-6;};
    for(const auto &e:elements)if(isPad(e)){if(!first){first=e.pos;continue;}const QPointF d=e.pos-*first;if(!whole(d.x())||!whole(d.y()))return false;}
    return true;
}
namespace {
// Whether the pins are a transistor's: B, C, E or G, D, S, each once, in any case and order; true for a field-effect one.
std::optional<bool> transistorKind(const QStringList &pins){
    QStringList upper;for(const auto &p:pins)upper<<p.trimmed().toUpper();
    std::sort(upper.begin(),upper.end());
    if(upper==QStringList{"B","C","E"})return false;
    if(upper==QStringList{"D","G","S"})return true;
    return std::nullopt;
}
// The component's pins for lead names (B, C, E, …), as spelt there; empty unless each names one of them.
QStringList pinsFor(const QStringList &leads,const QStringList &pins){
    QStringList out;
    for(const auto &lead:leads){
        QString found;for(const auto &p:pins)if(p.trimmed().compare(lead,Qt::CaseInsensitive)==0)found=p.trimmed();
        if(found.isEmpty())return {};out<<found;
    }
    return out;
}
}
bool transistorPins(const QStringList &pins){return transistorKind(pins).has_value();}
QStringList usualLeads(const QString &footprint,const documents::TargetComponent &component){
    const auto fet=transistorKind(component.pins);if(!fet)return {};
    return pinsFor(transistorLeads(footprint,component.value,*fet),component.pins);
}
QList<PartChoice> partChoices(const documents::TargetComponent &component,double grid){
    const auto library=footprints();QList<LibraryChoice> choices;
    for(int i=0;i<library.size();i++){
        const auto &f=library[i];LibraryChoice c{QStringLiteral("footprint"),i,f.name,f.prefix,{},{}};
        for(const auto &e:f.elements){if(isPad(e))c.pins<<e.name.trimmed();if(e.role==TextRole::Value)c.value=e.text;}
        choices<<c;
    }
    QList<PartChoice> typed,fitting,ordered;QSet<int> taken;const auto kind=kindOf(component);
    for(const auto &c:fittingParts(component,choices)){const bool same=sameKind(kind,c.id);fitting<<PartChoice{c.index,false,same,c.name,{},same};taken.insert(c.index);}
    // A transistor's packages in lead orders, never in the order of its pins: the type's or the package's order, for a
    // TO-92 the usual ones besides. Only the type's own order in a TO-92 is sure.
    if(const auto fet=transistorKind(component.pins))for(const auto &c:choices){
        if(taken.contains(c.index)||c.pins.size()!=3)continue;
        const QString id=library[c.index].id;const auto own=usualLeads(id,component);
        QList<QStringList> orders;if(!own.isEmpty())orders<<own;
        if(id==u"to-92")for(const auto &o:to92Orders(*fet))if(const auto p=pinsFor(o,component.pins);!p.isEmpty()&&!orders.contains(p))orders<<p;
        if(orders.isEmpty())continue;
        taken.insert(c.index);
        for(const auto &o:orders){
            const bool sure=id==u"to-92"&&o==own;const QString order=o.join(u'-');
            const PartChoice p{c.index,false,sameKind(kind,c.id),sure?ui("%1 – Anschlussfolge %2 (%3)").arg(c.name,order,component.value.trimmed()):ui("%1 – Anschlussfolge %2").arg(c.name,order),o,sure};
            (sure?typed:ordered)<<p;
        }
    }
    for(const auto &c:choices)if(!taken.contains(c.index)&&!c.pins.isEmpty()&&c.pins.size()==component.pins.size())
        ordered<<PartChoice{c.index,true,sameKind(kind,c.id),ui("%1 – Anschlüsse der Reihe nach").arg(c.name),{},false};
    std::stable_sort(ordered.begin(),ordered.end(),[](const PartChoice &a,const PartChoice &b){return a.sameKind&&!b.sameKind;});
    // Among those of the same kind, footprints with their pads on the board's grid first.
    if(grid>0){
        auto onGrid=[&](const PartChoice &c){return c.footprint>=0&&c.footprint<library.size()&&padsOnGrid(library[c.footprint].elements,grid);};
        for(auto *list:{&fitting,&ordered})std::stable_sort(list->begin(),list->end(),[&](const PartChoice &a,const PartChoice &b){
            if(a.sameKind!=b.sameKind)return a.sameKind;return onGrid(a)&&!onGrid(b);});
    }
    auto out=typed+fitting+ordered;
    if(!component.pins.isEmpty())out<<PartChoice{-1,true,false,ui("Reihe mit %1 Pads").arg(component.pins.size()),{},false};
    return out;
}
QList<Element> missingPart(const PartChoice &choice,const documents::TargetComponent &component,const Board &board){
    Footprint f;
    if(choice.footprint<0){Wizard w=wizardDefaults(Wizard::SingleRow);w.count=std::max(1,int(component.pins.size()));f=wizardFootprint(w);}
    else{const auto library=footprints();if(choice.footprint>=library.size())return {};f=library[choice.footprint];}
    Board part;part.elements=placeable(f,board);
    bool named=false;for(auto &e:part.elements)if(e.role==TextRole::Designator){e.component=component.id;named=true;}
    if(!named)return {};
    setComponentText(part,component.id,component.designator,component.value);
    if(!choice.leads.isEmpty()){
        for(auto &e:part.elements){bool numbered=false;const int n=e.name.trimmed().toInt(&numbered);
            if(isPad(e)&&numbered&&n>=1&&n<=choice.leads.size()){e.pin=choice.leads[n-1];e.name=e.pin;}}
    }
    else if(choice.inOrder){int k=0;for(auto &e:part.elements)if(isPad(e)&&k<component.pins.size()){e.pin=component.pins[k++].trimmed();e.name=e.pin;}}
    return part.elements;
}
QList<int> layBeside(Board &board,const QList<QList<Element>> &parts){
    QList<int> added;const double gap=2.54,g=board.grid>0?board.grid:1.27;
    double left=board.width;for(const auto &e:board.elements)left=std::max(left,bounds(e).right());
    double x=left+2*gap,y=0,column=0;
    // Up to the grid: never left of or above where the part was meant to go.
    auto up=[&](double v,double origin){return origin+std::ceil((v-origin)/g-1e-9)*g;};
    for(auto els:parts){
        if(els.isEmpty())continue;
        QRectF r;for(const auto &e:els)r=r.united(bounds(e));
        if(y>0&&y+r.height()>board.height){x+=column+2*gap;y=0;column=0;}
        QPointF shift=QPointF(x,y)-r.topLeft();
        for(const auto &e:els)if(isPad(e)){const QPointF at=e.pos+shift;shift+=QPointF(up(at.x(),board.origin.x()),up(at.y(),board.origin.y()))-at;break;}
        for(auto &e:els)pcb::move(e,shift);
        const QRectF placed=r.translated(shift);
        for(const auto &e:els){board.elements<<e;added<<int(board.elements.size())-1;}
        y=placed.bottom()+gap;column=std::max(column,placed.right()-x);
    }
    return added;
}
bool assignPins(Board &b,int designator,const QStringList &pins,const QString &component){
    const auto pads=padsOf(b,designator);if(pads.isEmpty()||pads.size()!=pins.size())return false;
    for(int k=0;k<pads.size();k++){auto &e=b.elements[pads[k]];const QString pin=pins[k].trimmed();if(e.pin.trimmed()==pin)continue;e.pin=pin;e.name=pin;}
    if(!component.isEmpty())b.elements[designator].component=component;
    return true;
}
}
