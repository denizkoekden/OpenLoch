#pragma once
#include "model.h"

// The parts list ("Stückliste"): the components with "In Stückliste aufnehmen", grouped by the letters of their
// designators, optionally with equal components merged into one row ("R1,R2,R3 = 3 x 1k").
namespace openloch::schematic {
struct PartsListOptions {
    enum class Sort {Alphabetical,Frequency};
    QList<int> sheets;              // the sheets taken into account, empty for all
    bool merge=true;                // equal value and additional texts make one row
    Sort sort=Sort::Alphabetical;   // the groups by their letters, or the largest group first
    QString fileName;               // for <FILENAME> and the like in designators
};
struct PartsRow {
    QString letters;                // the group
    QStringList designators;
    QString value;
    QStringList extra;              // additional texts 1 to 4
    int count() const{return int(designators.size());}
    bool operator==(const PartsRow &) const=default;
};
QList<PartsRow> partsList(const Document &document,const PartsListOptions &options={});
// Designators compared with their numbers as numbers ("R2" before "R10").
bool designatorLess(const QString &a,const QString &b);

// The list as a table: a header and one row of texts per row, the columns designators, count, value and the chosen
// additional texts.
struct PartsTable {
    QStringList header;
    QList<QStringList> rows;
};
PartsTable partsTable(const QList<PartsRow> &rows,const QList<int> &extras={});
// The table as text: fields separated by `separator`, the header as the first line if asked for, an empty line between
// the groups if asked for.
QString partsText(const PartsTable &table,const QList<int> &groupStarts,QChar separator,bool header,bool emptyLines);
// The table as a Rich Text Format document ("Speichern"): the lines above it as paragraphs, then the table with the
// header in bold. And back ("Öffnen"): the first table of an RTF document, its first row as the header; empty if
// there is none.
QByteArray partsRtf(const PartsTable &table,const QStringList &lines={},const QString &family=QStringLiteral("Arial"),int points=10);
PartsTable partsFromRtf(const QByteArray &rtf);
// The table drawn on a sheet: texts of `height` in cells, with a frame and separating lines, its top left at `at`.
struct PartsDrawing {
    double height=2.5;
    QString family=QStringLiteral("Arial");
    bool frame=true,verticalLines=true,horizontalLines=false;
    bool header=true;               // the header as the first row, bold
    bool alternate=false;           // every second row on light grey ("Zeilen alternierend")
    bool shadow=false;              // a grey shadow below and to the right of the frame ("Schatten")
    int rowsPerColumn=0;            // longer tables are broken into tables side by side of this many rows; 0: no break
};
Item partsGroup(const PartsTable &table,QPointF at,const PartsDrawing &drawing={});
// The rows of a table that fit into `height` (the sheet's, less a margin), with the header if there is one.
int fittingRows(double height,const PartsDrawing &drawing);

// "Childliste (Kontaktspiegel)": the children of a parent, one row each, with the columns chosen in their order.
enum class ChildColumn {Designator,Contacts,Value,PageNumber,PageName,RowColumn,PageColumn,Reference};
struct ChildListOptions {
    QList<ChildColumn> columns{ChildColumn::Designator,ChildColumn::Contacts,ChildColumn::PageColumn};
    bool rowLetters=true,columnLetters=false;   // row and column of the title block grid as letters (A, B, C) or numbers
    bool slash=false;                           // "Blatt.Spalte" written "/2.3"
};
// The texts of a column for a child: designator and value as shown, its contacts' texts joined by "-", the sheet's number
// and name, row and column of the title block grid ("B3"), sheet and column ("2.3"), or the reference "/2.B3-K1:13-14".
PartsTable childList(const Document &document,const QString &parentId,const ChildListOptions &options,const QString &fileName={});
QString childColumnName(ChildColumn column);
}
