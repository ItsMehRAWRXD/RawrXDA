// ============================================================================
// reasoning_profile.cpp — Tunable Reasoning Pipeline Profile System
// ============================================================================
//
// Profile management, presets, serialization, self-tuning PID controller,
// thermal monitoring, and input classification.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../include/reasoning_profile.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <fstream>
#include <cctype>
#include <regex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// ============================================================================
// Built-in Preset Definitions
// ============================================================================
namespace {

// Greeting detector patterns
static const char* kGreetingPatterns[] = {
    "hi", "hello", "hey", "yo", "sup", "greetings", "howdy",
    "good morning", "good afternoon", "good evening", "gm", "gn",
    "what's up", "whats up", "wassup", "hiya", "heya", nullptr
};

bool isGreetingLike(const std::string& input) {
    std::string lower;
    lower.resize(input.size());
    std::transform(input.begin(), input.end(), lower.begin(), ::tolower);
    // Trim whitespace
    size_t start = lower.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return true; // empty → trivial
    size_t end = lower.find_last_not_of(" \t\r\n");
    lower = lower.substr(start, end - start + 1);

    // Remove trailing punctuation
    while (!lower.empty() && (lower.back() == '!' || lower.back() == '?' || lower.back() == '.'))
        lower.pop_back();

    for (int i = 0; kGreetingPatterns[i]; ++i) {
        if (lower == kGreetingPatterns[i]) return true;
    }
    return false;
}

// Count rough "complexity indicators" in text
int countComplexityIndicators(const std::string& text) {
    int score = 0;
    std::string lower;
    lower.resize(text.size());
    std::transform(text.begin(), text.end(), lower.begin(), ::tolower);

    // Length-based score
    if (text.size() > 500) score += 2;
    else if (text.size() > 200) score += 1;

    // Code indicators
    if (lower.find("```") != std::string::npos) score += 2;
    if (lower.find("function") != std::string::npos) score += 1;
    if (lower.find("class ") != std::string::npos) score += 1;
    if (lower.find("implement") != std::string::npos) score += 1;
    if (lower.find("architecture") != std::string::npos) score += 2;
    if (lower.find("design") != std::string::npos) score += 1;
    if (lower.find("refactor") != std::string::npos) score += 1;
    if (lower.find("optimize") != std::string::npos) score += 1;
    if (lower.find("debug") != std::string::npos) score += 1;
    if (lower.find("explain") != std::string::npos) score += 1;

    // Multi-part indicators
    size_t questionMarks = std::count(text.begin(), text.end(), '?');
    if (questionMarks > 2) score += 2;
    if (questionMarks > 0) score += 1;

    // Numbered lists suggest multi-part
    if (lower.find("1.") != std::string::npos &&
        lower.find("2.") != std::string::npos) score += 2;

    // "and" / "also" / "additionally" suggest compound requests
    if (lower.find(" and ") != std::string::npos) score += 1;
    if (lower.find("also") != std::string::npos) score += 1;
    if (lower.find("additionally") != std::string::npos) score += 1;

    return score;
}

// Build all preset profiles
std::vector<ReasoningProfile> buildAllPresets() {
    std::vector<ReasoningProfile> presets;

    // --- FAST ---
    {
        ReasoningProfile p;
        p.name = "fast";
        p.reasoning.reasoningDepth = 0;
        p.reasoning.mode = ReasoningMode::Fast;
        p.reasoning.visibility = ReasoningVisibility::FinalOnly;
        p.reasoning.enableCritic = false;
        p.reasoning.enableAuditor = false;
        p.reasoning.enableThinker = false;
        p.reasoning.enableResearcher = false;
        p.reasoning.enableDebaters = false;
        p.reasoning.enableVerifier = false;
        p.reasoning.enableRefiner = false;
        p.reasoning.enableSynthesizer = false;
        p.reasoning.enableBrainstorm = false;
        p.reasoning.enableSummarizer = false;
        p.reasoning.autoBypassSimple = true;
        p.reasoning.exposeChainOfThought = false;
        presets.push_back(p);
    }

    // --- NORMAL ---
    {
        ReasoningProfile p;
        p.name = "normal";
        p.reasoning.reasoningDepth = 1;
        p.reasoning.mode = ReasoningMode::Normal;
        p.reasoning.visibility = ReasoningVisibility::ProgressBar;
        p.reasoning.enableThinker = true;
        p.reasoning.enableSynthesizer = true;
        presets.push_back(p);
    }

    // --- DEEP ---
    {
        ReasoningProfile p;
        p.name = "deep";
        p.reasoning.reasoningDepth = 3;
        p.reasoning.mode = ReasoningMode::Deep;
        p.reasoning.visibility = ReasoningVisibility::StepSummary;
        p.reasoning.enableCritic = true;
        p.reasoning.enableThinker = true;
        p.reasoning.enableRefiner = true;
        p.reasoning.enableSynthesizer = true;
        p.reasoning.enableVerifier = true;
        presets.push_back(p);
    }

    // --- CRITICAL ---
    {
        ReasoningProfile p;
        p.name = "critical";
        p.reasoning.reasoningDepth = 5;
        p.reasoning.mode = ReasoningMode::Critical;
        p.reasoning.visibility = ReasoningVisibility::StepSummary;
        p.reasoning.enableCritic = true;
        p.reasoning.enableAuditor = true;
        p.reasoning.enableThinker = true;
        p.reasoning.enableResearcher = true;
        p.reasoning.enableDebaters = true;
        p.reasoning.enableVerifier = true;
        p.reasoning.enableRefiner = true;
        p.reasoning.enableSynthesizer = true;
        p.reasoning.confidenceThreshold = 0.8f;
        p.reasoning.qualityFloor = 0.5f;
        presets.push_back(p);
    }

    // --- SWARM ---
    {
        ReasoningProfile p;
        p.name = "swarm";
        p.reasoning.reasoningDepth = 3;
        p.reasoning.mode = ReasoningMode::Swarm;
        p.reasoning.visibility = ReasoningVisibility::StepSummary;
        p.reasoning.enableSynthesizer = true;
        p.swarm.enabled = true;
        p.swarm.agentCount = 5;
        p.swarm.mode = SwarmReasoningMode::ParallelVote;
        p.swarm.voteThreshold = 0.6f;
        presets.push_back(p);
    }

    // --- ADAPTIVE ---
    {
        ReasoningProfile p;
        p.name = "adaptive";
        p.reasoning.reasoningDepth = 2;
        p.reasoning.mode = ReasoningMode::Adaptive;
        p.reasoning.visibility = ReasoningVisibility::ProgressBar;
        p.reasoning.enableCritic = true;
        p.reasoning.enableThinker = true;
        p.reasoning.enableSynthesizer = true;
        p.adaptive.enabled = true;
        p.adaptive.strategy = AdaptiveStrategy::Hybrid;
        p.adaptive.latencyTargetMs = 3000.0;
        p.adaptive.latencyMaxMs = 8000.0;
        p.thermal.enabled = true;
        p.selfTune.enabled = true;
        p.selfTune.objective = SelfTuneObjective::BalancedQoS;
        presets.push_back(p);
    }

    // --- DEV/DEBUG ---
    {
        ReasoningProfile p;
        p.name = "dev";
        p.reasoning.reasoningDepth = 4;
        p.reasoning.mode = ReasoningMode::DevDebug;
        p.reasoning.visibility = ReasoningVisibility::FullCoT;
        p.reasoning.enableCritic = true;
        p.reasoning.enableAuditor = true;
        p.reasoning.enableThinker = true;
        p.reasoning.enableResearcher = true;
        p.reasoning.enableVerifier = true;
        p.reasoning.enableRefiner = true;
        p.reasoning.enableSynthesizer = true;
        p.reasoning.enableBrainstorm = true;
        p.reasoning.enableSummarizer = true;
        p.reasoning.autoBypassSimple = false;
        p.reasoning.exposeChainOfThought = true;
        p.reasoning.exposeStepTimings = true;
        p.reasoning.exposeConfidence = true;
        p.adaptive.enabled = false;
        presets.push_back(p);
    }

    // --- MAX (all agents, full depth, swarm + adaptive) ---
    {
        ReasoningProfile p;
        p.name = "max";
        p.reasoning.reasoningDepth = 8;
        p.reasoning.mode = ReasoningMode::Critical;
        p.reasoning.visibility = ReasoningVisibility::StepSummary;
        p.reasoning.enableCritic = true;
        p.reasoning.enableAuditor = true;
        p.reasoning.enableThinker = true;
        p.reasoning.enableResearcher = true;
        p.reasoning.enableDebaters = true;
        p.reasoning.enableVerifier = true;
        p.reasoning.enableRefiner = true;
        p.reasoning.enableSynthesizer = true;
        p.reasoning.enableBrainstorm = true;
        p.reasoning.enableSummarizer = true;
        p.reasoning.confidenceThreshold = 0.9f;
        p.reasoning.qualityFloor = 0.6f;
        p.swarm.enabled = true;
        p.swarm.agentCount = 8;
        p.swarm.mode = SwarmReasoningMode::Tournament;
        p.swarm.tournamentRounds = 3;
        p.adaptive.enabled = true;
        p.adaptive.strategy = AdaptiveStrategy::ConfidenceBased;
        p.adaptive.maxAdaptiveDepth = 8;
        p.thermal.enabled = true;
        p.selfTune.enabled = true;
        p.selfTune.objective = SelfTuneObjective::MaxQuality;
        presets.push_back(p);
    }

    return presets;
}

const std::vector<ReasoningProfile>& getPresetTable() {
    static std::vector<ReasoningProfile> presets = buildAllPresets();
    return presets;
}

} // anonymous namespace

// ============================================================================
// Public Preset API
// ============================================================================

const ReasoningProfile* getReasoningPreset(const char* name) {
    if (!name) return nullptr;
    const auto& presets = getPresetTable();
    for (const auto& p : presets) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

std::vector<std::string> getReasoningPresetNames() {
    std::vector<std::string> names;
    const auto& presets = getPresetTable();
    for (const auto& p : presets) {
        names.push_back(p.name);
    }
    return names;
}

// ============================================================================
// ReasoningProfileManager — Singleton
// ============================================================================

ReasoningProfileManager::ReasoningProfileManager() {
    // Default to "normal" preset
    const ReasoningProfile* normal = getReasoningPreset("normal");
    if (normal) {
        m_profile = *normal;
        m_activePresetName = "normal";
    } else {
        m_profile.name = "default";
        m_activePresetName = "default";
    }
    memset(&m_stats, 0, sizeof(m_stats));
}

ReasoningProfileManager& ReasoningProfileManager::instance() {
    static ReasoningProfileManager mgr;
    return mgr;
}

// ============================================================================
// Profile Management
// ============================================================================

void ReasoningProfileManager::setProfile(const ReasoningProfile& profile) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ReasoningProfile old = m_profile;
    m_profile = profile;
    m_activePresetName = profile.name;

    if (m_profileChangeCb) {
        m_profileChangeCb(m_profile, m_profileChangeCtx);
    }
}

ReasoningProfile ReasoningProfileManager::getProfile() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profile;
}

void ReasoningProfileManager::applyPreset(const char* presetName) {
    const ReasoningProfile* p = getReasoningPreset(presetName);
    if (p) {
        setProfile(*p);
    }
}

std::string ReasoningProfileManager::getActivePresetName() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activePresetName;
}

// ============================================================================
// Quick Adjusters — "The P Dial"
// ============================================================================

void ReasoningProfileManager::setReasoningDepth(int depth) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.reasoning.reasoningDepth = (std::max)(0, (std::min)(depth, 8));
    m_activePresetName = "custom";
}

int ReasoningProfileManager::getReasoningDepth() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profile.reasoning.reasoningDepth;
}

void ReasoningProfileManager::setMode(ReasoningMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.reasoning.mode = mode;
    m_activePresetName = "custom";
}

ReasoningMode ReasoningProfileManager::getMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profile.reasoning.mode;
}

void ReasoningProfileManager::setVisibility(ReasoningVisibility vis) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.reasoning.visibility = vis;
}

ReasoningVisibility ReasoningProfileManager::getVisibility() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profile.reasoning.visibility;
}

// ============================================================================
// Adaptive Controls
// ============================================================================

void ReasoningProfileManager::setAdaptiveEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.adaptive.enabled = enabled;
}

bool ReasoningProfileManager::isAdaptiveEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profile.adaptive.enabled;
}

void ReasoningProfileManager::setAdaptiveStrategy(AdaptiveStrategy strategy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.adaptive.strategy = strategy;
}

void ReasoningProfileManager::setLatencyTarget(double ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.adaptive.latencyTargetMs = (std::max)(100.0, ms);
}

void ReasoningProfileManager::setLatencyMax(double ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.adaptive.latencyMaxMs = (std::max)(m_profile.adaptive.latencyTargetMs + 1.0, ms);
}

// ============================================================================
// Thermal Controls
// ============================================================================

void ReasoningProfileManager::setThermalEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.thermal.enabled = enabled;
}

bool ReasoningProfileManager::isThermalEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profile.thermal.enabled;
}

void ReasoningProfileManager::updateThermalState(ThermalState state) {
    ThermalState old = m_thermalState.exchange(state);
    if (old != state && m_thermalChangeCb) {
        m_thermalChangeCb(state, old, m_thermalChangeCtx);
    }
}

ThermalState ReasoningProfileManager::getThermalState() const {
    return m_thermalState.load(std::memory_order_relaxed);
}

// ============================================================================
// Swarm Controls
// ============================================================================

void ReasoningProfileManager::setSwarmEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.swarm.enabled = enabled;
}

bool ReasoningProfileManager::isSwarmEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profile.swarm.enabled;
}

void ReasoningProfileManager::setSwarmAgentCount(int count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.swarm.agentCount = (std::max)(2, (std::min)(count, 16));
}

void ReasoningProfileManager::setSwarmMode(SwarmReasoningMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.swarm.mode = mode;
}

// ============================================================================
// Self-Tune Controls
// ============================================================================

void ReasoningProfileManager::setSelfTuneEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.selfTune.enabled = enabled;
}

bool ReasoningProfileManager::isSelfTuneEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profile.selfTune.enabled;
}

void ReasoningProfileManager::setSelfTuneObjective(SelfTuneObjective objective) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile.selfTune.objective = objective;
}

SelfTuneState ReasoningProfileManager::getSelfTuneState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_selfTuneState;
}

void ReasoningProfileManager::feedObservation(const SelfTuneObservation& obs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_profile.selfTune.enabled) return;

    m_observations.push_back(obs);

    // Trim observation window
    int windowSize = m_profile.selfTune.windowSize;
    if (windowSize > 0 && (int)m_observations.size() > windowSize * 2) {
        m_observations.erase(m_observations.begin(),
                             m_observations.begin() + (int)m_observations.size() - windowSize);
    }

    runSelfTuneStep(obs);
}

void ReasoningProfileManager::resetSelfTuneState() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_selfTuneState = SelfTuneState();
    m_observations.clear();
}

// ============================================================================
// Self-Tuning PID Controller
// ============================================================================

void ReasoningProfileManager::runSelfTuneStep(const SelfTuneObservation& obs) {
    // Caller must hold m_mutex
    auto& st = m_selfTuneState;
    st.totalObservations++;

    int depth = (std::max)(0, (std::min)(obs.depthUsed, 8));

    // Update EWMA latency
    double alpha_l = m_profile.selfTune.latencyDecayAlpha;
    if (st.totalObservations == 1) {
        st.ewmaLatency = obs.latencyMs;
    } else {
        st.ewmaLatency = alpha_l * obs.latencyMs + (1.0 - alpha_l) * st.ewmaLatency;
    }

    // Update EWMA quality
    double alpha_q = m_profile.selfTune.qualityDecayAlpha;
    if (st.totalObservations == 1) {
        st.ewmaQuality = obs.qualityScore;
    } else {
        st.ewmaQuality = alpha_q * obs.qualityScore + (1.0 - alpha_q) * st.ewmaQuality;
    }

    // Update per-depth maps with running average
    st.depthUsageCount[depth]++;
    uint64_t n = st.depthUsageCount[depth];
    st.depthQualityMap[depth] =
        st.depthQualityMap[depth] * ((float)(n - 1) / (float)n) +
        obs.qualityScore / (float)n;
    st.depthLatencyMap[depth] =
        st.depthLatencyMap[depth] * ((float)(n - 1) / (float)n) +
        (float)obs.latencyMs / (float)n;

    // Only adjust after minimum samples
    if ((int)st.totalObservations < m_profile.selfTune.minSamplesBeforeTune) return;

    // Exploration: occasionally try a random depth
    double exploreRate = m_profile.selfTune.explorationRate;
    bool exploring = false;
    if (exploreRate > 0.0) {
        // Simple LCG-based decision (deterministic per observation count)
        uint64_t hash = st.totalObservations * 6364136223846793005ULL + 1442695040888963407ULL;
        double r = (double)(hash & 0xFFFFFF) / (double)0xFFFFFF;
        if (r < exploreRate) {
            exploring = true;
            st.explorationCount++;
            // Pick a random depth to try next time
            int randomDepth = (int)(hash >> 24) % (m_profile.adaptive.maxAdaptiveDepth + 1);
            st.currentOptimalDepth = (std::max)(m_profile.adaptive.minAdaptiveDepth,
                                                (std::min)(randomDepth, m_profile.adaptive.maxAdaptiveDepth));
            return;
        }
    }

    // PID-like optimization
    int newDepth = pidComputeOptimalDepth();
    int oldDepth = st.currentOptimalDepth;

    if (newDepth != oldDepth) {
        st.currentOptimalDepth = newDepth;
        m_stats.selfTuneAdjustments++;

        if (m_selfTuneCb) {
            m_selfTuneCb(oldDepth, newDepth, "PID self-tune adjustment", m_selfTuneCtx);
        }
    }
}

int ReasoningProfileManager::pidComputeOptimalDepth() const {
    // Caller must hold m_mutex
    const auto& st = m_selfTuneState;
    const auto& params = m_profile.selfTune;
    const auto& adaptive = m_profile.adaptive;

    int bestDepth = st.currentOptimalDepth;
    double bestScore = -1e9;

    int minD = adaptive.minAdaptiveDepth;
    int maxD = adaptive.maxAdaptiveDepth;

    for (int d = minD; d <= maxD; ++d) {
        if (st.depthUsageCount[d] == 0) continue; // No data for this depth

        float quality = st.depthQualityMap[d];
        float latency = st.depthLatencyMap[d];

        double score = 0.0;

        switch (params.objective) {
        case SelfTuneObjective::MinLatency:
            // Minimize latency, quality must be above floor
            if (quality >= m_profile.reasoning.qualityFloor) {
                score = 1000.0 - latency;
            } else {
                score = -1e6;
            }
            break;

        case SelfTuneObjective::MaxQuality:
            // Maximize quality, latency within max
            if (latency <= m_profile.adaptive.latencyMaxMs) {
                score = quality * 1000.0;
            } else {
                score = quality * 1000.0 - (latency - m_profile.adaptive.latencyMaxMs);
            }
            break;

        case SelfTuneObjective::BalancedQoS: {
            // Pareto: weighted combination
            double latencyNorm = 1.0 - (std::min)(latency / m_profile.adaptive.latencyMaxMs, 1.0);
            score = quality * 0.6 + latencyNorm * 0.4;
            break;
        }

        case SelfTuneObjective::MinCost:
            // Minimize depth (proxy for cost), quality must be acceptable
            if (quality >= m_profile.reasoning.qualityFloor) {
                score = 100.0 - d * 10.0;
            } else {
                score = -1e6;
            }
            break;

        case SelfTuneObjective::MaxThroughput:
            // Minimize latency aggressively, accept lower quality
            if (quality >= m_profile.reasoning.qualityFloor * 0.7f) {
                score = 2000.0 - latency * 2.0;
            } else {
                score = -1e6;
            }
            break;

        default:
            score = quality * 500.0 - latency * 0.1;
            break;
        }

        if (score > bestScore) {
            bestScore = score;
            bestDepth = d;
        }
    }

    return (std::max)(minD, (std::min)(bestDepth, maxD));
}

// ============================================================================
// Input Classification
// ============================================================================

InputComplexity ReasoningProfileManager::classifyInput(const std::string& input) const {
    if (input.empty()) return InputComplexity::Trivial;

    // Trim
    size_t start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return InputComplexity::Trivial;
    size_t end = input.find_last_not_of(" \t\r\n");
    std::string trimmed = input.substr(start, end - start + 1);

    // Greeting check
    if (isGreetingLike(trimmed)) return InputComplexity::Trivial;

    // Very short input
    if (trimmed.size() <= 5) return InputComplexity::Trivial;
    if (trimmed.size() <= 20) return InputComplexity::Simple;

    int score = countComplexityIndicators(trimmed);

    if (score >= 8) return InputComplexity::Expert;
    if (score >= 5) return InputComplexity::Complex;
    if (score >= 2) return InputComplexity::Moderate;
    if (score >= 1) return InputComplexity::Simple;
    return InputComplexity::Simple;
}

bool ReasoningProfileManager::shouldBypassPipeline(const std::string& input) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_profile.reasoning.autoBypassSimple) return false;
    if (m_profile.reasoning.mode == ReasoningMode::Fast) return true;
    if (m_profile.reasoning.mode == ReasoningMode::DevDebug) return false; // never bypass in dev

    // Trim
    size_t start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return true;
    size_t end = input.find_last_not_of(" \t\r\n");
    std::string trimmed = input.substr(start, end - start + 1);

    if ((int)trimmed.size() <= m_profile.reasoning.simpleInputMaxLen) {
        if (isGreetingLike(trimmed)) return true;
    }
    return false;
}

int ReasoningProfileManager::computeEffectiveDepth(InputComplexity complexity) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int baseDepth = m_profile.reasoning.reasoningDepth;

    // Adjust by complexity
    switch (complexity) {
    case InputComplexity::Trivial:
        return 0; // Always bypass for trivial
    case InputComplexity::Simple:
        return (std::min)(baseDepth, 1);
    case InputComplexity::Moderate:
        return baseDepth;
    case InputComplexity::Complex:
        return (std::max)(baseDepth, 2);
    case InputComplexity::Expert:
        return (std::max)(baseDepth, 3);
    default:
        return baseDepth;
    }
}

// ============================================================================
// Thermal Monitoring
// ============================================================================

float ReasoningProfileManager::readSystemCpuUsage() const {
    // Windows CPU usage via GetSystemTimes
    static FILETIME prevIdle = {}, prevKernel = {}, prevUser = {};
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) return 0.0f;

    auto toULL = [](FILETIME ft) -> uint64_t {
        return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    };

    uint64_t dIdle   = toULL(idle)   - toULL(prevIdle);
    uint64_t dKernel = toULL(kernel) - toULL(prevKernel);
    uint64_t dUser   = toULL(user)   - toULL(prevUser);

    prevIdle   = idle;
    prevKernel = kernel;
    prevUser   = user;

    uint64_t total = dKernel + dUser;
    if (total == 0) return 0.0f;

    float usage = (1.0f - (float)dIdle / (float)total) * 100.0f;
    return (std::max)(0.0f, (std::min)(usage, 100.0f));
}

ThermalState ReasoningProfileManager::classifyThermal(float cpuUsage) const {
    // Caller must hold m_mutex (for thresholds)
    if (cpuUsage >= m_profile.thermal.criticalThreshold) return ThermalState::Critical;
    if (cpuUsage >= m_profile.thermal.hotThreshold)      return ThermalState::Hot;
    if (cpuUsage >= m_profile.thermal.warmThreshold)     return ThermalState::Warm;
    return ThermalState::Cool;
}

// ============================================================================
// Telemetry
// ============================================================================

ReasoningTelemetry ReasoningProfileManager::getLastTelemetry() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_telemetry.empty()) return {};
    return m_telemetry.back();
}

std::vector<ReasoningTelemetry> ReasoningProfileManager::getRecentTelemetry(int count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int n = (std::min)(count, (int)m_telemetry.size());
    if (n <= 0) return {};
    return std::vector<ReasoningTelemetry>(m_telemetry.end() - n, m_telemetry.end());
}

void ReasoningProfileManager::recordTelemetry(const ReasoningTelemetry& t) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_telemetry.push_back(t);
    if (m_telemetry.size() > kMaxTelemetry) {
        m_telemetry.erase(m_telemetry.begin(),
                          m_telemetry.begin() + (int)(m_telemetry.size() - kMaxTelemetry));
    }
}

// ============================================================================
// Callbacks
// ============================================================================

void ReasoningProfileManager::setProfileChangeCallback(
    ReasoningProfileChangeCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profileChangeCb = cb;
    m_profileChangeCtx = userData;
}

void ReasoningProfileManager::setThermalChangeCallback(
    ThermalStateChangeCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_thermalChangeCb = cb;
    m_thermalChangeCtx = userData;
}

void ReasoningProfileManager::setSelfTuneCallback(
    SelfTuneAdjustCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_selfTuneCb = cb;
    m_selfTuneCtx = userData;
}

void ReasoningProfileManager::setStepProgressCallback(
    ReasoningStepProgressCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stepProgressCb = cb;
    m_stepProgressCtx = userData;
}

// ============================================================================
// Statistics
// ============================================================================

ReasoningProfileManager::ProfileStats ReasoningProfileManager::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void ReasoningProfileManager::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    memset(&m_stats, 0, sizeof(m_stats));
}

// ============================================================================
// Serialization — Manual JSON (no nlohmann dependency in hot path)
// ============================================================================

static void jsonAppend(std::ostringstream& ss, const char* key, int val) {
    ss << "\"" << key << "\":" << val;
}
static void jsonAppend(std::ostringstream& ss, const char* key, double val) {
    ss << "\"" << key << "\":" << val;
}
static void jsonAppend(std::ostringstream& ss, const char* key, float val) {
    ss << "\"" << key << "\":" << val;
}
static void jsonAppend(std::ostringstream& ss, const char* key, bool val) {
    ss << "\"" << key << "\":" << (val ? "true" : "false");
}
static void jsonAppend(std::ostringstream& ss, const char* key, const std::string& val) {
    ss << "\"" << key << "\":\"" << val << "\"";
}

std::string ReasoningProfileManager::serializeProfileToJSON(const ReasoningProfile& p) const {
    std::ostringstream ss;
    ss << "{";
    jsonAppend(ss, "name", p.name); ss << ",";

    // Reasoning
    ss << "\"reasoning\":{";
    jsonAppend(ss, "depth", p.reasoning.reasoningDepth); ss << ",";
    jsonAppend(ss, "mode", (int)p.reasoning.mode); ss << ",";
    jsonAppend(ss, "visibility", (int)p.reasoning.visibility); ss << ",";
    jsonAppend(ss, "enableCritic", p.reasoning.enableCritic); ss << ",";
    jsonAppend(ss, "enableAuditor", p.reasoning.enableAuditor); ss << ",";
    jsonAppend(ss, "enableThinker", p.reasoning.enableThinker); ss << ",";
    jsonAppend(ss, "enableResearcher", p.reasoning.enableResearcher); ss << ",";
    jsonAppend(ss, "enableDebaters", p.reasoning.enableDebaters); ss << ",";
    jsonAppend(ss, "enableVerifier", p.reasoning.enableVerifier); ss << ",";
    jsonAppend(ss, "enableRefiner", p.reasoning.enableRefiner); ss << ",";
    jsonAppend(ss, "enableSynthesizer", p.reasoning.enableSynthesizer); ss << ",";
    jsonAppend(ss, "enableBrainstorm", p.reasoning.enableBrainstorm); ss << ",";
    jsonAppend(ss, "enableSummarizer", p.reasoning.enableSummarizer); ss << ",";
    jsonAppend(ss, "confidenceThreshold", p.reasoning.confidenceThreshold); ss << ",";
    jsonAppend(ss, "qualityFloor", p.reasoning.qualityFloor); ss << ",";
    jsonAppend(ss, "requireFinalAnswer", p.reasoning.requireFinalAnswer); ss << ",";
    jsonAppend(ss, "allowFallback", p.reasoning.allowFallback); ss << ",";
    jsonAppend(ss, "autoBypassSimple", p.reasoning.autoBypassSimple); ss << ",";
    jsonAppend(ss, "simpleInputMaxLen", p.reasoning.simpleInputMaxLen); ss << ",";
    jsonAppend(ss, "exposeChainOfThought", p.reasoning.exposeChainOfThought); ss << ",";
    jsonAppend(ss, "exposeStepTimings", p.reasoning.exposeStepTimings); ss << ",";
    jsonAppend(ss, "exposeConfidence", p.reasoning.exposeConfidence);
    ss << "},";

    // Adaptive
    ss << "\"adaptive\":{";
    jsonAppend(ss, "enabled", p.adaptive.enabled); ss << ",";
    jsonAppend(ss, "strategy", (int)p.adaptive.strategy); ss << ",";
    jsonAppend(ss, "latencyTargetMs", p.adaptive.latencyTargetMs); ss << ",";
    jsonAppend(ss, "latencyMaxMs", p.adaptive.latencyMaxMs); ss << ",";
    jsonAppend(ss, "latencySmoothingAlpha", p.adaptive.latencySmoothingAlpha); ss << ",";
    jsonAppend(ss, "depthReductionPerStep", p.adaptive.depthReductionPerStep); ss << ",";
    jsonAppend(ss, "lowConfidenceThreshold", p.adaptive.lowConfidenceThreshold); ss << ",";
    jsonAppend(ss, "highConfidenceThreshold", p.adaptive.highConfidenceThreshold); ss << ",";
    jsonAppend(ss, "minAdaptiveDepth", p.adaptive.minAdaptiveDepth); ss << ",";
    jsonAppend(ss, "maxAdaptiveDepth", p.adaptive.maxAdaptiveDepth);
    ss << "},";

    // Thermal
    ss << "\"thermal\":{";
    jsonAppend(ss, "enabled", p.thermal.enabled); ss << ",";
    jsonAppend(ss, "pollIntervalMs", p.thermal.pollIntervalMs); ss << ",";
    jsonAppend(ss, "warmThreshold", p.thermal.warmThreshold); ss << ",";
    jsonAppend(ss, "hotThreshold", p.thermal.hotThreshold); ss << ",";
    jsonAppend(ss, "criticalThreshold", p.thermal.criticalThreshold); ss << ",";
    jsonAppend(ss, "depthAtWarm", p.thermal.depthAtWarm); ss << ",";
    jsonAppend(ss, "depthAtHot", p.thermal.depthAtHot); ss << ",";
    jsonAppend(ss, "depthAtCritical", p.thermal.depthAtCritical); ss << ",";
    jsonAppend(ss, "forceBypassInCritical", p.thermal.forceBypassInCritical);
    ss << "},";

    // Swarm
    ss << "\"swarm\":{";
    jsonAppend(ss, "enabled", p.swarm.enabled); ss << ",";
    jsonAppend(ss, "mode", (int)p.swarm.mode); ss << ",";
    jsonAppend(ss, "agentCount", p.swarm.agentCount); ss << ",";
    jsonAppend(ss, "voteThreshold", p.swarm.voteThreshold); ss << ",";
    jsonAppend(ss, "tournamentRounds", p.swarm.tournamentRounds); ss << ",";
    jsonAppend(ss, "heterogeneous", p.swarm.heterogeneous); ss << ",";
    jsonAppend(ss, "ensembleWeightDecay", p.swarm.ensembleWeightDecay); ss << ",";
    jsonAppend(ss, "timeoutPerAgentMs", p.swarm.timeoutPerAgentMs);
    ss << "},";

    // Self-tune
    ss << "\"selfTune\":{";
    jsonAppend(ss, "enabled", p.selfTune.enabled); ss << ",";
    jsonAppend(ss, "objective", (int)p.selfTune.objective); ss << ",";
    jsonAppend(ss, "windowSize", p.selfTune.windowSize); ss << ",";
    jsonAppend(ss, "learningRate", p.selfTune.learningRate); ss << ",";
    jsonAppend(ss, "explorationRate", p.selfTune.explorationRate); ss << ",";
    jsonAppend(ss, "persistTuning", p.selfTune.persistTuning); ss << ",";
    jsonAppend(ss, "persistPath", p.selfTune.persistPath); ss << ",";
    jsonAppend(ss, "minSamplesBeforeTune", p.selfTune.minSamplesBeforeTune); ss << ",";
    jsonAppend(ss, "qualityDecayAlpha", p.selfTune.qualityDecayAlpha); ss << ",";
    jsonAppend(ss, "latencyDecayAlpha", p.selfTune.latencyDecayAlpha);
    ss << "}";

    ss << "}";
    return ss.str();
}

// Minimal JSON value extractor (no external dependency)
namespace {
bool jsonExtractInt(const std::string& json, const char* key, int& out) {
    std::string needle = std::string("\"") + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    while (pos < json.size() && json[pos] == ' ') pos++;
    out = std::atoi(json.c_str() + pos);
    return true;
}
bool jsonExtractDouble(const std::string& json, const char* key, double& out) {
    std::string needle = std::string("\"") + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    while (pos < json.size() && json[pos] == ' ') pos++;
    out = std::atof(json.c_str() + pos);
    return true;
}
bool jsonExtractFloat(const std::string& json, const char* key, float& out) {
    double d = 0;
    if (jsonExtractDouble(json, key, d)) { out = (float)d; return true; }
    return false;
}
bool jsonExtractBool(const std::string& json, const char* key, bool& out) {
    std::string needle = std::string("\"") + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    while (pos < json.size() && json[pos] == ' ') pos++;
    out = (json.substr(pos, 4) == "true");
    return true;
}
bool jsonExtractString(const std::string& json, const char* key, std::string& out) {
    std::string needle = std::string("\"") + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) return false;
    out = json.substr(pos, end - pos);
    return true;
}
} // anonymous namespace

bool ReasoningProfileManager::deserializeProfileFromJSON(
    const std::string& json, ReasoningProfile& p) const {

    jsonExtractString(json, "name", p.name);

    int intVal = 0;
    double dVal = 0;
    float fVal = 0;
    bool bVal = false;

    // Reasoning
    if (jsonExtractInt(json, "depth", intVal))
        p.reasoning.reasoningDepth = (std::max)(0, (std::min)(intVal, 8));
    if (jsonExtractInt(json, "mode", intVal))
        p.reasoning.mode = (ReasoningMode)(std::max)(0, (std::min)(intVal, (int)ReasoningMode::Count - 1));
    if (jsonExtractInt(json, "visibility", intVal))
        p.reasoning.visibility = (ReasoningVisibility)(std::max)(0, (std::min)(intVal, (int)ReasoningVisibility::Count - 1));
    if (jsonExtractBool(json, "enableCritic", bVal)) p.reasoning.enableCritic = bVal;
    if (jsonExtractBool(json, "enableAuditor", bVal)) p.reasoning.enableAuditor = bVal;
    if (jsonExtractBool(json, "enableThinker", bVal)) p.reasoning.enableThinker = bVal;
    if (jsonExtractBool(json, "enableResearcher", bVal)) p.reasoning.enableResearcher = bVal;
    if (jsonExtractBool(json, "enableDebaters", bVal)) p.reasoning.enableDebaters = bVal;
    if (jsonExtractBool(json, "enableVerifier", bVal)) p.reasoning.enableVerifier = bVal;
    if (jsonExtractBool(json, "enableRefiner", bVal)) p.reasoning.enableRefiner = bVal;
    if (jsonExtractBool(json, "enableSynthesizer", bVal)) p.reasoning.enableSynthesizer = bVal;
    if (jsonExtractBool(json, "enableBrainstorm", bVal)) p.reasoning.enableBrainstorm = bVal;
    if (jsonExtractBool(json, "enableSummarizer", bVal)) p.reasoning.enableSummarizer = bVal;
    if (jsonExtractFloat(json, "confidenceThreshold", fVal)) p.reasoning.confidenceThreshold = fVal;
    if (jsonExtractFloat(json, "qualityFloor", fVal)) p.reasoning.qualityFloor = fVal;
    if (jsonExtractBool(json, "requireFinalAnswer", bVal)) p.reasoning.requireFinalAnswer = bVal;
    if (jsonExtractBool(json, "allowFallback", bVal)) p.reasoning.allowFallback = bVal;
    if (jsonExtractBool(json, "autoBypassSimple", bVal)) p.reasoning.autoBypassSimple = bVal;
    if (jsonExtractInt(json, "simpleInputMaxLen", intVal)) p.reasoning.simpleInputMaxLen = intVal;
    if (jsonExtractBool(json, "exposeChainOfThought", bVal)) p.reasoning.exposeChainOfThought = bVal;
    if (jsonExtractBool(json, "exposeStepTimings", bVal)) p.reasoning.exposeStepTimings = bVal;
    if (jsonExtractBool(json, "exposeConfidence", bVal)) p.reasoning.exposeConfidence = bVal;

    // Adaptive
    if (jsonExtractBool(json, "enabled", bVal)) p.adaptive.enabled = bVal;
    if (jsonExtractInt(json, "strategy", intVal))
        p.adaptive.strategy = (AdaptiveStrategy)(std::max)(0, (std::min)(intVal, (int)AdaptiveStrategy::Count - 1));
    if (jsonExtractDouble(json, "latencyTargetMs", dVal)) p.adaptive.latencyTargetMs = dVal;
    if (jsonExtractDouble(json, "latencyMaxMs", dVal)) p.adaptive.latencyMaxMs = dVal;
    if (jsonExtractDouble(json, "latencySmoothingAlpha", dVal)) p.adaptive.latencySmoothingAlpha = dVal;
    if (jsonExtractInt(json, "depthReductionPerStep", intVal)) p.adaptive.depthReductionPerStep = intVal;
    if (jsonExtractFloat(json, "lowConfidenceThreshold", fVal)) p.adaptive.lowConfidenceThreshold = fVal;
    if (jsonExtractFloat(json, "highConfidenceThreshold", fVal)) p.adaptive.highConfidenceThreshold = fVal;
    if (jsonExtractInt(json, "minAdaptiveDepth", intVal)) p.adaptive.minAdaptiveDepth = intVal;
    if (jsonExtractInt(json, "maxAdaptiveDepth", intVal)) p.adaptive.maxAdaptiveDepth = intVal;

    // Thermal
    if (jsonExtractFloat(json, "warmThreshold", fVal)) p.thermal.warmThreshold = fVal;
    if (jsonExtractFloat(json, "hotThreshold", fVal)) p.thermal.hotThreshold = fVal;
    if (jsonExtractFloat(json, "criticalThreshold", fVal)) p.thermal.criticalThreshold = fVal;
    if (jsonExtractInt(json, "depthAtWarm", intVal)) p.thermal.depthAtWarm = intVal;
    if (jsonExtractInt(json, "depthAtHot", intVal)) p.thermal.depthAtHot = intVal;
    if (jsonExtractInt(json, "depthAtCritical", intVal)) p.thermal.depthAtCritical = intVal;
    if (jsonExtractBool(json, "forceBypassInCritical", bVal)) p.thermal.forceBypassInCritical = bVal;

    // Swarm
    if (jsonExtractInt(json, "agentCount", intVal)) p.swarm.agentCount = intVal;
    if (jsonExtractFloat(json, "voteThreshold", fVal)) p.swarm.voteThreshold = fVal;
    if (jsonExtractInt(json, "tournamentRounds", intVal)) p.swarm.tournamentRounds = intVal;
    if (jsonExtractBool(json, "heterogeneous", bVal)) p.swarm.heterogeneous = bVal;
    if (jsonExtractFloat(json, "ensembleWeightDecay", fVal)) p.swarm.ensembleWeightDecay = fVal;
    if (jsonExtractInt(json, "timeoutPerAgentMs", intVal)) p.swarm.timeoutPerAgentMs = intVal;

    // Self-tune
    if (jsonExtractInt(json, "windowSize", intVal)) p.selfTune.windowSize = intVal;
    if (jsonExtractDouble(json, "learningRate", dVal)) p.selfTune.learningRate = dVal;
    if (jsonExtractDouble(json, "explorationRate", dVal)) p.selfTune.explorationRate = dVal;
    if (jsonExtractBool(json, "persistTuning", bVal)) p.selfTune.persistTuning = bVal;
    if (jsonExtractInt(json, "minSamplesBeforeTune", intVal)) p.selfTune.minSamplesBeforeTune = intVal;

    return true;
}

bool ReasoningProfileManager::saveProfileToFile(const std::string& path) const {
    ReasoningProfile p = getProfile();
    std::string json = serializeProfileToJSON(p);
    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;
    ofs << json;
    return ofs.good();
}

bool ReasoningProfileManager::loadProfileFromFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::string json((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
    ReasoningProfile p;
    if (!deserializeProfileFromJSON(json, p)) return false;
    setProfile(p);
    return true;
}
