#include "strokefont.h"
#include <QLineF>
#include <QRectF>
#include <QTransform>
#include <QHash>
#include <cmath>
#include <numbers>
#include <tuple>

// The module's own single-line font, drawn after the rules of technical lettering: capital height 10, x-height 7,
// descenders 3, straight lines and circular or elliptical arcs; round letters are two half circles joined by straight
// sides. Every letter is given by its width and its strokes in a small notation (y upwards, base line at 0):
//   M x,y         lift the pen and set it down at x,y
//   L x,y         a line to x,y
//   A cx,cy,rx,ry,a0,a1   an arc of the ellipse around cx,cy from angle a0 to a1 (degrees, counter-clockwise when a1 > a0);
//                 it continues the stroke when it starts where the pen is, else it starts a new one
//   D x,y         a dot (a small circle)
// Letters with accents, superscripts, fractions and ligatures are put together from these.
namespace openloch::frontpanel {
namespace {
constexpr double spacing=2,dotRadius=0.35;
struct Letter {char32_t code;double width;const char *strokes;};
const Letter letters[]={
    {U' ',3,""},
    {U'!',0,"M0,10 L0,3 D0,0.4"},
    {U'"',2,"M0,10 L0,7.5 M2,10 L2,7.5"},
    {U'#',6.3,"M1.5,0 L2.5,10 M4,0 L5,10 M0,3 L6,3 M0.3,7 L6.3,7"},
    {U'$',6,"A3,7.5,3,2.5,20,270 A3,2.5,3,2.5,90,-160 M3,11 L3,-1"},
    {U'%',6,"M0,0 L6,10 A1.3,8.6,1.3,1.3,0,360 A4.7,1.4,1.3,1.3,0,360"},
    {U'&',6.5,"M6.5,0 L1.6,6.6 A2.6,8.2,1.8,1.8,238,-50 L1.2,3.6 A2.6,2.3,2.3,2.3,145,325 L6.5,4.2"},
    {U'\'',0,"M0,10 L0,7.5"},
    {U'(',2.25,"A4.5,5,4.5,6,120,240"},
    {U')',2.25,"A-2.25,5,4.5,6,60,-60"},
    {U'*',5,"M2.5,10 L2.5,4 M0,8.5 L5,5.5 M0,5.5 L5,8.5"},
    {U'+',6,"M0,5 L6,5 M3,2 L3,8"},
    {U',',0.6,"M0.6,0.6 L0,-1.6"},
    {U'-',4,"M0,4 L4,4"},
    {U'.',0,"D0,0.4"},
    {U'/',5,"M0,0 L5,10"},
    {U'0',5,"M5,7.5 A2.5,7.5,2.5,2.5,0,180 L0,2.5 A2.5,2.5,2.5,2.5,180,360 L5,7.5"},
    {U'1',5,"M1,8 L3.5,10 L3.5,0"},
    {U'2',5,"A2.5,7.5,2.5,2.5,160,-40 L0,0 L5,0"},
    {U'3',5,"A2.5,7.6,2.4,2.4,150,-90 A2.5,2.6,2.6,2.6,90,-150"},
    {U'4',5.5,"M4,0 L4,10 L0,3 L5.5,3"},
    {U'5',5.5,"M4.8,10 L1,10 L1,5.598 A2.5,3,3,3,120,-150"},
    {U'6',5,"A2.5,7.5,2.5,2.5,40,180 L0,2.5 A2.5,2.5,2.5,2.5,180,360 L5,3 A2.5,3,2.5,2.5,0,180"},
    {U'7',5,"M0,10 L5,10 L1.5,0"},
    {U'8',5,"A2.5,7.6,2.4,2.4,-90,270 A2.5,2.6,2.6,2.6,90,450"},
    {U'9',5,"A2.5,2.5,2.5,2.5,220,360 L5,7.5 A2.5,7.5,2.5,2.5,0,180 L0,7 A2.5,7,2.5,2.5,180,360"},
    {U':',0,"D0,0.4 D0,6"},
    {U';',0.6,"M0.6,0.6 L0,-1.6 D0.6,6"},
    {U'<',5,"M5,8.5 L0,5 L5,1.5"},
    {U'=',5,"M0,3.5 L5,3.5 M0,6.5 L5,6.5"},
    {U'>',5,"M0,8.5 L5,5 L0,1.5"},
    {U'?',5,"A2.5,7.5,2.5,2.5,160,-40 L2.5,4 L2.5,2.6 D2.5,0.4"},
    {U'@',8,"A4,4.5,1.8,1.8,0,360 M5.8,6.3 L5.8,3.2 A6.8,3.2,1,1,180,360 A4,4.5,4.016,4.016,341.11,660"},
    {U'A',6,"M0,0 L3,10 L6,0 M1,3.333 L5,3.333"},
    {U'B',6,"M0,5 L3.5,5 A3.5,2.5,2.5,2.5,90,-90 L0,0 L0,10 L3,10 A3,7.5,2.5,2.5,90,-90 L0,5"},
    {U'C',6,"A3,7,3,3,30,180 L0,3 A3,3,3,3,180,330"},
    {U'D',6,"M0,0 L0,10 L3,10 A3,7,3,3,90,0 L6,3 A3,3,3,3,0,-90 L0,0"},
    {U'E',5,"M5,10 L0,10 L0,0 L5,0 M0,5 L4,5"},
    {U'F',5,"M5,10 L0,10 L0,0 M0,5 L4,5"},
    {U'G',6,"A3,7,3,3,30,180 L0,3 A3,3,3,3,180,360 L6,5 L3.5,5"},
    {U'H',6,"M0,0 L0,10 M6,0 L6,10 M0,5 L6,5"},
    {U'I',0,"M0,0 L0,10"},
    {U'J',5,"M5,10 L5,2.5 A2.5,2.5,2.5,2.5,0,-180 L0,3.5"},
    {U'K',6,"M0,0 L0,10 M6,10 L0,4 M2,6 L6,0"},
    {U'L',5,"M0,10 L0,0 L5,0"},
    {U'M',7,"M0,0 L0,10 L3.5,3 L7,10 L7,0"},
    {U'N',6,"M0,0 L0,10 L6,0 L6,10"},
    {U'O',6,"M6,7 A3,7,3,3,0,180 L0,3 A3,3,3,3,180,360 L6,7"},
    {U'P',6,"M0,0 L0,10 L3.5,10 A3.5,7.5,2.5,2.5,90,-90 L0,5"},
    {U'Q',6,"M6,7 A3,7,3,3,0,180 L0,3 A3,3,3,3,180,360 L6,7 M3.5,2.5 L6.5,-0.5"},
    {U'R',6,"M0,0 L0,10 L3.5,10 A3.5,7.5,2.5,2.5,90,-90 L0,5 M3,5 L6,0"},
    {U'S',6,"A3,7.5,3,2.5,20,270 A3,2.5,3,2.5,90,-160"},
    {U'T',6,"M0,10 L6,10 M3,10 L3,0"},
    {U'U',6,"M0,10 L0,3 A3,3,3,3,180,360 L6,10"},
    {U'V',6,"M0,10 L3,0 L6,10"},
    {U'W',8,"M0,10 L2,0 L4,8 L6,0 L8,10"},
    {U'X',6,"M0,0 L6,10 M0,10 L6,0"},
    {U'Y',6,"M0,10 L3,5 L6,10 M3,5 L3,0"},
    {U'Z',6,"M0,10 L6,10 L0,0 L6,0"},
    {U'[',2.5,"M2.5,11 L0,11 L0,-1 L2.5,-1"},
    {U'\\',5,"M0,10 L5,0"},
    {U']',2.5,"M0,11 L2.5,11 L2.5,-1 L0,-1"},
    {U'^',5,"M0,7 L2.5,10 L5,7"},
    {U'_',6,"M0,-1.5 L6,-1.5"},
    {U'`',1.5,"M0,10 L1.5,8.5"},
    {U'a',5,"M5,7 L5,0 M5,4.5 A2.5,4.5,2.5,2.5,0,180 L0,2.5 A2.5,2.5,2.5,2.5,180,360"},
    {U'b',5,"M0,10 L0,0 M0,4.5 A2.5,4.5,2.5,2.5,180,0 L5,2.5 A2.5,2.5,2.5,2.5,0,-180"},
    {U'c',5,"A2.5,4.5,2.5,2.5,30,180 L0,2.5 A2.5,2.5,2.5,2.5,180,330"},
    {U'd',5,"M5,10 L5,0 M5,4.5 A2.5,4.5,2.5,2.5,0,180 L0,2.5 A2.5,2.5,2.5,2.5,180,360"},
    {U'e',5,"M0,3.5 L5,3.5 L5,4.5 A2.5,4.5,2.5,2.5,0,180 L0,2.5 A2.5,2.5,2.5,2.5,180,330"},
    {U'f',4,"M1.5,0 L1.5,8.5 A3,8.5,1.5,1.5,180,45 M0,7 L3.5,7"},
    {U'g',5,"M5,7 L5,-0.5 A2.5,-0.5,2.5,2.5,0,-150 M5,4.5 A2.5,4.5,2.5,2.5,0,180 L0,2.5 A2.5,2.5,2.5,2.5,180,360"},
    {U'h',5,"M0,10 L0,0 M0,4.5 A2.5,4.5,2.5,2.5,180,0 L5,0"},
    {U'i',0,"M0,0 L0,7 D0,9.5"},
    {U'j',3,"M3,7 L3,-1.5 A1.5,-1.5,1.5,1.5,0,-180 D3,9.5"},
    {U'k',5,"M0,10 L0,0 M5,7 L0,2.5 M2,4.3 L5,0"},
    {U'l',0,"M0,10 L0,0"},
    {U'm',8,"M0,0 L0,7 M0,5 A2,5,2,2,180,0 L4,0 M4,5 A6,5,2,2,180,0 L8,0"},
    {U'n',5,"M0,0 L0,7 M0,4.5 A2.5,4.5,2.5,2.5,180,0 L5,0"},
    {U'o',5,"M5,4.5 A2.5,4.5,2.5,2.5,0,180 L0,2.5 A2.5,2.5,2.5,2.5,180,360 L5,4.5"},
    {U'p',5,"M0,7 L0,-3 M0,4.5 A2.5,4.5,2.5,2.5,180,0 L5,2.5 A2.5,2.5,2.5,2.5,0,-180"},
    {U'q',5,"M5,7 L5,-3 M5,4.5 A2.5,4.5,2.5,2.5,0,180 L0,2.5 A2.5,2.5,2.5,2.5,180,360"},
    {U'r',4.3,"M0,0 L0,7 M0,4.5 A2.5,4.5,2.5,2.5,180,45"},
    {U's',5,"A2.5,5.25,2.5,1.75,20,270 A2.5,1.75,2.5,1.75,90,-160"},
    {U't',4,"M1.5,9 L1.5,1.5 A3,1.5,1.5,1.5,180,300 M0,7 L3.5,7"},
    {U'u',5,"M5,7 L5,0 M0,7 L0,2.5 A2.5,2.5,2.5,2.5,180,360"},
    {U'v',5,"M0,7 L2.5,0 L5,7"},
    {U'w',7,"M0,7 L1.75,0 L3.5,5 L5.25,0 L7,7"},
    {U'x',5,"M0,0 L5,7 M0,7 L5,0"},
    {U'y',5,"M0,7 L2.5,0 M5,7 L1.43,-3"},
    {U'z',5,"M0,7 L5,7 L0,0 L5,0"},
    {U'{',3,"M3,11 L2,10.5 L1.8,6 L0.5,5 L1.8,4 L2,-0.5 L3,-1"},
    {U'|',0,"M0,11 L0,-1"},
    {U'}',3,"M0,11 L1,10.5 L1.2,6 L2.5,5 L1.2,4 L1,-0.5 L0,-1"},
    {U'~',5,"A1.25,4.5,1.25,1,180,0 A3.75,4.5,1.25,1,180,360"},
    {0x00A2,5,"A2.5,4.5,2.5,2.5,30,180 L0,2.5 A2.5,2.5,2.5,2.5,180,330 M2.5,8.5 L2.5,-1.5"},   // ¢
    {0x00A3,5.5,"M0,0 L5.5,0 M1.5,0 L1.5,7 A3.5,7,2,2,180,30 M0,5 L4,5"},                    // £
    {0x00A5,6,"M0,10 L3,5 L6,10 M3,5 L3,0 M1,4 L5,4 M1,2 L5,2"},                               // ¥
    {0x00A7,5,"A2.5,8.25,2.5,1.75,20,270 A2.5,4.75,2.5,1.75,90,-160 A2.5,5.25,2.5,1.75,20,270 A2.5,1.75,2.5,1.75,90,-160"},   // §
    {0x00A9,8,"A4,5,4,4,0,360 A4,5,2,2,35,325"},                                                 // ©
    {0x00AB,4.5,"M2,6.5 L0,4.5 L2,2.5 M4.5,6.5 L2.5,4.5 L4.5,2.5"},                             // «
    {0x00AE,8,"A4,5,4,4,0,360 M2.8,2.5 L2.8,7.5 L4.5,7.5 A4.5,6.5,1,1,90,-90 L2.8,5.5 M4.2,5.5 L5.2,2.5"},   // ®
    {0x00B0,3,"A1.5,8.5,1.5,1.5,0,360"},                                                        // °
    {0x00B1,5,"M0,6 L5,6 M2.5,3.5 L2.5,8.5 M0,1 L5,1"},                                         // ±
    {0x00B5,5,"M5,7 L5,0 M0,-3 L0,7 M0,2.5 A2.5,2.5,2.5,2.5,180,360"},                          // µ
    {0x00B7,0,"D0,4"},                                                                           // ·
    {0x00BB,4.5,"M0,6.5 L2,4.5 L0,2.5 M2.5,6.5 L4.5,4.5 L2.5,2.5"},                             // »
    {0x00C6,9.5,"M0,0 L4.5,10 L9.5,10 M4.5,10 L4.5,0 L9.5,0 M4.5,5 L8.5,5 M1.5,3.333 L4.5,3.333"},   // Æ
    {0x00D7,5,"M0,1.5 L5,6.5 M0,6.5 L5,1.5"},                                                   // ×
    {0x00DF,5,"M0,0 L0,7.75 A2.25,7.75,2.25,2.25,180,-50 L1.99,5.17 A2.4,2.6,2.6,2.6,99,-145"},   // ß
    {0x00F7,5,"M0,4 L5,4 D2.5,6.5 D2.5,1.5"},                                                   // ÷
    {0x0131,0,"M0,0 L0,7"},                                                                      // ı (base of accented i)
    {0x0394,6,"M0,0 L3,10 L6,0 L0,0"},                                                           // Δ
    {0x03A9,6,"M0,0 L2,0 L1.732,1.422 A3,5.5,3,4.5,245,-65 L4,0 L6,0"},                          // Ω
    {0x03C0,6,"M0,7 L6,7 M1.5,7 L1.5,0 M4.5,7 L4.5,0"},                                         // π
    {0x2013,5,"M0,4 L5,4"},                                                                      // –
    {0x2014,9,"M0,4 L9,4"},                                                                      // —
    {0x2018,0.6,"M0,10 L0.6,8"},                                                                 // ‘
    {0x2019,0.6,"M0.6,10 L0,8"},                                                                 // ’
    {0x201A,0.6,"M0.6,0.6 L0,-1.4"},                                                             // ‚
    {0x201C,2.6,"M0,10 L0.6,8 M2,10 L2.6,8"},                                                    // “
    {0x201D,2.6,"M0.6,10 L0,8 M2.6,10 L2,8"},                                                    // ”
    {0x201E,2.6,"M0.6,0.6 L0,-1.4 M2.6,0.6 L2,-1.4"},                                            // „
    {0x2022,2,"A1,4.5,1,1,0,360"},                                                               // •
    {0x2026,6,"D0,0.4 D3,0.4 D6,0.4"},                                                           // …
    {0x20AC,6.5,"A4,7,3,3,40,180 L1,3 A4,3,3,3,180,320 M0,6 L4.5,6 M0,4 L4.5,4"},                // €
    {0x2190,8,"M8,5 L0,5 M2.5,7 L0,5 L2.5,3"},                                                  // ←
    {0x2191,4,"M2,0 L2,10 M0,7.5 L2,10 L4,7.5"},                                                // ↑
    {0x2192,8,"M0,5 L8,5 M5.5,7 L8,5 L5.5,3"},                                                  // →
    {0x2193,4,"M2,10 L2,0 M0,2.5 L2,0 L4,2.5"},                                                 // ↓
    {0x221A,6,"M0,4 L1.5,5 L3,0 L6,11"},                                                         // √
    {0x221E,9,"A2.25,4.5,2.25,1.75,0,360 A6.75,4.5,2.25,1.75,180,540"},                          // ∞
    {0x2248,5,"A1.25,3.5,1.25,1,180,0 A3.75,3.5,1.25,1,180,360 A1.25,6,1.25,1,180,0 A3.75,6,1.25,1,180,360"},   // ≈
    {0x2264,5,"M5,9 L0,6 L5,3 M0,1 L5,1"},                                                      // ≤
    {0x2265,5,"M0,9 L5,6 L0,3 M0,1 L5,1"},                                                      // ≥
    {0x2300,7,"A3.5,5,3,3,0,360 M0,1 L7,9"},                                                     // ⌀
};

// The strokes of the notation, read into a path.
QPainterPath strokes(const char *text){
    QPainterPath path;bool open=false;
    auto start=[&](QPointF p){
        if(open&&QLineF(path.currentPosition(),p).length()<0.05){if(path.currentPosition()!=p)path.lineTo(p);return;}
        path.moveTo(p);open=true;
    };
    for(const QString &token:QString::fromLatin1(text).split(' ',Qt::SkipEmptyParts)){
        const QChar command=token[0];QList<double> v;for(const QString &n:token.mid(1).split(','))v<<n.toDouble();
        if(command=='M'&&v.size()==2){path.moveTo(v[0],v[1]);open=true;}
        else if(command=='L'&&v.size()==2&&open)path.lineTo(v[0],v[1]);
        else if(command=='A'&&v.size()==6){
            // Angles are counted with y upwards; QPainterPath counts them with y downwards, hence the negated angles.
            const double cx=v[0],cy=v[1],rx=v[2],ry=v[3],a0=v[4],a1=v[5];const double r0=a0*std::numbers::pi/180;
            start(QPointF(cx+rx*std::cos(r0),cy+ry*std::sin(r0)));path.arcTo(QRectF(cx-rx,cy-ry,2*rx,2*ry),-a0,-(a1-a0));
        }
        else if(command=='D'&&v.size()==2){path.moveTo(v[0]+dotRadius,v[1]);path.arcTo(QRectF(v[0]-dotRadius,v[1]-dotRadius,2*dotRadius,2*dotRadius),0,360);open=false;}
    }
    return path;
}
QPainterPath line(QPointF a,QPointF b){QPainterPath p;p.moveTo(a);p.lineTo(b);return p;}
QPainterPath dot(QPointF c){QPainterPath p;p.moveTo(c.x()+dotRadius,c.y());p.arcTo(QRectF(c.x()-dotRadius,c.y()-dotRadius,2*dotRadius,2*dotRadius),0,360);return p;}
// Accents above a letter whose top is at `top`, centred on `c`; over capitals they sit closer, to stay low.
QPainterPath accent(char kind,double c,double top){
    const double y=top+(top>8?0.9:1.5);QPainterPath p;
    switch(kind){
    case 'a':return line({c-0.75,y},{c+0.75,y+1.8});                                     // acute
    case 'g':return line({c-0.75,y+1.8},{c+0.75,y});                                     // grave
    case 'c':p.moveTo(c-1.5,y);p.lineTo(c,y+1.5);p.lineTo(c+1.5,y);return p;              // circumflex
    case 'h':p.moveTo(c-1.5,y+1.5);p.lineTo(c,y);p.lineTo(c+1.5,y+1.5);return p;          // caron
    case 'd':p.addPath(dot({c-1.2,y+0.6}));p.addPath(dot({c+1.2,y+0.6}));return p;        // diaeresis
    case 'r':p.addPath(strokes(QString("A%1,%2,0.8,0.8,0,360").arg(c).arg(y+0.9).toLatin1().constData()));return p;   // ring
    case 't':p.addPath(strokes(QString("A%1,%2,0.75,0.6,180,0 A%3,%2,0.75,0.6,180,360").arg(c-0.75).arg(y+0.7).arg(c+0.75).toLatin1().constData()));return p;   // tilde
    default:return p;
    }
}
}

const StrokeFont &ownStrokeFont(){
    static const StrokeFont font=[]{
        StrokeFont f;f.name=QStringLiteral("Normschrift");f.unicode=true;f.above=10;f.below=3;
        QHash<char32_t,double> width;
        for(const Letter &l:letters){f.glyphs.insert(int(l.code),StrokeGlyph{strokes(l.strokes),l.width+spacing});width.insert(l.code,l.width);}
        // Letters with marks: the base letter, the mark centred over it (or the cedilla below, the slash through it).
        const struct {char32_t code,base;char mark;} marked[]={
            {0xC0,'A','g'},{0xC1,'A','a'},{0xC2,'A','c'},{0xC3,'A','t'},{0xC4,'A','d'},{0xC5,'A','r'},{0xC7,'C',','},{0xC8,'E','g'},{0xC9,'E','a'},
            {0xCA,'E','c'},{0xCB,'E','d'},{0xCC,'I','g'},{0xCD,'I','a'},{0xCE,'I','c'},{0xCF,'I','d'},{0xD1,'N','t'},{0xD2,'O','g'},{0xD3,'O','a'},
            {0xD4,'O','c'},{0xD5,'O','t'},{0xD6,'O','d'},{0xD8,'O','/'},{0xD9,'U','g'},{0xDA,'U','a'},{0xDB,'U','c'},{0xDC,'U','d'},{0xDD,'Y','a'},
            {0xE0,'a','g'},{0xE1,'a','a'},{0xE2,'a','c'},{0xE3,'a','t'},{0xE4,'a','d'},{0xE5,'a','r'},{0xE7,'c',','},{0xE8,'e','g'},{0xE9,'e','a'},
            {0xEA,'e','c'},{0xEB,'e','d'},{0xEC,0x131,'g'},{0xED,0x131,'a'},{0xEE,0x131,'c'},{0xEF,0x131,'d'},{0xF1,'n','t'},{0xF2,'o','g'},{0xF3,'o','a'},
            {0xF4,'o','c'},{0xF5,'o','t'},{0xF6,'o','d'},{0xF8,'o','/'},{0xF9,'u','g'},{0xFA,'u','a'},{0xFB,'u','c'},{0xFC,'u','d'},{0xFD,'y','a'},
            {0xFF,'y','d'},{0x160,'S','h'},{0x161,'s','h'},{0x178,'Y','d'},{0x17D,'Z','h'},{0x17E,'z','h'}};
        for(const auto &m:marked){
            const StrokeGlyph base=f.glyphs.value(int(m.base));const double w=width.value(m.base);const bool capital=QChar(char16_t(m.base)).isUpper();
            QPainterPath p=base.path;double advance=base.advance;
            if(m.mark==',')p.addPath(strokes(QString("M%1,0 L%1,-1 A%2,-1,0.7,0.7,0,-150").arg(w/2).arg(w/2-0.7).toLatin1().constData()));
            else if(m.mark=='/')p.addPath(line({-0.5,-0.5},{w+0.5,capital?10.5:7.5}));
            else{
                // An accent wider than a narrow letter (the i) moves letter and accent to the right and widens the step.
                p.addPath(accent(m.mark,w/2,capital?10:7));const double over=-p.boundingRect().left();
                if(over>0){p.translate(over,0);advance+=2*over;}
            }
            f.glyphs.insert(int(m.code),StrokeGlyph{p,advance});
        }
        // Superscript digits and fractions from smaller digits.
        auto small=[&](char32_t digit,double scale,QPointF at){return QTransform::fromScale(scale,scale).map(f.glyphs.value(int(digit)).path).translated(at);};
        f.glyphs.insert(0xB9,StrokeGlyph{small('1',0.55,{0,5.5}),5*0.55+spacing});
        f.glyphs.insert(0xB2,StrokeGlyph{small('2',0.55,{0,5.5}),5*0.55+spacing});
        f.glyphs.insert(0xB3,StrokeGlyph{small('3',0.55,{0,5.5}),5*0.55+spacing});
        for(const auto &[code,top,bottom]:{std::tuple<int,char32_t,char32_t>{0xBD,'1','2'},{0xBC,'1','4'},{0xBE,'3','4'}}){
            QPainterPath p=small(top,0.5,{0,5});p.addPath(line({0.5,0},{6.5,10}));p.addPath(small(bottom,0.5,{4.5,0}));f.glyphs.insert(code,StrokeGlyph{p,7.25+spacing});
        }
        // Inverted marks, ligatures and the Greek mu.
        auto turned=[&](char32_t code,double w,double middle){return QTransform(-1,0,0,-1,w,2*middle).map(f.glyphs.value(int(code)).path);};
        f.glyphs.insert(0xA1,StrokeGlyph{turned('!',0,4.25),spacing});
        f.glyphs.insert(0xBF,StrokeGlyph{turned('?',5,4.25),5+spacing});
        auto joined=[&](char32_t a,char32_t b,double offset){QPainterPath p=f.glyphs.value(int(a)).path;p.addPath(f.glyphs.value(int(b)).path.translated(offset,0));return p;};
        f.glyphs.insert(0xE6,StrokeGlyph{joined('a','e',5),10+spacing});
        f.glyphs.insert(0x152,StrokeGlyph{joined('O','E',6),11+spacing});
        f.glyphs.insert(0x153,StrokeGlyph{joined('o','e',5),10+spacing});
        f.glyphs.insert(0x3BC,f.glyphs.value(0xB5));
        f.glyphs.insert(0x2126,f.glyphs.value(0x3A9));
        f.glyphs.insert(0x2205,f.glyphs.value(0x2300));
        return f;
    }();
    return font;
}
}
