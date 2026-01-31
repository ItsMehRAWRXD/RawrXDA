// gguf_server_hotpatch.hpp - Server-side GGUF request/response hotpatcher
#pragma once


#include <functional>
#include "model_memory_hotpatch.hpp"

// Hotpatch application points in the request/response pipeline
enum class HotpatchPoint {
    PreRequest,      // Before request is sent to model
    PostRequest,     // After request processing, before inference
    PreResponse,     // Before response is returned to client
    PostResponse,    // After response is fully generated
    StreamChunk      // During streaming response (per-chunk)
};

// Server-side hotpatch structure
struct ServerHotpatch {
    std::string name;
    HotpatchPoint applicationPoint;
    bool enabled = true;
    
    // Transform types
    enum TransformType {
        InjectSystemPrompt,       // Add system prompt to request
        ModifyParameter,          // Change parameter value (temperature, top_p, etc.)
        FilterResponse,           // Filter/censor response content
        TerminateStream,          // RST injection - abort stream early
        CacheResponse,            // Cache response for identical requests
        ModifyTokenLogits         // Modify token probabilities
    };
    TransformType transformType;
    
    // Configuration data
    std::string systemPromptInjection;
    std::string parameterName;
    std::any parameterValue;
    std::vector<std::string> filterPatterns;      // For response filtering
    int abortAfterChunks = -1;       // For stream termination (-1 = disabled)
    
    // Transform function (for custom logic)
    std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)> customTransform;
};

class GGUFServerHotpatch : public void
{

public:
    explicit GGUFServerHotpatch(void* parent = nullptr);
    ~GGUFServerHotpatch() override;

    // Hotpatch management
    void addHotpatch(const ServerHotpatch& patch);
    void removeHotpatch(const std::string& name);
    void enableHotpatch(const std::string& name, bool enable);
    bool hasHotpatch(const std::string& name) const;
    ServerHotpatch getHotpatch(const std::string& name) const;
    std::vector<std::string> listHotpatches() const;
    void clearAllHotpatches();

    // Request/Response processing
    void* processRequest(const void*& request);
    void* processResponse(const void*& response);
    std::vector<uint8_t> processStreamChunk(const std::vector<uint8_t>& chunk, int chunkIndex);
    
    // Parameter manipulation (zero-copy byte patching)
    std::vector<uint8_t> patchRequestBytes(const std::vector<uint8_t>& requestData);
    std::vector<uint8_t> patchResponseBytes(const std::vector<uint8_t>& responseData);
    
    // Default parameter overrides
    void setDefaultParameter(const std::string& name, const std::any& value);
    void clearDefaultParameter(const std::string& name);
    std::unordered_map<std::string, std::any> getDefaultParameters() const;
    
    // Response caching
    void setCachingEnabled(bool enable);
    bool isCachingEnabled() const;
    void clearCache();
    std::string getCacheKey(const void*& request) const;
    bool hasCachedResponse(const std::string& key) const;
    void* getCachedResponse(const std::string& key);
    void cacheResponse(const std::string& key, const void*& response);
    
    // Statistics
    struct Stats {
        int64_t requestsProcessed = 0;
        int64_t responsesProcessed = 0;
        int64_t chunksProcessed = 0;
        int64_t cacheHits = 0;
        int64_t cacheMisses = 0;
        int64_t bytesPatched = 0;
        int64_t patchesApplied = 0;
        double avgProcessingTimeMs = 0.0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();
    
    // Enable/Disable entire system
    void setEnabled(bool enable);
    bool isEnabled() const;
    
    // Direct Memory Manipulation API for Model Access
    void* attachToModelMemory(const std::string& modelPath);
    PatchResult detachFromModelMemory();
    
    std::vector<uint8_t> readModelMemory(size_t offset, size_t size) const;
    PatchResult writeModelMemory(size_t offset, const std::vector<uint8_t>& data);
    
    PatchResult modifyWeight(const std::string& tensorName, size_t indexOffset, const std::vector<uint8_t>& newValue);
    PatchResult modifyWeightsBatch(const std::unordered_map<std::string, std::unordered_map<size_t, std::vector<uint8_t>>>& modifications);
    
    PatchResult injectTemporaryData(size_t offset, const std::vector<uint8_t>& data, int durationMs);
    std::vector<uint8_t> extractTensorWeights(const std::string& tensorName, size_t offset, size_t size) const;
    PatchResult transformTensorWeights(const std::string& tensorName, std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)> transform);
    
    PatchResult cloneTensor(const std::string& sourceTensor, const std::string& destTensor);
    PatchResult swapTensors(const std::string& tensor1, const std::string& tensor2);
    PatchResult applyMemoryPatch(const std::unordered_map<size_t, std::vector<uint8_t>>& patches);
    int64_t searchModelMemory(size_t startOffset, const std::vector<uint8_t>& pattern) const;
    
    void* getModelMemoryPointer(size_t offset = 0);
    PatchResult lockMemoryRegion(size_t offset, size_t size);
    PatchResult unlockMemoryRegion(size_t offset, size_t size);
    
    // Tensor dependency tracking
    bool hasTensorDependency(const std::string& tensorName, const std::string& dependencyName) const;
    std::vector<std::string> getTensorDependencies(const std::string& tensorName) const;
    
    // Vocabulary patching
    PatchResult patchVocabularyEntry(int tokenId, const std::string& newToken);

    void hotpatchApplied(const std::string& name, HotpatchPoint point);
    void requestModified(const void*& original, const void*& modified);
    void responseModified(const void*& original, void*& modified);
    void streamTerminated(int chunkIndex, const std::string& reason);
    void cacheHit(const std::string& key);
    void errorOccurred(const std::string& error);

private:
    // Helper methods
    std::vector<uint8_t> bytePatchInPlace(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement);
    int64_t findPattern(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, int64_t startPos = 0) const;
    void* injectSystemPrompt(const void*& request, const std::string& prompt);
    void* modifyParameter(const void*& request, const std::string& param, const std::any& value);
    void* filterResponse(const void*& response, const std::vector<std::string>& patterns);
    
    // Data members
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ServerHotpatch> m_hotpatches;
    std::unordered_map<std::string, std::any> m_defaultParams;
    std::unordered_map<std::string, void*> m_responseCache;
    
    std::vector<uint8_t> m_modelData;         // Model data for direct memory operations
    std::string m_modelPath;            // Current model path
    std::unordered_map<std::string, size_t> m_tensorOffsets;  // Tensor name -> offset mapping
    std::unordered_map<std::string, std::vector<std::string>> m_tensorDependencies;  // Tensor name -> list of dependencies
    
    Stats m_stats;
    bool m_enabled = true;
    bool m_cachingEnabled = false;
    int m_currentChunkIndex = 0;
    
    std::chrono::system_clock::time_point m_lastProcessTime;
};


