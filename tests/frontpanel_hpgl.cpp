// Tests of the HPGL outlines as the original builds them: its x87 arithmetic, its outline builders (B-spline pieces,
// chamfers, rounded corners, arcs), the values of objects read from FPL files, and the plot file it writes. The
// expected values were computed independently with exact fractions.
#include "delphistream.h"
#include "fdplot.h"
#include "fdshapes.h"
#include "fpl.h"
#include "frontpanel.h"
#include "panelgeometry.h"
#include "panelmachining.h"
#include "strokefont.h"
#include "x87.h"
#include <QJsonObject>
#include <QLineF>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <utility>

using namespace openloch::frontpanel;
using namespace openloch::frontdesigner;

namespace fptest {
void require(bool ok,const char *message);
bool near(double a,double b,double tolerance);
}
using fptest::require;using fptest::near;

namespace {
Extended ext(const char *hex){const QByteArray b=QByteArray::fromHex(hex);return Extended::fromBytes(b.constData());}
QByteArray hexOf(const Extended &e){return e.bytes().toHex();}
QString text(const QPolygon &p){QStringList out;for(const QPoint &q:p)out<<QString("%1,%2").arg(q.x()).arg(q.y());return out.join(' ');}
PlotShape shape(const QList<QPointF> &points,bool closed,bool smooth,int style,double size){
    PlotShape s;for(QPointF p:points)s.points<<ExtendedPoint{Extended::fromDouble(p.x()),Extended::fromDouble(p.y())};
    s.closed=closed;s.smooth=smooth;s.cornerStyle=style;s.cornerSize=Extended::fromDouble(size);return s;
}

void arithmetic(){
    // a, b, a + b, a - b, a * b, a / b, sqrt |a| as the ten bytes of the files.
    const char *vectors[][7]={
        {"0000000000000080ff3f","00000000000000c00040","00000000000000800140","000000000000008000c0","00000000000000c00040","abaaaaaaaaaaaaaafd3f","0000000000000080ff3f"},
        {"00000000000000800040","0000000000000080bf3f","00000000000000800040","00000000000000800040","0000000000000080c03f","00000000000000804040","8464def933f304b5ff3f"},
        {"0000000000000080ff3f","00000000000000c0bf3f","0100000000000080ff3f","fefffffffffffffffe3f","00000000000000c0bf3f","abaaaaaaaaaaaaaa3e40","0000000000000080ff3f"},
        {"cdccccccccccccccfb3f","cdccccccccccccccfc3f","9a99999999999999fd3f","cdccccccccccccccfbbf","0bd7a3703d0ad7a3f93f","0000000000000080fe3f","da764842129be8a1fd3f"},
        {"00000000000080bb0b40","00000000000078bb0bc0","0000000000000080ff3f","0000000000007cbb0c40","0000000000644e8918c0","a591859a55760580ffbf","77408a8c7659eb9a0540"},
        {"be9f1a2fdd24529a0940","555555555555559500c0","13f56f84327a079a0940","694ac5d987cf9c9a0940","b30f9f6157800ab40ac0","11d21696bd68468408c0","7f1eae4deaad8b8c0440"}};
    for(const auto &v:vectors){
        const Extended a=ext(v[0]),b=ext(v[1]);
        require(hexOf(a+b)==v[2]&&hexOf(a-b)==v[3]&&hexOf(a*b)==v[4]&&hexOf(a/b)==v[5]&&hexOf(a.abs().sqrt())==v[6],"extended arithmetic rounded to nearest, ties to even");
    }
    // Delphi's Round: halves to the even number.
    for(auto [value,whole]:{std::pair{2.5,2},{3.5,4},{-2.5,-2},{0.5,0},{1.5,2},{0.4999999999999999,0},{-617.5,-618},{2.5000000000000004,3}})
        require(Extended::fromDouble(value).rounded()==whole,"rounding with halves to even");
    require(near(Extended::fromDouble(0.7).sin().toDouble(),std::sin(0.7),1e-16)&&near(Extended::fromDouble(5.9).cos().toDouble(),std::cos(5.9),1e-16),"sine and cosine");
}

void builders(){
    // An open B-spline reaches its end points; the middle of an edge and every curve point are rounded with halves to
    // even (50.5 gives 50).
    require(text(plotPolygon(shape({{0,0},{101,0},{101,101}},false,true,0,0)))=="0,0 50,0 55,0 60,1 64,1 68,2 72,3 76,5 79,6 83,8 86,10 88,13 91,15 93,18 95,21 96,25 98,28 99,32 100,36 100,41 101,45 101,50 101,101",
            "open B-spline with its 21 points");
    // Closed chamfers go round once more to the first corner before they come back to the first point.
    require(text(plotPolygon(shape({{0,0},{200,0},{200,200},{0,200}},true,true,1,25)))=="175,0 200,25 200,175 175,200 25,200 0,175 0,25 25,0 175,0 200,25 175,0","closed chamfers");
    require(text(plotPolygon(shape({{0,0},{300,0},{150,260}},true,true,2,30.5)))==
            "270,0 273,0 276,0 278,1 280,1 282,2 284,2 285,3 287,4 288,5 289,7 289,8 290,9 290,11 290,13 290,15 289,17 288,19 288,21 286,23 285,26 165,234 163,236 162,239 "
            "160,241 159,242 157,244 156,245 154,246 153,246 151,247 150,247 148,247 147,246 145,246 144,245 142,244 141,242 139,241 138,239 136,236 135,234 15,26 14,23 12,21 "
            "12,19 11,17 10,15 10,13 10,11 10,9 11,8 11,6 12,5 13,4 15,3 16,2 18,2 20,1 22,1 24,0 27,0 30,0 270,0","closed rounded corners");
    // Arcs run from the middle of the first edge to the middle of the last; a pie goes on to the centre and the first
    // support point, a chord only to that point.
    PlotShape arc=shape({{1000,500},{1100,520},{1180,580},{1230,670}},false,true,3,0);arc.arcCentre={Extended::fromInt(1000),Extended::fromInt(800)};
    const QString curve="1050,510 1055,511 1060,512 1065,513 1070,515 1074,516 1079,518 1084,519 1088,521 1093,523 1098,525 1102,527 1106,529 1111,531 1115,534 1119,536 "
                        "1124,539 1128,541 1132,544 1136,547 1140,550 1140,550 1144,553 1148,556 1152,559 1155,563 1159,566 1163,569 1166,573 1170,576 1173,580 1176,584 1179,588 "
                        "1183,591 1186,595 1189,599 1192,603 1194,608 1197,612 1200,616 1202,621 1205,625";
    arc.arcMode=1;require(text(plotPolygon(arc))==curve+" 1000,800 1000,500","pie");
    arc.arcMode=2;require(text(plotPolygon(arc))==curve+" 1000,500","chord");
    arc.arcMode=0;require(text(plotPolygon(arc))==curve,"open arc");
    require(text(plotPolygon(shape({{10.5,2.5},{30,3.5},{20,40}},true,false,1,0)))=="10,2 30,4 20,40 10,2","closed contour without corners");
    // A closed contour with the corners of arcs has no outline in the original.
    require(plotPolygon(shape({{0,0},{10,0},{10,10}},true,true,3,0)).isEmpty(),"closed arc corners");
}

// What the original would plot for an element of a document, compared with the element read back from FPL.
QString plotted(const Element &e){
    if(e.type==ElementType::Drill){const QPoint p=plotDrill(e);return QString("%1,%2 | %3").arg(p.x()).arg(p.y()).arg(text(plotMilledDrill(e,0.8)));}
    return text(plotOutline(e));
}

void elements(){
    Document d;Panel &p=d.panels[0];p.width=120.37;p.height=80.03;
    Element line=newElement(ElementType::Line);line.points={{10.01,10.03},{40.47,12.35},{30.3,40.13}};line.contour={Corners::Spline,2};
    Element poly=newElement(ElementType::Polygon);poly.points={{50,10},{80.33,12.71},{70.07,35.55},{55.55,30}};poly.contour={Corners::Chamfer,3.33};
    Element rect=newElement(ElementType::Rectangle);rect.points={{10,50},{40,50},{40,70},{10,70}};rect.contour={Corners::Round,4.11};
    Element ellipse=newElement(ElementType::Ellipse);ellipse.center={90.35,60.17};ellipse.radiusX=12.3;ellipse.radiusY=7.77;ellipse.rotation=17;
    Element pie=newElement(ElementType::Arc);pie.center={60.21,60.13};pie.radiusX=pie.radiusY=8.45;pie.startAngle=20;pie.spanAngle=135;pie.arcStyle=ArcStyle::Pie;
    Element chord=pie;chord.id=newId();chord.center={100.5,20.5};chord.arcStyle=ArcStyle::Chord;chord.spanAngle=-210;
    Element hole=newElement(ElementType::Drill);hole.center={12.35,77.77};hole.diameter=3.3;
    p.elements={line,poly,rect,ellipse,pie,chord,hole};
    // The values the writer stores are those the outlines use: read back, every element plots the same.
    const Document back=readFrontDesigner(writeFrontDesigner(d));
    for(int i=0;i<p.elements.size();i++){
        const QString a=plotted(p.elements[i]),b=plotted(back.panels[0].elements.value(i));
        require(!a.isEmpty()&&a==b,"outline of an element and of its FPL copy");
    }
    // An unchanged circle or arc plots the support points it was read with, however many the original made.
    Element read=back.panels[0].elements[4];QJsonObject fd=read.foreign["frontDesigner"].toObject();fd.remove("raw");
    QPolygonF more;for(int k=0;k<23;k++){const double a=(20+135.0*(k-0.5)/21)*3.141592653589793/180;more<<QPointF(60.21+8.5*std::cos(a),60.13-8.5*std::sin(a));}
    QByteArray values;for(QPointF q:more)values+=Extended::fromDouble(q.x()*50).bytes()+Extended::fromDouble(q.y()*50).bytes();
    fd["points"]=QString::fromLatin1(values.toBase64());read.foreign["frontDesigner"]=fd;
    require(plotOutline(read).size()==21*21+2,"stored support points of an arc");
    Document again=back;again.panels[0].elements[4]=read;
    const Document twice=readFrontDesigner(writeFrontDesigner(again));
    require(plotOutline(twice.panels[0].elements[4])==plotOutline(read),"stored support points written back");
    // A moved element plots from its new values, not from those it was read with.
    Element moved=back.panels[0].elements[3];transformElement(moved,QTransform::fromTranslate(1,0));Element plain=moved;plain.foreign={};
    require(plotOutline(moved)==plotOutline(plain)&&plotOutline(moved)!=plotOutline(back.panels[0].elements[3]),"a changed element plots from its values");
    // The outer rectangle from the bottom left corner, half the tool outside.
    require(text(plotPanelOutline(p,2))=="-50,4052 6068,4052 6068,-50 -50,-50 -50,4052","outer rectangle");
}

// A text in a font of the original's own format: an H of three lines and an O of one arc, font units with y downwards.
void texts(){
    openloch::frontdesigner::DelphiWriter w;
    auto line=[&](double x1,double y1,double x2,double y2){w.integer(0);w.real(x1);w.real(y1);w.real(x2);w.real(y2);};
    for(int code=1;code<=112;code++){
        if(code=='H'){w.integer(50);w.integer(7);w.integer(2);line(5,0,5,-100);w.integer(2);line(45,0,45,-100);w.integer(2);line(5,-50,45,-50);w.integer(2);}
        else if(code=='O'){w.integer(60);w.integer(3);w.integer(2);w.integer(1);for(double v:{10.0,-50.0,50.0,-50.0,30.0,-50.0,20.0})w.real(v);w.boolean(true);w.integer(2);}
        else if(code=='p'){w.integer(40);w.integer(3);w.integer(2);line(5,30,5,-60);w.integer(2);}
        else if(code==' '){w.integer(30);w.integer(0);}
        else{w.integer(0);w.integer(0);}
    }
    const StrokeFont font=StrokeFont::fromFhx(w.bytes);
    // Font height 260 over 100 + 30 gives a scale of 2; the advances 100 and 120 are stretched to the frame width 1000.
    StrokeText t;t.font=&font;t.height=260;t.text="HO";
    const QPointF corners[4]={{1000,1000},{2000,1000},{2000,1260},{1000,1260}};
    for(int i=0;i<4;i++)t.corners[i]={Extended::fromDouble(corners[i].x()),Extended::fromDouble(corners[i].y())};
    QList<QPolygon> strokes=plotStrokeText(t);
    require(strokes.size()==4&&text(strokes[0])=="1045,1200 1045,1000"&&text(strokes[1])=="1409,1200 1409,1000"&&text(strokes[2])=="1045,1100 1409,1100",
            "lines of a character from its origin on the base line, stretched to the frame");
    // The arc of the O (radius 40) in Round(sqrt(4 * 40) * pi / 2 pi + 6) = 12 equal steps, counter-clockwise on the
    // screen from the left to the right through the bottom; the second character starts at Round(100 * 1000 / 220).
    require(strokes[3].size()==13&&strokes[3].first()==QPoint(1546,1100)&&strokes[3][6]==QPoint(1728,1140)&&strokes[3].last()==QPoint(1910,1100),"arc in equal steps");
    // Mirrored across: the strokes run from the right edge to the left.
    std::swap(t.corners[0],t.corners[1]);std::swap(t.corners[2],t.corners[3]);t.flipX=true;
    strokes=plotStrokeText(t);require(strokes.size()==4&&text(strokes[0])=="1955,1200 1955,1000","mirrored text");
    // A character without strokes counts as the space.
    t.flipX=false;std::swap(t.corners[0],t.corners[1]);std::swap(t.corners[2],t.corners[3]);t.text="H\x01";
    require(plotStrokeText(t).size()==3,"missing character as space");
    // The same letters as a shape font in source text: the original makes the same lines and arcs of it.
    const StrokeFont shapes=StrokeFont::fromShp("*0,4,Test\r\n100,30,0,0\r\n*020,5,space\r\n2,8,(30,0),0\r\n"
        "*048,30,H\r\n2,8,(5,0),1,8,(0,100),2,8,(40,-100),1,8,(0,100),2,8,(-40,-50),1,8,(40,0),2,8,(5,-50),0\r\n"
        "*04F,13,O\r\n2,8,(10,50),1,12,(40,0,127),2,8,(10,-50),0\r\n");
    StrokeText u=t;u.font=&shapes;
    for(const char *s:{"HO","H\x01"}){t.text=u.text=s;require(!plotStrokeText(u).isEmpty()&&plotStrokeText(u)==plotStrokeText(t),"shape font plotted like the original's own format");}
}

// Shape fonts as the original turns them into lines and arcs (own shapes in the notation of .shp files; y downwards).
QString letter(const QHash<int,StrokeGlyph> &letters,int code){
    const StrokeGlyph g=letters.value(code);QStringList out;auto n=[](double v){return QString::number(v,'g',17);};
    for(const StrokeElement &e:g.elements){
        if(e.kind==StrokeElement::PenUp)out<<"U";
        else if(e.kind==StrokeElement::Line)out<<QString("L %1 %2 %3 %4").arg(n(e.x1),n(e.y1),n(e.x2),n(e.y2));
        else out<<QString("A %1 %2 %3 %4 %5 %6 %7 %8").arg(n(e.x1),n(e.y1),n(e.x2),n(e.y2),n(e.cx),n(e.cy),n(e.radius)).arg(e.counterClockwise?"+":"-");
    }
    return out.join(';')+QString(" | %1").arg(g.advance);
}
QList<int> values(const std::optional<QList<ShapeNumber>> &numbers,bool autocad=false){
    QList<int> out;if(numbers)for(const ShapeNumber &n:*numbers)out<<(autocad?n.autocad:n.value);return out;
}
void shapeFonts(){
    // Numbers as the original reads them: a leading 0 makes two hexadecimal digits, the others are decimal, also with
    // a minus sign, where AutoCAD reads hexadecimal digits.
    require(values(shpNumbers("*041,4,a\r\n0FF,(-3,4),\r\n0A\r\n")(0x41))==QList<int>{255,-3,4,10},"numbers of a shape");
    const auto octantSpec=shpNumbers("*041,4,a\r\n10,(2,-043),0\r\n")(0x41);
    require(values(octantSpec)==QList<int>{10,2,-43,0}&&values(octantSpec,true)==QList<int>{10,2,-67,0},"numbers as AutoCAD reads them");
    const QByteArray source=
        "*0,4,Test\r\n100,30,0,0\r\n"
        "*041,6,A\r\n1,014,8,(3,4),0\r\n"                                             // a vector up, a line
        "*042,14,B\r\n2,8,(1,0),1,12,(4,0,127),12,(-4,0,-127),0\r\n"                 // two half circles by bulges
        "*043,5,C\r\n1,10,(2,-67),0\r\n"                                               // an octant arc, clockwise
        "*049,5,I\r\n1,10,(2,000),0\r\n"                                               // a full circle
        "*04A,8,J\r\n1,11,(56,28,0,3,012),0\r\n"                                       // a fractional arc from 54.8° to 94.9°
        "*04B,8,K\r\n1,11,(0,147,0,6,-066),0\r\n"                                      // a fractional arc clockwise from 270° to 19.2°
        "*044,12,D\r\n7,041,5,8,(2,0),6,3,2,8,(4,0),0\r\n"                             // the A as a subshape; position kept; half scale
        "*045,23,E\r\n13,(4,0,127),(0,-2,0),(0,0),14,8,(9,9),9,(1,0),(0,1),(0,0),0\r\n" // bulges; vertical text only; vectors
        "*046,6,F\r\n3,2,2,8,(5,0),0\r\n"                                               // an advance of 2.5
        "*047,3,G\r\n1,011,0\r\n"                                                       // a slanted vector
        "*081,6,ue\r\n1,8,(1,0),0\r\n"                                                  // ü at its DOS code
        "*0100,6,x\r\n1,8,(7,0),0\r\n";                                                 // read as 010
    const auto letters=shapeLetters(shpNumbers(source));require(bool(letters),"shape font converted");
    require(letter(*letters,'A')=="L 0 0 0 -1;L 0 -1 3 -5 | 3","lines of vectors and displacements");
    require(letter(*letters,'B')=="U;A 1 0 5 0 3 0 2 +;A 5 0 1 0 3 0 2 - | 1","arcs of bulges");
    // Octant and fractional arcs as AutoCAD defines them (the original would mark them all counter-clockwise, make the
    // full circle a point and end the first fractional arc at 139.9°).
    auto arcIs=[](const StrokeElement &e,bool counterClockwise,double radius,QPointF centre,QPointF end){
        return e.kind==StrokeElement::Arc&&e.counterClockwise==counterClockwise&&near(e.radius,radius,1e-12)&&near(e.cx,centre.x(),1e-9)&&near(e.cy,centre.y(),1e-9)
            &&near(e.x2,end.x(),1e-9)&&near(e.y2,end.y(),1e-9);
    };
    require(arcIs(letters->value('C').elements.value(0),false,2,{2,0},{2+std::sqrt(2.0),-std::sqrt(2.0)}),"octant arc from 180° three octants clockwise");
    const auto circle=letters->value('I').elements;
    require(circle.size()==2&&arcIs(circle[0],true,2,{-2,0},{-4,0})&&arcIs(circle[1],true,2,{-2,0},{0,0}),"full circle as two half circles");
    require(arcIs(letters->value('J').elements.value(0),true,3,{-1.727424574253536,2.4527544394547514},{-1.9848165112868552,-0.5361833970935828}),"fractional arc");
    require(arcIs(letters->value('K').elements.value(0),false,6,{0,-6},{5.667629023568883,-7.969259061474555}),"fractional arc clockwise");
    require(letter(*letters,'D')=="L 0 0 0 -1;L 0 -1 3 -5;L 3 -5 5 -5;U;L 3 -5 5 -5 | 5","subshape, position stack and scale");
    require(letter(*letters,'E')=="A 0 0 4 0 2 0 2 +;L 4 0 4 2;L 4 2 5 2;L 5 2 5 1 | 5","bulges and vectors in a row");
    require(letter(*letters,'F')=="U | 2","advance rounded half to even");
    require(letter(*letters,'G')=="L 0 0 1 -0.41421356237309503 | 1","slanted vector a half step aside");
    require(letter(*letters,0xfc)=="L 0 0 1 0 | 1"&&letter(*letters,0x81)==" | 0","letter moved from the DOS code page");
    const auto windows=shapeLetters(shpNumbers(source),false);
    require(windows&&letter(*windows,0x81)=="L 0 0 1 0 | 1"&&letter(*windows,0xfc)==" | 0","letters in Windows order stay at their numbers");
    require(letter(*letters,16)=="L 0 0 7 0 | 7","shape number of two hexadecimal digits");
    // Text the original cannot read stops it: a header it cannot read on the way, a comment, a shape without its end.
    require(!shapeLetters(shpNumbers("*041,2,a\r\n1,0\r\n*zz,2,b\r\n1,0\r\n")),"unreadable header");
    require(!shapeLetters(shpNumbers("*041,5,a\r\n1,8,(1,2) ; up\r\n0\r\n")),"comment");
    require(!shapeLetters(shpNumbers("*041,4,a\r\n1,8,(1,2)\r\n")),"shape without end");
    // Plotted only through the original's letters when it can make them.
    require(originalLetters(StrokeFont::fromShp(source))&&!originalLetters(StrokeFont::fromShp("*041,5,a\r\n1,8,(1,2) ; up\r\n0\r\n")),"letters of a shape font");
}

void plotFile(){
    Document d;Panel &p=d.panels[0];p.width=100;p.height=50;
    Element hole=newElement(ElementType::Drill);hole.center={10,10};hole.diameter=3;p.elements={hole};
    MachiningOptions o;o.commonOrigin=true;
    // The pen goes down and up at the common origin; the next move needs no lifting.
    require(hpgl(machiningJobs(d,p,o).value(0),p,o)=="IN;\r\nSP1;\r\nPT0;\r\nPU;\r\nPA0,0;\r\nPD;\r\nPU;\r\nPA400,1600;\r\nPD;\r\nPA400,1600;\r\nPU;\r\nPA0,0;\r\n",
            "plot file with common origin");
    o.commonOrigin=false;o.outline=true;o.drills=false;
    require(hpgl(machiningJobs(d,p,o).value(0),p,o)=="IN;\r\nSP1;\r\nPT0;\r\nPU;\r\nPA-40,-40;\r\nPD;\r\nPA4040,-40;\r\nPA4040,2040;\r\nPA-40,2040;\r\nPA-40,-40;\r\nPU;\r\nPA0,0;\r\n",
            "outer rectangle with the 2 mm tool");
    // A diameter changed in the job list is whole fiftieths of a millimetre; holes milled out run on the circle.
    o.outline=false;o.drills=true;o.millDrills=true;o.tool=1;o.toolFor[hole.id]=3.014;
    const auto jobs=machiningJobs(d,p,o);
    require(jobs.size()==1&&jobs[0].kind==MachiningJob::DrillMill&&jobs[0].paths[0].points.size()==16*21,"hole milled out");
    double far=0;for(QPointF q:jobs[0].paths[0].points)far=std::max(far,QLineF(QPointF(10,10),q).length());require(near(far,1.0,0.03),"milled on the circle inside the hole");
}
}

void runHpglTests(){
    arithmetic();builders();elements();texts();shapeFonts();plotFile();
}
