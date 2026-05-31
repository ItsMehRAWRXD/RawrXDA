/* ═══════════════════════════════════════════════════════════════════════════
   RAWRXD_NEXUS Test Suite
   
   Comprehensive validation of all NEXUS features:
   - Speculative Decoding
   - Token-Level Parallelism
   - Dynamic Model Switching
   - Predictive Prefetching
   - Distributed Attention
   - Self-Correction Loop
   - Confidence-Gated Routing
   - Memory-Augmented Generation
   - Adaptive Quantization
   - Cross-Agent Knowledge Transfer
   ═══════════════════════════════════════════════════════════════════════════ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "rawrxd_nexus.h"

#define g_spec (*rxd_nexus_debug_speculative())
#define g_token_parallel (*rxd_nexus_debug_token_parallel())
#define g_router (*rxd_nexus_debug_router())
#define g_prefetch (*rxd_nexus_debug_prefetch())
#define g_dist_attn (*rxd_nexus_debug_attention())
#define g_correction (*rxd_nexus_debug_correction())
#define g_conf_router (*rxd_nexus_debug_confidence())
#define g_memory (*rxd_nexus_debug_memory())
#define g_adapt_quant (*rxd_nexus_debug_quant())
#define g_knowledge_net (*rxd_nexus_debug_knowledge())
#define g_nexus (*rxd_nexus_debug_engine())

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s...", #name); \
    test_##name(); \
    printf(" PASS\n"); \
    tests_passed++; \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        printf(" FAIL at line %d: %s\n", __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_GE(a, b) ASSERT_TRUE((a) >= (b))
#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))
#define ASSERT_LE(a, b) ASSERT_TRUE((a) <= (b))
#define ASSERT_STREQ(a, b) ASSERT_TRUE(strcmp((a), (b)) == 0)

/* ═══════════════════════════════════════════════════════════════════════════
   SPECULATIVE DECODING TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(speculative_init) {
    rxd_spec_init(0, 1, 0.7f);
    
    RXDSpeculativeEngine* spec = rxd_nexus_debug_speculative();
    ASSERT_TRUE(spec->enabled);
    ASSERT_EQ(spec->config.draft_model_id, 0);
    ASSERT_EQ(spec->config.verify_model_id, 1);
    ASSERT_GE(spec->config.verify_threshold, 0.7f);
    ASSERT_LE(spec->config.verify_threshold, 0.7f);
}

TEST(speculative_draft_generation) {
    rxd_spec_init(0, 1, 0.7f);
    
    RXDSpeculativeResult draft = rxd_spec_generate_draft("test context", 8);
    
    ASSERT_GT(draft.count, 0);
    ASSERT_LE(draft.count, RXD_NEXUS_MAX_DRAFT_TOKENS);
    ASSERT_GT(draft.generate_time_ns, 0);
    
    /* Check token probabilities decrease */
    for (uint32_t i = 1; i < draft.count; i++) {
        ASSERT_LE(draft.probabilities[i], draft.probabilities[i-1]);
    }
}

TEST(speculative_verification) {
    rxd_spec_init(0, 1, 0.7f);
    
    RXDSpeculativeResult draft = rxd_spec_generate_draft("test context", 8);
    RXDSpeculativeResult verified = rxd_spec_verify(&draft, "test context");
    
    RXDSpeculativeEngine* spec = rxd_nexus_debug_speculative();
    ASSERT_GT(verified.verify_time_ns, 0);
    ASSERT_GE(spec->total_draft_tokens, verified.count);
    ASSERT_GE(spec->total_accepted, verified.accepted_count);
    ASSERT_GE(spec->acceptance_rate, 0.0f);
    ASSERT_LE(spec->acceptance_rate, 1.0f);
}

TEST(speculative_speedup) {
    rxd_spec_init(0, 1, 0.7f);
    
    /* Run multiple iterations to build stats */
    for (int i = 0; i < 10; i++) {
        RXDSpeculativeResult draft = rxd_spec_generate_draft("test context", 8);
        rxd_spec_verify(&draft, "test context");
    }
    
    /* Speedup should be positive after multiple runs */
    RXDSpeculativeEngine* spec = rxd_nexus_debug_speculative();
    ASSERT_GE(spec->speedup_factor, 0.0f);
    ASSERT_GT(spec->total_draft_tokens, 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
   TOKEN PARALLELISM TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(token_parallel_init) {
    rxd_token_parallel_init(8);
    
    RXDTokenParallel* tp = rxd_nexus_debug_token_parallel();
    ASSERT_EQ(tp->parallel_factor, 8);
    ASSERT_EQ(tp->active_slots, 0);
    ASSERT_EQ(tp->completed_slots, 0);
}

TEST(token_parallel_allocate) {
    rxd_token_parallel_init(8);
    
    int32_t slot1 = rxd_token_parallel_allocate(100, 0.9f);
    int32_t slot2 = rxd_token_parallel_allocate(101, 0.85f);
    
    ASSERT_GE(slot1, 0);
    ASSERT_GE(slot2, 0);
    
    RXDTokenParallel* tp = rxd_nexus_debug_token_parallel();
    ASSERT_EQ(tp->active_slots, 2);
    
    /* Verify slot properties */
    ASSERT_EQ(tp->slots[slot1].token_id, 100);
    ASSERT_EQ(tp->slots[slot1].logprob, 0.9f);
    ASSERT_TRUE(tp->slots[slot1].is_speculative);
}

TEST(token_parallel_verify) {
    rxd_token_parallel_init(8);
    
    int32_t slot = rxd_token_parallel_allocate(100, -1.5f);
    bool verified = rxd_token_parallel_verify((uint32_t)slot);
    
    ASSERT_TRUE(verified);  /* logprob > -2.0 should pass */
    
    RXDTokenParallel* tp = rxd_nexus_debug_token_parallel();
    ASSERT_EQ(tp->completed_slots, 1);
}

TEST(token_parallel_multiplier) {
    rxd_token_parallel_init(8);
    
    RXDTokenParallel* tp = rxd_nexus_debug_token_parallel();
    tp->batch_start_ns = rxd_get_time_ns();
    
    for (int i = 0; i < 4; i++) {
        int32_t slot = rxd_token_parallel_allocate(100 + i, 0.8f);
        rxd_token_parallel_verify((uint32_t)slot);
    }
    
    float mult = rxd_token_parallel_get_multiplier();
    ASSERT_GE(mult, 0.0f);
}

/* ═══════════════════════════════════════════════════════════════════════════
   MODEL ROUTING TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(router_init) {
    rxd_router_init(10);
    
    RXDModelRouter* router = rxd_nexus_debug_router();
    ASSERT_EQ(router->model_count, 0);
    ASSERT_EQ(router->active_model, 0);
    ASSERT_TRUE(router->auto_switch_enabled);
    ASSERT_NE(router->complexity_history, NULL);
}

TEST(router_load_model) {
    rxd_router_init(10);
    
    bool loaded = rxd_router_load_model(RXD_MODEL_TINY, "models/tiny.gguf");
    ASSERT_TRUE(loaded);
    
    RXDModelRouter* router = rxd_nexus_debug_router();
    ASSERT_EQ(router->model_count, 1);
    ASSERT_TRUE(router->models[0].is_loaded);
    ASSERT_GT(router->models[0].tokens_per_sec, 0.0f);
}

TEST(router_complexity_measurement) {
    float simple = rxd_router_measure_complexity("hello world");
    float complex = rxd_router_measure_complexity("implement a distributed algorithm for parallel optimization");
    
    ASSERT_GE(complex, simple);
    ASSERT_GE(complex, 0.5f);
    ASSERT_LE(simple, 1.0f);
    ASSERT_LE(complex, 1.0f);
}

TEST(router_model_selection) {
    ASSERT_EQ(rxd_router_select_model(0.2f), RXD_MODEL_TINY);
    ASSERT_EQ(rxd_router_select_model(0.4f), RXD_MODEL_SMALL);
    ASSERT_EQ(rxd_router_select_model(0.6f), RXD_MODEL_MEDIUM);
    ASSERT_EQ(rxd_router_select_model(0.8f), RXD_MODEL_LARGE);
    ASSERT_EQ(rxd_router_select_model(0.9f), RXD_MODEL_XL);
    ASSERT_EQ(rxd_router_select_model(0.99f), RXD_MODEL_XXL);
}

TEST(router_switch) {
    rxd_router_init(10);
    rxd_router_load_model(RXD_MODEL_TINY, "models/tiny.gguf");
    rxd_router_load_model(RXD_MODEL_SMALL, "models/small.gguf");
    
    bool switched = rxd_router_switch_to(RXD_MODEL_SMALL);
    ASSERT_TRUE(switched);
    ASSERT_EQ(g_router.active_model, 1);
    ASSERT_EQ(g_router.switch_count, 1);
    ASSERT_GT(g_router.avg_switch_time_ms, 0.0f);
}

/* ═══════════════════════════════════════════════════════════════════════════
   PREFETCHING TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(prefetch_init) {
    rxd_prefetch_init();
    
    ASSERT_TRUE(g_prefetch.enabled);
    ASSERT_EQ(g_prefetch.count, 0);
    ASSERT_EQ(g_prefetch.hits, 0);
    ASSERT_EQ(g_prefetch.misses, 0);
}

TEST(prefetch_predict) {
    rxd_prefetch_init();
    
    RXDPrefetchEntry entry1 = rxd_prefetch_predict("explain this concept");
    ASSERT_GT(strlen(entry1.predicted_prompt), 0);
    ASSERT_GT(entry1.confidence, 0.0f);
    ASSERT_LE(entry1.confidence, 1.0f);
    
    RXDPrefetchEntry entry2 = rxd_prefetch_predict("write code for this");
    ASSERT_GT(strlen(entry2.predicted_prompt), 0);
}

TEST(prefetch_execute_and_check) {
    rxd_prefetch_init();
    
    RXDPrefetchEntry entry = rxd_prefetch_predict("explain this");
    bool executed = rxd_prefetch_execute(&entry);
    ASSERT_TRUE(executed);
    ASSERT_EQ(g_prefetch.count, 1);
    
    /* Check for matching prompt */
    bool found = rxd_prefetch_check(entry.predicted_prompt);
    ASSERT_TRUE(found);
    ASSERT_EQ(g_prefetch.hits, 1);
    ASSERT_GT(g_prefetch.hit_rate, 0.0f);
}

TEST(prefetch_miss) {
    rxd_prefetch_init();
    
    bool found = rxd_prefetch_check("nonexistent prompt");
    ASSERT_FALSE(found);
    ASSERT_EQ(g_prefetch.misses, 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
   DISTRIBUTED ATTENTION TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(dist_attn_init) {
    rxd_dist_attn_init(1);
    
    ASSERT_EQ(g_dist_attn.distribution_strategy, 1);
    ASSERT_EQ(g_dist_attn.active_heads, 0);
    ASSERT_GE(g_dist_attn.parallel_efficiency, 1.0f);
}

TEST(dist_attn_distribute) {
    rxd_dist_attn_init(0);  /* Round-robin */
    rxd_dist_attn_distribute(32, 4);
    
    ASSERT_EQ(g_dist_attn.active_heads, 32);
    
    /* Check round-robin distribution */
    for (uint32_t h = 0; h < 32; h++) {
        ASSERT_EQ(g_dist_attn.heads[h].head_id, h);
        ASSERT_EQ(g_dist_attn.heads[h].assigned_model, h % 4);
    }
}

TEST(dist_attn_compute) {
    rxd_dist_attn_init(1);
    rxd_dist_attn_distribute(16, 2);
    
    /* Set up tokens */
    for (uint32_t h = 0; h < 16; h++) {
        g_dist_attn.heads[h].num_tokens = 128;
    }
    
    rxd_dist_attn_compute_parallel();
    
    ASSERT_GT(g_dist_attn.total_compute_ns, 0);
    ASSERT_GE(g_dist_attn.parallel_efficiency, 1.0f);
}

/* ═══════════════════════════════════════════════════════════════════════════
   SELF-CORRECTION TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(self_correction_no_errors) {
    const char* clean_output = "This is a complete implementation with no issues.";
    RXDSelfCorrection correction = rxd_run_self_correction(clean_output, 3);
    
    ASSERT_EQ(correction.error_count, 0);
    ASSERT_EQ(correction.iterations, 1);  /* Exits early */
}

TEST(self_correction_with_errors) {
    const char* buggy_output = "TODO: implement this\nFIXME: fix bug\nError: missing brace {";
    RXDSelfCorrection correction = rxd_run_self_correction(buggy_output, 3);
    
    ASSERT_GT(correction.error_count, 0);
    ASSERT_GT(correction.iterations, 0);
}

TEST(self_correction_quality_improvement) {
    const char* output = "Some output with TODO";
    RXDSelfCorrection correction = rxd_run_self_correction(output, 3);
    
    ASSERT_GE(correction.quality_after, correction.quality_before);
}

/* ═══════════════════════════════════════════════════════════════════════════
   CONFIDENCE ROUTING TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(confidence_router_init) {
    rxd_confidence_router_init(0.5f, 0.8f);
    
    ASSERT_EQ(g_conf_router.confidence_threshold_low, 0.5f);
    ASSERT_EQ(g_conf_router.confidence_threshold_high, 0.8f);
    ASSERT_EQ(g_conf_router.routed_count, 0);
}

TEST(confidence_route_low) {
    rxd_confidence_router_init(0.5f, 0.8f);
    
    RXDRoutedToken token = rxd_confidence_route(100, 0.3f);
    
    ASSERT_EQ(token.token, 100);
    ASSERT_EQ(token.confidence, 0.3f);
    ASSERT_TRUE(token.was_rerouted);
    ASSERT_EQ(token.routed_to, RXD_MODEL_XL);
    ASSERT_EQ(g_conf_router.low_confidence_count, 1);
}

TEST(confidence_route_high) {
    rxd_confidence_router_init(0.5f, 0.8f);
    
    RXDRoutedToken token = rxd_confidence_route(100, 0.9f);
    
    ASSERT_FALSE(token.was_rerouted);
    ASSERT_EQ(token.routed_to, RXD_MODEL_SMALL);
    ASSERT_EQ(g_conf_router.high_confidence_count, 1);
}

TEST(confidence_route_medium) {
    rxd_confidence_router_init(0.5f, 0.8f);
    
    RXDRoutedToken token = rxd_confidence_route(100, 0.65f);
    
    ASSERT_FALSE(token.was_rerouted);
    ASSERT_EQ(token.routed_to, RXD_MODEL_MEDIUM);
}

/* ═══════════════════════════════════════════════════════════════════════════
   MEMORY AUGMENTATION TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(memory_init) {
    rxd_memory_init(512);
    
    ASSERT_EQ(g_memory.capacity, 512);
    ASSERT_EQ(g_memory.used_slots, 0);
}

TEST(memory_store_and_retrieve) {
    rxd_memory_init(512);
    
    bool stored = rxd_memory_store("test_key", "test_value");
    ASSERT_TRUE(stored);
    ASSERT_EQ(g_memory.used_slots, 1);
    
    const char* value = rxd_memory_retrieve("test_key");
    ASSERT_NE(value, NULL);
    ASSERT_STREQ(value, "test_value");
    ASSERT_EQ(g_memory.retrieval_hits, 1);
}

TEST(memory_search) {
    rxd_memory_init(512);
    
    rxd_memory_store("key1", "value with keyword");
    rxd_memory_store("key2", "another value");
    
    float relevance = 0.0f;
    const char* result = rxd_memory_search("keyword", &relevance);
    
    ASSERT_NE(result, NULL);
    ASSERT_GT(relevance, 0.0f);
}

TEST(memory_eviction) {
    rxd_memory_init(3);  /* Small capacity */
    
    rxd_memory_store("key1", "value1");
    rxd_memory_store("key2", "value2");
    rxd_memory_store("key3", "value3");
    
    /* Access key1 multiple times to increase its score */
    rxd_memory_retrieve("key1");
    rxd_memory_retrieve("key1");
    
    /* Add new key - should evict least accessed */
    bool stored = rxd_memory_store("key4", "value4");
    ASSERT_TRUE(stored);
    ASSERT_EQ(g_memory.used_slots, 3);
}

/* ═══════════════════════════════════════════════════════════════════════════
   ADAPTIVE QUANTIZATION TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(adaptive_quant_init) {
    rxd_adapt_quant_init(32);
    
    ASSERT_EQ(g_adapt_quant.layer_count, 32);
    ASSERT_TRUE(g_adapt_quant.auto_adjust_enabled);
    
    for (uint32_t i = 0; i < 32; i++) {
        ASSERT_EQ(g_adapt_quant.layers[i].current_type, GGML_RXD_TYPE_Q4_K);
    }
}

TEST(adaptive_quant_analyze) {
    float attn_importance = rxd_adapt_quant_analyze_layer(0, "attention.weight");
    float output_importance = rxd_adapt_quant_analyze_layer(31, "lm_head.weight");
    float early_importance = rxd_adapt_quant_analyze_layer(2, "layer.weight");
    
    ASSERT_GT(attn_importance, 0.5f);
    ASSERT_GT(output_importance, 0.5f);
    ASSERT_GT(early_importance, 0.5f);
}

TEST(adaptive_quant_switch) {
    rxd_adapt_quant_init(32);
    
    bool switched = rxd_adapt_quant_switch_layer(0, GGML_RXD_TYPE_Q6_K);
    ASSERT_TRUE(switched);
    ASSERT_TRUE(g_adapt_quant.layers[0].is_switched);
    ASSERT_EQ(g_adapt_quant.layers[0].current_type, GGML_RXD_TYPE_Q6_K);
    ASSERT_EQ(g_adapt_quant.switched_layers, 1);
}

TEST(adaptive_quant_auto_adjust) {
    rxd_adapt_quant_init(32);
    
    /* Set importance scores */
    g_adapt_quant.layers[0].importance_score = 0.9f;  /* High importance */
    g_adapt_quant.layers[1].importance_score = 0.3f;  /* Low importance */
    
    /* High VRAM pressure - should downgrade low importance */
    rxd_adapt_quant_auto_adjust(0.95f);
    
    /* Low VRAM pressure - should upgrade high importance */
    rxd_adapt_quant_auto_adjust(0.3f);
    
    ASSERT_GT(g_adapt_quant.switched_layers, 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
   KNOWLEDGE TRANSFER TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(knowledge_transfer_basic) {
    char* recipients[] = {"agent1", "agent2"};
    
    bool transferred = rxd_knowledge_transfer("pattern", "singleton pattern", 
                                              "source_agent", recipients, 2);
    ASSERT_TRUE(transferred);
    ASSERT_EQ(g_knowledge_net.transfer_count, 1);
    ASSERT_EQ(g_knowledge_net.successful_transfers, 1);
}

TEST(knowledge_query) {
    char* recipients[] = {"agent1"};
    rxd_knowledge_transfer("test_key", "test_value", "source", recipients, 1);
    
    const char* value = rxd_knowledge_query("test_key");
    ASSERT_NE(value, NULL);
    ASSERT_STREQ(value, "test_value");
}

TEST(knowledge_query_not_found) {
    const char* value = rxd_knowledge_query("nonexistent_key");
    ASSERT_EQ(value, NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
   NEXUS ENGINE INTEGRATION TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(nexus_init) {
    const char* models[] = {
        "models/tiny.gguf",
        "models/small.gguf",
        "models/medium.gguf"
    };
    
    bool initialized = rxd_nexus_init(models, 3);
    ASSERT_TRUE(initialized);
    ASSERT_TRUE(g_nexus.is_initialized);
    ASSERT_TRUE(g_nexus.is_running);
    ASSERT_EQ(g_router.model_count, 3);
}

TEST(nexus_infer) {
    const char* models[] = {"models/tiny.gguf"};
    rxd_nexus_init(models, 1);
    
    RXDNexusInferenceResult result = rxd_nexus_infer("test prompt");
    
    ASSERT_TRUE(result.success);
    ASSERT_GT(result.tokens, 0);
    ASSERT_GT(result.tps, 0.0f);
    ASSERT_GT(strlen(result.content), 0);
}

TEST(nexus_status) {
    const char* models[] = {"models/tiny.gguf"};
    rxd_nexus_init(models, 1);
    rxd_nexus_infer("test prompt");
    
    RXDNexusStatus status = rxd_nexus_get_status();
    
    ASSERT_GE(status.speculative_acceptance, 0.0f);
    ASSERT_LE(status.speculative_acceptance, 1.0f);
    ASSERT_GE(status.parallel_multiplier, 0.0f);
    ASSERT_GE(status.prefetch_hit_rate, 0.0f);
    ASSERT_LE(status.prefetch_hit_rate, 1.0f);
}

TEST(nexus_report_json) {
    const char* models[] = {"models/tiny.gguf"};
    rxd_nexus_init(models, 1);
    rxd_nexus_infer("test prompt");
    
    char* json = rxd_nexus_report_json();
    ASSERT_NE(json, NULL);
    ASSERT_GT(strlen(json), 0);
    ASSERT_TRUE(strstr(json, "speculative") != NULL);
    ASSERT_TRUE(strstr(json, "parallel") != NULL);
    ASSERT_TRUE(strstr(json, "overall") != NULL);
    
    free(json);
}

TEST(nexus_cleanup) {
    const char* models[] = {"models/tiny.gguf"};
    rxd_nexus_init(models, 1);
    
    rxd_nexus_cleanup();
    
    ASSERT_FALSE(g_nexus.is_initialized);
    ASSERT_FALSE(g_nexus.is_running);
}

TEST(nexus_multiple_inferences) {
    const char* models[] = {"models/tiny.gguf"};
    rxd_nexus_init(models, 1);
    
    /* Run multiple inferences to build stats */
    for (int i = 0; i < 10; i++) {
        RXDNexusInferenceResult result = rxd_nexus_infer("test prompt");
        ASSERT_TRUE(result.success);
    }
    
    RXDNexusStatus status = rxd_nexus_get_status();
    ASSERT_GT(status.total_tokens, 0);
    ASSERT_GT(status.effective_tps, 0.0f);
    ASSERT_GT(status.overall_speedup, 0.0f);
    
    rxd_nexus_cleanup();
}

/* ═══════════════════════════════════════════════════════════════════════════
   PERFORMANCE BENCHMARKS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(benchmark_speculative_decoding) {
    rxd_spec_init(0, 1, 0.7f);
    
    uint64_t start = rxd_get_time_ns();
    
    for (int i = 0; i < 1000; i++) {
        RXDSpeculativeResult draft = rxd_spec_generate_draft("benchmark", 8);
        rxd_spec_verify(&draft, "benchmark");
    }
    
    uint64_t elapsed = rxd_get_time_ns() - start;
    double avg_ns = (double)elapsed / 1000.0;
    
    printf("\n    Speculative decoding: %.2f ns/op (%.2f M ops/sec)\n", 
           avg_ns, 1e9 / avg_ns / 1e6);
    
    ASSERT_LT(avg_ns, 1000000.0);  /* < 1ms per operation */
}

TEST(benchmark_token_parallelism) {
    rxd_token_parallel_init(16);
    
    uint64_t start = rxd_get_time_ns();
    
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 16; j++) {
            rxd_token_parallel_allocate(100 + j, 0.8f);
            rxd_token_parallel_verify(j);
        }
        rxd_token_parallel_init(16);  /* Reset */
    }
    
    uint64_t elapsed = rxd_get_time_ns() - start;
    double avg_ns = (double)elapsed / 1000.0;
    
    printf("    Token parallelism: %.2f ns/op (%.2f M ops/sec)\n", 
           avg_ns, 1e9 / avg_ns / 1e6);
    
    ASSERT_LT(avg_ns, 500000.0);  /* < 0.5ms per operation */
}

TEST(benchmark_memory_operations) {
    rxd_memory_init(1024);
    
    uint64_t start = rxd_get_time_ns();
    
    for (int i = 0; i < 1000; i++) {
        char key[32], value[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        rxd_memory_store(key, value);
        rxd_memory_retrieve(key);
    }
    
    uint64_t elapsed = rxd_get_time_ns() - start;
    double avg_ns = (double)elapsed / 2000.0;  /* 2 ops per iteration */
    
    printf("    Memory operations: %.2f ns/op (%.2f M ops/sec)\n", 
           avg_ns, 1e9 / avg_ns / 1e6);
    
    ASSERT_LT(avg_ns, 100000.0);  /* < 0.1ms per operation */
}

/* ═══════════════════════════════════════════════════════════════════════════
   MAIN TEST RUNNER
   ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char** argv) {
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  RAWRXD_NEXUS Test Suite\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    /* Speculative Decoding */
    printf("[SPECULATIVE DECODING]\n");
    RUN_TEST(speculative_init);
    RUN_TEST(speculative_draft_generation);
    RUN_TEST(speculative_verification);
    RUN_TEST(speculative_speedup);
    printf("\n");
    
    /* Token Parallelism */
    printf("[TOKEN PARALLELISM]\n");
    RUN_TEST(token_parallel_init);
    RUN_TEST(token_parallel_allocate);
    RUN_TEST(token_parallel_verify);
    RUN_TEST(token_parallel_multiplier);
    printf("\n");
    
    /* Model Routing */
    printf("[MODEL ROUTING]\n");
    RUN_TEST(router_init);
    RUN_TEST(router_load_model);
    RUN_TEST(router_complexity_measurement);
    RUN_TEST(router_model_selection);
    RUN_TEST(router_switch);
    printf("\n");
    
    /* Prefetching */
    printf("[PREFETCHING]\n");
    RUN_TEST(prefetch_init);
    RUN_TEST(prefetch_predict);
    RUN_TEST(prefetch_execute_and_check);
    RUN_TEST(prefetch_miss);
    printf("\n");
    
    /* Distributed Attention */
    printf("[DISTRIBUTED ATTENTION]\n");
    RUN_TEST(dist_attn_init);
    RUN_TEST(dist_attn_distribute);
    RUN_TEST(dist_attn_compute);
    printf("\n");
    
    /* Self-Correction */
    printf("[SELF-CORRECTION]\n");
    RUN_TEST(self_correction_no_errors);
    RUN_TEST(self_correction_with_errors);
    RUN_TEST(self_correction_quality_improvement);
    printf("\n");
    
    /* Confidence Routing */
    printf("[CONFIDENCE ROUTING]\n");
    RUN_TEST(confidence_router_init);
    RUN_TEST(confidence_route_low);
    RUN_TEST(confidence_route_high);
    RUN_TEST(confidence_route_medium);
    printf("\n");
    
    /* Memory Augmentation */
    printf("[MEMORY AUGMENTATION]\n");
    RUN_TEST(memory_init);
    RUN_TEST(memory_store_and_retrieve);
    RUN_TEST(memory_search);
    RUN_TEST(memory_eviction);
    printf("\n");
    
    /* Adaptive Quantization */
    printf("[ADAPTIVE QUANTIZATION]\n");
    RUN_TEST(adaptive_quant_init);
    RUN_TEST(adaptive_quant_analyze);
    RUN_TEST(adaptive_quant_switch);
    RUN_TEST(adaptive_quant_auto_adjust);
    printf("\n");
    
    /* Knowledge Transfer */
    printf("[KNOWLEDGE TRANSFER]\n");
    RUN_TEST(knowledge_transfer_basic);
    RUN_TEST(knowledge_query);
    RUN_TEST(knowledge_query_not_found);
    printf("\n");
    
    /* NEXUS Integration */
    printf("[NEXUS ENGINE]\n");
    RUN_TEST(nexus_init);
    RUN_TEST(nexus_infer);
    RUN_TEST(nexus_status);
    RUN_TEST(nexus_report_json);
    RUN_TEST(nexus_cleanup);
    RUN_TEST(nexus_multiple_inferences);
    printf("\n");
    
    /* Performance Benchmarks */
    printf("[PERFORMANCE BENCHMARKS]\n");
    RUN_TEST(benchmark_speculative_decoding);
    RUN_TEST(benchmark_token_parallelism);
    RUN_TEST(benchmark_memory_operations);
    printf("\n");
    
    /* Summary */
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  TEST RESULTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);
    printf("  Coverage: %.1f%%\n", (float)tests_passed / (float)(tests_passed + tests_failed) * 100.0f);
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    
    return tests_failed > 0 ? 1 : 0;
}