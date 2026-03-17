#include "centralized_exception_handler.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

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

void CentralizedExceptionHandler::reportException(const std::exception& ex) noexcept {
    std::string details = "Exception: ";
    details += ex.what();
    handleUnhandledException(details);
}

void CentralizedExceptionHandler::reportError(const std::string& message, const std::string& context, const std::string& metadata_json) noexcept {
    std::string details = "Error: " + message + " | Context: " + context;
    if (!metadata_json.empty() && metadata_json != "{}") {
        details += " | Metadata: " + metadata_json;
    }
    handleUnhandledException(details);
}

void CentralizedExceptionHandler::handleTerminate() noexcept {
    std::string message = "Unhandled C++ exception detected.";
    handleUnhandledException(message);
    std::abort();
}

void CentralizedExceptionHandler::handleUnhandledException(const std::string& message) noexcept {
    std::filesystem::path baseDir = std::filesystem::current_path();
#ifdef _WIN32
    char pathBuffer[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(nullptr, pathBuffer, MAX_PATH);
    if (len > 0) {
        baseDir = std::filesystem::path(pathBuffer).parent_path();
    }
#endif

    const std::filesystem::path logDir = baseDir / "logs";
    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);

    const std::filesystem::path logPath = logDir / "exceptions.log";
    std::ofstream ofs(logPath.string(), std::ios::app);
    if (ofs.is_open()) {
        const auto now = std::chrono::system_clock::now();
        const auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        tm = *std::localtime(&t);
#endif
        ofs << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " | " << message << "\n";
    }

    std::cerr << "[CentralizedExceptionHandler] " << message << "\n";
}

void CentralizedExceptionHandler::terminateHandler() noexcept {
    CentralizedExceptionHandler::instance().handleTerminate();
}

#ifdef _WIN32
LONG WINAPI CentralizedExceptionHandler::unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    (void)exceptionInfo;
    CentralizedExceptionHandler::instance().handleUnhandledException(
        "Windows structured exception detected.");
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace RawrXD




