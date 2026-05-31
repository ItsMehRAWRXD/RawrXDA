// ExtensionAPI_VSCode.cpp
// Phase 2 Day 9: VS Code Extension API Implementation

#include "ExtensionAPI_VSCode.h"
#include "ExtensionSandboxManager.h"
#include "ExtensionHost.h"
#include "TitanIPC.h"
#include "IDELogger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <windows.h>
#include <Shlobj.h>
#include <shellapi.h>

#pragma comment(lib, "Shell32.lib")

namespace {

    std::string normalizePathForMatch(std::string path)
    {
        std::transform(path.begin(), path.end(), path.begin(), [](unsigned char ch) {
            if (ch == '\\') {
                return '/';
            }
            return static_cast<char>(std::tolower(ch));
        });
        return path;
    }

    std::regex globToRegex(const std::string& pattern)
    {
        std::string regexText;
        regexText.reserve(pattern.size() * 2);
        regexText += '^';

        const std::string normalized = normalizePathForMatch(pattern);
        for (size_t index = 0; index < normalized.size(); ++index) {
            const char ch = normalized[index];
            if (ch == '*') {
                const bool isDoubleStar = (index + 1 < normalized.size() && normalized[index + 1] == '*');
                if (isDoubleStar) {
                    regexText += ".*";
                    ++index;
                } else {
                    regexText += "[^/]*";
                }
                continue;
            }

            if (ch == '?') {
                regexText += '.';
                continue;
            }

            if (std::string(".^$|()[]{}+\\").find(ch) != std::string::npos) {
                regexText += '\\';
            }
            regexText += ch;
        }

        regexText += '$';
        return std::regex(regexText, std::regex::icase);
    }

    bool matchesGlob(const std::string& value, const std::string& pattern)
    {
        if (pattern.empty()) {
            return true;
        }

        const std::string normalizedPattern = normalizePathForMatch(pattern);
        if (normalizedPattern == "*" || normalizedPattern == "**" || normalizedPattern == "**/*") {
            return true;
        }

        return std::regex_match(normalizePathForMatch(value), globToRegex(normalizedPattern));
    }

} // namespace

namespace RawrXD::Extensions::VSCodeAPI {

    // ============================================================================
    // Uri Implementation
    // ============================================================================

    Uri::Uri(const std::string& scheme, const std::string& path)
        : m_scheme(scheme), m_path(path) {}

    Uri Uri::File(const std::string& path)
    {
        return Uri("file", path);
    }

    Uri Uri::Parse(const std::string& value)
    {
        size_t colonPos = value.find(':');
        if (colonPos == std::string::npos) {
            return Uri("file", value);
        }

        std::string scheme = value.substr(0, colonPos);
        std::string path = value.substr(colonPos + 1);

        // Remove leading slashes
        while (!path.empty() && (path[0] == '/' || path[0] == '\\')) {
            path = path.substr(1);
        }

        return Uri(scheme, path);
    }

    std::string Uri::ToString() const
    {
        return m_scheme + "://" + m_path;
    }

    // ============================================================================
    // TextDocument Implementation
    // ============================================================================

    int TextDocument::GetLineCount() const
    {
        return std::count(m_text.begin(), m_text.end(), '\n') + 1;
    }

    // ============================================================================
    // Progress Implementation
    // ============================================================================

    void Progress::Report(int percentage, const std::string& message)
    {
        LOG_INFO("Progress: " + std::to_string(percentage) + "% - " + message);
    }

    // ============================================================================
    // WorkspaceAPI Implementation
    // ============================================================================

    WorkspaceAPI& WorkspaceAPI::Get()
    {
        static WorkspaceAPI instance;
        return instance;
    }

    std::shared_ptr<TextDocument> WorkspaceAPI::OpenTextDocument(const std::string& fileName)
    {
        auto it = m_openDocuments.find(fileName);
        if (it != m_openDocuments.end()) {
            return it->second;
        }

        // SANDBOX CHECK: Verify read access
        // Note: In a real multi-tenant host, we would retrieve the context for the current extension.
        // For Day 9, we use a placeholder check that will bewired to the active IPC context in Phase 3.
        ExtensionSecurityContext dummyContext; 
        if (!FilesystemAccessControl::CheckReadAccess(dummyContext, fileName)) {
            LOG_WARNING("Sandbox Blocked: OpenTextDocument unauthorized for " + fileName);
            return nullptr;
        }

        // Read file content (fail-soft; empty doc if unreadable)
        auto doc = std::make_shared<TextDocument>();
        doc->m_fileName = fileName;
        doc->m_languageId = "plaintext";

        // Determine language ID from extension
        size_t dotPos = fileName.rfind('.');
        if (dotPos != std::string::npos) {
            std::string ext = fileName.substr(dotPos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == "cpp" || ext == "cc" || ext == "cxx") doc->m_languageId = "cpp";
            else if (ext == "h" || ext == "hpp") doc->m_languageId = "cpp";
            else if (ext == "cs") doc->m_languageId = "csharp";
            else if (ext == "py") doc->m_languageId = "python";
            else if (ext == "js" || ext == "jsx") doc->m_languageId = "javascript";
            else if (ext == "ts" || ext == "tsx") doc->m_languageId = "typescript";
            else if (ext == "json") doc->m_languageId = "json";
        }

        try {
            std::ifstream f(fileName, std::ios::binary);
            if (f.is_open()) {
                doc->m_text.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
                doc->m_isDirty = false;
            } else {
                doc->m_text.clear();
            }
        } catch (...) {
            doc->m_text.clear();
        }

        m_openDocuments[fileName] = doc;
        LOG_INFO("Opened document: " + fileName + " (" + doc->m_languageId + ")");

        return doc;
    }

    std::vector<std::string> WorkspaceAPI::FindFiles(const std::string& include, const std::string& exclude)
    {
        std::vector<std::string> results;

        auto folders = GetWorkspaceFolders();
        for (const auto& folder : folders) {
            std::filesystem::path root(folder.uri.GetPath());
            std::error_code error;
            if (!std::filesystem::exists(root, error)) {
                continue;
            }

            std::filesystem::recursive_directory_iterator it(
                root,
                std::filesystem::directory_options::skip_permission_denied,
                error);
            std::filesystem::recursive_directory_iterator end;

            while (it != end) {
                if (error) {
                    error.clear();
                    it.increment(error);
                    continue;
                }

                const auto& entry = *it;
                if (entry.is_regular_file(error)) {
                    const std::filesystem::path fullPath = entry.path();
                    std::string relativePath;

                    std::filesystem::path relative = std::filesystem::relative(fullPath, root, error);
                    if (!error) {
                        relativePath = relative.generic_string();
                    } else {
                        error.clear();
                        relativePath = fullPath.filename().generic_string();
                    }

                    if (matchesGlob(relativePath, include) && !matchesGlob(relativePath, exclude)) {
                        results.push_back(fullPath.string());
                    }
                }

                it.increment(error);
            }
        }

        std::sort(results.begin(), results.end());
        results.erase(std::unique(results.begin(), results.end()), results.end());

        LOG_INFO("Find files: " + include + (exclude.empty() ? "" : " (exclude: " + exclude + ")") +
                 " -> " + std::to_string(results.size()) + " match(es)");

        return results;
    }

    bool WorkspaceAPI::SaveAll()
    {
        for (auto& pair : m_openDocuments) {
            pair.second->m_isDirty = false;
        }
        LOG_INFO("Saved all documents");
        return true;
    }

    std::string WorkspaceAPI::AsRelativePath(const std::string& pathOrUri)
    {
        // Simple relative path calculation
        size_t lastSlash = pathOrUri.rfind('\\');
        if (lastSlash == std::string::npos) {
            lastSlash = pathOrUri.rfind('/');
        }

        if (lastSlash != std::string::npos) {
            return pathOrUri.substr(lastSlash + 1);
        }

        return pathOrUri;
    }

    std::vector<WorkspaceFolder> WorkspaceAPI::GetWorkspaceFolders() const
    {
        std::vector<WorkspaceFolder> folders;

        // Get current working directory as workspace root
        char cwdBuffer[MAX_PATH];
        if (GetCurrentDirectoryA(MAX_PATH, cwdBuffer)) {
            WorkspaceFolder folder("Workspace", Uri::File(cwdBuffer), 0);
            folders.push_back(folder);
        }

        return folders;
    }

    std::vector<std::string> WorkspaceAPI::GetWorkspaceFolderUris() const
    {
        std::vector<std::string> uris;
        auto folders = GetWorkspaceFolders();
        for (const auto& folder : folders) {
            uris.push_back(folder.uri.ToString());
        }
        return uris;
    }

    // ============================================================================
    // WindowAPI Implementation
    // ============================================================================

    WindowAPI& WindowAPI::Get()
    {
        static WindowAPI instance;
        return instance;
    }

    void WindowAPI::ShowInformationMessage(const std::string& message)
    {
        LOG_INFO("Message: " + message);
        MessageBoxA(nullptr, message.c_str(), "RawrXD Extension", MB_ICONINFORMATION | MB_OK);
    }

    void WindowAPI::ShowWarningMessage(const std::string& message)
    {
        LOG_WARNING("Warning: " + message);
        MessageBoxA(nullptr, message.c_str(), "RawrXD Extension", MB_ICONWARNING | MB_OK);
    }

    void WindowAPI::ShowErrorMessage(const std::string& message)
    {
        LOG_ERROR("Error: " + message);
        MessageBoxA(nullptr, message.c_str(), "RawrXD Extension", MB_ICONERROR | MB_OK);
    }

    void WindowAPI::ShowInputBox(const std::string& prompt, InputBoxCallback callback)
    {
        // In production, would show actual input dialog
        LOG_INFO("Input prompt: " + prompt);
        if (callback) {
            callback("user_input");
        }
    }

    void WindowAPI::ShowQuickPick(
        const std::vector<std::string>& items,
        const std::string& placeHolder,
        QuickPickCallback callback)
    {
        LOG_INFO("Quick pick with " + std::to_string(items.size()) + " items");
        if (callback && !items.empty()) {
            callback(items[0]);
        }
    }

    void WindowAPI::WithProgress(
        int totalWork,
        const std::string& title,
        ProgressCallback callback)
    {
        Progress progress;
        LOG_INFO("Starting progress: " + title);
        callback(progress);
        LOG_INFO("Completed progress: " + title);
    }

    // ============================================================================
    // CommandsAPI Implementation
    // ============================================================================

    CommandsAPI& CommandsAPI::Get()
    {
        static CommandsAPI instance;
        return instance;
    }

    void CommandsAPI::RegisterCommand(
        const std::string& commandId,
        std::function<void()> callback)
    {
        m_commands[commandId] = callback;
        LOG_INFO("Registered command: " + commandId);
    }

    void CommandsAPI::RegisterCommandWithArgs(
        const std::string& commandId,
        std::function<void(const std::string& args)> callback)
    {
        m_commandsWithArgs[commandId] = callback;
        LOG_INFO("Registered command with args: " + commandId);
    }

    void CommandsAPI::ExecuteCommand(const std::string& commandId)
    {
        auto it = m_commands.find(commandId);
        if (it != m_commands.end()) {
            it->second();
            LOG_INFO("Executed command: " + commandId);
        } else {
            LOG_WARNING("Command not found: " + commandId);
        }
    }

    void CommandsAPI::ExecuteCommandWithArgs(const std::string& commandId, const std::string& args)
    {
        auto it = m_commandsWithArgs.find(commandId);
        if (it != m_commandsWithArgs.end()) {
            it->second(args);
            LOG_INFO("Executed command with args: " + commandId);
        } else {
            LOG_WARNING("Command not found: " + commandId);
        }
    }

    std::vector<std::string> CommandsAPI::GetCommands()
    {
        std::vector<std::string> commands;
        for (const auto& pair : m_commands) {
            commands.push_back(pair.first);
        }
        for (const auto& pair : m_commandsWithArgs) {
            commands.push_back(pair.first);
        }
        return commands;
    }

    // ============================================================================
    // LanguagesAPI Implementation
    // ============================================================================

    LanguagesAPI& LanguagesAPI::Get()
    {
        static LanguagesAPI instance;
        return instance;
    }

    std::string LanguagesAPI::Predict(const std::string& prompt, const InferenceOptions& options)
    {
        // Day 9 Bridge: Forward to TitanProxy for host-isolated inference.
        // This allows extensions to use RawrXD's local LLM without loading model weights
        // into the sensitive extension process.
        
        std::string completion, metadata, error;
        bool ok = TitanProxy::instance().submit(
            prompt, 
            options.maxTokens, 
            options.timeoutMs, 
            completion, 
            metadata, 
            error
        );

        if (!ok) {
            LOG_ERROR("LanguagesAPI::Predict failed: " + error);
            return "";
        }

        return completion;
    }

    void LanguagesAPI::RegisterCompletionProvider(
        const std::string& language,
        std::shared_ptr<CompletionProvider> provider)
    {
        m_completionProviders[language] = provider;
        LOG_INFO("Registered completion provider for language: " + language);
    }

    void LanguagesAPI::RegisterHoverProvider(
        const std::string& language,
        std::shared_ptr<HoverProvider> provider)
    {
        m_hoverProviders[language] = provider;
        LOG_INFO("Registered hover provider for language: " + language);
    }

    std::shared_ptr<DiagnosticCollection> LanguagesAPI::CreateDiagnosticCollection(
        const std::string& name)
    {
        auto collection = std::make_shared<DiagnosticCollection>();
        m_diagnosticCollections[name] = collection;
        LOG_INFO("Created diagnostic collection: " + name);
        return collection;
    }

    // ============================================================================
    // DiagnosticCollection Implementation
    // ============================================================================

    void DiagnosticCollection::Set(const std::string& fileName, const std::vector<std::string>& diagnostics)
    {
        m_diagnostics[fileName] = diagnostics;
        LOG_INFO("Set " + std::to_string(diagnostics.size()) + " diagnostics for: " + fileName);
    }

    void DiagnosticCollection::Clear()
    {
        m_diagnostics.clear();
        LOG_INFO("Cleared all diagnostics");
    }

    void DiagnosticCollection::Delete(const std::string& fileName)
    {
        m_diagnostics.erase(fileName);
        LOG_INFO("Deleted diagnostics for: " + fileName);
    }

    // ============================================================================
    // EnvironmentAPI Implementation
    // ============================================================================

    EnvironmentAPI& EnvironmentAPI::Get()
    {
        static EnvironmentAPI instance;
        
        // Initialize machine/session IDs if not already done
        if (instance.m_machineId.empty()) {
            // Get machine GUID from registry
            HKEY hKey;
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
                "SOFTWARE\\Microsoft\\Cryptography", 
                0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                
                char machineGuid[40] = {0};
                DWORD size = sizeof(machineGuid);
                if (RegQueryValueExA(hKey, "MachineGuid", nullptr, nullptr, 
                    (LPBYTE)machineGuid, &size) == ERROR_SUCCESS) {
                    instance.m_machineId = machineGuid;
                }
                RegCloseKey(hKey);
            }

            if (instance.m_machineId.empty()) {
                instance.m_machineId = "default-machine-id";
            }

            // Generate session ID
            SYSTEMTIME st;
            GetSystemTime(&st);
            std::stringstream ss;
            ss << "session-" << std::to_string(st.wHour) 
               << "-" << std::to_string(st.wMinute)
               << "-" << std::to_string(st.wSecond);
            instance.m_sessionId = ss.str();
        }

        return instance;
    }

    void EnvironmentAPI::OpenExternal(const std::string& url)
    {
        // SANDBOX CHECK: Verify network/URL access
        ExtensionSecurityContext dummyContext;
        if (!NetworkAccessControl::CheckURLAccess(dummyContext, url)) {
            LOG_WARNING("Sandbox Blocked: OpenExternal unauthorized for " + url);
            return;
        }

        LOG_INFO("Opening external URL: " + url);
        
        // Use ShellExecute to open URL
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOW);
    }

    void EnvironmentAPI::ClipboardCopy(const std::string& text)
    {
        if (OpenClipboard(nullptr)) {
            HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, text.length() + 1);
            if (hglbCopy != nullptr) {
                memcpy_s(GlobalLock(hglbCopy), GlobalSize(hglbCopy), text.c_str(), text.length() + 1);
                GlobalUnlock(hglbCopy);
                SetClipboardData(CF_TEXT, hglbCopy);
            }
            CloseClipboard();
            LOG_INFO("Copied to clipboard: " + std::to_string(text.length()) + " bytes");
        }
    }

    std::string EnvironmentAPI::ClipboardPaste()
    {
        std::string result;

        if (OpenClipboard(nullptr)) {
            HANDLE hglbPaste = GetClipboardData(CF_TEXT);
            if (hglbPaste != nullptr) {
                LPSTR lpstrPaste = static_cast<LPSTR>(GlobalLock(hglbPaste));
                if (lpstrPaste != nullptr) {
                    result = lpstrPaste;
                    GlobalUnlock(hglbPaste);
                }
            }
            CloseClipboard();
            LOG_INFO("Pasted from clipboard: " + std::to_string(result.length()) + " bytes");
        }

        return result;
    }

    std::string EnvironmentAPI::GetMachineId()
    {
        return Get().m_machineId;
    }

    std::string EnvironmentAPI::GetSessionId()
    {
        return Get().m_sessionId;
    }

} // namespace RawrXD::Extensions::VSCodeAPI
