#pragma once


class AgenticEngine;

namespace RawrXD {
    class PlanOrchestrator;
}

class ChatInterface : public void {

public:
    explicit ChatInterface(void* parent = nullptr);
    
    // Two-phase initialization: call after void exists
    void initialize();
    
    void setAgenticEngine(AgenticEngine* engine) { m_agenticEngine = engine; }
    void setPlanOrchestrator(RawrXD::PlanOrchestrator* orchestrator) { m_planOrchestrator = orchestrator; }
    
    void addMessage(const std::string& sender, const std::string& message);
    std::string selectedModel() const;
    bool isMaxMode() const;
    void sendMessageProgrammatically(const std::string& message);
    
    // Agent tool commands
    // Updated to accept optional arguments string for future extensions
    void executeAgentCommand(const std::string& command, const std::string& args = "");
    bool isAgentCommand(const std::string& message) const;
    
public:
    void displayResponse(const std::string& response);
    void focusInput();
    void sendMessage();
    void refreshModels();
    void onModelChanged(int index);
    void onModel2Changed(int index);
    void onMaxModeToggled(bool enabled);
    void setCanSendMessage(bool enabled);
    
    // Phase 2: Streaming token progress
    void onTokenGenerated(int delta);
    void hideProgress();


    void messageSent(const std::string& message);
    void modelSelected(const std::string& modelPath);
    void model2Selected(const std::string& modelPath);
    void maxModeChanged(bool enabled);
    void messageReceived(const std::string& reply);
    
private:
    void loadAvailableModels();
    void loadAvailableModelsForSecond();
    std::string resolveGgufPath(const std::string& modelName);  // Resolve Ollama model name to GGUF file
    
    void* message_history_;
    void* message_input_;
    void* modelSelector_;
    void* modelSelector2_;
    void* maxModeToggle_;
    void* statusLabel_;
    bool maxMode_;
    bool m_busy = false;  // Prevent re-entrancy during inference
    std::string m_lastPrompt;  // Store last user message
    AgenticEngine* m_agenticEngine = nullptr;
    RawrXD::PlanOrchestrator* m_planOrchestrator = nullptr;
    
    // Phase 2: Streaming token progress bar
    void* m_tokenProgress{nullptr};
    void** m_hideTimer{nullptr};
};

