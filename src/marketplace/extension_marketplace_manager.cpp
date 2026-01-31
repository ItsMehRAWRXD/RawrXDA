#include "marketplace/extension_marketplace_manager.h"
#include "marketplace/vsix_installer.h"
#include "marketplace/enterprise_policy_engine.h"
#include "marketplace/offline_cache_store.h"
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
    
    // Set up update checking (every 6 hours)
    m_updateCheckTimer->setInterval(6 * 60 * 60 * 1000); // 6 hours  // Signal connection removed\nm_updateCheckTimer->start();
    
}

ExtensionMarketplaceManager::~ExtensionMarketplaceManager() {
    saveInstalledExtensions();
}

void ExtensionMarketplaceManager::searchExtensions(const std::string& query, int page, int pageSize) {
    if (m_offlineMode) {
        // Try to get from cache
        void* cachedResults = m_cacheStore->getCachedSearchResults(query);
        if (!cachedResults.empty()) {
            searchResultsReceived(cachedResults);
            return;
        }
        errorOccurred("Offline mode: No cached results for search query");
        return;
    }
    
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
    
    void* criteria;
    criteria.append(queryCriteria);
    filter["criteria"] = criteria;
    filter["pageNumber"] = page;
    filter["pageSize"] = pageSize;
    filter["sortBy"] = 0; // Relevance
    filter["sortOrder"] = 0; // Descending
    
    void* filters;
    filters.append(filter);
    requestBody["filters"] = filters;
    
    void* doc(requestBody);
    std::vector<uint8_t> jsonData = doc.toJson();
    
    void** reply = m_networkManager->post(request, jsonData);
    reply->setProperty("requestType", "search");
}

void ExtensionMarketplaceManager::getExtensionDetails(const std::string& extensionId) {
    if (m_offlineMode) {
        // Try to get from cache
        void* cachedDetails = m_cacheStore->getCachedExtensionDetails(extensionId);
        if (!cachedDetails.empty()) {
            extensionDetailsReceived(cachedDetails);
            return;
        }
        errorOccurred("Offline mode: No cached details for extension");
        return;
    }
    
    std::string url(std::string("https://marketplace.visualstudio.com/_apis/public/gallery/publishers/%1/extensions/%2")
             )  // Publisher
             ));   // Extension name
    void* request(url);
    request.setHeader(void*::ContentTypeHeader, "application/json");
    
    void** reply = m_networkManager->get(request);
    reply->setProperty("requestType", "details");
    reply->setProperty("extensionId", extensionId);
}

void ExtensionMarketplaceManager::installExtension(const std::string& extensionId, const std::string& version) {
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
    
    // Download and install
    m_vsixInstaller->installFromUrl(downloadUrl, extensionId);
}

void ExtensionMarketplaceManager::updateExtension(const std::string& extensionId) {
    // Check if extension is installed
    bool found = false;
    for (const auto& ext : m_installedExtensions) {
        if (ext.id == extensionId) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        installationError(extensionId, "Extension not installed");
        return;
    }
    
    // For now, just reinstall (in a real implementation, we'd check for updates)
    installExtension(extensionId);
}

void ExtensionMarketplaceManager::uninstallExtension(const std::string& extensionId) {
    if (m_vsixInstaller->uninstallExtension(extensionId)) {
        // Remove from installed list
        m_installedExtensions.erase(
            std::remove_if(m_installedExtensions.begin(), m_installedExtensions.end(),
                          [extensionId](const ExtensionInfo& ext) { return ext.id == extensionId; }),
            m_installedExtensions.end());
        saveInstalledExtensions();
        uninstallCompleted(extensionId, true);
    } else {
        uninstallCompleted(extensionId, false);
    }
}

void ExtensionMarketplaceManager::listInstalledExtensions() {
    void* result;
    for (const auto& ext : m_installedExtensions) {
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
    for (const std::string& id : extensionIds) {
        getExtensionDetails(id);
    }
}

void ExtensionMarketplaceManager::onSearchReplyFinished() {
    void** reply = qobject_cast<void**>(sender());
    if (!reply) return;
    
    if (reply->error() != void*::NoError) {
        errorOccurred(reply->errorString());
        reply->deleteLater();
        return;
    }
    
    std::vector<uint8_t> data = reply->readAll();
    void* doc = void*::fromJson(data);
    
    if (doc.isNull()) {
        errorOccurred("Invalid JSON response");
        reply->deleteLater();
        return;
    }
    
    parseSearchResults(reply);
    reply->deleteLater();
}

void ExtensionMarketplaceManager::onExtensionDetailsReplyFinished() {
    void** reply = qobject_cast<void**>(sender());
    if (!reply) return;
    
    if (reply->error() != void*::NoError) {
        errorOccurred(reply->errorString());
        reply->deleteLater();
        return;
    }
    
    std::vector<uint8_t> data = reply->readAll();
    void* doc = void*::fromJson(data);
    
    if (doc.isNull()) {
        errorOccurred("Invalid JSON response");
        reply->deleteLater();
        return;
    }
    
    parseExtensionDetails(reply);
    reply->deleteLater();
}

void ExtensionMarketplaceManager::parseSearchResults(void** reply) {
    void* doc = void*::fromJson(reply->readAll());
    void* root = doc.object();
    
    void* extensions;
    if (root.contains("results") && root["results"].isArray()) {
        void* results = root["results"].toArray();
        if (!results.empty() && results.first().isObject()) {
            void* result = results.first().toObject();
            if (result.contains("extensions") && result["extensions"].isArray()) {
                extensions = result["extensions"].toArray();
            }
        }
    }
    
    // Cache the results
    std::string query = reply->request().url().query();
    m_cacheStore->cacheSearchResults(query, extensions);
    
    searchResultsReceived(extensions);
}

void ExtensionMarketplaceManager::parseExtensionDetails(void** reply) {
    void* doc = void*::fromJson(reply->readAll());
    void* extension = doc.object();
    
    std::string extensionId = reply->property("extensionId").toString();
    if (!extensionId.empty()) {
        m_cacheStore->cacheExtensionDetails(extensionId, extension);
    }
    
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
    if (m_policyEngine) {
        return m_policyEngine->isExtensionAllowed(extensionId);
    }
    return true; // No policy engine, allow all
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
    auto* reply = qobject_cast<void**>(sender());
    if (!reply) {
        return;
    }
    
    if (reply->error() != void*::NoError) {
        installationError(std::string(), reply->errorString());
        reply->deleteLater();
        return;
    }
    
    std::vector<uint8_t> data = reply->readAll();
    std::string extensionId = reply->property("extensionId").toString();
    
    
    // Save to temp file and install
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
    
    reply->deleteLater();
}

void ExtensionMarketplaceManager::onUpdateCheckReplyFinished() {
    auto* reply = qobject_cast<void**>(sender());
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
                    updateAvailable(extensionId, latestVersion);
                    break;
                }
            }
        }
    }
    
    reply->deleteLater();
}

