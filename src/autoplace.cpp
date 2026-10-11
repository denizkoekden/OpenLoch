#include "autoplace.h"
#include "continuity.h"
#include "geometry.h"
#include "targetcheck.h"
#include <QHash>
#include <QJsonArray>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <random>
#include <tuple>
namespace openloch {
namespace {
constexpr double pitch=254;
struct Hole {int x=0,y=0;bool operator==(const Hole &) const=default;};
Hole holeAt(QPointF p){return {int(std::lround(p.x()/pitch)),int(std::lround(p.y()/pitch))};}
QString keyOf(const QString &kind,int index){return kind+'/'+QString::number(index);}

// A part as the arrangement moves it. Positions are holes relative to its first pin, for each quarter turn about its
// anchor; `first` is where the first pin lies relative to the anchor after the turn.
struct Item {
    int part=-1;                  // in TargetCheck::parts
    QString uid,designator;
    QPointF anchor;               // where its anchor lies now
    double angle=0;               // its angle now
    QPointF first[4];
    QList<Hole> pins[4];
    QList<Hole> cells[4];         // its pins and the holes under its body, each once
    QStringList pinNames;         // per pin the component's pin it stands for, empty for none
    QList<int> net;               // per pin the net, -1 for none
};

// Where the anchor of a part lies on the board, and its transform.
struct Placement {QPointF anchor;QTransform transform;QJsonObject node;double angle=0;};
Placement placementOf(const Project &board,const Project::Component &part,const QHash<QString,QTransform> &transforms){
    Placement p;p.transform=transforms.value(keyOf(part.kind,part.index));QPointF local;
    if(part.kind=="legacy"){
        p.node=board.legacyNode(part.index,false);local=componentAnchor(board.legacy["objects"].toArray()[part.index].toObject());
    }else{
        const auto stored=board.additions[part.index].toObject();auto plain=stored;plain.remove("reference");
        p.node=stored["type"]=="component"?board.componentNode(plain):stored;p.angle=stored["angle"].toDouble();
        local=componentAnchor(board.libraryNode(stored["library"].toString(),stored["index"].toInt()));
        p.anchor=p.transform.map(local);return p;
    }
    p.anchor=p.transform.map(local);p.angle=p.node["angle"].toDouble();return p;
}

// The item of a part whose pins lie at `at`; false when its pins are not a whole number of holes apart.
bool makeItem(Item &item,const Placement &placement,const QList<QPointF> &at){
    if(at.isEmpty())return false;
    QRectF body=placement.transform.mapRect(drawingBounds(placement.node));
    for(int k=0;k<4;k++){
        QTransform turn;turn.rotate(90.0*k);
        const QPointF first=turn.map(at[0]-placement.anchor);item.first[k]=first;item.pins[k].clear();item.cells[k].clear();
        for(const auto &p:at){
            const QPointF d=turn.map(p-placement.anchor)-first;const Hole h=holeAt(d);
            if(std::abs(d.x()-h.x*pitch)>1||std::abs(d.y()-h.y*pitch)>1)return false;
            item.pins[k]<<h;if(!item.cells[k].contains(h))item.cells[k]<<h;
        }
        if(!body.isEmpty()){
            const QRectF r=turn.mapRect(body.translated(-placement.anchor)).translated(-first);
            for(int x=int(std::ceil(r.left()/pitch-1e-6));x*pitch<=r.right()+1e-6;x++)
                for(int y=int(std::ceil(r.top()/pitch-1e-6));y*pitch<=r.bottom()+1e-6;y++)if(!item.cells[k].contains(Hole{x,y}))item.cells[k]<<Hole{x,y};
        }
    }
    item.anchor=placement.anchor;item.angle=placement.angle;return true;
}

// Two parts of the same kind joined to their nets alike: what a mirror-symmetric schematic draws twice. The partner
// follows the lead: turned so that its pins mirror the lead's ones, mirrored about the axis.
struct Mirror {int turn=0,dx=0,dy=0;};
struct Unit {int lead=-1,partner=-1;Mirror mirror[4],self[4];};   // self: the lead mirrored onto itself

struct Model {
    QList<Item> items;
    QList<QList<std::pair<int,int>>> nets;   // per net its pins: item and pin
    int supply=-1,ground=-1;
    int cols=0,rows=0;                       // holes across and down; the outer ones stay free
    QSet<qint64> blocked;                    // holes of parts that stay where they are, and their neighbours
    std::vector<char> blockedGrid;           // the same by hole, for the holes of the board
    QList<Unit> units;
};
struct State {QList<Hole> at;QList<int> turn;int axis=0;};

qint64 cellKey(int x,int y){return (qint64(x)<<32)^qint64(quint32(y));}

struct Eval {
    double length=0;int crossings=0,clash=0,height=0;int top=0,bottom=0;
    double cost(double crossing,double clashing=1000) const{return length+crossing*crossings+2.0*height+clashing*clash;}
};

// Proper crossing of two segments: they cross in a point inside both.
bool crosses(Hole a,Hole b,Hole c,Hole d){
    auto side=[](Hole p,Hole q,Hole r){const qint64 v=qint64(q.x-p.x)*(r.y-p.y)-qint64(q.y-p.y)*(r.x-p.x);return (v>0)-(v<0);};
    const int d1=side(a,b,c),d2=side(a,b,d),d3=side(c,d,a),d4=side(c,d,b);
    return d1*d2<0&&d3*d4<0;
}

Eval evaluate(const Model &m,const State &s,bool area){
    Eval e;
    // The rows of supply and ground: one above and one below everything arranged.
    int top=std::numeric_limits<int>::max(),bottom=std::numeric_limits<int>::min();
    for(int i=0;i<m.items.size();i++)for(const auto &c:m.items[i].cells[s.turn[i]]){top=qMin(top,s.at[i].y+c.y);bottom=qMax(bottom,s.at[i].y+c.y);}
    e.top=top-1;e.bottom=bottom+1;
    if(area){
        // Every hole once, and one free hole between parts: a hole of a part next to one of another part clashes.
        static thread_local std::vector<int> owner;owner.assign(size_t(m.cols)*size_t(m.rows),-1);
        const int minY=1+(m.supply>=0),maxY=m.rows-2-(m.ground>=0);
        for(int i=0;i<m.items.size();i++){
            for(const auto &c:m.items[i].cells[s.turn[i]]){
                const int x=s.at[i].x+c.x,y=s.at[i].y+c.y;
                if(x<1||x>m.cols-2||y<minY||y>maxY){e.clash++;continue;}
                if(m.blockedGrid[size_t(y)*size_t(m.cols)+size_t(x)])e.clash++;
                for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++){
                    const int nx=x+dx,ny=y+dy;if(nx<0||ny<0||nx>=m.cols||ny>=m.rows)continue;
                    const int o=owner[size_t(ny)*size_t(m.cols)+size_t(nx)];if(o>=0&&o!=i)e.clash++;
                }
            }
            for(const auto &c:m.items[i].cells[s.turn[i]]){
                const int x=s.at[i].x+c.x,y=s.at[i].y+c.y;if(x>=0&&y>=0&&x<m.cols&&y<m.rows)owner[size_t(y)*size_t(m.cols)+size_t(x)]=i;
            }
        }
    }
    // Which net each pin hole belongs to (-1: a pin of no net): a line over the pin of another net needs a bridge or a
    // detour, and counts as a crossing.
    // By hole of the board; pins off the board (a part by hand over the edge) in a table of their own.
    constexpr int none=-3;static thread_local std::vector<int> pinGrid;pinGrid.assign(size_t(m.cols)*size_t(m.rows),none);QHash<qint64,int> pinOff;
    auto setPin=[&](int x,int y,int net){if(x>=0&&y>=0&&x<m.cols&&y<m.rows)pinGrid[size_t(y)*size_t(m.cols)+size_t(x)]=net;else pinOff.insert(cellKey(x,y),net);};
    auto pinAt=[&](int x,int y){return x>=0&&y>=0&&x<m.cols&&y<m.rows?pinGrid[size_t(y)*size_t(m.cols)+size_t(x)]:pinOff.value(cellKey(x,y),none);};
    for(int i=0;i<m.items.size();i++)for(int p=0;p<m.items[i].pins[s.turn[i]].size();p++){const auto &h=m.items[i].pins[s.turn[i]][p];setPin(s.at[i].x+h.x,s.at[i].y+h.y,m.items[i].net[p]);}
    auto foreign=[&](Hole a,Hole b,int net){
        // Holes between a and b along a row or a column, without a and b.
        int count=0;const int dx=(b.x>a.x)-(b.x<a.x),dy=(b.y>a.y)-(b.y<a.y);
        for(Hole h{a.x+dx,a.y+dy};!(h==b);h={h.x+dx,h.y+dy}){const int o=pinAt(h.x,h.y);if(o!=none&&(o!=net||net<0))count++;}
        return count;
    };
    auto overPins=[&](Hole a,Hole b,int net){
        if(a.x==b.x||a.y==b.y)return foreign(a,b,net);
        // The better of the two ways round the corner, the corner counted once.
        auto corner=[&](Hole c){const int o=pinAt(c.x,c.y);return foreign(a,c,net)+foreign(c,b,net)+(o!=none&&o!=net?1:0);};
        return qMin(corner({b.x,a.y}),corner({a.x,b.y}));
    };
    // Supply on top, ground at the bottom: the supply pins of a part on one rail above all other pins, its ground pins
    // below them. A part on both rails (a battery) spans between them and is left out.
    {
        QList<int> up,down,rest;
        for(int i=0;i<m.items.size();i++){
            const auto &item=m.items[i];bool onSupply=false,onGround=false;for(int n:item.net){onSupply|=n>=0&&n==m.supply;onGround|=n>=0&&n==m.ground;}
            if(onSupply&&onGround)continue;
            for(int p=0;p<item.net.size();p++){const int row=s.at[i].y+item.pins[s.turn[i]][p].y;(item.net[p]>=0&&item.net[p]==m.supply?up:item.net[p]>=0&&item.net[p]==m.ground?down:rest)<<row;}
        }
        // Counted by sorting: for each supply pin the others above it, for each ground pin the rest below it.
        QList<int> notUp=rest+down;std::sort(notUp.begin(),notUp.end());std::sort(rest.begin(),rest.end());
        for(int u:up)e.height+=int(std::lower_bound(notUp.begin(),notUp.end(),u)-notUp.begin());
        for(int d:down)e.height+=int(rest.end()-std::upper_bound(rest.begin(),rest.end(),d));
    }
    struct Segment {Hole a,b;int net;};QList<Segment> segments;
    for(int n=0;n<m.nets.size();n++){
        QList<Hole> pins;for(const auto &[item,pin]:m.nets[n])pins<<Hole{s.at[item].x+m.items[item].pins[s.turn[item]][pin].x,s.at[item].y+m.items[item].pins[s.turn[item]][pin].y};
        if(pins.size()<2)continue;
        if(n==m.supply||n==m.ground){
            // Along its row, with a line from each pin to it.
            const int row=n==m.supply?e.top:e.bottom;int left=pins[0].x,right=pins[0].x;
            for(const auto &p:pins){left=qMin(left,p.x);right=qMax(right,p.x);e.length+=std::abs(p.y-row);segments<<Segment{p,{p.x,row},n};e.crossings+=foreign(p,{p.x,row},n);}
            e.length+=right-left;continue;
        }
        // A spanning tree along rows and columns (Prim).
        QList<int> distance(pins.size(),std::numeric_limits<int>::max()),from(pins.size(),0);QList<bool> in(pins.size(),false);distance[0]=0;
        for(int round=0;round<pins.size();round++){
            int best=-1;for(int j=0;j<pins.size();j++)if(!in[j]&&(best<0||distance[j]<distance[best]))best=j;
            in[best]=true;e.length+=distance[best];if(round>0){segments<<Segment{pins[from[best]],pins[best],n};e.crossings+=overPins(pins[from[best]],pins[best],n);}
            for(int j=0;j<pins.size();j++)if(!in[j]){const int d=std::abs(pins[j].x-pins[best].x)+std::abs(pins[j].y-pins[best].y);if(d<distance[j]){distance[j]=d;from[j]=best;}}
        }
    }
    for(int i=0;i<segments.size();i++){
        const auto &a=segments[i];const int l=qMin(a.a.x,a.b.x),r=qMax(a.a.x,a.b.x),t=qMin(a.a.y,a.b.y),b=qMax(a.a.y,a.b.y);
        for(int j=i+1;j<segments.size();j++){
            const auto &c=segments[j];if(c.net==a.net||qMax(c.a.x,c.b.x)<l||qMin(c.a.x,c.b.x)>r||qMax(c.a.y,c.b.y)<t||qMin(c.a.y,c.b.y)>b)continue;
            if(crosses(a.a,a.b,c.a,c.b))e.crossings++;
        }
    }
    return e;
}

// Places the partner of a pair after its lead.
void follow(State &s,const Unit &u){
    if(u.partner<0)return;const auto &mirror=u.mirror[s.turn[u.lead]];
    s.turn[u.partner]=mirror.turn;s.at[u.partner]={s.axis-s.at[u.lead].x-mirror.dx,s.at[u.lead].y+mirror.dy};
}

// How `to` turns and lies so that its pins mirror those of `from` turned by `turn`, matched by the component's pin names.
Mirror mirrorOf(const Item &from,int turn,const Item &to){
    Mirror best;int bestMismatch=std::numeric_limits<int>::max();
    for(int k=0;k<4;k++){
        QList<int> xs,ys;
        for(int i=0;i<from.pinNames.size();i++){
            if(from.pinNames[i].isEmpty())continue;const int j=to.pinNames.indexOf(from.pinNames[i]);if(j<0)continue;
            xs<<from.pins[turn][i].x+to.pins[k][j].x;ys<<from.pins[turn][i].y-to.pins[k][j].y;
        }
        if(xs.isEmpty())continue;
        const int mismatch=*std::max_element(xs.begin(),xs.end())-*std::min_element(xs.begin(),xs.end())+*std::max_element(ys.begin(),ys.end())-*std::min_element(ys.begin(),ys.end());
        if(mismatch<bestMismatch){
            bestMismatch=mismatch;double sx=0,sy=0;for(int v:xs)sx+=v;for(int v:ys)sy+=v;
            best={k,int(std::floor(sx/xs.size()+.5)),int(std::floor(sy/ys.size()+.5))};
        }
    }
    return best;
}

// Parts alike by their place in the nets (colour refinement on the graph of parts and nets): pairs of them become
// mirror-symmetric pairs.
QList<Unit> unitsOf(const Model &m,const documents::Targets &targets,const TargetCheck &check){
    QStringList part(m.items.size()),net(m.nets.size());
    for(int i=0;i<m.items.size();i++){auto names=m.items[i].pinNames;names.sort();part[i]=targets.components[check.parts[m.items[i].part].target].kind+'|'+names.join(',');}
    for(int n=0;n<m.nets.size();n++)net[n]=(n==m.supply?"S":n==m.ground?"G":"N")+QString::number(m.nets[n].size());
    for(int round=0;round<8;round++){
        QStringList nextPart(part.size()),nextNet(net.size());
        for(int i=0;i<m.items.size();i++){QStringList around;for(int p=0;p<m.items[i].net.size();p++)around<<m.items[i].pinNames[p]+':'+(m.items[i].net[p]>=0?net[m.items[i].net[p]]:QString());around.sort();nextPart[i]=QString::number(qHash(part[i]+'#'+around.join(';')),16);}
        for(int n=0;n<m.nets.size();n++){QStringList around;for(const auto &[item,pin]:m.nets[n])around<<part[item]+':'+m.items[item].pinNames[pin];around.sort();nextNet[n]=QString::number(qHash(net[n]+'#'+around.join(';')),16);}
        part=nextPart;net=nextNet;
    }
    QMap<QString,QList<int>> classes;for(int i=0;i<part.size();i++)classes[part[i]]<<i;
    QList<Unit> units;QSet<int> paired;
    for(const auto &members:classes)if(members.size()==2){
        Unit u;u.lead=members[0];u.partner=members[1];
        for(int k=0;k<4;k++){u.mirror[k]=mirrorOf(m.items[u.lead],k,m.items[u.partner]);u.self[k]=mirrorOf(m.items[u.lead],k,m.items[u.lead]);}
        units<<u;paired<<u.lead<<u.partner;
    }
    for(int i=0;i<m.items.size();i++)if(!paired.contains(i)){Unit u;u.lead=i;units<<u;}
    std::sort(units.begin(),units.end(),[](const Unit &a,const Unit &b){return a.lead<b.lead;});
    return units;
}

struct Random {
    std::mt19937 engine;
    explicit Random(unsigned seed):engine(seed){}
    int below(int n){return n<=1?0:int(engine()%unsigned(n));}
    int between(int a,int b){return a+below(b-a+1);}
    double unit(){return engine()/4294967296.0;}
};

// Which rail a part touches: 1 supply, 2 ground, 3 both.
int railsOf(const Model &m,const Item &item){int r=0;for(int n:item.net){if(n>=0&&n==m.supply)r|=1;if(n>=0&&n==m.ground)r|=2;}return r;}

State start(const Model &m,Random &random){
    State s;s.at.resize(m.items.size());s.turn.resize(m.items.size());s.axis=m.cols-1;
    const int minY=2,maxY=m.rows-3;
    for(const auto &u:m.units){
        s.turn[u.lead]=random.below(4);const int rails=railsOf(m,m.items[u.lead]);
        const int middle=(minY+maxY)/2;const int y=rails==1?minY+2:rails==2?maxY-2:middle;
        s.at[u.lead]={u.partner>=0?random.between(1,qMax(1,m.cols/2-2)):random.between(1,m.cols-2),qBound(minY,y+random.between(-2,2),maxY)};
        follow(s,u);
    }
    return s;
}

State anneal(const Model &m,State s,Random &random,int steps,double crossing,double *bestCost){
    const bool pairs=std::any_of(m.units.begin(),m.units.end(),[](const Unit &u){return u.partner>=0;});
    // Parts may pass through each other at first; a clash costs more and more, so that the run ends without one.
    Eval now=evaluate(m,s,true);State best=s;double bestValue=std::numeric_limits<double>::max();
    if(now.clash==0)bestValue=now.cost(crossing);
    const double t0=6,t1=0.03;const int reach=qMax(m.cols,m.rows)/2;
    for(int step=0;step<steps;step++){
        const double t=t0*std::pow(t1/t0,double(step)/steps),clashing=4*std::pow(250.0,double(step)/steps);
        State next=s;const int pick=random.below(int(m.units.size()));const Unit &u=m.units[pick];const int move=random.below(100);
        if(move<50){
            const int span=1+int(reach*t/t0);int dx=random.between(-span,span),dy=random.between(-span,span);if(dx==0&&dy==0)dx=1;
            next.at[u.lead]={next.at[u.lead].x+dx,next.at[u.lead].y+dy};follow(next,u);
        }else if(move<65){
            next.turn[u.lead]=(next.turn[u.lead]+1+random.below(3))%4;follow(next,u);
        }else if(move<80&&m.units.size()>1){
            const Unit &v=m.units[(pick+1+random.below(int(m.units.size())-1))%m.units.size()];
            std::swap(next.at[u.lead],next.at[v.lead]);follow(next,u);follow(next,v);
        }else if(move<90&&pairs){
            next.axis+=random.below(2)?1:-1;for(const auto &w:m.units)follow(next,w);
        }else if(u.partner>=0){
            // The pair the other way round: the lead mirrored to the other side, the partner after it.
            const auto &self=u.self[s.turn[u.lead]];
            next.turn[u.lead]=self.turn;next.at[u.lead]={next.axis-s.at[u.lead].x-self.dx,s.at[u.lead].y+self.dy};follow(next,u);
        }else{
            next.turn[u.lead]=(next.turn[u.lead]+1+random.below(3))%4;
        }
        const Eval e=evaluate(m,next,true);const double c=e.cost(crossing,clashing),cost=now.cost(crossing,clashing);
        if(c<=cost||random.unit()<std::exp((cost-c)/t)){
            s=next;now=e;
            if(e.clash==0&&e.cost(crossing)<bestValue){bestValue=e.cost(crossing);best=s;}
        }
    }
    *bestCost=bestValue;return best;
}

// After a run: each unit to the best place within a few holes and turn, as long as that helps.
State polish(const Model &m,State s,double crossing){
    const bool pairs=std::any_of(m.units.begin(),m.units.end(),[](const Unit &u){return u.partner>=0;});
    Eval e=evaluate(m,s,true);double cost=e.cost(crossing);
    for(int round=0;round<12;round++){
        bool better=false;
        for(const auto &u:m.units){
            State best=s;double bestCost=cost;
            for(int k=0;k<4;k++)for(int dx=-3;dx<=3;dx++)for(int dy=-3;dy<=3;dy++){
                State next=s;next.turn[u.lead]=(s.turn[u.lead]+k)%4;next.at[u.lead]={s.at[u.lead].x+dx,s.at[u.lead].y+dy};follow(next,u);
                const Eval n=evaluate(m,next,true);if(n.clash==0&&n.cost(crossing)<bestCost-1e-9){bestCost=n.cost(crossing);best=next;}
            }
            if(bestCost<cost){s=best;cost=bestCost;better=true;}
        }
        if(pairs)for(int d:{-2,-1,1,2}){
            State next=s;next.axis+=d;for(const auto &w:m.units)follow(next,w);
            const Eval n=evaluate(m,next,true);if(n.clash==0&&n.cost(crossing)<cost-1e-9){s=next;cost=n.cost(crossing);better=true;}
        }
        if(!better)break;
    }
    return s;
}

struct Built {Model model;TargetCheck check;QStringList fixed;};
Built build(const Project &board,const documents::Targets &targets,bool forScore){
    Built b;b.check=checkTargets(board,targets);auto &m=b.model;
    QHash<QString,QTransform> transforms;for(const auto &p:board.placedObjects())transforms.insert(keyOf(p.kind,p.index),p.transform);
    m.cols=int(std::floor(board.width/pitch))+1;m.rows=int(std::floor(board.height/pitch))+1;
    QHash<QPair<QString,QString>,int> netOf;
    for(int n=0;n<targets.nets.size();n++)for(const auto &pin:targets.nets[n].pins)netOf.insert({pin.component,pin.pin},n);
    std::tie(m.supply,m.ground)=supplyNets(board,targets);
    auto block=[&](const Placement &placement,const QList<QPointF> &at){
        QList<QPointF> points=at;const QRectF body=placement.transform.mapRect(drawingBounds(placement.node));
        if(!body.isEmpty())for(int x=int(std::ceil(body.left()/pitch));x*pitch<=body.right();x++)for(int y=int(std::ceil(body.top()/pitch));y*pitch<=body.bottom();y++)points<<QPointF(x*pitch,y*pitch);
        for(const auto &p:points){const Hole h=holeAt(p);for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++)m.blocked.insert(cellKey(h.x+dx,h.y+dy));}
    };
    for(int k=0;k<b.check.parts.size();k++){
        const auto &p=b.check.parts[k];const auto &component=targets.components[p.target];
        Item item;item.part=k;item.uid=p.part.uid;item.designator=component.designator;
        const auto placement=placementOf(board,p.part,transforms);
        if(!makeItem(item,placement,p.at)){if(!forScore){b.fixed<<component.designator;block(placement,p.at);}continue;}
        for(int i=0;i<p.pins.size();i++){
            const QString name=p.pins[i]>=0?component.pins[p.pins[i]]:QString();item.pinNames<<name;
            item.net<<(name.isEmpty()?-1:netOf.value({component.id,name},-1));
        }
        m.items<<item;
    }
    // Parts for no component and parts whose pins are not assigned stay where they are.
    if(!forScore){
        QList<Project::Component> others=b.check.extra;for(const auto &p:b.check.unassigned)others<<p.part;
        for(const auto &c:others)if(c.board==board.activeBoard)block(placementOf(board,c,transforms),pinPositions(board,c));
    }
    m.nets.resize(targets.nets.size());
    for(int i=0;i<m.items.size();i++)for(int p=0;p<m.items[i].net.size();p++)if(m.items[i].net[p]>=0)m.nets[m.items[i].net[p]]<<std::pair{i,p};
    m.blockedGrid.assign(size_t(m.cols)*size_t(m.rows),0);
    for(int x=0;x<m.cols;x++)for(int y=0;y<m.rows;y++)m.blockedGrid[size_t(y)*size_t(m.cols)+size_t(x)]=m.blocked.contains(cellKey(x,y));
    return b;
}

// The state of the parts as they lie: no turn, the first pin's hole.
State current(const Model &m){
    State s;for(const auto &item:m.items){s.at<<holeAt(item.anchor+item.first[0]);s.turn<<0;}return s;
}

ArrangeScore scoreOf(const Model &m,const State &s){
    const Eval e=evaluate(m,s,true);ArrangeScore score;score.length=e.length*pitch/100;score.crossings=e.crossings;score.fits=e.clash==0;return score;
}
}

std::pair<int,int> supplyNets(const Project &board,const documents::Targets &targets){
    static const QRegularExpression plusName(QStringLiteral("^(VCC|VDD|V\\+|\\+\\d+([.,]\\d+)?V|\\+UB|UB|\\+)$"),QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression groundName(QStringLiteral("^(GND|0V|MASSE|VSS|AGND|DGND|-)$"),QRegularExpression::CaseInsensitiveOption);
    int supply=-1,ground=-1;
    for(int n=0;n<targets.nets.size();n++){const auto name=targets.nets[n].name.trimmed();if(supply<0&&plusName.match(name).hasMatch())supply=n;if(ground<0&&groundName.match(name).hasMatch())ground=n;}
    if(supply>=0&&ground>=0)return {supply,ground};
    // A source: its pins + and -, as the part or the component names them.
    QHash<QPair<QString,QString>,int> netOf;
    for(int n=0;n<targets.nets.size();n++)for(const auto &pin:targets.nets[n].pins)netOf.insert({pin.component,pin.pin},n);
    const auto check=checkTargets(board,targets);
    // Batteries and sources only: an IC (U) has pins + and - too, the inputs of an operational amplifier.
    auto isSource=[](const QString &kind){return QStringList{"BAT","G","V"}.contains(kind.toUpper());};
    for(const auto &p:check.parts){
        const auto &component=targets.components[p.target];if(!isSource(component.kind))continue;
        for(int i=0;i<p.pins.size()&&i<p.part.pins.size();i++){
            if(p.pins[i]<0)continue;const int n=netOf.value({component.id,component.pins[p.pins[i]]},-1);
            const auto name=p.part.pins[i];
            if(supply<0&&name=="+")supply=n;
            if(ground<0&&(name=="-"||name==QString(QChar(0x2212))))ground=n;
        }
    }
    for(const auto &component:targets.components){
        if(!isSource(component.kind))continue;
        if(supply<0&&component.pins.contains("+"))supply=netOf.value({component.id,"+"},-1);
        if(ground<0&&component.pins.contains("-"))ground=netOf.value({component.id,"-"},-1);
        if(component.kind.toUpper()=="BAT"&&component.pins==QStringList{"1","2"}){
            if(supply<0)supply=netOf.value({component.id,"1"},-1);
            if(ground<0)ground=netOf.value({component.id,"2"},-1);
        }
    }
    if(supply==ground)ground=-1;
    return {supply,ground};
}

ArrangeScore scoreArrangement(const Project &board,const documents::Targets &targets){
    const auto built=build(board,targets,true);return scoreOf(built.model,current(built.model));
}

Arrangement arrangeParts(const Project &board,const documents::Targets &targets,const ArrangeOptions &options){
    auto built=build(board,targets,false);Model &m=built.model;
    Arrangement result;result.fixed=built.fixed;
    const QList<Item> all=m.items;QSet<int> dropped;
    // Parts that cannot fit by their area, with a free hole around each, are left out first, the largest first.
    {
        auto area=[&](const Item &item){int l=0,r=0,t=0,b=0;for(const auto &c:item.cells[0]){l=qMin(l,c.x);r=qMax(r,c.x);t=qMin(t,c.y);b=qMax(b,c.y);}return (r-l+2)*(b-t+2);};
        const int usable=(m.cols-2)*(m.rows-2-(m.supply>=0)-(m.ground>=0));int total=0;for(const auto &item:all)total+=area(item);
        QList<int> bySize;for(int i=0;i<all.size();i++)bySize<<i;std::stable_sort(bySize.begin(),bySize.end(),[&](int a,int b){return area(all[a])>area(all[b]);});
        for(int i:bySize){if(total<=usable)break;dropped<<i;result.beside<<all[i].designator;total-=area(all[i]);}
    }
    State best;bool found=false;
    while(!found){
        // What did not fit is left out, the part with the most holes first.
        m.items.clear();QList<int> kept;for(int i=0;i<all.size();i++)if(!dropped.contains(i)){m.items<<all[i];kept<<i;}
        if(m.items.isEmpty())break;
        m.nets=QList<QList<std::pair<int,int>>>(targets.nets.size());
        for(int i=0;i<m.items.size();i++)for(int p=0;p<m.items[i].net.size();p++)if(m.items[i].net[p]>=0)m.nets[m.items[i].net[p]]<<std::pair{i,p};
        m.units=unitsOf(m,targets,built.check);
        const int passes=options.passes>0?options.passes:m.units.size()<=12?4:2;
        const int steps=options.steps>0?options.steps:qMin(20000+6000*int(m.units.size()),240000/passes);
        double bestCost=std::numeric_limits<double>::max();
        for(int pass=0;pass<passes;pass++){
            Random random(options.seed*7919u+unsigned(pass));double cost=0;
            State s=anneal(m,start(m,random),random,steps,options.crossing,&cost);
            if(cost==std::numeric_limits<double>::max())continue;
            s=polish(m,s,options.crossing);cost=evaluate(m,s,true).cost(options.crossing);
            if(cost<bestCost){bestCost=cost;best=s;found=true;}
        }
        if(!found){
            int largest=-1;for(int i=0;i<m.items.size();i++)if(largest<0||m.items[i].cells[0].size()>m.items[largest].cells[0].size())largest=i;
            dropped<<kept[largest];result.beside<<m.items[largest].designator;
        }
    }
    if(!found)return result;
    // Into the top left corner, one hole from the edge and the supply row on the second row.
    {
        const Eval e=evaluate(m,best,true);int left=std::numeric_limits<int>::max();
        for(int i=0;i<m.items.size();i++)for(const auto &c:m.items[i].cells[best.turn[i]])left=qMin(left,best.at[i].x+c.x);
        const int dx=1-left,dy=(m.supply>=0?e.top:e.top+1)-1;
        State moved=best;for(auto &h:moved.at)h={h.x+dx,h.y-dy};moved.axis+=2*dx;
        if(evaluate(m,moved,true).clash==0)best=moved;
    }
    const Eval e=evaluate(m,best,true);
    result.score=scoreOf(m,best);result.supplyRow=m.supply>=0?e.top:-1;result.groundRow=m.ground>=0?e.bottom:-1;
    for(int i=0;i<m.items.size();i++){
        const auto &item=m.items[i];const int k=best.turn[i];
        const QPointF anchor=QPointF(best.at[i].x*pitch,best.at[i].y*pitch)-item.first[k];
        result.parts<<PartPlacement{item.uid,anchor,std::remainder(item.angle+90.0*k,360.0)};
    }
    return result;
}

bool applyArrangement(Project &board,const Arrangement &arrangement){
    QHash<QString,Project::Component> byUid;for(const auto &c:board.components())if(c.board==board.activeBoard)byUid.insert(c.uid,c);
    QHash<QString,QTransform> transforms;for(const auto &p:board.placedObjects())transforms.insert(keyOf(p.kind,p.index),p.transform);
    for(const auto &p:arrangement.parts)if(!byUid.contains(p.uid))return false;
    for(const auto &p:arrangement.parts){
        const auto &c=byUid[p.uid];
        if(c.kind=="new"){
            auto stored=board.additions[c.index].toObject();stored["x"]=p.at.x();stored["y"]=p.at.y();stored["angle"]=p.angle;board.additions[c.index]=stored;
        }else{
            const auto now=placementOf(board,c,transforms);const QString key=QString::number(c.index);
            const auto move=board.moves.value(key).toArray();const QPointF shift=p.at-now.anchor;
            board.moves[key]=QJsonArray{move.at(0).toDouble()+shift.x(),move.at(1).toDouble()+shift.y()};
            auto edit=board.edits.value(key).toObject();edit["angle"]=p.angle;board.edits[key]=edit;
        }
    }
    return true;
}

namespace {
// The router's grid: per hole the net whose wire or pin lies there, -1 for none, -2 for a pin of no net, of a part not
// arranged, or a wire already on the board.
struct Grid {
    int cols=0,rows=0;std::vector<int> owner;std::vector<char> pin;
    bool inside(int x,int y) const{return x>=1&&y>=1&&x<=cols-2&&y<=rows-2;}
    size_t index(int x,int y) const{return size_t(y)*size_t(cols)+size_t(x);}
};
struct Edge {Hole a,b;bool bridge=false;};
struct Attempt {QList<QList<Edge>> edges;QList<int> openPins;int open=0,bridges=0;double length=0;};

// Joins the pins of the nets in the given order, each net as a tree grown from its rail or its first pin to the nearest
// pin not yet joined (Dijkstra over holes, bends and bridges cost extra).
Attempt route(Grid grid,const QList<QList<Hole>> &netPins,int supply,int ground,int top,int bottom,const QList<int> &order){
    Attempt result;result.edges.resize(netPins.size());result.openPins.fill(-1,netPins.size());
    const int dx[4]={1,0,-1,0},dy[4]={0,1,0,-1};
    for(int n:order){
        const auto &pins=netPins[n];if(pins.size()<2)continue;
        auto free=[&](int x,int y){const int o=grid.owner[grid.index(x,y)];return o==-1||o==n;};
        QSet<qint64> tree;QList<bool> joined(pins.size(),false);
        auto take=[&](Hole h){tree.insert(cellKey(h.x,h.y));grid.owner[grid.index(h.x,h.y)]=n;for(int i=0;i<pins.size();i++)if(pins[i]==h)joined[i]=true;};
        // Supply and ground: first their row over the width of their pins.
        bool seeded=false;
        if(n==supply||n==ground){
            const int row=n==supply?top:bottom;int left=pins[0].x,right=pins[0].x;for(const auto &p:pins){left=qMin(left,p.x);right=qMax(right,p.x);}
            bool clear=row>=1&&row<=grid.rows-2&&left<right;for(int x=left;clear&&x<=right;x++)clear=grid.inside(x,row)&&free(x,row)&&!grid.pin[grid.index(x,row)];
            if(clear){for(int x=left;x<=right;x++){take({x,row});if(x>left)result.edges[n]<<Edge{{x-1,row},{x,row}};}seeded=true;}
        }
        if(!seeded)take(pins[0]);
        while(std::count(joined.begin(),joined.end(),false)>0){
            // States: hole and the direction it was entered from (4: a hole of the tree).
            const size_t states=size_t(grid.cols)*size_t(grid.rows)*5;std::vector<double> cost(states,std::numeric_limits<double>::max());std::vector<qint64> from(states,-1);
            using Entry=std::pair<double,qint64>;std::priority_queue<Entry,std::vector<Entry>,std::greater<>> queue;
            for(qint64 key:tree){const int x=int(key>>32),y=int(qint32(key&0xffffffff));const size_t s=grid.index(x,y)*5+4;cost[s]=0;queue.push({0,qint64(s)});}
            qint64 reached=-1;
            while(!queue.empty()){
                const auto [c,state]=queue.top();queue.pop();if(c>cost[size_t(state)])continue;
                const size_t cell=size_t(state)/5;const int dir=int(state%5),x=int(cell%size_t(grid.cols)),y=int(cell/size_t(grid.cols));
                if(!tree.contains(cellKey(x,y))){bool target=false;for(int i=0;i<pins.size();i++)target|=!joined[i]&&pins[i]==Hole{x,y};if(target){reached=state;break;}}
                for(int d=0;d<4;d++){
                    const double bend=dir<4&&dir!=d?2:0;
                    auto relax=[&](int nx,int ny,double step){const size_t s=grid.index(nx,ny)*5+size_t(d);if(c+step<cost[s]){cost[s]=c+step;from[s]=state;queue.push({c+step,qint64(s)});}};
                    const int nx=x+dx[d],ny=y+dy[d];
                    if(grid.inside(nx,ny)&&free(nx,ny))relax(nx,ny,1+bend);
                    // A bridge over the wires of other nets, never over a pin.
                    bool over=false;
                    for(int k=2;k<=12;k++){
                        const int mx=x+dx[d]*(k-1),my=y+dy[d]*(k-1),lx=x+dx[d]*k,ly=y+dy[d]*k;
                        if(!grid.inside(mx,my)||grid.pin[grid.index(mx,my)])break;
                        over|=!free(mx,my);if(!grid.inside(lx,ly))break;
                        if(over&&free(lx,ly)&&!(grid.pin[grid.index(lx,ly)]&&grid.owner[grid.index(lx,ly)]!=n))relax(lx,ly,8+k+bend);
                    }
                }
            }
            if(reached<0){for(int i=0;i<pins.size();i++)if(!joined[i]){result.open++;if(result.openPins[n]<0)result.openPins[n]=i;}break;}
            // Back from the pin to the tree.
            for(qint64 s=reached;from[size_t(s)]>=0;s=from[size_t(s)]){
                const size_t a=size_t(s)/5,b=size_t(from[size_t(s)])/5;const Hole ha{int(a%size_t(grid.cols)),int(a/size_t(grid.cols))},hb{int(b%size_t(grid.cols)),int(b/size_t(grid.cols))};
                const bool bridge=std::abs(ha.x-hb.x)+std::abs(ha.y-hb.y)>1;result.edges[n]<<Edge{hb,ha,bridge};take(ha);take(hb);
            }
        }
    }
    for(const auto &edges:result.edges)for(const auto &e:edges){result.length+=(std::abs(e.a.x-e.b.x)+std::abs(e.a.y-e.b.y))*pitch/100;result.bridges+=e.bridge;}
    return result;
}

// The wires of a net: straight runs between holes where they bend, branch or meet a pin, so that every joint is the
// end of a wire.
QList<Edge> wiresOf(const QList<Edge> &edges,const QSet<qint64> &pins){
    QHash<qint64,QList<int>> at;for(int i=0;i<edges.size();i++){at[cellKey(edges[i].a.x,edges[i].a.y)]<<i;at[cellKey(edges[i].b.x,edges[i].b.y)]<<i;}
    auto other=[&](const Edge &e,Hole h){return e.a==h?e.b:e.a;};
    auto stop=[&](Hole h){
        const auto &list=at[cellKey(h.x,h.y)];if(list.size()!=2||pins.contains(cellKey(h.x,h.y)))return true;
        const Edge &e=edges[list[0]],&f=edges[list[1]];if(e.bridge||f.bridge)return true;
        const Hole p=other(e,h),q=other(f,h);return !(p.x==q.x||p.y==q.y);
    };
    QList<Edge> wires;QList<bool> used(edges.size(),false);
    for(int i=0;i<edges.size();i++){
        if(used[i])continue;
        if(edges[i].bridge){used[i]=true;wires<<edges[i];continue;}
        // Grow the run both ways along its line.
        Hole ends[2]={edges[i].a,edges[i].b};used[i]=true;
        for(int side=0;side<2;side++){
            while(!stop(ends[side])){
                int next=-1;for(int j:at[cellKey(ends[side].x,ends[side].y)])if(!used[j])next=j;if(next<0)break;
                used[next]=true;ends[side]=other(edges[next],ends[side]);
            }
        }
        wires<<Edge{ends[0],ends[1],false};
    }
    return wires;
}
}

Routing routeNets(const Project &board,const documents::Targets &targets){
    const auto built=build(board,targets,false);const Model &m=built.model;const State s=current(m);const Eval e=evaluate(m,s,false);
    Grid grid;grid.cols=m.cols;grid.rows=m.rows;grid.owner.assign(size_t(m.cols)*size_t(m.rows),-1);grid.pin.assign(grid.owner.size(),0);
    auto mark=[&](Hole h,int owner,bool pin){if(h.x<0||h.y<0||h.x>=grid.cols||h.y>=grid.rows)return;grid.owner[grid.index(h.x,h.y)]=owner;grid.pin[grid.index(h.x,h.y)]|=pin;};
    // Every pin on the board blocks its hole; those of arranged parts belong to their nets.
    for(const auto &c:board.components())if(c.board==board.activeBoard)for(auto at:pinPositions(board,c))mark(holeAt(at),-2,true);
    // Wires, leads and solder already on the board block the holes they run over.
    {
        auto line=[&](QPointF from,QPointF to){
            const Hole a=holeAt(from),b=holeAt(to);const int steps=qMax(std::abs(b.x-a.x),std::abs(b.y-a.y));
            for(int k=0;k<=steps;k++)mark({a.x+(b.x-a.x)*k/qMax(1,steps),a.y+(b.y-a.y)*k/qMax(1,steps)},-2,false);
        };
        QSet<QString> parts;for(const auto &c:board.components())if(c.board==board.activeBoard)parts.insert(keyOf(c.kind,c.index));
        for(const auto &p:board.placedObjects()){
            if(parts.contains(keyOf(p.kind,p.index)))continue;const auto &o=p.node;
            if(o["type"]=="wire")line({o["x"].toDouble(),o["y"].toDouble()},{o["x2"].toDouble(),o["y2"].toDouble()});
            else if(o["type"]=="solder")mark(holeAt({o["x"].toDouble(),o["y"].toDouble()}),-2,false);
            else if(o["type"]=="lead"){const QPointF at(o["x"].toDouble(),o["y"].toDouble());QPointF last=at;for(const auto &v:o["points"].toArray()){const QPointF q=at+QPointF(v.toArray()[0].toDouble(),v.toArray()[1].toDouble());line(last,q);last=q;}}
            std::function<void(const QJsonObject&)> walk=[&](const QJsonObject &n){
                for(const auto &c:n["children"].toArray())walk(c.toObject());
                if(n["type"]!="TDraht")return;const auto path=n["path"].toArray();
                for(int i=0;i+1<path.size()||(i==0&&path.size()==1);i++){
                    const QPointF a=p.transform.map(QPointF(path[i].toArray()[0].toDouble(),path[i].toArray()[1].toDouble()));
                    const QPointF b=path.size()>1?p.transform.map(QPointF(path[i+1].toArray()[0].toDouble(),path[i+1].toArray()[1].toDouble())):a;line(a,b);
                }
            };
            walk(o);
        }
    }
    QList<QList<Hole>> netPins(m.nets.size());QSet<qint64> pinHoles;
    for(int n=0;n<m.nets.size();n++)for(const auto &[item,pin]:m.nets[n]){
        const Hole h{s.at[item].x+m.items[item].pins[0][pin].x,s.at[item].y+m.items[item].pins[0][pin].y};
        if(!netPins[n].contains(h))netPins[n]<<h;mark(h,n,true);pinHoles.insert(cellKey(h.x,h.y));
    }
    Routing routing;
    // Pins of parts that are not arranged (off the grid) cannot be reached.
    QSet<QString> arranged;for(const auto &item:m.items)arranged.insert(item.uid);
    for(const auto &p:built.check.parts)if(!arranged.contains(p.part.uid))for(int pin:p.pins)if(pin>=0){routing.open++;routing.unrouted<<targets.components[p.target].designator+'.'+targets.components[p.target].pins[pin];}
    // A few orders of the nets; the one with the fewest open pins, then bridges, then length counts.
    QList<int> rails,others;for(int n=0;n<netPins.size();n++)(n==m.supply||n==m.ground?rails:others)<<n;
    auto span=[&](int n){int l=1<<30,r=-(1<<30),t=1<<30,b=-(1<<30);for(const auto &h:netPins[n]){l=qMin(l,h.x);r=qMax(r,h.x);t=qMin(t,h.y);b=qMax(b,h.y);}return netPins[n].isEmpty()?0:r-l+b-t;};
    std::stable_sort(others.begin(),others.end(),[&](int a,int b){return span(a)<span(b);});
    QList<int> longFirst=others;std::reverse(longFirst.begin(),longFirst.end());
    const QList<QList<int>> orders{rails+others,rails+longFirst,others+rails};
    Attempt best;bool any=false;
    for(const auto &order:orders){
        const auto a=route(grid,netPins,m.supply,m.ground,e.top,e.bottom,order);
        if(!any||std::tuple(a.open,a.bridges,a.length)<std::tuple(best.open,best.bridges,best.length)){best=a;any=true;}
    }
    routing.open+=best.open;routing.bridges=best.bridges;routing.length=best.length;
    for(int n=0;n<netPins.size();n++)if(best.openPins[n]>=0){
        for(const auto &[item,pin]:m.nets[n]){
            const Hole h{s.at[item].x+m.items[item].pins[0][pin].x,s.at[item].y+m.items[item].pins[0][pin].y};
            if(h==netPins[n][best.openPins[n]]){routing.unrouted<<m.items[item].designator+'.'+m.items[item].pinNames[pin];break;}
        }
    }
    // No solder dots: the continuity tester joins a dot with every wire over it, a bridge over another net's joint too.
    for(int n=0;n<best.edges.size();n++)for(const auto &w:wiresOf(best.edges[n],pinHoles)){
        QJsonObject wire{{"type","wire"},{"x",w.a.x*pitch},{"y",w.a.y*pitch},{"x2",w.b.x*pitch},{"y2",w.b.y*pitch}};if(w.bridge)wire["back"]=false;
        routing.objects<<wire;
    }
    return routing;
}

bool singleHoleGrid(const Project &board){
    // The copper of the template alone: without the parts and wires on it.
    Project bare=board;bare.additions=QJsonArray();
    if(bare.legacy.contains("objects")){bare.legacy["objects"]=QJsonArray();bare.edits={};bare.moves={};}
    auto model=continuityModel(bare);
    for(double x=pitch;x<board.width;x+=pitch)for(double y=pitch;y<board.height;y+=pitch){
        const auto net=model.netAt({x,y});if(net.isEmpty())return false;
        if(x+pitch<board.width&&model.netAt({x+pitch,y})==net)return false;
        if(y+pitch<board.height&&model.netAt({x,y+pitch})==net)return false;
    }
    return true;
}
}
