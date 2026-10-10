#include "inifile.h"
#include "delphistream.h"
#include <QLocale>
#include <QStringDecoder>
#include <cmath>

namespace openloch::frontdesigner {
namespace {
QString decode(const QByteArray &b){
    if(b.startsWith("\xff\xfe"))return QStringDecoder(QStringDecoder::Utf16LE).decode(b.mid(2));
    if(b.startsWith("\xfe\xff"))return QStringDecoder(QStringDecoder::Utf16BE).decode(b.mid(2));
    if(b.startsWith("\xef\xbb\xbf"))return QString::fromUtf8(b.mid(3));
    QStringDecoder utf8(QStringDecoder::Utf8);const QString s=utf8.decode(b);
    return utf8.hasError()?fromWindows1252(b):s;
}
}

IniFile IniFile::parse(const QByteArray &bytes){
    IniFile ini;Section *current=nullptr;
    for(QString line:decode(bytes).split('\n')){
        line=line.trimmed();if(line.isEmpty()||line.startsWith(';'))continue;
        if(line.startsWith('[')&&line.endsWith(']')){ini.sectionList.append({line.mid(1,line.size()-2).trimmed(),{}});current=&ini.sectionList.last();continue;}
        const int eq=line.indexOf('=');if(eq<=0||!current)continue;
        current->values.append({line.left(eq).trimmed(),line.mid(eq+1).trimmed()});
    }
    return ini;
}
// The first section and the first key of a name count, as with the Windows functions the original uses.
const IniFile::Section *IniFile::find(const QString &name) const{
    for(const auto &s:sectionList)if(s.name.compare(name,Qt::CaseInsensitive)==0)return &s;
    return nullptr;
}
bool IniFile::hasSection(const QString &section) const{return find(section);}
bool IniFile::contains(const QString &section,const QString &key) const{
    if(const Section *s=find(section))for(const auto &[k,v]:s->values)if(k.compare(key,Qt::CaseInsensitive)==0)return true;
    return false;
}
QString IniFile::text(const QString &section,const QString &key,const QString &fallback) const{
    if(const Section *s=find(section))for(const auto &[k,v]:s->values)if(k.compare(key,Qt::CaseInsensitive)==0)return v;
    return fallback;
}
double IniFile::real(const QString &section,const QString &key,double fallback) const{
    bool ok=false;const double v=QLocale::c().toDouble(text(section,key).replace(',','.'),&ok);
    return ok&&std::isfinite(v)?v:fallback;
}
int IniFile::integer(const QString &section,const QString &key,int fallback) const{
    const QString t=text(section,key);bool ok=false;const int v=t.toInt(&ok);if(ok)return v;
    const double r=real(section,key,NAN);return std::isfinite(r)&&std::abs(r)<2e9?int(std::lround(r)):fallback;
}
bool IniFile::flag(const QString &section,const QString &key,bool fallback) const{
    const QString t=text(section,key).toLower();
    if(t=="true"||t=="yes")return true;
    if(t=="false"||t=="no")return false;
    bool ok=false;const int v=t.toInt(&ok);return ok?v!=0:fallback;
}
void IniFile::set(const QString &section,const QString &key,const QString &value){
    Section *s=nullptr;for(auto &x:sectionList)if(x.name.compare(section,Qt::CaseInsensitive)==0){s=&x;break;}
    if(!s){sectionList.append({section,{}});s=&sectionList.last();}
    for(auto &[k,v]:s->values)if(k.compare(key,Qt::CaseInsensitive)==0){v=value;return;}
    s->values.append({key,value});
}
void IniFile::setReal(const QString &section,const QString &key,double value){
    QLocale german(QLocale::German);german.setNumberOptions(QLocale::OmitGroupSeparator);
    set(section,key,german.toString(std::abs(value)<1e-12?0.0:value,'g',12));
}
QByteArray IniFile::toBytes() const{
    QString t;
    for(const auto &s:sectionList){
        if(!t.isEmpty())t+="\r\n";
        t+="["+s.name+"]\r\n";for(const auto &[k,v]:s.values)t+=k+"="+v+"\r\n";
    }
    return toWindows1252(t);
}

QColor colourFromDelphi(qint64 value,const QColor &fallback){
    if(value<0||value>0xffffff)return fallback;
    return QColor(int(value&0xff),int((value>>8)&0xff),int((value>>16)&0xff));
}
qint64 colourToDelphi(const QColor &c){return qint64(c.red())|qint64(c.green())<<8|qint64(c.blue())<<16;}
}
