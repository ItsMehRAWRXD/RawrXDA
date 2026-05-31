// =============================================================================
// OrchestratorBridge.cpp — Minimal Wiring Layer for CLI Ollama Testing
// =============================================================================

#include "OrchestratorBridge.h"
#include "FIMPromptBuilder.h"
#include <iostream>

namespace RawrXD {
namespace Agent {

// Static instance
OrchestratorBridge& OrchestratorBridge::Instance() {
    static OrchestratorBridge instance;
    return instance;
}

bool OrchestratorBridge::Initialize(const std::string& workingDir,
                                    const std::string& ollamaUrl) {
    m_workingDir = workingDir;
    m_initialized = true;
    
    // Initialize native client
    m_nativeClient = std::make_unique<NativeInferenceClient>();
    m_nativeConfig.host = ollamaUrl;
    
    return true;
}

std::string OrchestratorBridge::RunAgent(const std::string& userPrompt) {
    if (!EnsureClientReady()) {
        return "[Error] Orchestrator not initialized";
    }
    
    // Simplified agent execution
    return "Agent execution placeholder: " + userPrompt.substr(0, 50);
}

void OrchestratorBridge::RunAgentAsync(const std::string& userPrompt) {
    if (!EnsureClientReady()) {
        if (m_onError) m_onError("Orchestrator not initialized");
        return;
    }
    
    // Launch async execution in a detached thread
    std::thread([this, userPrompt]() {
        try {
            auto result = RunAgent(userPrompt);
            if (m_onAgentComplete) {
                m_onAgentComplete(result);
            }
        } catch (const std::exception& e) {
            if (m_onError) {
                m_onError(std::string("Async agent error: ") + e.what());
            }
        }
    }).detach();
}

Prediction::PredictionResult OrchestratorBridge::RequestGhostText(
    const Prediction::PredictionContext& ctx) {
    
    Prediction::PredictionResult result;
    
    if (!EnsureClientReady()) {
        result.success = false;
        result.error = "Not initialized";
        return result;
    }
    
    // Build FIM prompt
    std::string prompt = ctx.prefix + "<|fim_middle|>" + ctx.suffix;
    
    result.text = "// Ghost text placeholder";
    result.completion = result.text;
    result.success = true;
    
    return result;
}

void OrchestratorBridge::RequestGhostTextStream(
    const Prediction::PredictionContext& ctx,
    Prediction::StreamTokenCallback onToken) {
    
    if (!EnsureClientReady() || !onToken) {
        return;
    }
    
    try {
        // Build FIM prompt
        std::string prompt = ctx.prefix + "<|fim_middle|>" + ctx.suffix;
        
        // Use FIMStream from native client
        m_nativeClient->FIMStream(prompt, "", "",
            [onToken](const std::string& token) { onToken(token, false); },
            [onToken](const std::string& full, uint64_t, uint64_t, double) { onToken(full, true); },
            [](const std::string& err) { (void)err; });
        
    } catch (const std::exception& e) {
        if (m_onError) {
            m_onError(std::string("Ghost text stream error: ") + e.what());
        }
    }
}

void OrchestratorBridge::SetModel(const std::string& model) {
    m_nativeConfig.chat_model = model;
}

void OrchestratorBridge::SetFIMModel(const std::string& model) {
    m_nativeConfig.fim_model = model;
}

void OrchestratorBridge::SetTemperature(float temperature) {
    m_nativeConfig.temperature = temperature;
}

void OrchestratorBridge::SetMaxSteps(int steps) {
    m_maxSteps = steps;
}

void OrchestratorBridge::SetWorkingDirectory(const std::string& dir) {
    m_workingDir = dir;
}

bool OrchestratorBridge::EnsureClientReady() {
    return m_initialized && m_nativeClient != nullptr;
}

void OrchestratorBridge::RefreshAvailableModels() {
    if (!EnsureClientReady()) return;
    
    try {
        // Query native client for available models
        auto models = m_nativeClient->ListModels();
        m_availableModels = std::move(models);
        
        if (m_availableModels.empty()) {
            // Fallback to default models
            m_availableModels = {
                "qwen2.5-coder:14b",
                "qwen2.5-coder:7b",
                "deepseek-coder:6.7b",
                "codellama:13b",
                "mistral",
                "phi3:medium",
                "llama3.1:8b"
            };
        }
        
        if (m_onStatusUpdate) {
            m_onStatusUpdate("Available models refreshed: " + 
                           std::to_string(m_availableModels.size()) + " models found");
        }
    } catch (const std::exception& e) {
        if (m_onError) m_onError(std::string("Failed to refresh models: ") + e.what());
    }
}

void OrchestratorBridge::ApplyConfig() {
    if (!m_nativeClient) return;
    
    // Apply configuration to the native client
    m_nativeClient->SetConfig(m_nativeConfig);
}

std::string OrchestratorBridge::SelectPreferredModel(bool preferCoder) const {
    (void)preferCoder;
    return m_nativeConfig.chat_model.empty() ? "default" : m_nativeConfig.chat_model;
}

} // namespace Agent
} // namespace RawrXD

// C interop exports
extern "C" __declspec(dllexport) int RawrXD_AgentRunSync(const char* prompt,
                                                          char* out_buf,
                                                          unsigned int out_buf_size,
                                                          unsigned int* out_required) {
    if (!prompt || !out_buf || out_buf_size == 0) {
        return -1;
    }
    
    auto& bridge = RawrXD::Agent::OrchestratorBridge::Instance();
    std::string result = bridge.RunAgent(prompt);
    
    if (out_required) {
        *out_required = static_cast<unsigned int>(result.length() + 1);
    }
    
    if (result.length() >= out_buf_size) {
        return -2; // Buffer too small
    }
    
    std::copy(result.begin(), result.end(), out_buf);
    out_buf[result.length()] = '\0';
    
    return 0;
}

extern "C" __declspec(dllexport) int RawrXD_AgentRequestFIMSync(const char* prefix,
                                                                  const char* suffix,
                                                                  const char* file_path,
                                                                  char* out_buf,
                                                                  unsigned int out_buf_size,
                                                                  unsigned int* out_required) {
    (void)file_path;
    
    if (!prefix || !suffix || !out_buf || out_buf_size == 0) {
        return -1;
    }
    
    RawrXD::Prediction::PredictionContext ctx;
    ctx.prefix = prefix;
    ctx.suffix = suffix;
    
    auto& bridge = RawrXD::Agent::OrchestratorBridge::Instance();
    auto result = bridge.RequestGhostText(ctx);
    
    std::string completion = result.success ? result.completion : "";
    
    if (out_required) {
        *out_required = static_cast<unsigned int>(completion.length() + 1);
    }
    
    if (completion.length() >= out_buf_size) {
        return -2;
    }
    
    std::copy(completion.begin(), completion.end(), out_buf);
    out_buf[completion.length()] = '\0';
    
    return result.success ? 0 : -3;
}

extern "C" __declspec(dllexport) int RawrXD_AgentSetTemperature(float temperature) {
    auto& bridge = RawrXD::Agent::OrchestratorBridge::Instance();
    bridge.SetTemperature(temperature);
    return 0;
}
