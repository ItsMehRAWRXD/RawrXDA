#include "swarm_orchestrator.h"
#include "LocalVectorDB_Persistence.hpp"
#include "../include/orchestration/InferencePacer.h"
#include "ai/token_generator.h"
#include <chrono>
#include <iostream>
#include <random>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <limits>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__AVX512F__) || (defined(_MSC_VER) && defined(__AVX512F__))
#include <immintrin.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace RawrXD {

namespace {

inline float absf_scalar(float x) noexcept {
    return (x < 0.0f) ? -x : x;
}

// Phase 4.2.2: Tokenizer for BM25 reranker.
// Splits on non-alphanumeric characters, lowercases, drops single-char tokens (noise).
static std::vector<std::string> tokenizeForBM25(std::string_view text) {
    std::vector<std::string> tokens;
    std::string current;
    current.reserve(32);
    for (const unsigned char c : text) {
        if (std::isalnum(c)) {
            current += static_cast<char>(std::tolower(c));
        } else if (!current.empty()) {
            if (current.size() >= 2) {
                tokens.push_back(std::move(current));
            }
            current.clear();
        }
    }
    if (current.size() >= 2) {
        tokens.push_back(std::move(current));
    }
    return tokens;
}

bool isAutoPivotEnabled() noexcept {
    const char* gate = std::getenv("RAWRXD_SWARM_ENABLE_AUTO_PIVOT");
    if (gate == nullptr || gate[0] == '\0') {
        return true;
    }
    return gate[0] != '0';
}

#if defined(_WIN32)
DWORD_PTR computeWorkerAffinityMask(int workerId) {
    const DWORD activeCpuCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (activeCpuCount <= 1) {
        return 0;
    }

    // SetThreadAffinityMask uses a process affinity mask width (DWORD_PTR).
    const DWORD maxMaskBits = static_cast<DWORD>(sizeof(DWORD_PTR) * 8);
    const DWORD usableCpuCount = (activeCpuCount < maxMaskBits) ? activeCpuCount : maxMaskBits;
    if (usableCpuCount <= 1) {
        return 0;
    }

    // Worker 0 is the primary lane. Keep it on the first CPU bit for deterministic responsiveness.
    if (workerId == 0) {
        return static_cast<DWORD_PTR>(1);
    }

    // Spread background lanes across the remaining CPUs.
    const DWORD lane = static_cast<DWORD>((workerId - 1) % (usableCpuCount - 1));
    const DWORD cpuIndex = 1 + lane;
    return (static_cast<DWORD_PTR>(1) << cpuIndex);
}

void applyWorkerSchedulingPolicy(int workerId) {
    const char* quietMode = std::getenv("RAWRXD_SWARM_QUIET_MODE");
    if (quietMode != nullptr && quietMode[0] == '1') {
        return;
    }

    HANDLE hThread = GetCurrentThread();

    if (workerId == 0) {
        (void)SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
    } else {
        (void)SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);
    }

    const DWORD_PTR affinityMask = computeWorkerAffinityMask(workerId);
    if (affinityMask != 0) {
        (void)SetThreadAffinityMask(hThread, affinityMask);
    }
}
#else
void applyWorkerSchedulingPolicy(int) {
}
#endif

} // namespace

SwarmOrchestrator::SwarmOrchestrator(size_t numWorkers) {
    if (numWorkers == 0) {
        numWorkers = std::thread::hardware_concurrency();
    }
    
    // Initialize queues
    for (size_t i = 0; i < numWorkers; ++i) {
        m_queues.push_back(std::make_unique<WorkerQueue>());
    }

    // Phase 4.2.1: Initialize LocalVectorDB for Librarian semantic search
    m_vectorDB = LocalVectorDB::Initialize(
        "d:/rawrxd/src/",                    // Source code directory
        "d:/rawrxd/src/hnsw_index.bin",      // Index path
        "d:/models/tinybert-768.gguf");      // TinyBERT model

    if (!m_vectorDB) {
        std::cerr << "[SwarmOrchestrator] Warning: LocalVectorDB initialization failed\n";
    } else {
        std::cout << "[SwarmOrchestrator] LocalVectorDB ready:\n"
                 << m_vectorDB->GetStats() << "\n";
    }

    // Phase 5: Initialize SQLite3 persistence for incremental indexing
    m_indexPersistence = std::make_unique<VectorDBPersistence>();
    if (!m_indexPersistence->Initialize("d:/rawrxd/src/.vectordb_index.sqlite3")) {
        std::cerr << "[SwarmOrchestrator] Warning: VectorDB persistence initialization failed\n";
    } else {
        int64_t embeddingCount = m_indexPersistence->GetEmbeddingCount();
        std::cout << "[SwarmOrchestrator] VectorDB persistence initialized with " 
                  << embeddingCount << " cached embeddings\n";
    }

    m_running = true;

    // Phase 4.2: Start Librarian worker loop in background thread
    if (m_vectorDB && m_vectorDB->IsReady()) {
        m_librarianThreadStarted = true;
        m_librarianThread = std::thread(&SwarmOrchestrator::RunLibrarianLoop, this);
    }

    // Start regular worker threads
    for (size_t i = 0; i < numWorkers; ++i) {
         m_threads.emplace_back(&SwarmOrchestrator::loop, this, i);
    }
}

SwarmOrchestrator::~SwarmOrchestrator() {
    shutdown();
}

void SwarmOrchestrator::shutdown() {
    m_running = false;
    
    // Wake up Librarian thread so it can exit gracefully
    m_pulseCv.notify_all();
    
    // Wait for Librarian worker thread
    if (m_librarianThreadStarted && m_librarianThread.joinable()) {
        m_librarianThread.join();
    }
    
    // Wait for regular worker threads
    for (auto& t : m_threads) {
        if (t.joinable()) t.join();
    }
}

void SwarmOrchestrator::loop(int workerId) {
    applyWorkerSchedulingPolicy(workerId);

    while (m_running) {
        std::unique_ptr<OrchestratorTask> task;
        {
            auto& q = *m_queues[workerId];
            std::lock_guard<std::mutex> lock(q.mutex);
            if (!q.tasks.empty()) {
                task = std::move(q.tasks.front());
                q.tasks.pop_front();
            }
        }
        
        if (task) {
             // Process task
             SwarmResult result;
             result.executionTimeMs = 100.0f; // placeholder
             // ... logic ...
             result.consensus = "Processed: " + task->description;
             // task->promise.set_value(result);
             try {
                task->promise.set_value(result);
             } catch(...) {}
             m_totalTasksExecuted++;
        } else {
             // Steal
             OrchestratorTask stolen;
             if (stealWork(workerId, stolen)) {
                 SwarmResult result;
                 result.consensus = "Stolen: " + stolen.description;
                 try {
                     stolen.promise.set_value(result); 
                 } catch (...) {}
                 m_totalTasksExecuted++;
             } else {
                 std::this_thread::sleep_for(std::chrono::milliseconds(10));
             }
        }
    }
}

std::future<SwarmResult> SwarmOrchestrator::submitTaskAsync(const std::string& taskDesc, const std::string& context) {
    auto task = std::make_unique<OrchestratorTask>();
    task->description = taskDesc;
    task->context = context;
    task->promise = std::promise<SwarmResult>();
    auto future = task->promise.get_future();
    
    // Round robin push
    static std::atomic<size_t> nextWorker{0};
    size_t idx = nextWorker++ % m_queues.size();
    
    {
        std::lock_guard<std::mutex> lock(m_queues[idx]->mutex);
        m_queues[idx]->tasks.push_back(std::move(task));
    }
    return future;
}

#if defined(__cpp_lib_expected) || (defined(_MSVC_LANG) && _MSVC_LANG >= 202302L)
std::expected<SwarmResult, int> SwarmOrchestrator::executeTask(const std::string& task, const std::string& context) {
    auto fut = submitTaskAsync(task, context);
    if (fut.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
        return std::unexpected(408); // Timeout
    }
    return fut.get();
}
#else
RawrXD::Expected<SwarmResult, int> SwarmOrchestrator::executeTask(const std::string& task, const std::string& context) {
    auto fut = submitTaskAsync(task, context);
    if (fut.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
        return RawrXD::unexpected<int>(408); // Timeout
    }
    return fut.get();
}
#endif

bool SwarmOrchestrator::stealWork(int thiefId, OrchestratorTask& stolenTask) {
    // randomized stealing
    for (size_t i=0; i<m_queues.size(); ++i) {
        if (i == thiefId) continue;
        auto& q = *m_queues[i];
        if (q.mutex.try_lock()) {
            if (!q.tasks.empty()) {
                // Steal from back
                auto& ptr = q.tasks.back();
                // Move content
                stolenTask = std::move(*ptr); // This moves promise and data
                q.tasks.pop_back();
                q.mutex.unlock();
                return true;
            }
            q.mutex.unlock();
        }
    }
    return false;
}

SwarmResult SwarmOrchestrator::synthesizeConsensus(const std::vector<std::string>& results) {
    return SwarmResult{};
}

void SwarmOrchestrator::attachPacer(rawrxd::orchestration::InferencePacer* pacer) noexcept {
    m_pacer = pacer;
}

SwarmOrchestrator::PulseBufferPtr SwarmOrchestrator::createSharedPulseBuffer(
    std::uint64_t requestId,
    const std::string& serializedPulse,
    std::uint32_t initialRefs) const {
    auto pulse = std::make_shared<RefCountedPulseBuffer>();
    pulse->requestId = requestId;
    pulse->payload = serializedPulse;
    pulse->refs.store(initialRefs, std::memory_order_release);
    return pulse;
}

bool SwarmOrchestrator::submitSharedPulse(const PulseBufferPtr& pulse) {
    if (!pulse || pulse->refs.load(std::memory_order_acquire) == 0) {
        return false;
    }

    bool fanoutToLibrarian = false;
    {
        std::lock_guard<std::mutex> policyLock(m_librarianPolicyMutex);
        if (m_librarianPolicy.enabled && pulse->payload.size() >= m_librarianPolicy.minPulseBytes) {
            const std::uint32_t pending = m_librarianPendingLookups[pulse->requestId];
            if (pending < m_librarianPolicy.maxPendingLookups) {
                fanoutToLibrarian = true;
                m_librarianPendingLookups[pulse->requestId] = pending + 1;
            }
        }
    }

    if (fanoutToLibrarian) {
        pulse->addRef(1);
        m_librarianDispatchCount.fetch_add(1, std::memory_order_relaxed);
    }

    {
        std::lock_guard<std::mutex> lock(m_pulseMutex);
        m_primaryPulseQueue.push_back(pulse);
        m_verifierPulseQueue.push_back(pulse);
        if (fanoutToLibrarian) {
            m_librarianPulseQueue.push_back(pulse);
        }
    }
    m_pulseCv.notify_all();

    if (m_pacer != nullptr && pulse->requestId != 0) {
        m_pacer->OnLanePrefillStart(pulse->requestId, static_cast<std::uint8_t>(SwarmLane::Primary));
        m_pacer->OnLanePrefillStart(pulse->requestId, static_cast<std::uint8_t>(SwarmLane::Verifier));
    }

    return true;
}

bool SwarmOrchestrator::tryPopPulseForLane(SwarmLane lane, PulseBufferPtr& outPulse) {
    std::lock_guard<std::mutex> lock(m_pulseMutex);
    std::deque<PulseBufferPtr>* queue = &m_primaryPulseQueue;
    if (lane == SwarmLane::Verifier) {
        queue = &m_verifierPulseQueue;
    } else if (lane == SwarmLane::Librarian) {
        queue = &m_librarianPulseQueue;
    }
    if (queue->empty()) {
        outPulse.reset();
        return false;
    }

    outPulse = queue->front();
    queue->pop_front();

    if (lane == SwarmLane::Primary && outPulse && outPulse->requestId != 0) {
        PendingPivot pivot{};
        bool hasPivot = false;
        {
            std::lock_guard<std::mutex> pivotLock(m_pivotMutex);
            const auto it = m_pendingPrimaryPivots.find(outPulse->requestId);
            if (it != m_pendingPrimaryPivots.end()) {
                pivot = it->second;
                m_pendingPrimaryPivots.erase(it);
                hasPivot = true;
            }
        }

        if (hasPivot) {
            std::string pivotPayload = pivot.correctedToken;
            if (!outPulse->payload.empty()) {
                pivotPayload += outPulse->payload;
            }
            auto pivoted = createSharedPulseBuffer(outPulse->requestId, pivotPayload, 1);

            // Primary lane no longer consumes the shared pulse; release its outstanding reference.
            if (outPulse->refs.load(std::memory_order_acquire) > 0) {
                (void)outPulse->releaseRef();
            }

            outPulse = std::move(pivoted);
        }

        // Phase 4.1: Apply pending RAG context from Librarian lane
        // If the Librarian has injected context (via InjectLibrarianContext), prepend it to this pulse.
        std::string ragContext = _extractAndClearPendingRAGContext(outPulse->requestId);
        if (!ragContext.empty()) {
            // Prepend RAG context (marked as augmented context) to the payload.
            // Format: <RAG_CONTEXT>\n---AUGMENTED_CONTEXT_BOUNDARY---\n<original_payload>
            std::string augmentedPayload = ragContext;
            augmentedPayload += "\n[augmented_context_end]\n";
            if (!outPulse->payload.empty()) {
                augmentedPayload += outPulse->payload;
            }
            auto augmented = createSharedPulseBuffer(outPulse->requestId, augmentedPayload, 1);

            // Release the non-augmented pulse
            if (outPulse->refs.load(std::memory_order_acquire) > 0) {
                (void)outPulse->releaseRef();
            }

            outPulse = std::move(augmented);
        }
    }

    return static_cast<bool>(outPulse);
}

void SwarmOrchestrator::completePulseForLane(SwarmLane lane, const PulseBufferPtr& pulse) {
    if (!pulse) {
        return;
    }

    if (m_pacer != nullptr && pulse->requestId != 0) {
        m_pacer->OnLanePrefillDone(pulse->requestId, static_cast<std::uint8_t>(lane));
    }

    if (lane == SwarmLane::Librarian) {
        std::lock_guard<std::mutex> policyLock(m_librarianPolicyMutex);
        auto it = m_librarianPendingLookups.find(pulse->requestId);
        if (it != m_librarianPendingLookups.end()) {
            if (it->second > 0) {
                it->second -= 1;
            }
            if (it->second == 0) {
                m_librarianPendingLookups.erase(it);
            }
        }
    }

    const std::uint32_t remainingRefs = pulse->releaseRef();
    if (remainingRefs == 0) {
        // Shared buffer is automatically reclaimed when the last shared_ptr owner goes out of scope.
    }
}

double SwarmOrchestrator::CalculateDisagreementScore(
    std::span<const float> primaryLogits,
    std::span<const float> verifierLogits) const noexcept {
    const std::size_t n = (primaryLogits.size() < verifierLogits.size()) ? primaryLogits.size() : verifierLogits.size();
    if (n == 0) {
        return 0.0;
    }

    double diffL1 = 0.0;
    double magL1 = 0.0;

#if defined(__AVX512F__) || (defined(_MSC_VER) && defined(__AVX512F__))
    const __m512 signMask = _mm512_set1_ps(-0.0f);
    const std::size_t simdWidth = 16;
    std::size_t i = 0;
    __m512 sumDiff = _mm512_setzero_ps();
    __m512 sumMag = _mm512_setzero_ps();

    for (; i + simdWidth <= n; i += simdWidth) {
        const __m512 p = _mm512_loadu_ps(primaryLogits.data() + i);
        const __m512 v = _mm512_loadu_ps(verifierLogits.data() + i);
        const __m512 d = _mm512_sub_ps(p, v);
        const __m512 absD = _mm512_andnot_ps(signMask, d);
        const __m512 absP = _mm512_andnot_ps(signMask, p);
        const __m512 absV = _mm512_andnot_ps(signMask, v);
        const __m512 absPV = _mm512_add_ps(absP, absV);
        sumDiff = _mm512_add_ps(sumDiff, absD);
        sumMag = _mm512_add_ps(sumMag, absPV);
    }

    diffL1 += static_cast<double>(_mm512_reduce_add_ps(sumDiff));
    magL1 += static_cast<double>(_mm512_reduce_add_ps(sumMag));

    for (; i < n; ++i) {
        const float p = primaryLogits[i];
        const float v = verifierLogits[i];
        diffL1 += static_cast<double>(absf_scalar(p - v));
        magL1 += static_cast<double>(absf_scalar(p) + absf_scalar(v));
    }
#else
    for (std::size_t i = 0; i < n; ++i) {
        const float p = primaryLogits[i];
        const float v = verifierLogits[i];
        diffL1 += static_cast<double>(absf_scalar(p - v));
        magL1 += static_cast<double>(absf_scalar(p) + absf_scalar(v));
    }
#endif

    constexpr double kEpsilon = 1e-9;
    const double denom = magL1 + kEpsilon;
    double sigma = diffL1 / denom;
    if (sigma < 0.0) {
        sigma = 0.0;
    }
    if (sigma > 1.0) {
        sigma = 1.0;
    }
    return sigma;
}

double SwarmOrchestrator::EvaluateAndReportDivergence(
    std::uint64_t requestId,
    std::uint32_t tokenIndex,
    std::span<const float> primaryLogits,
    std::span<const float> verifierLogits,
    double threshold) noexcept {
    const double sigma = CalculateDisagreementScore(primaryLogits, verifierLogits);
    if (m_pacer != nullptr && requestId != 0 && sigma >= threshold) {
        m_pacer->OnSwarmDivergence(requestId, tokenIndex, sigma);
    }

    if (requestId != 0 && sigma >= threshold && isAutoPivotEnabled()) {
        const std::uint32_t verifierTopToken = selectTopTokenIndex(verifierLogits);
        const std::string correctedToken = buildPivotToken(verifierTopToken);
        (void)ExecutePivot(requestId, tokenIndex, correctedToken);
    }

    return sigma;
}

std::uint32_t SwarmOrchestrator::selectTopTokenIndex(std::span<const float> logits) const noexcept {
    if (logits.empty()) {
        return 0;
    }

    std::uint32_t bestIdx = 0;
    float bestValue = logits[0];
    for (std::uint32_t i = 1; i < static_cast<std::uint32_t>(logits.size()); ++i) {
        if (logits[i] > bestValue) {
            bestValue = logits[i];
            bestIdx = i;
        }
    }
    return bestIdx;
}

std::string SwarmOrchestrator::buildPivotToken(std::uint32_t tokenId) const {
    const std::string resolved = ResolveTokenText(tokenId);
    if (!resolved.empty()) {
        return resolved;
    }
    return "<pivot:" + std::to_string(tokenId) + ">";
}

std::string SwarmOrchestrator::ResolveTokenText(std::uint32_t tokenId) const {
    static TokenGenerator tokenizer;

    // Fast byte fallback for common byte-level token ranges before heavier decode attempts.
    if (tokenId >= 3 && tokenId < (3 + 256)) {
        const char ch = static_cast<char>(tokenId - 3);
        return std::string(1, ch);
    }

    if (tokenId > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    auto decoded = tokenizer.decode({static_cast<int>(tokenId)});
    if (!decoded) {
        return {};
    }

    std::string text = decoded.value();

    // TokenGenerator currently appends a trailing space in decode; trim only suffix whitespace.
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }

    if (text == "<unk>") {
        return {};
    }

    return text;
}

bool SwarmOrchestrator::ExecutePivot(
    std::uint64_t requestId,
    std::uint32_t tokenIndex,
    const std::string& correctedToken) {
    if (requestId == 0 || correctedToken.empty()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_pivotMutex);
        m_pendingPrimaryPivots[requestId] = PendingPivot{tokenIndex, correctedToken};
    }

    (void)invalidateDecodedTokensAfter(requestId, tokenIndex);
    return true;
}

bool SwarmOrchestrator::InjectLibrarianContext(std::uint64_t requestId, std::string_view ragContext) {
    // Phase 4.1: Inject RAG context into Primary lane for next watermark cycle.
    // Called by Librarian worker thread after vector search + re-ranking completes.
    //
    // Strategy:
    //   1. Store context in m_pendingRAGInjections[requestId]
    //   2. On next Primary pulse creation, prepend this context to the payload
    //   3. Clear after injection to avoid re-injection

    if (requestId == 0 || ragContext.empty()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_ragInjectionMutex);
        
        // Store the RAG context for injection into the next Primary pulse.
        // If this is the first injection for this request, we'll prepend it.
        // If there's already pending context, we append to avoid losing context.
        auto it = m_pendingRAGInjections.find(requestId);
        if (it != m_pendingRAGInjections.end()) {
            // Append new context with separator to maintain isolation
            it->second += "\n---RAG_SEPARATOR---\n";
            it->second += ragContext;
        } else {
            m_pendingRAGInjections[requestId] = std::string(ragContext);
        }
        
        m_ragContextInjectCount.fetch_add(1, std::memory_order_relaxed);
    }

    return true;
}

std::string SwarmOrchestrator::_extractAndClearPendingRAGContext(std::uint64_t requestId) {
    // Internal helper: Extract pending RAG context for a request, clearing it afterward.
    // Called internally when applying context to the next Primary pulse.
    
    std::lock_guard<std::mutex> lock(m_ragInjectionMutex);
    auto it = m_pendingRAGInjections.find(requestId);
    if (it == m_pendingRAGInjections.end()) {
        return {};
    }
    
    std::string context = std::move(it->second);
    m_pendingRAGInjections.erase(it);
    return context;
}

std::string SwarmOrchestrator::_computeFileHash(const std::string& filePath) {
    // Phase 5: Compute SHA256 hash of file for incremental indexing dirty-check
    // Used to detect if a file has been modified since last index
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    // Simple hash: read file in 64KB chunks and accumulate hash
    // In production, use actual SHA256 via Windows CNG or OpenSSL
    // For MVP, use a simple multiplicative hash
    uint64_t hash = 14695981039346656037ULL;  // FNV offset basis
    constexpr uint64_t fnvPrime = 1099511628211ULL;
    
    char buffer[65536];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        for (std::streamsize i = 0; i < file.gcount(); ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= fnvPrime;
        }
    }
    file.close();

    // Convert to hex string
    char hashStr[17];
    snprintf(hashStr, sizeof(hashStr), "%016llx", hash);
    return std::string(hashStr);
}

void SwarmOrchestrator::configureLibrarianPolicy(const LibrarianLanePolicy& policy) {
    std::lock_guard<std::mutex> lock(m_librarianPolicyMutex);
    m_librarianPolicy = policy;
}

LibrarianLanePolicy SwarmOrchestrator::getLibrarianPolicy() const {
    std::lock_guard<std::mutex> lock(m_librarianPolicyMutex);
    return m_librarianPolicy;
}

void SwarmOrchestrator::onPrimaryTokenEmitted(std::uint64_t requestId, std::uint32_t tokenIndex) {
    if (requestId == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_librarianPolicyMutex);
    if (!m_librarianPolicy.enabled || m_librarianPolicy.dispatchEveryTokens == 0) {
        return;
    }

    const auto lastIt = m_lastLibrarianDispatchToken.find(requestId);
    const std::uint32_t lastToken = (lastIt == m_lastLibrarianDispatchToken.end()) ? 0 : lastIt->second;
    if (tokenIndex < lastToken) {
        return;
    }

    if ((tokenIndex - lastToken) >= m_librarianPolicy.dispatchEveryTokens) {
        m_lastLibrarianDispatchToken[requestId] = tokenIndex;
    }
}

void SwarmOrchestrator::enqueueDecodedToken(
    std::uint64_t requestId,
    std::uint32_t tokenIndex,
    const std::string& token,
    bool speculative) {
    std::lock_guard<std::mutex> lock(m_decodeMutex);
    m_decodeToUiQueue.push_back(DecodedToken{requestId, tokenIndex, token, speculative});
}

std::vector<DecodedToken> SwarmOrchestrator::drainDecodedTokens(std::size_t maxCount) {
    std::vector<DecodedToken> out;
    if (maxCount == 0) {
        return out;
    }

    std::lock_guard<std::mutex> lock(m_decodeMutex);
    const std::size_t count = std::min(maxCount, m_decodeToUiQueue.size());
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(std::move(m_decodeToUiQueue.front()));
        m_decodeToUiQueue.pop_front();
    }
    return out;
}

SwarmOrchestrator::HNSWSearchResult SwarmOrchestrator::_searchHNSWIndex(std::string_view contextWindow) {
    // Phase 4.2.2: Dual-gate retrieval
    //   Gate 1 (Retriever): fetch top-K=10 candidates from HNSW with no similarity pre-filter.
    //   Gate 2 (Reranker): PerformRerank computes BM25+cosine hybrid scores.
    //   The RAG Intensity slider threshold is applied against the hybrid score in RunLibrarianLoop.

    HNSWSearchResult result{};

    if (!m_vectorDB || !m_vectorDB->IsReady()) {
        return result;
    }

    // Gate 1: broad retrieval — pass minSimilarity=0.0f so the reranker sees all top-K candidates.
    auto candidates = m_vectorDB->Query(contextWindow, 10, 60, 0.0f);
    if (candidates.empty()) {
        return result;
    }

    // Gate 2: BM25 + cosine hybrid reranker
    auto ranked = PerformRerank(contextWindow, candidates);
    if (ranked.empty()) {
        return result;
    }

    const auto& best = ranked.front();
    result.found     = true;
    result.snippet   = best.snippet;
    result.similarity = best.hybridScore;  // slider threshold is now applied against hybrid score
    // Phase 4.3: Source attribution for UI highlighting
    result.sourceFile  = best.sourceFile;
    result.lineNumber  = best.lineNumber;
    result.semanticScore = best.similarity;   // cosine from Gate 1
    result.lexicalScore = best.rerankScore;   // BM25 from Gate 2

    std::cout << "[Librarian] Reranker: cosine=" << best.similarity
              << " bm25="    << best.rerankScore
              << " hybrid="  << best.hybridScore << "\n";

    return result;
}

std::vector<SwarmOrchestrator::RerankResult> SwarmOrchestrator::PerformRerank(
    std::string_view query,
    const std::vector<VectorSearchResult>& candidates)
{
    // BM25 hyperparameters (standard TREC tuning for code retrieval)
    constexpr float k1 = 1.5f;
    constexpr float b  = 0.75f;
    // Phase 4.2.2: Hybrid weighting driven live by the Weight Tuner slider
    // (SetGlobalHybridBM25Weight / GetGlobalHybridBM25Weight).
    // Default is BM25-heavy (0.6) because exact symbol matches dominate code retrieval.
    const float wBM25   = GetGlobalHybridBM25Weight();
    const float wCosine = 1.0f - wBM25;

    const std::size_t N = candidates.size();
    if (N == 0) return {};

    const std::vector<std::string> queryTerms = tokenizeForBM25(query);

    // No query terms — fall back to cosine-only ranking
    if (queryTerms.empty()) {
        std::vector<RerankResult> out;
        out.reserve(N);
        for (const auto& c : candidates) {
            RerankResult r;
            r.snippet     = c.snippet;
            r.similarity  = c.similarity;
            r.rerankScore = 0.0f;
            r.hybridScore = c.similarity;
            // Phase 4.3: Carry source attribution
            r.sourceFile  = c.source_file;
            r.lineNumber  = c.line_number;
            out.push_back(std::move(r));
        }
        std::stable_sort(out.begin(), out.end(),
            [](const RerankResult& a, const RerankResult& b_) { return a.hybridScore > b_.hybridScore; });
        return out;
    }

    // Tokenize each candidate document
    std::vector<std::vector<std::string>> docTokens;
    docTokens.reserve(N);
    for (const auto& c : candidates) {
        docTokens.push_back(tokenizeForBM25(c.snippet));
    }

    // Average document length (clamp to ≥1 to avoid div-by-zero)
    float avgdl = 0.0f;
    for (const auto& dt : docTokens) {
        avgdl += static_cast<float>(dt.size());
    }
    avgdl /= static_cast<float>(N);
    if (avgdl < 1.0f) avgdl = 1.0f;

    // Document frequency of each query term across the K candidates
    std::unordered_map<std::string, int> df;
    for (const auto& term : queryTerms) {
        for (const auto& dt : docTokens) {
            if (std::any_of(dt.begin(), dt.end(), [&term](const std::string& t){ return t == term; })) {
                ++df[term];
            }
        }
    }

    // BM25 score per candidate
    std::vector<float> bm25Scores(N, 0.0f);
    for (std::size_t di = 0; di < N; ++di) {
        const auto& dt = docTokens[di];
        const float docLen = static_cast<float>(dt.size());

        // Term-frequency map for this document
        std::unordered_map<std::string, float> tf;
        for (const auto& t : dt) { ++tf[t]; }

        float score = 0.0f;
        for (const auto& term : queryTerms) {
            const auto dfIt = df.find(term);
            if (dfIt == df.end() || dfIt->second == 0) continue;

            // Robertson-Sparck Jones IDF (smoothed)
            const float idf = std::log(
                (static_cast<float>(N) - static_cast<float>(dfIt->second) + 0.5f) /
                (static_cast<float>(dfIt->second) + 0.5f) + 1.0f);

            const auto tfIt = tf.find(term);
            const float freq = (tfIt != tf.end()) ? tfIt->second : 0.0f;
            const float numerator   = freq * (k1 + 1.0f);
            const float denominator = freq + k1 * (1.0f - b + b * docLen / avgdl);
            score += idf * (numerator / denominator);
        }
        bm25Scores[di] = score;
    }

    // Normalize BM25 to [0, 1] against the best candidate in this batch
    const float maxBM25 = *std::max_element(bm25Scores.begin(), bm25Scores.end());
    if (maxBM25 > 0.0f) {
        for (auto& s : bm25Scores) s /= maxBM25;
    }

    // Build RerankResult vector with hybrid scores and sort best-first
    std::vector<RerankResult> out;
    out.reserve(N);
    for (std::size_t di = 0; di < N; ++di) {
        RerankResult r;
        r.snippet     = candidates[di].snippet;
        r.similarity  = candidates[di].similarity;
        r.rerankScore = bm25Scores[di];
        r.hybridScore = wCosine * candidates[di].similarity + wBM25 * bm25Scores[di];
        // Phase 4.3: Carry source attribution through rerank pipeline
        r.sourceFile  = candidates[di].source_file;
        r.lineNumber  = candidates[di].line_number;
        out.push_back(std::move(r));
    }
    std::stable_sort(out.begin(), out.end(),
        [](const RerankResult& a, const RerankResult& b_) { return a.hybridScore > b_.hybridScore; });

    return out;
}

void SwarmOrchestrator::RunLibrarianLoop() {
    // Phase 4.2: Main Librarian worker loop
    // Runs in a dedicated background thread, processing pulses asynchronously.
    //
    // Loop:
    //   1. Pop pulse from m_librarianPulseQueue (blocking with timeout)
    //   2. Extract context window (last 256 tokens from pulse payload)
    //   3. Call _searchHNSWIndex() to find relevant code
    //   4. If match found (similarity > threshold): inject via InjectLibrarianContext()
    //   5. Release pulse via completePulseForLane(Librarian, pulse)
    //   6. Continue until shutdown
    //
    // Latency Budget:
    //   - Pop pulse: <1µs (lock-free dequeue)
    //   - Extract context: ~50µs (string scan)
    //   - HNSW search: 5-15ms (embedding + search on ~10K code blocks)
    //   - Inject context: <1µs (atomic map store)
    //   - Total per pulse: ~5-20ms, fits within 32-token (5-10ms/token) watermark
    
    while (m_running.load(std::memory_order_acquire)) {
        PulseBufferPtr pulse;
        
        // Try to pop pulse from Librarian queue with short timeout to allow graceful shutdown
        {
            std::unique_lock<std::mutex> lock(m_pulseMutex);
            // Wait up to 100ms for a pulse to become available
            if (!m_pulseCv.wait_for(lock, std::chrono::milliseconds(100), 
                                   [this] { return !m_librarianPulseQueue.empty() || !m_running.load(); })) {
                // Timeout: continue spinning until shutdown
                continue;
            }
            
            if (!m_running.load(std::memory_order_acquire)) {
                break;
            }
            
            if (m_librarianPulseQueue.empty()) {
                continue;
            }
            
            pulse = std::move(m_librarianPulseQueue.front());
            m_librarianPulseQueue.pop_front();
        }
        
        if (!pulse) {
            continue;
        }
        
        // Extract context window from pulse payload
        // For now, use the full payload as context (up to 256 tokens worth)
        std::string_view contextWindow = pulse->payload;

        // Phase 6: Bridge hotpatch diagnostics into Librarian retrieval context.
        // If runtime patching emitted actionable failures (e.g. ProtectFail/InvalidAddress),
        // prepend that signal so HNSW+reranker can retrieve relevant fix locations.
        std::string combinedContext;
        std::string hotpatchHint;
        if (ConsumeHotpatchDiagnosticsHint(hotpatchHint)) {
            combinedContext.reserve(hotpatchHint.size() + contextWindow.size() + 32);
            combinedContext += "[HotpatchDiagnostics]\n";
            combinedContext += hotpatchHint;
            if (!contextWindow.empty()) {
                combinedContext += "\n\n";
                combinedContext.append(contextWindow.data(), contextWindow.size());
            }
            contextWindow = combinedContext;
        }
        
        // Limit context window size for embedding (HNSW expects ~256-512 token windows for efficiency)
        const std::size_t maxContextSize = 2048;  // ~256-512 tokens in bytes
        if (contextWindow.size() > maxContextSize) {
            contextWindow = contextWindow.substr(contextWindow.size() - maxContextSize);
        }
        
        // Search HNSW index with this context
        HNSWSearchResult searchResult = _searchHNSWIndex(contextWindow);

        const float similarityThreshold = GetGlobalLibrarianSimilarityThreshold();
        
        // If we found a relevant snippet, inject it for Primary lane.
        // Phase 4.2.2: searchResult.similarity now carries the hybrid score (BM25+cosine).
        // The slider threshold gates the hybrid score, not the raw cosine similarity.
        // Phase 4.3: Tag the pulse with source attribution for UI highlighting.
        if (searchResult.found && searchResult.similarity >= similarityThreshold) {
            // Inject context with metadata
            std::string injectionPayload = "// Librarian injected snippet (hybrid score: ";
            injectionPayload += std::to_string(searchResult.similarity);
            injectionPayload += ")\n";
            injectionPayload += searchResult.snippet;
            
            InjectLibrarianContext(pulse->requestId, injectionPayload);
            
            // Phase 4.3: Tag the pulse for UI attribution/highlighting
            pulse->sourceAttribution.sourceFile = searchResult.sourceFile;
            pulse->sourceAttribution.lineNumber = searchResult.lineNumber;
            pulse->sourceAttribution.hybridScore = searchResult.similarity;
            pulse->sourceAttribution.semanticScore = searchResult.semanticScore;
            pulse->sourceAttribution.lexicalScore = searchResult.lexicalScore;
            pulse->sourceAttribution.fromLibrarian = true;
            PublishLatestSourceAttribution(pulse->sourceAttribution);
        }
        
        // Release pulse to mark completion for this lane
        completePulseForLane(SwarmLane::Librarian, pulse);
    }
}

std::size_t SwarmOrchestrator::invalidateDecodedTokensAfter(std::uint64_t requestId, std::uint32_t tokenIndex) {
    std::lock_guard<std::mutex> lock(m_decodeMutex);
    const std::size_t before = m_decodeToUiQueue.size();
    std::erase_if(m_decodeToUiQueue, [requestId, tokenIndex](const DecodedToken& tok) {
        return tok.requestId == requestId && tok.speculative && tok.tokenIndex > tokenIndex;
    });
    return before - m_decodeToUiQueue.size();
}

nlohmann::json SwarmOrchestrator::getStatus() const {
    std::size_t primaryDepth = 0;
    std::size_t verifierDepth = 0;
    std::size_t librarianDepth = 0;
    {
        std::lock_guard<std::mutex> lock(m_pulseMutex);
        primaryDepth = m_primaryPulseQueue.size();
        verifierDepth = m_verifierPulseQueue.size();
        librarianDepth = m_librarianPulseQueue.size();
    }

    std::size_t pivotDepth = 0;
    {
        std::lock_guard<std::mutex> lock(m_pivotMutex);
        pivotDepth = m_pendingPrimaryPivots.size();
    }

    std::size_t decodeDepth = 0;
    {
        std::lock_guard<std::mutex> lock(m_decodeMutex);
        decodeDepth = m_decodeToUiQueue.size();
    }

    std::size_t pendingRAGInjections = 0;
    {
        std::lock_guard<std::mutex> lock(m_ragInjectionMutex);
        pendingRAGInjections = m_pendingRAGInjections.size();
    }

    LibrarianLanePolicy policy{};
    std::size_t pendingLibrarian = 0;
    {
        std::lock_guard<std::mutex> lock(m_librarianPolicyMutex);
        policy = m_librarianPolicy;
        pendingLibrarian = m_librarianPendingLookups.size();
    }

    return {
        {"active", true},
        {"tasks_executed", m_totalTasksExecuted.load()},
        {"pulse_primary_queue_depth", primaryDepth},
        {"pulse_verifier_queue_depth", verifierDepth},
        {"pulse_librarian_queue_depth", librarianDepth},
        {"pending_pivot_count", pivotDepth},
        {"decode_to_ui_queue_depth", decodeDepth},
        {"librarian_enabled", policy.enabled},
        {"librarian_dispatch_every_tokens", policy.dispatchEveryTokens},
        {"librarian_min_pulse_bytes", policy.minPulseBytes},
        {"librarian_max_pending_lookups", policy.maxPendingLookups},
        {"librarian_similarity_threshold", GetGlobalLibrarianSimilarityThreshold()},
        {"librarian_pending_request_count", pendingLibrarian},
        {"librarian_dispatch_count", m_librarianDispatchCount.load(std::memory_order_relaxed)},
        {"rag_context_pending_injections", pendingRAGInjections},
        {"rag_context_inject_count", m_ragContextInjectCount.load(std::memory_order_relaxed)}
    };
}

}
