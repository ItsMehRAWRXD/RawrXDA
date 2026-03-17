// ============================================================================
// CircularBeaconSystem.cpp — Implementation of BeaconHub, PanelBeaconBridge,
//                            CircularBeaconManager, and extern "C" bridges
// ============================================================================
// Architecture: C++17, Win32, no Qt, no external deps
// Implements: circular_beacon_system.h + CircularBeaconManager.h
// ============================================================================

#include "CircularBeaconSystem.h"
#include "CircularBeaconManager.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <sstream>
#include <cassert>
#include <functional>
#include "model_inference.hpp"
#include <thread>
#include <string>
#include <vector>
#include <cassert>
#include <windows.h>
#include <utility>

namespace RawrXD {

// ============================================================================
// BeaconHub — Singleton implementation
// ============================================================================

bool BeaconHub::registerBeacon(BeaconKind kind, const char* name, void* instance, BeaconHandler handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t key = static_cast<uint32_t>(kind);
    if (m_beaconMap.find(key) != m_beaconMap.end()) {
        OutputDebugStringA("[BeaconHub] WARNING: Beacon already registered: ");
        OutputDebugStringA(name ? name : "(null)");
        OutputDebugStringA("\n");
        return false;
    }

    BeaconEntry entry;
    entry.kind         = kind;
    entry.name         = name ? name : "";
    entry.instance     = instance;
    entry.handler      = std::move(handler);
    entry.active       = true;
    entry.msgSentCount = 0;
    entry.msgRecvCount = 0;
    entry.registeredAt = GetTickCount64();

    m_beaconMap[key] = std::move(entry);
    m_ringOrder.push_back(kind);

    OutputDebugStringA("[BeaconHub] Registered beacon: ");
    OutputDebugStringA(name ? name : "(null)");
    OutputDebugStringA("\n");

    return true;
}

bool BeaconHub::unregisterBeacon(BeaconKind kind)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t key = static_cast<uint32_t>(kind);
    auto it = m_beaconMap.find(key);
    if (it == m_beaconMap.end()) return false;

    OutputDebugStringA("[BeaconHub] Unregistered beacon: ");
    OutputDebugStringA(it->second.name.c_str());
    OutputDebugStringA("\n");

    m_beaconMap.erase(it);
    m_ringOrder.erase(
        std::remove(m_ringOrder.begin(), m_ringOrder.end(), kind),
        m_ringOrder.end()
    );

    return true;
}

bool BeaconHub::isRegistered(BeaconKind kind) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_beaconMap.find(static_cast<uint32_t>(kind)) != m_beaconMap.end();
}

void BeaconHub::setActive(BeaconKind kind, bool active)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_beaconMap.find(static_cast<uint32_t>(kind));
    if (it != m_beaconMap.end()) {
        it->second.active = active;
    }
}

// ── Dynamic beacon registration ──

BeaconKind BeaconHub::registerDynamicBeacon(const std::string& id, const char* name,
                                             void* instance, BeaconHandler handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto idIt = m_dynamicIdToKind.find(id);
    if (idIt != m_dynamicIdToKind.end()) {
        return idIt->second;
    }

    if (m_nextDynamicKind >= static_cast<uint32_t>(BeaconKind::DynamicEnd)) {
        OutputDebugStringA("[BeaconHub] ERROR: Dynamic beacon pool exhausted\n");
        return BeaconKind::Unknown;
    }

    BeaconKind dynKind = static_cast<BeaconKind>(m_nextDynamicKind++);
    uint32_t key = static_cast<uint32_t>(dynKind);

    BeaconEntry entry;
    entry.kind         = dynKind;
    entry.name         = name ? name : id;
    entry.instance     = instance;
    entry.handler      = std::move(handler);
    entry.active       = true;
    entry.msgSentCount = 0;
    entry.msgRecvCount = 0;
    entry.registeredAt = GetTickCount64();

    m_beaconMap[key] = std::move(entry);
    m_ringOrder.push_back(dynKind);
    m_dynamicIdToKind[id] = dynKind;
    m_dynamicKindToId[key] = id;

    return dynKind;
}

bool BeaconHub::unregisterDynamicBeacon(const std::string& id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto idIt = m_dynamicIdToKind.find(id);
    if (idIt == m_dynamicIdToKind.end()) return false;

    BeaconKind kind = idIt->second;
    uint32_t key = static_cast<uint32_t>(kind);

    m_beaconMap.erase(key);
    m_ringOrder.erase(std::remove(m_ringOrder.begin(), m_ringOrder.end(), kind), m_ringOrder.end());
    m_dynamicKindToId.erase(key);
    m_dynamicIdToKind.erase(idIt);

    return true;
}

bool BeaconHub::unregisterDynamicBeacon(BeaconKind dynamicKind)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t key = static_cast<uint32_t>(dynamicKind);
    auto kindIt = m_dynamicKindToId.find(key);
    if (kindIt == m_dynamicKindToId.end()) return false;

    std::string id = kindIt->second;
    m_beaconMap.erase(key);
    m_ringOrder.erase(std::remove(m_ringOrder.begin(), m_ringOrder.end(), dynamicKind), m_ringOrder.end());
    m_dynamicIdToKind.erase(id);
    m_dynamicKindToId.erase(kindIt);

    return true;
}

std::vector<std::pair<BeaconKind, std::string>> BeaconHub::getDynamicBeacons() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<BeaconKind, std::string>> result;
    result.reserve(m_dynamicIdToKind.size());
    for (const auto& [id, kind] : m_dynamicIdToKind) {
        result.emplace_back(kind, id);
    }
    return result;
}

// ── Raw instance access ──

void* BeaconHub::getBeaconRaw(BeaconKind kind) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_beaconMap.find(static_cast<uint32_t>(kind));
    if (it == m_beaconMap.end() || !it->second.active) return nullptr;
    return it->second.instance;
}

const char* BeaconHub::getBeaconName(BeaconKind kind) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_beaconMap.find(static_cast<uint32_t>(kind));
    if (it == m_beaconMap.end()) return "Unknown";
    return it->second.name.c_str();
}

// ── Message dispatch ──

BeaconResponse BeaconHub::send(BeaconKind from, BeaconKind to, const char* verb,
                                const char* payload, size_t payloadLen, uint32_t flags)
{
    BeaconResponse defaultResp{ false, -1, "", BeaconKind::Unknown };

    std::lock_guard<std::mutex> lock(m_mutex);

    auto fromIt = m_beaconMap.find(static_cast<uint32_t>(from));
    auto toIt   = m_beaconMap.find(static_cast<uint32_t>(to));

    if (toIt == m_beaconMap.end() || !toIt->second.active) {
        defaultResp.result = "Target beacon not found or inactive";
        return defaultResp;
    }

    if (!toIt->second.handler) {
        defaultResp.result = "Target beacon has no handler";
        return defaultResp;
    }

    BeaconMessage msg;
    msg.messageId  = m_nextMsgId.fetch_add(1);
    msg.sourceKind = from;
    msg.targetKind = to;
    msg.direction  = BeaconDirection::Forward;
    msg.verb       = verb;
    msg.payload    = payload;
    msg.payloadLen = payloadLen;
    msg.userData   = nullptr;
    msg.flags      = flags;

    if (fromIt != m_beaconMap.end()) fromIt->second.msgSentCount++;
    toIt->second.msgRecvCount++;

    BeaconResponse resp = toIt->second.handler(msg);
    resp.responderKind = to;
    return resp;
}

std::vector<BeaconResponse> BeaconHub::broadcast(BeaconKind from, const char* verb,
                                                   const char* payload, size_t payloadLen)
{
    std::vector<BeaconResponse> results;
    std::lock_guard<std::mutex> lock(m_mutex);

    m_totalBroadcasts++;

    auto fromIt = m_beaconMap.find(static_cast<uint32_t>(from));
    if (fromIt != m_beaconMap.end()) {
        fromIt->second.msgSentCount++;
    }

    BeaconMessage msg;
    msg.messageId  = m_nextMsgId.fetch_add(1);
    msg.sourceKind = from;
    msg.targetKind = BeaconKind::Unknown;
    msg.direction  = BeaconDirection::Circular;
    msg.verb       = verb;
    msg.payload    = payload;
    msg.payloadLen = payloadLen;
    msg.userData   = nullptr;
    msg.flags      = BeaconMessage::FLAG_FIRE_AND_FORGET;

    for (auto& [key, entry] : m_beaconMap) {
        if (entry.kind == from) continue;
        if (!entry.active || !entry.handler) continue;

        entry.msgRecvCount++;
        try {
            BeaconResponse resp = entry.handler(msg);
            resp.responderKind = entry.kind;
            results.push_back(std::move(resp));
        } catch (...) {
            results.push_back({ false, -1, "Exception in handler", entry.kind });
        }
    }

    return results;
}

std::vector<BeaconResponse> BeaconHub::sendDirectional(BeaconKind from, const char* verb,
                                                        BeaconDirection dir,
                                                        const char* payload, size_t payloadLen)
{
    if (dir == BeaconDirection::Circular || dir == BeaconDirection::Middle) {
        return broadcast(from, verb, payload, payloadLen);
    }

    std::vector<BeaconResponse> results;
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_ringOrder.empty()) return results;

    auto startIt = std::find(m_ringOrder.begin(), m_ringOrder.end(), from);
    if (startIt == m_ringOrder.end()) return results;

    size_t startIdx = static_cast<size_t>(std::distance(m_ringOrder.begin(), startIt));
    size_t count = m_ringOrder.size();

    BeaconMessage msg;
    msg.messageId  = m_nextMsgId.fetch_add(1);
    msg.sourceKind = from;
    msg.targetKind = BeaconKind::Unknown;
    msg.direction  = dir;
    msg.verb       = verb;
    msg.payload    = payload;
    msg.payloadLen = payloadLen;
    msg.userData   = nullptr;
    msg.flags      = BeaconMessage::FLAG_FIRE_AND_FORGET;

    auto dispatchAt = [&](size_t idx) {
        BeaconKind kind = m_ringOrder[idx];
        if (kind == from) return;
        auto it = m_beaconMap.find(static_cast<uint32_t>(kind));
        if (it == m_beaconMap.end() || !it->second.active || !it->second.handler) return;
        it->second.msgRecvCount++;
        try {
            BeaconResponse resp = it->second.handler(msg);
            resp.responderKind = kind;
            results.push_back(std::move(resp));
        } catch (...) {}
    };

    if (dir == BeaconDirection::Forward || dir == BeaconDirection::Bilateral) {
        for (size_t i = 1; i < count; i++) dispatchAt((startIdx + i) % count);
    }
    if (dir == BeaconDirection::Reverse || dir == BeaconDirection::Bilateral) {
        for (size_t i = 1; i < count; i++) dispatchAt((startIdx + count - i) % count);
    }

    return results;
}

// ── Enumeration ──

std::vector<BeaconKind> BeaconHub::getActiveBeacons() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BeaconKind> result;
    result.reserve(m_beaconMap.size());
    for (const auto& [key, entry] : m_beaconMap) {
        if (entry.active) result.push_back(entry.kind);
    }
    return result;
}

std::vector<BeaconKind> BeaconHub::getBeaconsByPrefix(uint32_t kindPrefix) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BeaconKind> result;
    uint32_t shiftedPrefix = kindPrefix << 8;
    uint32_t mask = 0xFF00;
    if (kindPrefix > 0xFF) { shiftedPrefix = kindPrefix; mask = 0xF000; }
    for (const auto& [key, entry] : m_beaconMap) {
        if ((key & mask) == (shiftedPrefix & mask) && entry.active)
            result.push_back(entry.kind);
    }
    return result;
}

size_t BeaconHub::getBeaconCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_beaconMap.size();
}

BeaconHub::HubStats BeaconHub::getStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    HubStats stats{};
    stats.totalBeacons = m_beaconMap.size();
    stats.totalBroadcasts = m_totalBroadcasts;
    for (const auto& [key, entry] : m_beaconMap) {
        if (entry.active) stats.activeBeacons++;
        stats.totalMessagesSent     += entry.msgSentCount;
        stats.totalMessagesReceived += entry.msgRecvCount;
    }
    return stats;
}

void BeaconHub::walkRing(BeaconKind start, std::function<bool(const BeaconEntry&)> visitor) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_ringOrder.empty() || !visitor) return;

    auto startIt = std::find(m_ringOrder.begin(), m_ringOrder.end(), start);
    size_t startIdx = (startIt != m_ringOrder.end())
                      ? static_cast<size_t>(std::distance(m_ringOrder.begin(), startIt)) : 0;
    size_t count = m_ringOrder.size();

    for (size_t i = 0; i < count; i++) {
        size_t idx = (startIdx + i) % count;
        auto it = m_beaconMap.find(static_cast<uint32_t>(m_ringOrder[idx]));
        if (it != m_beaconMap.end()) {
            if (!visitor(it->second)) break;
        }
    }
}

void BeaconHub::registerVerb(const char* verb, BeaconKind handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (verb) m_verbRoutes[verb] = handler;
}

BeaconKind BeaconHub::resolveVerb(const char* verb) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!verb) return BeaconKind::Unknown;
    auto it = m_verbRoutes.find(verb);
    return (it != m_verbRoutes.end()) ? it->second : BeaconKind::Unknown;
}

// ============================================================================
// PanelBeaconBridge — Implementation
// ============================================================================

PanelBeaconBridge::~PanelBeaconBridge() { shutdown(); }

bool PanelBeaconBridge::init(BeaconKind kind, const char* name, void* panelInstance,
                              BeaconHandler handler)
{
    if (m_initialized) return false;
    m_kind = kind;

    BeaconHandler actualHandler = handler ? std::move(handler) :
        [](const BeaconMessage&) -> BeaconResponse {
            return { false, 0, "No handler", BeaconKind::Unknown };
        };

    if (BeaconHub::instance().registerBeacon(kind, name, panelInstance, std::move(actualHandler))) {
        m_initialized = true;
        return true;
    }
    return false;
}

void PanelBeaconBridge::shutdown()
{
    if (!m_initialized) return;
    BeaconHub::instance().unregisterBeacon(m_kind);
    m_initialized = false;
    m_kind = BeaconKind::Unknown;
}

BeaconResponse PanelBeaconBridge::sendTo(BeaconKind target, const char* verb,
                                          const char* payload, size_t len)
{
    if (!m_initialized) return { false, -1, "Bridge not initialized", BeaconKind::Unknown };
    return BeaconHub::instance().send(m_kind, target, verb, payload, len);
}

std::vector<BeaconResponse> PanelBeaconBridge::broadcast(const char* verb,
                                                           const char* payload, size_t len)
{
    if (!m_initialized) return {};
    return BeaconHub::instance().broadcast(m_kind, verb, payload, len);
}

std::vector<BeaconResponse> PanelBeaconBridge::sendDirectional(const char* verb, BeaconDirection dir,
                                                                 const char* payload, size_t len)
{
    if (!m_initialized) return {};
    return BeaconHub::instance().sendDirectional(m_kind, verb, dir, payload, len);
}

// ============================================================================
// beaconKindToString — Debug/logging helper
// ============================================================================

const char* beaconKindToString(BeaconKind kind)
{
    switch (kind) {
        case BeaconKind::AgenticEngine:       return "AgenticEngine";
        case BeaconKind::HotpatchManager:     return "HotpatchManager";
        case BeaconKind::PlanExecutor:        return "PlanExecutor";
        case BeaconKind::AutonomyManager:     return "AutonomyManager";
        case BeaconKind::LLMRouter:           return "LLMRouter";
        case BeaconKind::EncryptionEngine:    return "EncryptionEngine";
        case BeaconKind::IDECore:             return "IDECore";
        case BeaconKind::InferenceEngine:     return "InferenceEngine";
        case BeaconKind::FeatureRegistry:     return "FeatureRegistry";
        case BeaconKind::CommandDispatch:     return "CommandDispatch";
        case BeaconKind::ToolRegistry:        return "ToolRegistry";
        case BeaconKind::SubAgentManager:     return "SubAgentManager";
        case BeaconKind::SwarmCoordinator:    return "SwarmCoordinator";
        case BeaconKind::KnowledgeGraph:      return "KnowledgeGraph";
        case BeaconKind::DebugEngine:         return "DebugEngine";
        case BeaconKind::LSPServer:           return "LSPServer";
        case BeaconKind::VulkanCompute:       return "VulkanCompute";
        case BeaconKind::FlightRecorder:      return "FlightRecorder";
        case BeaconKind::CrashContainment:    return "CrashContainment";
        case BeaconKind::TelemetryCore:       return "TelemetryCore";
        case BeaconKind::MCPServer:           return "MCPServer";
        case BeaconKind::SessionManager:      return "SessionManager";
        case BeaconKind::BackendSwitcher:     return "BackendSwitcher";
        case BeaconKind::ProxyHotpatcher:     return "ProxyHotpatcher";
        case BeaconKind::SelfRepairAgent:     return "SelfRepairAgent";
        case BeaconKind::VoiceEngine:         return "VoiceEngine";
        case BeaconKind::StaticAnalysis:      return "StaticAnalysis";
        case BeaconKind::SemanticIntel:       return "SemanticIntel";
        case BeaconKind::GitIntegration:      return "GitIntegration";
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
        case BeaconKind::BeaconJava:          return "BeaconJava";
        case BeaconKind::BeaconPython:        return "BeaconPython";
        case BeaconKind::BeaconMASM:          return "BeaconMASM";
        case BeaconKind::BeaconRust:          return "BeaconRust";
        case BeaconKind::BeaconTypeScript:    return "BeaconTypeScript";
        case BeaconKind::BeaconCSharp:        return "BeaconCSharp";
        case BeaconKind::BeaconGo:            return "BeaconGo";
        case BeaconKind::BeaconEncryption:    return "BeaconEncryption";
        case BeaconKind::BeaconCompression:   return "BeaconCompression";
        case BeaconKind::BeaconReverseEng:    return "BeaconReverseEng";
        case BeaconKind::BeaconSecurity:      return "BeaconSecurity";
        case BeaconKind::BeaconCustom:        return "BeaconCustom";
        case BeaconKind::Unknown:             return "Unknown";
        default: break;
    }
    uint32_t k = static_cast<uint32_t>(kind);
    if (k >= static_cast<uint32_t>(BeaconKind::DynamicBase) &&
        k <= static_cast<uint32_t>(BeaconKind::DynamicEnd))
        return "DynamicBeacon";
    return "Unknown";
}

// ============================================================================
// CircularBeaconManager — Implementation
// ============================================================================

void CircularBeaconManager::initializeFullCircularSystem(HWND parentHwnd, Win32IDE* ide)
{
    if (m_initialized) return;
    m_parentHwnd = parentHwnd;
    m_ide = ide;

    OutputDebugStringA("[CircularBeaconManager] Initializing full circular beacon system...\n");

    if (ide) {
        registerPanelBridge(BeaconKind::IDECore, "Win32IDE-Core", static_cast<void*>(ide),
            [](const BeaconMessage& msg) -> BeaconResponse {
                if (msg.verb) {
                    std::string verb(msg.verb);
                    if (verb == BEACON_CMD_REFRESH)
                        return { true, 0, "IDE refreshed", BeaconKind::IDECore };
                    if (verb == BEACON_CMD_HOTRELOAD)
                        return { true, 0, "Hot-reload initiated", BeaconKind::IDECore };
                    if (verb == BEACON_CMD_UPDATE)
                        return { true, 0, "Update acknowledged", BeaconKind::IDECore };
                }
                return { false, 0, "Unhandled verb", BeaconKind::IDECore };
            }
        );
    }

    // Register standard verb routes
    auto& hub = BeaconHub::instance();
    hub.registerVerb(BEACON_CMD_AGENTIC_REQUEST, BeaconKind::AgenticEngine);
    hub.registerVerb(BEACON_CMD_HOTRELOAD,       BeaconKind::HotpatchManager);
    hub.registerVerb(BEACON_CMD_EXECUTE_PLAN,    BeaconKind::PlanExecutor);
    hub.registerVerb(BEACON_CMD_TUNE_ENGINE,     BeaconKind::InferenceEngine);
    hub.registerVerb(BEACON_CMD_SWITCH_KERNEL,   BeaconKind::BackendSwitcher);

    m_initialized = true;
    OutputDebugStringA("[CircularBeaconManager] Circular beacon system initialized\n");
}

void CircularBeaconManager::shutdown()
{
    if (!m_initialized) return;
    OutputDebugStringA("[CircularBeaconManager] Shutting down...\n");
    m_bridges.clear();
    m_ide = nullptr;
    m_parentHwnd = nullptr;
    m_initialized = false;
}

void CircularBeaconManager::performSystemWideHotReload()
{
    if (!m_initialized) return;
    OutputDebugStringA("[CircularBeaconManager] System-wide hot-reload broadcast\n");
    BeaconHub::instance().broadcast(BeaconKind::IDECore, BEACON_CMD_HOTRELOAD);
}

void CircularBeaconManager::executeAgenticWorkflow(const std::string& workflow)
{
    if (!m_initialized) return;
    BeaconHub::instance().send(BeaconKind::IDECore, BeaconKind::AgenticEngine,
                                BEACON_CMD_AGENTIC_REQUEST, workflow.c_str(), workflow.size());
}

std::string CircularBeaconManager::getCircularSystemStatus()
{
    auto stats = BeaconHub::instance().getStats();
    std::ostringstream ss;
    ss << "=== Circular Beacon System Status ===\n"
       << "Total Beacons: "     << stats.totalBeacons << "\n"
       << "Active Beacons: "    << stats.activeBeacons << "\n"
       << "Messages Sent: "     << stats.totalMessagesSent << "\n"
       << "Messages Received: " << stats.totalMessagesReceived << "\n"
       << "Total Broadcasts: "  << stats.totalBroadcasts << "\n"
       << "=====================================\n";
    auto active = BeaconHub::instance().getActiveBeacons();
    for (auto kind : active) {
        ss << "  [" << beaconKindToString(kind) << "] "
           << BeaconHub::instance().getBeaconName(kind) << "\n";
    }
    return ss.str();
}

void CircularBeaconManager::performEmergencyCircularReset(HWND hwnd)
{
    OutputDebugStringA("[CircularBeaconManager] EMERGENCY RESET\n");
    Win32IDE* savedIde = m_ide;
    m_bridges.clear();
    m_initialized = false;
    if (savedIde && hwnd) initializeFullCircularSystem(hwnd, savedIde);
}

PanelBeaconBridge* CircularBeaconManager::getBridge(BeaconKind kind)
{
    auto it = m_bridges.find(static_cast<uint32_t>(kind));
    return (it != m_bridges.end()) ? it->second.get() : nullptr;
}

void CircularBeaconManager::registerPanelBridge(BeaconKind kind, const char* name,
                                                  void* instance, BeaconHandler handler)
{
    uint32_t key = static_cast<uint32_t>(kind);
    if (m_bridges.find(key) != m_bridges.end()) return;
    auto bridge = std::make_unique<PanelBeaconBridge>();
    bridge->init(kind, name, instance, std::move(handler));
    m_bridges[key] = std::move(bridge);
}

// ============================================================================
// Extern "C" bridges — for MASM hot-path / cross-module dispatch
// ============================================================================

extern "C" {

int rawrxd_beacon_register(uint32_t kind, const char* name, void* instance)
{
    return BeaconHub::instance().registerBeacon(
        static_cast<BeaconKind>(kind), name, instance, nullptr) ? 1 : 0;
}

int rawrxd_beacon_unregister(uint32_t kind)
{
    return BeaconHub::instance().unregisterBeacon(static_cast<BeaconKind>(kind)) ? 1 : 0;
}

void* rawrxd_beacon_get(uint32_t kind)
{
    return BeaconHub::instance().getBeaconRaw(static_cast<BeaconKind>(kind));
}

int rawrxd_beacon_send(uint32_t from, uint32_t to, const char* verb,
                        const char* payload, size_t payloadLen)
{
    auto resp = BeaconHub::instance().send(
        static_cast<BeaconKind>(from), static_cast<BeaconKind>(to),
        verb, payload, payloadLen);
    return resp.handled ? resp.statusCode : -1;
}

int rawrxd_beacon_broadcast(uint32_t from, const char* verb,
                              const char* payload, size_t payloadLen)
{
    auto results = BeaconHub::instance().broadcast(
        static_cast<BeaconKind>(from), verb, payload, payloadLen);
    return static_cast<int>(results.size());
}

int rawrxd_beacon_count()
{
    return static_cast<int>(BeaconHub::instance().getBeaconCount());
}

} // extern "C"

} // namespace RawrXD


