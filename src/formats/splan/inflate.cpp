#include "inflate.h"
#include "language.h"
#include "legacy_reader.h"
#include <QtEndian>
#include <array>

namespace openloch::splan {
namespace {
[[noreturn]] void broken(){throw FormatError(ui("Ein Bild der sPlan-Datei ist beschädigt"));}

// Reads bits from the least significant end, as deflate stores them.
struct Bits {
    const uchar *data;qsizetype size,pos=0;quint32 buffer=0;int count=0;
    int bit(){
        if(!count){if(pos>=size)broken();buffer=data[pos++];count=8;}
        const int b=buffer&1;buffer>>=1;count--;return b;
    }
    int bits(int n){int v=0;for(int i=0;i<n;i++)v|=bit()<<i;return v;}
    void align(){count=0;buffer=0;}
};
// A canonical Huffman code as counts of codes per length and the symbols in code order.
struct Huffman {
    std::array<short,16> counts{};
    std::array<short,320> symbols{};
    void build(const short *lengths,int n){
        counts.fill(0);for(int i=0;i<n;i++)counts[lengths[i]]++;
        counts[0]=0;
        std::array<short,16> offsets{};for(int len=1;len<16;len++)offsets[len]=short(offsets[len-1]+counts[len-1]);
        // Over-subscribed codes are refused; incomplete ones only matter when a missing code is met.
        int left=1;for(int len=1;len<16;len++){left<<=1;left-=counts[len];if(left<0)broken();}
        for(int i=0;i<n;i++)if(lengths[i])symbols[offsets[lengths[i]]++]=short(i);
    }
    int decode(Bits &in) const{
        int code=0,first=0,index=0;
        for(int len=1;len<16;len++){
            code|=in.bit();const int count=counts[len];
            if(code-count<first)return symbols[index+(code-first)];
            index+=count;first+=count;first<<=1;code<<=1;
        }
        broken();
    }
};
const short lengthBase[]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
const short lengthExtra[]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
const short distanceBase[]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
const short distanceExtra[]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

void codes(Bits &in,QByteArray &out,const Huffman &lengths,const Huffman &distances,qsizetype limit){
    for(;;){
        int symbol=lengths.decode(in);
        if(symbol<256){if(out.size()>=limit)broken();out.append(char(symbol));continue;}
        if(symbol==256)return;
        symbol-=257;if(symbol>=29)broken();
        const int length=lengthBase[symbol]+in.bits(lengthExtra[symbol]);
        const int d=distances.decode(in);if(d>=30)broken();
        const int distance=distanceBase[d]+in.bits(distanceExtra[d]);
        if(distance>out.size()||out.size()+length>limit)broken();
        const qsizetype from=out.size()-distance;
        for(int i=0;i<length;i++)out.append(out.at(from+i));
    }
}
}

namespace {
// The deflate blocks from the start of `in` to the last one.
QByteArray blocks(Bits &in,qsizetype limit){
    QByteArray out;
    for(bool last=false;!last;){
        last=in.bit();const int type=in.bits(2);
        if(type==0){
            in.align();if(in.pos+4>in.size)broken();
            const int length=in.data[in.pos]|(in.data[in.pos+1]<<8),check=in.data[in.pos+2]|(in.data[in.pos+3]<<8);
            in.pos+=4;if(length!=(~check&0xffff)||in.pos+length>in.size||out.size()+length>limit)broken();
            out.append(reinterpret_cast<const char*>(in.data+in.pos),length);in.pos+=length;
        }else if(type==1){
            static const auto fixed=[]{
                std::pair<Huffman,Huffman> h;short l[288];
                for(int i=0;i<144;i++)l[i]=8;for(int i=144;i<256;i++)l[i]=9;for(int i=256;i<280;i++)l[i]=7;for(int i=280;i<288;i++)l[i]=8;
                h.first.build(l,288);short d[30];for(auto &x:d)x=5;h.second.build(d,30);return h;}();
            codes(in,out,fixed.first,fixed.second,limit);
        }else if(type==2){
            const int nlen=in.bits(5)+257,ndist=in.bits(5)+1,ncode=in.bits(4)+4;
            if(nlen>286||ndist>30)broken();
            static const int order[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            short lengths[320]={};for(int i=0;i<ncode;i++)lengths[order[i]]=short(in.bits(3));
            Huffman lencode;lencode.build(lengths,19);
            short all[320]={};
            for(int i=0;i<nlen+ndist;){
                const int symbol=lencode.decode(in);
                if(symbol<16){all[i++]=short(symbol);continue;}
                int repeat=0;short value=0;
                if(symbol==16){if(!i)broken();value=all[i-1];repeat=3+in.bits(2);}
                else if(symbol==17)repeat=3+in.bits(3);
                else repeat=11+in.bits(7);
                if(i+repeat>nlen+ndist)broken();
                while(repeat--)all[i++]=value;
            }
            if(!all[256])broken();
            Huffman lengthCode,distanceCode;lengthCode.build(all,nlen);distanceCode.build(all+nlen,ndist);
            codes(in,out,lengthCode,distanceCode,limit);
        }else broken();
    }
    in.align();
    return out;
}
}
QByteArray inflateZlib(const QByteArray &data,qsizetype offset,qsizetype *consumed,qsizetype limit){
    if(offset<0||offset+2>data.size())broken();
    const auto *bytes=reinterpret_cast<const uchar*>(data.constData());
    const int cmf=bytes[offset],flg=bytes[offset+1];
    if((cmf&15)!=8||(cmf>>4)>7||((cmf<<8)|flg)%31||(flg&0x20))broken();
    Bits in{bytes+offset+2,data.size()-offset-2};
    const QByteArray out=blocks(in,limit);
    if(in.pos+4>in.size)broken();
    // Adler-32 of the unpacked data.
    quint32 a=1,b=0;for(char c:out){a=(a+uchar(c))%65521;b=(b+a)%65521;}
    if(qFromBigEndian<quint32>(in.data+in.pos)!=((b<<16)|a))broken();
    if(consumed)*consumed=2+in.pos+4;
    return out;
}
QByteArray inflateRaw(const QByteArray &data,qsizetype offset,qsizetype size,qsizetype limit){
    if(offset<0||size<0||offset+size>data.size())broken();
    Bits in{reinterpret_cast<const uchar*>(data.constData())+offset,size};
    return blocks(in,limit);
}
QByteArray deflateZlib(const QByteArray &data){return qCompress(data,9).mid(4);}
}
