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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

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
       << "  Doc Length:    " << GetDocLength() << " bytes\n"
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
    GlobalFree(ctx);
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
// CopilotGapCloser — Unified coordinator
// ============================================================================

int CopilotGapCloser::Initialize() {
    m_initCount = 0;

    // Module 1: Vector Database
    if (m_vecDb.Initialize()) {
        m_initCount++;
    }

    // Module 2: Composer — always ready (stateless until BeginTransaction)
    m_initCount++;

    // Module 3: CRDT — needs explicit InitDocument, but mark as available
    m_initCount++;

    // Module 4: Git Context — always ready
    m_initCount++;

    return m_initCount;
}

void CopilotGapCloser::GetPerfCounters(GapCloserPerfCounter& vecDb,
                                         GapCloserPerfCounter& composer,
                                         GapCloserPerfCounter& crdt) const {
    GapCloserPerfCounter buf[3] = {};
    GapCloser_GetPerfCounters(buf);
    vecDb    = buf[0];
    composer = buf[1];
    crdt     = buf[2];
}

std::string CopilotGapCloser::GetStatusString() const {
    std::ostringstream ss;
    ss << "=== Copilot Gap Closer — Feature Parity Engine ===\n"
       << "Subsystems Ready: " << m_initCount << " / 4\n\n";

    ss << m_vecDb.GetStatusString()    << "\n";
    ss << m_composer.GetStatusString() << "\n";
    ss << m_crdt.GetStatusString()     << "\n";
    ss << m_gitCtx.GetStatusString()   << "\n";

    // Performance counters
    GapCloserPerfCounter perf[3] = {};
    GapCloser_GetPerfCounters(perf);

    ss << "Performance Counters:\n"
       << "  VecDb   — Calls: " << perf[0].calls
       << ", Cycles: " << perf[0].totalCycles << "\n"
       << "  Composer — Calls: " << perf[1].calls
       << ", Cycles: " << perf[1].totalCycles << "\n"
       << "  CRDT    — Calls: " << perf[2].calls
       << ", Cycles: " << perf[2].totalCycles << "\n";

    return ss.str();
}

} // namespace RawrXD
