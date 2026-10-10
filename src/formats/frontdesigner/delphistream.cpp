#include "delphistream.h"
#include "language.h"
#include "legacy_reader.h"
#include <algorithm>
#include <QtEndian>
#include <cmath>
#include <limits>

namespace openloch::frontdesigner {
namespace {
enum Tag:quint8 {Int8=2,Int16=3,Int32=4,Extended=5,ShortString=6,False=8,True=9,LongString=12,WideString=18,Utf8String=20};
constexpr qint32 maxString=16*1024*1024;
// Windows-1252 differs from Latin-1 only in 0x80–0x9F; Qt cannot be relied on to know it on every platform.
constexpr char16_t cp1252High[32]={0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,
    0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
}
QString fromWindows1252(const QByteArray &b){
    QString s;s.reserve(b.size());
    for(char c:b){const quint8 u=quint8(c);s.append(QChar(u>=0x80&&u<0xa0?cp1252High[u-0x80]:char16_t(u)));}
    return s;
}
QByteArray toWindows1252(const QString &s){
    QByteArray b;b.reserve(s.size());
    for(QChar c:s){
        const char16_t u=c.unicode();char out='?';
        if(u<0x80||(u>=0xa0&&u<=0xff))out=char(u);
        else for(int i=0;i<32;i++)if(cp1252High[i]==u){out=char(0x80+i);break;}
        b.append(out);
    }
    return b;
}

double fromExtended(const char *b){
    const quint64 mantissa=qFromLittleEndian<quint64>(b);const quint16 top=qFromLittleEndian<quint16>(b+8);
    const bool negative=top&0x8000;const int exponent=top&0x7fff;
    if(exponent==0x7fff)return mantissa<<1?std::numeric_limits<double>::quiet_NaN():(negative?-1:1)*std::numeric_limits<double>::infinity();
    if(mantissa==0)return negative?-0.0:0.0;
    const double v=std::ldexp(double(mantissa),exponent-16383-63);return negative?-v:v;
}
QByteArray toExtended(double v){
    QByteArray out(10,0);quint64 mantissa=0;quint16 top=std::signbit(v)?0x8000:0;
    if(std::isnan(v)){top|=0x7fff;mantissa=0xc000000000000000ULL;}
    else if(std::isinf(v)){top|=0x7fff;mantissa=0x8000000000000000ULL;}
    else if(v!=0){int exponent=0;const double fraction=std::frexp(std::abs(v),&exponent);mantissa=quint64(std::ldexp(fraction,64));top|=quint16(exponent-1+16383);}
    qToLittleEndian(mantissa,out.data());qToLittleEndian(top,out.data()+8);return out;
}

DelphiReader::DelphiReader(QByteArray bytes):data(std::move(bytes)){}
void DelphiReader::fail(const QString &why) const{throw FormatError(ui("%1 (Position 0x%2)").arg(why).arg(pos,0,16));}
QByteArray DelphiReader::raw(qsizetype n){
    if(n<0||n>data.size()-pos)fail(ui("Datei ist unvollständig"));
    const QByteArray out=data.mid(pos,n);pos+=n;return out;
}
quint8 DelphiReader::tag(){return quint8(raw(1)[0]);}
qint32 DelphiReader::integer(){
    switch(tag()){
    case Int8:return qint8(raw(1)[0]);
    case Int16:return qFromLittleEndian<qint16>(raw(2).constData());
    case Int32:return qFromLittleEndian<qint32>(raw(4).constData());
    default:pos--;fail(ui("Ungültiger Integer-Typ"));
    }
}
QByteArray DelphiReader::extendedRaw(){
    if(peek(1)!=QByteArray(1,char(Extended))){const double v=real();return toExtended(v);}
    tag();return raw(10);
}
double DelphiReader::real(){
    const QByteArray t=peek(1);if(t.isEmpty())fail(ui("Datei ist unvollständig"));
    if(quint8(t[0])==Extended){tag();return fromExtended(raw(10).constData());}
    if(quint8(t[0])==Int8||quint8(t[0])==Int16||quint8(t[0])==Int32)return integer();
    fail(ui("Ungültige Gleitkommazahl"));
}
bool DelphiReader::boolean(){
    const quint8 t=tag();if(t!=False&&t!=True){pos--;fail(ui("Ungültiger Boolean-Typ"));}return t==True;
}
QString DelphiReader::string(){
    const quint8 t=tag();qint32 n=0;
    switch(t){
    case ShortString:n=quint8(raw(1)[0]);return fromWindows1252(raw(n));
    case LongString:case Utf8String:case WideString:
        n=qFromLittleEndian<qint32>(raw(4).constData());if(n<0||n>maxString)fail(ui("Ungültige Textlänge"));
        if(t==WideString){const QByteArray b=raw(qsizetype(n)*2);return QString::fromUtf16(reinterpret_cast<const char16_t*>(b.constData()),n);}
        if(t==Utf8String)return QString::fromUtf8(raw(n));
        return fromWindows1252(raw(n));
    default:pos--;fail(ui("Ungültiger Texttyp"));
    }
}

void DelphiWriter::integer(qint64 v){
    char b[4];
    if(v>=-128&&v<=127){bytes.append(char(Int8));bytes.append(char(qint8(v)));}
    else if(v>=-32768&&v<=32767){bytes.append(char(Int16));qToLittleEndian(qint16(v),b);bytes.append(b,2);}
    else{bytes.append(char(Int32));qToLittleEndian(qint32(std::clamp<qint64>(v,std::numeric_limits<qint32>::min(),std::numeric_limits<qint32>::max())),b);bytes.append(b,4);}
}
void DelphiWriter::real(double v){bytes.append(char(Extended));bytes.append(toExtended(v));}
void DelphiWriter::extendedRaw(const QByteArray &ten){bytes.append(char(Extended));bytes.append(ten.left(10).leftJustified(10,'\0'));}
void DelphiWriter::boolean(bool v){bytes.append(char(v?True:False));}
// Like the original writer: UTF-8 when it is shorter than UTF-16 (pure ASCII up to 255 bytes as a short string),
// otherwise UTF-16; an empty text is an empty UTF-16 string.
void DelphiWriter::string(const QString &v){
    const QByteArray utf8=v.toUtf8();char b[4];
    if(utf8.size()<v.size()*2){
        bool ascii=true;for(char c:utf8)if(quint8(c)>0x7f){ascii=false;break;}
        if(!ascii){bytes.append(char(Utf8String));qToLittleEndian(qint32(utf8.size()),b);bytes.append(b,4);}
        else if(utf8.size()<=255){bytes.append(char(ShortString));bytes.append(char(utf8.size()));}
        else{bytes.append(char(LongString));qToLittleEndian(qint32(utf8.size()),b);bytes.append(b,4);}
        bytes.append(utf8);return;
    }
    bytes.append(char(WideString));qToLittleEndian(qint32(v.size()),b);bytes.append(b,4);
    bytes.append(reinterpret_cast<const char*>(v.utf16()),v.size()*2);
}
}
