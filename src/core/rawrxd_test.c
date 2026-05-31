/* ═══════════════════════════════════════════════════════════════════════════
   rawrxd_test.c — Test Suite for RawrXD Weight Re-quantizer
   ═══════════════════════════════════════════════════════════════════════════ */

#include "rawrxd.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Running test: %s... ", #name); \
    try { \
        test_##name(); \
        printf("PASSED\n"); \
        tests_passed++; \
    } catch (...) { \
        printf("FAILED\n"); \
        tests_failed++; \
    } \
} while(0)

#define ASSERT_TRUE(cond) if (!(cond)) { printf("FAILED: %s\n", #cond); tests_failed++; return; }
#define ASSERT_FALSE(cond) if (cond) { printf("FAILED: !%s\n", #cond); tests_failed++; return; }
#define ASSERT_EQ(a, b) if ((a) != (b)) { printf("FAILED: %s != %s\n", #a, #b); tests_failed++; return; }
#define ASSERT_NE(a, b) if ((a) == (b)) { printf("FAILED: %s == %s\n", #a, #b); tests_failed++; return; }
#define ASSERT_FLOAT_EQ(a, b) if (fabsf((a) - (b)) > 0.001f) { printf("FAILED: %s != %s\n", #a, #b); tests_failed++; return; }

/* ═══════════════════════════════════════════════════════════════════════════
   Half Precision Tests
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(half_to_float) {
    ASSERT_FLOAT_EQ(half_to_float(0x3C00), 1.0f);  // 1.0
    ASSERT_FLOAT_EQ(half_to_float(0x4000), 2.0f);  // 2.0
    ASSERT_FLOAT_EQ(half_to_float(0x4248), 3.14f); // ~3.14
    ASSERT_FLOAT_EQ(half_to_float(0x0000), 0.0f);  // 0.0
    ASSERT_FLOAT_EQ(half_to_float(0x8000), -0.0f); // -0.0
}

TEST(float_to_half) {
    ASSERT_EQ(float_to_half(1.0f), 0x3C00);
    ASSERT_EQ(float_to_half(2.0f), 0x4000);
    ASSERT_EQ(float_to_half(0.0f), 0x0000);
}

TEST(half_roundtrip) {
    float test_values[] = {0.0f, 1.0f, -1.0f, 0.5f, 100.0f, 0.001f, 1000.0f};
    for (int i = 0; i < 7; i++) {
        half h = float_to_half(test_values[i]);
        float f = half_to_float(h);
        float diff = fabsf(f - test_values[i]);
        float rel_err = diff / fabsf(test_values[i]);
        ASSERT_TRUE(rel_err < 0.01f || diff < 0.001f);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   Quantization Tests
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(quant_q4_0_basic) {
    float data[32];
    for (int i = 0; i < 32; i++) data[i] = (float)(i - 16) / 16.0f;
    
    QuantResult q = quant_q4_0(data, 32);
    ASSERT_TRUE(q.data != NULL);
    ASSERT_EQ(q.size, 18); // 2 bytes scale + 16 bytes qs
    
    DequantResult d = dequant_q4_0(q.data, 1);
    ASSERT_TRUE(d.data != NULL);
    ASSERT_EQ(d.count, 32);
    
    // Check reconstruction quality
    QuantQuality qual = estimate_quality(data, d.data, 32);
    ASSERT_TRUE(qual.snr_db > 20.0f); // Should have good SNR
    
    quant_free(&q);
    dequant_free(&d);
}

TEST(quant_q8_0_basic) {
    float data[32];
    for (int i = 0; i < 32; i++) data[i] = (float)(i - 16) / 128.0f;
    
    QuantResult q = quant_q8_0(data, 32);
    ASSERT_TRUE(q.data != NULL);
    ASSERT_EQ(q.size, 34); // 2 bytes scale + 32 bytes qs
    
    DequantResult d = dequant_q8_0(q.data, 1);
    ASSERT_TRUE(d.data != NULL);
    ASSERT_EQ(d.count, 32);
    
    QuantQuality qual = estimate_quality(data, d.data, 32);
    ASSERT_TRUE(qual.snr_db > 30.0f); // Q8_0 should have excellent SNR
    
    quant_free(&q);
    dequant_free(&d);
}

TEST(quant_q4_k_basic) {
    float data[256];
    for (int i = 0; i < 256; i++) data[i] = (float)(i - 128) / 128.0f;
    
    QuantResult q = quant_q4_k(data, 256);
    ASSERT_TRUE(q.data != NULL);
    ASSERT_EQ(q.size, sizeof(block_q4_k));
    
    DequantResult d = dequant_q4_k(q.data, 1);
    ASSERT_TRUE(d.data != NULL);
    ASSERT_EQ(d.count, 256);
    
    QuantQuality qual = estimate_quality(data, d.data, 256);
    ASSERT_TRUE(qual.snr_db > 15.0f);
    
    quant_free(&q);
    dequant_free(&d);
}

TEST(dequant_tensor_generic) {
    // Test F32 passthrough
    float data[16];
    for (int i = 0; i < 16; i++) data[i] = (float)i;
    
    DequantResult d = dequant_tensor(data, GGML_RXD_TYPE_F32, 16);
    ASSERT_TRUE(d.data != NULL);
    ASSERT_EQ(d.count, 16);
    
    for (int i = 0; i < 16; i++) {
        ASSERT_FLOAT_EQ(d.data[i], (float)i);
    }
    
    dequant_free(&d);
}

TEST(estimate_quality_perfect) {
    float data[100];
    for (int i = 0; i < 100; i++) data[i] = (float)i;
    
    QuantQuality q = estimate_quality(data, data, 100);
    ASSERT_FLOAT_EQ(q.mse, 0.0f);
    ASSERT_TRUE(q.snr_db > 100.0f || q.snr_db == INFINITY);
}

TEST(estimate_quality_noisy) {
    float orig[100], noisy[100];
    for (int i = 0; i < 100; i++) {
        orig[i] = (float)i;
        noisy[i] = (float)i + 0.1f; // Add small noise
    }
    
    QuantQuality q = estimate_quality(orig, noisy, 100);
    ASSERT_TRUE(q.mse > 0.0f);
    ASSERT_TRUE(q.snr_db > 20.0f);
}

/* ═══════════════════════════════════════════════════════════════════════════
   GGUF Loading Tests
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(gguf_load_invalid) {
    GGUFContext* ctx = gguf_load("nonexistent_file.gguf");
    ASSERT_TRUE(ctx == NULL);
}

TEST(gguf_load_valid) {
    // Create a minimal test GGUF file
    FILE* f = fopen("test_model.gguf", "wb");
    if (!f) {
        printf("SKIPPED (cannot create test file)\n");
        tests_passed++;
        return;
    }
    
    // Write GGUF header
    uint32_t magic = GGUF_MAGIC;
    uint32_t version = GGUF_VERSION_3;
    uint64_t tensor_count = 1;
    uint64_t metadata_count = 0;
    
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&tensor_count, 8, 1, f);
    fwrite(&metadata_count, 8, 1, f);
    
    // Write tensor info
    uint64_t name_len = 4;
    char name[] = "test";
    fwrite(&name_len, 8, 1, f);
    fwrite(name, 1, name_len, f);
    
    uint32_t n_dims = 1;
    fwrite(&n_dims, 4, 1, f);
    
    uint64_t dim = 32;
    fwrite(&dim, 8, 1, f);
    
    uint32_t type = GGML_RXD_TYPE_Q4_0;
    fwrite(&type, 4, 1, f);
    
    uint64_t offset = 0;
    fwrite(&offset, 8, 1, f);
    
    // Write tensor data (18 bytes for Q4_0 block)
    uint8_t tensor_data[18] = {0};
    fwrite(tensor_data, 1, 18, f);
    
    fclose(f);
    
    // Load and verify
    GGUFContext* ctx = gguf_load("test_model.gguf");
    ASSERT_TRUE(ctx != NULL);
    ASSERT_TRUE(ctx->is_loaded);
    ASSERT_EQ(ctx->header.magic, GGUF_MAGIC);
    ASSERT_EQ(ctx->header.version, GGUF_VERSION_3);
    ASSERT_EQ(ctx->header.tensor_count, 1);
    
    gguf_close(ctx);
    
    // Cleanup
    remove("test_model.gguf");
}

/* ═══════════════════════════════════════════════════════════════════════════
   Hotswap Tests
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(hotswap_create_destroy) {
    // Create a minimal test file
    FILE* f = fopen("test_hotswap.gguf", "wb");
    if (!f) { printf("SKIPPED\n"); tests_passed++; return; }
    
    uint32_t magic = GGUF_MAGIC;
    uint32_t version = GGUF_VERSION_3;
    uint64_t tensor_count = 1;
    uint64_t metadata_count = 0;
    
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&tensor_count, 8, 1, f);
    fwrite(&metadata_count, 8, 1, f);
    
    uint64_t name_len = 4;
    char name[] = "test";
    fwrite(&name_len, 8, 1, f);
    fwrite(name, 1, name_len, f);
    
    uint32_t n_dims = 1;
    fwrite(&n_dims, 4, 1, f);
    uint64_t dim = 32;
    fwrite(&dim, 8, 1, f);
    uint32_t type = GGML_RXD_TYPE_Q4_0;
    fwrite(&type, 4, 1, f);
    uint64_t offset = 0;
    fwrite(&offset, 8, 1, f);
    
    uint8_t tensor_data[18] = {0};
    fwrite(tensor_data, 1, 18, f);
    fclose(f);
    
    HotswapSession* s = hotswap_create("test_hotswap.gguf");
    ASSERT_TRUE(s != NULL);
    ASSERT_EQ(s->tensor_count, 1);
    
    hotswap_destroy(s);
    remove("test_hotswap.gguf");
}

TEST(hotswap_get_tensor) {
    FILE* f = fopen("test_tensor.gguf", "wb");
    if (!f) { printf("SKIPPED\n"); tests_passed++; return; }
    
    uint32_t magic = GGUF_MAGIC;
    uint32_t version = GGUF_VERSION_3;
    uint64_t tensor_count = 2;
    uint64_t metadata_count = 0;
    
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&tensor_count, 8, 1, f);
    fwrite(&metadata_count, 8, 1, f);
    
    // Tensor 1
    uint64_t name_len = 10;
    char name1[] = "attn.q_proj";
    fwrite(&name_len, 8, 1, f);
    fwrite(name1, 1, name_len, f);
    uint32_t n_dims = 1; fwrite(&n_dims, 4, 1, f);
    uint64_t dim = 32; fwrite(&dim, 8, 1, f);
    uint32_t type = GGML_RXD_TYPE_Q4_0; fwrite(&type, 4, 1, f);
    uint64_t offset = 0; fwrite(&offset, 8, 1, f);
    
    // Tensor 2
    name_len = 8;
    char name2[] = "mlp.up_proj";
    fwrite(&name_len, 8, 1, f);
    fwrite(name2, 1, name_len, f);
    n_dims = 1; fwrite(&n_dims, 4, 1, f);
    dim = 32; fwrite(&dim, 8, 1, f);
    type = GGML_RXD_TYPE_Q4_0; fwrite(&type, 4, 1, f);
    offset = 18; fwrite(&offset, 8, 1, f);
    
    uint8_t tensor_data[36] = {0};
    fwrite(tensor_data, 1, 36, f);
    fclose(f);
    
    HotswapSession* s = hotswap_create("test_tensor.gguf");
    ASSERT_TRUE(s != NULL);
    
    WeightTensor* t1 = hotswap_get_tensor(s, "attn.q_proj");
    ASSERT_TRUE(t1 != NULL);
    ASSERT_TRUE(strstr(t1->name, "attn") != NULL);
    
    WeightTensor* t2 = hotswap_get_tensor(s, "mlp.up_proj");
    ASSERT_TRUE(t2 != NULL);
    ASSERT_TRUE(strstr(t2->name, "mlp") != NULL);
    
    WeightTensor* t3 = hotswap_get_tensor(s, "nonexistent");
    ASSERT_TRUE(t3 == NULL);
    
    hotswap_destroy(s);
    remove("test_tensor.gguf");
}

TEST(hotswap_backup_restore) {
    FILE* f = fopen("test_backup.gguf", "wb");
    if (!f) { printf("SKIPPED\n"); tests_passed++; return; }
    
    uint32_t magic = GGUF_MAGIC;
    uint32_t version = GGUF_VERSION_3;
    uint64_t tensor_count = 1;
    uint64_t metadata_count = 0;
    
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&tensor_count, 8, 1, f);
    fwrite(&metadata_count, 8, 1, f);
    
    uint64_t name_len = 4;
    char name[] = "test";
    fwrite(&name_len, 8, 1, f);
    fwrite(name, 1, name_len, f);
    uint32_t n_dims = 1; fwrite(&n_dims, 4, 1, f);
    uint64_t dim = 32; fwrite(&dim, 8, 1, f);
    uint32_t type = GGML_RXD_TYPE_Q4_0; fwrite(&type, 4, 1, f);
    uint64_t offset = 0; fwrite(&offset, 8, 1, f);
    
    uint8_t tensor_data[18] = {0};
    fwrite(tensor_data, 1, 18, f);
    fclose(f);
    
    HotswapSession* s = hotswap_create("test_backup.gguf");
    ASSERT_TRUE(s != NULL);
    
    ASSERT_TRUE(hotswap_backup(s));
    ASSERT_TRUE(s->has_backup);
    
    ASSERT_TRUE(hotswap_restore(s));
    
    hotswap_destroy(s);
    remove("test_backup.gguf");
}

/* ═══════════════════════════════════════════════════════════════════════════
   Lockpick Tests
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(lockpick_timer) {
    LockpickTimer t;
    lockpick_timer_start(&t);
    
    // Do some work
    volatile int x = 0;
    for (int i = 0; i < 1000000; i++) x += i;
    
    uint64_t ns = lockpick_timer_stop(&t);
    ASSERT_TRUE(ns > 0);
    ASSERT_TRUE(t.total_ns == ns);
}

TEST(lockpick_score) {
    LockpickMetrics m = {0};
    m.tokens_per_sec = 50.0;
    
    BatchQuality q = {0};
    q.avg_snr_db = 20.0f;
    
    float score = lockpick_score(&m, &q);
    ASSERT_TRUE(score > 0.0f);
    ASSERT_TRUE(score < 1.0f);
}

TEST(lockpick_add_experiment) {
    FILE* f = fopen("test_exp.gguf", "wb");
    if (!f) { printf("SKIPPED\n"); tests_passed++; return; }
    
    uint32_t magic = GGUF_MAGIC;
    uint32_t version = GGUF_VERSION_3;
    uint64_t tensor_count = 1;
    uint64_t metadata_count = 0;
    
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&tensor_count, 8, 1, f);
    fwrite(&metadata_count, 8, 1, f);
    
    uint64_t name_len = 4;
    char name[] = "test";
    fwrite(&name_len, 8, 1, f);
    fwrite(name, 1, name_len, f);
    uint32_t n_dims = 1; fwrite(&n_dims, 4, 1, f);
    uint64_t dim = 32; fwrite(&dim, 8, 1, f);
    uint32_t type = GGML_RXD_TYPE_Q4_0; fwrite(&type, 4, 1, f);
    uint64_t offset = 0; fwrite(&offset, 8, 1, f);
    
    uint8_t tensor_data[18] = {0};
    fwrite(tensor_data, 1, 18, f);
    fclose(f);
    
    LockpickSession* s = lockpick_create("test_exp.gguf");
    ASSERT_TRUE(s != NULL);
    
    uint32_t idx1 = lockpick_add_experiment(s, "speed", &QUANT_PROFILE_SPEED);
    ASSERT_EQ(idx1, 0);
    
    uint32_t idx2 = lockpick_add_experiment(s, "quality", &QUANT_PROFILE_QUALITY);
    ASSERT_EQ(idx2, 1);
    
    ASSERT_EQ(s->experiment_count, 2);
    
    lockpick_destroy(s);
    remove("test_exp.gguf");
}

TEST(lockpick_generate_json) {
    FILE* f = fopen("test_json.gguf", "wb");
    if (!f) { printf("SKIPPED\n"); tests_passed++; return; }
    
    uint32_t magic = GGUF_MAGIC;
    uint32_t version = GGUF_VERSION_3;
    uint64_t tensor_count = 1;
    uint64_t metadata_count = 0;
    
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&tensor_count, 8, 1, f);
    fwrite(&metadata_count, 8, 1, f);
    
    uint64_t name_len = 4;
    char name[] = "test";
    fwrite(&name_len, 8, 1, f);
    fwrite(name, 1, name_len, f);
    uint32_t n_dims = 1; fwrite(&n_dims, 4, 1, f);
    uint64_t dim = 32; fwrite(&dim, 8, 1, f);
    uint32_t type = GGML_RXD_TYPE_Q4_0; fwrite(&type, 4, 1, f);
    uint64_t offset = 0; fwrite(&offset, 8, 1, f);
    
    uint8_t tensor_data[18] = {0};
    fwrite(tensor_data, 1, 18, f);
    fclose(f);
    
    LockpickSession* s = lockpick_create("test_json.gguf");
    ASSERT_TRUE(s != NULL);
    
    lockpick_add_experiment(s, "test", &QUANT_PROFILE_SPEED);
    
    LockpickReport report = lockpick_generate_json(s);
    ASSERT_TRUE(report.data != NULL);
    ASSERT_TRUE(report.size > 0);
    ASSERT_TRUE(strstr(report.data, "experiments") != NULL);
    
    lockpick_free_report(&report);
    lockpick_destroy(s);
    remove("test_json.gguf");
}

/* ═══════════════════════════════════════════════════════════════════════════
   Main
   ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char** argv) {
    printf("=== RawrXD Test Suite ===\n\n");
    
    printf("--- Half Precision Tests ---\n");
    test_half_to_float();
    test_float_to_half();
    test_half_roundtrip();
    
    printf("\n--- Quantization Tests ---\n");
    test_quant_q4_0_basic();
    test_quant_q8_0_basic();
    test_quant_q4_k_basic();
    test_dequant_tensor_generic();
    test_estimate_quality_perfect();
    test_estimate_quality_noisy();
    
    printf("\n--- GGUF Loading Tests ---\n");
    test_gguf_load_invalid();
    test_gguf_load_valid();
    
    printf("\n--- Hotswap Tests ---\n");
    test_hotswap_create_destroy();
    test_hotswap_get_tensor();
    test_hotswap_backup_restore();
    
    printf("\n--- Lockpick Tests ---\n");
    test_lockpick_timer();
    test_lockpick_score();
    test_lockpick_add_experiment();
    test_lockpick_generate_json();
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}