#pragma once
#include <QCursor>
#include <QIcon>
#include <QString>
namespace openloch {
// OpenLoch's own toolbar symbols and mouse pointers, drawn in code as pixel art in the style of the original's
// Windows-era bitmaps (16-colour palette, black outlines, no smoothing) and scaled without blurring.
// Toolbar icons are 16 × 16, drawing tools 20 × 20, pointers 32 × 32.
QIcon openLochIcon(const QString &name);
// Pointer for a drawing tool or action: lupe, kolben, kolbendraht, kolbenanschluss, kolbenpin, loeten, kolbentest,
// stift, stiftcirc, stiftrect, stiftpoly, stifttext, kreuz, hand, finger.
QCursor openLochCursor(const QString &name);
// The names that have a drawing (for tests).
QStringList openLochIconNames();
QStringList openLochCursorNames();
}
