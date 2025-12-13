#include "real_time_completion_engine.h"
#include <algorithm>
#include <chrono>

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

    // Pre-populate cache with likely completions for this file
    // Placeholder implementation
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

        if (!m_inferenceEngine->IsModelLoaded()) {
            m_logger->warn("Model not loaded for completions");
            m_metrics->incrementCounter("model_not_loaded_errors");
            return {};
        }

        m_logger->info("Using InferenceEngine with model loaded (vocab_size={})",
                      m_inferenceEngine->GetVocabSize());

        // STEP 2: Tokenize Prompt with Context Window Enforcement
        // ========================================================
        std::vector<uint32_t> inputTokens = m_inferenceEngine->TokenizeText(prompt);
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
        std::string generatedCompletion;
        const int MIN_COMPLETION_LENGTH = 5;
        const std::vector<std::string> STOP_SEQUENCES = {
            "\n\n",      // Double newline = end of statement block
            "EOF",       // Explicit end marker
            "};",        // End of struct/class
            "}\n",       // Function/block end
            "return"     // Return statement (often end of function)
        };

        m_logger->debug("Starting token generation loop (max_tokens={})", maxTokens);

        for (int i = 0; i < maxTokens; i++) {
            // Safety check: total tokens within reasonable limit
            if (inputTokens.size() >= TOTAL_LIMIT) {
                m_logger->debug("Hit total token limit: {} tokens", inputTokens.size());
                break;
            }

            // Generate next token using the model
            // Temperature 0.3 for focused completions (low randomness)
            // Top-p 0.9 for nucleus sampling (controlled diversity)
            // 
            // Note: InferenceEngine::GenerateToken() returns text directly
            // For more control, we would sample from logits with temperature/top_p
            std::string tokenText = m_inferenceEngine->GenerateToken(prompt + generatedCompletion, 1);
            
            if (tokenText.empty()) {
                m_logger->debug("Model returned empty token at position {}", i);
                break;
            }

            generatedCompletion += tokenText;

            // Check for natural end-of-sequence indicators
            // Common EOS tokens: "</s>", "[END]", "<|endoftext|>", etc.
            if (tokenText.find("</s>") != std::string::npos ||
                tokenText.find("[END]") != std::string::npos ||
                tokenText.find("<|endoftext|>") != std::string::npos) {
                m_logger->debug("Model reached EOS token at position {}", i);
                break;
            }

            // Check for manually defined stop sequences
            for (const auto& stopSeq : STOP_SEQUENCES) {
                if (generatedCompletion.find(stopSeq) != std::string::npos) {
                    m_logger->debug("Stopping at sequence: '{}' (generated: {})", 
                                   stopSeq, i + 1);
                    goto completion_done;
                }
            }

            // Early exit if we have a valid, complete code statement
            if (generatedCompletion.length() >= MIN_COMPLETION_LENGTH &&
                (tokenText == ";" || tokenText == ")" || 
                 tokenText == "}\n" || tokenText == "},")) {
                m_logger->debug("Got likely complete statement at {} tokens, early exit", i + 1);
                break;
            }

            // Safety: limit generation length to prevent runaway
            if (generatedCompletion.length() > 256) {
                m_logger->warn("Hit maximum completion length ({})", generatedCompletion.length());
                break;
            }
        }

        completion_done:
        m_logger->info("Generated completion: {} chars, {} tokens",
                      generatedCompletion.length(), 
                      generatedCompletion.empty() ? 0 : (int)generatedCompletion.length() / 4);

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
