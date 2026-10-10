#pragma once
#include <QWidget>

namespace openloch::pcb {
class BoardView;
// The overview of the board below the tools, as the reference offers it: the working area in dark green, the part the
// view shows in light green. Dragging the light part moves the view; a click into it zooms in (left button) or out
// (right button) about the middle of the view, a click beside it moves the view there. Seen from below it is mirrored
// like the view.
class BoardOverview : public QWidget {
public:
    explicit BoardOverview(BoardView *view,QWidget *parent=nullptr);
    QSize sizeHint() const override{return {190,130};}
    // The working area and the shown part in the overview's pixels.
    QRectF boardRect() const;
    QRectF shownRect() const;
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
private:
    BoardView *view;
    bool pressed=false,dragged=false,onShown=false;
    QPointF pressAt,grab;       // where the button went down; the view's middle from the board point under it
    // Overview pixels to board millimetres and back.
    QPointF toBoard(QPointF pixel) const;
    QPointF toPixel(QPointF mm) const;
};
}
