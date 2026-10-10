#include "printdialog.h"
#include "canvas.h"
#include "icons.h"
#include "language.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
namespace openloch {
double PrintDialog::correctionX=1,PrintDialog::correctionY=1;
namespace {
int round(double v){return int(std::nearbyint(v));}
// The mm values the dialog shows and accepts: one decimal, either decimal separator, leading number only.
double lenient(const QString &text){
    QString s;for(const QChar c:text.trimmed()){if(c.isDigit()||QStringLiteral("+-.,eE").contains(c))s+=c;else break;}
    return s.replace(',','.').toDouble();
}
QString mm(double v){return uiLocale().toString(v,'f',1);}
// Kacheln: copies of the board in the active view, with the gaps between them.
bool tileDialog(QWidget *parent,PrintView &v){
    QDialog d(parent);d.setWindowTitle(ui("Kacheln"));auto *layout=new QVBoxLayout(&d);
    auto section=[&](const QString &title,int count,double gap,QSpinBox *&spin,QLineEdit *&edit){
        auto *box=new QGroupBox(title);auto *form=new QFormLayout(box);spin=new QSpinBox;spin->setRange(1,10);spin->setValue(count);spin->setFixedWidth(60);
        edit=new QLineEdit(uiLocale().toString(gap/100,'f',2));form->addRow(ui("Anzahl"),spin);form->addRow(ui("Abstand [mm]"),edit);layout->addWidget(box);};
    QSpinBox *nx,*ny;QLineEdit *gx,*gy;section(ui("Horizontal"),v.tilesX,v.gapX,nx,gx);section(ui("Vertikal"),v.tilesY,v.gapY,ny,gy);
    nx->setObjectName("tilesX");ny->setObjectName("tilesY");gx->setObjectName("gapX");gy->setObjectName("gapY");
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
    if(d.exec()!=QDialog::Accepted)return false;
    v.tilesX=nx->value();v.tilesY=ny->value();v.gapX=lenient(gx->text())*100;v.gapY=lenient(gy->text())*100;return true;
}
// Korrekturfaktoren: horizontal and vertical stretching of the printout, 0.8 to 1.2.
void calibrationDialog(QWidget *parent){
    QDialog d(parent);d.setWindowTitle(ui("Korrekturfaktoren"));auto *layout=new QVBoxLayout(&d);
    auto *text=new QLabel(ui("Bei einigen Druckern können geringfügige Verzerrungen bei der Druckausgabe auftreten. Sie können hier horizontale und vertikale Korrekturfaktoren angeben. Werte größer 1 strecken, Werte kleiner 1 stauchen den Ausdruck."));
    text->setWordWrap(true);layout->addWidget(text);auto *form=new QFormLayout;layout->addLayout(form);
    auto *h=new QLineEdit(QString::number(PrintDialog::correctionX,'f',5)),*v=new QLineEdit(QString::number(PrintDialog::correctionY,'f',5));h->setObjectName("correctionX");v->setObjectName("correctionY");
    form->addRow(ui("Horizontaler Korrekturfaktor:"),h);form->addRow(ui("Vertikaler Korrekturfaktor:"),v);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&d,[&]{
        const auto x=correctionFactor(h->text()),y=correctionFactor(v->text());
        if(!x||!y){QMessageBox::warning(&d,ui("Korrekturfaktoren"),ui("Die Korrekturfaktoren müssen zwischen 0.8...1.2 liegen."));return;}
        PrintDialog::correctionX=*x;PrintDialog::correctionY=*y;d.accept();});
    d.exec();
}
}
// The preview: all sheets shown (or the one of "nur ein Blatt"), scaled to fit with a margin of 25 pixels.
class PrintDialog::Preview : public QWidget {
public:
    explicit Preview(PrintDialog *d):dialog(d){setMinimumSize(300,300);setMouseTracking(false);setAutoFillBackground(true);setObjectName("printPreview");}
    PrintDialog *dialog;int hit=-1;bool dragging=false;QPoint last;
    struct Layout {QSize shown;double k=1;QRectF box;};
    Layout layout() const{
        const auto &s=dialog->setups[dialog->board];const Paper p=dialog->paper();const QSize n=dialog->sheets();Layout l;
        l.shown=s.onlyOne?QSize(1,1):n;const int PH=usableHeight(p,s),DF=s.dataField?dataFieldHeight:0;
        const double aw=width()-50,ah=height()-50;l.k=qMax(.01,qMin(ah/qMax(1.0,double(l.shown.height()*PH+DF)),aw/qMax(1.0,double(l.shown.width()*p.width))));
        const double w=l.shown.width()*p.width*l.k,h=(l.shown.height()*PH+DF)*l.k;l.box=QRectF(width()/2.0-w/2,height()/2.0-h/2,w,h);return l;
    }
    // The sheet whose content is shown at (i, j) of the preview.
    QPoint sheetAt(int i,int j) const{const auto &s=dialog->setups[dialog->board];return s.onlyOne?sheetCell(s.sheet,dialog->sheets()):QPoint(i,j);}
    int hitTest(QPointF at) const{
        const auto &s=dialog->setups[dialog->board];const auto l=layout();const Paper p=dialog->paper();const int PH=usableHeight(p,s);const QSizeF size=dialog->boardSize(dialog->board);
        const QPoint first=sheetAt(0,0);
        for(int i=s.count-1;i>=0;i--){const auto &v=s.views[i];
            for(int ty=0;ty<v.tilesY;ty++)for(int tx=0;tx<v.tilesX;tx++){
                const QTransform t=printTransform(v,size,tx,ty)*QTransform::fromTranslate(-first.x()*p.width,-first.y()*PH)*QTransform::fromScale(l.k,l.k)*QTransform::fromTranslate(l.box.left(),l.box.top());
                if(t.mapRect(QRectF(QPointF(),size)).contains(at))return i;}}
        return -1;
    }
protected:
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);p.fillRect(rect(),palette().window());const auto &s=dialog->setups[dialog->board];const Paper pp=dialog->paper();const int PH=usableHeight(pp,s);
        const auto l=layout();const QSize n=dialog->sheets();p.setRenderHint(QPainter::Antialiasing);
        // Paper sheets with their unprintable margins, the printable areas outlined with dots.
        p.setPen(Qt::NoPen);p.setBrush(Qt::white);
        for(int j=0;j<l.shown.height();j++)for(int i=0;i<l.shown.width();i++)p.drawRect(QRectF(l.box.left()+(i*pp.width-pp.left)*l.k,l.box.top()+(j*PH-pp.top)*l.k,pp.fullWidth*l.k,pp.fullHeight*l.k));
        p.save();p.translate(l.box.topLeft());
        for(int j=0;j<l.shown.height();j++)for(int i=0;i<l.shown.width();i++){
            p.save();p.translate(i*pp.width*l.k,j*PH*l.k);
            if(dragging){ // placeholders while a view is dragged
                p.setClipRect(QRectF(0,0,pp.width*l.k,PH*l.k));const QPoint sheet=sheetAt(i,j);
                for(int k=0;k<s.count;k++){const auto &v=s.views[k];for(int ty=0;ty<v.tilesY;ty++)for(int tx=0;tx<v.tilesX;tx++){
                    p.save();p.setTransform(printTransform(v,dialog->boardSize(dialog->board),tx,ty)*QTransform::fromTranslate(-sheet.x()*pp.width,-sheet.y()*PH)*QTransform::fromScale(l.k,l.k),true);
                    p.setPen(QPen(QColor("#000080"),0));p.setBrush(Qt::NoBrush);p.drawRect(QRectF(QPointF(),dialog->boardSize(dialog->board)));p.restore();}}
            }else dialog->paintSheet(p,dialog->board,pp,sheetAt(i,j),l.k,false,{1,1});
            p.restore();
        }
        const double w=l.shown.width()*pp.width*l.k,h=l.shown.height()*PH*l.k;
        p.setPen(QPen(QColor("#c0c0c0"),0,Qt::DotLine));p.setBrush(Qt::NoBrush);p.drawRect(QRectF(0,0,w,h));
        for(int i=1;i<l.shown.width();i++)p.drawLine(QPointF(i*pp.width*l.k,0),QPointF(i*pp.width*l.k,h));
        for(int j=1;j<l.shown.height();j++)p.drawLine(QPointF(0,j*PH*l.k),QPointF(w,j*PH*l.k));
        if(s.cutMarks&&n.width()*n.height()>1){p.setPen(QPen(Qt::black,0));
            for(int j=0;j<l.shown.height();j++)for(int i=0;i<l.shown.width();i++){const QRectF r(i*pp.width*l.k,j*PH*l.k,pp.width*l.k-1,PH*l.k-1);
                for(const auto &[c,dx,dy]:{std::tuple{r.topLeft(),1,1},{r.topRight(),-1,1},{r.bottomLeft(),1,-1},{r.bottomRight(),-1,-1}}){p.drawLine(c,c+QPointF(10*dx,0));p.drawLine(c,c+QPointF(0,10*dy));}}}
        if(s.dataField)for(int i=0;i<l.shown.width();i++){p.save();p.translate(i*pp.width*l.k,(l.shown.height()-1)*PH*l.k);
            const QPoint sheet=sheetAt(i,l.shown.height()-1);dialog->paintDataField(p,dialog->board,pp,l.k,s.onlyOne?s.sheet:sheet.y()*n.width()+sheet.x()+1,n.width()*n.height());p.restore();}
        p.restore();
    }
    void mousePressEvent(QMouseEvent *e) override{if(e->button()!=Qt::LeftButton)return;hit=hitTest(e->position());dragging=true;last=e->position().toPoint();}
    void mouseMoveEvent(QMouseEvent *e) override{
        if(!dragging)return;const auto l=layout();const QPoint now=e->position().toPoint();auto &s=dialog->setups[dialog->board];
        // Moves the view under the mouse, or all views when the press was on empty paper; moving ends the centring.
        for(int i=0;i<s.count;i++)if(hit<0||hit==i){auto &v=s.views[i];
            v.posX=qBound(-31000,v.posX+round((last.x()-now.x())*100/l.k/v.scale),31000);v.posY=qBound(-31000,v.posY+round((last.y()-now.y())*100/l.k/v.scale),31000);v.centred=false;}
        last=now;update();
    }
    void mouseReleaseEvent(QMouseEvent *e) override{
        if(!dragging)return;dragging=false;const int at=hitTest(e->position());if(at>=0)dialog->active=at;dialog->syncUi();dialog->refresh();
    }
};
PrintDialog::PrintDialog(Project &p,QPrinter &pr,const QString &name,int unit,QWidget *parent):QDialog(parent),project(p),printer(pr),projectName(name),mainUnit(unit){
    setObjectName("printDialog");
    // Every board of the project with its print settings.
    if(project.boards.isEmpty())pages={project};
    else{const auto root=QJsonDocument::fromJson(project.encode()).object();for(const auto &page:root["boards"].toArray())pages.append(Project::decode(QJsonDocument(page.toObject()).toJson()));}
    for(const auto &page:pages)setups.append(printSetupOf(page));
    board=project.boards.isEmpty()?0:qBound(0,project.activeBoard,int(pages.size())-1);
    const QString printerName=printer.printerName().isEmpty()?ui("(kein Drucker, Ausgabe als PDF)"):printer.printerName();
    setWindowTitle(ui("Druckvorschau - ")+printerName);
    auto *outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);auto *columns=new QHBoxLayout;outer->addLayout(columns,1);
    // Left: views, options, scale, position.
    auto *leftColumn=new QVBoxLayout;columns->addLayout(leftColumn);
    auto *viewsBox=new QGroupBox(ui("Ansichten"));auto *viewsLayout=new QGridLayout(viewsBox);leftColumn->addWidget(viewsBox);
    count=new QSpinBox;count->setObjectName("viewCount");count->setRange(1,10);activeView=new QComboBox;activeView->setObjectName("activeView");
    viewsLayout->addWidget(new QLabel(ui("Anzahl:")),0,0);viewsLayout->addWidget(count,0,1);viewsLayout->addWidget(new QLabel(ui("Aktiv:")),1,0);viewsLayout->addWidget(activeView,1,1);
    connect(count,&QSpinBox::valueChanged,this,[this](int n){if(syncing)return;setCount(n);syncUi();refresh();});
    connect(activeView,&QComboBox::currentIndexChanged,this,[this](int i){if(syncing||i<0)return;active=i;syncUi();refresh();});
    auto *bar=new QToolBar;bar->setIconSize(QSize(16,16));viewsLayout->addWidget(bar,2,0,1,2);
    const QList<std::pair<const char*,const char*>> switchList{{"S/W-Darstellung","view-mono"},{"Platine wenden","view-flip"},{"Andere Platinenseite bearbeiten (Durchsicht)","view-through"},{"Röntgenblick","view-xray"},{"BMP-Rendering","view-bitmaps"},{"Freie Bereiche anzeigen","view-free"},{"Potenziale anzeigen","view-potentials"}};
    for(const auto &[tip,icon]:switchList){auto *b=new QToolButton;b->setCheckable(true);b->setIcon(openLochIcon(icon));b->setToolTip(ui(tip));b->setObjectName(QString("print-")+icon);bar->addWidget(b);switches.append(b);
        connect(b,&QToolButton::toggled,this,[this,i=int(switches.size())-1](bool on){if(syncing)return;auto &v=view();bool *field[]={&v.mono,&v.flip,&v.through,&v.xray,&v.bitmaps,&v.free,&v.potentials};*field[i]=on;refresh();});}
    auto *optionsBox=new QGroupBox(ui("Ansicht-Optionen"));auto *optionsLayout=new QGridLayout(optionsBox);leftColumn->addWidget(optionsBox);
    auto check=[&](QCheckBox *&box,const char *text,const char *name,bool PrintView::*field,int row){box=new QCheckBox(ui(text));box->setObjectName(name);optionsLayout->addWidget(box,row,0);
        connect(box,&QCheckBox::toggled,this,[this,field](bool on){if(syncing)return;view().*field=on;refresh();});};
    check(objects,"&Objekte && Symbole","layerObjects",&PrintView::objects,0);check(texts,"&Texte","layerTexts",&PrintView::texts,1);check(copper,"L&eiterbahnen","layerCopper",&PrintView::copper,2);
    check(solderMarks,"&Lötstellen","layerSolder",&PrintView::solderMarks,3);check(cuts,"&Trennstellen","layerCuts",&PrintView::cuts,4);check(holes,"Bohrungen","layerHoles",&PrintView::holes,5);
    check(background,"H&intergrund","layerBackground",&PrintView::background,6);check(rulers,"Li&neale","layerRulers",&PrintView::rulers,7);
    unitButton=new QToolButton;unitButton->setObjectName("rulerUnit");unitButton->setToolTip(ui("Einheit der Lineale"));optionsLayout->addWidget(unitButton,7,1);
    connect(unitButton,&QToolButton::clicked,this,[this]{view().unit=(view().unit+1)%3;syncUi();refresh();});
    auto *scaleBox=new QGroupBox(ui("Ansicht-Skalierung"));auto *scaleLayout=new QGridLayout(scaleBox);leftColumn->addWidget(scaleBox);
    original=new QRadioButton(ui("Original&größe 1:1"));original->setObjectName("scaleOriginal");zoom=new QRadioButton(ui("&Vergrößern:"));zoom->setObjectName("scaleZoom");
    scaleLabel=new QLabel("100%");scaleLabel->setAlignment(Qt::AlignRight|Qt::AlignVCenter);scale=new QScrollBar(Qt::Horizontal);scale->setObjectName("scale");scale->setRange(1,300);
    scaleLayout->addWidget(original,0,0,1,2);scaleLayout->addWidget(zoom,1,0);scaleLayout->addWidget(scaleLabel,1,1);scaleLayout->addWidget(scale,2,0,1,2);
    // A new scale keeps the board's place on the paper.
    auto setScale=[this](double next){auto &v=view();const double old=v.scale;v.scale=next;v.posX=round(old/next*v.posX);v.posY=round(old/next*v.posY);};
    connect(original,&QRadioButton::toggled,this,[this,setScale](bool on){if(syncing)return;view().original=on;setScale(on?1:scale->value()/100.0);syncUi();refresh();});
    connect(scale,&QScrollBar::valueChanged,this,[this,setScale](int value){scaleLabel->setText(QString::number(value)+"%");if(syncing||view().original)return;setScale(value/100.0);refresh();});
    auto *positionBox=new QGroupBox(ui("Ansicht-Position"));auto *positionLayout=new QGridLayout(positionBox);leftColumn->addWidget(positionBox);
    centred=new QCheckBox(ui("&mittig ausrichten"));centred->setObjectName("centred");left=new QLineEdit;left->setObjectName("positionLeft");top=new QLineEdit;top->setObjectName("positionTop");
    positionLayout->addWidget(centred,0,0,1,3);positionLayout->addWidget(new QLabel(ui("Links:")),1,0);positionLayout->addWidget(left,1,1);positionLayout->addWidget(new QLabel("mm"),1,2);
    positionLayout->addWidget(new QLabel(ui("Oben:")),2,0);positionLayout->addWidget(top,2,1);positionLayout->addWidget(new QLabel("mm"),2,2);
    connect(centred,&QCheckBox::toggled,this,[this](bool on){if(syncing)return;view().centred=on;syncUi();refresh();});
    auto position=[this]{if(syncing)return;const auto at=viewPosition(view(),paper());setViewPosition(view(),paper(),{left->text().isEmpty()?at.x():lenient(left->text()),top->text().isEmpty()?at.y():lenient(top->text())});refresh();};
    connect(left,&QLineEdit::editingFinished,this,position);connect(top,&QLineEdit::editingFinished,this,position);
    leftColumn->addStretch(1);
    // Middle: boards and the preview.
    auto *middle=new QVBoxLayout;columns->addLayout(middle,1);tabs=new QTabBar;tabs->setObjectName("printBoards");tabs->setShape(QTabBar::RoundedNorth);
    for(const auto &page:pages)tabs->addTab(page.title);tabs->setCurrentIndex(board);middle->addWidget(tabs);preview=new Preview(this);middle->addWidget(preview,1);
    preview->setToolTip(ui("Ausdruck verschieben..."));
    connect(tabs,&QTabBar::currentChanged,this,[this](int index){if(syncing||index<0)return;board=index;active=setup().count-1;syncUi();refresh();});
    // Right: paper, copies and the buttons.
    auto *rightColumn=new QVBoxLayout;columns->addLayout(rightColumn);
    auto *paperBox=new QGroupBox(ui("Papier"));auto *paperLayout=new QGridLayout(paperBox);rightColumn->addWidget(paperBox);
    portrait=new QRadioButton(ui("&Hochformat"));portrait->setObjectName("portrait");landscape=new QRadioButton(ui("&Querformat"));landscape->setObjectName("landscape");
    onlyOne=new QCheckBox(ui("n&ur ein Blatt drucken:"));onlyOne->setObjectName("onlyOne");sheetLabel=new QLabel(ui("Blatt Nr.:"));sheetNumber=new QSpinBox;sheetNumber->setObjectName("sheetNumber");sheetNumber->setRange(1,1000);
    cutMarks=new QCheckBox(ui("Sch&nittmarken drucken"));cutMarks->setObjectName("cutMarks");dataField=new QCheckBox(ui("Da&tenfeld drucken"));dataField->setObjectName("dataField");
    paperLayout->addWidget(portrait,0,0,1,2);paperLayout->addWidget(landscape,1,0,1,2);paperLayout->addWidget(onlyOne,2,0,1,2);paperLayout->addWidget(sheetLabel,3,0);paperLayout->addWidget(sheetNumber,3,1);
    paperLayout->addWidget(cutMarks,4,0,1,2);paperLayout->addWidget(dataField,5,0,1,2);
    connect(landscape,&QRadioButton::toggled,this,[this](bool on){if(syncing)return;setup().landscape=on;refresh();});
    connect(onlyOne,&QCheckBox::toggled,this,[this](bool on){if(syncing)return;setup().onlyOne=on;refresh();});
    connect(sheetNumber,&QSpinBox::valueChanged,this,[this](int value){if(syncing)return;setup().sheet=value;refresh();});
    connect(cutMarks,&QCheckBox::toggled,this,[this](bool on){if(syncing)return;setup().cutMarks=on;refresh();});
    connect(dataField,&QCheckBox::toggled,this,[this](bool on){if(syncing)return;setup().dataField=on;refresh();});
    auto *copiesBox=new QGroupBox(ui("Exemplare"));auto *copiesLayout=new QGridLayout(copiesBox);rightColumn->addWidget(copiesBox);
    copies=new QSpinBox;copies->setObjectName("copies");copies->setRange(1,999);copies->setToolTip(ui("Anzahl der zu druckenden Exemplare"));
    range=new QComboBox;range->setObjectName("printRange");range->addItems({ui("von gewählter Platine"),ui("von allen Platinen")});
    copiesLayout->addWidget(copies,0,0);copiesLayout->addWidget(new QLabel(ui("Exemplare")),0,1);copiesLayout->addWidget(range,1,0,1,2);
    connect(copies,&QSpinBox::valueChanged,this,[this]{refresh();});connect(range,&QComboBox::currentIndexChanged,this,[this]{refresh();});
    auto *buttons=new QGridLayout;rightColumn->addLayout(buttons);
    auto button=[&](const char *text,const char *name,int row,int col){auto *b=new QPushButton(ui(text));b->setObjectName(name);b->setMinimumHeight(48);buttons->addWidget(b,row,col);return b;};
    connect(button("Kal&ibrieren...","calibrate",0,0),&QPushButton::clicked,this,[this]{calibrationDialog(this);});
    connect(button("Kacheln...","tiles",0,1),&QPushButton::clicked,this,[this]{if(tileDialog(this,view())){syncUi();refresh();}});
    connect(button("S&etup...","printerSetup",1,0),&QPushButton::clicked,this,[this]{
        QPrintDialog dialog(&printer,this);dialog.setOption(QAbstractPrintDialog::PrintToFile,true);dialog.exec();
        setup().landscape=printer.pageLayout().orientation()==QPageLayout::Landscape;setWindowTitle(ui("Druckvorschau - ")+(printer.printerName().isEmpty()?ui("(kein Drucker, Ausgabe als PDF)"):printer.printerName()));syncUi();refresh();});
    connect(button("Rese&t","reset",1,1),&QPushButton::clicked,this,[this]{
        // Like the original: views 1 to 9 and the paper flags, not view 10 and not the sheet number.
        auto &s=setup();s.onlyOne=false;s.count=1;s.cutMarks=true;s.landscape=false;s.dataField=true;for(int i=0;i<9;i++)s.views[i]=defaultPrintView(i);
        copies->setValue(1);range->setCurrentIndex(0);active=0;syncUi();refresh();});
    connect(button("&Drucken","print",2,0),&QPushButton::clicked,this,[this]{
        // Without an installed printer the sheets go into a PDF file.
        if(printer.outputFormat()==QPrinter::PdfFormat||QPrinterInfo::availablePrinters().isEmpty()){
            const auto path=QFileDialog::getSaveFileName(this,ui("Als PDF drucken"),projectName+".pdf",ui("PDF (*.pdf)"));if(path.isEmpty())return;printer.setOutputFormat(QPrinter::PdfFormat);printer.setOutputFileName(path);}
        print(printer);});
    connect(button("&Schliessen","close",2,1),&QPushButton::clicked,this,&QDialog::accept);
    rightColumn->addStretch(1);
    auto *statusBar=new QStatusBar;outer->addWidget(statusBar);for(int i=0;i<4;i++){auto *label=new QLabel;label->setObjectName(QString("printStatus%1").arg(i));status.append(label);statusBar->addWidget(label,i==3?1:0);}
    setCount(setup().count);syncUi();refresh();
    resize(1000,640);setWindowState(windowState()|Qt::WindowMaximized);
}
Paper PrintDialog::paper() const{
    QPageLayout layout=printer.pageLayout();layout.setOrientation(setups[board].landscape?QPageLayout::Landscape:QPageLayout::Portrait);return paperOf(layout);
}
QSize PrintDialog::sheets() const{return sheetCount(setups[board],boardSize(board),paper());}
int PrintDialog::sheetsToPrint() const{
    int total=0;const int n=copies?copies->value():1;
    for(int i=0;i<pages.size();i++){if(range&&range->currentIndex()==0&&i!=board)continue;
        QPageLayout layout=printer.pageLayout();layout.setOrientation(setups[i].landscape?QPageLayout::Landscape:QPageLayout::Portrait);const QSize s=sheetCount(setups[i],boardSize(i),paperOf(layout));
        total+=n*(setups[i].onlyOne?1:s.width()*s.height());}
    return total;
}
void PrintDialog::setCount(int n){
    // Changing the number of views makes the last one active.
    setup().count=qBound(1,n,10);active=setup().count-1;
}
void PrintDialog::syncUi(){
    syncing=true;auto &s=setup();auto &v=view();
    count->setValue(s.count);activeView->clear();for(int i=0;i<s.count;i++)activeView->addItem(ui("Ansicht")+" "+QString::number(i+1));activeView->setCurrentIndex(active);
    const bool flags[]={v.mono,v.flip,v.through,v.xray,v.bitmaps,v.free,v.potentials};for(int i=0;i<switches.size();i++)switches[i]->setChecked(flags[i]);
    objects->setChecked(v.objects);texts->setChecked(v.texts);copper->setChecked(v.copper||v.free);solderMarks->setChecked(v.solderMarks);cuts->setChecked(v.cuts);holes->setChecked(v.holes);background->setChecked(v.background);rulers->setChecked(v.rulers);
    unitButton->setText(v.unit==0?"mm":v.unit==1?"inch":"N");
    original->setChecked(v.original);zoom->setChecked(!v.original);scale->setValue(qBound(1,round(v.scale*100),300));scaleLabel->setText(QString::number(scale->value())+"%");
    centred->setChecked(v.centred);portrait->setChecked(!s.landscape);landscape->setChecked(s.landscape);onlyOne->setChecked(s.onlyOne);sheetNumber->setValue(s.sheet);cutMarks->setChecked(s.cutMarks);dataField->setChecked(s.dataField);
    tabs->setCurrentIndex(board);syncing=false;
}
void PrintDialog::refresh(){
    auto &s=setup();auto &v=view();
    if(v.centred)centreView(v,s,boardSize(board),paper());
    const QSize n=sheets();const int total=n.width()*n.height();
    syncing=true;
    scale->setEnabled(!v.original);unitButton->setEnabled(v.rulers);background->setEnabled(!v.mono);copper->setEnabled(!v.free);
    left->setEnabled(!v.centred);top->setEnabled(!v.centred);const auto at=viewPosition(v,paper());left->setText(mm(at.x()));top->setText(mm(at.y()));
    onlyOne->setEnabled(total>1);cutMarks->setEnabled(total>1);sheetNumber->setMaximum(qMax(1,total));sheetNumber->setEnabled(s.onlyOne&&total>=2);sheetLabel->setEnabled(sheetNumber->isEnabled());
    if(s.sheet>qMax(1,total)){s.sheet=qMax(1,total);sheetNumber->setValue(s.sheet);}
    preview->setCursor(v.centred?Qt::ArrowCursor:Qt::OpenHandCursor);
    syncing=false;
    printer.setPageOrientation(s.landscape?QPageLayout::Landscape:QPageLayout::Portrait);
    const int toPrint=sheetsToPrint();
    status[0]->setText(pages[board].title);status[1]->setText(total==1?ui("1 Blatt"):QString::number(total)+ui(" Blätter"));
    status[2]->setText(toPrint==1?ui("1 Blatt zu drucken"):QString::number(toPrint)+ui(" Blätter zu drucken"));status[3]->setText(printer.printerName());
    preview->update();
}
void PrintDialog::paintSheet(QPainter &p,int index,const Paper &paper,QPoint sheet,double px,bool printing,QPointF correction) const{
    const auto &s=setups[index];const int PH=usableHeight(paper,s);const QSizeF size=boardSize(index);
    p.save();p.setClipRect(QRectF(0,0,paper.width*px,PH*px),Qt::IntersectClip);
    for(int i=0;i<s.count;i++){const auto &v=s.views[i];
        for(int ty=0;ty<v.tilesY;ty++)for(int tx=0;tx<v.tilesX;tx++){
            p.save();p.setTransform(printTransform(v,size,tx,ty)*QTransform::fromTranslate(-sheet.x()*paper.width,-sheet.y()*PH)*QTransform::fromScale(px*correction.x(),px*correction.y()),true);
            // The active view has a red frame in the preview.
            if(!printing&&index==board&&i==active){p.setPen(QPen(Qt::red,200));p.setBrush(Qt::NoBrush);p.drawRect(QRectF(QPointF(),size));}
            paintPrintBoard(p,pages[index],v,printing);p.restore();
        }
    }
    p.restore();
}
void PrintDialog::paintDataField(QPainter &p,int index,const Paper &paper,double px,int sheet,int sheets) const{
    const auto &s=setups[index];const int PH=usableHeight(paper,s);
    const auto field=openloch::dataField(projectName,pages[index].title,boardSize(index),mainUnit,printEditor(),QDateTime::currentDateTime(),sheet,sheets);
    p.save();p.scale(px,px);p.setPen(QPen(Qt::black,0));p.setBrush(Qt::NoBrush);p.drawRect(QRectF(1,PH+1,paper.width-2,paper.height-PH-2));
    // Arial with a cell height of 4 mm.
    QFont f("Arial");f.setPixelSize(100);const double cell=QFontMetricsF(f).height();p.scale(4/cell,4/cell);p.setFont(f);const double unit=cell/4;const QFontMetricsF m(f);
    for(int i=0;i<3;i++){const double y=(PH+3+5*i)*unit+m.ascent();
        if(i<field.left.size())p.drawText(QPointF(3*unit,y),field.left[i]);
        if(i<field.right.size())p.drawText(QPointF((paper.width-3)*unit-m.horizontalAdvance(field.right[i]),y),field.right[i]);}
    p.restore();
}
int PrintDialog::print(QPrinter &target){
    // The status window: printer, board and "copy / sheet"; Abbrechen stops after the current sheet.
    cancelled=false;QDialog statusWindow(this,Qt::Tool|Qt::WindowStaysOnTopHint);statusWindow.setWindowTitle(ui("Drucken..."));auto *layout=new QVBoxLayout(&statusWindow);
    auto *printerLabel=new QLabel(target.printerName().isEmpty()?target.outputFileName():target.printerName()),*boardLabel=new QLabel,*sheetLabelNow=new QLabel;
    for(auto *l:{printerLabel,boardLabel,sheetLabelNow}){l->setAlignment(Qt::AlignCenter);layout->addWidget(l);}
    auto *stop=new QPushButton(ui("Abbrechen"));layout->addWidget(stop);connect(stop,&QPushButton::clicked,this,[this]{cancelled=true;});statusWindow.show();
    target.setDocName(projectName.isEmpty()?QStringLiteral("OpenLoch"):projectName);
    QList<int> boards;if(range->currentIndex()==0)boards={board};else for(int i=0;i<pages.size();i++)boards.append(i);
    QPainter p;bool open=false,first=true;int printed=0;
    for(int copy=1;copy<=copies->value()&&!cancelled;copy++)for(const int b:boards){
        if(cancelled)break;const auto &s=setups[b];const auto orientation=s.landscape?QPageLayout::Landscape:QPageLayout::Portrait;
        // The orientation cannot change within a job: a board in the other orientation starts a new one.
        if(open&&target.pageLayout().orientation()!=orientation){p.end();open=false;}
        if(!open){target.setPageOrientation(orientation);if(!p.begin(&target))return printed;open=true;first=true;}
        boardLabel->setText(pages[b].title);const Paper paper=paperOf(target.pageLayout());const QSize n=sheetCount(s,boardSize(b),paper);const double px=target.resolution()/25.4;
        QList<QPoint> cells;if(s.onlyOne)cells={sheetCell(s.sheet,n)};else for(int row=0;row<n.height();row++)for(int col=0;col<n.width();col++)cells.append({col,row});
        for(const auto cell:cells){
            if(cancelled)break;const int number=cell.y()*n.width()+cell.x()+1;
            sheetLabelNow->setText(QString::number(copy)+ui(". Exemplar / Blatt ")+QString::number(number));QApplication::processEvents();
            if(!first&&!target.newPage())break;first=false;
            paintSheet(p,b,paper,cell,px,true,{correctionX,correctionY});
            // Cut marks: 10 mm at the corners of the printable area (above the data field), only for several sheets.
            if(s.cutMarks&&n.width()*n.height()>1){p.save();p.setPen(QPen(Qt::black,0));const QRectF r(0,0,paper.width*px-1,(usableHeight(paper,s))*px-1);const double leg=10*px;
                for(const auto &[c,dx,dy]:{std::tuple{r.topLeft(),1,1},{r.topRight(),-1,1},{r.bottomLeft(),1,-1},{r.bottomRight(),-1,-1}}){p.drawLine(c,c+QPointF(leg*dx,0));p.drawLine(c,c+QPointF(0,leg*dy));}p.restore();}
            if(s.dataField)paintDataField(p,b,paper,px,number,n.width()*n.height());
            ++printed;
        }
    }
    if(open)p.end();statusWindow.close();
    // The preview stays open and shows the current board again.
    syncUi();refresh();return printed;
}
void PrintDialog::done(int result){
    // The settings stay with each board (saved with the project), without marking it changed.
    for(int i=0;i<pages.size();i++){
        auto page=pages[i];storePrintSetup(page,setups[i]);
        if(project.boards.isEmpty()||i==project.activeBoard)project.print=page.print;
        else{auto o=project.boards[i].toObject();o["print"]=page.print;project.boards[i]=o;}
    }
    QDialog::done(result);
}
}
