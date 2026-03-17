// model_memory_hotpatch.cpp — Memory-Layer Hotpatching (Layer 1) Implementation
// Direct RAM patching using OS protection APIs.
// Expanded from Qt version — batch patching, region cookies, undo journaling,
// CRC32 integrity, conflict detection, direct memory manipulation API.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "model_memory_hotpatch.hpp"
#include "license_enforcement.h"
#include <cstring>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static std::mutex             g_memPatchMutex;
static MemoryPatchStats       g_memPatchStats;

// ---------------------------------------------------------------------------
// RegionProtectCookie — Tracks VirtualProtect state for writable windows
// ---------------------------------------------------------------------------
struct RegionProtectCookie {
    DWORD   oldProtection{0};
    size_t  alignedStart{0};
    size_t  alignedSize{0};
};

// ---------------------------------------------------------------------------
// NamedPatchEntry — Named patch with full metadata (batch system)
// ---------------------------------------------------------------------------
struct NamedPatchEntry {
    std::string           name;
    size_t                offset{0};
    size_t                size{0};
    std::vector<uint8_t>  patchBytes;
    std::vector<uint8_t>  originalBytes;
    bool                  enabled{true};
    bool                  verifyChecksum{false};
    uint64_t              checksumBefore{0};
    uint64_t              checksumAfter{0};
    int                   priority{0};
    uint64_t              timesApplied{0};
};

// ---------------------------------------------------------------------------
// PatchConflict
// ---------------------------------------------------------------------------
struct PatchConflict {
    NamedPatchEntry existingPatch;
    NamedPatchEntry incomingPatch;
    std::string     reason;
};

// ---------------------------------------------------------------------------
// ModelMemoryHotpatchState — Full model-attached state (replaces QObject)
// ---------------------------------------------------------------------------
struct ModelMemoryHotpatchState {
    void*                                              modelPtr{nullptr};
    size_t                                             modelSize{0};
    bool                                               attached{false};
    std::vector<uint8_t>                               fullBackup;
    std::unordered_map<std::string, NamedPatchEntry>   patches;
    uint32_t                                           integrityHash{0};
    uint64_t                                           totalApplied{0};
    uint64_t                                           totalReverted{0};
    uint64_t                                           totalFailed{0};
    uint64_t                                           bytesModified{0};
    uint64_t                                           conflictsDetected{0};
    std::mutex                                         mtx;
};

static ModelMemoryHotpatchState g_modelState;

// ---------------------------------------------------------------------------
// apply_memory_patch
// ---------------------------------------------------------------------------
PatchResult apply_memory_patch(void* addr, size_t size, const void* data) {
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
        RawrXD::License::FeatureID::MemoryHotpatching, __FUNCTION__))
        return PatchResult::error("[LICENSE] Memory hotpatching requires Enterprise license", -1);

    if (!addr || !data || size == 0) {
        return PatchResult::error("Null address, data, or zero size", 1);
    }

    std::lock_guard<std::mutex> lock(g_memPatchMutex);

    // Attempt to make memory writable
    DWORD oldProtect = 0;
    if (!VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        g_memPatchStats.totalFailed.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect (make writable) failed", static_cast<int>(GetLastError()));
    }
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    // Write patch data
    std::memcpy(addr, data, size);

    // Restore original protection
    DWORD dummy = 0;
    VirtualProtect(addr, size, oldProtect, &dummy);
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    // Flush instruction cache (critical if patching code)
    FlushInstructionCache(GetCurrentProcess(), addr, size);

    g_memPatchStats.totalApplied.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Memory patch applied");
}

// ---------------------------------------------------------------------------
// revert_memory_patch
// ---------------------------------------------------------------------------
PatchResult revert_memory_patch(MemoryPatchEntry* entry) {
    if (!entry) {
        return PatchResult::error("Null entry", 1);
    }
    if (!entry->applied) {
        return PatchResult::error("Patch not currently applied", 2);
    }
    if (entry->originalSize == 0) {
        return PatchResult::error("No backup data to restore", 3);
    }

    std::lock_guard<std::mutex> lock(g_memPatchMutex);

    void* addr = reinterpret_cast<void*>(entry->targetAddr);
    DWORD oldProtect = 0;
    if (!VirtualProtect(addr, entry->originalSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        g_memPatchStats.totalFailed.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect (revert) failed", static_cast<int>(GetLastError()));
    }
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    std::memcpy(addr, entry->originalBytes, entry->originalSize);

    DWORD dummy = 0;
    VirtualProtect(addr, entry->originalSize, oldProtect, &dummy);
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    FlushInstructionCache(GetCurrentProcess(), addr, entry->originalSize);

    entry->applied = false;
    g_memPatchStats.totalReverted.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Memory patch reverted");
}

// ---------------------------------------------------------------------------
// apply_memory_patch_tracked
// ---------------------------------------------------------------------------
PatchResult apply_memory_patch_tracked(MemoryPatchEntry* entry) {
    if (!entry) {
        return PatchResult::error("Null entry", 1);
    }
    if (entry->patchSize == 0 || !entry->patchData) {
        return PatchResult::error("Invalid patch entry (zero size or null data)", 2);
    }
    if (entry->patchSize > sizeof(entry->originalBytes)) {
        return PatchResult::error("Patch size exceeds backup buffer (64 bytes max)", 3);
    }

    std::lock_guard<std::mutex> lock(g_memPatchMutex);

    void* addr = reinterpret_cast<void*>(entry->targetAddr);

    // Backup original bytes
    DWORD oldProtect = 0;
    if (!VirtualProtect(addr, entry->patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        g_memPatchStats.totalFailed.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect (backup read) failed", static_cast<int>(GetLastError()));
    }
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    std::memcpy(entry->originalBytes, addr, entry->patchSize);
    entry->originalSize = entry->patchSize;

    // Write new data
    std::memcpy(addr, entry->patchData, entry->patchSize);

    // Restore protection
    DWORD dummy = 0;
    VirtualProtect(addr, entry->patchSize, oldProtect, &dummy);
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    FlushInstructionCache(GetCurrentProcess(), addr, entry->patchSize);

    entry->applied = true;
    g_memPatchStats.totalApplied.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Tracked memory patch applied");
}

// ---------------------------------------------------------------------------
// query_memory_protection
// ---------------------------------------------------------------------------
PatchResult query_memory_protection(void* addr, size_t size, DWORD* outProtect) {
    if (!addr || !outProtect) {
        return PatchResult::error("Null address or output pointer", 1);
    }

    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T result = VirtualQuery(addr, &mbi, sizeof(mbi));
    if (result == 0) {
        return PatchResult::error("VirtualQuery failed", static_cast<int>(GetLastError()));
    }

    *outProtect = mbi.Protect;
    return PatchResult::ok("Protection queried");
}

// ---------------------------------------------------------------------------
// get_memory_patch_stats / reset_memory_patch_stats
// ---------------------------------------------------------------------------
const MemoryPatchStats& get_memory_patch_stats() {
    return g_memPatchStats;
}

void reset_memory_patch_stats() {
    g_memPatchStats.totalApplied.store(0, std::memory_order_relaxed);
    g_memPatchStats.totalReverted.store(0, std::memory_order_relaxed);
    g_memPatchStats.totalFailed.store(0, std::memory_order_relaxed);
    g_memPatchStats.protectionChanges.store(0, std::memory_order_relaxed);
}

// ===========================================================================
// System page size helper
// ===========================================================================
static size_t getSystemPageSize() {
    static size_t pageSize = 0;
    if (pageSize == 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        pageSize = si.dwPageSize;
    }
    return pageSize;
}

// ===========================================================================
// beginWritableWindow / endWritableWindow — Region cookies
// ===========================================================================
PatchResult begin_writable_window(void* modelPtr, size_t modelSize,
                                  size_t offset, size_t size, void*& cookie) {
    if (!modelPtr || offset + size > modelSize) {
        return PatchResult::error("Invalid offset or size for writable window", 1001);
    }

    size_t pageSize = getSystemPageSize();
    char*  startAddr    = static_cast<char*>(modelPtr) + offset;
    size_t alignedStart = reinterpret_cast<size_t>(startAddr) & ~(pageSize - 1);
    size_t endAddr      = reinterpret_cast<size_t>(startAddr) + size;
    size_t alignedEnd   = (endAddr + pageSize - 1) & ~(pageSize - 1);
    size_t alignedSize  = alignedEnd - alignedStart;

    auto* rc = new RegionProtectCookie();
    rc->alignedStart = alignedStart;
    rc->alignedSize  = alignedSize;

    if (!VirtualProtect(reinterpret_cast<void*>(alignedStart), alignedSize,
                        PAGE_EXECUTE_READWRITE, &rc->oldProtection)) {
        int err = static_cast<int>(GetLastError());
        delete rc;
        return PatchResult::error("VirtualProtect (open window) failed", err);
    }

    cookie = rc;
    return PatchResult::ok("Writable window opened");
}

PatchResult end_writable_window(void* cookie) {
    if (!cookie) return PatchResult::error("Invalid cookie", 1004);

    auto* rc = static_cast<RegionProtectCookie*>(cookie);
    DWORD dummy = 0;
    bool ok = VirtualProtect(reinterpret_cast<void*>(rc->alignedStart),
                             rc->alignedSize, rc->oldProtection, &dummy);
    delete rc;
    if (!ok) {
        return PatchResult::error("VirtualProtect (restore) failed", static_cast<int>(GetLastError()));
    }
    return PatchResult::ok("Protection restored");
}

// ===========================================================================
// safe_memory_write — Protected write with region cookie pattern
// ===========================================================================
PatchResult safe_memory_write(void* modelPtr, size_t modelSize,
                              size_t offset, const void* data, size_t dataSize) {
    if (!modelPtr || !data || dataSize == 0) {
        return PatchResult::error("Invalid args for safe_memory_write", 2001);
    }
    if (offset + dataSize > modelSize) {
        return PatchResult::error("Out of bounds", 2002);
    }

    void* cookie = nullptr;
    PatchResult openRes = begin_writable_window(modelPtr, modelSize, offset, dataSize, cookie);
    if (!openRes.success) return openRes;

    std::memcpy(static_cast<char*>(modelPtr) + offset, data, dataSize);

    PatchResult closeRes = end_writable_window(cookie);
    if (!closeRes.success) {
        std::cerr << "[MemHotpatch] WARNING: Write succeeded but protection restore failed\n";
    }

    FlushInstructionCache(GetCurrentProcess(),
                          static_cast<char*>(modelPtr) + offset, dataSize);
    return PatchResult::ok("Safe write completed");
}

// ===========================================================================
// CRC32 integrity check
// ===========================================================================
uint32_t calculate_crc32(const void* ptr, size_t offset, size_t size, size_t maxSize) {
    if (!ptr || offset + size > maxSize) return 0;

    static constexpr uint32_t CRC32_POLY = 0xEDB88320u;
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t* data = static_cast<const uint8_t*>(ptr) + offset;

    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc & 1) ? ((crc >> 1) ^ CRC32_POLY) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

// ===========================================================================
// FNV-1a 64-bit checksum (for patch verification)
// ===========================================================================
uint64_t calculate_checksum64(const void* ptr, size_t offset, size_t size, size_t maxSize) {
    if (!ptr || offset + size > maxSize) return 0;

    const char* data = static_cast<const char*>(ptr) + offset;
    uint64_t hash  = 0xcbf29ce484222325ULL;
    const uint64_t prime = 0x100000001b3ULL;

    for (size_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= prime;
    }
    return hash;
}

// ===========================================================================
// Model attach / detach
// ===========================================================================
PatchResult model_hotpatch_attach(void* modelPtr, size_t modelSize) {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (g_modelState.attached) {
        return PatchResult::error("Already attached — detach first", 1);
    }
    if (!modelPtr || modelSize == 0) {
        return PatchResult::error("Invalid model pointer or size", 2);
    }

    g_modelState.modelPtr    = modelPtr;
    g_modelState.modelSize   = modelSize;
    g_modelState.attached    = true;
    g_modelState.totalApplied  = 0;
    g_modelState.totalReverted = 0;
    g_modelState.totalFailed   = 0;
    g_modelState.bytesModified = 0;
    g_modelState.patches.clear();
    g_modelState.fullBackup.clear();

    std::cout << "[MemHotpatch] Attached to model at " << modelPtr
              << " (" << modelSize << " bytes)\n";
    return PatchResult::ok("Model attached");
}

PatchResult model_hotpatch_detach() {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!g_modelState.attached) {
        return PatchResult::error("Not attached", 1);
    }

    g_modelState.modelPtr  = nullptr;
    g_modelState.modelSize = 0;
    g_modelState.attached  = false;
    g_modelState.patches.clear();
    g_modelState.fullBackup.clear();

    std::cout << "[MemHotpatch] Detached from model\n";
    return PatchResult::ok("Model detached");
}

// ===========================================================================
// Full model backup / restore
// ===========================================================================
PatchResult model_create_backup() {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!g_modelState.attached) {
        return PatchResult::error("Not attached", 5001);
    }

    g_modelState.fullBackup.resize(g_modelState.modelSize);
    std::memcpy(g_modelState.fullBackup.data(), g_modelState.modelPtr,
                g_modelState.modelSize);

    std::cout << "[MemHotpatch] Full backup created (" << g_modelState.modelSize << " bytes)\n";
    return PatchResult::ok("Full model backup created");
}

PatchResult model_restore_backup() {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!g_modelState.attached || g_modelState.fullBackup.empty()) {
        return PatchResult::error("Not attached or no backup exists", 6001);
    }
    if (g_modelState.fullBackup.size() != g_modelState.modelSize) {
        return PatchResult::error("Backup size mismatch — aborting restore", 6002);
    }

    PatchResult res = safe_memory_write(g_modelState.modelPtr, g_modelState.modelSize,
                                        0, g_modelState.fullBackup.data(),
                                        g_modelState.fullBackup.size());
    if (res.success) {
        g_modelState.totalApplied  = 0;
        g_modelState.totalReverted = 0;
        g_modelState.bytesModified = 0;
        std::cout << "[MemHotpatch] Full backup restored\n";
    }
    return res;
}

// ===========================================================================
// Named patch management — add / remove / apply / revert / conflict check
// ===========================================================================
static bool check_patch_conflict(const NamedPatchEntry& newPatch,
                                 PatchConflict& conflict) {
    for (const auto& [name, existing] : g_modelState.patches) {
        if (existing.name == newPatch.name) continue;

        size_t eStart = existing.offset;
        size_t eEnd   = existing.offset + existing.size - 1;
        size_t nStart = newPatch.offset;
        size_t nEnd   = newPatch.offset + newPatch.size - 1;

        if (nStart <= eEnd && nEnd >= eStart) {
            if (newPatch.priority <= existing.priority) {
                conflict.existingPatch = existing;
                conflict.incomingPatch = newPatch;
                conflict.reason = "Memory overlap detected — incoming priority too low";
                return true;
            }
        }
    }
    return false;
}

PatchResult model_add_named_patch(const char* name, size_t offset, size_t size,
                                  const void* patchData, int priority) {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!name || !patchData || size == 0) {
        return PatchResult::error("Invalid patch args", 3000);
    }

    std::string patchName(name);
    if (g_modelState.patches.count(patchName)) {
        return PatchResult::error("Patch name already exists", 3001);
    }

    NamedPatchEntry entry;
    entry.name     = patchName;
    entry.offset   = offset;
    entry.size     = size;
    entry.priority = priority;
    entry.patchBytes.resize(size);
    std::memcpy(entry.patchBytes.data(), patchData, size);

    // Back up original bytes
    if (g_modelState.attached && offset + size <= g_modelState.modelSize) {
        entry.originalBytes.resize(size);
        std::memcpy(entry.originalBytes.data(),
                    static_cast<char*>(g_modelState.modelPtr) + offset, size);
    }

    // Conflict check
    PatchConflict conflict;
    if (check_patch_conflict(entry, conflict)) {
        g_modelState.conflictsDetected++;
        return PatchResult::error("Patch conflict detected — overlap with existing patch", 3003);
    }

    g_modelState.patches[patchName] = std::move(entry);
    return PatchResult::ok("Named patch added");
}

PatchResult model_apply_named_patch(const char* name) {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!g_modelState.attached) {
        return PatchResult::error("Not attached", 3010);
    }

    auto it = g_modelState.patches.find(std::string(name));
    if (it == g_modelState.patches.end()) {
        return PatchResult::error("Patch not found", 3011);
    }

    NamedPatchEntry& patch = it->second;
    if (!patch.enabled) {
        return PatchResult::ok("Patch skipped (disabled)");
    }

    // Checksum verification before apply
    if (patch.verifyChecksum && patch.checksumBefore != 0) {
        uint64_t current = calculate_checksum64(g_modelState.modelPtr,
                                                 patch.offset, patch.size,
                                                 g_modelState.modelSize);
        if (current != patch.checksumBefore) {
            g_modelState.totalFailed++;
            return PatchResult::error("Checksum mismatch — model region changed", 3012);
        }
    }

    PatchResult res = safe_memory_write(g_modelState.modelPtr, g_modelState.modelSize,
                                        patch.offset, patch.patchBytes.data(),
                                        patch.patchBytes.size());
    if (res.success) {
        patch.timesApplied++;
        g_modelState.totalApplied++;
        g_modelState.bytesModified += patch.size;
    } else {
        g_modelState.totalFailed++;
    }
    return res;
}

PatchResult model_revert_named_patch(const char* name) {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!g_modelState.attached) {
        return PatchResult::error("Not attached", 4010);
    }

    auto it = g_modelState.patches.find(std::string(name));
    if (it == g_modelState.patches.end()) {
        return PatchResult::error("Patch not found", 4011);
    }

    NamedPatchEntry& patch = it->second;
    if (patch.originalBytes.empty()) {
        return PatchResult::error("No original bytes to restore", 4012);
    }

    PatchResult res = safe_memory_write(g_modelState.modelPtr, g_modelState.modelSize,
                                        patch.offset, patch.originalBytes.data(),
                                        patch.originalBytes.size());
    if (res.success) {
        g_modelState.totalReverted++;
    }
    return res;
}

PatchResult model_remove_named_patch(const char* name) {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    auto it = g_modelState.patches.find(std::string(name));
    if (it == g_modelState.patches.end()) {
        return PatchResult::error("Patch not found", 3020);
    }
    if (it->second.timesApplied > 0 && !it->second.originalBytes.empty()) {
        std::cerr << "[MemHotpatch] Warning: removing applied patch '" << name << "'\n";
    }
    g_modelState.patches.erase(it);
    return PatchResult::ok("Patch removed");
}

// ===========================================================================
// Batch operations
// ===========================================================================
PatchResult model_apply_all_patches() {
    // Collect enabled patches sorted by offset
    std::map<size_t, std::string> sorted;
    {
        std::lock_guard<std::mutex> lock(g_modelState.mtx);
        for (const auto& [name, patch] : g_modelState.patches) {
            if (patch.enabled) sorted[patch.offset] = name;
        }
    }

    bool overallOk = true;
    for (const auto& [offset, name] : sorted) {
        PatchResult res = model_apply_named_patch(name.c_str());
        if (!res.success) {
            overallOk = false;
            std::cerr << "[MemHotpatch] Batch apply failed for '" << name
                      << "': " << res.detail << "\n";
        }
    }
    return overallOk ? PatchResult::ok("All patches applied")
                     : PatchResult::error("Some patches failed during batch apply", -1);
}

PatchResult model_revert_all_patches() {
    std::vector<std::string> names;
    {
        std::lock_guard<std::mutex> lock(g_modelState.mtx);
        for (const auto& [name, _] : g_modelState.patches) {
            names.push_back(name);
        }
    }

    bool overallOk = true;
    for (const auto& name : names) {
        PatchResult res = model_revert_named_patch(name.c_str());
        if (!res.success) overallOk = false;
    }
    return overallOk ? PatchResult::ok("All patches reverted")
                     : PatchResult::error("Some patches failed during batch revert", -1);
}

// ===========================================================================
// GGUF model integrity verification
// ===========================================================================
PatchResult model_verify_integrity() {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!g_modelState.attached || !g_modelState.modelPtr) {
        return PatchResult::error("Not attached", 7001);
    }

    // Verify GGUF signature
    const char* sig = static_cast<const char*>(g_modelState.modelPtr);
    if (g_modelState.modelSize < 4 || std::strncmp(sig, "GGUF", 4) != 0) {
        return PatchResult::error("Invalid GGUF signature", 7002);
    }

    // CRC32 of first 64KB
    size_t checkSize = (std::min)(g_modelState.modelSize, static_cast<size_t>(65536));
    uint32_t crc = calculate_crc32(g_modelState.modelPtr, 0, checkSize, g_modelState.modelSize);

    if (g_modelState.integrityHash != 0 && g_modelState.integrityHash != crc) {
        return PatchResult::error("Integrity hash mismatch", 7003);
    }

    g_modelState.integrityHash = crc;
    return PatchResult::ok("Model integrity verified");
}

// ===========================================================================
// Direct memory manipulation API (mirrors Qt version)
// ===========================================================================
void* model_get_direct_pointer(size_t offset) {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);
    if (!g_modelState.attached || offset >= g_modelState.modelSize) return nullptr;
    return static_cast<char*>(g_modelState.modelPtr) + offset;
}

PatchResult model_direct_read(size_t offset, size_t size, void* dest) {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!g_modelState.attached || !dest) {
        return PatchResult::error("Not attached or null dest", 6001);
    }
    if (offset + size > g_modelState.modelSize) {
        return PatchResult::error("Read out of bounds", 6002);
    }

    std::memcpy(dest, static_cast<char*>(g_modelState.modelPtr) + offset, size);
    return PatchResult::ok("Direct read completed");
}

PatchResult model_direct_write(size_t offset, const void* data, size_t size) {
    if (!g_modelState.attached) {
        return PatchResult::error("Not attached", 6003);
    }
    return safe_memory_write(g_modelState.modelPtr, g_modelState.modelSize,
                             offset, data, size);
}

PatchResult model_direct_fill(size_t offset, size_t size, uint8_t value) {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!g_modelState.attached) return PatchResult::error("Not attached", 6005);
    if (offset + size > g_modelState.modelSize) return PatchResult::error("Fill out of bounds", 6006);

    void* cookie = nullptr;
    PatchResult openRes = begin_writable_window(g_modelState.modelPtr,
                                                g_modelState.modelSize,
                                                offset, size, cookie);
    if (!openRes.success) return openRes;

    std::memset(static_cast<char*>(g_modelState.modelPtr) + offset, value, size);
    g_modelState.bytesModified += size;

    end_writable_window(cookie);
    return PatchResult::ok("Fill completed");
}

PatchResult model_direct_copy(size_t srcOffset, size_t dstOffset, size_t size) {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!g_modelState.attached) return PatchResult::error("Not attached", 6007);
    if (srcOffset + size > g_modelState.modelSize ||
        dstOffset + size > g_modelState.modelSize) {
        return PatchResult::error("Copy out of bounds", 6008);
    }

    void* cookie = nullptr;
    PatchResult openRes = begin_writable_window(g_modelState.modelPtr,
                                                g_modelState.modelSize,
                                                dstOffset, size, cookie);
    if (!openRes.success) return openRes;

    std::memmove(static_cast<char*>(g_modelState.modelPtr) + dstOffset,
                 static_cast<char*>(g_modelState.modelPtr) + srcOffset, size);
    g_modelState.bytesModified += size;

    end_writable_window(cookie);
    return PatchResult::ok("Copy completed");
}

int64_t model_direct_search(size_t startOffset, const void* pattern, size_t patternLen) {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!g_modelState.attached || !pattern || patternLen == 0) return -1;
    if (startOffset >= g_modelState.modelSize) return -1;

    const char* base = static_cast<const char*>(g_modelState.modelPtr);
    const char* haystack = base + startOffset;
    size_t haystackLen   = g_modelState.modelSize - startOffset;

    const char* found = std::search(haystack, haystack + haystackLen,
                                    static_cast<const char*>(pattern),
                                    static_cast<const char*>(pattern) + patternLen);
    if (found != haystack + haystackLen) {
        return static_cast<int64_t>(found - base);
    }
    return -1;
}

PatchResult model_direct_swap(size_t offset1, size_t offset2, size_t size) {
    std::lock_guard<std::mutex> lock(g_modelState.mtx);

    if (!g_modelState.attached) return PatchResult::error("Not attached", 6009);
    if (offset1 + size > g_modelState.modelSize ||
        offset2 + size > g_modelState.modelSize) {
        return PatchResult::error("Swap out of bounds", 6010);
    }

    std::vector<uint8_t> temp(size);
    std::memcpy(temp.data(), static_cast<char*>(g_modelState.modelPtr) + offset1, size);

    void* cookie1 = nullptr;
    begin_writable_window(g_modelState.modelPtr, g_modelState.modelSize, offset1, size, cookie1);
    std::memcpy(static_cast<char*>(g_modelState.modelPtr) + offset1,
                static_cast<char*>(g_modelState.modelPtr) + offset2, size);
    end_writable_window(cookie1);

    void* cookie2 = nullptr;
    begin_writable_window(g_modelState.modelPtr, g_modelState.modelSize, offset2, size, cookie2);
    std::memcpy(static_cast<char*>(g_modelState.modelPtr) + offset2, temp.data(), size);
    end_writable_window(cookie2);

    g_modelState.bytesModified += 2 * size;
    return PatchResult::ok("Swap completed");
}
