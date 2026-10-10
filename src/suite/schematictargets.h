#pragma once
#include "documents/targets.h"
#include <QJsonObject>
namespace openloch::suite {
// The target connections of a schematic document (its data as the project keeps it): its components with designator
// and value, and its nets. A pin is named after its contact, or after its place among the component's contacts when a
// name is missing or comes twice; symbols without designator (ground, supply) only join nets. Throws FormatError for
// data the schematic module does not accept.
documents::Targets schematicTargets(const QJsonObject &schematic);
}
