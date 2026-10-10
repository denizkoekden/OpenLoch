#pragma once
#include "project.h"
#include <QByteArray>
#include <QJsonArray>
#include <QList>
namespace openloch {
// A perfboard document in OpenLoch's own model (docs/openloch-project-format.md, "Lochraster-Dokument, Version 2"):
// every board a complete list of its objects, bottom first, each with its identifier in "uid". Read objects are nodes in
// the form of the LM4 reader with all changes applied; placements from libraries are resolved groups that name their
// entry in "part"; OpenLoch's own objects (wire, track, resistor, …) stay as the project stores them. The editor still
// works on Project: this model is built from it and writes the same LM4 file.
struct Board {
    // The board without its objects, in the members Project has for it: title, mode, size, notes, origin, print and
    // board settings, the copper layout (boardSource with its library) and the file the board was read from (original,
    // sourceName, sourceKind, and its document in legacy). Its own lists of objects stay empty.
    Project settings;
    QJsonArray objects;
};
struct BoardDocument {
    QList<Board> boards;
    int active=0;
    // The boards of a project; objects without identifier get one first.
    static BoardDocument fromProject(const Project &project);
    // The own format, "OpenLoch-Lochraster" version 2.
    QByteArray encode() const;
    // Throws FormatError for anything it cannot accept, also for an object it could not write as LM4.
    static BoardDocument decode(const QByteArray &bytes);
    // The LochMaster file; for a model built from a project the bytes writeLegacyProject gives for that project.
    QByteArray writeLm4() const;
};
}
