// Win32IDE_CopilotGapPanel.cpp — Phase 49: Copilot Gap Closer UI Integration
// Wires the four CopilotGapCloser subsystems into the Win32IDE
// command palette, menu bar, and output panel.
// Handles all IDM_GAPCLOSE_* commands (10800–10899).
//
// Modules:
//   1. HNSW Vector Database — semantic code search
//   2. Multi-file Composer — atomic file transactions
//   3. CRDT Engine — real-time collaboration
//   4. Git Context Extractor — AI context assembly
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "Win32IDE.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <random>
#include <chrono>

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initCopilotGap() {
    if (m_copilotGapInitialized) return;

    m_copilotGap = std::make_unique<RawrXD::CopilotGapCloser>();
    int ready = m_copilotGap->Initialize();

    if (ready == 0) {
        appendToOutput("[GapCloser] ERROR: Failed to initialize any subsystem.\n");
        return;
    }

    m_copilotGapInitialized = true;

    std::ostringstream os;
    os << "[GapCloser] Phase 49: Copilot Gap Closer initialized.\n"
       << "[GapCloser] " << ready << "/4 subsystems online:\n"
       << "[GapCloser]   1. HNSW Vector Database (768-dim, 1M capacity)\n"
       << "[GapCloser]   2. Multi-file Composer (256 files, atomic tx)\n"
       << "[GapCloser]   3. CRDT Engine (16 peers, vector clocks)\n"
       << "[GapCloser]   4. Git Context Extractor (repo-aware AI)\n";
    appendToOutput(os.str());
}

// ============================================================================
// Command Router
// ============================================================================

void Win32IDE::handleCopilotGapCommand(int commandId) {
    if (!m_copilotGapInitialized) {
        initCopilotGap();
    }

    switch (commandId) {
        // ── General ──
        case IDM_GAPCLOSE_INIT:                 cmdGapInit();                break;
        case IDM_GAPCLOSE_STATUS:               cmdGapStatus();              break;
        case IDM_GAPCLOSE_PERF:                 cmdGapPerf();                break;
        case IDM_GAPCLOSE_HELP:                 cmdGapHelp();                break;

        // ── Vector Database ──
        case IDM_GAPCLOSE_VECDB_INIT:           cmdGapVecDbInit();           break;
        case IDM_GAPCLOSE_VECDB_INSERT:         cmdGapVecDbInsert();         break;
        case IDM_GAPCLOSE_VECDB_SEARCH:         cmdGapVecDbSearch();         break;
        case IDM_GAPCLOSE_VECDB_DELETE:         cmdGapVecDbDelete();         break;
        case IDM_GAPCLOSE_VECDB_STATUS:         cmdGapVecDbStatus();         break;
        case IDM_GAPCLOSE_VECDB_BENCH:          cmdGapVecDbBench();          break;

        // ── Composer ──
        case IDM_GAPCLOSE_COMPOSER_BEGIN:       cmdGapComposerBegin();       break;
        case IDM_GAPCLOSE_COMPOSER_ADD:         cmdGapComposerAdd();         break;
        case IDM_GAPCLOSE_COMPOSER_COMMIT:      cmdGapComposerCommit();      break;
        case IDM_GAPCLOSE_COMPOSER_STATUS:      cmdGapComposerStatus();      break;

        // ── CRDT ──
        case IDM_GAPCLOSE_CRDT_INIT:            cmdGapCrdtInit();            break;
        case IDM_GAPCLOSE_CRDT_INSERT:          cmdGapCrdtInsert();          break;
        case IDM_GAPCLOSE_CRDT_DELETE:          cmdGapCrdtDelete();          break;
        case IDM_GAPCLOSE_CRDT_STATUS:          cmdGapCrdtStatus();          break;

        // ── Git Context ──
        case IDM_GAPCLOSE_GIT_CONTEXT:          cmdGapGitContext();          break;
        case IDM_GAPCLOSE_GIT_BRANCH:           cmdGapGitBranch();           break;

        default:
            appendToOutput("[GapCloser] Unknown command: " +
                           std::to_string(commandId) + "\n");
            break;
    }
}

// ============================================================================
// Menu Creation
// ============================================================================

void Win32IDE::createCopilotGapMenu(HMENU parentMenu) {
    HMENU hGap = CreatePopupMenu();

    // ── Vector DB submenu ──
    HMENU hVecDb = CreatePopupMenu();
    AppendMenuA(hVecDb, MF_STRING, IDM_GAPCLOSE_VECDB_INIT,   "&Initialize Index");
    AppendMenuA(hVecDb, MF_STRING, IDM_GAPCLOSE_VECDB_INSERT,  "Insert &Embedding...");
    AppendMenuA(hVecDb, MF_STRING, IDM_GAPCLOSE_VECDB_SEARCH,  "&Search Nearest...");
    AppendMenuA(hVecDb, MF_STRING, IDM_GAPCLOSE_VECDB_DELETE,  "&Delete Node...");
    AppendMenuA(hVecDb, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hVecDb, MF_STRING, IDM_GAPCLOSE_VECDB_STATUS,  "S&tatus");
    AppendMenuA(hVecDb, MF_STRING, IDM_GAPCLOSE_VECDB_BENCH,   "&Benchmark");

    // ── Composer submenu ──
    HMENU hComposer = CreatePopupMenu();
    AppendMenuA(hComposer, MF_STRING, IDM_GAPCLOSE_COMPOSER_BEGIN,  "&Begin Transaction");
    AppendMenuA(hComposer, MF_STRING, IDM_GAPCLOSE_COMPOSER_ADD,    "&Add File Op...");
    AppendMenuA(hComposer, MF_STRING, IDM_GAPCLOSE_COMPOSER_COMMIT, "&Commit");
    AppendMenuA(hComposer, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hComposer, MF_STRING, IDM_GAPCLOSE_COMPOSER_STATUS, "&Status");

    // ── CRDT submenu ──
    HMENU hCrdt = CreatePopupMenu();
    AppendMenuA(hCrdt, MF_STRING, IDM_GAPCLOSE_CRDT_INIT,    "&Init Document");
    AppendMenuA(hCrdt, MF_STRING, IDM_GAPCLOSE_CRDT_INSERT,  "Insert &Text...");
    AppendMenuA(hCrdt, MF_STRING, IDM_GAPCLOSE_CRDT_DELETE,  "&Delete Text...");
    AppendMenuA(hCrdt, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hCrdt, MF_STRING, IDM_GAPCLOSE_CRDT_STATUS,  "&Status");

    // ── Git submenu ──
    HMENU hGit = CreatePopupMenu();
    AppendMenuA(hGit, MF_STRING, IDM_GAPCLOSE_GIT_CONTEXT,   "Extract &Context");
    AppendMenuA(hGit, MF_STRING, IDM_GAPCLOSE_GIT_BRANCH,    "Set &Branch...");

    // ── Main menu ──
    AppendMenuA(hGap, MF_STRING, IDM_GAPCLOSE_INIT,    "&Initialize All");
    AppendMenuA(hGap, MF_STRING, IDM_GAPCLOSE_STATUS,  "Overall &Status");
    AppendMenuA(hGap, MF_STRING, IDM_GAPCLOSE_PERF,    "&Performance Counters");
    AppendMenuA(hGap, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hGap, MF_POPUP, (UINT_PTR)hVecDb,    "Vector &Database");
    AppendMenuA(hGap, MF_POPUP, (UINT_PTR)hComposer, "Multi-file &Composer");
    AppendMenuA(hGap, MF_POPUP, (UINT_PTR)hCrdt,     "C&RDT Engine");
    AppendMenuA(hGap, MF_POPUP, (UINT_PTR)hGit,      "&Git Context");
    AppendMenuA(hGap, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hGap, MF_STRING, IDM_GAPCLOSE_HELP,   "&Help");

    AppendMenuA(parentMenu, MF_POPUP, (UINT_PTR)hGap, "&Gap Closer");
}

// ============================================================================
// General Command Handlers
// ============================================================================

void Win32IDE::cmdGapInit() {
    if (m_copilotGapInitialized) {
        appendToOutput("[GapCloser] Already initialized.\n");
        return;
    }
    initCopilotGap();
}

void Win32IDE::cmdGapStatus() {
    if (!m_copilotGap) {
        appendToOutput("[GapCloser] Not initialized. Use Initialize All first.\n");
        return;
    }

    appendToOutput("\n");
    appendToOutput("╔══════════════════════════════════════════════════════════════════╗\n");
    appendToOutput("║       PHASE 49: COPILOT GAP CLOSER — STATUS                    ║\n");
    appendToOutput("╚══════════════════════════════════════════════════════════════════╝\n");
    appendToOutput(m_copilotGap->GetStatusString());
    appendToOutput("\n");
}

void Win32IDE::cmdGapPerf() {
    if (!m_copilotGap) {
        appendToOutput("[GapCloser] Not initialized.\n");
        return;
    }

    RawrXD::GapCloserPerfCounter vecDb{}, composer{}, crdt{};
    m_copilotGap->GetPerfCounters(vecDb, composer, crdt);

    std::ostringstream os;
    os << "\n[GapCloser] Performance Counters:\n"
       << "  VecDb    — Calls: " << std::setw(10) << vecDb.calls
       << "  Total Cycles: " << std::setw(15) << vecDb.totalCycles << "\n"
       << "  Composer — Calls: " << std::setw(10) << composer.calls
       << "  Total Cycles: " << std::setw(15) << composer.totalCycles << "\n"
       << "  CRDT     — Calls: " << std::setw(10) << crdt.calls
       << "  Total Cycles: " << std::setw(15) << crdt.totalCycles << "\n";
    appendToOutput(os.str());
}

void Win32IDE::cmdGapHelp() {
    appendToOutput("\n");
    appendToOutput("═══════════════════════════════════════════════════════════════════\n");
    appendToOutput("  Copilot Gap Closer — Phase 49 Help\n");
    appendToOutput("═══════════════════════════════════════════════════════════════════\n");
    appendToOutput("  Four subsystems closing the 60%% gap to full Copilot parity:\n\n");
    appendToOutput("  1. HNSW Vector Database\n");
    appendToOutput("     Pure MASM64 HNSW index with AVX2 L2 distance kernel.\n");
    appendToOutput("     768-dim BERT embeddings, 1M vector capacity.\n");
    appendToOutput("     ABA-safe lock-free allocation via CMPXCHG8B.\n\n");
    appendToOutput("  2. Multi-file Composer\n");
    appendToOutput("     Atomic transactional edits across up to 256 files.\n");
    appendToOutput("     Write-ahead journal, auto-rollback on failure.\n");
    appendToOutput("     CAS-guarded state transitions.\n\n");
    appendToOutput("  3. CRDT Collaboration Engine\n");
    appendToOutput("     RGA-style operation-based CRDTs for 16 peers.\n");
    appendToOutput("     Vector clocks for causal ordering.\n");
    appendToOutput("     Ring-buffer operation log.\n\n");
    appendToOutput("  4. Git Context Extractor\n");
    appendToOutput("     Builds repo-aware context strings for AI.\n");
    appendToOutput("     Branch, commit hash, diff integration.\n\n");
    appendToOutput("═══════════════════════════════════════════════════════════════════\n");
}

// ============================================================================
// Vector Database Command Handlers
// ============================================================================

void Win32IDE::cmdGapVecDbInit() {
    if (!m_copilotGap) { initCopilotGap(); }

    appendToOutput("[GapCloser] Initializing HNSW Vector Database...\n");
    if (m_copilotGap->GetVecDb().IsInitialized()) {
        appendToOutput("[GapCloser] VecDb already initialized.\n");
        return;
    }

    if (m_copilotGap->GetVecDb().Initialize()) {
        appendToOutput("[GapCloser] VecDb initialized: 768-dim, 1M capacity, AVX2 distance.\n");
    } else {
        appendToOutput("[GapCloser] VecDb initialization FAILED (allocation error).\n");
    }
}

void Win32IDE::cmdGapVecDbInsert() {
    if (!m_copilotGap || !m_copilotGap->GetVecDb().IsInitialized()) {
        appendToOutput("[GapCloser] VecDb not initialized.\n");
        return;
    }

    // Insert a test vector (random 768-dim)
    std::mt19937 rng(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::vector<float> vec(768);
    for (auto& v : vec) v = dist(rng);

    int32_t id = m_copilotGap->GetVecDb().Insert(vec.data());
    if (id >= 0) {
        std::ostringstream os;
        os << "[GapCloser] Inserted test vector, node ID: " << id
           << " (total: " << m_copilotGap->GetVecDb().GetNodeCount() << ")\n";
        appendToOutput(os.str());
    } else {
        appendToOutput("[GapCloser] VecDb insert FAILED (index full).\n");
    }
}

void Win32IDE::cmdGapVecDbSearch() {
    if (!m_copilotGap || !m_copilotGap->GetVecDb().IsInitialized()) {
        appendToOutput("[GapCloser] VecDb not initialized.\n");
        return;
    }

    if (m_copilotGap->GetVecDb().GetNodeCount() == 0) {
        appendToOutput("[GapCloser] VecDb is empty. Insert vectors first.\n");
        return;
    }

    // Search with a random query
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> query(768);
    for (auto& v : query) v = dist(rng);

    std::vector<int32_t> results;
    int32_t found = m_copilotGap->GetVecDb().Search(query.data(), results, 5);

    std::ostringstream os;
    os << "[GapCloser] Search returned " << found << " results: ";
    for (int i = 0; i < found; ++i) {
        if (i > 0) os << ", ";
        os << results[i];
    }
    os << "\n";
    appendToOutput(os.str());
}

void Win32IDE::cmdGapVecDbDelete() {
    if (!m_copilotGap || !m_copilotGap->GetVecDb().IsInitialized()) {
        appendToOutput("[GapCloser] VecDb not initialized.\n");
        return;
    }
    // Delete node 0 as demo
    if (m_copilotGap->GetVecDb().Delete(0)) {
        appendToOutput("[GapCloser] Node 0 marked as deleted (tombstoned).\n");
    } else {
        appendToOutput("[GapCloser] Delete failed (invalid node ID).\n");
    }
}

void Win32IDE::cmdGapVecDbStatus() {
    if (!m_copilotGap) {
        appendToOutput("[GapCloser] Not initialized.\n");
        return;
    }
    appendToOutput(m_copilotGap->GetVecDb().GetStatusString());
}

void Win32IDE::cmdGapVecDbBench() {
    if (!m_copilotGap || !m_copilotGap->GetVecDb().IsInitialized()) {
        appendToOutput("[GapCloser] VecDb not initialized.\n");
        return;
    }

    appendToOutput("[GapCloser] Running VecDb benchmark (100 inserts + 10 searches)...\n");

    std::thread([this]() {
        auto start = std::chrono::steady_clock::now();
        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        // Insert 100 vectors
        for (int i = 0; i < 100; ++i) {
            std::vector<float> vec(768);
            for (auto& v : vec) v = dist(rng);
            m_copilotGap->GetVecDb().Insert(vec.data());
        }

        auto insertEnd = std::chrono::steady_clock::now();

        // Search 10 times
        for (int i = 0; i < 10; ++i) {
            std::vector<float> query(768);
            for (auto& v : query) v = dist(rng);
            std::vector<int32_t> results;
            m_copilotGap->GetVecDb().Search(query.data(), results, 5);
        }

        auto searchEnd = std::chrono::steady_clock::now();

        double insertMs = std::chrono::duration<double, std::milli>(
            insertEnd - start).count();
        double searchMs = std::chrono::duration<double, std::milli>(
            searchEnd - insertEnd).count();

        std::ostringstream os;
        os << "[GapCloser] Benchmark results:\n"
           << "  100 inserts: " << std::fixed << std::setprecision(2)
           << insertMs << " ms (" << (insertMs / 100.0) << " ms/insert)\n"
           << "  10 searches: " << searchMs << " ms ("
           << (searchMs / 10.0) << " ms/search)\n"
           << "  Total nodes: " << m_copilotGap->GetVecDb().GetNodeCount() << "\n";
        appendToOutput(os.str());
    }).detach();
}

// ============================================================================
// Composer Command Handlers
// ============================================================================

void Win32IDE::cmdGapComposerBegin() {
    if (!m_copilotGap) { initCopilotGap(); }

    if (m_copilotGap->GetComposer().HasActiveTransaction()) {
        appendToOutput("[GapCloser] Transaction already in progress. Commit or cancel first.\n");
        return;
    }

    if (m_copilotGap->GetComposer().BeginTransaction()) {
        appendToOutput("[GapCloser] Transaction started (max 256 file ops).\n");
    } else {
        appendToOutput("[GapCloser] Failed to begin transaction.\n");
    }
}

void Win32IDE::cmdGapComposerAdd() {
    if (!m_copilotGap || !m_copilotGap->GetComposer().HasActiveTransaction()) {
        appendToOutput("[GapCloser] No active transaction. Begin one first.\n");
        return;
    }

    // Demo: queue a test file creation
    const char* testContent = "// Auto-generated by CopilotGapCloser Composer\n";
    if (m_copilotGap->GetComposer().CreateFile(
            "d:\\rawrxd\\composer_test.txt", testContent, strlen(testContent))) {
        appendToOutput("[GapCloser] Queued: CREATE composer_test.txt\n");
    } else {
        appendToOutput("[GapCloser] Failed to queue file operation.\n");
    }
}

void Win32IDE::cmdGapComposerCommit() {
    if (!m_copilotGap || !m_copilotGap->GetComposer().HasActiveTransaction()) {
        appendToOutput("[GapCloser] No active transaction to commit.\n");
        return;
    }

    appendToOutput("[GapCloser] Committing transaction...\n");
    if (m_copilotGap->GetComposer().Commit()) {
        std::ostringstream os;
        os << "[GapCloser] Transaction COMMITTED (total: "
           << m_copilotGap->GetComposer().GetTxCount() << ")\n";
        appendToOutput(os.str());
    } else {
        appendToOutput("[GapCloser] Transaction FAILED — rolled back.\n");
    }
}

void Win32IDE::cmdGapComposerStatus() {
    if (!m_copilotGap) {
        appendToOutput("[GapCloser] Not initialized.\n");
        return;
    }
    appendToOutput(m_copilotGap->GetComposer().GetStatusString());
}

// ============================================================================
// CRDT Command Handlers
// ============================================================================

void Win32IDE::cmdGapCrdtInit() {
    if (!m_copilotGap) { initCopilotGap(); }

    if (m_copilotGap->GetCrdt().IsInitialized()) {
        appendToOutput("[GapCloser] CRDT document already initialized.\n");
        return;
    }

    // Generate a random UUID
    uint8_t uuid[16] = {};
    for (int i = 0; i < 16; ++i)
        uuid[i] = static_cast<uint8_t>(GetTickCount64() + i * 17);

    if (m_copilotGap->GetCrdt().InitDocument(uuid, 0)) {
        appendToOutput("[GapCloser] CRDT document initialized (peer 0, 16-peer capacity).\n");
    } else {
        appendToOutput("[GapCloser] CRDT init FAILED.\n");
    }
}

void Win32IDE::cmdGapCrdtInsert() {
    if (!m_copilotGap || !m_copilotGap->GetCrdt().IsInitialized()) {
        appendToOutput("[GapCloser] CRDT document not initialized.\n");
        return;
    }

    const char* testText = "Hello, CRDT world! ";
    int64_t ts = m_copilotGap->GetCrdt().InsertText(
        m_copilotGap->GetCrdt().GetDocLength(), testText,
        static_cast<int32_t>(strlen(testText)));

    std::ostringstream os;
    os << "[GapCloser] Inserted text at Lamport=" << ts
       << ", doc length=" << m_copilotGap->GetCrdt().GetDocLength() << "\n";
    appendToOutput(os.str());
}

void Win32IDE::cmdGapCrdtDelete() {
    if (!m_copilotGap || !m_copilotGap->GetCrdt().IsInitialized()) {
        appendToOutput("[GapCloser] CRDT document not initialized.\n");
        return;
    }

    int64_t len = m_copilotGap->GetCrdt().GetDocLength();
    if (len == 0) {
        appendToOutput("[GapCloser] Document is empty.\n");
        return;
    }

    int32_t delLen = static_cast<int32_t>(len < 5 ? len : 5);
    int64_t ts = m_copilotGap->GetCrdt().DeleteText(0, delLen);

    std::ostringstream os;
    os << "[GapCloser] Deleted " << delLen << " chars at Lamport=" << ts
       << ", doc length=" << m_copilotGap->GetCrdt().GetDocLength() << "\n";
    appendToOutput(os.str());
}

void Win32IDE::cmdGapCrdtStatus() {
    if (!m_copilotGap) {
        appendToOutput("[GapCloser] Not initialized.\n");
        return;
    }
    appendToOutput(m_copilotGap->GetCrdt().GetStatusString());
}

// ============================================================================
// Git Context Command Handlers
// ============================================================================

void Win32IDE::cmdGapGitContext() {
    if (!m_copilotGap) { initCopilotGap(); }

    appendToOutput("[GapCloser] Extracting Git context...\n");
    std::string ctx = m_copilotGap->GetGitCtx().ExtractContext(
        "d:\\rawrxd", nullptr, 0);
    appendToOutput(ctx);
    appendToOutput("\n");
}

void Win32IDE::cmdGapGitBranch() {
    if (!m_copilotGap) { initCopilotGap(); }

    m_copilotGap->GetGitCtx().SetBranch("main");
    m_copilotGap->GetGitCtx().SetCommitHash("0000000000000000000000000000000000000000");
    appendToOutput("[GapCloser] Git branch set to 'main'.\n");
}
