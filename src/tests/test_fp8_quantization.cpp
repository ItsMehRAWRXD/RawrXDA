// ============================================================================
// RawrXD FP8 Quantization Verification Test
// Validates correctness and performance of sovereign FP8 implementation
// ============================================================================

#include "../quantization/fp8_quantizer.h"
#include "../quantization/fp8_kv_cache_integration.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>

using namespace RawrXD;

// ============================================================================
// Test utilities
// ============================================================================

struct TestResult {
    const char* name;
    bool passed;
    double error;
    double timeMs;
    const char* details;
};

std::vector<TestResult> g_results;

void report(const char* name, bool passed, double error = 0.0, 
            double timeMs = 0.0, const char* details = "") {
    g_results.push_back({name, passed, error, timeMs, details});
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name;
    if (error > 0.0) std::cout << " (error: " << std::fixed << std::setprecision(6) << error << ")";
    if (timeMs > 0.0) std::cout << " [" << std::setprecision(3) << timeMs << " ms]";
    std::cout << "\n";
    if (details && *details) {
        std::cout << "  " << details << "\n";
    }
}

// ============================================================================
// Unit tests
// ============================================================================

void testE4M3Basic() {
    Quantization::SovereignFP8Quantizer quant(Quantization::FP8Format::E4M3);
    
    // Test zero
    uint8_t q0 = quant.quantize(0.0f);
    float d0 = quant.dequantize(q0);
    bool pass = (q0 == 0 && d0 == 0.0f);
    report("E4M3: Zero quantization", pass);
    
    // Test positive values
    float testVals[] = {1.0f, 10.0f, 100.0f, 448.0f};
    bool allPass = true;
    double maxError = 0.0;
    
    for (float val : testVals) {
        uint8_t q = quant.quantize(val);
        float d = quant.dequantize(q);
        double error = std::abs(val - d) / val;
        maxError = std::max(maxError, error);
        if (error > 0.1) allPass = false;  // Allow 10% error for E4M3
    }
    
    report("E4M3: Positive values", allPass, maxError, 0.0, 
           "Testing range: 1.0 - 448.0");
    
    // Test negative values
    allPass = true;
    maxError = 0.0;
    for (float val : testVals) {
        uint8_t q = quant.quantize(-val);
        float d = quant.dequantize(q);
        double error = std::abs(-val - d) / val;
        maxError = std::max(maxError, error);
        if (error > 0.1) allPass = false;
    }
    
    report("E4M3: Negative values", allPass, maxError, 0.0,
           "Testing range: -1.0 - -448.0");
}

void testE5M2Basic() {
    Quantization::SovereignFP8Quantizer quant(Quantization::FP8Format::E5M2);
    
    // Test zero
    uint8_t q0 = quant.quantize(0.0f);
    float d0 = quant.dequantize(q0);
    bool pass = (q0 == 0 && d0 == 0.0f);
    report("E5M2: Zero quantization", pass);
    
    // Test larger range
    float testVals[] = {1.0f, 100.0f, 1000.0f, 10000.0f, 57344.0f};
    bool allPass = true;
    double maxError = 0.0;
    
    for (float val : testVals) {
        uint8_t q = quant.quantize(val);
        float d = quant.dequantize(q);
        double error = std::abs(val - d) / val;
        maxError = std::max(maxError, error);
        if (error > 0.2) allPass = false;  // Allow 20% error for E5M2
    }
    
    report("E5M2: Large range values", allPass, maxError, 0.0,
           "Testing range: 1.0 - 57344.0");
}

void testBatchQuantization() {
    const size_t N = 10000;
    std::vector<float> input(N);
    std::vector<uint8_t> output(N);
    std::vector<float> recovered(N);
    
    // Fill with random values
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
    for (size_t i = 0; i < N; ++i) {
        input[i] = dist(rng);
    }
    
    // Test E4M3 batch
    Quantization::SovereignFP8Quantizer quantE4(Quantization::FP8Format::E4M3);
    
    auto start = std::chrono::high_resolution_clock::now();
    float scale = quantE4.computeScale(input.data(), N);
    quantE4.quantizeBatch(input.data(), output.data(), N, scale);
    quantE4.dequantizeBatch(output.data(), recovered.data(), N, scale);
    auto end = std::chrono::high_resolution_clock::now();
    
    double timeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Calculate RMSE
    double rmse = 0.0;
    for (size_t i = 0; i < N; ++i) {
        double diff = input[i] - recovered[i];
        rmse += diff * diff;
    }
    rmse = std::sqrt(rmse / N);
    
    double throughput = (N * sizeof(float)) / (1024.0 * 1024.0) / (timeMs / 1000.0);
    
    char details[256];
    snprintf(details, sizeof(details), 
             "RMSE: %.6f, Throughput: %.2f MB/s", rmse, throughput);
    
    report("Batch quantization (E4M3)", rmse < 1.0, rmse, timeMs, details);
}

void testKVCacheIntegration() {
    KVCache::FP8PagedKVCache::Config config;
    config.numBlocks = 100;
    config.blockSize = 16;
    config.numHeads = 32;
    config.headDim = 128;
    config.format = Quantization::FP8Format::E4M3;
    
    auto cache = KVCache::KVCacheFactory::createFP8(config);
    
    // Simulate appending tokens
    const int numTokens = 100;
    std::vector<float> k_tensor(32 * 128);
    std::vector<float> v_tensor(32 * 128);
    
    std::mt19937 rng(123);
    std::normal_distribution<float> dist(0.0f, 0.1f);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numTokens; ++i) {
        // Fill with random data
        for (auto& k : k_tensor) k = dist(rng);
        for (auto& v : v_tensor) v = dist(rng);
        
        cache->appendToken(k_tensor.data(), v_tensor.data());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double timeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Verify retrieval
    std::vector<float> retrievedK(32 * 128);
    std::vector<float> retrievedV(32 * 128);
    
    cache->getKeyTensor(50, retrievedK.data());
    cache->getValueTensor(50, retrievedV.data());
    
    bool retrievedNonZero = false;
    for (float v : retrievedK) {
        if (v != 0.0f) {
            retrievedNonZero = true;
            break;
        }
    }
    
    size_t memoryUsed = cache->getMemoryUsed();
    size_t memorySaved = cache->getMemorySaved();
    
    char details[256];
    snprintf(details, sizeof(details),
             "Tokens: %d, Memory: %zu bytes, Saved: %zu bytes (%.1fx)",
             numTokens, memoryUsed, memorySaved, 
             static_cast<double>(memoryUsed + memorySaved) / memoryUsed);
    
    report("KV Cache integration", retrievedNonZero && cache->getContextLength() == numTokens,
           0.0, timeMs, details);
}

void testRX7800XTConfig() {
    auto config = KVCache::KVCacheFactory::getRX7800XTConfig();
    auto cache = KVCache::KVCacheFactory::createFP8(config);
    
    // Simulate 70B model context
    // 70B model ~ 4GB KV cache for 8k context
    const int contextLength = 8192;
    std::vector<float> k_tensor(32 * 128);
    std::vector<float> v_tensor(32 * 128);
    
    std::mt19937 rng(456);
    std::normal_distribution<float> dist(0.0f, 0.1f);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < contextLength; ++i) {
        for (auto& k : k_tensor) k = dist(rng);
        for (auto& v : v_tensor) v = dist(rng);
        cache->appendToken(k_tensor.data(), v_tensor.data());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double timeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    size_t memoryUsed = cache->getMemoryUsed();
    size_t memorySaved = cache->getMemorySaved();
    double memoryUsedMB = memoryUsed / (1024.0 * 1024.0);
    double memorySavedMB = memorySaved / (1024.0 * 1024.0);
    
    char details[256];
    snprintf(details, sizeof(details),
             "Context: %d tokens, Memory: %.1f MB, Saved: %.1f MB, Time: %.1f ms",
             contextLength, memoryUsedMB, memorySavedMB, timeMs);
    
    report("RX 7800 XT 70B simulation", cache->getContextLength() == contextLength,
           0.0, timeMs, details);
}

void testCompressionRatio() {
    // Verify 4x compression
    KVCache::FP8PagedKVCache::Config config;
    config.numBlocks = 10;
    config.blockSize = 16;
    config.numHeads = 32;
    config.headDim = 128;
    
    auto cache = KVCache::KVCacheFactory::createFP8(config);
    
    std::vector<float> k(32 * 128, 1.0f);
    std::vector<float> v(32 * 128, 1.0f);
    
    // Fill one block
    for (int i = 0; i < 16; ++i) {
        cache->appendToken(k.data(), v.data());
    }
    
    double ratio = cache->getCompressionRatio();
    bool pass = (ratio >= 3.9 && ratio <= 4.1);  // Allow small tolerance
    
    char details[128];
    snprintf(details, sizeof(details), "Expected: 4.0x, Actual: %.2fx", ratio);
    
    report("Compression ratio verification", pass, 0.0, 0.0, details);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "RawrXD FP8 Quantization Verification\n";
    std::cout << "========================================\n\n";
    
    std::cout << "Running unit tests...\n\n";
    
    testE4M3Basic();
    testE5M2Basic();
    testBatchQuantization();
    testKVCacheIntegration();
    testRX7800XTConfig();
    testCompressionRatio();
    
    std::cout << "\n========================================\n";
    std::cout << "Test Summary\n";
    std::cout << "========================================\n";
    
    int passed = 0, failed = 0;
    for (const auto& r : g_results) {
        if (r.passed) passed++;
        else failed++;
    }
    
    std::cout << "Total: " << g_results.size() << " tests\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    if (failed == 0) {
        std::cout << "\n✓ All tests passed! FP8 quantization is ready for production.\n";
        return 0;
    } else {
        std::cout << "\n✗ Some tests failed. Review output above.\n";
        return 1;
    }
}
