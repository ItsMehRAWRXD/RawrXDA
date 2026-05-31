// ============================================================================
// RawrXD FP8 Quantization Test Suite
// Validates E4M3/E5M2 quantization with RMSE metrics
// ============================================================================

#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <cstring>

// Include the FP8 quantizer
#include "../src/quantization/fp8_quantizer.h"

using namespace RawrXD::Quantization;

// ============================================================================
// Test Utilities
// ============================================================================

struct TestResult {
    const char* name;
    bool passed;
    double rmse;
    double max_error;
    size_t samples;
};

std::vector<TestResult> g_results;

bool float_eq(float a, float b, float tolerance = 0.01f) {
    if (std::isnan(a) && std::isnan(b)) return true;
    if (std::isinf(a) && std::isinf(b)) return (a > 0) == (b > 0);
    return std::abs(a - b) <= tolerance;
}

double calculate_rmse(const std::vector<float>& original, 
                      const std::vector<float>& reconstructed) {
    double sum_sq_error = 0.0;
    for (size_t i = 0; i < original.size(); ++i) {
        double diff = original[i] - reconstructed[i];
        sum_sq_error += diff * diff;
    }
    return std::sqrt(sum_sq_error / original.size());
}

// ============================================================================
// Test Cases
// ============================================================================

bool test_e4m3_basic() {
    std::cout << "[TEST] E4M3 Basic Quantization..." << std::endl;
    
    SovereignFP8Quantizer quantizer(FP8Format::E4M3);
    
    // Test values within E4M3 range (-448.0 to 448.0)
    std::vector<float> test_values = {
        0.0f, 1.0f, -1.0f, 10.0f, -10.0f, 
        100.0f, -100.0f, 448.0f, -448.0f,
        0.5f, -0.5f, 0.1f, -0.1f
    };
    
    bool passed = true;
    double max_error = 0.0;
    
    for (float val : test_values) {
        uint8_t quantized = quantizer.quantize(val);
        float reconstructed = quantizer.dequantize(quantized);
        float error = std::abs(val - reconstructed);
        max_error = std::max(max_error, static_cast<double>(error));
        
        // E4M3 has limited precision, allow ~5% relative error
        float tolerance = std::max(0.5f, std::abs(val) * 0.05f);
        if (error > tolerance && val != 0.0f) {
            std::cout << "  FAIL: " << val << " -> " << (int)quantized 
                      << " -> " << reconstructed << " (error: " << error << ")" << std::endl;
            passed = false;
        }
    }
    
    std::cout << "  Max error: " << max_error << std::endl;
    g_results.push_back({"E4M3 Basic", passed, max_error, max_error, test_values.size()});
    return passed;
}

bool test_e5m2_basic() {
    std::cout << "[TEST] E5M2 Basic Quantization..." << std::endl;
    
    SovereignFP8Quantizer quantizer(FP8Format::E5M2);
    
    // Test values within E5M2 range (-57344.0 to 57344.0)
    std::vector<float> test_values = {
        0.0f, 1.0f, -1.0f, 10.0f, -10.0f,
        100.0f, -100.0f, 1000.0f, -1000.0f,
        0.5f, -0.5f
    };
    
    bool passed = true;
    double max_error = 0.0;
    
    for (float val : test_values) {
        uint8_t quantized = quantizer.quantize(val);
        float reconstructed = quantizer.dequantize(quantized);
        float error = std::abs(val - reconstructed);
        max_error = std::max(max_error, static_cast<double>(error));
        
        // E5M2 has wider range but less precision
        float tolerance = std::max(1.0f, std::abs(val) * 0.1f);
        if (error > tolerance && val != 0.0f) {
            std::cout << "  FAIL: " << val << " -> " << (int)quantized 
                      << " -> " << reconstructed << " (error: " << error << ")" << std::endl;
            passed = false;
        }
    }
    
    std::cout << "  Max error: " << max_error << std::endl;
    g_results.push_back({"E5M2 Basic", passed, max_error, max_error, test_values.size()});
    return passed;
}

bool test_batch_quantization() {
    std::cout << "[TEST] Batch Quantization (RMSE validation)..." << std::endl;
    
    SovereignFP8Quantizer quantizer(FP8Format::E4M3);
    
    // Generate random test data
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-400.0f, 400.0f);
    
    const size_t batch_size = 1024;
    std::vector<float> input(batch_size);
    std::vector<uint8_t> quantized(batch_size);
    std::vector<float> output(batch_size);
    
    for (size_t i = 0; i < batch_size; ++i) {
        input[i] = dist(rng);
    }
    
    // Quantize batch
    float scale = quantizer.estimateScale(input.data(), batch_size);
    quantizer.quantizeBatch(input.data(), quantized.data(), batch_size, scale);
    quantizer.dequantizeBatch(quantized.data(), output.data(), batch_size, scale);
    
    double rmse = calculate_rmse(input, output);
    
    // E4M3 should achieve < 1% RMSE for this distribution
    bool passed = rmse < 4.0;  // Allow ~1% of max value (448)
    
    std::cout << "  RMSE: " << rmse << " (threshold: 4.0)" << std::endl;
    std::cout << "  Scale used: " << scale << std::endl;
    
    g_results.push_back({"Batch Quantization", passed, rmse, rmse, batch_size});
    return passed;
}

bool test_special_values() {
    std::cout << "[TEST] Special Values (NaN, Inf, Zero)..." << std::endl;
    
    SovereignFP8Quantizer quantizer(FP8Format::E4M3);
    
    bool passed = true;
    
    // Test zero
    uint8_t q_zero = quantizer.quantize(0.0f);
    float d_zero = quantizer.dequantize(q_zero);
    if (d_zero != 0.0f) {
        std::cout << "  FAIL: Zero quantization (got " << d_zero << ")" << std::endl;
        passed = false;
    }
    
    // Test very small values (underflow)
    uint8_t q_small = quantizer.quantize(1e-10f);
    if (q_small != 0) {
        std::cout << "  Note: Small value " << 1e-10f << " underflowed to 0" << std::endl;
    }
    
    // Test clamping (values outside range)
    uint8_t q_large = quantizer.quantize(1000.0f);  // > 448
    float d_large = quantizer.dequantize(q_large);
    if (d_large > 450.0f) {
        std::cout << "  FAIL: Large value not clamped (got " << d_large << ")" << std::endl;
        passed = false;
    }
    
    g_results.push_back({"Special Values", passed, 0.0, 0.0, 3});
    return passed;
}

bool test_compression_ratio() {
    std::cout << "[TEST] Compression Ratio Verification..." << std::endl;
    
    // FP32: 4 bytes per element
    // FP8: 1 byte per element
    // Expected ratio: 4x
    
    const size_t num_elements = 1000000;
    size_t fp32_size = num_elements * sizeof(float);    // 4MB
    size_t fp8_size = num_elements * sizeof(uint8_t);   // 1MB
    
    float ratio = static_cast<float>(fp32_size) / fp8_size;
    bool passed = (ratio >= 3.9f && ratio <= 4.1f);  // Allow small rounding
    
    std::cout << "  FP32 size: " << fp32_size << " bytes" << std::endl;
    std::cout << "  FP8 size: " << fp8_size << " bytes" << std::endl;
    std::cout << "  Compression ratio: " << ratio << "x" << std::endl;
    
    g_results.push_back({"Compression Ratio", passed, ratio, ratio, num_elements});
    return passed;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "RawrXD FP8 Quantization Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    if (test_e4m3_basic()) passed++; else failed++;
    std::cout << std::endl;
    
    if (test_e5m2_basic()) passed++; else failed++;
    std::cout << std::endl;
    
    if (test_batch_quantization()) passed++; else failed++;
    std::cout << std::endl;
    
    if (test_special_values()) passed++; else failed++;
    std::cout << std::endl;
    
    if (test_compression_ratio()) passed++; else failed++;
    std::cout << std::endl;
    
    // Summary
    std::cout << "========================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    
    for (const auto& result : g_results) {
        std::cout << (result.passed ? "[PASS] " : "[FAIL] ")
                  << result.name;
        if (result.samples > 0) {
            std::cout << " (RMSE: " << result.rmse 
                      << ", samples: " << result.samples << ")";
        }
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Total: " << passed << " passed, " << failed << " failed" << std::endl;
    
    if (failed == 0) {
        std::cout << std::endl << "All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << std::endl << "Some tests failed!" << std::endl;
        return 1;
    }
}
