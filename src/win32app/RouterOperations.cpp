// ============================================================================
// RouterOperations.cpp — Implementation
// ============================================================================
#include "RouterOperations.h"
#include <windows.h>
#include <shellapi.h>
#include <chrono>
#include <algorithm>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace RawrXD {
namespace Win32App {

// Singleton instance
static RouterOperations* g_router = nullptr;

RouterOperations& RouterOperations::Instance() {
    if (!g_router) {
        g_router = new RouterOperations();
    }
    return *g_router;
}

RouterOperations::RouterOperations() {
}

RouterOperations::~RouterOperations() {
}

RouterOperations::CommandResult RouterOperations::Execute(const std::string& commandId) {
    std::lock_guard<std::mutex> lock(m_commandsMutex);
    
    auto it = m_commands.find(commandId);
    if (it == m_commands.end()) {
        CommandResult result{false, "Command not found", "COMMAND_NOT_FOUND", -1};
        AddToHistory(commandId, "", result);
        return result;
    }

    const auto& entry = it->second;
    if (!entry.context.enabled) {
        CommandResult result{false, "Command is disabled", "COMMAND_DISABLED", -1};
        AddToHistory(commandId, entry.context.title, result);
        return result;
    }

    CommandResult result;
    try {
        if (entry.handler) {
            result = entry.handler();
        } else {
            result = {false, "No handler for command", "NO_HANDLER", -1};
        }
    } catch (const std::exception& ex) {
        result = {false, std::string("Exception: ") + ex.what(), "EXCEPTION", -1};
    } catch (...) {
        result = {false, "Unknown exception", "UNKNOWN_ERROR", -1};
    }

    AddToHistory(commandId, entry.context.title, result);
    return result;
}

RouterOperations::CommandResult RouterOperations::ExecuteWithArgs(const std::string& commandId, const std::string& args) {
    std::lock_guard<std::mutex> lock(m_commandsMutex);
    
    auto it = m_commands.find(commandId);
    if (it == m_commands.end()) {
        CommandResult result{false, "Command not found", "COMMAND_NOT_FOUND", -1};
        AddToHistory(commandId, "", result);
        return result;
    }

    const auto& entry = it->second;
    if (!entry.context.enabled) {
        CommandResult result{false, "Command is disabled", "COMMAND_DISABLED", -1};
        AddToHistory(commandId, entry.context.title, result);
        return result;
    }

    CommandResult result;
    try {
        if (entry.handlerWithArgs) {
            result = entry.handlerWithArgs(args);
        } else if (entry.handler) {
            result = entry.handler();
        } else {
            result = {false, "No handler for command", "NO_HANDLER", -1};
        }
    } catch (const std::exception& ex) {
        result = {false, std::string("Exception: ") + ex.what(), "EXCEPTION", -1};
    } catch (...) {
        result = {false, "Unknown exception", "UNKNOWN_ERROR", -1};
    }

    AddToHistory(commandId, entry.context.title, result);
    return result;
}

bool RouterOperations::RegisterCommand(const CommandContext& context, CommandHandler handler) {
    std::lock_guard<std::mutex> lock(m_commandsMutex);
    
    CommandEntry entry{};
    entry.context = context;
    entry.handler = handler;
    entry.hasArgs = false;
    
    m_commands[context.id] = entry;
    return true;
}

bool RouterOperations::RegisterCommandWithArgs(const CommandContext& context, CommandHandlerWithArgs handler) {
    std::lock_guard<std::mutex> lock(m_commandsMutex);
    
    CommandEntry entry{};
    entry.context = context;
    entry.handlerWithArgs = handler;
    entry.hasArgs = true;
    
    m_commands[context.id] = entry;
    return true;
}

bool RouterOperations::UnregisterCommand(const std::string& commandId) {
    std::lock_guard<std::mutex> lock(m_commandsMutex);
    
    auto it = m_commands.find(commandId);
    if (it != m_commands.end()) {
        m_commands.erase(it);
        return true;
    }
    return false;
}

bool RouterOperations::IsCommandEnabled(const std::string& commandId) const {
    std::lock_guard<std::mutex> lock(m_commandsMutex);
    
    auto it = m_commands.find(commandId);
    if (it != m_commands.end()) {
        return it->second.context.enabled;
    }
    return false;
}

bool RouterOperations::CommandExists(const std::string& commandId) const {
    std::lock_guard<std::mutex> lock(m_commandsMutex);
    return m_commands.find(commandId) != m_commands.end();
}

std::vector<RouterOperations::CommandContext> RouterOperations::GetAllCommands() const {
    std::lock_guard<std::mutex> lock(m_commandsMutex);
    
    std::vector<CommandContext> commands;
    for (const auto& pair : m_commands) {
        commands.push_back(pair.second.context);
    }
    return commands;
}

RouterOperations::CommandContext RouterOperations::GetCommandContext(const std::string& commandId) const {
    std::lock_guard<std::mutex> lock(m_commandsMutex);
    
    auto it = m_commands.find(commandId);
    if (it != m_commands.end()) {
        return it->second.context;
    }
    return CommandContext{};
}

void RouterOperations::SetCommandEnabled(const std::string& commandId, bool enabled) {
    std::lock_guard<std::mutex> lock(m_commandsMutex);
    
    auto it = m_commands.find(commandId);
    if (it != m_commands.end()) {
        it->second.context.enabled = enabled;
    }
}

std::vector<RouterOperations::HistoryEntry> RouterOperations::GetCommandHistory(int maxEntries) {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    
    std::vector<HistoryEntry> result;
    size_t startIdx = m_history.size() > static_cast<size_t>(maxEntries) 
                      ? m_history.size() - maxEntries 
                      : 0;
    
    for (size_t i = startIdx; i < m_history.size(); ++i) {
        result.push_back(m_history[i]);
    }
    return result;
}

void RouterOperations::ClearHistory() {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    m_history.clear();
}

void RouterOperations::AddToHistory(const std::string& commandId, const std::string& title, const CommandResult& result) {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    
    HistoryEntry entry{};
    entry.commandId = commandId;
    entry.title = title;
    entry.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    entry.result = result;
    
    m_history.push_back(entry);
    
    // Keep history size bounded
    if (m_history.size() > MAX_HISTORY_SIZE) {
        m_history.erase(m_history.begin());
    }
}

bool RouterOperations::ExecuteShellCommand(const std::string& command) {
    // Use Win32 API to execute shell command
    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    
    std::string cmdLine = "cmd.exe /c " + command;
    
    BOOL result = CreateProcessA(nullptr, (LPSTR)cmdLine.c_str(), nullptr, nullptr, 
                                FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    
    if (result) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    return false;
}

bool RouterOperations::LaunchProcess(const std::string& programPath, const std::string& args) {
    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    
    std::string cmdLine = programPath + " " + args;
    
    BOOL result = CreateProcessA(nullptr, (LPSTR)cmdLine.c_str(), nullptr, nullptr,
                                FALSE, 0, nullptr, nullptr, &si, &pi);
    
    if (result) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    return false;
}

std::string RouterOperations::GetClipboardText() {
    std::string text;
    
    if (!OpenClipboard(nullptr)) {
        return text;
    }

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData) {
        char* pszText = static_cast<char*>(GlobalLock(hData));
        if (pszText) {
            text = pszText;
            GlobalUnlock(hData);
        }
    }

    CloseClipboard();
    return text;
}

bool RouterOperations::SetClipboardText(const std::string& text) {
    if (!OpenClipboard(nullptr)) {
        return false;
    }

    EmptyClipboard();

    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!hGlobal) {
        CloseClipboard();
        return false;
    }

    char* pGlobal = static_cast<char*>(GlobalLock(hGlobal));
    strcpy_s(pGlobal, text.size() + 1, text.c_str());
    GlobalUnlock(hGlobal);

    SetClipboardData(CF_TEXT, hGlobal);
    CloseClipboard();
    return true;
}

bool RouterOperations::OpenFileDialog(std::string& outPath) {
    // TODO: Implement file open dialog using Windows API
    return false;
}

bool RouterOperations::SaveFileDialog(std::string& outPath) {
    // TODO: Implement file save dialog using Windows API
    return false;
}

bool RouterOperations::SelectFolderDialog(std::string& outPath) {
    // TODO: Implement folder selection dialog using Windows API
    return false;
}

} // namespace Win32App
} // namespace RawrXD
