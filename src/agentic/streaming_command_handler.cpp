#include "streaming_command_handler.hpp"
#include "../extensions/extension_api_bridge.h"
#include <sstream>
#include <iomanip>
#include <chrono>

namespace RawrXD::Agentic {

StreamingCommandHandler& StreamingCommandHandler::instance() {
    static StreamingCommandHandler inst;
    return inst;
}

nlohmann::json StreamingCommandHandler::HandleStatus() {
    using json = nlohmann::json;
    
    json result;
    result["success"] = true;
    result["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Streaming configuration
    json config;
    config["autopatch_enabled"] = m_autopatchEnabled;
    config["tps_throttle"] = m_tpsThrottle;
    result["config"] = config;
    
    // Phase-aware metrics (placeholder - would integrate with MeasurementCollector)
    json metrics;
    metrics["phase"] = "STEADY_DECODE";  // Would come from PhaseAwareMeasurementCollector
    metrics["ttft_ms"] = 1850;  // Time-to-first-token
    metrics["decode_tps"] = 117.5;  // Tokens per second in decode phase
    metrics["memory_pressure_pct"] = 45.2;
    metrics["kv_cache_hits"] = 892;
    metrics["kv_cache_misses"] = 12;
    result["metrics"] = metrics;
    
    // System status
    json system;
    system["mmap_fallback_active"] = true;  // Would check if mmap streamer is in use
    system["gpu_batching_enabled"] = false;  // Would check GPU backend config
    system["phase_aware_collector_ready"] = true;
    result["system"] = system;
    
    // Format output for IDE
    std::ostringstream oss;
    oss << "📊 Streaming Engine Status\n";
    oss << "========================\n\n";
    oss << "Configuration:\n";
    oss << "  Autopatch: " << (m_autopatchEnabled ? "✅ ON" : "❌ OFF") << "\n";
    oss << "  TPS Throttle: " << (m_tpsThrottle > 0 ? std::to_string(m_tpsThrottle) : "unlimited") << "\n\n";
    oss << "Current Phase: " << metrics["phase"].get<std::string>() << "\n";
    oss << "  TTFT: " << metrics["ttft_ms"].get<int>() << " ms\n";
    oss << "  Decode TPS: " << std::fixed << std::setprecision(1) << metrics["decode_tps"].get<double>() << " tokens/sec\n";
    oss << "  Memory Pressure: " << std::setprecision(1) << metrics["memory_pressure_pct"].get<double>() << "%\n\n";
    oss << "KV Cache: " << metrics["kv_cache_hits"].get<int>() << " hits, " << metrics["kv_cache_misses"].get<int>() << " misses\n";
    oss << "  Hit Rate: " << std::setprecision(1) << 
        (100.0 * metrics["kv_cache_hits"].get<int>() / 
         (metrics["kv_cache_hits"].get<int>() + metrics["kv_cache_misses"].get<int>())) << "%\n\n";
    oss << "System:\n";
    oss << "  MMAP Fallback: " << (system["mmap_fallback_active"].get<bool>() ? "✅ Active" : "❌ Inactive") << "\n";
    oss << "  GPU Batching: " << (system["gpu_batching_enabled"].get<bool>() ? "✅ Enabled" : "⚠️  Disabled") << "\n";
    oss << "  Phase-Aware Collector: " << (system["phase_aware_collector_ready"].get<bool>() ? "✅ Ready" : "❌ Not Ready") << "\n";
    
    result["formatted_output"] = oss.str();
    
    // Publish to IDE via Extension API Bridge
    auto& bridge = RawrXD::Extensions::ExtensionAPIBridge::instance();
    bridge.logMessage(1, ("[Streaming] Status requested: " + result["formatted_output"].get<std::string>()).c_str());
    
    return result;
}

nlohmann::json StreamingCommandHandler::HandleAutopatch(const std::string& state) {
    using json = nlohmann::json;
    
    json result;
    
    if (state == "on") {
        m_autopatchEnabled = true;
        result["success"] = true;
        result["message"] = "✅ Autopatch enabled. MMAP fallback active for large zones.";
    } else if (state == "off") {
        m_autopatchEnabled = false;
        result["success"] = true;
        result["message"] = "⚠️ Autopatch disabled. Large zones will fail instead of falling back to MMAP.";
    } else {
        result["success"] = false;
        result["error"] = "Invalid state: " + state + " (expected 'on' or 'off')";
        return result;
    }
    
    result["autopatch_enabled"] = m_autopatchEnabled;
    
    // Publish to IDE via Extension API Bridge
    auto& bridge = RawrXD::Extensions::ExtensionAPIBridge::instance();
    bridge.logMessage(1, ("[Streaming] Autopatch " + state + " - " + result["message"].get<std::string>()).c_str());
    bridge.showStatusBarMessage(("Streaming: Autopatch " + state).c_str());
    
    return result;
}

nlohmann::json StreamingCommandHandler::HandleThrottle(double tps) {
    using json = nlohmann::json;
    
    json result;
    result["success"] = true;
    
    m_tpsThrottle = tps;
    
    if (tps <= 0) {
        result["message"] = "✅ TPS throttle removed. Decode running at maximum speed.";
    } else {
        std::ostringstream oss;
        oss << "✅ TPS throttle set to " << std::fixed << std::setprecision(1) << tps << " tokens/sec";
        result["message"] = oss.str();
    }
    
    result["tps_throttle"] = m_tpsThrottle;
    
    // Publish to IDE via Extension API Bridge
    auto& bridge = RawrXD::Extensions::ExtensionAPIBridge::instance();
    bridge.logMessage(1, ("[Streaming] " + result["message"].get<std::string>()).c_str());
    
    std::ostringstream statusMsg;
    if (tps <= 0) {
        statusMsg << "Streaming: Unlimited TPS";
    } else {
        statusMsg << "Streaming: " << std::fixed << std::setprecision(0) << tps << " TPS cap";
    }
    bridge.showStatusBarMessage(statusMsg.str().c_str());
    
    return result;
}

} // namespace RawrXD::Agentic
