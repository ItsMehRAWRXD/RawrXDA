#include "Agent.h"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <fstream>

// Why: Add agent support for automating self-contained builds and interactive Ask mode.
// What: Add AGENT_AUTOMATE_SELF_CONTAINED_BUILD command and AgentMode to AgentContext.
// Outcome: Agent can automate build settings and prompt user for confirmation.

enum AgentMode {
    AGENT_MODE_CONTROL,
    AGENT_MODE_ASK
};

// ==================== External IDE State ====================
// These are defined in main.cpp and linked externally
struct EditorPane {
    HWND hwndEdit = nullptr;
    std::wstring filePath;
    bool dirty = false;
};
extern std::vector<EditorPane> gEditors;
extern int gCurrentTab;
extern HWND gTab;
extern HWND gStatusBar;
extern HWND gTerminalPane;
extern HANDLE gTermInWrite;

// Forward declarations from main.cpp
extern void SwitchTab(int idx);
extern int FindTabByPath(const std::wstring& path);
extern void OpenFileInTab(HWND hwndMain, const std::wstring& path);

std::vector<OpenFileInfo> gOpenFiles;

// ==================== IDE API Implementations ====================

void LoadFileIntoEdit(HWND hEdit, const std::wstring& path) {
    // Read file contents and load into the Scintilla/Edit control
    std::wifstream in(path);
    if (!in.is_open()) {
        if (hEdit) {
            SetWindowTextW(hEdit, (L"Could not open file: " + path).c_str());
        }
        return;
    }
    std::wstring content, line;
    while (std::getline(in, line)) {
        content += line + L"\r\n";
    }
    in.close();

    if (hEdit) {
        // Try Scintilla SCI_SETTEXT first, fall back to WM_SETTEXT
        LRESULT result = SendMessageW(hEdit, 2181 /*SCI_SETTEXT*/, 0, (LPARAM)content.c_str());
        if (result == 0) {
            // Scintilla handled it
        } else {
            // Fall back to standard edit control
            SetWindowTextW(hEdit, content.c_str());
        }
    }
}

void AppendEditorText(HWND hwnd, const std::wstring& text) {
    if (!hwnd) return;

    // For Scintilla: append to end of document
    // Get document length via SCI_GETLENGTH (2006)
    LRESULT docLen = SendMessageW(hwnd, 2006 /*SCI_GETLENGTH*/, 0, 0);
    if (docLen >= 0) {
        // SCI_APPENDTEXT (2282) — appends text at end
        // Convert wide string to UTF-8 for Scintilla
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
        std::string utf8(utf8Len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], utf8Len, NULL, NULL);
        SendMessageA(hwnd, 2282 /*SCI_APPENDTEXT*/, utf8.size(), (LPARAM)utf8.c_str());
        return;
    }

    // Fallback for standard Edit control: EM_REPLACESEL at end
    int len = GetWindowTextLengthW(hwnd);
    SendMessageW(hwnd, EM_SETSEL, len, len);
    SendMessageW(hwnd, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

std::wstring GetEditorText(HWND hwnd) {
    if (!hwnd) return L"";

    // Try Scintilla SCI_GETLENGTH (2006) + SCI_GETTEXT (2182)
    LRESULT docLen = SendMessageW(hwnd, 2006 /*SCI_GETLENGTH*/, 0, 0);
    if (docLen > 0) {
        std::string utf8(docLen + 1, '\0');
        SendMessageA(hwnd, 2182 /*SCI_GETTEXT*/, docLen + 1, (LPARAM)utf8.data());
        // Convert UTF-8 to wide string
        int wLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)docLen, NULL, 0);
        std::wstring result(wLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)docLen, &result[0], wLen);
        return result;
    }

    // Fallback for standard Edit control
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return L"";
    std::wstring buf(len + 1, L'\0');
    GetWindowTextW(hwnd, &buf[0], len + 1);
    buf.resize(len);
    return buf;
}

void SetEditorText(HWND hwnd, const std::wstring& text) {
    if (!hwnd) return;

    // Try Scintilla SCI_SETTEXT (2181)
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], utf8Len, NULL, NULL);
    SendMessageA(hwnd, 2181 /*SCI_SETTEXT*/, 0, (LPARAM)utf8.c_str());

    // Also try WM_SETTEXT for standard Edit controls
    SetWindowTextW(hwnd, text.c_str());
}

void WriteToTerminal(const std::wstring& text) {
    // Write to the embedded terminal via pipe
    if (gTermInWrite) {
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
        std::string utf8(utf8Len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], utf8Len, NULL, NULL);
        DWORD written;
        WriteFile(gTermInWrite, utf8.c_str(), (DWORD)utf8.size(), &written, NULL);
        return;
    }

    // Fallback: append to the terminal pane directly
    if (gTerminalPane) {
        int len = GetWindowTextLengthW(gTerminalPane);
        SendMessageW(gTerminalPane, EM_SETSEL, len, len);
        SendMessageW(gTerminalPane, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    }
}

void SaveFile(HWND hwnd, const std::wstring& path) {
    if (!hwnd) return;

    // Get text from editor
    std::wstring content = GetEditorText(hwnd);

    // Write to file
    std::wofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        MessageBoxW(hwnd, (L"Failed to save file: " + path).c_str(), L"Save Error", MB_OK | MB_ICONERROR);
        return;
    }
    out << content;
    out.close();

    // Update gOpenFiles tracking
    bool found = false;
    for (auto& f : gOpenFiles) {
        if (f.path == path) { found = true; break; }
    }
    if (!found) {
        gOpenFiles.push_back({path});
    }

    // Mark editor as clean (update dirty flag in gEditors)
    for (size_t i = 0; i < gEditors.size(); ++i) {
        if (gEditors[i].hwndEdit == hwnd || gEditors[i].filePath == path) {
            gEditors[i].dirty = false;
            break;
        }
    }
}

void ChangeLayout(const std::wstring& layoutName) {
    // Load the named layout from the solution config
    // Layout files are stored as: MySolution.depgraph.<layoutName>.json
    std::wstring layoutPath = L"MySolution.depgraph." + layoutName + L".json";

    // Check if the layout file exists
    DWORD attrs = GetFileAttributesW(layoutPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        // Layout not found — notify user
        MessageBoxW(NULL,
            (L"Layout '" + layoutName + L"' not found.\nExpected file: " + layoutPath).c_str(),
            L"Layout Change", MB_OK | MB_ICONWARNING);
        return;
    }

    // Read the layout JSON (simple key-value parsing for window positions)
    std::wifstream in(layoutPath);
    if (!in.is_open()) return;

    std::wstring content, line;
    while (std::getline(in, line)) content += line;
    in.close();

    // Apply layout name to status bar
    if (gStatusBar) {
        SendMessageW(gStatusBar, SB_SETTEXT, 1, (LPARAM)(L"Layout: " + layoutName).c_str());
    }
}

void ShowDialog(HWND hwnd, int dialogId) {
    if (!hwnd) return;

    // Map dialog IDs to resource dialog templates
    // These are defined in Resource.h
    switch (dialogId) {
        case 201: // IDD_LAYOUT_MANAGER
            // Show the Layout Manager modeless dialog
            {
                extern void ShowLayoutManager(HWND parent);
                ShowLayoutManager(hwnd);
            }
            break;
        case 301: // IDD_GOTO
            DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(dialogId), hwnd, NULL);
            break;
        case 304: // IDD_FIND
            DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(dialogId), hwnd, NULL);
            break;
        default:
            // Generic dialog display
            DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(dialogId), hwnd, NULL);
            break;
    }
}

void SelectTab(HWND hwnd, int tabIndex) {
    if (!gTab) return;

    int tabCount = TabCtrl_GetItemCount(gTab);
    if (tabIndex < 0 || tabIndex >= tabCount) {
        return;
    }

    // Set the tab control selection
    TabCtrl_SetCurSel(gTab, tabIndex);

    // Switch the editor pane visibility
    for (int i = 0; i < (int)gEditors.size(); ++i) {
        ShowWindow(gEditors[i].hwndEdit, (i == tabIndex) ? SW_SHOW : SW_HIDE);
    }
    gCurrentTab = tabIndex;

    // Update status bar with active file
    if (tabIndex < (int)gEditors.size() && gStatusBar) {
        SendMessageW(gStatusBar, SB_SETTEXT, 0,
            (LPARAM)(L"Active file: " + gEditors[tabIndex].filePath).c_str());
    }
}

std::wstring GetCurrentLayout() {
    // Read the current layout name from the solution config
    std::wifstream in(L"Solution.solconfig.ini");
    if (!in.is_open()) return L"Default";

    std::wstring line;
    while (std::getline(in, line)) {
        // Parse "Layout=<name>" entries
        if (line.find(L"Layout=") == 0) {
            return line.substr(7);
        }
        if (line.find(L"layout=") == 0) {
            return line.substr(7);
        }
    }
    return L"Default";
}

void SetCurrentLayout(const std::wstring& layoutName) {
    // Apply the layout change
    ChangeLayout(layoutName);

    // Persist to solution config
    // Read existing config, update Layout line, write back
    std::vector<std::wstring> lines;
    bool found = false;
    {
        std::wifstream in(L"Solution.solconfig.ini");
        if (in.is_open()) {
            std::wstring line;
            while (std::getline(in, line)) {
                if (line.find(L"Layout=") == 0 || line.find(L"layout=") == 0) {
                    lines.push_back(L"Layout=" + layoutName);
                    found = true;
                } else {
                    lines.push_back(line);
                }
            }
        }
    }
    if (!found) {
        lines.push_back(L"Layout=" + layoutName);
    }
    {
        std::wofstream out(L"Solution.solconfig.ini", std::ios::trunc);
        for (const auto& l : lines) {
            out << l << L"\n";
        }
    }
}

void AutomateSelfContainedBuild(HWND hwnd) {
    // Why: Automate static linking for self-contained EXE builds.
    // What: Would update project settings to use /MT (stub here).
    // Outcome: Future builds will produce portable executables.
    MessageBox(hwnd, L"Automated self-contained build: Runtime Library set to /MT (stub)", L"Agent Build Automation", MB_OK);
}

void AgentHandleCommand(AgentContext* ctx, AgentCommand cmd, const std::wstring& payload) {
    if (ctx->mode == AGENT_MODE_ASK) {
        int res = MessageBox(ctx->hwnd, L"Agent asks: Perform this action?", L"Agent Ask", MB_YESNO);
        if (res != IDYES) return;
    }
    switch (cmd) {
        case AGENT_OPEN_FILE:
            LoadFileIntoEdit(GetDlgItem(ctx->hwnd, 0 /*IDC_MAIN_EDIT*/), payload);
            break;
        case AGENT_INSERT_TEXT:
            AppendEditorText(ctx->hwnd, payload);
            break;
        case AGENT_GET_TEXT: {
            std::wstring txt = GetEditorText(ctx->hwnd);
            MessageBox(ctx->hwnd, txt.c_str(), L"Agent ? Editor Text", MB_OK);
            break;
        }
        case AGENT_RUN_CMD:
            WriteToTerminal(payload + L"\n");
            break;
        case AGENT_LIST_TABS: {
            std::wstring files;
            for (auto& f : gOpenFiles) files += f.path + L"\n";
            MessageBox(ctx->hwnd, files.c_str(), L"Agent ? Open Tabs", MB_OK);
            break;
        }
        case AGENT_AUTOMATE_SELF_CONTAINED_BUILD:
            AutomateSelfContainedBuild(ctx->hwnd);
            break;
        // ...repeat for other commands as needed...
    }
}

void RunAgent(AgentContext* ctx) {
    // Example: open a file, inject code, run a command, automate build
    AgentHandleCommand(ctx, AGENT_OPEN_FILE, L"C:\\temp\\demo.cpp");
    AgentHandleCommand(ctx, AGENT_INSERT_TEXT, L"\n// Added by Agent\n");
    AgentHandleCommand(ctx, AGENT_RUN_CMD, L"dir");
    AgentHandleCommand(ctx, AGENT_AUTOMATE_SELF_CONTAINED_BUILD, L"");
}
