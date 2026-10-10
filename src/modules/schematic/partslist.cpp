#include "partslist.h"
#include "numbering.h"
#include "text.h"
#include "language.h"
#include <QRegularExpression>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace openloch::schematic {
bool designatorLess(const QString &a,const QString &b){
    // Runs of digits compare as numbers, the rest as text.
    static const QRegularExpression parts(QStringLiteral("(\\d+|\\D+)"));
    auto ia=parts.globalMatch(a),ib=parts.globalMatch(b);
    while(ia.hasNext()&&ib.hasNext()){
        const QString x=ia.next().captured(1),y=ib.next().captured(1);
        const bool nx=x[0].isDigit(),ny=y[0].isDigit();
        if(nx&&ny){const qlonglong p=x.toLongLong(),q=y.toLongLong();if(p!=q)return p<q;continue;}
        const int c=QString::compare(x,y,Qt::CaseInsensitive);if(c)return c<0;
    }
    return ia.hasNext()?false:ib.hasNext();
}
namespace {
void collect(const QList<Item> &items,QList<const Item*> &out){
    for(const auto &i:items){
        if(i.type==ItemType::Component)out<<&i;
        else if(i.type==ItemType::Group)collect(i.children,out);
    }
}
}
QList<PartsRow> partsList(const Document &document,const PartsListOptions &options){
    QList<PartsRow> rows;
    for(int s=0;s<document.sheets.size();s++){
        if(!options.sheets.isEmpty()&&!options.sheets.contains(s))continue;
        QList<const Item*> parts;collect(document.sheets[s].items,parts);
        for(const auto *c:parts){
            if(!c->inPartsList)continue;
            // Listed as shown (with the drawing's prefix and sheet number), grouped by the letters as entered.
            const QString shown=shownDesignator(*c,{&document,s,c,options.fileName});
            PartsRow r;r.letters=designatorLetters(c->designator);r.designators<<shown;r.value=c->value;
            for(int k=0;k<4;k++)r.extra<<c->extra.value(k);
            if(options.merge){
                bool merged=false;
                for(auto &x:rows)if(x.letters==r.letters&&x.value==r.value&&x.extra==r.extra){x.designators<<shown;merged=true;break;}
                if(merged)continue;
            }
            rows<<r;
        }
    }
    for(auto &r:rows)std::sort(r.designators.begin(),r.designators.end(),designatorLess);
    QHash<QString,int> sizes;for(const auto &r:rows)sizes[r.letters]+=r.count();
    std::stable_sort(rows.begin(),rows.end(),[&](const PartsRow &a,const PartsRow &b){
        if(a.letters!=b.letters){
            if(options.sort==PartsListOptions::Sort::Frequency&&sizes[a.letters]!=sizes[b.letters])return sizes[a.letters]>sizes[b.letters];
            return QString::compare(a.letters,b.letters,Qt::CaseInsensitive)<0;
        }
        // In a group single parts come before merged rows, as in the reference.
        if((a.count()==1)!=(b.count()==1))return a.count()==1;
        return designatorLess(a.designators.value(0),b.designators.value(0));
    });
    return rows;
}
PartsTable partsTable(const QList<PartsRow> &rows,const QList<int> &extras){
    PartsTable t;t.header<<ui("Bezeichner")<<ui("Anzahl")<<ui("Wert");
    for(int k:extras)t.header<<ui("Zusatztext %1").arg(k+1);
    for(const auto &r:rows){
        QStringList cells{r.designators.join(u','),QString::number(r.count()),r.value};
        for(int k:extras)cells<<r.extra.value(k);
        t.rows<<cells;
    }
    return t;
}
QString partsText(const PartsTable &table,const QList<int> &groupStarts,QChar separator,bool header,bool emptyLines){
    QStringList lines;
    if(header)lines<<table.header.join(separator);
    for(int i=0;i<table.rows.size();i++){
        if(emptyLines&&i>0&&groupStarts.contains(i))lines<<QString();
        lines<<table.rows[i].join(separator);
    }
    return lines.join(u'\n')+u'\n';
}
namespace {
Item tableGroup(const PartsTable &table,QPointF at,const PartsDrawing &d);
}
int fittingRows(double height,const PartsDrawing &d){
    return std::max(1,int(std::floor((height-20)/(d.height*1.5)))-(d.header?1:0));
}
Item partsGroup(const PartsTable &table,QPointF at,const PartsDrawing &d){
    if(d.rowsPerColumn<=0||table.rows.size()<=d.rowsPerColumn)return tableGroup(table,at,d);
    // Tables side by side, a row's height apart, each with the header.
    Item all;all.type=ItemType::Group;double x=0;
    for(qsizetype k=0;k<table.rows.size();k+=d.rowsPerColumn){
        PartsTable part{table.header,table.rows.mid(k,d.rowsPerColumn)};
        Item g=tableGroup(part,at+QPointF(x,0),d);x+=bounds(g).width()+d.height*1.5;
        all.children<<g.children;
    }
    assignIds(all);return all;
}
namespace {
Item tableGroup(const PartsTable &table,QPointF at,const PartsDrawing &d){
    // Each column as wide as its longest text, rows a text height and a half apart.
    const double pad=d.height*.4,rowHeight=d.height*1.5;
    Font font;font.height=d.height;font.family=d.family;
    auto width=[&](const QString &s){Item t;t.type=ItemType::Text;t.font=font;t.text=s;return textRect(t,s).width();};
    QList<double> widths;
    const int columns=std::max<int>(int(table.header.size()),table.rows.isEmpty()?0:int(table.rows.first().size()));
    for(int c=0;c<columns;c++){
        double w=d.header?width(table.header.value(c)):0;
        for(const auto &r:table.rows)w=std::max(w,width(r.value(c)));
        widths<<w+2*pad;
    }
    double total=0;for(double w:widths)total+=w;
    const int first=d.header?1:0,lines=int(table.rows.size())+first;const double height=lines*rowHeight;
    Item group;group.type=ItemType::Group;
    auto line=[&](QPointF a,QPointF b,double w){Item l;l.type=ItemType::Line;l.points={at+a,at+b};l.pen.width=w;l.electrical=false;assignIds(l);group.children<<l;};
    auto text=[&](const QString &s,double x,double y,bool bold){
        if(s.isEmpty())return;
        Item t;t.type=ItemType::Text;t.text=s;t.font=font;t.font.bold=bold;t.pos=at+QPointF(x,y);assignIds(t);group.children<<t;};
    auto box=[&](QRectF r,QColor fill,double pen){
        Item f;f.type=ItemType::Rectangle;f.centre=at+r.center();f.size=r.size();f.pen.width=pen;
        if(pen<=0)f.pen.style=PenStyle::None;
        if(fill.isValid()){f.fill.style=FillStyle::Solid;f.fill.color=fill;}else f.fill.style=FillStyle::None;
        assignIds(f);return f;
    };
    // Behind the table: the shadow, then a white ground under the frame, and the grey of every second row.
    QList<Item> back;
    if(d.shadow){back<<box(QRectF(rowHeight/3,rowHeight/3,total,height),QColor(128,128,128),0);back<<box(QRectF(0,0,total,height),QColor(255,255,255),0);}
    if(d.alternate)for(int r=1;r<int(table.rows.size());r+=2)back<<box(QRectF(0,(r+first)*rowHeight,total,rowHeight),QColor(230,230,230),0);
    double x=0;
    for(int c=0;c<widths.size();c++){
        if(d.header)text(table.header.value(c),x+pad,(rowHeight-d.height)/2,true);
        for(int r=0;r<table.rows.size();r++)text(table.rows[r].value(c),x+pad,(r+first)*rowHeight+(rowHeight-d.height)/2,false);
        x+=widths[c];
        if(d.verticalLines&&c+1<widths.size())line({x,0},{x,height},.18);
    }
    if(d.header)line({0,rowHeight},{total,rowHeight},.25);
    if(d.horizontalLines)for(int r=first+1;r<lines;r++)line({0,r*rowHeight},{total,r*rowHeight},.13);
    if(d.frame)group.children.prepend(box(QRectF(0,0,total,height),QColor(),.35));
    group.children=back+group.children;
    assignIds(group);
    return group;
}
}
}
namespace openloch::schematic {
QString childColumnName(ChildColumn c){
    switch(c){
    case ChildColumn::Designator:return ui("Bezeichner");case ChildColumn::Contacts:return ui("Kontakte");case ChildColumn::Value:return ui("Wert");
    case ChildColumn::PageNumber:return ui("Blattnummer");case ChildColumn::PageName:return ui("Blattname");case ChildColumn::RowColumn:return ui("Zeile/Spalte");
    case ChildColumn::PageColumn:return ui("Blatt.Spalte");case ChildColumn::Reference:return ui("/Blatt.Koordinate-Bezeichner:Kontakte");
    }
    return {};
}
PartsTable childList(const Document &document,const QString &parentId,const ChildListOptions &o,const QString &fileName){
    PartsTable t;for(auto c:o.columns)t.header<<childColumnName(c);
    // Letters go on after Z with AA, AB, as the generated labels of the title block.
    auto letter=[](int n,bool letters){if(n<=0)return QString();if(!letters)return QString::number(n);
        QString out;for(int k=n;k>0;k=(k-1)/26)out.prepend(QChar(u'A'+(k-1)%26));return out;};
    for(const auto &child:childrenOf(document,parentId)){
        const Item &c=*child.item;const Sheet &s=document.sheets[child.sheet];const TextContext context{&document,child.sheet,&c,fileName};
        QStringList contactTexts;for(const auto *k:contacts(c))contactTexts<<shownText(*k,context);
        const QString designator=expandVariables(c.designator,context),contactsText=contactTexts.join(u'-');
        const QString page=QString::number(child.sheet+1);
        const QString row=letter(rowAt(s,c.pos),o.rowLetters),column=letter(columnAt(s,c.pos),o.columnLetters);
        QStringList cells;
        for(auto col:o.columns){
            switch(col){
            case ChildColumn::Designator:cells<<designator;break;
            case ChildColumn::Contacts:cells<<contactsText;break;
            case ChildColumn::Value:cells<<expandVariables(c.value,context);break;
            case ChildColumn::PageNumber:cells<<page;break;
            case ChildColumn::PageName:cells<<s.name;break;
            case ChildColumn::RowColumn:cells<<row+column;break;
            case ChildColumn::PageColumn:cells<<(o.slash?QStringLiteral("/"):QString())+page+u'.'+column;break;
            case ChildColumn::Reference:cells<<QStringLiteral("/%1.%2-%3:%4").arg(page,row+column,designator,contactsText);break;
            }
        }
        t.rows<<cells;
    }
    return t;
}
}
namespace openloch::schematic {
namespace {
QByteArray rtfText(const QString &s){
    QByteArray out;
    for(const QChar c:s){
        const ushort u=c.unicode();
        if(u==u'\\'||u==u'{'||u==u'}'){out+='\\';out+=char(u);}
        else if(u=='\n')out+="\\line ";
        else if(u<128)out+=char(u);
        else out+="\\u"+QByteArray::number(qint16(u))+'?';
    }
    return out;
}
}
QByteArray partsRtf(const PartsTable &table,const QStringList &lines,const QString &family,int points){
    QByteArray out="{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fswiss "+rtfText(family)+";}}\\f0\\fs"+QByteArray::number(std::clamp(points,1,800)*2)+"\n";
    for(const auto &l:lines)out+=rtfText(l)+"\\par\n";
    const int columns=int(table.header.size());QList<int> edges;int x=0;
    for(int c=0;c<columns;c++){
        qsizetype w=table.header[c].size();for(const auto &r:table.rows)w=std::max(w,r.value(c).size());
        x+=int(std::min<qsizetype>(w,60))*12*std::clamp(points,1,800)+400;edges<<x;
    }
    auto row=[&](const QStringList &cells,bool bold){
        out+="\\trowd\\trgaph70";
        for(int c=0;c<columns;c++)out+="\\clbrdrt\\brdrs\\clbrdrl\\brdrs\\clbrdrb\\brdrs\\clbrdrr\\brdrs\\cellx"+QByteArray::number(edges[c]);
        out+="\n\\pard\\intbl ";
        for(int c=0;c<columns;c++)out+=(bold?"{\\b ":"{")+rtfText(cells.value(c))+"}\\cell ";
        out+="\\row\n";
    };
    row(table.header,true);
    for(const auto &r:table.rows)row(r,false);
    return out+"}\n";
}
PartsTable partsFromRtf(const QByteArray &rtf){
    // Text by groups: destinations (font and colour tables, \\* groups) are skipped; \\cell ends a cell, \\row a row.
    QList<QStringList> rows;QStringList row;QString cell;
    QList<bool> skip{false};int unicodeSkip=0;
    auto text=[&](QChar c){if(unicodeSkip>0){unicodeSkip--;return;}if(!skip.last())cell+=c;};
    for(qsizetype i=0;i<rtf.size();i++){
        const char c=rtf[i];
        if(c=='{'){skip<<skip.last();continue;}
        if(c=='}'){if(skip.size()>1)skip.removeLast();continue;}
        if(c=='\r'||c=='\n')continue;
        if(c!='\\'){text(QChar(uchar(c)));continue;}
        if(i+1>=rtf.size())break;
        const char n=rtf[i+1];
        if(n=='\\'||n=='{'||n=='}'){text(QChar(n));i++;continue;}
        if(n=='\''){if(i+3<rtf.size()){const QByteArray b=QByteArray::fromHex(rtf.mid(i+2,2));if(!b.isEmpty())text(QString::fromLatin1(b).at(0));}i+=3;continue;}
        if(n=='*'){skip.last()=true;i++;continue;}
        // A control word with an optional number, ended by one space.
        qsizetype j=i+1;while(j<rtf.size()&&std::isalpha(uchar(rtf[j])))j++;
        const QByteArray word=rtf.mid(i+1,j-i-1);
        qsizetype k=j;if(k<rtf.size()&&(rtf[k]=='-'||std::isdigit(uchar(rtf[k])))){k++;while(k<rtf.size()&&std::isdigit(uchar(rtf[k])))k++;}
        const QByteArray number=rtf.mid(j,k-j);
        if(k<rtf.size()&&rtf[k]==' ')k++;
        i=k-1;
        if(word=="fonttbl"||word=="colortbl"||word=="stylesheet"||word=="info"||word=="pict")skip.last()=true;
        else if(word=="u"){const int v=number.toInt();text(QChar(ushort(v<0?v+65536:v)));unicodeSkip=1;}
        else if(word=="cell"){row<<cell;cell.clear();}
        else if(word=="row"){if(!row.isEmpty())rows<<row;row.clear();cell.clear();}
        else if(word=="line")text(QChar(u'\n'));
        else if(word=="par"&&rows.isEmpty()&&row.isEmpty())cell.clear();   // text before the table
        else if(word=="tab")text(QChar(u'\t'));
    }
    PartsTable t;
    if(rows.isEmpty())return t;
    t.header=rows.takeFirst();t.rows=rows;
    return t;
}
}
