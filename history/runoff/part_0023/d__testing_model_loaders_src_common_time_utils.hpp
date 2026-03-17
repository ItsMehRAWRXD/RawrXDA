// ============================================================================
// File: src/common/time_utils.hpp
// Purpose: Time utilities replacing QDateTime/QElapsedTimer/QTime
// No Qt dependency - pure C++17
// ============================================================================
#pragma once

#include <chrono>
#include <string>
#include <ctime>
#include <cstdio>

using TimePoint = std::chrono::system_clock::time_point;
using SteadyClock = std::chrono::steady_clock;
using SteadyTimePoint = std::chrono::steady_clock::time_point;

namespace TimeUtils {

inline TimePoint now() {
    return std::chrono::system_clock::now();
}

inline std::string toISOString(const TimePoint& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    struct tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return buf;
}

inline std::string toLocalString(const TimePoint& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}

inline std::string currentTimeString() {
    return toLocalString(now());
}

inline std::string currentTimeOnly() {
    auto t = std::chrono::system_clock::to_time_t(now());
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);
    return buf;
}

inline int currentYear() {
    auto t = std::chrono::system_clock::to_time_t(now());
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    return tm_buf.tm_year + 1900;
}

/**
 * @brief Elapsed timer replacing QElapsedTimer
 */
class ElapsedTimer {
public:
    void start() { m_start = SteadyClock::now(); m_running = true; }
    
    int64_t elapsedMs() const {
        auto end = m_running ? SteadyClock::now() : m_end;
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start).count();
    }
    
    int64_t elapsedUs() const {
        auto end = m_running ? SteadyClock::now() : m_end;
        return std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count();
    }
    
    double elapsedSeconds() const {
        return elapsedMs() / 1000.0;
    }
    
    void stop() { m_end = SteadyClock::now(); m_running = false; }
    
    int64_t restart() {
        auto now = SteadyClock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start).count();
        m_start = now;
        return elapsed;
    }

private:
    SteadyTimePoint m_start;
    SteadyTimePoint m_end;
    bool m_running = false;
};

} // namespace TimeUtils
