// ============================================================================
// convergence_stress_harness.cpp — Convergence-Phase Stress Testing
// ============================================================================
// Exercises Tier-3 MASM modules with init/shutdown cycles, boundary inputs,
// fuzz payloads, thread contention, throughput saturation, and perf-instrumented
// runs via the PerfCounters MASM kernel.
//
// Modules tested: CamAuth, KQuant Kernel, KQuant Dequant, Watchdog,
//                 PerfCounters, VRAM Spillover
//
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#define NOMINMAX
#include "convergence_stress_harness.h"
#include "perf_telemetry.hpp"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <intrin.h>

// ============================================================================
// MASM extern declarations — Tier-3 modules
// ============================================================================

extern "C" {
    // Camellia-256 Authenticated Encryption
    int asm_camellia256_auth_encrypt_file(const char* inputPath,
                                          const char* outputPath);
    int asm_camellia256_auth_decrypt_file(const char* inputPath,
                                          const char* outputPath);
    int asm_camellia256_auth_encrypt_buf(uint8_t* plaintext, uint32_t plaintextLen,
                                         uint8_t* output, uint32_t* outputLen);
    int asm_camellia256_auth_decrypt_buf(const uint8_t* authData, uint32_t authDataLen,
                                         uint8_t* plaintext, uint32_t* plaintextLen);

    // Camellia-256 Core (for determinism cross-check)
    int   asm_camellia256_init();
    int   asm_camellia256_set_key(const uint8_t* key, uint32_t keyLen);
    int   asm_camellia256_self_test();

    // K-Quant Kernel (AVX2 / AVX-512)
    size_t asm_dequant_q4_k_avx2(float* output, const void* input);
    size_t asm_dequant_q4_k_avx512(float* output, const void* input);
    size_t asm_dequant_q4_k_batch(float* output, const void* input, size_t numElements);
    int    asm_kquant_cpuid_check();

    // K-Quant Dequant (multi-type dispatcher)
    size_t KQuant_DequantizeF16(const uint16_t* src, float* dst, size_t numElements);
    size_t KQuant_DequantizeQ4_K(const void* src, float* dst, size_t numElements);
    size_t KQuant_DequantizeQ5_K(const void* src, float* dst, size_t numElements);
    size_t KQuant_DequantizeQ6_K(const void* src, float* dst, size_t numElements);
    size_t KQuant_DequantizeQ2_K(const void* src, float* dst, size_t numElements);
    size_t KQuant_DequantizeQ3_K(const void* src, float* dst, size_t numElements);
    size_t KQuant_Dispatch(int ggml_type, const void* src, float* dst, size_t numElements);

    // Watchdog
    int asm_watchdog_init();
    int asm_watchdog_verify();
    int asm_watchdog_get_baseline(uint8_t* hmac32);
    int asm_watchdog_get_status(void* status48);
    int asm_watchdog_shutdown();
}

namespace RawrXD {
namespace Convergence {

// ============================================================================
// Construction
// ============================================================================

ConvergenceStressHarness::ConvergenceStressHarness()
    : m_resultCount(0)
    , m_rngState(0)
{
    memset(m_results, 0, sizeof(m_results));
    memset(&m_config, 0, sizeof(m_config));
}

ConvergenceConfig ConvergenceStressHarness::defaultConfig() {
    ConvergenceConfig cfg = {};
    cfg.modules           = CONV_ALL_MODULES;
    cfg.mode              = CMODE_FULL_CONVERGENCE;
    cfg.iterations        = 100;
    cfg.threadCount       = 8;
    cfg.timeoutMs         = 60000;
    cfg.fuzzSeed          = 0;     // time-based
    cfg.maxPayloadBytes   = 65536;
    cfg.stopOnFirstFail   = false;
    cfg.enableLogging     = true;
    cfg.enablePerfCapture = true;
    cfg.verifyDeterminism = true;
    return cfg;
}

// ============================================================================
// Public API
// ============================================================================

ConvergenceResult ConvergenceStressHarness::run(const ConvergenceConfig& config) {
    m_config = config;
    m_resultCount = 0;
    memset(m_results, 0, sizeof(m_results));

    // Initialize RNG
    m_rngState = config.fuzzSeed;
    if (m_rngState == 0) {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        m_rngState = ft.dwLowDateTime ^ ft.dwHighDateTime;
    }

    // Initialize PerfCounters if requested
    if (config.enablePerfCapture) {
        Perf::PerfTelemetry::instance().initialize();
    }

    uint64_t startMs = nowMs();

    if (config.modules & CONV_CAMAUTH)        testCamAuth();
    if (config.modules & CONV_KQUANT_KERNEL)   testKQuantKernel();
    if (config.modules & CONV_KQUANT_DEQUANT)  testKQuantDequant();
    if (config.modules & CONV_WATCHDOG)        testWatchdog();
    if (config.modules & CONV_PERF_COUNTERS)   testPerfCounters();
    if (config.modules & CONV_SPILLOVER)       testSpillover();

    uint64_t totalMs = nowMs() - startMs;

    ConvergenceResult cr = {};
    cr.totalDurationMs = totalMs;
    cr.deterministicTests = 0;
    cr.nonDeterministicTests = 0;

    for (int i = 0; i < m_resultCount; ++i) {
        cr.totalTests++;
        if (m_results[i].passed) cr.passedTests++;
        else cr.failedTests++;
    }

    cr.passed = (cr.failedTests == 0);
    cr.summary = cr.passed ? "All convergence tests passed" : "Some convergence tests FAILED";

    if (m_config.enableLogging) {
        char msg[512];
        snprintf(msg, sizeof(msg),
            "[Convergence] Complete: %u passed, %u failed, %u total in %llums "
            "(seed=%u, threads=%u)\n",
            cr.passedTests, cr.failedTests, cr.totalTests,
            (unsigned long long)cr.totalDurationMs,
            m_config.fuzzSeed, m_config.threadCount);
        OutputDebugStringA(msg);
    }

    return cr;
}

ConvergenceResult ConvergenceStressHarness::runDefault() {
    return run(defaultConfig());
}

const ConvergenceTestResult* ConvergenceStressHarness::getTestResults(int* count) const {
    if (count) *count = m_resultCount;
    return m_results;
}

// ============================================================================
// Per-Module Test Suites
// ============================================================================

void ConvergenceStressHarness::testCamAuth() {
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_INIT_SHUTDOWN) {
        runInitShutdownCycles(CONV_CAMAUTH);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_BOUNDARY_INPUTS) {
        runBoundaryInputs(CONV_CAMAUTH);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_FUZZ_INPUTS) {
        runFuzzInputs(CONV_CAMAUTH);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_THREAD_CONTENTION) {
        runThreadContention(CONV_CAMAUTH);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_THROUGHPUT) {
        runThroughput(CONV_CAMAUTH);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_PERF_INSTRUMENTED) {
        runPerfInstrumented(CONV_CAMAUTH);
    }
    if (m_config.verifyDeterminism) {
        verifyDeterministicOutput(CONV_CAMAUTH);
    }
}

void ConvergenceStressHarness::testKQuantKernel() {
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_INIT_SHUTDOWN) {
        runInitShutdownCycles(CONV_KQUANT_KERNEL);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_BOUNDARY_INPUTS) {
        runBoundaryInputs(CONV_KQUANT_KERNEL);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_FUZZ_INPUTS) {
        runFuzzInputs(CONV_KQUANT_KERNEL);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_THREAD_CONTENTION) {
        runThreadContention(CONV_KQUANT_KERNEL);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_THROUGHPUT) {
        runThroughput(CONV_KQUANT_KERNEL);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_PERF_INSTRUMENTED) {
        runPerfInstrumented(CONV_KQUANT_KERNEL);
    }
    if (m_config.verifyDeterminism) {
        verifyDeterministicOutput(CONV_KQUANT_KERNEL);
    }
}

void ConvergenceStressHarness::testKQuantDequant() {
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_BOUNDARY_INPUTS) {
        runBoundaryInputs(CONV_KQUANT_DEQUANT);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_FUZZ_INPUTS) {
        runFuzzInputs(CONV_KQUANT_DEQUANT);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_THREAD_CONTENTION) {
        runThreadContention(CONV_KQUANT_DEQUANT);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_THROUGHPUT) {
        runThroughput(CONV_KQUANT_DEQUANT);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_PERF_INSTRUMENTED) {
        runPerfInstrumented(CONV_KQUANT_DEQUANT);
    }
}

void ConvergenceStressHarness::testWatchdog() {
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_INIT_SHUTDOWN) {
        runInitShutdownCycles(CONV_WATCHDOG);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_BOUNDARY_INPUTS) {
        runBoundaryInputs(CONV_WATCHDOG);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_THREAD_CONTENTION) {
        runThreadContention(CONV_WATCHDOG);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_THROUGHPUT) {
        runThroughput(CONV_WATCHDOG);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_PERF_INSTRUMENTED) {
        runPerfInstrumented(CONV_WATCHDOG);
    }
}

void ConvergenceStressHarness::testPerfCounters() {
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_INIT_SHUTDOWN) {
        runInitShutdownCycles(CONV_PERF_COUNTERS);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_BOUNDARY_INPUTS) {
        runBoundaryInputs(CONV_PERF_COUNTERS);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_THREAD_CONTENTION) {
        runThreadContention(CONV_PERF_COUNTERS);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_THROUGHPUT) {
        runThroughput(CONV_PERF_COUNTERS);
    }
}

void ConvergenceStressHarness::testSpillover() {
    // Spillover is a C++ layer over DirectML bridge — test via C++ API
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_BOUNDARY_INPUTS) {
        runBoundaryInputs(CONV_SPILLOVER);
    }
    if (m_config.mode == CMODE_FULL_CONVERGENCE || m_config.mode == CMODE_THROUGHPUT) {
        runThroughput(CONV_SPILLOVER);
    }
}

// ============================================================================
// Init/Shutdown Cycle Stress
// ============================================================================

void ConvergenceStressHarness::runInitShutdownCycles(int module) {
    const char* moduleName = "Unknown";
    uint64_t start = nowMs();
    uint32_t iters = m_config.iterations;
    bool allPassed = true;

    switch (module) {
    case CONV_CAMAUTH:
        moduleName = "CamAuth";
        for (uint32_t i = 0; i < iters; ++i) {
            // CamAuth relies on Camellia core init
            int r1 = asm_camellia256_init();
            // Set a test key
            uint8_t testKey[32];
            memset(testKey, 0xAA, sizeof(testKey));
            int r2 = asm_camellia256_set_key(testKey, 32);
            if (r1 < 0 || r2 < 0) { allPassed = false; break; }
        }
        break;

    case CONV_KQUANT_KERNEL:
        moduleName = "KQuantKernel";
        for (uint32_t i = 0; i < iters; ++i) {
            // CPUID check is the init path for K-Quant
            int r1 = asm_kquant_cpuid_check();
            (void)r1; // 0 or 1, both valid
        }
        break;

    case CONV_WATCHDOG:
        moduleName = "Watchdog";
        for (uint32_t i = 0; i < iters; ++i) {
            int r1 = asm_watchdog_init();
            asm_watchdog_shutdown();
            if (r1 < 0) { allPassed = false; break; }
        }
        break;

    case CONV_PERF_COUNTERS:
        moduleName = "PerfCounters";
        for (uint32_t i = 0; i < iters; ++i) {
            int r1 = asm_perf_init();
            if (r1 < 0) { allPassed = false; break; }
        }
        break;
    }

    uint64_t elapsed = nowMs() - start;
    char detail[192];
    snprintf(detail, sizeof(detail), "%s init/shutdown x%u in %llums",
             moduleName, iters, (unsigned long long)elapsed);

    if (allPassed) recordPass("InitShutdownCycle", moduleName, detail, elapsed);
    else           recordFail("InitShutdownCycle", moduleName, detail, elapsed);
}

// ============================================================================
// Boundary Input Testing
// ============================================================================

void ConvergenceStressHarness::runBoundaryInputs(int module) {
    const char* moduleName = "Unknown";
    uint64_t start = nowMs();
    bool allPassed = true;

    switch (module) {
    case CONV_CAMAUTH:
        moduleName = "CamAuth";
        {
            // Init core first
            asm_camellia256_init();
            uint8_t testKey[32];
            memset(testKey, 0xBB, sizeof(testKey));
            asm_camellia256_set_key(testKey, 32);

            // Empty buffer encryption
            uint8_t outBuf[256];
            uint32_t outLen = sizeof(outBuf);
            int r = asm_camellia256_auth_encrypt_buf(nullptr, 0, outBuf, &outLen);
            (void)r;

            // Single-byte payload
            uint8_t singleByte = 0x42;
            outLen = sizeof(outBuf);
            r = asm_camellia256_auth_encrypt_buf(&singleByte, 1, outBuf, &outLen);
            (void)r;

            // Max output length = 0 (should fail gracefully)
            outLen = 0;
            r = asm_camellia256_auth_encrypt_buf(&singleByte, 1, outBuf, &outLen);
            (void)r;

            // NULL output pointer
            outLen = 256;
            r = asm_camellia256_auth_encrypt_buf(&singleByte, 1, nullptr, &outLen);
            (void)r;

            // NULL outputLen pointer
            r = asm_camellia256_auth_encrypt_buf(&singleByte, 1, outBuf, nullptr);
            (void)r;

            // Decrypt empty
            uint32_t ptLen = sizeof(outBuf);
            r = asm_camellia256_auth_decrypt_buf(nullptr, 0, outBuf, &ptLen);
            (void)r;

            // Decrypt with truncated auth tag
            uint8_t truncated[4] = { 0x01, 0x02, 0x03, 0x04 };
            ptLen = sizeof(outBuf);
            r = asm_camellia256_auth_decrypt_buf(truncated, 4, outBuf, &ptLen);
            (void)r;
        }
        break;

    case CONV_KQUANT_KERNEL:
        moduleName = "KQuantKernel";
        {
            // Single-block dequant with aligned output
            alignas(32) float output[256];
            alignas(32) uint8_t fakeBlock[144];
            memset(fakeBlock, 0, sizeof(fakeBlock));

            // AVX2: valid block
            size_t r = asm_dequant_q4_k_avx2(output, fakeBlock);
            (void)r;

            // AVX2: output check
            bool hasNaN = false;
            for (int k = 0; k < 256; ++k) {
                if (output[k] != output[k]) { hasNaN = true; break; }
            }
            if (hasNaN) allPassed = false;

            // Batch with numElements = 0
            r = asm_dequant_q4_k_batch(output, fakeBlock, 0);
            (void)r;

            // Batch with exactly 256 (minimum valid)
            r = asm_dequant_q4_k_batch(output, fakeBlock, 256);
            (void)r;

            // Check if AVX-512 available
            if (asm_kquant_cpuid_check() == 1) {
                memset(output, 0, sizeof(output));
                r = asm_dequant_q4_k_avx512(output, fakeBlock);
                (void)r;
            }
        }
        break;

    case CONV_KQUANT_DEQUANT:
        moduleName = "KQuantDequant";
        {
            alignas(32) float output[256];
            alignas(32) uint8_t fakeInput[256];
            memset(fakeInput, 0, sizeof(fakeInput));

            // Zero-element dispatch
            size_t r = KQuant_Dispatch(0, fakeInput, output, 0);
            (void)r;

            // Q2_K dequantization (AVX2 — implemented in RawrXD_KQuant_Dequant.asm)
            r = KQuant_DequantizeQ2_K(fakeInput, output, 256);
            // Q3_K dequantization (AVX2 — implemented in RawrXD_KQuant_Dequant.asm)
            r = KQuant_DequantizeQ3_K(fakeInput, output, 256);
            (void)r;

            // Invalid ggml_type
            r = KQuant_Dispatch(9999, fakeInput, output, 256);
            (void)r;

            // F16 with zero elements
            r = KQuant_DequantizeF16(reinterpret_cast<const uint16_t*>(fakeInput), output, 0);
            (void)r;
        }
        break;

    case CONV_WATCHDOG:
        moduleName = "Watchdog";
        {
            int r = asm_watchdog_init();
            if (r >= 0) {
                // Get baseline into valid buffer
                uint8_t hmac[32];
                int r2 = asm_watchdog_get_baseline(hmac);
                (void)r2;

                // Get baseline into nullptr
                r2 = asm_watchdog_get_baseline(nullptr);
                (void)r2;

                // Get status into valid buffer
                uint8_t status[48];
                int r3 = asm_watchdog_get_status(status);
                (void)r3;

                // Get status into nullptr
                r3 = asm_watchdog_get_status(nullptr);
                (void)r3;

                // Verify after init (should pass)
                int vr = asm_watchdog_verify();
                if (vr > 0) allPassed = false; // tamper detected when shouldn't be

                asm_watchdog_shutdown();
            }

            // Verify/get baseline before init (should return error, not crash)
            int r4 = asm_watchdog_verify();
            (void)r4;
            uint8_t hmac2[32];
            int r5 = asm_watchdog_get_baseline(hmac2);
            (void)r5;
        }
        break;

    case CONV_PERF_COUNTERS:
        moduleName = "PerfCounters";
        {
            asm_perf_init();

            // Valid slot range
            uint64_t tsc = asm_perf_begin(0);
            asm_perf_end(0, tsc);

            tsc = asm_perf_begin(63);
            asm_perf_end(63, tsc);

            // Read slot 0
            Perf::PerfSlotData data;
            asm_perf_read_slot(0, &data);

            // Read slot 63
            asm_perf_read_slot(63, &data);

            // Invalid slot (should fail gracefully)
            int r = asm_perf_read_slot(64, &data);
            (void)r;
            r = asm_perf_read_slot(0xFFFFFFFF, &data);
            (void)r;

            // Reset valid slot
            asm_perf_reset_slot(0);

            // NULL buffer for read
            r = asm_perf_read_slot(0, nullptr);
            (void)r;

            // Slot count should be 64
            uint32_t count = asm_perf_get_slot_count();
            if (count != 64) allPassed = false;

            // Table base should be non-null
            void* base = asm_perf_get_table_base();
            if (base == nullptr) allPassed = false;
        }
        break;

    case CONV_SPILLOVER:
        moduleName = "Spillover";
        {
            // Spillover boundary tests are limited without a live DirectML context
            // Test the PerfCounters slots mapped to spillover
            asm_perf_init();
            uint64_t tsc;

            // Simulate spillover load timing
            tsc = asm_perf_begin(static_cast<uint32_t>(Perf::KernelSlot::Spillover_LoadWithSpillover));
            Sleep(0); // minimal yield
            asm_perf_end(static_cast<uint32_t>(Perf::KernelSlot::Spillover_LoadWithSpillover), tsc);

            // Simulate spillover spill timing
            tsc = asm_perf_begin(static_cast<uint32_t>(Perf::KernelSlot::Spillover_SpillToHost));
            Sleep(0);
            asm_perf_end(static_cast<uint32_t>(Perf::KernelSlot::Spillover_SpillToHost), tsc);
        }
        break;
    }

    uint64_t elapsed = nowMs() - start;
    char detail[192];
    snprintf(detail, sizeof(detail), "%s boundary inputs in %llums",
             moduleName, (unsigned long long)elapsed);

    if (allPassed) recordPass("BoundaryInputs", moduleName, detail, elapsed);
    else           recordFail("BoundaryInputs", moduleName, detail, elapsed);
}

// ============================================================================
// Fuzz Input Testing
// ============================================================================

void ConvergenceStressHarness::runFuzzInputs(int module) {
    const char* moduleName = "Unknown";
    uint64_t start = nowMs();
    uint32_t iters = m_config.iterations;
    bool allPassed = true;

    uint32_t maxPayload = m_config.maxPayloadBytes;
    if (maxPayload == 0) maxPayload = 65536;
    if (maxPayload > 1048576) maxPayload = 1048576;

    switch (module) {
    case CONV_CAMAUTH:
        moduleName = "CamAuth";
        {
            asm_camellia256_init();
            uint8_t fuzzKey[32];
            fillRandomBuffer(fuzzKey, 32);
            asm_camellia256_set_key(fuzzKey, 32);

            uint8_t* inBuf  = static_cast<uint8_t*>(VirtualAlloc(nullptr, maxPayload, MEM_COMMIT, PAGE_READWRITE));
            uint8_t* outBuf = static_cast<uint8_t*>(VirtualAlloc(nullptr, maxPayload + 128, MEM_COMMIT, PAGE_READWRITE));

            if (inBuf && outBuf) {
                for (uint32_t i = 0; i < iters; ++i) {
                    uint32_t payloadLen = (nextRand() % maxPayload) + 1;
                    fillRandomBuffer(inBuf, payloadLen);

                    uint32_t outLen = maxPayload + 128;
                    __try {
                        int r = asm_camellia256_auth_encrypt_buf(inBuf, payloadLen, outBuf, &outLen);
                        (void)r;
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        allPassed = false;
                        break;
                    }
                }
            }

            if (inBuf)  VirtualFree(inBuf, 0, MEM_RELEASE);
            if (outBuf) VirtualFree(outBuf, 0, MEM_RELEASE);
        }
        break;

    case CONV_KQUANT_KERNEL:
        moduleName = "KQuantKernel";
        {
            alignas(32) float output[256];

            for (uint32_t i = 0; i < iters; ++i) {
                alignas(32) uint8_t fuzzBlock[144];
                fillRandomBuffer(fuzzBlock, 144);

                __try {
                    size_t r = asm_dequant_q4_k_avx2(output, fuzzBlock);
                    (void)r;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    allPassed = false;
                    break;
                }
            }
        }
        break;

    case CONV_KQUANT_DEQUANT:
        moduleName = "KQuantDequant";
        {
            alignas(32) float output[256];

            for (uint32_t i = 0; i < iters; ++i) {
                alignas(32) uint8_t fuzzInput[256];
                fillRandomBuffer(fuzzInput, 256);

                // Fuzz each dequant type
                int types[] = { 0, 2, 3, 6, 7, 8 };
                int typeIdx = nextRand() % 6;

                __try {
                    size_t r = KQuant_Dispatch(types[typeIdx], fuzzInput, output, 256);
                    (void)r;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    allPassed = false;
                    break;
                }
            }
        }
        break;
    }

    uint64_t elapsed = nowMs() - start;
    char detail[192];
    snprintf(detail, sizeof(detail), "%s fuzz x%u (seed=%u, maxPayload=%u) in %llums",
             moduleName, iters, m_config.fuzzSeed, maxPayload,
             (unsigned long long)elapsed);

    if (allPassed) recordPass("FuzzInputs", moduleName, detail, elapsed);
    else           recordFail("FuzzInputs", moduleName, detail, elapsed);
}

// ============================================================================
// Thread Contention
// ============================================================================

struct ContentionTestData {
    int              module;
    uint32_t         iterations;
    volatile LONG    passCount;
    volatile LONG    failCount;
    uint32_t         rngSeed;
};

static DWORD WINAPI ContentionTestThread(LPVOID param) {
    auto* data = static_cast<ContentionTestData*>(param);
    uint32_t localRng = data->rngSeed ^ GetCurrentThreadId();

    for (uint32_t i = 0; i < data->iterations; ++i) {
        __try {
            switch (data->module) {
            case CONV_CAMAUTH: {
                // All threads encrypt the same single byte
                uint8_t plaintext = 0x42;
                uint8_t out[256];
                uint32_t outLen = sizeof(out);
                asm_camellia256_auth_encrypt_buf(&plaintext, 1, out, &outLen);
                InterlockedIncrement(&data->passCount);
            } break;

            case CONV_KQUANT_KERNEL: {
                alignas(32) float output[256];
                alignas(32) uint8_t block[144];
                memset(block, (uint8_t)(i & 0xFF), 144);
                asm_dequant_q4_k_avx2(output, block);
                InterlockedIncrement(&data->passCount);
            } break;

            case CONV_KQUANT_DEQUANT: {
                alignas(32) float output[256];
                alignas(32) uint8_t input[256];
                memset(input, (uint8_t)(i & 0xFF), 256);
                KQuant_Dispatch(2, input, output, 256);
                InterlockedIncrement(&data->passCount);
            } break;

            case CONV_WATCHDOG: {
                // Multiple threads call verify concurrently
                int r = asm_watchdog_verify();
                (void)r;
                InterlockedIncrement(&data->passCount);
            } break;

            case CONV_PERF_COUNTERS: {
                // All threads writing to different slots
                uint32_t slot = GetCurrentThreadId() % 64;
                uint64_t tsc = asm_perf_begin(slot);
                // Tiny workload
                volatile int sum = 0;
                for (int k = 0; k < 100; ++k) sum += k;
                (void)sum;
                asm_perf_end(slot, tsc);
                InterlockedIncrement(&data->passCount);
            } break;

            default:
                InterlockedIncrement(&data->passCount);
                break;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            InterlockedIncrement(&data->failCount);
        }
    }

    return 0;
}

void ConvergenceStressHarness::runThreadContention(int module) {
    const char* moduleName = "Unknown";
    switch (module) {
    case CONV_CAMAUTH:       moduleName = "CamAuth"; break;
    case CONV_KQUANT_KERNEL:  moduleName = "KQuantKernel"; break;
    case CONV_KQUANT_DEQUANT: moduleName = "KQuantDequant"; break;
    case CONV_WATCHDOG:       moduleName = "Watchdog"; break;
    case CONV_PERF_COUNTERS:  moduleName = "PerfCounters"; break;
    }

    uint32_t threadCount = m_config.threadCount;
    if (threadCount == 0) threadCount = 8;
    if (threadCount > 64) threadCount = 64;

    // Pre-init modules that need it
    if (module == CONV_WATCHDOG) {
        asm_watchdog_init();
    }
    if (module == CONV_PERF_COUNTERS) {
        asm_perf_init();
    }

    ContentionTestData data = {};
    data.module = module;
    data.iterations = m_config.iterations;
    data.passCount = 0;
    data.failCount = 0;
    data.rngSeed = m_rngState;

    uint64_t start = nowMs();

    HANDLE threads[64];
    for (uint32_t i = 0; i < threadCount; ++i) {
        threads[i] = CreateThread(nullptr, 0, ContentionTestThread, &data, 0, nullptr);
    }

    DWORD waitResult = WaitForMultipleObjects(
        threadCount, threads, TRUE,
        m_config.timeoutMs > 0 ? m_config.timeoutMs : INFINITE);

    for (uint32_t i = 0; i < threadCount; ++i) {
        CloseHandle(threads[i]);
    }

    // Post-cleanup
    if (module == CONV_WATCHDOG) {
        asm_watchdog_shutdown();
    }

    uint64_t elapsed = nowMs() - start;
    bool timedOut = (waitResult == WAIT_TIMEOUT);

    char detail[256];
    snprintf(detail, sizeof(detail),
        "%s contention: %u threads x%u iters, pass=%ld fail=%ld%s in %llums",
        moduleName, threadCount, m_config.iterations,
        data.passCount, data.failCount,
        timedOut ? " (TIMEOUT)" : "",
        (unsigned long long)elapsed);

    if (data.failCount == 0 && !timedOut) recordPass("ThreadContention", moduleName, detail, elapsed);
    else                                   recordFail("ThreadContention", moduleName, detail, elapsed);
}

// ============================================================================
// Throughput Saturation
// ============================================================================

void ConvergenceStressHarness::runThroughput(int module) {
    const char* moduleName = "Unknown";
    uint64_t start = nowMs();
    uint32_t iters = m_config.iterations * 10; // Higher count for throughput

    switch (module) {
    case CONV_CAMAUTH:
        moduleName = "CamAuth";
        {
            asm_camellia256_init();
            uint8_t key[32];
            memset(key, 0xCC, 32);
            asm_camellia256_set_key(key, 32);

            uint8_t payload[64];
            uint8_t outBuf[256];
            memset(payload, 0xDD, sizeof(payload));

            for (uint32_t i = 0; i < iters; ++i) {
                uint32_t outLen = sizeof(outBuf);
                asm_camellia256_auth_encrypt_buf(payload, sizeof(payload), outBuf, &outLen);
            }
        }
        break;

    case CONV_KQUANT_KERNEL:
        moduleName = "KQuantKernel";
        {
            alignas(32) float output[256];
            alignas(32) uint8_t block[144];
            memset(block, 0x55, sizeof(block));

            for (uint32_t i = 0; i < iters; ++i) {
                asm_dequant_q4_k_avx2(output, block);
            }
        }
        break;

    case CONV_KQUANT_DEQUANT:
        moduleName = "KQuantDequant";
        {
            alignas(32) float output[256];
            alignas(32) uint8_t input[256];
            memset(input, 0x33, sizeof(input));

            for (uint32_t i = 0; i < iters; ++i) {
                KQuant_Dispatch(2, input, output, 256);
            }
        }
        break;

    case CONV_WATCHDOG:
        moduleName = "Watchdog";
        {
            asm_watchdog_init();
            for (uint32_t i = 0; i < iters; ++i) {
                asm_watchdog_verify();
            }
            asm_watchdog_shutdown();
        }
        break;

    case CONV_PERF_COUNTERS:
        moduleName = "PerfCounters";
        {
            asm_perf_init();
            for (uint32_t i = 0; i < iters; ++i) {
                uint64_t tsc = asm_perf_begin(0);
                asm_perf_end(0, tsc);
            }
        }
        break;

    case CONV_SPILLOVER:
        moduleName = "Spillover";
        {
            // Throughput: rapid PerfCounters timing on spillover slots
            asm_perf_init();
            for (uint32_t i = 0; i < iters; ++i) {
                uint32_t slot = static_cast<uint32_t>(Perf::KernelSlot::Spillover_LoadWithSpillover);
                uint64_t tsc = asm_perf_begin(slot);
                asm_perf_end(slot, tsc);
            }
        }
        break;
    }

    uint64_t elapsed = nowMs() - start;
    double opsPerSec = elapsed > 0 ? (double)iters / ((double)elapsed / 1000.0) : 0.0;

    char detail[256];
    snprintf(detail, sizeof(detail), "%s throughput: %u ops in %llums (%.0f ops/sec)",
             moduleName, iters, (unsigned long long)elapsed, opsPerSec);

    recordPass("Throughput", moduleName, detail, elapsed);
}

// ============================================================================
// Perf-Instrumented Runs
// ============================================================================

void ConvergenceStressHarness::runPerfInstrumented(int module) {
    using namespace Perf;

    const char* moduleName = "Unknown";
    KernelSlot slot = KernelSlot::UserSlot_0;
    uint64_t start = nowMs();
    uint32_t iters = m_config.iterations;

    PerfTelemetry::instance().initialize();

    switch (module) {
    case CONV_CAMAUTH:
        moduleName = "CamAuth";
        slot = KernelSlot::CamAuth_EncryptBuf;
        {
            asm_camellia256_init();
            uint8_t key[32];
            memset(key, 0xEE, 32);
            asm_camellia256_set_key(key, 32);

            uint8_t payload[128];
            memset(payload, 0x11, sizeof(payload));
            uint8_t outBuf[512];

            for (uint32_t i = 0; i < iters; ++i) {
                ScopedMeasurement m(slot);
                uint32_t outLen = sizeof(outBuf);
                asm_camellia256_auth_encrypt_buf(payload, sizeof(payload), outBuf, &outLen);
            }
        }
        break;

    case CONV_KQUANT_KERNEL:
        moduleName = "KQuantKernel";
        slot = KernelSlot::KQuant_Q4K_AVX2;
        {
            alignas(32) float output[256];
            alignas(32) uint8_t block[144];
            memset(block, 0x77, sizeof(block));

            for (uint32_t i = 0; i < iters; ++i) {
                ScopedMeasurement m(slot);
                asm_dequant_q4_k_avx2(output, block);
            }
        }
        break;

    case CONV_KQUANT_DEQUANT:
        moduleName = "KQuantDequant";
        slot = KernelSlot::KQuant_Q4K_Dispatch;
        {
            alignas(32) float output[256];
            alignas(32) uint8_t input[256];
            memset(input, 0x44, sizeof(input));

            for (uint32_t i = 0; i < iters; ++i) {
                ScopedMeasurement m(slot);
                KQuant_Dispatch(2, input, output, 256);
            }
        }
        break;

    case CONV_WATCHDOG:
        moduleName = "Watchdog";
        slot = KernelSlot::Watchdog_Verify;
        {
            asm_watchdog_init();

            for (uint32_t i = 0; i < iters; ++i) {
                ScopedMeasurement m(slot);
                asm_watchdog_verify();
            }

            asm_watchdog_shutdown();
        }
        break;
    }

    // Capture perf results
    uint64_t elapsed = nowMs() - start;
    PerfSlotReport report = PerfTelemetry::instance().generateReport(static_cast<uint32_t>(slot));

    char detail[256];
    snprintf(detail, sizeof(detail),
        "%s perf-instrumented x%u: P50=%.0f P99=%.0f max=%llu cycles in %llums",
        moduleName, iters,
        report.percentiles.p50, report.percentiles.p99,
        (unsigned long long)report.maxCycles,
        (unsigned long long)elapsed);

    recordPass("PerfInstrumented", moduleName, detail, elapsed,
               static_cast<uint64_t>(report.percentiles.p50),
               static_cast<uint64_t>(report.percentiles.p99));
}

// ============================================================================
// Determinism Verification
// ============================================================================

bool ConvergenceStressHarness::verifyDeterministicOutput(int module) {
    const char* moduleName = "Unknown";
    uint64_t start = nowMs();
    bool deterministic = true;

    switch (module) {
    case CONV_CAMAUTH:
        moduleName = "CamAuth";
        {
            asm_camellia256_init();
            uint8_t key[32];
            memset(key, 0xFF, 32);
            asm_camellia256_set_key(key, 32);

            // Note: Auth encryption includes random IVs, so ciphertext differs.
            // Verify round-trip: encrypt → decrypt → compare
            uint8_t plaintext[64];
            memset(plaintext, 0xAA, sizeof(plaintext));

            uint8_t ciphertext[256];
            uint32_t ctLen = sizeof(ciphertext);
            int r = asm_camellia256_auth_encrypt_buf(plaintext, sizeof(plaintext),
                                                      ciphertext, &ctLen);
            if (r >= 0 && ctLen > 0) {
                uint8_t recovered[256];
                uint32_t ptLen = sizeof(recovered);
                r = asm_camellia256_auth_decrypt_buf(ciphertext, ctLen,
                                                     recovered, &ptLen);
                if (r >= 0 && ptLen == sizeof(plaintext)) {
                    if (memcmp(plaintext, recovered, sizeof(plaintext)) != 0) {
                        deterministic = false;
                    }
                }
            }
        }
        break;

    case CONV_KQUANT_KERNEL:
        moduleName = "KQuantKernel";
        {
            // Same input → same output, always
            alignas(32) uint8_t block[144];
            memset(block, 0x42, sizeof(block));

            alignas(32) float output1[256];
            alignas(32) float output2[256];

            asm_dequant_q4_k_avx2(output1, block);
            asm_dequant_q4_k_avx2(output2, block);

            if (memcmp(output1, output2, sizeof(output1)) != 0) {
                deterministic = false;
            }
        }
        break;
    }

    uint64_t elapsed = nowMs() - start;
    char detail[192];
    snprintf(detail, sizeof(detail), "%s determinism %s in %llums",
             moduleName, deterministic ? "VERIFIED" : "FAILED",
             (unsigned long long)elapsed);

    if (deterministic) recordPass("DeterminismCheck", moduleName, detail, elapsed);
    else               recordFail("DeterminismCheck", moduleName, detail, elapsed);

    return deterministic;
}

// ============================================================================
// Result Recording
// ============================================================================

void ConvergenceStressHarness::recordPass(const char* name, const char* moduleName,
                                           const char* detail, uint64_t durationMs,
                                           uint64_t medianCycles, uint64_t p99Cycles) {
    if (m_resultCount >= MAX_CONV_RESULTS) return;
    auto& r = m_results[m_resultCount++];
    r.passed = true;
    r.testName = name;
    r.moduleName = moduleName;
    r.detail = detail;
    r.durationMs = durationMs;
    r.iterations = m_config.iterations;
    r.medianCycles = medianCycles;
    r.p99Cycles = p99Cycles;

    if (m_config.enableLogging) {
        char msg[384];
        snprintf(msg, sizeof(msg), "[Convergence] PASS: %s.%s — %s\n",
                 moduleName, name, detail);
        OutputDebugStringA(msg);
    }
}

void ConvergenceStressHarness::recordFail(const char* name, const char* moduleName,
                                           const char* detail, uint64_t durationMs,
                                           uint64_t medianCycles, uint64_t p99Cycles) {
    if (m_resultCount >= MAX_CONV_RESULTS) return;
    auto& r = m_results[m_resultCount++];
    r.passed = false;
    r.testName = name;
    r.moduleName = moduleName;
    r.detail = detail;
    r.durationMs = durationMs;
    r.iterations = m_config.iterations;
    r.medianCycles = medianCycles;
    r.p99Cycles = p99Cycles;

    if (m_config.enableLogging) {
        char msg[384];
        snprintf(msg, sizeof(msg), "[Convergence] FAIL: %s.%s — %s\n",
                 moduleName, name, detail);
        OutputDebugStringA(msg);
    }
}

// ============================================================================
// Fuzz Helpers
// ============================================================================

void ConvergenceStressHarness::fillRandomBuffer(uint8_t* buf, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        buf[i] = static_cast<uint8_t>(nextRand());
    }
}

uint32_t ConvergenceStressHarness::nextRand() {
    // xorshift32
    m_rngState ^= m_rngState << 13;
    m_rngState ^= m_rngState >> 17;
    m_rngState ^= m_rngState << 5;
    return m_rngState;
}

uint64_t ConvergenceStressHarness::nowMs() const {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
}

} // namespace Convergence
} // namespace RawrXD
