#include "printing.h"
#include "../../printing.h"
#include "copper.h"
#include "draw.h"
#include "language.h"
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPageSetupDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>

namespace openloch::pcb {
namespace {
constexpr double markMargin=8,imageGap=5;
QColor defaultColour(int layer){
    switch(layer){
    case CopperTop:return QColor(29,106,249);case SilkTop:return QColor(255,0,0);case CopperBottom:return QColor(0,186,1);
    case SilkBottom:return QColor(225,215,4);case Inner1:return QColor(194,124,21);case Inner2:return QColor(238,182,98);default:return QColor(0,0,0);
    }
}
double margin(const PrintSettings &s){return s.marks?markMargin:0;}
// A shape grown by `distance` on every side, or shrunk by it when negative (in hundredths of a millimetre for the
// clipper, as the copper does).
QPainterPath offsetShape(const QPainterPath &shape,double distance){
    if(distance>=0)return grownShape(shape,distance);
    const QTransform up=QTransform::fromScale(100,100),down=QTransform::fromScale(.01,.01);const QPainterPath fine=up.map(shape);
    QPainterPathStroker s;s.setWidth(-2*distance*100);s.setJoinStyle(Qt::RoundJoin);s.setCapStyle(Qt::RoundCap);
    return down.map(fine.subtracted(s.createStroke(fine)));
}
// Text in millimetres of height at `at`, readable also in a mirrored image.
void label(QPainter &p,QPointF at,double height,const QString &text,bool mirrored){
    p.save();p.translate(at);if(mirrored)p.scale(-1,1);p.scale(height/10,height/10);
    QFont font=p.font();font.setPixelSize(10);p.setFont(font);p.drawText(QPointF(0,0),text);p.restore();
}
// One image: the working area with its template and layers in order, then holes, mask openings, drill plan, frame and
// marks. `side` is the side whose template lies under it (0 top, 1 bottom); `only` the side the image shows alone (0 top,
// 1 bottom), -1 for both: its solder mask openings are those of that side.
void paintImage(QPainter &p,const Board &b,const PrintSettings &s,const QList<int> &layers,int side,int only){
    const QRectF area(0,0,b.width,b.height);
    p.save();if(s.mirrored){p.translate(b.width,0);p.scale(-1,1);}
    const QColor paper=s.negative?QColor(Qt::black):QColor(Qt::white);
    auto colour=[&](const QColor &c){return s.negative?QColor(Qt::white):s.blackWhite?QColor(Qt::black):c;};
    if(s.negative)p.fillRect(area,Qt::black);
    if(s.withTemplate){
        const auto &t=b.templates[side];const QImage &picture=s.templatePictures[side];
        if(!t.file.isEmpty()&&!picture.isNull()){const double perPixel=25.4/std::max(1.0,t.dpi);p.drawImage(QRectF(t.offset,QSizeF(picture.width()*perPixel,picture.height()*perPixel)),picture);}
    }
    QList<int> copper;for(int l:layers)if(isCopper(l))copper.append(l);
    for(int layer:layers){
        const QColor c=colour(s.colours[layer]);
        if(isCopper(layer)&&b.groundPlane[layer]&&copperLayers(b).contains(layer)){p.setPen(Qt::NoPen);p.setBrush(c);p.drawPath(groundPlane(b,layer));}
        for(const auto &e:b.elements){
            // Copper layers take everything with copper on them (through-plated pads on all), the others their own elements.
            if(isCopper(layer)?copperOn(e,layer,b).isEmpty():(e.layer!=layer||e.cutout||(e.type==ElementType::Text&&!e.visible&&e.role!=TextRole::Plain)))continue;
            paintElement(p,e,c);
        }
    }
    // Holes stay open in every pad, so the drill finds the centre.
    if(!copper.isEmpty()){p.setPen(Qt::NoPen);p.setBrush(paper);for(const auto &e:b.elements)if(e.type==ElementType::Pad&&e.size2>0)p.drawEllipse(e.pos,e.size2/2,e.size2/2);}
    // The openings of the solder mask of each side chosen: the bottom one first. Each opening is larger than the copper by
    // the offset of its kind; areas that are only an opening keep their outline.
    for(const int mask:{CopperBottom,CopperTop}){
        if(!(mask==CopperTop?s.maskTop:s.maskBottom)||(only>=0&&only!=(mask==CopperTop?0:1)))continue;
        p.setPen(Qt::NoPen);p.setBrush(colour(mask==CopperTop?s.maskTopColour:s.maskBottomColour));
        for(const auto &e:b.elements){
            if(e.type==ElementType::Area&&e.maskOnly){if(e.layer==mask)p.drawPath(copperShape(e));continue;}
            if(!e.solderMask||copperOn(e,mask,b).isEmpty())continue;
            const bool pad=e.type==ElementType::Pad,smd=e.type==ElementType::SmdPad;
            if(pad?!s.maskPads:smd?!s.maskSmd:!s.maskOther)continue;
            p.drawPath(offsetShape(copperShape(e),pad?s.padMask:smd?s.smdMask:s.otherMask));
        }
    }
    if(s.drillPlan){
        QPen pen(colour(s.drillColour),.1);p.setPen(pen);p.setBrush(Qt::NoBrush);
        for(const auto &e:b.elements)if(e.type==ElementType::Pad&&e.size2>0){
            const double r=e.size2/2;p.drawEllipse(e.pos,r,r);p.drawLine(e.pos-QPointF(r,0),e.pos+QPointF(r,0));p.drawLine(e.pos-QPointF(0,r),e.pos+QPointF(0,r));
            const QPointF at=e.pos+QPointF((s.mirrored?-1:1)*(e.size/2+.3),s.drillTextHeight/2);label(p,at,s.drillTextHeight,uiLocale().toString(e.size2,'f',2),s.mirrored);
        }
    }
    const QColor line=colour(QColor(Qt::black));
    if(s.frame){QPen pen(s.negative?QColor(Qt::black):line,.2);p.setPen(pen);p.setBrush(Qt::NoBrush);p.drawRect(area);}
    if(s.marks){
        // A cross with a circle off each corner of the working area.
        QPen pen(s.negative?QColor(Qt::black):line,.15);p.setPen(pen);p.setBrush(Qt::NoBrush);
        for(QPointF corner:{area.topLeft(),area.topRight(),area.bottomLeft(),area.bottomRight()}){
            const QPointF c=corner+QPointF(corner.x()>0?4:-4,corner.y()>0?4:-4);
            p.drawLine(c-QPointF(3,0),c+QPointF(3,0));p.drawLine(c-QPointF(0,3),c+QPointF(0,3));p.drawEllipse(c,1.5,1.5);
        }
    }
    p.restore();
}
}

PrintSettings defaultPrintSettings(const Board &b){
    PrintSettings s;
    for(int l=1;l<=layerCount;l++){
        s.colours[l]=defaultColour(l);bool any=false;for(const auto &e:b.elements)any|=e.layer==l||(isCopper(l)&&!copperOn(e,l,b).isEmpty());
        s.layers[l]=b.visible[l]&&(any||(isCopper(l)&&b.groundPlane[l]));
    }
    return s;
}
QList<QList<int>> printImages(const PrintSettings &s){
    auto pick=[&](const QList<int> &order){QList<int> out;for(int l:order)if(s.layers[l])out.append(l);return out;};
    QList<QList<int>> images;
    switch(s.order){
    case 1:images={pick({CopperTop,Inner1,Inner2,CopperBottom,SilkTop,SilkBottom,Outline})};break;
    case 2:case 3:images={pick({Inner1,CopperTop,SilkTop,Outline}),pick({Inner2,CopperBottom,SilkBottom,Outline})};break;
    default:images={pick({CopperBottom,Inner2,Inner1,CopperTop,SilkBottom,SilkTop,Outline})};break;
    }
    // With special layers chosen every image stays, also without layers of its own: the openings and the plan print alone.
    if(!(s.maskTop||s.maskBottom||s.drillPlan))images.removeAll(QList<int>{});
    return images;
}
namespace {
// One copy of the printout: its images side by side or one below the other.
QSizeF copySize(const Board &b,const PrintSettings &s){
    const int n=std::max<qsizetype>(1,printImages(s).size());const double m=margin(s);
    const QSizeF image((b.width+2*m)*s.scale,(b.height+2*m)*s.scale);
    if(n==2&&s.order==2)return {image.width(),2*image.height()+imageGap};
    if(n==2&&s.order==3)return {2*image.width()+imageGap,image.height()};
    return image;
}
int tiles(int n){return std::clamp(n,1,20);}
}
QSizeF printSize(const Board &b,const PrintSettings &s){
    const QSizeF one=copySize(b,s);const int nx=tiles(s.tilesX),ny=tiles(s.tilesY);const double gap=std::max(0.0,s.tileGap);
    return {nx*one.width()+(nx-1)*gap,ny*one.height()+(ny-1)*gap};
}
void paintPrintout(QPainter &p,const Board &b,const PrintSettings &s){
    const auto images=printImages(s);const double m=margin(s);const QSizeF image((b.width+2*m)*s.scale,(b.height+2*m)*s.scale);
    const QSizeF one=copySize(b,s);const double gap=std::max(0.0,s.tileGap);
    for(int ty=0;ty<tiles(s.tilesY);ty++)for(int tx=0;tx<tiles(s.tilesX);tx++)for(int k=0;k<images.size();k++){
        p.save();p.translate(tx*(one.width()+gap),ty*(one.height()+gap));
        if(s.order==2)p.translate(0,k*(image.height()+imageGap));else if(s.order==3)p.translate(k*(image.width()+imageGap),0);
        // The side of the image: that of its layers, or the side on top when it holds both.
        bool top=false,bottom=false;for(int l:images[k]){top|=l==CopperTop||l==SilkTop||l==Inner1;bottom|=l==CopperBottom||l==SilkBottom||l==Inner2;}
        const int side=top&&!bottom?0:bottom&&!top?1:s.order==1?1:0,only=s.order>=2&&images.size()==2?int(k):-1;
        p.scale(s.scale,s.scale);p.translate(m,m);paintImage(p,b,s,images[k],only>=0?only:side,only);p.restore();
    }
}
QImage printPicture(const Board &b,const PrintSettings &s,double dpi){
    const double perMm=dpi/25.4;const QSizeF size=printSize(b,s);
    QImage image(std::max(1,int(std::ceil(size.width()*perMm))),std::max(1,int(std::ceil(size.height()*perMm))),QImage::Format_RGB32);image.fill(Qt::white);
    image.setDotsPerMeterX(int(std::lround(perMm*1000)));image.setDotsPerMeterY(int(std::lround(perMm*1000)));
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.scale(perMm,perMm);paintPrintout(p,b,s);return image;
}
QString printInfo(const QString &file,const Board &b,const PrintSettings &s){
    const QString scale=s.scale==1?QStringLiteral("1:1"):uiLocale().toString(s.scale*100,'f',0)+QStringLiteral(" %");
    return QStringList{QFileInfo(file).fileName(),b.name,scale,uiLocale().toString(QDateTime::currentDateTime(),QLocale::ShortFormat)}.join(QStringLiteral("  –  "));
}
void print(QPrinter &printer,const Board &b,const PrintSettings &s,const QString &info,QPointF correction){
    printer.setPageOrientation(s.orientation);
    QPainter p(&printer);if(!p.isActive())return;
    // The painter starts at the top left of the printable area; one unit becomes a millimetre, stretched by the correction.
    const double perMm=printer.resolution()/25.4;p.scale(perMm*correction.x(),perMm*correction.y());p.setRenderHint(QPainter::Antialiasing);
    p.save();p.translate(s.position);paintPrintout(p,b,s);p.restore();
    // The info line at the foot of the printable area, in plain millimetres: the correction is for the board only.
    if(s.infoLine){const QRectF area=printer.pageLayout().paintRect(QPageLayout::Millimeter);p.resetTransform();p.scale(perMm,perMm);p.setPen(Qt::black);label(p,QPointF(0,area.height()-1),2.5,info,false);}
}

// --- the preview
class PrintSheet : public QWidget {
public:
    const Board *board=nullptr;PrintSettings *settings=nullptr;QSizeF paper;QRectF printable;QString info;
    PrintSheet(){setMinimumSize(420,420);setCursor(Qt::OpenHandCursor);}
protected:
    double scale() const{return std::min((width()-40)/std::max(1.0,paper.width()),(height()-40)/std::max(1.0,paper.height()));}
    QPointF origin() const{return QPointF((width()-paper.width()*scale())/2,(height()-paper.height()*scale())/2);}
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);p.fillRect(rect(),QColor(128,128,128));if(!board||!settings)return;
        p.translate(origin());p.scale(scale(),scale());p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(QRectF(QPointF(1.5,1.5),paper),QColor(80,80,80));p.fillRect(QRectF(QPointF(),paper),Qt::white);
        if(settings->helpGrid){QPen grid(QColor(205,205,205),0);p.setPen(grid);
            for(double x=10;x<paper.width();x+=10)p.drawLine(QPointF(x,0),QPointF(x,paper.height()));for(double y=10;y<paper.height();y+=10)p.drawLine(QPointF(0,y),QPointF(paper.width(),y));}
        p.save();p.setClipRect(printable);p.translate(printable.topLeft()+settings->position);paintPrintout(p,*board,*settings);p.restore();
        if(settings->infoLine){p.setPen(Qt::black);label(p,QPointF(printable.left(),printable.bottom()-1),2.5,info,false);}
        QPen frame(QColor(255,0,0),0);p.setPen(frame);p.setBrush(Qt::NoBrush);p.drawRect(printable);
    }
    void mousePressEvent(QMouseEvent *event) override{last=event->position();setCursor(Qt::ClosedHandCursor);}
    void mouseMoveEvent(QMouseEvent *event) override{
        if(!(event->buttons()&Qt::LeftButton)||!settings)return;settings->position+=(event->position()-last)/scale();last=event->position();update();
    }
    void mouseReleaseEvent(QMouseEvent *) override{setCursor(Qt::OpenHandCursor);}
private:
    QPointF last;
};

QPointF PrintPreview::correction{1,1};
PrintPreview::PrintPreview(const Board &b,const PrintSettings &s,const QString &path,QPrinter *printer,QWidget *parent):QDialog(parent),board(b),current(s),file(path){
    // A printer set up before (Datei → Drucker einrichten) brings its orientation along.
    setWindowTitle(ui("Drucken"));if(!printer){own=std::make_unique<QPrinter>(QPrinter::HighResolution);printer=own.get();}else current.orientation=printer->pageLayout().orientation();
    device=printer;
    auto *layout=new QHBoxLayout(this);auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
    auto *panel=new QWidget;auto *left=new QVBoxLayout(panel);scroll->setWidget(panel);scroll->setMinimumWidth(340);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);layout->addWidget(scroll);
    sheet=new PrintSheet;sheet->board=&board;sheet->settings=&current;layout->addWidget(sheet,1);
    auto colourButton=[this](QColor *target){
        auto *button=new QToolButton;auto show=[button,target]{QPixmap swatch(16,16);swatch.fill(*target);button->setIcon(QIcon(swatch));};show();button->setToolTip(ui("Farbe wählen"));
        connect(button,&QToolButton::clicked,this,[this,target,show]{const QColor c=QColorDialog::getColor(*target,this);if(c.isValid()){*target=c;show();changed();}});
        return button;
    };
    auto check=[this](QBoxLayout *into,const QString &text,bool *target){
        auto *box=new QCheckBox(text);box->setChecked(*target);into->addWidget(box);
        connect(box,&QCheckBox::toggled,this,[this,target](bool on){*target=on;changed();});return box;
    };
    // Layers: only those with something to print can be chosen.
    auto *layers=new QGroupBox(ui("Layer"));auto *lv=new QVBoxLayout(layers);left->addWidget(layers);
    for(int layer:layerOrder()){
        bool any=false;for(const auto &e:board.elements)any|=e.layer==layer||(isCopper(layer)&&!copperOn(e,layer,board).isEmpty());any|=isCopper(layer)&&board.groundPlane[layer];
        if(!any)current.layers[layer]=false;
        auto *row=new QHBoxLayout;auto *box=check(row,layerName(layer),&current.layers[layer]);box->setEnabled(any);row->addStretch();row->addWidget(colourButton(&current.colours[layer]));lv->addLayout(row);
    }
    auto *order=new QGroupBox(ui("Druckreihenfolge"));auto *ov=new QVBoxLayout(order);left->addWidget(order);auto *orders=new QButtonGroup(this);
    const QStringList orderNames{ui("Oberseite über der Unterseite"),ui("Unterseite über der Oberseite"),ui("Unterseite unter der Oberseite"),ui("Unterseite rechts neben der Oberseite")};
    for(int k=0;k<orderNames.size();k++){auto *r=new QRadioButton(orderNames[k]);r->setChecked(current.order==k);orders->addButton(r,k);ov->addWidget(r);}
    connect(orders,&QButtonGroup::idClicked,this,[this](int k){current.order=k;changed();});
    // Special layers: the solder mask of each side, the drill plan, and their settings.
    auto *special=new QGroupBox(ui("Sonderlayer"));auto *sv=new QVBoxLayout(special);left->addWidget(special);
    {auto *row=new QHBoxLayout;check(row,ui("Lötstopp oben"),&current.maskTop)->setObjectName("maskTop");row->addStretch();row->addWidget(colourButton(&current.maskTopColour));sv->addLayout(row);}
    {auto *row=new QHBoxLayout;check(row,ui("Lötstopp unten"),&current.maskBottom)->setObjectName("maskBottom");row->addStretch();row->addWidget(colourButton(&current.maskBottomColour));sv->addLayout(row);}
    {auto *row=new QHBoxLayout;check(row,ui("Bohrplan"),&current.drillPlan)->setObjectName("drillPlan");row->addStretch();row->addWidget(colourButton(&current.drillColour));sv->addLayout(row);}
    {auto *more=new QPushButton(ui("Einstellungen…"));more->setObjectName("specialSettings");sv->addWidget(more);
        connect(more,&QPushButton::clicked,this,[this]{specialSettings();});}
    auto *options=new QGroupBox(ui("Optionen"));auto *opv=new QVBoxLayout(options);left->addWidget(options);
    check(opv,ui("Schwarz/Weiß"),&current.blackWhite);check(opv,ui("Spiegeln"),&current.mirrored);check(opv,ui("Passkreuze"),&current.marks);check(opv,ui("Rahmen"),&current.frame);
    check(opv,ui("Negativ"),&current.negative);check(opv,ui("Hilfsgitter"),&current.helpGrid);check(opv,ui("Infozeile"),&current.infoLine);
    {auto *box=check(opv,ui("Vorlage mitdrucken"),&current.withTemplate);bool any=false;
        for(int k=0;k<2;k++)any|=!board.templates[k].file.isEmpty()&&!current.templatePictures[k].isNull();
        box->setEnabled(any);if(!any)box->setChecked(false);}
    auto *scaling=new QGroupBox(ui("Skalierung"));auto *scv=new QHBoxLayout(scaling);left->addWidget(scaling);
    auto *exact=new QRadioButton(QStringLiteral("1:1"));auto *free=new QRadioButton(ui("frei"));auto *percent=new QDoubleSpinBox;
    percent->setRange(10,500);percent->setDecimals(0);percent->setSuffix(QStringLiteral(" %"));percent->setValue(current.scale*100);percent->setLocale(uiLocale());
    exact->setChecked(current.scale==1);free->setChecked(current.scale!=1);percent->setEnabled(current.scale!=1);scv->addWidget(exact);scv->addWidget(free);scv->addWidget(percent);
    connect(exact,&QRadioButton::toggled,this,[this,percent](bool on){percent->setEnabled(!on);current.scale=on?1:percent->value()/100;changed();});
    connect(percent,&QDoubleSpinBox::valueChanged,this,[this,exact](double v){if(!exact->isChecked()){current.scale=v/100;changed();}});
    auto *orientation=new QGroupBox(ui("Ausrichtung"));auto *orv=new QHBoxLayout(orientation);left->addWidget(orientation);
    auto *portrait=new QRadioButton(ui("Hochformat"));auto *landscape=new QRadioButton(ui("Querformat"));portrait->setChecked(current.orientation==QPageLayout::Portrait);
    landscape->setChecked(current.orientation==QPageLayout::Landscape);orv->addWidget(portrait);orv->addWidget(landscape);
    connect(landscape,&QRadioButton::toggled,this,[this](bool on){current.orientation=on?QPageLayout::Landscape:QPageLayout::Portrait;changed();});
    left->addStretch();
    // Above the sheet as in the reference: centre, clipboard, tiles, correction, page setup, print, close.
    auto *right=new QVBoxLayout;auto *buttons=new QHBoxLayout;right->addLayout(buttons);layout->removeWidget(sheet);right->addWidget(sheet,1);layout->addLayout(right,1);
    auto button=[&](const QString &text,const char *name){auto *b=new QPushButton(text);b->setObjectName(name);buttons->addWidget(b);return b;};
    connect(button(ui("Zentrieren"),"centre"),&QPushButton::clicked,this,[this]{centre();});
    connect(button(ui("Zwischenablage"),"clipboard"),&QPushButton::clicked,this,[this]{QApplication::clipboard()->setImage(printPicture(board,current));});
    connect(button(ui("Kacheln…"),"tiles"),&QPushButton::clicked,this,[this]{tilesDialog();});
    connect(button(ui("Korrektur…"),"correction"),&QPushButton::clicked,this,[this]{correctionDialog();});
    connect(button(ui("Seite einrichten…"),"pageSetup"),&QPushButton::clicked,this,[this]{QPageSetupDialog setup(device,this);if(setup.exec()==QDialog::Accepted){current.orientation=device->pageLayout().orientation();changed();}});
    buttons->addStretch();
    connect(button(ui("Drucken…"),"print"),&QPushButton::clicked,this,[this]{
        QPrintDialog dialog(device,this);if(dialog.exec()!=QDialog::Accepted)return;
        print(*device,board,current,printInfo(file,board,current),correction);accept();
    });
    connect(button(ui("Schließen"),"close"),&QPushButton::clicked,this,&QDialog::accept);
    resize(1100,760);changed();
}
PrintPreview::~PrintPreview()=default;
void PrintPreview::changed(){
    device->setPageOrientation(current.orientation);const auto page=device->pageLayout();
    sheet->paper=page.fullRect(QPageLayout::Millimeter).size();sheet->printable=page.paintRect(QPageLayout::Millimeter);
    sheet->info=printInfo(file,board,current);sheet->update();
}
void PrintPreview::specialSettings(){
    // The offsets of the mask openings per kind, each kind switchable, and the text height of the drill plan.
    QDialog dialog(this);dialog.setWindowTitle(ui("Einstellungen der Sonderlayer"));auto *form=new QFormLayout(&dialog);
    form->addRow(new QLabel(ui("Lötstoppmaske: Öffnungen größer als das Kupfer um")));
    auto row=[&](const QString &text,const char *name,bool on,double value){
        auto *box=new QCheckBox(text);box->setObjectName(QString::fromLatin1(name)+"On");box->setChecked(on);auto *spin=new QDoubleSpinBox;spin->setObjectName(name);
        spin->setRange(-9.99,9.99);spin->setDecimals(2);spin->setSingleStep(.01);spin->setSuffix(QStringLiteral(" mm"));spin->setLocale(uiLocale());spin->setValue(value);spin->setEnabled(on);
        connect(box,&QCheckBox::toggled,spin,&QWidget::setEnabled);form->addRow(box,spin);return std::pair{box,spin};};
    const auto pads=row(ui("Lötaugen"),"padMask",current.maskPads,current.padMask),smd=row(ui("SMD-Pads"),"smdMask",current.maskSmd,current.smdMask),
               other=row(ui("Sonstige"),"otherMask",current.maskOther,current.otherMask);
    auto *height=new QDoubleSpinBox;height->setObjectName("drillTextHeight");height->setRange(.1,9.9);height->setDecimals(1);height->setSingleStep(.1);height->setSuffix(QStringLiteral(" mm"));
    height->setLocale(uiLocale());height->setValue(current.drillTextHeight);form->addRow(ui("Texthöhe im Bohrplan"),height);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    current.maskPads=pads.first->isChecked();current.padMask=pads.second->value();current.maskSmd=smd.first->isChecked();current.smdMask=smd.second->value();
    current.maskOther=other.first->isChecked();current.otherMask=other.second->value();current.drillTextHeight=height->value();changed();
}
void PrintPreview::tilesDialog(){
    // Copies of the printout side by side and one below the other, with a gap between them.
    QDialog dialog(this);dialog.setWindowTitle(ui("Kacheln"));auto *form=new QFormLayout(&dialog);
    auto *across=new QSpinBox,*down=new QSpinBox;across->setObjectName("tilesX");down->setObjectName("tilesY");
    for(auto *box:{across,down})box->setRange(1,20);across->setValue(current.tilesX);down->setValue(current.tilesY);
    auto *gap=new QDoubleSpinBox;gap->setObjectName("tileGap");gap->setRange(0,50);gap->setDecimals(1);gap->setSingleStep(.1);gap->setSuffix(QStringLiteral(" mm"));gap->setLocale(uiLocale());gap->setValue(current.tileGap);
    form->addRow(ui("Nebeneinander"),across);form->addRow(ui("Untereinander"),down);form->addRow(ui("Abstand"),gap);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    current.tilesX=across->value();current.tilesY=down->value();current.tileGap=gap->value();changed();
}
void PrintPreview::correctionDialog(){
    // Factors that stretch the print for printers that print slightly too large or too small; they hold until the
    // program ends, as in the reference.
    QDialog dialog(this);dialog.setWindowTitle(ui("Korrekturfaktoren"));auto *form=new QFormLayout(&dialog);
    auto *note=new QLabel(ui("Manche Drucker drucken minimal zu groß oder zu klein. Die Faktoren (0,8 bis 1,2) strecken den Ausdruck waagerecht und senkrecht; sie gelten bis zum Ende des Programms."));
    note->setWordWrap(true);form->addRow(note);
    auto *across=new QLineEdit(uiLocale().toString(correction.x(),'f',5)),*down=new QLineEdit(uiLocale().toString(correction.y(),'f',5));across->setObjectName("correctionX");down->setObjectName("correctionY");
    form->addRow(ui("Waagerecht"),across);form->addRow(ui("Senkrecht"),down);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    // Values out of range change nothing.
    const auto x=correctionFactor(across->text()),y=correctionFactor(down->text());
    if(!x||!y){QMessageBox::warning(this,ui("Korrekturfaktoren"),ui("Bitte Faktoren zwischen 0,8 und 1,2 eingeben."));return;}
    correction={*x,*y};
}
void PrintPreview::centre(){
    const QSizeF size=printSize(board,current);const QSizeF area=sheet->printable.size();
    current.position=QPointF((area.width()-size.width())/2,(area.height()-size.height())/2);sheet->update();
}
}
