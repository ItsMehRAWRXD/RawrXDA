// Win32UIIntegration.hpp - Native Win32 Chat UI, Diff Viewer, Status Updates
// Pure C++20 / Win32 - Zero Qt Dependencies
#pragma once

#include "agent_kernel_main.hpp"
#include "AgentOrchestrator.hpp"
#include <fstream>
#include <functional>
#include <filesystem>
#include <commctrl.h>
#include <richedit.h>
#include <windowsx.h>
#include <shlobj.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
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

constexpr UINT WM_AGENT_EVENT = WM_APP + 0x100;

// Menu IDs
constexpr int IDM_FILE_OPEN = 1201;
constexpr int IDM_FILE_OPEN_FOLDER = 1202;
constexpr int IDM_FILE_EXIT = 1203;
constexpr int IDM_VIEW_TERMINAL = 1211;
constexpr int IDM_VIEW_FILEBROWSER = 1212;
constexpr int IDM_AI_AUTO_APPROVE = 1221;
constexpr int IDM_AI_CLEAR = 1222;
constexpr int IDM_AI_REFRESH_MODELS = 1223;
constexpr int IDM_SETTINGS_OLLAMA = 1231;

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
        Vector<String> parts;
        HTREEITEM hCur = hItem;
        while (hCur) {
            TVITEMW tvi = {0};
            tvi.hItem = hCur;
            tvi.mask = TVIF_TEXT;
            wchar_t text[MAX_PATH];
            tvi.pszText = text;
            tvi.cchTextMax = MAX_PATH;
            SendMessageW(m_hTree, TVM_GETITEMW, 0, (LPARAM)&tvi);
            parts.push_back(String(text));
            hCur = (HTREEITEM)SendMessageW(m_hTree, TVM_GETNEXTITEM, TVGN_PARENT, (LPARAM)hCur);
        }

        String fullPath;
        for (int i = (int)parts.size() - 1; i >= 0; i--) {
            fullPath += parts[i];
            if (i > 0 && (parts[i].empty() || parts[i].back() != L'\\')) fullPath += L"\\";
        }
        wcscpy(buffer, fullPath.c_str());
    }

    HWND getHWND() const { return m_hTree; }

private:
    HWND m_parent = nullptr;
    HWND m_hTree = nullptr;
};

// Editor Tabs — C++20 String, no Qt
struct EditorInstance {
    HWND hEdit = nullptr;
    String filePath;
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

    void openFile(const String& path) {
        // Check if already open
        for (size_t i = 0; i < m_editors.size(); i++) {
            if (m_editors[i].filePath == path) {
                SendMessageW(m_hTab, TCM_SETCURSEL, i, 0);
                showEditor(i);
                return;
            }
        }

        // Read file (C++20 / std::filesystem, no Qt)
        std::ifstream f(std::filesystem::path(path));
        if (!f.is_open()) return;
        std::string contentNarrow((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();
        String content = StringUtils::FromUtf8(contentNarrow);

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
        size_t lastSlash = path.find_last_of(L"\\/");
        String fileName = (lastSlash != String::npos) ? path.substr(lastSlash + 1) : path;
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

        // Start PowerShell process with piped I/O
        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
        HANDLE hStdoutWrite = nullptr, hStdinRead = nullptr;
        HANDLE hStdoutRead = nullptr, hStdinWrite = nullptr;
        
        if (CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0) &&
            CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
            
            SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
            
            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si.hStdOutput = hStdoutWrite;
            si.hStdError = hStdoutWrite;
            si.hStdInput = hStdinRead;
            si.wShowWindow = SW_HIDE;
            
            PROCESS_INFORMATION pi = {};
            wchar_t cmd[] = L"powershell.exe -NoLogo -NoProfile";
            
            if (CreateProcessW(nullptr, cmd, nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                
                m_hProcess = pi.hProcess;
                m_hStdoutRead = hStdoutRead;
                m_hStdinWrite = hStdinWrite;
                m_hOutput = hOutput;
                m_hInput = hInput;
                
                CloseHandle(pi.hThread);
                CloseHandle(hStdoutWrite);
                CloseHandle(hStdinRead);
                
                // Start reader thread to pipe shell output to RichEdit
                struct ReaderCtx {
                    HANDLE hRead;
                    HWND hOutput;
                    HWND hParent;
                };
                ReaderCtx* ctx = new ReaderCtx{ m_hStdoutRead, hOutput, m_parent };
                
                m_hReaderThread = CreateThread(nullptr, 0,
                    [](LPVOID param) -> DWORD {
                        ReaderCtx* rc = static_cast<ReaderCtx*>(param);
                        char buf[4096];
                        DWORD bytesRead;
                        while (ReadFile(rc->hRead, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                            buf[bytesRead] = '\0';
                            // Convert to wide string
                            int wLen = MultiByteToWideChar(CP_UTF8, 0, buf, bytesRead, nullptr, 0);
                            if (wLen <= 0) wLen = MultiByteToWideChar(CP_ACP, 0, buf, bytesRead, nullptr, 0);
                            if (wLen > 0) {
                                wchar_t* wBuf = new wchar_t[wLen + 1];
                                MultiByteToWideChar(CP_UTF8, 0, buf, bytesRead, wBuf, wLen);
                                wBuf[wLen] = L'\0';
                                int len = GetWindowTextLengthW(rc->hOutput);
                                SendMessageW(rc->hOutput, EM_SETSEL, len, len);
                                SendMessageW(rc->hOutput, EM_REPLACESEL, FALSE, (LPARAM)wBuf);
                                SendMessageW(rc->hOutput, WM_VSCROLL, SB_BOTTOM, 0);
                                delete[] wBuf;
                            }
                        }
                        delete rc;
                        return 0;
                    }, ctx, 0, nullptr);

                SetWindowSubclass(hInput, [](HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                    UINT_PTR, DWORD_PTR dwRefData) -> LRESULT {
                    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
                        HANDLE hStdin = reinterpret_cast<HANDLE>(dwRefData);
                        int len = GetWindowTextLengthW(hwnd);
                        if (len > 0 && hStdin) {
                            wchar_t* buf = new wchar_t[len + 1];
                            GetWindowTextW(hwnd, buf, len + 1);
                            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
                            char* utf8 = new char[utf8Len + 2];
                            WideCharToMultiByte(CP_UTF8, 0, buf, -1, utf8, utf8Len, nullptr, nullptr);
                            size_t sLen = strlen(utf8);
                            utf8[sLen] = '\n';
                            utf8[sLen + 1] = '\0';
                            DWORD written;
                            WriteFile(hStdin, utf8, (DWORD)(sLen + 1), &written, nullptr);
                            delete[] utf8;
                            delete[] buf;
                            SetWindowTextW(hwnd, L"");
                        }
                        return 0;
                    }
                    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
                }, 1, reinterpret_cast<DWORD_PTR>(m_hStdinWrite));
            } else {
                CloseHandle(hStdoutRead);
                CloseHandle(hStdoutWrite);
                CloseHandle(hStdinRead);
                CloseHandle(hStdinWrite);
            }
        }
    }

    void resize(int x, int y, int width, int height) {
        MoveWindow(m_hTab, x, y, width, height, TRUE);
    }

    HWND getHWND() const { return m_hTab; }

private:
    HWND m_parent = nullptr;
    HWND m_hTab = nullptr;
    HWND m_hOutput = nullptr;
    HWND m_hInput = nullptr;
    HANDLE m_hProcess = nullptr;
    HANDLE m_hStdoutRead = nullptr;
    HANDLE m_hStdinWrite = nullptr;
    HANDLE m_hReaderThread = nullptr;
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

    void appendMessage(const String& sender, const String& message, COLORREF color) {
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
        String senderLine = sender + L": ";
        SendMessageW(m_chatDisplay, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(senderLine.c_str()));

        // Set color for message (not bold)
        cf.dwEffects = 0;
        cf.crTextColor = m_colors.foreground;
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        // Add message
        String msgLine = message + L"\n\n";
        SendMessageW(m_chatDisplay, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(msgLine.c_str()));

        // Scroll to bottom
        SendMessageW(m_chatDisplay, WM_VSCROLL, SB_BOTTOM, 0);
    }

    void appendUserMessage(const String& message) {
        appendMessage(L"You", message, m_colors.userMessage);
    }

    void appendAssistantMessage(const String& message) {
        appendMessage(L"Assistant", message, m_colors.assistantMessage);
    }

    void appendToolCall(const String& tool, const String& args) {
        int length = GetWindowTextLengthW(m_chatDisplay);
        SendMessageW(m_chatDisplay, EM_SETSEL, length, length);

        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_ITALIC;
        cf.crTextColor = m_colors.toolCall;
        cf.dwEffects = CFE_ITALIC;
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        String text = L"[Tool: " + tool + L"]\n" + args + L"\n\n";
        SendMessageW(m_chatDisplay, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
        SendMessageW(m_chatDisplay, WM_VSCROLL, SB_BOTTOM, 0);
    }

    void appendError(const String& error) {
        appendMessage(L"Error", error, m_colors.error);
    }

    void appendStreamChunk(const String& chunk) {
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

    String getInputText() const {
        int length = GetWindowTextLengthW(m_chatInput);
        if (length == 0) return String();

        std::wstring buffer(length + 1, L'\0');
        GetWindowTextW(m_chatInput, buffer.data(), length + 1);
        buffer.resize(length);
        return String(buffer);
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

    void setState(const String& state) {
        SendMessageW(m_statusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(state.c_str()));
    }

    void setModel(const String& model) {
        String text = L"Model: " + model;
        SendMessageW(m_statusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(text.c_str()));
    }

    void setMessage(const String& message) {
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

    void showDiff(const String& original, const String& modified, const String& filename) {
        SetWindowTextW(m_window, L"");

        appendLine(L"=== " + filename + L" ===", m_colors.foreground);
        appendLine(L"", m_colors.foreground);

        auto origLines = StringUtils::Split(original, L"\n");
        auto modLines = StringUtils::Split(modified, L"\n");

        // Simple line-by-line diff
        size_t maxLines = (std::max)(origLines.size(), modLines.size());

        for (size_t i = 0; i < maxLines; ++i) {
            String origLine = i < origLines.size() ? origLines[i] : String();
            String modLine = i < modLines.size() ? modLines[i] : String();

            if (origLine == modLine) {
                appendLine(L"  " + origLine, m_colors.foreground);
            } else {
                if (!origLine.empty()) {
                    appendLine(L"- " + origLine, RGB(244, 71, 71));
                }
                if (!modLine.empty()) {
                    appendLine(L"+ " + modLine, RGB(78, 201, 176));
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
    void appendLine(const String& line, COLORREF color) {
        int length = GetWindowTextLengthW(m_window);
        SendMessageW(m_window, EM_SETSEL, length, length);

        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = color;
        SendMessageW(m_window, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        String text = line + L"\n";
        SendMessageW(m_window, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    }

    HWND m_window = nullptr;
    ColorScheme m_colors;
};

// Main Agent Window (Expanded IDE)
class AgentWindow {
public:
    AgentWindow() = default;

    bool create(HINSTANCE hInstance, const String& title, int width = 1400, int height = 900) {
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

        // Create menu bar
        HMENU hMenuBar = CreateMenu();
        HMENU hFile = CreatePopupMenu();
        AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN, L"&Open File...");
        AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN_FOLDER, L"Open &Folder (Working Dir)...");
        AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT, L"E&xit");
        AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFile, L"&File");

        HMENU hView = CreatePopupMenu();
        AppendMenuW(hView, MF_STRING, IDM_VIEW_TERMINAL, L"&Terminal");
        AppendMenuW(hView, MF_STRING, IDM_VIEW_FILEBROWSER, L"&File Browser");
        AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hView, L"&View");

        HMENU hAI = CreatePopupMenu();
        AppendMenuW(hAI, MF_STRING, IDM_AI_AUTO_APPROVE, L"&Auto-approve Tools");
        AppendMenuW(hAI, MF_STRING, IDM_AI_CLEAR, L"&Clear Chat");
        AppendMenuW(hAI, MF_STRING, IDM_AI_REFRESH_MODELS, L"Refresh &Models");
        AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hAI, L"&AI");

        HMENU hSettings = CreatePopupMenu();
        AppendMenuW(hSettings, MF_STRING, IDM_SETTINGS_OLLAMA, L"&Ollama Host...");
        AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hSettings, L"&Settings");

        SetMenu(m_window, hMenuBar);

        // Create UI components
        if (!m_statusBar.create(m_window)) return false;

        layout();

        m_statusBar.setState(L"Ready");
        m_statusBar.setModel(L"Not connected");

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
        int chatTopY = 0;
        int modelComboHeight = 26;
        int chatHeight = contentHeight - modelComboHeight;

        m_fileBrowser.create(m_window, 0, 0, leftWidth, contentHeight);
        m_editorTabs.create(m_window, leftWidth, 0, centerWidth, centerHeight);
        m_terminalPanel.create(m_window, leftWidth, centerHeight, centerWidth, bottomHeight);
        m_chatPanel.create(m_window, leftWidth + centerWidth, chatTopY + modelComboHeight, rightWidth, chatHeight);

        // Model selector combo
        if (!m_modelCombo) {
            m_modelCombo = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                leftWidth + centerWidth + 4, 2, rightWidth - 50, 200,
                m_window, (HMENU)ID_MODEL_COMBO, GetModuleHandleW(nullptr), nullptr);
            if (m_modelCombo && m_hFontUI) {
                SendMessageW(m_modelCombo, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
                SendMessageW(m_modelCombo, CB_ADDSTRING, 0, (LPARAM)L"(Select model)");
                SendMessageW(m_modelCombo, CB_SETCURSEL, 0, 0);
            }
        } else {
            MoveWindow(m_modelCombo, leftWidth + centerWidth + 4, 2, rightWidth - 50, 200, TRUE);
        }
    }

    void show() {
        ShowWindow(m_window, SW_SHOW);
        UpdateWindow(m_window);
    }

    void setAgentEventCallback(std::function<void(const AgentEvent&)> cb) {
        m_agentEventCallback = std::move(cb);
    }

    void setAgent(AgentOrchestrator* agent) {
        m_agent = agent;
        if (agent) {
            agent->setEventCallback([this](const AgentEvent& event) {
                if (m_agentEventCallback) {
                    AgentEvent* copy = new AgentEvent(event);
                    PostMessageW(m_window, WM_AGENT_EVENT, 0, (LPARAM)copy);
                } else {
                    handleAgentEvent(event);
                }
            });
            populateModels();
            if (agent->isLLMAvailable()) {
                m_statusBar.setModel(agent->config().model);
            } else {
                m_statusBar.setModel(L"Connect Ollama");
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
                int modelComboHeight = 26;
                int chatHeight = contentHeight - modelComboHeight;

                if (m_fileBrowser.getHWND()) MoveWindow(m_fileBrowser.getHWND(), 0, 0, leftWidth, contentHeight, TRUE);
                if (m_editorTabs.getHWND()) m_editorTabs.resize(leftWidth, 0, centerWidth, centerHeight);
                if (m_terminalPanel.getHWND()) m_terminalPanel.resize(leftWidth, centerHeight, centerWidth, bottomHeight);
                if (m_chatPanel.chatDisplay()) m_chatPanel.resize(leftWidth + centerWidth, modelComboHeight, rightWidth, chatHeight);
                if (m_modelCombo) MoveWindow(m_modelCombo, leftWidth + centerWidth + 4, 2, rightWidth - 50, 200, TRUE);
                
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
                } else if (id == IDM_FILE_OPEN) {
                    openFile();
                } else if (id == IDM_FILE_OPEN_FOLDER) {
                    openFolder();
                } else if (id == IDM_FILE_EXIT) {
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                } else if (id == IDM_VIEW_TERMINAL) {
                    toggleTerminal();
                } else if (id == IDM_VIEW_FILEBROWSER) {
                    toggleFileBrowser();
                } else if (id == IDM_AI_AUTO_APPROVE) {
                    toggleAutoApprove();
                } else if (id == IDM_AI_CLEAR) {
                    m_chatPanel.clear();
                    if (m_agent) m_agent->clearConversation();
                } else if (id == IDM_AI_REFRESH_MODELS) {
                    populateModels();
                } else if (id == IDM_SETTINGS_OLLAMA) {
                    showOllamaSettings();
                } else if (id == ID_MODEL_COMBO && HIWORD(wParam) == CBN_SELCHANGE) {
                    onModelSelected();
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
                        m_editorTabs.openFile(String(path));
                    }
                }
                return 0;
            }

            case WM_AGENT_EVENT: {
                AgentEvent* ev = reinterpret_cast<AgentEvent*>(lParam);
                if (ev) {
                    if (m_agentEventCallback) {
                        m_agentEventCallback(*ev);
                    } else {
                        handleAgentEvent(*ev);
                    }
                    delete ev;
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
        String text = m_chatPanel.getInputText();
        if (text.empty()) return;

        m_chatPanel.clearInput();
        m_chatPanel.appendUserMessage(text);

        if (m_agent) {
            m_agent->runAgentLoopAsync(text);
        }
    }

    void populateModels() {
        if (!m_modelCombo || !m_agent) return;
        SendMessageW(m_modelCombo, CB_RESETCONTENT, 0, 0);
        auto models = m_agent->listModels();
        for (const auto& m : models) {
            SendMessageW(m_modelCombo, CB_ADDSTRING, 0, (LPARAM)m.c_str());
        }
        if (models.empty()) {
            SendMessageW(m_modelCombo, CB_ADDSTRING, 0, (LPARAM)L"(No models - start Ollama)");
        }
        SendMessageW(m_modelCombo, CB_SETCURSEL, 0, 0);
        for (size_t i = 0; i < models.size(); i++) {
            if (models[i] == m_agent->config().model) {
                SendMessageW(m_modelCombo, CB_SETCURSEL, (WPARAM)i, 0);
                break;
            }
        }
    }

    void onModelSelected() {
        if (!m_modelCombo || !m_agent) return;
        int idx = (int)SendMessageW(m_modelCombo, CB_GETCURSEL, 0, 0);
        if (idx < 0) return;
        wchar_t buf[256];
        if (SendMessageW(m_modelCombo, CB_GETLBTEXT, idx, (LPARAM)buf) > 0) {
            String model(buf);
            if (!model.empty() && model != L"(No models - start Ollama)" && model != L"(Select model)") {
                m_agent->setModel(model);
                m_statusBar.setModel(model);
            }
        }
    }

    void openFile() {
        wchar_t path[MAX_PATH] = {};
        OPENFILENAMEW ofn = { sizeof(ofn) };
        ofn.hwndOwner = m_window;
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"All Files (*.*)\0*.*\0";
        ofn.Flags = OFN_FILEMUSTEXIST;
        if (GetOpenFileNameW(&ofn)) {
            m_editorTabs.openFile(String(path));
        }
    }

    void openFolder() {
        BROWSEINFOW bi = {};
        bi.hwndOwner = m_window;
        bi.lpszTitle = L"Select Working Directory";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (pidl) {
            wchar_t path[MAX_PATH];
            if (SHGetPathFromIDListW(pidl, path)) {
                if (m_agent) {
                    m_agent->setWorkingDirectory(String(path));
                    m_statusBar.setMessage(String(L"Working dir: ") + path);
                }
            }
            CoTaskMemFree(pidl);
        }
    }

    void toggleTerminal() {
        m_terminalVisible = !m_terminalVisible;
        if (m_terminalPanel.getHWND()) {
            ShowWindow(m_terminalPanel.getHWND(), m_terminalVisible ? SW_SHOW : SW_HIDE);
        }
    }

    void toggleFileBrowser() {
        m_fileBrowserVisible = !m_fileBrowserVisible;
        if (m_fileBrowser.getHWND()) {
            ShowWindow(m_fileBrowser.getHWND(), m_fileBrowserVisible ? SW_SHOW : SW_HIDE);
        }
    }

    void toggleAutoApprove() {
        if (m_agent) {
            m_agent->config().autoApproveTools = !m_agent->config().autoApproveTools;
            m_statusBar.setMessage(m_agent->config().autoApproveTools ? L"Auto-approve ON" : L"Auto-approve OFF");
        }
    }

    void showOllamaSettings() {
        if (!m_agent) return;
        const auto& c = m_agent->config();
        String msg = L"Ollama: " + c.ollamaHost + L":" + std::to_wstring(c.ollamaPort) + L"\nModel: " + c.model;
        MessageBoxW(m_window, msg.c_str(), L"Settings", MB_OK);
    }

    void handleAgentEvent(const AgentEvent& event) {
        String qmsg(event.message);
        switch (event.type) {
            case AgentEvent::Type::StateChanged: m_statusBar.setState(qmsg); break;
            case AgentEvent::Type::MessageReceived: m_chatPanel.appendAssistantMessage(qmsg); break;
            case AgentEvent::Type::ToolCalled: m_chatPanel.appendToolCall(qmsg, StringUtils::FromUtf8(JsonParser::Serialize(event.data, 2))); break;
            case AgentEvent::Type::Error: m_chatPanel.appendError(qmsg); break;
            case AgentEvent::Type::StreamChunk: m_chatPanel.appendStreamChunk(qmsg); break;
            case AgentEvent::Type::Completed: m_statusBar.setState(L"Ready"); break;
        }
    }

    HINSTANCE m_hInstance = nullptr;
    HWND m_window = nullptr;
    FileBrowser m_fileBrowser;
    EditorTabs m_editorTabs;
    TerminalPanel m_terminalPanel;
    ChatPanel m_chatPanel;
    StatusBar m_statusBar;
    HWND m_modelCombo = nullptr;
    AgentOrchestrator* m_agent = nullptr;
    HFONT m_hFontUI = nullptr;
    bool m_terminalVisible = true;
    bool m_fileBrowserVisible = true;
    std::function<void(const AgentEvent&)> m_agentEventCallback;
};

} // namespace UI
} // namespace RawrXD
