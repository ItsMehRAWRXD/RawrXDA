#pragma once
// =============================================================================
// DirectionlessLoader.h — Directionless Memory Load/Unload Engine API
//
// Omnidirectional memory management: memory can flow in ANY direction via
// 8 direction vectors (scatter/gather/swap/rotate/clone/evict/swap-lock/nop).
// Load level is pinned at a setpoint via VirtualProtect memory locking and
// self-modifying hotpatch trampolines.
//
// Usage:
//   auto dl = DirLoad_Init(75);          // 75% initial load target
//   DirLoad_LockLoad(dl);                // Lock at 75% — no drift allowed
//   for (...) {
//       int pct = DirLoad_Tick(dl);       // Directionless cycle
//       printf("Load: %d%%\n", pct);      // Stays pinned at 75%
//   }
//   DirLoad_UnlockLoad(dl);              // Allow drift again
//   DirLoad_Destroy(dl);
// =============================================================================

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Direction Vectors ──────────────────────────────────────────────────────
constexpr int DIRLOAD_SCATTER       = 0;   // Spread data across free regions
constexpr int DIRLOAD_GATHER        = 1;   // Pull/access loaded regions
constexpr int DIRLOAD_SWAP          = 2;   // Exchange loaded↔free bidirectionally
constexpr int DIRLOAD_ROTATE        = 3;   // Circular shift all slot states
constexpr int DIRLOAD_CLONE_SCATTER = 4;   // Duplicate + scatter (redundancy)
constexpr int DIRLOAD_EVICT_ANY     = 5;   // Free any loaded region (directionless)
constexpr int DIRLOAD_SWAP_LOCK     = 6;   // Swap + lock destination (pin)
constexpr int DIRLOAD_NOP_THROUGH   = 7;   // Pass-through (no change)
constexpr int DIRLOAD_MAX_DIR       = 8;

// ─── Memory Region States ───────────────────────────────────────────────────
constexpr int DIRLOAD_MR_FREE     = 0;   // Not allocated
constexpr int DIRLOAD_MR_LOADED   = 1;   // Data present, read-write
constexpr int DIRLOAD_MR_LOCKED   = 2;   // Data present, read-only (pinned via VirtualProtect)
constexpr int DIRLOAD_MR_EVICTING = 3;   // Being freed
constexpr int DIRLOAD_MR_PATCHING = 4;   // Being hotpatched

constexpr int DIRLOAD_MAX_SLOTS       = 64;
constexpr int DIRLOAD_MAX_TRAMPOLINES = 16;

// ─── Opaque Context ─────────────────────────────────────────────────────────
typedef void* DirLoadContext;

// =============================================================================
// API Functions (implemented in RawrXD_DirectionlessLoader.asm)
// =============================================================================

/// Initialize the directionless loader.
/// @param loadSetpointPct  Initial load target (0–100). 0 = no target.
/// @return                 Engine context, or NULL
DirLoadContext DirLoad_Init(uint32_t loadSetpointPct);

/// Destroy the engine and free all memory.
void DirLoad_Destroy(DirLoadContext ctx);

/// Advance one directionless cycle.
/// Picks a random direction, executes it, enforces load lock if active.
/// @return Current load percentage (0–100)
int DirLoad_Tick(DirLoadContext ctx);

/// Set target load percentage (0–100).
/// Does NOT lock — call DirLoad_LockLoad to pin the setpoint.
void DirLoad_SetLoadPct(DirLoadContext ctx, int loadPct);

/// Lock load at current setpoint. Engine auto-corrects any drift.
void DirLoad_LockLoad(DirLoadContext ctx);

/// Unlock load (allow drift from setpoint).
void DirLoad_UnlockLoad(DirLoadContext ctx);

/// Get current measured load percentage (0–100).
int DirLoad_GetLoadPct(DirLoadContext ctx);

/// Feed a memory region into the slot table.
/// @param baseAddr    Virtual address of the region
/// @param regionSize  Size in bytes
/// @return            Slot index, or -1 if table full
int DirLoad_FeedSlot(DirLoadContext ctx, void* baseAddr, uint64_t regionSize);

/// Write a hotpatch trampoline (self-modifying code).
/// @param slotID   Trampoline slot (0–15)
/// @param target   Target address to JMP to
/// @return         1 on success
int DirLoad_HotPatch(DirLoadContext ctx, int slotID, void* target);

/// Memory-patch: lock/unlock a slot's pages via VirtualProtect.
/// @param slotIdx  Slot index (0–63)
/// @param lock     1 = lock (PAGE_READONLY), 0 = unlock (PAGE_READWRITE)
/// @return         1 on success
int DirLoad_MemPatch(DirLoadContext ctx, int slotIdx, int lock);

/// Get current active direction vector (0–7).
int DirLoad_GetDirection(DirLoadContext ctx);

// =============================================================================
// Statistics
// =============================================================================

struct DirLoadStats {
    uint64_t tickCount;         // 0x00
    uint32_t currentLoadPct;    // 0x08
    uint32_t loadSetpoint;      // 0x0C
    uint32_t loadLocked;        // 0x10
    uint32_t activeDirection;   // 0x14
    uint32_t loadedSlots;       // 0x18
    uint32_t lockedSlots;       // 0x1C
    uint32_t freeSlots;         // 0x20
    uint32_t trampolineCount;   // 0x24
    uint64_t scatters;          // 0x28
    uint64_t gathers;           // 0x30
    uint64_t swaps;             // 0x38
    uint64_t rotates;           // 0x40
    uint64_t clones;            // 0x48
    uint64_t evicts;            // 0x50
    uint64_t swapLocks;         // 0x58
    uint64_t nops;              // 0x60
    uint64_t patchOps;          // 0x68
    uint64_t memPatches;        // 0x70
    uint64_t lockDrifts;        // 0x78
};

/// Read engine statistics (128 bytes).
int DirLoad_GetStats(DirLoadContext ctx, DirLoadStats* stats);

/// Direction vector name (for display).
inline const char* DirLoad_DirectionName(int dir) {
    static const char* names[] = {
        "SCATTER", "GATHER", "SWAP", "ROTATE",
        "CLONE_SCATTER", "EVICT_ANY", "SWAP_LOCK", "NOP_THROUGH"
    };
    if (dir >= 0 && dir < DIRLOAD_MAX_DIR) return names[dir];
    return "???";
}

#ifdef __cplusplus
} // extern "C"
#endif
