#include "IDELogger.h"

#include <windows.h>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <string>
#include <fstream>
#include <ctime>
#include <set>

namespace {
    std::mutex g_ideLoggerMutex;
    std::ofstream g_ideLogFile;

    const char* levelToString(IDELogLevel level) {
        switch (level) {
            case IDELogLevel::DEBUG:    return "DEBUG";
            case IDELogLevel::INFO:     return "INFO";
            case IDELogLevel::WARNING:  return "WARN";
#undef ERROR
            case IDELogLevel::ERROR:    return "ERROR";
            case IDELogLevel::CRITICAL: return "CRITICAL";
            default:                    return "UNKNOWN";
        }
    }

    std::string nowTimestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tmLocal{};
#ifdef _WIN32
        localtime_s(&tmLocal, &t);
#else
        tmLocal = *std::localtime(&t);
#endif

        std::ostringstream ss;
        ss << std::put_time(&tmLocal, "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

} // anonymous namespace

IDELogger& IDELogger::getInstance() {
    static IDELogger inst;
    return inst;
}

void IDELogger::initialize(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(g_ideLoggerMutex);

#if __has_include(<spdlog/spdlog.h>)
    try {
        (void)logFilePath;
        return;
    } catch (...) {
        // Fall back below.
    }

#endif

    if (g_ideLogFile.is_open()) {
        return;
    }

    g_ideLogFile.open(logFilePath, std::ios::out | std::ios::app);
    if (!g_ideLogFile.is_open()) {
        OutputDebugStringA("[IDELogger] Failed to open log file\n");
    }
}

void IDELogger::log(IDELogLevel level, const std::string& message) {
#if __has_include(<spdlog/spdlog.h>)
    try {
        switch (level) {
            case IDELogLevel::DEBUG:    spdlog::debug(message); break;
            case IDELogLevel::INFO:     spdlog::info(message); break;
            case IDELogLevel::WARNING:  spdlog::warn(message); break;
            case IDELogLevel::ERROR:    spdlog::error(message); break;
            case IDELogLevel::CRITICAL: spdlog::critical(message); break;
            default:                    spdlog::info(message); break;
        }

        return;
    } catch (...) {
        // Fall back below.
    }

#endif

    std::lock_guard<std::mutex> lock(g_ideLoggerMutex);

    std::string line = nowTimestamp();
    line += " [";
    line += levelToString(level);
    line += "] ";
    line += message;
    line += "\n";

    if (g_ideLogFile.is_open()) {
        g_ideLogFile << line;
        g_ideLogFile.flush();
    }

    OutputDebugStringA(line.c_str());
}


