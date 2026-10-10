#include "fdshapes.h"
#include "x87.h"
#include <algorithm>
#include <array>
#include <memory>

namespace openloch::frontdesigner {
using frontpanel::StrokeElement;using frontpanel::StrokeFont;using frontpanel::StrokeGlyph;
namespace {
struct Failure {};  // where the original stops with an error

// Delphi's StrToInt: blanks before it, a sign, then decimal digits, or hexadecimal ones after $, x or 0x; nothing
// after the digits.
std::optional<int> strToInt(const QByteArray &s){
    qsizetype i=0;auto at=[&](qsizetype k){return k<s.size()?s[k]:'\0';};
    while(at(i)==' ')i++;
    bool negative=false;
    if(at(i)=='-'){negative=true;i++;}else if(at(i)=='+')i++;
    bool hex=false;
    if(at(i)=='$'||at(i)=='x'||at(i)=='X'){hex=true;i++;}
    else if(at(i)=='0'&&(at(i+1)=='x'||at(i+1)=='X')){hex=true;i+=2;}
    if(i>=s.size())return std::nullopt;
    quint32 v=0;
    for(;i<s.size();i++){
        const char c=s[i];int d;
        if(hex){
            const char u=c>='a'?char(c-0x20):c;
            if(u>='0'&&u<='9')d=u-'0';else if(u>='A'&&u<='F')d=u-'A'+10;else return std::nullopt;
            if(v>0x0fffffffu)return std::nullopt;
            v=v<<4|quint32(d);
        }else{
            if(c<'0'||c>'9')return std::nullopt;
            if(v>0x0cccccccu)return std::nullopt;
            v=v*10+quint32(c-'0');
        }
    }
    if(negative)v=0u-v;
    if(!hex&&(negative?qint32(v)>0:qint32(v)<0))return std::nullopt;
    return qint32(v);
}
// A number of a shape as the original reads it: brackets and asterisks dropped; with a leading 0 the next two characters
// (or the 0 and one) as hexadecimal digits in capitals, anything else counting as 0.
std::optional<int> number(QByteArray t){
    t.removeIf([](char c){return c=='('||c==')'||c=='*';});
    if(t.isEmpty())return 0;
    if(t.size()==1||t[0]!='0')return strToInt(t);
    auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:0;};
    const char a=t.size()==2?'0':t[1],b=t.size()==2?t[1]:t[2];
    return (digit(a)*16+digit(b))&0xff;
}
// The same number as AutoCAD reads it: a sign, then hexadecimal digits after a leading 0, else decimal ones.
std::optional<int> autocadNumber(QByteArray t){
    t.removeIf([](char c){return c=='('||c==')'||c=='*';});t=t.trimmed();
    const bool negative=t.startsWith('-');if(negative||t.startsWith('+'))t=t.mid(1);
    bool ok=false;const int v=t.size()>1&&t.startsWith('0')?t.toInt(&ok,16):t.toInt(&ok,10);
    if(!ok)return std::nullopt;
    return negative?-v:v;
}

const Extended pi=Extended::fromBytes(QByteArray::fromHex("35c26821a2da0fc90040").constData()),
               halfPi=Extended::fromBytes(QByteArray::fromHex("35c26821a2da0fc9ff3f").constData()),
               twoPi=Extended::fromBytes(QByteArray::fromHex("35c26821a2da0fc90140").constData());
Extended E(double v){return Extended::fromDouble(v);}
Extended I(qint64 v){return Extended::fromInt(v);}
double D(const Extended &v){return v.toDouble();}
qint32 abs32(qint32 v){return v<0?qint32(0u-quint32(v)):v;}
qint32 sixteenths(qint32 v){return (v<0?qint32(quint32(v)+15u):v)>>4;}   // v / 16 towards zero

struct Point {double x=0,y=0;};
// The direction from b to a in [0, 2 pi) as the original computes it for bulges, kept as a double.
double direction(const Point &a,const Point &b){
    const double dx=D(E(a.x)-E(b.x)),dy=D(E(a.y)-E(b.y));
    double r=0;
    if(dx==0)r=dy<0?0x1.2d97c7f3321d2p+2:0x1.921fb54442d18p+0;
    if(dy==0)r=dx<0?0x1.921fb54442d18p+1:0;
    if(dx!=0&&dy!=0){
        r=D((E(dy)/E(dx)).atan().abs());
        if(dx<0&&dy>=0)r=D(pi-E(r));
        if(dx<0&&dy<0)r=D(pi+E(r));
        if(dx>=0&&dy<0)r=D(twoPi-E(r));
    }
    if(r<0)r=D(twoPi+E(r));
    if(!(E(r)<twoPi))r=D(E(r)-twoPi);
    return r;
}

// One letter: the shape's numbers run like commands, positions in doubles (y downwards), each step in the x87 unit.
StrokeGlyph letter(const ShapeNumbers &numbers,int code,double k){
    const auto first=numbers(code);if(!first)throw Failure();
    QList<ShapeNumber> list=*first;StrokeGlyph g;
    if(list.size()<2)return g;
    auto need=[&](qsizetype i){if(i<0||i>=list.size())throw Failure();return list[i].value;};
    auto octantValue=[&](qsizetype i){if(i<0||i>=list.size())throw Failure();return list[i].autocad;};
    Point pos,next;double scale=1;bool pen=true;std::array<Point,11> stack;int depth=0;
    auto line=[&]{if(pen)g.elements.append(StrokeElement{StrokeElement::Line,pos.x,pos.y,next.x,next.y});pos=next;};
    auto arc=[&](const Point &centre,double radius,bool counterClockwise){
        if(pen)g.elements.append(StrokeElement{StrokeElement::Arc,pos.x,pos.y,next.x,next.y,centre.x,centre.y,std::abs(radius),counterClockwise});pos=next;
    };
    // Octant arcs: start and end angle in degrees, the centre from the start angle. A full circle is made of two half
    // circles, as an arc that ends where it starts is plotted as a point.
    auto octants=[&](double radius,double start,double end,bool counterClockwise,bool full){
        const Extended a=E(D(E(start)/I(180)*pi)),r=E(radius);
        const Point centre{D(E(pos.x)-a.cos()*r),D(a.sin()*r+E(pos.y))};
        auto at=[&](double degrees){const Extended b=E(D(E(degrees)/I(180)*pi));return Point{D(b.cos()*r+E(centre.x)),D(E(centre.y)-b.sin()*r)};};
        if(full){next=at(D(E(start)+I(counterClockwise?180:-180)));arc(centre,radius,counterClockwise);}
        next=at(end);arc(centre,radius,counterClockwise);
    };
    for(qsizetype i=0;;i++){
        const int c=need(i);
        switch(quint32(c)<=14?c:-1){
        case 0:g.advance=double(qint32(E(pos.x).rounded()));return g;
        case 1:pen=true;break;
        case 2:pen=false;g.elements.append(StrokeElement{StrokeElement::PenUp});break;
        case 3:{const int b=need(i+1);if(b>0)scale=D(E(scale)/I(b));i++;break;}
        case 4:scale=D(I(need(i+1))*E(scale));i++;break;
        case 5:if(depth<10)stack[++depth]=pos;break;
        case 6:if(depth>0){pos=stack[depth--];g.elements.append(StrokeElement{StrokeElement::PenUp});}break;
        case 7:{
            // A subshape: all its numbers but the last (the end) go in after this one.
            const int sub=need(++i);const auto inner=numbers(sub&0xff);if(!inner)throw Failure();
            if(list.size()+inner->size()>1000000)throw Failure();
            for(qsizetype n=0;n+1<inner->size();n++)list.insert(i+1+n,(*inner)[n]);
            break;}
        case 8:next={D(I(need(i+1))*E(scale)+E(pos.x)),D(E(pos.y)-I(need(i+2))*E(scale))};line();i+=2;break;
        case 9:{
            double dx=D(I(need(i+1))*E(scale)),dy=D(I(need(i+2))*E(scale));
            while(dx!=0||dy!=0){next={D(E(pos.x)+E(dx)),D(E(pos.y)-E(dy))};line();i+=2;dx=D(I(need(i+1))*E(scale));dy=D(I(need(i+2))*E(scale));}
            i+=2;break;}
        case 10:case 11:{
            // The octant value as AutoCAD defines it: its sign the direction, its digits the start octant and the number
            // of octants (0 for all eight). A fractional arc starts and ends within an octant, by offsets of 1/256
            // octant from the start octant and from the last octant it reaches. The original instead turns every such
            // arc counter-clockwise, a full circle into a point and ends a fractional arc one octant later.
            const bool fraction=c==11;
            const qint32 radius=fraction?qint32((quint32(need(i+3))<<8)+quint32(need(i+4))):need(i+1),v=octantValue(fraction?i+5:i+2);
            const qint32 sign=v<0?-1:1,m=abs32(v),count=m&7?m&7:8,endOffset=fraction?need(i+2):0;
            const double r=D(I(radius)*E(scale)),first=double((m>>4)&7);
            double start=D(E(first)*I(45)),end=D(I(sign*(endOffset?count-1:count))*I(45)+E(start));
            if(fraction){
                if(I(360)<E(end))end=D(E(end)-I(360));
                const Extended from=I(qint32(quint32(need(i+1))*45u))/I(256),to=I(qint32(quint32(endOffset)*45u))/I(256);
                start=D(sign>0?from+E(start):E(start)-from);end=D(sign>0?to+E(end):E(end)-to);
            }
            octants(r,start,end,sign>0,count==8&&!endOffset&&(!fraction||!need(i+1)));i+=fraction?5:2;break;}
        case 12:{
            // A bulge: the arc's height over the chord in 1/127 of half the chord.
            const int dx=need(i+1),dy=need(i+2),bulge=need(i+3);
            next={D(I(dx)*E(scale)+E(pos.x)),D(E(pos.y)-I(dy)*E(scale))};
            const Point middle{D(I(dx)/I(2)*E(scale)+E(pos.x)),D(E(pos.y)-I(dy)/I(2)*E(scale))};
            const double bs=D(I(bulge)*E(scale));
            const Extended ex=E(pos.x)-E(next.x),ey=E(pos.y)-E(next.y);
            const double chord=D((ey*ey+ex*ex).sqrt()),h=D(E(bs)/I(254)*E(chord));
            if(bulge==0){line();i+=3;break;}
            const Extended half=E(chord)/I(2),height=E(h);
            const double radius=D((height*height+half*half)/(I(2)*height));
            const Extended a=E(D(E(direction(pos,next))-halfPi)),inset=E(radius)-height;
            const Point centre{D(E(middle.x)-a.cos()*inset),D(E(middle.y)-a.sin()*inset)};
            arc(centre,radius,bs>0);i+=3;break;}
        case 13:{
            // Bulges up to (0,0): each one is turned into a code 12 and the rest into a new code 13.
            const int dx=need(i+1),dy=need(i+2);
            if(dx==0&&dy==0){i+=2;break;}
            list.insert(i+1,ShapeNumber{12,12});if(i+5>list.size())throw Failure();list.insert(i+5,ShapeNumber{13,13});break;}
        case 14:{
            // Only for vertical text: the next command is skipped, codes 9, 13 and 14 not.
            static const int skip[15]={1,1,1,2,2,1,1,2,3,0,3,6,4,0,0};
            const int n=need(i+1);i+=quint32(n)>14?1:skip[n];break;}
        default:{
            // A vector: length and one of 16 directions; the slanted ones run a half step (tan 22.5°) aside.
            const qint32 length=sixteenths(c);const double l=double(length);
            const qint32 to=qint32(qint64(c)-(E(l)*I(16)).rounded());
            const Extended L=E(D(E(l)*E(scale))),Lk=L*E(k),x=E(pos.x),y=E(pos.y);
            switch(to){
            case 0:next={D(x+L),pos.y};break;
            case 1:next={D(x+L),D(y-Lk)};break;
            case 2:next={D(x+L),D(y-L)};break;
            case 3:next={D(Lk+x),D(y-L)};break;
            case 4:next={pos.x,D(y-L)};break;
            case 5:next={D(x-Lk),D(y-L)};break;
            case 6:next={D(x-L),D(y-L)};break;
            case 7:next={D(x-L),D(y-Lk)};break;
            case 8:next={D(x-L),pos.y};break;
            case 9:next={D(x-L),D(Lk+y)};break;
            case 10:next={D(x-L),D(y+L)};break;
            case 11:next={D(x-Lk),D(y+L)};break;
            case 12:next={pos.x,D(y+L)};break;
            case 13:next={D(Lk+x),D(y+L)};break;
            case 14:next={D(x+L),D(y+L)};break;
            case 15:next={D(x+L),D(Lk+y)};break;
            default:break;   // a negative vector: on to where the last step went
            }
            line();break;}
        }
    }
}
}

ShapeNumbers shpNumbers(const QByteArray &source){
    // Header lines (longer than four characters) in file order with the number they give and the lines after them.
    struct Header {std::optional<int> number;QByteArray body;};
    auto headers=std::make_shared<QList<Header>>();
    QByteArray text=source;if(const qsizetype end=text.indexOf('\x1a');end>=0)text.truncate(end);
    QList<QByteArray> lines=text.split('\n');for(QByteArray &l:lines)if(l.endsWith('\r'))l.chop(1);
    for(qsizetype i=0;i<lines.size();i++){
        const QByteArray &l=lines[i];if(l.size()<=4||l[0]!='*')continue;
        const qsizetype comma=l.indexOf(',');Header h{number(comma>=1?l.mid(1,comma-1):QByteArray()),{}};
        for(qsizetype k=i+1;k<lines.size()&&!(lines[k].startsWith('*'));k++)h.body+=lines[k];
        headers->append(h);
    }
    return [headers](int shape)->std::optional<QList<ShapeNumber>>{
        // The first header with the number wins; one the original cannot read stops it on its way.
        for(const Header &h:*headers){
            if(!h.number)return std::nullopt;
            if(*h.number!=shape)continue;
            QList<ShapeNumber> out;if(h.body.isEmpty())return out;
            for(const QByteArray &token:h.body.split(',')){const auto v=number(token);if(!v)return std::nullopt;out<<ShapeNumber{*v,autocadNumber(token).value_or(*v)};}
            return out;
        }
        return QList<ShapeNumber>();
    };
}

ShapeNumbers shxNumbers(const StrokeFont &font){
    auto table=std::make_shared<QHash<int,QList<ShapeNumber>>>();
    for(auto it=font.shapes.cbegin();it!=font.shapes.cend();++it){
        if(it.key()<0||it.key()>255)continue;
        const QByteArray &s=*it;QList<ShapeNumber> out;
        auto u=[&](qsizetype k){return k<s.size()?int(quint8(s[k])):0;};
        auto sg=[&](qsizetype k){return k<s.size()?int(qint8(s[k])):0;};
        auto octant=[&](qsizetype k){const int b=u(k);return b&0x80?-(b&0x7f):b;};
        auto put=[&](int v){out<<ShapeNumber{v,v};};
        for(qsizetype i=0;i<s.size();){
            const int c=u(i++);put(c);
            switch(c){
            case 3:case 4:case 7:put(u(i));i++;break;
            case 8:put(sg(i));put(sg(i+1));i+=2;break;
            case 9:while(i+1<s.size()){const int dx=sg(i),dy=sg(i+1);put(dx);put(dy);i+=2;if(!dx&&!dy)break;}break;
            case 10:put(u(i));put(octant(i+1));i+=2;break;
            case 11:put(u(i));put(u(i+1));put(u(i+2));put(u(i+3));put(octant(i+4));i+=5;break;
            case 12:put(sg(i));put(sg(i+1));put(sg(i+2));i+=3;break;
            case 13:while(i+1<s.size()){const int dx=sg(i),dy=sg(i+1);put(dx);put(dy);i+=2;if(!dx&&!dy)break;put(sg(i));i++;}break;
            default:break;
            }
        }
        table->insert(it.key(),out);
    }
    return [table](int shape)->std::optional<QList<ShapeNumber>>{return table->value(shape);};
}

std::optional<QHash<int,StrokeGlyph>> shapeLetters(const ShapeNumbers &numbers,bool dosOrder){
    // The half step of the slanted vector directions: tan(pi / 8), as a double.
    static const double k=Extended::fromBytes(QByteArray::fromHex("35c26821a2da0fc9fd3f").constData()).tan().toDouble();
    // The original's FHX files were computed with the unit set to 53 bits of precision (Windows' setting), unlike its
    // plotting, which runs at the 64 bits the program sets.
    const Precision precision(53);QHash<int,StrokeGlyph> out;
    try{for(int code=1;code<=255;code++)out.insert(code,letter(numbers,frontpanel::shapeNumberFor(code,dosOrder),k));}catch(const Failure&){return std::nullopt;}
    return out;
}

const QHash<int,StrokeGlyph> *originalLetters(const StrokeFont &font){
    auto drawn=[](const QHash<int,StrokeGlyph> &glyphs){return std::any_of(glyphs.cbegin(),glyphs.cend(),[](const StrokeGlyph &g){return !g.elements.isEmpty();});};
    if(drawn(font.glyphs))return &font.glyphs;
    if(font.unicode||font.bigfont||(font.source.isEmpty()&&font.shapes.isEmpty()))return nullptr;
    if(!font.letters){
        const auto made=shapeLetters(font.source.isEmpty()?shxNumbers(font):shpNumbers(font.source),font.dosOrder);
        font.letters=std::make_shared<const QHash<int,StrokeGlyph>>(made&&drawn(*made)?*made:QHash<int,StrokeGlyph>());
    }
    return font.letters->isEmpty()?nullptr:font.letters.get();
}
}
