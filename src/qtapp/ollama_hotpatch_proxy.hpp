// ollama_hotpatch_proxy.hpp - Ollama-specific hotpatch proxy with memory injection
#pragma once


#include <functional>

// Ollama-specific patch rule
struct OllamaHotpatchRule {
    std::string name;
    std::string description;
    bool enabled = true;
    
    enum RuleType {
        ParameterInjection,      // Inject/override request parameters
        ResponseTransform,       // Transform response data
        MemoryBypass,           // Bypass safety checks via memory manipulation
        TokenBiasing,           // Bias token probabilities
        ContextInjection,       // Inject system context
        LayerSkipping,          // Skip inference layers
        WeightModification      // Direct weight tensor modification
    };
    RuleType ruleType;
    
    // Rule parameters
    std::string targetModel;        // Model name pattern (empty = all)
    std::unordered_map<std::string, std::any> parameters;
    std::vector<uint8_t> searchPattern;
    std::vector<uint8_t> replacementData;
    int priority = 0;
    
    // Custom transformation function
    std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)> customTransform;
};

// Ollama request/response structure
struct OllamaMessage {
    std::string role;               // "user", "assistant", "system"
    std::string content;
    void* metadata;
};

class OllamaHotpatchProxy : public void {

public:
    explicit OllamaHotpatchProxy(void* parent = nullptr);
    ~OllamaHotpatchProxy() override;

    // Rule management
    void addRule(const OllamaHotpatchRule& rule);
    void removeRule(const std::string& name);
    void enableRule(const std::string& name, bool enable);
    bool hasRule(const std::string& name) const;
    OllamaHotpatchRule getRule(const std::string& name) const;
    std::vector<std::string> listRules(const std::string& modelPattern = std::string()) const;
    void clearAllRules();
    void setPriorityOrder(const std::vector<std::string>& ruleNames);

    // Request processing (pre-inference)
    void* processRequestJson(const void*& request);
    std::vector<uint8_t> processRequestBytes(const std::vector<uint8_t>& requestData);
    
    // Response processing (post-inference)
    void* processResponseJson(const void*& response);
    std::vector<uint8_t> processResponseBytes(const std::vector<uint8_t>& responseData);
    
    // Streaming chunk processing
    std::vector<uint8_t> processStreamChunk(const std::vector<uint8_t>& chunk, int chunkIndex);
    void beginStreamProcessing(const std::string& streamId);
    void endStreamProcessing(const std::string& streamId);

    // Direct memory manipulation for requests
    PatchResult injectIntoRequest(const std::string& key, const std::any& value);
    PatchResult injectIntoRequestBatch(const std::unordered_map<std::string, std::any>& injections);
    std::any extractFromRequest(const std::string& key) const;
    std::unordered_map<std::string, std::any> extractAllRequestParams() const;

    // Direct memory manipulation for responses
    PatchResult modifyInResponse(const std::string& jsonPath, const std::any& newValue);
    std::any readFromResponse(const std::string& jsonPath) const;
    
    // Parameter override system
    void setParameterOverride(const std::string& paramName, const std::any& value);
    void clearParameterOverride(const std::string& paramName);
    std::unordered_map<std::string, std::any> getParameterOverrides() const;
    
    // Model-specific targeting
    bool matchesModel(const std::string& modelName, const std::string& pattern) const;
    void setActiveModel(const std::string& modelName);
    std::string getActiveModel() const;

    // Caching for performance
    void setResponseCachingEnabled(bool enable);
    bool isResponseCachingEnabled() const;
    void clearResponseCache();
    
    // Statistics
    struct Stats {
        qint64 requestsProcessed = 0;
        qint64 responsesProcessed = 0;
        qint64 chunksProcessed = 0;
        qint64 rulesApplied = 0;
        qint64 bytesModified = 0;
        qint64 cachesHits = 0;
        qint64 transformationsApplied = 0;
        double avgProcessingTimeMs = 0.0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();
    
    // Enable/Disable entire proxy
    void setEnabled(bool enable);
    bool isEnabled() const;
    
    // Diagnostics
    void enableDiagnostics(bool enable);
    std::vector<std::string> getDiagnosticLog() const;
    void clearDiagnosticLog();

    void ruleApplied(const std::string& name, const std::string& type);
    void requestModified(const void*& original, const void*& modified);
    void responseModified(const void*& original, const void*& modified);
    void parameterInjected(const std::string& paramName, const std::any& value);
    void streamChunkProcessed(int chunkIndex, int bytesModified);
    void modelChanged(const std::string& modelName);
    void errorOccurred(const std::string& error);
    void diagnosticMessage(const std::string& message);

private:
    // Helper methods
    void* applyParameterInjection(const void*& request, const OllamaHotpatchRule& rule);
    void* applyResponseTransform(const void*& response, const OllamaHotpatchRule& rule);
    void* applyContextInjection(const void*& request, const std::string& context);
    std::vector<uint8_t> applyBytePatching(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement);
    
    bool shouldApplyRule(const OllamaHotpatchRule& rule, const std::string& modelName) const;
    std::string getCacheKey(const void*& request) const;
    
    void logDiagnostic(const std::string& message);

    // Data members
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, OllamaHotpatchRule> m_rules;
    std::vector<std::string> m_ruleOrder;            // Priority-ordered rule names
    std::unordered_map<std::string, std::any> m_parameterOverrides;
    std::unordered_map<std::string, void*> m_responseCache;
    
    std::string m_activeModel;
    Stats m_stats;
    bool m_enabled = true;
    bool m_cachingEnabled = false;
    bool m_diagnosticsEnabled = false;
    std::vector<std::string> m_diagnosticLog;
    
    // Active streams being processed
    std::unordered_map<std::string, int> m_activeStreams;
    
    void** m_statsReportTimer = nullptr;
};

#endif // OLLAMA_HOTPATCH_PROXY_HPP

