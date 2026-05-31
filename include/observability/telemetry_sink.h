#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <map>

namespace RawrXD::Observability {

enum class TelemetryLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    FATAL = 4
};

struct TelemetryEvent {
    std::string category;
    std::string name;
    double value = 0.0;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> attributes;
    TelemetryLevel level = TelemetryLevel::INFO;
};

class TelemetrySink {
public:
    static TelemetrySink& getInstance();
    
    void recordEvent(const TelemetryEvent& evt);
    void recordEvent(const std::string& category, const std::string& name, double value);
    
    void flush();
    void setOutputPath(const std::string& path);
    void setFlushThreshold(size_t threshold);
    void setMinLevel(TelemetryLevel level);
    
    // Batch operations
    void recordBatch(const std::vector<TelemetryEvent>& events);
    
    // Query
    std::vector<TelemetryEvent> queryEvents(const std::string& category, 
                                               std::chrono::system_clock::time_point since);
    
    // Stats
    size_t getBufferedCount() const;
    size_t getTotalEvents() const;

private:
    TelemetrySink() = default;
    ~TelemetrySink() { flush(); }
    
    TelemetrySink(const TelemetrySink&) = delete;
    TelemetrySink& operator=(const TelemetrySink&) = delete;
    
    void flushUnlocked();
    std::string formatEvent(const TelemetryEvent& evt) const;
    
    mutable std::mutex m_mutex;
    std::vector<TelemetryEvent> m_buffer;
    std::string m_outputPath = "rawrxd_telemetry.log";
    size_t m_flushThreshold = 100;
    TelemetryLevel m_minLevel = TelemetryLevel::DEBUG;
    size_t m_totalEvents = 0;
};

} // namespace RawrXD::Observability
