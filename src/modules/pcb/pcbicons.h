#pragma once
#include <QIcon>
#include <QString>

namespace openloch::pcb {
// Symbols of the PCB tools, drawn in code as pixel art in the style of OpenLoch's other symbols (16-colour palette,
// black outlines, no smoothing), 20 × 20: pcb-track, pcb-pad, pcb-smd, pcb-circle, pcb-area, pcb-measure, pcb-via,
// pcb-other-side. Other names are passed on to OpenLoch's shared symbols.
QIcon pcbIcon(const QString &name);
QStringList pcbIconNames();
}
