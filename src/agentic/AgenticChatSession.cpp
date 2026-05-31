#include "AgenticChatSession.h"

#include "NativeInferenceClient.h"
#include "AgentToolHandlers.h"
#include "core/scoped_instructions_provider.hpp"
#include "indexing/incremental_indexer.hpp"
#include "slash_command_parser.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

using json = nlohmann::json;

namespace RawrXD
{
namespace Agentic
{

namespace
{
std::vector<RawrXD::Agent::ChatMessage> BuildAgentMessages(const json& payload)
{
    std::vector<RawrXD::Agent::ChatMessage> messages;
    if (!payload.is_array())
    {
        return messages;
    }
    messages.reserve(payload.size());
    for (const auto& item : payload)
    {
        if (!item.is_object())
        {
            continue;
        }
        RawrXD::Agent::ChatMessage msg;
        msg.role = item.value("role", std::string());
        msg.content = item.value("content", std::string());
        if (msg.role.empty())
        {
            continue;
        }
        messages.push_back(std::move(msg));
    }
    return messages;
}
}  // namespace

AgenticChatSession::AgenticChatSession() = default;

void AgenticChatSession::SetChatModel(std::string ollamaModelTag)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!ollamaModelTag.empty())
    {
        m_model = std::move(ollamaModelTag);
    }
}

AgenticChatSession::~AgenticChatSession()
{
    m_shuttingDown.store(true, std::memory_order_release);
    m_cancelled.store(true, std::memory_order_release);

    // Wait for detached threads to drain (max 30s safety bail)
    for (int spins = 0; m_activeThreads.load(std::memory_order_acquire) > 0 && spins < 3000; ++spins)
    {
        Sleep(10);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_repoMonitoringStarted)
    {
        RawrXD::Indexing::IncrementalRepositoryIndexer::instance().stopMonitoring();
        m_repoMonitoringStarted = false;
    }
}

void AgenticChatSession::Reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
    m_streamBuffer.clear();
    m_cancelled.store(false);
    m_isProcessing.store(false, std::memory_order_release);
}

void AgenticChatSession::CancelCurrentTurn()
{
    m_cancelled.store(true);
}

void AgenticChatSession::Initialize(const std::string& workspace_root, const std::vector<std::string>& open_files)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_workspaceRoot = workspace_root;
    m_openFiles = open_files;

    RawrXD::Core::ScopedInstructionsProvider::instance().setProjectRoot(workspace_root);
    auto& incrementalIndexer = RawrXD::Indexing::IncrementalRepositoryIndexer::instance();
    incrementalIndexer.initialize(workspace_root);
    if (!m_repoMonitoringStarted)
    {
        incrementalIndexer.startMonitoring();
        m_repoMonitoringStarted = true;
    }

    RawrXD::Agent::ToolGuardrails guardrails;
    guardrails.allowedRoots.push_back(workspace_root);
    guardrails.maxFileSizeBytes = 1024 * 1024;
    guardrails.commandTimeoutMs = 60000;
    guardrails.requireBackupOnWrite = true;
    RawrXD::Agent::AgentToolHandlers::SetGuardrails(guardrails);
}

void AgenticChatSession::RefreshContext(const std::string& workspace_root, const std::vector<std::string>& open_files)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const bool rootChanged = (workspace_root != m_workspaceRoot);
    m_workspaceRoot = workspace_root;
    m_openFiles = open_files;

    RawrXD::Core::ScopedInstructionsProvider::instance().setProjectRoot(workspace_root);
    if (rootChanged)
    {
        auto& incrementalIndexer = RawrXD::Indexing::IncrementalRepositoryIndexer::instance();
        incrementalIndexer.initialize(workspace_root);
        if (!m_repoMonitoringStarted)
        {
            incrementalIndexer.startMonitoring();
            m_repoMonitoringStarted = true;
        }
    }

    RawrXD::Agent::ToolGuardrails guardrails;
    guardrails.allowedRoots.push_back(workspace_root);
    guardrails.maxFileSizeBytes = 1024 * 1024;
    guardrails.commandTimeoutMs = 60000;
    guardrails.requireBackupOnWrite = true;
    RawrXD::Agent::AgentToolHandlers::SetGuardrails(guardrails);
}

void AgenticChatSession::AddToHistory(const ChatMessage& msg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.push_back(msg);
    TrimHistory();
}

void AgenticChatSession::TrimHistory()
{
    if (m_history.size() <= m_maxHistorySize)
    {
        return;
    }
    const size_t start = m_history.size() - m_maxHistorySize;
    m_history.erase(m_history.begin(), m_history.begin() + static_cast<std::ptrdiff_t>(start));
}

std::string AgenticChatSession::BuildSystemPrompt()
{
    std::vector<std::string> scopedSources;
    std::string prompt = RawrXD::Agent::AgentToolHandlers::GetSystemPrompt(
        m_workspaceRoot.empty() ? "." : m_workspaceRoot, m_openFiles, &scopedSources);

    std::ostringstream sessionBlock;
    sessionBlock << "\n\nSession Mode:\n"
                 << "- agentic_mode=" << (m_agenticMode ? "true" : "false") << "\n"
                 << "- processing_async=true\n";
    prompt += sessionBlock.str();
    return prompt;
}

json AgenticChatSession::BuildMessagesPayload()
{
    json messages = json::array();
    messages.push_back({{"role", "system"}, {"content", BuildSystemPrompt()}});
    for (const auto& m : m_history)
    {
        messages.push_back({{"role", m.role}, {"content", m.content}});
    }
    return messages;
}

LLMResponse AgenticChatSession::ParseOpenAIResponse(const json& response)
{
    LLMResponse out;
    if (response.is_object() && response.contains("content") && response["content"].is_string())
    {
        out.content = response["content"].get<std::string>();
    }
    return out;
}

LLMResponse AgenticChatSession::ParseOllamaResponse(const json& response)
{
    LLMResponse out;
    if (response.is_object() && response.contains("response") && response["response"].is_string())
    {
        out.content = response["response"].get<std::string>();
    }
    return out;
}

LLMResponse AgenticChatSession::ParseStreamingResponse(const std::string& chunk)
{
    LLMResponse out;
    out.content = chunk;
    out.finish_reason = "stop";
    return out;
}

std::string AgenticChatSession::FormatToolResultForLLM(const std::string& tool_name, const json& result)
{
    const bool success = result.value("success", false);
    if (!success)
    {
        return "Tool '" + tool_name + "' failed: " + result.value("error", std::string("unknown error"));
    }

    std::string output = result.value("output", std::string());
    constexpr size_t kMax = 8192;
    if (output.size() > kMax)
    {
        output = output.substr(0, kMax) + "\n... [truncated, " + std::to_string(result.value("output", std::string()).size()) + " bytes total]";
    }
    return "Tool '" + tool_name + "' succeeded\n" + output;
}

json AgenticChatSession::ExecuteTool(const std::string& name, const json& args)
{
    auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute(name, args);
    return result.toJson();
}

void AgenticChatSession::RunTurn(const std::string& user_message,
                                 std::function<void(const std::string&)> on_content_chunk,
                                 std::function<void(const std::string&)> on_tool_start,
                                 std::function<void(const std::string&, const std::string&)> on_tool_result,
                                 std::function<void(const std::string&)> on_complete)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_isProcessing.load(std::memory_order_acquire))
        {
            on_complete("Busy processing another request.");
            return;
        }
        m_isProcessing.store(true, std::memory_order_release);
        m_cancelled.store(false);
    }

    auto releaseProcessing = [this]()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_isProcessing.store(false, std::memory_order_release);
    };

    AddToHistory({"user", user_message, "", json()});

    std::string finalText;
    bool executedTool = false;
    if (m_agenticMode && RawrXD::Agentic::SlashCommandParser::IsSlashCommand(user_message))
    {
        const auto parsed = RawrXD::Agentic::SlashCommandParser::Parse(user_message);
        if (!parsed.valid)
        {
            finalText =
                "Slash command parse error: " + parsed.error + "\n\n" + RawrXD::Agentic::SlashCommandParser::GetHelp();
        }
        else if (parsed.command == "help")
        {
            const std::string topic = parsed.args.empty() ? std::string() : parsed.args.front();
            finalText = RawrXD::Agentic::SlashCommandParser::GetHelp(topic);
        }
        else if (parsed.command == "taskframe")
        {
            finalText = RawrXD::Agentic::SlashCommandParser::BuildTaskFramework(parsed.args);
        }
        else if (parsed.command == "harden")
        {
            finalText = RawrXD::Agentic::SlashCommandParser::BuildHardenHarness(parsed.args);
        }
        else if (parsed.command == "language" || parsed.command == "context")
        {
            finalText = RawrXD::Agentic::SlashCommandParser::BuildContextSwitchResponse(parsed.args);
        }
        else
        {
            const json toolCall = parsed.ToToolCall();
            const std::string toolName = toolCall.value("tool", std::string());
            const json toolArgs = toolCall.value("args", json::object());
            if (toolName.empty())
            {
                finalText = "Slash command did not map to a tool.";
            }
            else
            {
                on_tool_start(toolName);
                const json toolResult = ExecuteTool(toolName, toolArgs);
                finalText = FormatToolResultForLLM(toolName, toolResult);
                on_tool_result(toolName, finalText);
                executedTool = true;
            }
        }
    }

    if (m_agenticMode && user_message.rfind("TOOL_CALL:", 0) == 0)
    {
        const size_t start = user_message.find('{');
        if (start != std::string::npos)
        {
            try
            {
                json toolCall = json::parse(user_message.substr(start));
                const std::string toolName = toolCall.value("name", std::string());
                const json args = toolCall.value("arguments", json::object());
                if (!toolName.empty())
                {
                    on_tool_start(toolName);
                    const json toolResult = ExecuteTool(toolName, args);
                    finalText = FormatToolResultForLLM(toolName, toolResult);
                    on_tool_result(toolName, finalText);
                    executedTool = true;
                }
            }
            catch (const std::exception&)
            {
                finalText = "Malformed TOOL_CALL payload.";
            }
        }
    }

    if (finalText.empty())
    {
        if (m_cancelled.load())
        {
            finalText = "[Cancelled]";
            AddToHistory({"assistant", finalText, "", json()});
            on_complete(finalText);
            releaseProcessing();
            return;
        }

        auto payload = BuildMessagesPayload();
        auto messages = BuildAgentMessages(payload);

        RawrXD::Agent::NativeInferenceConfig cfg;
        cfg.chat_model = m_model;
        RawrXD::Agent::NativeInferenceClient client(cfg);

        const json tools = m_agenticMode ? RawrXD::Agent::AgentToolHandlers::GetAllSchemas() : json::array();
        const auto turnStart = std::chrono::steady_clock::now();
        bool reachedToolLimit = false;
        std::string lastToolSummary;

        for (int iteration = 0; iteration < (m_agenticMode ? m_maxToolIterations : 1); ++iteration)
        {
            auto inference = client.ChatSync(messages, tools);

            if (m_cancelled.load())
            {
                finalText = "[Cancelled]";
                AddToHistory({"assistant", finalText, "", json()});
                on_complete(finalText);
                releaseProcessing();
                return;
            }

            if (!inference.success)
            {
                finalText = "Model inference failed: " + inference.error_message;
                break;
            }

            RawrXD::Agent::ChatMessage assistantMsg;
            assistantMsg.role = "assistant";
            assistantMsg.content = inference.response;
            messages.push_back(std::move(assistantMsg));

            if (!m_agenticMode || !inference.has_tool_calls || inference.tool_calls.empty())
            {
                finalText = inference.response.empty() ? std::string("Model returned an empty response.") : inference.response;
                break;
            }

            executedTool = true;
            lastToolSummary.clear();
            for (const auto& toolCall : inference.tool_calls)
            {
                if (m_cancelled.load())
                {
                    finalText = "[Cancelled]";
                    break;
                }

                on_tool_start(toolCall.first);
                const json toolResult = ExecuteTool(toolCall.first, toolCall.second);
                const std::string toolSummary = FormatToolResultForLLM(toolCall.first, toolResult);
                on_tool_result(toolCall.first, toolSummary);

                if (!lastToolSummary.empty())
                {
                    lastToolSummary += "\n\n";
                }
                lastToolSummary += toolSummary;

                RawrXD::Agent::ChatMessage toolMsg;
                toolMsg.role = "tool";
                toolMsg.tool_call_id = toolCall.first;
                toolMsg.content = "[" + toolCall.first + "]\n" + toolResult.dump(2);
                messages.push_back(std::move(toolMsg));
            }

            if (m_cancelled.load())
            {
                break;
            }

            const auto elapsedSeconds =
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - turnStart).count();
            if (elapsedSeconds >= m_maxTurnTimeSeconds)
            {
                finalText = lastToolSummary.empty() ? std::string("Turn timed out while executing tools.")
                                                    : lastToolSummary + "\n\n[Turn timed out before final response]";
                break;
            }

            if (iteration + 1 >= m_maxToolIterations)
            {
                reachedToolLimit = true;
                finalText = lastToolSummary;
                break;
            }
        }

        if (reachedToolLimit)
        {
            if (finalText.empty())
            {
                finalText = "Reached max tool iterations without a final response.";
            }
            else
            {
                finalText += "\n\n[Reached max tool iterations without a final response]";
            }
        }
    }

    AddToHistory({"assistant", finalText, "", json()});
    if (executedTool && on_content_chunk)
    {
        on_content_chunk("\n");
    }
    on_complete(finalText);
    releaseProcessing();
}

void AgenticChatSession::RunTurnAsync(const std::string& user_message,
                                      std::function<void(const std::string&)> on_content_chunk,
                                      std::function<void(const std::string&)> on_tool_start,
                                      std::function<void(const std::string&, const std::string&)> on_tool_result,
                                      std::function<void(const std::string&)> on_complete)
{
    m_activeThreads.fetch_add(1, std::memory_order_acq_rel);
    std::thread worker([this, user_message, on_content_chunk, on_tool_start, on_tool_result, on_complete]()
    {
        if (!m_shuttingDown.load(std::memory_order_acquire))
        {
            RunTurn(user_message, on_content_chunk, on_tool_start, on_tool_result, on_complete);
        }
        m_activeThreads.fetch_sub(1, std::memory_order_acq_rel);
    });
    worker.detach();
}

}  // namespace Agentic
}  // namespace RawrXD
