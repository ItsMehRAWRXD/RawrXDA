#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>
/* Qt removed */
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

// Forward declaration
namespace CPUInference {
    class CPUInferenceEngine;
}

namespace RawrXD {
namespace AI {

// AI Assistance Modes (mimics Cursor/Copilot behavior)
enum class AssistanceMode {
    InlineComplete,      // Like GitHub Copilot inline suggestions
    ChatMode,            // Like Cursor Chat / Copilot Chat
    CommandMode,         // Command palette AI
    EditMode,            // Inline editing with AI (Cursor's Cmd+K)
    AgentMode            // Autonomous coding agent (like Devin/Cursor Composer)
};

// Model Provider Types
enum class ModelProvider {
    Local_GGUF,          // Local GGUF models via cpu_inference_engine
    Ollama,              // Local Ollama server
    OpenAI_Compatible,   // Any OpenAI API compatible endpoint
    Anthropic,           // Claude API
    Custom               // Custom API endpoint
};

// Code Context for AI
struct CodeContext {
    std::string file_path;
    std::string file_content;
    std::string language;
    int cursor_line;
    int cursor_column;
    std::string selected_text;
    std::vector<std::string> open_files;
    std::map<std::string, std::string> workspace_context;  // File snippets
    std::vector<std::string> recent_edits;
    std::string current_function;
    std::vector<std::string> imported_modules;
    std::string project_type;  // "cpp", "python", "typescript", etc.
};

// AI Suggestion (for inline completions)
struct AISuggestion {
    std::string suggestion_text;
    std::string reasoning;
    float confidence;           // 0.0 - 1.0
    int insert_line;
    int insert_column;
    bool is_multiline;
    std::vector<std::string> alternatives;  // Alternative suggestions
};

// AI Chat Message
struct ChatMessage {
    enum class Role { User, Assistant, System };
    Role role;
    std::string content;
    std::string timestamp;
    std::map<std::string, std::string> metadata;  // e.g., model used, tokens
};

// AI Edit Operation (for Cursor-style inline edits)
struct EditOperation {
    std::string original_text;
    std::string new_text;
    std::string instruction;   // User's edit instruction
    int start_line;
    int end_line;
    float confidence;
};

// Agent Task (for autonomous coding agent)
struct AgentTask {
    enum class Status { Pending, Running, Completed, Failed };
    std::string task_id;
    std::string description;
    std::string plan;          // Multi-step plan
    std::vector<std::string> steps;
    int current_step;
    Status status;
    std::vector<EditOperation> applied_edits;
    std::string error_message;
};

// Model Configuration
struct ModelConfig {
    ModelProvider provider;
    std::string model_name;
    std::string api_endpoint;
    std::string api_key;
    int context_length;
    float temperature;
    int max_tokens;
    std::map<std::string, std::string> custom_params;
};

// Callback types
using SuggestionCallback = std::function<void(const AISuggestion&)>;
using ChatCallback = std::function<void(const ChatMessage&)>;
using EditCallback = std::function<void(const EditOperation&)>;
using AgentCallback = std::function<void(const AgentTask&)>;
using ErrorCallback = std::function<void(const std::string&)>;

/**
 * AI Assistant Engine
 * Provides Cursor/Copilot-like functionality for RawrXD IDE
 */
class AIAssistantEngine {
public:
    AIAssistantEngine();
    ~AIAssistantEngine();

    // Initialization
    bool Initialize(const ModelConfig& config);
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }

    // Model Management
    bool LoadModel(const std::string& model_path);
    bool SwitchModel(const ModelConfig& new_config);
    ModelConfig GetCurrentModel() const { return m_current_config; }
    std::vector<std::string> ListAvailableModels() const;

    // Inline Completion (GitHub Copilot style)
    void RequestInlineCompletion(const CodeContext& context, SuggestionCallback callback);
    void CancelInlineCompletion();
    void AcceptSuggestion(const AISuggestion& suggestion);
    void RejectSuggestion(const AISuggestion& suggestion);

    // Chat Mode (Cursor Chat / Copilot Chat)
    std::string StartChatSession();
    void SendChatMessage(const std::string& session_id, const std::string& message,
                        const CodeContext& context, ChatCallback callback);
    std::vector<ChatMessage> GetChatHistory(const std::string& session_id) const;
    void ClearChatSession(const std::string& session_id);

    // Inline Edit Mode (Cursor Cmd+K style)
    void RequestEdit(const std::string& instruction, const CodeContext& context,
                    EditCallback callback);
    void ApplyEdit(const EditOperation& edit);
    void UndoLastEdit();

    // Agent Mode (Autonomous coding)
    std::string CreateAgentTask(const std::string& task_description,
                                const CodeContext& context);
    void StartAgent(const std::string& task_id, AgentCallback callback);
    void PauseAgent(const std::string& task_id);
    void StopAgent(const std::string& task_id);
    AgentTask GetAgentStatus(const std::string& task_id) const;

    // Code Analysis
    std::string ExplainCode(const std::string& code, const std::string& language);
    std::vector<std::string> SuggestRefactorings(const std::string& code,
                                                  const std::string& language);
    std::string GenerateTests(const std::string& code, const std::string& language);
    std::string GenerateDocumentation(const std::string& code, const std::string& language);
    std::vector<std::string> FindBugs(const std::string& code, const std::string& language);
    std::string OptimizeCode(const std::string& code, const std::string& language);

    // Context Management
    void UpdateWorkspaceContext(const std::map<std::string, std::string>& files);
    void AddRecentEdit(const std::string& edit_description);
    void ClearContext();

    // Error handling
    void SetErrorCallback(ErrorCallback callback) { m_error_callback = callback; }

private:
    // Internal methods
    void ProcessInlineRequest(const CodeContext& context, SuggestionCallback callback);
    void ProcessChatRequest(const std::string& session_id, const std::string& message,
                           const CodeContext& context, ChatCallback callback);
    void ProcessEditRequest(const std::string& instruction, const CodeContext& context,
                           EditCallback callback);
    void ProcessAgentTask(const std::string& task_id);

    std::string BuildPrompt(const CodeContext& context, AssistanceMode mode,
                           const std::string& user_input = "");
    std::string CallModel(const std::string& prompt, int max_tokens = 2048);
    AISuggestion ParseInlineSuggestion(const std::string& model_response,
                                       const CodeContext& context);
    EditOperation ParseEditOperation(const std::string& model_response,
                                     const CodeContext& context);

    // Thread pool for async operations
    void WorkerThread();
    void EnqueueTask(std::function<void()> task);

    // State
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_shutdown_requested;
    ModelConfig m_current_config;
    
    // Model backend (can be GGUF, Ollama, or API)
    std::unique_ptr<class IModelBackend> m_backend;
    
    // Threading
    std::vector<std::thread> m_worker_threads;
    std::queue<std::function<void()>> m_task_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;

    // Chat sessions
    std::map<std::string, std::vector<ChatMessage>> m_chat_sessions;
    std::mutex m_chat_mutex;

    // Agent tasks
    struct AgentControl {
        std::atomic<bool> pause{false};
        std::atomic<bool> stop{false};
        std::mutex mutex;
        std::condition_variable cv;
    };
    std::map<std::string, AgentTask> m_agent_tasks;
    std::map<std::string, std::shared_ptr<AgentControl>> m_agent_controls;
    std::mutex m_agent_mutex;

    // Context
    std::map<std::string, std::string> m_workspace_context;
    std::vector<std::string> m_recent_edits;
    std::mutex m_context_mutex;

    // Edit history
    std::vector<EditOperation> m_edit_history;
    std::mutex m_edit_mutex;

    // Callbacks
    ErrorCallback m_error_callback;

    // Statistics
    size_t m_total_suggestions;
    size_t m_accepted_suggestions;
    size_t m_rejected_suggestions;
    std::atomic<bool> m_cancel_inline{false};
};

/**
 * Model Backend Interface
 * Abstracts different model providers (GGUF, Ollama, APIs)
 */
class IModelBackend {
public:
    virtual ~IModelBackend() = default;
    virtual bool Initialize(const ModelConfig& config) = 0;
    virtual std::string Generate(const std::string& prompt, int max_tokens) = 0;
    virtual bool IsReady() const = 0;
    virtual void Shutdown() = 0;
};

/**
 * Local GGUF Backend
 * Uses cpu_inference_engine for local model inference
 */
class GGUFBackend : public IModelBackend {
public:
    bool Initialize(const ModelConfig& config) override;
    std::string Generate(const std::string& prompt, int max_tokens) override;
    bool IsReady() const override { return m_ready; }
    void Shutdown() override;

private:
    std::unique_ptr<::CPUInference::CPUInferenceEngine> m_engine;
    bool m_ready = false;
};

/**
 * Ollama Backend
 * Connects to local Ollama server
 */
class OllamaBackend : public IModelBackend {
public:
    bool Initialize(const ModelConfig& config) override;
    std::string Generate(const std::string& prompt, int max_tokens) override;
    bool IsReady() const override { return m_ready; }
    void Shutdown() override;

private:
    std::string m_endpoint;
    std::string m_model_name;
    bool m_ready = false;
    std::string SendHTTPRequest(const std::string& json_payload);
};

/**
 * OpenAI Compatible Backend
 * Works with OpenAI, Azure OpenAI, or any compatible API
 */
class OpenAIBackend : public IModelBackend {
public:
    bool Initialize(const ModelConfig& config) override;
    std::string Generate(const std::string& prompt, int max_tokens) override;
    bool IsReady() const override { return m_ready; }
    void Shutdown() override;

private:
    std::string m_endpoint;
    std::string m_api_key;
    std::string m_model_name;
    bool m_ready = false;
    std::string SendHTTPRequest(const std::string& json_payload);
};

} // namespace AI
} // namespace RawrXD
