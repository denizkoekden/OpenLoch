#pragma once
#include "model.h"
#include <QFont>
#include <QPolygonF>
#include <QRectF>
#include <QStringList>
#include <QTransform>

// Laying out texts in millimetres, the same for the screen, pictures and print.
namespace openloch::schematic {
// A font drawn at a pixel size of 100, scaled by textScale(font) to the line height `font.height`.
QFont qtFont(const Font &font);
double textScale(const Font &font);
struct TextLayout {
    QStringList lines;
    QList<double> widths;   // of each line
    double width=0,lineHeight=0,ascent=0;
};
TextLayout layoutText(const QString &text,const Font &font);
// From a text's own frame (x along its lines, y down, origin at `pos`) to the coordinates it lives in.
QTransform textFrame(const Item &text);
// The rectangle the text covers in its own frame, and its corners where it lives.
QRectF textRect(const Item &text,const QString &shown);
QPolygonF textOutline(const Item &text,const QString &shown);
}
