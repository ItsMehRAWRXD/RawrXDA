#pragma once
// telemetry_collector.hpp – Qt-free telemetry (C++20 / Win32)
#include "../json_types.hpp"
#include <cstdint>
#include <mutex>
#include <string>

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
