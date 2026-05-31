// Win32IDE_ChatHistoryPersistence.cpp — project-scoped Copilot chat persistence (Cursor/Copilot-style)
// Wires RawrXD::Agent::ProjectChatSession (src/agent/project_scoped_chat.cpp) into Win32IDE chat state.

#include "Win32IDE.h"
#include <commdlg.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>

extern "C"
{
    bool RawrXD_Agent_InitProjectChat(const char* projectPath);
    void RawrXD_Agent_AddChatMessage(int role, const char* content, const char* metadata);
    void RawrXD_Agent_ClearChatHistory();
    bool RawrXD_Agent_SaveChatHistory();
    typedef void (*RawrXD_ProjectChatVisitorFn)(void* userData, int role, const char* content, const char* metadata);
    void RawrXD_Agent_VisitProjectChatMessages(void* userData, RawrXD_ProjectChatVisitorFn visitor);
}

namespace
{
std::string jsonEscapeMinimal(const std::string& s)
{
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s)
    {
        if (c == '"' || c == '\\')
            o += '\\';
        o += static_cast<char>(c);
    }
    return o;
}

// Legacy: metadata was bare tool name. New: JSON {"tool":"...","kind":"call|result|running|..."}
std::string packToolLineLegacy(const char* toolName, const char* body)
{
    std::string name = (toolName && toolName[0]) ? std::string(toolName) : std::string("tool");
    std::string b = body ? std::string(body) : std::string();
    return std::string("[tool:") + name + "]\n" + b;
}

std::string packToolLine(const char* metadata, const char* body)
{
    std::string b = body ? std::string(body) : std::string();
    if (metadata && metadata[0] == '{')
    {
        const std::string m = metadata;
        std::string tool;
        std::string kind = "step";
        const auto take = [&m](const char* key, std::string& out)
        {
            const std::string pat = std::string("\"") + key + "\":\"";
            const size_t p = m.find(pat);
            if (p == std::string::npos)
                return;
            const size_t start = p + pat.size();
            const size_t end = m.find('"', start);
            if (end != std::string::npos && end > start)
                out.assign(m, start, end - start);
        };
        take("tool", tool);
        take("kind", kind);
        if (tool.empty())
            tool = "tool";
        if (kind.empty())
            kind = "step";
        return std::string("[tool:") + tool + "] [" + kind + "]\n" + b;
    }
    return packToolLineLegacy(metadata, b.c_str());
}

static void visitOneMessage(void* user, int role, const char* content, const char* metadata)
{
    auto* hist = reinterpret_cast<std::vector<std::pair<std::string, std::string>>*>(user);
    if (!hist || !content)
    {
        return;
    }
    std::string roleStr = "assistant";
    if (role == 0)
        roleStr = "user";
    else if (role == 2)
        roleStr = "system";
    else if (role == 3)
        roleStr = "tool";

    std::string body;
    if (role == 3)
        body = packToolLine(metadata, content);
    else
        body = content;

    hist->emplace_back(std::move(roleStr), std::move(body));
}
}  // namespace

std::string Win32IDE::workspaceDirectoryForChatPersistence() const
{
    // Match `resolveRawrxdWorkspaceBase()` / RAWRXD_REPO_ROOT so chat storage tracks the IDE workspace root.
    std::filesystem::path base = resolveRawrxdWorkspaceBase();
    if (!base.empty())
    {
        std::string s = base.string();
        if (!s.empty())
            return s;
    }
    if (!m_projectRoot.empty())
        return m_projectRoot;
    if (!m_explorerRootPath.empty())
        return m_explorerRootPath;
    char cwd[MAX_PATH] = {};
    if (GetCurrentDirectoryA(static_cast<DWORD>(sizeof(cwd)), cwd))
        return std::string(cwd);
    return ".";
}

void Win32IDE::ensureProjectScopedChatPersistence()
{
    const std::string root = workspaceDirectoryForChatPersistence();
    if (!m_projectChatPersistenceRoot.empty() && m_projectChatPersistenceRoot == root)
    {
        return;
    }
    RawrXD_Agent_InitProjectChat(root.c_str());
    m_projectChatPersistenceRoot = root;
}

void Win32IDE::reloadPersistedChatHistoryIntoUi()
{
    m_projectChatPersistenceRoot.clear();
    ensureProjectScopedChatPersistence();

    m_chatHistory.clear();
    m_chatToolActions.clear();
    RawrXD_Agent_VisitProjectChatMessages(&m_chatHistory, visitOneMessage);

    if (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput))
    {
        if (!m_chatHistory.empty())
        {
            {
                std::lock_guard<std::mutex> lock(m_outputMutex);
                SetWindowTextW(m_hwndCopilotChatOutput, L"");
            }
            for (const auto& turn : m_chatHistory)
            {
                const std::string body = turn.second.empty() ? std::string(" ") : turn.second;
                appendFormattedChatMessage(turn.first, body);
            }
        }
        else
        {
            updateSecondarySidebarContent();
        }
    }
    else
    {
        updateSecondarySidebarContent();
    }

    rehydrateConversationSessionFromChatHistory();
}

void Win32IDE::persistChatTurnToDisk(const std::string& role, const std::string& content)
{
    if (content.empty())
    {
        return;
    }
    ensureProjectScopedChatPersistence();
    int r = 1;
    if (role == "user")
        r = 0;
    else if (role == "system")
        r = 2;
    else if (role == "tool")
        r = 3;
    RawrXD_Agent_AddChatMessage(r, content.c_str(), nullptr);
    RawrXD_Agent_SaveChatHistory();
}

void Win32IDE::persistChatToolTurnToDisk(const std::string& toolName, const std::string& content)
{
    ensureProjectScopedChatPersistence();
    const std::string tn = toolName.empty() ? std::string("tool") : toolName;
    const std::string body = content.empty() ? std::string("(no output)") : content;
    const std::string meta = std::string("{\"tool\":\"") + jsonEscapeMinimal(tn) + "\",\"kind\":\"result\"}";
    RawrXD_Agent_AddChatMessage(3, body.c_str(), meta.c_str());
    RawrXD_Agent_SaveChatHistory();
}

void Win32IDE::recordToolTurnInChatHistory(const std::string& toolName, const std::string& content,
                                           const std::string& kind)
{
    const std::string tn = toolName.empty() ? std::string("tool") : toolName;
    const std::string k = kind.empty() ? std::string("result") : kind;
    std::string body = content;
    if (body.empty())
    {
        body = (k == "call") ? std::string("{}") : std::string("(no output)");
    }
    conversationAddToolResult(tn, body);
    const std::string packed = std::string("[tool:") + tn + "] [" + k + "]\n" + body;
    m_chatHistory.push_back({"tool", packed});
    ensureProjectScopedChatPersistence();
    const std::string meta =
        std::string("{\"tool\":\"") + jsonEscapeMinimal(tn) + "\",\"kind\":\"" + jsonEscapeMinimal(k) + "\"}";
    RawrXD_Agent_AddChatMessage(3, body.c_str(), meta.c_str());
    RawrXD_Agent_SaveChatHistory();
}

void Win32IDE::clearPersistedChatHistoryOnDisk()
{
    ensureProjectScopedChatPersistence();
    RawrXD_Agent_ClearChatHistory();
    RawrXD_Agent_SaveChatHistory();
}

void Win32IDE::recordPersistedToolTurnOnUiThread(const std::string& toolName, const std::string& resultBody)
{
    const std::string tn = toolName.empty() ? std::string("tool") : toolName;
    const std::string body = resultBody.empty() ? std::string("(no output)") : resultBody;
    const std::string kind = (body == "[started]") ? std::string("running") : std::string("result");
    recordToolTurnInChatHistory(tn, body, kind);
}

extern "C" void RawrXD_ApplyCopilotChatEditLimits(HWND output, HWND input)
{
    constexpr LPARAM kOutputMaxChars = static_cast<LPARAM>(10u * 1024u * 1024u);
    constexpr LPARAM kInputMaxChars = static_cast<LPARAM>(512u * 1024u);
    if (output && IsWindow(output))
        SendMessageW(output, EM_SETLIMITTEXT, 0, kOutputMaxChars);
    if (input && IsWindow(input))
        SendMessageW(input, EM_SETLIMITTEXT, 0, kInputMaxChars);
}

void Win32IDE::applyWorkspaceFolderForChatHistory(const std::string& folderPath)
{
    if (folderPath.empty())
    {
        return;
    }
    m_settings.workingDirectory = folderPath;
    m_explorerRootPath = folderPath;
    m_projectRoot = folderPath;
    SetCurrentDirectoryA(folderPath.c_str());
    if (m_agenticBridge)
    {
        m_agenticBridge->SetWorkspaceRoot(folderPath);
    }
    refreshFileTree();
    m_projectChatPersistenceRoot.clear();
    reloadPersistedChatHistoryIntoUi();
    syncAgenticToolGuardrailsFromWorkspace();
    refreshAgenticChatSessionContext();
}

namespace
{
std::string buildCopilotChatExportMarkdown(const std::vector<std::pair<std::string, std::string>>& history)
{
    std::ostringstream md;
    md << "# RawrXD Copilot Chat Export\n\n";
    md << "_Project-scoped transcript (Cursor / Copilot-style roles)._\n\n";
    for (const auto& turn : history)
    {
        const std::string& role = turn.first;
        const std::string& body = turn.second;
        if (role == "user")
        {
            md << "### User\n\n" << body << "\n\n";
        }
        else if (role == "system")
        {
            md << "### System\n\n" << body << "\n\n";
        }
        else if (role == "tool")
        {
            md << "### Tool\n\n" << body << "\n\n";
        }
        else
        {
            md << "### Assistant\n\n" << body << "\n\n";
        }
    }
    return md.str();
}

std::wstring utf8ToWideClipboard(const std::string& utf8)
{
    if (utf8.empty())
    {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (n <= 0)
    {
        return std::wstring();
    }
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
    return w;
}
}  // namespace

void Win32IDE::exportCopilotChatHistoryToMarkdown()
{
    if (m_chatHistory.empty())
    {
        if (m_hwndMain && IsWindow(m_hwndMain))
        {
            MessageBoxW(m_hwndMain, L"No chat messages to export.", L"Copilot Chat", MB_OK | MB_ICONINFORMATION);
        }
        appendToOutput("[Chat] Export skipped (empty history).\n", "Output", OutputSeverity::Info);
        return;
    }

    char pathBuf[MAX_PATH] = {};
    std::strncpy(pathBuf, "copilot_chat_export.md", sizeof(pathBuf) - 1);
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain && IsWindow(m_hwndMain) ? m_hwndMain : nullptr;
    ofn.lpstrFilter = "Markdown (*.md)\0*.md\0All files\0*.*\0\0";
    ofn.lpstrFile = pathBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "md";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameA(&ofn))
    {
        return;
    }

    const std::string md = buildCopilotChatExportMarkdown(m_chatHistory);
    std::ofstream out(pathBuf, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        appendToOutput(std::string("[Chat] Export failed (open): ") + pathBuf + "\n", "Output", OutputSeverity::Error);
        return;
    }
    out << md;
    out.flush();
    if (!out.good())
    {
        appendToOutput(std::string("[Chat] Export failed (write): ") + pathBuf + "\n", "Output", OutputSeverity::Error);
        return;
    }

    appendToOutput(std::string("[Chat] Exported ") + std::to_string(m_chatHistory.size()) + " turns to " + pathBuf +
                       "\n",
                   "Output", OutputSeverity::Info);
}

void Win32IDE::copyChatHistoryToClipboard()
{
    if (m_chatHistory.empty())
    {
        if (m_hwndMain && IsWindow(m_hwndMain))
        {
            MessageBoxW(m_hwndMain, L"No chat messages to copy yet.", L"Copilot Chat", MB_OK | MB_ICONINFORMATION);
        }
        return;
    }

    const std::string text = buildCopilotChatExportMarkdown(m_chatHistory);
    const std::wstring wtext = utf8ToWideClipboard(text);
    if (wtext.empty() && !text.empty())
    {
        return;
    }
    const size_t byteCount = wtext.size() * sizeof(wchar_t);

    if (!OpenClipboard(m_hwndMain && IsWindow(m_hwndMain) ? m_hwndMain : nullptr))
    {
        return;
    }
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, byteCount);
    if (!hMem)
    {
        CloseClipboard();
        return;
    }
    void* locked = GlobalLock(hMem);
    if (!locked)
    {
        GlobalFree(hMem);
        CloseClipboard();
        return;
    }
    memcpy(locked, wtext.c_str(), byteCount);
    GlobalUnlock(hMem);
    if (!SetClipboardData(CF_UNICODETEXT, hMem))
    {
        GlobalFree(hMem);
    }
    CloseClipboard();

    appendToOutput("[Chat] Copied conversation to clipboard (" + std::to_string(m_chatHistory.size()) + " turns).\n",
                   "Output", OutputSeverity::Info);
}
