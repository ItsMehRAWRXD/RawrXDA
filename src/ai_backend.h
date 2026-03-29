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
#include <unordered_set>
#include <filesystem>

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

    // ── Auto-fallback & downgrade state management ──────────────────────────
    mutable std::mutex fallbackMu_;
    bool isMetadataOnlyBlocked_ = false;
    std::string metadataOnlyBlockReason_;
    std::string loadedModelPath_;
    std::string autoDowngradeAttemptedSourcePath_;
    std::string autoDowngradeLoadedPath_;

public:
    // ── Metadata-only blocking state (for LocalGGUF) ─────────────────────────
    void setLocalMetadataOnlyBlocked(const std::string& reason) {
        std::lock_guard<std::mutex> lk(fallbackMu_);
        isMetadataOnlyBlocked_ = true;
        metadataOnlyBlockReason_ = reason;
        log(2, "[BackendMgr] LocalGGUF blocked: " + reason);
    }

    void clearLocalMetadataOnlyBlocked() {
        std::lock_guard<std::mutex> lk(fallbackMu_);
        isMetadataOnlyBlocked_ = false;
        metadataOnlyBlockReason_.clear();
        log(1, "[BackendMgr] LocalGGUF blocking cleared");
    }

    bool isLocalMetadataOnlyBlocked(std::string* reason = nullptr) const {
        std::lock_guard<std::mutex> lk(fallbackMu_);
        if (reason && isMetadataOnlyBlocked_) {
            *reason = metadataOnlyBlockReason_;
        }
        return isMetadataOnlyBlocked_;
    }

    // ── Model path tracking ──────────────────────────────────────────────────
    void setLoadedModelPath(const std::string& path) {
        std::lock_guard<std::mutex> lk(fallbackMu_);
        loadedModelPath_ = path;
    }

    std::string getLoadedModelPath() const {
        std::lock_guard<std::mutex> lk(fallbackMu_);
        return loadedModelPath_;
    }

    void resetAutoDowngradeState() {
        std::lock_guard<std::mutex> lk(fallbackMu_);
        autoDowngradeAttemptedSourcePath_.clear();
        autoDowngradeLoadedPath_.clear();
        log(1, "[BackendMgr] Auto-downgrade state reset");
    }

    // ── Quantization downgrade candidates discovery ──────────────────────────
    std::vector<std::string> buildQuantizationDowngradeCandidates(const std::string& modelPath) const {
        std::vector<std::string> candidates;
        std::unordered_set<std::string> seen;
        if (modelPath.empty()) return candidates;

        std::filesystem::path p(modelPath);
        const std::string filename = p.filename().string();
        std::string filenameLower = filename;
        std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });

        struct QuantToken {
            const char* token;
            int tier;  // lower is smaller
        };

        // Ordered by token length to prefer specific matches first.
        const std::vector<QuantToken> knownTokens = {
            {"q8_0", 8}, {"q6_k", 6}, {"q5_k_m", 5}, {"q5_k_s", 5}, {"q5_1", 5}, {"q5_0", 5},
            {"q4_k_m", 4}, {"q4_k_s", 4}, {"q4_1", 4}, {"q4_0", 4}, {"q4_k", 4},
            {"q3_k_l", 3}, {"q3_k_m", 3}, {"q3_k_s", 3}, {"q3_k", 3},
            {"q2_k", 2}, {"q2", 2}};

        size_t tokenPos = std::string::npos;
        size_t tokenLen = 0;
        int sourceTier = 0;
        for (const auto& qt : knownTokens) {
            size_t pos = filenameLower.find(qt.token);
            if (pos != std::string::npos) {
                tokenPos = pos;
                tokenLen = strlen(qt.token);
                sourceTier = qt.tier;
                break;
            }
        }

        if (tokenPos == std::string::npos) return candidates;

        std::vector<const char*> targetTokens;
        if (sourceTier >= 4) {
            targetTokens = {"q3_k_m", "q3_k_s", "q3_k_l", "q2_k", "q2"};
        } else if (sourceTier == 3) {
            targetTokens = {"q2_k", "q2"};
        }

        for (const char* target : targetTokens) {
            std::string downgradedName = filename;
            downgradedName.replace(tokenPos, tokenLen, target);
            std::filesystem::path candidatePath = p.parent_path() / downgradedName;
            std::error_code ec;
            if (std::filesystem::exists(candidatePath, ec) && !ec) {
                const std::string k = candidatePath.string();
                if (seen.insert(k).second)
                    candidates.push_back(k);
            }
        }

        // Directory scan fallback for naming variants not captured by direct token replacement.
        const std::string prefix = filenameLower.substr(0, tokenPos);
        const std::string suffix = filenameLower.substr(tokenPos + tokenLen);
        std::error_code dirEc;
        for (const auto& ent : std::filesystem::directory_iterator(p.parent_path(), dirEc)) {
            if (dirEc) break;
            if (!ent.is_regular_file()) continue;

            const std::string name = ent.path().filename().string();
            std::string nameLower = name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });

            if (nameLower.size() < 5 || nameLower.substr(nameLower.size() - 5) != ".gguf") continue;
            if (nameLower.rfind(prefix, 0) != 0) continue;
            if (!suffix.empty() && nameLower.size() < suffix.size()) continue;
            if (!suffix.empty() && nameLower.substr(nameLower.size() - suffix.size()) != suffix) continue;

            for (const char* target : targetTokens) {
                if (nameLower.find(target) == std::string::npos) continue;
                const std::string k = ent.path().string();
                if (seen.insert(k).second) {
                    candidates.push_back(k);
                    break;
                }
            }
        }

        return candidates;
    }

    // ── Environment gate utilities ───────────────────────────────────────────
    static bool isTruthyEnv(const char* value) {
        if (!value || !*value) return false;
        std::string v(value);
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return v == "1" || v == "true" || v == "yes" || v == "on";
    }

    static bool endsWithCaseInsensitive(const std::string& value, const std::string& suffix) {
        if (suffix.size() > value.size()) return false;
        const size_t off = value.size() - suffix.size();
        for (size_t i = 0; i < suffix.size(); ++i) {
            const char a = (char)std::tolower((unsigned char)value[off + i]);
            const char b = (char)std::tolower((unsigned char)suffix[i]);
            if (a != b) return false;
        }
        return true;
    }

    // ── Auto-downgrade retry logic ────────────────────────────────────────────
    using ModelLoadFunction = std::function<bool(const std::string&)>;

    bool tryAutoDowngradeLocalModel(const ModelLoadFunction& loadFn, std::string* loadedCandidate = nullptr, std::string* failureReason = nullptr) {
        const char* gate = std::getenv("RAWRXD_ENABLE_AUTO_QUANT_DOWNGRADE");
        if (gate && !isTruthyEnv(gate)) {
            if (failureReason) *failureReason = "Auto-downgrade is disabled by RAWRXD_ENABLE_AUTO_QUANT_DOWNGRADE.";
            return false;
        }

        std::string sourcePath;
        {
            std::lock_guard<std::mutex> lock(fallbackMu_);
            sourcePath = loadedModelPath_;
            if (sourcePath.empty()) {
                if (failureReason) *failureReason = "No source model path available.";
                return false;
            }

            if (!autoDowngradeLoadedPath_.empty() && autoDowngradeLoadedPath_ == sourcePath) {
                if (failureReason) *failureReason = "Current model already reflects the last downgrade result.";
                return false;
            }
            if (autoDowngradeAttemptedSourcePath_ == sourcePath) {
                if (failureReason) *failureReason = "Auto-downgrade already attempted for this model.";
                return false;
            }
            autoDowngradeAttemptedSourcePath_ = sourcePath;
        }

        auto candidates = buildQuantizationDowngradeCandidates(sourcePath);
        if (candidates.empty()) {
            if (failureReason) *failureReason = "No lower quantized sibling GGUF candidates were found.";
            return false;
        }

        for (const auto& candidate : candidates) {
            log(2, "[BackendMgr] Auto-downgrade attempt: " + candidate);
            if (loadFn && loadFn(candidate)) {
                {
                    std::lock_guard<std::mutex> lock(fallbackMu_);
                    autoDowngradeLoadedPath_ = candidate;
                    loadedModelPath_ = candidate;
                }
                clearLocalMetadataOnlyBlocked();
                log(1, "[BackendMgr] Auto-downgrade succeeded: " + candidate);
                if (loadedCandidate) *loadedCandidate = candidate;
                return true;
            }
        }

        if (failureReason) *failureReason = "All lower-quantized candidates failed to load.";
        return false;
    }

    // ── Auto-fallback routing convenience ────────────────────────────────────
    bool shouldTryOllamaFallback() const {
        std::string blockReason;
        if (!isLocalMetadataOnlyBlocked(&blockReason)) {
            return false;
        }

        auto* ollamaCfg = getBackend("ollama");
        if (!ollamaCfg || !ollamaCfg->enabled || ollamaCfg->endpoint.empty() || ollamaCfg->model.empty()) {
            return false;
        }

        return true;
    }
};
