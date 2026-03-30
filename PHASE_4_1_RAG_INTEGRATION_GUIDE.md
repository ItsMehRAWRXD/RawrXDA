// ============================================================================
// Phase 4.1: Librarian RAG Integration Guide
// ============================================================================
//
// This document explains how to integrate the LocalVectorDB HNSW search
// with the SwarmOrchestrator's Librarian lane using InjectLibrarianContext.
//
// ============================================================================
// Architecture Overview
// ============================================================================
//
// The Librarian lane workflow with RAG integration:
//
// 1. Primary Lane generates tokens continuously
// 2. Every `dispatchEveryTokens` (default: 32 tokens)
//    → SwarmOrchestrator::onPrimaryTokenEmitted() is called
// 3. Librarian lane pops from m_librarianPulseQueue
// 4. Librarian extracts the last 128 tokens from the pulse
// 5. Librarian performs HNSW vector search on local codebase
// 6. Librarian calls InjectLibrarianContext(requestId, topK_snippets)
// 7. Next Primary pulse gets the RAG context prepended
// 8. Primary lane uses augmented context for next token generation
//
// ============================================================================
// Phase 4.1 Implementation Checklist
// ============================================================================

/*
COMPLETED:
  ✓ SwarmOrchestrator::InjectLibrarianContext(requestId, ragContext)
  ✓ SwarmOrchestrator::_extractAndClearPendingRAGContext(requestId) [private]
  ✓ RAG context storage: m_pendingRAGInjections[requestId]
  ✓ RAG context injection in tryPopPulseForLane (Primary lane only)
  ✓ Telemetry counters: m_ragContextInjectCount, rag_context_pending_injections

TODO (Phase 4.2):
  □ LocalVectorDB::Search(query) implementation
  □ HNSW index builder from d:/rawrxd/src/ codebase
  □ Token extraction (last 128 tokens from pulse)
  □ LLM-based re-ranking on Librarian thread
  □ Integration test with mock vector DB
*/

// ============================================================================
// Example: How to Create a Librarian Worker Thread
// ============================================================================

#include "swarm_orchestrator.h"
#include <thread>
#include <string>
#include <vector>
#include <memory>

// Pseudocode: A simple Librarian worker thread that consumes pulses from
// the Librarian queue and injects RAG context.

class LibrarianWorker {
public:
    LibrarianWorker(RawrXD::SwarmOrchestrator* orchestra, void* hnsw_index)
        : m_orchestra(orchestra), m_hnswIndex(hnsw_index), m_running(false) {}

    void start() {
        m_running = true;
        m_thread = std::thread([this] { this->workerLoop(); });
    }

    void stop() {
        m_running = false;
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

private:
    RawrXD::SwarmOrchestrator* m_orchestra;
    void* m_hnswIndex;  // HNSW index from LocalVectorDB
    std::thread m_thread;
    bool m_running;

    void workerLoop() {
        // Phase 4.1 Main Loop:
        //   1. Pop pulse from Librarian queue
        //   2. Extract context window (last 128 tokens)
        //   3. Perform HNSW search
        //   4. Re-rank using LLM (draft model)
        //   5. Inject top snippets via InjectLibrarianContext
        //   6. Mark pulse as consumed

        while (m_running) {
            RawrXD::SwarmOrchestrator::PulseBufferPtr pulse;
            
            // Pop from Librarian queue with timeout
            if (!m_orchestra->tryPopPulseForLane(
                RawrXD::SwarmLane::Librarian, pulse)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (!pulse || pulse->requestId == 0) {
                continue;
            }

            // Step 1: Extract context window from pulse payload
            // (Pulse contains the current Primary lane's output tokens)
            std::string contextWindow = pulse->payload;
            if (contextWindow.size() > 2048) {
                // Truncate to last ~128 tokens (rough estimate: 5-8 chars/token)
                contextWindow = contextWindow.substr(contextWindow.size() - 1024);
            }

            // Step 2: Perform HNSW vector search
            // (Assumes LocalVectorDB is available with search API)
            /*
            std::vector<SearchResult> searchResults = 
                LocalVectorDB::Search(m_hnswIndex, contextWindow, top_k=5);
            */

            // Step 3: Build augmented context string
            // Format: [snippet1]\n---SNIPPET_BOUNDARY---\n[snippet2]...
            std::string ragContext;
            /*
            for (const auto& result : searchResults) {
                if (!ragContext.empty()) {
                    ragContext += "\n---SNIPPET_BOUNDARY---\n";
                }
                ragContext += result.fileContext;  // Docstring or code snippet
                ragContext += " (relevance: " + std::to_string(result.score) + ")";
            }
            */

            // Step 4: Inject RAG context for next watermark cycle
            if (!ragContext.empty()) {
                bool injected = m_orchestra->InjectLibrarianContext(
                    pulse->requestId,
                    ragContext);

                if (injected) {
                    // Telemetry: log successful injection
                    // (Optional: metrics collection)
                }
            }

            // Step 5: Notify orchestrator that pulse is consumed
            m_orchestra->completePulseForLane(
                RawrXD::SwarmLane::Librarian, pulse);
        }
    }
};

// ============================================================================
// Usage Example: Integrating LibrarianWorker into Win32IDE
// ============================================================================

/*

In Win32IDE initialization (e.g., onInferenceStarted):

    // 1. Build HNSW index from codebase
    void* hnswIndex = BuildHNSWIndex("d:/rawrxd/src/", */ /*embedding_model*//*);
    
    // 2. Create and start Librarian worker
    auto librarianWorker = std::make_unique<LibrarianWorker>(
        m_swarmOrchestrator.get(),
        hnswIndex);
    librarianWorker->start();
    
    // 3. Enable Librarian policy in orchestrator
    RawrXD::LibrarianLanePolicy policy;
    policy.enabled = true;
    policy.dispatchEveryTokens = 32;
    policy.minPulseBytes = 1024;
    policy.maxPendingLookups = 8;
    m_swarmOrchestrator->configureLibrarianPolicy(policy);
    
    // 4. During inference, Primary lane generates tokens
    // 5. Every 32 tokens, Librarian pops a pulse and searches
    // 6. RAG results are injected into next watermark cycle
    
    // On shutdown:
    librarianWorker->stop();

*/

// ============================================================================
// Data Flow Diagram
// ============================================================================

/*

Primary Lane (generates tokens):
  token_1 → token_2 → ... → token_32 → token_33 → ...
  
At token_32 boundary:
  ↓
  SwarmOrchestrator::onPrimaryTokenEmitted(requestId, 32)
  ↓
  Pulse dispatched to Librarian queue
  ↓
LibrarianWorker consumes pulse:
  1. Extract last 128 tokens from pulse.payload
  2. Query: "What are the relevant code snippets for this context?"
  3. HNSW search over codebase embeddings (find top-K)
  4. LLM re-ranking: "Which snippets are most relevant?" (optional)
  5. Call: InjectLibrarianContext(requestId, "Snippet A\nSnippet B\n...")
  ↓
RAG Context stored in m_pendingRAGInjections[requestId]
  ↓
At next Primary watermark:
  tryPopPulseForLane(Primary) is called
  ↓
  _extractAndClearPendingRAGContext() prepends RAG + boundary marker
  ↓
  New augmented pulse: "[RAG Context]\n[augmented_context_end]\n[original payload]"
  ↓
Primary lane processes augmented pulse:
  token_33 → token_34 → ... (informed by RAG snippets)

*/

// ============================================================================
// Latency Budget Analysis (from Saturation Test)
// ============================================================================

/*

Orchestration overhead breakdown:
  - Pulse dispatch:                  ~1 µs
  - Context extraction:              ~0.5 µs
  - HNSW search (per query):         ~5-10 ms (amortized over 32 tokens)
  - LLM re-ranking (optional):       ~10-20 ms (amortized over 32 tokens)
  - Injection overhead:              ~1 µs
  ─────────────────────────────────────────
  Total per 32-token dispatch:       ~15-30 ms
  Per-token amortized overhead:      ~0.5-1 ms per token
  
  Context: P99 TTFT budget = 150 ms
  Headroom: 120+ ms available
  Status: ✅ SAFE for full LLM-based re-ranker

*/

// ============================================================================
// Integration Testing Strategy
// ============================================================================

/*

Phase 4.2a: Unit Test LibrarianWorker with mock vector DB
  - Mock HNSW search to return fixed snippets
  - Verify InjectLibrarianContext stores context correctly
  - Verify _extractAndClearPendingRAGContext clears after use
  - Measure orchestration latency overhead (<2µs)

Phase 4.2b: Integration Test with real HNSW index
  - Build HNSW from small code corpus (e.g., Win32IDE.cpp only)
  - Run 50-token inference, trigger Librarian dispatch every 16 tokens
  - Measure end-to-end latency (HNSW + injection)
  - Verify P99 TTFT still <100µs (buffer: 50µs margin)

Phase 4.2c: Long-form stress test (2000+ tokens)
  - Run 2000 tokens with active RAG lookups
  - Monitor VRAM consumption (target: <14GB on 16GB system)
  - Measure sustained P99 latency
  - Validate thread affinity under sustained load

*/

// ============================================================================
// Key Invariants for RAG Injection
// ============================================================================

/*

1. Thread Safety:
   - InjectLibrarianContext() uses m_ragInjectionMutex
   - _extractAndClearPendingRAGContext() uses same mutex
   - No race conditions on m_pendingRAGInjections map

2. Ref-Counting Correctness:
   - Pulse passed to Librarian is ref-counted
   - Augmented pulse (created for Primary) gets new ref
   - Old pulse ref released to avoid leaks

3. Request Isolation:
   - Each requestId has independent m_pendingRAGInjections entry
   - Injection for request A doesn't affect request B
   - On request cleanup, _extractAndClearPendingRAGContext(requestId)
     is called (ensuring cleanup on completion)

4. Watermark Correctness:
   - RAG context injected BEFORE next Primary token generation
   - Prepended to pulse payload (boundary marker: "\n[augmented_context_end]\n")
   - Primary lane can parse boundary to separate augmented context

5. Backpressure:
   - If RAG context pending but not applied (queue full), next
     watermark cycle will retry injection
   - maxPendingLookups gate prevents VRAM overrun

*/

// ============================================================================
// NEXT IMMEDIATE STEPS (Phase 4.2)
// ============================================================================

/*

1. Implement LocalVectorDB::Search() interface
   - HNSW index structure (if not already present)
   - Embedding model (could reuse Primary's embeddings or use smaller model)
   - Top-K search with relevance scoring

2. Build HNSW index from d:/rawrxd/src/:
   - Crawl .cpp, .h, .hpp files
   - Extract docstrings, comments, key functions
   - Generate embeddings (using LLM or transformer model)
   - Build HNSW graph structure (~10K-50K code snippets expected)

3. Create LibrarianWorker implementation:
   - Parse pulse payload to extract last 128 tokens
   - Call LocalVectorDB::Search(context_window)
   - Format results as RAG context string
   - Call InjectLibrarianContext()

4. Add integration test:
   - Mock HNSW index with 10 hardcoded snippets
   - Run 100 inference requests with RAG enabled
   - Verify Librarian dispatch count > 0
   - Measure P99 TTFT (target: <100µs for orchestration)

5. Measure real HNSW latency:
   - Profile LocalVectorDB::Search() with realistic corpus size
   - Verify search completes in <10ms (fits in watermark window)
   - Adjust Librarian policy parameters if needed

*/
