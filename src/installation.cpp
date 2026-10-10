#include "installation.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
namespace openloch {
namespace {
const QStringList libraryFilters{"*.LIB","*.Lib","*.lib"};
int countFiles(const QString &dir,const QStringList &filters){
    if(dir.isEmpty())return 0;int n=0;QDirIterator it(dir,filters,QDir::Files,QDirIterator::Subdirectories);while(it.hasNext()){it.next();++n;}return n;
}
QString existing(const QStringList &paths){for(const auto &p:paths)if(QFileInfo(p).isDir())return QDir::cleanPath(p);return {};}
// The drive (or Wine drive_c) holding LochMaster's program data or documents, searched from `start` upwards.
bool lochMasterDrive(const QDir &d){
    for(const char *part:{"ProgramData/LochMaster40","users/Public/Documents/LochMaster40","Users/Public/Documents/LochMaster40"})if(QFileInfo(d.filePath(part)).isDir())return true;
    return false;
}
QString driveAbove(const QString &start){
    QDir d(start);
    for(int level=0;level<10;level++){
        if(lochMasterDrive(d))return d.absolutePath();
        if(lochMasterDrive(QDir(d.filePath("drive_c"))))return d.absoluteFilePath("drive_c");
        if(!d.cdUp())break;
    }
    return {};
}
}
LochMasterInstallation findLochMaster(const QString &chosen,const QString &language){
    LochMasterInstallation found;if(chosen.trimmed().isEmpty())return found;
    const QFileInfo info(chosen);if(!info.exists())return found;
    const QString start=info.isDir()?info.absoluteFilePath():info.absolutePath();
    // A folder with library pages chosen directly (or one of its files): use it, with the pictures next to it.
    if(!QDir(start).entryList(libraryFilters,QDir::Files).isEmpty()){found.libraries=QDir::cleanPath(start);found.bitmaps=existing({start+"/../Bitmaps"});found.root=found.libraries;}
    const QString drive=driveAbove(start);
    if(!drive.isEmpty()){
        found.root=drive;const QDir data(drive+"/ProgramData/LochMaster40");
        if(found.libraries.isEmpty()){
            // One folder per language (DE, …): the interface language first, then any with library pages.
            QStringList languages=data.entryList(QDir::Dirs|QDir::NoDotAndDotDot);
            std::stable_sort(languages.begin(),languages.end(),[&](const QString &a,const QString &b){return (a.compare(language,Qt::CaseInsensitive)==0)>(b.compare(language,Qt::CaseInsensitive)==0);});
            for(const auto &l:languages)if(countFiles(data.filePath(l+"/LIB"),libraryFilters)>0){found.libraries=data.filePath(l+"/LIB");found.bitmaps=existing({data.filePath(l+"/Bitmaps")});break;}
        }
        // Wine writes "users", Windows "Users"; both are tried for case-sensitive file systems.
        const auto documents=existing({drive+"/users/Public/Documents/LochMaster40",drive+"/Users/Public/Documents/LochMaster40"});
        if(!documents.isEmpty()){found.projects=existing({documents+"/Boards"});found.templates=existing({documents+"/Board Layouts"});}
    }
    found.pages=countFiles(found.libraries,libraryFilters);found.projectCount=countFiles(found.projects,{"*.LM4","*.lm4"});found.templateCount=countFiles(found.templates,{"*.LMB","*.lmb"});
    return found;
}
QStringList lochMasterCandidates(const QStringList &near){
    QStringList out;const QString home=QDir::homePath();
    for(const auto &start:near){QDir d(start);for(int i=0;i<8;i++){if(lochMasterDrive(QDir(d.filePath("drive_c")))){out.append(d.absolutePath());break;}if(!d.cdUp())break;}}
#ifdef Q_OS_WIN
    const auto drive=qEnvironmentVariable("SystemDrive","C:");out.append(drive+"/");
#endif
    auto bottles=[&](const QString &dir){for(const auto &b:QDir(dir).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot))out.append(b.absoluteFilePath());};
    bottles(home+"/Library/Application Support/CrossOver/Bottles");  // macOS CrossOver
    out.append(home+"/.wine");                                        // Wine
    bottles(home+"/.cxoffice");                                       // CrossOver on Linux
    bottles(home+"/.local/share/wineprefixes");
    bottles(home+"/.var/app/com.usebottles.bottles/data/bottles/bottles");
    return out;
}
}
