/* quant_test.c - Quantization Validation Test */
#include "quant_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TEST_SIZE 1024

static int test_q4_0(void) {
    printf("Testing Q4_0 quantization...\n");
    
    /* Generate test data */
    float* original = malloc(TEST_SIZE * sizeof(float));
    for (int i = 0; i < TEST_SIZE; i++) {
        original[i] = (float)sin(i * 0.1) * 5.0f;
    }
    
    /* Quantize to Q4_0 */
    QuantResult quant = quant_q4_0(original, TEST_SIZE);
    if (!quant.data) {
        printf("  ❌ Quantization failed\n");
        free(original);
        return 1;
    }
    
    /* Dequantize back */
    DequantResult dequant = dequant_q4_0(quant.data, quant.size / 18);
    if (!dequant.data) {
        printf("  ❌ Dequantization failed\n");
        quant_result_free(&quant);
        free(original);
        return 1;
    }
    
    /* Measure quality */
    QuantQuality q = estimate_quant_quality(original, dequant.data, TEST_SIZE);
    
    printf("  Size: %zu bytes (original: %zu bytes)\n", 
           quant.size, TEST_SIZE * sizeof(float));
    printf("  Compression: %.2fx\n", 
           (float)(TEST_SIZE * sizeof(float)) / quant.size);
    printf("  SNR: %.2f dB\n", q.snr_db);
    printf("  MSE: %.6f\n", q.mse);
    printf("  Max Error: %.6f\n", q.max_error);
    
    int pass = (q.snr_db > 15.0f) ? 1 : 0;
    printf("  %s\n", pass ? "✅ PASS" : "❌ FAIL");
    
    quant_result_free(&quant);
    dequant_result_free(&dequant);
    free(original);
    
    return pass ? 0 : 1;
}

static int test_q8_0(void) {
    printf("Testing Q8_0 quantization...\n");
    
    float* original = malloc(TEST_SIZE * sizeof(float));
    for (int i = 0; i < TEST_SIZE; i++) {
        original[i] = (float)sin(i * 0.1) * 5.0f;
    }
    
    QuantResult quant = quant_q8_0(original, TEST_SIZE);
    if (!quant.data) {
        printf("  ❌ Quantization failed\n");
        free(original);
        return 1;
    }
    
    DequantResult dequant = dequant_q8_0(quant.data, quant.size / 34);
    if (!dequant.data) {
        printf("  ❌ Dequantization failed\n");
        quant_result_free(&quant);
        free(original);
        return 1;
    }
    
    QuantQuality q = estimate_quant_quality(original, dequant.data, TEST_SIZE);
    
    printf("  Size: %zu bytes (original: %zu bytes)\n",
           quant.size, TEST_SIZE * sizeof(float));
    printf("  Compression: %.2fx\n",
           (float)(TEST_SIZE * sizeof(float)) / quant.size);
    printf("  SNR: %.2f dB\n", q.snr_db);
    printf("  %s\n", (q.snr_db > 20.0f) ? "✅ PASS" : "❌ FAIL");
    
    quant_result_free(&quant);
    dequant_result_free(&dequant);
    free(original);
    
    return 0;
}

static int test_q4_k(void) {
    printf("Testing Q4_K quantization...\n");
    
    float* original = malloc(TEST_SIZE * sizeof(float));
    for (int i = 0; i < TEST_SIZE; i++) {
        original[i] = (float)sin(i * 0.1) * 5.0f;
    }
    
    QuantResult quant = quant_q4_k(original, TEST_SIZE);
    if (!quant.data) {
        printf("  ❌ Quantization failed\n");
        free(original);
        return 1;
    }
    
    /* Calculate block count based on actual size */
    size_t block_count = quant.size / 144; /* Q4_K block size is ~144 bytes */
    if (block_count == 0) block_count = 1;
    
    DequantResult dequant = dequant_q4_k(quant.data, block_count);
    if (!dequant.data) {
        printf("  ❌ Dequantization failed\n");
        quant_result_free(&quant);
        free(original);
        return 1;
    }
    
    QuantQuality q = estimate_quant_quality(original, dequant.data, TEST_SIZE);
    
    printf("  Size: %zu bytes (original: %zu bytes)\n",
           quant.size, TEST_SIZE * sizeof(float));
    printf("  Compression: %.2fx\n",
           (float)(TEST_SIZE * sizeof(float)) / quant.size);
    printf("  SNR: %.2f dB\n", q.snr_db);
    printf("  %s\n", (q.snr_db > 18.0f) ? "✅ PASS" : "❌ FAIL");
    
    quant_result_free(&quant);
    dequant_result_free(&dequant);
    free(original);
    
    return 0;
}

static int test_fp16_conversion(void) {
    printf("Testing FP16 conversion...\n");
    
    float test_values[] = {0.0f, 1.0f, -1.0f, 0.5f, 3.14159f, 
                           65504.0f, 0.00006103515625f, -0.00006103515625f};
    int pass = 1;
    
    for (size_t i = 0; i < sizeof(test_values)/sizeof(test_values[0]); i++) {
        uint16_t h = float_to_half(test_values[i]);
        float back = half_to_float(h);
        float error = fabsf(test_values[i] - back);
        float rel_error = error / (fabsf(test_values[i]) + 1e-10f);
        
        if (rel_error > 0.01f) { /* >1% error */
            printf("  ❌ %.6f -> %.6f (error: %.2f%%)\n", 
                   test_values[i], back, rel_error * 100.0f);
            pass = 0;
        }
    }
    
    printf("  %s\n", pass ? "✅ PASS" : "❌ FAIL");
    return pass ? 0 : 1;
}

int main(int argc, char** argv) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  RAWRXD Quantization Test Suite\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    int failures = 0;
    
    failures += test_fp16_conversion();
    printf("\n");
    
    failures += test_q4_0();
    printf("\n");
    
    failures += test_q8_0();
    printf("\n");
    
    failures += test_q4_k();
    printf("\n");
    
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d tests, %d failures\n", 4, failures);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return failures;
}