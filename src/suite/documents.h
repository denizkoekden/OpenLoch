#pragma once
#include <QIcon>
#include <QList>
#include <QString>
#include <optional>
namespace openloch::suite {
// The four kinds of document of the suite (docs/suite.md). The id is stable and goes into
// object names and settings; the name is the visible one.
enum class Kind { Schematic, Perfboard, Pcb, FrontPanel };
struct KindInfo {
    Kind kind;
    QString id;        // schematic, perfboard, pcb, frontpanel
    QString name;      // Schaltplan, Lochraster, Leiterplatte, Frontplatte
    QString summary;   // what one makes with it, in a few words
    bool available;    // false while the module has no editor yet
};
// In the order of the start screen: from the circuit to the box.
QList<KindInfo> documentKinds();
KindInfo kindInfo(Kind kind);
// The kind whose editor opens this file: by its ending, for projects the kind of their active document, and for the
// endings LochMaster, FrontDesigner and sPlan share (.LIB library pages, .BAK backups) by its content. Empty for files
// no editor opens as a document (macros, pictures, settings, sPlan's library pages).
std::optional<Kind> documentKind(const QString &path);
// The filter of the open dialog: all documents, projects, each kind, backups; `first` (the kind of the asking window)
// comes before all others.
QString documentFilter(std::optional<Kind> first=std::nullopt);
// The symbol of a kind: pixel art in the style of the toolbar symbols, 32 × 32 and doubled for 64.
QIcon kindIcon(Kind kind);
}
