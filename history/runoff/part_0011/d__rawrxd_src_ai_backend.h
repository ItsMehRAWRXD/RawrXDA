#pragma once
// ============================================================================
// ai_backend.h — Phase 8B: AI Backend Registry & Switcher
//
// Lightweight, header-only backend manager for the RawrEngine target.
// Provides CRUD for backend configs, active-backend selection, and JSON
// serialization.  Thread-safe via std::mutex.
//
// This is separate from Win32IDE's integrated AIBackendType/BackendSwitcher,
// which lives entirely inside Win32IDE.h / Win32IDE_BackendSwitcher.cpp.
// ============================================================================

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <functional>

// ── Backend type enum ──────────────────────────────────────────────────────
enum class AIBackendType {
    LocalGGUF = 0,
    Ollama    = 1,
    OpenAI    = 2,
    Claude    = 3,
    Gemini    = 4,
    Custom    = 5
};

inline const char* aiBackendTypeName(AIBackendType t) {
    switch (t) {
        case AIBackendType::LocalGGUF: return "LocalGGUF";
        case AIBackendType::Ollama:    return "Ollama";
        case AIBackendType::OpenAI:    return "OpenAI";
        case AIBackendType::Claude:    return "Claude";
        case AIBackendType::Gemini:    return "Gemini";
        case AIBackendType::Custom:    return "Custom";
    }
    return "Unknown";
}

inline AIBackendType aiBackendTypeFromName(const std::string& name) {
    if (name == "LocalGGUF" || name == "local")  return AIBackendType::LocalGGUF;
    if (name == "Ollama"    || name == "ollama")  return AIBackendType::Ollama;
    if (name == "OpenAI"    || name == "openai")  return AIBackendType::OpenAI;
    if (name == "Claude"    || name == "claude")  return AIBackendType::Claude;
    if (name == "Gemini"    || name == "gemini")  return AIBackendType::Gemini;
    if (name == "Custom"    || name == "custom")  return AIBackendType::Custom;
    return AIBackendType::LocalGGUF; // fallback
}

// ── Backend configuration ─────────────────────────────────────────────────
struct AIBackendConfig {
    std::string   id;                                  // unique slug, e.g. "local-gguf"
    std::string   displayName = "Local GGUF";
    AIBackendType type        = AIBackendType::LocalGGUF;
    std::string   endpoint;                            // base URL (empty for local)
    std::string   model;                               // model name / path
    std::string   apiKey;                              // empty for local
    int           timeoutMs   = 30000;
    int           maxTokens   = 512;
    float         temperature = 0.7f;
    bool          enabled     = true;

    // Minimal JSON serialisation (no nlohmann dependency)
    std::string toJSON() const {
        std::string j = "{";
        j += "\"id\":\"" + id + "\"";
        j += ",\"displayName\":\"" + displayName + "\"";
        j += ",\"type\":\"" + std::string(aiBackendTypeName(type)) + "\"";
        j += ",\"endpoint\":\"" + endpoint + "\"";
        j += ",\"model\":\"" + model + "\"";
        j += ",\"hasApiKey\":" + std::string(apiKey.empty() ? "false" : "true");
        j += ",\"timeoutMs\":" + std::to_string(timeoutMs);
        j += ",\"maxTokens\":" + std::to_string(maxTokens);
        j += ",\"temperature\":" + std::to_string(temperature);
        j += ",\"enabled\":" + std::string(enabled ? "true" : "false");
        j += "}";
        return j;
    }
};

// ── Backend manager ───────────────────────────────────────────────────────
class AIBackendManager {
public:
    using LogCallback = std::function<void(int, const std::string&)>;

    AIBackendManager() {
        // Register default local-gguf backend
        AIBackendConfig local;
        local.id          = "local-gguf";
        local.displayName = "Local GGUF";
        local.type        = AIBackendType::LocalGGUF;
        local.enabled     = true;
        backends_.push_back(local);
        activeId_ = "local-gguf";
    }

    void setLogCallback(LogCallback cb) { logCb_ = std::move(cb); }

    // ── CRUD ──

    bool addBackend(const AIBackendConfig& cfg) {
        std::lock_guard<std::mutex> lk(mu_);
        for (const auto& b : backends_) {
            if (b.id == cfg.id) return false; // duplicate
        }
        backends_.push_back(cfg);
        log(1, "[BackendMgr] Added backend: " + cfg.id + " (" + cfg.displayName + ")");
        return true;
    }

    bool removeBackend(const std::string& id) {
        std::lock_guard<std::mutex> lk(mu_);
        if (id == "local-gguf") return false; // cannot remove default
        auto it = std::find_if(backends_.begin(), backends_.end(),
                               [&](const AIBackendConfig& c){ return c.id == id; });
        if (it == backends_.end()) return false;
        backends_.erase(it);
        if (activeId_ == id) activeId_ = "local-gguf";
        log(1, "[BackendMgr] Removed backend: " + id);
        return true;
    }

    bool updateBackend(const AIBackendConfig& cfg) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& b : backends_) {
            if (b.id == cfg.id) {
                b = cfg;
                log(1, "[BackendMgr] Updated backend: " + cfg.id);
                return true;
            }
        }
        return false;
    }

    // ── Active backend ──

    bool setActiveBackend(const std::string& id) {
        std::lock_guard<std::mutex> lk(mu_);
        for (const auto& b : backends_) {
            if (b.id == id && b.enabled) {
                activeId_ = id;
                log(1, "[BackendMgr] Active backend → " + id);
                return true;
            }
        }
        return false;
    }

    std::string getActiveId() const {
        std::lock_guard<std::mutex> lk(mu_);
        return activeId_;
    }

    AIBackendConfig getActiveBackend() const {
        std::lock_guard<std::mutex> lk(mu_);
        for (const auto& b : backends_) {
            if (b.id == activeId_) return b;
        }
        return backends_.empty() ? AIBackendConfig{} : backends_[0];
    }

    std::string getActiveBackendName() const {
        std::lock_guard<std::mutex> lk(mu_);
        for (const auto& b : backends_) {
            if (b.id == activeId_) return b.displayName;
        }
        return "Unknown";
    }

    // ── Listing ──

    std::vector<AIBackendConfig> listBackends() const {
        std::lock_guard<std::mutex> lk(mu_);
        return backends_;
    }

    size_t backendCount() const {
        std::lock_guard<std::mutex> lk(mu_);
        return backends_.size();
    }

    const AIBackendConfig* getBackend(const std::string& id) const {
        std::lock_guard<std::mutex> lk(mu_);
        for (const auto& b : backends_) {
            if (b.id == id) return &b;
        }
        return nullptr;
    }

    // ── JSON output ──

    std::string toJSON() const {
        std::lock_guard<std::mutex> lk(mu_);
        std::string j = "{\"active\":\"" + activeId_ + "\",\"backends\":[";
        for (size_t i = 0; i < backends_.size(); ++i) {
            j += backends_[i].toJSON();
            if (i + 1 < backends_.size()) j += ",";
        }
        j += "],\"count\":" + std::to_string(backends_.size()) + "}";
        return j;
    }

    std::string getStatsJSON() const {
        std::lock_guard<std::mutex> lk(mu_);
        std::string j = "{\"active\":\"" + activeId_ + "\"";
        j += ",\"activeName\":\"" + getActiveNameLocked() + "\"";
        j += ",\"totalBackends\":" + std::to_string(backends_.size());
        int enabled = 0;
        for (const auto& b : backends_) if (b.enabled) ++enabled;
        j += ",\"enabledBackends\":" + std::to_string(enabled);
        j += "}";
        return j;
    }

    // ── Persistence ──

    bool save(const std::string& filePath) const {
        std::lock_guard<std::mutex> lk(mu_);
        std::ofstream f(filePath);
        if (!f.is_open()) return false;
        f << "{\"active\":\"" << activeId_ << "\",\"backends\":[";
        for (size_t i = 0; i < backends_.size(); ++i) {
            f << backends_[i].toJSON();
            if (i + 1 < backends_.size()) f << ",";
        }
        f << "]}";
        return true;
    }

private:
    std::string getActiveNameLocked() const {
        for (const auto& b : backends_) {
            if (b.id == activeId_) return b.displayName;
        }
        return "Unknown";
    }

    void log(int level, const std::string& msg) {
        if (logCb_) logCb_(level, msg);
    }

    mutable std::mutex mu_;
    std::vector<AIBackendConfig> backends_;
    std::string activeId_ = "local-gguf";
    LogCallback logCb_;
};
