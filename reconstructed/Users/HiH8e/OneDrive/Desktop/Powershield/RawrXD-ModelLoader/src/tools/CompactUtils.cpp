#include "CompactUtils.h"
#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace CompactUtils {

QByteArray compact(const QJsonObject &obj) {
    const QJsonDocument doc(obj);
    const QByteArray minified = doc.toJson(QJsonDocument::Compact);
    return qCompress(minified, 9);
}

QJsonObject expand(const QByteArray &compressed) {
    const QByteArray raw = qUncompress(compressed);
    return QJsonDocument::fromJson(raw).object();
}

}
