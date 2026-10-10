#pragma once
#include <array>
#include <optional>
#include <QJsonObject>
#include <QRectF>
#include <QTransform>
#include <QList>
#include <QPolygonF>
#include <QString>
#include <utility>
namespace openloch {
QRectF legacyBounds(const QJsonObject &node);
// The area of an object's drawing without its texts, in its own coordinates: the middle of a part as a front panel takes
// it. Empty when it draws nothing else.
QRectF drawingBounds(const QJsonObject &node);
// The side an object lies on: true for the solder side. The first of its parts with a side decides (tracks, eyes, holes,
// separators and potential marks have none); nullopt when none has one.
std::optional<bool> solderSide(const QJsonObject &node);
// The anchor of a part: the middle of its bounds on the 2.54 mm grid, with its labels' boxes estimated from their text.
// Placements are saved relative to it, so the stored corners of labels (which make them hit where they are drawn) do
// not move it.
QPointF componentAnchor(const QJsonObject &node);
// The box of a LochMaster text label from its stored corners: P0 (text_position, top left), P1, P2 and P3 (from
// text_anchors). Without stored corners nullopt: old files, fresh assistant labels, a collapsed box (P2 at P0) and
// corners all at the zero point while the label is not, which is no box either.
std::optional<std::array<QPointF,4>> labelStoredCorners(const QJsonObject &label);
QTransform objectTransform(const QJsonObject &properties,QPointF center);
QString componentText(const QString &text,const QJsonObject &group,const QString &title={});
QString componentId(const QJsonObject &group);
QList<QPointF> connectionPoints(const QJsonObject &node);
// The same points, each with the "label" of the object it belongs to: the name a library gives the pin of a part.
QList<QPair<QPointF,QString>> namedConnectionPoints(const QJsonObject &node);
QList<QPointF> boardHolePoints(const QJsonObject &node);
// The terminals of a part (leads, pins and wires of kinds 1, 9 and 11) in the order of its children, at their first
// point. LochMaster turns a part about its first terminal.
QList<QPointF> partTerminals(const QJsonObject &group);
// "Bezugspunkt": the part with its terminal `index` (in the order of partTerminals) first among its children, so that
// LochMaster turns it about that terminal and OpenLoch places that terminal on a hole.
QJsonObject withReferenceTerminal(QJsonObject group,int index);
// The outline LochMaster draws for a TDraht path: kinds 6 and 7 closed, corners smoothed when "flag2" is set
// ("style" 0 curve through the edge midpoints, 1 chamfer, 2 rounded corner; "rotation" holds the corner size).
QPolygonF drawnPath(const QJsonObject &wire);
// The original's object tree text, also used by the parts list: the type name (or the object's own name) and a detail
// like "(L=7,62 mm Lochabstand 3)", "(4 Knoten)", "(2,54 x 7,3 mm)" or the quoted text; "R1: Name" for parts.
std::pair<QString,QString> objectDescription(const QJsonObject &node);
}
