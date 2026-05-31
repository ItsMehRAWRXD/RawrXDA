// vulkan_benchmark.cpp - GPU vs CPU inference benchmark harness
// Uses existing VulkanCompute backend to measure throughput
// Build: cl /EHsc /O2 vulkan_benchmark.cpp /I..\src /link ..\build\RawrXD.lib
// Run:   vulkan_benchmark.exe [matmul|attention|all] [size]

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <chrono>
#include <vector>
#include <algorithm>

#include "vulkan_compute.h"

// =============================================================================
// Benchmark Configuration
// =============================================================================
struct BenchConfig {
    uint32_t M = 512;      // MatMul rows
    uint32_t K = 512;      // MatMul inner dim
    uint32_t N = 512;      // MatMul cols
    uint32_t seq_len = 256; // Attention sequence length
    uint32_t head_dim = 64;  // Attention head dimension
    uint32_t num_heads = 8;  // Number of attention heads
    uint32_t warmup_iters = 3;
    uint32_t bench_iters = 10;
};

// =============================================================================
// Timing Utilities
// =============================================================================
static double GetTimeMs() {
    LARGE_INTEGER freq, ctr;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&ctr);
    return (double)ctr.QuadPart * 1000.0 / (double)freq.QuadPart;
}

static double GetTimeUs() {
    return GetTimeMs() * 1000.0;
}

// =============================================================================
// CPU Reference Implementations
// =============================================================================
static void CpuMatMul(const float* A, const float* B, float* C, uint32_t M, uint32_t K, uint32_t N) {
    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t j = 0; j < N; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < K; k++) {
                sum += A[i * K + k] * B[k * N + j];
            }
            C[i * N + j] = sum;
        }
    }
}

static void CpuSoftmax(float* data, uint32_t size) {
    float max_val = data[0];
    for (uint32_t i = 1; i < size; i++) {
        if (data[i] > max_val) max_val = data[i];
    }
    float sum = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        data[i] = expf(data[i] - max_val);
        sum += data[i];
    }
    for (uint32_t i = 0; i < size; i++) {
        data[i] /= sum;
    }
}

static void CpuAttention(const float* Q, const float* K, const float* V, float* out,
                          uint32_t seq_len, uint32_t head_dim, uint32_t num_heads) {
    float scale = 1.0f / sqrtf((float)head_dim);
    
    for (uint32_t h = 0; h < num_heads; h++) {
        const float* qh = Q + h * head_dim;
        float* outh = out + h * head_dim;
        
        // Compute attention scores
        std::vector<float> scores(seq_len);
        for (uint32_t t = 0; t < seq_len; t++) {
            const float* kt = K + t * num_heads * head_dim + h * head_dim;
            float dot = 0.0f;
            for (uint32_t d = 0; d < head_dim; d++) {
                dot += qh[d] * kt[d];
            }
            scores[t] = dot * scale;
        }
        
        // Softmax
        CpuSoftmax(scores.data(), seq_len);
        
        // Weighted sum of V
        for (uint32_t d = 0; d < head_dim; d++) {
            outh[d] = 0.0f;
        }
        for (uint32_t t = 0; t < seq_len; t++) {
            const float* vt = V + t * num_heads * head_dim + h * head_dim;
            for (uint32_t d = 0; d < head_dim; d++) {
                outh[d] += scores[t] * vt[d];
            }
        }
    }
}

// =============================================================================
// Random Data Generation
// =============================================================================
static void FillRandom(float* data, uint32_t count, float min_val = -1.0f, float max_val = 1.0f) {
    for (uint32_t i = 0; i < count; i++) {
        float t = (float)rand() / (float)RAND_MAX;
        data[i] = min_val + t * (max_val - min_val);
    }
}

// =============================================================================
// Benchmark Results
// =============================================================================
struct BenchResult {
    double cpu_ms;
    double gpu_ms;
    double gflops;
    double tokens_per_sec;
    bool gpu_success;
    char error_msg[256];
};

// =============================================================================
// MatMul Benchmark
// =============================================================================
static BenchResult BenchmarkMatMul(RawrXD::VulkanCompute& vk, const BenchConfig& cfg) {
    BenchResult result = {};
    
    uint32_t M = cfg.M, K = cfg.K, N = cfg.N;
    uint64_t flops = (uint64_t)M * K * N * 2; // multiply-add
    
    // Allocate host buffers
    std::vector<float> host_A(M * K);
    std::vector<float> host_B(K * N);
    std::vector<float> host_C_cpu(M * N);
    std::vector<float> host_C_gpu(M * N);
    
    FillRandom(host_A.data(), M * K);
    FillRandom(host_B.data(), K * N);
    
    printf("  MatMul %ux%u x %ux%u (%.2f MFLOP)\n", M, K, K, N, flops / 1e6);
    
    // --- CPU Benchmark ---
    printf("  CPU warmup...\n");
    for (uint32_t i = 0; i < cfg.warmup_iters; i++) {
        CpuMatMul(host_A.data(), host_B.data(), host_C_cpu.data(), M, K, N);
    }
    
    printf("  CPU benchmark (%u iters)...\n", cfg.bench_iters);
    double cpu_start = GetTimeMs();
    for (uint32_t i = 0; i < cfg.bench_iters; i++) {
        CpuMatMul(host_A.data(), host_B.data(), host_C_cpu.data(), M, K, N);
    }
    double cpu_end = GetTimeMs();
    result.cpu_ms = (cpu_end - cpu_start) / cfg.bench_iters;
    
    // --- GPU Benchmark ---
    if (vk.IsInitialized()) {
        printf("  GPU warmup...\n");
        
        uint32_t buf_A, buf_B, buf_C;
        if (!vk.AllocateBuffer(M * K * sizeof(float), buf_A)) {
            strcpy(result.error_msg, "Failed to allocate buffer A");
            return result;
        }
        if (!vk.AllocateBuffer(K * N * sizeof(float), buf_B)) {
            strcpy(result.error_msg, "Failed to allocate buffer B");
            return result;
        }
        if (!vk.AllocateBuffer(M * N * sizeof(float), buf_C)) {
            strcpy(result.error_msg, "Failed to allocate buffer C");
            return result;
        }
        
        // Upload data
        vk.CopyHostToBuffer(host_A.data(), buf_A, M * K * sizeof(float));
        vk.CopyHostToBuffer(host_B.data(), buf_B, K * N * sizeof(float));
        
        // Warmup
        for (uint32_t i = 0; i < cfg.warmup_iters; i++) {
            vk.DispatchMatMul(buf_A, buf_B, buf_C, M, K, N);
        }
        
        printf("  GPU benchmark (%u iters)...\n", cfg.bench_iters);
        double gpu_start = GetTimeMs();
        for (uint32_t i = 0; i < cfg.bench_iters; i++) {
            vk.DispatchMatMul(buf_A, buf_B, buf_C, M, K, N);
        }
        double gpu_end = GetTimeMs();
        result.gpu_ms = (gpu_end - gpu_start) / cfg.bench_iters;
        
        // Read back
        vk.CopyBufferToHost(buf_C, host_C_gpu.data(), M * N * sizeof(float));
        
        // Verify (sample a few values)
        float max_err = 0.0f;
        for (uint32_t i = 0; i < std::min(100u, M * N); i++) {
            float err = fabsf(host_C_cpu[i] - host_C_gpu[i]);
            if (err > max_err) max_err = err;
        }
        printf("  Max error: %.6f\n", max_err);
        
        result.gpu_success = true;
        result.gflops = (double)flops / (result.gpu_ms * 1e6);
    } else {
        strcpy(result.error_msg, "Vulkan not initialized");
    }
    
    return result;
}

// =============================================================================
// Attention Benchmark
// =============================================================================
static BenchResult BenchmarkAttention(RawrXD::VulkanCompute& vk, const BenchConfig& cfg) {
    BenchResult result = {};
    
    uint32_t seq_len = cfg.seq_len;
    uint32_t head_dim = cfg.head_dim;
    uint32_t num_heads = cfg.num_heads;
    uint64_t flops = (uint64_t)seq_len * head_dim * num_heads * 2 + 
                     (uint64_t)seq_len * seq_len * num_heads;
    
    // Allocate host buffers
    std::vector<float> host_Q(num_heads * head_dim);
    std::vector<float> host_K(seq_len * num_heads * head_dim);
    std::vector<float> host_V(seq_len * num_heads * head_dim);
    std::vector<float> host_out_cpu(num_heads * head_dim);
    std::vector<float> host_out_gpu(num_heads * head_dim);
    
    FillRandom(host_Q.data(), num_heads * head_dim);
    FillRandom(host_K.data(), seq_len * num_heads * head_dim);
    FillRandom(host_V.data(), seq_len * num_heads * head_dim);
    
    printf("  Attention seq=%u heads=%u dim=%u (%.2f MFLOP)\n", 
           seq_len, num_heads, head_dim, flops / 1e6);
    
    // --- CPU Benchmark ---
    printf("  CPU warmup...\n");
    for (uint32_t i = 0; i < cfg.warmup_iters; i++) {
        CpuAttention(host_Q.data(), host_K.data(), host_V.data(), 
                     host_out_cpu.data(), seq_len, head_dim, num_heads);
    }
    
    printf("  CPU benchmark (%u iters)...\n", cfg.bench_iters);
    double cpu_start = GetTimeMs();
    for (uint32_t i = 0; i < cfg.bench_iters; i++) {
        CpuAttention(host_Q.data(), host_K.data(), host_V.data(), 
                     host_out_cpu.data(), seq_len, head_dim, num_heads);
    }
    double cpu_end = GetTimeMs();
    result.cpu_ms = (cpu_end - cpu_start) / cfg.bench_iters;
    
    // --- GPU Benchmark ---
    if (vk.IsInitialized()) {
        printf("  GPU warmup...\n");
        
        uint32_t buf_Q, buf_K, buf_V, buf_out;
        if (!vk.AllocateBuffer(num_heads * head_dim * sizeof(float), buf_Q)) {
            strcpy(result.error_msg, "Failed to allocate Q buffer");
            return result;
        }
        if (!vk.AllocateBuffer(seq_len * num_heads * head_dim * sizeof(float), buf_K)) {
            strcpy(result.error_msg, "Failed to allocate K buffer");
            return result;
        }
        if (!vk.AllocateBuffer(seq_len * num_heads * head_dim * sizeof(float), buf_V)) {
            strcpy(result.error_msg, "Failed to allocate V buffer");
            return result;
        }
        if (!vk.AllocateBuffer(num_heads * head_dim * sizeof(float), buf_out)) {
            strcpy(result.error_msg, "Failed to allocate output buffer");
            return result;
        }
        
        // Upload
        vk.CopyHostToBuffer(host_Q.data(), buf_Q, num_heads * head_dim * sizeof(float));
        vk.CopyHostToBuffer(host_K.data(), buf_K, seq_len * num_heads * head_dim * sizeof(float));
        vk.CopyHostToBuffer(host_V.data(), buf_V, seq_len * num_heads * head_dim * sizeof(float));
        
        // Warmup
        for (uint32_t i = 0; i < cfg.warmup_iters; i++) {
            vk.DispatchFlashAttentionV2(buf_Q, buf_K, buf_V, buf_out,
                                         seq_len, head_dim, num_heads);
        }
        
        printf("  GPU benchmark (%u iters)...\n", cfg.bench_iters);
        double gpu_start = GetTimeMs();
        for (uint32_t i = 0; i < cfg.bench_iters; i++) {
            vk.DispatchFlashAttentionV2(buf_Q, buf_K, buf_V, buf_out,
                                         seq_len, head_dim, num_heads);
        }
        double gpu_end = GetTimeMs();
        result.gpu_ms = (gpu_end - gpu_start) / cfg.bench_iters;
        
        // Read back
        vk.CopyBufferToHost(buf_out, host_out_gpu.data(), num_heads * head_dim * sizeof(float));
        
        // Verify
        float max_err = 0.0f;
        for (uint32_t i = 0; i < num_heads * head_dim; i++) {
            float err = fabsf(host_out_cpu[i] - host_out_gpu[i]);
            if (err > max_err) max_err = err;
        }
        printf("  Max error: %.6f\n", max_err);
        
        result.gpu_success = true;
        result.gflops = (double)flops / (result.gpu_ms * 1e6);
        // Estimate tokens/sec: assume 1 token = seq_len * head_dim * num_heads ops
        result.tokens_per_sec = 1000.0 / result.gpu_ms;
    } else {
        strcpy(result.error_msg, "Vulkan not initialized");
    }
    
    return result;
}

// =============================================================================
// Print Results
// =============================================================================
static void PrintResults(const char* name, const BenchResult& r) {
    printf("\n");
    printf("  ═══════════════════════════════════════════════════════\n");
    printf("  %s BENCHMARK RESULTS\n", name);
    printf("  ═══════════════════════════════════════════════════════\n");
    printf("  CPU time:      %.3f ms\n", r.cpu_ms);
    if (r.gpu_success) {
        printf("  GPU time:      %.3f ms\n", r.gpu_ms);
        printf("  Speedup:       %.2fx\n", r.cpu_ms / r.gpu_ms);
        printf("  Throughput:    %.2f GFLOPS\n", r.gflops);
        if (r.tokens_per_sec > 0) {
            printf("  Tokens/sec:    %.1f\n", r.tokens_per_sec);
        }
    } else {
        printf("  GPU:           FAILED - %s\n", r.error_msg);
    }
    printf("  ═══════════════════════════════════════════════════════\n\n");
}

// =============================================================================
// Main Entry
// =============================================================================
int main(int argc, char** argv) {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║         RawrXD Vulkan Compute Benchmark Harness              ║\n");
    printf("║         GPU vs CPU Inference Performance Test                ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    // Parse args
    const char* test_type = "all";
    uint32_t size = 512;
    
    if (argc > 1) test_type = argv[1];
    if (argc > 2) size = (uint32_t)atoi(argv[2]);
    
    BenchConfig cfg;
    cfg.M = cfg.K = cfg.N = size;
    cfg.seq_len = size;
    
    // Initialize Vulkan
    printf("Initializing Vulkan...\n");
    RawrXD::VulkanCompute vk;
    bool vk_ok = vk.Initialize();
    
    if (vk_ok) {
        const auto& info = vk.GetDeviceInfo();
        printf("  Device: %s\n", info.device_name.c_str());
        printf("  Vendor: %s\n", 
               vk.IsNvidiaDevice() ? "NVIDIA" :
               vk.IsAMDDevice() ? "AMD" :
               vk.IsIntelDevice() ? "Intel" : "Unknown");
        printf("  Vulkan initialized successfully\n\n");
    } else {
        printf("  Vulkan initialization FAILED - CPU-only mode\n\n");
    }
    
    // Run benchmarks
    if (strcmp(test_type, "matmul") == 0 || strcmp(test_type, "all") == 0) {
        BenchResult r = BenchmarkMatMul(vk, cfg);
        PrintResults("MATMUL", r);
    }
    
    if (strcmp(test_type, "attention") == 0 || strcmp(test_type, "all") == 0) {
        BenchResult r = BenchmarkAttention(vk, cfg);
        PrintResults("ATTENTION", r);
    }
    
    // Print stats
    if (vk_ok) {
        const auto& stats = vk.GetStats();
        printf("Vulkan Stats:\n");
        printf("  Dispatches:    %llu\n", (unsigned long long)stats.dispatch_count.load());
        printf("  MatMuls:       %llu\n", (unsigned long long)stats.matmul_count.load());
        printf("  Attentions:     %llu\n", (unsigned long long)stats.attention_count.load());
        printf("  Buffer allocs:  %llu bytes\n", (unsigned long long)stats.buffer_alloc_bytes.load());
    }
    
    // Cleanup
    vk.Cleanup();
    
    printf("\nBenchmark complete.\n");
    return 0;
}