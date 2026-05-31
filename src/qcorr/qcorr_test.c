// ============================================================================
// QCORR Test - Quantization Correction System
// ============================================================================

#include "qcorr.h"
#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// Test Utilities
// ============================================================================
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s at %s:%d\n", msg, __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

static double get_time_ms(void) {
#ifdef _WIN32
    return (double)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#endif
}

// ============================================================================
// Test: Quantization/Dequantization
// ============================================================================
static int test_quantization_int4(void) {
    printf("Testing INT4 quantization...\n");
    
    float weights[] = {0.5f, -0.3f, 0.8f, -0.7f, 0.2f, -0.9f, 0.4f, 0.1f};
    uint8_t packed[4];
    float unpacked[8];
    
    float scale = 0.1f;
    float min_val = -1.0f;
    
    quantize_int4(weights, packed, 8, scale, min_val);
    dequantize_int4(packed, unpacked, 8, scale, min_val);
    
    // Check reconstruction error
    float mse = 0.0f;
    for (int i = 0; i < 8; i++) {
        float error = weights[i] - unpacked[i];
        mse += error * error;
    }
    mse /= 8;
    
    printf("  MSE: %.6f\n", mse);
    TEST_ASSERT(mse < 0.1f, "INT4 MSE too high");
    
    printf("  PASS\n");
    return 0;
}

static int test_quantization_int8(void) {
    printf("Testing INT8 quantization...\n");
    
    float weights[] = {0.5f, -0.3f, 0.8f, -0.7f, 0.2f, -0.9f, 0.4f, 0.1f, 0.6f, -0.5f};
    int8_t quant[10];
    float dequant[10];
    float scale, min_val;
    
    quantize_int8(weights, quant, 10, &scale, &min_val);
    dequantize_int8(quant, dequant, 10, scale, min_val);
    
    float mse = 0.0f;
    for (int i = 0; i < 10; i++) {
        float error = weights[i] - dequant[i];
        mse += error * error;
    }
    mse /= 10;
    
    printf("  MSE: %.6f\n", mse);
    TEST_ASSERT(mse < 0.01f, "INT8 MSE too high");
    
    printf("  PASS\n");
    return 0;
}

// ============================================================================
// Test: Tensor Quantization
// ============================================================================
static int test_tensor_quantization(void) {
    printf("Testing tensor quantization...\n");
    
    // Create test weights (128x128 matrix)
    const uint32_t rows = 128, cols = 128;
    float* weights = malloc(rows * cols * sizeof(float));
    
    for (uint32_t i = 0; i < rows * cols; i++) {
        weights[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
    }
    
    // Quantize
    QTensor tensor;
    QuantParams params = {
        .target_type = QINT4,
        .corr_mode = CORR_NONE,
        .block_size = 32,
        .outlier_percentile = 99,
        .svd_rank = 16,
        .threshold = 0.05f
    };
    
    quantize_tensor(weights, &tensor, rows, cols, &params);
    
    printf("  Compression ratio: %.2fx\n", tensor.compression_ratio);
    printf("  MSE: %.6f\n", tensor.mse);
    
    TEST_ASSERT(tensor.compression_ratio > 5.0f, "Compression ratio too low");
    TEST_ASSERT(tensor.mse < 0.1f, "MSE too high");
    
    free(weights);
    printf("  PASS\n");
    return 0;
}

// ============================================================================
// Test: Matrix Multiplication
// ============================================================================
static int test_qmatmul(void) {
    printf("Testing quantized matrix multiplication...\n");
    
    const uint32_t in_features = 256;
    const uint32_t out_features = 128;
    const uint32_t batch_size = 1;
    
    // Create test weights
    float* weights = malloc(in_features * out_features * sizeof(float));
    for (uint32_t i = 0; i < in_features * out_features; i++) {
        weights[i] = (float)rand() / RAND_MAX * 0.1f - 0.05f;
    }
    
    // Create input
    float* input = malloc(batch_size * in_features * sizeof(float));
    for (uint32_t i = 0; i < batch_size * in_features; i++) {
        input[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
    }
    
    // Quantize weights
    QTensor tensor;
    QuantParams params = {
        .target_type = QINT4,
        .corr_mode = CORR_NONE,
        .block_size = 32,
        .outlier_percentile = 99,
        .svd_rank = 16,
        .threshold = 0.05f
    };
    
    quantize_tensor(weights, &tensor, out_features, in_features, &params);
    
    // Allocate output
    float* output = malloc(batch_size * out_features * sizeof(float));
    
    // Time the multiplication
    double start = get_time_ms();
    qmatmul(&tensor, input, output, batch_size, in_features, out_features);
    double end = get_time_ms();
    
    printf("  Time: %.2f ms\n", end - start);
    printf("  Output[0]: %.4f\n", output[0]);
    
    free(weights);
    free(input);
    free(output);
    printf("  PASS\n");
    return 0;
}

// ============================================================================
// Test: Sparse Correction
// ============================================================================
static int test_sparse_correction(void) {
    printf("Testing sparse correction...\n");
    
    const uint32_t n = 1024;
    
    // Create test weights with outliers
    float* weights = malloc(n * sizeof(float));
    int8_t* quant = malloc(n * sizeof(int8_t));
    
    for (uint32_t i = 0; i < n; i++) {
        weights[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
        // Add some outliers
        if (i % 100 == 0) weights[i] *= 10.0f;
    }
    
    // Quantize
    float scale = 0.01f;
    for (uint32_t i = 0; i < n; i++) {
        quant[i] = (int8_t)fmaxf(-127.0f, fminf(127.0f, roundf(weights[i] / scale)));
    }
    
    // Find outliers
    SparseCorrection correction;
    find_outliers(weights, quant, n, scale, 99.0f, &correction);
    
    printf("  Outliers found: %u\n", correction.num_outliers);
    TEST_ASSERT(correction.num_outliers > 0, "No outliers found");
    TEST_ASSERT(correction.num_outliers < n * 0.05f, "Too many outliers");
    
    free(weights);
    free(quant);
    free(correction.indices);
    free(correction.values);
    
    printf("  PASS\n");
    return 0;
}

// ============================================================================
// Test: Memory Manager
// ============================================================================
static int test_memory_manager(void) {
    printf("Testing memory manager...\n");
    
    // Create memory manager for 16GB VRAM + 32GB RAM
    UnifiedMemory* mem = umemory_create(16ULL * 1024 * 1024 * 1024, 32ULL * 1024 * 1024 * 1024);
    TEST_ASSERT(mem != NULL, "Failed to create memory manager");
    
    // Test allocation
    void* ptr1 = umemory_alloc(mem, 1024 * 1024, MEM_VRAM, "test1");
    TEST_ASSERT(ptr1 != NULL, "Failed to allocate VRAM");
    
    void* ptr2 = umemory_alloc(mem, 10 * 1024 * 1024, MEM_RAM, "test2");
    TEST_ASSERT(ptr2 != NULL, "Failed to allocate RAM");
    
    printf("  VRAM used: %lu MB\n", (unsigned long)(mem->vram_used / (1024 * 1024)));
    printf("  RAM used: %lu MB\n", (unsigned long)(mem->ram_used / (1024 * 1024)));
    
    // Test free
    umemory_free(mem, ptr1);
    umemory_free(mem, ptr2);
    
    umemory_print_stats(mem);
    umemory_destroy(mem);
    
    printf("  PASS\n");
    return 0;
}

// ============================================================================
// Test: Model Creation
// ============================================================================
static int test_model_creation(void) {
    printf("Testing model creation...\n");
    
    // Create model
    QModel* model = qmodel_create(32, 4096, 32000, 2048);
    TEST_ASSERT(model != NULL, "Failed to create model");
    
    printf("  Layers: %u\n", model->num_layers);
    printf("  Hidden size: %u\n", model->hidden_size);
    printf("  Vocab size: %u\n", model->vocab_size);
    printf("  Max seq len: %u\n", model->max_seq_len);
    
    qmodel_print_stats(model);
    qmodel_destroy(model);
    
    printf("  PASS\n");
    return 0;
}

// ============================================================================
// Test: Inference Context
// ============================================================================
static int test_inference_context(void) {
    printf("Testing inference context...\n");
    
    QModel* model = qmodel_create(4, 256, 1000, 128);
    TEST_ASSERT(model != NULL, "Failed to create model");
    
    QContext* ctx = qcontext_create(model);
    TEST_ASSERT(ctx != NULL, "Failed to create context");
    
    // Test forward pass
    double start = get_time_ms();
    for (int i = 0; i < 10; i++) {
        qmodel_forward(ctx, i);
    }
    double end = get_time_ms();
    
    printf("  Forward pass time: %.2f ms\n", (end - start) / 10);
    printf("  Total tokens: %lu\n", (unsigned long)ctx->total_tokens);
    
    qcontext_destroy(ctx);
    qmodel_destroy(model);
    
    printf("  PASS\n");
    return 0;
}

// ============================================================================
// Test: Token Generation
// ============================================================================
static int test_token_generation(void) {
    printf("Testing token generation...\n");
    
    QModel* model = qmodel_create(4, 256, 1000, 128);
    QContext* ctx = qcontext_create(model);
    
    uint32_t prompt[] = {1, 2, 3};
    uint32_t output[20];
    
    double start = get_time_ms();
    qmodel_generate(ctx, prompt, 3, output, 20);
    double end = get_time_ms();
    
    printf("  Generation time: %.2f ms\n", end - start);
    printf("  Tokens generated: ");
    for (int i = 0; i < 20; i++) {
        printf("%u ", output[i]);
    }
    printf("\n");
    
    qcontext_destroy(ctx);
    qmodel_destroy(model);
    
    printf("  PASS\n");
    return 0;
}

// ============================================================================
// Test: Memory Strategy
// ============================================================================
static int test_memory_strategy(void) {
    printf("Testing memory strategy for 16GB...\n");
    
    MemoryStrategy16GB strategy = get_recommended_strategy_16gb();
    
    printf("  Layers in VRAM: %u\n", strategy.layers_in_vram);
    printf("  Layers in RAM: %u\n", strategy.layers_in_ram);
    printf("  Layers on disk: %u\n", strategy.layers_on_disk);
    printf("  KV cache layers: %u\n", strategy.kv_cache_layers);
    
    TEST_ASSERT(strategy.layers_in_vram == 3, "Wrong VRAM layer count");
    TEST_ASSERT(strategy.layers_in_ram == 12, "Wrong RAM layer count");
    
    printf("  PASS\n");
    return 0;
}

// ============================================================================
// Benchmark: Throughput
// ============================================================================
static int benchmark_throughput(void) {
    printf("\n=== Throughput Benchmark ===\n");
    
    QModel* model = qmodel_create(32, 4096, 32000, 2048);
    QContext* ctx = qcontext_create(model);
    
    uint32_t prompt[] = {1, 2, 3};
    uint32_t output[100];
    
    // Warmup
    qmodel_generate(ctx, prompt, 3, output, 10);
    
    // Benchmark
    double start = get_time_ms();
    qmodel_generate(ctx, prompt, 3, output, 100);
    double end = get_time_ms();
    
    double elapsed_sec = (end - start) / 1000.0;
    double tokens_per_sec = 100.0 / elapsed_sec;
    
    printf("  Generated 100 tokens in %.2f ms\n", end - start);
    printf("  Throughput: %.1f tokens/sec\n", tokens_per_sec);
    
    qcontext_destroy(ctx);
    qmodel_destroy(model);
    
    return 0;
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("QCORR - Quantization Correction Tests\n");
    printf("========================================\n\n");
    
    int failures = 0;
    
    failures += test_quantization_int4();
    failures += test_quantization_int8();
    failures += test_tensor_quantization();
    failures += test_qmatmul();
    failures += test_sparse_correction();
    failures += test_memory_manager();
    failures += test_model_creation();
    failures += test_inference_context();
    failures += test_token_generation();
    failures += test_memory_strategy();
    
    failures += benchmark_throughput();
    
    printf("\n========================================\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("FAILED: %d tests\n", failures);
    }
    printf("========================================\n");
    
    return failures;
}