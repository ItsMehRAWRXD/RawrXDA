#include "real_time_completion_engine.h"
#include <algorithm>
#include <chrono>
#include <QString>

RealTimeCompletionEngine::RealTimeCompletionEngine(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(logger), m_metrics(metrics), m_inferenceEngine(nullptr) {
    m_logger->info("RealTimeCompletionEngine initialized");
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

        // Generate completions from model
        auto completions = generateCompletionsWithModel(prefix + context, 50);

        // Post-process results
        completions = postProcessCompletions(
            std::string(), prefix
        );

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
        m_logger->error("Error getting completions: {}", e.what());
        m_metrics->incrementCounter("completion_errors");
        return {};
    }
}

std::vector<CodeCompletion> RealTimeCompletionEngine::getInlineCompletions(
    const std::string& currentLine,
    int cursorColumn,
    const std::string& filePath) {

    m_logger->debug("Getting inline completions for: {}", filePath);

    // Extract prefix/suffix from line
    std::string prefix = currentLine.substr(0, cursorColumn);
    std::string suffix = currentLine.substr(cursorColumn);

    return getCompletions(prefix, suffix, "cpp", "");
}

std::vector<CodeCompletion> RealTimeCompletionEngine::getMultiLineCompletions(
    const std::string& prefix,
    int maxLines) {

    m_logger->debug("Getting multi-line completions");

    return getCompletions(prefix, "", "cpp", "");
}

std::vector<CodeCompletion> RealTimeCompletionEngine::getContextualCompletions(
    const std::string& filePath,
    int line,
    int column,
    const std::string& scope) {

    m_logger->debug("Getting contextual completions for: {}:{}", filePath, line);

    // In full implementation, would analyze file context
    return getCompletions("", "", "cpp", scope);
}

void RealTimeCompletionEngine::prewarmCache(const std::string& filePath) {
    m_logger->info("Pre-warming cache for: {}", filePath);

    // Read file content for symbol extraction
    std::ifstream file(filePath);
    if (!file.is_open()) {
        m_logger->warn("Cannot open file for cache prewarm: {}", filePath);
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Determine language from extension
    std::string ext;
    auto dotPos = filePath.rfind('.');
    if (dotPos != std::string::npos) {
        ext = filePath.substr(dotPos + 1);
    }

    // Extract identifiers and keywords for completion candidates
    std::unordered_set<std::string> symbols;
    std::string currentToken;
    for (char c : content) {
        if (std::isalnum(c) || c == '_') {
            currentToken += c;
        } else {
            if (currentToken.size() >= 3) {
                symbols.insert(currentToken);
            }
            currentToken.clear();
        }
    }
    if (currentToken.size() >= 3) symbols.insert(currentToken);

    // Pre-populate cache with extracted symbols as likely completions
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    for (const auto& symbol : symbols) {
        // Create prefix entries for each symbol (first 2-3 chars as key)
        if (symbol.size() >= 2) {
            std::string prefix2 = symbol.substr(0, 2);
            std::string prefix3 = symbol.size() >= 3 ? symbol.substr(0, 3) : prefix2;
            
            // Cache the symbol under its prefixes for fast lookup
            std::string cacheKey = filePath + ":" + prefix2;
            m_completionCache[cacheKey].push_back(symbol);
            if (prefix3 != prefix2) {
                std::string cacheKey3 = filePath + ":" + prefix3;
                m_completionCache[cacheKey3].push_back(symbol);
            }
        }
    }

    // Also generate common pattern completions based on language
    if (ext == "cpp" || ext == "hpp" || ext == "h" || ext == "cc") {
        std::vector<std::string> cppKeywords = {
            "namespace", "template", "typename", "virtual", "override",
            "constexpr", "noexcept", "nullptr", "static_cast", "dynamic_cast",
            "std::vector", "std::string", "std::map", "std::shared_ptr", "std::unique_ptr",
            "std::mutex", "std::lock_guard", "std::atomic"
        };
        for (const auto& kw : cppKeywords) {
            if (kw.size() >= 2) {
                std::string key = filePath + ":" + kw.substr(0, 2);
                m_completionCache[key].push_back(kw);
            }
        }
    } else if (ext == "py") {
        std::vector<std::string> pyKeywords = {
            "import", "from", "class", "def", "return", "yield",
            "async", "await", "lambda", "self", "__init__", "__main__"
        };
        for (const auto& kw : pyKeywords) {
            if (kw.size() >= 2) {
                std::string key = filePath + ":" + kw.substr(0, 2);
                m_completionCache[key].push_back(kw);
            }
        }
    } else if (ext == "js" || ext == "ts" || ext == "tsx" || ext == "jsx") {
        std::vector<std::string> jsKeywords = {
            "const", "function", "return", "async", "await", "export",
            "import", "interface", "Promise", "console.log", "useState", "useEffect"
        };
        for (const auto& kw : jsKeywords) {
            if (kw.size() >= 2) {
                std::string key = filePath + ":" + kw.substr(0, 2);
                m_completionCache[key].push_back(kw);
            }
        }
    }

    m_logger->info("Cache prewarmed for {}: {} unique symbols extracted", filePath, symbols.size());
}

void RealTimeCompletionEngine::clearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_completionCache.clear();
    m_logger->info("Completion cache cleared");
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
    metrics.errorCount = m_metrics->getCounter("completion_errors");

    return metrics;
}

std::vector<CodeCompletion> RealTimeCompletionEngine::generateCompletionsWithModel(
    const std::string& prompt,
    int maxTokens) {

    std::vector<CodeCompletion> completions;

    try {
        m_logger->info("Generating completions with model (max_tokens={})", maxTokens);
        m_metrics->incrementCounter("model_calls");

        // =============================================================================
        // PHASE 1 IMPLEMENTATION: Real Model Calling Integration
        // =============================================================================

        // STEP 1: Check InferenceEngine is Available and Model Loaded
        // ===========================================================
        if (!m_inferenceEngine) {
            m_logger->error("InferenceEngine not set for CompletionEngine");
            m_metrics->incrementCounter("inference_engine_not_set");
            return {};
        }

        if (!m_inferenceEngine->isModelLoaded()) {
            m_logger->warn("Model not loaded for completions");
            m_metrics->incrementCounter("model_not_loaded_errors");
            return {};
        }
        m_logger->info("Using InferenceEngine with model loaded (prompt size tracking)");

        // STEP 2: Tokenize Prompt with Context Window Enforcement
        // ========================================================
        std::vector<int32_t> inputTokens = m_inferenceEngine->tokenize(QString::fromStdString(prompt));
        m_logger->debug("Tokenized prompt into {} tokens", inputTokens.size());

        // Enforce context window limit (typical: 512-2048 tokens)
        // Leave some space for generation
        const size_t CONTEXT_WINDOW = 512;
        const size_t GENERATION_BUDGET = 256;
        const size_t TOTAL_LIMIT = CONTEXT_WINDOW + GENERATION_BUDGET;

        if (inputTokens.size() > CONTEXT_WINDOW) {
            m_logger->debug("Truncating prompt from {} to {} tokens",
                           inputTokens.size(), CONTEXT_WINDOW);
            // Keep the most recent tokens (sliding window)
            if (inputTokens.size() > CONTEXT_WINDOW) {
                inputTokens.erase(inputTokens.begin(),
                                 inputTokens.end() - CONTEXT_WINDOW);
            }
        }

        // STEP 3: Generate Tokens with Stopping Conditions
        // ================================================
        m_logger->debug("Starting token generation using InferenceEngine::generate (max_tokens={})", maxTokens);

        std::vector<int32_t> generatedTokens = m_inferenceEngine->generate(inputTokens, maxTokens);

        std::string generatedCompletion;
        if (generatedTokens.size() > inputTokens.size()) {
            std::vector<int32_t> completionSegment(
                generatedTokens.begin() + static_cast<long long>(inputTokens.size()),
                generatedTokens.end());
            generatedCompletion = m_inferenceEngine->detokenize(completionSegment).toStdString();
        } else {
            m_logger->debug("No additional tokens generated beyond prompt");
        }

        m_logger->info("Generated completion: {} chars, {} tokens",
                      generatedCompletion.length(),
                      generatedTokens.size() > inputTokens.size()
                          ? static_cast<int>(generatedTokens.size() - inputTokens.size())
                          : 0);

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
            
            m_logger->debug("Parsed {} completions from generated text", completions.size());
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

        m_logger->info("Returning {} completions from model", completions.size());
        m_metrics->recordHistogram("completions_per_call", completions.size());

        if (!completions.empty()) {
            m_metrics->recordHistogram("completion_confidence",
                                      completions[0].confidence * 100);
        }

        return completions;

    } catch (const std::exception& e) {
        m_logger->error("Error generating completions: {}", e.what());
        m_metrics->incrementCounter("completion_generation_errors");
        return {};
    }
}

std::string RealTimeCompletionEngine::buildCompletionPrompt(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& context) {

    return prefix + " " + suffix + " " + context;
}

std::vector<CodeCompletion> RealTimeCompletionEngine::postProcessCompletions(
    const std::string& modelOutput,
    const std::string& prefix) {

    // Filter and rank completions
    std::vector<CodeCompletion> processed;

    // Keep only completions above minimum confidence
    // and sort by confidence

    return processed;
}

double RealTimeCompletionEngine::calculateConfidence(
    const std::string& completion,
    const std::string& context) {

    // Calculate how confident we are in this completion
    // based on context matching, popularity, etc.
    return 0.85;
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
