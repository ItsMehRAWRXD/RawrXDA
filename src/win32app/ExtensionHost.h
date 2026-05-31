// ExtensionHost.h
// Phase 2 Day 6-9: Native Extension Host Runtime
// Coordinates extension loading, lifecycle, IPC, and VS Code API bridging

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <atomic>
#include "ExtensionHostProcess.h"
#include "ExtensionSandboxManager.h"
#include "ExtensionAPI_VSCode.h"

namespace RawrXD::Extensions {

// Forward declarations
class ExtensionHostProcess;
class ExtensionSandboxManager;
class IPC_Channel;

// ============================================================================
// Extension Info — runtime metadata
// ============================================================================
struct ExtensionInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string publisher;
    std::string description;
    std::string path;
    std::vector<std::string> activationEvents;
    std::vector<std::string> contributes;
    // Parsed contribution points (populated during LoadExtension)
    std::vector<std::pair<std::string, std::string>> contributedCommands; // {id, title}
    bool isActive = false;
    bool isBuiltin = false;
    HostProcessState state = HostProcessState::Uninitialized;
    DWORD processId = 0;
};

// ============================================================================
// ExtensionHost — Main coordinator
// ============================================================================
class ExtensionHost {
public:
    static ExtensionHost& GetInstance();

    // --- Lifecycle ---
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_initialized.load(); }

    // --- Extension Management ---
    bool LoadExtension(const std::string& path);
    bool UnloadExtension(const std::string& extensionId);
    bool ReloadExtension(const std::string& extensionId);
    bool ActivateExtension(const std::string& extensionId);
    bool DeactivateExtension(const std::string& extensionId);

    // --- Contribution Point Wiring ---
    // Parses manifest.contributes and registers commands/providers with the IDE
    bool RegisterContributionPoints(const std::string& extensionId);

    // --- Discovery ---
    void ScanExtensionsDirectory(const std::string& dir);
    std::vector<ExtensionInfo> ListExtensions() const;
    std::vector<ExtensionInfo> ListActiveExtensions() const;
    std::shared_ptr<ExtensionInfo> GetExtension(const std::string& id) const;

    // --- VS Code API Bridge ---
    VSCodeAPI::WorkspaceAPI& GetWorkspaceAPI() { return VSCodeAPI::WorkspaceAPI::Get(); }

    // Command execution
    bool ExecuteCommand(const std::string& commandId, const std::string& args = "");
    std::vector<std::string> GetAvailableCommands() const;

    // Language providers
    bool RegisterCompletionProvider(
        const std::string& extensionId,
        const std::string& language,
        std::shared_ptr<VSCodeAPI::CompletionProvider> provider);
    bool RegisterHoverProvider(
        const std::string& extensionId,
        const std::string& language,
        std::shared_ptr<VSCodeAPI::HoverProvider> provider);
    std::shared_ptr<VSCodeAPI::DiagnosticCollection> CreateDiagnosticCollection(
        const std::string& extensionId,
        const std::string& name);

    // Window API
    void ShowInformationMessage(const std::string& extensionId, const std::string& message);
    void ShowWarningMessage(const std::string& extensionId, const std::string& message);
    void ShowErrorMessage(const std::string& extensionId, const std::string& message);

    // Workspace API
    std::shared_ptr<VSCodeAPI::TextDocument> OpenDocument(const std::string& fileName);
    std::vector<std::string> FindFiles(const std::string& include, const std::string& exclude = "");
    bool SaveAllDocuments();

    // Environment API
    void OpenExternal(const std::string& url);
    std::string GetMachineId() const;
    std::string GetSessionId() const;

    // Extension-to-extension messaging
    bool SendMessageToExtension(
        const std::string& fromExtensionId,
        const std::string& toExtensionId,
        const std::string& message);

    // Activation events
    void FireActivationEvent(const std::string& event, const std::string& data);

    // Telemetry
    void LogExtensionTelemetry(
        const std::string& extensionId,
        const std::string& event,
        const std::map<std::string, std::string>& properties);

    // --- Discovery & Marketplace ---
    bool InstallExtensionFromMarketplace(
        const std::string& publisher,
        const std::string& name,
        const std::string& version);
    bool ValidateExtensionManifest(
        const std::string& manifestPath,
        std::vector<std::string>& outErrors,
        std::vector<std::string>& outWarnings);
    std::vector<std::string> ResolveDependencies(const std::string& extensionId);
    bool CheckForExtensionUpdates(
        std::vector<std::pair<std::string, std::string>>& outUpdates);

    // --- Event Broadcasting ---
    using EventHandler = std::function<void(const std::string& event, const std::string& data)>;
    void RegisterEventHandler(const std::string& event, EventHandler handler);
    void BroadcastEvent(const std::string& event, const std::string& data);

    // --- Security ---
    ExtensionSandboxManager& GetSandboxManager() { return *m_sandboxManager; }
    
    // --- Phase 2 Integration: Direct Access to Proxy for VS Code API Bridging ---
    // THIS IS A TEST COMMENT

    // --- Stats ---
    struct Stats {
        size_t totalExtensions;
        size_t activeExtensions;
        size_t crashedExtensions;
        size_t totalMessagesExchanged;
    };
    Stats GetStats() const;

private:
    ExtensionHost() = default;
    ~ExtensionHost();

    ExtensionHost(const ExtensionHost&) = delete;
    ExtensionHost& operator=(const ExtensionHost&) = delete;

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_shuttingDown{false};

    mutable std::mutex m_extensionsMutex;
    std::map<std::string, std::shared_ptr<ExtensionInfo>> m_extensions;
    std::map<std::string, std::unique_ptr<ExtensionHostProcess>> m_processes;

    ExtensionSandboxManager* m_sandboxManager = nullptr;

    mutable std::mutex m_eventHandlersMutex;
    std::map<std::string, std::vector<EventHandler>> m_eventHandlers;

    std::atomic<size_t> m_totalMessages{0};

    bool StartExtensionProcess(const std::string& extensionId);
    void OnProcessCrashed(DWORD processId, int exitCode);
    void OnProcessShutdown(DWORD processId, int exitCode);
};

} // namespace RawrXD::Extensions
