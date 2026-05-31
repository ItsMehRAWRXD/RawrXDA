// ============================================================================
// CLI_SlashRouter.hpp — CLI Integration for 25 Slash Commands
// ============================================================================
// Provides full functionality for all 25 commands in headless/terminal mode.
// Integrates with InteractiveShell for seamless CLI experience.
// ============================================================================

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace RawrXD {
    class CPUInferenceEngine;
    class InteractiveShell;
}

class AgenticEngine;

namespace RawrXD {
namespace CLI {

// ============================================================================
// CLI Context - Provides IDE-like interface for commands
// ============================================================================

class CLIContext
{
public:
    CLIContext() = default;
    
    // File operations
    std::string getCurrentFile() const;
    void setCurrentFile(const std::string& path);
    
    std::string getEditorSelection() const;
    void setEditorSelection(const std::string& sel);
    
    // Model operations
    std::shared_ptr<CPUInferenceEngine> getInferenceEngine() const;
    void setInferenceEngine(std::shared_ptr<CPUInferenceEngine> engine);
    
    std::shared_ptr<AgenticEngine> getAgenticEngine() const;
    void setAgenticEngine(std::shared_ptr<AgenticEngine> engine);
    
    // Model state
    std::string getLoadedModelPath() const;
    void setLoadedModelPath(const std::string& path);
    
    std::vector<std::string> getAvailableModels() const;
    void setAvailableModels(const std::vector<std::string>& models);
    
    // Ollama connection
    bool isOllamaConnected() const;
    void setOllamaConnected(bool connected);
    
    // KV-Cache state
    uint64_t getKVCacheSeqLen() const;
    void setKVCacheSeqLen(uint64_t len);
    
    // Output
    void appendToOutput(const std::string& msg, const std::string& pane = "Output");
    void clearOutput(const std::string& pane = "Output");
    
    // Inference
    std::string sendMessageToModel(const std::string& prompt);
    bool loadModelFromPath(const std::string& path);
    
    // Current File Context Toggle
    bool isCurrentFileContextEnabled() const { return m_currentFileContextEnabled; }
    void setCurrentFileContextEnabled(bool enabled) { m_currentFileContextEnabled = enabled; }
};

// ============================================================================
// Public API
// ============================================================================

/// Initialize the CLI slash router with inference engines
void InitializeCLISlashRouter(
    std::shared_ptr<CPUInferenceEngine> inferenceEngine,
    std::shared_ptr<AgenticEngine> agenticEngine);

/// Get the global CLI context
CLIContext* GetCLIContext();

/// Process a slash command and return the result
std::string ProcessSlashCommand(const std::string& input);

/// Register all slash commands with an InteractiveShell
void RegisterSlashCommands(InteractiveShell& shell);

}  // namespace CLI
}  // namespace RawrXD

// ============================================================================
// C API for external integration
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the CLI slash router
void CLI_InitializeSlashRouter(void* inferenceEngine, void* agenticEngine);

/// Process a slash command
const char* CLI_ProcessSlashCommand(const char* input);

/// Register slash commands with an InteractiveShell
void CLI_RegisterSlashCommands(void* shell);

/// Set the current file context
void CLI_SetCurrentFile(const char* path);

/// Set the editor selection context
void CLI_SetEditorSelection(const char* selection);

#ifdef __cplusplus
}
#endif