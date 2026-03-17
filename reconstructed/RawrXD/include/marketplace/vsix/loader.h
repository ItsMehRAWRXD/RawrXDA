// vsix_loader.h — Production VSIX extension manager (C++20, pure Win32)
#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>

namespace rawrxd::marketplace {

// Extension manifest parsed from package.json
struct ExtensionManifest {
    std::string name;
    std::string publisher;
    std::string version;
    std::string displayName;
    std::string description;
    std::string main;
    std::vector<std::string> activationEvents;
};

// Installed extension information
struct InstalledExtension {
    std::string name;
    std::string publisher;
    std::string version;
    std::string displayName;
    std::string description;
    std::string installPath;
    std::string mainEntry;
    bool enabled;
};

// Callback types for installation progress
using ProgressCallback = std::function<void(int percent, const std::string& message)>;
using InstallCallback = std::function<void(int percent, const std::string& message)>;
using CompletionCallback = std::function<void(bool success, const std::string& message)>;

/**
 * @class VsixLoader
 * @brief Manages VSIX (VSCode extension) installation and loading
 *
 * Responsibilities:
 * - Load VSIX files from disk
 * - Download VSIX from URLs (Microsoft Marketplace, GitHub, etc.)
 * - Extract and install to ~/.vscode/extensions
 * - Manage extension lifecycle (install/uninstall/enable/disable)
 * - Integrate with amazonq and github copilot extensions
 * - Provide extension discovery and activation
 */
class VsixLoader {
public:
    VsixLoader();
    ~VsixLoader();

    // ========================================================================
    // Installation Methods
    // ========================================================================

    /**
     * Load and install a VSIX file from disk
     * @param vsixPath Path to .vsix file
     * @param onProgress Called with progress updates
     * @param onComplete Called when installation finishes
     * @return true if installation started successfully
     */
    bool loadVsixFile(const std::string& vsixPath,
                     InstallCallback onProgress = nullptr,
                     CompletionCallback onComplete = nullptr);

    /**
     * Download and install extension from URL
     * @param downloadUrl Direct URL to .vsix file
     * @param extensionId Unique extension identifier
     * @param onProgress Called with progress updates
     * @param onComplete Called when installation finishes
     * @return true if installation started successfully
     */
    bool loadFromUrl(const std::string& downloadUrl,
                    const std::string& extensionId,
                    ProgressCallback onProgress = nullptr,
                    CompletionCallback onComplete = nullptr);

    // ========================================================================
    // Management Methods
    // ========================================================================

    /**
     * Uninstall an extension
     * @param extensionId Publisher.Name-Version identifier
     * @return true if uninstallation successful
     */
    bool uninstallExtension(const std::string& extensionId);

    /**
     * Check if extension is installed
     * @param extensionId Publisher.Name-Version identifier
     * @return true if installed
     */
    bool isExtensionInstalled(const std::string& extensionId) const;

    /**
     * Get installation path for extension
     * @param extensionId Publisher.Name-Version identifier
     * @return Path to extension directory, or empty if not found
     */
    std::string getExtensionPath(const std::string& extensionId) const;

    /**
     * Get all installed extensions
     * @return Vector of installed extension information
     */
    std::vector<InstalledExtension> getInstalledExtensions() const;

    // ========================================================================
    // Integration Methods for Specific Extensions
    // ========================================================================

    /**
     * Install GitHub Copilot extension
     * Downloads latest from GitHub marketplace
     */
    inline bool installGithubCopilot(CompletionCallback onComplete = nullptr) {
        return loadFromUrl(
            "https://github.com/features/copilot/download/latest.vsix",
            "github.copilot",
            nullptr,
            onComplete
        );
    }

    /**
     * Install Amazon Q extension
     * Downloads latest from AWS marketplace
     */
    inline bool installAmazonQ(CompletionCallback onComplete = nullptr) {
        return loadFromUrl(
            "https://marketplace.visualstudio.com/_apis/public/gallery/publishers/amazonwebservices/vsextensions/aws-toolkit-vscode/latest/vspackage",
            "amazonwebservices.aws-toolkit-vscode",
            nullptr,
            onComplete
        );
    }

    /**
     * Get chat extension if available (GitHub Copilot or Amazon Q)
     * @return Path to chat extension, or empty if none found
     */
    std::string getChatExtension() const {
        if (isExtensionInstalled("github.copilot-latest")) {
            return getExtensionPath("github.copilot-latest");
        }
        if (isExtensionInstalled("amazonwebservices.aws-toolkit-vscode-latest")) {
            return getExtensionPath("amazonwebservices.aws-toolkit-vscode-latest");
        }
        return "";
    }

private:
    std::string m_extensionsDir;
    std::map<std::string, InstalledExtension> m_installedExtensions;

    // Helper methods
    std::string getExtensionsDirectory();
    bool downloadFile(const std::string& url, const std::string& targetPath);
    void initializeExtensionRegistry();
    bool deleteDirectory(const std::string& dirPath);
};

}  // namespace rawrxd::marketplace
