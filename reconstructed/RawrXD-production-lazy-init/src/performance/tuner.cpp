#include "performance_tuner.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#else
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

namespace Performance {

PerformanceTuner::PerformanceTuner() {
    hardware_ = DetectHardware();
    config_ = GenerateConfig(hardware_);
}

HardwareProfile PerformanceTuner::DetectHardware() {
    HardwareProfile profile;
    
#ifdef _WIN32
    // Get CPU info
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    profile.cpu_cores = sysInfo.dwNumberOfProcessors;
    profile.cpu_threads = std::thread::hardware_concurrency();
    
    // Get memory info
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        profile.total_ram_mb = memInfo.ullTotalPhys / (1024 * 1024);
        profile.available_ram_mb = memInfo.ullAvailPhys / (1024 * 1024);
    }
    
    // Detect CPU features
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    profile.has_avx2 = (cpuInfo[2] & (1 << 28)) != 0;  // AVX bit
    
    __cpuid(cpuInfo, 7);
    profile.has_avx512 = (cpuInfo[1] & (1 << 16)) != 0;  // AVX-512F bit
#else
    profile.cpu_threads = std::thread::hardware_concurrency();
    profile.cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
    
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        profile.total_ram_mb = info.totalram / (1024 * 1024);
        profile.available_ram_mb = info.freeram / (1024 * 1024);
    }
#endif
    
    // Add GPU detection for CUDA, ROCm, or Vulkan
#ifdef _WIN32
    // Check for CUDA on Windows
    const char* cuda_path = std::getenv("CUDA_PATH");
    if (cuda_path) {
        profile.has_gpu_compute = true;
        std::cout << "[PerformanceTuner] CUDA detected at: " << cuda_path << std::endl;
    }
#else
    // Check for GPU on Linux
    if (std::system("command -v nvidia-smi >/dev/null 2>&1") == 0) {
        profile.has_gpu_compute = true;
        std::cout << "[PerformanceTuner] NVIDIA GPU detected" << std::endl;
    } else if (std::system("command -v rocm-smi >/dev/null 2>&1") == 0) {
        profile.has_gpu_compute = true;
        std::cout << "[PerformanceTuner] AMD ROCm GPU detected" << std::endl;
    }
#endif
    
    if (!profile.has_gpu_compute) {
        std::cout << "[PerformanceTuner] No GPU acceleration detected, using CPU only" << std::endl;
    }
    
    return profile;
}

AdaptiveConfig PerformanceTuner::GenerateConfig(const HardwareProfile& hardware) {
    AdaptiveConfig config;
    
    // Threading configuration
    config.worker_threads = std::max(2u, hardware.cpu_threads - 2);
    config.io_threads = std::min(4u, hardware.cpu_threads / 4);
    config.compute_threads = std::max(1u, hardware.cpu_threads / 2);
    
    // Memory configuration (use 60% of available RAM)
    uint64_t usable_ram = static_cast<uint64_t>(hardware.available_ram_mb * 0.6);
    
    // Allocate memory hierarchy
    config.model_cache_size_mb = std::min(8192ull, usable_ram / 2);
    config.kv_cache_size_mb = std::min(4096ull, usable_ram / 4);
    config.context_cache_size_mb = std::min(1024ull, usable_ram / 8);
    
    // Batch size based on RAM
    if (usable_ram > 16000) {
        config.batch_size = 1024;
        config.mini_batch_size = 64;
    } else if (usable_ram > 8000) {
        config.batch_size = 512;
        config.mini_batch_size = 32;
    } else {
        config.batch_size = 256;
        config.mini_batch_size = 16;
    }
    
    // GPU configuration
    if (hardware.has_gpu_compute) {
        config.use_gpu_offload = true;
        config.gpu_layers = 32;  // Default, will be tuned
        config.gpu_memory_fraction = 0.85f;
    }
    
    // Enable optimizations based on CPU features
    config.enable_flash_attention = hardware.has_avx2 || hardware.has_avx512;
    config.enable_quantization = true;
    config.enable_tensor_parallelism = hardware.cpu_threads >= 8;
    
    return config;
}

void PerformanceTuner::ApplyConfig(const AdaptiveConfig& config) {
    config_ = config;
    
    std::cout << "=== Performance Configuration Applied ===" << std::endl;
    std::cout << "Worker Threads: " << config.worker_threads << std::endl;
    std::cout << "I/O Threads: " << config.io_threads << std::endl;
    std::cout << "Compute Threads: " << config.compute_threads << std::endl;
    std::cout << "KV Cache: " << config.kv_cache_size_mb << " MB" << std::endl;
    std::cout << "Model Cache: " << config.model_cache_size_mb << " MB" << std::endl;
    std::cout << "Batch Size: " << config.batch_size << std::endl;
    
    if (config.use_gpu_offload) {
        std::cout << "GPU Offload: Enabled (" << config.gpu_layers << " layers)" << std::endl;
    }
    
    std::cout << "Flash Attention: " << (config.enable_flash_attention ? "Enabled" : "Disabled") << std::endl;
    std::cout << "=========================================" << std::endl;
}

void PerformanceTuner::AutoTune() {
    std::cout << "Starting performance auto-tuning..." << std::endl;
    
    // Run basic benchmarks to adjust configuration
    auto start = std::chrono::high_resolution_clock::now();
    
    // Benchmark 1: Memory bandwidth test
    const size_t test_size = 100 * 1024 * 1024;  // 100 MB
    std::vector<uint8_t> buffer(test_size);
    
    // Warm up
    std::fill(buffer.begin(), buffer.end(), 0);
    
    auto mem_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 3; i++) {
        std::fill(buffer.begin(), buffer.end(), i);
    }
    auto mem_end = std::chrono::high_resolution_clock::now();
    
    double bandwidth_gb_s = (test_size * 3.0 / 1e9) / 
        std::chrono::duration<double>(mem_end - mem_start).count();
    
    std::cout << "[AutoTune] Memory bandwidth: " << bandwidth_gb_s << " GB/s" << std::endl;
    
    // Benchmark 2: CPU speed test
    auto cpu_start = std::chrono::high_resolution_clock::now();
    volatile uint64_t result = 0;
    for (uint64_t i = 0; i < 1000000000; i++) {
        result ^= (i * 7);
    }
    auto cpu_end = std::chrono::high_resolution_clock::now();
    
    double cpu_gops = 1000.0 / std::chrono::duration<double>(cpu_end - cpu_start).count();
    std::cout << "[AutoTune] CPU performance: " << cpu_gops << " GOP/s" << std::endl;
    
    // Adjust configuration based on benchmarks
    if (bandwidth_gb_s < 50) {
        config_.batch_size = std::max(128, (int)(config_.batch_size / 2));
    } else if (bandwidth_gb_s > 200) {
        config_.batch_size = std::min(2048, (int)(config_.batch_size * 2));
    }
    
    ApplyConfig(config_);
}

PerformanceTuner::Metrics PerformanceTuner::GetMetrics() const {
    return metrics_;
}


// Global instance
static PerformanceTuner global_tuner;

PerformanceTuner& GetPerformanceTuner() {
    return global_tuner;
}

} // namespace Performance
