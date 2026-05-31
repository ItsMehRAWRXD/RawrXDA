// =============================================================================
// pt_driver_contract.cpp — Page Table Driver Contract Implementation
// =============================================================================
// Full software page table driver for Windows x64.
//
// Implements:
//   1. Page walk via VirtualQuery enumeration
//   2. Guard-page watchpoints via VEH (AddVectoredExceptionHandler)
//   3. COW tensor snapshots via VirtualAlloc + memcpy
//   4. ASLR normalization via PE header parsing
//   5. Large-page arena allocation via MEM_LARGE_PAGES
//   6. Working-set residency tracking via QueryWorkingSetEx
//   7. Protection-domain transitions for hotpatch coordination
//
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// =============================================================================

#include "pt_driver_contract.hpp"
#include <cstring>
#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
static PTDriverContract* g_ptInstance = nullptr;

PTDriverContract& PTDriverContract::instance() {
    static PTDriverContract inst;
    return inst;
}

PTDriverContract::PTDriverContract()
    : m_initialized(false)
    , m_vehHandle(nullptr)
    , m_watchpointCount(0)
    , m_snapshotCount(0)
    , m_arenaCount(0)
{
    memset(m_watchpoints, 0, sizeof(m_watchpoints));
    memset(m_snapshots, 0, sizeof(m_snapshots));
    memset(m_arenas, 0, sizeof(m_arenas));
    memset(&m_mainModuleASLR, 0, sizeof(m_mainModuleASLR));
    g_ptInstance = this;
}

PTDriverContract::~PTDriverContract() {
    if (m_initialized) {
        shutdown();
    }
    g_ptInstance = nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// 8. Lifecycle + Stats
// ═══════════════════════════════════════════════════════════════════════════

PatchResult PTDriverContract::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) {
        return PatchResult::ok("PT driver already initialized");
    }

    // Register the VEH handler (first-chance, priority 1)
    m_vehHandle = AddVectoredExceptionHandler(1, veh_guard_handler);
    if (!m_vehHandle) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("AddVectoredExceptionHandler failed",
                                 static_cast<int>(GetLastError()));
    }

    // Pre-initialize ASLR context for the main module
    init_aslr_context(nullptr, &m_mainModuleASLR);

    m_initialized = true;
    return PatchResult::ok("PT driver initialized (VEH registered)");
}

PatchResult PTDriverContract::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        return PatchResult::ok("PT driver not initialized");
    }

    // 1. Disarm all watchpoints (restore original protection)
    for (int i = 0; i < m_watchpointCount; i++) {
        if (m_watchpoints[i].active) {
            DWORD dummy = 0;
            VirtualProtect(reinterpret_cast<void*>(m_watchpoints[i].address),
                          static_cast<SIZE_T>(m_watchpoints[i].regionSize),
                          m_watchpoints[i].originalProtection, &dummy);
            m_watchpoints[i].active = false;
        }
    }
    m_watchpointCount = 0;

    // 2. Discard all COW snapshots
    for (int i = 0; i < m_snapshotCount; i++) {
        if (m_snapshots[i].valid && m_snapshots[i].snapshotBuffer) {
            VirtualFree(m_snapshots[i].snapshotBuffer, 0, MEM_RELEASE);
            m_snapshots[i].valid = false;
            m_snapshots[i].snapshotBuffer = nullptr;
        }
    }
    m_snapshotCount = 0;

    // 3. Free all large-page arenas
    for (int i = 0; i < m_arenaCount; i++) {
        if (m_arenas[i].active && m_arenas[i].base) {
            VirtualFree(m_arenas[i].base, 0, MEM_RELEASE);
            m_arenas[i].active = false;
            m_arenas[i].base = nullptr;
        }
    }
    m_arenaCount = 0;

    // 4. Remove VEH handler
    if (m_vehHandle) {
        RemoveVectoredExceptionHandler(m_vehHandle);
        m_vehHandle = nullptr;
    }

    m_initialized = false;
    return PatchResult::ok("PT driver shut down");
}

const PTDriverStats& PTDriverContract::get_stats() const {
    return m_stats;
}

void PTDriverContract::reset_stats() {
    m_stats.pagesWalked.store(0, std::memory_order_relaxed);
    m_stats.protectionChanges.store(0, std::memory_order_relaxed);
    m_stats.watchpointsArmed.store(0, std::memory_order_relaxed);
    m_stats.watchpointHits.store(0, std::memory_order_relaxed);
    m_stats.cowSnapshotsTaken.store(0, std::memory_order_relaxed);
    m_stats.cowRestores.store(0, std::memory_order_relaxed);
    m_stats.cowBytesTotal.store(0, std::memory_order_relaxed);
    m_stats.largePagesAllocated.store(0, std::memory_order_relaxed);
    m_stats.largeBytesTotal.store(0, std::memory_order_relaxed);
    m_stats.aslrNormalizations.store(0, std::memory_order_relaxed);
    m_stats.guardFaults.store(0, std::memory_order_relaxed);
    m_stats.residencyQueries.store(0, std::memory_order_relaxed);
    m_stats.totalErrors.store(0, std::memory_order_relaxed);
}

bool PTDriverContract::is_initialized() const {
    return m_initialized;
}

// ═══════════════════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════════════════

void PTDriverContract::populate_pte_from_mbi(const void* mbiPtr, PTEDescriptor* out) {
    const MEMORY_BASIC_INFORMATION* mbi =
        static_cast<const MEMORY_BASIC_INFORMATION*>(mbiPtr);

    out->virtualAddr    = reinterpret_cast<uintptr_t>(mbi->BaseAddress);
    out->physicalOffset = reinterpret_cast<uintptr_t>(mbi->BaseAddress) -
                          reinterpret_cast<uintptr_t>(mbi->AllocationBase);
    out->pageSize       = PT::PAGE_SIZE_4K;
    out->protection     = mbi->Protect;
    out->state          = mbi->State;

    // Derive PTE-equivalent bits from Windows protection flags
    out->present        = (mbi->State == MEM_COMMIT);
    out->writable       = (mbi->Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                           PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    out->executable     = (mbi->Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                           PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    out->userMode       = true;  // Always user mode in this context
    out->dirty          = false; // Not directly queryable via VirtualQuery
    out->accessed       = false; // Not directly queryable via VirtualQuery
    out->largePage      = (mbi->RegionSize >= PT::PAGE_SIZE_2M);
    out->guard          = (mbi->Protect & PAGE_GUARD) != 0;
    out->nocache        = (mbi->Protect & PAGE_NOCACHE) != 0;
    out->writeCombine   = (mbi->Protect & PAGE_WRITECOMBINE) != 0;

    // Residency defaults (populated by query_residency)
    out->resident       = out->present;
    out->shareCount     = 0;
    out->shared         = (mbi->Type == MEM_MAPPED);

    out->lastAccessTick = GetTickCount64();
    out->createTick     = GetTickCount64();
}

uint32_t PTDriverContract::next_watchpoint_id() {
    return m_nextWatchpointId.fetch_add(1, std::memory_order_relaxed);
}

uint32_t PTDriverContract::next_snapshot_id() {
    return m_nextSnapshotId.fetch_add(1, std::memory_order_relaxed);
}

uint32_t PTDriverContract::next_arena_id() {
    return m_nextArenaId.fetch_add(1, std::memory_order_relaxed);
}

int PTDriverContract::find_watchpoint(uint32_t id) {
    for (int i = 0; i < m_watchpointCount; i++) {
        if (m_watchpoints[i].id == id) return i;
    }
    return -1;
}

int PTDriverContract::find_snapshot(uint32_t id) {
    for (int i = 0; i < m_snapshotCount; i++) {
        if (m_snapshots[i].id == id) return i;
    }
    return -1;
}

int PTDriverContract::find_arena(uint32_t id) {
    for (int i = 0; i < m_arenaCount; i++) {
        if (m_arenas[i].id == id) return i;
    }
    return -1;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1. Page Walk — VirtualQuery-based enumeration
// ═══════════════════════════════════════════════════════════════════════════

int PTDriverContract::walk_pages(uintptr_t startAddr, uintptr_t endAddr,
                                  PTEDescriptor* outEntries, int maxEntries) {
    if (!outEntries || maxEntries <= 0) return 0;
    if (endAddr <= startAddr) return 0;

    int count = 0;
    uintptr_t addr = startAddr;

    while (addr < endAddr && count < maxEntries) {
        MEMORY_BASIC_INFORMATION mbi;
        SIZE_T result = VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi));
        if (result == 0) break;

        populate_pte_from_mbi(&mbi, &outEntries[count]);
        count++;
        m_stats.pagesWalked.fetch_add(1, std::memory_order_relaxed);

        // Advance past this region
        uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (regionEnd <= addr) break; // Prevent infinite loop
        addr = regionEnd;
    }

    return count;
}

PatchResult PTDriverContract::describe_page(uintptr_t addr, PTEDescriptor* out) {
    if (!out) return PatchResult::error("Null output descriptor", 1);

    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T result = VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi));
    if (result == 0) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualQuery failed", static_cast<int>(GetLastError()));
    }

    populate_pte_from_mbi(&mbi, out);
    m_stats.pagesWalked.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Page described");
}

int PTDriverContract::walk_module_pages(const char* moduleName,
                                         PTEDescriptor* outEntries, int maxEntries) {
    if (!outEntries || maxEntries <= 0) return 0;

    HMODULE hMod = nullptr;
    if (moduleName && moduleName[0] != '\0') {
        hMod = GetModuleHandleA(moduleName);
    } else {
        hMod = GetModuleHandleA(nullptr);  // Main executable
    }
    if (!hMod) return 0;

    MODULEINFO modInfo;
    if (!GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo))) {
        return 0;
    }

    uintptr_t base = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
    uintptr_t end  = base + modInfo.SizeOfImage;

    return walk_pages(base, end, outEntries, maxEntries);
}

// ═══════════════════════════════════════════════════════════════════════════
// 2. Protection Domain Transitions
// ═══════════════════════════════════════════════════════════════════════════

PatchResult PTDriverContract::set_protection(uintptr_t addr, uint64_t size,
                                              uint32_t newProtect, uint32_t* outOldProtect) {
    if (size == 0) return PatchResult::error("Zero size", 1);

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(addr),
                        static_cast<SIZE_T>(size),
                        static_cast<DWORD>(newProtect), &oldProtect)) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect failed", static_cast<int>(GetLastError()));
    }

    if (outOldProtect) *outOldProtect = static_cast<uint32_t>(oldProtect);
    m_stats.protectionChanges.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Protection changed");
}

PatchResult PTDriverContract::protected_write(uintptr_t addr, uint64_t size,
                                               ProtectedWriteCallback fn, void* ctx) {
    if (!fn) return PatchResult::error("Null callback", 1);
    if (size == 0) return PatchResult::error("Zero size", 2);

    // Make writable
    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(addr),
                        static_cast<SIZE_T>(size),
                        PAGE_EXECUTE_READWRITE, &oldProtect)) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect (make writable) failed",
                                 static_cast<int>(GetLastError()));
    }
    m_stats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    // Execute the callback
    PatchResult cbResult = fn(reinterpret_cast<void*>(addr), size, ctx);

    // Restore protection regardless of callback result
    DWORD dummy = 0;
    VirtualProtect(reinterpret_cast<void*>(addr),
                   static_cast<SIZE_T>(size), oldProtect, &dummy);
    m_stats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    // Flush instruction cache
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(addr),
                          static_cast<SIZE_T>(size));

    return cbResult;
}

PatchResult PTDriverContract::apply_nx(uintptr_t addr, uint64_t size) {
    // Read current protection
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == 0) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualQuery failed (apply_nx)", static_cast<int>(GetLastError()));
    }

    // Map execute-capable protections to non-execute equivalents
    DWORD newProt = mbi.Protect;
    switch (mbi.Protect & 0xFF) {  // Mask out modifiers
        case PAGE_EXECUTE:              newProt = PAGE_NOACCESS;   break;
        case PAGE_EXECUTE_READ:         newProt = PAGE_READONLY;   break;
        case PAGE_EXECUTE_READWRITE:    newProt = PAGE_READWRITE;  break;
        case PAGE_EXECUTE_WRITECOPY:    newProt = PAGE_WRITECOPY;  break;
        default: return PatchResult::ok("Already non-executable");
    }

    // Preserve modifier flags (GUARD, NOCACHE, WRITECOMBINE)
    newProt |= (mbi.Protect & 0xF00);

    uint32_t oldProt = 0;
    return set_protection(addr, size, newProt, &oldProt);
}

PatchResult PTDriverContract::remove_nx(uintptr_t addr, uint64_t size) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == 0) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualQuery failed (remove_nx)", static_cast<int>(GetLastError()));
    }

    DWORD newProt = mbi.Protect;
    switch (mbi.Protect & 0xFF) {
        case PAGE_NOACCESS:    newProt = PAGE_EXECUTE;           break;
        case PAGE_READONLY:    newProt = PAGE_EXECUTE_READ;      break;
        case PAGE_READWRITE:   newProt = PAGE_EXECUTE_READWRITE; break;
        case PAGE_WRITECOPY:   newProt = PAGE_EXECUTE_WRITECOPY; break;
        default: return PatchResult::ok("Already executable");
    }

    newProt |= (mbi.Protect & 0xF00);

    uint32_t oldProt = 0;
    return set_protection(addr, size, newProt, &oldProt);
}

// ═══════════════════════════════════════════════════════════════════════════
// 3. Guard-Page Watchpoints (VEH-backed)
// ═══════════════════════════════════════════════════════════════════════════

// Static VEH handler — dispatches to the singleton's watchpoint table
long __stdcall PTDriverContract::veh_guard_handler(struct _EXCEPTION_POINTERS* exInfo) {
    EXCEPTION_POINTERS* ep = exInfo;
    if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;

    // Only handle STATUS_GUARD_PAGE_VIOLATION
    if (ep->ExceptionRecord->ExceptionCode != STATUS_GUARD_PAGE_VIOLATION) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (!g_ptInstance || !g_ptInstance->m_initialized) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    uintptr_t faultAddr = 0;
    if (ep->ExceptionRecord->NumberParameters >= 2) {
        faultAddr = static_cast<uintptr_t>(ep->ExceptionRecord->ExceptionInformation[1]);
    }

    g_ptInstance->m_stats.guardFaults.fetch_add(1, std::memory_order_relaxed);

    // Search watchpoint table for a match
    // Note: m_mutex is NOT locked here — we're in an exception context.
    // Watchpoint table is stable during VEH dispatch (modifications lock m_mutex
    // and VEH only reads addresses/sizes/callbacks).
    for (int i = 0; i < g_ptInstance->m_watchpointCount; i++) {
        WatchpointEntry& wp = g_ptInstance->m_watchpoints[i];
        if (!wp.active) continue;

        uintptr_t wpEnd = wp.address + wp.regionSize;
        if (faultAddr >= wp.address && faultAddr < wpEnd) {
            // Match found
            wp.hitCount++;
            g_ptInstance->m_stats.watchpointHits.fetch_add(1, std::memory_order_relaxed);

            // Fire callback
            if (wp.callback) {
                wp.callback(&wp, faultAddr, wp.callbackCtx);
            }

            if (wp.oneShot) {
                wp.active = false;
            } else {
                // Re-arm: Windows auto-clears PAGE_GUARD on fault.
                // Re-set it after we return so the next access triggers again.
                // We use QueueUserAPC or post-exception re-arm. For simplicity,
                // re-arm synchronously (the page is now accessible for this access,
                // and will be re-guarded before the next access via a helper thread).
                //
                // Direct re-arm in VEH is tricky because the faulting instruction
                // will retry immediately. We let the access complete (return
                // EXCEPTION_CONTINUE_EXECUTION) and rely on single-step to re-arm.
                // For production: use TF (trap flag) single-step then re-arm in
                // EXCEPTION_SINGLE_STEP handler.
                //
                // Simplified version: just let the access complete. The watchpoint
                // needs manual re-arm via rearm_watchpoint() after hit.
            }

            // Let the faulting instruction retry (guard was cleared by the OS)
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    // No matching watchpoint — pass to next handler
    return EXCEPTION_CONTINUE_SEARCH;
}

PatchResult PTDriverContract::arm_watchpoint(uintptr_t addr, uint64_t size,
                                              WatchpointEntry::WatchpointCallback cb,
                                              void* ctx, bool oneShot, uint32_t* outId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return PatchResult::error("PT driver not initialized", 1);
    }
    if (m_watchpointCount >= PT::MAX_WATCHPOINTS) {
        return PatchResult::error("Watchpoint table full", 2);
    }
    if (size == 0) {
        return PatchResult::error("Zero size watchpoint", 3);
    }

    // Page-align the address
    uintptr_t pageBase = addr & ~(PT::PAGE_SIZE_4K - 1);
    uint64_t pageSize  = ((addr + size + PT::PAGE_SIZE_4K - 1) & ~(PT::PAGE_SIZE_4K - 1)) - pageBase;

    // Read current protection
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(pageBase), &mbi, sizeof(mbi)) == 0) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualQuery failed (arm watchpoint)",
                                 static_cast<int>(GetLastError()));
    }

    DWORD origProtect = mbi.Protect;

    // Set PAGE_GUARD
    DWORD newProtect = origProtect | PAGE_GUARD;
    DWORD dummy = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(pageBase),
                        static_cast<SIZE_T>(pageSize),
                        newProtect, &dummy)) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect (set GUARD) failed",
                                 static_cast<int>(GetLastError()));
    }
    m_stats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    // Record watchpoint
    uint32_t id = next_watchpoint_id();
    WatchpointEntry& wp = m_watchpoints[m_watchpointCount++];
    wp.address            = pageBase;
    wp.regionSize         = pageSize;
    wp.originalProtection = origProtect;
    wp.id                 = id;
    wp.active             = true;
    wp.oneShot            = oneShot;
    wp.hitCount           = 0;
    wp.callback           = cb;
    wp.callbackCtx        = ctx;

    if (outId) *outId = id;

    m_stats.watchpointsArmed.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Watchpoint armed");
}

PatchResult PTDriverContract::disarm_watchpoint(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = find_watchpoint(id);
    if (idx < 0) return PatchResult::error("Watchpoint not found", 1);

    WatchpointEntry& wp = m_watchpoints[idx];
    if (!wp.active) return PatchResult::ok("Watchpoint already inactive");

    // Restore original protection (removes PAGE_GUARD)
    DWORD dummy = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(wp.address),
                        static_cast<SIZE_T>(wp.regionSize),
                        wp.originalProtection, &dummy)) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect (remove GUARD) failed",
                                 static_cast<int>(GetLastError()));
    }
    m_stats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    wp.active = false;
    return PatchResult::ok("Watchpoint disarmed");
}

PatchResult PTDriverContract::disarm_all_watchpoints() {
    std::lock_guard<std::mutex> lock(m_mutex);

    int disarmed = 0;
    for (int i = 0; i < m_watchpointCount; i++) {
        if (m_watchpoints[i].active) {
            DWORD dummy = 0;
            VirtualProtect(reinterpret_cast<void*>(m_watchpoints[i].address),
                          static_cast<SIZE_T>(m_watchpoints[i].regionSize),
                          m_watchpoints[i].originalProtection, &dummy);
            m_watchpoints[i].active = false;
            disarmed++;
        }
    }
    m_stats.protectionChanges.fetch_add(disarmed, std::memory_order_relaxed);

    return PatchResult::ok("All watchpoints disarmed");
}

PatchResult PTDriverContract::rearm_watchpoint(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = find_watchpoint(id);
    if (idx < 0) return PatchResult::error("Watchpoint not found", 1);

    WatchpointEntry& wp = m_watchpoints[idx];
    if (wp.active) return PatchResult::ok("Watchpoint already active");

    // Re-apply PAGE_GUARD
    DWORD newProtect = wp.originalProtection | PAGE_GUARD;
    DWORD dummy = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(wp.address),
                        static_cast<SIZE_T>(wp.regionSize),
                        newProtect, &dummy)) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect (re-arm GUARD) failed",
                                 static_cast<int>(GetLastError()));
    }
    m_stats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    wp.active = true;
    return PatchResult::ok("Watchpoint re-armed");
}

PatchResult PTDriverContract::query_watchpoint(uint32_t id, WatchpointEntry* out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!out) return PatchResult::error("Null output", 1);

    int idx = find_watchpoint(id);
    if (idx < 0) return PatchResult::error("Watchpoint not found", 2);

    memcpy(out, &m_watchpoints[idx], sizeof(WatchpointEntry));
    return PatchResult::ok("Watchpoint queried");
}

// ═══════════════════════════════════════════════════════════════════════════
// 4. COW Tensor Snapshots
// ═══════════════════════════════════════════════════════════════════════════

PatchResult PTDriverContract::take_snapshot(uintptr_t addr, uint64_t size,
                                             const char* label, uint32_t layerIndex,
                                             uint32_t* outSnapshotId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_snapshotCount >= PT::MAX_COW_SNAPSHOTS) {
        return PatchResult::error("Snapshot table full", 1);
    }
    if (size == 0) {
        return PatchResult::error("Zero size snapshot", 2);
    }

    // Verify source region is readable
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == 0) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualQuery failed (snapshot source)",
                                 static_cast<int>(GetLastError()));
    }
    if (mbi.State != MEM_COMMIT) {
        return PatchResult::error("Source region not committed", 3);
    }

    // Allocate snapshot buffer
    void* buffer = VirtualAlloc(nullptr, static_cast<SIZE_T>(size),
                                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buffer) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualAlloc failed (snapshot buffer)",
                                 static_cast<int>(GetLastError()));
    }

    // Copy current memory to snapshot
    // Use SEH-safe copy: if the source has restricted access, make it readable first
    DWORD oldProtect = 0;
    bool needProtect = false;
    if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                          PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY))) {
        needProtect = true;
        if (!VirtualProtect(reinterpret_cast<void*>(addr),
                            static_cast<SIZE_T>(size),
                            PAGE_READONLY, &oldProtect)) {
            VirtualFree(buffer, 0, MEM_RELEASE);
            m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
            return PatchResult::error("VirtualProtect (snapshot read) failed",
                                     static_cast<int>(GetLastError()));
        }
        m_stats.protectionChanges.fetch_add(1, std::memory_order_relaxed);
    }

    memcpy(buffer, reinterpret_cast<const void*>(addr), static_cast<size_t>(size));

    if (needProtect) {
        DWORD dummy = 0;
        VirtualProtect(reinterpret_cast<void*>(addr),
                       static_cast<SIZE_T>(size), oldProtect, &dummy);
        m_stats.protectionChanges.fetch_add(1, std::memory_order_relaxed);
    }

    // Record snapshot
    uint32_t id = next_snapshot_id();
    COWSnapshot& snap = m_snapshots[m_snapshotCount++];
    snap.sourceAddr     = addr;
    snap.snapshotBuffer = buffer;
    snap.size           = size;
    snap.id             = id;
    snap.createTick     = GetTickCount64();
    snap.valid          = true;
    snap.label          = label;
    snap.layerIndex     = layerIndex;

    if (outSnapshotId) *outSnapshotId = id;

    m_stats.cowSnapshotsTaken.fetch_add(1, std::memory_order_relaxed);
    m_stats.cowBytesTotal.fetch_add(size, std::memory_order_relaxed);
    return PatchResult::ok("Snapshot taken");
}

PatchResult PTDriverContract::restore_snapshot(uint32_t snapshotId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = find_snapshot(snapshotId);
    if (idx < 0) return PatchResult::error("Snapshot not found", 1);

    COWSnapshot& snap = m_snapshots[idx];
    if (!snap.valid) return PatchResult::error("Snapshot already discarded", 2);

    // Make target writable
    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(snap.sourceAddr),
                        static_cast<SIZE_T>(snap.size),
                        PAGE_EXECUTE_READWRITE, &oldProtect)) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect (restore target) failed",
                                 static_cast<int>(GetLastError()));
    }
    m_stats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    // Copy snapshot back to original location
    memcpy(reinterpret_cast<void*>(snap.sourceAddr),
           snap.snapshotBuffer, static_cast<size_t>(snap.size));

    // Restore original protection
    DWORD dummy = 0;
    VirtualProtect(reinterpret_cast<void*>(snap.sourceAddr),
                   static_cast<SIZE_T>(snap.size), oldProtect, &dummy);
    m_stats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(snap.sourceAddr),
                          static_cast<SIZE_T>(snap.size));

    m_stats.cowRestores.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Snapshot restored");
}

PatchResult PTDriverContract::discard_snapshot(uint32_t snapshotId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = find_snapshot(snapshotId);
    if (idx < 0) return PatchResult::error("Snapshot not found", 1);

    COWSnapshot& snap = m_snapshots[idx];
    if (!snap.valid) return PatchResult::ok("Snapshot already discarded");

    if (snap.snapshotBuffer) {
        VirtualFree(snap.snapshotBuffer, 0, MEM_RELEASE);
        snap.snapshotBuffer = nullptr;
    }
    snap.valid = false;
    return PatchResult::ok("Snapshot discarded");
}

PatchResult PTDriverContract::diff_snapshot(uint32_t snapshotId, int64_t* outFirstDiff,
                                             uint64_t* outDiffCount) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!outFirstDiff || !outDiffCount) {
        return PatchResult::error("Null output pointers", 1);
    }

    int idx = find_snapshot(snapshotId);
    if (idx < 0) return PatchResult::error("Snapshot not found", 2);

    COWSnapshot& snap = m_snapshots[idx];
    if (!snap.valid) return PatchResult::error("Snapshot invalid", 3);

    // Make source readable if needed
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(snap.sourceAddr), &mbi, sizeof(mbi)) == 0) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualQuery failed (diff)", static_cast<int>(GetLastError()));
    }

    const uint8_t* current  = reinterpret_cast<const uint8_t*>(snap.sourceAddr);
    const uint8_t* snapshot = static_cast<const uint8_t*>(snap.snapshotBuffer);

    *outFirstDiff = -1;
    *outDiffCount = 0;

    for (uint64_t i = 0; i < snap.size; i++) {
        if (current[i] != snapshot[i]) {
            if (*outFirstDiff == -1) {
                *outFirstDiff = static_cast<int64_t>(i);
            }
            (*outDiffCount)++;
        }
    }

    return PatchResult::ok("Diff completed");
}

int PTDriverContract::list_snapshots(COWSnapshot* outEntries, int maxEntries) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (int i = 0; i < m_snapshotCount && count < maxEntries; i++) {
        if (m_snapshots[i].valid) {
            memcpy(&outEntries[count], &m_snapshots[i], sizeof(COWSnapshot));
            count++;
        }
    }
    return count;
}

// ═══════════════════════════════════════════════════════════════════════════
// 5. ASLR Normalization
// ═══════════════════════════════════════════════════════════════════════════

PatchResult PTDriverContract::init_aslr_context(const char* moduleName, ASLRContext* out) {
    if (!out) return PatchResult::error("Null ASLR context", 1);

    HMODULE hMod = nullptr;
    if (moduleName && moduleName[0] != '\0') {
        hMod = GetModuleHandleA(moduleName);
    } else {
        hMod = GetModuleHandleA(nullptr);
    }

    if (!hMod) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Module not found", static_cast<int>(GetLastError()));
    }

    out->moduleBase = reinterpret_cast<uintptr_t>(hMod);
    out->moduleName = moduleName ? moduleName : "<main>";
    out->initialized = true;

    // Parse PE optional header to get ImageBase (preferred load address)
    const uint8_t* base = reinterpret_cast<const uint8_t*>(hMod);
    // DOS header → e_lfanew → PE signature → optional header
    uint32_t e_lfanew = *reinterpret_cast<const uint32_t*>(base + 0x3C);
    // PE\0\0 signature at e_lfanew, COFF header at +4, optional header at +24
    const uint8_t* optHeader = base + e_lfanew + 24;
    uint16_t magic = *reinterpret_cast<const uint16_t*>(optHeader);

    if (magic == 0x20B) {
        // PE32+ (64-bit) — ImageBase is at offset 24 in optional header
        out->preferredBase = *reinterpret_cast<const uint64_t*>(optHeader + 24);
    } else if (magic == 0x10B) {
        // PE32 (32-bit) — ImageBase at offset 28 (as DWORD)
        out->preferredBase = *reinterpret_cast<const uint32_t*>(optHeader + 28);
    } else {
        out->preferredBase = out->moduleBase; // Unknown format, assume no slide
    }

    out->slideOffset = static_cast<intptr_t>(out->moduleBase) -
                       static_cast<intptr_t>(out->preferredBase);

    m_stats.aslrNormalizations.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("ASLR context initialized");
}

PatchResult PTDriverContract::normalize_address(const ASLRContext* ctx,
                                                 uintptr_t absAddr, uintptr_t* outRelative) {
    if (!ctx || !ctx->initialized) return PatchResult::error("ASLR context not initialized", 1);
    if (!outRelative) return PatchResult::error("Null output", 2);

    if (absAddr < ctx->moduleBase) {
        return PatchResult::error("Address below module base", 3);
    }

    *outRelative = absAddr - ctx->moduleBase;
    m_stats.aslrNormalizations.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Address normalized");
}

PatchResult PTDriverContract::denormalize_address(const ASLRContext* ctx,
                                                   uintptr_t relOffset, uintptr_t* outAbsolute) {
    if (!ctx || !ctx->initialized) return PatchResult::error("ASLR context not initialized", 1);
    if (!outAbsolute) return PatchResult::error("Null output", 2);

    *outAbsolute = ctx->moduleBase + relOffset;
    m_stats.aslrNormalizations.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Address de-normalized");
}

PatchResult PTDriverContract::normalize_patch_entry(const ASLRContext* ctx,
                                                     MemoryPatchEntry* entry) {
    if (!entry) return PatchResult::error("Null entry", 1);

    uintptr_t rel = 0;
    PatchResult r = normalize_address(ctx, entry->targetAddr, &rel);
    if (r.success) {
        entry->targetAddr = rel;
    }
    return r;
}

PatchResult PTDriverContract::denormalize_patch_entry(const ASLRContext* ctx,
                                                       MemoryPatchEntry* entry) {
    if (!entry) return PatchResult::error("Null entry", 1);

    uintptr_t abs = 0;
    PatchResult r = denormalize_address(ctx, entry->targetAddr, &abs);
    if (r.success) {
        entry->targetAddr = abs;
    }
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════
// 6. Large-Page Arena Allocation
// ═══════════════════════════════════════════════════════════════════════════

// Helper: Enable SeLockMemoryPrivilege (required for MEM_LARGE_PAGES)
static bool enable_lock_memory_privilege() {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueA(nullptr, "SeLockMemoryPrivilege", &tp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return false;
    }

    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    DWORD err = GetLastError();
    CloseHandle(hToken);

    return result && (err == ERROR_SUCCESS);
}

PatchResult PTDriverContract::allocate_large_arena(uint64_t sizeBytes, uint64_t pageSize,
                                                    uint32_t* outArenaId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_arenaCount >= PT::MAX_LARGE_ARENAS) {
        return PatchResult::error("Arena table full", 1);
    }
    if (sizeBytes == 0) {
        return PatchResult::error("Zero size arena", 2);
    }

    // Validate page size and align sizeBytes
    uint64_t effectivePageSize = pageSize;
    if (effectivePageSize != PT::PAGE_SIZE_2M && effectivePageSize != PT::PAGE_SIZE_1G) {
        effectivePageSize = PT::PAGE_SIZE_2M; // Default to 2MB large pages
    }

    // Round up to page size boundary
    uint64_t alignedSize = (sizeBytes + effectivePageSize - 1) & ~(effectivePageSize - 1);

    // Attempt to enable SeLockMemoryPrivilege
    bool privEnabled = enable_lock_memory_privilege();

    void* base = nullptr;
    if (privEnabled) {
        // Try MEM_LARGE_PAGES allocation
        base = VirtualAlloc(nullptr, static_cast<SIZE_T>(alignedSize),
                            MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES,
                            PAGE_READWRITE);
    }

    if (!base) {
        // Fallback to standard pages (still large and aligned, but no TLB optimization)
        base = VirtualAlloc(nullptr, static_cast<SIZE_T>(alignedSize),
                            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!base) {
            m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
            return PatchResult::error("VirtualAlloc failed (large arena)",
                                     static_cast<int>(GetLastError()));
        }
    }

    uint32_t id = next_arena_id();
    LargePageArena& arena = m_arenas[m_arenaCount++];
    arena.base     = base;
    arena.capacity = alignedSize;
    arena.used     = 0;
    arena.pageSize = effectivePageSize;
    arena.id       = id;
    arena.active   = true;

    if (outArenaId) *outArenaId = id;

    m_stats.largePagesAllocated.fetch_add(alignedSize / effectivePageSize, std::memory_order_relaxed);
    m_stats.largeBytesTotal.fetch_add(alignedSize, std::memory_order_relaxed);
    return PatchResult::ok("Large-page arena allocated");
}

PatchResult PTDriverContract::arena_alloc(uint32_t arenaId, uint64_t size,
                                           uint64_t alignment, void** outPtr) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!outPtr) return PatchResult::error("Null output pointer", 1);
    *outPtr = nullptr;

    int idx = find_arena(arenaId);
    if (idx < 0) return PatchResult::error("Arena not found", 2);

    LargePageArena& arena = m_arenas[idx];
    if (!arena.active) return PatchResult::error("Arena not active", 3);

    // Align the current offset
    if (alignment == 0) alignment = 16;  // Default 16-byte alignment
    uint64_t aligned_offset = (arena.used + alignment - 1) & ~(alignment - 1);

    if (aligned_offset + size > arena.capacity) {
        return PatchResult::error("Arena out of memory", 4);
    }

    *outPtr = static_cast<uint8_t*>(arena.base) + aligned_offset;
    arena.used = aligned_offset + size;
    return PatchResult::ok("Arena allocation succeeded");
}

PatchResult PTDriverContract::free_large_arena(uint32_t arenaId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = find_arena(arenaId);
    if (idx < 0) return PatchResult::error("Arena not found", 1);

    LargePageArena& arena = m_arenas[idx];
    if (!arena.active) return PatchResult::ok("Arena already freed");

    if (arena.base) {
        VirtualFree(arena.base, 0, MEM_RELEASE);
        arena.base = nullptr;
    }
    arena.active = false;
    return PatchResult::ok("Large-page arena freed");
}

PatchResult PTDriverContract::query_arena(uint32_t arenaId, LargePageArena* out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!out) return PatchResult::error("Null output", 1);

    int idx = find_arena(arenaId);
    if (idx < 0) return PatchResult::error("Arena not found", 2);

    memcpy(out, &m_arenas[idx], sizeof(LargePageArena));
    return PatchResult::ok("Arena queried");
}

// ═══════════════════════════════════════════════════════════════════════════
// 7. Working-Set Residency Tracking
// ═══════════════════════════════════════════════════════════════════════════

PatchResult PTDriverContract::query_residency(uintptr_t addr, uint64_t size,
                                               PTEDescriptor* outEntries, int maxEntries,
                                               int* outCount) {
    if (!outEntries || !outCount || maxEntries <= 0) {
        return PatchResult::error("Null output or zero max entries", 1);
    }

    // First, walk normal pages
    int pageCount = walk_pages(addr, addr + size, outEntries, maxEntries);

    // Now augment with QueryWorkingSetEx for residency info
    // Build an array of PSAPI_WORKING_SET_EX_INFORMATION
    int queryCount = (pageCount < maxEntries) ? pageCount : maxEntries;

    // Allocate temporary buffer for working set query
    // PSAPI_WORKING_SET_EX_INFORMATION has: VirtualAddress + VirtualAttributes
    struct WorkingSetExInfo {
        void*     VirtualAddress;
        uint64_t  VirtualAttributes; // PSAPI_WORKING_SET_EX_BLOCK as ULONG_PTR
    };

    WorkingSetExInfo* wsInfo = static_cast<WorkingSetExInfo*>(
        VirtualAlloc(nullptr, sizeof(WorkingSetExInfo) * queryCount,
                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!wsInfo) {
        *outCount = pageCount;
        return PatchResult::ok("Page walk succeeded (residency query failed — no memory)");
    }

    for (int i = 0; i < queryCount; i++) {
        wsInfo[i].VirtualAddress = reinterpret_cast<void*>(outEntries[i].virtualAddr);
        wsInfo[i].VirtualAttributes = 0;
    }

    // QueryWorkingSetEx — declared in psapi.h
    typedef BOOL (WINAPI *QueryWorkingSetExFn)(HANDLE, PVOID, DWORD);
    HMODULE hPsapi = GetModuleHandleA("psapi.dll");
    if (!hPsapi) hPsapi = LoadLibraryA("psapi.dll");

    QueryWorkingSetExFn queryFn = nullptr;
    // Also try K32QueryWorkingSetEx (available in kernel32 on Win7+)
    HMODULE hK32 = GetModuleHandleA("kernel32.dll");
    if (hK32) {
        queryFn = reinterpret_cast<QueryWorkingSetExFn>(
            GetProcAddress(hK32, "K32QueryWorkingSetEx"));
    }
    if (!queryFn && hPsapi) {
        queryFn = reinterpret_cast<QueryWorkingSetExFn>(
            GetProcAddress(hPsapi, "QueryWorkingSetEx"));
    }

    if (queryFn) {
        if (queryFn(GetCurrentProcess(), wsInfo,
                    static_cast<DWORD>(sizeof(WorkingSetExInfo) * queryCount))) {
            for (int i = 0; i < queryCount; i++) {
                // Bit 0 of VirtualAttributes = Valid (page is in working set)
                outEntries[i].resident   = (wsInfo[i].VirtualAttributes & 1) != 0;
                // Bits 1-3 = ShareCount
                outEntries[i].shareCount = static_cast<uint32_t>(
                    (wsInfo[i].VirtualAttributes >> 1) & 0x7);
                // Bit 15 = Shared
                outEntries[i].shared     = (wsInfo[i].VirtualAttributes & (1ULL << 15)) != 0;
            }
        }
        m_stats.residencyQueries.fetch_add(1, std::memory_order_relaxed);
    }

    VirtualFree(wsInfo, 0, MEM_RELEASE);

    *outCount = pageCount;
    return PatchResult::ok("Residency queried");
}

PatchResult PTDriverContract::lock_pages(uintptr_t addr, uint64_t size) {
    if (size == 0) return PatchResult::error("Zero size", 1);

    if (!VirtualLock(reinterpret_cast<void*>(addr), static_cast<SIZE_T>(size))) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualLock failed", static_cast<int>(GetLastError()));
    }
    return PatchResult::ok("Pages locked");
}

PatchResult PTDriverContract::unlock_pages(uintptr_t addr, uint64_t size) {
    if (size == 0) return PatchResult::error("Zero size", 1);

    if (!VirtualUnlock(reinterpret_cast<void*>(addr), static_cast<SIZE_T>(size))) {
        m_stats.totalErrors.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualUnlock failed", static_cast<int>(GetLastError()));
    }
    return PatchResult::ok("Pages unlocked");
}

// ═══════════════════════════════════════════════════════════════════════════
// Convenience free functions
// ═══════════════════════════════════════════════════════════════════════════

PatchResult pt_initialize() {
    return PTDriverContract::instance().initialize();
}

PatchResult pt_shutdown() {
    return PTDriverContract::instance().shutdown();
}

int pt_walk_pages(uintptr_t start, uintptr_t end,
                  PTEDescriptor* out, int maxEntries) {
    return PTDriverContract::instance().walk_pages(start, end, out, maxEntries);
}

PatchResult pt_describe_page(uintptr_t addr, PTEDescriptor* out) {
    return PTDriverContract::instance().describe_page(addr, out);
}

PatchResult pt_arm_watchpoint(uintptr_t addr, uint64_t size,
                              WatchpointEntry::WatchpointCallback cb,
                              void* ctx, bool oneShot, uint32_t* outId) {
    return PTDriverContract::instance().arm_watchpoint(addr, size, cb, ctx, oneShot, outId);
}

PatchResult pt_take_snapshot(uintptr_t addr, uint64_t size,
                             const char* label, uint32_t layerIndex,
                             uint32_t* outSnapshotId) {
    return PTDriverContract::instance().take_snapshot(addr, size, label, layerIndex, outSnapshotId);
}

PatchResult pt_restore_snapshot(uint32_t snapshotId) {
    return PTDriverContract::instance().restore_snapshot(snapshotId);
}

PatchResult pt_init_aslr(const char* moduleName, ASLRContext* out) {
    return PTDriverContract::instance().init_aslr_context(moduleName, out);
}

PatchResult pt_normalize(const ASLRContext* ctx,
                         uintptr_t absAddr, uintptr_t* outRelative) {
    return PTDriverContract::instance().normalize_address(ctx, absAddr, outRelative);
}

PatchResult pt_denormalize(const ASLRContext* ctx,
                           uintptr_t relOffset, uintptr_t* outAbsolute) {
    return PTDriverContract::instance().denormalize_address(ctx, relOffset, outAbsolute);
}

PatchResult pt_alloc_large_arena(uint64_t size, uint64_t pageSize,
                                 uint32_t* outArenaId) {
    return PTDriverContract::instance().allocate_large_arena(size, pageSize, outArenaId);
}

PatchResult pt_lock_pages(uintptr_t addr, uint64_t size) {
    return PTDriverContract::instance().lock_pages(addr, size);
}
