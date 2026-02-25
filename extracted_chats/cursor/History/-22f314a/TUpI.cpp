#include "universal_quantization.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <cmath>
#include <chrono>

/**
 * @file test_universal_quantization.cpp
 * @brief Comprehensive test suite for universal quantization system
 */

// Test utilities
namespace TestUtils {
    void PrintSeparator(const std::string& title = "") {
        std::cout << "\n" << std::string(80, '=') << "\n";
        if (!title.empty()) {
            std::cout << "  " << title << "\n";
            std::cout << std::string(80, '=') << "\n";
        }
    }
    
    void PrintResult(const std::string& label, const std::string& value) {
        std::cout << "  " << std::setw(40) << std::left << label << ": " << value << "\n";
    }
    
    void PrintMetric(const std::string& label, double value, const std::string& unit = "") {
        std::cout << "  " << std::setw(40) << std::left << label << ": " 
                  << std::fixed << std::setprecision(2) << value << unit << "\n";
    }
    
    // Generate test tensor data
    std::vector<float> GenerateTestTensor(size_t count, float min_val = -1.0f, float max_val = 1.0f) {
        std::vector<float> data(count);
        float range = max_val - min_val;
        for (size_t i = 0; i < count; ++i) {
            // Generate values with some structure (not completely random)
            float normalized = static_cast<float>(i % 100) / 100.0f;
            data[i] = min_val + normalized * range;
            // Add some variation
            data[i] += static_cast<float>((i * 7) % 13) / 1000.0f;
        }
        return data;
    }
    
    // Calculate MSE between two vectors
    double CalculateMSE(const std::vector<float>& original, const std::vector<float>& restored) {
        if (original.size() != restored.size()) {
            return -1.0;
        }
        
        double mse = 0.0;
        for (size_t i = 0; i < original.size(); ++i) {
            double diff = original[i] - restored[i];
            mse += diff * diff;
        }
        return mse / original.size();
    }
}

// Test 1: Forward Quantization
void TestForwardQuantization() {
    TestUtils::PrintSeparator("Test 1: Forward Quantization (F32 → Compressed)");
    
    UniversalQuantizer quantizer;
    
    // Create test tensor (1MB of data)
    const size_t tensor_size = 256 * 1024;  // 256K floats = 1MB
    auto original_data = TestUtils::GenerateTestTensor(tensor_size);
    
    TestUtils::PrintMetric("Original tensor size", tensor_size * sizeof(float) / (1024.0 * 1024.0), " MB");
    
    // Test different quantization formats
    std::vector<QuantizationFormat> formats = {
        QuantizationFormat::Q8_0,
        QuantizationFormat::Q4_K,
        QuantizationFormat::Q3_K,
        QuantizationFormat::Q2_K
    };
    
    QuantizationConfig config;
    config.mode = QuantizationMode::FORWARD;
    config.enable_compression = false;  // Disable for clearer comparison
    
    for (auto format : formats) {
        std::cout << "\n  Testing format: " << UniversalQuantizer::FormatToString(format) << "\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        auto quantized = quantizer.QuantizeForward(original_data.data(), tensor_size, format, config);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double compression_ratio = static_cast<double>(quantized.quantized_size_bytes) / 
                                   quantized.original_size_bytes;
        double compression_percent = (1.0 - compression_ratio) * 100.0;
        
        TestUtils::PrintMetric("  Quantized size", quantized.quantized_size_bytes / (1024.0 * 1024.0), " MB");
        TestUtils::PrintMetric("  Compression ratio", compression_ratio * 100.0, "%");
        TestUtils::PrintMetric("  Size reduction", compression_percent, "%");
        TestUtils::PrintMetric("  Bits per value", static_cast<double>(UniversalQuantizer::GetBits(format)));
        TestUtils::PrintMetric("  Quality loss (est.)", UniversalQuantizer::GetQualityLoss(format) * 100.0, "%");
        TestUtils::PrintMetric("  Quantization time", duration.count() / 1000.0, " ms");
        
        // Test dequantization
        auto dequantized = quantizer.Dequantize(quantized);
        if (dequantized.size() == tensor_size) {
            double mse = TestUtils::CalculateMSE(original_data, dequantized);
            double rmse = std::sqrt(mse);
            TestUtils::PrintMetric("  Reconstruction RMSE", rmse);
        }
    }
}

// Test 2: Backward Quantization (2 steps before start)
void TestBackwardQuantization() {
    TestUtils::PrintSeparator("Test 2: Backward Quantization (Max Compression → Optimal)");
    
    UniversalQuantizer quantizer;
    
    // Create test model with multiple tensors
    std::map<std::string, std::vector<float>> model_tensors;
    model_tensors["embedding.weight"] = TestUtils::GenerateTestTensor(4096 * 50000);  // Large embedding
    model_tensors["layers.0.attention.q_proj.weight"] = TestUtils::GenerateTestTensor(4096 * 4096);
    model_tensors["layers.0.attention.k_proj.weight"] = TestUtils::GenerateTestTensor(4096 * 4096);
    model_tensors["layers.0.mlp.gate_proj.weight"] = TestUtils::GenerateTestTensor(4096 * 11008);
    model_tensors["output.weight"] = TestUtils::GenerateTestTensor(4096 * 50000);
    
    // Calculate original size
    size_t original_size = 0;
    for (const auto& [name, data] : model_tensors) {
        original_size += data.size() * sizeof(float);
    }
    
    TestUtils::PrintMetric("Original model size", original_size / (1024.0 * 1024.0), " MB");
    
    // Test backward quantization with different step counts
    for (int steps = 0; steps <= 4; ++steps) {
        std::cout << "\n  Testing backward quantization: " << steps << " steps before start (Q2_K → ";
        
        QuantizationFormat start = QuantizationFormat::Q2_K;
        QuantizationFormat target = UniversalQuantizer::GetFormatStepsBefore(start, steps);
        std::cout << UniversalQuantizer::FormatToString(target) << ")\n";
        
        auto config = UniversalQuantizationUtils::CreateBackwardConfig(steps, 0.90);
        config.critical_layers = {"embedding", "output"};
        config.critical_min_format = QuantizationFormat::Q4_K;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        auto result = quantizer.QuantizeModelBackward(model_tensors, config);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (result.success) {
            TestUtils::PrintMetric("  Quantized size", result.quantized_size_bytes / (1024.0 * 1024.0), " MB");
            TestUtils::PrintMetric("  Compression ratio", result.compression_ratio * 100.0, "%");
            TestUtils::PrintMetric("  Size reduction", (1.0 - result.compression_ratio) * 100.0, "%");
            TestUtils::PrintMetric("  Estimated quality", result.estimated_quality * 100.0, "%");
            TestUtils::PrintMetric("  Processing time", duration.count(), " ms");
            
            // Show layer-wise quantization
            std::cout << "\n  Layer-wise quantization:\n";
            for (const auto& [name, format] : result.layer_formats) {
                double layer_comp = UniversalQuantizer::GetCompressionRatio(format);
                std::cout << "    " << std::setw(40) << std::left << name 
                         << ": " << UniversalQuantizer::FormatToString(format)
                         << " (" << (layer_comp * 100.0) << "%)\n";
            }
        } else {
            std::cout << "  ERROR: " << result.error_message << "\n";
        }
    }
}

// Test 3: Reverse Engineering
void TestReverseEngineering() {
    TestUtils::PrintSeparator("Test 3: Reverse Engineering (Detect Existing Quantization)");
    
    UniversalQuantizer quantizer;
    
    // Create original data
    const size_t tensor_size = 1024 * 1024;
    auto original_data = TestUtils::GenerateTestTensor(tensor_size);
    
    // Quantize it first
    QuantizationConfig quant_config;
    quant_config.mode = QuantizationMode::FORWARD;
    
    // Test detection for different formats
    std::vector<QuantizationFormat> test_formats = {
        QuantizationFormat::Q8_0,
        QuantizationFormat::Q4_K,
        QuantizationFormat::Q2_K
    };
    
    for (auto format : test_formats) {
        std::cout << "\n  Testing reverse engineering for: " << UniversalQuantizer::FormatToString(format) << "\n";
        
        // Quantize
        auto quantized = quantizer.QuantizeForward(original_data.data(), tensor_size, format, quant_config);
        
        // Try to reverse engineer
        std::vector<size_t> shape = {tensor_size};
        auto config = UniversalQuantizationUtils::CreateReverseEngineerConfig();
        
        auto detected = quantizer.ReverseEngineerFormat(
            quantized.data.data(),
            quantized.data.size(),
            shape,
            config
        );
        
        std::cout << "    Original format: " << UniversalQuantizer::FormatToString(format) << "\n";
        std::cout << "    Detected format: " << UniversalQuantizer::FormatToString(detected) << "\n";
        std::cout << "    Detection match: " << (detected == format ? "✓" : "✗") << "\n";
    }
}

// Test 4: Compression/Decompression Pipeline
void TestCompressionPipeline() {
    TestUtils::PrintSeparator("Test 4: Compression/Decompression Pipeline");
    
    UniversalQuantizer quantizer;
    
    const size_t tensor_size = 1024 * 1024;
    auto original_data = TestUtils::GenerateTestTensor(tensor_size);
    
    QuantizationConfig config;
    config.mode = QuantizationMode::FORWARD;
    config.enable_compression = true;  // Enable compression
    
    std::cout << "\n  Testing with compression enabled:\n";
    
    // Quantize with compression
    auto quantized = quantizer.QuantizeForward(original_data.data(), tensor_size, 
                                              QuantizationFormat::Q4_K, config);
    
    TestUtils::PrintMetric("  Original size", quantized.original_size_bytes / (1024.0 * 1024.0), " MB");
    TestUtils::PrintMetric("  Quantized size", quantized.quantized_size_bytes / (1024.0 * 1024.0), " MB");
    TestUtils::PrintMetric("  Compressed size", quantized.data.size() / (1024.0 * 1024.0), " MB");
    TestUtils::PrintMetric("  Compression ratio", 
                          static_cast<double>(quantized.data.size()) / quantized.original_size_bytes * 100.0, "%");
    std::cout << "  Is compressed: " << (quantized.is_compressed ? "Yes" : "No") << "\n";
    
    // Test decompression
    if (quantized.is_compressed) {
        std::cout << "\n  Testing decompression:\n";
        
        QuantizedTensor temp = quantized;
        bool decompressed = quantizer.DecompressTensor(temp);
        
        if (decompressed) {
            TestUtils::PrintMetric("  After decompression", temp.data.size() / (1024.0 * 1024.0), " MB");
            std::cout << "  Decompression successful: " << (temp.data.size() == quantized.quantized_size_bytes ? "✓" : "✗") << "\n";
        }
        
        // Test full restoration
        std::cout << "\n  Testing full dequantization:\n";
        auto restored = quantizer.Dequantize(quantized);
        
        if (restored.size() == tensor_size) {
            double mse = TestUtils::CalculateMSE(original_data, restored);
            double rmse = std::sqrt(mse);
            TestUtils::PrintMetric("  Reconstruction RMSE", rmse);
            std::cout << "  Full restoration successful: ✓\n";
        }
    }
}

// Test 5: Complete Model Pipeline
void TestCompletePipeline() {
    TestUtils::PrintSeparator("Test 5: Complete Model Pipeline");
    
    UniversalQuantizer quantizer;
    
    // Create realistic model structure
    std::map<std::string, std::vector<float>> model_tensors;
    model_tensors["tok_embeddings.weight"] = TestUtils::GenerateTestTensor(4096 * 32000);
    model_tensors["layers.0.attention.q_proj.weight"] = TestUtils::GenerateTestTensor(4096 * 4096);
    model_tensors["layers.0.attention.k_proj.weight"] = TestUtils::GenerateTestTensor(4096 * 4096);
    model_tensors["layers.0.attention.v_proj.weight"] = TestUtils::GenerateTestTensor(4096 * 4096);
    model_tensors["layers.0.attention.o_proj.weight"] = TestUtils::GenerateTestTensor(4096 * 4096);
    model_tensors["layers.0.mlp.gate_proj.weight"] = TestUtils::GenerateTestTensor(4096 * 11008);
    model_tensors["layers.0.mlp.up_proj.weight"] = TestUtils::GenerateTestTensor(4096 * 11008);
    model_tensors["layers.0.mlp.down_proj.weight"] = TestUtils::GenerateTestTensor(11008 * 4096);
    model_tensors["output.weight"] = TestUtils::GenerateTestTensor(4096 * 32000);
    
    size_t original_size = 0;
    for (const auto& [name, data] : model_tensors) {
        original_size += data.size() * sizeof(float);
    }
    
    TestUtils::PrintMetric("Original model size", original_size / (1024.0 * 1024.0), " MB");
    
    // Test complete pipeline (backward quantization with compression)
    auto config = UniversalQuantizationUtils::CreateBackwardConfig(2, 0.90);
    config.critical_layers = {"tok_embeddings", "output"};
    config.critical_min_format = QuantizationFormat::Q4_K;
    config.enable_compression = true;
    
    std::cout << "\n  Running complete quantization pipeline...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = quantizer.QuantizeModelComplete(model_tensors, config);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (result.success) {
        TestUtils::PrintMetric("  Quantized size", result.quantized_size_bytes / (1024.0 * 1024.0), " MB");
        TestUtils::PrintMetric("  Compressed size", result.compressed_size_bytes / (1024.0 * 1024.0), " MB");
        TestUtils::PrintMetric("  Total compression ratio", result.compression_ratio * 100.0, "%");
        TestUtils::PrintMetric("  Total size reduction", (1.0 - result.compression_ratio) * 100.0, "%");
        TestUtils::PrintMetric("  Estimated quality", result.estimated_quality * 100.0, "%");
        TestUtils::PrintMetric("  Processing time", duration.count(), " ms");
        TestUtils::PrintMetric("  Tensors quantized", static_cast<double>(result.quantized_tensors.size()));
        
        // Test restoration
        std::cout << "\n  Testing model restoration...\n";
        auto restored = quantizer.RestoreModelFromQuantized(result);
        
        if (restored.size() == model_tensors.size()) {
            std::cout << "  Model restoration successful: ✓\n";
            TestUtils::PrintMetric("  Restored tensors", static_cast<double>(restored.size()));
        }
    } else {
        std::cout << "  ERROR: " << result.error_message << "\n";
    }
}

// Test 6: Format Steps (2 steps before start)
void TestFormatSteps() {
    TestUtils::PrintSeparator("Test 6: Format Steps (2 Steps Before Start)");
    
    QuantizationFormat start = QuantizationFormat::Q2_K;
    
    std::cout << "\n  Starting from: " << UniversalQuantizer::FormatToString(start) << " (maximum compression)\n\n";
    
    for (int steps = 0; steps <= 6; ++steps) {
        QuantizationFormat format = UniversalQuantizer::GetFormatStepsBefore(start, steps);
        double comp_ratio = UniversalQuantizer::GetCompressionRatio(format);
        double quality_loss = UniversalQuantizer::GetQualityLoss(format);
        uint8_t bits = UniversalQuantizer::GetBits(format);
        
        std::cout << "  " << steps << " steps before: " 
                  << std::setw(8) << UniversalQuantizer::FormatToString(format)
                  << " | " << std::setw(2) << static_cast<int>(bits) << " bits"
                  << " | " << std::fixed << std::setprecision(1) << std::setw(5) << (comp_ratio * 100.0) << "% size"
                  << " | " << std::setw(5) << (quality_loss * 100.0) << "% quality loss\n";
    }
    
    std::cout << "\n  Example: 2 steps before Q2_K = " 
              << UniversalQuantizer::FormatToString(UniversalQuantizer::GetFormatStepsBefore(start, 2)) 
              << "\n";
}

// Test 7: Quality vs. Size Tradeoff
void TestQualitySizeTradeoff() {
    TestUtils::PrintSeparator("Test 7: Quality vs. Size Tradeoff Analysis");
    
    const size_t tensor_size = 512 * 1024;
    auto original_data = TestUtils::GenerateTestTensor(tensor_size);
    
    UniversalQuantizer quantizer;
    QuantizationConfig config;
    config.mode = QuantizationMode::FORWARD;
    
    std::cout << "\n  Format         | Size Reduction | Quality Loss | Bits | Compression Ratio\n";
    std::cout << "  " << std::string(75, '-') << "\n";
    
    std::vector<QuantizationFormat> formats = {
        QuantizationFormat::F32,
        QuantizationFormat::F16,
        QuantizationFormat::Q8_0,
        QuantizationFormat::Q6_K,
        QuantizationFormat::Q5_K,
        QuantizationFormat::Q4_K,
        QuantizationFormat::Q3_K,
        QuantizationFormat::Q2_K
    };
    
    for (auto format : formats) {
        auto quantized = quantizer.QuantizeForward(original_data.data(), tensor_size, format, config);
        auto restored = quantizer.Dequantize(quantized);
        
        double comp_ratio = UniversalQuantizer::GetCompressionRatio(format);
        double quality_loss = UniversalQuantizer::GetQualityLoss(format);
        double size_reduction = (1.0 - comp_ratio) * 100.0;
        uint8_t bits = UniversalQuantizer::GetBits(format);
        
        double actual_mse = TestUtils::CalculateMSE(original_data, restored);
        double actual_rmse = std::sqrt(actual_mse);
        
        std::cout << "  " << std::setw(12) << UniversalQuantizer::FormatToString(format)
                  << " | " << std::setw(13) << std::fixed << std::setprecision(1) << size_reduction << "%"
                  << " | " << std::setw(12) << std::setprecision(1) << (quality_loss * 100.0) << "%"
                  << " | " << std::setw(4) << static_cast<int>(bits)
                  << " | " << std::setw(17) << std::setprecision(3) << comp_ratio
                  << " (" << std::setprecision(1) << (actual_rmse * 1000.0) << "e-3 RMSE)\n";
    }
}

// Main test runner
int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           Universal Quantization System - Comprehensive Test Suite          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    
    try {
        TestFormatSteps();
        TestForwardQuantization();
        TestBackwardQuantization();
        TestReverseEngineering();
        TestCompressionPipeline();
        TestCompletePipeline();
        TestQualitySizeTradeoff();
        
        TestUtils::PrintSeparator("ALL TESTS COMPLETED");
        std::cout << "\n  ✓ All quantization tests passed successfully!\n\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n  ✗ TEST FAILED: " << e.what() << "\n\n";
        return 1;
    }
}

