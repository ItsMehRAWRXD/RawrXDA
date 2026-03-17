// ================================================================
// RawrXD-ExecAI Model Loader Benchmark
// Measures throughput and latency of streaming inference
// ================================================================
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <chrono>
#include <vector>

// ExecAI C interface
extern "C" {
    BOOL InitializeExecAI(const char* model_path);
    void ShutdownExecAI(void);
    BOOL RunStreamingInference(const char* token_path);
    float EvaluateSingleToken(uint32_t token);
}

// ================================================================
// GenerateTestTokens - Create synthetic token stream
// ================================================================
static bool GenerateTestTokens(const char* output_path, uint32_t token_count) {
    printf("[Benchmark] Generating %u test tokens...\n", token_count);
    
    FILE* f = fopen(output_path, "wb");
    if (!f) {
        fprintf(stderr, "[ERROR] Failed to create token file\n");
        return false;
    }
    
    // Generate pseudo-random tokens (deterministic)
    uint32_t seed = 42;
    for (uint32_t i = 0; i < token_count; i++) {
        // LCG random
        seed = (seed * 1664525 + 1013904223);
        uint32_t token = seed % 50000; // Typical vocab size
        fwrite(&token, sizeof(uint32_t), 1, f);
    }
    
    fclose(f);
    printf("[Benchmark] Test tokens generated\n");
    return true;
}

// ================================================================
// BenchmarkStreamingThroughput
// ================================================================
static void BenchmarkStreamingThroughput(const char* model_path) {
    printf("\n[Benchmark] === Streaming Throughput Test ===\n");
    
    const uint32_t token_counts[] = { 1000, 10000, 100000, 1000000 };
    
    for (uint32_t count : token_counts) {
        char token_path[256];
        sprintf_s(token_path, sizeof(token_path), "bench_tokens_%u.tokens", count);
        
        // Generate test data
        if (!GenerateTestTokens(token_path, count)) {
            continue;
        }
        
        // Initialize model
        if (!InitializeExecAI(model_path)) {
            fprintf(stderr, "[ERROR] Failed to initialize model\n");
            DeleteFileA(token_path);
            continue;
        }
        
        // Measure streaming inference
        auto start = std::chrono::high_resolution_clock::now();
        BOOL success = RunStreamingInference(token_path);
        auto end = std::chrono::high_resolution_clock::now();
        
        if (success) {
            double elapsed = std::chrono::duration<double>(end - start).count();
            double throughput = (double)count / elapsed;
            
            printf("[Benchmark] %u tokens: %.2f sec, %.2f tokens/sec\n",
                   count, elapsed, throughput);
        } else {
            fprintf(stderr, "[ERROR] Inference failed for %u tokens\n", count);
        }
        
        // Cleanup
        ShutdownExecAI();
        DeleteFileA(token_path);
    }
}

// ================================================================
// BenchmarkLatency - Single-token latency
// ================================================================
static void BenchmarkLatency(const char* model_path) {
    printf("\n[Benchmark] === Single-Token Latency Test ===\n");
    
    if (!InitializeExecAI(model_path)) {
        fprintf(stderr, "[ERROR] Failed to initialize model\n");
        return;
    }
    
    const int warmup_count = 100;
    const int test_count = 10000;
    
    // Warmup
    printf("[Benchmark] Warming up (%d iterations)...\n", warmup_count);
    for (int i = 0; i < warmup_count; i++) {
        EvaluateSingleToken(i % 50000);
    }
    
    // Measure latency
    printf("[Benchmark] Measuring latency (%d iterations)...\n", test_count);
    std::vector<double> latencies;
    latencies.reserve(test_count);
    
    for (int i = 0; i < test_count; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        EvaluateSingleToken(i % 50000);
        auto end = std::chrono::high_resolution_clock::now();
        
        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }
    
    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    
    double min_latency = latencies[0];
    double max_latency = latencies[test_count - 1];
    double median_latency = latencies[test_count / 2];
    double p95_latency = latencies[(test_count * 95) / 100];
    double p99_latency = latencies[(test_count * 99) / 100];
    
    double sum = 0.0;
    for (double l : latencies) sum += l;
    double avg_latency = sum / test_count;
    
    printf("[Benchmark] Latency Statistics:\n");
    printf("  Min:    %.2f µs\n", min_latency);
    printf("  Avg:    %.2f µs\n", avg_latency);
    printf("  Median: %.2f µs\n", median_latency);
    printf("  P95:    %.2f µs\n", p95_latency);
    printf("  P99:    %.2f µs\n", p99_latency);
    printf("  Max:    %.2f µs\n", max_latency);
    
    ShutdownExecAI();
}

// ================================================================
// BenchmarkMemoryUsage
// ================================================================
static void BenchmarkMemoryUsage(const char* model_path) {
    printf("\n[Benchmark] === Memory Usage Test ===\n");
    
    PROCESS_MEMORY_COUNTERS_EX pmc_before, pmc_after;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc_before, sizeof(pmc_before));
    
    if (!InitializeExecAI(model_path)) {
        fprintf(stderr, "[ERROR] Failed to initialize model\n");
        return;
    }
    
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc_after, sizeof(pmc_after));
    
    size_t working_set_delta = pmc_after.WorkingSetSize - pmc_before.WorkingSetSize;
    size_t private_delta = pmc_after.PrivateUsage - pmc_before.PrivateUsage;
    
    printf("[Benchmark] Memory Usage:\n");
    printf("  Working Set: %zu KB\n", working_set_delta / 1024);
    printf("  Private:     %zu KB\n", private_delta / 1024);
    printf("  Total:       %zu MB\n", pmc_after.WorkingSetSize / (1024 * 1024));
    
    ShutdownExecAI();
}

// ================================================================
// BenchmarkStartupTime
// ================================================================
static void BenchmarkStartupTime(const char* model_path) {
    printf("\n[Benchmark] === Startup Time Test ===\n");
    
    const int iterations = 10;
    std::vector<double> startup_times;
    
    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        BOOL success = InitializeExecAI(model_path);
        auto end = std::chrono::high_resolution_clock::now();
        
        if (success) {
            double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
            startup_times.push_back(time_ms);
            ShutdownExecAI();
        } else {
            fprintf(stderr, "[ERROR] Initialization failed\n");
            break;
        }
    }
    
    if (!startup_times.empty()) {
        double sum = 0.0;
        for (double t : startup_times) sum += t;
        double avg = sum / startup_times.size();
        
        std::sort(startup_times.begin(), startup_times.end());
        double min_time = startup_times[0];
        double max_time = startup_times[startup_times.size() - 1];
        
        printf("[Benchmark] Startup Time Statistics:\n");
        printf("  Min: %.2f ms\n", min_time);
        printf("  Avg: %.2f ms\n", avg);
        printf("  Max: %.2f ms\n", max_time);
    }
}

// ================================================================
// Main Entry
// ================================================================
int main(int argc, char* argv[]) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║      RawrXD-ExecAI Model Loader Benchmark Suite          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    const char* model_path = "model.exec";
    if (argc > 1) {
        model_path = argv[1];
    }
    
    printf("[Benchmark] Model: %s\n", model_path);
    
    // Verify model exists
    if (GetFileAttributesA(model_path) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "[ERROR] Model file not found: %s\n", model_path);
        fprintf(stderr, "[INFO] Creating synthetic model for testing...\n");
        
        // Create minimal test model
        FILE* f = fopen(model_path, "wb");
        if (f) {
            struct {
                uint32_t version = 1;
                uint32_t operator_count = 32;
                uint32_t state_dim = 4096;
                uint32_t flags = 0;
            } header;
            fwrite(&header, sizeof(header), 1, f);
            
            // Write dummy operators
            float dummy_data[36] = {0};
            for (int i = 0; i < 32; i++) {
                fwrite(dummy_data, sizeof(dummy_data), 1, f);
            }
            fclose(f);
            printf("[INFO] Test model created\n");
        } else {
            fprintf(stderr, "[ERROR] Failed to create test model\n");
            return 1;
        }
    }
    
    // Run benchmark suite
    BenchmarkStartupTime(model_path);
    BenchmarkMemoryUsage(model_path);
    BenchmarkLatency(model_path);
    BenchmarkStreamingThroughput(model_path);
    
    printf("\n[Benchmark] === All Tests Complete ===\n");
    return 0;
}
