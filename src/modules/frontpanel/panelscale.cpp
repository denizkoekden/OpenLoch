#include "panelgenerators.h"
#include "panelgeometry.h"
#include "inifile.h"
#include "language.h"
#include <QJsonArray>
#include <QLineF>
#include <algorithm>
#include <cmath>
#include <numbers>

// Scales as the original's scale assistant builds them. Each style has a fixed list of parameters and a fixed list of
// parts with their own look; docs/modules/frontpanel.md describes the rules the shapes follow.
namespace openloch::frontpanel {
using S=ScaleParameters;
namespace {
constexpr double degree=std::numbers::pi/180;
QPointF direction(double degrees){return {std::cos(degrees*degree),-std::sin(degrees*degree)};}  // counter-clockwise on screen
QPointF unit(QPointF v){const double l=std::hypot(v.x(),v.y());return l>0?v/l:QPointF(1,0);}
double angleOf(QPointF v){return std::atan2(-v.y(),v.x())/degree;}
QJsonArray transformJson(const QTransform &t){return QJsonArray{t.m11(),t.m12(),t.m21(),t.m22(),t.dx(),t.dy()};}
QJsonValue colourJson(const QColor &c){return c.name(QColor::HexRgb);}
QColor colourOf(const QJsonValue &v,const QColor &fallback){const QColor c=QColor::fromString(v.toString());return c.isValid()?c:fallback;}


bool isStraight(S::Style s){return s==S::StraightLinear||s==S::StraightLogarithmic||s==S::StraightDots;}
bool isRound(S::Style s){return s==S::RoundLinear||s==S::RoundLogarithmic||s==S::RoundDots;}
bool isArc(S::Style s){return s==S::IncreasingArc||s==S::DecreasingArc||s==S::BothSidesArc||s==S::CircleSegment;}

S::Info flagInfo(const QString &label,bool on){return {S::Info::Flag,label,{},on?1.0:0.0,0,1};}
S::Info countInfo(const QString &label,int value,int low,int high){return {S::Info::Count,label,{},double(value),double(low),double(high)};}
S::Info realInfo(const QString &label,const char *unit,double value,double low,double high){return {S::Info::Real,label,QString::fromUtf8(unit),value,low,high};}

S::Design part(const QString &name,bool fill=false,bool font=false){S::Design d;d.name=name;d.hasFill=fill;d.hasFont=font;return d;}

Element lineElement(QPointF a,QPointF b,const S::Design &d,const QString &name={}){
    Element e=newElement(ElementType::Line);e.points={a,b};e.pen=d.pen;e.machining=d.tool;e.name=name;e.fill.style=FillStyle::None;return e;
}
Element group(const QString &name,QList<Element> children){Element e=newElement(ElementType::Group);e.name=name;e.children=std::move(children);e.pen.style=PenStyle::None;return e;}
void styleShape(Element &e,const S::Design &d){e.pen=d.pen;e.machining=d.tool;if(d.hasFill)e.fill=d.fill;else e.fill.style=FillStyle::None;}
// A label centred on `at`, its base line turned by `angle` degrees.
Element labelElement(const QString &text,const S::Design &d,double height,QPointF at,double angle){
    Element e=newElement(ElementType::Text);e.text=text;e.font=d.font;e.bold=d.bold;e.italic=d.italic;e.strokeFont=d.useStrokeFont?d.strokeFont:QString();
    styleShape(e,d);
    e.frame={QPointF(0,0),QPointF(1,0),QPointF(0,1)};const double width=naturalTextWidth(e,height);
    const QPointF along=direction(angle),down(-along.y(),along.x());const QPointF p0=at-along*(width/2)-down*(height/2);
    e.frame={p0,p0+along*width,p0+down*height};return e;
}
// Two strokes through the centre, a fifth of the radius long to each side.
Element centreMark(double radius,const S::Design &d){
    const double a=std::abs(radius)/5;
    return group(d.name,{lineElement(QPointF(-a,0),QPointF(a,0),d),lineElement(QPointF(0,-a),QPointF(0,a),d)});
}
// The marks of a division as fractions 0…1 of the course, in the direction of rising values. A logarithmic scale is one
// decade whose marks stand for equal steps of value (1 + 9·v for v from 0 to 1).
QList<double> marks(bool logarithmic,int n1,int n2,bool main){
    QList<double> out;n1=std::max(1,n1);
    auto at=[&](double v){return logarithmic?std::log10(1+9*v):v;};
    if(main){for(int i=0;i<=n1;i++)out<<at(double(i)/n1);return out;}
    if(n2>=2)for(int i=0;i<n1;i++)for(int j=1;j<n2;j++)out<<at((i+double(j)/n2)/n1);
    return out;
}
// The line or circle along which the marks of a scale lie. Straight: middle at the origin, values rising to the right.
// Round: centre at the origin, values rising clockwise from `start`. A flattened arc ("low profile") runs through the
// same end points around a centre moved away from the scale; marks are carried over to it from the true centre.
struct Course {
    bool round=false,reverse=false,flat=false;double length=0,radius=0,start=0,range=0,flatRadius=0;QPointF flatCentre;
    double angle(double f) const{if(reverse)f=1-f;return start-f*range;}
    QPointF onCircle(double f) const{return direction(angle(f))*radius;}
    QPointF at(double f) const{
        if(!round){if(reverse)f=1-f;return QPointF(-length/2+f*length,0);}
        if(!flat)return onCircle(f);
        return flatCentre+unit(onCircle(f)-flatCentre)*flatRadius;
    }
    QPointF normal(double f) const{
        if(!round)return QPointF(0,-1);
        return flat?unit(onCircle(f)-flatCentre):direction(angle(f));
    }
};
// A ring piece from fraction f0 to f1 of a course whose width changes linearly from w0 to w1 (outwards from its radius).
// As in the original, each edge has sixteen equal steps and the outline is drawn as a B-spline, which rounds the
// corners slightly.
Element band(const Course &c,double f0,double f1,double w0,double w1,const S::Design &d){
    QPolygonF p;constexpr int steps=16;
    for(int i=0;i<=steps;i++){const double t=double(i)/steps;p<<direction(c.angle(f0+(f1-f0)*t))*(c.radius+w0+(w1-w0)*t);}
    for(int i=steps;i>=0;i--)p<<direction(c.angle(f0+(f1-f0)*double(i)/steps))*c.radius;
    Element e=newElement(ElementType::Polygon);e.points=p;e.contour.corners=Corners::Spline;styleShape(e,d);e.name=d.name;return e;
}
double normalAngle(double a){a=std::fmod(a,360);return a<0?a+360:a;}
}

// ---------------------------------------------------------------- parameter lists and parts
QString scaleStyleTitle(S::Style s){
    switch(s){
    case S::StraightLinear:return ui("Gerade Skala, linear geteilt");
    case S::RoundLinear:return ui("Runde Skala, linear geteilt");
    case S::StraightLogarithmic:return ui("Gerade Skala, logarithmisch geteilt");
    case S::RoundLogarithmic:return ui("Runde Skala, logarithmisch geteilt");
    case S::StraightDots:return ui("Gerade Punktskala");
    case S::RoundDots:return ui("Runde Punktskala");
    case S::Segments:return ui("Skala aus farbigen Kreisabschnitten");
    case S::IncreasingArc:return ui("Zunehmender Bogen");
    case S::DecreasingArc:return ui("Abnehmender Bogen");
    case S::BothSidesArc:return ui("Zu beiden Seiten zunehmender Bogen");
    case S::CircleSegment:return ui("Kreisringabschnitt");
    case S::Polygon:return ui("Regelmäßiges Vieleck");
    case S::Sine:return ui("Sinuskurve");
    }
    return {};
}
QList<S::Info> S::info(Style s){
    // Lengths may be negative where the original allows it: marks and labels then lie on the other side.
    switch(s){
    case StraightLinear:case StraightLogarithmic:return {
        realInfo(ui("Länge"),"mm",50,0.01,200),flagInfo(ui("Lauflinie"),true),countInfo(ui("1. Teilung: Segmente"),10,1,50),realInfo(ui("1. Teilung: Länge"),"mm",4,-100,200),
        flagInfo(ui("2. Teilung"),true),countInfo(ui("2. Teilung: Segmente"),5,2,50),realInfo(ui("2. Teilung: Länge"),"mm",2,-100,200),flagInfo(ui("Beschriftung"),true),
        realInfo(ui("Texthöhe"),"mm",3,0.01,200),realInfo(ui("Abstand"),"mm",2,-100,200),realInfo(ui("Textwinkel"),"°",0,0,360),flagInfo(ui("Richtung umkehren"),false)};
    case StraightDots:return {
        realInfo(ui("Länge"),"mm",30,0.01,200),flagInfo(ui("Lauflinie"),false),countInfo(ui("1. Teilung: Segmente"),6,1,50),realInfo(ui("1. Teilung: Durchmesser"),"mm",0.3,0.01,200),
        flagInfo(ui("2. Teilung"),true),countInfo(ui("2. Teilung: Segmente"),5,2,50),realInfo(ui("2. Teilung: Durchmesser"),"mm",0.1,0.01,200),flagInfo(ui("Beschriftung"),true),
        realInfo(ui("Texthöhe"),"mm",2,0.01,200),realInfo(ui("Abstand"),"mm",2,-100,200),realInfo(ui("Textwinkel"),"°",0,0,360),flagInfo(ui("Richtung umkehren"),false)};
    case RoundLinear:case RoundLogarithmic:return {
        realInfo(ui("Winkelbereich"),"°",120,1,360),realInfo(ui("Radius"),"mm",25,0.01,200),flagInfo(ui("Lauflinie"),true),flagInfo(ui("Mittelpunkt"),true),
        countInfo(ui("1. Teilung: Segmente"),10,1,50),realInfo(ui("1. Teilung: Länge"),"mm",7,-100,200),flagInfo(ui("2. Teilung"),true),countInfo(ui("2. Teilung: Segmente"),5,2,50),
        realInfo(ui("2. Teilung: Länge"),"mm",3,-100,200),realInfo(ui("Drehung"),"°",0,0,360),flagInfo(ui("Beschriftung"),true),realInfo(ui("Texthöhe"),"mm",4,0.01,200),
        realInfo(ui("Abstand"),"mm",3,-100,200),realInfo(ui("Textwinkel"),"°",0,0,360),flagInfo(ui("Richtung umkehren"),false),realInfo(ui("„Low profile“"),"%",0,0,100),
        flagInfo(ui("Text mitdrehend"),false)};
    case RoundDots:return {
        realInfo(ui("Winkelbereich"),"°",120,1,360),realInfo(ui("Radius"),"mm",25,0.01,200),flagInfo(ui("Lauflinie"),false),flagInfo(ui("Mittelpunkt"),true),
        countInfo(ui("1. Teilung: Segmente"),6,1,50),realInfo(ui("1. Teilung: Durchmesser"),"mm",0.5,0.01,200),flagInfo(ui("2. Teilung"),true),countInfo(ui("2. Teilung: Segmente"),5,2,50),
        realInfo(ui("2. Teilung: Durchmesser"),"mm",0.2,0.01,200),realInfo(ui("Drehung"),"°",0,0,360),flagInfo(ui("Beschriftung"),true),realInfo(ui("Texthöhe"),"mm",4,0.01,200),
        realInfo(ui("Abstand"),"mm",4,-100,200),realInfo(ui("Textwinkel"),"°",0,0,360),flagInfo(ui("Richtung umkehren"),false),realInfo(ui("„Low profile“"),"%",0,0,100),
        flagInfo(ui("Text mitdrehend"),false)};
    case Segments:{
        QList<Info> list{realInfo(ui("Winkelbereich"),"°",120,1,360),realInfo(ui("Radius innen"),"mm",15,0.01,200),realInfo(ui("Breite"),"mm",2,0.01,200),
            countInfo(ui("Segmente (1…10)"),5,1,10),flagInfo(ui("Mittelpunkt"),true)};
        const double limits[9]{20,40,60,80,85,90,93,96,99};
        for(int k=0;k<9;k++){Info i=realInfo(ui("%1. Segmentgrenze"),"%",limits[k],0,100);i.label=i.label.arg(k+1);list<<i;}
        list<<flagInfo(ui("Grenzlinien"),true)<<realInfo(ui("Länge"),"mm",4,-100,200)<<realInfo(ui("Drehung"),"°",0,0,360)<<flagInfo(ui("Beschriftung"),true)
            <<realInfo(ui("Texthöhe"),"mm",3,0.01,200)<<realInfo(ui("Abstand"),"mm",5,-100,200)<<realInfo(ui("Textwinkel"),"°",0,0,360)<<flagInfo(ui("Text mitdrehend"),false);
        return list;}
    case IncreasingArc:case DecreasingArc:case BothSidesArc:return {
        realInfo(ui("Winkelbereich"),"°",90,1,360),realInfo(ui("Radius"),"mm",15,0.01,200),realInfo(ui("Breite"),"mm",5,-100,200),flagInfo(ui("Mittelpunkt"),true),realInfo(ui("Drehung"),"°",0,0,360)};
    case CircleSegment:return {
        realInfo(ui("Winkelbereich"),"°",90,1,360),realInfo(ui("Radius innen"),"mm",15,0.01,200),realInfo(ui("Breite"),"mm",2,0.01,200),flagInfo(ui("Mittelpunkt"),true),realInfo(ui("Drehung"),"°",0,0,360)};
    case Polygon:return {realInfo(ui("Durchmesser außen"),"mm",30,0.01,200),countInfo(ui("Anzahl der Ecken"),6,3,20),flagInfo(ui("Mittelpunkt"),true),realInfo(ui("Drehung"),"°",0,0,360)};
    case Sine:return {realInfo(ui("Höhe"),"mm",30,0.01,200),realInfo(ui("Breite"),"mm",50,0.01,200),realInfo(ui("Anzahl der Schwingungen"),"",1,0.1,50),realInfo(ui("Phasenlage"),"°",0,0,360),
        flagInfo(ui("Mittellinie"),true)};
    }
    return {};
}
QList<S::Design> S::defaultDesign(Style s){
    QList<Design> list;
    switch(s){
    case StraightLinear:case StraightLogarithmic:case RoundLinear:case RoundLogarithmic:case StraightDots:case RoundDots:{
        const bool dots=s==StraightDots||s==RoundDots;
        list<<part(ui("Beschriftung"),true,true)<<part(ui("1. Teilung"),dots)<<part(ui("2. Teilung"),dots)<<part(ui("Lauflinie"));
        if(isRound(s))list<<part(ui("Mittelpunkt"));
        break;}
    case Segments:
        list<<part(ui("Beschriftung"),true,true)<<part(ui("Grenzlinien"))<<part(ui("Mittelpunkt"));
        for(int k=1;k<=10;k++){Design d=part(ui("%1. Segment"),true);d.name=d.name.arg(k);list<<d;}
        break;
    case IncreasingArc:case DecreasingArc:case BothSidesArc:case CircleSegment:list<<part(ui("Bogen"),true)<<part(ui("Mittelpunkt"));break;
    case Polygon:{Design d=part(ui("Polygon"),true);d.fill=Fill{FillStyle::None,Qt::white,QColor(0xc0,0xc0,0xc0),Gradient::None};list<<d<<part(ui("Mittelpunkt"));break;}
    case Sine:list<<part(ui("Kurve"))<<part(ui("Mittellinie"));break;
    }
    return list;
}
S::ScaleParameters(Style s):style(s){for(const auto &i:info(s))values<<i.value;design=defaultDesign(s);}
double S::value(int index) const{
    if(index>=0&&index<values.size())return values[index];
    const auto list=info(style);return index>=0&&index<list.size()?list[index].value:0;
}
void S::setValue(int index,double v){
    const auto list=info(style);if(index<0||index>=list.size())return;
    while(values.size()<list.size())values<<list[values.size()].value;
    values[index]=v;
}
int S::labelCount() const{
    if(isStraight(style))return int(value(S::StraightDivisions1))+1;
    if(isRound(style))return int(value(S::RoundDivisions1))+1;
    if(style==Segments)return int(value(S::SegmentCount))+1;
    return 0;
}
QStringList S::labelTexts() const{
    QStringList out;for(int i=0;i<labelCount();i++)out<<(i<texts.size()?texts[i]:QString::number(i));return out;
}

// ---------------------------------------------------------------- storing
namespace {
QJsonObject designJson(const S::Design &d){
    QJsonObject o{{"pen",colourJson(d.pen.color)},{"width",d.pen.width},{"line",int(d.pen.style)},{"tool",int(d.tool)}};
    if(d.hasFill){o["fill"]=colourJson(d.fill.color);o["fill2"]=colourJson(d.fill.color2);o["pattern"]=int(d.fill.style);o["gradient"]=int(d.fill.gradient);}
    if(d.hasFont){o["font"]=d.font;o["strokeFont"]=d.strokeFont;o["useStrokeFont"]=d.useStrokeFont;o["bold"]=d.bold;o["italic"]=d.italic;o["fontSize"]=d.fontSize;}
    return o;
}
void readDesign(S::Design &d,const QJsonObject &o){
    d.pen.color=colourOf(o["pen"],d.pen.color);d.pen.width=std::clamp(o["width"].toDouble(d.pen.width),0.0,50.0);
    d.pen.style=PenStyle(std::clamp(o["line"].toInt(int(d.pen.style)),0,4));d.tool=Machining(std::clamp(o["tool"].toInt(int(d.tool)),0,2));
    if(d.hasFill){d.fill.color=colourOf(o["fill"],d.fill.color);d.fill.color2=colourOf(o["fill2"],d.fill.color2);
        d.fill.style=FillStyle(std::clamp(o["pattern"].toInt(int(d.fill.style)),0,7));d.fill.gradient=Gradient(std::clamp(o["gradient"].toInt(int(d.fill.gradient)),0,4));}
    if(d.hasFont){d.font=o["font"].toString(d.font);d.strokeFont=o["strokeFont"].toString(d.strokeFont);d.useStrokeFont=o["useStrokeFont"].toBool(d.useStrokeFont);
        d.bold=o["bold"].toBool(d.bold);d.italic=o["italic"].toBool(d.italic);d.fontSize=std::clamp(o["fontSize"].toInt(d.fontSize),0,100000);}
}
// Values from a file: flags 0 or 1, counts within their range, numbers finite and of sensible size.
double sanitise(const S::Info &i,double v){
    if(!std::isfinite(v))return i.value;
    switch(i.kind){
    case S::Info::Flag:return v!=0?1:0;
    case S::Info::Count:return std::clamp(std::round(v),i.low,i.high);
    case S::Info::Real:return std::clamp(v,-10000.0,10000.0);
    }
    return v;
}
// Documents written before the parameters followed the original kept named values; they are carried over as well
// as the two models allow.
S legacyScale(const QJsonObject &o){
    const S::Style s=S::Style(std::clamp(o["style"].toInt(int(S::RoundLinear)),0,int(S::Sine)));S p(s);
    auto r=[&](const char *k,double f){return o[k].toDouble(f);};auto b=[&](const char *k,bool f){return o[k].toBool(f);};
    const QJsonArray div=o["divisions"].toArray(),tick=o["tick"].toArray(),widths=o["tickWidth"].toArray();
    const int n1=div.at(0).toInt(10),n2=div.at(1).toInt(2);const double t1=tick.at(0).toDouble(4),t2=tick.at(1).toDouble(3);
    const double range=r("range",270),height=r("textHeight",3),rotation=normalAngle(r("startAngle",225)-90-range/2);
    QList<double> segs;for(const auto &v:o["segments"].toArray())segs<<v.toDouble();
    const auto in=S::info(s);
    auto set=[&](int i,double v){p.values[i]=sanitise(in[i],v);};
    if(isStraight(s)){
        set(S::StraightLength,r("length",80));set(S::StraightBaseLine,b("baseLine",true));set(S::StraightDivisions1,n1);set(S::StraightTick1,t1);set(S::StraightSecond,n2>1);set(S::StraightDivisions2,std::max(2,n2));set(S::StraightTick2,t2);
        set(S::StraightLabels,b("labels",true));set(S::StraightTextHeight,height);set(S::StraightDistance,r("textDistance",1.5)+height/2);set(S::StraightTextAngle,normalAngle(r("textAngle",0)));set(S::StraightReverse,b("reverse",false));}
    else if(isRound(s)){
        set(S::RoundRange,range);set(S::RoundRadius,r("radius",30));set(S::RoundBaseLine,b("baseLine",true));set(S::RoundCentre,b("centerCross",true));set(S::RoundDivisions1,n1);set(S::RoundTick1,t1);set(S::RoundSecond,n2>1);
        set(S::RoundDivisions2,std::max(2,n2));set(S::RoundTick2,t2);set(S::RoundRotation,rotation);set(S::RoundLabels,b("labels",true));set(S::RoundTextHeight,height);set(S::RoundDistance,r("textDistance",1.5)+height/2);
        set(S::RoundTextAngle,normalAngle(r("textAngle",0)));set(S::RoundReverse,b("reverse",false));set(S::RoundRotateText,b("rotateText",false));}
    else if(s==S::Segments){
        set(S::SegmentRange,range);set(S::SegmentRadius,r("radius",30));set(S::SegmentWidth,r("width",4));set(S::SegmentCount,segs.size()+1);set(S::SegmentCentre,b("centerCross",true));
        for(int k=0;k<9&&k<segs.size();k++)set(S::SegmentLimit1+k,segs[k]*100);
        set(S::SegmentBoundary,b("boundaryLines",true));set(S::SegmentBoundaryLength,t1);set(S::SegmentRotation,rotation);set(S::SegmentLabels,b("labels",true));set(S::SegmentTextHeight,height);
        set(S::SegmentDistance,r("textDistance",1.5)+height/2);set(S::SegmentTextAngle,normalAngle(r("textAngle",0)));set(S::SegmentRotateText,b("rotateText",false));}
    else if(isArc(s)){
        set(S::ArcRange,range);set(S::ArcRadius,r("radius",30));set(S::ArcWidth,s==S::CircleSegment?r("width",4):r("widthEnd",8));set(S::ArcCentre,b("centerCross",true));set(S::ArcRotation,rotation);}
    else if(s==S::Polygon){set(S::PolygonDiameter,2*r("radius",30));set(S::PolygonCorners,o["corners"].toInt(6));set(S::PolygonCentre,b("centerCross",true));set(S::PolygonRotation,normalAngle(r("startAngle",0)));}
    else{set(S::SineHeight,2*r("amplitude",10));set(S::SineWidth,r("length",80));set(S::SineOscillations,o["oscillations"].toInt(3));set(S::SinePhase,normalAngle(r("phase",0)));set(S::SineCentreLine,b("neutralLine",true));}
    for(const auto &t:o["texts"].toArray())p.texts<<t.toString();
    const QColor colour=colourOf(o["color"],Qt::black);const Machining tool=Machining(std::clamp(o["machining"].toInt(0),0,2));
    QList<QColor> segmentColours;for(const auto &c:o["segmentColors"].toArray())segmentColours<<colourOf(c,Qt::gray);
    for(int i=0;i<p.design.size();i++){
        S::Design &d=p.design[i];d.pen.color=colour;d.tool=tool;
        if(d.hasFont){d.font=o["font"].toString(d.font);d.fill.color=colour;}
        if(isArc(s)&&i==0){d.fill.color=colourOf(o["fill"],d.fill.color);d.fill.color2=colourOf(o["fill2"],d.fill.color2);if(s!=S::CircleSegment)d.fill.gradient=Gradient::Horizontal;}
        if(s==S::Segments&&i>=3&&i-3<segmentColours.size())d.fill.color=segmentColours[i-3];
    }
    if(p.design.size()>3&&!isArc(s)&&s!=S::Segments&&s<=S::RoundDots){
        p.design[1].pen.width=widths.at(0).toDouble(0);p.design[2].pen.width=widths.at(1).toDouble(0);p.design[3].pen.width=widths.at(0).toDouble(0);}
    return p;
}
}
QJsonObject S::toJson() const{
    QJsonArray v,t,d;for(double x:values)v.append(x);for(const auto &x:texts)t.append(x);for(const auto &x:design)d.append(designJson(x));
    return {{"style",int(style)},{"values",v},{"texts",t},{"design",d}};
}
S S::fromJson(const QJsonObject &o){
    if(!o.contains("values"))return legacyScale(o);
    S p(Style(std::clamp(o["style"].toInt(int(RoundLinear)),0,int(Sine))));
    const auto in=info(p.style);const QJsonArray v=o["values"].toArray();
    for(int i=0;i<in.size()&&i<v.size();i++)p.values[i]=sanitise(in[i],v[i].toDouble(in[i].value));
    for(const auto &t:o["texts"].toArray())p.texts<<t.toString();
    const QJsonArray d=o["design"].toArray();for(int i=0;i<p.design.size()&&i<d.size();i++)readDesign(p.design[i],d[i].toObject());
    return p;
}

S S::fromScl(const QByteArray &bytes){
    using frontdesigner::IniFile;const IniFile ini=IniFile::parse(bytes);
    S p(Style(std::clamp(ini.integer("Parameter","Style",int(RoundLinear)),0,int(Sine))));
    const auto in=info(p.style);
    for(int i=0;i<in.size();i++){const QString key=QString("Parameter%1").arg(i+1);if(ini.contains("Parameter",key))p.values[i]=sanitise(in[i],ini.real("Parameter",key,in[i].value));}
    const int count=std::clamp(ini.integer("Text","Count",0),0,1000);for(int i=0;i<count;i++)p.texts<<ini.text("Text",QString("Text%1").arg(i));
    if(ini.hasSection("Design")){
        const int n=std::min<int>(std::max(0,ini.integer("Design","Count",0)),p.design.size());
        for(int i=0;i<n;i++){
            Design &d=p.design[i];auto key=[i](const char *k){return QString::fromLatin1(k)+QString::number(i);};
            d.pen.width=std::clamp(ini.integer("Design",key("PenWidth"),0),0,2500)/50.0;d.pen.color=frontdesigner::colourFromDelphi(ini.integer("Design",key("PenColour"),0));
            const int pattern=ini.integer("Design",key("PenPattern"),0);d.pen.style=pattern>=0&&pattern<=4?PenStyle(pattern):PenStyle::Solid;
            d.tool=Machining(std::clamp(ini.integer("Design",key("PenTool"),0),0,2));
            if(d.hasFill){const int style=ini.integer("Design",key("BrushStyle"),0),gradient=ini.integer("Design",key("BrushGradientStyle"),0);
                d.fill.style=style>=0&&style<=7?FillStyle(style):FillStyle::Solid;d.fill.color=frontdesigner::colourFromDelphi(ini.integer("Design",key("BrushColour"),0));
                d.fill.gradient=gradient>=0&&gradient<=4?Gradient(gradient):Gradient::None;d.fill.color2=frontdesigner::colourFromDelphi(ini.integer("Design",key("BrushGradientColour"),0xffffff),Qt::white);}
            if(d.hasFont){d.fontSize=ini.integer("Design",key("FontHeight"),d.fontSize);d.font=ini.text("Design",key("FontName"),d.font);if(d.font.isEmpty())d.font="Arial";
                d.strokeFont=ini.text("Design",key("FontSHXName"));d.useStrokeFont=ini.flag("Design",key("FontSHX"),false);
                d.italic=ini.flag("Design",key("FontItalic"),false);d.bold=ini.flag("Design",key("FontBold"),false);}
        }
    }else if(ini.hasSection("Pen")||ini.hasSection("Font")){
        // Older files: one pen for all strokes and the font of the labels.
        const QColor pen=frontdesigner::colourFromDelphi(ini.integer("Pen","Color",0));const double width=std::clamp(ini.integer("Pen","Width",0),0,2500)/50.0;
        const int pattern=ini.integer("Pen","Style",0);
        for(Design &d:p.design){d.pen.color=pen;d.pen.width=width;d.pen.style=pattern>=0&&pattern<=4?PenStyle(pattern):PenStyle::Solid;
            if(d.hasFont){d.font=ini.text("Font","Name",d.font);d.fill.color=frontdesigner::colourFromDelphi(ini.integer("Font","Color",0));
                d.italic=ini.flag("Font","Italic",false);d.bold=ini.flag("Font","Bold",false);}}
    }
    return p;
}
QByteArray S::toScl() const{
    frontdesigner::IniFile ini;const auto in=info(style);
    ini.setInteger("Parameter","Style",int(style));
    for(int i=0;i<in.size();i++){const QString key=QString("Parameter%1").arg(i+1);const double v=value(i);
        if(in[i].kind==Info::Real)ini.setReal("Parameter",key,v);else ini.setInteger("Parameter",key,qint64(std::llround(v)));}
    const QStringList labels=labelTexts();ini.setInteger("Text","Count",labels.size());for(int i=0;i<labels.size();i++)ini.set("Text",QString("Text%1").arg(i),labels[i]);
    ini.setInteger("Design","Count",design.size());
    for(int i=0;i<design.size();i++){
        const Design &d=design[i];auto key=[i](const char *k){return QString::fromLatin1(k)+QString::number(i);};
        ini.setInteger("Design",key("PenWidth"),std::lround(d.pen.width*50));ini.setInteger("Design",key("PenColour"),frontdesigner::colourToDelphi(d.pen.color));
        ini.setInteger("Design",key("PenPattern"),int(d.pen.style));ini.setInteger("Design",key("PenTool"),int(d.tool));
        if(d.hasFill){ini.setInteger("Design",key("BrushStyle"),int(d.fill.style));ini.setInteger("Design",key("BrushColour"),frontdesigner::colourToDelphi(d.fill.color));
            ini.setInteger("Design",key("BrushGradientStyle"),int(d.fill.gradient));ini.setInteger("Design",key("BrushGradientColour"),frontdesigner::colourToDelphi(d.fill.color2));}
        if(d.hasFont){ini.setInteger("Design",key("FontHeight"),d.fontSize);ini.set("Design",key("FontName"),d.font);ini.set("Design",key("FontSHXName"),d.strokeFont);
            ini.setFlag("Design",key("FontSHX"),d.useStrokeFont);ini.setFlag("Design",key("FontItalic"),d.italic);ini.setFlag("Design",key("FontBold"),d.bold);}
    }
    return ini.toBytes();
}

// ---------------------------------------------------------------- building
Element scale(const S &P,const QTransform &placement){
    const S::Style s=P.style;QList<Element> parts;
    auto v=[&](int i){return P.value(i);};auto on=[&](int i){return P.value(i)!=0;};
    auto look=[&](int i){return i<P.design.size()?P.design[i]:S::Design();};
    const QStringList texts=P.labelTexts();
    if(isStraight(s)||isRound(s)){
        const bool circular=isRound(s),dots=s==S::StraightDots||s==S::RoundDots,log=s==S::StraightLogarithmic||s==S::RoundLogarithmic;
        // Round scales have a few parameters more; the others are the same in both lists, at other positions.
        auto index=[circular](int whenRound,int whenStraight){return circular?whenRound:whenStraight;};
        const int n1=int(v(index(S::RoundDivisions1,S::StraightDivisions1))),n2=int(v(index(S::RoundDivisions2,S::StraightDivisions2)));
        const double t1=v(index(S::RoundTick1,S::StraightTick1)),t2=v(index(S::RoundTick2,S::StraightTick2));
        const bool second=on(index(S::RoundSecond,S::StraightSecond)),labels=on(index(S::RoundLabels,S::StraightLabels)),baseLine=on(index(S::RoundBaseLine,S::StraightBaseLine));
        const double textHeight=v(index(S::RoundTextHeight,S::StraightTextHeight)),distance=v(index(S::RoundDistance,S::StraightDistance)),textAngle=v(index(S::RoundTextAngle,S::StraightTextAngle));
        Course c;c.round=circular;c.reverse=on(index(S::RoundReverse,S::StraightReverse));
        if(circular){
            c.radius=v(S::RoundRadius);c.range=v(S::RoundRange);c.start=90+c.range/2+v(S::RoundRotation);
            // "Low profile": below half a circle the centre moves away from the scale by (p/3)⁴ fiftieths of a millimetre.
            const double flat=v(S::RoundLowProfile);
            if(flat>0&&std::abs(c.range)<180){
                c.flatCentre=direction(v(S::RoundRotation)-90)*(std::pow(flat/3,4)/50);c.flatRadius=QLineF(c.flatCentre,direction(c.start)*c.radius).length();c.flat=true;}
        }else c.length=v(S::StraightLength);
        auto division=[&](const QList<double> &fractions,double size,const S::Design &d){
            QList<Element> list;
            for(double f:fractions){
                if(dots){Element e=newElement(ElementType::Ellipse);e.center=c.at(f);e.radiusX=e.radiusY=std::abs(size)/2;styleShape(e,d);list<<e;}
                else list<<lineElement(c.at(f),c.at(f)+c.normal(f)*size,d);
            }
            if(!list.isEmpty())parts<<group(d.name,list);
        };
        division(marks(log,n1,n2,true),t1,look(1));
        if(second)division(marks(log,n1,n2,false),t2,look(2));
        if(baseLine){
            if(!circular)parts<<lineElement(QPointF(-c.length/2,0),QPointF(c.length/2,0),look(3),look(3).name);
            else{
                Element e=newElement(std::abs(c.range)>=360&&!c.flat?ElementType::Ellipse:ElementType::Arc);e.name=look(3).name;
                if(c.flat){const double a0=angleOf(direction(c.start-c.range)*c.radius-c.flatCentre),a1=angleOf(direction(c.start)*c.radius-c.flatCentre);
                    e.center=c.flatCentre;e.radiusX=e.radiusY=c.flatRadius;e.startAngle=a0;e.spanAngle=normalAngle(a1-a0);}
                else{e.radiusX=e.radiusY=c.radius;e.startAngle=c.start-c.range;e.spanAngle=c.range;}
                styleShape(e,look(3));e.fill.style=FillStyle::None;parts<<e;
            }
        }
        if(circular&&on(S::RoundCentre))parts<<centreMark(c.radius,look(4));
        if(labels){
            QList<Element> list;const QList<double> at=marks(log,n1,n2,true);
            for(int i=0;i<at.size()&&i<texts.size();i++){
                if(texts[i].isEmpty())continue;
                const double f=at[i];double angle=textAngle;if(circular&&on(S::RoundRotateText))angle+=c.angle(f)-90;
                list<<labelElement(texts[i],look(0),textHeight,c.at(f)+c.normal(f)*((dots?0:t1)+distance),angle);
            }
            if(!list.isEmpty())parts<<group(look(0).name,list);
        }
    }else if(s==S::Segments){
        Course c;c.round=true;c.radius=v(S::SegmentRadius);c.range=v(S::SegmentRange);c.start=90+c.range/2+v(S::SegmentRotation);
        const int n=int(v(S::SegmentCount));const double width=v(S::SegmentWidth);
        QList<double> limits{0};for(int k=0;k+1<n;k++)limits<<std::clamp(v(S::SegmentLimit1+k)/100,0.0,1.0);limits<<1;
        if(on(S::SegmentBoundary)){QList<Element> lines;for(double f:limits)lines<<lineElement(c.at(f)+c.normal(f)*width,c.at(f)+c.normal(f)*(width+v(S::SegmentBoundaryLength)),look(1));
            parts<<group(look(1).name,lines);}
        for(int k=0;k+1<limits.size();k++)parts<<band(c,limits[k],limits[k+1],width,width,look(3+k));
        if(on(S::SegmentCentre))parts<<centreMark(c.radius,look(2));
        if(on(S::SegmentLabels)){
            QList<Element> list;
            for(int i=0;i<limits.size()&&i<texts.size();i++){
                if(texts[i].isEmpty())continue;
                const double f=limits[i];double angle=v(S::SegmentTextAngle);if(on(S::SegmentRotateText))angle+=c.angle(f)-90;
                list<<labelElement(texts[i],look(0),v(S::SegmentTextHeight),c.at(f)+c.normal(f)*(width+v(S::SegmentDistance)),angle);
            }
            if(!list.isEmpty())parts<<group(look(0).name,list);
        }
    }else if(isArc(s)){
        Course c;c.round=true;c.radius=v(S::ArcRadius);c.range=v(S::ArcRange);c.start=90+c.range/2+v(S::ArcRotation);const double w=v(S::ArcWidth);
        auto piece=[&](double f0,double f1,double w0,double w1){parts<<band(c,f0,f1,w0,w1,look(0));};
        if(s==S::IncreasingArc)piece(0,1,0,w);
        else if(s==S::DecreasingArc)piece(0,1,w,0);
        else if(s==S::BothSidesArc){piece(0,0.5,w,0);piece(0.5,1,0,w);}
        else piece(0,1,w,w);
        if(on(S::ArcCentre))parts<<centreMark(c.radius,look(1));
    }else if(s==S::Polygon){
        Element e=regularPolygon(QPointF(),int(v(S::PolygonCorners)),v(S::PolygonDiameter)/2,false,v(S::PolygonRotation));styleShape(e,look(0));e.name=look(0).name;parts<<e;
        if(on(S::PolygonCentre))parts<<centreMark(v(S::PolygonDiameter)/2,look(1));
    }else{
        // Eight points per oscillation, drawn as a B-spline like the original does.
        const double height=v(S::SineHeight),width=v(S::SineWidth),k=v(S::SineOscillations),phase=v(S::SinePhase)*degree;const int n=std::max(1,int(std::lround(k*8)));
        Element e=newElement(ElementType::Line);
        for(int i=0;i<=n;i++)e.points<<QPointF(-width/2+width*i/n,-height/2*std::sin(2*std::numbers::pi*k*i/n+phase));
        e.contour=Contour{Corners::Spline,0};styleShape(e,look(0));e.fill.style=FillStyle::None;e.name=look(0).name;parts<<e;
        if(on(S::SineCentreLine))parts<<lineElement(QPointF(-width/2,0),QPointF(width/2,0),look(1),look(1).name);
    }
    Element e=group(scaleStyleTitle(s),parts);e.type=ElementType::Scale;
    for(auto &child:e.children)transformElement(child,placement);
    e.parameters=QJsonObject{{"generator","scale"},{"scale",P.toJson()},{"placement",transformJson(placement)}};
    return e;
}
}
