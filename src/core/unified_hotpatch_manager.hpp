// unified_hotpatch_manager.hpp — Unified Hotpatch Coordination Layer
// Routes patches to the proper layer (Memory, Byte, Server).
// Tracks statistics. Preset save/load via JSON (manual serializer).
//
// No signals. Instead: Function pointer callbacks, event ring buffer,
// poll-based notification.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#include "model_memory_hotpatch.hpp"
#include "byte_level_hotpatcher.hpp"
#include "../server/gguf_server_hotpatch.hpp"
#include "pt_driver_contract.hpp"
#include "live_binary_patcher.hpp"
#include "shadow_page_detour.hpp"
#include "sentinel_watchdog.hpp"
#include "vscode_copilot_hotpatch.hpp"

// Forward declarations — platform subsystems
class AutonomousWorkflowEngine;
class WorkspaceReasoningProfileManager;
class DeterministicSwarmEngine;
class SafeRefactorEngine;
class ReasoningSchemaRegistry;
class CoTFallbackSystem;
class InputGuardSlicer;

// Forward declarations — gap-closing subsystems (v2.0)
namespace RawrXD { namespace Agentic { class AgenticTaskGraph; } }
namespace RawrXD { namespace Embeddings { class EmbeddingEngine; } }
namespace RawrXD { namespace Vision { class VisionEncoder; } }
namespace RawrXD { namespace Extensions { class ExtensionMarketplace; } }
namespace RawrXD { namespace Auth { class RBACEngine; } }

// Forward declaration — VS Code Copilot Integration (Layer 4)
class VSCodeCopilotHotpatcher;
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>

// Forward declarations removed — now included via gguf_server_hotpatch.hpp
// (ServerHotpatch, Request, Response are fully defined there)

// ---------------------------------------------------------------------------
// UnifiedResult — Extends PatchResult with layer info
// ---------------------------------------------------------------------------
struct UnifiedResult {
    PatchResult result;
    const char* layerName;      // "memory", "byte", "server"
    uint64_t    sequenceId;     // Monotonic sequence number

    static UnifiedResult from(PatchResult r, const char* layer, uint64_t seq) {
        UnifiedResult ur;
        ur.result    = r;
        ur.layerName = layer;
        ur.sequenceId = seq;
        return ur;
    }
};

// ---------------------------------------------------------------------------
// HotpatchEvent — Ring buffer entry for event notification
// ---------------------------------------------------------------------------
struct HotpatchEvent {
    enum Type : uint8_t {
        MemoryPatchApplied  = 0,
        MemoryPatchReverted = 1,
        BytePatchApplied    = 2,
        BytePatchFailed     = 3,
        ServerPatchAdded    = 4,
        ServerPatchRemoved  = 5,
        PresetLoaded        = 6,
        PresetSaved         = 7,
        // PT driver events
        PTWatchpointArmed   = 8,
        PTWatchpointHit     = 9,
        PTSnapshotTaken     = 10,
        PTSnapshotRestored  = 11,
        PTProtectionChanged = 12,
        PTArenaAllocated    = 13,
        // Live Binary (Layer 5) events
        LiveBinaryRegistered     = 14,
        LiveBinaryTrampolineSet  = 15,
        LiveBinarySwapped        = 16,
        LiveBinaryReverted       = 17,
        LiveBinaryBatchApplied   = 18,
        LiveBinaryModuleLoaded   = 19,
        // Shadow-Page Detour + Sentinel (Layer 6) events
        ShadowDetourRegistered   = 20,
        ShadowDetourApplied      = 21,
        ShadowDetourReverted     = 22,
        ShadowDetourRollbackAll  = 23,
        ShadowDetourVerified     = 24,
        SentinelActivated        = 25,
        SentinelDeactivated      = 26,
        SentinelViolation        = 27,
        SentinelLockdown         = 28,
        // VS Code Copilot (Layer 4) events
        CopilotInterceptorApplied = 29,
        CopilotOperationEnhanced  = 30,
        CopilotCheckpointCreated  = 31,
        CopilotCheckpointRestored = 32,
        CopilotFeatureProcessed   = 33,
    };

    Type        type;
    uint64_t    timestamp;      // GetTickCount64() or RDTSC
    uint64_t    sequenceId;
    char        detail[256];
    const char* layerName;      // "memory", "byte", or "server"
    const char* patchName;      // Human-readable patch identifier
    bool        success;        // Whether the operation succeeded
};

// ---------------------------------------------------------------------------
// Callback types (no std::function — raw function pointers)
// ---------------------------------------------------------------------------
typedef void (*HotpatchEventCallback)(const HotpatchEvent* event, void* userData);

// ---------------------------------------------------------------------------
// HotpatchPreset — Serializable collection of patches
// ---------------------------------------------------------------------------
struct HotpatchPreset {
    char                        name[128];
    int                         version = 0;
    std::vector<MemoryPatchEntry>  memoryPatches;
    std::vector<BytePatchEnhanced> bytePatches;
    // Server patches are function pointers, not serializable — stored by name
    std::vector<std::string>       serverPatchNames;
    // Memory patches stored by name for preset load/save
    std::vector<std::string>       memoryPatchNames;
};

// ---------------------------------------------------------------------------
// UnifiedHotpatchManager
// ---------------------------------------------------------------------------
class UnifiedHotpatchManager {
public:
    static UnifiedHotpatchManager& instance();

    // ---- Memory Layer (Layer 1) ----
    UnifiedResult apply_memory_patch(void* addr, size_t size, const void* data);
    UnifiedResult apply_memory_patch_tracked(MemoryPatchEntry* entry);
    UnifiedResult revert_memory_patch(MemoryPatchEntry* entry);

    // ---- Byte Layer (Layer 2) ----
    UnifiedResult apply_byte_patch(const char* filename, const BytePatchEnhanced& patch);
    UnifiedResult apply_byte_search_patch(const char* filename,
                                          const std::vector<uint8_t>& pattern,
                                          const std::vector<uint8_t>& replacement);

    // ---- Server Layer (Layer 3) ----
    UnifiedResult add_server_patch(ServerHotpatch* patch);
    UnifiedResult remove_server_patch(const char* name);

    /// Clear all patches across all layers (memory, byte, server)
    void clearAllPatches();

    // ---- PT Driver (Layer 0 — Page Table substrate) ----
    UnifiedResult pt_arm_watchpoint(uintptr_t addr, uint64_t size,
                                    WatchpointEntry::WatchpointCallback cb,
                                    void* ctx, bool oneShot, uint32_t* outId);
    UnifiedResult pt_disarm_watchpoint(uint32_t id);
    UnifiedResult pt_take_snapshot(uintptr_t addr, uint64_t size,
                                   const char* label, uint32_t layerIndex,
                                   uint32_t* outSnapshotId);
    UnifiedResult pt_restore_snapshot(uint32_t snapshotId);
    UnifiedResult pt_set_protection(uintptr_t addr, uint64_t size,
                                    uint32_t newProtect, uint32_t* outOldProtect);
    UnifiedResult pt_alloc_large_arena(uint64_t sizeBytes, uint64_t pageSize,
                                       uint32_t* outArenaId);
    UnifiedResult pt_normalize(const ASLRContext* ctx,
                               uintptr_t absAddr, uintptr_t* outRelative);

    // PT lifecycle (delegates to PTDriverContract singleton)
    PatchResult pt_initialize();
    PatchResult pt_shutdown();
    const PTDriverStats& pt_get_stats() const;

    // ---- Live Binary (Layer 5 — Real-Time Code Replacement) ----
    UnifiedResult live_register_function(const char* name, uintptr_t addr, uint32_t* outSlotId);
    UnifiedResult live_install_trampoline(uint32_t slotId);
    UnifiedResult live_revert_trampoline(uint32_t slotId);
    UnifiedResult live_swap_implementation(uint32_t slotId,
                                           const uint8_t* newCode, size_t codeSize,
                                           const RVARelocation* relocs, size_t relocCount);
    UnifiedResult live_apply_batch(const LivePatchUnit* units, size_t count);
    UnifiedResult live_revert_last(uint32_t slotId);
    UnifiedResult live_load_module(const char* dll_path, uint32_t* outModuleId);
    UnifiedResult live_unload_module(uint32_t moduleId);
    PatchResult   live_initialize(size_t pool_pages = 4);
    PatchResult   live_shutdown();
    PatchResult   live_verify_integrity();
    const LiveBinaryPatcherStats& live_get_stats() const;

    // ---- Shadow-Page Detour (Layer 6 — Atomic Prologue Rewrite) ----
    UnifiedResult shadow_register_detour(const char* name, void* funcAddr);
    UnifiedResult shadow_apply_patch(const char* name,
                                      const uint8_t* newCode, size_t codeSize);
    UnifiedResult shadow_verify_and_patch(void* originalFn,
                                           const std::string& newAsmSource);
    UnifiedResult shadow_rollback(const char* name);
    UnifiedResult shadow_rollback_all();
    UnifiedResult shadow_verify_all();
    size_t        shadow_get_active_count() const;
    HotpatchKernelStats shadow_get_kernel_stats() const;
    SnapshotStats       shadow_get_snapshot_stats() const;
    PatchResult   shadow_initialize();
    PatchResult   shadow_shutdown();

    // ---- VS Code Copilot Integration (Layer 4 — AI-Enhanced Development) ----
    UnifiedResult copilot_process_operation(const char* message, size_t messageLen,
                                           CopilotOperationEnhanced* operation = nullptr);
    UnifiedResult copilot_add_status_interceptor(const CopilotStatusInterceptor& interceptor);
    UnifiedResult copilot_remove_status_interceptor(const char* name);
    UnifiedResult copilot_add_enhancement_rule(const CopilotEnhancementRule& rule);
    UnifiedResult copilot_remove_enhancement_rule(const char* name);
    
    // Feature-specific operations
    UnifiedResult copilot_compact_conversation(const std::string& conversation,
                                             std::string* result = nullptr);
    UnifiedResult copilot_optimize_tool_selection(const std::vector<std::string>& tools,
                                                 std::vector<std::string>* result = nullptr);
    UnifiedResult copilot_enhance_resolution(const std::string& context,
                                            std::string* result = nullptr);
    UnifiedResult copilot_process_read_lines(const std::string& content, size_t start, size_t end,
                                            std::string* result = nullptr);
    UnifiedResult copilot_plan_exploration(const std::string& codebase,
                                          std::vector<std::string>* result = nullptr);
    UnifiedResult copilot_enhance_file_search(const std::string& query,
                                             const std::vector<std::string>& results,
                                             std::string* result = nullptr);
    UnifiedResult copilot_evaluate_audit(const std::string& context,
                                        std::string* result = nullptr);
    
    // Checkpoint management
    UnifiedResult copilot_create_checkpoint(const std::string& id,
                                           const std::string& conversation,
                                           const std::string& workspace);
    UnifiedResult copilot_restore_checkpoint(const std::string& id,
                                            std::string* conversation = nullptr,
                                            std::string* workspace = nullptr);
    UnifiedResult copilot_delete_checkpoint(const std::string& id);
    std::vector<std::string> copilot_list_checkpoints() const;
    
    // Configuration
    void          copilot_set_auto_optimize(bool enable);
    void          copilot_set_conversation_compaction(bool enable);
    void          copilot_set_checkpoint_restore(bool enable);
    void          copilot_set_operation_caching(bool enable);
    
    // Statistics
    CopilotHotpatchStats copilot_get_stats() const;
    void                 copilot_reset_stats();
    
    PatchResult   copilot_initialize();
    PatchResult   copilot_shutdown();

    // ---- Sentinel Watchdog (Layer 6 — .text Integrity Monitor) ----
    PatchResult   sentinel_activate();
    PatchResult   sentinel_deactivate();
    PatchResult   sentinel_update_baseline();
    SentinelStats sentinel_get_stats() const;
    bool          sentinel_is_active() const;

    // ---- Platform Subsystem Integration (Valuation-Critical) ----

    // Autonomous Workflow Engine — scan → fix → verify → build → test → diff
    PatchResult   workflow_initialize();
    PatchResult   workflow_shutdown();
    bool          workflow_is_running() const;

    // Workspace Reasoning Profiles — per-repo fast/dev/critical modes
    PatchResult   profiles_initialize(const char* persistPath);
    PatchResult   profiles_load_for_workspace(const char* workspacePath);

    // Deterministic Swarm — reproducible execution
    PatchResult   swarm_set_seed(uint64_t masterSeed);
    PatchResult   swarm_verify_determinism();

    // Safe Refactor Engine — diff previews + rollback
    PatchResult   refactor_initialize();
    PatchResult   refactor_shutdown();

    // Reasoning Schema Versioning — migration + compatibility
    PatchResult   schema_initialize();
    PatchResult   schema_verify_compatibility(const char* versionStr);

    // CoT Fallback — circuit breaker + graceful degradation
    PatchResult   cot_initialize();
    PatchResult   cot_disable();
    PatchResult   cot_enable();
    bool          cot_is_healthy() const;

    // Input Guard — OOM protection + backend slicing
    PatchResult   guard_initialize();
    PatchResult   guard_preflight(const char* input, size_t inputLen);

    // Unified stats JSON across all subsystems
    std::string   getFullStatsJSON() const;

    // ---- Gap-Closing Subsystems (v2.0) ----

    // Agentic Task Graph — DAG-based task orchestration
    PatchResult   taskgraph_initialize();
    PatchResult   taskgraph_shutdown();
    bool          taskgraph_is_running() const;

    // Embedding Engine — vector index + semantic search
    PatchResult   embedding_initialize(const char* modelPath, uint32_t dimensions);
    PatchResult   embedding_shutdown();
    PatchResult   embedding_index_directory(const char* dirPath);

    // Vision Encoder — multi-modal image input
    PatchResult   vision_initialize(const char* modelPath);
    PatchResult   vision_shutdown();

    // Extension Marketplace — non-Qt extension management
    PatchResult   marketplace_initialize(const char* installDir, const char* cacheDir);
    PatchResult   marketplace_shutdown();

    // RBAC Engine — enterprise auth + audit
    PatchResult   rbac_initialize();
    PatchResult   rbac_shutdown();
    PatchResult   rbac_authorize(const char* sessionToken,
                                  uint32_t requiredPermission,
                                  const char* action, const char* resource);

    // ---- Preset Management (JSON, manual serializer) ----
    PatchResult save_preset(const char* filename, const HotpatchPreset& preset);
    PatchResult load_preset(const char* filename, HotpatchPreset* outPreset);

    // ---- Event System ----
    void register_callback(HotpatchEventCallback cb, void* userData);
    void unregister_callback(HotpatchEventCallback cb);

    // Poll the most recent event (returns false if no new events).
    bool poll_event(HotpatchEvent* outEvent);

    // ---- Force TPS (tokens per second) hotpatching ----
    /** Set target TPS for token delivery. 0 or negative = disabled (run normally). When set, throttle_token_delivery() will sleep to achieve this rate. */
    void set_target_tps(double tps);
    /** Get current target TPS. 0 = disabled. */
    double get_target_tps() const { return m_targetTPS.load(std::memory_order_relaxed); }
    /** Call after each token is delivered. If target TPS is set, sleeps so that effective rate does not exceed target. Leave target unset (0) to run normally. */
    void throttle_token_delivery(uint64_t tokenIndex1Based);

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> memoryPatchCount{0};
        std::atomic<uint64_t> bytePatchCount{0};
        std::atomic<uint64_t> serverPatchCount{0};
        std::atomic<uint64_t> ptOperationCount{0};
        std::atomic<uint64_t> liveBinaryCount{0};
        std::atomic<uint64_t> shadowDetourCount{0};
        std::atomic<uint64_t> sentinelEventCount{0};
        std::atomic<uint64_t> copilotOperationCount{0};  // VS Code Copilot operations
        std::atomic<uint64_t> totalOperations{0};
        std::atomic<uint64_t> totalFailures{0};
    };

    const Stats& getStats() const;
    void resetStats();

private:
    UnifiedHotpatchManager();
    ~UnifiedHotpatchManager();
    UnifiedHotpatchManager(const UnifiedHotpatchManager&) = delete;
    UnifiedHotpatchManager& operator=(const UnifiedHotpatchManager&) = delete;

    void emit_event(HotpatchEvent::Type type, const char* detail);
    void emit_event(HotpatchEvent::Type type, const std::string& detail);
    uint64_t next_sequence();

    std::mutex                              m_mutex;
    std::atomic<uint64_t>                   m_sequence{0};
    Stats                                   m_stats;

    // Event ring buffer (fixed 256 entries, power-of-2 for mask)
    static constexpr size_t EVENT_RING_SIZE = 256;
    static constexpr size_t EVENT_RING_MASK = EVENT_RING_SIZE - 1;
    HotpatchEvent                           m_eventRing[EVENT_RING_SIZE];
    std::atomic<uint64_t>                   m_eventHead{0};
    std::atomic<uint64_t>                   m_eventTail{0};

    // Callback table (max 16 callbacks)
    static constexpr size_t MAX_CALLBACKS = 16;
    struct CallbackEntry {
        HotpatchEventCallback callback;
        void*                 userData;
    };
    CallbackEntry                           m_callbacks[MAX_CALLBACKS];
    size_t                                  m_callbackCount = 0;

    // Registered server patches
    std::vector<ServerHotpatch*>            m_serverPatches;

    // Platform subsystem initialization flags
    std::atomic<bool>                       m_workflowInit{false};
    std::atomic<bool>                       m_profilesInit{false};
    std::atomic<bool>                       m_refactorInit{false};
    std::atomic<bool>                       m_schemaInit{false};
    std::atomic<bool>                       m_cotInit{false};
    std::atomic<bool>                       m_guardInit{false};

    // Gap-closing subsystem initialization flags (v2.0)
    std::atomic<bool>                       m_taskgraphInit{false};
    std::atomic<bool>                       m_embeddingInit{false};
    std::atomic<bool>                       m_visionInit{false};
    std::atomic<bool>                       m_marketplaceInit{false};
    std::atomic<bool>                       m_rbacInit{false};

    // Layer 6: Shadow-Page Detour + Sentinel Watchdog
    std::atomic<bool>                       m_shadowInit{false};
    std::atomic<bool>                       m_sentinelInit{false};

    // Layer 4: VS Code Copilot Integration
    std::atomic<bool>                       m_copilotInit{false};

    // Force TPS: 0 = disabled; >0 = target tokens per second (throttle token delivery)
    std::atomic<double>                     m_targetTPS{0.0};
    std::mutex                              m_tpsMutex;
    double                                  m_tpsStreamStartMs{0.0};  // Set when token 1 is delivered
};
