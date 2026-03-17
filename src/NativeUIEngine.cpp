#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include "include/rawrxd_dock_manager.h"

// Core Interface Exports
extern "C" bool Titan_LoadModel(const char* modelPath);
extern "C" bool Titan_UnloadModel();
extern "C" void AppendTerminalText(const char* text);

/**
 * RawrXD Native UI Engine (v22.0.0-DOCKMASTER)
 * Implementation of the sovereign Docking Manager shell.
 * Multi-Domain Workspace Compositor using Recursive Layout Trees.
 */

#pragma comment(lib, "comctl32.lib")

// --- Global UI State for Docking Manager ---
static UI_STATE g_UI = {0};
static DOCK_NODE g_Nodes[16]; // Pool of pre-allocated nodes for basic layout
static int g_NodeCount = 0;

DOCK_NODE* CreateSplit(DOCK_NODE* parent, SPLIT_AXIS axis, int pos) {
    DOCK_NODE* n = &g_Nodes[g_NodeCount++];
    n->kind = DNK_SPLIT;
    n->parent = parent;
    n->u.split.axis = axis;
    n->u.split.splitPos = pos;
    n->u.split.splitSize = 5;
    return n;
}

DOCK_NODE* CreateLeaf(DOCK_NODE* parent, uint32_t panelId) {
    DOCK_NODE* n = &g_Nodes[g_NodeCount++];
    n->kind = DNK_LEAF;
    n->parent = parent;
    n->u.leaf.panelId = panelId;
    return n;
}

// Global Handles for the Quad-Surface Architecture (v20.0.0-QUAD)
HWND hExplorer, hEditor, hChat;
HWND hTerminal;    
HWND hLineNumbers; 
HWND hSplitter1, hSplitter2; // Vertical Splitters
HWND hSplitter3;             // Horizontal Splitter for Terminal
WNDPROC OldEditorProc;
bool isDraggingS1 = false, isDraggingS2 = false, isDraggingS3 = false;
int explorerWidth = 250;     // LEFT: Navigation
int chatWidth = 350;         // RIGHT: AI / Analysis
int terminalHeight = 200;    // BOTTOM: Execution / Runtime
int lineNumberWidth = 45;

// Splitter Class Name
#define RAWR_SPLITTER_CLASS "RawrSplitter"

// MASM64 Keywords for Lexer
const char* masmKeywords[] = { 
    "mov", "add", "sub", "mul", "div", "ret", "proc", "endp", 
    "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
    "db", "dw", "dd", "dq", "equ", "offset", "ptr", "byte", "word", "dword", "qword",
    "vaddps", "vfmadd213ps", "vmlaunch", "int3", "syscall", "ALIGN", "STRUCT", "ENDS"
};

// Apply syntax highlighting (Basic)
void ApplyMasmHighlighting() {
    if (!hEditor) return;

    // Save Selection & Scroll
    CHARRANGE cr;
    SendMessage(hEditor, EM_EXGETSEL, 0, (LPARAM)&cr);
    POINT pt;
    SendMessage(hEditor, EM_GETSCROLLPOS, 0, (LPARAM)&pt);

    // Freeze screen for performance
    SendMessage(hEditor, WM_SETREDRAW, FALSE, 0);

    // Get Full Text
    int len = GetWindowTextLengthA(hEditor);
    if (len == 0) {
        SendMessage(hEditor, WM_SETREDRAW, TRUE, 0);
        return;
    }
    char* text = new char[len + 1];
    GetWindowTextA(hEditor, text, len + 1);

    // Reset default color
    CHARFORMAT2A cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = RGB(220, 220, 220); // Default Gray
    SendMessage(hEditor, EM_SETSEL, 0, -1);
    SendMessage(hEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    // Keyword pass (BLUE)
    cf.crTextColor = RGB(86, 156, 214); 
    for (const char* kw : masmKeywords) {
        size_t kwLen = strlen(kw);
        char* pos = text;
        while ((pos = strstr(pos, kw)) != NULL) {
            bool boundaryStart = (pos == text || !isalnum(*(pos-1)) && *(pos-1) != '_');
            bool boundaryEnd = (!isalnum(*(pos+kwLen)) && *(pos+kwLen) != '_');
            
            if (boundaryStart && boundaryEnd) {
                int startIdx = (int)(pos - text);
                SendMessage(hEditor, EM_SETSEL, startIdx, startIdx + kwLen);
                SendMessage(hEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            }
            pos += kwLen;
        }
    }

    // Comment pass (GREEN)
    cf.crTextColor = RGB(106, 153, 85);
    char* comment = text;
    while ((comment = strchr(comment, ';'))) {
        int startIdx = (int)(comment - text);
        char* endOfLine = strchr(comment, '\n');
        int endIdx = endOfLine ? (int)(endOfLine - text) : len;
        SendMessage(hEditor, EM_SETSEL, startIdx, endIdx);
        SendMessage(hEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        comment = endOfLine ? endOfLine + 1 : text + len;
    }

    // Restore selection and scroll
    SendMessage(hEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
    SendMessage(hEditor, EM_SETSCROLLPOS, 0, (LPARAM)&pt);
    SendMessage(hEditor, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hEditor, NULL, TRUE);

    delete[] text;
}


LRESULT CALLBACK LineNumberProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        SetBkColor(hdc, RGB(30, 30, 30));
        SetTextColor(hdc, RGB(120, 120, 120));
        HFONT hFont = (HFONT)SendMessage(hEditor, WM_GETFONT, 0, 0);
        SelectObject(hdc, hFont);

        // Get Editor Metrics
        int firstLine = (int)SendMessage(hEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
        int lineCount = (int)SendMessage(hEditor, EM_GETLINECOUNT, 0, 0);
        
        // Calculate Line Height
        TEXTMETRICA tm;
        GetTextMetricsA(hdc, &tm);
        int lineHeight = tm.tmHeight + tm.tmExternalLeading;

        // Draw visible line numbers
        RECT rc;
        GetClientRect(hWnd, &rc);
        int maxLines = (rc.bottom / lineHeight) + 1;
        
        char buf[16];
        for (int i = 0; i <= maxLines; i++) {
            int lineIdx = firstLine + i;
            if (lineIdx >= lineCount) break;
            sprintf(buf, "%d", lineIdx + 1);
            TextOutA(hdc, 5, i * lineHeight, buf, strlen(buf));
        }

        EndPaint(hWnd, &ps);
        return 0;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


// Helper to Load File into Editor
void LoadFileToEditor(const char* fileName) {
    if (!hEditor) return;
    FILE* f = fopen(fileName, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buf = new char[size + 1];
        fread(buf, 1, size, f);
        buf[size] = '\0';
        SetWindowTextA(hEditor, buf);
        delete[] buf;
        fclose(f);
        ApplyMasmHighlighting();
    }
}

// v22.0.1-VIRTUAL: Virtual Tab Rendering System
void RenderVirtualTabs(HDC hdc, DOCK_NODE* node) {
    if (node->kind != DNK_TABS) return;

    RECT rc = node->rcTabStrip;
    HBRUSH hBrush = CreateSolidBrush(RGB(45, 45, 48));
    FillRect(hdc, &rc, hBrush);
    DeleteObject(hBrush);

    // Render Tab Headers
    SetTextColor(hdc, RGB(220, 220, 220));
    SetBkMode(hdc, TRANSPARENT);
    
    // For now, render dummy tabs based on panel registry
    const char* tabs[] = { "Source", "ASM", "Trace" };
    int tabX = rc.left + 5;
    for (int i = 0; i < 3; i++) {
        RECT tabRc = { tabX, rc.top + 2, tabX + 80, rc.bottom - 2 };
        if (i == node->u.tabs.activeTabIndex) {
            HBRUSH hActive = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &tabRc, hActive);
            DeleteObject(hActive);
        }
        DrawTextA(hdc, tabs[i], -1, &tabRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        tabX += 85;
    }
}

// v22.1.0-FLICKER-FREE: Buffered GDI Repaint System
void RepaintShell(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    
    RECT rc;
    GetClientRect(hWnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    // 1. Create Backbuffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(memDC, memBM);

    // 2. Clear Background (Sovereign Gray)
    HBRUSH hbg = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(memDC, &rc, hbg);
    DeleteObject(hbg);

    // 3. Render Virtual Chrome (Tabs / Splitters / Overlays)
    if (g_UI.pShellRoot) {
        // Walk the tree and render chrome regions
        // For simplicity in this demo, render active node tabs
        RenderVirtualTabs(memDC, g_UI.pShellRoot); 
    }

    // 4. BitBlt to Screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    // 5. Cleanup
    DeleteObject(memBM);
    DeleteDC(memDC);
    EndPaint(hWnd, &ps);
}

LRESULT CALLBACK SplitterProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SETCURSOR:
            if (hWnd == hSplitter3) SetCursor(LoadCursor(NULL, IDC_SIZENS));
            else SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            return TRUE;
        case WM_LBUTTONDOWN:
            if (hWnd == hSplitter1) isDraggingS1 = true;
            else if (hWnd == hSplitter2) isDraggingS2 = true;
            else if (hWnd == hSplitter3) isDraggingS3 = true;
            SetCapture(hWnd);
            return 0;
        case WM_LBUTTONUP:
            isDraggingS1 = false;
            isDraggingS2 = false;
            isDraggingS3 = false;
            ReleaseCapture();
            return 0;
        case WM_MOUSEMOVE: {
            if (isDraggingS1 || isDraggingS2 || isDraggingS3) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(GetParent(hWnd), &pt);
                if (isDraggingS1) {
                    explorerWidth = pt.x;
                    if (explorerWidth < 50) explorerWidth = 50;
                } else if (isDraggingS2) {
                    RECT rc;
                    GetClientRect(GetParent(hWnd), &rc);
                    chatWidth = rc.right - pt.x;
                    if (chatWidth < 50) chatWidth = 50;
                } else if (isDraggingS3) {
                    RECT rc;
                    GetClientRect(GetParent(hWnd), &rc);
                    terminalHeight = rc.bottom - pt.y;
                    if (terminalHeight < 50) terminalHeight = 50;
                }
                // Trigger re-layout
                SendMessage(GetParent(hWnd), WM_SIZE, 0, 0);
            }
            return 0;
        }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Callback for the Editor (Subclassing for MASM interaction in Phase 3)
// v22.1.0-OVERLAY: Command Palette Management
static bool g_CommandActive = false;
static char g_CommandBuffer[256] = {0};

void ShowCommandPalette(bool show) {
    g_CommandActive = show;
    if (show) memset(g_CommandBuffer, 0, sizeof(g_CommandBuffer));
    InvalidateRect(GetParent(hEditor), NULL, TRUE);
}

void RenderCommandPalette(HDC hdc, RECT rc) {
    if (!g_CommandActive) return;

    int w = 500, h = 40;
    RECT palette = { (rc.right - w) / 2, 100, (rc.right + w) / 2, 140 };
    
    HBRUSH hp = CreateSolidBrush(RGB(45, 45, 48));
    FillRect(hdc, &palette, hp);
    DeleteObject(hp);

    SetTextColor(hdc, RGB(0, 122, 204));
    DrawTextA(hdc, "> Execute Sovereign Command", -1, &palette, DT_TOP | DT_LEFT | DT_SINGLELINE);
}

// v22.2.0-SOVEREIGN-A: Agentic Cognition Loop
#define WM_TITAN_SUGGESTION (WM_USER + 0x1000)

static char g_GhostText[4096] = {0};
static bool g_GhostVisible = false;

extern "C" __declspec(dllexport) void RenderGhostText(HDC hdc, RECT rc) {
    if (!g_GhostVisible) return;
    
    // Ghost Gray Color
    SetTextColor(hdc, RGB(95, 95, 95));
    SetBkMode(hdc, TRANSPARENT);
    
    // Logic: Render suggestion directly at caret (simulated for now)
    // In real implementation, we would use SendMessage(hEditor, EM_POSFROMCHAR, index, 0LL)
    DrawTextA(hdc, g_GhostText, -1, &rc, DT_LEFT | DT_TOP);
}

extern "C" __declspec(dllexport) void AcceptSuggestion() {
    if (g_GhostVisible && hEditor) {
        int len = GetWindowTextLengthA(hEditor);
        SendMessage(hEditor, EM_SETSEL, len, len);
        SendMessage(hEditor, EM_REPLACESEL, TRUE, (LPARAM)g_GhostText);
        g_GhostVisible = false;
        memset(g_GhostText, 0, sizeof(g_GhostText));
        InvalidateRect(hEditor, NULL, TRUE);
    }
}

// v22.2.0-SOVEREIGN-A: Ghost Text Implementation (Agent Alpha)
extern "C" __declspec(dllexport) void SetGhostText(const char* text) {
    if (text) {
        strncpy(g_GhostText, text, sizeof(g_GhostText) - 1);
        g_GhostVisible = true;
        if (hEditor) InvalidateRect(hEditor, NULL, TRUE);
    }
}

extern "C" __declspec(dllexport) void ClearGhostText() {
    g_GhostVisible = false;
    memset(g_GhostText, 0, sizeof(g_GhostText));
    if (hEditor) InvalidateRect(hEditor, NULL, TRUE);
}

LRESULT CALLBACK EditorSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Enhancement: [A] Wire Agentic Cognition Loop
    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_TAB && g_GhostVisible) {
            AcceptSuggestion();
            return 0; // Handled
        }
        if (wParam == VK_ESCAPE && g_GhostVisible) {
            g_GhostVisible = false;
            InvalidateRect(hEditor, NULL, TRUE);
            return 0; // Handled
        }
    }

    // v22.2.0-SOVEREIGN-B: Runtime Model Lifecycle Registry (Hot-Swap)
    if (uMsg == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'M') {
        // Toggle model swap (Demo cycle)
        static int cycle = 0;
        const char* models[] = { "Llama-3-8B.gguf", "DeepSeek-V3.gguf", "Phi-4.gguf" };
        Titan_LoadModel(models[cycle++ % 3]);
        char buf[256];
        sprintf(buf, "[Agent-B] Hot-Swapped to: %s", models[(cycle-1) % 3]);
        AppendTerminalText(buf);
        return 0;
    }

    // Enhancement: Ctrl+P for Command Palette
    if (uMsg == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'P') {
        ShowCommandPalette(!g_CommandActive);
        return 0;
    }

    if (uMsg == WM_CHAR && wParam == 9) { // Tab key support
        SendMessage(hWnd, EM_REPLACESEL, TRUE, (LPARAM)"    ");
        return 0;
    }
    
    // Invalidate line numbers when scrolling or resizing
    if (uMsg == WM_VSCROLL || uMsg == WM_SETTEXT || uMsg == WM_SIZE) {
        InvalidateRect(hLineNumbers, NULL, TRUE);
    }

    return CallWindowProc(OldEditorProc, hWnd, uMsg, wParam, lParam);
}

// Helper to Set Chat Text (Thread Safe candidate)
extern "C" __declspec(dllexport) void SetChatContent(const char* text) {
    if (hChat) {
        SetWindowTextA(hChat, text);
        SendMessage(hChat, EM_SETSEL, -1, -1);
        SendMessage(hChat, WM_VSCROLL, SB_BOTTOM, 0);
    }
}

// Helper to Append Chat Text (Token Generation Simulation)
extern "C" __declspec(dllexport) void AppendChatToken(const char* token) {
    if (hChat) {
        int len = GetWindowTextLengthA(hChat);
        SendMessage(hChat, EM_SETSEL, len, len);
        SendMessage(hChat, EM_REPLACESEL, FALSE, (LPARAM)token);
    }
}

// Helper to Save Content from Editor to File
extern "C" __declspec(dllexport) bool SaveEditorToFile(const char* fileName) {
    if (!hEditor || !fileName) return false;
    int len = GetWindowTextLengthA(hEditor);
    char* buf = new char[len + 1];
    GetWindowTextA(hEditor, buf, len + 1);
    
    FILE* f = fopen(fileName, "wb");
    if (f) {
        fwrite(buf, 1, len, f);
        fclose(f);
        delete[] buf;
        return true;
    }
    delete[] buf;
    return false;
}

// Logic to handle EN_CHANGE notifications to trigger lexer
extern "C" __declspec(dllexport) void HandleEditorChange() {
    static DWORD lastTick = 0;
    DWORD currentTick = GetTickCount();
    
    // Simple debounce to prevent coloring lag
    if (currentTick - lastTick > 500) {
        ApplyMasmHighlighting();
        lastTick = currentTick;
    }
    InvalidateRect(hLineNumbers, NULL, TRUE);
}


// v21.0.0-WORKSPACE: Multi-Domain Workspace Compositor
typedef int WorkspaceDomain;
#define DOMAIN_EDIT 0
#define DOMAIN_DEBUG 1
#define DOMAIN_MODEL 2

static WorkspaceDomain g_CurrentDomain = DOMAIN_EDIT;

extern "C" __declspec(dllexport) void SwitchWorkspaceDomain(WorkspaceDomain domain) {
    g_CurrentDomain = domain;
    switch (domain) {
        case DOMAIN_EDIT: 
            explorerWidth = 250; chatWidth = 350; terminalHeight = 200; 
            break;
        case DOMAIN_DEBUG: 
            explorerWidth = 0; chatWidth = 0; terminalHeight = 500; 
            break;
        case DOMAIN_MODEL: 
            explorerWidth = 200; chatWidth = 800; terminalHeight = 100; 
            break;
    }
    HWND hp = GetParent(hEditor);
    if (hp) PostMessage(hp, WM_SIZE, 0, 0);
}

// v22.1.0-PERSIST: Layout Serialization
extern "C" __declspec(dllexport) void SaveLayout(const char* fileName) {
    FILE* f = fopen(fileName, "w");
    if (!f) return;
    
    fprintf(f, "[SovereignLayout]\nDomain=%d\n", g_CurrentDomain);
    fprintf(f, "ExpW=%d\nChatW=%d\nTermH=%d\n", explorerWidth, chatWidth, terminalHeight);
    
    fclose(f);
}

extern "C" __declspec(dllexport) void LoadLayout(const char* fileName) {
    FILE* f = fopen(fileName, "r");
    if (!f) return;
    
    char buf[256];
    int domain = 0;
    while (fgets(buf, sizeof(buf), f)) {
        if (sscanf(buf, "Domain=%d", &domain) == 1) SwitchWorkspaceDomain(domain);
        sscanf(buf, "ExpW=%d", &explorerWidth);
        sscanf(buf, "ChatW=%d", &chatWidth);
        sscanf(buf, "TermH=%d", &terminalHeight);
    }
    
    fclose(f);
    // Explicit trigger re-layout
    HWND hp = GetParent(hEditor);
    if (hp) PostMessage(hp, WM_SIZE, 0, 0);
}

// v22.3.0-DRIVE-SENSE: Real Drive Enumeration
static std::string g_ExplorerCurrentPath = ".";

extern "C" __declspec(dllexport) void PopulateExplorer(const char* path) {
    if (!hExplorer) return;
    SendMessage(hExplorer, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
    g_ExplorerCurrentPath = path;

    TVINSERTSTRUCTA tvis = {0};
    tvis.hParent = TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;

    // Add "Computer" Root
    tvis.item.pszText = (char*)"This PC";
    HTREEITEM hPC = (HTREEITEM)SendMessage(hExplorer, TVM_INSERTITEMA, 0, (LPARAM)&tvis);

    // List Logical Drives
    char drives[256];
    DWORD len = GetLogicalDriveStringsA(sizeof(drives), drives);
    char* pDrive = drives;
    while (*pDrive) {
        tvis.hParent = hPC;
        tvis.item.pszText = pDrive;
        SendMessage(hExplorer, TVM_INSERTITEMA, 0, (LPARAM)&tvis);
        pDrive += strlen(pDrive) + 1;
    }

    // Expand current directory if specified
    if (path && strcmp(path, "This PC") != 0) {
        std::string searchPath = std::string(path) + "\\*";
        WIN32_FIND_DATAA ffd;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &ffd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (ffd.cFileName[0] == '.' && (ffd.cFileName[1] == 0 || ffd.cFileName[1] == '.')) continue;
                tvis.hParent = TVI_ROOT;
                tvis.item.pszText = ffd.cFileName;
                SendMessage(hExplorer, TVM_INSERTITEMA, 0, (LPARAM)&tvis);
            } while (FindNextFileA(hFind, &ffd) != 0);
            FindClose(hFind);
        }
    }
}

// Handle TreeView Notifications
extern "C" __declspec(dllexport) void HandleExplorerNotify(LPARAM lParam) {
    LPNMTREEVIEWA pnm = (LPNMTREEVIEWA)lParam;
    if (pnm->hdr.code == NM_DBLCLK || pnm->hdr.code == TVN_SELCHANGEDA) {
        HTREEITEM hItem = (HTREEITEM)SendMessage(hExplorer, TVM_GETNEXTITEM, TVGN_CARET, 0);
        if (hItem) {
            TVITEMA tvi = {0};
            char buf[MAX_PATH];
            tvi.hItem = hItem;
            tvi.mask = TVIF_TEXT | TVIF_HANDLE;
            tvi.pszText = buf;
            tvi.cchTextMax = sizeof(buf);
            if (SendMessage(hExplorer, TVM_GETITEMA, 0, (LPARAM)&tvi)) {
                // If it's a drive or directory, drill down. If file, load.
                std::string fullPath = g_ExplorerCurrentPath + "\\" + tvi.pszText;
                DWORD attr = GetFileAttributesA(fullPath.c_str());
                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                    PopulateExplorer(fullPath.c_str());
                } else {
                    LoadFileToEditor(fullPath.c_str());
                }
            }
        }
    }
}

// Helper to Append text to Terminal
extern "C" __declspec(dllexport) void AppendTerminalText(const char* text) {
    if (hTerminal) {
        int len = GetWindowTextLengthA(hTerminal);
        SendMessage(hTerminal, EM_SETSEL, len, len);
        SendMessage(hTerminal, EM_REPLACESEL, FALSE, (LPARAM)text);
        SendMessage(hTerminal, WM_VSCROLL, SB_BOTTOM, 0);
    }
}

extern "C" __declspec(dllexport) HWND CreateRawrXDTriPane(HWND hParent) {
    LoadLibraryA("Msftedit.dll"); // Load RichEdit 4.1+

    WNDCLASSEXA wc = {0} ;
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = SplitterProc;
    wc.lpszClassName = RAWR_SPLITTER_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_SIZEWE);
    RegisterClassExA(&wc);

    WNDCLASSEXA wlc = {0} ;
    wlc.cbSize = sizeof(WNDCLASSEXA);
    wlc.lpfnWndProc = LineNumberProc;
    wlc.lpszClassName = "RawrLineNumbers";
    wlc.hbrBackground = CreateSolidBrush(RGB(30,30,30));
    RegisterClassExA(&wlc);

    RECT rc;
    GetClientRect(hParent, &rc);
    int height = rc.bottom - rc.top;
    int width = rc.right - rc.left;

    // 1. EXPLORER (SysTreeView32)
    hExplorer = CreateWindowExA(0, WC_TREEVIEWA, "Explorer", 
        WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
        0, 0, explorerWidth, height, hParent, (HMENU)101, GetModuleHandle(NULL), NULL);

    // Splitter 1
    hSplitter1 = CreateWindowExA(0, RAWR_SPLITTER_CLASS, NULL, WS_CHILD | WS_VISIBLE,
        explorerWidth, 0, 5, height, hParent, NULL, GetModuleHandle(NULL), NULL);

    // Line Numbers
    hLineNumbers = CreateWindowExA(0, "RawrLineNumbers", "", WS_CHILD | WS_VISIBLE,
        explorerWidth + 5, 0, lineNumberWidth, height, hParent, NULL, GetModuleHandle(NULL), NULL);

    // 2. CHAT (Multiline Edit)
    hChat = CreateWindowExA(0, WC_EDITA, "RawrXD Sovereign Chat v15.1\r\nReady for input...", 
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
        width - chatWidth, 0, chatWidth, height, hParent, (HMENU)103, GetModuleHandle(NULL), NULL);

    // Splitter 2
    hSplitter2 = CreateWindowExA(0, RAWR_SPLITTER_CLASS, NULL, WS_CHILD | WS_VISIBLE,
        width - chatWidth - 5, 0, 5, height, hParent, NULL, GetModuleHandle(NULL), NULL);

    // 3. EDITOR (RichEdit 4.1+)
    int editorWidth = width - explorerWidth - chatWidth - lineNumberWidth - 10;
    int editorHeight = height - terminalHeight - 5;
    hEditor = CreateWindowExA(0, "RichEdit50W", "; RawrXD Native Editor", 
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL | ES_NOHIDESEL | WS_CLIPCHILDREN,
        explorerWidth + 5 + lineNumberWidth, 0, editorWidth, editorHeight, hParent, (HMENU)102, GetModuleHandle(NULL), NULL);

    // 4. TERMINAL (Native RichEdit for ANSI/CMD emulation)
    hTerminal = CreateWindowExA(0, "RichEdit50W", "RawrXD Terminal v1.0\r\n>", 
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
        explorerWidth + 5, editorHeight + 5, width - explorerWidth - chatWidth - 5, terminalHeight, hParent, (HMENU)104, GetModuleHandle(NULL), NULL);

    // Splitter 3 (Horizontal)
    hSplitter3 = CreateWindowExA(0, RAWR_SPLITTER_CLASS, NULL, WS_CHILD | WS_VISIBLE,
        explorerWidth + 5, editorHeight, width - explorerWidth - chatWidth - 5, 5, hParent, NULL, GetModuleHandle(NULL), NULL);

    // Apply Native Font (Consolas for coding)
    HFONT hFont = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    SendMessage(hExplorer, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hEditor, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hChat, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hTerminal, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Set RichEdit Event Mask to receive EN_CHANGE
    SendMessage(hEditor, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SCROLL);

    // Subclass editor for custom behavior
    OldEditorProc = (WNDPROC)SetWindowLongPtr(hEditor, GWLP_WNDPROC, (LONG_PTR)EditorSubclass);


    return hEditor;
}

extern "C" __declspec(dllexport) void ResizeRawrXDLanes(HWND hParent) {
    if (!hParent) return;
    RECT rc;
    GetClientRect(hParent, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    int termY = height - terminalHeight - 5;
    int edW = width - explorerWidth - chatWidth - lineNumberWidth - 10;
    int edH = height - terminalHeight - 5;

    MoveWindow(hExplorer, 0, 0, explorerWidth, height, TRUE);
    MoveWindow(hSplitter1, explorerWidth, 0, 5, height, TRUE);
    MoveWindow(hLineNumbers, explorerWidth + 5, 0, lineNumberWidth, height, TRUE);

    MoveWindow(hChat, width - chatWidth, 0, chatWidth, height, TRUE);
    MoveWindow(hSplitter2, width - chatWidth - 5, 0, 5, height, TRUE);
    
    MoveWindow(hEditor, explorerWidth + 5 + lineNumberWidth, 0, edW, edH, TRUE);
    MoveWindow(hSplitter3, explorerWidth + 5, termY, width - explorerWidth - chatWidth - 5, 5, TRUE);
    MoveWindow(hTerminal, explorerWidth + 5, termY + 5, width - explorerWidth - chatWidth - 5, terminalHeight, TRUE);
}

extern "C" __declspec(dllexport) void RunNativeMessageLoop() { MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); } }

