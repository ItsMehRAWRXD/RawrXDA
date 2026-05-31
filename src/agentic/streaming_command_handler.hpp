#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <memory>

namespace RawrXD::Agentic {

/**
 * StreamingCommandHandler - Bridge between slash commands and streaming engine
 * 
 * Handles:
 * - /streaming status      → Phase-aware metrics snapshot
 * - /streaming autopatch   → Toggle autopatch and mmap fallback
 * - /streaming throttle    → Adjust TPS cap
 * 
 * Integrates with ExtensionAPIBridge for IDE feedback
 */
class StreamingCommandHandler {
public:
    static StreamingCommandHandler& instance();
    
    // Command handlers
    nlohmann::json HandleStatus();
    nlohmann::json HandleAutopatch(const std::string& state);  // "on" or "off"
    nlohmann::json HandleThrottle(double tps);  // 0 = unlimited
    
    // Configuration
    void SetAutopatchEnabled(bool enabled) { m_autopatchEnabled = enabled; }
    void SetTpsThrottle(double tps) { m_tpsThrottle = tps; }
    bool IsAutopatchEnabled() const { return m_autopatchEnabled; }
    double GetTpsThrottle() const { return m_tpsThrottle; }
    
private:
    StreamingCommandHandler() = default;
    
    bool m_autopatchEnabled = true;
    double m_tpsThrottle = 0.0;  // 0 = unlimited
};

} // namespace RawrXD::Agentic
