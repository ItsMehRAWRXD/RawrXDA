#include "MLInferenceEngine.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <windows.h>

using json = nlohmann::json;

namespace RawrXD::ML {

// Static callback for CURL writes
static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

MLInferenceEngine& MLInferenceEngine::getInstance() {
    static MLInferenceEngine instance;
    return instance;
}

bool MLInferenceEngine::initialize() {
    if (m_initialized) return true;

    // Test connection to RawrEngine
    if (!connectToRawrEngine()) {
        return false;
    }

    m_initialized = true;
    m_sessionStart = std::chrono::steady_clock::now();
    return true;
}

bool MLInferenceEngine::connectToRawrEngine() {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string response;
    std::string statusUrl = m_engineEndpoint + "/api/status";

    curl_easy_setopt(curl, CURLOPT_URL, statusUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK && !response.empty());
}

std::string MLInferenceEngine::buildJSONRequest(const std::string& prompt, size_t maxTokens) {
    json req;
    req["prompt"] = prompt;
    req["n_tokens"] = maxTokens;
    req["temperature"] = 0.7;
    req["top_p"] = 0.9;
    req["stream"] = true;  // Enable token streaming

    return req.dump();
}

std::vector<std::string> MLInferenceEngine::parseTokensFromResponse(const std::string& response) {
    std::vector<std::string> tokens;

    // Parse JSON streaming format (newline-delimited JSON)
    std::istringstream stream(response);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        try {
            auto lineJson = json::parse(line);
            if (lineJson.contains("token")) {
                tokens.push_back(lineJson["token"].get<std::string>());
            }
        } catch (...) {
            // Skip malformed lines
        }
    }

    return tokens;
}

MLInferenceEngine::InferenceResult MLInferenceEngine::query(
    const std::string& prompt,
    std::function<void(const std::string& token)> onTokenCallback,
    size_t maxTokens)
{
    auto startTime = std::chrono::steady_clock::now();
    InferenceResult result{.success = false};

    if (!m_initialized && !initialize()) {
        result.response = "ERROR: RawrEngine not available on localhost:23959";
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.response = "ERROR: Failed to initialize CURL";
        return result;
    }

    std::string jsonRequest = buildJSONRequest(prompt, maxTokens);
    std::string responseStr;

    std::string inferUrl = m_engineEndpoint + "/api/infer";
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: RawrXD-ML/1.0");

    curl_easy_setopt(curl, CURLOPT_URL, inferUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonRequest.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);

    CURLcode res = curl_easy_perform(curl);

    auto ttfFirstToken = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    if (res == CURLE_OK) {
        result.success = true;
        result.response = responseStr;

        // Parse tokens and invoke callback
        auto tokens = parseTokensFromResponse(responseStr);
        result.tokenCount = tokens.size();

        for (const auto& token : tokens) {
            if (onTokenCallback) {
                onTokenCallback(token);
            }
        }

        // Record telemetry
        auto endTime = std::chrono::steady_clock::now();
        m_lastTelemetry.ttfFirstToken = ttfFirstToken;
        m_lastTelemetry.ttfCompletionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        m_lastTelemetry.tokenCount = result.tokenCount;
        m_lastTelemetry.completionTokens = result.tokenCount;
        m_lastTelemetry.tokensPerSecond = 
            (m_lastTelemetry.ttfCompletionTime > 0) 
            ? (float)result.tokenCount * 1000.0f / m_lastTelemetry.ttfCompletionTime
            : 0.0f;
        m_lastTelemetry.status = "success";

    } else {
        result.response = "ERROR: HTTP request failed: " + std::string(curl_easy_strerror(res));
        m_lastTelemetry.status = "error";
    }

    // Set prompt info
    m_lastTelemetry.prompt = prompt;
    m_lastTelemetry.promptTokens = prompt.length() / 4;  // Rough estimate

    // Timestamp
    auto now = std::time(nullptr);
    auto tm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
    m_lastTelemetry.timestamp = oss.str();

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    result.latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    return result;
}

std::string MLInferenceEngine::telemetryToJSON() const {
    json telemetry;
    telemetry["timestamp"] = m_lastTelemetry.timestamp;
    telemetry["prompt"] = m_lastTelemetry.prompt;
    telemetry["promptTokens"] = m_lastTelemetry.promptTokens;
    telemetry["completionTokens"] = m_lastTelemetry.completionTokens;
    telemetry["ttfFirstToken"] = m_lastTelemetry.ttfFirstToken;
    telemetry["ttfCompletion"] = m_lastTelemetry.ttfCompletionTime;
    telemetry["tokensPerSecond"] = m_lastTelemetry.tokensPerSecond;
    telemetry["status"] = m_lastTelemetry.status;

    return telemetry.dump(4);
}

std::string MLInferenceEngine::resultToJSON(const InferenceResult& result) const {
    json output;
    output["success"] = result.success;
    output["response"] = result.response;
    output["tokenCount"] = result.tokenCount;
    output["latencyMs"] = result.latencyMs;
    output["telemetry"] = json::parse(telemetryToJSON());

    return output.dump(4);
}

void MLInferenceEngine::shutdown() {
    m_initialized = false;
}

}
