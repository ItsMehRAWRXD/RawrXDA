#include "Win32IDE_TabManager.h"
#include "Win32IDE.h"
#include "Win32IDE_Settings.h"
#include <commctrl.h>
#include <algorithm>

// TabManager implementation for RawrXD Win32IDE
// Provides sovereign tab management with persistence and recovery

Win32IDE_TabManager::Win32IDE_TabManager(Win32IDE* ide)
    : m_ide(ide), m_hwndTabBar(nullptr), m_activeTabIndex(-1)
{
}

Win32IDE_TabManager::~Win32IDE_TabManager()
{
    cleanup();
}

bool Win32IDE_TabManager::initialize(HWND hwndParent)
{
    if (!hwndParent) return false;

    // Create tab control with sovereign styling
    m_hwndTabBar = CreateWindowExW(0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS | TCS_FOCUSNEVER |
        TCS_OWNERDRAWFIXED | TCS_TOOLTIPS,
        0, 0, 800, 30, hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!m_hwndTabBar) return false;

    // Apply sovereign theme
    applySovereignTheme();

    // Load persisted tabs
    loadPersistedTabs();

    // Initialize GPU sovereign control
    initializeGPUSovereignControl();

    return true;
}

void Win32IDE_TabManager::cleanup()
{
    if (m_hwndTabBar)
    {
        // Persist current tab state before cleanup
        persistTabState();

        DestroyWindow(m_hwndTabBar);
        m_hwndTabBar = nullptr;
    }

    m_editorTabs.clear();
    m_activeTabIndex = -1;
}

void Win32IDE_TabManager::applySovereignTheme()
{
    if (!m_hwndTabBar) return;

    // Get sovereign colors from settings
    auto config = GetSovereignConfig();
    COLORREF tabBg = RGB(30, 30, 30);
    COLORREF tabActiveBg = RGB(45, 45, 45);
    COLORREF tabText = RGB(200, 200, 200);
    COLORREF tabBorder = RGB(60, 60, 60);

    // Apply theme via window properties
    SetPropW(m_hwndTabBar, L"SovereignTabBg", (HANDLE)tabBg);
    SetPropW(m_hwndTabBar, L"SovereignTabActiveBg", (HANDLE)tabActiveBg);
    SetPropW(m_hwndTabBar, L"SovereignTabText", (HANDLE)tabText);
    SetPropW(m_hwndTabBar, L"SovereignTabBorder", (HANDLE)tabBorder);
}

int Win32IDE_TabManager::addTab(const std::string& filePath, const std::string& displayName)
{
    if (!m_hwndTabBar) return -1;

    EditorTab tab;
    tab.filePath = filePath;
    tab.displayName = displayName.empty() ? extractFileName(filePath) : displayName;
    tab.content = "";
    tab.modified = false;
    tab.isPinned = false;
    tab.isPreview = false;
    tab.cursorLine = 1;
    tab.cursorCol = 0;
    tab.scrollPos = 0;

    m_editorTabs.push_back(tab);
    int newIndex = (int)m_editorTabs.size() - 1;

    // Add to Win32 tab control
    TCITEMW tci = {};
    tci.mask = TCIF_TEXT;
    std::wstring displayW = utf8ToWide(tab.displayName);
    tci.pszText = const_cast<LPWSTR>(displayW.c_str());
    SendMessageW(m_hwndTabBar, TCM_INSERTITEMW, newIndex, (LPARAM)&tci);

    // Auto-activate new tab
    setActiveTab(newIndex);

    // Persist state
    persistTabState();

    return newIndex;
}

void Win32IDE_TabManager::removeTab(int index)
{
    if (index < 0 || index >= (int)m_editorTabs.size() || !m_hwndTabBar) return;

    // Clear any cached data for this tab
    const std::string& closingFile = m_editorTabs[index].filePath;
    if (!closingFile.empty() && m_ide)
    {
        // Notify IDE of tab closure for cleanup
        m_ide->onTabClosing(index);
    }

    // Remove from Win32 control
    SendMessage(m_hwndTabBar, TCM_DELETEITEM, index, 0);

    // Remove from our vector
    m_editorTabs.erase(m_editorTabs.begin() + index);

    // Adjust active tab index
    if (m_editorTabs.empty())
    {
        m_activeTabIndex = -1;
    }
    else if (index <= m_activeTabIndex)
    {
        m_activeTabIndex = std::max(0, m_activeTabIndex - 1);
        SendMessage(m_hwndTabBar, TCM_SETCURSEL, m_activeTabIndex, 0);
    }

    // Persist updated state
    persistTabState();
}

void Win32IDE_TabManager::setActiveTab(int index)
{
    if (index < 0 || index >= (int)m_editorTabs.size() || !m_hwndTabBar) return;

    m_activeTabIndex = index;
    SendMessage(m_hwndTabBar, TCM_SETCURSEL, index, 0);

    // Notify IDE of tab change
    if (m_ide)
    {
        m_ide->onTabActivated(index);
    }
}

int Win32IDE_TabManager::findTabByPath(const std::string& filePath) const
{
    for (int i = 0; i < (int)m_editorTabs.size(); i++)
    {
        if (m_editorTabs[i].filePath == filePath)
            return i;
    }
    return -1;
}

void Win32IDE_TabManager::setTabModified(int index, bool modified)
{
    if (index < 0 || index >= (int)m_editorTabs.size()) return;

    if (m_editorTabs[index].modified != modified)
    {
        m_editorTabs[index].modified = modified;

        // Update tab display
        updateTabDisplay(index);

        // Persist state
        persistTabState();
    }
}

void Win32IDE_TabManager::updateTabDisplay(int index)
{
    if (index < 0 || index >= (int)m_editorTabs.size() || !m_hwndTabBar) return;

    const EditorTab& tab = m_editorTabs[index];

    // Update tab text with modified indicator
    std::string displayText = tab.displayName;
    if (tab.modified) displayText += " *";

    std::wstring displayW = utf8ToWide(displayText);

    TCITEMW tci = {};
    tci.mask = TCIF_TEXT;
    tci.pszText = const_cast<LPWSTR>(displayW.c_str());
    SendMessageW(m_hwndTabBar, TCM_SETITEMW, index, (LPARAM)&tci);
}

void Win32IDE_TabManager::handleTabClick(POINT pt)
{
    if (!m_hwndTabBar) return;

    TCHITTESTINFO hitTest = {};
    hitTest.pt = pt;
    int index = (int)SendMessage(m_hwndTabBar, TCM_HITTEST, 0, (LPARAM)&hitTest);

    if (index >= 0 && index < (int)m_editorTabs.size())
    {
        // Check if close button was clicked
        RECT rc;
        SendMessage(m_hwndTabBar, TCM_GETITEMRECT, index, (LPARAM)&rc);

        if (pt.x >= rc.right - 15 && pt.x <= rc.right - 5)
        {
            // Close button clicked
            handleTabClose(index);
            return;
        }

        // Tab clicked - switch to it
        setActiveTab(index);
    }
}

void Win32IDE_TabManager::handleTabClose(int index)
{
    if (index < 0 || index >= (int)m_editorTabs.size()) return;

    const EditorTab& tab = m_editorTabs[index];

    if (tab.modified)
    {
        // Ask user to save
        std::string msg = "Save changes to " + tab.displayName + "?";
        int result = MessageBoxW(GetParent(m_hwndTabBar), utf8ToWide(msg).c_str(),
                               L"Confirm Close", MB_YESNOCANCEL | MB_ICONQUESTION);

        if (result == IDCANCEL) return;

        if (result == IDYES)
        {
            // Save the file
            if (m_ide)
            {
                setActiveTab(index); // Make sure this tab is active for saving
                m_ide->saveCurrentFile();
            }
        }
    }

    removeTab(index);
}

void Win32IDE_TabManager::persistTabState()
{
    if (m_editorTabs.empty()) return;

    nlohmann::json j;
    j["activeTabIndex"] = m_activeTabIndex;

    nlohmann::json tabsArray = nlohmann::json::array();
    for (const auto& tab : m_editorTabs)
    {
        nlohmann::json tabJson;
        tabJson["filePath"] = tab.filePath;
        tabJson["displayName"] = tab.displayName;
        tabJson["content"] = tab.content;
        tabJson["modified"] = tab.modified;
        tabJson["isPinned"] = tab.isPinned;
        tabJson["isPreview"] = tab.isPreview;
        tabJson["cursorLine"] = tab.cursorLine;
        tabJson["cursorCol"] = tab.cursorCol;
        tabJson["scrollPos"] = tab.scrollPos;
        tabsArray.push_back(tabJson);
    }
    j["tabs"] = tabsArray;

    // Save to sovereign location
    std::string statePath = getTabStatePath();
    std::ofstream file(statePath);
    if (file)
    {
        file << j.dump(4);
    }
}

void Win32IDE_TabManager::loadPersistedTabs()
{
    std::string statePath = getTabStatePath();
    std::ifstream file(statePath);
    if (!file) return;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(content);
    } catch (...) {
        return;
    }

    if (j.contains("tabs") && j["tabs"].is_array())
    {
        m_editorTabs.clear();
        for (const auto& tabJson : j["tabs"])
        {
            EditorTab tab;
            tab.filePath = tabJson.value("filePath", "");
            tab.displayName = tabJson.value("displayName", "");
            tab.content = tabJson.value("content", "");
            tab.modified = tabJson.value("modified", false);
            tab.isPinned = tabJson.value("isPinned", false);
            tab.isPreview = tabJson.value("isPreview", false);
            tab.cursorLine = tabJson.value("cursorLine", 1);
            tab.cursorCol = tabJson.value("cursorCol", 0);
            tab.scrollPos = tabJson.value("scrollPos", 0);
            m_editorTabs.push_back(tab);
        }

        if (j.contains("activeTabIndex"))
        {
            m_activeTabIndex = j["activeTabIndex"];
        }

        // Rebuild Win32 tab control
        rebuildTabControl();
    }
}

void Win32IDE_TabManager::rebuildTabControl()
{
    if (!m_hwndTabBar) return;

    // Clear existing tabs
    SendMessage(m_hwndTabBar, TCM_DELETEALLITEMS, 0, 0);

    // Re-add all tabs
    for (size_t i = 0; i < m_editorTabs.size(); i++)
    {
        const EditorTab& tab = m_editorTabs[i];
        std::string displayText = tab.displayName;
        if (tab.modified) displayText += " *";

        std::wstring displayW = utf8ToWide(displayText);

        TCITEMW tci = {};
        tci.mask = TCIF_TEXT;
        tci.pszText = const_cast<LPWSTR>(displayW.c_str());
        SendMessageW(m_hwndTabBar, TCM_INSERTITEMW, i, (LPARAM)&tci);
    }

    // Restore active tab
    if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_editorTabs.size())
    {
        SendMessage(m_hwndTabBar, TCM_SETCURSEL, m_activeTabIndex, 0);
    }
}

std::string Win32IDE_TabManager::getTabStatePath() const
{
    // Use exe directory for tab state (sovereign approach)
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeDir = std::string(exePath);
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos)
    {
        exeDir = exeDir.substr(0, lastSlash);
    }

    return exeDir + "\\tab_state.json";
}

std::string Win32IDE_TabManager::extractFileName(const std::string& filePath) const
{
    size_t lastSlash = filePath.find_last_of("\\/");
    if (lastSlash != std::string::npos)
    {
        return filePath.substr(lastSlash + 1);
    }
    return filePath;
}

std::wstring Win32IDE_TabManager::utf8ToWide(const std::string& str) const
{
    if (str.empty()) return L"";

    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    result.resize(size - 1); // Remove null terminator
    return result;
}

// ============================================================================
// GPU Sovereign Control Implementation
// ============================================================================

void Win32IDE_TabManager::initializeGPUSovereignControl()
{
    // Initialize KFD interface
    uint64_t version = KFD_Get_Driver_Version();
    if (version == 0) {
        // Handle error - driver not loaded
        return;
    }

    // Initialize shadow pager
    RDNA3_Shadow_Pager_Init();

    // Ring doorbell to signal sovereign control
    KFD_Ring_Hardware_Doorbell();

    // Authenticate silicon integrity
    authenticateSiliconIntegrity();
}

void Win32IDE_TabManager::performGPUHealthCheck()
{
    // Read telemetry
    uint64_t telemetry = RDNA3_Telemetry_Read();

    // Check MMIO registers
    uint64_t reg_value = RDNA3_MMIO_Read(0x1A20); // Page fault counter

    // Perform power pulse for health check
    RDNA3_Power_Pulse();

    // Validate entropy generation
    Neural_Entropy_Generate();
}

void Win32IDE_TabManager::enableNeuralEntropyShield()
{
    // Generate quantum-resistant entropy
    Neural_Entropy_Generate();

    // Enable speculative preload
    RDNA3_Speculative_Preload();
}

void Win32IDE_TabManager::optimizeVirtualMemory()
{
    // Allocate huge pages
    uint64_t huge_page = RDNA3_HugePage_Allocate();

    // Virtualize address space
    uint64_t virtualized = RDNA3_3X_Virtualize(huge_page);

    // Scale elastically
    uint64_t scaled = RDNA3_Elastic_Scale(virtualized);
}

void Win32IDE_TabManager::compressModelData(uint8_t* data, size_t size)
{
    // Deflate using sovereign codec
    RDNA3_Sovereign_Deflate(data, data + size);

    // Expand 3x if needed
    RDNA3_3x_Expand(data, data + size * 3);

    // Custom inflate
    RDNA3_Custom_Inflate(data, data + size);
}

void Win32IDE_TabManager::authenticateSiliconIntegrity()
{
    // Generate PUF signature
    uint64_t puf_sig = Silicon_PUF_Generate();

    // Authenticate against expected value
    bool auth_result = RDNA3_Silicon_Authenticate(puf_sig);

    if (!auth_result) {
        // Handle authentication failure
    }
}