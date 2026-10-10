#pragma once
#include "model.h"
#include "shapes.h"
#include <QPointer>
#include <QWidget>
#include <functional>

// The drawing area of the schematic editor: one sheet of the document on the grey desk, with rulers, the snap grid
// and the drawing modes of the reference. It edits the document it is given; the editor around it keeps undo steps.
namespace openloch::schematic {
class SheetView : public QWidget {
public:
    enum class Tool {Select,Line,Junction,Rectangle,Ellipse,Polygon,Text,TextBox,NetLabel,SheetReference,Contact,Zoom,Bezier,Freehand,Special,Measure,Dimension};
    explicit SheetView(QWidget *parent=nullptr);
    void setDocument(Document *document);
    Document *document() const{return doc;}
    // The elements edited: those of the active sheet, or of its title block while that is edited.
    QList<Item> &items();
    const QList<Item> &items() const;
    void setTitleBlockMode(bool on);
    bool titleBlockMode() const{return titleMode;}
    // While a component is edited ("Bauteileditor"), only its elements are shown and edited, in its own coordinates
    // with the insertion point at the origin. Empty: the sheet.
    void setComponentMode(const QString &componentId);
    QString componentMode() const{return editedId;}
    Item *editedComponent() const;
    // Call after the document changed from outside (undo, property edits, files, another sheet).
    void documentChanged();

    void setTool(Tool tool);
    Tool tool() const{return current;}
    // Snapping, as the switches in the status bar: grid ("Rasterfang", Ctrl places freely), 45° while drawing lines
    // ("Winkelfang", Shift), the nodes of lines and the connection points of contacts ("Anschlussfang", Alt), and
    // conductors following moved components ("Gummiband"). Turning by hand snaps to `rotationSnap` degrees.
    bool gridSnap=true,angleSnap=true,terminalSnap=true,rubberBand=true;
    // The point rulers and coordinates count from ("Ursprung", the button between the rulers): a corner of the sheet; the
    // axes keep their directions, as in the reference.
    enum class Origin {TopLeft,BottomLeft,TopRight,BottomRight};
    Origin origin=Origin::TopLeft;
    QPointF originPoint() const;
    // The sizers of the selection in sheet coordinates (none without a selection or outside the Standard mode), and
    // whether they are arrows for turning and shearing.
    QList<QPointF> sizers() const;
    bool turning() const{return turnMode&&turnFor==selected;}
    // Rectangles and circles drawn as a frame from a corner or from their centre (the choice at the tool's button);
    // Shift held while drawing makes a square or a circle.
    bool rectanglesFromCentre=false,ellipsesFromCentre=false;
    // The frame of the rectangle, circle or text box being drawn from `from` to `to`.
    QRectF shapeFrame(QPointF from,QPointF to,Qt::KeyboardModifiers modifiers) const;
    // How the grid shows ("Grundeinstellungen", Raster): its strength in percent, a cross every `gridMarks` points (none
    // below 2), lines instead of dots, and drawn over the title block instead of under it. It always lies under the
    // circuit and is hidden where its points would come closer than seven pixels.
    int gridContrast=100,gridMarks=5;
    bool gridLines=false,gridOverTitleBlock=false;
    // "Weißer Hintergrund" (Grundeinstellungen › Anzeige): the sheet pure white instead of sPlan's slight tint.
    bool whiteBackground=false;
    // The texts of a component that is not selected (designator, value, contacts) are dragged on their own; with
    // `componentTextsWithKey` only while exactly the modifiers `componentTextKey` are held (sPlan: Alt), otherwise a
    // click on them takes the whole component ("Grundeinstellungen").
    bool componentTextsWithKey=false;
    Qt::KeyboardModifiers componentTextKey=Qt::AltModifier;
    // The text of a component under the last context menu (index among its children), or -1.
    int contextComponentText=-1;
    // "Bauteiltext verschieben": the text follows the pointer until a click puts it down; Esc or the right button puts
    // it back. Works without any modifier.
    void startComponentTextMove(const QString &componentId,int text);
    bool componentTextFollows() const{return textFollows;}
    double rotationSnap=30;
    // How texts follow when elements are turned or mirrored: completely, or only their place (letters readable).
    bool textsTurned=true,textsMirrored=false;
    // Where the last click was, for the distances in the status bar.
    QPointF pressPoint() const{return pressSheet;}
    // The settings for new elements ("Voreinstellungen").
    Item linePreset,shapePreset,junctionPreset,textPreset,labelPreset;
    Item textBoxPreset;     // text boxes have their own, as in the reference
    // The reference's presets of component texts: designator and value of components made from a selection, contacts
    // put down in the component editor.
    Item designatorPreset,valuePreset,contactPreset;
    // The special shape drawn with Tool::Special, and the settings of those that have some.
    SpecialShape special=SpecialShape::RegularPolygon;
    SpecialShapeOptions specialOptions;
    // New dimensions: their kind and settings. A length takes the two points and a third click for the place of the
    // dimension line; a radius or diameter two points; an angle the vertex, a point on the first leg (its radius)
    // and one on the second.
    Item dimensionPreset;
    // The dimension the clicks so far make, with the pointer as the next point.
    Item dimensionDraft(const QPolygonF &clicks,QPointF pointer) const;

    // The selection: top-level elements of items() by id.
    QStringList selection() const{return selected;}
    // An element inside a group, chosen alone with Alt as in the reference: its properties change without ungrouping,
    // it is neither moved nor deleted on its own. Null when `id` is no element of a group of the edited elements.
    Item *nestedItem(const QString &id);
    bool nestedSelection(){return selected.size()==1&&!itemById(selected[0])&&nestedItem(selected[0]);}
    void setSelection(const QStringList &ids);
    void selectAll();
    // Elements following the pointer until a click puts them down (library symbols, pasted elements), in coordinates
    // relative to the pointer. `placed` gets the ids of the new elements.
    void beginPlacement(const QList<Item> &items,std::function<void(QStringList)> placed={});
    bool placing() const{return !floating.isEmpty();}
    void turnPlacement(double degrees);

    // View: millimetres to widget pixels and back.
    void fitSheet();
    void fitItems(bool selectedOnly);
    void zoomAt(double factor,QPointF pixel);
    // Zooms by `factor` and puts the sheet point under `pixel` in the middle of the drawing area (zoom mode and wheel).
    void zoomCentred(double factor,QPointF pixel);
    double scale() const{return pixelsPerMm;}
    // The zoom as sPlan counts and shows it: pixels per tenth of a millimetre, from 0.05 to 30; a step of the wheel or of
    // "Vergrößern"/"Verkleinern" changes it by 1.2, a click in the zoom mode by 1.4.
    double zoom() const{return pixelsPerMm/10;}
    static constexpr double minScale=.5,maxScale=300,wheelStep=1.2,clickStep=1.4;
    QPointF toSheet(QPointF pixel) const;
    QPointF toPixel(QPointF mm) const;
    // A sheet position snapped as the switches and keys say; `from` is the previous point of a line being drawn.
    QPointF snapped(QPointF mm,Qt::KeyboardModifiers modifiers,const QPointF *from=nullptr) const;
    QPointF onGrid(QPointF mm) const;
    // The connection point nearest to `mm` within a few pixels: a node of a line, a junction, a contact's connection
    // point; false if there is none.
    bool terminalAt(QPointF mm,QPointF *at) const;
    // Moves the selection, with the conductors the rubber band takes along, as one undo step.
    void moveSelection(QPointF delta);
    // The topmost element of items() at a sheet position, or -1.
    int hit(QPointF mm) const;
    bool hits(const Item &item,QPointF mm) const;
    // The sheet drawn into an image as on screen without marks.
    QImage render(QSize size);
    static constexpr int rulerSize=20;

    std::function<void()> beforeChange,changed,selectionChanged;
    std::function<void()> changeCancelled;     // a change begun with beforeChange was taken back (Esc)
    std::function<void(QPointF)> pointerMoved;
    std::function<void()> zoomChanged;
    std::function<void(Tool)> toolChanged;
    // Asked before a new text, net label or contact is placed; fills in the text and returns false to cancel.
    std::function<bool(Item&)> textRequested;
    std::function<void(const QString&)> editRequested;
    std::function<void(QPoint,const QString&,int)> contextMenuRequested;   // global position, element id, node (-1)
    std::function<void(const QString&)> hintChanged;
    // The arrows of text links, as in sPlan: over the outer arrow of a link a small window shows its target, over the
    // inner arrow of a target the texts linking to it; a double click on an arrow asks for the jump (`outgoing`: to the
    // target, else to the first text linking here).
    std::function<void(const QString &text,bool outgoing)> linkArrowActivated;
    // The text whose link arrow lies at `mm` on the current sheet (also in groups), with the arrow's direction; empty
    // when there is none. And what the small window over an arrow shows.
    QString linkArrowAt(QPointF mm,bool *outgoing=nullptr) const;
    QString linkWindowText(const QString &text,bool outgoing) const;
    // A library entry dropped from the library panel: its symbol at the drop position, a parent with its children.
    std::function<bool(const QString&,QList<Item>*)> libraryItems;   // a symbol and, for a parent, its children
    QString fileName;               // for <FILENAME> in texts
    bool parentChildColours=false;  // parents and children coloured, the selected ones' relatives stronger
    // Shows a point of the sheet in the middle of the view, at the same zoom.
    void centreOn(QPointF point);
    // The node of a line or polygon under the pointer for the context menu ("Knoten entfernen", "Linie auftrennen").
    void removeNode(const QString &id,int node);
    void splitLine(const QString &id,int node);
    void joinLines();
    // Magnetic guide lines of the sheet ("Magnetlinien"): drawn on the sheet (not in the component editor), they
    // attract what is moved, drawn or placed within a few pixels. New ones are dragged from the rulers, existing ones
    // moved by dragging and removed by dragging them onto a ruler or choosing one (it turns red) and deleting it.
    // Fixed, they can be neither added, moved nor deleted; hidden, they neither show nor attract.
    bool guidesFixed=false,guidesHidden=false;
    bool guidesShown() const;
    // A new guide line: vertical at x, or horizontal at y; false while guides are fixed or hidden.
    bool addGuide(bool vertical,double at);
    // Deletes the chosen guide line; false if none is chosen.
    bool deleteChosenGuide();
    bool guideChosen() const{return chosenGuide>=0;}
    static constexpr int magnetPixels=8;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool event(QEvent *event) override;              // pinching on a touchpad zooms
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool focusNextPrevChild(bool next) override;     // Tab chooses elements instead
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    Document *doc=nullptr;
    bool linkTipShown=false;
    bool titleMode=false;
    QString editedId;
    QList<Item> emptyItems;
    Tool current=Tool::Select;
    QStringList selected;
    double pixelsPerMm=3;
    void setScale(double pixels);                    // within sPlan's limits, told by zoomChanged
    // The rounding changer of a rectangle stands at least 10 pixels from the corner, so that the corner's handle stays
    // free; while it is dragged the rounding follows it from where it was taken (`roundingGrab` the distance beyond).
    double roundingShown(double rounding,double width) const{return std::min(std::max(rounding,10/pixelsPerMm),std::abs(width));}
    double roundingGrab=0;
    QPointF offset{40,40};
    bool fitted=false;
    // Interaction state
    enum class Drag {None,Pan,Move,Rubber,Node,Corner,Shape,Anchor,Guide,Freehand,Measure,Special,ComponentText,Segment,ArcAngle,Rounding,Sizer};
    QPointF anchorAt;               // the insertion point being dragged in the component editor
    Drag drag=Drag::None;
    QPointF pressPixel,pressSheet,lastSheet,anchor,moved;
    QString dragId;
    QPointF segmentFrom,segmentTo;  // the ends of the segment moved with Alt, as they were
    // The eight sizers around the selection (corners and the middles of the sides, clockwise from the top left); a
    // second click on a selected element turns them into arrows that turn (corners) and shear (sides).
    bool turnMode=false,clickedSelected=false;QStringList turnFor;int sizer=-1;QRectF sizerFrame;
    int dragNode=-1;
    QPointF textStart;              // where a dragged component text was, in the component's coordinates
    bool textFollows=false;         // the component text follows the pointer without a button held
    bool insertNode=false;          // the dragged node is a virtual one, inserted at `dragNode` when it first moves
    bool changedDuringDrag=false;
    QList<Item> before;             // the elements as they were when a drag began
    QPolygonF drawing;              // nodes of a line or polygon being drawn
    QPointF pointer;
    bool pointerInside=false,caughtTerminal=false;
    QPointF caughtAt;
    // The chosen guide line (index, vertical or not) and the one being dragged (-1: a new one from a ruler).
    int chosenGuide=-1;bool chosenVertical=false;QString chosenSheet;
    void chooseGuide(int index,bool vertical);
    QList<double> &guides(bool vertical);
    int guideIndex=-1;bool guideVertical=false;double guideAt=0;
    // The guide line within a few pixels of a widget position: its index, -1 if none.
    int guideAtPixel(QPointF pixel,bool *vertical) const;
    QList<Item> floating;
    std::function<void(QStringList)> floatingPlaced;
    void finishDrawing(bool close);
    void addItem(Item item);
    void startChange();
    void finishChange();
    void cancelChange();            // the elements are back as before the drag
    QRectF sheetRect() const;
    Item *itemById(const QString &id);
    // Handles of a single selected element: nodes of lines and polygons, corners of rectangles and the like.
    QList<QPointF> handles(const Item &item) const;
    int handleAt(const Item &item,QPointF pixel) const;
    // "Virtuelle Knotenpunkte": the middle of each segment of a line or polygon; dragged, one becomes a node.
    QList<QPointF> virtualNodes(const Item &item) const;
    // The shown text of a component under a point (index among its children), or -1.
    int componentTextAt(const Item &component,QPointF mm) const;
    // What a text of a component on the current sheet shows (designator with prefix and sheet number, variables).
    QString componentText(const Item &component,const Item &text) const;
    int virtualNodeAt(const Item &item,QPointF pixel) const;
    // Connection points of a component on the sheet: its contacts' and the free ends of its own lines.
    QList<QPointF> connectionPoints(const Item &component) const;
    void paintRulers(QPainter &painter);
    void paintGrid(QPainter &painter,const QRectF &area);
    void setHint(const QString &hint);
};
}
