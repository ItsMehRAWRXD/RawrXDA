// ============================================================================
// async_logger.hpp — Production Async Logger
// ============================================================================
// Thread-safe, lock-free(ish) async logging with severity levels.
// No Qt, no exceptions, C++20, Win32.
// ============================================================================

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3, Fatal = 4 };

struct LogEntry {
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
    std::string message;
    std::string source;
};

class AsyncLogger {
public:
    static AsyncLogger& instance();

    void initialize(const char* logPath = nullptr);
    void shutdown();
    void log(LogLevel level, const char* source, const char* fmt, ...);
    void flush();

    void setLevel(LogLevel minLevel) { m_minLevel.store(minLevel); }
    LogLevel getLevel() const { return m_minLevel.load(); }

private:
    AsyncLogger() = default;
    ~AsyncLogger() { shutdown(); }

    void workerThread();
    void writeEntry(const LogEntry& entry);

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_running{false};
    std::atomic<LogLevel> m_minLevel{LogLevel::Debug};
    std::queue<LogEntry> m_queue;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    std::thread m_worker;
    std::ofstream m_file;
    std::string m_logPath;
};

inline void LogMessage(const char* msg) {
    AsyncLogger::instance().log(LogLevel::Info, "RawrXD", "%s", msg);
}
