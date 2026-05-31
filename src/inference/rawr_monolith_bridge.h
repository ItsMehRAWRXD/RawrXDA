/*
====================================================================
 RAWR MONOLITH v2 INTEGRATION BRIDGE
 Connects mmap-based GGUF loader to RawrXD speculative execution
====================================================================
 
 Integration Points:
   1. MMapFile + GGUFModel → Replace simulated weights in Transformer::bind()
   2. PagedKVCache → Wire into existing KVRollbackManager
   3. SpecDecoder → Replace simulation in SpeculativeExecutionEngine
   4. PrefetchEngine → Add to ghost text pipeline
   5. MoERouter → Wire into spec.telemetry.expert_id gauge

 Usage:
   // In speculative_execution_engine.cpp:
   #include "rawr_monolith_bridge.h"
   
   // Replace simulation:
   RawrMonolithBridge bridge;
   bridge.loadModel("model.gguf");
   auto result = bridge.generate(context, maxTokens);
====================================================================
*/

#pragma once

#include "rawr_monolith_v2.cpp"
#include "speculative_execution_engine.h"
#include <memory>
#include <functional>

namespace rawrxd {

// ============================================================================
// Monolith-to-Speculative Bridge
// ============================================================================
// Adapts the mmap-based monolith engine to the RawrXD speculative interface.
// This enables zero-copy weight loading and vLLM-style paged KV cache.

class RawrMonolithBridge {
public:
    struct Config {
        std::string modelPath;
        std::string draftModelPath;  // Optional: smaller draft model
        int maxSeqLen = 2048;
        int kvCachePages = 1024;
        int numThreads = 0;  // 0 = auto-detect
        bool enablePrefetch = true;
        bool enableMoE = true;
        bool enableSpeculative = true;
    };

    RawrMonolithBridge();
    ~RawrMonolithBridge();

    // Load model via mmap (instant startup)
    bool loadModel(const std::string& ggufPath);
    
    // Load draft model for speculative decoding
    bool loadDraftModel(const std::string& ggufPath);
    
    // Generate tokens using the monolith engine
    struct GenerationResult {
        std::vector<int> tokens;
        std::string text;
        double totalMs;
        double tokensPerSecond;
        float acceptanceRate;
        int expertId;
    };
    
    GenerationResult generate(
        const std::vector<int>& context,
        int maxTokens,
        float temperature = 0.7f
    );
    
    // Stream tokens via callback (for ghost text)
    void generateStream(
        const std::vector<int>& context,
        int maxTokens,
        float temperature,
        std::function<bool(int token, const std::string& text)> callback
    );
    
    // Get diagnostic frame for telemetry
    monolith::SpeculativeExecutionEngine::DiagnosticFrame getDiagnosticFrame() const;
    
    // Get model metadata
    int getEmbeddingDim() const { return m_dim; }
    int getNumLayers() const { return m_nLayers; }
    int getNumHeads() const { return m_nHeads; }
    int getVocabSize() const { return m_vocabSize; }
    int getNumExperts() const { return m_nExperts; }
    
    // KV Cache management
    int createSequence();
    void destroySequence(int seqId);
    void clearAllSequences();
    
    // Prefetch control
    void prefetchLayer(int layerIndex);
    void prefetchRange(int startLayer, int count);
    
    // MoE routing stats
    struct MoEStats {
        std::vector<double> expertWeights;
        std::vector<int> expertTrials;
        std::vector<int> expertSelections;
    };
    MoEStats getMoEStats() const;

private:
    std::unique_ptr<monolith::GGUFModel> m_model;
    std::unique_ptr<monolith::GGUFModel> m_draftModel;
    std::unique_ptr<monolith::Transformer> m_transformer;
    std::unique_ptr<monolith::Transformer> m_draftTransformer;
    std::unique_ptr<monolith::PagedKVCache> m_kvCache;
    std::unique_ptr<monolith::PagedKVCache> m_draftKvCache;
    std::unique_ptr<monolith::MoERouter> m_moeRouter;
    std::unique_ptr<monolith::PrefetchEngine> m_prefetch;
    std::unique_ptr<monolith::Tokenizer> m_tokenizer;
    std::unique_ptr<monolith::ComputeBackend> m_compute;
    
    int m_dim = 512;
    int m_nLayers = 4;
    int m_nHeads = 8;
    int m_vocabSize = 32000;
    int m_nExperts = 8;
    int m_nextSeqId = 0;
    
    std::atomic<float> m_lastAcceptanceRate{0.0f};
    std::atomic<int> m_lastExpertId{-1};
    std::atomic<double> m_lastLatencyMs{0.0};
};

// ============================================================================
// Integration with SpeculativeExecutionEngine
// ============================================================================
// This adapter allows the monolith engine to be used as the backend
// for RawrXD's speculative execution pipeline.

class MonolithSpeculativeAdapter {
public:
    MonolithSpeculativeAdapter(SpeculativeExecutionEngine& engine);
    ~MonolithSpeculativeAdapter();
    
    // Configure the monolith backend
    bool configure(const std::string& mainModelPath, const std::string& draftModelPath = "");
    
    // Generate speculative tokens
    struct SpecResult {
        std::vector<uint32_t> draftTokens;
        std::vector<float> draftLogProbs;
        std::vector<uint32_t> acceptedTokens;
        float acceptanceRate;
        int64_t draftMs;
        int64_t verifyMs;
    };
    
    SpecResult generateSpeculative(
        const std::vector<uint32_t>& context,
        int numDraftTokens,
        float temperature
    );
    
    // KV Rollback support
    void checkpointKV();
    void rollbackKV();
    void advanceKV(int numTokens);
    
    // Get current diagnostic frame
    SpeculativeExecutionEngine::DiagnosticFrame getFrame() const;

private:
    SpeculativeExecutionEngine& m_engine;
    std::unique_ptr<RawrMonolithBridge> m_bridge;
    std::vector<monolith::KVRollbackPoint> m_kvCheckpoints;
};

// ============================================================================
// Integration with Win32IDE Ghost Text
// ============================================================================
// Provides the interface for ghost text completion using the monolith engine.

class MonolithGhostTextProvider {
public:
    using CompletionCallback = std::function<void(const std::string& completion, double latencyMs)>;
    
    MonolithGhostTextProvider();
    ~MonolithGhostTextProvider();
    
    // Initialize with model path
    bool initialize(const std::string& modelPath);
    
    // Request completion (async)
    void requestCompletion(
        const std::string& context,
        const std::string& language,
        int maxTokens,
        CompletionCallback callback
    );
    
    // Cancel pending request
    void cancelRequest();
    
    // Get telemetry for status bar
    struct Telemetry {
        float acceptanceRate;
        int tokensProduced;
        double draftLatencyMs;
        double verifyLatencyMs;
        double totalMs;
        int expertId;
        double effectiveTps;
        double roi;
    };
    
    Telemetry getTelemetry() const;

private:
    std::unique_ptr<RawrMonolithBridge> m_bridge;
    std::atomic<bool> m_cancelled{false};
    std::thread m_workerThread;
    
    Telemetry m_lastTelemetry;
    std::mutex m_telemetryMutex;
};

// ============================================================================
// Factory for creating monolith instances
// ============================================================================

class MonolithFactory {
public:
    // Create a monolith bridge with default config
    static std::unique_ptr<RawrMonolithBridge> create(const std::string& modelPath);
    
    // Create with custom config
    static std::unique_ptr<RawrMonolithBridge> create(const RawrMonolithBridge::Config& config);
    
    // Create speculative adapter for existing engine
    static std::unique_ptr<MonolithSpeculativeAdapter> createAdapter(
        SpeculativeExecutionEngine& engine,
        const std::string& mainModelPath,
        const std::string& draftModelPath = ""
    );
    
    // Create ghost text provider
    static std::unique_ptr<MonolithGhostTextProvider> createGhostProvider(
        const std::string& modelPath
    );
};

} // namespace rawrxd