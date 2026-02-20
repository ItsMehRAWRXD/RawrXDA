// =============================================================================
// pt_driver_contract.hpp — Page Table Driver Contract
// =============================================================================
// Full software page table abstraction for Windows x64.
//
// Provides:
//   1. PTE/PDE emulation via VirtualQuery page-walk
//   2. Guard-page watchpoints (PAGE_GUARD + VEH interception)
//   3. COW tensor snapshots (VirtualAlloc + lazy copy)
//   4. ASLR normalization (rebase to module-relative offsets)
//   5. Large-page arena allocation (MEM_LARGE_PAGES)
//   6. Working-set residency tracking (QueryWorkingSetEx)
//   7. Protection-domain transitions for hotpatch coordination
//
// Integration: Plugs into UnifiedHotpatchManager as the memory-management
//              substrate beneath the Memory Layer (Layer 1).
//
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// =============================================================================
#pragma once

#include "model_memory_hotpatch.hpp"   // PatchResult, MemoryPatchEntry
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>

// ---------------------------------------------------------------------------
// Page protection constants (mirror Windows but stay portable)
// ---------------------------------------------------------------------------
namespace PT {

    // Protection flags (match Windows PAGE_* values for zero-cost translation)
    static constexpr uint32_t PROT_NOACCESS         = 0x01;   // PAGE_NOACCESS
    static constexpr uint32_t PROT_READONLY          = 0x02;   // PAGE_READONLY
    static constexpr uint32_t PROT_READWRITE         = 0x04;   // PAGE_READWRITE
    static constexpr uint32_t PROT_WRITECOPY         = 0x08;   // PAGE_WRITECOPY
    static constexpr uint32_t PROT_EXECUTE           = 0x10;   // PAGE_EXECUTE
    static constexpr uint32_t PROT_EXECUTE_READ      = 0x20;   // PAGE_EXECUTE_READ
    static constexpr uint32_t PROT_EXECUTE_READWRITE = 0x40;   // PAGE_EXECUTE_READWRITE
    static constexpr uint32_t PROT_EXECUTE_WRITECOPY = 0x80;   // PAGE_EXECUTE_WRITECOPY

    // Modifier flags
    static constexpr uint32_t PROT_GUARD             = 0x100;  // PAGE_GUARD
    static constexpr uint32_t PROT_NOCACHE           = 0x200;  // PAGE_NOCACHE
    static constexpr uint32_t PROT_WRITECOMBINE      = 0x400;  // PAGE_WRITECOMBINE

    // Page sizes
    static constexpr uint64_t PAGE_SIZE_4K  = 4096;
    static constexpr uint64_t PAGE_SIZE_2M  = 2 * 1024 * 1024;
    static constexpr uint64_t PAGE_SIZE_1G  = 1024ULL * 1024 * 1024;

    // Page state (MEM_*)
    static constexpr uint32_t STATE_FREE             = 0x10000;   // MEM_FREE
    static constexpr uint32_t STATE_RESERVE           = 0x2000;    // MEM_RESERVE
    static constexpr uint32_t STATE_COMMIT            = 0x1000;    // MEM_COMMIT

    // Maximum tracked regions / watchpoints
    static constexpr int MAX_WATCHPOINTS    = 256;
    static constexpr int MAX_COW_SNAPSHOTS  = 64;
    static constexpr int MAX_LARGE_ARENAS   = 16;
    static constexpr int MAX_WALK_DEPTH     = 4096;

} // namespace PT

// ---------------------------------------------------------------------------
// PTEDescriptor — Software emulation of a Page Table Entry
// ---------------------------------------------------------------------------
struct PTEDescriptor {
    uintptr_t   virtualAddr;        // Page-aligned virtual address
    uintptr_t   physicalOffset;     // Offset relative to region base (pseudo-physical)
    uint64_t    pageSize;           // 4K, 2M, or 1G
    uint32_t    protection;         // PT::PROT_* flags
    uint32_t    state;              // PT::STATE_* (free / reserve / commit)

    // x64 PTE-equivalent bit fields
    bool        present;            // Page is committed in physical memory
    bool        writable;           // RW bit
    bool        executable;         // NX bit inverted (true = executable)
    bool        userMode;           // U/S bit (true = user-accessible)
    bool        dirty;              // Page has been written to
    bool        accessed;           // Page has been read or written
    bool        largePage;          // PS bit (2M or 1G)
    bool        guard;              // Guard page flag (triggers exception on first access)
    bool        nocache;            // Cache-disable bit
    bool        writeCombine;       // Write-combine flag

    // Residency info (from QueryWorkingSetEx)
    bool        resident;           // Currently in physical RAM (working set)
    uint32_t    shareCount;         // Number of processes sharing this page
    bool        shared;             // Page is shared (mapped section, etc.)

    // Timing
    uint64_t    lastAccessTick;     // Tick of last tracked access
    uint64_t    createTick;         // Tick when entry was first recorded
};

// ---------------------------------------------------------------------------
// WatchpointEntry — Guard-page watchpoint descriptor
// ---------------------------------------------------------------------------
struct WatchpointEntry {
    uintptr_t   address;            // Page-aligned base address to watch
    uint64_t    regionSize;         // Size of watched region (may span multiple pages)
    uint32_t    originalProtection; // Protection before guard was set
    uint32_t    id;                 // Unique watchpoint ID
    bool        active;             // Currently armed
    bool        oneShot;            // Auto-disarm after first hit
    uint64_t    hitCount;           // Total trigger count

    // Callback — raw function pointer, no std::function
    typedef void (*WatchpointCallback)(const WatchpointEntry* wp, uintptr_t faultAddr, void* ctx);
    WatchpointCallback  callback;
    void*               callbackCtx;
};

// ---------------------------------------------------------------------------
// COWSnapshot — Copy-On-Write tensor snapshot
// ---------------------------------------------------------------------------
struct COWSnapshot {
    uintptr_t   sourceAddr;         // Original tensor address
    void*       snapshotBuffer;     // VirtualAlloc'd copy
    uint64_t    size;               // Byte count
    uint32_t    id;                 // Snapshot ID
    uint64_t    createTick;         // When snapshot was taken
    bool        valid;              // Snapshot is live and restorable

    // Metadata for debugging
    const char* label;              // "layer_23_weights", etc.
    uint32_t    layerIndex;         // Model layer this snapshot belongs to
};

// ---------------------------------------------------------------------------
// LargePageArena — 2MB/1GB page-backed allocation
// ---------------------------------------------------------------------------
struct LargePageArena {
    void*       base;               // VirtualAlloc'd base (MEM_LARGE_PAGES)
    uint64_t    capacity;           // Total bytes allocated
    uint64_t    used;               // Bytes committed so far
    uint64_t    pageSize;           // 2MB or 1GB
    uint32_t    id;                 // Arena ID
    bool        active;             // Arena is live
};

// ---------------------------------------------------------------------------
// ASLRContext — ASLR normalization state
// ---------------------------------------------------------------------------
struct ASLRContext {
    uintptr_t   moduleBase;         // Actual load address (from GetModuleHandle)
    uintptr_t   preferredBase;      // PE preferred load address (from optional header)
    intptr_t    slideOffset;        // moduleBase - preferredBase (can be negative)
    const char* moduleName;         // e.g. "RawrXD-Shell.exe"
    bool        initialized;        // Context has been populated
};

// ---------------------------------------------------------------------------
// PTDriverStats — Runtime statistics
// ---------------------------------------------------------------------------
struct PTDriverStats {
    std::atomic<uint64_t> pagesWalked{0};
    std::atomic<uint64_t> protectionChanges{0};
    std::atomic<uint64_t> watchpointsArmed{0};
    std::atomic<uint64_t> watchpointHits{0};
    std::atomic<uint64_t> cowSnapshotsTaken{0};
    std::atomic<uint64_t> cowRestores{0};
    std::atomic<uint64_t> cowBytesTotal{0};
    std::atomic<uint64_t> largePagesAllocated{0};
    std::atomic<uint64_t> largeBytesTotal{0};
    std::atomic<uint64_t> aslrNormalizations{0};
    std::atomic<uint64_t> guardFaults{0};
    std::atomic<uint64_t> residencyQueries{0};
    std::atomic<uint64_t> totalErrors{0};
};

// ---------------------------------------------------------------------------
// PTDriverContract — The full page table driver
// ---------------------------------------------------------------------------
class PTDriverContract {
public:
    static PTDriverContract& instance();

    // ═══════════════════════════════════════════════════════════════
    // 1. Page Walk — Enumerate virtual address space via VirtualQuery
    // ═══════════════════════════════════════════════════════════════

    // Walk the virtual address space of the current process.
    // Populates outEntries[] with PTE descriptors for each committed region.
    // Returns actual count of entries written (≤ maxEntries).
    int walk_pages(uintptr_t startAddr, uintptr_t endAddr,
                   PTEDescriptor* outEntries, int maxEntries);

    // Walk a single address — return its PTE descriptor.
    PatchResult describe_page(uintptr_t addr, PTEDescriptor* out);

    // Walk all pages belonging to a specific module (DLL/EXE).
    int walk_module_pages(const char* moduleName,
                          PTEDescriptor* outEntries, int maxEntries);

    // ═══════════════════════════════════════════════════════════════
    // 2. Protection Domain Transitions
    // ═══════════════════════════════════════════════════════════════

    // Change protection on a page-aligned region.
    // Returns old protection in *outOldProtect.
    PatchResult set_protection(uintptr_t addr, uint64_t size,
                               uint32_t newProtect, uint32_t* outOldProtect);

    // Make a region writable, execute a callback, then restore.
    // The callback receives the (now-writable) address and user context.
    typedef PatchResult (*ProtectedWriteCallback)(void* addr, uint64_t size, void* ctx);
    PatchResult protected_write(uintptr_t addr, uint64_t size,
                                ProtectedWriteCallback fn, void* ctx);

    // Strip execute permission from a region (apply NX).
    PatchResult apply_nx(uintptr_t addr, uint64_t size);

    // Grant execute permission to a region (remove NX).
    PatchResult remove_nx(uintptr_t addr, uint64_t size);

    // ═══════════════════════════════════════════════════════════════
    // 3. Guard-Page Watchpoints (VEH-backed)
    // ═══════════════════════════════════════════════════════════════

    // Arm a watchpoint on a memory region.
    // On first access, the VEH fires the callback and (optionally) re-arms.
    PatchResult arm_watchpoint(uintptr_t addr, uint64_t size,
                               WatchpointEntry::WatchpointCallback cb,
                               void* ctx, bool oneShot, uint32_t* outId);

    // Disarm a specific watchpoint by ID.
    PatchResult disarm_watchpoint(uint32_t id);

    // Disarm all active watchpoints.
    PatchResult disarm_all_watchpoints();

    // Re-arm a watchpoint that was disarmed by a one-shot hit.
    PatchResult rearm_watchpoint(uint32_t id);

    // Query watchpoint status.
    PatchResult query_watchpoint(uint32_t id, WatchpointEntry* out);

    // ═══════════════════════════════════════════════════════════════
    // 4. COW Tensor Snapshots
    // ═══════════════════════════════════════════════════════════════

    // Take a snapshot of a memory region (VirtualAlloc + memcpy).
    PatchResult take_snapshot(uintptr_t addr, uint64_t size,
                              const char* label, uint32_t layerIndex,
                              uint32_t* outSnapshotId);

    // Restore a snapshot (memcpy back to original addr).
    PatchResult restore_snapshot(uint32_t snapshotId);

    // Discard a snapshot (VirtualFree the copy buffer).
    PatchResult discard_snapshot(uint32_t snapshotId);

    // Compare a snapshot against current memory.
    // Returns the byte offset of the first difference, or -1 if identical.
    PatchResult diff_snapshot(uint32_t snapshotId, int64_t* outFirstDiff,
                              uint64_t* outDiffCount);

    // List all active snapshots.
    int list_snapshots(COWSnapshot* outEntries, int maxEntries);

    // ═══════════════════════════════════════════════════════════════
    // 5. ASLR Normalization
    // ═══════════════════════════════════════════════════════════════

    // Initialize ASLR context for a module.
    PatchResult init_aslr_context(const char* moduleName, ASLRContext* out);

    // Normalize an absolute address to a module-relative offset.
    // Result = addr - moduleBase (stable across sessions).
    PatchResult normalize_address(const ASLRContext* ctx,
                                  uintptr_t absAddr, uintptr_t* outRelative);

    // De-normalize (rebase) a module-relative offset to absolute.
    // Result = moduleBase + relOffset.
    PatchResult denormalize_address(const ASLRContext* ctx,
                                   uintptr_t relOffset, uintptr_t* outAbsolute);

    // Normalize a MemoryPatchEntry's targetAddr in-place.
    PatchResult normalize_patch_entry(const ASLRContext* ctx,
                                      MemoryPatchEntry* entry);

    // De-normalize a MemoryPatchEntry's targetAddr in-place.
    PatchResult denormalize_patch_entry(const ASLRContext* ctx,
                                       MemoryPatchEntry* entry);

    // ═══════════════════════════════════════════════════════════════
    // 6. Large-Page Arena Allocation
    // ═══════════════════════════════════════════════════════════════

    // Allocate a large-page arena (requires SeLockMemoryPrivilege).
    PatchResult allocate_large_arena(uint64_t sizeBytes, uint64_t pageSize,
                                     uint32_t* outArenaId);

    // Sub-allocate from a large-page arena (bump allocator).
    PatchResult arena_alloc(uint32_t arenaId, uint64_t size,
                            uint64_t alignment, void** outPtr);

    // Free a large-page arena entirely.
    PatchResult free_large_arena(uint32_t arenaId);

    // Query arena stats.
    PatchResult query_arena(uint32_t arenaId, LargePageArena* out);

    // ═══════════════════════════════════════════════════════════════
    // 7. Working-Set Residency Tracking
    // ═══════════════════════════════════════════════════════════════

    // Query whether a set of pages are currently resident in RAM.
    // Populates the 'resident' and 'shareCount' fields of each PTE.
    PatchResult query_residency(uintptr_t addr, uint64_t size,
                                PTEDescriptor* outEntries, int maxEntries,
                                int* outCount);

    // Force pages into the working set (VirtualLock).
    PatchResult lock_pages(uintptr_t addr, uint64_t size);

    // Release pages from the working set (VirtualUnlock).
    PatchResult unlock_pages(uintptr_t addr, uint64_t size);

    // ═══════════════════════════════════════════════════════════════
    // 8. Lifecycle + Stats
    // ═══════════════════════════════════════════════════════════════

    // Initialize the VEH handler and internal state.
    PatchResult initialize();

    // Tear down: disarm all watchpoints, discard all snapshots, free arenas,
    // remove VEH handler.
    PatchResult shutdown();

    // Get live statistics.
    const PTDriverStats& get_stats() const;

    // Reset statistics.
    void reset_stats();

    // Check whether the driver has been initialized.
    bool is_initialized() const;

private:
    PTDriverContract();
    ~PTDriverContract();
    PTDriverContract(const PTDriverContract&) = delete;
    PTDriverContract& operator=(const PTDriverContract&) = delete;

    // ---- Internal helpers ----
    void populate_pte_from_mbi(const void* mbi, PTEDescriptor* out);
    uint32_t next_watchpoint_id();
    uint32_t next_snapshot_id();
    uint32_t next_arena_id();
    int find_watchpoint(uint32_t id);
    int find_snapshot(uint32_t id);
    int find_arena(uint32_t id);

    // ---- VEH handler (static, registered with AddVectoredExceptionHandler) ----
    static long __stdcall veh_guard_handler(struct _EXCEPTION_POINTERS* exceptionInfo);

    // ---- State ----
    bool                    m_initialized;
    void*                   m_vehHandle;        // PVOID from AddVectoredExceptionHandler
    std::mutex              m_mutex;

    // Watchpoint table
    WatchpointEntry         m_watchpoints[PT::MAX_WATCHPOINTS];
    int                     m_watchpointCount;
    std::atomic<uint32_t>   m_nextWatchpointId{1};

    // COW snapshot table
    COWSnapshot             m_snapshots[PT::MAX_COW_SNAPSHOTS];
    int                     m_snapshotCount;
    std::atomic<uint32_t>   m_nextSnapshotId{1};

    // Large-page arena table
    LargePageArena          m_arenas[PT::MAX_LARGE_ARENAS];
    int                     m_arenaCount;
    std::atomic<uint32_t>   m_nextArenaId{1};

    // ASLR context cache (main module)
    ASLRContext             m_mainModuleASLR;

    // Stats
    PTDriverStats           m_stats;
};

// ---------------------------------------------------------------------------
// Convenience free functions (route through PTDriverContract::instance())
// ---------------------------------------------------------------------------

// Initialize the PT driver. Must be called before any other PT operation.
PatchResult pt_initialize();

// Shut down the PT driver, releasing all resources.
PatchResult pt_shutdown();

// Walk pages in a range.
int pt_walk_pages(uintptr_t start, uintptr_t end,
                  PTEDescriptor* out, int maxEntries);

// Describe a single page.
PatchResult pt_describe_page(uintptr_t addr, PTEDescriptor* out);

// Arm a guard-page watchpoint.
PatchResult pt_arm_watchpoint(uintptr_t addr, uint64_t size,
                              WatchpointEntry::WatchpointCallback cb,
                              void* ctx, bool oneShot, uint32_t* outId);

// Take a COW snapshot of a tensor.
PatchResult pt_take_snapshot(uintptr_t addr, uint64_t size,
                             const char* label, uint32_t layerIndex,
                             uint32_t* outSnapshotId);

// Restore a COW snapshot.
PatchResult pt_restore_snapshot(uint32_t snapshotId);

// Initialize ASLR context for a module.
PatchResult pt_init_aslr(const char* moduleName, ASLRContext* out);

// Normalize an address.
PatchResult pt_normalize(const ASLRContext* ctx,
                         uintptr_t absAddr, uintptr_t* outRelative);

// De-normalize an address.
PatchResult pt_denormalize(const ASLRContext* ctx,
                           uintptr_t relOffset, uintptr_t* outAbsolute);

// Allocate a large-page arena.
PatchResult pt_alloc_large_arena(uint64_t size, uint64_t pageSize,
                                 uint32_t* outArenaId);

// Lock pages into working set.
PatchResult pt_lock_pages(uintptr_t addr, uint64_t size);
