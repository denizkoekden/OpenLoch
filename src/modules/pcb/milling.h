#pragma once
#include <functional>
#include "fabrication.h"
#include <QByteArray>
#include <QList>
#include <QPolygonF>
#include <QString>
#include <utility>

// Isolation milling: the paths of a cutter around the copper of a side, so that the copper stays as drawn, with drill
// and contour jobs, written as HPGL for CNC mills.
namespace openloch::pcb {
struct MillingSettings {
    double toolWidth=.2;                // the isolation cutter
    // The copper sides milled, each with its mirroring (0 none, 1 left-right, 2 top-bottom) and, if wanted, a short
    // plunge into every hole as a centre punch.
    bool top=false,bottom=true;
    int topMirror=0,bottomMirror=1;
    bool punchTop=false,punchBottom=false;
    // Paths around the copper: the first half a cutter width away, each further one a cutter width less the overlap, as
    // the reference gives it: in percent of the cutter width.
    int passes=1;
    double overlap=25;
    int drillSide=0;                    // 0 no drilling, 1 from the top, 2 from the bottom
    int drillMode=1;                    // 0 holes milled as circles (HPGL CI), 1 plunges with one tool, 2 a tool per diameter
    double drillToolWidth=.8;           // the cutter of milled holes
    int contourSide=0;                  // the outline (tracks and circles on U), milled along its centre: 0 not, 1 from the top, 2 from the bottom
    // Registration holes outside the working area for turning the board over, at the eight places of the reference as
    // bits (1 top left, 2 top right, 4 bottom left, 8 bottom right, 16 top middle, 32 right middle, 64 bottom middle,
    // 128 left middle), their distance from the working area and their diameter; with separate files, if wanted, in every
    // file before its job as the zero point.
    int registrationCorners=0;
    double registrationDistance=5,registrationDiameter=1;
    bool registrationEveryFile=false;
    // Texts on copper: 0 isolated like other copper (outline), 1 milled along their strokes (single line); selected
    // texts can be treated differently.
    int texts=0,selectedTexts=0;
    bool onlySelected=false;            // only the selected elements
    bool minimalFeed=false;             // a short move between lowering and lifting in drill plunges
    bool roundedScale=false;            // 0.025 mm per plotter unit instead of 0.0254 mm
    bool separateFiles=false;           // a file per job, named with its pen number
    bool jobListFile=false;             // the job list as a text file next to the milling file
    bool fromOrigin=true;               // coordinates from the board's origin, else from the top left corner (outputFrame)
};
enum class MillingJobKind {Registration,Isolation,Drill,Contour};
struct MillingJob {
    MillingJobKind kind=MillingJobKind::Isolation;
    int side=CopperBottom;              // the side milled from (it decides the mirroring)
    double tool=0;                      // cutter width, drill diameter, or 0 when it is up to the user
    QString name;
    QList<QPolygonF> paths;             // board coordinates in milling order; closed paths end at their start
    QList<QPointF> plunges;             // drill holes and centre punches
    QList<std::pair<QPointF,double>> circles;   // milled holes: centre and radius of the cutter's path
    int skipped=0;                      // holes smaller than the cutter
};
// The jobs in the order they are milled: registration holes first, then for each side milled from (top first) the
// isolation, the drilling and the contour. Without `paths` only names and tools are worked out, for the job list.
// `progress` hears of each milling path computed (done of total).
QList<MillingJob> millingJobs(const Board &board,const MillingSettings &settings,const QList<int> &selection={},bool paths=true,
                              const std::function<void(int,int)> &progress={});
// HPGL: IN and PA, then per job SP with its pen number (from `firstPen` on), PU and PD for paths and plunges, CI for
// milled holes, at last SP0. Plotter units of 0.0254 mm (or 0.025 mm), from the board's origin with y upwards.
QByteArray hpgl(const Board &board,const QList<MillingJob> &jobs,const MillingSettings &settings,int firstPen=1);
// The files of the jobs: one, or one per job named `base`_n with its pen number n, each with the registration holes
// before its job when wanted (on the registration job's pen).
QList<std::pair<QString,QByteArray>> millingFiles(const Board &board,const QList<MillingJob> &jobs,const MillingSettings &settings,const QString &base,const QString &suffix);
// The job list as text: a line per job with its pen.
QString millingJobList(const QList<MillingJob> &jobs);
}
