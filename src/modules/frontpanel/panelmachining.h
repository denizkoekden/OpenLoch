#pragma once
#include "frontpanel.h"
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QPolygonF>
#include <QString>

class QWidget;
// Manufacturing output: drills, milled and engraved contours of a panel, collected into plot jobs by tool, and each job
// written as an HPGL file. Contours drawn with the plain pen are not machined.
namespace openloch::frontpanel {
struct MachiningOptions {
    bool drills=true,mills=true,engravings=true,outline=false,commonOrigin=false,millDrills=false,layer2=false;
    double tool=1;                   // milling tool for drilled holes, mm
    // Another width chosen in the job list (element id → mm): the tool of a contour, the diameter of a drill, and under
    // "outline" the tool of the outer rectangle (2 mm otherwise).
    QMap<QString,double> toolFor;
};
// One movement of the tool: a single point is a plunge (drill), otherwise the tool runs along the points.
struct MachiningPath {
    QPolygonF points;   // panel millimetres
    QString element;    // id of the element it comes from (empty for the outer rectangle)
};
struct MachiningJob {
    enum Kind {Drill,Mill,DrillMill,Engrave,Outline};   // DrillMill: the holes milled out with one tool
    Kind kind=Drill;
    double tool=0;
    QList<MachiningPath> paths;
    QString title() const;       // for example "Bohren mit 3 mm"
    QString fileName() const;    // the title as file name with the ending .PLT
};
// The jobs in the order drills, milling, engraving (each by increasing tool), outer rectangle. As in the original,
// groups and combinations are taken apart and every part counts with its own tool; contours run as the original's
// outline builders make them (in whole fiftieths of a millimetre, curves as quadratic pieces of 21 points), without tool
// compensation, and texts in its own stroke font format as it plots them. Drills milled with a smaller tool run on a
// circle inside the hole.
QList<MachiningJob> machiningJobs(const Document &document,const Panel &panel,const MachiningOptions &options);
// The job as HPGL like the original writes it: plotter units of 1/40 mm from the bottom left corner of the panel,
// y upwards, the pen lifted and lowered only when needed, one command per line.
QByteArray hpgl(const MachiningJob &job,const Panel &panel,const MachiningOptions &options);
// The export dialog with the job list, a preview and the output folder; writes one file per job.
bool exportMachining(QWidget *parent,const Document &document,const Panel &panel,const QString &projectPath);
}
