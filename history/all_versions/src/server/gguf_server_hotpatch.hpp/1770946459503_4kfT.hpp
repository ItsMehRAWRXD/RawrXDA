#pragma once
// gguf_server_hotpatch.hpp — Server-Layer Hotpatching (Layer 3)
// Modify inference request/response at runtime.
// Injection Points: PreRequest, PostRequest, PreResponse, PostResponse, StreamChunk
// Rule: void* customValidator — function pointer, not std::function
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "../core/model_memory_hotpatch.hpp"
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <mutex>
#include <atomic>

// ---------------------------------------------------------------------------
// Request / Response structs (used by transform functions)
// ---------------------------------------------------------------------------
struct Request {
    std::string prompt;
    std::map<std::string, float> params;
};

struct Response {
    std::string text;
    uint32_t tokens;
};

// ---------------------------------------------------------------------------
// HotpatchPoint — where in the pipeline this patch fires
// ---------------------------------------------------------------------------
enum class HotpatchPoint : uint8_t {
    PreRequest    = 0,
    PostRequest   = 1,
    PreResponse   = 2,
    PostResponse  = 3,
    StreamChunk   = 4,
};

// ---------------------------------------------------------------------------
// ServerHotpatch transform types
// ---------------------------------------------------------------------------
enum class ServerTransformType : uint8_t {
    Custom            = 0,
    InjectSystemPrompt = 1,
    ModifyParameter   = 2,
    FilterResponse    = 3,
    TerminateStream   = 4,
};

// ---------------------------------------------------------------------------
// Custom stream chunk transform — function pointer (NOT std::function)
// ---------------------------------------------------------------------------
typedef bool (*ChunkTransformFn)(uint8_t* chunkData, size_t* chunkLen, void* userData);

// ---------------------------------------------------------------------------
// ServerHotpatch — single hotpatch registration
// ---------------------------------------------------------------------------
struct ServerHotpatch {
    const char*           name{nullptr};
    bool                  (*transform)(Request*, Response*){nullptr};
    uint64_t              hit_count{0};
    bool                  enabled{true};
    HotpatchPoint         applicationPoint{HotpatchPoint::PreRequest};
    ServerTransformType   transformType{ServerTransformType::Custom};

    // InjectSystemPrompt data
    const char*           systemPromptInjection{nullptr};

    // ModifyParameter data
    const char*           parameterName{nullptr};
    float                 parameterValue{0.0f};

    // FilterResponse data
    const char**          filterPatterns{nullptr};
    size_t                filterPatternCount{0};

    // TerminateStream data
    int                   abortAfterChunks{0};

    // Custom chunk-level transform (for StreamChunk point)
    ChunkTransformFn      chunkTransform{nullptr};
    void*                 chunkTransformUserData{nullptr};
};

// ---------------------------------------------------------------------------
// Default parameter entry
// ---------------------------------------------------------------------------
struct DefaultParam {
    std::string name;
    float       value;
};

// ---------------------------------------------------------------------------
// ServerHotpatchStats
// ---------------------------------------------------------------------------
struct ServerHotpatchFullStats {
    std::atomic<uint64_t> requestsProcessed{0};
    std::atomic<uint64_t> responsesProcessed{0};
    std::atomic<uint64_t> patchesApplied{0};
    std::atomic<uint64_t> patchesFailed{0};
    std::atomic<uint64_t> chunksProcessed{0};
    std::atomic<uint64_t> bytesPatched{0};
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    double                avgProcessingTimeMs{0.0};
};

// ---------------------------------------------------------------------------
// GGUFServerHotpatch — Main class (Server Layer 3)
// ---------------------------------------------------------------------------
class GGUFServerHotpatch {
public:
    static GGUFServerHotpatch& instance();

    // ---- Hotpatch Management ----
    void        add_patch(const ServerHotpatch& patch);
    bool        apply_patches(Request* req, Response* res);
    bool        removePatch(const char* name);
    bool        enablePatch(const char* name, bool enable);
    bool        hasPatch(const char* name) const;
    size_t      clearAllPatches();
    size_t      patchCount() const;
    const std::vector<ServerHotpatch>& getActivePatches() const;

    // ---- Request/Response Processing ----
    void processRequest(Request* req);
    void processResponse(Response* res);

    // ---- Stream Chunk Processing ----
    // Returns false if stream should be terminated (RST injection)
    bool processStreamChunk(uint8_t* chunkData, size_t* chunkLen, int chunkIndex);

    // ---- Byte-Level Patching ----
    size_t patchRequestBytes(uint8_t* data, size_t dataLen, size_t bufferCapacity);
    size_t patchResponseBytes(uint8_t* data, size_t dataLen, size_t bufferCapacity);

    // ---- Default Parameter Management ----
    void setDefaultParameter(const char* name, float value);
    void clearDefaultParameter(const char* name);
    void clearAllDefaultParameters();

    // ---- Response Caching ----
    void setCachingEnabled(bool enable);
    bool isCachingEnabled() const;
    void clearCache();
    uint64_t computeCacheKey(const char* data, size_t len) const;
    bool hasCachedResponse(uint64_t key) const;
    const Response* getCachedResponse(uint64_t key);
    void cacheResponse(uint64_t key, const Response& response);

    // ---- Direct Memory API ----
    void* attachToModelMemory(const char* modelPath, size_t* outSize);
    PatchResult detachFromModelMemory();
    size_t readModelMemory(size_t offset, size_t size, void* outBuf) const;
    PatchResult writeModelMemory(size_t offset, const void* data, size_t size);
    int64_t searchModelMemory(size_t startOffset, const void* pattern, size_t patLen) const;
    void* getModelMemoryPointer(size_t offset);

    // ---- Memory Region Locking ----
    PatchResult lockMemoryRegion(size_t offset, size_t size);
    PatchResult unlockMemoryRegion(size_t offset, size_t size);

    // ---- Tensor Operations ----
    PatchResult modifyWeight(const char* tensorName, size_t indexOffset,
                             const void* newValue, size_t valueSize);
    PatchResult cloneTensor(const char* srcTensor, const char* dstTensor);
    PatchResult swapTensors(const char* tensor1, const char* tensor2);

    // ---- Vocabulary Patching ----
    PatchResult patchVocabularyEntry(int tokenId, const char* newToken);

    // ---- Enable/Disable ----
    void setEnabled(bool enable);
    bool isEnabled() const;

    // ---- Statistics ----
    const ServerHotpatchFullStats& getStats() const;
    void resetStats();

private:
    GGUFServerHotpatch();
    ~GGUFServerHotpatch();
    GGUFServerHotpatch(const GGUFServerHotpatch&) = delete;
    GGUFServerHotpatch& operator=(const GGUFServerHotpatch&) = delete;

    // Internal helpers
    int64_t findPattern(const uint8_t* data, size_t dataLen,
                        const uint8_t* pattern, size_t patLen,
                        size_t startPos) const;
    void    injectSystemPrompt(Request* req, const char* prompt);
    void    filterResponseContent(Response* res, const char** patterns, size_t count);

    mutable std::mutex                              m_mutex;
    ServerHotpatchFullStats                         m_stats;
    bool                                            m_enabled{true};
    bool                                            m_cachingEnabled{false};
    int                                             m_currentChunkIndex{0};

    std::vector<ServerHotpatch>                     m_patches;
    std::vector<DefaultParam>                       m_defaultParams;
    std::unordered_map<uint64_t, Response>           m_responseCache;

    // Model memory (direct API)
    uint8_t*                                        m_modelData{nullptr};
    size_t                                          m_modelDataSize{0};
    bool                                            m_modelOwned{false};
};

