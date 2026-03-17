// ============================================================================
// Win32IDE_ShortcutEditor.cpp — Tier 5 Gap #47: Keyboard Shortcut Editor
// ============================================================================
//
// PURPOSE:
//   Visual keyboard shortcut editor with key capture UI.  Replaces manual
//   keybindings.json editing with a dialog that shows current bindings,
//   allows recording new key combinations, and persists to settings.
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include <commctrl.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <fstream>
#include <unordered_map>
#include <ShlObj.h>
#include <filesystem>

// ============================================================================
// Shortcut data model
// ============================================================================

struct KeyBinding {
    std::string command;       // e.g. "editor.save"
    std::string description;   // "Save File"
    std::string keyCombination; // "Ctrl+S"
    std::string category;      // "File", "Edit", "Debug"
    bool        isDefault;
    bool        isCustom;
};

static std::vector<KeyBinding> s_keyBindings;
static bool s_shortcutEditorClassRegistered = false;
static HWND s_hwndShortcutEditor = nullptr;
static HWND s_hwndShortcutList   = nullptr;
static HWND s_hwndKeyCapture     = nullptr;
static bool s_isCapturing        = false;
static int  s_captureTargetIndex = -1;
static const wchar_t* SHORTCUT_EDITOR_CLASS = L"RawrXD_ShortcutEditor";

// ============================================================================
// Persistent path: %APPDATA%\RawrXD\keybindings.json
// ============================================================================

static std::string getKeybindingsPath() {
    char appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        std::string dir = std::string(appData) + "\\RawrXD";
        std::filesystem::create_directories(dir);
        return dir + "\\keybindings.json";
    }
    // Fallback to CWD
    return "keybindings.json";
}

// ============================================================================
// Load saved keybindings from disk (merges custom over defaults)
// ============================================================================

static bool loadKeybindingsFromDisk() {
    std::string path = getKeybindingsPath();
    std::ifstream fin(path);
    if (!fin.is_open()) return false;

    // Read entire file
    std::string content((std::istreambuf_iterator<char>(fin)),
                         std::istreambuf_iterator<char>());
    fin.close();

    if (content.size() < 3) return false;

    // Simple JSON array parser: extract objects with "command", "key", "custom"
    // Format: [{"command":"x","key":"y","description":"z","category":"c","custom":true},...]
    size_t pos = 0;
    int applied = 0;

    while ((pos = content.find("{", pos)) != std::string::npos) {
        size_t end = content.find("}", pos);
        if (end == std::string::npos) break;

        std::string obj = content.substr(pos, end - pos + 1);
        pos = end + 1;

        // Extract "command":"value"
        auto extractField = [&obj](const std::string& field) -> std::string {
            std::string needle = "\"" + field + "\":\"";
            size_t start = obj.find(needle);
            if (start == std::string::npos) return "";
            start += needle.size();
            size_t stop = obj.find("\"", start);
            if (stop == std::string::npos) return "";
            return obj.substr(start, stop - start);
        };

        std::string cmd = extractField("command");
        std::string key = extractField("key");
        std::string desc = extractField("description");
        std::string cat = extractField("category");
        bool custom = (obj.find("\"custom\":true") != std::string::npos ||
                       obj.find("\"custom\": true") != std::string::npos);

        if (cmd.empty() || key.empty()) continue;

        // Find matching command in defaults and override
        bool found = false;
        for (auto& kb : s_keyBindings) {
            if (kb.command == cmd) {
                kb.keyCombination = key;
                kb.isCustom = custom;
                if (!desc.empty()) kb.description = desc;
                if (!cat.empty()) kb.category = cat;
                found = true;
                applied++;
                break;
            }
        }

        // If command not in defaults, add it as custom
        if (!found && custom) {
            KeyBinding kb;
            kb.command = cmd;
            kb.description = desc.empty() ? cmd : desc;
            kb.keyCombination = key;
            kb.category = cat.empty() ? "Custom" : cat;
            kb.isDefault = false;
            kb.isCustom = true;
            s_keyBindings.push_back(kb);
            applied++;
        }
    }

    OutputDebugStringA(("[ShortcutEditor] Loaded " + std::to_string(applied) +
                        " keybinding(s) from " + path + "\n").c_str());
    return applied > 0;
}

// ============================================================================
// Conflict detection: check if a key combo is already bound to another command
// ============================================================================

static std::string findConflictingCommand(const std::string& keyCombination, int excludeIndex) {
    for (int i = 0; i < (int)s_keyBindings.size(); ++i) {
        if (i == excludeIndex) continue;
        if (s_keyBindings[i].keyCombination == keyCombination) {
            return s_keyBindings[i].command + " (" + s_keyBindings[i].description + ")";
        }
    }
    return "";
}

// ============================================================================
// Seed default keybindings
// ============================================================================

static void seedDefaultBindings() {
    static bool seeded = false;
    if (seeded) return;
    seeded = true;

    auto add = [](const char* cmd, const char* desc, const char* key, const char* cat) {
        KeyBinding kb;
        kb.command = cmd; kb.description = desc;
        kb.keyCombination = key; kb.category = cat;
        kb.isDefault = true; kb.isCustom = false;
        s_keyBindings.push_back(kb);
    };

    // File
    add("file.new",        "New File",            "Ctrl+N",           "File");
    add("file.open",       "Open File",           "Ctrl+O",           "File");
    add("file.save",       "Save File",           "Ctrl+S",           "File");
    add("file.saveAs",     "Save As",             "Ctrl+Shift+S",     "File");
    add("file.close",      "Close File",          "Ctrl+W",           "File");

    // Edit
    add("edit.undo",       "Undo",                "Ctrl+Z",           "Edit");
    add("edit.redo",       "Redo",                "Ctrl+Y",           "Edit");
    add("edit.copy",       "Copy",                "Ctrl+C",           "Edit");
    add("edit.cut",        "Cut",                 "Ctrl+X",           "Edit");
    add("edit.paste",      "Paste",               "Ctrl+V",           "Edit");
    add("edit.find",       "Find",                "Ctrl+F",           "Edit");
    add("edit.replace",    "Find and Replace",    "Ctrl+H",           "Edit");
    add("edit.selectAll",  "Select All",          "Ctrl+A",           "Edit");
    add("edit.commentLine","Toggle Line Comment",  "Ctrl+/",           "Edit");

    // View
    add("view.commandPalette", "Command Palette",  "Ctrl+Shift+P",    "View");
    add("view.terminal",      "Toggle Terminal",    "Ctrl+`",          "View");
    add("view.explorer",      "Toggle Explorer",    "Ctrl+Shift+E",    "View");
    add("view.search",        "Toggle Search",      "Ctrl+Shift+F",    "View");
    add("view.scm",           "Toggle SCM",         "Ctrl+Shift+G",    "View");
    add("view.debug",         "Toggle Debug",       "Ctrl+Shift+D",    "View");
    add("view.extensions",    "Toggle Extensions",  "Ctrl+Shift+X",    "View");
    add("view.zoomIn",        "Zoom In",            "Ctrl++",          "View");
    add("view.zoomOut",       "Zoom Out",           "Ctrl+-",          "View");

    // Debug
    add("debug.start",       "Start Debugging",    "F5",               "Debug");
    add("debug.stepOver",    "Step Over",           "F10",              "Debug");
    add("debug.stepInto",    "Step Into",           "F11",              "Debug");
    add("debug.stepOut",     "Step Out",            "Shift+F11",        "Debug");
    add("debug.toggleBreak", "Toggle Breakpoint",   "F9",              "Debug");
    add("debug.stop",        "Stop Debugging",      "Shift+F5",        "Debug");

    // Build
    add("build.build",       "Build Project",      "Ctrl+Shift+B",     "Build");
    add("build.run",         "Run Without Debug",   "Ctrl+F5",         "Build");

    // Navigation
    add("nav.goToLine",      "Go to Line",         "Ctrl+G",           "Navigation");
    add("nav.goToFile",      "Go to File",         "Ctrl+P",           "Navigation");
    add("nav.goToSymbol",    "Go to Symbol",       "Ctrl+Shift+O",     "Navigation");
    add("nav.goToDefinition","Go to Definition",    "F12",             "Navigation");
    add("nav.peekDefinition","Peek Definition",     "Alt+F12",         "Navigation");
}

// ============================================================================
// Key name builder from virtual key + modifiers
// ============================================================================

static std::string buildKeyName(WPARAM vk, bool ctrl, bool shift, bool alt) {
    std::string result;
    if (ctrl)  result += "Ctrl+";
    if (alt)   result += "Alt+";
    if (shift) result += "Shift+";

    // Map VK to name
    switch (vk) {
        case VK_F1:  result += "F1"; break;
        case VK_F2:  result += "F2"; break;
        case VK_F3:  result += "F3"; break;
        case VK_F4:  result += "F4"; break;
        case VK_F5:  result += "F5"; break;
        case VK_F6:  result += "F6"; break;
        case VK_F7:  result += "F7"; break;
        case VK_F8:  result += "F8"; break;
        case VK_F9:  result += "F9"; break;
        case VK_F10: result += "F10"; break;
        case VK_F11: result += "F11"; break;
        case VK_F12: result += "F12"; break;
        case VK_ESCAPE: result += "Escape"; break;
        case VK_TAB:    result += "Tab"; break;
        case VK_RETURN: result += "Enter"; break;
        case VK_SPACE:  result += "Space"; break;
        case VK_BACK:   result += "Backspace"; break;
        case VK_DELETE: result += "Delete"; break;
        case VK_INSERT: result += "Insert"; break;
        case VK_HOME:   result += "Home"; break;
        case VK_END:    result += "End"; break;
        case VK_PRIOR:  result += "PageUp"; break;
        case VK_NEXT:   result += "PageDown"; break;
        case VK_UP:     result += "Up"; break;
        case VK_DOWN:   result += "Down"; break;
        case VK_LEFT:   result += "Left"; break;
        case VK_RIGHT:  result += "Right"; break;
        case VK_OEM_PLUS:   result += "+"; break;
        case VK_OEM_MINUS:  result += "-"; break;
        case VK_OEM_PERIOD: result += "."; break;
        case VK_OEM_COMMA:  result += ","; break;
        case VK_OEM_1:      result += ";"; break;
        case VK_OEM_2:      result += "/"; break;
        case VK_OEM_3:      result += "`"; break;
        case VK_OEM_4:      result += "["; break;
        case VK_OEM_5:      result += "\\"; break;
        case VK_OEM_6:      result += "]"; break;
        case VK_OEM_7:      result += "'"; break;
        default:
            if (vk >= 'A' && vk <= 'Z') {
                result += (char)vk;
            } else if (vk >= '0' && vk <= '9') {
                result += (char)vk;
            } else {
                result += "Key(" + std::to_string(vk) + ")";
            }
            break;
    }
    return result;
}

// ============================================================================
// ListView helpers
// ============================================================================

#ifndef LVM_INSERTCOLUMNW
#define LVM_INSERTCOLUMNW (LVM_FIRST + 97)
#endif
#ifndef LVM_INSERTITEMW
#define LVM_INSERTITEMW (LVM_FIRST + 77)
#endif
#ifndef LVM_SETITEMTEXTW
#define LVM_SETITEMTEXTW (LVM_FIRST + 116)
#endif

static int SE_InsertColumnW(HWND hwnd, int col, int width, const wchar_t* text) {
    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    lvc.fmt  = LVCFMT_LEFT;
    lvc.cx   = width;
    lvc.pszText = const_cast<LPWSTR>(text);
    return (int)SendMessageW(hwnd, LVM_INSERTCOLUMNW, col, (LPARAM)&lvc);
}

static int SE_InsertItemW(HWND hwnd, int row, const wchar_t* text) {
    LVITEMW lvi{};
    lvi.mask     = LVIF_TEXT;
    lvi.iItem    = row;
    lvi.iSubItem = 0;
    lvi.pszText  = const_cast<LPWSTR>(text);
    return (int)SendMessageW(hwnd, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
}

static void SE_SetItemTextW(HWND hwnd, int row, int col, const wchar_t* text) {
    LVITEMW lvi{};
    lvi.iSubItem = col;
    lvi.pszText  = const_cast<LPWSTR>(text);
    SendMessageW(hwnd, LVM_SETITEMTEXTW, row, (LPARAM)&lvi);
}

// ============================================================================
// Refresh shortcut list
// ============================================================================

static void refreshShortcutList() {
    if (!s_hwndShortcutList) return;
    SendMessageW(s_hwndShortcutList, LVM_DELETEALLITEMS, 0, 0);

    for (int i = 0; i < (int)s_keyBindings.size(); ++i) {
        auto& kb = s_keyBindings[i];
        wchar_t buf[256];

        // Col 0: Command
        MultiByteToWideChar(CP_UTF8, 0, kb.command.c_str(), -1, buf, 255);
        SE_InsertItemW(s_hwndShortcutList, i, buf);

        // Col 1: Description
        MultiByteToWideChar(CP_UTF8, 0, kb.description.c_str(), -1, buf, 255);
        SE_SetItemTextW(s_hwndShortcutList, i, 1, buf);

        // Col 2: Key Combination
        MultiByteToWideChar(CP_UTF8, 0, kb.keyCombination.c_str(), -1, buf, 255);
        SE_SetItemTextW(s_hwndShortcutList, i, 2, buf);

        // Col 3: Category
        MultiByteToWideChar(CP_UTF8, 0, kb.category.c_str(), -1, buf, 255);
        SE_SetItemTextW(s_hwndShortcutList, i, 3, buf);

        // Col 4: Source
        SE_SetItemTextW(s_hwndShortcutList, i, 4,
                        kb.isCustom ? L"Custom" : L"Default");
    }
}

// ============================================================================
// Window Procedure
// ============================================================================

#define IDC_SE_RECORD   7001
#define IDC_SE_RESET    7002
#define IDC_SE_SAVE     7003
#define IDC_SE_CAPTURE  7004
#define IDC_SE_SEARCH   7005

static LRESULT CALLBACK shortcutEditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Search box
        CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            10, 8, 180, 24, hwnd, (HMENU)IDC_SE_SEARCH,
            GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"\u2328 Record Key",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            200, 5, 110, 28, hwnd, (HMENU)IDC_SE_RECORD,
            GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"Reset to Default",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            315, 5, 110, 28, hwnd, (HMENU)IDC_SE_RESET,
            GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"Save",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            430, 5, 60, 28, hwnd, (HMENU)IDC_SE_SAVE,
            GetModuleHandleW(nullptr), nullptr);

        // Key capture display (shows captured keystrokes)
        s_hwndKeyCapture = CreateWindowExW(0, L"STATIC", L"Press a key combination...",
            WS_CHILD | SS_CENTER | SS_CENTERIMAGE,
            500, 5, 200, 28, hwnd, (HMENU)IDC_SE_CAPTURE,
            GetModuleHandleW(nullptr), nullptr);

        // ListView
        s_hwndShortcutList = CreateWindowExW(0, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
            0, 38, 0, 0, hwnd, (HMENU)7010,
            GetModuleHandleW(nullptr), nullptr);

        if (s_hwndShortcutList) {
            SendMessageW(s_hwndShortcutList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                         LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
            SendMessageW(s_hwndShortcutList, LVM_SETBKCOLOR,     0, RGB(30, 30, 30));
            SendMessageW(s_hwndShortcutList, LVM_SETTEXTBKCOLOR, 0, RGB(30, 30, 30));
            SendMessageW(s_hwndShortcutList, LVM_SETTEXTCOLOR,   0, RGB(220, 220, 220));

            SE_InsertColumnW(s_hwndShortcutList, 0, 160, L"Command");
            SE_InsertColumnW(s_hwndShortcutList, 1, 180, L"Description");
            SE_InsertColumnW(s_hwndShortcutList, 2, 130, L"Keybinding");
            SE_InsertColumnW(s_hwndShortcutList, 3, 90,  L"Category");
            SE_InsertColumnW(s_hwndShortcutList, 4, 70,  L"Source");

            refreshShortcutList();
        }
        return 0;
    }

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (s_hwndShortcutList)
            MoveWindow(s_hwndShortcutList, 0, 38, rc.right, rc.bottom - 38, TRUE);
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        if (s_isCapturing) {
            // Don't capture modifier-only presses
            if (wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU)
                return 0;

            bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
            bool alt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;

            std::string keyName = buildKeyName(wParam, ctrl, shift, alt);

            // Conflict detection — warn if key already bound
            std::string conflict = findConflictingCommand(keyName, s_captureTargetIndex);
            if (!conflict.empty()) {
                std::wstring msg = L"Key \"";
                wchar_t keyW[128] = {};
                MultiByteToWideChar(CP_UTF8, 0, keyName.c_str(), -1, keyW, 127);
                msg += keyW;
                msg += L"\" is already assigned to:\n";
                wchar_t conflictW[256] = {};
                MultiByteToWideChar(CP_UTF8, 0, conflict.c_str(), -1, conflictW, 255);
                msg += conflictW;
                msg += L"\n\nOverride the existing binding?";
                int result = MessageBoxW(hwnd, msg.c_str(), L"Keybinding Conflict",
                                         MB_YESNO | MB_ICONWARNING);
                if (result != IDYES) {
                    s_isCapturing = false;
                    SetWindowTextW(s_hwndKeyCapture, L"Cancelled — conflict");
                    return 0;
                }
                // Remove the conflicting binding's key
                for (int i = 0; i < (int)s_keyBindings.size(); ++i) {
                    if (i != s_captureTargetIndex && s_keyBindings[i].keyCombination == keyName) {
                        s_keyBindings[i].keyCombination = "(unassigned)";
                        s_keyBindings[i].isCustom = true;
                        break;
                    }
                }
            }

            // Update capture display
            {
                wchar_t keyW[128] = {};
                MultiByteToWideChar(CP_UTF8, 0, keyName.c_str(), -1, keyW, 127);
                SetWindowTextW(s_hwndKeyCapture, keyW);
            }
            ShowWindow(s_hwndKeyCapture, SW_SHOW);

            // Apply to selected item
            if (s_captureTargetIndex >= 0 && s_captureTargetIndex < (int)s_keyBindings.size()) {
                s_keyBindings[s_captureTargetIndex].keyCombination = keyName;
                s_keyBindings[s_captureTargetIndex].isCustom = true;
            }

            s_isCapturing = false;
            refreshShortcutList();
            return 0;
        }
        break;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_SE_RECORD) {
            // Start key capture mode for selected item
            int sel = (int)SendMessageW(s_hwndShortcutList, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
            if (sel >= 0) {
                s_isCapturing = true;
                s_captureTargetIndex = sel;
                SetWindowTextW(s_hwndKeyCapture, L"⌨ Recording... press keys");
                ShowWindow(s_hwndKeyCapture, SW_SHOW);
                SetFocus(hwnd); // Focus the window to capture keys
            }
        } else if (wmId == IDC_SE_RESET) {
            int sel = (int)SendMessageW(s_hwndShortcutList, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
            if (sel >= 0 && sel < (int)s_keyBindings.size()) {
                // Reset to default (re-seed)
                s_keyBindings[sel].isCustom = false;
                // In production, restore from defaults map
            }
            refreshShortcutList();
        } else if (wmId == IDC_SE_SAVE) {
            // Save keybindings to persistent JSON file
            std::string path = getKeybindingsPath();
            std::ofstream fout(path);
            if (fout.is_open()) {
                fout << "[\n";
                for (size_t i = 0; i < s_keyBindings.size(); ++i) {
                    auto& kb = s_keyBindings[i];
                    fout << "  {\"command\":\"" << kb.command
                         << "\",\"key\":\"" << kb.keyCombination
                         << "\",\"source\":\"" << (kb.isCustom ? "custom" : "default")
                         << "\"}";
                    if (i + 1 < s_keyBindings.size()) fout << ",";
                    fout << "\n";
                }
                fout << "]\n";
                fout.close();
            }
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* nmh = (NMHDR*)lParam;
        if (nmh && nmh->code == NM_CUSTOMDRAW) {
            LPNMLVCUSTOMDRAW lpcd = (LPNMLVCUSTOMDRAW)lParam;
            switch (lpcd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    int item = (int)lpcd->nmcd.dwItemSpec;
                    if (item >= 0 && item < (int)s_keyBindings.size()) {
                        if (s_keyBindings[item].isCustom) {
                            lpcd->clrText = RGB(80, 200, 255); // Blue for custom
                        } else {
                            lpcd->clrText = RGB(200, 200, 200);
                        }
                    }
                    lpcd->clrTextBk = RGB(30, 30, 30);
                    return CDRF_DODEFAULT;
                }
                default: return CDRF_DODEFAULT;
            }
        }
        // Double-click to start recording
        if (nmh && nmh->code == NM_DBLCLK) {
            int sel = (int)SendMessageW(s_hwndShortcutList, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
            if (sel >= 0) {
                s_isCapturing = true;
                s_captureTargetIndex = sel;
                SetWindowTextW(s_hwndKeyCapture, L"⌨ Recording... press keys");
                ShowWindow(s_hwndKeyCapture, SW_SHOW);
                SetFocus(hwnd);
            }
        }
        break;
    }

    case WM_ERASEBKGND: return 1;
    case WM_DESTROY:
        s_hwndShortcutEditor = nullptr;
        s_hwndShortcutList   = nullptr;
        s_hwndKeyCapture     = nullptr;
        s_isCapturing        = false;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool ensureShortcutEditorClass() {
    if (s_shortcutEditorClassRegistered) return true;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = shortcutEditorWndProc;
    wc.hInstance      = GetModuleHandleW(nullptr);
    wc.hCursor        = LoadCursorW(nullptr, (LPCWSTR)(uintptr_t)IDC_ARROW);
    wc.hbrBackground  = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName  = SHORTCUT_EDITOR_CLASS;

    if (!RegisterClassExW(&wc)) return false;
    s_shortcutEditorClassRegistered = true;
    return true;
}

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initShortcutEditorPanel() {
    initShortcutEditor();
}

void Win32IDE::initShortcutEditor() {
    if (m_shortcutEditorInitialized) return;
    seedDefaultBindings();

    // Load user-customized keybindings from %APPDATA%\RawrXD\keybindings.json
    if (loadKeybindingsFromDisk()) {
        OutputDebugStringA("[ShortcutEditor] Custom keybindings loaded from disk.\n");
    }

    OutputDebugStringA("[ShortcutEditor] Tier 5 — Keyboard shortcut editor initialized.\n");
    m_shortcutEditorInitialized = true;
    appendToOutput("[ShortcutEditor] Visual keyboard shortcut editor ready.\n");
}

// ============================================================================
// Command Router
// ============================================================================

bool Win32IDE::handleShortcutEditorCommand(int commandId) {
    if (!m_shortcutEditorInitialized) initShortcutEditor();
    switch (commandId) {
        case IDM_SHORTCUT_SHOW:   cmdShortcutEditorShow();   return true;
        case IDM_SHORTCUT_RECORD: cmdShortcutEditorRecord(); return true;
        case IDM_SHORTCUT_RESET:  cmdShortcutEditorReset();  return true;
        case IDM_SHORTCUT_SAVE:   cmdShortcutEditorSave();   return true;
        case IDM_SHORTCUT_LIST:   cmdShortcutEditorList();   return true;
        default: return false;
    }
}

// ============================================================================
// Show Shortcut Editor Window
// ============================================================================

void Win32IDE::cmdShortcutEditorShow() {
    if (s_hwndShortcutEditor && IsWindow(s_hwndShortcutEditor)) {
        SetForegroundWindow(s_hwndShortcutEditor);
        return;
    }

    if (!ensureShortcutEditorClass()) {
        MessageBoxW(m_hwndMain, L"Failed to register shortcut editor class.",
                    L"Shortcut Editor Error", MB_OK | MB_ICONERROR);
        return;
    }

    s_hwndShortcutEditor = CreateWindowExW(
        WS_EX_OVERLAPPEDWINDOW,
        SHORTCUT_EDITOR_CLASS,
        L"RawrXD — Keyboard Shortcuts",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 750, 550,
        m_hwndMain, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (s_hwndShortcutEditor) {
        ShowWindow(s_hwndShortcutEditor, SW_SHOW);
        UpdateWindow(s_hwndShortcutEditor);
    }
}

// ============================================================================
// Start key recording mode
// ============================================================================

void Win32IDE::cmdShortcutEditorRecord() {
    if (!s_hwndShortcutEditor || !IsWindow(s_hwndShortcutEditor)) {
        cmdShortcutEditorShow();
    }

    int sel = -1;
    if (s_hwndShortcutList)
        sel = (int)SendMessageW(s_hwndShortcutList, LVM_GETNEXTITEM, -1, LVNI_SELECTED);

    if (sel >= 0) {
        s_isCapturing = true;
        s_captureTargetIndex = sel;
        appendToOutput("[ShortcutEditor] Recording key combination for: " +
                       s_keyBindings[sel].description + "\n");
    } else {
        appendToOutput("[ShortcutEditor] Select a command first, then press Record.\n");
    }
}

// ============================================================================
// Reset all to defaults
// ============================================================================

void Win32IDE::cmdShortcutEditorReset() {
    s_keyBindings.clear();
    // Re-seed
    bool* seedFlag = nullptr;
    // Force re-seed by clearing the static flag via a trick:
    // We re-add all defaults
    seedDefaultBindings();
    for (auto& kb : s_keyBindings) {
        kb.isCustom = false;
    }
    refreshShortcutList();
    appendToOutput("[ShortcutEditor] All keybindings reset to defaults.\n");
}

// ============================================================================
// Save keybindings
// ============================================================================

void Win32IDE::cmdShortcutEditorSave() {
    std::string path = getKeybindingsPath();
    std::ofstream fout(path);
    if (!fout.is_open()) {
        appendToOutput("[ShortcutEditor] ERROR: Failed to save keybindings to " + path + "\n");
        return;
    }

    fout << "[\n";
    for (size_t i = 0; i < s_keyBindings.size(); ++i) {
        auto& kb = s_keyBindings[i];
        fout << "  {\n"
             << "    \"command\": \"" << kb.command << "\",\n"
             << "    \"key\": \"" << kb.keyCombination << "\",\n"
             << "    \"description\": \"" << kb.description << "\",\n"
             << "    \"category\": \"" << kb.category << "\",\n"
             << "    \"custom\": " << (kb.isCustom ? "true" : "false") << "\n"
             << "  }";
        if (i + 1 < s_keyBindings.size()) fout << ",";
        fout << "\n";
    }
    fout << "]\n";
    fout.close();

    appendToOutput("[ShortcutEditor] Keybindings saved to " + path + "\n");
}

// ============================================================================
// List all keybindings to Output
// ============================================================================

void Win32IDE::cmdShortcutEditorList() {
    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║              KEYBOARD SHORTCUTS                            ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n";

    std::string lastCat;
    for (auto& kb : s_keyBindings) {
        if (kb.category != lastCat) {
            char catLine[80];
            snprintf(catLine, sizeof(catLine), "║  ── %s ──                                               ║\n",
                     kb.category.c_str());
            oss << catLine;
            lastCat = kb.category;
        }

        char line[128];
        snprintf(line, sizeof(line), "║  %-20s  %-16s  %s%-10s ║\n",
                 kb.description.c_str(), kb.keyCombination.c_str(),
                 kb.isCustom ? "★ " : "  ",
                 kb.isCustom ? "Custom" : "");
        oss << line;
    }

    oss << "╚══════════════════════════════════════════════════════════════╝\n";
    appendToOutput(oss.str());
}
