#include "marketplace/marketplace_ui_view.h"
#include "marketplace/extension_marketplace_manager.h"
#include "marketplace/enterprise_policy_engine.h"
MarketplaceUIView::MarketplaceUIView(void* parent)
    : // Widget(parent)
    , m_marketplaceManager(nullptr)
    , m_policyEngine(nullptr)
{
    // Set up the main layout
    void* mainLayout = new void(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // Create search bar
    void* searchLayout = new void();
    m_searchBox = new voidEdit();
    m_searchBox->setPlaceholderText("Search extensions...");
    m_searchButton = new void("Search");
    m_refreshButton = new void();
    m_refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_refreshButton->setToolTip("Refresh");
    
    searchLayout->addWidget(m_searchBox);
    searchLayout->addWidget(m_searchButton);
    searchLayout->addWidget(m_refreshButton);
    
    mainLayout->addLayout(searchLayout);
    
    // Create tab widget
    m_tabWidget = new void();
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
    
}

MarketplaceUIView::~MarketplaceUIView() {
    // Cleanup if needed
}

void MarketplaceUIView::setMarketplaceManager(ExtensionMarketplaceManager* manager) {
    m_marketplaceManager = manager;
    
    // Connect signals
    if (m_marketplaceManager) {  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}
}

void MarketplaceUIView::setPolicyEngine(EnterprisePolicyEngine* engine) {
    m_policyEngine = engine;
}

void MarketplaceUIView::resizeEvent(void* event) {
    void::resizeEvent(event);
    // Handle resize if needed
}

void MarketplaceUIView::onSearchClicked() {
    std::string query = m_searchBox->text().trimmed();
    if (query.empty()) {
        showError("Please enter a search query");
        return;
    }
    
    showStatus("Searching...");
    if (m_marketplaceManager) {
        m_marketplaceManager->searchExtensions(query);
    }
}

void MarketplaceUIView::onInstallClicked() {
    if (m_selectedExtensionId.empty()) {
        showError("No extension selected");
        return;
    }
    
    if (m_marketplaceManager) {
        m_marketplaceManager->installExtension(m_selectedExtensionId);
    }
}

void MarketplaceUIView::onUninstallClicked() {
    if (m_selectedExtensionId.empty()) {
        showError("No extension selected");
        return;
    }
    
    void::StandardButton reply = void::question(
        this, "Uninstall Extension", 
        std::string("Are you sure you want to uninstall '%1'?"),
        void::Yes | void::No);
    
    if (reply == void::Yes) {
        if (m_marketplaceManager) {
            m_marketplaceManager->uninstallExtension(m_selectedExtensionId);
        }
    }
}

void MarketplaceUIView::onExtensionSelected() {
    QListWidgetItem* item = m_searchResultsList->currentItem();
    if (!item) return;
    
    std::string extensionId = item->data(UserRole).toString();
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

void MarketplaceUIView::onSearchResultsReceived(const void*& extensions) {
    updateExtensionList(extensions);
    showStatus(std::string("Found %1 extensions")));
}

void MarketplaceUIView::onExtensionDetailsReceived(const void*& extension) {
    std::string extensionId = extension["extensionId"].toString();
    if (extensionId.empty()) {
        extensionId = extension["name"].toString();
    }
    
    // Cache the extension details
    m_extensionCache[extensionId] = extension;
    
    // If this is the currently selected extension, show details
    if (m_selectedExtensionId == extensionId) {
        showExtensionDetails(extension);
    }
}

void MarketplaceUIView::onInstallationStarted(const std::string& extensionId) {
    m_installStatus->setText(std::string("Installing %1..."));
    m_installProgress->setRange(0, 0); // Indeterminate progress
    m_installButton->setEnabled(false);
}

void MarketplaceUIView::onInstallationCompleted(const std::string& extensionId, bool success) {
    m_installProgress->setRange(0, 100);
    m_installProgress->setValue(success ? 100 : 0);
    m_installStatus->setText(success ? "Installation completed" : "Installation failed");
    m_installButton->setEnabled(true);
    
    if (success) {
        void::information(this, "Installation Complete", 
                                std::string("Extension '%1' installed successfully"));
        // Refresh installed extensions list
        if (m_marketplaceManager) {
            m_marketplaceManager->listInstalledExtensions();
        }
    }
}

void MarketplaceUIView::onInstallationError(const std::string& extensionId, const std::string& error) {
    m_installProgress->setRange(0, 100);
    m_installProgress->setValue(0);
    m_installStatus->setText(std::string("Installation failed: %1"));
    m_installButton->setEnabled(true);
    
    void::warning(this, "Installation Error", 
                        std::string("Failed to install extension '%1': %2"));
}

void MarketplaceUIView::onUpdateAvailable(const std::string& extensionId, const std::string& version) {
    showStatus(std::string("Update available for %1 (v%2)"));
}

void MarketplaceUIView::onUninstallCompleted(const std::string& extensionId, bool success) {
    if (success) {
        void::information(this, "Uninstall Complete", 
                                std::string("Extension '%1' uninstalled successfully"));
        // Refresh installed extensions list
        if (m_marketplaceManager) {
            m_marketplaceManager->listInstalledExtensions();
        }
    } else {
        void::warning(this, "Uninstall Error", 
                            std::string("Failed to uninstall extension '%1'"));
    }
}

void MarketplaceUIView::onInstalledExtensionsList(const void*& extensions) {
    updateInstalledExtensionsList(extensions);
}

void MarketplaceUIView::onErrorOccurred(const std::string& error) {
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
        std::stringList allowList = m_allowListEdit->toPlainText().split('\n', SkipEmptyParts);
        std::stringList denyList = m_denyListEdit->toPlainText().split('\n', SkipEmptyParts);
        
        m_policyEngine->setAllowList(allowList);
        m_policyEngine->setDenyList(denyList);
        m_policyEngine->setRequireSignature(m_requireSignatureCheckBox->isChecked());
    }
    
    showStatus("Settings updated");
}

void MarketplaceUIView::setupSearchTab() {
    void* searchTab = new // Widget();
    void* layout = new void(searchTab);
    
    m_searchResultsList = nullptr;
    m_searchStatus = new void("Enter a search query to find extensions");
    m_searchStatus->setStyleSheet("void { color: gray; font-style: italic; }");
    
    layout->addWidget(m_searchResultsList);
    layout->addWidget(m_searchStatus);
    
    m_tabWidget->addTab(searchTab, "Search");
}

void MarketplaceUIView::setupDetailsTab() {
    void* detailsTab = new // Widget();
    void* layout = new void(detailsTab);
    
    // Extension header
    void* headerLayout = new void();
    m_extensionIcon = new void();
    m_extensionIcon->setFixedSize(64, 64);
    m_extensionIcon->setStyleSheet("void { background-color: #f0f0f0; border: 1px solid #ccc; }");
    
    void* infoLayout = new void();
    m_extensionName = new void("Extension Name");
    m_extensionName->setStyleSheet("void { font-size: 16px; font-weight: bold; }");
    m_extensionPublisher = new void("Publisher");
    m_extensionVersion = new void("Version");
    m_extensionRating = new void("Rating");
    m_extensionDownloads = new void("Downloads");
    
    infoLayout->addWidget(m_extensionName);
    infoLayout->addWidget(m_extensionPublisher);
    infoLayout->addWidget(m_extensionVersion);
    infoLayout->addWidget(m_extensionRating);
    infoLayout->addWidget(m_extensionDownloads);
    
    headerLayout->addWidget(m_extensionIcon);
    headerLayout->addLayout(infoLayout);
    headerLayout->addStretch();
    
    // Action buttons
    void* buttonLayout = new void();
    m_installButton = new void("Install");
    m_uninstallButton = new void("Uninstall");
    m_installProgress = new void();
    m_installStatus = new void();
    
    buttonLayout->addWidget(m_installButton);
    buttonLayout->addWidget(m_uninstallButton);
    buttonLayout->addWidget(m_installProgress);
    buttonLayout->addWidget(m_installStatus);
    buttonLayout->addStretch();
    
    // Description
    m_extensionDescription = new void();
    m_extensionDescription->setReadOnly(true);
    
    layout->addLayout(headerLayout);
    layout->addLayout(buttonLayout);
    layout->addWidget(new void("Description:"));
    layout->addWidget(m_extensionDescription);
    
    m_tabWidget->addTab(detailsTab, "Details");
}

void MarketplaceUIView::setupInstalledTab() {
    void* installedTab = new // Widget();
    void* layout = new void(installedTab);
    
    m_installedExtensionsList = nullptr;
    m_uninstallSelectedButton = new void("Uninstall Selected");
    
    layout->addWidget(m_installedExtensionsList);
    layout->addWidget(m_uninstallSelectedButton);
    
    m_tabWidget->addTab(installedTab, "Installed");
}

void MarketplaceUIView::setupSettingsTab() {
    void* settingsTab = new // Widget();
    void* layout = new void(settingsTab);
    
    // Offline mode
    void* offlineLayout = new void();
    m_offlineModeCheckBox = new void("Enable Offline Mode");
    m_clearCacheButton = new void("Clear Cache");
    m_cacheSizeLabel = new void("Cache size: 0 MB");
    
    offlineLayout->addWidget(m_offlineModeCheckBox);
    offlineLayout->addWidget(m_clearCacheButton);
    offlineLayout->addWidget(m_cacheSizeLabel);
    offlineLayout->addStretch();
    
    layout->addLayout(offlineLayout);
    
    // Private marketplace
    void* privateLayout = new void();
    privateLayout->addWidget(new void("Private Marketplace URL:"), 0, 0);
    m_privateMarketplaceUrl = new voidEdit();
    m_syncButton = new void("Sync");
    privateLayout->addWidget(m_privateMarketplaceUrl, 0, 1);
    privateLayout->addWidget(m_syncButton, 0, 2);
    
    layout->addLayout(privateLayout);
    
    // Allow list
    layout->addWidget(new void("Extension Allow List (one per line):"));
    m_allowListEdit = new void();
    m_allowListEdit->setMaximumHeight(100);
    layout->addWidget(m_allowListEdit);
    
    // Deny list
    layout->addWidget(new void("Extension Deny List (one per line):"));
    m_denyListEdit = new void();
    m_denyListEdit->setMaximumHeight(100);
    layout->addWidget(m_denyListEdit);
    
    // Signature requirement
    m_requireSignatureCheckBox = new void("Require Extension Signatures");
    layout->addWidget(m_requireSignatureCheckBox);
    
    m_tabWidget->addTab(settingsTab, "Settings");
}

void MarketplaceUIView::setupConnections() {  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}
    });  // Signal connection removed\n// Settings change connections  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}

void MarketplaceUIView::updateExtensionList(const void*& extensions) {
    m_searchResultsList->clear();
    
    for (const void*& value : extensions) {
        void* extension = value.toObject();
        
        std::string name = extension["displayName"].toString();
        if (name.empty()) {
            name = extension["name"].toString();
        }
        
        std::string publisher = extension["publisher"].toObject()["displayName"].toString();
        std::string version = extension["versions"].toArray().first().toObject()["version"].toString();
        
        std::string itemText = std::string("%1\nby %2 (v%3)");
        
        QListWidgetItem* item = nullptr;
        std::string extensionId = extension["extensionId"].toString();
        if (extensionId.empty()) {
            extensionId = extension["name"].toString();
        }
        item->setData(UserRole, extensionId);
        
        m_searchResultsList->addItem(item);
    }
}

void MarketplaceUIView::showExtensionDetails(const void*& extension) {
    std::string name = extension["displayName"].toString();
    if (name.empty()) {
        name = extension["name"].toString();
    }
    
    m_extensionName->setText(name);
    
    std::string publisher = extension["publisher"].toObject()["displayName"].toString();
    m_extensionPublisher->setText(std::string("by %1"));
    
    std::string version = extension["versions"].toArray().first().toObject()["version"].toString();
    m_extensionVersion->setText(std::string("Version: %1"));
    
    double rating = extension["statistics"].toArray().first().toObject()["value"].toDouble();
    m_extensionRating->setText(std::string("Rating: %1/5"));
    
    int64_t downloads = extension["statistics"].toArray().last().toObject()["value"].toVariant().toLongLong();
    m_extensionDownloads->setText(std::string("Downloads: %1"));
    
    std::string description = extension["versions"].toArray().first().toObject()["description"].toString();
    m_extensionDescription->setPlainText(description);
    
    // Update button states
    m_installButton->setEnabled(true);
    m_uninstallButton->setEnabled(false); // Would check if installed in real implementation
}

void MarketplaceUIView::updateInstalledExtensionsList(const void*& extensions) {
    m_installedExtensionsList->clear();
    
    for (const void*& value : extensions) {
        void* extension = value.toObject();
        
        std::string name = extension["name"].toString();
        std::string version = extension["version"].toString();
        std::string publisher = extension["publisher"].toString();
        
        std::string itemText = std::string("%1 (v%2) by %3");
        
        QListWidgetItem* item = nullptr;
        item->setData(UserRole, extension["id"].toString());
        
        m_installedExtensionsList->addItem(item);
    }
}

void MarketplaceUIView::showError(const std::string& message) {
    m_searchStatus->setText(std::string("Error: %1"));
    m_searchStatus->setStyleSheet("void { color: red; }");
    
    void::warning(this, "Error", message);
}

void MarketplaceUIView::showStatus(const std::string& message) {
    m_searchStatus->setText(message);
    m_searchStatus->setStyleSheet("void { color: gray; font-style: italic; }");
}

void* MarketplaceUIView::createExtensionItemWidget(const void*& extension) {
    void* widget = new // Widget();
    void* layout = new void(widget);
    
    void* iconLabel = new void();
    iconLabel->setFixedSize(32, 32);
    iconLabel->setStyleSheet("void { background-color: #f0f0f0; }");
    
    void* textLayout = new void();
    std::string name = extension["displayName"].toString();
    if (name.empty()) {
        name = extension["name"].toString();
    }
    void* nameLabel = new void(name);
    nameLabel->setStyleSheet("void { font-weight: bold; }");
    
    std::string publisher = extension["publisher"].toObject()["displayName"].toString();
    void* publisherLabel = new void(std::string("by %1"));
    
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

