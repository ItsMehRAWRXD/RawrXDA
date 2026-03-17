#pragma once

#include <atomic>
#include <exception>
#include <QString>
#include <QJsonObject>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {

class CentralizedExceptionHandler {
public:
    static CentralizedExceptionHandler& instance();

    void installHandler();
    void uninstallHandler();
    void enableAutomaticRecovery(bool enabled);
    bool isAutomaticRecoveryEnabled() const;

    void reportException(const std::exception& ex) noexcept;
    void reportError(const QString& message, const QString& context, const QJsonObject& metadata) noexcept;

private:
    CentralizedExceptionHandler();
    ~CentralizedExceptionHandler();

    void handleTerminate() noexcept;
    void handleUnhandledException(const QString& message) noexcept;

    static void terminateHandler() noexcept;
#ifdef _WIN32
    static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
#endif

    std::atomic<bool> installed_;
    std::atomic<bool> autoRecovery_;
    std::terminate_handler previousTerminate_;
#ifdef _WIN32
    void* previousFilter_;
#endif
};

} // namespace RawrXD
