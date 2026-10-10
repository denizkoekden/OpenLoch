#pragma once
#include <QCursor>
#include <QIcon>
#include <QString>
#include <QStringList>

namespace openloch::frontpanel {
// Symbols of the front panel editor: its own pixel drawings (16 × 16 for actions, 20 × 20 for tools) in the style of
// the program's other symbols, and the shared ones from openLochIcon() for everything both editors have.
QIcon panelIcon(const QString &name);
QCursor panelCursor(const QString &name);
QStringList panelIconNames();
}
