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
    };

    Type        type;
    uint64_t    timestamp;      // GetTickCount64() or RDTSC
    uint64_t    sequenceId;
    const char* detail;
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
