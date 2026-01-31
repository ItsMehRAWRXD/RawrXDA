#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QList>
#include <QTimer>

// Forward declarations
class VsixInstaller;
class EnterprisePolicyEngine;
class OfflineCacheStore;

/**
 * @class ExtensionMarketplaceManager
 * @brief Manages VS Code extension marketplace operations
 * 
 * This class handles:
 * - Searching and browsing extensions
 * - Fetching extension metadata
 * - Installing/updating/uninstalling extensions
 * - Managing extension lifecycle
 * - Enterprise policy enforcement
 * - Offline cache management
 */
class ExtensionMarketplaceManager : public QObject {
    Q_OBJECT

public:
    explicit ExtensionMarketplaceManager(QObject* parent = nullptr);
    ~ExtensionMarketplaceManager();

    // Marketplace search and browse
    void searchExtensions(const QString& query, int page = 1, int pageSize = 20);
    void getFeaturedExtensions(int page = 1, int pageSize = 20);
    void getCategoryExtensions(const QString& category, int page = 1, int pageSize = 20);
    void getExtensionDetails(const QString& extensionId);

    // Extension lifecycle management
    void installExtension(const QString& extensionId, const QString& version = QString());
    void updateExtension(const QString& extensionId);
    void uninstallExtension(const QString& extensionId);
    void listInstalledExtensions();

    // Enterprise features
    void setEnterprisePolicyEngine(EnterprisePolicyEngine* policyEngine);
    void enableOfflineMode(bool enabled);
    void syncWithPrivateMarketplace(const QString& url);

    // Cache management
    void clearCache();
    void preloadExtensions(const QStringList& extensionIds);

signals:
    void searchResultsReceived(const QJsonArray& extensions);
    void extensionDetailsReceived(const QJsonObject& extension);
    void installationStarted(const QString& extensionId);
    void installationCompleted(const QString& extensionId, bool success);
    void installationError(const QString& extensionId, const QString& error);
    void updateAvailable(const QString& extensionId, const QString& version);
    void uninstallCompleted(const QString& extensionId, bool success);
    void installedExtensionsList(const QJsonArray& extensions);
    void cacheCleared();
    void errorOccurred(const QString& error);

private slots:
    void onSearchReplyFinished();
    void onExtensionDetailsReplyFinished();
    void onInstallReplyFinished();
    void onUpdateCheckReplyFinished();

private:
    struct ExtensionInfo {
        QString id;
        QString name;
        QString version;
        QString publisher;
        QString description;
        QString iconUrl;
        qint64 downloadCount;
        double rating;
        QStringList categories;
        QDateTime lastUpdated;
        bool installed;
        QString installedVersion;
    };

    QNetworkAccessManager* m_networkManager;
    VsixInstaller* m_vsixInstaller;
    EnterprisePolicyEngine* m_policyEngine;
    OfflineCacheStore* m_cacheStore;
    
    QList<ExtensionInfo> m_installedExtensions;
    bool m_offlineMode;
    QString m_privateMarketplaceUrl;
    QTimer* m_updateCheckTimer;

    void parseSearchResults(QNetworkReply* reply);
    void parseExtensionDetails(QNetworkReply* reply);
    QString getExtensionDownloadUrl(const QString& extensionId, const QString& version);
    bool isExtensionAllowed(const QString& extensionId);
    void saveInstalledExtensions();
    void loadInstalledExtensions();
    void checkForUpdates();
};