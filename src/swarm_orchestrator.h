#pragma once
#include <vector>
#include <string>
#include <future>
#include <deque>
#include <thread>
#include <mutex>
#include <cstdint>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <span>
#include <unordered_map>
// Check if compiler supports <expected>, if not rely on utils/Expected not matching std::expected
// But .cpp uses std::expected explicitly.
#if __has_include(<expected>)
#include <expected>
#else
// Fallback if needed, but MinGW 15 should have it
#endif
#include <nlohmann/json.hpp>
#include "CommonTypes.h"
#include "utils/Expected.h"
#include "local_vector_db.h"

namespace rawrxd::orchestration {
class InferencePacer;
}

namespace RawrXD {

enum class SwarmLane : std::uint8_t {
    Primary = 0,
    Verifier = 1,
    Librarian = 2
};

struct LibrarianLanePolicy {
    bool enabled = false;
    std::uint32_t dispatchEveryTokens = 32;
    std::uint32_t minPulseBytes = 1024;
    std::uint32_t maxPendingLookups = 8;
};

// Phase 4.3: RAG Attribution metadata for hover insights and UI highlighting
struct SourceMetadata {
    std::string sourceFile;      // Local file path where the snippet came from
    int lineNumber = 0;          // Starting line in the source file
    float hybridScore = 0.0f;    // Combined BM25+cosine score from reranker
    float semanticScore = 0.0f;  // Pure HNSW cosine similarity (before BM25)
    float lexicalScore = 0.0f;   // Pure BM25 lexical score (before cosine)
    bool fromLibrarian = false;  // True if injected by Librarian lane
};

struct RefCountedPulseBuffer {
    std::uint64_t requestId = 0;
    std::string payload;
    std::atomic<std::uint32_t> refs{0};
    SourceMetadata sourceAttribution;  // Phase 4.3: Attribution for highlighting

    std::uint32_t addRef(std::uint32_t count = 1) noexcept {
        return refs.fetch_add(count, std::memory_order_acq_rel) + count;
    }

    std::uint32_t releaseRef() noexcept {
        return refs.fetch_sub(1, std::memory_order_acq_rel) - 1;
    }
};

enum class SwarmError {
    Success = 0,
    InitializationFailed,
    TaskSubmissionFailed,
    ConsensusFailed,
    Timeout,
    InternalError
};

struct SwarmResult {
    std::string consensus;
    float confidence;
    std::vector<std::string> individualThoughts;
    double executionTimeMs;
};

struct OrchestratorTask {
    std::string id;
    std::string description;
    std::string context;
    int priority;
    std::promise<SwarmResult> promise;
};

struct WorkerQueue {
    std::mutex mutex;
    std::deque<std::unique_ptr<OrchestratorTask>> tasks;
};

struct DecodedToken {
    std::uint64_t requestId = 0;
    std::uint32_t tokenIndex = 0;
    std::string token;
    bool speculative = true;
};

class SwarmOrchestrator {
public:
    SwarmOrchestrator(size_t numWorkers = 0);
    ~SwarmOrchestrator();

    void shutdown();

    // Matching .cpp implementation
#if defined(__cpp_lib_expected) || (defined(_MSVC_LANG) && _MSVC_LANG >= 202202L)
    std::expected<SwarmResult, int> executeTask(const std::string& task, const std::string& context);
#else
    RawrXD::Expected<SwarmResult, int> executeTask(const std::string& task, const std::string& context);
#endif
    std::future<SwarmResult> submitTaskAsync(const std::string& taskDesc, const std::string& context); // .cpp name
    // Also provide alias for submitTask which might be used by others
    std::future<SwarmResult> submitTask(const std::string& desc, const std::string& ctx) {
        return submitTaskAsync(desc, ctx);
    }
    
    // Public for thread entry point or make private + friend
    void loop(int workerId);

    bool stealWork(int thiefId, OrchestratorTask& stolenTask);
    SwarmResult synthesizeConsensus(const std::vector<std::string>& results);

    using PulseBufferPtr = std::shared_ptr<RefCountedPulseBuffer>;

    void attachPacer(::rawrxd::orchestration::InferencePacer* pacer) noexcept;
    PulseBufferPtr createSharedPulseBuffer(std::uint64_t requestId, const std::string& serializedPulse, std::uint32_t initialRefs = 2) const;
    bool submitSharedPulse(const PulseBufferPtr& pulse);
    bool tryPopPulseForLane(SwarmLane lane, PulseBufferPtr& outPulse);
    void completePulseForLane(SwarmLane lane, const PulseBufferPtr& pulse);
    void configureLibrarianPolicy(const LibrarianLanePolicy& policy);
    LibrarianLanePolicy getLibrarianPolicy() const;
    static void SetGlobalLibrarianSimilarityThreshold(float threshold) noexcept {
        const float clamped = (threshold < 0.0f) ? 0.0f : ((threshold > 1.0f) ? 1.0f : threshold);
        librarianSimilarityThresholdStorage().store(clamped, std::memory_order_relaxed);
    }
    static float GetGlobalLibrarianSimilarityThreshold() noexcept {
        return librarianSimilarityThresholdStorage().load(std::memory_order_relaxed);
    }

    // Phase 4.2.2: Live BM25-vs-cosine balance for the hybrid reranker.
    // wBM25 is stored; wCosine = 1 - wBM25.  Range [0, 1].
    // Default 0.6 (BM25-heavy) as established in Phase 4.2.2 implementation.
    static void SetGlobalHybridBM25Weight(float w) noexcept {
        const float clamped = (w < 0.0f) ? 0.0f : ((w > 1.0f) ? 1.0f : w);
        hybridBM25WeightStorage().store(clamped, std::memory_order_relaxed);
    }
    static float GetGlobalHybridBM25Weight() noexcept {
        return hybridBM25WeightStorage().load(std::memory_order_relaxed);
    }
    static void PublishLatestSourceAttribution(const SourceMetadata& metadata) {
        {
            std::lock_guard<std::mutex> lock(latestAttributionMutex());
            latestAttributionStorage() = metadata;
        }
        latestAttributionAvailable().store(true, std::memory_order_release);
    }
    static bool ConsumeLatestSourceAttribution(SourceMetadata& outMetadata) {
        if (!latestAttributionAvailable().exchange(false, std::memory_order_acq_rel)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(latestAttributionMutex());
        outMetadata = latestAttributionStorage();
        return outMetadata.fromLibrarian;
    }
    // Phase 6: Hotpatch diagnostics bridge.
    // Publishes actionable runtime patch diagnostics to the Librarian so semantic search can
    // retrieve related code paths when memory patch failures happen.
    static void PublishHotpatchDiagnosticsHint(const std::string& hint) {
        if (hint.empty()) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(hotpatchHintMutex());
            hotpatchHintStorage() = hint;
        }
        hotpatchHintAvailable().store(true, std::memory_order_release);
    }
    static bool ConsumeHotpatchDiagnosticsHint(std::string& outHint) {
        if (!hotpatchHintAvailable().exchange(false, std::memory_order_acq_rel)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(hotpatchHintMutex());
        outHint = hotpatchHintStorage();
        return !outHint.empty();
    }
    void onPrimaryTokenEmitted(std::uint64_t requestId, std::uint32_t tokenIndex);

    double CalculateDisagreementScore(std::span<const float> primaryLogits, std::span<const float> verifierLogits) const noexcept;
    double EvaluateAndReportDivergence(
        std::uint64_t requestId,
        std::uint32_t tokenIndex,
        std::span<const float> primaryLogits,
        std::span<const float> verifierLogits,
        double threshold = 0.85) noexcept;

    bool ExecutePivot(std::uint64_t requestId, std::uint32_t tokenIndex, const std::string& correctedToken);
    void enqueueDecodedToken(std::uint64_t requestId, std::uint32_t tokenIndex, const std::string& token,
                            bool speculative = true);
    std::vector<DecodedToken> drainDecodedTokens(std::size_t maxCount);
    
    // Phase 4.1: Librarian RAG Context Injection
    // Injects augmented context (RAG snippets) into the Primary lane for the next prefill watermark.
    // Called by Librarian worker thread after HNSW search + re-ranking completes.
    bool InjectLibrarianContext(std::uint64_t requestId, std::string_view ragContext);
    
    // Phase 4.2: Librarian Worker Loop
    // Implements the main processing loop for the Librarian lane:
    //   1. Pop pulse from m_librarianPulseQueue
    //   2. Extract context window (last 256 tokens)
    //   3. Search HNSW index with context embedding
    //   4. If match found: inject context via InjectLibrarianContext()
    //   5. Release pulse and repeat
    // This loop runs asynchronously in a dedicated thread and operates independently of Primary lane.
    void RunLibrarianLoop();
    
    nlohmann::json getStatus() const; 

    // Legacy initialize stub
    RawrXD::Expected<void, SwarmError> initialize() { return {}; }

private:
    struct PendingPivot
    {
         std::uint32_t tokenIndex = 0;
         std::string correctedToken;
    };

    struct HNSWSearchResult {
        bool found = false;
        std::string snippet;
        float similarity = 0.0f;  // Hybrid score (BM25+cosine) after reranking
        // Phase 4.3: Source attribution for highlighting and UI insights
        std::string sourceFile;
        int lineNumber = 0;
        float semanticScore = 0.0f;   // Pure cosine similarity (before BM25)
        float lexicalScore = 0.0f;    // Pure BM25 score (before cosine)
    };

    // Phase 4.2.2: Reranker result — carries both the raw HNSW cosine score and the
    // BM25 lexical score so the hybrid gate can be surfaced in diagnostics.
    struct RerankResult {
        std::string snippet;
        float similarity  = 0.0f;  // Original HNSW cosine similarity (Gate 1)
        float rerankScore = 0.0f;  // Normalized BM25 lexical relevance (Gate 2)
        float hybridScore = 0.0f;  // Combined acceptance score (slider threshold applied here)
        // Phase 4.3: Source attribution for UI highlighting
        std::string sourceFile;
        int lineNumber = 0;
    };

    std::uint32_t selectTopTokenIndex(std::span<const float> logits) const noexcept;
    std::string ResolveTokenText(std::uint32_t tokenId) const;
    std::string buildPivotToken(std::uint32_t tokenId) const;

    std::size_t invalidateDecodedTokensAfter(std::uint64_t requestId, std::uint32_t tokenIndex);
    
    // Phase 4.2: Internal helper for HNSW vector search + Phase 4.2.2 reranker pass.
    // Gate 1 (Retriever): fetches top-K=10 candidates from LocalVectorDB (HNSW).
    // Gate 2 (Reranker): calls PerformRerank to produce hybrid scores.
    // Returns the best candidate — similarity field carries the hybridScore.
    HNSWSearchResult _searchHNSWIndex(std::string_view contextWindow);

    // Phase 4.2.2: BM25 + cosine hybrid reranker.
    // Runs on the Librarian thread immediately after HNSW retrieval.
    // Returns candidates sorted by hybridScore descending (best first).
    std::vector<RerankResult> PerformRerank(std::string_view query,
                                            const std::vector<VectorSearchResult>& candidates);
    
    // Phase 4.1: Internal helper to extract and clear pending RAG context for a request.
    // Called when assembling the next Primary pulse to incorporate injected context.
    std::string _extractAndClearPendingRAGContext(std::uint64_t requestId);

    // Phase 5: File hashing for incremental indexing dirty-check
    // Computes SHA256 of entire file for change detection
    std::string _computeFileHash(const std::string& filePath);

   std::vector<std::unique_ptr<WorkerQueue>> m_queues;
   std::vector<std::thread> m_threads;
   std::atomic<bool> m_running{false};
   std::atomic<size_t> m_totalTasksExecuted{0};
    ::rawrxd::orchestration::InferencePacer* m_pacer = nullptr;

    mutable std::mutex m_pulseMutex;
    std::deque<PulseBufferPtr> m_primaryPulseQueue;
    std::deque<PulseBufferPtr> m_verifierPulseQueue;
    std::deque<PulseBufferPtr> m_librarianPulseQueue;
    std::condition_variable m_pulseCv;

    mutable std::mutex m_librarianPolicyMutex;
    LibrarianLanePolicy m_librarianPolicy{};
    std::unordered_map<std::uint64_t, std::uint32_t> m_lastLibrarianDispatchToken;
    std::unordered_map<std::uint64_t, std::uint32_t> m_librarianPendingLookups;
    std::atomic<std::uint64_t> m_librarianDispatchCount{0};

    mutable std::mutex m_pivotMutex;
    std::unordered_map<std::uint64_t, PendingPivot> m_pendingPrimaryPivots;

    // Phase 4.1: RAG Context Injection Tracking
    // Stores pending RAG context snippets awaiting injection into Primary lane's next watermark.
    // Key: requestId, Value: RAG context string (e.g., docstring or code snippet)
    mutable std::mutex m_ragInjectionMutex;
    std::unordered_map<std::uint64_t, std::string> m_pendingRAGInjections;
    std::atomic<std::uint64_t> m_ragContextInjectCount{0};  // Telemetry counter

    mutable std::mutex m_decodeMutex;
    std::deque<DecodedToken> m_decodeToUiQueue;

    static std::atomic<float>& librarianSimilarityThresholdStorage() noexcept {
        static std::atomic<float> threshold{0.65f};
        return threshold;
    }
    static std::atomic<float>& hybridBM25WeightStorage() noexcept {
        static std::atomic<float> weight{0.6f};
        return weight;
    }
    static std::mutex& latestAttributionMutex() noexcept {
        static std::mutex mutex;
        return mutex;
    }
    static SourceMetadata& latestAttributionStorage() noexcept {
        static SourceMetadata metadata;
        return metadata;
    }
    static std::atomic<bool>& latestAttributionAvailable() noexcept {
        static std::atomic<bool> available{false};
        return available;
    }
    static std::mutex& hotpatchHintMutex() noexcept {
        static std::mutex mutex;
        return mutex;
    }
    static std::string& hotpatchHintStorage() noexcept {
        static std::string hint;
        return hint;
    }
    static std::atomic<bool>& hotpatchHintAvailable() noexcept {
        static std::atomic<bool> available{false};
        return available;
    }

    // Phase 4.2.1: LocalVectorDB for semantic code search (HNSW + TinyBERT)
    std::unique_ptr<LocalVectorDB> m_vectorDB;
    // Phase 5: SQLite3 persistence for incremental indexing
    std::unique_ptr<class VectorDBPersistence> m_indexPersistence;
    bool m_librarianThreadStarted = false;
    std::thread m_librarianThread;
};

} // namespace RawrXD
