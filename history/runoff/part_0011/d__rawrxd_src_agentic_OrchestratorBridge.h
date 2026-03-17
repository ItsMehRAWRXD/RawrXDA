// =============================================================================
// OrchestratorBridge.h — Wiring Layer Between New & Existing Agent Systems
// =============================================================================
// Bridges:
//   1. AgentOllamaClient → BoundedAgentLoop (as LLMChatFunction backend)
//   2. AgentToolRegistry  → AgentToolHandlers (unified tool dispatch)
//   3. FIMPromptBuilder   → PredictionProvider (OllamaProvider FIM)
//   4. DiffEngine         → AgentOrchestrator (coverage-aware verification)
//
// This is the integration seam — the new orchestrator components slot into
// the existing BoundedAgentLoop architecture via adapters, not by replacing it.
// =============================================================================
#pragma once

#include "AgentOrchestrator.h"
#include "BoundedAgentLoop.h"
#include "AgentToolHandlers.h"
#include "OllamaProvider.h"
#include "DiffEngine.h"
#include "FIMPromptBuilder.h"
#include "ToolRegistry.h"
#include "AgentOllamaClient.h"

namespace RawrXD {
namespace Agent {

// ---------------------------------------------------------------------------
// OrchestratorBridge — integrates all agent subsystems
// ---------------------------------------------------------------------------
class OrchestratorBridge {
public:
    static OrchestratorBridge& Instance();

    // ---- Initialization (call during IDE startup) ----
    bool Initialize(const std::string& workingDir,
                    const std::string& ollamaUrl = "http://localhost:11434");

    bool IsInitialized() const { return m_initialized; }

    // ---- Bounded Agent Loop (tool-calling agent) ----
    // Get the BoundedAgentLoop pre-wired with OllamaClient + tools
    BoundedAgentLoop& GetAgentLoop() { return m_agentLoop; }

    // Run a user request through the bounded loop
    std::string RunAgent(const std::string& userPrompt);

    // Run async (fires progress/complete callbacks set on the loop)
    void RunAgentAsync(const std::string& userPrompt);

    // ---- Ghost Text / FIM (inline completions) ----
    // Request a FIM completion from editor state
    Prediction::PredictionResult RequestGhostText(
        const Prediction::PredictionContext& ctx);

    // Stream FIM tokens
    void RequestGhostTextStream(
        const Prediction::PredictionContext& ctx,
        Prediction::StreamTokenCallback onToken);

    // ---- Access subsystems ----
    AgentToolRegistry&       GetToolRegistry()    { return m_xMacroRegistry; }
    AgentOrchestrator&       GetOrchestrator()    { return m_orchestrator; }
    AgentOllamaClient&       GetOllamaClient()    { return *m_ollamaClient; }
    FIMPromptBuilder&        GetFIMBuilder()       { return m_fimBuilder; }
    Diff::DiffEngine&        GetDiffEngine()       { return m_diffEngine; }

    // ---- Configuration ----
    void SetModel(const std::string& model);
    void SetFIMModel(const std::string& model);
    void SetMaxSteps(int steps);
    void SetWorkingDirectory(const std::string& dir);

private:
    OrchestratorBridge() = default;
    ~OrchestratorBridge() = default;

    // Build the LLMChatFunction adapter for BoundedAgentLoop
    LLMChatFunction BuildLLMAdapter();

    // Adapt X-Macro tool schemas for BoundedAgentLoop
    void WireToolSchemas();

    // Wire AgentToolHandlers to X-Macro AgentToolRegistry
    void WireToolHandlers();

    bool m_initialized = false;
    std::string m_workingDir;

    // Core subsystems
    BoundedAgentLoop        m_agentLoop;
    AgentOrchestrator       m_orchestrator;
    AgentToolRegistry&      m_xMacroRegistry = AgentToolRegistry::Instance();
    std::unique_ptr<AgentOllamaClient> m_ollamaClient;
    FIMPromptBuilder        m_fimBuilder;
    Diff::DiffEngine        m_diffEngine;

    // Configuration
    OllamaConfig m_ollamaConfig;

    // Unified tool schemas (merged X-Macro + handler schemas)
    nlohmann::json m_unifiedSchemas;
};

} // namespace Agent
} // namespace RawrXD
