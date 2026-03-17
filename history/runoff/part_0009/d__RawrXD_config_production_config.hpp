// config/production_config.hpp
#pragma once
#include <cstdint>
#include <string>

namespace RawrXD::Config {

// Production configuration - zero external dependencies
struct ProductionConfig {
    // Model settings
    static constexpr const char* DEFAULT_MODEL_PATH = "models/default.gguf";
    static constexpr uint32_t MAX_CONTEXT_LENGTH = 4096;
    static constexpr uint32_t MAX_BATCH_SIZE = 32;
    
    // Memory settings
    static constexpr size_t MAX_MEMORY_USAGE_MB = 4096;
    static constexpr bool USE_MEMORY_MAPPING = true;
    
    // Performance settings
    static constexpr bool ENABLE_AVX2 = true;
    static constexpr bool ENABLE_AVX512 = false; // Not all CPUs support this
    
    // Logging settings
    static constexpr bool ENABLE_METRICS = true;
    static constexpr int METRICS_PORT = 9090;
    
    // Threading
    static constexpr uint32_t MAX_THREADS = 8;
    
    // Get config values at runtime
    static std::string getModelPath();
    static uint32_t getMaxContextLength();
    static uint32_t getMaxBatchSize();
    static size_t getMaxMemoryUsageMB();
    static bool getUseMemoryMapping();
    static bool getEnableAVX2();
    static bool getEnableAVX512();
    static bool getEnableMetrics();
    static int getMetricsPort();
    static uint32_t getMaxThreads();
};

} // namespace RawrXD::Config