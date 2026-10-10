#include "openlibrary.h"
#include "assistant.h"
#include "legacy_writer.h"
#include "partbuilder.h"
#include <QBuffer>
#include <QColor>
#include <QGuiApplication>
#include <QJsonArray>
#include <QSet>
#include <QLineF>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QRectF>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>
namespace openloch {
using namespace parts;
namespace {
constexpr double holeMm=2.54;
QJsonObject merged(QJsonObject base,const QJsonObject &over){for(auto it=over.begin();it!=over.end();++it)base[it.key()]=it.value();return base;}
QPointF point(const QJsonValue &v){const auto a=v.toArray();return {a.at(0).toDouble(),a.at(1).toDouble()};}
QJsonArray xy(QPointF p){return {p.x(),p.y()};}
// Colours by name (the LED and body colours of the definitions) or as #rrggbb.
QColor colourOf(const QJsonValue &v,QColor fallback=QColor()){
    static const QMap<QString,QString> names{{"rot","#e3241b"},{"grün","#25c43a"},{"gelb","#ffd319"},{"blau","#2f6dff"},{"weiß","#eef2ff"},{"orange","#ff8a1a"},
        {"infrarot","#4a3d5c"},{"schwarz","#1c1c1e"},{"silber","#c9ccd1"},{"beige","#d9c79e"},{"hellblau","#8fb8de"},{"braun","#8a5a32"}};
    const auto s=v.toString().trimmed();if(s.isEmpty())return fallback;const QColor c(names.value(s.toLower(),s));return c.isValid()?c:fallback;
}
int delphi(QColor c){return c.red()|c.green()<<8|c.blue()<<16;}
QColor mix(QColor a,QColor b,double t){t=qBound(0.0,t,1.0);return QColor::fromRgbF(a.redF()+(b.redF()-a.redF())*t,a.greenF()+(b.greenF()-a.greenF())*t,a.blueF()+(b.blueF()-a.blueF())*t);}

// ---------------------------------------------------------------- pictures
struct Light {double x=-.42,y=-.55,z=.72;};
// Lambert shading with a specular highlight for a surface normal (nx, ny, nz).
QColor lit(QColor base,double nx,double ny,double nz,double gloss,double ambient=.38,double shine=24){
    const Light l;const double len=std::sqrt(l.x*l.x+l.y*l.y+l.z*l.z);const double lx=l.x/len,ly=l.y/len,lz=l.z/len;
    const double diffuse=qMax(0.0,nx*lx+ny*ly+nz*lz);const double rz=2*diffuse*nz-lz;const double spec=std::pow(qMax(0.0,rz),shine)*gloss;
    const double f=ambient+(1-ambient)*diffuse;
    QColor c=QColor::fromRgbF(qMin(1.0,base.redF()*f),qMin(1.0,base.greenF()*f),qMin(1.0,base.blueF()*f));return mix(c,Qt::white,spec);
}
void grain(QImage &image,quint32 seed,double amount){
    if(amount<=0)return;QRandomGenerator random(seed);
    for(int y=0;y<image.height();y++){auto *line=reinterpret_cast<QRgb*>(image.scanLine(y));
        for(int x=0;x<image.width();x++){const double n=(random.generateDouble()-.5)*2*amount;const QColor c(line[x]);
            line[x]=qRgb(qBound(0,int(c.red()*(1+n)),255),qBound(0,int(c.green()*(1+n)),255),qBound(0,int(c.blue()*(1+n)),255));}}
}
// Fine streaks along the x axis for brushed metal.
void brushed(QImage &image,quint32 seed,double amount){
    QRandomGenerator random(seed^0x5bd1e995u);double streak=0;
    for(int y=0;y<image.height();y++){streak=streak*.6+(random.generateDouble()-.5)*amount;auto *line=reinterpret_cast<QRgb*>(image.scanLine(y));
        for(int x=0;x<image.width();x++){const QColor c(line[x]);const double f=1+streak;line[x]=qRgb(qBound(0,int(c.red()*f),255),qBound(0,int(c.green()*f),255),qBound(0,int(c.blue()*f),255));}}
}
QImage renderPicture(const QJsonObject &look,QSize px,quint32 seed){
    QImage image(px,QImage::Format_RGB32);const int w=px.width(),h=px.height();
    const QString form=look["form"].toString("box");const QColor base=colourOf(look["colour"],QColor("#3a3a3c"));const double gloss=look["gloss"].toDouble(.35);
    const QString material=look["material"].toString("plastic");
    auto each=[&](const std::function<QColor(double,double)> &shade){for(int y=0;y<h;y++){auto *line=reinterpret_cast<QRgb*>(image.scanLine(y));for(int x=0;x<w;x++)line[x]=shade((x+.5)/w,(y+.5)/h).rgb();}};
    if(form=="cylinder"){
        // Round body seen from above: the normal turns across the axis.
        const bool vertical=look["axis"].toString("x")=="y";
        each([&](double u,double v){const double t=(vertical?u:v)*2-1;const double n=std::sqrt(qMax(0.0,1-t*t));return vertical?lit(base,t*.95,0,n,gloss):lit(base,0,t*.95,n,gloss);});
    }else if(form=="sphere"||form=="led"||form=="cap"){
        // Domes from above: LED lenses with their reflector cup and chip, ends of standing parts.
        const double flange=form=="led"?look["flange"].toDouble(.12):0;
        each([&](double u,double v){
            const double x=u*2-1,y=v*2-1,r=std::sqrt(x*x+y*y);
            if(r>=1)return base.darker(160);
            if(form=="led"&&r>1-flange){const double t=(r-(1-flange))/flange;return lit(base.darker(115),x/r*.5*t,y/r*.5*t,1,gloss*.4);}
            const double rr=form=="led"?r/(1-flange):r;const double nz=std::sqrt(qMax(0.0,1-rr*rr));
            QColor c=lit(form=="led"?mix(base,Qt::white,.12):base,x*.85,y*.85,qMax(nz,.15),form=="cap"?gloss*.5:gloss+.25,form=="led"?.55:.38,form=="led"?40:24);
            if(form=="led"){
                if(rr<.42)c=mix(c,base.darker(170),.45*(1-rr/.42));                       // reflector cup
                if(std::abs(x)<.07&&std::abs(y)<.07)c=mix(base,QColor(255,250,235),.55);    // chip
                if(std::abs(x+.12)<.03&&y>-.05&&y<.4)c=mix(c,QColor(70,70,70),.35);           // bond wire
            }
            return c;
        });
    }else if(form=="display7"||form=="bars"){
        image.fill(QColor("#2b2b2d"));
    }else if(form=="elko"){
        // Radial electrolytic from above: coloured sleeve at the rim, aluminium top with the scored vent.
        const QColor metal("#c9ccd1");
        each([&](double u,double v){
            const double x=u*2-1,y=v*2-1,r=std::sqrt(x*x+y*y);if(r>=1)return base.darker(160);
            if(r>.84){const double t=(r-.84)/.16;return lit(base,x/r*t*.8,y/r*t*.8,std::sqrt(qMax(0.0,1-t*t*.64)),gloss);}
            return lit(metal,x*.15,y*.15,1,.55,.55,30);
        });
    }else if(form=="pcb"){
        // A module's circuit board from above: matte solder mask, the bare laminate showing at the milled edge.
        const QColor laminate("#cbb98a");const double edge=qMax(1.5,qMin(w,h)*.012);
        each([&](double u,double v){const double e=qMin(qMin(u,1-u)*w,qMin(v,1-v)*h);if(e<edge)return mix(laminate,base,.35);
            const double g=(1-u-v)*.05;QColor c=lit(base,0,0,1,gloss*.3,.75);return QColor::fromRgbF(qBound(0.0,c.redF()+g,1.0),qBound(0.0,c.greenF()+g,1.0),qBound(0.0,c.blueF()+g,1.0));});
    }else if(form=="fins"){
        // Heat sink from above: dark anodised fins across the long side.
        const int n=qMax(2,look["count"].toInt(6));const bool across=w>=h;
        each([&](double u,double v){const double t=(across?u:v)*n;const double f=t-std::floor(t);const double nx=f<.18?-.6:f>.82?.6:0;
            return across?lit(base,nx,0,.8,gloss,.4):lit(base,0,nx,.8,gloss,.4);});
    }else{
        // Flat top with bevelled edges, lit from the upper left.
        const double bevel=qBound(.04,look["bevel"].toDouble(.12),.4);
        each([&](double u,double v){
            const double e=qMin(qMin(u,1-u)*w,qMin(v,1-v)*h)/qMin(w,h);double nx=0,ny=0;
            if(e<bevel){const double k=(bevel-e)/bevel*.7;const double du=qMin(u,1-u)*w,dv=qMin(v,1-v)*h;if(du<dv)nx=u<.5?-k:k;else ny=v<.5?-k:k;}
            const double g=(1-u-v)*.06;QColor c=lit(base,nx,ny,std::sqrt(qMax(0.0,1-nx*nx-ny*ny)),gloss,.55);
            return QColor::fromRgbF(qBound(0.0,c.redF()+g,1.0),qBound(0.0,c.greenF()+g,1.0),qBound(0.0,c.blueF()+g,1.0));
        });
    }
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
    if(form=="display7"){
        // A digit in segments, lit from below like a dark front with pale segments.
        const QColor seg=mix(colourOf(look["colour"],QColor("#e3241b")),Qt::white,.25);const double sx=w*.18,sy=h*.12,len=w*.46,thick=qMin(w,h)*.07;
        // Each segment shortened at both ends, so the digit shows its separate segments like a real display.
        auto bar=[&](double x1,double y1,double x2,double y2){const QLineF l(x1,y1,x2,y2);const double cut=qMin(thick*.9,l.length()*.2);const QPointF d=(l.p2()-l.p1())/l.length()*cut;
            QPen pen(seg,thick,Qt::SolidLine,Qt::FlatCap);p.setPen(pen);p.drawLine(l.p1()+d,l.p2()-d);};
        const double top=sy,mid=h*.5,bottom=h-sy,left=sx,right=sx+len,slant=w*.06;
        bar(left+slant,top,right+slant,top);bar(left+slant/2,mid,right+slant/2,mid);bar(left,bottom,right,bottom);
        bar(left+slant,top,left+slant/2,mid);bar(left+slant/2,mid,left,bottom);bar(right+slant,top,right+slant/2,mid);bar(right+slant/2,mid,right,bottom);
        p.setPen(Qt::NoPen);p.setBrush(seg);p.drawEllipse(QPointF(right+w*.1,bottom),thick*.8,thick*.8);
    }else if(form=="bars"){
        const QColor bar=mix(colourOf(look["colour"],QColor("#e3241b")),Qt::white,.2);const int n=qMax(1,look["count"].toInt(10));
        // Bars side by side along the long side, each across the short side.
        const bool across=w>=h;const double step=(across?w*.8:h*.8)/n,start=(across?w:h)*.1;p.setPen(Qt::NoPen);
        for(int i=0;i<n;i++){const QRectF r=across?QRectF(start+step*i+step*.18,h*.22,step*.64,h*.56):QRectF(w*.22,start+step*i+step*.18,w*.56,step*.64);
            QLinearGradient g(r.topLeft(),across?r.topRight():r.bottomLeft());g.setColorAt(0,bar.lighter(125));g.setColorAt(1,bar.darker(110));p.setBrush(g);p.drawRect(r);}
    }
    // Metal end caps along the long side (SMD chips, fuses) and a metal tab at the top (TO-220).
    if(look["ends"].toDouble()>0){const double f=look["ends"].toDouble();const QColor metal=colourOf(look["ends_colour"],QColor("#c8cbd0"));const bool across=w>=h;
        for(int side=0;side<2;side++){const QRectF r=across?QRectF(side?w*(1-f):0,0,w*f,h):QRectF(0,side?h*(1-f):0,w,h*f);
            QLinearGradient g(r.topLeft(),across?r.bottomLeft():r.topRight());g.setColorAt(0,metal.darker(130));g.setColorAt(.35,metal.lighter(125));g.setColorAt(1,metal.darker(150));p.setPen(Qt::NoPen);p.setBrush(g);p.drawRect(r);}}
    if(look["tab"].toDouble()>0){const double f=look["tab"].toDouble();const QRectF r(0,0,w,h*f);QLinearGradient g(r.topLeft(),r.topRight());const QColor metal("#c4c7cc");
        g.setColorAt(0,metal.darker(125));g.setColorAt(.3,metal.lighter(118));g.setColorAt(1,metal.darker(140));p.setPen(Qt::NoPen);p.setBrush(g);p.drawRect(r);
        const double hr=qMin(double(w),h*f)*look["tab_hole"].toDouble(.18);p.setBrush(QColor(28,28,28));p.drawEllipse(QPointF(w/2.0,h*f*.5),hr,hr);}
    if(look["stripes"].toInt()>0){const int n=look["stripes"].toInt();const bool across=w>=h;p.setPen(QPen(base.darker(150),qMax(1.0,qMin(w,h)/(n*6.0))));
        for(int i=1;i<n;i++){const double t=double(i)/n;if(across)p.drawLine(QPointF(0,h*t),QPointF(w,h*t));else p.drawLine(QPointF(w*t,0),QPointF(w*t,h));}}
    if(form=="elko"&&look.contains("stripe")){
        // The minus stripe on the sleeve, towards the negative lead.
        const QString side=look["stripe"].toString("down");const double angle=side=="up"?90:side=="left"?180:side=="right"?0:270;
        p.setPen(QPen(QColor(225,228,232),qMin(w,h)*.08,Qt::SolidLine,Qt::FlatCap));p.drawArc(QRectF(w*.04,h*.04,w*.92,h*.92),int((angle-35)*16),int(70*16));
        p.setPen(QPen(QColor(150,152,156),qMin(w,h)*.025));const double c=w/2.0,d=h/2.0,a=qMin(w,h)*.22;p.drawLine(QPointF(c-a,d),QPointF(c+a,d));p.drawLine(QPointF(c,d-a),QPointF(c,d+a));
    }
    // Marks at given places (u, v, size as fractions of the picture): contact holes, gold pins, screws, buttons, rotors.
    for(const auto &m:look["marks"].toArray()){
        const auto o=m.toObject();const QPointF c(o["u"].toDouble()*w,o["v"].toDouble()*h);const double rw=o["w"].toDouble(.1)*w/2,rh=o["h"].toDouble(.1)*h/2;const double r=qMin(rw,rh);const auto kind=o["kind"].toString("hole");
        p.setPen(Qt::NoPen);
        if(kind=="hole"){QRadialGradient g(c,r);g.setColorAt(0,QColor(8,8,8));g.setColorAt(.7,QColor(35,35,35));g.setColorAt(1,base.lighter(130));p.setBrush(g);p.drawEllipse(c,r,r);}
        else if(kind=="square"){QRectF q(c.x()-r,c.y()-r,2*r,2*r);p.setBrush(QColor(12,12,12));p.drawRect(q);}
        else if(kind=="pin"){const QColor gold("#d8b048");QRectF q(c.x()-r*.55,c.y()-r*.55,r*1.1,r*1.1);QLinearGradient g(q.topLeft(),q.bottomRight());g.setColorAt(0,gold.lighter(140));g.setColorAt(.5,gold);g.setColorAt(1,gold.darker(150));p.setBrush(base.darker(140));p.drawRect(QRectF(c.x()-r,c.y()-r,2*r,2*r));p.setBrush(g);p.drawRect(q);}
        else if(kind=="screw"||kind=="rotor"){const QColor metal=kind=="screw"?QColor("#c9ccd1"):colourOf(o["colour"],QColor("#f2f2f2"));QRadialGradient g(c-QPointF(r*.3,r*.3),r*1.3);g.setColorAt(0,metal.lighter(130));g.setColorAt(1,metal.darker(160));
            p.setBrush(g);p.drawEllipse(c,r,r);p.setPen(QPen(QColor(40,40,40),r*.22,Qt::SolidLine,Qt::FlatCap));p.drawLine(c-QPointF(r*.7,r*.7),c+QPointF(r*.7,r*.7));if(kind=="rotor")p.drawLine(c-QPointF(-r*.7,r*.7),c+QPointF(-r*.7,r*.7));}
        else if(kind=="button"){const QColor cap=colourOf(o["colour"],QColor("#2a2a2c"));QRadialGradient g(c-QPointF(r*.3,r*.35),r*1.2);g.setColorAt(0,cap.lighter(170));g.setColorAt(.6,cap);g.setColorAt(1,cap.darker(170));p.setBrush(g);p.drawEllipse(c,r,r);}
        else if(kind=="pad"){const QColor tin("#c8c9cc");p.setBrush(tin);p.drawEllipse(c,r,r);}
        // Module parts, sized by w and h separately: plated holes of a header row, chips, shielding cans, USB sockets,
        // SMD LEDs, printed text, the meander of a PCB antenna and slots.
        const QRectF box(c.x()-rw,c.y()-rh,2*rw,2*rh);
        if(kind=="plated"){const QColor ring=colourOf(o["colour"],QColor("#d8b048"));QRadialGradient g(c-QPointF(r*.25,r*.25),r*1.2);g.setColorAt(0,ring.lighter(140));g.setColorAt(1,ring.darker(140));
            p.setBrush(g);p.drawEllipse(c,r,r);p.setBrush(QColor(14,14,14));p.drawEllipse(c,r*.55,r*.55);}
        else if(kind=="chip"){const QColor body=colourOf(o["colour"],QColor("#1c1c1e"));QLinearGradient g(box.topLeft(),box.bottomRight());g.setColorAt(0,body.lighter(150));g.setColorAt(.4,body);g.setColorAt(1,body.darker(130));
            p.setBrush(g);p.drawRoundedRect(box,qMin(rw,rh)*.08,qMin(rw,rh)*.08);p.setBrush(body.lighter(190));const double d=qMin(rw,rh)*.16;p.drawEllipse(box.topLeft()+QPointF(d*1.8,d*1.8),d,d);}
        else if(kind=="shield"||kind=="usb"){const QColor metal=colourOf(o["colour"],QColor("#c9ccd1"));QLinearGradient g(box.topLeft(),box.bottomRight());g.setColorAt(0,metal.lighter(118));g.setColorAt(.5,metal);g.setColorAt(1,metal.darker(135));
            p.setPen(QPen(metal.darker(170),qMax(1.0,qMin(rw,rh)*.04)));p.setBrush(g);p.drawRect(box);
            if(kind=="usb"){p.setPen(Qt::NoPen);p.setBrush(QColor(30,30,30));const bool across=rw>=rh;p.drawRect(across?QRectF(box.left()+rw*.15,c.y()-rh*.25,rw*1.7,rh*.5):QRectF(c.x()-rw*.25,box.top()+rh*.15,rw*.5,rh*1.7));}}
        else if(kind=="led"){const QColor glow=colourOf(o["colour"],QColor("#e3241b"));p.setBrush(QColor("#ecebe6"));p.drawRect(box);p.setBrush(glow);p.drawRect(box.adjusted(rw*.3,rh*.3,-rw*.3,-rh*.3));}
        else if(kind=="print"&&qobject_cast<QGuiApplication*>(QCoreApplication::instance())){p.setPen(colourOf(o["colour"],QColor("#f2f2f2")));QFont f("Arial");f.setPixelSize(qMax(4,int(2*rh*.8)));f.setBold(o["bold"].toBool());p.setFont(f);
            p.save();p.translate(c);p.rotate(o["angle"].toDouble());p.drawText(QRectF(-rw*4,-rh,rw*8,2*rh),Qt::AlignCenter,o["text"].toString());p.restore();}
        else if(kind=="antenna"){const QColor copper=colourOf(o["colour"],QColor("#d9b45a"));const int turns=qMax(2,o["count"].toInt(6));QPainterPath path;const bool across=rw>=rh;
            for(int i=0;i<=turns;i++){const double t=double(i)/turns;if(across){const double x=box.left()+box.width()*t;if(i==0)path.moveTo(x,box.bottom());path.lineTo(x,i%2?box.bottom():box.top());path.lineTo(x,i%2?box.top():box.bottom());}
                else{const double y=box.top()+box.height()*t;if(i==0)path.moveTo(box.left(),y);path.lineTo(i%2?box.right():box.left(),y);path.lineTo(i%2?box.left():box.right(),y);}}
            p.setPen(QPen(copper,qMax(1.0,qMin(box.width(),box.height())*.06)));p.setBrush(Qt::NoBrush);p.drawPath(path);}
        else if(kind=="slot"){p.setBrush(QColor(12,12,12));p.drawRoundedRect(box,qMin(rw,rh),qMin(rw,rh));}
    }
    if(look["dot"].toBool()){p.setPen(Qt::NoPen);p.setBrush(QColor(235,235,235));const double r=qMin(w,h)*.12;p.drawEllipse(QPointF(qMin(w,h)*.3,qMin(w,h)*.3),r,r);}
    if(look["notch"].toBool()){p.setPen(Qt::NoPen);p.setBrush(base.darker(170));const double r=qMin(w,h)*.14;p.drawEllipse(QPointF(w/2.0,0),r,r);}
    if(look["hole"].toDouble()>0){const double r=qMin(w,h)/2.0*look["hole"].toDouble();QRadialGradient g(QPointF(w/2.0,h/2.0),r);g.setColorAt(0,QColor(10,10,10));g.setColorAt(.85,QColor(40,40,40));g.setColorAt(1,base.darker(140));p.setPen(Qt::NoPen);p.setBrush(g);p.drawEllipse(QPointF(w/2.0,h/2.0),r,r);}
    p.end();
    if(material=="metal")brushed(image,seed,.05);
    grain(image,seed,material=="ceramic"?.07:material=="metal"?.02:material=="epoxy"?.035:.02);
    return image;
}

// ---------------------------------------------------------------- expanding package macros
struct Expand {QJsonArray pins,shapes,lines,labels;QJsonObject bands;};
QJsonObject pin(QPointF hole,int kind,const QList<QPointF> &lead={}){QJsonObject p{{"at",xy(hole)},{"kind",kind}};if(!lead.isEmpty()){QJsonArray a;for(auto q:lead)a.append(xy(q));p["lead"]=a;}return p;}
QJsonObject shape(const QString &kind,QRectF r,const QJsonObject &look,double round=0){return {{"kind",kind},{"x",r.x()},{"y",r.y()},{"w",r.width()},{"h",r.height()},{"look",look},{"round",round}};}
QJsonObject line(const QList<QPointF> &points,double width,const QString &colour){QJsonArray a;for(auto q:points)a.append(xy(q));return {{"points",a},{"width",width},{"colour",colour}};}
QJsonObject text(const QString &t,QPointF at,double height,const QString &colour,double angle=0){return {{"text",t},{"at",xy(at)},{"height",height},{"colour",colour},{"angle",angle}};}
QPointF holeMmOf(QPointF hole){return hole*holeMm;}
QList<QPointF> holesOf(const QJsonValue &v){QList<QPointF> out;for(const auto &h:v.toArray())out.append(point(h));return out;}
// Bounding box of points in mm; QRectF's union would skip the empty rectangles of single points.
QRectF boundsOf(const QList<QPointF> &points){
    if(points.isEmpty())return {};double l=points[0].x(),r=l,t=points[0].y(),b=t;
    for(auto p:points){l=qMin(l,p.x());r=qMax(r,p.x());t=qMin(t,p.y());b=qMax(b,p.y());}return QRectF(QPointF(l,t),QPointF(r,b));
}
QList<QPointF> holesMm(const QList<QPointF> &holes){QList<QPointF> out;for(auto h:holes)out.append(holeMmOf(h));return out;}
// Polarity signs beside their pins: outside the body on the side away from it, inside dark on an electrolytic's aluminium
// top, else in the contrast to the body.
void polaritySigns(Expand &e,const QList<QPointF> &holes,QRectF body,const QJsonArray &polarity,const QJsonObject &look,const QColor &bodyColour);
// Lead from a hole straight towards the body edge (body rectangle in mm).
QList<QPointF> leadTo(QPointF hole,QRectF body){
    const QPointF h=holeMmOf(hole);if(body.contains(h))return {};
    const QPointF c=body.center();QPointF edge(qBound(body.left(),h.x(),body.right()),qBound(body.top(),h.y(),body.bottom()));
    if(std::abs(h.x()-c.x())>std::abs(h.y()-c.y())&&h.y()>=body.top()&&h.y()<=body.bottom())edge=QPointF(h.x()<c.x()?body.left():body.right(),h.y());
    else if(h.x()>=body.left()&&h.x()<=body.right())edge=QPointF(h.x(),h.y()<c.y()?body.top():body.bottom());
    return {edge};
}
// look "contacts" (hole, square, pin, screw, button, pad) puts a mark on the picture at every pin inside the body.
void addContacts(QJsonObject &look,const QList<QPointF> &holes,QRectF body,const QJsonObject &d){
    const QString kind=look["contacts"].toString();if(kind.isEmpty()||body.width()<=0||body.height()<=0)return;
    const double size=d["contact_size"].toDouble(kind=="screw"?2.6:kind=="pin"?1.8:kind=="button"?3.0:kind=="pad"?1.4:1.1);
    QJsonArray marks=look["marks"].toArray();
    for(auto h:holes){const QPointF m=holeMmOf(h);if(!body.contains(m))continue;
        marks.append(QJsonObject{{"kind",kind},{"u",(m.x()-body.left())/body.width()},{"v",(m.y()-body.top())/body.height()},{"w",size/body.width()},{"h",size/body.height()}});}
    look["marks"]=marks;
}
QString contrast(QColor c){return c.lightnessF()<.5?"#ffffff":"#000000";}
void defaultLabels(Expand &e,const QJsonObject &d,QRectF body,QColor bodyColour){
    const auto wanted=d["labels"].toArray({"id"});const double h=qBound(1.0,d["label_height"].toDouble(qMin(1.8,qMax(body.height(),body.width())*.4)),2.5);
    double x=body.left();
    for(const auto &w:wanted){
        const auto which=w.toString();const QString t=which=="value"?"<BauteilWertTyp>":which=="name"?"<BauteilName>":"<BauteilKennung>";
        const bool inside=d["labels_inside"].toBool(body.height()>=h*1.8&&body.width()>=h*5.5);
        e.labels.append(text(t,inside?QPointF(x+h*.3,body.top()+h*.3):QPointF(x,body.top()-h-.3),h,inside?contrast(bodyColour):"#000000"));
        x+=h*(t.size()>16?4.5:3.5);
    }
}
Expand expand(const QJsonObject &d,QStringList *problems,const QString &where){
    Expand e;const QString package=d["package"].toString("custom");auto look=d["look"].toObject();const QColor bodyColour=colourOf(look["colour"],QColor("#3a3a3c"));
    auto problem=[&](const QString &m){if(problems)problems->append(where+": "+m);};
    const double leadWidth=d["lead"].toDouble(.6);
    if(package=="axial"){
        // Lying body between two leads; resistors carry the colour code, diodes a cathode ring at pin 2.
        const int pitch=d["pitch"].toInt(4);const bool vertical=d["axis"].toString("x")=="y";const double l=d["length"].toDouble(6.3),dia=d["diameter"].toDouble(2.5);
        const QPointF p2=vertical?QPointF(0,pitch):QPointF(pitch,0);const QPointF c=holeMmOf(p2)/2;
        const QRectF body=vertical?QRectF(c.x()-dia/2,c.y()-l/2,dia,l):QRectF(c.x()-l/2,c.y()-dia/2,l,dia);
        if(!look.contains("form")){look["form"]="cylinder";look["axis"]=vertical?"y":"x";}
        e.pins.append(pin({0,0},9,{vertical?QPointF(0,body.top()):QPointF(body.left(),0)}));e.pins.append(pin(p2,9,{vertical?QPointF(0,body.bottom()):QPointF(body.right(),c.y())}));
        e.shapes.append(shape(d["caps"].toBool()?"axial":d["body"].toString("capsule"),body,look,d["round"].toDouble()));
        if(d.contains("stripe")){const double s=l*.12,at=l/2-l*.17;const QString colour=d["stripe"].toString("#d8d8d8");
            if(vertical)e.lines.append(line({{c.x()-dia/2+.1,c.y()+at},{c.x()+dia/2-.1,c.y()+at}},s,colour));else e.lines.append(line({{c.x()+at,c.y()-dia/2+.1},{c.x()+at,c.y()+dia/2-.1}},s,colour));}
        if(d["bands"].toBool()){QJsonObject b{{"x",body.x()},{"y",body.y()},{"w",body.width()},{"h",body.height()},{"vertical",vertical}};e.bands=b;}
        defaultLabels(e,d,body,bodyColour);
    }else if(package=="standing"){
        // Standing axial part from above: the round end at pin 1, the returning lead to pin 2.
        const int pitch=d["pitch"].toInt(1);const double dia=d["diameter"].toDouble(2.5);const QRectF body(-dia/2,-dia/2,dia,dia);
        if(!look.contains("form"))look["form"]="cap";
        e.pins.append(pin({0,0},11));e.pins.append(pin({0,double(pitch)},9,{QPointF(0,dia/2)}));
        e.shapes.append(shape("circle",body,look));
        // "band": the cathode band at the top end, where the returning lead to pin 2 starts: a ring on the cap from above.
        if(d["band"].toBool()){
            QList<QPointF> ring;const double r=dia/2*.74;for(int k=0;k<=40;k++){const double a=k*2*pi/40;ring.append({std::cos(a)*r,std::sin(a)*r});}
            e.lines.append(line(ring,dia*.18,bodyColour.lightnessF()<.5?"#c9ccd1":"#141414"));
        }
        e.labels.append(text("<BauteilKennung>",QPointF(body.right()+.4,body.top()-.4),1.6,"#000000"));
    }else if(package=="round"||package=="box"){
        // A body from above with its pins: round (LEDs, radial parts) or rectangular.
        auto holes=holesOf(d["pins"]);if(holes.isEmpty())holes={{0,0}};
        const QRectF pinBox=boundsOf(holesMm(holes));
        // Round bodies may be elliptic (flat tantal beads): width and height instead of the diameter.
        const double w=package=="round"&&!d.contains("width")?d["diameter"].toDouble(5):d["width"].toDouble(5),h=package=="round"&&!d.contains("height")?w:d["height"].toDouble(5);
        QPointF centre=d.contains("centre")?point(d["centre"]):pinBox.center();
        const QRectF body(centre.x()-w/2,centre.y()-h/2,w,h);
        if(!look.contains("form"))look["form"]=package=="round"?"sphere":"box";
        addContacts(look,holes,body,d);
        const int kind=d["pin_kind"].toInt(9);const auto polarity=d["polarity"].toArray();
        for(int i=0;i<holes.size();i++){const auto lead=kind==9?leadTo(holes[i],body):QList<QPointF>{};e.pins.append(pin(holes[i],lead.isEmpty()&&kind==9?11:kind,lead));}
        QJsonObject s=shape(package=="round"?"circle":"rect",body,look,d["round"].toDouble(package=="box"?qMin(w,h)*.08:0));
        if(d.contains("flat"))s["flat"]=d["flat"];if(d.contains("flat_cut"))s["flat_cut"]=d["flat_cut"];
        e.shapes.append(s);
        polaritySigns(e,holes,body,polarity,look,bodyColour);
        defaultLabels(e,d,body,bodyColour);
    }else if(package=="lying"){
        // A radial part lying flat (LEDs, electrolytics): body along +x, both leads bent from their holes.
        auto holes=holesOf(d["pins"]);if(holes.size()<2)holes={{0,0},{0,1}};
        const double start=d["start"].toDouble(3.3),l=d["length"].toDouble(8.6),dia=d["diameter"].toDouble(5);
        const double cy=boundsOf(holesMm(holes)).center().y();
        const QRectF body(start,cy-dia/2,l,dia);if(!look.contains("form")){look["form"]="cylinder";look["axis"]="x";}
        for(int i=0;i<holes.size();i++){const QPointF h=holeMmOf(holes[i]);const double y=cy+(h.y()-cy)*.45;e.pins.append(pin(holes[i],9,{QPointF(h.x()+1.2,h.y()),QPointF(start-.6,y),QPointF(start,y)}));}
        QJsonObject s=shape(d["dome"].toBool(true)?"bullet":"rect",body,look);e.shapes.append(s);
        polaritySigns(e,holes,body,d["polarity"].toArray(),look,bodyColour);
        if(d.contains("collar"))e.shapes.append(shape("rect",QRectF(start,cy-dia/2-.25,d["collar"].toDouble(1),dia+.5),QJsonObject{{"form","box"},{"colour",look["colour"]},{"gloss",.3}}));
        defaultLabels(e,d,body,bodyColour);
    }else if(package=="inline"||package=="dil"){
        // Rows of pins on 2.54 mm: SIP and headers (one row), DIL packages and displays (two rows).
        const int count=d["count"].toInt(package=="dil"?8:4);const int row=package=="dil"?d["row"].toInt(3):0;const int perRow=package=="dil"?count/2:count;
        const double margin=d["margin"].toDouble(1),across=d["across"].toDouble(package=="dil"?std::abs(row)*holeMm-2:2.5);
        // "axis": "y" turns the rows downwards (pin 1 on top), as the original's SIP parts stand. "pitch" spaces the
        // pins by more than one hole (2 for 5.08 mm); a negative "row" puts the second row above the first.
        const bool down=d["axis"].toString("x")=="y";const double pitch=d["pitch"].toDouble(1);
        QList<QPointF> holes;for(int i=0;i<perRow;i++)holes.append({i*pitch,0});if(package=="dil")for(int i=perRow-1;i>=0;i--)holes.append({i*pitch,double(row)});
        if(down)for(auto &h:holes)h=QPointF(h.y(),h.x());
        const double length=(perRow-1)*pitch*holeMm+2*margin;const double cy=row*holeMm/2;
        const QRectF body=down?QRectF(cy-across/2,-margin,across,length):QRectF(-margin,cy-across/2,length,across);if(!look.contains("form"))look["form"]="box";
        addContacts(look,holes,body,d);
        for(auto h:holes)e.pins.append(pin(h,11));
        e.shapes.append(shape("rect",body,look,d["round"].toDouble(.2)));
        defaultLabels(e,d,body,bodyColour);
    }else if(package=="to220"){
        // TO-220 and similar tab packages: three pins on 2.54 mm; lying with the body and its metal tab towards -y,
        // or standing, seen from above with the tab behind the body.
        const int count=d["count"].toInt(3);const double width=d["width"].toDouble(10.0),thick=d["thickness"].toDouble(4.5),bodyLength=d["length"].toDouble(9.0),tab=d["tab"].toDouble(6.5);
        QList<QPointF> holes;for(int i=0;i<count;i++)holes.append({double(i),0});const double cx=(count-1)*holeMm/2;
        if(!look.contains("form")){look["form"]="box";look["material"]="epoxy";look["colour"]=look["colour"].toString("#202022");look["gloss"]=.25;}
        if(d["standing"].toBool()){
            const QRectF body(cx-width/2,-thick/2,width,thick);e.shapes.append(shape("rect",QRectF(cx-width/2,body.top()-1.3,width,1.3),QJsonObject{{"form","box"},{"colour","#c4c7cc"},{"material","metal"},{"gloss",.6}}));
            e.shapes.append(shape("rect",body,look,.2));for(auto h:holes)e.pins.append(pin(h,11));defaultLabels(e,d,body,bodyColour);
        }else{
            const double gap=d["gap"].toDouble(2.6);const QRectF body(cx-width/2,-gap-bodyLength-tab,width,bodyLength+tab);
            look["tab"]=tab/(bodyLength+tab);look["tab_hole"]=.2;
            for(auto h:holes){const QPointF m=holeMmOf(h);e.pins.append(pin(h,9,{QPointF(m.x(),-gap)}));}
            e.shapes.append(shape("rect",body,look,.2));
            auto labelled=d;if(!d.contains("labels"))labelled["labels"]=QJsonArray{"id","value"};labelled["labels_inside"]=true;defaultLabels(e,labelled,QRectF(body.left(),body.top()+tab,width,bodyLength),bodyColour);
        }
    }else if(package=="custom"){
        e.pins=d["pins"].toArray();e.shapes=d["shapes"].toArray();e.lines=d["lines"].toArray();e.labels=d["labels"].toArray();
    }else problem("unbekannte Bauform "+package);
    // "pin_labels": each pin's name from "pin_names" beside its hole, as the polarity signs stand: the drawing shows the
    // names the pins carry.
    if(d["pin_labels"].toBool()){
        QList<QPointF> holes;for(const auto &p:e.pins)holes.append(point(p.toObject()["at"]));
        QRectF body;QJsonObject first;for(const auto &v:e.shapes){const auto s=v.toObject();body|=QRectF(s["x"].toDouble(),s["y"].toDouble(),s["w"].toDouble(),s["h"].toDouble());if(first.isEmpty())first=s["look"].toObject();}
        if(body.isEmpty())body=boundsOf(holesMm(holes));
        polaritySigns(e,holes,body,d["pin_names"].toArray(),first.isEmpty()?look:first,colourOf((first.isEmpty()?look:first)["colour"],bodyColour));
    }
    return e;
}
}

namespace {
void polaritySigns(Expand &e,const QList<QPointF> &holes,QRectF body,const QJsonArray &polarity,const QJsonObject &look,const QColor &bodyColour){
    for(int i=0;i<polarity.size()&&i<holes.size();i++){
        const auto sign=polarity[i].toString();if(sign.isEmpty())continue;const QPointF h=holeMmOf(holes[i]);const bool outside=!body.contains(h);QPointF at(h.x()-.5,h.y()-.9);
        if(outside){const QPointF away=h-body.center();if(std::abs(away.x())>=std::abs(away.y()))at=QPointF(away.x()>0?h.x()+.6:h.x()-1.5,h.y()-.6);else at=QPointF(h.x()+.6,away.y()>0?h.y()+.1:h.y()-1.4);}
        e.labels.append(text(sign,at,1.2,outside||look["form"]=="elko"?"#000000":contrast(bodyColour)));
    }
}
}

// ---------------------------------------------------------------- public
int openLibraryGeneratorVersion(){return 8;}
QString openLibraryTitle(const QJsonObject &page,bool english){return english&&page.contains("page_en")?page["page_en"].toString():page["page"].toString();}
QList<QJsonObject> openLibraryParts(const QJsonObject &page,QStringList *problems){
    QList<QJsonObject> out;const auto title=page["page"].toString();
    for(int i=0;i<page["parts"].toArray().size();i++){
        const auto base=page["parts"].toArray()[i].toObject();auto variants=base["variants"].toArray();if(variants.isEmpty())variants.append(QJsonObject{});
        for(int v=0;v<variants.size();v++){
            auto d=merged(base,variants[v].toObject());d.remove("variants");const QString where=QString("%1, %2 (%3)").arg(title,d["name"].toString()).arg(out.size()+1);
            if(d["source"].toString().trimmed().isEmpty()&&problems)problems->append(where+": Quelle fehlt");
            if(d["name"].toString().isEmpty()&&problems)problems->append(where+": Name fehlt");
            const auto e=expand(d,problems,where);
            // Pins sit on holes; parts with real pitches off the 2.54 mm grid (some electrolytics in the original) say so.
            for(const auto &p:e.pins){const auto at=point(p.toObject()["at"]);if(problems&&!d["off_grid"].toBool()&&(at.x()!=std::round(at.x())||at.y()!=std::round(at.y())))problems->append(where+": Anschluss nicht auf dem Raster");}
            // Pin names: one for each pin, none twice.
            if(problems&&d.contains("pin_names")){const auto names=d["pin_names"].toArray();QSet<QString> seen;
                if(names.size()!=e.pins.size())problems->append(where+": pin_names passt nicht zu den Anschlüssen");
                for(const auto &n:names){const auto t=n.toString().trimmed();if(t.isEmpty()||seen.contains(t))problems->append(where+": Anschlussname leer oder doppelt");seen.insert(t);}}
            // A wire pin needs its lead, or it becomes a wire of length zero.
            if(problems&&d["package"].toString()=="custom")for(const auto &p:e.pins){const auto o=p.toObject();if(o["kind"].toInt()==9&&o["lead"].toArray().isEmpty()){problems->append(where+": Anschlussdraht ohne lead");break;}}
            d["pins"]=e.pins;d["shapes"]=e.shapes;d["lines"]=e.lines;d["labels"]=e.labels;if(!e.bands.isEmpty())d["bands"]=e.bands;else d.remove("bands");
            d["package"]="custom";out.append(d);
        }
    }
    return out;
}
QImage openLibraryPicture(const QJsonObject &look,QSizeF size,const QString &seed){
    // 12 pixels per mm up to 25 mm; larger bodies (transformers, heat sinks) at most 300 pixels on the long side, because
    // every placed part carries its picture into the board file. Modules with printed pin names ask for more with "pixels"
    // (up to 960). The same in both directions so round marks stay round.
    const double pxPerMm=qMin(12.0,qBound(64,look["pixels"].toInt(300),960)/qMax(1.0,qMax(size.width(),size.height())));const QSize px(qMax(16,int(std::lround(size.width()*pxPerMm))),qMax(16,int(std::lround(size.height()*pxPerMm))));
    const quint32 s=qHash(seed);return renderPicture(look,px*2,s).scaled(px,Qt::IgnoreAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);
}
QJsonObject openLibraryPart(const QJsonObject &part,QPointF at,bool english){
    auto mm=[&](QPointF p){return QPointF(round(at.x()+p.x()*100),round(at.y()+p.y()*100));};
    QJsonArray leads,pins,shapes,lines,labels;
    // Each lead and pin carries its name in its "label", as projects name the pins of a part (docs/open-libraries.md):
    // from "pin_names", else its number in the order of the pins.
    const auto names=part["pin_names"].toArray();int number=0;
    for(const auto &v:part["pins"].toArray()){
        const auto p=v.toObject();const QPointF hole=mm(holeMmOf(point(p["at"])));++number;
        const QString name=names.size()==part["pins"].toArray().size()?names[number-1].toString():QString::number(number);
        if(p["kind"].toInt()==9){QList<QPointF> path{hole};for(const auto &q:p["lead"].toArray())path.append(mm(point(q)));if(path.size()<2)path.append(hole);
            auto w=wire(9,int(std::lround(part["lead"].toDouble(.6)*100)),delphi(colourOf(p["colour"],QColor(0xC0,0xC0,0xC0))),path);w["label"]=name;leads.append(w);}
        else{auto w=wire(11,50,0,{hole,hole});w["label"]=name;pins.append(w);}
    }
    int index=0;
    for(const auto &v:part["shapes"].toArray()){
        const auto s=v.toObject();const QRectF r(s["x"].toDouble(),s["y"].toDouble(),s["w"].toDouble(),s["h"].toDouble());const auto kind=s["kind"].toString();
        QList<QPointF> outlinePoints;
        if(kind=="circle"||kind=="ellipse"){
            // A fine polygon instead of a smoothed one: exact at every size, flattened where "flat" says.
            const QString flat=s["flat"].toString();const double cut=s["flat_cut"].toDouble(.86);
            const int corners=qBound(36,int(pi*(r.width()+r.height())/2/.8),180);   // about every 0.8 mm, so large rings stay round
            for(int k=0;k<corners;k++){const double a=k*2*pi/corners;QPointF q(std::cos(a),std::sin(a));
                if(flat=="right")q.setX(qMin(q.x(),cut));if(flat=="left")q.setX(qMax(q.x(),-cut));if(flat=="down")q.setY(qMin(q.y(),cut));if(flat=="up")q.setY(qMax(q.y(),-cut));
                outlinePoints.append(mm(r.center()+QPointF(q.x()*r.width()/2,q.y()*r.height()/2)));}
        }else if(kind=="axial"){
            // Resistor body: thicker end caps, thinner middle.
            const double cap=r.width()>r.height()?r.width()*.2:r.height()*.2;const bool across=r.width()>=r.height();
            QList<QPointF> v;
            if(across){const double t=r.height()*.08;v={{r.left(),r.top()},{r.left()+cap,r.top()},{r.left()+cap,r.top()+t},{r.right()-cap,r.top()+t},{r.right()-cap,r.top()},{r.right(),r.top()},
                {r.right(),r.bottom()},{r.right()-cap,r.bottom()},{r.right()-cap,r.bottom()-t},{r.left()+cap,r.bottom()-t},{r.left()+cap,r.bottom()},{r.left(),r.bottom()}};}
            else{const double t=r.width()*.08;v={{r.left(),r.top()},{r.right(),r.top()},{r.right(),r.top()+cap},{r.right()-t,r.top()+cap},{r.right()-t,r.bottom()-cap},{r.right(),r.bottom()-cap},
                {r.right(),r.bottom()},{r.left(),r.bottom()},{r.left(),r.bottom()-cap},{r.left()+t,r.bottom()-cap},{r.left()+t,r.top()+cap},{r.left(),r.top()+cap}};}
            for(auto q:v)outlinePoints.append(mm(q));
        }else if(kind=="capsule"||kind=="bullet"){
            // Rounded ends (glass and plastic bodies), or one round end (LED lying on its side).
            const bool across=r.width()>=r.height();const double rad=(across?r.height():r.width())/2;const int n=8;
            if(across){
                // Left end from top to bottom, right end from bottom to top.
                if(kind=="capsule")for(int k=0;k<=n;k++){const double a=pi/2+k*pi/n;outlinePoints.append(mm({r.left()+rad+std::cos(a)*rad,r.center().y()-std::sin(a)*rad}));}
                else{outlinePoints.append(mm(r.topLeft()));outlinePoints.append(mm(r.bottomLeft()));}
                for(int k=0;k<=n;k++){const double a=-pi/2+k*pi/n;outlinePoints.append(mm({r.right()-rad+std::cos(a)*rad,r.center().y()-std::sin(a)*rad}));}
            }else{
                for(int k=0;k<=n;k++){const double a=pi+k*pi/n;outlinePoints.append(mm({r.center().x()+std::cos(a)*rad,r.top()+rad+std::sin(a)*rad}));}
                for(int k=0;k<=n;k++){const double a=k*pi/n;outlinePoints.append(mm({r.center().x()+std::cos(a)*rad,r.bottom()-rad+std::sin(a)*rad}));}
            }
        }else if(kind=="polygon"){for(const auto &q:s["points"].toArray())outlinePoints.append(mm(point(q)));}
        else for(auto q:QList<QPointF>{r.topLeft(),r.topRight(),r.bottomRight(),r.bottomLeft()})outlinePoints.append(mm(q));
        auto o=wire(kind=="rect"?6:7,10,0,outlinePoints);
        if(s["round"].toDouble()>0)smooth(o,2,s["round"].toDouble()*100);
        const auto look=s["look"].toObject();const QColor colour=colourOf(look["colour"],QColor("#3a3a3c"));
        QByteArray bmp;if(!look.isEmpty()){QBuffer buffer(&bmp);buffer.open(QIODevice::WriteOnly);openLibraryPicture(look,r.size(),part["name"].toString()+QString::number(index)).save(&buffer,"BMP");}
        shapes.append(body(o,delphi(colour),bmp));++index;
    }
    for(const auto &v:part["lines"].toArray()){const auto l=v.toObject();QList<QPointF> path;for(const auto &q:l["points"].toArray())path.append(mm(point(q)));auto w=wire(4,int(std::lround(l["width"].toDouble(.2)*100)),delphi(colourOf(l["colour"],Qt::black)),path);w["flag"]=true;lines.append(w);}
    if(part.contains("bands")){
        // Resistor colour code from the value, four bands across the body like the assistant's.
        const auto b=part["bands"].toObject();const QRectF r(b["x"].toDouble(),b["y"].toDouble(),b["w"].toDouble(),b["h"].toDouble());const bool vertical=b["vertical"].toBool();
        const double ohm=parseOhm(part["value"].toString());QJsonArray bands;const auto colours=colourBands(ohm>0?ohm:10000,1);
        const double length=vertical?r.height():r.width();const QList<double> places{.27,.37,.47,.76};
        for(int k=0;k<4;k++){const double t=places[k]*length;const int c=colours.value(k,-1);const int width=c<0?0:20; // the original sets visible bands to 20 when it loads the part
            auto w=vertical?wire(4,width,c<0?0xFFFF00:c,{mm({r.left()+r.width()*.12,r.top()+t}),mm({r.right()-r.width()*.12,r.top()+t})}):wire(4,width,c<0?0xFFFF00:c,{mm({r.left()+t,r.top()+r.height()*.12}),mm({r.left()+t,r.bottom()-r.height()*.12})});
            w["flag"]=true;bands.append(w);}
        auto code=legacyGroup(bands);code["type"]="TFarbcode";code["id"]="";code["value"]="";code["description"]="";code["group_value"]=0;code["group_flags"]=QJsonArray{false,true};code["flag"]=true;code["bands"]=1;code["resistance"]=ohm>0?ohm:10000;
        lines.append(code);
    }
    for(const auto &v:part["labels"].toArray()){const auto l=v.toObject();const double h=l["height"].toDouble(1.6);
        labels.append(label(l["text"].toString(),mm(point(l["at"])),int(std::lround(h*100)),delphi(colourOf(l["colour"],Qt::black)),l["angle"].toDouble()*pi/180));}
    // Pin 1 must be the first terminal (the anchor and pivot): a pin 1 without lead goes before the leads then.
    const auto pinList=part["pins"].toArray();
    if(!pinList.isEmpty()&&pinList.first().toObject()["kind"].toInt()!=9&&!leads.isEmpty()&&!pins.isEmpty()){leads.prepend(pins.first());pins.removeFirst();}
    QJsonArray children;for(const auto &list:{leads,shapes,lines,pins,labels})for(const auto &c:list)children.append(c);
    for(auto &&c:children){auto n=c.toObject();n["flag"]=true;c=n;}
    auto group=legacyGroup(children);
    group["id"]=part["id"].toString();group["value"]=english&&part.contains("value_en")?part["value_en"].toString():part["value"].toString();
    group["label"]=english&&part.contains("name_en")?part["name_en"].toString():part["name"].toString();
    group["description"]=english&&part.contains("description_en")?part["description_en"].toString():part["description"].toString(group["label"].toString());
    group["group_flags"]=QJsonArray{true,true};group["group_value"]=0;
    return group;
}
QByteArray openLibraryPage(const QJsonObject &page,bool english){
    // Parts in rows on the hole grid, each with room for its body and labels.
    QJsonArray objects;double x=6,y=8,rowHeight=0;
    for(const auto &part:openLibraryParts(page)){
        QRectF box;for(const auto &p:part["pins"].toArray()){const auto o=p.toObject();box|=QRectF(holeMmOf(point(o["at"])),QSizeF(.01,.01));for(const auto &q:o["lead"].toArray())box|=QRectF(point(q),QSizeF(.01,.01));}
        for(const auto &l:part["lines"].toArray())for(const auto &q:l.toObject()["points"].toArray())box|=QRectF(point(q),QSizeF(.01,.01));
        for(const auto &s:part["shapes"].toArray()){const auto o=s.toObject();box|=QRectF(o["x"].toDouble(),o["y"].toDouble(),o["w"].toDouble(),o["h"].toDouble());}
        // Labels as wide as their text will be (placeholders replaced by Kennung, value or name), about 0.6 of the height per character.
        for(const auto &l:part["labels"].toArray()){const auto o=l.toObject();const auto a=point(o["at"]);const double h=o["height"].toDouble(1.6);auto t=o["text"].toString();
            t.replace("<BauteilKennung>",part["id"].toString().replace("#","0")).replace("<BauteilWertTyp>",part["value"].toString()).replace("<BauteilName>",part["name"].toString());
            box|=QRectF(a,QSizeF(qMax(1.0,t.size()*h*.6),h));}
        if(x+box.width()>194&&rowHeight>0){x=6;y+=rowHeight+7;rowHeight=0;}
        const QPointF pin1(std::ceil((x-box.left())/holeMm)*254,std::ceil((y-box.top())/holeMm)*254);
        objects.append(openLibraryPart(part,pin1,english));
        x=pin1.x()/100+box.right()+5;rowHeight=qMax(rowHeight,pin1.y()/100+box.bottom()-y);
    }
    return writeLegacyObjects(objects,openLibraryTitle(page,english),QSizeF(200,qMax(80.0,std::ceil((y+rowHeight+8)/10)*10)));
}
}
