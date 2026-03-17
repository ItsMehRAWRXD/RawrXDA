// ============================================================================
// copilot_gap_closer.h — C++ Bridge to RawrXD_CopilotGapCloser.asm
// ============================================================================
// Declares extern "C" ASM exports and C++ wrapper classes for the four
// Copilot feature-parity subsystems:
//
//   Module 1: HNSW Vector Database (semantic code search across 1M snippets)
//   Module 2: Multi-file Composer (atomic transactions across 256 files)
//   Module 3: CRDT Collaboration Engine (real-time 16-peer collaboration)
//   Module 4: Git Context Extractor (repo-wide AI context assembly)
//
// ASM Source:  src/asm/RawrXD_CopilotGapCloser.asm
// License:     Pro tier — gated behind FEATURE_COPILOT_PARITY
//
// Usage:
//   CopilotGapCloser gap;
//   gap.Initialize();
//   gap.GetVecDb().Insert(embedding, metadata);
//   auto results = gap.GetVecDb().Search(query, k);
// ============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace RawrXD {

// ============================================================================
// Constants — must match ASM definitions
// ============================================================================
constexpr int VECDB_DIMENSIONS      = 768;
constexpr int VECDB_MAX_VECTORS     = 1000000;
constexpr int VECDB_M               = 16;
constexpr int VECDB_M_MAX0          = 32;
constexpr int VECDB_MAX_LEVEL       = 16;
constexpr int VECDB_BYTES_PER_VEC   = VECDB_DIMENSIONS * 4;

constexpr int COMPOSER_MAX_FILES    = 256;
constexpr int COMPOSER_MAX_OPS      = 4096;
constexpr int COMPOSER_STATE_IDLE   = 0;
constexpr int COMPOSER_STATE_PENDING = 1;
constexpr int COMPOSER_STATE_APPLYING = 2;
constexpr int COMPOSER_STATE_COMMITTED = 3;
constexpr int COMPOSER_STATE_ROLLBACK = 4;
constexpr int COMPOSER_OP_CREATE    = 0;
constexpr int COMPOSER_OP_MODIFY    = 1;
constexpr int COMPOSER_OP_DELETE    = 2;
constexpr int COMPOSER_OP_RENAME    = 3;

constexpr int CRDT_MAX_PEERS        = 16;
constexpr int CRDT_MAX_DOC_SIZE     = 16 * 1024 * 1024;
constexpr int CRDT_MAX_CONTENT      = 256;

// ============================================================================
// PerfCounter — must match ASM struct layout
// ============================================================================
#pragma pack(push, 1)
struct GapCloserPerfCounter {
    uint64_t calls;
    uint64_t totalCycles;
    uint64_t lastCycles;
};
#pragma pack(pop)

static_assert(sizeof(GapCloserPerfCounter) == 24,
    "GapCloserPerfCounter must be 24 bytes to match ASM layout");

// ============================================================================
// ASM Extern Declarations
// ============================================================================
extern "C" {

    // ── Vector Database (HNSW) ──
    int32_t VecDb_Init();
    int32_t VecDb_Insert(const float* vector, void* metadata);
    int32_t VecDb_Search(const float* query, int32_t* results, int32_t k);
    int32_t VecDb_Delete(int32_t nodeId);
    int32_t VecDb_GetNodeCount();
    float   VecDb_L2Distance_AVX2(const float* a, const float* b);

    extern void*    g_VecDbNodes;
    extern int32_t  g_VecDbNumNodes;
    extern int32_t  g_VecDbEntryPoint;
    extern int32_t  g_VecDbMaxLevel;
    extern GapCloserPerfCounter g_VecDbPerf;

    // ── Multi-file Composer ──
    void*   Composer_BeginTransaction();
    int32_t Composer_AddFileOp(void* tx, const char* path, int32_t opType,
                               const void* content, uint64_t contentLen);
    int32_t Composer_Commit(void* tx);
    int32_t Composer_GetState();
    int64_t Composer_GetTxCount();

    extern int32_t  g_ComposerState;
    extern GapCloserPerfCounter g_ComposerPerf;

    // ── CRDT Collaboration ──
    void*   Crdt_InitDocument(const void* uuid, int32_t peerId);
    int64_t Crdt_InsertText(void* doc, uint64_t position,
                            const void* text, int32_t length);
    int64_t Crdt_DeleteText(void* doc, uint64_t position, int32_t length);
    int32_t Crdt_MergeRemoteOp(void* doc, const void* remoteOp);
    int64_t Crdt_GetDocLength(void* doc);
    int64_t Crdt_GetLamport(void* doc);

    // ── Task Ingestion & Dispatcher ──
    // Stubbed in copilot_gap_closer.cpp because implementaiton is in monolithic/tasks.asm 
    // which uses different naming conventions.
    // int32_t Task_SubmitRequest(const char* taskDescription, const void** attachments, int32_t attachmentCount);
    // int32_t Task_GetStatus(int32_t taskId);
    // int32_t Task_Cancel(int32_t taskId);

    extern void*    g_CrdtLocalDoc;
    extern int32_t  g_CrdtPeerCount;
    extern GapCloserPerfCounter g_CrdtPerf;

    // ── Git Context ──
    void*   Git_ExtractContext(const char* repoPath, const char* currentFile,
                               int32_t lineNumber);
    void    Git_SetBranch(const char* branch);
    void    Git_SetCommitHash(const char* hash);

    // ── Performance ──
    void    GapCloser_GetPerfCounters(GapCloserPerfCounter* out3);
}

// ============================================================================
// VectorDatabase — C++ wrapper for HNSW vector index
// ============================================================================
class VectorDatabase {
public:
    VectorDatabase()  = default;
    ~VectorDatabase() = default;

    VectorDatabase(const VectorDatabase&) = delete;
    VectorDatabase& operator=(const VectorDatabase&) = delete;

    /// Initialize the HNSW index (allocates ~3GB for 1M vectors).
    bool Initialize() {
        return VecDb_Init() == 0;
    }

    /// Insert a 768-dim embedding with associated metadata.
    /// Returns node ID, or -1 on failure.
    int32_t Insert(const float* embedding, void* metadata = nullptr) {
        return VecDb_Insert(embedding, metadata);
    }

    /// Search for K approximate nearest neighbors.
    /// Returns number of results found.
    int32_t Search(const float* query, std::vector<int32_t>& outIds, int32_t k = 10) {
        outIds.resize(k);
        int32_t found = VecDb_Search(query, outIds.data(), k);
        outIds.resize(found);
        return found;
    }

    /// Mark a node as deleted (tombstone).
    bool Delete(int32_t nodeId) {
        return VecDb_Delete(nodeId) == 0;
    }

    /// Get current node count.
    int32_t GetNodeCount() const {
        return VecDb_GetNodeCount();
    }

    /// Get max HNSW level.
    int32_t GetMaxLevel() const {
        return g_VecDbMaxLevel;
    }

    /// Get entry point node ID.
    int32_t GetEntryPoint() const {
        return g_VecDbEntryPoint;
    }

    /// Compute L2 distance between two 768-dim vectors.
    float L2Distance(const float* a, const float* b) const {
        return VecDb_L2Distance_AVX2(a, b);
    }

    /// Check if index is initialized.
    bool IsInitialized() const {
        return g_VecDbNodes != nullptr;
    }

    /// Get status string.
    std::string GetStatusString() const;
};

// ============================================================================
// MultiFileComposer — C++ wrapper for transactional file edits
// ============================================================================
class MultiFileComposer {
public:
    MultiFileComposer()  = default;
    ~MultiFileComposer() = default;

    MultiFileComposer(const MultiFileComposer&) = delete;
    MultiFileComposer& operator=(const MultiFileComposer&) = delete;

    /// Begin a new multi-file transaction.
    /// Returns false if a transaction is already in progress.
    bool BeginTransaction() {
        m_txHandle = Composer_BeginTransaction();
        return m_txHandle != nullptr;
    }

    /// Queue a file creation.
    bool CreateFile(const char* path, const void* content, uint64_t len) {
        if (!m_txHandle) return false;
        return Composer_AddFileOp(m_txHandle, path, COMPOSER_OP_CREATE,
                                  content, len) == 1;
    }

    /// Queue a file modification.
    bool ModifyFile(const char* path, const void* content, uint64_t len) {
        if (!m_txHandle) return false;
        return Composer_AddFileOp(m_txHandle, path, COMPOSER_OP_MODIFY,
                                  content, len) == 1;
    }

    /// Queue a file deletion.
    bool DeleteFile(const char* path) {
        if (!m_txHandle) return false;
        return Composer_AddFileOp(m_txHandle, path, COMPOSER_OP_DELETE,
                                  nullptr, 0) == 1;
    }

    /// Commit all queued operations atomically.
    /// Returns true on success, false if rolled back.
    bool Commit() {
        if (!m_txHandle) return false;
        int32_t result = Composer_Commit(m_txHandle);
        m_txHandle = nullptr;
        return result == 0;
    }

    /// Get current composer state.
    int32_t GetState() const {
        return Composer_GetState();
    }

    /// Get total lifetime transaction count.
    int64_t GetTxCount() const {
        return Composer_GetTxCount();
    }

    /// Check if a transaction is in progress.
    bool HasActiveTransaction() const {
        return m_txHandle != nullptr;
    }

    /// Get status string.
    std::string GetStatusString() const;

private:
    void* m_txHandle = nullptr;
};

// ============================================================================
// CrdtEngine — C++ wrapper for CRDT collaboration
// ============================================================================
class CrdtEngine {
public:
    CrdtEngine()  = default;
    ~CrdtEngine() = default;

    CrdtEngine(const CrdtEngine&) = delete;
    CrdtEngine& operator=(const CrdtEngine&) = delete;

    /// Initialize a new CRDT document.
    /// UUID must be 16 bytes. PeerId is 0..15.
    bool InitDocument(const uint8_t uuid[16], int32_t peerId) {
        m_docHandle = Crdt_InitDocument(uuid, peerId);
        m_peerId    = peerId;
        return m_docHandle != nullptr;
    }

    /// Insert text at a logical position.
    /// Returns Lamport timestamp of the operation.
    int64_t InsertText(uint64_t position, const char* text, int32_t length) {
        if (!m_docHandle) return -1;
        return Crdt_InsertText(m_docHandle, position, text, length);
    }

    /// Delete text at a logical position.
    int64_t DeleteText(uint64_t position, int32_t length) {
        if (!m_docHandle) return -1;
        return Crdt_DeleteText(m_docHandle, position, length);
    }

    /// Merge a remote operation (causal ordering enforced).
    bool MergeRemoteOp(const void* remoteOp) {
        if (!m_docHandle) return false;
        return Crdt_MergeRemoteOp(m_docHandle, remoteOp) == 0;
    }

    /// Get current document length.
    int64_t GetDocLength() const {
        if (!m_docHandle) return 0;
        return Crdt_GetDocLength(m_docHandle);
    }

    /// Get current Lamport timestamp.
    int64_t GetLamport() const {
        if (!m_docHandle) return 0;
        return Crdt_GetLamport(m_docHandle);
    }

    /// Check if document is initialized.
    bool IsInitialized() const {
        return m_docHandle != nullptr;
    }

    /// Get local peer ID.
    int32_t GetPeerId() const { return m_peerId; }

    /// Get status string.
    std::string GetStatusString() const;

private:
    void*   m_docHandle = nullptr;
    int32_t m_peerId    = -1;
};

// ============================================================================
// GitContextProvider — C++ wrapper for Git context extraction
// ============================================================================
class GitContextProvider {
public:
    GitContextProvider()  = default;
    ~GitContextProvider() = default;

    GitContextProvider(const GitContextProvider&) = delete;
    GitContextProvider& operator=(const GitContextProvider&) = delete;

    /// Set the current branch name.
    void SetBranch(const char* branch) {
        Git_SetBranch(branch);
    }

    /// Set the current commit hash (40 hex chars).
    void SetCommitHash(const char* hash) {
        Git_SetCommitHash(hash);
    }

    /// Extract AI context string for the given repo/file/line.
    /// Caller must free the returned string via GlobalFree.
    std::string ExtractContext(const char* repoPath,
                               const char* currentFile = nullptr,
                               int32_t lineNumber = 0);

    /// Get status string.
    std::string GetStatusString() const;
};

extern "C" {
    int32_t Task_SubmitRequest(const char* task, const void** files, int32_t count);
    int32_t Task_GetStatus(int32_t taskId);
    int32_t Task_Cancel(int32_t taskId);
}

// ============================================================================
// TaskDispatcher — C++ wrapper for general AI task ingestion
// ============================================================================
class TaskDispatcher {
public:
    TaskDispatcher()  = default;
    ~TaskDispatcher() = default;

    /// Submit a generalized task (e.g. "audit project", "refactor code", "explain this")
    /// Returns a unique Task ID (> 0) on success, or 0 on failure.
    int32_t Submit(const std::string& task, const std::vector<std::string>& files = {});

    /// Get task status code (Ready, Processing, Failed, Completed).
    int32_t GetStatus(int32_t taskId);

    /// Cancel a pending or running task.
    bool Cancel(int32_t taskId);
};

// ============================================================================
// CopilotGapCloser — Unified coordinator for all 5 subsystems
// ============================================================================
class CopilotGapCloser {
public:
    CopilotGapCloser()  = default;
    ~CopilotGapCloser() = default;

    CopilotGapCloser(const CopilotGapCloser&) = delete;
    CopilotGapCloser& operator=(const CopilotGapCloser&) = delete;

    /// Initialize all subsystems.
    /// Returns number of subsystems successfully initialized (0..5).
    int Initialize();

    /// Access individual subsystems.
    VectorDatabase&    GetVecDb()    { return m_vecDb; }
    MultiFileComposer& GetComposer() { return m_composer; }
    CrdtEngine&        GetCrdt()     { return m_crdt; }
    GitContextProvider& GetGitCtx()  { return m_gitCtx; }
    TaskDispatcher&    GetDispatcher() { return m_dispatcher; }

    /// Get performance counters for measured subsystems.
    void GetPerfCounters(GapCloserPerfCounter& vecDb,
                         GapCloserPerfCounter& composer,
                         GapCloserPerfCounter& crdt) const;

    /// Check overall readiness.
    bool IsReady() const { return m_initCount > 0; }

    /// Get number of successfully initialized subsystems.
    int GetInitCount() const { return m_initCount; }

    /// Get comprehensive status string.
    std::string GetStatusString() const;

private:
    VectorDatabase     m_vecDb;
    MultiFileComposer  m_composer;
    CrdtEngine         m_crdt;
    GitContextProvider m_gitCtx;
    TaskDispatcher     m_dispatcher;
    int                m_initCount = 0;
};

} // namespace RawrXD
