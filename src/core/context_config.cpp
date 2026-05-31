#include "context_config.h"
#include "vram_probe.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace RawrXD {

namespace {

int32_t clampContextTokenRange(int32_t value)
{
    if (value < ContextLimits::TINY) {
        return ContextLimits::TINY;
    }
    if (value > ContextLimits::FLAGSHIP) {
        return ContextLimits::FLAGSHIP;
    }
    return value;
}

float clampScale(float value, float fallback)
{
    if (value > 0.0f && value <= 1.0f) {
        return value;
    }
    return fallback;
}

float clampThreshold(float value, float fallback)
{
    if (value > 0.0f && value < 1.0f) {
        return value;
    }
    return fallback;
}

int32_t finalClamp(int32_t requested, int32_t hardMax)
{
    int32_t effective = requested;
    if (effective < ContextLimits::TINY) {
        effective = ContextLimits::TINY;
    }
    if (effective > hardMax) {
        effective = hardMax;
    }
    return effective;
}

int32_t parsePositiveContextOverride(const char* envVarName)
{
    if (!envVarName || !*envVarName) {
        return 0;
    }

#ifdef _WIN32
    char* value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, envVarName) != 0 || value == nullptr) {
        return 0;
    }

    std::string text(value);
    free(value);
#else
    const char* raw = std::getenv(envVarName);
    if (!raw || !*raw) {
        return 0;
    }
    std::string text(raw);
#endif

    try {
        size_t consumed = 0;
        long long parsed = std::stoll(text, &consumed, 10);
        if (consumed != text.size() || parsed <= 0 || parsed > static_cast<long long>(ContextLimits::FLAGSHIP)) {
            return 0;
        }
        return static_cast<int32_t>(parsed);
    } catch (...) {
        return 0;
    }
}

int64_t parsePositiveInt64Env(const char* envVarName)
{
    if (!envVarName || !*envVarName) {
        return 0;
    }

#ifdef _WIN32
    char* value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, envVarName) != 0 || value == nullptr) {
        return 0;
    }

    std::string text(value);
    free(value);
#else
    const char* raw = std::getenv(envVarName);
    if (!raw || !*raw) {
        return 0;
    }
    std::string text(raw);
#endif

    try {
        size_t consumed = 0;
        long long parsed = std::stoll(text, &consumed, 10);
        if (consumed != text.size() || parsed <= 0) {
            return 0;
        }
        return static_cast<int64_t>(parsed);
    } catch (...) {
        return 0;
    }
}

int64_t getDefaultKVBudgetBytes()
{
    const int64_t forcedBudget = parsePositiveInt64Env("RAWRXD_KV_BUDGET");
    if (forcedBudget > 0) {
        return forcedBudget;
    }

    const int64_t disableProbe = parsePositiveInt64Env("RAWRXD_DISABLE_VRAM_PROBE");
    if (disableProbe > 0) {
        return 2LL * 1024LL * 1024LL * 1024LL;
    }

    const RawrXD::VRAMInfo vram = RawrXD::QueryVRAM();
    if (vram.valid && vram.dedicated_free > 0) {
        const int64_t fromFree = static_cast<int64_t>((vram.dedicated_free * 8ULL) / 10ULL);
        const int64_t fromTotal = vram.dedicated_total > 0
            ? static_cast<int64_t>((vram.dedicated_total * 6ULL) / 10ULL)
            : fromFree;
        const int64_t headroomBudget = std::min(fromFree, fromTotal);
        if (headroomBudget > 0) {
            return headroomBudget;
        }
    }

    // Safe fallback when hardware probe is unavailable.
    return 2LL * 1024LL * 1024LL * 1024LL;
}

} // namespace

int32_t ContextLimits::getSystemSafeMax(int64_t vramBytes, int64_t ramBytes)
{
    constexpr int64_t bytesPerToken = DEFAULT_KV_BYTES_PER_TOKEN;

#ifdef _WIN32
    if (ramBytes <= 0) {
        MEMORYSTATUSEX memoryStatus = {};
        memoryStatus.dwLength = sizeof(memoryStatus);
        if (GlobalMemoryStatusEx(&memoryStatus)) {
            ramBytes = static_cast<int64_t>(memoryStatus.ullTotalPhys);
        }
    }
#endif

    const int64_t effectiveVram = (vramBytes > 0) ? vramBytes : (16LL * 1024LL * 1024LL * 1024LL);
    const int64_t effectiveRam = (ramBytes > 0) ? ramBytes : (64LL * 1024LL * 1024LL * 1024LL);

    const int64_t memoryBudget = std::min(effectiveVram, effectiveRam / 2);
    const int64_t rawTokens = memoryBudget / bytesPerToken;
    const int64_t safeTokens = static_cast<int64_t>(rawTokens * 0.8);

    if (safeTokens >= FLAGSHIP) {
        return FLAGSHIP;
    }
    if (safeTokens >= XL) {
        return XL;
    }
    if (safeTokens >= LARGE) {
        return LARGE;
    }
    if (safeTokens >= STANDARD) {
        return STANDARD;
    }

    return TINY;
}

int32_t ContextLimits::getKVSafeMax(int64_t vramBytes,
                                    int64_t ramBytes,
                                    int64_t kvBytesPerToken,
                                    float safetyMargin)
{
#ifdef _WIN32
    if (ramBytes <= 0) {
        MEMORYSTATUSEX memoryStatus = {};
        memoryStatus.dwLength = sizeof(memoryStatus);
        if (GlobalMemoryStatusEx(&memoryStatus)) {
            ramBytes = static_cast<int64_t>(memoryStatus.ullTotalPhys);
        }
    }
#endif

    const int64_t effectiveVram = (vramBytes > 0) ? vramBytes : (16LL * 1024LL * 1024LL * 1024LL);
    const int64_t effectiveRam = (ramBytes > 0) ? ramBytes : (64LL * 1024LL * 1024LL * 1024LL);
    const int64_t bytesPerToken = kvBytesPerToken > 0 ? kvBytesPerToken : DEFAULT_KV_BYTES_PER_TOKEN;

    const float margin = (safetyMargin > 0.0f && safetyMargin <= 1.0f) ? safetyMargin : 0.8f;
    const int64_t memoryBudget = std::min(effectiveVram, effectiveRam / 2);
    const int64_t safeBudget = static_cast<int64_t>(static_cast<double>(memoryBudget) * static_cast<double>(margin));
    const int64_t kvSafeTokens = safeBudget / bytesPerToken;

    if (kvSafeTokens <= 0) {
        return TINY;
    }

    if (kvSafeTokens > static_cast<int64_t>(FLAGSHIP)) {
        return FLAGSHIP;
    }

    if (kvSafeTokens < static_cast<int64_t>(TINY)) {
        return TINY;
    }

    return static_cast<int32_t>(kvSafeTokens);
}

int64_t ContextLimits::estimateKVBytes(int32_t contextTokens, int64_t kvBytesPerToken)
{
    if (contextTokens <= 0) {
        return 0;
    }

    const int64_t bytesPerToken = kvBytesPerToken > 0 ? kvBytesPerToken : DEFAULT_KV_BYTES_PER_TOKEN;
    const int64_t tokens = static_cast<int64_t>(contextTokens);
    return tokens * bytesPerToken;
}

ContextDecision ResolveContextDecisionWithHints(int32_t requested,
                                                const ContextResolveHints& hints,
                                                int64_t vramBytes,
                                                int64_t ramBytes,
                                                const char* envVarName)
{
    ContextDecision decision;
    decision.requested = requested > 0 ? requested : ContextLimits::DEFAULT;

    const int32_t envOverride = parsePositiveContextOverride(envVarName);
    if (envOverride > 0) {
        decision.env_override_applied = true;
        decision.env_override_value = envOverride;
        decision.requested = envOverride;
    }

    decision.system_safe_max = ContextLimits::getSystemSafeMax(vramBytes, ramBytes);
    decision.kv_safe_max = ContextLimits::getKVSafeMax(vramBytes, ramBytes);

    const int64_t effectiveVram = (vramBytes > 0) ? vramBytes : (16LL * 1024LL * 1024LL * 1024LL);
    const int64_t effectiveRam = (ramBytes > 0) ? ramBytes : (64LL * 1024LL * 1024LL * 1024LL);
    decision.vram_budget_bytes = effectiveVram;

    const int64_t memoryBoundBudget =
        static_cast<int64_t>(static_cast<double>(std::min(effectiveVram, effectiveRam / 2)) * 0.8);
    const int64_t defaultKvBudget = getDefaultKVBudgetBytes();
    decision.kv_budget_bytes = hints.explicit_kv_budget_bytes > 0
                                   ? hints.explicit_kv_budget_bytes
                                   : defaultKvBudget;
    if (memoryBoundBudget > 0) {
        decision.kv_budget_bytes = std::min(decision.kv_budget_bytes, memoryBoundBudget);
    }

    const float latencyScale = clampScale(hints.latency_scale, 0.5f);
    if (hints.latency_sensitive) {
        const int32_t scaled = static_cast<int32_t>(
            static_cast<double>(decision.requested) * static_cast<double>(latencyScale));
        decision.requested = clampContextTokenRange(scaled);
    }

    const int32_t hardMax = std::max(ContextLimits::TINY,
                                     std::min(decision.system_safe_max, decision.kv_safe_max));

    decision.effective = finalClamp(decision.requested, hardMax);
    decision.estimated_kv_bytes = ContextLimits::estimateKVBytes(decision.effective);
    if (decision.kv_budget_bytes > 0) {
        decision.pressure_ratio = static_cast<double>(decision.estimated_kv_bytes) /
                                  static_cast<double>(decision.kv_budget_bytes);
    } else {
        decision.pressure_ratio = 0.0;
    }
    if (decision.effective > 0) {
        decision.kv_bytes_per_token = static_cast<double>(decision.estimated_kv_bytes) /
                                      static_cast<double>(decision.effective);
    }

    const float pressureThreshold = clampThreshold(hints.pressure_threshold, 0.9f);
    const int64_t pressureLimit = static_cast<int64_t>(
        static_cast<double>(decision.kv_budget_bytes) * static_cast<double>(pressureThreshold));
    decision.pressure_detected = (decision.kv_budget_bytes > 0 && decision.estimated_kv_bytes > pressureLimit);
    decision.adapted = false;

    if (decision.pressure_detected) {
        int32_t reduced = decision.effective;
        if (decision.estimated_kv_bytes > 0 && decision.kv_budget_bytes > 0) {
            const int64_t proportional =
                (static_cast<int64_t>(decision.effective) * decision.kv_budget_bytes) / decision.estimated_kv_bytes;
            reduced = static_cast<int32_t>(proportional);
        } else {
            const float pressureScale = clampScale(hints.pressure_scale, 0.5f);
            reduced = static_cast<int32_t>(
                static_cast<double>(decision.effective) * static_cast<double>(pressureScale));
        }

        decision.effective = finalClamp(clampContextTokenRange(reduced), hardMax);
        decision.adapted = decision.effective < decision.requested;

        // Recompute KV usage after adaptive shrink.
        decision.estimated_kv_bytes = ContextLimits::estimateKVBytes(decision.effective);
        if (decision.effective > 0) {
            decision.kv_bytes_per_token = static_cast<double>(decision.estimated_kv_bytes) /
                                          static_cast<double>(decision.effective);
        } else {
            decision.kv_bytes_per_token = 0.0;
        }
    }

    // Second-pass correction to guarantee resolver exits with kv_bytes <= kv_budget.
    if (decision.kv_budget_bytes > 0 && decision.estimated_kv_bytes > decision.kv_budget_bytes && decision.estimated_kv_bytes > 0) {
        const int64_t corrected =
            (static_cast<int64_t>(decision.effective) * decision.kv_budget_bytes) / decision.estimated_kv_bytes;
        decision.effective = finalClamp(clampContextTokenRange(static_cast<int32_t>(corrected)), hardMax);
        decision.adapted = true;
        decision.estimated_kv_bytes = ContextLimits::estimateKVBytes(decision.effective);
    }

    // Final post-adaptation clamp pass.
    decision.effective = std::min(decision.effective, decision.kv_safe_max);
    decision.effective = finalClamp(decision.effective, hardMax);
    decision.estimated_kv_bytes = ContextLimits::estimateKVBytes(decision.effective);
    if (decision.kv_budget_bytes > 0) {
        decision.pressure_ratio = static_cast<double>(decision.estimated_kv_bytes) /
                                  static_cast<double>(decision.kv_budget_bytes);
    } else {
        decision.pressure_ratio = 0.0;
    }
    if (decision.effective > 0) {
        decision.kv_bytes_per_token = static_cast<double>(decision.estimated_kv_bytes) /
                                      static_cast<double>(decision.effective);
    } else {
        decision.kv_bytes_per_token = 0.0;
    }

    return decision;
}

ContextDecision ResolveContextDecision(int32_t requested,
                                       int64_t vramBytes,
                                       int64_t ramBytes,
                                       const char* envVarName)
{
    ContextResolveHints hints;
    return ResolveContextDecisionWithHints(requested, hints, vramBytes, ramBytes, envVarName);
}

UnifiedContextConfig UnifiedContextConfig::forModel(const std::string& modelName,
                                                     int64_t availableVramBytes)
{
    UnifiedContextConfig config;

    const std::string lowered = [&modelName]() {
        std::string text = modelName;
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return text;
    }();

    if (lowered.find("phi-3") != std::string::npos ||
        lowered.find("llama-3") != std::string::npos ||
        lowered.find("qwen") != std::string::npos) {
        config.context_limit = ContextLimits::LARGE;
    } else if (lowered.find("tiny") != std::string::npos ||
               lowered.find("mini") != std::string::npos) {
        config.context_limit = ContextLimits::XL;
    } else {
        config.context_limit = ContextLimits::STANDARD;
    }

    const int64_t vram8GiB = 8LL * 1024LL * 1024LL * 1024LL;
    const int64_t vram16GiB = 16LL * 1024LL * 1024LL * 1024LL;

    if (availableVramBytes > 0) {
        if (availableVramBytes < vram8GiB) {
            config.context_limit = std::min(config.context_limit, ContextLimits::TINY);
            config.use_kv_quantization = true;
            config.enable_kv_offload = true;
        } else if (availableVramBytes < vram16GiB) {
            config.context_limit = std::min(config.context_limit, ContextLimits::STANDARD);
            config.use_kv_quantization = true;
        }
    }

    const ContextDecision decision = ResolveContextDecision(config.context_limit, availableVramBytes);
    config.context_limit = decision.effective;
    if (config.context_limit >= ContextLimits::LARGE) {
        config.enable_kv_offload = true;
    }

    return config;
}

} // namespace RawrXD
