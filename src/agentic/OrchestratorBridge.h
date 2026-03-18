// =============================================================================
// OrchestratorBridge.h — Minimal Wiring Layer for CLI Ollama Testing
// =============================================================================
#pragma once

#include "AgentOllamaClient.h"
#include "AgentToolHandlers.h"
#include "PredictionProvider.h"
#include <memory>
#include <string>
#include <vector>

namespace RawrXD {
namespace Agent {

// ---------------------------------------------------------------------------
// OrchestratorBridge — Minimal for CLI Ollama testing
// ---------------------------------------------------------------------------
class OrchestratorBridge {
public:
    static OrchestratorBridge& Instance();

    // ---- Initialization (call during CLI startup) ----
    bool Initialize(const std::string& workingDir,
                    const std::string& ollamaUrl = "http://localhost:11434");

    bool IsInitialized() const { return m_initialized; }

    // ---- Agent Execution - Simplified for CLI ----
    std::string RunAgent(const std::string& userPrompt);
    void RunAgentAsync(const std::string& userPrompt);

    // ---- Ghost Text / FIM ----
    Prediction::PredictionResult RequestGhostText(
        const Prediction::PredictionContext& ctx);
    void RequestGhostTextStream(
        const Prediction::PredictionContext& ctx,
        Prediction::StreamTokenCallback onToken);

    // ---- Configuration ----
    void SetModel(const std::string& model);
    void SetFIMModel(const std::string& model);
    void SetMaxSteps(int steps);
    void SetWorkingDirectory(const std::string& dir);
    OrchestratorBridge() = default;
    ~OrchestratorBridge() = default;

private:
    bool EnsureClientReady();
    void RefreshAvailableModels();
    void ApplyConfig();
    std::string SelectPreferredModel(bool preferCoder) const;

public:
    bool m_initialized = false;
    std::string m_workingDir;
    std::unique_ptr<AgentOllamaClient> m_ollamaClient;
    OllamaConfig m_ollamaConfig;
    int m_maxSteps = 8;
    std::vector<std::string> m_availableModels;
};

} // namespace Agent
} // namespace RawrXD
