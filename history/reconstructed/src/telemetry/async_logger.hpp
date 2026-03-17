// telemetry/async_logger.hpp
// Zero-alloc lock-free ring buffer structured logging
#pragma once

// Must define WIN32_LEAN_AND_MEAN before windows.h to avoid conflicts
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Windows.h defines ERROR as 0 — undefine so we can use it as an enum value
#ifdef ERROR
#undef ERROR
#endif

#include <atomic>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <thread>

namespace RawrXD::Telemetry {

    enum class LogLevel : uint8_t { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3, FATAL = 4 };

    struct alignas(64) LogEntry {
        char message[256];
        char function[64];
        char file[32];
        uint64_t timestamp;
        uint32_t thread_id;
        LogLevel level;
        uint32_t line;
    };

    class LockFreeLogger {
        static constexpr size_t RING_SIZE = 4096;
        LogEntry ring_[RING_SIZE];
        alignas(64) std::atomic<size_t> write_idx_{0};
        alignas(64) std::atomic<size_t> read_idx_{0};
        HANDLE flush_event_;
        HANDLE log_file_;
        std::atomic<bool> running_{true};
        std::thread flush_thread_;

        static const char* levelStr(LogLevel lvl) {
            switch (lvl) {
                case LogLevel::DEBUG: return "DEBUG";
                case LogLevel::INFO:  return "INFO";
                case LogLevel::WARN:  return "WARN";
                case LogLevel::ERROR: return "ERROR";
                case LogLevel::FATAL: return "FATAL";
                default: return "UNKNOWN";
            }
        }

    public:
        static LockFreeLogger& instance() {
            static LockFreeLogger inst;
            return inst;
        }

        LockFreeLogger() {
            flush_event_ = CreateEventW(nullptr, FALSE, FALSE, L"RawrLogFlush");
            log_file_ = CreateFileA("rawrxd_production.log",
                                    GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            flush_thread_ = std::thread([this] {
                while (running_.load(std::memory_order_relaxed)) {
                    WaitForSingleObject(flush_event_, 100);
                    flush();
                }
            });
        }

        ~LockFreeLogger() {
            running_.store(false, std::memory_order_release);
            SetEvent(flush_event_);
            if (flush_thread_.joinable()) flush_thread_.join();
            flush();
            if (log_file_ != INVALID_HANDLE_VALUE) CloseHandle(log_file_);
            if (flush_event_) CloseHandle(flush_event_);
        }

        void log(LogLevel lvl, const char* file, int line,
                 const char* func, const char* fmt, ...) {
            const size_t idx = write_idx_.fetch_add(1, std::memory_order_relaxed) % RING_SIZE;
            auto& entry = ring_[idx];

            LARGE_INTEGER perf;
            QueryPerformanceCounter(&perf);
            entry.timestamp = static_cast<uint64_t>(perf.QuadPart);
            entry.thread_id = GetCurrentThreadId();
            entry.level = lvl;
            entry.line = static_cast<uint32_t>(line);

            // Copy file basename only
            const char* basename = file;
            for (const char* p = file; *p; ++p) {
                if (*p == '\\' || *p == '/') basename = p + 1;
            }
            strncpy_s(entry.file, basename, 31);
            entry.file[31] = '\0';
            strncpy_s(entry.function, func, 63);
            entry.function[63] = '\0';

            va_list args;
            va_start(args, fmt);
            vsnprintf(entry.message, 255, fmt, args);
            va_end(args);
            entry.message[255] = '\0';

            SetEvent(flush_event_);
        }

        void flush() {
            if (log_file_ == INVALID_HANDLE_VALUE) return;

            size_t r = read_idx_.load(std::memory_order_acquire);
            size_t w = write_idx_.load(std::memory_order_acquire);

            while (r != w) {
                const auto& entry = ring_[r % RING_SIZE];
                char buf[512];
                int len = snprintf(buf, sizeof(buf),
                    "[%s] [T:%u] [%s:%u] [%s] %s\n",
                    levelStr(entry.level), entry.thread_id,
                    entry.file, entry.line, entry.function, entry.message);

                if (len > 0) {
                    DWORD written;
                    WriteFile(log_file_, buf, static_cast<DWORD>(len), &written, nullptr);
                }
                ++r;
            }
            read_idx_.store(r, std::memory_order_release);
            FlushFileBuffers(log_file_);
        }
    };

} // namespace RawrXD::Telemetry

// Non-intrusive macro wrappers
#define RAWR_LOG_DEBUG(...) \
    RawrXD::Telemetry::LockFreeLogger::instance().log( \
        RawrXD::Telemetry::LogLevel::DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define RAWR_LOG_INFO(...) \
    RawrXD::Telemetry::LockFreeLogger::instance().log( \
        RawrXD::Telemetry::LogLevel::INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define RAWR_LOG_WARN(...) \
    RawrXD::Telemetry::LockFreeLogger::instance().log( \
        RawrXD::Telemetry::LogLevel::WARN, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define RAWR_LOG_ERROR(...) \
    RawrXD::Telemetry::LockFreeLogger::instance().log( \
        RawrXD::Telemetry::LogLevel::ERROR, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define RAWR_LOG_FATAL(...) \
    RawrXD::Telemetry::LockFreeLogger::instance().log( \
        RawrXD::Telemetry::LogLevel::FATAL, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define RAWR_LOG(lvl, ...) \
    RawrXD::Telemetry::LockFreeLogger::instance().log(lvl, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
