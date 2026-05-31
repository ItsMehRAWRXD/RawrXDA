// GhostTextContextSubscriber.h — Ghost Text adapter for ContextFusionEngine
// Makes ghost text consume unified context instead of building its own.

#pragma once

#include "core/ContextFusionEngine.h"
#include "win32app/Win32IDE_GhostText.h"

namespace RawrXD {

class GhostTextContextSubscriber : public IContextSubscriber {
public:
    GhostTextContextSubscriber(Win32IDE_GhostText* ghostText);
    
    // IContextSubscriber
    void OnContextUpdate(const ContextFrame& frame) override;
    void OnContextEvent(const ContextEvent& event) override;
    int GetPriority() const override { return 10; } // High priority
    std::string GetName() const override { return "GhostTextSubscriber"; }
    
    // Configuration
    void SetDebounceMs(int ms) { m_debounceMs = ms; }
    void SetMinConfidence(float conf) { m_minConfidence = conf; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }

    // Latency telemetry (zero-overhead, populated after each request)
    static int64_t GetLastLatencyUs();
    static int GetRequestCount();
    static uint64_t GetStaleFrameCount();
    static uint64_t GetTotalFrameCount();
    static double GetStaleRate();
    static void ResetTelemetry();

private:
    Win32IDE_GhostText* m_ghostText;
    int m_debounceMs = 77;  // Calibrated for 28ms P50 latency
    float m_minConfidence = 0.6f;
    bool m_enabled = true;
    
    uint64_t m_lastVersion = 0;
    std::chrono::steady_clock::time_point m_lastUpdate;
    
    void RequestGhostText(const ContextFrame& frame);
    bool ShouldRequest(const ContextFrame& frame) const;
};

} // namespace RawrXD