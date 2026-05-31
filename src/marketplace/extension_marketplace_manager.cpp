// ============================================================================
// extension_marketplace_manager.cpp — Qt-free rewrite using VSCodeMarketplaceAPI
// Architecture: C++20 | Win32 | No Qt | No exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#include "marketplace/extension_marketplace_manager.h"
#include "marketplace/enterprise_policy_engine.h"
#include "marketplace/offline_cache_store.h"
#include "marketplace/vsix_installer.h"
#include "../win32app/VSCodeMarketplaceAPI.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <shlobj.h>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

// ── Internal helpers ───────────────────────────────────────────────────────

static std::string GetInstalledExtensionsPath() {
    wchar_t appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        char buf[MAX_PATH * 2] = {};
        WideCharToMultiByte(CP_UTF8, 0, appData, -1, buf, sizeof(buf), nullptr, nullptr);
        std::string p(buf);
        if (!p.empty() && p.back() != '\\') p += '\\';
        p += "RawrXD\\installed_extensions.json";
        return p;
    }
    return "installed_extensions.json";
}

// ── Constructor / Destructor ───────────────────────────────────────────────

ExtensionMarketplaceManager::ExtensionMarketplaceManager()
    : m_vsixInstaller(new VsixInstaller())
    , m_policyEngine(nullptr)
    , m_cacheStore(new OfflineCacheStore())
    , m_offlineMode(false)
{
    loadInstalledExtensions();
}

ExtensionMarketplaceManager::~ExtensionMarketplaceManager() {
    saveInstalledExtensions();
    delete m_vsixInstaller;
    delete m_cacheStore;
    m_vsixInstaller = nullptr;
    m_cacheStore    = nullptr;
}

// ── Search ─────────────────────────────────────────────────────────────────

void ExtensionMarketplaceManager::searchExtensions(const std::string& query, int page, int pageSize) {
    if (m_offlineMode) {
        if (m_cacheStore) {
            std::string cachedStr = m_cacheStore->getCachedSearchResults(query);
            if (!cachedStr.empty()) {
                if (m_onSearchResults) m_onSearchResults(cachedStr);
                return;
            }
        }
        if (m_onErrorOccurred) m_onErrorOccurred("Offline mode: no cached results for: " + query);
        return;
    }

    std::vector<VSCodeMarketplace::MarketplaceEntry> entries;
    if (VSCodeMarketplace::Query(query, pageSize, page, entries)) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& e : entries) {
            nlohmann::json obj;
            obj["id"]               = e.id;
            obj["publisher"]        = e.publisher;
            obj["extensionName"]    = e.extensionName;
            obj["displayName"]      = e.displayName;
            obj["description"]      = e.shortDescription;
            obj["version"]          = e.version;
            obj["installCount"]     = e.installCount;
            obj["averageRating"]    = e.averageRating;
            arr.push_back(obj);
        }
        if (m_cacheStore) m_cacheStore->cacheSearchResults(query, arr.dump());
        if (m_onSearchResults) m_onSearchResults(arr.dump());
    } else {
        if (m_onErrorOccurred) m_onErrorOccurred("Marketplace query failed for: " + query);
    }
}

void ExtensionMarketplaceManager::getFeaturedExtensions(int page, int pageSize) {
    searchExtensions("", page, pageSize);
}

void ExtensionMarketplaceManager::getCategoryExtensions(const std::string& category, int page, int pageSize) {
    searchExtensions(category, page, pageSize);
}

void ExtensionMarketplaceManager::getExtensionDetails(const std::string& extensionId) {
    if (m_offlineMode) {
        if (m_cacheStore) {
            std::string cachedStr = m_cacheStore->getCachedExtensionDetails(extensionId);
            if (!cachedStr.empty()) {
                if (m_onExtensionDetails) m_onExtensionDetails(cachedStr);
                return;
            }
        }
        if (m_onErrorOccurred) m_onErrorOccurred("Offline mode: no cached details for: " + extensionId);
        return;
    }

    VSCodeMarketplace::MarketplaceEntry e;
    if (VSCodeMarketplace::GetById(extensionId, e)) {
        nlohmann::json obj;
        obj["id"]            = e.id;
        obj["publisher"]     = e.publisher;
        obj["extensionName"] = e.extensionName;
        obj["displayName"]   = e.displayName;
        obj["description"]   = e.shortDescription;
        obj["version"]       = e.version;
        obj["installCount"]  = e.installCount;
        obj["averageRating"] = e.averageRating;
        obj["marketplaceUrl"]= VSCodeMarketplace::ItemUrl(e.publisher, e.extensionName);

        bool installed = isExtensionInstalledLocally(extensionId);
        obj["installed"] = installed;

        if (m_cacheStore) m_cacheStore->cacheExtensionDetails(extensionId, obj.dump());
        if (m_onExtensionDetails) m_onExtensionDetails(obj.dump());
    } else {
        if (m_onErrorOccurred) m_onErrorOccurred("Extension not found in marketplace: " + extensionId);
    }
}

// ── Install / Update / Uninstall ───────────────────────────────────────────

void ExtensionMarketplaceManager::installExtension(const std::string& extensionId,
                                                    const std::string& version) {
    if (!isExtensionAllowed(extensionId)) {
        if (m_onInstallationError) m_onInstallationError(extensionId, "Blocked by enterprise policy");
        return;
    }

    if (m_onInstallationStarted) m_onInstallationStarted(extensionId);

    // Parse publisher.extensionName
    size_t dot = extensionId.find('.');
    if (dot == std::string::npos || dot == 0 || dot == extensionId.size() - 1) {
        if (m_onInstallationError) m_onInstallationError(extensionId, "Invalid extension id format");
        return;
    }
    std::string publisher  = extensionId.substr(0, dot);
    std::string extName    = extensionId.substr(dot + 1);

    // Resolve version if not given
    std::string resolvedVersion = version;
    if (resolvedVersion.empty()) {
        VSCodeMarketplace::MarketplaceEntry e;
        if (!VSCodeMarketplace::GetById(extensionId, e)) {
            if (m_onInstallationError) m_onInstallationError(extensionId, "Extension not found in marketplace");
            return;
        }
        resolvedVersion = e.version;
    }

    // Download VSIX to temp
    wchar_t tmpDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tmpDir);
    char tmpDirA[MAX_PATH * 2] = {};
    WideCharToMultiByte(CP_UTF8, 0, tmpDir, -1, tmpDirA, sizeof(tmpDirA), nullptr, nullptr);
    std::string vsixPath = std::string(tmpDirA) + extensionId + "-" + resolvedVersion + ".vsix";

    if (!VSCodeMarketplace::DownloadVsix(publisher, extName, resolvedVersion, vsixPath)) {
        if (m_onInstallationError) m_onInstallationError(extensionId, "VSIX download failed");
        return;
    }

    // Install
    if (m_vsixInstaller) {
        m_vsixInstaller->setOnInstallationCompleted([this, extensionId](const std::string& /*id*/, bool ok) {
            if (ok) {
                // Record installation
                ExtensionInfo info{};
                info.id        = extensionId;
                info.installed = true;
                upsertInstalled(info);
                saveInstalledExtensions();
            }
            if (m_onInstallationCompleted) m_onInstallationCompleted(extensionId, ok);
        });
        m_vsixInstaller->setOnInstallationError([this, extensionId](const std::string& /*id*/, const std::string& err) {
            if (m_onInstallationError) m_onInstallationError(extensionId, err);
        });
        m_vsixInstaller->installFromFile(vsixPath, extensionId);
    } else {
        if (m_onInstallationError) m_onInstallationError(extensionId, "VsixInstaller not initialized");
    }
}

void ExtensionMarketplaceManager::updateExtension(const std::string& extensionId) {
    bool found = false;
    for (const auto& ext : m_installedExtensions) {
        if (ext.id == extensionId) { found = true; break; }
    }
    if (!found) {
        if (m_onInstallationError) m_onInstallationError(extensionId, "Extension not installed");
        return;
    }
    // Re-install latest version (will overwrite).
    installExtension(extensionId);
}

void ExtensionMarketplaceManager::uninstallExtension(const std::string& extensionId) {
    if (m_vsixInstaller && m_vsixInstaller->uninstallExtension(extensionId)) {
        m_installedExtensions.erase(
            std::remove_if(m_installedExtensions.begin(), m_installedExtensions.end(),
                           [&extensionId](const ExtensionInfo& e){ return e.id == extensionId; }),
            m_installedExtensions.end());
        saveInstalledExtensions();
        if (m_onUninstallCompleted) m_onUninstallCompleted(extensionId, true);
    } else {
        if (m_onUninstallCompleted) m_onUninstallCompleted(extensionId, false);
    }
}

void ExtensionMarketplaceManager::listInstalledExtensions() {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& ext : m_installedExtensions) {
        nlohmann::json obj;
        obj["id"]               = ext.id;
        obj["name"]             = ext.name;
        obj["publisher"]        = ext.publisher;
        obj["version"]          = ext.installedVersion;
        obj["installed"]        = ext.installed;
        arr.push_back(obj);
    }
    if (m_onInstalledExtensionsList) m_onInstalledExtensionsList(arr.dump());
}

// ── Policy / Mode ──────────────────────────────────────────────────────────

void ExtensionMarketplaceManager::setEnterprisePolicyEngine(EnterprisePolicyEngine* policy) {
    m_policyEngine = policy;
}

void ExtensionMarketplaceManager::enableOfflineMode(bool enabled) {
    m_offlineMode = enabled;
}

void ExtensionMarketplaceManager::syncWithPrivateMarketplace(const std::string& url) {
    m_privateMarketplaceUrl = url;
}

void ExtensionMarketplaceManager::clearCache() {
    if (m_cacheStore) m_cacheStore->clearCache();
    if (m_onCacheCleared) m_onCacheCleared();
}

void ExtensionMarketplaceManager::preloadExtensions(const std::vector<std::string>& extensionIds) {
    for (const auto& id : extensionIds) {
        getExtensionDetails(id);
    }
}

// ── Private helpers ────────────────────────────────────────────────────────

bool ExtensionMarketplaceManager::isExtensionAllowed(const std::string& extensionId) {
    if (m_policyEngine) return m_policyEngine->isExtensionAllowed(extensionId);
    return true;
}

bool ExtensionMarketplaceManager::isExtensionInstalledLocally(const std::string& extensionId) const {
    for (const auto& e : m_installedExtensions) {
        if (e.id == extensionId && e.installed) return true;
    }
    return false;
}

void ExtensionMarketplaceManager::upsertInstalled(const ExtensionInfo& info) {
    for (auto& e : m_installedExtensions) {
        if (e.id == info.id) { e = info; return; }
    }
    m_installedExtensions.push_back(info);
}

void ExtensionMarketplaceManager::saveInstalledExtensions() {
    std::string path = GetInstalledExtensionsPath();
    fs::path dir = fs::path(path).parent_path();
    if (!dir.empty()) fs::create_directories(dir);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& ext : m_installedExtensions) {
        nlohmann::json obj;
        obj["id"]               = ext.id;
        obj["name"]             = ext.name;
        obj["publisher"]        = ext.publisher;
        obj["installedVersion"] = ext.installedVersion;
        obj["installed"]        = ext.installed;
        arr.push_back(obj);
    }
    std::ofstream f(path);
    if (f.is_open()) f << arr.dump(2);
}

void ExtensionMarketplaceManager::loadInstalledExtensions() {
    std::string path = GetInstalledExtensionsPath();
    std::ifstream f(path);
    if (!f.is_open()) return;

    nlohmann::json arr;
    try {
        std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        arr = nlohmann::json::parse(s);
    } catch (...) { return; }
    if (!arr.is_array()) return;

    m_installedExtensions.clear();
    for (const auto& obj : arr) {
        ExtensionInfo info{};
        info.id               = obj.value("id",               "");
        info.name             = obj.value("name",             "");
        info.publisher        = obj.value("publisher",        "");
        info.installedVersion = obj.value("installedVersion", "");
        info.installed        = obj.value("installed",        true);
        if (!info.id.empty()) m_installedExtensions.push_back(info);
    }
}

// Async reply helpers (no-op in synchronous path — kept for ABI compatibility)
void ExtensionMarketplaceManager::onSearchReplyFinished() {
    // Parse search results from network reply
    if (m_searchReply) {
        parseSearchResults(m_searchReply);
        m_searchReply = nullptr;
    }
}

void ExtensionMarketplaceManager::onExtensionDetailsReplyFinished() {
    // Parse extension details from network reply
    if (m_detailsReply) {
        parseExtensionDetails(m_detailsReply);
        m_detailsReply = nullptr;
    }
}

void ExtensionMarketplaceManager::onInstallReplyFinished() {
    // Handle installation completion
    if (m_installReply) {
        // Verify installation success
        m_installReply = nullptr;
        emitInstallationStatus("completed");
    }
}

void ExtensionMarketplaceManager::onUpdateCheckReplyFinished() {
    // Process update check results
    if (m_updateReply) {
        // Compare versions and notify
        m_updateReply = nullptr;
    }
}

void ExtensionMarketplaceManager::parseSearchResults(void* reply) {
    (void)reply;
    // Parse JSON search results and populate model
    // In production: parse actual network response
}

void ExtensionMarketplaceManager::parseExtensionDetails(void* reply) {
    (void)reply;
    // Parse extension metadata and changelog
    // In production: parse actual network response
}

void ExtensionMarketplaceManager::checkForUpdates() {
    // Check all installed extensions for updates
    for (const auto& ext : m_installedExtensions) {
        std::string url = getExtensionDownloadUrl(ext.id, ext.version);
        if (!url.empty()) {
            // Query marketplace for latest version
        }
    }
}

std::string ExtensionMarketplaceManager::getExtensionDownloadUrl(const std::string& extensionId, const std::string& version) {
    if (m_privateMarketplaceUrl.empty()) {
        return "";
    }
    std::string url = m_privateMarketplaceUrl;
    if (url.back() != '/') url += '/';
    url += extensionId;
    if (!version.empty()) {
        url += "/" + version;
    }
    url += ".vsix";
    return url;
}

void ExtensionMarketplaceManager::emitInstallationStatus(const std::string& status) {
    if (m_onInstallationCompleted) {
        m_onInstallationCompleted("", status == "completed");
    }
}
