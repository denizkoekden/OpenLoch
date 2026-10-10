// Export and library tests of the PCB module: GIF read back by a small decoder of the test, EMF played back by a small
// player of the test, both compared with the board's own picture; the macro library over a folder of the test.
#include "language.h"
#include "modules/pcb/editor.h"
#include "modules/pcb/font.h"
#include "modules/pcb/macropanel.h"
#include "modules/pcb/pictures.h"
#include "formats/sprint/sprint.h"
#include <QApplication>
#include <QDialog>
#include <QDropEvent>
#include <QEventLoop>
#include <QFileSystemModel>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QTreeView>
#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QTemporaryDir>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <stdexcept>

using namespace openloch;
using namespace openloch::pcb;
namespace {
void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
void require(bool b,const QString &message){if(!b)throw std::runtime_error(message.toStdString());}
quint16 u16(const QByteArray &d,qsizetype at){return qFromLittleEndian<quint16>(d.constData()+at);}
quint32 u32(const QByteArray &d,qsizetype at){return qFromLittleEndian<quint32>(d.constData()+at);}
qint32 i32(const QByteArray &d,qsizetype at){return qFromLittleEndian<qint32>(d.constData()+at);}
float f32(const QByteArray &d,qsizetype at){const quint32 v=u32(d,at);float f;std::memcpy(&f,&v,4);return f;}

// A GIF decoder for one image with a global colour table, as the GIF specification describes it: extensions are
// skipped, the image data is gathered from its blocks and expanded with LZW (codes from the lowest bit on, the width
// growing when the table fills the current width, a clear code starting over).
QImage readGif(const QByteArray &d){
    require(d.startsWith("GIF89a")||d.startsWith("GIF87a"),"a GIF signature");
    const int w=u16(d,6),h=u16(d,8);const quint8 packed=quint8(d[10]);require(packed&0x80,"a global colour table");
    const int size=2<<(packed&7);QList<QRgb> colours;qsizetype at=13;for(int k=0;k<size;k++,at+=3)colours<<qRgb(quint8(d[at]),quint8(d[at+1]),quint8(d[at+2]));
    while(quint8(d[at])==0x21){at+=2;while(quint8(d[at])){at+=1+quint8(d[at]);}at++;}
    require(quint8(d[at])==0x2c&&u16(d,at+5)==w&&u16(d,at+7)==h&&!(quint8(d[at+9])&0x80),"one image of the screen's size");at+=10;
    const int minimum=quint8(d[at++]);QByteArray data;while(quint8(d[at])){const int n=quint8(d[at]);data+=d.mid(at+1,n);at+=1+n;}at++;require(quint8(d[at])==0x3b,"the trailer");
    const int clear=1<<minimum,stop=clear+1;int width=minimum+1;QList<QByteArray> table;
    auto reset=[&]{table.clear();for(int k=0;k<clear+2;k++)table<<QByteArray(1,char(k));width=minimum+1;};reset();
    QByteArray pixels;qint64 bitPos=0;const qint64 bits=qint64(data.size())*8;QByteArray previous;
    auto read=[&]{int code=0;for(int k=0;k<width;k++,bitPos++)if(bitPos<bits&&(quint8(data[bitPos/8])>>(bitPos%8))&1)code|=1<<k;return code;};
    while(bitPos+width<=bits){
        const int code=read();
        if(code==clear){reset();previous.clear();continue;}
        if(code==stop)break;
        QByteArray entry;
        if(code<table.size())entry=table[code];
        else{require(code==table.size()&&!previous.isEmpty(),"an LZW code within the table");entry=previous+previous.left(1);}
        pixels+=entry;
        if(!previous.isEmpty()&&table.size()<4096)table<<previous+entry.left(1);
        if(table.size()==(1<<width)&&width<12)width++;
        previous=entry;
    }
    require(pixels.size()==qsizetype(w)*h,"as many pixels as the image has");
    QImage image(w,h,QImage::Format_RGB32);
    for(int y=0;y<h;y++){auto *row=reinterpret_cast<QRgb*>(image.scanLine(y));for(int x=0;x<w;x++)row[x]=colours.value(quint8(pixels[qsizetype(y)*w+x]))|0xff000000;}
    return image;
}

// An EMF player for the records the shared writer (src/emfwriter.h) writes: objects (pens, brushes, stock objects), fill
// mode, paths (move, lines, Béziers, close; filled, stroked or both), clipping (rectangles, paths) with saved states,
// bitmaps (also with alpha) under a world transformation; polygons and polylines as well. Device units are hundredths
// of a millimetre; `scale` maps them to pixels. Any other record fails.
QImage playEmf(const QByteArray &d,QSize size,double scale){
    require(u32(d,0)==1&&u32(d,40)==0x464D4520&&u32(d,48)==quint32(d.size()),"an EMF header with the file's size");
    QImage image(size,QImage::Format_RGB32);image.fill(Qt::white);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing,false);p.scale(scale,scale);
    QMap<quint32,QPen> pens;QMap<quint32,QBrush> brushes;QPen pen(Qt::black,0);QBrush brush(Qt::white);Qt::FillRule rule=Qt::OddEvenFill;QTransform world;
    QPainterPath path;auto filled=[&]{QPainterPath q=path;q.setFillRule(rule);return q;};
    auto bitmap=[&](qsizetype at,qsizetype infoAt,qsizetype bitsAt,QRectF target){
        const qsizetype info=at+u32(d,infoAt),bits=at+u32(d,bitsAt);const int w=i32(d,info+4),h=i32(d,info+8),depth=u16(d,info+14);
        require(depth==24||depth==32,"a 24- or 32-bit bitmap");QImage picture(w,h,depth==32?QImage::Format_ARGB32_Premultiplied:QImage::Format_RGB32);
        const int stride=depth==32?w*4:(w*3+3)/4*4;
        for(int y=0;y<h;y++)for(int c=0;c<w;c++){const qsizetype q=bits+qsizetype(h-1-y)*stride+(depth/8)*c;
            picture.setPixel(c,y,depth==32?qRgba(quint8(d[q+2]),quint8(d[q+1]),quint8(d[q]),quint8(d[q+3])):qRgb(quint8(d[q+2]),quint8(d[q+1]),quint8(d[q])));}
        p.save();p.setTransform(world*p.transform());p.drawImage(target,picture);p.restore();};
    auto colour=[](quint32 c){return QColor(int(c&0xff),int((c>>8)&0xff),int((c>>16)&0xff));};
    auto points=[&](qsizetype at,quint32 groups,QList<QPolygonF> &out){
        qsizetype pt=at+8+4*groups;for(quint32 k=0;k<groups;k++){QPolygonF poly;const quint32 n=u32(d,at+8+4*k);
            for(quint32 j=0;j<n;j++,pt+=8)poly<<QPointF(i32(d,pt),i32(d,pt+4));out<<poly;}};
    qsizetype at=u32(d,4);int records=1;
    while(at+8<=d.size()){
        const quint32 type=u32(d,at),length=u32(d,at+4);records++;const qsizetype x=at+8;
        switch(type){
        case 18:break;
        case 59:path=QPainterPath();break;
        case 60:break;
        case 27:path.moveTo(i32(d,x),i32(d,x+4));break;
        case 6:{const quint32 n=u32(d,x+16);for(quint32 k=0;k<n;k++)path.lineTo(i32(d,x+20+8*k),i32(d,x+24+8*k));break;}
        case 5:{const quint32 n=u32(d,x+16);
            for(quint32 k=0;k+2<n;k+=3)path.cubicTo(QPointF(i32(d,x+20+8*k),i32(d,x+24+8*k)),QPointF(i32(d,x+28+8*k),i32(d,x+32+8*k)),QPointF(i32(d,x+36+8*k),i32(d,x+40+8*k)));
            break;}
        case 61:path.closeSubpath();break;
        case 62:p.fillPath(filled(),brush);break;
        case 64:p.strokePath(path,pen);break;
        case 63:p.setPen(pen);p.setBrush(brush);p.drawPath(filled());break;
        case 33:p.save();break;
        case 34:p.restore();break;
        case 30:p.setClipRect(QRectF(QPointF(i32(d,x),i32(d,x+4)),QPointF(i32(d,x+8),i32(d,x+12))),Qt::IntersectClip);break;
        case 67:require(u32(d,x)==1,"a clip path cutting the clip");p.setClipPath(filled(),Qt::IntersectClip);break;
        case 114:bitmap(at,x+76,x+84,QRectF(i32(d,x+16),i32(d,x+20),i32(d,x+24),i32(d,x+28)));break;
        case 19:rule=u32(d,x)==2?Qt::WindingFill:Qt::OddEvenFill;break;
        case 37:{const quint32 h=u32(d,x);
            if(h==0x80000005)brush=Qt::NoBrush;else if(h==0x80000000)brush=QBrush(Qt::white);else if(h==0x80000008)pen=Qt::NoPen;else if(h==0x80000007)pen=QPen(Qt::black,0);
            else if(pens.contains(h))pen=pens[h];else if(brushes.contains(h))brush=brushes[h];else require(false,"a known object");break;}
        case 38:{QPen q(colour(u32(d,x+16)),i32(d,x+8));q.setCosmetic(i32(d,x+8)==0);pens[u32(d,x)]=q;break;}
        case 95:{const quint32 style=u32(d,x+20);QPen q(colour(u32(d,x+32)),double(u32(d,x+24)));require(style&0x10000,"a geometric pen");
            q.setCapStyle(style&0x200?Qt::FlatCap:style&0x100?Qt::SquareCap:Qt::RoundCap);q.setJoinStyle(style&0x1000?Qt::BevelJoin:style&0x2000?Qt::MiterJoin:Qt::RoundJoin);pens[u32(d,x)]=q;break;}
        case 39:brushes[u32(d,x)]=u32(d,x+4)==1?QBrush(Qt::NoBrush):QBrush(colour(u32(d,x+8)));break;
        case 40:pens.remove(u32(d,x));brushes.remove(u32(d,x));break;
        case 8:{QList<QPolygonF> polys;points(x+16,u32(d,x+16),polys);QPainterPath path;path.setFillRule(rule);for(const auto &poly:polys){path.addPolygon(poly);path.closeSubpath();}
            p.setPen(pen);p.setBrush(brush);p.drawPath(path);break;}
        case 7:{QList<QPolygonF> lines;points(x+16,u32(d,x+16),lines);p.setPen(pen);p.setBrush(Qt::NoBrush);for(const auto &line:lines)p.drawPolyline(line);break;}
        case 35:world=QTransform(f32(d,x),f32(d,x+4),f32(d,x+8),f32(d,x+12),f32(d,x+16),f32(d,x+20));break;
        case 36:world=QTransform();break;
        case 81:bitmap(at,x+40,x+48,QRectF(i32(d,x+16),i32(d,x+20),i32(d,x+64),i32(d,x+68)));break;
        case 14:require(at+length==quint32(d.size()),"EOF is the last record");break;
        default:require(false,QStringLiteral("an EMF record this module writes (%1)").arg(type));
        }
        at+=length;
    }
    require(quint32(records)==u32(d,52),"as many records as the header says");
    return image;
}
// Pixels of the played picture whose colour the reference has nowhere next to them: differences of more than a pixel.
int differences(const QImage &image,const QImage &ref){
    require(image.size()==ref.size(),"pictures of the same size");int bad=0;
    for(int y=1;y+1<ref.height();y++)for(int x=1;x+1<ref.width();x++){
        const QRgb v=image.pixel(x,y);if(v==ref.pixel(x,y))continue;
        bool near=false;for(int dy=-1;dy<=1&&!near;dy++)for(int dx=-1;dx<=1;dx++)if(ref.pixel(x+dx,y+dy)==v){near=true;break;}
        if(!near)bad++;
    }
    return bad;
}
void send(BoardView *view,QEvent::Type type,QPointF mm,Qt::MouseButton button=Qt::LeftButton){
    const QPointF at=view->toPixel(mm);const auto buttons=type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::MouseButtons(button);
    QMouseEvent event(type,at,view->mapToGlobal(at),type==QEvent::MouseMove?Qt::NoButton:button,type==QEvent::MouseMove?Qt::NoButton:buttons,Qt::NoModifier);
    QApplication::sendEvent(view,&event);
}
void click(BoardView *view,QPointF mm){send(view,QEvent::MouseMove,mm,Qt::NoButton);send(view,QEvent::MouseButtonPress,mm);send(view,QEvent::MouseButtonRelease,mm);}
bool samePoint(QPointF a,QPointF b){return QLineF(a,b).length()<1e-6;}
void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
// Fills in the next modal dialog with `fill` and accepts it.
void inDialog(const std::function<void(QDialog*)> &fill){
    auto *timer=new QTimer;timer->setInterval(5);
    QObject::connect(timer,&QTimer::timeout,[timer,fill]{
        if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){timer->stop();timer->deleteLater();fill(dialog);dialog->accept();}});
    timer->start();
}
// A board with every kind of element, a ground plane with clearances, a thermal pad, a keep-out area, hatched areas
// and an opening of the solder mask (drawn clipped).
Board testBoard(){
    Board b=newBoard("Bilder",60,40);b.groundPlane[CopperBottom]=true;
    auto pad=[&](QPointF at,PadShape shape,double size,double drill){auto e=newElement(ElementType::Pad);e.pos=at;e.shape=shape;e.size=size;e.size2=drill;updateOutline(e);b.elements<<e;};
    pad({8,8},PadShape::Round,2,.8);pad({14,8},PadShape::Square,2,.8);pad({20,8},PadShape::Octagon,2,.8);pad({26,8},PadShape::OvalWide,1.6,.8);
    {auto e=newElement(ElementType::Pad);e.pos={32,8};e.size=1.4;e.size2=.6;e.via=true;updateOutline(e);b.elements<<e;}
    {auto e=newElement(ElementType::Pad);e.pos={38,8};e.size=2;e.size2=.8;e.thermal=true;e.clearance=.5;updateOutline(e);b.elements<<e;}
    {auto e=newElement(ElementType::SmdPad);e.layer=CopperTop;e.pos={44,8};e.size=1.5;e.size2=2.5;e.rotation=30;updateOutline(e);b.elements<<e;}
    {auto e=newElement(ElementType::Track);e.points={{5,15},{20,15},{25,20}};e.width=.8;e.flatEnd=true;b.elements<<e;}
    {auto e=newElement(ElementType::Track);e.layer=CopperTop;e.points={{30,15},{45,15}};e.width=.5;b.elements<<e;}
    {auto e=newElement(ElementType::Area);e.points={{5,25},{15,25},{15,32},{5,32}};e.width=.3;b.elements<<e;}
    {auto e=newElement(ElementType::Area);e.points={{18,25},{28,25},{23,34}};e.width=.4;e.hatched=true;b.elements<<e;}
    {auto e=newElement(ElementType::Area);e.layer=CopperTop;e.points={{31,25},{38,25},{38,31},{31,31}};e.maskOnly=true;e.width=.2;b.elements<<e;}
    {auto e=newElement(ElementType::Area);e.points={{50,25},{55,25},{55,30}};e.cutout=true;b.elements<<e;}
    {auto e=newElement(ElementType::Circle);e.layer=SilkTop;e.pos={45,30};e.size=3;e.width=.3;e.start=30;e.stop=300;b.elements<<e;}
    {auto e=newElement(ElementType::Circle);e.pos={50,15};e.size=1.5;e.width=.4;e.filled=true;b.elements<<e;}
    {auto e=newElement(ElementType::Text);e.layer=SilkTop;e.text="OpenLoch äi";e.pos={5,38};e.size=2.5;updateStrokes(e);b.elements<<e;}
    {auto e=newElement(ElementType::Track);e.layer=Outline;e.points={{0,0},{60,0},{60,40}};e.width=0;b.elements<<e;}
    b.elements[0].connections={1};b.elements[1].connections={0};
    return b;
}
}

int picturesTests(){
    Editor editor;Document d;d.boards={testBoard()};editor.setDocument(d);QTemporaryDir dir;QString error;auto *view=editor.view();
    // --- GIF: the board without smoothed edges, every pixel back as it was, at the resolution asked for.
    {const QString file=dir.filePath("platine.gif");require(editor.exportImage(file,254,true,&error),"GIF written");
        QFile f(file);require(f.open(QIODevice::ReadOnly),"GIF readable");const QByteArray bytes=f.readAll();
        const QImage read=readGif(bytes),ref=view->renderBoard(10,false);
        require(read.size()==QSize(600,400)&&read==ref.convertToFormat(QImage::Format_RGB32),"the GIF holds the board pixel for pixel");
        if(QImageReader::supportedImageFormats().contains("gif"))require(QImage(file).convertToFormat(QImage::Format_RGB32)==read,"Qt reads the GIF alike");
        require(editor.exportImage(dir.filePath("schwarzweiss.gif"),254,false,&error),"black and white GIF written");
        QFile g(dir.filePath("schwarzweiss.gif"));require(g.open(QIODevice::ReadOnly),"readable");const QImage mono=readGif(g.readAll());
        int other=0;for(int y=0;y<mono.height();y++)for(int x=0;x<mono.width();x++){const QRgb c=mono.pixel(x,y);other+=c!=qRgb(0,0,0)&&c!=qRgb(255,255,255);}
        require(mono.size()==QSize(600,400)&&other==0,"black on white");
        // Many colours: the codes grow to 12 bits and the table starts over; a picture of 4000 colours still decodes.
        QImage many(200,200,QImage::Format_RGB32);for(int y=0;y<200;y++)for(int x=0;x<200;x++)many.setPixel(x,y,qRgb((x*7)%256,(y*13)%256,((x+y)*5)%256));
        const QImage back=readGif(gifData(many));
        require(back==many.convertToFormat(QImage::Format_Indexed8).convertToFormat(QImage::Format_RGB32),"a picture of many colours comes back with Qt's palette");
        QImage few(300,100,QImage::Format_RGB32);for(int y=0;y<100;y++)for(int x=0;x<300;x++)few.setPixel(x,y,qRgb((x/3)%2*255,y%4*60,((x*y)%7)*30));
        require(readGif(gifData(few))==few,"a picture of 56 colours comes back exactly");
        require(gifData(QImage()).isEmpty()&&gifData(QImage(70000,1,QImage::Format_RGB32)).isEmpty(),"no GIF of nothing or of more than 65535 pixels a side");}
    // --- EMF: the same board as vectors; played back it is the board's own picture but for edges.
    {const QString file=dir.filePath("platine.emf");require(editor.exportEmf(file,&error),"EMF written");
        QFile f(file);require(f.open(QIODevice::ReadOnly),"EMF readable");const QByteArray emf=f.readAll();
        require(i32(emf,24)==0&&i32(emf,28)==0&&i32(emf,32)==6000&&i32(emf,36)==4000,"the frame is the working area in hundredths of a millimetre");
        require(i32(emf,72)==100*i32(emf,80)&&i32(emf,76)==100*i32(emf,84),"a reference device of 100 units per millimetre");
        const QImage played=playEmf(emf,QSize(600,400),.1),ref=view->renderBoard(10,false);
        const int bad=differences(played,ref);
        if(bad>5){played.save("emf-played.png");ref.save("emf-reference.png");}
        require(bad<=5,QStringLiteral("the EMF played back is the board (%1 pixels differ)").arg(bad));
        // Tracks are lines of their width with round ends, not filled outlines.
        bool line=false;for(qsizetype at=u32(emf,4);at+8<=emf.size();at+=u32(emf,at+4))if(u32(emf,at)==95&&u32(emf,at+32)==80&&!(u32(emf,at+28)&0x300))line=true;
        require(line,"a geometric pen 0.8 mm wide with round ends for the track");}
    return 0;
}

int macroTests(){
    // A folder of macros of the test's own: two pads with a silkscreen line, no designator; a file that is no macro.
    QTemporaryDir dir;QDir(dir.path()).mkpath("Steckverbinder");const QString file=dir.filePath("Steckverbinder/zwei.lmk");
    {auto pad=newElement(ElementType::Pad);pad.size=1.6;pad.size2=.8;updateOutline(pad);auto pad2=pad;pad2.pos={2.54,0};updateOutline(pad2);
        auto line=newElement(ElementType::Track);line.layer=SilkTop;line.points={{-1,-1.5},{3.54,-1.5}};line.width=.2;
        QFile f(file);require(f.open(QIODevice::WriteOnly)&&f.write(sprint::writeMacro({pad,pad2,line}))>0,"macro written");f.close();
        QFile note(dir.filePath("Steckverbinder/notiz.txt"));require(note.open(QIODevice::WriteOnly),"note written");note.write("x");}
    Editor editor;editor.resize(1200,800);editor.show();QApplication::processEvents();auto *view=editor.view();auto *panel=editor.macroPanel();
    Document d;Board b=newBoard("Makros",60,40);b.grid=1.27;b.origin={0,0};d.boards={b};editor.setDocument(d);QApplication::processEvents();view->fitBoard();
    auto els=[&]()->const QList<Element>&{return editor.document().board().elements;};
    require(panel->findChild<QLabel*>("macroFolderName")->text()==ui("Noch kein Makroordner gewählt."),"no macro folder at first: OpenLoch ships none");
    // The tree: the folder with its subfolders and macros only (the file system model reads in the background).
    panel->setFolder(dir.path());auto *tree=panel->findChild<QTreeView*>("macroTree");auto *model=qobject_cast<QFileSystemModel*>(tree->model());
    const QModelIndex sub=model->index(dir.filePath("Steckverbinder"));
    for(int k=0;k<300&&model->rowCount(sub)<1;k++){if(model->canFetchMore(sub))model->fetchMore(sub);wait(10);}
    wait(50);QStringList shown;for(int r=0;r<model->rowCount(sub);r++){const QModelIndex i=model->index(r,0,sub);if(!(model->flags(i)&Qt::ItemIsEnabled))continue;shown<<model->fileName(i);}
    require(shown==QStringList{"zwei.lmk"},"the macro, not the note");
    tree->setCurrentIndex(model->index(file));require(panel->picked()==file&&panel->macro().size()==3&&panel->currentFolder()==dir.filePath("Steckverbinder"),"picking a macro in the tree");
    // Placing: on the pointer, a click puts it down with the pads where the macro has them.
    panel->findChild<QPushButton*>("macroPlace")->click();require(view->placing(),"the macro on the pointer");click(view,{20.32,20.32});
    require(els().size()==3&&samePoint(els()[1].pos-els()[0].pos,{2.54,0})&&!els()[0].groups.isEmpty()&&els()[0].groups==els()[2].groups&&components(editor.document().board()).isEmpty(),
            "placed as a group, no component without a designator");
    // From below, through-plated, a quarter turn clockwise.
    panel->setOptions(true,true,1,false);const auto turned=panel->macro();
    require(turned[0].layer==CopperTop&&turned[2].layer==SilkBottom&&turned[0].via&&turned[1].via&&samePoint(turned[1].pos-turned[0].pos,{0,-2.54}),
            "the other side, vias and a quarter turn clockwise as seen: the second pad, left of the first from below, comes above it");
    // As a component: a designator and a value come with it, the component dialog follows.
    panel->setOptions(false,false,0,true);
    inDialog([](QDialog *x){const auto edits=x->findChildren<QLineEdit*>();if(edits.size()>=2){edits[0]->setText("J5");edits[1]->setText("Stecker");}});
    panel->findChild<QPushButton*>("macroPlace")->click();click(view,{40.64,20.32});QApplication::processEvents();
    const auto parts=components(editor.document().board());
    require(parts.size()==1&&els()[parts[0].designator].text=="J5"&&parts[0].value>=0&&els()[parts[0].value].text=="Stecker"&&parts[0].members.size()==5,"a component with the dialog's designator and value");
    // Dragged from the preview: the macro follows the drag and goes down where it is dropped, as one undo step.
    panel->setOptions(false,false,0,false);const int before=int(els().size());QMimeData data;data.setData(MacroPanel::dragFormat(),"macro");
    {const QPointF at=view->toPixel({20.32,30.48});QDragEnterEvent enter(at.toPoint(),Qt::CopyAction,&data,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view,&enter);
        require(enter.isAccepted()&&view->placing(),"a dragged macro is taken");
        QDropEvent drop(at,Qt::CopyAction,&data,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view,&drop);}
    require(els().size()==before+3&&!view->placing()&&QLineF(els()[before].pos,{20.32,30.48}).length()<5,"dropped where the drag ends");
    editor.undo();require(els().size()==before,"one undo step");
    {QDragEnterEvent enter(view->toPixel({20,20}).toPoint(),Qt::CopyAction,&data,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view,&enter);
        QDragLeaveEvent leave;QApplication::sendEvent(view,&leave);require(!view->placing()&&els().size()==before,"a drag that leaves the board puts nothing down");}
    // Dragging over the board leaves the tool, what is being drawn and what waits at the pointer as they are.
    view->setTool(BoardView::Tool::Track);click(view,{10.16,35.56});
    {QDragEnterEvent enter(view->toPixel({20,20}).toPoint(),Qt::CopyAction,&data,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view,&enter);
        QDragLeaveEvent leave;QApplication::sendEvent(view,&leave);}
    require(view->tool()==BoardView::Tool::Track&&els().size()==before,"the track being drawn goes on");
    click(view,{20.32,35.56});send(view,QEvent::MouseButtonPress,{20.32,35.56},Qt::RightButton);send(view,QEvent::MouseButtonRelease,{20.32,35.56},Qt::RightButton);
    require(els().size()==before+1&&els().last().type==ElementType::Track&&samePoint(els().last().points.first(),{10.16,35.56}),"and ends where it was going");
    editor.undo();view->setTool(BoardView::Tool::Select);view->setSelection({0});editor.copySelection();editor.pasteClipboard();
    {QDragEnterEvent enter(view->toPixel({20,20}).toPoint(),Qt::CopyAction,&data,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view,&enter);
        QDragLeaveEvent leave;QApplication::sendEvent(view,&leave);}
    require(view->placing(),"what was pasted still waits at the pointer");
    send(view,QEvent::MouseButtonPress,{20,20},Qt::RightButton);send(view,QEvent::MouseButtonRelease,{20,20},Qt::RightButton);require(!view->placing(),"pasting called off");
    // Dropped as a component: the component dialog comes once the drop has ended.
    panel->setOptions(false,false,0,true);
    inDialog([](QDialog *x){const auto edits=x->findChildren<QLineEdit*>();if(edits.size()>=2){edits[0]->setText("J6");edits[1]->setText("Buchse");}});
    {const QPointF at=view->toPixel({40.64,30.48});QDragEnterEvent enter(at.toPoint(),Qt::CopyAction,&data,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view,&enter);
        QDropEvent drop(at,Qt::CopyAction,&data,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view,&drop);
        require(!QApplication::activeModalWidget(),"no dialog inside the drop");}
    QApplication::processEvents();QApplication::processEvents();
    {bool found=false;for(const auto &c:components(editor.document().board()))found=found||els()[c.designator].text=="J6";require(found,"the dialog follows the drop");}
    editor.undo();editor.undo();require(els().size()==before,"dropped and named: two undo steps");panel->setOptions(false,false,0,false);
    QMimeData other;other.setText("x");{QDragEnterEvent enter(view->toPixel({20,20}).toPoint(),Qt::CopyAction,&other,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view,&enter);
        require(!enter.isAccepted()&&!view->placing(),"other things are not taken");}
    // The selection saved as a macro into the folder of the macro picked, read back, removed again.
    view->setSelection({0,1,2});const QString saved=dir.filePath("Steckverbinder/neu.lmk");
    require(editor.saveMacroAs(panel->currentFolder(),saved)&&sprint::readMacro([&]{QFile f(saved);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}()).size()==3,"the selection as a macro");
    // Two lines describing it, kept in the file.
    {require(panel->pick(saved)&&panel->description()==QStringList({"",""}),"a new macro has no description");
        auto *first=panel->findChild<QLineEdit*>("macroText1");auto *second=panel->findChild<QLineEdit*>("macroText2");
        require(first&&second&&first->isEnabled()&&first->maxLength()==50,"the description fields");
        first->setText("Stiftleiste");second->setText("RM 2,54");Q_EMIT second->editingFinished();
        require(panel->pick(saved)&&panel->description()==QStringList({"Stiftleiste","RM 2,54"})&&first->text()=="Stiftleiste","the description written into the macro");}
    require(panel->pick(saved)&&panel->removeMacro(saved)&&!QFile::exists(saved)&&panel->picked().isEmpty(),"a macro removed");
    QFile broken(dir.filePath("kaputt.lmk"));require(broken.open(QIODevice::WriteOnly),"broken file");broken.write("nichts");broken.close();
    require(!panel->pick(dir.filePath("kaputt.lmk"))&&!panel->problem().isEmpty()&&panel->macro().isEmpty(),"a broken macro says why");
    require(panel->removeMacro(dir.filePath("kaputt.lmk"))&&panel->problem().isEmpty(),"removed, its problem goes with it");
    // Through-plating is for pads with copper: a plain hole stays unplated.
    {auto pad=newElement(ElementType::Pad);pad.size=1.6;pad.size2=.8;updateOutline(pad);auto hole=pad;hole.pos={5,0};hole.size=3.2;hole.size2=3.2;updateOutline(hole);
        const QString holes=dir.filePath("loch.lmk");QFile f(holes);require(f.open(QIODevice::WriteOnly)&&f.write(sprint::writeMacro({pad,hole}))>0,"macro with a hole");f.close();
        panel->setOptions(false,true,0,false);require(panel->pick(holes)&&panel->macro().size()==2&&panel->macro()[0].via&&!panel->macro()[1].via,"a plain hole is not plated through");
        panel->setOptions(false,false,0,false);}
    return 0;
}
