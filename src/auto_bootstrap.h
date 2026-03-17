#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace fs = std::filesystem;

// Bootstrap status constants
enum class BootstrapStatus {
    OK,
    MISSING_DEPENDENCY,
    DOWNLOAD_FAILED,
    INSTALL_FAILED,
    VERIFICATION_FAILED,
    ENVIRONMENT_ERROR
};

// Dependency information
struct DependencyInfo {
    std::string name;
    std::string version;
    std::string minVersion;
    std::string checkPath;        // Where to check if installed
    std::string checkCommand;     // Command to verify installation
    std::string downloadUrl;      // URL to download from
    std::string installPath;      // Where to install
    bool required;
    bool installed;
    std::string detectedVersion;
};

/**
 * @class AutoBootstrap
 * @brief Automatic dependency detection and installation for RawrXD
 */
class AutoBootstrap {
public:
    AutoBootstrap();
    ~AutoBootstrap();

    // Main bootstrap sequence
    BootstrapStatus bootstrapEnvironment();

    // Individual operations
    BootstrapStatus detectDependencies();
    BootstrapStatus downloadMissingDependencies();
    BootstrapStatus verifyInstallations();
    BootstrapStatus setupEnvironmentVariables();

    // Dependency queries
    bool isDependencyInstalled(const std::string& depName) const;
    std::string getDependencyVersion(const std::string& depName) const;
    std::vector<std::string> getMissingDependencies() const;
    
    // Configuration
    void setDownloadPath(const std::string& path);
    void setInstallPath(const std::string& path);
    void setSkipDownloads(bool skip) { m_skipDownloads = skip; }
    void setVerbose(bool verbose) { m_verbose = verbose; }

    // Status reporting
    const std::map<std::string, DependencyInfo>& getDependencies() const { return m_dependencies; }
    std::string getLastError() const { return m_lastError; }

private:
    // Dependency detection
    void initializeDependencies();
    bool checkGit();
    bool checkCMake();
    bool checkCompiler();
    bool checkPython();
    bool checkPowershell();

    // Download and install (Win32 URLDownloadToFile)
    BootstrapStatus downloadFile(const std::string& url, const std::string& destPath);
    BootstrapStatus installDependency(const DependencyInfo& dep);
    
    // Environment setup
    void setupPath();
    void setupCompilerEnvironment();
    bool setEnvironmentVariable(const std::string& name, const std::string& value);
    std::string getEnvironmentVariable(const std::string& name) const;

    // Utilities
    std::string executeCommand(const std::string& command);
    std::string getFileVersion(const std::string& filePath);
    bool compareVersions(const std::string& detected, const std::string& minimum) const;

    // Member variables
    std::map<std::string, DependencyInfo> m_dependencies;
    std::string m_downloadPath;
    std::string m_installPath;
    std::string m_lastError;
    bool m_skipDownloads;
    bool m_verbose;
};
