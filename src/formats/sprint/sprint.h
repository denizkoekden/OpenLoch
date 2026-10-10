#pragma once
#include "modules/pcb/model.h"
#include <QByteArray>
#include <QList>
#include <QStringList>

// Sprint-Layout files: layouts (.lay6 of Sprint-Layout 6, .lay of older versions) and macros (.lmk). Versions 0 to 6
// are read, those older than Sprint-Layout 4.0 (0 to 3) converted as Sprint-Layout 6 converts them; versions 6 and 4
// are written. Newer versions are recognised and refused. The record layouts are described in docs/modules/pcb.md.
// Malformed or truncated input throws FormatError.
namespace openloch::sprint {
// The format version from the first bytes (0 to 11; 7 and above come from versions newer than Sprint-Layout 6), or -1
// if the bytes are no Sprint-Layout file.
int fileVersion(const QByteArray &bytes);
// What the conversion of a file older than Sprint-Layout 4.0 changed or left out is added to `notes`.
pcb::Document readLayout(const QByteArray &bytes,QStringList *notes=nullptr);
QList<pcb::Element> readMacro(const QByteArray &bytes,QStringList *notes=nullptr);
// Version 6 holds everything of the model. Version 4 (Sprint-Layout 4.0) knows neither inner layers nor the outline
// layer and turns pads only in quarter turns: elements on I1, I2 and U are left out and counted in `skipped`, and a
// through-plated pad becomes a pair of pads on K1 and K2, as that version stores it.
QByteArray writeLayout(const pcb::Document &document,int version=6,int *skipped=nullptr);
QByteArray writeMacro(const QList<pcb::Element> &elements,int version=6,int *skipped=nullptr);
// The two lines describing a macro (at most 50 characters each), kept in the two short strings that end macros of
// versions 4 to 6; empty for older macros, which have none. withMacroText() puts new lines into a macro's bytes (older
// macros stay as they are).
QStringList readMacroText(const QByteArray &bytes);
QByteArray withMacroText(QByteArray macro,const QString &first,const QString &second);
// Whether a text is mirrored top to bottom as the reference stores it. The model holds that as a mirror left to right
// and half a turn, and the flag (pcb::Element::flipped) for writing it again.
bool mirroredVertically(const pcb::Element &text);
}
