#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QProgressBar>
#include <QTabWidget>
#include <QScrollArea>
#include <QFrame>
#include <QComboBox>
#include <QCheckBox>
#include <QTextEdit>

// Forward declarations
class ExtensionMarketplaceManager;
class EnterprisePolicyEngine;

/**
 * @class MarketplaceUIView
 * @brief UI widget for the VS Code extension marketplace
 * 
 * This class provides:
 * - Search and browse interface
 * - Extension details view
 * - Installation management
 * - Enterprise policy settings
 * - Offline cache management
 */
class MarketplaceUIView : public QWidget {
    Q_OBJECT

public:
    explicit MarketplaceUIView(QWidget* parent = nullptr);
    ~MarketplaceUIView();

    void setMarketplaceManager(ExtensionMarketplaceManager* manager);
    void setPolicyEngine(EnterprisePolicyEngine* engine);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSearchClicked();
    void onInstallClicked();
    void onUninstallClicked();
    void onExtensionSelected();
    void onSearchResultsReceived(const QJsonArray& extensions);
    void onExtensionDetailsReceived(const QJsonObject& extension);
    void onInstallationStarted(const QString& extensionId);
    void onInstallationCompleted(const QString& extensionId, bool success);
    void onInstallationError(const QString& extensionId, const QString& error);
    void onUpdateAvailable(const QString& extensionId, const QString& version);
    void onUninstallCompleted(const QString& extensionId, bool success);
    void onInstalledExtensionsList(const QJsonArray& extensions);
    void onErrorOccurred(const QString& error);
    void onCacheCleared();
    void onRefreshClicked();
    void onSettingsChanged();

private:
    // UI components
    QLineEdit* m_searchBox;
    QPushButton* m_searchButton;
    QPushButton* m_refreshButton;
    QTabWidget* m_tabWidget;
    
    // Search results tab
    QListWidget* m_searchResultsList;
    QLabel* m_searchStatus;
    
    // Extension details tab
    QLabel* m_extensionIcon;
    QLabel* m_extensionName;
    QLabel* m_extensionPublisher;
    QLabel* m_extensionVersion;
    QLabel* m_extensionRating;
    QLabel* m_extensionDownloads;
    QTextEdit* m_extensionDescription;
    QPushButton* m_installButton;
    QPushButton* m_uninstallButton;
    QProgressBar* m_installProgress;
    QLabel* m_installStatus;
    
    // Installed extensions tab
    QListWidget* m_installedExtensionsList;
    QPushButton* m_uninstallSelectedButton;
    
    // Settings tab
    QCheckBox* m_offlineModeCheckBox;
    QLineEdit* m_privateMarketplaceUrl;
    QPushButton* m_syncButton;
    QPushButton* m_clearCacheButton;
    QLabel* m_cacheSizeLabel;
    QTextEdit* m_allowListEdit;
    QTextEdit* m_denyListEdit;
    QCheckBox* m_requireSignatureCheckBox;
    
    // Backend components
    ExtensionMarketplaceManager* m_marketplaceManager;
    EnterprisePolicyEngine* m_policyEngine;
    
    // State tracking
    QString m_selectedExtensionId;
    QHash<QString, QJsonObject> m_extensionCache;
    
    // UI setup methods
    void setupSearchTab();
    void setupDetailsTab();
    void setupInstalledTab();
    void setupSettingsTab();
    void setupConnections();
    
    // Helper methods
    void updateExtensionList(const QJsonArray& extensions);
    void showExtensionDetails(const QJsonObject& extension);
    void updateInstalledExtensionsList(const QJsonArray& extensions);
    void showError(const QString& message);
    void showStatus(const QString& message);
    QWidget* createExtensionItemWidget(const QJsonObject& extension);
    void clearDetailsView();
};