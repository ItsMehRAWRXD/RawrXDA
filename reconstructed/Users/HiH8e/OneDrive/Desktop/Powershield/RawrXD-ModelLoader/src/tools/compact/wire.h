#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace rwx {

// Compact: minify JSON then gzip (qCompress)
inline QByteArray compact(const QJsonObject &obj) {
    QJsonDocument doc(obj);
    QByteArray minified = doc.toJson(QJsonDocument::Compact);
    return qCompress(minified, 9);
}

// Expand: gunzip (qUncompress) then parse JSON
inline QJsonObject expand(const QByteArray &compressed) {
    QByteArray raw = qUncompress(compressed);
    return QJsonDocument::fromJson(raw).object();
}

// Usage note: set header `X-Compact: 1` and `Content-Encoding: gzip` when sending compact bytes.

}
