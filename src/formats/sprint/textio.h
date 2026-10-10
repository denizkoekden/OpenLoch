#pragma once
#include "modules/pcb/model.h"
#include <QList>
#include <QString>

// Sprint-Layout's text interface ("Text-IO"): elements as lines of text, one statement per element, ended by a
// semicolon. A statement starts with its kind (TRACK, PAD, SMDPAD, ZONE, TEXT, CIRCLE) and lists KEY=VALUE pairs
// separated by commas, in any order and any case. Lengths are millimetres × 10000, points "x / y" from the top left
// corner with y downwards; texts stand between bars (|…|). GROUP … END_GROUP and BEGIN_COMPONENT … END_COMPONENT
// (with ID_TEXT and VALUE_TEXT) enclose groups and components; PAD_ID and CON0, CON1, … give airwires.
namespace openloch::sprint {
// The elements as Text-IO; groups and components become blocks (each component one block, inside the groups all its
// members share), airwires PAD_ID and CON pairs.
QString writeTextIO(const QList<pcb::Element> &elements);
// Elements from Text-IO; throws FormatError for anything it cannot read.
QList<pcb::Element> readTextIO(const QString &text);
// The file's bytes: Windows code page 1252 as the reference writes them; reading also takes UTF-8.
QByteArray textIOBytes(const QString &text);
QString textIOText(const QByteArray &bytes);
}
