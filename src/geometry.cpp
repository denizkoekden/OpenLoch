#include "geometry.h"
#include "language.h"
#include <QJsonArray>
#include <QPolygonF>
#include <QSet>
#include <QLocale>
#include <QLineF>
#include <QMap>
#include <functional>
#include <cmath>
namespace openloch {
static QPointF point(const QJsonArray &a,int i=0){return {a[i].toDouble(),a[i+1].toDouble()};}
QString componentId(const QJsonObject &group) {
    auto id=group["id"].toString();auto index=id.indexOf('#');
    return index<0?id:id.left(index)+QString::number(group["group_value"].toInt());
}
// Placeholders in labels, replaced wherever they occur like in the original: the part's extra fields as <Feldname>
// (exact case; they win over built-ins of the same name), then Kennung, Wert/Typ, Name, Beschreibung and board name.
QString componentText(const QString &text,const QJsonObject &group,const QString &title) {
    if(!text.contains('<'))return text;QString out=text;
    for(auto v:group["extra"].toArray()){const auto f=v.toArray();if(!f.at(0).toString().isEmpty())out.replace('<'+f.at(0).toString()+'>',f.at(2).toString());}
    const auto name=group["label"].toString().isEmpty()?group["description"].toString():group["label"].toString();
    const QList<std::pair<QStringList,QString>> builtins{{{"<<KB>>","<BauteilKennung>","<ComponentID>"},componentId(group)},
        {{"<<Wert>>","<BauteilWertTyp>","<BauteilWert>","<ComponentValueType>","<ComonentValueType>"},group["value"].toString()},
        {{"<BauteilName>","<ComponentName>"},name},{{"<BauteilBeschreibung>","<ComponentDescription>"},group["description"].toString()},{{"<BoardName>","<BordName>"},title}};
    for(const auto &[keys,value]:builtins)for(const auto &key:keys)out.replace(key,value);
    return out;
}
std::optional<std::array<QPointF,4>> labelStoredCorners(const QJsonObject &o){
    const QPointF p0=point(o["text_position"].toArray());const auto a=o["text_anchors"].toArray();if(a.size()<10)return std::nullopt;
    const QPointF p1(a[0].toDouble(),a[1].toDouble()),p2(a[4].toDouble(),a[5].toDouble()),p3(a[8].toDouble(),a[9].toDouble());
    if(p2==p0||(p1.isNull()&&p2.isNull()&&p3.isNull()))return std::nullopt;
    return std::array<QPointF,4>{p0,p1,p2,p3};
}
// `corners`: labels with stored corners take their box from them, else every label's box is estimated from its text.
static QRectF bounds(const QJsonObject &o,const QJsonObject &group,bool texts=true,bool corners=true) {
    QRectF r;bool initialized=false;
    auto add=[&](QRectF b){if(b.isEmpty())b.adjust(-1,-1,1,1);r=initialized?r.united(b):b;initialized=true;};
    if(o.contains("children")){for(auto child:o["children"].toArray()){const auto c=child.toObject();if(texts||c["type"]!="TTextLabel")add(bounds(c,o,texts,corners));}return r;}
    if(o.contains("inner"))return bounds(o["inner"].toObject(),group,texts,corners);
    if(o.contains("path")){QPolygonF poly;for(auto p:o["path"].toArray())poly<<point(p.toArray());double w=o["type"]=="TLeiterbahn"?qMax(1.0,o["width"].toDouble()):qMax(2.0,o["width"].toDouble())/2;add(poly.boundingRect().adjusted(-w,-w,w,w));}
    if(o.contains("rect")){auto a=o["rect"].toArray();add(QRectF(point(a),point(a,2)).normalized());}
    if(o.contains("center")){double radius=o["diameter"].toDouble()*50;add(QRectF(point(o["center"].toArray())-QPointF(radius,radius),QSizeF(2*radius,2*radius)));}
    if(o["type"]=="TTextLabel"&&texts) {
        // Stored corners are the label's box; without them (old files, collapsed labels) it is estimated from the angle.
        if(const auto c=corners?labelStoredCorners(o):std::nullopt){add(QPolygonF{(*c)[0],(*c)[1],(*c)[2],(*c)[3]}.boundingRect());return r;}
        auto text=componentText(o["text"].toString(),group);double height=qMax(20,o["text_kind"].toInt());
        QTransform t;t.translate(point(o["text_position"].toArray()).x(),point(o["text_position"].toArray()).y());t.rotateRadians(-o["text_height"].toDouble()); // counter-clockwise like the drawing
        add(t.mapRect(QRectF(0,0,qMax(1,text.size())*height*.7,height*1.2)));
    }
    return r;
}
QRectF legacyBounds(const QJsonObject &o){return bounds(o,{});}
QRectF drawingBounds(const QJsonObject &o){return o["type"]=="TTextLabel"?QRectF():bounds(o,{},false);}
std::optional<bool> solderSide(const QJsonObject &n){
    if(n.contains("children")){for(auto v:n["children"].toArray())if(auto s=solderSide(v.toObject()))return s;return std::nullopt;}
    const auto t=n["type"].toString();if(t=="TLeiterbahn"||t=="TAuge"||t=="TBohrung"||t.startsWith("TTrenner")||n["kind"].toInt()==18)return std::nullopt;
    return t=="wire"?n.value("back").toBool(true):n["back"].toBool();
}
QPointF componentAnchor(const QJsonObject &o){auto c=bounds(o,{},true,false).center();return {std::round(c.x()/254)*254,std::round(c.y()/254)*254};}
QTransform objectTransform(const QJsonObject &properties,QPointF center) {
    QTransform t;t.translate(center.x(),center.y());t.rotate(properties["angle"].toDouble());
    t.scale(properties["mirrorX"].toBool()?-1:1,properties["mirrorY"].toBool()?-1:1);t.translate(-center.x(),-center.y());return t;
}
namespace {
// A part's terminal: the top-level child holding it, its point and whether it is the far end of a wire (kind 1).
struct Terminal {int child;QPointF at;bool end;};
QList<Terminal> terminalsOf(const QJsonObject &group){
    QList<Terminal> out;const auto children=group["children"].toArray();
    std::function<void(int,const QJsonObject&)> walk=[&](int child,const QJsonObject &n){
        if(n.contains("children")){for(const auto &v:n["children"].toArray())walk(child,v.toObject());return;}
        const auto path=n["path"].toArray();const int kind=n["kind"].toInt();auto at=[&](const QJsonValue &v){return QPointF(v.toArray()[0].toDouble(),v.toArray()[1].toDouble());};
        if((n["type"]!="TDraht"&&n["type"]!="TDrahtFest")||path.isEmpty()||!(kind==1||kind==9||kind==11))return;
        out.append({child,at(path.first()),false});if(kind==1&&path.size()>1&&at(path.last())!=at(path.first()))out.append({child,at(path.last()),true});
    };
    for(int i=0;i<children.size();i++)walk(i,children[i].toObject());
    return out;
}
}
QList<QPointF> partTerminals(const QJsonObject &group){QList<QPointF> out;for(const auto &t:terminalsOf(group))out.append(t.at);return out;}
QJsonObject withReferenceTerminal(QJsonObject group,int index){
    const auto terminals=terminalsOf(group);if(index<0||index>=terminals.size())return group;
    auto children=group["children"].toArray();auto child=children.takeAt(terminals[index].child).toObject();
    // The far end of a wire comes first by reversing the wire, which looks the same.
    if(terminals[index].end&&!child.contains("children")){QJsonArray reversed;for(const auto &p:child["path"].toArray())reversed.prepend(p);child["path"]=reversed;}
    children.prepend(child);group["children"]=children;return group;
}
QList<QPair<QPointF,QString>> namedConnectionPoints(const QJsonObject &node) {
    QList<QPair<QPointF,QString>> result;
    std::function<void(const QJsonObject&)> walk=[&](const QJsonObject &n){
        auto add=[&](QPointF p){for(const auto &r:result)if(r.first==p)return;result.append({p,n["label"].toString()});};
        if(n.contains("children")){for(const auto &child:n["children"].toArray())walk(child.toObject());return;}
        const auto type=n["type"].toString();const int kind=n["kind"].toInt();
        // The legacy base point list is a cached electrical grid. Use the physical
        // path: lead types 9/11 terminate at its first point, wire type 1 at both ends.
        if(type=="TDraht"||type=="TDrahtFest"||type=="TLeiterbahn"){
            const auto path=n["path"].toArray();
            if(!path.isEmpty()&&(kind==1||kind==9||kind==11||kind==18||kind==19||type=="TLeiterbahn")){
                add(point(path.first().toArray()));
                if(kind==1||type=="TLeiterbahn")add(point(path.last().toArray()));
            }
        }else if(type=="TAuge")add(point(n["center"].toArray()));
        else if(n.contains("x")){
            const QPointF at(n["x"].toDouble(),n["y"].toDouble());const auto transform=objectTransform(n,at);
            if(type=="pin"||type=="pad"||type=="solder"||type=="lead")add(at);
            else if(type=="wire"){add(at);add(transform.map(QPointF(n["x2"].toDouble(),n["y2"].toDouble())));}
            else if(type=="resistor"||type=="capacitor"||type=="diode"){add(transform.map(at+QPointF(-254,0)));add(transform.map(at+QPointF(254,0)));}
            else if(type=="ground")add(transform.map(at+QPointF(0,-254)));
        }
    };
    walk(node);return result;
}
QList<QPointF> connectionPoints(const QJsonObject &node) {QList<QPointF> result;for(const auto &p:namedConnectionPoints(node))result.append(p.first);return result;}
QList<QPointF> boardHolePoints(const QJsonObject &node) {
    QList<QPointF> result;QSet<QPair<double,double>> seen;
    std::function<void(const QJsonObject&)> visit=[&](const QJsonObject &n){
        // Stripboards often have holes and copper strips, without separate pads.
        // Larger mounting bores are not component insertion targets.
        if(n["type"]=="TAuge"||(n["type"]=="TBohrung"&&n["diameter"].toDouble()<=1.5)){
            const auto p=point(n["center"].toArray());const auto key=qMakePair(p.x(),p.y());
            if(!seen.contains(key)){seen.insert(key);result.append(p);}
        }
        for(auto child:n["children"].toArray())visit(child.toObject());
    };
    visit(node);return result;
}
// Integer helpers as in the original: midpoints truncate towards zero, distances and samples are rounded.
static QPointF halfway(QPointF a,QPointF b){return {a.x()+std::trunc((b.x()-a.x())/2),a.y()+std::trunc((b.y()-a.y())/2)};}
static double rounded(double v){return std::nearbyint(v);} // round half to even like Delphi's Round
static void curve(QPolygonF &out,QPointF a,QPointF b,QPointF c){
    for(int k=0;k<20;k++){const double t=k*.05,u=1-t;out<<QPointF(rounded(u*u*a.x()+2*t*u*b.x()+t*t*c.x()),rounded(u*u*a.y()+2*t*u*b.y()+t*t*c.y()));}
    out<<c;
}
// The point `size` away from b towards a, or a itself when the edge is not longer than that.
static QPointF cut(QPointF a,QPointF b,double size){
    const double d=a==b?0:rounded(std::hypot(a.x()-b.x(),a.y()-b.y()));if(!(size<d))return a;const double t=size/d;
    return {rounded((a.x()-b.x())*t+b.x()),rounded((a.y()-b.y())*t+b.y())};
}
QPolygonF drawnPath(const QJsonObject &wire){
    QPolygonF p;for(auto v:wire["path"].toArray()){auto a=v.toArray();p<<QPointF(a[0].toDouble(),a[1].toDouble());}
    const int n=p.size();if(!n)return p;const int kind=wire["kind"].toInt();const bool closed=kind==6||kind==7,smooth=wire["flag2"].toBool();
    const int style=wire["style"].toInt();const double size=wire["rotation"].toDouble(200);
    auto at=[&](int i){return p[i%n];};QPolygonF out;
    auto corner=[&](int i){
        const QPointF a=at(i),b=at(i+1),c=at(i+2);
        if(style==0)curve(out,halfway(a,b),b,halfway(b,c));
        else{const QPointF from=cut(a,b,size),to=cut(c,b,size);if(style==1)out<<from<<to;else curve(out,from,b,to);}
    };
    if(!smooth||style<0||style>2){out=p;if(closed)out<<p.first();return out;}
    if(closed){
        const int corners=style==1?n+1:n;for(int i=0;i<corners;i++)corner(i);
        if(style!=0&&!out.isEmpty())out<<out.first();
    }else{out<<p.first();for(int i=0;i+2<n;i++)corner(i);out<<p.last();}
    return out;
}
std::pair<QString,QString> objectDescription(const QJsonObject &n){
    const QLocale locale=uiLocale();auto mm=[&](double v){return locale.toString(v/100,'g',6);};
    if(n.contains("children")){
        const auto flags=n["group_flags"].toArray();const bool part=flags.isEmpty()||flags.at(0).toBool();const auto id=componentId(n);
        const auto name=n["label"].toString().isEmpty()?n["description"].toString():n["label"].toString();
        if(part&&!id.isEmpty())return {id+": "+name,{}};
        return {n["label"].toString().isEmpty()?ui("Gruppe ; (%1 Objekte)").arg(n["children"].toArray().size()):n["label"].toString(),{}};
    }
    const auto type=n["type"].toString();const int kind=n["kind"].toInt();QString name,detail;QPolygonF path;
    for(auto v:(n.contains("path")?n["path"]:n["points"]).toArray()){const auto a=v.toArray();path<<QPointF(a[0].toDouble(),a[1].toDouble());}
    auto length=[&]{double l=0;for(int i=1;i<path.size();i++)l+=QLineF(path[i-1],path[i]).length();return l;};
    auto wire=[&](double l,bool bridge){QString d="(L="+mm(l)+" mm";const double holes=l/254;if(bridge&&holes>=.5&&std::abs(holes-std::round(holes))<1e-6)d+=ui(" Lochabstand %1").arg(qRound(holes));return d+")";};
    if(type=="TDraht"||type=="TDrahtFest"){
        static const QMap<int,QString> names{{1,"Draht"},{4,"Linie"},{6,"Rechteck"},{7,"Polygon"},{9,"Bauteilanschluss"},{11,"Pin"},{18,"Potenzial"},{19,"Lötstelle"}};name=ui(names.value(kind,"Draht"));
        if(kind==1||kind==9)detail=wire(length(),kind==1&&path.size()==2);else if(kind==4||kind==7)detail=ui("(%1 Knoten)").arg(path.size());
        else if(kind==6){const auto r=path.boundingRect();detail="("+mm(r.width())+" x "+mm(r.height())+" mm)";}
    }
    else if(type=="TLeiterbahn")name=ui("Leiterbahn");else if(type.startsWith("TTrenner"))name=ui("Trenner");else if(type=="TAuge")name=ui("Lötauge");
    else if(type=="TBohrung"){name=ui("Bohrung");detail="("+locale.toString(n["diameter"].toDouble(),'g',6)+" mm)";}
    else if(type=="TTextLabel"){name=ui("Text");detail="\""+n["text"].toString()+"\"";}
    else if(type=="TKreis"){name=ui("Kreis");path.clear();for(auto v:n["inner"].toObject()["path"].toArray()){const auto a=v.toArray();path<<QPointF(a[0].toDouble(),a[1].toDouble());}const auto r=path.boundingRect();detail="("+mm(r.width())+" x "+mm(r.height())+" mm)";}
    else{
        static const QMap<QString,QString> names{{"wire","Draht"},{"polyline","Linie"},{"rectangle","Rechteck"},{"polygon","Polygon"},{"lead","Bauteilanschluss"},{"pin","Pin"},{"potential","Potenzial"},
            {"solder","Lötstelle"},{"cut","Trenner"},{"pad","Lötauge"},{"eye","Lötauge"},{"drill","Bohrung"},{"text","Text"},{"ellipse","Kreis"},{"track","Leiterbahn"}};name=names.contains(type)?ui(names.value(type)):type;
        const QPointF at(n["x"].toDouble(),n["y"].toDouble()),end(n["x2"].toDouble(),n["y2"].toDouble());
        if(type=="wire")detail=wire(QLineF(at,end).length(),true);else if(type=="lead"){path.prepend(QPointF());detail=wire(length(),false);}
        else if(type=="polyline"||type=="polygon")detail=ui("(%1 Knoten)").arg(path.size());else if(type=="rectangle"||type=="ellipse"){const auto r=QRectF(at,end).normalized();detail="("+mm(r.width())+" x "+mm(r.height())+" mm)";}
        else if(type=="drill")detail="("+locale.toString(n["diameter"].toDouble(.9),'g',6)+" mm)";else if(type=="text")detail="\""+n["text"].toString()+"\"";
    }
    if(!n["label"].toString().isEmpty()&&type!="TDraht")name=n["label"].toString();
    return {name,detail};
}
}
