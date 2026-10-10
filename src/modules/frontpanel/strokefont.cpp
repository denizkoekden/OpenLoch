#include "strokefont.h"
#include "delphistream.h"
#include "language.h"
#include "legacy_reader.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringEncoder>
#include <QtEndian>
#include <cmath>
#include <memory>
#include <numbers>
#include <array>
#include <tuple>

namespace openloch::frontpanel {
namespace {
constexpr qsizetype maxFontSize=16*1024*1024;
[[noreturn]] void fail(const QString &why){throw FormatError(why);}
// The sixteen directions of a vector byte: a square of slopes, not a circle.
QPointF direction(int code){
    static const QPointF table[16]={{1,0},{1,0.5},{1,1},{0.5,1},{0,1},{-0.5,1},{-1,1},{-1,0.5},{-1,0},{-1,-0.5},{-1,-1},{-0.5,-1},{0,-1},{0.5,-1},{1,-1},{1,-0.5}};
    return table[code&15];
}
// The length of the command at `i` with its operands (code 14 skips one command in horizontal text).
int commandLength(const QByteArray &s,int i,const StrokeFont &font){
    if(i>=s.size())return 1;
    switch(quint8(s[i])){
    case 3:case 4:return 2;
    case 7:return font.unicode?3:font.bigfont&&i+1<s.size()&&s[i+1]==0?(font.extended?8:7):2;
    case 8:return 3;
    case 9:{int k=i+1;while(k+1<s.size()&&!(s[k]==0&&s[k+1]==0))k+=2;return k+2-i;}
    case 10:return 3;
    case 11:return 6;
    case 12:return 4;
    case 13:{int k=i+1;while(k+1<s.size()&&!(s[k]==0&&s[k+1]==0))k+=3;return k+2-i;}
    case 14:return 1+commandLength(s,i+1,font);
    default:return 1;
    }
}
// The octant value of codes 10 and 11 as AutoCAD compiles it: the magnitude, with bit 7 set for clockwise.
int octantValue(int byte){return byte&0x80?-(byte&0x7f):byte;}
// The pen of the shape interpreter: draws while it is down, in font units with y upwards.
struct Plotter {
    QPainterPath path;QPointF pos,origin;bool down=true,drawing=false;double scale=1;QList<QPointF> stack;   // origin: where the character starts
    void moveTo(QPointF to){
        if(down){if(!drawing){path.moveTo(pos);drawing=true;}path.lineTo(to);}else drawing=false;
        pos=to;
    }
    void arc(QPointF centre,double radius,double start,double sweep){   // degrees, counter-clockwise for positive sweep
        const int steps=std::max(2,int(std::ceil(std::abs(sweep)/7.5)));
        for(int i=1;i<=steps;i++){const double a=(start+sweep*i/steps)*std::numbers::pi/180;moveTo(centre+QPointF(radius*std::cos(a),radius*std::sin(a)));}
    }
    // An arc to pos + d whose height is bulge/127 of half the chord; positive bulges run counter-clockwise.
    void bulge(QPointF d,int b){
        const double chord=std::hypot(d.x(),d.y());if(chord<1e-12)return;
        if(b==0){moveTo(pos+d);return;}
        const double h=std::abs(b)/127.0*chord/2,r=(chord*chord/4+h*h)/(2*h),side=b>0?1:-1;
        const QPointF end=pos+d,normal(-d.y()/chord,d.x()/chord),centre=pos+d/2+normal*(side*(r-h));
        const double start=std::atan2(pos.y()-centre.y(),pos.x()-centre.x())*180/std::numbers::pi,theta=4*std::atan(h/(chord/2))*180/std::numbers::pi;
        arc(centre,r,start,side*theta);pos=end;
    }
    // Arcs given by octants: start octant, number of octants (0 = all eight) and direction; fractional arcs start and
    // end within an octant by offsets of 1/256 octant.
    void octants(double radius,int octantByte,int startOffset,int endOffset){
        if(radius<=0)return;
        const double sign=octantByte<0?-1:1;const int m=std::abs(octantByte),first=(m>>4)&7;int count=m&7;if(count==0)count=8;
        double start=first*45.0,end=start+sign*count*45.0;
        if(startOffset)start+=sign*startOffset*45.0/256;
        if(endOffset)end=first*45.0+sign*((count-1)*45.0+endOffset*45.0/256);
        const double a=start*std::numbers::pi/180;arc(pos-QPointF(radius*std::cos(a),radius*std::sin(a)),radius,start,end-start);
    }
};
void run(const StrokeFont &font,const QByteArray &s,Plotter &p,int depth){
    if(depth>8)return;
    auto at=[&](int k){return k<s.size()?int(quint8(s[k])):0;};
    auto signedAt=[&](int k){return k<s.size()?int(qint8(s[k])):0;};
    for(int i=0;i<s.size();){
        const int code=at(i);
        switch(code){
        case 0:return;
        case 1:p.down=true;i++;break;
        case 2:p.down=false;p.drawing=false;i++;break;
        case 3:if(at(i+1))p.scale/=at(i+1);i+=2;break;
        case 4:if(at(i+1))p.scale*=at(i+1);i+=2;break;
        case 5:if(p.stack.size()<16)p.stack<<p.pos;i++;break;
        case 6:if(!p.stack.isEmpty()){p.pos=p.stack.takeLast();p.drawing=false;}i++;break;   // back without drawing
        case 7:{
            if(font.bigfont&&at(i+1)==0){
                // A part drawn into a box of the character: its number, the box's corner and height (extended fonts also
                // give a width); the pen comes back to where it was.
                const int number=at(i+2)*256+at(i+3);const QPointF corner(signedAt(i+4),signedAt(i+5));const int height=font.extended?at(i+7):at(i+6);
                i+=font.extended?8:7;
                if(const auto it=font.shapes.constFind(number);it!=font.shapes.constEnd()&&font.above>0){
                    const QPointF keep=p.pos;const double scale=p.scale;const bool down=p.down;
                    p.drawing=false;p.down=true;p.pos=p.origin+corner*scale;p.scale=scale*height/font.above;run(font,*it,p,depth+1);
                    p.scale=scale;p.pos=keep;p.down=down;p.drawing=false;
                }
                break;
            }
            const int number=font.unicode?at(i+1)*256+at(i+2):at(i+1);i+=font.unicode?3:2;
            if(const auto it=font.shapes.constFind(number);it!=font.shapes.constEnd()){const double scale=p.scale;run(font,*it,p,depth+1);p.scale=scale;}
            break;}
        case 8:p.moveTo(p.pos+QPointF(signedAt(i+1),signedAt(i+2))*p.scale);i+=3;break;
        case 9:i++;while(i+1<s.size()){const int dx=signedAt(i),dy=signedAt(i+1);i+=2;if(dx==0&&dy==0)break;p.moveTo(p.pos+QPointF(dx,dy)*p.scale);}break;
        case 10:p.octants(at(i+1)*p.scale,octantValue(at(i+2)),0,0);i+=3;break;
        case 11:p.octants((at(i+3)*256+at(i+4))*p.scale,octantValue(at(i+5)),at(i+1),at(i+2));i+=6;break;
        case 12:p.bulge(QPointF(signedAt(i+1),signedAt(i+2))*p.scale,signedAt(i+3));i+=4;break;
        case 13:i++;while(i+1<s.size()){const int dx=signedAt(i),dy=signedAt(i+1);if(dx==0&&dy==0){i+=2;break;}p.bulge(QPointF(dx,dy)*p.scale,signedAt(i+2));i+=3;}break;
        case 14:i+=commandLength(s,i,font);break;   // only for vertical text
        default:p.moveTo(p.pos+direction(code)*(code>>4)*p.scale);i++;break;
        }
    }
}
// The numbers a character may have in a big font: below 128 its own code, otherwise its two bytes in one of the
// encodings such fonts are made for. The font does not name its encoding; Japanese fonts are recognised by their escape
// range, the others are tried in turn, and only numbers whose first byte opens an escape range count.
QList<int> bigfontCodes(const StrokeFont &font,QChar c){
    if(c.unicode()<0x80)return {int(c.unicode())};
    QList<int> out;
    auto escapes=[&](int lead){for(const auto &[first,last]:font.escapes)if(lead>=first&&lead<=last)return true;return false;};
    const bool japanese=escapes(0x81)&&escapes(0x9f)&&!escapes(0xa1);
    QList<const char*> encodings{"GBK","Big5","EUC-KR"};if(japanese)encodings.prepend("Shift_JIS");else encodings.append("Shift_JIS");
    for(const char *name:encodings){
        QStringEncoder encoder(name);if(!encoder.isValid())continue;
        const QByteArray bytes=encoder.encode(QString(c));if(encoder.hasError())continue;
        if(bytes.size()==2&&escapes(quint8(bytes[0])))out<<(quint8(bytes[0])<<8|quint8(bytes[1]));else if(bytes.size()==1)out<<quint8(bytes[0]);
    }
    return out;
}
}

int shapeNumberFor(int code,bool dosOrder){
    // FrontDesigner swaps the letters in this order once it has made them.
    static const std::array<int,256> moved=[]{
        constexpr std::pair<int,int> swaps[]={
            {0xb1,0xf1},{0xdf,0xe1},{0xbf,0xa8},{0xd1,0xa5},{0xf1,0xa4},{0xfa,0xa3},{0xf3,0xa2},{0xed,0xa1},{0xe1,0xa0},{0xd8,0x9d},
            {0xa3,0x9c},{0xf8,0x9b},{0xdc,0x9a},{0xd6,0x99},{0xff,0x98},{0xf9,0x97},{0xfb,0x96},{0xf2,0x95},{0xf6,0x94},{0xf4,0x93},
            {0xc6,0x92},{0xe6,0x91},{0xc9,0x90},{0xc5,0x8f},{0xc4,0x8e},{0xcc,0x8d},{0xce,0x8c},{0xcf,0x8b},{0xeb,0x89},{0xea,0x88},
            {0xe7,0x87},{0xe5,0x86},{0xe0,0x85},{0xe4,0x84},{0xe2,0x83},{0xe9,0x82},{0xfc,0x81},{0xc7,0x80},{0xb0,0x7f},{0xa7,0x15}};
        std::array<int,256> m{};for(int i=0;i<256;i++)m[i]=i;
        for(const auto &[a,b]:swaps)std::swap(m[a],m[b]);
        return m;
    }();
    return dosOrder&&code>=0&&code<256?moved[code]:code;
}

QPainterPath StrokeFont::text(const QString &s,double *advance) const{
    Plotter p;double x=0;
    auto draw=[&](int glyphCode,int shapeNumber,bool strokesOnly){
        if(const auto g=glyphs.constFind(glyphCode);g!=glyphs.constEnd()&&(!strokesOnly||!g->elements.isEmpty()||glyphCode==' ')){p.path.addPath(g->path.translated(x,0));x+=g->advance;return true;}
        if(const auto it=shapes.constFind(shapeNumber);it!=shapes.constEnd()){p.pos=p.origin=QPointF(x,0);p.down=true;p.drawing=false;p.scale=1;p.stack.clear();run(*this,*it,p,0);x=p.pos.x();return true;}
        return false;
    };
    for(const QChar c:s){
        bool found=false;
        if(bigfont||unicode){for(int code:bigfont?bigfontCodes(*this,c):QList<int>{c.unicode()})if(draw(code,code,false)){found=true;break;}}
        else{
            // As FrontDesigner: the Windows-1252 code (a question mark outside it), the letter of its own format by that
            // code, the shape by its number for the code; a letter without strokes is the space.
            const QByteArray b=frontdesigner::toWindows1252(QString(c));const int code=b.size()==1&&b[0]?quint8(b[0]):'?';
            found=draw(code,shapeNumberFor(code,dosOrder),true)||draw(' ',' ',true);
        }
        if(!found)x+=std::max(above,1)*0.6;   // missing letter: a gap
    }
    if(advance)*advance=x;
    return p.path;
}

StrokeFont StrokeFont::fromShx(const QByteArray &data){
    const int end=data.indexOf('\x1a');
    if(!data.startsWith("AutoCAD-86")||end<0||end>64)fail(ui("Keine Strichschrift (SHX)"));
    const QByteArray head=data.left(end);qsizetype at=end+1;StrokeFont f;
    auto need=[&](qsizetype n){if(n<0||at+n>data.size())fail(ui("Die Strichschrift ist abgeschnitten"));};
    auto u16=[&](){need(2);const int v=qFromLittleEndian<quint16>(data.constData()+at);at+=2;return v;};
    auto take=[&](int n){need(n);const QByteArray b=data.mid(at,n);at+=n;return b;};
    auto shape=[&](int number,const QByteArray &definition,bool info){
        const int zero=definition.indexOf('\0');const QByteArray spec=zero>=0?definition.mid(zero+1):definition;
        if(info){f.name=QString::fromLatin1(definition.left(std::max(0,zero))).trimmed();if(spec.size()>=2){f.above=quint8(spec[0]);f.below=quint8(spec[1]);}}
        else f.shapes.insert(number,spec);
    };
    if(head.contains("unifont")){
        f.unicode=true;need(4);const quint32 count=qFromLittleEndian<quint32>(data.constData()+at);at+=4;if(count>100000)fail(ui("Ungültige Strichschrift"));
        const int infoLength=u16();shape(0,take(infoLength),true);
        while(at+4<=data.size()){const int number=u16(),length=u16();shape(number,take(length),false);}
    }else if(head.contains("bigfont")){
        // The size of this header, the number of characters and of escape ranges, the ranges (first and last lead byte),
        // then per character its number, length and position in the file. Character 0 holds the name and the heights;
        // extended big fonts keep height, mode and width there.
        f.bigfont=true;u16();const int count=u16(),ranges=u16();if(count<=0||ranges>128)fail(ui("Ungültige Strichschrift"));
        for(int k=0;k<ranges;k++){const int first=u16(),last=u16();f.escapes.append({first,last});}
        QList<std::tuple<int,int,quint32>> index;
        for(int k=0;k<count;k++){const int number=u16(),length=u16();need(4);const quint32 offset=qFromLittleEndian<quint32>(data.constData()+at);at+=4;
            if(number||length||offset)index.append({number,length,offset});}
        for(const auto &[number,length,offset]:index){
            if(qsizetype(offset)+length>data.size())fail(ui("Die Strichschrift ist abgeschnitten"));
            const QByteArray definition=data.mid(offset,length);
            if(number!=0){shape(number,definition,false);continue;}
            const int zero=definition.indexOf('\0');f.name=QString::fromLatin1(definition.left(std::max(0,zero))).trimmed();const QByteArray spec=zero>=0?definition.mid(zero+1):QByteArray();
            if(spec.size()>4){f.extended=true;f.above=quint8(spec[0]);f.below=0;}else if(spec.size()>=2){f.above=quint8(spec[0]);f.below=quint8(spec[1]);}
        }
    }else if(head.contains("shapes")){
        u16();u16();const int count=u16();if(count<=0||count>65535)fail(ui("Ungültige Strichschrift"));
        QList<std::pair<int,int>> index;for(int k=0;k<count;k++){const int number=u16(),length=u16();index.append({number,length});}
        for(const auto &[number,length]:index)shape(number,take(length),number==0);
    }else fail(ui("Diese Art von Strichschrift wird nicht unterstützt: %1").arg(QString::fromLatin1(head.trimmed())));
    if(f.shapes.isEmpty())fail(ui("Die Strichschrift enthält keine Zeichen"));
    if(f.above<=0)f.above=10;
    return f;
}

StrokeFont StrokeFont::fromShp(const QByteArray &text){
    StrokeFont f;int number=-1;bool info=false;QList<int> values;QString name;
    auto flush=[&]{
        if(number<0)return;
        QByteArray spec;for(int v:values)spec.append(char(v&0xff));
        if(info){f.name=name;if(spec.size()>=2){f.above=quint8(spec[0]);f.below=quint8(spec[1]);}}
        else{
            // Compiled like AutoCAD: a negative octant value as its magnitude with bit 7 set, other negative numbers in
            // two's complement.
            for(int i=0;i<spec.size();){
                const int c=quint8(spec[i]),k=c==10?i+2:c==11?i+5:-1;
                if(k>=0&&k<spec.size()&&values[k]<0)spec[k]=char(0x80|(-values[k]&0x7f));
                i+=c==14?1:commandLength(spec,i,f);
            }
            f.shapes.insert(number,spec);
        }
        number=-1;info=false;values.clear();
    };
    // Numbers with a leading zero are hexadecimal.
    auto value=[](QString t,bool *ok){
        t=t.trimmed();const bool negative=t.startsWith('-');if(negative||t.startsWith('+'))t=t.mid(1).trimmed();
        const int v=t.size()>1&&t.startsWith('0')?t.toInt(ok,16):t.toInt(ok,10);return negative?-v:v;
    };
    for(QString line:QString::fromLatin1(text).split('\n')){
        if(const int comment=line.indexOf(';');comment>=0)line.truncate(comment);
        line=line.trimmed();if(line.isEmpty())continue;
        if(line.startsWith('*')){
            flush();const QStringList parts=line.mid(1).split(',');name=parts.mid(2).join(',').trimmed();
            if(parts.value(0).trimmed().compare("UNIFONT",Qt::CaseInsensitive)==0){f.unicode=true;info=true;number=0;continue;}
            // *BIGFONT characters,ranges,first,last,…: the escape ranges of a big font; its name follows as character 0.
            if(parts.value(0).trimmed().startsWith("BIGFONT",Qt::CaseInsensitive)){
                f.bigfont=true;bool ok=false;const int ranges=value(parts.value(1),&ok);
                for(int k=0;ok&&k<ranges&&k<128;k++){bool a=false,b=false;const int first=value(parts.value(2+2*k),&a),last=value(parts.value(3+2*k),&b);if(a&&b)f.escapes.append({first,last});}
                continue;
            }
            bool ok=false;number=value(parts.value(0),&ok);if(!ok||number<0||number>65535)fail(ui("Ungültige Zeichennummer in der Strichschrift: %1").arg(parts.value(0)));
            info=number==0&&!f.unicode;continue;
        }
        if(number<0)continue;
        for(const QString &token:line.split(',',Qt::SkipEmptyParts)){
            QString t=token;t.remove('(').remove(')');if(t.trimmed().isEmpty())continue;
            bool ok=false;const int v=value(t,&ok);if(!ok||v<-128||v>255)fail(ui("Ungültiger Wert in der Strichschrift: %1").arg(token.trimmed()));
            values.append(v);
        }
    }
    flush();
    if(f.shapes.isEmpty())fail(ui("Die Strichschrift enthält keine Zeichen"));
    if(f.above<=0)f.above=10;
    f.source=text;
    return f;
}

StrokeFont StrokeFont::fromFhx(const QByteArray &data){
    // A value stream with one record per character code from 1 to 255: the advance, the number of elements, then the
    // elements: 2 lifts the pen, 0 is a line (start and end), 1 an arc (start, end, centre, radius and whether it runs
    // counter-clockwise). The coordinates have y downwards.
    frontdesigner::DelphiReader r(data);StrokeFont f;int code=1;
    while(!r.atEnd()){
        if(code>255)fail(ui("Ungültige Strichschrift"));
        StrokeGlyph g;g.advance=r.integer();const int count=r.integer();if(count<0||count>100000)fail(ui("Ungültige Strichschrift"));
        bool drawing=false;
        auto from=[&](QPointF a){if(!drawing||g.path.currentPosition()!=a)g.path.moveTo(a);drawing=true;};
        for(int k=0;k<count;k++){
            const int kind=r.integer();
            if(kind==2){drawing=false;g.elements.append(StrokeElement{StrokeElement::PenUp});continue;}
            if(kind==0){const double x1=r.real(),y1=r.real(),x2=r.real(),y2=r.real();from(QPointF(x1,-y1));g.path.lineTo(x2,-y2);g.elements.append(StrokeElement{StrokeElement::Line,x1,y1,x2,y2});continue;}
            if(kind!=1)fail(ui("Ungültige Strichschrift"));
            const double x1=r.real(),y1=r.real(),x2=r.real(),y2=r.real(),cx=r.real(),cy=r.real(),radius=r.real();const bool counterClockwise=r.boolean();
            g.elements.append(StrokeElement{StrokeElement::Arc,x1,y1,x2,y2,cx,cy,radius,counterClockwise});
            const QPointF a(x1,-y1),b(x2,-y2),c(cx,-cy);from(a);
            const double a0=std::atan2(a.y()-c.y(),a.x()-c.x())*180/std::numbers::pi,a1=std::atan2(b.y()-c.y(),b.x()-c.x())*180/std::numbers::pi;
            double sweep=a1-a0;if(counterClockwise){while(sweep<=1e-9)sweep+=360;}else while(sweep>=-1e-9)sweep-=360;
            const int steps=std::max(2,int(std::ceil(std::abs(sweep)/7.5)));
            for(int i=1;i<steps;i++){const double t=(a0+sweep*i/steps)*std::numbers::pi/180;g.path.lineTo(c+QPointF(radius*std::cos(t),radius*std::sin(t)));}
            g.path.lineTo(b);
        }
        if(count>0||g.advance!=0)f.glyphs.insert(code,g);
        code++;
    }
    if(f.glyphs.isEmpty())fail(ui("Die Strichschrift enthält keine Zeichen"));
    // The file has no font height: capital height from the H, descender depth from the p.
    auto extent=[&](QChar letter){const auto g=f.glyphs.constFind(letter.unicode());return g==f.glyphs.constEnd()?QRectF():g->path.boundingRect();};
    const QRectF capital=extent('H'),descender=extent('p');
    f.above=int(std::lround(capital.isValid()?capital.bottom():10));f.below=int(std::lround(descender.isValid()?std::max(0.0,-descender.top()):f.above*0.3));
    if(f.above<=0)f.above=10;
    return f;
}

StrokeFont StrokeFont::load(const QString &path){
    QFile file(path);if(!file.open(QIODevice::ReadOnly))fail(file.errorString());
    if(file.size()>maxFontSize)fail(ui("Datei ist zu groß"));
    const QByteArray data=file.readAll();
    StrokeFont f=data.startsWith("AutoCAD-86")?fromShx(data):QFileInfo(path).suffix().compare("fhx",Qt::CaseInsensitive)==0?fromFhx(data):fromShp(data);
    if(f.name.isEmpty())f.name=QFileInfo(path).completeBaseName();
    return f;
}

namespace {
// The files of each font name by their ending (shx, shp, fhx), found once in the folders.
struct Registry {bool scanned=false;QHash<QString,QHash<QString,QString>> files;QHash<QString,std::shared_ptr<StrokeFont>> fonts;};
Registry &registry(){static Registry r;return r;}
QString keyOf(const QString &name){
    QString n=name.trimmed();for(const char *ending:{".shx",".shp",".fhx"})if(n.endsWith(ending,Qt::CaseInsensitive)){n.chop(4);break;}return n.toLower();
}
void scan(){
    Registry &r=registry();if(r.scanned)return;r.scanned=true;
    for(const auto &folder:strokeFontFolders())
        for(const auto &info:QDir(folder).entryInfoList({"*.shx","*.SHX","*.Shx","*.shp","*.SHP","*.Shp","*.fhx","*.FHX","*.Fhx"},QDir::Files,QDir::Name)){
            auto &found=r.files[keyOf(info.completeBaseName())];const QString suffix=info.suffix().toLower();
            if(!found.contains(suffix))found.insert(suffix,info.filePath());
        }
}
// FrontDesigner's own file holds the letters as the original draws them, in Windows code order; its font height comes
// from a shape font of the same name when there is one. Otherwise a compiled shape font comes before its source text.
std::shared_ptr<StrokeFont> loadFont(const QString &key,const QHash<QString,QString> &files){
    auto load=[&](const char *suffix)->std::shared_ptr<StrokeFont>{
        if(!files.contains(suffix))return nullptr;
        try{return std::make_shared<StrokeFont>(StrokeFont::load(files.value(suffix)));}catch(const std::exception&){return nullptr;}
    };
    auto shape=load("shx");
    // FrontDesigner makes its letters from the source text, so a compiled font keeps the text that comes with it.
    if(shape&&files.contains("shp")){QFile text(files.value("shp"));if(text.size()<=maxFontSize&&text.open(QIODevice::ReadOnly))shape->source=text.readAll();}
    if(!shape)shape=load("shp");
    if(shape)shape->dosOrder=strokeFontInDosOrder(key);
    auto own=load("fhx");
    if(own&&shape){own->above=shape->above;own->below=shape->below;}
    return own?own:shape;
}
}

QString ownStrokeFontFolder(){return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("OpenLoch/Strichschriften");}
QStringList strokeFontFolders(){QStringList folders{ownStrokeFontFolder()};for(const auto &f:QSettings().value("frontpanel/strokeFontFolders").toStringList())if(!folders.contains(f))folders<<f;return folders;}
void setStrokeFontFolders(const QStringList &folders){QStringList extra=folders;extra.removeAll(ownStrokeFontFolder());QSettings().setValue("frontpanel/strokeFontFolders",extra);forgetStrokeFonts();}
void forgetStrokeFonts(){registry()=Registry();}
bool strokeFontsInDosOrder(){return QSettings().value("frontpanel/strokeFontsDosOrder",true).toBool();}
void setStrokeFontsInDosOrder(bool on){QSettings().setValue("frontpanel/strokeFontsDosOrder",on);forgetStrokeFonts();}
std::optional<bool> ownStrokeFontOrder(const QString &name){
    const QVariantMap orders=QSettings().value("frontpanel/strokeFontOrders").toMap();const auto it=orders.constFind(keyOf(name));
    if(it==orders.constEnd())return std::nullopt;return it->toBool();
}
bool strokeFontInDosOrder(const QString &name){return ownStrokeFontOrder(name).value_or(strokeFontsInDosOrder());}
void setStrokeFontInDosOrder(const QString &name,std::optional<bool> dos){
    QVariantMap orders=QSettings().value("frontpanel/strokeFontOrders").toMap();if(dos)orders.insert(keyOf(name),*dos);else orders.remove(keyOf(name));
    QSettings().setValue("frontpanel/strokeFontOrders",orders);forgetStrokeFonts();
}
QStringList shapeFontNames(){
    scan();QStringList names;
    for(auto it=registry().files.constBegin();it!=registry().files.constEnd();++it)
        if(it->contains("shx")||it->contains("shp"))names<<QFileInfo(it->contains("shx")?it->value("shx"):it->value("shp")).completeBaseName();
    names.sort(Qt::CaseInsensitive);return names;
}
std::optional<bool> umlautsInDosPlaces(const QString &name){
    scan();const auto files=registry().files.value(keyOf(name));const QString path=files.value("shx",files.value("shp"));if(path.isEmpty())return std::nullopt;
    StrokeFont font;try{font=StrokeFont::load(path);}catch(const std::exception&){return std::nullopt;}
    auto count=[&](std::initializer_list<int> numbers){int n=0;for(int c:numbers)n+=font.shapes.contains(c);return n;};
    const int dos=count({0x84,0x94,0x81,0x8e,0x99,0x9a}),windows=count({0xe4,0xf6,0xfc,0xc4,0xd6,0xdc});   // äöüÄÖÜ
    if(dos>windows)return true;if(windows>dos)return false;return std::nullopt;
}
QStringList strokeFontNames(){
    scan();QStringList names;
    for(auto it=registry().files.constBegin();it!=registry().files.constEnd();++it)names<<QFileInfo(it.value().constBegin().value()).completeBaseName();
    if(!registry().files.contains(keyOf(ownStrokeFont().name)))names<<ownStrokeFont().name;
    names.sort(Qt::CaseInsensitive);return names;
}
const StrokeFont *findStrokeFont(const QString &name){
    if(name.trimmed().isEmpty())return nullptr;
    scan();Registry &r=registry();QString key=keyOf(name);
    // A stroke text without a font name ("SHX" after reading) is drawn by the original in its standard font.
    if(key==QLatin1String("shx"))key=QStringLiteral("din1451");
    if(const auto it=r.fonts.constFind(key);it!=r.fonts.constEnd())return it->get();
    const auto files=r.files.constFind(key);const std::shared_ptr<StrokeFont> font=files==r.files.constEnd()?nullptr:loadFont(key,*files);
    r.fonts.insert(key,font);
    // The own font answers to its name unless a file of that name is installed.
    if(!font&&key==keyOf(ownStrokeFont().name))return &ownStrokeFont();
    return font.get();
}
const StrokeFont *strokeFontFor(const QString &name){
    if(name.trimmed().isEmpty())return nullptr;
    const StrokeFont *font=findStrokeFont(name);return font?font:&ownStrokeFont();
}
}
