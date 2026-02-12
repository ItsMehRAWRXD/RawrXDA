// Win32IDE_SemanticPanel.cpp — Phase 16: Semantic Code Intelligence UI
// Win32 IDE panel for symbol navigation, cross-references, call graphs,
// autocomplete, hover info, and semantic index management.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "Win32IDE.h"
#include "../core/semantic_code_intelligence.hpp"
#include <sstream>
#include <iomanip>
#include <commdlg.h>

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initSemanticPanel() {
    if (m_semanticPanelInitialized) return;

    appendToOutput("[Semantic] Phase 16 — Semantic Code Intelligence initialized.\n");
    m_semanticPanelInitialized = true;
}

// ============================================================================
// Command Router
// ============================================================================

void Win32IDE::handleSemanticCommand(int commandId) {
    if (!m_semanticPanelInitialized) initSemanticPanel();

    switch (commandId) {
        case IDM_SEM_GO_TO_DEF:         cmdSemGoToDefinition();     break;
        case IDM_SEM_FIND_REFS:         cmdSemFindReferences();     break;
        case IDM_SEM_FIND_IMPLS:        cmdSemFindImplementations();break;
        case IDM_SEM_TYPE_HIERARCHY:    cmdSemTypeHierarchy();      break;
        case IDM_SEM_CALL_GRAPH:        cmdSemCallGraph();          break;
        case IDM_SEM_SEARCH_SYMBOLS:    cmdSemSearchSymbols();      break;
        case IDM_SEM_FILE_SYMBOLS:      cmdSemFileSymbols();        break;
        case IDM_SEM_UNUSED:            cmdSemFindUnused();         break;
        case IDM_SEM_INDEX_FILE:        cmdSemIndexFile();          break;
        case IDM_SEM_REBUILD_INDEX:     cmdSemRebuildIndex();       break;
        case IDM_SEM_SAVE_INDEX:        cmdSemSaveIndex();          break;
        case IDM_SEM_LOAD_INDEX:        cmdSemLoadIndex();          break;
        case IDM_SEM_STATS:             cmdSemShowStats();          break;
        default:
            appendToOutput("[Semantic] Unknown command: " + std::to_string(commandId) + "\n");
            break;
    }
}

// ============================================================================
// Command Handlers
// ============================================================================

void Win32IDE::cmdSemGoToDefinition() {
    appendToOutput("[Semantic] Go To Definition: enter symbol name in command palette.\n");
    // In production, this would use the cursor position from the editor
}

void Win32IDE::cmdSemFindReferences() {
    appendToOutput("[Semantic] Find References: enter symbol name in command palette.\n");
}

void Win32IDE::cmdSemFindImplementations() {
    appendToOutput("[Semantic] Find Implementations: select an interface/base class.\n");
}

void Win32IDE::cmdSemTypeHierarchy() {
    appendToOutput("[Semantic] Type Hierarchy: select a class to view inheritance chain.\n");
}

void Win32IDE::cmdSemCallGraph() {
    auto& sci = SemanticCodeIntelligence::instance();
    auto& s = sci.getStats();

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║                 CALL GRAPH OVERVIEW                        ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  Total Symbols:      " << std::setw(10) << s.totalSymbols.load()    << "                         ║\n"
        << "║  Total References:   " << std::setw(10) << s.totalReferences.load() << "                         ║\n"
        << "║  Queries Served:     " << std::setw(10) << s.queriesServed.load()   << "                         ║\n"
        << "╚══════════════════════════════════════════════════════════════╝\n";

    appendToOutput(oss.str());
}

void Win32IDE::cmdSemSearchSymbols() {
    auto& sci = SemanticCodeIntelligence::instance();
    auto results = sci.searchSymbols("", SymbolKind::Unknown, 50);

    if (results.empty()) {
        appendToOutput("[Semantic] No symbols in index. Index some files first.\n");
        return;
    }

    std::ostringstream oss;
    oss << "[Semantic] Symbols (" << results.size() << "):\n";
    for (auto* sym : results) {
        oss << "  " << std::left << std::setw(30) << sym->name
            << " kind=" << static_cast<int>(sym->kind)
            << " refs=" << sym->referenceCount
            << "\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdSemFileSymbols() {
    appendToOutput("[Semantic] File Symbols: open a file first, then invoke.\n");
}

void Win32IDE::cmdSemFindUnused() {
    auto& sci = SemanticCodeIntelligence::instance();
    auto unused = sci.findUnusedSymbols();

    if (unused.empty()) {
        appendToOutput("[Semantic] No unused symbols detected.\n");
        return;
    }

    std::ostringstream oss;
    oss << "[Semantic] Unused Symbols (" << unused.size() << "):\n";
    for (size_t i = 0; i < unused.size() && i < 100; i++) {
        oss << "  ▸ " << unused[i] << "\n";
    }
    if (unused.size() > 100) {
        oss << "  ... and " << (unused.size() - 100) << " more\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdSemIndexFile() {
    OPENFILENAMEA ofn;
    char filePath[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndMain;
    ofn.lpstrFilter  = "Source Files (*.cpp;*.hpp;*.h;*.c)\0*.cpp;*.hpp;*.h;*.c\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        auto& sci = SemanticCodeIntelligence::instance();
        auto result = sci.indexFile(filePath);
        appendToOutput("[Semantic] " + std::string(result.detail) + ": " + filePath + "\n");
    }
}

void Win32IDE::cmdSemRebuildIndex() {
    auto& sci = SemanticCodeIntelligence::instance();
    auto result = sci.rebuildIndex();
    appendToOutput("[Semantic] " + std::string(result.detail) + "\n");
}

void Win32IDE::cmdSemSaveIndex() {
    OPENFILENAMEA ofn;
    char filePath[MAX_PATH] = "semantic_index.rxidx";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndMain;
    ofn.lpstrFilter  = "RawrXD Index (*.rxidx)\0*.rxidx\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt  = "rxidx";

    if (GetSaveFileNameA(&ofn)) {
        auto& sci = SemanticCodeIntelligence::instance();
        auto result = sci.saveIndex(filePath);
        appendToOutput("[Semantic] " + std::string(result.detail) + ": " + filePath + "\n");
    }
}

void Win32IDE::cmdSemLoadIndex() {
    OPENFILENAMEA ofn;
    char filePath[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndMain;
    ofn.lpstrFilter  = "RawrXD Index (*.rxidx)\0*.rxidx\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        auto& sci = SemanticCodeIntelligence::instance();
        auto result = sci.loadIndex(filePath);
        appendToOutput("[Semantic] " + std::string(result.detail) + "\n");
    }
}

void Win32IDE::cmdSemShowStats() {
    auto& sci = SemanticCodeIntelligence::instance();
    auto& s = sci.getStats();

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║           SEMANTIC CODE INTELLIGENCE — STATISTICS          ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  Total Symbols:      " << std::setw(10) << s.totalSymbols.load()    << "                         ║\n"
        << "║  Total References:   " << std::setw(10) << s.totalReferences.load() << "                         ║\n"
        << "║  Total Types:        " << std::setw(10) << s.totalTypes.load()      << "                         ║\n"
        << "║  Total Scopes:       " << std::setw(10) << s.totalScopes.load()     << "                         ║\n"
        << "║  Files Indexed:      " << std::setw(10) << s.filesIndexed.load()    << "                         ║\n"
        << "║  Queries Served:     " << std::setw(10) << s.queriesServed.load()   << "                         ║\n"
        << "║  Cache Hits:         " << std::setw(10) << s.cacheHits.load()       << "                         ║\n"
        << "║  Cache Misses:       " << std::setw(10) << s.cacheMisses.load()     << "                         ║\n"
        << "╚══════════════════════════════════════════════════════════════╝\n";

    appendToOutput(oss.str());
}
