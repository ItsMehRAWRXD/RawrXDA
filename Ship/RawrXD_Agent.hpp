// RawrXD_Agent.hpp - Final Integration Header
// Pure C++20 / Win32 - Zero Qt Dependencies
// Single header to include all agent components
#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// Core foundations (C++20 String = std::wstring, no Qt)
#include "agent_kernel_main.hpp"

// Tool system
#include "ToolExecutionEngine.hpp"
#include "ToolImplementations.hpp"

// LLM integration
#include "LLMClient.hpp"

// Agent orchestration
#include "AgentOrchestrator.hpp"

// UI components
#include "Win32UIIntegration.hpp"

namespace RawrXD {

// Version info
constexpr const wchar_t* AGENT_VERSION = L"1.0.0";
constexpr const wchar_t* AGENT_NAME = L"RawrXD Agent";
constexpr const wchar_t* AGENT_BUILD_DATE = L"2025";

// Initialize all components
inline bool initializeAgent() {
    // Initialize COM for UUID generation
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Initialize Winsock for networking
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    return true;
}

// Cleanup all components
inline void cleanupAgent() {
    WSACleanup();
    CoUninitialize();
}

// Full agent system
class Agent {
public:
    Agent() = default;
    ~Agent() { shutdown(); }

    bool initialize(const AgentConfig& config = AgentConfig()) {
        if (m_initialized) return true;

        if (!initializeAgent()) {
            return false;
        }

        m_config = config;

        // Initialize tool engine
        m_toolEngine = std::make_unique<ToolExecutionEngine>();
        Tools::registerAllTools(*m_toolEngine);

        // Set tool context
        ToolContext ctx;
        ctx.workingDirectory = config.workingDirectory;
        ctx.onOutput = [this](const String& output) {
            if (m_outputCallback) m_outputCallback(output);
        };
        ctx.onConfirmation = [this](const String& tool, const String& desc) {
            if (m_confirmCallback) return m_confirmCallback(tool, desc);
            return m_config.autoApproveTools;
        };
        m_toolEngine->setContext(ctx);

        // Initialize orchestrator
        m_orchestrator = std::make_unique<AgentOrchestrator>();
        m_orchestrator->initialize(config);
        m_orchestrator->setToolEngine(m_toolEngine.get());

        m_initialized = true;
        return true;
    }

    void shutdown() {
        if (!m_initialized) return;

        m_orchestrator.reset();
        m_toolEngine.reset();

        cleanupAgent();
        m_initialized = false;
    }

    // Process a message from the user (C++20 std::wstring API — Qt-free)
    void processMessage(const String& message) {
        if (!m_initialized || !m_orchestrator) return;
        m_orchestrator->processMessage(message);
    }

    // Process message asynchronously
    void processMessageAsync(const String& message) {
        if (!m_initialized || !m_orchestrator) return;
        m_orchestrator->runAgentLoopAsync(message);
    }

    // Stop current execution
    void stop() {
        if (m_orchestrator) m_orchestrator->stop();
    }

    // Clear conversation
    void clearConversation() {
        if (m_orchestrator) m_orchestrator->clearConversation();
    }

    // Check if LLM is available
    bool isLLMAvailable() {
        return m_orchestrator && m_orchestrator->isLLMAvailable();
    }

    // List available models (C++20 Vector<String>)
    Vector<String> listModels() {
        if (!m_orchestrator) return {};
        return m_orchestrator->listModels();
    }

    // Get current state
    AgentState state() const {
        return m_orchestrator ? m_orchestrator->state() : AgentState::Idle;
    }

    // Set event callback
    void setEventCallback(AgentEventCallback callback) {
        if (m_orchestrator) {
            m_orchestrator->setEventCallback(std::move(callback));
        }
    }

    // Set output callback (C++20 String)
    void setOutputCallback(std::function<void(const String&)> callback) {
        m_outputCallback = std::move(callback);
    }

    // Set confirmation callback (C++20 String)
    void setConfirmCallback(std::function<bool(const String&, const String&)> callback) {
        m_confirmCallback = std::move(callback);
    }

    // Get orchestrator
    AgentOrchestrator* orchestrator() { return m_orchestrator.get(); }

    // Get tool engine
    ToolExecutionEngine* toolEngine() { return m_toolEngine.get(); }

    // Get config
    const AgentConfig& config() const { return m_config; }

    // Register custom tool
    void registerTool(const ToolDefinition& def, ToolExecutor executor) {
        if (m_toolEngine) {
            m_toolEngine->registerTool(def, std::move(executor));
        }
    }

    // Execute tool directly (C++20 String)
    ToolResult executeTool(const String& name, const JsonObject& params) {
        if (!m_toolEngine) return ToolResult::Error(L"Tool engine not initialized");
        return m_toolEngine->execute(name, params);
    }

private:
    bool m_initialized = false;
    AgentConfig m_config;
    UniquePtr<ToolExecutionEngine> m_toolEngine;
    UniquePtr<AgentOrchestrator> m_orchestrator;
    std::function<void(const String&)> m_outputCallback;
    std::function<bool(const String&, const String&)> m_confirmCallback;
};

// Agent application with UI
class AgentApp {
public:
    AgentApp(HINSTANCE hInstance)
        : m_hInstance(hInstance) {}

    bool initialize(const AgentConfig& config = AgentConfig()) {
        if (!m_agent.initialize(config)) {
            return false;
        }

        if (!m_window.create(m_hInstance, String(L"RawrXD Agent - ") + AGENT_VERSION)) {
            return false;
        }

        m_window.setAgentEventCallback([this](const AgentEvent& event) {
            handleEvent(event);
        });
        m_window.setAgent(m_agent.orchestrator());

        return true;
    }

    int run() {
        m_window.show();
        return m_window.run();
    }

    Agent& agent() { return m_agent; }
    UI::AgentWindow& window() { return m_window; }

private:
    void handleEvent(const AgentEvent& event) {
        const String& msg = event.message;
        switch (event.type) {
            case AgentEvent::Type::StateChanged:
                m_window.statusBar().setState(msg);
                break;

            case AgentEvent::Type::MessageReceived:
                m_window.chatPanel().appendAssistantMessage(msg);
                break;

            case AgentEvent::Type::ToolCalled:
                m_window.chatPanel().appendToolCall(msg,
                    StringUtils::FromUtf8(JsonParser::Serialize(event.data, 2)));
                break;

            case AgentEvent::Type::ToolResult:
                m_window.statusBar().setMessage(L"Tool completed");
                break;

            case AgentEvent::Type::Error:
                m_window.chatPanel().appendError(msg);
                m_window.statusBar().setState(L"Error");
                break;

            case AgentEvent::Type::StreamChunk:
                m_window.chatPanel().appendStreamChunk(msg);
                break;

            case AgentEvent::Type::Completed:
                m_window.statusBar().setState(L"Ready");
                break;
        }
    }

    HINSTANCE m_hInstance;
    Agent m_agent;
    UI::AgentWindow m_window;
};

} // namespace RawrXD
