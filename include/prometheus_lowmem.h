#pragma once
#include "prometheus_config_lowmem.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Prometheus {

// ============================================================================
// Message / Token / Result types
// ============================================================================

struct Message {
    std::string role;      // "system", "user", "assistant"
    std::string content;
};

struct Token {
    uint32_t id = 0;
    std::string text;
};

struct GenerationResult {
    std::string text;
    uint32_t totalTokens = 0;
    uint32_t promptTokens = 0;
    uint32_t completionTokens = 0;
};

struct GenerationConfig {
    uint32_t maxTokens = 1024;
    float temperature = 0.7f;
    float topP = 0.9f;
    float topK = 40.0f;
    float repetitionPenalty = 1.0f;
};

struct MemoryStats {
    uint64_t modelBytesLoaded = 0;
    uint64_t modelBytesTotal = 0;
    uint64_t kvCacheBytes = 0;
    uint64_t gpuBytesUsed = 0;
    uint64_t gpuBytesTotal = 0;
    float systemRAMPercent = 0.0f;
    float gpuVRAMPercent = 0.0f;
};

using StreamCallback = std::function<void(const Token&)>;

// ============================================================================
// PrometheusLowMemory — Abstract interface
// ============================================================================

class PrometheusLowMemory {
public:
    virtual ~PrometheusLowMemory() = default;

    static std::unique_ptr<PrometheusLowMemory> create(
        const std::string& modelPath,
        LowMemoryConfig config = {}
    );

    virtual bool loadModel(const std::string& path) = 0;

    virtual GenerationResult generate(
        const std::vector<Message>& messages,
        GenerationConfig config = {}
    ) = 0;

    virtual void generateStream(
        const std::vector<Message>& messages,
        StreamCallback onToken,
        GenerationConfig config = {}
    ) = 0;

    virtual MemoryStats getMemoryStats() const = 0;
    virtual void pruneKVCache(float ratio = 0.5f) = 0;
    virtual void clearKVCache() = 0;
    virtual void setContextWindow(uint32_t newWindow) = 0;
    virtual void setGPULayers(uint32_t layers) = 0;
};

} // namespace Prometheus
