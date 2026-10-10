#pragma once
#include "modules/schematic/library.h"
#include "modules/schematic/model.h"
#include <QStringList>

// sPlan 7 and 8 files (.spl7, .spl8) as schematic documents. The file's records are mapped onto OpenLoch's model; what
// the model does not hold is kept with the elements (`splan` fields), so that an unchanged document is written back
// byte for byte. See docs/modules/schematic.md for the layout and the mapping.
namespace openloch::splan {
// The version in a file's first bytes (70, 80), 0 for anything else.
int version(const QByteArray &bytes);
// A sPlan file as a document. `notes` gets what OpenLoch shows differently or leaves out (shown as a partial preview).
// Throws FormatError with the place of the damage.
schematic::Document read(const QByteArray &bytes,QStringList *notes=nullptr);
// The document as a sPlan file of `version` (70 or 80). `losses` gets what the file cannot hold. Unchanged elements are
// written as read; with `keepUnchanged` false every element is written from its fields (for checks).
QByteArray write(const schematic::Document &document,int version,QStringList *losses=nullptr,bool keepUnchanged=true);
// Writes the file atomically (read back first); a failed write leaves an existing file unchanged.
void save(const schematic::Document &document,const QString &path,int version,QStringList *losses=nullptr);
// What would be lost writing the document as `version`.
QStringList losses(const schematic::Document &document,int version);
// The version of the sPlan file a document was read from, 0 for own documents.
int sourceVersion(const schematic::Document &document);
// A page of a sPlan library (.LIB) with its components and groups, each in local coordinates around its insertion
// point. Names and captions keep all their languages (see schematic::localized); pictures go with their entries.
schematic::LibraryPage readLibrary(const QByteArray &bytes,QStringList *notes=nullptr);
// The page as a sPlan library page in the frame of `original` (the page's file as read; empty for a new page of
// `version`). Entries as read that are unchanged keep their bytes and places, changed and new ones are written from
// their fields; what the page holds besides components and groups stays where it was. `losses` as for write.
QByteArray writeLibrary(const schematic::LibraryPage &page,const QByteArray &original,int version,QStringList *losses=nullptr);
// The elements of a sPlan title block (.sbk), and the pictures they show.
QList<schematic::Item> readTitleBlock(const QByteArray &bytes,QMap<QString,schematic::Resource> *resources,QStringList *notes=nullptr);
// Elements as a sPlan title block of `version`; `losses` as for write.
QByteArray writeTitleBlock(const QList<schematic::Item> &items,const QMap<QString,schematic::Resource> &resources,int version,QStringList *losses=nullptr);
}
