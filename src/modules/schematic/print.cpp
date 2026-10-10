#include "print.h"
#include "dialogs.h"
#include "language.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPageSetupDialog>
#include <QPainter>
#include <QPdfWriter>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <functional>

namespace openloch::schematic {
QPageLayout pageFor(const QPageLayout &printer,const Sheet &sheet,const PrintSettings &s){
    QPageLayout l=printer;
    const bool wide=s.orientation==PrintSettings::Orientation::Automatic?sheet.width>sheet.height:s.orientation==PrintSettings::Orientation::Landscape;
    l.setOrientation(wide?QPageLayout::Landscape:QPageLayout::Portrait);
    return l;
}
namespace {
QSizeF paperOf(const QPageLayout &page){return page.fullRect(QPageLayout::Millimeter).size();}
QPointF stepOf(const QPageLayout &page,const PrintSettings &s){const QSizeF p=paperOf(page);return QPointF(p.width()-s.overlap,p.height()-s.overlap);}
// The printable part of one page: inside its margins, also when the layout paints the full page.
QRectF printableOne(const QPageLayout &page){return page.fullRect(QPageLayout::Millimeter).marginsRemoved(page.margins(QPageLayout::Millimeter));}
// The printable part of all pages of a banner print taken together.
QRectF printable(const QPageLayout &page,const PrintSettings &s){
    const QRectF one=printableOne(page);const QPointF step=stepOf(page,s);
    return QRectF(one.topLeft(),one.bottomRight()+QPointF((s.bannerX-1)*step.x(),(s.bannerY-1)*step.y()));
}
}
QRectF bannerArea(const QPageLayout &page,const PrintSettings &s){
    const QSizeF p=paperOf(page);const QPointF step=stepOf(page,s);
    return QRectF(0,0,(s.bannerX-1)*step.x()+p.width(),(s.bannerY-1)*step.y()+p.height());
}
QRectF printedContent(const Sheet &sheet){
    QRectF content;for(const auto &i:sheet.titleBlock.items)content|=bounds(i);for(const auto &i:sheet.items)content|=bounds(i);
    return content;
}
QPointF sheetOrigin(const QPageLayout &page,const Sheet &sheet,const PrintSettings &s){
    const QPointF corner=printableOne(page).topLeft()+s.offset;
    const QRectF content=printedContent(sheet);
    return content.isNull()?corner:corner-content.topLeft()*s.factor();
}
QRectF drawingArea(const QPageLayout &page,const Sheet &sheet,const PrintSettings &s){
    return QRectF(sheetOrigin(page,sheet,s),QSizeF(sheet.width,sheet.height)*s.factor());
}
std::array<bool,4> cutSides(const QPageLayout &page,const Sheet &sheet,const PrintSettings &s){
    const QRectF content=printedContent(sheet);
    if(content.isNull())return {false,false,false,false};
    const QRectF d(printableOne(page).topLeft()+s.offset,content.size()*s.factor());
    const QRectF p=printable(page,s);constexpr double eps=1e-6;
    return {d.left()<p.left()-eps,d.top()<p.top()-eps,d.right()>p.right()+eps,d.bottom()>p.bottom()+eps};
}
PrintSettings fitted(const QPageLayout &page,const Sheet &sheet,PrintSettings s){
    const QRectF content=printedContent(sheet);
    if(content.width()<=0||content.height()<=0)return centred(page,sheet,s);
    // As the reference: the printable part of every page of a banner print less the overlaps, 98 % of the scale that
    // fits, in whole percent rounded to the nearest (ties to even, as the reference's x87 store rounds).
    const QSizeF one=printableOne(page).size();
    const double w=s.bannerX*one.width()-(s.bannerX-1)*s.overlap,h=s.bannerY*one.height()-(s.bannerY-1)*s.overlap;
    // Within the reference's 10 to 800 %.
    s.free=true;s.scale=std::clamp(std::nearbyint(std::min(w/content.width(),h/content.height())*98)/100,.1,8.);
    return centred(page,sheet,s);
}
PrintSettings centred(const QPageLayout &page,const Sheet &sheet,PrintSettings s){
    const QRectF p=printable(page,s),content=printedContent(sheet);
    if(content.isNull())return s;
    s.offset=p.center()-QPointF(content.width(),content.height())*s.factor()/2-printableOne(page).topLeft();
    // A tenth of a millimetre, as the preview keeps it.
    s.offset=QPointF(std::round(s.offset.x()*10)/10,std::round(s.offset.y()*10)/10);
    return s;
}
QPointF resetOffset(const QPageLayout &page,const Sheet &sheet,OffsetReset to){
    // The offset runs from the printable area's corner to the printed content's: the paper's edge lies the margin
    // before it, the sheet's corner the content's place on the sheet before the content.
    const QPointF margin=printableOne(page).topLeft()-page.fullRect(QPageLayout::Millimeter).topLeft(),content=printedContent(sheet).topLeft();
    switch(to){
    case OffsetReset::SheetToPaper:return content-margin;
    case OffsetReset::SheetToPrintable:return content;
    case OffsetReset::ContentToPaper:return -margin;
    default:return {};
    }
}
int printSheets(QPrinter &printer,const Document &document,const QList<int> &sheets,const QList<PrintSettings> &settings,const RenderOptions &options){
    printer.setFullPage(true);
    QPainter painter;int pages=0;const QPageLayout base=printer.pageLayout();
    for(int s:sheets){
        if(s<0||s>=document.sheets.size())continue;
        const PrintSettings st=settings.value(s);const QPageLayout layout=pageFor(base,document.sheets[s],st);const QPointF step=stepOf(layout,st);
        for(int j=0;j<st.bannerY;j++)for(int i=0;i<st.bannerX;i++){
            printer.setPageLayout(layout);
            if(!pages){if(!painter.begin(&printer))return 0;}
            else printer.newPage();
            painter.save();
            const double dpm=printer.resolution()/25.4;painter.scale(dpm,dpm);
            painter.translate(-i*step.x(),-j*step.y());painter.translate(sheetOrigin(layout,document.sheets[s],st));painter.scale(st.factor(),st.factor());
            paintSheet(painter,document,s,options);
            painter.restore();pages++;
        }
    }
    if(pages)painter.end();
    return pages;
}
bool exportPdf(const Document &document,const QList<int> &sheets,const QString &file,const RenderOptions &options,QString *error){
    QPdfWriter writer(file);writer.setResolution(1200);writer.setCreator(QStringLiteral("OpenLoch"));
    QPainter painter;bool first=true;
    for(int s:sheets){
        if(s<0||s>=document.sheets.size())continue;
        const Sheet &sheet=document.sheets[s];
        const QPageSize size(QSizeF(std::min(sheet.width,sheet.height),std::max(sheet.width,sheet.height)),QPageSize::Millimeter,QString(),QPageSize::ExactMatch);
        writer.setPageLayout(QPageLayout(size,sheet.width>sheet.height?QPageLayout::Landscape:QPageLayout::Portrait,QMarginsF()));
        if(first){if(!painter.begin(&writer)){if(error)*error=ui("Die PDF-Datei kann nicht geschrieben werden: %1").arg(file);return false;}first=false;}
        else writer.newPage();
        painter.save();painter.scale(writer.resolution()/25.4,writer.resolution()/25.4);paintSheet(painter,document,s,options);painter.restore();
    }
    if(first){if(error)*error=ui("Kein Blatt zum Exportieren");return false;}
    painter.end();
    return true;
}

// --- The print preview
class PaperView : public QWidget {
public:
    explicit PaperView(PrintPreview *p):preview(p){setMinimumSize(320,240);setMouseTracking(false);setCursor(Qt::OpenHandCursor);}
protected:
    void paintEvent(QPaintEvent *) override{
        QPainter painter(this);painter.fillRect(rect(),QColor(128,128,128));
        const auto &doc=preview->document;const int s=preview->current;if(s<0||s>=doc.sheets.size())return;
        const PrintSettings st=preview->settings.value(s);const QPageLayout page=pageFor(preview->device->pageLayout(),doc.sheets[s],st);
        const QRectF area=bannerArea(page,st);
        view=std::min((width()-24)/area.width(),(height()-24)/area.height());
        origin=QPointF((width()-area.width()*view)/2,(height()-area.height()*view)/2);
        painter.setRenderHint(QPainter::Antialiasing);painter.translate(origin);painter.scale(view,view);
        const QSizeF paper=paperOf(page);const QPointF step=stepOf(page,st);const QRectF printableFirst=printableOne(page);
        for(int j=0;j<st.bannerY;j++)for(int i=0;i<st.bannerX;i++){
            const QRectF r(QPointF(i*step.x(),j*step.y()),paper);painter.fillRect(r,Qt::white);
            painter.setPen(QPen(QColor(90,90,90),0));painter.drawRect(r);
        }
        painter.save();painter.setClipRect(area);painter.translate(sheetOrigin(page,doc.sheets[s],st));painter.scale(st.factor(),st.factor());
        RenderOptions o;o.fileName=preview->fileName;o.paper=false;paintSheet(painter,doc,s,o);painter.restore();
        // The printable area of each page, its sides red where the drawing is cut.
        for(int j=0;j<st.bannerY;j++)for(int i=0;i<st.bannerX;i++){
            painter.setPen(QPen(QColor(230,120,120),0,Qt::DashLine));painter.setBrush(Qt::NoBrush);
            painter.drawRect(printableFirst.translated(i*step.x(),j*step.y()));
        }
        const auto cut=cutSides(page,doc.sheets[s],st);const QRectF all=printable(page,st);
        painter.setPen(QPen(QColor(220,0,0),3/view));
        if(cut[0])painter.drawLine(all.topLeft(),all.bottomLeft());
        if(cut[1])painter.drawLine(all.topLeft(),all.topRight());
        if(cut[2])painter.drawLine(all.topRight(),all.bottomRight());
        if(cut[3])painter.drawLine(all.bottomLeft(),all.bottomRight());
    }
    void mousePressEvent(QMouseEvent *e) override{
        if(e->button()!=Qt::LeftButton)return;
        start=e->position();startOffset=preview->settings.value(preview->current).offset;dragging=true;setCursor(Qt::ClosedHandCursor);
    }
    void mouseMoveEvent(QMouseEvent *e) override{
        if(!dragging||view<=0)return;
        PrintSettings s=preview->settings.value(preview->current);s.offset=startOffset+(e->position()-start)/view;
        s.offset=QPointF(std::round(s.offset.x()*10)/10,std::round(s.offset.y()*10)/10);
        preview->apply(s);
    }
    void mouseReleaseEvent(QMouseEvent *) override{dragging=false;setCursor(Qt::OpenHandCursor);}
private:
    PrintPreview *preview;
    double view=1;QPointF origin,start,startOffset;bool dragging=false;
};

PrintPreview::PrintPreview(const Document &doc,QList<PrintSettings> &all,const QString &file,QWidget *parent)
    :QDialog(parent),document(doc),settings(all),fileName(file){
    setWindowTitle(ui("Druckvorschau"));
    while(settings.size()<document.sheets.size())settings.append(PrintSettings());
    // The free scale within the reference's 10 to 800 %, so that what is shown is what is printed.
    for(auto &s:settings)s.scale=std::clamp(s.scale,.1,8.);
    device=new QPrinter(QPrinter::HighResolution);
    current=std::clamp(document.activeSheet,0,int(document.sheets.size())-1);
    auto *outer=new QHBoxLayout(this);auto *left=new QVBoxLayout;outer->addLayout(left);
    auto *middle=new QVBoxLayout;outer->addLayout(middle,1);
    // Printer
    auto *printerBox=new QGroupBox(ui("Drucker"));auto *pl=new QHBoxLayout(printerBox);left->addWidget(printerBox);
    printers=new QComboBox;printers->setObjectName("printer");
    for(const auto &name:QPrinterInfo::availablePrinterNames())printers->addItem(name,name);
    printers->addItem(ui("PDF-Datei"),QString());
    {const QString standard=QPrinterInfo::defaultPrinterName();const int at=printers->findData(standard);printers->setCurrentIndex(at>=0&&!standard.isEmpty()?at:printers->count()-1);}
    auto choosePrinter=[this]{
        const QString name=printers->currentData().toString();
        if(name.isEmpty())device->setOutputFormat(QPrinter::PdfFormat);else{device->setOutputFormat(QPrinter::NativeFormat);device->setPrinterName(name);}
        settingsChanged();};
    choosePrinter();connect(printers,&QComboBox::currentIndexChanged,this,choosePrinter);
    pl->addWidget(printers,1);
    auto *setup=new QPushButton(ui("Einrichten..."));setup->setObjectName("setup");pl->addWidget(setup);
    connect(setup,&QPushButton::clicked,this,[this]{QPageSetupDialog d(device,this);if(d.exec()==QDialog::Accepted)settingsChanged();});
    // Scale
    auto *scaleBox=new QGroupBox(ui("Skalierung"));auto *sl=new QVBoxLayout(scaleBox);left->addWidget(scaleBox);
    {auto *row=new QHBoxLayout;sl->addLayout(row);
        oneToOne=new QRadioButton(QStringLiteral("1:1"));oneToOne->setObjectName("oneToOne");row->addWidget(oneToOne);
        freeScale=new QRadioButton(ui("frei"));freeScale->setObjectName("freeScale");row->addWidget(freeScale);row->addStretch();
        auto *g=new QButtonGroup(this);g->addButton(oneToOne);g->addButton(freeScale);}
    {auto *row=new QHBoxLayout;sl->addLayout(row);
        scalePercent=new QSpinBox;scalePercent->setObjectName("scalePercent");scalePercent->setRange(10,800);row->addWidget(scalePercent);
        row->addWidget(new QLabel(QStringLiteral("%")));row->addStretch();}
    scaleSlider=new QSlider(Qt::Horizontal);scaleSlider->setObjectName("scale");scaleSlider->setRange(10,800);sl->addWidget(scaleSlider);
    // Offset
    auto *offsetBox=new QGroupBox(ui("Offset"));auto *of=new QFormLayout(offsetBox);left->addWidget(offsetBox);
    offsetBox->setToolTip(ui("Von der linken oberen Ecke des bedruckbaren Bereichs zur linken oberen Ecke der Zeichnung (Formblatt und Schaltung), nach rechts und unten positiv, unabhängig von der Skalierung"));
    auto mm=[](const char *name){auto *b=new QDoubleSpinBox;b->setObjectName(name);b->setRange(-5000,5000);b->setDecimals(1);b->setSuffix(QStringLiteral(" mm"));b->setLocale(uiLocale());return b;};
    offsetX=mm("offsetX");offsetY=mm("offsetY");of->addRow(QStringLiteral("X:"),offsetX);of->addRow(QStringLiteral("Y:"),offsetY);
    resetButton=new QPushButton(ui("Reset"));resetButton->setObjectName("resetOffset");of->addRow(resetButton);
    {auto *menu=new QMenu(resetButton);resetButton->setMenu(menu);
        const std::pair<QString,OffsetReset> ways[]={{ui("Blatt an Papierkante"),OffsetReset::SheetToPaper},{ui("Blatt an Druckbereich"),OffsetReset::SheetToPrintable},
                                                     {ui("Ausdruck an Papierkante"),OffsetReset::ContentToPaper},{ui("Ausdruck an Druckbereich"),OffsetReset::ContentToPrintable}};
        for(const auto &[caption,to]:ways)menu->addAction(caption,this,[this,to]{
            PrintSettings s=settings[current];s.offset=resetOffset(pageFor(device->pageLayout(),document.sheets[current],s),document.sheets[current],to);
            s.offset=QPointF(std::round(s.offset.x()*10)/10,std::round(s.offset.y()*10)/10);apply(s);});}
    // Banner
    auto *bannerBox=new QGroupBox(ui("Bannerdruck"));auto *bf=new QFormLayout(bannerBox);left->addWidget(bannerBox);
    bannerX=new QSpinBox;bannerX->setObjectName("bannerX");bannerX->setRange(1,20);bannerY=new QSpinBox;bannerY->setObjectName("bannerY");bannerY->setRange(1,20);
    {auto *row=new QHBoxLayout;row->addWidget(new QLabel(QStringLiteral("X:")));row->addWidget(bannerX);row->addWidget(new QLabel(QStringLiteral("Y:")));row->addWidget(bannerY);bf->addRow(row);}
    overlap=mm("overlap");overlap->setRange(0,100);bf->addRow(ui("Überlapp.[mm]:"),overlap);
    // Orientation
    auto *formatBox=new QGroupBox(ui("Blattformat"));auto *fl=new QVBoxLayout(formatBox);left->addWidget(formatBox);
    automatic=new QRadioButton(ui("Automatisch"));automatic->setObjectName("automatic");portrait=new QRadioButton(ui("Hochformat"));portrait->setObjectName("portrait");
    landscape=new QRadioButton(ui("Querformat"));landscape->setObjectName("landscape");
    fl->addWidget(automatic);fl->addWidget(portrait);fl->addWidget(landscape);
    // Functions
    auto *functions=new QToolButton;functions->setObjectName("functions");functions->setText(ui("Funktionen"));functions->setPopupMode(QToolButton::InstantPopup);
    functions->setToolButtonStyle(Qt::ToolButtonTextOnly);left->addWidget(functions);
    auto *fm=new QMenu(functions);functions->setMenu(fm);
    auto page=[this](int s){return pageFor(device->pageLayout(),document.sheets[s],settings[s]);};
    fm->addAction(ui("Anpassen (Größe und Position)"),this,[this,page]{apply(fitted(page(current),document.sheets[current],settings[current]));})->setObjectName("fit");
    fm->addAction(ui("Zentrieren"),this,[this,page]{apply(centred(page(current),document.sheets[current],settings[current]));})->setObjectName("centre");
    fm->addSeparator();
    fm->addAction(ui("Alle Blätter -> Anpassen (Größe und Position)"),this,[this,page]{for(int s=0;s<settings.size()&&s<document.sheets.size();s++)settings[s]=fitted(page(s),document.sheets[s],settings[s]);showSettings();settingsChanged();});
    fm->addAction(ui("Alle Blätter -> Zentrieren"),this,[this,page]{for(int s=0;s<settings.size()&&s<document.sheets.size();s++)settings[s]=centred(page(s),document.sheets[s],settings[s]);showSettings();settingsChanged();});
    fm->addSeparator();
    auto takeOver=[this](std::function<void(PrintSettings&,const PrintSettings&)> copy){for(int s=0;s<settings.size();s++)if(s!=current)copy(settings[s],settings[current]);settingsChanged();};
    fm->addAction(ui("Für alle Blätter übernehmen -> Skalierung"),this,[takeOver]{takeOver([](PrintSettings &a,const PrintSettings &b){a.free=b.free;a.scale=b.scale;});});
    fm->addAction(ui("Für alle Blätter übernehmen -> Offset"),this,[takeOver]{takeOver([](PrintSettings &a,const PrintSettings &b){a.offset=b.offset;});});
    fm->addAction(ui("Für alle Blätter übernehmen -> Blattformat"),this,[takeOver]{takeOver([](PrintSettings &a,const PrintSettings &b){a.orientation=b.orientation;});});
    fm->addAction(ui("Für alle Blätter übernehmen -> Bannerdruck"),this,[takeOver]{takeOver([](PrintSettings &a,const PrintSettings &b){a.bannerX=b.bannerX;a.bannerY=b.bannerY;a.overlap=b.overlap;});});
    fm->addSeparator();
    fm->addAction(ui("Alle Blätter -> Reset Druckeinstellungen"),this,[this]{for(auto &s:settings)s=PrintSettings();showSettings();settingsChanged();});
    // What is printed
    auto *printBox=new QGroupBox(ui("Drucken"));auto *pr=new QVBoxLayout(printBox);left->addWidget(printBox);
    thisSheet=new QRadioButton(ui("Aktuelles Blatt"));thisSheet->setObjectName("thisSheet");thisSheet->setChecked(true);pr->addWidget(thisSheet);
    allSheets=new QRadioButton(ui("Alle Blätter"));allSheets->setObjectName("allSheets");pr->addWidget(allSheets);
    {auto *row=new QHBoxLayout;pr->addLayout(row);
        someSheets=new QRadioButton(ui("Auswahl"));someSheets->setObjectName("someSheets");row->addWidget(someSheets);
        selection=new QLineEdit;selection->setObjectName("selection");selection->setPlaceholderText(QStringLiteral("1,3"));row->addWidget(selection,1);
        auto *choose=new QToolButton;choose->setText(QStringLiteral("..."));row->addWidget(choose);
        connect(choose,&QToolButton::clicked,this,[this]{
            QStringList names;for(const auto &s:document.sheets)names<<s.name;
            SheetChoiceDialog d(ui("Druckauswahl"),names,this);if(d.exec()!=QDialog::Accepted)return;
            QStringList numbers;for(int i:d.sheets())numbers<<QString::number(i+1);selection->setText(numbers.join(u','));someSheets->setChecked(true);});}
    left->addStretch();
    {auto *row=new QHBoxLayout;left->addLayout(row);
        printButton=new QPushButton;printButton->setObjectName("print");row->addWidget(printButton);
        auto *close=new QPushButton(ui("Schließen"));close->setObjectName("close");row->addWidget(close);
        auto *help=new QPushButton(QStringLiteral("?"));help->setObjectName("help");help->setFixedWidth(32);row->addWidget(help);
        connect(help,&QPushButton::clicked,this,[this]{if(helpRequested)helpRequested();});
        connect(printButton,&QPushButton::clicked,this,[this]{print();});connect(close,&QPushButton::clicked,this,&QDialog::reject);}
    // Paper and sheets
    paper=new PaperView(this);paper->setObjectName("paper");middle->addWidget(paper,1);
    sheetBar=new QTabBar;sheetBar->setObjectName("sheets");sheetBar->setDrawBase(false);sheetBar->setExpanding(false);
    for(int i=0;i<document.sheets.size();i++)sheetBar->addTab(QString());
    sheetBar->setCurrentIndex(current);
    {auto *row=new QHBoxLayout;middle->addLayout(row);row->addWidget(sheetBar,1);
        cutCount=new QLabel;cutCount->setObjectName("cutCount");row->addWidget(cutCount);}
    connect(sheetBar,&QTabBar::currentChanged,this,[this](int i){if(!updating)setSheet(i);});
    // The context menu of the bar: the sheets, the current one ticked.
    sheetMenu=new QMenu(this);sheetBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(sheetMenu,&QMenu::aboutToShow,this,[this]{
        sheetMenu->clear();
        for(int i=0;i<document.sheets.size();i++){
            auto *a=sheetMenu->addAction(sheetBar->tabText(i),this,[this,i]{setSheet(i);});a->setCheckable(true);a->setChecked(i==current);}});
    connect(sheetBar,&QTabBar::customContextMenuRequested,this,[this](const QPoint &at){sheetMenu->popup(sheetBar->mapToGlobal(at));});
    // Every change of a control is a change of the sheet's settings.
    for(QRadioButton *r:{oneToOne,freeScale,automatic,portrait,landscape})connect(r,&QRadioButton::toggled,this,[this](bool on){if(on)settingsChanged();});
    for(QRadioButton *r:{thisSheet,allSheets,someSheets})connect(r,&QRadioButton::toggled,this,[this](bool on){if(on)settingsChanged();});
    connect(selection,&QLineEdit::textChanged,this,[this]{settingsChanged();});
    // Slider and field set the free scale together.
    auto scaleTo=[this](int percent){
        if(updating)return;
        updating=true;scaleSlider->setValue(percent);scalePercent->setValue(percent);freeScale->setChecked(true);updating=false;
        settings[current].scale=percent/100.;settingsChanged();};
    connect(scaleSlider,&QSlider::valueChanged,this,scaleTo);connect(scalePercent,&QSpinBox::valueChanged,this,scaleTo);
    for(QDoubleSpinBox *b:{offsetX,offsetY,overlap})connect(b,&QDoubleSpinBox::valueChanged,this,[this]{settingsChanged();});
    for(QSpinBox *b:{bannerX,bannerY})connect(b,&QSpinBox::valueChanged,this,[this]{settingsChanged();});
    showSettings();settingsChanged();resize(1100,760);
}
PrintPreview::~PrintPreview(){delete device;}
void PrintPreview::setSheet(int s){
    if(s<0||s>=document.sheets.size())return;
    current=s;updating=true;sheetBar->setCurrentIndex(s);updating=false;showSettings();settingsChanged();
}
void PrintPreview::apply(const PrintSettings &s){settings[current]=s;showSettings();settingsChanged();}
void PrintPreview::showSettings(){
    updating=true;const PrintSettings &s=settings[current];
    (s.free?freeScale:oneToOne)->setChecked(true);scaleSlider->setValue(int(std::lround(s.scale*100)));scalePercent->setValue(int(std::lround(s.scale*100)));
    offsetX->setValue(s.offset.x());offsetY->setValue(s.offset.y());bannerX->setValue(s.bannerX);bannerY->setValue(s.bannerY);overlap->setValue(s.overlap);
    (s.orientation==PrintSettings::Orientation::Portrait?portrait:s.orientation==PrintSettings::Orientation::Landscape?landscape:automatic)->setChecked(true);
    updating=false;
}
void PrintPreview::settingsChanged(){
    if(updating||!printButton||!cutCount)return;
    PrintSettings &s=settings[current];
    s.free=freeScale->isChecked();
    s.offset=QPointF(offsetX->value(),offsetY->value());s.bannerX=bannerX->value();s.bannerY=bannerY->value();s.overlap=overlap->value();
    s.orientation=portrait->isChecked()?PrintSettings::Orientation::Portrait:landscape->isChecked()?PrintSettings::Orientation::Landscape:PrintSettings::Orientation::Automatic;
    refreshMarks();
    // The number of pages, and the button red if a sheet would be cut.
    int pages=0;bool cut=false;
    for(int i:sheetsToPrint()){
        pages+=settings[i].bannerX*settings[i].bannerY;
        for(bool c:cutSides(pageFor(device->pageLayout(),document.sheets[i],settings[i]),document.sheets[i],settings[i]))cut|=c;
    }
    printButton->setText(ui("Drucken")+QStringLiteral(" [%1]").arg(pages));
    printButton->setStyleSheet(cut?QStringLiteral("QPushButton{color:white;background:#c62828}"):QStringLiteral("QPushButton{color:white;background:#2e7d32}"));
    if(paper)paper->update();
}
void PrintPreview::refreshMarks(){
    int count=0;
    for(int i=0;i<document.sheets.size();i++){
        bool cut=false;for(bool c:cutSides(pageFor(device->pageLayout(),document.sheets[i],settings[i]),document.sheets[i],settings[i]))cut|=c;
        sheetBar->setTabText(i,QStringLiteral("%1: %2%3").arg(i+1).arg(document.sheets[i].name,cut?QStringLiteral(" [!]"):QString()));count+=cut;
    }
    // "Blätter abgeschnitten: n", shown while there are such sheets.
    cutCount->setText(ui("Blätter abgeschnitten:")+QStringLiteral(" %1").arg(count));cutCount->setVisible(count>0);
    cutCount->setStyleSheet(QStringLiteral("QLabel{color:#c62828;font-weight:bold}"));
}
QList<int> PrintPreview::sheetsToPrint() const{
    QList<int> out;
    if(thisSheet->isChecked())return {current};
    // Spare sheets ("Reserveblatt") are skipped when several are printed, as in the reference.
    if(allSheets->isChecked()){for(int i=0;i<document.sheets.size();i++)if(!document.sheets[i].spare)out<<i;return out;}
    // "1,3,5-7"
    for(const auto &part:selection->text().split(u',',Qt::SkipEmptyParts)){
        const auto range=part.trimmed().split(u'-');bool ok1=false,ok2=false;
        const int a=range.value(0).toInt(&ok1),b=range.size()>1?range[1].toInt(&ok2):a;
        if(!ok1||(range.size()>1&&!ok2))continue;
        // Within the sheets there are, so that a large number neither takes long nor overflows.
        const int count=int(document.sheets.size()),from=std::clamp(std::min(a,b),1,count+1),to=std::clamp(std::max(a,b),0,count);
        for(int n=from;n<=to;n++)if(n>=1&&n<=document.sheets.size()&&!document.sheets[n-1].spare&&!out.contains(n-1))out<<n-1;
    }
    return out;
}
void PrintPreview::print(){
    const QList<int> sheets=sheetsToPrint();if(sheets.isEmpty())return;
    int cut=0;for(int i:sheets){bool c=false;for(bool x:cutSides(pageFor(device->pageLayout(),document.sheets[i],settings[i]),document.sheets[i],settings[i]))c|=x;cut+=c;}
    if(cut&&QMessageBox::question(this,ui("Drucken"),(cut==1?ui("Blatt ist abgeschnitten"):ui("%1 Blätter sind abgeschnitten").arg(cut))+QStringLiteral("\n\n")+ui("Möchten Sie den Ausdruck trotzdem starten?"))!=QMessageBox::Yes)return;
    if(device->outputFormat()==QPrinter::PdfFormat){
        const QString file=QFileDialog::getSaveFileName(this,ui("Drucken"),QFileInfo(fileName).completeBaseName()+QStringLiteral(".pdf"),ui("PDF-Dokument (*.pdf)"));
        if(file.isEmpty())return;device->setOutputFileName(file);
    }
    RenderOptions o;o.fileName=fileName;
    if(!printSheets(*device,document,sheets,settings,o))QMessageBox::warning(this,ui("Drucken"),ui("Der Ausdruck konnte nicht gestartet werden."));
    else accept();
}
}
