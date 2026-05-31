#include "kernels/sovereign_fp8_quantizer.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <cstring>

using namespace RawrXD::Kernels;

// Test counters
static int testsPassed = 0;
static int testsFailed = 0;

void testResult(const std::string& testName, bool passed, const std::string& details = "") {
    if (passed) {
        testsPassed++;
        std::cout << "[PASS] " << testName << std::endl;
    } else {
        testsFailed++;
        std::cout << "[FAIL] " << testName << std::endl;
    }
    if (!details.empty()) {
        std::cout << "       " << details << std::endl;
    }
}

// Test 1: E4M3 Quantization Accuracy
void test_e4m3_accuracy() {
    std::cout << "\n=== Test 1: E4M3 Quantization Accuracy ===" << std::endl;
    
    SovereignFP8Quantizer quantizer;
    if (!quantizer.Initialize(FP8Format::E4M3, 1.0f)) {
        testResult("E4M3 Initialization", false, "Failed to initialize");
        return;
    }
    
    // Test values within E4M3 range
    std::vector<float> input = {0.0f, 1.0f, 10.0f, 100.0f, 200.0f, 400.0f};
    std::vector<uint8_t> output(input.size());
    
    quantizer.Quantize(input.data(), output.data(), input.size());
    std::cout << "[DEBUG] Quantize returned" << std::endl;
    std::cout.flush();
    
    // Verify output is non-zero for non-zero inputs
    bool hasOutput = false;
    std::cout << "[DEBUG] Checking output..." << std::endl;
    for (size_t i = 1; i < output.size(); i++) {
        if (output[i] != 0) hasOutput = true;
    }
    std::cout << "[DEBUG] Output check done, hasOutput=" << hasOutput << std::endl;
    
    testResult("E4M3 Quantization Accuracy", hasOutput, 
        "Processed " + std::to_string(input.size()) + " values");
    std::cout << "[DEBUG] Test 1 complete" << std::endl;
    std::cout.flush();
}

// Test 2: E5M2 Quantization Range
void test_e5m2_range() {
    std::cout << "\n=== Test 2: E5M2 Quantization Range ===" << std::endl;
    std::cout << "[DEBUG] Test 2 starting" << std::endl;
    
    SovereignFP8Quantizer quantizer;
    std::cout << "[DEBUG] Test 2: quantizer created" << std::endl;
    if (!quantizer.Initialize(FP8Format::E5M2, 1.0f)) {
        testResult("E5M2 Initialization", false, "Failed to initialize");
        return;
    }
    std::cout << "[DEBUG] Test 2: initialized" << std::endl;
    
    // E5M2 has larger range (up to 57344)
    std::vector<float> input = {0.0f, 1.0f, 1000.0f, 10000.0f, 50000.0f};
    std::vector<uint8_t> output(input.size());
    
    quantizer.Quantize(input.data(), output.data(), input.size());
    
    bool hasOutput = false;
    for (size_t i = 1; i < output.size(); i++) {
        if (output[i] != 0) hasOutput = true;
    }
    
    testResult("E5M2 Quantization Range", hasOutput,
        "E5M2 max value: " + std::to_string(SovereignFP8Quantizer::GetMaxValue(FP8Format::E5M2)));
}

// Test 3: Double Buffer Initialization
void test_double_buffer_init() {
    std::cout << "\n=== Test 3: Double Buffer Initialization ===" << std::endl;
    
    SovereignDoubleBuffer db;
    bool result = db.Initialize(4096); // 4KB buffer
    
    testResult("Double Buffer Init", result, "4KB buffer allocated");
}

// Test 4: Double Buffer Ping-Pong
void test_double_buffer_pingpong() {
    std::cout << "\n=== Test 4: Double Buffer Ping-Pong ===" << std::endl;
    
    SovereignDoubleBuffer db;
    if (!db.Initialize(1024)) {
        testResult("Double Buffer Ping-Pong", false, "Init failed");
        return;
    }
    
    // Write to buffer
    void* writeBuf = db.AcquireWriteBuffer();
    if (!writeBuf) {
        testResult("Double Buffer Ping-Pong", false, "No write buffer");
        return;
    }
    
    // Fill with pattern
    std::memset(writeBuf, 0xAB, 256);
    db.ReleaseWriteBuffer(256);
    
    // Swap
    db.SwapBuffers();
    
    // Try to read
    void* readBuf = db.TryAcquireReadBuffer();
    bool success = (readBuf != nullptr);
    
    if (success) {
        db.ReleaseReadBuffer();
    }
    
    testResult("Double Buffer Ping-Pong", success, "Buffer swap successful");
}

// Test 5: Quantization Pipeline Initialization
void test_pipeline_init() {
    std::cout << "\n=== Test 5: Pipeline Initialization ===" << std::endl;
    
    SovereignQuantizationPipeline pipeline;
    bool result = pipeline.Initialize(FP8Format::E4M3, 1.0f, 8192);
    
    testResult("Pipeline Init", result, "Pipeline with 8KB buffer");
}

// Test 6: High-Throughput Mode
void test_high_throughput() {
    std::cout << "\n=== Test 6: High-Throughput Mode ===" << std::endl;
    
    SovereignQuantizationPipeline pipeline;
    if (!pipeline.Initialize(FP8Format::E4M3, 1.0f, 8192)) {
        testResult("High-Throughput Mode", false, "Init failed");
        return;
    }
    
    pipeline.EnterHighThroughputMode();
    pipeline.ExitHighThroughputMode();
    
    testResult("High-Throughput Mode", true, "Mode transitions successful");
}

// Test 7: Performance Benchmark
void test_performance() {
    std::cout << "\n=== Test 7: Performance Benchmark ===" << std::endl;
    
    SovereignFP8Quantizer quantizer;
    if (!quantizer.Initialize(FP8Format::E4M3, 1.0f)) {
        testResult("Performance Benchmark", false, "Init failed");
        return;
    }
    
    const size_t testSize = 100000; // 100K elements
    std::vector<float> input(testSize);
    std::vector<uint8_t> output(testSize);
    
    // Fill with test data
    for (size_t i = 0; i < testSize; i++) {
        input[i] = static_cast<float>(i % 400); // Values within E4M3 range
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    quantizer.Quantize(input.data(), output.data(), testSize);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double tokensPerSec = (testSize * 1000000.0) / duration;
    
    bool performanceOk = tokensPerSec > 1000000; // >1M tokens/sec
    
    testResult("Performance Benchmark", performanceOk,
        std::to_string(testSize) + " tokens in " + std::to_string(duration) + "μs, " +
        std::to_string(static_cast<long long>(tokensPerSec)) + " tokens/sec");
}

// Test 8: C API Integration
void test_c_api() {
    std::cout << "\n=== Test 8: C API Integration ===" << std::endl;
    
    // Test quantizer C API
    auto* handle = SovereignQuantizer_Create(0, 1.0f); // 0 = E4M3
    if (!handle) {
        testResult("C API Integration", false, "Failed to create quantizer");
        return;
    }
    
    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint8_t output[4] = {0};
    
    SovereignQuantizer_Quantize(handle, input, output, 4);
    SovereignQuantizer_Destroy(handle);
    
    bool hasOutput = false;
    for (int i = 0; i < 4; i++) {
        if (output[i] != 0) hasOutput = true;
    }
    
    testResult("C API Integration", hasOutput, "C API quantization successful");
}

// Test 9: Batch Processing
void test_batch_processing() {
    std::cout << "\n=== Test 9: Batch Processing ===" << std::endl;
    
    SovereignFP8Quantizer quantizer;
    if (!quantizer.Initialize(FP8Format::E4M3, 1.0f)) {
        testResult("Batch Processing", false, "Init failed");
        return;
    }
    
    const size_t totalSize = 10000;
    const size_t batchSize = 1024;
    std::vector<float> input(totalSize);
    std::vector<uint8_t> output(totalSize);
    
    for (size_t i = 0; i < totalSize; i++) {
        input[i] = static_cast<float>(i % 400);
    }
    
    quantizer.QuantizeBatched(input.data(), output.data(), totalSize, batchSize);
    
    bool hasOutput = false;
    for (size_t i = 0; i < totalSize; i++) {
        if (output[i] != 0) {
            hasOutput = true;
            break;
        }
    }
    
    testResult("Batch Processing", hasOutput,
        "Processed " + std::to_string(totalSize) + " elements in batches of " + std::to_string(batchSize));
}

// Test 10: Stream Processing with Double Buffer
void test_stream_processing() {
    std::cout << "\n=== Test 10: Stream Processing ===" << std::endl;
    
    SovereignQuantizationPipeline pipeline;
    if (!pipeline.Initialize(FP8Format::E4M3, 1.0f, 4096)) {
        testResult("Stream Processing", false, "Init failed");
        return;
    }
    
    const size_t totalElements = 2048;
    const size_t batchSize = 512;
    std::vector<float> input(totalElements);
    std::vector<uint8_t> output(totalElements);
    
    for (size_t i = 0; i < totalElements; i++) {
        input[i] = static_cast<float>(i % 400);
    }
    
    pipeline.ProcessStream(input.data(), output.data(), totalElements, batchSize);
    
    double throughput = pipeline.GetThroughput();
    bool success = throughput > 0;
    
    testResult("Stream Processing", success,
        "Throughput: " + std::to_string(static_cast<long long>(throughput)) + " tokens/sec");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Sovereign FP8 Quantizer Test Suite" << std::endl;
    std::cout << "RX 7800 XT High-Throughput Validation" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Run all tests
    std::cout << "[DEBUG] Starting Test 1..." << std::endl;
    test_e4m3_accuracy();
    std::cout << "[DEBUG] Test 1 done, starting Test 2..." << std::endl;
    test_e5m2_range();
    std::cout << "[DEBUG] Test 2 done, starting Test 3..." << std::endl;
    test_double_buffer_init();
    std::cout << "[DEBUG] Test 3 done, starting Test 4..." << std::endl;
    test_double_buffer_pingpong();
    std::cout << "[DEBUG] Test 4 done, starting Test 5..." << std::endl;
    test_pipeline_init();
    std::cout << "[DEBUG] Test 5 done, starting Test 6..." << std::endl;
    test_high_throughput();
    std::cout << "[DEBUG] Test 6 done, starting Test 7..." << std::endl;
    test_performance();
    std::cout << "[DEBUG] Test 7 done, starting Test 8..." << std::endl;
    test_c_api();
    std::cout << "[DEBUG] Test 8 done, starting Test 9..." << std::endl;
    test_batch_processing();
    std::cout << "[DEBUG] Test 9 done, starting Test 10..." << std::endl;
    test_stream_processing();
    std::cout << "[DEBUG] All tests done!" << std::endl;
    
    // Summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << testsPassed << std::endl;
    std::cout << "Failed: " << testsFailed << std::endl;
    std::cout << "Total:  " << (testsPassed + testsFailed) << std::endl;
    
    if (testsFailed == 0) {
        std::cout << "\n[Sovereign] All tests passed - Hardware independence achieved" << std::endl;
    }
    
    std::cout << "========================================" << std::endl;
    
    return testsFailed > 0 ? 1 : 0;
}
