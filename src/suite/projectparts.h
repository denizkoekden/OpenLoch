#pragma once
#include "documents/projectfile.h"
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
namespace openloch::suite {
// A component of a project across its documents (docs/suite.md, "Projektübersicht"), joined by the identifier the
// documents name it by: the schematic's component, to which the boards link their parts, and a board's part, to which a
// front panel links its holes.
struct ProjectPart {
    QString component;          // the joining identifier
    QString designator,value;   // from the schematic when it has the part, else from the first document that has it
    QStringList documents;      // the documents that have it, by their identifiers, in the project's order
};
// The parts of all documents, the schematic's first, then by designator (R2 before R10). `current`: the data of
// documents open in a window by their identifier, else the project's stored data counts. A document that cannot be read
// adds nothing.
QList<ProjectPart> projectParts(const documents::ProjectFile &project,const QMap<QString,QJsonObject> &current={});
// The parts list of the project: the parts of its schematic and boards (a front panel's holes are no parts to buy),
// grouped by value and the letters of their designator, as count, designators and value.
struct PartsRow {int count=0;QStringList designators;QString value;};
QList<PartsRow> partsList(const QList<ProjectPart> &parts,const documents::ProjectFile &project);
// The parts list as CSV for spreadsheets: a header, then "count;designators;value", UTF-8 with a byte order mark.
QByteArray partsListCsv(const QList<PartsRow> &rows);
}
