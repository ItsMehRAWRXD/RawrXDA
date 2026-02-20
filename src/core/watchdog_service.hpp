// ============================================================================
// watchdog_service.hpp — Agentic .text Section Integrity Watchdog Service
// ============================================================================
//
// PURPOSE:
//   C++ bridge to the MASM Watchdog kernel (RawrXD_Watchdog.asm).
//   Provides a background thread that periodically re-hashes the IDE's
//   .text section via HMAC-SHA256, detecting runtime code patching,
//   in-memory hooks, or malicious modification.
//
// Features:
//   - One-shot initialize + baseline capture
//   - Configurable periodic verification (default: every 30 seconds)
//   - Tamper callback — user-supplied function pointer (no std::function)
//   - Thread-safe status queries
//   - PatchResult-compatible returns
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      Singleton, background OS thread
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <string>

// ============================================================================
//  MASM extern declarations (RawrXD_Watchdog.asm exports)
// ============================================================================

extern "C" {
    // Initialize watchdog: locate .text, derive HMAC key, capture baseline
    int asm_watchdog_init();

    // Re-hash .text and compare against baseline (0=OK, 1=tampered, <0=error)
    int asm_watchdog_verify();

    // Copy 32-byte baseline HMAC to caller buffer
    int asm_watchdog_get_baseline(uint8_t* hmac32);

    // Get watchdog status structure (48 bytes)
    int asm_watchdog_get_status(void* status48);

    // Zero all keying material and state
    int asm_watchdog_shutdown();
}

namespace RawrXD {

// ============================================================================
//  Status codes (must match RawrXD_Watchdog.asm constants)
// ============================================================================

enum class WatchdogStatus : uint32_t {
    Uninitialized = 0xFFFFFFFF,
    OK            = 0,
    Tampered      = 1,
    NoKey         = 2,
    NoTextSection = 3,
    CryptoFailure = 4,
    InvalidPE     = 5,
};

// ============================================================================
//  Status structure (matches MASM layout at asm_watchdog_get_status)
// ============================================================================

struct WatchdogStatusInfo {
    uint32_t status;            // WatchdogStatus code
    uint32_t reserved;
    uint64_t textBase;          // VA of .text section
    uint64_t textSize;          // Size of .text section
    uint64_t verifyCount;       // Total verification passes
    uint64_t tamperCount;       // Number of tamper detections
    uint64_t lastVerifyTick;    // GetTickCount64 of last check
};

// ============================================================================
//  PatchResult-compatible result type
// ============================================================================

struct WatchdogResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static WatchdogResult ok(const char* msg) {
        return { true, msg, 0 };
    }
    static WatchdogResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
//  Tamper callback — plain function pointer (no std::function per rules)
// ============================================================================
//   event:     WatchdogStatus code that triggered the callback
//   detail:    Human-readable message from MASM kernel
//   userData:  Opaque pointer passed during registration
// ============================================================================

using WatchdogTamperCallback = void(*)(WatchdogStatus event,
                                        const char* detail,
                                        void* userData);

// ============================================================================
//  WatchdogService — High-level C++ wrapper with background timer thread
// ============================================================================

class WatchdogService {
public:
    // Singleton access
    static WatchdogService& instance();

    // ---- Lifecycle ----

    // Initialize the MASM kernel: locate .text, derive key, capture baseline
    WatchdogResult initialize();

    // Start periodic background verification thread
    WatchdogResult startPeriodicVerification(uint32_t intervalMs = 30000);

    // Stop the background thread (blocks until thread exits)
    WatchdogResult stopPeriodicVerification();

    // Perform a single verification (callable from any thread)
    WatchdogResult verifySingle();

    // Secure shutdown (stops thread + zeros keying material)
    WatchdogResult shutdown();

    // ---- Configuration ----

    // Set verification interval (can be called while thread is running)
    void setVerifyIntervalMs(uint32_t ms);

    // Register tamper callback (replaces any previous callback)
    void setTamperCallback(WatchdogTamperCallback cb, void* userData = nullptr);

    // ---- Query ----

    bool isInitialized() const;
    bool isRunning() const;
    WatchdogStatus getStatus() const;
    WatchdogStatusInfo getStatusInfo() const;

    // Copy 32-byte baseline HMAC
    bool getBaselineHMAC(uint8_t* hmac32) const;

    // Human-readable diagnostics string for UI/log
    std::string getDiagnostics() const;

private:
    WatchdogService();
    ~WatchdogService();

    // Non-copyable
    WatchdogService(const WatchdogService&) = delete;
    WatchdogService& operator=(const WatchdogService&) = delete;

    // Background thread entry point (Win32 thread via CreateThread)
    static unsigned long __stdcall threadProc(void* param);
    void threadLoop();

    // ---- State ----
    std::atomic<bool>       m_initialized;
    std::atomic<bool>       m_running;
    std::atomic<bool>       m_stopRequested;
    std::atomic<uint32_t>   m_intervalMs;
    void*                   m_threadHandle;     // HANDLE
    mutable std::mutex      m_mutex;

    // Tamper callback
    WatchdogTamperCallback  m_tamperCb;
    void*                   m_tamperUserData;
};

} // namespace RawrXD
