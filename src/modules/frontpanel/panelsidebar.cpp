#include "panelsidebar.h"
#include "paneleditor.h"
#include "paneldialogs.h"
#include "panelgenerators.h"
#include "panelgeometry.h"
#include "panelicons.h"
#include "panelrender.h"
#include "panelview.h"
#include "strokefont.h"
#include "fpl.h"
#include "delphistream.h"
#include "inifile.h"
#include "language.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFontComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QSaveFile>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

namespace openloch::frontpanel {
namespace {
QToolButton *tool(const QString &icon,const QString &tip,QWidget *parent){auto *b=new QToolButton(parent);b->setIcon(panelIcon(icon));b->setToolTip(tip);b->setAutoRaise(true);return b;}
// Add, remove, up and down above a list, as the original's list pages have them.
QWidget *listButtons(QWidget *parent,const std::function<void()> &add,const std::function<void()> &remove,const std::function<void(int)> &move){
    auto *row=new QWidget(parent);auto *h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);
    auto *a=tool("list-add",ui("Eintrag hinzufügen"),row),*r=tool("list-remove",ui("Eintrag löschen"),row),*u=tool("list-up",ui("Eintrag nach oben"),row),*d=tool("list-down",ui("Eintrag nach unten"),row);
    QObject::connect(a,&QToolButton::clicked,row,add);QObject::connect(r,&QToolButton::clicked,row,remove);QObject::connect(u,&QToolButton::clicked,row,[move]{move(-1);});QObject::connect(d,&QToolButton::clicked,row,[move]{move(1);});
    for(auto *b:{a,r,u,d})h->addWidget(b);h->addStretch();return row;
}
template<class T> void moveItem(QList<T> &list,QListWidget *view,int direction){
    const int i=view->currentRow(),j=i+direction;if(i<0||j<0||j>=list.size())return;list.swapItemsAt(i,j);
}
QIcon swatch(const std::function<void(QPainter&,QRectF)> &paint){QPixmap pm(36,18);pm.fill(Qt::transparent);QPainter p(&pm);p.setRenderHint(QPainter::Antialiasing);paint(p,QRectF(1,1,34,16));return QIcon(pm);}
QJsonObject penJson(const PenPreset &p){return {{"name",p.name},{"color",p.pen.color.name()},{"width",p.pen.width},{"style",int(p.pen.style)},{"tool",int(p.machining)}};}
PenPreset penOf(const QJsonObject &o){PenPreset p;p.name=o["name"].toString();p.pen.color=QColor::fromString(o["color"].toString("#000000"));p.pen.width=std::clamp(o["width"].toDouble(0.5),0.0,100.0);
    p.pen.style=PenStyle(std::clamp(o["style"].toInt(),0,4));p.machining=Machining(std::clamp(o["tool"].toInt(),0,2));return p;}
QJsonObject fillJson(const FillPreset &f){return {{"name",f.name},{"choice",fillChoice(f.fill)},{"color",f.fill.color.name()},{"color2",f.fill.color2.name()}};}
FillPreset fillOf(const QJsonObject &o){FillPreset f;f.name=o["name"].toString();setFillChoice(f.fill,std::clamp(o["choice"].toInt(1),0,11));f.fill.color=QColor::fromString(o["color"].toString("#ffffff"));f.fill.color2=QColor::fromString(o["color2"].toString("#ffffff"));return f;}
QJsonObject fontJson(const FontPreset &f){return {{"name",f.name},{"family",f.family},{"stroke",f.strokeFont},{"height",f.height},{"bold",f.bold},{"italic",f.italic}};}
FontPreset fontOf(const QJsonObject &o){FontPreset f;f.name=o["name"].toString();f.family=o["family"].toString("Arial");f.strokeFont=o["stroke"].toString();f.height=std::clamp(o["height"].toDouble(3.5),0.2,500.0);f.bold=o["bold"].toBool();f.italic=o["italic"].toBool();return f;}

// A small set of own symbols for the first library page.
QList<Element> starterSymbols(){
    QList<Element> out;const Pen black{Qt::black,0.5,PenStyle::Solid};const Fill solid{FillStyle::Solid,Qt::black,Qt::black,Gradient::None};
    auto named=[&](Element e,const QString &name,QPointF anchor){e.name=name;e.hasAnchor=true;e.anchor=anchor;out<<e;};
    {Element a=newElement(ElementType::Polygon);a.points={{0,-1},{6,-1},{6,-3},{10,0},{6,3},{6,1},{0,1}};a.pen.style=PenStyle::None;a.fill=solid;named(a,ui("Pfeil"),{0,0});}
    {Element arc=newElement(ElementType::Arc);arc.radiusX=arc.radiusY=4;arc.startAngle=120;arc.spanAngle=300;arc.pen=black;Element bar=newElement(ElementType::Line);bar.points={{0,-5},{0,-1}};bar.pen=black;
     Element g=newElement(ElementType::Group);g.children={arc,bar};named(g,ui("Ein/Aus"),{0,0});}
    {Element g=newElement(ElementType::Group);Element stem=newElement(ElementType::Line);stem.points={{0,-4},{0,0}};stem.pen=black;g.children<<stem;
     for(int i=0;i<3;i++){Element l=newElement(ElementType::Line);const double w=3-i;l.points={{-w,i*1.2},{w,i*1.2}};l.pen=black;g.children<<l;}named(g,ui("Masse"),{0,0});}
    {Element h=newElement(ElementType::Line);h.points={{-2,0},{2,0}};h.pen=black;Element v=newElement(ElementType::Line);v.points={{0,-2},{0,2}};v.pen=black;Element g=newElement(ElementType::Group);g.children={h,v};named(g,ui("Plus"),{0,0});}
    {Element h=newElement(ElementType::Line);h.points={{-2,0},{2,0}};h.pen=black;named(h,ui("Minus"),{0,0});}
    {Element ring=newElement(ElementType::Ellipse);ring.radiusX=ring.radiusY=4;ring.pen=black;Element drill=newElement(ElementType::Drill);drill.diameter=5;Element g=newElement(ElementType::Group);g.children={ring,drill};named(g,ui("LED 5 mm"),{0,0});}
    {Element drill=newElement(ElementType::Drill);drill.diameter=7;using S=ScaleParameters;S s(S::RoundLinear);
     for(auto [i,v]:{std::pair{S::RoundRange,270.0},{S::RoundRadius,11.0},{S::RoundBaseLine,0.0},{S::RoundCentre,0.0},{S::RoundTick1,1.5},{S::RoundSecond,0.0},{S::RoundTextHeight,2.0},{S::RoundDistance,2.0}})s.setValue(i,v);
     s.design[1].pen.width=0.3;
     Element sc=scale(s,QTransform());Element g=newElement(ElementType::Group);g.children={sc,drill};named(g,ui("Potentiometer 0–10"),{0,0});}
    {Element drill=newElement(ElementType::Drill);drill.diameter=6;Element g=newElement(ElementType::Group);g.children<<drill;
     for(const auto &[t,y]:{std::pair<QString,double>{ui("EIN"),-7.0},std::pair<QString,double>{ui("AUS"),7.0}}){Element text=newElement(ElementType::Text);text.text=t;text.font="Arial";const double w=naturalTextWidth(text,2.5);
        text.frame={QPointF(-w/2,y-1.25),QPointF(w/2,y-1.25),QPointF(-w/2,y+1.25)};g.children<<text;}
     named(g,ui("Kippschalter"),{0,0});}
    {Element ring=newElement(ElementType::Ellipse);ring.radiusX=ring.radiusY=5;ring.pen=black;Element drill=newElement(ElementType::Drill);drill.diameter=6;Element g=newElement(ElementType::Group);g.children={ring,drill};named(g,ui("Buchse 4 mm"),{0,0});}
    {Element r=newElement(ElementType::Rectangle);r.points={{-4,-1.5},{4,-1.5},{4,1.5},{-4,1.5}};r.pen=black;Element l=newElement(ElementType::Line);l.points={{-6,0},{6,0}};l.pen=black;
     Element g=newElement(ElementType::Group);g.children={r,l};named(g,ui("Sicherung"),{0,0});}
    return out;
}
}

QList<PenPreset> defaultPens(){
    return {{ui("Haarlinie"),{Qt::black,0,PenStyle::Solid},Machining::None},{ui("Schwarz 0,25 mm"),{Qt::black,0.25,PenStyle::Solid},Machining::None},
        {ui("Schwarz 0,5 mm"),{Qt::black,0.5,PenStyle::Solid},Machining::None},{ui("Schwarz 1 mm"),{Qt::black,1,PenStyle::Solid},Machining::None},
        {ui("Weiß 0,5 mm"),{Qt::white,0.5,PenStyle::Solid},Machining::None},{ui("Rot 0,5 mm"),{Qt::red,0.5,PenStyle::Solid},Machining::None},
        {ui("Blau 0,5 mm"),{QColor(0,0,200),0.5,PenStyle::Solid},Machining::None},{ui("Unsichtbar"),{Qt::black,0,PenStyle::None},Machining::None},
        {ui("Fräser 1 mm"),{Qt::black,1,PenStyle::Solid},Machining::Mill},{ui("Fräser 2 mm"),{Qt::black,2,PenStyle::Solid},Machining::Mill},
        {ui("Fräser 3 mm"),{Qt::black,3,PenStyle::Solid},Machining::Mill},{ui("Fräser 6 mm"),{Qt::black,6,PenStyle::Solid},Machining::Mill},
        {ui("Gravierer 0,2 mm"),{Qt::black,0.2,PenStyle::Solid},Machining::Engrave},{ui("Gravierer 0,3 mm"),{Qt::black,0.3,PenStyle::Solid},Machining::Engrave},
        {ui("Gravierer 0,5 mm"),{Qt::black,0.5,PenStyle::Solid},Machining::Engrave},{ui("Gravierer 1 mm"),{Qt::black,1,PenStyle::Solid},Machining::Engrave}};
}
QList<FillPreset> defaultFills(){
    auto f=[](const QString &n,int choice,QColor a,QColor b=Qt::white){FillPreset p;p.name=n;setFillChoice(p.fill,choice);p.fill.color=a;p.fill.color2=b;return p;};
    return {f(ui("Ohne Füllung"),1,Qt::white),f(ui("Weiß"),0,Qt::white),f(ui("Schwarz"),0,Qt::black),f(ui("Hellgrau"),0,QColor(0xd8,0xd8,0xd8)),f(ui("Dunkelgrau"),0,QColor(0x50,0x50,0x50)),
        f(ui("Rot"),0,QColor(0xd0,0x20,0x20)),f(ui("Gelb"),0,QColor(0xff,0xd8,0x20)),f(ui("Grün"),0,QColor(0x20,0xa0,0x40)),f(ui("Blau"),0,QColor(0x20,0x50,0xc0)),
        f(ui("Verlauf Grau"),9,QColor(0xf0,0xf0,0xf0),QColor(0x90,0x90,0x90)),f(ui("Verlauf Blau"),10,QColor(0x80,0xb0,0xff),QColor(0x10,0x30,0x80)),f(ui("Gitter Schwarz"),6,Qt::black)};
}
QList<FontPreset> defaultFonts(){
    auto f=[](const QString &n,double h,bool b,bool i){FontPreset p;p.name=n;p.height=h;p.bold=b;p.italic=i;return p;};
    return {f(ui("Klein"),2.5,false,false),f(ui("Normal"),3.5,false,false),f(ui("Fett"),3.5,true,false),f(ui("Groß"),5,true,false),f(ui("Titel"),8,true,true)};
}

QList<PenPreset> frontDesignerPens(const QByteArray &bytes){
    const auto ini=frontdesigner::IniFile::parse(bytes);QList<PenPreset> out;
    for(int i=0,n=std::clamp(ini.integer("Stifte","Anzahl",0),0,10000);i<n;i++){
        const QString s=QString("Stift%1").arg(i);if(!ini.hasSection(s))continue;
        PenPreset p;p.name=ini.text(s,"Name");p.pen.color=frontdesigner::colourFromDelphi(ini.integer(s,"Farbe",0));
        p.pen.width=std::clamp(ini.integer(s,"Breite",0),0,5000)/50.0;const int pattern=ini.integer(s,"Pattern",0);
        p.pen.style=pattern>=0&&pattern<=4?PenStyle(pattern):PenStyle::Solid;p.machining=Machining(std::clamp(ini.integer(s,"Tool",0),0,2));out<<p;
    }
    return out;
}
// A gradient ("VerlaufStyle", as in the FPL files) belongs to a solid fill; the further key "Verlauf" has no effect.
QList<FillPreset> frontDesignerFills(const QByteArray &bytes){
    const auto ini=frontdesigner::IniFile::parse(bytes);QList<FillPreset> out;
    for(int i=0,n=std::clamp(ini.integer("Fuellung","Anzahl",0),0,10000);i<n;i++){
        const QString s=QString("Fuellung%1").arg(i);if(!ini.hasSection(s))continue;
        FillPreset f;f.name=ini.text(s,"Name");const int style=ini.integer(s,"Style",0),gradient=ini.integer(s,"VerlaufStyle",0);
        f.fill.style=style>=0&&style<=7?FillStyle(style):FillStyle::None;f.fill.gradient=f.fill.style==FillStyle::Solid&&gradient>0&&gradient<=4?Gradient(gradient):Gradient::None;
        f.fill.color=frontdesigner::colourFromDelphi(ini.integer(s,"Farbe",0xffffff),Qt::white);f.fill.color2=frontdesigner::colourFromDelphi(ini.integer(s,"VerlaufFarbe",0xffffff),Qt::white);out<<f;
    }
    return out;
}
// The name of a font entry is its description; the height is stored in fiftieths of a millimetre. Colours, pen and
// fill of the entries have no place in the font list here.
QList<FontPreset> frontDesignerFonts(const QByteArray &bytes){
    const auto ini=frontdesigner::IniFile::parse(bytes);QList<FontPreset> out;
    for(int i=0,n=std::clamp(ini.integer("Fonts","Anzahl",0),0,10000);i<n;i++){
        const QString s=QString("Font%1").arg(i);if(!ini.hasSection(s))continue;
        FontPreset f;f.family=ini.text(s,"Name","Arial");if(f.family.isEmpty())f.family="Arial";f.name=ini.text(s,"Beschreibung",f.family);
        f.height=std::clamp(ini.integer(s,"Hoehe",175)/50.0,0.2,500.0);f.bold=ini.flag(s,"Bold",false);f.italic=ini.flag(s,"Italic",false);
        if(ini.flag(s,"SHX",false))f.strokeFont=ini.text(s,"SHXName");
        out<<f;
    }
    return out;
}

PanelSidebar::PanelSidebar(PanelEditor *e):QWidget(e),editor(e){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);
    auto *row=new QHBoxLayout;auto *group=new QButtonGroup(this);
    const QList<std::pair<QString,QString>> names{{"page-symbols",ui("Symbole")},{"page-pens",ui("Stift")},{"page-fills",ui("Füllung")},{"page-views",ui("Ansicht")},{"page-fonts",ui("Schrift")}};
    for(int i=0;i<names.size();i++){auto *b=new QToolButton(this);b->setIcon(panelIcon(names[i].first));b->setText(names[i].second);b->setToolTip(names[i].second);b->setCheckable(true);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);b->setAutoRaise(true);group->addButton(b,i);row->addWidget(b);pageButtons<<b;}
    layout->addLayout(row);stack=new QStackedWidget(this);layout->addWidget(stack,1);
    loadPresets();
    stack->addWidget(symbolPage());stack->addWidget(penPage());stack->addWidget(fillPage());stack->addWidget(viewPage());stack->addWidget(fontPage());
    connect(group,&QButtonGroup::idClicked,this,[this](int id){showPage(id);});
    showPage(QSettings().value("frontpanel/sidebarPage",0).toInt());refreshLibrary();
}
void PanelSidebar::showPage(int index){index=std::clamp(index,0,4);stack->setCurrentIndex(index);pageButtons[index]->setChecked(true);QSettings().setValue("frontpanel/sidebarPage",index);}
int PanelSidebar::page() const{return stack->currentIndex();}

void PanelSidebar::loadPresets(){
    QSettings s;auto list=[&](const char *key){return QJsonDocument::fromJson(s.value(key).toByteArray()).array();};
    pens.clear();for(const auto &v:list("frontpanel/pens"))pens<<penOf(v.toObject());if(pens.isEmpty())pens=defaultPens();
    fills.clear();for(const auto &v:list("frontpanel/fills"))fills<<fillOf(v.toObject());if(fills.isEmpty())fills=defaultFills();
    fonts.clear();for(const auto &v:list("frontpanel/fonts"))fonts<<fontOf(v.toObject());if(fonts.isEmpty())fonts=defaultFonts();
}
int PanelSidebar::importFrontDesignerPresets(const QStringList &files,bool replace,QStringList *notes){
    int taken=0;
    // The original's lists may hold a name twice; each own entry is replaced at most once.
    auto merge=[&](auto &list,const auto &entries){
        if(entries.isEmpty())return;if(replace)list.clear();
        const qsizetype own=list.size();QList<bool> used(own,false);
        for(const auto &entry:entries){
            qsizetype hit=-1;for(qsizetype i=0;i<own;i++)if(!used[i]&&list[i].name==entry.name){hit=i;break;}
            if(hit<0)list<<entry;else{list[hit]=entry;used[hit]=true;}
        }
        taken+=entries.size();
    };
    for(const QString &file:files){
        QFile f(file);const QByteArray bytes=f.open(QIODevice::ReadOnly)?f.read(4<<20):QByteArray();
        const auto ini=frontdesigner::IniFile::parse(bytes);const int before=taken;
        if(ini.hasSection("Stifte"))merge(pens,frontDesignerPens(bytes));
        if(ini.hasSection("Fuellung"))merge(fills,frontDesignerFills(bytes));
        if(ini.hasSection("Fonts"))merge(fonts,frontDesignerFonts(bytes));
        if(taken==before&&notes)notes->append(ui("Keine Vorgaben in %1").arg(QFileInfo(file).fileName()));
    }
    if(taken){savePresets();fillPenList();fillFillList();fillFontList();}
    return taken;
}
void PanelSidebar::savePresets() const{
    QSettings s;QJsonArray p,f,t;for(const auto &x:pens)p.append(penJson(x));for(const auto &x:fills)f.append(fillJson(x));for(const auto &x:fonts)t.append(fontJson(x));
    s.setValue("frontpanel/pens",QJsonDocument(p).toJson(QJsonDocument::Compact));s.setValue("frontpanel/fills",QJsonDocument(f).toJson(QJsonDocument::Compact));s.setValue("frontpanel/fonts",QJsonDocument(t).toJson(QJsonDocument::Compact));
}

// ------------------------------------------------------------------ symbols
LibraryFolders symbolLibraryFolders(){
    const QString saved=QSettings().value("frontpanel/libraryFolder").toString();
    return {saved.isEmpty()?openLochDocumentsFolder(QStringLiteral("Frontplattensymbole")):saved,QSettings().value("frontpanel/extraLibraries").toStringList()};
}
void setSymbolLibraryFolders(const LibraryFolders &folders){
    QSettings().setValue("frontpanel/libraryFolder",folders.own==openLochDocumentsFolder(QStringLiteral("Frontplattensymbole"))?QString():folders.own);
    QSettings().setValue("frontpanel/extraLibraries",folders.extra);
}
QString PanelSidebar::ownLibraryFolder() const{return symbolLibraryFolders().own;}
QStringList PanelSidebar::extraLibraryFolders() const{return symbolLibraryFolders().extra;}
void PanelSidebar::setExtraLibraryFolders(const QStringList &folders){setSymbolLibraryFolders({ownLibraryFolder(),folders});refreshLibrary();}
QWidget *PanelSidebar::symbolPage(){
    auto *w=new QWidget;auto *v=new QVBoxLayout(w);v->setContentsMargins(0,0,0,0);
    pageCombo=new QComboBox;pageCombo->setToolTip(ui("Bibliotheksseite wählen"));v->addWidget(pageCombo);
    symbols=new QListWidget;symbols->setViewMode(QListView::IconMode);symbols->setIconSize(QSize(64,64));symbols->setGridSize(QSize(84,96));symbols->setResizeMode(QListView::Adjust);
    symbols->setMovement(QListView::Static);symbols->setWordWrap(true);symbols->setContextMenuPolicy(Qt::CustomContextMenu);symbols->setToolTip(ui("Ein Klick hängt das Symbol an den Mauszeiger"));v->addWidget(symbols,1);
    connect(pageCombo,&QComboBox::currentIndexChanged,this,[this](int i){loadPage(i);});
    connect(symbols,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){
        const int i=symbols->row(item);if(i<0||pageDocument.panels.isEmpty()||i>=pageDocument.panel().elements.size())return;
        Element symbol=pageDocument.panel().elements[i];renewIds(symbol);
        // Pictures of the library page come along with the symbol.
        std::function<void(const Element&)> take=[&](const Element &e){if(!e.resource.isEmpty()&&pageDocument.resources.contains(e.resource))editor->document().resources.insert(e.resource,pageDocument.resources.value(e.resource));for(const auto &c:e.children)take(c);};
        take(symbol);editor->place({symbol});
    });
    connect(symbols,&QListWidget::customContextMenuRequested,this,[this](QPoint at){
        QListWidgetItem *item=symbols->itemAt(at);const int i=item?symbols->row(item):-1;const bool writable=pageCombo->currentIndex()>=0&&pages.value(pageCombo->currentIndex()).writable;
        QMenu menu;
        if(i>=0&&writable){
            menu.addAction(ui("Nach oben setzen"),this,[this,i]{auto &list=pageDocument.panel().elements;list.move(i,0);savePage();loadPage(pageCombo->currentIndex());});
            menu.addAction(ui("Nach unten setzen"),this,[this,i]{auto &list=pageDocument.panel().elements;list.move(i,list.size()-1);savePage();loadPage(pageCombo->currentIndex());});
            menu.addAction(ui("Umbenennen…"),this,[this,i]{auto &e=pageDocument.panel().elements[i];bool ok=false;const QString n=QInputDialog::getText(this,ui("Symbol umbenennen"),ui("Name:"),QLineEdit::Normal,e.name,&ok);if(ok){e.name=n;savePage();loadPage(pageCombo->currentIndex());}});
            menu.addAction(ui("Eigenschaften…"),this,[this,i]{   // name and insertion point, as when it was added
                Element e=pageDocument.panel().elements[i];if(!editSymbol(this,pageDocument,e,editor->document().panel().grid,ui("Eigenschaften Symbol")))return;
                pageDocument.panel().elements[i]=e;savePage();loadPage(pageCombo->currentIndex());});
            menu.addAction(ui("Löschen"),this,[this,i]{if(QMessageBox::question(this,ui("Symbol löschen"),ui("Das Symbol aus der Bibliotheksseite löschen?"))!=QMessageBox::Yes)return;pageDocument.panel().elements.removeAt(i);savePage();loadPage(pageCombo->currentIndex());});
            menu.addSeparator();
        }
        menu.addAction(ui("Seite anlegen…"),this,[this]{newPage();});
        if(writable){menu.addAction(ui("Seite umbenennen…"),this,[this]{renamePage();});menu.addAction(ui("Seite löschen"),this,[this]{deletePage();});}
        menu.exec(symbols->viewport()->mapToGlobal(at));
    });
    return w;
}
void PanelSidebar::refreshLibrary(){
    const QString own=ownLibraryFolder();QDir dir(own);
    if(!dir.exists()&&QDir().mkpath(own)){
        // The first own page with a few symbols to start with.
        Document start;start.panels[0].name=ui("Grundsymbole");start.panels[0].width=start.panels[0].height=100;double x=10,y=10;
        for(auto s:starterSymbols()){const QPointF to(x,y);transformElement(s,QTransform::fromTranslate(to.x()-s.anchor.x(),to.y()-s.anchor.y()));start.panels[0].elements<<s;x+=25;if(x>90){x=10;y+=25;}}
        QSaveFile f(dir.filePath(ui("Grundsymbole")+".LIB"));if(f.open(QIODevice::WriteOnly)){f.write(frontdesigner::writeFrontDesignerLibrary(start,0));f.commit();}
    }
    const QString previous=pageCombo->currentText();pages.clear();
    auto scan=[&](const QString &folder,bool writable){
        for(const auto &info:QDir(folder).entryInfoList({"*.lib","*.LIB","*.Lib"},QDir::Files,QDir::Name|QDir::IgnoreCase)){
            QString name=info.completeBaseName();
            // The page name is the panel name stored in the file when it has one.
            QFile f(info.filePath());if(f.open(QIODevice::ReadOnly)){const QByteArray head=f.read(41);if(head.size()==41){const int n=std::min(36,int(quint8(head[0])));const QString title=frontdesigner::fromWindows1252(head.mid(1,n)).trimmed();if(!title.isEmpty())name=title;}}
            pages<<LibraryPage{name,info.filePath(),writable};
        }
    };
    scan(own,true);for(const auto &folder:extraLibraryFolders())scan(folder,false);
    std::sort(pages.begin(),pages.end(),[](const LibraryPage &a,const LibraryPage &b){return QString::localeAwareCompare(a.name,b.name)<0;});
    const QSignalBlocker block(pageCombo);pageCombo->clear();for(const auto &p:pages)pageCombo->addItem(p.writable?p.name:p.name+" 🔒");
    int index=0;for(int i=0;i<pages.size();i++)if(pageCombo->itemText(i)==previous)index=i;
    pageCombo->setCurrentIndex(pages.isEmpty()?-1:index);loadPage(pageCombo->currentIndex());
}
void PanelSidebar::loadPage(int index){
    symbols->clear();pageDocument=Document();if(index<0||index>=pages.size())return;
    try{pageDocument=frontdesigner::loadFrontDesigner(pages[index].path);}catch(const std::exception &e){symbols->addItem(ui("Seite nicht lesbar: %1").arg(QString::fromUtf8(e.what())));return;}
    for(const auto &e:pageDocument.panel().elements){
        QPixmap pm(64,64);pm.fill(Qt::white);{QPainter p(&pm);p.setRenderHint(QPainter::Antialiasing);const QRectF b=elementBounds(e).adjusted(-0.5,-0.5,0.5,0.5);
            if(b.isValid()){const double s=std::min(60/b.width(),60/b.height());p.translate(32,32);p.scale(s,s);p.translate(-b.center());paintElement(p,pageDocument,e);}}
        symbols->addItem(new QListWidgetItem(QIcon(pm),e.name.isEmpty()?typeTitle(e.type):e.name));
    }
}
void PanelSidebar::savePage(){
    const int index=pageCombo->currentIndex();if(index<0||!pages[index].writable)return;
    QSaveFile f(pages[index].path);if(!f.open(QIODevice::WriteOnly)||f.write(frontdesigner::writeFrontDesignerLibrary(pageDocument,0))<0||!f.commit())QMessageBox::warning(this,ui("Bibliothek"),ui("Die Seite konnte nicht gespeichert werden: %1").arg(f.errorString()));
}
bool PanelSidebar::addSymbol(const Element &symbol){
    int index=pageCombo->currentIndex();
    if(index<0||!pages[index].writable){newPage();index=pageCombo->currentIndex();if(index<0||!pages[index].writable)return false;}
    Element s=symbol;renewIds(s);
    // Lay the symbol out next to the others on the page.
    QRectF used;for(const auto &e:pageDocument.panel().elements)used=used.isNull()?elementBounds(e):used.united(elementBounds(e));
    const QRectF b=elementBounds(s);const QPointF to=used.isNull()?QPointF(5,5):QPointF(5,used.bottom()+5);transformElement(s,QTransform::fromTranslate(to.x()-b.left(),to.y()-b.top()));
    Panel &p=pageDocument.panel();p.elements<<s;p.height=std::max(p.height,elementBounds(s).bottom()+5);p.width=std::max(p.width,elementBounds(s).right()+5);
    std::function<void(const Element&)> take=[&](const Element &e){if(!e.resource.isEmpty())pageDocument.resources.insert(e.resource,editor->document().resources.value(e.resource));for(const auto &c:e.children)take(c);};take(s);
    savePage();loadPage(index);return true;
}
void PanelSidebar::newPage(){
    bool ok=false;const QString name=QInputDialog::getText(this,ui("Seite anlegen"),ui("Name der neuen Bibliotheksseite:"),QLineEdit::Normal,{},&ok).trimmed();if(!ok||name.isEmpty())return;
    QDir().mkpath(ownLibraryFolder());QString file=QDir(ownLibraryFolder()).filePath(QString(name).replace(QRegularExpression("[\\\\/:*?\"<>|]"),"_")+".LIB");
    Document d;d.panels[0].name=name;d.panels[0].width=d.panels[0].height=100;QSaveFile f(file);
    if(!f.open(QIODevice::WriteOnly)||f.write(frontdesigner::writeFrontDesignerLibrary(d,0))<0||!f.commit()){QMessageBox::warning(this,ui("Bibliothek"),f.errorString());return;}
    refreshLibrary();for(int i=0;i<pages.size();i++)if(pages[i].path==file)pageCombo->setCurrentIndex(i);
}
void PanelSidebar::renamePage(){
    const int index=pageCombo->currentIndex();if(index<0||!pages[index].writable)return;bool ok=false;
    const QString name=QInputDialog::getText(this,ui("Seite umbenennen"),ui("Name:"),QLineEdit::Normal,pages[index].name,&ok).trimmed();if(!ok||name.isEmpty())return;
    pageDocument.panel().name=name;savePage();refreshLibrary();
}
void PanelSidebar::deletePage(){
    const int index=pageCombo->currentIndex();if(index<0||!pages[index].writable)return;
    if(QMessageBox::question(this,ui("Seite löschen"),ui("Die Bibliotheksseite „%1“ mit allen Symbolen löschen?").arg(pages[index].name))!=QMessageBox::Yes)return;
    QFile::moveToTrash(pages[index].path)||QFile::remove(pages[index].path);refreshLibrary();
}

// ------------------------------------------------------------------ pens
QWidget *PanelSidebar::penPage(){
    auto *w=new QWidget;auto *v=new QVBoxLayout(w);v->setContentsMargins(0,0,0,0);
    auto *tools=new QHBoxLayout;auto *group=new QButtonGroup(w);const QStringList icons{"pen-draw","pen-mill","pen-engrave"};const QStringList tips{ui("Kontur mit dem Stift zeichnen"),ui("Kontur fräsen"),ui("Kontur gravieren")};
    for(int i=0;i<3;i++){toolButtons[i]=tool(icons[i],tips[i],w);toolButtons[i]->setCheckable(true);group->addButton(toolButtons[i],i);tools->addWidget(toolButtons[i]);}
    tools->addStretch();v->addLayout(tools);
    auto *grid=new QGridLayout;penColor=new ColorButton;penWidth=new QDoubleSpinBox;penWidth->setRange(0,20);penWidth->setDecimals(2);penWidth->setSingleStep(0.1);penWidth->setSuffix(" mm");penWidth->setLocale(uiLocale());
    penStyle=new QComboBox;penStyle->addItems(penStyleNames());
    grid->addWidget(new QLabel(ui("Farbe:")),0,0);grid->addWidget(penColor,0,1);grid->addWidget(new QLabel(ui("Breite:")),1,0);grid->addWidget(penWidth,1,1);grid->addWidget(new QLabel(ui("Stil:")),2,0);grid->addWidget(penStyle,2,1);v->addLayout(grid);
    penList=new QListWidget;penList->setIconSize(QSize(36,18));
    v->addWidget(listButtons(w,[this]{bool ok=false;const QString n=QInputDialog::getText(this,ui("Stift aufnehmen"),ui("Name:"),QLineEdit::Normal,{},&ok);if(!ok)return;pens<<PenPreset{n,editor->currentPen,editor->currentMachining};savePresets();fillPenList();},
        [this]{QListWidgetItem *item=penList->currentItem();if(!item)return;pens.removeAt(item->data(Qt::UserRole).toInt());savePresets();fillPenList();},
        [this](int d){QListWidgetItem *item=penList->currentItem();if(!item)return;const int i=item->data(Qt::UserRole).toInt();int j=i;
            for(int k=i+d;k>=0&&k<pens.size();k+=d)if(pens[k].machining==pens[i].machining){j=k;break;}if(j!=i){pens.swapItemsAt(i,j);savePresets();fillPenList();}}));
    v->addWidget(penList,1);
    connect(group,&QButtonGroup::idClicked,this,[this](int){penControlsChanged();fillPenList();});
    penColor->changed=[this](const QColor&){penControlsChanged();};
    connect(penWidth,&QDoubleSpinBox::valueChanged,this,[this]{penControlsChanged();});connect(penStyle,&QComboBox::currentIndexChanged,this,[this]{penControlsChanged();});
    connect(penList,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){const auto &p=pens.value(item->data(Qt::UserRole).toInt());editor->applyPen(p.pen,p.machining);syncFromEditor();});
    connect(penList,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *item){const int i=item->data(Qt::UserRole).toInt();bool ok=false;const QString n=QInputDialog::getText(this,ui("Umbenennen"),ui("Name:"),QLineEdit::Normal,pens[i].name,&ok);if(ok){pens[i].name=n;savePresets();fillPenList();}});
    return w;
}
void PanelSidebar::fillPenList(){
    penList->clear();const int tool=std::max(0,int(editor->currentMachining));
    for(int i=0;i<pens.size();i++){if(int(pens[i].machining)!=tool)continue;const Pen pen=pens[i].pen;const Machining m=pens[i].machining;
        auto *item=new QListWidgetItem(swatch([pen,m](QPainter &p,QRectF r){p.fillRect(r,Qt::white);QPen q(m==Machining::Mill?milledColor():m==Machining::Engrave?QColor(Qt::gray):pen.color,std::clamp(pen.width*4,1.0,14.0));
            if(pen.style==PenStyle::None)q.setStyle(Qt::NoPen);q.setCapStyle(Qt::FlatCap);p.setPen(q);p.drawLine(QPointF(r.left()+2,r.center().y()),QPointF(r.right()-2,r.center().y()));}),pens[i].name);
        item->setData(Qt::UserRole,i);penList->addItem(item);}
}
void PanelSidebar::penControlsChanged(){
    if(syncing)return;Pen pen{penColor->color(),penWidth->value(),PenStyle(penStyle->currentIndex())};const int tool=std::max(0,toolButtons[1]->isChecked()?1:toolButtons[2]->isChecked()?2:0);
    editor->applyPen(pen,Machining(tool));const bool drawing=tool==0;penColor->setEnabled(drawing);penStyle->setEnabled(drawing);
}

// ------------------------------------------------------------------ fills
QWidget *PanelSidebar::fillPage(){
    auto *w=new QWidget;auto *v=new QVBoxLayout(w);v->setContentsMargins(0,0,0,0);auto *grid=new QGridLayout;
    fillStyle=new QComboBox;fillStyle->addItems(fillChoices());fillColor=new ColorButton;fillColor2=new ColorButton;auto *swap=tool("swap",ui("Farben tauschen"),w);
    grid->addWidget(new QLabel(ui("Stil:")),0,0);grid->addWidget(fillStyle,0,1,1,2);grid->addWidget(new QLabel(ui("Farbe:")),1,0);grid->addWidget(fillColor,1,1);grid->addWidget(swap,1,2);
    grid->addWidget(new QLabel(ui("Verlauf:")),2,0);grid->addWidget(fillColor2,2,1);v->addLayout(grid);
    fillList=new QListWidget;fillList->setIconSize(QSize(36,18));
    v->addWidget(listButtons(w,[this]{bool ok=false;const QString n=QInputDialog::getText(this,ui("Füllung aufnehmen"),ui("Name:"),QLineEdit::Normal,{},&ok);if(!ok)return;fills<<FillPreset{n,editor->currentFill};savePresets();fillFillList();},
        [this]{const int i=fillList->currentRow();if(i<0)return;fills.removeAt(i);savePresets();fillFillList();},[this](int d){moveItem(fills,fillList,d);savePresets();const int i=fillList->currentRow()+d;fillFillList();fillList->setCurrentRow(i);}));
    v->addWidget(fillList,1);
    connect(fillStyle,&QComboBox::currentIndexChanged,this,[this]{fillControlsChanged();});fillColor->changed=[this](const QColor&){fillControlsChanged();};fillColor2->changed=[this](const QColor&){fillControlsChanged();};
    connect(swap,&QToolButton::clicked,this,[this]{const QColor a=fillColor->color();fillColor->setColor(fillColor2->color());fillColor2->setColor(a);fillControlsChanged();});
    connect(fillList,&QListWidget::itemClicked,this,[this](QListWidgetItem*){const int i=fillList->currentRow();if(i<0)return;editor->applyFill(fills[i].fill);syncFromEditor();});
    connect(fillList,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem*){const int i=fillList->currentRow();bool ok=false;const QString n=QInputDialog::getText(this,ui("Umbenennen"),ui("Name:"),QLineEdit::Normal,fills[i].name,&ok);if(ok){fills[i].name=n;savePresets();fillFillList();}});
    fillFillList();return w;
}
void PanelSidebar::fillFillList(){
    fillList->clear();
    for(const auto &f:fills){Document none;Element sample=newElement(ElementType::Rectangle);sample.points={{0,0},{34,0},{34,16},{0,16}};sample.fill=f.fill;sample.pen=Pen{Qt::black,0,PenStyle::Solid};
        fillList->addItem(new QListWidgetItem(swatch([sample,none](QPainter &p,QRectF r){p.translate(r.topLeft());RenderOptions o;o.machiningLook=false;paintElement(p,none,sample,o);}),f.name));}
}
void PanelSidebar::fillControlsChanged(){
    if(syncing)return;Fill f;setFillChoice(f,fillStyle->currentIndex());f.color=fillColor->color();f.color2=fillColor2->color();fillColor2->setEnabled(f.gradient!=Gradient::None);editor->applyFill(f);
}

// ------------------------------------------------------------------ views
QWidget *PanelSidebar::viewPage(){
    auto *w=new QWidget;auto *v=new QVBoxLayout(w);v->setContentsMargins(0,0,0,0);auto *grid=new QGridLayout;
    auto button=[&](const QString &icon,const QString &tip,int row,int column,const std::function<void()> &run){auto *b=tool(icon,tip,w);b->setAutoRepeat(true);grid->addWidget(b,row,column);connect(b,&QToolButton::clicked,this,run);};
    button("zoom-in",ui("Vergrößern"),0,0,[this]{editor->view()->zoomBy(1.25);});button("zoom-out",ui("Verkleinern"),0,1,[this]{editor->view()->zoomBy(0.8);});
    button("zoom-all",ui("Alle Objekte zeigen"),0,2,[this]{editor->view()->fitElements(false);});button("zoom-marked",ui("Markierte Objekte zeigen"),0,3,[this]{editor->view()->fitElements(true);});
    button("zoom-board",ui("Ganze Frontplatte zeigen"),0,4,[this]{editor->view()->fitPanel();});
    button("scroll-left",ui("Nach links"),1,1,[this]{editor->view()->scrollStep(-1,0);});button("scroll-up",ui("Nach oben"),1,2,[this]{editor->view()->scrollStep(0,-1);});
    button("scroll-down",ui("Nach unten"),1,3,[this]{editor->view()->scrollStep(0,1);});button("scroll-right",ui("Nach rechts"),1,4,[this]{editor->view()->scrollStep(1,0);});
    v->addLayout(grid);viewList=new QListWidget;
    v->addWidget(listButtons(w,[this]{auto &views=editor->document().views;View view;view.name=ui("Ansicht %1").arg(views.size()+1);view.panel=editor->document().activePanel;view.area=editor->view()->visibleArea();
            editor->change([&](Document &d){d.views<<view;});refreshViews();viewList->setCurrentRow(viewList->count()-1);},
        [this]{const int i=viewList->currentRow();if(i<0)return;editor->change([&](Document &d){d.views.removeAt(i);});refreshViews();},
        [this](int dir){const int i=viewList->currentRow(),j=i+dir;if(i<0||j<0||j>=editor->document().views.size())return;editor->change([&](Document &d){d.views.swapItemsAt(i,j);});refreshViews();viewList->setCurrentRow(j);}));
    v->addWidget(viewList,1);
    connect(viewList,&QListWidget::itemClicked,this,[this](QListWidgetItem*){const int i=viewList->currentRow();const auto &views=editor->document().views;if(i<0||i>=views.size())return;
        if(views[i].panel!=editor->document().activePanel)editor->selectPanel(views[i].panel);editor->view()->showArea(views[i].area);});
    connect(viewList,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem*){const int i=viewList->currentRow();bool ok=false;
        const QString n=QInputDialog::getText(this,ui("Ansicht umbenennen"),ui("Name:"),QLineEdit::Normal,editor->document().views.value(i).name,&ok);if(ok)editor->change([&](Document &d){d.views[i].name=n;});refreshViews();});
    return w;
}
void PanelSidebar::refreshViews(){
    if(!viewList)return;const int row=viewList->currentRow();viewList->clear();const auto &d=editor->document();
    for(const auto &v:d.views)viewList->addItem(v.name+(v.panel<d.panels.size()?" · "+d.panels[v.panel].name:QString()));viewList->setCurrentRow(std::min(row,viewList->count()-1));
}

// ------------------------------------------------------------------ fonts
QWidget *PanelSidebar::fontPage(){
    auto *w=new QWidget;auto *v=new QVBoxLayout(w);v->setContentsMargins(0,0,0,0);
    auto *kinds=new QHBoxLayout;ttfButton=new QToolButton;ttfButton->setText("TTF");ttfButton->setToolTip(ui("Konturschrift (TrueType, gefüllt)"));shxButton=new QToolButton;shxButton->setText("SHX");shxButton->setToolTip(ui("Strichschrift (einlinig, für Gravuren)"));
    for(auto *b:{ttfButton,shxButton}){b->setCheckable(true);b->setAutoExclusive(true);kinds->addWidget(b);}
    boldButton=new QToolButton;boldButton->setText(ui("fett"));boldButton->setCheckable(true);italicButton=new QToolButton;italicButton->setText(ui("kursiv"));italicButton->setCheckable(true);
    kinds->addWidget(boldButton);kinds->addWidget(italicButton);kinds->addStretch();v->addLayout(kinds);
    fontCombo=new QFontComboBox;strokeEdit=new QComboBox;strokeEdit->setEditable(true);strokeEdit->addItems(strokeFontNames());strokeEdit->lineEdit()->setPlaceholderText(ui("Name der Strichschrift"));
    strokeEdit->setToolTip(ui("Einlinige Schrift (SHX oder SHP) für Gravuren; über „Optionen › Strichschrift installieren…“ kommen weitere hinzu"));fontHeight=new QDoubleSpinBox;fontHeight->setRange(0.5,600);fontHeight->setDecimals(2);fontHeight->setSingleStep(0.5);fontHeight->setSuffix(" mm");fontHeight->setLocale(uiLocale());
    auto *grid=new QGridLayout;grid->addWidget(new QLabel(ui("Schrift:")),0,0);grid->addWidget(fontCombo,0,1);grid->addWidget(strokeEdit,1,1);grid->addWidget(new QLabel(ui("Höhe:")),2,0);grid->addWidget(fontHeight,2,1);v->addLayout(grid);
    fontList=new QListWidget;
    v->addWidget(listButtons(w,[this]{bool ok=false;const QString n=QInputDialog::getText(this,ui("Schrift aufnehmen"),ui("Name:"),QLineEdit::Normal,{},&ok);if(!ok)return;
            fonts<<FontPreset{n,editor->currentFont,editor->currentStrokeFont,editor->currentTextHeight,editor->currentBold,editor->currentItalic};savePresets();fillFontList();},
        [this]{QListWidgetItem *item=fontList->currentItem();if(!item)return;fonts.removeAt(item->data(Qt::UserRole).toInt());savePresets();fillFontList();},
        [this](int d){QListWidgetItem *item=fontList->currentItem();if(!item)return;const int i=item->data(Qt::UserRole).toInt();int j=i;
            for(int k=i+d;k>=0&&k<fonts.size();k+=d)if(fonts[k].strokeFont.isEmpty()==fonts[i].strokeFont.isEmpty()){j=k;break;}if(j!=i){fonts.swapItemsAt(i,j);savePresets();fillFontList();}}));
    v->addWidget(fontList,1);
    auto changedSlot=[this]{fontControlsChanged();};
    connect(ttfButton,&QToolButton::clicked,this,changedSlot);connect(shxButton,&QToolButton::clicked,this,changedSlot);connect(boldButton,&QToolButton::clicked,this,changedSlot);connect(italicButton,&QToolButton::clicked,this,changedSlot);
    connect(fontCombo,&QFontComboBox::currentFontChanged,this,changedSlot);connect(strokeEdit,&QComboBox::textActivated,this,changedSlot);connect(strokeEdit->lineEdit(),&QLineEdit::editingFinished,this,changedSlot);connect(fontHeight,&QDoubleSpinBox::valueChanged,this,changedSlot);
    connect(fontList,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){const auto &f=fonts.value(item->data(Qt::UserRole).toInt());editor->applyFont(f.family,f.height,f.bold,f.italic,f.strokeFont);syncFromEditor();});
    connect(fontList,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *item){const int i=item->data(Qt::UserRole).toInt();bool ok=false;const QString n=QInputDialog::getText(this,ui("Umbenennen"),ui("Name:"),QLineEdit::Normal,fonts[i].name,&ok);if(ok){fonts[i].name=n;savePresets();fillFontList();}});
    fillFontList();return w;
}
void PanelSidebar::fillFontList(){
    // Outline and stroke fonts have their own lists, as in the original.
    fontList->clear();const bool stroke=shxButton&&shxButton->isChecked();
    for(int i=0;i<fonts.size();i++){const auto &f=fonts[i];if(f.strokeFont.isEmpty()==stroke)continue;
        QFont q(f.family);q.setBold(f.bold);q.setItalic(f.italic);q.setPointSizeF(10);
        auto *item=new QListWidgetItem(QString("%1 · %2 · %3 mm").arg(f.name,f.strokeFont.isEmpty()?f.family:f.strokeFont,uiLocale().toString(f.height,'g',4)));
        item->setFont(q);item->setData(Qt::UserRole,i);fontList->addItem(item);}
}
void PanelSidebar::fontControlsChanged(){
    if(syncing)return;const bool stroke=shxButton->isChecked();fontCombo->setVisible(!stroke);strokeEdit->setVisible(stroke);
    QString name=strokeEdit->currentText().trimmed();if(stroke&&name.isEmpty())name=strokeFontNames().value(0,QStringLiteral("DIN1451"));
    editor->applyFont(fontCombo->currentFont().family(),fontHeight->value(),boldButton->isChecked(),italicButton->isChecked(),stroke?name:QString());fillFontList();
}

void PanelSidebar::refreshStrokeFonts(){const QString current=strokeEdit->currentText();const QSignalBlocker block(strokeEdit);strokeEdit->clear();strokeEdit->addItems(strokeFontNames());strokeEdit->setCurrentText(current);}
void PanelSidebar::syncFromEditor(){
    syncing=true;
    const int tool=int(editor->currentMachining);for(int i=0;i<3;i++)toolButtons[i]->setChecked(i==tool);
    penColor->setColor(editor->currentPen.color);penWidth->setValue(editor->currentPen.width);penStyle->setCurrentIndex(int(editor->currentPen.style));penColor->setEnabled(tool==0);penStyle->setEnabled(tool==0);fillPenList();
    fillStyle->setCurrentIndex(fillChoice(editor->currentFill));fillColor->setColor(editor->currentFill.color);fillColor2->setColor(editor->currentFill.color2);fillColor2->setEnabled(editor->currentFill.gradient!=Gradient::None);
    const bool stroke=!editor->currentStrokeFont.isEmpty();ttfButton->setChecked(!stroke);shxButton->setChecked(stroke);fontCombo->setVisible(!stroke);strokeEdit->setVisible(stroke);
    fontCombo->setCurrentFont(QFont(editor->currentFont));strokeEdit->setCurrentText(editor->currentStrokeFont);fontHeight->setValue(editor->currentTextHeight);boldButton->setChecked(editor->currentBold);italicButton->setChecked(editor->currentItalic);
    fillFontList();syncing=false;
}
}
