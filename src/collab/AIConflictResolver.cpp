// ============================================================================
// AIConflictResolver.cpp — AI-Powered Conflict Resolution
// ============================================================================
// Bridges LiveShare collaborative editing with SovereignInferenceClient
// for neural merge suggestions using local inference (zero HTTP).
//
// Architecture:
//   LiveShareSession::applyLocalOperation() → detect conflict → AIConflictResolver
//   → build prompt → SovereignInferenceClient::ChatSync() → parse result
//   → return MergeSuggestion
// ============================================================================

#include "collab/AIConflictResolver.h"
#include <chrono>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace RawrXD::Collaboration {

// ============================================================================
// Constants
// ============================================================================

static const char* SYSTEM_PROMPT = R"(
You are a code merge resolution engine. Given conflicting edits from multiple users,
produce a single coherent merged result that preserves the intent of all edits.

Rules:
1. Return ONLY the merged text — no explanations, no markdown, no commentary.
2. Preserve code structure, indentation, and syntax.
3. If edits are incompatible, prefer the more recent edit.
4. If one edit is a deletion and another is an insertion at the same location,
   include the insertion unless it clearly conflicts with surrounding context.
5. Never return empty text unless all inputs are deletions.
)";

static const char* DEFAULT_MODEL_PATH = "./models/codestral-22b-q4_0.gguf";

// ============================================================================
// Constructor / Destructor
// ============================================================================

AIConflictResolver::AIConflictResolver() = default;
AIConflictResolver::~AIConflictResolver() { shutdown(); }

// ============================================================================
// Initialization
// ============================================================================

bool AIConflictResolver::initialize(const std::string& modelPath) {
    if (m_initialized) {
        return true;
    }

    std::string path = modelPath;
    if (path.empty()) {
        const char* envPath = std::getenv("RAWRXD_MERGE_MODEL_PATH");
        if (envPath) {
            path = envPath;
        } else {
            path = DEFAULT_MODEL_PATH;
        }
    }

    try {
        Agent::SovereignModelConfig cfg;
        cfg.model_path = path;
        cfg.context_size = 4096;
        cfg.n_batch = 512;
        cfg.temperature = m_temperature;
        cfg.max_tokens = m_maxTokens;
        cfg.enable_speculative = true;
        cfg.draft_tokens = 5;

        m_inferenceClient = std::make_unique<Agent::SovereignInferenceClient>(cfg);

        if (!m_inferenceClient->LoadModel(path)) {
            std::cerr << "[AIConflictResolver] Failed to load model: " << path << std::endl;
            std::cerr << "[AIConflictResolver] Will use heuristic fallback." << std::endl;
            m_inferenceClient.reset();
            m_initialized = true;
            m_enableAI = false;
            return true;
        }

        m_initialized = true;
        m_enableAI = true;
        std::cout << "[AIConflictResolver] Initialized with model: " << path << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[AIConflictResolver] Initialization error: " << e.what() << std::endl;
        m_initialized = true;
        m_enableAI = false;
        return true;
    }
}

void AIConflictResolver::shutdown() {
    if (m_inferenceClient) {
        m_inferenceClient->UnloadModel();
        m_inferenceClient.reset();
    }
    m_initialized = false;
}

bool AIConflictResolver::isInitialized() const {
    return m_initialized;
}

// ============================================================================
// Main Resolution Entry Point
// ============================================================================

MergeSuggestion AIConflictResolver::resolveConflicts(
    const std::vector<std::string>& conflictTexts,
    const std::string& documentContext) {

    auto startTime = std::chrono::high_resolution_clock::now();

    MergeSuggestion result;

    if (conflictTexts.empty()) {
        result.confidence = 1.0;
        result.reasoning = "No conflicts to resolve";
        return result;
    }

    if (m_enableAI && m_inferenceClient && m_inferenceClient->IsLoaded()) {
        try {
            result = aiResolve(conflictTexts, documentContext);
        } catch (const std::exception& e) {
            std::cerr << "[AIConflictResolver] AI resolution failed: " << e.what() << std::endl;
            result = heuristicResolve(conflictTexts);
        }
    } else {
        result = heuristicResolve(conflictTexts);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    result.latencyMs = latencyMs;

    updateStats(result, latencyMs);
    return result;
}

std::vector<MergeSuggestion> AIConflictResolver::resolveConflictsBatch(
    const std::vector<std::vector<std::string>>& conflictSets) {

    std::vector<MergeSuggestion> results;
    results.reserve(conflictSets.size());

    for (const auto& conflicts : conflictSets) {
        results.push_back(resolveConflicts(conflicts));
    }

    return results;
}

// ============================================================================
// AI Resolution (SovereignInferenceClient)
// ============================================================================

MergeSuggestion AIConflictResolver::aiResolve(
    const std::vector<std::string>& conflictTexts,
    const std::string& documentContext) {

    MergeSuggestion result;
    result.fromAI = true;

    std::string prompt = buildPrompt(conflictTexts, documentContext);

    std::vector<Agent::ChatMessage> messages;
    messages.push_back({"system", SYSTEM_PROMPT});
    messages.push_back({"user", prompt});

    auto inferenceResult = m_inferenceClient->ChatSync(messages);

    if (!inferenceResult.success || inferenceResult.response.empty()) {
        std::cerr << "[AIConflictResolver] Inference returned empty, using heuristic" << std::endl;
        return heuristicResolve(conflictTexts);
    }

    std::string mergedText = inferenceResult.response;

    // Clean up common prefixes/suffixes
    const char* prefixes[] = {"Merged:", "Result:", "Merged result:", "```", "```cpp"};
    for (const auto* prefix : prefixes) {
        size_t pos = mergedText.find(prefix);
        if (pos != std::string::npos && pos < 20) {
            mergedText = mergedText.substr(pos + strlen(prefix));
        }
    }

    size_t endPos = mergedText.rfind("```");
    if (endPos != std::string::npos && endPos > mergedText.size() - 10) {
        mergedText = mergedText.substr(0, endPos);
    }

    size_t first = mergedText.find_first_not_of(" \t\n\r");
    size_t last = mergedText.find_last_not_of(" \t\n\r");
    if (first != std::string::npos && last != std::string::npos) {
        mergedText = mergedText.substr(first, last - first + 1);
    }

    result.mergedText = mergedText;
    result.confidence = 0.85;
    result.reasoning = "AI neural merge via SovereignInferenceClient";

    return result;
}

// ============================================================================
// Heuristic Resolution (Fallback)
// ============================================================================

MergeSuggestion AIConflictResolver::heuristicResolve(
    const std::vector<std::string>& conflictTexts) {

    MergeSuggestion result;
    result.fromAI = false;

    // Simple strategy: concatenate all non-empty texts
    std::ostringstream merged;
    for (const auto& text : conflictTexts) {
        if (!text.empty()) {
            if (merged.tellp() > 0) merged << "\n";
            merged << text;
        }
    }

    result.mergedText = merged.str();
    result.confidence = 0.5;
    result.reasoning = "Heuristic merge (concatenation)";

    return result;
}

// ============================================================================
// Prompt Building
// ============================================================================

std::string AIConflictResolver::buildPrompt(
    const std::vector<std::string>& conflictTexts,
    const std::string& documentContext) {

    std::ostringstream ss;

    ss << "Resolve the following conflicting edits into a single coherent result.\n\n";

    if (!documentContext.empty()) {
        ss << "Document context:\n";
        ss << documentContext << "\n\n";
    }

    ss << "Conflicting edits:\n";

    for (size_t i = 0; i < conflictTexts.size(); ++i) {
        ss << "Edit " << (i + 1) << ":\n";
        ss << "```\n" << conflictTexts[i] << "\n```\n\n";
    }

    ss << "Merged result:\n";

    return ss.str();
}

// ============================================================================
// Statistics
// ============================================================================

AIConflictResolver::Stats AIConflictResolver::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void AIConflictResolver::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = Stats{};
}

void AIConflictResolver::updateStats(const MergeSuggestion& result, double latencyMs) {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    m_stats.totalResolutions++;

    if (result.fromAI) {
        m_stats.aiResolutions++;
    } else {
        m_stats.heuristicResolutions++;
    }

    if (result.confidence < 0.1) {
        m_stats.failedResolutions++;
    }

    if (m_stats.totalResolutions == 1) {
        m_stats.avgLatencyMs = latencyMs;
        m_stats.avgConfidence = result.confidence;
    } else {
        m_stats.avgLatencyMs = 0.9 * m_stats.avgLatencyMs + 0.1 * latencyMs;
        m_stats.avgConfidence = 0.9 * m_stats.avgConfidence + 0.1 * result.confidence;
    }
}

// ============================================================================
// Configuration
// ============================================================================

void AIConflictResolver::setTemperature(float temp) {
    m_temperature = std::clamp(temp, 0.0f, 2.0f);
    if (m_inferenceClient) {
        auto cfg = m_inferenceClient->GetConfig();
        cfg.temperature = m_temperature;
        m_inferenceClient->SetConfig(cfg);
    }
}

void AIConflictResolver::setMaxTokens(uint32_t maxTokens) {
    m_maxTokens = maxTokens;
    if (m_inferenceClient) {
        auto cfg = m_inferenceClient->GetConfig();
        cfg.max_tokens = m_maxTokens;
        m_inferenceClient->SetConfig(cfg);
    }
}

void AIConflictResolver::setEnableAI(bool enable) {
    m_enableAI = enable;
}

} // namespace RawrXD::Collaboration
