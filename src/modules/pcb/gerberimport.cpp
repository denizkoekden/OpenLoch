#include "gerberimport.h"
#include "language.h"
#include "legacy_reader.h"
#include <QPainter>
#include <QPainterPathStroker>
#include <QRegularExpression>
#include <QTransform>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <optional>

namespace openloch::pcb {
namespace {
[[noreturn]] void broken(int line,const QString &what){throw FormatError(ui("Die Gerber-Datei ist fehlerhaft (Zeile %1): %2").arg(line).arg(what));}
double signedArea(const QPolygonF &p){double a=0;for(qsizetype i=0;i<p.size();i++){const QPointF u=p[i],v=p[(i+1)%p.size()];a+=u.x()*v.y()-v.x()*u.y();}return a/2;}
// Points of an arc from `from` to `to` about `centre` (y upwards), the start left out; a full circle when `full`.
void appendArc(QPolygonF &out,QPointF from,QPointF to,QPointF centre,bool clockwise,bool full){
    const double r=std::hypot(from.x()-centre.x(),from.y()-centre.y());
    const double a0=std::atan2(from.y()-centre.y(),from.x()-centre.x()),a1=std::atan2(to.y()-centre.y(),to.x()-centre.x());
    double sweep=a1-a0;
    if(clockwise){while(sweep>=0)sweep-=2*M_PI;if(full)sweep=-2*M_PI;}else{while(sweep<=0)sweep+=2*M_PI;if(full)sweep=2*M_PI;}
    // Chords no more than a micrometre off the arc.
    const double step=r>.001?2*std::acos(std::max(-1.0,1-.001/r)):M_PI/4;
    const int n=std::clamp(int(std::ceil(std::abs(sweep)/std::max(step,1e-6))),1,4000);
    for(int k=1;k<=n;k++){const double a=a0+sweep*k/n;out.append(k==n&&!full?to:centre+QPointF(r*std::cos(a),r*std::sin(a)));}
}
QPolygonF convexHull(QList<QPointF> points){
    std::sort(points.begin(),points.end(),[](QPointF a,QPointF b){return a.x()<b.x()||(a.x()==b.x()&&a.y()<b.y());});
    auto cross=[](QPointF o,QPointF a,QPointF b){return (a.x()-o.x())*(b.y()-o.y())-(a.y()-o.y())*(b.x()-o.x());};
    QList<QPointF> hull;
    for(int pass=0;pass<2;pass++){
        const qsizetype start=hull.size();
        for(qsizetype k=0;k<points.size();k++){const QPointF p=pass?points[points.size()-1-k]:points[k];
            while(hull.size()>=start+2&&cross(hull[hull.size()-2],hull.last(),p)<=0)hull.removeLast();hull.append(p);}
        hull.removeLast();
    }
    return QPolygonF(hull);
}

// Aperture macros: statements of primitives and variable definitions, evaluated when an aperture uses the macro.
class Expression {
public:
    Expression(const QString &text,const QMap<int,double> &variables):s(text),vars(variables){}
    double value(int line){const double v=sum(line);if(i!=s.size())broken(line,ui("unverständlicher Ausdruck „%1“").arg(s));return v;}
private:
    QString s;const QMap<int,double> &vars;qsizetype i=0;
    QChar peek(){while(i<s.size()&&s[i].isSpace())i++;return i<s.size()?s[i]:QChar();}
    double sum(int line){double v=product(line);for(;;){const QChar c=peek();if(c=='+'){i++;v+=product(line);}else if(c=='-'){i++;v-=product(line);}else return v;}}
    double product(int line){double v=factor(line);for(;;){const QChar c=peek();if(c=='x'||c=='X'){i++;v*=factor(line);}else if(c=='/'){i++;const double d=factor(line);v=d==0?0:v/d;}else return v;}}
    double factor(int line){
        const QChar c=peek();
        if(c=='-'){i++;return -factor(line);}if(c=='+'){i++;return factor(line);}
        if(c=='('){i++;const double v=sum(line);if(peek()!=')')broken(line,ui("unverständlicher Ausdruck „%1“").arg(s));i++;return v;}
        if(c=='$'){i++;const qsizetype b=i;while(i<s.size()&&s[i].isDigit())i++;return vars.value(s.mid(b,i-b).toInt(),0);}
        const qsizetype b=i;while(i<s.size()&&(s[i].isDigit()||s[i]=='.'))i++;
        bool ok=false;const double v=s.mid(b,i-b).toDouble(&ok);if(!ok)broken(line,ui("unverständlicher Ausdruck „%1“").arg(s));return v;
    }
};
struct Macro {QStringList statements;};
QPainterPath rotated(const QPainterPath &p,double degrees){return degrees==0?p:QTransform().rotate(degrees).map(p);}
QPainterPath polygonPath(const QPolygonF &poly){QPainterPath p;p.addPolygon(poly);p.closeSubpath();return p;}
// The shape of a macro with its parameters, in the file's units.
QPainterPath macroShape(const Macro &macro,const QList<double> &parameters,int line){
    QMap<int,double> vars;for(int k=0;k<parameters.size();k++)vars[k+1]=parameters[k];
    QPainterPath shape;shape.setFillRule(Qt::WindingFill);
    for(const auto &raw:macro.statements){
        const QString st=raw.trimmed();if(st.isEmpty()||st.startsWith('0'))continue;   // comments start with 0
        if(st.startsWith('$')){const qsizetype eq=st.indexOf('=');if(eq<0)broken(line,ui("unverständlicher Ausdruck „%1“").arg(st));
            vars[st.mid(1,eq-1).toInt()]=Expression(st.mid(eq+1),vars).value(line);continue;}
        QList<double> v;for(const auto &part:st.split(','))v.append(Expression(part,vars).value(line));
        const int code=int(v.value(0));auto at=[&](int k){return v.value(k);};
        QPainterPath p;bool exposure=true;
        switch(code){
        case 1:{exposure=at(1)!=0;const double r=at(2)/2;p.addEllipse(QPointF(at(3),at(4)),r,r);p=rotated(p,at(5));break;}
        case 2:case 20:{exposure=at(1)!=0;const QPointF a(at(3),at(4)),b(at(5),at(6));QPointF d=b-a;const double l=std::hypot(d.x(),d.y());
            if(l>0){d/=l;const QPointF n(-d.y()*at(2)/2,d.x()*at(2)/2);p=polygonPath(QPolygonF{a+n,b+n,b-n,a-n});}p=rotated(p,at(7));break;}
        case 21:{exposure=at(1)!=0;p.addRect(QRectF(at(4)-at(2)/2,at(5)-at(3)/2,at(2),at(3)));p=rotated(p,at(6));break;}
        case 22:{exposure=at(1)!=0;p.addRect(QRectF(at(4),at(5),at(2),at(3)));p=rotated(p,at(6));break;}
        case 4:{exposure=at(1)!=0;const int n=int(at(2));QPolygonF poly;for(int k=0;k<=n;k++)poly.append(QPointF(at(3+2*k),at(4+2*k)));p=rotated(polygonPath(poly),at(5+2*n));break;}
        case 5:{exposure=at(1)!=0;const int n=std::max(3,int(at(2)));QPolygonF poly;
            for(int k=0;k<n;k++){const double a=2*M_PI*k/n;poly.append(QPointF(at(3)+at(5)/2*std::cos(a),at(4)+at(5)/2*std::sin(a)));}p=rotated(polygonPath(poly),at(6));break;}
        case 6:{const QPointF c(at(1),at(2));double outer=at(3);const double ring=at(4),gap=at(5);const int rings=int(at(6));
            for(int k=0;k<rings&&outer>0;k++){QPainterPath o;o.addEllipse(c,outer/2,outer/2);QPainterPath in;const double ir=outer/2-ring;if(ir>0)in.addEllipse(c,ir,ir);p=p.united(o.subtracted(in));outer-=2*(ring+gap);}
            QPainterPath cross;cross.addRect(QRectF(c.x()-at(8)/2,c.y()-at(7)/2,at(8),at(7)));cross.addRect(QRectF(c.x()-at(7)/2,c.y()-at(8)/2,at(7),at(8)));
            p=rotated(p.united(cross.simplified()),at(9));break;}
        case 7:{const QPointF c(at(1),at(2));QPainterPath o;o.addEllipse(c,at(3)/2,at(3)/2);QPainterPath in;in.addEllipse(c,at(4)/2,at(4)/2);
            QPainterPath gaps;gaps.addRect(QRectF(c.x()-at(3),c.y()-at(5)/2,2*at(3),at(5)));gaps.addRect(QRectF(c.x()-at(5)/2,c.y()-at(3),at(5),2*at(3)));
            p=rotated(o.subtracted(in).subtracted(gaps.simplified()),at(6));break;}
        default:broken(line,ui("unbekanntes Makro-Element %1").arg(code));
        }
        shape=exposure?shape.united(p):shape.subtracted(p);
    }
    return shape;
}
QPainterPath standardShape(char kind,const QList<double> &v){
    QPainterPath p;const double w=v.value(0),h=v.value(1,w);
    switch(kind){
    case 'C':p.addEllipse(QPointF(),w/2,w/2);break;
    case 'R':p.addRect(QRectF(-w/2,-h/2,w,h));break;
    case 'O':{const double r=std::min(w,h)/2;p.addRoundedRect(QRectF(-w/2,-h/2,w,h),r,r);break;}
    case 'P':{const int n=std::max(3,int(v.value(1)));QPolygonF poly;
        for(int k=0;k<n;k++){const double a=qDegreesToRadians(v.value(2))+2*M_PI*k/n;poly.append(QPointF(w/2*std::cos(a),w/2*std::sin(a)));}p=polygonPath(poly);break;}
    }
    return p;
}

class Parser {
public:
    explicit Parser(const QByteArray &bytes):data(bytes){}
    GerberData run(){
        for(qsizetype i=0;i<data.size()&&!ended;){
            const char c=data[i];
            if(c=='\n'){line++;i++;continue;}
            if(c=='\r'||c==' '||c=='\t'){i++;continue;}
            if(c=='%'){
                const qsizetype j=data.indexOf('%',i+1);if(j<0)broken(line,ui("ein %-Block ohne Ende"));
                const QByteArray block=data.mid(i+1,j-i-1);extended(block);line+=int(block.count('\n'));i=j+1;continue;
            }
            // A word ends with *; some programs leave the * out before an extended command.
            qsizetype j=data.indexOf('*',i);const qsizetype percent=data.indexOf('%',i);
            if(percent>=0&&(j<0||percent<j)){const QByteArray word=data.mid(i,percent-i);const int here=line;line+=int(word.count('\n'));command(QString::fromLatin1(word).simplified().remove(' '),here);i=percent;continue;}
            if(j<0){if(data.mid(i).trimmed().isEmpty())break;broken(line,ui("ein Befehl ohne * am Ende"));}
            const QByteArray word=data.mid(i,j-i);const int here=line;line+=int(word.count('\n'));command(QString::fromLatin1(word).simplified().remove(' '),here);i=j+1;
        }
        if(region)broken(line,ui("eine Region ohne Ende"));
        if(repeating)endRepeat();
        return out;
    }
private:
    const QByteArray &data;GerberData out;
    int line=1;bool ended=false;
    int xInt=2,xDec=4,yInt=2,yDec=4;bool formatSet=false,trailing=false,incremental=false;
    double unit=25.4;                   // millimetres per unit of the file: inches until said otherwise
    int mode=1;std::optional<bool> multi;
    bool region=false;QList<QPolygonF> contours;QPolygonF contour;
    bool dark=true,mirrorX=false,mirrorY=false;double rotation=0,scale=1;
    int aperture=-1,operation=0;QPointF at;
    QMap<QString,Macro> macros;
    bool repeating=false;int repeatX=1,repeatY=1;double stepX=0,stepY=0;qsizetype repeatStart=0;

    double coordinate(const QString &text,bool x){
        if(text.contains('.'))return text.toDouble()*unit;
        if(!formatSet)broken(line,ui("Koordinaten ohne Angabe des Formats (FS)"));
        QString digits=text;bool negative=false;if(digits.startsWith('-')||digits.startsWith('+')){negative=digits.startsWith('-');digits.remove(0,1);}
        const int integers=x?xInt:yInt,decimals=x?xDec:yDec;
        if(trailing)digits=digits.leftJustified(integers+decimals,'0');
        return (negative?-1:1)*digits.toLongLong()/std::pow(10.0,decimals)*unit;
    }
    void extended(const QByteArray &block){
        QStringList parts=QString::fromLatin1(block).split('*');for(auto &p:parts)p=p.trimmed().remove('\n').remove('\r');
        if(!parts.isEmpty()&&parts.last().isEmpty())parts.removeLast();
        if(parts.isEmpty())return;
        if(parts[0].startsWith("AM")){Macro m;m.statements=parts.mid(1);macros[parts[0].mid(2)]=m;return;}
        for(const auto &p:parts){
            if(p.startsWith("FS")){
                static const QRegularExpression fs("^FS([LTD]?)([AI]?)(?:N\\d)?(?:G\\d)?X(\\d)(\\d)Y(\\d)(\\d)");
                const auto m=fs.match(p);if(!m.hasMatch())broken(line,ui("unverständliches Format „%1“").arg(p));
                trailing=m.captured(1)=="T";incremental=m.captured(2)=="I";
                xInt=m.captured(3).toInt();xDec=m.captured(4).toInt();yInt=m.captured(5).toInt();yDec=m.captured(6).toInt();formatSet=true;
            }else if(p=="MOMM")unit=1;else if(p=="MOIN")unit=25.4;
            else if(p=="LPD")dark=true;else if(p=="LPC")dark=false;
            else if(p.startsWith("LM")){mirrorX=p.contains('X');mirrorY=p.contains('Y');}
            else if(p.startsWith("LR"))rotation=p.mid(2).toDouble();
            else if(p.startsWith("LS"))scale=p.mid(2).toDouble();
            else if(p.startsWith("AD"))define(p.mid(2));
            else if(p.startsWith("SR")){
                if(repeating)endRepeat();
                static const QRegularExpression sr("^SR(?:X(\\d+))?(?:Y(\\d+))?(?:I([-0-9.]+))?(?:J([-0-9.]+))?$");const auto m=sr.match(p);
                if(m.hasMatch()&&p.size()>2){repeatX=std::max(1,m.captured(1).isEmpty()?1:m.captured(1).toInt());repeatY=std::max(1,m.captured(2).isEmpty()?1:m.captured(2).toInt());
                    stepX=m.captured(3).toDouble()*unit;stepY=m.captured(4).toDouble()*unit;repeating=repeatX>1||repeatY>1;repeatStart=out.objects.size();}
            }else if(p.startsWith("AB"))broken(line,ui("Blendenblöcke (AB) werden nicht unterstützt"));
            // Attributes, names, image polarity and the old image transformations change nothing here.
        }
    }
    void define(const QString &text){
        static const QRegularExpression ad("^D(\\d+)([^,]+)(?:,(.*))?$");const auto m=ad.match(text);if(!m.hasMatch())broken(line,ui("unverständliche Blende „%1“").arg(text));
        const int code=m.captured(1).toInt();const QString name=m.captured(2);QList<double> v;
        if(!m.captured(3).isEmpty())for(const auto &n:m.captured(3).split('X',Qt::SkipEmptyParts)){bool ok=false;v.append(n.toDouble(&ok));if(!ok)broken(line,ui("unverständliche Blende „%1“").arg(text));}
        GerberAperture a;
        if(name.size()==1&&QStringLiteral("CROP").contains(name)){
            a.kind=name[0].toLatin1();a.parameters=v;
            // Sizes in millimetres; a polygon's vertex count and rotation stay as they are.
            for(int k=0;k<a.parameters.size();k++)if(!(a.kind=='P'&&k>0))a.parameters[k]*=unit;
            a.shape=standardShape(a.kind,a.parameters);
        }else{
            if(!macros.contains(name))broken(line,ui("unbekanntes Makro „%1“").arg(name));
            a.kind='M';a.parameters=v;a.shape=QTransform::fromScale(unit,unit).map(macroShape(macros[name],v,line));
        }
        out.apertures[code]=a;
    }
    void command(const QString &word,int here){
        line=here;
        if(word.isEmpty()||word.startsWith("G04")||word.startsWith("G4"))return;
        static const QRegularExpression token("([A-Z])([-+]?[0-9.]+)");
        QMap<QChar,QString> values;bool coordinates=false;
        for(auto it=token.globalMatch(word);it.hasNext();){
            const auto m=it.next();const QChar letter=m.captured(1)[0];const QString number=m.captured(2);
            if(letter=='G'){
                switch(number.toInt()){
                case 1:mode=1;break;case 2:mode=2;break;case 3:mode=3;break;
                case 36:region=true;contours.clear();contour.clear();break;
                case 37:if(!region)broken(line,ui("ein Regionsende ohne Anfang"));closeContour();
                    if(!contours.isEmpty()){GerberObject o;o.kind=GerberObject::Region;o.dark=dark;o.contours=contours;out.objects.append(o);}region=false;contours.clear();break;
                case 74:multi=false;break;case 75:multi=true;break;
                case 70:unit=25.4;break;case 71:unit=1;break;
                case 90:incremental=false;break;case 91:incremental=true;break;
                default:break;
                }
            }else if(letter=='M'){if(number.toInt()==2||number.toInt()==0||number.toInt()==1){ended=true;return;}}
            else if(letter=='D'){
                const int d=number.toInt();
                if(d>=10){if(!out.apertures.contains(d))broken(line,ui("unbekannte Blende D%1").arg(d));aperture=d;}
                else if(d>=1&&d<=3)operation=d;
            }else if(QStringLiteral("XYIJ").contains(letter)){values[letter]=number;coordinates=true;}
        }
        if(!coordinates)return;
        if(!operation)operation=1;
        const QPointF from=at;QPointF to=at;
        if(values.contains('X'))to.setX((incremental?at.x():0)+coordinate(values['X'],true));
        if(values.contains('Y'))to.setY((incremental?at.y():0)+coordinate(values['Y'],false));
        const QPointF offset(values.contains('I')?coordinate(values['I'],true):0,values.contains('J')?coordinate(values['J'],false):0);
        const bool signedOffset=values.value('I').startsWith('-')||values.value('J').startsWith('-');
        at=to;
        if(operation==2){if(region)closeContour();return;}
        if(operation==3){
            if(region)broken(line,ui("ein Blitz in einer Region"));if(aperture<0)broken(line,ui("ein Blitz ohne Blende"));
            GerberObject o;o.kind=GerberObject::Flash;o.dark=dark;o.aperture=aperture;o.to=to;o.rotation=rotation;o.mirrorX=mirrorX;o.mirrorY=mirrorY;o.scale=scale;out.objects.append(o);return;
        }
        // D01: a straight or curved piece.
        QPointF centre;bool full=false;
        if(mode!=1){
            const bool quadrants=multi.has_value()?*multi:true;
            if(quadrants||signedOffset){
                centre=from+offset;full=std::hypot(from.x()-to.x(),from.y()-to.y())<1e-9;
                // Older files without G74 or G75: a centre that does not fit both ends means single quadrant mode.
                if(!multi.has_value()&&!signedOffset&&std::abs(std::hypot(from.x()-centre.x(),from.y()-centre.y())-std::hypot(to.x()-centre.x(),to.y()-centre.y()))>.01)centre=quadrantCentre(from,to,offset);
            }else centre=quadrantCentre(from,to,offset);
        }
        if(region){
            if(contour.isEmpty())contour.append(from);
            if(mode==1)contour.append(to);else appendArc(contour,from,to,centre,mode==2,full);
            return;
        }
        if(aperture<0)broken(line,ui("ein Strich ohne Blende"));
        GerberObject o;o.dark=dark;o.aperture=aperture;o.from=from;o.to=to;
        if(mode!=1){o.kind=GerberObject::Arc;o.centre=centre;o.clockwise=mode==2;}
        out.objects.append(o);
    }
    // Single quadrant: of the four centres the offsets allow, the one with both ends equally far that spans at most 90°.
    QPointF quadrantCentre(QPointF from,QPointF to,QPointF offset) const{
        QPointF best=from+offset;double error=1e300;
        for(int sx:{1,-1})for(int sy:{1,-1}){
            const QPointF c=from+QPointF(sx*std::abs(offset.x()),sy*std::abs(offset.y()));
            const double r0=std::hypot(from.x()-c.x(),from.y()-c.y()),r1=std::hypot(to.x()-c.x(),to.y()-c.y());
            double sweep=std::atan2(to.y()-c.y(),to.x()-c.x())-std::atan2(from.y()-c.y(),from.x()-c.x());
            if(mode==2)sweep=-sweep;while(sweep<0)sweep+=2*M_PI;
            const double e=std::abs(r0-r1)+(sweep>M_PI/2+1e-6?1e6:0);
            if(e<error){error=e;best=c;}
        }
        return best;
    }
    void closeContour(){if(contour.size()>=3){if(contour.first()!=contour.last())contour.append(contour.first());contours.append(contour);}contour.clear();}
    void endRepeat(){
        const QList<GerberObject> block=out.objects.mid(repeatStart);
        for(int iy=0;iy<repeatY;iy++)for(int ix=0;ix<repeatX;ix++){
            if(!ix&&!iy)continue;const QPointF d(ix*stepX,iy*stepY);
            for(auto o:block){o.from+=d;o.to+=d;o.centre+=d;for(auto &c:o.contours)c.translate(d);out.objects.append(o);}
        }
        repeating=false;
    }
};
}

QPainterPath GerberData::shape(const GerberObject &o) const{
    QPainterPath p;const auto a=apertures.value(o.aperture);
    auto stroked=[&](const QPainterPath &line,double width){QPainterPathStroker s;s.setWidth(std::max(width,1e-4));s.setCapStyle(Qt::RoundCap);s.setJoinStyle(Qt::RoundJoin);return s.createStroke(line);};
    switch(o.kind){
    case GerberObject::Draw:{
        if(a.kind=='R'){
            // A rectangle swept along the line.
            const QRectF r=a.shape.boundingRect();QList<QPointF> corners;
            for(QPointF c:{o.from,o.to})for(QPointF k:{r.topLeft(),r.topRight(),r.bottomLeft(),r.bottomRight()})corners.append(c+k);
            p.addPolygon(convexHull(corners));p.closeSubpath();break;
        }
        const double width=a.kind=='C'?a.parameters.value(0):std::min(a.shape.boundingRect().width(),a.shape.boundingRect().height());
        if(std::hypot(o.from.x()-o.to.x(),o.from.y()-o.to.y())<1e-9){p.addEllipse(o.to,width/2,width/2);break;}
        QPainterPath line(o.from);line.lineTo(o.to);p=stroked(line,width);break;}
    case GerberObject::Arc:{
        QPolygonF points{o.from};appendArc(points,o.from,o.to,o.centre,o.clockwise,std::hypot(o.from.x()-o.to.x(),o.from.y()-o.to.y())<1e-9);
        QPainterPath line;line.addPolygon(points);const double width=a.kind=='C'?a.parameters.value(0):std::min(a.shape.boundingRect().width(),a.shape.boundingRect().height());
        p=stroked(line,width);break;}
    case GerberObject::Flash:{
        QTransform t;t.translate(o.to.x(),o.to.y());t.rotate(o.rotation);t.scale((o.mirrorX?-1:1)*o.scale,(o.mirrorY?-1:1)*o.scale);p=t.map(a.shape);break;}
    case GerberObject::Region:
        // Every contour is filled; together they are their union.
        p.setFillRule(Qt::WindingFill);
        for(auto c:o.contours){if(signedArea(c)<0)std::reverse(c.begin(),c.end());p.addPolygon(c);p.closeSubpath();}
        break;
    }
    return p;
}
QRectF GerberData::bounds() const{QRectF r;for(const auto &o:objects)if(o.dark)r=r.united(shape(o).boundingRect());return r;}
void GerberData::paint(QPainter &painter,const QColor &darkColour,const QColor &clearColour) const{
    painter.save();painter.setPen(Qt::NoPen);
    for(const auto &o:objects){painter.setBrush(o.dark?darkColour:clearColour);painter.drawPath(shape(o));}
    painter.restore();
}
GerberData readGerber(const QByteArray &data){return Parser(data).run();}

// --- Excellon
DrillFormat drillFormat(const QByteArray &data){
    DrillFormat f;bool digits=false;
    for(auto line:data.split('\n')){
        line=line.trimmed();if(line=="%"||line.startsWith("M95"))break;
        static const QRegularExpression unitLine("^(METRIC|INCH|M71|M72)(?:,(LZ|TZ))?(?:,(0*)\\.(0*))?"),fileFormat(";\\s*FILE_FORMAT\\s*=\\s*(\\d):(\\d)"),
            kicad(";\\s*FORMAT=\\{(\\d):(\\d)");
        const QString text=QString::fromLatin1(line);
        if(auto m=unitLine.match(text);m.hasMatch()){
            f.metric=m.captured(1)=="METRIC"||m.captured(1)=="M71";f.trailingZeros=m.captured(2)=="TZ";
            if(!m.captured(3).isEmpty()||!m.captured(4).isEmpty()){f.integerDigits=int(m.captured(3).size());f.decimalDigits=int(m.captured(4).size());digits=true;}
            else if(!digits){f.integerDigits=f.metric?3:2;f.decimalDigits=f.metric?3:4;}
        }
        if(auto m=fileFormat.match(text);m.hasMatch()){f.integerDigits=m.captured(1).toInt();f.decimalDigits=m.captured(2).toInt();digits=true;}
        if(auto m=kicad.match(text);m.hasMatch()){f.integerDigits=m.captured(1).toInt();f.decimalDigits=m.captured(2).toInt();digits=true;}
    }
    return f;
}
QList<DrillHit> readExcellon(const QByteArray &data,const DrillFormat &format){
    QList<DrillHit> out;QMap<int,double> tools;int tool=-1;double unit=format.metric?1:25.4;QPointF at;int lineNumber=0;bool any=false;
    auto fail=[&](const QString &what){throw FormatError(ui("Die Bohrdatei ist fehlerhaft (Zeile %1): %2").arg(lineNumber).arg(what));};
    auto number=[&](QString v){
        if(v.contains('.')||format.decimalPoint)return v.toDouble()*unit;
        const bool negative=v.startsWith('-');if(negative||v.startsWith('+'))v.remove(0,1);
        v=format.trailingZeros&&!format.allDigits?v.rightJustified(format.integerDigits+format.decimalDigits,'0'):v.leftJustified(format.integerDigits+format.decimalDigits,'0');
        return (negative?-1:1)*v.toLongLong()/std::pow(10.0,format.decimalDigits)*unit;
    };
    static const QRegularExpression toolDef("^T(\\d+)(?:[FSB][0-9.]+)*C([0-9.]+)"),select("^T(\\d+)$"),xy("^(?:X([-+]?[0-9.]+))?(?:Y([-+]?[0-9.]+))?");
    for(auto raw:data.split('\n')){
        lineNumber++;const QString line=QString::fromLatin1(raw).trimmed();
        if(line.isEmpty()||line.startsWith(';'))continue;
        if(line.startsWith("METRIC")||line.startsWith("M71")){unit=1;continue;}
        if(line.startsWith("INCH")||line.startsWith("M72")){unit=25.4;continue;}
        if(line=="M30"||line=="M00")break;
        if(auto m=toolDef.match(line);m.hasMatch()){tools[m.captured(1).toInt()]=m.captured(2).toDouble()*unit;any=true;
            if(!line.contains('X')&&!line.contains('Y')&&line.indexOf('C')>0){continue;}}
        if(auto m=select.match(line);m.hasMatch()){tool=m.captured(1).toInt();if(tool&&!tools.contains(tool))fail(ui("unbekanntes Werkzeug T%1").arg(tool));continue;}
        if(line.startsWith('X')||line.startsWith('Y')){
            const auto m=xy.match(line);if(!m.hasMatch())continue;
            if(tool<=0)fail(ui("eine Bohrung ohne Werkzeug"));
            if(!m.captured(1).isEmpty())at.setX(number(m.captured(1)));if(!m.captured(2).isEmpty())at.setY(number(m.captured(2)));
            out.append({at,tools[tool]});any=true;
        }
    }
    if(!any)throw FormatError(ui("Die Datei enthält keine Bohrdaten."));
    return out;
}
// --- from Gerber objects to elements
namespace {
constexpr double near=.02;              // positions closer than this count as the same (mm)
bool same(QPointF a,QPointF b,double d=near){return std::hypot(a.x()-b.x(),a.y()-b.y())<d;}
// The contours of a shape, curves flattened to a micrometre, without repeated closing points.
QList<QPolygonF> fineContours(const QPainterPath &shape){
    QList<QPolygonF> out;const QTransform back=QTransform::fromScale(.001,.001);
    for(auto c:QTransform::fromScale(1000,1000).map(shape).simplified().toSubpathPolygons()){
        c=back.map(c);if(c.size()>1&&c.first()==c.last())c.removeLast();if(c.size()>=3)out.append(c);
    }
    return out;
}
// Polygons of a shape without holes: a contour inside an odd number of others is a hole and is joined to the outline
// around it by a narrow cut, so that every piece can be one area.
QList<QPolygonF> areaPolygons(const QPainterPath &shape){
    const QList<QPolygonF> contours=fineContours(shape);QList<QPolygonF> outers,holes;
    for(qsizetype i=0;i<contours.size();i++){
        int around=0;for(qsizetype k=0;k<contours.size();k++)if(k!=i&&contours[k].containsPoint(contours[i].first(),Qt::OddEvenFill))around++;
        (around%2?holes:outers).append(contours[i]);
    }
    for(auto &o:outers){
        if(signedArea(o)<0)std::reverse(o.begin(),o.end());
        for(auto h:holes){
            if(!o.containsPoint(h.first(),Qt::OddEvenFill))continue;
            if(signedArea(h)>0)std::reverse(h.begin(),h.end());
            qsizetype oi=0,hi=0;double d=1e300;
            for(qsizetype i=0;i<o.size();i++)for(qsizetype k=0;k<h.size();k++){const double e=std::hypot(o[i].x()-h[k].x(),o[i].y()-h[k].y());if(e<d){d=e;oi=i;hi=k;}}
            QPolygonF joined;for(qsizetype i=0;i<=oi;i++)joined.append(o[i]);
            for(qsizetype k=0;k<=h.size();k++)joined.append(h[(hi+k)%h.size()]);
            for(qsizetype i=oi;i<o.size();i++)joined.append(o[i]);
            o=joined;
        }
    }
    return outers;
}
// Whether two polygons have the same corners (in any order and direction).
bool sameCorners(QPolygonF a,QPolygonF b,double tolerance=.005){
    if(a.size()>1&&a.first()==a.last())a.removeLast();if(b.size()>1&&b.first()==b.last())b.removeLast();
    if(a.size()!=b.size())return false;
    for(auto p:a){bool found=false;for(auto q:b)if(same(p,q,tolerance)){found=true;break;}if(!found)return false;}
    return true;
}
double angleOf(QPointF d){double a=qRadiansToDegrees(std::atan2(-d.y(),d.x()));a=std::fmod(a,360);if(a<0)a+=360;return std::abs(a-std::round(a))<1e-6?std::round(a):a;}
// A flashed aperture as a through-hole pad form, if it is one: round, square, octagon, or wide/tall at 2:1.
std::optional<std::pair<PadShape,double>> padForm(const GerberAperture &a,const GerberObject &o,double *rotation){
    const QRectF r=a.shape.boundingRect();const double w=r.width()*o.scale,h=r.height()*o.scale;*rotation=o.rotation;
    auto ratio=[](double a,double b){return std::abs(a-2*b)<1e-3*std::max(1.0,a);};
    switch(a.kind){
    case 'C':return std::pair{PadShape::Round,w};
    case 'R':if(std::abs(w-h)<1e-6)return std::pair{PadShape::Square,w};if(ratio(w,h))return std::pair{PadShape::SquareWide,h};if(ratio(h,w))return std::pair{PadShape::SquareTall,w};break;
    case 'O':if(ratio(w,h))return std::pair{PadShape::OvalWide,h};if(ratio(h,w))return std::pair{PadShape::OvalTall,w};if(std::abs(w-h)<1e-6)return std::pair{PadShape::Round,w};break;
    }
    return std::nullopt;
}
// A through-hole pad whose outline is this polygon (board coordinates) about `centre`, if there is one.
std::optional<Element> fitPad(const QPolygonF &poly,QPointF centre){
    if(poly.size()!=4&&poly.size()!=8)return std::nullopt;
    const QList<PadShape> shapes=poly.size()==4?QList<PadShape>{PadShape::Square,PadShape::SquareWide,PadShape::SquareTall}:QList<PadShape>{PadShape::Octagon,PadShape::OctagonWide,PadShape::OctagonTall};
    for(qsizetype i=0;i<poly.size();i++){
        const double base=angleOf(poly[(i+1)%poly.size()]-poly[i]);
        for(int quarter=0;quarter<4;quarter++){
            const double r=std::fmod(base+90*quarter,360.0),a=qDegreesToRadians(r);const QPointF u(std::cos(a),-std::sin(a)),v(std::sin(a),std::cos(a));
            double eu=0,ev=0;for(auto p:poly){const QPointF d=p-centre;eu=std::max(eu,std::abs(d.x()*u.x()+d.y()*u.y()));ev=std::max(ev,std::abs(d.x()*v.x()+d.y()*v.y()));}
            for(auto shape:shapes){
                const double size=shape==PadShape::SquareWide||shape==PadShape::OctagonWide?2*ev:2*eu;
                Element pad=newElement(ElementType::Pad);pad.pos=centre;pad.shape=shape;pad.size=size;pad.rotation=r;updateOutline(pad);
                if(sameCorners(pad.points,poly))return pad;
            }
        }
    }
    return std::nullopt;
}
// An SMD pad whose outline is this rectangle (board coordinates), if it is one.
std::optional<Element> fitSmd(const QPolygonF &poly){
    if(poly.size()!=4)return std::nullopt;
    const QPointF u=poly[1]-poly[0],v=poly[2]-poly[1];const double lu=std::hypot(u.x(),u.y()),lv=std::hypot(v.x(),v.y());
    if(lu<=0||lv<=0||std::abs(u.x()*v.x()+u.y()*v.y())>1e-4*lu*lv||lu>10||lv>10)return std::nullopt;
    QPointF centre;for(auto p:poly)centre+=p/4;
    for(int k=0;k<4;k++){
        Element smd=newElement(ElementType::SmdPad);smd.pos=centre;smd.rotation=std::fmod(angleOf(u)+90*k,360.0);
        smd.size=k%2?lv:lu;smd.size2=k%2?lu:lv;updateOutline(smd);if(sameCorners(smd.points,poly)){if(smd.rotation>=180)smd.rotation-=180;updateOutline(smd);return smd;}
    }
    return std::nullopt;
}
}

Board importGerber(const GerberImport &import,const QString &name,const std::function<void(int,int)> &progress){
    // The working area around everything, its origin where the files have theirs.
    QRectF all;for(const auto &[layer,data]:import.layers.asKeyValueRange())all=all.united(data.bounds());
    for(const auto &h:import.drills)all=all.united(QRectF(h.at-QPointF(h.diameter/2,h.diameter/2),QSizeF(h.diameter,h.diameter)));
    if(all.isEmpty())all=QRectF(0,0,100,80);
    Board b=newBoard(name,std::max(1.0,all.width()),std::max(1.0,all.height()));b.origin=QPointF(-all.left(),all.bottom());
    // Board coordinates: x from the left edge, y down from the top edge.
    auto map=[&](QPointF p){return QPointF(p.x()-all.left(),all.bottom()-p.y());};
    auto mapPolygon=[&](const QPolygonF &poly){QPolygonF out;for(auto p:poly)out.append(map(p));return out;};
    auto mapRect=[&](const QRectF &r){return QRectF(map(r.topLeft()),map(r.bottomRight())).normalized();};
    QList<QPointF> holes;for(const auto &h:import.drills)holes.append(map(h.at));
    auto holeAt=[&](QPointF p){for(int k=0;k<holes.size();k++)if(same(holes[k],p))return k;return -1;};
    int done=0;const int total=int(import.layers.size());
    for(const auto &[layer,data]:import.layers.asKeyValueRange()){
        if(progress)progress(done++,total);
        const bool copper=isCopper(layer);QList<Element> made;
        // A dark region over most of everything, followed by clear objects, is the layer's ground plane.
        qsizetype ground=-1;
        if(copper){
            for(qsizetype i=0;i<data.objects.size();i++){
                const auto &o=data.objects[i];if(o.kind!=GerberObject::Region||!o.dark)continue;
                const QRectF r=data.shape(o).boundingRect();
                if(r.width()*r.height()>=.8*all.width()*all.height()){bool clearLater=false;for(qsizetype k=i+1;k<data.objects.size();k++)clearLater|=!data.objects[k].dark;if(clearLater)ground=i;}
                break;
            }
            if(ground>=0)b.groundPlane[layer]=true;
        }
        // The dark regions of the layer, to tell SMD pads (alone) from the strips of a hatched area (crossing others).
        QList<QRectF> regionBoxes;
        for(qsizetype i=0;i<data.objects.size();i++){const auto &o=data.objects[i];if(i!=ground&&o.dark&&o.kind==GerberObject::Region)regionBoxes.append(mapRect(data.shape(o).boundingRect()));}
        auto area=[&](const QPolygonF &poly,bool cutout=false){
            if(copper&&!cutout){
                // The square end of a track drawn before it.
                for(auto &t:made){
                    if(t.type!=ElementType::Track||t.points.size()<2||t.width<=0)continue;
                    if(!t.flatStart&&sameCorners(squareEnd(t.points.first(),t.points[1],t.width),poly)){t.flatStart=true;return;}
                    if(!t.flatEnd&&sameCorners(squareEnd(t.points.last(),t.points[t.points.size()-2],t.width),poly)){t.flatEnd=true;return;}
                }
                // A polygon about a drill hole may be a through-hole pad, a small rectangle on its own an SMD pad.
                QPointF centre;for(auto p:poly)centre+=p/double(poly.size());
                const int hole=holeAt(centre);
                if(hole>=0)if(auto pad=fitPad(poly,centre)){pad->layer=layer;pad->size2=import.drills[hole].diameter;updateOutline(*pad);made.append(*pad);return;}
                const QRectF box=poly.boundingRect();int crossing=0;
                for(const auto &r:regionBoxes){const QRectF i=r.intersected(box);if(i.width()>1e-6&&i.height()>1e-6&&!(std::abs(r.left()-box.left())<1e-6&&std::abs(r.right()-box.right())<1e-6&&std::abs(r.top()-box.top())<1e-6&&std::abs(r.bottom()-box.bottom())<1e-6))crossing++;}
                if(hole<0&&!crossing)if(auto smd=fitSmd(poly)){smd->layer=layer;made.append(*smd);return;}
            }
            Element a=newElement(ElementType::Area);a.layer=layer;a.width=0;a.cutout=cutout;a.points=poly;made.append(a);
        };
        // Clear objects of a ground plane in groups: draws continuing each other are one outline.
        struct Cut {QPainterPath shape;QRectF box;};QList<Cut> cuts;int cutAperture=-1;QPointF cutEnd;bool cutOpen=false;
        // Tracks: draws continuing each other with the same aperture become one track.
        Element track;bool open=false;int trackAperture=-1;
        auto flush=[&]{if(open&&track.points.size()>1)made.append(track);open=false;};
        for(qsizetype i=0;i<data.objects.size();i++){
            const auto &o=data.objects[i];if(i==ground)continue;
            if(!o.dark){
                flush();const QPainterPath shape=data.shape(o);
                if(copper&&ground>=0){
                    const bool continues=cutOpen&&(o.kind==GerberObject::Draw||o.kind==GerberObject::Arc)&&o.aperture==cutAperture&&same(o.from,cutEnd,1e-6);
                    if(continues){cuts.last().shape.addPath(shape);cuts.last().box=cuts.last().box.united(mapRect(shape.boundingRect()));}
                    else cuts.append({shape,mapRect(shape.boundingRect())});
                    cutOpen=o.kind==GerberObject::Draw||o.kind==GerberObject::Arc;cutAperture=o.aperture;cutEnd=o.to;
                }else if(copper)for(const auto &poly:areaPolygons(shape))area(mapPolygon(poly),true);
                continue;
            }
            cutOpen=false;
            const auto a=data.apertures.value(o.aperture);const double width=a.kind=='C'?a.parameters.value(0):std::min(a.shape.boundingRect().width(),a.shape.boundingRect().height());
            const bool dot=(o.kind==GerberObject::Draw||o.kind==GerberObject::Arc)&&same(o.from,o.to,1e-9)&&o.kind==GerberObject::Draw;
            if(o.kind==GerberObject::Draw&&a.kind!='R'&&!dot){
                if(open&&import.joinTracks&&trackAperture==o.aperture&&same(map(o.from),track.points.last(),1e-6)){track.points.append(map(o.to));continue;}
                flush();track=newElement(ElementType::Track);track.layer=layer;track.width=width;track.points={map(o.from),map(o.to)};open=true;trackAperture=o.aperture;
                if(!import.joinTracks)flush();
                continue;
            }
            flush();
            if(dot&&a.kind!='R'){
                // A draw that does not move is a dot: a round pad or a filled circle.
                if(copper){Element pad=newElement(ElementType::Pad);pad.layer=layer;pad.pos=map(o.to);pad.size=width;const int hole=holeAt(pad.pos);pad.size2=hole>=0?import.drills[hole].diameter:0;updateOutline(pad);made.append(pad);}
                else{Element c=newElement(ElementType::Circle);c.layer=layer;c.pos=map(o.to);c.size=width/2;c.width=0;c.filled=true;made.append(c);}
                continue;
            }
            if(o.kind==GerberObject::Arc&&a.kind!='R'){
                Element c=newElement(ElementType::Circle);c.layer=layer;c.width=width;c.pos=map(o.centre);c.size=std::hypot(o.from.x()-o.centre.x(),o.from.y()-o.centre.y());
                if(!same(o.from,o.to,1e-9)){
                    auto at=[&](QPointF p){double v=qRadiansToDegrees(std::atan2(p.y()-o.centre.y(),p.x()-o.centre.x()));return v<0?v+360:v;};
                    const double s=at(o.from),e=at(o.to);c.start=o.clockwise?e:s;c.stop=o.clockwise?s:e;
                }
                made.append(c);continue;
            }
            if(o.kind==GerberObject::Flash){
                // Over a drill hole a flash is a through-hole pad; without one a pad without hole, an SMD pad or an area.
                const int hole=holeAt(map(o.to));double rotation=0;const auto form=padForm(a,o,&rotation);
                if(copper&&form&&(hole>=0||a.kind=='C'||form->first==PadShape::OvalWide||form->first==PadShape::OvalTall)){
                    Element pad=newElement(ElementType::Pad);pad.layer=layer;pad.pos=map(o.to);pad.shape=form->first;pad.size=form->second;
                    pad.size2=hole>=0?import.drills[hole].diameter:0;pad.rotation=rotation;updateOutline(pad);made.append(pad);continue;
                }
                if(!copper&&a.kind=='C'){Element c=newElement(ElementType::Circle);c.layer=layer;c.pos=map(o.to);c.size=width/2;c.width=0;c.filled=true;made.append(c);continue;}
            }
            // Regions, other flashes and draws with rectangular apertures become areas (or pads that fit).
            for(const auto &poly:areaPolygons(data.shape(o)))area(mapPolygon(poly));
        }
        flush();
        if(copper){
            // A short single track over a drill hole, as long as it is wide, is an oval pad.
            for(auto &t:made){
                if(t.type!=ElementType::Track||t.points.size()!=2)continue;
                const QPointF m=(t.points[0]+t.points[1])/2;const int hole=holeAt(m);const QPointF d=t.points[1]-t.points[0];
                if(hole<0||std::abs(std::hypot(d.x(),d.y())-t.width)>.01)continue;
                Element pad=newElement(ElementType::Pad);pad.layer=t.layer;pad.pos=m;pad.shape=PadShape::OvalWide;pad.size=t.width;pad.size2=import.drills[hole].diameter;
                pad.rotation=std::fmod(angleOf(d),180.0);updateOutline(pad);t=pad;
            }
        }
        if(ground>=0){
            // Clearances: a cut around an element, larger by the same amount on every side, gives its distance to the
            // ground plane; elements without one are joined to the plane.
            QList<QRectF> boxes;for(const auto &m:made)boxes.append(copperShape(m).boundingRect());
            QList<bool> used(cuts.size(),false);
            for(int k=0;k<made.size();k++){
                auto &e=made[k];if(e.cutout)continue;e.clearance=0;const QRectF r=boxes[k];
                for(int c=0;c<cuts.size();c++){
                    const QRectF m=cuts[c].box;if(!same(m.center(),r.center(),.005))continue;
                    const double gx=(m.width()-r.width())/2,gy=(m.height()-r.height())/2;
                    if(gx>=-1e-4&&std::abs(gx-gy)<.005){e.clearance=std::max(e.clearance,std::max(0.0,gx));used[c]=true;}
                }
                if(e.clearance>0)e.clearance=std::round(e.clearance*1e4)/1e4;
            }
            // Cuts inside the clearance of one element (the square end of a track) belong to it; holes of drills need
            // nothing; the rest keeps the plane away. Only the elements before the keep-out areas added here count.
            const qsizetype counted=boxes.size();
            for(int c=0;c<cuts.size();c++){
                if(used[c])continue;const QRectF m=cuts[c].box;
                for(int k=0;k<counted&&!used[c];k++){
                    const auto &e=made[k];if(e.clearance<=0||!boxes[k].adjusted(-e.clearance-.005,-e.clearance-.005,e.clearance+.005,e.clearance+.005).contains(m))continue;
                    bool others=false;for(int j=0;j<counted&&!others;j++)others=j!=k&&m.contains(boxes[j].center());
                    if(!others)used[c]=true;
                }
                if(used[c])continue;
                if(holeAt(m.center())>=0&&std::abs(m.width()-import.drills[holeAt(m.center())].diameter)<.01)continue;
                for(const auto &poly:areaPolygons(cuts[c].shape))area(mapPolygon(poly),true);
            }
            // Spokes of thermal pads: small areas reaching from a pad across its clearance.
            QList<int> spokes;
            for(int p=0;p<made.size();p++){
                auto &pad=made[p];if((pad.type!=ElementType::Pad&&pad.type!=ElementType::SmdPad)||pad.clearance<=0)continue;
                const QPainterPath padShape=copperShape(pad);const QRectF zone=padShape.boundingRect().adjusted(-pad.clearance-1e-3,-pad.clearance-1e-3,pad.clearance+1e-3,pad.clearance+1e-3);
                for(int k=0;k<made.size();k++){
                    const auto &a=made[k];if(a.type!=ElementType::Area||a.cutout||spokes.contains(k))continue;const QRectF ar=a.points.boundingRect();
                    if(!zone.contains(ar)||!padShape.intersects(copperShape(a)))continue;
                    const QPointF d=ar.center()-pad.pos;const int bit=int(std::lround((90-angleOf(d)+pad.rotation)/45.0+16))%8;
                    if(!pad.thermal){pad.thermal=true;pad.thermalSpokes=0;}
                    pad.thermalSpokes|=quint32(1)<<bit;
                    const double size=pad.type==ElementType::Pad?pad.size:std::min(pad.size,pad.size2),span=std::min(ar.width(),ar.height());
                    if(bit%2==0&&size>0)pad.thermalWidth=std::clamp(int(std::lround(span*300/size)),10,300);
                    spokes.append(k);
                }
            }
            std::sort(spokes.begin(),spokes.end());for(qsizetype i=spokes.size()-1;i>=0;i--)made.removeAt(spokes[i]);
        }
        b.elements+=made;
    }
    // Holes without a pad over them are plain holes; pads of the outer sides over one hole become one through-plated pad.
    for(int k=0;k<holes.size();k++){
        bool pad=false;for(const auto &e:b.elements)pad|=e.type==ElementType::Pad&&e.size2>0&&same(e.pos,holes[k]);
        if(pad)continue;
        Element hole=newElement(ElementType::Pad);hole.pos=holes[k];hole.size=hole.size2=import.drills[k].diameter;hole.solderMask=false;updateOutline(hole);b.elements.append(hole);
    }
    if(import.vias){
        QList<int> gone;
        for(int i=0;i<b.elements.size();i++){
            const auto &e=b.elements[i];if(e.type!=ElementType::Pad||e.size2<=0||gone.contains(i))continue;
            QList<int> others;for(int k=i+1;k<b.elements.size();k++){const auto &o=b.elements[k];if(o.type==ElementType::Pad&&o.size2>0&&o.layer!=e.layer&&same(o.pos,e.pos)&&!gone.contains(k))others.append(k);}
            if(others.isEmpty())continue;
            // The pad of the bottom side stands for all; a through-plated pad is on every copper layer.
            int keep=i;for(int k:others)if(b.elements[k].layer==CopperBottom)keep=k;
            for(int k:QList<int>{i}+others)if(k!=keep)gone.append(k);
            b.elements[keep].via=true;b.elements[keep].layer=CopperBottom;
        }
        removeElements(b,gone);
    }
    b.multilayer=import.layers.contains(Inner1)||import.layers.contains(Inner2);
    for(int l=1;l<=layerCount;l++)b.visible[l]=true;
    return b;
}
}
