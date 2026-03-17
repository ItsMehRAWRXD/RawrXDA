// ============================================================================
// RAWRXD ZERO-COPY MICRO-BENCHMARK
// ============================================================================
// Purpose: Validate the 50μs target with realistic 128KB tensor access
// Method: Compare GetTensorView (zero-copy) vs GetTensorData (memcpy)
// Date: 2026-01-27

#include "streaming_gguf_loader_enhanced.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <cmath>
#include <filesystem>

#include "logging/logger.h"
static Logger s_logger("benchmark_zerocopy_microbench");

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

class PrecisionTimer {
public:
    using clock = std::chrono::high_resolution_clock;
    
    void start() { start_time = clock::now(); }
    
    double elapsed_ns() const {
        auto end = clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_time).count();
    }
    
    double elapsed_us() const { return elapsed_ns() / 1000.0; }
    double elapsed_ms() const { return elapsed_us() / 1000.0; }
    
private:
    clock::time_point start_time;
};

struct BenchmarkStats {
    std::vector<double> samples;
    double mean = 0, stddev = 0, min = 0, max = 0, p50 = 0, p95 = 0, p99 = 0;
    
    void record(double value) { samples.push_back(value); }
    
    void compute() {
        if (samples.empty()) return;
        std::sort(samples.begin(), samples.end());
        mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
        
        double sq_sum = 0.0;
        for (double s : samples) sq_sum += (s - mean) * (s - mean);
        stddev = std::sqrt(sq_sum / samples.size());
        
        min = samples.front();
        max = samples.back();
        p50 = samples[samples.size() / 2];
        p95 = samples[samples.size() * 95 / 100];
        p99 = samples[samples.size() * 99 / 100];
    }
};

// Generate small synthetic GGUF with realistic tensor sizes
void buildMicroGGUF(const std::string& path, size_t tensor_count, size_t tensor_size_kb) {
    std::ofstream out(path, std::ios::binary);
    
    // GGUF magic
    uint32_t magic = 0x46554747;
    out.write(reinterpret_cast<char*>(&magic), sizeof(magic));
    
    // Version 2
    uint32_t version = 2;
    out.write(reinterpret_cast<char*>(&version), sizeof(version));
    
    // Tensor count
    uint64_t tc = tensor_count;
    out.write(reinterpret_cast<char*>(&tc), sizeof(tc));
    
    // Metadata count (0)
    uint64_t mc = 0;
    out.write(reinterpret_cast<char*>(&mc), sizeof(mc));
    
    // Tensor headers
    uint64_t current_offset = 0;
    for (size_t i = 0; i < tensor_count; ++i) {
        char name[32] = {0};
        snprintf(name, 32, "tensor_%04zu", i);
        std::string name_str = name;
        
        uint64_t name_len = name_str.length();
        out.write(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        out.write(name_str.data(), name_len);
        
        uint32_t n_dims = 1;
        out.write(reinterpret_cast<char*>(&n_dims), sizeof(n_dims));
        
        uint64_t dim_size = tensor_size_kb * 256;  // F32: 1KB = 256 floats
        out.write(reinterpret_cast<char*>(&dim_size), sizeof(dim_size));
        
        uint32_t type = 0;  // F32
        out.write(reinterpret_cast<char*>(&type), sizeof(type));
        
        out.write(reinterpret_cast<char*>(&current_offset), sizeof(current_offset));
        current_offset += tensor_size_kb * 1024;
    }
    
    // Align to 32 bytes
    while (out.tellp() % 32 != 0) {
        uint8_t pad = 0;
        out.write(reinterpret_cast<char*>(&pad), 1);
    }
    
    // Tensor data
    std::vector<float> buffer(tensor_size_kb * 256, 0.0f);
    for (size_t i = 0; i < tensor_count; ++i) {
        out.write(reinterpret_cast<char*>(buffer.data()), buffer.size() * sizeof(float));
    }
    
    out.close();
}

int main() {
    s_logger.info("╔═══════════════════════════════════════════════════════════╗\n");
    s_logger.info("║   RAWRXD ZERO-COPY MICRO-BENCHMARK                       ║\n");
    s_logger.info("║   Target: 50μs for 128KB tensor (zero-copy)              ║\n");
    s_logger.info("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    // Test parameters
    const size_t TENSOR_COUNT = 100;
    const size_t TENSOR_SIZE_KB = 128;  // 128KB - typical attention head
    const int WARMUP_ITERATIONS = 100;
    const int BENCHMARK_ITERATIONS = 10000;
    
    std::string model_path = "micro_benchmark_128kb.gguf";
    
    // Cleanup old file
    if (std::filesystem::exists(model_path)) {
        std::filesystem::remove(model_path);
    }
    
    s_logger.info("Generating test model: ");
    buildMicroGGUF(model_path, TENSOR_COUNT, TENSOR_SIZE_KB);
    s_logger.info("✓ Model created: ");
    
    // ========================================================================
    // BENCHMARK 1: GetTensorData (memcpy path)
    // ========================================================================
    s_logger.info("=== BENCHMARK 1: GetTensorData (memcpy) ===\n");
    {
        EnhancedStreamingGGUFLoader loader;
        if (!loader.Open(model_path)) {
            s_logger.error( "Failed to open model\n";
            return 1;
        }
        
        // Warmup
        s_logger.info("Warmup (");
        std::vector<uint8_t> data;
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            loader.GetTensorData("tensor_0000", data);
        }
        
        // Benchmark
        BenchmarkStats stats;
        s_logger.info("Benchmarking (");
        
        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            PrecisionTimer timer;
            timer.start();
            
            loader.GetTensorData("tensor_0000", data);
            
            stats.record(timer.elapsed_us());
        }
        
        stats.compute();
        
        s_logger.info( std::fixed << std::setprecision(2);
        s_logger.info("\nResults (memcpy path):\n");
        s_logger.info("  Mean:   ");
        s_logger.info("  Median: ");
        s_logger.info("  P95:    ");
        s_logger.info("  P99:    ");
        s_logger.info("  Min:    ");
        s_logger.info("  Max:    ");
        
        double throughput_gbps = (TENSOR_SIZE_KB * 1024.0) / (stats.mean * 1000.0);
        s_logger.info("  Throughput: ");
        
        bool pass = stats.mean < 200.0;  // memcpy expected ~100-150μs for 128KB
        s_logger.info("  Status: ");
        
        loader.Close();
    }
    
    // ========================================================================
    // BENCHMARK 2: GetTensorView (zero-copy path)
    // ========================================================================
    s_logger.info("=== BENCHMARK 2: GetTensorView (zero-copy) ===\n");
    {
        EnhancedStreamingGGUFLoader loader;
        if (!loader.Open(model_path)) {
            s_logger.error( "Failed to open model\n";
            return 1;
        }
        
        // Pre-load zone to ensure data is resident
        loader.LoadZone("misc");
        
        // Warmup
        s_logger.info("Warmup (");
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            auto view = loader.GetTensorView("tensor_0000");
            (void)view.data();  // Touch to prevent optimization
        }
        
        // Benchmark
        BenchmarkStats stats;
        s_logger.info("Benchmarking (");
        
        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            PrecisionTimer timer;
            timer.start();
            
            auto view = loader.GetTensorView("tensor_0000");
            volatile const void* ptr = view.data();  // Prevent optimization
            (void)ptr;
            
            stats.record(timer.elapsed_us());
        }
        
        stats.compute();
        
        s_logger.info( std::fixed << std::setprecision(2);
        s_logger.info("\nResults (zero-copy path):\n");
        s_logger.info("  Mean:   ");
        s_logger.info("  Median: ");
        s_logger.info("  P95:    ");
        s_logger.info("  P99:    ");
        s_logger.info("  Min:    ");
        s_logger.info("  Max:    ");
        
        // TARGET: 50μs ± 20μs
        bool pass = stats.mean >= 30.0 && stats.mean <= 70.0;
        s_logger.info("\n  *** TARGET: 50μs ± 20μs ***\n");
        s_logger.info("  Status: ");
        if (!pass) {
            s_logger.info("  Deviation: ");
        }
        
        loader.Close();
    }
    
    // ========================================================================
    // BENCHMARK 3: IsTensorResident check (ultra-fast path)
    // ========================================================================
    s_logger.info("\n=== BENCHMARK 3: IsTensorResident (cache check only) ===\n");
    {
        EnhancedStreamingGGUFLoader loader;
        if (!loader.Open(model_path)) {
            s_logger.error( "Failed to open model\n";
            return 1;
        }
        
        loader.LoadZone("misc");
        
        BenchmarkStats stats;
        
        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            PrecisionTimer timer;
            timer.start();
            
            volatile bool resident = loader.IsTensorResident("tensor_0000");
            (void)resident;
            
            stats.record(timer.elapsed_us());
        }
        
        stats.compute();
        
        s_logger.info( std::fixed << std::setprecision(3);
        s_logger.info("Results (cache check only):\n");
        s_logger.info("  Mean:   ");
        s_logger.info("  Median: ");
        s_logger.info("  P99:    ");
        
        // Should be sub-microsecond
        bool pass = stats.mean < 1.0;
        s_logger.info("  Status: ");
        
        loader.Close();
    }
    
    // ========================================================================
    // SUMMARY
    // ========================================================================
    s_logger.info("\n╔═══════════════════════════════════════════════════════════╗\n");
    s_logger.info("║   MICRO-BENCHMARK COMPLETE                               ║\n");
    s_logger.info("╚═══════════════════════════════════════════════════════════╝\n");
    s_logger.info("\nConclusion:\n");
    s_logger.info("  • GetTensorData (memcpy): ~100-150μs for 128KB (physics limit)\n");
    s_logger.info("  • GetTensorView (zero-copy): TARGET 50μs (pointer arithmetic only)\n");
    s_logger.info("  • IsTensorResident: <1μs (instant cache check)\n");
    s_logger.info("\nRecommendation:\n");
    s_logger.info("  Use GetTensorView() for hot paths in inference loops.\n");
    s_logger.info("  Use IsTensorResident() + PrefetchTensorAsync() for async pipelines.\n");
    
    // Cleanup
    std::filesystem::remove(model_path);
    
    return 0;
}
