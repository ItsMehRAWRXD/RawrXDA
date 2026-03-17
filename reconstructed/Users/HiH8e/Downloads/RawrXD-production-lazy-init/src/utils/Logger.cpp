#include "Logger.hpp"
#include "ErrorReporter.hpp"
#include <iostream>

#ifdef QT_VERSION
#include <QDebug>
#endif

namespace Logger {

void info(const std::string& msg) {
#ifdef QT_VERSION
    qInfo().noquote() << QString::fromStdString(msg);
#else
    std::cout << "[INFO] " << msg << std::endl;
#endif
}

void warn(const std::string& msg) {
#ifdef QT_VERSION
    qWarning().noquote() << QString::fromStdString(msg);
#else
    std::cerr << "[WARN] " << msg << std::endl;
#endif
}

void error(const std::string& msg) {
    // Log to console and show a UI report via ErrorReporter when available
#ifdef QT_VERSION
    qCritical().noquote() << QString::fromStdString(msg);
#endif
    std::cerr << "[ERROR] " << msg << std::endl;
    // Use ErrorReporter when present to pop up on UI
    ErrorReporter::report(msg, nullptr);
}

} // namespace Logger
