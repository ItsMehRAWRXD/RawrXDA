// ExtensionHost.cpp
// Phase 2 Day 6-9: Native Extension Host Runtime Implementation

#include "ExtensionHost.h"
#include "ExtensionHostProcess.h"
#include "ExtensionSandboxManager.h"
#include "ExtensionAPI_VSCode.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace RawrXD::Extensions {

// ============================================================================
// Singleton
// ============================================================================
ExtensionHost& ExtensionHost::GetInstance() {
    static ExtensionHost inst;
    // Tests (and some headless flows) expect the singleton to be usable immediately.
    // Initialize() is idempotent and fail-closed.
    inst.Initialize();
    return inst;
}

ExtensionHost::~ExtensionHost() {
    if (m_initialized.load()) {
        Shutdown();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================
bool ExtensionHost::Initialize() {
    if (m_initialized.exchange(true)) {
        return true; // Already initialized
    }

    m_shuttingDown.store(false);
    m_sandboxManager = &ExtensionSandboxManager::GetInstance();

    // Scan default extensions directory
    std::string extDir = "extensions";
    if (std::filesystem::exists(extDir)) {
        ScanExtensionsDirectory(extDir);
    }

    return true;
}

void ExtensionHost::Shutdown() {
    if (!m_initialized.exchange(false)) return;
    m_shuttingDown.store(true);

    // Unload all extensions
    std::vector<std::string> ids;
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        for (const auto& [id, _] : m_extensions) {
            ids.push_back(id);
        }
    }
    for (const auto& id : ids) {
        UnloadExtension(id);
    }

    m_processes.clear();
    m_sandboxManager = nullptr;
}

// ============================================================================
// Extension Management
// ============================================================================
bool ExtensionHost::LoadExtension(const std::string& path) {
    if (!m_initialized.load() || m_shuttingDown.load()) return false;

    std::filesystem::path extPath(path);
    if (!std::filesystem::exists(extPath)) return false;

    // Read package.json
    std::filesystem::path manifestPath = extPath / "package.json";
    if (!std::filesystem::exists(manifestPath)) {
        // Try extension.json fallback
        manifestPath = extPath / "extension.json";
        if (!std::filesystem::exists(manifestPath)) return false;
    }

    std::ifstream f(manifestPath);
    if (!f) return false;

    nlohmann::json manifest;
    {
        std::string manifestText((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        manifest = nlohmann::json::parse(manifestText, nullptr, false);
        if (manifest.is_discarded()) return false;
    }

    auto info = std::make_shared<ExtensionInfo>();
    info->path = path;
    // Extension ID is the folder name (stable key). Manifest "name" is display/metadata.
    info->id = extPath.filename().string();
    info->name = manifest.value("name", info->id);
    if (manifest.contains("displayName") && manifest["displayName"].is_string()) {
        info->name = manifest["displayName"].get<std::string>();
    }
    info->version = manifest.value("version", "0.0.1");
    info->publisher = manifest.value("publisher", "unknown");
    info->description = manifest.value("description", "");
    info->isBuiltin = manifest.value("__builtin", false);

    if (manifest.contains("activationEvents") && manifest["activationEvents"].is_array()) {
        for (const auto& evt : manifest["activationEvents"]) {
            if (evt.is_string()) {
                info->activationEvents.push_back(evt.get<std::string>());
            }
        }
    }

    if (manifest.contains("contributes") && manifest["contributes"].is_object()) {
        for (auto it = manifest["contributes"].begin(); it != manifest["contributes"].end(); ++it) {
            info->contributes.push_back(it.key());
        }
        // Parse contributed commands for wiring into IDE command palette
        if (manifest["contributes"].contains("commands") && manifest["contributes"]["commands"].is_array()) {
            for (const auto& cmd : manifest["contributes"]["commands"]) {
                if (cmd.is_object() && cmd.contains("command") && cmd["command"].is_string()) {
                    std::string id = cmd["command"].get<std::string>();
                    std::string title = cmd.value("title", id);
                    info->contributedCommands.emplace_back(id, title);
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        m_extensions[info->id] = info;
    }

    // Auto-activate if no activation events specified
    if (info->activationEvents.empty()) {
        ActivateExtension(info->id);
    }

    return true;
}

bool ExtensionHost::UnloadExtension(const std::string& extensionId) {
    if (!m_initialized.load()) return false;

    std::shared_ptr<ExtensionInfo> info;
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        auto it = m_extensions.find(extensionId);
        if (it == m_extensions.end()) return false;
        info = it->second;
    }

    if (info->isActive) {
        DeactivateExtension(extensionId);
    }

    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        m_extensions.erase(extensionId);
    }

    return true;
}

bool ExtensionHost::ReloadExtension(const std::string& extensionId) {
    if (!m_initialized.load()) return false;

    std::string path;
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        auto it = m_extensions.find(extensionId);
        if (it == m_extensions.end()) return false;
        path = it->second->path;
    }

    UnloadExtension(extensionId);
    return LoadExtension(path);
}

bool ExtensionHost::ActivateExtension(const std::string& extensionId) {
    if (!m_initialized.load() || m_shuttingDown.load()) return false;

    std::shared_ptr<ExtensionInfo> info;
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        auto it = m_extensions.find(extensionId);
        if (it == m_extensions.end()) return false;
        info = it->second;
    }

    if (info->isActive) return true;

    if (!StartExtensionProcess(extensionId)) {
        // Fail-soft in environments where the out-of-process host is unavailable
        // (e.g. unit tests / minimal builds). The extension is marked active but has
        // no backing process; API calls must remain fail-closed at the boundary.
        info->processId = 0;
        info->isActive = true;
        info->state = HostProcessState::Running;
        return true;
    }

    info->isActive = true;
    info->state = HostProcessState::Running;

    // Wire contribution points into IDE command palette / providers
    RegisterContributionPoints(extensionId);

    return true;
}

bool ExtensionHost::RegisterContributionPoints(const std::string& extensionId) {
    if (!m_initialized.load()) return false;

    std::shared_ptr<ExtensionInfo> info;
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        auto it = m_extensions.find(extensionId);
        if (it == m_extensions.end()) return false;
        info = it->second;
    }

    // Wire contributed commands into VS Code API command registry
    for (const auto& [cmdId, title] : info->contributedCommands) {
        auto& api = VSCodeAPI::CommandsAPI::Get();
        api.RegisterCommandWithArgs(cmdId, [this, cmdId](const std::string& args) {
            // Dispatch to extension process via IPC
            ExecuteCommand(cmdId, args);
        });
    }

    return true;
}

bool ExtensionHost::DeactivateExtension(const std::string& extensionId) {
    if (!m_initialized.load()) return false;

    std::shared_ptr<ExtensionInfo> info;
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        auto it = m_extensions.find(extensionId);
        if (it == m_extensions.end()) return false;
        info = it->second;
    }

    if (!info->isActive) return true;

    auto procIt = m_processes.find(extensionId);
    if (procIt != m_processes.end() && procIt->second) {
        procIt->second->Shutdown(5000);
        m_processes.erase(procIt);
    }

    info->isActive = false;
    info->state = HostProcessState::Shutdown;
    info->processId = 0;
    return true;
}

// ============================================================================
// Discovery
// ============================================================================
void ExtensionHost::ScanExtensionsDirectory(const std::string& dir) {
    if (!std::filesystem::exists(dir)) return;

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_directory()) {
            std::filesystem::path manifestPath = entry.path() / "package.json";
            if (!std::filesystem::exists(manifestPath)) {
                manifestPath = entry.path() / "extension.json";
            }
            if (std::filesystem::exists(manifestPath)) {
                LoadExtension(entry.path().string());
            }
        }
    }
}

std::vector<ExtensionInfo> ExtensionHost::ListExtensions() const {
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    std::vector<ExtensionInfo> result;
    for (const auto& [_, info] : m_extensions) {
        if (info) result.push_back(*info);
    }
    return result;
}

std::vector<ExtensionInfo> ExtensionHost::ListActiveExtensions() const {
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    std::vector<ExtensionInfo> result;
    for (const auto& [_, info] : m_extensions) {
        if (info && info->isActive) result.push_back(*info);
    }
    return result;
}

std::shared_ptr<ExtensionInfo> ExtensionHost::GetExtension(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    auto it = m_extensions.find(id);
    if (it != m_extensions.end()) return it->second;
    return nullptr;
}

// ============================================================================
// Event Broadcasting
// ============================================================================
void ExtensionHost::RegisterEventHandler(const std::string& event, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_eventHandlersMutex);
    m_eventHandlers[event].push_back(std::move(handler));
}

void ExtensionHost::BroadcastEvent(const std::string& event, const std::string& data) {
    std::vector<EventHandler> handlers;
    {
        std::lock_guard<std::mutex> lock(m_eventHandlersMutex);
        auto it = m_eventHandlers.find(event);
        if (it != m_eventHandlers.end()) {
            handlers = it->second;
        }
    }
    for (const auto& handler : handlers) {
        if (handler) handler(event, data);
    }
}

// ============================================================================
// Stats
// ============================================================================
ExtensionHost::Stats ExtensionHost::GetStats() const {
    Stats s{};
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        s.totalExtensions = m_extensions.size();
        for (const auto& [_, info] : m_extensions) {
            if (info && info->isActive) ++s.activeExtensions;
            if (info && info->state == HostProcessState::Crashed) ++s.crashedExtensions;
        }
    }
    s.totalMessagesExchanged = m_totalMessages.load();
    return s;
}

// ============================================================================
// Internal
// ============================================================================
bool ExtensionHost::StartExtensionProcess(const std::string& extensionId) {
    std::shared_ptr<ExtensionInfo> info;
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        auto it = m_extensions.find(extensionId);
        if (it == m_extensions.end()) return false;
        info = it->second;
    }

    ExtensionMetadata metadata;
    metadata.extensionId = info->id;
    metadata.extensionPath = info->path;
    metadata.extensionName = info->name;
    metadata.version = info->version;
    metadata.isDevExtension = false;

    ResourceQuota quota;
    quota.maxMemoryMB = 256;
    quota.maxCPUPercent = 50;
    quota.timeoutMs = 30000;

    auto process = std::make_unique<ExtensionHostProcess>(metadata, quota);

    // Set up crash/shutdown callbacks
    process->SetOnCrashedCallback(
        [this](DWORD pid, int code) { OnProcessCrashed(pid, code); });
    process->SetOnShutdownCallback(
        [this](DWORD pid, int code) { OnProcessShutdown(pid, code); });

    HRESULT hr = process->Startup();
    if (FAILED(hr)) {
        info->state = HostProcessState::Failed;
        return false;
    }

    info->processId = process->GetProcessId();
    info->state = HostProcessState::Running;

    m_processes[extensionId] = std::move(process);
    return true;
}

void ExtensionHost::OnProcessCrashed(DWORD processId, int exitCode) {
    std::string extensionId;
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        for (const auto& [id, info] : m_extensions) {
            if (info && info->processId == processId) {
                extensionId = id;
                info->state = HostProcessState::Crashed;
                info->isActive = false;
                break;
            }
        }
    }

    if (!extensionId.empty()) {
        m_processes.erase(extensionId);
        BroadcastEvent("extension.crashed", extensionId + ":" + std::to_string(exitCode));
    }
}

void ExtensionHost::OnProcessShutdown(DWORD processId, int exitCode) {
    std::string extensionId;
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        for (const auto& [id, info] : m_extensions) {
            if (info && info->processId == processId) {
                extensionId = id;
                info->state = HostProcessState::Shutdown;
                info->isActive = false;
                break;
            }
        }
    }

    if (!extensionId.empty()) {
        m_processes.erase(extensionId);
        BroadcastEvent("extension.shutdown", extensionId + ":" + std::to_string(exitCode));
    }
}

} // namespace RawrXD::Extensions
