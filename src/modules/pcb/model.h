#pragma once
#include <QByteArray>
#include <QColor>
#include <QJsonObject>
#include <QLineF>
#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QTransform>
#include <array>

// The printed circuit board document ("Leiterplatte"): the module's own model, independent of the perfboard project.
// Units are millimetres. Coordinates belong to one board: origin in the top left corner of its working area, x to the
// right, y down. Angles are degrees, positive counter-clockwise as seen from above (the component side).
namespace openloch::pcb {

// The seven layers, numbered as Sprint-Layout numbers them: copper and silkscreen of the top side, copper and
// silkscreen of the bottom side, two inner copper layers and the board outline.
enum Layer {CopperTop=1,SilkTop=2,CopperBottom=3,SilkBottom=4,Inner1=5,Inner2=6,Outline=7};
constexpr int layerCount=7;
bool isCopper(int layer);
// The short layer names of the interface: K1, B1, K2, B2, I1, I2, U.
QString layerName(int layer);
// The layers in drawing order as the reference stacks them: copper below silkscreen, the outline on top, and within
// copper and silkscreen the side of the active layer above the other. With the outline active, the physical order as
// seen from the side looked at.
QList<int> drawingOrder(int active,bool fromBelow=false);
// The layers in the order the reference lists them: K1, B1, I1, I2, K2, B2, U.
QList<int> layerOrder();

enum class ElementType {Pad,SmdPad,Track,Area,Circle,Text};
// Through-hole pad forms. "Wide" and "tall" forms are twice as long as their diameter, across or along the y axis.
enum class PadShape {Round=1,Octagon,Square,OvalWide,OctagonWide,SquareWide,OvalTall,OctagonTall,SquareTall};
// Texts of a component: free text, the designator ("R1") and the value ("10k").
enum class TextRole {Plain,Designator,Value};

// Fields of an imported Sprint-Layout record that OpenLoch does not edit; written back unchanged.
struct SprintSource {
    QByteArray record;      // the fixed part of the record
    QByteArray component;   // designator texts: the fixed part of the component data
    QByteArray text2;       // the record's second string
    bool operator==(const SprintSource &) const=default;
};

struct Element {
    ElementType type=ElementType::Track;
    int layer=CopperBottom;
    QString name;
    // Track and area: the nodes. Pads: the copper outline in the form the reference keeps it (two points for round
    // and oval pads, the corners otherwise); see padOutline().
    QPolygonF points;
    // Pad and circle: centre. Text: start of the text (left end of the baseline).
    QPointF pos;
    double width=0;             // track width, area outline, circle line width
    // Pad: outer and drill diameter. SMD pad: width and height before turning. Circle: radius of the line's centre.
    // Text: height.
    double size=0,size2=0;
    double rotation=0;          // pads and texts
    PadShape shape=PadShape::Round;
    bool via=false;             // through-plated pad
    double start=0,stop=0;      // circle: drawn counter-clockwise from start to stop, 0 at three o'clock; equal: full
    bool filled=false;          // circle: a disc or sector instead of a ring
    // Text: style 0 narrow, 1 normal, 2 wide; thickness 0 thin, 1 normal, 2 thick.
    QString text;
    TextRole role=TextRole::Plain;
    int style=1,thickness=1;
    bool mirrored=false;
    // Texts: their mirror counts as top to bottom (the reference's vertical mirror) for showing and storing it; mirrored
    // and rotation hold the text as drawn, top to bottom being left to right and half a turn.
    bool flipped=false;
    bool visible=true;          // designator and value texts can be hidden; plain texts are always shown
    QList<QPolygonF> strokes;   // the text as stroked polylines (as read, or built with textStrokes())
    double strokeWidth=0;
    // Manufacturing and ground planes ("AutoMasse")
    double clearance=.4;        // distance to a ground plane
    bool solderMask=false;      // pads: opening in the solder mask
    bool cutout=false;          // keeps the ground plane away
    bool flatStart=false,flatEnd=false;     // track ends square instead of round (reaching half the width beyond the node)
    bool autorouted=false;      // a track laid by the autorouter; it can be turned back into its airwire
    // An autorouted track: the two pads whose airwire it replaced (element indexes, -1 for none).
    std::array<int,2> autoroutePads{-1,-1};
    // Area filled with a grid of lines instead of solid copper: the grid pitch follows the outline width (twice it)
    // or is set, at least 0.5 mm; the lines are half the pitch wide.
    bool hatched=false,hatchAuto=true;
    double hatchPitch=.5;
    // Area on a copper layer that is only an opening in that side's solder mask ("Nur Lötstopp"): no copper, drawn
    // hatched.
    bool maskOnly=false;
    bool thermal=false;         // pad joins a ground plane through spokes
    // Eight spokes as bits from twelve o'clock clockwise: one byte per copper layer (K1, K2, I1, I2) when
    // `thermalPerLayer` is set, else the first byte for all layers.
    quint32 thermalSpokes=0x55;
    bool thermalPerLayer=false;
    int thermalWidth=100;       // spoke width in percent
    // Component data, kept on the designator text. The rotation for pick and place counts counter-clockwise on the top
    // side and clockwise on the bottom side. The centre for pick and place is the middle of the copper (0), of the
    // silkscreen (1) or of both (2), moved by the offset (x to the right, y upwards).
    QString package,comment;
    double componentRotation=0;
    bool pickAndPlace=false;
    int pickCentre=0;
    QPointF pickOffset;
    // Identifiers for projects: on a designator text the identifier of its component, on a pad the pin it stands for.
    QString component,pin;
    int part=0;                 // the number of the component the element belongs to on its board, 0 for none
    QList<int> groups;          // the groups the element belongs to
    QList<int> connections;     // airwires (Luftlinien) from this pad to the pads with these indexes
    SprintSource sprint;
    bool operator==(const Element &) const=default;
};

// A scanned layout under one side of a board ("Vorlage"): a picture file, its resolution and where its top left
// corner lies on the working area (from the working area's top left corner, y down). One-bit pictures are drawn in
// `colour`, lime as in the reference.
struct Template {
    QString file;
    double dpi=600;
    QPointF offset;
    bool shown=false;
    QColor colour{0,255,0};
    bool operator==(const Template &) const=default;
};

struct Board {
    QString id;                 // stable identifier (32 hexadecimal digits)
    QString name;
    double width=160,height=100;    // working area
    double grid=1.27;
    int activeLayer=CopperBottom;
    std::array<bool,layerCount+1> visible{false,true,true,true,true,true,true,true};    // index = layer
    std::array<bool,layerCount+1> groundPlane{};                                       // AutoMasse per layer
    bool multilayer=false;          // inner copper layers I1 and I2 in use
    // The origin of the coordinates shown (x to the right, y upwards from it): new boards have it at the bottom left
    // corner as in the reference; Sprint-Layout files store it with each board.
    QPointF origin;
    std::array<Template,2> templates;   // top side, bottom side; the one of the active layer's side is shown
    QList<Element> elements;
    QByteArray sprintHeader;        // the board header of an imported Sprint-Layout file
    bool operator==(const Board &) const=default;
};

struct Document {
    QList<Board> boards;
    int activeBoard=0;
    QString title,author,company,comment;   // "Projekt-Info"
    bool operator==(const Document &) const=default;
    Board &board(){return boards[qBound(0,activeBoard,int(boards.size())-1)];}
    const Board &board() const{return boards[qBound(0,activeBoard,int(boards.size())-1)];}
};

// Own file format: JSON with format "OpenLoch PCB", version 4. Version 1 lacks the pick and place centre and the solder
// mask openings, versions 1 and 2 the identifiers of boards, components and pins; they read with defaults and new
// identifiers. Before version 4 a component was the innermost group of its designator: its members get a component
// number when read. Numbers are millimetres and degrees.
QJsonObject toJson(const Document &document);
Document fromJson(const QJsonObject &json);     // throws FormatError for anything malformed or out of range
QByteArray encode(const Document &document);
Document decode(const QByteArray &bytes);
// Own format or Sprint-Layout (.lay6, .lay), by content; what converting an older Sprint-Layout file changed or left out
// is added to `notes`.
Document load(const QString &path,QStringList *notes=nullptr);
void save(const Document &document,const QString &path);  // own format, written atomically
constexpr const char *fileSuffix="olpcb";

// Geometry
// The copper outline of a pad as the reference stores it: round and oval forms as the two ends of their centre line,
// the others as corners, starting at the top left corner and running clockwise on the screen.
QPolygonF padOutline(PadShape shape,QPointF centre,double diameter,double rotation);
QPolygonF smdOutline(QPointF centre,double width,double height,double rotation);
// Updates the stored outline of a pad after its form, size, position or rotation changed.
void updateOutline(Element &pad);
// The area an element covers on its layer (tracks and lines with their width), and its bounding box. A hatched area
// counts as filled.
QPainterPath copperShape(const Element &element);
// Areas fill by the even-odd rule, as the reference fills them on the screen, in its ground plane and in its test: what
// an outline crossing itself encloses twice stays open. crossesItself tells whether an outline crosses or touches
// itself away from neighbouring edges; evenOddArea gives what it encloses (the outline as it is when it does not cross
// itself), as contours that do not cross, outer ones running one way and holes the other, so that either fill rule fills
// the same; orientedRegion turns any path into such contours by its own fill rule.
bool crossesItself(const QPolygonF &outline);
QPainterPath evenOddArea(const QPolygonF &outline);
QPainterPath orientedRegion(const QPainterPath &region);
// The nodes of a track or area without repeated points and without points in the middle of a straight run.
QPolygonF withoutRedundantNodes(const QPolygonF &nodes,bool closed=false);
// The square that a flat track end adds beyond its last node `end` (coming from `previous`).
QPolygonF squareEnd(QPointF end,QPointF previous,double width);
// The grid pitch of a hatched area.
double hatchSpacing(const Element &area);
// The lines of a hatched area's grid as the reference lays them: vertical and horizontal lines at multiples of the pitch,
// counted from the top left corner of the working area, each running from one crossing with the outline to the next
// (the crossings along a line taken in pairs). Nodes lying exactly on a grid line count as lying 0.2 µm further right
// and up. The lines are not cut to the outline: drawn hatchLineWidth() wide with round ends, they reach a quarter of
// the pitch beyond it. An outline with a node beyond 10 m or not a number has no lines.
QList<QLineF> hatchLines(const Element &area);
// The width of those lines: half the pitch.
double hatchLineWidth(const Element &area);
QRectF bounds(const Element &element);
// Moves, turns (about `pivot`) or mirrors (left-right about x = `axis`) an element with everything it stores.
void move(Element &element,QPointF delta);
void rotate(Element &element,QPointF pivot,double degrees);
void mirror(Element &element,double axis);
// Removes elements and keeps airwires and the pads of autorouted tracks pointing at the same pads.
void removeElements(Board &board,QList<int> indexes);
// An element of a list put behind `base` other elements: the elements it names (airwires, the pads of an autorouted
// track) are counted from there.
void offsetLinks(Element &element,int base);
// The next free group number and the next free component number on a board.
int nextGroup(const Board &board);
int nextPart(const Board &board);
// A component number none of `used` has, which is added to it: the one after the highest, or else the lowest free one
// (new numbers stay below 0xFFFF).
int freePart(QSet<int> &used);
// The elements together with all other members of their outermost groups (the last group of each element) and of
// their components, sorted.
QList<int> withGroups(const Board &board,QList<int> indexes);
// Component numbers for elements that go onto a board: a number the board uses already becomes a free one, the same
// for all elements that share it.
void freshParts(QList<Element> &elements,const Board &board);
// A component ("Bauteil"): the elements with one component number, among them a designator text and usually a value
// text. As in the reference, the last designator and the last value text in element order count.
struct Component {
    int designator=-1,value=-1;     // element indexes; value -1 when there is none
    int part=0;                     // its number on the board
    QList<int> members;             // all elements with the number, sorted
    QString id;                     // the identifier on its designator
    QStringList pins;               // the pins of its pads, in natural order (1, 2, …, 10)
    bool operator==(const Component &) const=default;
};
// A new identifier: 32 random hexadecimal digits, as projects use them.
QString newId();
// Identifiers for whatever lacks one: boards, components; pads of components without a pin take their name. An
// identifier used twice (within the board, or within the document) is renewed where it comes again.
void assignIds(Board &board);
void assignIds(Document &document);
// The identifier of the component an element belongs to, empty if none.
QString componentOf(const Board &board,int element);
// Designator and value of the component with this identifier, as a project gives them; false if there is none.
bool setComponentText(Board &board,const QString &id,const QString &designator,const QString &value);
// The components of a board in the order of their designators (R2 before R10).
QList<Component> components(const Board &board);
// Where a component sits: the middle of its pads (of all its elements when it has none).
QPointF componentCentre(const Board &board,const Component &component);
// Whether a component sits on the top side: its designator on the top silkscreen or copper.
bool componentOnTop(const Board &board,const Component &component);
// The centre a pick and place machine takes, as the reference finds it: the middle of the rectangle around the
// component's elements on the copper layers, on the silkscreen or on both, as its designator says (texts never count;
// around all its elements but hidden texts if none lies there), moved by the designator's offset.
QPointF pickPlaceCentre(const Board &board,const Component &component);
// The selector ("Selector"): the elements of one kind sorted into groups by one of their properties, the groups in
// ascending order. Properties: pads 0 diameter, 1 drill, 2 form, 3 through-plated; SMD pads 0 size; tracks 0 width;
// circles 0 radius, 1 width; areas 0 outline width; texts 0 height, 1 text. Layers: 0 all, 1 copper, 2 silkscreen and
// outline. `viasOnly` keeps the through-plated pads, which the reference lists as a kind of their own.
struct SelectorGroup {
    QString label;
    QList<int> elements;
};
QList<SelectorGroup> selectorGroups(const Board &board,ElementType type,int property,int layers=0,bool viasOnly=false);
// A new, empty board with the reference's defaults (160 × 100 mm, raster 1.27 mm, bottom copper active).
Board newBoard(const QString &name,double width=160,double height=100);
// A board as the reference's dialog for new boards sets it up: only a working area of the given size, or a working area
// round a rectangular or round outline on the outline layer with a margin on every side (the working area is the outline
// plus twice the margin; the outline is a closed track or a full circle `line` wide, 0 as in the reference). The origin
// goes to the bottom left corner of the outline (of the square round a circle) as in the reference, or to its top left
// corner with `originTopLeft`; a plain working area has it at its own corner.
struct NewBoard {
    enum Kind {Plain,Rectangle,Round} kind=Plain;
    double width=160,height=100;    // the working area (plain) or the rectangular outline
    double diameter=100;            // the round outline
    double margin=20;
    double line=0;
    bool originTopLeft=false;
};
Board newBoard(const QString &name,const NewBoard &shape);
// A new element of the given type with the defaults the reference uses for it.
Element newElement(ElementType type);
}
