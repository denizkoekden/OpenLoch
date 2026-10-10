#include "frontpanel.h"
#include "documents/projectfile.h"
#include "language.h"
#include "legacy_reader.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <cmath>

namespace openloch::frontpanel {
namespace {
constexpr qint64 maxFileSize=128*1024*1024;
constexpr int maxDepth=32,maxPoints=200000,maxElements=200000,maxPanels=100;
constexpr double maxSize=10000;  // mm, far beyond any front panel

const QStringList elementTypes{"line","polygon","rectangle","ellipse","arc","text","drill","image","picture","group","dimension","scale","cutout"};
const QStringList penStyles{"solid","dot","dash","dashdot","none"};
const QStringList fillStyles{"solid","none","horizontal","vertical","fdiagonal","bdiagonal","cross","diagcross"};
const QStringList gradients{"none","horizontal","vertical","diagonal","crossdiagonal"};
const QStringList cornerStyles{"sharp","spline","chamfer","round","arcspline"};
const QStringList machinings{"none","mill","engrave"};
const QStringList arcStyles{"open","pie","chord"};

[[noreturn]] void fail(const QString &why){throw FormatError(why);}
QJsonValue number(double v){return std::isfinite(v)?QJsonValue(v):QJsonValue(0);}
QJsonArray point(QPointF p){return QJsonArray{number(p.x()),number(p.y())};}
QJsonArray polygon(const QPolygonF &p){QJsonArray a;for(auto q:p)a.append(point(q));return a;}
QString colour(const QColor &c){return c.alpha()==255?c.name(QColor::HexRgb):c.name(QColor::HexArgb);}

// Strict readers: a missing optional field takes the default, a present one must be valid.
struct In {
    const QJsonObject &o;
    double real(const char *key,double fallback,double low=-maxSize,double high=maxSize) const{
        if(!o.contains(key))return fallback;const auto v=o[key];
        if(!v.isDouble()||!std::isfinite(v.toDouble())||v.toDouble()<low||v.toDouble()>high)fail(ui("Ungültiger Zahlenwert: %1").arg(key));
        return v.toDouble();
    }
    bool flag(const char *key,bool fallback=false) const{
        if(!o.contains(key))return fallback;if(!o[key].isBool())fail(ui("Ungültiger Wahrheitswert: %1").arg(key));return o[key].toBool();
    }
    QString text(const char *key,const QString &fallback={},int limit=1<<20) const{
        if(!o.contains(key))return fallback;if(!o[key].isString()||o[key].toString().size()>limit)fail(ui("Ungültiger Text: %1").arg(key));return o[key].toString();
    }
    QColor colour(const char *key,const QColor &fallback) const{
        if(!o.contains(key))return fallback;const QColor c=QColor::fromString(text(key));if(!c.isValid())fail(ui("Ungültige Farbe: %1").arg(key));return c;
    }
    int choice(const char *key,const QStringList &names,int fallback=0) const{
        if(!o.contains(key))return fallback;const int i=names.indexOf(text(key));if(i<0)fail(ui("Unbekannter Wert für %1: %2").arg(key,o[key].toString()));return i;
    }
    QJsonObject object(const char *key) const{
        if(!o.contains(key))return {};if(!o[key].isObject())fail(ui("Ungültiges Objekt: %1").arg(key));return o[key].toObject();
    }
    QPointF at(const char *key,QPointF fallback={}) const{
        if(!o.contains(key))return fallback;const auto a=o[key].toArray();
        if(!o[key].isArray()||a.size()!=2||!a[0].isDouble()||!a[1].isDouble())fail(ui("Ungültiger Punkt: %1").arg(key));
        const QPointF p(a[0].toDouble(),a[1].toDouble());if(!std::isfinite(p.x())||!std::isfinite(p.y())||std::abs(p.x())>maxSize||std::abs(p.y())>maxSize)fail(ui("Ungültiger Punkt: %1").arg(key));
        return p;
    }
    QPolygonF points(const char *key,int count=-1) const{
        QPolygonF out;if(!o.contains(key)){if(count>0)fail(ui("Punkte fehlen: %1").arg(key));return out;}
        if(!o[key].isArray())fail(ui("Ungültige Punktliste: %1").arg(key));const auto a=o[key].toArray();
        if(a.size()>maxPoints||(count>=0&&a.size()!=count))fail(ui("Ungültige Punktanzahl: %1").arg(key));
        for(const auto &v:a){const QJsonObject wrap{{"p",v}};out<<In{wrap}.at("p");}
        return out;
    }
};

QJsonObject printJson(const PrintSettings &s){
    return {{"mirror",s.mirror},{"background",s.background},{"frame",s.frame},{"rulers",s.rulers},{"data",s.data},{"dimensions",s.dimensions},
        {"machining",s.machining},{"objects",s.objects},{"cutMarks",s.cutMarks},{"texts",s.texts},{"original",s.original},{"zoom",number(s.zoom)},
        {"centred",s.centred},{"onlyOne",s.onlyOne},{"sheet",s.sheet},{"landscape",s.landscape},
        {"tiles",QJsonArray{s.tilesX,s.tilesY}},{"gap",point({s.gapX,s.gapY})},{"offset",point({s.left,s.top})}};
}
PrintSettings printOf(const QJsonObject &o){
    In in{o};PrintSettings s;
    for(auto [key,flag]:{std::pair{"mirror",&s.mirror},{"background",&s.background},{"frame",&s.frame},{"rulers",&s.rulers},{"data",&s.data},
            {"dimensions",&s.dimensions},{"machining",&s.machining},{"objects",&s.objects},{"cutMarks",&s.cutMarks},{"texts",&s.texts},
            {"original",&s.original},{"centred",&s.centred},{"onlyOne",&s.onlyOne},{"landscape",&s.landscape}})*flag=in.flag(key,*flag);
    s.zoom=in.real("zoom",s.zoom,0.01,100);s.sheet=int(in.real("sheet",s.sheet,1,1e6));
    const QPointF tiles=in.at("tiles",{1,1});
    if(tiles.x()!=std::floor(tiles.x())||tiles.y()!=std::floor(tiles.y())||std::min(tiles.x(),tiles.y())<1||std::max(tiles.x(),tiles.y())>PrintSettings::maxTiles)
        fail(ui("Ungültiger Zahlenwert: %1").arg("tiles"));
    s.tilesX=int(tiles.x());s.tilesY=int(tiles.y());
    const QPointF gap=in.at("gap",{0,0});if(gap.x()<0||gap.y()<0)fail(ui("Ungültiger Zahlenwert: %1").arg("gap"));
    s.gapX=gap.x();s.gapY=gap.y();
    const QPointF offset=in.at("offset",{s.left,s.top});s.left=offset.x();s.top=offset.y();
    return s;
}

QJsonObject penJson(const Pen &p){return {{"color",colour(p.color)},{"width",number(p.width)},{"style",penStyles[int(p.style)]}};}
Pen penOf(const QJsonObject &o){In in{o};Pen p;p.color=in.colour("color",p.color);p.width=in.real("width",p.width,0,100);p.style=PenStyle(in.choice("style",penStyles));return p;}
QJsonObject fillJson(const Fill &f){return {{"style",fillStyles[int(f.style)]},{"color",colour(f.color)},{"color2",colour(f.color2)},{"gradient",gradients[int(f.gradient)]}};}
Fill fillOf(const QJsonObject &o){In in{o};Fill f;f.style=FillStyle(in.choice("style",fillStyles,1));f.color=in.colour("color",f.color);f.color2=in.colour("color2",f.color2);f.gradient=Gradient(in.choice("gradient",gradients));return f;}

QJsonObject elementJson(const Element &e,bool identity=true){
    QJsonObject o{{"type",typeName(e.type)}};if(identity)o["id"]=e.id;
    if(!e.name.isEmpty())o["name"]=e.name;
    if(identity&&!e.component.isEmpty())o["component"]=e.component;
    o["pen"]=penJson(e.pen);o["fill"]=fillJson(e.fill);
    if(e.machining!=Machining::None)o["machining"]=machinings[int(e.machining)];
    switch(e.type){
    case ElementType::Line:case ElementType::Polygon:case ElementType::Rectangle:
        o["points"]=polygon(e.points);
        if(e.contour.corners!=Corners::Sharp)o["contour"]=QJsonObject{{"corners",cornerStyles[int(e.contour.corners)]},{"size",number(e.contour.size)}};
        break;
    case ElementType::Ellipse:case ElementType::Arc:
        o["center"]=point(e.center);o["radiusX"]=number(e.radiusX);o["radiusY"]=number(e.radiusY);o["rotation"]=number(e.rotation);
        if(e.type==ElementType::Arc){o["startAngle"]=number(e.startAngle);o["spanAngle"]=number(e.spanAngle);o["arcStyle"]=arcStyles[int(e.arcStyle)];}
        break;
    case ElementType::Text:
        o["frame"]=polygon(e.frame);o["text"]=e.text;o["font"]=e.font;
        if(e.bold)o["bold"]=true;if(e.italic)o["italic"]=true;if(e.underline)o["underline"]=true;if(e.strikeOut)o["strikeOut"]=true;
        if(!e.strokeFont.isEmpty())o["strokeFont"]=e.strokeFont;
        break;
    case ElementType::Drill:
        o["center"]=point(e.center);o["diameter"]=number(e.diameter);break;
    case ElementType::Image:case ElementType::Picture:
        o["frame"]=polygon(e.frame);o["resource"]=e.resource;
        if(e.transparent){o["transparent"]=true;o["transparentColor"]=colour(e.transparentColor);}
        break;
    default:{QJsonArray children;for(const auto &c:e.children)children.append(elementJson(c,identity));o["children"]=children;
        if(!e.parameters.isEmpty())o["parameters"]=e.parameters;}
    }
    if(e.hasAnchor)o["anchor"]=point(e.anchor);
    if(identity&&!e.foreign.isEmpty())o["foreign"]=e.foreign;
    return o;
}

Element elementOf(const QJsonObject &o,int depth,int &count,const QMap<QString,Resource> &resources){
    if(depth>maxDepth)fail(ui("Gruppen sind zu tief verschachtelt"));
    if(++count>maxElements)fail(ui("Zu viele Objekte"));
    In in{o};Element e;
    e.type=ElementType(in.choice("type",elementTypes,-1));if(!o.contains("type"))fail(ui("Objekt ohne Typ"));
    e.id=in.text("id",{},200);if(e.id.isEmpty())e.id=newId();
    e.name=in.text("name",{},10000);
    e.component=in.text("component",{},64);if(!e.component.isEmpty()&&!documents::isId(e.component))fail(ui("Ungültige Bauteilkennung: %1").arg(e.component));
    e.pen=penOf(in.object("pen"));e.fill=fillOf(in.object("fill"));e.machining=Machining(in.choice("machining",machinings));
    e.foreign=in.object("foreign");
    if(o.contains("anchor")){e.hasAnchor=true;e.anchor=in.at("anchor");}
    switch(e.type){
    case ElementType::Line:case ElementType::Polygon:case ElementType::Rectangle:{
        e.points=in.points("points");if(e.points.size()<(e.type==ElementType::Line?2:3)||(e.type==ElementType::Rectangle&&e.points.size()!=4))fail(ui("Ungültige Punktanzahl: %1").arg("points"));
        const auto contour=in.object("contour");In c{contour};e.contour.corners=Corners(c.choice("corners",cornerStyles));e.contour.size=c.real("size",e.contour.size,0,1000);
        break;}
    case ElementType::Ellipse:case ElementType::Arc:
        e.center=in.at("center");e.radiusX=in.real("radiusX",0,0);e.radiusY=in.real("radiusY",0,0);e.rotation=in.real("rotation",0,-1e6,1e6);
        if(e.type==ElementType::Arc){e.startAngle=in.real("startAngle",0,-1e6,1e6);e.spanAngle=in.real("spanAngle",90,-360,360);e.arcStyle=ArcStyle(in.choice("arcStyle",arcStyles));}
        break;
    case ElementType::Text:
        e.frame=in.points("frame",3);e.text=in.text("text",{},100000);e.font=in.text("font","Arial",200);
        e.bold=in.flag("bold");e.italic=in.flag("italic");e.underline=in.flag("underline");e.strikeOut=in.flag("strikeOut");e.strokeFont=in.text("strokeFont",{},200);
        break;
    case ElementType::Drill:
        e.center=in.at("center");e.diameter=in.real("diameter",0,0,1000);break;
    case ElementType::Image:case ElementType::Picture:
        e.frame=in.points("frame",3);e.resource=in.text("resource",{},200);
        if(!resources.contains(e.resource))fail(ui("Unbekannte Ressource: %1").arg(e.resource));
        e.transparent=in.flag("transparent");e.transparentColor=in.colour("transparentColor",e.transparentColor);
        break;
    default:{
        if(o.contains("children")&&!o["children"].isArray())fail(ui("Ungültige Teileliste"));
        for(const auto &v:o["children"].toArray()){if(!v.isObject())fail(ui("Ungültige Teileliste"));e.children.append(elementOf(v.toObject(),depth+1,count,resources));}
        e.parameters=in.object("parameters");}
    }
    return e;
}

void collectResources(const Element &e,QSet<QString> &used){if(!e.resource.isEmpty())used.insert(e.resource);for(const auto &c:e.children)collectResources(c,used);}
}

bool Element::closed() const{
    return combined()||type==ElementType::Polygon||type==ElementType::Rectangle||type==ElementType::Ellipse||(type==ElementType::Arc&&arcStyle!=ArcStyle::Open);
}

QString newId(){return QUuid::createUuid().toString(QUuid::Id128);}
QJsonObject elementToJson(const Element &element,bool withIdentity){return elementJson(element,withIdentity);}
QString typeName(ElementType type){return elementTypes.value(int(type));}
QString typeTitle(ElementType type){
    switch(type){
    case ElementType::Line:return ui("Linie");case ElementType::Polygon:return ui("Polygon");case ElementType::Rectangle:return ui("Rechteck");
    case ElementType::Ellipse:return ui("Ellipse");case ElementType::Arc:return ui("Bogen");case ElementType::Text:return ui("Text");
    case ElementType::Drill:return ui("Bohrung");case ElementType::Image:return ui("Bild");case ElementType::Picture:return ui("Vektorgrafik");
    case ElementType::Group:return ui("Gruppe");case ElementType::Dimension:return ui("Bemaßung");case ElementType::Scale:return ui("Skala");
    case ElementType::Cutout:return ui("Ausschnitt");
    }
    return {};
}

Panel newPanel(const QString &name,double width,double height){Panel p;p.id=newId();p.name=name;p.width=width;p.height=height;return p;}
Element newElement(ElementType type){
    Element e;e.id=newId();e.type=type;
    if(type==ElementType::Drill){e.pen=Pen{Qt::black,0.2,PenStyle::Solid};e.fill=Fill{FillStyle::Solid,Qt::white,Qt::white,Gradient::None};e.diameter=3;e.machining=Machining::Mill;}
    if(type==ElementType::Text){e.pen.style=PenStyle::None;e.pen.width=0;e.fill=Fill{FillStyle::Solid,Qt::black,Qt::black,Gradient::None};}
    return e;
}
void renewIds(Element &element,QHash<QString,QString> *components){
    QHash<QString,QString> own;auto &renamed=components?*components:own;
    element.id=newId();
    if(!element.component.isEmpty()){auto it=renamed.find(element.component);if(it==renamed.end())it=renamed.insert(element.component,newId());element.component=*it;}
    for(auto &c:element.children)renewIds(c,&renamed);
}
QTransform BoardBehind::toPanel() const{
    // Applied to a point in the reverse order: mirrored, turned, moved.
    QTransform t;t.translate(offset.x(),offset.y());t.rotate(-rotation);if(solderSide)t.scale(-1,1);return t;
}

Document::Document(){id=newId();title=ui("Neue Frontplatte");panels.append(newPanel(ui("Neue Frontplatte"),100,100));}   // the original's default

QString Document::addResource(const QByteArray &data,const QString &kind){
    const QString key=QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex());
    resources.insert(key,Resource{kind,data});return key;
}

QJsonObject Document::toJson() const{
    QJsonArray pages;QSet<QString> used;
    for(const auto &p:panels){
        QJsonArray elements;for(const auto &e:p.elements){elements.append(elementJson(e));collectResources(e,used);}
        QJsonObject o{{"id",p.id},{"name",p.name},{"width",number(p.width)},{"height",number(p.height)},
            {"color",colour(p.color)},{"color2",colour(p.color2)},{"gradient",gradients[int(p.gradient)]},
            {"grid",QJsonObject{{"size",number(p.grid)},{"visible",p.gridVisible},{"snap",p.snap},{"color",colour(p.gridColor)}}},
            {"origin",point(p.origin)},{"elements",elements}};
        if(p.inch)o["inch"]=true;
        if(!p.boards.isEmpty()){
            QJsonArray list;
            for(const auto &b:p.boards)list.append(QJsonObject{{"document",b.document},{"board",b.board},{"offset",point(b.offset)},{"rotation",number(b.rotation)},{"side",b.solderSide?"bottom":"top"}});
            o["boards"]=list;
        }
        if(!(p.print==PrintSettings()))o["print"]=printJson(p.print);
        if(!p.foreign.isEmpty())o["foreign"]=p.foreign;
        pages.append(o);
    }
    QJsonObject stored;
    for(auto it=resources.begin();it!=resources.end();++it)if(used.contains(it.key()))stored[it.key()]=QJsonObject{{"kind",it->kind},{"data",QString::fromLatin1(it->data.toBase64())}};
    QJsonObject root{{"format","OpenLoch-Frontplatte"},{"version",schemaVersion},{"id",id},{"title",title},{"revision",revision},
        {"activePanel",activePanel},{"panels",pages}};
    if(!stored.isEmpty())root["resources"]=stored;
    if(!views.isEmpty()){QJsonArray list;for(const auto &v:views)list.append(QJsonObject{{"name",v.name},{"panel",v.panel},{"area",QJsonArray{number(v.area.x()),number(v.area.y()),number(v.area.width()),number(v.area.height())}}});root["views"]=list;}
    if(!foreign.isEmpty())root["foreign"]=foreign;
    return root;
}
QByteArray Document::encode() const{return QJsonDocument(toJson()).toJson(QJsonDocument::Indented);}

Document Document::decode(const QByteArray &data){
    if(data.size()>maxFileSize)fail(ui("Datei ist zu groß"));
    QJsonParseError error;const auto json=QJsonDocument::fromJson(data,&error);
    if(error.error!=QJsonParseError::NoError||!json.isObject())fail(ui("Keine gültige Frontplattendatei: %1").arg(error.errorString()));
    return fromJson(json.object());
}
Document Document::fromJson(const QJsonObject &root){
    In in{root};
    if(root["format"]!="OpenLoch-Frontplatte")fail(ui("Keine Frontplattendatei"));
    if(!root["version"].isDouble()||root["version"].toInt()<1)fail(ui("Ungültige Formatversion"));
    if(root["version"].toInt()>schemaVersion)fail(ui("Die Datei stammt aus einer neueren OpenLoch-Version (Format %1)").arg(root["version"].toInt()));
    Document d;d.panels.clear();
    d.id=in.text("id",{},200);if(d.id.isEmpty())d.id=newId();
    d.title=in.text("title",{},10000);d.revision=int(in.real("revision",0,0,2e9));d.foreign=in.object("foreign");
    for(const auto &key:in.object("resources").keys()){
        const auto r=in.object("resources")[key].toObject();In ri{r};const auto kind=ri.text("kind");
        if(!QStringList{"bmp","png","jpg","emf"}.contains(kind))fail(ui("Unbekannte Ressourcenart: %1").arg(kind));
        const auto bytes=QByteArray::fromBase64(ri.text("data",{},maxFileSize).toLatin1());
        d.resources.insert(key,Resource{kind,bytes});
    }
    if(!root["panels"].isArray()||root["panels"].toArray().isEmpty()||root["panels"].toArray().size()>maxPanels)fail(ui("Ungültige Frontplattenliste"));
    int count=0;
    for(const auto &v:root["panels"].toArray()){
        if(!v.isObject())fail(ui("Ungültige Frontplattenliste"));const auto o=v.toObject();In p{o};Panel panel;
        panel.id=p.text("id",{},200);if(panel.id.isEmpty())panel.id=newId();
        panel.name=p.text("name",{},1000);panel.width=p.real("width",100,1,maxSize);panel.height=p.real("height",50,1,maxSize);
        panel.color=p.colour("color",panel.color);panel.color2=p.colour("color2",panel.color2);panel.gradient=Gradient(p.choice("gradient",gradients));
        const auto grid=p.object("grid");In g{grid};panel.grid=g.real("size",1,0.001,1000);panel.gridVisible=g.flag("visible",true);panel.snap=g.flag("snap",true);panel.gridColor=g.colour("color",panel.gridColor);
        panel.origin=p.at("origin");panel.inch=p.flag("inch");panel.print=printOf(p.object("print"));panel.foreign=p.object("foreign");
        if(o.contains("boards")&&(!o["boards"].isArray()||o["boards"].toArray().size()>maxPanels))fail(ui("Ungültige Platinenliste"));
        for(const auto &v:o["boards"].toArray()){
            if(!v.isObject())fail(ui("Ungültige Platinenliste"));const auto b=v.toObject();In bi{b};BoardBehind board;
            board.document=bi.text("document",{},64);board.board=bi.text("board",{},64);
            if(!documents::isId(board.document)||!documents::isId(board.board))fail(ui("Ungültige Platine hinter der Frontplatte"));
            board.offset=bi.at("offset");board.rotation=bi.real("rotation",0,-360,360);board.solderSide=bi.choice("side",{"top","bottom"})==1;
            for(const auto &other:panel.boards)if(other.document==board.document&&other.board==board.board)fail(ui("Platine zweimal hinter derselben Frontplatte"));
            panel.boards.append(board);
        }
        if(o.contains("elements")&&!o["elements"].isArray())fail(ui("Ungültige Objektliste"));
        for(const auto &e:o["elements"].toArray()){if(!e.isObject())fail(ui("Ungültige Objektliste"));panel.elements.append(elementOf(e.toObject(),0,count,d.resources));}
        d.panels.append(panel);
    }
    d.activePanel=int(in.real("activePanel",0,0,d.panels.size()-1));
    for(const auto &v:root["views"].toArray()){
        const auto o=v.toObject();In vi{o};View view;view.name=vi.text("name",{},1000);view.panel=int(vi.real("panel",0,0,d.panels.size()-1));
        const auto a=o["area"].toArray();if(a.size()!=4)fail(ui("Ungültige Ansicht"));view.area=QRectF(a[0].toDouble(),a[1].toDouble(),a[2].toDouble(),a[3].toDouble());
        if(!view.area.isValid()||!std::isfinite(view.area.width())||!std::isfinite(view.area.height()))fail(ui("Ungültige Ansicht"));d.views.append(view);
    }
    return d;
}

Document Document::load(const QString &path){
    QFile file(path);if(!file.open(QIODevice::ReadOnly))fail(file.errorString());
    if(file.size()>maxFileSize)fail(ui("Datei ist zu groß"));
    return decode(file.readAll());
}

void Document::save(const QString &path) const{
    const auto data=encode();decode(data);  // never write what could not be read back
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))fail(file.errorString());
    if(file.write(data)!=data.size()||!file.commit())fail(file.errorString());
}
}
