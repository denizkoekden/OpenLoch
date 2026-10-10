#include "images.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QImage>
#include <QSet>
#include <QTransform>
#include <algorithm>
#include <functional>

namespace openloch::schematic {
namespace {
QImage picture(const Document &d,const Item &i){return QImage::fromData(d.resources.value(i.resource).data);}
// Stores a changed picture in the kind it had and lets the element show it.
bool store(Document &d,Item &i,const QImage &image){
    if(image.isNull())return false;
    QString kind=d.resources.value(i.resource).kind;if(kind!=u"jpg"&&kind!=u"bmp")kind=QStringLiteral("png");
    QByteArray data;QBuffer buffer(&data);buffer.open(QIODevice::WriteOnly);
    if(!image.save(&buffer,kind==u"jpg"?"JPG":kind==u"bmp"?"BMP":"PNG",kind==u"jpg"?92:-1))return false;
    const QString key=QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex());
    d.resources.insert(key,{kind,data});i.resource=key;
    return true;
}
void eachImage(QList<Item> &items,const std::function<void(Item&)> &f){
    for(auto &i:items){if(i.type==ItemType::Image)f(i);eachImage(i.children,f);}
}
void eachImage(const QList<Item> &items,const std::function<void(const Item&)> &f){
    for(const auto &i:items){if(i.type==ItemType::Image)f(i);eachImage(i.children,f);}
}
}
int bitsPerPixel(const QByteArray &b){
    auto u8=[&](qsizetype at){return at<b.size()?int(uchar(b[at])):0;};
    // PNG: bit depth times the channels of the colour type in IHDR.
    if(b.startsWith("\x89PNG\r\n\x1a\n")&&b.size()>26){
        const int depth=u8(24),type=u8(25);const int channels=type==2?3:type==4?2:type==6?4:1;return depth*channels;
    }
    // BMP: biBitCount of the info header.
    if(b.startsWith("BM")&&b.size()>30)return u8(28)|u8(29)<<8;
    // JPEG: precision times the components of the first frame header.
    if(b.startsWith("\xff\xd8")){
        for(qsizetype at=2;at+9<b.size();){
            if(u8(at)!=0xff){at++;continue;}
            const int marker=u8(at+1);if(marker==0xff){at++;continue;}
            if(marker>=0xc0&&marker<=0xcf&&marker!=0xc4&&marker!=0xc8&&marker!=0xcc)return u8(at+4)*u8(at+9);
            at+=2+(u8(at+2)<<8|u8(at+3));
        }
    }
    return 0;
}
ImageInfo imageInfo(const Document &d,const Item &i){
    ImageInfo info;const QImage image=picture(d,i);
    info.pixels=image.size();info.bytes=d.resources.value(i.resource).data.size();info.bits=bitsPerPixel(d.resources.value(i.resource).data);
    if(i.size.width()>0)info.dpi=image.width()/(i.size.width()/25.4);
    return info;
}
bool reduceResolution(Document &d,Item &i,double factor){
    const QImage image=picture(d,i);if(image.isNull()||factor<=1)return false;
    const QSize to(std::max(1,int(std::lround(image.width()/factor))),std::max(1,int(std::lround(image.height()/factor))));
    if(to==image.size())return false;
    return store(d,i,image.scaled(to,Qt::IgnoreAspectRatio,Qt::SmoothTransformation));
}
bool rotateImage(Document &d,Item &i){
    const QImage image=picture(d,i);if(image.isNull())return false;
    if(!store(d,i,image.transformed(QTransform().rotate(90))))return false;
    i.size=i.size.transposed();return true;
}
bool lightenImage(Document &d,Item &i){
    QImage image=picture(d,i).convertToFormat(QImage::Format_ARGB32);if(image.isNull())return false;
    // A quarter of the way to white.
    for(int y=0;y<image.height();y++){
        auto *line=reinterpret_cast<QRgb*>(image.scanLine(y));
        for(int x=0;x<image.width();x++){const QRgb c=line[x];auto up=[](int v){return v+(255-v)/4;};line[x]=qRgba(up(qRed(c)),up(qGreen(c)),up(qBlue(c)),qAlpha(c));}
    }
    return store(d,i,image);
}
bool normaliseImage(const Document &d,Item &i){
    const QImage image=picture(d,i);if(image.isNull()||image.width()==0)return false;
    i.size.setHeight(i.size.width()*image.height()/image.width());return true;
}
QList<PlacedImage> placedImages(const Document &d){
    QList<PlacedImage> out;
    for(int s=0;s<d.sheets.size();s++){
        auto add=[&](const Item &i){out.append({s,i.id});};
        eachImage(d.sheets[s].items,add);eachImage(d.sheets[s].titleBlock.items,add);
    }
    return out;
}
Item *imageWithId(Document &d,int sheet,const QString &id){
    if(sheet<0||sheet>=d.sheets.size())return nullptr;
    Item *found=nullptr;auto look=[&](Item &i){if(i.id==id)found=&i;};
    eachImage(d.sheets[sheet].items,look);if(!found)eachImage(d.sheets[sheet].titleBlock.items,look);
    return found;
}
const Item *imageWithId(const Document &d,int sheet,const QString &id){
    // Read only: the lists are not detached from undo steps sharing them.
    if(sheet<0||sheet>=d.sheets.size())return nullptr;
    const Item *found=nullptr;auto look=[&](const Item &i){if(i.id==id)found=&i;};
    eachImage(d.sheets[sheet].items,look);if(!found)eachImage(d.sheets[sheet].titleBlock.items,look);
    return found;
}
int dropUnusedResources(Document &d){
    QSet<QString> used;
    for(const auto &s:d.sheets){auto use=[&](const Item &i){used.insert(i.resource);};eachImage(s.items,use);eachImage(s.titleBlock.items,use);}
    int dropped=0;
    for(auto it=d.resources.begin();it!=d.resources.end();){if(used.contains(it.key()))++it;else{it=d.resources.erase(it);dropped++;}}
    return dropped;
}
}
