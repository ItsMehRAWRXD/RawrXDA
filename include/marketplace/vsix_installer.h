#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QFile>

/**
 * @class VsixInstaller
 * @brief Handles installation of VSIX packages
 * 
 * This class manages:
 * - Downloading VSIX files from URLs
 * - Extracting VSIX packages
 * - Installing extensions to the IDE
 * - Managing installation state
 */
class VsixInstaller : public QObject {
    Q_OBJECT

public:
    explicit VsixInstaller(QObject* parent = nullptr);
    ~VsixInstaller();

    // Installation methods
    void installFromUrl(const QString& url, const QString& extensionId);
    void installFromFile(const QString& filePath);
    bool uninstallExtension(const QString& extensionId);
    
    // Utility methods
    bool isExtensionInstalled(const QString& extensionId);
    QString getExtensionInstallPath(const QString& extensionId);

signals:
    void installationStarted(const QString& extensionId);
    void installationProgress(const QString& extensionId, int percentage);
    void installationCompleted(const QString& extensionId, bool success);
    void installationError(const QString& extensionId, const QString& error);
    void uninstallCompleted(const QString& extensionId, bool success);

private slots:
    void onDownloadFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    struct InstallationInfo {
        QString extensionId;
        QString downloadUrl;
        QString tempFilePath;
        QFile* file;
    };

    QNetworkAccessManager* m_networkManager;
    QList<InstallationInfo> m_activeInstallations;

    bool extractVsixPackage(const QString& vsixPath, const QString& destination);
    bool activateExtension(const QString& extensionPath);
    bool deactivateExtension(const QString& extensionId);
    QString getExtensionsDirectory();
    void cleanupTempFiles();
};