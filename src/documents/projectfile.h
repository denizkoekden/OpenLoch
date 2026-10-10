#pragma once
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
namespace openloch::documents {
// One document of a project: its kind and the data of its module, unchanged (docs/openloch-project-format.md).
struct ProjectDocument {
    QString id;          // 32 hexadecimal digits
    QString kind;        // schematic, perfboard, pcb, frontpanel, or a kind this program does not know
    QString name;        // shown in the lists of the project
    QJsonObject data;    // the module's own JSON object, with its own format and version
    QJsonObject origin;  // imported documents: format, file and, when kept, resource
    QJsonObject rest;    // further fields of the entry, written back unchanged
};
// An OpenLoch project, version 2. Reading also takes the files that become a project, by their content: version 1
// (a perfboard document), schematics (.olsch), circuit boards (.olpcb) and front panels (.olfp). Writing always gives
// version 2. Whatever this program does not know (kinds of document, fields) is kept and written back.
struct ProjectFile {
    static constexpr int version=2;
    QString id,title,active;
    QJsonObject info;
    QList<ProjectDocument> documents;
    QJsonArray components,library,links;
    QJsonObject resources;   // SHA-256 in lowercase hexadecimal → kind, name, Base64 data
    QJsonObject rest;        // further fields of the project
    bool converted=false;    // read from an older file; saving makes it version 2

    // Throws FormatError for anything it cannot accept.
    static ProjectFile decode(const QByteArray &bytes);
    // Reads a file; a project without title takes the file's name.
    static ProjectFile load(const QString &path);
    QByteArray encode() const;
    // Encodes, reads back and replaces the file atomically.
    void save(const QString &path) const;
    int indexOf(const QString &document) const;
    // Adds a document and returns its id; the first one becomes the active document.
    QString addDocument(const QString &kind,const QString &name,const QJsonObject &data,const QJsonObject &origin={});
    // Adds bytes as resource and returns their key; the same bytes give the same key.
    QString addResource(const QByteArray &bytes,const QString &kind,const QString &name);
};
// 32 random hexadecimal digits, the form of every id of a project.
QString newId();
bool isId(const QString &text);
}
