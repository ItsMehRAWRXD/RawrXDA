#include "marketplace/extension_marketplace_manager.h"
<<<<<<< HEAD
#include "marketplace/enterprise_policy_engine.h"
#include "marketplace/offline_cache_store.h"
#include "marketplace/vsix_installer.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <fstream>
=======
#include "marketplace/vsix_installer.h"
#include "marketplace/enterprise_policy_engine.h"
#include "marketplace/offline_cache_store.h"
>>>>>>> origin/main
ExtensionMarketplaceManager::ExtensionMarketplaceManager()
    
    , m_networkManager(new void*(this))
    , m_vsixInstaller(new VsixInstaller(this))
    , m_policyEngine(nullptr)
    , m_cacheStore(new OfflineCacheStore(this))
    , m_offlineMode(false)
    , m_updateCheckTimer(new // Timer(this))
{
    // Load installed extensions from storage
    loadInstalledExtensions();
<<<<<<< HEAD

    // Set up update checking (every 6 hours)
    m_updateCheckTimer->setInterval(6 * 60 * 60 *
                                    1000);  // 6 hours  // Signal connection removed\nm_updateCheckTimer->start();
=======
    
    // Set up update checking (every 6 hours)
    m_updateCheckTimer->setInterval(6 * 60 * 60 * 1000); // 6 hours  // Signal connection removed\nm_updateCheckTimer->start();
>>>>>>> origin/main
    
}

ExtensionMarketplaceManager::~ExtensionMarketplaceManager() {
    saveInstalledExtensions();
}

void ExtensionMarketplaceManager::searchExtensions(const std::string& query, int page, int pageSize) {
<<<<<<< HEAD
    if (m_offlineMode)
    {
        // Try to get from cache
        void* cachedResults = m_cacheStore->getCachedSearchResults(query);
        if (!cachedResults.empty())
        {
=======
    if (m_offlineMode) {
        // Try to get from cache
        void* cachedResults = m_cacheStore->getCachedSearchResults(query);
        if (!cachedResults.empty()) {
>>>>>>> origin/main
            searchResultsReceived(cachedResults);
            return;
        }
        errorOccurred("Offline mode: No cached results for search query");
        return;
    }
<<<<<<< HEAD

    std::string url("https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery");
    void* request(url);
    request.setHeader(void* ::ContentTypeHeader, "application/json");

    void* requestBody;
    requestBody["filters"] = void*();
    requestBody["flags"] = 0x1FF;  // Include most extension details

    void* filter;
    filter["criteria"] = void*();

    void* queryCriteria;
    queryCriteria["filterType"] = 8;  // Search text
    queryCriteria["value"] = query;

=======
    
    std::string url("https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery");
    void* request(url);
    request.setHeader(void*::ContentTypeHeader, "application/json");
    
    void* requestBody;
    requestBody["filters"] = void*();
    requestBody["flags"] = 0x1FF; // Include most extension details
    
    void* filter;
    filter["criteria"] = void*();
    
    void* queryCriteria;
    queryCriteria["filterType"] = 8; // Search text
    queryCriteria["value"] = query;
    
>>>>>>> origin/main
    void* criteria;
    criteria.append(queryCriteria);
    filter["criteria"] = criteria;
    filter["pageNumber"] = page;
    filter["pageSize"] = pageSize;
<<<<<<< HEAD
    filter["sortBy"] = 0;     // Relevance
    filter["sortOrder"] = 0;  // Descending

    void* filters;
    filters.append(filter);
    requestBody["filters"] = filters;

    void* doc(requestBody);
    std::vector<uint8_t> jsonData = doc.toJson();

=======
    filter["sortBy"] = 0; // Relevance
    filter["sortOrder"] = 0; // Descending
    
    void* filters;
    filters.append(filter);
    requestBody["filters"] = filters;
    
    void* doc(requestBody);
    std::vector<uint8_t> jsonData = doc.toJson();
    
>>>>>>> origin/main
    void** reply = m_networkManager->post(request, jsonData);
    reply->setProperty("requestType", "search");
}

void ExtensionMarketplaceManager::getExtensionDetails(const std::string& extensionId) {
<<<<<<< HEAD
    if (m_offlineMode)
    {
        // Try to get from cache
        void* cachedDetails = m_cacheStore->getCachedExtensionDetails(extensionId);
        if (!cachedDetails.empty())
        {
=======
    if (m_offlineMode) {
        // Try to get from cache
        void* cachedDetails = m_cacheStore->getCachedExtensionDetails(extensionId);
        if (!cachedDetails.empty()) {
>>>>>>> origin/main
            extensionDetailsReceived(cachedDetails);
            return;
        }
        errorOccurred("Offline mode: No cached details for extension");
        return;
    }
<<<<<<< HEAD

=======
    
>>>>>>> origin/main
    std::string url(std::string("https://marketplace.visualstudio.com/_apis/public/gallery/publishers/%1/extensions/%2")
             )  // Publisher
             ));   // Extension name
    void* request(url);
<<<<<<< HEAD
    request.setHeader(void* ::ContentTypeHeader, "application/json");

=======
    request.setHeader(void*::ContentTypeHeader, "application/json");
    
>>>>>>> origin/main
    void** reply = m_networkManager->get(request);
    reply->setProperty("requestType", "details");
    reply->setProperty("extensionId", extensionId);
}

void ExtensionMarketplaceManager::installExtension(const std::string& extensionId, const std::string& version) {
<<<<<<< HEAD
    if (!isExtensionAllowed(extensionId))
    {
        installationError(extensionId, "Extension blocked by enterprise policy");
        return;
    }

    installationStarted(extensionId);

    // Get download URL
    std::string downloadUrl = getExtensionDownloadUrl(extensionId, version);
    if (downloadUrl.empty())
    {
        installationError(extensionId, "Failed to get download URL");
        return;
    }

=======
    if (!isExtensionAllowed(extensionId)) {
        installationError(extensionId, "Extension blocked by enterprise policy");
        return;
    }
    
    installationStarted(extensionId);
    
    // Get download URL
    std::string downloadUrl = getExtensionDownloadUrl(extensionId, version);
    if (downloadUrl.empty()) {
        installationError(extensionId, "Failed to get download URL");
        return;
    }
    
>>>>>>> origin/main
    // Download and install
    m_vsixInstaller->installFromUrl(downloadUrl, extensionId);
}

void ExtensionMarketplaceManager::updateExtension(const std::string& extensionId) {
    // Check if extension is installed
    bool found = false;
<<<<<<< HEAD
    for (const auto& ext : m_installedExtensions)
    {
        if (ext.id == extensionId)
        {
=======
    for (const auto& ext : m_installedExtensions) {
        if (ext.id == extensionId) {
>>>>>>> origin/main
            found = true;
            break;
        }
    }
<<<<<<< HEAD

    if (!found)
    {
        installationError(extensionId, "Extension not installed");
        return;
    }

=======
    
    if (!found) {
        installationError(extensionId, "Extension not installed");
        return;
    }
    
>>>>>>> origin/main
    // For now, just reinstall (in a real implementation, we'd check for updates)
    installExtension(extensionId);
}

void ExtensionMarketplaceManager::uninstallExtension(const std::string& extensionId) {
<<<<<<< HEAD
    if (m_vsixInstaller->uninstallExtension(extensionId))
    {
        // Remove from installed list
        m_installedExtensions.erase(std::remove_if(m_installedExtensions.begin(), m_installedExtensions.end(),
                                                   [extensionId](const ExtensionInfo& ext)
                                                   { return ext.id == extensionId; }),
                                    m_installedExtensions.end());
        saveInstalledExtensions();
        uninstallCompleted(extensionId, true);
    }
    else
    {
=======
    if (m_vsixInstaller->uninstallExtension(extensionId)) {
        // Remove from installed list
        m_installedExtensions.erase(
            std::remove_if(m_installedExtensions.begin(), m_installedExtensions.end(),
                          [extensionId](const ExtensionInfo& ext) { return ext.id == extensionId; }),
            m_installedExtensions.end());
        saveInstalledExtensions();
        uninstallCompleted(extensionId, true);
    } else {
>>>>>>> origin/main
        uninstallCompleted(extensionId, false);
    }
}

void ExtensionMarketplaceManager::listInstalledExtensions() {
    void* result;
<<<<<<< HEAD
    for (const auto& ext : m_installedExtensions)
    {
=======
    for (const auto& ext : m_installedExtensions) {
>>>>>>> origin/main
        void* obj;
        obj["id"] = ext.id;
        obj["name"] = ext.name;
        obj["version"] = ext.installedVersion;
        obj["publisher"] = ext.publisher;
        result.append(obj);
    }
    installedExtensionsList(result);
}

void ExtensionMarketplaceManager::setEnterprisePolicyEngine(EnterprisePolicyEngine* policyEngine) {
    m_policyEngine = policyEngine;
}

void ExtensionMarketplaceManager::enableOfflineMode(bool enabled) {
    m_offlineMode = enabled;
}

void ExtensionMarketplaceManager::syncWithPrivateMarketplace(const std::string& url) {
    m_privateMarketplaceUrl = url;
    // In a real implementation, this would sync with the private marketplace
}

void ExtensionMarketplaceManager::clearCache() {
    m_cacheStore->clearCache();
    cacheCleared();
}

void ExtensionMarketplaceManager::preloadExtensions(const std::stringList& extensionIds) {
<<<<<<< HEAD
    for (const std::string& id : extensionIds)
    {
=======
    for (const std::string& id : extensionIds) {
>>>>>>> origin/main
        getExtensionDetails(id);
    }
}

void ExtensionMarketplaceManager::onSearchReplyFinished() {
<<<<<<< HEAD
    // Native: reply from async HTTP (Win32: use WinHTTP callback or completion context)
    if (!reply)
        return;

    if (reply->error() != void* ::NoError)
    {
=======
// REMOVED_QT:     void** reply = qobject_cast<void**>(sender());
    if (!reply) return;
    
    if (reply->error() != void*::NoError) {
>>>>>>> origin/main
        errorOccurred(reply->errorString());
        reply->deleteLater();
        return;
    }
<<<<<<< HEAD

    std::vector<uint8_t> data = reply->readAll();
    void* doc = void* ::fromJson(data);

    if (doc.isNull())
    {
=======
    
    std::vector<uint8_t> data = reply->readAll();
    void* doc = void*::fromJson(data);
    
    if (doc.isNull()) {
>>>>>>> origin/main
        errorOccurred("Invalid JSON response");
        reply->deleteLater();
        return;
    }
<<<<<<< HEAD

=======
    
>>>>>>> origin/main
    parseSearchResults(reply);
    reply->deleteLater();
}

void ExtensionMarketplaceManager::onExtensionDetailsReplyFinished() {
<<<<<<< HEAD
    // Native: reply from async HTTP (Win32: use WinHTTP callback or completion context)
    if (!reply)
        return;

    if (reply->error() != void* ::NoError)
    {
=======
// REMOVED_QT:     void** reply = qobject_cast<void**>(sender());
    if (!reply) return;
    
    if (reply->error() != void*::NoError) {
>>>>>>> origin/main
        errorOccurred(reply->errorString());
        reply->deleteLater();
        return;
    }
<<<<<<< HEAD

    std::vector<uint8_t> data = reply->readAll();
    void* doc = void* ::fromJson(data);

    if (doc.isNull())
    {
=======
    
    std::vector<uint8_t> data = reply->readAll();
    void* doc = void*::fromJson(data);
    
    if (doc.isNull()) {
>>>>>>> origin/main
        errorOccurred("Invalid JSON response");
        reply->deleteLater();
        return;
    }
<<<<<<< HEAD

=======
    
>>>>>>> origin/main
    parseExtensionDetails(reply);
    reply->deleteLater();
}

void ExtensionMarketplaceManager::parseSearchResults(void** reply) {
<<<<<<< HEAD
    void* doc = void* ::fromJson(reply->readAll());
    void* root = doc.object();

    void* extensions;
    if (root.contains("results") && root["results"].isArray())
    {
        void* results = root["results"].toArray();
        if (!results.empty() && results.first().isObject())
        {
            void* result = results.first().toObject();
            if (result.contains("extensions") && result["extensions"].isArray())
            {
=======
    void* doc = void*::fromJson(reply->readAll());
    void* root = doc.object();
    
    void* extensions;
    if (root.contains("results") && root["results"].isArray()) {
        void* results = root["results"].toArray();
        if (!results.empty() && results.first().isObject()) {
            void* result = results.first().toObject();
            if (result.contains("extensions") && result["extensions"].isArray()) {
>>>>>>> origin/main
                extensions = result["extensions"].toArray();
            }
        }
    }
<<<<<<< HEAD

    // Cache the results
    std::string query = reply->request().url().query();
    m_cacheStore->cacheSearchResults(query, extensions);

=======
    
    // Cache the results
    std::string query = reply->request().url().query();
    m_cacheStore->cacheSearchResults(query, extensions);
    
>>>>>>> origin/main
    searchResultsReceived(extensions);
}

void ExtensionMarketplaceManager::parseExtensionDetails(void** reply) {
<<<<<<< HEAD
    void* doc = void* ::fromJson(reply->readAll());
    void* extension = doc.object();

    std::string extensionId = reply->property("extensionId").toString();
    if (!extensionId.empty())
    {
        m_cacheStore->cacheExtensionDetails(extensionId, extension);
    }

=======
    void* doc = void*::fromJson(reply->readAll());
    void* extension = doc.object();
    
    std::string extensionId = reply->property("extensionId").toString();
    if (!extensionId.empty()) {
        m_cacheStore->cacheExtensionDetails(extensionId, extension);
    }
    
>>>>>>> origin/main
    extensionDetailsReceived(extension);
}

std::string ExtensionMarketplaceManager::getExtensionDownloadUrl(const std::string& extensionId, const std::string& version) {
    // In a real implementation, this would fetch the actual download URL
    // For now, we'll return a placeholder
    return std::string("https://marketplace.visualstudio.com/_apis/public/gallery/publishers/%1/extensions/%2/%3/vspackage")
            )
            )
             ? "latest" : version);
}

bool ExtensionMarketplaceManager::isExtensionAllowed(const std::string& extensionId) {
<<<<<<< HEAD
    if (m_policyEngine)
    {
        return m_policyEngine->isExtensionAllowed(extensionId);
    }
    return true;  // No policy engine, allow all
=======
    if (m_policyEngine) {
        return m_policyEngine->isExtensionAllowed(extensionId);
    }
    return true; // No policy engine, allow all
>>>>>>> origin/main
}

void ExtensionMarketplaceManager::saveInstalledExtensions() {
    // In a real implementation, this would save to a file or database
}

void ExtensionMarketplaceManager::loadInstalledExtensions() {
    // In a real implementation, this would load from a file or database
}

void ExtensionMarketplaceManager::checkForUpdates() {
    // In a real implementation, this would check all installed extensions for updates
}

void ExtensionMarketplaceManager::onInstallReplyFinished() {
<<<<<<< HEAD
    // Native: reply from async HTTP (Win32: use completion context)
    if (!reply)
    {
        return;
    }

    if (reply->error() != void* ::NoError)
    {
=======
// REMOVED_QT:     auto* reply = qobject_cast<void**>(sender());
    if (!reply) {
        return;
    }
    
    if (reply->error() != void*::NoError) {
>>>>>>> origin/main
        installationError(std::string(), reply->errorString());
        reply->deleteLater();
        return;
    }
<<<<<<< HEAD

=======
    
>>>>>>> origin/main
    std::vector<uint8_t> data = reply->readAll();
    std::string extensionId = reply->property("extensionId").toString();


    // Save to temp file and install
<<<<<<< HEAD
#ifdef _WIN32
    char tmpDir[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tmpDir) == 0)
        tmpDir[0] = '\0';
    std::string tempPath = std::string(tmpDir) + extensionId + ".vsix";
#else
    const char* t = std::getenv("TMPDIR");
    if (!t)
        t = std::getenv("TEMP");
    if (!t)
        t = "/tmp";
    std::string tempPath = std::string(t) + "/" + extensionId + ".vsix";
#endif
    std::ofstream tempFile(tempPath, std::ios::out | std::ios::binary);
    if (tempFile.is_open())
    {
        tempFile.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        tempFile.close();
        if (m_vsixInstaller)
            m_vsixInstaller->installFromFile(tempPath);
    }
    else
    {
        installationError(extensionId, "Failed to write temporary file");
    }

=======
    std::string tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + extensionId + ".vsix";
    // File operation removed;
    if (tempFile.open(std::iostream::WriteOnly)) {
        tempFile.write(data);
        tempFile.close();
        
        if (m_vsixInstaller) {
            m_vsixInstaller->installFromFile(tempPath);
        }
    } else {
        installationError(extensionId, "Failed to write temporary file");
    }
    
>>>>>>> origin/main
    reply->deleteLater();
}

void ExtensionMarketplaceManager::onUpdateCheckReplyFinished() {
<<<<<<< HEAD
    // Native: reply from async HTTP (Win32: use completion context)
    if (!reply)
    {
        return;
    }

    if (reply->error() != void* ::NoError)
    {
        reply->deleteLater();
        return;
    }

    std::vector<uint8_t> data = reply->readAll();
    void* doc = void* ::fromJson(data);

    if (doc.isObject())
    {
        void* obj = doc.object();
        std::string extensionId = reply->property("extensionId").toString();
        std::string latestVersion = obj.value("version").toString();

        // Check if update is needed - search through installed extensions
        if (!extensionId.empty())
        {
            for (const auto& installed : m_installedExtensions)
            {
                if (installed.id == extensionId && latestVersion != installed.version)
                {
                    << ":" << installed.version << "->" << latestVersion;
=======
// REMOVED_QT:     auto* reply = qobject_cast<void**>(sender());
    if (!reply) {
        return;
    }
    
    if (reply->error() != void*::NoError) {
        reply->deleteLater();
        return;
    }
    
    std::vector<uint8_t> data = reply->readAll();
    void* doc = void*::fromJson(data);
    
    if (doc.isObject()) {
        void* obj = doc.object();
        std::string extensionId = reply->property("extensionId").toString();
        std::string latestVersion = obj.value("version").toString();
        
        // Check if update is needed - search through installed extensions
        if (!extensionId.empty()) {
            for (const auto& installed : m_installedExtensions) {
                if (installed.id == extensionId && latestVersion != installed.version) {
                             << ":" << installed.version << "->" << latestVersion;
>>>>>>> origin/main
                    updateAvailable(extensionId, latestVersion);
                    break;
                }
            }
        }
    }
<<<<<<< HEAD

    reply->deleteLater();
}
=======
    
    reply->deleteLater();
}

>>>>>>> origin/main
