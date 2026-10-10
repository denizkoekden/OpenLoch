#include "zip.h"
#include "formats/splan/inflate.h"
#include "language.h"
#include "legacy_reader.h"
#include <QtEndian>
#include <algorithm>
#include <array>

namespace openloch::schematic {
namespace {
[[noreturn]] void broken(){throw FormatError(ui("Die ZIP-Datei ist beschädigt oder wird nicht unterstützt."));}
quint32 crc32(const QByteArray &data){
    static const auto table=[]{
        std::array<quint32,256> t{};
        for(quint32 i=0;i<256;i++){quint32 c=i;for(int k=0;k<8;k++)c=c&1?0xEDB88320u^(c>>1):c>>1;t[i]=c;}
        return t;}();
    quint32 c=0xFFFFFFFFu;for(char ch:data)c=table[(c^uchar(ch))&0xFF]^(c>>8);
    return c^0xFFFFFFFFu;
}
void put16(QByteArray &b,quint16 v){char c[2];qToLittleEndian(v,c);b.append(c,2);}
void put32(QByteArray &b,quint32 v){char c[4];qToLittleEndian(v,c);b.append(c,4);}
quint16 get16(const QByteArray &b,qsizetype at){if(at<0||at+2>b.size())broken();return qFromLittleEndian<quint16>(b.constData()+at);}
quint32 get32(const QByteArray &b,qsizetype at){if(at<0||at+4>b.size())broken();return qFromLittleEndian<quint32>(b.constData()+at);}
// Time and date as MS-DOS keeps them: two-second steps, years from 1980.
std::pair<quint16,quint16> dosTime(const QDateTime &t){
    const QDateTime l=(t.isValid()?t:QDateTime::currentDateTime()).toLocalTime();const QDate d=l.date();const QTime h=l.time();
    const int year=std::clamp(d.year(),1980,2107);
    return {quint16((h.hour()<<11)|(h.minute()<<5)|(h.second()/2)),quint16(((year-1980)<<9)|(d.month()<<5)|d.day())};
}
QDateTime fromDos(quint16 time,quint16 date){return QDateTime(QDate(1980+(date>>9),(date>>5)&15,date&31),QTime(time>>11,(time>>5)&63,(time&31)*2));}
bool safePath(const QString &p){
    if(p.isEmpty()||p.startsWith(u'/')||p.contains(u'\\')||p.contains(u':'))return false;
    for(const auto &part:p.split(u'/'))if(part.isEmpty()||part==u"."||part==u"..")return false;
    return true;
}
}
QByteArray writeZip(const QList<ZipEntry> &entries){
    QByteArray out,central;
    for(const auto &e:entries){
        const QByteArray name=e.path.toUtf8();const quint32 crc=crc32(e.data);
        // qCompress gives a length and a zlib stream: two bytes of header, the deflate data, four of checksum.
        QByteArray packed=qCompress(e.data,9);packed=packed.size()>10?packed.mid(6,packed.size()-10):QByteArray();
        const bool deflated=!e.data.isEmpty()&&packed.size()<e.data.size();
        const QByteArray &body=deflated?packed:e.data;
        const auto [time,date]=dosTime(e.modified);const quint32 offset=quint32(out.size());
        // From "version needed" on, local and central headers are alike; names are UTF-8 (flag 11).
        auto common=[&](QByteArray &b){
            put16(b,20);put16(b,0x0800);put16(b,deflated?8:0);put16(b,time);put16(b,date);put32(b,crc);
            put32(b,quint32(body.size()));put32(b,quint32(e.data.size()));put16(b,quint16(name.size()));put16(b,0);};
        put32(out,0x04034b50);common(out);out+=name;out+=body;
        put32(central,0x02014b50);put16(central,20);common(central);put16(central,0);put16(central,0);put16(central,0);put32(central,0);put32(central,offset);central+=name;
    }
    const quint32 at=quint32(out.size());out+=central;
    put32(out,0x06054b50);put16(out,0);put16(out,0);put16(out,quint16(entries.size()));put16(out,quint16(entries.size()));
    put32(out,quint32(central.size()));put32(out,at);put16(out,0);
    return out;
}
QList<ZipEntry> readZip(const QByteArray &zip){
    // The end record lies within the last 22 bytes and a comment of up to 64 KiB.
    qsizetype end=-1;
    for(qsizetype i=zip.size()-22;i>=0&&i>=zip.size()-22-65535;i--)if(get32(zip,i)==0x06054b50){end=i;break;}
    if(end<0)broken();
    const int count=get16(zip,end+10);qsizetype at=get32(zip,end+16);
    QList<ZipEntry> out;
    for(int k=0;k<count;k++){
        if(get32(zip,at)!=0x02014b50)broken();
        const quint16 flags=get16(zip,at+8),method=get16(zip,at+10),time=get16(zip,at+12),date=get16(zip,at+14);
        const quint32 crc=get32(zip,at+16),packed=get32(zip,at+20),size=get32(zip,at+24),local=get32(zip,at+42);
        const int nameLength=get16(zip,at+28),extra=get16(zip,at+30),comment=get16(zip,at+32);
        if(at+46+nameLength>zip.size())broken();
        const QByteArray raw=zip.mid(at+46,nameLength);
        const QString name=flags&0x0800?QString::fromUtf8(raw):QString::fromLatin1(raw);
        at+=46+nameLength+extra+comment;
        if(name.endsWith(u'/'))continue;
        if(flags&1||(method!=0&&method!=8)||packed==0xFFFFFFFFu||size==0xFFFFFFFFu||!safePath(name))broken();
        if(get32(zip,local)!=0x04034b50)broken();
        const qsizetype data=qsizetype(local)+30+get16(zip,qsizetype(local)+26)+get16(zip,qsizetype(local)+28);
        if(data+qsizetype(packed)>zip.size())broken();
        QByteArray body;
        if(method==0)body=zip.mid(data,packed);
        else try{body=splan::inflateRaw(zip,data,packed,qsizetype(size));}catch(const FormatError &){broken();}
        if(body.size()!=qsizetype(size)||crc32(body)!=crc)broken();
        out.append({name,body,fromDos(time,date)});
    }
    return out;
}
}
