#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>


#include "nlohmann/json.hpp"

// Forward declare to avoid heavy includes in header
namespace RawrXD
{
namespace Agent
{
class AgentToolHandlers;
}
}  // namespace RawrXD

namespace RawrXD
{
namespace Agentic
{

struct LLMResponse
{
    bool is_tool_call = false;
    std::string tool_name;
    nlohmann::json arguments;
    std::string content;        // For regular text or error messages
    std::string finish_reason;  // "stop", "tool_calls", "length", etc.
};

struct ChatMessage
{
    std::string role;  // "system", "user", "assistant", "tool"
    std::string content;
    std::string name;           // For tool responses: the tool name
    nlohmann::json tool_calls;  // For assistant's tool requests
};

class AgenticChatSession
{
  public:
    AgenticChatSession();
    ~AgenticChatSession();

    // Initialize with workspace context
    void Initialize(const std::string& workspace_root, const std::vector<std::string>& open_files);

    /// Update workspace + open editors without resetting chat history (Copilot-style live context).
    /// Re-runs indexer only when the workspace root changes.
    void RefreshContext(const std::string& workspace_root, const std::vector<std::string>& open_files);

    // Enable/disable function calling mode
    void SetAgenticMode(bool enabled) { m_agenticMode = enabled; }
    bool IsAgenticMode() const { return m_agenticMode; }

    /// Ollama model tag for `/api/chat` (e.g. `llama3.2:latest`). Ignored when using local GGUF via Win32IDE bridge.
    void SetChatModel(std::string ollamaModelTag);
    const std::string& GetChatModel() const { return m_model; }

    // Main entry: Process user message, potentially through multiple tool turns
    // Callbacks are invoked on the calling thread (assumed to be UI thread)
    // For long-running tools, use the async variant below
    void RunTurn(const std::string& user_message,
                 std::function<void(const std::string&)> on_content_chunk,  // Streaming text
                 std::function<void(const std::string&)> on_tool_start,     // "Running search_code..."
                 std::function<void(const std::string&, const std::string&)> on_tool_result,  // name, result summary
                 std::function<void(const std::string&)> on_complete);                        // Final response or error

    // Async variant that doesn't block UI during long tool execution
    void RunTurnAsync(const std::string& user_message, std::function<void(const std::string&)> on_content_chunk,
                      std::function<void(const std::string&)> on_tool_start,
                      std::function<void(const std::string&, const std::string&)> on_tool_result,
                      std::function<void(const std::string&)> on_complete);

    // Clear conversation history
    void Reset();

    // Cancel the in-flight turn (streaming / tool loop)
    void CancelCurrentTurn();

    // Check if currently processing (blocks new sends)
    bool IsProcessing() const { return m_isProcessing.load(std::memory_order_acquire); }

  private:
    LLMResponse ParseOpenAIResponse(const nlohmann::json& response);
    LLMResponse ParseOllamaResponse(const nlohmann::json& response);
    LLMResponse ParseStreamingResponse(const std::string& chunk);

    std::string FormatToolResultForLLM(const std::string& tool_name, const nlohmann::json& result);

    // Tool execution (may be long-running)
    nlohmann::json ExecuteTool(const std::string& name, const nlohmann::json& args);

    // Build messages array for LLM API
    nlohmann::json BuildMessagesPayload();

    // System prompt generation
    std::string BuildSystemPrompt();

    // History management
    void AddToHistory(const ChatMessage& msg);
    void TrimHistory();

  private:
    bool m_agenticMode = false;
    std::atomic<bool> m_isProcessing{false};
    std::string m_workspaceRoot;
    std::vector<std::string> m_openFiles;
    std::vector<ChatMessage> m_history;

    // Configuration
    std::string m_model = "phi3:mini";
    int m_maxToolIterations = 10;    // Safety limit to prevent infinite loops
    int m_maxTurnTimeSeconds = 300;  // 5 minute timeout per turn
    size_t m_maxHistorySize = 20;    // Keep last N messages to prevent context explosion

    // Thread safety for async operations
    std::mutex m_mutex;
    std::atomic<bool> m_cancelled{false};

    // Streaming buffer for partial JSON responses
    std::string m_streamBuffer;

    // Detached-thread lifetime management
    std::atomic<int> m_activeThreads{0};
    std::atomic<bool> m_shuttingDown{false};

    // Optional incremental index monitor lifecycle for this session.
    bool m_repoMonitoringStarted = false;
};

}  // namespace Agentic
}  // namespace RawrXD
