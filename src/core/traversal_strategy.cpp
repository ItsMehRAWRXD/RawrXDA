// ============================================================================
// traversal_strategy.cpp — Adaptive Tensor Traversal Strategy Implementation
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "traversal_strategy.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstring>

// ============================================================================
// Singleton
// ============================================================================

TraversalStrategy& TraversalStrategy::instance() {
    static TraversalStrategy s_instance;
    return s_instance;
}

TraversalStrategy::TraversalStrategy()
    : m_currentMode(TraversalMode::BunnyHop)
    , m_currentSkipRatio(0.3f)
    , m_passCounter(0)
    , m_customFn(nullptr)
    , m_customUserData(nullptr)
{
    std::memset(m_history, 0, sizeof(m_history));
}

TraversalStrategy::~TraversalStrategy() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

PatchResult TraversalStrategy::initialize(const TraversalStrategyConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("TraversalStrategy already initialized", -1);
    }

    m_config          = config;
    m_currentMode     = config.initialMode;
    m_currentSkipRatio = config.initialSkipRatio;
    m_passCounter     = 0;

    // Initialize per-layer priorities
    m_layerPriorities.resize(config.totalLayers, LayerPriority::Unknown);

    // Mark first and last layers as critical
    for (uint32_t i = 0; i < config.keepFirstLayers && i < config.totalLayers; ++i) {
        m_layerPriorities[i] = LayerPriority::Critical;
    }
    for (uint32_t i = 0; i < config.keepLastLayers && i < config.totalLayers; ++i) {
        uint32_t idx = config.totalLayers - 1 - i;
        m_layerPriorities[idx] = LayerPriority::Critical;
    }

    m_activeClamps.clear();
    m_customFn       = nullptr;
    m_customUserData = nullptr;
    m_historyHead.store(0, std::memory_order_release);

    m_initialized.store(true, std::memory_order_release);
    return PatchResult::ok("TraversalStrategy initialized");
}

void TraversalStrategy::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized.store(false, std::memory_order_release);
    m_layerPriorities.clear();
    m_activeClamps.clear();
    m_customFn       = nullptr;
    m_customUserData = nullptr;
}

// ============================================================================
// Plan Generation
// ============================================================================

PatchResult TraversalStrategy::generatePlan(const HardwareFeedback& feedback,
                                             TraversalPlan* outPlan) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("TraversalStrategy not initialized", -1);
    }
    if (!outPlan) {
        return PatchResult::error("Null output plan pointer", -2);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // First, adapt strategy based on feedback if enabled
    if (m_config.adaptiveEnabled) {
        // Check for high latency
        if (feedback.latencyMs > m_config.maxAcceptableLatencyMs) {
            adaptForHighLatency(feedback);
        }
        // Check for low TPS
        if (feedback.tokensPerSecond > 0 && feedback.tokensPerSecond < m_config.minAcceptableTPS) {
            adaptForLowTPS(feedback);
        }
        // Check for memory pressure
        if (feedback.memoryPressure() > m_config.memoryPressureThreshold) {
            adaptForMemoryPressure(feedback);
        }
        // Check for decode failures
        if (feedback.decodeFailCount > 0 && feedback.successRate() < 0.5) {
            adaptForDecodeFail(feedback);
        }
    }

    // Generate plan based on current mode
    switch (m_currentMode) {
        case TraversalMode::Full:          generateFullPlan(outPlan); break;
        case TraversalMode::SkipUniform:   generateSkipUniformPlan(outPlan); break;
        case TraversalMode::SkipAdaptive:  generateSkipAdaptivePlan(outPlan); break;
        case TraversalMode::DepthFirst:    generateDepthFirstPlan(outPlan); break;
        case TraversalMode::BreadthFirst:  generateBreadthFirstPlan(outPlan); break;
        case TraversalMode::BunnyHop:      generateBunnyHopPlan(outPlan); break;
        case TraversalMode::IterDeepening: generateIterDeepeningPlan(outPlan); break;
        case TraversalMode::Custom: {
            if (m_customFn) {
                m_customFn(&feedback, nullptr, outPlan, m_customUserData);
            } else {
                generateBunnyHopPlan(outPlan);
            }
            break;
        }
        default:
            generateBunnyHopPlan(outPlan);
            break;
    }

    // Apply clamps to the plan
    applyClampsToPlan(outPlan);

    outPlan->mode       = m_currentMode;
    outPlan->passNumber = m_passCounter++;

    // Estimate cost ratio
    outPlan->estimatedCostRatio = outPlan->activeLayerRatio();

    m_stats.plansGenerated.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Plan generated");
}

PatchResult TraversalStrategy::generateInitialPlan(TraversalPlan* outPlan) {
    HardwareFeedback empty;
    std::memset(&empty, 0, sizeof(empty));
    empty.tokensPerSecond = 10.0;  // Assume reasonable defaults
    empty.latencyMs       = 100.0;
    return generatePlan(empty, outPlan);
}

// ============================================================================
// Internal Plan Generators
// ============================================================================

void TraversalStrategy::generateFullPlan(TraversalPlan* out) {
    out->layerMask.assign(m_config.totalLayers, true);
    out->contextLength = m_config.maxContextLength;
    out->tokenBudget   = 512;
    out->batchSize     = 1;
}

void TraversalStrategy::generateSkipUniformPlan(TraversalPlan* out) {
    uint32_t totalLayers = m_config.totalLayers;
    out->layerMask.resize(totalLayers, false);

    uint32_t stride = static_cast<uint32_t>(1.0f / (1.0f - m_currentSkipRatio + 0.001f));
    if (stride < 1) stride = 1;

    for (uint32_t i = 0; i < totalLayers; ++i) {
        // Always include protected layers
        if (m_layerPriorities[i] == LayerPriority::Critical) {
            out->layerMask[i] = true;
            continue;
        }
        // Include every Nth layer
        out->layerMask[i] = (i % stride == 0);
    }

    // Scale context by active ratio
    float ratio = out->activeLayerRatio();
    out->contextLength = static_cast<uint32_t>(m_config.maxContextLength * ratio);
    if (out->contextLength < m_config.minContextLength) {
        out->contextLength = m_config.minContextLength;
    }
    out->tokenBudget = static_cast<uint32_t>(256 * ratio + 64);
    out->batchSize   = 1;

    m_stats.layerSkips.fetch_add(totalLayers - out->activeLayerCount(),
                                  std::memory_order_relaxed);
}

void TraversalStrategy::generateSkipAdaptivePlan(TraversalPlan* out) {
    uint32_t totalLayers = m_config.totalLayers;
    out->layerMask.resize(totalLayers, false);

    for (uint32_t i = 0; i < totalLayers; ++i) {
        LayerPriority prio = m_layerPriorities[i];
        switch (prio) {
            case LayerPriority::Critical:
            case LayerPriority::High:
                out->layerMask[i] = true;
                break;
            case LayerPriority::Medium:
                out->layerMask[i] = (m_currentSkipRatio < 0.5f);
                break;
            case LayerPriority::Low:
                out->layerMask[i] = (m_currentSkipRatio < 0.2f);
                break;
            case LayerPriority::Negligible:
                out->layerMask[i] = false;
                break;
            case LayerPriority::Unknown:
            default:
                // Unknown layers: include in early passes, skip later
                out->layerMask[i] = (m_passCounter < 3);
                break;
        }
    }

    float ratio = out->activeLayerRatio();
    out->contextLength = static_cast<uint32_t>(m_config.maxContextLength * ratio);
    if (out->contextLength < m_config.minContextLength) {
        out->contextLength = m_config.minContextLength;
    }
    out->tokenBudget = static_cast<uint32_t>(256 * ratio + 64);
    out->batchSize   = 1;

    m_stats.layerSkips.fetch_add(totalLayers - out->activeLayerCount(),
                                  std::memory_order_relaxed);
}

void TraversalStrategy::generateDepthFirstPlan(TraversalPlan* out) {
    // Deep traversal: early layers get priority, diminishing toward the end
    uint32_t totalLayers = m_config.totalLayers;
    out->layerMask.resize(totalLayers, false);

    // Divide into quartiles
    uint32_t q1 = totalLayers / 4;
    uint32_t q2 = totalLayers / 2;
    uint32_t q3 = 3 * totalLayers / 4;

    for (uint32_t i = 0; i < totalLayers; ++i) {
        if (m_layerPriorities[i] == LayerPriority::Critical) {
            out->layerMask[i] = true;
        } else if (i < q1) {
            out->layerMask[i] = true;              // All early layers
        } else if (i < q2) {
            out->layerMask[i] = (i % 2 == 0);      // Every other middle-early
        } else if (i < q3) {
            out->layerMask[i] = (i % 3 == 0);      // Every 3rd middle-late
        } else {
            out->layerMask[i] = (i % 4 == 0);      // Every 4th late layer
        }
    }

    out->contextLength = m_config.maxContextLength;
    out->tokenBudget   = 256;
    out->batchSize     = 1;
}

void TraversalStrategy::generateBreadthFirstPlan(TraversalPlan* out) {
    // Broad coverage with minimal depth: sample evenly across all layers
    uint32_t totalLayers = m_config.totalLayers;
    out->layerMask.resize(totalLayers, false);

    // Target: ~25% of layers, evenly distributed
    uint32_t targetCount = totalLayers / 4;
    if (targetCount < 4) targetCount = 4;
    uint32_t stride = totalLayers / targetCount;
    if (stride < 1) stride = 1;

    for (uint32_t i = 0; i < totalLayers; ++i) {
        if (m_layerPriorities[i] == LayerPriority::Critical) {
            out->layerMask[i] = true;
        } else {
            out->layerMask[i] = (i % stride == 0);
        }
    }

    // Shorter context but broader layer coverage
    out->contextLength = m_config.maxContextLength / 2;
    if (out->contextLength < m_config.minContextLength) {
        out->contextLength = m_config.minContextLength;
    }
    out->tokenBudget = 128;
    out->batchSize   = 1;
}

void TraversalStrategy::generateBunnyHopPlan(TraversalPlan* out) {
    // The signature traversal mode:
    // Keep first N layers, keep last M layers, hop through the middle
    uint32_t totalLayers = m_config.totalLayers;
    out->layerMask.resize(totalLayers, false);

    uint32_t keepFirst = m_config.keepFirstLayers;
    uint32_t keepLast  = m_config.keepLastLayers;
    uint32_t stride    = m_config.hopStride;
    if (stride < 1) stride = 1;

    for (uint32_t i = 0; i < totalLayers; ++i) {
        // Always keep critical layers
        if (m_layerPriorities[i] == LayerPriority::Critical) {
            out->layerMask[i] = true;
            continue;
        }

        // Keep first N layers
        if (i < keepFirst) {
            out->layerMask[i] = true;
            continue;
        }

        // Keep last M layers
        if (i >= totalLayers - keepLast) {
            out->layerMask[i] = true;
            continue;
        }

        // Middle: hop pattern based on stride
        uint32_t middleIndex = i - keepFirst;
        out->layerMask[i] = (middleIndex % stride == 0);
    }

    // Scale context based on active ratio and pass number
    float ratio = out->activeLayerRatio();
    uint32_t baseCtx = static_cast<uint32_t>(m_config.maxContextLength * ratio);
    if (baseCtx < m_config.minContextLength) {
        baseCtx = m_config.minContextLength;
    }
    out->contextLength = baseCtx;
    out->tokenBudget   = static_cast<uint32_t>(256 * ratio + 64);
    out->batchSize     = 1;

    m_stats.layerSkips.fetch_add(totalLayers - out->activeLayerCount(),
                                  std::memory_order_relaxed);
}

void TraversalStrategy::generateIterDeepeningPlan(TraversalPlan* out) {
    // Iterative deepening: each pass includes more layers than the last
    uint32_t totalLayers = m_config.totalLayers;
    out->layerMask.resize(totalLayers, false);

    // Pass 0: ~10% of layers
    // Pass 1: ~25%
    // Pass 2: ~50%
    // Pass 3: ~75%
    // Pass 4+: ~100%
    float coverageTarget;
    if (m_passCounter == 0)       coverageTarget = 0.10f;
    else if (m_passCounter == 1)  coverageTarget = 0.25f;
    else if (m_passCounter == 2)  coverageTarget = 0.50f;
    else if (m_passCounter == 3)  coverageTarget = 0.75f;
    else                          coverageTarget = 1.0f;

    // Adjust by skip ratio
    coverageTarget *= (1.0f - m_currentSkipRatio);
    if (coverageTarget > 1.0f) coverageTarget = 1.0f;
    if (coverageTarget < 0.05f) coverageTarget = 0.05f;

    uint32_t targetLayers = static_cast<uint32_t>(totalLayers * coverageTarget);
    if (targetLayers < 2) targetLayers = 2;

    // Distribute evenly, biased toward early layers
    uint32_t assigned = 0;

    // Always include critical layers first
    for (uint32_t i = 0; i < totalLayers && assigned < targetLayers; ++i) {
        if (m_layerPriorities[i] == LayerPriority::Critical) {
            out->layerMask[i] = true;
            ++assigned;
        }
    }

    // Then fill remaining slots evenly
    if (assigned < targetLayers) {
        uint32_t remaining = targetLayers - assigned;
        uint32_t stride = totalLayers / (remaining + 1);
        if (stride < 1) stride = 1;

        for (uint32_t i = 0; i < totalLayers && assigned < targetLayers; ++i) {
            if (!out->layerMask[i] && (i % stride == 0)) {
                out->layerMask[i] = true;
                ++assigned;
            }
        }
    }

    float ratio = out->activeLayerRatio();
    out->contextLength = static_cast<uint32_t>(m_config.maxContextLength * coverageTarget);
    if (out->contextLength < m_config.minContextLength) {
        out->contextLength = m_config.minContextLength;
    }
    out->tokenBudget = static_cast<uint32_t>(128 + 256 * coverageTarget);
    out->batchSize   = 1;
}

void TraversalStrategy::applyClampsToPlan(TraversalPlan* out) {
    out->clamps.clear();
    for (const auto& clamp : m_activeClamps) {
        if (clamp.enabled) {
            out->clamps.push_back(clamp);
            m_stats.clampActivations.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

// ============================================================================
// Adaptation
// ============================================================================

PatchResult TraversalStrategy::adapt(const HardwareFeedback& feedback) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_config.adaptiveEnabled) {
        return PatchResult::ok("Adaptation disabled, no changes");
    }

    uint32_t adaptCount = m_stats.adaptations.load(std::memory_order_relaxed);
    if (adaptCount >= m_config.maxAdaptations) {
        return PatchResult::ok("Max adaptations reached, locked");
    }

    if (feedback.latencyMs > m_config.maxAcceptableLatencyMs) {
        adaptForHighLatency(feedback);
    } else if (feedback.tokensPerSecond > 0 && feedback.tokensPerSecond < m_config.minAcceptableTPS) {
        adaptForLowTPS(feedback);
    } else if (feedback.memoryPressure() > m_config.memoryPressureThreshold) {
        adaptForMemoryPressure(feedback);
    }

    return PatchResult::ok("Adaptation applied");
}

PatchResult TraversalStrategy::setMode(TraversalMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    TraversalMode old = m_currentMode;
    m_currentMode = mode;
    recordAdaptation(AdaptationReason::None, old, mode,
                      m_currentSkipRatio, m_currentSkipRatio, "Manual mode change");
    m_stats.modeChanges.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Mode set");
}

PatchResult TraversalStrategy::setSkipRatio(float ratio) {
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentSkipRatio = ratio;
    return PatchResult::ok("Skip ratio set");
}

// ============================================================================
// Adaptation Logic
// ============================================================================

void TraversalStrategy::adaptForHighLatency(const HardwareFeedback& fb) {
    TraversalMode oldMode = m_currentMode;
    float oldSkip = m_currentSkipRatio;

    // Increase skip ratio to reduce load
    m_currentSkipRatio = std::min(m_currentSkipRatio + 0.1f, 0.9f);

    // If already skipping a lot and still slow, switch to BunnyHop
    if (m_currentSkipRatio > 0.6f && m_currentMode != TraversalMode::BunnyHop) {
        m_currentMode = TraversalMode::BunnyHop;
    }

    recordAdaptation(AdaptationReason::HighLatency, oldMode, m_currentMode,
                      oldSkip, m_currentSkipRatio, "High latency detected, increasing skip ratio");
    m_stats.adaptations.fetch_add(1, std::memory_order_relaxed);
}

void TraversalStrategy::adaptForLowTPS(const HardwareFeedback& fb) {
    TraversalMode oldMode = m_currentMode;
    float oldSkip = m_currentSkipRatio;

    // Reduce context and increase skipping
    m_currentSkipRatio = std::min(m_currentSkipRatio + 0.15f, 0.9f);

    // Switch to BunnyHop if not already
    if (m_currentMode == TraversalMode::Full || m_currentMode == TraversalMode::DepthFirst) {
        m_currentMode = TraversalMode::BunnyHop;
    }

    recordAdaptation(AdaptationReason::LowTPS, oldMode, m_currentMode,
                      oldSkip, m_currentSkipRatio, "Low TPS detected, reducing coverage");
    m_stats.adaptations.fetch_add(1, std::memory_order_relaxed);
}

void TraversalStrategy::adaptForMemoryPressure(const HardwareFeedback& fb) {
    TraversalMode oldMode = m_currentMode;
    float oldSkip = m_currentSkipRatio;

    // Aggressively reduce coverage
    m_currentSkipRatio = std::min(m_currentSkipRatio + 0.2f, 0.95f);

    // Switch to lightest mode
    if (m_currentMode != TraversalMode::BreadthFirst) {
        m_currentMode = TraversalMode::BreadthFirst;
    }

    // Add context length clamp
    ParameterClamp ctxClamp = ParameterClamp::make(
        ClampTarget::ContextLength,
        static_cast<float>(m_config.minContextLength),
        static_cast<float>(m_config.maxContextLength / 4),
        static_cast<float>(m_config.minContextLength)
    );
    m_activeClamps.push_back(ctxClamp);

    recordAdaptation(AdaptationReason::MemoryPressure, oldMode, m_currentMode,
                      oldSkip, m_currentSkipRatio, "Memory pressure, aggressive reduction");
    m_stats.adaptations.fetch_add(1, std::memory_order_relaxed);
}

void TraversalStrategy::adaptForDecodeFail(const HardwareFeedback& fb) {
    TraversalMode oldMode = m_currentMode;
    float oldSkip = m_currentSkipRatio;

    // Back off: reduce skip ratio to include more layers (failure may be from skipping)
    m_currentSkipRatio = std::max(m_currentSkipRatio - 0.1f, 0.0f);

    // But reduce context to compensate
    ParameterClamp ctxClamp = ParameterClamp::make(
        ClampTarget::ContextLength,
        static_cast<float>(m_config.minContextLength),
        static_cast<float>(m_config.maxContextLength / 2),
        static_cast<float>(m_config.minContextLength)
    );
    m_activeClamps.push_back(ctxClamp);

    // Clamp temperature down for stability
    ParameterClamp tempClamp = ParameterClamp::make(
        ClampTarget::Temperature, 0.1f, 0.7f, 0.5f
    );
    m_activeClamps.push_back(tempClamp);

    recordAdaptation(AdaptationReason::DecodeFailed, oldMode, m_currentMode,
                      oldSkip, m_currentSkipRatio, "Decode failures, reducing skip and context");
    m_stats.adaptations.fetch_add(1, std::memory_order_relaxed);
}

void TraversalStrategy::recordAdaptation(AdaptationReason reason, TraversalMode oldMode,
                                           TraversalMode newMode, float oldSkip, float newSkip,
                                           const char* desc) {
    uint32_t idx = m_historyHead.fetch_add(1, std::memory_order_relaxed) & HISTORY_MASK;
    StrategyAdaptation& entry = m_history[idx];
    entry.reason       = reason;
    entry.oldMode      = oldMode;
    entry.newMode      = newMode;
    entry.oldSkipRatio = oldSkip;
    entry.newSkipRatio = newSkip;
    entry.passNumber   = m_passCounter;
    entry.timestampMs  = GetTickCount64();
    entry.description  = desc;
}

// ============================================================================
// Layer Priority
// ============================================================================

PatchResult TraversalStrategy::setLayerPriority(uint32_t layerIndex, LayerPriority priority) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (layerIndex >= m_layerPriorities.size()) {
        return PatchResult::error("Layer index out of range", -2);
    }
    m_layerPriorities[layerIndex] = priority;
    return PatchResult::ok("Layer priority set");
}

PatchResult TraversalStrategy::setLayerPrioritiesFromScores(const float* scores, uint32_t count) {
    if (!scores) return PatchResult::error("Null scores pointer", -2);

    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t limit = std::min(count, static_cast<uint32_t>(m_layerPriorities.size()));

    for (uint32_t i = 0; i < limit; ++i) {
        float s = scores[i];
        if (s >= 0.8f)      m_layerPriorities[i] = LayerPriority::Critical;
        else if (s >= 0.6f) m_layerPriorities[i] = LayerPriority::High;
        else if (s >= 0.4f) m_layerPriorities[i] = LayerPriority::Medium;
        else if (s >= 0.15f) m_layerPriorities[i] = LayerPriority::Low;
        else                m_layerPriorities[i] = LayerPriority::Negligible;
    }

    return PatchResult::ok("Layer priorities set from scores");
}

LayerPriority TraversalStrategy::getLayerPriority(uint32_t layerIndex) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (layerIndex >= m_layerPriorities.size()) {
        return LayerPriority::Unknown;
    }
    return m_layerPriorities[layerIndex];
}

// ============================================================================
// Custom Strategy
// ============================================================================

PatchResult TraversalStrategy::setCustomStrategy(CustomStrategyFn fn, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_customFn       = fn;
    m_customUserData = userData;
    return PatchResult::ok("Custom strategy set");
}

PatchResult TraversalStrategy::clearCustomStrategy() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_customFn       = nullptr;
    m_customUserData = nullptr;
    return PatchResult::ok("Custom strategy cleared");
}

// ============================================================================
// Parameter Clamps
// ============================================================================

PatchResult TraversalStrategy::setClamp(const ParameterClamp& clamp) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Replace existing clamp for same target, or add new
    for (auto& c : m_activeClamps) {
        if (c.target == clamp.target) {
            c = clamp;
            return PatchResult::ok("Clamp updated");
        }
    }
    m_activeClamps.push_back(clamp);
    return PatchResult::ok("Clamp added");
}

PatchResult TraversalStrategy::removeClamp(ClampTarget target) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::remove_if(m_activeClamps.begin(), m_activeClamps.end(),
        [target](const ParameterClamp& c) { return c.target == target; });
    if (it != m_activeClamps.end()) {
        m_activeClamps.erase(it, m_activeClamps.end());
        return PatchResult::ok("Clamp removed");
    }
    return PatchResult::ok("Clamp not found (no-op)");
}

PatchResult TraversalStrategy::clearClamps() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeClamps.clear();
    return PatchResult::ok("All clamps cleared");
}

// ============================================================================
// Queries
// ============================================================================

TraversalMode TraversalStrategy::currentMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentMode;
}

float TraversalStrategy::currentSkipRatio() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentSkipRatio;
}

uint32_t TraversalStrategy::adaptationCount() const {
    return m_stats.adaptations.load(std::memory_order_relaxed);
}

uint32_t TraversalStrategy::getAdaptationHistory(StrategyAdaptation* outBuf,
                                                   uint32_t maxCount) const {
    if (!outBuf || maxCount == 0) return 0;
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t head = m_historyHead.load(std::memory_order_relaxed);
    uint32_t available = std::min(head, static_cast<uint32_t>(HISTORY_SIZE));
    uint32_t count = std::min(available, maxCount);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t idx = (head - 1 - i) & HISTORY_MASK;
        outBuf[i] = m_history[idx];
    }
    return count;
}

void TraversalStrategy::resetStats() {
    m_stats.plansGenerated.store(0, std::memory_order_relaxed);
    m_stats.adaptations.store(0, std::memory_order_relaxed);
    m_stats.layerSkips.store(0, std::memory_order_relaxed);
    m_stats.clampActivations.store(0, std::memory_order_relaxed);
    m_stats.modeChanges.store(0, std::memory_order_relaxed);
}
