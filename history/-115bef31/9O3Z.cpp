// ============================================================================
// extension_auto_installer.cpp — Automatic Extension Installation Implementation
// ============================================================================
// NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "extension_auto_installer.hpp"
#include "../win32app/VSCodeMarketplaceAPI.hpp"
#include "../win32app/VSIXInstaller.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Singleton
// ============================================================================
ExtensionAutoInstaller& ExtensionAutoInstaller::instance() {
    static ExtensionAutoInstaller inst;
    return inst;
}

ExtensionAutoInstaller::ExtensionAutoInstaller()
    : firstRunComplete_(false)
{
    // Get %APPDATA%\RawrXD\install_state.json
#ifdef _WIN32
    wchar_t appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        char buf[MAX_PATH * 2] = {};
        WideCharToMultiByte(CP_UTF8, 0, appData, -1, buf, sizeof(buf), nullptr, nullptr);
        installStatePath_ = std::string(buf) + "\\RawrXD\\install_state.json";
    } else {
        installStatePath_ = "install_state.json";
    }
#else
    const char* home = getenv("HOME");
    if (home) {
        installStatePath_ = std::string(home) + "/.rawrxd/install_state.json";
    } else {
        installStatePath_ = "install_state.json";
    }
#endif

    loadInstallState();
}

ExtensionAutoInstaller::~ExtensionAutoInstaller() {
    saveInstallState();
}

// ============================================================================
// State Persistence
// ============================================================================

void ExtensionAutoInstaller::loadInstallState() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(installStatePath_);
    if (!file.is_open()) {
        firstRunComplete_ = false;
        return;
    }

    // Simple line-based format:
    // Line 1: "FirstRunComplete" or "FirstRunPending"
    // Remaining lines: installed extension IDs
    std::string line;
    if (std::getline(file, line)) {
        firstRunComplete_ = (line == "FirstRunComplete");
    }

    while (std::getline(file, line)) {
        if (!line.empty() && line[0] != '#') {
            installed_.push_back(line);
        }
    }
}

void ExtensionAutoInstaller::saveInstallState() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Ensure directory exists
    std::filesystem::path statePath(installStatePath_);
    std::filesystem::create_directories(statePath.parent_path());

    std::ofstream file(installStatePath_);
    if (!file.is_open()) return;

    file << (firstRunComplete_.load() ? "FirstRunComplete" : "FirstRunPending") << "\n";
    file << "# Installed Extensions\n";
    for (const auto& id : installed_) {
        file << id << "\n";
    }
}

// ============================================================================
// Query API
// ============================================================================

bool ExtensionAutoInstaller::needsFirstRunInstall() const {
    return !firstRunComplete_.load();
}

void ExtensionAutoInstaller::setFirstRunComplete() {
    firstRunComplete_ = true;
    saveInstallState();
}

bool ExtensionAutoInstaller::isInstalled(const std::string& extensionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::find(installed_.begin(), installed_.end(), extensionId) != installed_.end();
}

std::vector<std::string> ExtensionAutoInstaller::getInstalledExtensions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return installed_;
}

std::vector<std::string> ExtensionAutoInstaller::getPendingExtensions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_;
}

// ============================================================================
// Progress Emission
// ============================================================================

void ExtensionAutoInstaller::emitProgress(const InstallProgress& progress,
                                           InstallProgressCallback callback) {
    if (callback) {
        callback(progress);
    }
}

// ============================================================================
// Single Extension Installation
// ============================================================================

AutoInstallResult ExtensionAutoInstaller::installSingleExtension(
    const std::string& extensionId,
    InstallProgressCallback callback)
{
    // Parse extensionId: "publisher.extensionName"
    size_t dotPos = extensionId.find('.');
    if (dotPos == std::string::npos || dotPos == 0 || dotPos == extensionId.size() - 1) {
        return AutoInstallResult::error("Invalid extension ID format", 1);
    }

    std::string publisher = extensionId.substr(0, dotPos);
    std::string extensionName = extensionId.substr(dotPos + 1);

    // Step 1: Query marketplace
    {
        InstallProgress progress{};
        progress.stage = InstallProgress::Stage::Querying;
        progress.extensionId = extensionId.c_str();
        progress.currentIndex = 0;
        progress.totalExtensions = 1;
        progress.detail = "Querying VS Code Marketplace...";
        emitProgress(progress, callback);
    }

    VSCodeMarketplace::MarketplaceEntry entry;
    if (!VSCodeMarketplace::GetById(extensionId, entry)) {
        InstallProgress progress{};
        progress.stage = InstallProgress::Stage::Failed;
        progress.extensionId = extensionId.c_str();
        progress.detail = "Extension not found in marketplace";
        emitProgress(progress, callback);
        return AutoInstallResult::error("Extension not found in marketplace", 2);
    }

    // Step 2: Download VSIX
    {
        InstallProgress progress{};
        progress.stage = InstallProgress::Stage::Downloading;
        progress.extensionId = extensionId.c_str();
        progress.detail = "Downloading .vsix package...";
        emitProgress(progress, callback);
    }

    std::string tempDir = ".rawrxd\\temp\\";
    std::filesystem::create_directories(tempDir);
    std::string vsixPath = tempDir + extensionId + "-" + entry.version + ".vsix";

    if (!VSCodeMarketplace::DownloadVsix(publisher, extensionName, entry.version, vsixPath)) {
        InstallProgress progress{};
        progress.stage = InstallProgress::Stage::Failed;
        progress.extensionId = extensionId.c_str();
        progress.detail = "Failed to download .vsix";
        emitProgress(progress, callback);
        return AutoInstallResult::error("Failed to download .vsix", 3);
    }

    // Step 3: Install VSIX
    {
        InstallProgress progress{};
        progress.stage = InstallProgress::Stage::Installing;
        progress.extensionId = extensionId.c_str();
        progress.detail = "Installing extension...";
        emitProgress(progress, callback);
    }

    if (!RawrXD::VSIXInstaller::Install(vsixPath)) {
        InstallProgress progress{};
        progress.stage = InstallProgress::Stage::Failed;
        progress.extensionId = extensionId.c_str();
        progress.detail = "Failed to install .vsix";
        emitProgress(progress, callback);
        return AutoInstallResult::error("Failed to install .vsix", 4);
    }

    // Step 4: Verify installation
    {
        InstallProgress progress{};
        progress.stage = InstallProgress::Stage::Verifying;
        progress.extensionId = extensionId.c_str();
        progress.detail = "Verifying installation...";
        emitProgress(progress, callback);
    }

    // Step 5: Mark as installed
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (std::find(installed_.begin(), installed_.end(), extensionId) == installed_.end()) {
            installed_.push_back(extensionId);
        }
        saveInstallState();
    }

    // Step 6: Complete
    {
        InstallProgress progress{};
        progress.stage = InstallProgress::Stage::Complete;
        progress.extensionId = extensionId.c_str();
        progress.detail = "Installation complete";
        emitProgress(progress, callback);
    }

    AutoInstallResult result = AutoInstallResult::ok(1);
    result.installedIds.push_back(extensionId);
    result.detail = "Successfully installed " + extensionId;
    return result;
}

// ============================================================================
// Public API
// ============================================================================

AutoInstallResult ExtensionAutoInstaller::installExtension(
    const std::string& extensionId,
    InstallProgressCallback callback)
{
    // Check if already installed
    if (isInstalled(extensionId)) {
        return AutoInstallResult::error("Extension already installed", 0);
    }

    return installSingleExtension(extensionId, callback);
}

AutoInstallResult ExtensionAutoInstaller::installExtensions(
    const std::vector<std::string>& extensionIds,
    InstallProgressCallback callback)
{
    AutoInstallResult result{};
    result.success = true;
    result.installedCount = 0;
    result.failedCount = 0;
    result.errorCode = 0;

    int index = 0;
    for (const auto& id : extensionIds) {
        // Skip if already installed
        if (isInstalled(id)) {
            continue;
        }

        // Update progress context
        InstallProgress progress{};
        progress.currentIndex = index;
        progress.totalExtensions = (int)extensionIds.size();

        AutoInstallResult singleResult = installSingleExtension(id, callback);
        if (singleResult.success) {
            result.installedCount++;
            result.installedIds.push_back(id);
        } else {
            result.failedCount++;
            result.failedIds.push_back(id);
        }

        index++;
    }

    if (result.failedCount > 0) {
        result.success = false;
        result.detail = "Installed " + std::to_string(result.installedCount) +
                        ", failed " + std::to_string(result.failedCount);
    } else {
        result.detail = "Successfully installed " + std::to_string(result.installedCount) +
                        " extensions";
    }

    return result;
}

AutoInstallResult ExtensionAutoInstaller::installPriorityExtensions(
    InstallProgressCallback callback)
{
    std::vector<std::string> toInstall;
    for (const auto& ext : PRIORITY_EXTENSIONS) {
        if (ext.autoInstall) {
            toInstall.push_back(ext.id);
        }
    }

    AutoInstallResult result = installExtensions(toInstall, callback);
    if (result.success || result.installedCount > 0) {
        setFirstRunComplete();
    }

    return result;
}

AutoInstallResult ExtensionAutoInstaller::syncMarketplaceCatalog(
    int maxExtensions,
    InstallProgressCallback callback)
{
    // Query VS Code marketplace for top extensions
    std::vector<VSCodeMarketplace::MarketplaceEntry> entries;

    {
        InstallProgress progress{};
        progress.stage = InstallProgress::Stage::Querying;
        progress.detail = "Syncing marketplace catalog...";
        emitProgress(progress, callback);
    }

    // Query in batches (max 100 per request)
    const int pageSize = 100;
    int page = 1;
    int totalFetched = 0;

    while (totalFetched < maxExtensions) {
        std::vector<VSCodeMarketplace::MarketplaceEntry> batch;
        if (!VSCodeMarketplace::Query("", pageSize, page, batch)) {
            break;
        }

        if (batch.empty()) break;

        for (const auto& entry : batch) {
            entries.push_back(entry);
            totalFetched++;
            if (totalFetched >= maxExtensions) break;
        }

        page++;
    }

    // Now install each extension
    std::vector<std::string> extensionIds;
    for (const auto& entry : entries) {
        extensionIds.push_back(entry.id);
    }

    return installExtensions(extensionIds, callback);
}

} // namespace Extensions
} // namespace RawrXD
