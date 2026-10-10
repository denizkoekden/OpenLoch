#pragma once
#include <QByteArray>
#include <QList>
#include <QString>
class QDateTime;
class QTextDocument;
namespace openloch {
struct Project;
QString plainNotes(const QByteArray &rtf);
QByteArray rtfNotes(const QString &text);
// Formatted notes as LochMaster's RichEdit stores them: fonts, sizes, bold, italic, underline, colours, alignment,
// indents, bullets, tabs and line breaks. Every paragraph ends with \par, the text ends with "}\r\n" and a NUL.
void readRtf(const QByteArray &rtf,QTextDocument &document);
QByteArray writeRtf(const QTextDocument &document);
// The original's "Stückliste einfügen" (mode 0) and "Einkaufsliste einfügen" (mode 1): header, parts and the wire
// bridges with hole coordinates from the origin, one entry per line.
struct NoteLine {QString text;bool bold=false;int size=10;};
QList<NoteLine> partsListNotes(const Project &project,int mode,const QString &projectName,const QDateTime &when,const QString &user);
}
