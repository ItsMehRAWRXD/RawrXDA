// ExtensionAPI_VSCode.h
// Phase 2 Day 9: VS Code Extension API Compatibility
// Minimal but functional API surface for real VS Code extensions

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace RawrXD::Extensions::VSCodeAPI {

    // Forward declarations
    class WorkspaceFolder;
    class TextDocument;
    class Uri;
    class QuickPickItem;
    class Progress;
    class DiagnosticCollection;
    class Hover;
    class CompletionItem;

    // ============================================================================
    // Uri - Represents a uniform resource identifier
    // ============================================================================
    class Uri {
    public:
        static Uri File(const std::string& path);
        static Uri Parse(const std::string& value);
        
        std::string GetScheme() const { return m_scheme; }
        std::string GetPath() const { return m_path; }
        std::string ToString() const;

    private:
        Uri(const std::string& scheme, const std::string& path);
        std::string m_scheme;
        std::string m_path;
    };

    // ============================================================================
    // WorkspaceFolder - Represents a folder in the workspace
    // ============================================================================
    class WorkspaceFolder {
    public:
        std::string name;
        Uri uri;
        int index;

        WorkspaceFolder(const std::string& name_, const Uri& uri_, int index_)
            : name(name_), uri(uri_), index(index_) {}
    };

    // ============================================================================
    // TextDocument - Represents opened text document
    // ============================================================================
    class TextDocument {
    public:
        std::string GetFileName() const { return m_fileName; }
        std::string GetText() const { return m_text; }
        std::string GetLanguageId() const { return m_languageId; }
        int GetLineCount() const;
        bool IsDirty() const { return m_isDirty; }
        bool IsClosed() const { return m_isClosed; }

    private:
        friend class WorkspaceAPI;
        std::string m_fileName;
        std::string m_text;
        std::string m_languageId;
        bool m_isDirty = false;
        bool m_isClosed = false;
    };

    // Callback types
    using ProgressCallback = std::function<void(Progress& progress)>;
    using MessageCallback = std::function<void(const std::string& value)>;
    using QuickPickCallback = std::function<void(const std::string& value)>;
    using InputBoxCallback = std::function<void(const std::string& value)>;

    // ============================================================================
    // Progress - Progress indication helper
    // ============================================================================
    class Progress {
    public:
        void Report(int percentage, const std::string& message);
    };

    // ============================================================================
    // Workspace API
    // ============================================================================
    class WorkspaceAPI {
    public:
        static WorkspaceAPI& Get();

        // File operations
        std::shared_ptr<TextDocument> OpenTextDocument(const std::string& fileName);
        std::vector<std::string> FindFiles(const std::string& include, const std::string& exclude = "");
        bool SaveAll();
        std::string AsRelativePath(const std::string& pathOrUri);
        
        // Workspace folder operations
        std::vector<WorkspaceFolder> GetWorkspaceFolders() const;
        std::vector<std::string> GetWorkspaceFolderUris() const;

    private:
        WorkspaceAPI() = default;
        std::map<std::string, std::shared_ptr<TextDocument>> m_openDocuments;
    };

    // ============================================================================
    // Window API
    // ============================================================================
    class WindowAPI {
    public:
        static WindowAPI& Get();

        // UI messaging
        void ShowInformationMessage(const std::string& message);
        void ShowWarningMessage(const std::string& message);
        void ShowErrorMessage(const std::string& message);

        // User input
        void ShowInputBox(const std::string& prompt, InputBoxCallback callback);
        void ShowQuickPick(
            const std::vector<std::string>& items,
            const std::string& placeHolder,
            QuickPickCallback callback);

        // Progress indication
        void WithProgress(
            int totalWork,
            const std::string& title,
            ProgressCallback callback);

    private:
        WindowAPI() = default;
    };

    // ============================================================================
    // Commands API
    // ============================================================================
    class CommandsAPI {
    public:
        static CommandsAPI& Get();

        // Command registration
        void RegisterCommand(
            const std::string& commandId,
            std::function<void()> callback);

        void RegisterCommandWithArgs(
            const std::string& commandId,
            std::function<void(const std::string& args)> callback);

        // Command execution
        void ExecuteCommand(const std::string& commandId);
        void ExecuteCommandWithArgs(const std::string& commandId, const std::string& args);

        // Query
        std::vector<std::string> GetCommands();

    private:
        CommandsAPI() = default;
        std::map<std::string, std::function<void()>> m_commands;
        std::map<std::string, std::function<void(const std::string&)>> m_commandsWithArgs;
    };

    // ============================================================================
    // Languages API
    // ============================================================================
    class CompletionProvider {
    public:
        virtual ~CompletionProvider() = default;
        virtual std::vector<CompletionItem> ProvideCompletions(
            const std::shared_ptr<TextDocument>& document,
            int line,
            int column) = 0;
    };

    class HoverProvider {
    public:
        virtual ~HoverProvider() = default;
        virtual std::shared_ptr<Hover> ProvideHover(
            const std::shared_ptr<TextDocument>& document,
            int line,
            int column) = 0;
    };

    class CompletionItem {
    public:
        std::string label;
        std::string insertText;
        std::string documentation;
        int kind = 0;  // CompletionItemKind
    };

    class Hover {
    public:
        std::string contents;
        Hover(const std::string& content) : contents(content) {}
    };

    class DiagnosticCollection {
    public:
        void Set(const std::string& fileName, const std::vector<std::string>& diagnostics);
        void Clear();
        void Delete(const std::string& fileName);
    
    private:
        std::map<std::string, std::vector<std::string>> m_diagnostics;
    };

    class LanguagesAPI {
    public:
        static LanguagesAPI& Get();

        // Model-driven services
        struct InferenceOptions {
            uint32_t maxTokens = 512;
            uint32_t timeoutMs = 30000;
        };

        std::string Predict(const std::string& prompt, const InferenceOptions& options = InferenceOptions());

        // Provider registration
        void RegisterCompletionProvider(
            const std::string& language,
            std::shared_ptr<CompletionProvider> provider);

        void RegisterHoverProvider(
            const std::string& language,
            std::shared_ptr<HoverProvider> provider);

        // Diagnostic collection
        std::shared_ptr<DiagnosticCollection> CreateDiagnosticCollection(
            const std::string& name);

    private:
        LanguagesAPI() = default;
        std::map<std::string, std::shared_ptr<CompletionProvider>> m_completionProviders;
        std::map<std::string, std::shared_ptr<HoverProvider>> m_hoverProviders;
        std::map<std::string, std::shared_ptr<DiagnosticCollection>> m_diagnosticCollections;
    };

    // ============================================================================
    // Environment API
    // ============================================================================
    class EnvironmentAPI {
    public:
        static EnvironmentAPI& Get();

        // External URL handling
        void OpenExternal(const std::string& url);

        // Clipboard operations
        void ClipboardCopy(const std::string& text);
        std::string ClipboardPaste();

        // Machine/Session identifiers
        std::string GetMachineId();
        std::string GetSessionId();

        // App name
        std::string GetAppName() { return "RawrXD"; }

    private:
        EnvironmentAPI() = default;
        std::string m_machineId;
        std::string m_sessionId;
    };

    // ============================================================================
    // Extension Context
    // ============================================================================
    class ExtensionContext {
    public:
        std::string subscriptionUri;
        std::string storageUri;
        std::string globalStorageUri;
        std::string logUri;
        std::string extensionPath;
        std::string extensionMode;  // "production" or "development"

        WorkspaceAPI* workspace = nullptr;
        WindowAPI* window = nullptr;
        CommandsAPI* commands = nullptr;
        LanguagesAPI* languages = nullptr;
        EnvironmentAPI* env = nullptr;

        ExtensionContext() {
            workspace = &WorkspaceAPI::Get();
            window = &WindowAPI::Get();
            commands = &CommandsAPI::Get();
            languages = &LanguagesAPI::Get();
            env = &EnvironmentAPI::Get();
        }
    };

} // namespace RawrXD::Extensions::VSCodeAPI
