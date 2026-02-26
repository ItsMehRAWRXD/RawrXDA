/*==========================================================================
 * RawrXD Diagnostics Provider
 *
 * Bridges the toolchain's tc_diagnostic_t format to the IDE's LSP
 * Diagnostic type and integrates with LanguageServerIntegrationImpl.
 *
 * Responsibilities:
 *  - Convert toolchain diagnostics to LSP-compatible format
 *  - Publish diagnostics to LSP when source changes
 *  - Provide code actions for common MASM errors
 *  - Debounce rapid edits before re-analyzing
 *=========================================================================*/

#pragma once

#include "../include/toolchain/toolchain_bridge.h"
#include "language_server_integration.hpp"
#include "CommonTypes.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <mutex>
#include <future>
#include <atomic>
#include <memory>

namespace RawrXD {
namespace IDE {

/* ----- Severity mapping ----- */
enum class DiagSeverity : int {
    Error   = 1,
    Warning = 2,
    Info    = 3,
    Hint    = 4
};

/* ----- Quick-fix descriptor ----- */
struct QuickFix {
    std::string title;
    std::string kind;          /* "quickfix" | "refactor" */
    std::string newText;
    Range       range;
    bool        isPreferred = false;
};

/* ----- DiagnosticsProvider ----- */
class DiagnosticsProvider {
public:
    using PublishCallback = std::function<void(const std::string& uri,
                                               const std::vector<Diagnostic>& diags)>;
    using CodeActionCallback = std::function<void(const std::string& uri,
                                                   const std::vector<CodeAction>& actions)>;

    DiagnosticsProvider();
    ~DiagnosticsProvider();

    /* --- Lifecycle --- */
    bool initialize(RawrXD::Toolchain::ToolchainBridge* bridge);
    void shutdown();

    /* --- Event hooks (called by LSP integration) --- */
    void onDocumentOpened(const std::string& uri, const std::string& content);
    void onDocumentChanged(const std::string& uri, const std::string& content);
    void onDocumentClosed(const std::string& uri);

    /* --- Incremental change (range update) --- */
    void onDocumentRangeChanged(const std::string& uri,
                                 uint32_t startLine, uint32_t startCol,
                                 uint32_t endLine,   uint32_t endCol,
                                 const std::string& newText);

    /* --- On-demand query --- */
    std::vector<Diagnostic> getDiagnostics(const std::string& uri);

    /* --- Code actions for a given range + diagnostics context --- */
    std::vector<CodeAction> getCodeActions(const std::string& uri,
                                            const Range& range,
                                            const std::vector<Diagnostic>& context);

    /* --- Callbacks --- */
    void setPublishCallback(PublishCallback cb)       { m_publishCb = std::move(cb); }
    void setCodeActionCallback(CodeActionCallback cb) { m_codeActionCb = std::move(cb); }

    /* --- Configuration --- */
    void setDebounceMs(uint32_t ms) { m_debounceMs = ms; }
    void setMaxDiagnosticsPerFile(uint32_t n) { m_maxDiags = n; }

    /* --- Stats --- */
    struct Stats {
        uint64_t totalAnalyses     = 0;
        uint64_t totalDiagnostics  = 0;
        double   avgAnalysisTimeMs = 0.0;
    };
    Stats getStats() const;

private:
    /* --- Conversion --- */
    static Diagnostic convertDiagnostic(const RawrXD::Toolchain::ToolchainDiagnostic& td,
                                         const std::string& source);
    static int        mapSeverity(const std::string& sev);
    static CodeAction createQuickFix(const QuickFix& qf);

    /* --- Analysis (may be async) --- */
    void scheduleAnalysis(const std::string& uri);
    void runAnalysis(const std::string& uri);

    /* --- Quick-fix generation --- */
    std::vector<QuickFix> generateQuickFixes(const Diagnostic& diag,
                                              const std::string& uri);

    /* --- Debouncing --- */
    struct PendingAnalysis {
        std::chrono::steady_clock::time_point deadline;
        bool cancelled = false;
    };

    /* --- State --- */
    RawrXD::Toolchain::ToolchainBridge*                     m_bridge     = nullptr;
    std::atomic<bool>                                        m_initialized{false};

    std::mutex                                               m_mutex;
    std::unordered_map<std::string, std::vector<Diagnostic>> m_cache;
    std::unordered_map<std::string, PendingAnalysis>         m_pending;

    PublishCallback     m_publishCb;
    CodeActionCallback  m_codeActionCb;

    uint32_t m_debounceMs = 300;
    uint32_t m_maxDiags   = 500;

    mutable std::mutex m_statsMutex;
    Stats              m_stats;
};

} // namespace IDE
} // namespace RawrXD
