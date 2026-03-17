#include "CompletionEngine.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <regex>
#include <nlohmann/json.hpp>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

namespace RawrXD {
namespace IDE {

IntelligentCompletionEngine::IntelligentCompletionEngine()
    : m_confidenceThreshold(0.5f), m_maxSuggestions(10),
      m_modelUrl("http://localhost:11434"), m_embeddingUrl("http://localhost:11434") {
}

std::vector<CompletionSuggestion> IntelligentCompletionEngine::getCompletions(
    const CompletionContext& context, int maxSuggestions) {
    
    std::string cacheKey = context.filePath + ":" + context.linePrefix;
    
    // Check cache
    auto it = m_cache.find(cacheKey);
    if (it != m_cache.end()) {
        auto elapsed = std::chrono::system_clock::now() - it->second.timestamp;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 5) {
            return it->second.suggestions;
        }
    }
    
    // Get completions from model
    auto suggestions = inferCompletions(context);
    
    // Score and rank
    scoreCompletions(suggestions, context);
    
    // Sort by priority
    std::sort(suggestions.begin(), suggestions.end(),
        [](const CompletionSuggestion& a, const CompletionSuggestion& b) {
            return a.priority > b.priority;
        });
    
    // Limit results
    if (suggestions.size() > (size_t)maxSuggestions) {
        suggestions.resize(maxSuggestions);
    }
    
    // Cache result
    CachedCompletion cached{suggestions, std::chrono::system_clock::now()};
    m_cache[cacheKey] = cached;
    
    return suggestions;
}

std::vector<std::string> IntelligentCompletionEngine::getMultiLineCompletions(
    const CompletionContext& context, int maxLines) {
    
    std::vector<std::string> results;
    
    std::string prompt = "Complete this code with " + std::to_string(maxLines) +
                        " lines. Return ONLY code, no explanation.\n\n" +
                        context.contextBefore + "\n" + context.currentLine;
    
    // Call Ollama /api/generate for multi-line completion
    try {
        json payload = {
            {"model", "codellama"},
            {"prompt", prompt},
            {"stream", false},
            {"options", {
                {"temperature", 0.2},
                {"num_predict", maxLines * 40},
                {"stop", {"\n\n\n"}}
            }}
        };
        std::string payloadStr = payload.dump();

        HINTERNET hSession = WinHttpOpen(L"RawrXD-CompletionEngine/1.0",
                                          WINHTTP_ACCESS_TYPE_NO_PROXY,
                                          WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return results;

        HINTERNET hConnect = WinHttpConnect(hSession, L"localhost",
                                             11434, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return results; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
                                                 L"/api/generate",
                                                 nullptr, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return results;
        }

        WinHttpSetTimeouts(hRequest, 2000, 5000, 30000, 30000);

        LPCWSTR ct = L"Content-Type: application/json";
        if (!WinHttpSendRequest(hRequest, ct, -1L,
                                 (LPVOID)payloadStr.c_str(),
                                 (DWORD)payloadStr.size(),
                                 (DWORD)payloadStr.size(), 0) ||
            !WinHttpReceiveResponse(hRequest, nullptr)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            // Fallback to single-line
            auto single = getCompletions(context, 1);
            if (!single.empty()) results.push_back(single[0].text);
            return results;
        }

        std::string responseBody;
        DWORD bytesAvailable = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            std::vector<char> buf(bytesAvailable + 1, 0);
            DWORD bytesRead = 0;
            WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
            responseBody.append(buf.data(), bytesRead);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        // Parse response
        json resp = json::parse(responseBody, nullptr, false);
        if (!resp.is_discarded() && resp.contains("response")) {
            std::string generated = resp["response"].get<std::string>();
            // Split into lines
            std::istringstream iss(generated);
            std::string line;
            int count = 0;
            while (std::getline(iss, line) && count < maxLines) {
                results.push_back(line);
                count++;
            }
        }
    } catch (...) {
        // Fallback to single-line completions
        auto single = getCompletions(context, 1);
        if (!single.empty()) results.push_back(single[0].text);
    }
    
    return results;
}

bool IntelligentCompletionEngine::validateCompletion(
    const std::string& completion, const std::string& language, const std::string& context) {
    
    // Basic syntax validation
    if (completion.empty()) return false;
    
    // Check for balanced braces/parens
    int braceCount = 0, parenCount = 0, bracketCount = 0;
    for (char c : completion) {
        if (c == '{') braceCount++;
        else if (c == '}') braceCount--;
        else if (c == '(') parenCount++;
        else if (c == ')') parenCount--;
        else if (c == '[') bracketCount++;
        else if (c == ']') bracketCount--;
    }
    
    return braceCount >= 0 && parenCount >= 0 && bracketCount >= 0;
}

std::string IntelligentCompletionEngine::getCompletionDocumentation(
    const std::string& completion) {
    // Extract function signature and provide documentation
    // This would integrate with codebase analyzer
    return "Documentation for: " + completion;
}

std::vector<std::string> IntelligentCompletionEngine::getCompletionParams(
    const std::string& functionName) {
    // Extract parameters from function signature
    // This would use codebase analyzer
    return {"param1", "param2"};
}

void IntelligentCompletionEngine::setMinConfidenceThreshold(float threshold) {
    m_confidenceThreshold = threshold;
}

void IntelligentCompletionEngine::setMaxSuggestions(int count) {
    m_maxSuggestions = count;
}

void IntelligentCompletionEngine::setLanguageModelUrl(const std::string& url) {
    m_modelUrl = url;
}

void IntelligentCompletionEngine::setEmbeddingModelUrl(const std::string& url) {
    m_embeddingUrl = url;
}

std::vector<CompletionSuggestion> IntelligentCompletionEngine::inferCompletions(
    const CompletionContext& context) {
    
    std::vector<CompletionSuggestion> suggestions;
    
    // Build prompt for code completion
    std::string prompt = enrichContext(context);
    
    // Call Ollama /api/generate via WinHTTP
    try {
        json payload = {
            {"model", "codellama"},
            {"prompt", prompt},
            {"stream", false},
            {"options", {
                {"temperature", 0.3},
                {"num_predict", 64},
                {"top_p", 0.9}
            }}
        };
        std::string payloadStr = payload.dump();

        HINTERNET hSession = WinHttpOpen(L"RawrXD-CompletionEngine/1.0",
                                          WINHTTP_ACCESS_TYPE_NO_PROXY,
                                          WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return suggestions;

        HINTERNET hConnect = WinHttpConnect(hSession, L"localhost", 11434, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return suggestions; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
                                                 L"/api/generate",
                                                 nullptr, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return suggestions;
        }

        WinHttpSetTimeouts(hRequest, 2000, 5000, 15000, 15000);

        LPCWSTR ct = L"Content-Type: application/json";
        bool ok = WinHttpSendRequest(hRequest, ct, -1L,
                                      (LPVOID)payloadStr.c_str(),
                                      (DWORD)payloadStr.size(),
                                      (DWORD)payloadStr.size(), 0)
                  && WinHttpReceiveResponse(hRequest, nullptr);

        std::string responseBody;
        if (ok) {
            DWORD bytesAvailable = 0;
            while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
                std::vector<char> buf(bytesAvailable + 1, 0);
                DWORD bytesRead = 0;
                WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
                responseBody.append(buf.data(), bytesRead);
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (!responseBody.empty()) {
            json resp = json::parse(responseBody, nullptr, false);
            if (!resp.is_discarded() && resp.contains("response")) {
                std::string generated = resp["response"].get<std::string>();
                if (!generated.empty()) {
                    // Split by double-newline to get multiple suggestions,
                    // or treat the whole response as a single completion.
                    CompletionSuggestion s;
                    s.text = generated;
                    s.label = generated.substr(0, std::min((size_t)40, generated.size()));
                    s.confidence = 0.85f;
                    s.priority = 70;
                    s.kind = "snippet";
                    suggestions.push_back(s);
                }
            }
        }
    } catch (...) {
        // Model unavailable — fall through to heuristic fallback
    }

    // Heuristic fallback if no model response
    if (suggestions.empty()) {
        std::vector<std::string> fallbacks = {
            context.linePrefix + " = ",
            context.linePrefix + "()",
            "if (" + context.linePrefix + ") {\n}",
            "for (auto " + context.linePrefix + " : ",
        };
        
        for (const auto& comp : fallbacks) {
            CompletionSuggestion s;
            s.text = comp;
            s.label = comp.substr(0, 30);
            s.confidence = 0.3f;
            s.priority = 20;
            s.kind = "snippet";
            suggestions.push_back(s);
        }
    }
    
    return suggestions;
}

void IntelligentCompletionEngine::scoreCompletions(
    std::vector<CompletionSuggestion>& suggestions,
    const CompletionContext& context) {
    
    for (auto& sug : suggestions) {
        // Fuzzy match score
        float matchScore = fuzzyMatchScore(sug.text, context.linePrefix);
        
        // Boost by category
        if (sug.kind == "function") sug.priority += 20;
        else if (sug.kind == "class") sug.priority += 15;
        else if (sug.kind == "variable") sug.priority += 10;
        
        // Apply fuzzy score
        sug.priority = static_cast<int>(sug.priority * (0.5f + matchScore * 0.5f));
    }
}

float IntelligentCompletionEngine::fuzzyMatchScore(
    const std::string& suggestion, const std::string& userInput) {
    
    if (userInput.empty()) return 1.0f;
    if (suggestion.empty()) return 0.0f;
    
    size_t pos = suggestion.find(userInput);
    if (pos == std::string::npos) return 0.1f;
    
    // Boost for prefix match
    if (pos == 0) return 0.9f;
    
    // Partial match
    return 0.5f;
}

std::string IntelligentCompletionEngine::enrichContext(
    const CompletionContext& context) {
    
    std::stringstream ss;
    ss << "File: " << context.filePath << "\n";
    ss << "Language: " << context.fileLanguage << "\n";
    ss << "Context: \n" << context.contextBefore << "\n";
    ss << "Current: " << context.currentLine << "\n";
    ss << "Visible symbols: ";
    for (const auto& sym : context.visibleSymbols) {
        ss << sym << ", ";
    }
    ss << "\n";
    ss << "Complete the line.";
    
    return ss.str();
}

} // namespace IDE
} // namespace RawrXD
