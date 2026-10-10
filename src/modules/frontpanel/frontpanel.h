#pragma once
#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QTransform>

// The front panel document ("Frontplatte"): the module's own model, independent of the perfboard project.
// Units are millimetres. Coordinates belong to one panel: origin in its top left corner, x to the right, y down.
// Angles are degrees, positive counter-clockwise as seen on the screen.
namespace openloch::frontpanel {

enum class PenStyle {Solid, Dot, Dash, DashDot, None};
struct Pen {
    QColor color{Qt::black};
    double width=0.5;          // line width in mm
    PenStyle style=PenStyle::Solid;
    bool operator==(const Pen &) const=default;
};

// A fill is a colour or a hatch; a solid fill may run from `color` to `color2` (gradient).
enum class FillStyle {Solid, None, Horizontal, Vertical, ForwardDiagonal, BackwardDiagonal, Cross, DiagonalCross};
enum class Gradient {None, Horizontal, Vertical, Diagonal, CrossDiagonal};
struct Fill {
    FillStyle style=FillStyle::None;
    QColor color{Qt::white},color2{Qt::white};
    Gradient gradient=Gradient::None;
    bool operator==(const Fill &) const=default;
};

// Corners of a contour: as drawn, a B-spline through the edge midpoints, chamfered or rounded by `size` mm. An open
// B-spline begins and ends at the end points; the B-spline of the original's arcs (kept for lines read from its files)
// begins and ends in the middle of the first and last edge.
enum class Corners {Sharp, Spline, Chamfer, Round, ArcSpline};
struct Contour {
    Corners corners=Corners::Sharp;
    double size=2;
    bool operator==(const Contour &) const=default;
};

// What a contour means for manufacturing: only drawn, milled through, or engraved.
enum class Machining {None, Mill, Engrave};

enum class ElementType {Line, Polygon, Rectangle, Ellipse, Arc, Text, Drill, Image, Picture, Group, Dimension, Scale, Cutout};
enum class ArcStyle {Open, Pie, Chord};

struct Element {
    QString id;
    ElementType type=ElementType::Line;
    QString name;
    Pen pen;
    Fill fill;
    Contour contour;
    Machining machining=Machining::None;
    // Line, polygon and rectangle: the nodes (a rectangle keeps its four corners, also when turned).
    QPolygonF points;
    // Ellipse and arc: centre, radii along the turned axes, start and span of the arc.
    QPointF center;
    double radiusX=0,radiusY=0,rotation=0,startAngle=0,spanAngle=360;
    ArcStyle arcStyle=ArcStyle::Open;
    // Text, image and picture: the frame as top left, top right and bottom left corner. Text fills its frame.
    QPolygonF frame;
    QString text,font="Arial";
    bool bold=false,italic=false,underline=false,strikeOut=false;
    QString strokeFont;        // single-line engraving font; empty for the outline font
    // Drill: centre and diameter.
    double diameter=0;
    // Image and picture: key into Document::resources; an image may hide one colour.
    QString resource;
    bool transparent=false;
    QColor transparentColor{Qt::white};
    // Group, dimension, scale and cutout: the parts, and the parameters that generated them.
    QList<Element> children;
    QJsonObject parameters;
    // Insertion point of a symbol: the point that sticks to the cursor and snaps to the grid when it is placed.
    bool hasAnchor=false;
    QPointF anchor;
    // Values of an imported file that have no meaning in this model, kept to write the file back.
    QJsonObject foreign;
    // The component of a project the element stands for (32 hexadecimal digits): a hole for a potentiometer, or a symbol
    // of hole, cutout and texts as a group. Empty for the elements of no component; a copy is a new component.
    QString component;
    bool hasContour() const{return type==ElementType::Line||type==ElementType::Polygon||type==ElementType::Rectangle;}
    bool isContainer() const{return type==ElementType::Group||type==ElementType::Dimension||type==ElementType::Scale||type==ElementType::Cutout;}
    // A combination: closed contours that form one shape with holes (pen, fill and tool of the group, filled even-odd).
    bool combined() const{return type==ElementType::Group&&parameters.value("combine").toBool();}
    bool closed() const;
};

// How a panel is printed; each panel keeps its own settings. Gaps between tiles and the place of the first tile are
// panel millimetres, so that they grow and shrink with the print scale like the panel.
struct PrintSettings {
    static constexpr int maxTiles=100;
    bool mirror=false;
    bool background=true,frame=true,rulers=true,data=true,dimensions=true,machining=true,objects=true,cutMarks=true,texts=true;
    bool original=true;double zoom=1;      // print scale when not at original size
    bool centred=true,onlyOne=false;int sheet=1;
    bool landscape=false;
    int tilesX=1,tilesY=1;double gapX=0,gapY=0;
    double left=20,top=20;                 // when not centred: the first tile's corner from that of the printable area
    bool operator==(const PrintSettings &other) const=default;
};

// A circuit board behind a panel, in a project: a board of a perfboard or printed circuit board document, named by the
// identifiers of that document and of the board. The board's coordinates are millimetres as seen from its component
// side, origin in its top left corner, x to the right, y down. On the panel, as seen from the front, the board is
// mirrored left to right when its solder side faces the panel, then turned by `rotation` (degrees, counter-clockwise on
// the screen) about its origin, which lies at `offset`.
struct BoardBehind {
    QString document,board;
    QPointF offset;
    double rotation=0;
    bool solderSide=false;
    // From the board's millimetres to the panel's.
    QTransform toPanel() const;
    bool operator==(const BoardBehind &) const=default;
};

struct Panel {
    QString id,name;
    double width=100,height=50;
    QColor color{QColor(0xd8,0xd8,0xd8)},color2{Qt::white};
    Gradient gradient=Gradient::None;
    double grid=1;
    bool gridVisible=true,snap=true;
    QColor gridColor{QColor(0xc0,0xc0,0xc0)};
    QPointF origin;            // user origin for rulers and coordinates
    bool inch=false;           // rulers and coordinates in inch instead of mm
    QList<Element> elements;   // drawing order, bottom first
    QList<BoardBehind> boards; // circuit boards behind the panel, each board of a document at most once
    PrintSettings print;
    QJsonObject foreign;
};

// A saved view: a named area of one panel that the view manager zooms to.
struct View {
    QString name;
    int panel=0;
    QRectF area;
};

// Pictures and images are stored once per document and referenced by key (SHA-256 of the bytes).
struct Resource {
    QString kind;              // "bmp", "png", "jpg" or "emf"
    QByteArray data;
};

struct Document {
    static constexpr int schemaVersion=2;
    QString id,title;
    int revision=0;
    QList<Panel> panels;
    int activePanel=0;
    QMap<QString,Resource> resources;
    QList<View> views;
    QJsonObject foreign;
    Document();
    Panel &panel(){return panels[activePanel];}
    const Panel &panel() const{return panels[activePanel];}
    // Adds bytes as a resource and returns its key; the same bytes give the same key.
    QString addResource(const QByteArray &data,const QString &kind);
    // The document as a JSON object: the content of an .olfp file and, unchanged, the data of the document in a project.
    QJsonObject toJson() const;
    QByteArray encode() const;
    // Reads and checks a document; throws FormatError for anything it cannot accept.
    static Document fromJson(const QJsonObject &root);
    static Document decode(const QByteArray &data);
    static Document load(const QString &path);
    // Writes atomically: a failed save leaves an existing file untouched.
    void save(const QString &path) const;
};

QString newId();
// An element in the native format; `withIdentity` false leaves out ids and foreign values (to compare geometry).
QJsonObject elementToJson(const Element &element,bool withIdentity=true);
Panel newPanel(const QString &name,double width,double height);
Element newElement(ElementType type);
// Fresh ids for an element and everything inside it (pasted and duplicated copies). A component becomes a new one: the
// same new one for every element that shared it, across all elements renewed with the same `components`.
void renewIds(Element &element,QHash<QString,QString> *components=nullptr);
QString typeName(ElementType type);
// The visible name of an element type in the interface (German, through ui()).
QString typeTitle(ElementType type);
}
