#include "project.h"
#include "language.h"
#include <QtEndian>
#include "legacy_reader.h"
#include "legacy_writer.h"
#include "geometry.h"
#include "notes.h"
#include "documents/projectfile.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSet>
#include <QColor>
#include <cmath>
#include <functional>

namespace openloch {
namespace {
// Extra fields as LochMaster stores them: name, shown in dialogs, value.
bool validExtraFields(const QJsonValue &value){
    if(!value.isArray()||value.toArray().size()>1000)return false;
    for(auto v:value.toArray()){const auto f=v.toArray();if(!v.isArray()||f.size()!=3||!f[0].isString()||!f[1].isBool()||!f[2].isString()||f[0].toString().size()>256||f[2].toString().size()>65536)return false;}
    return true;
}
void validateProperties(const QJsonObject &node) {
    for(auto key:{"x","y","x2","y2"})if(node.contains(key)&&(!node[key].isDouble()||!std::isfinite(node[key].toDouble())||std::abs(node[key].toDouble())>1000000))throw FormatError(ui("Ungültige Koordinate"));
    if(node.contains("angle")&&(!node["angle"].isDouble()||!std::isfinite(node["angle"].toDouble())||std::abs(node["angle"].toDouble())>360000))throw FormatError(ui("Ungültiger Drehwinkel"));
    for(auto key:{"mirrorX","mirrorY","deleted","filled","back","otherSide","flag2","flag3"})if(node.contains(key)&&!node[key].isBool())throw FormatError(ui("Ungültige Bauteileigenschaft"));
    // Outline buttons and fill pictures: smoothing style and size, the picture (BMP as Base64) and its three corners.
    if(node.contains("style")&&(!node["style"].isDouble()||node["style"].toDouble()!=node["style"].toInt(-1)||node["style"].toInt()>2))throw FormatError(ui("Ungültige Kontur"));
    if(node.contains("rotation")&&(!node["rotation"].isDouble()||!std::isfinite(node["rotation"].toDouble())||std::abs(node["rotation"].toDouble())>1000000))throw FormatError(ui("Ungültige Kontur"));
    if(node.contains("bitmap")&&(!node["bitmap"].isString()||node["bitmap"].toString().size()>24*1024*1024))throw FormatError(ui("Ungültige Bitmapfüllung"));
    if(node.contains("anchors")){const auto a=node["anchors"].toArray();if(!node["anchors"].isArray()||a.size()!=6)throw FormatError(ui("Ungültige Bitmapfüllung"));
        for(auto v:a)if(!v.isDouble()||!std::isfinite(v.toDouble())||std::abs(v.toDouble())>1000000)throw FormatError(ui("Ungültige Bitmapfüllung"));}
    if(node.contains("z")&&(!node["z"].isDouble()||!std::isfinite(node["z"].toDouble())||std::abs(node["z"].toDouble())>1000000))throw FormatError(ui("Ungültige Ebenenfolge"));
    for(auto key:{"path","points"})if(node.contains(key)){
        if(!node[key].isArray()||node[key].toArray().isEmpty()||node[key].toArray().size()>100000)throw FormatError(ui("Ungültige Kontur"));
        for(auto value:node[key].toArray()){auto a=value.toArray();if(a.size()!=2)throw FormatError(ui("Ungültiger Knoten"));for(auto v:a)if(!v.isDouble()||!std::isfinite(v.toDouble())||std::abs(v.toDouble())>1000000)throw FormatError(ui("Ungültige Knotenkoordinate"));}
    }
    for(auto key:{"id","value","description","text","font","color","fill"})if(node.contains(key)&&(!node[key].isString()||node[key].toString().size()>65536))throw FormatError(ui("Ungültige Beschriftung"));
    for(auto key:{"color","fill"})if(node.contains(key)&&!QColor(node[key].toString()).isValid())throw FormatError(ui("Ungültige Farbe"));
    for(auto key:{"pen","brush"})if(node.contains(key)&&(!node[key].isDouble()||node[key].toDouble()<0||node[key].toDouble()>4294967295.0||std::floor(node[key].toDouble())!=node[key].toDouble()))throw FormatError(ui("Ungültige Farbe"));
    for(auto key:{"width","diameter","text_kind","textSize"})if(node.contains(key)&&(!node[key].isDouble()||node[key].toDouble()<(QString(key)=="width"?0:QString(key)=="diameter"?.01:1)||node[key].toDouble()>100000))throw FormatError(ui("Ungültige Elementgröße"));
    // The Bauteil dialog: name, part and list flags, extra fields and the Bezugspunkt (-1: automatic).
    if(node.contains("label")&&(!node["label"].isString()||node["label"].toString().size()>65536))throw FormatError(ui("Ungültige Beschriftung"));
    if(node.contains("group_flags")){const auto f=node["group_flags"].toArray();if(!node["group_flags"].isArray()||f.size()!=2||!f[0].isBool()||!f[1].isBool())throw FormatError(ui("Ungültige Bauteileigenschaft"));}
    if(node.contains("extra")&&!validExtraFields(node["extra"]))throw FormatError(ui("Ungültige Bauteileigenschaft"));
    // A part in the project (docs/openloch-project-format.md, "Kennungen und Bauteile"): the component it stands for and
    // the names of its pins in the order of its connection points.
    if(node.contains("component")&&(!node["component"].isString()||!documents::isId(node["component"].toString())))throw FormatError(ui("Ungültige Kennungen"));
    if(node.contains("pins")){
        const auto names=node["pins"].toArray();QSet<QString> seen;if(!node["pins"].isArray()||names.size()>1000)throw FormatError(ui("Ungültige Anschlüsse"));
        for(const auto &v:names){if(!v.isString()||v.toString().trimmed().isEmpty()||v.toString().size()>64||seen.contains(v.toString()))throw FormatError(ui("Ungültige Anschlüsse"));seen.insert(v.toString());}
    }
    // Values of parts inside a part, by their child indices ("2" or "2/0"), as the Bauteil dialog sets them.
    if(node.contains("nested")){
        if(!node["nested"].isObject()||node["nested"].toObject().size()>10000)throw FormatError(ui("Ungültige Bauteileigenschaft"));const auto nested=node["nested"].toObject();
        static const QRegularExpression pathPattern("^\\d{1,6}(/\\d{1,6}){0,63}$");
        for(auto it=nested.begin();it!=nested.end();++it){
            if(!pathPattern.match(it.key()).hasMatch()||!it.value().isObject())throw FormatError(ui("Ungültige Bauteileigenschaft"));const auto values=it.value().toObject();
            for(auto key:values.keys())if(!QStringList{"id","value","description","label","group_value","group_flags","extra"}.contains(key))throw FormatError(ui("Ungültige Bauteileigenschaft"));
            validateProperties(values);
        }
    }
    if(node.contains("group_value")&&(!node["group_value"].isDouble()||node["group_value"].toDouble()!=node["group_value"].toInt(-1)||node["group_value"].toInt()<0||node["group_value"].toInt()>1000000000))throw FormatError(ui("Ungültige Bauteileigenschaft"));
    if(node.contains("reference")&&(!node["reference"].isDouble()||node["reference"].toDouble()!=node["reference"].toInt(-2)||node["reference"].toInt()<-1||node["reference"].toInt()>100000))throw FormatError(ui("Ungültige Bauteileigenschaft"));
}
void validateBoardSettings(const QJsonObject &s){
    const auto invalid=[]{return FormatError(ui("Ungültige Platineneinstellungen"));};
    for(auto it=s.begin();it!=s.end();++it)if(!QStringList{"pitch","gridMm","gridInch","offset","extra","unit","view"}.contains(it.key()))throw invalid();
    if(s.contains("pitch")&&(!s["pitch"].isDouble()||!(s["pitch"].toDouble()>=.1&&s["pitch"].toDouble()<=1000)))throw invalid();
    for(auto key:{"gridMm","gridInch"})if(s.contains(key)&&(!s[key].isDouble()||!(s[key].toDouble()>=0&&s[key].toDouble()<=1000)))throw invalid();
    if(s.contains("offset")){const auto a=s["offset"].toArray();if(!s["offset"].isArray()||a.size()!=2)throw invalid();for(auto v:a)if(!v.isDouble()||!std::isfinite(v.toDouble())||std::abs(v.toDouble())>1000000)throw invalid();}
    if(s.contains("unit")&&(!s["unit"].isDouble()||s["unit"].toDouble()!=s["unit"].toInt(-1)||s["unit"].toInt()<0||s["unit"].toInt()>2))throw invalid();
    if(s.contains("view")){if(!s["view"].isObject())throw invalid();const auto v=s["view"].toObject();for(auto it=v.begin();it!=v.end();++it)if(!QStringList{"flip","bitmaps","xray","through","potentials"}.contains(it.key())||!it.value().isBool())throw invalid();}
    if(s.contains("extra")&&!validExtraFields(s["extra"]))throw invalid();
}
}
Project Project::load(const QString &path) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly))throw FormatError(f.errorString());
    if(f.size()>128*1024*1024)throw FormatError(ui("Datei ist zu groß"));auto b=f.readAll();
    auto suffix=QFileInfo(path).suffix().toLower();
    // Backups (.BAK of AutoSpeichern, .OLD written on opening) hold a whole project, LochMaster's or OpenLoch's.
    if(suffix=="bak"||suffix=="old")suffix=b.trimmed().startsWith('{')?"openloch":"lm4";
    if(suffix=="openloch")return decode(b);
    if(suffix!="lm4"&&suffix!="lmb"&&suffix!="lib")throw FormatError(ui("Dieses Dateiformat ist noch nicht implementiert"));
    auto single=[&](const QByteArray &bytes){
        Project p;p.sourceName=QFileInfo(path).fileName();p.sourceKind=suffix;p.original=bytes;
        p.legacy=LegacyReader(bytes).read(suffix=="lm4");p.title=legacyTitle(p.legacy);
        p.notesRtf=bytes.mid(p.legacy["annotations_offset"].toInteger(),p.legacy["annotations_size"].toInteger());p.notes=plainNotes(p.notesRtf);
        auto size=p.legacy["size"].toArray();p.width=size[0].toDouble();p.height=size[1].toDouble();return p;
    };
    auto p=single(b);if(!p.legacy.contains("more_boards")){p.assignIds();return p;}
    // Several boards: each becomes a board of its own, read from its own bytes plus the count 1, so an unchanged board is
    // written back byte for byte. As in the original, the last board is shown after loading.
    QList<QByteArray> slices{b.left(p.legacy["board_end"].toInteger())};
    for(auto v:p.legacy["more_boards"].toArray()){auto r=v.toObject();slices.append(b.mid(r["start"].toInteger(),r["end"].toInteger()-r["start"].toInteger()));}
    QSet<QString> taken;QJsonArray pages;Project last;
    for(const auto &slice:slices){last=single(slice+QByteArray::fromHex("0201"));last.assignIds(&taken);pages.append(QJsonDocument::fromJson(last.encode()).object());}
    last.boards=pages;last.activeBoard=int(pages.size())-1;return last;
}
// The original stores the origin in file coordinates; OpenLoch draws imported boards shifted by their offset.
QPointF Project::origin() const{
    if(userOrigin.size()==2)return {userOrigin[0].toDouble(),userOrigin[1].toDouble()};
    const auto raw=QByteArray::fromHex(legacy["raw_points"].toString().toLatin1());if(raw.size()<8)return {};
    return QPointF(qFromLittleEndian<qint32>(raw.constData()),qFromLittleEndian<qint32>(raw.constData()+4))-offset();
}
QPointF Project::offset() const{const auto o=(boardSettings.contains("offset")?boardSettings:legacy).value(boardSettings.contains("offset")?"offset":"origin").toArray();return {o.at(0).toDouble(),o.at(1).toDouble()};}
double Project::pitch() const{return boardSettings.contains("pitch")?boardSettings["pitch"].toDouble():legacy.value("grid_mm").toDouble(2.54);}
double Project::gridMm() const{return boardSettings.contains("gridMm")?boardSettings["gridMm"].toDouble():legacy.value("numbers").toArray().at(0).toDouble(.1);}
double Project::gridInch() const{return boardSettings.contains("gridInch")?boardSettings["gridInch"].toDouble():legacy.value("numbers").toArray().at(1).toDouble(.254);}
int Project::unit() const{
    const int stored=boardSettings.contains("unit")?boardSettings["unit"].toInt():legacy.value("views").toArray().at(0).toObject()["integers"].toArray().at(2).toInt(2);
    return stored>=0&&stored<=2?stored:2;
}
QJsonArray Project::boardExtra() const{return boardSettings.contains("extra")?boardSettings["extra"].toArray():legacy.value("metadata").toObject()["extra"].toArray();}
// The main view of a LochMaster file: flags 1 Wenden, 2 BMP-Rendering, 3 Röntgenblick, 4 Durchsicht, 6 Potenziale.
Project::MainView Project::mainView() const{
    MainView v;
    if(boardSettings.contains("view")){const auto o=boardSettings["view"].toObject();v.flip=o["flip"].toBool();v.bitmaps=o["bitmaps"].toBool(true);v.xray=o["xray"].toBool(true);v.through=o["through"].toBool();v.potentials=o["potentials"].toBool();return v;}
    const auto flags=legacy.value("views").toArray().at(0).toObject()["flags"].toArray();
    if(flags.size()==7){v.flip=flags[1].toBool();v.bitmaps=flags[2].toBool();v.xray=flags[3].toBool();v.through=flags[4].toBool();v.potentials=flags[6].toBool();}
    return v;
}
void Project::storeMainView(const MainView &view,int chosenUnit){
    const auto now=mainView();
    if(now.flip!=view.flip||now.bitmaps!=view.bitmaps||now.xray!=view.xray||now.through!=view.through||now.potentials!=view.potentials)
        boardSettings["view"]=QJsonObject{{"flip",view.flip},{"bitmaps",view.bitmaps},{"xray",view.xray},{"through",view.through},{"potentials",view.potentials}};
    if(chosenUnit!=unit())boardSettings["unit"]=chosenUnit;
}
QByteArray Project::encode() const {
    QSet<QString> used;if(!boardSource.isEmpty())used.insert(boardSource);for(auto v:additions){auto node=v.toObject();if(node["type"]=="component")used.insert(node["library"].toString());}
    QJsonObject sources;for(auto it=libraries.begin();it!=libraries.end();++it)if(used.contains(it.key()))sources[it.key()]=QJsonObject{{"name",it->name},{"kind",it->kind},{"data",QString::fromLatin1(it->bytes.toBase64())}};
    QJsonObject root{{"format","OpenLoch"},{"version",1},{"title",title},{"mode",mode},
        {"width",width},{"height",height},{"sourceName",sourceName},{"sourceKind",sourceKind},
        {"originalBase64",QString::fromLatin1(original.toBase64())},{"userOrigin",userOrigin},{"additions",additions},{"moves",moves},{"edits",edits},{"libraries",sources},{"boardSource",boardSource},{"notes",notes},{"notesEdited",notesEdited}};
    if(notesEdited&&!notesRtf.isEmpty())root["notesRtf"]=QString::fromLatin1(notesRtf.toBase64());
    if(!print.isEmpty())root["print"]=print;
    if(!boardSettings.isEmpty())root["boardSettings"]=boardSettings;
    if(!uids.isEmpty())root["uids"]=uids;
    if(!boardId.isEmpty())root["boardId"]=boardId;
    if(!boards.isEmpty()){auto pages=boards;pages[activeBoard]=root;root["boards"]=pages;root["activeBoard"]=activeBoard;}return QJsonDocument(root).toJson();
}
Project Project::decode(const QByteArray &b) {
    if(b.size()>128*1024*1024)throw FormatError(ui("Projekt ist zu groß"));
    QJsonParseError error;auto json=QJsonDocument::fromJson(b,&error);
    if(error.error!=QJsonParseError::NoError||!json.isObject())throw FormatError(ui("Ungültiges OpenLoch-Projekt"));
    auto o=json.object();if(o["format"]!="OpenLoch"||o["version"].toInt()!=1)throw FormatError(ui("Unbekannte Projektversion"));
    Project p;p.title=o["title"].toString();p.mode=o["mode"].toString();p.width=o["width"].toDouble();p.height=o["height"].toDouble();
    if((p.mode!="board"&&p.mode!="schematic")||!std::isfinite(p.width)||!std::isfinite(p.height)||p.width<=0||p.height<=0||p.width>1000000||p.height>1000000)throw FormatError(ui("Ungültige Projekteinstellungen"));
    p.sourceName=o["sourceName"].toString();p.sourceKind=o["sourceKind"].toString();
    p.notes=o["notes"].toString();p.notesEdited=o["notesEdited"].toBool();
    if(o.contains("print")){if(!o["print"].isObject()||(o["print"].toObject().contains("views")&&o["print"].toObject()["views"].toArray().size()!=11))throw FormatError(ui("Ungültige Druckeinstellungen"));p.print=o["print"].toObject();}
    if(o.contains("boardSettings")){if(!o["boardSettings"].isObject())throw FormatError(ui("Ungültige Platineneinstellungen"));p.boardSettings=o["boardSettings"].toObject();validateBoardSettings(p.boardSettings);}
    if(o["userOrigin"].isArray()){p.userOrigin=o["userOrigin"].toArray();if(!p.userOrigin.isEmpty()&&(p.userOrigin.size()!=2||!p.userOrigin[0].isDouble()||!p.userOrigin[1].isDouble()))throw FormatError(ui("Ungültiger Ursprung"));}if(p.notes.size()>1000000)throw FormatError(ui("Anmerkungen sind zu lang"));
    auto decoded=QByteArray::fromBase64Encoding(o["originalBase64"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(!decoded)throw FormatError(ui("Ungültige eingebettete Quelldatei"));p.original=decoded.decoded;
    if(!p.original.isEmpty()){if(!QStringList{"lm4","lmb","lib"}.contains(p.sourceKind))throw FormatError(ui("Unbekanntes Quellformat"));p.legacy=LegacyReader(p.original).read(p.sourceKind=="lm4");}
    // Edited notes carry their RTF (older files only the text); unedited ones are those of the embedded file.
    if(p.notesEdited){const auto rtf=QByteArray::fromBase64Encoding(o["notesRtf"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);if(!rtf||rtf.decoded.size()>4000000)throw FormatError(ui("Ungültige Anmerkungen"));p.notesRtf=rtf.decoded.isEmpty()?rtfNotes(p.notes):rtf.decoded;}
    else if(p.legacy.contains("annotations_offset"))p.notesRtf=p.original.mid(p.legacy["annotations_offset"].toInteger(),p.legacy["annotations_size"].toInteger());
    auto sources=o["libraries"].toObject();if(sources.size()>1000)throw FormatError(ui("Zu viele Bibliotheken"));
    qint64 sourceSize=0;
    for(auto it=sources.begin();it!=sources.end();++it){auto s=it.value().toObject();auto data=QByteArray::fromBase64Encoding(s["data"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);if(!data)throw FormatError(ui("Ungültige Bibliotheksdaten"));sourceSize+=data.decoded.size();if(sourceSize>96*1024*1024)throw FormatError(ui("Bibliotheksdaten sind zu groß"));auto key=p.addLibrary(data.decoded,s["name"].toString(),s["kind"].toString("lib"));if(key!=it.key())throw FormatError(ui("Bibliotheksprüfsumme stimmt nicht"));}
    p.boardSource=o["boardSource"].toString();if(!p.boardSource.isEmpty()&&(!p.libraries.contains(p.boardSource)||p.libraries[p.boardSource].kind!="lmb"))throw FormatError(ui("Platinenvorlage fehlt"));
    p.additions=o["additions"].toArray();if(p.additions.size()>100000)throw FormatError(ui("Zu viele neue Elemente"));
    for(const auto &value:p.additions) {
        if(!value.isObject())throw FormatError(ui("Ungültiges Zeichenelement"));auto node=value.toObject();
        if(!QStringList{"wire","cut","pad","text","resistor","capacitor","ground","diode","component","rectangle","ellipse","polygon","polyline","drill","pin","potential","lead","solder","track","eye"}.contains(node["type"].toString()))throw FormatError(ui("Unbekanntes Zeichenelement"));
        validateProperties(node);
        if(!node["x"].isDouble()||!node["y"].isDouble())throw FormatError(ui("Elementposition fehlt"));
        if(QStringList{"wire","rectangle","ellipse","track"}.contains(node["type"].toString())&&(!node["x2"].isDouble()||!node["y2"].isDouble()))throw FormatError(ui("Elementende fehlt"));
        if(QStringList{"polygon","polyline"}.contains(node["type"].toString())&&node["points"].toArray().size()<(node["type"]=="polygon"?3:2))throw FormatError(ui("Kontur hat zu wenige Knoten"));
        if(node["type"]=="component"){if(!node["index"].isDouble()||node["index"].toDouble()!=node["index"].toInt(-1))throw FormatError(ui("Ungültiges Bibliotheksbauteil"));p.componentNode(node);}
    }
    p.moves=o["moves"].toObject();
    for(auto it=p.moves.begin();it!=p.moves.end();++it) {
        bool ok=false;int index=it.key().toInt(&ok);auto offset=it.value().toArray();
        if(!ok||it.key()!=QString::number(index)||index<0||index>=p.legacy["objects"].toArray().size()||offset.size()!=2)throw FormatError(ui("Ungültige Bauteilverschiebung"));
        for(const auto &coordinate:offset)if(!coordinate.isDouble()||!std::isfinite(coordinate.toDouble())||std::abs(coordinate.toDouble())>1000000)throw FormatError(ui("Ungültige Bauteilverschiebung"));
    }
    p.edits=o["edits"].toObject();
    for(auto it=p.edits.begin();it!=p.edits.end();++it){
        bool ok=false;int index=it.key().toInt(&ok);if(!ok||it.key()!=QString::number(index)||index<0||index>=p.legacy["objects"].toArray().size()||!it.value().isObject())throw FormatError(ui("Ungültige Bauteiländerung"));
        const auto edit=it.value().toObject();validateProperties(edit);
        for(auto key:edit.keys())if(!QStringList{"id","value","description","text","angle","mirrorX","mirrorY","deleted","width","pen","brush","diameter","text_kind","font","z","path","filled","back","otherSide","flag2","style","rotation","flag3","bitmap","anchors","label","group_flags","extra","reference","group_value","nested","component","pins"}.contains(key))throw FormatError(ui("Unbekannte Bauteileigenschaft"));
    }
    if(o.contains("uids")){
        if(!o["uids"].isObject())throw FormatError(ui("Ungültige Kennungen"));p.uids=o["uids"].toObject();
        for(auto it=p.uids.begin();it!=p.uids.end();++it){bool ok=false;const int index=it.key().toInt(&ok);if(!ok||index<0||index>=p.legacy.value("objects").toArray().size()||!it.value().isString())throw FormatError(ui("Ungültige Kennungen"));}
    }
    if(o.contains("boardId")){if(!o["boardId"].isString()||!documents::isId(o["boardId"].toString()))throw FormatError(ui("Ungültige Kennungen"));p.boardId=o["boardId"].toString();}
    if(o.contains("boards")){
        if(!o["boards"].isArray())throw FormatError(ui("Ungültige Platinenliste"));p.boards=o["boards"].toArray();p.activeBoard=o["activeBoard"].toInt(-1);
        if(p.boards.isEmpty()||p.boards.size()>100||p.activeBoard<0||p.activeBoard>=p.boards.size())throw FormatError(ui("Ungültige aktive Platine"));
        // Every board's objects get their identifiers now, unique in the whole document: one that an earlier board has
        // already is renewed. The other boards are kept as decoded, so that they are saved with them.
        QSet<QString> taken;
        for(int i=0;i<p.boards.size();i++){
            const auto v=p.boards[i];auto page=v.toObject();if(!v.isObject()||page.contains("boards"))throw FormatError(ui("Ungültige Platine im Projekt"));
            if(i==p.activeBoard){p.assignIds(&taken);continue;}
            auto board=Project::decode(QJsonDocument(page).toJson(QJsonDocument::Compact));board.assignIds(&taken);p.boards[i]=QJsonDocument::fromJson(board.encode()).object();
        }
        return p;
    }
    p.assignIds();return p;
}
QString Project::addLibrary(const QByteArray &data,const QString &name,const QString &kind) {
    if(!QStringList{"lib","lm4","lmb"}.contains(kind))throw FormatError(ui("Unbekanntes Bibliotheksformat"));
    auto key=QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex());
    if(!libraries.contains(key))libraries.insert(key,LibrarySource{name,kind,data,LegacyReader(data).read(kind=="lm4")});return key;
}
QJsonObject Project::libraryNode(const QString &key,int index) const {
    auto it=libraries.constFind(key);if(it==libraries.cend())throw FormatError(ui("Bauteilbibliothek fehlt"));auto objects=it->document["objects"].toArray();
    if(index<0||index>=objects.size())throw FormatError(ui("Bibliotheksbauteil fehlt"));return objects[index].toObject();
}
// "Auf andere Platinenseite setzen" on a group switches the side of everything inside, as the original does.
static void switchSide(QJsonObject &n){
    n["back"]=!n["back"].toBool();
    if(n.contains("children")){auto c=n["children"].toArray();for(auto &&v:c){auto x=v.toObject();switchSide(x);v=x;}n["children"]=c;}
    if(n.contains("inner")){auto i=n["inner"].toObject();switchSide(i);n["inner"]=i;}
}
// Values of parts inside a part, addressed by their child indices ("2/0" is the first child of the third).
static void applyNested(QJsonObject &node,const QJsonObject &nested){
    for(auto it=nested.begin();it!=nested.end();++it){
        const auto path=it.key().split('/');const auto values=it.value().toObject();
        std::function<void(QJsonObject&,int)> walk=[&](QJsonObject &n,int depth){
            auto children=n["children"].toArray();const int i=path[depth].toInt();if(i<0||i>=children.size())return;auto child=children[i].toObject();
            if(depth+1==path.size()){for(auto v=values.begin();v!=values.end();++v)child[v.key()]=v.value();}else walk(child,depth+1);
            children[i]=child;n["children"]=children;
        };
        walk(node,0);
    }
    node.remove("nested");
}
// A circle (TKreis) is drawn and saved through its inner outline: colours, width, fill, milling and smoothing size go
// along, as in the original (its inner outline always stays a B-spline).
// `ownAnchors` are the circle's own picture corners before the changes.
static void forwardToInner(QJsonObject &node,const QJsonObject &changes,const QJsonValue &ownAnchors){
    if(!node.contains("inner")||changes.isEmpty())return;auto inner=node["inner"].toObject();
    for(auto key:{"pen","brush","width","transparent","flag3","rotation"})if(changes.contains(key)||(QString(key)=="transparent"&&changes.contains("filled")))inner[key]=node[key];
    // A loaded picture belongs to the inner outline, as the original's LoadBitmap passes it on. The circle keeps the same
    // picture with its own corners unchanged, as the original writes it.
    if(changes.contains("bitmap")){
        if(node.contains("bitmap")){inner["bitmap"]=node["bitmap"];inner["anchors"]=node["anchors"];}else{inner.remove("bitmap");inner.remove("bitmap_offset");inner.remove("bitmap_size");}
        if(ownAnchors.isArray())node["anchors"]=ownAnchors;else node.remove("anchors");
    }
    node["inner"]=inner;
}
QJsonObject Project::componentNode(const QJsonObject &placement) const {
    auto node=libraryNode(placement["library"].toString(),placement["index"].toInt(-1));if(placement["otherSide"].toBool())switchSide(node);const auto ownAnchors=node.value("anchors");
    for(auto key:{"id","value","description","text","width","pen","brush","diameter","text_kind","font","path","filled","back","group_flags","label","extra","reference","flag2","style","rotation","flag3","bitmap","anchors","group_value"})if(placement.contains(key))node[key]=placement[key];
    if(node.contains("filled"))node["transparent"]=node["filled"];
    if(placement.contains("bitmap")&&placement["bitmap"].toString().isEmpty()){node.remove("bitmap");node.remove("bitmap_offset");node.remove("bitmap_size");}
    forwardToInner(node,placement,ownAnchors);if(placement.contains("nested"))applyNested(node,placement["nested"].toObject());
    if(node.contains("reference")&&node.contains("children"))node=withReferenceTerminal(node,node["reference"].toInt());if(node.contains("children"))node["id"]=componentId(node);return node;
}
QJsonObject Project::legacyNode(int index,bool reference) const {
    auto node=legacy["objects"].toArray()[index].toObject();const auto edit=edits.value(QString::number(index)).toObject();if(edit.value("otherSide").toBool())switchSide(node);
    const auto ownAnchors=node.value("anchors");
    for(auto it=edit.begin();it!=edit.end();++it)node[it.key()]=it.value();if(node.contains("filled"))node["transparent"]=node["filled"];
    // An empty picture removes the one of the file (a fill colour was chosen instead).
    if(edit.contains("bitmap")&&edit["bitmap"].toString().isEmpty()){node.remove("bitmap");node.remove("bitmap_offset");node.remove("bitmap_size");}
    forwardToInner(node,edit,ownAnchors);if(edit.contains("nested"))applyNested(node,edit["nested"].toObject());
    if(!reference)node.remove("reference");else if(node.contains("reference")&&node.contains("children"))node=withReferenceTerminal(node,node["reference"].toInt());
    if(node.contains("children"))node["id"]=componentId(node);return node;
}
QString Project::nextId(const QString &pattern) const {
    if(pattern.trimmed().isEmpty())return {};
    QString prefix=pattern;prefix.remove(QRegularExpression("[#\\d]+$"));if(prefix.isEmpty())prefix="X";return prefix+QString::number(nextNumber(pattern));
}
int Project::nextNumber(const QString &pattern) const {
    QString prefix=pattern;prefix.remove(QRegularExpression("[#\\d]+$"));if(prefix.isEmpty())prefix="X";
    QSet<QString> used;std::function<void(const QJsonObject&)> collect=[&](const QJsonObject &n){if(n["deleted"].toBool())return;if(n.contains("id"))used.insert(componentId(n).toUpper());for(auto v:n["children"].toArray())collect(v.toObject());};
    for(int i=0;i<legacy["objects"].toArray().size();i++)collect(legacyNode(i));
    for(auto v:additions){auto node=v.toObject();if(node["type"]=="component")node=componentNode(node);collect(node);}
    int n=1;while(used.contains((prefix+QString::number(n)).toUpper()))++n;return n;
}
QString Project::rawId(const QString &kind,int index) const {
    if(kind=="legacy"){const auto edit=edits.value(QString::number(index)).toObject();return edit.contains("id")?edit["id"].toString():legacy["objects"].toArray().at(index).toObject()["id"].toString();}
    const auto placement=additions.at(index).toObject();if(placement.contains("id")||placement["type"]!="component")return placement["id"].toString();
    return libraryNode(placement["library"].toString(),placement["index"].toInt())["id"].toString();
}
QJsonArray Project::billOfMaterials() const {
    // Like LochMaster's parts list: every group, nested ones included, that is a component (first group flag) and is
    // marked "Erscheint in Stückliste" (second flag). Groups without stored flags count as listed components.
    // Each entry also carries the name, the enclosing part ("Teil von") and the extra fields for the Excel columns.
    QJsonArray result;std::function<void(QJsonObject,QString)> add=[&](QJsonObject n,QString partOf){
        if(n.value("deleted").toBool()||!n.contains("children"))return;
        const auto flags=n["group_flags"].toArray();const bool selection=n["label"]=="OpenLochSelectionGroup"&&n["id"].toString().isEmpty(); // groups of older OpenLoch files
        const bool component=!selection&&(flags.isEmpty()||flags.at(0).toBool());
        if(component&&(flags.size()<2||flags.at(1).toBool()))result.append(QJsonObject{{"id",componentId(n)},{"value",n["value"]},{"description",n["description"]},{"name",n["label"].toString().isEmpty()?n["description"]:n["label"]},{"partOf",partOf},{"extra",n["extra"]}});
        for(auto v:n["children"].toArray())add(v.toObject(),component?componentId(n):partOf);
    };
    for(int i=0;i<legacy["objects"].toArray().size();i++)add(legacyNode(i),{});
    for(auto v:additions){auto n=v.toObject();if(n["type"]=="component")add(componentNode(n),{});else if(QStringList{"resistor","capacitor","diode"}.contains(n["type"].toString()))result.append(QJsonObject{{"id",n["text"]},{"value",""},{"description",n["type"]}});}
    return result;
}
// "Neu nummerieren" like the original: every part, also one inside another part, whose Kennung contains "#" gets the
// next number of exactly that Kennung, in the order of the document; parts with other Kennungen keep theirs.
void Project::renumber(){
    QHash<QString,int> counters;
    for(const auto &placed:placedObjects()){
        int topValue=-1;QJsonObject nested;
        std::function<void(const QJsonObject&,const QString&)> walk=[&](const QJsonObject &n,const QString &path){
            if(!n.contains("children"))return;const QString raw=path.isEmpty()?rawId(placed.kind,placed.index):n["id"].toString();
            if(n["group_flags"].toArray().at(0).toBool()&&raw.contains('#')){const int value=++counters[raw];if(path.isEmpty())topValue=value;else nested[path]=value;}
            const auto children=n["children"].toArray();for(int k=0;k<children.size();k++)walk(children[k].toObject(),path.isEmpty()?QString::number(k):path+'/'+QString::number(k));
        };
        walk(placed.node,{});if(topValue<0&&nested.isEmpty())continue;
        auto stored=placed.kind=="legacy"?edits.value(QString::number(placed.index)).toObject():additions.at(placed.index).toObject();
        if(topValue>=0)stored["group_value"]=topValue;
        if(!nested.isEmpty()){auto all=stored["nested"].toObject();for(auto it=nested.begin();it!=nested.end();++it){auto entry=all[it.key()].toObject();entry["group_value"]=it.value();all[it.key()]=entry;}stored["nested"]=all;}
        if(placed.kind=="legacy")edits[QString::number(placed.index)]=stored;else additions[placed.index]=stored;
    }
}
void Project::switchBoard(int index){
    if(index==activeBoard||boards.isEmpty())return;if(index<0||index>=boards.size())throw FormatError(ui("Platine fehlt"));
    auto current=QJsonDocument::fromJson(encode()).object();auto pages=current["boards"].toArray();auto next=Project::decode(QJsonDocument(pages[index].toObject()).toJson());next.boards=pages;next.activeBoard=index;*this=std::move(next);
}
void Project::addBoard(bool duplicate){
    if(boards.size()>=100)throw FormatError(ui("Höchstens 100 Platinen pro Projekt"));
    auto current=QJsonDocument::fromJson(encode()).object();auto pages=current["boards"].toArray();current.remove("boards");current.remove("activeBoard");if(pages.isEmpty())pages.append(current);
    auto next=duplicate?Project::decode(QJsonDocument(current).toJson()):Project();if(duplicate)next.renewIds();next.width=width;next.height=height;next.title=duplicate?title+ui(" – Kopie"):ui("Neue Platine ")+QString::number(pages.size()+1);pages.append(QJsonDocument::fromJson(next.encode()).object());next.boards=pages;next.activeBoard=pages.size()-1;*this=std::move(next);
}
void Project::moveBoard(int index){
    if(boards.size()<2)return;index=qBound(0,index,int(boards.size())-1);if(index==activeBoard)return;
    auto pages=QJsonDocument::fromJson(encode()).object()["boards"].toArray();const auto page=pages.takeAt(activeBoard);pages.insert(index,page);boards=pages;activeBoard=index;
}
void Project::appendBoards(const Project &other){
    auto pagesOf=[](const Project &p){auto root=QJsonDocument::fromJson(p.encode()).object();auto pages=root["boards"].toArray();root.remove("boards");root.remove("activeBoard");if(pages.isEmpty())pages.append(root);return pages;};
    auto pages=pagesOf(*this);const auto added=pagesOf(other);if(pages.size()+added.size()>100)throw FormatError(ui("Höchstens 100 Platinen pro Projekt"));
    // The added boards are copies: their objects get new identifiers.
    const int first=int(pages.size());for(auto v:added){auto board=Project::decode(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));board.renewIds();pages.append(QJsonDocument::fromJson(board.encode()).object());}
    auto next=Project::decode(QJsonDocument(pages[first].toObject()).toJson());next.boards=pages;next.activeBoard=first;*this=std::move(next);
}
void Project::removeBoard(){
    if(boards.size()<2)return;auto pages=QJsonDocument::fromJson(encode()).object()["boards"].toArray();pages.removeAt(activeBoard);int index=qMin(activeBoard,int(pages.size()-1));auto next=Project::decode(QJsonDocument(pages[index].toObject()).toJson());if(pages.size()>1){next.boards=pages;next.activeBoard=index;}*this=std::move(next);
}
Project Project::layoutProject() const{
    // Export the current underlay first, including a generated perfboard, and take its embedded layout document.
    auto single=*this;single.boards={};auto exported=writeLegacyProject(single);auto board=LegacyReader(exported).read(true)["board"].toObject();
    Project layout;layout.original=exported.mid(board["start"].toInteger(),board["end"].toInteger()-board["start"].toInteger())+QByteArray::fromHex("0201");layout.sourceKind="lmb";layout.sourceName="Layout.LMB";
    layout.legacy=LegacyReader(layout.original).read(false);layout.title=legacyTitle(layout.legacy);layout.width=width;layout.height=height;return layout;
}
void Project::applyLayout(const Project &layout){boardSource=addLibrary(writeLegacyDocument(layout),"Layout.LMB","lmb");}
void Project::setOffset(QPointF next){
    const QPointF delta=next-offset();if(delta.isNull())return;const QPointF before=origin();
    auto layout=layoutProject();layout.boardSettings["offset"]=QJsonArray{next.x(),next.y()};applyLayout(layout);
    boardSettings["offset"]=QJsonArray{next.x(),next.y()};
    for(int i=0;i<additions.size();i++){auto n=additions[i].toObject();
        for(auto [x,y]:{std::pair{"x","y"},std::pair{"x2","y2"}})if(n.contains(x)){n[x]=n[x].toDouble()-delta.x();n[y]=n[y].toDouble()-delta.y();}additions[i]=n;}
    userOrigin=QJsonArray{before.x()-delta.x(),before.y()-delta.y()};
}
Project Project::fromLegacyBytes(const QByteArray &bytes,const QString &suffix,const QString &name){
    Project p;p.sourceName=name;p.sourceKind=suffix;p.original=bytes;p.legacy=LegacyReader(bytes).read(suffix=="lm4");p.title=legacyTitle(p.legacy);
    p.notesRtf=bytes.mid(p.legacy["annotations_offset"].toInteger(),p.legacy["annotations_size"].toInteger());p.notes=plainNotes(p.notesRtf);
    auto size=p.legacy["size"].toArray();p.width=size[0].toDouble();p.height=size[1].toDouble();p.assignIds();return p;
}
void Project::rotateBoard(){
    auto rotate=[](Project &p){
        const double oldHeight=p.height;auto turn=[&](QPointF v){return QPointF(oldHeight-v.y(),v.x());};const QPointF shift=p.offset();
        for(int i=0;i<p.legacy.value("objects").toArray().size();i++){auto key=QString::number(i);auto n=p.edits.value(key).toObject();if(n.value("deleted").toBool())continue;auto pivot=componentAnchor(p.legacy.value("objects").toArray()[i].toObject())-shift;auto a=p.moves.value(key).toArray();auto offset=turn(pivot+QPointF(a.at(0).toDouble(),a.at(1).toDouble()))-pivot;p.moves[key]=QJsonArray{offset.x(),offset.y()};n["angle"]=std::remainder(n["angle"].toDouble()+90,360.0);p.edits[key]=n;}
        for(int i=0;i<p.additions.size();i++){auto n=p.additions[i].toObject();QPointF at(n["x"].toDouble(),n["y"].toDouble());auto next=turn(at);n["x"]=next.x();n["y"]=next.y();if(n.contains("x2")){n["x2"]=n["x2"].toDouble()+next.x()-at.x();n["y2"]=n["y2"].toDouble()+next.y()-at.y();}n["angle"]=std::remainder(n["angle"].toDouble()+90,360.0);p.additions[i]=n;}
        std::swap(p.width,p.height);
    };
    // Export the current underlay first, including a generated perfboard. It
    // must rotate with the design even when its margins are off-grid.
    auto templateProject=layoutProject();rotate(templateProject);auto bytes=writeLegacyDocument(templateProject);
    Project next=*this;rotate(next);next.boardSource=next.addLibrary(bytes,"Gedrehte Platine.LMB","lmb");Project::decode(next.encode());*this=std::move(next);
}
void Project::save(const QString &path) const {
    const auto suffix=QFileInfo(path).suffix().toLower();
    if(!QStringList{"openloch","lm4","lib","lmb"}.contains(suffix))throw FormatError(ui("Unbekanntes Speicherformat"));
    auto data=suffix=="lm4"?writeLegacyProject(*this):(suffix=="lib"||suffix=="lmb")?writeLegacyDocument(*this,false,false,suffix=="lmb"):encode();
    if(suffix=="openloch")Project::decode(data);
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))throw FormatError(file.errorString());
    if(data.size()>128*1024*1024)throw FormatError(ui("Projekt ist zu groß zum Speichern"));
    if(file.write(data)!=data.size()||!file.commit())throw FormatError(file.errorString());
}
QList<Project::Placed> Project::placedObjects() const{
    struct Entry{double z;Placed placed;};QList<Entry> entries;auto point=[](const QJsonValue &v){const auto a=v.toArray();return QPointF(a.at(0).toDouble(),a.at(1).toDouble());};
    const QPointF shift=-offset();const auto objects=legacy.value("objects").toArray();
    for(int i=0;i<objects.size();i++){
        const auto node=legacyNode(i);if(node["deleted"].toBool())continue;const QPointF at=shift+point(moves.value(QString::number(i)));
        entries.append({node["z"].toDouble(10+i*.001),{"legacy",i,node,objectTransform(node,componentAnchor(objects[i].toObject()))*QTransform::fromTranslate(at.x(),at.y())}});
    }
    for(int i=0;i<additions.size();i++){
        const auto stored=additions[i].toObject();if(stored["deleted"].toBool())continue;
        if(stored["type"]=="component"){
            const auto anchor=componentAnchor(libraryNode(stored["library"].toString(),stored["index"].toInt()));const QPointF at=QPointF(stored["x"].toDouble(),stored["y"].toDouble())-anchor;
            entries.append({stored["z"].toDouble(50+i*.001),{"new",i,componentNode(stored),objectTransform(stored,anchor)*QTransform::fromTranslate(at.x(),at.y())}});
        }else entries.append({stored["z"].toDouble(50+i*.001),{"new",i,stored,{}}});
    }
    std::stable_sort(entries.begin(),entries.end(),[](const Entry &a,const Entry &b){return a.z<b.z;});
    QList<Placed> result;for(const auto &e:entries)result.append(e.placed);return result;
}
void Project::assignIds(QSet<QString> *taken){
    QSet<QString> own;QSet<QString> &seen=taken?*taken:own;QJsonObject given;
    if(!documents::isId(boardId)||seen.contains(boardId))boardId=documents::newId();
    seen.insert(boardId);
    const int count=int(legacy.value("objects").toArray().size());   // value(): reading must not add the key to a project without import
    for(int i=0;i<count;i++){
        QString id=uids.value(QString::number(i)).toString();
        if(!documents::isId(id)||seen.contains(id))id=documents::newId();
        seen.insert(id);given[QString::number(i)]=id;
    }
    uids=given;
    for(int i=0;i<additions.size();i++){
        QJsonObject node=additions[i].toObject();const QString id=node["uid"].toString();
        if(documents::isId(id)&&!seen.contains(id)){seen.insert(id);continue;}
        node["uid"]=documents::newId();seen.insert(node["uid"].toString());additions[i]=node;
    }
}
void Project::renewIds(){
    boardId.clear();uids={};for(int i=0;i<additions.size();i++){auto node=additions[i].toObject();node.remove("uid");additions[i]=node;}
    assignIds();
}
QString Project::uidOf(const QString &kind,int index) const{
    if(kind=="legacy")return uids.value(QString::number(index)).toString();
    return index>=0&&index<additions.size()?additions[index].toObject()["uid"].toString():QString();
}
// The parts of the board shown; the other boards are pages of their own. `own` are the part's fields as stored (its edit
// or the addition); `node` is the part without a chosen Bezugspunkt, so that the order of its connection points, which
// its pins follow, does not change with it.
static QList<Project::Component> partsOf(const Project &p){
    QList<Project::Component> list;
    auto part=[&](const QString &kind,int index,const QJsonObject &node,const QJsonObject &own){
        if(node.value("deleted").toBool())return;
        Project::Component c;
        if(node.contains("children")){
            // Groups marked as part, as the parts list counts them (no flags: a part); not the selection groups of older files.
            const auto flags=node["group_flags"].toArray();const bool selection=node["label"]=="OpenLochSelectionGroup"&&node["id"].toString().isEmpty();
            if(selection||(!flags.isEmpty()&&!flags.at(0).toBool()))return;
            c.designator=componentId(node);c.value=node["value"].toString();
        }else if(QStringList{"resistor","capacitor","diode"}.contains(node["type"].toString())){c.designator=node["text"].toString();c.value=node["value"].toString();}
        else return;
        c.uid=p.uidOf(kind,index);c.id=own["component"].toString(c.uid);c.kind=kind;c.index=index;c.board=p.activeBoard;
        // Pins: as named for the project, else by the names the library gave their leads (all of them, none twice), else numbered.
        const auto points=namedConnectionPoints(node);const int count=int(points.size());const auto named=own["pins"].toArray();
        QStringList labels;for(const auto &point:points)labels<<point.second.trimmed();
        const bool labelled=!labels.contains(QString())&&QSet<QString>(labels.begin(),labels.end()).size()==labels.size();
        for(int pin=0;pin<count;pin++)c.pins<<(named.size()==count?named[pin].toString():labelled?labels[pin]:QString::number(pin+1));
        list.append(c);
    };
    for(int i=0;i<p.legacy.value("objects").toArray().size();i++)part("legacy",i,p.legacyNode(i,false),p.edits.value(QString::number(i)).toObject());
    for(int i=0;i<p.additions.size();i++){
        const auto stored=p.additions[i].toObject();auto plain=stored;plain.remove("reference");
        part("new",i,stored["type"]=="component"?p.componentNode(plain):stored,stored);
    }
    return list;
}
QList<Project::Component> Project::components() const{
    if(boards.isEmpty())return partsOf(*this);
    QList<Component> list;
    for(int b=0;b<boards.size();b++){
        if(b==activeBoard){list+=partsOf(*this);continue;}
        for(auto c:partsOf(Project::decode(QJsonDocument(boards[b].toObject()).toJson(QJsonDocument::Compact)))){c.board=b;list.append(c);}
    }
    return list;
}
QString Project::componentOf(const QString &kind,int index) const{
    for(const auto &c:partsOf(*this))if(c.kind==kind&&c.index==index)return c.id;
    return {};
}
// Sets or (an undefined value) removes an own field of a part, found by its identifier on any board.
static bool setPartField(Project &p,const QString &uid,const QString &key,const QJsonValue &value){
    for(const auto &c:p.components()){
        if(c.uid!=uid)continue;
        if(!p.boards.isEmpty()&&c.board!=p.activeBoard){
            auto page=Project::decode(QJsonDocument(p.boards[c.board].toObject()).toJson(QJsonDocument::Compact));
            if(!setPartField(page,uid,key,value))return false;
            p.boards[c.board]=QJsonDocument::fromJson(page.encode()).object();return true;
        }
        const auto at=QString::number(c.index);QJsonObject own=c.kind=="legacy"?p.edits.value(at).toObject():p.additions[c.index].toObject();
        if(value.isUndefined())own.remove(key);else own[key]=value;
        // An edit without fields is no edit, so that an untouched object stays as read.
        if(c.kind=="legacy"){if(own.isEmpty())p.edits.remove(at);else p.edits[at]=own;}else p.additions[c.index]=own;
        return true;
    }
    return false;
}
bool Project::linkComponent(const QString &uid,const QString &component){
    if(!component.isEmpty()&&!documents::isId(component))return false;
    return setPartField(*this,uid,"component",component.isEmpty()||component==uid?QJsonValue(QJsonValue::Undefined):QJsonValue(component));
}
bool Project::setPins(const QString &uid,const QStringList &pins){
    QSet<QString> seen;for(const auto &pin:pins){if(pin.trimmed().isEmpty()||pin.size()>64||seen.contains(pin))return false;seen.insert(pin);}
    return setPartField(*this,uid,"pins",pins.isEmpty()?QJsonValue(QJsonValue::Undefined):QJsonValue(QJsonArray::fromStringList(pins)));
}
bool Project::setComponent(const QString &id,const QString &designator,const QString &value){
    for(const auto &c:components()){
        if(c.id!=id)continue;
        if(!boards.isEmpty()&&c.board!=activeBoard){
            auto page=Project::decode(QJsonDocument(boards[c.board].toObject()).toJson(QJsonDocument::Compact));
            if(!page.setComponent(id,designator,value))return false;
            boards[c.board]=QJsonDocument::fromJson(page.encode()).object();return true;
        }
        QJsonObject change=c.kind=="legacy"?edits[QString::number(c.index)].toObject():additions[c.index].toObject();
        const QString type=c.kind=="new"?change["type"].toString():QString();
        if(type=="resistor"||type=="capacitor"||type=="diode"){change["text"]=designator;change["value"]=value;}
        else{
            // As LochMaster numbers parts: the Kennung up to "#", the number apart.
            static const QRegularExpression numbered(QStringLiteral("^(.*\\D)(\\d+)$"));const auto match=numbered.match(designator);
            change["id"]=match.hasMatch()?match.captured(1)+'#':designator;change["group_value"]=match.hasMatch()?match.captured(2).toInt():0;change["value"]=value;
        }
        if(c.kind=="legacy")edits[QString::number(c.index)]=change;else additions[c.index]=change;
        return true;
    }
    return false;
}
}
