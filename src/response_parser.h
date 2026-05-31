#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// Logger — Structured logging with OutputDebugString + stderr
// ============================================================================
class Logger {
public:
    template<typename... Args>
    void info(const char* fmt, Args&&... args) {
        logImpl("INFO", fmt, std::forward<Args>(args)...);
    }
    template<typename... Args>
    void debug(const char* fmt, Args&&... args) {
#ifndef NDEBUG
        logImpl("DEBUG", fmt, std::forward<Args>(args)...);
#else
        (void)fmt; ((void)args, ...);
#endif
    }
    template<typename... Args>
    void warn(const char* fmt, Args&&... args) {
        logImpl("WARN", fmt, std::forward<Args>(args)...);
    }
    template<typename... Args>
    void error(const char* fmt, Args&&... args) {
        logImpl("ERROR", fmt, std::forward<Args>(args)...);
    }

private:
    template<typename... Args>
    void logImpl(const char* level, const char* fmt, Args&&... args) {
        char buf[1024];
        int off = snprintf(buf, sizeof(buf), "[%s] [ResponseParser] ", level);
        if (off > 0 && off < (int)sizeof(buf)) {
            snprintf(buf + off, sizeof(buf) - off, fmt, std::forward<Args>(args)...);
        }
        fprintf(stderr, "%s\n", buf);
#ifdef _WIN32
        OutputDebugStringA(buf);
        OutputDebugStringA("\n");
#endif
    }
};

// ============================================================================
// Metrics — Counter + histogram tracking with atomic operations
// ============================================================================
class Metrics {
public:
    void recordHistogram(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& h = m_histograms[name];
        h.count++;
        h.sum += value;
        if (value < h.min) h.min = value;
        if (value > h.max) h.max = value;
    }

    void incrementCounter(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counters[name]++;
    }

    uint64_t getCounter(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_counters.find(name);
        return it != m_counters.end() ? it->second : 0;
    }

    struct HistogramStats {
        uint64_t count = 0;
        double sum = 0.0;
        double min = 1e18;
        double max = -1e18;
        double avg() const { return count > 0 ? sum / count : 0.0; }
    };

    HistogramStats getHistogram(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_histograms.find(name);
        return it != m_histograms.end() ? it->second : HistogramStats{};
    }

private:
    mutable std::mutex                                   m_mutex;
    std::unordered_map<std::string, uint64_t>            m_counters;
    std::unordered_map<std::string, HistogramStats>      m_histograms;
};

enum class BoundaryType {
    Statement,
    Block,
    Section,
    None
};

struct ParsedCompletion {
    std::string text;
    int tokenCount = 0;
    double confidence = 0.0;
    std::string boundary;
    bool isComplete = false;
};

class ResponseParser {
public:
    ResponseParser(std::shared_ptr<Logger> logger, std::shared_ptr<Metrics> metrics);
    
    std::vector<ParsedCompletion> parseResponse(const std::string& response);
    std::vector<ParsedCompletion> parseChunk(const std::string& chunk);
    
    std::vector<ParsedCompletion> flush();
    std::pair<BoundaryType, std::string> detectBoundary(const std::string& text);
    std::vector<std::pair<std::string, double>> getStatistics() const;
    void reset();
    void setCustomDelimiters(const std::vector<std::string>& delimiters);
    void setStatementBoundaries(const std::vector<std::string>& boundaries);

private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    
    std::vector<std::string> m_statementBoundaries = {";", "}\n", "\n\n"};
    std::vector<std::string> m_customDelimiters;
    std::string m_buffer;
    size_t m_totalCharsParsed = 0;

    std::vector<ParsedCompletion> splitByStatementBoundaries(const std::string& text);
    std::vector<ParsedCompletion> splitByLineBoundaries(const std::string& text);
    std::vector<ParsedCompletion> splitByTokenBoundaries(const std::string& text);
    
    int estimateTokenCount(const std::string& text);
    double calculateConfidence(const std::string& text);
};
