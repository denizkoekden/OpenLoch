#include "projectfile.h"
#include "language.h"
#include "legacy_reader.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <functional>
namespace openloch::documents {
namespace {
constexpr qint64 sizeLimit=256LL*1024*1024;
[[noreturn]] void invalid(const QString &text){throw FormatError(text);}
bool isHash(const QString &text){static const QRegularExpression hex("^[0-9a-f]{64}$");return hex.match(text).hasMatch();}
// An id that is missing is given; one that is there must have the right form, since other entries may name it.
QString idOf(const QJsonObject &object,const QString &what){
    const QJsonValue value=object.value("id");if(value.isUndefined())return newId();
    if(!value.isString()||!isId(value.toString()))invalid(ui("Ungültige Kennung im Projekt: %1").arg(what));
    return value.toString();
}
QString textOf(const QJsonObject &object,const QString &key){
    const QJsonValue value=object.value(key);if(value.isUndefined())return {};
    if(!value.isString())invalid(ui("Ungültiger Text im Projekt: %1").arg(key));
    return value.toString();
}
QJsonArray listOf(const QJsonObject &object,const QString &key){
    const QJsonValue value=object.value(key);if(value.isUndefined())return {};
    if(!value.isArray())invalid(ui("Ungültige Liste im Projekt: %1").arg(key));
    return value.toArray();
}
QJsonObject without(QJsonObject object,const QStringList &keys){for(const auto &key:keys)object.remove(key);return object;}
// Every text of a JSON tree, to find the resources that something still names.
void texts(const QJsonValue &value,const std::function<void(const QString&)> &found){
    if(value.isString())found(value.toString());
    else if(value.isArray()){for(const auto &item:value.toArray())texts(item,found);}
    else if(value.isObject()){for(const auto &item:value.toObject())texts(item,found);}
}
}

QString newId(){return QUuid::createUuid().toString(QUuid::Id128);}
bool isId(const QString &text){static const QRegularExpression hex("^[0-9a-fA-F]{32}$");return hex.match(text).hasMatch();}

ProjectFile ProjectFile::decode(const QByteArray &bytes){
    if(bytes.size()>sizeLimit)invalid(ui("Datei ist zu groß"));
    QJsonParseError error;const QJsonDocument json=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!json.isObject())invalid(ui("Keine OpenLoch-Datei"));
    const QJsonObject root=json.object();const QString format=root.value("format").toString();
    ProjectFile project;
    // Older files and the modules' own files are one document of their kind; the file is that document's data.
    const QString single=format=="OpenLoch PCB"?QStringLiteral("pcb"):format=="OpenLoch-Frontplatte"?QStringLiteral("frontpanel")
        :format=="OpenLoch Schematic"?QStringLiteral("schematic"):format=="OpenLoch"&&root.value("version").toInt(0)==1?QStringLiteral("perfboard"):QString();
    if(!single.isEmpty()){
        project.id=newId();project.title=root.value("title").toString();project.converted=true;
        project.addDocument(single,project.title,root);return project;
    }
    if(format!="OpenLoch")invalid(ui("Keine OpenLoch-Datei"));
    if(!root.value("version").isDouble())invalid(ui("Ungültige Formatversion"));
    const int number=root.value("version").toInt();
    if(number>version)invalid(ui("Die Datei stammt aus einer neueren OpenLoch-Version (Format %1)").arg(number));
    if(number!=version)invalid(ui("Ungültige Formatversion"));
    project.id=idOf(root,ui("Projekt"));project.title=textOf(root,"title");project.active=textOf(root,"active");
    if(root.contains("info")&&!root.value("info").isObject())invalid(ui("Ungültige Angaben im Projekt"));
    project.info=root.value("info").toObject();
    const QJsonArray documents=listOf(root,"documents");if(documents.isEmpty())invalid(ui("Das Projekt enthält kein Dokument"));
    QSet<QString> ids;
    for(const auto &value:documents){
        if(!value.isObject())invalid(ui("Ungültiges Dokument im Projekt"));
        const QJsonObject entry=value.toObject();ProjectDocument d;
        d.id=idOf(entry,ui("Dokument"));d.kind=textOf(entry,"kind");d.name=textOf(entry,"name");
        if(d.kind.isEmpty())invalid(ui("Dokument ohne Art im Projekt"));
        if(!entry.value("data").isObject())invalid(ui("Dokument ohne Daten im Projekt"));
        d.data=entry.value("data").toObject();
        if(entry.contains("origin")){
            if(!entry.value("origin").isObject())invalid(ui("Ungültige Herkunft eines Dokuments"));
            d.origin=entry.value("origin").toObject();textOf(d.origin,"format");textOf(d.origin,"file");
            const QString resource=textOf(d.origin,"resource");if(!resource.isEmpty()&&!isHash(resource))invalid(ui("Ungültige Herkunft eines Dokuments"));
        }
        d.rest=without(entry,{"id","kind","name","data","origin"});
        if(ids.contains(d.id))invalid(ui("Doppelte Kennung im Projekt: %1").arg(d.id));
        ids.insert(d.id);project.documents.append(d);
    }
    project.components=listOf(root,"components");project.library=listOf(root,"library");project.links=listOf(root,"links");
    QSet<QString> components;
    for(const auto &value:project.components){
        const QString id=value.toObject().value("id").toString();
        if(!value.isObject()||!isId(id)||components.contains(id))invalid(ui("Ungültiges Bauteil im Projekt"));
        components.insert(id);
    }
    for(const auto &list:{project.library,project.links})for(const auto &value:list)if(!value.isObject())invalid(ui("Ungültiger Eintrag im Projekt"));
    if(root.contains("resources")&&!root.value("resources").isObject())invalid(ui("Ungültige Ressourcen im Projekt"));
    project.resources=root.value("resources").toObject();
    for(auto it=project.resources.begin();it!=project.resources.end();++it){
        // The key is the SHA-256 of the content, so damaged or exchanged data shows.
        const QJsonObject resource=it.value().toObject();
        const auto data=QByteArray::fromBase64Encoding(resource.value("data").toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
        if(!isHash(it.key())||!it.value().isObject()||!resource.value("kind").isString()||!data
           ||QCryptographicHash::hash(*data,QCryptographicHash::Sha256).toHex()!=it.key().toLatin1())invalid(ui("Ungültige Ressource im Projekt: %1").arg(it.key()));
    }
    for(const auto &d:project.documents){
        const QString key=d.origin.value("resource").toString();if(!key.isEmpty()&&!project.resources.contains(key))invalid(ui("Unbekannte Ressource: %1").arg(key));
    }
    if(project.indexOf(project.active)<0)project.active=project.documents.first().id;
    project.rest=without(root,{"format","version","id","title","info","active","documents","components","library","links","resources"});
    return project;
}
ProjectFile ProjectFile::load(const QString &path){
    QFile file(path);if(!file.open(QIODevice::ReadOnly))invalid(file.errorString());
    if(file.size()>sizeLimit)invalid(ui("Datei ist zu groß"));
    ProjectFile project=decode(file.readAll());if(project.title.isEmpty())project.title=QFileInfo(path).completeBaseName();
    // A file that becomes a project names its document after itself when the document has no title of its own.
    if(project.converted&&project.documents.size()==1&&project.documents[0].name.isEmpty())project.documents[0].name=QFileInfo(path).completeBaseName();
    return project;
}
QByteArray ProjectFile::encode() const{
    QJsonObject root=rest;
    root["format"]=QStringLiteral("OpenLoch");root["version"]=version;root["id"]=id;root["title"]=title;
    if(!info.isEmpty())root["info"]=info;
    root["active"]=indexOf(active)>=0||documents.isEmpty()?active:documents.first().id;
    QJsonArray list;
    for(const auto &d:documents){
        QJsonObject entry=d.rest;entry["id"]=d.id;entry["kind"]=d.kind;entry["name"]=d.name;entry["data"]=d.data;
        if(!d.origin.isEmpty())entry["origin"]=d.origin;
        list.append(entry);
    }
    root["documents"]=list;
    if(!components.isEmpty())root["components"]=components;
    if(!library.isEmpty())root["library"]=library;
    if(!links.isEmpty())root["links"]=links;
    // Resources nothing names any more are left out; a name counts wherever it stands, also in fields unknown here.
    QSet<QString> named;texts(root,[&](const QString &text){if(resources.contains(text))named.insert(text);});
    QJsonObject kept;for(auto it=resources.begin();it!=resources.end();++it)if(named.contains(it.key()))kept[it.key()]=it.value();
    if(!kept.isEmpty())root["resources"]=kept;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}
void ProjectFile::save(const QString &path) const{
    const QByteArray bytes=encode();if(bytes.size()>sizeLimit)invalid(ui("Projekt ist zu groß zum Speichern"));
    decode(bytes);   // never write what could not be read back
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())invalid(file.errorString());
}
int ProjectFile::indexOf(const QString &document) const{
    for(int i=0;i<documents.size();i++)if(documents[i].id==document)return i;
    return -1;
}
QString ProjectFile::addDocument(const QString &kind,const QString &name,const QJsonObject &data,const QJsonObject &origin){
    ProjectDocument d;d.id=newId();d.kind=kind;d.name=name;d.data=data;d.origin=origin;documents.append(d);
    if(active.isEmpty())active=d.id;
    return d.id;
}
QString ProjectFile::addResource(const QByteArray &bytes,const QString &kind,const QString &name){
    const QString key=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
    resources[key]=QJsonObject{{"kind",kind},{"name",name},{"data",QString::fromLatin1(bytes.toBase64())}};
    return key;
}
}
