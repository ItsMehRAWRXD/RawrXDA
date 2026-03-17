#include "real_time_completion_engine.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <regex>
#include <map>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

RealTimeCompletionEngine::RealTimeCompletionEngine(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(logger), m_metrics(metrics) {
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
                m_logger->debug("Cache hit for completion");
                return it->second;
            }
        }

        m_logger->debug("Cache miss - generating new completions");

        // Build completion prompt with context
        std::string prompt = buildCompletionPrompt(prefix, suffix, context);

        // Generate completions from model (REAL CALL)
        auto completions = generateCompletionsWithModel(prompt, 256);

        // Post-process results
        completions = postProcessCompletions(prefix, prefix);

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

    // Read file and extract common patterns for prewarming
    std::ifstream file(filePath);
    if (!file.is_open()) {
        m_logger->warn("Cannot open file for cache prewarming: {}", filePath);
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Extract identifiers that appear frequently (likely to be completed)
    std::map<std::string, int> identifiers;
    std::regex idPattern(R"(\b([a-zA-Z_]\w{2,})\b)");
    auto begin = std::sregex_iterator(content.begin(), content.end(), idPattern);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        identifiers[(*it)[1].str()]++;
    }

    // Pre-populate cache with completions for common prefixes
    std::vector<std::pair<std::string, int>> sorted(identifiers.begin(), identifiers.end());
    std::sort(sorted.begin(), sorted.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });

    int prewarmed = 0;
    for (const auto& [id, count] : sorted) {
        if (count < 3 || id.length() < 3) continue;
        if (prewarmed >= 50) break; // Limit prewarming

        // Create cache entries for partial prefixes
        for (size_t len = 2; len <= std::min(id.length(), (size_t)5); ++len) {
            std::string prefix = id.substr(0, len);
            std::string cacheKey = generateCacheKey(prefix, "");
            
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            if (m_completionCache.find(cacheKey) == m_completionCache.end()) {
                CodeCompletion comp;
                comp.text = id;
                comp.detail = "Identifier from project";
                comp.confidence = 0.7 + (count / 100.0);
                comp.kind = "identifier";
                comp.insertTextLength = static_cast<int>(id.length());
                m_completionCache[cacheKey] = {comp};
            }
        }
        prewarmed++;
    }

    m_logger->info("Pre-warmed {} identifiers into cache", prewarmed);
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

        // Build Ollama /api/generate JSON request
        std::string jsonBody = "{\"model\":\"" + m_modelName + "\","
            "\"prompt\":" + [&]() -> std::string {
                // JSON-escape the prompt
                std::string escaped;
                escaped += '"';
                for (char c : prompt) {
                    switch (c) {
                        case '"':  escaped += "\\\""; break;
                        case '\\': escaped += "\\\\"; break;
                        case '\n': escaped += "\\n"; break;
                        case '\r': escaped += "\\r"; break;
                        case '\t': escaped += "\\t"; break;
                        default:   escaped += c;
                    }
                }
                escaped += '"';
                return escaped;
            }() + ","
            "\"stream\":false,"
            "\"options\":{\"temperature\":0.2,\"num_predict\":" + std::to_string(maxTokens) + ","
            "\"top_p\":0.9,\"stop\":[\"\\n\\n\",\"```\"]}}";

        // WinHTTP request to Ollama on localhost:11434
        HINTERNET hSession = WinHttpOpen(L"RawrXD-CompletionEngine/1.0",
            WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        
        if (!hSession) {
            m_logger->error("WinHttpOpen failed: {}", GetLastError());
            m_metrics->incrementCounter("model_call_errors");
            return {};
        }

        HINTERNET hConnect = WinHttpConnect(hSession, L"localhost",
            static_cast<INTERNET_PORT>(m_ollamaPort > 0 ? m_ollamaPort : 11434), 0);
        
        if (!hConnect) {
            m_logger->error("WinHttpConnect failed: {}", GetLastError());
            WinHttpCloseHandle(hSession);
            m_metrics->incrementCounter("model_call_errors");
            return {};
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
            L"/api/generate", NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        
        if (!hRequest) {
            m_logger->error("WinHttpOpenRequest failed: {}", GetLastError());
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            m_metrics->incrementCounter("model_call_errors");
            return {};
        }

        // Set timeout (completion should be fast - 30 seconds max)
        DWORD timeout = 30000;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

        // Send request
        const wchar_t* contentType = L"Content-Type: application/json";
        BOOL bResults = WinHttpSendRequest(hRequest,
            contentType, -1L,
            (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.size(),
            (DWORD)jsonBody.size(), 0);

        if (!bResults) {
            m_logger->error("WinHttpSendRequest failed: {}", GetLastError());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            m_metrics->incrementCounter("model_call_errors");
            return {};
        }

        bResults = WinHttpReceiveResponse(hRequest, NULL);
        if (!bResults) {
            m_logger->error("WinHttpReceiveResponse failed: {}", GetLastError());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            m_metrics->incrementCounter("model_call_errors");
            return {};
        }

        // Read response body
        std::string responseBody;
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        
        do {
            dwSize = 0;
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (dwSize == 0) break;
            
            std::vector<char> buffer(dwSize + 1, 0);
            WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded);
            responseBody.append(buffer.data(), dwDownloaded);
        } while (dwSize > 0);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        // Parse JSON response to extract "response" field
        std::string generatedText;
        size_t respPos = responseBody.find("\"response\":\"");
        if (respPos != std::string::npos) {
            size_t start = respPos + 12; // length of "response":"
            size_t end = start;
            while (end < responseBody.size()) {
                if (responseBody[end] == '"' && responseBody[end - 1] != '\\') break;
                end++;
            }
            generatedText = responseBody.substr(start, end - start);
            
            // Unescape JSON string
            std::string unescaped;
            for (size_t i = 0; i < generatedText.size(); ++i) {
                if (generatedText[i] == '\\' && i + 1 < generatedText.size()) {
                    switch (generatedText[i + 1]) {
                        case 'n': unescaped += '\n'; ++i; break;
                        case 't': unescaped += '\t'; ++i; break;
                        case 'r': unescaped += '\r'; ++i; break;
                        case '"': unescaped += '"'; ++i; break;
                        case '\\': unescaped += '\\'; ++i; break;
                        default: unescaped += generatedText[i];
                    }
                } else {
                    unescaped += generatedText[i];
                }
            }
            generatedText = unescaped;
        }

        if (generatedText.empty()) {
            m_logger->warn("Empty response from model, response body: {}",
                responseBody.substr(0, 200));
            m_metrics->incrementCounter("model_empty_responses");
            return {};
        }

        m_logger->info("Model generated {} chars of completion text", generatedText.size());

        // Post-process into structured completions
        completions = postProcessCompletions(generatedText, prompt);

        m_logger->info("Generated {} completions from model output", completions.size());
        m_metrics->recordHistogram("completions_per_call", completions.size());
        
        if (!completions.empty()) {
            m_metrics->recordHistogram("completion_confidence", 
                                      completions[0].confidence * 100);
        }

        return completions;

    } catch (const std::exception& e) {
        m_logger->error("Error generating completions with model: {}", e.what());
        m_metrics->incrementCounter("model_call_errors");
        return {};
    }
}

std::string RealTimeCompletionEngine::buildCompletionPrompt(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& context) {

    // Build a well-structured prompt for the model
    std::string prompt = "You are an expert code completion assistant.\n";
    prompt += "Complete the code based on context. Return only code.\n\n";
    
    if (!context.empty()) {
        prompt += "Context:\n";
        prompt += context;
        prompt += "\n\n";
    }
    
    prompt += "Code:\n";
    prompt += prefix;
    prompt += " /* COMPLETE */";
    prompt += suffix;
    prompt += "\n";
    
    return prompt;
}

std::vector<CodeCompletion> RealTimeCompletionEngine::postProcessCompletions(
    const std::string& modelOutput,
    const std::string& prefix) {

    std::vector<CodeCompletion> processed;

    if (modelOutput.empty()) return processed;

    // Parse model output into individual completion candidates
    // Split by newlines, semicolons, or code blocks
    std::vector<std::string> candidates;
    std::istringstream iss(modelOutput);
    std::string line;
    std::string currentCandidate;
    
    while (std::getline(iss, line)) {
        // Skip empty lines and comments
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) {
            if (!currentCandidate.empty()) {
                candidates.push_back(currentCandidate);
                currentCandidate.clear();
            }
            continue;
        }
        
        std::string trimmed = line.substr(first);
        if (trimmed.substr(0, 2) == "//" || trimmed.substr(0, 2) == "/*") continue;
        if (trimmed.substr(0, 3) == "```") continue;
        
        if (!currentCandidate.empty()) currentCandidate += "\n";
        currentCandidate += line;
        
        // Complete statement ends with ; or { or }
        if (trimmed.back() == ';' || trimmed.back() == '{' || trimmed.back() == '}') {
            candidates.push_back(currentCandidate);
            currentCandidate.clear();
        }
    }
    if (!currentCandidate.empty()) {
        candidates.push_back(currentCandidate);
    }

    // Score and create CodeCompletion objects
    for (const auto& candidate : candidates) {
        if (candidate.empty()) continue;
        
        CodeCompletion comp;
        comp.text = candidate;
        comp.confidence = calculateConfidence(candidate, prefix);
        comp.insertTextLength = static_cast<int>(candidate.size());
        
        // Determine completion kind
        if (candidate.find('(') != std::string::npos) {
            comp.kind = "function";
            comp.detail = "Function call";
        } else if (candidate.find('=') != std::string::npos) {
            comp.kind = "variable";
            comp.detail = "Assignment";
        } else if (candidate.find("class ") != std::string::npos || 
                   candidate.find("struct ") != std::string::npos) {
            comp.kind = "class";
            comp.detail = "Type definition";
        } else {
            comp.kind = "snippet";
            comp.detail = "Code snippet";
        }
        
        // Filter by minimum confidence
        if (comp.confidence >= (m_minConfidence / 100.0)) {
            processed.push_back(comp);
        }
    }

    // Sort by confidence
    std::sort(processed.begin(), processed.end(),
             [](const CodeCompletion& a, const CodeCompletion& b) {
                 return a.confidence > b.confidence;
             });

    // Limit to top 10
    if (processed.size() > 10) {
        processed.resize(10);
    }

    return processed;
}

double RealTimeCompletionEngine::calculateConfidence(
    const std::string& completion,
    const std::string& context) {

    double confidence = 0.5;

    // Bonus for common patterns
    if (completion.find("push_back") != std::string::npos) confidence += 0.15;
    if (completion.find("size()") != std::string::npos) confidence += 0.10;
    if (completion.find("=") != std::string::npos) confidence += 0.05;
    
    // Penalty for long completions (less likely to be correct)
    if (completion.length() > 100) confidence -= 0.15;

    return std::min(1.0, std::max(0.0, confidence));
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

    m_metrics->recordHistogram("cache_size", m_completionCache.size());

    // LRU eviction if cache too large
    if (m_completionCache.size() > 1000) {
        auto it = m_completionCache.begin();
        m_completionCache.erase(it);
        m_metrics->incrementCounter("cache_evictions");
    }
}

std::string RealTimeCompletionEngine::generateCacheKey(
    const std::string& prefix,
    const std::string& suffix) {

    // Create a hash-based key for faster comparison
    // In production, would use proper hashing
    return prefix.substr(0, 50) + "|" + suffix.substr(0, 50);
}
