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
    void SetTemperature(float temperature);
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

// ---------------------------------------------------------------------------
// MASM/C interop surface (single-hop request model)
// ---------------------------------------------------------------------------
// Returns 0 on success, negative on error.
// If out_buf is provided, output is always NUL-terminated when out_buf_size > 0.
// out_required receives the full required byte count including terminating NUL.
extern "C" __declspec(dllexport) int RawrXD_AgentRunSync(const char* prompt,
                                                          char* out_buf,
                                                          unsigned int out_buf_size,
                                                          unsigned int* out_required);

extern "C" __declspec(dllexport) int RawrXD_AgentRequestFIMSync(const char* prefix,
                                                                  const char* suffix,
                                                                  const char* file_path,
                                                                  char* out_buf,
                                                                  unsigned int out_buf_size,
                                                                  unsigned int* out_required);

extern "C" __declspec(dllexport) int RawrXD_AgentSetTemperature(float temperature);
