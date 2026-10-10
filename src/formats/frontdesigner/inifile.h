#pragma once
#include <QByteArray>
#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>
#include <utility>

// The INI texts in which FrontDesigner keeps its settings: scale settings (.SCL and Skale0.INI … Skale12.INI), the
// lists of pens, fills and fonts, instrument cut-outs (.CUT). Sections and keys are compared without regard to case.
// Files are read as UTF-16 (with byte order mark), UTF-8 or Windows-1252; numbers may have a decimal comma.
namespace openloch::frontdesigner {
class IniFile {
public:
    static IniFile parse(const QByteArray &bytes);
    bool hasSection(const QString &section) const;
    bool contains(const QString &section,const QString &key) const;
    QString text(const QString &section,const QString &key,const QString &fallback={}) const;
    double real(const QString &section,const QString &key,double fallback) const;
    int integer(const QString &section,const QString &key,int fallback) const;
    bool flag(const QString &section,const QString &key,bool fallback) const;
    // Values are written in the order they were set, numbers with a decimal comma as the original writes them.
    void set(const QString &section,const QString &key,const QString &value);
    void setReal(const QString &section,const QString &key,double value);
    void setInteger(const QString &section,const QString &key,qint64 value){set(section,key,QString::number(value));}
    void setFlag(const QString &section,const QString &key,bool value){set(section,key,value?"1":"0");}
    // Windows-1252 text with CR LF line ends and an empty line between sections.
    QByteArray toBytes() const;
private:
    struct Section {QString name;QList<std::pair<QString,QString>> values;};
    QList<Section> sectionList;
    const Section *find(const QString &name) const;
};
// Colours as the original stores them (Delphi TColor: 0x00BBGGRR). System colours (negative values) cannot be
// resolved outside Windows and give `fallback`.
QColor colourFromDelphi(qint64 value,const QColor &fallback=Qt::black);
qint64 colourToDelphi(const QColor &colour);
}
