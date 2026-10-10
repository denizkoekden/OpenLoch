#pragma once
#include "frontpanel.h"
#include <QJsonObject>
#include <QStringList>

// Objects built from parameters: regular polygons, dimensions, scales and cut-outs. Generated groups keep their
// parameters (and for scales and cut-outs where they were placed), so that they can be edited and built again; their
// parts are ordinary elements.
namespace openloch::frontpanel {
// A polygon with `corners` equal sides around `center`; `radius` to the corners, or to the edge midpoints when
// `inner` is set. The first corner lies at `startAngle` (counter-clockwise from the right).
Element regularPolygon(QPointF center,int corners,double radius,bool inner,double startAngle);

struct DimensionStyle {
    QColor color{Qt::black};
    double lineWidth=0,textHeight=3,arrowLength=2;
    int decimals=2;
    QString font="Arial";
    bool inch=false;
    QJsonObject toJson() const;
    static DimensionStyle fromJson(const QJsonObject &o);
};
// A dimension of the distance from `a` to `b`, its line parallel to a–b through `at`: two arrows (each the half
// dimension line with its head), the measured value, two extension lines. The value reads in the direction a → b;
// when the distance is too short, the arrows point inwards from outside.
Element dimension(QPointF a,QPointF b,QPointF at,const DimensionStyle &style={});
// The distance a dimension measures, and the same dimension with another value (b moves along a–b).
double dimensionValue(const Element &dimension);
Element dimensionWithValue(const Element &dimension,double value);

// A scale of the scale assistant, described like in FrontDesigner: the style (numbered as there), the list of
// parameters that belongs to the style, the label texts and the look of each part. This keeps the original's settings
// files (.SCL) readable and writable without loss.
struct ScaleParameters {
    enum Style {StraightLinear,RoundLinear,StraightLogarithmic,RoundLogarithmic,StraightDots,RoundDots,Segments,
                IncreasingArc,DecreasingArc,BothSidesArc,CircleSegment,Polygon,Sine};
    // One parameter of a style: yes/no, a count or a number with unit; `low`…`high` is the range the assistant allows.
    struct Info {
        enum Kind {Flag,Count,Real};
        Kind kind=Real;QString label,unit;double value=0,low=0,high=0;
    };
    // The look of one part of a scale (labels, first division, base line, …): pen and tool, and for parts that have
    // them a fill and a font. `fontSize` only travels through the original's files; the label height is a parameter.
    struct Design {
        QString name;bool hasFill=false,hasFont=false;
        Pen pen{Qt::black,0,PenStyle::Solid};
        Machining tool=Machining::None;
        Fill fill{FillStyle::Solid,Qt::black,Qt::white,Gradient::None};
        QString font="Arial",strokeFont;bool useStrokeFont=false,bold=false,italic=false;
        int fontSize=50;
        bool operator==(const Design &) const=default;
    };
    // Positions in `values`, by style family (the order of info()).
    enum StraightValue {StraightLength,StraightBaseLine,StraightDivisions1,StraightTick1,StraightSecond,StraightDivisions2,StraightTick2,StraightLabels,
                        StraightTextHeight,StraightDistance,StraightTextAngle,StraightReverse};
    enum RoundValue {RoundRange,RoundRadius,RoundBaseLine,RoundCentre,RoundDivisions1,RoundTick1,RoundSecond,RoundDivisions2,RoundTick2,RoundRotation,
                     RoundLabels,RoundTextHeight,RoundDistance,RoundTextAngle,RoundReverse,RoundLowProfile,RoundRotateText};
    enum SegmentValue {SegmentRange,SegmentRadius,SegmentWidth,SegmentCount,SegmentCentre,SegmentLimit1,SegmentBoundary=SegmentLimit1+9,
                       SegmentBoundaryLength,SegmentRotation,SegmentLabels,SegmentTextHeight,SegmentDistance,SegmentTextAngle,SegmentRotateText};
    enum ArcValue {ArcRange,ArcRadius,ArcWidth,ArcCentre,ArcRotation};
    enum PolygonValue {PolygonDiameter,PolygonCorners,PolygonCentre,PolygonRotation};
    enum SineValue {SineHeight,SineWidth,SineOscillations,SinePhase,SineCentreLine};
    Style style=RoundLinear;
    QList<double> values;   // in the order of info(style); flags are 0 or 1
    QStringList texts;      // one per label position (labelCount()); missing ones are numbered
    QList<Design> design;   // in the order of defaultDesign(style)
    ScaleParameters():ScaleParameters(RoundLinear){}
    explicit ScaleParameters(Style style);   // the original's factory settings of the style
    static QList<Info> info(Style style);
    static QList<Design> defaultDesign(Style style);
    double value(int index) const;   // the default where the list is short
    void setValue(int index,double v);
    // Positions that carry a label (0 for styles without labels) and their texts: given ones, else 0, 1, 2, …
    int labelCount() const;
    QStringList labelTexts() const;
    QJsonObject toJson() const;
    static ScaleParameters fromJson(const QJsonObject &o);
    // The original's scale settings: INI text with the sections Parameter (Style, Parameter1 …), Text and Design;
    // older files have Font and Pen instead of Design. Missing parameters keep their defaults.
    static ScaleParameters fromScl(const QByteArray &ini);
    QByteArray toScl() const;
};
QString scaleStyleTitle(ScaleParameters::Style style);
// The scale around its reference point, then moved by `placement` (for example to the position chosen by the user).
// The reference point is the centre of round scales, polygons and sine curves and the middle of straight scales.
Element scale(const ScaleParameters &parameters,const QTransform &placement);

// A cut-out for a panel instrument: an optional frame (outline of the instrument's front), the milled opening
// (milled along its inside with the tool, so that the opening gets its nominal size) and mounting holes.
struct CutoutParameters {
    enum Shape {None,Rectangular,Round};
    Shape frame=Rectangular,cut=Rectangular;
    bool din=true;
    double frameWidth=96,frameHeight=96,cutWidth=92,cutHeight=92,tool=3;
    bool holesSides=false,holesTopBottom=false;
    double holesSidesDistance=110,holesTopBottomDistance=110,holesSidesDiameter=3.2,holesTopBottomDiameter=3.2;
    QString name;
    QJsonObject toJson() const;
    static CutoutParameters fromJson(const QJsonObject &o);
    // The instrument definition files (.CUT) of the original program: a small INI text with sections Name, Frame, Cut and Holes.
    static CutoutParameters fromCut(const QByteArray &ini);
    QByteArray toCut() const;
};
Element cutout(const CutoutParameters &parameters,const QTransform &placement);
// Front frame sizes of panel instruments (DIN 43700 / IEC 61554) and the cut-out that belongs to each.
QList<std::pair<double,double>> standardCutoutSizes();
double standardCutoutFor(double frame);

// A generated group built again from its parameters at the place it was moved to (dimension, scale, cut-out);
// other elements are returned unchanged.
Element regenerate(const Element &e);
QTransform placementOf(const Element &e);
}
