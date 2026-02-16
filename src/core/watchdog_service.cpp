// ============================================================================
// watchdog_service.cpp — Agentic .text Section Integrity Watchdog Service
// ============================================================================
// Full implementation of the C++ bridge to RawrXD_Watchdog.asm.
// Provides singleton lifecycle, background OS thread for periodic HMAC
// verification, tamper callback dispatch, and diagnostic reporting.
//
// Threading:
//   Uses CreateThread for the background verifier (no std::thread to
//   avoid CRT dependency issues in some link configurations).
//   The thread sleeps in configurable intervals, wakes, calls
//   asm_watchdog_verify(), and dispatches the tamper callback if needed.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "watchdog_service.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace RawrXD {

// ============================================================================
// Constructor / Destructor
// ============================================================================

WatchdogService::WatchdogService()
    : m_initialized(false)
    , m_running(false)
    , m_stopRequested(false)
    , m_intervalMs(30000)
    , m_threadHandle(nullptr)
    , m_tamperCb(nullptr)
    , m_tamperUserData(nullptr)
{}

WatchdogService::~WatchdogService() {
    shutdown();
}

// ============================================================================
// Singleton
// ============================================================================

WatchdogService& WatchdogService::instance() {
    static WatchdogService s_instance;
    return s_instance;
}

// ============================================================================
// Initialize — delegates to asm_watchdog_init
// ============================================================================

WatchdogResult WatchdogService::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized.load()) {
        return WatchdogResult::ok("Already initialized");
    }

    int result = asm_watchdog_init();
    if (result != 0) {
        // The MASM kernel returns the status code in EAX and a detail
        // string in RDX.  We don't have a clean way to capture RDX from
        // C++, so map the status code to a message here.
        const char* msg = "Unknown error";
        switch (static_cast<WatchdogStatus>(result)) {
            case WatchdogStatus::NoKey:         msg = "HMAC key not available (Camellia engine not initialized)"; break;
            case WatchdogStatus::NoTextSection:  msg = ".text section not found in PE"; break;
            case WatchdogStatus::CryptoFailure:  msg = "CryptoAPI HMAC-SHA256 failure"; break;
            case WatchdogStatus::InvalidPE:      msg = "Invalid PE header"; break;
            default: break;
        }
        return WatchdogResult::error(msg, result);
    }

    m_initialized.store(true);

    // Log baseline info
    WatchdogStatusInfo info = {};
    asm_watchdog_get_status(&info);

    std::cout << "[Watchdog] Initialized — .text section at 0x"
              << std::hex << info.textBase << std::dec
              << " (" << (info.textSize / 1024) << " KB)"
              << " — baseline HMAC captured"
              << std::endl;

    return WatchdogResult::ok("Initialized");
}

// ============================================================================
// Start Periodic Verification Thread
// ============================================================================

WatchdogResult WatchdogService::startPeriodicVerification(uint32_t intervalMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized.load()) {
        return WatchdogResult::error("Watchdog not initialized", -1);
    }

    if (m_running.load()) {
        return WatchdogResult::ok("Periodic verification already running");
    }

    m_intervalMs.store(intervalMs);
    m_stopRequested.store(false);

    // Create background thread via Win32 API
    DWORD threadId = 0;
    m_threadHandle = CreateThread(
        nullptr,                    // Security attributes
        0,                          // Default stack size
        &WatchdogService::threadProc,
        this,                       // Param = this
        0,                          // Run immediately
        &threadId
    );

    if (!m_threadHandle) {
        return WatchdogResult::error("CreateThread failed", GetLastError());
    }

    m_running.store(true);

    std::cout << "[Watchdog] Periodic verification started (every "
              << intervalMs << " ms, thread " << threadId << ")"
              << std::endl;

    return WatchdogResult::ok("Periodic verification started");
}

// ============================================================================
// Stop Periodic Verification
// ============================================================================

WatchdogResult WatchdogService::stopPeriodicVerification() {
    if (!m_running.load()) {
        return WatchdogResult::ok("Not running");
    }

    m_stopRequested.store(true);

    // Wait for thread to exit (with timeout)
    if (m_threadHandle) {
        DWORD waitResult = WaitForSingleObject(m_threadHandle, 10000);
        if (waitResult == WAIT_TIMEOUT) {
            // Force terminate as last resort
            TerminateThread(m_threadHandle, 1);
            std::cerr << "[Watchdog] Warning: thread did not exit gracefully, terminated"
                      << std::endl;
        }
        CloseHandle(m_threadHandle);
        m_threadHandle = nullptr;
    }

    m_running.store(false);
    std::cout << "[Watchdog] Periodic verification stopped" << std::endl;

    return WatchdogResult::ok("Stopped");
}

// ============================================================================
// Thread Procedure — Win32 thread entry
// ============================================================================

unsigned long __stdcall WatchdogService::threadProc(void* param) {
    auto* self = static_cast<WatchdogService*>(param);
    self->threadLoop();
    return 0;
}

void WatchdogService::threadLoop() {
    while (!m_stopRequested.load()) {
        // Sleep in small increments so we can respond to stop requests quickly
        uint32_t interval = m_intervalMs.load();
        uint32_t elapsed = 0;
        const uint32_t sleepGranularity = 250;  // 250ms granularity

        while (elapsed < interval && !m_stopRequested.load()) {
            uint32_t sleepTime = (interval - elapsed < sleepGranularity)
                                 ? (interval - elapsed)
                                 : sleepGranularity;
            Sleep(sleepTime);
            elapsed += sleepTime;
        }

        if (m_stopRequested.load()) break;

        // ---- Perform verification ----
        int result = asm_watchdog_verify();

        if (result == 1) {
            // TAMPER DETECTED
            std::cerr << "[Watchdog] *** TAMPER DETECTED *** .text section has been modified!"
                      << std::endl;

            // Dispatch callback
            WatchdogTamperCallback cb = nullptr;
            void* ud = nullptr;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                cb = m_tamperCb;
                ud = m_tamperUserData;
            }

            if (cb) {
                cb(WatchdogStatus::Tampered,
                   ".text section integrity violation detected",
                   ud);
            }

            // Continue running — the callback may choose to take action
            // (e.g., terminate the process) or may just log/alert.
        }
        else if (result < 0) {
            std::cerr << "[Watchdog] Verification error: code " << result << std::endl;
        }
        // result == 0 means OK, no output needed
    }
}

// ============================================================================
// Single Verification
// ============================================================================

WatchdogResult WatchdogService::verifySingle() {
    if (!m_initialized.load()) {
        return WatchdogResult::error("Watchdog not initialized", -1);
    }

    int result = asm_watchdog_verify();

    switch (result) {
        case 0:
            return WatchdogResult::ok("Integrity verified");
        case 1:
            return WatchdogResult::error("TAMPER DETECTED", 1);
        default:
            return WatchdogResult::error("Verification failed", result);
    }
}

// ============================================================================
// Shutdown
// ============================================================================

WatchdogResult WatchdogService::shutdown() {
    // Stop background thread first
    if (m_running.load()) {
        stopPeriodicVerification();
    }

    if (m_initialized.load()) {
        asm_watchdog_shutdown();
        m_initialized.store(false);
        std::cout << "[Watchdog] Shutdown — keying material zeroed" << std::endl;
    }

    return WatchdogResult::ok("Shutdown complete");
}

// ============================================================================
// Configuration
// ============================================================================

void WatchdogService::setVerifyIntervalMs(uint32_t ms) {
    m_intervalMs.store(ms);
}

void WatchdogService::setTamperCallback(WatchdogTamperCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tamperCb = cb;
    m_tamperUserData = userData;
}

// ============================================================================
// Query
// ============================================================================

bool WatchdogService::isInitialized() const {
    return m_initialized.load();
}

bool WatchdogService::isRunning() const {
    return m_running.load();
}

WatchdogStatus WatchdogService::getStatus() const {
    if (!m_initialized.load()) return WatchdogStatus::Uninitialized;

    WatchdogStatusInfo info = {};
    asm_watchdog_get_status(&info);
    return static_cast<WatchdogStatus>(info.status);
}

WatchdogStatusInfo WatchdogService::getStatusInfo() const {
    WatchdogStatusInfo info = {};
    if (m_initialized.load()) {
        asm_watchdog_get_status(&info);
    }
    return info;
}

bool WatchdogService::getBaselineHMAC(uint8_t* hmac32) const {
    if (!hmac32 || !m_initialized.load()) return false;
    return (asm_watchdog_get_baseline(hmac32) == 0);
}

// ============================================================================
// Diagnostics
// ============================================================================

std::string WatchdogService::getDiagnostics() const {
    std::ostringstream ss;
    ss << "=== Watchdog Service Diagnostics ===\n";
    ss << "Initialized: " << (m_initialized.load() ? "Yes" : "No") << "\n";
    ss << "Background Thread: " << (m_running.load() ? "Running" : "Stopped") << "\n";
    ss << "Verify Interval: " << m_intervalMs.load() << " ms\n";
    ss << "Tamper Callback: " << (m_tamperCb ? "Registered" : "None") << "\n";

    if (m_initialized.load()) {
        WatchdogStatusInfo info = {};
        asm_watchdog_get_status(&info);

        const char* statusStr = "Unknown";
        switch (static_cast<WatchdogStatus>(info.status)) {
            case WatchdogStatus::OK:            statusStr = "OK"; break;
            case WatchdogStatus::Tampered:      statusStr = "TAMPERED"; break;
            case WatchdogStatus::NoKey:         statusStr = "No HMAC Key"; break;
            case WatchdogStatus::NoTextSection:  statusStr = "No .text"; break;
            case WatchdogStatus::CryptoFailure:  statusStr = "Crypto Error"; break;
            case WatchdogStatus::InvalidPE:      statusStr = "Invalid PE"; break;
            case WatchdogStatus::Uninitialized:  statusStr = "Uninitialized"; break;
        }

        ss << "Status: " << statusStr << "\n";
        ss << ".text Base: 0x" << std::hex << info.textBase << std::dec << "\n";
        ss << ".text Size: " << (info.textSize / 1024) << " KB\n";
        ss << "Verify Count: " << info.verifyCount << "\n";
        ss << "Tamper Count: " << info.tamperCount << "\n";
        ss << "Last Verify: tick " << info.lastVerifyTick << "\n";

        // Display baseline HMAC
        uint8_t baseline[32];
        if (asm_watchdog_get_baseline(baseline) == 0) {
            ss << "Baseline HMAC: ";
            for (int i = 0; i < 32; ++i) {
                ss << std::hex << std::setfill('0') << std::setw(2)
                   << static_cast<int>(baseline[i]);
            }
            ss << std::dec << "\n";
        }
    }

    return ss.str();
}

} // namespace RawrXD
