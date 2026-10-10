#include "assistant.h"
#include "language.h"
#include "legacy_writer.h"
#include "partbuilder.h"
#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QJsonArray>
#include <QLocale>
#include <functional>
#include <cmath>
#include <limits>

namespace openloch {
using namespace parts;
namespace {
// Decimal comma as in the original, but no group separator: "1.000" would read back as 1.
const QLocale german=[]{QLocale l(QLocale::German,QLocale::Germany);l.setNumberOptions(QLocale::OmitGroupSeparator);return l;}();
const QList<int> textColours{0x000000,0xFFFFFF,0xFF0000,0x0000FF,0x00FFFF,0x008000,0x808080};
const QList<QList<int>> series{
    {100,150,220,330,470,680},
    {100,120,150,180,220,270,330,390,470,560,680,820},
    {100,110,120,130,150,160,180,200,220,240,270,300,330,360,390,430,470,510,560,620,680,750,820,910},
    {100,105,110,115,121,127,133,140,147,154,162,169,178,187,196,205,215,226,237,249,261,274,287,301,316,332,348,
     365,383,402,422,442,464,487,511,536,562,590,619,649,681,715,750,787,825,866,909,953},
    {100,102,105,107,110,113,115,118,121,124,127,130,133,137,140,143,147,150,154,158,162,165,169,174,178,182,
     187,191,196,200,205,210,215,221,226,232,237,243,249,255,261,267,274,280,287,294,301,309,316,324,332,340,348,
     357,365,374,383,392,402,412,422,432,442,453,464,475,487,499,511,523,536,549,562,576,590,604,619,634,649,665,
     681,698,715,732,750,768,787,806,825,845,866,887,909,931,953,976}};
const QList<int> digitColours{0x000000,0x0055BB,0x0000FF,0x00B0FF,0x00FFFF,0x00FF00,0xFF0000,0xFF00FF,0x808080,0xFFFFFF};
constexpr int gold=0x008080,silver=0xC0C0C0,noBand=0xFFFF00;
// Nearest member of a series as (three significant digits, decade): value = digits * 10^(decade-2).
QPair<int,int> snap(double ohm,int index){
    const auto &table=series.value(qBound(0,index,4));int decade=int(std::floor(std::log10(ohm)));double m=ohm/std::pow(10.0,decade)*100;
    int best=100;double error=std::numeric_limits<double>::max();
    for(int t:table+QList<int>{1000}){const double e=std::abs(m-t)/t;if(e<error){error=e;best=t;}}
    if(best==1000){best=100;++decade;}
    return {best,decade};
}
struct Palette {QString name;QColor colour;};
QList<Palette> palette(int style){
    if(style==11)return {{"Beige",QColor(222,214,140)},{"Braun",QColor(150,100,55)},{"Dunkelblau",QColor(40,60,140)},{"Dunkelgruen",QColor(45,105,60)},
        {"Grau",QColor(150,150,150)},{"Hellblau",QColor(125,180,230)},{"Orange",QColor(240,140,40)},{"Schwarz",QColor(60,60,60)}};
    if(style==12||style==14)return {{"Blau",QColor(30,110,225)},{"Blau2",QColor(65,145,240)},{"Gelb",QColor(245,215,75)},{"Gelb2",QColor(255,230,130)},
        {"Gold",QColor(200,168,105)},{"Gold2",QColor(220,192,140)},{"Orange",QColor(255,130,20)},{"Orange2",QColor(255,160,65)},{"Schwarz",QColor(75,66,60)}};
    if(style==13)return {{"Blau",QColor(45,155,235)},{"Gelb",QColor(215,205,0)},{"Grau",QColor(182,182,182)},{"Gruen",QColor(110,160,45)},
        {"Rot",QColor(240,100,112)},{"Schwarz",QColor(106,106,106)}};
    return {};
}
}

QStringList assistantStyles(){
    return {"Kontur: regelmäßiges Vieleck","Kontur: Rechteck","Kontur: Kreis/Ellipse","Kontur: \" H \"","Kontur: \" I \"","Kontur: \" L \"",
        "Kontur: \" M \"","Kontur: \" T \"","Kontur: \" U \"","Kontur: Elko","Kontur: Widerstand","Bauteil: Widerstand, liegend",
        "Bauteil: Elko, koaxial","Bauteil: Kondensator","Bauteil: Diode, liegend"};
}
QList<AssistantParameter> assistantParameters(int style,const QStringList &colours){
    auto number=[](const QString &name,const QString &unit,double value,double minimum,double maximum,AssistantParameter::Type type=AssistantParameter::Number){
        AssistantParameter p;p.name=name;p.unit=unit;p.type=type;p.value=value;p.minimum=minimum;p.maximum=maximum;return p;};
    auto mm=[&](const QString &name,double value){return number(name,"mm",value,.01,200);};
    auto text=[](const QString &name,const QString &unit,const QString &value){AssistantParameter p;p.name=name;p.unit=unit;p.type=AssistantParameter::Text;p.text=value;return p;};
    auto choice=[](const QString &name,const QStringList &items,int index){AssistantParameter p;p.name=name;p.type=AssistantParameter::Choice;p.choices=items;p.choice=qBound(0,index,qMax(0,int(items.size())-1));return p;};
    const auto angle=number("Drehung","°",0,0,360);const auto lead=number("Drahtstärke","mm",.4,.1,2);
    const auto bodies=colours.isEmpty()?gradientNames(style):colours;const QStringList inks{"Schwarz","Weiss","Blau","Rot","Gelb","Grün","Grau"};
    switch(style){
    case 0:return {mm("Durchmesser aussen",10),number("Anzahl der Ecken","",5,3,20,AssistantParameter::Integer),angle};
    case 1:return {mm("Breite",10),mm("Höhe",10),angle};
    case 2:return {mm("Durchmesser 1",10),mm("Durchmesser 2",10),angle};
    case 3:case 5:case 7:case 8:return {mm("Durchmesser 1",2),mm("Durchmesser 2",2),mm("Breite",7),mm("Höhe",10),angle};
    case 4:return {mm("Durchmesser 1",3),mm("Durchmesser 2",4),mm("Länge 1",7),mm("Länge 2",3),angle};
    case 6:return {mm("Durchmesser 1",2),mm("Durchmesser 2",2),mm("Durchmesser 3",2),mm("Breite",10),mm("Höhe",10),angle};
    case 9:case 10:return {mm("Durchmesser",5),mm("Länge",10),angle};
    case 11:return {mm("Durchmesser",3),mm("Länge",7),lead,text("Beschreibung","",ui("Widerstand")),number("Normwert","Ohm",1000,.1,1e12,AssistantParameter::Ohm),
        choice("Norm",{"E6","E12","E24","E48","E96"},1),choice("Farbe",bodies,0),choice("Textfarbe",inks,0)};
    case 12:return {mm("Durchmesser",6),mm("Länge",15),lead,text("Beschreibung","",ui("Elko")),text("Wert","uF/V","10uF/16V"),choice("Farbe",bodies,0),choice("Textfarbe",inks,1)};
    case 13:return {mm("Breite",6),mm("Länge",15),lead,mm("Rastermass",5.08),text("Beschreibung","",ui("Kondensator")),text("Wert","F","100nF"),choice("Farbe",bodies,4),choice("Textfarbe",inks,1)};
    case 14:return {mm("Breite",3),mm("Länge",6),lead,text("Beschreibung","",ui("Diode")),text("Wert","Typ","?"),choice("Farbe",bodies,8),choice("Textfarbe",inks,1)};
    }
    return {};
}
QString formatOhm(double ohm){
    QChar prefix=' ';double divisor=1;
    if(ohm>1e12){prefix='T';divisor=1e12;}else if(ohm>1e9){prefix='G';divisor=1e9;}else if(ohm>999999.999999){prefix='M';divisor=1e6;}else if(ohm>999.999999){prefix='K';divisor=1e3;}
    return german.toString(ohm/divisor,'g',15)+' '+prefix+"Ohm";
}
double parseOhm(const QString &text){
    int i=0;while(i<text.size()&&(text[i].isDigit()||QStringLiteral(" +,-.").contains(text[i])))++i;
    double factor=1;if(i<text.size()){const QChar c=text[i];factor=c=='k'||c=='K'?1e3:c=='M'?1e6:c=='G'?1e9:c=='T'?1e12:1;}
    QString number=text.left(i).trimmed();number.replace(',','.');bool ok=false;const double v=number.toDouble(&ok);
    return ok?v*factor:std::numeric_limits<double>::quiet_NaN();
}
QString assistantValueText(const AssistantParameter &p){
    switch(p.type){
    case AssistantParameter::Number:return german.toString(p.value,'g',15);
    case AssistantParameter::Integer:return QString::number(qRound(p.value));
    case AssistantParameter::Text:return p.text;
    case AssistantParameter::Choice:return p.choices.value(p.choice);
    case AssistantParameter::Ohm:return formatOhm(p.value);
    }
    return {};
}
AssistantInput assistantSetValue(AssistantParameter &p,const QString &input){
    if(p.type==AssistantParameter::Text){p.text=input;return AssistantInput::Accepted;}
    if(p.type==AssistantParameter::Choice){const int i=p.choices.indexOf(input);if(i<0)return AssistantInput::Invalid;p.choice=i;return AssistantInput::Accepted;}
    double v;
    if(p.type==AssistantParameter::Ohm)v=parseOhm(input);
    else{QString number=input.trimmed();number.replace(',','.');bool ok=false;v=number.toDouble(&ok);if(!ok)v=std::numeric_limits<double>::quiet_NaN();}
    if(std::isnan(v))return AssistantInput::Invalid;
    if(p.type==AssistantParameter::Integer)v=round(v);
    const double lo=p.type==AssistantParameter::Integer?round(p.minimum):p.minimum,hi=p.type==AssistantParameter::Integer?round(p.maximum):p.maximum;
    if(lo==hi||(v>=lo&&v<=hi)){p.value=v;return AssistantInput::Accepted;}
    // Out of range: the nearer bound. For Ohm values the original measures from zero, so it is always the lower one.
    p.value=p.type==AssistantParameter::Ohm||std::abs(v-lo)<=std::abs(v-hi)?lo:hi;return AssistantInput::Corrected;
}
double standardValue(double ohm,int index){
    if(!(ohm>0))return ohm;const auto [digits,decade]=snap(ohm,index);return digits*std::pow(10.0,decade-2);
}
QList<int> colourBands(double ohm,int index){
    if(!(ohm>0))return {-1,-1,-1,-1};
    const auto [t,decade]=snap(ohm,index);const bool three=index>=3;const int digits=three?t:t/10,exponent=three?decade-2:decade-1;
    int multiplier;
    if(exponent>=0&&exponent<=9)multiplier=digitColours[exponent];else if(exponent>9||exponent==-1)multiplier=gold;else if(exponent==-2)multiplier=silver;else return {-1,-1,-1,-1};
    if(three)return {digitColours[digits/100],digitColours[digits/10%10],digitColours[digits%10],multiplier};
    return {digitColours[digits/10],digitColours[digits%10],multiplier,-1};
}
QStringList gradientNames(int style){QStringList names;for(const auto &p:palette(style))names<<p.name;return names;}
// OpenLoch's own body pictures: a soft highlight for round and diagonal shading, a bevelled face for square ones.
QByteArray gradientBitmap(int style,const QString &name){
    QColor base;for(const auto &p:palette(style))if(p.name.compare(name,Qt::CaseInsensitive)==0)base=p.colour;
    if(!base.isValid())return {};
    const int size=64;QImage image(size,size,QImage::Format_RGB888);
    auto mix=[](QColor a,QColor b,double t){t=qBound(0.0,t,1.0);return QColor::fromRgbF(a.redF()+(b.redF()-a.redF())*t,a.greenF()+(b.greenF()-a.greenF())*t,a.blueF()+(b.blueF()-a.blueF())*t);};
    const QColor light=mix(base,Qt::white,.65),dark=mix(base,Qt::black,.3);
    for(int y=0;y<size;y++)for(int x=0;x<size;x++){
        QColor c;
        if(style==13){const int edge=qMin(qMin(x,y),qMin(size-1-x,size-1-y));c=edge>=3?base:(x<=y&&x+y<size?mix(base,Qt::white,.25):mix(base,Qt::black,.2));}
        else{const double d=std::hypot(x-.36*size,y-.4*size)/(size*.9);c=d<.5?mix(light,base,d/.5):mix(base,dark,(d-.5)/.5);}
        image.setPixelColor(x,y,c);
    }
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"BMP");return bytes;
}
QJsonObject assistantObject(int style,const QList<AssistantParameter> &p,const QByteArray &bitmap,bool back,QPointF o){
    auto mm=[&](int i){return p.value(i).value*100;};auto radians=[&](int i){return p.value(i).value*pi/180;};
    auto ink=[&](int i){return textColours.value(p.value(i).choice);};
    QJsonObject result;
    switch(style){
    case 0:{
        const double d=mm(0),n=qMax(3.0,p.value(1).value);double a=radians(2);QList<QPointF> v;
        for(int i=0;i<int(n);i++){v<<QPointF(round(o.x()+d/2*std::cos(a)),round(o.y()-d/2*std::sin(a)));a+=2*pi/n;}
        result=wire(7,10,0,v);break;
    }
    case 1:result=outline(6,rectangle(o,mm(0),mm(1)),o,radians(2));break;
    case 2:{
        const double d1=mm(0),d2=mm(1);QList<QPointF> v;for(int k=1;k<=16;k++){const double a=k*2*pi/16;v<<QPointF(o.x()+std::cos(a)*d1/2,o.y()+std::sin(a)*d2/2);}
        auto inner=outline(7,v,o,radians(2));result=legacyObject("TKreis",5,10);result["pen"]=0;result["brush"]=0x808080;result["transparent"]=false;
        result["ellipse"]=QJsonArray{0,0,0,0};result["ellipse_flag"]=false;result["ellipse2"]=QJsonArray{0,0,0,0};result["inner"]=inner;break;
    }
    case 3:case 5:case 7:case 8:{
        const double a=mm(0),b=mm(1),w=mm(2),h=mm(3),x=o.x(),y=o.y();QList<QPointF> v;
        if(style==3)v={{x-w/2,y-h/2},{x+a-w/2,y-h/2},{x+a-w/2,y-b/2},{x-a+w/2,y-b/2},{x-a+w/2,y-h/2},{x+w/2,y-h/2},{x+w/2,y+h/2},{x-a+w/2,y+h/2},{x-a+w/2,y+b/2},{x+a-w/2,y+b/2},{x+a-w/2,y+h/2},{x-w/2,y+h/2}};
        if(style==5)v={{x-w/2,y-h/2},{x+a-w/2,y-h/2},{x+a-w/2,y+h/2-b},{x+w/2,y+h/2-b},{x+w/2,y+h/2},{x-w/2,y+h/2}};
        if(style==7)v={{x-w/2,y-h/2},{x+w/2,y-h/2},{x+w/2,y-h/2+b},{x+a/2,y-h/2+b},{x+a/2,y+h/2},{x-a/2,y+h/2},{x-a/2,y-h/2+b},{x-w/2,y-h/2+b}};
        if(style==8)v={{x-w/2,y-h/2},{x+a-w/2,y-h/2},{x+a-w/2,y+h/2-b},{x-a+w/2,y+h/2-b},{x-a+w/2,y-h/2},{x+w/2,y-h/2},{x+w/2,y+h/2},{x-w/2,y+h/2}};
        result=outline(7,v,o,radians(4));break;
    }
    case 4:result=outline(7,shapeI(o,mm(0),mm(1),mm(2),mm(3)),o,radians(4));break;
    case 6:{
        const double a=mm(0),b=mm(1),m=mm(2),w=mm(3),h=mm(4),x=o.x(),y=o.y();
        result=outline(7,{{x-w/2,y-h/2},{x+w/2,y-h/2},{x+w/2,y+h/2},{x-a+w/2,y+h/2},{x-a+w/2,y-h/2+b},{x+m/2,y-h/2+b},{x+m/2,y+h/2},{x-m/2,y+h/2},{x-m/2,y-h/2+b},{x+a-w/2,y-h/2+b},{x+a-w/2,y+h/2},{x-w/2,y+h/2}},o,radians(5));break;
    }
    case 9:result=outline(7,shapeElko(o,mm(0),mm(1)),o,radians(2));smooth(result,2,mm(0)/20);break;
    case 10:{const double d=mm(0),l=mm(1);result=outline(7,shapeI(o,d-d/10,d,.6*l,.2*l),o,radians(2));smooth(result,0,1);break;}
    case 11:{
        const double d=mm(0),l=mm(1);const int series=p.value(5).choice;const double ohm=standardValue(p.value(4).value,series);
        QJsonArray children=leads(o,l,mm(2));
        auto shell=body(outline(7,shapeI(o,.9*d,d,.6*l,.2*l),o,0),0x00FFFF,bitmap);smooth(shell,0,1);children.append(shell);
        children.append(label("<BauteilKennung>",{o.x()+round(.3*d),round(o.y()-.45*l)},int(round(.6*d)),ink(7)));
        // Colour rings on the lower half of the body, the first band at the bottom.
        const double ring=round(l/4),e=round(ring/16),w=round(.45*d);const auto colours=colourBands(p.value(4).value,series);QJsonArray bands;
        for(int child=0;child<4;child++){
            const double y=round(o.y()+ring*(3-child)/4);const int colour=colours.value(child,-1);
            // Visible bands get width 20: inserting the part reloads it, and loading sets the colour code's default width.
            bands.append(wire(4,colour<0?0:20,colour<0?noBand:colour,{{o.x()-w+e,y},{o.x()+w-e,y}}));
        }
        for(auto &&b:bands){auto n=b.toObject();n["flag"]=true;b=n;}
        auto code=legacyGroup(bands);code["type"]="TFarbcode";code["id"]="";code["value"]="";code["description"]="";code["group_value"]=0;
        code["group_flags"]=QJsonArray{false,true};code["flag"]=true;code["bands"]=series;code["resistance"]=ohm;children.append(code);
        result=legacyGroup(children);result["id"]="R#";result["value"]=formatOhm(ohm);result["description"]=p.value(3).text;break;
    }
    case 12:{
        const double d=mm(0),l=mm(1);QJsonArray children=leads(o,l,mm(2));
        children.append(wire(7,10,0xFFFFFF,{{round(o.x()-.4*d),round(o.y()-l/2)},{round(o.x()+.4*d),round(o.y()-l/2)}}));
        children.append(wire(7,10,0x000000,{{round(o.x()-.4*d),round(o.y()+l/2)},{round(o.x()+.4*d),round(o.y()+l/2)}}));
        auto shell=body(outline(7,shapeElko(o,d,l),o,0),0x800000,bitmap);smooth(shell,2,d/20);children.append(shell);
        const QPointF plus(o.x(),round(o.y()+.35*l));const double arm=d/8;
        children.append(wire(7,20,ink(6),{{round(plus.x()-arm),plus.y()},{round(plus.x()+arm),plus.y()}}));
        children.append(wire(7,20,ink(6),{{plus.x(),round(plus.y()-arm)},{plus.x(),round(plus.y()+arm)}}));
        const double y=round(o.y()-.45*l);const int h=int(round(.3*d));
        children.append(label("<BauteilKennung>",{o.x()+round(.45*d),y},h,ink(6)));children.append(label("<BauteilWertTyp>",{o.x()-round(.05*d),y},h,ink(6)));
        result=legacyGroup(children);result["id"]="C#";result["value"]=p.value(4).text;result["description"]=p.value(3).text;break;
    }
    case 13:{
        const double b=mm(0),l=mm(1);QJsonArray children=leads(o,mm(3),mm(2));
        auto shell=body(outline(6,rectangle(o,b,l),o,0),0x800000,bitmap);smooth(shell,2,round((b+l)/40));children.append(shell);
        const double y=round(o.y()-.45*l);const int h=int(round(.3*b));
        children.append(label("<BauteilKennung>",{o.x()+round(.45*b),y},h,ink(7)));children.append(label("<BauteilWertTyp>",{o.x()-round(.05*b),y},h,ink(7)));
        result=legacyGroup(children);result["id"]="C#";result["value"]=p.value(5).text;result["description"]=p.value(4).text;break;
    }
    case 14:{
        const double b=mm(0),l=mm(1);QJsonArray children=leads(o,l,mm(2));
        auto shell=body(outline(6,rectangle(o,b,l),o,0),0x800000,bitmap);smooth(shell,2,round((b+l)/40));children.append(shell);
        children.append(label("<BauteilKennung>",{o.x()+round(.3*b),round(o.y()-.45*l)},int(round(.6*b)),ink(6)));
        // Cathode ring towards the second lead, created after the label as in the original.
        const double y=round(o.y()+.35*l);auto ring=wire(4,int(round(l/15)),ink(6),{{round(o.x()-b/2+l/30),y},{round(o.x()+b/2-l/30),y}});ring["flag"]=true;children.append(ring);
        result=legacyGroup(children);result["id"]="D#";result["value"]=p.value(4).text;result["description"]=p.value(3).text;break;
    }
    default:return {};
    }
    if(style>=11){
        auto children=result["children"].toArray();for(auto &&c:children){auto n=c.toObject();n["flag"]=true;c=n;}result["children"]=children;
        result["type"]="TBt";result["component_kind"]=style;result["group_value"]=1;result["group_flags"]=QJsonArray{true,true};result["width"]=0;result["pen"]=0;result["brush"]=0;
    }
    // Everything goes on the side being edited.
    std::function<void(QJsonObject&)> side=[&](QJsonObject &n){n["back"]=back;if(n.contains("inner")){auto i=n["inner"].toObject();side(i);n["inner"]=i;}
        if(n.contains("children")){auto c=n["children"].toArray();for(auto &&v:c){auto x=v.toObject();side(x);v=x;}n["children"]=c;}};
    side(result);return result;
}
}
