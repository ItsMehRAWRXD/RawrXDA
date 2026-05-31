// gpu_ab_telemetry.cpp
// A/B telemetry harness: CPU AVX512 vs GPU Vulkan FP8 quantizer.
// Emits JSON compatible with telemetry/v1.0.0-gold-baseline.json for regression detection.
// Compile: g++ -O3 -std=c++20 -I d:\rawrxd\include -I d:\rawrxd\src -lvulkan gpu_ab_telemetry.cpp gpu_dispatch.cpp -o gpu_ab_telemetry.exe

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>

// CPU fallback (same as telemetry_benchmark.cpp)
extern "C" void sovereign_fp8_quantize_avx512(const float* src, uint8_t* dst, size_t n, float scale) {
    for (size_t i = 0; i < n; ++i) {
        float v = src[i] * scale;
        v = std::max(-448.0f, std::min(448.0f, v));
        dst[i] = static_cast<uint8_t>(static_cast<int>(v) & 0xFF);
    }
}

// GPU dispatch (from gpu_dispatch.hpp)
#include "gpu_dispatch.hpp"

struct AbResult {
    double mean_ms{0.0};
    double min_ms{0.0};
    double p95_ms{0.0};
    double bandwidth_gbps{0.0};
    double tps{0.0};
    size_t elements{0};
};

static AbResult benchmark_cpu(const float* src, uint8_t* dst, size_t n, float scale, size_t iterations) {
    std::vector<double> times;
    times.reserve(iterations);
    size_t totalElements = n * iterations;
    for (size_t i = 0; i < iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        sovereign_fp8_quantize_avx512(src, dst, n, scale);
        auto t1 = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::sort(times.begin(), times.end());
    double sum = 0.0;
    for (auto v : times) sum += v;
    double mean = sum / times.size();
    double bytes = static_cast<double>(totalElements) * (sizeof(float) + sizeof(uint8_t));
    double elapsed_sec = sum / 1000.0;
    return {
        mean,
        times.front(),
        times[static_cast<size_t>(times.size() * 0.95)],
        (bytes / elapsed_sec) / 1e9,
        static_cast<double>(totalElements) / elapsed_sec,
        totalElements
    };
}

static AbResult benchmark_gpu(rawrxd::gpu::Fp8QuantizerDispatch& gpu, const float* src, uint8_t* dst, size_t n, float scale, size_t iterations) {
    std::vector<double> times;
    times.reserve(iterations);
    size_t totalElements = n * iterations;
    for (size_t i = 0; i < iterations; ++i) {
        auto r = gpu.dispatch(src, dst, n, scale);
        if (!r.ok) {
            fprintf(stderr, "GPU dispatch failed: %s\n", r.error_msg ? r.error_msg : "unknown");
            return {};
        }
        times.push_back(r.elapsed_ms);
    }
    std::sort(times.begin(), times.end());
    double sum = 0.0;
    for (auto v : times) sum += v;
    double mean = sum / times.size();
    double bytes = static_cast<double>(totalElements) * (sizeof(float) + sizeof(uint8_t));
    double elapsed_sec = sum / 1000.0;
    return {
        mean,
        times.front(),
        times[static_cast<size_t>(times.size() * 0.95)],
        (bytes / elapsed_sec) / 1e9,
        static_cast<double>(totalElements) / elapsed_sec,
        totalElements
    };
}

int main(int argc, char** argv) {
    size_t n = 4096;
    size_t iterations = 1000;
    if (argc > 1) n = static_cast<size_t>(std::atoll(argv[1]));
    if (argc > 2) iterations = static_cast<size_t>(std::atoll(argv[2]));

    std::vector<float> src(n);
    std::vector<uint8_t> dst_cpu(n);
    std::vector<uint8_t> dst_gpu(n);
    for (size_t i = 0; i < n; ++i) src[i] = static_cast<float>(i % 1000) / 1000.0f * 2.0f - 1.0f;
    const float scale = 127.0f / 448.0f;

    printf("[gpu_ab_telemetry] A/B sweep: batch=%zu iterations=%zu\n", n, iterations);

    // CPU baseline
    auto cpu = benchmark_cpu(src.data(), dst_cpu.data(), n, scale, iterations);
    printf("[gpu_ab_telemetry] CPU  mean=%.4f ms p95=%.4f ms bw=%.3f GB/s tps=%.2f\n",
           cpu.mean_ms, cpu.p95_ms, cpu.bandwidth_gbps, cpu.tps);

    // GPU candidate
    rawrxd::gpu::Fp8QuantizerDispatch gpu;
    AbResult gpu_r{};
    if (gpu.initialize("D:\\rawrxd\\src\\kernels\\gpu\\fp8_quantize.spv")) {
        gpu_r = benchmark_gpu(gpu, src.data(), dst_gpu.data(), n, scale, iterations);
        if (gpu_r.elements > 0) {
            printf("[gpu_ab_telemetry] GPU  mean=%.4f ms p95=%.4f ms bw=%.3f GB/s tps=%.2f\n",
                   gpu_r.mean_ms, gpu_r.p95_ms, gpu_r.bandwidth_gbps, gpu_r.tps);
        }
        gpu.shutdown();
    } else {
        printf("[gpu_ab_telemetry] GPU init failed (Vulkan may be unavailable)\n");
    }

    // Verify parity
    bool parity = true;
    for (size_t i = 0; i < n; ++i) {
        if (dst_cpu[i] != dst_gpu[i]) { parity = false; break; }
    }
    printf("[gpu_ab_telemetry] Parity: %s\n", parity ? "PASS" : "FAIL");

    // Emit JSON
    printf("\nJSON_BEGIN\n");
    printf("{\n");
    printf("  \"version\": \"v1.1.0-dev-gpu\",\n");
    printf("  \"batch_size\": %zu,\n", n);
    printf("  \"iterations\": %zu,\n", iterations);
    printf("  \"cpu\": {\n");
    printf("    \"mean_ms\": %.4f,\n", cpu.mean_ms);
    printf("    \"p95_ms\": %.4f,\n", cpu.p95_ms);
    printf("    \"bandwidth_gbps\": %.3f,\n", cpu.bandwidth_gbps);
    printf("    \"tps\": %.2f\n", cpu.tps);
    printf("  },\n");
    printf("  \"gpu\": {\n");
    printf("    \"mean_ms\": %.4f,\n", gpu_r.mean_ms);
    printf("    \"p95_ms\": %.4f,\n", gpu_r.p95_ms);
    printf("    \"bandwidth_gbps\": %.3f,\n", gpu_r.bandwidth_gbps);
    printf("    \"tps\": %.2f\n", gpu_r.tps);
    printf("  },\n");
    printf("  \"parity\": %s,\n", parity ? "true" : "false");
    printf("  \"speedup\": %.2f\n", gpu_r.mean_ms > 0.0 ? cpu.mean_ms / gpu_r.mean_ms : 0.0);
    printf("}\n");
    printf("JSON_END\n");

    return parity ? 0 : 1;
}
