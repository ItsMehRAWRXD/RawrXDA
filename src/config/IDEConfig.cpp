// ============================================================================
// IDEConfig.cpp - External Configuration Management & Feature Toggles
// Enterprise-grade configuration for RawrXD IDE.
// ============================================================================

#include "IDEConfig.h"
#include "win32app/IDELogger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <random>
#include <windows.h>

// ============================================================================
// IDEConfig — Load / Save / Defaults
// ============================================================================

void IDEConfig::setDefaults()
{
    // Editor defaults
    m_values["editor.fontSize"] = "14";
    m_values["editor.fontFamily"] = "Consolas";
    m_values["editor.tabSize"] = "4";
    m_values["editor.insertSpaces"] = "true";
    m_values["editor.wordWrap"] = "false";
    m_values["editor.minimapEnabled"] = "true";
    m_values["editor.lineNumbers"] = "true";
    m_values["editor.autoSave"] = "false";
    m_values["editor.autoSaveDelay"] = "1000";

    // Theme
    m_values["theme.name"] = "RawrXD Dark";
    m_values["theme.darkMode"] = "true";

    // Inference engine
    m_values["inference.maxTokens"] = "512";
    m_values["inference.temperature"] = "0.7";
    m_values["inference.topP"] = "0.9";
    m_values["inference.topK"] = "40";
    m_values["inference.repetitionPenalty"] = "1.1";
    m_values["inference.contextWindow"] = "4096";
    m_values["inference.streamOutput"] = "true";
    m_values["inference.threadCount"] = "0";  // 0 = auto-detect

    // Ollama
    m_values["ollama.baseUrl"] = "http://localhost:11434";
    m_values["ollama.modelOverride"] = "";

    // Agentic system — 1x–99x production limits (balance, speed, agenticness, autonomy)
    m_values["agent.maxMode"] = "false";
    m_values["agent.deepThinking"] = "false";
    m_values["agent.deepResearch"] = "false";
    m_values["agent.noRefusal"] = "false";
    m_values["agent.autoStart"] = "false";
    m_values["agent.cycleCount"] = "10";               // Max agent loop cycles (1–99)
    m_values["agent.perModelInstances"] = "4";        // Per-model instances when multi-model (4x per piece, 1–99)
    m_values["agent.cycleAgentCounter"] = "1";         // Cycle agent repetition multiplier (1–99)
    m_values["agent.maxModelsInParallel"] = "99";      // Max models in parallel (1–99)
    m_values["agent.qualitySpeedBalance"] = "Auto";   // Auto | QualityBias | SpeedBias | MAX_MODE
    m_values["agent.operationMode"] = "Agent";        // Agent | Plan | Debug | Ask
    m_values["agent.modelSelectionMode"] = "Auto";    // Auto | MAX | UseMultipleModels (max mode x99)
    m_values["agent.todoMaxItems"] = "99";              // Todo list cap (1–99) for "write what you want" agentic flow

    // Terminal — timeout: fixed ms, random (min-max), or auto; self-adjusted or auto-adjusted per run
    m_values["terminal.defaultShell"] = "powershell";
    m_values["terminal.fontSize"] = "13";
    m_values["terminal.timeoutMs"] = "30000";           // base timeout (fixed or random center)
    m_values["terminal.timeoutMode"] = "fixed";        // fixed | random | auto
    m_values["terminal.timeoutMinMs"] = "10000";       // for random: min ms
    m_values["terminal.timeoutMaxMs"] = "120000";      // for random: max ms
    m_values["terminal.timeoutAutoAdjustPercent"] = "25";  // auto mode: ± this % each run (0–100)

    // Debugger
    m_values["debugger.stopAtEntry"] = "true";
    m_values["debugger.externalConsole"] = "false";
    m_values["debugger.engine"] = "gdb";

    // Reverse engineering
    m_values["reverseEng.dumpbinPath"] = "";
    m_values["reverseEng.nasmPath"] = "nasm";
    m_values["reverseEng.autoAnalyze"] = "false";

    // Performance
    m_values["performance.gpuTextRendering"] = "true";
    m_values["performance.streamingGGUFLoad"] = "true";
    m_values["performance.vulkanRenderer"] = "false";
    m_values["performance.lazyInit"] = "true";

    // Logging
    m_values["logging.level"] = "info";
    m_values["logging.file"] = "RawrXD_IDE.log";
    m_values["logging.maxSize"] = "10485760";  // 10 MB

    // Feature toggles
    m_values["features.agenticBridge"] = "true";
    m_values["features.autonomy"] = "true";
    m_values["features.reverseEngineering"] = "true";
    m_values["features.extensionSystem"] = "true";
    m_values["features.copilotChat"] = "true";
    m_values["features.commandPalette"] = "true";
    m_values["features.gpuRenderer"] = "true";
    m_values["features.powerShellPanel"] = "true";
    m_values["features.debugger"] = "true";
    m_values["features.gitIntegration"] = "true";
    m_values["features.metricsExport"] = "false";
    m_values["features.vulkanCompute"] = "false";
    m_values["features.speculativeDecoding"] = "false";
    m_values["features.flashAttention"] = "false";
}

bool IDEConfig::loadFromFile(const std::string& configPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "[IDEConfig] Config file not found: " << configPath
                  << " — using defaults." << std::endl;
        return false;
    }

    try {
        file.seekg(0, std::ios::end);
        size_t fsize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);
        std::string content;
        content.reserve(std::min(fsize + 1, size_t(65536)));  // Cap reserve; typical config <16KB
        content.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        nlohmann::json json = nlohmann::json::parse(content);

        // Flatten JSON into key-value pairs using dot notation
        std::function<void(const std::string&, nlohmann::json)> flatten;
        flatten = [&](const std::string& prefix, nlohmann::json j) {
            if (j.is_object()) {
                for (auto it = j.begin(); it != j.end(); ++it) {
                    std::string key = it.key();
                    nlohmann::json value = it.value();
                    std::string fullKey = prefix.empty() ? key : prefix + "." + key;
                    flatten(fullKey, value);
                }
            } else if (j.is_string()) {
                m_values[prefix] = j.get<std::string>();
            } else if (j.is_boolean()) {
                m_values[prefix] = j.get<bool>() ? "true" : "false";
            } else if (j.is_number()) {
                if (j.is_number_integer()) {
                    m_values[prefix] = std::to_string(j.get<int64_t>());
                } else {
                    m_values[prefix] = std::to_string(j.get<double>());
                }
            }
        };

        flatten("", json);
        LOG_INFO(std::string("[IDEConfig] Loaded ") + std::to_string(m_values.size()) + " config keys from: " + configPath);
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(std::string("[IDEConfig] Error parsing config: ") + e.what());
        return false;
    }
}

bool IDEConfig::saveToFile(const std::string& configPath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        // Build nested JSON from dot-notation keys
        nlohmann::json root = nlohmann::json::object();

        for (const auto& [key, value] : m_values) {
            // Split key by '.' (typical depth 2–4; reserve to avoid realloc)
            std::vector<std::string> parts;
            parts.reserve(6);
            std::istringstream ss(key);
            std::string part;
            while (std::getline(ss, part, '.')) {
                parts.push_back(part);
            }

            nlohmann::json* current = &root;
            for (size_t i = 0; i < parts.size() - 1; i++) {
                if (!current->contains(parts[i])) {
                    (*current)[parts[i]] = nlohmann::json::object();
                }
                current = &(*current)[parts[i]];
            }

            // Try to preserve type: bool, int, float, or string
            const std::string& val = value;
            if (val == "true" || val == "false") {
                (*current)[parts.back()] = (val == "true");
            } else {
                // Try integer
                try {
                    size_t pos = 0;
                    int64_t ival = std::stoll(val, &pos);
                    if (pos == val.size()) {
                        (*current)[parts.back()] = ival;
                        continue;
                    }
                } catch (...) {}
                // Try float
                try {
                    size_t pos = 0;
                    double dval = std::stod(val, &pos);
                    if (pos == val.size()) {
                        (*current)[parts.back()] = dval;
                        continue;
                    }
                } catch (...) {}
                // String fallback
                (*current)[parts.back()] = val;
            }
        }

        std::ofstream out(configPath, std::ios::trunc);
        if (!out.is_open()) return false;
        out << root.dump(2) << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[IDEConfig] Error saving config: " << e.what() << std::endl;
        return false;
    }
}

std::string IDEConfig::getString(const std::string& key, const std::string& defaultValue) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_values.find(key);
    return it != m_values.end() ? it->second : defaultValue;
}

int IDEConfig::getInt(const std::string& key, int defaultValue) const
{
    std::string val = getString(key);
    if (val.empty()) return defaultValue;
    try { return std::stoi(val); }
    catch (...) { return defaultValue; }
}

double IDEConfig::getDouble(const std::string& key, double defaultValue) const
{
    std::string val = getString(key);
    if (val.empty()) return defaultValue;
    try { return std::stod(val); }
    catch (...) { return defaultValue; }
}

bool IDEConfig::getBool(const std::string& key, bool defaultValue) const
{
    std::string val = getString(key);
    if (val.empty()) return defaultValue;
    return val == "true" || val == "1" || val == "yes";
}

void IDEConfig::setString(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_values[key] = value;
}

void IDEConfig::setInt(const std::string& key, int value)
{
    setString(key, std::to_string(value));
}

void IDEConfig::setDouble(const std::string& key, double value)
{
    setString(key, std::to_string(value));
}

void IDEConfig::setBool(const std::string& key, bool value)
{
    setString(key, value ? "true" : "false");
}

void IDEConfig::applyFeatureToggles() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& ft = FeatureToggle::getInstance();

    for (const auto& [key, value] : m_values) {
        if (key.substr(0, 9) == "features.") {
            std::string featureName = key.substr(9);
            ft.setEnabled(featureName, value == "true" || value == "1");
        }
    }
}

void IDEConfig::applySafeModeOverrides()
{
    // Caller must hold m_mutex (called from applyEnvironmentOverrides)
    m_values["features.extensionSystem"] = "false";
    m_values["features.vulkanCompute"] = "false";
    m_values["performance.vulkanRenderer"] = "false";
    m_values["performance.gpuTextRendering"] = "false";
    m_values["features.gpuRenderer"] = "false";
}

void IDEConfig::applyEnvironmentOverrides()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Safe mode: modularize IDE (disable extensions, Vulkan, GPU)
    {
        char buf[8];
        if (GetEnvironmentVariableA("RAWRXD_SAFE_MODE", buf, sizeof(buf)) > 0 && buf[0] == '1') {
            applySafeModeOverrides();
        }
    }

    // Check for RAWRXD_* environment variables that override config
    // Format: RAWRXD_SECTION_KEY maps to section.key
    // e.g., RAWRXD_INFERENCE_MAX_TOKENS -> inference.maxTokens

    const char* envOverrides[] = {
        "RAWRXD_OLLAMA_BASE_URL",
        "RAWRXD_OLLAMA_MODEL",
        "RAWRXD_LOG_LEVEL",
        "RAWRXD_FEATURES_VULKAN",
        "RAWRXD_INFERENCE_THREADS",
        "RAWRXD_INFERENCE_MAX_TOKENS",
        "RAWRXD_THEME",
        "RAWRXD_AGENT_CYCLE_COUNT",
        nullptr
    };

    struct EnvMapping { const char* envVar; const char* configKey; };
    const EnvMapping mappings[] = {
        {"RAWRXD_OLLAMA_BASE_URL", "ollama.baseUrl"},
        {"RAWRXD_OLLAMA_MODEL", "ollama.modelOverride"},
        {"RAWRXD_LOG_LEVEL", "logging.level"},
        {"RAWRXD_FEATURES_VULKAN", "features.vulkanCompute"},
        {"RAWRXD_INFERENCE_THREADS", "inference.threadCount"},
        {"RAWRXD_INFERENCE_MAX_TOKENS", "inference.maxTokens"},
        {"RAWRXD_THEME", "theme.name"},
        {"RAWRXD_AGENT_CYCLE_COUNT", "agent.cycleCount"},
    };

    for (const auto& mapping : mappings) {
        char buf[512] = {};
        DWORD len = GetEnvironmentVariableA(mapping.envVar, buf, sizeof(buf));
        if (len > 0 && len < sizeof(buf)) {
            m_values[mapping.configKey] = std::string(buf, len);
            LOG_INFO(std::string("[IDEConfig] Env override: ") + mapping.envVar + " -> " + mapping.configKey + " = " + buf);
        }
    }
}

std::vector<std::string> IDEConfig::getAllKeys() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> keys;
    keys.reserve(m_values.size());  // Avoid realloc during iteration
    for (const auto& [key, _] : m_values) {
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

unsigned int IDEConfig::getTerminalTimeoutMs(bool isAgenticTask,
                                            const std::string& requirementHint) const
{
    {
        unsigned int perRun;
        { std::lock_guard<std::mutex> lock(m_mutex); perRun = m_terminalTimeoutPerRunMs; }
        if (perRun > 0)
            return perRun;
    }

    std::string mode = getString("terminal.timeoutMode", "fixed");
    int base = getInt("terminal.timeoutMs", 30000);
    int minMs = getInt("terminal.timeoutMinMs", 10000);
    int maxMs = getInt("terminal.timeoutMaxMs", 120000);
    int autoAdjustPct = std::max(0, std::min(100, getInt("terminal.timeoutAutoAdjustPercent", 25)));

    // Requirement hint overrides isAgenticTask for effective base
    if (!requirementHint.empty()) {
        if (requirementHint == "audit") base = std::max(base, 120000);
        else if (requirementHint == "agentic") base = std::max(base, 60000);
        else if (requirementHint == "quick") base = std::min(base, 15000);
    } else if (isAgenticTask)
        base = std::max(base, 60000);

    if (mode == "random") {
        minMs = std::max(1000, std::min(minMs, maxMs));
        maxMs = std::max(minMs, maxMs);
        int range = maxMs - minMs + 1;
        static std::mt19937 rng(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
        return static_cast<unsigned int>(minMs + (rng() % range));
    }
    if (mode == "auto") {
        int effective = base;
        if (autoAdjustPct > 0) {
            static std::mt19937 rng(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
            int delta = (base * autoAdjustPct / 100);
            std::uniform_int_distribution<int> dist(-delta, delta);
            effective = std::max(1000, base + dist(rng));
        }
        return static_cast<unsigned int>(effective);
    }
    return static_cast<unsigned int>(std::max(1000, base));
}

void IDEConfig::setTerminalTimeoutPerRunMs(unsigned int ms)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_terminalTimeoutPerRunMs = ms;
}

unsigned int IDEConfig::getTerminalTimeoutPerRunMs() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_terminalTimeoutPerRunMs;
}

void IDEConfig::clearTerminalTimeoutPerRunMs()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_terminalTimeoutPerRunMs = 0;
}

unsigned int IDEConfig::setTerminalTimeoutPerRunFromConfig(bool isAgenticTask,
                                                          const std::string& requirementHint)
{
    // Compute fresh from config (random/auto yields new value each run) and store for this run
    unsigned int ms = computeTerminalTimeoutMs(isAgenticTask, requirementHint);
    setTerminalTimeoutPerRunMs(ms);
    return ms;
}

unsigned int IDEConfig::computeTerminalTimeoutMs(bool isAgenticTask,
                                                 const std::string& requirementHint) const
{
    // Always compute from config (ignores per-run override) for UI display / pre-flight
    std::string mode = getString("terminal.timeoutMode", "fixed");
    int base = getInt("terminal.timeoutMs", 30000);
    int minMs = getInt("terminal.timeoutMinMs", 10000);
    int maxMs = getInt("terminal.timeoutMaxMs", 120000);
    int autoAdjustPct = std::max(0, std::min(100, getInt("terminal.timeoutAutoAdjustPercent", 25)));

    if (!requirementHint.empty()) {
        if (requirementHint == "audit") base = std::max(base, 120000);
        else if (requirementHint == "agentic") base = std::max(base, 60000);
        else if (requirementHint == "quick") base = std::min(base, 15000);
    } else if (isAgenticTask)
        base = std::max(base, 60000);

    if (mode == "random") {
        minMs = std::max(1000, std::min(minMs, maxMs));
        maxMs = std::max(minMs, maxMs);
        int range = maxMs - minMs + 1;
        static std::mt19937 rng(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
        return static_cast<unsigned int>(minMs + (rng() % range));
    }
    if (mode == "auto") {
        int effective = base;
        if (autoAdjustPct > 0) {
            static std::mt19937 rng(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
            int delta = (base * autoAdjustPct / 100);
            std::uniform_int_distribution<int> dist(-delta, delta);
            effective = std::max(1000, base + dist(rng));
        }
        return static_cast<unsigned int>(effective);
    }
    return static_cast<unsigned int>(std::max(1000, base));
}

// ============================================================================
// MetricsCollector — Prometheus-compatible text export
// ============================================================================
std::string MetricsCollector::exportPrometheus() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;

    // Export counters
    for (const auto& [name, value] : m_counters) {
        std::string metricName = "rawrxd_" + name;
        // Replace dots/dashes with underscores for Prometheus
        for (char& c : metricName) {
            if (c == '.' || c == '-' || c == ' ') c = '_';
        }
        oss << "# TYPE " << metricName << " counter\n";
        oss << metricName << " " << value << "\n";
    }

    // Export gauges
    for (const auto& [name, value] : m_gauges) {
        std::string metricName = "rawrxd_" + name;
        for (char& c : metricName) {
            if (c == '.' || c == '-' || c == ' ') c = '_';
        }
        oss << "# TYPE " << metricName << " gauge\n";
        oss << metricName << " " << value << "\n";
    }

    // Export histograms (as summary)
    for (const auto& [name, hist] : m_histograms) {
        std::string metricName = "rawrxd_" + name;
        for (char& c : metricName) {
            if (c == '.' || c == '-' || c == ' ') c = '_';
        }
        oss << "# TYPE " << metricName << " summary\n";
        oss << metricName << "_count " << hist.count << "\n";
        oss << metricName << "_sum " << hist.sum << "\n";
        if (hist.count > 0) {
            oss << metricName << "_avg " << hist.avg() << "\n";
            oss << metricName << "_min " << hist.min << "\n";
            oss << metricName << "_max " << hist.max << "\n";
        }
    }

    return oss.str();
}
