// Win32IDE_CruciblePanel.cpp — Phase 48: The Final Crucible UI Integration
// Wires the CrucibleEngine stress-test harness into the Win32IDE
// command palette, menu bar, and output panel.
// Handles all IDM_CRUCIBLE_* commands.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "Win32IDE.h"
#include <sstream>
#include <iomanip>
#include <commdlg.h>
#include <thread>

using namespace RawrXD::Crucible;

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initCrucible() {
    if (m_crucibleInitialized) return;

    m_crucibleEngine = std::make_unique<CrucibleEngine>();
    if (!m_crucibleEngine->initialize()) {
        appendToOutput("[Crucible] ERROR: Failed to initialize CrucibleEngine.\n");
        return;
    }

    // Set up progress callback
    m_crucibleEngine->setProgressCallback(
        [](CrucibleStage stage, float progress, const char* detail, void* ud) {
            (void)ud;
            char buf[512];
            snprintf(buf, sizeof(buf), "[Crucible] [%.0f%%] %s: %s\n",
                     progress * 100.0f, CrucibleEngine::stageName(stage), detail);
            OutputDebugStringA(buf);
        }, nullptr);

    // Set up stage-complete callback
    m_crucibleEngine->setStageCompleteCallback(
        [](const CrucibleStageResult* result, void* ud) {
            (void)ud;
            char buf[512];
            snprintf(buf, sizeof(buf), "[Crucible] %s %s: %s (%.2f ms, %llu items)\n",
                     result->success ? "[PASS]" : "[FAIL]",
                     CrucibleEngine::stageName(result->stage),
                     result->detail, result->durationMs,
                     (unsigned long long)result->itemsProcessed);
            OutputDebugStringA(buf);
        }, nullptr);

    m_crucibleInitialized = true;
    appendToOutput("[Crucible] Phase 48: The Final Crucible initialized.\n");
    appendToOutput("[Crucible] 3 barrels loaded: Shadow Patch | Cluster Hammer | Semantic Index\n");
}

// ============================================================================
// Command Router
// ============================================================================

void Win32IDE::handleCrucibleCommand(int commandId) {
    if (!m_crucibleInitialized) {
        initCrucible();
    }

    switch (commandId) {
        case IDM_CRUCIBLE_RUN_ALL:              cmdCrucibleRunAll();            break;
        case IDM_CRUCIBLE_RUN_SHADOW:           cmdCrucibleRunShadow();         break;
        case IDM_CRUCIBLE_RUN_CLUSTER:          cmdCrucibleRunCluster();        break;
        case IDM_CRUCIBLE_RUN_SEMANTIC:         cmdCrucibleRunSemantic();       break;
        case IDM_CRUCIBLE_CANCEL:               cmdCrucibleCancel();            break;
        case IDM_CRUCIBLE_STATUS:               cmdCrucibleStatus();            break;
        case IDM_CRUCIBLE_REPORT:               cmdCrucibleReport();            break;
        case IDM_CRUCIBLE_EXPORT_JSON:          cmdCrucibleExportJSON();        break;
        case IDM_CRUCIBLE_CONFIG:               cmdCrucibleConfig();            break;
        case IDM_CRUCIBLE_HELP:                 cmdCrucibleHelp();              break;
        default:
            appendToOutput("[Crucible] Unknown command: " + std::to_string(commandId) + "\n");
            break;
    }
}

// ============================================================================
// Menu Creation
// ============================================================================

void Win32IDE::createCrucibleMenu(HMENU parentMenu) {
    HMENU hCrucible = CreatePopupMenu();

    // Run submenu
    HMENU hRun = CreatePopupMenu();
    AppendMenuA(hRun, MF_STRING, IDM_CRUCIBLE_RUN_ALL,     "Run &All Barrels\tCtrl+Shift+F12");
    AppendMenuA(hRun, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hRun, MF_STRING, IDM_CRUCIBLE_RUN_SHADOW,  "Barrel 1: &Shadow Patch");
    AppendMenuA(hRun, MF_STRING, IDM_CRUCIBLE_RUN_CLUSTER, "Barrel 2: &Cluster Hammer");
    AppendMenuA(hRun, MF_STRING, IDM_CRUCIBLE_RUN_SEMANTIC,"Barrel 3: S&emantic Index");

    AppendMenuA(hCrucible, MF_POPUP, (UINT_PTR)hRun, "&Run");
    AppendMenuA(hCrucible, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hCrucible, MF_STRING, IDM_CRUCIBLE_CANCEL,       "&Cancel Running Test");
    AppendMenuA(hCrucible, MF_STRING, IDM_CRUCIBLE_STATUS,       "&Status");
    AppendMenuA(hCrucible, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hCrucible, MF_STRING, IDM_CRUCIBLE_REPORT,       "View &Report");
    AppendMenuA(hCrucible, MF_STRING, IDM_CRUCIBLE_EXPORT_JSON,  "&Export JSON...");
    AppendMenuA(hCrucible, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hCrucible, MF_STRING, IDM_CRUCIBLE_CONFIG,       "C&onfigure...");
    AppendMenuA(hCrucible, MF_STRING, IDM_CRUCIBLE_HELP,         "&Help");

    AppendMenuA(parentMenu, MF_POPUP, (UINT_PTR)hCrucible, "Cr&ucible");
}

// ============================================================================
// Command Handlers
// ============================================================================

void Win32IDE::cmdCrucibleRunAll() {
    if (!m_crucibleEngine) {
        appendToOutput("[Crucible] Engine not initialized.\n");
        return;
    }
    if (m_crucibleEngine->isRunning()) {
        appendToOutput("[Crucible] Tests already running. Cancel first.\n");
        return;
    }

    appendToOutput("\n");
    appendToOutput("╔══════════════════════════════════════════════════════════════════╗\n");
    appendToOutput("║       PHASE 48: THE FINAL CRUCIBLE — COMMENCING                ║\n");
    appendToOutput("║       3 Barrels × 8 Stages = 24 Tests                          ║\n");
    appendToOutput("╚══════════════════════════════════════════════════════════════════╝\n");
    appendToOutput("\n");

    // Run async so the UI doesn't freeze
    m_crucibleEngine->setCompleteCallback(
        [](const CrucibleSummary* summary, void* ud) {
            auto* self = reinterpret_cast<Win32IDE*>(ud);
            if (!self || !self->m_crucibleEngine) return;

            std::string report = self->m_crucibleEngine->getReport();
            self->appendToOutput(report);

            if (summary->allPassed) {
                self->appendToOutput("\n[Crucible] === ALL 24 STAGES PASSED ===\n");
            } else {
                char buf[128];
                snprintf(buf, sizeof(buf),
                    "\n[Crucible] === %d PASSED, %d FAILED ===\n",
                    summary->passed, summary->failed);
                self->appendToOutput(buf);
            }
        }, this);

    m_crucibleEngine->runAllAsync();
    appendToOutput("[Crucible] Running all 3 barrels asynchronously...\n");
}

void Win32IDE::cmdCrucibleRunShadow() {
    if (!m_crucibleEngine) return;
    if (m_crucibleEngine->isRunning()) {
        appendToOutput("[Crucible] Already running.\n");
        return;
    }

    appendToOutput("[Crucible] Running Barrel 1: Shadow Patch...\n");

    std::thread([this]() {
        auto result = m_crucibleEngine->runBarrel(CrucibleBarrel::ShadowPatch);
        std::ostringstream os;
        os << "[Crucible] Shadow Patch: " << (result.allPassed ? "PASSED" : "FAILED")
           << " (" << result.stagesPassed << "/" << result.stageCount
           << " stages, " << result.totalMs << " ms)\n";
        for (int i = 0; i < result.stageCount; ++i) {
            os << "  " << (result.stages[i].success ? "[PASS]" : "[FAIL]")
               << " " << CrucibleEngine::stageName(result.stages[i].stage)
               << ": " << result.stages[i].detail << "\n";
        }
        this->appendToOutput(os.str());
    }).detach();
}

void Win32IDE::cmdCrucibleRunCluster() {
    if (!m_crucibleEngine) return;
    if (m_crucibleEngine->isRunning()) {
        appendToOutput("[Crucible] Already running.\n");
        return;
    }

    appendToOutput("[Crucible] Running Barrel 2: Cluster Hammer...\n");

    std::thread([this]() {
        auto result = m_crucibleEngine->runBarrel(CrucibleBarrel::ClusterHammer);
        std::ostringstream os;
        os << "[Crucible] Cluster Hammer: " << (result.allPassed ? "PASSED" : "FAILED")
           << " (" << result.stagesPassed << "/" << result.stageCount
           << " stages, " << result.totalMs << " ms)\n";
        for (int i = 0; i < result.stageCount; ++i) {
            os << "  " << (result.stages[i].success ? "[PASS]" : "[FAIL]")
               << " " << CrucibleEngine::stageName(result.stages[i].stage)
               << ": " << result.stages[i].detail << "\n";
        }
        this->appendToOutput(os.str());
    }).detach();
}

void Win32IDE::cmdCrucibleRunSemantic() {
    if (!m_crucibleEngine) return;
    if (m_crucibleEngine->isRunning()) {
        appendToOutput("[Crucible] Already running.\n");
        return;
    }

    appendToOutput("[Crucible] Running Barrel 3: Semantic Index...\n");

    std::thread([this]() {
        auto result = m_crucibleEngine->runBarrel(CrucibleBarrel::SemanticIndex);
        std::ostringstream os;
        os << "[Crucible] Semantic Index: " << (result.allPassed ? "PASSED" : "FAILED")
           << " (" << result.stagesPassed << "/" << result.stageCount
           << " stages, " << result.totalMs << " ms)\n";
        for (int i = 0; i < result.stageCount; ++i) {
            os << "  " << (result.stages[i].success ? "[PASS]" : "[FAIL]")
               << " " << CrucibleEngine::stageName(result.stages[i].stage)
               << ": " << result.stages[i].detail << "\n";
        }
        this->appendToOutput(os.str());
    }).detach();
}

void Win32IDE::cmdCrucibleCancel() {
    if (!m_crucibleEngine) return;
    if (!m_crucibleEngine->isRunning()) {
        appendToOutput("[Crucible] No test currently running.\n");
        return;
    }
    m_crucibleEngine->cancel();
    appendToOutput("[Crucible] Cancel requested — aborting after current stage.\n");
}

void Win32IDE::cmdCrucibleStatus() {
    if (!m_crucibleEngine) {
        appendToOutput("[Crucible] Engine not initialized.\n");
        return;
    }

    std::ostringstream os;
    os << "[Crucible] Status:\n";
    os << "  Initialized: " << (m_crucibleEngine->isInitialized() ? "yes" : "no") << "\n";
    os << "  Running:     " << (m_crucibleEngine->isRunning() ? "yes" : "no") << "\n";
    os << "  Progress:    " << std::fixed << std::setprecision(1)
       << (m_crucibleEngine->getProgress() * 100.0f) << "%\n";

    auto config = m_crucibleEngine->getConfig();
    os << "  Config:\n";
    os << "    Synthetic target:    " << (config.useSyntheticTarget ? "yes" : "no") << "\n";
    os << "    Flash Attn heads:    " << config.flashAttnHeads << "\n";
    os << "    Flash Attn seqLen:   " << config.flashAttnSeqLen << "\n";
    os << "    Flash Attn headDim:  " << config.flashAttnHeadDim << "\n";
    os << "    Benchmark iters:     " << config.benchmarkIterations << "\n";
    os << "    Max files to index:  " << config.maxFilesToIndex << "\n";
    os << "    Abort on fail:       " << (config.abortOnFirstFailure ? "yes" : "no") << "\n";

    appendToOutput(os.str());
}

void Win32IDE::cmdCrucibleReport() {
    if (!m_crucibleEngine) {
        appendToOutput("[Crucible] No engine — run the test first.\n");
        return;
    }

    std::string report = m_crucibleEngine->getReport();
    if (report.empty()) {
        appendToOutput("[Crucible] No results yet. Run the crucible first.\n");
        return;
    }

    appendToOutput("\n");
    appendToOutput(report);
}

void Win32IDE::cmdCrucibleExportJSON() {
    if (!m_crucibleEngine) return;

    std::string json = m_crucibleEngine->toJSON();
    if (json.empty()) {
        appendToOutput("[Crucible] No results to export.\n");
        return;
    }

    // Save dialog
    OPENFILENAMEA ofn = {};
    char szFile[MAX_PATH] = "crucible_results.json";
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = m_hwndMain;
    ofn.lpstrFilter  = "JSON Files\0*.json\0All Files\0*.*\0";
    ofn.lpstrFile    = szFile;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt  = "json";

    if (GetSaveFileNameA(&ofn)) {
        FILE* fp = fopen(szFile, "w");
        if (fp) {
            fwrite(json.c_str(), 1, json.size(), fp);
            fclose(fp);
            appendToOutput("[Crucible] Results exported to: " + std::string(szFile) + "\n");
        } else {
            appendToOutput("[Crucible] Failed to write file.\n");
        }
    }
}

void Win32IDE::cmdCrucibleConfig() {
    if (!m_crucibleEngine) {
        initCrucible();
    }
    if (!m_crucibleEngine) return;

    auto config = m_crucibleEngine->getConfig();

    // Simple dialog to configure key parameters
    // For now, use a message box with current config + instructions
    std::ostringstream os;
    os << "Current Crucible Configuration:\n\n"
       << "Barrel 1 (Shadow Patch):\n"
       << "  Synthetic target: " << (config.useSyntheticTarget ? "Yes" : "No") << "\n"
       << "  Target function: " << (config.targetFunctionName.empty()
                                     ? "(auto)" : config.targetFunctionName) << "\n\n"
       << "Barrel 2 (Cluster Hammer):\n"
       << "  Flash Attention heads: " << config.flashAttnHeads << "\n"
       << "  Head dimension: " << config.flashAttnHeadDim << "\n"
       << "  Sequence length: " << config.flashAttnSeqLen << "\n"
       << "  Batch size: " << config.flashAttnBatch << "\n"
       << "  Benchmark iterations: " << config.benchmarkIterations << "\n\n"
       << "Barrel 3 (Semantic Index):\n"
       << "  Source tree: " << (config.sourceTreePath.empty()
                                 ? "(project dir)" : config.sourceTreePath) << "\n"
       << "  Max files: " << config.maxFilesToIndex << "\n\n"
       << "General:\n"
       << "  Abort on first failure: " << (config.abortOnFirstFailure ? "Yes" : "No") << "\n"
       << "  Stage timeout: " << config.timeoutPerStageMs << " ms\n\n"
       << "Use the command palette to modify individual settings.";

    MessageBoxA(m_hwndMain, os.str().c_str(), "Crucible Configuration", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdCrucibleHelp() {
    const char* helpText =
        "Phase 48: The Final Crucible\n"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
        "The Crucible is a unified stress-test harness that chains three\n"
        "\"barrel\" tests to exercise every major subsystem:\n\n"
        "BARREL 1 — SHADOW PATCH\n"
        "  Builds a CFG from synthetic code, converts to SSA form,\n"
        "  runs 4 optimization passes (constant propagation, copy\n"
        "  propagation, dead code elimination, CSE), generates an\n"
        "  optimized machine code payload, and live-patches it into\n"
        "  executable memory via the UnifiedHotpatchManager.\n\n"
        "BARREL 2 — CLUSTER HAMMER\n"
        "  Initializes the Distributed Pipeline Orchestrator,\n"
        "  allocates Flash Attention Q/K/V/O matrices, builds a\n"
        "  benchmark DAG, distributes tasks via work-stealing,\n"
        "  executes Flash Attention forward passes (AVX-512 or\n"
        "  naive fallback), and validates output correctness.\n\n"
        "BARREL 3 — SEMANTIC INDEX\n"
        "  Scans a source tree, parses compilation units, builds\n"
        "  a symbol table, resolves types, constructs a call graph,\n"
        "  computes cross-references, serializes the index to disk,\n"
        "  and validates the entire cross-reference database.\n\n"
        "Total: 3 barrels × 8 stages = 24 individual tests.\n\n"
        "Keyboard shortcut: Ctrl+Shift+F12 to run all barrels.";

    MessageBoxA(m_hwndMain, helpText, "Crucible Help", MB_OK | MB_ICONINFORMATION);
}
