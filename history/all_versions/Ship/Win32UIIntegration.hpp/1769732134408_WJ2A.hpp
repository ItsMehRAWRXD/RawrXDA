// Win32UIIntegration.hpp - Native Win32 Chat UI, Diff Viewer, Status Updates
// Pure C++20 / Win32 - Zero Qt Dependencies
#pragma once

#include "agent_kernel_main.hpp"
#include "QtReplacements.hpp"
#include "AgentOrchestrator.hpp"
#include <commctrl.h>
#include <richedit.h>
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace RawrXD {
namespace UI {

// Color scheme
struct ColorScheme {
    COLORREF background = RGB(30, 30, 30);
    COLORREF foreground = RGB(212, 212, 212);
    COLORREF userMessage = RGB(86, 156, 214);
    COLORREF assistantMessage = RGB(78, 201, 176);
    COLORREF toolCall = RGB(220, 220, 170);
    COLORREF error = RGB(244, 71, 71);
    COLORREF success = RGB(78, 201, 176);
    COLORREF selection = RGB(51, 153, 255);
    COLORREF border = RGB(60, 60, 60);
    COLORREF inputBg = RGB(45, 45, 45);
};

// Window IDs
constexpr int ID_CHAT_DISPLAY = 1001;
constexpr int ID_CHAT_INPUT = 1002;
constexpr int ID_SEND_BUTTON = 1003;
constexpr int ID_STOP_BUTTON = 1004;
constexpr int ID_CLEAR_BUTTON = 1005;
constexpr int ID_STATUS_BAR = 1006;
constexpr int ID_MODEL_COMBO = 1007;

// New IDE Component IDs
constexpr int ID_EDITOR_TABS = 1100;
constexpr int ID_FILE_TREE = 1101;
constexpr int ID_TERMINAL_TABS = 1102;
constexpr int ID_MAIN_SPLITTER_V = 1103;
constexpr int ID_MAIN_SPLITTER_H = 1104;

// File Browser
class FileBrowser {
public:
    FileBrowser() = default;

    bool create(HWND parent, int x, int y, int width, int height) {
        m_parent = parent;
        m_hTree = CreateWindowExW(
            0, WC_TREEVIEWW, L"FileBrowser",
            WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT,
            x, y, width, height,
            parent, (HMENU)ID_FILE_TREE, GetModuleHandleW(nullptr), nullptr
        );

        if (!m_hTree) return false;

        // Set dark theme for treeview if possible
        SendMessageW(m_hTree, TVM_SETBKCOLOR, 0, (LPARAM)RGB(30, 30, 30));
        SendMessageW(m_hTree, TVM_SETTEXTCOLOR, 0, (LPARAM)RGB(212, 212, 212));
        SendMessageW(m_hTree, TVM_SETLINECOLOR, 0, (LPARAM)RGB(80, 80, 80));

        populateDrives();
        return true;
    }

    void populateDrives() {
        wchar_t drives[256];
        DWORD len = GetLogicalDriveStringsW(255, drives);
        wchar_t* p = drives;
        while (*p) {
            TVINSERTSTRUCTW tvis = {0};
            tvis.hParent = TVI_ROOT;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
            tvis.item.pszText = p;
            tvis.item.lParam = (LPARAM)1; // 1 = drive/folder
            HTREEITEM hItem = (HTREEITEM)SendMessageW(m_hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
            
            // Add a dummy child so it looks expandable
            tvis.hParent = hItem;
            tvis.item.pszText = (LPWSTR)L"Loading...";
            SendMessageW(m_hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);

            p += wcslen(p) + 1;
        }
    }

    void onExpand(NMTREEVIEWW* pnm) {
        if (pnm->action == TVE_EXPAND) {
            // Remove dummy
            HTREEITEM hChild = (HTREEITEM)SendMessageW(m_hTree, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)pnm->itemNew.hItem);
            while (hChild) {
                SendMessageW(m_hTree, TVM_DELETEITEM, 0, (LPARAM)hChild);
                hChild = (HTREEITEM)SendMessageW(m_hTree, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)pnm->itemNew.hItem);
            }

            // Get path
            wchar_t path[MAX_PATH];
            getItemPath(pnm->itemNew.hItem, path);
            
            // Enum directory
            WIN32_FIND_DATAW ffd;
            wchar_t searchPath[MAX_PATH];
            swprintf(searchPath, MAX_PATH, L"%s*", path);
            HANDLE hFind = FindFirstFileW(searchPath, &ffd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;

                    TVINSERTSTRUCTW tvis = {0};
                    tvis.hParent = pnm->itemNew.hItem;
                    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
                    tvis.item.pszText = ffd.cFileName;
                    tvis.item.lParam = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
                    HTREEITEM hNew = (HTREEITEM)SendMessageW(m_hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);

                    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        tvis.hParent = hNew;
                        tvis.item.pszText = (LPWSTR)L"Loading...";
                        SendMessageW(m_hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
                    }
                } while (FindNextFileW(hFind, &ffd));
                FindClose(hFind);
            }
        }
    }

    void getItemPath(HTREEITEM hItem, wchar_t* buffer) {
        Vector<QString> parts;
        HTREEITEM hCur = hItem;
        while (hCur) {
            TVITEMW tvi = {0};
            tvi.hItem = hCur;
            tvi.mask = TVIF_TEXT;
            wchar_t text[MAX_PATH];
            tvi.pszText = text;
            tvi.cchTextMax = MAX_PATH;
            SendMessageW(m_hTree, TVM_GETITEMW, 0, (LPARAM)&tvi);
            parts.push_back(QString(text));
            hCur = (HTREEITEM)SendMessageW(m_hTree, TVM_GETNEXTITEM, TVGN_PARENT, (LPARAM)hCur);
        }

        QString fullPath;
        for (int i = parts.size() - 1; i >= 0; i--) {
            fullPath += parts[i];
            if (i > 0 && !parts[i].endsWith(L"\\")) fullPath += L"\\";
        }
        wcscpy(buffer, fullPath.c_str());
    }

    HWND getHWND() const { return m_hTree; }

private:
    HWND m_parent = nullptr;
    HWND m_hTree = nullptr;
};

// Editor Tabs
struct EditorInstance {
    HWND hEdit = nullptr;
    QString filePath;
    bool modified = false;
};

class EditorTabs {
public:
    EditorTabs() = default;

    bool create(HWND parent, int x, int y, int width, int height) {
        m_parent = parent;
        m_hTab = CreateWindowExW(
            0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS | TCS_FOCUSNEVER,
            x, y, width, height,
            parent, (HMENU)ID_EDITOR_TABS, GetModuleHandleW(nullptr), nullptr
        );

        if (!m_hTab) return false;

        // Set Font
        HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SendMessageW(m_hTab, WM_SETFONT, (WPARAM)hFont, TRUE);

        return true;
    }

    void openFile(const QString& path) {
        // Check if already open
        for (size_t i = 0; i < m_editors.size(); i++) {
            if (m_editors[i].filePath == path) {
                SendMessageW(m_hTab, TCM_SETCURSEL, i, 0);
                showEditor(i);
                return;
            }
        }

        // Read file
        QFile f(path);
        if (!f.open(L"r")) return;
        QString content = f.readAll();
        f.close();

        // Create new editor
        RECT rc;
        GetClientRect(m_hTab, &rc);
        SendMessageW(m_hTab, TCM_ADJUSTRECT, FALSE, (LPARAM)&rc);

        HWND hEdit = CreateWindowExW(
            0, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            m_hTab, nullptr, GetModuleHandleW(nullptr), nullptr
        );

        if (!hEdit) return;

        // Dark theme for editor
        SendMessageW(hEdit, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
        cf.crTextColor = RGB(212, 212, 212);
        wcscpy_s(cf.szFaceName, L"Consolas");
        cf.yHeight = 200; 
        SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

        SetWindowTextW(hEdit, content.c_str());

        // Add tab
        TCITEMW tie = {0};
        tie.mask = TCIF_TEXT;
        QString fileName = path.mid(path.lastIndexOf(L'\\') + 1);
        tie.pszText = const_cast<LPWSTR>(fileName.c_str());
        int idx = (int)SendMessageW(m_hTab, TCM_INSERTITEMW, m_editors.size(), (LPARAM)&tie);

        m_editors.push_back({hEdit, path, false});
        SendMessageW(m_hTab, TCM_SETCURSEL, idx, 0);
        showEditor(idx);
    }

    void showEditor(int index) {
        for (int i = 0; i < (int)m_editors.size(); i++) {
            ShowWindow(m_editors[i].hEdit, i == index ? SW_SHOW : SW_HIDE);
        }
    }

    void resize(int x, int y, int width, int height) {
        MoveWindow(m_hTab, x, y, width, height, TRUE);
        RECT rc = {0, 0, width, height};
        SendMessageW(m_hTab, TCM_ADJUSTRECT, FALSE, (LPARAM)&rc);
        for (auto& ed : m_editors) {
            MoveWindow(ed.hEdit, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
        }
    }

    HWND getHWND() const { return m_hTab; }

private:
    HWND m_parent = nullptr;
    HWND m_hTab = nullptr;
    Vector<EditorInstance> m_editors;
};

// Terminal Panel
class TerminalPanel {
public:
    TerminalPanel() = default;

    bool create(HWND parent, int x, int y, int width, int height) {
        m_parent = parent;
        m_hTab = CreateWindowExW(
            0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | TCS_TABS,
            x, y, width, height,
            parent, (HMENU)ID_TERMINAL_TABS, GetModuleHandleW(nullptr), nullptr
        );

        if (!m_hTab) return false;

        // Create initial terminal
        newTerminal();
        return true;
    }

    void newTerminal() {
        RECT rc;
        GetClientRect(m_hTab, &rc);
        SendMessageW(m_hTab, TCM_ADJUSTRECT, FALSE, (LPARAM)&rc);

        HWND hOutput = CreateWindowExW(
            0, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top - 30,
            m_hTab, nullptr, GetModuleHandleW(nullptr), nullptr
        );

        HWND hInput = CreateWindowExW(
            0, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            rc.left, rc.top + (rc.bottom - rc.top) - 30, rc.right - rc.left, 30,
            m_hTab, nullptr, GetModuleHandleW(nullptr), nullptr
        );

        // Dark theme
        SendMessageW(hOutput, EM_SETBKGNDCOLOR, 0, RGB(0, 0, 0));
        SendMessageW(hInput, EM_SETBKGNDCOLOR, 0, RGB(20, 20, 20));

        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
        cf.crTextColor = RGB(0, 255, 0); // Matrix green
        wcscpy_s(cf.szFaceName, L"Consolas");
        cf.yHeight = 180;
        SendMessageW(hOutput, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));
        SendMessageW(hInput, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

        // Add tab
        TCITEMW tie = {0};
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)L"pwsh";
        SendMessageW(m_hTab, TCM_INSERTITEMW, 0, (LPARAM)&tie);

        // TODO: Start shell process and pipe I/O
    }

    void resize(int x, int y, int width, int height) {
        MoveWindow(m_hTab, x, y, width, height, TRUE);
    }

    HWND getHWND() const { return m_hTab; }

private:
    HWND m_parent = nullptr;
    HWND m_hTab = nullptr;
};

// Chat Panel
class ChatPanel {
public:
    ChatPanel() = default;

    bool create(HWND parent, int x, int y, int width, int height) {
        m_parent = parent;

        // Load RichEdit
        LoadLibraryW(L"Msftedit.dll");

        // Create chat display (RichEdit)
        m_chatDisplay = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            MSFTEDIT_CLASS,
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            x, y, width, height - 80,
            parent,
            reinterpret_cast<HMENU>(ID_CHAT_DISPLAY),
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (!m_chatDisplay) return false;

        // Set background color
        SendMessageW(m_chatDisplay, EM_SETBKGNDCOLOR, 0, m_colors.background);

        // Set default text format
        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
        cf.crTextColor = m_colors.foreground;
        wcscpy_s(cf.szFaceName, L"Consolas");
        cf.yHeight = 200; // 10pt
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

        // Create input box
        m_chatInput = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            MSFTEDIT_CLASS,
            L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            x, y + height - 75, width - 180, 70,
            parent,
            reinterpret_cast<HMENU>(ID_CHAT_INPUT),
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (!m_chatInput) return false;

        SendMessageW(m_chatInput, EM_SETBKGNDCOLOR, 0, m_colors.inputBg);
        SendMessageW(m_chatInput, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

        // Create buttons
        m_sendButton = CreateWindowW(
            L"BUTTON", L"Send",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x + width - 175, y + height - 75, 80, 32,
            parent,
            reinterpret_cast<HMENU>(ID_SEND_BUTTON),
            GetModuleHandleW(nullptr),
            nullptr
        );

        m_stopButton = CreateWindowW(
            L"BUTTON", L"Stop",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x + width - 90, y + height - 75, 80, 32,
            parent,
            reinterpret_cast<HMENU>(ID_STOP_BUTTON),
            GetModuleHandleW(nullptr),
            nullptr
        );

        m_clearButton = CreateWindowW(
            L"BUTTON", L"Clear",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x + width - 175, y + height - 38, 165, 32,
            parent,
            reinterpret_cast<HMENU>(ID_CLEAR_BUTTON),
            GetModuleHandleW(nullptr),
            nullptr
        );

        return true;
    }

    void appendMessage(const QString& sender, const QString& message, COLORREF color) {
        // Move to end
        int length = GetWindowTextLengthW(m_chatDisplay);
        SendMessageW(m_chatDisplay, EM_SETSEL, length, length);

        // Set color for sender
        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_BOLD;
        cf.crTextColor = color;
        cf.dwEffects = CFE_BOLD;
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        // Add sender
        QString senderLine = sender + QString(": ");
        SendMessageW(m_chatDisplay, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(senderLine.c_str()));

        // Set color for message (not bold)
        cf.dwEffects = 0;
        cf.crTextColor = m_colors.foreground;
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        // Add message
        QString msgLine = message + QString("\n\n");
        SendMessageW(m_chatDisplay, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(msgLine.c_str()));

        // Scroll to bottom
        SendMessageW(m_chatDisplay, WM_VSCROLL, SB_BOTTOM, 0);
    }

    void appendUserMessage(const QString& message) {
        appendMessage(QString("You"), message, m_colors.userMessage);
    }

    void appendAssistantMessage(const QString& message) {
        appendMessage(QString("Assistant"), message, m_colors.assistantMessage);
    }

    void appendToolCall(const QString& tool, const QString& args) {
        int length = GetWindowTextLengthW(m_chatDisplay);
        SendMessageW(m_chatDisplay, EM_SETSEL, length, length);

        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_ITALIC;
        cf.crTextColor = m_colors.toolCall;
        cf.dwEffects = CFE_ITALIC;
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        QString text = QString("[Tool: ") + tool + QString("]\n") + args + QString("\n\n");
        SendMessageW(m_chatDisplay, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
        SendMessageW(m_chatDisplay, WM_VSCROLL, SB_BOTTOM, 0);
    }

    void appendError(const QString& error) {
        appendMessage(QString("Error"), error, m_colors.error);
    }

    void appendStreamChunk(const QString& chunk) {
        int length = GetWindowTextLengthW(m_chatDisplay);
        SendMessageW(m_chatDisplay, EM_SETSEL, length, length);

        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = m_colors.foreground;
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        SendMessageW(m_chatDisplay, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(chunk.c_str()));
        SendMessageW(m_chatDisplay, WM_VSCROLL, SB_BOTTOM, 0);
    }

    void clear() {
        SetWindowTextW(m_chatDisplay, L"");
    }

    QString getInputText() const {
        int length = GetWindowTextLengthW(m_chatInput);
        if (length == 0) return QString();

        std::wstring buffer(length + 1, L'\0');
        GetWindowTextW(m_chatInput, buffer.data(), length + 1);
        buffer.resize(length);
        return QString(buffer);
    }

    void clearInput() {
        SetWindowTextW(m_chatInput, L"");
    }

    void setInputFocus() {
        SetFocus(m_chatInput);
    }

    void resize(int x, int y, int width, int height) {
        MoveWindow(m_chatDisplay, x, y, width, height - 80, TRUE);
        MoveWindow(m_chatInput, x, y + height - 75, width - 180, 70, TRUE);
        MoveWindow(m_sendButton, x + width - 175, y + height - 75, 80, 32, TRUE);
        MoveWindow(m_stopButton, x + width - 90, y + height - 75, 80, 32, TRUE);
        MoveWindow(m_clearButton, x + width - 175, y + height - 38, 165, 32, TRUE);
    }

    HWND chatDisplay() const { return m_chatDisplay; }
    HWND chatInput() const { return m_chatInput; }
    HWND sendButton() const { return m_sendButton; }
    HWND stopButton() const { return m_stopButton; }
    HWND clearButton() const { return m_clearButton; }

private:
    HWND m_parent = nullptr;
    HWND m_chatDisplay = nullptr;
    HWND m_chatInput = nullptr;
    HWND m_sendButton = nullptr;
    HWND m_stopButton = nullptr;
    HWND m_clearButton = nullptr;
    ColorScheme m_colors;
};

// Status Bar
class StatusBar {
public:
    bool create(HWND parent) {
        m_statusBar = CreateWindowExW(
            0,
            STATUSCLASSNAMEW,
            nullptr,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0,
            parent,
            reinterpret_cast<HMENU>(ID_STATUS_BAR),
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (!m_statusBar) return false;

        // Set parts
        int parts[] = { 200, 400, -1 };
        SendMessageW(m_statusBar, SB_SETPARTS, 3, reinterpret_cast<LPARAM>(parts));

        return true;
    }

    void setState(const QString& state) {
        SendMessageW(m_statusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(state.c_str()));
    }

    void setModel(const QString& model) {
        QString text = QString("Model: ") + model;
        SendMessageW(m_statusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(text.c_str()));
    }

    void setMessage(const QString& message) {
        SendMessageW(m_statusBar, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(message.c_str()));
    }

    void resize() {
        SendMessageW(m_statusBar, WM_SIZE, 0, 0);
    }

    int height() const {
        RECT rc;
        GetWindowRect(m_statusBar, &rc);
        return rc.bottom - rc.top;
    }

    HWND handle() const { return m_statusBar; }

private:
    HWND m_statusBar = nullptr;
};

// Diff Viewer (for showing file changes)
class DiffViewer {
public:
    bool create(HWND parent, int x, int y, int width, int height) {
        LoadLibraryW(L"Msftedit.dll");

        m_window = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            MSFTEDIT_CLASS,
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY,
            x, y, width, height,
            parent,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (!m_window) return false;

        SendMessageW(m_window, EM_SETBKGNDCOLOR, 0, m_colors.background);

        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
        cf.crTextColor = m_colors.foreground;
        wcscpy_s(cf.szFaceName, L"Consolas");
        cf.yHeight = 180;
        SendMessageW(m_window, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

        return true;
    }

    void showDiff(const QString& original, const QString& modified, const QString& filename) {
        SetWindowTextW(m_window, L"");

        appendLine(QString("=== ") + filename + QString(" ==="), m_colors.foreground);
        appendLine(QString(""), m_colors.foreground);

        auto origLines = original.split(QString("\n"));
        auto modLines = modified.split(QString("\n"));

        // Simple line-by-line diff
        size_t maxLines = (std::max)(origLines.size(), modLines.size());

        for (size_t i = 0; i < maxLines; ++i) {
            QString origLine = i < origLines.size() ? origLines[i] : QString();
            QString modLine = i < modLines.size() ? modLines[i] : QString();

            if (origLine == modLine) {
                appendLine(QString("  ") + origLine, m_colors.foreground);
            } else {
                if (!origLine.isEmpty()) {
                    appendLine(QString("- ") + origLine, RGB(244, 71, 71));
                }
                if (!modLine.isEmpty()) {
                    appendLine(QString("+ ") + modLine, RGB(78, 201, 176));
                }
            }
        }
    }

    void show() { ShowWindow(m_window, SW_SHOW); }
    void hide() { ShowWindow(m_window, SW_HIDE); }

    void resize(int x, int y, int width, int height) {
        MoveWindow(m_window, x, y, width, height, TRUE);
    }

    HWND handle() const { return m_window; }

private:
    void appendLine(const QString& line, COLORREF color) {
        int length = GetWindowTextLengthW(m_window);
        SendMessageW(m_window, EM_SETSEL, length, length);

        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = color;
        SendMessageW(m_window, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        QString text = line + QString("\n");
        SendMessageW(m_window, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    }

    HWND m_window = nullptr;
    ColorScheme m_colors;
};

// Main Agent Window (Expanded IDE)
class AgentWindow {
public:
    AgentWindow() = default;

    bool create(HINSTANCE hInstance, const QString& title, int width = 1400, int height = 900) {
        m_hInstance = hInstance;

        // Register window class
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
        wc.lpszClassName = L"RawrXDAgentWindow";
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

        if (!RegisterClassExW(&wc)) return false;

        // Create window
        m_window = CreateWindowExW(
            0,
            L"RawrXDAgentWindow",
            title.c_str(),
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            nullptr, nullptr, hInstance, this
        );

        if (!m_window) return false;

        // Initialize common controls
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES };
        InitCommonControlsEx(&icc);

        // Load fonts
        m_hFontUI = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        // Create UI components
        if (!m_statusBar.create(m_window)) return false;

        layout();

        m_statusBar.setState(QString("Ready"));
        m_statusBar.setModel(QString("Not connected"));

        return true;
    }

    void layout() {
        RECT rc;
        GetClientRect(m_window, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int statusHeight = m_statusBar.height();
        int contentHeight = h - statusHeight;

        int leftWidth = 280;
        int rightWidth = 350;
        int bottomHeight = 220;
        int centerWidth = w - leftWidth - rightWidth;
        int centerHeight = contentHeight - bottomHeight;

        m_fileBrowser.create(m_window, 0, 0, leftWidth, contentHeight);
        m_editorTabs.create(m_window, leftWidth, 0, centerWidth, centerHeight);
        m_terminalPanel.create(m_window, leftWidth, centerHeight, centerWidth, bottomHeight);
        m_chatPanel.create(m_window, leftWidth + centerWidth, 0, rightWidth, contentHeight);
    }

    void show() {
        ShowWindow(m_window, SW_SHOW);
        UpdateWindow(m_window);
    }

    void setAgent(AgentOrchestrator* agent) {
        m_agent = agent;
        if (agent) {
            agent->setEventCallback([this](const AgentEvent& event) {
                handleAgentEvent(event);
            });

            if (agent->isLLMAvailable()) {
                m_statusBar.setModel(agent->config().model);
            }
        }
    }

    int run() {
        MSG msg = {};
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

    HWND handle() const { return m_window; }
    ChatPanel& chatPanel() { return m_chatPanel; }
    StatusBar& statusBar() { return m_statusBar; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        AgentWindow* self = nullptr;

        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = static_cast<AgentWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<AgentWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self) {
            return self->handleMessage(hwnd, msg, wParam, lParam);
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_SIZE: {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;
                m_statusBar.resize();
                int statusHeight = m_statusBar.height();
                int contentHeight = h - statusHeight;

                int leftWidth = 280;
                int rightWidth = 350;
                int bottomHeight = 220;
                int centerWidth = w - leftWidth - rightWidth;
                int centerHeight = contentHeight - bottomHeight;

                if (m_fileBrowser.getHWND()) MoveWindow(m_fileBrowser.getHWND(), 0, 0, leftWidth, contentHeight, TRUE);
                if (m_editorTabs.getHWND()) m_editorTabs.resize(leftWidth, 0, centerWidth, centerHeight);
                if (m_terminalPanel.getHWND()) m_terminalPanel.resize(leftWidth, centerHeight, centerWidth, bottomHeight);
                if (m_chatPanel.chatDisplay()) m_chatPanel.resize(leftWidth + centerWidth, 0, rightWidth, contentHeight);
                
                return 0;
            }

            case WM_COMMAND: {
                int id = LOWORD(wParam);
                if (id == ID_SEND_BUTTON) {
                    sendMessage();
                } else if (id == ID_STOP_BUTTON) {
                    if (m_agent) m_agent->stop();
                } else if (id == ID_CLEAR_BUTTON) {
                    m_chatPanel.clear();
                    if (m_agent) m_agent->clearConversation();
                }
                return 0;
            }

            case WM_NOTIFY: {
                NMHDR* nm = (NMHDR*)lParam;
                if (nm->idFrom == ID_FILE_TREE) {
                    if (nm->code == TVN_ITEMEXPANDINGW) {
                        m_fileBrowser.onExpand((NMTREEVIEWW*)lParam);
                    } else if (nm->code == NM_DBLCLK) {
                        wchar_t path[MAX_PATH];
                        TVITEMW tvi = {0};
                        tvi.hItem = ((LPNMTREEVIEWW)lParam)->itemNew.hItem;
                        m_fileBrowser.getItemPath(tvi.hItem, path);
                        m_editorTabs.openFile(QString(path));
                    }
                }
                return 0;
            }

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void sendMessage() {
        QString text = m_chatPanel.getInputText();
        if (text.isEmpty()) return;

        m_chatPanel.clearInput();
        m_chatPanel.appendUserMessage(text);

        if (m_agent) {
            m_agent->runAgentLoopAsync(text);
        }
    }

    void handleAgentEvent(const AgentEvent& event) {
        // Direct update for now, ideally use PostMessage for thread safety
        switch (event.type) {
            case AgentEvent::Type::StateChanged: m_statusBar.setState(event.message); break;
            case AgentEvent::Type::MessageReceived: m_chatPanel.appendAssistantMessage(event.message); break;
            case AgentEvent::Type::ToolCalled: m_chatPanel.appendToolCall(event.message, QString(JsonParser::Serialize(event.data, 2))); break;
            case AgentEvent::Type::Error: m_chatPanel.appendError(event.message); break;
            case AgentEvent::Type::StreamChunk: m_chatPanel.appendStreamChunk(event.message); break;
            case AgentEvent::Type::Completed: m_statusBar.setState(QString("Ready")); break;
        }
    }

    HINSTANCE m_hInstance = nullptr;
    HWND m_window = nullptr;
    FileBrowser m_fileBrowser;
    EditorTabs m_editorTabs;
    TerminalPanel m_terminalPanel;
    ChatPanel m_chatPanel;
    StatusBar m_statusBar;
    AgentOrchestrator* m_agent = nullptr;
    HFONT m_hFontUI = nullptr;
};

} // namespace UI
} // namespace RawrXD
