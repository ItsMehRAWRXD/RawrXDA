#include "centralized_exception_handler.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {

CentralizedExceptionHandler& CentralizedExceptionHandler::instance() {
    static CentralizedExceptionHandler handler;
    return handler;
}

CentralizedExceptionHandler::CentralizedExceptionHandler()
    : installed_(false)
    , autoRecovery_(false)
    , previousTerminate_(nullptr)
#ifdef _WIN32
    , previousFilter_(nullptr)
#endif
{
}

CentralizedExceptionHandler::~CentralizedExceptionHandler() = default;

void CentralizedExceptionHandler::installHandler() {
    if (installed_.exchange(true)) {
        return;
    }

    previousTerminate_ = std::set_terminate(&CentralizedExceptionHandler::terminateHandler);
#ifdef _WIN32
    previousFilter_ = reinterpret_cast<void*>(SetUnhandledExceptionFilter(&CentralizedExceptionHandler::unhandledExceptionFilter));
#endif
}

void CentralizedExceptionHandler::uninstallHandler() {
    if (!installed_.exchange(false)) {
        return;
    }

    if (previousTerminate_) {
        std::set_terminate(previousTerminate_);
    }
#ifdef _WIN32
    if (previousFilter_) {
        SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(previousFilter_));
    }
#endif
}

void CentralizedExceptionHandler::enableAutomaticRecovery(bool enabled) {
    autoRecovery_.store(enabled);
}

bool CentralizedExceptionHandler::isAutomaticRecoveryEnabled() const {
    return autoRecovery_.load();
}

void CentralizedExceptionHandler::handleTerminate() noexcept {
    QString message = QStringLiteral("Unhandled C++ exception detected.");
    handleUnhandledException(message);
    std::abort();
}

void CentralizedExceptionHandler::handleUnhandledException(const QString& message) noexcept {
    QString baseDir = QCoreApplication::applicationDirPath();
    if (baseDir.isEmpty()) {
        baseDir = QDir::currentPath();
    }

    QDir logDir(baseDir + "/logs");
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    QFile logFile(logDir.filePath("exception.log"));
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        stream << QDateTime::currentDateTime().toString(Qt::ISODate) << " | " << message << "\n";
    }

    qCritical() << "[CentralizedExceptionHandler]" << message;
}

void CentralizedExceptionHandler::terminateHandler() noexcept {
    CentralizedExceptionHandler::instance().handleTerminate();
}

#ifdef _WIN32
long __stdcall CentralizedExceptionHandler::unhandledExceptionFilter(void* exceptionInfo) {
    Q_UNUSED(exceptionInfo);
    CentralizedExceptionHandler::instance().handleUnhandledException(
        QStringLiteral("Windows structured exception detected."));
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace RawrXD
