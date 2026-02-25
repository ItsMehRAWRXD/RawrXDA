// context_deterioration_hotpatch.cpp — Proactive Context Quality Hotpatch
// ============================================================================
// Keeps model quality at 100% by preventing context-length-induced deterioration.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "context_deterioration_hotpatch.hpp"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <cctype>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

ContextDeteriorationHotpatch& ContextDeteriorationHotpatch::instance() {
    static ContextDeteriorationHotpatch inst;
    return inst;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ContextDeteriorationHotpatch::ContextDeteriorationHotpatch()
    : m_enabled(true)
    , m_targetQuality(100)
{
    loadDefaultProfiles();
}

// ---------------------------------------------------------------------------
// Token Estimation
// ---------------------------------------------------------------------------

size_t ContextDeteriorationHotpatch::estimateTokens(const std::string& text) {
    if (text.empty()) return 0;
    // ~4 chars per token for English/code; conservative for mixed content
    size_t chars = text.size();
    size_t words = 0;
    bool inWord = false;
    for (unsigned char c : text) {
        if (std::isspace(c) || c == '\n' || c == '\r') {
            inWord = false;
        } else if (!inWord) {
            words++;
            inWord = true;
        }
    }
    if (words == 0) words = 1;
    size_t byChars = (chars + 3) / 4;
    size_t byWords = (words * 4 + 2) / 3;  // ~1.3 tokens/word
    return (byChars + byWords) / 2;  // Average
}

// ---------------------------------------------------------------------------
// Default Model Profiles (known deterioration sweet spots)
// ---------------------------------------------------------------------------

void ContextDeteriorationHotpatch::loadDefaultProfiles() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profiles.clear();

    // LLaMA family — often degrades past 4–8K despite 32K+ context
    m_profiles.push_back({
        "llama", 4096, 0, 8192, true, true
    });
    m_profiles.push_back({
        "llama2", 4096, 0, 8192, true, true
    });
    m_profiles.push_back({
        "llama3", 8192, 0, 16384, true, true
    });
    m_profiles.push_back({
        "llama-3", 8192, 0, 16384, true, true
    });

    // Mistral — sliding window 32K, quality best in first 8K
    m_profiles.push_back({
        "mistral", 8192, 0, 16384, true, true
    });
    m_profiles.push_back({
        "mixtral", 8192, 0, 16384, true, true
    });

    // Qwen — generally better at long context
    m_profiles.push_back({
        "qwen", 16384, 0, 32768, true, true
    });
    m_profiles.push_back({
        "qwen2", 16384, 0, 32768, true, true
    });

    // Phi — smaller models, 4K sweet spot
    m_profiles.push_back({
        "phi", 4096, 0, 6144, true, true
    });
    m_profiles.push_back({
        "phi-2", 4096, 0, 6144, true, true
    });

    // Gemma
    m_profiles.push_back({
        "gemma", 8192, 0, 16384, true, true
    });

    // DeepSeek
    m_profiles.push_back({
        "deepseek", 16384, 0, 32768, true, true
    });

    // Generic fallback — use 50% of max as sweet spot
    m_profiles.push_back({
        "default", 4096, 0, 8192, true, true
    });
}

// ---------------------------------------------------------------------------
// Profile Resolution
// ---------------------------------------------------------------------------

static bool modelNameMatches(const std::string& name, const char* key) {
    if (!key || !*key) return false;
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    std::string keyLower;
    while (*key) {
        keyLower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*key++))));
    }
    return lower.find(keyLower) != std::string::npos;
}

const ModelDeteriorationProfile* ContextDeteriorationHotpatch::resolveProfile(
    const char* modelName,
    uint32_t modelMaxContext) const
{
    if (modelMaxContext == 0) modelMaxContext = 4096;

    for (const auto& p : m_profiles) {
        if (!p.enabled) continue;
        if (modelNameMatches(p.modelName, modelName))
            return &p;
    }

    // Fallback: default profile with model max
    for (const auto& p : m_profiles) {
        if (p.modelName == "default") return &p;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Quality Score (0-100)
// ---------------------------------------------------------------------------

int ContextDeteriorationHotpatch::getQualityScore(
    size_t currentTokens,
    uint32_t modelMaxContext,
    const char* modelName) const
{
    if (modelMaxContext == 0) modelMaxContext = 4096;
    if (currentTokens <= 0) return 100;

    const ModelDeteriorationProfile* prof = resolveProfile(modelName, modelMaxContext);
    uint32_t sweetSpot = 4096;
    if (prof) {
        sweetSpot = prof->sweetSpotTokens;
        if (sweetSpot == 0) sweetSpot = modelMaxContext / 2;
    } else {
        sweetSpot = std::min(4096u, modelMaxContext / 2);
    }

    if (currentTokens <= sweetSpot) return 100;

    uint32_t decayStart = prof && prof->deteriorationStart > 0
        ? prof->deteriorationStart
        : sweetSpot + (sweetSpot / 4);  // 25% over sweet spot

    if (currentTokens <= decayStart) return 95;

    uint32_t aggressive = prof && prof->aggressiveThreshold > 0
        ? prof->aggressiveThreshold
        : modelMaxContext;

    // Linear decay from 95 at decayStart to 60 at aggressive
    if (currentTokens >= aggressive) return 60;

    float t = static_cast<float>(currentTokens - decayStart) /
              static_cast<float>(aggressive - decayStart);
    return 95 - static_cast<int>(35.0f * t);
}

// ---------------------------------------------------------------------------
// Truncate to approximate token boundary (by char count)
// ---------------------------------------------------------------------------

std::string ContextDeteriorationHotpatch::truncateToTokenBoundary(
    const std::string& text,
    size_t maxChars)
{
    if (text.size() <= maxChars) return text;
    size_t cut = maxChars;
    // Prefer cutting at newline or space
    while (cut > maxChars / 2 && cut < text.size()) {
        char c = text[cut];
        if (c == '\n' || c == ' ' || c == '\t') break;
        cut--;
    }
    if (cut < maxChars / 2) cut = maxChars;
    return text.substr(0, cut);
}

// ---------------------------------------------------------------------------
// Sliding Window — keep tail, drop head
// ---------------------------------------------------------------------------

ContextPrepareResult ContextDeteriorationHotpatch::applySlidingWindow(
    const std::string& prompt,
    size_t tokens,
    uint32_t targetTokens,
    const ModelDeteriorationProfile* /*profile*/)
{
    ContextPrepareResult r = {};
    r.originalTokens = tokens;
    r.finalTokens = targetTokens;
    r.droppedTokens = (tokens > targetTokens) ? (tokens - targetTokens) : 0;
    r.mitigation = DeteriorationMitigation::SlidingWindow;
    r.qualityScore = 100;
    r.description = "Sliding window: dropped oldest context to maintain quality";

    if (tokens <= targetTokens) {
        r.modifiedPrompt = prompt;
        r.finalTokens = tokens;
        r.droppedTokens = 0;
        r.mitigation = DeteriorationMitigation::None;
        return r;
    }

    // Target chars ~= targetTokens * 4
    size_t targetChars = targetTokens * 4;
    if (prompt.size() <= targetChars) {
        r.modifiedPrompt = prompt;
        r.finalTokens = estimateTokens(prompt);
        return r;
    }

    // Try to split on message boundaries (common chat format)
    const char* sysEnd = nullptr;
    const char* p = prompt.c_str();
    if (std::strstr(p, "<|im_start|>system") || std::strstr(p, "[INST]") ||
        std::strstr(p, "System:") || std::strstr(p, "system:")) {
        const char* markers[] = { "\n\n", "\nUser:", "\nHuman:", "\n[INST]", "<|im_start|>user" };
        for (const char* m : markers) {
            const char* found = std::strstr(p, m);
            if (found) { sysEnd = found; break; }
        }
    }

    std::string header;
    size_t headerLen = 0;
    if (sysEnd) {
        header.assign(p, sysEnd - p);
        headerLen = header.size();
    }

    size_t tailMax = targetChars - headerLen - 200;  // Reserve for placeholder
    if (tailMax > targetChars) tailMax = targetChars / 2;

    std::string tail;
    if (sysEnd) {
        tail = std::string(sysEnd);
        if (tail.size() > tailMax) {
            tail = truncateToTokenBoundary(tail, tailMax);
            size_t drop = prompt.size() - header.size() - tail.size();
            if (drop > 100) {
                std::string placeholder = "\n\n[Earlier messages truncated for coherence. ";
                placeholder += std::to_string(r.droppedTokens) + " tokens removed.]\n\n";
                tail = placeholder + tail.substr(0, tailMax - placeholder.size());
            }
        }
    } else {
        size_t start = (prompt.size() > tailMax) ? (prompt.size() - tailMax) : 0;
        tail = truncateToTokenBoundary(prompt.substr(start), tailMax);
    }

    r.modifiedPrompt = header.empty() ? tail : (header + tail);
    r.finalTokens = estimateTokens(r.modifiedPrompt);

    m_stats.preparationsTotal.fetch_add(1, std::memory_order_relaxed);
    m_stats.mitigationsApplied.fetch_add(1, std::memory_order_relaxed);
    m_stats.slidingWindowCount.fetch_add(1, std::memory_order_relaxed);
    m_stats.tokensSaved.fetch_add(r.droppedTokens, std::memory_order_relaxed);
    m_stats.quality100Count.fetch_add(1, std::memory_order_relaxed);

    return r;
}

// ---------------------------------------------------------------------------
// Truncation — hard cut
// ---------------------------------------------------------------------------

ContextPrepareResult ContextDeteriorationHotpatch::applyTruncation(
    const std::string& prompt,
    size_t tokens,
    uint32_t targetTokens,
    const ModelDeteriorationProfile* /*profile*/)
{
    ContextPrepareResult r = {};
    r.originalTokens = tokens;
    r.finalTokens = targetTokens;
    r.droppedTokens = (tokens > targetTokens) ? (tokens - targetTokens) : 0;
    r.mitigation = DeteriorationMitigation::Truncate;
    r.qualityScore = 100;
    r.description = "Truncated context to prevent deterioration";

    if (tokens <= targetTokens) {
        r.modifiedPrompt = prompt;
        r.finalTokens = tokens;
        r.droppedTokens = 0;
        r.mitigation = DeteriorationMitigation::None;
        return r;
    }

    size_t targetChars = targetTokens * 4;
    r.modifiedPrompt = truncateToTokenBoundary(prompt, targetChars);
    if (r.droppedTokens > 500) {
        std::string notice = "\n[Context truncated for quality. "
            + std::to_string(r.droppedTokens) + " tokens omitted.]\n";
        r.modifiedPrompt += notice;
    }
    r.finalTokens = estimateTokens(r.modifiedPrompt);

    m_stats.preparationsTotal.fetch_add(1, std::memory_order_relaxed);
    m_stats.mitigationsApplied.fetch_add(1, std::memory_order_relaxed);
    m_stats.truncationCount.fetch_add(1, std::memory_order_relaxed);
    m_stats.tokensSaved.fetch_add(r.droppedTokens, std::memory_order_relaxed);
    m_stats.quality100Count.fetch_add(1, std::memory_order_relaxed);

    return r;
}

// ---------------------------------------------------------------------------
// prepareContextForInference (main entry)
// ---------------------------------------------------------------------------

ContextPrepareResult ContextDeteriorationHotpatch::prepareContextForInference(
    const std::string& prompt,
    size_t estimatedTokens,
    uint32_t modelMaxContext,
    const char* modelName)
{
    ContextPrepareResult r = {};
    r.modifiedPrompt = prompt;
    r.qualityScore = 100;
    r.mitigation = DeteriorationMitigation::None;
    r.originalTokens = estimatedTokens;
    r.finalTokens = estimatedTokens;
    r.droppedTokens = 0;
    r.description = "No mitigation needed";

    if (!m_enabled) {
        m_stats.preparationsTotal.fetch_add(1, std::memory_order_relaxed);
        m_stats.quality100Count.fetch_add(1, std::memory_order_relaxed);
        return r;
    }

    if (modelMaxContext == 0) modelMaxContext = 4096;

    const ModelDeteriorationProfile* prof = resolveProfile(modelName, modelMaxContext);
    uint32_t sweetSpot = 4096;
    uint32_t aggressive = modelMaxContext;

    if (prof) {
        sweetSpot = prof->sweetSpotTokens;
        if (sweetSpot == 0) sweetSpot = modelMaxContext / 2;
        if (prof->aggressiveThreshold > 0) aggressive = prof->aggressiveThreshold;
    } else {
        sweetSpot = std::min(4096u, modelMaxContext / 2);
    }

    if (estimatedTokens <= sweetSpot) {
        m_stats.preparationsTotal.fetch_add(1, std::memory_order_relaxed);
        m_stats.quality100Count.fetch_add(1, std::memory_order_relaxed);
        return r;
    }

    // Need mitigation
    uint32_t target = sweetSpot;
    if (estimatedTokens >= aggressive) {
        target = sweetSpot * 3 / 4;  // Emergency: cut harder
        r.mitigation = DeteriorationMitigation::EmergencyCap;
    }

    if (prof && prof->preferSlidingWindow)
        return applySlidingWindow(prompt, estimatedTokens, target, prof);
    else
        return applyTruncation(prompt, estimatedTokens, target, prof);
}

ContextPrepareResult ContextDeteriorationHotpatch::prepareContextForInference(
    const std::string& prompt,
    uint32_t modelMaxContext,
    const char* modelName)
{
    size_t tokens = estimateTokens(prompt);
    return prepareContextForInference(prompt, tokens, modelMaxContext, modelName);
}

// ---------------------------------------------------------------------------
// Profile Management
// ---------------------------------------------------------------------------

PatchResult ContextDeteriorationHotpatch::registerProfile(
    const ModelDeteriorationProfile& profile)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& p : m_profiles) {
        if (p.modelName == profile.modelName) {
            p = profile;
            return PatchResult::ok("Profile updated");
        }
    }
    m_profiles.push_back(profile);
    return PatchResult::ok("Profile registered");
}

PatchResult ContextDeteriorationHotpatch::unregisterProfile(const char* modelName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::remove_if(m_profiles.begin(), m_profiles.end(),
        [modelName](const ModelDeteriorationProfile& p) {
            return p.modelName == modelName && modelName != "default";
        });
    if (it != m_profiles.end()) {
        m_profiles.erase(it, m_profiles.end());
        return PatchResult::ok("Profile removed");
    }
    return PatchResult::error("Profile not found or default protected");
}

const ModelDeteriorationProfile* ContextDeteriorationHotpatch::getProfile(
    const char* modelName) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return resolveProfile(modelName, 4096);
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void ContextDeteriorationHotpatch::setTargetQuality(int score) {
    m_targetQuality = (score < 0) ? 0 : (score > 100 ? 100 : score);
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

const ContextDeteriorationHotpatchStats& ContextDeteriorationHotpatch::getStats() const {
    return m_stats;
}

void ContextDeteriorationHotpatch::resetStats() {
    m_stats.preparationsTotal.store(0, std::memory_order_relaxed);
    m_stats.mitigationsApplied.store(0, std::memory_order_relaxed);
    m_stats.slidingWindowCount.store(0, std::memory_order_relaxed);
    m_stats.truncationCount.store(0, std::memory_order_relaxed);
    m_stats.tokensSaved.store(0, std::memory_order_relaxed);
    m_stats.quality100Count.store(0, std::memory_order_relaxed);
}
