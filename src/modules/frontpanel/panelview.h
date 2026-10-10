#pragma once
#include "frontpanel.h"
#include "panelrender.h"
#include <QAbstractScrollArea>
#include <QMap>
#include <functional>
#include <optional>

class QTimer;
class QToolButton;
namespace openloch::frontpanel {
class Ruler;
// The working area of the front panel editor: the active panel with rulers, grid and the drawing and editing tools.
// The view changes the document directly between beforeChange() and afterChange(); the editor keeps the history.
class PanelView : public QAbstractScrollArea {
public:
    enum Tool {Select,Rotate,Zoom,Line,Polygon,Rectangle,Circle,Arc,Text,Drill,Dimension,Origin,Place};
    explicit PanelView(QWidget *parent=nullptr);
    void setDocument(Document *document);
    Document *document() const{return doc;}
    // The document was replaced or changed outside the view: forget stale selections and repaint.
    void refresh();

    void setTool(Tool tool);
    Tool tool() const{return current;}
    // Elements that stick to the cursor until a click places them (paste, duplicate, symbols, images, generated objects);
    // their insertion point (or the top left corner) follows the grid.
    void beginPlacement(const QList<Element> &elements);
    bool placing() const{return current==Place;}

    QStringList selection() const{return selected;}
    void setSelection(const QStringList &ids);
    void selectAll();
    void clearSelection();
    // The selected elements wherever they are (also inside groups when chosen in the object tree), in panel order.
    QList<Element*> selectedElements();
    // The last selected element: the reference for aligning (the original aligns to the element marked last).
    QString lastSelected() const{return selected.isEmpty()?QString():selected.last();}
    bool rotateHandles=false;  // the selection shows turning handles instead of stretching handles

    RenderOptions options;
    bool proportional=false;    // stretching keeps the proportions
    // The unit of rulers and coordinates belongs to the panel; changing it is an edit.
    void setInch(bool inch);
    bool inch() const{const frontpanel::Panel *p=panel();return p&&p->inch;}

    double zoom() const{return scale;}
    void setZoom(double pixelsPerMm,std::optional<QPointF> keep={});
    void zoomBy(double factor);
    void fitPanel();
    void fitElements(bool selectedOnly);
    void showArea(const QRectF &area);
    QRectF visibleArea() const;
    void scrollStep(int dx,int dy);
    QPointF toPanel(QPointF view) const;
    QPointF toView(QPointF panel) const;
    QPointF snap(QPointF p,bool free=false) const;

    // Hooks for the editor.
    std::function<void()> beforeChange,afterChange,selectionChanged,zoomChanged;
    std::function<void(Tool)> toolChanged;
    std::function<void(QPointF)> pointerMoved;
    std::function<Element(ElementType)> newElement;            // a new element in the current pen, fill and font
    std::function<void(QPointF)> textRequested,drillRequested;  // the editor asks for text or diameter and adds it
    std::function<void(const QString &id)> propertiesRequested;
    std::function<void(QPoint global,int node)> contextMenuRequested;
    std::function<void(QPointF a,QPointF b,QPointF at)> dimensionRequested;
    // Node editing from the context menu of a node handle.
    void addNode(int index);
    void deleteNode(int index);
    // Circuit boards behind the panel (in a project), drawn over it while they are shown and never printed or exported:
    // the board's outline, the outlines and middles of its parts and their designators, in panel millimetres. Parts on
    // the side away from the panel are drawn fainter.
    struct Underlay {
        QString name;
        QPolygonF outline;
        struct Part {QPolygonF outline;QPointF centre;QString label;bool facing=true;};
        QList<Part> parts;
    };
    void setUnderlay(const QList<Underlay> &boards);
    const QList<Underlay> &shownUnderlay() const{return underlay;}
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void scrollContentsBy(int dx,int dy) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    bool viewportEvent(QEvent *) override;
private:
    friend class Ruler;
    Document *doc=nullptr;
    QList<Underlay> underlay;
    Tool current=Select;
    QStringList selected;
    double scale=4;              // pixels per millimetre
    Ruler *top=nullptr,*left=nullptr;
    QToolButton *unitButton=nullptr;
    // What a mouse drag is doing.
    enum Drag {None,Move,Stretch,Turn,NodeDrag,Band,ZoomBand,Shape,Pan};
    Drag drag=None;
    QPointF pressAt,cursor;      // panel coordinates
    QPoint pressView;
    bool moved=false,pressedSelected=false;
    int handle=-1,node=-1;
    QMap<QString,Element> originals;  // selected elements as they were when the drag began
    QRectF originalBounds;
    QPolygonF points;            // clicks of the line, polygon, arc and dimension tools
    QList<Element> placement;
    QPointF placementReference;
    // Scrolling by itself while the pointer rests at the edge during a drag or while placing.
    QTimer *scroller=nullptr;QPointF lastView;Qt::KeyboardModifiers lastModifiers;
    void autoScroll();
    void updateScrollBars();
    QPointF origin() const;       // view position of the panel's top left corner
    frontpanel::Panel *panel() const;
    QString hit(QPointF p) const;
    int handleAt(QPointF view) const;
    int nodeAt(QPointF view) const;
    Element *singleContour();
    QRectF selectionBounds();
    QList<QPointF> handlePoints(const QRectF &bounds) const;
    void beginDrag(Drag kind);
    void applyDrag(QPointF to,Qt::KeyboardModifiers modifiers);
    void finishShape();
    void cancelShape();
    void add(const Element &e,bool select=true);
    void changed();
    void setCursorFor(QPointF view);
    QList<Element> placed(QPointF at,bool free) const;
};
}
