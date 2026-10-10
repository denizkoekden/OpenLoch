#pragma once
#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
namespace openloch {
// The library folders of one kind of document (docs/suite.md, "Bibliotheken"): its own folder, where new parts and
// symbols are written, and further folders that are only read. Each module reads and writes them from its existing
// settings with two free functions; the suite shows them all in one overview.
struct LibraryFolders {
    QString own;
    QStringList extra;
    bool operator==(const LibraryFolders &) const=default;
};
// A fixed folder of the program under Dokumente/OpenLoch ("Bauteile", "Frontplattensymbole", "Strichschriften",
// "Schaltplan-Bibliothek", "Makros"): its name does not change with the language of the interface.
inline QString openLochDocumentsFolder(const QString &name){
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("OpenLoch/")+name);
}
}
