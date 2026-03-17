#include "marketplace/extension_marketplace_manager.h"
#include "marketplace/vsix_installer.h"
#include "marketplace/enterprise_policy_engine.h"
#include "marketplace/offline_cache_store.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QDateTime>
#include <QDebug>

ExtensionMarketplaceManager::ExtensionMarketplaceManager(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_vsixInstaller(new VsixInstaller(this))
    , m_policyEngine(nullptr)
    , m_cacheStore(new OfflineCacheStore(this))
    , m_offlineMode(false)
    , m_updateCheckTimer(new QTimer(this))
{
    // Load installed extensions from storage
    loadInstalledExtensions();
    
    // Set up update checking (every 6 hours)
    m_updateCheckTimer->setInterval(6 * 60 * 60 * 1000); // 6 hours
    connect(m_updateCheckTimer, &QTimer::timeout, this, &ExtensionMarketplaceManager::checkForUpdates);
    m_updateCheckTimer->start();
    
    // Connect network manager signals
    connect(m_networkManager, &QNetworkAccessManager::finished, 
            this, &ExtensionMarketplaceManager::onSearchReplyFinished);
    
    // Connect VSIX installer signals
    connect(m_vsixInstaller, &VsixInstaller::installationCompleted, 
            this, &ExtensionMarketplaceManager::installationCompleted);
    connect(m_vsixInstaller, &VsixInstaller::installationError, 
            this, &ExtensionMarketplaceManager::installationError);
    
    qDebug() << "[ExtensionMarketplaceManager] Initialized";
}

ExtensionMarketplaceManager::~ExtensionMarketplaceManager() {
    saveInstalledExtensions();
}

void ExtensionMarketplaceManager::searchExtensions(const QString& query, int page, int pageSize) {
    if (m_offlineMode) {
        // Try to get from cache
        QJsonArray cachedResults = m_cacheStore->getCachedSearchResults(query);
        if (!cachedResults.isEmpty()) {
            emit searchResultsReceived(cachedResults);
            return;
        }
        emit errorOccurred("Offline mode: No cached results for search query");
        return;
    }
    
    QUrl url("https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject requestBody;
    requestBody["filters"] = QJsonArray();
    requestBody["flags"] = 0x1FF; // Include most extension details
    
    QJsonObject filter;
    filter["criteria"] = QJsonArray();
    
    QJsonObject queryCriteria;
    queryCriteria["filterType"] = 8; // Search text
    queryCriteria["value"] = query;
    
    QJsonArray criteria;
    criteria.append(queryCriteria);
    filter["criteria"] = criteria;
    filter["pageNumber"] = page;
    filter["pageSize"] = pageSize;
    filter["sortBy"] = 0; // Relevance
    filter["sortOrder"] = 0; // Descending
    
    QJsonArray filters;
    filters.append(filter);
    requestBody["filters"] = filters;
    
    QJsonDocument doc(requestBody);
    QByteArray jsonData = doc.toJson();
    
    QNetworkReply* reply = m_networkManager->post(request, jsonData);
    reply->setProperty("requestType", "search");
}

void ExtensionMarketplaceManager::getExtensionDetails(const QString& extensionId) {
    if (m_offlineMode) {
        // Try to get from cache
        QJsonObject cachedDetails = m_cacheStore->getCachedExtensionDetails(extensionId);
        if (!cachedDetails.isEmpty()) {
            emit extensionDetailsReceived(cachedDetails);
            return;
        }
        emit errorOccurred("Offline mode: No cached details for extension");
        return;
    }
    
    QUrl url(QString("https://marketplace.visualstudio.com/_apis/public/gallery/publishers/%1/extensions/%2")
             .arg(extensionId.section('.', 0, 0))  // Publisher
             .arg(extensionId.section('.', 1)));   // Extension name
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_networkManager->get(request);
    reply->setProperty("requestType", "details");
    reply->setProperty("extensionId", extensionId);
}

void ExtensionMarketplaceManager::installExtension(const QString& extensionId, const QString& version) {
    if (!isExtensionAllowed(extensionId)) {
        emit installationError(extensionId, "Extension blocked by enterprise policy");
        return;
    }
    
    emit installationStarted(extensionId);
    
    // Get download URL
    QString downloadUrl = getExtensionDownloadUrl(extensionId, version);
    if (downloadUrl.isEmpty()) {
        emit installationError(extensionId, "Failed to get download URL");
        return;
    }
    
    // Download and install
    m_vsixInstaller->installFromUrl(downloadUrl, extensionId);
}

void ExtensionMarketplaceManager::updateExtension(const QString& extensionId) {
    // Check if extension is installed
    bool found = false;
    for (const auto& ext : m_installedExtensions) {
        if (ext.id == extensionId) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        emit installationError(extensionId, "Extension not installed");
        return;
    }
    
    // For now, just reinstall (in a real implementation, we'd check for updates)
    installExtension(extensionId);
}

void ExtensionMarketplaceManager::uninstallExtension(const QString& extensionId) {
    if (m_vsixInstaller->uninstallExtension(extensionId)) {
        // Remove from installed list
        m_installedExtensions.erase(
            std::remove_if(m_installedExtensions.begin(), m_installedExtensions.end(),
                          [extensionId](const ExtensionInfo& ext) { return ext.id == extensionId; }),
            m_installedExtensions.end());
        saveInstalledExtensions();
        emit uninstallCompleted(extensionId, true);
    } else {
        emit uninstallCompleted(extensionId, false);
    }
}

void ExtensionMarketplaceManager::listInstalledExtensions() {
    QJsonArray result;
    for (const auto& ext : m_installedExtensions) {
        QJsonObject obj;
        obj["id"] = ext.id;
        obj["name"] = ext.name;
        obj["version"] = ext.installedVersion;
        obj["publisher"] = ext.publisher;
        result.append(obj);
    }
    emit installedExtensionsList(result);
}

void ExtensionMarketplaceManager::setEnterprisePolicyEngine(EnterprisePolicyEngine* policyEngine) {
    m_policyEngine = policyEngine;
}

void ExtensionMarketplaceManager::enableOfflineMode(bool enabled) {
    m_offlineMode = enabled;
}

void ExtensionMarketplaceManager::syncWithPrivateMarketplace(const QString& url) {
    m_privateMarketplaceUrl = url;
    // In a real implementation, this would sync with the private marketplace
    qDebug() << "[ExtensionMarketplaceManager] Sync with private marketplace:" << url;
}

void ExtensionMarketplaceManager::clearCache() {
    m_cacheStore->clearCache();
    emit cacheCleared();
}

void ExtensionMarketplaceManager::preloadExtensions(const QStringList& extensionIds) {
    for (const QString& id : extensionIds) {
        getExtensionDetails(id);
    }
}

void ExtensionMarketplaceManager::onSearchReplyFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(reply->errorString());
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (doc.isNull()) {
        emit errorOccurred("Invalid JSON response");
        reply->deleteLater();
        return;
    }
    
    parseSearchResults(reply);
    reply->deleteLater();
}

void ExtensionMarketplaceManager::onExtensionDetailsReplyFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(reply->errorString());
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (doc.isNull()) {
        emit errorOccurred("Invalid JSON response");
        reply->deleteLater();
        return;
    }
    
    parseExtensionDetails(reply);
    reply->deleteLater();
}

void ExtensionMarketplaceManager::parseSearchResults(QNetworkReply* reply) {
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();
    
    QJsonArray extensions;
    if (root.contains("results") && root["results"].isArray()) {
        QJsonArray results = root["results"].toArray();
        if (!results.isEmpty() && results.first().isObject()) {
            QJsonObject result = results.first().toObject();
            if (result.contains("extensions") && result["extensions"].isArray()) {
                extensions = result["extensions"].toArray();
            }
        }
    }
    
    // Cache the results
    QString query = reply->request().url().query();
    m_cacheStore->cacheSearchResults(query, extensions);
    
    emit searchResultsReceived(extensions);
}

void ExtensionMarketplaceManager::parseExtensionDetails(QNetworkReply* reply) {
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject extension = doc.object();
    
    QString extensionId = reply->property("extensionId").toString();
    if (!extensionId.isEmpty()) {
        m_cacheStore->cacheExtensionDetails(extensionId, extension);
    }
    
    emit extensionDetailsReceived(extension);
}

QString ExtensionMarketplaceManager::getExtensionDownloadUrl(const QString& extensionId, const QString& version) {
    // In a real implementation, this would fetch the actual download URL
    // For now, we'll return a placeholder
    return QString("https://marketplace.visualstudio.com/_apis/public/gallery/publishers/%1/extensions/%2/%3/vspackage")
            .arg(extensionId.section('.', 0, 0))
            .arg(extensionId.section('.', 1))
            .arg(version.isEmpty() ? "latest" : version);
}

bool ExtensionMarketplaceManager::isExtensionAllowed(const QString& extensionId) {
    if (m_policyEngine) {
        return m_policyEngine->isExtensionAllowed(extensionId);
    }
    return true; // No policy engine, allow all
}

void ExtensionMarketplaceManager::saveInstalledExtensions() {
    // In a real implementation, this would save to a file or database
    qDebug() << "[ExtensionMarketplaceManager] Saved" << m_installedExtensions.size() << "installed extensions";
}

void ExtensionMarketplaceManager::loadInstalledExtensions() {
    // In a real implementation, this would load from a file or database
    qDebug() << "[ExtensionMarketplaceManager] Loaded installed extensions";
}

void ExtensionMarketplaceManager::checkForUpdates() {
    // In a real implementation, this would check all installed extensions for updates
    qDebug() << "[ExtensionMarketplaceManager] Checking for updates";
}