// Tests of the stroke fonts: a small shape font of its own, written as source text (.shp) and compiled (.shx).
// With OPENLOCH_STROKE_FONT_CORPUS set to a folder of local fonts, each is read; with OPENLOCH_STROKE_FONT_SAMPLES set
// to a folder, a sample line of each is drawn there for a look.
#include "strokefont.h"
#include "delphistream.h"
#include "fdshapes.h"
#include "panelgeometry.h"
#include "panelmachining.h"
#include "panelrender.h"
#include "paneldialogs.h"
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QListWidget>
#include <QPainter>
#include <QSettings>
#include <QStringEncoder>
#include <QTemporaryDir>
#include <QTimer>
#include <QtEndian>
#include <cmath>
#include <cstring>

using namespace openloch::frontpanel;

namespace fptest {
void require(bool ok,const char *message);
bool near(double a,double b,double tolerance);
}
using fptest::require;using fptest::near;

namespace {
// Shapes as (number, name, specification) in the notation of the source format.
struct Shape {int number;const char *name;const char *spec;};
const Shape shapes[]={
    {0x49,"I","1,0A4,2,8,(2,-10),0"},                              // a stroke up, then on to the next letter
    {0x4c,"L","1,0A4,2,8,(0,-10),1,040,2,8,(2,0),0"},              // up and back down, then along the base line
    {0x4f,"O","2,8,(10,5),1,10,(5,000),2,8,(2,-5),0"},             // a full circle from octant 0
    {0x53,"S","1,12,(4,0,127),2,8,(2,0),0"},                       // half a circle below the chord
    {0x43,"C","2,8,(0,5),1,10,(5,-022),2,8,(2,0),0"},              // a quarter circle clockwise from 90°
    {0x45,"E","1,5,8,(0,10),6,8,(4,0),0"},                         // up, back to the saved place without drawing, along
    {0x44,"D","7,049,0"},                                          // the I as a subshape
    {142,"AE","1,8,(0,6),2,8,(2,-6),0"},                           // Ä at its DOS code
    {0xc4,"AEw","1,8,(0,3),2,8,(2,-3),0"},                         // a smaller Ä at its Windows code
    {0x20,"space","2,060,0"},
};
QByteArray shpText(){
    QByteArray t="; own test font\r\n*0,4,Testschrift\r\n10,2,2,0\r\n";
    for(const auto &s:shapes){const int count=QByteArray(s.spec).count(',')+1;t+="*"+QByteArray::number(s.number)+","+QByteArray::number(count)+","+s.name+"\r\n"+s.spec+"\r\n";}
    return t;
}
// Compiled like AutoCAD: negative numbers in two's complement, but octant values (codes 10 and 11) as their magnitude
// with bit 7 set. Enough of the commands for the shapes here.
QByteArray specBytes(const char *spec){
    QList<int> values;for(QByteArray token:QByteArray(spec).split(',')){token.replace("(","").replace(")","");token=token.trimmed();
        const bool negative=token.startsWith('-');if(negative)token=token.mid(1);bool ok=false;int v=token.size()>1&&token.startsWith('0')?token.toInt(&ok,16):token.toInt(&ok,10);
        values<<(negative?-v:v);}
    QByteArray out;for(int v:values)out.append(char(v&0xff));
    static const int length[15]={1,1,1,2,2,1,1,2,3,1,3,6,4,1,1};
    for(int i=0;i<values.size();){
        const int c=values[i],k=c==10?i+2:c==11?i+5:-1;
        if(k>=0&&k<values.size()&&values[k]<0)out[k]=char(0x80|(-values[k]&0x7f));
        i+=c>=0&&c<15?length[c]:1;
    }
    return out;
}
QByteArray shxBytes(){
    QList<std::pair<int,QByteArray>> list{{0,QByteArray("Testschrift")+'\0'+QByteArray("\x0a\x02\x02\x00",4)}};
    for(const auto &s:shapes)list.append({s.number,QByteArray(s.name)+'\0'+specBytes(s.spec)});
    QByteArray b="AutoCAD-86 shapes 1.0\r\n\x1a";auto u16=[&](int v){char c[2];qToLittleEndian<quint16>(quint16(v),c);b.append(c,2);};
    u16(0);u16(255);u16(list.size());for(const auto &[n,d]:list){u16(n);u16(d.size());}for(const auto &[n,d]:list)b+=d;
    return b;
}
// A font in FrontDesigner's own format with an H, an O (an arc) and a p (a descender).
QByteArray fhxBytes(){
    openloch::frontdesigner::DelphiWriter w;
    auto line=[&](double x1,double y1,double x2,double y2){w.integer(0);w.real(x1);w.real(y1);w.real(x2);w.real(y2);};
    for(int code=1;code<=112;code++){
        if(code=='H'){w.integer(50);w.integer(7);w.integer(2);line(5,0,5,-100);w.integer(2);line(45,0,45,-100);w.integer(2);line(5,-50,45,-50);w.integer(2);}
        else if(code=='O'){w.integer(60);w.integer(3);w.integer(2);w.integer(1);for(double v:{10.0,-50.0,50.0,-50.0,30.0,-50.0,20.0})w.real(v);w.boolean(true);w.integer(2);}
        else if(code=='p'){w.integer(40);w.integer(3);w.integer(2);line(5,30,5,-60);w.integer(2);}
        else{w.integer(0);w.integer(0);}
    }
    return w.bytes;
}
bool samePath(const QPainterPath &a,const QPainterPath &b){
    if(a.elementCount()!=b.elementCount())return false;
    for(int i=0;i<a.elementCount();i++)if(std::abs(a.elementAt(i).x-b.elementAt(i).x)>1e-9||std::abs(a.elementAt(i).y-b.elementAt(i).y)>1e-9)return false;
    return true;
}
// The original's letters for the codes 1 to 255: the same advances and elements, bit for bit.
bool sameLetters(const QHash<int,StrokeGlyph> &a,const QHash<int,StrokeGlyph> &b){
    for(int code=1;code<=255;code++){
        const StrokeGlyph x=a.value(code),y=b.value(code);
        if(x.advance!=y.advance||x.elements.size()!=y.elements.size())return false;
        for(int k=0;k<x.elements.size();k++){
            const StrokeElement &e=x.elements[k],&f=y.elements[k];
            const double u[7]={e.x1,e.y1,e.x2,e.y2,e.cx,e.cy,e.radius},v[7]={f.x1,f.y1,f.x2,f.y2,f.cx,f.cy,f.radius};
            if(e.kind!=f.kind||e.counterClockwise!=f.counterClockwise||std::memcmp(u,v,sizeof u)!=0)return false;
        }
    }
    return true;
}
bool sameRect(const QRectF &a,const QRectF &b,double tolerance){return near(a.left(),b.left(),tolerance)&&near(a.top(),b.top(),tolerance)&&near(a.right(),b.right(),tolerance)&&near(a.bottom(),b.bottom(),tolerance);}
// An own big font with the escape range 0x81–0x9F: an A (one byte), a square as part 0x8140, a character 0x8142 that
// draws the part into two boxes of half the height, and 0x8143 whose box command is only for vertical text.
// Extended big fonts give a width before the height of each box.
QByteArray bigfontBytes(bool extended){
    const QByteArray box1=extended?"7,0,081,040,0,0,5,5":"7,0,081,040,0,0,5",box2=extended?"7,0,081,040,5,5,5,5":"7,0,081,040,5,5,5";
    QList<std::pair<int,QByteArray>> list{
        {0,QByteArray("Testgross")+'\0'+(extended?QByteArray("\x0a\x00\x00\x0a\x00\x00",6):QByteArray("\x0a\x02\x00\x00",4))},
        {0x41,QByteArray("A")+'\0'+specBytes("8,(0,10),2,8,(6,-10),0")},
        {0x8140,QByteArray("Quadrat")+'\0'+specBytes("8,(10,0),8,(0,10),8,(-10,0),8,(0,-10),0")},
        {0x8142,QByteArray("Zwei")+'\0'+specBytes(box1+","+box2+",2,8,(12,0),0")},
        {0x8143,QByteArray("Senkrecht")+'\0'+specBytes("14,"+box1+",8,(3,0),0")}};
    QByteArray b="AutoCAD-86 bigfont 1.0\r\n\x1a";auto u16=[&](int v){char c[2];qToLittleEndian<quint16>(quint16(v),c);b.append(c,2);};
    auto u32=[&](quint32 v){char c[4];qToLittleEndian<quint32>(v,c);b.append(c,4);};
    u16(10);u16(list.size());u16(1);u16(0x81);u16(0x9f);
    quint32 offset=quint32(b.size()+list.size()*8);for(const auto &[n,d]:list){u16(n);u16(d.size());u32(offset);offset+=d.size();}
    for(const auto &[n,d]:list)b+=d;
    return b;
}
void bigfonts(){
    const StrokeFont f=StrokeFont::fromShx(bigfontBytes(false));
    require(f.bigfont&&!f.extended&&f.name=="Testgross"&&f.above==10&&f.below==2&&f.escapes==QList<std::pair<int,int>>{{0x81,0x9f}}&&f.shapes.size()==4,"big font read");
    double advance=0;QPainterPath path=f.text("A",&advance);require(near(advance,6,1e-12)&&sameRect(path.boundingRect(),QRectF(0,0,0,10),1e-12),"one-byte letter of a big font");
    // 0x8142 is "。" in Shift_JIS; the parts are drawn in boxes of half the height, the pen comes back after each.
    if(QStringEncoder("Shift_JIS").isValid()){
        path=f.text(QString(QChar(0x3002)),&advance);
        require(near(advance,12,1e-12)&&sameRect(path.boundingRect(),QRectF(0,0,10,10),1e-9)&&path.toSubpathPolygons().size()==2,"parts of a big font character");
        path=f.text(QString(QChar(0xff0c)),&advance);require(near(advance,3,1e-12),"box command only for vertical text");   // 0x8143
    }
    const StrokeFont e=StrokeFont::fromShx(bigfontBytes(true));require(e.extended&&e.above==10&&e.shapes.size()==4,"extended big font read");
    if(QStringEncoder("Shift_JIS").isValid()){path=e.text(QString(QChar(0x3002)),&advance);require(near(advance,12,1e-12)&&sameRect(path.boundingRect(),QRectF(0,0,10,10),1e-9),"parts of an extended big font character");}
    // The same font as source text.
    const QByteArray source="*BIGFONT 4,1,081,09F\r\n*0,4,Testgross\r\n10,2,0,0\r\n*041,6,A\r\n8,(0,10),2,8,(6,-10),0\r\n"
        "*08140,13,Quadrat\r\n8,(10,0),8,(0,10),8,(-10,0),8,(0,-10),0\r\n*08142,19,Zwei\r\n7,0,081,040,0,0,5,7,0,081,040,5,5,5,2,8,(12,0),0\r\n";
    const StrokeFont shp=StrokeFont::fromShp(source);
    require(shp.bigfont&&shp.name=="Testgross"&&shp.escapes==f.escapes&&shp.shapes.value(0x8142)==f.shapes.value(0x8142)&&shp.shapes.value(0x41)==f.shapes.value(0x41),"big font as source text");
}
}

// The module's own font: all printable ASCII and Windows-1252 characters and the signs of panels, letters within their
// cells, listed, found by name and standing in for missing fonts, engraved as strokes.
void ownFont(){
    const StrokeFont &f=ownStrokeFont();
    require(f.name=="Normschrift"&&f.unicode&&f.above==10&&f.below==3,"own stroke font");
    QString needed;for(char16_t c=0x20;c<0x7f;c++)needed+=QChar(c);
    needed+=QString::fromUtf8("ÄÖÜäöüßÀÁÂÃÅÆÇÈÉÊËÌÍÎÏÑÒÓÔÕØÙÚÛÝàáâãåæçèéêëìíîïñòóôõøùúûýÿŠšŽžŒœŸ€„“”‚‘’–—…°±µΩ×÷²³½¼¾§©®«»¢£¥¡¿·");
    for(QChar c:needed)require(f.glyphs.contains(c.unicode()),"own stroke font lacks a character");
    for(auto it=f.glyphs.cbegin();it!=f.glyphs.cend();++it){
        const QRectF b=it.value().path.boundingRect();
        require(it.key()==' '||!it.value().path.isEmpty(),"empty letter in the own stroke font");
        require(it.key()==' '||(b.left()>=-1&&b.right()<=it.value().advance+0.6&&b.top()>=-3.5&&b.bottom()<=13.5),"letter outside its cell");
    }
    double advance=0;const QPainterPath hi=f.text("Hi",&advance);require(near(advance,10,1e-9)&&sameRect(hi.boundingRect(),QRectF(0,0,8.35,10),1e-6),"own stroke font spacing");
    require(strokeFontNames().contains("Normschrift")&&findStrokeFont("normschrift")==&f&&strokeFontFor("Keine solche Schrift")==&f&&!findStrokeFont("Keine solche Schrift")
        &&strokeFontFor("")==nullptr,"own stroke font as substitute");
    Element t=newElement(ElementType::Text);t.text="AB";t.strokeFont="Keine solche Schrift";t.frame=rectFrame(QRectF(0,0,20,13));t.machining=Machining::Engrave;
    require(strokeText(t)&&!textPath(t).isEmpty(),"a text in a missing stroke font is drawn with the own one");
    Document d;d.panels[0].elements={t};const auto jobs=machiningJobs(d,d.panels[0],MachiningOptions());
    require(jobs.size()==1&&jobs[0].kind==MachiningJob::Engrave&&!jobs[0].paths.isEmpty(),"engraving with the own stroke font");
}
void runStrokeFontTests(){
    bigfonts();ownFont();
    const StrokeFont shp=StrokeFont::fromShp(shpText());
    require(shp.name=="Testschrift"&&shp.above==10&&shp.below==2&&shp.shapes.size()==10,"source font read");
    double advance=0;QPainterPath path=shp.text("IL",&advance);
    require(near(advance,8,1e-9)&&sameRect(path.boundingRect(),QRectF(0,0,6,10),1e-9),"letters advance and stand on the base line");
    require(path.toSubpathPolygons().size()==3,"pen up starts a new stroke");
    path=shp.text("O",&advance);require(near(advance,12,1e-9)&&sameRect(path.boundingRect(),QRectF(0,0,10,10),1e-6),"octant circle");
    path=shp.text("S",&advance);require(near(path.boundingRect().top(),-2,1e-9)&&near(path.boundingRect().bottom(),0,1e-9)&&near(advance,6,1e-9),"bulge arc runs counter-clockwise");
    path=shp.text("C",&advance);require(near(advance,7,1e-9)&&sameRect(path.boundingRect(),QRectF(0,0,5,5),1e-6),"clockwise octant arc");
    path=shp.text("E",&advance);require(near(advance,4,1e-9)&&path.toSubpathPolygons().size()==2&&near(path.length(),14,1e-9),"saved place restored without drawing");
    require(samePath(shp.text("D"),shp.text("I")),"subshape");
    path=shp.text(QString::fromUtf8("Ä"),&advance);require(near(path.boundingRect().height(),6,1e-9)&&near(advance,2,1e-9),"German letters at their DOS codes");
    {StrokeFont windows=shp;windows.dosOrder=false;require(near(windows.text(QString::fromUtf8("Ä")).boundingRect().height(),3,1e-9),"letters at their Windows codes");}
    shp.text(QString::fromUtf8("Ω"),&advance);require(advance>0,"a missing letter leaves a gap");
    // The compiled font gives the same strokes.
    const StrokeFont shx=StrokeFont::fromShx(shxBytes());
    require(shx.name=="Testschrift"&&shx.above==10&&shx.below==2&&samePath(shx.text("ILOS DCE"),shp.text("ILOS DCE"))&&shx.shapes.value('C')==shp.shapes.value('C'),"compiled font");
    bool rejected=false;try{StrokeFont::fromShx("AutoCAD-86 shapes 1.0\r\n\x1a\x01");}catch(const std::exception&){rejected=true;}require(rejected,"truncated font rejected");
    // FrontDesigner's letters, made the same of the source text and of the compiled font. The Ä moves from its DOS code
    // to Windows-1252; the full circle by octants is two half circles, the quarter circle runs clockwise.
    {
        using namespace openloch::frontdesigner;
        const auto a=shapeLetters(shpNumbers(shpText())),b=shapeLetters(shxNumbers(shx));
        require(a&&b&&sameLetters(*a,*b),"letters made of the source text and of the compiled font");
        require(a->value(0xc4).elements.value(0).y2==-6&&a->value(142).elements.value(0).y2==-3,"DOS letter moved to Windows-1252");
        const auto w=shapeLetters(shpNumbers(shpText()),false);
        require(w&&w->value(0xc4).elements.value(0).y2==-3&&w->value(142).elements.value(0).y2==-6,"letters kept at their numbers in Windows order");
        const auto circle=a->value('O').elements;
        require(circle.size()==4&&circle[1].kind==StrokeElement::Arc&&circle[2].kind==StrokeElement::Arc&&circle[1].counterClockwise&&circle[2].counterClockwise
                &&near(circle[1].x2,0,1e-9)&&near(circle[2].x2,10,1e-9)&&near(circle[2].y2,-5,1e-9),"full circle by octants");
        const StrokeElement quarter=a->value('C').elements.value(1);
        require(quarter.kind==StrokeElement::Arc&&!quarter.counterClockwise&&near(quarter.x2,5,1e-9)&&near(quarter.y2,0,1e-9),"quarter circle clockwise");
    }
    // FrontDesigner's own font files: ready strokes per character code from 1 on, y downwards, no font height.
    {
        const StrokeFont fhx=StrokeFont::fromFhx(fhxBytes());double width=0;
        require(fhx.above==100&&fhx.below==30,"font height from the letters");
        require(sameRect(fhx.text("H",&width).boundingRect(),QRectF(5,0,40,100),1e-9)&&near(width,50,1e-9),"lines of a FrontDesigner font");
        const QRectF o=fhx.text("O").boundingRect();require(near(o.top(),30,1e-9)&&near(o.left(),10,1e-9)&&near(o.right(),50,1e-9)&&near(o.bottom(),50,1e-9),"counter-clockwise arc below its chord");
        fhx.text("Hp",&width);require(near(width,90,1e-9),"advances add up");
        bool wrong=false;try{openloch::frontdesigner::DelphiWriter bad;bad.integer(10);bad.integer(1);bad.integer(7);StrokeFont::fromFhx(bad.bytes);}catch(const std::exception&){wrong=true;}
        require(wrong,"unknown element rejected");
    }

    // Installed fonts: texts with their name are drawn as strokes in their frame.
    QTemporaryDir folder,settings;require(folder.isValid()&&settings.isValid(),"temporary folders");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    {QFile f(QDir(folder.path()).filePath("TESTSCHRIFT.SHX"));require(f.open(QIODevice::WriteOnly)&&f.write(shxBytes())>0,"write font");}
    const QStringList before=QSettings().value("frontpanel/strokeFontFolders").toStringList();
    setStrokeFontFolders({folder.path()});
    require(findStrokeFont("testschrift")&&strokeFontNames().contains("TESTSCHRIFT"),"font found by name");
    {QFile din(QDir(folder.path()).filePath("DIN1451.SHP"));require(din.open(QIODevice::WriteOnly)&&din.write(shpText())>0,"write font");}
    forgetStrokeFonts();require(findStrokeFont("SHX")==findStrokeFont("din1451")&&findStrokeFont("SHX"),"texts without a font name use the standard font");
    // The character order of shape fonts is a setting, DOS like the original unless switched off.
    require(strokeFontsInDosOrder()&&findStrokeFont("testschrift")->dosOrder,"shape fonts in DOS order");
    setStrokeFontsInDosOrder(false);require(!findStrokeFont("testschrift")->dosOrder,"shape fonts in Windows order");
    setStrokeFontsInDosOrder(true);require(findStrokeFont("testschrift")->dosOrder,"back to DOS order");
    // One font may keep an order of its own; the dialog shows where its umlauts lie.
    setStrokeFontInDosOrder("TestSchrift",false);
    require(!findStrokeFont("testschrift")->dosOrder&&findStrokeFont("din1451")->dosOrder&&ownStrokeFontOrder("TESTSCHRIFT")==false,"one font in Windows order");
    setStrokeFontsInDosOrder(false);setStrokeFontInDosOrder("din1451",true);require(findStrokeFont("din1451")->dosOrder&&!findStrokeFont("testschrift")->dosOrder,"one font in DOS order, the others in Windows order");
    setStrokeFontsInDosOrder(true);setStrokeFontInDosOrder("TestSchrift",std::nullopt);setStrokeFontInDosOrder("DIN1451",std::nullopt);
    require(findStrokeFont("testschrift")->dosOrder&&!ownStrokeFontOrder("testschrift")&&!ownStrokeFontOrder("din1451"),"back to the order for all");
    {QFile dos(QDir(folder.path()).filePath("DOSUML.SHP"));require(dos.open(QIODevice::WriteOnly)&&dos.write("*0,4,Dos\r\n10,2,2,0\r\n*142,5,AE\r\n1,8,(0,6),0\r\n*148,5,OE\r\n1,8,(0,6),0\r\n")>0,"write font");}
    forgetStrokeFonts();
    require(shapeFontNames().contains("TESTSCHRIFT")&&shapeFontNames().contains("DOSUML")&&umlautsInDosPlaces("dosuml")==true&&!umlautsInDosPlaces("testschrift"),"where the umlauts of a font lie");
    // With FrontDesigner's own file and a shape font of the same name, the letters come from the first, the font
    // height from the second.
    {QFile a(QDir(folder.path()).filePath("MIX.FHX"));require(a.open(QIODevice::WriteOnly)&&a.write(fhxBytes())>0,"write font");
     QFile b(QDir(folder.path()).filePath("MIX.SHP"));require(b.open(QIODevice::WriteOnly)&&b.write(shpText())>0,"write font");}
    forgetStrokeFonts();
    {const StrokeFont *mix=findStrokeFont("Mix");require(mix&&!mix->glyphs.isEmpty()&&mix->above==10&&mix->below==2,"letters of the original's file, height of the shape font");}
    Element t=newElement(ElementType::Text);t.text="IO";t.strokeFont="TestSchrift";t.fill=Fill{FillStyle::Solid,Qt::red,Qt::red,Gradient::None};t.pen=Pen{Qt::black,0.3,PenStyle::Solid};
    require(near(naturalTextWidth(t,12),14,1e-9),"width from the font");
    t.frame=rectFrame(QRectF(10,10,14,12));require(strokeText(t)&&sameRect(textPath(t).boundingRect(),QRectF(10,10,12,10),1e-6),"strokes fill the frame from the capital height");
    Document d;d.panels[0].width=50;d.panels[0].height=30;d.panels[0].color=Qt::white;d.panels[0].elements={t};
    RenderOptions o;o.machiningLook=false;const QImage image=renderPanel(d,d.panels[0],254,o);   // ten pixels per millimetre
    require(image.pixelColor(int((10+2+5)*10),int((10+5)*10))!=QColor(Qt::red),"stroke texts are not filled");
    t.machining=Machining::Engrave;d.panels[0].elements={t};const auto jobs=machiningJobs(d,d.panels[0],MachiningOptions());
    require(jobs.size()==1&&jobs[0].kind==MachiningJob::Engrave&&jobs[0].paths.size()==2&&jobs[0].paths[0].points.first()!=jobs[0].paths[0].points.last(),"engraving follows the strokes");
    // The character table of an installed font ("SHX…" in the text dialog): the characters it draws, a click picks one.
    {QString offered;int shown=0;QTimer::singleShot(0,[&]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget())){auto *list=d->findChild<QListWidget*>("strokeFontCharacters");
        if(const QString shot=qEnvironmentVariable("OPENLOCH_STROKE_TABLE_SCREENSHOT");!shot.isEmpty())d->grab().save(shot);
        if(list&&list->count()>0){shown=list->count();offered=list->item(0)->data(Qt::UserRole).toString();emit list->itemClicked(list->item(0));}else d->reject();}});
     const QString picked=pickStrokeFontCharacter(nullptr,"testschrift");require(shown>3&&!picked.isEmpty()&&picked==offered,"a character picked from the font's table");}
    require(pickStrokeFontCharacter(nullptr,"Keine Schrift dieses Namens").isEmpty(),"no table for a font that is not installed");
    setStrokeFontFolders(before);forgetStrokeFonts();

    // Local fonts, if a folder is given.
    if(const QString corpus=qEnvironmentVariable("OPENLOCH_STROKE_FONT_CORPUS");!corpus.isEmpty()){
        const QString samples=qEnvironmentVariable("OPENLOCH_STROKE_FONT_SAMPLES");
        QDirIterator it(corpus,{"*.shx","*.SHX","*.shp","*.SHP","*.fhx","*.FHX"},QDir::Files);
        while(it.hasNext()){
            const QString file=it.next();const StrokeFont f=StrokeFont::load(file);double width=0;
            // FrontDesigner's own file next to the source text it was made of: the letters made of the text are the same.
            const QFileInfo info(file);const QStringList sources=QDir(info.path()).entryList({info.completeBaseName()+".shp"},QDir::Files);
            if(info.suffix().compare("fhx",Qt::CaseInsensitive)==0&&!sources.isEmpty()){
                QFile text(QDir(info.path()).filePath(sources.first()));require(text.open(QIODevice::ReadOnly),"read local font");
                const auto made=openloch::frontdesigner::shapeLetters(openloch::frontdesigner::shpNumbers(text.readAll()));
                require(made&&sameLetters(*made,f.glyphs),"letters made like the original's own file");
            }
            const QPainterPath strokes=f.text(QString::fromUtf8("ABC xyz 0123 ÄÖÜ äöüß °±µ"),&width);require(width>0&&!strokes.isEmpty(),"local stroke font");
            if(!samples.isEmpty()){
                QImage picture(int(width*4)+20,int((f.above+f.below)*4)+20,QImage::Format_RGB32);picture.fill(Qt::white);QPainter p(&picture);p.setRenderHint(QPainter::Antialiasing);
                p.translate(10,10+f.above*4);p.scale(4,-4);QPen pen(Qt::black,0);pen.setCosmetic(true);pen.setWidthF(1.5);p.setPen(pen);p.drawPath(strokes);p.end();
                picture.save(QDir(samples).filePath(QFileInfo(file).fileName()+".png"));
            }
        }
    }
}
