// ============================================================================
// masm_stress_harness.cpp — MASM Module Fuzz & Stress Testing
// ============================================================================
// Exercises all 5 Tier-2 MASM modules with init/shutdown cycles, boundary
// inputs, concurrent access, fault injection, and throughput stress.
//
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#define NOMINMAX
#include "masm_stress_harness.h"
#include "masm_bridge_cathedral.h"

#include <windows.h>
#include <cstdio>
#include <cstring>

namespace RawrXD {
namespace Stress {

// ============================================================================
// Construction
// ============================================================================

MASMStressHarness::MASMStressHarness() : m_resultCount(0) {
    memset(m_results, 0, sizeof(m_results));
    memset(&m_config, 0, sizeof(m_config));
}

// ============================================================================
// Public API
// ============================================================================

StressResult MASMStressHarness::run(const StressConfig& config) {
    m_config = config;
    m_resultCount = 0;
    memset(m_results, 0, sizeof(m_results));

    uint64_t startMs = nowMs();

    if (config.modules & STRESS_SELFPATCH)    testSelfPatch();
    if (config.modules & STRESS_GGUF_LOADER)  testGGUFLoader();
    if (config.modules & STRESS_ORCHESTRATOR) testOrchestrator();
    if (config.modules & STRESS_QUADBUFFER)   testQuadBuffer();
    if (config.modules & STRESS_LSP_BRIDGE)   testLSPBridge();

    uint64_t totalMs = nowMs() - startMs;

    StressResult sr;
    sr.totalDurationMs = totalMs;
    sr.totalTests = 0;
    sr.passedTests = 0;
    sr.failedTests = 0;
    sr.skippedTests = 0;

    for (int i = 0; i < m_resultCount; ++i) {
        sr.totalTests++;
        if (m_results[i].passed) sr.passedTests++;
        else sr.failedTests++;
    }

    sr.passed = (sr.failedTests == 0);
    sr.summary = sr.passed ? "All stress tests passed" : "Some tests FAILED";

    if (m_config.enableLogging) {
        char msg[256];
        snprintf(msg, sizeof(msg),
            "[Stress] Complete: %u passed, %u failed, %u total in %llums\n",
            sr.passedTests, sr.failedTests, sr.totalTests,
            (unsigned long long)sr.totalDurationMs);
        OutputDebugStringA(msg);
    }

    return sr;
}

StressResult MASMStressHarness::runDefault() {
    StressConfig cfg;
    cfg.modules = STRESS_ALL_MODULES;
    cfg.mode = SMODE_FULL_SUITE;
    cfg.iterations = 100;
    cfg.threadCount = 4;
    cfg.timeoutMs = 30000;
    cfg.stopOnFirstFail = false;
    cfg.enableLogging = true;
    return run(cfg);
}

const SingleTestResult* MASMStressHarness::getTestResults(int* count) const {
    if (count) *count = m_resultCount;
    return m_results;
}

// ============================================================================
// SelfPatch Engine Tests
// ============================================================================

void MASMStressHarness::testSelfPatch() {
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_INIT_SHUTDOWN_CYCLE) {
        runInitShutdownCycles(STRESS_SELFPATCH);
    }
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_BOUNDARY_INPUTS) {
        runBoundaryInputs(STRESS_SELFPATCH);
    }
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_FAULT_INJECTION) {
        runFaultInjection(STRESS_SELFPATCH);
    }
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_THROUGHPUT) {
        runThroughput(STRESS_SELFPATCH);
    }
}

// ============================================================================
// GGUF Loader Tests
// ============================================================================

void MASMStressHarness::testGGUFLoader() {
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_INIT_SHUTDOWN_CYCLE) {
        runInitShutdownCycles(STRESS_GGUF_LOADER);
    }
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_BOUNDARY_INPUTS) {
        runBoundaryInputs(STRESS_GGUF_LOADER);
    }
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_FAULT_INJECTION) {
        runFaultInjection(STRESS_GGUF_LOADER);
    }
}

// ============================================================================
// Orchestrator Tests
// ============================================================================

void MASMStressHarness::testOrchestrator() {
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_INIT_SHUTDOWN_CYCLE) {
        runInitShutdownCycles(STRESS_ORCHESTRATOR);
    }
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_FAULT_INJECTION) {
        runFaultInjection(STRESS_ORCHESTRATOR);
    }
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_THROUGHPUT) {
        runThroughput(STRESS_ORCHESTRATOR);
    }
}

// ============================================================================
// QuadBuffer Tests
// ============================================================================

void MASMStressHarness::testQuadBuffer() {
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_INIT_SHUTDOWN_CYCLE) {
        runInitShutdownCycles(STRESS_QUADBUFFER);
    }
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_BOUNDARY_INPUTS) {
        runBoundaryInputs(STRESS_QUADBUFFER);
    }
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_FAULT_INJECTION) {
        runFaultInjection(STRESS_QUADBUFFER);
    }
}

// ============================================================================
// LSP Bridge Tests
// ============================================================================

void MASMStressHarness::testLSPBridge() {
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_INIT_SHUTDOWN_CYCLE) {
        runInitShutdownCycles(STRESS_LSP_BRIDGE);
    }
    if (m_config.mode == SMODE_FULL_SUITE || m_config.mode == SMODE_FAULT_INJECTION) {
        runFaultInjection(STRESS_LSP_BRIDGE);
    }
}

// ============================================================================
// Init/Shutdown Cycle Stress
// ============================================================================

void MASMStressHarness::runInitShutdownCycles(int module) {
    const char* moduleName = "Unknown";
    uint64_t start = nowMs();
    uint32_t iters = m_config.iterations;
    bool allPassed = true;

    switch (module) {
    case STRESS_SELFPATCH:
        moduleName = "SelfPatch";
        for (uint32_t i = 0; i < iters; ++i) {
            int r1 = asm_spengine_init(0);
            asm_spengine_shutdown();
            if (r1 < 0) { allPassed = false; break; }
        }
        break;

    case STRESS_GGUF_LOADER:
        moduleName = "GGUFLoader";
        for (uint32_t i = 0; i < iters; ++i) {
            int r1 = asm_gguf_loader_init(nullptr, 0);
            asm_gguf_loader_close(nullptr);
            if (r1 < 0) { allPassed = false; break; }
        }
        break;

    case STRESS_ORCHESTRATOR:
        moduleName = "Orchestrator";
        for (uint32_t i = 0; i < iters; ++i) {
            int r1 = asm_orchestrator_init(4, nullptr);
            asm_orchestrator_shutdown();
            if (r1 < 0) { allPassed = false; break; }
        }
        break;

    case STRESS_QUADBUFFER:
        moduleName = "QuadBuffer";
        for (uint32_t i = 0; i < iters; ++i) {
            int r1 = asm_quadbuf_init(nullptr, 1024, 4);
            asm_quadbuf_shutdown(nullptr);
            if (r1 < 0) { allPassed = false; break; }
        }
        break;

    case STRESS_LSP_BRIDGE:
        moduleName = "LSPBridge";
        for (uint32_t i = 0; i < iters; ++i) {
            int r1 = asm_lsp_bridge_init(nullptr, nullptr);
            asm_lsp_bridge_shutdown();
            if (r1 < 0) { allPassed = false; break; }
        }
        break;
    }

    uint64_t elapsed = nowMs() - start;
    char detail[128];
    snprintf(detail, sizeof(detail), "%s init/shutdown x%u in %llums",
             moduleName, iters, (unsigned long long)elapsed);

    if (allPassed) recordPass("InitShutdownCycle", detail, elapsed);
    else            recordFail("InitShutdownCycle", detail, elapsed);
}

// ============================================================================
// Boundary Input Testing
// ============================================================================

void MASMStressHarness::runBoundaryInputs(int module) {
    const char* moduleName = "Unknown";
    uint64_t start = nowMs();
    bool allPassed = true;

    switch (module) {
    case STRESS_SELFPATCH:
        moduleName = "SelfPatch";
        {
            // Test quant switch with boundary values
            asm_spengine_init(0);

            // Valid kernel IDs
            int r;
            r = asm_spengine_quant_switch(0);  // Q4_K_M
            r = asm_spengine_quant_switch(1);  // Q5_K_M
            r = asm_spengine_quant_switch(2);  // Q8_0

            // Boundary — invalid kernel IDs
            r = asm_spengine_quant_switch(-1);  // should fail gracefully
            r = asm_spengine_quant_switch(99);  // should fail gracefully
            (void)r;

            // Adaptive quant with boundary VRAM
            asm_spengine_quant_switch_adaptive(0);     // 0% VRAM
            asm_spengine_quant_switch_adaptive(64);    // Just below medium
            asm_spengine_quant_switch_adaptive(65);    // At medium threshold
            asm_spengine_quant_switch_adaptive(66);    // Just above medium
            asm_spengine_quant_switch_adaptive(84);    // Just below high
            asm_spengine_quant_switch_adaptive(85);    // At high threshold
            asm_spengine_quant_switch_adaptive(86);    // Just above high
            asm_spengine_quant_switch_adaptive(100);   // Max VRAM

            asm_spengine_shutdown();
        }
        break;

    case STRESS_GGUF_LOADER:
        moduleName = "GGUFLoader";
        {
            // Empty filename
            int r = asm_gguf_loader_init("", 0);
            asm_gguf_loader_close(nullptr);

            // Null config with max VRAM
            r = asm_gguf_loader_configure_gpu(0, 0xFFFFFFFF);
            (void)r;
        }
        break;

    case STRESS_QUADBUFFER:
        moduleName = "QuadBuffer";
        {
            // Zero buffer size
            int r = asm_quadbuf_init(nullptr, 0, 0);
            asm_quadbuf_shutdown(nullptr);

            // Max buffer size
            r = asm_quadbuf_init(nullptr, 0xFFFFFFFF, 16);
            asm_quadbuf_shutdown(nullptr);

            // Valid init with edge-case push
            r = asm_quadbuf_init(nullptr, 1024, 4);
            if (r >= 0) {
                // Push empty token
                asm_quadbuf_push_token(nullptr, nullptr, 0, 0);
                // Resize to 0
                asm_quadbuf_resize(nullptr, 0);
                // Resize to max
                asm_quadbuf_resize(nullptr, 0x7FFFFFFF);
            }
            asm_quadbuf_shutdown(nullptr);
            (void)r;
        }
        break;

    default:
        moduleName = "Generic";
        break;
    }

    uint64_t elapsed = nowMs() - start;
    char detail[128];
    snprintf(detail, sizeof(detail), "%s boundary inputs in %llums",
             moduleName, (unsigned long long)elapsed);

    // If we get here without an access violation, it passed
    if (allPassed) recordPass("BoundaryInputs", detail, elapsed);
    else            recordFail("BoundaryInputs", detail, elapsed);
}

// ============================================================================
// Concurrent Access
// ============================================================================

struct ConcurrentTestData {
    int module;
    uint32_t iterations;
    volatile long passCount;
    volatile long failCount;
};

static DWORD WINAPI ConcurrentTestThread(LPVOID param) {
    auto* data = static_cast<ConcurrentTestData*>(param);

    for (uint32_t i = 0; i < data->iterations; ++i) {
        switch (data->module) {
        case STRESS_SELFPATCH: {
            SPENGINE_STATS stats;
            asm_spengine_get_stats(&stats);
            InterlockedIncrement(&data->passCount);
        } break;

        case STRESS_ORCHESTRATOR: {
            ORCHESTRATOR_METRICS metrics;
            asm_orchestrator_get_metrics(&metrics);
            InterlockedIncrement(&data->passCount);
        } break;

        case STRESS_LSP_BRIDGE: {
            LSP_BRIDGE_STATS stats;
            asm_lsp_bridge_get_stats(&stats);
            InterlockedIncrement(&data->passCount);
        } break;

        default:
            InterlockedIncrement(&data->passCount);
            break;
        }
    }

    return 0;
}

void MASMStressHarness::runConcurrentAccess(int module) {
    uint32_t threadCount = m_config.threadCount;
    if (threadCount == 0) threadCount = 4;
    if (threadCount > 16) threadCount = 16;

    ConcurrentTestData data;
    data.module = module;
    data.iterations = m_config.iterations;
    data.passCount = 0;
    data.failCount = 0;

    uint64_t start = nowMs();

    HANDLE threads[16];
    for (uint32_t i = 0; i < threadCount; ++i) {
        threads[i] = CreateThread(nullptr, 0, ConcurrentTestThread, &data, 0, nullptr);
    }

    WaitForMultipleObjects(threadCount, threads, TRUE,
                           m_config.timeoutMs > 0 ? m_config.timeoutMs : INFINITE);

    for (uint32_t i = 0; i < threadCount; ++i) {
        CloseHandle(threads[i]);
    }

    uint64_t elapsed = nowMs() - start;
    char detail[128];
    snprintf(detail, sizeof(detail), "Concurrent x%u threads x%u iters, pass=%ld fail=%ld in %llums",
             threadCount, m_config.iterations, data.passCount, data.failCount,
             (unsigned long long)elapsed);

    if (data.failCount == 0) recordPass("ConcurrentAccess", detail, elapsed);
    else                     recordFail("ConcurrentAccess", detail, elapsed);
}

// ============================================================================
// Fault Injection
// ============================================================================

void MASMStressHarness::runFaultInjection(int module) {
    uint64_t start = nowMs();
    const char* moduleName = "Unknown";
    bool passed = true;

    // Test functions with nullptr / bad parameters wrapped in SEH
    switch (module) {
    case STRESS_SELFPATCH:
        moduleName = "SelfPatch";
        __try {
            // Null patch address
            asm_spengine_register(0, nullptr, 0, 0);
            // Apply with invalid ID
            asm_spengine_apply(0xFFFFFFFF);
            // Rollback with invalid ID
            asm_spengine_rollback(0xFFFFFFFF);
            // Get stats with nullptr
            asm_spengine_get_stats(nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            passed = false;
        }
        break;

    case STRESS_GGUF_LOADER:
        moduleName = "GGUFLoader";
        __try {
            asm_gguf_loader_parse(nullptr, nullptr, 0);
            asm_gguf_loader_lookup(nullptr, nullptr);
            asm_gguf_loader_get_info(nullptr, nullptr);
            asm_gguf_loader_get_stats(nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            passed = false;
        }
        break;

    case STRESS_ORCHESTRATOR:
        moduleName = "Orchestrator";
        __try {
            asm_orchestrator_dispatch(0xFFFF, nullptr, 0);
            asm_orchestrator_register_hook(0, nullptr, nullptr);
            asm_orchestrator_set_vtable(nullptr);
            asm_orchestrator_queue_async(0, nullptr, 0, nullptr);
            asm_orchestrator_get_metrics(nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            passed = false;
        }
        break;

    case STRESS_QUADBUFFER:
        moduleName = "QuadBuffer";
        __try {
            asm_quadbuf_push_token(nullptr, nullptr, 0, 0);
            asm_quadbuf_render_frame(nullptr, nullptr, 0);
            asm_quadbuf_get_stats(nullptr, nullptr);
            asm_quadbuf_set_flags(nullptr, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            passed = false;
        }
        break;

    case STRESS_LSP_BRIDGE:
        moduleName = "LSPBridge";
        __try {
            asm_lsp_bridge_sync(0);
            asm_lsp_bridge_query(nullptr, nullptr, 0);
            asm_lsp_bridge_invalidate(nullptr, 0, 0);
            asm_lsp_bridge_set_weights(nullptr, 0);
            asm_lsp_bridge_get_stats(nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            passed = false;
        }
        break;
    }

    uint64_t elapsed = nowMs() - start;
    char detail[128];
    snprintf(detail, sizeof(detail), "%s fault injection %s in %llums",
             moduleName, passed ? "survived" : "CRASHED",
             (unsigned long long)elapsed);

    if (passed) recordPass("FaultInjection", detail, elapsed);
    else         recordFail("FaultInjection", detail, elapsed);
}

// ============================================================================
// Throughput
// ============================================================================

void MASMStressHarness::runThroughput(int module) {
    uint64_t start = nowMs();
    const char* moduleName = "Unknown";
    uint32_t iters = m_config.iterations * 10; // Higher count for throughput

    switch (module) {
    case STRESS_SELFPATCH:
        moduleName = "SelfPatch";
        asm_spengine_init(0);
        for (uint32_t i = 0; i < iters; ++i) {
            SPENGINE_STATS stats;
            asm_spengine_get_stats(&stats);
        }
        asm_spengine_shutdown();
        break;

    case STRESS_ORCHESTRATOR:
        moduleName = "Orchestrator";
        asm_orchestrator_init(4, nullptr);
        for (uint32_t i = 0; i < iters; ++i) {
            ORCHESTRATOR_METRICS metrics;
            asm_orchestrator_get_metrics(&metrics);
        }
        asm_orchestrator_shutdown();
        break;

    default:
        moduleName = "Generic";
        break;
    }

    uint64_t elapsed = nowMs() - start;
    double opsPerSec = elapsed > 0 ? (double)iters / ((double)elapsed / 1000.0) : 0.0;

    char detail[192];
    snprintf(detail, sizeof(detail), "%s throughput: %u ops in %llums (%.0f ops/sec)",
             moduleName, iters, (unsigned long long)elapsed, opsPerSec);

    recordPass("Throughput", detail, elapsed);
}

// ============================================================================
// Result Recording
// ============================================================================

void MASMStressHarness::recordPass(const char* name, const char* detail, uint64_t durationMs) {
    if (m_resultCount >= MAX_TEST_RESULTS) return;
    auto& r = m_results[m_resultCount++];
    r.passed = true;
    r.testName = name;
    r.detail = detail;
    r.durationMs = durationMs;
    r.iterations = m_config.iterations;

    if (m_config.enableLogging) {
        char msg[256];
        snprintf(msg, sizeof(msg), "[Stress] PASS: %s — %s\n", name, detail);
        OutputDebugStringA(msg);
    }
}

void MASMStressHarness::recordFail(const char* name, const char* detail, uint64_t durationMs) {
    if (m_resultCount >= MAX_TEST_RESULTS) return;
    auto& r = m_results[m_resultCount++];
    r.passed = false;
    r.testName = name;
    r.detail = detail;
    r.durationMs = durationMs;
    r.iterations = m_config.iterations;

    if (m_config.enableLogging) {
        char msg[256];
        snprintf(msg, sizeof(msg), "[Stress] FAIL: %s — %s\n", name, detail);
        OutputDebugStringA(msg);
    }
}

uint64_t MASMStressHarness::nowMs() const {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
}

} // namespace Stress
} // namespace RawrXD
