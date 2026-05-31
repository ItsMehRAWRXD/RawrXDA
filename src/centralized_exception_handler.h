#pragma once

#include <atomic>
#include <exception>
#include <string>
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
    // Qt-free version: metadata is just a string key-value dump
    void reportError(const std::string& message, const std::string& context, const std::string& metadata_json = "{}") noexcept;

private:
    CentralizedExceptionHandler();
    ~CentralizedExceptionHandler();

    void handleTerminate() noexcept;
    void handleUnhandledException(const std::string& message) noexcept;

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

