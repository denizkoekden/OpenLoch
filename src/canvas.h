#pragma once
#include <QMap>
#include "project.h"
#include "continuity.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QImage>
#include <functional>
#include <optional>
class QPrinter;
class QMimeData;

namespace openloch {
// Draws one stored node at `scale` pixels per 1/100 mm, transparent outside the drawing.
// `bitmaps`: BMP-Rendering as the view has it (pictures of bodies and shaded solder joints, else their average colour).
QImage renderNode(const QJsonObject &node,const QByteArray &source,double scale,qreal ratio=1,bool bitmaps=true);
struct PrintView;
// The board as LochMaster prints a print view, in board coordinates (1/100 mm): fill, copper, cuts and the solder marks
// at lead ends, holes, objects, the outline (always) and the rulers, each layer as the view switches it.
void paintPrintBoard(QPainter &p,const Project &project,const PrintView &view,bool printer);
class Canvas : public QGraphicsView {
public:
    explicit Canvas(QWidget *parent=nullptr);
    void setProject(Project *p);
    void rebuild();
    void fit();
    // Zoom like the original: onto all objects, onto the marked ones, and real size (1:1 on this screen).
    void fitObjects(bool selectedOnly);
    // Shows an area of the document, such as the board with parts beside it.
    void fitArea(const QRectF &area);
    // Zooms by `factor` keeping the point of the board under `at` (viewport pixels) where it is (docs/suite.md, "Zoom
    // und Touchpad"): Strg/⌘ and scrolling, and a pinch on a touchpad.
    void zoomAbout(double factor,QPointF at);
    void realSize();
    void setTool(const QString &name);
    // A fixed snap step in 1/100 mm instead of the board's grid for the unit (0: no snapping); below 0 back to the unit's.
    void setGrid(double units);
    // The snap step: as in the original the unit's grid of the board (N: hole pitch, mm and inch: their grids).
    double gridStep() const{return fixedGrid>=0?fixedGrid:project?project->snapGrid(units):254;}
    // Snapping happens unless Shift is held or the grid is below 0.01 mm, as in the original.
    bool snapping() const{return gridStep()>=1&&!(pointerModifiers&Qt::ShiftModifier);}
    void removeSelected();
    bool exportImage(const QString &path,QString *error=nullptr);
    bool exportPdf(const QString &path);
    void setShowComponents(bool visible);
    // LochMaster's view switches, kept per board: Wenden mirrors top to bottom and makes the solder side active, Durchsicht makes the other
    // side active without mirroring; objects of the inactive side are x-rayed (Röntgenblick, on by default) or hidden.
    // BMP-Rendering draws fill pictures, S/W shows everything in black, white and grey.
    // sides: boards only. The print preview adds texts (Texte), potential markers (only with Potenziale), upright text in
    // a vertically flipped view (textMirror 2), outline-only x-ray on paper and one pass of holes (1), cuts (2) or all
    // other objects (3).
    struct ViewState {bool flip=false,through=false,xray=true,bitmaps=true,mono=false,sides=true,texts=true,potentialMarkers=true,printer=false;int textMirror=0,pass=0;bool backActive() const{return flip!=through;}};
    void setViewState(const ViewState &state);
    ViewState viewState() const{return views;}
    // Wenden turns the board over top to bottom, as the original does; texts stay upright (their glyphs are turned back).
    ViewState effectiveView() const{auto v=views;v.sides=project&&project->mode=="board";v.textMirror=v.flip?2:0;return v;}
    // Röntgenkontrast: grey levels of the x-ray fill and outline (defaults 0xB9 and 0xD0).
    void setXRayGrey(int fill,int outline);
    // Older single-choice API: front, back (Wenden), xray (Durchsicht), outline (S/W).
    void setViewMode(const QString &mode);
    void setRulers(bool enabled);
    // Ruler and coordinate unit like the original: 0 mm, 1 inch, 2 N (holes of the 2.54 mm pitch from the origin).
    void setUnit(int unit);int unit() const{return units;}
    void beginPlacement(const QString &library,int index);
    void rotateSelected(double degrees);
    QPointF selectionPivot() const;
    // The original's outline buttons on the selected outlines (groups are not entered): style -1 switches smoothing off
    // keeping style and size, 0 B-spline, 1 chamfer and 2 rounding with `size` in 1/100 mm. Milling toggles flag3 on
    // lines, areas and circles (kinds 4, 6, 7, TKreis).
    void setSmoothing(int style,double size);
    // "Normale Kontur" and "Kontur fräsen".
    void setMilling(bool on);
    // The original's Breite, Farben and Füllen toolbars on the selected objects (groups and parts keep theirs): mode 0 the
    // line width in 1/100 mm (0 = unsichtbar), 1 the line colour, 2 "Flächen füllen", 3 the fill colour, which also
    // removes a fill picture. Wires, leads, lines, pins and potential markers never fill, solder blobs always do.
    void applyStyle(int mode,const QVariant &value);
    // "Auf andere Platinenseite setzen": switches the board side of the selected objects, of groups with all they contain.
    void switchSide();
    // Context menu of an outline handle: insert a point halfway to the previous one, or delete it (an outline with two
    // points is deleted as a whole).
    void addNode(int index);
    void deleteNode(int index);
    // Fill pictures like the original: a picture (BMP bytes) on the selected areas (outlines of kind 6/7, OpenLoch
    // rectangles and polygons), stretched over the bounding box grown by half the line width; turning rotates the
    // pixels 90° clockwise and keeps the corners.
    // A picture with transparent parts brings the outline of its opaque part (fractions of the picture, see
    // pictureFill): the areas take it as their outline, rectangles become polygons.
    void setBitmapFill(const QByteArray &bmp,const QPolygonF &outline={});
    void rotateBitmapFill();
    void mirrorSelected(bool horizontal);
    void duplicateSelected();
    void selectAll();
    void clearSelection();
    void moveSelected(QPointF delta);
    void alignSelected(const QString &edge);
    void alignSelectedToBoard();
    void reorderSelected(bool front);
    void renumber();
    void groupSelected(bool dissolve=false);
    // Replaces the one selected object by the first object of `bytes` (a library document in board coordinates), moved by
    // `move` and with `properties` set; one undo step (a scaled group).
    void replaceSelected(const QByteArray &bytes,const QString &name,QPointF move,const QJsonObject &properties);
    QJsonArray selectedPath() const;
    // Points of the single selected outline in scene coordinates, shown as handles in the select tool.
    QList<QPointF> nodeHandles() const;
    void editPath(const QJsonArray &points);
    void finishContour();
    bool printTo(QPrinter &printer);
    QByteArray selectionData() const;
    void pasteData(const QByteArray &data);
    void editSelected(const QJsonObject &properties,QPointF move={});
    QPointF selectedCentre() const;
    QJsonObject selectedNode() const;
    int selectionCount() const;
    QList<QPointF> connectionTargets() const;
    void selectObject(const QString &kind,int index);
    // Selects several objects given as (kind "legacy"/"new", index), and lists the selected ones the same way.
    void selectObjects(const QList<std::pair<QString,int>> &objects);
    QList<std::pair<QString,int>> selectedObjects() const;
    // Continuity tester: marks everything connected to the copper at `at` without changing the project.
    QList<Conductor> traceContinuity(QPointF at);
    // LochMaster's "Potenziale anzeigen": each net in the colour of its first potential marker.
    void setShowPotentials(bool shown);
    bool potentialsShown() const{return showPotentialsEnabled;}
    // LochMaster's "Freie Bereiche anzeigen": copper without any terminal, shown while the button is held.
    void setShowFreeAreas(bool shown);
    // OpenLoch short check: highlights the chain of conductors joining two potentials.
    void showShort(const QList<Conductor> &chain);
    void clearShort();
    int markedShortItems() const{return int(shortMarks.size());}
    // The schematic's target connections: lines between pins still to be joined ("Luftlinien") and names shown at pins
    // while they are assigned. They stay until replaced; empty lists remove them.
    void setAirwires(const QList<QLineF> &lines);
    void setPinNames(const QList<QPair<QPointF,QString>> &names);
    int markedAirwireItems() const{return int(airwireMarks.size());}
    std::function<void()> beforeChange, changed;
    std::function<void()> propertiesRequested;
    // Right click with the selection tool: the object under the pointer is selected first, then the context menu opens.
    std::function<void(QPoint global)> contextRequested;
    std::function<void(const QString &)> toolChanged;
    std::function<void(QPointF)> pointerMoved;
    std::function<void(int copper,int wires)> continuityTraced;
    std::function<void(int conflicts)> potentialsComputed;
    std::function<bool(QString &name,QColor &colour)> potentialRequested;
    QString activeTool="select";
    // Values merged into objects the current tools create (pad and drill diameters, track width), and a check that may
    // edit or cancel a new object before it is added (the layout editor's drill dialog).
    QMap<QString,QJsonObject> toolDefaults;
    std::function<bool(QJsonObject&)> confirmNew;
    bool backActive() const{return views.backActive();}
    const Project *document() const{return project;}
    static constexpr const char *libraryPartMime="application/x-openloch-library-part";
protected:
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dropEvent(QDropEvent *) override;
    void showEvent(QShowEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    bool viewportEvent(QEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void drawBackground(QPainter *,const QRectF &) override;
    void drawForeground(QPainter *,const QRectF &) override;
private:
    Project *project=nullptr;
    QGraphicsScene content;
    double fixedGrid=-1; // set by setGrid(); otherwise the board's grid of the unit
    Qt::KeyboardModifiers pointerModifiers; // of the last mouse or drag event: Shift switches snapping off
    bool showComponents=true,dragging=false;
    ViewState views;
    bool rulers=false;int units=2; // 0 mm, 1 inch, 2 N (holes of the pitch, counted from the origin), N first as in the original
    QPointF start;
    bool hasStart=false;
    QJsonArray contour;
    QGraphicsPathItem *preview=nullptr;
    QGraphicsItem *placementPreview=nullptr;
    QJsonObject placement;
    QList<QPointF> boardHoles;
    bool generatedBoard=false,fitPending=false;
    QByteArray lastPaste;
    int pasteStep=0;
    bool moveItem(QGraphicsItem *item,QPointF delta);
    QPointF snap(QPointF p) const;
    std::optional<QPointF> nearestBoardHole(QPointF p) const;
    QPointF placementPosition(QPointF p) const;
    void placeAt(QPointF at);
    void movePlacementPreview(QPointF at);
    bool acceptsPart(const QMimeData *data) const;
    QGraphicsItem *boardItem=nullptr;
    QList<QGraphicsItem*> continuityMarks,potentialMarks,freeMarks,shortMarks,airwireMarks;
    QList<QLineF> airwireLines;
    QList<QPair<QPointF,QString>> pinNameMarks;
    void paintAirwires();
    int nodeDrag=-1;QPointF nodeTarget;QPoint zoomStart;
    QTransform nodeTransform(QGraphicsItem **item) const;
    void transformSelection(const QTransform &map,const std::function<void(QJsonObject&)> &change);
    QList<Conductor> shortChain;
    bool showPotentialsEnabled=false,showFreeEnabled=false;
    QList<QPair<QPointF,double>> markedHoles;
    void showPotentials(const QList<Conductor> &coloured);
    QList<QGraphicsItem*> paintConductors(const QList<Conductor> &conductors,const QColor &colour,double copperZ,double wireZ);
    std::optional<QPointF> continuityPoint;
    void clearContinuity();
    void showContinuity(const QList<Conductor> &found);
    bool alignComponentItems();
    void addElement(const QJsonObject &node);
    void renderTo(QPainter &p,const QRectF &target);
};
}
