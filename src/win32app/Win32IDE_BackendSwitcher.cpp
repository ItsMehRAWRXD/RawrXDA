// ============================================================================
// Win32IDE_BackendSwitcher.cpp — Phase 8B: AI Backend Switcher
// ============================================================================
// Runtime switching between inference backends:
//   - LocalGGUF  : Native RawrXD CPU inference engine
//   - Ollama     : Ollama HTTP API (local or remote)
//   - OpenAI     : OpenAI Chat Completions API
//   - Claude     : Anthropic Messages API
//   - Gemini     : Google Generative Language API
//
// Guardrails:
//   - No history mutation on switch
//   - No session invalidation on switch
//   - No auto-routing (explicit user selection only)
// ============================================================================

#include "Win32IDE.h"
#include <winhttp.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <algorithm>

// nlohmann/json already included via Win32IDE.h

// ============================================================================
// INITIALIZATION & LIFECYCLE
// ============================================================================

void Win32IDE::initBackendManager() {
    if (m_backendManagerInitialized) return;

    logFunction("initBackendManager");

    // ---- Populate default configs for each backend type --------------------
    auto& local  = m_backendConfigs[(size_t)AIBackendType::LocalGGUF];
    local.type      = AIBackendType::LocalGGUF;
    local.name      = "Local GGUF";
    local.endpoint  = "";       // No endpoint — uses native engine directly
    local.model     = "";       // Determined by loaded model file
    local.apiKey    = "";
    local.enabled   = true;
    local.timeoutMs = 60000;    // Local can be slow on large models
    local.maxTokens = 2048;
    local.temperature = 0.7f;

    auto& ollama = m_backendConfigs[(size_t)AIBackendType::Ollama];
    ollama.type      = AIBackendType::Ollama;
    ollama.name      = "Ollama";
    ollama.endpoint  = "http://localhost:11434";
    ollama.model     = "llama3.2";
    ollama.apiKey    = "";
    ollama.enabled   = true;
    ollama.timeoutMs = 30000;
    ollama.maxTokens = 2048;
    ollama.temperature = 0.7f;

    auto& openai = m_backendConfigs[(size_t)AIBackendType::OpenAI];
    openai.type      = AIBackendType::OpenAI;
    openai.name      = "OpenAI";
    openai.endpoint  = "https://api.openai.com";
    openai.model     = "gpt-4o";
    openai.apiKey    = "";
    openai.enabled   = false;   // Disabled until API key set
    openai.timeoutMs = 30000;
    openai.maxTokens = 4096;
    openai.temperature = 0.7f;

    auto& claude = m_backendConfigs[(size_t)AIBackendType::Claude];
    claude.type      = AIBackendType::Claude;
    claude.name      = "Claude";
    claude.endpoint  = "https://api.anthropic.com";
    claude.model     = "claude-sonnet-4-20250514";
    claude.apiKey    = "";
    claude.enabled   = false;   // Disabled until API key set
    claude.timeoutMs = 30000;
    claude.maxTokens = 4096;
    claude.temperature = 0.7f;

    auto& gemini = m_backendConfigs[(size_t)AIBackendType::Gemini];
    gemini.type      = AIBackendType::Gemini;
    gemini.name      = "Gemini";
    gemini.endpoint  = "https://generativelanguage.googleapis.com";
    gemini.model     = "gemini-2.0-flash";
    gemini.apiKey    = "";
    gemini.enabled   = false;   // Disabled until API key set
    gemini.timeoutMs = 30000;
    gemini.maxTokens = 4096;
    gemini.temperature = 0.7f;

    // ---- Initialize statuses -----------------------------------------------
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        m_backendStatuses[i].type         = (AIBackendType)i;
        m_backendStatuses[i].connected    = false;
        m_backendStatuses[i].healthy      = false;
        m_backendStatuses[i].latencyMs    = -1;
        m_backendStatuses[i].requestCount = 0;
        m_backendStatuses[i].failureCount = 0;
        m_backendStatuses[i].lastError    = "";
        m_backendStatuses[i].lastModel    = "";
        m_backendStatuses[i].lastUsedEpochMs = 0;
    }

    // Mark local as connected by default (it's always available if model loaded)
    m_backendStatuses[(size_t)AIBackendType::LocalGGUF].connected = true;
    m_backendStatuses[(size_t)AIBackendType::LocalGGUF].healthy   = (m_nativeEngine && m_nativeEngine->IsModelLoaded());

    // ---- Load saved configs (overrides defaults) ---------------------------
    loadBackendConfigs();

    m_activeBackend = AIBackendType::LocalGGUF;
    m_backendManagerInitialized = true;

    logInfo("[BackendSwitcher] Initialized — active backend: " + getActiveBackendName());
}

void Win32IDE::shutdownBackendManager() {
    if (!m_backendManagerInitialized) return;
    logFunction("shutdownBackendManager");
    saveBackendConfigs();
    m_backendManagerInitialized = false;
}

// ============================================================================
// CONFIG PERSISTENCE (JSON)
// ============================================================================

std::string Win32IDE::getBackendConfigFilePath() const {
    // Store next to session file
    std::string dir = getSessionFilePath();
    // Replace session.json with backends.json
    size_t pos = dir.find_last_of("/\\");
    if (pos != std::string::npos) {
        dir = dir.substr(0, pos + 1) + "backends.json";
    } else {
        dir = "backends.json";
    }
    return dir;
}

void Win32IDE::loadBackendConfigs() {
    std::string path = getBackendConfigFilePath();
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        logInfo("[BackendSwitcher] No saved config at " + path + " — using defaults");
        return;
    }

    try {
        std::string fileContent((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
        nlohmann::json j = nlohmann::json::parse(fileContent);

        if (j.contains("active")) {
            std::string activeName = j["active"].get<std::string>();
            m_activeBackend = backendTypeFromString(activeName);
        }

        if (j.contains("backends") && j["backends"].is_array()) {
            for (size_t bi = 0; bi < j["backends"].size(); ++bi) {
                nlohmann::json bj = j["backends"][bi];
                std::string typeName = bj.contains("type") ? bj["type"].get<std::string>() : "";
                AIBackendType bt = backendTypeFromString(typeName);
                if (bt == AIBackendType::Count) continue; // unknown type

                size_t idx = (size_t)bt;
                auto& cfg = m_backendConfigs[idx];
                cfg.type      = bt;
                if (bj.contains("name"))        cfg.name      = bj["name"].get<std::string>();
                if (bj.contains("endpoint"))    cfg.endpoint  = bj["endpoint"].get<std::string>();
                if (bj.contains("model"))       cfg.model     = bj["model"].get<std::string>();
                if (bj.contains("apiKey"))      cfg.apiKey    = bj["apiKey"].get<std::string>();
                if (bj.contains("enabled"))     cfg.enabled   = bj["enabled"].get<bool>();
                if (bj.contains("timeoutMs"))   cfg.timeoutMs = bj["timeoutMs"].get<int>();
                if (bj.contains("maxTokens"))   cfg.maxTokens = bj["maxTokens"].get<int>();
                if (bj.contains("temperature")) cfg.temperature = bj["temperature"].get<float>();
            }
        }

        logInfo("[BackendSwitcher] Loaded configs from " + path);
    } catch (const std::exception& e) {
        logError("loadBackendConfigs", std::string("JSON parse error: ") + e.what());
    }
}

void Win32IDE::saveBackendConfigs() {
    std::string path = getBackendConfigFilePath();

    // Ensure parent directory exists
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    nlohmann::json j;
    j["active"] = backendTypeString(m_activeBackend);

    nlohmann::json arr = nlohmann::json::array();
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        const auto& cfg = m_backendConfigs[i];
        nlohmann::json bj;
        bj["type"]        = backendTypeString(cfg.type);
        bj["name"]        = cfg.name;
        bj["endpoint"]    = cfg.endpoint;
        bj["model"]       = cfg.model;
        // NOTE: API keys are stored in plaintext — user-local config only
        bj["apiKey"]      = cfg.apiKey;
        bj["enabled"]     = cfg.enabled;
        bj["timeoutMs"]   = cfg.timeoutMs;
        bj["maxTokens"]   = cfg.maxTokens;
        bj["temperature"] = cfg.temperature;
        arr.push_back(bj);
    }
    j["backends"] = arr;

    std::ofstream ofs(path);
    if (ofs.is_open()) {
        ofs << j.dump(2);
        logInfo("[BackendSwitcher] Saved configs to " + path);
    } else {
        logError("saveBackendConfigs", "Failed to write " + path);
    }
}

// ============================================================================
// CORE SWITCHING LOGIC
// ============================================================================

bool Win32IDE::setActiveBackend(AIBackendType type) {
    if (type >= AIBackendType::Count) {
        logError("setActiveBackend", "Invalid backend type: " + std::to_string((int)type));
        return false;
    }

    std::lock_guard<std::mutex> lock(m_backendMutex);

    const auto& cfg = m_backendConfigs[(size_t)type];
    if (!cfg.enabled) {
        logWarning("setActiveBackend", "Backend '" + cfg.name + "' is disabled — enable it first");
        appendToOutput("[BackendSwitcher] Cannot switch to '" + cfg.name + "' — backend is disabled.\n"
                       "  Configure it via 'AI: Configure Backend' or set an API key first.",
                       "General", OutputSeverity::Warning);
        return false;
    }

    AIBackendType previous = m_activeBackend;
    m_activeBackend = type;

    // Update status bar to reflect new backend
    updateStatusBarBackend();

    // Persist the choice
    saveBackendConfigs();

    std::string msg = "[BackendSwitcher] Switched from '" + backendTypeString(previous) +
                      "' to '" + backendTypeString(type) + "' (model: " + cfg.model + ")";
    logInfo(msg);
    appendToOutput(msg, "General", OutputSeverity::Info);

    // NOTE: No history mutation. No session invalidation. No auto-routing.
    // The user explicitly chose this backend; we honor it.

    return true;
}

Win32IDE::Win32IDE::AIBackendType Win32IDE::getActiveBackendType() const {
    return m_activeBackend;
}

const Win32IDE::AIBackendConfig& Win32IDE::getActiveBackendConfig() const {
    return m_backendConfigs[(size_t)m_activeBackend];
}

std::string Win32IDE::getActiveBackendName() const {
    return m_backendConfigs[(size_t)m_activeBackend].name;
}

// ============================================================================
// LISTING & INSPECTION
// ============================================================================

std::vector<Win32IDE::AIBackendConfig> Win32IDE::listBackends() const {
    std::vector<AIBackendConfig> result;
    result.reserve((size_t)AIBackendType::Count);
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        result.push_back(m_backendConfigs[i]);
    }
    return result;
}

Win32IDE::AIBackendConfig Win32IDE::getBackendConfig(AIBackendType type) const {
    if (type >= AIBackendType::Count) return AIBackendConfig{};
    return m_backendConfigs[(size_t)type];
}

Win32IDE::AIBackendStatus Win32IDE::getBackendStatus(AIBackendType type) const {
    if (type >= AIBackendType::Count) return AIBackendStatus{};
    return m_backendStatuses[(size_t)type];
}

std::string Win32IDE::getBackendStatusString() const {
    std::ostringstream ss;
    ss << "[BackendSwitcher] Status:\n";
    ss << "  Active: " << backendTypeString(m_activeBackend) << "\n";
    ss << "  ──────────────────────────────────────\n";

    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        const auto& cfg = m_backendConfigs[i];
        const auto& st  = m_backendStatuses[i];
        bool isActive = ((AIBackendType)i == m_activeBackend);

        ss << "  " << (isActive ? "▶ " : "  ") << cfg.name;
        ss << " [" << (cfg.enabled ? "enabled" : "DISABLED") << "]";
        ss << " — " << (st.healthy ? "healthy" : "unhealthy");
        if (st.latencyMs >= 0) ss << " (" << st.latencyMs << "ms)";
        ss << "  reqs=" << st.requestCount << " fails=" << st.failureCount;
        if (!st.lastError.empty()) ss << "  err=\"" << st.lastError.substr(0, 80) << "\"";
        ss << "\n";
        ss << "     endpoint: " << (cfg.endpoint.empty() ? "(native)" : cfg.endpoint) << "\n";
        ss << "     model:    " << (cfg.model.empty() ? "(loaded file)" : cfg.model) << "\n";
    }
    return ss.str();
}

// ============================================================================
// CONFIGURATION MUTATIONS
// ============================================================================

void Win32IDE::setBackendEndpoint(AIBackendType type, const std::string& endpoint) {
    if (type >= AIBackendType::Count) return;
    std::lock_guard<std::mutex> lock(m_backendMutex);
    m_backendConfigs[(size_t)type].endpoint = endpoint;
    logInfo("[BackendSwitcher] Set endpoint for " + backendTypeString(type) + " → " + endpoint);
}

void Win32IDE::setBackendModel(AIBackendType type, const std::string& model) {
    if (type >= AIBackendType::Count) return;
    std::lock_guard<std::mutex> lock(m_backendMutex);
    m_backendConfigs[(size_t)type].model = model;
    logInfo("[BackendSwitcher] Set model for " + backendTypeString(type) + " → " + model);
}

void Win32IDE::setBackendApiKey(AIBackendType type, const std::string& apiKey) {
    if (type >= AIBackendType::Count) return;
    std::lock_guard<std::mutex> lock(m_backendMutex);
    m_backendConfigs[(size_t)type].apiKey = apiKey;
    // Auto-enable if a key is provided for remote backends
    if (!apiKey.empty() && type != AIBackendType::LocalGGUF) {
        m_backendConfigs[(size_t)type].enabled = true;
    }
    logInfo("[BackendSwitcher] API key updated for " + backendTypeString(type));
}

void Win32IDE::setBackendEnabled(AIBackendType type, bool enabled) {
    if (type >= AIBackendType::Count) return;
    std::lock_guard<std::mutex> lock(m_backendMutex);
    m_backendConfigs[(size_t)type].enabled = enabled;
    logInfo("[BackendSwitcher] " + backendTypeString(type) + " → " + (enabled ? "enabled" : "disabled"));
}

void Win32IDE::setBackendTimeout(AIBackendType type, int timeoutMs) {
    if (type >= AIBackendType::Count) return;
    std::lock_guard<std::mutex> lock(m_backendMutex);
    m_backendConfigs[(size_t)type].timeoutMs = timeoutMs;
}

// ============================================================================
// HEALTH PROBING
// ============================================================================

bool Win32IDE::probeBackendHealth(AIBackendType type) {
    if (type >= AIBackendType::Count) return false;

    auto startTime = std::chrono::steady_clock::now();
    bool healthy = false;
    std::string error;

    switch (type) {
        case AIBackendType::LocalGGUF: {
            healthy = (m_nativeEngine && m_nativeEngine->IsModelLoaded());
            if (!healthy) error = "No model loaded";
            break;
        }
        case AIBackendType::Ollama: {
            // Probe /api/tags endpoint
            const auto& cfg = m_backendConfigs[(size_t)type];
            try {
                std::string resp = httpPost(cfg.endpoint + "/api/tags", "", {}, 5000);
                healthy = !resp.empty() && resp.find("models") != std::string::npos;
                if (!healthy) error = "Ollama not responding or no models available";
            } catch (...) {
                error = "Connection failed to " + cfg.endpoint;
            }
            break;
        }
        case AIBackendType::OpenAI: {
            const auto& cfg = m_backendConfigs[(size_t)type];
            healthy = !cfg.apiKey.empty();
            if (!healthy) error = "No API key configured";
            else {
                try {
                    std::string resp = httpPost(cfg.endpoint + "/v1/models", "",
                        {"Authorization: Bearer " + cfg.apiKey}, 5000);
                    healthy = !resp.empty() && resp.find("data") != std::string::npos;
                    if (!healthy) error = "OpenAI API returned unexpected response";
                } catch (...) {
                    error = "Connection failed to OpenAI";
                }
            }
            break;
        }
        case AIBackendType::Claude: {
            const auto& cfg = m_backendConfigs[(size_t)type];
            healthy = !cfg.apiKey.empty();
            if (!healthy) error = "No API key configured";
            // Anthropic doesn't have a lightweight health endpoint; we trust the key
            break;
        }
        case AIBackendType::Gemini: {
            const auto& cfg = m_backendConfigs[(size_t)type];
            healthy = !cfg.apiKey.empty();
            if (!healthy) error = "No API key configured";
            break;
        }
        default:
            break;
    }

    auto endTime = std::chrono::steady_clock::now();
    int latencyMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    onBackendHealthResult(type, healthy, latencyMs, error);
    return healthy;
}

void Win32IDE::probeAllBackendsAsync() {
    std::thread([this]() {
        for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
            if (m_backendConfigs[i].enabled) {
                probeBackendHealth((AIBackendType)i);
            }
        }
    }).detach();
}

void Win32IDE::onBackendHealthResult(AIBackendType type, bool healthy, int latencyMs, const std::string& error) {
    if (type >= AIBackendType::Count) return;
    std::lock_guard<std::mutex> lock(m_backendMutex);

    auto& st        = m_backendStatuses[(size_t)type];
    st.connected    = healthy;
    st.healthy      = healthy;
    st.latencyMs    = latencyMs;
    st.lastError    = error;
}

// ============================================================================
// INFERENCE ROUTING
// ============================================================================

std::string Win32IDE::routeInferenceRequest(const std::string& prompt) {
    AIBackendType active;
    {
        std::lock_guard<std::mutex> lock(m_backendMutex);
        active = m_activeBackend;
    }

    std::string result;
    auto startTime = std::chrono::steady_clock::now();
    bool success = false;

    switch (active) {
        case AIBackendType::LocalGGUF:
            result = routeToLocalGGUF(prompt);
            break;
        case AIBackendType::Ollama:
            result = routeToOllama(prompt);
            break;
        case AIBackendType::OpenAI:
            result = routeToOpenAI(prompt);
            break;
        case AIBackendType::Claude:
            result = routeToClaude(prompt);
            break;
        case AIBackendType::Gemini:
            result = routeToGemini(prompt);
            break;
        default:
            result = "[BackendSwitcher] Unknown active backend";
            break;
    }

    auto endTime = std::chrono::steady_clock::now();
    int latencyMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    success = !result.empty() && result.find("[BackendSwitcher] Error") == std::string::npos;

    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_backendMutex);
        auto& st = m_backendStatuses[(size_t)active];
        st.requestCount++;
        st.latencyMs = latencyMs;
        st.lastUsedEpochMs = currentEpochMs();
        st.lastModel = m_backendConfigs[(size_t)active].model;
        if (!success) {
            st.failureCount++;
            st.lastError = result.substr(0, 256);
        } else {
            st.lastError = "";
        }
    }

    return result;
}

void Win32IDE::routeInferenceRequestAsync(const std::string& prompt,
                                            std::function<void(const std::string&, bool)> callback) {
    std::thread([this, prompt, callback]() {
        std::string result = routeInferenceRequest(prompt);
        bool ok = !result.empty() && result.find("[BackendSwitcher] Error") == std::string::npos;
        if (callback) callback(result, ok);
    }).detach();
}

// ============================================================================
// PER-BACKEND ROUTING IMPLEMENTATIONS
// ============================================================================

std::string Win32IDE::routeToLocalGGUF(const std::string& prompt) {
    // Use the existing native engine path
    if (!m_nativeEngine || !m_nativeEngine->IsModelLoaded()) {
        return "[BackendSwitcher] Error: No local GGUF model loaded. Use File > Load Model first.";
    }
    // Delegate to existing generateResponse (which uses m_nativeEngine)
    return generateResponse(prompt);
}

std::string Win32IDE::routeToOllama(const std::string& prompt) {
    const auto& cfg = m_backendConfigs[(size_t)AIBackendType::Ollama];
    if (cfg.endpoint.empty()) {
        return "[BackendSwitcher] Error: Ollama endpoint not configured";
    }

    // Build Ollama /api/generate request body
    nlohmann::json reqBody;
    reqBody["model"]  = cfg.model;
    reqBody["prompt"] = prompt;
    reqBody["stream"] = false;
    reqBody["options"] = {
        {"temperature", cfg.temperature},
        {"num_predict", cfg.maxTokens}
    };

    try {
        std::string resp = httpPost(cfg.endpoint + "/api/generate",
                                     reqBody.dump(), {"Content-Type: application/json"}, cfg.timeoutMs);
        nlohmann::json rj = nlohmann::json::parse(resp);
        if (rj.contains("response")) {
            return rj["response"].get<std::string>();
        }
        if (rj.contains("error")) {
            return "[BackendSwitcher] Error (Ollama): " + rj["error"].get<std::string>();
        }
        return "[BackendSwitcher] Error (Ollama): Unexpected response format";
    } catch (const std::exception& e) {
        return std::string("[BackendSwitcher] Error (Ollama): ") + e.what();
    }
}

std::string Win32IDE::routeToOpenAI(const std::string& prompt) {
    const auto& cfg = m_backendConfigs[(size_t)AIBackendType::OpenAI];
    if (cfg.apiKey.empty()) {
        return "[BackendSwitcher] Error: OpenAI API key not set. Use 'AI: Set API Key (OpenAI)'.";
    }

    // Build OpenAI Chat Completions request
    nlohmann::json reqBody;
    reqBody["model"] = cfg.model;
    reqBody["max_tokens"] = cfg.maxTokens;
    reqBody["temperature"] = cfg.temperature;
    {
        nlohmann::json msgs = nlohmann::json::array();
        nlohmann::json msg;
        msg["role"] = "user";
        msg["content"] = prompt;
        msgs.push_back(msg);
        reqBody["messages"] = msgs;
    }

    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "Authorization: Bearer " + cfg.apiKey
    };

    try {
        std::string resp = httpPost(cfg.endpoint + "/v1/chat/completions",
                                     reqBody.dump(), headers, cfg.timeoutMs);
        nlohmann::json rj = nlohmann::json::parse(resp);
        if (rj.contains("choices") && rj["choices"].is_array() && !rj["choices"].empty()) {
            return rj["choices"][(size_t)0]["message"]["content"].get<std::string>();
        }
        if (rj.contains("error")) {
            return "[BackendSwitcher] Error (OpenAI): " +
                   rj["error"].value("message", "Unknown error");
        }
        return "[BackendSwitcher] Error (OpenAI): Unexpected response format";
    } catch (const std::exception& e) {
        return std::string("[BackendSwitcher] Error (OpenAI): ") + e.what();
    }
}

std::string Win32IDE::routeToClaude(const std::string& prompt) {
    const auto& cfg = m_backendConfigs[(size_t)AIBackendType::Claude];
    if (cfg.apiKey.empty()) {
        return "[BackendSwitcher] Error: Claude API key not set. Use 'AI: Set API Key (Claude)'.";
    }

    // Build Anthropic Messages API request
    nlohmann::json reqBody;
    reqBody["model"] = cfg.model;
    reqBody["max_tokens"] = cfg.maxTokens;
    {
        nlohmann::json msgs = nlohmann::json::array();
        nlohmann::json msg;
        msg["role"] = "user";
        msg["content"] = prompt;
        msgs.push_back(msg);
        reqBody["messages"] = msgs;
    }

    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "x-api-key: " + cfg.apiKey,
        "anthropic-version: 2023-06-01"
    };

    try {
        std::string resp = httpPost(cfg.endpoint + "/v1/messages",
                                     reqBody.dump(), headers, cfg.timeoutMs);
        nlohmann::json rj = nlohmann::json::parse(resp);
        if (rj.contains("content") && rj["content"].is_array() && !rj["content"].empty()) {
            return rj["content"][(size_t)0]["text"].get<std::string>();
        }
        if (rj.contains("error")) {
            return "[BackendSwitcher] Error (Claude): " +
                   rj["error"].value("message", "Unknown error");
        }
        return "[BackendSwitcher] Error (Claude): Unexpected response format";
    } catch (const std::exception& e) {
        return std::string("[BackendSwitcher] Error (Claude): ") + e.what();
    }
}

std::string Win32IDE::routeToGemini(const std::string& prompt) {
    const auto& cfg = m_backendConfigs[(size_t)AIBackendType::Gemini];
    if (cfg.apiKey.empty()) {
        return "[BackendSwitcher] Error: Gemini API key not set. Use 'AI: Set API Key (Gemini)'.";
    }

    // Build Google Generative Language API request
    nlohmann::json reqBody;
    {
        nlohmann::json contents = nlohmann::json::array();
        nlohmann::json contentItem;
        nlohmann::json parts = nlohmann::json::array();
        nlohmann::json part;
        part["text"] = prompt;
        parts.push_back(part);
        contentItem["parts"] = parts;
        contents.push_back(contentItem);
        reqBody["contents"] = contents;
    }
    {
        nlohmann::json genCfg;
        genCfg["temperature"] = cfg.temperature;
        genCfg["maxOutputTokens"] = cfg.maxTokens;
        reqBody["generationConfig"] = genCfg;
    }

    std::string url = cfg.endpoint + "/v1beta/models/" + cfg.model +
                      ":generateContent?key=" + cfg.apiKey;
    std::vector<std::string> headers = {
        "Content-Type: application/json"
    };

    try {
        std::string resp = httpPost(url, reqBody.dump(), headers, cfg.timeoutMs);
        nlohmann::json rj = nlohmann::json::parse(resp);
        if (rj.contains("candidates") && rj["candidates"].is_array() && !rj["candidates"].empty()) {
            auto& content = rj["candidates"][(size_t)0]["content"];
            if (content.contains("parts") && content["parts"].is_array() && !content["parts"].empty()) {
                return content["parts"][(size_t)0]["text"].get<std::string>();
            }
        }
        if (rj.contains("error")) {
            return "[BackendSwitcher] Error (Gemini): " +
                   rj["error"].value("message", "Unknown error");
        }
        return "[BackendSwitcher] Error (Gemini): Unexpected response format";
    } catch (const std::exception& e) {
        return std::string("[BackendSwitcher] Error (Gemini): ") + e.what();
    }
}

// ============================================================================
// HTTP POST HELPER (WinHTTP)
// ============================================================================

std::string Win32IDE::httpPost(const std::string& url, const std::string& body,
                                const std::vector<std::string>& headers, int timeoutMs) {
    // Parse URL into components
    std::string scheme, host, path;
    int port = 80;
    bool useSSL = false;

    // Extract scheme
    size_t schemeEnd = url.find("://");
    if (schemeEnd != std::string::npos) {
        scheme = url.substr(0, schemeEnd);
        size_t hostStart = schemeEnd + 3;
        size_t pathStart = url.find('/', hostStart);
        if (pathStart != std::string::npos) {
            host = url.substr(hostStart, pathStart - hostStart);
            path = url.substr(pathStart);
        } else {
            host = url.substr(hostStart);
            path = "/";
        }
    } else {
        // No scheme — assume http
        size_t pathStart = url.find('/');
        if (pathStart != std::string::npos) {
            host = url.substr(0, pathStart);
            path = url.substr(pathStart);
        } else {
            host = url;
            path = "/";
        }
    }

    useSSL = (scheme == "https");
    port = useSSL ? 443 : 80;

    // Extract port from host if present
    size_t colonPos = host.find(':');
    if (colonPos != std::string::npos) {
        port = std::stoi(host.substr(colonPos + 1));
        host = host.substr(0, colonPos);
    }

    // Convert to wide strings for WinHTTP
    std::wstring wHost(host.begin(), host.end());
    std::wstring wPath(path.begin(), path.end());

    HINTERNET hSession = WinHttpOpen(L"RawrXD-BackendSwitcher/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    // Set timeouts
    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    DWORD flags = useSSL ? WINHTTP_FLAG_SECURE : 0;
    LPCWSTR verb = body.empty() ? L"GET" : L"POST";
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, verb, wPath.c_str(),
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Add custom headers
    for (const auto& hdr : headers) {
        std::wstring wHdr(hdr.begin(), hdr.end());
        WinHttpAddRequestHeaders(hRequest, wHdr.c_str(), (DWORD)wHdr.length(),
                                  WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Send request
    BOOL sent = WinHttpSendRequest(hRequest,
                                    WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(),
                                    (DWORD)body.size(),
                                    (DWORD)body.size(), 0);
    if (!sent) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Read response
    std::string responseBody;
    DWORD bytesRead = 0;
    DWORD bytesAvailable = 0;
    do {
        bytesAvailable = 0;
        WinHttpQueryDataAvailable(hRequest, &bytesAvailable);
        if (bytesAvailable == 0) break;

        std::vector<char> buf(bytesAvailable + 1, 0);
        WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
        responseBody.append(buf.data(), bytesRead);
    } while (bytesRead > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return responseBody;
}

// ============================================================================
// UI HELPERS
// ============================================================================

void Win32IDE::showBackendSwitcherDialog() {
    // Show a quick picker via the output panel (full dialog would be Phase 8C+)
    std::string status = getBackendStatusString();
    appendToOutput(status, "General", OutputSeverity::Info);
    appendToOutput("[BackendSwitcher] Use Command Palette (Ctrl+Shift+P) to switch backends:\n"
                   "  'AI: Switch to Local GGUF'\n"
                   "  'AI: Switch to Ollama'\n"
                   "  'AI: Switch to OpenAI'\n"
                   "  'AI: Switch to Claude'\n"
                   "  'AI: Switch to Gemini'\n",
                   "General", OutputSeverity::Info);
}

void Win32IDE::showBackendConfigDialog(AIBackendType type) {
    if (type >= AIBackendType::Count) return;

    const auto& cfg = m_backendConfigs[(size_t)type];
    std::ostringstream ss;
    ss << "[BackendSwitcher] Configuration for '" << cfg.name << "':\n";
    ss << "  Type:        " << backendTypeString(cfg.type) << "\n";
    ss << "  Endpoint:    " << (cfg.endpoint.empty() ? "(native engine)" : cfg.endpoint) << "\n";
    ss << "  Model:       " << (cfg.model.empty() ? "(loaded file)" : cfg.model) << "\n";
    ss << "  API Key:     " << (cfg.apiKey.empty() ? "(not set)" : "****" + cfg.apiKey.substr(cfg.apiKey.size() > 4 ? cfg.apiKey.size() - 4 : 0)) << "\n";
    ss << "  Enabled:     " << (cfg.enabled ? "yes" : "no") << "\n";
    ss << "  Timeout:     " << cfg.timeoutMs << " ms\n";
    ss << "  Max Tokens:  " << cfg.maxTokens << "\n";
    ss << "  Temperature: " << cfg.temperature << "\n";

    appendToOutput(ss.str(), "General", OutputSeverity::Info);
}

void Win32IDE::updateStatusBarBackend() {
    // Update the status bar to show active backend
    if (m_hwndStatusBar) {
        std::string label = "[" + getActiveBackendName() + "]";
        // Use m_statusBarInfo.copilotActive area or a dedicated section
        m_statusBarInfo.copilotActive = true;
        updateEnhancedStatusBar();
    }
}

std::string Win32IDE::backendTypeString(AIBackendType type) const {
    switch (type) {
        case AIBackendType::LocalGGUF: return "LocalGGUF";
        case AIBackendType::Ollama:    return "Ollama";
        case AIBackendType::OpenAI:    return "OpenAI";
        case AIBackendType::Claude:    return "Claude";
        case AIBackendType::Gemini:    return "Gemini";
        default:                       return "Unknown";
    }
}

Win32IDE::Win32IDE::AIBackendType Win32IDE::backendTypeFromString(const std::string& name) const {
    if (name == "LocalGGUF" || name == "local" || name == "Local GGUF") return AIBackendType::LocalGGUF;
    if (name == "Ollama"    || name == "ollama")                        return AIBackendType::Ollama;
    if (name == "OpenAI"    || name == "openai")                        return AIBackendType::OpenAI;
    if (name == "Claude"    || name == "claude")                        return AIBackendType::Claude;
    if (name == "Gemini"    || name == "gemini")                        return AIBackendType::Gemini;
    return AIBackendType::Count; // sentinel for "not found"
}
