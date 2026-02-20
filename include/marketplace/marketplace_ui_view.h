#pragma once

// ============================================================================
// MarketplaceUIView — C++20, Win32. No Qt. Extension marketplace UI.
// ============================================================================

#include <map>
#include <string>

class ExtensionMarketplaceManager;
class EnterprisePolicyEngine;

/**
 * UI for extension marketplace: search, details, install/uninstall,
 * enterprise policy, offline cache. Use void* for Win32 controls.
 */
class MarketplaceUIView {
public:
    explicit MarketplaceUIView(void* parent = nullptr);
    ~MarketplaceUIView();

    void setMarketplaceManager(ExtensionMarketplaceManager* manager);
    void setPolicyEngine(EnterprisePolicyEngine* engine);

    void onSearchClicked();
    void onInstallClicked();
    void onUninstallClicked();
    void onExtensionSelected();
    void onSearchResultsReceived(const std::string& extensionsJson);
    void onExtensionDetailsReceived(const std::string& extensionJson);
    void onInstallationStarted(const std::string& extensionId);
    void onInstallationCompleted(const std::string& extensionId, bool success);
    void onInstallationError(const std::string& extensionId, const std::string& error);
    void onUpdateAvailable(const std::string& extensionId, const std::string& version);
    void onUninstallCompleted(const std::string& extensionId, bool success);
    void onInstalledExtensionsList(const std::string& extensionsJson);
    void onErrorOccurred(const std::string& error);
    void onCacheCleared();
    void onRefreshClicked();
    void onSettingsChanged();

private:
    void setupSearchTab();
    void setupDetailsTab();
    void setupInstalledTab();
    void setupSettingsTab();
    void setupConnections();
    void updateExtensionList(const std::string& extensionsJson);
    void showExtensionDetails(const std::string& extensionJson);
    void updateInstalledExtensionsList(const std::string& extensionsJson);
    void showError(const std::string& message);
    void showStatus(const std::string& message);
    void* createExtensionItemWidget(const std::string& extensionJson);
    void clearDetailsView();

    void* m_searchBox = nullptr;
    void* m_searchButton = nullptr;
    void* m_refreshButton = nullptr;
    void* m_tabWidget = nullptr;
    void* m_searchResultsList = nullptr;
    void* m_searchStatus = nullptr;
    void* m_extensionIcon = nullptr;
    void* m_extensionName = nullptr;
    void* m_extensionPublisher = nullptr;
    void* m_extensionVersion = nullptr;
    void* m_extensionRating = nullptr;
    void* m_extensionDownloads = nullptr;
    void* m_extensionDescription = nullptr;
    void* m_installButton = nullptr;
    void* m_uninstallButton = nullptr;
    void* m_installProgress = nullptr;
    void* m_installStatus = nullptr;
    void* m_installedExtensionsList = nullptr;
    void* m_uninstallSelectedButton = nullptr;
    void* m_offlineModeCheckBox = nullptr;
    void* m_privateMarketplaceUrl = nullptr;
    void* m_syncButton = nullptr;
    void* m_clearCacheButton = nullptr;
    void* m_cacheSizeLabel = nullptr;
    void* m_allowListEdit = nullptr;
    void* m_denyListEdit = nullptr;
    void* m_requireSignatureCheckBox = nullptr;

    ExtensionMarketplaceManager* m_marketplaceManager = nullptr;
    EnterprisePolicyEngine* m_policyEngine = nullptr;

    std::string m_selectedExtensionId;
    std::map<std::string, std::string> m_extensionCache;  // extensionId -> JSON
};
