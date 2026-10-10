#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <stdexcept>

namespace openloch {
QString legacyTitle(const QJsonObject &document);
// A bitmap of a LochMaster object as a BMP file that picture readers take: the original saves a 24-bit picture with a
// colour table with a data offset (and file size) 1024 bytes too large, past the end of its bits; such an offset is set
// to the one of its info header. Other bitmaps come back unchanged, and the stored bytes stay as they are.
QByteArray legacyBitmapData(QByteArray bmp);
class FormatError : public std::runtime_error {
public: explicit FormatError(const QString &s) : std::runtime_error(s.toStdString()) {}
};

// A sequential, bounds-checked reader for the verified Delphi 3.08–4.07 streams.
class LegacyReader {
public:
    explicit LegacyReader(QByteArray bytes);
    QJsonObject read(bool embeddedBoard);
    qsizetype position() const { return pos; }
    int objectCount() const { return total; }
private:
    QByteArray bytes;
    qsizetype pos = 0;
    double version = 0;
    int total = 0;
    QByteArray raw(qsizetype n);
    quint8 byte();
    quint32 u32();
    qint32 i32();
    int integer();
    bool boolean();
    double number();
    QString string();
    QJsonArray points();
    QJsonArray integers(int n);
    QJsonArray booleans(int n);
    QJsonObject base(const QString &type);
    QJsonObject object(QString type = {}, int depth = 0);
    QJsonObject document(bool embeddedBoard, int depth = 0);
    [[noreturn]] void fail(const QString &why) const;
};
}
