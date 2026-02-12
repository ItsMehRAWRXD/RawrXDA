#pragma once
// telemetry_collector.hpp – Qt-free telemetry (C++20 / Win32)
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// Lightweight JSON-like value for telemetry metadata (no nlohmann dependency)
struct JsonValue {
    enum Type { Null, Bool, Int, Double, String, Array, Object };
    Type type = Null;

    bool        boolVal   = false;
    int64_t     intVal    = 0;
    double      doubleVal = 0.0;
    std::string strVal;
    std::vector<JsonValue>                          arrVal;
    std::unordered_map<std::string, JsonValue>      objVal;

    JsonValue() = default;
    JsonValue(bool v)               : type(Bool),   boolVal(v) {}
    JsonValue(int v)                : type(Int),    intVal(v) {}
    JsonValue(int64_t v)            : type(Int),    intVal(v) {}
    JsonValue(double v)             : type(Double), doubleVal(v) {}
    JsonValue(const char* v)        : type(String), strVal(v ? v : "") {}
    JsonValue(const std::string& v) : type(String), strVal(v) {}
};
using JsonObject = std::unordered_map<std::string, JsonValue>;

/**
 * Privacy-respecting telemetry for feature usage and crash analysis.
 *   - Opt-in only (disabled by default)
 *   - No PII collection
 *   - Anonymous session IDs
 *   - GDPR/CCPA compliant
 */
class TelemetryCollector {
public:
    static TelemetryCollector* instance();

    bool initialize();
    bool isEnabled() const { return m_enabled; }

    void enableTelemetry();
    void disableTelemetry();

    void trackFeatureUsage(const std::string& featureName, const JsonObject& metadata = {});
    void trackCrash(const std::string& crashReason);
    void trackPerformance(const std::string& metricName, double value, const std::string& unit = {});

    JsonObject getAllTelemetryData() const;
    void       clearAllData();
    void       flushData();

    // --- Callbacks (replaces Qt signals) ---
    using VoidCb    = void(*)(void* ctx);
    using FlushCb   = void(*)(void* ctx, int eventCount);

    void setEnabledCb(VoidCb cb, void* ctx)   { m_enabledCb  = cb; m_enabledCtx  = ctx; }
    void setDisabledCb(VoidCb cb, void* ctx)   { m_disabledCb = cb; m_disabledCtx = ctx; }
    void setFlushedCb(FlushCb cb, void* ctx)   { m_flushedCb  = cb; m_flushedCtx  = ctx; }

private:
    TelemetryCollector();
    ~TelemetryCollector();

    static TelemetryCollector* s_instance;

    bool        m_enabled = false;
    std::string m_sessionId;
    int64_t     m_sessionStartTime = 0;

    mutable std::mutex                          m_mutex;
    std::unordered_map<std::string, int>        m_featureUsage;
    std::vector<JsonObject>                     m_events;

    std::string sanitize(const std::string& input) const;
    void        sendTelemetry(const JsonObject& payload);
    bool        loadUserConsent() const;
    void        saveUserConsent(bool enabled);

    // Callback state
    VoidCb  m_enabledCb  = nullptr;  void* m_enabledCtx  = nullptr;
    VoidCb  m_disabledCb = nullptr;  void* m_disabledCtx = nullptr;
    FlushCb m_flushedCb  = nullptr;  void* m_flushedCtx  = nullptr;
};
