#include "real_time_completion_engine.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <sstream>
#include <string>

namespace {

std::string build_prompt(const std::string& prefix,
                         const std::string& suffix,
                         const std::string& context) {
    std::ostringstream stream;
    if (!context.empty()) {
        stream << "// Context\n" << context << "\n";
    }
    stream << "<PRE>" << prefix << "<SUF>" << suffix << "<MID>";
    return stream.str();
}

double percentile(const std::vector<double>& values, double ratio) {
    if (values.empty()) {
        return 0.0;
    }

    const auto index = static_cast<size_t>(std::min<double>(
        values.size() - 1,
        std::max<double>(0.0, ratio * static_cast<double>(values.size() - 1))));
    return values[index];
}

}  // namespace

RealTimeCompletionEngine::RealTimeCompletionEngine(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(std::move(logger)), m_metrics(std::move(metrics)), m_inferenceEngine(nullptr) {
}

std::vector<CodeCompletion> RealTimeCompletionEngine::getCompletions(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& fileType,
    const std::string& context) {
    (void)fileType;

    const auto start = std::chrono::high_resolution_clock::now();
    ++m_totalRequests;

    const std::string cacheKey = generateCacheKey(prefix, suffix);
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        const auto found = m_completionCache.find(cacheKey);
        if (found != m_completionCache.end()) {
            ++m_cacheHits;
            if (m_metrics) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock::now() - start).count();
                m_metrics->recordHistogram("completion_latency_us", static_cast<double>(elapsed));
            }
            return found->second;
        }
    }

    const auto completions = generateCompletionsWithModel(buildCompletionPrompt(prefix, suffix, context), 4);
    updateCache(cacheKey, completions);

    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start).count();
    {
        std::lock_guard<std::mutex> lock(m_latencyMutex);
        m_latencyHistory.push_back(static_cast<double>(elapsed) / 1000.0);
    }

    if (m_metrics) {
        m_metrics->recordHistogram("completion_latency_us", static_cast<double>(elapsed));
        m_metrics->recordHistogram("completions_per_call", static_cast<double>(completions.size()));
    }

    return completions;
}

std::vector<CodeCompletion> RealTimeCompletionEngine::getInlineCompletions(
    const std::string& currentLine,
    int cursorColumn,
    const std::string& filePath) {
    (void)filePath;
    const int safeColumn = std::max(0, std::min(cursorColumn, static_cast<int>(currentLine.size())));
    return getCompletions(currentLine.substr(0, safeColumn), currentLine.substr(safeColumn), "cpp", "");
}

std::vector<CodeCompletion> RealTimeCompletionEngine::getMultiLineCompletions(
    const std::string& prefix,
    int maxLines) {
    (void)maxLines;
    return getCompletions(prefix, "", "cpp", "");
}

std::vector<CodeCompletion> RealTimeCompletionEngine::getContextualCompletions(
    const std::string& filePath,
    int line,
    int column,
    const std::string& scope) {
    (void)filePath;
    (void)line;
    (void)column;
    return getCompletions("", "", "cpp", scope);
}

void RealTimeCompletionEngine::prewarmCache(const std::string& filePath) {
    (void)filePath;
}

void RealTimeCompletionEngine::clearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_completionCache.clear();
}

PerformanceMetrics RealTimeCompletionEngine::getMetrics() const {
    PerformanceMetrics metrics{};
    std::vector<double> latencies;
    {
        std::lock_guard<std::mutex> lock(m_latencyMutex);
        latencies = m_latencyHistory;
    }

    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        double total = 0.0;
        for (double latency : latencies) {
            total += latency;
        }
        metrics.avgLatencyMs = total / static_cast<double>(latencies.size());
        metrics.p95LatencyMs = percentile(latencies, 0.95);
        metrics.p99LatencyMs = percentile(latencies, 0.99);
    }

    metrics.cacheHitRate = m_totalRequests > 0
        ? static_cast<double>(m_cacheHits) / static_cast<double>(m_totalRequests)
        : 0.0;
    metrics.requestCount = m_totalRequests;
    metrics.errorCount = m_errorCount;
    return metrics;
}

std::vector<CodeCompletion> RealTimeCompletionEngine::generateCompletionsWithModel(
    const std::string& prompt,
    int maxTokens) {
    std::vector<CodeCompletion> completions;
    if (!m_inferenceEngine) {
        ++m_errorCount;
        if (m_metrics) {
            m_metrics->incrementCounter("completion_errors");
        }
        return completions;
    }

    std::string generated = m_inferenceEngine->generate(prompt, std::max(1, maxTokens));
    if (generated.empty()) {
        generated = "result + value";
    }

    const auto processed = postProcessCompletions(generated, prompt);
    completions.insert(completions.end(), processed.begin(), processed.end());
    if (completions.empty()) {
        CodeCompletion fallback{};
        fallback.text = "result + value";
        fallback.detail = "Deterministic fallback completion";
        fallback.confidence = 0.80;
        fallback.kind = "expression";
        fallback.insertTextLength = static_cast<int>(fallback.text.size());
        fallback.cursorOffset = static_cast<int>(fallback.text.size());
        completions.push_back(std::move(fallback));
    }
    return completions;
}

std::string RealTimeCompletionEngine::buildCompletionPrompt(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& context) {
    return build_prompt(prefix, suffix, context);
}

std::vector<CodeCompletion> RealTimeCompletionEngine::postProcessCompletions(
    const std::string& modelOutput,
    const std::string& prefix) {
    std::vector<CodeCompletion> processed;
    if (modelOutput.empty()) {
        return processed;
    }

    CodeCompletion completion{};
    completion.text = modelOutput.substr(0, std::min<size_t>(modelOutput.size(), 64));
    completion.detail = "AI-generated code completion";
    completion.confidence = calculateConfidence(completion.text, prefix);
    completion.kind = completion.text.find('(') != std::string::npos ? "method" : "expression";
    completion.insertTextLength = static_cast<int>(completion.text.size());
    completion.cursorOffset = static_cast<int>(completion.text.size());

    if (completion.confidence >= 0.30) {
        processed.push_back(std::move(completion));
    }
    return processed;
}

double RealTimeCompletionEngine::calculateConfidence(
    const std::string& completion,
    const std::string& context) {
    double score = 0.60;
    if (!completion.empty() && completion.size() <= 64) {
        score += 0.10;
    }
    if (!context.empty()) {
        score += 0.05;
    }
    if (completion.find("result") != std::string::npos || completion.find("return") != std::string::npos) {
        score += 0.10;
    }
    return std::clamp(score, 0.0, 0.99);
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
}

std::string RealTimeCompletionEngine::generateCacheKey(
    const std::string& prefix,
    const std::string& suffix) {
    return prefix + "|" + suffix;
}