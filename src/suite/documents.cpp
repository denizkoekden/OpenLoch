#include "documents.h"
#include "language.h"
#include "fpl.h"
#include "documents/projectfile.h"
#include "formats/splan/splan.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>
#include <QPolygon>
#include <exception>
namespace openloch::suite {
QList<KindInfo> documentKinds(){
    return {
        {Kind::Schematic,"schematic",ui("Schaltplan"),ui("Schaltungen mit Symbolen und Leitungen zeichnen"),true},
        {Kind::Perfboard,"perfboard",ui("Lochraster"),ui("Bauteile, Drähte und Brücken auf Lochraster- und Streifenplatinen"),true},
        {Kind::Pcb,"pcb",ui("Leiterplatte"),ui("Leiterbahnen, Lötaugen und Bestückungsdruck für geätzte Platinen"),true},
        {Kind::FrontPanel,"frontpanel",ui("Frontplatte"),ui("Gehäusefronten mit Bohrungen, Ausschnitten, Beschriftungen und Skalen"),true},
    };
}
KindInfo kindInfo(Kind kind){for(const auto &info:documentKinds())if(info.kind==kind)return info;return documentKinds().first();}

std::optional<Kind> documentKind(const QString &path){
    const QString suffix=QFileInfo(path).suffix().toLower();
    if(suffix=="openloch"||suffix=="olsch"||suffix=="olpcb"||suffix=="olfp"){
        // A project: the kind of its active document (none for a kind this program does not know). A file that cannot
        // be read goes by its ending; opening it names the problem. Projects can be large and the start screen and
        // dragged files ask often, so the answer is kept while the file stays the same.
        struct Known {QDateTime modified;qint64 size=-1;std::optional<Kind> kind;};
        static QHash<QString,Known> known;
        const QFileInfo info(path);const QString key=info.absoluteFilePath();
        if(const auto it=known.constFind(key);it!=known.constEnd()&&info.exists()&&it->modified==info.lastModified()&&it->size==info.size())return it->kind;
        std::optional<Kind> kind;
        try{
            const auto project=documents::ProjectFile::load(path);const QString id=project.documents[project.indexOf(project.active)].kind;
            for(const auto &entry:documentKinds())if(entry.id==id)kind=entry.kind;
        }catch(const std::exception&){
            return suffix=="olsch"?Kind::Schematic:suffix=="olpcb"?Kind::Pcb:suffix=="olfp"?Kind::FrontPanel:Kind::Perfboard;
        }
        known.insert(key,{info.lastModified(),info.size(),kind});
        return kind;
    }
    if(suffix=="spl7"||suffix=="spl8")return Kind::Schematic;
    if(suffix=="lm4"||suffix=="lmb"||suffix=="old")return Kind::Perfboard;
    if(suffix=="lay6"||suffix=="lay")return Kind::Pcb;
    if(suffix=="fpl")return Kind::FrontPanel;
    if(suffix!="lib"&&suffix!="bak")return std::nullopt;
    // LochMaster and FrontDesigner write library pages and backups with the same 41-byte header, and LochMaster's own
    // files carry versions from 3 to 4. So the content decides: whatever FrontDesigner's reader accepts is a front
    // panel, the rest goes to the Lochraster editor as before, which names the problem if there is one. sPlan's pages
    // and backups start with sPlan's own header; its library pages are no documents (the schematic shows them).
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>256LL*1024*1024)return Kind::Perfboard;
    const QByteArray bytes=file.readAll();
    if(splan::version(bytes))return suffix=="bak"?std::optional<Kind>(Kind::Schematic):std::nullopt;
    if(bytes.trimmed().startsWith('{')){   // OpenLoch's own formats in a backup
        const QString format=QJsonDocument::fromJson(bytes).object().value("format").toString();
        if(format=="OpenLoch-Frontplatte")return Kind::FrontPanel;
        if(format=="OpenLoch PCB")return Kind::Pcb;
        return Kind::Perfboard;
    }
    for(const bool library:{suffix=="lib",suffix!="lib"}){
        try{frontdesigner::readFrontDesigner(bytes,library);return Kind::FrontPanel;}catch(const std::exception&){}
    }
    return Kind::Perfboard;
}

QString documentFilter(std::optional<Kind> first){
    auto patterns=[](const QStringList &suffixes){QStringList p;for(const auto &s:suffixes){p<<"*."+s;if(s!=s.toUpper())p<<"*."+s.toUpper();}return p.join(' ');};
    const QStringList projects{"openloch"},schematic{"olsch","spl8","spl7"},perfboard{"lm4","lmb","lib"},pcb{"lay6","lay","olpcb"},panel{"fpl","lib","olfp"},backups{"bak","old"};
    QStringList all=projects+schematic+perfboard+pcb+panel+backups;all.removeDuplicates();
    const QList<std::pair<std::optional<Kind>,QString>> entries{
        {std::nullopt,QString("%1 (%2)").arg(ui("Alle Dokumente"),patterns(all))},{std::nullopt,QString("%1 (%2)").arg(ui("OpenLoch-Projekte"),patterns(projects))},
        {Kind::Schematic,QString("%1 (%2)").arg(ui("Schaltplan"),patterns(schematic))},{Kind::Perfboard,QString("%1 (%2)").arg(ui("Lochraster"),patterns(perfboard))},
        {Kind::Pcb,QString("%1 (%2)").arg(ui("Leiterplatte"),patterns(pcb))},{Kind::FrontPanel,QString("%1 (%2)").arg(ui("Frontplatte"),patterns(panel))},
        {std::nullopt,QString("%1 (%2)").arg(ui("Sicherungen"),patterns(backups))}};
    // A window's own kind comes first, as in the dialog of its original program, with the projects of the suite (which
    // hold documents of every kind).
    QStringList filters;for(const auto &[kind,text]:entries)if(first&&kind==first){QString own=text;own.insert(own.indexOf(u'(')+1,patterns(projects)+u' ');filters<<own;}
    for(const auto &[kind,text]:entries)if(!(first&&kind==first))filters<<text;
    return filters.join(";;");
}

namespace {
// The Windows 16-colour palette of the toolbar symbols (src/icons.cpp), drawn whole pixels without smoothing.
const QColor K(0,0,0),W(255,255,255),G(192,192,192),D(128,128,128),N(0,0,128),R(255,0,0),M(128,0,0),E(0,128,0),T(0,128,128),L(0,255,0);
struct Pixels {
    QImage image{32,32,QImage::Format_ARGB32};QPainter p;
    Pixels(){image.fill(Qt::transparent);p.begin(&image);p.setRenderHint(QPainter::Antialiasing,false);}
    void dot(int x,int y,QColor c){p.fillRect(x,y,1,1,c);}
    void fill(int x,int y,int w,int h,QColor c){p.fillRect(x,y,w,h,c);}
    void line(int x1,int y1,int x2,int y2,QColor c){p.setPen(QPen(c,1));p.drawLine(x1,y1,x2,y2);}
    void frame(int x,int y,int w,int h,QColor c){line(x,y,x+w-1,y,c);line(x,y+h-1,x+w-1,y+h-1,c);line(x,y,x,y+h-1,c);line(x+w-1,y,x+w-1,y+h-1,c);}
    void box(int x,int y,int w,int h,QColor inside,QColor border=K){fill(x,y,w,h,border);fill(x+1,y+1,w-2,h-2,inside);}
    void ellipse(int x,int y,int w,int h,QColor inside,QColor border=K){p.setPen(QPen(border,1));p.setBrush(inside);p.drawEllipse(x,y,w-1,h-1);}
};
void schematic(Pixels &q){
    q.p.setPen(QPen(K,1));q.p.setBrush(W);q.p.drawPolygon(QPolygon({{4,1},{23,1},{27,5},{27,30},{4,30}}));q.line(23,1,23,5,K);q.line(23,5,27,5,K);
    q.line(11,4,21,4,N);q.line(11,4,11,6,N);q.box(9,7,5,9,W,N);q.line(11,16,11,21,N);q.line(11,21,21,21,N);   // resistor
    q.line(21,4,21,13,N);q.line(18,14,24,14,N);q.line(18,16,24,16,N);q.line(21,17,21,21,N);                     // capacitor
    q.fill(15,20,3,3,N);q.line(16,22,16,25,N);q.line(13,25,19,25,N);q.line(14,27,18,27,N);q.line(15,29,17,29,N);  // ground
}
void perfboard(Pixels &q){
    q.fill(2,5,28,22,QColor(232,200,40));q.frame(2,5,28,22,K);
    for(int x=4;x<=28;x+=3)for(int y=7;y<=25;y+=3)q.dot(x,y,QColor(160,112,24));
    q.line(7,13,24,13,D);q.box(11,11,10,5,QColor(216,200,160));for(const auto &[x,c]:{std::pair{13,R},{15,R},{17,M},{19,QColor(200,160,40)}})q.fill(x,12,1,3,c);
    q.line(8,19,22,19,QColor(0,0,255));q.line(8,20,22,20,QColor(0,0,160));                                        // wire bridge
    for(const auto &[x,y]:{std::pair{6,12},{24,12},{6,18},{22,18}})q.fill(x,y,2,3,G);
}
void circuitBoard(Pixels &q){
    q.fill(2,5,28,22,E);q.frame(2,5,28,22,K);const QColor copper(232,176,48);
    q.fill(8,10,10,2,copper);q.line(17,10,21,14,copper);q.line(18,10,22,14,copper);q.line(17,11,21,15,copper);q.fill(21,14,5,2,copper);
    q.fill(8,20,18,2,copper);
    for(const auto &[x,y]:{std::pair{4,8},{23,12},{4,18},{23,18}}){q.ellipse(x,y,6,6,copper,copper);q.fill(x+2,y+2,2,2,K);}
}
void frontPanel(Pixels &q){
    q.fill(0,6,32,21,K);q.fill(1,7,30,19,G);q.line(1,7,30,7,W);q.line(1,7,1,25,W);q.line(2,25,30,25,D);q.line(30,8,30,25,D);
    for(const auto &[x,y]:{std::pair{5,21},{3,16},{5,11},{10,9},{15,11},{17,16},{15,21}})q.dot(x,y,K);            // scale
    q.ellipse(5,11,11,11,D);q.ellipse(7,13,7,7,QColor(96,96,96),QColor(96,96,96));q.line(10,16,13,12,W);           // knob
    q.box(19,10,10,6,T);q.line(21,12,26,12,L);q.line(21,13,24,13,L);                                               // display
    q.ellipse(19,18,4,4,R);q.ellipse(25,18,4,4,E);                                                                 // lamps
    for(const auto &[x,y]:{std::pair{2,8},{28,8},{2,23},{28,23}})q.fill(x,y,2,2,D);
}
}

QIcon kindIcon(Kind kind){
    Pixels q;
    switch(kind){
    case Kind::Schematic:schematic(q);break;
    case Kind::Perfboard:perfboard(q);break;
    case Kind::Pcb:circuitBoard(q);break;
    case Kind::FrontPanel:frontPanel(q);break;
    }
    q.p.end();QIcon icon;
    for(const int size:{32,64,128})icon.addPixmap(QPixmap::fromImage(q.image.scaled(size,size,Qt::IgnoreAspectRatio,Qt::FastTransformation)));
    return icon;
}
}
