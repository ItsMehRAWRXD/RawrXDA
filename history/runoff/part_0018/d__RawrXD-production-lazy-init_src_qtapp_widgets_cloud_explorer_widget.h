/**
 * @file cloud_explorer_widget.h
 * @brief Cloud storage explorer for AWS S3, Azure Blob, Google Cloud Storage
 * 
 * Production-ready cloud storage browser providing:
 * - Multi-provider support (AWS S3, Azure Blob, Google Cloud Storage)
 * - Bucket/container browsing with tree navigation
 * - File upload/download with progress tracking
 * - Drag-and-drop support
 * - File preview for common formats
 * - Credential management with secure storage
 * - Batch operations (copy, move, delete)
 * - Signed URL generation
 * - Cross-provider file transfer
 */

#ifndef CLOUD_EXPLORER_WIDGET_H
#define CLOUD_EXPLORER_WIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMap>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMimeData>
#include <QTimer>

/**
 * @enum CloudProvider
 * @brief Supported cloud storage providers
 */
enum class CloudProvider {
    AWS_S3,
    Azure_Blob,
    Google_Cloud_Storage,
    MinIO,  // S3-compatible
    Local   // For testing
};

/**
 * @struct CloudCredentials
 * @brief Storage credentials for cloud providers
 */
struct CloudCredentials {
    QString name;           // Profile name
    CloudProvider provider;
    QString accessKeyId;    // AWS: Access Key, Azure: Account Name
    QString secretKey;      // AWS: Secret Key, Azure: Account Key
    QString region;         // AWS region, Azure location
    QString endpoint;       // Custom endpoint for MinIO
    QString projectId;      // GCP project ID
    bool isDefault = false;
    
    QString getProviderName() const;
};

/**
 * @struct CloudBucket
 * @brief Represents a storage bucket/container
 */
struct CloudBucket {
    QString name;
    QString region;
    QDateTime creationDate;
    CloudProvider provider;
    QString credentialProfile;
    qint64 objectCount = 0;
    qint64 totalSize = 0;
    bool versioningEnabled = false;
    QString accessPolicy;  // public, private, etc.
};

/**
 * @struct CloudObject
 * @brief Represents a file/object in cloud storage
 */
struct CloudObject {
    QString key;            // Full path/key
    QString name;           // Display name (last part of key)
    QString bucket;
    qint64 size = 0;
    QDateTime lastModified;
    QString etag;
    QString contentType;
    QString storageClass;
    bool isFolder = false;
    QMap<QString, QString> metadata;
    
    QString getExtension() const;
    bool isImage() const;
    bool isText() const;
};

/**
 * @struct CloudTransfer
 * @brief Represents an ongoing transfer operation
 */
struct CloudTransfer {
    QString id;
    QString sourcePath;     // Local or cloud path
    QString destPath;       // Cloud or local path
    QString bucket;
    qint64 totalBytes = 0;
    qint64 transferredBytes = 0;
    QString status;         // queued, uploading, downloading, completed, failed
    QString errorMessage;
    QDateTime startTime;
    QDateTime endTime;
    bool isUpload = true;
    
    double progressPercent() const {
        return totalBytes > 0 ? (transferredBytes * 100.0 / totalBytes) : 0;
    }
};

/**
 * @class CloudTransferManager
 * @brief Manages file transfers with queue and progress tracking
 */
class CloudTransferManager : public QObject {
    Q_OBJECT

public:
    explicit CloudTransferManager(QObject* parent = nullptr);
    
    QString queueUpload(const QString& localPath, const QString& bucket, 
                       const QString& key, const CloudCredentials& credentials);
    QString queueDownload(const QString& bucket, const QString& key,
                         const QString& localPath, const CloudCredentials& credentials);
    
    void cancelTransfer(const QString& transferId);
    void pauseTransfer(const QString& transferId);
    void resumeTransfer(const QString& transferId);
    void clearCompleted();
    
    QList<CloudTransfer> activeTransfers() const { return m_transfers.values(); }
    CloudTransfer getTransfer(const QString& id) const { return m_transfers.value(id); }

signals:
    void transferStarted(const QString& transferId);
    void transferProgress(const QString& transferId, qint64 bytes, qint64 total);
    void transferCompleted(const QString& transferId);
    void transferFailed(const QString& transferId, const QString& error);
    void queueChanged();

public slots:
    void processQueue();
    void onUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onTransferFinished(QNetworkReply* reply);

private:
    QMap<QString, CloudTransfer> m_transfers;
    QList<QString> m_queue;
    QNetworkAccessManager* m_networkManager;
    int m_maxConcurrentTransfers = 3;
    int m_activeTransfers = 0;
};

/**
 * @class CloudExplorerWidget
 * @brief Main cloud storage explorer widget
 */
class CloudExplorerWidget : public QWidget {
    Q_OBJECT

public:
    explicit CloudExplorerWidget(QWidget* parent = nullptr);
    ~CloudExplorerWidget();
    
    // Credential management
    void addCredentials(const CloudCredentials& credentials);
    void removeCredentials(const QString& profileName);
    void setActiveCredentials(const QString& profileName);
    QList<CloudCredentials> savedCredentials() const { return m_credentials.values(); }
    
    // Bucket operations
    void refreshBuckets();
    void createBucket(const QString& name, const QString& region = QString());
    void deleteBucket(const QString& name);
    void listObjects(const QString& bucket, const QString& prefix = QString());
    
    // Object operations
    void uploadFile(const QString& localPath, const QString& bucket, const QString& key);
    void uploadFolder(const QString& localPath, const QString& bucket, const QString& prefix);
    void downloadFile(const QString& bucket, const QString& key, const QString& localPath);
    void downloadFolder(const QString& bucket, const QString& prefix, const QString& localPath);
    void deleteObject(const QString& bucket, const QString& key);
    void deleteObjects(const QString& bucket, const QStringList& keys);
    void copyObject(const QString& srcBucket, const QString& srcKey,
                   const QString& destBucket, const QString& destKey);
    void moveObject(const QString& srcBucket, const QString& srcKey,
                   const QString& destBucket, const QString& destKey);
    
    // URL generation
    QString getSignedUrl(const QString& bucket, const QString& key, int expiresInSeconds = 3600);
    QString getPublicUrl(const QString& bucket, const QString& key);
    
    // Navigation
    void navigateTo(const QString& bucket, const QString& path);
    void goBack();
    void goForward();
    void goUp();
    void refresh();

signals:
    void bucketSelected(const CloudBucket& bucket);
    void objectSelected(const CloudObject& object);
    void objectDoubleClicked(const CloudObject& object);
    void uploadStarted(const QString& localPath, const QString& key);
    void downloadStarted(const QString& key, const QString& localPath);
    void operationCompleted(const QString& operation);
    void errorOccurred(const QString& error);
    void credentialsChanged();

private slots:
    void onProviderChanged(int index);
    void onProfileChanged(int index);
    void onBucketSelected(QTreeWidgetItem* item, int column);
    void onObjectDoubleClicked(QTableWidgetItem* item);
    void onBreadcrumbClicked();
    void onSearchTextChanged(const QString& text);
    void onUploadClicked();
    void onDownloadClicked();
    void onDeleteClicked();
    void onNewFolderClicked();
    void onRefreshClicked();
    void onGenerateUrlClicked();
    void onContextMenu(const QPoint& pos);
    void onTransferProgress(const QString& id, qint64 bytes, qint64 total);
    void onTransferCompleted(const QString& id);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupUI();
    void setupConnections();
    void loadSavedCredentials();
    void saveCresentials();
    
    // S3 operations
    void s3ListBuckets();
    void s3ListObjects(const QString& bucket, const QString& prefix);
    void s3PutObject(const QString& bucket, const QString& key, const QByteArray& data);
    void s3GetObject(const QString& bucket, const QString& key);
    void s3DeleteObject(const QString& bucket, const QString& key);
    QByteArray s3Sign(const QString& method, const QString& path, 
                      const QMap<QString, QString>& headers);
    
    // Azure Blob operations
    void azureListContainers();
    void azureListBlobs(const QString& container, const QString& prefix);
    void azurePutBlob(const QString& container, const QString& blob, const QByteArray& data);
    void azureGetBlob(const QString& container, const QString& blob);
    void azureDeleteBlob(const QString& container, const QString& blob);
    
    // GCS operations
    void gcsListBuckets();
    void gcsListObjects(const QString& bucket, const QString& prefix);
    
    void updateBreadcrumb();
    void updateObjectList();
    void updateTransfersList();
    QString formatSize(qint64 bytes) const;
    QIcon getFileIcon(const CloudObject& object) const;
    
    // UI Components
    QComboBox* m_providerCombo;
    QComboBox* m_profileCombo;
    QPushButton* m_addProfileBtn;
    QPushButton* m_editProfileBtn;
    
    QTreeWidget* m_bucketsTree;
    QWidget* m_breadcrumbWidget;
    QHBoxLayout* m_breadcrumbLayout;
    QLineEdit* m_searchEdit;
    QTableWidget* m_objectsTable;
    
    QPushButton* m_uploadBtn;
    QPushButton* m_downloadBtn;
    QPushButton* m_deleteBtn;
    QPushButton* m_newFolderBtn;
    QPushButton* m_refreshBtn;
    QPushButton* m_generateUrlBtn;
    
    QGroupBox* m_transfersGroup;
    QTableWidget* m_transfersTable;
    QProgressBar* m_totalProgressBar;
    QLabel* m_transferStatusLabel;
    
    // Preview panel
    QGroupBox* m_previewGroup;
    QLabel* m_previewImage;
    QLabel* m_previewInfo;
    
    // Data
    QMap<QString, CloudCredentials> m_credentials;
    CloudCredentials m_activeCredentials;
    QMap<QString, CloudBucket> m_buckets;
    QList<CloudObject> m_currentObjects;
    QString m_currentBucket;
    QString m_currentPrefix;
    QStringList m_navigationHistory;
    int m_historyIndex = -1;
    
    CloudTransferManager* m_transferManager;
    QNetworkAccessManager* m_networkManager;
};

#endif // CLOUD_EXPLORER_WIDGET_H
