// Tests of the scale assistant's styles: parameter lists, the shapes they build, settings files (.SCL) and documents
// from before the parameters followed the original.
#include "panelgenerators.h"
#include "panelgeometry.h"
#include "delphistream.h"
#include <QJsonArray>
#include <QLineF>
#include <cmath>
#include <numbers>

using namespace openloch::frontpanel;
using S=ScaleParameters;

namespace fptest {
void require(bool ok,const char *message);
bool near(double a,double b,double tolerance);
bool near(QPointF a,QPointF b,double tolerance);
}
using fptest::require;using fptest::near;

namespace {
QPointF dir(double degrees){return {std::cos(degrees*std::numbers::pi/180),-std::sin(degrees*std::numbers::pi/180)};}
const Element *part(const Element &scale,const QString &name){for(const auto &c:scale.children)if(c.name==name)return &c;return nullptr;}
QList<const Element*> parts(const Element &scale,const QString &name){QList<const Element*> out;for(const auto &c:scale.children)if(c.name==name)out<<&c;return out;}
QPointF centreOf(const Element &text){return (text.frame[1]+text.frame[2])/2;}
double heightOf(const Element &text){return QLineF(text.frame[0],text.frame[2]).length();}
S with(S::Style style,const QList<std::pair<int,double>> &values){S p(style);for(const auto &[i,v]:values)p.setValue(i,v);return p;}

void parameterLists(){
    // The parameter lists and parts of the original, style by style.
    const int counts[13]{12,17,12,17,12,17,22,5,5,5,5,4,5},designs[13]{4,5,4,5,4,5,13,2,2,2,2,2,2};
    for(int s=0;s<13;s++){
        const S p{S::Style(s)};
        require(S::info(S::Style(s)).size()==counts[s]&&p.values.size()==counts[s]&&p.design.size()==designs[s],"scale parameter list or parts of a style");
        for(int i=0;i<p.values.size();i++){const auto in=S::info(S::Style(s))[i];require(in.kind!=S::Info::Count||(p.values[i]>=in.low&&p.values[i]<=in.high),"a default count outside its range");}
        const Element e=scale(p,QTransform());require(e.type==ElementType::Scale&&!e.children.isEmpty()&&elementBounds(e).isValid(),"a scale style builds nothing");
    }
    require(S(S::RoundLinear).labelCount()==11&&S(S::StraightDots).labelCount()==7&&S(S::Segments).labelCount()==6&&S(S::Polygon).labelCount()==0,"label positions");
    S t(S::StraightLinear);t.texts={"A","","C"};require(t.labelTexts().size()==11&&t.labelTexts()[0]=="A"&&t.labelTexts()[1].isEmpty()&&t.labelTexts()[3]=="3","label texts");
}

void straightScales(){
    // Factory settings: 50 mm long around the reference point, ten divisions with marks of 4 mm, five sub-divisions of
    // 2 mm, labels of 3 mm whose middle is 2 mm beyond the marks.
    const Element e=scale(S(S::StraightLinear),QTransform());
    const Element *main=part(e,"1. Teilung"),*sub=part(e,"2. Teilung"),*line=part(e,"Lauflinie"),*labels=part(e,"Beschriftung");
    require(main&&sub&&line&&labels&&main->children.size()==11&&sub->children.size()==40&&labels->children.size()==11,"straight scale parts");
    for(int i=0;i<=10;i++){const Element &m=main->children[i];require(near(m.points[0],{-25.0+5*i,0},1e-9)&&near(m.points[1],{-25.0+5*i,-4},1e-9),"straight main marks");}
    require(near(sub->children[0].points[0],{-24,0},1e-9)&&near(sub->children[0].points[1],{-24,-2},1e-9),"straight sub-division marks");
    require(line->type==ElementType::Line&&near(line->points[0],{-25,0},1e-9)&&near(line->points[1],{25,0},1e-9),"straight base line");
    for(int i=0;i<=10;i++){const Element &t=labels->children[i];require(t.text==QString::number(i)&&near(centreOf(t),{-25.0+5*i,-6},1e-9)&&near(heightOf(t),3,1e-9),"straight labels");}
    // Reversed: the first value at the right end.
    const Element r=scale(with(S::StraightLinear,{{S::StraightReverse,1}}),QTransform());require(near(centreOf(part(r,"Beschriftung")->children[0]),{25,-6},1e-9),"reversed straight scale");
    // Logarithmic: one decade, the marks for equal steps of value.
    const Element l=scale(S(S::StraightLogarithmic),QTransform());
    require(near(part(l,"1. Teilung")->children[1].points[0].x(),-25+50*std::log10(1.9),1e-9)&&near(part(l,"2. Teilung")->children[0].points[0].x(),-25+50*std::log10(1.18),1e-9)
        &&near(part(l,"1. Teilung")->children[10].points[0].x(),25,1e-9),"logarithmic marks");
    // Dots: centred on the line, labels the distance from it.
    const Element d=scale(S(S::StraightDots),QTransform());const Element &dot=part(d,"1. Teilung")->children[0];
    require(dot.type==ElementType::Ellipse&&near(dot.center,{-15,0},1e-9)&&near(dot.radiusX,0.15,1e-12)&&near(centreOf(part(d,"Beschriftung")->children[0]),{-15,-2},1e-9),"straight dots");
}

void roundScales(){
    // 270° with 26 mm radius: marks from 26 to 31 mm, labels centred at 33 mm, the first value lower left.
    const S p=with(S::RoundLinear,{{S::RoundRange,270},{S::RoundRadius,26},{S::RoundTick1,5},{S::RoundTick2,2},{S::RoundTextHeight,2.5},{S::RoundDistance,2}});
    const Element e=scale(p,QTransform());const Element *main=part(e,"1. Teilung"),*sub=part(e,"2. Teilung"),*labels=part(e,"Beschriftung");
    require(main&&sub&&labels&&main->children.size()==11&&sub->children.size()==40,"round scale parts");
    require(near(main->children[0].points[0],dir(225)*26,1e-9)&&near(main->children[0].points[1],dir(225)*31,1e-9)&&near(main->children[5].points[1],{0,-31},1e-9)
        &&near(main->children[10].points[0],dir(-45)*26,1e-9),"round main marks");
    require(near(sub->children[0].points[0],dir(225-5.4)*26,1e-9)&&near(sub->children[0].points[1],dir(225-5.4)*28,1e-9),"round sub-division marks");
    require(near(centreOf(labels->children[5]),{0,-33},1e-9)&&near(centreOf(labels->children[0]),dir(225)*33,1e-9)&&near(heightOf(labels->children[0]),2.5,1e-9),"round labels");
    const Element *arc=part(e,"Lauflinie"),*cross=part(e,"Mittelpunkt");
    require(arc&&arc->type==ElementType::Arc&&near(arc->radiusX,26,1e-12)&&near(arc->startAngle,315,1e-9)&&near(arc->spanAngle,270,1e-9),"round base line");
    require(cross&&cross->children.size()==2&&near(cross->children[0].points[0],{-5.2,0},1e-9),"centre mark of a fifth of the radius");
    // Rotation turns the whole scale; the factory range of 120° then starts at 240°.
    const Element turned=scale(with(S::RoundLinear,{{S::RoundRotation,90}}),QTransform());require(near(part(turned,"1. Teilung")->children[0].points[0],dir(240)*25,1e-9),"rotated round scale");
    // Turning labels follow the marks: at the top they stand upright.
    const Element t=scale(with(S::RoundLinear,{{S::RoundRotateText,1}}),QTransform());const Element &first=part(t,"Beschriftung")->children[0],&middle=part(t,"Beschriftung")->children[5];
    require(near(QLineF(first.frame[0],first.frame[1]).angle(),60,1e-6)&&near(QLineF(middle.frame[0],middle.frame[1]).angle(),0,1e-6),"labels turning with the scale");
    // Dots on the circle, labels the distance from it.
    const Element d=scale(S(S::RoundDots),QTransform());const Element &dot=part(d,"1. Teilung")->children[0];
    require(part(d,"1. Teilung")->children.size()==7&&near(dot.center,dir(150)*25,1e-9)&&near(dot.radiusX,0.25,1e-12)&&near(centreOf(part(d,"Beschriftung")->children[0]),dir(150)*29,1e-9),"round dots");
    // "Low profile" 15 %: the centre moves 12.5 mm down, the arc keeps its end points and becomes flatter.
    const Element f=scale(with(S::RoundLinear,{{S::RoundLowProfile,15}}),QTransform());const Element *m=part(f,"1. Teilung");
    const double radius=QLineF(dir(150)*25,QPointF(0,12.5)).length();
    require(near(m->children[0].points[0],dir(150)*25,1e-9)&&near(m->children[10].points[0],dir(30)*25,1e-9)&&near(m->children[5].points[0],{0,12.5-radius},1e-9)
        &&near(m->children[5].points[1],{0,12.5-radius-7},1e-9),"low profile");
    const Element *flatArc=part(f,"Lauflinie");require(near(flatArc->center,{0,12.5},1e-9)&&near(flatArc->radiusX,radius,1e-9),"flattened base line");
    const Element half=scale(with(S::RoundLinear,{{S::RoundLowProfile,15},{S::RoundRange,200}}),QTransform());
    require(near(part(half,"1. Teilung")->children[5].points[0],{0,-25},1e-9),"low profile only below half a circle");
}

void otherStyles(){
    // Coloured segments: band from the inner radius outwards, boundary lines beyond it, labels from the band's edge.
    const S p=with(S::Segments,{{S::SegmentRadius,15},{S::SegmentWidth,2},{S::SegmentBoundaryLength,3},{S::SegmentDistance,6}});
    const Element e=scale(p,QTransform());const Element *lines=part(e,"Grenzlinien"),*labels=part(e,"Beschriftung");
    require(lines&&lines->children.size()==6&&labels&&labels->children.size()==6,"segment scale parts");
    for(int k=0;k<6;k++){const double a=150-24*k;require(near(lines->children[k].points[0],dir(a)*17,1e-9)&&near(lines->children[k].points[1],dir(a)*20,1e-9)&&near(centreOf(labels->children[k]),dir(a)*23,1e-9),"segment limits");}
    // Bands as in the original: sixteen steps along each edge, drawn as a B-spline.
    for(int k=1;k<=5;k++){const auto list=parts(e,QString("%1. Segment").arg(k));require(list.size()==1&&list[0]->type==ElementType::Polygon&&list[0]->points.size()==34
            &&list[0]->contour.corners==Corners::Spline&&near(list[0]->points[1],dir(150-24*(k-1)-1.5)*17,1e-9),"segment bands");
        for(QPointF q:list[0]->points){const double r=std::hypot(q.x(),q.y());require(r>15-1e-9&&r<17+1e-9,"segment band radius");}}
    require(near(part(e,"Mittelpunkt")->children[0].points[0],{-3,0},1e-9),"segment centre mark");
    // Arcs: 90° from 135° to 45°, 15 mm inside, 5 mm wide at the growing end.
    auto outer=[](const Element &band,double angle){double r=0;for(QPointF q:band.points)if(near(std::atan2(-q.y(),q.x())*180/std::numbers::pi,angle,1e-6))r=std::max(r,std::hypot(q.x(),q.y()));return r;};
    const Element up=scale(S(S::IncreasingArc),QTransform()),down=scale(S(S::DecreasingArc),QTransform()),both=scale(S(S::BothSidesArc),QTransform()),ring=scale(S(S::CircleSegment),QTransform());
    require(near(outer(*part(up,"Bogen"),45),20,1e-9)&&near(outer(*part(up,"Bogen"),135),15,1e-9),"increasing arc");
    require(near(outer(*part(down,"Bogen"),135),20,1e-9)&&near(outer(*part(down,"Bogen"),45),15,1e-9),"decreasing arc");
    const auto halves=parts(both,"Bogen");require(halves.size()==2&&near(outer(*halves[0],135),20,1e-9)&&near(outer(*halves[0],90),15,1e-9)&&near(outer(*halves[1],45),20,1e-9),"arc growing to both sides");
    require(near(outer(*part(ring,"Bogen"),45),17,1e-9)&&near(outer(*part(ring,"Bogen"),135),17,1e-9),"circle segment");
    // Polygon: corners on the outer diameter, the first one at the rotation angle.
    const Element poly=scale(with(S::Polygon,{{S::PolygonRotation,30}}),QTransform());const Element *shape=part(poly,"Polygon");
    require(shape&&shape->points.size()==6&&near(shape->points[0],dir(30)*15,1e-9)&&near(part(poly,"Mittelpunkt")->children[0].points[0],{-3,0},1e-9),"regular polygon");
    // Sine: eight points per oscillation as B-spline, height from peak to peak, centred on the reference point.
    const Element sine=scale(with(S::Sine,{{S::SinePhase,90}}),QTransform());const Element *curve=part(sine,"Kurve"),*axis=part(sine,"Mittellinie");
    require(curve&&curve->points.size()==9&&curve->contour.corners==Corners::Spline&&near(curve->points[0],{-25,-15},1e-9)&&near(curve->points[4],{0,15},1e-9),"sine curve");
    require(axis&&near(axis->points[0],{-25,0},1e-9)&&near(axis->points[1],{25,0},1e-9),"sine centre line");
}

void settingsFiles(){
    // Through the original's settings format and back.
    S p(S::Segments);p.setValue(S::SegmentCount,7);p.setValue(S::SegmentLimit1+2,55.5);p.setValue(S::SegmentRotation,12.25);
    p.texts={"Aus","1","2","3","4","5","6","Ende"};
    p.design[0].useStrokeFont=true;p.design[0].strokeFont="DIN1451";p.design[0].bold=true;p.design[0].font="Verdana";p.design[0].fontSize=150;
    p.design[3].fill=Fill{FillStyle::Solid,QColor(200,10,20),QColor(250,250,0),Gradient::Vertical};p.design[1].pen=Pen{QColor(0,0,255),0.3,PenStyle::Dash};p.design[1].tool=Machining::Engrave;
    const QByteArray scl=p.toScl();const S q=S::fromScl(scl);
    require(scl.startsWith("[Parameter]\r\nStyle=6\r\nParameter1=120\r\n")&&scl.contains("Parameter8=55,5\r\n")&&scl.contains("[Text]\r\nCount=8\r\nText0=Aus\r\n"),"scale settings written");
    require(q.style==p.style&&q.values==p.values&&q.labelTexts()==p.labelTexts()&&q.design==p.design,"scale settings do not round-trip");
    require(S::fromJson(p.toJson()).toJson()==p.toJson(),"scale parameters do not survive storing");
    // A file of the original's kind: decimal commas, Windows-1252 text, a parameter missing at the end, design with
    // gradient and stroke font.
    QByteArray own="[Parameter]\r\nStyle=5\r\nParameter1=300\r\nParameter2=12\r\nParameter3=0\r\nParameter4=1\r\nParameter5=8\r\nParameter6=0,4\r\nParameter7=0\r\nParameter8=4\r\n"
        "Parameter9=0,15\r\nParameter10=150\r\nParameter11=1\r\nParameter12=3,5\r\nParameter13=2,5\r\nParameter14=0\r\nParameter15=1\r\nParameter16=0\r\n"
        "[Text]\r\nCount=9\r\nText0=\r\nText1=K\xe4lte\r\nText2=B\r\nText9=unused\r\n"
        "[Design]\r\nCount=5\r\nPenWidth0=10\r\nPenColour0=255\r\nPenPattern0=4\r\nPenTool0=2\r\nBrushStyle0=0\r\nBrushColour0=65280\r\nBrushGradientStyle0=2\r\n"
        "BrushGradientColour0=16711680\r\nFontHeight0=175\r\nFontName0=Verdana\r\nFontSHXName0=ISO3098\r\nFontSHX0=1\r\nFontItalic0=0\r\nFontBold0=1\r\n"
        "PenWidth1=5\r\nPenColour1=0\r\nPenPattern1=0\r\nPenTool1=1\r\nBrushStyle1=1\r\nBrushColour1=16777215\r\n";
    const S r=S::fromScl(own);
    require(r.style==S::RoundDots&&r.values[S::RoundRange]==300&&near(r.values[S::RoundTick1],0.4,1e-12)&&r.values[S::RoundRotation]==150&&r.values[S::RoundReverse]==1
        &&r.values[S::RoundRotateText]==0&&r.texts.size()==9&&r.texts[0].isEmpty()&&r.texts[1]==QString::fromUtf8("Kälte"),"reading scale settings");
    const S::Design &label=r.design[0],&first=r.design[1];
    require(label.pen.color==QColor(255,0,0)&&near(label.pen.width,0.2,1e-12)&&label.pen.style==PenStyle::None&&label.tool==Machining::Engrave&&label.fill.color==QColor(0,255,0)
        &&label.fill.gradient==Gradient::Vertical&&label.fill.color2==QColor(0,0,255)&&label.font=="Verdana"&&label.strokeFont=="ISO3098"&&label.useStrokeFont&&label.bold&&!label.italic
        &&label.fontSize==175,"reading the design of the labels");
    require(near(first.pen.width,0.1,1e-12)&&first.tool==Machining::Mill&&first.fill.style==FillStyle::None&&r.design[4].pen.color==Qt::black,"reading the design of the divisions");
    // Older files: one pen and the font of the labels.
    const S o=S::fromScl("[Parameter]\nStyle=1\nParameter1=360\n[Font]\nName=Times New Roman\nColor=255\nItalic=1\nBold=0\n[Pen]\nColor=32768\nWidth=25\nStyle=0\n");
    require(o.style==S::RoundLinear&&o.values[S::RoundRange]==360&&o.values[S::RoundRadius]==25&&o.design[2].pen.color==QColor(0,128,0)&&near(o.design[2].pen.width,0.5,1e-12)
        &&o.design[0].font=="Times New Roman"&&o.design[0].fill.color==QColor(255,0,0)&&o.design[0].italic,"reading older scale settings");
    require(S::fromScl("[Parameter]\nStyle=99\n").style==S::Sine&&S::fromScl("nothing").style==S::RoundLinear,"scale settings out of range");
}

void olderDocuments(){
    // Documents from before: named values are carried over, straight scales keep their place.
    const QJsonObject round{{"style",1},{"radius",20},{"range",180},{"startAngle",180},{"divisions",QJsonArray{8,2,1}},{"tick",QJsonArray{3,2,1}},{"texts",QJsonArray{"a","b"}},{"color","#ff0000"}};
    const S r=S::fromJson(round);
    require(r.style==S::RoundLinear&&r.values[S::RoundRange]==180&&r.values[S::RoundRadius]==20&&r.values[S::RoundRotation]==0&&r.values[S::RoundDivisions1]==8&&r.values[S::RoundSecond]==1
        &&r.values[S::RoundDivisions2]==2&&r.texts==QStringList{"a","b"}&&r.design[1].pen.color==QColor(255,0,0),"older scale parameters");
    Element old=newElement(ElementType::Scale);old.parameters=QJsonObject{{"generator","scale"},{"scale",QJsonObject{{"style",0},{"length",60}}},{"placement",QJsonArray{1,0,0,1,10,20}}};
    require(placementOf(old).map(QPointF())==QPointF(40,20),"an older straight scale moves");
    const Element rebuilt=regenerate(old);require(rebuilt.type==ElementType::Scale&&near(part(rebuilt,"Lauflinie")->points[0],{10,20},1e-9),"an older straight scale is rebuilt in place");
}
}

void runScaleTests(){parameterLists();straightScales();roundScales();otherStyles();settingsFiles();olderDocuments();}
