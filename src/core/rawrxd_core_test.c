/* ═══════════════════════════════════════════════════════════════════════════
   RAWRXD_CORE Test Suite
   Tests for GGUF loading, quantization, hotswap, lockpick, tools, inference
   ═══════════════════════════════════════════════════════════════════════════ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "rawrxd_core.h"

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s... ", #name); \
    test_##name(); \
    printf("PASSED\n"); \
    tests_passed++; \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_GT(a, b) ASSERT((a) > (b))
#define ASSERT_LT(a, b) ASSERT((a) < (b))
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/* ═══════════════════════════════════════════════════════════════════════════
   HALF PRECISION TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(half_precision_roundtrip) {
    float values[] = {0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 3.14159f, -3.14159f, 
                       0.0001f, 10000.0f, 0.0078125f, 65504.0f};
    for (int i = 0; i < 11; i++) {
        rxd_half h = rxd_float_to_half(values[i]);
        float f = rxd_half_to_float(h);
        float diff = fabsf(values[i] - f);
        float rel_err = values[i] != 0.0f ? diff / fabsf(values[i]) : diff;
        ASSERT(rel_err < 0.01f || diff < 0.001f);
    }
}

TEST(half_precision_special) {
    /* Test zero */
    rxd_half h = rxd_float_to_half(0.0f);
    ASSERT_EQ(h, 0);
    
    /* Test negative zero */
    h = rxd_float_to_half(-0.0f);
    float f = rxd_half_to_float(h);
    ASSERT(f == 0.0f || f == -0.0f);
    
    /* Test infinity */
    h = rxd_float_to_half(INFINITY);
    ASSERT_EQ(h & 0x7C00, 0x7C00); /* Exponent all 1s */
}

/* ═══════════════════════════════════════════════════════════════════════════
   QUANTIZATION TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(quant_q4_0_roundtrip) {
    /* Create test data */
    float data[64];
    for (int i = 0; i < 64; i++) {
        data[i] = (float)(i - 32) / 32.0f; /* Range -1 to 1 */
    }
    
    /* Quantize */
    RXDQuant q = rxd_quant_q4_0(data, 64);
    ASSERT(q.data != NULL);
    ASSERT_GT(q.size, 0);
    
    /* Dequantize */
    RXDDequant d = rxd_dequant_q4_0(q.data, 2); /* 2 blocks */
    ASSERT(d.data != NULL);
    ASSERT_EQ(d.count, 64);
    
    /* Check quality */
    RXDQuality qual = rxd_estimate_quality(data, d.data, 64);
    ASSERT_GT(qual.snr_db, 10.0f); /* At least 10dB SNR */
    
    rxd_quant_free(&q);
    rxd_dequant_free(&d);
}

TEST(quant_q8_0_roundtrip) {
    float data[64];
    for (int i = 0; i < 64; i++) {
        data[i] = (float)(i - 32) / 16.0f; /* Range -2 to 2 */
    }
    
    RXDQuant q = rxd_quant_q8_0(data, 64);
    ASSERT(q.data != NULL);
    
    RXDDequant d = rxd_dequant_q8_0(q.data, 2);
    ASSERT(d.data != NULL);
    ASSERT_EQ(d.count, 64);
    
    RXDQuality qual = rxd_estimate_quality(data, d.data, 64);
    ASSERT_GT(qual.snr_db, 20.0f); /* Q8_0 should have better quality */
    
    rxd_quant_free(&q);
    rxd_dequant_free(&d);
}

TEST(quant_q4_k_roundtrip) {
    float data[512];
    for (int i = 0; i < 512; i++) {
        data[i] = (float)(i - 256) / 128.0f;
    }
    
    RXDQuant q = rxd_quant_q4_k(data, 512);
    ASSERT(q.data != NULL);
    
    RXDDequant d = rxd_dequant_q4_k(q.data, 2);
    ASSERT(d.data != NULL);
    ASSERT_EQ(d.count, 512);
    
    RXDQuality qual = rxd_estimate_quality(data, d.data, 512);
    ASSERT_GT(qual.snr_db, 15.0f);
    
    rxd_quant_free(&q);
    rxd_dequant_free(&d);
}

TEST(quant_generic_dispatch) {
    float data[128];
    for (int i = 0; i < 128; i++) data[i] = (float)i / 64.0f;
    
    /* Test F32 */
    RXDQuant qf32 = rxd_quant(data, 128, GGML_RXD_TYPE_F32);
    RXDDequant df32 = rxd_dequant(qf32.data, GGML_RXD_TYPE_F32, 128);
    ASSERT(df32.data != NULL);
    ASSERT_EQ(df32.count, 128);
    rxd_quant_free(&qf32);
    rxd_dequant_free(&df32);
    
    /* Test Q4_0 */
    RXDQuant q4 = rxd_quant(data, 128, GGML_RXD_TYPE_Q4_0);
    ASSERT(q4.data != NULL);
    RXDDequant d4 = rxd_dequant(q4.data, GGML_RXD_TYPE_Q4_0, 128);
    ASSERT(d4.data != NULL);
    rxd_quant_free(&q4);
    rxd_dequant_free(&d4);
}

TEST(quality_estimation) {
    /* Perfect reconstruction */
    float data[100];
    for (int i = 0; i < 100; i++) data[i] = (float)i;
    
    RXDQuality q = rxd_estimate_quality(data, data, 100);
    ASSERT_LT(q.mse, 1e-10f);
    ASSERT_GT(q.snr_db, 100.0f); /* Near infinite SNR */
    
    /* Noisy reconstruction */
    float noisy[100];
    for (int i = 0; i < 100; i++) noisy[i] = data[i] + 0.1f;
    
    q = rxd_estimate_quality(data, noisy, 100);
    ASSERT_GT(q.mse, 0.0f);
    ASSERT_LT(q.snr_db, 100.0f);
}

/* ═══════════════════════════════════════════════════════════════════════════
   MEMORY MAPPING TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(mmap_create_destroy) {
    /* Create a temporary file */
    const char* test_file = "test_mmap.bin";
    FILE* f = fopen(test_file, "wb");
    ASSERT(f != NULL);
    
    /* Write test data */
    uint8_t test_data[1024];
    for (int i = 0; i < 1024; i++) test_data[i] = (uint8_t)(i & 0xFF);
    fwrite(test_data, 1, 1024, f);
    fclose(f);
    
    /* Memory map it */
    RXDMemoryMap map = rxd_mmap_create(test_file);
    ASSERT(map.base != NULL);
    ASSERT_EQ(map.size, 1024);
    
    /* Verify data */
    uint8_t* data = (uint8_t*)map.base;
    for (int i = 0; i < 1024; i++) {
        ASSERT_EQ(data[i], (uint8_t)(i & 0xFF));
    }
    
    /* Cleanup */
    rxd_mmap_destroy(&map);
    ASSERT(map.base == NULL);
    
    remove(test_file);
}

TEST(mmap_large_file) {
    /* Test >2GB file support (simulated with smaller file) */
    const char* test_file = "test_large.bin";
    FILE* f = fopen(test_file, "wb");
    ASSERT(f != NULL);
    
    /* Write 10MB */
    uint8_t chunk[65536];
    for (int i = 0; i < 65536; i++) chunk[i] = (uint8_t)(i & 0xFF);
    for (int i = 0; i < 160; i++) fwrite(chunk, 1, 65536, f);
    fclose(f);
    
    RXDMemoryMap map = rxd_mmap_create(test_file);
    ASSERT(map.base != NULL);
    ASSERT_EQ(map.size, 160 * 65536);
    
    rxd_mmap_destroy(&map);
    remove(test_file);
}

/* ═══════════════════════════════════════════════════════════════════════════
   TOOLS TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(tools_init) {
    rxd_tools_init();
    ASSERT_GT(rxd_tool_count, 0);
    ASSERT(rxd_tool_count <= RXD_MAX_TOOLS);
}

TEST(tools_find) {
    rxd_tools_init();
    
    RXDToolDef* t = rxd_tool_find("file_read");
    ASSERT(t != NULL);
    ASSERT_STR_EQ(t->name, "file_read");
    
    t = rxd_tool_find("nonexistent");
    ASSERT(t == NULL);
}

TEST(tools_file_operations) {
    rxd_tools_init();
    
    /* Test file write */
    char output[8192];
    char args[4096];
    snprintf(args, sizeof(args), "test_tool.txt\nHello, World!");
    
    RXDToolResult r = rxd_tool_execute("file_write", args);
    ASSERT(r.success);
    
    /* Test file read */
    r = rxd_tool_execute("file_read", "test_tool.txt");
    ASSERT(r.success);
    ASSERT(strstr(r.output, "Hello") != NULL);
    
    /* Test file delete */
    r = rxd_tool_execute("file_delete", "test_tool.txt");
    ASSERT(r.success);
    
    /* Verify deleted */
    r = rxd_tool_execute("file_read", "test_tool.txt");
    ASSERT(!r.success);
}

TEST(tools_memory) {
    rxd_tools_init();
    
    char args[4096];
    snprintf(args, sizeof(args), "test_key\nThis is a test value");
    
    RXDToolResult r = rxd_tool_execute("memory_store", args);
    ASSERT(r.success);
    
    r = rxd_tool_execute("memory_recall", "test_key");
    ASSERT(r.success);
    ASSERT(strstr(r.output, "test value") != NULL);
    
    r = rxd_tool_execute("memory_recall", "nonexistent_key");
    ASSERT(!r.success);
}

/* ═══════════════════════════════════════════════════════════════════════════
   TIMER TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(timer_basic) {
    RXDTimer t;
    rxd_timer_start(&t);
    
    /* Do some work */
    volatile int sum = 0;
    for (int i = 0; i < 1000000; i++) sum += i;
    
    uint64_t ns = rxd_timer_stop(&t);
    ASSERT_GT(ns, 0);
    ASSERT_GT(t.total_ns, 0);
}

TEST(timer_precision) {
    RXDTimer t;
    uint64_t times[10];
    
    for (int i = 0; i < 10; i++) {
        rxd_timer_start(&t);
        times[i] = rxd_timer_stop(&t);
    }
    
    /* All times should be positive */
    for (int i = 0; i < 10; i++) {
        ASSERT_GT(times[i], 0);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   LSP BRIDGE TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(lsp_connect_disconnect) {
    bool connected = rxd_lsp_connect("test-language-server");
    ASSERT(connected);
    ASSERT(g_lsp.connected);
    
    rxd_lsp_disconnect();
    ASSERT(!g_lsp.connected);
}

TEST(lsp_requests) {
    rxd_lsp_connect("test-server");
    
    RXDLSPRequest req = rxd_lsp_definition("test.c", 10, 5);
    ASSERT(req.success);
    ASSERT_STR_EQ(req.type, "definition");
    
    req = rxd_lsp_references("test.c", 20, 10);
    ASSERT(req.success);
    
    req = rxd_lsp_hover("test.c", 15, 8);
    ASSERT(req.success);
    
    rxd_lsp_disconnect();
}

/* ═══════════════════════════════════════════════════════════════════════════
   ENGINE TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(engine_init_destroy) {
    /* Create a minimal test GGUF file */
    const char* test_file = "test_model.gguf";
    FILE* f = fopen(test_file, "wb");
    ASSERT(f != NULL);
    
    /* Write minimal GGUF header */
    uint32_t magic = GGUF_MAGIC;
    uint32_t version = 3;
    uint64_t tensor_count = 0;
    uint64_t meta_kv = 0;
    
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&tensor_count, 8, 1, f);
    fwrite(&meta_kv, 8, 1, f);
    fclose(f);
    
    bool init = rxd_engine_init(test_file);
    /* Note: May fail if model is invalid, that's OK for this test */
    
    rxd_engine_destroy();
    ASSERT(!g_engine.initialized);
    
    remove(test_file);
}

TEST(engine_metrics) {
    RXDEngineMetrics m = rxd_engine_get_metrics();
    /* Initial state should be zeros */
    ASSERT_EQ(m.total_requests, 0);
    ASSERT_EQ(m.total_tokens, 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
   PROFILE TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(profile_defaults) {
    ASSERT_STR_EQ(RXD_PROFILE_SPEED.name, "speed");
    ASSERT_EQ(RXD_PROFILE_SPEED.default_type, GGML_RXD_TYPE_Q4_0);
    ASSERT_EQ(RXD_PROFILE_SPEED.attn_type, GGML_RXD_TYPE_Q4_0);
    
    ASSERT_STR_EQ(RXD_PROFILE_BALANCED.name, "balanced");
    ASSERT_EQ(RXD_PROFILE_BALANCED.default_type, GGML_RXD_TYPE_Q4_K);
    
    ASSERT_STR_EQ(RXD_PROFILE_QUALITY.name, "quality");
    ASSERT_EQ(RXD_PROFILE_QUALITY.default_type, GGML_RXD_TYPE_Q6_K);
}

/* ═══════════════════════════════════════════════════════════════════════════
   WEIGHT CLASSIFICATION TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(weight_classification) {
    RXDWeight w;
    
    strcpy(w.name, "layers.0.attn.q_proj.weight");
    ASSERT(rxd_is_attn(&w));
    ASSERT(!rxd_is_ffn(&w));
    
    strcpy(w.name, "layers.0.mlp.up_proj.weight");
    ASSERT(!rxd_is_attn(&w));
    ASSERT(rxd_is_ffn(&w));
    
    strcpy(w.name, "tok_embeddings.weight");
    ASSERT(rxd_is_embed(&w));
    
    strcpy(w.name, "output.weight");
    ASSERT(rxd_is_output(&w));
}

/* ═══════════════════════════════════════════════════════════════════════════
   REPORT TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(report_json) {
    /* Create minimal lockpick */
    RXDLockpick l = {0};
    l.exp_capacity = 2;
    l.experiments = calloc(2, sizeof(RXDExperiment));
    l.exp_count = 1;
    strcpy(l.experiments[0].name, "test");
    l.experiments[0].metrics.tps = 50.0;
    l.experiments[0].quality.avg_snr = 25.0f;
    l.experiments[0].metrics.model_size = 1024;
    l.best_exp = 0;
    
    RXDReport r = rxd_report_json(&l);
    ASSERT(r.data != NULL);
    ASSERT(strstr(r.data, "test") != NULL);
    ASSERT(strstr(r.data, "50.00") != NULL);
    
    rxd_report_free(&r);
    ASSERT(r.data == NULL);
    
    free(l.experiments);
}

/* ═══════════════════════════════════════════════════════════════════════════
   MAIN
   ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════════════\n");
    printf("  RAWRXD_CORE Test Suite\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("[Half Precision Tests]\n");
    RUN_TEST(half_precision_roundtrip);
    RUN_TEST(half_precision_special);
    
    printf("\n[Quantization Tests]\n");
    RUN_TEST(quant_q4_0_roundtrip);
    RUN_TEST(quant_q8_0_roundtrip);
    RUN_TEST(quant_q4_k_roundtrip);
    RUN_TEST(quant_generic_dispatch);
    RUN_TEST(quality_estimation);
    
    printf("\n[Memory Mapping Tests]\n");
    RUN_TEST(mmap_create_destroy);
    RUN_TEST(mmap_large_file);
    
    printf("\n[Tools Tests]\n");
    RUN_TEST(tools_init);
    RUN_TEST(tools_find);
    RUN_TEST(tools_file_operations);
    RUN_TEST(tools_memory);
    
    printf("\n[Timer Tests]\n");
    RUN_TEST(timer_basic);
    RUN_TEST(timer_precision);
    
    printf("\n[LSP Bridge Tests]\n");
    RUN_TEST(lsp_connect_disconnect);
    RUN_TEST(lsp_requests);
    
    printf("\n[Engine Tests]\n");
    RUN_TEST(engine_init_destroy);
    RUN_TEST(engine_metrics);
    
    printf("\n[Profile Tests]\n");
    RUN_TEST(profile_defaults);
    
    printf("\n[Weight Classification Tests]\n");
    RUN_TEST(weight_classification);
    
    printf("\n[Report Tests]\n");
    RUN_TEST(report_json);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}