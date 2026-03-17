// ============================================================================
// extension_auto_installer.hpp — Automatic Installation of Priority Extensions
// ============================================================================
// PURPOSE:
//   Automatically installs Amazon Q, GitHub Copilot, and full VS Code marketplace
//   extensions on first run or when user requests full marketplace sync.
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <functional>

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Priority Extension List
// ============================================================================
struct PriorityExtension {
    const char* id;                // "publisher.extensionName"
    const char* displayName;
    const char* category;          // "AI", "Languages", "Themes", etc.
    bool        autoInstall;       // Install on first run
    bool        requiresAuth;      // Requires authentication
};

// Critical AI Extensions
static constexpr PriorityExtension PRIORITY_EXTENSIONS[] = {
    // AI / Copilot Extensions
    {"GitHub.copilot", "GitHub Copilot", "AI", true, true},
    {"GitHub.copilot-chat", "GitHub Copilot Chat", "AI", true, true},
    {"amazonwebservices.amazon-q-vscode", "Amazon Q", "AI", true, true},
    {"Continue.continue", "Continue", "AI", true, false},
    {"TabNine.tabnine-vscode", "TabNine AI", "AI", false, false},
    
    // Language Support
    {"ms-vscode.cpptools", "C/C++ Tools", "Languages", true, false},
    {"ms-python.python", "Python", "Languages", true, false},
    {"rust-lang.rust-analyzer", "Rust Analyzer", "Languages", false, false},
    {"golang.go", "Go", "Languages", false, false},
    {"ms-vscode.csharp", "C#", "Languages", false, false},
    
    // Debuggers
    {"ms-vscode.cpptools-extension-pack", "C++ Extension Pack", "Debuggers", true, false},
    {"ms-python.debugpy", "Python Debugger", "Debuggers", false, false},
    
    // Productivity
    {"eamodio.gitlens", "GitLens", "SCM", false, false},
    {"esbenp.prettier-vscode", "Prettier", "Formatters", false, false},
    {"dbaeumer.vscode-eslint", "ESLint", "Linters", false, false},
    
    // Themes
    {"PKief.material-icon-theme", "Material Icon Theme", "Themes", false, false},
    {"Equinusocio.vsc-material-theme", "Material Theme", "Themes", false, false},
};

constexpr size_t PRIORITY_EXTENSION_COUNT = sizeof(PRIORITY_EXTENSIONS) / sizeof(PriorityExtension);

// ============================================================================
// Installation Result
// ============================================================================
struct AutoInstallResult {
    bool success;
    std::string detail;
    int errorCode;
    int installedCount;
    int failedCount;
    std::vector<std::string> installedIds;
    std::vector<std::string> failedIds;

    static AutoInstallResult ok(int installed) {
        AutoInstallResult r{};
        r.success = true;
        r.installedCount = installed;
        r.failedCount = 0;
        r.errorCode = 0;
        return r;
    }

    static AutoInstallResult error(const std::string& msg, int code = -1) {
        AutoInstallResult r{};
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        r.installedCount = 0;
        r.failedCount = 0;
        return r;
    }
};

// ============================================================================
// Progress Callback
// ============================================================================
struct InstallProgress {
    enum class Stage {
        Querying,
        Downloading,
        Installing,
        Verifying,
        Complete,
        Failed
    };

    Stage stage;
    const char* extensionId;
    int currentIndex;
    int totalExtensions;
    uint64_t bytesDownloaded;
    uint64_t totalBytes;
    const char* detail;
};

using InstallProgressCallback = std::function<void(const InstallProgress&)>;

// ============================================================================
// ExtensionAutoInstaller — Singleton
// ============================================================================
class ExtensionAutoInstaller {
public:
    static ExtensionAutoInstaller& instance();

    // Install all priority extensions marked with autoInstall=true
    AutoInstallResult installPriorityExtensions(InstallProgressCallback callback = nullptr);

    // Install specific extension by ID
    AutoInstallResult installExtension(const std::string& extensionId,
                                        InstallProgressCallback callback = nullptr);

    // Install multiple extensions
    AutoInstallResult installExtensions(const std::vector<std::string>& extensionIds,
                                         InstallProgressCallback callback = nullptr);

    // Sync entire VS Code marketplace catalog (for offline mode)
    AutoInstallResult syncMarketplaceCatalog(int maxExtensions = 5000,
                                              InstallProgressCallback callback = nullptr);

    // Check if first-run auto-install is needed
    bool needsFirstRunInstall() const;

    // Mark first-run installation as complete
    void setFirstRunComplete();

    // Get installation status
    bool isInstalled(const std::string& extensionId) const;
    std::vector<std::string> getInstalledExtensions() const;
    std::vector<std::string> getPendingExtensions() const;

private:
    ExtensionAutoInstaller();
    ~ExtensionAutoInstaller();
    ExtensionAutoInstaller(const ExtensionAutoInstaller&) = delete;
    ExtensionAutoInstaller& operator=(const ExtensionAutoInstaller&) = delete;

    AutoInstallResult installSingleExtension(const std::string& extensionId,
                                              InstallProgressCallback callback);

    void emitProgress(const InstallProgress& progress, InstallProgressCallback callback);
    void saveInstallState();
    void loadInstallState();

    mutable std::mutex mutex_;
    std::atomic<bool> firstRunComplete_;
    std::vector<std::string> installed_;
    std::vector<std::string> pending_;
    std::string installStatePath_;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Install Amazon Q, GitHub Copilot, and other AI extensions
inline AutoInstallResult installAIExtensions(InstallProgressCallback callback = nullptr) {
    std::vector<std::string> aiExtensions;
    for (const auto& ext : PRIORITY_EXTENSIONS) {
        if (std::string(ext.category) == "AI") {
            aiExtensions.push_back(ext.id);
        }
    }
    return ExtensionAutoInstaller::instance().installExtensions(aiExtensions, callback);
}

// Install all language support extensions
inline AutoInstallResult installLanguageExtensions(InstallProgressCallback callback = nullptr) {
    std::vector<std::string> langExtensions;
    for (const auto& ext : PRIORITY_EXTENSIONS) {
        if (std::string(ext.category) == "Languages") {
            langExtensions.push_back(ext.id);
        }
    }
    return ExtensionAutoInstaller::instance().installExtensions(langExtensions, callback);
}

} // namespace Extensions
} // namespace RawrXD
