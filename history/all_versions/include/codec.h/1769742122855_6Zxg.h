#pragma once

#include "deflate_brutal_qt.hpp"
#if defined(QT_CORE_LIB) || defined(QT_VERSION)
#include <QByteArray>
#endif

namespace codec {
    QByteArray deflate(const QByteArray& input, bool* success = nullptr);
    QByteArray inflate(const QByteArray& input, bool* success = nullptr);
}