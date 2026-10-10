#include "pictures.h"
#include <QHash>
#include <QtEndian>
#include <algorithm>
#include <cstring>

namespace openloch::pcb {
namespace {
struct Bytes {
    QByteArray b;
    Bytes &u8(quint8 v){b.append(char(v));return *this;}
    Bytes &u16(quint16 v){char c[2];qToLittleEndian(v,c);b.append(c,2);return *this;}
    Bytes &u32(quint32 v){char c[4];qToLittleEndian(v,c);b.append(c,4);return *this;}
    Bytes &i32(qint32 v){char c[4];qToLittleEndian(v,c);b.append(c,4);return *this;}
    Bytes &f32(float f){quint32 v;std::memcpy(&v,&f,4);return u32(v);}
    Bytes &raw(const QByteArray &x){b+=x;return *this;}
};

// --- GIF: the LZW compression of the image data as GIF uses it, codes packed from the lowest bit on. The code width
// grows when the next free code no longer fits; a full table (4095 codes) starts again with a clear code.
QByteArray lzw(const QByteArray &pixels,int minimum){
    const int clear=1<<minimum,stop=clear+1;int width=minimum+1,next=stop+1;
    QByteArray out;quint32 bits=0;int count=0;QHash<quint32,int> table;
    auto put=[&](int code){
        bits|=quint32(code)<<count;count+=width;while(count>=8){out.append(char(bits&0xff));bits>>=8;count-=8;}
        if(next>=(1<<width)&&width<12)width++;
    };
    put(clear);if(pixels.isEmpty()){put(stop);if(count>0)out.append(char(bits&0xff));return out;}
    int prefix=quint8(pixels[0]);
    for(qsizetype i=1;i<pixels.size();i++){
        const int c=quint8(pixels[i]);const quint32 key=quint32(prefix)<<8|quint32(c);const auto found=table.constFind(key);
        if(found!=table.constEnd()){prefix=*found;continue;}
        put(prefix);prefix=c;
        if(next>=4095){put(clear);table.clear();width=minimum+1;next=stop+1;}
        else table.insert(key,next++);
    }
    put(prefix);put(stop);if(count>0)out.append(char(bits&0xff));
    return out;
}
}

QByteArray gifData(const QImage &picture){
    if(picture.isNull()||picture.width()>65535||picture.height()>65535)return {};
    // Exact colours where there are no more than 256 of them (Qt finds them for pictures without alpha), else Qt's palette.
    const QImage indexed=picture.convertToFormat(QImage::Format_RGB32).convertToFormat(QImage::Format_Indexed8);
    const auto colours=indexed.colorTable();int depth=1;while((1<<depth)<colours.size())depth++;
    Bytes g;g.raw("GIF89a").u16(quint16(indexed.width())).u16(quint16(indexed.height())).u8(quint8(0x80|(depth-1)<<4|(depth-1))).u8(0).u8(0);
    for(int k=0;k<(1<<depth);k++){const QRgb c=k<colours.size()?colours[k]:0;g.u8(quint8(qRed(c))).u8(quint8(qGreen(c))).u8(quint8(qBlue(c)));}
    // Who made it, as a comment.
    g.u8(0x21).u8(0xfe).u8(8).raw("OpenLoch").u8(0);
    g.u8(0x2c).u16(0).u16(0).u16(quint16(indexed.width())).u16(quint16(indexed.height())).u8(0);
    QByteArray pixels;pixels.reserve(qsizetype(indexed.width())*indexed.height());
    for(int y=0;y<indexed.height();y++)pixels.append(reinterpret_cast<const char*>(indexed.constScanLine(y)),indexed.width());
    const int minimum=std::max(2,depth);const QByteArray data=lzw(pixels,minimum);g.u8(quint8(minimum));
    for(qsizetype at=0;at<data.size();at+=255){const qsizetype n=std::min<qsizetype>(255,data.size()-at);g.u8(quint8(n)).raw(data.mid(at,n));}
    g.u8(0).u8(0x3b);
    return g.b;
}
}
