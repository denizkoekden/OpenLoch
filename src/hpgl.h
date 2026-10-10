#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QPoint>
#include <QString>
namespace openloch {
struct Project;
// LochMaster's "HPGL-Bearbeitungsdateien": one plot file per job (drills of one diameter, milled outlines of one
// width, cuts, outer rectangle) in plotter units of 1/40 mm from the origin, with the original's rounding.
struct PlotBoard {
    QList<QJsonObject> objects;  // collected like the original: cuts, drills, circles, outlines, lines; groups flattened
    QPoint origin;               // the user origin ("Ursprung") in file coordinates
    int width=0,height=0;
    QString name;
};
// The active board written as LM4 and read back, as the original exports from such a copy.
PlotBoard plotBoard(const Project &project);
struct PlotOptions {
    bool drills=true,mills=true,cuts=true,outline=false,millDrills=false,commonOrigin=false,layer2=false;
    float tool=1.0f;      // "Bohrungen ausfräsen mit", read back as float32 like the original's spin edit
    int outlineWidth=200; // tool of the outer rectangle in 1/100 mm
};
struct PlotJob {
    enum Type {Drill,MillDrill,Mill,Cut,Outline};
    Type type=Drill;QString name;double tool=0;bool layer2=false;
    QList<int> objects;   // indices into PlotBoard::objects
};
// The jobs in the original's order; lowers options.tool to the smallest drill diameter like the dialog does.
QList<PlotJob> plotJobs(const PlotBoard &board,PlotOptions &options);
QByteArray plotFile(const PlotJob &job,const PlotBoard &board,const PlotOptions &options);
// The points the original mills for an outline (TDraht or TKreis) and for a drill milled with a tool, in file
// coordinates; and the three milled sides of the outer rectangle.
QList<QPoint> plotPath(const QJsonObject &object);
QList<QPoint> plotDrillCircle(const QJsonObject &drill,float tool);
QList<QPoint> plotOutline(const PlotBoard &board,int width);
// Delphi's FloatToStr on a German system: shortest form with up to 15 digits and a decimal comma.
QString plotNumber(double value);
}
