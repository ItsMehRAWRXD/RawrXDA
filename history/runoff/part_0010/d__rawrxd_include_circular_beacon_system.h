// ============================================================================
// circular_beacon_system.h — Circular Beacon Interconnect System
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions, no external deps
//
// PURPOSE:
//   Every IDE panel and every core subsystem registers itself as a "Beacon".
//   Any beacon can reach any other beacon — bidirectional, non-directional,
//   and middle-directional dispatch. All panels get full access to:
//     - Agentic Engine (BoundedAgentLoop, AgentOrchestrator)
//     - Hotpatch Manager (UnifiedHotpatchManager, ProxyHotpatcher)
//     - Executor (PlanExecutor, AgentTransaction)
//     - Autonomy (AutonomyManager, BackgroundDaemon)
//     - LLM Router (BackendSwitcher, TaskClassifier)
//     - Encryption (Camellia256, AES-256-CBC)
//     - IDE Core (Win32IDE instance pointer)
//     - Any future subsystem (Java, Python, MASM, etc.)
//
// PATTERN:
//   - Singleton BeaconHub with thread-safe registration
//   - Each beacon has a BeaconKind enum and a void* to its instance
//   - BeaconMessage struct for inter-beacon dispatch (fire-and-forget or request/response)
//   - Circular ring: every beacon can enumerate all others
//   - PanelBeaconBridge: convenience wrapper for Win32IDE panels
//
// RULE: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_CIRCULAR_BEACON_SYSTEM_H
#define RAWRXD_CIRCULAR_BEACON_SYSTEM_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {

// ============================================================================
// BeaconKind — Every known subsystem / panel type
// ============================================================================
enum class BeaconKind : uint32_t {
    // ── Core Subsystems ──
    AgenticEngine       = 0x0001,
    HotpatchManager     = 0x0002,
    PlanExecutor        = 0x0003,
    AutonomyManager     = 0x0004,
    LLMRouter           = 0x0005,
    EncryptionEngine    = 0x0006,
    IDECore             = 0x0007,
    InferenceEngine     = 0x0008,
    FeatureRegistry     = 0x0009,
    CommandDispatch     = 0x000A,
    ToolRegistry        = 0x000B,
    SubAgentManager     = 0x000C,
    SwarmCoordinator    = 0x000D,
    KnowledgeGraph      = 0x000E,
    DebugEngine         = 0x000F,
    LSPServer           = 0x0010,
    VulkanCompute       = 0x0011,
    FlightRecorder      = 0x0012,
    CrashContainment    = 0x0013,
    TelemetryCore       = 0x0014,
    MCPServer           = 0x0015,
    SessionManager      = 0x0016,
    BackendSwitcher     = 0x0017,
    ProxyHotpatcher     = 0x0018,
    SelfRepairAgent     = 0x0019,
    VoiceEngine         = 0x001A,
    StaticAnalysis      = 0x001B,
    SemanticIntel       = 0x001C,
    GitIntegration      = 0x001D,

    // ── GUI Panels ──
    PanelChat           = 0x1001,
    PanelAgent          = 0x1002,
    PanelHotpatch       = 0x1003,
    PanelHotpatchCtrl   = 0x1004,
    PanelSwarm          = 0x1005,
    PanelDualAgent      = 0x1006,
    PanelCrucible       = 0x1007,
    PanelTranscendence  = 0x1008,
    PanelPipeline       = 0x1009,
    PanelSemantic       = 0x100A,
    PanelStaticAnalysis = 0x100B,
    PanelTelemetry      = 0x100C,
    PanelVoiceChat      = 0x100D,
    PanelNetwork        = 0x100E,
    PanelPowerShell     = 0x100F,
    PanelGit            = 0x1010,
    PanelFailureIntel   = 0x1011,
    PanelAgentHistory   = 0x1012,
    PanelDebugger       = 0x1013,
    PanelDecompiler     = 0x1014,
    PanelAudit          = 0x1015,
    PanelGauntlet       = 0x1016,
    PanelMarketplace    = 0x1017,
    PanelTestExplorer   = 0x1018,
    PanelGameEngine     = 0x1019,
    PanelCopilotGap     = 0x101A,
    PanelProvableAgent  = 0x101B,
    PanelAIReverseEng   = 0x101C,
    PanelAirgapped      = 0x101D,
    PanelNativeDebug    = 0x101E,
    PanelShortcutEditor = 0x101F,
    PanelColorPicker    = 0x1020,
    PanelPlanDialog     = 0x1021,
    PanelOutline        = 0x1022,
    PanelReference      = 0x1023,
    PanelBreadcrumbs    = 0x1024,
    PanelMCP            = 0x1025,
    PanelInstructions   = 0x1026,
    PanelCursorParity   = 0x1027,
    PanelFlagship       = 0x1028,

    // ── Language / Domain Beacons ──
    BeaconJava          = 0x2001,
    BeaconPython        = 0x2002,
    BeaconMASM          = 0x2003,
    BeaconRust          = 0x2004,
    BeaconTypeScript    = 0x2005,
    BeaconCSharp        = 0x2006,
    BeaconGo            = 0x2007,
    BeaconEncryption    = 0x2008,
    BeaconCompression   = 0x2009,
    BeaconReverseEng    = 0x200A,
    BeaconSecurity      = 0x200B,
    BeaconCustom        = 0x2FFF,

    Unknown             = 0xFFFF
};

// ============================================================================
// BeaconDirection — Dispatch direction control
// ============================================================================
enum class BeaconDirection : uint8_t {
    Forward     = 0,   // Source → Target (left to right in the circle)
    Reverse     = 1,   // Target → Source (right to left)
    Bilateral   = 2,   // Both directions simultaneously
    Middle      = 3,   // Through the hub center (broadcast via hub)
    Circular    = 4    // Full ring traversal — every beacon sees the message
};

// ============================================================================
// BeaconMessage — Inter-beacon communication packet
// ============================================================================
struct BeaconMessage {
    uint32_t        messageId;            // Unique message ID (auto-incremented)
    BeaconKind      sourceKind;           // Who sent this
    BeaconKind      targetKind;           // Who should receive (Unknown = broadcast)
    BeaconDirection direction;            // Which direction to dispatch
    const char*     verb;                 // Command verb (e.g. "hotpatch.apply", "agent.execute")
    const char*     payload;              // JSON or text payload (nullable)
    size_t          payloadLen;           // Payload byte length
    void*           userData;             // Opaque caller data
    uint32_t        flags;                // Message flags
    
    // Flag constants
    static constexpr uint32_t FLAG_FIRE_AND_FORGET  = 0x0001;
    static constexpr uint32_t FLAG_EXPECT_RESPONSE  = 0x0002;
    static constexpr uint32_t FLAG_HIGH_PRIORITY    = 0x0004;
    static constexpr uint32_t FLAG_ASYNC            = 0x0008;
    static constexpr uint32_t FLAG_ENCRYPTED        = 0x0010;
};

// ============================================================================
// BeaconResponse — Response to a BeaconMessage
// ============================================================================
struct BeaconResponse {
    bool            handled;
    int             statusCode;           // 0 = success, nonzero = error
    std::string     result;               // Response payload
    BeaconKind      responderKind;        // Who handled it
};

// ============================================================================
// BeaconHandler — Callback type for receiving beacon messages
// ============================================================================
using BeaconHandler = std::function<BeaconResponse(const BeaconMessage& msg)>;

// ============================================================================
// BeaconEntry — A registered beacon in the hub
// ============================================================================
struct BeaconEntry {
    BeaconKind      kind;
    std::string     name;                 // Human-readable name
    void*           instance;             // Pointer to the subsystem/panel instance
    BeaconHandler   handler;              // Message handler callback
    bool            active;               // Is this beacon currently active?
    uint64_t        msgSentCount;         // Stats: messages sent
    uint64_t        msgRecvCount;         // Stats: messages received
    uint64_t        registeredAt;         // GetTickCount64 when registered
};

// ============================================================================
// BeaconHub — Singleton circular interconnect hub
// ============================================================================
class BeaconHub {
public:
    // Singleton access
    static BeaconHub& instance() {
        static BeaconHub s_hub;
        return s_hub;
    }

    // ── Registration ──
    bool registerBeacon(BeaconKind kind, const char* name, void* instance, BeaconHandler handler);
    bool unregisterBeacon(BeaconKind kind);
    bool isRegistered(BeaconKind kind) const;
    void setActive(BeaconKind kind, bool active);

    // ── Instance access (typed) ──
    template<typename T>
    T* getBeacon(BeaconKind kind) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_beaconMap.find(kind);
        if (it == m_beaconMap.end() || !it->second.active) return nullptr;
        return static_cast<T*>(it->second.instance);
    }

    // ── Raw instance access ──
    void* getBeaconRaw(BeaconKind kind) const;
    const char* getBeaconName(BeaconKind kind) const;

    // ── Message dispatch ──
    
    // Targeted send: source → specific target
    BeaconResponse send(BeaconKind from, BeaconKind to, const char* verb,
                        const char* payload = nullptr, size_t payloadLen = 0,
                        uint32_t flags = BeaconMessage::FLAG_FIRE_AND_FORGET);

    // Broadcast: source → all beacons (Circular direction)
    std::vector<BeaconResponse> broadcast(BeaconKind from, const char* verb,
                                          const char* payload = nullptr, size_t payloadLen = 0);

    // Directional send: respects BeaconDirection
    std::vector<BeaconResponse> sendDirectional(BeaconKind from, const char* verb,
                                                 BeaconDirection dir,
                                                 const char* payload = nullptr,
                                                 size_t payloadLen = 0);

    // ── Enumeration ──
    std::vector<BeaconKind> getActiveBeacons() const;
    std::vector<BeaconKind> getBeaconsByPrefix(uint32_t kindPrefix) const; // e.g. 0x10 for panels
    size_t getBeaconCount() const;
    
    // ── Hub Statistics ──
    struct HubStats {
        size_t   totalBeacons;
        size_t   activeBeacons;
        uint64_t totalMessagesSent;
        uint64_t totalMessagesReceived;
        uint64_t totalBroadcasts;
    };
    HubStats getStats() const;

    // ── Circular Ring Traversal ──
    // Walk the ring starting from `start`, calling visitor for each beacon
    // If visitor returns false, traversal stops
    void walkRing(BeaconKind start, std::function<bool(const BeaconEntry&)> visitor) const;

    // ── Convenience: Direct subsystem access (typed shortcuts) ──
    // These avoid the template syntax for the most common subsystems
    void* getAgenticEngine()    const { return getBeaconRaw(BeaconKind::AgenticEngine); }
    void* getHotpatchManager()  const { return getBeaconRaw(BeaconKind::HotpatchManager); }
    void* getPlanExecutor()     const { return getBeaconRaw(BeaconKind::PlanExecutor); }
    void* getAutonomyManager()  const { return getBeaconRaw(BeaconKind::AutonomyManager); }
    void* getLLMRouter()        const { return getBeaconRaw(BeaconKind::LLMRouter); }
    void* getEncryptionEngine() const { return getBeaconRaw(BeaconKind::EncryptionEngine); }
    void* getIDECore()          const { return getBeaconRaw(BeaconKind::IDECore); }
    void* getInferenceEngine()  const { return getBeaconRaw(BeaconKind::InferenceEngine); }
    void* getSwarmCoordinator() const { return getBeaconRaw(BeaconKind::SwarmCoordinator); }
    void* getDebugEngine()      const { return getBeaconRaw(BeaconKind::DebugEngine); }
    void* getLSPServer()        const { return getBeaconRaw(BeaconKind::LSPServer); }
    void* getVoiceEngine()      const { return getBeaconRaw(BeaconKind::VoiceEngine); }
    void* getBackendSwitcher()  const { return getBeaconRaw(BeaconKind::BackendSwitcher); }
    void* getSessionManager()   const { return getBeaconRaw(BeaconKind::SessionManager); }
    void* getToolRegistry()     const { return getBeaconRaw(BeaconKind::ToolRegistry); }
    void* getMCPServer()        const { return getBeaconRaw(BeaconKind::MCPServer); }

    // ── Verb routing table ──
    // Register a global verb handler that any beacon can invoke
    void registerVerb(const char* verb, BeaconKind handler);
    BeaconKind resolveVerb(const char* verb) const;

private:
    BeaconHub() = default;
    ~BeaconHub() = default;
    BeaconHub(const BeaconHub&) = delete;
    BeaconHub& operator=(const BeaconHub&) = delete;

    mutable std::mutex                                  m_mutex;
    std::unordered_map<uint32_t, BeaconEntry>            m_beaconMap;   // kind → entry
    std::vector<BeaconKind>                              m_ringOrder;   // insertion order for circular walk
    std::unordered_map<std::string, BeaconKind>          m_verbRoutes;  // verb → handler beacon
    std::atomic<uint32_t>                                m_nextMsgId{1};
    uint64_t                                             m_totalBroadcasts{0};
};

// ============================================================================
// PanelBeaconBridge — Drop-in convenience for any Win32IDE panel
// ============================================================================
// Panels include this and call bridge methods to reach any subsystem.
// The bridge holds its own BeaconKind and auto-registers/unregisters.
//
// Usage in a panel .cpp:
//   PanelBeaconBridge m_beacon;
//   m_beacon.init(BeaconKind::PanelSwarm, "SwarmPanel", this, handler);
//   auto* agentic = m_beacon.agentic();        // → AgenticEngine*
//   auto* hotpatch = m_beacon.hotpatch();       // → UnifiedHotpatchManager*
//   m_beacon.sendTo(BeaconKind::LLMRouter, "route.inference", jsonPayload);
// ============================================================================
class PanelBeaconBridge {
public:
    PanelBeaconBridge() = default;
    ~PanelBeaconBridge();

    // Initialize and register this panel as a beacon
    bool init(BeaconKind kind, const char* name, void* panelInstance,
              BeaconHandler handler = nullptr);

    // Unregister (also called by destructor)
    void shutdown();

    // ── Quick subsystem access ──
    void* agentic()         const { return BeaconHub::instance().getAgenticEngine(); }
    void* hotpatch()        const { return BeaconHub::instance().getHotpatchManager(); }
    void* executor()        const { return BeaconHub::instance().getPlanExecutor(); }
    void* autonomy()        const { return BeaconHub::instance().getAutonomyManager(); }
    void* llmRouter()       const { return BeaconHub::instance().getLLMRouter(); }
    void* encryption()      const { return BeaconHub::instance().getEncryptionEngine(); }
    void* ide()             const { return BeaconHub::instance().getIDECore(); }
    void* inference()       const { return BeaconHub::instance().getInferenceEngine(); }
    void* swarm()           const { return BeaconHub::instance().getSwarmCoordinator(); }
    void* debugEngine()     const { return BeaconHub::instance().getDebugEngine(); }
    void* lsp()             const { return BeaconHub::instance().getLSPServer(); }
    void* voice()           const { return BeaconHub::instance().getVoiceEngine(); }
    void* backend()         const { return BeaconHub::instance().getBackendSwitcher(); }
    void* session()         const { return BeaconHub::instance().getSessionManager(); }
    void* tools()           const { return BeaconHub::instance().getToolRegistry(); }
    void* mcp()             const { return BeaconHub::instance().getMCPServer(); }
    
    // ── Typed access ──
    template<typename T> T* get(BeaconKind k) const {
        return BeaconHub::instance().getBeacon<T>(k);
    }

    // ── Send to a specific beacon ──
    BeaconResponse sendTo(BeaconKind target, const char* verb,
                          const char* payload = nullptr, size_t len = 0);

    // ── Broadcast to all ──
    std::vector<BeaconResponse> broadcast(const char* verb,
                                           const char* payload = nullptr, size_t len = 0);

    // ── Directional dispatch ──
    std::vector<BeaconResponse> sendDirectional(const char* verb, BeaconDirection dir,
                                                 const char* payload = nullptr, size_t len = 0);

    // ── State ──
    bool isInitialized() const { return m_initialized; }
    BeaconKind kind() const { return m_kind; }

private:
    BeaconKind  m_kind = BeaconKind::Unknown;
    bool        m_initialized = false;
};

// ============================================================================
// beaconKindToString — Debug/logging helper
// ============================================================================
const char* beaconKindToString(BeaconKind kind);

// ============================================================================
// Extern "C" bridges for MASM hot-path / cross-module dispatch
// ============================================================================
extern "C" {
    int  rawrxd_beacon_register(uint32_t kind, const char* name, void* instance);
    int  rawrxd_beacon_unregister(uint32_t kind);
    void* rawrxd_beacon_get(uint32_t kind);
    int  rawrxd_beacon_send(uint32_t from, uint32_t to, const char* verb,
                            const char* payload, size_t payloadLen);
    int  rawrxd_beacon_broadcast(uint32_t from, const char* verb,
                                 const char* payload, size_t payloadLen);
    int  rawrxd_beacon_count();
}

} // namespace RawrXD

#endif // RAWRXD_CIRCULAR_BEACON_SYSTEM_H
