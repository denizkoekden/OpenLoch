#include "router.h"
#include "copper.h"
#include "netcheck.h"
#include <QHash>
#include <QImage>
#include <QLineF>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <vector>

namespace openloch::pcb {
namespace {
bool padElement(const Element &e){return e.type==ElementType::Pad||e.type==ElementType::SmdPad;}
bool meet(QPointF a,QPointF b){return QLineF(a,b).length()<.01;}
double treeLength(const QList<QPointF> &points){
    const int n=int(points.size());if(n<2)return 0;
    std::vector<double> best(n,std::numeric_limits<double>::infinity());std::vector<char> in(n,0);best[0]=0;double total=0;
    for(int k=0;k<n;k++){
        int u=-1;for(int i=0;i<n;i++)if(!in[i]&&(u<0||best[i]<best[u]))u=i;
        in[u]=1;total+=best[u];
        for(int i=0;i<n;i++)if(!in[i])best[i]=std::min(best[i],QLineF(points[u],points[i]).length());
    }
    return total;
}
// 0 ground, 1 supply, 2 any other net.
int netClass(const QString &name){
    static const QRegularExpression ground(QStringLiteral("^([ADP]?GND\\d*|GROUND|MASSE|VSS|0V)$"),QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression supply(QStringLiteral("^(VCC|VDD|VEE|VBAT|VIN|VPP|VS|UB)\\w*$|^[+-]|^\\d+([.,]\\d+)?V\\d*$|^\\d+V\\d+$"),
                                           QRegularExpression::CaseInsensitiveOption);
    const QString n=name.trimmed();
    if(ground.match(n).hasMatch())return 0;
    if(!n.isEmpty()&&supply.match(n).hasMatch())return 1;
    return 2;
}
}

QList<RouteNet> routingNets(const Board &board,const documents::Targets *targets){
    QList<RouteNet> nets;
    if(targets&&!targets->isEmpty()){
        const auto check=checkNets(board,*targets);
        for(const auto &n:targets->nets){
            RouteNet net{n.name,{}};
            for(const auto &p:n.pins)for(int pad:check.padsFor(*targets,p.component,p.pin))if(!net.pads.contains(pad))net.pads<<pad;
            if(net.pads.size()>1)nets<<net;
        }
        return nets;
    }
    const int n=int(board.elements.size());std::vector<int> root(n);std::iota(root.begin(),root.end(),0);std::vector<char> linked(n,0);
    auto find=[&](int x){while(root[x]!=x){root[x]=root[root[x]];x=root[x];}return x;};
    for(int i=0;i<n;i++)if(padElement(board.elements[i]))for(int j:board.elements[i].connections)
        if(j>=0&&j<n&&j!=i&&padElement(board.elements[j])){root[find(i)]=find(j);linked[i]=1;linked[j]=1;}
    QMap<int,QList<int>> groups;for(int i=0;i<n;i++)if(linked[i])groups[find(i)]<<i;
    for(const auto &g:groups)if(g.size()>1)nets<<RouteNet{QString(),g};
    return nets;
}
QList<int> routingOrder(const Board &board,const QList<RouteNet> &nets){
    QList<int> order(nets.size());std::iota(order.begin(),order.end(),0);QList<double> length;
    for(const auto &n:nets){QList<QPointF> points;for(int p:n.pads)if(p>=0&&p<board.elements.size())points<<board.elements[p].pos;length<<treeLength(points);}
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){
        const int ca=netClass(nets[a].name),cb=netClass(nets[b].name);if(ca!=cb)return ca<cb;return length[a]<length[b];});
    return order;
}

namespace {
constexpr int freeCell=-1,blockedCell=-2;
class Router {
public:
    Router(Board &board,const QList<RouteNet> &nets,const RouteSettings &settings):b(board),nets(nets),s(settings){
        layers={CopperBottom};if(s.twoLayers)layers<<CopperTop;
        step=std::max(.05,s.step);while((b.width/step+1)*(b.height/step+1)>4e5)step*=2;
        w=int(std::floor(b.width/step+1e-9))+1;h=int(std::floor(b.height/step+1e-9))+1;
        owner.assign(size_t(layers.size())*w*h,freeCell);viaBlocked.assign(size_t(w)*h,0);
        margin=step/2;
        // Which net each piece of copper belongs to: the net of a pad it joins, else none (it blocks every net).
        const auto parts=connections(b,false);QHash<int,int> partNet;
        for(int k=0;k<nets.size();k++)for(int p:nets[k].pads)if(p>=0&&p<parts.size()&&parts[p]>=0&&!partNet.contains(parts[p]))partNet[parts[p]]=k;
        netOf.assign(b.elements.size(),blockedCell);
        for(int i=0;i<b.elements.size();i++)if(i<parts.size()&&parts[i]>=0)netOf[i]=partNet.value(parts[i],blockedCell);
        // Outside the region and within half a track of its edge: blocked on every layer.
        {const QPainterPath region=boardRegion(b);QImage inside(w,h,QImage::Format_Grayscale8);inside.fill(0);
            {QPainter p(&inside);p.setRenderHint(QPainter::Antialiasing,false);p.translate(.5,.5);p.scale(1/step,1/step);
                p.fillPath(region,Qt::white);p.strokePath(region,QPen(Qt::black,2*(s.width/2+margin)));}
            for(int y=0;y<h;y++){const uchar *line=inside.constScanLine(y);for(int x=0;x<w;x++)if(line[x]<128)for(int l=0;l<layers.size();l++)owner[cell(l,x,y)]=blockedCell;}}
        const double grow=s.clearance+s.width/2+margin;
        for(int i=0;i<b.elements.size();i++){
            const auto &e=b.elements[i];
            for(int l=0;l<layers.size();l++){const auto copper=copperOn(e,layers[l],b);if(!copper.isEmpty())claim(l,copper,grow,netOf[i]);}
            if(e.type==ElementType::Pad&&e.size2>0){
                QPainterPath hole;hole.addEllipse(e.pos,e.size2/2,e.size2/2);for(int l=0;l<layers.size();l++)claim(l,hole,grow,netOf[i]);}
            // No via near a pad or a hole of any net.
            if(padElement(e)||(e.type==ElementType::Pad&&e.size2>0)){
                QPainterPath shape=copperShape(e);if(e.type==ElementType::Pad&&e.size2>0)shape.addEllipse(e.pos,e.size2/2,e.size2/2);
                cells(shape,s.clearance+s.viaDiameter/2+margin,[&](int x,int y){viaBlocked[size_t(y)*w+x]=1;});
            }
        }
        // The cells around a via's centre that must be free besides its centre.
        const int r=int(std::ceil((s.viaDiameter/2-s.width/2+margin)/step));
        for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++)if(std::hypot(dx,dy)*step<=s.viaDiameter/2-s.width/2+margin)viaRing<<QPoint(dx,dy);
    }
    RouteResult run(const QList<int> &order,const std::function<void(int)> &tick){
        RouteResult result;
        const auto parts=connections(b,false);
        for(int k:order){
            const auto &net=nets[k];
            // Islands: pads joined by copper already.
            QMap<int,QList<int>> byPart;for(int p:net.pads)if(p>=0&&p<b.elements.size())byPart[p<parts.size()&&parts[p]>=0?parts[p]:-1-p]<<p;
            QList<QList<int>> islands=byPart.values();
            std::stable_sort(islands.begin(),islands.end(),[](const QList<int> &a,const QList<int> &c){return a.size()>c.size();});
            if(islands.size()<2)continue;
            result.connections+=int(islands.size())-1;
            QList<int> joined=islands.takeFirst();std::vector<int> sources;
            for(int p:joined)padCells(k,p,sources);
            while(!islands.isEmpty()){
                tick(1);
                QHash<int,std::pair<int,int>> targets;QList<QPointF> aims;     // cell → island, pad
                for(int i=0;i<islands.size();i++)for(int p:islands[i]){std::vector<int> c;padCells(k,p,c);for(int x:c)targets.insert(x,{i,p});aims<<b.elements[p].pos;}
                const auto path=search(k,sources,targets,aims);
                if(path.isEmpty()){
                    for(const auto &island:islands){
                        int from=-1,to=island.first();double best=std::numeric_limits<double>::infinity();
                        for(int p:joined)for(int q:island){const double d=QLineF(b.elements[p].pos,b.elements[q].pos).length();if(d<best){best=d;from=p;to=q;}}
                        result.open<<RouteResult::Open{from,to,net.name};
                    }
                    break;
                }
                const auto [island,pad]=targets.value(path.last());
                int from=-1;double best=std::numeric_limits<double>::infinity();const QPointF start=point(path.first());
                for(int p:joined){const double d=QLineF(b.elements[p].pos,start).length();if(d<best){best=d;from=p;}}
                lay(k,path,from,pad,result,sources);
                result.routed++;
                for(int p:islands[island]){joined<<p;padCells(k,p,sources);}
                islands.removeAt(island);
            }
        }
        return result;
    }

private:
    Board &b;const QList<RouteNet> &nets;const RouteSettings s;
    QList<int> layers;double step=.4,margin=.2;int w=0,h=0;
    std::vector<int> owner;std::vector<char> viaBlocked;std::vector<int> netOf;QList<QPoint> viaRing;

    size_t cell(int l,int x,int y) const{return (size_t(l)*h+y)*w+x;}
    QPointF point(int c) const{const int x=c%w,y=(c/w)%h;return QPointF(x*step,y*step);}
    bool open(int net,int l,int x,int y) const{if(x<0||y<0||x>=w||y>=h)return false;const int o=owner[cell(l,x,y)];return o==freeCell||o==net;}
    // Calls f for every raster point within `grow` of the shape.
    void cells(const QPainterPath &shape,double grow,const std::function<void(int,int)> &f) const{
        const QRectF r=shape.boundingRect().adjusted(-grow,-grow,grow,grow);
        const int x0=std::max(0,int(std::floor(r.left()/step))),y0=std::max(0,int(std::floor(r.top()/step)));
        const int x1=std::min(w-1,int(std::ceil(r.right()/step))),y1=std::min(h-1,int(std::ceil(r.bottom()/step)));
        if(x1<x0||y1<y0)return;
        QImage mask(x1-x0+1,y1-y0+1,QImage::Format_Grayscale8);mask.fill(0);
        {QPainter p(&mask);p.setRenderHint(QPainter::Antialiasing,false);p.translate(.5-x0,.5-y0);p.scale(1/step,1/step);
            p.setPen(grow>0?QPen(Qt::white,2*grow,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin):QPen(Qt::NoPen));p.setBrush(Qt::white);p.drawPath(shape);}
        for(int y=0;y<mask.height();y++){const uchar *line=mask.constScanLine(y);for(int x=0;x<mask.width();x++)if(line[x]>127)f(x0+x,y0+y);}
    }
    void claim(int l,const QPainterPath &shape,double grow,int net){
        cells(shape,grow,[&](int x,int y){int &o=owner[cell(l,x,y)];if(o==freeCell)o=net;else if(o!=net)o=blockedCell;});
    }
    // The raster points of a pad's copper free for its net, on every layer it has copper on.
    void padCells(int net,int pad,std::vector<int> &out) const{
        const auto &e=b.elements[pad];
        for(int l=0;l<layers.size();l++){
            const auto copper=copperOn(e,layers[l],b);if(copper.isEmpty())continue;
            const QRectF r=copper.boundingRect();
            for(int y=std::max(0,int(std::ceil(r.top()/step)));y<=std::min(h-1,int(std::floor(r.bottom()/step)));y++)
                for(int x=std::max(0,int(std::ceil(r.left()/step)));x<=std::min(w-1,int(std::floor(r.right()/step)));x++)
                    if(copper.contains(QPointF(x*step,y*step))&&open(net,l,x,y))out.push_back(int(cell(l,x,y)));
        }
    }
    bool viaFits(int net,int x,int y) const{
        if(viaBlocked[size_t(y)*w+x])return false;
        for(int l=0;l<layers.size();l++)for(const QPoint &d:viaRing)if(!open(net,l,x+d.x(),y+d.y()))return false;
        return true;
    }
    // The cheapest way from any source to any target: A* over raster point and layer; a step costs its length, on the
    // top layer half as much again, a bend 0.4 (45°) or 1.2 (90°), a via 8 mm; no sharper bends, no cut corners.
    QList<int> search(int net,const std::vector<int> &sources,const QHash<int,std::pair<int,int>> &targets,const QList<QPointF> &aims) const{
        static const int dx[8]={1,1,0,-1,-1,-1,0,1},dy[8]={0,1,1,1,0,-1,-1,-1};
        const size_t n=size_t(layers.size())*w*h;std::vector<float> cost(n,std::numeric_limits<float>::infinity());std::vector<int> previous(n,-1);std::vector<signed char> heading(n,8);
        const float viaCost=float(8/step),top=1.5f;
        auto guess=[&](int x,int y){double best=std::numeric_limits<double>::infinity();
            for(const auto &a:aims){const double ax=std::abs(x-a.x()/step),ay=std::abs(y-a.y()/step);best=std::min(best,std::max(ax,ay)+(M_SQRT2-1)*std::min(ax,ay));}
            return float(std::max(0.0,best-1));};
        using Entry=std::pair<float,int>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;
        for(int c:sources){if(cost[size_t(c)]==0)continue;cost[size_t(c)]=0;const int x=c%w,y=(c/w)%h;queue.push({guess(x,y),c});}
        int found=-1;
        while(!queue.empty()){
            const auto [f,c]=queue.top();queue.pop();const int x=c%w,y=(c/w)%h,l=c/(w*h);
            if(f>cost[size_t(c)]+guess(x,y)+1e-3f)continue;
            if(targets.contains(c)){found=c;break;}
            const int d=heading[size_t(c)];
            for(int k=0;k<8;k++){
                const int nx=x+dx[k],ny=y+dy[k];if(!open(net,l,nx,ny))continue;
                if(dx[k]&&dy[k]&&(!open(net,l,x+dx[k],y)||!open(net,l,x,y+dy[k])))continue;
                const int turn=d==8?0:std::min((k-d+8)%8,(d-k+8)%8);if(turn>2)continue;
                const float stepCost=(dx[k]&&dy[k]?float(M_SQRT2):1.f)*(layers[l]==CopperTop?top:1.f)+(turn==0?0.f:turn==1?.4f:1.2f);
                const int m=int(cell(l,nx,ny));const float total=cost[size_t(c)]+stepCost;
                if(total<cost[size_t(m)]){cost[size_t(m)]=total;previous[size_t(m)]=c;heading[size_t(m)]=static_cast<signed char>(k);queue.push({total+guess(nx,ny),m});}
            }
            if(layers.size()>1&&viaFits(net,x,y)){
                const int m=int(cell(1-l,x,y));const float total=cost[size_t(c)]+viaCost;
                if(total<cost[size_t(m)]){cost[size_t(m)]=total;previous[size_t(m)]=c;heading[size_t(m)]=8;queue.push({total+guess(x,y),m});}
            }
        }
        QList<int> path;for(int c=found;c>=0;c=previous[size_t(c)])path.prepend(c);
        return path;
    }
    // The way as tracks, a via at each change of layer; its ends at the centres of the pads it starts or ends on.
    void lay(int net,const QList<int> &path,int from,int to,RouteResult &result,std::vector<int> &sources){
        auto layerOf=[&](int c){return c/(w*h);};
        auto padAt=[&](int c,int pad){const int l=layerOf(c);return pad>=0&&copperOn(b.elements[pad],layers[l],b).contains(point(c));};
        int begin=0;
        for(int k=1;k<=path.size();k++){
            if(k<path.size()&&layerOf(path[k])==layerOf(path[k-1]))continue;
            // path[begin .. k-1] lie on one layer.
            QPolygonF line;for(int i=begin;i<k;i++)line<<point(path[i]);
            QPolygonF corners;for(int i=0;i<line.size();i++){
                if(i>0&&i+1<line.size()){const QPointF a=line[i]-line[i-1],c=line[i+1]-line[i];if(std::abs(a.x()*c.y()-a.y()*c.x())<1e-9&&a.x()*c.x()+a.y()*c.y()>0)continue;}
                corners<<line[i];}
            if(begin==0&&padAt(path.first(),from))corners.prepend(b.elements[from].pos);
            if(k==path.size()&&padAt(path.last(),to))corners<<b.elements[to].pos;
            if(corners.size()>=2&&!(corners.size()==2&&meet(corners[0],corners[1]))){
                Element track=newElement(ElementType::Track);track.layer=layers[layerOf(path[begin])];track.width=s.width;track.points=corners;
                track.autorouted=true;track.autoroutePads={from,to};track.clearance=s.padClearance;
                b.elements<<track;result.added<<int(b.elements.size())-1;
                claimLine(layerOf(path[begin]),corners,net);
            }
            if(k<path.size()){
                const QPointF at=point(path[k]);Element via=newElement(ElementType::Pad);via.pos=at;via.size=s.viaDiameter;via.size2=s.viaDrill;via.via=true;
                via.layer=CopperBottom;via.clearance=s.padClearance;updateOutline(via);b.elements<<via;result.added<<int(b.elements.size())-1;result.vias++;
                QPainterPath ring;ring.addEllipse(at,s.viaDiameter/2,s.viaDiameter/2);
                for(int l=0;l<layers.size();l++)claim(l,ring,s.clearance+s.width/2+margin,net);
                cells(ring,s.clearance+s.viaDiameter/2+margin,[&](int x,int y){viaBlocked[size_t(y)*w+x]=1;});
            }
            begin=k;
        }
        for(int c:path)sources.push_back(c);
    }
    void claimLine(int l,const QPolygonF &line,int net){
        QPainterPath path;path.moveTo(line.first());for(int i=1;i<line.size();i++)path.lineTo(line[i]);
        const double grow=s.width+s.clearance+margin;
        cells(path,grow,[&](int x,int y){int &o=owner[cell(l,x,y)];if(o==freeCell)o=net;else if(o!=net)o=blockedCell;});
    }
};
// The open ones of `nets` first, the others after them in `order`.
QList<int> openFirst(const QList<int> &order,const RouteResult &tried,const QList<RouteNet> &nets){
    QList<int> first,rest;
    for(int k:order){bool open=false;for(const auto &o:tried.open)open=open||nets[k].pads.contains(o.from)||nets[k].pads.contains(o.to);(open?first:rest)<<k;}
    return first+rest;
}
}

RouteResult routeAll(Board &board,const QList<RouteNet> &nets,const RouteSettings &settings,const std::function<void(int,int)> &progress){
    const QList<int> order=routingOrder(board,nets);
    constexpr int tries=4;
    int total=0;for(const auto &n:nets)total+=std::max(0,int(n.pads.size())-1);total=std::max(1,tries*total);int done=0;
    auto tick=[&](int k){done+=k;if(progress)progress(std::min(done,total),total);};
    Board first=board;RouteResult result=Router(first,nets,settings).run(order,tick);
    QList<int> next=order;RouteResult last=result;
    for(int t=1;t<tries&&!last.open.isEmpty();t++){
        next=openFirst(next,last,nets);Board other=board;last=Router(other,nets,settings).run(next,tick);
        if(last.open.size()<result.open.size()||(last.open.size()==result.open.size()&&last.vias<result.vias)){result=last;first=other;}
    }
    if(progress)progress(total,total);
    board=first;
    // Stored airwires the copper joins now are gone.
    const auto joined=connections(board,false);
    for(int i=0;i<board.elements.size();i++){
        auto &list=board.elements[i].connections;
        list.removeIf([&](int t){return t>=0&&t<joined.size()&&joined[i]>=0&&joined[i]==joined[t];});
    }
    return result;
}
}
