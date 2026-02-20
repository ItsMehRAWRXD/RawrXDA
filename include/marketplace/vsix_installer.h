#pragma once

// ============================================================================
// VsixInstaller — C++20, no Qt. Handles installation of VSIX packages.
// ============================================================================

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * Manages: downloading VSIX from URLs, extracting, installing to IDE.
 */
class VsixInstaller {
public:
    VsixInstaller() = default;
    ~VsixInstaller();

    void installFromUrl(const std::string& url, const std::string& extensionId);
    void installFromFile(const std::string& filePath);
    bool uninstallExtension(const std::string& extensionId);

    bool isExtensionInstalled(const std::string& extensionId);
    std::string getExtensionInstallPath(const std::string& extensionId);

    using StartedFn = std::function<void(const std::string& extensionId)>;
    using ProgressFn = std::function<void(const std::string& extensionId, int percentage)>;
    using CompletedFn = std::function<void(const std::string& extensionId, bool success)>;
    using ErrorFn = std::function<void(const std::string& extensionId, const std::string& error)>;

    void setOnInstallationStarted(StartedFn fn) { m_onStarted = std::move(fn); }
    void setOnInstallationProgress(ProgressFn fn) { m_onProgress = std::move(fn); }
    void setOnInstallationCompleted(CompletedFn fn) { m_onCompleted = std::move(fn); }
    void setOnInstallationError(ErrorFn fn) { m_onError = std::move(fn); }
    void setOnUninstallCompleted(CompletedFn fn) { m_onUninstallCompleted = std::move(fn); }

private:
    struct InstallationInfo {
        std::string extensionId;
        std::string downloadUrl;
        std::string tempFilePath;
        void* fileHandle = nullptr;  // FILE* or Win32 handle
    };

    void onDownloadFinished();
    void onDownloadProgress(int64_t bytesReceived, int64_t bytesTotal);

    void* m_networkContext = nullptr;
    std::vector<InstallationInfo> m_activeInstallations;

    bool extractVsixPackage(const std::string& vsixPath, const std::string& destination);
    bool activateExtension(const std::string& extensionPath);
    bool deactivateExtension(const std::string& extensionId);
    std::string getExtensionsDirectory();
    void cleanupTempFiles();

    StartedFn m_onStarted;
    ProgressFn m_onProgress;
    CompletedFn m_onCompleted;
    ErrorFn m_onError;
    CompletedFn m_onUninstallCompleted;
};
