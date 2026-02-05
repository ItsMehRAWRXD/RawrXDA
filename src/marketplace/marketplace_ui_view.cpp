#include "marketplace/marketplace_ui_view.h"
#include "marketplace/extension_marketplace_manager.h"
#include "marketplace/enterprise_policy_engine.h"
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPixmap>
#include <QIcon>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>
#include <QScrollBar>
#include <QStyle>
#include <QDebug>

MarketplaceUIView::MarketplaceUIView(QWidget* parent)
    : QWidget(parent)
    , m_marketplaceManager(nullptr)
    , m_policyEngine(nullptr)
{
    // Set up the main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // Create search bar
    QHBoxLayout* searchLayout = new QHBoxLayout();
    m_searchBox = new QLineEdit();
    m_searchBox->setPlaceholderText("Search extensions...");
    m_searchButton = new QPushButton("Search");
    m_refreshButton = new QPushButton();
    m_refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_refreshButton->setToolTip("Refresh");
    
    searchLayout->addWidget(m_searchBox);
    searchLayout->addWidget(m_searchButton);
    searchLayout->addWidget(m_refreshButton);
    
    mainLayout->addLayout(searchLayout);
    
    // Create tab widget
    m_tabWidget = new QTabWidget();
    mainLayout->addWidget(m_tabWidget);
    
    // Set up tabs
    setupSearchTab();
    setupDetailsTab();
    setupInstalledTab();
    setupSettingsTab();
    
    // Set up connections
    setupConnections();
    
    // Set initial state
    clearDetailsView();
    
    setWindowTitle("Extension Marketplace");
    resize(800, 600);
    
    qDebug() << "[MarketplaceUIView] Initialized";
}

MarketplaceUIView::~MarketplaceUIView() {
    // Cleanup if needed
}

void MarketplaceUIView::setMarketplaceManager(ExtensionMarketplaceManager* manager) {
    m_marketplaceManager = manager;
    
    // Connect signals
    if (m_marketplaceManager) {
        connect(m_marketplaceManager, &ExtensionMarketplaceManager::searchResultsReceived,
                this, &MarketplaceUIView::onSearchResultsReceived);
        connect(m_marketplaceManager, &ExtensionMarketplaceManager::extensionDetailsReceived,
                this, &MarketplaceUIView::onExtensionDetailsReceived);
        connect(m_marketplaceManager, &ExtensionMarketplaceManager::installationStarted,
                this, &MarketplaceUIView::onInstallationStarted);
        connect(m_marketplaceManager, &ExtensionMarketplaceManager::installationCompleted,
                this, &MarketplaceUIView::onInstallationCompleted);
        connect(m_marketplaceManager, &ExtensionMarketplaceManager::installationError,
                this, &MarketplaceUIView::onInstallationError);
        connect(m_marketplaceManager, &ExtensionMarketplaceManager::updateAvailable,
                this, &MarketplaceUIView::onUpdateAvailable);
        connect(m_marketplaceManager, &ExtensionMarketplaceManager::uninstallCompleted,
                this, &MarketplaceUIView::onUninstallCompleted);
        connect(m_marketplaceManager, &ExtensionMarketplaceManager::installedExtensionsList,
                this, &MarketplaceUIView::onInstalledExtensionsList);
        connect(m_marketplaceManager, &ExtensionMarketplaceManager::errorOccurred,
                this, &MarketplaceUIView::onErrorOccurred);
        connect(m_marketplaceManager, &ExtensionMarketplaceManager::cacheCleared,
                this, &MarketplaceUIView::onCacheCleared);
    }
}

void MarketplaceUIView::setPolicyEngine(EnterprisePolicyEngine* engine) {
    m_policyEngine = engine;
}

void MarketplaceUIView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Handle resize if needed
}

void MarketplaceUIView::onSearchClicked() {
    QString query = m_searchBox->text().trimmed();
    if (query.isEmpty()) {
        showError("Please enter a search query");
        return;
    }
    
    showStatus("Searching...");
    if (m_marketplaceManager) {
        m_marketplaceManager->searchExtensions(query);
    }
}

void MarketplaceUIView::onInstallClicked() {
    if (m_selectedExtensionId.isEmpty()) {
        showError("No extension selected");
        return;
    }
    
    if (m_marketplaceManager) {
        m_marketplaceManager->installExtension(m_selectedExtensionId);
    }
}

void MarketplaceUIView::onUninstallClicked() {
    if (m_selectedExtensionId.isEmpty()) {
        showError("No extension selected");
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Uninstall Extension", 
        QString("Are you sure you want to uninstall '%1'?").arg(m_selectedExtensionId),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (m_marketplaceManager) {
            m_marketplaceManager->uninstallExtension(m_selectedExtensionId);
        }
    }
}

void MarketplaceUIView::onExtensionSelected() {
    QListWidgetItem* item = m_searchResultsList->currentItem();
    if (!item) return;
    
    QString extensionId = item->data(Qt::UserRole).toString();
    m_selectedExtensionId = extensionId;
    
    // Check if we have cached details
    if (m_extensionCache.contains(extensionId)) {
        showExtensionDetails(m_extensionCache[extensionId]);
    } else {
        // Fetch details
        showStatus("Loading extension details...");
        if (m_marketplaceManager) {
            m_marketplaceManager->getExtensionDetails(extensionId);
        }
    }
    
    // Switch to details tab
    m_tabWidget->setCurrentIndex(1);
}

void MarketplaceUIView::onSearchResultsReceived(const QJsonArray& extensions) {
    updateExtensionList(extensions);
    showStatus(QString("Found %1 extensions").arg(extensions.size()));
}

void MarketplaceUIView::onExtensionDetailsReceived(const QJsonObject& extension) {
    QString extensionId = extension["extensionId"].toString();
    if (extensionId.isEmpty()) {
        extensionId = extension["name"].toString();
    }
    
    // Cache the extension details
    m_extensionCache[extensionId] = extension;
    
    // If this is the currently selected extension, show details
    if (m_selectedExtensionId == extensionId) {
        showExtensionDetails(extension);
    }
}

void MarketplaceUIView::onInstallationStarted(const QString& extensionId) {
    m_installStatus->setText(QString("Installing %1...").arg(extensionId));
    m_installProgress->setRange(0, 0); // Indeterminate progress
    m_installButton->setEnabled(false);
}

void MarketplaceUIView::onInstallationCompleted(const QString& extensionId, bool success) {
    m_installProgress->setRange(0, 100);
    m_installProgress->setValue(success ? 100 : 0);
    m_installStatus->setText(success ? "Installation completed" : "Installation failed");
    m_installButton->setEnabled(true);
    
    if (success) {
        QMessageBox::information(this, "Installation Complete", 
                                QString("Extension '%1' installed successfully").arg(extensionId));
        // Refresh installed extensions list
        if (m_marketplaceManager) {
            m_marketplaceManager->listInstalledExtensions();
        }
    }
}

void MarketplaceUIView::onInstallationError(const QString& extensionId, const QString& error) {
    m_installProgress->setRange(0, 100);
    m_installProgress->setValue(0);
    m_installStatus->setText(QString("Installation failed: %1").arg(error));
    m_installButton->setEnabled(true);
    
    QMessageBox::warning(this, "Installation Error", 
                        QString("Failed to install extension '%1': %2").arg(extensionId).arg(error));
}

void MarketplaceUIView::onUpdateAvailable(const QString& extensionId, const QString& version) {
    showStatus(QString("Update available for %1 (v%2)").arg(extensionId).arg(version));
}

void MarketplaceUIView::onUninstallCompleted(const QString& extensionId, bool success) {
    if (success) {
        QMessageBox::information(this, "Uninstall Complete", 
                                QString("Extension '%1' uninstalled successfully").arg(extensionId));
        // Refresh installed extensions list
        if (m_marketplaceManager) {
            m_marketplaceManager->listInstalledExtensions();
        }
    } else {
        QMessageBox::warning(this, "Uninstall Error", 
                            QString("Failed to uninstall extension '%1'").arg(extensionId));
    }
}

void MarketplaceUIView::onInstalledExtensionsList(const QJsonArray& extensions) {
    updateInstalledExtensionsList(extensions);
}

void MarketplaceUIView::onErrorOccurred(const QString& error) {
    showError(error);
}

void MarketplaceUIView::onCacheCleared() {
    showStatus("Cache cleared");
}

void MarketplaceUIView::onRefreshClicked() {
    // Refresh search results or installed extensions
    if (m_tabWidget->currentIndex() == 0) {
        // Search tab - re-run current search
        onSearchClicked();
    } else if (m_tabWidget->currentIndex() == 2) {
        // Installed tab - refresh installed extensions
        if (m_marketplaceManager) {
            m_marketplaceManager->listInstalledExtensions();
        }
    }
}

void MarketplaceUIView::onSettingsChanged() {
    // Apply settings changes
    if (m_marketplaceManager) {
        m_marketplaceManager->enableOfflineMode(m_offlineModeCheckBox->isChecked());
        m_marketplaceManager->syncWithPrivateMarketplace(m_privateMarketplaceUrl->text());
    }
    
    if (m_policyEngine) {
        // Parse allow/deny lists
        QStringList allowList = m_allowListEdit->toPlainText().split('\n', Qt::SkipEmptyParts);
        QStringList denyList = m_denyListEdit->toPlainText().split('\n', Qt::SkipEmptyParts);
        
        m_policyEngine->setAllowList(allowList);
        m_policyEngine->setDenyList(denyList);
        m_policyEngine->setRequireSignature(m_requireSignatureCheckBox->isChecked());
    }
    
    showStatus("Settings updated");
}

void MarketplaceUIView::setupSearchTab() {
    QWidget* searchTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(searchTab);
    
    m_searchResultsList = new QListWidget();
    m_searchStatus = new QLabel("Enter a search query to find extensions");
    m_searchStatus->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    
    layout->addWidget(m_searchResultsList);
    layout->addWidget(m_searchStatus);
    
    m_tabWidget->addTab(searchTab, "Search");
}

void MarketplaceUIView::setupDetailsTab() {
    QWidget* detailsTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(detailsTab);
    
    // Extension header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    m_extensionIcon = new QLabel();
    m_extensionIcon->setFixedSize(64, 64);
    m_extensionIcon->setStyleSheet("QLabel { background-color: #f0f0f0; border: 1px solid #ccc; }");
    
    QVBoxLayout* infoLayout = new QVBoxLayout();
    m_extensionName = new QLabel("Extension Name");
    m_extensionName->setStyleSheet("QLabel { font-size: 16px; font-weight: bold; }");
    m_extensionPublisher = new QLabel("Publisher");
    m_extensionVersion = new QLabel("Version");
    m_extensionRating = new QLabel("Rating");
    m_extensionDownloads = new QLabel("Downloads");
    
    infoLayout->addWidget(m_extensionName);
    infoLayout->addWidget(m_extensionPublisher);
    infoLayout->addWidget(m_extensionVersion);
    infoLayout->addWidget(m_extensionRating);
    infoLayout->addWidget(m_extensionDownloads);
    
    headerLayout->addWidget(m_extensionIcon);
    headerLayout->addLayout(infoLayout);
    headerLayout->addStretch();
    
    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_installButton = new QPushButton("Install");
    m_uninstallButton = new QPushButton("Uninstall");
    m_installProgress = new QProgressBar();
    m_installStatus = new QLabel();
    
    buttonLayout->addWidget(m_installButton);
    buttonLayout->addWidget(m_uninstallButton);
    buttonLayout->addWidget(m_installProgress);
    buttonLayout->addWidget(m_installStatus);
    buttonLayout->addStretch();
    
    // Description
    m_extensionDescription = new QTextEdit();
    m_extensionDescription->setReadOnly(true);
    
    layout->addLayout(headerLayout);
    layout->addLayout(buttonLayout);
    layout->addWidget(new QLabel("Description:"));
    layout->addWidget(m_extensionDescription);
    
    m_tabWidget->addTab(detailsTab, "Details");
}

void MarketplaceUIView::setupInstalledTab() {
    QWidget* installedTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(installedTab);
    
    m_installedExtensionsList = new QListWidget();
    m_uninstallSelectedButton = new QPushButton("Uninstall Selected");
    
    layout->addWidget(m_installedExtensionsList);
    layout->addWidget(m_uninstallSelectedButton);
    
    m_tabWidget->addTab(installedTab, "Installed");
}

void MarketplaceUIView::setupSettingsTab() {
    QWidget* settingsTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(settingsTab);
    
    // Offline mode
    QHBoxLayout* offlineLayout = new QHBoxLayout();
    m_offlineModeCheckBox = new QCheckBox("Enable Offline Mode");
    m_clearCacheButton = new QPushButton("Clear Cache");
    m_cacheSizeLabel = new QLabel("Cache size: 0 MB");
    
    offlineLayout->addWidget(m_offlineModeCheckBox);
    offlineLayout->addWidget(m_clearCacheButton);
    offlineLayout->addWidget(m_cacheSizeLabel);
    offlineLayout->addStretch();
    
    layout->addLayout(offlineLayout);
    
    // Private marketplace
    QGridLayout* privateLayout = new QGridLayout();
    privateLayout->addWidget(new QLabel("Private Marketplace URL:"), 0, 0);
    m_privateMarketplaceUrl = new QLineEdit();
    m_syncButton = new QPushButton("Sync");
    privateLayout->addWidget(m_privateMarketplaceUrl, 0, 1);
    privateLayout->addWidget(m_syncButton, 0, 2);
    
    layout->addLayout(privateLayout);
    
    // Allow list
    layout->addWidget(new QLabel("Extension Allow List (one per line):"));
    m_allowListEdit = new QTextEdit();
    m_allowListEdit->setMaximumHeight(100);
    layout->addWidget(m_allowListEdit);
    
    // Deny list
    layout->addWidget(new QLabel("Extension Deny List (one per line):"));
    m_denyListEdit = new QTextEdit();
    m_denyListEdit->setMaximumHeight(100);
    layout->addWidget(m_denyListEdit);
    
    // Signature requirement
    m_requireSignatureCheckBox = new QCheckBox("Require Extension Signatures");
    layout->addWidget(m_requireSignatureCheckBox);
    
    m_tabWidget->addTab(settingsTab, "Settings");
}

void MarketplaceUIView::setupConnections() {
    connect(m_searchButton, &QPushButton::clicked, this, &MarketplaceUIView::onSearchClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &MarketplaceUIView::onRefreshClicked);
    connect(m_installButton, &QPushButton::clicked, this, &MarketplaceUIView::onInstallClicked);
    connect(m_uninstallButton, &QPushButton::clicked, this, &MarketplaceUIView::onUninstallClicked);
    connect(m_searchResultsList, &QListWidget::itemClicked, this, &MarketplaceUIView::onExtensionSelected);
    connect(m_clearCacheButton, &QPushButton::clicked, [this]() {
        if (m_marketplaceManager) {
            m_marketplaceManager->clearCache();
        }
    });
    connect(m_syncButton, &QPushButton::clicked, this, &MarketplaceUIView::onSettingsChanged);
    
    // Settings change connections
    connect(m_offlineModeCheckBox, &QCheckBox::stateChanged, this, &MarketplaceUIView::onSettingsChanged);
    connect(m_privateMarketplaceUrl, &QLineEdit::editingFinished, this, &MarketplaceUIView::onSettingsChanged);
    connect(m_allowListEdit, &QTextEdit::textChanged, this, &MarketplaceUIView::onSettingsChanged);
    connect(m_denyListEdit, &QTextEdit::textChanged, this, &MarketplaceUIView::onSettingsChanged);
    connect(m_requireSignatureCheckBox, &QCheckBox::stateChanged, this, &MarketplaceUIView::onSettingsChanged);
}

void MarketplaceUIView::updateExtensionList(const QJsonArray& extensions) {
    m_searchResultsList->clear();
    
    for (const QJsonValue& value : extensions) {
        QJsonObject extension = value.toObject();
        
        QString name = extension["displayName"].toString();
        if (name.isEmpty()) {
            name = extension["name"].toString();
        }
        
        QString publisher = extension["publisher"].toObject()["displayName"].toString();
        QString version = extension["versions"].toArray().first().toObject()["version"].toString();
        
        QString itemText = QString("%1\nby %2 (v%3)").arg(name).arg(publisher).arg(version);
        
        QListWidgetItem* item = new QListWidgetItem(itemText);
        QString extensionId = extension["extensionId"].toString();
        if (extensionId.isEmpty()) {
            extensionId = extension["name"].toString();
        }
        item->setData(Qt::UserRole, extensionId);
        
        m_searchResultsList->addItem(item);
    }
}

void MarketplaceUIView::showExtensionDetails(const QJsonObject& extension) {
    QString name = extension["displayName"].toString();
    if (name.isEmpty()) {
        name = extension["name"].toString();
    }
    
    m_extensionName->setText(name);
    
    QString publisher = extension["publisher"].toObject()["displayName"].toString();
    m_extensionPublisher->setText(QString("by %1").arg(publisher));
    
    QString version = extension["versions"].toArray().first().toObject()["version"].toString();
    m_extensionVersion->setText(QString("Version: %1").arg(version));
    
    double rating = extension["statistics"].toArray().first().toObject()["value"].toDouble();
    m_extensionRating->setText(QString("Rating: %1/5").arg(rating, 0, 'f', 1));
    
    qint64 downloads = extension["statistics"].toArray().last().toObject()["value"].toVariant().toLongLong();
    m_extensionDownloads->setText(QString("Downloads: %1").arg(downloads));
    
    QString description = extension["versions"].toArray().first().toObject()["description"].toString();
    m_extensionDescription->setPlainText(description);
    
    // Update button states
    m_installButton->setEnabled(true);
    m_uninstallButton->setEnabled(false); // Would check if installed in real implementation
}

void MarketplaceUIView::updateInstalledExtensionsList(const QJsonArray& extensions) {
    m_installedExtensionsList->clear();
    
    for (const QJsonValue& value : extensions) {
        QJsonObject extension = value.toObject();
        
        QString name = extension["name"].toString();
        QString version = extension["version"].toString();
        QString publisher = extension["publisher"].toString();
        
        QString itemText = QString("%1 (v%2) by %3").arg(name).arg(version).arg(publisher);
        
        QListWidgetItem* item = new QListWidgetItem(itemText);
        item->setData(Qt::UserRole, extension["id"].toString());
        
        m_installedExtensionsList->addItem(item);
    }
}

void MarketplaceUIView::showError(const QString& message) {
    m_searchStatus->setText(QString("Error: %1").arg(message));
    m_searchStatus->setStyleSheet("QLabel { color: red; }");
    
    QMessageBox::warning(this, "Error", message);
}

void MarketplaceUIView::showStatus(const QString& message) {
    m_searchStatus->setText(message);
    m_searchStatus->setStyleSheet("QLabel { color: gray; font-style: italic; }");
}

QWidget* MarketplaceUIView::createExtensionItemWidget(const QJsonObject& extension) {
    QWidget* widget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(widget);
    
    QLabel* iconLabel = new QLabel();
    iconLabel->setFixedSize(32, 32);
    iconLabel->setStyleSheet("QLabel { background-color: #f0f0f0; }");
    
    QVBoxLayout* textLayout = new QVBoxLayout();
    QString name = extension["displayName"].toString();
    if (name.isEmpty()) {
        name = extension["name"].toString();
    }
    QLabel* nameLabel = new QLabel(name);
    nameLabel->setStyleSheet("QLabel { font-weight: bold; }");
    
    QString publisher = extension["publisher"].toObject()["displayName"].toString();
    QLabel* publisherLabel = new QLabel(QString("by %1").arg(publisher));
    
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(publisherLabel);
    
    layout->addWidget(iconLabel);
    layout->addLayout(textLayout);
    layout->addStretch();
    
    return widget;
}

void MarketplaceUIView::clearDetailsView() {
    m_extensionName->setText("Extension Name");
    m_extensionPublisher->setText("Publisher");
    m_extensionVersion->setText("Version");
    m_extensionRating->setText("Rating");
    m_extensionDownloads->setText("Downloads");
    m_extensionDescription->clear();
    m_installButton->setEnabled(false);
    m_uninstallButton->setEnabled(false);
    m_installProgress->setValue(0);
    m_installStatus->clear();
}