#include "paneldialogs.h"
#include "panelgeometry.h"
#include "panelrender.h"
#include "strokefont.h"
#include "delphistream.h"
#include "language.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QIcon>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>
#include <cmath>

namespace openloch::frontpanel {
namespace {
QDialogButtonBox *buttons(QDialog *d){
    auto *box=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,d);
    box->button(QDialogButtonBox::Ok)->setText(ui("OK"));box->button(QDialogButtonBox::Cancel)->setText(ui("Abbrechen"));
    QObject::connect(box,&QDialogButtonBox::accepted,d,&QDialog::accept);QObject::connect(box,&QDialogButtonBox::rejected,d,&QDialog::reject);return box;
}
QDoubleSpinBox *number(double value,double low,double high,int decimals=2,const QString &suffix={}){
    auto *s=new QDoubleSpinBox;s->setRange(low,high);s->setDecimals(decimals);s->setValue(value);s->setLocale(uiLocale());if(!suffix.isEmpty())s->setSuffix(suffix);s->setAccelerated(true);return s;
}
QString mm(){return QStringLiteral(" mm");}
}

ColorButton::ColorButton(QWidget *parent):QToolButton(parent){
    setMinimumSize(38,22);setColor(value);
    connect(this,&QToolButton::clicked,this,[this]{const QColor c=QColorDialog::getColor(value,this,ui("Farbe wählen"));if(c.isValid()){setColor(c);if(changed)changed(c);}});
}
void ColorButton::setColor(const QColor &c){
    value=c;QPixmap swatch(28,14);swatch.fill(c);{QPainter p(&swatch);p.setPen(Qt::black);p.drawRect(0,0,27,13);}setIcon(QIcon(swatch));setIconSize(swatch.size());setToolTip(c.name());
}

LengthEdit::LengthEdit(double v,double lo,double hi,QWidget *parent):QWidget(parent),minimum(lo),maximum(hi){
    auto *row=new QHBoxLayout(this);row->setContentsMargins(0,0,0,0);spin=number(v,lo,hi,3);unit=new QComboBox;unit->addItems({"mm","inch"});row->addWidget(spin,1);row->addWidget(unit);
    // The spin box still shows the value in the unit chosen before.
    connect(unit,&QComboBox::currentIndexChanged,this,[this](int index){show(index==1?spin->value():spin->value()*25.4);});
    connect(spin,&QDoubleSpinBox::valueChanged,this,[this]{if(changed)changed(value());});
}
void LengthEdit::show(double mmValue){const QSignalBlocker block(spin);const bool inch=unit->currentIndex()==1;spin->setRange(inch?minimum/25.4:minimum,inch?maximum/25.4:maximum);spin->setDecimals(inch?4:3);spin->setValue(inch?mmValue/25.4:mmValue);}
double LengthEdit::value() const{return unit->currentIndex()==1?spin->value()*25.4:spin->value();}
void LengthEdit::setValue(double mmValue){show(mmValue);}

QStringList fillChoices(){
    return {ui("Einfarbig"),ui("Ohne Füllung"),ui("Schraffur waagerecht"),ui("Schraffur senkrecht"),ui("Schraffur fallend"),ui("Schraffur steigend"),ui("Gitter"),ui("Rautengitter"),
        ui("Verlauf waagerecht"),ui("Verlauf senkrecht"),ui("Verlauf diagonal fallend"),ui("Verlauf diagonal steigend")};
}
int fillChoice(const Fill &f){if(f.style==FillStyle::Solid&&f.gradient!=Gradient::None)return 7+int(f.gradient);return int(f.style);}
void setFillChoice(Fill &f,int choice){if(choice>=8){f.style=FillStyle::Solid;f.gradient=Gradient(choice-7);}else{f.style=FillStyle(std::clamp(choice,0,7));f.gradient=Gradient::None;}}
QStringList penStyleNames(){return {ui("Durchgezogen"),ui("Gepunktet"),ui("Gestrichelt"),ui("Strich-Punkt"),ui("Unsichtbar")};}
QStringList machiningNames(){return {ui("Stift (nur zeichnen)"),ui("Fräser"),ui("Gravierer")};}

bool editPanelProperties(QWidget *parent,Panel &panel,bool create){
    QDialog d(parent);d.setWindowTitle(create?ui("Neue Frontplatte"):ui("Eigenschaften der Frontplatte"));auto *form=new QFormLayout(&d);
    auto *name=new QLineEdit(panel.name);// Sizes as the original allows them.
    auto *width=new LengthEdit(panel.width,10,600),*height=new LengthEdit(panel.height,10,600);
    auto *color=new ColorButton,*color2=new ColorButton;color->setColor(panel.color);color2->setColor(panel.color2);
    auto *style=new QComboBox;style->addItems({ui("Einfarbig"),ui("Verlauf waagerecht"),ui("Verlauf senkrecht"),ui("Verlauf diagonal fallend"),ui("Verlauf diagonal steigend")});style->setCurrentIndex(int(panel.gradient));
    auto toggle=[&]{color2->setEnabled(style->currentIndex()>0);};QObject::connect(style,&QComboBox::currentIndexChanged,&d,toggle);toggle();
    form->addRow(ui("Name:"),name);form->addRow(ui("Breite:"),width);form->addRow(ui("Höhe:"),height);
    form->addRow(ui("Grundfarbe:"),color);form->addRow(ui("Verlaufsfarbe:"),color2);form->addRow(ui("Farbgebung:"),style);form->addRow(buttons(&d));
    if(d.exec()!=QDialog::Accepted)return false;
    panel.name=name->text();panel.width=width->value();panel.height=height->value();panel.color=color->color();panel.color2=color2->color();panel.gradient=Gradient(style->currentIndex());
    return true;
}

bool editGrid(QWidget *parent,Panel &panel){
    QDialog d(parent);d.setWindowTitle(ui("Raster einrichten"));auto *layout=new QVBoxLayout(&d);
    auto *gridBox=new QGroupBox(ui("Raster und Fang"));auto *gf=new QFormLayout(gridBox);
    auto *size=new LengthEdit(panel.grid,0.1,100);auto *visible=new QCheckBox(ui("Raster anzeigen"));visible->setChecked(panel.gridVisible);
    auto *snap=new QCheckBox(ui("Fang (Umschalttaste hält ihn beim Arbeiten an)"));snap->setChecked(panel.snap);auto *color=new ColorButton;color->setColor(panel.gridColor);
    gf->addRow(ui("Rasterweite:"),size);gf->addRow(visible);gf->addRow(snap);gf->addRow(ui("Rasterfarbe:"),color);
    auto *originBox=new QGroupBox(ui("Ursprung"));auto *of=new QFormLayout(originBox);auto *x=new LengthEdit(panel.origin.x(),-10000,10000),*y=new LengthEdit(panel.origin.y(),-10000,10000);
    of->addRow(ui("Ursprung X:"),x);of->addRow(ui("Ursprung Y:"),y);
    layout->addWidget(gridBox);layout->addWidget(originBox);layout->addWidget(buttons(&d));
    if(d.exec()!=QDialog::Accepted)return false;
    panel.grid=size->value();panel.gridVisible=visible->isChecked();panel.snap=snap->isChecked();panel.gridColor=color->color();panel.origin={x->value(),y->value()};return true;
}

QString pickStrokeFontCharacter(QWidget *parent,const QString &name){
    const StrokeFont *font=findStrokeFont(name);if(!font)return {};
    // The characters of the Windows code page, and for Unicode fonts every character they define, as far as the font
    // draws something for them in its order.
    QStringList characters;
    for(int code=33;code<256;code++){const QString c=frontdesigner::fromWindows1252(QByteArray(1,char(code)));if(c.size()==1&&c[0]!=QChar::ReplacementCharacter&&c[0].isPrint()&&(code=='?'||c[0]!='?')&&!characters.contains(c))characters<<c;}
    if(font->unicode){auto codes=font->shapes.keys();std::sort(codes.begin(),codes.end());
        for(int code:std::as_const(codes))if(code>255&&code<0x110000){const char32_t u=char32_t(code);characters<<QString::fromUcs4(&u,1);}}
    QDialog d(parent);d.setObjectName("strokeFontTable");d.setWindowTitle(name);auto *layout=new QVBoxLayout(&d);
    auto *list=new QListWidget;list->setObjectName("strokeFontCharacters");list->setViewMode(QListView::IconMode);list->setIconSize(QSize(40,40));list->setGridSize(QSize(56,64));
    list->setResizeMode(QListView::Adjust);list->setMovement(QListView::Static);list->setUniformItemSizes(true);
    for(const QString &c:characters){
        double advance=0;const QPainterPath path=font->text(c,&advance);if(path.isEmpty())continue;
        QPixmap icon(40,40);icon.fill(Qt::white);{QPainter p(&icon);p.setRenderHint(QPainter::Antialiasing);const QRectF r=path.boundingRect();
            const double scale=std::min(32/std::max(1.0,r.width()),32/std::max(1.0,r.height()));
            p.translate(20,20);p.scale(scale,-scale);p.translate(-r.center());p.setPen(QPen(Qt::black,0));p.drawPath(path);}
        auto *item=new QListWidgetItem(QIcon(icon),c,list);item->setData(Qt::UserRole,c);item->setToolTip(QString("%1 · U+%2").arg(c,QString::number(c.toUcs4().value(0),16).toUpper().rightJustified(4,u'0')));
    }
    layout->addWidget(list,1);
    if(list->count()==0)layout->addWidget(new QLabel(ui("Die Schrift zeichnet keine Zeichen.")));
    auto *cancel=new QDialogButtonBox(QDialogButtonBox::Cancel);QObject::connect(cancel,&QDialogButtonBox::rejected,&d,&QDialog::reject);layout->addWidget(cancel);
    QString picked;QObject::connect(list,&QListWidget::itemClicked,&d,[&](QListWidgetItem *item){picked=item->data(Qt::UserRole).toString();d.accept();});
    d.resize(720,560);
    return d.exec()==QDialog::Accepted?picked:QString();
}

bool editText(QWidget *parent,Element &text,bool create){
    QDialog d(parent);d.setWindowTitle(create?ui("Text einfügen"):ui("Eigenschaften Text"));auto *form=new QFormLayout(&d);
    auto *edit=new QLineEdit(text.text);edit->setMinimumWidth(320);auto *font=new QFontComboBox;font->setCurrentFont(QFont(text.font));
    const QPolygonF c=frameCorners(text.frame);auto *height=new LengthEdit(c.size()==4?QLineF(c[0],c[3]).length():3,0.2,1000);
    auto *bold=new QCheckBox(ui("Fett")),*italic=new QCheckBox(ui("Kursiv")),*underline=new QCheckBox(ui("Unterstrichen")),*strike=new QCheckBox(ui("Durchgestrichen"));
    bold->setChecked(text.bold);italic->setChecked(text.italic);underline->setChecked(text.underline);strike->setChecked(text.strikeOut);
    // The stroke fonts that are installed; an empty entry means the outline font.
    auto *stroke=new QComboBox;stroke->setEditable(true);stroke->addItem(QString());stroke->addItems(strokeFontNames());stroke->setCurrentText(text.strokeFont);
    stroke->lineEdit()->setPlaceholderText(ui("leer: Konturschrift (TrueType)"));
    auto *styles=new QHBoxLayout;for(auto *b:{bold,italic,underline,strike})styles->addWidget(b);
    // "SHX…" as in the original: the characters of the stroke font, a click puts one into the text at the cursor.
    auto *table=new QPushButton(ui("SHX…"));table->setObjectName("strokeFontTableButton");table->setToolTip(ui("Zeichen der Strichschrift wählen"));
    auto strokeRow=new QHBoxLayout;strokeRow->addWidget(stroke,1);strokeRow->addWidget(table);
    auto enable=[stroke,table]{table->setEnabled(findStrokeFont(stroke->currentText())!=nullptr);};enable();
    QObject::connect(stroke,&QComboBox::currentTextChanged,&d,enable);
    QObject::connect(table,&QPushButton::clicked,&d,[&d,stroke,edit]{const QString c=pickStrokeFontCharacter(&d,stroke->currentText());if(!c.isEmpty()){edit->insert(c);edit->setFocus();}});
    form->addRow(ui("Text:"),edit);form->addRow(ui("Schriftart:"),font);form->addRow(ui("Schrifthöhe:"),height);form->addRow(ui("Stil:"),styles);form->addRow(ui("Strichschrift:"),strokeRow);form->addRow(buttons(&d));
    edit->setFocus();if(d.exec()!=QDialog::Accepted||edit->text().isEmpty())return false;
    const double oldHeight=c.size()==4?QLineF(c[0],c[3]).length():0;
    text.text=edit->text();text.font=font->currentFont().family();text.bold=bold->isChecked();text.italic=italic->isChecked();text.underline=underline->isChecked();text.strikeOut=strike->isChecked();text.strokeFont=stroke->currentText().trimmed();
    // The frame keeps its place, direction and height; its width follows the text like the original lays it out.
    if(text.frame.size()==3){
        const QPointF p0=text.frame[0];QPointF along=text.frame[1]-p0,down=text.frame[2]-p0;const double la=std::hypot(along.x(),along.y()),ld=std::hypot(down.x(),down.y());
        if(la>0)along/=la;else along=QPointF(1,0);if(ld>0)down/=ld;else down=QPointF(0,1);
        const double h=height->value();Q_UNUSED(oldHeight);const double w=naturalTextWidth(text,h);text.frame={p0,p0+along*w,p0+down*h};
    }
    return true;
}

bool askDrillDiameter(QWidget *parent,double &diameter){
    QDialog d(parent);d.setWindowTitle(ui("Bohrung"));auto *form=new QFormLayout(&d);auto *value=number(diameter,0.01,100,2,mm());
    form->addRow(ui("Durchmesser:"),value);form->addRow(buttons(&d));value->selectAll();value->setFocus();
    if(d.exec()!=QDialog::Accepted)return false;diameter=value->value();return true;
}

bool editElementProperties(QWidget *parent,Document &document,Element &e){
    QDialog d(parent);d.setWindowTitle(ui("Eigenschaften: %1").arg(typeTitle(e.type)));auto *layout=new QVBoxLayout(&d);
    auto *general=new QFormLayout;layout->addLayout(general);
    auto *name=new QLineEdit(e.name);general->addRow(ui("Name:"),name);
    std::function<void()> apply;auto chain=[&](std::function<void()> f){auto previous=apply;apply=[previous,f]{if(previous)previous();f();};};
    const bool drawable=!e.isContainer()&&e.type!=ElementType::Image&&e.type!=ElementType::Picture;
    if(drawable&&e.type!=ElementType::Drill){
        auto *penBox=new QGroupBox(ui("Stift"));auto *pf=new QFormLayout(penBox);
        auto *tool=new QComboBox;tool->addItems(machiningNames());tool->setCurrentIndex(int(e.machining));
        auto *color=new ColorButton;color->setColor(e.pen.color);auto *width=number(e.pen.width,0,100,2,mm());auto *style=new QComboBox;style->addItems(penStyleNames());style->setCurrentIndex(int(e.pen.style));
        pf->addRow(ui("Werkzeug:"),tool);pf->addRow(ui("Farbe:"),color);pf->addRow(ui("Breite:"),width);pf->addRow(ui("Stil:"),style);layout->addWidget(penBox);
        chain([&e,tool,color,width,style]{e.machining=Machining(tool->currentIndex());e.pen.color=color->color();e.pen.width=width->value();e.pen.style=PenStyle(style->currentIndex());});
    }
    if(drawable&&(e.closed()||e.type==ElementType::Text||e.type==ElementType::Drill)){
        auto *fillBox=new QGroupBox(ui("Füllung"));auto *ff=new QFormLayout(fillBox);
        auto *style=new QComboBox;style->addItems(fillChoices());style->setCurrentIndex(fillChoice(e.fill));auto *color=new ColorButton,*color2=new ColorButton;color->setColor(e.fill.color);color2->setColor(e.fill.color2);
        ff->addRow(ui("Füllung:"),style);ff->addRow(ui("Farbe:"),color);ff->addRow(ui("Verlaufsfarbe:"),color2);layout->addWidget(fillBox);
        chain([&e,style,color,color2]{setFillChoice(e.fill,style->currentIndex());e.fill.color=color->color();e.fill.color2=color2->color();});
    }
    auto *geometry=new QGroupBox(ui("Lage und Größe"));auto *gf=new QFormLayout(geometry);bool geometryShown=true;
    switch(e.type){
    case ElementType::Ellipse:case ElementType::Arc:{
        auto *cx=number(e.center.x(),-10000,10000,3,mm()),*cy=number(e.center.y(),-10000,10000,3,mm()),*rx=number(e.radiusX,0,10000,3,mm()),*ry=number(e.radiusY,0,10000,3,mm()),*rot=number(e.rotation,-360,360,2," °");
        gf->addRow(ui("Mitte X:"),cx);gf->addRow(ui("Mitte Y:"),cy);gf->addRow(ui("Radius X:"),rx);gf->addRow(ui("Radius Y:"),ry);gf->addRow(ui("Drehung:"),rot);
        QDoubleSpinBox *start=nullptr,*span=nullptr;QComboBox *arcStyle=nullptr;
        if(e.type==ElementType::Arc){start=number(e.startAngle,-360,360,2," °");span=number(e.spanAngle,-360,360,2," °");arcStyle=new QComboBox;arcStyle->addItems({ui("Offener Bogen"),ui("Tortenstück"),ui("Bogen mit Sehne")});arcStyle->setCurrentIndex(int(e.arcStyle));
            gf->addRow(ui("Startwinkel:"),start);gf->addRow(ui("Bogenwinkel:"),span);gf->addRow(ui("Darstellung:"),arcStyle);}
        chain([&e,cx,cy,rx,ry,rot,start,span,arcStyle]{e.center={cx->value(),cy->value()};e.radiusX=rx->value();e.radiusY=ry->value();e.rotation=rot->value();
            if(start){e.startAngle=start->value();e.spanAngle=span->value();e.arcStyle=ArcStyle(arcStyle->currentIndex());}});
        break;}
    case ElementType::Drill:{
        auto *cx=number(e.center.x(),-10000,10000,3,mm()),*cy=number(e.center.y(),-10000,10000,3,mm()),*dm=number(e.diameter,0.01,1000,3,mm());
        gf->addRow(ui("Mitte X:"),cx);gf->addRow(ui("Mitte Y:"),cy);gf->addRow(ui("Durchmesser:"),dm);
        chain([&e,cx,cy,dm]{e.center={cx->value(),cy->value()};e.diameter=dm->value();});break;}
    case ElementType::Dimension:{
        auto *value=number(dimensionValue(e),0.001,10000,3,mm());gf->addRow(ui("Maß:"),value);const double before=dimensionValue(e);
        chain([&e,value,before]{if(std::abs(value->value()-before)>1e-9)e=dimensionWithValue(e,value->value());});break;}
    default:{
        const QRectF b=elementBounds(e);if(b.isNull()){geometryShown=false;break;}
        const QRectF shape=e.isContainer()?elementsBounds(e.children):elementPath(e).boundingRect();
        auto *x=number(shape.left(),-10000,10000,3,mm()),*y=number(shape.top(),-10000,10000,3,mm()),*w=number(shape.width(),0,10000,3,mm()),*h=number(shape.height(),0,10000,3,mm());
        gf->addRow(ui("Links:"),x);gf->addRow(ui("Oben:"),y);gf->addRow(ui("Breite:"),w);gf->addRow(ui("Höhe:"),h);
        chain([&e,x,y,w,h,shape]{
            const double sx=shape.width()>1e-9?w->value()/shape.width():1,sy=shape.height()>1e-9?h->value()/shape.height():1;
            const QTransform map=QTransform::fromTranslate(-shape.left(),-shape.top())*QTransform::fromScale(sx,sy)*QTransform::fromTranslate(x->value(),y->value());
            if(!map.isIdentity())transformElement(e,map);});
    }
    }
    if(geometryShown)layout->addWidget(geometry);else delete geometry;
    if(e.hasContour()){
        // The entries follow the order of Corners; the arc spline of lines read from the original only when present.
        auto *box=new QGroupBox(ui("Kontur"));auto *cf=new QFormLayout(box);auto *corners=new QComboBox;corners->addItems({ui("Original"),ui("B-Spline"),ui("Fase"),ui("Rundung")});
        if(e.contour.corners==Corners::ArcSpline)corners->addItem(ui("B-Spline ohne Endstücke"));
        corners->setCurrentIndex(int(e.contour.corners));
        auto *size=number(e.contour.size,0,1000,2,mm());cf->addRow(ui("Ecken:"),corners);cf->addRow(ui("Maß:"),size);layout->addWidget(box);
        chain([&e,corners,size]{e.contour.corners=Corners(std::max(0,corners->currentIndex()));e.contour.size=size->value();});
    }
    if(e.type==ElementType::Image){
        auto *box=new QGroupBox(ui("Transparenz"));auto *v=new QVBoxLayout(box);auto *on=new QCheckBox(ui("Eine Farbe ausblenden"));on->setChecked(e.transparent);
        auto *color=new ColorButton;color->setColor(e.transparentColor);auto *hint=new QLabel(ui("Klicken Sie in das Bild, um die auszublendende Farbe zu wählen."));hint->setWordWrap(true);
        const QImage image=resourceImage(document,[&]{Element plain=e;plain.transparent=false;return plain;}());
        auto *picture=new QLabel;picture->setPixmap(QPixmap::fromImage(image.scaled(240,160,Qt::KeepAspectRatio,Qt::SmoothTransformation)));picture->setCursor(Qt::CrossCursor);
        struct Picker:QObject{QImage image;QLabel *label;ColorButton *button;QCheckBox *on;bool eventFilter(QObject *,QEvent *ev) override{
            if(ev->type()==QEvent::MouseButtonPress&&!image.isNull()){const auto *m=static_cast<QMouseEvent*>(ev);const QPixmap pm=label->pixmap();if(pm.isNull())return false;
                const QPoint at=m->position().toPoint()-QPoint((label->width()-pm.width())/2,(label->height()-pm.height())/2);
                const QPoint source(at.x()*image.width()/std::max(1,pm.width()),at.y()*image.height()/std::max(1,pm.height()));
                if(image.rect().contains(source)){button->setColor(image.pixelColor(source));on->setChecked(true);}return true;}
            return false;}};
        auto *picker=new Picker;picker->setParent(&d);picker->image=image;picker->label=picture;picker->button=color;picker->on=on;picture->installEventFilter(picker);
        v->addWidget(on);v->addWidget(color);v->addWidget(picture);v->addWidget(hint);layout->addWidget(box);
        chain([&e,on,color]{e.transparent=on->isChecked();e.transparentColor=color->color();});
    }
    layout->addWidget(buttons(&d));
    if(d.exec()!=QDialog::Accepted)return false;
    e.name=name->text();if(apply)apply();return true;
}

bool askRegularPolygon(QWidget *parent,int &corners,double &radius,bool &inner,double &startAngle){
    QDialog d(parent);d.setWindowTitle(ui("Regelmäßiges Vieleck erzeugen"));auto *form=new QFormLayout(&d);
    auto *count=new QSpinBox;count->setRange(3,360);count->setValue(corners);auto *r=number(radius,0.01,10000,3,mm());
    auto *innerButton=new QRadioButton(ui("Innenradius (zu den Seitenmitten)")),*outerButton=new QRadioButton(ui("Außenradius (zu den Ecken)"));(inner?innerButton:outerButton)->setChecked(true);
    auto *angle=number(startAngle,-360,360,2," °");
    form->addRow(ui("Anzahl der Ecken:"),count);form->addRow(ui("Radius:"),r);form->addRow(innerButton);form->addRow(outerButton);form->addRow(ui("Startwinkel:"),angle);form->addRow(buttons(&d));
    if(d.exec()!=QDialog::Accepted)return false;
    corners=count->value();radius=r->value();inner=innerButton->isChecked();startAngle=angle->value();return true;
}

bool editDimensionStyle(QWidget *parent,DimensionStyle &style,bool &automaticColor,bool &askEveryTime){
    QDialog d(parent);d.setWindowTitle(ui("Vorgaben Bemaßung"));auto *form=new QFormLayout(&d);
    auto *font=new QFontComboBox;font->setCurrentFont(QFont(style.font));auto *height=number(style.textHeight,0.5,100,2,mm());
    auto *autoColor=new QRadioButton(ui("Automatisch (hebt sich von der Frontplatte ab)")),*ownColor=new QRadioButton(ui("Eigene Farbe:"));(automaticColor?autoColor:ownColor)->setChecked(true);
    auto *color=new ColorButton;color->setColor(style.color);auto *width=number(style.lineWidth,0,10,2,mm());auto *arrow=number(style.arrowLength,0.2,50,2,mm());
    auto *decimals=new QSpinBox;decimals->setRange(0,6);decimals->setValue(style.decimals);auto *inch=new QCheckBox(ui("Maße in inch"));inch->setChecked(style.inch);
    auto *ask=new QCheckBox(ui("Diesen Dialog beim Bemaßen immer zeigen"));ask->setChecked(askEveryTime);
    form->addRow(ui("Schriftart:"),font);form->addRow(ui("Schrifthöhe:"),height);form->addRow(autoColor);form->addRow(ownColor,color);
    form->addRow(ui("Stiftbreite:"),width);form->addRow(ui("Pfeillänge:"),arrow);form->addRow(ui("Nachkommastellen:"),decimals);form->addRow(inch);form->addRow(ask);form->addRow(buttons(&d));
    if(d.exec()!=QDialog::Accepted)return false;
    style.font=font->currentFont().family();style.textHeight=height->value();style.color=color->color();style.lineWidth=width->value();style.arrowLength=arrow->value();
    style.decimals=decimals->value();style.inch=inch->isChecked();automaticColor=autoColor->isChecked();askEveryTime=ask->isChecked();return true;
}

bool askDistribute(QWidget *parent,DistributeOptions &o){
    QDialog d(parent);d.setWindowTitle(ui("Objekte verteilen"));auto *layout=new QVBoxLayout(&d);
    auto side=[&](bool horizontal){
        auto *box=new QGroupBox;box->setCheckable(true);box->setTitle(horizontal?ui("Waagerecht gleichmäßig verteilen"):ui("Senkrecht gleichmäßig verteilen"));box->setChecked(horizontal?o.horizontal:o.vertical);
        auto *f=new QFormLayout(box);auto *reference=new QComboBox;
        reference->addItems(horizontal?QStringList{ui("Mittelpunkte"),ui("Linke Seiten"),ui("Rechte Seiten"),ui("Benachbarte Seiten")}:QStringList{ui("Mittelpunkte"),ui("Obere Seiten"),ui("Untere Seiten"),ui("Benachbarte Seiten")});
        reference->setCurrentIndex(horizontal?o.horizontalReference:o.verticalReference);
        auto *mode=new QComboBox;mode->addItems({ui("Automatisch, Gesamtgröße bleibt"),ui("Neue Gesamtgröße vorgeben"),ui("Abstand vorgeben")});mode->setCurrentIndex(horizontal?o.horizontalMode:o.verticalMode);
        auto *value=new LengthEdit(horizontal?o.horizontalValue:o.verticalValue,0,10000);auto *pen=new QComboBox;pen->addItems({ui("Ohne Stiftbreite (Außenlinie)"),ui("Mit Stiftbreite (Außenkante)")});
        pen->setCurrentIndex((horizontal?o.horizontalPen:o.verticalPen)?1:0);
        auto toggle=[mode,value]{value->setEnabled(mode->currentIndex()>0);};QObject::connect(mode,&QComboBox::currentIndexChanged,box,toggle);toggle();
        f->addRow(ui("Gleicher Abstand zwischen:"),reference);f->addRow(ui("Abstand:"),mode);f->addRow(ui("Vorgabe:"),value);f->addRow(ui("Stiftbreite:"),pen);layout->addWidget(box);
        return [=,&o]{if(horizontal){o.horizontal=box->isChecked();o.horizontalReference=reference->currentIndex();o.horizontalMode=mode->currentIndex();o.horizontalValue=value->value();o.horizontalPen=pen->currentIndex()==1;}
            else{o.vertical=box->isChecked();o.verticalReference=reference->currentIndex();o.verticalMode=mode->currentIndex();o.verticalValue=value->value();o.verticalPen=pen->currentIndex()==1;}};
    };
    auto h=side(true),v=side(false);layout->addWidget(buttons(&d));
    if(d.exec()!=QDialog::Accepted)return false;h();v();return true;
}

bool askAlignToGrid(QWidget *parent,GridAlignOptions &o){
    QDialog d(parent);d.setWindowTitle(ui("Am Raster ausrichten"));auto *layout=new QVBoxLayout(&d);
    auto side=[&](bool horizontal){
        auto *box=new QGroupBox;box->setCheckable(true);box->setTitle(horizontal?ui("Waagerecht am Raster ausrichten"):ui("Senkrecht am Raster ausrichten"));box->setChecked(horizontal?o.horizontal:o.vertical);
        auto *f=new QFormLayout(box);auto *reference=new QComboBox;reference->addItems(horizontal?QStringList{ui("Mittelpunkt"),ui("Linke Seite"),ui("Rechte Seite")}:QStringList{ui("Mittelpunkt"),ui("Obere Seite"),ui("Untere Seite")});
        reference->setCurrentIndex(horizontal?o.horizontalReference:o.verticalReference);auto *pen=new QComboBox;pen->addItems({ui("Ohne Stiftbreite (Außenlinie)"),ui("Mit Stiftbreite (Außenkante)")});
        pen->setCurrentIndex((horizontal?o.horizontalPen:o.verticalPen)?1:0);
        f->addRow(ui("Auf das Raster setzen:"),reference);f->addRow(ui("Stiftbreite:"),pen);layout->addWidget(box);
        return [=,&o]{if(horizontal){o.horizontal=box->isChecked();o.horizontalReference=reference->currentIndex();o.horizontalPen=pen->currentIndex()==1;}
            else{o.vertical=box->isChecked();o.verticalReference=reference->currentIndex();o.verticalPen=pen->currentIndex()==1;}};
    };
    auto h=side(true),v=side(false);layout->addWidget(buttons(&d));
    if(d.exec()!=QDialog::Accepted)return false;h();v();return true;
}

namespace {
// The symbol in a box; a click puts the insertion point there (on the grid unless Shift is held).
class SymbolPreview : public QWidget {
public:
    const Document *document=nullptr;Element *symbol=nullptr;double grid=1;
    SymbolPreview(){setMinimumSize(260,200);setCursor(Qt::CrossCursor);}
    QTransform map() const{
        const QRectF b=elementBounds(*symbol).adjusted(-2,-2,2,2);const double s=std::min((width()-10)/std::max(1e-6,b.width()),(height()-10)/std::max(1e-6,b.height()));
        return QTransform::fromTranslate(-b.center().x(),-b.center().y())*QTransform::fromScale(s,s)*QTransform::fromTranslate(width()/2.0,height()/2.0);
    }
protected:
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);p.fillRect(rect(),Qt::white);p.setRenderHint(QPainter::Antialiasing);p.setTransform(map());paintElement(p,*document,*symbol);
        p.resetTransform();if(symbol->hasAnchor){const QPointF a=map().map(symbol->anchor);p.setPen(Qt::NoPen);p.setBrush(Qt::red);p.drawEllipse(a,4,4);}
    }
    void mousePressEvent(QMouseEvent *event) override{
        QPointF at=map().inverted().map(event->position());if(!(event->modifiers()&Qt::ShiftModifier)&&grid>0)at=QPointF(std::round(at.x()/grid)*grid,std::round(at.y()/grid)*grid);
        symbol->hasAnchor=true;symbol->anchor=at;update();
    }
};
}
bool editSymbol(QWidget *parent,const Document &document,Element &symbol,double grid,const QString &title){
    QDialog d(parent);d.setObjectName("symbolProperties");d.setWindowTitle(title.isEmpty()?ui("Symbol in die Bibliothek aufnehmen"):title);auto *layout=new QVBoxLayout(&d);
    auto *form=new QFormLayout;auto *name=new QLineEdit(symbol.name);form->addRow(ui("Name:"),name);layout->addLayout(form);
    layout->addWidget(new QLabel(ui("Einfügepunkt, der beim Platzieren am Raster liegt:")));
    Element working=symbol;if(!working.hasAnchor){working.hasAnchor=true;working.anchor=elementPath(working).boundingRect().center();}
    auto *preview=new SymbolPreview;preview->document=&document;preview->symbol=&working;preview->grid=grid;layout->addWidget(preview,1);
    auto *positions=new QGridLayout;auto *edge=new QCheckBox(ui("Außenkante (mit Stiftbreite) statt Außenlinie"));
    const QStringList names{ui("oben links"),ui("oben mittig"),ui("oben rechts"),ui("mittig links"),ui("mittig"),ui("mittig rechts"),ui("unten links"),ui("unten mittig"),ui("unten rechts")};
    for(int i=0;i<9;i++){
        auto *b=new QPushButton(names[i]);positions->addWidget(b,i/3,i%3);
        QObject::connect(b,&QPushButton::clicked,&d,[&,i]{
            const QRectF r=edge->isChecked()?elementBounds(working):(working.isContainer()?QRectF():elementPath(working).boundingRect());
            QRectF box=r;if(box.isNull()){QPainterPath all;for(const auto &c:working.children)all.addPath(elementPath(c));box=all.boundingRect();}
            working.anchor=QPointF(box.left()+box.width()*(i%3)/2.0,box.top()+box.height()*(i/3)/2.0);working.hasAnchor=true;preview->update();
        });
    }
    layout->addLayout(positions);layout->addWidget(edge);layout->addWidget(buttons(&d));
    if(d.exec()!=QDialog::Accepted)return false;
    symbol=working;symbol.name=name->text();return true;
}

bool askImageExport(QWidget *parent,ImageExportOptions &o,QSizeF size,bool hasSelection){
    QDialog d(parent);d.setWindowTitle(ui("Grafik exportieren"));auto *form=new QFormLayout(&d);
    auto *format=new QComboBox;format->addItems({"PNG","JPG","BMP","EMF","PDF"});format->setCurrentText(o.format);
    auto *dpi=new QComboBox;dpi->setEditable(true);dpi->addItems({"75","100","150","300","600","1200"});dpi->setCurrentText(QString::number(o.dpi));
    auto *info=new QLabel,*memory=new QLabel;memory->setObjectName("exportMemory");auto *all=new QRadioButton(ui("Alle Objekte")),*marked=new QRadioButton(ui("Nur markierte Objekte"));(o.selectionOnly&&hasSelection?marked:all)->setChecked(true);marked->setEnabled(hasSelection);
    auto *background=new QCheckBox(ui("Mit Hintergrund"));background->setChecked(o.background);
    auto update=[&]{const int r=dpi->currentText().toInt();const int w=int(std::lround(size.width()/25.4*r)),h=int(std::lround(size.height()/25.4*r));
        info->setText(ui("Originalgröße %1 × %2 mm · Bild %3 × %4 Pixel").arg(uiLocale().toString(size.width(),'f',1),uiLocale().toString(size.height(),'f',1)).arg(w).arg(h));
        const bool raster=format->currentText()!="PDF"&&format->currentText()!="EMF";dpi->setEnabled(raster);
        // The memory of the picture as FrontDesigner names it: 24 bits a pixel, rows filled up to four bytes.
        memory->setText(uiLocale().formattedDataSize(qint64((qint64(w)*3+3)/4*4)*h,1,QLocale::DataSizeTraditionalFormat));form->setRowVisible(memory,raster);};
    QObject::connect(dpi,&QComboBox::currentTextChanged,&d,update);QObject::connect(format,&QComboBox::currentTextChanged,&d,update);
    form->addRow(ui("Dateiformat:"),format);form->addRow(ui("Auflösung (dpi):"),dpi);form->addRow(info);form->addRow(ui("Speicherbedarf:"),memory);form->addRow(all);form->addRow(marked);form->addRow(background);form->addRow(buttons(&d));update();
    if(d.exec()!=QDialog::Accepted)return false;
    o.format=format->currentText();o.dpi=std::clamp(dpi->currentText().toInt(),10,4800);o.selectionOnly=marked->isChecked();o.background=background->isChecked();return true;
}

bool askAutosave(QWidget *parent,int &minutes){
    QDialog d(parent);d.setWindowTitle(ui("AutoSpeichern"));auto *form=new QFormLayout(&d);
    auto *on=new QCheckBox(ui("Projekt regelmäßig als Sicherung (.BAK) speichern"));on->setChecked(minutes>0);auto *every=new QSpinBox;every->setRange(1,240);every->setValue(minutes>0?minutes:10);every->setSuffix(ui(" min"));
    form->addRow(on);form->addRow(ui("Alle:"),every);form->addRow(buttons(&d));
    if(d.exec()!=QDialog::Accepted)return false;minutes=on->isChecked()?every->value():0;return true;
}

namespace {
// The fonts of the texts with how often each is used and whether it is installed.
struct UsedFont {QString name;int count=0;bool installed=true;};
QList<UsedFont> usedFonts(const Document &document){
    QMap<QString,int> fonts;std::function<void(const Element&)> visit=[&](const Element &e){if(e.type==ElementType::Text)fonts[e.strokeFont.isEmpty()?e.font:e.strokeFont+" "+ui("(Strichschrift)")]++;for(const auto &c:e.children)visit(c);};
    for(const auto &p:document.panels)for(const auto &e:p.elements)visit(e);
    const QStringList installed=QFontDatabase::families();QList<UsedFont> out;
    for(auto it=fonts.begin();it!=fonts.end();++it){
        const QString family=it.key(),suffix=" "+ui("(Strichschrift)");const bool stroke=family.endsWith(suffix);
        out<<UsedFont{family,it.value(),stroke?findStrokeFont(family.left(family.size()-suffix.size()))!=nullptr:installed.contains(family,Qt::CaseInsensitive)};
    }
    return out;
}
QDialog *usedFontsDialog(QWidget *parent,const Document &document){
    auto *d=new QDialog(parent);d->setObjectName("usedFonts");d->setWindowTitle(ui("Verwendete Schriftarten"));auto *layout=new QVBoxLayout(d);auto *list=new QListWidget;
    const auto fonts=usedFonts(document);
    for(const auto &f:fonts)list->addItem(QString("%1 · %2 · %3").arg(f.name).arg(f.count).arg(f.installed?ui("vorhanden"):ui("fehlt, Ersatzschrift")));
    if(fonts.isEmpty())list->addItem(ui("Keine Texte"));
    layout->addWidget(list);auto *close=new QDialogButtonBox(QDialogButtonBox::Close);close->button(QDialogButtonBox::Close)->setText(ui("Schließen"));QObject::connect(close,&QDialogButtonBox::rejected,d,&QDialog::reject);layout->addWidget(close);
    return d;
}
}
bool editStrokeFontOrders(QWidget *parent){
    QDialog d(parent);d.setObjectName("strokeFontOrders");d.setWindowTitle(ui("Zeichenordnung der Strichschriften"));auto *layout=new QVBoxLayout(&d);
    auto *about=new QLabel(ui("FrontDesigner nimmt die Zeichen einer Strichschrift in der Ordnung von DOS. Eine Schrift, deren Umlaute an den Stellen von Windows liegen, lässt sich hier einzeln umstellen."));
    about->setWordWrap(true);layout->addWidget(about);
    const QStringList names=shapeFontNames();QList<QComboBox*> choices;
    auto *table=new QTableWidget(int(names.size()),3);table->setObjectName("strokeFontOrderTable");table->setHorizontalHeaderLabels({ui("Schrift"),ui("Umlaute"),ui("Zeichenordnung")});
    table->verticalHeader()->hide();table->setSelectionMode(QAbstractItemView::NoSelection);
    for(int row=0;row<names.size();row++){
        const auto places=umlautsInDosPlaces(names[row]);
        auto cell=[&](int column,const QString &text){auto *item=new QTableWidgetItem(text);item->setFlags(item->flags()&~Qt::ItemIsEditable);table->setItem(row,column,item);};
        cell(0,names[row]);cell(1,places?(*places?ui("an den Stellen von DOS"):ui("an den Stellen von Windows")):QStringLiteral("–"));
        auto *choice=new QComboBox;choice->addItems({strokeFontsInDosOrder()?ui("wie alle (DOS)"):ui("wie alle (Windows)"),QStringLiteral("DOS"),QStringLiteral("Windows")});
        const auto own=ownStrokeFontOrder(names[row]);choice->setCurrentIndex(own?(*own?1:2):0);table->setCellWidget(row,2,choice);choices<<choice;
    }
    table->resizeColumnsToContents();table->horizontalHeader()->setStretchLastSection(true);layout->addWidget(table);
    if(names.isEmpty())layout->addWidget(new QLabel(ui("Es sind keine Strichschriften installiert.")));
    layout->addWidget(buttons(&d));d.resize(560,360);
    if(d.exec()!=QDialog::Accepted)return false;
    for(int row=0;row<names.size();row++){const int c=choices[row]->currentIndex();setStrokeFontInDosOrder(names[row],c==0?std::nullopt:std::optional<bool>(c==1));}
    return true;
}
void showUsedFonts(QWidget *parent,const Document &document){std::unique_ptr<QDialog> d(usedFontsDialog(parent,document));d->exec();}
QStringList missingFonts(const Document &document){QStringList out;for(const auto &f:usedFonts(document))if(!f.installed)out<<f.name;return out;}
QDialog *usedFontsWindow(QWidget *parent,const Document &document){auto *d=usedFontsDialog(parent,document);d->setAttribute(Qt::WA_DeleteOnClose);d->setModal(false);return d;}
}
