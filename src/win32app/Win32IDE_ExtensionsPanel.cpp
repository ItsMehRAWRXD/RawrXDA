// ============================================================================
// Win32IDE_ExtensionsPanel.cpp — Extensions View with search, install, manage
// ============================================================================
// VS Code parity: Extensions panel with:
//   - Search / filter installed extensions
//   - Install from .vsix file (local)
//   - Enable / Disable toggle per extension
//   - Uninstall (remove .vsix)
//   - Extension info display (name, version, state)
//   - Reload extensions
// ============================================================================

#include "Win32IDE.h"
#include <commctrl.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

// ── Control IDs ────────────────────────────────────────────────────────────
#define IDC_EXT_SEARCH        11801
#define IDC_EXT_LIST          11802
#define IDM_EXT_INSTALL       11810
#define IDM_EXT_ENABLE        11811
#define IDM_EXT_DISABLE       11812
#define IDM_EXT_UNINSTALL     11813
#define IDM_EXT_RELOAD        11814
#define IDC_EXT_INFO          11815

// ── Extension Data ─────────────────────────────────────────────────────────
namespace {

struct ExtensionInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    bool enabled;
    std::string filePath;
};

static std::vector<ExtensionInfo> s_loadedExtensions;

static std::string getExtensionsDir() {
    char cwd[MAX_PATH] = {};
    GetCurrentDirectoryA(MAX_PATH, cwd);
    std::string dir = std::string(cwd) + "\\extensions";
    fs::create_directories(dir);
    return dir;
}

static std::string getStateFilePath() {
    return getExtensionsDir() + "\\extensions_state.txt";
}

static std::map<std::string, bool> loadStateFile() {
    std::map<std::string, bool> state;
    std::ifstream f(getStateFilePath());
    if (!f.is_open()) return state;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string id = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            state[id] = (val == "1" || val == "true");
        }
    }
    return state;
}

static void saveStateFile(const std::vector<ExtensionInfo>& exts) {
    std::ofstream f(getStateFilePath());
    if (!f.is_open()) return;
    f << "# RawrXD Extension State (id=0|1)\n";
    for (const auto& e : exts) {
        f << e.id << "=" << (e.enabled ? "1" : "0") << "\n";
    }
}

static void scanExtensions() {
    s_loadedExtensions.clear();
    std::string extDir = getExtensionsDir();
    auto stateMap = loadStateFile();

    if (!fs::exists(extDir)) return;

    for (const auto& entry : fs::directory_iterator(extDir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (ext != ".vsix" && ext != ".dll" && ext != ".so") continue;

        ExtensionInfo info;
        info.id = entry.path().stem().string();
        info.name = info.id;
        info.version = "local";
        info.filePath = entry.path().string();

        // Try to parse name from filename (publisher.name-version.vsix)
        std::string stem = info.id;
        size_t dash = stem.rfind('-');
        if (dash != std::string::npos) {
            info.version = stem.substr(dash + 1);
            info.name = stem.substr(0, dash);
        }

        auto it = stateMap.find(info.id);
        info.enabled = (it != stateMap.end()) ? it->second : true;

        info.description = ext == ".vsix" ? "VS Code Extension (VSIX)"
                         : ext == ".dll" ? "Native Plugin (DLL)"
                         : "Shared Library Plugin";

        s_loadedExtensions.push_back(std::move(info));
    }

    // Sort alphabetically
    std::sort(s_loadedExtensions.begin(), s_loadedExtensions.end(),
              [](const ExtensionInfo& a, const ExtensionInfo& b) {
                  return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
              });
}

} // anonymous namespace

// ============================================================================
// CREATE EXTENSIONS VIEW
// ============================================================================

void Win32IDE::createExtensionsView(HWND hwndParent) {
    HWND parent = hwndParent ? hwndParent : m_hwndSidebar;
    if (!parent) return;

    int w = m_sidebarWidth - 10;
    int y = 5;

    // ── Search bar ─────────────────────────────────────────────────────────
    CreateWindowExA(0, "STATIC", "Extensions", WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, y, 80, 16, parent, nullptr, m_hInstance, nullptr);
    y += 18;

    m_hwndExtensionSearch = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        5, y, w, 22, parent,
        (HMENU)(UINT_PTR)IDC_EXT_SEARCH, m_hInstance, nullptr);
    SendMessageA(m_hwndExtensionSearch, EM_SETCUEBANNER, FALSE,
                 (LPARAM)L"Search installed extensions...");
    y += 26;

    // ── Action buttons ─────────────────────────────────────────────────────
    int bw = (w - 15) / 4;
    CreateWindowExA(0, "BUTTON", "Install", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5, y, bw, 22, parent, (HMENU)(UINT_PTR)IDM_EXT_INSTALL, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Enable", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5 + bw + 5, y, bw, 22, parent, (HMENU)(UINT_PTR)IDM_EXT_ENABLE, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Disable", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5 + 2 * (bw + 5), y, bw, 22, parent, (HMENU)(UINT_PTR)IDM_EXT_DISABLE, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5 + 3 * (bw + 5), y, bw, 22, parent, (HMENU)(UINT_PTR)IDM_EXT_UNINSTALL, m_hInstance, nullptr);
    y += 26;

    // ── Extensions ListView ────────────────────────────────────────────────
    m_hwndExtensionsList = CreateWindowExA(
        WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        5, y, w, 280, parent,
        (HMENU)(UINT_PTR)IDC_EXT_LIST, m_hInstance, nullptr);

    ListView_SetExtendedListViewStyle(m_hwndExtensionsList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);

    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;

    col.pszText = (LPSTR)"Name";
    col.cx = 130;
    ListView_InsertColumn(m_hwndExtensionsList, 0, &col);

    col.pszText = (LPSTR)"Version";
    col.cx = 60;
    ListView_InsertColumn(m_hwndExtensionsList, 1, &col);

    col.pszText = (LPSTR)"State";
    col.cx = 60;
    ListView_InsertColumn(m_hwndExtensionsList, 2, &col);
    y += 284;

    // ── Info panel ─────────────────────────────────────────────────────────
    CreateWindowExA(0, "STATIC", "Details:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, y, 60, 14, parent, nullptr, m_hInstance, nullptr);
    y += 16;

    m_hwndExtensionDetails = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        5, y, w, 80, parent,
        (HMENU)(UINT_PTR)IDC_EXT_INFO, m_hInstance, nullptr);

    // Load extensions
    loadInstalledExtensions();
}

// ============================================================================
// LOAD / REFRESH EXTENSIONS
// ============================================================================

void Win32IDE::loadInstalledExtensions() {
    if (!m_hwndExtensionsList) return;

    scanExtensions();

    ListView_DeleteAllItems(m_hwndExtensionsList);

    // Apply search filter
    char filterBuf[256] = {};
    if (m_hwndExtensionSearch)
        GetWindowTextA(m_hwndExtensionSearch, filterBuf, sizeof(filterBuf));
    std::string filter(filterBuf);

    for (const auto& ext : s_loadedExtensions) {
        // Filter check
        if (!filter.empty()) {
            std::string nameLower = ext.name;
            std::string filterLower = filter;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
            if (nameLower.find(filterLower) == std::string::npos) continue;
        }

        LVITEMA item = {};
        item.mask = LVIF_TEXT;
        item.iItem = ListView_GetItemCount(m_hwndExtensionsList);
        item.pszText = (LPSTR)ext.name.c_str();
        int idx = ListView_InsertItem(m_hwndExtensionsList, &item);

        ListView_SetItemText(m_hwndExtensionsList, idx, 1, (LPSTR)ext.version.c_str());
        ListView_SetItemText(m_hwndExtensionsList, idx, 2,
                             (LPSTR)(ext.enabled ? "Enabled" : "Disabled"));
    }

    // Update info count
    if (m_hwndExtensionDetails) {
        std::string info = "Installed: " + std::to_string(s_loadedExtensions.size()) + " extension(s)\r\n";
        int enabled = 0;
        for (const auto& e : s_loadedExtensions) if (e.enabled) enabled++;
        info += "Enabled: " + std::to_string(enabled) + " / " + std::to_string(s_loadedExtensions.size());
        SetWindowTextA(m_hwndExtensionDetails, info.c_str());
    }
}

// ============================================================================
// EXTENSION PANEL COMMAND HANDLER
// ============================================================================

// ============================================================================
// INDIVIDUAL EXTENSION METHODS (match header declarations)
// ============================================================================

void Win32IDE::searchExtensions(const std::string& query) {
    if (m_hwndExtensionSearch)
        SetWindowTextA(m_hwndExtensionSearch, query.c_str());
    loadInstalledExtensions();
}

void Win32IDE::installExtension(const std::string& extensionId) {
    // Install by ID — search extensions dir or marketplace stub
    appendToOutput("[Extensions] installExtension(\"" + extensionId + "\") — use Install button for local VSIX.\n",
                   "Extensions", OutputSeverity::Info);
}

void Win32IDE::uninstallExtension(const std::string& extensionId) {
    for (auto it = s_loadedExtensions.begin(); it != s_loadedExtensions.end(); ++it) {
        if (it->id == extensionId || it->name == extensionId) {
            std::error_code ec;
            fs::remove(it->filePath, ec);
            appendToOutput("[Extensions] Uninstalled: " + it->name + "\n",
                           "Extensions", OutputSeverity::Info);
            s_loadedExtensions.erase(it);
            break;
        }
    }
    saveStateFile(s_loadedExtensions);
    loadInstalledExtensions();
}

void Win32IDE::enableExtension(const std::string& extensionId) {
    for (auto& ext : s_loadedExtensions) {
        if (ext.id == extensionId || ext.name == extensionId) {
            ext.enabled = true;
            break;
        }
    }
    saveStateFile(s_loadedExtensions);
    loadInstalledExtensions();
}

void Win32IDE::disableExtension(const std::string& extensionId) {
    for (auto& ext : s_loadedExtensions) {
        if (ext.id == extensionId || ext.name == extensionId) {
            ext.enabled = false;
            break;
        }
    }
    saveStateFile(s_loadedExtensions);
    loadInstalledExtensions();
}

void Win32IDE::updateExtension(const std::string& extensionId) {
    appendToOutput("[Extensions] updateExtension(\"" + extensionId + "\") — no marketplace backend yet.\n",
                   "Extensions", OutputSeverity::Info);
}

void Win32IDE::showExtensionDetails(const std::string& extensionId) {
    if (!m_hwndExtensionDetails) return;
    for (const auto& ext : s_loadedExtensions) {
        if (ext.id == extensionId || ext.name == extensionId) {
            std::string info = "Name: " + ext.name + "\r\n"
                             + "ID: " + ext.id + "\r\n"
                             + "Version: " + ext.version + "\r\n"
                             + "State: " + (ext.enabled ? "Enabled" : "Disabled") + "\r\n"
                             + "Type: " + ext.description + "\r\n"
                             + "Path: " + ext.filePath;
            SetWindowTextA(m_hwndExtensionDetails, info.c_str());
            return;
        }
    }
    SetWindowTextA(m_hwndExtensionDetails, "Extension not found.");
}

// ============================================================================
// EXTENSION PANEL COMMAND HANDLER (WM_COMMAND dispatch)
// ============================================================================

void Win32IDE::handleExtensionCommand(int commandId) {
    switch (commandId) {

    case IDM_EXT_INSTALL: {
        // Open file dialog for .vsix files
        char path[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = m_hwndMain;
        ofn.lpstrFilter = "VS Code Extensions (*.vsix)\0*.vsix\0"
                         "Native Plugins (*.dll)\0*.dll\0"
                         "All Files (*.*)\0*.*\0";
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (GetOpenFileNameA(&ofn)) {
            std::string extDir = getExtensionsDir();
            fs::path src(path);
            fs::path dst = fs::path(extDir) / src.filename();

            std::error_code ec;
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);

            if (!ec) {
                appendToOutput("[Extensions] Installed: " + src.filename().string() + "\n",
                               "Extensions", OutputSeverity::Info);
                loadInstalledExtensions();
            } else {
                appendToOutput("[Extensions] Failed to install: " + ec.message() + "\n",
                               "Extensions", OutputSeverity::Error);
            }
        }
        break;
    }

    case IDM_EXT_ENABLE: {
        int sel = ListView_GetNextItem(m_hwndExtensionsList, -1, LVNI_SELECTED);
        if (sel < 0) break;
        char nameBuf[256] = {};
        ListView_GetItemText(m_hwndExtensionsList, sel, 0, nameBuf, sizeof(nameBuf));
        enableExtension(nameBuf);
        break;
    }

    case IDM_EXT_DISABLE: {
        int sel = ListView_GetNextItem(m_hwndExtensionsList, -1, LVNI_SELECTED);
        if (sel < 0) break;
        char nameBuf[256] = {};
        ListView_GetItemText(m_hwndExtensionsList, sel, 0, nameBuf, sizeof(nameBuf));
        disableExtension(nameBuf);
        break;
    }

    case IDM_EXT_UNINSTALL: {
        int sel = ListView_GetNextItem(m_hwndExtensionsList, -1, LVNI_SELECTED);
        if (sel < 0) break;

        char nameBuf[256] = {};
        ListView_GetItemText(m_hwndExtensionsList, sel, 0, nameBuf, sizeof(nameBuf));

        int confirm = MessageBoxA(m_hwndMain,
            (std::string("Uninstall extension \"") + nameBuf + "\"?").c_str(),
            "Uninstall Extension", MB_YESNO | MB_ICONQUESTION);
        if (confirm != IDYES) break;

        uninstallExtension(nameBuf);
        break;
    }

    case IDM_EXT_RELOAD:
        loadInstalledExtensions();
        break;

    default:
        break;
    }
}
