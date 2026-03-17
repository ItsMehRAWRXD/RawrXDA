#include "agentic_deep_thinking_engine.hpp"

#include <chrono>
#include <utility>

AgenticDeepThinkingEngine::AgenticDeepThinkingEngine() = default;

AgenticDeepThinkingEngine::~AgenticDeepThinkingEngine() {
    cancelThinking();
}

AgenticDeepThinkingEngine::ThinkingResult
AgenticDeepThinkingEngine::think(const ThinkingContext& context) {
    ThinkingResult r{};
    r.finalAnswer = "[deep_thinking] Disabled at build-time (RAWRXD_ENABLE_DEEP_THINKING=OFF).";
    r.overallConfidence = 0.0f;
    r.iterationCount = 0;
    r.elapsedMilliseconds = 0;
    r.requiresUserInput = false;

    // Provide a minimal, user-facing hint.
    ReasoningStep step{};
    step.step = ThinkingStep::FinalSynthesis;
    step.title = "Deep Thinking Disabled";
    step.content = "This build is configured without AgenticDeepThinkingEngine. Enable RAWRXD_ENABLE_DEEP_THINKING to build the full implementation.";
    step.confidence = 0.0f;
    step.successful = false;
    r.steps.push_back(std::move(step));

    // Optional: include the original problem so the caller can still log it.
    if (!context.problem.empty()) {
        r.suggestedFixes.push_back("Re-run build with -DRAWRXD_ENABLE_DEEP_THINKING=ON if you need deep research.");
    }

    return r;
}

void AgenticDeepThinkingEngine::startThinking(
    const ThinkingContext& context,
    std::function<void(const ReasoningStep&)> onStepComplete,
    std::function<void(float)> onProgressUpdate,
    std::function<void(const std::string&)> onError) {

    (void) context;

    if (onProgressUpdate) onProgressUpdate(0.0f);

    ReasoningStep step{};
    step.step = ThinkingStep::FinalSynthesis;
    step.title = "Deep Thinking Disabled";
    step.content = "AgenticDeepThinkingEngine is disabled in this build (RAWRXD_ENABLE_DEEP_THINKING=OFF).";
    step.confidence = 0.0f;
    step.successful = false;

    if (onStepComplete) onStepComplete(step);
    if (onProgressUpdate) onProgressUpdate(1.0f);

    if (onError) onError("deep_thinking disabled");
}

void AgenticDeepThinkingEngine::cancelThinking() {
    // No background work in stub.
    m_thinking.store(false);
}

std::vector<std::string> AgenticDeepThinkingEngine::findRelatedFiles(const std::string& /*query*/, int /*maxResults*/) {
    return {};
}

std::string AgenticDeepThinkingEngine::analyzeFile(const std::string& /*filePath*/) {
    return "";
}

std::string AgenticDeepThinkingEngine::searchProjectForPattern(const std::string& /*pattern*/) {
    return "";
}

bool AgenticDeepThinkingEngine::evaluateAnswer(const std::string& /*answer*/, const ThinkingContext& /*context*/) {
    return false;
}

std::string AgenticDeepThinkingEngine::refineAnswer(const std::string& currentAnswer, const std::string& /*feedback*/) {
    return currentAnswer;
}

void AgenticDeepThinkingEngine::saveThinkingResult(const std::string& key, const ThinkingResult& result) {
    m_thinkingCache[key] = result;
}

AgenticDeepThinkingEngine::ThinkingResult*
AgenticDeepThinkingEngine::getCachedThinking(const std::string& key) {
    auto it = m_thinkingCache.find(key);
    if (it == m_thinkingCache.end()) return nullptr;
    return &it->second;
}

void AgenticDeepThinkingEngine::clearMemory() {
    m_thinkingCache.clear();
    m_patternFrequency.clear();
}

std::vector<std::pair<std::string, int>> AgenticDeepThinkingEngine::getMostUsedPatterns() const {
    std::vector<std::pair<std::string, int>> out;
    out.reserve(m_patternFrequency.size());
    for (const auto& kv : m_patternFrequency) out.emplace_back(kv.first, kv.second);
    return out;
}

AgenticDeepThinkingEngine::MultiAgentResult
AgenticDeepThinkingEngine::thinkMultiAgent(const ThinkingContext& context) {
    MultiAgentResult mar{};
    mar.consensusResult = think(context);
    mar.consensusConfidence = 0.0f;
    mar.consensusReached = false;
    return mar;
}

AgenticDeepThinkingEngine::ThinkingStats AgenticDeepThinkingEngine::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void AgenticDeepThinkingEngine::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = ThinkingStats{};
}
