#include "legacy_writer.h"
#include "language.h"
#include "legacy_reader.h"
#include "project.h"
#include "board.h"
#include "geometry.h"
#include "notes.h"
#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHash>
#include <QtEndian>
#include <cmath>
#include <array>
#include <limits>
#include <functional>

namespace openloch {
namespace {
QPointF point(const QJsonArray &a,int i=0){return {a[i].toDouble(),a[i+1].toDouble()};}
QJsonArray pair(QPointF p){return {qRound(p.x()),qRound(p.y())};}
QByteArray slice(const QByteArray &source,qint64 start,qint64 length){
    if(start<0||length<0||start>source.size()||length>source.size()-start)throw FormatError(ui("Ungültiger Quellbereich beim LM4-Export"));
    return source.mid(start,length);
}
// Drills and pads carry pen 0 in the original's files (it rewrites them so when saving).
QJsonObject base(const QString &type,int kind=9,int width=25){
    return {{"type",type},{"kind",kind},{"width",width},{"pen",type=="TBohrung"||type=="TAuge"?0:0x004d6228},{"brush",0},
        {"transparent",false},{"flag",false},{"points",QJsonArray{}},{"back",false},
        {"flag2",false},{"style",0},{"rotation",200},{"flag3",false},
        {"anchors",QJsonArray{0,0,0,0,0,0}},{"label",""},{"extra",QJsonArray{}}};
}
QJsonObject group(const QJsonArray &children={}){
    auto o=base("TGruppe",8,1);o["children"]=children;o["id"]="";o["value"]="";
    o["description"]="";o["group_value"]=1;o["group_flags"]=QJsonArray{true,true};return o;
}
// The board's own group for its extra fields, as the original writes it for a new board.
QJsonObject boardMetadata(){auto m=group();m["width"]=0;m["group_value"]=0;m["description"]="Extrafelder von TPlatine";return m;}
QJsonArray flatPairs(const QJsonArray &a,const QTransform &t){
    QJsonArray result;for(int i=0;i+1<a.size();i+=2){auto p=pair(t.map(point(a,i)));result.append(p[0]);result.append(p[1]);}return result;
}
// The original writer enumerates the virtual terminal count, rather than the
// stored cache length. Missing entries therefore make a loaded component fail
// when saved in LochMaster. Keep valid existing caches (which can contain grid
// indices or stale coordinates), but supply physical endpoints for new records.
// Preserve duplicate terminals: a zero-length wire still has two cache entries.
QJsonArray terminalCache(const QJsonObject &o){
    QJsonArray result;
    if(o.contains("children")){
        for(auto child:o["children"].toArray())for(auto p:terminalCache(child.toObject()))result.append(p);
    }else if(QStringList{"TDraht","TDrahtFest","TLeiterbahn"}.contains(o["type"].toString())){
        const int kind=o["kind"].toInt();const auto path=o["path"].toArray();
        if(kind==1||kind==9||kind==11||kind==18||kind==19){
            if(path.isEmpty())throw FormatError(ui("Anschluss ohne Koordinaten beim LM4-Export"));
            result.append(path.first());if(kind==1)result.append(path.last());
        }
    }
    return result;
}
// Width of a text at cell height h in Arial, from the font's advance widths (1/1000 em; the cell is 1.117 em). The core
// has no font metrics; this only places mirrored labels, the original measures the text itself.
double arialWidth(const QString &text,double h){
    static const int advance[95]={278,278,355,556,556,889,667,191,333,333,389,584,278,333,278,278,556,556,556,556,556,556,556,556,556,556,278,278,584,584,584,556,
        1015,667,667,722,722,667,611,778,722,278,500,667,556,833,722,778,667,778,722,667,611,722,667,944,667,667,611,278,278,278,469,556,
        333,556,556,500,556,556,278,556,556,222,222,500,222,833,556,556,556,556,333,500,278,556,500,722,500,500,500,334,260,334,584};
    double sum=0;for(auto c:text)sum+=c.unicode()>=32&&c.unicode()<127?advance[c.unicode()-32]:556;return sum/1000*h/1.117;
}
// A label's box P0 (top left), P1, P2, P3 (bottom left); a label without stored corners is laid out from its angle.
// The corners of a label as written: those it has or, without any (files before 4.04, or zeros in their place while the
// label is not at the zero point), collapsed at P0 with the snapshot at S0, as the original reads such a label.
QJsonArray labelAnchors(const QJsonObject &o){
    QJsonArray anchors=o["text_anchors"].toArray();const auto p0=o["text_position"].toArray(),s0=o["text_end"].toArray();
    bool zeros=anchors.size()==12;for(const auto v:std::as_const(anchors))zeros&=v.toInt()==0;
    if(anchors.size()!=12||(zeros&&(p0.at(0).toInt()||p0.at(1).toInt()))){anchors={};for(int k=0;k<3;k++)anchors<<p0.at(0)<<p0.at(1)<<s0.at(0)<<s0.at(1);}
    return anchors;
}
std::array<QPointF,4> labelBox(const QJsonObject &o,const QJsonObject &group,bool &collapsed){
    if(const auto c=labelStoredCorners(o)){collapsed=false;return *c;}
    const QPointF p0=point(o["text_position"].toArray());
    // The width of the text as displayed, e.g. "R8" for <BauteilKennung>.
    collapsed=true;const double h=o["text_kind"].toDouble(120),w=arialWidth(componentText(o["text"].toString(),group),h);double angle=o["text_height"].toDouble();QPointF start=p0;
    auto along=[&]{return QPointF(std::cos(angle),-std::sin(angle));};auto down=[&]{return QPointF(std::sin(angle),std::cos(angle));};
    if(o["text_flags"].toArray().at(1).toBool()){start=p0+w*along()+h*down();angle+=3.14159265358979323846;}
    return {start,start+w*along(),start+w*along()+h*down(),start+h*down()};
}
// `mirror` holds the placement's mirror flags (bit 0 horizontal, bit 1 vertical): like the original, a mirrored label
// keeps upright glyphs and only swaps its corners.
QJsonObject transformed(QJsonObject o,const QTransform &t,int mirror=0,const QJsonObject &group={}){
    for(auto key:{"path","second_path"})if(o.contains(key)){
        QJsonArray result;for(auto value:o[key].toArray()){
            result.append(pair(t.map(point(value.toArray()))));
        }o[key]=result;
    }
    if(!o.contains("anchors")){
        auto r=legacyBounds(o);o["anchors"]=QJsonArray{qRound(r.left()),qRound(r.top()),qRound(r.right()),qRound(r.top()),qRound(r.left()),qRound(r.bottom())};
    }
    for(auto key:{"anchors","center"})if(o.contains(key))o[key]=flatPairs(o[key].toArray(),t);
    for(auto key:{"rect","ellipse","ellipse2"})if(o.contains(key)){
        if(QString(key)=="rect"&&std::abs(t.m11()*t.m12())>1e-8)throw FormatError(ui("Trennstellen lassen sich im LM4-Format nur in 90°-Schritten drehen."));
        auto a=o[key].toArray();auto r=t.mapRect(QRectF(point(a),point(a,2)).normalized());
        o[key]=QJsonArray{qRound(r.left()),qRound(r.top()),qRound(r.right()),qRound(r.bottom())};
    }
    if(o["type"]=="TTextLabel"&&t.type()==QTransform::TxTranslate&&!mirror){
        // Moved as the original moves a label: its current corners and, while its snapshot is valid (text_flags[0]), the
        // snapshot's corners (text_end and every second corner) move; angle, snapshot angle and flags stay. A label of
        // a file before 4.04 has neither: its corners are P0 and its snapshot corners S0, as the original reads it.
        const QPointF d(t.dx(),t.dy());const bool snapshot=o["text_flags"].toArray().at(0).toBool();
        const QPointF p0=point(o["text_position"].toArray()),s0=point(o["text_end"].toArray());
        QJsonArray anchors=labelAnchors(o);
        for(int i=0;i<12;i+=2)if((i/2)%2==0||snapshot){anchors[i]=qRound(anchors[i].toDouble()+d.x());anchors[i+1]=qRound(anchors[i+1].toDouble()+d.y());}
        o["text_position"]=pair(p0+d);if(snapshot)o["text_end"]=pair(s0+d);o["text_anchors"]=anchors;
        return o;
    }
    if(o["type"]=="TTextLabel"&&!t.isIdentity()){
        // The original turns and mirrors the current corners and keeps angle, snapshot (text_end and every second corner)
        // and flags. A label without stored corners stays collapsed at its new top-left corner with the new angle
        // (counter-clockwise on screen), so that the original lays it out again when loading; its snapshot is taken anew.
        bool collapsed=false;auto c=labelBox(o,group,collapsed);for(auto &p:c)p=t.map(p);
        if(mirror&1)c={c[1],c[0],c[3],c[2]};if(mirror&2)c={c[3],c[2],c[1],c[0]};
        // Mirroring toggles the label's two mirror flags (horizontal, vertical); they are saved but never drawn.
        if(mirror){auto toggles=o["text_flags2"].toArray();while(toggles.size()<2)toggles.append(false);for(int k:{0,1})if(mirror&(1<<k))toggles[k]=!toggles[k].toBool();o["text_flags2"]=toggles;}
        if(!collapsed){
            QJsonArray anchors=o["text_anchors"].toArray();
            for(int k=1;k<4;k++){const auto at=pair(c[k]);anchors[(k-1)*4]=at[0];anchors[(k-1)*4+1]=at[1];}
            o["text_position"]=pair(c[0]);o["text_anchors"]=anchors;
            return o;
        }
        const auto v=c[1]-c[0];double angle=std::atan2(-v.y(),v.x());if(angle<0)angle+=2*3.14159265358979323846;
        QJsonArray corners;const auto at=pair(c[0]);for(int k=0;k<6;k++){corners.append(at[0]);corners.append(at[1]);}
        o["text_position"]=at;o["text_end"]=at;o["text_anchors"]=corners;o["text_height"]=angle;o["text_width"]=angle;
        auto flags=o["text_flags"].toArray();if(flags.size()<2)flags={true,false};flags[1]=false;o["text_flags"]=flags;
        return o;
    }
    if(o.contains("children")){const auto context=o["id"].toString().isEmpty()?group:o;QJsonArray a;for(auto value:o["children"].toArray())a.append(transformed(value.toObject(),t,mirror,context));o["children"]=a;}
    if(o.contains("inner"))o["inner"]=transformed(o["inner"].toObject(),t,mirror);
    return o;
}
// A user origin set in OpenLoch, in file coordinates (board offset added) as two little-endian integers.
QByteArray originBytes(const Project &p,QPointF offset){
    if(p.userOrigin.size()!=2)return {};QByteArray b(8,0);const auto at=p.origin()+offset;
    qToLittleEndian<qint32>(qRound(at.x()),b.data());qToLittleEndian<qint32>(qRound(at.y()),b.data()+4);return b;
}
int mirrorFlags(const QJsonObject &properties){return (properties["mirrorX"].toBool()?1:0)|(properties["mirrorY"].toBool()?2:0);}
QTransform placementTransform(const QJsonObject &properties,QPointF anchor,QPointF offset){
    auto t=objectTransform(properties,anchor);t.setMatrix(t.m11(),t.m12(),t.m13(),t.m21(),t.m22(),t.m23(),t.dx()+offset.x(),t.dy()+offset.y(),t.m33());return t;
}

class Stream {
public:
    QByteArray bytes;
    void byte(quint8 v){bytes.append(char(v));}
    template<class T> void rawNumber(T v){char b[sizeof(T)];qToLittleEndian(v,b);bytes.append(b,sizeof(T));}
    void integer(int v){
        if(v>=-128&&v<=127){byte(2);byte(quint8(v));}
        else if(v>=-32768&&v<=32767){byte(3);rawNumber<qint16>(v);}
        else{byte(4);rawNumber<qint32>(v);}
    }
    void boolean(bool v){byte(v?9:8);}
    void number(double v){
        if(!std::isfinite(v))throw FormatError(ui("Ungültige Zahl beim LM4-Export"));
        byte(5);int exponent=0;auto fraction=std::frexp(std::abs(v),&exponent);
        rawNumber<quint64>(quint64(std::ldexp(fraction,64)));
        rawNumber<quint16>((v?exponent-1+16383:0)|(std::signbit(v)?0x8000:0));
    }
    void string(const QString &v){auto b=v.toUtf8();byte(20);rawNumber<qint32>(b.size());bytes.append(b);}
    void integers(const QJsonArray &a,int count,int fallback=0){for(int i=0;i<count;i++)integer(a[i].toInt(fallback));}
    void booleans(const QJsonArray &a,int count,bool fallback=false){for(int i=0;i<count;i++)boolean(a[i].toBool(fallback));}
    void points(const QJsonArray &a){
        integer(a.size()-1);
        for(auto v:a){
            const auto p=v.toArray();
            if(p.size()!=2||!p[0].isDouble()||!p[1].isDouble())throw FormatError(ui("Ungültiges Koordinatenpaar beim LM4-Export"));
            rawNumber<qint32>(p[0].toInt());rawNumber<qint32>(p[1].toInt());
        }
    }
    void object(const QJsonObject &o,const QByteArray &source,bool prefix=true,int depth=0){
        if(depth>64)throw FormatError(ui("Zu viele verschachtelte Objekte beim LM4-Export"));
        auto type=o["type"].toString();if(prefix)string(type);
        integer(o["kind"].toInt());integer(o["width"].toInt());rawNumber<quint32>(quint32(o["pen"].toInteger()));rawNumber<quint32>(quint32(o["brush"].toInteger()));
        boolean(o["transparent"].toBool());boolean(o["flag"].toBool());
        const auto terminals=terminalCache(o);const auto cache=o["points"].toArray();
        points(cache.size()==terminals.size()?cache:terminals);boolean(o["back"].toBool());
        const auto picture=QByteArray::fromBase64(o["bitmap"].toString().toLatin1());const auto bitmapSize=picture.isEmpty()?o["bitmap_size"].toInteger():picture.size();boolean(bitmapSize>0);
        if(!picture.isEmpty())bytes.append(picture);else if(bitmapSize>0)bytes.append(slice(source,o["bitmap_offset"].toInteger(),bitmapSize));
        boolean(o["flag2"].toBool());integer(o["style"].toInt());number(o["rotation"].toDouble(200));boolean(o["flag3"].toBool());
        integers(o["anchors"].toArray(),6);string(o["label"].toString());auto extra=o["extra"].toArray();integer(extra.size());
        for(auto v:extra){auto a=v.toArray();string(a[0].toString());boolean(a[1].toBool());string(a[2].toString());}
        if(type=="TGruppe"||type=="TFarbcode"||type=="TBt"){
            auto children=o["children"].toArray();integer(children.size()-1);string(o["id"].toString());string(o["value"].toString());string(o["description"].toString());integer(o["group_value"].toInt());
            booleans(o["group_flags"].toArray(),2);for(auto child:children)object(child.toObject(),source,true,depth+1);
            if(type=="TFarbcode"){integer(o["bands"].toInt());number(o["resistance"].toDouble());}
            if(type=="TBt")byte(o["component_kind"].toInt());
        }else if(type=="TBohrung"||type=="TAuge"){number(o["diameter"].toDouble());integers(o["center"].toArray(),2);}
        else if(type=="TDraht"||type=="TDrahtFest"||type=="TLeiterbahn"){points(o["path"].toArray());boolean(o.contains("second_path"));if(o.contains("second_path"))points(o["second_path"].toArray());}
        else if(type=="TTrenner"||type=="TTrennerFest"){auto a=o["rect"].toArray();for(int i=0;i<4;i++)rawNumber<qint32>(a[i].toInt());}
        else if(type=="TKreis"){integers(o["ellipse"].toArray(),4);boolean(o["ellipse_flag"].toBool());integers(o["ellipse2"].toArray(),4);object(o["inner"].toObject(),source,false,depth+1);}
        else if(type=="TTextLabel"){
            for(auto key:{"text_position","text_end"}){auto a=o[key].toArray();for(int i=0;i<2;i++)rawNumber<qint32>(a[i].toInt());}
            integer(o["text_kind"].toInt(120));number(o["text_height"].toDouble());number(o["text_width"].toDouble());string(o["text"].toString());byte(o["text_role"].toInt());booleans(o["text_flags"].toArray(),2);
            string(o["font"].toString("Arial"));booleans(o["font_style"].toArray(),4);integers(labelAnchors(o),12);booleans(o["text_flags2"].toArray(),2);integer(o["text_metric"].toInt());
        }else if(type!="TBauteil")throw FormatError(ui("Nicht unterstützte Objektklasse beim LM4-Export: ")+type);
    }
    void header(const QString &title,const QString &description){
        // Delphi's compatibility header remains an ANSI ShortString. The full
        // Unicode name is written separately with TWriter's UTF-8 string tag.
        static const QString cp=QString::fromUtf8("€\u0081‚ƒ„…†‡ˆ‰Š‹Œ\u008dŽ\u008f\u0090‘’“”•–—˜™š›œ\u009džŸ");
        QByteArray name;for(auto c:title){int at=cp.indexOf(c);name.append(char(at>=0?at+128:c.unicode()<=255?c.unicode():'?'));if(name.size()==35)break;}
        QByteArray h(41,0);h[0]=char(name.size());h.replace(1,name.size(),name);h.replace(37,4,"4,07");bytes.append(h);string(description);
    }
    void tail(const QJsonObject &doc,const QByteArray &source,const QByteArray &overrideNotes={},const QByteArray &origin={}){
        const auto start=bytes.size();pendingRawPoints=-1;tailShift=0;tailBody(doc,source,overrideNotes);
        // A user origin replaces the first eight bytes of the raw point block, also in a tail copied unchanged.
        if(origin.size()!=8)return;
        if(pendingRawPoints>=0)bytes.replace(pendingRawPoints,8,origin);
        else if(doc.contains("raw_points_offset"))bytes.replace(start+doc["raw_points_offset"].toInteger()-doc["tail_offset"].toInteger()+tailShift,8,origin);
    }
    qint64 pendingRawPoints=-1,tailShift=0; // tailShift: bytes added by replacing the print views of a copied tail
    void tailBody(const QJsonObject &doc,const QByteArray &source,const QByteArray &overrideNotes){
        // Version 4.07 tails can be preserved exactly, including unexposed RTF
        // notes, metadata, bookmarks and display settings.
        if(doc["version"]=="4.07"){
            const auto start=bytes.size();
            if(!overrideNotes.isEmpty()){bytes.append(slice(source,doc["tail_offset"].toInteger(),doc["annotations_length_offset"].toInteger()-doc["tail_offset"].toInteger()));integer(overrideNotes.size());bytes.append(overrideNotes);}
            else{auto end=doc["board"].toObject().isEmpty()?doc["end"].toInteger():doc["board"].toObject()["start"].toInteger();bytes.append(slice(source,doc["tail_offset"].toInteger(),end-doc["tail_offset"].toInteger()));}
            // Changed settings replace their parts of the copied tail, from the back so that the earlier offsets stay valid:
            // the two grids, the view block (its length depends on the values), the pitch and the board's metadata.
            const auto at=[&](const char *key){return start+doc[key].toInteger()-doc["tail_offset"].toInteger();};
            const auto replace=[&](qint64 from,qint64 to,const QByteArray &with){
                if(from<start||to<from||to>bytes.size())throw FormatError(ui("Platineneinstellungen passen nicht in die LM4-Datei"));bytes.replace(from,to-from,with);tailShift+=with.size()-(to-from);};
            const auto changed=doc["changed_settings"].toObject();const auto numbers=doc["numbers"].toArray();
            if(changed["gridInch"].toBool()){Stream n;n.number(numbers.at(1).toDouble());replace(at("numbers_second_offset"),at("raw_points_offset"),n.bytes);}
            if(changed["gridMm"].toBool()){Stream n;n.number(numbers.at(0).toDouble());replace(at("numbers_offset"),at("numbers_second_offset"),n.bytes);}
            if(doc["print_override"].toBool()&&doc.contains("views_offset")){Stream block;block.printViews(doc);replace(at("views_offset"),at("views_end"),block.bytes);}
            if(changed["pitch"].toBool()){Stream n;n.number(doc["grid_mm"].toDouble());replace(at("grid_offset"),at("views_offset"),n.bytes);}
            if(changed["metadata"].toBool()){Stream m;m.object(transformed(doc["metadata"].toObject(),QTransform()),source,false);replace(start,at("grid_offset"),m.bytes);}
            return;
        }
        auto metadata=doc["metadata"].toObject();if(metadata.isEmpty())metadata=boardMetadata();
        object(transformed(metadata,QTransform()),source,false);number(doc["grid_mm"].toDouble(2.54));
        printViews(doc);
        auto numbers=doc["numbers"].toArray();number(numbers.at(0).toDouble(.1));number(numbers.at(1).toDouble(.254));
        auto rawPoints=QByteArray::fromHex(doc["raw_points"].toString().toLatin1());if(rawPoints.size()!=16)rawPoints=QByteArray(16,0);bytes.append(rawPoints);pendingRawPoints=bytes.size()-16;
        auto notes=overrideNotes.isEmpty()?slice(source,doc["annotations_offset"].toInteger(),doc["annotations_size"].toInteger()):overrideNotes;
        if(notes.isEmpty())notes="{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Arial;}}\\f0\\fs20\\par}";
        integer(notes.size());bytes.append(notes);
    }
    // The main window's view and the ten print views, then count, sheet flags and sheet number of the print preview.
    void printViews(const QJsonObject &doc){
        auto views=doc["views"].toArray();for(int i=0;i<11;i++){
            // Record 0 is the main window's view, 1..10 the print views with the original's defaults for a new board:
            // print view 1 centred, the others cascaded 10 mm, Potenziale off.
            auto view=views.at(i).toObject();auto flags=view["flags"].toArray();if(flags.isEmpty())flags=QJsonArray{false,false,true,true,false,false,i==0};
            booleans(flags,7);number(view["scale"].toDouble(1));auto bounds=view["bounds"].toArray();if(bounds.isEmpty())bounds={0,0,1,1};integers(bounds,4);
            number(view["number1"].toDouble());number(view["number2"].toDouble());
            auto flags2=view["flags2"].toArray();if(flags2.isEmpty())flags2={true,true,true,true,true,true,true,i<=1,true,true};booleans(flags2,10,true);number(view["number3"].toDouble(1));
            auto ints=view["integers"].toArray();if(ints.isEmpty())ints={-1000*qMax(0,i-1),-1000*qMax(0,i-1),2};integers(ints,3);boolean(view["flag"].toBool(true));
        }
        integer(doc["view_index"].toInt(1));auto flags=doc["view_flags"].toArray();if(flags.isEmpty())flags={false,true,false,true};booleans(flags,4);integer(doc["view_integer"].toInt(1));
    }
    void document(const QJsonObject &doc,const QByteArray &source,const QJsonArray &objects,const QString &title,int width,int height,QPointF origin){
        header(title,doc["description"].toString(title));integer(objects.size());for(auto v:objects)object(transformed(v.toObject(),QTransform()),source);
        integer(doc["settings"].toArray().at(0).toInt(21));integer(width);integer(height);integer(qRound(origin.x()));integer(qRound(origin.y()));tail(doc,source);
    }
};

QJsonObject wire(const QJsonArray &path,const QJsonObject &properties={},int kind=9){
    auto o=base("TDraht",kind,qRound(properties["width"].toDouble(25)));o["path"]=path;
    // Smoothing and milling set with the outline buttons.
    for(auto key:{"flag2","style","rotation","flag3","bitmap"})if(properties.contains(key))o[key]=properties[key];
    // OpenLoch keeps a fill picture's corners relative to the object; the file stores them on the board.
    if(properties.contains("bitmap")){const QPointF at(properties["x"].toDouble(),properties["y"].toDouble());QJsonArray a;const auto r=properties["anchors"].toArray();for(int i=0;i+1<r.size();i+=2){a.append(qRound(at.x()+r[i].toDouble()));a.append(qRound(at.y()+r[i+1].toDouble()));}o["anchors"]=a;}
    QJsonArray connections;
    if(!path.isEmpty()&&(kind==1||kind==9||kind==11||kind==18||kind==19)){connections.append(path.first());if(kind==1)connections.append(path.last());}
    o["points"]=connections;
    auto c=QColor(properties["color"].toString("#28624d"));o["pen"]=c.red()|(c.green()<<8)|(c.blue()<<16);
    const QColor fill(properties["fill"].toString());o["brush"]=fill.isValid()?fill.red()|(fill.green()<<8)|(fill.blue()<<16):o["pen"].toInt();o["transparent"]=properties["filled"].toBool();o["back"]=properties["back"].toBool();return o;
}
// A new label without stored corners (all equal to its top-left corner), like the original's fresh assistant labels:
// the original lays it out from the angle with the measured width. Placed by the top of its cell (baseline at 0.81 h).
QJsonObject text(const QString &value,QPointF at,int height,const QJsonObject &properties={}){
    auto o=base("TTextLabel",10,1);o["text_position"]=pair(at);o["text_end"]=pair(at);
    const auto p=pair(at);o["text_anchors"]=QJsonArray{p[0],p[1],p[0],p[1],p[0],p[1],p[0],p[1],p[0],p[1],p[0],p[1]};
    o["text_kind"]=height;o["text"]=value;o["font"]="Arial";o["text_flags"]=QJsonArray{true,false};
    if(properties.contains("color")){auto c=QColor(properties["color"].toString());o["pen"]=c.red()|(c.green()<<8)|(c.blue()<<16);}return o;
}
QJsonObject primitive(const QJsonObject &o){
    auto type=o["type"].toString();QPointF at(o["x"].toDouble(),o["y"].toDouble());QJsonArray children;
    auto line=[&](std::initializer_list<QPointF> points){QJsonArray path;for(auto p:points)path.append(pair(at+p));children.append(wire(path,o));};
    if(type=="wire"){
        auto properties=o;if(!properties.contains("back"))properties["back"]=true;
        children.append(wire(QJsonArray{pair(at),QJsonArray{o["x2"],o["y2"]}},properties,1));
    }
    // A pin is LochMaster's "Anschluss-Pin" (kind 11): two equal points, as the original never paints single-point wires.
    else if(type=="pin"){auto pin=o;if(!pin.contains("color"))pin["color"]="#b53831";if(!pin.contains("width"))pin["width"]=45;children.append(wire(QJsonArray{pair(at),pair(at)},pin,11));}
    // Potential markers are LochMaster's kind 18: two equal points, width 200, the colour as pen and the name as label.
    else if(type=="potential"){auto marker=o;marker["width"]=200;auto n=wire(QJsonArray{pair(at),pair(at)},marker,18);n["label"]=o["name"].toString();children.append(n);}
    else if(type=="drill"){auto n=base("TBohrung",14);n["diameter"]=o["diameter"].toDouble(.9);n["center"]=pair(at);n["transparent"]=true;children.append(n);}
    // LochMaster kinds: 6 filled outline, 4 plain line (no electrical function), 9 lead soldered at its first point.
    else if(type=="polyline"||type=="polygon"||type=="lead"){
        QJsonArray path;for(auto v:o["points"].toArray())path.append(pair(at+point(v.toArray())));if(type=="polygon")path.append(path.first());children.append(wire(path,o,type=="polygon"?6:type=="lead"?9:4));
    }
    else if(type=="solder"){auto blob=o;blob["filled"]=true;blob["width"]=o["width"].toDouble(150);if(!blob.contains("color"))blob["color"]="#c0c0c0";children.append(wire(QJsonArray{pair(at),pair(at)},blob,19));}
    else if(type=="rectangle"||type=="ellipse"){
        QRectF r(at,QPointF(o["x2"].toDouble(),o["y2"].toDouble()));r=r.normalized();QJsonArray path;
        if(type=="rectangle"){for(auto p:{r.topLeft(),r.topRight(),r.bottomRight(),r.bottomLeft(),r.topLeft()})path.append(pair(p));children.append(wire(path,o,6));}
        else{
            for(int i=0;i<=64;i++){double a=i*2*3.14159265358979323846/64;path.append(pair(r.center()+QPointF(std::cos(a)*r.width()/2,std::sin(a)*r.height()/2)));}
            auto n=base("TKreis",7,qRound(o["width"].toDouble(25)));n["ellipse"]=QJsonArray{qRound(r.left()),qRound(r.top()),qRound(r.right()),qRound(r.bottom())};n["ellipse2"]=n["ellipse"];n["inner"]=wire(path,o,6);
            for(auto key:{"pen","brush","transparent","back","flag3"})n[key]=n["inner"].toObject()[key];
            // The original writes a circle's fill picture twice: on the inner outline with its corners and on the circle itself.
            if(n["inner"].toObject().contains("bitmap"))n["bitmap"]=n["inner"].toObject()["bitmap"];children.append(n);
        }
    }
    // Layout objects as the original's templates store them: tracks (half width, grey brush, filled), pads, drills.
    else if(type=="track"){auto n=base("TLeiterbahn",16,qRound(o["width"].toDouble(200)/2));n["pen"]=0;n["brush"]=0x808080;n["transparent"]=true;n["flag"]=true;n["path"]=QJsonArray{pair(at),pair(QPointF(o["x2"].toDouble(),o["y2"].toDouble()))};children.append(n);}
    else if(type=="eye"){auto n=base("TAuge",15);n["diameter"]=o["diameter"].toDouble(2);n["center"]=pair(at);n["transparent"]=true;children.append(n);}
    else if(type=="cut"){auto n=base("TTrenner",3);n["rect"]=QJsonArray{qRound(at.x()-85),qRound(at.y()-85),qRound(at.x()+85),qRound(at.y()+85)};children.append(n);}
    else if(type=="pad"){
        auto n=base("TAuge",15);n["diameter"]=o["diameter"].toDouble(1.8);n["center"]=pair(at);n["transparent"]=true;children.append(n);
        n=base("TBohrung",14);n["diameter"]=o["drill"].toDouble(.8);n["center"]=pair(at);n["transparent"]=true;children.append(n);
    }
    else if(type=="resistor"){line({{-254,0},{-127,0}});line({{-127,-75},{127,-75},{127,75},{-127,75},{-127,-75}});line({{127,0},{254,0}});}
    else if(type=="capacitor"){line({{-254,0},{-40,0}});line({{-40,-100},{-40,100}});line({{40,-100},{40,100}});line({{40,0},{254,0}});}
    else if(type=="diode"){line({{-254,0},{-100,0}});line({{-100,-100},{-100,100},{100,0},{-100,-100}});line({{100,-100},{100,100}});line({{100,0},{254,0}});}
    else if(type=="ground"){line({{0,-254},{0,0}});line({{-150,0},{150,0}});line({{-100,65},{100,65}});line({{-50,130},{50,130}});}
    else if(type!="text")throw FormatError(ui("Nicht unterstütztes Zeichenelement beim LM4-Export"));
    // OpenLoch draws the text with its baseline 130 units above the anchor.
    if(!o["text"].toString().isEmpty())children.append(text(o["text"].toString(),at+QPointF(type=="text"?0:-127,-130-std::round(.81*o["textSize"].toDouble(120))),o["textSize"].toInt(120),o));
    if(children.size()==1)return children[0].toObject();
    // A pad with drill is a plain group like the original's (not a part, listed if it ever were one).
    if(type=="pad"){auto plain=group(children);plain["group_flags"]=QJsonArray{false,true};plain["id"]="";plain["description"]="";plain["group_value"]=0;return plain;}
    auto result=group(children);result["id"]=o["id"].toString(o["text"].toString());result["value"]=o["value"].toString();result["description"]=o["description"].toString(type);return result;
}
// The changes of an imported object the LM4 file sees: without the fields only a project knows (component, pins).
QJsonObject lm4Edit(QJsonObject edit){edit.remove("component");edit.remove("pins");return edit;}
bool lm4Edits(const Project &p){for(const auto &edit:p.edits)if(!lm4Edit(edit.toObject()).isEmpty())return true;return false;}
// The own model (board.h): OpenLoch's change fields, which it applies to the node, stay out of it.
QJsonObject bakedNode(QJsonObject o){for(auto key:{"angle","mirrorX","mirrorY","z","otherSide","reference","nested","deleted","filled"})o.remove(key);return o;}
// A read object as the own model holds it while it is unchanged.
QJsonObject unchangedNode(const QJsonObject &original){return transformed(original,QTransform());}
// Pictures that point into another file (a library) go into the node as Base64, also inside groups and circles.
QJsonObject withPictures(QJsonObject o,const QByteArray &source){
    if(o.contains("bitmap_offset")){
        if(QByteArray::fromBase64(o["bitmap"].toString().toLatin1()).isEmpty()&&o["bitmap_size"].toInteger()>0)
            o["bitmap"]=QString::fromLatin1(slice(source,o["bitmap_offset"].toInteger(),o["bitmap_size"].toInteger()).toBase64());
        o.remove("bitmap_offset");o.remove("bitmap_size");
    }
    if(o.contains("children")){QJsonArray a;for(const auto &v:o["children"].toArray())a.append(withPictures(v.toObject(),source));o["children"]=a;}
    if(o.contains("inner"))o["inner"]=withPictures(o["inner"].toObject(),source);
    return o;
}
// The settings of a board are those of the file it was read from (an unchanged board is written back as read).
bool settingsAsRead(const Project &p){
    const auto size=p.legacy.value("size").toArray();
    return p.print.isEmpty()&&p.boardSettings.isEmpty()&&!p.notesEdited&&p.sourceKind=="lm4"&&!p.original.isEmpty()&&p.boardSource.isEmpty()&&p.userOrigin.isEmpty()
        &&p.title==legacyTitle(p.legacy)&&p.width==size.at(0).toDouble()&&p.height==size.at(1).toDouble();
}
std::function<void(const Project&,const QByteArray&)> &writeCheck(){static std::function<void(const Project&,const QByteArray&)> check;return check;}
}

// Board settings changed in OpenLoch go into the document: pitch, grids, the board's extra fields and the main view with
// its unit. The parts that change are marked, so that a copied 4.07 tail is replaced only there.
static void applyBoardSettings(const Project &p,QJsonObject &doc){
    QJsonObject changed;
    if(p.pitch()!=doc["grid_mm"].toDouble(2.54)){doc["grid_mm"]=p.pitch();changed["pitch"]=true;}
    auto numbers=doc["numbers"].toArray();if(numbers.size()!=2)numbers={.1,.254};
    if(p.gridMm()!=numbers[0].toDouble()){numbers[0]=p.gridMm();changed["gridMm"]=true;}
    if(p.gridInch()!=numbers[1].toDouble()){numbers[1]=p.gridInch();changed["gridInch"]=true;}
    doc["numbers"]=numbers;
    auto metadata=doc["metadata"].toObject();
    if(p.boardExtra()!=metadata["extra"].toArray()){if(metadata.isEmpty())metadata=boardMetadata();metadata["extra"]=p.boardExtra();doc["metadata"]=metadata;changed["metadata"]=true;}
    if(p.boardSettings.contains("view")||p.boardSettings.contains("unit")){
        auto views=doc["views"].toArray();auto view=views.at(0).toObject();auto flags=view["flags"].toArray();if(flags.size()!=7)flags={false,false,true,true,false,false,true};
        auto ints=view["integers"].toArray();if(ints.size()!=3)ints={0,0,2};const auto v=p.mainView();
        const QJsonArray nextFlags{flags[0],v.flip,v.bitmaps,v.xray,v.through,flags[5],v.potentials},nextInts{ints[0],ints[1],p.unit()};
        if(nextFlags!=flags||nextInts!=ints){view["flags"]=nextFlags;view["integers"]=nextInts;if(views.isEmpty())views.append(view);else views[0]=view;doc["views"]=views;doc["print_override"]=true;}
    }
    if(!changed.isEmpty())doc["changed_settings"]=changed;
}
// The board's file from its records (bottom first) and the board's settings in `p`.
static QByteArray writeBoardFile(const Project &p,bool embeddedBoard,const QList<QByteArray> &records){
    auto originalDoc=p.legacy;auto origin=p.offset();
    // Print settings changed in OpenLoch replace those of the file.
    if(!p.print.isEmpty()){for(auto it=p.print.begin();it!=p.print.end();++it)originalDoc[it.key()]=it.value();originalDoc["print_override"]=true;}
    applyBoardSettings(p,originalDoc);
    const bool sameTitle=p.title==legacyTitle(originalDoc);
    Stream s;s.header(sameTitle?originalDoc["title"].toString():p.title,sameTitle?originalDoc["description"].toString(p.title):p.title);
    s.integer(records.size());for(const auto &record:records)s.bytes.append(record);
    s.integer(originalDoc["settings"].toArray().at(0).toInt(21));s.integer(qRound(p.width));s.integer(qRound(p.height));s.integer(qRound(origin.x()));s.integer(qRound(origin.y()));s.tail(originalDoc,p.original,p.notesEdited?(p.notesRtf.isEmpty()?rtfNotes(p.notes):p.notesRtf):QByteArray{},originBytes(p,origin));
    if(!embeddedBoard){s.bytes.append(QByteArray::fromHex("0201"));LegacyReader(s.bytes).read(false);return s.bytes;}
    auto board=originalDoc["board"].toObject();auto boardBytes=p.original;
    if(!p.boardSource.isEmpty()){auto source=p.libraries[p.boardSource];board=source.document;boardBytes=source.bytes;}
    if(!board.isEmpty()){
        const auto shift=origin-point(board["origin"].toArray());
        if(shift.isNull()&&p.width==board["size"].toArray().at(0).toDouble()&&p.height==board["size"].toArray().at(1).toDouble())s.bytes.append(slice(boardBytes,board["start"].toInteger(),board["end"].toInteger()-board["start"].toInteger()));
        else{auto t=QTransform::fromTranslate(shift.x(),shift.y());QJsonArray a;for(auto v:board["objects"].toArray())a.append(transformed(v.toObject(),t));s.document(board,boardBytes,a,board["title"].toString(p.title),qRound(p.width),qRound(p.height),origin);}
    }else{
        QJsonArray pads;
        // Match the native editor's default perfboard: pads on a 2.54 mm grid.
        qint64 columns=qMax(0,int(std::ceil(p.width/254))-1),rows=qMax(0,int(std::ceil(p.height/254))-1);
        if(columns*rows>45000)throw FormatError(ui("Platine hat zu viele Lötpunkte für das LM4-Format"));
        for(int x=1;x<=columns;x++)for(int y=1;y<=rows;y++){
            auto n=base("TAuge",15);n["diameter"]=1.8;n["center"]=pair(origin+QPointF(x*254,y*254));n["transparent"]=true;pads.append(n);
            n=base("TBohrung",14);n["diameter"]=.9;n["center"]=pair(origin+QPointF(x*254,y*254));n["transparent"]=true;pads.append(n);
        }
        s.document({}, {},pads,p.title,qRound(p.width),qRound(p.height),origin);
    }
    s.bytes.append(QByteArray::fromHex("0201"));
    if(s.bytes.size()>128*1024*1024)throw FormatError(ui("LM4-Datei ist zu groß"));
    LegacyReader(s.bytes).read(true);return s.bytes;
}
static QByteArray writeProject(const Project &p,bool embeddedBoard){
    if(p.mode!="board")throw FormatError(ui("LM4 speichert Lochrasterplatinen. Schaltpläne bitte als .openloch speichern."));
    // Validate in-memory edits as strictly as edits restored from disk.
    Project::decode(p.encode());
    if(embeddedBoard&&settingsAsRead(p)&&p.moves.isEmpty()&&!lm4Edits(p)&&p.additions.isEmpty())return p.original;
    const auto originals=p.legacy["objects"].toArray();const auto origin=p.offset();
    struct Record{double z;QByteArray data;};QList<Record> records;
    for(int i=0;i<originals.size();i++){
        const auto key=QString::number(i);const auto edit=p.edits.value(key).toObject();if(edit["deleted"].toBool())continue;auto original=originals[i].toObject();
        Stream record;
        if(p.legacy["version"]=="4.07"&&lm4Edit(edit).isEmpty()&&!p.moves.contains(key))record.bytes.append(slice(p.original,original["start"].toInteger(),original["end"].toInteger()-original["start"].toInteger()));
        else{
            // The file keeps the raw Kennung ("R#" with the number in group_value), not the shown "R4".
            auto node=p.legacyNode(i);if(original.contains("children"))node["id"]=edit.contains("id")?edit["id"]:original["id"];
            auto t=placementTransform(node,componentAnchor(original),point(p.moves[key].toArray()));record.object(transformed(node,t,mirrorFlags(node)),p.original);
        }
        records.append({edit["z"].toDouble(10+i*.001),record.bytes});
    }
    for(auto value:p.additions){
        auto placement=value.toObject();QJsonObject node;QByteArray source;QPointF anchor,offset;
        if(placement["type"]=="component"){
            auto key=placement["library"].toString();auto original=p.libraryNode(key,placement["index"].toInt());node=p.componentNode(placement);source=p.libraries[key].bytes;
            if(node.contains("children"))node["id"]=placement.contains("id")?placement["id"]:original["id"];
            anchor=componentAnchor(original);offset=QPointF(placement["x"].toDouble(),placement["y"].toDouble())-anchor+origin;
        }else{node=primitive(placement);anchor={placement["x"].toDouble(),placement["y"].toDouble()};offset=origin;}
        Stream record;record.object(transformed(node,placementTransform(placement,anchor,offset),mirrorFlags(placement)),source);records.append({placement["z"].toDouble(50+records.size()*.001),record.bytes});
    }
    std::stable_sort(records.begin(),records.end(),[](const auto &a,const auto &b){return a.z<b.z;});
    QList<QByteArray> bytes;for(const auto &record:records)bytes.append(record.data);
    return writeBoardFile(p,embeddedBoard,bytes);
}
// A board of the own model (board.h). A read object whose node is the one read at its place is unchanged: from a 4.07
// file it is copied as read. When everything is unchanged and in its order, the file is the one read.
static QByteArray writeBoard(const Board &board,bool embeddedBoard){
    const Project &p=board.settings;
    if(p.mode!="board")throw FormatError(ui("LM4 speichert Lochrasterplatinen. Schaltpläne bitte als .openloch speichern."));
    const auto originals=p.legacy.value("objects").toArray();const bool copy=p.legacy.value("version")=="4.07";const auto origin=p.offset();
    QHash<qint64,int> readAt;for(int i=0;i<originals.size();i++)readAt.insert(originals[i].toObject()["start"].toInteger(),i);
    bool asRead=embeddedBoard&&settingsAsRead(p)&&board.objects.size()==originals.size();
    QList<QByteArray> records;
    for(int k=0;k<board.objects.size();k++){
        auto o=board.objects[k].toObject();Stream record;
        if(o["type"].toString().startsWith('T')){
            const int i=o.contains("start")?readAt.value(o["start"].toInteger(),-1):-1;
            for(auto field:{"uid","part","component","pins"})o.remove(field);
            const bool same=i>=0&&o==unchangedNode(originals[i].toObject());asRead=asRead&&same&&i==k;
            if(same&&copy){const auto read=originals[i].toObject();record.bytes=slice(p.original,read["start"].toInteger(),read["end"].toInteger()-read["start"].toInteger());}
            else record.object(o,p.original);
        }else{
            asRead=false;
            record.object(transformed(primitive(o),placementTransform(o,{o["x"].toDouble(),o["y"].toDouble()},origin),mirrorFlags(o)),{});
        }
        records.append(record.bytes);
    }
    if(asRead)return p.original;
    return writeBoardFile(p,embeddedBoard,records);
}
// Board 0, the board count, then the other boards, each written as a single-board file without its count.
static QByteArray joinBoards(const QList<QByteArray> &files){
    QByteArray out;
    for(int i=0;i<files.size();i++){
        auto bytes=files[i];if(!bytes.endsWith(QByteArray::fromHex("0201")))throw FormatError(ui("Platine ohne Abschluss beim LM4-Export"));bytes.chop(2);out.append(bytes);
        if(i==0){Stream count;count.integer(int(files.size()));out.append(count.bytes);}
    }
    if(out.size()>128*1024*1024)throw FormatError(ui("LM4-Datei ist zu groß"));LegacyReader(out).read(true);return out;
}
QByteArray writeLegacyProject(const Project &p){
    QByteArray out;
    if(p.boards.size()<2)out=writeProject(p,true);
    else{
        QList<QByteArray> files;for(const auto &page:QJsonDocument::fromJson(p.encode()).object()["boards"].toArray())files.append(writeProject(Project::decode(QJsonDocument(page.toObject()).toJson()),true));
        out=joinBoards(files);
    }
    if(writeCheck())writeCheck()(p,out);
    return out;
}
QJsonArray boardObjects(const Project &p){
    struct Entry{double z;QJsonObject node;};QList<Entry> entries;
    const auto originals=p.legacy.value("objects").toArray();const auto origin=p.offset();
    for(int i=0;i<originals.size();i++){
        const auto key=QString::number(i);const auto edit=p.edits.value(key).toObject();if(edit["deleted"].toBool())continue;
        const auto original=originals[i].toObject();QJsonObject node;
        if(lm4Edit(edit).isEmpty()&&!p.moves.contains(key)){node=unchangedNode(original);for(auto field:{"component","pins"})if(edit.contains(field))node[field]=edit[field];}
        else{
            node=p.legacyNode(i);if(original.contains("children"))node["id"]=edit.contains("id")?edit["id"]:original["id"];
            node=bakedNode(transformed(node,placementTransform(node,componentAnchor(original),point(p.moves[key].toArray())),mirrorFlags(node)));
            // A changed object is written from its node, as version 1 does: it no longer names its record.
            node.remove("start");node.remove("end");
        }
        node["uid"]=p.uidOf("legacy",i);entries.append({edit["z"].toDouble(10+i*.001),node});
    }
    for(const auto &value:p.additions){
        auto placement=value.toObject();const double z=placement["z"].toDouble(50+entries.size()*.001);QJsonObject node;
        if(placement["type"]=="component"){
            const auto key=placement["library"].toString();const auto library=p.libraries.value(key);const auto original=p.libraryNode(key,placement["index"].toInt());
            node=p.componentNode(placement);if(node.contains("children"))node["id"]=placement.contains("id")?placement["id"]:original["id"];
            const auto anchor=componentAnchor(original);const auto offset=QPointF(placement["x"].toDouble(),placement["y"].toDouble())-anchor+origin;
            node=withPictures(bakedNode(transformed(node,placementTransform(placement,anchor,offset),mirrorFlags(placement))),library.bytes);
            node.remove("start");node.remove("end");node.remove("offset");
            node["part"]=QJsonObject{{"library",key},{"index",placement["index"].toInt()},{"name",library.name}};node["uid"]=placement["uid"];
            for(auto field:{"component","pins"})if(placement.contains(field))node[field]=placement[field];
        }else{node=placement;node.remove("z");}
        entries.append({z,node});
    }
    std::stable_sort(entries.begin(),entries.end(),[](const Entry &a,const Entry &b){return a.z<b.z;});
    QJsonArray result;for(const auto &e:entries)result.append(e.node);return result;
}
QByteArray writeLegacyBoards(const QList<Board> &boards){
    if(boards.isEmpty())throw FormatError(ui("Platine fehlt"));
    if(boards.size()<2)return writeBoard(boards[0],true);
    QList<QByteArray> files;for(const auto &board:boards)files.append(writeBoard(board,true));
    return joinBoards(files);
}
void checkBoardObjects(const Board &board){
    for(const auto &value:board.objects){const auto o=value.toObject();if(o["type"].toString().startsWith('T')){Stream s;s.object(o,board.settings.original);}}
}
void setLegacyWriteCheck(std::function<void(const Project&,const QByteArray&)> check){writeCheck()=std::move(check);}
QByteArray writeLegacyDocument(const Project &p,bool groupObjects,bool dissolveGroups,bool boardTemplate){
    auto bytes=writeProject(p,false);auto doc=LegacyReader(bytes).read(false);auto objects=doc["objects"].toArray();
    if(dissolveGroups){
        std::function<QJsonObject(QJsonObject,QJsonObject)> resolve=[&](QJsonObject n,QJsonObject context){
            if(n.contains("children")){auto own=context;for(auto key:{"id","value","description"})if(!n[key].toString().isEmpty())own[key]=n[key];QJsonArray children;for(auto v:n["children"].toArray())children.append(resolve(v.toObject(),own));n["children"]=children;}
            if(n["type"]=="TTextLabel")n["text"]=componentText(n["text"].toString(),context,p.title);return n;
        };
        QJsonArray expanded;for(auto v:objects){auto n=resolve(v.toObject(),{});if(n.contains("children"))for(auto child:n["children"].toArray())expanded.append(child);else expanded.append(n);}objects=expanded;
    }
    // Like the original's "Gruppe bilden": a plain group (not a part, listed if it ever becomes one), empty texts.
    if(groupObjects){auto assembly=group(objects);assembly["group_flags"]=QJsonArray{false,true};assembly["group_value"]=0;assembly["description"]="";objects=QJsonArray{assembly};}
    Stream s;s.document(doc,bytes,objects,p.title,qRound(p.width),qRound(p.height),point(doc["origin"].toArray()));if(boardTemplate){s.byte(2);s.byte(1);}
    LegacyReader(s.bytes).read(false);return s.bytes;
}
QByteArray writeScaledObject(const QByteArray &document,int index,double width,double height){
    const auto doc=LegacyReader(document).read(false);auto o=doc["objects"].toArray().at(index).toObject();const double smallest=o["width"].toDouble()*2+1;
    for(int axis=0;axis<2;axis++){
        const double target=qMin(axis?height:width,1000000.0);if(target<=smallest)continue;
        for(int step=0;step<6;step++){
            const auto r=legacyBounds(o);const double size=axis?r.height():r.width();if(size<=smallest||std::abs(size-target)<.01)break;const double f=target/size;
            QTransform t;t.translate(r.center().x(),r.center().y());t.scale(axis?1:f,axis?f:1);t.translate(-r.center().x(),-r.center().y());o=transformed(o,t);
        }
    }
    Stream s;const auto size=doc["size"].toArray();s.document(doc,document,QJsonArray{o},doc["title"].toString(),size.at(0).toInt(),size.at(1).toInt(),point(doc["origin"].toArray()));
    LegacyReader(s.bytes).read(false);return s.bytes;
}
QByteArray writeLegacyObjects(const QJsonArray &objects,const QString &title,QSizeF size){
    Project blank;blank.mode="board";blank.title=title;blank.width=size.width()*100;blank.height=size.height()*100;auto bytes=writeProject(blank,false);auto doc=LegacyReader(bytes).read(false);
    Stream s;s.document(doc,bytes,objects,title,qRound(blank.width),qRound(blank.height),point(doc["origin"].toArray()));
    LegacyReader(s.bytes).read(false);return s.bytes;
}
QJsonObject legacyObject(const QString &type,int kind,int width){return base(type,kind,width);}
QJsonObject legacyLabel(const QString &value,QPointF at,int height){return text(value,at,height);}
QJsonObject legacyGroup(const QJsonArray &children){return group(children);}
}
