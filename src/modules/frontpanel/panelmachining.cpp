#include "panelmachining.h"
#include "fdplot.h"
#include "panelgeometry.h"
#include "panelrender.h"
#include "language.h"
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
#include <functional>
#include <numbers>

namespace openloch::frontpanel {
namespace {
QString millimetres(double v){return uiLocale().toString(v,'g',6);}
double toolKey(double t){return std::round(std::max(0.0,t)*1000)/1000;}
// The course of a text as polygons; its strokes or outlines are flattened in plotter units, so that they come out finely.
QList<QPolygonF> textCourse(const Element &e){
    const QTransform up=QTransform::fromScale(40,40),down=QTransform::fromScale(1.0/40,1.0/40);QList<QPolygonF> out;
    for(const QPolygonF &p:elementPath(e).toSubpathPolygons(up))if(p.size()>=2)out<<down.map(p);
    return out;
}
// A polygon of whole file units (1/50 mm) as panel millimetres.
QPolygonF millimetres(const QPolygon &units){QPolygonF out;out.reserve(units.size());for(const QPoint &p:units)out<<QPointF(p.x()/50.0,p.y()/50.0);return out;}
struct Collector {
    const MachiningOptions &o;
    QMap<double,QList<MachiningPath>> drills,drillMills,mills,engravings;
    // Like the original, groups and combinations are taken apart: every part counts with its own tool, and contours
    // run as its outline builders make them.
    void add(const Element &e){
        if(e.isContainer()){for(const auto &c:e.children)add(c);return;}
        if(e.type==ElementType::Drill){
            // A diameter changed in the job list is that of the drill, in whole fiftieths of a millimetre.
            const bool changed=o.toolFor.contains(e.id);const double d=toolKey(o.toolFor.value(e.id,e.diameter));
            const QPointF centre=millimetres(QPolygon{frontdesigner::plotDrill(e)}).value(0);
            if(o.millDrills&&o.tool>0){
                // Milled on a circle inside the hole; a tool as large as the hole only plunges.
                drillMills[toolKey(o.tool)]<<MachiningPath{d>o.tool?millimetres(frontdesigner::plotMilledDrill(e,o.tool,changed?d:-1)):QPolygonF{centre},e.id};
            }
            else drills[d]<<MachiningPath{QPolygonF{centre},e.id};
            return;
        }
        if(e.machining==Machining::None||e.type==ElementType::Image||e.type==ElementType::Picture)return;
        auto &target=e.machining==Machining::Mill?mills:engravings;const double t=toolKey(o.toolFor.value(e.id,e.pen.width));
        if(e.type==ElementType::Text){
            // Texts in a stroke font of the original's own format run as it plots them, other texts along their strokes or outlines.
            QList<QPolygon> strokes;
            if(frontdesigner::plotText(e,strokes)){for(const QPolygon &s:strokes)if(!s.isEmpty())target[t]<<MachiningPath{millimetres(s),e.id};}
            else for(const auto &course:textCourse(e))target[t]<<MachiningPath{course,e.id};
            return;
        }
        const QPolygon outline=frontdesigner::plotOutline(e);
        if(!outline.isEmpty())target[t]<<MachiningPath{millimetres(outline),e.id};
    }
};
void names(const QList<Element> &list,QHash<QString,QString> &out){for(const auto &e:list){out.insert(e.id,e.name.isEmpty()?typeTitle(e.type):e.name);names(e.children,out);}}

// The panel with the paths of all jobs; the chosen job stands out.
class JobPreview : public QWidget {
public:
    const Panel *panel=nullptr;const QList<MachiningJob> *jobs=nullptr;int current=-1;
    JobPreview(){setMinimumSize(360,280);}
protected:
    void paintEvent(QPaintEvent *) override{
        QPainter p(this);p.fillRect(rect(),milledColor());if(!panel||!jobs)return;p.setRenderHint(QPainter::Antialiasing);
        QRectF area(0,0,panel->width,panel->height);for(const auto &j:*jobs)for(const auto &path:j.paths)area=area.united(path.points.boundingRect());
        area.adjust(-3,-3,3,3);const double s=std::min(width()/area.width(),height()/area.height());
        p.translate(width()/2.0,height()/2.0);p.scale(s,s);p.translate(-area.center());
        p.fillRect(QRectF(0,0,panel->width,panel->height),panel->color);
        for(int pass=0;pass<2;pass++)for(int i=0;i<jobs->size();i++){
            const bool chosen=i==current;if((pass==1)!=chosen)continue;const auto &j=(*jobs)[i];
            const QColor c=chosen?QColor(220,0,0):j.kind==MachiningJob::Engrave?QColor(Qt::white):QColor(0x50,0x50,0x50);
            QPen line(c,std::max(j.tool,0.0),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);if(j.tool<=0){line.setCosmetic(true);line.setWidthF(1);}
            for(const auto &path:j.paths){
                if(path.points.size()==1){p.setPen(Qt::NoPen);p.setBrush(c);const double r=std::max(j.tool/2,1/s);p.drawEllipse(path.points[0],r,r);}
                else{p.setPen(line);p.setBrush(Qt::NoBrush);p.drawPolyline(path.points);}
            }
        }
    }
};
}

QString MachiningJob::title() const{
    // As the original builds them: the kind, the tool as a general number, " mm" (its "Aussenrechteck" with ss).
    const QString value=millimetres(tool)+" mm";
    switch(kind){
    case Drill:return ui("Bohren mit ")+value;
    case Mill:return ui("Fräsen mit ")+value;
    case DrillMill:return ui("Bohrungen fräsen mit ")+value;
    case Engrave:return ui("Gravieren mit ")+value;
    case Outline:return ui("Aussenrechteck mit ")+value;
    }
    return {};
}
QString MachiningJob::fileName() const{return QString(title()).replace(QRegularExpression("[\\\\/:*?\"<>|]"),"_")+".PLT";}

QList<MachiningJob> machiningJobs(const Document &,const Panel &panel,const MachiningOptions &o){
    Collector c{o};for(const auto &e:panel.elements)c.add(e);
    QList<MachiningJob> jobs;
    auto take=[&](MachiningJob::Kind kind,const QMap<double,QList<MachiningPath>> &map){for(auto it=map.begin();it!=map.end();++it){MachiningJob j;j.kind=kind;j.tool=it.key();j.paths=it.value();jobs<<j;}};
    if(o.drills){take(MachiningJob::Drill,c.drills);take(MachiningJob::DrillMill,c.drillMills);}
    if(o.mills)take(MachiningJob::Mill,c.mills);
    if(o.engravings)take(MachiningJob::Engrave,c.engravings);
    if(o.outline){
        // Around the panel, half the tool outside its edge, so that the panel keeps its size; 2 mm unless changed.
        const double t=toolKey(o.toolFor.value("outline",2.0));MachiningJob j;j.kind=MachiningJob::Outline;j.tool=t;
        j.paths<<MachiningPath{millimetres(frontdesigner::plotPanelOutline(panel,t)),{}};jobs<<j;
    }
    return jobs;
}

QByteArray hpgl(const MachiningJob &job,const Panel &panel,const MachiningOptions &o){
    // Like the original's plot file: the header goes out with the first command and ends with the pen lifted, the pen
    // is lowered or lifted only when it is not already, and every command has its own line.
    QByteArray out;bool started=false,down=false;
    auto line=[&](const QByteArray &command){
        if(!started){started=true;out+="IN;\r\n";out+=o.layer2&&job.kind!=MachiningJob::Engrave?"SP2;\r\n":"SP1;\r\n";out+="PT0;\r\nPU;\r\n";down=false;}
        out+=command;out+="\r\n";
    };
    auto penUp=[&]{if(down){line("PU;");down=false;}};
    auto penDown=[&]{if(!down){line("PD;");down=true;}};
    // Coordinates: first to whole fiftieths of a millimetre (its own unit), then to plotter units of 1/40 mm from the
    // bottom left corner, each rounded to the nearest value with halves to even.
    auto own=[](double mm){return std::nearbyint(mm*50);};const double height=own(panel.height);
    auto at=[&](QPointF p){return "PA"+QByteArray::number(qint64(std::nearbyint(own(p.x())*0.8)))+","+QByteArray::number(qint64(std::nearbyint((height-own(p.y()))*0.8)))+";";};
    auto moveTo=[&](QPointF p){penUp();line(at(p));};
    auto lineTo=[&](QPointF p){penDown();line(at(p));};
    // A common origin: the pen down and up at the panel's bottom left corner before the job.
    if(o.commonOrigin){moveTo({0,panel.height});penDown();penUp();}
    for(const auto &path:job.paths){
        if(path.points.isEmpty())continue;
        moveTo(path.points[0]);
        if(path.points.size()==1)lineTo(path.points[0]);else for(int i=1;i<path.points.size();i++)lineTo(path.points[i]);
    }
    penUp();line("PA0,0;");
    return out;
}

bool exportMachining(QWidget *parent,const Document &document,const Panel &panel,const QString &projectPath){
    QSettings settings;MachiningOptions o;
    o.drills=settings.value("frontpanel/hpgl/drills",true).toBool();o.mills=settings.value("frontpanel/hpgl/mills",true).toBool();o.engravings=settings.value("frontpanel/hpgl/engravings",true).toBool();
    o.outline=settings.value("frontpanel/hpgl/outline",false).toBool();o.commonOrigin=settings.value("frontpanel/hpgl/commonOrigin",false).toBool();
    o.millDrills=settings.value("frontpanel/hpgl/millDrills",false).toBool();o.layer2=settings.value("frontpanel/hpgl/layer2",false).toBool();o.tool=settings.value("frontpanel/hpgl/tool",1.0).toDouble();
    QHash<QString,QString> elementNames;names(panel.elements,elementNames);
    QHash<QString,double> diameters;std::function<void(const QList<Element>&)> holes=[&](const QList<Element> &list){for(const auto &e:list){if(e.type==ElementType::Drill)diameters.insert(e.id,e.diameter);holes(e.children);}};
    holes(panel.elements);
    double smallest=0;{MachiningOptions plain;plain.millDrills=false;for(const auto &j:machiningJobs(document,panel,plain))if(j.kind==MachiningJob::Drill&&(smallest<=0||j.tool<smallest))smallest=j.tool;}

    QDialog d(parent);d.setWindowTitle(ui("HPGL-Bearbeitungsdateien exportieren"));d.resize(900,620);
    auto *layout=new QVBoxLayout(&d);auto *top=new QHBoxLayout;layout->addLayout(top,1);auto *side=new QVBoxLayout;top->addLayout(side);
    auto *drills=new QCheckBox(ui("&Bohrungen")),*mills=new QCheckBox(ui("&Fräsungen")),*engravings=new QCheckBox(ui("&Gravuren")),*outline=new QCheckBox(ui("Außenrechteck"));
    auto *origin=new QCheckBox(ui("Gemeinsamer Ursprung")),*millDrills=new QCheckBox(ui("Bohrungen ausfräsen mit:")),*layer2=new QCheckBox(ui("Bohren/Fräsen auf Layer &2"));
    auto *tool=new QDoubleSpinBox;tool->setRange(0.05,20);tool->setDecimals(2);tool->setSingleStep(0.1);tool->setSuffix(" mm");tool->setLocale(uiLocale());
    tool->setToolTip(ui("Fräser für das Ausfräsen der Bohrungen"));
    drills->setChecked(o.drills);mills->setChecked(o.mills);engravings->setChecked(o.engravings);outline->setChecked(o.outline);origin->setChecked(o.commonOrigin);millDrills->setChecked(o.millDrills);
    layer2->setChecked(o.layer2);tool->setValue(o.tool);
    for(auto *b:{drills,mills,engravings,outline,origin})side->addWidget(b);
    auto *toolRow=new QHBoxLayout;toolRow->addWidget(millDrills);toolRow->addWidget(tool);side->addLayout(toolRow);side->addWidget(layer2);
    auto *hint=new QLabel;hint->setWordWrap(true);side->addWidget(hint);
    auto *tree=new QTreeWidget;tree->setHeaderLabels({ui("Jobs")});tree->setContextMenuPolicy(Qt::CustomContextMenu);tree->setMinimumWidth(300);side->addWidget(tree,1);
    auto *preview=new JobPreview;preview->panel=&panel;top->addWidget(preview,1);
    auto *folderRow=new QHBoxLayout;auto *folder=new QLineEdit;auto *browse=new QToolButton;browse->setText(QStringLiteral("…"));browse->setToolTip(ui("Ordner wählen"));
    folderRow->addWidget(new QLabel(ui("Ausgabeordner für die Plotdateien:")));folderRow->addWidget(folder,1);folderRow->addWidget(browse);layout->addLayout(folderRow);
    const QFileInfo project(projectPath);
    folder->setText(projectPath.isEmpty()?QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("OpenLoch/HPGL")
                                         :project.absoluteDir().filePath(project.completeBaseName()+"-HPGL"));
    auto *box=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);box->button(QDialogButtonBox::Ok)->setText(ui("&Exportieren"));box->button(QDialogButtonBox::Cancel)->setText(ui("Abbrechen"));
    layout->addWidget(box);QObject::connect(box,&QDialogButtonBox::rejected,&d,&QDialog::reject);

    QList<MachiningJob> jobs;preview->jobs=&jobs;
    auto rebuild=[&]{
        o.drills=drills->isChecked();o.mills=mills->isChecked();o.engravings=engravings->isChecked();o.outline=outline->isChecked();o.commonOrigin=origin->isChecked();
        o.millDrills=millDrills->isChecked();o.layer2=layer2->isChecked();o.tool=tool->value();
        tool->setEnabled(o.millDrills);
        hint->setText(o.millDrills&&smallest>0&&o.tool>=smallest?ui("Der Fräser ist nicht kleiner als die kleinste Bohrung (%1 mm): solche Bohrungen werden nur angebohrt.").arg(millimetres(smallest)):QString());
        jobs=machiningJobs(document,panel,o);tree->clear();
        for(int i=0;i<jobs.size();i++){
            auto *item=new QTreeWidgetItem(tree,{ui("Job %1: %2").arg(i+1).arg(jobs[i].title())});item->setData(0,Qt::UserRole,i);
            QStringList seen;for(const auto &path:jobs[i].paths){if(path.element.isEmpty()||seen.contains(path.element))continue;seen<<path.element;
                auto *child=new QTreeWidgetItem(item,{elementNames.value(path.element)});child->setData(0,Qt::UserRole,i);child->setData(0,Qt::UserRole+1,path.element);}
        }
        if(jobs.isEmpty())new QTreeWidgetItem(tree,{ui("Keine Bohrungen, Fräsungen oder Gravuren")});
        box->button(QDialogButtonBox::Ok)->setEnabled(!jobs.isEmpty());preview->current=-1;preview->update();
    };
    rebuild();
    for(auto *b:{drills,mills,engravings,outline,origin,millDrills,layer2})QObject::connect(b,&QCheckBox::toggled,&d,rebuild);
    QObject::connect(tool,&QDoubleSpinBox::valueChanged,&d,rebuild);
    QObject::connect(tree,&QTreeWidget::currentItemChanged,&d,[&](QTreeWidgetItem *item){const QVariant v=item?item->data(0,Qt::UserRole):QVariant();preview->current=v.isValid()?v.toInt():-1;preview->update();});
    QObject::connect(tree,&QTreeWidget::customContextMenuRequested,&d,[&](QPoint at){
        QTreeWidgetItem *item=tree->itemAt(at);if(!item||!item->data(0,Qt::UserRole).isValid())return;const int index=item->data(0,Qt::UserRole).toInt();if(index<0||index>=jobs.size())return;
        QMenu menu;menu.addAction(ui("Ä&ndern…"),&d,[&,item,index]{
            const MachiningJob job=jobs[index];bool ok=false;const QString only=item->data(0,Qt::UserRole+1).toString();
            // A hole that is milled out changes its diameter, the job of milled holes its tool.
            const bool hole=job.kind==MachiningJob::DrillMill&&!only.isEmpty();
            const double value=QInputDialog::getDouble(&d,ui("Werkzeugbreite ändern"),hole?ui("Neuer Bohrungsdurchmesser in mm:"):ui("Neue Werkzeugbreite in mm:"),
                                                       hole?o.toolFor.value(only,diameters.value(only)):job.tool,0.01,100,3,&ok);
            if(!ok)return;
            if(job.kind==MachiningJob::DrillMill&&!hole){tool->setValue(value);return;}
            // Like the original, widths and diameters changed here are whole fiftieths of a millimetre.
            const double units=std::nearbyint(value*50)/50;
            if(job.kind==MachiningJob::Outline)o.toolFor["outline"]=units;
            else for(const auto &path:job.paths){if(!only.isEmpty()&&path.element!=only)continue;o.toolFor[path.element]=units;}
            rebuild();
        });
        menu.exec(tree->viewport()->mapToGlobal(at));
    });
    QObject::connect(browse,&QToolButton::clicked,&d,[&]{const QString dir=QFileDialog::getExistingDirectory(&d,ui("Ausgabeordner wählen"),folder->text());if(!dir.isEmpty())folder->setText(dir);});
    QObject::connect(box,&QDialogButtonBox::accepted,&d,[&]{
        const QString dir=folder->text().trimmed();if(dir.isEmpty())return;
        if(!QDir().mkpath(dir)){QMessageBox::warning(&d,d.windowTitle(),ui("Der Ordner %1 kann nicht angelegt werden.").arg(QDir::toNativeSeparators(dir)));return;}
        QStringList existing;for(const auto &j:jobs)if(QFileInfo::exists(QDir(dir).filePath(j.fileName())))existing<<j.fileName();
        if(!existing.isEmpty()&&QMessageBox::question(&d,d.windowTitle(),ui("Diese Plotdateien gibt es im Ordner schon:\n%1\n\nÜberschreiben?").arg(existing.join("\n")))!=QMessageBox::Yes)return;
        for(const auto &j:jobs){
            QSaveFile f(QDir(dir).filePath(j.fileName()));
            if(!f.open(QIODevice::WriteOnly)||f.write(hpgl(j,panel,o))<0||!f.commit()){QMessageBox::warning(&d,d.windowTitle(),ui("%1 kann nicht geschrieben werden: %2").arg(j.fileName(),f.errorString()));return;}
        }
        settings.setValue("frontpanel/hpgl/drills",o.drills);settings.setValue("frontpanel/hpgl/mills",o.mills);settings.setValue("frontpanel/hpgl/engravings",o.engravings);
        settings.setValue("frontpanel/hpgl/outline",o.outline);settings.setValue("frontpanel/hpgl/commonOrigin",o.commonOrigin);settings.setValue("frontpanel/hpgl/millDrills",o.millDrills);
        settings.setValue("frontpanel/hpgl/layer2",o.layer2);settings.setValue("frontpanel/hpgl/tool",o.tool);
        d.accept();QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    return d.exec()==QDialog::Accepted;
}
}
