#pragma once
#include <QIcon>
#include <QStringList>

namespace openloch::schematic {
// The schematic editor's own symbols, drawn as pixel art like OpenLoch's other symbols (src/icons.cpp); names
// without a drawing of their own fall back to openLochIcon.
QIcon schematicIcon(const QString &name);
QStringList schematicIconNames();
}
