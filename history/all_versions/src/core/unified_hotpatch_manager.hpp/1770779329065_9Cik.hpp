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
#include "pt_driver_contract.hpp"
#include "live_binary_patcher.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <vector>

// Forward declaration — server layer
struct ServerHotpatch;
struct Request;
struct Response;

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
    };

    Type        type;
    uint64_t    timestamp;      // GetTickCount64() or RDTSC
    uint64_t    sequenceId;
    const char* detail;
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
    std::vector<BytePatch>         bytePatches;
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
    UnifiedResult apply_byte_patch(const char* filename, const BytePatch& patch);
    UnifiedResult apply_byte_search_patch(const char* filename,
                                          const std::vector<uint8_t>& pattern,
                                          const std::vector<uint8_t>& replacement);

    // ---- Server Layer (Layer 3) ----
    UnifiedResult add_server_patch(ServerHotpatch* patch);
    UnifiedResult remove_server_patch(const char* name);

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

    // ---- Preset Management (JSON, manual serializer) ----
    PatchResult save_preset(const char* filename, const HotpatchPreset& preset);
    PatchResult load_preset(const char* filename, HotpatchPreset* outPreset);

    // ---- Event System ----
    void register_callback(HotpatchEventCallback cb, void* userData);
    void unregister_callback(HotpatchEventCallback cb);

    // Poll the most recent event (returns false if no new events).
    bool poll_event(HotpatchEvent* outEvent);

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> memoryPatchCount{0};
        std::atomic<uint64_t> bytePatchCount{0};
        std::atomic<uint64_t> serverPatchCount{0};
        std::atomic<uint64_t> ptOperationCount{0};
        std::atomic<uint64_t> liveBinaryCount{0};
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
};
