#pragma once
#include "strokefont.h"
#include <QByteArray>
#include <QHash>
#include <QList>
#include <functional>
#include <optional>

// FrontDesigner's conversion of an AutoCAD shape font into letters of its own font format (FHX): it reads the font's
// source text (.shp) and computes each character's lines and arcs in its arithmetic, the first time it uses a font it
// has no FHX file for.
namespace openloch::frontdesigner {
// A number of a shape as the original reads it and as AutoCAD reads it; they differ for a negative number with a
// leading 0, which is hexadecimal for AutoCAD and decimal for the original.
struct ShapeNumber {int value=0,autocad=0;};
// The numbers of a shape by its number: empty for a shape that is not there, nullopt where the original stops with an
// error.
using ShapeNumbers=std::function<std::optional<QList<ShapeNumber>>(int shape)>;
// From a font's source text as the original reads it: the lines after the first header line ("*number,…") with that
// number, up to the next line beginning with an asterisk, joined and split at commas, brackets dropped. A number with
// a leading 0 counts as two hexadecimal digits (the two after the 0, or the 0 and the one after it).
ShapeNumbers shpNumbers(const QByteArray &source);
// From a compiled font (.shx), which the original first turns into source text: the bytes as numbers, signed where the
// command takes them signed (displacements and bulges in two's complement, octant values as sign and magnitude).
ShapeNumbers shxNumbers(const frontpanel::StrokeFont &font);
// The letters for the character codes 1 to 255 in Windows order (the original moves the letters of the DOS code page
// there, see shapeNumberFor; without `dosOrder` the shape numbers are the codes), with the advance rounded to whole
// font units; nullopt when the original would stop with an error. Computed
// with the x87 unit at 53 bits, like the FHX files that come with the original. Octant and fractional arcs are made as
// AutoCAD defines them, not as the original does: their octant value read as AutoCAD reads it, the direction from its
// sign, a count of 0 as the full circle, the end of a fractional arc in the last octant it reaches.
std::optional<QHash<int,frontpanel::StrokeGlyph>> shapeLetters(const ShapeNumbers &numbers,bool dosOrder=true);
// The letters the original plots a font with: those of its own format, else those it makes of the shapes in the font's
// character order (made once and kept with the font); nullptr when there are none (fonts by Unicode and big fonts, text
// it cannot read).
const QHash<int,frontpanel::StrokeGlyph> *originalLetters(const frontpanel::StrokeFont &font);
}
