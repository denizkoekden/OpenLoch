#include "settings.h"
#include "library.h"
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace openloch::schematic {
QString standardFormFolder(){
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("OpenLoch/Schaltplan-Formblätter"));
}
QString formFolder(){return QSettings().value("schematic/formFolder",standardFormFolder()).toString();}
const QList<PaperFormat> &paperFormats(){
    static const QList<PaperFormat> formats{{"A0",1189,841},{"A1",841,594},{"A2",594,420},{"A3",420,297},{"A4",297,210},{"A5",210,148},
                                            {"B4",353,250},{"B5",250,176},{"C3",458,324},{"C4",324,229},{"C5",229,162},
                                            {"Letter",279.4,215.9},{"Legal",355.6,215.9}};
    return formats;
}
int paperFormatOf(double width,double height){
    const double w=std::max(width,height),h=std::min(width,height);const auto &formats=paperFormats();
    for(int i=0;i<formats.size();i++)if(std::abs(w-formats[i].width)<.05&&std::abs(h-formats[i].height)<.05)return i;
    return -1;
}
QList<Qt::KeyboardModifiers> componentTextKeys(){
    return {Qt::AltModifier,Qt::ShiftModifier|Qt::AltModifier,Qt::ControlModifier|Qt::AltModifier,Qt::MetaModifier};
}
GeneralSettings GeneralSettings::load(){
    QSettings s;GeneralSettings g;
    g.libraryFolder=schematic::libraryFolder();g.templateFolder=schematic::templateFolder();g.formFolder=schematic::formFolder();
    g.drawingFolder=s.value("schematic/drawingFolder").toString();g.formWorkFolder=s.value("schematic/formWorkFolder").toString();g.exportFolder=s.value("schematic/exportFolder").toString();
    // Earlier there was only Alt ("componentTextsWithAlt").
    g.componentTextsWithKey=s.value("schematic/componentTextsWithKey",s.value("schematic/componentTextsWithAlt",false)).toBool();
    const Qt::KeyboardModifiers key=Qt::KeyboardModifiers::fromInt(s.value("schematic/componentTextKey",int(Qt::AltModifier)).toInt());
    if(componentTextKeys().contains(key))g.componentTextKey=key;
    g.gridContrast=std::clamp(s.value("schematic/gridContrast",g.gridContrast).toInt(),0,100);
    g.gridMarks=std::clamp(s.value("schematic/gridMarks",g.gridMarks).toInt(),0,100);
    g.gridLines=s.value("schematic/gridLines",g.gridLines).toBool();
    g.gridOverTitleBlock=s.value("schematic/gridOverTitleBlock",g.gridOverTitleBlock).toBool();g.whiteBackground=s.value("schematic/whiteBackground",g.whiteBackground).toBool();
    g.autosaveMinutes=std::clamp(s.value("schematic/autosaveMinutes",g.autosaveMinutes).toInt(),0,999);
    g.backupOnOpen=s.value("schematic/backupOnOpen",g.backupOnOpen).toBool();
    auto size=[&](const char *key,double fallback,double low,double high){const double v=s.value(QLatin1String(key),fallback).toDouble();return v>=low&&v<=high?v:fallback;};
    g.newSheetWidth=size("schematic/newSheetWidth",g.newSheetWidth,10,10000);g.newSheetHeight=size("schematic/newSheetHeight",g.newSheetHeight,10,10000);
    g.newSheetGrid=size("schematic/newSheetGrid",g.newSheetGrid,.01,100);g.newSheetForm=s.value("schematic/newSheetForm").toString();
    s.beginGroup("schematic/hotkeys");for(const QString &k:s.childKeys())g.hotkeys.insert(k,s.value(k).toString());s.endGroup();
    return g;
}
void GeneralSettings::store() const{
    QSettings s;
    auto folder=[&](const char *key,const QString &value,const QString &standard){if(value==standard)s.remove(QLatin1String(key));else s.setValue(QLatin1String(key),value);};
    folder("schematic/libraryFolder",libraryFolder,standardLibraryFolder());
    folder("schematic/templateFolder",templateFolder,standardTemplateFolder());
    folder("schematic/formFolder",formFolder,standardFormFolder());
    folder("schematic/drawingFolder",drawingFolder,QString());folder("schematic/formWorkFolder",formWorkFolder,QString());folder("schematic/exportFolder",exportFolder,QString());
    s.remove("schematic/sheetNumbers");s.remove("schematic/componentTextsWithAlt");
    s.setValue("schematic/componentTextsWithKey",componentTextsWithKey);s.setValue("schematic/componentTextKey",componentTextKey.toInt());
    s.setValue("schematic/gridContrast",gridContrast);s.setValue("schematic/gridMarks",gridMarks);
    s.setValue("schematic/gridLines",gridLines);s.setValue("schematic/gridOverTitleBlock",gridOverTitleBlock);s.setValue("schematic/whiteBackground",whiteBackground);
    s.setValue("schematic/autosaveMinutes",autosaveMinutes);s.setValue("schematic/backupOnOpen",backupOnOpen);
    s.setValue("schematic/newSheetWidth",newSheetWidth);s.setValue("schematic/newSheetHeight",newSheetHeight);s.setValue("schematic/newSheetGrid",newSheetGrid);
    if(newSheetForm.isEmpty())s.remove("schematic/newSheetForm");else s.setValue("schematic/newSheetForm",newSheetForm);
    s.remove("schematic/hotkeys");for(auto it=hotkeys.cbegin();it!=hotkeys.cend();++it)s.setValue(QStringLiteral("schematic/hotkeys/")+it.key(),it.value());
}
}
