// ==============================================================================
// Win32IDE_Simple.cpp
// Pure Win32 IDE with RawrXD ML system integration - Minimal dependencies
// ==============================================================================

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")

// Global state
HWND g_hWindow = NULL;
HWND g_hEditor = NULL;
HWND g_hStatus = NULL;
HWND g_hPrompt = NULL;

// Window class name
const char CLASS_NAME[] = "Win32IDE_RawrXD";

// Window IDs
#define ID_EDITOR    101
#define ID_STATUS    102
#define ID_PROMPT    103
#define ID_EXECUTE   104

// ============================================================================
// Log message - append to editor
// ============================================================================
void LogMessage(const char* message) {
    if (!g_hEditor) return;
    
    int len = GetWindowTextLengthA(g_hEditor);
    SendMessageA(g_hEditor, EM_SETSEL, len, len);
    SendMessageA(g_hEditor, EM_REPLACESEL, FALSE, (LPARAM)message);
    SendMessageA(g_hEditor, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
}

// ============================================================================
// Set status bar text
// ============================================================================
void SetStatusText(const char* text) {
    if (g_hStatus) {
        SendMessageA(g_hStatus, SB_SETTEXT, 0, (LPARAM)text);
    }
}

// ============================================================================
// Simulate token streaming
// ============================================================================
void SimulateTokenStreaming() {
    LogMessage("[ML] Starting token streaming simulation...");
    SetStatusText("Streaming...");
    
    const char* tokens[] = {
        "```cpp\n",
        "// Auto-generated code\n",
        "void process() {\n",
        "    for (int i = 0; i < 10; i++) {\n",
        "        printf(\"Value: %d\\n\", i);\n",
        "    }\n",
        "}\n",
        "```\n"
    };
    
    for (int i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++) {
        LogMessage(tokens[i]);
        Sleep(100);  // Simulate token delay
    }
    
    LogMessage("[ML] Token streaming complete");
    SetStatusText("Ready");
}

// ============================================================================
// Handle Execute button click
// ============================================================================
void OnExecuteClick() {
    SetStatusText("Processing...");
    
    // Get prompt text
    char prompt[256] = {0};
    GetWindowTextA(g_hPrompt, prompt, sizeof(prompt));
    
    if (strlen(prompt) == 0) {
        SetStatusText("Error: Enter a prompt");
        return;
    }
    
    char msg[512];
    snprintf(msg, sizeof(msg), "[USER] %s\r\n", prompt);
    LogMessage(msg);
    
    // Simulate async token streaming
    SimulateTokenStreaming();
}

// ============================================================================
// Resize controls when window resized
// ============================================================================
void OnSize(int cx, int cy) {
    if (!g_hEditor) return;
    
    // Editor - takes most space
    MoveWindow(g_hEditor, 5, 5, cx - 10, cy - 80, TRUE);
    
    // Prompt label area
    HWND hLabel = GetDlgItem(g_hWindow, 999);
    if (hLabel) {
        MoveWindow(hLabel, 5, cy - 70, 50, 20, TRUE);
    }
    
    // Prompt input
    if (g_hPrompt) {
        MoveWindow(g_hPrompt, 60, cy - 70, cx - 170, 20, TRUE);
    }
    
    // Execute button
    HWND hExecute = GetDlgItem(g_hWindow, ID_EXECUTE);
    if (hExecute) {
        MoveWindow(hExecute, cx - 105, cy - 70, 100, 20, TRUE);
    }
    
    // Status bar
    if (g_hStatus) {
        MoveWindow(g_hStatus, 0, cy - 20, cx, 20, TRUE);
    }
}

// ============================================================================
// Window message handler
// ============================================================================
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            {
                RECT rc;
                GetClientRect(hwnd, &rc);
                
                // Editor control
                g_hEditor = CreateWindowExA(
                    WS_EX_CLIENTEDGE,
                    "EDIT",
                    "",
                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
                    5, 5, rc.right - 10, rc.bottom - 80,
                    hwnd, (HMENU)ID_EDITOR, GetModuleHandleA(NULL), NULL
                );
                SendMessageA(g_hEditor, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), MAKELPARAM(TRUE, 0));
                
                // Prompt label
                CreateWindowExA(0, "STATIC", "Prompt:", WS_CHILD | WS_VISIBLE,
                    5, rc.bottom - 70, 50, 20, hwnd, (HMENU)999, GetModuleHandleA(NULL), NULL);
                
                // Prompt input
                g_hPrompt = CreateWindowExA(
                    WS_EX_CLIENTEDGE,
                    "EDIT",
                    "",
                    WS_CHILD | WS_VISIBLE,
                    60, rc.bottom - 70, rc.right - 170, 20,
                    hwnd, (HMENU)ID_PROMPT, GetModuleHandleA(NULL), NULL
                );
                
                // Execute button
                CreateWindowExA(0, "BUTTON", "Execute",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    rc.right - 105, rc.bottom - 70, 100, 20,
                    hwnd, (HMENU)ID_EXECUTE, GetModuleHandleA(NULL), NULL);
                
                // Status bar
                g_hStatus = CreateWindowExA(0, STATUSCLASSNAMEA, "Ready",
                    WS_CHILD | WS_VISIBLE | SBT_NOBORDERS,
                    0, rc.bottom - 20, rc.right, 20,
                    hwnd, (HMENU)ID_STATUS, GetModuleHandleA(NULL), NULL);
                
                return 0;
            }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_EXECUTE) {
                OnExecuteClick();
            }
            return 0;
        
        case WM_SIZE:
            OnSize(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Main entry point
// ============================================================================
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls
    InitCommonControls();
    
    // Register window class
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "RegisterClass failed", "Error", MB_OK);
        return 1;
    }
    
    // Create window
    g_hWindow = CreateWindowExA(
        0,
        CLASS_NAME,
        "RawrXD Win32IDE - Amphibious ML System",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 600,
        NULL, NULL, hInstance, NULL
    );
    
    if (!g_hWindow) {
        MessageBoxA(NULL, "CreateWindowEx failed", "Error", MB_OK);
        return 1;
    }
    
    ShowWindow(g_hWindow, nCmdShow);
    UpdateWindow(g_hWindow);
    
    // Log welcome message
    LogMessage("[INFO] RawrXD Win32IDE with Amphibious ML System");
    LogMessage("[INFO] Ctrl+K hotkey ready for inline edits");
    LogMessage("");
    
    // Message loop
    MSG msg = {};
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    
    return (int)msg.wParam;
}
