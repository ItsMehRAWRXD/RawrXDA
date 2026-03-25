<<<<<<< HEAD
#pragma once

// C++20 / Win32. Chat interface; no Qt. Opaque handle + callbacks.

#include <string>
#include <functional>

class AgenticEngine;
namespace RawrXD { class PlanOrchestrator; }

class ChatInterface
{
public:
    using MessageSentFn     = std::function<void(const std::string&)>;
    using ModelSelectedFn   = std::function<void(const std::string& modelPath)>;
    using MaxModeChangedFn  = std::function<void(bool)>;
    using MessageReceivedFn = std::function<void(const std::string&)>;

    ChatInterface() = default;
    void initialize();

    void setOnMessageSent(MessageSentFn f)       { m_onMessageSent = std::move(f); }
    void setOnModelSelected(ModelSelectedFn f)  { m_onModelSelected = std::move(f); }
    void setOnMaxModeChanged(MaxModeChangedFn f) { m_onMaxModeChanged = std::move(f); }
    void setOnMessageReceived(MessageReceivedFn f) { m_onMessageReceived = std::move(f); }

    void setAgenticEngine(AgenticEngine* engine) { m_agenticEngine = engine; }
    void setPlanOrchestrator(RawrXD::PlanOrchestrator* orchestrator) { m_planOrchestrator = orchestrator; }

    void addMessage(const std::string& sender, const std::string& message);
    std::string selectedModel() const;
    bool isMaxMode() const;
    void sendMessageProgrammatically(const std::string& message);

    void executeAgentCommand(const std::string& command, const std::string& args = "");
    bool isAgentCommand(const std::string& message) const;

    void displayResponse(const std::string& response);
    void focusInput();
    void sendMessage();
    void refreshModels();
    void onTokenGenerated(int delta);
    void hideProgress();
    void setCanSendMessage(bool enabled);

    void* getWidgetHandle() const { return m_handle; }

private:
    void loadAvailableModels();
    std::string resolveGgufPath(const std::string& modelName);

    void* m_handle = nullptr;
    bool maxMode_ = false;
    bool m_busy = false;
    std::string m_lastPrompt;
    AgenticEngine* m_agenticEngine = nullptr;
    RawrXD::PlanOrchestrator* m_planOrchestrator = nullptr;

    MessageSentFn     m_onMessageSent;
    ModelSelectedFn  m_onModelSelected;
    MaxModeChangedFn  m_onMaxModeChanged;
    MessageReceivedFn m_onMessageReceived;
};
=======
#pragma once

// C++20 / Win32. Chat interface; no Qt. Opaque handle + callbacks.

#include <string>
#include <functional>

class AgenticEngine;
namespace RawrXD { class PlanOrchestrator; }

class ChatInterface
{
public:
    using MessageSentFn     = std::function<void(const std::string&)>;
    using ModelSelectedFn   = std::function<void(const std::string& modelPath)>;
    using MaxModeChangedFn  = std::function<void(bool)>;
    using MessageReceivedFn = std::function<void(const std::string&)>;

    ChatInterface() = default;
    void initialize();

    void setOnMessageSent(MessageSentFn f)       { m_onMessageSent = std::move(f); }
    void setOnModelSelected(ModelSelectedFn f)  { m_onModelSelected = std::move(f); }
    void setOnMaxModeChanged(MaxModeChangedFn f) { m_onMaxModeChanged = std::move(f); }
    void setOnMessageReceived(MessageReceivedFn f) { m_onMessageReceived = std::move(f); }

    void setAgenticEngine(AgenticEngine* engine) { m_agenticEngine = engine; }
    void setPlanOrchestrator(RawrXD::PlanOrchestrator* orchestrator) { m_planOrchestrator = orchestrator; }

    void addMessage(const std::string& sender, const std::string& message);
    std::string selectedModel() const;
    bool isMaxMode() const;
    void sendMessageProgrammatically(const std::string& message);

    void executeAgentCommand(const std::string& command, const std::string& args = "");
    bool isAgentCommand(const std::string& message) const;

    void displayResponse(const std::string& response);
    void focusInput();
    void sendMessage();
    void refreshModels();
    void onTokenGenerated(int delta);
    void hideProgress();
    void setCanSendMessage(bool enabled);

    void* getWidgetHandle() const { return m_handle; }

private:
    void loadAvailableModels();
    std::string resolveGgufPath(const std::string& modelName);

    void* m_handle = nullptr;
    bool maxMode_ = false;
    bool m_busy = false;
    std::string m_lastPrompt;
    AgenticEngine* m_agenticEngine = nullptr;
    RawrXD::PlanOrchestrator* m_planOrchestrator = nullptr;

    MessageSentFn     m_onMessageSent;
    ModelSelectedFn  m_onModelSelected;
    MaxModeChangedFn  m_onMaxModeChanged;
    MessageReceivedFn m_onMessageReceived;
};
>>>>>>> origin/main
