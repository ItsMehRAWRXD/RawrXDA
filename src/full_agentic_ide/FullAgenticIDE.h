#pragma once

// =============================================================================
// Full Agentic IDE — Single entry point for the complete agentic stack
// =============================================================================
// This folder (src/full_agentic_ide/) is THE place for everything needed to
// complete and maintain the full agentic IDE. No scattered prototypes.
// Win32IDE and HeadlessIDE use this API only for agentic behavior.
// =============================================================================

#include "../win32app/Win32IDE_AgenticBridge.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Win32IDE;

namespace full_agentic_ide {

struct FullAgenticIDEConfig {
    std::string frameworkPath;
    std::string defaultModel;
    std::string ollamaServer;
};

// Single orchestrator for the full agentic IDE: model, chat, tools, workspace
class FullAgenticIDE {
public:
    explicit FullAgenticIDE(Win32IDE* ide);
    ~FullAgenticIDE();

    bool initialize(const FullAgenticIDEConfig& config = {});
    bool isInitialized() const;

    bool loadModel(const std::string& path);
    std::string getCurrentModel() const;

    // Chat with model (local GGUF or Ollama/cloud); tool calls in response are dispatched
    AgentResponse chat(const std::string& userMessage);

    void setWorkspaceRoot(const std::string& root);
    std::string getWorkspaceRoot() const;

    void setOutputCallback(std::function<void(const std::string&, const std::string&)> cb);
    void setErrorCallback(std::function<void(const std::string&)> cb);
    void setProgressCallback(std::function<void(const std::string&)> cb);

    std::vector<std::string> getAvailableTools() const;
    std::string getStatus() const;

    // Compatibility: expose bridge for existing call sites that need it during migration
    AgenticBridge* getBridge();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace full_agentic_ide
