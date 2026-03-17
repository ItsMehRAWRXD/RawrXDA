// ============================================================================
// Win32IDE_ExtensionToggles.cpp — Extension Toggle UI (Checkboxes/Switches)
// ============================================================================
//
// PURPOSE:
//   Provides a visual toggle interface for installed extensions with:
//     - Enable/Disable checkboxes
//     - Model selection dropdowns
//     - Tool activation toggles
//     - Real-time settings persistence
//     - Integration with IDE core
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include "../marketplace/extension_marketplace.hpp"
#include "../marketplace/extension_auto_installer.hpp"
#include <nlohmann/json.hpp>
#include <commctrl.h>
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <shlobj.h>

using namespace RawrXD::Extensions;

// ============================================================================
// Extension Toggle Data Model
// ============================================================================

struct ExtensionToggle {
    std::string id;
    std::string displayName;
    bool enabled;
    bool hasModelSelection;
    bool hasToolSelection;
    std::vector<std::string> availableModels;
    std::vector<std::string> availableTools;
    std::string selectedModel;
    std::vector<std::string> enabledTools;

    HWND checkboxHwnd;
    HWND modelComboHwnd;
    HWND toolsListHwnd;
};

static std::vector<ExtensionToggle> s_toggles;
static std::mutex s_toggleMutex;

static HWND s_hwndTogglePanel = nullptr;
static HWND s_hwndScrollView = nullptr;
static bool s_toggleClassRegistered = false;
static const wchar_t* TOGGLE_PANEL_CLASS = L"RawrXD_ExtensionToggles";

// Control IDs
#define ID_TOGGLE_SCROLL          7000
#define ID_EXTENSION_CHECKBOX_BASE 7100  // 7100-7199
#define ID_MODEL_COMBO_BASE       7200  // 7200-7299
#define ID_TOOL_LIST_BASE         7300  // 7300-7399
#define ID_BTN_INSTALL_AMAZONQ    7400
#define ID_BTN_INSTALL_COPILOT    7401
#define ID_BTN_SYNC_MARKETPLACE   7402
#define ID_BTN_SAVE_SETTINGS      7403
#define ID_BTN_SELECT_ALL         7404
#define ID_BTN_DESELECT_ALL       7405

// ============================================================================
// AI Model Definitions (for extensions that support model selection)
// ============================================================================

static const char* AMAZON_Q_MODELS[] = {
    "amazon-q-developer-agent",
    "amazon-q-code-transformation",
    "amazon-q-inline-completion",
    nullptr
};

static const char* GITHUB_COPILOT_MODELS[] = {
    "gpt-4-turbo",
    "gpt-4",
    "gpt-3.5-turbo",
    "claude-3-opus",
    "claude-3-sonnet",
    nullptr
};

static const char* CONTINUE_MODELS[] = {
    "gpt-4",
    "claude-3-opus",
    "claude-3-sonnet",
    "llama-3-70b",
    "codellama-34b",
    "deepseek-coder-33b",
    "custom-local-model",
    nullptr
};

// Tool definitions
static const char* AMAZON_Q_TOOLS[] = {
    "Code Completion",
    "Code Transformation",
    "Security Scanning",
    "Test Generation",
    "Documentation Generation",
    nullptr
};

static const char* GITHUB_COPILOT_TOOLS[] = {
    "Inline Completion",
    "Chat Interface",
    "Code Explanation",
    "Test Generation",
    "Code Review",
    nullptr
};

static const char* CONTINUE_TOOLS[] = {
    "Autocomplete",
    "Chat",
    "Edit",
    "Generate",
    "Custom Commands",
    nullptr
};

// ============================================================================
// Seed Extension Toggles
// ============================================================================

static void seedExtensionToggles() {
    static bool seeded = false;
    if (seeded) return;
    seeded = true;

    std::lock_guard<std::mutex> lock(s_toggleMutex);

    // Helper lambda
    auto addToggle = [](const char* id, const char* name, bool enabled,
                        const char** models, const char** tools) {
        ExtensionToggle toggle;
        toggle.id = id;
        toggle.displayName = name;
        toggle.enabled = enabled;
        toggle.hasModelSelection = (models != nullptr);
        toggle.hasToolSelection = (tools != nullptr);

        if (models) {
            for (int i = 0; models[i] != nullptr; i++) {
                toggle.availableModels.push_back(models[i]);
            }
            if (!toggle.availableModels.empty()) {
                toggle.selectedModel = toggle.availableModels[0];
            }
        }

        if (tools) {
            for (int i = 0; tools[i] != nullptr; i++) {
                toggle.availableTools.push_back(tools[i]);
                // Enable all tools by default
                toggle.enabledTools.push_back(tools[i]);
            }
        }

        toggle.checkboxHwnd = nullptr;
        toggle.modelComboHwnd = nullptr;
        toggle.toolsListHwnd = nullptr;

        s_toggles.push_back(toggle);
    };

    // Add priority extensions
    addToggle("amazonwebservices.amazon-q-vscode", "Amazon Q", true,
              AMAZON_Q_MODELS, AMAZON_Q_TOOLS);
    addToggle("GitHub.copilot", "GitHub Copilot", true,
              GITHUB_COPILOT_MODELS, GITHUB_COPILOT_TOOLS);
    addToggle("GitHub.copilot-chat", "GitHub Copilot Chat", true,
              nullptr, GITHUB_COPILOT_TOOLS);
    addToggle("Continue.continue", "Continue", true,
              CONTINUE_MODELS, CONTINUE_TOOLS);
    addToggle("TabNine.tabnine-vscode", "TabNine AI", false,
              nullptr, nullptr);
    addToggle("ms-vscode.cpptools", "C/C++ Tools", true,
              nullptr, nullptr);
    addToggle("ms-python.python", "Python", true,
              nullptr, nullptr);
    addToggle("eamodio.gitlens", "GitLens", false,
              nullptr, nullptr);
}

// ============================================================================
// Settings Persistence
// ============================================================================

static void saveToggleSettings() {
    // Save to %APPDATA%\RawrXD\extension_toggles.json
    std::string settingsPath;
#ifdef _WIN32
    wchar_t appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        char buf[MAX_PATH * 2] = {};
        WideCharToMultiByte(CP_UTF8, 0, appData, -1, buf, sizeof(buf), nullptr, nullptr);
        settingsPath = std::string(buf) + "\\RawrXD\\extension_toggles.json";
    } else {
        settingsPath = "extension_toggles.json";
    }
#else
    settingsPath = ".rawrxd/extension_toggles.json";
#endif

    std::filesystem::create_directories(std::filesystem::path(settingsPath).parent_path());

    std::ofstream file(settingsPath);
    if (!file.is_open()) return;

    // Simple JSON format
    file << "{\n";
    file << "  \"extensions\": [\n";

    std::lock_guard<std::mutex> lock(s_toggleMutex);
    for (size_t i = 0; i < s_toggles.size(); i++) {
        const auto& toggle = s_toggles[i];
        file << "    {\n";
        file << "      \"id\": \"" << toggle.id << "\",\n";
        file << "      \"enabled\": " << (toggle.enabled ? "true" : "false") << ",\n";
        
        if (toggle.hasModelSelection && !toggle.selectedModel.empty()) {
            file << "      \"selectedModel\": \"" << toggle.selectedModel << "\",\n";
        }

        if (toggle.hasToolSelection && !toggle.enabledTools.empty()) {
            file << "      \"enabledTools\": [";
            for (size_t j = 0; j < toggle.enabledTools.size(); j++) {
                file << "\"" << toggle.enabledTools[j] << "\"";
                if (j < toggle.enabledTools.size() - 1) file << ", ";
            }
            file << "]\n";
        } else {
            file << "      \"enabledTools\": []\n";
        }

        file << "    }";
        if (i < s_toggles.size() - 1) file << ",";
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";
}

static void loadToggleSettings() {
    std::string settingsPath;
#ifdef _WIN32
    wchar_t appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        char buf[MAX_PATH * 2] = {};
        WideCharToMultiByte(CP_UTF8, 0, appData, -1, buf, sizeof(buf), nullptr, nullptr);
        settingsPath = std::string(buf) + "\\RawrXD\\extension_toggles.json";
    } else {
        settingsPath = "extension_toggles.json";
    }
#else
    settingsPath = ".rawrxd/extension_toggles.json";
#endif

    std::ifstream file(settingsPath);
    if (!file.is_open()) return;

    nlohmann::json j;
    try {
        file >> j;
    } catch (...) { return; }

    auto exts = j.find("extensions");
    if (exts == j.end() || !exts->is_array()) return;

    std::lock_guard<std::mutex> lock(s_toggleMutex);
    for (const auto& ext : *exts) {
        std::string id = ext.value("id", "");
        if (id.empty()) continue;

        for (auto& toggle : s_toggles) {
            if (toggle.id != id) continue;

            toggle.enabled = ext.value("enabled", toggle.enabled);
            if (ext.contains("selectedModel") && ext["selectedModel"].is_string())
                toggle.selectedModel = ext["selectedModel"].get<std::string>();
            if (ext.contains("enabledTools") && ext["enabledTools"].is_array()) {
                toggle.enabledTools.clear();
                for (const auto& t : ext["enabledTools"])
                    if (t.is_string())
                        toggle.enabledTools.push_back(t.get<std::string>());
            }
            break;
        }
    }
}

// ============================================================================
// Toggle UI Creation
// ============================================================================

static void createToggleControls(HWND parent) {
    int yOffset = 10;
    const int xMargin = 20;
    const int controlHeight = 25;
    const int spacing = 10;

    std::lock_guard<std::mutex> lock(s_toggleMutex);

    for (size_t i = 0; i < s_toggles.size(); i++) {
        auto& toggle = s_toggles[i];

        // Extension checkbox
        std::wstring displayName = std::wstring(toggle.displayName.begin(), toggle.displayName.end());
        toggle.checkboxHwnd = CreateWindowW(
            L"BUTTON", displayName.c_str(),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            xMargin, yOffset, 300, controlHeight,
            parent, (HMENU)(ID_EXTENSION_CHECKBOX_BASE + i), GetModuleHandle(nullptr), nullptr
        );
        SendMessageW(toggle.checkboxHwnd, BM_SETCHECK, toggle.enabled ? BST_CHECKED : BST_UNCHECKED, 0);

        yOffset += controlHeight + 5;

        // Model selection dropdown (if applicable)
        if (toggle.hasModelSelection && !toggle.availableModels.empty()) {
            CreateWindowW(L"STATIC", L"  Model:",
                          WS_CHILD | WS_VISIBLE | SS_LEFT,
                          xMargin + 20, yOffset, 80, controlHeight,
                          parent, nullptr, GetModuleHandle(nullptr), nullptr);

            toggle.modelComboHwnd = CreateWindowW(
                L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                xMargin + 100, yOffset, 250, 200,
                parent, (HMENU)(ID_MODEL_COMBO_BASE + i), GetModuleHandle(nullptr), nullptr
            );

            for (const auto& model : toggle.availableModels) {
                std::wstring wModel(model.begin(), model.end());
                SendMessageW(toggle.modelComboHwnd, CB_ADDSTRING, 0, (LPARAM)wModel.c_str());
            }
            SendMessageW(toggle.modelComboHwnd, CB_SETCURSEL, 0, 0);

            yOffset += controlHeight + 5;
        }

        // Tool selection checkboxes (if applicable)
        if (toggle.hasToolSelection && !toggle.availableTools.empty()) {
            CreateWindowW(L"STATIC", L"  Tools:",
                          WS_CHILD | WS_VISIBLE | SS_LEFT,
                          xMargin + 20, yOffset, 80, controlHeight,
                          parent, nullptr, GetModuleHandle(nullptr), nullptr);
            yOffset += controlHeight + 5;

            for (size_t j = 0; j < toggle.availableTools.size(); j++) {
                std::wstring wTool(toggle.availableTools[j].begin(), toggle.availableTools[j].end());
                HWND toolCheck = CreateWindowW(
                    L"BUTTON", wTool.c_str(),
                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                    xMargin + 40, yOffset, 280, controlHeight,
                    parent, (HMENU)(ID_TOOL_LIST_BASE + i * 10 + j), GetModuleHandle(nullptr), nullptr
                );

                // Check if tool is enabled
                bool toolEnabled = std::find(toggle.enabledTools.begin(), toggle.enabledTools.end(),
                                              toggle.availableTools[j]) != toggle.enabledTools.end();
                SendMessageW(toolCheck, BM_SETCHECK, toolEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

                yOffset += controlHeight + 3;
            }
        }

        yOffset += spacing;
    }
}

// ============================================================================
// Window Procedure
// ============================================================================

static LRESULT CALLBACK TogglePanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Create action buttons at top
            CreateWindowW(L"BUTTON", L"Install Amazon Q",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          10, 10, 150, 30, hwnd, (HMENU)ID_BTN_INSTALL_AMAZONQ,
                          GetModuleHandle(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Install GitHub Copilot",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          170, 10, 180, 30, hwnd, (HMENU)ID_BTN_INSTALL_COPILOT,
                          GetModuleHandle(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Sync Marketplace",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          360, 10, 150, 30, hwnd, (HMENU)ID_BTN_SYNC_MARKETPLACE,
                          GetModuleHandle(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Select All",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          520, 10, 100, 30, hwnd, (HMENU)ID_BTN_SELECT_ALL,
                          GetModuleHandle(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Deselect All",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          630, 10, 100, 30, hwnd, (HMENU)ID_BTN_DESELECT_ALL,
                          GetModuleHandle(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Save Settings",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          740, 10, 120, 30, hwnd, (HMENU)ID_BTN_SAVE_SETTINGS,
                          GetModuleHandle(nullptr), nullptr);

            // Create scrollable container for toggles
            RECT rc;
            GetClientRect(hwnd, &rc);
            s_hwndScrollView = CreateWindowExW(
                0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER,
                0, 50, rc.right, rc.bottom - 50,
                hwnd, (HMENU)ID_TOGGLE_SCROLL, GetModuleHandle(nullptr), nullptr
            );

            seedExtensionToggles();
            loadToggleSettings();
            createToggleControls(s_hwndScrollView);
            return 0;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == ID_BTN_INSTALL_AMAZONQ) {
                MessageBoxW(hwnd, L"Installing Amazon Q...", L"Extension Installer", MB_OK | MB_ICONINFORMATION);
                installAIExtensions();
                return 0;
            }
            else if (wmId == ID_BTN_INSTALL_COPILOT) {
                MessageBoxW(hwnd, L"Installing GitHub Copilot...", L"Extension Installer", MB_OK | MB_ICONINFORMATION);
                ExtensionAutoInstaller::instance().installExtension("GitHub.copilot");
                ExtensionAutoInstaller::instance().installExtension("GitHub.copilot-chat");
                return 0;
            }
            else if (wmId == ID_BTN_SYNC_MARKETPLACE) {
                MessageBoxW(hwnd, L"Syncing marketplace catalog... (This may take several minutes)", L"Marketplace Sync", MB_OK | MB_ICONINFORMATION);
                ExtensionAutoInstaller::instance().syncMarketplaceCatalog(5000);
                return 0;
            }
            else if (wmId == ID_BTN_SELECT_ALL) {
                std::lock_guard<std::mutex> lock(s_toggleMutex);
                for (auto& toggle : s_toggles) {
                    toggle.enabled = true;
                    if (toggle.checkboxHwnd) {
                        SendMessageW(toggle.checkboxHwnd, BM_SETCHECK, BST_CHECKED, 0);
                    }
                }
                return 0;
            }
            else if (wmId == ID_BTN_DESELECT_ALL) {
                std::lock_guard<std::mutex> lock(s_toggleMutex);
                for (auto& toggle : s_toggles) {
                    toggle.enabled = false;
                    if (toggle.checkboxHwnd) {
                        SendMessageW(toggle.checkboxHwnd, BM_SETCHECK, BST_UNCHECKED, 0);
                    }
                }
                return 0;
            }
            else if (wmId == ID_BTN_SAVE_SETTINGS) {
                // Update toggle states from UI
                std::lock_guard<std::mutex> lock(s_toggleMutex);
                for (size_t i = 0; i < s_toggles.size(); i++) {
                    auto& toggle = s_toggles[i];
                    if (toggle.checkboxHwnd) {
                        toggle.enabled = (SendMessageW(toggle.checkboxHwnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    }
                    if (toggle.modelComboHwnd) {
                        int sel = (int)SendMessageW(toggle.modelComboHwnd, CB_GETCURSEL, 0, 0);
                        if (sel >= 0 && sel < (int)toggle.availableModels.size()) {
                            toggle.selectedModel = toggle.availableModels[sel];
                        }
                    }
                }
                saveToggleSettings();
                MessageBoxW(hwnd, L"Extension settings saved!", L"Settings", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            break;
        }

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (s_hwndScrollView) {
                SetWindowPos(s_hwndScrollView, nullptr,
                             0, 50, rc.right, rc.bottom - 50,
                             SWP_NOZORDER);
            }
            return 0;
        }

        case WM_DESTROY:
            s_hwndTogglePanel = nullptr;
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Public API
// ============================================================================

void Win32IDE::CreateExtensionTogglePanel(HWND parent) {
    if (s_hwndTogglePanel && IsWindow(s_hwndTogglePanel)) {
        ShowWindow(s_hwndTogglePanel, SW_SHOW);
        return;
    }

    if (!s_toggleClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = TogglePanelProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = TOGGLE_PANEL_CLASS;
        RegisterClassExW(&wc);
        s_toggleClassRegistered = true;
    }

    RECT rc;
    GetClientRect(parent, &rc);

    s_hwndTogglePanel = CreateWindowExW(
        0, TOGGLE_PANEL_CLASS, L"Extension Toggles",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL,
        0, 0, rc.right, rc.bottom,
        parent, nullptr, GetModuleHandle(nullptr), nullptr
    );
}

void Win32IDE::ShowExtensionTogglePanel() {
    if (s_hwndTogglePanel && IsWindow(s_hwndTogglePanel)) {
        ShowWindow(s_hwndTogglePanel, SW_SHOW);
        BringWindowToTop(s_hwndTogglePanel);
    }
}

void Win32IDE::HideExtensionTogglePanel() {
    if (s_hwndTogglePanel && IsWindow(s_hwndTogglePanel)) {
        ShowWindow(s_hwndTogglePanel, SW_HIDE);
    }
}
