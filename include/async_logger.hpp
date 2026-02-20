// ============================================================================
// async_logger.hpp — Non-blocking Structured Logging (Hot Path)
// ============================================================================
//
// Action Item #7: Move log writes to a lock-free queue / background flush.
// Under high-frequency requests (e.g., 50 rps), p95 latency must not
// degrade due to logging.
//
// Design:
//   - Lock-free SPSC ring buffer for zero-contention enqueue
//   - Background flush thread drains to stderr/file
//   - Bounded buffer: drops oldest on overflow (no blocking)
//   - Thread-safe via atomic operations only (no mutex on hot path)
//   - Fallback: direct write if flush thread not started
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_ASYNC_LOGGER_H
#define RAWRXD_ASYNC_LOGGER_H

#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <array>
#include <functional>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// ============================================================================
// Log Severity
// ============================================================================
enum class LogSeverity : uint8_t {
    TRACE   = 0,
    DEBUG   = 1,
    INFO    = 2,
    WARN    = 3,
    ERROR   = 4,
    FATAL   = 5
};

inline const char* severityToString(LogSeverity s) {
    switch (s) {
        case LogSeverity::TRACE: return "TRACE";
        case LogSeverity::DEBUG: return "DEBUG";
        case LogSeverity::INFO:  return "INFO";
        case LogSeverity::WARN:  return "WARN";
        case LogSeverity::ERROR: return "ERROR";
        case LogSeverity::FATAL: return "FATAL";
        default: return "?";
    }
}

// ============================================================================
// Log Entry — fixed-size for ring buffer
// ============================================================================
struct LogEntry {
    static constexpr size_t MAX_MSG_LEN    = 512;
    static constexpr size_t MAX_OP_LEN     = 64;
    static constexpr size_t MAX_COMP_LEN   = 32;
    static constexpr size_t MAX_REQID_LEN  = 64;

    LogSeverity severity    = LogSeverity::INFO;
    uint64_t    timestampMs = 0;     // Epoch ms
    char        component[MAX_COMP_LEN]  = {};
    char        operation[MAX_OP_LEN]    = {};
    char        message[MAX_MSG_LEN]     = {};
    char        reqId[MAX_REQID_LEN]     = {};   // Action Item #8

    void set(LogSeverity sev, const char* comp, const char* op,
             const char* msg, const char* rid = nullptr) {
        severity = sev;
        auto now = std::chrono::system_clock::now();
        timestampMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());
        strncpy_s(component, comp ? comp : "", MAX_COMP_LEN - 1);
        strncpy_s(operation, op ? op : "", MAX_OP_LEN - 1);
        strncpy_s(message, msg ? msg : "", MAX_MSG_LEN - 1);
        strncpy_s(reqId, rid ? rid : "", MAX_REQID_LEN - 1);
    }
};

// ============================================================================
// Lock-Free Ring Buffer (SPSC — Single Producer, Single Consumer)
// ============================================================================
template <typename T, size_t Capacity>
class SPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

public:
    SPSCRingBuffer() : m_head(0), m_tail(0) {}

    bool tryPush(const T& item) {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t next = (head + 1) & (Capacity - 1);
        if (next == m_tail.load(std::memory_order_acquire)) {
            // Buffer full — drop oldest by advancing tail
            m_tail.store((m_tail.load(std::memory_order_relaxed) + 1) & (Capacity - 1),
                         std::memory_order_release);
        }
        m_buffer[head] = item;
        m_head.store(next, std::memory_order_release);
        return true;
    }

    bool tryPop(T& item) {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire)) {
            return false; // Empty
        }
        item = m_buffer[tail];
        m_tail.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }

    size_t size() const {
        size_t h = m_head.load(std::memory_order_acquire);
        size_t t = m_tail.load(std::memory_order_acquire);
        return (h - t) & (Capacity - 1);
    }

private:
    std::array<T, Capacity>     m_buffer;
    alignas(64) std::atomic<size_t> m_head;
    alignas(64) std::atomic<size_t> m_tail;
};

// ============================================================================
// AsyncLogger — Singleton
// ============================================================================
class AsyncLogger {
public:
    static constexpr size_t RING_CAPACITY = 4096; // Must be power of 2

    static AsyncLogger& instance() {
        static AsyncLogger logger;
        return logger;
    }

    // Start background flush thread
    void start() {
        if (m_running.exchange(true)) return; // Already running
        m_flushThread = std::thread([this]() { flushLoop(); });
    }

    // Stop and flush remaining
    void stop() {
        if (!m_running.exchange(false)) return;
        if (m_flushThread.joinable()) {
            m_flushThread.join();
        }
        // Drain remaining entries
        LogEntry entry;
        while (m_ring.tryPop(entry)) {
            writeEntry(entry);
        }
    }

    // Set minimum severity (below this = dropped)
    void setMinSeverity(LogSeverity sev) {
        m_minSeverity.store(static_cast<uint8_t>(sev), std::memory_order_relaxed);
    }

    // Set output file (in addition to stderr)
    void setLogFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_fileMutex);
        if (m_logFile) {
            fclose(m_logFile);
            m_logFile = nullptr;
        }
        if (!path.empty()) {
            m_logFile = fopen(path.c_str(), "a");
        }
    }

    // Non-blocking log enqueue — HOT PATH
    void log(LogSeverity severity, const char* component, const char* operation,
             const char* message, const char* reqId = nullptr) {
        if (static_cast<uint8_t>(severity) < m_minSeverity.load(std::memory_order_relaxed)) {
            return;
        }

        LogEntry entry;
        entry.set(severity, component, operation, message, reqId);

        if (m_running.load(std::memory_order_relaxed)) {
            m_ring.tryPush(entry);
        } else {
            // Fallback: direct write if flush thread not started
            writeEntry(entry);
        }
    }

    // Convenience methods
    void trace(const char* comp, const char* op, const char* msg, const char* rid = nullptr) {
        log(LogSeverity::TRACE, comp, op, msg, rid);
    }
    void debug(const char* comp, const char* op, const char* msg, const char* rid = nullptr) {
        log(LogSeverity::DEBUG, comp, op, msg, rid);
    }
    void info(const char* comp, const char* op, const char* msg, const char* rid = nullptr) {
        log(LogSeverity::INFO, comp, op, msg, rid);
    }
    void warn(const char* comp, const char* op, const char* msg, const char* rid = nullptr) {
        log(LogSeverity::WARN, comp, op, msg, rid);
    }
    void error(const char* comp, const char* op, const char* msg, const char* rid = nullptr) {
        log(LogSeverity::ERROR, comp, op, msg, rid);
    }

    // Stats
    uint64_t totalEnqueued() const { return m_totalEnqueued.load(std::memory_order_relaxed); }
    uint64_t totalFlushed()  const { return m_totalFlushed.load(std::memory_order_relaxed); }

    ~AsyncLogger() { stop(); }

private:
    AsyncLogger() : m_running(false), m_logFile(nullptr) {
        m_minSeverity.store(static_cast<uint8_t>(LogSeverity::DEBUG));
    }
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    void flushLoop() {
        LogEntry entry;
        while (m_running.load(std::memory_order_relaxed)) {
            int drained = 0;
            while (m_ring.tryPop(entry) && drained < 256) {
                writeEntry(entry);
                ++drained;
                m_totalFlushed.fetch_add(1, std::memory_order_relaxed);
            }
            if (drained == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }

    void writeEntry(const LogEntry& e) {
        // Format: [timestamp] [component] [SEVERITY] operation — message {reqId}
        uint64_t ts = e.timestampMs;
        uint64_t sec = ts / 1000;
        uint64_t ms  = ts % 1000;

        // Convert to local time
        time_t rawTime = static_cast<time_t>(sec);
        struct tm localTm;
        localtime_s(&localTm, &rawTime);

        char buf[1024];
        int len = snprintf(buf, sizeof(buf),
            "[%04d-%02d-%02d %02d:%02d:%02d.%03llu] [%s] [%s] %s — %s",
            localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday,
            localTm.tm_hour, localTm.tm_min, localTm.tm_sec,
            static_cast<unsigned long long>(ms),
            e.component, severityToString(e.severity),
            e.operation, e.message);

        if (e.reqId[0] != '\0' && len < static_cast<int>(sizeof(buf)) - 80) {
            len += snprintf(buf + len, sizeof(buf) - len, " {reqId=%s}", e.reqId);
        }
        if (len < static_cast<int>(sizeof(buf)) - 2) {
            buf[len++] = '\n';
            buf[len] = '\0';
        }

        fprintf(stderr, "%s", buf);

        if (m_logFile) {
            std::lock_guard<std::mutex> lock(m_fileMutex);
            if (m_logFile) {
                fputs(buf, m_logFile);
                fflush(m_logFile);
            }
        }

        m_totalEnqueued.fetch_add(1, std::memory_order_relaxed);
    }

    SPSCRingBuffer<LogEntry, RING_CAPACITY> m_ring;
    std::atomic<bool>       m_running;
    std::thread             m_flushThread;
    std::atomic<uint8_t>    m_minSeverity;
    FILE*                   m_logFile;
    std::mutex              m_fileMutex;
    std::atomic<uint64_t>   m_totalEnqueued{0};
    std::atomic<uint64_t>   m_totalFlushed{0};
};

// ============================================================================
// Convenience macros — non-blocking log with auto-component
// ============================================================================
#define RAWRXD_LOG(sev, op, msg) \
    AsyncLogger::instance().log(sev, __FUNCTION__, op, msg)
#define RAWRXD_LOG_REQ(sev, op, msg, reqId) \
    AsyncLogger::instance().log(sev, __FUNCTION__, op, msg, reqId)

#define RAWRXD_TRACE(op, msg) RAWRXD_LOG(LogSeverity::TRACE, op, msg)
#define RAWRXD_DEBUG(op, msg) RAWRXD_LOG(LogSeverity::DEBUG, op, msg)
#define RAWRXD_INFO(op, msg)  RAWRXD_LOG(LogSeverity::INFO, op, msg)
#define RAWRXD_WARN(op, msg)  RAWRXD_LOG(LogSeverity::WARN, op, msg)
#define RAWRXD_ERROR(op, msg) RAWRXD_LOG(LogSeverity::ERROR, op, msg)

#endif // RAWRXD_ASYNC_LOGGER_H
