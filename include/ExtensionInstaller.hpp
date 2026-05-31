// ExtensionInstaller.hpp — Download, verify, install, lifecycle manager
//
// Builds on ExtensionLoader to provide:
//   - Download from URL / marketplace
//   - Signature verification
//   - Extract to %APPDATA%\RawrXD\extensions\
//   - Activate / deactivate / uninstall
//   - Progress reporting + cancellation
//
// Thread-safe: all public methods are async-friendly with callback-based progress.

#pragma once
#include "ExtensionManifest.hpp"
#include "../../src/modules/ExtensionLoader.hpp"
#include <windows.h>
#include <wininet.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "wininet.lib")

namespace RawrXD {

// Lifecycle state of an installed extension
enum class ExtensionLifecycleState {
    NotInstalled,
    Downloading,
    DownloadFailed,
    Verifying,
    VerifyFailed,
    Installing,
    InstallFailed,
    Installed,
    Activating,
    Active,
    Deactivating,
    Inactive,
    Uninstalling,
    Uninstalled
};

// Full extension record — combines manifest + lifecycle + runtime
struct ExtensionRecord {
    ExtensionManifest manifest;
    ExtensionLifecycleState state = ExtensionLifecycleState::NotInstalled;
    std::string installPath;           // where it's installed on disk
    std::string downloadUrl;           // source URL
    std::string downloadedArchive;     // temp path to .zip/.tar
    uint64_t installedSize = 0;
    std::chrono::system_clock::time_point installedAt;
    std::chrono::system_clock::time_point lastActivatedAt;
    int activationCount = 0;
    std::string lastError;
};

// Progress callback — called on download / install progress
using ExtensionProgressCallback = std::function<void(const ExtensionDownloadProgress&)>;
using ExtensionLifecycleCallback = std::function<void(const std::string& extId, ExtensionLifecycleState state)>;

class ExtensionInstaller {
public:
    ExtensionInstaller();
    ~ExtensionInstaller();

    // Non-copyable
    ExtensionInstaller(const ExtensionInstaller&) = delete;
    ExtensionInstaller& operator=(const ExtensionInstaller&) = delete;

    // -------------------------------------------------------------------------
    // Discovery
    // -------------------------------------------------------------------------

    // Search marketplace / registry for extensions matching query
    std::vector<ExtensionManifest> search(const std::string& query, int maxResults = 20);

    // Get available updates for installed extensions
    std::vector<ExtensionManifest> checkForUpdates();

    // -------------------------------------------------------------------------
    // Download + Install
    // -------------------------------------------------------------------------

    // Start async download + install from URL
    // Returns task ID for tracking / cancellation
    std::string installFromUrl(const std::string& url,
                                const std::string& expectedId = {},
                                ExtensionProgressCallback onProgress = nullptr,
                                ExtensionLifecycleCallback onStateChange = nullptr);

    // Install from local .zip or directory
    bool installFromLocal(const std::string& path,
                          std::string* outError = nullptr);

    // Cancel an in-progress download/install
    bool cancelInstall(const std::string& taskId);

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    // Activate an installed extension (loads into ExtensionLoader)
    bool activate(const std::string& extId, std::string* outError = nullptr);

    // Deactivate (unload from ExtensionLoader but keep installed)
    bool deactivate(const std::string& extId, std::string* outError = nullptr);

    // Uninstall completely (deactivate + delete files)
    bool uninstall(const std::string& extId, std::string* outError = nullptr);

    // Update an extension (download new version + replace)
    std::string update(const std::string& extId,
                       ExtensionProgressCallback onProgress = nullptr);

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------

    // Get all installed extensions
    std::vector<ExtensionRecord> listInstalled() const;

    // Get single extension record
    std::optional<ExtensionRecord> getRecord(const std::string& extId) const;

    // Get download progress for a task
    std::optional<ExtensionDownloadProgress> getProgress(const std::string& taskId) const;

    // Check if extension is installed
    bool isInstalled(const std::string& extId) const;

    // Check if extension is active
    bool isActive(const std::string& extId) const;

    // -------------------------------------------------------------------------
    // Registry persistence
    // -------------------------------------------------------------------------

    // Save installed extension registry to disk
    bool saveRegistry(std::string* outError = nullptr);

    // Load installed extension registry from disk
    bool loadRegistry(std::string* outError = nullptr);

    // -------------------------------------------------------------------------
    // Tool registry integration
    // -------------------------------------------------------------------------

    // Register all tools from active extensions into the agentic tool registry
    void registerToolsWithAgenticSystem(class AgentToolHandlers* handlers);

    // -------------------------------------------------------------------------
    // Settings
    // -------------------------------------------------------------------------

    void setTrustedSources(const std::vector<ExtensionSource>& sources);
    void setAutoUpdate(bool enabled);
    void setVerifySignatures(bool enabled);

private:
    mutable std::mutex m_mutex;
    std::map<std::string, ExtensionRecord> m_installed;
    std::map<std::string, ExtensionDownloadProgress> m_downloads;
    std::vector<ExtensionSource> m_trustedSources;
    bool m_autoUpdate = true;
    bool m_verifySignatures = true;

    std::unique_ptr<ExtensionLoader> m_loader;
    std::string m_registryPath;
    std::string m_extensionsRoot;
    std::string m_tempDir;

    // Background download worker
    void downloadWorker(const std::string& taskId,
                        const std::string& url,
                        const std::string& expectedId,
                        ExtensionProgressCallback onProgress,
                        ExtensionLifecycleCallback onStateChange);

    // Helpers
    bool verifySignature(const std::string& path, std::string* outError);
    bool extractArchive(const std::string& archivePath,
                        const std::string& destDir,
                        std::string* outError);
    bool validateManifest(const ExtensionManifest& manifest, std::string* outError);
    std::string generateTaskId();
    void setState(const std::string& extId, ExtensionLifecycleState state);
    void setError(const std::string& extId, const std::string& error);
};

} // namespace RawrXD
