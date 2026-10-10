#include "panelgenerators.h"
#include "panelgeometry.h"
#include "language.h"
#include <QJsonArray>
#include <QLineF>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace openloch::frontpanel {
namespace {
constexpr double degree=std::numbers::pi/180;
QPointF direction(double degrees){return {std::cos(degrees*degree),-std::sin(degrees*degree)};}  // counter-clockwise on screen
QPointF unit(QPointF v){const double l=std::hypot(v.x(),v.y());return l>0?v/l:QPointF(1,0);}
QJsonArray pointJson(QPointF p){return QJsonArray{p.x(),p.y()};}
QPointF pointOf(const QJsonValue &v){const auto a=v.toArray();return {a.at(0).toDouble(),a.at(1).toDouble()};}
QJsonArray transformJson(const QTransform &t){return QJsonArray{t.m11(),t.m12(),t.m21(),t.m22(),t.dx(),t.dy()};}
QJsonValue colourJson(const QColor &c){return c.name(QColor::HexRgb);}
QColor colourOf(const QJsonValue &v,const QColor &fallback){const QColor c=QColor::fromString(v.toString());return c.isValid()?c:fallback;}

Element lineElement(QPointF a,QPointF b,const Pen &pen,Machining machining=Machining::None,const QString &name={}){
    Element e=newElement(ElementType::Line);e.points={a,b};e.pen=pen;e.machining=machining;e.name=name;return e;
}
Element group(ElementType type,const QString &name,QList<Element> children){Element e=newElement(type);e.name=name;e.children=std::move(children);e.pen.style=PenStyle::None;return e;}
// A text whose frame is placed with its point at fractions (fx, fy) of the frame (0 left/top, 0.5 middle, 1 right/bottom)
// on `at`, the baseline turned by `angle` degrees counter-clockwise.
Element textElement(const QString &text,const QString &font,double height,QPointF at,double angle,double fx,double fy,const QColor &colour,Machining machining=Machining::None){
    Element e=newElement(ElementType::Text);e.text=text;e.font=font;e.fill=Fill{FillStyle::Solid,colour,colour,Gradient::None};e.pen.style=PenStyle::None;e.machining=machining;
    if(machining!=Machining::None){e.pen=Pen{colour,0.2,PenStyle::Solid};e.fill.style=FillStyle::None;}
    e.frame={QPointF(0,0),QPointF(1,0),QPointF(0,1)};const double width=naturalTextWidth(e,height);
    const QPointF along=direction(angle),down(-along.y(),along.x());const QPointF p0=at-along*(width*fx)-down*(height*fy);
    e.frame={p0,p0+along*width,p0+down*height};return e;
}
QString number(double v,int decimals){return uiLocale().toString(v,'f',std::max(0,decimals));}
}

Element regularPolygon(QPointF center,int corners,double radius,bool inner,double startAngle){
    corners=std::clamp(corners,3,1000);const double r=inner?radius/std::cos(std::numbers::pi/corners):radius;
    Element e=newElement(ElementType::Polygon);
    for(int i=0;i<corners;i++)e.points<<center+direction(startAngle+360.0*i/corners)*r;
    return e;
}

// ---------------------------------------------------------------- dimensions
QJsonObject DimensionStyle::toJson() const{return {{"color",colourJson(color)},{"lineWidth",lineWidth},{"textHeight",textHeight},{"arrowLength",arrowLength},{"decimals",decimals},{"font",font},{"inch",inch}};}
DimensionStyle DimensionStyle::fromJson(const QJsonObject &o){
    DimensionStyle s;s.color=colourOf(o["color"],s.color);s.lineWidth=std::clamp(o["lineWidth"].toDouble(s.lineWidth),0.0,10.0);s.textHeight=std::clamp(o["textHeight"].toDouble(s.textHeight),0.5,100.0);
    s.arrowLength=std::clamp(o["arrowLength"].toDouble(s.arrowLength),0.2,50.0);s.decimals=std::clamp(o["decimals"].toInt(s.decimals),0,6);s.font=o["font"].toString(s.font);s.inch=o["inch"].toBool();return s;
}
Element dimension(QPointF a,QPointF b,QPointF at,const DimensionStyle &style){
    const QPointF u=unit(b-a),n(-u.y(),u.x());const double length=QLineF(a,b).length(),offset=QPointF::dotProduct(at-a,n);
    const QPointF A=a+n*offset,B=b+n*offset,M=(A+B)/2;const Pen pen{style.color,style.lineWidth,PenStyle::Solid};
    const double head=style.arrowLength,overshoot=2,gap=head*0.5;
    const QString value=number(style.inch?length/25.4:length,style.decimals);
    Element probe=newElement(ElementType::Text);probe.text=value;probe.font=style.font;const double textWidth=naturalTextWidth(probe,style.textHeight);
    // Heads are two strokes at ±22.5° from the tip.
    auto heads=[&](QPointF tip,QPointF back){const QPointF r1=QTransform().rotate(22.5).map(back),r2=QTransform().rotate(-22.5).map(back);
        return QList<Element>{lineElement(tip,tip+r1*head,pen),lineElement(tip,tip+r2*head,pen)};};
    const bool inside=length>=2*head+textWidth+2*gap;
    // The value reads along a → b and stands on the dimension line (its "down" is n).
    const double angle=std::atan2(-u.y(),u.x())/degree;
    QList<Element> left,right;Element text;
    if(inside){
        left={lineElement(A,M,pen)};left+=heads(A,u);right={lineElement(B,M,pen)};right+=heads(B,-u);
        text=textElement(value,style.font,style.textHeight,M-n*(style.textHeight*0.1),angle,0.5,1,style.color);
    }else{
        // Too short for arrows and value between the extension lines: arrows from outside, the value after b.
        left={lineElement(A-u*(2*head),A,pen)};left+=heads(A,-u);
        right={lineElement(B,B+u*(head+gap+textWidth),pen)};right+=heads(B,u);
        text=textElement(value,style.font,style.textHeight,B+u*(head+gap)-n*(style.textHeight*0.1),angle,0,1,style.color);
    }
    const double side=offset>=0?1:-1;
    Element e=group(ElementType::Dimension,ui("Bemaßung"),{
        group(ElementType::Group,ui("Pfeil links"),left),group(ElementType::Group,ui("Pfeil rechts"),right),text,
        lineElement(a,A+n*(side*overshoot),pen,Machining::None,ui("Hilfslinie links")),lineElement(b,B+n*(side*overshoot),pen,Machining::None,ui("Hilfslinie rechts"))});
    e.children[2].name=ui("Maßzahl");e.pen=pen;
    e.parameters=QJsonObject{{"generator","dimension"},{"anchors",QJsonArray{pointJson(at),pointJson(a),pointJson(b)}},{"style",style.toJson()}};
    return e;
}
double dimensionValue(const Element &d){
    const auto a=d.parameters["anchors"].toArray();if(a.size()!=3)return 0;return QLineF(pointOf(a[1]),pointOf(a[2])).length();
}
Element dimensionWithValue(const Element &d,double value){
    const auto a=d.parameters["anchors"].toArray();if(a.size()!=3)return d;
    const QPointF p=pointOf(a[1]),q=pointOf(a[2]);const QPointF u=unit(q-p);
    Element e=dimension(p,p+u*value,pointOf(a[0]),DimensionStyle::fromJson(d.parameters["style"].toObject()));
    e.id=d.id;e.name=d.name;return e;
}

// ---------------------------------------------------------------- cut-outs
QList<std::pair<double,double>> standardCutoutSizes(){return {{24,22.2},{36,33},{48,45},{72,68},{96,92},{144,138},{192,186},{288,282}};}
double standardCutoutFor(double frame){
    const auto sizes=standardCutoutSizes();for(const auto &[f,c]:sizes)if(std::abs(f-frame)<0.01)return c;
    // Between the listed sizes: the same margin as the next larger frame.
    for(const auto &[f,c]:sizes)if(frame<f)return frame*c/f;return frame-6;
}
QJsonObject CutoutParameters::toJson() const{
    return {{"frame",int(frame)},{"cut",int(cut)},{"din",din},{"frameWidth",frameWidth},{"frameHeight",frameHeight},{"cutWidth",cutWidth},{"cutHeight",cutHeight},{"tool",tool},
        {"holesSides",holesSides},{"holesTopBottom",holesTopBottom},{"holesSidesDistance",holesSidesDistance},{"holesTopBottomDistance",holesTopBottomDistance},
        {"holesSidesDiameter",holesSidesDiameter},{"holesTopBottomDiameter",holesTopBottomDiameter},{"name",name}};
}
CutoutParameters CutoutParameters::fromJson(const QJsonObject &o){
    CutoutParameters p;auto real=[&](const char *k,double f){return std::clamp(o[k].toDouble(f),0.0,2000.0);};
    p.frame=Shape(std::clamp(o["frame"].toInt(int(p.frame)),0,2));p.cut=Shape(std::clamp(o["cut"].toInt(int(p.cut)),1,2));p.din=o["din"].toBool(p.din);
    p.frameWidth=real("frameWidth",p.frameWidth);p.frameHeight=real("frameHeight",p.frameHeight);p.cutWidth=real("cutWidth",p.cutWidth);p.cutHeight=real("cutHeight",p.cutHeight);p.tool=real("tool",p.tool);
    p.holesSides=o["holesSides"].toBool();p.holesTopBottom=o["holesTopBottom"].toBool();p.holesSidesDistance=real("holesSidesDistance",p.holesSidesDistance);
    p.holesTopBottomDistance=real("holesTopBottomDistance",p.holesTopBottomDistance);p.holesSidesDiameter=real("holesSidesDiameter",p.holesSidesDiameter);p.holesTopBottomDiameter=real("holesTopBottomDiameter",p.holesTopBottomDiameter);
    p.name=o["name"].toString();return p;
}
CutoutParameters CutoutParameters::fromCut(const QByteArray &ini){
    // Parsed by hand: QSettings would need a file and treats some values specially.
    QMap<QString,QString> values;QString section;
    for(QString line:QString::fromLatin1(ini).split('\n')){
        line=line.trimmed();if(line.startsWith('[')&&line.endsWith(']')){section=line.mid(1,line.size()-2).toLower();continue;}
        const int eq=line.indexOf('=');if(eq>0)values.insert(section+"/"+line.left(eq).trimmed().toLower(),line.mid(eq+1).trimmed());
    }
    auto real=[&](const QString &k,double f){bool ok=false;const double v=QString(values.value(k)).replace(',','.').toDouble(&ok);return ok?std::clamp(v,0.0,2000.0):f;};
    auto flag=[&](const QString &k){return values.value(k).toInt()!=0;};
    CutoutParameters p;p.name=values.value("name/name");
    p.frame=Shape(std::clamp(values.value("frame/style").toInt(),0,2));p.din=flag("frame/din");
    p.frameWidth=p.din?real("frame/dinwidth",96):real("frame/userwidth",p.frameWidth);p.frameHeight=p.din?real("frame/dinheight",96):real("frame/userheight",p.frameHeight);
    p.cut=values.value("cut/style").toInt()==1?Round:Rectangular;p.cutWidth=real("cut/userwidth",p.cutWidth);p.cutHeight=real("cut/userheight",p.cutHeight);p.tool=real("cut/milling",p.tool);
    p.holesSides=flag("holes/horizontal");p.holesTopBottom=flag("holes/vertical");
    p.holesSidesDistance=real("holes/distancex",p.holesSidesDistance);p.holesTopBottomDistance=real("holes/distancey",p.holesTopBottomDistance);
    p.holesSidesDiameter=real("holes/diameterx",p.holesSidesDiameter);p.holesTopBottomDiameter=real("holes/diametery",p.holesTopBottomDiameter);
    if(p.din){p.frame=Rectangular;p.cut=Rectangular;p.cutWidth=standardCutoutFor(p.frameWidth);p.cutHeight=standardCutoutFor(p.frameHeight);}
    return p;
}
QByteArray CutoutParameters::toCut() const{
    auto n=[](double v){return QLocale(QLocale::German).toString(v,'g',8);};
    QString t;t+="[Name]\r\nName="+name+"\r\n[Frame]\r\nStyle="+QString::number(int(frame))+"\r\nUserWidth="+n(frameWidth)+"\r\nUserHeight="+n(frameHeight)
        +"\r\nDINWidth="+n(din?frameWidth:24)+"\r\nDINHeight="+n(din?frameHeight:24)+"\r\nDIN="+(din?"1":"0")+"\r\n[Cut]\r\nStyle="+(cut==Round?"1":"0")
        +"\r\nUserWidth="+n(cutWidth)+"\r\nUserHeight="+n(cutHeight)+"\r\nMilling="+n(tool)+"\r\n[Holes]\r\nVertical="+(holesTopBottom?"1":"0")+"\r\nHorizontal="+(holesSides?"1":"0")
        +"\r\nDistanceX="+n(holesSidesDistance)+"\r\nDistanceY="+n(holesTopBottomDistance)+"\r\nDiameterX="+n(holesSidesDiameter)+"\r\nDiameterY="+n(holesTopBottomDiameter)+"\r\n";
    return t.toLatin1();
}
Element cutout(const CutoutParameters &P,const QTransform &placement){
    QList<Element> parts;const Pen thin{Qt::black,0,PenStyle::Solid};
    double cw=P.cutWidth,ch=P.cutHeight;bool roundCut=P.cut==CutoutParameters::Round;
    if(P.din){cw=standardCutoutFor(P.frameWidth);ch=standardCutoutFor(P.frameHeight);roundCut=false;}
    // The frame shows the instrument's front; DIN instruments always have a rectangular one.
    if(P.frame==CutoutParameters::Round&&!P.din){Element e=newElement(ElementType::Ellipse);e.radiusX=P.frameWidth/2;e.radiusY=P.frameHeight/2;e.pen=thin;e.name=ui("Rahmen");parts<<e;}
    else if(P.frame!=CutoutParameters::None||P.din){Element e=newElement(ElementType::Rectangle);const double w=P.frameWidth/2,h=P.frameHeight/2;e.points={{-w,-h},{w,-h},{w,h},{-w,h}};e.pen=thin;e.name=ui("Rahmen");parts<<e;}
    // The tool runs inside the opening, half its diameter from the edge.
    const double t=std::max(0.0,P.tool),w=std::max(0.0,(cw-t)/2),h=std::max(0.0,(ch-t)/2);
    Element cut=newElement(roundCut?ElementType::Ellipse:ElementType::Rectangle);
    if(roundCut){cut.radiusX=w;cut.radiusY=h;}else cut.points={{-w,-h},{w,-h},{w,h},{-w,h}};
    cut.pen=Pen{QColor(80,80,80),t,PenStyle::Solid};cut.machining=Machining::Mill;cut.name=ui("Ausschnitt");parts<<cut;
    auto hole=[&](QPointF at,double d){Element e=newElement(ElementType::Drill);e.center=at;e.diameter=d;e.name=ui("Befestigungsloch");parts<<e;};
    if(P.holesSides&&!P.din){hole({-P.holesSidesDistance/2,0},P.holesSidesDiameter);hole({P.holesSidesDistance/2,0},P.holesSidesDiameter);}
    if(P.holesTopBottom&&!P.din){hole({0,-P.holesTopBottomDistance/2},P.holesTopBottomDiameter);hole({0,P.holesTopBottomDistance/2},P.holesTopBottomDiameter);}
    Element e=group(ElementType::Cutout,P.name.isEmpty()?ui("Ausschnitt"):P.name,parts);
    for(auto &c:e.children)transformElement(c,placement);
    e.parameters=QJsonObject{{"generator","cutout"},{"cutout",P.toJson()},{"placement",transformJson(placement)}};
    return e;
}

QTransform placementOf(const Element &e){
    const auto a=e.parameters["placement"].toArray();if(a.size()!=6)return {};
    const QTransform t(a[0].toDouble(),a[1].toDouble(),a[2].toDouble(),a[3].toDouble(),a[4].toDouble(),a[5].toDouble());
    // Scales stored before their parameters followed the original started straight scales and sine curves at the
    // reference point; their middle is the reference point now.
    const QJsonObject s=e.parameters["scale"].toObject();
    if(e.parameters["generator"]=="scale"&&!s.contains("values")){
        const int style=s["style"].toInt(1);
        if(style==ScaleParameters::StraightLinear||style==ScaleParameters::StraightLogarithmic||style==ScaleParameters::StraightDots||style==ScaleParameters::Sine)
            return QTransform::fromTranslate(s["length"].toDouble(80)/2,0)*t;
    }
    return t;
}
Element regenerate(const Element &e){
    const QString generator=e.parameters["generator"].toString();Element out=e;
    if(generator=="dimension"){const auto a=e.parameters["anchors"].toArray();if(a.size()!=3)return e;out=dimension(pointOf(a[1]),pointOf(a[2]),pointOf(a[0]),DimensionStyle::fromJson(e.parameters["style"].toObject()));}
    else if(generator=="scale")out=scale(ScaleParameters::fromJson(e.parameters["scale"].toObject()),placementOf(e));
    else if(generator=="cutout")out=cutout(CutoutParameters::fromJson(e.parameters["cutout"].toObject()),placementOf(e));
    else return e;
    out.id=e.id;out.name=e.name;return out;
}
}
