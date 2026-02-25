/**
 * @file ai_completion_provider.cpp
 * @brief AI-powered code completion provider (Ollama/local LLM)
 * 
 * @author RawrXD Team
 * @date 2026-01-07
 */

#include "ai_completion_provider.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

// Windows HTTP requirements
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

namespace RawrXD {

// Helper: String to WString
static std::wstring s2ws(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

AICompletionProvider::AICompletionProvider(void* parent)
{
    // Parent ignored in pure C++ version
}

AICompletionProvider::~AICompletionProvider()
{
    cancelPendingRequest();
}

void AICompletionProvider::setModelEndpoint(const std::string& endpoint) {
    m_modelEndpoint = endpoint;
}

void AICompletionProvider::setModel(const std::string& modelName) {
    m_modelName = modelName;
}

void AICompletionProvider::setRequestTimeout(int timeoutMs) {
    m_requestTimeoutMs = timeoutMs;
}

void AICompletionProvider::setTimeoutFallback(bool enabled) {
    m_useTimeoutFallback = enabled;
}

void AICompletionProvider::setMinConfidence(float threshold) {
    m_minConfidence = threshold;
    if (m_minConfidence < 0.0f) m_minConfidence = 0.0f;
    if (m_minConfidence > 1.0f) m_minConfidence = 1.0f;
}

void AICompletionProvider::setMaxSuggestions(int count) {
    m_maxSuggestions = count < 1 ? 1 : count;
}

void AICompletionProvider::requestCompletions(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& filePath,
    const std::string& fileType,
    const std::vector<std::string>& contextLines)
{
    if (m_isPending) {
        // Already pending
        return;
    }

    if (prefix.length() < 2 && prefix.find_first_not_of(" \t\n\r") == std::string::npos) {
        // Skipping short/empty prefix
        return;
    }

    m_isPending = true;
    m_lastPrefix = prefix;
    m_lastSuffix = suffix;
    m_lastFileType = fileType;
    m_lastContext = contextLines; // Store context

    // Launch async thread
    std::thread([this]() {
        this->onCompletionRequest();
    }).detach();
}

void AICompletionProvider::cancelPendingRequest() {
    m_isPending = false;
}

void AICompletionProvider::onCompletionRequest() {
    if (!m_isPending) return;

    auto start_time = std::chrono::steady_clock::now();

    try {
        std::string prompt = formatPrompt(m_lastPrefix, m_lastSuffix, m_lastFileType, m_lastContext);
        std::string response = callModel(prompt);

        auto end_time = std::chrono::steady_clock::now();
        m_lastLatencyMs = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        if (m_latencyCallback) m_latencyCallback(m_lastLatencyMs);

        std::vector<AICompletion> completions = parseCompletions(response);
        
        // Filter
        std::vector<AICompletion> filtered;
        for(const auto& c : completions) {
            if (c.confidence >= m_minConfidence) {
                filtered.push_back(c);
            }
        }
        if (filtered.size() > (size_t)m_maxSuggestions) {
            filtered.resize(m_maxSuggestions);
        }

        m_isPending = false;
        if (m_completionCallback) m_completionCallback(filtered);

    } catch (const std::exception& e) {
        m_isPending = false;
        if (m_errorCallback) m_errorCallback(e.what());
    }
}

std::string AICompletionProvider::formatPrompt(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& fileType,
    const std::vector<std::string>& contextLines)
{
    std::stringstream ss;
    ss << "Language: " << fileType << "\n";
    if (!contextLines.empty()) {
        ss << "Context:\n";
        for(const auto& line : contextLines) ss << line << "\n";
    }
    ss << "Current line: " << prefix << "\n";
    ss << "Complete the code:\n" << prefix;
    return ss.str();
}

float AICompletionProvider::extractConfidence(const std::string& suggestion) {
    // Heuristic confidence
    if (suggestion.empty()) return 0.0f;
    if (suggestion.length() < 3) return 0.6f;
    return 0.8f;
}

std::vector<AICompletion> AICompletionProvider::parseCompletions(const std::string& responseText) {
    std::vector<AICompletion> results;
    
    // Parse JSON response from Ollama
    try {
        auto j = json::parse(responseText);
        std::string text;
        if (j.contains("response")) {
            text = j["response"].get<std::string>();
        } else if (j.contains("content")) {
             text = j["content"].get<std::string>();
        }
        
        // Split into lines if multiple suggestions ?? 
        // Usually LLM gives one stream. Assuming text is the code.
        AICompletion c;
        c.text = text;
        c.type = "snippet";
        c.confidence = extractConfidence(text);
        results.push_back(c);

    } catch (...) {
        // Fallback for plain text
        AICompletion c;
        c.text = responseText; 
        c.type = "text";
        c.confidence = 0.5f;
        results.push_back(c);
    }
    return results;
}

std::string AICompletionProvider::callModel(const std::string& prompt) {
    // WinHttp Implementation for "No Stub" Requirement
    std::string result;
    
    // Parse URL (assuming m_modelEndpoint is simple like http://localhost:11434)
    // We need to extract hostname and port.
    std::wstring wUrl_full = s2ws(m_modelEndpoint);
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength    = (DWORD)-1;
    urlComp.dwHostNameLength  = (DWORD)-1;
    urlComp.dwUrlPathLength   = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl_full.c_str(), (DWORD)wUrl_full.length(), 0, &urlComp)) {
        throw std::runtime_error("Invalid URL");
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD-AI/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) throw std::runtime_error("WinHttpOpen failed");

    std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("WinHttpConnect failed");
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/generate", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("WinHttpOpenRequest failed");
    }

    // Prepare JSON body
    json jBody;
    jBody["model"] = m_modelName;
    jBody["prompt"] = prompt;
    jBody["stream"] = false;
    std::string sBody = jBody.dump();

    if (!WinHttpSendRequest(hRequest, L"Content-Type: application/json", -1L, (LPVOID)sBody.c_str(), (DWORD)sBody.length(), (DWORD)sBody.length(), 0)) {
         WinHttpCloseHandle(hRequest);
         WinHttpCloseHandle(hConnect);
         WinHttpCloseHandle(hSession);
         throw std::runtime_error("WinHttpSendRequest failed");
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
         WinHttpCloseHandle(hRequest);
         WinHttpCloseHandle(hConnect);
         WinHttpCloseHandle(hSession);
         throw std::runtime_error("WinHttpReceiveResponse failed");
    }

    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        
        std::vector<char> buffer(dwSize + 1);
        if (WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded)) {
            result.append(buffer.data(), dwDownloaded);
        }
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

std::string AICompletionProvider::generateFallbackCompletion(const std::string& prompt) {
    return "// AI Unavailable - Auto Fallback";
}

} // namespace RawrXD

