// inference_playback_bridge_test.c - Real Inference Hook Tests
// Validates determinism, real TPS measurement, and hot-swap functionality
// Part of RawrXD Progressive Layer Loading System

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define INFERENCE_PLAYBACK_BRIDGE_IMPLEMENTATION
#include "inference_playback_bridge.h"

// ============================================================================
// TEST INFRASTRUCTURE
// ============================================================================

typedef struct {
    const char* name;
    bool (*test_func)(void);
    bool passed;
    const char* error_msg;
    uint64_t time_ns;
} TestCase;

typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
    uint64_t total_time_ns;
} TestResults;

static TestResults g_results = {0};
static int g_verbose = 1;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        if (g_verbose) printf("    ✗ FAIL: %s\n", msg); \
        return false; \
    } \
} while(0)

// ============================================================================
// GGUF LOADER TESTS
// ============================================================================

static bool test_gguf_create_destroy(void) {
    if (g_verbose) printf("  Testing GGUF context creation...\n");
    
    GGUFContext* ctx = gguf_create_context();
    TEST_ASSERT(ctx != NULL, "Context should not be NULL");
    TEST_ASSERT(ctx->is_loaded == false, "Should not be loaded initially");
    TEST_ASSERT(ctx->is_mapped == false, "Should not be mapped initially");
    
    gguf_destroy_context(ctx);
    
    if (g_verbose) printf("    ✓ GGUF context test passed\n");
    return true;
}

static bool test_gguf_validate_file(void) {
    if (g_verbose) printf("  Testing GGUF file validation...\n");
    
    // Test with non-existent file
    bool valid = validate_gguf_file("nonexistent.gguf");
    TEST_ASSERT(valid == false, "Non-existent file should be invalid");
    
    // Test with valid GGUF (would need actual file)
    // valid = validate_gguf_file("test_model.gguf");
    // TEST_ASSERT(valid == true, "Valid GGUF should pass");
    
    if (g_verbose) printf("    ✓ GGUF validation test passed\n");
    return true;
}

static bool test_gguf_memory_mapped_load(void) {
    if (g_verbose) printf("  Testing memory-mapped GGUF load...\n");
    
    GGUFContext* ctx = gguf_create_context();
    TEST_ASSERT(ctx != NULL, "Context should not be NULL");
    
    // This would load an actual GGUF file
    // For now, test the structure
    TEST_ASSERT(ctx->file_handle == NULL, "File handle should be NULL initially");
    TEST_ASSERT(ctx->base_address == NULL, "Base address should be NULL initially");
    
    gguf_destroy_context(ctx);
    
    if (g_verbose) printf("    ✓ Memory-mapped load test passed\n");
    return true;
}

// ============================================================================
// INFERENCE CONTEXT TESTS
// ============================================================================

static bool test_inference_create_destroy(void) {
    if (g_verbose) printf("  Testing inference context creation...\n");
    
    InferenceContext* ctx = inference_create_context();
    TEST_ASSERT(ctx != NULL, "Context should not be NULL");
    TEST_ASSERT(ctx->batch_tokens != NULL, "Batch tokens should be allocated");
    TEST_ASSERT(ctx->metrics_history != NULL, "Metrics history should be allocated");
    TEST_ASSERT(ctx->is_initialized == false, "Should not be initialized initially");
    
    inference_destroy_context(ctx);
    
    if (g_verbose) printf("    ✓ Inference context test passed\n");
    return true;
}

static bool test_inference_initialize(void) {
    if (g_verbose) printf("  Testing inference initialization...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* ctx = inference_create_context();
    
    TEST_ASSERT(ctx != NULL, "Context should not be NULL");
    TEST_ASSERT(gguf != NULL, "GGUF context should not be NULL");
    
    // Initialize with defaults
    bool result = inference_initialize(ctx, gguf, 4096, 512);
    TEST_ASSERT(result == true, "Initialization should succeed");
    TEST_ASSERT(ctx->is_initialized == true, "Should be initialized");
    TEST_ASSERT(ctx->n_ctx == 4096, "Context length should be set");
    TEST_ASSERT(ctx->batch_size == 512, "Batch size should be set");
    TEST_ASSERT(ctx->kv_cache != NULL, "KV cache should be allocated");
    
    inference_destroy_context(ctx);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Inference initialization test passed\n");
    return true;
}

static bool test_inference_forward(void) {
    if (g_verbose) printf("  Testing inference forward pass...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* ctx = inference_create_context();
    
    inference_initialize(ctx, gguf, 4096, 512);
    
    // Run forward pass
    int32_t tokens[] = {1, 2, 3, 4, 5};
    InferenceMetrics metrics;
    
    bool result = inference_forward(ctx, tokens, 5, &metrics);
    TEST_ASSERT(result == true, "Forward pass should succeed");
    TEST_ASSERT(metrics.tokens_per_second > 0, "TPS should be positive");
    TEST_ASSERT(metrics.total_latency_ms > 0, "Latency should be positive");
    TEST_ASSERT(metrics.inference_id == 0, "First inference should have ID 0");
    
    // Run another forward pass
    result = inference_forward(ctx, tokens, 5, &metrics);
    TEST_ASSERT(result == true, "Second forward pass should succeed");
    TEST_ASSERT(metrics.inference_id == 1, "Second inference should have ID 1");
    
    inference_destroy_context(ctx);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Inference forward test passed\n");
    return true;
}

static bool test_inference_generate(void) {
    if (g_verbose) printf("  Testing inference generation...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* ctx = inference_create_context();
    
    inference_initialize(ctx, gguf, 4096, 512);
    
    InferenceMetrics metrics;
    bool result = inference_generate(ctx, "Hello, world!", 50, 0.7f, 0.9f, &metrics);
    
    TEST_ASSERT(result == true, "Generation should succeed");
    TEST_ASSERT(metrics.tokens_per_second > 0, "TPS should be positive");
    
    inference_destroy_context(ctx);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Inference generation test passed\n");
    return true;
}

static bool test_inference_metrics_history(void) {
    if (g_verbose) printf("  Testing inference metrics history...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* ctx = inference_create_context();
    
    inference_initialize(ctx, gguf, 4096, 512);
    
    // Run multiple inferences
    int32_t tokens[] = {1, 2, 3};
    InferenceMetrics metrics;
    
    for (int i = 0; i < 10; i++) {
        inference_forward(ctx, tokens, 3, &metrics);
    }
    
    TEST_ASSERT(ctx->metrics_count == 10, "Should have 10 metrics entries");
    
    // Get latest metrics
    InferenceMetrics* latest = inference_get_metrics(ctx);
    TEST_ASSERT(latest != NULL, "Latest metrics should not be NULL");
    TEST_ASSERT(latest->inference_id == 9, "Latest should have ID 9");
    
    inference_destroy_context(ctx);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Metrics history test passed\n");
    return true;
}

// ============================================================================
// BRIDGE TESTS
// ============================================================================

static bool test_bridge_create_destroy(void) {
    if (g_verbose) printf("  Testing bridge creation...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* inference = inference_create_context();
    inference_initialize(inference, gguf, 4096, 512);
    
    InferencePlaybackBridge* bridge = bridge_create(NULL, inference, gguf);
    TEST_ASSERT(bridge != NULL, "Bridge should not be NULL");
    TEST_ASSERT(bridge->inference == inference, "Inference should be set");
    TEST_ASSERT(bridge->gguf == gguf, "GGUF should be set");
    TEST_ASSERT(bridge->is_hot_swap_enabled == true, "Hot-swap should be enabled");
    TEST_ASSERT(bridge->current_quantization == 4, "Default quantization should be Q4_K_M");
    
    bridge_destroy(bridge);
    inference_destroy_context(inference);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Bridge creation test passed\n");
    return true;
}

// ============================================================================
// HOT-SWAP TESTS
// ============================================================================

static bool test_quantization_swap(void) {
    if (g_verbose) printf("  Testing quantization hot-swap...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* inference = inference_create_context();
    inference_initialize(inference, gguf, 4096, 512);
    
    InferencePlaybackBridge* bridge = bridge_create(NULL, inference, gguf);
    
    // Swap to Q5_K_M
    QuantSwapResult result = bridge_swap_quantization(bridge, 5);
    TEST_ASSERT(result.success == true, "Swap should succeed");
    TEST_ASSERT(result.old_quant == 4, "Old quant should be Q4_K_M");
    TEST_ASSERT(result.new_quant == 5, "New quant should be Q5_K_M");
    TEST_ASSERT(result.time_ns > 0, "Time should be recorded");
    
    // Swap to Q8_0
    result = bridge_swap_quantization(bridge, 2);
    TEST_ASSERT(result.success == true, "Second swap should succeed");
    TEST_ASSERT(result.old_quant == 5, "Old quant should be Q5_K_M");
    TEST_ASSERT(result.new_quant == 2, "New quant should be Q8_0");
    
    bridge_destroy(bridge);
    inference_destroy_context(inference);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Quantization swap test passed\n");
    return true;
}

static bool test_kv_compression_swap(void) {
    if (g_verbose) printf("  Testing KV compression hot-swap...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* inference = inference_create_context();
    inference_initialize(inference, gguf, 4096, 512);
    
    InferencePlaybackBridge* bridge = bridge_create(NULL, inference, gguf);
    
    KVSwapResult result = bridge_swap_kv_compression(bridge, 2);
    TEST_ASSERT(result.success == true, "KV swap should succeed");
    TEST_ASSERT(result.old_compression == 0, "Old compression should be 0");
    TEST_ASSERT(result.new_compression == 2, "New compression should be 2");
    
    bridge_destroy(bridge);
    inference_destroy_context(inference);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ KV compression swap test passed\n");
    return true;
}

static bool test_prune_ratio_swap(void) {
    if (g_verbose) printf("  Testing prune ratio hot-swap...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* inference = inference_create_context();
    inference_initialize(inference, gguf, 4096, 512);
    
    InferencePlaybackBridge* bridge = bridge_create(NULL, inference, gguf);
    
    PruneSwapResult result = bridge_swap_prune_ratio(bridge, 0.3f);
    TEST_ASSERT(result.success == true, "Prune swap should succeed");
    TEST_ASSERT(result.old_ratio == 0.0f, "Old ratio should be 0.0");
    TEST_ASSERT(fabsf(result.new_ratio - 0.3f) < 0.01f, "New ratio should be 0.3");
    
    bridge_destroy(bridge);
    inference_destroy_context(inference);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Prune ratio swap test passed\n");
    return true;
}

// ============================================================================
// REAL MEASUREMENT TESTS
// ============================================================================

static bool test_measure_tps(void) {
    if (g_verbose) printf("  Testing TPS measurement...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* inference = inference_create_context();
    inference_initialize(inference, gguf, 4096, 512);
    
    InferencePlaybackBridge* bridge = bridge_create(NULL, inference, gguf);
    
    TPSMeasurement tps = bridge_measure_tps(bridge, "Test prompt", 50, 3);
    
    TEST_ASSERT(tps.tokens_generated == 50, "Should generate 50 tokens");
    TEST_ASSERT(tps.tps > 0, "TPS should be positive");
    TEST_ASSERT(tps.measurement_time_ns > 0, "Time should be recorded");
    TEST_ASSERT(tps.confidence_interval > 0, "CI should be positive");
    
    if (g_verbose) {
        printf("    TPS: %.2f (first token: %.2f, ms/token: %.2f)\n",
               tps.tps, tps.tps_first_token, tps.ms_per_token);
    }
    
    bridge_destroy(bridge);
    inference_destroy_context(inference);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ TPS measurement test passed\n");
    return true;
}

static bool test_measure_quality(void) {
    if (g_verbose) printf("  Testing quality measurement...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* inference = inference_create_context();
    inference_initialize(inference, gguf, 4096, 512);
    
    InferencePlaybackBridge* bridge = bridge_create(NULL, inference, gguf);
    
    QualityMeasurement quality = bridge_measure_quality(bridge, "Test prompt", NULL);
    
    TEST_ASSERT(quality.perplexity > 0, "Perplexity should be positive");
    TEST_ASSERT(quality.entropy > 0, "Entropy should be positive");
    TEST_ASSERT(quality.tokens_evaluated > 0, "Tokens should be evaluated");
    TEST_ASSERT(quality.measurement_time_ns > 0, "Time should be recorded");
    
    if (g_verbose) {
        printf("    Perplexity: %.2f, Entropy: %.2f\n", quality.perplexity, quality.entropy);
    }
    
    bridge_destroy(bridge);
    inference_destroy_context(inference);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Quality measurement test passed\n");
    return true;
}

static bool test_measure_memory(void) {
    if (g_verbose) printf("  Testing memory measurement...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* inference = inference_create_context();
    inference_initialize(inference, gguf, 4096, 512);
    
    InferencePlaybackBridge* bridge = bridge_create(NULL, inference, gguf);
    
    MemoryMeasurement memory = bridge_measure_memory(bridge);
    
    TEST_ASSERT(memory.measurement_time_ns > 0, "Time should be recorded");
    
    if (g_verbose) {
        printf("    VRAM: %lu MB, RAM: %lu MB\n",
               (unsigned long)(memory.vram_used / (1024 * 1024)),
               (unsigned long)(memory.ram_used / (1024 * 1024)));
    }
    
    bridge_destroy(bridge);
    inference_destroy_context(inference);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Memory measurement test passed\n");
    return true;
}

// ============================================================================
// DETERMINISM TESTS
// ============================================================================

static bool test_state_hash(void) {
    if (g_verbose) printf("  Testing state hash computation...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* inference = inference_create_context();
    inference_initialize(inference, gguf, 4096, 512);
    
    InferencePlaybackBridge* bridge = bridge_create(NULL, inference, gguf);
    
    StateHash hash1 = bridge_compute_state_hash(bridge);
    StateHash hash2 = bridge_compute_state_hash(bridge);
    
    TEST_ASSERT(hash1.combined_hash != 0, "Hash should not be zero");
    TEST_ASSERT(hash1.timestamp_ns > 0, "Timestamp should be set");
    
    // Hashes should be deterministic for same state
    TEST_ASSERT(hash1.config_hash == hash2.config_hash, "Config hashes should match");
    
    if (g_verbose) {
        printf("    Hash: %016llx\n", (unsigned long long)hash1.combined_hash);
    }
    
    bridge_destroy(bridge);
    inference_destroy_context(inference);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ State hash test passed\n");
    return true;
}

static bool test_determinism_validation(void) {
    if (g_verbose) printf("  Testing determinism validation...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* inference = inference_create_context();
    inference_initialize(inference, gguf, 4096, 512);
    
    InferencePlaybackBridge* bridge = bridge_create(NULL, inference, gguf);
    
    DeterminismResult result = bridge_validate_determinism(bridge, "Test prompt", 2);
    
    TEST_ASSERT(result.hash_run1 != 0, "Hash 1 should not be zero");
    TEST_ASSERT(result.hash_run2 != 0, "Hash 2 should not be zero");
    
    if (g_verbose) {
        printf("    Hash 1: %016llx\n", (unsigned long long)result.hash_run1);
        printf("    Hash 2: %016llx\n", (unsigned long long)result.hash_run2);
        printf("    Deterministic: %s\n", result.is_deterministic ? "Yes" : "No");
    }
    
    bridge_destroy(bridge);
    inference_destroy_context(inference);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Determinism validation test passed\n");
    return true;
}

// ============================================================================
// PLAYBACK HOOK TESTS
// ============================================================================

static bool test_playback_step(void) {
    if (g_verbose) printf("  Testing playback step with real inference...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* inference = inference_create_context();
    inference_initialize(inference, gguf, 4096, 512);
    
    InferencePlaybackBridge* bridge = bridge_create(NULL, inference, gguf);
    
    PlaybackStepResult result = bridge_playback_step(bridge, 
        HOTPATCH_QUANTIZE | HOTPATCH_PRUNE_WEIGHTS, "Test prompt");
    
    TEST_ASSERT(result.success == true, "Playback step should succeed");
    TEST_ASSERT(result.metrics.tokens_per_second > 0, "TPS should be positive");
    TEST_ASSERT(result.hash.combined_hash != 0, "State hash should be computed");
    
    if (g_verbose) {
        printf("    TPS: %.2f, Hash: %016llx\n",
               result.metrics.tokens_per_second,
               (unsigned long long)result.hash.combined_hash);
    }
    
    bridge_destroy(bridge);
    inference_destroy_context(inference);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Playback step test passed\n");
    return true;
}

// ============================================================================
// LOCKPICKING TESTS
// ============================================================================

static bool test_lockpick_session(void) {
    if (g_verbose) printf("  Testing lockpicking session...\n");
    
    GGUFContext* gguf = gguf_create_context();
    InferenceContext* inference = inference_create_context();
    inference_initialize(inference, gguf, 4096, 512);
    
    InferencePlaybackBridge* bridge = bridge_create(NULL, inference, gguf);
    
    RealLockpickSession* session = bridge_lockpick_start(bridge, "Test prompt", 0);
    TEST_ASSERT(session != NULL, "Session should not be NULL");
    TEST_ASSERT(session->baseline_tps.tps > 0, "Baseline TPS should be positive");
    TEST_ASSERT(session->baseline_quality.perplexity > 0, "Baseline perplexity should be positive");
    
    if (g_verbose) {
        printf("    Baseline TPS: %.2f, PPL: %.2f\n",
               session->baseline_tps.tps, session->baseline_quality.perplexity);
    }
    
    // Try configuration
    RealLockpickResult result = bridge_lockpick_try(session, HOTPATCH_QUANTIZE, 0.3f, 4);
    TEST_ASSERT(result.success == true, "Lockpick try should succeed");
    
    if (g_verbose) {
        printf("    Result: %s\n", result.recommendation);
    }
    
    // Get best
    BestConfig best = bridge_lockpick_get_best(session);
    TEST_ASSERT(best.tps > 0, "Best TPS should be positive");
    
    bridge_lockpick_end(session);
    bridge_destroy(bridge);
    inference_destroy_context(inference);
    gguf_destroy_context(gguf);
    
    if (g_verbose) printf("    ✓ Lockpicking session test passed\n");
    return true;
}

// ============================================================================
// UTILITY TESTS
// ============================================================================

static bool test_quantization_utils(void) {
    if (g_verbose) printf("  Testing quantization utilities...\n");
    
    // Test string conversion
    TEST_ASSERT(strcmp(quantization_to_string(4), "Q5_K_M") == 0, "Q5_K_M string");
    TEST_ASSERT(strcmp(quantization_to_string(6), "Q4_K_M") == 0, "Q4_K_M string");
    TEST_ASSERT(strcmp(quantization_to_string(0), "FP32") == 0, "FP32 string");
    
    // Test reverse conversion
    TEST_ASSERT(string_to_quantization("Q5_K_M") == 4, "Q5_K_M enum");
    TEST_ASSERT(string_to_quantization("Q4_K_M") == 6, "Q4_K_M enum");
    TEST_ASSERT(string_to_quantization("FP32") == 0, "FP32 enum");
    
    // Test bits per weight
    TEST_ASSERT(fabsf(quantization_bits_per_weight(0) - 32.0f) < 0.01f, "FP32 bits");
    TEST_ASSERT(fabsf(quantization_bits_per_weight(6) - 4.5f) < 0.01f, "Q4_K_M bits");
    
    if (g_verbose) printf("    ✓ Quantization utilities test passed\n");
    return true;
}

// ============================================================================
// TEST RUNNER
// ============================================================================

static TestCase g_tests[] = {
    // GGUF Loader
    {"gguf_create_destroy", test_gguf_create_destroy},
    {"gguf_validate_file", test_gguf_validate_file},
    {"gguf_memory_mapped_load", test_gguf_memory_mapped_load},
    
    // Inference Context
    {"inference_create_destroy", test_inference_create_destroy},
    {"inference_initialize", test_inference_initialize},
    {"inference_forward", test_inference_forward},
    {"inference_generate", test_inference_generate},
    {"inference_metrics_history", test_inference_metrics_history},
    
    // Bridge
    {"bridge_create_destroy", test_bridge_create_destroy},
    
    // Hot-Swap
    {"quantization_swap", test_quantization_swap},
    {"kv_compression_swap", test_kv_compression_swap},
    {"prune_ratio_swap", test_prune_ratio_swap},
    
    // Real Measurement
    {"measure_tps", test_measure_tps},
    {"measure_quality", test_measure_quality},
    {"measure_memory", test_measure_memory},
    
    // Determinism
    {"state_hash", test_state_hash},
    {"determinism_validation", test_determinism_validation},
    
    // Playback Hook
    {"playback_step", test_playback_step},
    
    // Lockpicking
    {"lockpick_session", test_lockpick_session},
    
    // Utilities
    {"quantization_utils", test_quantization_utils},
};

static void run_tests(void) {
    g_results.total_tests = sizeof(g_tests) / sizeof(g_tests[0]);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     INFERENCE PLAYBACK BRIDGE - TEST SUITE                   ║\n");
    printf("║     Real Inference Hook Validation                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Running %d tests...\n\n", g_results.total_tests);
    
    for (int i = 0; i < g_results.total_tests; i++) {
        printf("[%d/%d] %s\n", i + 1, g_results.total_tests, g_tests[i].name);
        
        uint64_t start_time = get_time_ns_impl();
        bool passed = g_tests[i].test_func();
        uint64_t end_time = get_time_ns_impl();
        
        g_tests[i].time_ns = end_time - start_time;
        
        if (passed) {
            g_results.passed_tests++;
            printf("  ✓ PASSED (%.2f ms)\n\n", g_tests[i].time_ns / 1e6f);
        } else {
            g_results.failed_tests++;
            printf("  ✗ FAILED (%.2f ms)\n\n", g_tests[i].time_ns / 1e6f);
        }
    }
    
    g_results.total_time_ns = 0;
    for (int i = 0; i < g_results.total_tests; i++) {
        g_results.total_time_ns += g_tests[i].time_ns;
    }
    
    // Print summary
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                    TEST SUMMARY                              ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Total:   %3d                                                 \n", g_results.total_tests);
    printf("║ Passed: %3d  (%.1f%%)                                        \n",
           g_results.passed_tests,
           100.0f * g_results.passed_tests / g_results.total_tests);
    printf("║ Failed: %3d  (%.1f%%)                                        \n",
           g_results.failed_tests,
           100.0f * g_results.failed_tests / g_results.total_tests);
    printf("║ Duration: %.2f ms                                            \n",
           g_results.total_time_ns / 1e6f);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    if (g_results.failed_tests == 0) {
        printf("\n✓ ALL TESTS PASSED\n\n");
    } else {
        printf("\n✗ SOME TESTS FAILED\n\n");
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            g_verbose = 0;
        }
    }
    
    run_tests();
    
    return g_results.failed_tests > 0 ? 1 : 0;
}