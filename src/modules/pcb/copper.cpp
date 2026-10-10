#include "copper.h"
#include "font.h"
#include "language.h"
#include <QImage>
#include <QPainter>
#include <QPainterPathStroker>
#include <QTransform>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <vector>

namespace openloch::pcb {
namespace {
// Boolean operations on paths flatten curves to about half a unit, far too coarse in millimetres: they run on paths
// scaled to hundredths of a millimetre, where the error stays below a few micrometres.
constexpr double fine=100;
QPainterPath scaled(const QPainterPath &path,double factor){return path.isEmpty()?path:QTransform::fromScale(factor,factor).map(path);}
// A shape grown by `distance` on all sides (rounded corners).
QPainterPath grown(const QPainterPath &shape,double distance){
    if(distance<=0)return shape;
    QPainterPathStroker s;s.setWidth(2*distance);s.setJoinStyle(Qt::RoundJoin);s.setCapStyle(Qt::RoundCap);
    return shape.united(s.createStroke(shape));
}
struct UnionFind {
    QList<int> parent;
    explicit UnionFind(int n):parent(n){std::iota(parent.begin(),parent.end(),0);}
    int find(int i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;}
    void join(int a,int b){a=find(a);b=find(b);if(a!=b)parent[std::max(a,b)]=std::min(a,b);}
};
// Copper shapes of all elements per copper layer, with their bounding boxes, in fine units.
struct Shapes {
    QList<int> layers;
    QList<QList<QPainterPath>> paths;   // [layer index][element]
    QList<QList<QRectF>> boxes;
    Shapes(const Board &board):layers(copperLayers(board)){
        for(int layer:layers){
            QList<QPainterPath> p;QList<QRectF> b;
            for(const auto &e:board.elements){auto shape=scaled(copperOn(e,layer,board),fine);b.append(shape.isEmpty()?QRectF():shape.boundingRect());p.append(shape);}
            paths.append(p);boxes.append(b);
        }
    }
};
}

QList<int> copperLayers(const Board &board){
    return board.multilayer?QList<int>{CopperTop,Inner1,Inner2,CopperBottom}:QList<int>{CopperTop,CopperBottom};
}
QPainterPath copperOn(const Element &e,int layer,const Board &board){
    if(!isCopper(layer))return {};
    const bool here=e.layer==layer||(e.type==ElementType::Pad&&e.via&&copperLayers(board).contains(layer));
    if(!here||e.cutout||e.maskOnly||(e.type==ElementType::Text&&!e.visible&&e.role!=TextRole::Plain))return {};
    // Plain drill holes (as wide as the pad) carry no copper.
    if(e.type==ElementType::Pad&&e.size2>=e.size-1e-9)return {};
    return copperShape(e);
}
QPainterPath thermalSpokes(const Element &pad,int layer){
    QPainterPath out;if(!pad.thermal||(pad.type!=ElementType::Pad&&pad.type!=ElementType::SmdPad))return out;
    // One byte of spokes per copper layer (K1, K2, I1, I2) when the pad has spokes per layer, else the first byte for
    // all layers; bit 0 at twelve o'clock, then clockwise in steps of 45°.
    const int byte=pad.thermalPerLayer&&pad.type==ElementType::Pad?(layer==CopperTop?0:layer==CopperBottom?1:layer==Inner1?2:3):0;const int bits=int((pad.thermalSpokes>>(8*byte))&0xff);
    const double size=pad.type==ElementType::Pad?pad.size:std::min(pad.size,pad.size2);
    const double width=size/3*pad.thermalWidth/100,reach=std::max(pad.size,pad.size2)/2+pad.clearance+width;
    for(int i=0;i<8;i++){
        if(!(bits&(1<<i)))continue;
        const double a=qDegreesToRadians(90-45.0*i+pad.rotation);const QPointF d(std::cos(a),-std::sin(a)),n(-d.y(),d.x());
        QPolygonF bar{pad.pos+n*width/2,pad.pos+d*reach+n*width/2,pad.pos+d*reach-n*width/2,pad.pos-n*width/2};
        out.addPolygon(bar);out.closeSubpath();
    }
    return out;
}
QPainterPath thermalBridges(const Element &pad,int layer,const Board &board){
    if(!pad.thermal||pad.clearance<=0)return {};const auto copper=scaled(copperOn(pad,layer,board),fine);if(copper.isEmpty())return {};
    return scaled(scaled(thermalSpokes(pad,layer),fine).intersected(grown(copper,pad.clearance*fine)),1/fine);
}
QPainterPath grownShape(const QPainterPath &shape,double distance){return scaled(grown(scaled(shape,fine),distance*fine),1/fine);}
QPainterPath groundPlane(const Board &board,int layer){
    if(!isCopper(layer)||!board.groundPlane[layer]||!copperLayers(board).contains(layer))return {};
    QPainterPath cut;cut.setFillRule(Qt::WindingFill);QPainterPath spokes;
    for(const auto &e:board.elements){
        const auto copper=scaled(copperOn(e,layer,board),fine);
        // Hatched areas keep the plane out of their whole outline, also at clearance 0: it never fills their gaps.
        if(!copper.isEmpty()&&(e.clearance>0||(e.type==ElementType::Area&&e.hatched))){
            cut=cut.united(grown(copper,std::max(0.0,e.clearance)*fine));
            if(e.thermal)spokes=spokes.united(scaled(thermalBridges(e,layer,board),fine));
        }
        if(e.cutout&&e.layer==layer)cut=cut.united(scaled(copperShape(e),fine));
        if(e.type==ElementType::Pad&&e.size2>0){QPainterPath hole;hole.addEllipse(e.pos*fine,e.size2/2*fine,e.size2/2*fine);cut=cut.united(hole);}
    }
    QPainterPath area;area.addRect(QRectF(0,0,board.width*fine,board.height*fine));
    QPainterPath fill=area.subtracted(cut);
    if(!spokes.isEmpty())fill=fill.united(spokes);
    return scaled(fill,1/fine);
}

QList<int> connections(const Board &board,bool airwires){
    const int n=int(board.elements.size());const Shapes shapes(board);const auto &layers=shapes.layers;
    // Nodes: the elements, then one ground plane per copper layer.
    UnionFind sets(n+int(layers.size()));QList<bool> copper(n,false);
    for(int k=0;k<layers.size();k++){
        const auto &p=shapes.paths[k];const auto &b=shapes.boxes[k];const int layer=layers[k];const bool ground=board.groundPlane[layer];
        for(int i=0;i<n;i++){
            if(p[i].isEmpty())continue;copper[i]=true;
            if(ground&&(board.elements[i].clearance<=0||board.elements[i].thermal))sets.join(i,n+k);
            for(int j=i+1;j<n;j++)if(!p[j].isEmpty()&&b[i].intersects(b[j])&&sets.find(i)!=sets.find(j)&&p[i].intersects(p[j]))sets.join(i,j);
        }
    }
    if(airwires)for(int i=0;i<n;i++)for(int t:board.elements[i].connections)if(t>=0&&t<n)sets.join(i,t);
    QList<int> out(n,-1);for(int i=0;i<n;i++)if(copper[i])out[i]=sets.find(i);
    return out;
}
QList<int> connectedAt(const Board &board,QPointF at,int layer,bool airwires){
    QList<int> layers=layer?QList<int>{layer}:copperLayers(board);int hit=-1;
    for(int l:layers){for(int i=int(board.elements.size())-1;i>=0;i--)if(copperOn(board.elements[i],l,board).contains(at)){hit=i;break;}if(hit>=0)break;}
    if(hit<0)return {};
    const auto c=connections(board,airwires);QList<int> out;for(int i=0;i<c.size();i++)if(c[i]>=0&&c[i]==c[hit])out.append(i);
    return out;
}
QPolygonF autoroute(const Board &b,int from,int to,int layer,double width,double clearance,double step){
    if(from<0||to<0||from>=b.elements.size()||to>=b.elements.size()||from==to||step<=0||!isCopper(layer))return {};
    // Not more than about half a million raster points: a coarser raster on large boards.
    while((b.width/step+1)*(b.height/step+1)>5e5)step*=2;
    const int w=int(std::floor(b.width/step+1e-9))+1,h=int(std::floor(b.height/step+1e-9))+1;
    // Raster point (i, j) lies at (i·step, j·step); it is blocked where a track centre would come too close.
    QImage blocked(w,h,QImage::Format_Grayscale8);blocked.fill(0);
    {
        QPainter p(&blocked);p.setRenderHint(QPainter::Antialiasing,false);p.translate(.5,.5);p.scale(1/step,1/step);
        const double grow=clearance+width/2;p.setPen(QPen(Qt::white,2*grow,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::white);
        for(int i=0;i<b.elements.size();i++){
            if(i==from||i==to)continue;const auto &e=b.elements[i];const auto copper=copperOn(e,layer,b);
            if(!copper.isEmpty())p.drawPath(copper);
            else if(e.type==ElementType::Pad&&e.size2>0)p.drawEllipse(e.pos,e.size2/2,e.size2/2);   // drill holes go through every layer
        }
        p.setBrush(Qt::NoBrush);p.setPen(QPen(Qt::white,width,Qt::SolidLine,Qt::SquareCap,Qt::MiterJoin));p.drawRect(QRectF(0,0,b.width,b.height));
        // The two pads stay open.
        p.setPen(Qt::NoPen);p.setBrush(Qt::black);p.drawPath(copperShape(b.elements[from]));p.drawPath(copperShape(b.elements[to]));
    }
    auto at=[&](QPointF mm){return QPoint(std::clamp(int(std::lround(mm.x()/step)),0,w-1),std::clamp(int(std::lround(mm.y()/step)),0,h-1));};
    auto open=[&](int x,int y){return x>=0&&y>=0&&x<w&&y<h&&blocked.constScanLine(y)[x]==0;};
    const QPoint start=at(b.elements[from].pos),goal=at(b.elements[to].pos);
    if(!open(start.x(),start.y())||!open(goal.x(),goal.y()))return {};
    // A* over raster point and direction: steps of 1 or √2, a bend costs extra, so routes keep few corners.
    static const int dx[8]={1,1,0,-1,-1,-1,0,1},dy[8]={0,1,1,1,0,-1,-1,-1};
    const size_t states=size_t(w)*h*9;std::vector<float> cost(states,std::numeric_limits<float>::infinity());std::vector<int> previous(states,-1);
    auto state=[&](int x,int y,int d){return int((size_t(y)*w+x)*9+d);};
    auto guess=[&](int x,int y){const double ax=std::abs(x-goal.x()),ay=std::abs(y-goal.y());return std::max(ax,ay)+(M_SQRT2-1)*std::min(ax,ay);};
    using Entry=std::pair<float,int>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;
    const int first=state(start.x(),start.y(),8);cost[size_t(first)]=0;queue.push({float(guess(start.x(),start.y())),first});int found=-1;
    while(!queue.empty()){
        const auto [f,s]=queue.top();queue.pop();const int d=s%9,cellIndex=s/9,x=cellIndex%w,y=cellIndex/w;
        if(f>cost[size_t(s)]+guess(x,y)+1e-3)continue;
        if(x==goal.x()&&y==goal.y()){found=s;break;}
        for(int k=0;k<8;k++){
            const int nx=x+dx[k],ny=y+dy[k];if(!open(nx,ny))continue;
            if(dx[k]&&dy[k]&&(!open(x+dx[k],y)||!open(x,y+dy[k])))continue;     // no cutting of corners
            const int turn=d==8?0:std::min((k-d+8)%8,(d-k+8)%8);if(turn>2)continue;   // at most a right angle at once
            const float step=(dx[k]&&dy[k]?float(M_SQRT2):1.f)+(turn==0?0.f:turn==1?.4f:1.2f);
            const int n=state(nx,ny,k);const float c=cost[size_t(s)]+step;
            if(c<cost[size_t(n)]){cost[size_t(n)]=c;previous[size_t(n)]=s;queue.push({float(c+guess(nx,ny)),n});}
        }
    }
    if(found<0)return {};
    // The raster points back to the start, then only the corners.
    QList<QPoint> cells;for(int s=found;s>=0;s=previous[size_t(s)])cells.prepend(QPoint((s/9)%w,(s/9)/w));
    QPolygonF path;path<<b.elements[from].pos;
    for(int k=1;k+1<cells.size();k++){const QPoint a=cells[k]-cells[k-1],c=cells[k+1]-cells[k];if(a!=c)path<<QPointF(cells[k].x()*step,cells[k].y()*step);}
    path<<b.elements[to].pos;
    return path;
}
QList<std::pair<int,int>> routedAirwires(const Board &board){
    const auto c=connections(board,false);QList<std::pair<int,int>> out;
    for(int i=0;i<board.elements.size();i++)for(int t:board.elements[i].connections)
        if(t>i&&t<c.size()&&c[i]>=0&&c[i]==c[t])out.append({i,t});
    return out;
}

double maximumCurrent(double width,double copper,double rise){
    const double t=copper/1000;if(width<=0||t<=0||rise<=0)return 0;return 5.25*.7*std::sqrt(rise*t*width*(t+width));
}
double widthForCurrent(double current,double copper,double rise){
    // ΔT·t·b² + ΔT·t²·b − (I/3.675)² = 0, the positive root.
    const double t=copper/1000;if(current<=0||t<=0||rise<=0)return 0;const double k=current/(5.25*.7),a=rise*t,b=rise*t*t;
    return (-b+std::sqrt(b*b+4*a*k*k))/(2*a);
}
QList<Finding> checkDesign(const Board &board,const Rules &rules,const std::function<void(int,int)> &progress){
    QList<Finding> out;const int n=int(board.elements.size());
    auto mm=[](double v){return uiLocale().toString(v,'f',2);};
    // With a window, only elements reaching into it count, and only findings marking a part of it.
    const bool whole=rules.window.isEmpty();QList<QRectF> box(n);for(int i=0;i<n;i++)box[i]=bounds(board.elements[i]);
    auto touches=[&](const QRectF &r){return whole||r.adjusted(-1e-6,-1e-6,1e-6,1e-6).intersects(rules.window);};
    auto inside=[&](int i){return touches(box[i]);};
    auto add=[&](const QString &message,const QRectF &area,int layer,const QList<int> &elements){if(touches(area))out.append({message,area.center(),layer,elements,area});};
    auto hole=[](const Element &e){return QRectF(e.pos-QPointF(e.size2/2,e.size2/2),QSizeF(e.size2,e.size2));};
    // Clearance between copper of different connections, layer by layer.
    if(rules.clearanceOn){
        const Shapes shapes(board);const auto parts=connections(board,false);
        const int total=int(shapes.layers.size())*n;
        for(int k=0;k<shapes.layers.size();k++){
            const auto &p=shapes.paths[k];const auto &b=shapes.boxes[k];const int layer=shapes.layers[k];
            QList<QPainterPath> wide(n);
            for(int i=0;i<n;i++){
                if(progress&&i%64==0)progress(k*n+i,total);
                if(p[i].isEmpty())continue;
                for(int j=i+1;j<n;j++){
                    const double reach=rules.clearance*fine;
                    if(p[j].isEmpty()||parts[i]==parts[j]||(!inside(i)&&!inside(j))||!b[i].adjusted(-reach,-reach,reach,reach).intersects(b[j]))continue;
                    if(wide[i].isEmpty())wide[i]=grown(p[i],reach-1e-4);
                    if(!wide[i].intersects(p[j]))continue;
                    // The mark in the middle of the gap: where both shapes, each grown by half the distance, overlap.
                    QRectF where=grown(p[i],(reach-1e-4)/2).intersected(grown(p[j],(reach-1e-4)/2)).boundingRect();
                    if(where.isEmpty())where=wide[i].intersected(p[j]).boundingRect();
                    if(where.isEmpty())where=b[j];
                    add(ui("Abstand kleiner als %1 mm").arg(mm(rules.clearance)),QRectF(where.topLeft()/fine,where.bottomRight()/fine),layer,{i,j});
                }
            }
        }
    }
    // Drill holes too close to each other, from edge to edge, the mark over both.
    if(rules.holeDistanceOn){
        QList<int> holes;for(int i=0;i<n;i++){const auto &e=board.elements[i];if(e.type==ElementType::Pad&&e.size2>0&&isCopper(e.layer))holes.append(i);}
        for(int a=0;a<holes.size();a++)for(int c=a+1;c<holes.size();c++){
            const int i=holes[a],j=holes[c];if(!inside(i)&&!inside(j))continue;const auto &p=board.elements[i],&q=board.elements[j];
            const QPointF d=p.pos-q.pos;const double gap=std::hypot(d.x(),d.y())-p.size2/2-q.size2/2;
            if(gap<rules.holeDistance-1e-9)add(ui("Bohrungen näher als %1 mm beieinander").arg(mm(rules.holeDistance)),hole(p).united(hole(q)),p.layer,{i,j});
        }
    }
    for(int i=0;i<n;i++){
        const auto &e=board.elements[i];if(!inside(i))continue;
        if(rules.minTrackOn&&isCopper(e.layer)&&(e.type==ElementType::Track||(e.type==ElementType::Circle&&!e.filled))&&e.width>0&&e.width<rules.minTrack-1e-9)
            add(ui("Leiterbahn schmaler als %1 mm").arg(mm(rules.minTrack)),box[i],e.layer,{i});
        if(e.type==ElementType::Pad&&e.size2>0){
            if(rules.minRingOn&&e.size2<e.size-1e-9&&(e.size-e.size2)/2<rules.minRing-1e-9)add(ui("Restring kleiner als %1 mm").arg(mm(rules.minRing)),box[i],e.layer,{i});
            if(rules.minDrillOn&&e.size2<rules.minDrill-1e-9)add(ui("Bohrung kleiner als %1 mm").arg(mm(rules.minDrill)),hole(e),e.layer,{i});
            if(rules.maxDrillOn&&e.size2>rules.maxDrill+1e-9)add(ui("Bohrung größer als %1 mm").arg(mm(rules.maxDrill)),hole(e),e.layer,{i});
        }
        // Lines on the silkscreen: tracks, rings and the strokes of texts shown (hidden designators and values do not
        // count); a line without width counts as too narrow. Areas, filled circles and pads have no line to check.
        if(rules.minSilkOn&&(e.layer==SilkTop||e.layer==SilkBottom)){
            double w=-1;
            if(e.type==ElementType::Track||(e.type==ElementType::Circle&&!e.filled))w=e.width;
            else if(e.type==ElementType::Text&&(e.visible||e.role==TextRole::Plain))w=e.strokes.isEmpty()?textStrokeWidth(e.size,e.thickness):e.strokeWidth;
            if(w>=0&&w<rules.minSilk-1e-9)add(ui("Bestückungsdruck schmaler als %1 mm").arg(mm(rules.minSilk)),box[i],e.layer,{i});
        }
        // The solder mask: pads that have no opening (plain holes need none), and openings over other elements of the
        // outer copper layers (an area that is only a mask opening counts by its own opening switch alone).
        const bool pad=e.type==ElementType::Pad||e.type==ElementType::SmdPad;
        if(rules.padsWithoutMask&&pad&&isCopper(e.layer)&&!e.solderMask&&!(e.type==ElementType::Pad&&e.size2>=e.size-1e-9))
            add(ui("Pad ohne Öffnung in der Lötstoppmaske"),box[i],e.layer,{i});
        if(rules.maskOutsidePads&&!pad&&e.solderMask&&(e.layer==CopperTop||e.layer==CopperBottom))
            add(ui("Öffnung in der Lötstoppmaske außerhalb der Pads"),box[i],e.layer,{i});
    }
    // Drill holes reaching onto SMD pads, on any copper layer (holes go through the board), the mark over the hole.
    if(rules.holesOnSmd)for(int j=0;j<n;j++){
        const auto &smd=board.elements[j];if(smd.type!=ElementType::SmdPad||!isCopper(smd.layer))continue;
        const QPainterPath copper=scaled(copperShape(smd),fine);const QRectF sb=box[j];
        for(int i=0;i<n;i++){
            const auto &pad=board.elements[i];if(pad.type!=ElementType::Pad||pad.size2<=0||!isCopper(pad.layer)||!hole(pad).intersects(sb)||(!inside(i)&&!inside(j)))continue;
            QPainterPath disc;disc.addEllipse(pad.pos*fine,pad.size2/2*fine,pad.size2/2*fine);
            if(copper.intersects(disc))add(ui("Bohrung auf einem SMD-Pad"),hole(pad),smd.layer,{j,i});
        }
    }
    if(rules.silkOnPads){
        // Silkscreen of a side over solderable pads of the same side; hidden designators and values are not printed.
        for(int i=0;i<n;i++){
            const auto &s=board.elements[i];if((s.layer!=SilkTop&&s.layer!=SilkBottom)||(s.type==ElementType::Text&&!s.visible&&s.role!=TextRole::Plain))continue;
            const int side=s.layer==SilkTop?CopperTop:CopperBottom;const auto silk=copperShape(s);const QRectF sb=silk.boundingRect();
            for(int j=0;j<n;j++){
                const auto &pad=board.elements[j];if((pad.type!=ElementType::Pad&&pad.type!=ElementType::SmdPad)||!pad.solderMask||(!inside(i)&&!inside(j)))continue;
                const auto copper=copperOn(pad,side,board);if(copper.isEmpty()||!sb.intersects(copper.boundingRect())||!silk.intersects(copper))continue;
                // Shapes that only touch along an edge have no common area: the mark goes where their boxes meet.
                QRectF where=silk.intersected(copper).boundingRect();if(where.isEmpty())where=sb.intersected(copper.boundingRect());if(where.isNull())where=sb;
                add(ui("Bestückungsdruck auf einem Pad"),where,s.layer,{i,j});
            }
        }
    }
    return out;
}
}
