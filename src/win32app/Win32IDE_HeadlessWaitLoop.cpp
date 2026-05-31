// ============================================================================
// Win32IDE_HeadlessWaitLoop.cpp
// ============================================================================
// Critical Fix 2: Headless /Health Readiness
// Implements the runHeadlessWaitLoop() and ensures the /health endpoint is
// properly initialized before the server becomes ready. Prevents immediate
// process termination that was causing contract test suites to timeout.
//
// Key insight: The headless server must pump a message loop or use a
// synchronization primitive to stay alive while serving HTTP requests.
// ============================================================================

#include "Win32IDE.h"
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>

namespace {

// ---- Global headless server state ----
std::atomic<bool> g_headlessServerReady(false);
std::atomic<bool> g_headlessServerShutdown(false);
std::condition_variable g_headlessServerCV;
std::mutex g_headlessServerMutex;

}  // namespace


void Win32IDE::runHeadlessWaitLoop()
{
    // ========================================================================
    // CRITICAL FIX 2: Headless /Health Readiness
    // ========================================================================
    // After startLocalServer() is called, we must signal readiness and then
    // pump a lightweight message loop to prevent immediate process exit.
    // This is the main thread; we use Windows message pump for portability.
    // ========================================================================

    if (!m_isHeadless) {
        OutputDebugStringA("[HeadlessWaitLoop] ERROR: runHeadlessWaitLoop() called in GUI mode!\n");
        return;
    }

    fprintf(stdout, "[RawrXD] Entering headless wait loop (PID: %u)...\n", ::GetCurrentProcessId());
    fflush(stdout);

    // Step 1: Ensure the local server is actually listening
    if (!m_localServerRunning.load()) {
        fprintf(stderr, "[RawrXD] ERROR: Local server not running. Starting now...\n");
        try {
            startLocalServer();  // Blocking start
        } catch (const std::exception& e) {
            fprintf(stderr, "[RawrXD] FATAL: Could not start local server: %s\n", e.what());
            return;
        }
        
        // Give it a moment to stabilize
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Step 2: Mark server as ready for health checks
    {
        std::lock_guard<std::mutex> lock(g_headlessServerMutex);
        g_headlessServerReady.store(true, std::memory_order_release);
    }
    g_headlessServerCV.notify_all();

    fprintf(stdout, "[RawrXD] Headless server ready for health checks on port 11435.\n");
    fflush(stdout);

    // Step 3: Enter minimal message loop
    // This prevents the process from exiting while HTTP requests arrive.
    // We use Windows' built-in message pump for event processing.
    MSG msg = {};
    while (!g_headlessServerShutdown.load(std::memory_order_acquire)) {
        // Use PeekMessage with timeout to avoid 100% CPU
        BOOL bRet = ::PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE);
        
        if (bRet) {
            ::TranslateMessage(&msg);
            ::DispatchMessageA(&msg);
            
            // WM_QUIT will exit the loop
            if (msg.message == WM_QUIT) {
                break;
            }
        } else {
            // No message available; sleep briefly to yield CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    fprintf(stdout, "[RawrXD] Headless wait loop exiting (PID: %u).\n", ::GetCurrentProcessId());
    fflush(stdout);
}


void Win32IDE::initializeHeadlessServer()
{
    // ========================================================================
    // Headless Server Initialization
    // Called from WinMain -> main() when --headless flag is set.
    // Ensures all subsystems are ready before the wait loop starts.
    // ========================================================================

    if (!m_isHeadless) {
        return;  // Not in headless mode
    }

    fprintf(stdout, "[RawrXD] Initializing headless server mode...\n");
    fflush(stdout);

    // Phase 1: Initialize core inference (model loading, etc.)
    try {
        if (!initializeInference()) {
            fprintf(stderr, "[RawrXD] ERROR: Failed to initialize inference engine.\n");
            return;
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[RawrXD] ERROR during inference init: %s\n", e.what());
        return;
    }

    fprintf(stdout, "[RawrXD] Inference engine initialized.\n");
    fflush(stdout);

    // Phase 2: Start the local HTTP server (but don't wait for it yet)
    try {
        startLocalServer();  // Spawns background thread
        
        // Give it a moment to bind to the port
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    } catch (const std::exception& e) {
        fprintf(stderr, "[RawrXD] ERROR starting local server: %s\n", e.what());
        return;
    }

    fprintf(stdout, "[RawrXD] Local HTTP server started (listening on port 11435).\n");
    fflush(stdout);

    // Phase 3: Load default model if configured
    if (!m_loadedModelPath.empty()) {
        fprintf(stdout, "[RawrXD] Loading model: %s\n", m_loadedModelPath.c_str());
        fflush(stdout);
        
        try {
            if (!loadModelForInference(m_loadedModelPath)) {
                fprintf(stderr, "[RawrXD] WARNING: Model load failed, continuing without model.\n");
            } else {
                fprintf(stdout, "[RawrXD] Model loaded successfully.\n");
                fflush(stdout);
            }
        } catch (const std::exception& e) {
            fprintf(stderr, "[RawrXD] WARNING: Exception loading model: %s\n", e.what());
        }
    } else {
        fprintf(stdout, "[RawrXD] No default model configured (use --model flag).\n");
        fflush(stdout);
    }

    // Phase 4: Mark initialization complete and enter wait loop
    fprintf(stdout, "[RawrXD] Headless initialization complete. Entering event loop...\n");
    fflush(stdout);

    // This blocks until shutdown signal
    runHeadlessWaitLoop();
}


void Win32IDE::signalHeadlessServerShutdown()
{
    // Called when the server should gracefully shut down
    g_headlessServerShutdown.store(true, std::memory_order_release);
    
    fprintf(stdout, "[RawrXD] Shutdown signal received.\n");
    fflush(stdout);
    
    // Stop the local server
    try {
        stopLocalServer();
    } catch (...) {
    }
}


bool Win32IDE::isHeadlessServerReady() const
{
    // Query whether the server is ready to serve /health checks
    return g_headlessServerReady.load(std::memory_order_acquire);
}
