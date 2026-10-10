#pragma once
#include <QByteArray>
#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QTransform>
#include <optional>

// The schematic document ("Schaltplan"): the module's own model, after the drawings of sPlan.
// Units are millimetres. Coordinates belong to one sheet: origin in the top left corner of the sheet, x to the right,
// y down. Angles are degrees, positive counter-clockwise on the screen.
namespace openloch::schematic {

// Outline of an element ("Umriss"): colour, width and style. `None` draws no outline ("kein Umriss").
// Lines and the outlines of shapes can also show `color2` in the gaps of a dashed style ("Zweifarbig"), a stripe of
// half their width along their middle ("Längsstreifen") and short stripes across them at even distances
// ("Querstreifen").
enum class PenStyle {Solid,Dash,Dot,DashDot,DashDotDot,None};
struct Pen {
    QColor color=QColor(0,0,0);
    double width=.25;
    PenStyle style=PenStyle::Solid;
    bool twoColour=false;QColor color2=QColor(255,0,0);
    bool inner=false;QColor innerColor=QColor(255,255,0);
    bool cross=false;QColor crossColor=QColor(0,0,255);
    bool operator==(const Pen &) const=default;
};
// Fill of closed elements: none, solid, or hatched with lines `lineWidth` wide and `spacing` apart (between the lines).
// `Diagonal` rises to the right ("/"), `BackDiagonal` falls ("\").
enum class FillStyle {None,Solid,Horizontal,Vertical,Cross,Diagonal,BackDiagonal,DiagonalCross};
struct Fill {
    FillStyle style=FillStyle::None;
    QColor color=QColor(255,255,255);
    double spacing=2,lineWidth=.1;
    bool operator==(const Fill &) const=default;
};
// Ends of open lines ("Linienenden"), in the order of the reference: none, hollow triangle, arrow, open arrow, circle,
// dot, diamond, filled diamond, square, filled square, bar, arrow pointing back, hollow triangle pointing back. Arrows
// and marks are `endSize` long.
enum class LineEnd {None,Triangle,Arrow,OpenArrow,Circle,Dot,Diamond,FilledDiamond,Square,FilledSquare,Bar,BackArrow,BackTriangle};
// Font of a text: family, height of a line of text ("Texthöhe": from the top of the highest to the bottom of the
// lowest letters of the font) in millimetres, style and colour. A text can have a background ("Hintergrund").
struct Font {
    QString family=QStringLiteral("Arial");
    double height=2.5;
    bool bold=false,italic=false,underline=false,strikeOut=false;
    QColor color=QColor(0,0,0);
    bool operator==(const Font &) const=default;
};
enum class Align {Left,Centre,Right};
// Curves of an ellipse: the whole ellipse, the arc from `start` to `stop` alone, as a pie or as a chord.
enum class ArcStyle {Full,Arc,Pie,Chord};
// Rectangle corners: square, rounded or bevelled by `corner` percent of the shorter side.
enum class Corners {Square,Round,Bevel};

enum class ItemType {
    Line,           // open line through `points` ("Linie"); a conductor of the circuit when `electrical`
    Polygon,        // closed line through `points`
    Bezier,         // cubic Bézier curve: `points` are start, control, control, end, control, control, end, …
    Rectangle,      // `centre`, `size`, `rotation`, corners
    Ellipse,        // `centre`, `size` (diameters), `rotation`, arc from `start` to `stop` (true angles from the centre)
    Text,           // `text` at `pos`, see Item
    TextBox,        // text in the rectangle of `centre` and `size` ("Mengentext"), with line breaks when `wrap`
    Junction,       // connection dot ("Lötpunkt") at `pos`, `size` across
    Image,          // picture `resource` in the rectangle of `centre` and `size`
    Group,          // `children` in sheet coordinates
    Component,      // a part ("Bauteil"), see Item
    Contact,        // a labelled terminal inside a component ("Kontakt"), see Item
    NetLabel,       // the name of the net at `pos` ("Netzname"); with `global` it joins sheets ("Blattverweis")
    Dimension       // a dimension ("Bemaßung"), see Item
};
// Kinds of dimensions: a length between two points, a radius and a diameter (the same with one or two arrows and no
// extension lines), an angle at a vertex.
enum class DimensionKind {Standard,Radial,Diameter,Angle};
// The texts of a component: its designator ("Bezeichner") and value ("Wert") show the component's fields.
enum class TextRole {Plain,Designator,Value};

struct Item {
    ItemType type=ItemType::Line;
    // 32 random hexadecimal digits: survives editing, saving and undo; copies get new ones (see assignIds). The id of
    // a component is its identity for other documents of a project, a contact's `name` its pin there.
    QString id;
    QPolygonF points;
    QPointF pos,centre;
    QSizeF size;
    double rotation=0;
    bool mirrored=false;
    Pen pen;
    Fill fill;
    // Lines: ends and the length of arrows and marks.
    LineEnd startEnd=LineEnd::None,endEnd=LineEnd::None;
    double endSize=2;
    // Lines at sheet level are conductors of the circuit unless switched off (a line used as a drawing).
    bool electrical=true;
    Corners corners=Corners::Square;
    double corner=0;
    double start=0,stop=360;
    ArcStyle arc=ArcStyle::Full;
    // Text, text box, contact, net label: the text and its font. A text's `pos` is the middle (Centre), left or
    // right end (Left, Right) of the top edge of its first line; it turns about `pos`. `mirrored` mirrors the letters.
    QString text;
    Font font;
    Align align=Align::Left;
    bool background=false;
    QColor backgroundColor=QColor(255,255,255);
    bool wrap=true,middle=false;    // text box: line breaks, vertically centred
    TextRole role=TextRole::Plain;
    // Junction: with `autoSize` ("Automatik") size and colour follow the lines it sits on, as in the reference: the
    // widest of them times 3, 4, 5, 6 or 7 for the step XS, S, M, L or XL (`sizeStep` 0 to 4), the colour of the last.
    bool autoSize=true;
    int sizeStep=3;
    QString resource;               // image: key into Document::resources
    bool global=false;              // net label: joins nets of the same name on all sheets ("Blattverweis")
    // Text links ("Text-Verlinkung"): an external link (a web address or a file), the text it leads to inside the
    // document (its id), and whether the text may be the target of links ("Als Ziel freigeben").
    QString link,linkTarget;
    bool linkable=false;
    // Text: written the other way round inside its unchanged frame, turned by 180° ("Textrichtung umkehren").
    bool reversed=false;
    QList<Item> children;           // group and component members

    // Component: placed with its insertion point ("Einfügepunkt") at `pos`; its `children` are drawn in local
    // coordinates around the insertion point, mirrored left to right when `mirrored`, then turned by `rotation`.
    // The fields are kept on the component, the texts with role Designator and Value show them.
    QString designator,value;
    QStringList extra;              // "Zusatztext" 1 to 4, for the parts list
    bool designatorVisible=true,valueVisible=true,autoNumber=true,askValue=false,inPartsList=true;
    QString caption;                // caption in the library ("Unterschrift")
    QString libraryEntry;           // the library entry it was placed from (only for information)
    // Parent and child ("Parent-Child"): a component with `parent` set accepts children, a child names its parent.
    bool parent=false;
    QString parentId;
    // Contact: `name` identifies the terminal (for example its pin number), `text` is shown at `pos` with `font`.
    // `pin` is where a wire connects, in the component's local coordinates; contacts read from sPlan files have none.
    QString name;
    QPointF pin;
    bool hasPin=false;
    bool visible=true;
    // Dimension: `points` are the two measured points and, for an angle, the vertex as the third. The dimension line
    // lies `offset` millimetres beside the measured points, positive to the left of the direction from the first to the
    // second point (above a dimension measured to the right). The text is `prefix`, Ø when `showDiameter`, the value
    // (the length times the sheet's scale, or the angle in degrees, with up to `digits` decimals) or `fixedValue` when
    // not `autoValue`, and `suffix`; the tolerances follow it small. It is drawn in `font`, lines and arrows in their
    // colours; arrows are `arrowLength` long and open `arrowAngle` degrees.
    DimensionKind dimension=DimensionKind::Standard;
    double offset=0,arrowAngle=15,arrowLength=2.8;
    QColor extensionColor=QColor(0,0,0),lineColor=QColor(0,0,0);
    int digits=1;
    bool decimalPoint=false,showDiameter=false,autoValue=true;
    QString fixedValue,prefix,suffix,upperTolerance,lowerTolerance;
    // The element as read from a sPlan file, kept so that writing it back unchanged gives the same bytes.
    QByteArray splan;

    // Declared here and defaulted after the class: Item holds a list of Items, which GCC and MSVC compare only once
    // Item is complete.
    bool operator==(const Item &) const;
};
inline bool Item::operator==(const Item &) const=default;

// The title block ("Formblatt") of a sheet: its items lie behind the circuit and are edited in a mode of their own.
// `frame` is the area the title block divides into `columns` and `rows` (for references to a place on the sheet),
// numbered from `columnStart` and `rowStart`; the lines of that grid are drawn with `showGrid`. The frame itself and
// its labels are items, made by the generator.
struct TitleBlock {
    QString name;
    QList<Item> items;
    QRectF frame;
    int columns=0,rows=0;
    int columnStart=1,rowStart=1;   // 1 to 10000
    bool showGrid=false;
    bool operator==(const TitleBlock &) const;
};
inline bool TitleBlock::operator==(const TitleBlock &) const=default;

// How a sheet is printed (see print.h): 1:1 or a free scale, the offset of the printed content (title block and circuit)
// from the top left corner of the printable area (mm on the paper, not scaled), the paper's orientation and a banner
// print over pages across and down with their overlap (mm).
struct PrintSettings {
    enum class Orientation {Automatic,Portrait,Landscape};
    bool free=false;                // free scale instead of 1:1
    double scale=1;
    QPointF offset;                 // of the content's top left corner from the printable area's
    Orientation orientation=Orientation::Automatic;
    int bannerX=1,bannerY=1;        // pages across and down
    double overlap=0;               // of neighbouring pages in a banner print
    double factor() const{return free?scale:1;}
    bool operator==(const PrintSettings &) const=default;
};

// The unit a sheet's scale is in ("1 mm = 2 m").
enum class ScaleUnit {Millimetre,Centimetre,Metre,Kilometre};
QString unitName(ScaleUnit unit);           // "mm", "cm", "m", "km"
double unitMillimetres(ScaleUnit unit);     // 1, 10, 1000, 1000000
struct Sheet {
    QString id;
    QString name,description;
    double width=297,height=210;    // A4 landscape
    double grid=1.27;               // snap grid ("Fangraster") in millimetres
    bool spare=false;               // "Reserveblatt": not printed
    // Magnetic guide lines ("Magnetlinien"): the y of each horizontal and the x of each vertical one, in millimetres.
    // They attract what is moved or drawn and are neither printed nor exported.
    QList<double> horizontalGuides,verticalGuides;
    // "Maßstab": one millimetre on the sheet stands for `scale` of `scaleUnit`; dimensions, rulers and coordinates show
    // lengths times `scale`, the unit names them.
    double scale=1;
    ScaleUnit scaleUnit=ScaleUnit::Millimetre;
    PrintSettings print;            // kept with the sheet
    TitleBlock titleBlock;
    QList<Item> items;              // drawn first = lowest
    QByteArray splan;               // the sheet's settings as read from a sPlan file, without its elements
    bool operator==(const Sheet &) const;
};
inline bool Sheet::operator==(const Sheet &) const=default;

// A user variable ("Anwender-Variable"): used in texts as <NAME>.
struct Variable {
    QString name,value;
    bool operator==(const Variable &) const=default;
};
// An embedded picture, keyed by the SHA-256 of its bytes.
struct Resource {
    QString kind;                   // "png", "jpg", "bmp"
    QByteArray data;
    bool operator==(const Resource &) const=default;
};

struct Document {
    QString id;                     // identity of the document in a project, 32 hexadecimal digits
    QList<Sheet> sheets;
    int activeSheet=0;
    QList<Variable> variables;
    QMap<QString,Resource> resources;
    // "Blätter mit Seitennummer": the sheet tabs show "number: name".
    bool sheetNumbers=true;
    // "Bauteile mit Seitennummer" and "Präfix": designators of components without a parent are shown as prefix, the
    // number of their sheet and the designator, without separator; the designator itself stays as entered.
    bool designatorPageNumbers=false;
    QString designatorPrefix;
    QByteArray splan;               // the start of a sPlan file as read (version, settings), without its sheets
    bool operator==(const Document &) const;
    Sheet &sheet(){return sheets[qBound(0,activeSheet,int(sheets.size())-1)];}
    const Sheet &sheet() const{return sheets[qBound(0,activeSheet,int(sheets.size())-1)];}
};
inline bool Document::operator==(const Document &) const=default;

// --- documents and ids
// A new document with one empty sheet (A4 landscape) named `sheetName`.
Document newDocument(const QString &sheetName);
Sheet newSheet(const QString &name,double width=297,double height=210,double grid=1.27);
// A new id: 32 random hexadecimal digits, as for the documents and components of a project.
QString newId();
// Gives an item and everything inside it new ids (for copies and pasted items).
void assignIds(Item &item);
// New ids for copied elements (also inside groups and components); children among the copies follow their copied
// parent, others keep theirs. Several lists share the renaming (sheets copied together).
void freshIds(const QList<QList<Item>*> &lists);
inline void freshIds(QList<Item> &items){freshIds(QList<QList<Item>*>{&items});}
// Ids missing anywhere get new ones; of two equal ids the later one is replaced.
void completeIds(Document &document);
// Finds an item by id on a sheet (also inside groups and components); nullptr if there is none.
Item *findItem(Sheet &sheet,const QString &id);
const Item *findItem(const Sheet &sheet,const QString &id);
int sheetIndex(const Document &document,const QString &id);

// --- geometry
// The placement of a component: local coordinates to sheet coordinates.
QTransform placement(const Item &component);
// The connection point of a contact on the sheet.
QPointF pinPosition(const Item &component,const Item &contact);
// The geometry of a line, polygon, Bézier curve, rectangle, ellipse, text box, picture or junction without its width,
// where it lives (local coordinates inside a component); empty for the others.
QPainterPath path(const Item &item);
// The outline of an element as drawn (lines with their width), in its own coordinates (local ones inside a component).
QPainterPath shape(const Item &item);
// The rectangle around an element on the sheet.
QRectF bounds(const Item &item);
QRectF bounds(const QList<Item> &items);
// The four corners of a rectangle, text box or picture in the order top left, top right, bottom right, bottom left.
QPolygonF corners(const Item &item);
// Moves, turns (counter-clockwise about `pivot`) or mirrors (left to right about x = `axis`, top to bottom about
// y = `axis`) an element with everything it holds. Texts keep readable letters unless `textsToo`.
void move(Item &item,QPointF delta);
void rotate(Item &item,QPointF pivot,double degrees);
void mirror(Item &item,double axis,bool textsToo=false);
void mirrorVertically(Item &item,double axis,bool textsToo=false);
// The lettering of components ("Bauteilbeschriftung"): font family, height, bold and italic for designator, value
// and contacts; a part left unset stays as it is.
struct Lettering {
    bool set=false;
    QString family;
    double height=2.5;
    bool bold=false,italic=false;
    QColor color=QColor(0,0,0);
};
void letter(Item &component,const Lettering &designator,const Lettering &value,const Lettering &contacts);
// Connection points for contacts from files that keep none (sPlan): for each contact of the component, also in groups,
// in drawing order, the free end of one of the component's lines nearest to its text (at most 6 mm away; an end
// touching another part other than a dot is not free, and each end serves one contact), in the component's own
// coordinates.
QList<std::optional<QPointF>> guessedPins(const Item &component);
// Gives the contacts without a connection point the guessed one; returns how many got one.
int guessPins(Item &component);
// The point "Am Raster ausrichten" puts on the grid, moving the whole element (sPlan's reference point): the first
// point of a line, polygon or curve, the first corner of a rectangle, picture or text box as sPlan keeps it (bottom
// left before turning), the left and top of an ellipse's outline before turning, the anchor of a text, the place of a
// junction, the insertion point of a component; for a group the point of its first element, replaced by that of any
// later one lying not further right (in tenths of a millimetre) and higher; contacts take no part.
QPointF gridPoint(const Item &item);
// "Ausrichten": moves elements so that their bounds share an edge or the middle of the bounds of all of them.
// HorizontalCentre puts the middles above one another (one x), VerticalCentre beside one another (one y).
enum class Alignment {Top,Bottom,Left,Right,HorizontalCentre,VerticalCentre};
void align(const QList<Item*> &items,Alignment how);
// "Gleichmäßig verteilen": in the order of their middles, the first and the last element stay and the others move so
// that the middles are equally far apart (horizontally or vertically).
void spread(const QList<Item*> &items,bool horizontally);
// Scales an element about `origin` by `factor`: places, sizes, text heights and line ends; line widths stay.
void scale(Item &item,QPointF origin,double factor);
// Stretches an element about `origin`, `sx` across and `sy` down, and shears it (`kx` moves x by that much per unit of y
// from the origin, `ky` y per unit of x), as the reference's sizers and arrows do: points follow exactly; rectangles,
// ellipses, text boxes and pictures move their centre and stretch their size when they stand upright (else by the mean
// factor); texts, junctions and components move with their point, their sizes by the mean factor.
void stretch(Item &item,QPointF origin,double sx,double sy,double kx=0,double ky=0);
// Texts, contacts and net labels.
bool isText(const Item &item);
// A text, contact or net label of a component as it is placed on the sheet: its `pos`, `pin`, `rotation` and `align`
// in sheet coordinates. Letters inside a component stay readable, so mirroring moves a text and the side it extends to.
Item placedText(const Item &text,const Item &component);

// --- components
// The components of a sheet in the order they are drawn (also inside groups).
QList<const Item*> components(const Sheet &sheet);
QList<Item*> components(Sheet &sheet);
// The contacts of a component.
QList<const Item*> contacts(const Item &component);
// The component an element belongs to: the component itself, or the one holding the element; nullptr if none.
const Item *componentOf(const Sheet &sheet,const QString &id);
// A component around `children` (in local coordinates) with its insertion point at `pos`. Designator and value texts
// are added when there are none.
// With presets for designator and value (as the reference's "Bauteil erstellen") their fonts are taken and the texts go
// above the component: the value just above it, the designator above the value.
Item makeComponent(QList<Item> children,QPointF pos,const QString &designator,const QString &value,const Item *designatorPreset=nullptr,const Item *valuePreset=nullptr);

// --- texts
// What a text shows: the component's field for designator and value texts, otherwise the text with its variables
// (<PAGENO>, <PAGECOUNT>, <PAGENAME>, <FILENAME>, <FILENAME_PURE>, <FILEPATH>, <DATE>, <TIME>, <VERSION>, <BEZ> or <ID>,
// <WERT> or <VALUE>, <VALEUR>, <Z1>…<Z4>, the column and row of a component <COLNUM>, <COLCHAR>, <ROWNUM>, <ROWCHAR>, for children
// <PARENT_ID>, <PARENT_VALUE>, <PARENT_PAGENO>, <PARENT_Z1>…, <PARENT_CONTACT_n>, <PARENT_COLNUM>…, <CHILDNO>,
// <CHILDCHAR>, for parents <CHILD_PAGENO>, <CHILD_PAGENAME>, <CHILD_COLNUM>… of the first child or with "_n" of the
// n-th, in linked texts <LINK_PAGENO>, <LINK_PAGENAME>, <LINK_TEXT>, <LINK_COLNUM>… of the target and
// <LINKFROM_PAGENO>, <LINKFROM_PAGENAME>, <LINKFROM_TEXT> of the first text linking to it, and the user variables)
// replaced. Unknown names stay as written; variables inside the values of
// user variables are replaced too, at most eight levels deep. A designator is shown with the document's prefix and
// sheet number when they are switched on (see Document); <BEZ>, <ID> and <PARENT_ID> give it so too.
struct TextContext {
    const Document *document=nullptr;
    int sheet=0;
    const Item *component=nullptr;
    QString fileName;
    const Item *text=nullptr;       // the text itself, for the variables of its links
};
QString shownText(const Item &item,const TextContext &context);
// The designator of a component as shown: with prefix and sheet number when the document asks for them.
QString shownDesignator(const Item &component,const TextContext &context);
QString expandVariables(const QString &text,const TextContext &context);

// --- parents and children
// The components of a document with their sheet, in the order of the sheets and their elements (also in groups).
struct PlacedComponent {
    int sheet=-1;
    const Item *item=nullptr;
};
QList<PlacedComponent> allComponents(const Document &document);
PlacedComponent componentWithId(const Document &document,const QString &id);
// The children of a parent, numbered (from 1) in this order.
QList<PlacedComponent> childrenOf(const Document &document,const QString &parentId);
// The texts of a document that can take part in links (on the sheets, also in groups) with their sheet; the text with
// an id; the texts linking to a text.
QList<PlacedComponent> allTexts(const Document &document);
PlacedComponent textWithId(const Document &document,const QString &id);
QList<PlacedComponent> linksTo(const Document &document,const QString &id);
// The column and row of the title block's grid a point lies in, counted from the title block's starts; 0 outside it
// or without a grid.
int columnAt(const Sheet &sheet,QPointF point);
int rowAt(const Sheet &sheet,QPointF point);
// "Auto" of the title block's grid, as in sPlan: the frame 10 mm inside the sheet, fields of about 23 mm (rounded to
// the nearest count, a half to the even one), numbered from 1. The labels of the title block stay as they are.
void autoGrid(Sheet &sheet);

// --- own file format
// JSON with format "OpenLoch Schematic", version 2 (version 1 has no data kept from sPlan files and fewer line ends;
// it is read with its meaning kept); numbers are millimetres and degrees. See docs/modules/schematic.md.
constexpr int formatVersion=13;
QJsonObject toJson(const Document &document);
Document fromJson(const QJsonObject &json);     // throws FormatError for anything malformed or out of range
// One element as in the file, and back (with full checks; without `ids` missing ids are given new ones).
QJsonObject itemToJson(const Item &item);
Item itemFromJson(const QJsonValue &value,bool ids=true);
QByteArray encode(const Document &document);
Document decode(const QByteArray &bytes);
// Own format, or sPlan 7 and 8 (by content).
Document load(const QString &path);
// Own format, read back before it is written atomically; a failed write leaves an existing file unchanged.
void save(const Document &document,const QString &path);
constexpr const char *fileSuffix="olsch";
}
