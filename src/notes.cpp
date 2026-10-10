#include "notes.h"
#include "project.h"
#include "geometry.h"
#include "language.h"
#include <QList>
#include <QMap>
#include <QStringDecoder>
#include <QDateTime>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextList>
#include <QColor>
#include <QFont>
#include <cmath>
#include <functional>
#include <algorithm>
namespace openloch {
static const QString &cp1252High(){static const QString high=QString::fromUtf8("€\u0081‚ƒ„…†‡ˆ‰Š‹Œ\u008dŽ\u008f\u0090‘’“”•–—˜™š›œ\u009džŸ");return high;}
static QChar fromCp1252(quint8 c){return c>=128&&c<160?cp1252High()[c-128]:QChar(c);}
QByteArray rtfNotes(const QString &text){
    QByteArray r="{\\rtf1\\ansi\\ansicpg1252\\uc1{\\fonttbl{\\f0 Arial;}}\\f0\\fs20 ";
    for(auto c:text){auto u=c.unicode();if(c=='\r')continue;if(c=='\n')r+="\\par\n";else if(c=='\t')r+="\\tab ";else if(c=='{'||c=='}'||c=='\\'){r+='\\';r+=char(u);}else if(u>=32&&u<127)r+=char(u);else r+="\\u"+QByteArray::number(qint16(u))+"?";}return r+'}';
}
QString plainNotes(const QByteArray &rtf){
    if(!rtf.startsWith("{\\rtf"))return QString::fromUtf8(rtf);
    struct State{bool skip=false;int fallback=1;};State state;QList<State> stack;QString result;int ignore=0;
    auto append=[&](quint8 c){if(ignore){--ignore;return;}if(!state.skip)result+=fromCp1252(c);};
    for(qsizetype i=0;i<rtf.size();){char c=rtf[i++];if(c=='{'){stack.append(state);continue;}if(c=='}'){if(!stack.isEmpty())state=stack.takeLast();continue;}
        if(c=='\r'||c=='\n')continue;if(c!='\\'){append(quint8(c));continue;}if(i>=rtf.size())break;char next=rtf[i++];
        if(next=='\\'||next=='{'||next=='}'){append(quint8(next));continue;}if(next=='*'){state.skip=true;continue;}
        if(next=='\''){if(i+1<rtf.size()){bool ok=false;int value=rtf.mid(i,2).toInt(&ok,16);if(ok)append(quint8(value));i+=2;}continue;}
        if(next=='~'){append(' ');continue;}if(!((next>='a'&&next<='z')||(next>='A'&&next<='Z')))continue;
        QByteArray word(1,next);while(i<rtf.size()&&((rtf[i]>='a'&&rtf[i]<='z')||(rtf[i]>='A'&&rtf[i]<='Z')))word+=rtf[i++];
        QByteArray number;if(i<rtf.size()&&rtf[i]=='-')number+=rtf[i++];while(i<rtf.size()&&rtf[i]>='0'&&rtf[i]<='9')number+=rtf[i++];if(i<rtf.size()&&rtf[i]==' ')i++;
        int value=number.toInt();if(word=="fonttbl"||word=="colortbl"||word=="stylesheet"||word=="info"||word=="pict"||word=="object")state.skip=true;
        else if(word=="bin")i=qMin(rtf.size(),i+qMax(0,value));else if(word=="uc")state.fallback=qBound(0,value,16);
        else if(word=="u"){if(!state.skip)result+=QChar(quint16(value));ignore=state.fallback;}
        else if(!state.skip&&(word=="par"||word=="line"))result+='\n';else if(!state.skip&&word=="tab")result+='\t';
    }return result;
}
void readRtf(const QByteArray &rtf,QTextDocument &document){
    document.clear();QTextCursor cursor(&document);
    if(!rtf.startsWith("{\\rtf")){cursor.insertText(QString::fromUtf8(rtf));return;}
    struct Char{int font=-1,size=24,colour=0;bool bold=false,italic=false,underline=false;};
    struct Paragraph{int align=0,left=0,first=0,right=0;bool bullet=false;};
    enum Destination{Text,Skip,Fonts,Colours};
    struct State{Char c;Destination destination=Text;int fallback=1;};
    State state;QList<State> stack;Paragraph paragraph;int defaultFont=0,ignore=0;
    QMap<int,QString> fonts;QList<QColor> colours;int fontNumber=0;QString fontName;int red=0,green=0,blue=0;bool colourSet=false;
    QString pending;bool pendingBlock=false;QTextList *list=nullptr;
    auto format=[&]{
        QTextCharFormat f;const auto family=fonts.value(state.c.font<0?defaultFont:state.c.font);if(!family.isEmpty())f.setFontFamilies(QStringList{family});
        f.setFontPointSize(state.c.size/2.0);f.setFontWeight(state.c.bold?QFont::Bold:QFont::Normal);f.setFontItalic(state.c.italic);f.setFontUnderline(state.c.underline);
        if(state.c.colour>0&&state.c.colour<colours.size()&&colours[state.c.colour].isValid())f.setForeground(colours[state.c.colour]);return f;
    };
    auto openBlock=[&]{if(pendingBlock){cursor.insertBlock();pendingBlock=false;}};
    auto flush=[&]{if(pending.isEmpty())return;openBlock();cursor.insertText(pending,format());pending.clear();};
    auto finishParagraph=[&]{
        QTextBlockFormat block;block.setAlignment(paragraph.align==1?Qt::AlignRight:paragraph.align==2?Qt::AlignHCenter:paragraph.align==3?Qt::AlignJustify:Qt::AlignLeft);
        if(!paragraph.bullet){block.setLeftMargin(paragraph.left/15.0);block.setTextIndent(paragraph.first/15.0);}block.setRightMargin(paragraph.right/15.0);cursor.setBlockFormat(block);
        if(!paragraph.bullet)list=nullptr;else if(list)list->add(cursor.block());else list=cursor.createList(QTextListFormat::ListDisc);
    };
    auto text=[&](QChar c){
        if(ignore){--ignore;return;}
        if(state.destination==Fonts){if(c==';'){fonts[fontNumber]=fontName.trimmed();fontName.clear();}else fontName+=c;return;}
        if(state.destination==Colours){if(c==';'){colours.append(colourSet?QColor(red,green,blue):QColor());red=green=blue=0;colourSet=false;}return;}
        if(state.destination==Text)pending+=c;
    };
    for(qsizetype i=0;i<rtf.size();){
        const char c=rtf[i++];
        if(c=='{'){flush();stack.append(state);continue;}
        if(c=='}'){flush();if(stack.isEmpty())break;state=stack.takeLast();if(stack.isEmpty())break;continue;}
        if(c=='\r'||c=='\n'||c=='\0')continue;
        if(c!='\\'){text(fromCp1252(quint8(c)));continue;}
        if(i>=rtf.size())break;const char next=rtf[i++];
        if(next=='\\'||next=='{'||next=='}'){text(QChar(next));continue;}
        if(next=='*'){state.destination=Skip;continue;}
        if(next=='\''){if(i+1<rtf.size()){bool ok=false;const int value=rtf.mid(i,2).toInt(&ok,16);if(ok)text(fromCp1252(quint8(value)));i+=2;}continue;}
        if(next=='~'){text(QChar(0xa0));continue;}if(next=='_'){text(QChar(0x2011));continue;}
        if(!((next>='a'&&next<='z')||(next>='A'&&next<='Z'))){continue;}
        QByteArray word(1,next);while(i<rtf.size()&&((rtf[i]>='a'&&rtf[i]<='z')||(rtf[i]>='A'&&rtf[i]<='Z')))word+=rtf[i++];
        QByteArray number;if(i<rtf.size()&&rtf[i]=='-')number+=rtf[i++];while(i<rtf.size()&&rtf[i]>='0'&&rtf[i]<='9')number+=rtf[i++];if(i<rtf.size()&&rtf[i]==' ')i++;
        const bool has=!number.isEmpty();const int value=number.toInt();const bool on=!has||value!=0;
        // Characters first: they do not change the formatting.
        if(word=="u"){if(state.destination==Text&&!ignore)pending+=QChar(quint16(value));else if(ignore)--ignore;ignore=state.fallback;continue;}
        static const QMap<QByteArray,QChar> symbols{{"tab",'\t'},{"line",QChar::LineSeparator},{"emdash",QChar(0x2014)},{"endash",QChar(0x2013)},{"bullet",QChar(0x2022)},
            {"lquote",QChar(0x2018)},{"rquote",QChar(0x2019)},{"ldblquote",QChar(0x201c)},{"rdblquote",QChar(0x201d)},{"emspace",QChar(0x2003)},{"enspace",QChar(0x2002)}};
        if(symbols.contains(word)){text(symbols[word]);continue;}
        if(word=="pnlvlblt"){paragraph.bullet=true;continue;}  // also inside the skipped {\*\pn …} group
        if(word=="bin"){i=qMin(rtf.size(),i+qMax(0,value));continue;}
        if(state.destination==Fonts&&word=="f"){fontNumber=value;continue;}
        if(state.destination==Colours){if(word=="red")red=value;else if(word=="green")green=value;else if(word=="blue")blue=value;colourSet=true;continue;}
        if(state.destination==Skip)continue;
        flush();
        if(word=="fonttbl")state.destination=Fonts;else if(word=="colortbl")state.destination=Colours;
        else if(QList<QByteArray>{"stylesheet","info","pict","object","header","footer","headerl","headerr","footerl","footerr","pntext","pn","listtext","fldinst","themedata","colorschememapping","latentstyles","datastore","xmlnstbl","listtable","listoverridetable","rsidtbl","generator"}.contains(word))state.destination=Skip;
        else if(word=="uc")state.fallback=qBound(0,value,16);else if(word=="deff")defaultFont=value;
        else if(word=="par"){openBlock();finishParagraph();pendingBlock=true;}
        else if(word=="pard")paragraph=Paragraph{};
        else if(word=="ql")paragraph.align=0;else if(word=="qr")paragraph.align=1;else if(word=="qc")paragraph.align=2;else if(word=="qj")paragraph.align=3;
        else if(word=="li")paragraph.left=value;else if(word=="fi")paragraph.first=value;else if(word=="ri")paragraph.right=value;
        else if(word=="plain")state.c=Char{};else if(word=="f")state.c.font=value;else if(word=="fs")state.c.size=has&&value>0?value:24;else if(word=="cf")state.c.colour=value;
        else if(word=="b")state.c.bold=on;else if(word=="i")state.c.italic=on;else if(word=="ulnone")state.c.underline=false;
        else if(word=="ul"||word=="uld"||word=="uldb"||word=="uldash"||word=="ulw"||word=="ulth"||word=="ulwave")state.c.underline=on;
    }
    flush();if(!pendingBlock)finishParagraph();
}
QByteArray writeRtf(const QTextDocument &document){
    auto familyOf=[&](const QTextCharFormat &f){const auto families=f.fontFamilies().toStringList();return families.isEmpty()?document.defaultFont().family():families.first();};
    auto sizeOf=[&](const QTextCharFormat &f){const double size=f.fontPointSize()>0?f.fontPointSize():document.defaultFont().pointSizeF();return qMax(1,int(std::lround(size*2)));};
    auto escape=[](const QString &text){
        QByteArray out;
        for(const QChar c:text){
            const auto u=c.unicode();
            if(c=='\\'||c=='{'||c=='}'){out+='\\';out+=char(u);}else if(c=='\t')out+="\\tab ";else if(c==QChar::LineSeparator)out+="\\line ";
            else if(u>=32&&u<128)out+=char(u);else if(u>=160&&u<256)out+="\\'"+QByteArray::number(u,16);
            else{const int high=int(cp1252High().indexOf(c));if(high>=0&&u>=256)out+="\\'"+QByteArray::number(128+high,16);else if(u>=32)out+="\\u"+QByteArray::number(qint16(u))+"?";}
        }
        return out;
    };
    QStringList fonts;QList<QColor> colours;bool bullets=false;
    for(auto block=document.begin();block!=document.end();block=block.next()){
        bullets|=block.textList()!=nullptr;
        for(auto it=block.begin();!it.atEnd();++it){const auto f=it.fragment().charFormat();if(!fonts.contains(familyOf(f)))fonts.append(familyOf(f));if(f.hasProperty(QTextFormat::ForegroundBrush)&&!colours.contains(f.foreground().color()))colours.append(f.foreground().color());}
    }
    if(fonts.isEmpty())fonts.append(document.defaultFont().family());
    const int symbol=int(fonts.size());
    QByteArray out="{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1031{\\fonttbl";
    for(int i=0;i<fonts.size();i++)out+="{\\f"+QByteArray::number(i)+"\\fnil\\fcharset0 "+escape(fonts[i])+";}";
    if(bullets)out+="{\\f"+QByteArray::number(symbol)+"\\fnil\\fcharset2 Symbol;}";
    out+="}\r\n";
    if(!colours.isEmpty()){out+="{\\colortbl ;";for(const auto &c:colours)out+="\\red"+QByteArray::number(c.red())+"\\green"+QByteArray::number(c.green())+"\\blue"+QByteArray::number(c.blue())+";";out+="}\r\n";}
    out+="\\viewkind4\\uc1";
    int font=-1,size=-1,colour=0;bool bold=false,italic=false,underline=false;QByteArray paragraph;
    for(auto block=document.begin();block!=document.end();block=block.next()){
        const auto b=block.blockFormat();const auto align=b.alignment();QByteArray controls="\\pard";
        if(block.textList())controls+="{\\*\\pn\\pnlvlblt\\pnf"+QByteArray::number(symbol)+"\\pnindent0{\\pntxtb\\'B7}}\\fi-200\\li200";
        else{
            const int left=int(std::lround(b.leftMargin()*15)),first=int(std::lround(b.textIndent()*15)),right=int(std::lround(b.rightMargin()*15));
            if(first)controls+="\\fi"+QByteArray::number(first);if(left)controls+="\\li"+QByteArray::number(left);if(right)controls+="\\ri"+QByteArray::number(right);
        }
        if(align&Qt::AlignHCenter)controls+="\\qc";else if(align&Qt::AlignRight)controls+="\\qr";else if(align&Qt::AlignJustify)controls+="\\qj";
        bool word=false;
        if(controls!=paragraph){out+=controls;paragraph=controls;word=true;}
        if(block.textList()){out+="{\\pntext\\f"+QByteArray::number(symbol)+"\\'B7\\tab}";word=false;}
        auto set=[&](const QByteArray &control){out+=control;word=true;};
        auto character=[&](const QTextCharFormat &f){
            const int fi=int(fonts.indexOf(familyOf(f))),fs=sizeOf(f),cf=f.hasProperty(QTextFormat::ForegroundBrush)?int(colours.indexOf(f.foreground().color()))+1:0;
            if(cf!=colour){set("\\cf"+QByteArray::number(cf));colour=cf;}
            if((f.fontWeight()>=QFont::DemiBold)!=bold){bold=!bold;set(bold?"\\b":"\\b0");}
            if(f.fontItalic()!=italic){italic=!italic;set(italic?"\\i":"\\i0");}
            if(f.fontUnderline()!=underline){underline=!underline;set(underline?"\\ul":"\\ulnone");}
            if(fi!=font){set("\\f"+QByteArray::number(fi));font=fi;}
            if(fs!=size){set("\\fs"+QByteArray::number(fs));size=fs;}
        };
        // An empty paragraph keeps the size of its paragraph mark, like RichEdit writes it.
        if(block.begin().atEnd())character(block.charFormat());
        for(auto it=block.begin();!it.atEnd();++it){
            const auto fragment=it.fragment();character(fragment.charFormat());
            const auto text=escape(fragment.text());if(text.isEmpty())continue;
            if(word&&!text.startsWith('\\'))out+=' ';out+=text;word=false;
        }
        out+="\\par\r\n";
    }
    out+="}\r\n";out+='\0';return out;
}
namespace {
struct Record {QString key,value,description;int number=0;QList<Record> children;};
// Kennung "R#" with the number, as the original keeps them; a written-out "R7" counts as "R#" number 7.
std::pair<QString,int> splitId(const QString &id,int number){
    const auto hash=id.indexOf('#');if(hash>=0)return {id.left(hash)+'#',number};
    static const QRegularExpression trailing("^(.*\\D)(\\d+)$");const auto m=trailing.match(id);if(m.hasMatch())return {m.captured(1)+'#',m.captured(2).toInt()};
    return {id,number};
}
QString expand(const QString &key,int number){const auto hash=key.indexOf('#');return hash<0?key:key.left(hash)+QString::number(number);}
// The original's order: parts before units, then R, C, D and T, other Kennungen in order of appearance, each by number.
void sortRecords(QList<Record> &records,bool units){
    for(auto &r:records)sortRecords(r.children,units);
    if(units)std::stable_partition(records.begin(),records.end(),[](const Record &r){return r.children.isEmpty();});
    QMap<QString,int> first;for(int i=0;i<records.size();i++)if(!first.contains(records[i].key))first[records[i].key]=i;
    auto rank=[](const Record &r){static const QStringList order{"R#","C#","D#","T#"};const auto k=order.indexOf(r.key);return k<0?4:int(k);};
    std::stable_sort(records.begin(),records.end(),[&](const Record &a,const Record &b){
        if(rank(a)!=rank(b))return rank(a)<rank(b);if(a.key!=b.key)return first[a.key]<first[b.key];return a.number<b.number;});
}
}
QList<NoteLine> partsListNotes(const Project &project,int mode,const QString &projectName,const QDateTime &when,const QString &user){
    QList<NoteLine> out;auto add=[&](const QString &text,bool bold=false,int size=10){out.append({text,bold,size});};
    add(ui(mode?"Einkaufsliste für ":"Stückliste für ")+project.title,true,20);
    add("");add(ui("Projekt: ")+projectName);add(ui("Erstellt am %1 um %2").arg(uiDate(when.date()),uiTime(when.time())));add(ui("von ")+user);add("");
    const auto placed=project.placedObjects();
    // Parts listed in the parts list ("Erscheint in Stückliste"), with the parts they contain.
    QList<Record> records;
    std::function<void(const QJsonObject&,QList<Record>&)> collect=[&](const QJsonObject &n,QList<Record> &into){
        if(n["deleted"].toBool()||!n.contains("children"))return;
        const auto flags=n["group_flags"].toArray();const bool selection=n["label"]=="OpenLochSelectionGroup"&&n["id"].toString().isEmpty();
        const bool component=!selection&&(flags.isEmpty()||flags.at(0).toBool()),listed=flags.size()<2||flags.at(1).toBool();
        if(!component||!listed){for(const auto &child:n["children"].toArray())collect(child.toObject(),into);return;}
        const auto [key,number]=splitId(n["id"].toString(),n["group_value"].toInt());Record r{key,n["value"].toString(),n["description"].toString(),number,{}};
        for(const auto &child:n["children"].toArray())collect(child.toObject(),r.children);into.append(r);
    };
    for(const auto &p:placed)collect(p.node,records);
    if(mode==0){
        sortRecords(records,true);
        struct Line{const Record *r;int depth;};QList<Line> flat;
        std::function<void(const QList<Record>&,int)> walk=[&](const QList<Record> &list,int depth){for(const auto &r:list){flat.append({&r,depth});walk(r.children,depth+1);}};walk(records,0);
        for(int i=0;i<flat.size();i++){
            const auto &r=*flat[i].r;const bool unit=!r.children.isEmpty();
            QString s=(unit?">>> ":"")+r.key;if(s.isEmpty())s="-";s=expand(s,r.number);s+=(unit?" - ":"\t")+r.description;if(!r.value.isEmpty())s+=", "+r.value;if(unit)s+=" <<<";
            add(QString(flat[i].depth,'\t')+s);
            if(i+1<flat.size()&&!flat[i+1].r->children.isEmpty())add("");
        }
    }else{
        // Order list: units dissolved, equal parts (Kennung, Wert/Typ, Beschreibung) counted together.
        QList<Record> parts;std::function<void(const QList<Record>&)> leaves=[&](const QList<Record> &list){for(const auto &r:list){if(r.children.isEmpty())parts.append(r);else leaves(r.children);}};leaves(records);
        sortRecords(parts,false);QList<bool> visited(parts.size(),false);
        for(int i=0;i<parts.size();i++){
            if(visited[i])continue;QString line;int count=0;
            for(int j=i;j<parts.size();j++){
                if(visited[j]||parts[j].key!=parts[i].key||parts[j].value!=parts[i].value||parts[j].description!=parts[i].description)continue;
                line+=expand(parts[j].key,parts[j].number)+",";visited[j]=true;if(++count%10==0){add(line);line.clear();}
            }
            if(line.endsWith(','))line.chop(1);if(!line.isEmpty())add(line);
            add(QString::number(count)+"x\t"+parts[i].description+(parts[i].value.isEmpty()?"":", "+parts[i].value));add("");
        }
    }
    // Wire bridges (kind 1) with their ends in holes of the board's pitch from the origin, rounded half to even like
    // Delphi's Round.
    add("");add(ui("Drahtbrücken:"),true);add("");
    const QPointF origin=project.origin();const double pitch=project.pitch()*100;
    auto hole=[&](QPointF p){return QPoint(int(std::nearbyint((p.x()-origin.x())/pitch)),int(std::nearbyint((p.y()-origin.y())/pitch)));};
    std::function<void(const QJsonObject&,const QTransform&)> wires=[&](const QJsonObject &o,const QTransform &t){
        if(o["deleted"].toBool())return;if(o.contains("children")){for(const auto &child:o["children"].toArray())wires(child.toObject(),t);return;}
        QPolygonF path;
        if(o["type"]=="TDraht"&&o["kind"].toInt()==1){for(const auto &v:o["path"].toArray()){const auto a=v.toArray();path<<t.map(QPointF(a.at(0).toDouble(),a.at(1).toDouble()));}}
        else if(o["type"]=="wire"){const QPointF at(o["x"].toDouble(),o["y"].toDouble());path<<t.map(at)<<(objectTransform(o,at)*t).map(QPointF(o["x2"].toDouble(),o["y2"].toDouble()));}
        if(path.size()<2)return;const auto a=hole(path.first()),b=hole(path.last());if(a==b)return;
        const auto [name,detail]=objectDescription(o);
        add(QString("(%1/%2)\t(%3/%4)\t%5; %6").arg(a.x()).arg(a.y()).arg(b.x()).arg(b.y()).arg(name,detail));
    };
    for(const auto &p:placed)wires(p.node,p.transform);
    return out;
}
}
