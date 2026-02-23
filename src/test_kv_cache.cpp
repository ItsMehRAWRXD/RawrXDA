// KV Cache Infrastructure Test
// Tests the GPU KV cache allocation, append, and retrieval operations

#include "vulkan_compute.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

// Simple assertion helper
#define ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "ASSERTION FAILED: " << message << std::endl; \
        return false; \
    }

// Test 1: Basic KV cache allocation
bool testKVCacheAllocation() {
    std::cout << "\n=== Test 1: KV Cache Allocation ===" << std::endl;
    
    VulkanCompute gpu;
    if (!gpu.Initialize()) {
        std::cerr << "Failed to initialize Vulkan" << std::endl;
        return false;
    }
    
    // Allocate KV cache: 4 layers, 128 max tokens, 64 head_dim
    uint32_t num_layers = 4;
    uint32_t max_seq_len = 128;
    uint32_t head_dim = 64;
    
    bool success = gpu.AllocateKVCache(num_layers, max_seq_len, head_dim);
    ASSERT(success, "AllocateKVCache should succeed");
    ASSERT(gpu.IsKVCacheAllocated(), "KV cache should be allocated");
    
    std::cout << "✓ KV cache allocation successful" << std::endl;
    
    // Clean up
    gpu.ClearKVCache();
    ASSERT(!gpu.IsKVCacheAllocated(), "KV cache should be cleared");
    
    std::cout << "✓ KV cache cleanup successful" << std::endl;
    
    return true;
}

// Test 2: Append to KV cache
bool testKVCacheAppend() {
    std::cout << "\n=== Test 2: KV Cache Append ===" << std::endl;
    
    VulkanCompute gpu;
    if (!gpu.Initialize()) {
        return false;
    }
    
    uint32_t num_layers = 2;
    uint32_t max_seq_len = 64;
    uint32_t head_dim = 32;
    
    if (!gpu.AllocateKVCache(num_layers, max_seq_len, head_dim)) {
        return false;
    }
    
    // Create test K/V vectors (filled with distinctive values)
    std::vector<float> k_new(head_dim);
    std::vector<float> v_new(head_dim);
    
    for (uint32_t i = 0; i < head_dim; ++i) {
        k_new[i] = static_cast<float>(i) + 100.0f;  // 100, 101, 102, ...
        v_new[i] = static_cast<float>(i) + 200.0f;  // 200, 201, 202, ...
    }
    
    // Append to layer 0, position 0
    bool success = gpu.AppendToKVCache(0, k_new.data(), v_new.data(), 0);
    ASSERT(success, "AppendToKVCache should succeed");
    
    std::cout << "✓ Appended K/V to cache at layer 0, pos 0" << std::endl;
    
    // Append to layer 0, position 1 (different values)
    for (uint32_t i = 0; i < head_dim; ++i) {
        k_new[i] = static_cast<float>(i) + 300.0f;
        v_new[i] = static_cast<float>(i) + 400.0f;
    }
    
    success = gpu.AppendToKVCache(0, k_new.data(), v_new.data(), 1);
    ASSERT(success, "Second append should succeed");
    
    std::cout << "✓ Appended K/V to cache at layer 0, pos 1" << std::endl;
    
    return true;
}

// Test 3: Retrieve from KV cache
bool testKVCacheRetrieval() {
    std::cout << "\n=== Test 3: KV Cache Retrieval ===" << std::endl;
    
    VulkanCompute gpu;
    if (!gpu.Initialize()) {
        return false;
    }
    
    uint32_t num_layers = 1;
    uint32_t max_seq_len = 16;
    uint32_t head_dim = 8;
    
    if (!gpu.AllocateKVCache(num_layers, max_seq_len, head_dim)) {
        return false;
    }
    
    // Write known values to cache
    std::vector<float> k_write(head_dim);
    std::vector<float> v_write(head_dim);
    
    for (uint32_t i = 0; i < head_dim; ++i) {
        k_write[i] = static_cast<float>(i) * 10.0f;  // 0, 10, 20, 30, ...
        v_write[i] = static_cast<float>(i) * 20.0f;  // 0, 20, 40, 60, ...
    }
    
    // Write to position 5
    if (!gpu.AppendToKVCache(0, k_write.data(), v_write.data(), 5)) {
        return false;
    }
    
    std::cout << "✓ Written test data to cache at pos 5" << std::endl;
    
    // Flush GPU commands to ensure write completes
    gpu.FlushAsyncCommands();
    
    // Read back slice [5, 6)
    std::vector<float> k_read(head_dim);
    std::vector<float> v_read(head_dim);
    
    bool success = gpu.GetKVCacheSlice(0, 5, 6, k_read.data(), v_read.data());
    ASSERT(success, "GetKVCacheSlice should succeed");
    
    // Verify values
    const float epsilon = 1e-4f;
    for (uint32_t i = 0; i < head_dim; ++i) {
        float k_expected = static_cast<float>(i) * 10.0f;
        float v_expected = static_cast<float>(i) * 20.0f;
        
        ASSERT(std::abs(k_read[i] - k_expected) < epsilon, 
               "K value mismatch at index " + std::to_string(i));
        ASSERT(std::abs(v_read[i] - v_expected) < epsilon,
               "V value mismatch at index " + std::to_string(i));
    }
    
    std::cout << "✓ Retrieved and verified K/V cache values" << std::endl;
    std::cout << "  K[0] = " << k_read[0] << " (expected 0)" << std::endl;
    std::cout << "  K[7] = " << k_read[7] << " (expected 70)" << std::endl;
    std::cout << "  V[0] = " << v_read[0] << " (expected 0)" << std::endl;
    std::cout << "  V[7] = " << v_read[7] << " (expected 140)" << std::endl;
    
    return true;
}

// Test 4: Multi-layer KV cache
bool testMultiLayerKVCache() {
    std::cout << "\n=== Test 4: Multi-Layer KV Cache ===" << std::endl;
    
    VulkanCompute gpu;
    if (!gpu.Initialize()) {
        return false;
    }
    
    uint32_t num_layers = 8;
    uint32_t max_seq_len = 32;
    uint32_t head_dim = 16;
    
    if (!gpu.AllocateKVCache(num_layers, max_seq_len, head_dim)) {
        return false;
    }
    
    // Write different values to each layer
    for (uint32_t layer = 0; layer < num_layers; ++layer) {
        std::vector<float> k_layer(head_dim);
        std::vector<float> v_layer(head_dim);
        
        // Each layer has unique signature
        float layer_signature = static_cast<float>(layer) * 1000.0f;
        for (uint32_t i = 0; i < head_dim; ++i) {
            k_layer[i] = layer_signature + static_cast<float>(i);
            v_layer[i] = layer_signature + static_cast<float>(i) + 0.5f;
        }
        
        // Write to position 0 of each layer
        bool success = gpu.AppendToKVCache(layer, k_layer.data(), v_layer.data(), 0);
        ASSERT(success, "Append to layer " + std::to_string(layer) + " should succeed");
    }
    
    std::cout << "✓ Written to all " << num_layers << " layers" << std::endl;
    
    gpu.FlushAsyncCommands();
    
    // Verify each layer has correct values
    for (uint32_t layer = 0; layer < num_layers; ++layer) {
        std::vector<float> k_verify(head_dim);
        std::vector<float> v_verify(head_dim);
        
        bool success = gpu.GetKVCacheSlice(layer, 0, 1, k_verify.data(), v_verify.data());
        ASSERT(success, "Read from layer " + std::to_string(layer) + " should succeed");
        
        float expected_k = static_cast<float>(layer) * 1000.0f;
        const float epsilon = 1e-4f;
        
        ASSERT(std::abs(k_verify[0] - expected_k) < epsilon,
               "Layer " + std::to_string(layer) + " K[0] mismatch");
        
        std::cout << "  Layer " << layer << " K[0] = " << k_verify[0] 
                  << " (expected " << expected_k << ") ✓" << std::endl;
    }
    
    std::cout << "✓ All layers verified successfully" << std::endl;
    
    return true;
}

// Test 5: Realistic token sequence
bool testRealisticTokenSequence() {
    std::cout << "\n=== Test 5: Realistic Token Sequence ===" << std::endl;
    
    VulkanCompute gpu;
    if (!gpu.Initialize()) {
        return false;
    }
    
    // Realistic dimensions for small model
    uint32_t num_layers = 4;
    uint32_t max_seq_len = 256;
    uint32_t head_dim = 64;
    
    if (!gpu.AllocateKVCache(num_layers, max_seq_len, head_dim)) {
        return false;
    }
    
    std::cout << "✓ Allocated cache for " << num_layers << " layers, "
              << max_seq_len << " max tokens, " << head_dim << " head_dim" << std::endl;
    
    // Simulate autoregressive token generation (10 tokens)
    uint32_t num_tokens = 10;
    
    for (uint32_t token_pos = 0; token_pos < num_tokens; ++token_pos) {
        // For each layer, append K/V for this token
        for (uint32_t layer = 0; layer < num_layers; ++layer) {
            std::vector<float> k_token(head_dim);
            std::vector<float> v_token(head_dim);
            
            // Simulate K/V projection output (random-ish values)
            for (uint32_t i = 0; i < head_dim; ++i) {
                k_token[i] = static_cast<float>(token_pos * 100 + layer * 10 + i);
                v_token[i] = static_cast<float>(token_pos * 100 + layer * 10 + i) + 0.1f;
            }
            
            bool success = gpu.AppendToKVCache(layer, k_token.data(), v_token.data(), token_pos);
            ASSERT(success, "Append token " + std::to_string(token_pos) + 
                   " layer " + std::to_string(layer) + " should succeed");
        }
        
        if (token_pos % 5 == 0) {
            gpu.FlushAsyncCommands();  // Periodic flush
        }
    }
    
    std::cout << "✓ Appended " << num_tokens << " tokens across " << num_layers << " layers" << std::endl;
    
    gpu.FlushAsyncCommands();
    
    // Verify we can retrieve full sequence for layer 0
    std::vector<float> k_sequence(num_tokens * head_dim);
    std::vector<float> v_sequence(num_tokens * head_dim);
    
    bool success = gpu.GetKVCacheSlice(0, 0, num_tokens, k_sequence.data(), v_sequence.data());
    ASSERT(success, "Retrieve full sequence should succeed");
    
    // Spot check a few values
    float expected_k_token0 = 0.0f;  // token_pos=0, layer=0, i=0
    float expected_k_token5 = 500.0f;  // token_pos=5, layer=0, i=0
    
    const float epsilon = 1e-4f;
    ASSERT(std::abs(k_sequence[0] - expected_k_token0) < epsilon,
           "Token 0 K[0] mismatch");
    ASSERT(std::abs(k_sequence[5 * head_dim] - expected_k_token5) < epsilon,
           "Token 5 K[0] mismatch");
    
    std::cout << "✓ Verified full sequence retrieval" << std::endl;
    std::cout << "  Token 0 K[0] = " << k_sequence[0] << std::endl;
    std::cout << "  Token 5 K[0] = " << k_sequence[5 * head_dim] << std::endl;
    
    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "KV Cache Infrastructure Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    bool all_passed = true;
    
    all_passed &= testKVCacheAllocation();
    all_passed &= testKVCacheAppend();
    all_passed &= testKVCacheRetrieval();
    all_passed &= testMultiLayerKVCache();
    all_passed &= testRealisticTokenSequence();
    
    std::cout << "\n========================================" << std::endl;
    if (all_passed) {
        std::cout << "✓✓✓ ALL TESTS PASSED ✓✓✓" << std::endl;
        std::cout << "KV Cache infrastructure is working correctly!" << std::endl;
    } else {
        std::cout << "✗✗✗ SOME TESTS FAILED ✗✗✗" << std::endl;
        std::cout << "Check error messages above." << std::endl;
    }
    std::cout << "========================================" << std::endl;
    
    return all_passed ? 0 : 1;
}
