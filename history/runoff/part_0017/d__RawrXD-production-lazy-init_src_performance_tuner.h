#pragma once

#include <cstdint>
#include <string>
#include <thread>

namespace Performance {

// Hardware detection and adaptive configuration
struct HardwareProfile {
    uint32_t cpu_cores = 0;
    uint32_t cpu_threads = 0;
    uint64_t total_ram_mb = 0;
    uint64_t available_ram_mb = 0;
    bool has_avx2 = false;
    bool has_avx512 = false;
    bool has_gpu_compute = false;
    std::string gpu_name;
    uint64_t gpu_vram_mb = 0;
};

// Adaptive configuration based on hardware
struct AdaptiveConfig {
    // Threading
    uint32_t worker_threads = 4;
    uint32_t io_threads = 2;
    uint32_t compute_threads = 4;
    
    // Memory
    size_t kv_cache_size_mb = 2048;
    size_t model_cache_size_mb = 4096;
    size_t context_cache_size_mb = 512;
    
    // Batch processing
    uint32_t batch_size = 512;
    uint32_t mini_batch_size = 32;
    
    // GPU
    bool use_gpu_offload = false;
    uint32_t gpu_layers = 0;
    float gpu_memory_fraction = 0.9f;
    
    // Performance hints
    bool enable_flash_attention = false;
    bool enable_quantization = true;
    bool enable_tensor_parallelism = false;
};

class PerformanceTuner {
public:
    PerformanceTuner();
    
    // Detect hardware capabilities
    static HardwareProfile DetectHardware();
    
    // Generate optimal configuration
    static AdaptiveConfig GenerateConfig(const HardwareProfile& hardware);
    
    // Apply configuration
    void ApplyConfig(const AdaptiveConfig& config);
    
    // Get current configuration
    const AdaptiveConfig& GetConfig() const { return config_; }
    
    // Benchmark and auto-tune
    void AutoTune();
    
    // Get performance metrics
    struct Metrics {
        float tokens_per_second = 0.0f;
        float memory_utilization = 0.0f;
        float gpu_utilization = 0.0f;
        uint64_t total_tokens_processed = 0;
    };
    
    Metrics GetMetrics() const;
    
private:
    AdaptiveConfig config_;
    HardwareProfile hardware_;
    Metrics metrics_;
};

// Global performance tuner instance
extern PerformanceTuner& GetPerformanceTuner();

} // namespace Performance
