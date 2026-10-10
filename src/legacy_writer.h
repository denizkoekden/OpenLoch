#pragma once
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QSizeF>
#include <QList>
#include <functional>

namespace openloch {
struct Project;
struct Board;
// Write the original Delphi stream without depending on Wine or vendor code.
// Unchanged imported records and their opaque document tail are copied verbatim.
QByteArray writeLegacyProject(const Project &project);
// The own model (board.h): the objects of the project's board as this writer writes them, bottom first.
QJsonArray boardObjects(const Project &project);
// The LochMaster file of boards in the own model.
QByteArray writeLegacyBoards(const QList<Board> &boards);
// Throws FormatError when a read object of the board could not be written as LM4.
void checkBoardObjects(const Board &board);
// Tests: called after writeLegacyProject with the project and the bytes it wrote.
void setLegacyWriteCheck(std::function<void(const Project&,const QByteArray&)> check);
// A library page or, with `boardTemplate`, a board template (LMB), which ends with the board count 1 like the original's.
QByteArray writeLegacyDocument(const Project &project,bool groupObjects=false,bool dissolveGroups=false,bool boardTemplate=false);
// Writes objects in the reader's JSON form as a library document. A "bitmap" field with Base64 BMP bytes embeds
// that picture, as the original does for gradient-filled component bodies.
QByteArray writeLegacyObjects(const QJsonArray &objects,const QString &title,QSizeF size={100,80});
// Objects in the reader's JSON form: a base record, a text label anchored at its top-left corner, a group.
QJsonObject legacyObject(const QString &type,int kind,int width);
QJsonObject legacyLabel(const QString &text,QPointF at,int height);
QJsonObject legacyGroup(const QJsonArray &children);
// "Abmessungen" of the original's group dialog: the object `index` of a library document scaled about the centre of its
// bounds to `width` × `height` (1/100 mm), as a document of its own. Line widths and hole sizes stay, so the factor is
// refined six times like the original does; a size of 0 or below the group's line keeps that direction.
QByteArray writeScaledObject(const QByteArray &document,int index,double width,double height);
}
