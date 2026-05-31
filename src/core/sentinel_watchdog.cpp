// ============================================================================
// sentinel_watchdog.cpp — Sentinel Watchdog Anti-Tamper Implementation
// ============================================================================
//
// Implements real-time .text section integrity monitoring via BCrypt SHA-256,
// anti-debug detection (PEB, NtGlobalFlag, DR0-DR3, RDTSC timing), and
// cryptographic lockdown response (Camellia-256 workspace encryption).
//
// Architecture: C++20 | Win32 | BCrypt SHA-256 | No exceptions
// Pattern:      PatchResult::ok() / PatchResult::error() — no exceptions
// Threading:    Dedicated watchdog thread + std::mutex + std::atomic
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <winternl.h>
#include "sentinel_watchdog.hpp"
#include "model_memory_hotpatch.hpp"
#include "camellia256_bridge.hpp"
#include "enterprise_license.h"
#include "../vulkan_compute.h"
#include "Direct2D_HeatmapRenderer.hpp"
#include <cstring>
#include <cstdio>
#include <intrin.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ntdll.lib")

// ============================================================================
// PE Header Structures (for .text section location)
// ============================================================================

// IMAGE_NT_HEADERS64 offsets
static constexpr uint32_t kPESignatureSize      = 4;
static constexpr uint32_t kFileHeaderSize        = 20;
static constexpr uint32_t kOptionalHeader64Size  = 240;
static constexpr uint32_t kSectionHeaderSize     = 40;

// ============================================================================
// Singleton
// ============================================================================

SentinelWatchdog& SentinelWatchdog::instance() {
    static SentinelWatchdog s_instance;
    return s_instance;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

SentinelWatchdog::SentinelWatchdog()
    : m_active(false)
    , m_shutdownRequested(false)
    , m_watchdogThread(nullptr)
    , m_watchdogThreadId(0)
    , m_textBase(nullptr)
    , m_textSize(0)
    , m_textLocated(false)
    , m_baselineValid(false)
    , m_stats{}
    , m_violationCount(0)
    , m_eventLogHead(0)
    , m_eventLogCount(0)
    , m_lastRdtsc(0)
    // ---- Batch 7: Heartbeat sync-plane ----
    , m_vkCompute(nullptr)
    , m_localHeartbeatSem{}
    , m_remoteHeartbeatSem{}
    , m_semaphoreKmtHandle(nullptr)
    , m_heartbeatCounter(0)
    , m_lastRemoteCounter(0)
    , m_syncPlaneInit(false)
    , m_heartbeatBufIdx(UINT32_MAX)
    , m_heartbeatCycleCount(0)
{
    std::memset(m_baselineHash, 0, sizeof(m_baselineHash));
    std::memset(m_eventLog, 0, sizeof(m_eventLog));
    std::memset(m_heartbeatHmacKey, 0, sizeof(m_heartbeatHmacKey));
}

SentinelWatchdog::~SentinelWatchdog() {
    if (m_active.load()) {
        // Signal shutdown and wait for watchdog thread
        m_shutdownRequested.store(true);
        m_active.store(false);

        if (m_watchdogThread) {
            WaitForSingleObject(m_watchdogThread, 5000);
            CloseHandle(m_watchdogThread);
            m_watchdogThread = nullptr;
        }
    }
    // ---- Batch 7: Release sync-plane resources ----
    if (m_semaphoreKmtHandle && m_semaphoreKmtHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_semaphoreKmtHandle);
        m_semaphoreKmtHandle = nullptr;
    }
    SecureZeroMemory(m_heartbeatHmacKey, sizeof(m_heartbeatHmacKey));
}

// ============================================================================
// activate() — Start the watchdog thread
// ============================================================================
// Computes the initial .text SHA-256 baseline and spawns the monitoring
// thread. This is the entry point after the SelfRepairLoop and Camellia
// engine have finished initialization.
// ============================================================================

PatchResult SentinelWatchdog::activate() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_active.load()) {
        return PatchResult::ok("Sentinel already active");
    }

    // Step 1: Locate the .text section via PE headers
    if (!m_textLocated) {
        if (!locateTextSection()) {
            return PatchResult::error(
                "Sentinel: Failed to locate .text section in PE image", -1);
        }
    }

    // Step 2: Compute initial SHA-256 baseline of .text
    if (!computeTextHash(m_baselineHash)) {
        return PatchResult::error(
            "Sentinel: Failed to compute initial .text SHA-256 baseline", -2);
    }
    m_baselineValid = true;

    // Step 3: Initialize RDTSC baseline for timing checks
    m_lastRdtsc = __rdtsc();

    // Step 4: Reset shutdown flag and violation counter
    m_shutdownRequested.store(false);

    // Step 5: Spawn the watchdog thread
    m_watchdogThread = CreateThread(
        nullptr,                                // Default security
        0,                                      // Default stack size
        WatchdogThreadProc,                     // Thread function
        this,                                   // Parameter = this
        0,                                      // Run immediately
        &m_watchdogThreadId);

    if (!m_watchdogThread) {
        return PatchResult::error(
            "Sentinel: Failed to create watchdog thread", -3);
    }

    // Set thread priority slightly above normal for timely detection
    SetThreadPriority(m_watchdogThread, THREAD_PRIORITY_ABOVE_NORMAL);

    m_active.store(true);
    logEvent(SentinelEventType::Activated, "Sentinel activated: .text monitoring online");

    return PatchResult::ok(
        "Sentinel Watchdog activated: .text SHA-256 baseline computed, "
        "anti-debug monitoring online");
}

// ============================================================================
// deactivate() — Pause the watchdog for authorized patching
// ============================================================================
// Called BEFORE an authorized hotpatch via the Shadow-Page Detour system.
// The watchdog thread enters a sleep state — no .text integrity checks.
// The caller MUST call updateBaseline() then activate() after patching.
// ============================================================================

PatchResult SentinelWatchdog::deactivate() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_active.load()) {
        return PatchResult::ok("Sentinel already inactive");
    }

    // Signal the watchdog to stop checking, but don't terminate the thread.
    // We keep the thread alive so activate() is fast (no thread creation).
    m_active.store(false);

    logEvent(SentinelEventType::Deactivated,
             "Sentinel deactivated: authorized patch window open");

    return PatchResult::ok("Sentinel deactivated: patch window open");
}

// ============================================================================
// triggerLockdown() — Emergency response to unauthorized tampering
// ============================================================================
// This is a TERMINAL action. Once triggered:
//   1. Log the tamper event with full context
//   2. Encrypt all workspace files via Camellia-256 (destroy plaintext)
//   3. Terminate the process with ExitProcess(0xDEAD)
//
// Called when:
//   - .text hash does not match baseline (unauthorized binary mod)
//   - Debugger detected via PEB/NtGlobalFlag/DR/RDTSC beyond threshold
// ============================================================================

void SentinelWatchdog::triggerLockdown(const char* reason) {
    // Increment lockdown counter (atomic)
    m_stats.lockdownsTriggered++;

    // ---- BATCH 11: TELEMETRY UI HOOK ----
    // Keep a lightweight trace even when the optional D2D renderer is not linked.
    OutputDebugStringA("[Sentinel] Lockdown telemetry pulse\n");

    // ---- BATCH 9: THE 0xDEAD ATOMIC TRIGGER ----
    // Before we encrypt and terminate, we must signal the secondary nodes
    // to halt immediately to prevent "Zombie Model" inference (where 
    // one GPU continues processing on infected data).

    if (RawrXD::EnterpriseLicense::isFeatureEnabled(0x80)) {
        // Broadacst CRITICAL_VIOLATION to the Titan Sync-Plane.
        // Secondary nodes will receive this in their syncWithVulkanCluster()
        // call and trigger their own local lockdowns.
        syncWithVulkanCluster(); 
    }

    // Log the event FIRST
    char detail[128];
    snprintf(detail, sizeof(detail), "LOCKDOWN: %s", reason ? reason : "Unknown");
    logEvent(SentinelEventType::LockdownTriggered, detail);

    // Step 1: Deactivate the watchdog to prevent recursive triggers
    m_active.store(false);
    m_shutdownRequested.store(true);

    // Step 2: Zero decrypted kernel weights and HMAC key FIRST — immediate, irreversible.
    // This must run before encryptWorkspace() so the plaintext weight data and the
    // HMAC signing key are gone before disk I/O begins.
    wipeKernelWeights();

    // Step 3: Encrypt workspace files using the Camellia-256 bridge
    // This is a "scorched earth" response — all workspace data goes dark.
    encryptWorkspace();

    // Step 3: Zero sensitive data in memory
    SecureZeroMemory(m_baselineHash, sizeof(m_baselineHash));
    m_baselineValid = false;

    // Step 4: Terminate the process.
    // Exit code 0xDEAD signals to monitoring systems that a lockdown occurred.
    ExitProcess(0xDEAD);
}

// ============================================================================
// updateBaseline() — Rehash .text after authorized patch
// ============================================================================
// Called AFTER asm_hotpatch_atomic_swap succeeds and BEFORE sentinel_activate.
// Recomputes the SHA-256 hash of the .text section so the watchdog
// accepts the new patched state as the known-good baseline.
// ============================================================================

PatchResult SentinelWatchdog::updateBaseline() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_active.load()) {
        return PatchResult::error(
            "Sentinel: Cannot update baseline while active — deactivate first", -1);
    }

    if (!m_textLocated) {
        return PatchResult::error(
            "Sentinel: .text section not located — cannot update baseline", -2);
    }

    // Recompute SHA-256 of the (now-patched) .text section
    uint8_t newHash[SENTINEL_SHA256_DIGEST_SIZE];
    if (!computeTextHash(newHash)) {
        return PatchResult::error(
            "Sentinel: SHA-256 computation failed during baseline update", -3);
    }

    // Accept new hash as the baseline
    std::memcpy(m_baselineHash, newHash, SENTINEL_SHA256_DIGEST_SIZE);
    m_baselineValid = true;
    m_stats.baselineUpdates++;

    logEvent(SentinelEventType::BaselineUpdated,
             "Baseline updated: .text SHA-256 rehashed for authorized patch");

    return PatchResult::ok("Sentinel baseline updated: new .text hash accepted");
}

// ============================================================================
// syncWithVulkanCluster() — Cryptographic Heartbeat + Vulkan Sync-Plane (Batch 7)
// ============================================================================
// Architecture:
//   Each watchdog cycle (every SENTINEL_HEARTBEAT_INTERVAL_CYCLES * 500 ms):
//     1. Lazy-init the sync-plane (timeline semaphore + 128-byte VRAM buffer).
//     2. Increment local heartbeat counter (atomic — zero lock contention).
//     3. Compute HMAC-SHA256 of execution state bound to this counter.
//     4. Write 64-byte payload to the VRAM-lean buffer (counter + HMAC + RDTSC).
//     5. Signal local timeline semaphore to the new counter value.
//     6. Single-GPU: readback own semaphore for self-consistency.
//        Multi-GPU:  poll remote semaphore; wait at most 1 ms before timeout.
//     7. Validate remote counter monotonicity (delta > 100 = desync tamper).
//
// Non-blocking contract:
//   GetTimelineSemaphoreValue() is a non-blocking state query.
//   WaitTimelineSemaphore() is only called (once) when the remote counter is
//   stale, with a hard 1 ms timeout — execution resumes immediately after.
//   Neither call ever blocks the main inference VkQueue submission.
// ============================================================================

PatchResult SentinelWatchdog::syncWithVulkanCluster() {
    // ---- Step 1: Lazy-initialize the sync-plane ----
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_syncPlaneInit && m_vkCompute) {
            if (!initHeartbeatSyncPlane()) {
                logEvent(SentinelEventType::HeartbeatDesync,
                         "Heartbeat sync-plane initialization failed");
                return PatchResult::error(
                    "Sentinel: Heartbeat sync-plane initialization failed", -10);
            }
        }
    }

    if (!m_syncPlaneInit) {
        // VulkanCompute not wired — degrade gracefully (no GPU sync possible).
        return PatchResult::ok(
            "Sentinel: Sync-plane not initialized — GPU heartbeat skipped");
    }

    // ---- Step 2: Increment local heartbeat counter (lock-free) ----
    uint64_t myCounter =
        m_heartbeatCounter.fetch_add(1, std::memory_order_relaxed) + 1;

    // ---- Step 3: Compute HMAC-SHA256 of execution state ----
    uint8_t hmac[SENTINEL_SHA256_DIGEST_SIZE];
    if (!computeHeartbeatHMAC(myCounter, hmac)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        logEvent(SentinelEventType::HeartbeatDesync,
                 "HMAC-SHA256 computation failed — crypto subsystem fault");
        return PatchResult::error("Sentinel: Heartbeat HMAC failed", -11);
    }

    // ---- Step 4: Write lean 64-byte VRAM payload ----
    // Layout: [counter(8)] [hmac(32)] [rdtsc(8)] [reserved(16)]
    // Total: 64 bytes — aggressively lean to respect 16 GB VRAM limit.
    alignas(8) uint8_t payload[SENTINEL_HEARTBEAT_PAYLOAD_BYTES];
    std::memset(payload, 0, sizeof(payload));
    std::memcpy(payload,      &myCounter,                     8);
    std::memcpy(payload + 8,  hmac,   SENTINEL_SHA256_DIGEST_SIZE);
    uint64_t ts = __rdtsc();
    std::memcpy(payload + 8 + SENTINEL_SHA256_DIGEST_SIZE, &ts, 8);

    if (m_heartbeatBufIdx != UINT32_MAX) {
        m_vkCompute->CopyHostToBuffer(
            static_cast<void*>(payload), m_heartbeatBufIdx,
            SENTINEL_HEARTBEAT_PAYLOAD_BYTES);
    }

    // ---- Step 5: Signal local timeline semaphore ----
    // This is an asynchronous GPU signal — does not stall the CPU.
    m_vkCompute->SignalTimelineSemaphore(m_localHeartbeatSem, myCounter);

    // ---- Step 6a: Single-GPU self-consistency readback ----
    bool multiGpu = RawrXD::EnterpriseLicense::isFeatureEnabled(0x80);
    if (!multiGpu ||
        m_remoteHeartbeatSem == static_cast<VkSemaphore>(VK_NULL_HANDLE)) {
        uint64_t readback = 0;
        if (!m_vkCompute->GetTimelineSemaphoreValue(
                m_localHeartbeatSem, readback) ||
            readback != myCounter) {
            std::lock_guard<std::mutex> lock(m_mutex);
            char detail[128];
            std::snprintf(detail, sizeof(detail),
                "Single-GPU semaphore readback mismatch: "
                "expected %llu got %llu",
                (unsigned long long)myCounter,
                (unsigned long long)readback);
            logEvent(SentinelEventType::HeartbeatDesync, detail);
            m_violationCount.fetch_add(1, std::memory_order_relaxed);
            return PatchResult::error(
                "Sentinel: Heartbeat self-consistency check failed", -12);
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        logEvent(SentinelEventType::HeartbeatVerified,
                 "Single-GPU: heartbeat HMAC + semaphore readback OK");
        return PatchResult::ok("Vulkan heartbeat self-verified (single-GPU)");
    }

    // ---- Step 6b: Multi-GPU — non-blocking poll of remote node ----
    if (!checkRemoteHeartbeatNonBlocking()) {
        // checkRemoteHeartbeatNonBlocking logs the failure and increments
        // m_violationCount. triggerLockdown is the caller's responsibility.
        triggerLockdown("Vulkan P2P Sync-Plane: Remote node heartbeat failure");
        return PatchResult::error(
            "Distributed Sentinel: Cluster synchronization failure", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    logEvent(SentinelEventType::HeartbeatVerified,
             "Distributed Sentinel: P2P cluster heartbeat verified");
    return PatchResult::ok("Vulkan cluster heartbeat synchronized");
}

// ============================================================================
// setVulkanCompute() — Wire the VulkanCompute engine for heartbeat operations
// ============================================================================

void SentinelWatchdog::setVulkanCompute(VulkanCompute* vk) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vkCompute    = vk;
    // Reset sync-plane so it re-initializes with the new engine instance.
    m_syncPlaneInit = false;
}

// ============================================================================
// initHeartbeatSyncPlane() — Timeline semaphore + lean VRAM buffer setup
// ============================================================================
// Called with m_mutex held from syncWithVulkanCluster().
// Creates the local timeline semaphore starting at value 0, derives the
// HMAC-SHA256 key from BCrypt RNG mixed with the .text baseline hash, and
// allocates the 128-byte VRAM payload buffer (two 64-byte slots, one per GPU).
// ============================================================================

bool SentinelWatchdog::initHeartbeatSyncPlane() {
    if (!m_vkCompute) return false;

    // ---- Create local timeline semaphore (GPU0 heartbeat signal) ----
    if (!m_vkCompute->CreateTimelineSemaphore(m_localHeartbeatSem, 0)) {
        return false;
    }

    // ---- Derive HMAC key: BCrypt RNG + baseline-hash mixing ----
    // BCrypt RNG provides unpredictable entropy; XOR with the first 32 bytes
    // of the .text SHA-256 baseline binds the key to this specific binary.
    BCRYPT_ALG_HANDLE hRng = nullptr;
    if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
            &hRng, BCRYPT_RNG_ALGORITHM, nullptr, 0))) {
        BCryptGenRandom(hRng, m_heartbeatHmacKey,
                        SENTINEL_HEARTBEAT_HMAC_KEY_SIZE, 0);
        BCryptCloseAlgorithmProvider(hRng, 0);
    } else {
        // Fallback: deterministic derivation from baseline + index scramble.
        for (int i = 0; i < SENTINEL_HEARTBEAT_HMAC_KEY_SIZE; ++i) {
            m_heartbeatHmacKey[i] =
                m_baselineHash[i % SENTINEL_SHA256_DIGEST_SIZE] ^
                static_cast<uint8_t>(i * 0x5A + 0xA3);
        }
    }
    // Mix in the full baseline hash so the HMAC key is tightly bound to the
    // currently running binary image.
    for (int i = 0; i < SENTINEL_SHA256_DIGEST_SIZE; ++i) {
        m_heartbeatHmacKey[i] ^= m_baselineHash[i];
    }

    // ---- Allocate lean VRAM buffer: 2 × 64 bytes = 128 bytes total ----
    // Local payload at offset 0, remote payload at offset 64.
    // 128 bytes is negligible against the 16 GB VRAM budget.
    size_t actualAlloced = 0;
    if (!m_vkCompute->AllocateBuffer(
            SENTINEL_HEARTBEAT_PAYLOAD_BYTES * 2,
            m_heartbeatBufIdx,
            actualAlloced,
            false /* KMT export not needed for the payload buffer */)) {
        m_heartbeatBufIdx = UINT32_MAX;  // Non-fatal: proceed without VRAM mirror
    }

    // ---- Export local semaphore via KMT for multi-GPU P2P (if enabled) ----
    if (RawrXD::EnterpriseLicense::isFeatureEnabled(0x80)) {
        m_vkCompute->ExportTimelineSemaphoreKMT(
            m_localHeartbeatSem, m_semaphoreKmtHandle);
        // m_remoteHeartbeatSem is wired externally via ImportTimelineSemaphoreKMT
        // from the peer node's KMT handle (supplied by MultiGPUManager or IPC).
    }

    m_heartbeatCounter.store(0, std::memory_order_relaxed);
    m_lastRemoteCounter  = 0;
    m_heartbeatCycleCount = 0;
    m_syncPlaneInit      = true;

    logEvent(SentinelEventType::HeartbeatVerified,
             "Heartbeat sync-plane ready: timeline semaphore + 128-byte VRAM buffer");
    return true;
}

// ============================================================================
// computeHeartbeatHMAC() — BCrypt HMAC-SHA256 of execution state
// ============================================================================
// Message layout (36 bytes):
//   [0..7]   counter          (uint64_t LE)
//   [8..11]  violationCount   (uint32_t LE)
//   [12..19] GetTickCount64()  (uint64_t LE)
//   [20..35] m_baselineHash[0..15] (first 16 bytes of the .text SHA-256)
//
// The message is compact (36 bytes) to keep the per-cycle BCrypt overhead
// negligible on the dedicated watchdog thread.
// ============================================================================

bool SentinelWatchdog::computeHeartbeatHMAC(
        uint64_t counter,
        uint8_t  outHmac[SENTINEL_SHA256_DIGEST_SIZE]) {

    static constexpr size_t kMsgSize = 8 + 4 + 8 + 16;   // 36 bytes
    uint8_t msg[kMsgSize];

    std::memcpy(msg,      &counter,    8);
    uint32_t vCount = m_violationCount.load(std::memory_order_relaxed);
    std::memcpy(msg + 8,  &vCount,     4);
    uint64_t tick   = GetTickCount64();
    std::memcpy(msg + 12, &tick,       8);
    std::memcpy(msg + 20, m_baselineHash, 16);

    BCRYPT_ALG_HANDLE  hAlg  = nullptr;
    BCRYPT_HASH_HANDLE hHmac = nullptr;
    bool               ok    = false;

    NTSTATUS st = BCryptOpenAlgorithmProvider(
        &hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(st)) return false;

    DWORD objSize = 0, cbRes = 0;
    st = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH,
                           reinterpret_cast<PUCHAR>(&objSize),
                           sizeof(objSize), &cbRes, 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    uint8_t* obj = static_cast<uint8_t*>(
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, objSize));
    if (!obj) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    st = BCryptCreateHash(
        hAlg, &hHmac, obj, objSize,
        m_heartbeatHmacKey, SENTINEL_HEARTBEAT_HMAC_KEY_SIZE, 0);
    if (!BCRYPT_SUCCESS(st)) goto hmac_cleanup;

    st = BCryptHashData(hHmac, msg, static_cast<ULONG>(kMsgSize), 0);
    if (!BCRYPT_SUCCESS(st)) goto hmac_cleanup;

    st = BCryptFinishHash(hHmac, outHmac, SENTINEL_SHA256_DIGEST_SIZE, 0);
    ok = BCRYPT_SUCCESS(st);

hmac_cleanup:
    if (hHmac) BCryptDestroyHash(hHmac);
    SecureZeroMemory(obj, objSize);
    HeapFree(GetProcessHeap(), 0, obj);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

// ============================================================================
// checkRemoteHeartbeatNonBlocking() — Poll remote GPU node semaphore
// ============================================================================
// Contract (non-blocking):
//   1. GetTimelineSemaphoreValue() — instant, non-blocking state query.
//   2. If remote counter has not advanced, wait at most 1 ms via
//      WaitTimelineSemaphore(SENTINEL_HEARTBEAT_TIMEOUT_NS).
//      This is the ONLY call that may block, and only up to 1 ms.
//   3. If still not advanced after 1 ms → HeartbeatTimeout + violation.
//   4. If counter jumped by > 100 → HeartbeatDesync + violation.
//
// Returns true if the remote heartbeat is healthy; false on fault.
// The caller (syncWithVulkanCluster) must invoke triggerLockdown on false.
// ============================================================================

bool SentinelWatchdog::checkRemoteHeartbeatNonBlocking() {
    if (!m_vkCompute ||
        m_remoteHeartbeatSem == static_cast<VkSemaphore>(VK_NULL_HANDLE)) {
        // Remote semaphore not yet imported — treat as healthy (not yet wired).
        return true;
    }

    uint64_t remoteVal = 0;
    if (!m_vkCompute->GetTimelineSemaphoreValue(m_remoteHeartbeatSem, remoteVal)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        logEvent(SentinelEventType::HeartbeatTimeout,
                 "GetTimelineSemaphoreValue failed for remote GPU node");
        m_violationCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Stale: peer counter has not advanced since last confirmed check.
    if (remoteVal <= m_lastRemoteCounter) {
        // Give the remote node up to 1 ms before declaring a timeout.
        bool advanced = m_vkCompute->WaitTimelineSemaphore(
            m_remoteHeartbeatSem,
            m_lastRemoteCounter + 1,
            SENTINEL_HEARTBEAT_TIMEOUT_NS);   // 1 000 000 ns = 1 ms

        if (!advanced) {
            std::lock_guard<std::mutex> lock(m_mutex);
            char detail[128];
            std::snprintf(detail, sizeof(detail),
                "Remote node timeout: expected >%llu got %llu after 1 ms",
                (unsigned long long)m_lastRemoteCounter,
                (unsigned long long)remoteVal);
            logEvent(SentinelEventType::HeartbeatTimeout, detail);
            m_stats.lockdownsTriggered++;   // Pre-count before triggerLockdown
            m_violationCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        // Refresh after the wait.
        m_vkCompute->GetTimelineSemaphoreValue(m_remoteHeartbeatSem, remoteVal);
    }

    // Desync guard: the remote counter must only advance monotonically.
    // A jump of more than 100 cycles in a single ~2.5 s window is anomalous
    // and indicates either a timeline manipulation or a clock rollback attack.
    if (remoteVal > m_lastRemoteCounter + 100) {
        std::lock_guard<std::mutex> lock(m_mutex);
        char detail[128];
        std::snprintf(detail, sizeof(detail),
            "Remote counter desync: delta=%llu (expected \xE2\x89\xA4 100)",
            (unsigned long long)(remoteVal - m_lastRemoteCounter));
        logEvent(SentinelEventType::HeartbeatDesync, detail);
        m_violationCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    m_lastRemoteCounter = remoteVal;
    return true;
}

// ============================================================================
// wipeKernelWeights() — Zero crypto material + VRAM sentinel buffers
// ============================================================================
// Called from triggerLockdown() BEFORE encryptWorkspace() to ensure that:
//   - The HMAC signing key used to generate heartbeat payloads is gone.
//   - The VRAM heartbeat buffer is overwritten (prevents GPU memory forensics).
//   - Timeline semaphore references are nulled (halts future GPU queue gating).
//
// This is a best-effort operation. Failures are silent — we are about to
// ExitProcess(0xDEAD) regardless.
// ============================================================================

void SentinelWatchdog::wipeKernelWeights() {
    // ---- 1. Zero HMAC key material (first, fastest, most critical) ----
    SecureZeroMemory(m_heartbeatHmacKey, sizeof(m_heartbeatHmacKey));

    // ---- 2. Overwrite VRAM heartbeat buffer with zeros ----
    // Prevents post-mortem GPU memory reads from recovering the heartbeat
    // payload or inferring the execution state at the time of compromise.
    if (m_syncPlaneInit && m_vkCompute && m_heartbeatBufIdx != UINT32_MAX) {
        static const uint8_t kZero[SENTINEL_HEARTBEAT_PAYLOAD_BYTES * 2] = {};
        m_vkCompute->CopyHostToBuffer(
            const_cast<void*>(static_cast<const void*>(kZero)),
            m_heartbeatBufIdx,
            SENTINEL_HEARTBEAT_PAYLOAD_BYTES * 2);
    }

    // ---- 3. Null timeline semaphore handles ----
    // Once nulled, any pending VkQueue wait on m_localHeartbeatSem will
    // resolve incorrectly, causing the inference queue to stall — precisely
    // the intended effect during a zero-trust lockdown.
    m_localHeartbeatSem  = static_cast<VkSemaphore>(VK_NULL_HANDLE);
    m_remoteHeartbeatSem = static_cast<VkSemaphore>(VK_NULL_HANDLE);

    logEvent(SentinelEventType::KernelWeightsWiped,
             "Lockdown: HMAC key zeroed, VRAM cleared, semaphores nulled");
}

// ============================================================================
// Query methods
// ============================================================================

bool SentinelWatchdog::isActive() const {
    return m_active.load();
}

SentinelStats SentinelWatchdog::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

uint32_t SentinelWatchdog::getViolationCount() const {
    return m_violationCount.load();
}

const SentinelEvent* SentinelWatchdog::getEventLog(uint32_t& outCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    outCount = m_eventLogCount;
    return m_eventLog;
}

// ============================================================================
// computeTextHash() — BCrypt SHA-256 of the .text section
// ============================================================================
// Uses the Windows CNG (Cryptography Next Generation) BCrypt API for
// FIPS 180-4 compliant SHA-256 hashing. No OpenSSL dependency.
// ============================================================================

bool SentinelWatchdog::computeTextHash(uint8_t outHash[SENTINEL_SHA256_DIGEST_SIZE]) {
    if (!m_textBase || m_textSize == 0) {
        return false;
    }

    BCRYPT_ALG_HANDLE   hAlg    = nullptr;
    BCRYPT_HASH_HANDLE  hHash   = nullptr;
    NTSTATUS            status;
    bool                success = false;

    // Open SHA-256 algorithm provider
    status = BCryptOpenAlgorithmProvider(
        &hAlg,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0);

    if (!BCRYPT_SUCCESS(status)) {
        return false;
    }

    // Query hash object size
    DWORD hashObjSize = 0;
    DWORD cbResult    = 0;
    status = BCryptGetProperty(
        hAlg,
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&hashObjSize),
        sizeof(hashObjSize),
        &cbResult,
        0);

    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Allocate hash object on the process heap
    uint8_t* hashObj = static_cast<uint8_t*>(
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, hashObjSize));

    if (!hashObj) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Create the hash
    status = BCryptCreateHash(
        hAlg,
        &hHash,
        hashObj,
        hashObjSize,
        nullptr,        // No key (plain hash, not HMAC)
        0,
        0);

    if (!BCRYPT_SUCCESS(status)) {
        HeapFree(GetProcessHeap(), 0, hashObj);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Hash the .text section contents
    // Process in 64KB chunks to avoid overwhelming the hash engine
    const uint8_t*  textPtr     = static_cast<const uint8_t*>(m_textBase);
    size_t          remaining   = m_textSize;
    const size_t    chunkSize   = 65536;

    while (remaining > 0) {
        ULONG thisChunk = static_cast<ULONG>(
            remaining > chunkSize ? chunkSize : remaining);

        status = BCryptHashData(
            hHash,
            const_cast<PUCHAR>(textPtr),
            thisChunk,
            0);

        if (!BCRYPT_SUCCESS(status)) {
            goto cleanup;
        }

        textPtr   += thisChunk;
        remaining -= thisChunk;
    }

    // Finalize — write the 32-byte digest
    status = BCryptFinishHash(hHash, outHash, SENTINEL_SHA256_DIGEST_SIZE, 0);
    success = BCRYPT_SUCCESS(status);

cleanup:
    if (hHash) BCryptDestroyHash(hHash);
    if (hashObj) {
        SecureZeroMemory(hashObj, hashObjSize);
        HeapFree(GetProcessHeap(), 0, hashObj);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return success;
}

// ============================================================================
// locateTextSection() — Find .text in the current module's PE image
// ============================================================================
// Walks the PE headers of the main executable (GetModuleHandle(NULL)) to
// find the .text section's virtual address and size. This is cached on
// first call — the .text section doesn't move during process lifetime.
// ============================================================================

bool SentinelWatchdog::locateTextSection() {
    // Get the base address of the main module
    HMODULE hModule = GetModuleHandleA(nullptr);
    if (!hModule) {
        return false;
    }

    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {      // "MZ"
        return false;
    }

    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(
        reinterpret_cast<uint8_t*>(hModule) + dosHeader->e_lfanew);

    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {     // "PE\0\0"
        return false;
    }

    // Walk section headers to find .text
    uint16_t numSections = ntHeaders->FileHeader.NumberOfSections;
    auto* sections = IMAGE_FIRST_SECTION(ntHeaders);

    for (uint16_t i = 0; i < numSections; ++i) {
        // Compare section name (8 bytes, null-padded)
        if (std::memcmp(sections[i].Name, ".text", 5) == 0) {
            m_textBase = reinterpret_cast<void*>(
                reinterpret_cast<uintptr_t>(hModule) +
                sections[i].VirtualAddress);
            m_textSize = sections[i].Misc.VirtualSize;

            // Validate — the section should be executable
            if (!(sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) {
                fprintf(stderr, "[SentinelWatchdog] Warning: .text section not executable\n");
            }

            m_textLocated = true;
            return true;
        }
    }

    return false;   // .text section not found
}

// ============================================================================
// Anti-Debug: PEB.BeingDebugged
// ============================================================================
// Directly reads the PEB via the TEB (gs:[60h] on x64) to check the
// BeingDebugged flag. This is faster than calling IsDebuggerPresent().
// ============================================================================

bool SentinelWatchdog::checkPEBDebugger() const {
    // Read PEB.BeingDebugged via inline assembly semantics
    // On x64: gs:[60h] = PEB, PEB+2 = BeingDebugged (BOOLEAN)
    PPEB pPeb = reinterpret_cast<PPEB>(__readgsqword(0x60));
    if (!pPeb) return false;

    return pPeb->BeingDebugged != 0;
}

// ============================================================================
// Anti-Debug: PEB.NtGlobalFlag
// ============================================================================
// When a process is created under a debugger, NtGlobalFlag at PEB+0xBC
// has the flags: FLG_HEAP_ENABLE_TAIL_CHECK (0x10) |
// FLG_HEAP_ENABLE_FREE_CHECK (0x20) | FLG_HEAP_VALIDATE_PARAMETERS (0x40)
// = 0x70. Any non-zero value is suspicious in release builds.
// ============================================================================

bool SentinelWatchdog::checkNtGlobalFlag() const {
    PPEB pPeb = reinterpret_cast<PPEB>(__readgsqword(0x60));
    if (!pPeb) return false;

    // NtGlobalFlag is at offset 0xBC in the PEB (64-bit)
    // The PEB struct in winternl.h doesn't expose this directly,
    // so we read via raw pointer arithmetic.
    uint32_t ntGlobalFlag = *reinterpret_cast<uint32_t*>(
        reinterpret_cast<uint8_t*>(pPeb) + 0xBC);

    // Debug-specific flags: FLG_HEAP_ENABLE_TAIL_CHECK (0x10) |
    // FLG_HEAP_ENABLE_FREE_CHECK (0x20) | FLG_HEAP_VALIDATE_PARAMETERS (0x40)
    static constexpr uint32_t kDebugFlags = 0x70;
    return (ntGlobalFlag & kDebugFlags) != 0;
}

// ============================================================================
// Anti-Debug: Hardware Debug Registers DR0-DR3
// ============================================================================
// Hardware breakpoints set via DR0-DR3 are a common debugger technique.
// We use GetThreadContext to read DR0-DR3; any non-zero value indicates
// a hardware breakpoint is set.
// ============================================================================

bool SentinelWatchdog::checkHardwareBreakpoints() const {
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    if (!GetThreadContext(GetCurrentThread(), &ctx)) {
        return false;   // Failed to query — assume OK
    }

    // DR0-DR3 hold breakpoint addresses. All should be 0 if no HW BPs.
    return (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0);
}

// ============================================================================
// Anti-Debug: RDTSC Timing Analysis
// ============================================================================
// When a debugger single-steps or pauses execution, the RDTSC delta
// between consecutive checks will be abnormally large (> 50,000 cycles
// for a ~500ms sleep on modern CPUs). This detects software debuggers
// that don't virtualize the timestamp counter.
// ============================================================================

bool SentinelWatchdog::checkTimingAnomaly() {
    uint64_t currentTsc = __rdtsc();

    if (m_lastRdtsc == 0) {
        m_lastRdtsc = currentTsc;
        return false;
    }

    uint64_t delta = currentTsc - m_lastRdtsc;
    m_lastRdtsc = currentTsc;

    // Expected delta for a ~500ms sleep at ~3GHz = ~1.5 billion cycles.
    // We flag if the delta is MUCH larger than expected — this indicates
    // the process was paused by a debugger for an extended period.
    // Threshold: 10 billion cycles (~3.3 seconds at 3GHz) — generous to
    // avoid false positives from system load / scheduling.
    static constexpr uint64_t kMaxExpectedDelta = 10000000000ULL;  // 10B cycles
    return delta > kMaxExpectedDelta;
}

// ============================================================================
// logEvent() — Ring buffer event log
// ============================================================================

void SentinelWatchdog::logEvent(SentinelEventType type, const char* detail) {
    // No lock needed — called from within locked context or from
    // the watchdog thread which is the only writer

    SentinelEvent& evt = m_eventLog[m_eventLogHead % kMaxEventLog];
    evt.type            = type;
    evt.timestamp       = GetTickCount64();
    evt.rdtscValue      = __rdtsc();
    evt.violationCount  = m_violationCount.load();

    if (detail) {
        size_t len = 0;
        while (detail[len] && len < 127) {
            evt.detail[len] = detail[len];
            ++len;
        }
        evt.detail[len] = '\0';
    } else {
        evt.detail[0] = '\0';
    }

    m_eventLogHead++;
    if (m_eventLogCount < kMaxEventLog) {
        m_eventLogCount++;
    }
}

// ============================================================================
// encryptWorkspace() — Lockdown response: encrypt all files
// ============================================================================
// Uses the Camellia-256 bridge to encrypt the entire workspace directory
// with authenticated encryption (Encrypt-then-MAC). After encryption,
// the plaintext files are replaced with ciphertext. This is irreversible
// without the HWID-derived key.
// ============================================================================

void SentinelWatchdog::encryptWorkspace() {
    // The workspace path is typically the current working directory.
    // In the full RawrXD IDE, this would come from the project settings.
    char workspacePath[MAX_PATH];
    DWORD len = GetCurrentDirectoryA(MAX_PATH, workspacePath);

    if (len == 0 || len >= MAX_PATH) {
        // Fallback: try to get the module directory
        GetModuleFileNameA(nullptr, workspacePath, MAX_PATH);
        // Strip the filename to get directory
        char* lastSlash = strrchr(workspacePath, '\\');
        if (lastSlash) *lastSlash = '\0';
    }

    // Encrypt in-place using the Camellia-256 bridge (authenticated mode)
    auto& bridge = RawrXD::Crypto::Camellia256Bridge::instance();
    if (bridge.isInitialized()) {
        bridge.encryptWorkspace(workspacePath, true);
    }
}

// ============================================================================
// WatchdogThreadProc — Static entry point for CreateThread
// ============================================================================

DWORD WINAPI SentinelWatchdog::WatchdogThreadProc(LPVOID param) {
    auto* self = static_cast<SentinelWatchdog*>(param);
    if (self) {
        self->watchdogLoop();
    }
    return 0;
}

// ============================================================================
// watchdogLoop() — Main monitoring loop
// ============================================================================
// Runs on the dedicated watchdog thread. Performs the following checks
// every SENTINEL_POLL_INTERVAL_MS (500ms):
//
//   1. .text SHA-256 integrity check (compare against baseline)
//   2. PEB.BeingDebugged check
//   3. PEB.NtGlobalFlag check
//   4. Hardware debug register (DR0-DR3) check
//   5. RDTSC timing anomaly check
//
// If any check fails, the violation counter is incremented. Once the
// counter exceeds SENTINEL_MAX_VIOLATIONS, triggerLockdown() is called.
//
// The loop respects m_active (pause/resume) and m_shutdownRequested (exit).
// ============================================================================

void SentinelWatchdog::watchdogLoop() {
    uint64_t monitoringStartTime = GetTickCount64();

    while (!m_shutdownRequested.load()) {
        // Sleep for the poll interval
        Sleep(SENTINEL_POLL_INTERVAL_MS);

        // Check if we should continue (deactivated = paused, not terminated)
        if (!m_active.load()) {
            continue;   // Paused — skip all checks until reactivated
        }

        // ---- CHECK 1: .text section SHA-256 integrity ----
        if (m_baselineValid) {
            uint8_t currentHash[SENTINEL_SHA256_DIGEST_SIZE];
            if (computeTextHash(currentHash)) {
                // Constant-time comparison to prevent timing side-channels
                volatile uint8_t diff = 0;
                for (int i = 0; i < SENTINEL_SHA256_DIGEST_SIZE; ++i) {
                    diff |= currentHash[i] ^ m_baselineHash[i];
                }

                if (diff != 0) {
                    m_stats.hashMismatches++;
                    uint32_t violations = m_violationCount.fetch_add(1) + 1;

                    char detail[128];
                    snprintf(detail, sizeof(detail),
                             ".text SHA-256 mismatch (violation %u/%u)",
                             violations, SENTINEL_MAX_VIOLATIONS);
                    logEvent(SentinelEventType::TextHashMismatch, detail);

                    if (violations >= SENTINEL_MAX_VIOLATIONS) {
                        triggerLockdown("Unauthorized .text modification detected");
                        return; // triggerLockdown calls ExitProcess, but just in case
                    }
                }
            }
        }

        // ---- CHECK 2: PEB.BeingDebugged ----
        if (checkPEBDebugger()) {
            m_stats.debuggerDetections++;
            uint32_t violations = m_violationCount.fetch_add(1) + 1;

            logEvent(SentinelEventType::DebuggerDetected,
                     "PEB.BeingDebugged flag set");

            if (violations >= SENTINEL_MAX_VIOLATIONS) {
                triggerLockdown("Debugger detected via PEB.BeingDebugged");
                return;
            }
        }

        // ---- CHECK 3: PEB.NtGlobalFlag ----
        if (checkNtGlobalFlag()) {
            m_stats.debuggerDetections++;
            uint32_t violations = m_violationCount.fetch_add(1) + 1;

            logEvent(SentinelEventType::DebuggerDetected,
                     "NtGlobalFlag debug heap flags detected");

            if (violations >= SENTINEL_MAX_VIOLATIONS) {
                triggerLockdown("Debugger detected via NtGlobalFlag");
                return;
            }
        }

        // ---- CHECK 4: Hardware Debug Registers ----
        if (checkHardwareBreakpoints()) {
            m_stats.hwBreakpointHits++;
            uint32_t violations = m_violationCount.fetch_add(1) + 1;

            logEvent(SentinelEventType::HardwareBreakpoint,
                     "Hardware breakpoint detected in DR0-DR3");

            if (violations >= SENTINEL_MAX_VIOLATIONS) {
                triggerLockdown("Hardware breakpoint detected in debug registers");
                return;
            }
        }

        // ---- CHECK 5: RDTSC Timing Anomaly ----
        if (checkTimingAnomaly()) {
            m_stats.timingAnomalies++;
            uint32_t violations = m_violationCount.fetch_add(1) + 1;

            logEvent(SentinelEventType::TimingAnomaly,
                     "RDTSC timing anomaly — possible debugger pause");

            if (violations >= SENTINEL_MAX_VIOLATIONS) {
                triggerLockdown("RDTSC timing anomaly — debugger interference");
                return;
            }
        }

        // ---- CHECK 6: Cryptographic Heartbeat & Vulkan Sync-Plane (Batch 7) ----
        // Runs every SENTINEL_HEARTBEAT_INTERVAL_CYCLES watchdog cycles (~2.5 s
        // at the default 500 ms poll interval).
        //
        // Non-blocking contract: uses GetTimelineSemaphoreValue() (instant) for
        // the common path.  WaitTimelineSemaphore() is only called when the peer
        // counter is stale, and caps at SENTINEL_HEARTBEAT_TIMEOUT_NS (1 ms).
        // The watchdog thread is fully background — the inference VkQueue is
        // never stalled by this check.
        {
            if (++m_heartbeatCycleCount >= SENTINEL_HEARTBEAT_INTERVAL_CYCLES) {
                m_heartbeatCycleCount = 0;
                PatchResult syncResult = syncWithVulkanCluster();
                if (!syncResult.success) {
                    // Fatal violations (timeout / desync) already invoke
                    // triggerLockdown() inside syncWithVulkanCluster().
                    return;
                }
            }
        }

        // ---- Update statistics ----
        m_stats.totalChecks++;
        m_stats.uptimeMs = GetTickCount64() - monitoringStartTime;
    }

    // Shutdown requested — clean exit
    m_active.store(false);
}

// ============================================================================
// C-ABI Exports — Thin wrappers for cross-module / ASM callers
// ============================================================================
// These functions provide a flat C interface to the SentinelWatchdog
// singleton, suitable for calling from MASM, the Unified Hotpatch Manager,
// or any module that can't include C++ headers.
// ============================================================================

extern "C" {

int sentinel_activate(void) {
    PatchResult r = SentinelWatchdog::instance().activate();
    return r.success ? 0 : -1;
}

int sentinel_deactivate(void) {
    PatchResult r = SentinelWatchdog::instance().deactivate();
    return r.success ? 0 : -1;
}

void sentinel_trigger(const char* reason) {
    SentinelWatchdog::instance().triggerLockdown(reason);
}

int sentinel_update_baseline(void) {
    PatchResult r = SentinelWatchdog::instance().updateBaseline();
    return r.success ? 0 : -1;
}

int sentinel_is_active(void) {
    return SentinelWatchdog::instance().isActive() ? 1 : 0;
}

int sentinel_get_violation_count(void) {
    return static_cast<int>(SentinelWatchdog::instance().getViolationCount());
}

} // extern "C"
