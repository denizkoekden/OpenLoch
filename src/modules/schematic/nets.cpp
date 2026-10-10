#include "nets.h"
#include <QHash>
#include <QMap>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace openloch::schematic {
namespace {
struct Node {
    enum Kind {Conductor,Pin,Junction,Label} kind;
    int sheet=0;
    int order=0;            // drawing order on the sheet, for sorting
    QString id,component;
    QPolygonF points;       // conductor: its nodes; the others: one point
    QString name;           // label
    bool global=false;
};
struct Sets {
    std::vector<int> parent;
    explicit Sets(int n):parent(n){std::iota(parent.begin(),parent.end(),0);}
    int find(int i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;}
    void join(int a,int b){a=find(a);b=find(b);if(a!=b)parent[std::max(a,b)]=std::min(a,b);}
};
bool meet(QPointF a,QPointF b){return std::hypot(a.x()-b.x(),a.y()-b.y())<tolerance;}
bool onSegment(QPointF p,QPointF a,QPointF b){
    const QPointF d=b-a;const double l=d.x()*d.x()+d.y()*d.y();
    if(l<1e-18)return meet(p,a);
    const double t=std::clamp(((p.x()-a.x())*d.x()+(p.y()-a.y())*d.y())/l,0.,1.);
    return meet(p,a+t*d);
}
bool onConductor(QPointF p,const QPolygonF &line){
    for(int i=0;i+1<line.size();i++)if(onSegment(p,line[i],line[i+1]))return true;
    return line.size()==1&&meet(p,line[0]);
}
void gather(const QList<Item> &items,int sheet,std::vector<Node> &out,int &order){
    for(const auto &i:items){
        order++;
        switch(i.type){
        case ItemType::Line:
            if(i.electrical&&i.points.size()>=2)out.push_back({Node::Conductor,sheet,order,i.id,{},i.points,{},false});
            break;
        case ItemType::Junction:out.push_back({Node::Junction,sheet,order,i.id,{},QPolygonF{i.pos},{},false});break;
        case ItemType::NetLabel:out.push_back({Node::Label,sheet,order,i.id,{},QPolygonF{i.pos},i.text.trimmed(),i.global});break;
        case ItemType::Group:gather(i.children,sheet,out,order);break;
        case ItemType::Component:
            for(const auto &c:i.children)
                if(c.type==ItemType::Contact&&c.hasPin)out.push_back({Node::Pin,sheet,order,c.id,i.id,QPolygonF{pinPosition(i,c)},{},false});
            break;
        default:break;
        }
    }
}
}

QList<Net> deriveNets(const Document &document){
    std::vector<Node> nodes;
    for(int s=0;s<document.sheets.size();s++){int order=0;gather(document.sheets[s].items,s,nodes,order);}
    Sets sets(int(nodes.size()));
    // Rules 2 to 5 on each sheet: only end points, connection points, junctions and labels make connections.
    for(int a=0;a<int(nodes.size());a++){
        const Node &n=nodes[a];
        for(int b=a+1;b<int(nodes.size());b++){
            const Node &m=nodes[b];if(m.sheet!=n.sheet)continue;
            bool joined=false;
            if(n.kind==Node::Conductor&&m.kind==Node::Conductor){
                joined=onConductor(n.points.first(),m.points)||onConductor(n.points.last(),m.points)
                     ||onConductor(m.points.first(),n.points)||onConductor(m.points.last(),n.points);
            }else if(n.kind==Node::Conductor||m.kind==Node::Conductor){
                const Node &line=n.kind==Node::Conductor?n:m,&point=n.kind==Node::Conductor?m:n;
                joined=onConductor(point.points[0],line.points);
            }else joined=meet(n.points[0],m.points[0])&&!(n.kind==Node::Label&&m.kind==Node::Label);
            if(joined)sets.join(a,b);
        }
    }
    // Rules 6 and 7: names.
    QHash<QString,int> local,global;
    for(int a=0;a<int(nodes.size());a++){
        const Node &n=nodes[a];if(n.kind!=Node::Label||n.name.isEmpty())continue;
        const QString key=QString::number(n.sheet)+u'\n'+n.name;
        if(local.contains(key))sets.join(a,local[key]);else local.insert(key,a);
        if(n.global){if(global.contains(n.name))sets.join(a,global[n.name]);else global.insert(n.name,a);}
    }
    // Collect the nets in the order of their first element.
    QMap<int,int> netOfRoot;QList<Net> nets;QList<std::pair<int,int>> firsts;   // (sheet, order) of the first element
    for(int a=0;a<int(nodes.size());a++){
        const int root=sets.find(a);
        if(!netOfRoot.contains(root)){netOfRoot.insert(root,int(nets.size()));nets.append(Net());firsts.append({nodes[a].sheet,nodes[a].order});}
        Net &net=nets[netOfRoot[root]];const Node &n=nodes[a];
        switch(n.kind){
        case Node::Conductor:net.conductors.append({n.sheet,n.id});break;
        case Node::Pin:net.pins.append({n.sheet,n.component,n.id});break;
        case Node::Junction:net.junctions.append({n.sheet,n.id});break;
        case Node::Label:net.labels.append({n.sheet,n.id});if(!net.names.contains(n.name)&&!n.name.isEmpty())net.names.append(n.name);break;
        }
    }
    QList<int> order(nets.size());std::iota(order.begin(),order.end(),0);
    for(auto &n:nets){std::sort(n.names.begin(),n.names.end());if(!n.names.isEmpty())n.name=n.names.first();}
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){
        const bool na=!nets[a].name.isEmpty(),nb=!nets[b].name.isEmpty();
        if(na!=nb)return na;
        if(na&&nets[a].name!=nets[b].name)return nets[a].name<nets[b].name;
        return firsts[a]<firsts[b];
    });
    QList<Net> out;for(int i:order)out.append(nets[i]);
    return out;
}
const Net *netOf(const QList<Net> &nets,const PinRef &pin){
    for(const auto &n:nets)if(n.pins.contains(pin))return &n;
    return nullptr;
}
}
