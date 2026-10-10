#pragma once
#include "panelboards.h"
#include <QJsonObject>
#include <QList>
#include <QString>
namespace openloch::suite {
// The boards of a perfboard or circuit board document as a front panel sees them (frontpanel::BoardSource): each board
// in millimetres from its top left corner with its components, their middle (the perfboard: of the drawing without
// texts, else of the pins; the circuit board: the pick and place centre), their drawing without texts and their side.
// `document` and `name` are the document's identifier and name in the project. An older document without identifiers
// gets them in `data`, once, so that a front panel can name its boards and parts for good. Throws FormatError for data
// the module does not accept.
QList<frontpanel::BoardSource> perfboardSources(const QString &document,const QString &name,QJsonObject &data);
QList<frontpanel::BoardSource> circuitBoardSources(const QString &document,const QString &name,QJsonObject &data);
}
