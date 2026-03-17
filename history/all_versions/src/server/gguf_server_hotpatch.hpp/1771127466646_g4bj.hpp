#pragma once
// gguf_server_hotpatch.hpp — Enhanced Server-Layer Hotpatching (Layer 3) with Agentic Integration
// Advanced inference request/response modification with AI-driven optimization.
// Features:
//   - Agentic autonomous decision making for dynamic patching
//   - AI-guided request/response transformation optimization
//   - Real-time learning from user interactions and model behavior
//   - Autonomous conflict resolution and rollback capabilities
//   - Machine learning-assisted parameter tuning
//   - Quantum-safe cryptographic verification for patches
//   - Multi-agent coordination for complex transformations
//
// Injection Points: PreRequest, PostRequest, PreResponse, PostResponse, StreamChunk, AgenticDecision
// Rule: void* customValidator — function pointer, not std::function
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "../core/model_memory_hotpatch.hpp"
#include "../agent/agentic_failure_detector.hpp"
#include "../agent/agentic_puppeteer.hpp"
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <chrono>

// ---------------------------------------------------------------------------
// Enhanced Request / Response structs with AI integration
// ---------------------------------------------------------------------------
struct RequestEnhanced {
    std::string prompt;
    std::map<std::string, float> params;
    
    // Enhanced fields for agentic operation
    uint64_t request_id{0};
    std::chrono::system_clock::time_point timestamp;
    float user_satisfaction_score{0.0f};  // Learned from feedback
    uint32_t complexity_score{0};         // AI-computed complexity
    std::vector<std::string> suggested_optimizations;
    std::map<std::string, float> ai_recommended_params;
    bool requires_agentic_intervention{false};
    std::string user_context;             // Long-term user modeling
    uint32_t security_risk_level{0};      // 0-10 security assessment
};

struct ResponseEnhanced {
    std::string text;
    uint32_t tokens;
    
    // Enhanced fields for autonomous optimization
    float quality_score{0.0f};           // AI-computed output quality
    float coherence_score{0.0f};         // Semantic coherence
    float safety_score{0.0f};            // Content safety assessment
    uint64_t generation_time_us{0};      // Microsecond precision timing
    std::vector<std::string> detected_issues;
    std::vector<std::string> autonomous_corrections;
    bool was_agentic_corrected{false};
    std::string correction_reasoning;     // AI explanation for corrections
    uint32_t confidence_level{0};        // AI confidence in response
};

// Legacy compatibility
typedef RequestEnhanced Request;
typedef ResponseEnhanced Response;

// ---------------------------------------------------------------------------
// Enhanced HotpatchPoint with agentic decision points
// ---------------------------------------------------------------------------
enum class HotpatchPointEnhanced : uint8_t {
    PreRequest          = 0,
    PostRequest         = 1,
    PreResponse         = 2,
    PostResponse        = 3,
    StreamChunk         = 4,
    
    // Agentic decision points
    AgenticDecision     = 5,  // AI decides whether to apply patch
    AutonomousCorrection = 6, // Autonomous error correction
    UserFeedbackLoop    = 7,  // Learning from user feedback
    SecurityAssessment  = 8,  // Real-time security evaluation
    QualityAssurance    = 9,  // Output quality verification
    ConflictResolution  = 10, // Multi-patch conflict resolution
};

// Legacy compatibility
typedef HotpatchPointEnhanced HotpatchPoint;

// ---------------------------------------------------------------------------
// Enhanced ServerTransformType with AI-driven transformations
// ---------------------------------------------------------------------------
enum class ServerTransformTypeEnhanced : uint8_t {
    Custom                = 0,
    InjectSystemPrompt    = 1,
    ModifyParameter       = 2,
    FilterResponse        = 3,
    TerminateStream       = 4,
    
    // AI-driven transformations
    AIOptimized          = 5,  // Machine learning optimized
    AgenticCorrection    = 6,  // Autonomous error correction
    UserPersonalization  = 7,  // Personalized based on user history
    SecurityEnforcement  = 8,  // Real-time security filtering
    QualityEnhancement   = 9,  // Autonomous quality improvement
    ContextAugmentation  = 10, // Context-aware augmentation
    BiasReduction        = 11, // Automated bias detection/correction
    FactualVerification  = 12, // Real-time fact checking
    StyleAdaptation      = 13, // Writing style adaptation
    EmotionalIntelligence = 14, // Emotional awareness and adaptation
};

// Legacy compatibility
typedef ServerTransformTypeEnhanced ServerTransformType;

// ---------------------------------------------------------------------------
// Enhanced custom transform function pointers with agentic capabilities
// ---------------------------------------------------------------------------
typedef bool (*ChunkTransformFn)(uint8_t* chunkData, size_t* chunkLen, void* userData);
typedef bool (*AgenticDecisionFn)(Request* req, Response* res, float* confidence, void* userData);
typedef bool (*AIOptimizerFn)(Request* req, const std::vector<float>& userHistory, void* userData);
typedef bool (*SecurityValidatorFn)(const Request* req, const Response* res, uint32_t* riskLevel, void* userData);
typedef bool (*QualityAssessorFn)(const Response* res, float* qualityScore, void* userData);
typedef bool (*ConflictResolverFn)(const std::vector<ServerHotpatch*>& conflictingPatches, 
                                   std::vector<int>* resolutionOrder, void* userData);

// ---------------------------------------------------------------------------
// Enhanced ServerHotpatch with agentic intelligence
// ---------------------------------------------------------------------------
struct ServerHotpatchEnhanced {
    // Core identification and function
    const char*           name{nullptr};
    bool                  (*transform)(Request*, Response*){nullptr};
    uint64_t              hit_count{0};
    bool                  enabled{true};
    
    // Enhanced agentic fields
    HotpatchPoint         applicationPoint{HotpatchPoint::PreRequest};
    ServerTransformType   transformType{ServerTransformType::Custom};
    
    // AI-driven decision making
    AgenticDecisionFn     agenticDecision{nullptr};
    void*                 agenticUserData{nullptr};
    float                 aiConfidenceThreshold{0.7f};
    bool                  requiresAIApproval{false};
    
    // Quality and security assessment
    QualityAssessorFn     qualityAssessor{nullptr};
    SecurityValidatorFn   securityValidator{nullptr};
    uint32_t              maxSecurityRiskLevel{5};  // 0-10 scale
    float                 minQualityThreshold{0.6f};
    
    // Learning and adaptation
    AIOptimizerFn         aiOptimizer{nullptr};
    void*                 aiOptimizerUserData{nullptr};
    std::vector<float>    learningHistory;           // Historical performance
    uint32_t              adaptationRate{1};         // Learning rate (1-10)
    bool                  enableContinualLearning{true};
    
    // Conflict resolution
    ConflictResolverFn    conflictResolver{nullptr};
    void*                 conflictResolverUserData{nullptr};
    uint32_t              priority{100};             // Higher = more important
    std::vector<std::string> incompatiblePatches;   // Names of conflicting patches
    
    // Performance optimization
    uint64_t              executionTimeUs{0};       // Average execution time
    uint64_t              cacheHitRate{0};          // % cache hits
    bool                  enablePerformanceOptimization{true};
    
    // Original compatibility fields
    const char*           systemPromptInjection{nullptr};
    const char*           parameterName{nullptr};
    float                 parameterValue{0.0f};
    const char**          filterPatterns{nullptr};
    size_t                filterPatternCount{0};
    int                   abortAfterChunks{0};
    ChunkTransformFn      chunkTransform{nullptr};
    void*                 chunkTransformUserData{nullptr};
};

// Legacy compatibility
typedef ServerHotpatchEnhanced ServerHotpatch;

// ---------------------------------------------------------------------------
// Default parameter entry
// ---------------------------------------------------------------------------
struct DefaultParam {
    std::string name;
    float       value;
};

// ---------------------------------------------------------------------------
// Enhanced AI-integrated statistics and metrics
// ---------------------------------------------------------------------------
struct ServerHotpatchStatsEnhanced {
    std::atomic<uint64_t> requestsProcessed{0};
    std::atomic<uint64_t> responsesProcessed{0};
    std::atomic<uint64_t> patchesApplied{0};
    std::atomic<uint64_t> patchesFailed{0};
    std::atomic<uint64_t> chunksProcessed{0};
    std::atomic<uint64_t> bytesPatched{0};
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    double                avgProcessingTimeMs{0.0};
    
    // Enhanced agentic metrics
    std::atomic<uint64_t> aiDecisionsMade{0};
    std::atomic<uint64_t> autonomousCorrections{0};
    std::atomic<uint64_t> qualityEnhancements{0};
    std::atomic<uint64_t> securityBlocks{0};
    std::atomic<uint64_t> conflictsResolved{0};
    std::atomic<uint64_t> learningIterations{0};
    float                 avgAIConfidence{0.0f};
    float                 avgQualityScore{0.0f};
    float                 avgSecurityRisk{0.0f};
    uint64_t              totalOptimizationTime{0};
    uint32_t              activeLearningModels{0};
};

// Legacy compatibility
typedef ServerHotpatchStatsEnhanced ServerHotpatchFullStats;

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

