#include "model_tester.h"
#include <sstream>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <ctime>

// Note: This implementation shows the framework structure.
// For full HTTP integration, link against libcurl:
// #include <curl/curl.h>

ModelTester::ModelTester(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics,
    std::shared_ptr<ResponseParser> parser)
    : m_logger(logger), m_metrics(metrics), m_parser(parser) {
    if (m_logger) m_logger->info("ModelTester initialized");
}

ModelTestResult ModelTester::testWithOllama(
    const std::string& modelName,
    const std::string& prompt,
    int maxTokens) {

    if (m_logger) m_logger->info("Testing model: {} with prompt: {}...", modelName, prompt.substr(0, 50));

    auto startTime = std::chrono::high_resolution_clock::now();

    ModelTestResult result;
    result.modelName = modelName;
    result.prompt = prompt;

    try {
        // Construct Ollama request payload
        // Format: {"model": "llama2", "prompt": "...", "stream": true, "temperature": 0.7}
        std::ostringstream payload;
        payload << "{\n"
                << "  \"model\": \"" << modelName << "\",\n"
                << "  \"prompt\": \"" << prompt << "\",\n"
                << "  \"stream\": true,\n"
                << "  \"temperature\": 0.7,\n"
                << "  \"top_p\": 0.9,\n"
                << "  \"num_predict\": " << maxTokens << "\n"
                << "}";

        if (m_logger) m_logger->debug("Sending request to Ollama API");

        // Make HTTP request to Ollama
        std::string response = makeOllamaRequest("/api/generate", payload.str());

        auto firstTokenTime = std::chrono::high_resolution_clock::now();
        result.timeToFirstTokenUs = std::chrono::duration_cast<std::chrono::microseconds>(
            firstTokenTime - startTime).count();

        if (m_logger) m_logger->debug("Received response: {} chars", response.length());

        // Parse response
        result.response = parseOllamaStreamingResponse(response);
        
        // Parse completions
        auto completions = m_parser->parseResponse(result.response);
        result.completionCount = completions.size();

        // Quality assessment
        result.responseQuality = scoreResponseQuality(result.response);
        result.tokenCount = m_parser->ResponseParser::estimateTokenCount(result.response);
        result.parseSuccessful = !completions.empty();

        // Latency calculations
        auto endTime = std::chrono::high_resolution_clock::now();
        result.totalLatencyUs = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime).count();
        
        if (result.tokenCount > 0) {
            result.avgTokenLatencyUs = result.totalLatencyUs / static_cast<double>(result.tokenCount);
        }

        // Record timestamp
        auto now = std::time(nullptr);
        auto tm = std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
        result.timestamp = oss.str();

        // Store result
        m_testResults.push_back(result);
        m_latencyHistory[modelName].push_back(result.totalLatencyUs);

        // Record metrics
        if (m_metrics) {
            m_metrics->recordHistogram("model_test_latency_us", result.totalLatencyUs);
            m_metrics->recordHistogram("model_test_tokens", result.tokenCount);
            m_metrics->recordHistogram("model_test_quality", result.responseQuality * 100);
        }

        if (m_logger) m_logger->info("Test complete: {} tokens in {} us (avg {} us/token, quality: {:.2f}%)",
                       result.tokenCount, result.totalLatencyUs, 
                       static_cast<int>(result.avgTokenLatencyUs),
                       result.responseQuality * 100);

    } catch (const std::exception& e) {
        if (m_logger) m_logger->error("Test failed: {}", e.what());
        result.responseQuality = 0.0;
        result.parseSuccessful = false;
    }

    return result;
}

std::vector<LatencyBenchmark> ModelTester::benchmarkModels(
    const std::vector<std::string>& modelNames,
    const std::vector<std::string>& testPrompts,
    int runsPerModel) {

    if (m_logger) m_logger->info("Starting benchmark: {} models x {} prompts x {} runs",
                   modelNames.size(), testPrompts.size(), runsPerModel);

    std::vector<LatencyBenchmark> results;

    for (const auto& model : modelNames) {
        if (m_logger) m_logger->info("Benchmarking model: {}", model);

        std::vector<int64_t> allLatencies;

        for (int run = 0; run < runsPerModel; run++) {
            for (const auto& prompt : testPrompts) {
                auto testResult = testWithOllama(model, prompt, 50);
                allLatencies.push_back(testResult.totalLatencyUs);
            }
        }

        if (!allLatencies.empty()) {
            // Calculate statistics
            std::sort(allLatencies.begin(), allLatencies.end());

            double sumLatency = std::accumulate(allLatencies.begin(), allLatencies.end(), 0.0);
            double avgLatency = sumLatency / allLatencies.size() / 1000.0; // Convert to ms

            LatencyBenchmark bench;
            bench.modelName = model;
            bench.totalRequests = allLatencies.size();
            bench.avgLatencyMs = avgLatency;
            bench.minLatencyMs = allLatencies.front() / 1000.0;
            bench.maxLatencyMs = allLatencies.back() / 1000.0;
            bench.p50LatencyMs = calculatePercentile(allLatencies, 50.0) / 1000.0;
            bench.p95LatencyMs = calculatePercentile(allLatencies, 95.0) / 1000.0;
            bench.p99LatencyMs = calculatePercentile(allLatencies, 99.0) / 1000.0;
            
            // Assume average 50 tokens generated, calculate throughput
            double avgTokens = 50.0;
            bench.throughputTokensPerSecond = (1000.0 / avgLatency) * avgTokens;

            results.push_back(bench);

            if (m_logger) {
                m_logger->info("Benchmark results for {}: avg={:.2f}ms, p95={:.2f}ms, p99={:.2f}ms",
                               model, bench.avgLatencyMs,
                               bench.p95LatencyMs, bench.p99LatencyMs);
            }
        }
    }

    return results;
}

std::vector<ParsedCompletion> ModelTester::testResponseParsing(const std::string& modelOutput) {
    if (m_logger) m_logger->debug("Testing response parsing on {} chars", modelOutput.length());

    auto completions = m_parser->parseResponse(modelOutput);

    if (m_logger) {
        m_logger->info("Parsed {} completions, confidence scores: {}",
                       completions.size(),
                       completions.empty() ? "N/A" : std::to_string(completions[0].confidence));
    }

    return completions;
}

LatencyBenchmark ModelTester::measureLatencyDistribution(
    const std::string& modelName,
    int testCount) {

    if (m_logger) m_logger->info("Measuring latency distribution for {} ({} requests)", modelName, testCount);

    std::vector<std::string> testPrompts = {
        "print('hello')",
        "function add(a, b) {",
        "const x = 42;",
        "for i in range(10):",
        "SELECT * FROM users WHERE"
    };

    std::vector<int64_t> latencies;

    for (int i = 0; i < testCount; i++) {
        const auto& prompt = testPrompts[i % testPrompts.size()];
        auto result = testWithOllama(modelName, prompt, 30);
        latencies.push_back(result.totalLatencyUs);
    }

    std::sort(latencies.begin(), latencies.end());

    LatencyBenchmark bench;
    bench.modelName = modelName;
    bench.totalRequests = testCount;
    bench.avgLatencyMs = std::accumulate(latencies.begin(), latencies.end(), 0.0) / 
                         (testCount * 1000.0);
    bench.minLatencyMs = latencies.front() / 1000.0;
    bench.maxLatencyMs = latencies.back() / 1000.0;
    bench.p50LatencyMs = calculatePercentile(latencies, 50.0) / 1000.0;
    bench.p95LatencyMs = calculatePercentile(latencies, 95.0) / 1000.0;
    bench.p99LatencyMs = calculatePercentile(latencies, 99.0) / 1000.0;

    return bench;
}

bool ModelTester::validateModelResponse(
    const std::string& modelName,
    const std::string& prompt) {

    if (m_logger) m_logger->debug("Validating model response for: {}", modelName);

    auto result = testWithOllama(modelName, prompt, 50);

    bool isValid = result.parseSuccessful && 
                   !result.response.empty() && 
                   result.responseQuality > 0.5;

    if (m_logger) m_logger->info("Validation result: {}", isValid ? "PASS" : "FAIL");

    return isValid;
}

std::string ModelTester::generateTestReport() const {
    std::ostringstream report;

    report << "=== Model Testing Report ===\n";
    report << "Total Tests: " << m_testResults.size() << "\n\n";

    // Group by model
    std::map<std::string, std::vector<const ModelTestResult*>> resultsByModel;
    for (const auto& result : m_testResults) {
        resultsByModel[result.modelName].push_back(&result);
    }

    for (const auto& [modelName, results] : resultsByModel) {
        report << "Model: " << modelName << "\n";
        report << "  Tests: " << results.size() << "\n";

        // Calculate statistics
        std::vector<double> qualities;
        std::vector<int64_t> latencies;

        for (const auto* result : results) {
            qualities.push_back(result->responseQuality);
            latencies.push_back(result->totalLatencyUs);
        }

        std::sort(latencies.begin(), latencies.end());

        double avgQuality = std::accumulate(qualities.begin(), qualities.end(), 0.0) / 
                           qualities.size();
        double avgLatency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / 
                           (latencies.size() * 1000.0); // ms

        report << "  Quality (avg): " << std::fixed << std::setprecision(2) 
               << (avgQuality * 100) << "%\n";
        report << "  Latency (avg): " << avgLatency << " ms\n";
        report << "  Latency (min): " << (latencies.front() / 1000.0) << " ms\n";
        report << "  Latency (max): " << (latencies.back() / 1000.0) << " ms\n";
        report << "\n";
    }

    return report.str();
}

std::string ModelTester::exportToJSON() const {
    std::ostringstream json;

    json << "{\n";
    json << "  \"testCount\": " << m_testResults.size() << ",\n";
    json << "  \"results\": [\n";

    for (size_t i = 0; i < m_testResults.size(); i++) {
        const auto& result = m_testResults[i];
        json << "    {\n";
        json << "      \"model\": \"" << result.modelName << "\",\n";
        json << "      \"prompt\": \"" << result.prompt << "\",\n";
        json << "      \"tokens\": " << result.tokenCount << ",\n";
        json << "      \"latencyMs\": " << (result.totalLatencyUs / 1000.0) << ",\n";
        json << "      \"quality\": " << result.responseQuality << ",\n";
        json << "      \"timestamp\": \"" << result.timestamp << "\"\n";
        json << "    }";
        if (i < m_testResults.size() - 1) json << ",";
        json << "\n";
    }

    json << "  ]\n";
    json << "}\n";

    return json.str();
}

void ModelTester::resetResults() {
    if (m_logger) m_logger->info("Resetting test results");
    m_testResults.clear();
    m_latencyHistory.clear();
}

std::string ModelTester::makeOllamaRequest(
    const std::string& endpoint,
    const std::string& payload) {

    // Implementation requires libcurl
    // For now, return placeholder response
    // In production, would use curl_easy_perform()

    if (m_logger) m_logger->debug("Making Ollama request to {} with {} chars payload", 
                    endpoint, payload.length());

    // Placeholder: Simulate a response
    // Real implementation would make actual HTTP call to http://localhost:11434{endpoint}
    
    std::string response = R"(
{
  "model": "llama2",
  "created_at": "2024-12-12T10:00:00Z",
  "response": "function add(a, b) {\n  return a + b;\n}",
  "done": true,
  "total_duration": 1234567,
  "load_duration": 123456,
  "prompt_eval_count": 10,
  "prompt_eval_duration": 234567,
  "eval_count": 15,
  "eval_duration": 876543
}
)";

    if (m_logger) m_logger->debug("Received response: {} chars", response.length());
    return response;
}

std::string ModelTester::parseOllamaStreamingResponse(const std::string& streamResponse) {
    // In streaming mode, Ollama returns multiple JSON objects separated by newlines
    // Each object has a "response" field with partial text

    std::ostringstream result;
    std::istringstream stream(streamResponse);
    std::string line;

    while (std::getline(stream, line)) {
        // Simple extraction: look for "response": "..."
        size_t pos = line.find("\"response\":");
        if (pos != std::string::npos) {
            pos = line.find("\"", pos + 11); // Find opening quote
            if (pos != std::string::npos) {
                size_t endPos = line.find("\"", pos + 1);
                if (endPos != std::string::npos) {
                    result << line.substr(pos + 1, endPos - pos - 1);
                }
            }
        }
    }

    return result.str();
}

double ModelTester::calculatePercentile(
    const std::vector<int64_t>& latencies,
    double percentile) const {

    if (latencies.empty()) return 0.0;

    size_t index = static_cast<size_t>((percentile / 100.0) * latencies.size());
    if (index >= latencies.size()) {
        index = latencies.size() - 1;
    }

    return latencies[index];
}

double ModelTester::scoreResponseQuality(const std::string& response) const {
    if (response.empty()) return 0.0;

    double score = 0.5; // Base score

    // Bonus for reasonable length
    if (response.length() > 10 && response.length() < 1000) {
        score += 0.15;
    }

    // Bonus for code-like patterns
    if (response.find('{') != std::string::npos || 
        response.find('}') != std::string::npos ||
        response.find('(') != std::string::npos) {
        score += 0.20;
    }

    // Bonus for newlines (multi-line code)
    if (response.find('\n') != std::string::npos) {
        score += 0.10;
    }

    // Penalty for too many repeated characters
    int maxRepeats = 0;
    int currentRepeats = 1;
    for (size_t i = 1; i < response.length(); i++) {
        if (response[i] == response[i-1]) {
            currentRepeats++;
            maxRepeats = std::max(maxRepeats, currentRepeats);
        } else {
            currentRepeats = 1;
        }
    }

    if (maxRepeats > 5) {
        score -= 0.20;
    }

    return std::max(0.0, std::min(score, 1.0));
}
