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

#include "Win32IDE.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
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
        approxTokens = EstimateTokens(c);
        timestampMs = GetTickCount64();
    }

    static int EstimateTokens(const std::string& text)
    {
        // Approximate: ~4 chars per token for English, ~3 for code
        if (text.empty())
            return 0;

        // Count non-space chars vs space chars to estimate code density
        int nonSpace = 0;
        for (char c : text)
        {
            if (c != ' ' && c != '\n' && c != '\t' && c != '\r')
                nonSpace++;
        }
        float density = text.empty() ? 1.0f : (float)nonSpace / (float)text.size();

        // Higher density (more code/symbols) = fewer chars per token
        float charsPerToken = (density > 0.85f) ? 3.0f : 4.0f;
        return static_cast<int>((float)text.size() / charsPerToken) + 1;
    }
};

// =============================================================================
// ConversationSession — Full conversation state
// =============================================================================
class ConversationSession
{
  public:
    ConversationSession() : m_maxContextTokens(4096), m_promptFormat(PromptFormat::Style::Phi3)
    {
        m_sessionId = GenerateSessionId();
    }

    // ---- Configuration ----
    void SetMaxContextTokens(int maxTokens) { m_maxContextTokens = maxTokens; }
    int GetMaxContextTokens() const { return m_maxContextTokens; }
    void SetPromptFormat(PromptFormat::Style style) { m_promptFormat = style; }
    PromptFormat::Style GetPromptFormat() const { return m_promptFormat; }

    void SetSystemPrompt(const std::string& prompt)
    {
        m_systemPrompt = prompt;
        m_systemTokens = ChatMessage::EstimateTokens(prompt);
    }
    const std::string& GetSystemPrompt() const { return m_systemPrompt; }

    void SetToolDefinitions(const std::string& toolDefs)
    {
        m_toolDefinitions = toolDefs;
        m_toolDefTokens = ChatMessage::EstimateTokens(toolDefs);
    }

    // ---- Message Management ----
    void AddUserMessage(const std::string& content)
    {
        m_messages.emplace_back("user", content);
        TruncateIfOverBudget();
    }

    void AddAssistantMessage(const std::string& content) { m_messages.emplace_back("assistant", content); }

    void AddToolResult(const std::string& toolName, const std::string& result)
    {
        ChatMessage msg("tool", result);
        msg.toolName = toolName;
        m_messages.push_back(std::move(msg));
    }

    void ClearHistory() { m_messages.clear(); }

    const std::vector<ChatMessage>& GetMessages() const { return m_messages; }
    size_t GetMessageCount() const { return m_messages.size(); }

    // ---- Token Budget ----
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

    TokenBudget GetTokenBudget() const
    {
        TokenBudget budget;
        budget.maxTokens = m_maxContextTokens;
        budget.systemTokens = m_systemTokens;
        budget.toolDefTokens = m_toolDefTokens;

        for (const auto& msg : m_messages)
        {
            budget.historyTokens += msg.approxTokens;
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
        auto cfg = PromptFormat::GetConfig(m_promptFormat);
        std::string prompt;
        prompt.reserve(m_maxContextTokens * 4);  // Pre-size

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
        std::ofstream f(path, std::ios::binary);
        if (!f.is_open())
            return false;

        // Simple JSON-like format
        f << "{\n";
        f << "  \"session_id\": \"" << EscapeJson(m_sessionId) << "\",\n";
        f << "  \"max_context_tokens\": " << m_maxContextTokens << ",\n";
        f << "  \"prompt_format\": " << (int)m_promptFormat << ",\n";
        f << "  \"system_prompt\": \"" << EscapeJson(m_systemPrompt) << "\",\n";
        f << "  \"messages\": [\n";

        for (size_t i = 0; i < m_messages.size(); i++)
        {
            const auto& msg = m_messages[i];
            f << "    {\"role\": \"" << EscapeJson(msg.role) << "\", \"content\": \"" << EscapeJson(msg.content)
              << "\", \"tokens\": " << msg.approxTokens << ", \"timestamp\": " << msg.timestampMs << "}";
            if (i + 1 < m_messages.size())
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
        m_messages.clear();

        // Extract session_id
        size_t pos = content.find("\"session_id\":");
        if (pos != std::string::npos)
        {
            m_sessionId = ExtractJsonString(content, pos);
        }

        // Extract max_context_tokens
        pos = content.find("\"max_context_tokens\":");
        if (pos != std::string::npos)
        {
            m_maxContextTokens = ExtractJsonInt(content, pos);
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
                        m_messages.push_back(std::move(msg));
                    }

                    cur = objEnd + 1;
                }
            }
        }

        return true;
    }

    // ---- Export ----
    std::string ExportAsMarkdown() const
    {
        std::ostringstream md;
        md << "# Conversation Export\n\n";
        md << "Session: " << m_sessionId << "\n";

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf;
        localtime_s(&tm_buf, &time);
        md << "Date: " << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "\n\n";
        md << "---\n\n";

        if (!m_systemPrompt.empty())
        {
            md << "**System:** " << m_systemPrompt << "\n\n";
        }

        for (const auto& msg : m_messages)
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
    void TruncateIfOverBudget()
    {
        int totalTokens = m_systemTokens + m_toolDefTokens;
        for (const auto& msg : m_messages)
            totalTokens += msg.approxTokens;

        // Drop oldest messages (after system/tool) until within budget
        // Always keep the most recent user message
        while (totalTokens > m_maxContextTokens && m_messages.size() > 1)
        {
            totalTokens -= m_messages.front().approxTokens;
            m_messages.erase(m_messages.begin());
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
static bool s_sessionInitialized = false;

void Win32IDE::initConversationSession()
{
    s_conversationSession = ConversationSession();
    s_conversationSession.SetSystemPrompt("You are RawrXD AI, a highly capable coding assistant embedded in the "
                                          "RawrXD Sovereign IDE. You help with code generation, debugging, "
                                          "refactoring, and answering technical questions. Be concise and direct.");
    s_conversationSession.SetMaxContextTokens(m_inferenceConfig.contextWindow > 0 ? m_inferenceConfig.contextWindow
                                                                                  : 4096);
    s_sessionInitialized = true;
}

void Win32IDE::conversationAddUser(const std::string& content)
{
    // If UI history was loaded from disk but the session was never initialized, rebuild before appending.
    if (!s_sessionInitialized && !m_chatHistory.empty())
        rehydrateConversationSessionFromChatHistory();
    if (!s_sessionInitialized)
        initConversationSession();
    s_conversationSession.AddUserMessage(content);
}

void Win32IDE::conversationAddAssistant(const std::string& content)
{
    if (!s_sessionInitialized && !m_chatHistory.empty())
        rehydrateConversationSessionFromChatHistory();
    if (!s_sessionInitialized)
        initConversationSession();
    s_conversationSession.AddAssistantMessage(content);
}

void Win32IDE::conversationAddToolResult(const std::string& toolName, const std::string& resultBody)
{
    if (!s_sessionInitialized && !m_chatHistory.empty())
        rehydrateConversationSessionFromChatHistory();
    if (!s_sessionInitialized)
        initConversationSession();
    const std::string tn = toolName.empty() ? std::string("tool") : toolName;
    s_conversationSession.AddToolResult(tn, resultBody);
}

std::string Win32IDE::conversationBuildPrompt(const std::string& userMessage)
{
    if (!s_sessionInitialized)
        initConversationSession();
    return s_conversationSession.BuildPrompt(userMessage);
}

void Win32IDE::conversationDetectModelFormat(const std::string& modelPath)
{
    if (!s_sessionInitialized)
        initConversationSession();
    // Redirected to authoritative load pipeline:
    // Prompt format detection now happens during model load success.
}

void Win32IDE::conversationSetContextWindow(int maxTokens)
{
    if (!s_sessionInitialized)
        initConversationSession();
    s_conversationSession.SetMaxContextTokens(maxTokens);
}

void Win32IDE::conversationClear()
{
    s_conversationSession.ClearHistory();
    s_sessionInitialized = false;
}

void Win32IDE::rehydrateConversationSessionFromChatHistory()
{
    s_conversationSession = ConversationSession();
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
