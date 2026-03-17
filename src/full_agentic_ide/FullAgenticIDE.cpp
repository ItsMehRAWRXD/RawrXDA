// =============================================================================
// Full Agentic IDE — Implementation (single orchestrator for model, chat, tools, workspace)
// =============================================================================

#include "FullAgenticIDE.h"
#include "../win32app/Win32IDE.h"
#include "../win32app/Win32IDE_AgenticBridge.h"
#include <memory>

namespace full_agentic_ide {

struct FullAgenticIDE::Impl {
    Win32IDE* ide = nullptr;
    std::unique_ptr<AgenticBridge> bridge;

    Impl(Win32IDE* i) : ide(i) {
        bridge = std::make_unique<AgenticBridge>(ide);
    }
};

FullAgenticIDE::FullAgenticIDE(Win32IDE* ide)
    : m_impl(std::make_unique<Impl>(ide)) {}

FullAgenticIDE::~FullAgenticIDE() = default;

bool FullAgenticIDE::initialize(const FullAgenticIDEConfig& config) {
    if (!m_impl->bridge)
        return false;
    bool ok = m_impl->bridge->Initialize(config.frameworkPath, config.defaultModel);
    if (ok && !config.ollamaServer.empty())
        m_impl->bridge->SetOllamaServer(config.ollamaServer);
    return ok;
}

bool FullAgenticIDE::isInitialized() const {
    return m_impl->bridge && m_impl->bridge->IsInitialized();
}

bool FullAgenticIDE::loadModel(const std::string& path) {
    return m_impl->bridge && m_impl->bridge->LoadModel(path);
}

std::string FullAgenticIDE::getCurrentModel() const {
    return m_impl->bridge ? m_impl->bridge->GetCurrentModel() : "";
}

AgentResponse FullAgenticIDE::chat(const std::string& userMessage) {
    if (!m_impl->bridge || !m_impl->bridge->IsInitialized())
        return {AgentResponseType::AGENT_ERROR, "Full Agentic IDE not initialized", "", "", ""};
    return m_impl->bridge->ExecuteAgentCommand(userMessage);
}

void FullAgenticIDE::setWorkspaceRoot(const std::string& root) {
    if (m_impl->bridge)
        m_impl->bridge->SetWorkspaceRoot(root);
}

std::string FullAgenticIDE::getWorkspaceRoot() const {
    return m_impl->bridge ? m_impl->bridge->GetWorkspaceRoot() : "";
}

void FullAgenticIDE::setOutputCallback(std::function<void(const std::string&, const std::string&)> cb) {
    if (m_impl->bridge)
        m_impl->bridge->SetOutputCallback(std::move(cb));
}

void FullAgenticIDE::setErrorCallback(std::function<void(const std::string&)> cb) {
    if (m_impl->bridge)
        m_impl->bridge->SetErrorCallback(std::move(cb));
}

void FullAgenticIDE::setProgressCallback(std::function<void(const std::string&)> cb) {
    if (m_impl->bridge)
        m_impl->bridge->SetProgressCallback(std::move(cb));
}

std::vector<std::string> FullAgenticIDE::getAvailableTools() const {
    return m_impl->bridge ? m_impl->bridge->GetAvailableTools() : std::vector<std::string>{};
}

std::string FullAgenticIDE::getStatus() const {
    return m_impl->bridge ? m_impl->bridge->GetAgentStatus() : "Not initialized";
}

AgenticBridge* FullAgenticIDE::getBridge() {
    return m_impl->bridge.get();
}

} // namespace full_agentic_ide
