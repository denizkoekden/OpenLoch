#pragma once
#include <QColor>
#include <QList>
#include <QPainterPath>
#include <QPolygonF>
namespace openloch {
struct Project;
// One conducting part as LochMaster's continuity tester sees it, in board coordinates.
struct Conductor {
    bool copper=false;   // board copper: a strip piece or a pad; otherwise a wire-like object
    int kind=0;          // wire kind 1, 9, 11, 13 or 19
    bool back=false;     // side; only compared for solder blobs (kind 19)
    QPolygonF path;      // wires: their path
    double width=0;
    QPainterPath shape;  // area used for contact tests; empty for wires without an area
    QRectF bounds;
    QColor potential;    // set by potentials()
};
// LochMaster's continuity model: board copper split by cuts and large drills, wires attached at their soldered ends.
class Continuity {
public:
    void addPad(QPointF centre,double diameter);
    void addStrip(QPointF from,QPointF to,double width);
    void addWire(int kind,bool back,const QPolygonF &path,double width);
    void addCut(const QRectF &rect);
    void addDrill(QPointF centre,double diameter);
    // Potential marker (LochMaster kind 18): colours the net under `at`; black markers colour nothing but still occupy copper.
    void addMarker(QPointF at,const QColor &colour,const QString &name={});
    // Everything connected to the copper under `at`; empty off copper or inside a drill larger than 1.5 mm.
    QList<Conductor> trace(QPointF at);
    // The conductors connected to the copper under `at`, by their place in the model, in ascending order; empty off
    // copper. Two points are connected when their lists are the same.
    QList<int> netAt(QPointF at);
    // Each net takes the colour of the first marker on it, in document order.
    struct Potentials {QList<Conductor> coloured;int conflicts=0;};  // conflicts: markers on a net already coloured differently
    Potentials potentials();
    // LochMaster's "Freie Bereiche": copper without any terminal, occupation spreading through overlapping copper only.
    QList<Conductor> freeCopper();
    // All drills as centre and diameter in millimetres, for drawing holes above coloured copper.
    QList<QPair<QPointF,double>> holes() const{return drills;}
    // OpenLoch extension: nets joining markers of different potentials (different names, or colours when unnamed).
    // `chain` is the shortest run of conductors from the first marker's copper to the other's: where the short lies.
    struct Short {QString first,second;QPointF from,to;QList<Conductor> chain;};
    QList<Short> shorts();
    int potentialMarkers() const{int n=0;for(const auto &m:markers)n+=m.colour.isValid()&&m.colour.rgb()!=QColor(Qt::black).rgb();return n;}
private:
    struct Strip {QPointF from,to;double width;};
    QList<Strip> strips;
    QList<QPair<QPointF,double>> pads,drills;
    QList<QRectF> cuts;
    struct Marker {QPointF at;QColor colour;QString name;};
    QList<Marker> markers;
    QList<Conductor> parts;
    QList<bool> marked;
    bool prepared=false;
    void prepare();
    QList<int> connected(QPointF at,bool click);
    QList<int> chain(QPointF from,QPointF to);
};
// Builds the model from a board project: template or generated copper, document wires, cuts and drills.
Continuity continuityModel(const Project &project);
}
