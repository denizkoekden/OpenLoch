#include "panelprint.h"
#include "panelrender.h"
#include "language.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTabBar>
#include <QVBoxLayout>
#include <cmath>

namespace openloch::frontpanel {
namespace {
constexpr double rulerBand=8,markBand=6,dataBand=8;
double rulerOf(const PrintOptions &o){return o.rulers&&o.tilesX==1&&o.tilesY==1?rulerBand:0;}   // rulers only for a single copy
double scaleOf(const PrintOptions &o){return o.original?1:std::clamp(o.zoom,0.05,20.0);}
QSizeF tilesSize(const Panel &panel,const PrintOptions &o){
    // Gaps are panel millimetres and grow with the scale like the panel.
    const double s=scaleOf(o);const int nx=std::max(1,o.tilesX),ny=std::max(1,o.tilesY);
    return {(nx*panel.width+(nx-1)*o.gapX)*s,(ny*panel.height+(ny-1)*o.gapY)*s};
}
void text(QPainter &p,QPointF at,const QString &s,double height,Qt::Alignment align=Qt::AlignLeft){
    QFont f(QStringLiteral("Arial"));f.setPixelSize(100);p.save();p.translate(at);p.scale(height/100*1.35,height/100*1.35);p.setFont(f);
    const double w=QFontMetricsF(f).horizontalAdvance(s);p.drawText(QPointF(align&Qt::AlignHCenter?-w/2:align&Qt::AlignRight?-w:0,0),s);p.restore();
}
}

QPointF printPanelOrigin(const PrintOptions &o){const double b=rulerOf(o)+(o.cutMarks?markBand:0);return {b,b};}
QSizeF printExtent(const Panel &panel,const PrintOptions &o){
    const QPointF origin=printPanelOrigin(o);const QSizeF tiles=tilesSize(panel,o);const double mark=o.cutMarks?markBand:0;
    return {origin.x()+tiles.width()+mark,origin.y()+tiles.height()+mark+(o.data?dataBand:0)};
}
QPointF printOffset(const PrintOptions &o,QSizeF extent,QSizeF area){
    if(o.centred){const QSize n=sheetsFor(extent,QPointF(),area);return {(n.width()*area.width()-extent.width())/2,(n.height()*area.height()-extent.height())/2};}
    // The place of the first tile is panel millimetres from the corner of the printable area.
    return QPointF(o.left,o.top)*scaleOf(o)-printPanelOrigin(o);
}
QSize sheetsFor(QSizeF extent,QPointF offset,QSizeF area){
    if(area.width()<=0||area.height()<=0)return {1,1};
    const auto count=[](double length,double size){return std::max(1,int(std::ceil(length/size-1e-9)));};
    return {count(offset.x()+extent.width(),area.width()),count(offset.y()+extent.height(),area.height())};
}

void paintPrintout(QPainter &p,const Document &document,const Panel &panel,const PrintOptions &o,const QString &title){
    const double s=scaleOf(o),ruler=rulerOf(o),mark=o.cutMarks?markBand:0;const QPointF origin=printPanelOrigin(o);
    RenderOptions r;r.background=o.background;r.dimensions=o.dimensions;r.milled=r.drills=r.engraved=o.machining;r.other=o.objects;r.texts=o.texts;
    QPen thin(Qt::black,0.1);
    for(int ty=0;ty<std::max(1,o.tilesY);ty++)for(int tx=0;tx<std::max(1,o.tilesX);tx++){
        const QPointF at=origin+QPointF(tx*(panel.width+o.gapX),ty*(panel.height+o.gapY))*s;const QRectF tile(at,QSizeF(panel.width*s,panel.height*s));
        p.save();p.translate(at);p.scale(s,s);
        if(o.mirror){p.translate(panel.width,0);p.scale(-1,1);}
        paintPanel(p,document,panel,r);
        if(o.frame){p.setPen(QPen(Qt::black,0.1/s));p.setBrush(Qt::NoBrush);p.drawRect(QRectF(0,0,panel.width,panel.height));}
        p.restore();
        if(o.cutMarks){
            // Short lines in line with the edges, a little away from each corner.
            p.setPen(thin);const double gap=1.5,len=mark-gap-0.5;
            for(const QPointF &c:{tile.topLeft(),tile.topRight(),tile.bottomLeft(),tile.bottomRight()}){
                const double sx=c.x()<tile.center().x()?-1:1,sy=c.y()<tile.center().y()?-1:1;
                p.drawLine(c+QPointF(sx*gap,0),c+QPointF(sx*(gap+len),0));p.drawLine(c+QPointF(0,sy*gap),c+QPointF(0,sy*(gap+len)));
            }
        }
    }
    if(ruler>0){
        // Millimetre rulers along the top and left edge of the panel.
        p.setPen(thin);const QRectF tile(origin,QSizeF(panel.width*s,panel.height*s));
        for(int k=0;k<=int(std::floor(panel.width+1e-9));k++){const double x=tile.left()+k*s,len=k%10==0?4:k%5==0?2.5:1.5;p.drawLine(QPointF(x,ruler),QPointF(x,ruler-len));
            if(k%10==0)text(p,QPointF(x+0.4,ruler-4.2),QString::number(k),2);}
        for(int k=0;k<=int(std::floor(panel.height+1e-9));k++){const double y=tile.top()+k*s,len=k%10==0?4:k%5==0?2.5:1.5;p.drawLine(QPointF(ruler,y),QPointF(ruler-len,y));
            if(k%10==0){p.save();p.translate(ruler-4.2,y-0.4);p.rotate(-90);text(p,QPointF(),QString::number(k),2);p.restore();}}
        p.drawLine(QPointF(tile.left(),ruler),QPointF(tile.right(),ruler));p.drawLine(QPointF(ruler,tile.top()),QPointF(ruler,tile.bottom()));
    }
    if(o.data){
        const QSizeF tiles=tilesSize(panel,o);const QLocale l=uiLocale();
        const QString line=QStringList{title,panel.name,ui("%1 × %2 mm").arg(l.toString(panel.width,'g',6),l.toString(panel.height,'g',6)),
            o.original?QStringLiteral("1:1"):QString("%1 %").arg(l.toString(s*100,'f',0)),l.toString(QDate::currentDate(),QLocale::ShortFormat)}.join(QStringLiteral("  ·  "));
        p.setPen(Qt::black);text(p,QPointF(origin.x(),origin.y()+tiles.height()+mark+5),line,2.5);
    }
}

namespace {
struct Paper {QSizeF size;QRectF printable;};
Paper paperOf(const QPrinter &printer){const QPageLayout l=printer.pageLayout();return {l.fullRect(QPageLayout::Millimeter).size(),l.paintRect(QPageLayout::Millimeter)};}
// All sheets of the printout side by side; the printout can be moved with the mouse.
class PrintPreview : public QWidget {
public:
    const Document *document=nullptr;int panel=0;PrintOptions *options=nullptr;Paper paper;QString title;
    std::function<void()> moved;
    PrintPreview(){setMinimumSize(420,420);setCursor(Qt::OpenHandCursor);setToolTip(ui("Ausdruck verschieben…"));}
    QSize sheets() const{const Panel &p=document->panels[panel];const QSizeF e=printExtent(p,*options);return sheetsFor(e,printOffset(*options,e,paper.printable.size()),paper.printable.size());}
protected:
    double factor(QSize n) const{
        const double gap=10;const double w=n.width()*paper.size.width(),h=n.height()*paper.size.height();
        return std::min((width()-20-(n.width()-1)*gap)/std::max(1.0,w),(height()-20-(n.height()-1)*gap)/std::max(1.0,h));
    }
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);p.fillRect(rect(),QColor(0x80,0x80,0x80));if(!document||!options)return;p.setRenderHint(QPainter::Antialiasing);
        const Panel &pl=document->panels[panel];const QSizeF extent=printExtent(pl,*options);const QPointF offset=printOffset(*options,extent,paper.printable.size());const QSize n=sheetsFor(extent,offset,paper.printable.size());
        const double k=factor(n),gap=10;const QSizeF area=paper.printable.size();
        for(int r=0;r<n.height();r++)for(int c=0;c<n.width();c++){
            const int number=r*n.width()+c+1;const QPointF at(10+c*(paper.size.width()*k+gap),10+r*(paper.size.height()*k+gap));
            const QRectF sheet(at,paper.size*k);p.fillRect(sheet.translated(3,3),QColor(0,0,0,80));
            p.fillRect(sheet,options->onlyOne&&number!=options->sheet?QColor(0xe8,0xe8,0xe8):QColor(Qt::white));
            p.save();p.translate(at);p.scale(k,k);
            QPen dash(QColor(0xa0,0xa0,0xa0),0);dash.setCosmetic(true);dash.setStyle(Qt::DashLine);p.setPen(dash);p.drawRect(paper.printable);
            p.setClipRect(paper.printable);p.translate(paper.printable.topLeft());
            p.scale(options->correctionX,options->correctionY);p.translate(-c*area.width()+offset.x(),-r*area.height()+offset.y());
            paintPrintout(p,*document,pl,*options,title);p.restore();
            if(n.width()*n.height()>1){p.setPen(Qt::black);p.drawText(sheet.adjusted(4,2,-4,-2),Qt::AlignRight|Qt::AlignBottom,QString::number(number));}
        }
    }
    void mousePressEvent(QMouseEvent *e) override{last=e->position();setCursor(Qt::ClosedHandCursor);}
    void mouseMoveEvent(QMouseEvent *e) override{
        if(!(e->buttons()&Qt::LeftButton)||!options)return;
        const Panel &pl=document->panels[panel];const QSizeF extent=printExtent(pl,*options);const QPointF offset=printOffset(*options,extent,paper.printable.size());
        const double k=factor(sheetsFor(extent,offset,paper.printable.size()));const QPointF d=(e->position()-last)/k;last=e->position();
        // The panel follows the mouse, also beyond the printable area (label sheets are printed up to their edge).
        const QPointF corner=(offset+printPanelOrigin(*options)+d)/scaleOf(*options);options->centred=false;options->left=corner.x();options->top=corner.y();
        if(moved)moved();update();
    }
    void mouseReleaseEvent(QMouseEvent *) override{setCursor(Qt::OpenHandCursor);}
private:
    QPointF last;
};
QDoubleSpinBox *millimetres(double low,double high){auto *s=new QDoubleSpinBox;s->setRange(low,high);s->setDecimals(2);s->setSuffix(" mm");s->setLocale(uiLocale());s->setKeyboardTracking(false);return s;}
}

bool printPanels(QWidget *parent,const Document &document,const QString &title,QList<PrintSettings> *settings){
    if(document.panels.isEmpty())return false;
    // Each panel has its own settings; the preview edits those of the panel shown. Only the calibration is kept here.
    QList<PrintSettings> all;for(const auto &p:document.panels)all<<p.print;
    const int active=std::clamp(document.activePanel,0,int(document.panels.size())-1);
    PrintOptions o;static_cast<PrintSettings&>(o)=all[active];QSettings stored;
    o.correctionX=std::clamp(stored.value("frontpanel/printCorrectionX",1.0).toDouble(),0.8,1.2);o.correctionY=std::clamp(stored.value("frontpanel/printCorrectionY",1.0).toDouble(),0.8,1.2);
    QPrinter printer(QPrinter::HighResolution);printer.setPageOrientation(o.landscape?QPageLayout::Landscape:QPageLayout::Portrait);printer.setDocName(title);

    QDialog d(parent);d.setWindowTitle(ui("Druckvorschau"));d.resize(1080,760);bool loading=false;
    auto *layout=new QHBoxLayout(&d);auto *left=new QVBoxLayout;layout->addLayout(left,1);
    auto *preview=new PrintPreview;preview->document=&document;preview->panel=active;preview->options=&o;preview->paper=paperOf(printer);preview->title=title;left->addWidget(preview,1);
    auto *tabs=new QTabBar;tabs->setShape(QTabBar::RoundedSouth);tabs->setExpanding(false);for(const auto &p:document.panels)tabs->addTab(p.name);tabs->setCurrentIndex(active);left->addWidget(tabs);
    auto *status=new QLabel;left->addWidget(status);
    auto *right=new QVBoxLayout;layout->addLayout(right);

    auto *viewBox=new QGroupBox(ui("Ansicht"));auto *vl=new QVBoxLayout(viewBox);auto *normal=new QRadioButton(ui("&Normal")),*mirror=new QRadioButton(ui("&Spiegeln"));vl->addWidget(normal);vl->addWidget(mirror);
    auto *optionBox=new QGroupBox(ui("Optionen"));auto *ol=new QGridLayout(optionBox);
    auto *background=new QCheckBox(ui("H&intergrund")),*frame=new QCheckBox(ui("&Rahmen")),*rulers=new QCheckBox(ui("&Lineale")),*data=new QCheckBox(ui("Da&ten")),*dims=new QCheckBox(ui("&Bemaßungen"));
    auto *machining=new QCheckBox(ui("Bohren && &Fräsen")),*objects=new QCheckBox(ui("&Objekte && Symbole")),*marks=new QCheckBox(ui("Sch&nittmarken")),*texts=new QCheckBox(ui("&Texte"));
    const QList<QCheckBox*> checks{background,frame,rulers,data,dims,machining,objects,marks,texts};for(int i=0;i<checks.size();i++)ol->addWidget(checks[i],i/2,i%2);
    auto *scaleBox=new QGroupBox(ui("Skalierung"));auto *sl=new QGridLayout(scaleBox);auto *original=new QRadioButton(ui("Original&größe 1:1")),*zoom=new QRadioButton(ui("&Vergrößern:"));
    auto *slider=new QSlider(Qt::Horizontal);slider->setRange(10,400);auto *percent=new QLabel;auto *centred=new QCheckBox(ui("&mittig ausrichten"));auto *onlyOne=new QCheckBox(ui("n&ur ein Blatt drucken:"));
    auto *sheet=new QSpinBox;sheet->setMinimum(1);
    sl->addWidget(original,0,0,1,3);sl->addWidget(zoom,1,0);sl->addWidget(slider,1,1);sl->addWidget(percent,1,2);sl->addWidget(centred,2,0,1,3);sl->addWidget(onlyOne,3,0,1,2);sl->addWidget(sheet,3,2);
    auto *paperBox=new QGroupBox(ui("Papier"));auto *pl=new QVBoxLayout(paperBox);auto *portrait=new QRadioButton(ui("&Hochformat")),*landscape=new QRadioButton(ui("&Querformat"));pl->addWidget(portrait);pl->addWidget(landscape);
    auto *marginBox=new QGroupBox(ui("Abstand vom Papierrand"));auto *ml=new QFormLayout(marginBox);auto *horizontal=millimetres(0,2000),*vertical=millimetres(0,2000);
    ml->addRow(ui("Horizontal:"),horizontal);ml->addRow(ui("Vertikal:"),vertical);
    auto *copyBox=new QGroupBox(ui("Exemplare"));auto *cl=new QHBoxLayout(copyBox);auto *copies=new QSpinBox;copies->setRange(1,999);copies->setToolTip(ui("Anzahl der Exemplare"));
    auto *range=new QComboBox;range->addItems({ui("von der gewählten Frontplatte"),ui("von allen Frontplatten")});cl->addWidget(copies);cl->addWidget(range,1);
    for(auto *w:{viewBox,optionBox,scaleBox,paperBox,marginBox,copyBox})right->addWidget(w);
    auto *buttonsRow=new QGridLayout;auto *tilesButton=new QPushButton(ui("&Kacheln…")),*calibrate=new QPushButton(ui("Kal&ibrieren…")),*reset=new QPushButton(ui("&Zurücksetzen")),*setup=new QPushButton(ui("&Einrichten…"));
    auto *printButton=new QPushButton(ui("&Drucken")),*cancel=new QPushButton(ui("Abbre&chen"));printButton->setDefault(true);
    buttonsRow->addWidget(tilesButton,0,0);buttonsRow->addWidget(calibrate,0,1);buttonsRow->addWidget(reset,1,0);buttonsRow->addWidget(setup,1,1);buttonsRow->addWidget(printButton,2,0);buttonsRow->addWidget(cancel,2,1);
    right->addStretch();right->addLayout(buttonsRow);

    auto write=[&]{
        loading=true;(o.mirror?mirror:normal)->setChecked(true);background->setChecked(o.background);frame->setChecked(o.frame);rulers->setChecked(o.rulers);data->setChecked(o.data);dims->setChecked(o.dimensions);
        machining->setChecked(o.machining);objects->setChecked(o.objects);marks->setChecked(o.cutMarks);texts->setChecked(o.texts);(o.original?original:zoom)->setChecked(true);
        slider->setValue(int(std::lround(o.zoom*100)));centred->setChecked(o.centred);onlyOne->setChecked(o.onlyOne);(o.landscape?landscape:portrait)->setChecked(true);
        sheet->setValue(o.sheet);copies->setValue(o.copies);range->setCurrentIndex(o.allPanels?1:0);loading=false;
    };
    // The margin fields show where the panel lies on the paper, from its edge.
    auto panelOnPaper=[&]{const Panel &p=document.panels[preview->panel];const QSizeF area=preview->paper.printable.size();
        return preview->paper.printable.topLeft()+printOffset(o,printExtent(p,o),area)+printPanelOrigin(o);};
    auto refresh=[&]{
        loading=true;percent->setText(QString("%1 %").arg(slider->value()));slider->setEnabled(!o.original);
        const QSize n=preview->sheets();sheet->setMaximum(std::max(1,n.width()*n.height()));sheet->setEnabled(o.onlyOne);const QPointF place=panelOnPaper();horizontal->setValue(place.x());vertical->setValue(place.y());
        horizontal->setEnabled(!o.centred);vertical->setEnabled(!o.centred);centred->setChecked(o.centred);loading=false;
        const Panel &p=document.panels[preview->panel];
        status->setText(ui("%1 Blatt · Frontplatte %2 × %3 mm · Kacheln %4 × %5").arg(n.width()*n.height()).arg(uiLocale().toString(p.width,'g',6),uiLocale().toString(p.height,'g',6)).arg(o.tilesX).arg(o.tilesY));
        preview->update();
    };
    auto read=[&]{
        if(loading)return;
        o.mirror=mirror->isChecked();o.background=background->isChecked();o.frame=frame->isChecked();o.rulers=rulers->isChecked();o.data=data->isChecked();o.dimensions=dims->isChecked();
        o.machining=machining->isChecked();o.objects=objects->isChecked();o.cutMarks=marks->isChecked();o.texts=texts->isChecked();o.original=original->isChecked();o.zoom=slider->value()/100.0;
        o.centred=centred->isChecked();o.onlyOne=onlyOne->isChecked();o.sheet=sheet->value();o.copies=copies->value();o.allPanels=range->currentIndex()==1;
        const bool wide=landscape->isChecked();if(wide!=o.landscape){o.landscape=wide;printer.setPageOrientation(wide?QPageLayout::Landscape:QPageLayout::Portrait);preview->paper=paperOf(printer);}
        refresh();
    };
    // Another panel brings its own settings and paper orientation.
    auto show=[&](int index){
        all[preview->panel]=o;static_cast<PrintSettings&>(o)=all[index];preview->panel=index;
        printer.setPageOrientation(o.landscape?QPageLayout::Landscape:QPageLayout::Portrait);preview->paper=paperOf(printer);write();refresh();};
    write();refresh();
    for(auto *b:checks)QObject::connect(b,&QCheckBox::toggled,&d,read);
    for(auto *b:{normal,mirror,original,zoom,portrait,landscape})QObject::connect(b,&QRadioButton::toggled,&d,read);
    for(auto *b:{centred,onlyOne})QObject::connect(b,&QCheckBox::toggled,&d,read);
    QObject::connect(slider,&QSlider::valueChanged,&d,read);QObject::connect(sheet,&QSpinBox::valueChanged,&d,read);QObject::connect(copies,&QSpinBox::valueChanged,&d,read);
    QObject::connect(range,&QComboBox::currentIndexChanged,&d,read);
    // A margin typed in moves the panel to that distance from the paper's edge.
    QObject::connect(horizontal,&QDoubleSpinBox::valueChanged,&d,[&](double v){if(!loading){o.left=(v-preview->paper.printable.left())/scaleOf(o);refresh();}});
    QObject::connect(vertical,&QDoubleSpinBox::valueChanged,&d,[&](double v){if(!loading){o.top=(v-preview->paper.printable.top())/scaleOf(o);refresh();}});
    QObject::connect(tabs,&QTabBar::currentChanged,&d,[&](int i){if(i>=0&&i!=preview->panel)show(i);});
    preview->moved=refresh;
    QObject::connect(tilesButton,&QPushButton::clicked,&d,[&]{
        QDialog t(&d);t.setWindowTitle(ui("Kacheln"));auto *f=new QFormLayout(&t);auto *nx=new QSpinBox,*ny=new QSpinBox;nx->setRange(1,PrintSettings::maxTiles);ny->setRange(1,PrintSettings::maxTiles);nx->setValue(o.tilesX);ny->setValue(o.tilesY);
        auto *gx=millimetres(0,500),*gy=millimetres(0,500);gx->setValue(o.gapX);gy->setValue(o.gapY);
        f->addRow(ui("Anzahl waagerecht:"),nx);f->addRow(ui("Anzahl senkrecht:"),ny);f->addRow(ui("Abstand waagerecht:"),gx);f->addRow(ui("Abstand senkrecht:"),gy);
        auto *box=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);box->button(QDialogButtonBox::Ok)->setText(ui("OK"));box->button(QDialogButtonBox::Cancel)->setText(ui("Abbrechen"));
        QObject::connect(box,&QDialogButtonBox::accepted,&t,&QDialog::accept);QObject::connect(box,&QDialogButtonBox::rejected,&t,&QDialog::reject);f->addRow(box);
        if(t.exec()==QDialog::Accepted){o.tilesX=nx->value();o.tilesY=ny->value();o.gapX=gx->value();o.gapY=gy->value();refresh();}
    });
    QObject::connect(calibrate,&QPushButton::clicked,&d,[&]{
        QDialog t(&d);t.setWindowTitle(ui("Korrekturfaktoren"));auto *f=new QFormLayout(&t);
        auto *hint=new QLabel(ui("Manche Drucker geben Längen etwas verzerrt aus. Drucken Sie eine Frontplatte in Originalgröße, messen Sie nach und tragen Sie hier Sollmaß durch Istmaß ein: Werte über 1 vergrößern den Ausdruck, Werte unter 1 verkleinern ihn."));
        hint->setWordWrap(true);hint->setMaximumWidth(380);f->addRow(hint);
        auto factor=[](double v){auto *s=new QDoubleSpinBox;s->setRange(0.8,1.2);s->setDecimals(4);s->setSingleStep(0.001);s->setLocale(uiLocale());s->setValue(v);return s;};
        auto *x=factor(o.correctionX),*y=factor(o.correctionY);f->addRow(ui("Waagerechter Korrekturfaktor:"),x);f->addRow(ui("Senkrechter Korrekturfaktor:"),y);
        auto *box=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);box->button(QDialogButtonBox::Ok)->setText(ui("OK"));box->button(QDialogButtonBox::Cancel)->setText(ui("Abbrechen"));
        QObject::connect(box,&QDialogButtonBox::accepted,&t,&QDialog::accept);QObject::connect(box,&QDialogButtonBox::rejected,&t,&QDialog::reject);f->addRow(box);
        if(t.exec()==QDialog::Accepted){o.correctionX=x->value();o.correctionY=y->value();stored.setValue("frontpanel/printCorrectionX",o.correctionX);stored.setValue("frontpanel/printCorrectionY",o.correctionY);refresh();}
    });
    QObject::connect(reset,&QPushButton::clicked,&d,[&]{static_cast<PrintSettings&>(o)=PrintSettings();
        printer.setPageOrientation(QPageLayout::Portrait);preview->paper=paperOf(printer);write();refresh();});
    QObject::connect(setup,&QPushButton::clicked,&d,[&]{
        QPrintDialog dialog(&printer,&d);dialog.setWindowTitle(ui("Drucker einrichten"));dialog.setOptions(QAbstractPrintDialog::PrintToFile|QAbstractPrintDialog::PrintShowPageSize);
        if(dialog.exec()==QDialog::Accepted){o.landscape=printer.pageLayout().orientation()==QPageLayout::Landscape;preview->paper=paperOf(printer);write();refresh();}
    });
    QObject::connect(cancel,&QPushButton::clicked,&d,&QDialog::reject);
    QObject::connect(printButton,&QPushButton::clicked,&d,[&]{
        read();all[preview->panel]=o;printer.setCopyCount(o.copies);
        QList<int> which;if(o.allPanels)for(int i=0;i<document.panels.size();i++)which<<i;else which<<preview->panel;
        // Every panel is printed with its own settings, on paper turned as they say.
        auto optionsFor=[&](int index){PrintOptions po=o;static_cast<PrintSettings&>(po)=all[index];return po;};
        auto turned=[](const PrintOptions &po){return po.landscape?QPageLayout::Landscape:QPageLayout::Portrait;};
        printer.setPageOrientation(turned(optionsFor(which.first())));QPainter painter;
        if(!painter.begin(&printer)){
            printer.setPageOrientation(turned(o));QMessageBox::warning(&d,ui("Drucken"),ui("Der Drucker kann nicht angesprochen werden."));return;}
        bool first=true;
        for(int index:which){
            const PrintOptions po=optionsFor(index);const Panel &panel=document.panels[index];
            if(!first)printer.setPageOrientation(turned(po));   // applies from the next page on
            const Paper paper=paperOf(printer);const QSizeF area=paper.printable.size();
            const QSizeF extent=printExtent(panel,po);const QPointF offset=printOffset(po,extent,area);const QSize n=sheetsFor(extent,offset,area);
            for(int r=0;r<n.height();r++)for(int c=0;c<n.width();c++){
                const int number=r*n.width()+c+1;if(po.onlyOne&&!o.allPanels&&number!=po.sheet)continue;
                if(!first)printer.newPage();first=false;
                painter.save();const double dots=printer.resolution()/25.4;painter.scale(dots*po.correctionX,dots*po.correctionY);
                painter.translate(-c*area.width()+offset.x(),-r*area.height()+offset.y());paintPrintout(painter,document,panel,po,title);painter.restore();
            }
        }
        painter.end();d.accept();
    });
    const bool done=d.exec()==QDialog::Accepted;
    all[preview->panel]=o;if(settings)*settings=all;
    return done;
}
}
