// ============================================================================
// circular_beacon_system.cpp — Circular Beacon Interconnect Implementation
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions, no external deps
//
// Implements BeaconHub singleton, PanelBeaconBridge, and extern "C" bridges.
// Every panel and subsystem registers here; any beacon can reach any other.
//
// Threading: All public methods are thread-safe (std::mutex).
// Error handling: bool returns / status codes — no exceptions.
//
// RULE: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "../../include/circular_beacon_system.h"
#include <cstdio>
#include <algorithm>

namespace RawrXD {

// ============================================================================
// BeaconHub — Registration
// ============================================================================

bool BeaconHub::registerBeacon(BeaconKind kind, const char* name, void* instance, BeaconHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t key = static_cast<uint32_t>(kind);
    
    // Allow re-registration (update instance/handler)
    auto it = m_beaconMap.find(key);
    if (it != m_beaconMap.end()) {
        it->second.instance = instance;
        it->second.handler = handler;
        it->second.active = true;
        if (name) it->second.name = name;
        return true;
    }

    BeaconEntry entry;
    entry.kind          = kind;
    entry.name          = name ? name : "unnamed";
    entry.instance      = instance;
    entry.handler       = handler;
    entry.active        = true;
    entry.msgSentCount  = 0;
    entry.msgRecvCount  = 0;
    entry.registeredAt  = GetTickCount64();

    m_beaconMap[key] = std::move(entry);
    m_ringOrder.push_back(kind);

    char buf[256];
    snprintf(buf, sizeof(buf), "[BeaconHub] Registered: %s (0x%04X) — ring position %zu",
             name ? name : "?", key, m_ringOrder.size());
    OutputDebugStringA(buf);
    
    return true;
}

bool BeaconHub::unregisterBeacon(BeaconKind kind) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t key = static_cast<uint32_t>(kind);
    
    auto it = m_beaconMap.find(key);
    if (it == m_beaconMap.end()) return false;
    
    m_beaconMap.erase(it);
    m_ringOrder.erase(
        std::remove(m_ringOrder.begin(), m_ringOrder.end(), kind),
        m_ringOrder.end()
    );
    return true;
}

bool BeaconHub::isRegistered(BeaconKind kind) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_beaconMap.count(static_cast<uint32_t>(kind)) > 0;
}

void BeaconHub::setActive(BeaconKind kind, bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_beaconMap.find(static_cast<uint32_t>(kind));
    if (it != m_beaconMap.end()) {
        it->second.active = active;
    }
}

void* BeaconHub::getBeaconRaw(BeaconKind kind) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_beaconMap.find(static_cast<uint32_t>(kind));
    if (it == m_beaconMap.end() || !it->second.active) return nullptr;
    return it->second.instance;
}

const char* BeaconHub::getBeaconName(BeaconKind kind) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_beaconMap.find(static_cast<uint32_t>(kind));
    if (it == m_beaconMap.end()) return "unknown";
    return it->second.name.c_str();
}

// ============================================================================
// BeaconHub — Message Dispatch
// ============================================================================

BeaconResponse BeaconHub::send(BeaconKind from, BeaconKind to, const char* verb,
                                const char* payload, size_t payloadLen, uint32_t flags) {
    BeaconResponse resp{};
    resp.handled = false;
    resp.statusCode = -1;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    uint32_t fromKey = static_cast<uint32_t>(from);
    uint32_t toKey   = static_cast<uint32_t>(to);

    // Update sender stats
    auto fromIt = m_beaconMap.find(fromKey);
    if (fromIt != m_beaconMap.end()) {
        fromIt->second.msgSentCount++;
    }

    // Find target
    auto toIt = m_beaconMap.find(toKey);
    if (toIt == m_beaconMap.end() || !toIt->second.active) {
        resp.result = "target beacon not found or inactive";
        return resp;
    }

    if (!toIt->second.handler) {
        resp.result = "target beacon has no handler";
        return resp;
    }

    // Build message
    BeaconMessage msg{};
    msg.messageId   = m_nextMsgId.fetch_add(1);
    msg.sourceKind  = from;
    msg.targetKind  = to;
    msg.direction   = BeaconDirection::Forward;
    msg.verb        = verb;
    msg.payload     = payload;
    msg.payloadLen  = payloadLen;
    msg.userData    = nullptr;
    msg.flags       = flags;

    // Dispatch
    toIt->second.msgRecvCount++;
    resp = toIt->second.handler(msg);
    resp.responderKind = to;
    
    return resp;
}

std::vector<BeaconResponse> BeaconHub::broadcast(BeaconKind from, const char* verb,
                                                   const char* payload, size_t payloadLen) {
    std::vector<BeaconResponse> results;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_totalBroadcasts++;

    auto fromIt = m_beaconMap.find(static_cast<uint32_t>(from));
    if (fromIt != m_beaconMap.end()) {
        fromIt->second.msgSentCount++;
    }

    BeaconMessage msg{};
    msg.messageId   = m_nextMsgId.fetch_add(1);
    msg.sourceKind  = from;
    msg.targetKind  = BeaconKind::Unknown; // broadcast
    msg.direction   = BeaconDirection::Circular;
    msg.verb        = verb;
    msg.payload     = payload;
    msg.payloadLen  = payloadLen;
    msg.userData    = nullptr;
    msg.flags       = BeaconMessage::FLAG_FIRE_AND_FORGET;

    for (auto& [key, entry] : m_beaconMap) {
        if (key == static_cast<uint32_t>(from)) continue; // skip sender
        if (!entry.active || !entry.handler) continue;
        
        entry.msgRecvCount++;
        BeaconResponse r = entry.handler(msg);
        r.responderKind = entry.kind;
        results.push_back(std::move(r));
    }

    return results;
}

std::vector<BeaconResponse> BeaconHub::sendDirectional(BeaconKind from, const char* verb,
                                                        BeaconDirection dir,
                                                        const char* payload, size_t payloadLen) {
    std::vector<BeaconResponse> results;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_ringOrder.empty()) return results;

    // Find sender position in ring
    size_t fromPos = 0;
    for (size_t i = 0; i < m_ringOrder.size(); ++i) {
        if (m_ringOrder[i] == from) { fromPos = i; break; }
    }

    BeaconMessage msg{};
    msg.messageId   = m_nextMsgId.fetch_add(1);
    msg.sourceKind  = from;
    msg.targetKind  = BeaconKind::Unknown;
    msg.direction   = dir;
    msg.verb        = verb;
    msg.payload     = payload;
    msg.payloadLen  = payloadLen;
    msg.userData    = nullptr;
    msg.flags       = BeaconMessage::FLAG_FIRE_AND_FORGET;

    auto dispatch = [&](size_t idx) {
        BeaconKind kind = m_ringOrder[idx];
        if (kind == from) return;
        auto it = m_beaconMap.find(static_cast<uint32_t>(kind));
        if (it == m_beaconMap.end() || !it->second.active || !it->second.handler) return;
        it->second.msgRecvCount++;
        BeaconResponse r = it->second.handler(msg);
        r.responderKind = kind;
        results.push_back(std::move(r));
    };

    size_t n = m_ringOrder.size();
    switch (dir) {
        case BeaconDirection::Forward:
            // Forward from sender position to end, wrap to beginning
            for (size_t i = 1; i < n; ++i) {
                dispatch((fromPos + i) % n);
            }
            break;

        case BeaconDirection::Reverse:
            // Reverse from sender position backward
            for (size_t i = 1; i < n; ++i) {
                dispatch((fromPos + n - i) % n);
            }
            break;

        case BeaconDirection::Bilateral:
            // Forward AND reverse simultaneously (deduplicated)
            for (size_t i = 1; i < n; ++i) {
                dispatch((fromPos + i) % n);
            }
            break;

        case BeaconDirection::Middle:
            // Through hub center: subsystems first (0x00xx), then panels (0x10xx)
            for (auto& [key, entry] : m_beaconMap) {
                if (key == static_cast<uint32_t>(from)) continue;
                if (!entry.active || !entry.handler) continue;
                if ((key & 0xF000) == 0x0000) { // Subsystems first
                    entry.msgRecvCount++;
                    BeaconResponse r = entry.handler(msg);
                    r.responderKind = entry.kind;
                    results.push_back(std::move(r));
                }
            }
            for (auto& [key, entry] : m_beaconMap) {
                if (key == static_cast<uint32_t>(from)) continue;
                if (!entry.active || !entry.handler) continue;
                if ((key & 0xF000) != 0x0000) { // Then panels + languages
                    entry.msgRecvCount++;
                    BeaconResponse r = entry.handler(msg);
                    r.responderKind = entry.kind;
                    results.push_back(std::move(r));
                }
            }
            break;

        case BeaconDirection::Circular:
            // Full ring in insertion order
            for (size_t i = 0; i < n; ++i) {
                dispatch(i);
            }
            break;
    }

    return results;
}

// ============================================================================
// BeaconHub — Enumeration
// ============================================================================

std::vector<BeaconKind> BeaconHub::getActiveBeacons() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BeaconKind> result;
    for (auto& [key, entry] : m_beaconMap) {
        if (entry.active) result.push_back(entry.kind);
    }
    return result;
}

std::vector<BeaconKind> BeaconHub::getBeaconsByPrefix(uint32_t kindPrefix) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BeaconKind> result;
    uint32_t mask = kindPrefix << 12; // e.g. 0x1 → 0x1000
    for (auto& [key, entry] : m_beaconMap) {
        if ((key & 0xF000) == mask && entry.active) {
            result.push_back(entry.kind);
        }
    }
    return result;
}

size_t BeaconHub::getBeaconCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_beaconMap.size();
}

BeaconHub::HubStats BeaconHub::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    HubStats stats{};
    stats.totalBeacons = m_beaconMap.size();
    stats.totalBroadcasts = m_totalBroadcasts;
    for (auto& [key, entry] : m_beaconMap) {
        if (entry.active) stats.activeBeacons++;
        stats.totalMessagesSent += entry.msgSentCount;
        stats.totalMessagesReceived += entry.msgRecvCount;
    }
    return stats;
}

void BeaconHub::walkRing(BeaconKind start, std::function<bool(const BeaconEntry&)> visitor) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_ringOrder.empty()) return;

    size_t startPos = 0;
    for (size_t i = 0; i < m_ringOrder.size(); ++i) {
        if (m_ringOrder[i] == start) { startPos = i; break; }
    }

    size_t n = m_ringOrder.size();
    for (size_t i = 0; i < n; ++i) {
        size_t idx = (startPos + i) % n;
        auto it = m_beaconMap.find(static_cast<uint32_t>(m_ringOrder[idx]));
        if (it != m_beaconMap.end()) {
            if (!visitor(it->second)) return; // visitor said stop
        }
    }
}

// ============================================================================
// BeaconHub — Verb routing
// ============================================================================

void BeaconHub::registerVerb(const char* verb, BeaconKind handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (verb) m_verbRoutes[verb] = handler;
}

BeaconKind BeaconHub::resolveVerb(const char* verb) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!verb) return BeaconKind::Unknown;
    auto it = m_verbRoutes.find(verb);
    return (it != m_verbRoutes.end()) ? it->second : BeaconKind::Unknown;
}

// ============================================================================
// PanelBeaconBridge
// ============================================================================

PanelBeaconBridge::~PanelBeaconBridge() {
    shutdown();
}

bool PanelBeaconBridge::init(BeaconKind kind, const char* name, void* panelInstance,
                              BeaconHandler handler) {
    if (m_initialized) shutdown();
    
    m_kind = kind;
    bool ok = BeaconHub::instance().registerBeacon(kind, name, panelInstance, handler);
    m_initialized = ok;
    return ok;
}

void PanelBeaconBridge::shutdown() {
    if (m_initialized) {
        BeaconHub::instance().unregisterBeacon(m_kind);
        m_initialized = false;
        m_kind = BeaconKind::Unknown;
    }
}

BeaconResponse PanelBeaconBridge::sendTo(BeaconKind target, const char* verb,
                                          const char* payload, size_t len) {
    return BeaconHub::instance().send(m_kind, target, verb, payload, len);
}

std::vector<BeaconResponse> PanelBeaconBridge::broadcast(const char* verb,
                                                          const char* payload, size_t len) {
    return BeaconHub::instance().broadcast(m_kind, verb, payload, len);
}

std::vector<BeaconResponse> PanelBeaconBridge::sendDirectional(const char* verb,
                                                                BeaconDirection dir,
                                                                const char* payload, size_t len) {
    return BeaconHub::instance().sendDirectional(m_kind, verb, dir, payload, len);
}

// ============================================================================
// beaconKindToString
// ============================================================================

const char* beaconKindToString(BeaconKind kind) {
    switch (kind) {
        // Core Subsystems
        case BeaconKind::AgenticEngine:      return "AgenticEngine";
        case BeaconKind::HotpatchManager:    return "HotpatchManager";
        case BeaconKind::PlanExecutor:       return "PlanExecutor";
        case BeaconKind::AutonomyManager:    return "AutonomyManager";
        case BeaconKind::LLMRouter:          return "LLMRouter";
        case BeaconKind::EncryptionEngine:   return "EncryptionEngine";
        case BeaconKind::IDECore:            return "IDECore";
        case BeaconKind::InferenceEngine:    return "InferenceEngine";
        case BeaconKind::FeatureRegistry:    return "FeatureRegistry";
        case BeaconKind::CommandDispatch:    return "CommandDispatch";
        case BeaconKind::ToolRegistry:       return "ToolRegistry";
        case BeaconKind::SubAgentManager:    return "SubAgentManager";
        case BeaconKind::SwarmCoordinator:   return "SwarmCoordinator";
        case BeaconKind::KnowledgeGraph:     return "KnowledgeGraph";
        case BeaconKind::DebugEngine:        return "DebugEngine";
        case BeaconKind::LSPServer:          return "LSPServer";
        case BeaconKind::VulkanCompute:      return "VulkanCompute";
        case BeaconKind::FlightRecorder:     return "FlightRecorder";
        case BeaconKind::CrashContainment:   return "CrashContainment";
        case BeaconKind::TelemetryCore:      return "TelemetryCore";
        case BeaconKind::MCPServer:          return "MCPServer";
        case BeaconKind::SessionManager:     return "SessionManager";
        case BeaconKind::BackendSwitcher:    return "BackendSwitcher";
        case BeaconKind::ProxyHotpatcher:    return "ProxyHotpatcher";
        case BeaconKind::SelfRepairAgent:    return "SelfRepairAgent";
        case BeaconKind::VoiceEngine:        return "VoiceEngine";
        case BeaconKind::StaticAnalysis:     return "StaticAnalysis";
        case BeaconKind::SemanticIntel:      return "SemanticIntel";
        case BeaconKind::GitIntegration:     return "GitIntegration";

        // GUI Panels
        case BeaconKind::PanelChat:           return "PanelChat";
        case BeaconKind::PanelAgent:          return "PanelAgent";
        case BeaconKind::PanelHotpatch:       return "PanelHotpatch";
        case BeaconKind::PanelHotpatchCtrl:   return "PanelHotpatchCtrl";
        case BeaconKind::PanelSwarm:          return "PanelSwarm";
        case BeaconKind::PanelDualAgent:      return "PanelDualAgent";
        case BeaconKind::PanelCrucible:       return "PanelCrucible";
        case BeaconKind::PanelTranscendence:  return "PanelTranscendence";
        case BeaconKind::PanelPipeline:       return "PanelPipeline";
        case BeaconKind::PanelSemantic:       return "PanelSemantic";
        case BeaconKind::PanelStaticAnalysis: return "PanelStaticAnalysis";
        case BeaconKind::PanelTelemetry:      return "PanelTelemetry";
        case BeaconKind::PanelVoiceChat:      return "PanelVoiceChat";
        case BeaconKind::PanelNetwork:        return "PanelNetwork";
        case BeaconKind::PanelPowerShell:     return "PanelPowerShell";
        case BeaconKind::PanelGit:            return "PanelGit";
        case BeaconKind::PanelFailureIntel:   return "PanelFailureIntel";
        case BeaconKind::PanelAgentHistory:   return "PanelAgentHistory";
        case BeaconKind::PanelDebugger:       return "PanelDebugger";
        case BeaconKind::PanelDecompiler:     return "PanelDecompiler";
        case BeaconKind::PanelAudit:          return "PanelAudit";
        case BeaconKind::PanelGauntlet:       return "PanelGauntlet";
        case BeaconKind::PanelMarketplace:    return "PanelMarketplace";
        case BeaconKind::PanelTestExplorer:   return "PanelTestExplorer";
        case BeaconKind::PanelGameEngine:     return "PanelGameEngine";
        case BeaconKind::PanelCopilotGap:     return "PanelCopilotGap";
        case BeaconKind::PanelProvableAgent:  return "PanelProvableAgent";
        case BeaconKind::PanelAIReverseEng:   return "PanelAIReverseEng";
        case BeaconKind::PanelAirgapped:      return "PanelAirgapped";
        case BeaconKind::PanelNativeDebug:    return "PanelNativeDebug";
        case BeaconKind::PanelShortcutEditor: return "PanelShortcutEditor";
        case BeaconKind::PanelColorPicker:    return "PanelColorPicker";
        case BeaconKind::PanelPlanDialog:     return "PanelPlanDialog";
        case BeaconKind::PanelOutline:        return "PanelOutline";
        case BeaconKind::PanelReference:      return "PanelReference";
        case BeaconKind::PanelBreadcrumbs:    return "PanelBreadcrumbs";
        case BeaconKind::PanelMCP:            return "PanelMCP";
        case BeaconKind::PanelInstructions:   return "PanelInstructions";
        case BeaconKind::PanelCursorParity:   return "PanelCursorParity";
        case BeaconKind::PanelFlagship:       return "PanelFlagship";

        // Language/Domain
        case BeaconKind::BeaconJava:          return "Java";
        case BeaconKind::BeaconPython:        return "Python";
        case BeaconKind::BeaconMASM:          return "MASM";
        case BeaconKind::BeaconRust:          return "Rust";
        case BeaconKind::BeaconTypeScript:    return "TypeScript";
        case BeaconKind::BeaconCSharp:        return "CSharp";
        case BeaconKind::BeaconGo:            return "Go";
        case BeaconKind::BeaconEncryption:    return "Encryption";
        case BeaconKind::BeaconCompression:   return "Compression";
        case BeaconKind::BeaconReverseEng:    return "ReverseEngineering";
        case BeaconKind::BeaconSecurity:      return "Security";
        case BeaconKind::BeaconCustom:        return "Custom";
        
        default: return "Unknown";
    }
}

// ============================================================================
// Extern "C" bridges — MASM / cross-module dispatch
// ============================================================================

extern "C" int rawrxd_beacon_register(uint32_t kind, const char* name, void* instance) {
    return RawrXD::BeaconHub::instance().registerBeacon(
        static_cast<RawrXD::BeaconKind>(kind), name, instance, nullptr) ? 1 : 0;
}

extern "C" int rawrxd_beacon_unregister(uint32_t kind) {
    return RawrXD::BeaconHub::instance().unregisterBeacon(
        static_cast<RawrXD::BeaconKind>(kind)) ? 1 : 0;
}

extern "C" void* rawrxd_beacon_get(uint32_t kind) {
    return RawrXD::BeaconHub::instance().getBeaconRaw(
        static_cast<RawrXD::BeaconKind>(kind));
}

extern "C" int rawrxd_beacon_send(uint32_t from, uint32_t to, const char* verb,
                                   const char* payload, size_t payloadLen) {
    auto resp = RawrXD::BeaconHub::instance().send(
        static_cast<RawrXD::BeaconKind>(from),
        static_cast<RawrXD::BeaconKind>(to),
        verb, payload, payloadLen);
    return resp.handled ? 0 : -1;
}

extern "C" int rawrxd_beacon_broadcast(uint32_t from, const char* verb,
                                        const char* payload, size_t payloadLen) {
    auto results = RawrXD::BeaconHub::instance().broadcast(
        static_cast<RawrXD::BeaconKind>(from), verb, payload, payloadLen);
    int handled = 0;
    for (auto& r : results) {
        if (r.handled) handled++;
    }
    return handled;
}

extern "C" int rawrxd_beacon_count() {
    return static_cast<int>(RawrXD::BeaconHub::instance().getBeaconCount());
}

} // namespace RawrXD
