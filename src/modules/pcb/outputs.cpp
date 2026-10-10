#include "outputs.h"
#include "progress.h"
#include "language.h"
#include "legacy_reader.h"
#include <QAbstractItemModel>
#include <QMouseEvent>
#include <QPainter>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QVBoxLayout>
#include <functional>

namespace openloch::pcb {
namespace {
QDoubleSpinBox *spin(double value,double low,double high,int decimals,const QString &suffix=QStringLiteral(" mm")){
    auto *box=new QDoubleSpinBox;box->setRange(low,high);box->setDecimals(decimals);box->setSuffix(suffix);box->setValue(value);box->setKeyboardTracking(false);
    box->setLocale(uiLocale());return box;
}
}

// --- Gerber
GerberDialog::GerberDialog(const Board &b,const QString &base,const QString &folder,const GerberSettings &s,QWidget *parent)
    :QDialog(parent),board(b),current(s),directory(folder){
    setWindowTitle(ui("Gerber-Export"));
    auto *layout=new QHBoxLayout(this);auto *left=new QVBoxLayout;auto *right=new QVBoxLayout;layout->addLayout(left);layout->addLayout(right,1);
    // The outputs with their endings and the resulting file names; those with something on them are chosen.
    auto *files=new QGroupBox(ui("Layer"));auto *grid=new QGridLayout(files);left->addWidget(files);
    name=new QLineEdit(base);grid->addWidget(new QLabel(ui("Dateiname")),0,0);grid->addWidget(name,0,1,1,2);
    grid->addWidget(new QLabel(ui("Dateiendung")),1,1);
    int row=2;
    for(int o:gerberOutputs()){
        auto *box=new QCheckBox(gerberOutputName(o));box->setChecked(gerberOutputUsed(b,o));
        auto *suffix=new QLineEdit(gerberSuffix(o));suffix->setMaximumWidth(80);auto *file=new QLabel;file->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if((o==Inner1||o==Inner2)&&!b.multilayer){box->setChecked(false);box->setEnabled(false);}
        grid->addWidget(box,row,0);grid->addWidget(suffix,row,1);grid->addWidget(file,row,2);row++;
        outputs[o]=box;suffixes[o]=suffix;names[o]=file;
        connect(box,&QCheckBox::toggled,this,[this]{changed();});connect(suffix,&QLineEdit::textChanged,this,[this]{changed();});
    }
    connect(name,&QLineEdit::textChanged,this,[this]{changed();});
    // The endings back to the usual ones.
    {auto *reset=new QPushButton(ui("Übliche Endungen"));reset->setObjectName("standardSuffixes");grid->addWidget(reset,row,1,1,2);
        connect(reset,&QPushButton::clicked,this,[this]{for(auto it=suffixes.begin();it!=suffixes.end();++it)it.value()->setText(gerberSuffix(it.key()));});}
    auto check=[this](QBoxLayout *into,const QString &text,bool *target){
        auto *c=new QCheckBox(text);c->setChecked(*target);into->addWidget(c);connect(c,&QCheckBox::toggled,this,[this,target](bool on){*target=on;changed();});return c;
    };
    auto *options=new QGroupBox(ui("Optionen"));auto *ov=new QVBoxLayout(options);left->addWidget(options);
    check(ov,ui("Ausgabe spiegeln"),&current.mirrored);check(ov,ui("Ausgabe mit Rahmen (Arbeitsfläche)"),&current.frame);
    check(ov,ui("Bohrungen freistanzen"),&current.clearHoles);punch=check(ov,ui("Bohrungen als Körnung (0,15 mm)"),&current.punchMarks);
    left->addStretch();
    // Mask openings per kind of element, the SMD mask's offset, inverted output.
    auto offset=[this](QGridLayout *g,int row,const QString &text,bool *on,double *value){
        auto *c=new QCheckBox(text);auto *v=spin(*value,-2,2,3);c->setChecked(*on);v->setEnabled(*on);g->addWidget(c,row,0);g->addWidget(v,row,1);
        connect(c,&QCheckBox::toggled,this,[this,on,v](bool x){*on=x;v->setEnabled(x);changed();});
        connect(v,&QDoubleSpinBox::valueChanged,this,[this,value](double x){*value=x;changed();});
    };
    auto *mask=new QGroupBox(ui("Offset für Lötstoppmaske"));auto *mg=new QGridLayout(mask);right->addWidget(mask);maskBox=mask;
    offset(mg,0,ui("Lötaugen"),&current.maskPads,&current.padOffset);offset(mg,1,ui("SMD-Pads"),&current.maskSmd,&current.smdOffset);
    offset(mg,2,ui("Sonstige Elemente"),&current.maskOther,&current.otherOffset);
    {auto *row=new QVBoxLayout;check(row,ui("Invertierte Ausgabe"),&current.maskInverted);mg->addLayout(row,3,0,1,2);}
    auto *paste=new QGroupBox(ui("Offset für SMD-Maske"));auto *pg=new QGridLayout(paste);right->addWidget(paste);pasteBox=paste;
    {auto *v=spin(current.pasteOffset,-2,2,3);pg->addWidget(new QLabel(ui("SMD-Pads")),0,0);pg->addWidget(v,0,1);
        connect(v,&QDoubleSpinBox::valueChanged,this,[this](double x){current.pasteOffset=x;changed();});
        auto *row=new QVBoxLayout;check(row,ui("Invertierte Ausgabe"),&current.pasteInverted);pg->addLayout(row,1,0,1,2);}
    // The folder and the log of the written files.
    auto *out=new QGroupBox(ui("Ausgabeverzeichnis"));auto *outv=new QVBoxLayout(out);right->addWidget(out,1);
    folderLabel=new QLabel;folderLabel->setWordWrap(true);folderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *change=new QPushButton(ui("Ändern…"));auto *fr=new QHBoxLayout;fr->addWidget(folderLabel,1);fr->addWidget(change);outv->addLayout(fr);
    logList=new QListWidget;logList->setObjectName("gerberLog");outv->addWidget(logList,1);
    auto *buttons=new QHBoxLayout;right->addLayout(buttons);
    auto *create=new QPushButton(ui("Gerberdateien erstellen"));auto *close=new QPushButton(ui("Schließen"));
    buttons->addStretch();buttons->addWidget(create);buttons->addWidget(close);
    connect(change,&QPushButton::clicked,this,[this]{const auto d=QFileDialog::getExistingDirectory(this,ui("Ausgabeverzeichnis"),directory);if(!d.isEmpty())setFolder(d);});
    connect(create,&QPushButton::clicked,this,[this]{
        int existing=0;for(int o:chosen())existing+=QFileInfo::exists(QDir(directory).filePath(fileName(o)));
        if(existing&&QMessageBox::question(this,ui("Gerber-Export"),ui("%1 der Dateien gibt es schon. Ersetzen?").arg(existing))!=QMessageBox::Yes)return;
        writeFiles();
    });
    connect(close,&QPushButton::clicked,this,&QDialog::accept);
    setFolder(folder);changed();
}
QList<int> GerberDialog::chosen() const{
    QList<int> out;for(int o:gerberOutputs())if(outputs[o]->isEnabled()&&outputs[o]->isChecked())out.append(o);return out;
}
void GerberDialog::choose(int output,bool on){if(outputs.contains(output))outputs[output]->setChecked(on);}
QString GerberDialog::fileName(int output) const{return name->text()+suffixes.value(output)->text();}
void GerberDialog::setFolder(const QString &folder){directory=folder;folderLabel->setText(QDir::toNativeSeparators(folder));}
void GerberDialog::changed(){
    for(int o:gerberOutputs())names[o]->setText(fileName(o));
    const auto list=chosen();
    maskBox->setEnabled(list.contains(SolderMaskTop)||list.contains(SolderMaskBottom));
    pasteBox->setEnabled(list.contains(PasteMaskTop)||list.contains(PasteMaskBottom));
    punch->setEnabled(current.clearHoles);
}
int GerberDialog::writeFiles(){
    logList->clear();int written=0;
    // A file after the other, with the progress window of long work.
    ProgressWindow progress(this,ui("Gerber-Dateien werden geschrieben …"));const auto files=chosen();int done=0;
    for(int o:files){
        progress.report()(done++,int(files.size()));
        const QByteArray data=gerber(board,o,current);QSaveFile f(QDir(directory).filePath(fileName(o)));
        if(f.open(QIODevice::WriteOnly)&&f.write(data)==data.size()&&f.commit()){
            written++;logList->addItem(ui("%1: %2 geschrieben (%3 Bytes)").arg(gerberOutputName(o),fileName(o)).arg(data.size()));
        }else logList->addItem(ui("%1: %2 konnte nicht geschrieben werden").arg(gerberOutputName(o),fileName(o)));
    }
    if(!written)logList->addItem(ui("Keine Datei geschrieben"));
    return written;
}
QStringList GerberDialog::log() const{QStringList out;for(int i=0;i<logList->count();i++)out.append(logList->item(i)->text());return out;}

// --- Excellon
DrillDialog::DrillDialog(const Board &b,const QString &base,const DrillSettings &s,QWidget *parent):QDialog(parent),current(s),file(base+QStringLiteral(".drl")){
    setWindowTitle(ui("Bohrdaten (Excellon)"));auto *v=new QVBoxLayout(this);
    auto radios=[this](QBoxLayout *into,const QStringList &labels,int checked,const std::function<void(int)> &pick){
        auto *group=new QButtonGroup(this);
        for(int k=0;k<labels.size();k++){auto *r=new QRadioButton(labels[k]);r->setChecked(k==checked);group->addButton(r,k);into->addWidget(r);}
        connect(group,&QButtonGroup::idClicked,this,[this,pick](int k){pick(k);changed();});return group;
    };
    auto *which=new QGroupBox(ui("Auswahl"));auto *wv=new QVBoxLayout(which);v->addWidget(which);
    radios(wv,{ui("Alle Bohrungen (%1)").arg(drillHoles(b,0).size()),ui("Nur Durchkontaktierungen (%1)").arg(drillHoles(b,1).size()),
               ui("Nur einfache Bohrungen (%1)").arg(drillHoles(b,2).size())},current.holes,[this](int k){current.holes=k;});
    auto *side=new QGroupBox(ui("Bohren von"));auto *sv=new QVBoxLayout(side);v->addWidget(side);
    radios(sv,{ui("Oberseite"),ui("Unterseite (gespiegelt wie Gerber)"),ui("Unterseite (gespiegelt um die Mitte der Bohrungen, für HPGL)")},current.fromBelow?(current.mirrorHoles?2:1):0,
           [this](int k){current.fromBelow=k>0;current.mirrorHoles=k==2;});
    auto *sorted=new QCheckBox(ui("Bohrungen sortieren"));sorted->setChecked(current.sorted);sv->addWidget(sorted);
    connect(sorted,&QCheckBox::toggled,this,[this](bool on){current.sorted=on;changed();});
    auto *format=new QGroupBox(ui("Format"));auto *fv=new QVBoxLayout(format);v->addWidget(format);
    auto *units=new QHBoxLayout;fv->addLayout(units);
    auto *integers=new QSpinBox;integers->setRange(1,6);integers->setValue(current.integerDigits);
    auto *decimals=new QSpinBox;decimals->setRange(0,6);decimals->setValue(current.decimalDigits);
    radios(units,{ui("Zoll"),ui("mm")},current.metric?1:0,[this,integers,decimals](int k){
        // The usual digits of the unit: 2.4 in inches, 3.3 in millimetres.
        current.metric=k==1;integers->setValue(current.metric?3:2);decimals->setValue(current.metric?3:4);});
    auto *digits=new QHBoxLayout;fv->addLayout(digits);
    digits->addWidget(new QLabel(ui("Vorkommastellen")));digits->addWidget(integers);digits->addWidget(new QLabel(ui("Nachkommastellen")));digits->addWidget(decimals);digits->addStretch();
    connect(integers,&QSpinBox::valueChanged,this,[this](int n){current.integerDigits=n;changed();});
    connect(decimals,&QSpinBox::valueChanged,this,[this](int n){current.decimalDigits=n;changed();});
    auto *zeros=new QCheckBox(ui("Führende Nullen entfernen"));zeros->setChecked(current.suppressLeadingZeros);fv->addWidget(zeros);
    auto *point=new QCheckBox(ui("Mit Dezimalpunkt"));point->setChecked(current.decimalPoint);fv->addWidget(point);
    connect(zeros,&QCheckBox::toggled,this,[this](bool on){current.suppressLeadingZeros=on;changed();});
    connect(point,&QCheckBox::toggled,this,[this,zeros](bool on){current.decimalPoint=on;zeros->setEnabled(!on);changed();});
    zeros->setEnabled(!current.decimalPoint);
    example=new QLabel;fv->addWidget(example);
    // Special options for machines that read Excellon their own way.
    auto *special=new QGroupBox(ui("Für besondere Maschinen"));auto *spv=new QVBoxLayout(special);v->addWidget(special);
    auto option=[&](const QString &text,const char *name,bool *target){auto *box=new QCheckBox(text);box->setObjectName(name);box->setChecked(*target);spv->addWidget(box);
        connect(box,&QCheckBox::toggled,this,[this,target](bool on){*target=on;changed();});};
    option(ui("Einheit als M71/M72 statt METRIC/INCH"),"m71",&current.m71);option(ui("Ohne G90"),"noG90",&current.noG90);option(ui("Ohne Kommentarzeilen"),"noComments",&current.noComments);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);v->addWidget(buttons);
    buttons->button(QDialogButtonBox::Save)->setText(ui("Speichern…"));
    connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    changed();
}
void DrillDialog::changed(){
    const double value=2.134;
    example->setText(ui("Beispiel: %1 steht für %2 %3").arg(QString::fromLatin1(excellonNumber(value,current)),uiLocale().toString(value,'f',3),current.metric?ui("mm"):ui("Zoll")));
}

// --- Isolation milling
MillingDialog::MillingDialog(const Board &b,const QList<int> &chosen,const MillingSettings &s,QWidget *parent):QDialog(parent),board(b),selection(chosen),current(s){
    setWindowTitle(ui("Isolationsfräsen"));
    auto *layout=new QHBoxLayout(this);auto *left=new QVBoxLayout;auto *middle=new QVBoxLayout;auto *right=new QVBoxLayout;
    layout->addLayout(left);layout->addLayout(middle);layout->addLayout(right,1);
    auto check=[this](const QString &text,bool *target){
        auto *c=new QCheckBox(text);c->setChecked(*target);connect(c,&QCheckBox::toggled,this,[this,target](bool on){*target=on;changed();});return c;
    };
    auto combo=[this](const QStringList &items,int *target){
        auto *c=new QComboBox;c->addItems(items);c->setCurrentIndex(std::clamp(*target,0,int(items.size())-1));
        connect(c,&QComboBox::currentIndexChanged,this,[this,target](int k){*target=k;changed();});return c;
    };
    auto number=[this](double low,double high,double *target,int decimals=2,const QString &suffix=QStringLiteral(" mm")){
        auto *v=spin(*target,low,high,decimals,suffix);connect(v,&QDoubleSpinBox::valueChanged,this,[this,target](double x){*target=x;changed();});return v;
    };
    // The copper sides with their mirroring and centre punches; the passes.
    auto *iso=new QGroupBox(ui("Isolationsfräsen"));auto *ig=new QGridLayout(iso);left->addWidget(iso);
    ig->addWidget(new QLabel(ui("Fräserbreite")),0,0);ig->addWidget(number(.01,10,&current.toolWidth,3),0,1,1,2);
    const QStringList mirrors{ui("nicht spiegeln"),ui("horizontal spiegeln"),ui("vertikal spiegeln")};
    ig->addWidget(check(ui("K1 (oben)"),&current.top),1,0);ig->addWidget(combo(mirrors,&current.topMirror),1,1);ig->addWidget(check(ui("Bohrungen ankörnen"),&current.punchTop),1,2);
    ig->addWidget(check(ui("K2 (unten)"),&current.bottom),2,0);ig->addWidget(combo(mirrors,&current.bottomMirror),2,1);ig->addWidget(check(ui("Bohrungen ankörnen"),&current.punchBottom),2,2);
    {auto *passes=new QSpinBox;passes->setRange(1,20);passes->setValue(current.passes);connect(passes,&QSpinBox::valueChanged,this,[this](int n){current.passes=n;changed();});
        ig->addWidget(new QLabel(ui("Fräsbahnen")),3,0);ig->addWidget(passes,3,1);}
    {auto *share=number(0,99,&current.overlap,0);share->setSuffix(QStringLiteral(" %"));share->setObjectName("overlap");
        ig->addWidget(new QLabel(ui("Überlappung der Fräsbahnen")),4,0);ig->addWidget(share,4,1);}
    auto *drills=new QGroupBox(ui("Bohrungen"));auto *dv=new QVBoxLayout(drills);left->addWidget(drills);
    dv->addWidget(combo({ui("keine Bohrungen"),ui("von oben bohren"),ui("von unten bohren")},&current.drillSide));
    {auto *group=new QButtonGroup(this);const QStringList modes{ui("Als Kreise fräsen (CI)"),ui("Bohren (PD), ein Werkzeug für alle"),
                                                                    ui("Bohren (PD), ein Werkzeug je Durchmesser")};
        for(int k=0;k<modes.size();k++){auto *r=new QRadioButton(modes[k]);r->setChecked(current.drillMode==k);group->addButton(r,k);dv->addWidget(r);}
        connect(group,&QButtonGroup::idClicked,this,[this](int k){current.drillMode=k;changed();});}
    {auto *row=new QHBoxLayout;row->addWidget(new QLabel(ui("Fräserbreite für Bohrungen")));row->addWidget(number(.05,10,&current.drillToolWidth,3));dv->addLayout(row);}
    auto *outline=new QGroupBox(ui("Umriss"));auto *ov=new QVBoxLayout(outline);left->addWidget(outline);
    ov->addWidget(combo({ui("nicht fräsen"),ui("von oben fräsen"),ui("von unten fräsen")},&current.contourSide));
    left->addStretch();
    // Registration holes off the working area: at the corners and the middles of the sides, laid out as they lie.
    auto *registration=new QGroupBox(ui("Passbohrungen"));auto *rg=new QGridLayout(registration);middle->addWidget(registration);
    {const QStringList places{ui("oben links"),ui("oben rechts"),ui("unten links"),ui("unten rechts"),ui("oben Mitte"),ui("rechts Mitte"),ui("unten Mitte"),ui("links Mitte")};
        const int rows[]={0,0,2,2,0,1,2,1},columns[]={0,2,0,2,1,2,1,0};
        for(int k=0;k<8;k++){auto *c=new QCheckBox(places[k]);c->setObjectName(QStringLiteral("registration-%1").arg(k));c->setChecked(current.registrationCorners&(1<<k));rg->addWidget(c,rows[k],columns[k]);
            connect(c,&QCheckBox::toggled,this,[this,k](bool on){current.registrationCorners=on?current.registrationCorners|(1<<k):current.registrationCorners&~(1<<k);changed();});}}
    rg->addWidget(new QLabel(ui("Abstand zur Platine")),3,0);rg->addWidget(number(0,100,&current.registrationDistance),3,1,1,2);
    rg->addWidget(new QLabel(ui("Durchmesser")),4,0);rg->addWidget(number(.1,10,&current.registrationDiameter),4,1,1,2);
    auto *texts=new QGroupBox(ui("Texte"));auto *tf=new QFormLayout(texts);middle->addWidget(texts);
    const QStringList kinds{ui("als Kontur umfräsen"),ui("entlang der Striche fräsen")};
    tf->addRow(ui("Texte"),combo(kinds,&current.texts));
    {auto *c=combo(kinds,&current.selectedTexts);bool any=false;for(int i:selection)any|=i>=0&&i<b.elements.size()&&b.elements[i].type==ElementType::Text;c->setEnabled(any);tf->addRow(ui("markierte Texte"),c);}
    auto *options=new QGroupBox(ui("Optionen"));auto *opv=new QVBoxLayout(options);middle->addWidget(options);
    {auto *c=check(ui("Nur die markierten Elemente"),&current.onlySelected);c->setEnabled(!selection.isEmpty());if(selection.isEmpty())c->setChecked(false);opv->addWidget(c);}
    opv->addWidget(check(ui("Beim Bohren kurz vorschieben"),&current.minimalFeed));
    {int scale=current.roundedScale?1:0;auto *c=new QComboBox;c->addItems({ui("0,0254 mm pro HPGL-Einheit"),ui("0,025 mm pro HPGL-Einheit")});c->setCurrentIndex(scale);
        connect(c,&QComboBox::currentIndexChanged,this,[this](int k){current.roundedScale=k==1;changed();});opv->addWidget(c);}
    middle->addStretch();
    // The job list in milling order; dragging changes the order.
    auto *list=new QGroupBox(ui("Jobliste"));auto *lv=new QVBoxLayout(list);right->addWidget(list,1);
    jobList=new QListWidget;jobList->setObjectName("millingJobs");jobList->setDragDropMode(QAbstractItemView::InternalMove);lv->addWidget(jobList,1);
    lv->addWidget(check(ui("Jobliste zusätzlich als Textdatei"),&current.jobListFile));
    lv->addWidget(check(ui("Eine Datei je Job"),&current.separateFiles));
    lv->addWidget(check(ui("Passbohrungen in jede Datei (als Nullpunkt)"),&current.registrationEveryFile));
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);right->addWidget(buttons);
    buttons->button(QDialogButtonBox::Save)->setText(ui("Speichern…"));
    connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    changed();
}
void MillingDialog::changed(){
    if(!jobList)return;jobList->clear();
    for(const auto &j:millingJobs(board,current,selection,false))jobList->addItem(j.name);
}
QStringList MillingDialog::jobNames() const{QStringList out;for(int i=0;i<jobList->count();i++)out.append(jobList->item(i)->text());return out;}
QList<MillingJob> MillingDialog::jobs(const std::function<void(int,int)> &progress) const{
    const auto all=millingJobs(board,current,selection,true,progress);QList<MillingJob> out;
    for(const auto &name:jobNames())for(const auto &j:all)if(j.name==name){out.append(j);break;}
    return out;
}

// --- Gerber import
// The preview: the layers read so far in their colours, bottom first, the drill holes as rings.
class GerberPreview : public QWidget {
public:
    const QMap<int,GerberData> *layers=nullptr;const QList<DrillHit> *holes=nullptr;
    std::function<void()> clicked;
    GerberPreview(){setMinimumSize(420,320);setCursor(Qt::PointingHandCursor);setToolTip(ui("Klick zeigt die Vorschau groß"));}
protected:
    void mousePressEvent(QMouseEvent *) override{if(clicked)clicked();}
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);p.fillRect(rect(),Qt::black);if(!layers)return;
        QRectF all;for(const auto &d:*layers)all=all.united(d.bounds());for(const auto &h:*holes)all=all.united(QRectF(h.at,QSizeF(0,0)));
        if(all.isEmpty()){p.setPen(QColor(128,128,128));p.drawText(rect(),Qt::AlignCenter,ui("Keine Datei gewählt"));return;}
        const double scale=std::min((width()-20)/std::max(.1,all.width()),(height()-20)/std::max(.1,all.height()));
        p.setRenderHint(QPainter::Antialiasing);p.translate(width()/2.0,height()/2.0);p.scale(scale,-scale);p.translate(-all.center());
        static const QMap<int,QColor> colours{{CopperBottom,QColor(0,186,1)},{Inner2,QColor(238,182,98)},{Inner1,QColor(194,124,21)},{CopperTop,QColor(29,106,249)},
                                              {SilkBottom,QColor(225,215,4)},{SilkTop,QColor(255,0,0)},{Outline,QColor(255,255,255)}};
        for(int layer:{CopperBottom,Inner2,Inner1,CopperTop,SilkBottom,SilkTop,Outline})if(layers->contains(layer))(*layers)[layer].paint(p,colours[layer],Qt::black);
        QPen ring(QColor(255,255,255),1);ring.setCosmetic(true);p.setPen(ring);p.setBrush(Qt::NoBrush);
        for(const auto &h:*holes)p.drawEllipse(h.at,h.diameter/2,h.diameter/2);
    }
};
GerberImportDialog::GerberImportDialog(QWidget *parent):QDialog(parent){
    setWindowTitle(ui("Gerber importieren"));
    auto *layout=new QHBoxLayout(this);auto *left=new QVBoxLayout;layout->addLayout(left);
    auto *fileBox=new QGroupBox(ui("Gerber (RS-274X)"));auto *grid=new QGridLayout(fileBox);left->addWidget(fileBox);
    int row=0;
    for(int layer:layerOrder()){
        auto *name=new QLineEdit;name->setReadOnly(true);name->setMinimumWidth(220);auto *choose=new QPushButton(QStringLiteral("…"));choose->setMaximumWidth(36);
        auto *remove=new QPushButton(QStringLiteral("×"));remove->setMaximumWidth(36);remove->setToolTip(ui("Datei entfernen"));remove->setObjectName(QStringLiteral("remove-%1").arg(layer));
        auto *state=new QLabel;grid->addWidget(new QLabel(gerberOutputName(layer)),row,0);grid->addWidget(name,row,1);grid->addWidget(choose,row,2);grid->addWidget(remove,row,3);grid->addWidget(state,row+1,1,1,3);row+=2;
        names[layer]=name;states[layer]=state;
        connect(remove,&QPushButton::clicked,this,[this,layer]{setFile(layer,QString());});
        connect(choose,&QPushButton::clicked,this,[this,layer]{
            const auto file=QFileDialog::getOpenFileName(this,gerberOutputName(layer),QFileInfo(files.value(layer)).absolutePath(),
                ui("Gerber-Dateien (*.gbr *.ger *.pho *.art *.gtl *.gbl *.gto *.gbo *.gts *.gbs *.gtp *.gbp *.gko *.gm1 *.g1 *.g2);;Alle Dateien (*)"));
            if(!file.isEmpty())setFile(layer,file);});
    }
    // The drill file and its number format.
    auto *drills=new QGroupBox(ui("Bohrdaten (Excellon)"));auto *dg=new QGridLayout(drills);left->addWidget(drills);
    drillName=new QLineEdit;drillName->setReadOnly(true);auto *chooseDrills=new QPushButton(QStringLiteral("…"));chooseDrills->setMaximumWidth(36);drillState=new QLabel;
    auto *removeDrills=new QPushButton(QStringLiteral("×"));removeDrills->setMaximumWidth(36);removeDrills->setToolTip(ui("Datei entfernen"));removeDrills->setObjectName("removeDrills");
    dg->addWidget(drillName,0,0,1,4);dg->addWidget(chooseDrills,0,4);dg->addWidget(removeDrills,0,5);dg->addWidget(drillState,1,0,1,6);
    connect(removeDrills,&QPushButton::clicked,this,[this]{setDrillFile(QString());});
    unit=new QComboBox;unit->addItems({ui("Zoll"),ui("mm")});integers=new QSpinBox;integers->setRange(1,6);decimals=new QSpinBox;decimals->setRange(0,6);
    // How the numbers leave out zeros: the four ways of the reference.
    zeros=new QComboBox;zeros->setObjectName("drillZeros");
    zeros->addItems({ui("führende Nullen geschrieben (LZ)"),ui("führende Nullen weggelassen (TZ)"),ui("alle Stellen geschrieben"),ui("mit Dezimalpunkt")});
    dg->addWidget(unit,2,0);dg->addWidget(new QLabel(ui("Stellen")),2,1);dg->addWidget(integers,2,2);dg->addWidget(decimals,2,3);dg->addWidget(zeros,3,0,1,5);
    connect(chooseDrills,&QPushButton::clicked,this,[this]{
        const auto file=QFileDialog::getOpenFileName(this,ui("Bohrdaten (Excellon)"),QFileInfo(drillPath).absolutePath(),ui("Excellon-Bohrdaten (*.drl *.txt *.xln *.exc);;Alle Dateien (*)"));
        if(!file.isEmpty())setDrillFile(file);});
    for(auto *box:{unit,zeros})connect(box,&QComboBox::currentIndexChanged,this,[this]{if(!refreshing)readDrills();});
    for(auto *box:{integers,decimals})connect(box,&QSpinBox::valueChanged,this,[this]{if(!refreshing)readDrills();});
    auto *options=new QGroupBox(ui("Ziel"));auto *ov=new QVBoxLayout(options);left->addWidget(options);
    fresh=new QRadioButton(ui("Auf einer neuen Platine"));auto *current=new QRadioButton(ui("Auf der aktuellen Platine"));fresh->setChecked(true);ov->addWidget(fresh);ov->addWidget(current);
    vias=new QCheckBox(ui("Übereinanderliegende Pads mit Bohrung zu Vias machen"));vias->setChecked(true);ov->addWidget(vias);
    join=new QCheckBox(ui("Aneinanderhängende Striche zu einer Leiterbahn verbinden"));join->setChecked(true);ov->addWidget(join);
    left->addStretch();
    auto *right=new QVBoxLayout;layout->addLayout(right,1);
    preview=new GerberPreview;preview->layers=&layers;preview->holes=&hits;preview->clicked=[this]{showLarge();};right->addWidget(preview,1);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);right->addWidget(buttons);
    importButton=buttons->button(QDialogButtonBox::Ok);importButton->setText(ui("Importieren"));
    connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    setDrillFormat({});resize(1000,620);changed();
}
GerberImportDialog::~GerberImportDialog()=default;
bool GerberImportDialog::setFile(int layer,const QString &path){
    files.remove(layer);layers.remove(layer);names[layer]->setText(QFileInfo(path).fileName());bool ok=true;
    if(!path.isEmpty()){
        QFile f(path);
        try{if(!f.open(QIODevice::ReadOnly))throw FormatError(f.errorString());layers[layer]=readGerber(f.readAll());files[layer]=path;
            states[layer]->setText(ui("%1 Objekte").arg(layers[layer].objects.size()));}
        catch(const std::exception &e){states[layer]->setText(QString::fromUtf8(e.what()));ok=false;}
    }else states[layer]->clear();
    changed();return ok;
}
bool GerberImportDialog::setDrillFile(const QString &path){
    drillPath=path;drillName->setText(QFileInfo(path).fileName());
    if(path.isEmpty()){drillBytes.clear();hits.clear();drillState->clear();changed();return false;}
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)){drillBytes.clear();hits.clear();drillState->setText(f.errorString());changed();return false;}
    drillBytes=f.readAll();setDrillFormat(drillFormat(drillBytes));return !hits.isEmpty();
}
void GerberImportDialog::setDrillFormat(const DrillFormat &f){
    refreshing=true;unit->setCurrentIndex(f.metric?1:0);integers->setValue(f.integerDigits);decimals->setValue(f.decimalDigits);
    zeros->setCurrentIndex(f.decimalPoint?3:f.allDigits?2:f.trailingZeros?1:0);refreshing=false;
    readDrills();
}
void GerberImportDialog::showLarge(){
    QDialog large(this);large.setWindowTitle(ui("Vorschau"));auto *v=new QVBoxLayout(&large);auto *big=new GerberPreview;big->layers=&layers;big->holes=&hits;big->clicked=[&large]{large.accept();};
    v->addWidget(big);large.resize(1200,900);large.exec();
}
void GerberImportDialog::readDrills(){
    format.metric=unit->currentIndex()==1;format.integerDigits=integers->value();format.decimalDigits=decimals->value();
    const int z=zeros->currentIndex();format.trailingZeros=z==1;format.allDigits=z==2;format.decimalPoint=z==3;
    hits.clear();
    if(!drillBytes.isEmpty()){
        try{hits=readExcellon(drillBytes,format);drillState->setText(ui("%1 Bohrungen").arg(hits.size()));}
        catch(const std::exception &e){drillState->setText(QString::fromUtf8(e.what()));}
    }
    changed();
}
void GerberImportDialog::changed(){preview->update();importButton->setEnabled(!layers.isEmpty());}
GerberImport GerberImportDialog::import() const{
    GerberImport in;in.layers=layers;in.drills=hits;in.vias=vias->isChecked();in.joinTracks=join->isChecked();return in;
}
bool GerberImportDialog::newBoard() const{return fresh->isChecked();}
QString GerberImportDialog::boardName() const{
    for(int layer:layerOrder())if(files.contains(layer))return QFileInfo(files[layer]).completeBaseName();
    return ui("Gerber-Import");
}
QString GerberImportDialog::state(int layer) const{return states.contains(layer)?states[layer]->text():QString();}

// --- Component data
ComponentDataDialog::ComponentDataDialog(const Board &b,const QString &base,const ComponentDataSettings &s,QWidget *parent)
    :QDialog(parent),board(b),current(s),file(base+QStringLiteral(".csv")){
    setWindowTitle(ui("Export der Bauteildaten"));
    auto *layout=new QHBoxLayout(this);auto *left=new QVBoxLayout;auto *right=new QVBoxLayout;layout->addLayout(left);layout->addLayout(right,1);
    // The fields: checked ones are written, in the order of the list; dragging changes the order.
    auto *data=new QGroupBox(ui("Zu exportierende Daten"));auto *dv=new QVBoxLayout(data);left->addWidget(data);
    fields=new QListWidget;fields->setObjectName("componentFields");fields->setDragDropMode(QAbstractItemView::InternalMove);dv->addWidget(fields);
    QList<int> order=current.fields;for(int f=0;f<componentFieldCount;f++)if(!order.contains(f))order.append(f);
    for(int f:order){
        auto *item=new QListWidgetItem(componentFieldName(f));item->setFlags(item->flags()|Qt::ItemIsUserCheckable|Qt::ItemIsDragEnabled);
        item->setFlags(item->flags()&~Qt::ItemIsDropEnabled);item->setCheckState(current.fields.contains(f)?Qt::Checked:Qt::Unchecked);item->setData(Qt::UserRole,f);fields->addItem(item);
    }
    connect(fields,&QListWidget::itemChanged,this,[this]{changed();});
    connect(fields->model(),&QAbstractItemModel::rowsMoved,this,[this]{changed();});
    connect(fields->model(),&QAbstractItemModel::rowsInserted,this,[this]{changed();});
    connect(fields->model(),&QAbstractItemModel::rowsRemoved,this,[this]{changed();});
    auto *form=new QFormLayout;left->addLayout(form);
    auto *separator=new QComboBox;
    const QList<std::pair<QString,QString>> separators{{ui("Komma"),QStringLiteral(",")},{ui("Semikolon"),QStringLiteral(";")},{ui("Tabulator"),QStringLiteral("\t")},{ui("Leerzeichen"),QStringLiteral(" ")}};
    for(const auto &[text,value]:separators)separator->addItem(text,value);
    separator->setCurrentIndex(std::max(0,separator->findData(current.separator)));form->addRow(ui("Trennzeichen"),separator);
    connect(separator,&QComboBox::currentIndexChanged,this,[this,separator]{current.separator=separator->currentData().toString();changed();});
    auto *top=new QLineEdit(current.top);auto *bottom=new QLineEdit(current.bottom);auto *standard=new QPushButton(ui("Standard"));
    auto *sides=new QHBoxLayout;sides->addWidget(top);sides->addWidget(bottom);sides->addWidget(standard);form->addRow(ui("Texte für Layerseiten"),sides);
    connect(top,&QLineEdit::textChanged,this,[this](const QString &t){current.top=t;changed();});
    connect(bottom,&QLineEdit::textChanged,this,[this](const QString &t){current.bottom=t;changed();});
    connect(standard,&QPushButton::clicked,this,[top,bottom]{top->setText(QStringLiteral("Top"));bottom->setText(QStringLiteral("Bottom"));});
    auto *unit=new QComboBox;unit->addItems({ui("mm"),ui("mil"),ui("Zoll")});unit->setCurrentIndex(std::clamp(current.unit,0,2));
    auto *decimals=new QSpinBox;decimals->setRange(0,6);decimals->setValue(current.decimals);decimals->setSuffix(ui(" Nachkommastellen"));
    auto *strip=new QCheckBox(ui("Nullen am Ende weglassen"));strip->setObjectName("stripZeros");strip->setChecked(current.stripZeros);
    auto *position=new QHBoxLayout;position->addWidget(unit);position->addWidget(decimals);position->addWidget(strip);form->addRow(ui("X/Y-Position"),position);
    connect(strip,&QCheckBox::toggled,this,[this](bool on){current.stripZeros=on;changed();});
    connect(unit,&QComboBox::currentIndexChanged,this,[this](int k){current.unit=k;changed();});
    connect(decimals,&QSpinBox::valueChanged,this,[this](int n){current.decimals=n;changed();});
    auto *prefix=new QCheckBox(ui("Mit vorangestelltem R"));prefix->setChecked(current.rotationPrefix);form->addRow(ui("Rotation"),prefix);
    connect(prefix,&QCheckBox::toggled,this,[this](bool on){current.rotationPrefix=on;changed();});
    // Which components: by kind, by side, and only those with pick and place data if wanted.
    {auto *grid=new QGridLayout;auto box=[&](const QString &text,const char *name,bool *target,int r,int c){auto *b=new QCheckBox(text);b->setObjectName(name);b->setChecked(*target);grid->addWidget(b,r,c);
            connect(b,&QCheckBox::toggled,this,[this,target](bool on){*target=on;changed();});};
        box(ui("SMD-Bauteile"),"smdParts",&current.smdParts,0,0);box(ui("Durchgesteckte Bauteile"),"throughHoleParts",&current.throughHoleParts,1,0);
        box(ui("Oberseite"),"onTop",&current.onTop,0,1);box(ui("Unterseite"),"onBottom",&current.onBottom,1,1);
        box(ui("Nur Bauteile mit Pick+Place-Daten"),"pickPlaceOnly",&current.pickPlaceOnly,2,0);form->addRow(ui("Filter"),grid);}
    auto *header=new QCheckBox(ui("Kopfzeile mit den Namen der Felder"));header->setChecked(current.header);form->addRow(header);
    connect(header,&QCheckBox::toggled,this,[this](bool on){current.header=on;changed();});
    auto *view=new QGroupBox(ui("Vorschau"));auto *vv=new QVBoxLayout(view);right->addWidget(view,1);
    text=new QPlainTextEdit;text->setObjectName("componentPreview");text->setReadOnly(true);text->setLineWrapMode(QPlainTextEdit::NoWrap);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));vv->addWidget(text);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Close);right->addWidget(buttons);
    buttons->button(QDialogButtonBox::Save)->setText(ui("Exportieren…"));
    connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    resize(900,520);changed();
}
QString ComponentDataDialog::preview() const{return componentData(board,current);}
void ComponentDataDialog::changed(){
    current.fields.clear();
    for(int i=0;i<fields->count();i++){const auto *item=fields->item(i);if(item->checkState()==Qt::Checked)current.fields.append(item->data(Qt::UserRole).toInt());}
    text->setPlainText(preview());
}
}
