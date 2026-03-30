#include "real_time_completion_engine.h"
#ifndef RAWRXD_NO_DEFAULT_EXECUTION_STATE_PROVIDER
#include "debug/execution_state_provider_debugger_core.hpp"
#endif
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <sstream>
#include <thread>

namespace {
#ifndef RAWRXD_NO_DEFAULT_EXECUTION_STATE_PROVIDER
std::shared_ptr<IExecutionStateProvider> makeDefaultExecutionStateProvider() {
    return std::make_shared<DebuggerCoreExecutionStateProvider>(&rawrxd::debug::DebuggerCore::getInstance());
}
#endif
}

RealTimeCompletionEngine::RealTimeCompletionEngine(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(logger),
      m_metrics(metrics),
      m_inferenceEngine(nullptr),
    #ifndef RAWRXD_NO_DEFAULT_EXECUTION_STATE_PROVIDER
      m_executionStateProvider(makeDefaultExecutionStateProvider()) {
    #else
          m_executionStateProvider(nullptr) {
    #endif
    if (m_logger) {
        m_logger->log("RealTimeCompletionEngine initialized");
    #ifndef RAWRXD_NO_DEFAULT_EXECUTION_STATE_PROVIDER
        m_logger->log("Default execution state provider attached");
    #endif
    }
}

std::vector<CodeCompletion> RealTimeCompletionEngine::getCompletions(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& fileType,
    const std::string& context) {

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        m_totalRequests++;

        std::string cacheKey = generateCacheKey(prefix, suffix);

        // Check cache first
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            auto it = m_completionCache.find(cacheKey);
            if (it != m_completionCache.end()) {
                m_cacheHits++;
                auto endTime = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    endTime - startTime).count();
                m_metrics->recordHistogram("completion_latency_us", latency);
                return it->second;
            }
        }

        const std::string prompt = buildCompletionPrompt(prefix, suffix, context);
        auto completions = generateCompletionsWithModel(prompt, 50);

        // Update cache
        updateCache(cacheKey, completions);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime).count();

        {
            std::lock_guard<std::mutex> lock(m_latencyMutex);
            m_latencyHistory.push_back(latency / 1000.0); // Convert to ms
            if (m_latencyHistory.size() > 1000) {
                m_latencyHistory.erase(m_latencyHistory.begin());
            }
        }

        m_metrics->recordHistogram("completion_latency_us", latency);

        return completions;

    } catch (const std::exception& e) {
        m_errorCount++;
        m_metrics->incrementCounter("completion_errors");
        return {};
    }
}

std::vector<CodeCompletion> RealTimeCompletionEngine::getInlineCompletions(
    const std::string& currentLine,
    int cursorColumn,
    const std::string& filePath) {


    // Extract prefix/suffix from line
    const int safeColumn = std::max(0, std::min(cursorColumn, static_cast<int>(currentLine.size())));
    std::string prefix = currentLine.substr(0, safeColumn);
    std::string suffix = currentLine.substr(safeColumn);

    return getCompletions(prefix, suffix, "cpp", "");
}

std::vector<CodeCompletion> RealTimeCompletionEngine::getMultiLineCompletions(
    const std::string& prefix,
    int maxLines) {


    return getCompletions(prefix, "", "cpp", "");
}

std::vector<CodeCompletion> RealTimeCompletionEngine::getContextualCompletions(
    const std::string& filePath,
    int line,
    int column,
    const std::string& scope) {


    // In full implementation, would analyze file context
    return getCompletions("", "", "cpp", scope);
}

void RealTimeCompletionEngine::prewarmCache(const std::string& filePath) {
    // Pre-populate cache with likely completions for this file
    // Strategy: Run a cheap inference pass to ensure weights are loaded
    // and potentially cache common headers or structures.
    
    // 1. Ensure model is loaded
    if (!m_inferenceEngine) return;

    // 2. Create a "warmup" prompt based on file extension
    std::string warmupPrompt = "// Language: C++\n#include <"; 
    if (filePath.find(".py") != std::string::npos) {
        warmupPrompt = "# Language: Python\nimport ";
    } else if (filePath.find(".js") != std::string::npos) {
        warmupPrompt = "// Language: JavaScript\nconst ";
    }

    // 3. Fire-and-forget generation (don't wait for result, just trigger compute)
    // Running in a detached thread strictly for side-effect of loading paging
    std::thread([this, warmupPrompt]() {
        try {
            // Short generation just to touch memory pages
            std::vector<int32_t> tokens = m_inferenceEngine->Tokenize(warmupPrompt);
            m_inferenceEngine->Generate(tokens, 5); 
        } catch (...) {
            // Ignore errors during prewarm
        }
    }).detach();
}

void RealTimeCompletionEngine::clearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_completionCache.clear();

}

PerformanceMetrics RealTimeCompletionEngine::getMetrics() const {
    PerformanceMetrics metrics;

    {
        std::lock_guard<std::mutex> lock(m_latencyMutex);
        if (m_latencyHistory.empty()) {
            metrics.avgLatencyMs = 0.0;
            metrics.p95LatencyMs = 0.0;
            metrics.p99LatencyMs = 0.0;
        } else {
            double sum = 0.0;
            for (double val : m_latencyHistory) {
                sum += val;
            }
            metrics.avgLatencyMs = sum / m_latencyHistory.size();

            auto sorted = m_latencyHistory;
            std::sort(sorted.begin(), sorted.end());
            metrics.p95LatencyMs = sorted[static_cast<int>(sorted.size() * 0.95)];
            metrics.p99LatencyMs = sorted[static_cast<int>(sorted.size() * 0.99)];
        }
    }

    metrics.cacheHitRate = m_totalRequests > 0 
        ? (double)m_cacheHits / m_totalRequests 
        : 0.0;
    metrics.requestCount = m_totalRequests;
    metrics.errorCount = m_errorCount;

    return metrics;
}

std::vector<CodeCompletion> RealTimeCompletionEngine::generateCompletionsWithModel(
    const std::string& prompt,
    int maxTokens) {

    std::vector<CodeCompletion> completions;

    try {

        m_metrics->incrementCounter("model_calls");

        // =============================================================================
        // PHASE 1 IMPLEMENTATION: Real Model Calling Integration
        // =============================================================================

        // STEP 1: Check InferenceEngine is Available and Model Loaded
        // ===========================================================
        if (!m_inferenceEngine) {

            m_metrics->incrementCounter("inference_engine_not_set");
            return {};
        }

        if (!m_inferenceEngine->IsModelLoaded()) {

            m_metrics->incrementCounter("model_not_loaded_errors");
            return {};
        }


        // STEP 2: Tokenize Prompt with Context Window Enforcement
        // ========================================================
        std::vector<int32_t> inputTokens = m_inferenceEngine->Tokenize(prompt);


        // Enforce context window limit (typical: 512-2048 tokens)
        // Leave some space for generation
        const size_t CONTEXT_WINDOW = 512;

        if (inputTokens.size() > CONTEXT_WINDOW) {
            // Keep the most recent tokens (sliding window)
            if (inputTokens.size() > CONTEXT_WINDOW) {
                inputTokens.erase(inputTokens.begin(),
                                 inputTokens.end() - CONTEXT_WINDOW);
            }
        }

        // STEP 3: Generate Tokens with Stopping Conditions
        // ================================================


        std::vector<int32_t> generatedTokens = m_inferenceEngine->Generate(inputTokens, maxTokens);

        std::string generatedCompletion;
        if (generatedTokens.size() > inputTokens.size()) {
            std::vector<int32_t> completionSegment(
                generatedTokens.begin() + static_cast<std::ptrdiff_t>(inputTokens.size()),
                generatedTokens.end());
            generatedCompletion = m_inferenceEngine->Detokenize(completionSegment);
        }

        // STEP 4: Parse Completions from Generated Text
        // =============================================
        if (!generatedCompletion.empty()) {
            // For now, treat the entire generation as one completion
            // In production, would split by statement boundaries, etc.
            
            CodeCompletion comp;
            comp.text = generatedCompletion;
            comp.detail = "AI-generated code completion from model";
            comp.confidence = 0.85;  // High confidence in model output
            comp.kind = "method";
            comp.insertTextLength = generatedCompletion.length();
            comp.cursorOffset = generatedCompletion.length() - 1;
            
            completions.push_back(comp);

        }

        // STEP 5: Score and Rank by Confidence
        // ====================================
        // Current: Single completion already scored
        // In production: Multiple completions would be sorted here
        std::sort(completions.begin(), completions.end(),
                 [](const CodeCompletion& a, const CodeCompletion& b) {
                     return a.confidence > b.confidence;
                 });

        // Limit to top completions
        if (completions.size() > 10) {
            completions.erase(completions.begin() + 10, completions.end());
        }

        m_metrics->recordHistogram("completions_per_call", completions.size());

        if (!completions.empty()) {
            m_metrics->recordHistogram("completion_confidence",
                                      completions[0].confidence * 100);
        }

        return completions;

    } catch (const std::exception&) {
        m_errorCount++;
        m_metrics->incrementCounter("completion_generation_errors");
        return {};
    }
}

std::string RealTimeCompletionEngine::buildCompletionPrompt(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& context) {

    // Using Fill-In-the-Middle (FIM) format common in code models (e.g. CodeLlama, StarCoder)
    // <PRE> prefix <SUF> suffix <MID>
    
    // 1. Inject Context if available (simplified RAG)
    std::string fullContext;
    if (m_executionStateProvider) {
        ExecutionStateSnapshot snapshot;
        if (m_executionStateProvider->getLatestSnapshot(snapshot)) {
            const std::string execSection = buildExecutionStateSection(snapshot);
            if (!execSection.empty()) {
                fullContext += execSection + "\n\n";
            }
        }
    }

    if (!context.empty()) {
        fullContext += "// Context:\n" + context + "\n\n";
    }

    // 2. Construct FIM prompt
    // Note: Tokens <PRE>, <SUF>, <MID> should ideally be special tokens, 
    // but here we use the string representation which many models support.
    std::ostringstream ss;
    ss << "<PRE> " << fullContext << prefix << " <SUF> " << suffix << " <MID>";
    
    return ss.str();
}

std::string RealTimeCompletionEngine::buildExecutionStateSection(const ExecutionStateSnapshot& snapshot) const {
    if (!snapshot.valid) {
        return {};
    }

    int64_t ageMs = -1;
    if (snapshot.capturedAtUnixMs > 0) {
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ageMs = nowMs - snapshot.capturedAtUnixMs;
        if (ageMs < 0 || ageMs > m_executionStateMaxAgeMs) {
            return {};
        }
    }

    std::ostringstream out;
    out << "[Execution State]\n";
    if (snapshot.capturedAtUnixMs > 0) {
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        out << "age_ms=" << (nowMs - snapshot.capturedAtUnixMs);
        if (!snapshot.threadId.empty()) {
            out << ", thread=" << snapshot.threadId;
        }
        out << "\n";
    }

    const std::string currentReality = buildCurrentRealityBlock(snapshot, ageMs);
    if (!currentReality.empty()) {
        out << currentReality;
    }

    const size_t stackLimit = std::min(snapshot.stackFrames.size(), static_cast<size_t>(std::max(0, m_maxExecutionStackFrames)));
    if (stackLimit > 0) {
        out << "stack_frames:\n";
        for (size_t i = 0; i < stackLimit; ++i) {
            out << "  #" << i << " " << snapshot.stackFrames[i] << "\n";
        }
    }

    const size_t regLimit = std::min(snapshot.registers.size(), static_cast<size_t>(std::max(0, m_maxExecutionRegisters)));
    if (regLimit > 0) {
        out << "registers:\n";
        for (size_t i = 0; i < regLimit; ++i) {
            out << "  " << snapshot.registers[i].first << "=" << snapshot.registers[i].second << "\n";
        }
    }

    if (!snapshot.notes.empty()) {
        std::string boundedNotes = snapshot.notes;
        if (m_maxExecutionNotesChars > 0 && boundedNotes.size() > static_cast<size_t>(m_maxExecutionNotesChars)) {
            boundedNotes.resize(static_cast<size_t>(m_maxExecutionNotesChars));
            boundedNotes += "...";
        }
        out << "notes: " << boundedNotes << "\n";
    }

    return out.str();
}

std::string RealTimeCompletionEngine::buildCurrentRealityBlock(const ExecutionStateSnapshot& snapshot, int64_t ageMs) const {
    std::ostringstream out;
    out << "current_reality:\n";

    if (!snapshot.threadId.empty()) {
        out << "- thread: " << snapshot.threadId << "\n";
    }
    if (ageMs >= 0) {
        out << "- freshness_ms: " << ageMs << "\n";
    }

    std::string ripValue;
    for (const auto& reg : snapshot.registers) {
        if (reg.first == "rip" || reg.first == "RIP") {
            ripValue = reg.second;
            break;
        }
    }
    if (!ripValue.empty()) {
        out << "- instruction_pointer: " << ripValue << "\n";
    }

    if (!snapshot.stackFrames.empty()) {
        out << "- top_frame: " << snapshot.stackFrames.front() << "\n";
    }

    return out.str();
}

std::vector<CodeCompletion> RealTimeCompletionEngine::postProcessCompletions(
    const std::string& modelOutput,
    const std::string& prefix) {

    // Filter and rank completions
    std::vector<CodeCompletion> processed;
    
    if (modelOutput.empty()) return processed;

    CodeCompletion comp;
    comp.text = modelOutput;
    
    // Heuristic: trimming common generation artifacts
    if (comp.text.find("<EOT>") != std::string::npos) {
        comp.text = comp.text.substr(0, comp.text.find("<EOT>"));
    }
    
    comp.insertTextLength = comp.text.length();
    comp.cursorOffset = comp.text.length(); // Move cursor to end
    comp.kind = "text"; 
    
    // Detect kind
    if (comp.text.find("(") != std::string::npos && comp.text.find(")") != std::string::npos) {
        comp.kind = "method";
    } else if (comp.text.find("class ") != std::string::npos) {
        comp.kind = "class";
    }
    
    comp.confidence = calculateConfidence(comp.text, prefix);
    
    // Basic filtering
    if (comp.confidence > 0.3) {
        processed.push_back(comp);
    }

    return processed;
}

double RealTimeCompletionEngine::calculateConfidence(
    const std::string& completion,
    const std::string& context) {

    double score = 0.5; // Base score

    // 1. Syntax heuristic: Does it end with a semicolon or brace?
    if (!completion.empty()) {
        char last = completion.back();
        if (last == ';' || last == '}' || last == ')') {
            score += 0.2;
        }
    }

    // 2. Context matching: Does it continue the indentation?
    // (Simplified check)
    if (context.size() > 0 && context.back() == ' ' && completion.size() > 0 && completion[0] != ' ') {
         // Context ends in space, completion starts with non-space -> good flow
         score += 0.1;
    }

    // 3. Length penalty (too short is often noise, too long might be hallucination)
    if (completion.length() < 2) score -= 0.1;
    if (completion.length() > 200) score -= 0.1;

    return std::min(1.0, std::max(0.0, score));
}

bool RealTimeCompletionEngine::shouldUseCache(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_completionCache.find(key) != m_completionCache.end();
}

void RealTimeCompletionEngine::updateCache(
    const std::string& key,
    const std::vector<CodeCompletion>& completions) {

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_completionCache[key] = completions;

    // LRU eviction if cache too large
    if (m_completionCache.size() > 1000) {
        auto it = m_completionCache.begin();
        m_completionCache.erase(it);
    }
}

std::string RealTimeCompletionEngine::generateCacheKey(
    const std::string& prefix,
    const std::string& suffix) {

    return prefix + "|" + suffix;
}

// ============================================================================
// Asynchronous API Implementations
// ============================================================================

std::future<std::vector<CodeCompletion>> RealTimeCompletionEngine::getCompletionsAsync(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& fileType,
    const std::string& context) {

    return std::async(std::launch::async, [this, prefix, suffix, fileType, context]() {
        return getCompletions(prefix, suffix, fileType, context);
    });
}

std::future<std::vector<CodeCompletion>> RealTimeCompletionEngine::getInlineCompletionsAsync(
    const std::string& currentLine,
    int cursorColumn,
    const std::string& filePath) {

    return std::async(std::launch::async, [this, currentLine, cursorColumn, filePath]() {
        return getInlineCompletions(currentLine, cursorColumn, filePath);
    });
}

std::future<std::vector<CodeCompletion>> RealTimeCompletionEngine::getMultiLineCompletionsAsync(
    const std::string& prefix,
    int maxLines) {

    return std::async(std::launch::async, [this, prefix, maxLines]() {
        return getMultiLineCompletions(prefix, maxLines);
    });
}

std::future<std::vector<CodeCompletion>> RealTimeCompletionEngine::getContextualCompletionsAsync(
    const std::string& filePath,
    int line,
    int column,
    const std::string& scope) {

    return std::async(std::launch::async, [this, filePath, line, column, scope]() {
        return getContextualCompletions(filePath, line, column, scope);
    });
}
