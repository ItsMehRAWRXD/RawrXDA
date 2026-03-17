// ============================================================================
// AI Backend Abstraction — Phase 8B
//
// Owns the registry of configured backends and the active-backend pointer.
// Pure data + CRUD — no inference, no routing, no heuristics.
// Thread-safe: all public methods are mutex-guarded.
// Session-only by default; persistence is opt-in via save()/load().
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <sstream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <filesystem>

// ============================================================================
// Enums + Structs
// ============================================================================

enum class AIBackendType {
    LocalGGUF  = 0,   // Direct GGUF inference via RawrXD native engine
    Ollama     = 1,   // Ollama HTTP API (localhost:11434)
    OpenAI     = 2,   // OpenAI-compatible API
    Claude     = 3,   // Anthropic Claude API
    Gemini     = 4,   // Google Gemini API
    Custom     = 5    // User-defined HTTP endpoint
};

inline const char* AIBackendTypeString(AIBackendType type) {
    switch (type) {
        case AIBackendType::LocalGGUF: return "LocalGGUF";
        case AIBackendType::Ollama:    return "Ollama";
        case AIBackendType::OpenAI:    return "OpenAI";
        case AIBackendType::Claude:    return "Claude";
        case AIBackendType::Gemini:    return "Gemini";
        case AIBackendType::Custom:    return "Custom";
        default:                       return "Unknown";
    }
}

inline AIBackendType AIBackendTypeFromString(const std::string& s) {
    if (s == "LocalGGUF") return AIBackendType::LocalGGUF;
    if (s == "Ollama")    return AIBackendType::Ollama;
    if (s == "OpenAI")    return AIBackendType::OpenAI;
    if (s == "Claude")    return AIBackendType::Claude;
    if (s == "Gemini")    return AIBackendType::Gemini;
    if (s == "Custom")    return AIBackendType::Custom;
    return AIBackendType::LocalGGUF;
}

struct AIBackendConfig {
    std::string id;               // unique backend id (e.g., "local-gguf-default")
    std::string displayName;      // human-readable name
    AIBackendType type = AIBackendType::LocalGGUF;
    std::string endpoint;         // base URL (e.g., "http://localhost:11434")
    std::string model;            // model name/path (e.g., "llama3.2:7b")
    std::string apiKey;           // API key (stored in memory only, never logged)
    int timeoutMs  = 30000;       // request timeout
    int maxTokens  = 512;         // default max tokens
    float temperature = 0.7f;     // default temperature
    bool enabled   = true;        // soft toggle

    // ── Serialization ──
    std::string toJSON() const {
        std::ostringstream o;
        o << "{\"id\":\"" << id << "\""
          << ",\"displayName\":\"" << displayName << "\""
          << ",\"type\":\"" << AIBackendTypeString(type) << "\""
          << ",\"endpoint\":\"" << endpoint << "\""
          << ",\"model\":\"" << model << "\""
          << ",\"timeoutMs\":" << timeoutMs
          << ",\"maxTokens\":" << maxTokens
          << ",\"temperature\":" << temperature
          << ",\"enabled\":" << (enabled ? "true" : "false")
          << "}";
        return o.str();
    }
};

// ============================================================================
// AIBackendManager — CRUD + active-backend management
// ============================================================================

class AIBackendManager {
public:
    AIBackendManager() {
        // Register the default local GGUF backend
        AIBackendConfig local;
        local.id          = "local-gguf";
        local.displayName = "Local GGUF (Native Engine)";
        local.type        = AIBackendType::LocalGGUF;
        local.endpoint    = "local";
        local.model       = "";
        local.enabled     = true;
        m_backends.push_back(local);
        m_activeId = local.id;
    }

    // ── CRUD ──

    std::string addBackend(const AIBackendConfig& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        AIBackendConfig c = config;
        if (c.id.empty()) {
            c.id = generateId(c.type);
        }
        m_backends.push_back(c);
        return c.id;
    }

    bool removeBackend(const std::string& backendId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (backendId == "local-gguf") return false; // cannot remove default

        auto it = std::remove_if(m_backends.begin(), m_backends.end(),
            [&](const AIBackendConfig& b) { return b.id == backendId; });
        if (it != m_backends.end()) {
            m_backends.erase(it, m_backends.end());
            if (m_activeId == backendId) {
                m_activeId = "local-gguf"; // fall back to default
            }
            return true;
        }
        return false;
    }

    bool updateBackend(const std::string& backendId, const AIBackendConfig& updated) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& b : m_backends) {
            if (b.id == backendId) {
                std::string preservedId = b.id;
                b = updated;
                b.id = preservedId; // preserve original ID
                return true;
            }
        }
        return false;
    }

    // ── Active Backend ──

    bool setActiveBackend(const std::string& backendId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& b : m_backends) {
            if (b.id == backendId && b.enabled) {
                std::string oldActive = m_activeId;
                m_activeId = backendId;
                m_switchCount++;
                return true;
            }
        }
        return false;
    }

    AIBackendConfig getActiveBackend() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& b : m_backends) {
            if (b.id == m_activeId) return b;
        }
        // Fallback: return first backend
        if (!m_backends.empty()) return m_backends[0];
        return {};
    }

    std::string getActiveId() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activeId;
    }

    // ── Query ──

    const AIBackendConfig* getBackend(const std::string& backendId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& b : m_backends) {
            if (b.id == backendId) return &b;
        }
        return nullptr;
    }

    std::vector<AIBackendConfig> listBackends(bool enabledOnly = false) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!enabledOnly) return m_backends;

        std::vector<AIBackendConfig> result;
        for (const auto& b : m_backends) {
            if (b.enabled) result.push_back(b);
        }
        return result;
    }

    size_t backendCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_backends.size();
    }

    // ── Stats ──

    std::string getStatsJSON() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ostringstream o;
        o << "{\"total\":" << m_backends.size()
          << ",\"activeId\":\"" << m_activeId << "\""
          << ",\"switchCount\":" << m_switchCount
          << ",\"backends\":[";
        for (size_t i = 0; i < m_backends.size(); ++i) {
            if (i > 0) o << ",";
            o << m_backends[i].toJSON();
        }
        o << "]}";
        return o.str();
    }

    // ── Persistence (opt-in) ──

    bool save(const std::string& filePath) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ofstream f(filePath);
        if (!f.is_open()) return false;

        f << "[";
        for (size_t i = 0; i < m_backends.size(); ++i) {
            if (i > 0) f << ",\n";
            f << m_backends[i].toJSON();
        }
        f << "]";
        f.close();
        return true;
    }

    // ── Bulk JSON export ──

    std::string toJSON() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ostringstream o;
        o << "{\"activeId\":\"" << m_activeId << "\",\"backends\":[";
        for (size_t i = 0; i < m_backends.size(); ++i) {
            if (i > 0) o << ",";
            o << m_backends[i].toJSON();
        }
        o << "]}";
        return o.str();
    }

private:
    mutable std::mutex m_mutex;
    std::vector<AIBackendConfig> m_backends;
    std::string m_activeId;
    int m_switchCount = 0;

    std::string generateId(AIBackendType type) const {
        static std::atomic<int> counter{0};
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        int c = counter.fetch_add(1);

        std::ostringstream oss;
        oss << AIBackendTypeString(type) << "-" << std::hex << (ms & 0xFFFF) << "-" << std::dec << c;
        // Lowercase the type portion
        std::string id = oss.str();
        std::transform(id.begin(), id.end(), id.begin(),
                       [](unsigned char ch) { return (char)std::tolower(ch); });
        return id;
    }
};
