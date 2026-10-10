#include "textio.h"
#include "sprint.h"
#include "language.h"
#include "legacy_reader.h"
#include "modules/pcb/font.h"
#include <QMap>
#include <QStringDecoder>
#include <QSet>
#include <QStringList>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>

namespace openloch::sprint {
using namespace pcb;
namespace {
constexpr double unit=10000;    // lengths in millimetres × 10000
[[noreturn]] void broken(const QString &what){throw FormatError(ui("Die Text-IO-Datei ist fehlerhaft: %1").arg(what));}
QString length(double mm){return QString::number(qint64(std::llround(mm*unit)));}
QString point(QPointF p){return length(p.x())+QStringLiteral(" / ")+length(p.y());}
QString bars(QString text){text.replace(QLatin1Char('|'),QLatin1Char('/'));return QLatin1Char('|')+text+QLatin1Char('|');}   // a bar cannot stand inside
QString yes(bool on){return on?QStringLiteral("true"):QStringLiteral("false");}
double normalized(double a){a=std::fmod(a,360.0);return a<0?a+360:a;}

// --- writing
// Rotations of pads and texts count clockwise, as the reference turns elements (pads in hundredths, texts in
// thousandths of a degree); arcs counter-clockwise from three o'clock.
QStringList statement(const Element &e,const QString &kind,int padId,const QMap<int,int> &ids){
    QStringList p{QStringLiteral("LAYER=%1").arg(e.layer)};
    auto add=[&](const QString &key,const QString &value){p<<key+QLatin1Char('=')+value;};
    auto nodes=[&]{for(int k=0;k<e.points.size();k++)add(QStringLiteral("P%1").arg(k),point(e.points[k]));};
    const bool pad=e.type==ElementType::Pad||e.type==ElementType::SmdPad;
    switch(e.type){
    case ElementType::Track:add("WIDTH",length(e.width));nodes();if(e.flatStart)add("FLATSTART","true");if(e.flatEnd)add("FLATEND","true");break;
    case ElementType::Area:add("WIDTH",length(e.width));nodes();
        if(e.hatched){add("HATCH","true");add("HATCH_AUTO",yes(e.hatchAuto));if(!e.hatchAuto)add("HATCH_WIDTH",length(e.hatchPitch));}
        if(e.maskOnly)add("SOLDERMASK_CUTOUT","true");break;
    case ElementType::Pad:add("POS",point(e.pos));add("SIZE",length(e.size));add("DRILL",length(e.size2));add("FORM",QString::number(int(e.shape)));
        if(e.via)add("VIA","true");if(e.thermalPerLayer)add("THERMAL_TRACKS_INDIVIDUAL","true");break;
    case ElementType::SmdPad:add("POS",point(e.pos));add("SIZE_X",length(e.size));add("SIZE_Y",length(e.size2));break;
    case ElementType::Circle:add("WIDTH",length(e.width));add("CENTER",point(e.pos));add("RADIUS",length(e.size));
        if(std::abs(normalized(e.stop-e.start))>1e-9){add("START",QString::number(qint64(std::llround(normalized(e.start)*1000))));add("STOP",QString::number(qint64(std::llround(normalized(e.stop)*1000))));}
        if(e.filled)add("FILL","true");break;
    case ElementType::Text:{
        // A text mirrored top to bottom stays so: mirrored left to right the other way and turned by half a turn more.
        add("POS",point(e.pos));add("TEXT",bars(e.text));add("HEIGHT",length(e.size));add("STYLE",QString::number(e.style));add("THICKNESS",QString::number(e.thickness));
        const bool vertical=mirroredVertically(e);const qint64 angle=std::llround(normalized((vertical?180:0)-e.rotation)*1000)%360000;
        if(angle)add("ROTATION",QString::number(angle));if(e.mirrored!=vertical)add("MIRROR_HORZ","true");if(vertical)add("MIRROR_VERT","true");
        if(e.role!=TextRole::Plain)add("VISIBLE",yes(e.visible));break;}
    }
    if(std::abs(e.clearance-.4)>1e-9)add("CLEAR",length(e.clearance));
    if(pad){
        add("SOLDERMASK",yes(e.solderMask));
        if(std::abs(normalized(e.rotation))>1e-9)add("ROTATION",QString::number(qint64(std::llround(normalized(-e.rotation)*100))%36000));
        if(e.thermal){add("THERMAL","true");add("THERMAL_TRACKS_WIDTH",QString::number(e.thermalWidth));
            add("THERMAL_TRACKS",QString::number(e.type==ElementType::SmdPad?e.thermalSpokes&0xff:e.thermalSpokes));}
        add("PAD_ID",QString::number(padId));
        int k=0;for(int c:e.connections)if(ids.contains(c))add(QStringLiteral("CON%1").arg(k++),QString::number(ids[c]));
    }else{
        if(e.cutout)add("CUTOUT","true");if(e.solderMask)add("SOLDERMASK","true");
    }
    if(e.type!=ElementType::Text&&!e.name.isEmpty())add("NAME",bars(e.name));
    return QStringList{kind}+p;
}
}

QString writeTextIO(const QList<Element> &elements){
    // Pads get running ids for the airwires.
    QMap<int,int> ids;for(int i=0;i<elements.size();i++)if(elements[i].type==ElementType::Pad||elements[i].type==ElementType::SmdPad)ids.insert(i,int(ids.size())+1);
    // Groups become nested blocks, outermost first. A component (a number with a designator) is one block, as the
    // reference writes it, inside the groups all its members share; the groups only some of them have are blocks inside
    // it. Components are kept as negative block numbers.
    QSet<int> components;for(const auto &e:elements)if(e.part&&e.type==ElementType::Text&&e.role==TextRole::Designator)components.insert(e.part);
    QMap<int,QList<int>> shared;   // per component: the outer groups all its members have, innermost first
    for(const auto &e:elements)if(components.contains(e.part)){
        if(!shared.contains(e.part)){shared.insert(e.part,e.groups);continue;}
        auto &s=shared[e.part];qsizetype n=0;while(n<s.size()&&n<e.groups.size()&&s[s.size()-1-n]==e.groups[e.groups.size()-1-n])n++;s=s.mid(s.size()-n);}
    QList<QList<int>> blocks;for(const auto &e:elements){
        if(!components.contains(e.part)){blocks.append(e.groups);continue;}
        const auto &s=shared[e.part];blocks.append(e.groups.mid(0,e.groups.size()-s.size())+QList<int>{-e.part}+s);}
    QString out;
    std::function<void(const QList<int>&,int,const QString&)> level=[&](const QList<int> &members,int depth,const QString &indent){
        QList<int> order;   // the blocks at this depth, in the order they first appear
        for(int i:members){const auto &g=blocks[i];
            if(g.size()<=depth){
                const auto &e=elements[i];QString kind;
                switch(e.type){case ElementType::Track:kind="TRACK";break;case ElementType::Area:kind="ZONE";break;case ElementType::Pad:kind="PAD";break;
                    case ElementType::SmdPad:kind="SMDPAD";break;case ElementType::Circle:kind="CIRCLE";break;
                    case ElementType::Text:kind=e.role==TextRole::Designator?"ID_TEXT":e.role==TextRole::Value?"VALUE_TEXT":"TEXT";break;}
                out+=indent+statement(e,kind,ids.value(i),ids).join(QStringLiteral(", "))+QStringLiteral(";\n");
            }else{const int group=g[g.size()-1-depth];if(!order.contains(group))order.append(group);}
        }
        for(int group:order){
            QList<int> inside;for(int i:members){const auto &g=blocks[i];if(g.size()>depth&&g[g.size()-1-depth]==group)inside.append(i);}
            if(group<0){
                // The component data stands on the designator, the last one as the reference takes it.
                QStringList head{QStringLiteral("BEGIN_COMPONENT")};int designator=-1;
                for(int i:inside)if(elements[i].type==ElementType::Text&&elements[i].role==TextRole::Designator)designator=i;
                if(designator>=0){const auto &e=elements[designator];
                    if(!e.comment.isEmpty())head<<QStringLiteral("COMMENT=")+bars(e.comment);if(!e.package.isEmpty())head<<QStringLiteral("PACKAGE=")+bars(e.package);
                    head<<QStringLiteral("USE_PICKPLACE=")+yes(e.pickAndPlace);if(std::abs(e.componentRotation)>1e-9)head<<QStringLiteral("ROTATION=")+QString::number(qint64(std::llround(e.componentRotation)));}
                out+=indent+head.join(QStringLiteral(", "))+QStringLiteral(";\n");level(inside,depth+1,indent+QStringLiteral("   "));out+=indent+QStringLiteral("END_COMPONENT;\n");
            }else{out+=indent+QStringLiteral("GROUP;\n");level(inside,depth+1,indent+QStringLiteral("   "));out+=indent+QStringLiteral("END_GROUP;\n");}
        }
    };
    QList<int> all;for(int i=0;i<elements.size();i++)all.append(i);
    level(all,0,QString());
    return out;
}

namespace {
// Code page 1252 where it differs from Latin-1.
const char16_t cp1252[32]={0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,
                           0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
}
QByteArray textIOBytes(const QString &text){
    QByteArray out;out.reserve(text.size());
    for(QChar c:text){
        const char16_t u=c.unicode();char b='?';
        if(u<0x80||(u>=0xa0&&u<0x100))b=char(u);else for(int i=0;i<32;i++)if(cp1252[i]==u&&!(u>=0x80&&u<0xa0)){b=char(0x80+i);break;}
        out.append(b);
    }
    return out;
}
QString textIOText(const QByteArray &bytes){
    auto utf8=QStringDecoder(QStringDecoder::Utf8,QStringDecoder::Flag::Stateless);const QString decoded=utf8(bytes);
    if(!utf8.hasError())return decoded;
    QString s;s.reserve(bytes.size());for(char c:bytes){const auto b=quint8(c);s.append(b>=0x80&&b<0xa0?QChar(cp1252[b-0x80]):QChar(b));}
    return s;
}
QList<Element> readTextIO(const QString &text){
    // Statements end at semicolons outside bars; their parts are separated by commas outside bars.
    QList<QStringList> statements;{QStringList parts;QString part;bool quoted=false;
        for(QChar c:text){
            if(c==QLatin1Char('|'))quoted=!quoted;
            if(!quoted&&c==QLatin1Char(',')){parts<<part.trimmed();part.clear();continue;}
            if(!quoted&&c==QLatin1Char(';')){parts<<part.trimmed();if(!(parts.size()==1&&parts[0].isEmpty()))statements<<parts;parts.clear();part.clear();continue;}
            part+=c;
        }
        if(!part.trimmed().isEmpty()||!parts.isEmpty())broken(ui("ein Eintrag ohne Semikolon am Ende"));}
    QList<Element> out;QList<int> stack;int nextGroup=1,nextPart=1;   // the open blocks, components as negative numbers
    struct ComponentData {QString comment,package;bool pickAndPlace=false;double rotation=0;};QList<ComponentData> components;
    QMap<int,int> padIds;QList<std::pair<int,QList<int>>> wires;
    for(const auto &st:statements){
        const QString kind=st.first().toUpper();QMap<QString,QString> v;
        for(int k=1;k<st.size();k++){const int eq=int(st[k].indexOf(QLatin1Char('=')));if(eq<0){if(!st[k].isEmpty())broken(st[k]);continue;}v.insert(st[k].left(eq).trimmed().toUpper(),st[k].mid(eq+1).trimmed());}
        auto has=[&](const QString &key){return v.contains(key);};
        auto number=[&](const QString &key,double fallback)->double{
            if(!has(key))return fallback;bool ok;const double n=v[key].toDouble(&ok);if(!ok||!std::isfinite(n)||std::abs(n)>1e10)broken(key+QLatin1Char('=')+v[key]);return n;};
        auto need=[&](const QString &key){if(!has(key))broken(ui("%1 fehlt bei %2").arg(key,kind));return number(key,0);};
        auto mm=[&](const QString &key,double fallback){return has(key)?number(key,0)/unit:fallback;};
        auto at=[&](const QString &key){
            if(!has(key))broken(ui("%1 fehlt bei %2").arg(key,kind));const auto xy=v[key].split(QLatin1Char('/'));bool a,b;
            const double x=xy.value(0).trimmed().toDouble(&a),y=xy.value(1).trimmed().toDouble(&b);if(xy.size()!=2||!a||!b)broken(key+QLatin1Char('=')+v[key]);return QPointF(x/unit,y/unit);};
        auto on=[&](const QString &key,bool fallback){if(!has(key))return fallback;const QString s=v[key].toLower();if(s!="true"&&s!="false")broken(key+QLatin1Char('=')+v[key]);return s=="true";};
        auto string=[&](const QString &key){QString s=v.value(key);if(s.startsWith(QLatin1Char('|'))&&s.endsWith(QLatin1Char('|'))&&s.size()>=2)s=s.mid(1,s.size()-2);return s;};
        if(kind=="GROUP"){stack.append(nextGroup++);continue;}
        if(kind=="END_GROUP"||kind=="END_COMPONENT"){if(stack.isEmpty())broken(kind);stack.removeLast();if(kind=="END_COMPONENT"&&!components.isEmpty())components.removeLast();continue;}
        if(kind=="BEGIN_COMPONENT"){stack.append(-nextPart++);components.append({string("COMMENT"),string("PACKAGE"),on("USE_PICKPLACE",false),number("ROTATION",0)});continue;}
        Element e;
        if(kind=="TRACK"||kind=="ZONE"){
            e=newElement(kind=="TRACK"?ElementType::Track:ElementType::Area);e.width=need("WIDTH")/unit;
            for(int k=0;has(QStringLiteral("P%1").arg(k));k++)e.points<<at(QStringLiteral("P%1").arg(k));
            if(e.points.size()<(kind=="TRACK"?2:3))broken(ui("zu wenige Punkte bei %1").arg(kind));
            e.flatStart=on("FLATSTART",false);e.flatEnd=on("FLATEND",false);
            e.hatched=on("HATCH",false);e.hatchAuto=on("HATCH_AUTO",true);e.hatchPitch=mm("HATCH_WIDTH",e.hatchPitch);
            if(kind=="ZONE")e.maskOnly=on("SOLDERMASK_CUTOUT",false);
        }else if(kind=="PAD"||kind=="SMDPAD"){
            const bool through=kind=="PAD";e=newElement(through?ElementType::Pad:ElementType::SmdPad);e.pos=at("POS");
            if(through){e.size=need("SIZE")/unit;e.size2=need("DRILL")/unit;const int form=int(need("FORM"));if(form<1||form>9)broken(QStringLiteral("FORM=%1").arg(form));
                e.shape=PadShape(form);e.via=on("VIA",false);e.thermalPerLayer=on("THERMAL_TRACKS_INDIVIDUAL",false);}
            else{e.size=need("SIZE_X")/unit;e.size2=need("SIZE_Y")/unit;}
            e.rotation=normalized(-number("ROTATION",0)/100);e.thermal=on("THERMAL",false);e.thermalWidth=int(std::clamp(number("THERMAL_TRACKS_WIDTH",100),0.0,1000.0));
            e.thermalSpokes=quint32(std::clamp(number("THERMAL_TRACKS",85),0.0,4294967295.0));
            if(has("PAD_ID"))padIds.insert(int(number("PAD_ID",0)),int(out.size()));
            QList<int> targets;for(int k=0;has(QStringLiteral("CON%1").arg(k));k++)targets<<int(number(QStringLiteral("CON%1").arg(k),0));
            if(!targets.isEmpty())wires.append({int(out.size()),targets});
            updateOutline(e);
        }else if(kind=="CIRCLE"){
            e=newElement(ElementType::Circle);e.width=need("WIDTH")/unit;e.pos=at("CENTER");e.size=need("RADIUS")/unit;
            e.start=normalized(number("START",0)/1000);e.stop=normalized(number("STOP",0)/1000);e.filled=on("FILL",false);
        }else if(kind=="TEXT"||kind=="ID_TEXT"||kind=="VALUE_TEXT"){
            e=newElement(ElementType::Text);e.pos=at("POS");e.size=need("HEIGHT")/unit;e.text=string("TEXT");if(!has("TEXT"))broken(ui("%1 fehlt bei %2").arg("TEXT",kind));
            e.style=int(std::clamp(number("STYLE",1),0.0,2.0));e.thickness=int(std::clamp(number("THICKNESS",1),0.0,2.0));e.rotation=normalized(-number("ROTATION",0)/1000);
            e.mirrored=on("MIRROR_HORZ",false);
            // Mirrored upside down: the same as mirrored left to right and half a turn.
            if(on("MIRROR_VERT",false)){e.mirrored=!e.mirrored;e.rotation=normalized(e.rotation+180);e.flipped=true;}
            e.role=kind=="ID_TEXT"?TextRole::Designator:kind=="VALUE_TEXT"?TextRole::Value:TextRole::Plain;e.visible=on("VISIBLE",true);
            if(e.role==TextRole::Designator&&!components.isEmpty()){const auto &c=components.last();e.comment=c.comment;e.package=c.package;e.pickAndPlace=c.pickAndPlace;e.componentRotation=c.rotation;}
        }else broken(ui("unbekannter Eintrag %1").arg(kind));
        const int layer=int(need("LAYER"));if(layer<1||layer>layerCount)broken(QStringLiteral("LAYER=%1").arg(layer));e.layer=layer;
        e.clearance=mm("CLEAR",.4);
        if(e.type==ElementType::Pad||e.type==ElementType::SmdPad)e.solderMask=on("SOLDERMASK",true);else{e.solderMask=on("SOLDERMASK",false);e.cutout=on("CUTOUT",false);}
        if(e.type!=ElementType::Text)e.name=string("NAME");else updateStrokes(e);
        // The innermost component block gives the component number, the group blocks the groups.
        for(qsizetype k=stack.size()-1;k>=0;k--){if(stack[k]>0)e.groups.append(stack[k]);else if(!e.part)e.part=-stack[k];}
        out.append(e);
    }
    if(!stack.isEmpty())broken(ui("ein Block ohne Ende"));
    // Airwires to the pads with those ids, on both pads.
    for(const auto &[from,targets]:wires)for(int id:targets){const int to=padIds.value(id,-1);if(to<0||to==from)continue;
        if(!out[from].connections.contains(to))out[from].connections.append(to);if(!out[to].connections.contains(from))out[to].connections.append(from);}
    return out;
}
}
