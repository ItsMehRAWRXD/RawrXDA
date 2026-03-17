/**
 * @file cloud_explorer_widget.cpp
 * @brief Implementation of cloud storage explorer widget
 */

#include "cloud_explorer_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QStyle>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QUrlQuery>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QNetworkRequest>
#include <QHttpMultiPart>

// ============================================================================
// CloudCredentials Implementation
// ============================================================================

QString CloudCredentials::getProviderName() const {
    switch (provider) {
        case CloudProvider::AWS_S3: return "Amazon S3";
        case CloudProvider::Azure_Blob: return "Azure Blob Storage";
        case CloudProvider::Google_Cloud_Storage: return "Google Cloud Storage";
        case CloudProvider::MinIO: return "MinIO (S3 Compatible)";
        case CloudProvider::Local: return "Local Storage";
        default: return "Unknown";
    }
}

// ============================================================================
// CloudObject Implementation
// ============================================================================

QString CloudObject::getExtension() const {
    int dot = name.lastIndexOf('.');
    return dot >= 0 ? name.mid(dot + 1).toLower() : QString();
}

bool CloudObject::isImage() const {
    static QStringList imageExts = {"jpg", "jpeg", "png", "gif", "bmp", "webp", "svg", "ico"};
    return imageExts.contains(getExtension());
}

bool CloudObject::isText() const {
    static QStringList textExts = {"txt", "md", "json", "xml", "yaml", "yml", "ini", "cfg",
                                   "html", "css", "js", "ts", "py", "cpp", "h", "java"};
    return textExts.contains(getExtension());
}

// ============================================================================
// CloudTransferManager Implementation
// ============================================================================

CloudTransferManager::CloudTransferManager(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &CloudTransferManager::onTransferFinished);
}

QString CloudTransferManager::queueUpload(const QString& localPath, const QString& bucket,
                                          const QString& key, const CloudCredentials& credentials) {
    CloudTransfer transfer;
    transfer.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    transfer.sourcePath = localPath;
    transfer.destPath = QString("%1/%2").arg(bucket, key);
    transfer.bucket = bucket;
    transfer.isUpload = true;
    transfer.status = "queued";
    transfer.startTime = QDateTime::currentDateTime();
    
    QFileInfo fi(localPath);
    transfer.totalBytes = fi.size();
    
    m_transfers[transfer.id] = transfer;
    m_queue.append(transfer.id);
    
    emit queueChanged();
    QTimer::singleShot(0, this, &CloudTransferManager::processQueue);
    
    return transfer.id;
}

QString CloudTransferManager::queueDownload(const QString& bucket, const QString& key,
                                            const QString& localPath, const CloudCredentials& credentials) {
    CloudTransfer transfer;
    transfer.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    transfer.sourcePath = QString("%1/%2").arg(bucket, key);
    transfer.destPath = localPath;
    transfer.bucket = bucket;
    transfer.isUpload = false;
    transfer.status = "queued";
    transfer.startTime = QDateTime::currentDateTime();
    
    m_transfers[transfer.id] = transfer;
    m_queue.append(transfer.id);
    
    emit queueChanged();
    QTimer::singleShot(0, this, &CloudTransferManager::processQueue);
    
    return transfer.id;
}

void CloudTransferManager::cancelTransfer(const QString& transferId) {
    if (m_transfers.contains(transferId)) {
        m_transfers[transferId].status = "cancelled";
        m_queue.removeAll(transferId);
        emit queueChanged();
    }
}

void CloudTransferManager::pauseTransfer(const QString& transferId) {
    if (m_transfers.contains(transferId)) {
        m_transfers[transferId].status = "paused";
    }
}

void CloudTransferManager::resumeTransfer(const QString& transferId) {
    if (m_transfers.contains(transferId) && m_transfers[transferId].status == "paused") {
        m_transfers[transferId].status = "queued";
        m_queue.append(transferId);
        processQueue();
    }
}

void CloudTransferManager::clearCompleted() {
    QList<QString> toRemove;
    for (auto it = m_transfers.begin(); it != m_transfers.end(); ++it) {
        if (it.value().status == "completed" || it.value().status == "failed" ||
            it.value().status == "cancelled") {
            toRemove.append(it.key());
        }
    }
    for (const QString& id : toRemove) {
        m_transfers.remove(id);
    }
    emit queueChanged();
}

void CloudTransferManager::processQueue() {
    while (m_activeTransfers < m_maxConcurrentTransfers && !m_queue.isEmpty()) {
        QString transferId = m_queue.takeFirst();
        if (!m_transfers.contains(transferId)) continue;
        
        CloudTransfer& transfer = m_transfers[transferId];
        if (transfer.status == "cancelled") continue;
        
        transfer.status = transfer.isUpload ? "uploading" : "downloading";
        m_activeTransfers++;
        
        emit transferStarted(transferId);
        
        // Actual transfer would happen here using AWS/Azure/GCS SDK
        // For now, simulate with network manager
    }
}

void CloudTransferManager::onUploadProgress(qint64 bytesSent, qint64 bytesTotal) {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString transferId = reply->property("transferId").toString();
    if (m_transfers.contains(transferId)) {
        m_transfers[transferId].transferredBytes = bytesSent;
        m_transfers[transferId].totalBytes = bytesTotal;
        emit transferProgress(transferId, bytesSent, bytesTotal);
    }
}

void CloudTransferManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString transferId = reply->property("transferId").toString();
    if (m_transfers.contains(transferId)) {
        m_transfers[transferId].transferredBytes = bytesReceived;
        m_transfers[transferId].totalBytes = bytesTotal;
        emit transferProgress(transferId, bytesReceived, bytesTotal);
    }
}

void CloudTransferManager::onTransferFinished(QNetworkReply* reply) {
    QString transferId = reply->property("transferId").toString();
    
    if (m_transfers.contains(transferId)) {
        CloudTransfer& transfer = m_transfers[transferId];
        m_activeTransfers--;
        
        if (reply->error() == QNetworkReply::NoError) {
            transfer.status = "completed";
            transfer.endTime = QDateTime::currentDateTime();
            emit transferCompleted(transferId);
        } else {
            transfer.status = "failed";
            transfer.errorMessage = reply->errorString();
            emit transferFailed(transferId, reply->errorString());
        }
        
        emit queueChanged();
        processQueue();
    }
    
    reply->deleteLater();
}

// ============================================================================
// CloudExplorerWidget Implementation
// ============================================================================

CloudExplorerWidget::CloudExplorerWidget(QWidget* parent)
    : QWidget(parent)
    , m_transferManager(new CloudTransferManager(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    setupUI();
    setupConnections();
    loadSavedCredentials();
    
    setAcceptDrops(true);
}

CloudExplorerWidget::~CloudExplorerWidget() {
    saveCresentials();
}

void CloudExplorerWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    
    // Provider and profile selection
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    m_providerCombo = new QComboBox();
    m_providerCombo->addItem("Amazon S3", static_cast<int>(CloudProvider::AWS_S3));
    m_providerCombo->addItem("Azure Blob", static_cast<int>(CloudProvider::Azure_Blob));
    m_providerCombo->addItem("Google Cloud Storage", static_cast<int>(CloudProvider::Google_Cloud_Storage));
    m_providerCombo->addItem("MinIO", static_cast<int>(CloudProvider::MinIO));
    
    m_profileCombo = new QComboBox();
    m_profileCombo->setMinimumWidth(150);
    
    m_addProfileBtn = new QPushButton(tr("Add Profile"));
    m_editProfileBtn = new QPushButton(tr("Edit"));
    
    headerLayout->addWidget(new QLabel(tr("Provider:")));
    headerLayout->addWidget(m_providerCombo);
    headerLayout->addWidget(new QLabel(tr("Profile:")));
    headerLayout->addWidget(m_profileCombo, 1);
    headerLayout->addWidget(m_addProfileBtn);
    headerLayout->addWidget(m_editProfileBtn);
    
    mainLayout->addLayout(headerLayout);
    
    // Main splitter
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal);
    
    // Left panel - Buckets tree
    QWidget* leftPanel = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* bucketsLabel = new QLabel(tr("Buckets"));
    bucketsLabel->setStyleSheet("font-weight: bold;");
    
    m_bucketsTree = new QTreeWidget();
    m_bucketsTree->setHeaderHidden(true);
    m_bucketsTree->setRootIsDecorated(true);
    
    leftLayout->addWidget(bucketsLabel);
    leftLayout->addWidget(m_bucketsTree);
    
    mainSplitter->addWidget(leftPanel);
    
    // Right panel - Objects
    QWidget* rightPanel = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    // Breadcrumb navigation
    m_breadcrumbWidget = new QWidget();
    m_breadcrumbLayout = new QHBoxLayout(m_breadcrumbWidget);
    m_breadcrumbLayout->setContentsMargins(0, 0, 0, 0);
    m_breadcrumbLayout->setSpacing(2);
    
    // Toolbar
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search objects..."));
    m_searchEdit->setClearButtonEnabled(true);
    
    m_uploadBtn = new QPushButton(tr("Upload"));
    m_uploadBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_downloadBtn = new QPushButton(tr("Download"));
    m_downloadBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    m_deleteBtn = new QPushButton(tr("Delete"));
    m_deleteBtn->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    m_newFolderBtn = new QPushButton(tr("New Folder"));
    m_newFolderBtn->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    m_refreshBtn = new QPushButton(tr("Refresh"));
    m_refreshBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_generateUrlBtn = new QPushButton(tr("Get URL"));
    
    toolbarLayout->addWidget(m_searchEdit, 1);
    toolbarLayout->addWidget(m_uploadBtn);
    toolbarLayout->addWidget(m_downloadBtn);
    toolbarLayout->addWidget(m_newFolderBtn);
    toolbarLayout->addWidget(m_deleteBtn);
    toolbarLayout->addWidget(m_generateUrlBtn);
    toolbarLayout->addWidget(m_refreshBtn);
    
    // Objects table
    m_objectsTable = new QTableWidget();
    m_objectsTable->setColumnCount(5);
    m_objectsTable->setHorizontalHeaderLabels({
        tr("Name"), tr("Size"), tr("Type"), tr("Last Modified"), tr("Storage Class")
    });
    m_objectsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_objectsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_objectsTable->setAlternatingRowColors(true);
    m_objectsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_objectsTable->horizontalHeader()->setStretchLastSection(false);
    m_objectsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_objectsTable->verticalHeader()->setVisible(false);
    m_objectsTable->setSortingEnabled(true);
    
    rightLayout->addWidget(m_breadcrumbWidget);
    rightLayout->addLayout(toolbarLayout);
    rightLayout->addWidget(m_objectsTable, 1);
    
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setSizes({200, 600});
    
    mainLayout->addWidget(mainSplitter, 1);
    
    // Transfers panel at bottom
    m_transfersGroup = new QGroupBox(tr("Transfers"));
    QVBoxLayout* transfersLayout = new QVBoxLayout(m_transfersGroup);
    
    m_transfersTable = new QTableWidget();
    m_transfersTable->setColumnCount(5);
    m_transfersTable->setHorizontalHeaderLabels({
        tr("File"), tr("Direction"), tr("Progress"), tr("Size"), tr("Status")
    });
    m_transfersTable->setMaximumHeight(150);
    m_transfersTable->horizontalHeader()->setStretchLastSection(false);
    m_transfersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_transfersTable->verticalHeader()->setVisible(false);
    
    QHBoxLayout* transferStatusLayout = new QHBoxLayout();
    m_totalProgressBar = new QProgressBar();
    m_totalProgressBar->setRange(0, 100);
    m_transferStatusLabel = new QLabel(tr("No active transfers"));
    
    QPushButton* clearTransfersBtn = new QPushButton(tr("Clear Completed"));
    connect(clearTransfersBtn, &QPushButton::clicked, m_transferManager, &CloudTransferManager::clearCompleted);
    
    transferStatusLayout->addWidget(m_transferStatusLabel);
    transferStatusLayout->addWidget(m_totalProgressBar, 1);
    transferStatusLayout->addWidget(clearTransfersBtn);
    
    transfersLayout->addWidget(m_transfersTable);
    transfersLayout->addLayout(transferStatusLayout);
    
    mainLayout->addWidget(m_transfersGroup);
    
    // Initially disable most controls
    m_uploadBtn->setEnabled(false);
    m_downloadBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_newFolderBtn->setEnabled(false);
    m_generateUrlBtn->setEnabled(false);
}

void CloudExplorerWidget::setupConnections() {
    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CloudExplorerWidget::onProviderChanged);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CloudExplorerWidget::onProfileChanged);
    
    connect(m_bucketsTree, &QTreeWidget::itemClicked,
            this, &CloudExplorerWidget::onBucketSelected);
    connect(m_objectsTable, &QTableWidget::itemDoubleClicked,
            this, &CloudExplorerWidget::onObjectDoubleClicked);
    connect(m_objectsTable, &QTableWidget::customContextMenuRequested,
            this, &CloudExplorerWidget::onContextMenu);
    
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &CloudExplorerWidget::onSearchTextChanged);
    
    connect(m_uploadBtn, &QPushButton::clicked, this, &CloudExplorerWidget::onUploadClicked);
    connect(m_downloadBtn, &QPushButton::clicked, this, &CloudExplorerWidget::onDownloadClicked);
    connect(m_deleteBtn, &QPushButton::clicked, this, &CloudExplorerWidget::onDeleteClicked);
    connect(m_newFolderBtn, &QPushButton::clicked, this, &CloudExplorerWidget::onNewFolderClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &CloudExplorerWidget::onRefreshClicked);
    connect(m_generateUrlBtn, &QPushButton::clicked, this, &CloudExplorerWidget::onGenerateUrlClicked);
    
    connect(m_addProfileBtn, &QPushButton::clicked, [this]() {
        // Show add profile dialog
        QString name = QInputDialog::getText(this, tr("Add Profile"), tr("Profile name:"));
        if (!name.isEmpty()) {
            CloudCredentials creds;
            creds.name = name;
            creds.provider = static_cast<CloudProvider>(m_providerCombo->currentData().toInt());
            
            // In a real implementation, show a proper credentials dialog
            creds.accessKeyId = QInputDialog::getText(this, tr("Access Key"), tr("Access Key ID:"));
            creds.secretKey = QInputDialog::getText(this, tr("Secret Key"), tr("Secret Access Key:"),
                                                    QLineEdit::Password);
            creds.region = QInputDialog::getText(this, tr("Region"), tr("Region (e.g., us-east-1):"));
            
            addCredentials(creds);
        }
    });
    
    connect(m_transferManager, &CloudTransferManager::transferProgress,
            this, &CloudExplorerWidget::onTransferProgress);
    connect(m_transferManager, &CloudTransferManager::transferCompleted,
            this, &CloudExplorerWidget::onTransferCompleted);
    connect(m_transferManager, &CloudTransferManager::queueChanged,
            this, &CloudExplorerWidget::updateTransfersList);
}

void CloudExplorerWidget::loadSavedCredentials() {
    QSettings settings;
    settings.beginGroup("CloudExplorer");
    
    int count = settings.beginReadArray("Credentials");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        
        CloudCredentials creds;
        creds.name = settings.value("name").toString();
        creds.provider = static_cast<CloudProvider>(settings.value("provider").toInt());
        creds.accessKeyId = settings.value("accessKeyId").toString();
        // In production, use secure credential storage (e.g., OS keychain)
        creds.secretKey = settings.value("secretKey").toString();
        creds.region = settings.value("region").toString();
        creds.endpoint = settings.value("endpoint").toString();
        creds.isDefault = settings.value("isDefault").toBool();
        
        m_credentials[creds.name] = creds;
        m_profileCombo->addItem(creds.name, creds.name);
    }
    settings.endArray();
    settings.endGroup();
    
    // Select default profile
    for (const auto& cred : m_credentials) {
        if (cred.isDefault) {
            int index = m_profileCombo->findData(cred.name);
            if (index >= 0) {
                m_profileCombo->setCurrentIndex(index);
            }
            break;
        }
    }
}

void CloudExplorerWidget::saveCresentials() {
    QSettings settings;
    settings.beginGroup("CloudExplorer");
    
    settings.beginWriteArray("Credentials", m_credentials.size());
    int i = 0;
    for (const auto& cred : m_credentials) {
        settings.setArrayIndex(i++);
        settings.setValue("name", cred.name);
        settings.setValue("provider", static_cast<int>(cred.provider));
        settings.setValue("accessKeyId", cred.accessKeyId);
        // In production, use secure credential storage
        settings.setValue("secretKey", cred.secretKey);
        settings.setValue("region", cred.region);
        settings.setValue("endpoint", cred.endpoint);
        settings.setValue("isDefault", cred.isDefault);
    }
    settings.endArray();
    settings.endGroup();
}

void CloudExplorerWidget::addCredentials(const CloudCredentials& credentials) {
    m_credentials[credentials.name] = credentials;
    
    if (m_profileCombo->findData(credentials.name) < 0) {
        m_profileCombo->addItem(credentials.name, credentials.name);
    }
    
    saveCresentials();
    emit credentialsChanged();
}

void CloudExplorerWidget::removeCredentials(const QString& profileName) {
    m_credentials.remove(profileName);
    
    int index = m_profileCombo->findData(profileName);
    if (index >= 0) {
        m_profileCombo->removeItem(index);
    }
    
    saveCresentials();
    emit credentialsChanged();
}

void CloudExplorerWidget::setActiveCredentials(const QString& profileName) {
    if (m_credentials.contains(profileName)) {
        m_activeCredentials = m_credentials[profileName];
        refreshBuckets();
    }
}

void CloudExplorerWidget::refreshBuckets() {
    m_bucketsTree->clear();
    
    if (m_activeCredentials.name.isEmpty()) {
        return;
    }
    
    switch (m_activeCredentials.provider) {
        case CloudProvider::AWS_S3:
        case CloudProvider::MinIO:
            s3ListBuckets();
            break;
        case CloudProvider::Azure_Blob:
            azureListContainers();
            break;
        case CloudProvider::Google_Cloud_Storage:
            gcsListBuckets();
            break;
        default:
            break;
    }
}

void CloudExplorerWidget::s3ListBuckets() {
    // In production, use AWS SDK or proper signing
    // This is a simplified example
    
    QString endpoint = m_activeCredentials.endpoint.isEmpty() 
        ? QString("https://s3.%1.amazonaws.com").arg(m_activeCredentials.region)
        : m_activeCredentials.endpoint;
    
    QUrl requestUrl(endpoint);
    QNetworkRequest request{requestUrl};
    request.setRawHeader("Host", requestUrl.host().toUtf8());
    
    // Add AWS signature headers here
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            // Parse XML response and populate buckets
            QString response = reply->readAll();
            // Parse S3 ListBuckets XML response
            
            // For demo, add sample buckets
            QTreeWidgetItem* item = new QTreeWidgetItem(m_bucketsTree);
            item->setText(0, "sample-bucket");
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            item->setData(0, Qt::UserRole, "sample-bucket");
        } else {
            emit errorOccurred(reply->errorString());
        }
        reply->deleteLater();
    });
}

void CloudExplorerWidget::s3ListObjects(const QString& bucket, const QString& prefix) {
    m_currentBucket = bucket;
    m_currentPrefix = prefix;
    
    // S3 ListObjects API call
    QString endpoint = m_activeCredentials.endpoint.isEmpty()
        ? QString("https://%1.s3.%2.amazonaws.com").arg(bucket, m_activeCredentials.region)
        : QString("%1/%2").arg(m_activeCredentials.endpoint, bucket);
    
    QUrl url(endpoint);
    QUrlQuery query;
    query.addQueryItem("list-type", "2");
    query.addQueryItem("delimiter", "/");
    if (!prefix.isEmpty()) {
        query.addQueryItem("prefix", prefix);
    }
    url.setQuery(query);
    
    QNetworkRequest request(url);
    // Add AWS signature headers
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            // Parse S3 ListObjectsV2 XML response
            QString response = reply->readAll();
            // Parse and update m_currentObjects
            updateObjectList();
        } else {
            emit errorOccurred(reply->errorString());
        }
        reply->deleteLater();
    });
}

void CloudExplorerWidget::s3PutObject(const QString& bucket, const QString& key, const QByteArray& data) {
    QString endpoint = m_activeCredentials.endpoint.isEmpty()
        ? QString("https://%1.s3.%2.amazonaws.com/%3").arg(bucket, m_activeCredentials.region, key)
        : QString("%1/%2/%3").arg(m_activeCredentials.endpoint, bucket, key);
    
    QUrl requestUrl(endpoint);
    QNetworkRequest request{requestUrl};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    // Add AWS signature headers
    
    QNetworkReply* reply = m_networkManager->put(request, data);
    connect(reply, &QNetworkReply::uploadProgress, m_transferManager, &CloudTransferManager::onUploadProgress);
}

void CloudExplorerWidget::s3GetObject(const QString& bucket, const QString& key) {
    QString endpoint = m_activeCredentials.endpoint.isEmpty()
        ? QString("https://%1.s3.%2.amazonaws.com/%3").arg(bucket, m_activeCredentials.region, key)
        : QString("%1/%2/%3").arg(m_activeCredentials.endpoint, bucket, key);
    
    QUrl requestUrl(endpoint);
    QNetworkRequest request{requestUrl};
    // Add AWS signature headers
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::downloadProgress, m_transferManager, &CloudTransferManager::onDownloadProgress);
}

void CloudExplorerWidget::s3DeleteObject(const QString& bucket, const QString& key) {
    QString endpoint = m_activeCredentials.endpoint.isEmpty()
        ? QString("https://%1.s3.%2.amazonaws.com/%3").arg(bucket, m_activeCredentials.region, key)
        : QString("%1/%2/%3").arg(m_activeCredentials.endpoint, bucket, key);
    
    QUrl requestUrl(endpoint);
    QNetworkRequest request{requestUrl};
    // Add AWS signature headers
    
    m_networkManager->deleteResource(request);
}

QByteArray CloudExplorerWidget::s3Sign(const QString& method, const QString& path,
                                       const QMap<QString, QString>& headers) {
    // AWS Signature Version 4
    // This is a simplified version - production code should use AWS SDK
    
    QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmssZ");
    QString datestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd");
    
    QString scope = QString("%1/%2/s3/aws4_request")
        .arg(datestamp, m_activeCredentials.region);
    
    // Create canonical request
    QString canonicalRequest = QString("%1\n%2\n\nhost:%3\nx-amz-date:%4\n\nhost;x-amz-date\n%5")
        .arg(method, path, headers["Host"], timestamp, "UNSIGNED-PAYLOAD");
    
    // Create string to sign
    QByteArray hashedRequest = QCryptographicHash::hash(
        canonicalRequest.toUtf8(), QCryptographicHash::Sha256).toHex();
    
    QString stringToSign = QString("AWS4-HMAC-SHA256\n%1\n%2\n%3")
        .arg(timestamp, scope, QString(hashedRequest));
    
    // Calculate signature
    // ... HMAC-SHA256 chain with secret key
    
    return QByteArray();  // Return signature
}

void CloudExplorerWidget::azureListContainers() {
    QString endpoint = QString("https://%1.blob.core.windows.net/?comp=list")
        .arg(m_activeCredentials.accessKeyId);
    
    QUrl requestUrl(endpoint);
    QNetworkRequest request{requestUrl};
    // Add Azure Shared Key authorization header
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            // Parse Azure ListContainers XML response
        } else {
            emit errorOccurred(reply->errorString());
        }
        reply->deleteLater();
    });
}

void CloudExplorerWidget::azureListBlobs(const QString& container, const QString& prefix) {
    QString endpoint = QString("https://%1.blob.core.windows.net/%2?restype=container&comp=list")
        .arg(m_activeCredentials.accessKeyId, container);
    
    if (!prefix.isEmpty()) {
        endpoint += QString("&prefix=%1").arg(prefix);
    }
    
    QUrl requestUrl(endpoint);
    QNetworkRequest request{requestUrl};
    // Add Azure authorization
    
    m_networkManager->get(request);
}

void CloudExplorerWidget::azurePutBlob(const QString& container, const QString& blob, const QByteArray& data) {
    QString endpoint = QString("https://%1.blob.core.windows.net/%2/%3")
        .arg(m_activeCredentials.accessKeyId, container, blob);
    
    QUrl requestUrl(endpoint);
    QNetworkRequest request{requestUrl};
    request.setRawHeader("x-ms-blob-type", "BlockBlob");
    // Add Azure authorization
    
    m_networkManager->put(request, data);
}

void CloudExplorerWidget::azureGetBlob(const QString& container, const QString& blob) {
    QString endpoint = QString("https://%1.blob.core.windows.net/%2/%3")
        .arg(m_activeCredentials.accessKeyId, container, blob);
    
    QUrl requestUrl(endpoint);
    QNetworkRequest request{requestUrl};
    m_networkManager->get(request);
}

void CloudExplorerWidget::azureDeleteBlob(const QString& container, const QString& blob) {
    QString endpoint = QString("https://%1.blob.core.windows.net/%2/%3")
        .arg(m_activeCredentials.accessKeyId, container, blob);
    
    QUrl requestUrl(endpoint);
    QNetworkRequest request{requestUrl};
    m_networkManager->deleteResource(request);
}

void CloudExplorerWidget::gcsListBuckets() {
    QString endpoint = QString("https://storage.googleapis.com/storage/v1/b?project=%1")
        .arg(m_activeCredentials.projectId);
    
    QUrl requestUrl(endpoint);
    QNetworkRequest request{requestUrl};
    // Add OAuth2 bearer token
    
    m_networkManager->get(request);
}

void CloudExplorerWidget::gcsListObjects(const QString& bucket, const QString& prefix) {
    QString endpoint = QString("https://storage.googleapis.com/storage/v1/b/%1/o").arg(bucket);
    
    QUrl url(endpoint);
    QUrlQuery query;
    query.addQueryItem("delimiter", "/");
    if (!prefix.isEmpty()) {
        query.addQueryItem("prefix", prefix);
    }
    url.setQuery(query);
    
    QNetworkRequest request(url);
    m_networkManager->get(request);
}

void CloudExplorerWidget::listObjects(const QString& bucket, const QString& prefix) {
    switch (m_activeCredentials.provider) {
        case CloudProvider::AWS_S3:
        case CloudProvider::MinIO:
            s3ListObjects(bucket, prefix);
            break;
        case CloudProvider::Azure_Blob:
            azureListBlobs(bucket, prefix);
            break;
        case CloudProvider::Google_Cloud_Storage:
            gcsListObjects(bucket, prefix);
            break;
        default:
            break;
    }
}

void CloudExplorerWidget::uploadFile(const QString& localPath, const QString& bucket, const QString& key) {
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Could not open file: %1").arg(localPath));
        return;
    }
    
    m_transferManager->queueUpload(localPath, bucket, key, m_activeCredentials);
    emit uploadStarted(localPath, key);
}

void CloudExplorerWidget::uploadFolder(const QString& localPath, const QString& bucket, const QString& prefix) {
    QDir dir(localPath);
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QFileInfo& fi : files) {
        QString key = prefix + fi.fileName();
        if (fi.isDir()) {
            uploadFolder(fi.absoluteFilePath(), bucket, key + "/");
        } else {
            uploadFile(fi.absoluteFilePath(), bucket, key);
        }
    }
}

void CloudExplorerWidget::downloadFile(const QString& bucket, const QString& key, const QString& localPath) {
    m_transferManager->queueDownload(bucket, key, localPath, m_activeCredentials);
    emit downloadStarted(key, localPath);
}

void CloudExplorerWidget::downloadFolder(const QString& bucket, const QString& prefix, const QString& localPath) {
    // List objects with prefix and download each
    QDir().mkpath(localPath);
    
    for (const auto& obj : m_currentObjects) {
        if (obj.key.startsWith(prefix)) {
            QString relativePath = obj.key.mid(prefix.length());
            QString destPath = localPath + "/" + relativePath;
            
            if (obj.isFolder) {
                downloadFolder(bucket, obj.key, destPath);
            } else {
                QFileInfo fi(destPath);
                QDir().mkpath(fi.absolutePath());
                downloadFile(bucket, obj.key, destPath);
            }
        }
    }
}

void CloudExplorerWidget::deleteObject(const QString& bucket, const QString& key) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Delete Object"),
        tr("Are you sure you want to delete '%1'?").arg(key),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) return;
    
    switch (m_activeCredentials.provider) {
        case CloudProvider::AWS_S3:
        case CloudProvider::MinIO:
            s3DeleteObject(bucket, key);
            break;
        case CloudProvider::Azure_Blob:
            azureDeleteBlob(bucket, key);
            break;
        default:
            break;
    }
    
    refresh();
}

void CloudExplorerWidget::deleteObjects(const QString& bucket, const QStringList& keys) {
    for (const QString& key : keys) {
        // Batch delete
        switch (m_activeCredentials.provider) {
            case CloudProvider::AWS_S3:
            case CloudProvider::MinIO:
                s3DeleteObject(bucket, key);
                break;
            case CloudProvider::Azure_Blob:
                azureDeleteBlob(bucket, key);
                break;
            default:
                break;
        }
    }
    refresh();
}

QString CloudExplorerWidget::getSignedUrl(const QString& bucket, const QString& key, int expiresInSeconds) {
    // Generate pre-signed URL for temporary access
    QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmssZ");
    
    // Build signed URL with expiration
    QString url = QString("https://%1.s3.%2.amazonaws.com/%3")
        .arg(bucket, m_activeCredentials.region, key);
    
    // Add signature query parameters
    // This is simplified - use AWS SDK for proper signing
    
    return url;
}

QString CloudExplorerWidget::getPublicUrl(const QString& bucket, const QString& key) {
    switch (m_activeCredentials.provider) {
        case CloudProvider::AWS_S3:
            return QString("https://%1.s3.%2.amazonaws.com/%3")
                .arg(bucket, m_activeCredentials.region, key);
        case CloudProvider::Azure_Blob:
            return QString("https://%1.blob.core.windows.net/%2/%3")
                .arg(m_activeCredentials.accessKeyId, bucket, key);
        case CloudProvider::Google_Cloud_Storage:
            return QString("https://storage.googleapis.com/%1/%2").arg(bucket, key);
        default:
            return QString();
    }
}

void CloudExplorerWidget::navigateTo(const QString& bucket, const QString& path) {
    m_currentBucket = bucket;
    m_currentPrefix = path;
    
    // Update history
    if (m_historyIndex < m_navigationHistory.size() - 1) {
        m_navigationHistory = m_navigationHistory.mid(0, m_historyIndex + 1);
    }
    m_navigationHistory.append(QString("%1/%2").arg(bucket, path));
    m_historyIndex = m_navigationHistory.size() - 1;
    
    listObjects(bucket, path);
    updateBreadcrumb();
}

void CloudExplorerWidget::goBack() {
    if (m_historyIndex > 0) {
        m_historyIndex--;
        QString location = m_navigationHistory[m_historyIndex];
        // Parse bucket and path
        int slash = location.indexOf('/');
        m_currentBucket = location.left(slash);
        m_currentPrefix = location.mid(slash + 1);
        listObjects(m_currentBucket, m_currentPrefix);
        updateBreadcrumb();
    }
}

void CloudExplorerWidget::goForward() {
    if (m_historyIndex < m_navigationHistory.size() - 1) {
        m_historyIndex++;
        QString location = m_navigationHistory[m_historyIndex];
        int slash = location.indexOf('/');
        m_currentBucket = location.left(slash);
        m_currentPrefix = location.mid(slash + 1);
        listObjects(m_currentBucket, m_currentPrefix);
        updateBreadcrumb();
    }
}

void CloudExplorerWidget::goUp() {
    if (m_currentPrefix.isEmpty()) return;
    
    // Remove last path component
    QString newPrefix = m_currentPrefix;
    if (newPrefix.endsWith('/')) {
        newPrefix.chop(1);
    }
    int lastSlash = newPrefix.lastIndexOf('/');
    newPrefix = lastSlash >= 0 ? newPrefix.left(lastSlash + 1) : QString();
    
    navigateTo(m_currentBucket, newPrefix);
}

void CloudExplorerWidget::refresh() {
    if (!m_currentBucket.isEmpty()) {
        listObjects(m_currentBucket, m_currentPrefix);
    }
}

void CloudExplorerWidget::updateBreadcrumb() {
    // Clear existing breadcrumbs
    QLayoutItem* item;
    while ((item = m_breadcrumbLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    
    // Add bucket
    QPushButton* bucketBtn = new QPushButton(m_currentBucket);
    bucketBtn->setFlat(true);
    bucketBtn->setCursor(Qt::PointingHandCursor);
    connect(bucketBtn, &QPushButton::clicked, [this]() {
        navigateTo(m_currentBucket, QString());
    });
    m_breadcrumbLayout->addWidget(bucketBtn);
    
    // Add path components
    QStringList parts = m_currentPrefix.split('/', Qt::SkipEmptyParts);
    QString path;
    for (int i = 0; i < parts.size(); ++i) {
        m_breadcrumbLayout->addWidget(new QLabel("/"));
        
        path += parts[i] + "/";
        QString currentPath = path;  // Capture for lambda
        
        QPushButton* partBtn = new QPushButton(parts[i]);
        partBtn->setFlat(true);
        partBtn->setCursor(Qt::PointingHandCursor);
        connect(partBtn, &QPushButton::clicked, [this, currentPath]() {
            navigateTo(m_currentBucket, currentPath);
        });
        m_breadcrumbLayout->addWidget(partBtn);
    }
    
    m_breadcrumbLayout->addStretch();
}

void CloudExplorerWidget::updateObjectList() {
    m_objectsTable->setRowCount(0);
    
    // Add parent directory if not at root
    if (!m_currentPrefix.isEmpty()) {
        int row = m_objectsTable->rowCount();
        m_objectsTable->insertRow(row);
        
        QTableWidgetItem* nameItem = new QTableWidgetItem("..");
        nameItem->setIcon(style()->standardIcon(QStyle::SP_FileDialogToParent));
        nameItem->setData(Qt::UserRole, "..");
        m_objectsTable->setItem(row, 0, nameItem);
    }
    
    for (const CloudObject& obj : m_currentObjects) {
        int row = m_objectsTable->rowCount();
        m_objectsTable->insertRow(row);
        
        QTableWidgetItem* nameItem = new QTableWidgetItem(obj.name);
        nameItem->setIcon(getFileIcon(obj));
        nameItem->setData(Qt::UserRole, obj.key);
        nameItem->setData(Qt::UserRole + 1, obj.isFolder);
        
        m_objectsTable->setItem(row, 0, nameItem);
        m_objectsTable->setItem(row, 1, new QTableWidgetItem(
            obj.isFolder ? "" : formatSize(obj.size)));
        m_objectsTable->setItem(row, 2, new QTableWidgetItem(obj.contentType));
        m_objectsTable->setItem(row, 3, new QTableWidgetItem(
            obj.lastModified.toString("yyyy-MM-dd HH:mm:ss")));
        m_objectsTable->setItem(row, 4, new QTableWidgetItem(obj.storageClass));
    }
}

void CloudExplorerWidget::updateTransfersList() {
    m_transfersTable->setRowCount(0);
    
    auto transfers = m_transferManager->activeTransfers();
    
    qint64 totalBytes = 0;
    qint64 transferredBytes = 0;
    
    for (const CloudTransfer& transfer : transfers) {
        int row = m_transfersTable->rowCount();
        m_transfersTable->insertRow(row);
        
        QString name = transfer.isUpload ? transfer.sourcePath : transfer.destPath;
        QFileInfo fi(name);
        
        m_transfersTable->setItem(row, 0, new QTableWidgetItem(fi.fileName()));
        m_transfersTable->setItem(row, 1, new QTableWidgetItem(
            transfer.isUpload ? "↑ Upload" : "↓ Download"));
        
        QProgressBar* progress = new QProgressBar();
        progress->setRange(0, 100);
        progress->setValue(static_cast<int>(transfer.progressPercent()));
        m_transfersTable->setCellWidget(row, 2, progress);
        
        m_transfersTable->setItem(row, 3, new QTableWidgetItem(
            QString("%1 / %2").arg(formatSize(transfer.transferredBytes), 
                                   formatSize(transfer.totalBytes))));
        m_transfersTable->setItem(row, 4, new QTableWidgetItem(transfer.status));
        
        if (transfer.status == "uploading" || transfer.status == "downloading") {
            totalBytes += transfer.totalBytes;
            transferredBytes += transfer.transferredBytes;
        }
    }
    
    if (totalBytes > 0) {
        m_totalProgressBar->setValue(static_cast<int>(transferredBytes * 100 / totalBytes));
        m_transferStatusLabel->setText(tr("%1 active transfer(s)").arg(transfers.size()));
    } else {
        m_totalProgressBar->setValue(0);
        m_transferStatusLabel->setText(tr("No active transfers"));
    }
}

QString CloudExplorerWidget::formatSize(qint64 bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;
    
    while (size >= 1024 && unit < 4) {
        size /= 1024;
        unit++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', unit > 0 ? 1 : 0).arg(units[unit]);
}

QIcon CloudExplorerWidget::getFileIcon(const CloudObject& object) const {
    if (object.isFolder) {
        return style()->standardIcon(QStyle::SP_DirIcon);
    }
    
    QString ext = object.getExtension();
    
    if (object.isImage()) {
        return style()->standardIcon(QStyle::SP_FileIcon);  // Would use image icon
    }
    
    return style()->standardIcon(QStyle::SP_FileIcon);
}

// Slot implementations

void CloudExplorerWidget::onProviderChanged(int index) {
    Q_UNUSED(index);
    // Filter profiles by provider
    m_profileCombo->clear();
    
    CloudProvider provider = static_cast<CloudProvider>(m_providerCombo->currentData().toInt());
    for (const auto& cred : m_credentials) {
        if (cred.provider == provider) {
            m_profileCombo->addItem(cred.name, cred.name);
        }
    }
}

void CloudExplorerWidget::onProfileChanged(int index) {
    if (index < 0) return;
    
    QString profileName = m_profileCombo->currentData().toString();
    setActiveCredentials(profileName);
}

void CloudExplorerWidget::onBucketSelected(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (!item) return;
    
    QString bucketName = item->data(0, Qt::UserRole).toString();
    navigateTo(bucketName, QString());
    
    m_uploadBtn->setEnabled(true);
    m_newFolderBtn->setEnabled(true);
}

void CloudExplorerWidget::onObjectDoubleClicked(QTableWidgetItem* item) {
    if (!item) return;
    
    int row = item->row();
    QTableWidgetItem* nameItem = m_objectsTable->item(row, 0);
    QString key = nameItem->data(Qt::UserRole).toString();
    bool isFolder = nameItem->data(Qt::UserRole + 1).toBool();
    
    if (key == "..") {
        goUp();
    } else if (isFolder) {
        navigateTo(m_currentBucket, key);
    } else {
        // Find object and emit signal
        for (const auto& obj : m_currentObjects) {
            if (obj.key == key) {
                emit objectDoubleClicked(obj);
                break;
            }
        }
    }
}

void CloudExplorerWidget::onBreadcrumbClicked() {
    // Handled by individual breadcrumb button connections
}

void CloudExplorerWidget::onSearchTextChanged(const QString& text) {
    // Filter visible objects
    for (int row = 0; row < m_objectsTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_objectsTable->item(row, 0);
        bool match = text.isEmpty() || item->text().contains(text, Qt::CaseInsensitive);
        m_objectsTable->setRowHidden(row, !match);
    }
}

void CloudExplorerWidget::onUploadClicked() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Select Files to Upload"));
    
    if (files.isEmpty()) return;
    
    for (const QString& file : files) {
        QFileInfo fi(file);
        QString key = m_currentPrefix + fi.fileName();
        uploadFile(file, m_currentBucket, key);
    }
}

void CloudExplorerWidget::onDownloadClicked() {
    QList<QTableWidgetItem*> selected = m_objectsTable->selectedItems();
    if (selected.isEmpty()) return;
    
    QString destDir = QFileDialog::getExistingDirectory(this, tr("Select Download Location"));
    if (destDir.isEmpty()) return;
    
    QSet<int> processedRows;
    for (QTableWidgetItem* item : selected) {
        int row = item->row();
        if (processedRows.contains(row)) continue;
        processedRows.insert(row);
        
        QTableWidgetItem* nameItem = m_objectsTable->item(row, 0);
        QString key = nameItem->data(Qt::UserRole).toString();
        bool isFolder = nameItem->data(Qt::UserRole + 1).toBool();
        
        if (isFolder) {
            downloadFolder(m_currentBucket, key, destDir + "/" + nameItem->text());
        } else {
            downloadFile(m_currentBucket, key, destDir + "/" + nameItem->text());
        }
    }
}

void CloudExplorerWidget::onDeleteClicked() {
    QList<QTableWidgetItem*> selected = m_objectsTable->selectedItems();
    if (selected.isEmpty()) return;
    
    QStringList keys;
    QSet<int> processedRows;
    for (QTableWidgetItem* item : selected) {
        int row = item->row();
        if (processedRows.contains(row)) continue;
        processedRows.insert(row);
        
        QTableWidgetItem* nameItem = m_objectsTable->item(row, 0);
        QString key = nameItem->data(Qt::UserRole).toString();
        if (key != "..") {
            keys.append(key);
        }
    }
    
    if (keys.isEmpty()) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Delete Objects"),
        tr("Are you sure you want to delete %1 object(s)?").arg(keys.size()),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        deleteObjects(m_currentBucket, keys);
    }
}

void CloudExplorerWidget::onNewFolderClicked() {
    QString name = QInputDialog::getText(this, tr("New Folder"), tr("Folder name:"));
    if (name.isEmpty()) return;
    
    QString key = m_currentPrefix + name + "/";
    
    // Create empty object with trailing slash (folder marker)
    switch (m_activeCredentials.provider) {
        case CloudProvider::AWS_S3:
        case CloudProvider::MinIO:
            s3PutObject(m_currentBucket, key, QByteArray());
            break;
        case CloudProvider::Azure_Blob:
            azurePutBlob(m_currentBucket, key, QByteArray());
            break;
        default:
            break;
    }
    
    refresh();
}

void CloudExplorerWidget::onRefreshClicked() {
    refresh();
}

void CloudExplorerWidget::onGenerateUrlClicked() {
    QList<QTableWidgetItem*> selected = m_objectsTable->selectedItems();
    if (selected.isEmpty()) return;
    
    QTableWidgetItem* nameItem = m_objectsTable->item(selected.first()->row(), 0);
    QString key = nameItem->data(Qt::UserRole).toString();
    bool isFolder = nameItem->data(Qt::UserRole + 1).toBool();
    
    if (isFolder) {
        QMessageBox::warning(this, tr("Generate URL"), tr("Cannot generate URL for folders"));
        return;
    }
    
    // Ask for expiration time
    QStringList options = {tr("1 hour"), tr("6 hours"), tr("24 hours"), tr("7 days")};
    QList<int> seconds = {3600, 21600, 86400, 604800};
    
    bool ok;
    QString choice = QInputDialog::getItem(this, tr("URL Expiration"),
        tr("Select expiration time:"), options, 0, false, &ok);
    
    if (!ok) return;
    
    int expireSeconds = seconds[options.indexOf(choice)];
    QString url = getSignedUrl(m_currentBucket, key, expireSeconds);
    
    QApplication::clipboard()->setText(url);
    QMessageBox::information(this, tr("URL Generated"),
        tr("Signed URL copied to clipboard:\n\n%1").arg(url));
}

void CloudExplorerWidget::onContextMenu(const QPoint& pos) {
    QTableWidgetItem* item = m_objectsTable->itemAt(pos);
    if (!item) return;
    
    QMenu menu;
    
    menu.addAction(tr("Download"), this, &CloudExplorerWidget::onDownloadClicked);
    menu.addAction(tr("Delete"), this, &CloudExplorerWidget::onDeleteClicked);
    menu.addSeparator();
    menu.addAction(tr("Get URL"), this, &CloudExplorerWidget::onGenerateUrlClicked);
    menu.addAction(tr("Copy Path"), [this, item]() {
        QString key = m_objectsTable->item(item->row(), 0)->data(Qt::UserRole).toString();
        QApplication::clipboard()->setText(QString("s3://%1/%2").arg(m_currentBucket, key));
    });
    
    menu.exec(m_objectsTable->mapToGlobal(pos));
}

void CloudExplorerWidget::onTransferProgress(const QString& id, qint64 bytes, qint64 total) {
    Q_UNUSED(id);
    Q_UNUSED(bytes);
    Q_UNUSED(total);
    updateTransfersList();
}

void CloudExplorerWidget::onTransferCompleted(const QString& id) {
    Q_UNUSED(id);
    updateTransfersList();
    refresh();
}

void CloudExplorerWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void CloudExplorerWidget::dropEvent(QDropEvent* event) {
    if (m_currentBucket.isEmpty()) {
        QMessageBox::warning(this, tr("Upload"), tr("Please select a bucket first"));
        return;
    }
    
    QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            QString localPath = url.toLocalFile();
            QFileInfo fi(localPath);
            
            if (fi.isDir()) {
                uploadFolder(localPath, m_currentBucket, m_currentPrefix + fi.fileName() + "/");
            } else {
                uploadFile(localPath, m_currentBucket, m_currentPrefix + fi.fileName());
            }
        }
    }
}

void CloudExplorerWidget::createBucket(const QString& name, const QString& region) {
    // Implementation depends on provider
    Q_UNUSED(name);
    Q_UNUSED(region);
}

void CloudExplorerWidget::deleteBucket(const QString& name) {
    Q_UNUSED(name);
}

void CloudExplorerWidget::copyObject(const QString& srcBucket, const QString& srcKey,
                                     const QString& destBucket, const QString& destKey) {
    Q_UNUSED(srcBucket);
    Q_UNUSED(srcKey);
    Q_UNUSED(destBucket);
    Q_UNUSED(destKey);
}

void CloudExplorerWidget::moveObject(const QString& srcBucket, const QString& srcKey,
                                     const QString& destBucket, const QString& destKey) {
    copyObject(srcBucket, srcKey, destBucket, destKey);
    deleteObject(srcBucket, srcKey);
}

