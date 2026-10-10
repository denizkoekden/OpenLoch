#pragma once
#include "frontpanel.h"
#include <QByteArray>
#include <QString>
#include <QStringList>

// FrontDesigner projects (.FPL) and symbol libraries (.LIB) as front panel documents. The format is described in
// docs/modules/frontpanel.md. Coordinates in the files are fiftieths of a millimetre.
namespace openloch::frontdesigner {
// Reads a project with all its panels, or a library as one panel of symbols. Throws FormatError on anything it cannot
// read safely; `notes` receives what was imported only approximately (for example pictures that cannot be shown).
frontpanel::Document readFrontDesigner(const QByteArray &bytes,bool library=false,QStringList *notes=nullptr);
frontpanel::Document loadFrontDesigner(const QString &path,QStringList *notes=nullptr);

struct WriteOptions {
    // Objects that were read from a file and are unchanged are written back with their original bytes.
    bool keepUnchanged=true;
};
// Writes the document as project in the newest known format version (3,16).
QByteArray writeFrontDesigner(const frontpanel::Document &document,const WriteOptions &options={});
// Writes one panel as symbol library page.
QByteArray writeFrontDesignerLibrary(const frontpanel::Document &document,int panel,const WriteOptions &options={});
// True for the endings this module reads: .fpl and .lib.
bool isFrontDesignerFile(const QString &path);
}
