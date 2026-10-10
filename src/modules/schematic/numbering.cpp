#include "numbering.h"
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <tuple>

namespace openloch::schematic {
QString designatorLetters(const QString &designator){
    static const QRegularExpression number(QStringLiteral("(\\d+|\\?)$"));
    return QString(designator).remove(number);
}
QString partKind(const Item &component){
    // OpenLoch/<folder>/<page>/<caption>
    const auto path=component.libraryEntry.split(u'/');
    if(path.size()>=4&&path.first()==u"OpenLoch"){
        const QString page=path[path.size()-2],folder=path.mid(1,path.size()-3).join(u'/');
        if(page.startsWith(u"Widerstände")||page==u"RES VAR")return QStringLiteral("R");
        if(page==u"Kondensatoren")return QStringLiteral("C");
        if(page==u"Spulen"||page==u"IND")return QStringLiteral("L");
        if(page.startsWith(u"Dioden")||page==u"DIODE 10")return path.last().startsWith(u"Brückengleichrichter")?QStringLiteral("B"):QStringLiteral("D");
        if(page==u"LED")return QStringLiteral("D");
        if(page==u"Transistoren"||page==u"FET"||page==u"TRANSISTOR")return QStringLiteral("T");
        if(page==u"OP's"||page==u"Spannungsregler"||folder.contains(u"/Digital")||page==u"ANLG"||page==u"CD 40xxx"||page==u"GATEs"||page==u"FLIP FLOP")return QStringLiteral("IC");
        if(page==u"Stecker, Buchsen, Klemmen"||page==u"Klemmen"||page==u"CON 1")return QStringLiteral("X");
        if(page.startsWith(u"Schalter")||page==u"SWITCH")return QStringLiteral("S");
        if(page==u"Sicherungen")return QStringLiteral("F");
        if(page.startsWith(u"Schütze"))return QStringLiteral("K");
    }
    QString designator=component.designator.trimmed();
    const auto sign=std::max({designator.lastIndexOf(u'-'),designator.lastIndexOf(u'='),designator.lastIndexOf(u'+')});
    if(sign>=0)designator=designator.mid(sign+1);
    return designatorLetters(designator);
}
namespace {
void collect(QList<Item> &items,QList<Item*> &out){
    for(auto &i:items){
        if(i.type==ItemType::Component)out<<&i;
        else if(i.type==ItemType::Group)collect(i.children,out);
    }
}
}
int renumber(Document &document,const NumberingOptions &options){
    QHash<QString,int> next;int count=0;
    for(int s=0;s<document.sheets.size();s++){
        if(!options.sheets.isEmpty()&&!options.sheets.contains(s))continue;
        QList<Item*> parts;collect(document.sheets[s].items,parts);
        QList<Item*> chosen;
        for(auto *c:parts){
            if(!c->autoNumber)continue;
            if(!options.only.isEmpty()&&!options.only.contains(c->id))continue;
            const QString letters=designatorLetters(c->designator);
            if(letters.isEmpty()||(!options.letters.isEmpty()&&letters!=options.letters))continue;
            chosen<<c;
        }
        if(options.order!=NumberingOptions::Order::None){
            const double r=std::max(.1,options.raster);const bool columns=options.order==NumberingOptions::Order::Columns;
            auto key=[&](const Item *c){
                const QPointF p=c->pos;const double cx=std::floor(p.x()/r),cy=std::floor(p.y()/r);
                return columns?std::tuple(cx,cy,p.y(),p.x()):std::tuple(cy,cx,p.x(),p.y());
            };
            std::stable_sort(chosen.begin(),chosen.end(),[&](const Item *a,const Item *b){return key(a)<key(b);});
        }
        for(auto *c:chosen){
            const QString letters=designatorLetters(c->designator);
            const int n=next.value(letters,options.start);next[letters]=n+1;
            c->designator=letters+QString::number(n);count++;
        }
    }
    return count;
}
}
