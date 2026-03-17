/*==========================================================================
 * RawrXD Toolchain Integration Module
 *
 * Wires the ToolchainBridge, DiagnosticsProvider, and BuildTaskProvider
 * into the IDE Orchestrator and the LSP layer.
 *
 * This is the single integration point:
 *  - IDEOrchestrator calls ToolchainIntegration::initialize() after
 *    its own component init.
 *  - ToolchainIntegration registers itself with the LSP's
 *    LanguageServerIntegrationImpl for hover / definition / references /
 *    diagnostics / completions / code-actions on .asm files.
 *  - Build tasks are registered for the workspace.
 *=========================================================================*/

#pragma once

#include "../include/toolchain/toolchain_bridge.h"
#include "diagnostics_provider.hpp"
#include "build_task_provider.hpp"
#include "language_server_integration.hpp"
#include "language_server_integration_impl.hpp"
#include "CommonTypes.h"

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace IDE {

/* ---- Configuration ---- */
struct ToolchainConfig {
    bool                   enabled         = true;
    bool                   autoDetectTasks = true;
    bool                   autoAnalyze     = true;
    uint32_t               debounceMs      = 300;
    uint32_t               maxDiagnostics  = 500;
    std::string            workspaceRoot;
    std::vector<std::string> extraIncludePaths;
};

/* ---- Central Integration Facade ---- */

class ToolchainIntegration {
public:
    ToolchainIntegration();
    ~ToolchainIntegration();

    /* --- Lifecycle (called by IDEOrchestrator) --- */
    bool initialize(const ToolchainConfig& config);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(); }

    /* --- Wire into LSP layer --- */
    void connectToLSP(rxd::lsp::LanguageServerIntegrationImpl* lsp);

    /* --- Document events (forwarded from LSP didOpen/didChange/didClose) --- */
    void onDidOpen(const std::string& uri, const std::string& content);
    void onDidChange(const std::string& uri, const std::string& content);
    void onDidClose(const std::string& uri);

    /* --- Build API (exposed to IDE UI / commands) --- */
    uint64_t buildActiveFile(const std::string& filePath);
    uint64_t buildProject(const std::vector<std::string>& files,
                          const std::string& outputPath);
    uint64_t cleanBuild(const std::string& outputDir);
    bool     cancelBuild(uint64_t taskId);
    BuildTaskResult waitForBuild(uint64_t taskId, uint32_t timeoutMs = 30000);

    /* --- Diagnostics on demand --- */
    std::vector<Diagnostic> getDiagnostics(const std::string& uri);

    /* --- Hover / Definition / References / Completions --- */
    std::string getHoverInfo(const std::string& uri, int line, int col);
    RawrXD::Toolchain::ToolchainSymbol findDefinition(const std::string& uri,
                                                        int line, int col);
    std::vector<RawrXD::Toolchain::ToolchainSymbol> findReferences(
        const std::string& uri, const std::string& symbolName);
    std::vector<tc_completion_t> getCompletions(const std::string& uri,
                                                 int line, int col,
                                                 const std::string& trigger);
    std::vector<RawrXD::Toolchain::ToolchainSymbol> getDocumentSymbols(const std::string& uri);

    /* --- Code Actions --- */
    std::vector<CodeAction> getCodeActions(const std::string& uri,
                                            const Range& range,
                                            const std::vector<Diagnostic>& context);

    /* --- VS Code tasks.json --- */
    nlohmann::json generateTasksJson();

    /* --- Component access --- */
    RawrXD::Toolchain::ToolchainBridge*  getToolchainBridge()  { return m_bridge.get(); }
    DiagnosticsProvider*                  getDiagnosticsProvider() { return m_diagProv.get(); }
    BuildTaskProvider*                    getBuildTaskProvider()  { return m_buildProv.get(); }

    /* --- Status / Metrics --- */
    nlohmann::json getStatus() const;
    nlohmann::json getMetrics() const;

private:
    /* ---- Helpers ---- */
    static bool isAssemblyFile(const std::string& uri);
    void registerLSPProviders(rxd::lsp::LanguageServerIntegrationImpl* lsp);

    /* ---- State ---- */
    std::atomic<bool>                                    m_initialized{false};
    ToolchainConfig                                      m_config;

    std::unique_ptr<RawrXD::Toolchain::ToolchainBridge>  m_bridge;
    std::unique_ptr<DiagnosticsProvider>                  m_diagProv;
    std::unique_ptr<BuildTaskProvider>                    m_buildProv;

    rxd::lsp::LanguageServerIntegrationImpl*             m_lsp = nullptr;
    mutable std::mutex                                   m_mutex;
};

} // namespace IDE
} // namespace RawrXD
