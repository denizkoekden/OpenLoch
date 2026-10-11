#include "placement.h"
#include "netcheck.h"
#include <QHash>
#include <QImage>
#include <QLineF>
#include <QMap>
#include <QPainter>
#include <QSet>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace openloch::pcb {
namespace {
constexpr double cell=.25;          // the raster of free room, in mm
bool copperLayer(int layer){return layer==CopperTop||layer==CopperBottom||layer==Inner1||layer==Inner2;}
bool padElement(const Element &e){return e.type==ElementType::Pad||e.type==ElementType::SmdPad;}
}

QList<QList<int>> arrangementNets(const Board &board,const documents::Targets *targets){
    QList<QList<int>> nets;
    if(targets&&!targets->isEmpty()){
        const auto check=checkNets(board,*targets);
        for(const auto &n:targets->nets){
            QList<int> pads;
            for(const auto &p:n.pins){const auto at=check.padsFor(*targets,p.component,p.pin);if(!at.isEmpty()&&!pads.contains(at.first()))pads<<at.first();}
            if(pads.size()>1)nets<<pads;
        }
        return nets;
    }
    const int n=int(board.elements.size());std::vector<int> root(n);std::iota(root.begin(),root.end(),0);std::vector<char> linked(n,0);
    auto find=[&](int x){while(root[x]!=x){root[x]=root[root[x]];x=root[x];}return x;};
    for(int i=0;i<n;i++)if(padElement(board.elements[i]))for(int j:board.elements[i].connections)
        if(j>=0&&j<n&&j!=i&&padElement(board.elements[j])){root[find(i)]=find(j);linked[i]=1;linked[j]=1;}
    QMap<int,QList<int>> groups;for(int i=0;i<n;i++)if(linked[i])groups[find(i)]<<i;
    for(const auto &g:groups)if(g.size()>1)nets<<g;
    return nets;
}

bool padWired(const Board &board,int pad){
    if(pad<0||pad>=board.elements.size())return false;const auto &p=board.elements[pad];
    const double reach=(p.type==ElementType::SmdPad?std::max(p.size,p.size2):p.size)/2;
    for(int i=0;i<board.elements.size();i++){
        const auto &e=board.elements[i];
        if(i==pad||!copperLayer(e.layer)||(p.part&&e.part==p.part))continue;
        if(e.type==ElementType::Track){
            if(e.autorouted&&(e.autoroutePads[0]==pad||e.autoroutePads[1]==pad))return true;
            for(const auto &q:e.points)if(QLineF(q,p.pos).length()<=reach+e.width/2+1e-9)return true;
        }else if(e.type==ElementType::Area&&e.points.size()>2&&QPolygonF(e.points).containsPoint(p.pos,Qt::OddEvenFill))return true;
    }
    return false;
}

namespace {
// The shortest tree through the points (Prim), its length; its lines added to `edges`.
double tree(const QList<QPointF> &points,QList<QLineF> *edges){
    const int n=int(points.size());if(n<2)return 0;
    std::vector<double> best(n,std::numeric_limits<double>::infinity());std::vector<int> from(n,-1);std::vector<char> in(n,0);
    best[0]=0;double total=0;
    for(int k=0;k<n;k++){
        int u=-1;for(int i=0;i<n;i++)if(!in[i]&&(u<0||best[i]<best[u]))u=i;
        in[u]=1;total+=best[u];if(from[u]>=0&&edges)*edges<<QLineF(points[from[u]],points[u]);
        for(int i=0;i<n;i++)if(!in[i]){const double d=QLineF(points[u],points[i]).length();if(d<best[i]){best[i]=d;from[i]=u;}}
    }
    return total;
}
// Two airwires cross where they meet inside both, not at an end of either.
bool crossing(const QLineF &a,const QLineF &b){
    QPointF at;if(a.intersects(b,&at)!=QLineF::BoundedIntersection)return false;
    auto end=[&at](const QLineF &l){return QLineF(at,l.p1()).length()<1e-6||QLineF(at,l.p2()).length()<1e-6;};
    return !end(a)&&!end(b);
}
int crossings(const QList<QLineF> &a,const QList<QLineF> &b){int n=0;for(const auto &x:a)for(const auto &y:b)n+=crossing(x,y);return n;}

struct Pin {int item=-1,pad=-1;QPointF at;};    // a pad of a movable component (by its place among its pads), else a fixed point
// Where a component lies: its anchor and whether it is turned by 90°.
struct Spot {QPointF at;int turn=0;bool operator==(const Spot &) const=default;};
struct Item {
    int designator=-1;
    QList<int> members,nets;
    QPointF anchor;                         // its first pad, or the middle of its elements without pads
    std::array<QRectF,2> box;               // its elements around the anchor, grown by half the gap, as it is and turned
    std::array<QList<QPointF>,2> offsets;   // its pads from the anchor, as it is and turned
};
// The elements of a component turned by 90° about its anchor, as "Drehen" turns them (texts with them).
QList<Element> turned(const Board &b,const Item &item){
    QList<Element> out;for(int m:item.members){Element e=b.elements[m];rotate(e,item.anchor,90);out<<e;}return out;
}

struct Arrangement {
    const Board &board;const ArrangeSettings settings;
    QList<Item> items;QList<QList<Pin>> nets;
    QList<Spot> at;QList<char> placed;bool everything=false;    // everything: unplaced components count where they lie
    QList<double> length;QList<QList<QLineF>> edges;            // per net, for the state as it is
    QRectF frame;int columns=0,rows=0;std::vector<int> blocked; // summed area table of the raster, (columns+1) × (rows+1)
    QPointF origin;double step=1.27;
    long evaluations=0;

    Arrangement(const Board &b,const QList<int> &designators,const QList<QList<int>> &netPads,const ArrangeSettings &s,ArrangeResult &result):board(b),settings(s){
        const auto all=components(b);QSet<int> moving;QHash<int,std::pair<int,int>> padOf;const double g=settings.gap/2;
        for(int d:designators){
            const Component *c=nullptr;for(const auto &x:all)if(x.designator==d)c=&x;
            if(!c||std::any_of(items.cbegin(),items.cend(),[d](const Item &i){return i.designator==d;}))continue;
            QList<int> pads;for(int m:c->members)if(padElement(b.elements[m]))pads<<m;
            if(std::any_of(pads.cbegin(),pads.cend(),[&](int p){return padWired(b,p);})){result.wired<<d;continue;}
            Item item;item.designator=d;item.members=c->members;QRectF r;for(int m:c->members)r=r.united(bounds(b.elements[m]));
            item.anchor=pads.isEmpty()?r.center():b.elements[pads.first()].pos;
            item.box[0]=r.translated(-item.anchor).adjusted(-g,-g,g,g);
            for(int k=0;k<pads.size();k++){item.offsets[0]<<b.elements[pads[k]].pos-item.anchor;padOf[pads[k]]={int(items.size()),k};}
            const auto turn=turned(b,item);QRectF t;
            for(int k=0;k<turn.size();k++){t=t.united(bounds(turn[k]));if(padElement(turn[k]))item.offsets[1]<<turn[k].pos-item.anchor;}
            item.box[1]=t.translated(-item.anchor).adjusted(-g,-g,g,g);
            for(int m:c->members)moving.insert(m);
            items<<item;
        }
        for(const auto &pads:netPads){
            QList<Pin> net;
            for(int p:pads){if(p<0||p>=b.elements.size())continue;const auto it=padOf.constFind(p);net<<(it!=padOf.cend()?Pin{it->first,it->second,{}}:Pin{-1,-1,b.elements[p].pos});}
            if(net.size()<2)continue;
            for(const auto &pin:net)if(pin.item>=0&&!items[pin.item].nets.contains(int(nets.size())))items[pin.item].nets<<int(nets.size());
            nets<<net;
        }
        for(const auto &i:items)at<<Spot{i.anchor,0};placed=QList<char>(items.size(),0);
        length=QList<double>(nets.size(),0);edges=QList<QList<QLineF>>(nets.size());
        // The raster: blocked outside the region and within half the gap of its edge, and where the rest of the board lies.
        const QPainterPath region=boardRegion(b);
        frame=region.boundingRect().adjusted(-2*cell,-2*cell,2*cell,2*cell);
        columns=std::max(1,int(std::ceil(frame.width()/cell)));rows=std::max(1,int(std::ceil(frame.height()/cell)));
        QImage raster(columns,rows,QImage::Format_Grayscale8);raster.fill(255);
        {QPainter p(&raster);p.setRenderHint(QPainter::Antialiasing,false);QTransform t;t.scale(1/cell,1/cell);t.translate(-frame.left(),-frame.top());p.setTransform(t);
            p.fillPath(region,Qt::black);p.strokePath(region,QPen(Qt::white,settings.gap+2*cell));
            const double grow=g+cell;
            QSet<int> inComponents;
            for(const auto &c:all){
                for(int m:c.members)inComponents.insert(m);
                if(moving.contains(c.designator))continue;
                QRectF r;for(int m:c.members)r=r.united(bounds(b.elements[m]));p.fillRect(r.adjusted(-grow,-grow,grow,grow),Qt::white);
            }
            for(int i=0;i<b.elements.size();i++){
                const auto &e=b.elements[i];
                if(inComponents.contains(i)||e.layer==Outline||e.type==ElementType::Track||e.type==ElementType::Area)continue;
                p.fillRect(bounds(e).adjusted(-grow,-grow,grow,grow),Qt::white);
            }}
        blocked.assign(size_t(columns+1)*size_t(rows+1),0);
        for(int y=0;y<rows;y++){const uchar *line=raster.constScanLine(y);int run=0;
            for(int x=0;x<columns;x++){run+=line[x]>127;blocked[size_t(y+1)*(columns+1)+x+1]=blocked[size_t(y)*(columns+1)+x+1]+run;}}
        origin=b.origin;const double unit=b.grid>.05?b.grid:1.27;
        step=unit*std::max(1.0,std::ceil(std::sqrt(frame.width()*frame.height()/(unit*unit*6000))));
    }

    QRectF box(int i,const Spot &s) const{return items[i].box[s.turn].translated(s.at);}
    QPointF where(const Pin &p) const{if(p.item<0)return p.at;const Spot &s=at[p.item];return s.at+items[p.item].offsets[s.turn][p.pad];}
    bool active(const Pin &p) const{return p.item<0||placed[p.item]||everything;}
    double net(int n,QList<QLineF> *lines) const{QList<QPointF> points;for(const auto &p:nets[n])if(active(p))points<<where(p);return tree(points,lines);}
    double cost() const{double c=0;for(double l:length)c+=l;return c;}
    int crossed() const{int c=0;for(int a=0;a<nets.size();a++)for(int b=a+1;b<nets.size();b++)c+=crossings(edges[a],edges[b]);return c;}
    void measure(){for(int n=0;n<nets.size();n++){edges[n].clear();length[n]=net(n,&edges[n]);}}

    bool room(const QRectF &r) const{
        const int x0=int(std::floor((r.left()-frame.left())/cell)),x1=int(std::ceil((r.right()-frame.left())/cell));
        const int y0=int(std::floor((r.top()-frame.top())/cell)),y1=int(std::ceil((r.bottom()-frame.top())/cell));
        if(x0<0||y0<0||x1>columns||y1>rows)return false;
        auto s=[&](int x,int y){return blocked[size_t(y)*(columns+1)+x];};
        return s(x1,y1)-s(x0,y1)-s(x1,y0)+s(x0,y0)==0;
    }
    bool fits(int i,const Spot &spot,int other=-1) const{
        const QRectF r=box(i,spot);if(!room(r))return false;
        for(int j=0;j<items.size();j++)if(j!=i&&j!=other&&placed[j]&&box(j,at[j]).intersects(r))return false;
        return true;
    }
    QPointF snap(QPointF p) const{return origin+QPointF(std::round((p.x()-origin.x())/step)*step,std::round((p.y()-origin.y())/step)*step);}
    // Spots where component i fits, its box centred as near `centre` as the lattice allows, ring by ring outwards; each
    // ring as it is and turned.
    QList<Spot> around(int i,QPointF centre,int count,int other=-1) const{
        QList<Spot> out;const int turns=settings.rotate?2:1;
        const int reach=int(std::ceil(std::max(frame.width(),frame.height())/step))+1;
        for(int r=0;r<=reach&&out.size()<count;r++)for(int t=0;t<turns;t++){
            const QPointF start=snap(centre-items[i].box[t].center());
            for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++){
                if(std::max(std::abs(dx),std::abs(dy))!=r)continue;
                const Spot s{start+QPointF(dx*step,dy*step),t};if(fits(i,s,other))out<<s;
            }
        }
        return out;
    }
    // The cost of moving components to spots (placing them, if they were not): the change of length and of crossings
    // of their nets. The state stays as it was.
    std::pair<double,int> change(const QList<int> &moved,const QList<Spot> &to,bool counting){
        evaluations++;
        QList<int> touched;for(int i:moved)for(int n:items[i].nets)if(!touched.contains(n))touched<<n;
        const auto oldAt=at;const auto oldPlaced=placed;
        for(int k=0;k<moved.size();k++){at[moved[k]]=to[k];placed[moved[k]]=1;}
        double delta=0;QList<QList<QLineF>> lines(touched.size());
        for(int k=0;k<touched.size();k++)delta+=net(touched[k],&lines[k])-length[touched[k]];
        at=oldAt;placed=oldPlaced;
        if(!counting)return {delta,0};
        auto among=[&](const QList<QList<QLineF>> &mine){
            int c=0;
            for(int k=0;k<touched.size();k++){
                for(int n=0;n<nets.size();n++)if(!touched.contains(n))c+=crossings(mine[k],edges[n]);
                for(int m=k+1;m<touched.size();m++)c+=crossings(mine[k],mine[m]);
            }
            return c;};
        QList<QList<QLineF>> before;for(int n:touched)before<<edges[n];
        return {delta,among(lines)-among(before)};
    }
    void commit(const QList<int> &moved,const QList<Spot> &to){
        for(int k=0;k<moved.size();k++){at[moved[k]]=to[k];placed[moved[k]]=1;}
        for(int i:moved)for(int n:items[i].nets){edges[n].clear();length[n]=net(n,&edges[n]);}
    }
    // The best of the spots for component i: the shortest few by length, then crossings counted.
    std::pair<int,double> best(int i,const QList<Spot> &spots){
        QList<std::pair<double,int>> byLength;
        for(int k=0;k<spots.size();k++)byLength<<std::pair{change({i},{spots[k]},false).first,k};
        std::stable_sort(byLength.begin(),byLength.end(),[](const auto &a,const auto &b){return a.first<b.first;});
        int chosen=-1;double lowest=std::numeric_limits<double>::infinity();
        for(int k=0;k<std::min<int>(12,int(byLength.size()));k++){
            const auto [d,c]=change({i},{spots[byLength[k].second]},true);const double total=d+settings.crossing*c;
            if(total<lowest-1e-9){lowest=total;chosen=byLength[k].second;}
        }
        return {chosen,lowest};
    }
    // Where the other pads of component i's nets lie, on average; false when none is placed.
    bool pull(int i,QPointF &centre) const{
        QPointF sum;int count=0;
        for(int n:items[i].nets)for(const auto &p:nets[n])if(p.item!=i&&active(p)){sum+=where(p);count++;}
        if(!count)return false;centre=sum/count;return true;
    }
};
}

ArrangeResult arrangeParts(Board &board,const QList<int> &designators,const QList<QList<int>> &nets,const ArrangeSettings &settings,const std::function<void(int,int)> &progress){
    ArrangeResult result;Arrangement a(board,designators,nets,settings,result);
    const int n=int(a.items.size());constexpr int passes=8;
    a.everything=true;a.measure();result.lengthBefore=a.cost();result.crossingsBefore=a.crossed();a.everything=false;a.measure();
    auto report=[&](int done){if(progress)progress(done,n*(passes+1));};
    // First placement: the component with the most nets to what lies already first, each where its nets are shortest.
    for(int done=0;done<n;done++){
        int next=-1,bestLinks=-1;
        auto area=[&](int k){return a.items[k].box[0].width()*a.items[k].box[0].height();};
        for(int i=0;i<n;i++){
            if(a.placed[i]||result.left.contains(a.items[i].designator))continue;
            int links=0;for(int m:a.items[i].nets)for(const auto &p:a.nets[m])if(p.item!=i&&a.active(p)){links++;break;}
            if(next<0||links>bestLinks||(links==bestLinks&&(a.items[i].nets.size()>a.items[next].nets.size()||(a.items[i].nets.size()==a.items[next].nets.size()&&area(i)>area(next)+1e-9)))){next=i;bestLinks=links;}
        }
        if(next<0)break;
        QPointF centre=a.frame.center();a.pull(next,centre);
        const auto spots=a.around(next,centre,160);
        if(spots.isEmpty()){result.left<<a.items[next].designator;report(done+1);continue;}
        const auto [chosen,total]=a.best(next,spots);Q_UNUSED(total);
        a.commit({next},{spots[chosen]});report(done+1);
    }
    // Moves and swaps while they help.
    for(int pass=0;pass<passes&&a.evaluations<2000000;pass++){
        bool better=false;
        for(int i=0;i<n;i++){
            QPointF centre;if(!a.placed[i]||!a.pull(i,centre))continue;
            const auto spots=a.around(i,centre,120);if(spots.isEmpty())continue;
            const auto [chosen,total]=a.best(i,spots);
            if(chosen>=0&&total<-1e-6){a.commit({i},{spots[chosen]});better=true;}
            report(n*(pass+1)+i);
        }
        for(int i=0;i<n;i++)for(int j=i+1;j<n;j++){
            if(!a.placed[i]||!a.placed[j])continue;
            const QPointF ci=a.box(i,a.at[i]).center(),cj=a.box(j,a.at[j]).center();
            for(int ti=0;ti<(settings.rotate?2:1);ti++)for(int tj=0;tj<(settings.rotate?2:1);tj++){
                const Spot si{a.snap(cj-a.items[i].box[ti].center()),ti},sj{a.snap(ci-a.items[j].box[tj].center()),tj};
                if(!a.fits(i,si,j)||!a.fits(j,sj,i)||a.box(i,si).intersects(a.box(j,sj)))continue;
                const auto [d,c]=a.change({i,j},{si,sj},true);
                if(d+settings.crossing*c<-1e-6){a.commit({i,j},{si,sj});better=true;break;}
            }
        }
        if(!better)break;
    }
    report(n*(passes+1));
    for(int i=0;i<n;i++){
        if(!a.placed[i])continue;
        const auto &item=a.items[i];const Spot &s=a.at[i];
        for(int m:item.members){if(s.turn)rotate(board.elements[m],item.anchor,90);move(board.elements[m],s.at-item.anchor);}
        result.placed<<item.designator;
    }
    a.everything=true;a.measure();result.lengthAfter=a.cost();result.crossingsAfter=a.crossed();
    return result;
}
}
