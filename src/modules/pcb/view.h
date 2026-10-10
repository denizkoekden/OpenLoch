#pragma once
#include "copper.h"
#include "model.h"
#include <optional>
#include <QImage>
#include <QHash>
#include <QTimer>
#include <QMap>
#include <QWidget>
#include <functional>

class QMimeData;

namespace openloch::pcb {
// The colours of the board on the screen, a scheme of the preferences: the board itself (the background), the grid
// as lines and as dots, the seven layers, through-plated pads and airwires. standard() is the reference's scheme.
struct Colours {
    QColor board,grid,via,airwire,dots;
    std::array<QColor,layerCount+1> layers;     // index = layer
    static Colours standard();
    bool operator==(const Colours &) const=default;
};
// The cross hair at the pointer in the drawing tools: white lines through the whole view (red, apart from a gap around
// it, where the pointer caught a point), grey lines at 45° as well, and the coordinates in a box beside the pointer, in
// large type, on black or white or without a box.
struct Crosshair {
    bool lines=true,diagonals=false,coordinates=false,bigText=false,transparent=false,whiteBox=false;
    bool operator==(const Crosshair &) const=default;
};
// The drawing area of the PCB editor: one board of the document, drawn in layer colours on a black board, with the
// drawing tools of the reference. It edits the document it is given; the editor around it keeps undo steps and files.
class BoardView : public QWidget {
public:
    enum class Tool {Select,Zoom,Track,Pad,Smd,Circle,Rectangle,Area,Keepout,Text,SolderMask,Airwire,Autoroute,Test,Measure};
    explicit BoardView(QWidget *parent=nullptr);
    void setDocument(Document *document);
    Document *document() const{return doc;}
    Board &board();
    // Call after the document changed from outside (undo, property edits, file loads). A drag under way ends as it is,
    // nothing is put back.
    void documentChanged();
    // Ends a drag under way without a change: moved elements and nodes go back where they were. The editor calls it
    // before its own edits and undo steps; another tool, a placement or panning call it as well.
    void cancelDrag();

    void setTool(Tool tool);
    Tool tool() const{return current;}
    // Settings for new elements, as the reference shows them at the left: track width, pad, SMD pad and text, and the
    // clearance to the ground plane ("Freistanzung").
    double trackWidth=.8,padDiameter=1.8,padDrill=.6,smdWidth=.9,smdHeight=1.8,textHeight=2,clearance=.4;
    PadShape padShape=PadShape::Round;
    bool padVia=false,filledRectangle=false;
    int textStyle=1,textThickness=1;
    // Bend modes while drawing tracks and areas (Space switches): straight, straight then 45°, 45° then straight,
    // horizontal then vertical, vertical then horizontal.
    int bendMode=0;
    std::function<void()> bendChanged;
    // Keep-out areas drawn as rectangles instead of polygons, as the reference's switch below offers.
    bool keepoutRectangle=false;
    bool snapToGrid=true;
    // Settings of the reference's "Grundeinstellungen": drill holes in the board colour (0), white (1) or black (2), the
    // ground plane in a darker shade, the ground planes of all copper layers at once, the test tool following airwires,
    // a double click taking an element's sizes for new elements, redundant track nodes removed as tracks are drawn, the
    // colours and the cross hair.
    int holes=0;
    bool darkGround=true,allGrounds=false,testAirwires=false,takeSizes=true,optimizeNodes=true;
    Colours colours=Colours::standard();
    Crosshair crosshair;
    // The grid: shown or not, as lines or as dots, every so many lines (or dots) stronger (0 for none; the reference
    // offers 2, 4, 5 and 10).
    bool gridShown=true,gridDots=false;
    int gridMarking=4;
    std::function<void(const Element&)> sizesTaken;
    // The autorouter: width of its tracks, distance to other copper, nodes on the grid; it reports what it did.
    double autorouteWidth=.8,autorouteClearance=.4;
    bool autorouteOnGrid=true;
    std::function<void(const QString&)> autorouteStatus;
    // Automatic snapping ("automatischer Fangmodus"): near the pointer, pad centres, circle centres and the nodes of
    // tracks and areas catch it before the grid does. Ctrl places freely.
    bool autoSnap=true;
    // Rubber band ("Gummiband") while moving: tracks with a node on a moved pad follow it. 0 off, 1 small catch (the
    // node on the pad's centre), 2 large catch (the node anywhere on the pad's copper).
    int rubberBand=2;
    // Rulers at the top and left in millimetres or mil (a click into their corner switches); the origin is a cross on
    // the board that can be dragged, and the key 0 puts it under the pointer.
    bool milUnits=false;
    static constexpr int rulerSize=18;
    std::function<void()> unitsChanged;
    // A length in the units shown (mm or mil).
    double inUnits(double mm) const{return milUnits?mm/.0254:mm;}
    // View: from below (mirrored left to right, the bottom side on top) and the photo view of the finished board.
    void setFromBelow(bool on);
    bool fromBelow() const{return below;}
    void setPhotoView(bool on);
    bool photoView() const{return photo;}
    // The photo view's options, as the reference's bar above it offers them: the silkscreen shown, the board translucent
    // (the copper of the far side seen through it), the board's colour (0 green, 1 blue, 2 copper) and the finish of
    // the pads (0 gold, 1 silver, 2 none: the copper's colour).
    bool photoSilk=true,photoTranslucent=true;
    int photoBoard=0,photoFinish=0;
    // The colours of the photo view for the board's colour: board, copper under the mask, copper of the far side.
    struct PhotoColours {QColor board,copper,farCopper,finish;};
    PhotoColours photoColours() const;
    // Overlapping elements mixed bit by bit, as the reference's transparent mode shows them: OR on a dark board, AND on a
    // light one, so the grid shows through. Not kept with the document.
    bool transparent=false;
    // The scanned template alone (everything else hidden) or hidden, as the switches below offer.
    bool templateOnly=false,templateHidden=false;
    // The folder of the document: template pictures whose stored path does not exist are looked for there.
    QString documentFolder;
    // The picture of a template file (empty if it cannot be read); pictures are kept once read.
    QImage templatePicture(const QString &file) const;
    // Marks of the design rule check: white hatched frames over the findings shown (all of them at first). A finding can
    // have its elements selected with the view centred on it, or be zoomed to.
    void setFindings(const QList<Finding> &findings);
    void setShownFindings(const QList<int> &indexes);
    QList<int> shownFindings() const{return findingsShown;}
    void showFinding(int index);
    void zoomToFinding(int index);
    // The part of the board in the view (without the rulers), in board coordinates.
    QRectF visibleArea() const;

    QList<int> selection() const{return selected;}
    void setSelection(QList<int> indexes);
    void selectAll();
    // Elements following the pointer until a click puts them down (footprints, pasted elements); positions relative
    // to the pointer, airwires as indexes into the list. `placed` gets the indexes of the new elements.
    void beginPlacement(const QList<Element> &elements,std::function<void(QList<int>)> placed={});
    bool placing() const{return !floating.isEmpty();}
    // The elements the test tool found connected with the point clicked last.
    QList<int> tested() const{return testMarks;}

    // View: millimetres to widget pixels and back.
    void fitBoard();
    void zoomAt(double factor,QPointF pixel);
    // A click of the zoom tool or into the overview zooms in by this much, a right click out, as in the reference.
    static constexpr double zoomStep=1.4;
    void centreOn(QPointF mm);
    // Zooms so that an area of the board fills the view with some room around it.
    void showArea(const QRectF &mm);
    double scale() const{return pixelsPerMm;}
    // Zooming as the reference offers it: the whole working area, all elements on visible layers (the working area when
    // there are none), the selection, and back to the view before the last zoom. Every zoom (also with the wheel, the
    // zoom tool and the overview) keeps the view it leaves, up to 50 of them; a run of wheel steps counts as one.
    void zoomBoard();
    void zoomElements();
    bool zoomSelection();
    bool zoomBack();
    bool canZoomBack() const{return !views.isEmpty();}
    // Keeps the current view for zoomBack() (done by every zoom of the view itself).
    void rememberView();
    // Called whenever the part of the board the view shows has changed (zoom, scrolling, size).
    std::function<void()> viewChanged;
    // Blinking test results ("Grundeinstellungen"): the elements the test tool found blink instead of showing steadily.
    bool blinkTest=false;
    bool testShown() const{return !blinkTest||blinkOn;}
    QPointF toBoard(QPointF pixel) const;
    QPointF toPixel(QPointF mm) const;
    // Where a position snaps: a caught point of an element, else the grid; Ctrl places freely and, as the reference's
    // status line says, Shift snaps to half the grid.
    QPointF snap(QPointF mm) const;
    // The point of the grid nearest to a position, the grid counted from the origin as snap() counts it (without catching
    // points of elements and without Ctrl), or of a grid of `pitch`.
    QPointF onGrid(QPointF mm) const;
    QPointF onGrid(QPointF mm,double pitch) const;
    // Where the pointer shows in the rulers and the status line: as it is with the standard tool, snapped otherwise.
    QPointF shownPointer() const;
    // Where the origin goes when it is set: on the grid counted from the top left corner.
    QPointF originAt(QPointF mm) const;
    // The point of an element that catches the pointer at `mm`, if automatic snapping finds one.
    bool caught(QPointF mm,QPointF *at=nullptr) const;
    // Moves the selection, with the tracks the rubber band takes along, as one undo step.
    void moveSelection(QPointF delta);
    // The topmost element at a board position on a visible layer (the active layer first), or -1; `only` limits the
    // elements looked at.
    int hit(QPointF mm,const std::function<bool(const Element&)> &only={}) const;
    // Whether an element on a visible layer lies at a board position (with a few pixels' tolerance).
    bool hits(const Element &element,QPointF mm) const;
    // The pad (through-hole or SMD) at a board position, or -1.
    int padAt(QPointF mm) const;
    // The board drawn into an image, as on screen without selection marks.
    QImage render(QSize size);
    // Exactly the working area at `pixelsPerMm`, as on screen without marks; without smoothing the edges are hard, so
    // that the picture has the layer colours only (for GIF).
    QImage renderBoard(double pixelsPerMm,bool smooth=true);
    // The same into any painter, the working area from (0, 0) at `pixelsPerMm` units per millimetre (a metafile).
    void paintBoard(QPainter &painter,double pixelsPerMm,bool smooth=true);
    // The milling paths last written, shown over the layout: as hairlines or as wide as the cutter. Plunges show as
    // small crosses.
    void setMillingPaths(const QList<QPolygonF> &paths,const QList<QPointF> &plunges,double cutter);
    bool hasMillingPaths() const{return !millingPaths.isEmpty()||!millingPlunges.isEmpty();}
    void setMillingWide(bool wide){millingWide=wide;update();}
    // The centres of the components with pick and place data and the layers of their designators, for the small
    // crosses. Worked out again only when the board has changed since.
    const QList<std::pair<QPointF,int>> &pickPlaceMarks();
    // Pads and their airwires: the board's connections as pairs of element indexes.
    QList<std::pair<int,int>> airwires() const;
    // Airwires the project's schematic asks for, as pairs of pads: drawn dashed in the airwire colour on the screen, never
    // stored with the board. They hold for the elements they were found for; when elements come or go, they wait for
    // the next comparison.
    void setSchematicAirwires(const QList<std::pair<int,int>> &pads);
    QList<std::pair<int,int>> schematicAirwires() const{return schematicLines;}
    // Whether the schematic wants these two pads connected (set by the editor). The autoroute tool routes the schematic's
    // airwires as well; taking such a route back then leaves no stored airwire, as the schematic shows the line again.
    std::function<bool(int,int)> schematicConnects;
    // Labels on pads (element index, text), as the dialog that assigns pins shows them meanwhile; empty: none.
    void setPadLabels(const QList<std::pair<int,QString>> &labels){padLabels=labels;update();}
    QList<std::pair<int,QString>> shownPadLabels() const{return padLabels;}

    std::function<void()> beforeChange,changed,selectionChanged;
    std::function<void(QPointF)> pointerMoved;
    std::function<void(Tool)> toolChanged;
    // Asked before a new text is placed; fills in the text and returns false to cancel.
    std::function<bool(Element&)> textRequested;
    std::function<void(int)> editRequested;
    // The keys of the modes, chosen freely as in the reference: mode to key (Qt::Key, 0 for none). Esc always ends what
    // the current tool is doing and returns to the standard mode.
    static QStringList modes();                     // in the order of the reference's list
    static QString modeName(const QString &mode);
    static QMap<QString,int> defaultModeKeys();
    QMap<QString,int> modeKeys=defaultModeKeys();
    // The reference's mode keys that are not drawing tools: N special shape, V photo view.
    std::function<void()> shapeRequested,photoRequested;
    // The keys 1 to 9 choose a grid (counted from 1).
    std::function<void(int)> gridKey;
    // Something dragged onto the board (a macro from the library): the elements it stands for, around the origin, empty
    // if it cannot be dropped. They follow the drag without changing the tool or what waits at the pointer, and go down
    // where they are dropped; `dropped` then gets their indexes, once the drop has ended.
    std::function<QList<Element>(const QMimeData*)> dropElements;
    std::function<void(QList<int>)> dropped;
    // A right click on the board in the standard tool, at a global screen position: the popup menu.
    std::function<void(QPoint)> contextMenuRequested;
    // Where the last right click for that menu was (millimetres) and the element under it (-1 for none).
    QPointF contextPoint() const{return contextAt;}
    int contextHit() const{return contextElement;}
    // A right click on a node of the selected track or area (element, node, global screen position): the node's menu.
    std::function<void(int,int,QPoint)> nodeMenuRequested;
    // The nodes of the selected track or area that can be dragged: its own nodes, and halfway along each segment (for an
    // area also on the edge closing it) a virtual node that becomes a node of its own when it is dragged. Virtual nodes
    // are left out on segments too short on the screen to tell them from their ends.
    QList<int> nodeHandles() const;
    QList<std::pair<int,QPointF>> virtualNodes() const;     // index the new node gets, position
    // Text series ("Automatik"): the text with the number `next` follows the pointer, each click puts it down and the
    // number after follows; a right click or Esc ends the series, the text tool stays.
    void startTextSeries(const Element &text,const QString &prefix,int next);
    void endTextSeries(){series=false;update();}
    bool textSeries() const{return series;}
    int nextSeriesNumber() const{return seriesNext;}
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void leaveEvent(QEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dragLeaveEvent(QDragLeaveEvent *) override;
    void dropEvent(QDropEvent *) override;
private:
    Document *doc=nullptr;
    Tool current=Tool::Select;
    double pixelsPerMm=4;
    QPointF offset;             // pixel position of the board origin (of its right edge seen from below)
    bool fitted=false,below=false,photo=false;
    QList<std::pair<double,QPointF>> views;   // the views before the last zooms (scale, centre in millimetres)
    qint64 lastWheel=0;                       // when the wheel zoomed last (milliseconds since the epoch)
    QTimer *blinker=nullptr;bool blinkOn=true;
    QPointF crossAt;            // where the cross hair was drawn last
    QPointF contextAt;int contextElement=-1;
    bool crossOn=false;         // and whether it was
    bool crossTool() const;     // the tool draws or places something: the cross hair shows
    bool smoothing=true;        // edges smoothed (antialiasing)
    QList<std::pair<int,int>> schematicLines;qsizetype schematicCount=-1;   // and the number of elements they hold for
    QList<std::pair<int,QString>> padLabels;
    void viewMoved();
    void zoomBy(double factor,QPointF pixel);
    QList<int> selected,testMarks;
    QList<Finding> findings;
    QList<int> findingsShown;
    int airwireStart=-1;
    // Interaction state
    enum class Drag {None,Move,Band,Node,Pan,Zoom,Circle,Rectangle,Measure,Origin} drag=Drag::None;
    QPointF pressMm,pointerMm,lastPan;
    bool hasPointer=false,moved=false;
    int nodeElement=-1,nodeIndex=-1;
    bool nodeInsert=false;      // the node dragged is a virtual one: it is inserted at nodeIndex once the pointer moves
    bool series=false;QString seriesPrefix;int seriesNext=0;Element seriesText;
    int seriesFirst=0,seriesLast=0;     // the number the series begins with, the highest one it got to
    QList<Element> seriesTexts;         // the texts the series put down, as they went down
    QPolygonF drawing;          // nodes of the track or area being drawn
    mutable QHash<QString,QImage> pictures;
    QList<Element> moving;      // copies of the selected elements before a move
    QPointF pressSnapped;       // where the move started, caught or on the grid
    QMap<int,Element> rubberTracks;         // tracks the rubber band moves, as they were
    QList<std::pair<int,int>> rubberNodes;  // their nodes on moved pads (track, node)
    void catchRubber();
    QList<Element> floating;
    std::function<void(QList<int>)> floatingPlaced;
    // While something is dragged over the board: what waited at the pointer before, back when the drag leaves.
    bool dropping=false;
    QList<Element> stashed;
    std::function<void(QList<int>)> stashedPlaced;
    QList<std::pair<QPointF,int>> pickMarks;
    size_t pickKey=0;
    QList<QPolygonF> millingPaths;
    QList<QPointF> millingPlunges;
    double millingCutter=0;
    bool millingWide=false;
    QPointF mirrored(QPointF mm) const;
    void paint(QPainter &p,const QRectF &target,bool marks);
    void paintPhoto(QPainter &p);
    void paintGround(QPainter &p,int layer,const QColor &colour);
    void paintRulers(QPainter &p);
    void paintPadLabels(QPainter &p);
    bool nearOrigin(QPointF pixel) const;
    void drawElement(QPainter &p,const Element &e,const QColor &colour,double grow=0) const;
    QColor layerColour(int layer) const;
    void change(const std::function<void()> &edit);
    QPolygonF bentPath(QPointF from,QPointF to) const;
    void finishDrawing();
    void addElement(Element e);
    // Puts the elements on the pointer down at a point, one undo step.
    void placeFloating(QPointF at);
    int airwireAt(QPointF mm) const;
    int schematicAirwireAt(QPointF mm) const;
    // The airwire under the pointer for the autoroute tool: a stored one first, else one of the schematic.
    std::optional<std::pair<int,int>> routableAt(QPointF mm,bool *fromSchematic=nullptr) const;
};
}
