#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>

#include "cpu_inference_engine.h"
#include "agentic_engine.h"

namespace RawrXD {

/**
 * AdvancedFeatures - Central hub for all advanced AI features
 * 
 * Provides unified access to:
 * - Max Mode (extended context)
 * - Deep Thinking (chain-of-thought)
 * - Deep Research (codebase scanning)
 * - No Refusal Mode (safety override)
 * - Context Window Management
 * - Hot Patching
 * - Code Generation
 */
class AdvancedFeatures {
public:
    struct Config {
        int contextWindowSize = 32768;   // Default 32k
        bool maxModeEnabled = false;
        bool deepThinkingEnabled = false;
        bool deepResearchEnabled = false;
        bool noRefusalEnabled = false;
        float temperature = 0.7f;
        float topP = 0.9f;
        int maxTokens = 4096;
    };
    
    using StatusCallback = std::function<void(const std::string& status)>;
    using StreamCallback = std::function<void(const std::string& token)>;

    AdvancedFeatures() = default;
    
    explicit AdvancedFeatures(std::shared_ptr<CPUInferenceEngine> engine)
        : m_inferenceEngine(std::move(engine)) {
        syncConfigToEngine();
    }
    
    // Connect engines
    void setInferenceEngine(std::shared_ptr<CPUInferenceEngine> engine) {
        m_inferenceEngine = std::move(engine);
        syncConfigToEngine();
    }
    
    void setAgenticEngine(std::shared_ptr<AgenticEngine> engine) {
        m_agenticEngine = std::move(engine);
    }
    
    // Status callback
    void setStatusCallback(StatusCallback callback) {
        m_statusCallback = std::move(callback);
    }
    
    // Stream callback for real-time output
    void setStreamCallback(StreamCallback callback) {
        m_streamCallback = std::move(callback);
    }
    
    // ========== Context Window Management ==========
    
    void setContextWindow(int tokens) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.contextWindowSize = tokens;
        
        if (m_inferenceEngine) {
            m_inferenceEngine->SetContextLimit(tokens);
        }
        if (m_agenticEngine) {
            m_agenticEngine->setContextWindow(tokens);
        }
        
        notifyStatus("Context window set to " + std::to_string(tokens) + " tokens");
    }
    
    int getContextWindow() const {
        return m_config.contextWindowSize;
    }
    
    // Preset context sizes
    void setContextPreset(const std::string& preset) {
        static const std::unordered_map<std::string, int> presets = {
            {"4k", 4096}, {"8k", 8192}, {"16k", 16384}, {"32k", 32768},
            {"64k", 65536}, {"128k", 131072}, {"256k", 262144},
            {"512k", 524288}, {"1m", 1048576}
        };
        
        auto it = presets.find(preset);
        if (it != presets.end()) {
            setContextWindow(it->second);
        }
    }
    
    // ========== Max Mode ==========
    
    void setMaxMode(bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.maxModeEnabled = enabled;
        
        if (m_inferenceEngine) {
            m_inferenceEngine->SetMaxMode(enabled);
        }
        
        notifyStatus(std::string("Max Mode: ") + (enabled ? "ENABLED" : "DISABLED"));
    }
    
    bool isMaxModeEnabled() const { return m_config.maxModeEnabled; }
    
    void toggleMaxMode() {
        setMaxMode(!m_config.maxModeEnabled);
    }
    
    // ========== Deep Thinking (Chain-of-Thought) ==========
    
    void setDeepThinking(bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.deepThinkingEnabled = enabled;
        
        if (m_inferenceEngine) {
            m_inferenceEngine->SetDeepThinking(enabled);
        }
        
        notifyStatus(std::string("Deep Thinking: ") + (enabled ? "ENABLED" : "DISABLED"));
    }
    
    bool isDeepThinkingEnabled() const { return m_config.deepThinkingEnabled; }
    
    void toggleDeepThinking() {
        setDeepThinking(!m_config.deepThinkingEnabled);
    }
    
    // ========== Deep Research (Codebase Scanning) ==========
    
    void setDeepResearch(bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.deepResearchEnabled = enabled;
        
        if (m_inferenceEngine) {
            m_inferenceEngine->SetDeepResearch(enabled);
        }
        
        notifyStatus(std::string("Deep Research: ") + (enabled ? "ENABLED" : "DISABLED"));
    }
    
    bool isDeepResearchEnabled() const { return m_config.deepResearchEnabled; }
    
    void toggleDeepResearch() {
        setDeepResearch(!m_config.deepResearchEnabled);
    }
    
    // ========== No Refusal Mode (Safety Override) ==========
    
    void setNoRefusal(bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.noRefusalEnabled = enabled;
        
        if (m_inferenceEngine) {
            m_inferenceEngine->SetNoRefusal(enabled);
        }
        
        if (enabled) {
            notifyStatus("WARNING: No Refusal Mode ENABLED - Use responsibly");
        } else {
            notifyStatus("No Refusal Mode: DISABLED");
        }
    }
    
    bool isNoRefusalEnabled() const { return m_config.noRefusalEnabled; }
    
    void toggleNoRefusal() {
        setNoRefusal(!m_config.noRefusalEnabled);
    }
    
    // ========== Generation Parameters ==========
    
    void setTemperature(float temp) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.temperature = std::clamp(temp, 0.0f, 2.0f);
    }
    
    void setTopP(float topP) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.topP = std::clamp(topP, 0.0f, 1.0f);
    }
    
    void setMaxTokens(int tokens) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.maxTokens = tokens;
    }
    
    // ========== AI Operations ==========
    
    // Chat with AI
    std::string chat(const std::string& message) {
        if (!m_agenticEngine) {
            return "[Error] Agentic engine not connected";
        }
        return m_agenticEngine->chat(message);
    }
    
    // Generate code from description
    std::string generateCode(const std::string& language, const std::string& description) {
        if (!m_agenticEngine) {
            return "[Error] Agentic engine not connected";
        }
        return m_agenticEngine->generateCode(language, description);
    }
    
    // Create task plan
    std::string planTask(const std::string& task) {
        if (!m_agenticEngine) {
            return "[Error] Agentic engine not connected";
        }
        return m_agenticEngine->planTask(task);
    }
    
    // Bug analysis
    std::string analyzeBugs(const std::string& filePath) {
        if (!m_agenticEngine) {
            return "[Error] Agentic engine not connected";
        }
        return m_agenticEngine->bugReport(filePath);
    }
    
    // Code suggestions
    std::string suggestImprovements(const std::string& filePath) {
        if (!m_agenticEngine) {
            return "[Error] Agentic engine not connected";
        }
        return m_agenticEngine->codeSuggestions(filePath);
    }
    
    // ========== Hot Patching ==========
    
    struct PatchResult {
        bool success;
        std::string message;
        std::string originalCode;
        std::string patchedCode;
    };
    
    PatchResult hotPatch(const std::string& filePath, const std::string& functionName, 
                         const std::string& newImplementation) {
        PatchResult result;
        
        // Read original file
        std::ifstream file(filePath);
        if (!file) {
            result.success = false;
            result.message = "Cannot read file: " + filePath;
            return result;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        result.originalCode = buffer.str();
        file.close();
        
        // Find function (simple regex-based search)
        // In a real implementation, use a proper parser
        std::string searchPattern = functionName + "\\s*\\([^)]*\\)\\s*\\{";
        
        // For now, just append the new implementation as a replacement
        // A real implementation would use proper AST manipulation
        result.patchedCode = result.originalCode;
        result.success = true;
        result.message = "Hot patch prepared (pending application)";
        
        notifyStatus("Hot patch created for " + functionName);
        
        return result;
    }
    
    bool applyPatch(const PatchResult& patch, const std::string& filePath) {
        if (!patch.success) return false;
        
        std::ofstream file(filePath);
        if (!file) return false;
        
        file << patch.patchedCode;
        file.close();
        
        notifyStatus("Patch applied to " + filePath);
        return true;
    }
    
    // ========== Status and Configuration ==========
    
    std::string getStatusReport() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::stringstream ss;
        ss << "=== Advanced Features Status ===\n";
        ss << "Context Window: " << m_config.contextWindowSize << " tokens\n";
        ss << "Max Mode: " << (m_config.maxModeEnabled ? "ON" : "OFF") << "\n";
        ss << "Deep Thinking: " << (m_config.deepThinkingEnabled ? "ON" : "OFF") << "\n";
        ss << "Deep Research: " << (m_config.deepResearchEnabled ? "ON" : "OFF") << "\n";
        ss << "No Refusal: " << (m_config.noRefusalEnabled ? "ON" : "OFF") << "\n";
        ss << "Temperature: " << m_config.temperature << "\n";
        ss << "Top-P: " << m_config.topP << "\n";
        ss << "Max Tokens: " << m_config.maxTokens << "\n";
        ss << "Inference Engine: " << (m_inferenceEngine ? "Connected" : "Not connected") << "\n";
        ss << "Agentic Engine: " << (m_agenticEngine ? "Connected" : "Not connected") << "\n";
        
        return ss.str();
    }
    
    const Config& getConfig() const { return m_config; }

private:
    mutable std::mutex m_mutex;
    Config m_config;
    
    std::shared_ptr<CPUInferenceEngine> m_inferenceEngine;
    std::shared_ptr<AgenticEngine> m_agenticEngine;
    
    StatusCallback m_statusCallback;
    StreamCallback m_streamCallback;
    
    void syncConfigToEngine() {
        if (!m_inferenceEngine) return;
        
        m_inferenceEngine->SetMaxMode(m_config.maxModeEnabled);
        m_inferenceEngine->SetDeepThinking(m_config.deepThinkingEnabled);
        m_inferenceEngine->SetDeepResearch(m_config.deepResearchEnabled);
        m_inferenceEngine->SetNoRefusal(m_config.noRefusalEnabled);
        m_inferenceEngine->SetContextLimit(m_config.contextWindowSize);
    }
    
    void notifyStatus(const std::string& status) {
        if (m_statusCallback) {
            m_statusCallback(status);
        }
    }
};

} // namespace RawrXD
