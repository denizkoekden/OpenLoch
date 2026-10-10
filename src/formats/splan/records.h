#pragma once
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>
#include <QVariant>

// The records of sPlan 7 and 8 files (.spl7, .spl8), field by field, as docs/modules/schematic.md describes them.
// One description of each record's layout serves reading and writing, so that a record written from its fields reads
// back to the same fields and an unchanged record is written byte for byte.
namespace openloch::splan {
// Object types in the files.
enum Type {Ellipse=1,Rectangle=2,Text=3,Line=4,Polygon=5,Group=6,Component=7,Junction=8,Picture=9,TextBox=10,Contact=11,Bezier=12,Dimension=14};

// A record: its fields by name and the records inside it. Numbers are kept as read (32-bit integers, floats with their
// bits), strings with the bytes they were read from.
struct Record {
    int type=0;                         // object type, 0 for the file, a sheet or a part without a type byte
    QHash<QString,QVariant> fields;
    QHash<QString,QList<Record>> lists; // children, sheets, sub-records
    QByteArray bytes;                   // what it was read from (empty for new records)
    qsizetype offset=0;                 // where in the file

    qint64 integer(const QString &name) const{return fields.value(name).toLongLong();}
    double real(const QString &name) const{return double(fields.value(name).toFloat());}
    QString string(const QString &name) const{return fields.value(name).toString();}
    QPointF point(const QString &name) const;
    QList<QPointF> points(const QString &name) const;
    QByteArray raw(const QString &name) const{return fields.value(name).toByteArray();}
    void set(const QString &name,const QVariant &value){fields.insert(name,value);}
    void setPoints(const QString &name,const QList<QPointF> &points);   // stored as floats
    void setPoint(const QString &name,QPointF point);
    void setReal(const QString &name,double value){fields.insert(name,QVariant::fromValue(float(value)));}
    const Record *child(const QString &list,int index=0) const{return lists.value(list).size()>index?&lists[list][index]:nullptr;}
};

// The version in a file's first bytes ("SPLAN70", "SPLAN80"), 0 for anything else.
int fileVersion(const QByteArray &bytes);
// The versions read and written.
bool supportedVersion(int version);
// Reads a whole file; throws FormatError naming the place of the damage.
Record readFile(const QByteArray &bytes);
// Writes a file from its records (the same version as read, see `version`).
QByteArray writeFile(const Record &file);
// A page of a sPlan library (.LIB, versions 7 and 8): `name` and the objects as `children`.
Record readLibraryFile(const QByteArray &bytes);
QByteArray writeLibraryFile(const Record &page,int version);
// A sPlan title block (.sbk): the version (from its first number, 0 for anything else), the objects as `children`.
int formVersion(const QByteArray &bytes);
Record readFormFile(const QByteArray &bytes);
QByteArray writeFormFile(const Record &form,int version);
// A single object record (type byte and body), and the bytes of one.
QByteArray writeObject(const Record &object,int version);
Record readObject(const QByteArray &bytes,int version);
// A sheet without its objects, and a file without its sheets: the parts kept for writing back.
QByteArray writeSheetFrame(const Record &sheet,int version);
Record readSheetFrame(const QByteArray &bytes,int version);
QByteArray writeFileFrame(const Record &file);
Record readFileFrame(const QByteArray &bytes);
// Whether a text record reads the additional string (a designator text, or a text with a variable in it).
bool hasDisplayString(const QString &text);
}
