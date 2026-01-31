// proxy_hotpatcher.hpp - Agentic correction proxy with token reverse proxy byte hacking
#pragma once


#include <functional>
#include "model_memory_hotpatch.hpp"

// Agent output validation result
struct AgentValidation {
    bool isValid = true;
    std::string errorMessage;
    std::string correctedOutput;
    std::vector<std::string> violations;
    
    static AgentValidation valid() {
        return AgentValidation{true, std::string(), std::string(), std::vector<std::string>()};
    }
    
    static AgentValidation invalid(const std::string& error, const std::string& corrected = std::string()) {
        return AgentValidation{false, error, corrected, std::vector<std::string>()};
    }
};

// Byte-level pattern matching result
struct PatternMatch {
    int64_t position = -1;
    int64_t length = 0;
    std::vector<uint8_t> matchedData;
    
    bool isValid() const { return position >= 0; }
};

// Proxy hotpatch rule
struct ProxyHotpatchRule {
    std::string name;
    bool enabled = true;
    
    enum RuleType {
        ParameterOverride,        // Override request parameters
        ResponseCorrection,       // Fix agent output errors
        StreamTermination,        // RST injection
        AgentValidation,          // Validate agent responses
        MemoryInjection,          // Direct byte patching
        TokenLogitBias           // Bias token probabilities
    };
    RuleType type;
    
    // Rule-specific data
    std::vector<uint8_t> searchPattern;
    std::vector<uint8_t> replacement;
    std::string parameterName;
    std::any parameterValue;
    int abortAfterChunks = -1;
    
    // Agent validation rules
    std::vector<std::string> forbiddenPatterns;
    std::vector<std::string> requiredPatterns;
    bool enforcePlanFormat = false;
    bool enforceAgentFormat = false;
    
    // Custom validator callback pointer (store as void* to avoid template issues)
    void* customValidator = nullptr;
};

class ProxyHotpatcher : public void
{

public:
    explicit ProxyHotpatcher(void* parent = nullptr);
    ~ProxyHotpatcher() override;

    // Rule management
    void addRule(const ProxyHotpatchRule& rule);
    void removeRule(const std::string& name);
    void enableRule(const std::string& name, bool enable);
    bool hasRule(const std::string& name) const;
    ProxyHotpatchRule getRule(const std::string& name) const;
    std::vector<std::string> listRules() const;
    void clearAllRules();

    // Request processing (Memory Injection via Proxy)
    std::vector<uint8_t> processRequest(const std::vector<uint8_t>& requestData);
    void* processRequestJson(const void*& request);
    
    // Response processing (Agent Correction)
    std::vector<uint8_t> processResponse(const std::vector<uint8_t>& responseData);
    void* processResponseJson(const void*& response);
    std::vector<uint8_t> processStreamChunk(const std::vector<uint8_t>& chunk, int chunkIndex);
    
    // Zero-copy byte patching (production-ready)
    std::vector<uint8_t> bytePatchInPlace(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement);
    PatternMatch findPattern(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, int64_t startPos = 0) const;
    std::vector<uint8_t> findAndReplace(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement);
    
    // Boyer-Moore pattern matching (high-performance)
    PatternMatch boyerMooreSearch(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern) const;
    
    // Agent output validation
    AgentValidation validateAgentOutput(const std::vector<uint8_t>& output);
    AgentValidation validatePlanMode(const std::vector<uint8_t>& output);
    AgentValidation validateAgentMode(const std::vector<uint8_t>& output);
    AgentValidation validateAskMode(const std::vector<uint8_t>& output);
    
    // Agent correction logic
    std::vector<uint8_t> correctAgentOutput(const std::vector<uint8_t>& output, const AgentValidation& validation);
    std::vector<uint8_t> enforcePlanFormat(const std::vector<uint8_t>& output);
    std::vector<uint8_t> enforceAgentFormat(const std::vector<uint8_t>& output);
    
    // RST Injection (Response Stream Termination)
    bool shouldTerminateStream(int chunkIndex) const;
    void setStreamTerminationPoint(int chunkCount);
    void clearStreamTermination();
    
    // Statistics
    struct Stats {
        int64_t requestsProcessed = 0;
        int64_t responsesProcessed = 0;
        int64_t chunksProcessed = 0;
        int64_t bytesPatched = 0;
        int64_t patchesApplied = 0;
        int64_t validationFailures = 0;
        int64_t correctionsApplied = 0;
        int64_t streamsTerminated = 0;
        double avgProcessingTimeMs = 0.0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();
    
    // Enable/Disable
    void setEnabled(bool enable);
    bool isEnabled() const;
    
    // Direct Memory Manipulation API (Proxy-Layer)
    PatchResult directMemoryInject(size_t offset, const std::vector<uint8_t>& data);
    PatchResult directMemoryInjectBatch(const std::unordered_map<size_t, std::vector<uint8_t>>& injections);
    std::vector<uint8_t> directMemoryExtract(size_t offset, size_t size) const;
    
    PatchResult replaceInRequestBuffer(const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement);
    PatchResult replaceInResponseBuffer(const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement);
    
    PatchResult injectIntoStream(const std::vector<uint8_t>& chunk, int chunkIndex, const std::vector<uint8_t>& injection);
    std::vector<uint8_t> extractFromStream(const std::vector<uint8_t>& chunk, int startOffset, int length);
    
    PatchResult overwriteTokenBuffer(const std::vector<uint8_t>& tokenData);
    PatchResult modifyLogitsBatch(const std::unordered_map<size_t, float>& logitModifications);
    
    int64_t searchInRequestBuffer(const std::vector<uint8_t>& pattern) const;
    int64_t searchInResponseBuffer(const std::vector<uint8_t>& pattern) const;
    
    PatchResult swapBufferRegions(size_t region1Offset, size_t region2Offset, size_t size);
    PatchResult cloneBufferRegion(size_t sourceOffset, size_t destOffset, size_t size);

    void ruleApplied(const std::string& name, const std::string& context);
    void requestModified(const std::vector<uint8_t>& original, const std::vector<uint8_t>& modified);
    void responseModified(const std::vector<uint8_t>& original, const std::vector<uint8_t>& modified);
    void agentOutputCorrected(const std::string& error, const std::vector<uint8_t>& corrected);
    void validationFailed(const std::string& error, const std::vector<std::string>& violations);
    void streamTerminated(int chunkIndex, const std::string& reason);
    void errorOccurred(const std::string& error);

private:
    // Boyer-Moore preprocessing
    std::unordered_map<uint8_t, int64_t> buildBadCharTable(const std::vector<uint8_t>& pattern) const;
    std::vector<int64_t> buildGoodSuffixTable(const std::vector<uint8_t>& pattern) const;
    
    // Agent validation helpers
    bool checkForbiddenPatterns(const std::vector<uint8_t>& output, std::vector<std::string>& violations);
    bool checkRequiredPatterns(const std::vector<uint8_t>& output, std::vector<std::string>& violations);
    bool isPlanFormatValid(const std::vector<uint8_t>& output);
    bool isAgentFormatValid(const std::vector<uint8_t>& output);
    
    // Data members
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ProxyHotpatchRule> m_rules;
    
    Stats m_stats;
    bool m_enabled = true;
    int m_streamTerminationPoint = -1;
    int m_currentChunkIndex = 0;
    
    std::chrono::steady_clock::time_point m_timer;
};


