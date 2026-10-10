#include "footprints.h"
#include "font.h"
#include "language.h"
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace openloch::pcb {
namespace {
constexpr double silk=.2;   // silkscreen line width
Element pad(QPointF at,double diameter,double drill,int number,PadShape shape=PadShape::Round){
    auto e=newElement(ElementType::Pad);e.pos=at;e.size=diameter;e.size2=drill;e.shape=shape;e.name=QString::number(number);updateOutline(e);return e;
}
Element smd(QPointF at,double width,double height,int number){
    auto e=newElement(ElementType::SmdPad);e.pos=at;e.size=width;e.size2=height;e.name=QString::number(number);updateOutline(e);return e;
}
Element line(const QPolygonF &points){auto e=newElement(ElementType::Track);e.layer=SilkTop;e.width=silk;e.points=points;return e;}
Element box(double x0,double y0,double x1,double y1){return line(QPolygonF(QList<QPointF>{{x0,y0},{x1,y0},{x1,y1},{x0,y1},{x0,y0}}));}
Element arc(QPointF centre,double radius,double start,double stop){
    auto e=newElement(ElementType::Circle);e.layer=SilkTop;e.pos=centre;e.size=radius;e.width=silk;e.start=start;e.stop=stop;return e;
}
Element dot(QPointF centre,double radius){auto e=arc(centre,radius/2,0,0);e.width=radius;return e;}
Element label(const QString &text,QPointF at,double height,TextRole role){
    auto e=newElement(ElementType::Text);e.layer=SilkTop;e.text=text;e.pos=at;e.size=height;e.role=role;updateStrokes(e);return e;
}
// Designator above and value below the outline `body` (its bounding box).
void labels(QList<Element> &els,const QString &prefix,const QString &value,QRectF body){
    els<<label(prefix+"?",{body.left(),body.top()-.6},1.3,TextRole::Designator)<<label(value,{body.left(),body.bottom()+1.6},1,TextRole::Value);
}
Footprint dil(int pins){
    // JEDEC MS-001 (300 mil) and MS-011 (600 mil) plastic DIP: pitch 2.54 mm; leads at most 0.56 mm wide, so 0.8 mm
    // drills in 1.6 mm pads. Pin 1 square, counted counter-clockwise from the top left.
    const bool wide=pins>=32;const double row=wide?15.24:7.62;const int rows=pins/2;Footprint f;
    f.id=QString("dil-%1").arg(pins);f.name=ui("DIL %1").arg(pins)+(wide?ui(", breit"):QString());f.prefix="IC";f.source="JEDEC MS-001 / MS-011";
    for(int k=0;k<rows;k++){const double y=(k-(rows-1)/2.0)*2.54;
        f.elements<<pad({-row/2,y},1.6,.8,k+1,k==0?PadShape::Square:PadShape::Round);}
    for(int k=0;k<rows;k++){const double y=((rows-1)/2.0-k)*2.54;f.elements<<pad({row/2,y},1.6,.8,rows+k+1);}
    const double x=row/2-1.2,top=-(rows*2.54)/2,bottom=-top;
    f.elements<<line(QPolygonF(QList<QPointF>{{-.6,top},{-x,top},{-x,bottom},{x,bottom},{x,top},{.6,top}}))<<arc({0,top},.6,180,0);
    labels(f.elements,f.prefix,"",QRectF(QPointF(-row/2-.8,top),QPointF(row/2+.8,bottom)));return f;
}
Footprint header(int pins){
    // Pin headers on the 2.54 mm grid; square posts of 0.64 mm need a 1.0 mm drill.
    Footprint f;f.id=QString("header-%1").arg(pins);f.name=ui("Stiftleiste 1×%1").arg(pins);f.prefix="J";f.source="2.54 mm pitch, 0.64 mm posts";
    const double top=-(pins-1)*2.54/2;
    for(int k=0;k<pins;k++)f.elements<<pad({0,top+k*2.54},1.7,1.0,k+1,k==0?PadShape::Square:PadShape::Round);
    f.elements<<box(-1.27,top-1.27,1.27,-top+1.27);labels(f.elements,f.prefix,"",QRectF(QPointF(-1.27,top-1.27),QPointF(1.27,-top+1.27)));return f;
}
Footprint axial(const QString &id,const QString &name,const QString &prefix,const QString &value,double pitch,double length,double diameter,bool band,const QString &source){
    // Axial parts lying flat: body drawn as a rectangle, leads as lines to the pads.
    Footprint f;f.id=id;f.name=name;f.prefix=prefix;f.source=source;
    f.elements<<pad({-pitch/2,0},1.8,.8,1,band?PadShape::Square:PadShape::Round)<<pad({pitch/2,0},1.8,.8,2);
    // A diode's pads are named after their contacts, the cathode at the band: a schematic's numbered contacts (1 the
    // anode, 2 the cathode) then find the right one.
    if(band){f.elements[0].name=QStringLiteral("K");f.elements[1].name=QStringLiteral("A");}
    const double x=length/2,y=diameter/2;
    f.elements<<box(-x,-y,x,y)<<line(QPolygonF(QList<QPointF>{{-pitch/2+1.2,0},{-x,0}}))<<line(QPolygonF(QList<QPointF>{{x,0},{pitch/2-1.2,0}}));
    if(band)f.elements<<line(QPolygonF(QList<QPointF>{{-x+.7,-y},{-x+.7,y}}));
    labels(f.elements,prefix,value,QRectF(QPointF(-pitch/2-1,-y),QPointF(pitch/2+1,y)));return f;
}
Footprint disc(double pitch){
    // Ceramic capacitors with radial leads: body about 4 × 2.5 mm (5.5 mm for 5.08 mm pitch).
    Footprint f;f.id=QString("cap-%1").arg(pitch==2.54?"254":"508");f.name=ui("Kondensator, RM %1").arg(uiLocale().toString(pitch,'f',2));f.prefix="C";f.source="radial leads, pitch as named";
    f.elements<<pad({-pitch/2,0},1.6,.8,1)<<pad({pitch/2,0},1.6,.8,2);const double x=pitch/2+1.5;
    f.elements<<box(-x,-1.25,x,1.25);labels(f.elements,f.prefix,"100n",QRectF(QPointF(-x,-1.25),QPointF(x,1.25)));return f;
}
Footprint electrolytic(double diameter,double pitch){
    // Radial electrolytic capacitors: case diameter and lead pitch as in the usual series (5 × 2.0, 6.3 × 2.5, 8 × 3.5 mm).
    Footprint f;f.id=QString("elko-%1").arg(diameter*10);f.name=ui("Elko Ø %1 mm, RM %2").arg(uiLocale().toString(diameter),uiLocale().toString(pitch));
    f.prefix="C";f.source="radial case, diameter and pitch as named";
    f.elements<<pad({-pitch/2,0},1.6,.8,1,PadShape::Square)<<pad({pitch/2,0},1.6,.8,2);const double r=diameter/2+.2;
    f.elements[0].name=QStringLiteral("+");f.elements[1].name=QStringLiteral("-");     // plus at the square pad and the sign
    f.elements<<arc({0,0},r,0,0)<<line(QPolygonF(QList<QPointF>{{-r-.9,-r*.55},{-r+.3,-r*.55}}))<<line(QPolygonF(QList<QPointF>{{-r-.3,-r*.55-.6},{-r-.3,-r*.55+.6}}));
    labels(f.elements,f.prefix,"10µ",QRectF(QPointF(-r,-r),QPointF(r,r)));return f;
}
Footprint led(double diameter){
    // Round LEDs (T-1 3 mm, T-1¾ 5 mm): leads 2.54 mm apart, the cathode at the flat side of the collar.
    Footprint f;f.id=QString("led-%1").arg(diameter);f.name=ui("LED Ø %1 mm").arg(uiLocale().toString(diameter));f.prefix="LED";f.source="T-1 / T-1¾ lamp, 2.54 mm leads";
    f.elements<<pad({-1.27,0},1.6,.8,1)<<pad({1.27,0},1.6,.8,2,PadShape::Square);
    f.elements[0].name=QStringLiteral("A");f.elements[1].name=QStringLiteral("K");     // the cathode at the flat side
    const double r=diameter/2+.4,flat=30;
    f.elements<<arc({0,0},r,flat,360-flat)<<line(QPolygonF(QList<QPointF>{{r*std::cos(qDegreesToRadians(flat)),-r*std::sin(qDegreesToRadians(flat))},{r*std::cos(qDegreesToRadians(flat)),r*std::sin(qDegreesToRadians(flat))}}));
    labels(f.elements,f.prefix,"",QRectF(QPointF(-r,-r),QPointF(r,r)));return f;
}
Footprint to92(){
    // JEDEC TO-226AA (TO-92) with the leads bent to 2.54 mm; body 4.8 mm across with the flat side at the bottom.
    Footprint f;f.id="to-92";f.name=ui("TO-92, RM 2,54");f.prefix="T";f.source="JEDEC TO-226AA";
    for(int k=0;k<3;k++)f.elements<<pad({(k-1)*2.54,0},1.5,.75,k+1);
    const double r=2.8,y=1.0,a=qRadiansToDegrees(std::asin(y/r)),x=std::sqrt(r*r-y*y);
    f.elements<<arc({0,0},r,-a,180+a)<<line(QPolygonF(QList<QPointF>{{-x,y},{x,y}}));
    labels(f.elements,f.prefix,"",QRectF(QPointF(-r,-r),QPointF(r,y)));return f;
}
Footprint to220(){
    // JEDEC TO-220AB standing: leads 2.54 mm apart, 0.9 × 0.5 mm, so 1.1 mm drills; body 10.2 × 4.6 mm with the tab at the back.
    Footprint f;f.id="to-220";f.name=ui("TO-220, stehend");f.prefix="IC";f.source="JEDEC TO-220AB";
    for(int k=0;k<3;k++)f.elements<<pad({(k-1)*2.54,0},1.8,1.1,k+1,k==0?PadShape::SquareTall:PadShape::OvalTall);
    f.elements<<box(-5.1,-3.1,5.1,1.5)<<line(QPolygonF(QList<QPointF>{{-5.1,-1.9},{5.1,-1.9}}));
    labels(f.elements,f.prefix,"",QRectF(QPointF(-5.1,-3.1),QPointF(5.1,1.5)));return f;
}
Footprint chip(const QString &size,double pitch,double padX,double padY,double bodyX,double bodyY,const QString &source){
    // Two-terminal chip parts with the nominal IPC-7351 land pattern of the size.
    Footprint f;f.id="chip-"+size;f.name=ui("Chip %1 (R, C)").arg(size);f.prefix="R";f.source=source;
    f.elements<<smd({-pitch/2,0},padX,padY,1)<<smd({pitch/2,0},padX,padY,2);
    const double x=bodyX/2,y=std::max(bodyY,padY)/2+.25;
    f.elements<<line(QPolygonF(QList<QPointF>{{-x+.1,-y},{x-.1,-y}}))<<line(QPolygonF(QList<QPointF>{{-x+.1,y},{x-.1,y}}));
    labels(f.elements,f.prefix,"",QRectF(QPointF(-pitch/2-padX/2,-y),QPointF(pitch/2+padX/2,y)));return f;
}
Footprint sot23(){
    // JEDEC TO-236AB (SOT-23): pins 1 and 2 at the bottom 1.9 mm apart, pin 3 at the top; IPC-7351 pads 0.6 × 1.0 mm.
    Footprint f;f.id="sot-23";f.name="SOT-23";f.prefix="T";f.source="JEDEC TO-236AB, IPC-7351 SOT95P237X112-3N";
    f.elements<<smd({-.95,1.15},.6,1.0,1)<<smd({.95,1.15},.6,1.0,2)<<smd({0,-1.15},.6,1.0,3);
    f.elements<<line(QPolygonF(QList<QPointF>{{-1.45,-.3},{-1.45,.65},{-1.35,.65}}))<<line(QPolygonF(QList<QPointF>{{1.45,-.3},{1.45,.65},{1.35,.65}}))
              <<line(QPolygonF(QList<QPointF>{{-1.45,-.3},{-1.45,-.65},{-.5,-.65}}))<<line(QPolygonF(QList<QPointF>{{1.45,-.3},{1.45,-.65},{.5,-.65}}));
    labels(f.elements,f.prefix,"",QRectF(QPointF(-1.45,-1.65),QPointF(1.45,1.65)));return f;
}
Footprint soic(int pins){
    // JEDEC MS-012 (SOIC, 3.9 mm body): pitch 1.27 mm, IPC-7351 pads 0.6 × 1.55 mm with rows 5.4 mm apart.
    Footprint f;f.id=QString("soic-%1").arg(pins);f.name=QString("SOIC-%1").arg(pins);f.prefix="IC";f.source="JEDEC MS-012, IPC-7351 SOIC127P600X175";
    const int half=pins/2;const double first=-(half-1)*1.27/2;
    for(int k=0;k<half;k++)f.elements<<smd({first+k*1.27,2.7},.6,1.55,k+1);
    for(int k=0;k<half;k++)f.elements<<smd({-first-k*1.27,-2.7},.6,1.55,half+k+1);
    const double x=-first+.85;
    f.elements<<box(-x,-1.95,x,1.95)<<dot({-x+.75,1.2},.5);
    labels(f.elements,f.prefix,"",QRectF(QPointF(-x,-3.5),QPointF(x,3.5)));return f;
}
}

Wizard wizardDefaults(Wizard::Form form){
    Wizard w;w.form=form;
    switch(form){
    case Wizard::SingleRow:w.count=6;break;
    case Wizard::DoubleRow:w.count=8;break;
    case Wizard::Quad:w.count=16;w.smd=true;w.pitch=1.27;w.quadWidth=w.quadHeight=10;break;
    case Wizard::Circle:w.count=8;w.circle=10;break;
    case Wizard::DoubleCircle:w.count=12;w.circle=12;w.innerCircle=8;break;
    }
    return w;
}
Footprint wizardFootprint(const Wizard &w){
    Footprint f;f.id="wizard";f.name=ui("Bauteil-Assistent");f.prefix=w.form==Wizard::SingleRow?"J":"IC";f.source={};
    const int n=std::clamp(w.form==Wizard::Quad?(w.count+3)/4*4:w.count,1,1000);int number=0;
    // A pad at a place; `across` is true where its row runs from top to bottom (the SMD pad lies across it).
    auto put=[&](QPointF at,bool across){
        number++;if(w.smd){f.elements<<(across?smd(at,w.length,w.width,number):smd(at,w.width,w.length,number));return;}
        f.elements<<pad(at,w.diameter,w.drill,number,number==1?PadShape::Square:PadShape::Round);
    };
    const double reach=w.smd?w.length/2:w.diameter/2;
    switch(w.form){
    case Wizard::SingleRow:{
        for(int k=0;k<n;k++)put({(k-(n-1)/2.0)*w.pitch,0},false);
        const double x=(n-1)*w.pitch/2+w.pitch/2,y=std::max(reach,w.pitch/2);f.elements<<box(-x,-y,x,y);break;}
    case Wizard::DoubleRow:{
        const int rows=(n+1)/2;
        for(int k=0;k<rows;k++)put({-w.rowSpacing/2,(k-(rows-1)/2.0)*w.pitch},true);
        for(int k=0;k<n-rows;k++)put({w.rowSpacing/2,((rows-1)/2.0-k)*w.pitch},true);
        const double x=std::max(w.rowSpacing/2-reach-.5,1.0),top=-rows*w.pitch/2;
        f.elements<<line(QPolygonF(QList<QPointF>{{-.6,top},{-x,top},{-x,-top},{x,-top},{x,top},{.6,top}}))<<arc({0,top},.6,180,0);break;}
    case Wizard::Quad:{
        // Counter-clockwise: left side downwards, bottom to the right, right side upwards, top to the left.
        const int side=n/4;const double x=w.quadWidth/2,y=w.quadHeight/2,run=(side-1)*w.pitch/2;
        for(int k=0;k<side;k++)put({-x,-run+k*w.pitch},true);
        for(int k=0;k<side;k++)put({-run+k*w.pitch,y},false);
        for(int k=0;k<side;k++)put({x,run-k*w.pitch},true);
        for(int k=0;k<side;k++)put({run-k*w.pitch,-y},false);
        const double bx=std::max(x-reach-.5,run+.5),by=std::max(y-reach-.5,run+.5);f.elements<<box(-bx,-by,bx,by)<<dot({-bx+1,-by+1},.5);break;}
    case Wizard::Circle:case Wizard::DoubleCircle:{
        for(int k=0;k<n;k++){
            const double r=(w.form==Wizard::DoubleCircle&&k%2?w.innerCircle:w.circle)/2,a=qDegreesToRadians(90-360.0*k/n);
            put(QPointF(r*std::cos(a),-r*std::sin(a)),false);
        }
        f.elements<<arc({0,0},w.circle/2+reach+.6,0,0);break;}
    }
    QRectF body;for(const auto &e:f.elements)body=body.united(bounds(e));
    labels(f.elements,f.prefix,QString(),body);return f;
}
QList<Footprint> footprints(){
    QList<Footprint> out;
    for(int n:{8,14,16,18,20,24,28,40})out<<dil(n);
    for(int n:{2,3,4,5,6,8,10})out<<header(n);
    out<<axial("res-0207-10",ui("Widerstand 0207, RM 10,16"),"R","",10.16,6.6,2.6,false,"body 0207 (6.3 × 2.5 mm), 400 mil pitch")
       <<axial("res-0207-7",ui("Widerstand 0207, RM 7,62"),"R","",7.62,6.6,2.6,false,"body 0207 (6.3 × 2.5 mm), 300 mil pitch")
       <<axial("do-41",ui("Diode DO-41, RM 10,16"),"D","",10.16,5.4,2.8,true,"JEDEC DO-204AL (DO-41), band at the cathode");
    out<<disc(2.54)<<disc(5.08)<<electrolytic(5,2)<<electrolytic(6.3,2.5)<<electrolytic(8,3.5)<<led(3)<<led(5)<<to92()<<to220();
    out<<chip("0603",1.6,.95,1.0,1.6,.8,"IPC-7351 RESC1608X55N")<<chip("0805",1.9,1.15,1.45,2.0,1.25,"IPC-7351 RESC2012X70N")<<chip("1206",2.9,1.15,1.8,3.2,1.6,"IPC-7351 RESC3216X70N");
    out<<sot23()<<soic(8)<<soic(14);
    return out;
}

QList<Element> placeable(QList<Element> elements,const Board &board,const QString &prefix,bool groupLoose){
    // Fresh group numbers, or one new group for loose elements.
    int next=nextGroup(board);QMap<int,int> fresh;
    for(auto &e:elements)for(int &g:e.groups){if(!fresh.contains(g))fresh[g]=next++;g=fresh[g];}
    if(groupLoose&&fresh.isEmpty()&&elements.size()>1)for(auto &e:elements)e.groups.append(next);
    // A component number the board has already becomes a new one, so the copy is a component of its own. Without any
    // numbers, the innermost group of each designator becomes a component of its own (loose elements got one group
    // above); a group that is the innermost one of several designators stays without, as nothing tells their elements
    // apart. Elements in no such group stay loose.
    freshParts(elements,board);
    if(groupLoose&&std::none_of(elements.begin(),elements.end(),[](const Element &e){return e.part!=0;})){
        auto designator=[](const Element &e){return e.type==ElementType::Text&&e.role==TextRole::Designator&&!e.groups.isEmpty();};
        QMap<int,int> count;for(const auto &e:elements)if(designator(e))count[e.groups.first()]++;
        QSet<int> used;for(const auto &e:board.elements)if(e.part)used.insert(e.part);QMap<int,int> number;
        for(const auto &e:elements)if(designator(e)&&count[e.groups.first()]==1)number.insert(e.groups.first(),freePart(used));
        for(auto &e:elements)for(int g:e.groups)if(count.contains(g)){e.part=number.value(g);break;}
    }
    // Designators: the prefix (letters before the number) with the next free number on the board.
    static const QRegularExpression numbered(QStringLiteral("^(.*?)(\\d+|\\?)?$"));
    QMap<QString,int> highest;
    for(const auto &e:board.elements)if(e.type==ElementType::Text&&e.role==TextRole::Designator){
        const auto m=numbered.match(e.text);bool ok;const int n=m.captured(2).toInt(&ok);if(ok)highest[m.captured(1)]=std::max(highest.value(m.captured(1)),n);
    }
    for(auto &e:elements)if(e.type==ElementType::Text&&e.role==TextRole::Designator){
        QString p=numbered.match(e.text).captured(1);if(p.isEmpty())p=prefix;if(p.isEmpty())continue;
        const int n=highest.value(p)+1;highest[p]=n;e.text=p+QString::number(n);updateStrokes(e);
    }
    // A copy is a new component with an identifier of its own; one cut out and put back keeps its identifier.
    QSet<QString> onBoard;for(const auto &e:board.elements)if(e.role==TextRole::Designator&&!e.component.isEmpty())onBoard.insert(e.component);
    for(auto &e:elements)if(e.type==ElementType::Text&&e.role==TextRole::Designator&&(e.component.isEmpty()||onBoard.contains(e.component)))e.component=newId();
    // Pads of footprints carry their pin number as name.
    for(auto &e:elements)if((e.type==ElementType::Pad||e.type==ElementType::SmdPad)&&e.pin.isEmpty()&&(e.part||!e.groups.isEmpty()))e.pin=e.name.trimmed();
    return elements;
}
QList<Element> placeable(const Footprint &footprint,const Board &board){return placeable(footprint.elements,board,footprint.prefix);}
QList<Element> macroComponent(QList<Element> elements){
    // Loose elements join the one component (or make it); placeable() then gives a number the board has a new one.
    QSet<int> numbers;for(const auto &e:elements)if(e.part)numbers.insert(e.part);
    const bool designator=std::any_of(elements.begin(),elements.end(),[](const Element &e){return e.type==ElementType::Text&&e.role==TextRole::Designator;});
    if(numbers.size()<=1&&designator){const int part=numbers.isEmpty()?1:*numbers.begin();for(auto &e:elements)e.part=part;}
    return elements;
}
}
