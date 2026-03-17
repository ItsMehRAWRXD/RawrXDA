// MarketplaceUIView — native Win32, no Qt. Uses JSON strings and HWND controls.
#include "marketplace/marketplace_ui_view.h"
#include "marketplace/enterprise_policy_engine.h"
#include "marketplace/extension_marketplace_manager.h"
#include <nlohmann/json.hpp>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <commctrl.h>
#include <windows.h>
#endif

namespace
{

void setWindowText(void* hwnd, const std::string& s)
{
#ifdef _WIN32
    if (hwnd)
        SetWindowTextA(static_cast<HWND>(hwnd), s.c_str());
#endif
}

std::string getWindowText(void* hwnd)
{
#ifdef _WIN32
    if (!hwnd)
        return {};
    char buf[4096] = {};
    if (GetWindowTextA(static_cast<HWND>(hwnd), buf, sizeof(buf)))
        return buf;
#endif
    return {};
}

}  // namespace

MarketplaceUIView::MarketplaceUIView(void* parent) : m_marketplaceManager(nullptr), m_policyEngine(nullptr)
{
    (void)parent;
    setupSearchTab();
    setupDetailsTab();
    setupInstalledTab();
    setupSettingsTab();
    setupConnections();
    clearDetailsView();
}

MarketplaceUIView::~MarketplaceUIView() {}

void MarketplaceUIView::setMarketplaceManager(ExtensionMarketplaceManager* manager)
{
    m_marketplaceManager = manager;
}

void MarketplaceUIView::setPolicyEngine(EnterprisePolicyEngine* engine)
{
    m_policyEngine = engine;
}

void MarketplaceUIView::onSearchClicked()
{
    std::string query = getWindowText(m_searchBox);
    while (!query.empty() && (query.back() == ' ' || query.back() == '\t'))
        query.pop_back();
    if (query.empty())
    {
        showError("Please enter a search query");
        return;
    }
    showStatus("Searching...");
    if (m_marketplaceManager)
        m_marketplaceManager->searchExtensions(query);
}

void MarketplaceUIView::onInstallClicked()
{
    if (m_selectedExtensionId.empty())
    {
        showError("No extension selected");
        return;
    }
    if (m_marketplaceManager)
        m_marketplaceManager->installExtension(m_selectedExtensionId);
}

void MarketplaceUIView::onUninstallClicked()
{
    if (m_selectedExtensionId.empty())
    {
        showError("No extension selected");
        return;
    }
#ifdef _WIN32
    std::string msg = "Are you sure you want to uninstall '" + m_selectedExtensionId + "'?";
    if (MessageBoxA(nullptr, msg.c_str(), "Uninstall Extension", MB_YESNO | MB_ICONQUESTION) == IDYES)
#endif
        if (m_marketplaceManager)
            m_marketplaceManager->uninstallExtension(m_selectedExtensionId);
}

void MarketplaceUIView::onExtensionSelected()
{
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_searchResultsData.size()))
        m_selectedExtensionId = m_searchResultsData[static_cast<size_t>(m_selectedIndex)].first;
    if (m_selectedExtensionId.empty())
        return;
    auto it = m_extensionCache.find(m_selectedExtensionId);
    if (it != m_extensionCache.end())
    {
        showExtensionDetails(it->second);
    }
    else
    {
        showStatus("Loading extension details...");
        if (m_marketplaceManager)
            m_marketplaceManager->getExtensionDetails(m_selectedExtensionId);
    }
}

void MarketplaceUIView::onSearchResultsReceived(const std::string& extensionsJson)
{
    updateExtensionList(extensionsJson);
    showStatus("Found " + std::to_string(m_searchResultsData.size()) + " extensions");
}

void MarketplaceUIView::onExtensionDetailsReceived(const std::string& extensionJson)
{
    try
    {
        auto j = nlohmann::json::parse(extensionJson);
        std::string id = j.value("extensionId", j.value("name", ""));
        m_extensionCache[id] = extensionJson;
        if (m_selectedExtensionId == id)
            showExtensionDetails(extensionJson);
    }
    catch (...)
    {
    }
}

void MarketplaceUIView::onInstallationStarted(const std::string& extensionId)
{
    (void)extensionId;
    setWindowText(m_installStatus, "Installing...");
}

void MarketplaceUIView::onInstallationCompleted(const std::string& extensionId, bool success)
{
    setWindowText(m_installStatus, success ? "Installation completed" : "Installation failed");
    if (success)
    {
#ifdef _WIN32
        MessageBoxA(nullptr, ("Extension '" + extensionId + "' installed successfully").c_str(),
                    "Installation Complete", MB_OK | MB_ICONINFORMATION);
#endif
        if (m_marketplaceManager)
            m_marketplaceManager->listInstalledExtensions();
    }
}

void MarketplaceUIView::onInstallationError(const std::string& extensionId, const std::string& error)
{
    setWindowText(m_installStatus, "Installation failed: " + error);
#ifdef _WIN32
    MessageBoxA(nullptr, ("Failed to install extension '" + extensionId + "': " + error).c_str(), "Installation Error",
                MB_OK | MB_ICONWARNING);
#endif
}

void MarketplaceUIView::onUpdateAvailable(const std::string& extensionId, const std::string& version)
{
    showStatus("Update available for " + extensionId + " (v" + version + ")");
}

void MarketplaceUIView::onUninstallCompleted(const std::string& extensionId, bool success)
{
    if (success)
    {
#ifdef _WIN32
        MessageBoxA(nullptr, ("Extension '" + extensionId + "' uninstalled successfully").c_str(), "Uninstall Complete",
                    MB_OK | MB_ICONINFORMATION);
#endif
        if (m_marketplaceManager)
            m_marketplaceManager->listInstalledExtensions();
    }
    else
    {
#ifdef _WIN32
        MessageBoxA(nullptr, ("Failed to uninstall extension '" + extensionId + "'").c_str(), "Uninstall Error",
                    MB_OK | MB_ICONWARNING);
#endif
    }
}

void MarketplaceUIView::onInstalledExtensionsList(const std::string& extensionsJson)
{
    updateInstalledExtensionsList(extensionsJson);
}

void MarketplaceUIView::onErrorOccurred(const std::string& error)
{
    showError(error);
}

void MarketplaceUIView::onCacheCleared()
{
    showStatus("Cache cleared");
}

void MarketplaceUIView::onRefreshClicked()
{
    if (m_tabIndex == 0)
        onSearchClicked();
    else if (m_tabIndex == 2 && m_marketplaceManager)
        m_marketplaceManager->listInstalledExtensions();
}

void MarketplaceUIView::onSettingsChanged()
{
    if (m_marketplaceManager)
    {
        m_marketplaceManager->enableOfflineMode(m_offlineMode);
        m_marketplaceManager->syncWithPrivateMarketplace(getWindowText(m_privateMarketplaceUrl));
    }
    if (m_policyEngine)
    {
        m_policyEngine->setAllowList(m_allowList);
        m_policyEngine->setDenyList(m_denyList);
        m_policyEngine->setRequireSignature(m_requireSignature);
    }
    showStatus("Settings updated");
}

void MarketplaceUIView::setupSearchTab() {}
void MarketplaceUIView::setupDetailsTab() {}
void MarketplaceUIView::setupInstalledTab() {}
void MarketplaceUIView::setupSettingsTab() {}
void MarketplaceUIView::setupConnections() {}

void MarketplaceUIView::updateExtensionList(const std::string& extensionsJson)
{
    m_searchResultsData.clear();
    try
    {
        auto j = nlohmann::json::parse(extensionsJson);
        auto arr = j.is_array() ? j
                                : (j.contains("results")      ? j["results"]
                                   : j.contains("extensions") ? j["extensions"]
                                                              : nlohmann::json::array());
        for (const auto& ext : arr)
        {
            std::string name = ext.value("displayName", ext.value("name", ""));
            std::string publisher = ext.contains("publisher") && ext["publisher"].is_object()
                                        ? ext["publisher"].value("displayName", "")
                                        : "";
            std::string version;
            if (ext.contains("versions") && ext["versions"].is_array() && !ext["versions"].empty())
                version = ext["versions"][0].value("version", "");
            std::string id = ext.value("extensionId", ext.value("name", ""));
            std::string display = name + "\nby " + publisher + (version.empty() ? "" : " (v" + version + ")");
            m_searchResultsData.push_back({id, display});
        }
    }
    catch (...)
    {
    }
#ifdef _WIN32
    if (m_searchResultsList)
    {
        SendMessage(static_cast<HWND>(m_searchResultsList), LVM_DELETEALLITEMS, 0, 0);
        for (size_t i = 0; i < m_searchResultsData.size(); ++i)
        {
            LVITEMA lv = {};
            lv.mask = LVIF_TEXT;
            lv.iItem = static_cast<int>(i);
            lv.pszText = const_cast<char*>(m_searchResultsData[i].second.c_str());
            SendMessage(static_cast<HWND>(m_searchResultsList), LVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&lv));
        }
    }
#endif
}

void MarketplaceUIView::showExtensionDetails(const std::string& extensionJson)
{
    try
    {
        auto j = nlohmann::json::parse(extensionJson);
        std::string name = j.value("displayName", j.value("name", "Extension Name"));
        setWindowText(m_extensionName, name);
        std::string publisher = j.contains("publisher") && j["publisher"].is_object()
                                    ? j["publisher"].value("displayName", "Publisher")
                                    : "Publisher";
        setWindowText(m_extensionPublisher, "by " + publisher);
        std::string version = "Version: ";
        if (j.contains("versions") && j["versions"].is_array() && !j["versions"].empty())
            version += j["versions"][0].value("version", "");
        setWindowText(m_extensionVersion, version);
        double rating = 0;
        if (j.contains("statistics") && j["statistics"].is_array() && !j["statistics"].empty())
            rating = j["statistics"][0].value("value", 0.0);
        setWindowText(m_extensionRating, "Rating: " + std::to_string(static_cast<int>(rating)) + "/5");
        int64_t downloads = 0;
        if (j.contains("statistics") && j["statistics"].is_array() && j["statistics"].size() > 1)
            downloads = j["statistics"][1].value("value", 0);
        setWindowText(m_extensionDownloads, "Downloads: " + std::to_string(downloads));
        std::string desc = "";
        if (j.contains("versions") && j["versions"].is_array() && !j["versions"].empty() &&
            j["versions"][0].contains("description"))
            desc = j["versions"][0].value("description", "");
        setWindowText(m_extensionDescription, desc);
    }
    catch (...)
    {
        setWindowText(m_extensionName, "Extension Name");
        setWindowText(m_extensionPublisher, "Publisher");
        setWindowText(m_extensionVersion, "Version");
        setWindowText(m_extensionRating, "Rating");
        setWindowText(m_extensionDownloads, "Downloads");
        setWindowText(m_extensionDescription, "");
    }
}

void MarketplaceUIView::updateInstalledExtensionsList(const std::string& extensionsJson)
{
    m_installedExtensionsData.clear();
    try
    {
        auto j = nlohmann::json::parse(extensionsJson);
        auto arr = j.is_array() ? j : (j.contains("extensions") ? j["extensions"] : nlohmann::json::array());
        for (const auto& ext : arr)
        {
            std::string id = ext.value("id", ext.value("name", ""));
            std::string name = ext.value("name", "");
            std::string version = ext.value("version", "");
            std::string publisher = ext.value("publisher", "");
            m_installedExtensionsData.push_back({id, name + " (v" + version + ") by " + publisher});
        }
    }
    catch (...)
    {
    }
#ifdef _WIN32
    if (m_installedExtensionsList)
    {
        SendMessage(static_cast<HWND>(m_installedExtensionsList), LVM_DELETEALLITEMS, 0, 0);
        for (size_t i = 0; i < m_installedExtensionsData.size(); ++i)
        {
            LVITEMA lv = {};
            lv.mask = LVIF_TEXT;
            lv.iItem = static_cast<int>(i);
            lv.pszText = const_cast<char*>(m_installedExtensionsData[i].second.c_str());
            SendMessage(static_cast<HWND>(m_installedExtensionsList), LVM_INSERTITEMA, 0,
                        reinterpret_cast<LPARAM>(&lv));
        }
    }
#endif
}

void MarketplaceUIView::showError(const std::string& message)
{
    setWindowText(m_searchStatus, "Error: " + message);
#ifdef _WIN32
    MessageBoxA(nullptr, message.c_str(), "Error", MB_OK | MB_ICONWARNING);
#endif
}

void MarketplaceUIView::showStatus(const std::string& message)
{
    setWindowText(m_searchStatus, message);
}

void* MarketplaceUIView::createExtensionItemWidget(const std::string& extensionJson)
{
    (void)extensionJson;
    return nullptr;
}

void MarketplaceUIView::clearDetailsView()
{
    setWindowText(m_extensionName, "Extension Name");
    setWindowText(m_extensionPublisher, "Publisher");
    setWindowText(m_extensionVersion, "Version");
    setWindowText(m_extensionRating, "Rating");
    setWindowText(m_extensionDownloads, "Downloads");
    setWindowText(m_extensionDescription, "");
    setWindowText(m_installStatus, "");
}
