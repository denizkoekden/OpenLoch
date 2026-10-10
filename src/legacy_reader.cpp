#include "legacy_reader.h"
#include "language.h"
#include <QJsonArray>
#include <QStringDecoder>
#include <QtEndian>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <QtNumeric>

namespace openloch {
QString legacyTitle(const QJsonObject &document){
    auto full=document["description"].toString();return full.isEmpty()?document["title"].toString():full;
}
LegacyReader::LegacyReader(QByteArray b) : bytes(std::move(b)) {
    if (bytes.size() > 128*1024*1024) fail(ui("Datei ist zu groß"));
}
void LegacyReader::fail(const QString &why) const {
    throw FormatError(ui("%1 (Position 0x%2)").arg(why).arg(pos,0,16));
}
QByteArray LegacyReader::raw(qsizetype n) {
    if (n<0 || n>bytes.size()-pos) fail(ui("Datei ist unvollständig"));
    auto r=bytes.mid(pos,n);pos+=n;return r;
}
quint8 LegacyReader::byte() { return quint8(raw(1)[0]); }
quint32 LegacyReader::u32() { auto r=raw(4); return qFromLittleEndian<quint32>(r.constData()); }
qint32 LegacyReader::i32() { return qint32(u32()); }
int LegacyReader::integer() {
    switch(byte()) {
    case 2:return qint8(byte());
    case 3:{auto r=raw(2);return qFromLittleEndian<qint16>(r.constData());}
    case 4:return i32();
    default:fail(ui("Ungültiger Integer-Typ"));
    }
}
bool LegacyReader::boolean() {
    auto t=byte();if(t!=8&&t!=9)fail(ui("Ungültiger Boolean-Typ"));return t==9;
}
double LegacyReader::number() {
    auto t=byte();double value=0;
    if(t==5) {
        auto r=raw(10);auto m=qFromLittleEndian<quint64>(r.constData());
        auto e=qFromLittleEndian<quint16>(r.constData()+8);int exponent=e&0x7fff;
        if(exponent==0x7fff)fail(ui("Ungültige Gleitkommazahl"));
        // Decode x87 80-bit values explicitly; Apple Silicon long double is 64-bit.
        value=m?std::ldexp(double(m)/9223372036854775808.0,exponent?exponent-16383:-16382):0;
        if(e&0x8000)value=-value;
    } else if(t==2)value=qint8(byte());
    else if(t==3){auto r=raw(2);value=qFromLittleEndian<qint16>(r.constData());}
    else if(t==4)value=i32();
    else if(t==15){auto bits=u32();float f;std::memcpy(&f,&bits,4);value=f;}
    else if(t==21){auto r=raw(8);auto bits=qFromLittleEndian<quint64>(r.constData());std::memcpy(&value,&bits,8);}
    else fail(ui("Ungültiger Zahlentyp"));
    if(!std::isfinite(value))fail(ui("Gleitkommazahl außerhalb des Wertebereichs"));
    return value;
}
static QString windowsText(const QByteArray &b) {
    // Qt's built-in Windows-1252 decoder is available without Qt5Compat.
    QStringDecoder decoder("Windows-1252");
    if(decoder.isValid())return decoder.decode(b);
    static const char16_t cp[32]={0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
    QString result;for(unsigned char c:b)result+=QChar(c>=128&&c<160?cp[c-128]:char16_t(c));return result;
}
QString LegacyReader::string() {
    auto t=byte();
    if(t==6)return windowsText(raw(byte()));
    if(t==12)return windowsText(raw(i32()));
    if(t==20)return QString::fromUtf8(raw(i32()));
    if(t==18){qint64 n=i32();if(n<0||n>10000000)fail(ui("Ungültige Textlänge"));auto r=raw(n*2);QStringDecoder decoder(QStringDecoder::Utf16LE);return decoder.decode(r);}
    fail(ui("Ungültiger Texttyp"));
}
QJsonArray LegacyReader::integers(int n){QJsonArray a;while(n--)a.append(integer());return a;}
QJsonArray LegacyReader::booleans(int n){QJsonArray a;while(n--)a.append(boolean());return a;}
QJsonArray LegacyReader::points() {
    qint64 n=qint64(integer())+1;
    if(n<0||n>100000)fail(ui("Ungültige Punktanzahl"));
    QJsonArray a;while(n--){int x=i32(),y=i32();a.append(QJsonArray{x,y});}return a;
}
namespace {
// The layout of a bitmap as its info header gives it: where its bits start (file header, info header, colour masks and
// colour table) and how many bytes they take; nullopt when the header is none of the known ones or the sizes overflow.
struct DibLayout {qint64 offset,bits;};
std::optional<DibLayout> dibLayout(const QByteArray &b,qint64 at){
    if(at<0||at+14+16>b.size())return std::nullopt;
    const char *d=b.constData()+at;const quint32 header=qFromLittleEndian<quint32>(d+14);
    auto rows=[](qint64 w,qint64 bpp,qint64 h)->std::optional<qint64>{qint64 bits;if(qMulOverflow((w*bpp+31)/32*4,h,&bits))return std::nullopt;return bits;};
    if(header==12){   // OS/2: 16-bit sizes, colour table of three bytes per entry
        const int w=qFromLittleEndian<quint16>(d+18),h=qFromLittleEndian<quint16>(d+20),bpp=qFromLittleEndian<quint16>(d+24);
        if(w<=0||bpp==0||bpp>32)return std::nullopt;
        const auto bits=rows(w,bpp,h);if(!bits)return std::nullopt;
        return DibLayout{14+12+(bpp<=8?(qint64(1)<<bpp)*3:0),*bits};
    }
    if(header<40||header>1024||at+14+40>b.size())return std::nullopt;
    const qint32 w=qFromLittleEndian<qint32>(d+18),h=qFromLittleEndian<qint32>(d+22);const quint16 bpp=qFromLittleEndian<quint16>(d+28);
    const quint32 compression=qFromLittleEndian<quint32>(d+30),image=qFromLittleEndian<quint32>(d+34),used=qFromLittleEndian<quint32>(d+46);
    if(w<=0||bpp==0||bpp>32||used>65536)return std::nullopt;
    const qint64 colours=used?used:(bpp<=8?(qint64(1)<<bpp):0),masks=compression==3&&header==40?12:0;
    const auto bits=image?std::optional<qint64>(image):rows(w,bpp,std::abs(qint64(h)));if(!bits)return std::nullopt;
    return DibLayout{14+header+masks+colours*4,*bits};
}
// The length of a bitmap as its info header gives it; -1 when the header is none of the known ones.
qint64 dibLength(const QByteArray &b,qint64 at){const auto l=dibLayout(b,at);return l?l->offset+l->bits:-1;}
}
QByteArray legacyBitmapData(QByteArray bmp){
    if(!bmp.startsWith("BM"))return bmp;
    const auto l=dibLayout(bmp,0);if(!l||l->offset>=bmp.size())return bmp;
    const qint64 stored=qFromLittleEndian<quint32>(bmp.constData()+10);
    if(stored==l->offset||stored+l->bits<=bmp.size())return bmp;
    qToLittleEndian<quint32>(quint32(l->offset),bmp.data()+10);qToLittleEndian<quint32>(quint32(bmp.size()),bmp.data()+2);
    return bmp;
}
QJsonObject LegacyReader::base(const QString &type) {
    QJsonObject o{{"type",type},{"offset",qint64(pos)}};
    o["kind"]=integer();o["width"]=integer();o["pen"]=qint64(u32());o["brush"]=qint64(u32());
    o["transparent"]=boolean();o["flag"]=boolean();o["points"]=points();o["back"]=boolean();
    if(boolean()) {
        if(bytes.mid(pos,2)!="BM")fail(ui("Unbekanntes Bildformat"));
        if(bytes.size()-pos<6)fail(ui("Unvollständiges BMP"));
        quint32 n=qFromLittleEndian<quint32>(bytes.constData()+pos+2);
        if(n<26)fail(ui("Ungültiges BMP"));
        // The original reads a bitmap by its info header (header, colour table, bits) and, saving a 24-bit picture with a
        // colour table, writes a file size and data offset 1024 bytes too large. So the info header's length counts when
        // the next field (a truth value) follows it; the file size only when it does not.
        if(const qint64 stored=dibLength(bytes,pos);stored>=26&&stored!=n&&pos+stored<bytes.size()&&(bytes[pos+stored]==8||bytes[pos+stored]==9))n=quint32(stored);
        o["bitmap_offset"]=qint64(pos);o["bitmap_size"]=qint64(n);raw(n);
    }
    o["flag2"]=boolean();o["style"]=integer();o["rotation"]=number();
    if(version>3.055)o["flag3"]=boolean();
    if(version>3.995)o["anchors"]=integers(6);
    if(version>4.005) {
        o["label"]=string();int n=integer();if(n<0||n>10000)fail(ui("Ungültige Zusatzfelder"));
        QJsonArray extra;while(n--){auto k=string();bool b=boolean();auto v=string();extra.append(QJsonArray{k,b,v});}o["extra"]=extra;
    }
    return o;
}
QJsonObject LegacyReader::object(QString type,int depth) {
    if(depth>64||++total>100000)fail(ui("Zu viele verschachtelte Objekte"));
    const auto start=pos;
    if(type.isEmpty())type=string();auto o=base(type);o["start"]=qint64(start);
    if(type=="TGruppe"||type=="TFarbcode"||type=="TBt") {
        qint64 n=qint64(integer())+1;if(n<0||n>100000)fail(ui("Ungültige Gruppengröße"));
        if(version>3.9){o["id"]=string();o["value"]=string();o["description"]=string();o["group_value"]=integer();}
        else {auto b=raw(95);auto ss=[&](int p,int max){return windowsText(b.mid(p+1,qMin(int(quint8(b[p])),max)));};o["id"]=ss(0,10);o["value"]=ss(11,40);o["description"]=ss(52,40);o["group_value"]=qFromLittleEndian<qint16>(b.constData()+93);}
        o["group_flags"]=booleans(2);QJsonArray children;while(n--)children.append(object({},depth+1));o["children"]=children;
        if(type=="TFarbcode"&&version>=3.045){o["bands"]=integer();o["resistance"]=number();}
        if(type=="TBt")o["component_kind"]=byte();
    } else if(type=="TBohrung"||type=="TAuge") {o["diameter"]=number();o["center"]=integers(2);}
    else if(type=="TDraht"||type=="TDrahtFest"||type=="TLeiterbahn") {o["path"]=points();if(boolean())o["second_path"]=points();}
    else if(type=="TTrenner"||type=="TTrennerFest") {QJsonArray a;for(int i=0;i<4;i++)a.append(i32());o["rect"]=a;}
    else if(type=="TKreis") {o["ellipse"]=integers(4);o["ellipse_flag"]=boolean();o["ellipse2"]=integers(4);if(version>3.01)o["inner"]=object("TDraht",depth+1);}
    else if(type=="TTextLabel") {
        QJsonArray p1,p2;for(int i=0;i<2;i++)p1.append(i32());for(int i=0;i<2;i++)p2.append(i32());o["text_position"]=p1;o["text_end"]=p2;
        o["text_kind"]=integer();o["text_height"]=number();o["text_width"]=number();o["text"]=string();o["text_role"]=byte();o["text_flags"]=booleans(2);
        if(version>3.035){o["font"]=string();o["font_style"]=booleans(4);}
        if(version>4.035){o["text_anchors"]=integers(12);o["text_flags2"]=booleans(2);}
        if(version>4.065)o["text_metric"]=integer();
    } else if(type!="TBauteil")fail(ui("Unbekannte Objektklasse: ")+type);
    o["end"]=qint64(pos);return o;
}
QJsonObject LegacyReader::document(bool embeddedBoard,int depth) {
    if(depth>1)fail(ui("Ungültige Platinenverschachtelung"));
    const auto start=pos;
    auto h=raw(41);if(quint8(h[0])>35)fail(ui("Ungültiger Projekttitel"));
    auto vs=QString::fromLatin1(h.mid(37,4)).replace(',','.');
    if(!QStringList{"3.08","4.03","4.04","4.06","4.07"}.contains(vs))fail(ui("Noch nicht unterstützte Dateiversion: ")+vs);
    version=vs.toDouble();QJsonObject d{{"title",windowsText(h.mid(1,quint8(h[0])))},{"version",vs},{"start",qint64(start)}};
    if(version>3.9)d["description"]=string();
    d["count_offset"]=qint64(pos);
    int n=integer();if(n<0||n>100000)fail(ui("Ungültige Objektanzahl"));
    QJsonArray objects;while(n--)objects.append(object());d["objects"]=objects;
    d["list_end"]=qint64(pos);auto settings=integers(5);d["settings"]=settings;
    d["size"]=QJsonArray{settings[1],settings[2]};d["origin"]=QJsonArray{settings[3],settings[4]};
    if(settings[1].toInt()<=0||settings[2].toInt()<=0||settings[1].toInt()>1000000||settings[2].toInt()>1000000)fail(ui("Ungültige Platinengröße"));
    d["tail_offset"]=qint64(pos);
    if(version>4.015)d["metadata"]=object("TGruppe");
    if(version>4.025){d["grid_offset"]=qint64(pos);d["grid_mm"]=number();}
    if(version>4.045) {
        QJsonArray views;d["views_offset"]=qint64(pos);
        for(int i=0;i<11;i++){
            QJsonObject view;view["flags"]=booleans(7);view["scale"]=number();view["bounds"]=integers(4);
            view["number1"]=number();view["number2"]=number();view["flags2"]=booleans(10);
            view["number3"]=number();view["integers"]=integers(3);view["flag"]=boolean();views.append(view);
        }
        d["views"]=views;d["view_index"]=integer();d["view_flags"]=booleans(4);d["view_integer"]=integer();d["views_end"]=qint64(pos);
    }
    if(version>4.055){d["numbers_offset"]=qint64(pos);const double a=number();d["numbers_second_offset"]=qint64(pos);const double b=number();d["numbers"]=QJsonArray{a,b};}
    // The first two integers are the user origin ("Ursprung setzen") in file coordinates.
    if(version>3.075){d["raw_points_offset"]=qint64(pos);d["raw_points"]=QString::fromLatin1(raw(16).toHex());}
    if(version>3.025){d["annotations_length_offset"]=qint64(pos);int count=integer();d["annotations_size"]=count;d["annotations_offset"]=qint64(pos);raw(count);}
    if(embeddedBoard&&version>3.065)d["board"]=document(false,depth+1);
    d["end"]=qint64(pos);d["tail_size"]=qint64(bytes.size()-pos);d["object_count"]=total;return d;
}
QJsonObject LegacyReader::read(bool embeddedBoard) {
    auto d=document(embeddedBoard);
    if(!embeddedBoard){
        // Library pages end with their document; board templates add the board count 1 (02 01).
        if(bytes.size()-pos>2||bytes.size()-pos==1)fail(ui("Unerwartete Daten am Dateiende"));
        if(bytes.size()-pos==2&&bytes.mid(pos)!=QByteArray::fromHex("0201"))fail(ui("Unbekannter Dateiabschluss"));
        return d;
    }
    // A project holds board 0, then the board count, then boards 1 to count-1, each a document with its own layout.
    // Like the original, a count below 2 means one board and bytes after the last board are ignored.
    d["board_end"]=qint64(pos);if(pos>=bytes.size())return d;
    const int count=integer();if(count>100)fail(ui("Zu viele Platinen in der Datei"));
    QJsonArray more;for(int i=1;i<count;i++){const auto start=pos;document(true);more.append(QJsonObject{{"start",qint64(start)},{"end",qint64(pos)}});}
    if(!more.isEmpty())d["more_boards"]=more;
    return d;
}
}
