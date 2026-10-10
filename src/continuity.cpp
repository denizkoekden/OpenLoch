#include "continuity.h"
#include "geometry.h"
#include "project.h"
#include <QJsonArray>
#include <QLineF>
#include <QMap>
#include <QPainterPathStroker>
#include <QTransform>
#include <cmath>
#include <functional>

namespace openloch {
namespace {
// Rectangles follow LochMaster's normalisation: ordered corners, degenerate sides widened by one unit.
struct Box {
    double l,t,r,b;
    static Box from(QPointF a,QPointF c){Box x{qMin(a.x(),c.x()),qMin(a.y(),c.y()),qMax(a.x(),c.x()),qMax(a.y(),c.y())};if(x.l==x.r){x.l--;x.r++;}if(x.t==x.b){x.t--;x.b++;}return x;}
    bool intersects(const Box &o) const{return qMax(l,o.l)<qMin(r,o.r)&&qMax(t,o.t)<qMin(b,o.b);}
    QRectF rect() const{return QRectF(QPointF(l,t),QPointF(r,b));}
};
QPointF point(const QJsonValue &v,int at=0){const auto a=v.toArray();return {a.at(at).toDouble(),a.at(at+1).toDouble()};}
Conductor copperPart(const QPainterPath &shape){Conductor c;c.copper=true;c.shape=shape;c.bounds=shape.boundingRect();return c;}
// Contact areas of wire-like objects; kinds without an area, or paths below two points, never overlap anything.
QPainterPath wireArea(int kind,const QPolygonF &path,double width) {
    QPainterPath area;if(path.size()<2)return area;
    if(kind==1||kind==4||kind==9){QPainterPath line;line.addPolygon(path);QPainterPathStroker s;s.setWidth(qMax(1.0,width));s.setCapStyle(Qt::RoundCap);return s.createStroke(line);}
    const int half=int(width)/2;const auto box=Box::from(path[0]-QPointF(half,half),path[0]+QPointF(half,half)).rect();
    if(kind==11||kind==18)area.addRect(box);else if(kind==19)area.addEllipse(box);
    return area;
}
bool contains(const Conductor &c,QPointF p){return c.bounds.adjusted(-1,-1,1,1).contains(p)&&c.shape.intersects(QRectF(p.x()-1,p.y()-1,2,2));}
bool overlaps(const Conductor &a,const Conductor &b){return !a.shape.isEmpty()&&!b.shape.isEmpty()&&a.bounds.intersects(b.bounds)&&a.shape.intersects(b.shape);}
}
void Continuity::addPad(QPointF centre,double diameter){pads.append({centre,diameter});prepared=false;}
void Continuity::addStrip(QPointF from,QPointF to,double width){strips.append({from,to,width});prepared=false;}
void Continuity::addCut(const QRectF &rect){cuts.append(rect.normalized());prepared=false;}
void Continuity::addDrill(QPointF centre,double diameter){drills.append({centre,diameter});prepared=false;}
void Continuity::addMarker(QPointF at,const QColor &colour,const QString &name){markers.append({at,colour,name});}
void Continuity::addWire(int kind,bool back,const QPolygonF &path,double width){
    Conductor c;c.kind=kind;c.back=back;c.path=path;c.width=width;c.shape=wireArea(kind,path,width);c.bounds=c.shape.isEmpty()?path.boundingRect():c.shape.boundingRect();parts.append(c);prepared=false;
}
void Continuity::prepare(){
    if(prepared)return;
    QList<Conductor> wires;for(const auto &c:parts)if(!c.copper)wires.append(c);parts.clear();
    // Cutters: separators, and drills above 1.5 mm as a thin cross over their diameter.
    QList<Box> cutters;for(const auto &r:cuts)cutters.append(Box::from(r.topLeft(),r.bottomRight()));
    for(const auto &[c,d]:drills)if(d>1.5){const double r=std::round(d*50);cutters.append(Box::from(c+QPointF(-1,-r),c+QPointF(1,r)));cutters.append(Box::from(c+QPointF(-r,-1),c+QPointF(r,1)));}
    // Axis-parallel strips split where a cutter spans their full width; other copper stays whole.
    struct Piece{Box box;bool horizontal;};QList<Piece> pieces;
    for(const auto &s:strips){
        const double w=std::round(s.width);
        if(std::abs(s.from.y()-s.to.y())<.5)pieces.append({Box::from(QPointF(s.from.x(),s.from.y()-w),QPointF(s.to.x(),s.from.y()+w)),true});
        else if(std::abs(s.from.x()-s.to.x())<.5)pieces.append({Box::from(QPointF(s.from.x()-w,s.from.y()),QPointF(s.from.x()+w,s.to.y())),false});
        else{QLineF line(s.from,s.to);auto normal=line.normalVector().unitVector();QPointF n(normal.dx()*w,normal.dy()*w);QPainterPath area;area.addPolygon(QPolygonF{s.from-n,s.to-n,s.to+n,s.from+n,s.from-n});parts.append(copperPart(area));}
    }
    for(int i=0;i<pieces.size();i++){
        const auto s=pieces[i];
        for(const auto &c:cutters){
            if(!c.intersects(s.box))continue;
            if(s.horizontal&&c.t<=s.box.t&&s.box.b<c.b){
                if(s.box.l<c.l)pieces.append({{s.box.l,s.box.t,c.l-1,s.box.b},true});
                if(c.r<s.box.r)pieces.append({{c.r+1,s.box.t,s.box.r,s.box.b},true});
            }else if(!s.horizontal&&c.l<=s.box.l&&s.box.r<c.r){
                if(s.box.t<c.t)pieces.append({{s.box.l,s.box.t,s.box.r,c.t-1},false});
                if(c.b<s.box.b)pieces.append({{s.box.l,c.b+1,s.box.r,s.box.b},false});
            }else continue;
            pieces.removeAt(i);i=-1;break;
        }
    }
    for(const auto &p:pieces){QPainterPath area;area.addRect(p.box.rect());parts.append(copperPart(area));}
    for(const auto &[c,d]:pads){const double r=std::round(d*50);QPainterPath area;area.addEllipse(c,r,r);parts.append(copperPart(area));}
    parts.append(wires);prepared=true;
}
QList<Conductor> Continuity::trace(QPointF at){QList<Conductor> result;for(int i:connected(at,true))result.append(parts[i]);return result;}
QList<int> Continuity::netAt(QPointF at){return connected(at,false);}
// LochMaster floods each marker separately, keeping the first colour on copper. Connections form disjoint nets, so the
// first marker on a net colours all of it and later markers on that net change nothing.
Continuity::Potentials Continuity::potentials(){
    prepare();Potentials result;QList<QColor> colours(parts.size());
    for(const auto &marker:markers){
        if(!marker.colour.isValid()||marker.colour.rgb()==QColor(Qt::black).rgb())continue;
        const auto net=connected(marker.at,false);if(net.isEmpty())continue;
        if(colours[net.first()].isValid()){result.conflicts+=colours[net.first()]!=marker.colour;continue;}
        for(int i:net)colours[i]=marker.colour;
    }
    for(int i=0;i<parts.size();i++)if(colours[i].isValid()){auto c=parts[i];c.potential=colours[i];result.coloured.append(c);}
    return result;
}
QList<Conductor> Continuity::freeCopper(){
    prepare();QList<QPointF> terminals;
    // Terminal counts as in LochMaster: bridges both ends; leads, pins and copper-side blobs their first point; kind 13 none.
    for(const auto &c:parts)if(!c.copper&&!c.path.isEmpty()){
        if(c.kind==1)terminals<<c.path.first()<<c.path.last();
        else if(c.kind==9||c.kind==11||(c.kind==19&&c.back))terminals<<c.path.first();
    }
    for(const auto &marker:markers)terminals<<marker.at;
    QList<bool> used(parts.size(),false);QList<int> spread;
    for(auto at:terminals)for(int i=0;i<parts.size();i++)if(parts[i].copper&&!used[i]&&contains(parts[i],at)){used[i]=true;spread<<i;}
    while(!spread.isEmpty()){const int i=spread.takeLast();for(int j=0;j<parts.size();j++)if(parts[j].copper&&!used[j]&&overlaps(parts[i],parts[j])){used[j]=true;spread<<j;}}
    QList<Conductor> result;for(int i=0;i<parts.size();i++)if(parts[i].copper&&!used[i])result.append(parts[i]);return result;
}
QList<Continuity::Short> Continuity::shorts(){
    prepare();QList<Short> result;
    auto identity=[](const Marker &m){const auto name=m.name.trimmed();return name.isEmpty()?m.colour.name():name.toUpper();};
    auto label=[](const Marker &m){const auto name=m.name.trimmed();return name.isEmpty()?m.colour.name():name;};
    // Markers grouped by net, in document order; black markers carry no potential, as in LochMaster.
    QMap<int,QList<int>> nets;
    for(int m=0;m<markers.size();m++){
        if(!markers[m].colour.isValid()||markers[m].colour.rgb()==QColor(Qt::black).rgb())continue;
        const auto net=connected(markers[m].at,false);if(!net.isEmpty())nets[net.first()].append(m);
    }
    for(const auto &onNet:nets){
        QList<int> distinct;for(int m:onNet){bool known=false;for(int d:distinct)known|=identity(markers[d])==identity(markers[m]);if(!known)distinct<<m;}
        for(int k=1;k<distinct.size();k++){
            const auto &a=markers[distinct[0]],&b=markers[distinct[k]];Short s{label(a),label(b),a.at,b.at,{}};
            for(int i:chain(a.at,b.at))s.chain<<parts[i];result<<s;
        }
    }
    return result;
}
// Breadth-first search over copper overlaps and bridges (leads do not pass current on), so the chain is shortest in steps.
QList<int> Continuity::chain(QPointF from,QPointF to){
    prepare();QList<int> parent(parts.size(),-2),queue;int head=0;
    auto enter=[&](QPointF p,int via){for(int i=0;i<parts.size();i++)if(parts[i].copper&&parent[i]==-2&&contains(parts[i],p)){parent[i]=via;queue<<i;}};
    enter(from,-1);
    while(head<queue.size()){
        const int i=queue[head++];const auto &c=parts[i];
        if(!c.copper){enter(c.path.first(),i);enter(c.path.last(),i);continue;}
        if(contains(c,to)){QList<int> result;for(int k=i;k>=0;k=parent[k])result.prepend(k);return result;}
        for(int j=0;j<parts.size();j++){
            if(parent[j]!=-2)continue;const auto &x=parts[j];
            if(x.copper?overlaps(c,x):(x.kind==1&&!x.path.isEmpty()&&(contains(c,x.path.first())||contains(c,x.path.last())))){parent[j]=i;queue<<j;}
        }
    }
    return {};
}
QList<int> Continuity::connected(QPointF at,bool click){
    prepare();marked=QList<bool>(parts.size(),false);
    // Only a click into a large drill is refused; potential markers are not checked against drills.
    if(click)for(const auto &[c,d]:drills)if(d>1.5&&QLineF(c,at).length()<=d*50)return {};
    // Worklist instead of LochMaster's recursion; marking is monotone, so the connected set is the same.
    enum Step{FromPoint,FromCopper,FromBlobs};struct Job{Step step;QPointF at;int index;};
    QList<Job> jobs{{FromPoint,at,-1}};
    auto first=[&](int i){return parts[i].path.isEmpty()?QPointF():parts[i].path.first();};
    auto last=[&](int i){return parts[i].path.isEmpty()?QPointF():parts[i].path.last();};
    auto lead=[&](int k){return k==9||k==11;};
    while(!jobs.isEmpty()){
        const auto job=jobs.takeLast();
        if(job.step==FromPoint){
            for(int i=0;i<parts.size();i++)if(parts[i].copper&&!marked[i]&&contains(parts[i],job.at)){marked[i]=true;jobs.append({FromCopper,{},i});}
        }else if(job.step==FromCopper){
            const auto &copper=parts[job.index];
            for(int i=0;i<parts.size();i++){
                const auto &x=parts[i];if(marked[i])continue;
                if(x.copper){if(overlaps(copper,x)){marked[i]=true;jobs.append({FromCopper,{},i});}continue;}
                if(x.path.isEmpty())continue;
                // Bridges connect through either end, leads only through the soldered first point.
                if(x.kind==1){
                    if(contains(copper,first(i))){marked[i]=true;jobs.append({FromPoint,last(i),-1});jobs.append({FromBlobs,{},i});}
                    else if(contains(copper,last(i))){marked[i]=true;jobs.append({FromPoint,first(i),-1});jobs.append({FromBlobs,{},i});}
                }else if(lead(x.kind)){if(contains(copper,first(i))){marked[i]=true;jobs.append({FromBlobs,{},i});}}
                else if(x.kind==19&&x.back&&contains(copper,first(i)))jobs.append({FromBlobs,{},i});
            }
        }else{
            // Solder blobs: same side joins by overlapping areas, the other side only through wire ends inside the blob.
            const auto &object=parts[job.index];
            for(int b=0;b<parts.size();b++){
                if(parts[b].kind!=19||parts[b].copper||marked[b]||!overlaps(object,parts[b]))continue;
                marked[b]=true;const auto &blob=parts[b];
                for(int e=0;e<parts.size();e++){
                    const auto &x=parts[e];if(x.copper||x.path.isEmpty())continue;
                    if(x.back==blob.back){
                        if(!overlaps(blob,x))continue;marked[e]=true;
                        if(x.kind==1){jobs.append({FromPoint,first(e),-1});jobs.append({FromPoint,last(e),-1});jobs.append({FromBlobs,{},e});}
                        else if(lead(x.kind)){jobs.append({FromPoint,first(e),-1});jobs.append({FromBlobs,{},e});}
                    }else if(x.kind==1&&(contains(blob,first(e))||contains(blob,last(e)))){marked[e]=true;jobs.append({FromPoint,first(e),-1});jobs.append({FromPoint,last(e),-1});jobs.append({FromBlobs,{},e});}
                    else if(lead(x.kind)&&contains(blob,first(e))){marked[e]=true;jobs.append({FromPoint,first(e),-1});jobs.append({FromBlobs,{},e});}
                }
                if(blob.back)jobs.append({FromPoint,first(b),-1});
            }
        }
    }
    QList<int> result;for(int i=0;i<parts.size();i++)if(marked[i])result.append(i);return result;
}
Continuity continuityModel(const Project &project) {
    Continuity model;if(project.mode!="board")return model;
    const QPointF shift=-project.offset();const bool documentIsBoard=project.sourceKind=="lmb"&&project.boardSource.isEmpty();
    std::function<void(const QJsonObject&,const QTransform&,bool)> walk=[&](const QJsonObject &o,const QTransform &t,bool board){
        if(o.value("deleted").toBool())return;
        if(o.contains("children")){for(const auto &child:o["children"].toArray())walk(child.toObject(),t,board);return;}
        const auto type=o["type"].toString();const int kind=o["kind"].toInt();
        // Only the board's strips and pads conduct; document copper classes do not.
        if(type=="TLeiterbahn"){auto path=o["path"].toArray();if(board&&!path.isEmpty())model.addStrip(t.map(point(path.first())),t.map(point(path.last())),o["width"].toDouble());}
        else if(type=="TAuge"){if(board)model.addPad(t.map(point(o["center"])),o["diameter"].toDouble());}
        else if(type=="TBohrung")model.addDrill(t.map(point(o["center"])),o["diameter"].toDouble());
        else if(type=="TTrenner"||type=="TTrennerFest"){if(!board||type=="TTrennerFest"){auto r=o["rect"];model.addCut(t.mapRect(QRectF(point(r),point(r,2)).normalized()));}}
        else if(type=="TDraht"){
            if(!board&&kind==18&&!o["path"].toArray().isEmpty()){const auto pen=quint32(o["pen"].toInteger());model.addMarker(t.map(point(o["path"].toArray().first())),QColor(pen&255,(pen>>8)&255,(pen>>16)&255),o["label"].toString());}
            if(board||!QList<int>{1,9,11,13,19}.contains(kind))return;
            QPolygonF path;for(const auto &p:o["path"].toArray())path<<t.map(point(p));model.addWire(kind,o["back"].toBool(),path,o["width"].toDouble());
        }else if(o.contains("x")&&!board){
            const QPointF at(o["x"].toDouble(),o["y"].toDouble());const auto local=objectTransform(o,at)*t;
            if(type=="wire")model.addWire(1,o.value("back").toBool(true),QPolygonF{t.map(at),local.map(QPointF(o["x2"].toDouble(),o["y2"].toDouble()))},o["width"].toDouble(45));
            else if(type=="cut")model.addCut(local.mapRect(QRectF(at-QPointF(85,85),QSizeF(170,170))));
            else if(type=="drill")model.addDrill(t.map(at),o["diameter"].toDouble(.9));
            else if(type=="lead"){QPolygonF path;for(const auto &v:o["points"].toArray())path<<local.map(at+point(v));if(path.isEmpty())path<<t.map(at);model.addWire(9,o["back"].toBool(),path,o["width"].toDouble(45));}
            else if(type=="solder"){const auto centre=t.map(at);model.addWire(19,o["back"].toBool(),QPolygonF{centre,centre},o["width"].toDouble(150));}
            else if(type=="potential")model.addMarker(t.map(at),QColor(o["color"].toString()),o["name"].toString());
        }
    };
    // Same placement as the canvas: board underlay, imported objects with their moves, then additions.
    auto boardDoc=project.legacy.value("board").toObject();QPointF boardShift=shift;
    if(!project.boardSource.isEmpty()){boardDoc=project.libraries.value(project.boardSource).document;boardShift=-point(boardDoc.value("origin"));}
    for(const auto &o:boardDoc.value("objects").toArray())walk(o.toObject(),QTransform::fromTranslate(boardShift.x(),boardShift.y()),true);
    const auto objects=project.legacy.value("objects").toArray();
    for(int i=0;i<objects.size();i++){
        const auto node=project.legacyNode(i);const QPointF pos=shift+point(project.moves.value(QString::number(i)));
        walk(node,objectTransform(node,componentAnchor(objects[i].toObject()))*QTransform::fromTranslate(pos.x(),pos.y()),documentIsBoard);
    }
    for(const auto &value:project.additions){
        const auto node=value.toObject();
        if(node["type"]=="component"){
            const auto anchor=componentAnchor(project.libraryNode(node["library"].toString(),node["index"].toInt()));const QPointF pos=QPointF(node["x"].toDouble(),node["y"].toDouble())-anchor;
            walk(project.componentNode(node),objectTransform(node,anchor)*QTransform::fromTranslate(pos.x(),pos.y()),false);
        }else walk(node,{},false);
    }
    // A board without template is OpenLoch's perfboard: isolated 1.8 mm pads on the 2.54 mm grid, as exported to LM4.
    const bool templatePresent=!project.boardSource.isEmpty()||!project.legacy.value("board").toObject().isEmpty()||!project.legacy.isEmpty();
    if(!templatePresent)for(double x=254;x<project.width;x+=254)for(double y=254;y<project.height;y+=254){model.addPad({x,y},1.8);model.addDrill({x,y},.9);}
    return model;
}
}
