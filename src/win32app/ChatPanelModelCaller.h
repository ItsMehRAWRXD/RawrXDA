// ChatPanelModelCaller.h — Verified ModelCaller connection for Chat Panel
// Fixes the partial Chat Panel by ensuring ModelCaller is properly wired.

#pragma once

#include "core/ContextFusionEngine.h"
#include "chat_interface.h"
#include "ai_model_caller.h"  // Real ModelCaller implementation

namespace RawrXD {

class ChatPanelModelCaller : public IContextSubscriber {
public:
    ChatPanelModelCaller(IChatPanel* chatPanel);
    
    // IContextSubscriber
    void OnContextUpdate(const ContextFrame& frame) override;
    void OnContextEvent(const ContextEvent& event) override;
    int GetPriority() const override { return 20; }
    std::string GetName() const override { return "ChatPanelModelCaller"; }
    
    // Lifecycle
    bool Initialize(const std::string& modelEndpoint);
    void Shutdown();
    
    // Configuration
    void SetSystemPrompt(const std::string& prompt);
    void SetMaxContextLines(int lines) { m_maxContextLines = lines; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    
    // Status
    bool IsConnected() const { return ModelCaller::IsReady(); }
    std::string GetLastError() const { return m_lastError; }

private:
    IChatPanel* m_chatPanel;
    std::string m_systemPrompt;
    std::string m_lastError;
    int m_maxContextLines = 50;
    bool m_enabled = true;
    bool m_initialized = false;
    
    uint64_t m_lastVersion = 0;
    
    void SendToModel(const ContextFrame& frame, const std::string& userMessage);
    std::string BuildPrompt(const ContextFrame& frame, const std::string& userMessage);
    void HandleModelResponse(const std::string& response);
    void HandleModelError(const std::string& error);
};

} // namespace RawrXD