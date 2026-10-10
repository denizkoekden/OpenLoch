#include "panelwizards.h"
#include "paneldialogs.h"
#include "panelgeometry.h"
#include "panelicons.h"
#include "panelrender.h"
#include "strokefont.h"
#include "language.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

namespace openloch::frontpanel {
namespace {
QDoubleSpinBox *number(double low,double high,int decimals,const QString &suffix={}){
    auto *s=new QDoubleSpinBox;s->setRange(low,high);s->setDecimals(decimals);s->setLocale(uiLocale());s->setSuffix(suffix);s->setKeyboardTracking(false);s->setAccelerated(true);return s;
}
QSpinBox *count(int low,int high){auto *s=new QSpinBox;s->setRange(low,high);s->setKeyboardTracking(false);return s;}
// A number shown as the original lists its parameters: without trailing zeros.
class PlainNumber : public QDoubleSpinBox {
public:
    PlainNumber(double low,double high){setRange(low,high);setDecimals(3);setLocale(uiLocale());setKeyboardTracking(false);setAccelerated(true);}
    QString textFromValue(double v) const override{
        QString t=locale().toString(v,'f',decimals());const QChar point=locale().decimalPoint().front();
        if(t.contains(point)){while(t.endsWith('0'))t.chop(1);if(t.endsWith(point))t.chop(1);}
        return t;
    }
};
QDialogButtonBox *okCancel(QDialog *d,const QString &ok){
    auto *box=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,d);box->button(QDialogButtonBox::Ok)->setText(ok);box->button(QDialogButtonBox::Cancel)->setText(ui("Abbrechen"));
    QObject::connect(box,&QDialogButtonBox::accepted,d,&QDialog::accept);QObject::connect(box,&QDialogButtonBox::rejected,d,&QDialog::reject);return box;
}
// The generated object, fitted into the widget on the panel colour.
class Preview : public QWidget {
public:
    std::function<Element()> build;
    Preview(){setMinimumSize(320,320);}
protected:
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);p.fillRect(rect(),QColor(0xd8,0xd8,0xd8));if(!build)return;
        const Element e=build();const QRectF b=elementBounds(e).adjusted(-3,-3,3,3);if(!b.isValid())return;
        const double s=std::min(width()/b.width(),height()/b.height());p.setRenderHint(QPainter::Antialiasing);
        p.translate(width()/2.0,height()/2.0);p.scale(s,s);p.translate(-b.center());const Document none;paintElement(p,none,e);
    }
};
}

bool scaleWizard(QWidget *parent,ScaleParameters &parameters,const std::function<void(const Element&)> &copy){
    using S=ScaleParameters;
    QDialog d(parent);d.setWindowTitle(ui("Skala erzeugen"));
    S P=parameters;bool loading=false;
    // Each style keeps its own settings while the assistant is open; a style not used yet starts with the settings it
    // had last time, as the original keeps one settings file per style.
    QHash<int,S> styles;
    auto remembered=[](S::Style s){
        const QJsonDocument json=QJsonDocument::fromJson(QSettings().value(QString("frontpanel/scaleStyle%1").arg(int(s))).toByteArray());
        S p=json.isObject()?S::fromJson(json.object()):S(s);if(p.style!=s)p=S(s);return p;
    };
    auto *layout=new QVBoxLayout(&d);
    auto *row=new QHBoxLayout;auto *style=new QComboBox;for(int i=0;i<=int(S::Sine);i++)style->addItem(scaleStyleTitle(S::Style(i)));
    row->addWidget(new QLabel(ui("Stil:")));row->addWidget(style,1);
    auto button=[&](const QString &icon,const QString &tip){auto *b=new QToolButton;b->setIcon(panelIcon(icon));b->setToolTip(tip);b->setAutoRaise(true);row->addWidget(b);return b;};
    auto *open=button("open",ui("Skaleneinstellungen laden…")),*save=button("save",ui("Skaleneinstellungen speichern…"));
    auto *reset=button("undo",ui("Auf die Grundeinstellung zurücksetzen")),*copyButton=button("copy",ui("Skala in die Zwischenablage kopieren"));
    copyButton->setEnabled(bool(copy));layout->addLayout(row);
    auto *body=new QHBoxLayout;auto *preview=new Preview;body->addWidget(preview,1);auto *tabs=new QTabWidget;tabs->setMinimumWidth(400);body->addWidget(tabs);layout->addLayout(body,1);

    // Construction: the parameter list of the style, as in the original.
    auto *grid=new QTableWidget(0,3);grid->setHorizontalHeaderLabels({ui("Parameter"),ui("Wert"),ui("Einheit")});grid->verticalHeader()->hide();
    grid->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);grid->setSelectionMode(QAbstractItemView::NoSelection);grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabs->addTab(grid,ui("&Konstruktion"));
    // Labels: one text per labelled position.
    auto *labelPage=new QWidget;auto *lv=new QVBoxLayout(labelPage);
    auto *table=new QTableWidget(0,1);table->setHorizontalHeaderLabels({ui("Text")});table->horizontalHeader()->setStretchLastSection(true);
    auto *standard=new QPushButton(ui("Standardtexte"));lv->addWidget(table,1);lv->addWidget(standard,0,Qt::AlignLeft);
    tabs->addTab(labelPage,ui("&Beschriftung"));
    // Design: pen, fill and font of the parts chosen in the list.
    auto *designPage=new QWidget;auto *dv=new QVBoxLayout(designPage);
    auto *partList=new QListWidget;partList->setSelectionMode(QAbstractItemView::ExtendedSelection);dv->addWidget(partList,1);
    auto *penBox=new QGroupBox(ui("Stift"));auto *pf=new QFormLayout(penBox);
    auto *penColour=new ColorButton;auto *penWidth=number(0,50,2," mm");auto *penStyle=new QComboBox;penStyle->addItems(penStyleNames());auto *tool=new QComboBox;tool->addItems(machiningNames());
    pf->addRow(ui("Farbe:"),penColour);pf->addRow(ui("Breite:"),penWidth);pf->addRow(ui("Linie:"),penStyle);pf->addRow(ui("Werkzeug:"),tool);dv->addWidget(penBox);
    auto *fillBox=new QGroupBox(ui("Füllung"));auto *ff=new QFormLayout(fillBox);
    auto *fillStyle=new QComboBox;fillStyle->addItems(fillChoices());auto *fillColour=new ColorButton,*fillColour2=new ColorButton;
    ff->addRow(ui("Muster:"),fillStyle);ff->addRow(ui("Farbe:"),fillColour);ff->addRow(ui("Verlaufsfarbe:"),fillColour2);dv->addWidget(fillBox);
    auto *fontBox=new QGroupBox(ui("Schrift"));auto *tf=new QFormLayout(fontBox);
    auto *font=new QFontComboBox;auto *useStroke=new QCheckBox(ui("Strichschrift"));auto *strokeFont=new QComboBox;strokeFont->setEditable(true);strokeFont->addItems(strokeFontNames());
    auto *bold=new QCheckBox(ui("Fett")),*italic=new QCheckBox(ui("Kursiv"));auto *styleRow=new QHBoxLayout;styleRow->addWidget(bold);styleRow->addWidget(italic);styleRow->addStretch();
    tf->addRow(ui("Schriftart:"),font);tf->addRow(useStroke,strokeFont);tf->addRow(styleRow);dv->addWidget(fontBox);
    tabs->addTab(designPage,ui("&Gestaltung"));

    std::function<void()> fillTexts,fillDesign;
    auto selectedParts=[&]{QList<int> out;for(auto *item:partList->selectedItems())out<<partList->row(item);std::sort(out.begin(),out.end());return out;};
    auto fillGrid=[&]{
        loading=true;const auto info=S::info(P.style);grid->setRowCount(0);grid->setRowCount(info.size());
        for(int i=0;i<info.size();i++){
            const S::Info &in=info[i];auto *label=new QTableWidgetItem(in.label);grid->setItem(i,0,label);grid->setItem(i,2,new QTableWidgetItem(in.unit));
            if(in.kind==S::Info::Flag){
                auto *box=new QCheckBox;box->setChecked(P.value(i)!=0);grid->setCellWidget(i,1,box);
                QObject::connect(box,&QCheckBox::toggled,&d,[&,i](bool on){if(loading)return;P.setValue(i,on?1:0);fillTexts();preview->update();});
            }else if(in.kind==S::Info::Count){
                auto *spin=count(int(std::min(in.low,P.value(i))),int(std::max(in.high,P.value(i))));spin->setValue(int(P.value(i)));grid->setCellWidget(i,1,spin);
                QObject::connect(spin,&QSpinBox::valueChanged,&d,[&,i](int v){if(loading)return;P.setValue(i,v);fillTexts();preview->update();});
            }else{
                auto *spin=new PlainNumber(std::min(in.low,P.value(i)),std::max(in.high,P.value(i)));spin->setValue(P.value(i));grid->setCellWidget(i,1,spin);
                QObject::connect(spin,&QDoubleSpinBox::valueChanged,&d,[&,i](double v){if(loading)return;P.setValue(i,v);preview->update();});
            }
        }
        grid->resizeColumnToContents(1);grid->setColumnWidth(1,std::max(grid->columnWidth(1),110));grid->resizeColumnToContents(2);loading=false;
    };
    fillTexts=[&]{
        loading=true;const QStringList texts=P.labelTexts();table->setRowCount(texts.size());
        QStringList numbers;for(int i=0;i<texts.size();i++){numbers<<QString::number(i);table->setItem(i,0,new QTableWidgetItem(texts[i]));}
        table->setVerticalHeaderLabels(numbers);tabs->setTabEnabled(1,!texts.isEmpty());loading=false;
    };
    // The controls show the first chosen part; changes go to every chosen part that has the property.
    fillDesign=[&]{
        const QList<int> chosen=selectedParts();bool fill=false,text=false;for(int i:chosen){fill|=P.design[i].hasFill;text|=P.design[i].hasFont;}
        penBox->setEnabled(!chosen.isEmpty());fillBox->setEnabled(fill);fontBox->setEnabled(text);if(chosen.isEmpty())return;
        const S::Design *withFill=nullptr,*withFont=nullptr;for(int i:chosen){if(!withFill&&P.design[i].hasFill)withFill=&P.design[i];if(!withFont&&P.design[i].hasFont)withFont=&P.design[i];}
        loading=true;const S::Design &first=P.design[chosen.first()];
        penColour->setColor(first.pen.color);penWidth->setValue(first.pen.width);penStyle->setCurrentIndex(int(first.pen.style));tool->setCurrentIndex(int(first.tool));
        if(withFill){fillStyle->setCurrentIndex(fillChoice(withFill->fill));fillColour->setColor(withFill->fill.color);fillColour2->setColor(withFill->fill.color2);}
        if(withFont){font->setCurrentFont(QFont(withFont->font));useStroke->setChecked(withFont->useStrokeFont);strokeFont->setCurrentText(withFont->strokeFont);
            bold->setChecked(withFont->bold);italic->setChecked(withFont->italic);}
        strokeFont->setEnabled(useStroke->isChecked());loading=false;
    };
    auto fillParts=[&]{
        loading=true;partList->clear();for(const auto &part:P.design)partList->addItem(part.name);
        if(partList->count())partList->item(0)->setSelected(true);loading=false;fillDesign();
    };
    auto showAll=[&]{loading=true;style->setCurrentIndex(int(P.style));loading=false;fillGrid();fillTexts();fillParts();preview->update();};
    auto change=[&](const std::function<void(S::Design&)> &apply,bool fill=false,bool text=false){
        if(loading)return;
        for(int i:selectedParts()){S::Design &part=P.design[i];if((fill&&!part.hasFill)||(text&&!part.hasFont))continue;apply(part);}
        preview->update();
    };
    preview->build=[&]{return scale(P,QTransform());};
    showAll();

    QObject::connect(style,&QComboBox::currentIndexChanged,&d,[&](int index){
        if(loading)return;styles[int(P.style)]=P;const S::Style s=S::Style(index);P=styles.contains(index)?styles[index]:remembered(s);showAll();
    });
    QObject::connect(partList,&QListWidget::itemSelectionChanged,&d,[&]{if(!loading)fillDesign();});
    penColour->changed=[&](const QColor &c){change([&](S::Design &p){p.pen.color=c;});};
    QObject::connect(penWidth,&QDoubleSpinBox::valueChanged,&d,[&](double w){change([&](S::Design &p){p.pen.width=w;});});
    QObject::connect(penStyle,&QComboBox::currentIndexChanged,&d,[&](int i){change([&](S::Design &p){p.pen.style=PenStyle(i);});});
    QObject::connect(tool,&QComboBox::currentIndexChanged,&d,[&](int i){change([&](S::Design &p){p.tool=Machining(i);});});
    QObject::connect(fillStyle,&QComboBox::currentIndexChanged,&d,[&](int i){change([&](S::Design &p){setFillChoice(p.fill,i);},true);});
    fillColour->changed=[&](const QColor &c){change([&](S::Design &p){p.fill.color=c;},true);};
    fillColour2->changed=[&](const QColor &c){change([&](S::Design &p){p.fill.color2=c;},true);};
    QObject::connect(font,&QFontComboBox::currentFontChanged,&d,[&](const QFont &f){change([&](S::Design &p){p.font=f.family();},false,true);});
    QObject::connect(useStroke,&QCheckBox::toggled,&d,[&](bool on){strokeFont->setEnabled(on);change([&](S::Design &p){p.useStrokeFont=on;if(on&&p.strokeFont.isEmpty())p.strokeFont=strokeFont->currentText();},false,true);});
    QObject::connect(strokeFont,&QComboBox::currentTextChanged,&d,[&](const QString &t){change([&](S::Design &p){p.strokeFont=t.trimmed();},false,true);});
    QObject::connect(bold,&QCheckBox::toggled,&d,[&](bool on){change([&](S::Design &p){p.bold=on;},false,true);});
    QObject::connect(italic,&QCheckBox::toggled,&d,[&](bool on){change([&](S::Design &p){p.italic=on;},false,true);});
    QObject::connect(table,&QTableWidget::itemChanged,&d,[&]{
        if(loading)return;P.texts.clear();for(int i=0;i<table->rowCount();i++)P.texts<<(table->item(i,0)?table->item(i,0)->text():QString());preview->update();
    });
    QObject::connect(standard,&QPushButton::clicked,&d,[&]{P.texts.clear();fillTexts();preview->update();});
    QObject::connect(reset,&QToolButton::clicked,&d,[&]{P=S(P.style);showAll();});
    QObject::connect(copyButton,&QToolButton::clicked,&d,[&]{if(copy)copy(scale(P,QTransform()));});
    QObject::connect(open,&QToolButton::clicked,&d,[&]{
        QSettings settings;const QString file=QFileDialog::getOpenFileName(&d,ui("Skaleneinstellungen laden"),settings.value("frontpanel/scaleFolder").toString(),
            ui("Skaleneinstellungen (*.scl *.SCL *.olscl)"));
        if(file.isEmpty())return;QFile f(file);
        if(!f.open(QIODevice::ReadOnly)){QMessageBox::warning(&d,ui("Skaleneinstellungen laden"),f.errorString());return;}
        const QByteArray bytes=f.read(4<<20);
        if(file.endsWith(".olscl",Qt::CaseInsensitive)){
            const QJsonDocument json=QJsonDocument::fromJson(bytes);
            if(!json.isObject()){QMessageBox::warning(&d,ui("Skaleneinstellungen laden"),ui("Die Datei enthält keine Skaleneinstellungen."));return;}
            P=S::fromJson(json.object());
        }else{
            if(!bytes.contains("[Parameter]")&&!bytes.contains("[parameter]")){QMessageBox::warning(&d,ui("Skaleneinstellungen laden"),ui("Die Datei enthält keine Skaleneinstellungen."));return;}
            P=S::fromScl(bytes);
        }
        settings.setValue("frontpanel/scaleFolder",QFileInfo(file).absolutePath());showAll();
    });
    QObject::connect(save,&QToolButton::clicked,&d,[&]{
        QSettings settings;QString file=QFileDialog::getSaveFileName(&d,ui("Skaleneinstellungen speichern"),settings.value("frontpanel/scaleFolder").toString(),ui("Skaleneinstellungen (*.scl)"));
        if(file.isEmpty())return;if(QFileInfo(file).suffix().isEmpty())file+=".SCL";
        QSaveFile f(file);if(!f.open(QIODevice::WriteOnly)||f.write(P.toScl())<0||!f.commit()){QMessageBox::warning(&d,ui("Skaleneinstellungen speichern"),f.errorString());return;}
        settings.setValue("frontpanel/scaleFolder",QFileInfo(file).absolutePath());
    });
    layout->addWidget(okCancel(&d,ui("Einfügen")));
    if(d.exec()!=QDialog::Accepted)return false;
    styles[int(P.style)]=P;QSettings settings;
    for(auto it=styles.cbegin();it!=styles.cend();++it)settings.setValue(QString("frontpanel/scaleStyle%1").arg(it.key()),QJsonDocument(it.value().toJson()).toJson(QJsonDocument::Compact));
    parameters=P;return true;
}

bool cutoutDialog(QWidget *parent,CutoutParameters &parameters){
    CutoutParameters P=parameters;bool loading=false;
    QDialog d(parent);d.setWindowTitle(ui("Frontplattenausschnitt erzeugen"));auto *layout=new QHBoxLayout(&d);
    auto *left=new QVBoxLayout;layout->addLayout(left);auto *preview=new Preview;preview->setMinimumSize(260,260);layout->addWidget(preview,1);
    const auto sizes=standardCutoutSizes();
    auto *frameBox=new QGroupBox(ui("Rahmen [mm]"));auto *fg=new QGridLayout(frameBox);
    auto *din=new QRadioButton("DIN 43700"),*own=new QRadioButton(ui("Benutzerdefiniert"));auto *dinWidth=new QComboBox,*dinHeight=new QComboBox;
    for(const auto &[frame,cut]:sizes){dinWidth->addItem(uiLocale().toString(frame));dinHeight->addItem(uiLocale().toString(frame));}
    auto *frameWidth=number(1,2000,2),*frameHeight=number(1,2000,2);auto *frameStyle=new QComboBox;frameStyle->addItems({ui("Kein"),ui("Rechteckig"),ui("Rund")});
    fg->addWidget(din,0,0);fg->addWidget(dinWidth,0,1);fg->addWidget(new QLabel("×"),0,2);fg->addWidget(dinHeight,0,3);
    fg->addWidget(own,1,0);fg->addWidget(frameWidth,1,1);fg->addWidget(new QLabel("×"),1,2);fg->addWidget(frameHeight,1,3);
    fg->addWidget(new QLabel(ui("Form:")),2,0);fg->addWidget(frameStyle,2,1,1,3);
    auto *cutBox=new QGroupBox(ui("Ausschnitt [mm]"));auto *cg=new QGridLayout(cutBox);auto *cutWidth=number(0.1,2000,2),*cutHeight=number(0.1,2000,2);
    auto *cutStyle=new QComboBox;cutStyle->addItems({ui("Rechteckig"),ui("Rund")});auto *tool=number(0,10,2," mm");tool->setSingleStep(0.1);
    cg->addWidget(cutWidth,0,0);cg->addWidget(new QLabel("×"),0,1);cg->addWidget(cutHeight,0,2);cg->addWidget(new QLabel(ui("Form:")),1,0);cg->addWidget(cutStyle,1,1,1,2);
    cg->addWidget(new QLabel(ui("Fräserbreite:")),2,0);cg->addWidget(tool,2,1,1,2);
    auto *holeBox=new QGroupBox(ui("Montagelöcher"));auto *hg=new QGridLayout(holeBox);
    auto *topBottom=new QCheckBox(ui("oben / unten:")),*sides=new QCheckBox(ui("links / rechts:"));
    auto *topBottomDistance=number(0,2000,2),*sidesDistance=number(0,2000,2),*topBottomDiameter=number(0.1,10,2),*sidesDiameter=number(0.1,10,2);
    hg->addWidget(new QLabel(ui("Abstand")),0,1);hg->addWidget(new QLabel(ui("Durchmesser")),0,2);
    hg->addWidget(topBottom,1,0);hg->addWidget(topBottomDistance,1,1);hg->addWidget(topBottomDiameter,1,2);hg->addWidget(sides,2,0);hg->addWidget(sidesDistance,2,1);hg->addWidget(sidesDiameter,2,2);
    auto *nameBox=new QGroupBox(ui("Name"));auto *nl=new QVBoxLayout(nameBox);auto *name=new QLineEdit;nl->addWidget(name);
    auto *files=new QHBoxLayout;auto *load=new QPushButton(ui("Laden…")),*store=new QPushButton(ui("Speichern…"));files->addWidget(load);files->addWidget(store);files->addStretch();
    left->addWidget(frameBox);left->addWidget(cutBox);left->addWidget(holeBox);left->addWidget(nameBox);left->addLayout(files);left->addStretch();left->addWidget(okCancel(&d,ui("OK")));

    auto indexOf=[&](double frame){int best=0;for(int i=0;i<sizes.size();i++)if(std::abs(sizes[i].first-frame)<std::abs(sizes[best].first-frame))best=i;return best;};
    auto write=[&]{
        loading=true;(P.din?din:own)->setChecked(true);dinWidth->setCurrentIndex(indexOf(P.frameWidth));dinHeight->setCurrentIndex(indexOf(P.frameHeight));
        frameWidth->setValue(P.frameWidth);frameHeight->setValue(P.frameHeight);frameStyle->setCurrentIndex(int(P.frame));
        cutWidth->setValue(P.cutWidth);cutHeight->setValue(P.cutHeight);cutStyle->setCurrentIndex(P.cut==CutoutParameters::Round?1:0);tool->setValue(P.tool);
        topBottom->setChecked(P.holesTopBottom);sides->setChecked(P.holesSides);topBottomDistance->setValue(P.holesTopBottomDistance);sidesDistance->setValue(P.holesSidesDistance);
        topBottomDiameter->setValue(P.holesTopBottomDiameter);sidesDiameter->setValue(P.holesSidesDiameter);name->setText(P.name);loading=false;
    };
    auto refresh=[&]{
        // DIN instruments: the opening follows from the frame, always rectangular and without holes.
        const bool standardSize=P.din;dinWidth->setEnabled(standardSize);dinHeight->setEnabled(standardSize);frameWidth->setEnabled(!standardSize);frameHeight->setEnabled(!standardSize);
        frameStyle->setEnabled(!standardSize);cutWidth->setEnabled(!standardSize);cutHeight->setEnabled(!standardSize);cutStyle->setEnabled(!standardSize);holeBox->setEnabled(!standardSize);
        if(standardSize){loading=true;cutWidth->setValue(standardCutoutFor(P.frameWidth));cutHeight->setValue(standardCutoutFor(P.frameHeight));frameWidth->setValue(P.frameWidth);frameHeight->setValue(P.frameHeight);loading=false;}
        topBottomDistance->setEnabled(P.holesTopBottom);topBottomDiameter->setEnabled(P.holesTopBottom);sidesDistance->setEnabled(P.holesSides);sidesDiameter->setEnabled(P.holesSides);
        preview->update();
    };
    auto read=[&]{
        if(loading)return;
        P.din=din->isChecked();
        if(P.din){P.frameWidth=sizes[dinWidth->currentIndex()].first;P.frameHeight=sizes[dinHeight->currentIndex()].first;P.frame=CutoutParameters::Rectangular;P.cut=CutoutParameters::Rectangular;
            P.cutWidth=standardCutoutFor(P.frameWidth);P.cutHeight=standardCutoutFor(P.frameHeight);}
        else{P.frameWidth=frameWidth->value();P.frameHeight=frameHeight->value();P.frame=CutoutParameters::Shape(frameStyle->currentIndex());P.cutWidth=cutWidth->value();P.cutHeight=cutHeight->value();
            P.cut=cutStyle->currentIndex()==1?CutoutParameters::Round:CutoutParameters::Rectangular;}
        P.tool=tool->value();P.holesTopBottom=topBottom->isChecked();P.holesSides=sides->isChecked();P.holesTopBottomDistance=topBottomDistance->value();P.holesSidesDistance=sidesDistance->value();
        P.holesTopBottomDiameter=topBottomDiameter->value();P.holesSidesDiameter=sidesDiameter->value();P.name=name->text();
        refresh();
    };
    preview->build=[&]{return cutout(P,QTransform());};
    write();refresh();
    for(auto *w:{frameWidth,frameHeight,cutWidth,cutHeight,tool,topBottomDistance,sidesDistance,topBottomDiameter,sidesDiameter})QObject::connect(w,&QDoubleSpinBox::valueChanged,&d,read);
    for(auto *w:{dinWidth,dinHeight,frameStyle,cutStyle})QObject::connect(w,&QComboBox::currentIndexChanged,&d,read);
    for(auto *w:{static_cast<QAbstractButton*>(din),static_cast<QAbstractButton*>(own),static_cast<QAbstractButton*>(topBottom),static_cast<QAbstractButton*>(sides)})QObject::connect(w,&QAbstractButton::toggled,&d,read);
    QObject::connect(name,&QLineEdit::textChanged,&d,read);
    QObject::connect(load,&QPushButton::clicked,&d,[&]{
        QSettings settings;const QString file=QFileDialog::getOpenFileName(&d,ui("Ausschnitt laden"),settings.value("frontpanel/cutFolder").toString(),ui("Ausschnitte (*.cut *.CUT)"));if(file.isEmpty())return;
        QFile f(file);if(!f.open(QIODevice::ReadOnly)){QMessageBox::warning(&d,ui("Ausschnitt laden"),f.errorString());return;}
        settings.setValue("frontpanel/cutFolder",QFileInfo(file).absolutePath());P=CutoutParameters::fromCut(f.read(64*1024));if(P.name.isEmpty())P.name=QFileInfo(file).completeBaseName();write();refresh();
    });
    QObject::connect(store,&QPushButton::clicked,&d,[&]{
        read();QSettings settings;QString file=QFileDialog::getSaveFileName(&d,ui("Ausschnitt speichern"),QDir(settings.value("frontpanel/cutFolder").toString()).filePath(P.name),ui("Ausschnitte (*.cut *.CUT)"));
        if(file.isEmpty())return;if(!file.endsWith(".cut",Qt::CaseInsensitive))file+=".CUT";
        QSaveFile f(file);if(!f.open(QIODevice::WriteOnly)||f.write(P.toCut())<0||!f.commit()){QMessageBox::warning(&d,ui("Ausschnitt speichern"),f.errorString());return;}
        settings.setValue("frontpanel/cutFolder",QFileInfo(file).absolutePath());
    });
    if(d.exec()!=QDialog::Accepted)return false;
    read();parameters=P;return true;
}
}
