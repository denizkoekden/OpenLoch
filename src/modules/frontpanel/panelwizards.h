#pragma once
#include "panelgenerators.h"
#include <functional>

class QWidget;
namespace openloch::frontpanel {
// The scale assistant: style, then construction, labels and design on three pages, with a preview of the result.
// Settings can be saved to and loaded from files; `copy` puts the current scale on the clipboard. False when cancelled.
bool scaleWizard(QWidget *parent,ScaleParameters &parameters,const std::function<void(const Element&)> &copy={});
// The panel cut-out for an instrument: frame (DIN 43700 or own sizes), the milled opening and mounting holes.
// The settings of an instrument can be kept in .CUT files. False when cancelled.
bool cutoutDialog(QWidget *parent,CutoutParameters &parameters);
}
