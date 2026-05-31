#pragma once

#include <cstdint>
#include <string>

namespace RawrXD {

struct ContextLimits {
    static constexpr int32_t TINY = 4096;
    static constexpr int32_t STANDARD = 32768;
    static constexpr int32_t LARGE = 131072;
    static constexpr int32_t XL = 262144;
    static constexpr int32_t FLAGSHIP = 1048576;

    // Production default for IDE parity targets.
    static constexpr int32_t DEFAULT = LARGE;

    static int32_t getSystemSafeMax(
        int64_t vramBytes = 16LL * 1024LL * 1024LL * 1024LL,
        int64_t ramBytes = 64LL * 1024LL * 1024LL * 1024LL);

    static int32_t getKVSafeMax(
        int64_t vramBytes = 16LL * 1024LL * 1024LL * 1024LL,
        int64_t ramBytes = 64LL * 1024LL * 1024LL * 1024LL,
        int64_t kvBytesPerToken = 81920,
        float safetyMargin = 0.8f);

    static constexpr int64_t DEFAULT_KV_BYTES_PER_TOKEN = 81920;
    static int64_t estimateKVBytes(int32_t contextTokens,
                                   int64_t kvBytesPerToken = DEFAULT_KV_BYTES_PER_TOKEN);
};

struct ContextDecision {
    int32_t requested = ContextLimits::DEFAULT;
    bool env_override_applied = false;
    int32_t env_override_value = 0;
    int32_t system_safe_max = ContextLimits::DEFAULT;
    int32_t kv_safe_max = ContextLimits::DEFAULT;
    int32_t effective = ContextLimits::DEFAULT;
    int64_t estimated_kv_bytes = 0;
    int64_t vram_budget_bytes = 0;
    int64_t kv_budget_bytes = 0;
    bool pressure_detected = false;
    bool adapted = false;
    double pressure_ratio = 0.0;
    double kv_bytes_per_token = 0.0;
};

struct ContextResolveHints {
    bool latency_sensitive = false;
    float latency_scale = 0.5f;
    float pressure_threshold = 0.9f;
    float pressure_scale = 0.5f;
    int64_t explicit_kv_budget_bytes = 0;
};

ContextDecision ResolveContextDecision(
    int32_t requested,
    int64_t vramBytes = 16LL * 1024LL * 1024LL * 1024LL,
    int64_t ramBytes = 64LL * 1024LL * 1024LL * 1024LL,
    const char* envVarName = "RAWRXD_CTX");

ContextDecision ResolveContextDecisionWithHints(
    int32_t requested,
    const ContextResolveHints& hints,
    int64_t vramBytes = 16LL * 1024LL * 1024LL * 1024LL,
    int64_t ramBytes = 64LL * 1024LL * 1024LL * 1024LL,
    const char* envVarName = "RAWRXD_CTX");

struct UnifiedContextConfig {
    int32_t context_limit = ContextLimits::DEFAULT;
    int32_t max_tokens = 4096;
    float memory_safety_margin = 0.8f;

    bool use_kv_quantization = true;
    bool use_flash_attention = true;
    bool enable_kv_offload = false;

    static UnifiedContextConfig forModel(const std::string& modelName,
                                         int64_t availableVramBytes);
};

} // namespace RawrXD
