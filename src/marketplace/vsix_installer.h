#pragma once
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>

class VsixInstaller {
public:
    struct InstallationInfo {
        std::string extensionId;
        std::string downloadUrl;
        std::string tempFilePath;
    };

    VsixInstaller();
    ~VsixInstaller();

    void installFromUrl(const std::string& url, const std::string& extensionId);
    void installFromFile(const std::string& filePath); // Logic merge for local install
    void installFromFile(const std::string& filePath, const std::string& extensionId);
    bool uninstallExtension(const std::string& extensionId);
    bool isExtensionInstalled(const std::string& extensionId);
    
    // Callbacks
    std::function<void(const std::string&)> installationStarted;
    std::function<void(const std::string&, int)> installationProgress;
    std::function<void(const std::string&, bool)> installationCompleted;
    std::function<void(const std::string&, const std::string&)> installationError;
    std::function<void(const std::string&, bool)> uninstallCompleted;

private:
    std::string getExtensionsDirectory();
    std::string getExtensionInstallPath(const std::string& extensionId);
    bool extractVsixPackage(const std::string& vsixPath, const std::string& destination);
    bool activateExtension(const std::string& extensionPath);
    bool deactivateExtension(const std::string& extensionId);
    void cleanupTempFiles();

    std::vector<InstallationInfo> m_activeInstallations;
    std::mutex m_mutex;
};
