#pragma once
#include <QString>
#include <QStringList>
namespace openloch {
// The parts of an existing LochMaster 4 installation OpenLoch reads and never changes: the library pages, the body
// pictures of the object assistant, the sample projects and the board templates.
struct LochMasterInstallation {
    QString root;                                    // everything found lies below this folder
    QString libraries,bitmaps,projects,templates;    // empty when missing
    int pages=0,projectCount=0,templateCount=0;
    bool valid() const{return pages>0;}
};
// Finds the installation from what the user chose: LochMaster40.exe or any other file or folder inside the
// installation, the folder with the .LIB files, a Windows drive or a Wine/CrossOver bottle (the folder with drive_c).
// The library folder of `language` (DE, EN, …) is preferred when several exist.
LochMasterInstallation findLochMaster(const QString &chosen,const QString &language="DE");
// Places a LochMaster installation usually is: C:\ProgramData on Windows, CrossOver bottles on macOS, Wine prefixes
// and CrossOver on Linux, and the folders above `near` (for an OpenLoch next to a copied installation).
QStringList lochMasterCandidates(const QStringList &near={});
}
