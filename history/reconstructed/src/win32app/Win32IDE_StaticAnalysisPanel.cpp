// Win32IDE_StaticAnalysisPanel.cpp — Phase 15: Static Analysis Engine UI
// Win32 IDE panel for CFG/SSA analysis, dominator trees, loop detection,
// optimization passes, and DOT/JSON export of control flow graphs.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "Win32IDE.h"
#include "../core/static_analysis_engine.hpp"
#include <sstream>
#include <iomanip>
#include <commdlg.h>

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initStaticAnalysisPanel() {
    if (m_staticAnalysisPanelInitialized) return;

    appendToOutput("[StaticAnalysis] Phase 15 — Static Analysis Engine initialized.\n");
    m_staticAnalysisPanelInitialized = true;
}

// ============================================================================
// Command Router
// ============================================================================

void Win32IDE::handleStaticAnalysisCommand(int commandId) {
    if (!m_staticAnalysisPanelInitialized) initStaticAnalysisPanel();

    switch (commandId) {
        case IDM_SA_BUILD_CFG:          cmdSABuildCFG();           break;
        case IDM_SA_COMPUTE_DOMINATORS: cmdSAComputeDominators();  break;
        case IDM_SA_CONVERT_SSA:        cmdSAConvertSSA();         break;
        case IDM_SA_DETECT_LOOPS:       cmdSADetectLoops();        break;
        case IDM_SA_OPTIMIZE:           cmdSAOptimize();           break;
        case IDM_SA_FULL_ANALYSIS:      cmdSAFullAnalysis();       break;
        case IDM_SA_EXPORT_DOT:         cmdSAExportDOT();          break;
        case IDM_SA_EXPORT_JSON:        cmdSAExportJSON();         break;
        case IDM_SA_STATS:              cmdSAShowStats();          break;
        default:
            appendToOutput("[StaticAnalysis] Unknown command: " + std::to_string(commandId) + "\n");
            break;
    }
}

// ============================================================================
// Command Handlers
// ============================================================================

void Win32IDE::cmdSABuildCFG() {
    auto& engine = StaticAnalysisEngine::instance();
    auto funcs = engine.getAllFunctions();

    if (funcs.empty()) {
        appendToOutput("[StaticAnalysis] No functions loaded. Load instructions first.\n");
        return;
    }

    uint32_t funcId = funcs.front();
    const auto* cfg = engine.getCFG(funcId);

    std::ostringstream oss;
    oss << "[StaticAnalysis] CFG Status:\n";

    if (!cfg || cfg->blocks.empty()) {
        oss << "  No basic blocks. Load instructions first, then build the CFG.\n";
    } else {
        oss << "  Basic Blocks: " << cfg->blocks.size() << "\n"
            << "  Entry Block:  " << cfg->entryBlockId << "\n";
        for (const auto& [id, block] : cfg->blocks) {
            oss << "  BB" << id << ": " << block.instructions.size() << " instructions"
                << ", succs=[";
            for (size_t i = 0; i < block.successors.size(); i++) {
                if (i > 0) oss << ",";
                oss << block.successors[i];
            }
            oss << "]\n";
        }
    }

    appendToOutput(oss.str());
}

void Win32IDE::cmdSAComputeDominators() {
    auto& engine = StaticAnalysisEngine::instance();
    auto funcs = engine.getAllFunctions();
    if (funcs.empty()) {
        appendToOutput("[StaticAnalysis] No functions loaded.\n");
        return;
    }
    uint32_t funcId = funcs.front();
    auto result = engine.computeDominators(funcId);

    if (result.success) {
        appendToOutput("[StaticAnalysis] Dominator tree computed successfully.\n");

        const auto* cfg = engine.getCFG(funcId);
        if (cfg) {
            std::ostringstream oss;
            for (const auto& [id, block] : cfg->blocks) {
                oss << "  BB" << id << " idom=" << block.immediateDominator << "\n";
            }
            appendToOutput(oss.str());
        }
    } else {
        appendToOutput("[StaticAnalysis] Dominator computation failed: " +
                       std::string(result.detail) + "\n");
    }
}

void Win32IDE::cmdSAConvertSSA() {
    auto& engine = StaticAnalysisEngine::instance();
    auto funcs = engine.getAllFunctions();
    if (funcs.empty()) {
        appendToOutput("[StaticAnalysis] No functions loaded.\n");
        return;
    }
    auto result = engine.convertToSSA(funcs.front());

    if (result.success) {
        appendToOutput("[StaticAnalysis] SSA conversion complete.\n");
    } else {
        appendToOutput("[StaticAnalysis] SSA conversion failed: " +
                       std::string(result.detail) + "\n");
    }
}

void Win32IDE::cmdSADetectLoops() {
    auto& engine = StaticAnalysisEngine::instance();
    auto funcs = engine.getAllFunctions();
    if (funcs.empty()) {
        appendToOutput("[StaticAnalysis] No functions loaded.\n");
        return;
    }
    uint32_t funcId = funcs.front();
    auto result = engine.detectLoops(funcId);
    if (!result.success) {
        appendToOutput("[StaticAnalysis] Loop detection failed: " +
                       std::string(result.detail) + "\n");
        return;
    }

    auto loops = engine.getLoops(funcId);
    if (loops.empty()) {
        appendToOutput("[StaticAnalysis] No natural loops detected.\n");
        return;
    }

    std::ostringstream oss;
    oss << "[StaticAnalysis] Detected " << loops.size() << " loops:\n";
    for (auto& loop : loops) {
        oss << "  Loop: header=BB" << loop.headerBlockId
            << ", depth=" << loop.nestingDepth
            << ", blocks={";
        for (size_t i = 0; i < loop.bodyBlockIds.size() && i < 10; i++) {
            if (i > 0) oss << ",";
            oss << loop.bodyBlockIds[i];
        }
        if (loop.bodyBlockIds.size() > 10) oss << ",...";
        oss << "}\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdSAOptimize() {
    auto& engine = StaticAnalysisEngine::instance();
    auto funcs = engine.getAllFunctions();
    if (funcs.empty()) {
        appendToOutput("[StaticAnalysis] No functions loaded.\n");
        return;
    }
    uint32_t funcId = funcs.front();

    appendToOutput("[StaticAnalysis] Running optimization passes...\n");

    auto r1 = engine.constantPropagation(funcId);
    appendToOutput("  Constant propagation: " + std::string(r1.detail) + "\n");

    auto r2 = engine.deadCodeElimination(funcId);
    appendToOutput("  Dead code elimination: " + std::string(r2.detail) + "\n");

    auto r3 = engine.copyPropagation(funcId);
    appendToOutput("  Copy propagation: " + std::string(r3.detail) + "\n");

    auto r4 = engine.commonSubexpressionElimination(funcId);
    appendToOutput("  Common subexpression elimination: " + std::string(r4.detail) + "\n");

    appendToOutput("[StaticAnalysis] All optimization passes complete.\n");
}

void Win32IDE::cmdSAFullAnalysis() {
    auto& engine = StaticAnalysisEngine::instance();
    auto funcs = engine.getAllFunctions();
    if (funcs.empty()) {
        appendToOutput("[StaticAnalysis] No functions loaded.\n");
        return;
    }
    appendToOutput("[StaticAnalysis] Running full analysis pipeline...\n");

    auto result = engine.runFullAnalysis(funcs.front());
    appendToOutput("[StaticAnalysis] Full analysis: " + std::string(result.detail) + "\n");
}

void Win32IDE::cmdSAExportDOT() {
    auto& engine = StaticAnalysisEngine::instance();

    OPENFILENAMEA ofn;
    char filePath[MAX_PATH] = "cfg_export.dot";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndMain;
    ofn.lpstrFilter  = "DOT Files (*.dot)\0*.dot\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt  = "dot";

    if (GetSaveFileNameA(&ofn)) {
        auto funcs = engine.getAllFunctions();
        uint32_t funcId = funcs.empty() ? 0 : funcs.front();
        auto result = engine.exportDot(funcId, filePath);
        if (result.success) {
            appendToOutput("[StaticAnalysis] CFG exported to: " + std::string(filePath) + "\n");
        } else {
            appendToOutput("[StaticAnalysis] Export failed: " + std::string(result.detail) + "\n");
        }
    }
}

void Win32IDE::cmdSAExportJSON() {
    auto& engine = StaticAnalysisEngine::instance();

    OPENFILENAMEA ofn;
    char filePath[MAX_PATH] = "cfg_export.json";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndMain;
    ofn.lpstrFilter  = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt  = "json";

    if (GetSaveFileNameA(&ofn)) {
        auto funcs = engine.getAllFunctions();
        uint32_t funcId = funcs.empty() ? 0 : funcs.front();
        auto result = engine.exportJSON(funcId, filePath);
        if (result.success) {
            appendToOutput("[StaticAnalysis] JSON exported to: " + std::string(filePath) + "\n");
        } else {
            appendToOutput("[StaticAnalysis] Export failed: " + std::string(result.detail) + "\n");
        }
    }
}

void Win32IDE::cmdSAShowStats() {
    auto& engine = StaticAnalysisEngine::instance();
    auto& s = engine.getStats();

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║             STATIC ANALYSIS ENGINE — STATISTICS            ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  Basic Blocks Built:     " << std::setw(10) << s.blocksBuilt.load()          << "                     ║\n"
        << "║  Instructions Parsed:    " << std::setw(10) << s.instructionsParsed.load()   << "                     ║\n"
        << "║  Edges Created:          " << std::setw(10) << s.edgesCreated.load()          << "                     ║\n"
        << "║  Phi Nodes Inserted:     " << std::setw(10) << s.phiNodesInserted.load()     << "                     ║\n"
        << "║  Optimizations Applied:  " << std::setw(10) << s.optimizationsApplied.load() << "                     ║\n"
        << "║  Loops Detected:         " << std::setw(10) << s.loopsDetected.load()         << "                     ║\n"
        << "║  Analyses Run:           " << std::setw(10) << s.analysesRun.load()            << "                     ║\n"
        << "╚══════════════════════════════════════════════════════════════╝\n";

    appendToOutput(oss.str());
}
