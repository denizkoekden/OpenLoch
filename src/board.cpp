#include "board.h"
#include "documents/projectfile.h"
#include "language.h"
#include "legacy_reader.h"
#include "legacy_writer.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QSet>
#include <QStringList>
#include <utility>
namespace openloch {
namespace {
QString base64(const QByteArray &bytes){return QString::fromLatin1(bytes.toBase64());}
// The types of OpenLoch's own objects, as version 1 stores them in "additions"; a placement from a library ("component")
// is resolved in the own model.
bool ownType(const QString &type){
    return QStringList{"wire","cut","pad","text","resistor","capacitor","ground","diode","rectangle","ellipse","polygon","polyline","drill","pin","potential","lead","solder","track","eye"}.contains(type);
}
}

BoardDocument BoardDocument::fromProject(const Project &project){
    auto own=project;own.assignIds();
    QList<Project> pages;BoardDocument d;
    if(own.boards.size()<2)pages.append(own);
    else{
        // Every board in its order, the one shown with its current state (as writeLegacyProject takes them).
        for(const auto &page:QJsonDocument::fromJson(own.encode()).object()["boards"].toArray())pages.append(Project::decode(QJsonDocument(page.toObject()).toJson(QJsonDocument::Compact)));
        d.active=own.activeBoard;
    }
    for(const auto &page:pages){
        Board board;board.objects=boardObjects(page);board.settings=page;
        auto &s=board.settings;s.additions={};s.moves={};s.edits={};s.uids={};s.boards={};s.activeBoard=0;
        // Of the libraries only the copper layout's stays; placements from libraries are resolved groups now.
        QMap<QString,LibrarySource> layout;if(!page.boardSource.isEmpty())layout.insert(page.boardSource,page.libraries.value(page.boardSource));s.libraries=layout;
        d.boards.append(board);
    }
    return d;
}
QByteArray BoardDocument::encode() const{
    QJsonArray pages;
    for(const auto &board:boards){
        const Project &p=board.settings;
        QJsonObject page{{"title",p.title},{"mode",p.mode},{"width",p.width},{"height",p.height},{"notes",p.notes},{"notesEdited",p.notesEdited},{"objects",board.objects}};
        if(!p.boardId.isEmpty())page["id"]=p.boardId;
        if(p.notesEdited&&!p.notesRtf.isEmpty())page["notesRtf"]=base64(p.notesRtf);
        if(!p.userOrigin.isEmpty())page["origin"]=p.userOrigin;
        if(!p.print.isEmpty())page["print"]=p.print;
        if(!p.boardSettings.isEmpty())page["settings"]=p.boardSettings;
        if(!p.boardSource.isEmpty()){const auto layout=p.libraries.value(p.boardSource);page["layout"]=QJsonObject{{"name",layout.name},{"kind",layout.kind},{"data",base64(layout.bytes)}};}
        if(!p.original.isEmpty())page["source"]=QJsonObject{{"name",p.sourceName},{"kind",p.sourceKind},{"data",base64(p.original)}};
        pages.append(page);
    }
    return QJsonDocument(QJsonObject{{"format","OpenLoch-Lochraster"},{"version",2},{"active",active},{"boards",pages}}).toJson();
}
BoardDocument BoardDocument::decode(const QByteArray &bytes){
    if(bytes.size()>128*1024*1024)throw FormatError(ui("Projekt ist zu groß"));
    QJsonParseError error;const auto json=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!json.isObject())throw FormatError(ui("Ungültiges OpenLoch-Projekt"));
    const auto root=json.object();
    if(root["format"]!="OpenLoch-Lochraster"||!root["version"].isDouble())throw FormatError(ui("Ungültiges OpenLoch-Projekt"));
    if(root["version"].toInt()>2)throw FormatError(ui("Die Datei stammt aus einer neueren OpenLoch-Version (Format %1)").arg(root["version"].toInt()));
    if(root["version"].toInt()!=2)throw FormatError(ui("Unbekannte Projektversion"));
    if(!root["boards"].isArray())throw FormatError(ui("Ungültige Platinenliste"));
    const auto pages=root["boards"].toArray();BoardDocument d;d.active=root["active"].toInt(-1);
    if(pages.isEmpty()||pages.size()>100||!root["active"].isDouble()||d.active<0||d.active>=pages.size())throw FormatError(ui("Ungültige aktive Platine"));
    QSet<QString> taken;qint64 count=0;
    for(const auto &value:pages){
        if(!value.isObject())throw FormatError(ui("Ungültige Platine im Projekt"));const auto page=value.toObject();
        if(!page["objects"].isArray())throw FormatError(ui("Ungültige Platine im Projekt"));
        auto objects=page["objects"].toArray();count+=objects.size();if(count>100000)throw FormatError(ui("Zu viele neue Elemente"));
        // The settings and OpenLoch's own objects are checked as version 1 checks them: as such a board.
        QJsonArray own;
        for(auto &&item:objects){
            if(!item.isObject())throw FormatError(ui("Ungültiges Zeichenelement"));auto node=item.toObject();const auto type=node["type"].toString();
            if(!type.startsWith('T')&&!ownType(type))throw FormatError(ui("Unbekanntes Zeichenelement"));
            if(node.contains("part")&&!node["part"].isObject())throw FormatError(ui("Ungültiges Bibliotheksbauteil"));
            // An identifier that is missing is given, one that comes twice renewed; one of the wrong form is an error.
            const auto uid=node.value("uid");
            if(!uid.isUndefined()&&(!uid.isString()||!documents::isId(uid.toString())))throw FormatError(ui("Ungültige Kennungen"));
            if(uid.isUndefined()||taken.contains(uid.toString()))node["uid"]=documents::newId();
            taken.insert(node["uid"].toString());item=node;
            if(!type.startsWith('T'))own.append(node);
        }
        QJsonObject v1{{"format","OpenLoch"},{"version",1},{"title",page["title"]},{"mode",page["mode"]},{"width",page["width"]},{"height",page["height"]},
            {"notes",page["notes"]},{"notesEdited",page["notesEdited"]},{"additions",own}};
        const std::pair<const char*,const char*> renamed[]={{"id","boardId"},{"notesRtf","notesRtf"},{"origin","userOrigin"},{"print","print"},{"settings","boardSettings"}};
        for(const auto &[from,to]:renamed)if(page.contains(from))v1[to]=page[from];
        if(page.contains("source")){
            if(!page["source"].isObject())throw FormatError(ui("Ungültige eingebettete Quelldatei"));const auto source=page["source"].toObject();
            v1["sourceName"]=source["name"];v1["sourceKind"]=source["kind"];v1["originalBase64"]=source["data"];
            if(!source["data"].isString()||source["data"].toString().isEmpty())throw FormatError(ui("Ungültige eingebettete Quelldatei"));
        }
        if(page.contains("layout")){
            if(!page["layout"].isObject())throw FormatError(ui("Platinenvorlage fehlt"));const auto layout=page["layout"].toObject();
            const auto data=QByteArray::fromBase64Encoding(layout["data"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);if(!data)throw FormatError(ui("Ungültige Bibliotheksdaten"));
            const auto key=QString::fromLatin1(QCryptographicHash::hash(data.decoded,QCryptographicHash::Sha256).toHex());
            v1["libraries"]=QJsonObject{{key,layout}};v1["boardSource"]=key;
        }
        Board board;board.settings=Project::decode(QJsonDocument(v1).toJson(QJsonDocument::Compact));
        auto &s=board.settings;s.additions={};s.uids={};
        if(taken.contains(s.boardId))s.boardId=documents::newId();   // a board's identifier that came before is renewed
        taken.insert(s.boardId);
        board.objects=objects;checkBoardObjects(board);d.boards.append(board);
    }
    return d;
}
QByteArray BoardDocument::writeLm4() const{return writeLegacyBoards(boards);}
}
