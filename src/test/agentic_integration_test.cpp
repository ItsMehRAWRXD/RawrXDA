// ============================================================================
// Agentic Integration Test - Validates ExecModeToolbar + GhostOverlay + ExecPipeline
// ============================================================================

#include <windows.h>
#include <iostream>
#include <string>
#include <memory>

#include "../win32ide/ExecModeToolbar.h"
#include "../win32ide/GhostOverlay.h"
#include "../agentic/ExecPipeline.h"
#include "../agentic/PatchEngine.h"

using namespace RawrXD::UI;
using namespace RawrXD::Agentic;

// Simple test window
LRESULT CALLBACK TestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static ExecModeToolbar* toolbar = nullptr;
    static GhostOverlay* ghost = nullptr;
    static ExecPipeline* pipeline = nullptr;
    
    switch (msg) {
    case WM_CREATE: {
        // Create toolbar
        toolbar = new ExecModeToolbar();
        toolbar->Create(hwnd, 10, 10);
        
        // Create a simple edit control as "editor"
        HWND hEdit = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
            10, 50, 600, 300, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        
        // Attach ghost overlay
        ghost = new GhostOverlay();
        ghost->Attach(hEdit);
        
        // Initialize pipeline
        pipeline = new ExecPipeline();
        VerificationConfig config;
        config.ctestCommand = L"echo Test passed";
        config.timeoutSeconds = 10;
        pipeline->Initialize(config);
        
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)toolbar);
        
        OutputDebugStringW(L"[Test] Agentic system initialized\n");
        return 0;
    }
    
    case WM_EXEC_MODE_CHANGED: {
        auto mode = (ExecMode)wParam;
        std::wstring label;
        switch (mode) {
        case ExecMode::Shadow: label = L"Shadow"; break;
        case ExecMode::Normal: label = L"Normal"; break;
        case ExecMode::Unsafe: label = L"Unsafe"; break;
        case ExecMode::Kernel: label = L"Kernel"; break;
        }
        OutputDebugStringW((L"[Test] Mode changed to: " + label + L"\n").c_str());
        return 0;
    }
    
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id >= 1000 && id < 1004 && toolbar) {
            toolbar->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return 0;
    }
    
    case WM_DRAWITEM: {
        if (toolbar) {
            LRESULT result = toolbar->HandleMessage(hwnd, msg, wParam, lParam);
            if (result != 0) return result;
        }
        return 0;
    }
    
    case WM_KEYDOWN: {
        if (toolbar && toolbar->HandleAccelerator(hwnd, wParam)) {
            return 0;
        }
        break;
    }
    
    case WM_SIZE: {
        // Resize edit control
        HWND hEdit = FindWindowEx(hwnd, nullptr, L"EDIT", nullptr);
        if (hEdit) {
            MoveWindow(hEdit, 10, 50, LOWORD(lParam) - 20, HIWORD(lParam) - 60, TRUE);
        }
        return 0;
    }
    
    case WM_DESTROY: {
        delete ghost;
        delete toolbar;
        delete pipeline;
        PostQuitMessage(0);
        return 0;
    }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TestWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"RawrXD_AgenticTest";
    RegisterClassExW(&wc);
    
    // Create window
    HWND hwnd = CreateWindowExW(0, L"RawrXD_AgenticTest", L"RawrXD Agentic Integration Test",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
