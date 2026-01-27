#pragma once

#include <QByteArray>

namespace codec {
    QByteArray deflate(const QByteArray& input, bool* success = nullptr);
    QByteArray inflate(const QByteArray& input, bool* success = nullptr);
}