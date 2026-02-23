// AgentOrchestrator.hpp - Agent Loop, Context Management, Tool Orchestration
// Pure C++20 / Win32 - Zero Qt Dependencies
#pragma once

#include "agent_kernel_main.hpp"
#include "ToolExecutionEngine.hpp"
#include "LLMClient.hpp"
#include <filesystem>
#include <any>
#include <algorithm>
#include <mutex>
#include <condition_variable>

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

// Agent configuration — C++20 String (std::wstring), no Qt
struct AgentConfig {
    String model = L"qwen2.5-coder:14b";
    String ollamaHost = L"localhost";
    int ollamaPort = 11434;
    int maxIterations = 50;
    int maxToolCalls = 100;
    int contextWindow = 32000;
    double temperature = 0.7;
    String workingDirectory;
    bool autoApproveTools = false;
    bool useStreaming = true;
    Vector<String> dangerousTools = {L"run_command", L"delete_file", L"write_file"};

    JsonObject toJson() const {
        JsonObject obj;
        obj[L"model"] = model;
        obj[L"ollamaHost"] = ollamaHost;
        obj[L"ollamaPort"] = static_cast<int64_t>(ollamaPort);
        obj[L"maxIterations"] = static_cast<int64_t>(maxIterations);
        obj[L"maxToolCalls"] = static_cast<int64_t>(maxToolCalls);
        obj[L"contextWindow"] = static_cast<int64_t>(contextWindow);
        obj[L"temperature"] = temperature;
        obj[L"workingDirectory"] = workingDirectory;
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
    String message;
    JsonObject data;
    int64_t timestamp;

    AgentEvent(Type t, const String& msg = String())
        : type(t), message(msg)
    {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

using AgentEventCallback = std::function<void(const AgentEvent&)>;

// Task definition — C++20 String, no Qt
struct AgentTask {
    String id;
    String description;
    String status;
    Vector<String> subtasks;
    int progress = 0;

    JsonObject toJson() const {
        JsonObject obj;
        obj[L"id"] = id;
        obj[L"description"] = description;
        obj[L"status"] = status;
        obj[L"progress"] = static_cast<int64_t>(progress);
        JsonArray subs;
        for (const auto& s : subtasks) {
            subs.push_back(s);
        }
        obj[L"subtasks"] = subs;
        return obj;
    }
};

// Agent context - workspace and session state
class AgentContext {
public:
    AgentContext() = default;

    void setWorkingDirectory(const String& dir) {
        m_workingDirectory = dir;
    }

    String workingDirectory() const {
        return m_workingDirectory.empty() ?
            std::filesystem::current_path().wstring() : m_workingDirectory;
    }

    void addOpenFile(const String& path) {
        if (std::find(m_openFiles.begin(), m_openFiles.end(), path) == m_openFiles.end()) {
            m_openFiles.push_back(path);
        }
    }

    void removeOpenFile(const String& path) {
        auto it = std::find(m_openFiles.begin(), m_openFiles.end(), path);
        if (it != m_openFiles.end()) {
            m_openFiles.erase(it);
        }
    }

    const Vector<String>& openFiles() const { return m_openFiles; }

    void setVariable(const String& name, const std::any& value) {
        m_variables[name] = value;
    }

    std::any variable(const String& name) const {
        auto it = m_variables.find(name);
        return it != m_variables.end() ? it->second : std::any{};
    }

    void addTask(const AgentTask& task) {
        m_tasks[task.id] = task;
    }

    AgentTask* task(const String& id) {
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
        obj[L"workingDirectory"] = workingDirectory();

        JsonArray files;
        for (const auto& f : m_openFiles) {
            files.push_back(f);
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
    String m_workingDirectory;
    Vector<String> m_openFiles;
    Map<String, std::any> m_variables;
    Map<String, AgentTask> m_tasks;
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

        if (!config.workingDirectory.empty()) {
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
    void processMessage(const String& message) {
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
    void runAgentLoopAsync(const String& message) {
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
    AgentConfig& config() { return m_config; }

    // Update model and reinitialize LLM client
    void setModel(const String& model) {
        m_config.model = model;
        m_llmClient.setModel(model);
    }

    // Set working directory (updates context and tool engine)
    void setWorkingDirectory(const String& dir) {
        m_context.setWorkingDirectory(dir);
        m_config.workingDirectory = dir;
        if (m_toolEngine) {
            m_toolEngine->context().workingDirectory = dir;
        }
    }

    // Check if LLM is available
    bool isLLMAvailable() {
        return m_llmClient.isAvailable();
    }

    // List available models (C++20 Vector<String>)
    Vector<String> listModels() {
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

    void emitEvent(AgentEvent::Type type, const String& message, const JsonObject& data = {}) {
        if (m_eventCallback) {
            AgentEvent event(type, message);
            event.data = data;
            m_eventCallback(event);
        }
    }

    void setupSystemPrompt() {
        String prompt = L"You are RawrXD, an autonomous AI coding assistant with access to powerful tools.\n\n"
            L"Your capabilities include:\n"
            L"- Reading and writing files\n"
            L"- Running terminal commands\n"
            L"- Searching code and documentation\n"
            L"- Managing projects and builds\n"
            L"- Analyzing code and suggesting improvements\n\n"
            L"Guidelines:\n"
            L"1. Always think step by step before taking action\n"
            L"2. Use tools to gather information before making changes\n"
            L"3. Verify your changes work correctly\n"
            L"4. Ask for clarification when requirements are unclear\n"
            L"5. Be concise but thorough in explanations\n\n"
            L"Current working directory: ";
        prompt += m_context.workingDirectory();

        m_conversation.setSystemPrompt(prompt);
    }

    void runAgentLoop() {
        m_shouldStop = false;

        while (!m_shouldStop && m_iterationCount < m_config.maxIterations) {
            m_iterationCount++;

            LLMResponse response;

            if (m_config.useStreaming) {
                response = runStreamingCompletion();
            } else {
                response = m_llmClient.complete(m_conversation.getMessages());
            }

            if (!response.success) {
                setState(AgentState::Error);
                emitEvent(AgentEvent::Type::Error, response.error);
                return;
            }

            // Process response content (for non-streaming, emit full message; streaming already emitted chunks)
            if (!response.content.empty() && !m_config.useStreaming) {
                emitEvent(AgentEvent::Type::MessageReceived, response.content);
            }

            // Check for tool calls
            if (response.toolCalls.empty()) {
                m_conversation.addAssistantMessage(response.content);
                setState(AgentState::Completed);
                emitEvent(AgentEvent::Type::Completed, L"Task completed");
                return;
            }

            Vector<JsonObject> toolCallsJson;
            for (const auto& tc : response.toolCalls) {
                JsonObject tcObj;
                tcObj[L"id"] = tc.id;
                JsonObject func;
                func[L"name"] = tc.name;
                func[L"arguments"] = tc.arguments;
                tcObj[L"function"] = func;
                toolCallsJson.push_back(tcObj);
            }
            m_conversation.addAssistantMessage(response.content, toolCallsJson);

            for (const auto& toolCall : response.toolCalls) {
                if (m_shouldStop) break;
                if (m_toolCallCount >= m_config.maxToolCalls) {
                    emitEvent(AgentEvent::Type::Error, L"Maximum tool calls reached");
                    setState(AgentState::Error);
                    return;
                }
                m_toolCallCount++;
                executeToolCall(toolCall);
            }
        }

        if (m_iterationCount >= m_config.maxIterations) {
            emitEvent(AgentEvent::Type::Error, L"Maximum iterations reached");
            setState(AgentState::Error);
        }
    }

    LLMResponse runStreamingCompletion() {
        LLMResponse result;
        std::mutex mtx;
        std::condition_variable cv;
        bool done = false;

        m_llmClient.streamComplete(
            m_conversation.getMessages(),
            [this](const String& chunk) {
                if (!m_shouldStop && !chunk.empty()) {
                    emitEvent(AgentEvent::Type::StreamChunk, chunk);
                }
            },
            [&](const String& fullResponse, const Vector<ToolCall>& toolCalls) {
                std::lock_guard<std::mutex> lock(mtx);
                result.success = true;
                result.content = fullResponse;
                result.toolCalls = toolCalls;
                done = true;
                cv.notify_one();
            },
            [&](const String& err) {
                std::lock_guard<std::mutex> lock(mtx);
                result.success = false;
                result.error = err;
                done = true;
                cv.notify_one();
            }
        );

        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return done; });
        return result;
    }

    void executeToolCall(const ToolCall& toolCall) {
        setState(AgentState::ExecutingTool);

        JsonObject eventData;
        eventData[L"tool"] = toolCall.name;
        eventData[L"arguments"] = toolCall.arguments;
        emitEvent(AgentEvent::Type::ToolCalled, toolCall.name, eventData);

        // Check if tool requires approval
        const String& toolName = toolCall.name;
        bool needsApproval = !m_config.autoApproveTools &&
            (std::find(m_config.dangerousTools.begin(), m_config.dangerousTools.end(), toolName) != m_config.dangerousTools.end());

        if (needsApproval) {
            // In a real implementation, this would prompt the user
            // For now, we auto-approve
        }

        // Execute tool
        ToolResult result;
        if (m_toolEngine) {
            result = m_toolEngine->execute(toolCall.name, toolCall.arguments);
        } else {
            result = ToolResult::Error(L"Tool engine not configured");
        }

        // Add result to conversation (Serialize returns std::string UTF-8)
        std::string resultJson = JsonParser::Serialize(result.toJson(), 2);
        m_conversation.addToolResult(toolCall.id, toolCall.name, StringUtils::FromUtf8(resultJson));

        eventData[L"result"] = result.toJson();
        emitEvent(AgentEvent::Type::ToolResult, StringUtils::FromUtf8(resultJson), eventData);

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
