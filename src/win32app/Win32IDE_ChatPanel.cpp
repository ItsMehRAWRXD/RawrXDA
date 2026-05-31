// =============================================================================
// Win32IDE_ChatPanel.cpp — Conversation Session Manager
// =============================================================================
// Production conversation management for the IDE chat panel:
//   - Multi-turn conversation state with role tracking
//   - Context window budget management and automatic truncation
//   - System prompt injection and tool definition accounting
//   - Session persistence (save/load conversations)
//   - Prompt template building for various model formats
//   - Token counting (approximate + exact via tokenizer when loaded)
//   - Conversation export (Markdown, JSON)
//
// Architecture: Decoupled from UI rendering. Produces formatted prompts
//               consumed by inference providers (native, Ollama, etc.)
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "IDELogger.h"
#include "Win32IDE.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <windows.h>


// =============================================================================
// Prompt Format Templates
// =============================================================================
namespace PromptFormat
{

enum class Style : uint8_t
{
    ChatML,   // <|im_start|>role\n...<|im_end|>
    Llama3,   // <|start_header_id|>role<|end_header_id|>\n...<|eot_id|>
    Phi3,     // <|system|>\n...<|end|>\n<|user|>\n...<|end|>\n<|assistant|>
    Mistral,  // [INST] ... [/INST]
    Alpaca,   // ### Instruction:\n...\n### Response:
    Raw       // No template — raw text concatenation
};

struct FormatConfig
{
    Style style = Style::ChatML;
    std::string systemTag;
    std::string userTag;
    std::string assistantTag;
    std::string closeTag;
    std::string endOfTurn;
};

static FormatConfig GetConfig(Style style)
{
    FormatConfig cfg;
    cfg.style = style;

    switch (style)
    {
        case Style::ChatML:
            cfg.systemTag = "<|im_start|>system\n";
            cfg.userTag = "<|im_start|>user\n";
            cfg.assistantTag = "<|im_start|>assistant\n";
            cfg.closeTag = "<|im_end|>\n";
            cfg.endOfTurn = "";
            break;

        case Style::Llama3:
            cfg.systemTag = "<|start_header_id|>system<|end_header_id|>\n\n";
            cfg.userTag = "<|start_header_id|>user<|end_header_id|>\n\n";
            cfg.assistantTag = "<|start_header_id|>assistant<|end_header_id|>\n\n";
            cfg.closeTag = "<|eot_id|>";
            cfg.endOfTurn = "";
            break;

        case Style::Phi3:
            cfg.systemTag = "<|system|>\n";
            cfg.userTag = "<|user|>\n";
            cfg.assistantTag = "<|assistant|>\n";
            cfg.closeTag = "<|end|>\n";
            cfg.endOfTurn = "";
            break;

        case Style::Mistral:
            cfg.systemTag = "";  // Mistral has no system prefix
            cfg.userTag = "[INST] ";
            cfg.assistantTag = "";
            cfg.closeTag = "";
            cfg.endOfTurn = " [/INST]";
            break;

        case Style::Alpaca:
            cfg.systemTag = "";
            cfg.userTag = "### Instruction:\n";
            cfg.assistantTag = "### Response:\n";
            cfg.closeTag = "\n";
            cfg.endOfTurn = "";
            break;

        case Style::Raw:
        default:
            break;
    }
    return cfg;
}
}  // namespace PromptFormat

namespace
{
std::string MakeChatPanelPreview(const std::string& value, size_t maxLen = 120)
{
    std::string preview;
    preview.reserve(std::min(value.size(), maxLen) + 8);
    for (char ch : value)
    {
        if (preview.size() >= maxLen)
            break;
        switch (ch)
        {
            case '\r':
                preview += "\\r";
                break;
            case '\n':
                preview += "\\n";
                break;
            case '\t':
                preview += "\\t";
                break;
            default:
                preview.push_back(ch);
                break;
        }
    }
    if (value.size() > maxLen)
        preview += "...";
    return preview;
}
}  // namespace

// =============================================================================
// ChatMessage — Single conversation message
// =============================================================================
struct ChatMessage
{
    std::string role;  // "system", "user", "assistant", "tool"
    std::string content;
    std::string toolName;      // For tool calls/results
    int approxTokens = 0;      // Approximate token count
    uint64_t timestampMs = 0;  // Message creation time

    ChatMessage() = default;
    ChatMessage(const std::string& r, const std::string& c) : role(r), content(c)
    {
        try
        {
            // Validate the string before processing
            if (c.size() > 0 && c.c_str() != nullptr)
            {
                approxTokens = EstimateTokens(c);
            }
            else
            {
                approxTokens = 0;
            }
        }
        catch (...)
        {
            // If token estimation fails, use safe default
            approxTokens = std::max(1, static_cast<int>(c.size() / 4));
        }
        timestampMs = GetTickCount64();
    }

    static int EstimateTokens(const std::string& text)
    {
        // Approximate: ~4 chars per token for English, ~3 for code
        if (text.empty())
            return 0;

        try
        {
            // Safety check: ensure the string data is accessible and valid
            if (text.size() > 100000000)  // 100MB sanity limit
                return static_cast<int>(text.size() / 4) + 1;

            int nonSpace = 0;
            for (size_t i = 0; i < text.size(); ++i)
            {
                char c = text[i];
                if (c != ' ' && c != '\n' && c != '\t' && c != '\r')
                    nonSpace++;
            }
            float density = static_cast<float>(nonSpace) / static_cast<float>(text.size());

            // Higher density (more code/symbols) = fewer chars per token
            float charsPerToken = (density > 0.85f) ? 3.0f : 4.0f;
            int tokens = static_cast<int>(static_cast<float>(text.size()) / charsPerToken) + 1;
            return std::max(1, tokens);
        }
        catch (...)
        {
            // If any exception occurs during processing, return safe estimate
            return std::max(1, static_cast<int>(text.size() / 4));
        }
    }
};

// =============================================================================
// ConversationSession — Full conversation state
// =============================================================================
class ConversationSession
{
  public:
    struct TokenBudget
    {
        int maxTokens = 0;
        int systemTokens = 0;
        int toolDefTokens = 0;
        int historyTokens = 0;
        int totalUsed = 0;
        int remaining = 0;
        float usagePercent = 0.0f;
        int messagesInContext = 0;
        int messagesTruncated = 0;
    };

    ConversationSession() : m_maxContextTokens(4096), m_promptFormat(PromptFormat::Style::Phi3)
    {
        m_sessionId = GenerateSessionId();
    }

    void Reset()
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_sessionId = GenerateSessionId();
        m_systemPrompt.clear();
        m_systemTokens = 0;
        m_toolDefinitions.clear();
        m_toolDefTokens = 0;
        m_maxContextTokens = 4096;
        m_promptFormat = PromptFormat::Style::Phi3;
        m_messages.clear();
    }

    // Thread-safe wrapper for adding user messages
    void AddUserMessageSafe(const std::string& content) { AddUserMessage(content); }

    // Thread-safe GetTokenBudget
    TokenBudget GetTokenBudgetSafe() const { return GetTokenBudget(); }

    // Thread-safe GetMessages copy
    std::vector<ChatMessage> GetMessagesCopy() const { return GetMessages(); }

    // ---- Configuration ----
    void SetMaxContextTokens(int maxTokens)
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_maxContextTokens = maxTokens;
    }
    int GetMaxContextTokens() const
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        return m_maxContextTokens;
    }
    void SetPromptFormat(PromptFormat::Style style)
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_promptFormat = style;
    }
    PromptFormat::Style GetPromptFormat() const
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        return m_promptFormat;
    }

    void SetSystemPrompt(const std::string& prompt)
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_systemPrompt = prompt;
        m_systemTokens = ChatMessage::EstimateTokens(prompt);
    }
    const std::string& GetSystemPrompt() const { return m_systemPrompt; }

    void SetToolDefinitions(const std::string& toolDefs)
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_toolDefinitions = toolDefs;
        m_toolDefTokens = ChatMessage::EstimateTokens(toolDefs);
    }

    // ---- Message Management ----
    void AddUserMessage(const std::string& content)
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        if (content.empty())
        {
            return;  // Ignore empty messages
        }

        try
        {
            constexpr size_t kMaxChatMessageBytes = 1024u * 1024u;  // 1MB limit per message

            // Validate the input string
            if (content.size() > 100000000)  // 100MB safety check
            {
                LOG_ERROR("[ConversationSession] AddUserMessage rejected: content too large (" +
                          std::to_string(content.size()) + " bytes)");
                return;
            }

            const std::string bounded =
                (content.size() > kMaxChatMessageBytes) ? content.substr(0, kMaxChatMessageBytes) : content;

            LOG_INFO("[ConversationSession] AddUserMessage bytes=" + std::to_string(content.size()) +
                     " bounded_bytes=" + std::to_string(bounded.size()) + " existing_messages=" +
                     std::to_string(m_messages.size()) + " preview='" + MakeChatPanelPreview(bounded) + "'");

            // Add the message safely
            m_messages.emplace_back("user", bounded);

            if (!m_messages.empty())
            {
                LOG_DEBUG("[ConversationSession] AddUserMessage appended approx_tokens=" +
                          std::to_string(m_messages.back().approxTokens) +
                          " total_messages=" + std::to_string(m_messages.size()));
            }

            // Truncate if over budget
            TruncateIfOverBudgetLocked();
        }
        catch (...)
        {
            LOG_ERROR("[ConversationSession] AddUserMessage exception; cleanup may be needed");
        }
    }

    void AddAssistantMessage(const std::string& content)
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        if (content.empty())
        {
            return;  // Ignore empty messages
        }
        constexpr size_t kMaxChatMessageBytes = 1024u * 1024u;
        const std::string bounded =
            (content.size() > kMaxChatMessageBytes) ? content.substr(0, kMaxChatMessageBytes) : content;
        m_messages.emplace_back("assistant", bounded);
    }

    void AddToolResult(const std::string& toolName, const std::string& result)
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        if (result.empty())
        {
            return;  // Ignore empty results
        }
        ChatMessage msg("tool", result);
        msg.toolName = toolName;
        if (msg.approxTokens <= 0)
            msg.approxTokens = 1;
        m_messages.push_back(std::move(msg));
    }

    void ClearHistory()
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_messages.clear();
    }

    std::vector<ChatMessage> GetMessages() const
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        return m_messages;
    }
    size_t GetMessageCount() const
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        return m_messages.size();
    }

    // ---- Token Budget ----
    TokenBudget GetTokenBudget() const
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);

        TokenBudget budget;
        budget.maxTokens = m_maxContextTokens;
        budget.systemTokens = m_systemTokens;
        budget.toolDefTokens = m_toolDefTokens;

        if (m_messages.empty())
        {
            budget.historyTokens = 0;
        }
        else
        {
            for (const auto& msg : m_messages)
            {
                budget.historyTokens += msg.approxTokens;
            }
        }

        budget.totalUsed = budget.systemTokens + budget.toolDefTokens + budget.historyTokens;
        budget.remaining = budget.maxTokens - budget.totalUsed;
        if (budget.remaining < 0)
            budget.remaining = 0;
        budget.usagePercent = budget.maxTokens > 0 ? (float)budget.totalUsed / (float)budget.maxTokens * 100.0f : 0.0f;
        budget.messagesInContext = (int)m_messages.size();
        return budget;
    }

    // ---- Prompt Building ----
    std::string BuildPrompt(const std::string& userMessage)
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);

        auto cfg = PromptFormat::GetConfig(m_promptFormat);
        std::string prompt;
        const int safeMaxTokens = (m_maxContextTokens > 0 && m_maxContextTokens <= 2000000) ? m_maxContextTokens : 4096;
        const size_t reserveHint =
            static_cast<size_t>(safeMaxTokens) * 4u;
        prompt.reserve((std::min)(reserveHint, static_cast<size_t>(8 * 1024 * 1024)));

        // System prompt
        if (!m_systemPrompt.empty())
        {
            prompt += cfg.systemTag;
            prompt += m_systemPrompt;
            if (!m_toolDefinitions.empty())
            {
                prompt += "\n\n## Available Tools\n";
                prompt += m_toolDefinitions;
            }
            prompt += cfg.closeTag;
        }

        // Conversation history — only include messages that fit in budget
        int budgetRemaining = m_maxContextTokens - m_systemTokens - m_toolDefTokens;
        int userMsgTokens = ChatMessage::EstimateTokens(userMessage);
        budgetRemaining -= userMsgTokens;
        budgetRemaining -= 256;  // Reserve for assistant response generation

        // Determine how many history messages fit (walk from most recent)
        int historyStart = 0;
        if (!m_messages.empty())
        {
            int accumTokens = 0;
            for (int i = (int)m_messages.size() - 1; i >= 0; i--)
            {
                accumTokens += m_messages[i].approxTokens;
                if (accumTokens > budgetRemaining)
                {
                    historyStart = i + 1;
                    break;
                }
            }
        }

        // Emit history messages from historyStart
        for (size_t i = historyStart; i < m_messages.size(); i++)
        {
            const auto& msg = m_messages[i];
            if (msg.role == "user")
            {
                prompt += cfg.userTag;
                prompt += msg.content;
                prompt += cfg.endOfTurn;
                prompt += cfg.closeTag;
            }
            else if (msg.role == "assistant")
            {
                prompt += cfg.assistantTag;
                prompt += msg.content;
                prompt += cfg.closeTag;
            }
            else if (msg.role == "tool")
            {
                prompt += cfg.userTag;
                prompt += "[Tool Result: " + msg.toolName + "]\n";
                prompt += msg.content;
                prompt += cfg.endOfTurn;
                prompt += cfg.closeTag;
            }
        }

        // Current user message
        prompt += cfg.userTag;
        prompt += userMessage;
        prompt += cfg.endOfTurn;
        prompt += cfg.closeTag;

        // Open assistant turn
        prompt += cfg.assistantTag;

        return prompt;
    }

    // ---- Auto-detect prompt format from model name ----
    static PromptFormat::Style DetectFormatFromModel(const std::string& modelPath)
    {
        std::string lower = modelPath;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });

        if (lower.find("llama-3") != std::string::npos || lower.find("llama3") != std::string::npos)
        {
            return PromptFormat::Style::Llama3;
        }
        if (lower.find("phi-3") != std::string::npos || lower.find("phi3") != std::string::npos ||
            lower.find("phi-4") != std::string::npos)
        {
            return PromptFormat::Style::Phi3;
        }
        if (lower.find("mistral") != std::string::npos || lower.find("codestral") != std::string::npos ||
            lower.find("ministral") != std::string::npos)
        {
            return PromptFormat::Style::Mistral;
        }
        if (lower.find("qwen") != std::string::npos || lower.find("deepseek") != std::string::npos)
        {
            return PromptFormat::Style::ChatML;
        }
        // Default
        return PromptFormat::Style::ChatML;
    }

    // ---- Session Persistence ----
    bool SaveToFile(const std::string& path) const
    {
        std::vector<ChatMessage> messagesSnapshot;
        std::string sessionId;
        std::string systemPrompt;
        int maxContextTokens = 0;
        PromptFormat::Style promptFormat = PromptFormat::Style::ChatML;
        {
            std::lock_guard<std::mutex> lock(m_messagesMutex);
            messagesSnapshot = m_messages;
            sessionId = m_sessionId;
            systemPrompt = m_systemPrompt;
            maxContextTokens = m_maxContextTokens;
            promptFormat = m_promptFormat;
        }

        std::ofstream f(path, std::ios::binary);
        if (!f.is_open())
            return false;

        // Simple JSON-like format
        f << "{\n";
        f << "  \"session_id\": \"" << EscapeJson(sessionId) << "\",\n";
        f << "  \"max_context_tokens\": " << maxContextTokens << ",\n";
        f << "  \"prompt_format\": " << (int)promptFormat << ",\n";
        f << "  \"system_prompt\": \"" << EscapeJson(systemPrompt) << "\",\n";
        f << "  \"messages\": [\n";

        for (size_t i = 0; i < messagesSnapshot.size(); i++)
        {
            const auto& msg = messagesSnapshot[i];
            f << "    {\"role\": \"" << EscapeJson(msg.role) << "\", \"content\": \"" << EscapeJson(msg.content)
              << "\", \"tokens\": " << msg.approxTokens << ", \"timestamp\": " << msg.timestampMs << "}";
            if (i + 1 < messagesSnapshot.size())
                f << ",";
            f << "\n";
        }

        f << "  ]\n";
        f << "}\n";
        return f.good();
    }

    bool LoadFromFile(const std::string& path)
    {
        // Only load if file exists and is readable
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open())
            return false;

        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (content.empty())
            return false;

        // Minimal JSON parsing for our known format
        std::vector<ChatMessage> parsedMessages;
        std::string parsedSessionId;
        int parsedMaxContextTokens = m_maxContextTokens;

        // Extract session_id
        size_t pos = content.find("\"session_id\":");
        if (pos != std::string::npos)
        {
            parsedSessionId = ExtractJsonString(content, pos);
        }

        // Extract max_context_tokens
        pos = content.find("\"max_context_tokens\":");
        if (pos != std::string::npos)
        {
            parsedMaxContextTokens = ExtractJsonInt(content, pos);
        }

        // Extract messages
        pos = content.find("\"messages\":");
        if (pos != std::string::npos)
        {
            size_t arrStart = content.find('[', pos);
            if (arrStart != std::string::npos)
            {
                size_t cur = arrStart + 1;
                while (cur < content.size())
                {
                    size_t objStart = content.find('{', cur);
                    if (objStart == std::string::npos)
                        break;
                    size_t objEnd = content.find('}', objStart);
                    if (objEnd == std::string::npos)
                        break;

                    std::string obj = content.substr(objStart, objEnd - objStart + 1);

                    ChatMessage msg;
                    size_t rp = obj.find("\"role\":");
                    if (rp != std::string::npos)
                        msg.role = ExtractJsonString(obj, rp);
                    size_t cp = obj.find("\"content\":");
                    if (cp != std::string::npos)
                        msg.content = ExtractJsonString(obj, cp);
                    size_t tp = obj.find("\"tokens\":");
                    if (tp != std::string::npos)
                        msg.approxTokens = ExtractJsonInt(obj, tp);
                    size_t ts = obj.find("\"timestamp\":");
                    if (ts != std::string::npos)
                        msg.timestampMs = (uint64_t)ExtractJsonInt(obj, ts);

                    if (!msg.role.empty())
                    {
                        parsedMessages.push_back(std::move(msg));
                    }

                    cur = objEnd + 1;
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_messagesMutex);
            m_messages = std::move(parsedMessages);
            if (!parsedSessionId.empty())
                m_sessionId = std::move(parsedSessionId);
            if (parsedMaxContextTokens > 0)
                m_maxContextTokens = parsedMaxContextTokens;
        }

        return true;
    }

    // ---- Export ----
    std::string ExportAsMarkdown() const
    {
        std::vector<ChatMessage> messagesSnapshot;
        std::string sessionId;
        std::string systemPrompt;
        {
            std::lock_guard<std::mutex> lock(m_messagesMutex);
            messagesSnapshot = m_messages;
            sessionId = m_sessionId;
            systemPrompt = m_systemPrompt;
        }

        std::ostringstream md;
        md << "# Conversation Export\n\n";
        md << "Session: " << sessionId << "\n";

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf;
        localtime_s(&tm_buf, &time);
        md << "Date: " << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "\n\n";
        md << "---\n\n";

        if (!systemPrompt.empty())
        {
            md << "**System:** " << systemPrompt << "\n\n";
        }

        for (const auto& msg : messagesSnapshot)
        {
            if (msg.role == "user")
            {
                md << "**User:** " << msg.content << "\n\n";
            }
            else if (msg.role == "assistant")
            {
                md << "**Assistant:** " << msg.content << "\n\n";
            }
            else if (msg.role == "tool")
            {
                md << "**Tool [" << msg.toolName << "]:** " << msg.content << "\n\n";
            }
        }

        return md.str();
    }

    const std::string& GetSessionId() const { return m_sessionId; }

  private:
    // Internal: caller must already hold m_messagesMutex (see AddUserMessage).
    void TruncateIfOverBudgetLocked()
    {
        if (m_messages.empty())
            return;

        try
        {
            const int safeMaxContextTokens = m_maxContextTokens > 0 ? m_maxContextTokens : 4096;

            long long totalTokens = static_cast<long long>(m_systemTokens) + static_cast<long long>(m_toolDefTokens);

            // Calculate total tokens with safety checks
            for (size_t idx = 0; idx < m_messages.size(); ++idx)
            {
                // Defensive index access with bounds check
                if (idx >= m_messages.size())
                    break;

                try
                {
                    const auto& msg = m_messages[idx];
                    const int normalizedTokens =
                        (msg.approxTokens > 0 && msg.approxTokens < 1000000) ? msg.approxTokens : 1;
                    totalTokens += static_cast<long long>(normalizedTokens);
                }
                catch (...)
                {
                    // If accessing a message throws, use safe default
                    totalTokens += 1;
                }
            }

            LOG_INFO("[ConversationSession] TruncateIfOverBudget begin total_tokens=" + std::to_string(totalTokens) +
                     " safe_max_tokens=" + std::to_string(safeMaxContextTokens) +
                     " messages=" + std::to_string(m_messages.size()));

            if (totalTokens <= static_cast<long long>(safeMaxContextTokens))
            {
                LOG_DEBUG("[ConversationSession] TruncateIfOverBudget no truncation required");
                return;
            }

            size_t dropCount = 0;
            while (totalTokens > static_cast<long long>(safeMaxContextTokens) && (m_messages.size() - dropCount) > 1)
            {
                // Safety check: ensure we don't access out of bounds
                if (dropCount >= m_messages.size())
                    break;

                try
                {
                    const auto& oldest = m_messages[dropCount];
                    const int oldestTokens = oldest.approxTokens > 0 ? oldest.approxTokens : 1;

                    // Build preview safely without calling potentially unsafe methods on corrupted strings
                    std::string preview;
                    try
                    {
                        if (!oldest.content.empty() && oldest.content.size() < 10000000)  // Sanity check: 10MB max
                        {
                            preview = MakeChatPanelPreview(oldest.content, 80);
                        }
                        else
                        {
                            preview = "[content too large or empty]";
                        }
                    }
                    catch (...)
                    {
                        preview = "[preview unavailable]";
                    }

                    LOG_DEBUG("[ConversationSession] dropping message index=" + std::to_string(dropCount) + " role='" +
                              oldest.role + "' tokens=" + std::to_string(oldestTokens) + " preview='" + preview + "'");
                    totalTokens -= static_cast<long long>(oldestTokens);
                }
                catch (...)
                {
                    // If we can't process this message, just remove it anyway
                    LOG_WARNING("[ConversationSession] error processing message at index=" + std::to_string(dropCount) +
                                " during truncation; removing it safely");
                    totalTokens -= 1;  // Assume 1 token and move on
                }
                ++dropCount;
            }

            if (dropCount > 0 && dropCount < m_messages.size())
            {
                std::vector<ChatMessage> retained;
                retained.reserve(m_messages.size() - dropCount);
                for (size_t i = dropCount; i < m_messages.size(); ++i)
                {
                    retained.push_back(std::move(m_messages[i]));
                }
                m_messages.swap(retained);
                LOG_INFO("[ConversationSession] TruncateIfOverBudget dropped=" + std::to_string(dropCount) +
                         " remaining_messages=" + std::to_string(m_messages.size()) +
                         " remaining_tokens=" + std::to_string(totalTokens));
            }
        }
        catch (...)
        {
            LOG_ERROR("[ConversationSession] FATAL exception in TruncateIfOverBudget; clearing all messages");
            try
            {
                m_messages.clear();
            }
            catch (...)
            {
                // Last-ditch effort to recover
            }
        }
    }

    static std::string GenerateSessionId()
    {
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        std::ostringstream ss;
        ss << "ses_" << std::hex << ms;
        return ss.str();
    }

    static std::string EscapeJson(const std::string& s)
    {
        std::string out;
        out.reserve(s.size() + 16);
        for (char c : s)
        {
            switch (c)
            {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                        out += buf;
                    }
                    else
                    {
                        out += c;
                    }
                    break;
            }
        }
        return out;
    }

    static std::string ExtractJsonString(const std::string& json, size_t keyPos)
    {
        size_t colon = json.find(':', keyPos);
        if (colon == std::string::npos)
            return {};
        size_t quote1 = json.find('"', colon + 1);
        if (quote1 == std::string::npos)
            return {};
        // Find closing quote, handling escapes
        std::string result;
        for (size_t i = quote1 + 1; i < json.size(); i++)
        {
            if (json[i] == '\\' && i + 1 < json.size())
            {
                char next = json[i + 1];
                if (next == '"')
                {
                    result += '"';
                    i++;
                }
                else if (next == '\\')
                {
                    result += '\\';
                    i++;
                }
                else if (next == 'n')
                {
                    result += '\n';
                    i++;
                }
                else if (next == 'r')
                {
                    result += '\r';
                    i++;
                }
                else if (next == 't')
                {
                    result += '\t';
                    i++;
                }
                else
                {
                    result += json[i];
                }
            }
            else if (json[i] == '"')
            {
                break;
            }
            else
            {
                result += json[i];
            }
        }
        return result;
    }

    static int ExtractJsonInt(const std::string& json, size_t keyPos)
    {
        size_t colon = json.find(':', keyPos);
        if (colon == std::string::npos)
            return 0;
        size_t start = colon + 1;
        while (start < json.size() && (json[start] == ' ' || json[start] == '\t'))
            start++;
        std::string numStr;
        while (start < json.size() && (json[start] >= '0' && json[start] <= '9'))
        {
            numStr += json[start++];
        }
        return numStr.empty() ? 0 : std::stoi(numStr);
    }

    mutable std::mutex m_messagesMutex;  // Protects m_messages and message operations
    std::string m_sessionId;
    std::string m_systemPrompt;
    int m_systemTokens = 0;
    std::string m_toolDefinitions;
    int m_toolDefTokens = 0;
    int m_maxContextTokens;
    PromptFormat::Style m_promptFormat;
    std::vector<ChatMessage> m_messages;
};

// =============================================================================
// HandleChatPanel — Feature manifest entry point (backwards compat)
// =============================================================================
void HandleChatPanel(void* idePtr)
{
    // The conversation session is now a class-based API used directly by the IDE.
    // This handler is retained for feature manifest compatibility.
    (void)idePtr;
}

// =============================================================================
// Win32IDE bridge methods — ConversationSession integration
// =============================================================================
// File-static session instance. Lifetime bound to the process.
static ConversationSession s_conversationSession;
static std::recursive_mutex s_conversationSessionMutex;
static bool s_sessionInitialized = false;

void Win32IDE::initConversationSession()
{
    std::lock_guard<std::recursive_mutex> lock(s_conversationSessionMutex);
    s_conversationSession.Reset();
    s_conversationSession.SetSystemPrompt("You are RawrXD AI, a highly capable coding assistant embedded in the "
                                          "RawrXD Sovereign IDE. You help with code generation, debugging, "
                                          "refactoring, and answering technical questions. Be concise and direct.");
    s_conversationSession.SetMaxContextTokens(m_inferenceConfig.contextWindow > 0 ? m_inferenceConfig.contextWindow
                                                                                  : 4096);
    s_sessionInitialized = true;
}

void Win32IDE::conversationAddUser(const std::string& content)
{
    // Validate input before processing
    if (content.empty())
    {
        return;  // Silently ignore empty input
    }

    // If UI history was loaded from disk but the session was never initialized, rebuild before appending.
    std::lock_guard<std::recursive_mutex> lock(s_conversationSessionMutex);
    LOG_INFO("[ConversationSession] conversationAddUser start bytes=" + std::to_string(content.size()) +
             " session_initialized=" + std::string(s_sessionInitialized ? "true" : "false") + " chat_history_size=" +
             std::to_string(m_chatHistory.size()) + " preview='" + MakeChatPanelPreview(content) + "'");
    if (!s_sessionInitialized && !m_chatHistory.empty())
        rehydrateConversationSessionFromChatHistory();
    if (!s_sessionInitialized)
        initConversationSession();

    s_conversationSession.AddUserMessage(content);
    LOG_INFO("[ConversationSession] conversationAddUser complete");
}

void Win32IDE::conversationAddAssistant(const std::string& content)
{
    std::lock_guard<std::recursive_mutex> lock(s_conversationSessionMutex);
    if (!s_sessionInitialized && !m_chatHistory.empty())
        rehydrateConversationSessionFromChatHistory();
    if (!s_sessionInitialized)
        initConversationSession();
    s_conversationSession.AddAssistantMessage(content);
}

void Win32IDE::conversationAddToolResult(const std::string& toolName, const std::string& resultBody)
{
    std::lock_guard<std::recursive_mutex> lock(s_conversationSessionMutex);
    if (!s_sessionInitialized && !m_chatHistory.empty())
        rehydrateConversationSessionFromChatHistory();
    if (!s_sessionInitialized)
        initConversationSession();
    const std::string tn = toolName.empty() ? std::string("tool") : toolName;
    s_conversationSession.AddToolResult(tn, resultBody);
}

std::string Win32IDE::conversationBuildPrompt(const std::string& userMessage)
{
    std::lock_guard<std::recursive_mutex> lock(s_conversationSessionMutex);
    if (!s_sessionInitialized)
        initConversationSession();
    return s_conversationSession.BuildPrompt(userMessage);
}

void Win32IDE::conversationDetectModelFormat(const std::string& modelPath)
{
    std::lock_guard<std::recursive_mutex> lock(s_conversationSessionMutex);
    if (!s_sessionInitialized)
        initConversationSession();
    // Redirected to authoritative load pipeline:
    // Prompt format detection now happens during model load success.
}

void Win32IDE::conversationSetContextWindow(int maxTokens)
{
    std::lock_guard<std::recursive_mutex> lock(s_conversationSessionMutex);
    if (!s_sessionInitialized)
        initConversationSession();
    s_conversationSession.SetMaxContextTokens(maxTokens);
}

void Win32IDE::conversationClear()
{
    std::lock_guard<std::recursive_mutex> lock(s_conversationSessionMutex);
    s_conversationSession.ClearHistory();
    s_sessionInitialized = false;
}

void Win32IDE::rehydrateConversationSessionFromChatHistory()
{
    if (!smokeCopilotChatEnabled())
    {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(s_conversationSessionMutex);
    s_conversationSession.Reset();
    const std::string kBaseSystem = "You are RawrXD AI, a highly capable coding assistant embedded in the "
                                    "RawrXD Sovereign IDE. You help with code generation, debugging, "
                                    "refactoring, and answering technical questions. Be concise and direct.";
    std::string systemAugment;
    for (const auto& turn : m_chatHistory)
    {
        if (turn.first == "system")
        {
            if (!systemAugment.empty())
                systemAugment += "\n\n";
            systemAugment += turn.second;
        }
    }
    if (!systemAugment.empty())
    {
        s_conversationSession.SetSystemPrompt(kBaseSystem + "\n\n--- Persisted workspace context ---\n" +
                                              systemAugment);
    }
    else
    {
        s_conversationSession.SetSystemPrompt(kBaseSystem);
    }
    s_conversationSession.SetMaxContextTokens(m_inferenceConfig.contextWindow > 0 ? m_inferenceConfig.contextWindow
                                                                                  : 4096);
    for (const auto& turn : m_chatHistory)
    {
        if (turn.first == "user")
            s_conversationSession.AddUserMessage(turn.second);
        else if (turn.first == "assistant")
            s_conversationSession.AddAssistantMessage(turn.second);
        else if (turn.first == "tool")
        {
            const std::string& packed = turn.second;
            if (packed.size() > 7 && packed.rfind("[tool:", 0) == 0)
            {
                const size_t close = packed.find(']');
                if (close != std::string::npos && close > 6)
                {
                    std::string tname = packed.substr(6, close - 6);
                    std::string body;
                    size_t bodyStart = close + 1;
                    if (bodyStart < packed.size() && packed[bodyStart] == ' ')
                    {
                        const size_t kopen = packed.find('[', bodyStart);
                        const size_t kclose =
                            (kopen != std::string::npos) ? packed.find(']', kopen + 1) : std::string::npos;
                        if (kopen != std::string::npos && kclose != std::string::npos && kclose > kopen)
                        {
                            bodyStart = kclose + 1;
                        }
                    }
                    while (bodyStart < packed.size() && (packed[bodyStart] == '\n' || packed[bodyStart] == '\r'))
                    {
                        ++bodyStart;
                    }
                    body = packed.substr(bodyStart);
                    s_conversationSession.AddToolResult(tname.empty() ? "tool" : tname, body);
                    continue;
                }
            }
            s_conversationSession.AddToolResult("tool", packed);
        }
    }
    s_sessionInitialized = true;
}
