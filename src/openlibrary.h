#pragma once
#include <QByteArray>
#include <QImage>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QStringList>
namespace openloch {
// OpenLoch's own component libraries (libraries/*.json, CC0): the pages and parts of LochMaster's libraries with
// pictures drawn by OpenLoch. A page file holds "page", "position" (the LIB number) and "parts"; a part has a name,
// Kennung, value, a "source" for its dimensions and either a package macro or explicit pins, shapes, lines and labels.
// Lengths are millimetres relative to pin 1 (x to the right, y down); pins sit on holes of 2.54 mm.

// The parts of a page with variants and package macros expanded to explicit pins, shapes, lines and labels.
// Problems (missing source, pins off the grid, unknown package) are reported, the part is still returned if usable.
QList<QJsonObject> openLibraryParts(const QJsonObject &page,QStringList *problems=nullptr);
// One expanded part as a LochMaster component group with pin 1 at `at` (1/100 mm).
QJsonObject openLibraryPart(const QJsonObject &part,QPointF at,bool english=false);
// A whole page as a LIB file, the parts laid out on the hole grid.
QByteArray openLibraryPage(const QJsonObject &page,bool english=false);
// The page title in the interface language.
QString openLibraryTitle(const QJsonObject &page,bool english=false);
// The photo-like picture of a body: `look` names the form and colours, `size` is the body's size in mm. The same
// look, size and seed always give the same picture.
QImage openLibraryPicture(const QJsonObject &look,QSizeF size,const QString &seed);
// Changes whenever generated pages would differ: part of the cache key for the generated LIB files.
int openLibraryGeneratorVersion();
}
