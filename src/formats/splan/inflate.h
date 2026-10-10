#pragma once
#include <QByteArray>

namespace openloch::splan {
// Unpacks a zlib stream (RFC 1950 with deflate data, RFC 1951) that starts at `offset` in `data` and may be followed by
// other bytes; `consumed` gets the length of the stream including its checksum. Throws FormatError for broken data or
// more than `limit` unpacked bytes.
QByteArray inflateZlib(const QByteArray &data,qsizetype offset,qsizetype *consumed,qsizetype limit=256*1024*1024);
// Unpacks raw deflate data (RFC 1951, as in ZIP files) of `size` bytes at `offset`; throws FormatError like inflateZlib.
QByteArray inflateRaw(const QByteArray &data,qsizetype offset,qsizetype size,qsizetype limit=256*1024*1024);
// A zlib stream of `data` (without Qt's length prefix).
QByteArray deflateZlib(const QByteArray &data);
}
