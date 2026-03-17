// AgentOrchestrator.hpp - Agent Loop, Context Management, Tool Orchestration
// Pure C++20 / Win32 - Zero Qt Dependencies
#pragma once

#include "agent_kernel_main.hpp"
#include "QtReplacements.hpp"
#include "ToolExecutionEngine.hpp"
#include "LLMClient.hpp"
#include <filesystem>

namespace RawrXD {

// Agent state
enum class AgentState {
    Idle,
    Thinking,
    ExecutingTool,
    WaitingForInput,
    Error,
    Completed
};

inline String agentStateToString(AgentState state) {
    switch (state) {
        case AgentState::Idle: return L"idle";
        case AgentState::Thinking: return L"thinking";
        case AgentState::ExecutingTool: return L"executing_tool";
        case AgentState::WaitingForInput: return L"waiting_for_input";
        case AgentState::Error: return L"error";
        case AgentState::Completed: return L"completed";
    }
    return L"unknown";
}

// Agent configuration
struct AgentConfig {
    QString model = "qwen2.5-coder:14b";
    QString ollamaHost = "localhost";
    int ollamaPort = 11434;
    int maxIterations = 50;
    int maxToolCalls = 100;
    int contextWindow = 32000;
    double temperature = 0.7;
    QString workingDirectory;
    bool autoApproveTools = false;
    QStringList dangerousTools = {"run_command", "delete_file", "write_file"};

    JsonObject toJson() const {
        JsonObject obj;
        obj[L"model"] = model.toStdWString();
        obj[L"ollamaHost"] = ollamaHost.toStdWString();
        obj[L"ollamaPort"] = static_cast<int64_t>(ollamaPort);
        obj[L"maxIterations"] = static_cast<int64_t>(maxIterations);
        obj[L"maxToolCalls"] = static_cast<int64_t>(maxToolCalls);
        obj[L"contextWindow"] = static_cast<int64_t>(contextWindow);
        obj[L"temperature"] = temperature;
        obj[L"workingDirectory"] = workingDirectory.toStdWString();
        obj[L"autoApproveTools"] = autoApproveTools;
        return obj;
    }
};

// Agent event for UI updates
struct AgentEvent {
    enum class Type {
        StateChanged,
        MessageReceived,
        ToolCalled,
        ToolResult,
        Error,
        StreamChunk,
        Completed
    };

    Type type;
    QString message;
    JsonObject data;
    int64_t timestamp;

    AgentEvent(Type t, const QString& msg = QString())
        : type(t), message(msg)
    {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

using AgentEventCallback = std::function<void(const AgentEvent&)>;

// Task definition
struct AgentTask {
    QString id;
    QString description;
    QString status;
    QStringList subtasks;
    int progress = 0;

    JsonObject toJson() const {
        JsonObject obj;
        obj[L"id"] = id.toStdWString();
        obj[L"description"] = description.toStdWString();
        obj[L"status"] = status.toStdWString();
        obj[L"progress"] = static_cast<int64_t>(progress);
        JsonArray subs;
        for (const auto& s : subtasks) {
            subs.push_back(s.toStdWString());
        }
        obj[L"subtasks"] = subs;
        return obj;
    }
};

// Agent context - workspace and session state
class AgentContext {
public:
    AgentContext() = default;

    void setWorkingDirectory(const QString& dir) {
        m_workingDirectory = dir;
    }

    QString workingDirectory() const {
        return m_workingDirectory.isEmpty() ?
            QString(std::filesystem::current_path().wstring()) : m_workingDirectory;
    }

    void addOpenFile(const QString& path) {
        if (!m_openFiles.contains(path)) {
            m_openFiles.push_back(path);
        }
    }

    void removeOpenFile(const QString& path) {
        auto it = std::find(m_openFiles.begin(), m_openFiles.end(), path);
        if (it != m_openFiles.end()) {
            m_openFiles.erase(it);
        }
    }

    const QStringList& openFiles() const { return m_openFiles; }

    void setVariable(const QString& name, const QVariant& value) {
        m_variables[name] = value;
    }

    QVariant variable(const QString& name) const {
        return m_variables.value(name);
    }

    void addTask(const AgentTask& task) {
        m_tasks[task.id] = task;
    }

    AgentTask* task(const QString& id) {
        auto it = m_tasks.find(id);
        return it != m_tasks.end() ? &it->second : nullptr;
    }

    Vector<AgentTask> tasks() const {
        Vector<AgentTask> result;
        for (const auto& [id, task] : m_tasks) {
            result.push_back(task);
        }
        return result;
    }

    JsonObject toJson() const {
        JsonObject obj;
        obj[L"workingDirectory"] = workingDirectory().toStdWString();

        JsonArray files;
        for (const auto& f : m_openFiles) {
            files.push_back(f.toStdWString());
        }
        obj[L"openFiles"] = files;

        JsonArray tasksArr;
        for (const auto& [id, task] : m_tasks) {
            tasksArr.push_back(task.toJson());
        }
        obj[L"tasks"] = tasksArr;

        return obj;
    }

private:
    QString m_workingDirectory;
    QStringList m_openFiles;
    QMap<QString, QVariant> m_variables;
    QMap<QString, AgentTask> m_tasks;
};

// Main Agent Orchestrator
class AgentOrchestrator {
public:
    AgentOrchestrator()
        : m_llmClient("localhost", 11434)
    {
        m_state = AgentState::Idle;
    }

    // Initialize with config
    void initialize(const AgentConfig& config) {
        m_config = config;
        m_llmClient = LLMClient(config.ollamaHost, config.ollamaPort);
        m_llmClient.setModel(config.model);
        m_llmClient.setTemperature(config.temperature);
        m_llmClient.setMaxTokens(config.contextWindow / 4); // Reserve 3/4 for context

        if (!config.workingDirectory.isEmpty()) {
            m_context.setWorkingDirectory(config.workingDirectory);
        }

        setupSystemPrompt();
    }

    // Set tool engine
    void setToolEngine(ToolExecutionEngine* engine) {
        m_toolEngine = engine;
        if (engine) {
            m_llmClient.setTools(engine->getToolsSchema());
        }
    }

    // Set event callback
    void setEventCallback(AgentEventCallback callback) {
        m_eventCallback = std::move(callback);
    }

    // Process user message
    void processMessage(const QString& message) {
        if (m_state == AgentState::Thinking || m_state == AgentState::ExecutingTool) {
            return; // Already processing
        }

        setState(AgentState::Thinking);
        m_conversation.addUserMessage(message);
        m_iterationCount = 0;
        m_toolCallCount = 0;

        runAgentLoop();
    }

    // Run agent loop asynchronously
    void runAgentLoopAsync(const QString& message) {
        std::thread([this, message]() {
            processMessage(message);
        }).detach();
    }

    // Stop current execution
    void stop() {
        m_shouldStop = true;
    }

    // Get current state
    AgentState state() const { return m_state; }

    // Get context
    AgentContext& context() { return m_context; }
    const AgentContext& context() const { return m_context; }

    // Get conversation
    ConversationManager& conversation() { return m_conversation; }

    // Get config
    const AgentConfig& config() const { return m_config; }

    // Check if LLM is available
    bool isLLMAvailable() {
        return m_llmClient.isAvailable();
    }

    // List available models
    QStringList listModels() {
        return m_llmClient.listModels();
    }

    // Clear conversation
    void clearConversation() {
        m_conversation.clear();
    }

private:
    void setState(AgentState state) {
        m_state = state;
        emitEvent(AgentEvent::Type::StateChanged, agentStateToString(state));
    }

    void emitEvent(AgentEvent::Type type, const QString& message, const JsonObject& data = {}) {
        if (m_eventCallback) {
            AgentEvent event(type, message);
            event.data = data;
            m_eventCallback(event);
        }
    }

    void setupSystemPrompt() {
        QString prompt = R"(You are RawrXD, an autonomous AI coding assistant with access to powerful tools.

Your capabilities include:
- Reading and writing files
- Running terminal commands
- Searching code and documentation
- Managing projects and builds
- Analyzing code and suggesting improvements

Guidelines:
1. Always think step by step before taking action
2. Use tools to gather information before making changes
3. Verify your changes work correctly
4. Ask for clarification when requirements are unclear
5. Be concise but thorough in explanations

Current working directory: )";
        prompt += m_context.workingDirectory();

        m_conversation.setSystemPrompt(prompt);
    }

    void runAgentLoop() {
        m_shouldStop = false;

        while (!m_shouldStop && m_iterationCount < m_config.maxIterations) {
            m_iterationCount++;

            // Get LLM response
            auto response = m_llmClient.complete(m_conversation.getMessages());

            if (!response.success) {
                setState(AgentState::Error);
                emitEvent(AgentEvent::Type::Error, response.error);
                return;
            }

            // Process response content
            if (!response.content.isEmpty()) {
                emitEvent(AgentEvent::Type::MessageReceived, response.content);
            }

            // Check for tool calls
            if (response.toolCalls.empty()) {
                // No tool calls - assistant is done
                m_conversation.addAssistantMessage(response.content);
                setState(AgentState::Completed);
                emitEvent(AgentEvent::Type::Completed, "Task completed");
                return;
            }

            // Add assistant message with tool calls
            Vector<JsonObject> toolCallsJson;
            for (const auto& tc : response.toolCalls) {
                JsonObject tcObj;
                tcObj[L"id"] = tc.id.toStdWString();
                JsonObject func;
                func[L"name"] = tc.name.toStdWString();
                func[L"arguments"] = tc.arguments;
                tcObj[L"function"] = func;
                toolCallsJson.push_back(tcObj);
            }
            m_conversation.addAssistantMessage(response.content, toolCallsJson);

            // Execute tool calls
            for (const auto& toolCall : response.toolCalls) {
                if (m_shouldStop) break;
                if (m_toolCallCount >= m_config.maxToolCalls) {
                    emitEvent(AgentEvent::Type::Error, "Maximum tool calls reached");
                    setState(AgentState::Error);
                    return;
                }

                m_toolCallCount++;
                executeToolCall(toolCall);
            }
        }

        if (m_iterationCount >= m_config.maxIterations) {
            emitEvent(AgentEvent::Type::Error, "Maximum iterations reached");
            setState(AgentState::Error);
        }
    }

    void executeToolCall(const ToolCall& toolCall) {
        setState(AgentState::ExecutingTool);

        JsonObject eventData;
        eventData[L"tool"] = toolCall.name.toStdWString();
        eventData[L"arguments"] = toolCall.arguments;
        emitEvent(AgentEvent::Type::ToolCalled, toolCall.name, eventData);

        // Check if tool requires approval
        bool needsApproval = !m_config.autoApproveTools &&
            m_config.dangerousTools.contains(toolCall.name);

        if (needsApproval) {
            // In a real implementation, this would prompt the user
            // For now, we auto-approve
        }

        // Execute tool
        ToolResult result;
        if (m_toolEngine) {
            result = m_toolEngine->execute(toolCall.name, toolCall.arguments);
        } else {
            result = ToolResult::Error("Tool engine not configured");
        }

        // Add result to conversation
        QString resultStr = JsonParser::Serialize(result.toJson(), 2);
        m_conversation.addToolResult(toolCall.id, toolCall.name, resultStr);

        eventData[L"result"] = result.toJson();
        emitEvent(AgentEvent::Type::ToolResult, resultStr, eventData);

        setState(AgentState::Thinking);
    }

    AgentConfig m_config;
    AgentState m_state = AgentState::Idle;
    AgentContext m_context;
    LLMClient m_llmClient;
    ConversationManager m_conversation;
    ToolExecutionEngine* m_toolEngine = nullptr;
    AgentEventCallback m_eventCallback;

    int m_iterationCount = 0;
    int m_toolCallCount = 0;
    std::atomic<bool> m_shouldStop{false};
};

// Agent factory
class AgentFactory {
public:
    static UniquePtr<AgentOrchestrator> create(const AgentConfig& config,
                                                ToolExecutionEngine* toolEngine)
    {
        auto agent = std::make_unique<AgentOrchestrator>();
        agent->initialize(config);
        agent->setToolEngine(toolEngine);
        return agent;
    }
};

} // namespace RawrXD
