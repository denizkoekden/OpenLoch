#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

// ZIP archives as the library backup ("Datensicherung") writes and reads them: files with relative paths (UTF-8, "/"
// between folders), deflated when that makes them smaller. Reading takes stored and deflated files of any ZIP without
// encryption and ZIP64.
namespace openloch::schematic {
struct ZipEntry {
    QString path;
    QByteArray data;
    QDateTime modified;
};
QByteArray writeZip(const QList<ZipEntry> &entries);
// Throws FormatError for a broken archive, encrypted or otherwise packed files, wrong checksums and paths that leave
// their folder (absolute ones, ".."); folders in the archive are left out.
QList<ZipEntry> readZip(const QByteArray &zip);
}
