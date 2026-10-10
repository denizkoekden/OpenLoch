#include "records.h"
#include "inflate.h"
#include "language.h"
#include "legacy_reader.h"
#include <QPolygonF>
#include <QtEndian>
#include <cstring>

namespace openloch::splan {
namespace {
constexpr int maxDepth=32;
constexpr qint64 maxCount=4000000;

// Windows-1252, the code page of the short strings in sPlan 7 files and of font names.
const char16_t cp1252High[32]={0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,
                               0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
QString fromAnsi(const QByteArray &bytes){
    QString s;s.reserve(bytes.size());
    for(char c:bytes){const uchar u=uchar(c);s.append(u>=0x80&&u<0xa0?QChar(cp1252High[u-0x80]):QChar(u));}
    return s;
}
QByteArray toAnsi(const QString &text){
    QByteArray out;out.reserve(text.size());
    for(QChar c:text){
        const char16_t u=c.unicode();
        if(u<0x80||(u>=0xa0&&u<0x100)){out.append(char(u));continue;}
        char replacement='?';for(int i=0;i<32;i++)if(cp1252High[i]==u)replacement=char(0x80+i);
        out.append(replacement);
    }
    return out;
}

class Io {
public:
    bool reading;
    int version;
    const QByteArray *in=nullptr;
    qsizetype pos=0;
    QByteArray out;
    int depth=0;
    Io(const QByteArray &data,int v):reading(true),version(v),in(&data){}
    explicit Io(int v):reading(false),version(v){}

    [[noreturn]] void broken(qsizetype at=-1) const{
        throw FormatError(ui("Die sPlan-Datei ist beschädigt (bei Byte %1)").arg(at<0?pos:at));
    }
    const char *take(qsizetype n){
        if(n<0||pos+n>in->size())broken();
        const char *p=in->constData()+pos;pos+=n;return p;
    }
    void put(const void *p,qsizetype n){out.append(static_cast<const char*>(p),n);}
    // Raw values
    qint32 readI32(){qint32 v;std::memcpy(&v,take(4),4);return qFromLittleEndian(v);}
    void writeI32(qint32 v){v=qToLittleEndian(v);put(&v,4);}
    float readF32(){quint32 u=qFromLittleEndian<quint32>(take(4));float f;std::memcpy(&f,&u,4);return f;}
    void writeF32(float f){quint32 u;std::memcpy(&u,&f,4);u=qToLittleEndian(u);put(&u,4);}

    void u8(Record &r,const char *name){
        if(reading)r.set(name,int(uchar(*take(1))));else{const char c=char(r.integer(name));put(&c,1);}
    }
    void i32(Record &r,const char *name){if(reading)r.set(name,readI32());else writeI32(qint32(r.integer(name)));}
    void u32(Record &r,const char *name){if(reading)r.set(name,uint(readI32()));else writeI32(qint32(quint32(r.integer(name))));}
    void f32(Record &r,const char *name){
        if(reading)r.set(name,QVariant::fromValue(readF32()));else writeF32(r.fields.value(name).toFloat());
    }
    void raw(Record &r,const char *name,int n){
        if(reading)r.set(name,QByteArray(take(n),n));
        else{QByteArray b=r.raw(name);b.resize(n,'\0');out.append(b);}
    }
    void point(Record &r,const char *name){
        if(reading){const float x=readF32(),y=readF32();r.set(name,QPointF(x,y));}
        else{const QPointF p=r.fields.value(name).toPointF();writeF32(float(p.x()));writeF32(float(p.y()));}
    }
    void pointArray(Record &r,const char *name,int n){
        if(reading){QPolygonF p;for(int i=0;i<n;i++){const float x=readF32(),y=readF32();p<<QPointF(x,y);}r.set(name,QVariant::fromValue(p));}
        else{QPolygonF p=r.fields.value(name).value<QPolygonF>();p.resize(n);for(auto q:p){writeF32(float(q.x()));writeF32(float(q.y()));}}
    }
    // A list of points led by its count less one.
    void points(Record &r,const char *name){
        if(reading){
            const qint64 n=qint64(readI32())+1;if(n<0||n>maxCount)broken(pos-4);
            QPolygonF p;p.reserve(n);for(qint64 i=0;i<n;i++){const float x=readF32(),y=readF32();p<<QPointF(x,y);}
            r.set(name,QVariant::fromValue(p));
        }else{
            const QPolygonF p=r.fields.value(name).value<QPolygonF>();writeI32(qint32(p.size())-1);
            for(auto q:p){writeF32(float(q.x()));writeF32(float(q.y()));}
        }
    }
    // A UTF-16 string led by its length in characters (sPlan 8).
    void wstr(Record &r,const char *name){
        if(reading){
            const qint64 n=readI32();if(n<0||n>16*1024*1024)broken(pos-4);
            const char *p=take(n*2);QString s(n,Qt::Uninitialized);
            for(qint64 i=0;i<n;i++)s[i]=QChar(char16_t(uchar(p[2*i])|(uchar(p[2*i+1])<<8)));
            r.set(name,s);
        }else{
            const QString s=r.string(name);writeI32(qint32(s.size()));
            for(QChar c:s){const char16_t u=c.unicode();const char b[2]={char(u&0xff),char(u>>8)};put(b,2);}
        }
    }
    // A short string led by its length as a 32-bit number and again as a byte (sPlan 7, font names). The bytes are
    // kept, so that a string that is not changed is written as read.
    void sstr(Record &r,const char *name){
        const QString rawName=QString::fromLatin1(name)+QStringLiteral("#raw");
        if(reading){
            const qint64 n=readI32();if(n<0||n>255)broken(pos-4);
            const QByteArray b(take(n+1),n+1);
            r.set(name,fromAnsi(b.mid(1,uchar(b[0]))));r.set(rawName,b);
        }else{
            QByteArray b=r.raw(rawName);
            if(b.isEmpty()||fromAnsi(b.mid(1,uchar(b[0])))!=r.string(name)){b=toAnsi(r.string(name)).left(255);b.prepend(char(b.size()));}
            writeI32(qint32(b.size())-1);out.append(b);
        }
    }
    void vstr(Record &r,const char *name){if(version>=80)wstr(r,name);else sstr(r,name);}
    // A string led by its length, without a length byte (sPlan 7 sheet description).
    void ansi(Record &r,const char *name){
        if(reading){const qint64 n=readI32();if(n<0||n>16*1024*1024)broken(pos-4);r.set(name,fromAnsi(QByteArray(take(n),n)));}
        else{const QByteArray b=toAnsi(r.string(name));writeI32(qint32(b.size()));out.append(b);}
    }
    // A short string in a buffer of fixed size: its bytes after the text are kept (sPlan 7 sheet names).
    void fixedString(Record &r,const char *name,int size){
        const QString rawName=QString::fromLatin1(name)+QStringLiteral("#raw");
        if(reading){const QByteArray b(take(size),size);r.set(rawName,b);r.set(name,fromAnsi(b.mid(1,std::min<int>(uchar(b[0]),size-1))));}
        else{
            QByteArray b=r.raw(rawName);b.resize(size,'\0');
            if(fromAnsi(b.mid(1,std::min<int>(uchar(b[0]),size-1)))!=r.string(name)){
                const QByteArray t=toAnsi(r.string(name)).left(size-1);b[0]=char(t.size());b.replace(1,t.size(),t);
            }
            out.append(b);
        }
    }
    // A zlib stream: kept as read; `name#data` holds what it unpacks to.
    void zlib(Record &r,const char *name){
        const QString dataName=QString::fromLatin1(name)+QStringLiteral("#data");
        if(reading){
            qsizetype used=0;const QByteArray data=inflateZlib(*in,pos,&used);
            r.set(name,in->mid(pos,used));r.set(dataName,data);pos+=used;
        }else{
            QByteArray packed=r.raw(name);
            qsizetype used=0;bool same=false;
            try{same=!packed.isEmpty()&&inflateZlib(packed,0,&used)==r.raw(dataName)&&used==packed.size();}catch(const FormatError &){same=false;}
            if(!same)packed=deflateZlib(r.raw(dataName));
            out.append(packed);
        }
    }
};

void object(Io &io,Record &r);
void body(Io &io,Record &r,int type);

// The 23 bytes every drawn object starts with: outline and fill switches, outline width (1/10 mm), outline colour,
// line style (lines) or size (junctions), fill colour, fill style and the title block mark.
void header(Io &io,Record &r){
    io.u8(r,"noOutline");io.u8(r,"noFill");io.i32(r,"width");io.u32(r,"colour");io.i32(r,"style");io.u32(r,"fillColour");io.i32(r,"fillStyle");
    io.u8(r,"titleBlock");
}
// Two-colour lines and stripes along and across lines and outlines (sPlan 8).
void stripes(Io &io,Record &r){io.u8(r,"inner");io.u8(r,"cross");io.u32(r,"innerColour");io.u32(r,"crossColour");}
void hatch(Io &io,Record &r){io.i32(r,"hatchWidth");io.i32(r,"hatchSpacing");}

bool displayString(const Record &r,bool designator){return designator||hasDisplayString(r.string("text"));}
void textBody(Io &io,Record &r,bool designator){
    header(io,r);io.pointArray(r,"corners",4);io.vstr(r,"text");
    io.sstr(r,"font");io.u8(r,"fontStyle");io.u32(r,"textColour");io.i32(r,"size");io.i32(r,"align");io.u8(r,"afterAlign");
    if(io.version<80)io.sstr(r,"string7");
    io.vstr(r,"link");
    if(io.version>=80){io.raw(r,"guid",16);io.raw(r,"target",16);io.u8(r,"background");io.u32(r,"backgroundColour");io.u8(r,"afterBackground");}
    if(displayString(r,designator))io.vstr(r,"display");
}
// A sub-record without type byte.
void part(Io &io,Record &r,const char *list,int type,const std::function<void(Record&)> &layout){
    if(io.reading){
        Record p;p.type=type;p.offset=io.pos;layout(p);p.bytes=io.in->mid(p.offset,io.pos-p.offset);r.lists[list]={p};
    }else{
        if(r.lists.value(list).isEmpty())io.broken(0);
        layout(r.lists[list][0]);
    }
}
void children(Io &io,Record &r){
    if(++io.depth>maxDepth)io.broken();
    if(io.reading){
        const qint64 n=qint64(io.readI32())+1;if(n<0||n>maxCount)io.broken(io.pos-4);
        QList<Record> list;list.reserve(n);
        for(qint64 i=0;i<n;i++){Record c;object(io,c);list.append(c);}
        r.lists["children"]=list;
    }else{
        const auto list=r.lists.value("children");io.writeI32(qint32(list.size())-1);
        for(auto c:list)object(io,c);
    }
    io.depth--;
}
void body(Io &io,Record &r,int type){
    switch(type){
    case Ellipse:
        header(io,r);io.points(r,"points");io.point(r,"centre");io.f32(r,"start");io.f32(r,"stop");
        io.u8(r,"twoColour");io.u32(r,"colour2");io.i32(r,"dash");
        if(io.version>=80){io.i32(r,"arc");stripes(io,r);hatch(io,r);}
        break;
    case Rectangle:
        header(io,r);io.pointArray(r,"corners",4);io.point(r,"rounding");
        io.u8(r,"twoColour");io.u32(r,"colour2");io.i32(r,"dash");
        if(io.version>=80){io.i32(r,"cornerStyle");stripes(io,r);hatch(io,r);}
        break;
    case Text:textBody(io,r,false);break;
    case Contact:textBody(io,r,false);io.vstr(r,"name");break;
    case Line:case Bezier:
        header(io,r);io.points(r,"points");io.u32(r,"colour2");io.u8(r,"twoColour");io.u8(r,"startEnd");io.u8(r,"endEnd");io.i32(r,"endSize");
        if(io.version>=80)stripes(io,r);
        break;
    case Polygon:
        header(io,r);io.points(r,"points");io.u8(r,"twoColour");io.u32(r,"colour2");io.i32(r,"dash");
        if(io.version>=80){stripes(io,r);hatch(io,r);}
        break;
    case Group:children(io,r);break;
    case Component:
        part(io,r,"group",Group,[&](Record &p){children(io,p);});
        part(io,r,"designator",Text,[&](Record &p){textBody(io,p,true);});
        part(io,r,"value",Text,[&](Record &p){textBody(io,p,false);});
        io.raw(r,"flags",4);io.vstr(r,"caption");io.vstr(r,"prefix");io.i32(r,"number");io.point(r,"insertion");io.i32(r,"afterInsertion");io.u8(r,"afterInsertionFlag");
        io.vstr(r,"extra1");
        if(io.version>=80){io.vstr(r,"extra2");io.vstr(r,"extra3");io.vstr(r,"extra4");}
        io.u8(r,"parent");io.i32(r,"key");io.i32(r,"parentKey");
        if(io.version>=80)io.u8(r,"lastByte");
        break;
    case Junction:header(io,r);io.point(r,"pos");break;
    case Picture:header(io,r);io.pointArray(r,"corners",4);io.zlib(r,"image");io.i32(r,"imageKey");break;
    case TextBox:{
        header(io,r);io.pointArray(r,"corners",4);io.u8(r,"beforeFont");io.sstr(r,"font");io.u8(r,"fontStyle");io.u32(r,"textColour");io.i32(r,"size");
        if(io.reading){
            const qint64 n=io.readI32();if(n<0||n>maxCount)io.broken(io.pos-4);
            QStringList lines;for(qint64 i=0;i<n;i++){Record l;io.vstr(l,"line");lines<<l.string("line");}
            r.set("lines",lines);
        }else{
            const QStringList lines=r.fields.value("lines").toStringList();io.writeI32(qint32(lines.size()));
            for(const auto &line:lines){Record l;l.set("line",line);io.vstr(l,"line");}
        }
        if(io.version>=80){io.u8(r,"afterLines1");io.i32(r,"afterLines2");io.u8(r,"afterLines3");io.u8(r,"afterLines4");}
        break;
    }
    case Dimension:
        header(io,r);
        part(io,r,"text1",Text,[&](Record &p){textBody(io,p,false);});
        part(io,r,"text2",Text,[&](Record &p){textBody(io,p,false);});
        part(io,r,"text3",Text,[&](Record &p){textBody(io,p,false);});
        io.u8(r,"beforePoints");io.pointArray(r,"points",3);io.point(r,"point4");io.raw(r,"offset",8);io.u8(r,"flag1");io.u8(r,"flag2");
        io.vstr(r,"s1");io.vstr(r,"s2");io.vstr(r,"s3");io.i32(r,"value1");io.i32(r,"value2");io.i32(r,"value3");io.i32(r,"value4");io.u8(r,"flag3");io.i32(r,"value5");
        break;
    default:io.broken(io.pos-1);
    }
}
void object(Io &io,Record &r){
    if(io.reading){
        r.offset=io.pos;r.type=uchar(*io.take(1));
        body(io,r,r.type);
        r.bytes=io.in->mid(r.offset,io.pos-r.offset);
    }else{const char t=char(r.type);io.put(&t,1);body(io,r,r.type);}
}

void sheetFrame(Io &io,Record &r,bool withObjects){
    if(io.version>=80)io.wstr(r,"name");else io.fixedString(r,"name",31);
    io.i32(r,"height");io.i32(r,"width");
    if(io.version<80)io.raw(r,"b2",2);
    io.i32(r,"grid");io.i32(r,"scrollX");io.i32(r,"scrollY");io.f32(r,"zoom");
    // Without its objects, a sheet keeps in their place which of them belonged to the title block, one byte each
    // (empty when the title block's objects come first).
    if(withObjects)children(io,r);
    else if(io.reading){const qint64 n=qint64(io.readI32())+1;if(n<0||n>maxCount)io.broken(io.pos-4);r.set("order",QByteArray(io.take(n),n));}
    else{const QByteArray order=r.raw("order");io.writeI32(qint32(order.size())-1);io.out.append(order);}
    // Magnetic guide lines: the y of the horizontal ones, then the x of the vertical ones, as floats.
    for(const char *list:{"guidesY","guidesX"}){
        if(io.reading){
            const qint64 n=qint64(io.readI32())+1;if(n<0||n>maxCount)io.broken(io.pos-4);
            r.set(list,QByteArray(io.take(n*4),n*4));
        }else{const QByteArray b=r.raw(list);io.writeI32(qint32(b.size()/4)-1);io.out.append(b.left(b.size()/4*4));}
    }
    io.raw(r,"reserved8",8);io.raw(r,"scale",8);io.i32(r,"scaleUnit");
    if(io.version>=80)io.wstr(r,"description");else io.ansi(r,"description");
    if(io.version>=80){
        io.raw(r,"reserved36",36);io.u8(r,"frameFlag");
        io.f32(r,"frameWidth");io.f32(r,"frameHeight");io.f32(r,"frameX");io.f32(r,"frameY");
        io.i32(r,"columns");io.i32(r,"rows");io.i32(r,"columnStart");io.i32(r,"rowStart");io.u8(r,"showGrid");
    }
}
void fileFrame(Io &io,Record &r,bool withSheets){
    io.raw(r,"head",11);
    if(io.reading){
        const qint64 n=io.readI32();if(n<0||n>30000)io.broken(io.pos-4);
        QStringList vars;for(qint64 i=0;i<n;i++){Record v;io.vstr(v,"v");vars<<v.string("v");}
        r.set("variables",vars);
    }else{
        const QStringList vars=r.fields.value("variables").toStringList();io.writeI32(qint32(vars.size()));
        for(const auto &v:vars){Record x;x.set("v",v);io.vstr(x,"v");}
    }
    io.u8(r,"b1");io.u8(r,"b2");
    if(io.version>=80){io.wstr(r,"s1");io.wstr(r,"s2");}
    io.i32(r,"active");
    if(io.reading){
        const qint64 n=qint64(io.readI32())+1;if(n<0||n>10000)io.broken(io.pos-4);
        if(withSheets){
            QList<Record> sheets;
            for(qint64 i=0;i<n;i++){Record s;s.offset=io.pos;sheetFrame(io,s,true);s.bytes=io.in->mid(s.offset,io.pos-s.offset);sheets<<s;}
            r.lists["sheets"]=sheets;
        }else if(n!=0)io.broken(io.pos-4);
    }else{
        const auto sheets=withSheets?r.lists.value("sheets"):QList<Record>();io.writeI32(qint32(sheets.size())-1);
        for(auto s:sheets)sheetFrame(io,s,true);
    }
}
// A page of a library: the file's head, two numbers, then the head of a sheet with its objects (no more).
void libraryFrame(Io &io,Record &r){
    io.raw(r,"head",11);io.i32(r,"first");io.i32(r,"second");
    if(io.version>=80)io.wstr(r,"name");else io.fixedString(r,"name",31);
    io.i32(r,"height");io.i32(r,"width");
    if(io.version<80)io.raw(r,"afterSize",2);
    io.i32(r,"grid");io.i32(r,"scrollX");io.i32(r,"scrollY");io.f32(r,"zoom");
    children(io,r);
}
// A title block: its version as a negative number, a number, then its objects.
void formFrame(Io &io,Record &r){
    if(io.reading)io.readI32();else io.writeI32(-io.version);
    io.i32(r,"second");children(io,r);
}
int headVersion(const QByteArray &head){
    if(head.size()<8||head[0]!=7||!head.startsWith(QByteArray("\x07SPLAN",6)))return 0;
    const char a=head[6],b=head[7];
    if(a<'0'||a>'9'||b<'0'||b>'9')return 0;
    return (a-'0')*10+(b-'0');
}
}

QPointF Record::point(const QString &name) const{return fields.value(name).toPointF();}
QList<QPointF> Record::points(const QString &name) const{const auto p=fields.value(name).value<QPolygonF>();return QList<QPointF>(p.begin(),p.end());}
void Record::setPoints(const QString &name,const QList<QPointF> &points){
    QPolygonF p;for(auto q:points)p<<QPointF(double(float(q.x())),double(float(q.y())));fields.insert(name,QVariant::fromValue(p));
}
void Record::setPoint(const QString &name,QPointF point){fields.insert(name,QPointF(double(float(point.x())),double(float(point.y()))));}

int fileVersion(const QByteArray &bytes){return headVersion(bytes.left(8));}
bool supportedVersion(int version){return version==70||version==80;}
bool hasDisplayString(const QString &text){return text.contains(u'<')&&text.contains(u'>');}

Record readFile(const QByteArray &bytes){
    const int version=fileVersion(bytes);
    if(!version)throw FormatError(ui("Keine sPlan-Datei"));
    if(!supportedVersion(version))throw FormatError(ui("sPlan-Dateien der Version %1 werden nicht unterstützt").arg(version/10.,0,'f',1));
    Io io(bytes,version);Record r;fileFrame(io,r,true);
    if(io.pos!=bytes.size())io.broken();
    r.bytes=bytes;
    return r;
}
Record readLibraryFile(const QByteArray &bytes){
    const int version=fileVersion(bytes);
    if(!version)throw FormatError(ui("Keine sPlan-Bibliothek"));
    if(!supportedVersion(version))throw FormatError(ui("sPlan-Dateien der Version %1 werden nicht unterstützt").arg(version/10.,0,'f',1));
    Io io(bytes,version);Record r;libraryFrame(io,r);
    if(io.pos!=bytes.size())io.broken();
    r.bytes=bytes;return r;
}
QByteArray writeLibraryFile(const Record &page,int version){
    if(!supportedVersion(version))throw FormatError(ui("sPlan-Dateien der Version %1 werden nicht unterstützt").arg(version/10.,0,'f',1));
    Io io(version);Record r=page;libraryFrame(io,r);return io.out;
}
int formVersion(const QByteArray &bytes){return bytes.size()<4?0:-qFromLittleEndian<qint32>(bytes.constData());}
Record readFormFile(const QByteArray &bytes){
    const int version=formVersion(bytes);
    if(version<=0||version>99)throw FormatError(ui("Kein sPlan-Formblatt"));
    if(!supportedVersion(version))throw FormatError(ui("sPlan-Dateien der Version %1 werden nicht unterstützt").arg(version/10.,0,'f',1));
    Io io(bytes,version);Record r;formFrame(io,r);
    if(io.pos!=bytes.size())io.broken();
    r.bytes=bytes;return r;
}
QByteArray writeFormFile(const Record &form,int version){
    if(!supportedVersion(version))throw FormatError(ui("sPlan-Dateien der Version %1 werden nicht unterstützt").arg(version/10.,0,'f',1));
    Io io(version);Record r=form;formFrame(io,r);return io.out;
}
QByteArray writeFile(const Record &file){
    const int version=headVersion(file.raw("head"));
    if(!supportedVersion(version))throw FormatError(ui("Keine sPlan-Datei"));
    Io io(version);Record r=file;fileFrame(io,r,true);return io.out;
}
QByteArray writeObject(const Record &object,int version){Io io(version);Record r=object;splan::object(io,r);return io.out;}
Record readObject(const QByteArray &bytes,int version){
    Io io(bytes,version);Record r;object(io,r);if(io.pos!=bytes.size())io.broken();return r;
}
QByteArray writeSheetFrame(const Record &sheet,int version){Io io(version);Record r=sheet;sheetFrame(io,r,false);return io.out;}
Record readSheetFrame(const QByteArray &bytes,int version){
    Io io(bytes,version);Record r;sheetFrame(io,r,false);if(io.pos!=bytes.size())io.broken();r.bytes=bytes;return r;
}
QByteArray writeFileFrame(const Record &file){
    const int version=headVersion(file.raw("head"));Io io(version);Record r=file;fileFrame(io,r,false);return io.out;
}
Record readFileFrame(const QByteArray &bytes){
    const int version=fileVersion(bytes);if(!supportedVersion(version))throw FormatError(ui("Keine sPlan-Datei"));
    Io io(bytes,version);Record r;fileFrame(io,r,false);if(io.pos!=bytes.size())io.broken();r.bytes=bytes;return r;
}
}
