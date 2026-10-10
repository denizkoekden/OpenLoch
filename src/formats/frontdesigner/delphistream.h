#pragma once
#include <QByteArray>
#include <QString>

// The value stream of FrontDesigner files: typed values (a tag byte, then the value) mixed with untyped raw blocks.
// Integers are written in the shortest of 8, 16 or 32 bits, floating point numbers as 80-bit extended values.
namespace openloch::frontdesigner {
class DelphiReader {
public:
    explicit DelphiReader(QByteArray bytes);
    qsizetype position() const{return pos;}
    qsizetype size() const{return data.size();}
    bool atEnd() const{return pos>=data.size();}
    QByteArray raw(qsizetype n);
    QByteArray peek(qsizetype n) const{return data.mid(pos,n);}
    QByteArray since(qsizetype from) const{return data.mid(from,pos-from);}
    qint32 integer();
    double real();            // extended; integers are accepted like the original reader does
    QByteArray extendedRaw(); // the ten bytes of an extended value as stored
    bool boolean();
    QString string();
    [[noreturn]] void fail(const QString &why) const;
private:
    QByteArray data;
    qsizetype pos=0;
    quint8 tag();
};

class DelphiWriter {
public:
    QByteArray bytes;
    void raw(const QByteArray &b){bytes.append(b);}
    void integer(qint64 v);
    void real(double v);
    void extendedRaw(const QByteArray &ten);
    void boolean(bool v);
    void string(const QString &v);
};

// 80-bit extended values: sign, 15-bit exponent and 64-bit mantissa with explicit integer bit. Converted by hand,
// since not every compiler has an 80-bit long double.
double fromExtended(const char *ten);
QByteArray toExtended(double v);
// Short strings of the files use the Windows code page of the German edition.
QString fromWindows1252(const QByteArray &b);
QByteArray toWindows1252(const QString &s);
}
