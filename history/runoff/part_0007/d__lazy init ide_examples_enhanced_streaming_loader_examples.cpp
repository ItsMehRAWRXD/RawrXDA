#include "streaming_gguf_loader_enhanced.h"
#include <iostream>
#include <vector>
#include <chrono>

// ============================================================================
// EXAMPLE 1: Basic 800B Model Loading
// ============================================================================

void Example_BasicUsage()
{
    std::cout << "\n=== Example 1: Basic 800B Model Loading ===\n";
    
    // Create enhanced loader
    EnhancedStreamingGGUFLoader loader;
    
    // Configure for 800B model
    loader.AllocateHugePages(1024);      // 1GB huge page pool
    loader.EnableNVMeDirectIO();         // Enable NVMe if available
    loader.EnableIOring();               // Enable IORING (Windows 11+)
    loader.DetectComputeDevices();       // Auto-detect GPUs
    
    // Open model
    if (loader.Open("model-800b-IQ2_XS.gguf")) {
        std::cout << "✓ Model loaded successfully\n";
        
        // Load a tensor (transparent optimization)
        std::vector<uint8_t> tensor_data;
        if (loader.GetTensorData("blk.0.attn_q.weight", tensor_data)) {
            std::cout << "✓ Tensor loaded: " << tensor_data.size() << " bytes\n";
        }
        
        loader.Close();
    }
}

// ============================================================================
// EXAMPLE 2: Predictive Caching & Prefetching
// ============================================================================

void Example_PredictiveCaching()
{
    std::cout << "\n=== Example 2: Predictive Caching & Prefetching ===\n";
    
    EnhancedStreamingGGUFLoader loader;
    loader.AllocateHugePages(512);
    loader.Open("model-800b-IQ2_XS.gguf");
    
    // Simulate layer-by-layer inference
    for (int layer = 0; layer < 80; ++layer) {
        std::cout << "\nLayer " << layer << ":\n";
        
        // Update access pattern (learns sequential behavior)
        loader.UpdateAccessPattern(layer);
        
        // Get predictions for next zones
        auto predictions = loader.PredictNextZones(8);
        std::cout << "  Predicted zones: ";
        for (auto zone : predictions) {
            std::cout << zone << " ";
        }
        std::cout << "\n";
        
        // Queue async prefetch
        for (auto zone_id : predictions) {
            loader.PrefetchZoneAsync(zone_id);
        }
        
        // Load current layer tensors
        std::string tensor_names[] = {
            "blk." + std::to_string(layer) + ".attn_q.weight",
            "blk." + std::to_string(layer) + ".ffn.weight"
        };
        
        for (const auto& name : tensor_names) {
            std::vector<uint8_t> data;
            
            auto start = std::chrono::high_resolution_clock::now();
            loader.GetTensorData(name, data);
            auto elapsed = std::chrono::high_resolution_clock::now() - start;
            
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
            std::cout << "  " << name << ": " << us << "μs\n";
        }
    }
    
    // Show cache statistics
    auto metrics = loader.GetMetrics();
    std::cout << "\nCache Statistics:\n";
    std::cout << "  Total loads: " << metrics.total_tensor_loads << "\n";
    std::cout << "  Cache hits: " << metrics.cache_hits << "\n";
    std::cout << "  Cache misses: " << metrics.cache_misses << "\n";
    
    double hit_rate = (metrics.cache_hits * 100.0) / 
                      std::max(1UL, metrics.total_tensor_loads);
    std::cout << "  Hit rate: " << hit_rate << "%\n";
    std::cout << "  Avg load time: " << metrics.avg_load_time_us << "μs\n";
    
    loader.Close();
}

// ============================================================================
// EXAMPLE 3: Parallel Tensor Loading (Multi-GPU)
// ============================================================================

void Example_ParallelLoading()
{
    std::cout << "\n=== Example 3: Parallel Tensor Loading ===\n";
    
    EnhancedStreamingGGUFLoader loader;
    loader.AllocateHugePages(2048);
    
    int device_count = loader.DetectComputeDevices();
    std::cout << "Available compute devices: " << device_count << "\n";
    
    loader.Open("model-800b-IQ2_XS.gguf");
    
    // Load large tensors in parallel
    std::string large_tensors[] = {
        "token_embd.weight",
        "blk.0.attn_q.weight",
        "blk.0.attn_k.weight",
        "blk.0.attn_v.weight"
    };
    
    for (const auto& tensor_name : large_tensors) {
        std::cout << "\nLoading " << tensor_name << " on " << device_count << " devices:\n";
        
        std::vector<uint8_t> data;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Load tensor in parallel (sharded across devices)
        loader.LoadTensorParallel(tensor_name, data, 0);  // GPU 0 preferred
        
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        
        std::cout << "  Time: " << ms << "ms\n";
        std::cout << "  Size: " << (data.size() / 1024.0 / 1024.0) << " MB\n";
    }
    
    loader.Close();
}

// ============================================================================
// EXAMPLE 4: Compression & I/O Optimization
// ============================================================================

void Example_CompressionOptimization()
{
    std::cout << "\n=== Example 4: Compression & I/O Optimization ===\n";
    
    EnhancedStreamingGGUFLoader loader;
    
    // Different compression preferences for different hardware
    std::cout << "\nCompression preference options:\n";
    std::cout << "  0: None (raw)\n";
    std::cout << "  1: Fast (LZ4)\n";
    std::cout << "  2: Balanced (ZSTD level 3)\n";
    std::cout << "  3: Maximum (ZSTD level 22)\n";
    
    // Example: Balanced for typical hardware
    loader.SetCompressionPreference(2);
    std::cout << "\nSet to: Balanced (ZSTD)\n";
    
    // NVMe status
    if (loader.EnableNVMeDirectIO()) {
        std::cout << "✓ NVMe direct I/O: Enabled (kernel-bypass, ~10μs latency)\n";
    } else {
        std::cout << "✗ NVMe direct I/O: Not available\n";
    }
    
    // IORING status
    if (loader.EnableIOring()) {
        std::cout << "✓ IORING batch I/O: Enabled (64 ops/syscall)\n";
    } else {
        std::cout << "✗ IORING batch I/O: Not available (Windows 11 22H2+ required)\n";
    }
    
    loader.Open("model-800b-IQ2_XS.gguf");
    
    // Benchmark load times
    std::vector<std::chrono::microseconds> load_times;
    
    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> data;
        
        auto start = std::chrono::high_resolution_clock::now();
        loader.GetTensorData("blk.0.attn_q.weight", data);
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        
        load_times.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed)
        );
    }
    
    // Calculate statistics
    auto min_time = *std::min_element(load_times.begin(), load_times.end());
    auto max_time = *std::max_element(load_times.begin(), load_times.end());
    
    long long sum = 0;
    for (const auto& t : load_times) sum += t.count();
    double avg_time = static_cast<double>(sum) / load_times.size();
    
    std::cout << "\nLoad time statistics (10 samples):\n";
    std::cout << "  Min: " << min_time.count() << "μs\n";
    std::cout << "  Max: " << max_time.count() << "μs\n";
    std::cout << "  Avg: " << avg_time << "μs\n";
    
    loader.Close();
}

// ============================================================================
// EXAMPLE 5: Performance Benchmarking
// ============================================================================

void Example_PerformanceBenchmark()
{
    std::cout << "\n=== Example 5: Performance Benchmarking ===\n";
    
    EnhancedStreamingGGUFLoader loader;
    loader.AllocateHugePages(1024);
    loader.EnableNVMeDirectIO();
    loader.EnableIOring();
    
    loader.Open("model-800b-IQ2_XS.gguf");
    
    // Benchmark 1: Cold start (first layer)
    {
        loader.ResetMetrics();
        std::cout << "\nBenchmark 1: Cold Start\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<uint8_t> data;
        loader.GetTensorData("blk.0.attn_q.weight", data);
        
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        
        std::cout << "  Time: " << ms << "ms (expected: ~5ms)\n";
    }
    
    // Benchmark 2: Hot access (predicted zone)
    {
        loader.ResetMetrics();
        std::cout << "\nBenchmark 2: Hot Access (Predicted)\n";
        
        // Prime the predictor
        for (int i = 0; i < 5; ++i) {
            loader.UpdateAccessPattern(i);
        }
        
        auto predictions = loader.PredictNextZones(8);
        for (auto zone : predictions) {
            loader.PrefetchZoneAsync(zone);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<uint8_t> data;
        loader.GetTensorData("blk.5.attn_q.weight", data);
        
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        
        std::cout << "  Time: " << us << "μs (expected: <50μs)\n";
    }
    
    // Benchmark 3: Sequential inference loop
    {
        loader.ResetMetrics();
        std::cout << "\nBenchmark 3: Sequential Inference (80 layers)\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int layer = 0; layer < 80; ++layer) {
            loader.UpdateAccessPattern(layer);
            auto predictions = loader.PredictNextZones(8);
            
            for (auto zone : predictions) {
                loader.PrefetchZoneAsync(zone);
            }
            
            std::vector<uint8_t> data;
            loader.GetTensorData("blk." + std::to_string(layer) + ".attn_q.weight", data);
        }
        
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        
        auto metrics = loader.GetMetrics();
        double hit_rate = (metrics.cache_hits * 100.0) / 
                         std::max(1UL, metrics.total_tensor_loads);
        
        std::cout << "  Total time: " << total_ms << "ms\n";
        std::cout << "  Cache hit rate: " << hit_rate << "%\n";
        std::cout << "  Avg time/layer: " << (total_ms / 80.0) << "ms\n";
    }
    
    loader.Close();
}

// ============================================================================
// EXAMPLE 6: Error Handling & Fallbacks
// ============================================================================

void Example_ErrorHandling()
{
    std::cout << "\n=== Example 6: Error Handling & Fallbacks ===\n";
    
    EnhancedStreamingGGUFLoader loader;
    
    // Attempt to enable advanced features (graceful degradation)
    std::cout << "Attempting to enable advanced features...\n";
    
    if (!loader.EnableNVMeDirectIO()) {
        std::cout << "  NVMe direct I/O not available, skipping\n";
    } else {
        std::cout << "  ✓ NVMe direct I/O enabled\n";
    }
    
    if (!loader.EnableIOring()) {
        std::cout << "  IORING not available, skipping (requires Windows 11 22H2+)\n";
    } else {
        std::cout << "  ✓ IORING enabled\n";
    }
    
    if (!loader.AllocateHugePages(1024)) {
        std::cout << "  Huge pages not available, using standard pages\n";
    } else {
        std::cout << "  ✓ Huge pages allocated (1GB)\n";
    }
    
    int device_count = loader.DetectComputeDevices();
    std::cout << "  Detected " << device_count << " compute device(s)\n";
    
    std::cout << "\nLoader is ready with available capabilities:\n";
    std::cout << "  - Predictive caching: Always enabled\n";
    std::cout << "  - NVMe I/O: " << (loader.IsNVMeEnabled() ? "Yes" : "No") << "\n";
    std::cout << "  - IORING: " << (loader.IsIOringEnabled() ? "Yes" : "No") << "\n";
    std::cout << "  - Huge pages: " << (loader.GetHugePageUsage() > 0 ? "Yes" : "No") << "\n";
    std::cout << "  - Tensor parallelism: " << device_count << " devices\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main()
{
    try {
        Example_BasicUsage();
        Example_PredictiveCaching();
        Example_ParallelLoading();
        Example_CompressionOptimization();
        Example_PerformanceBenchmark();
        Example_ErrorHandling();
        
        std::cout << "\n=== All examples completed successfully ===\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    
    return 0;
}
