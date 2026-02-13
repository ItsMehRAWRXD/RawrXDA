// ============================================================================
// shadow_page_detour.hpp — Shadow-Page Detour Hotpatching System
// ============================================================================
//
// PURPOSE:
//   Replaces standard PE loading with a Shadow-Page Detour system for live
//   binary hotpatching. Enables the SelfRepairLoop to modify Camellia-256
//   or DirectML kernels while they are executing, without process restart.
//
//   The AI Agent identifies a bug, re-assembles a fix in MASM via the
//   AgenticAssembler, and this system swaps the opcode stream atomically
//   through the RawrXD_Hotpatch_Kernel.asm MASM kernel.
//
// Architecture:
//   - Shadow pages: RWX VirtualAlloc'd code caves for patched functions
//   - Atomic 14-byte absolute jump prologue rewrite (FF 25)
//   - CRC32 pre/post verification guard
//   - Rollback via prologue backup/restore
//   - RFC 3713 sentinel round-trip test for Camellia patches
//
// Pattern:      PatchResult / CamelliaResult (no exceptions)
// Threading:    std::mutex + std::lock_guard (no recursive locks)
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_SHADOW_PAGE_DETOUR_HPP
#define RAWRXD_SHADOW_PAGE_DETOUR_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "model_memory_hotpatch.hpp"
#include "camellia256_bridge.hpp"
#include "sentinel_watchdog.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>

// ============================================================================
// MASM Kernel extern declarations (RawrXD_Hotpatch_Kernel.asm exports)
// ============================================================================

extern "C" {
    // Atomic redirect: rewrite function prologue with 14-byte absolute jump
    // Uses corrected 8+6 split atomic write (lock cmpxchg8b + lock cmpxchg).
    // CALLER is responsible for VirtualProtect (RWX) before calling.
    int asm_hotpatch_atomic_swap(void* targetFn, void* newFn);

    // Install trampoline: copy original 14 bytes + append jump-back (28B total).
    // Must be called BEFORE asm_hotpatch_atomic_swap while original bytes intact.
    // trampolineBuffer must be RWX (allocated via asm_hotpatch_alloc_shadow).
    int asm_hotpatch_install_trampoline(void* originalFn, void* trampolineBuffer);

    // Allocate RWX shadow page for new code (0 = default 64KB)
    void* asm_hotpatch_alloc_shadow(size_t size);

    // Release a shadow page
    int asm_hotpatch_free_shadow(void* base, size_t size);

    // Backup function prologue (16 bytes) into snapshot slot
    int asm_hotpatch_backup_prologue(void* funcAddr, uint32_t slotIndex);

    // Restore backed-up prologue (rollback to known-good state)
    int asm_hotpatch_restore_prologue(uint32_t slotIndex);

    // CRC32 verify function prologue matches expected checksum
    int asm_hotpatch_verify_prologue(void* funcAddr, uint32_t expectedCRC);

    // Flush instruction cache for patched region
    int asm_hotpatch_flush_icache(void* base, size_t size);

    // Read hotpatch statistics (64 bytes = 8 QWORDs)
    int asm_hotpatch_get_stats(void* statsBuffer64);

    // Snapshot kernel exports (RawrXD_Snapshot.asm)
    int asm_snapshot_capture(void* funcAddr, uint32_t snapshotId, size_t captureSize);
    int asm_snapshot_restore(uint32_t snapshotId);
    int asm_snapshot_verify(uint32_t snapshotId, uint32_t expectedCRC);
    int asm_snapshot_discard(uint32_t snapshotId);
    int asm_snapshot_get_stats(void* statsBuffer48);
}

// ============================================================================
// Hotpatch Statistics (mirrors ASM layout: 8 QWORDs)
// ============================================================================

struct HotpatchKernelStats {
    uint64_t swapsApplied;
    uint64_t swapsRolledBack;
    uint64_t swapsFailed;
    uint64_t shadowPagesAllocated;
    uint64_t shadowPagesFreed;
    uint64_t icacheFlushes;
    uint64_t crcChecks;
    uint64_t crcMismatches;
};

// ============================================================================
// Snapshot Statistics (mirrors ASM layout: 6 QWORDs)
// ============================================================================

struct SnapshotStats {
    uint64_t snapshotsCaptured;
    uint64_t snapshotsRestored;
    uint64_t snapshotsDiscarded;
    uint64_t verifyPassed;
    uint64_t verifyFailed;
    uint64_t totalBytesStored;
};

// ============================================================================
// ShadowPage — Tracks an allocated RWX code page
// ============================================================================

struct ShadowPage {
    void*       base;               // VirtualAlloc'd RWX base
    size_t      capacity;           // Total allocated size
    size_t      used;               // Current watermark
    uint32_t    pageId;             // Unique page identifier
    bool        sealed;             // If true, no more allocations

    // Carve out `bytes` aligned to `alignment` from this page
    uintptr_t allocate(size_t bytes, size_t alignment = 16) {
        size_t aligned = (used + alignment - 1) & ~(alignment - 1);
        if (aligned + bytes > capacity || sealed) return 0;
        uintptr_t result = reinterpret_cast<uintptr_t>(base) + aligned;
        used = aligned + bytes;
        return result;
    }
};

// ============================================================================
// DetourEntry — Tracks a single active function detour
// ============================================================================

struct DetourEntry {
    char                name[128];          // Human-readable function name
    void*               originalAddr;       // Original function entry point
    void*               patchedAddr;        // New implementation in shadow page
    void*               trampolineAddr;     // Trampoline to call original (28 bytes)
    uint32_t            backupSlot;         // Snapshot slot for prologue backup
    uint32_t            snapshotId;         // Full snapshot ID (for rollback)
    uint32_t            expectedCRC;        // CRC32 of original prologue
    uint64_t            patchCount;         // Number of times detoured
    uint64_t            timestamp;          // Last patch timestamp (GetTickCount64)
    bool                isActive;           // Is the detour currently live?
    bool                isVerified;         // Last CRC verification passed?

    static DetourEntry make(const char* fname, void* addr) {
        DetourEntry e{};
        if (fname) {
            size_t len = 0;
            while (fname[len] && len < 127) { e.name[len] = fname[len]; ++len; }
            e.name[len] = '\0';
        }
        e.originalAddr = addr;
        e.trampolineAddr = nullptr;
        return e;
    }
};

// ============================================================================
// AgenticAssembler — Compiles MASM source to executable opcodes
// ============================================================================
// The SelfRepairLoop uses this to compile LLM-generated MASM patches
// into executable buffers on shadow pages.
// ============================================================================

struct AssembledBuffer {
    void*       data;                   // Raw assembled bytes
    size_t      size;                   // Size in bytes
    uintptr_t   executableAddr;         // Address in RWX shadow page

    void* executable_addr() const { return reinterpret_cast<void*>(executableAddr); }
};

class AgenticAssembler {
public:
    // Compile MASM source to executable buffer on a shadow page.
    // Cathedral Code-Style enforcement validates structure before compilation.
    static AssembledBuffer Compile(const std::string& masmSource);

    // Validate MASM source against Cathedral style rules before compilation.
    static bool ValidateStyle(const std::string& masmSource);

    // Get last compilation error message.
    static const char* GetLastError();

private:
    static char s_lastError[512];
};

// ============================================================================
// TestRunner — RFC 3713 Sentinel Round-Trip Verification
// ============================================================================
// Before committing a Camellia patch, the SelfRepairLoop runs the patched
// code against Appendix A known-answer vectors in a sandbox thread.
// ============================================================================

class TestRunner {
public:
    // Verify Camellia-256 test vectors against the provided function buffer.
    // Returns true only if ALL vectors produce the expected ciphertext.
    static bool VerifyVectors(void* patchedEncryptFn);

    // Verify a single encrypt/decrypt round-trip with Appendix A data.
    // Key:  0123456789ABCDEFFEDCBA98765432100011223344556677 8899AABBCCDDEEFF
    // PT:   0123456789ABCDEFFEDCBA9876543210
    // CT:   9ACC237DFF16D76C20EF7C919E3A7509
    static bool VerifyAppendixA(void* encryptFn, void* decryptFn);

    // Run the self-test in a protected thread with a timeout.
    // If the patch causes a crash or infinite loop, the thread is terminated.
    static bool VerifyVectorsProtected(void* patchedEncryptFn,
                                        uint32_t timeoutMs = 5000);
};

// ============================================================================
// SelfRepairLoop — Agentic Self-Repair Orchestrator
// ============================================================================
// Coordinates the AI's reasoning with the MASM hotpatcher to detect bugs,
// compile fixes, verify RFC 3713 vectors, and atomically swap.
// ============================================================================

class SelfRepairLoop {
public:
    static SelfRepairLoop& instance();

    // ---- Lifecycle ----
    PatchResult initialize();
    bool isInitialized() const;
    PatchResult shutdown();

    // ---- Core Hotpatch API ----
    // The main entry point: verify a fix against test vectors and atomically
    // swap the original function to the patched implementation.
    RawrXD::Crypto::CamelliaResult VerifyAndPatch(
        void* originalFn,
        const std::string& newAsmSource);

    // Register a function for detour capability.
    PatchResult registerDetour(const char* name, void* funcAddr);

    // Apply a precompiled binary patch to a registered detour.
    PatchResult applyBinaryPatch(const char* name,
                                  const uint8_t* newCode, size_t codeSize);

    // Rollback a detour to its original ("Known Good") state.
    PatchResult rollbackDetour(const char* name);

    // Rollback ALL active detours.
    PatchResult rollbackAll();

    // ---- S-Box Evolution ----
    // Swap Camellia-256 S-Boxes with a hardened variant.
    // The new S-Boxes are verified against RFC 3713 vectors before commit.
    RawrXD::Crypto::CamelliaResult evolveSBoxes(
        const uint8_t* newSBox1_256,
        const uint8_t* newSBox2_256,
        const uint8_t* newSBox3_256,
        const uint8_t* newSBox4_256);

    // ---- Verification ----
    // Verify all active detours' CRC integrity.
    PatchResult verifyAllDetours() const;

    // Run RFC 3713 self-test against the live Camellia engine.
    bool runCamelliaSelfTest() const;

    // ---- Statistics ----
    HotpatchKernelStats getKernelStats() const;
    SnapshotStats getSnapshotStats() const;
    size_t getActiveDetourCount() const;
    const std::vector<DetourEntry>& getDetours() const;

    // ---- Callbacks ----
    typedef void (*DetourCallback)(const DetourEntry* entry, bool success, void* userData);
    void registerCallback(DetourCallback cb, void* userData);

private:
    SelfRepairLoop();
    ~SelfRepairLoop();
    SelfRepairLoop(const SelfRepairLoop&) = delete;
    SelfRepairLoop& operator=(const SelfRepairLoop&) = delete;

    // Find a detour by name. Returns index or -1.
    int findDetour(const char* name) const;

    // Allocate executable bytes on a shadow page and copy code into it.
    uintptr_t copyToShadowPage(const uint8_t* code, size_t size);

    // Compute CRC32 of the first 16 bytes of a function.
    uint32_t computePrologueCRC(void* funcAddr) const;

    // State
    mutable std::mutex              m_mutex;
    bool                            m_initialized;
    std::vector<DetourEntry>        m_detours;
    std::vector<ShadowPage>         m_shadowPages;
    std::atomic<uint32_t>           m_nextBackupSlot{0};
    std::atomic<uint32_t>           m_nextSnapshotId{0};
    std::atomic<uint32_t>           m_nextPageId{0};

    // Global encryption mutex — pauses all crypto ops during patch
    static std::mutex               s_camelliaMtx;

    // Callbacks
    struct CBEntry { DetourCallback fn; void* userData; };
    std::vector<CBEntry>            m_callbacks;
};

#endif // RAWRXD_SHADOW_PAGE_DETOUR_HPP
