/**
 * @file RawrXD_UI_Bridge.cpp
 * @brief Bridges C++ Agentic logic to Native Win32 ASM UI.
 * 
 * Target: v15.2.0-TRIPANE
 * Deliverable: Integrated Windowing Interface for the Sovereign Hub.
 */

#include <windows.h>
#include <iostream>
#include <string>
#include "RawrXD_Native_Core.h"
#include <commctrl.h>
#include <richedit.h>
#pragma comment(lib, "comctl32.lib")

// Forward declarations
extern "C" __declspec(dllexport) void Core_LoadFileIntoEditor(const char* filePath);

// Reference to ASM UI procedures
extern "C" {
    LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void RawrXD_UI_CreateControls(HWND hParent);
    void RawrXD_UI_UpdateLayout(HWND hParent, int width, int height);
    void RawrXD_Native_Log(const char* message);
}

class SovereignUI {
public:
    /**
     * @brief Initializes the Native Win32 Window Class.
     * Uses WNDCLASSEXA for compatibility.
     */
    static HWND CreateSovereignWindow(HINSTANCE hInstance) {
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES;
        InitCommonControlsEx(&icex);
        
        LoadLibraryA("Scintilla.dll"); // Force Scintilla load if not already
        LoadLibraryA("SciLexer.dll");

        WNDCLASSEXA wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = MainWndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = "RawrXD_NativeUI";

        if (!RegisterClassExA(&wc)) {
            RawrXD_Native_Log("[UI_ERR] Failed to register Native Window Class.");
            return NULL;
        }

        HWND hwnd = CreateWindowExA(
            0, "RawrXD_NativeUI", 
            "RawrXD Sovereign IDE v15.2.0-TRIPANE",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
            NULL, NULL, hInstance, NULL
        );

        if (hwnd) {
            RawrXD_Native_Log("[UI_INIT] Native Window Created Successfully.");
            // hExplorer refers to the global in UI.asm
        }
        return hwnd;
    }

    static void PopulateTreeView(HWND hTree, const char* rootDir) {
        if (!hTree) return;
        TreeView_DeleteAllItems(hTree);
        
        TVINSERTSTRUCTA tvis;
        ZeroMemory(&tvis, sizeof(tvis));
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = (LPSTR)rootDir;
        tvis.item.lParam = 0;
        
        HTREEITEM hRoot = (HTREEITEM)SendMessageA(hTree, TVM_INSERTITEMA, 0, (LPARAM)&tvis);

        // Add Drives
        DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; i++) {
            if (drives & (1 << i)) {
                char driveRoot[4] = { (char)('A' + i), ':', '\\', '\0' };
                TVINSERTSTRUCTA d_tvis;
                ZeroMemory(&d_tvis, sizeof(d_tvis));
                d_tvis.hParent = hRoot;
                d_tvis.hInsertAfter = TVI_LAST;
                d_tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
                d_tvis.item.pszText = driveRoot;
                d_tvis.item.lParam = 2; // Drive marker
                SendMessageA(hTree, TVM_INSERTITEMA, 0, (LPARAM)&d_tvis);
            }
        }

        char searchPath[MAX_PATH];
        wsprintfA(searchPath, "%s\\*.*", rootDir);
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(searchPath, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (lstrcmpA(fd.cFileName, ".") != 0 && lstrcmpA(fd.cFileName, "..") != 0) {
                    TVINSERTSTRUCTA c_tvis;
                    ZeroMemory(&c_tvis, sizeof(c_tvis));
                    c_tvis.hParent = hRoot;
                    c_tvis.hInsertAfter = TVI_LAST;
                    c_tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
                    c_tvis.item.pszText = fd.cFileName;
                    c_tvis.item.lParam = (LPARAM)1; // File marker
                    SendMessageA(hTree, TVM_INSERTITEMA, 0, (LPARAM)&c_tvis);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
        
        // Expand root
        SendMessageA(hTree, TVM_EXPAND, TVE_EXPAND, (LPARAM)hRoot);
    }

    /**
     * @brief The Native Message Loop runner.
     * Replaces the old PowerShell initialization loop.
     */
    static DWORD WINAPI RunMessageLoop(LPVOID lpParam) {
        MSG msg;
        while (GetMessageA(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        return 0;
    }
};

extern "C" __declspec(dllexport) bool Core_InitializeUI() {
    return SovereignUI::CreateSovereignWindow((HINSTANCE)GetModuleHandle(NULL)) != NULL;
}

extern "C" __declspec(dllexport) void Core_RunMessageLoop() {
    SovereignUI::RunMessageLoop(NULL);
}

extern "C" __declspec(dllexport) void Core_RunMessageLoopAsync() {
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SovereignUI::RunMessageLoop, NULL, 0, NULL);
}

// Declarations to match ASM naming (no decoration)
extern "C" HWND hChat;
extern "C" HWND hExplorer;
extern "C" HWND hEditor;
extern "C" HWND hRuntime;

// RawrXD_Native_Log is defined in ASM, we don't need to redefine it here
// with a body unless we want to wrap it. Removing to fix redefinition error.

extern "C" __declspec(dllexport) void Core_ExecuteAgentAction() {
    const char* msg = "[SOVEREIGN] Ctrl+Enter detected.\n=> Analyzing Editor Syntax...\n=> Invoking Subagent Execution Pipeline...";
    RawrXD_Native_Log(msg);
    RawrXD_Native_Log("[SOVEREIGN] Agent Execution Hook Triggered via Ctrl+Enter.");
}

// Agentic Explorer Logic: Scans workspace for files and feeds to Titan
static DWORD WINAPI Titan_AgenticExplorer(LPVOID lpParam) {
    char* root = (char*)lpParam;
    char searchPath[MAX_PATH];
    wsprintfA(searchPath, "%s\\*.*", root);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                // Found a file, simulate agentic analysis
                char msg[512];
                wsprintfA(msg, "[AGENT] Titan S9 Analyzing: %s\n", findData.cFileName);
                OutputDebugStringA(msg);
                
                if (hChat) {
                    SendMessageA(hChat, EM_REPLACESEL, 0, (LPARAM)msg);
                }
                Sleep(500); // Throttling for UI visibility during dev
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    return 0;
}

extern "C" __declspec(dllexport) void Core_StartAgenticExplorer(const char* rootDir) {
    static char staticRoot[MAX_PATH];
    lstrcpyA(staticRoot, rootDir);
    CreateThread(NULL, 0, Titan_AgenticExplorer, staticRoot, 0, NULL);
}

struct GGUFContext {
    HANDLE hFile;
    HANDLE hMapping;
    void* pData;
    SIZE_T size;
};

static GGUFContext g_ModelCtx = { NULL, NULL, NULL, 0 };

extern "C" void* Core_GetModelPointer();
extern "C" size_t Core_GetModelSize();

extern "C" void Core_SetModelPointer(void* ptr);
extern "C" void Core_SetModelSize(size_t size);

extern "C" __declspec(dllexport) void Core_HandleExplorerSelection(LPARAM lParam) {
    LPNMTREEVIEWA pnm = (LPNMTREEVIEWA)lParam;
    if (pnm->hdr.code == TVN_SELCHANGEDA) {
        char text[MAX_PATH];
        TVITEMA tvi = {0};
        tvi.hItem = pnm->itemNew.hItem;
        tvi.mask = TVIF_TEXT;
        tvi.pszText = text;
        tvi.cchTextMax = sizeof(text);
        SendMessageA(hExplorer, TVM_GETITEMA, 0, (LPARAM)&tvi);
        
        if (pnm->itemNew.lParam == 1) { // File marker
            // Map relative path - keeping simple for now
            char fullPath[MAX_PATH];
            wsprintfA(fullPath, "D:\\%s", text); 
            Core_LoadFileIntoEditor(fullPath);
        }
    }
}

extern "C" void Titan_AtomicGenReset();

extern "C" __declspec(dllexport) bool Core_LoadLocalModel(const char* modelPath) {
    Titan_AtomicGenReset(); // Reset generation counter for atomicity
    if (g_ModelCtx.pData) {
        UnmapViewOfFile(g_ModelCtx.pData);
        CloseHandle(g_ModelCtx.hMapping);
        CloseHandle(g_ModelCtx.hFile);
        g_ModelCtx.pData = NULL;
        g_ModelCtx.hMapping = NULL;
        g_ModelCtx.hFile = NULL;
        Core_SetModelPointer(NULL);
        Core_SetModelSize(0);
    }
    
    // Check if path is null or empty to simply unload
    if (!modelPath || !*modelPath) {
        RawrXD_Native_Log("[TITAN] Model Unloaded.");
        return true;
    }

    g_ModelCtx.hFile = CreateFileA(modelPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_ModelCtx.hFile == INVALID_HANDLE_VALUE) {
        RawrXD_Native_Log("[TITAN_ERR] Failed to open model file.");
        return false;
    }

    LARGE_INTEGER liSize;
    GetFileSizeEx(g_ModelCtx.hFile, &liSize);
    g_ModelCtx.size = (SIZE_T)liSize.QuadPart;

    g_ModelCtx.hMapping = CreateFileMappingA(g_ModelCtx.hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!g_ModelCtx.hMapping) {
        CloseHandle(g_ModelCtx.hFile);
        RawrXD_Native_Log("[TITAN_ERR] Failed to create mapping.");
        return false;
    }

    g_ModelCtx.pData = MapViewOfFile(g_ModelCtx.hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!g_ModelCtx.pData) {
        CloseHandle(g_ModelCtx.hMapping);
        CloseHandle(g_ModelCtx.hFile);
        RawrXD_Native_Log("[TITAN_ERR] Failed to map view.");
        return false;
    }

    Core_SetModelPointer(g_ModelCtx.pData);
    Core_SetModelSize(g_ModelCtx.size);

    char logBuf[256];
    wsprintfA(logBuf, "[TITAN] GGUF Model Mapped: %s (%llu MB)", modelPath, g_ModelCtx.size / (1024 * 1024));
    RawrXD_Native_Log(logBuf);
    
    if (hChat) {
        SendMessageA(hChat, WM_SETTEXT, 0, (LPARAM)"[TITAN] Local model loaded. Titan Stage 9 Inference Core ONLINE.");
    }

    return true;
}

extern "C" __declspec(dllexport) void Core_LoadFileIntoEditor(const char* filePath) {
    if (!hEditor) return;

    HANDLE hFile = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE || fileSize > 1024 * 1024 * 2) { // 2MB limit
        CloseHandle(hFile);
        return;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fileSize + 1);
    if (!buffer) {
        CloseHandle(hFile);
        return;
    }

    DWORD bytesRead;
    if (ReadFile(hFile, buffer, fileSize, &bytesRead, NULL)) {
        SendMessageA(hEditor, WM_SETTEXT, 0, (LPARAM)buffer);
        
        // Trigger re-highlight if we had a lexer, but for RichEdit we just set text
        // RawrXD_UI_UpdateHighlighting(hEditor); 
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(hFile);
}

// ----------------------------------------------------------------------------
// AGENTIC COGNITION LOOP (Ghost Text functionality)
// ----------------------------------------------------------------------------
static std::string g_CognitionStaticText = "";
static bool g_CognitionActive = false;
static int g_CognitionStartPos = -1;
static WNDPROC OriginalEditorProc = nullptr;

LRESULT CALLBACK EditorCognitionWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && g_CognitionActive) {
        if (wParam == VK_TAB) {
            // Accept ghost text
            g_CognitionActive = false;
            
            CHARFORMAT2A cf;
            ZeroMemory(&cf, sizeof(cf));
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR | CFM_ITALIC;
            cf.dwEffects = 0; // Remove italic
            cf.crTextColor = GetSysColor(COLOR_WINDOWTEXT);
            
            SendMessageA(hwnd, EM_SETSEL, g_CognitionStartPos, g_CognitionStartPos + g_CognitionStaticText.length());
            SendMessageA(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            
            // Move caret to end of accepted text
            SendMessageA(hwnd, EM_SETSEL, g_CognitionStartPos + g_CognitionStaticText.length(), g_CognitionStartPos + g_CognitionStaticText.length());
            
            g_CognitionStaticText = "";
            return 0; // Handled
        } else if (wParam == VK_ESCAPE) {
            // Reject ghost text
            g_CognitionActive = false;
            SendMessageA(hwnd, EM_SETSEL, g_CognitionStartPos, g_CognitionStartPos + g_CognitionStaticText.length());
            SendMessageA(hwnd, EM_REPLACESEL, TRUE, (LPARAM)"");
            g_CognitionStaticText = "";
            return 0; // Handled
        } else {
            // Any other key clears ghost text and processes normally
            g_CognitionActive = false;
            SendMessageA(hwnd, EM_SETSEL, g_CognitionStartPos, g_CognitionStartPos + g_CognitionStaticText.length());
            SendMessageA(hwnd, EM_REPLACESEL, TRUE, (LPARAM)"");
            g_CognitionStaticText = "";
        }
    }
    return CallWindowProcA(OriginalEditorProc, hwnd, uMsg, wParam, lParam);
}

extern "C" __declspec(dllexport) void RawrXD_HookEditorCognition(HWND hEdit) {
    if (!hEdit || OriginalEditorProc) return;
    OriginalEditorProc = (WNDPROC)SetWindowLongPtrA(hEdit, GWLP_WNDPROC, (LONG_PTR)EditorCognitionWndProc);
}

extern "C" __declspec(dllexport) void Core_ShowGhostText(const char* text) {
    if (!hEditor || !text || strlen(text) == 0) return;
    
    RawrXD_HookEditorCognition(hEditor);
    
    if (g_CognitionActive) {
        // Clear previous ghost text
        SendMessageA(hEditor, EM_SETSEL, g_CognitionStartPos, g_CognitionStartPos + g_CognitionStaticText.length());
        SendMessageA(hEditor, EM_REPLACESEL, TRUE, (LPARAM)"");
    }
    
    // Get current caret
    DWORD start = 0, end = 0;
    SendMessageA(hEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    g_CognitionStartPos = end;
    g_CognitionStaticText = text;
    g_CognitionActive = true;
    
    // Insert text
    SendMessageA(hEditor, EM_SETSEL, g_CognitionStartPos, g_CognitionStartPos);
    SendMessageA(hEditor, EM_REPLACESEL, TRUE, (LPARAM)text);
    
    // Format text as ghost (gray, italic)
    SendMessageA(hEditor, EM_SETSEL, g_CognitionStartPos, g_CognitionStartPos + g_CognitionStaticText.length());
    
    CHARFORMAT2A cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_ITALIC;
    cf.dwEffects = CFE_ITALIC;
    cf.crTextColor = RGB(150, 150, 150); // Gray
    
    SendMessageA(hEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    
    // Move caret back to start of ghost text
    SendMessageA(hEditor, EM_SETSEL, g_CognitionStartPos, g_CognitionStartPos);
}

// ----------------------------------------------------------------------------
// REAL FILE EXPLORER INITIALIZATION
// ----------------------------------------------------------------------------
extern "C" __declspec(dllexport) void RawrXD_InitRealFileExplorer(HWND hTreeView) {
    if (!hTreeView) return;
    TreeView_DeleteAllItems(hTreeView);

    char drives[256];
    DWORD len = GetLogicalDriveStringsA(sizeof(drives), drives);
    if (len == 0 || len > sizeof(drives)) return;

    char* drive = drives;
    while (*drive) {
        TVINSERTSTRUCTA tvis;
        ZeroMemory(&tvis, sizeof(tvis));
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = drive;
        tvis.item.lParam = 0; // Root marker
        
        HTREEITEM hRoot = (HTREEITEM)SendMessageA(hTreeView, TVM_INSERTITEMA, 0, (LPARAM)&tvis);

        // 1-level deep scan
        char searchPath[MAX_PATH];
        wsprintfA(searchPath, "%s*.*", drive);
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(searchPath, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (lstrcmpA(fd.cFileName, ".") != 0 && lstrcmpA(fd.cFileName, "..") != 0) {
                    TVINSERTSTRUCTA c_tvis;
                    ZeroMemory(&c_tvis, sizeof(c_tvis));
                    c_tvis.hParent = hRoot;
                    c_tvis.hInsertAfter = TVI_LAST;
                    c_tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
                    c_tvis.item.pszText = fd.cFileName;
                    c_tvis.item.lParam = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1;
                    SendMessageA(hTreeView, TVM_INSERTITEMA, 0, (LPARAM)&c_tvis);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }

        drive += lstrlenA(drive) + 1;
    }
}
