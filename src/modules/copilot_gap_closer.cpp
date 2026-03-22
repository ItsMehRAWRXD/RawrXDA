// ============================================================================
// copilot_gap_closer.cpp — Implementation for CopilotGapCloser C++ wrappers
// ============================================================================
// Bridges the pure MASM64 kernels (RawrXD_CopilotGapCloser.asm) to the
// Win32IDE through clean C++ interfaces.
//
// Modules:
//   1. VectorDatabase    — HNSW approximate nearest neighbor
//   2. MultiFileComposer — Atomic transactional file edits
//   3. CrdtEngine        — Real-time collaborative editing
//   4. GitContextProvider — Repo-aware AI context assembly
// ============================================================================

#include "copilot_gap_closer.h"
#include <sstream>
#include <iomanip>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
    int32_t Task_SubmitRequest(const char* task, const void** files, int32_t count) { return 0; }
    int32_t Task_GetStatus(int32_t taskId) { return 0; }
    int32_t Task_Cancel(int32_t taskId) { return 0; }
}

// SCAFFOLD_259: Copilot gap closer module


namespace RawrXD {

// ============================================================================
// VectorDatabase implementation
// ============================================================================

std::string VectorDatabase::GetStatusString() const {
    std::ostringstream ss;
    ss << "HNSW Vector Database\n"
       << "  Initialized: " << (IsInitialized() ? "YES" : "NO") << "\n"
       << "  Node Count:  " << GetNodeCount() << " / " << VECDB_MAX_VECTORS << "\n"
       << "  Entry Point: " << GetEntryPoint() << "\n"
       << "  Max Level:   " << GetMaxLevel() << "\n"
       << "  Dimensions:  " << VECDB_DIMENSIONS << "\n"
       << "  M:           " << VECDB_M << "\n"
       << "  M_max0:      " << VECDB_M_MAX0 << "\n";
    return ss.str();
}

// ============================================================================
// MultiFileComposer implementation
// ============================================================================

std::string MultiFileComposer::GetStatusString() const {
    std::ostringstream ss;
    const char* stateNames[] = {
        "IDLE", "PENDING", "APPLYING", "COMMITTED", "ROLLBACK"
    };
    int st = GetState();
    ss << "Multi-file Composer\n"
       << "  State:              " << (st >= 0 && st <= 4 ? stateNames[st] : "UNKNOWN") << "\n"
       << "  Active Transaction: " << (HasActiveTransaction() ? "YES" : "NO") << "\n"
       << "  Total Commits:      " << GetTxCount() << "\n"
       << "  Max Files/Tx:       " << COMPOSER_MAX_FILES << "\n";
    return ss.str();
}

// ============================================================================
// CrdtEngine implementation
// ============================================================================

std::string CrdtEngine::GetStatusString() const {
    std::ostringstream ss;
    ss << "CRDT Collaboration Engine\n"
       << "  Initialized:  " << (IsInitialized() ? "YES" : "NO") << "\n"
       << "  Peer ID:      " << GetPeerId() << "\n"
       << "  Doc Length: " << GetDocLength() << " bytes\n"
       << "  Lamport:      " << GetLamport() << "\n"
       << "  Max Peers:    " << CRDT_MAX_PEERS << "\n"
       << "  Max Content:  " << CRDT_MAX_CONTENT << " per op\n";
    return ss.str();
}

// ============================================================================
// GitContextProvider implementation
// ============================================================================

std::string GitContextProvider::ExtractContext(const char* repoPath,
                                               const char* currentFile,
                                               int32_t lineNumber) {
    void* ctx = Git_ExtractContext(repoPath, currentFile, lineNumber);
    if (!ctx) return "(no context available)";

    std::string result(static_cast<const char*>(ctx));
#ifdef _WIN32
    GlobalFree(ctx);
#else
    free(ctx);
#endif
    return result;
}

std::string GitContextProvider::GetStatusString() const {
    std::ostringstream ss;
    ss << "Git Context Extractor\n"
       << "  Status: READY\n"
       << "  Max Diff Size:    " << (1048576 / 1024) << " KB\n"
       << "  Context Lines:    10\n"
       << "  Max Hunks:        512\n";
    return ss.str();
}

// ============================================================================
// TaskDispatcher implementation
// ============================================================================

int32_t TaskDispatcher::Submit(const std::string& task, const std::vector<std::string>& files) {
    // Current ASM implementation doesn't have Task_SubmitRequest, so we stub it for now
    // to allow the link to proceed.
    //
    // Return value semantics:
    //   - Future implementation: on success, this should return a positive task identifier
    //     that can be passed to GetStatus/Cancel; on failure, it should return 0 or a
    //     negative error code as defined by the final API contract.
    //   - Current stub: always returns 0, which means "no task was submitted / not supported
    //     yet". Callers must treat 0 as a failed or unimplemented submission.
    return 0;
}

int32_t TaskDispatcher::GetStatus(int32_t taskId) {
    // Stub implementation: task status queries are not yet supported and always
    // report status code 0. The concrete status-code semantics will be defined
    // when the underlying dispatcher is implemented.
    return 0;
}

bool TaskDispatcher::Cancel(int32_t taskId) {
    // Stub implementation: task cancellation is not yet supported and always
    // returns false to indicate that no cancellation was performed.
    return false;
}

int CopilotGapCloser::Initialize() {
    m_initCount = 0;
    if (m_vecDb.Initialize()) m_initCount++;
    // MultiFileComposer, CrdtEngine, and GitContextProvider are ready
    // when their handles are valid, but they don't have explicit global init calls
    // except for VecDb. We count them based on successful handle creation in use.
    return m_initCount;
}

void CopilotGapCloser::GetPerfCounters(GapCloserPerfCounter& vecDb,
                                       GapCloserPerfCounter& composer,
                                       GapCloserPerfCounter& crdt) const {
    GapCloserPerfCounter counters[3];
    GapCloser_GetPerfCounters(counters);
    vecDb = counters[0];
    composer = counters[1];
    crdt = counters[2];
}

std::string CopilotGapCloser::GetStatusString() const {
    std::ostringstream ss;
    ss << "=== RawrXD Copilot Gap Closer Status ===\n\n"
       << m_vecDb.GetStatusString() << "\n"
       << m_composer.GetStatusString() << "\n"
       << m_crdt.GetStatusString() << "\n"
       << m_gitCtx.GetStatusString() << "\n"
       << "Subsystems Active: " << m_initCount << " / 5\n";
    return ss.str();
}

} // namespace RawrXD
