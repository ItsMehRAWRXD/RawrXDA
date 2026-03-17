// ═════════════════════════════════════════════════════════════════════════════
// Win32_MainWindow.hpp - Pure Win32 Replacement for Qt MainWindow
// Zero Qt Dependencies - Native Win32 Window with RichEdit + Controls
// ═════════════════════════════════════════════════════════════════════════════

#pragma once

#ifndef RAWRXD_WIN32_MAIN_WINDOW_HPP
#define RAWRXD_WIN32_MAIN_WINDOW_HPP

#include "agent_kernel_main.hpp"
#include "Win32_HexConsole.hpp"
#include "Win32_HotpatchManager.hpp"
#include <windows.h>
#include <commctrl.h>
#include <functional>
#include <memory>

#pragma comment(lib, "comctl32.lib")

namespace RawrXD {
namespace Win32 {

// ═════════════════════════════════════════════════════════════════════════════
// Main Window - Pure Win32 Implementation
// ═════════════════════════════════════════════════════════════════════════════

class MainWindow {
public:
    static const UINT WM_HOTPATCH_CLICK = WM_USER + 1;
    
    explicit MainWindow(HINSTANCE hInstance = nullptr) 
        : hInstance_(hInstance), hwndMain_(nullptr), 
          hexConsole_(nullptr), hotpatchManager_(nullptr) {}
    
    virtual ~MainWindow() { Destroy(); }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Window Creation and Management
    // ─────────────────────────────────────────────────────────────────────────
    
    HWND Create(const String& title = L"RawrXD IDE", int width = 1024, int height = 768) {
        // Register window class
        WNDCLASSEXW wcex{};
        wcex.cbSize = sizeof(wcex);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = MainWindow::WndProc;
        wcex.hInstance = hInstance_;
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
        wcex.lpszClassName = L"RawrXD_MainWindow";
        wcex.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcex.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
        
        if (!RegisterClassExW(&wcex)) {
            return nullptr;
        }
        
        // Create main window
        hwndMain_ = CreateWindowExW(
            0,
            L"RawrXD_MainWindow",
            title.c_str(),
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            nullptr, nullptr, hInstance_, this
        );
        
        if (!hwndMain_) {
            return nullptr;
        }
        
        // Initialize components
        InitializeComponents();
        
        // Show window
        ShowWindowAsync(hwndMain_, SW_SHOW);
        UpdateWindow(hwndMain_);
        
        return hwndMain_;
    }
    
    HWND GetHandle() const { return hwndMain_; }
    
    void Destroy() {
        if (hwndMain_) {
            DestroyWindow(hwndMain_);
            hwndMain_ = nullptr;
        }
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Component Access
    // ─────────────────────────────────────────────────────────────────────────
    
    HexConsole* GetHexConsole() { return hexConsole_.get(); }
    HotpatchManager* GetHotpatchManager() { return hotpatchManager_.get(); }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Message Processing
    // ─────────────────────────────────────────────────────────────────────────
    
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_CREATE:
                return OnCreate();
                
            case WM_SIZE:
                return OnSize(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                
            case WM_COMMAND: {
                UINT id = LOWORD(wParam);
                UINT notifyCode = HIWORD(wParam);
                
                if (id == 1001) {  // Hotpatch button
                    OnHotpatchButtonClick();
                    return 0;
                }
                break;
            }
                
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
                
            case WM_CLOSE:
                DestroyWindow(hwndMain_);
                return 0;
        }
        
        return DefWindowProcW(hwndMain_, uMsg, wParam, lParam);
    }
    
private:
    // ─────────────────────────────────────────────────────────────────────────
    // Initialization
    // ─────────────────────────────────────────────────────────────────────────
    
    void InitializeComponents() {
        if (!hwndMain_) return;
        
        // Create HexConsole
        hexConsole_ = std::make_unique<HexConsole>(hwndMain_);
        HWND hwndConsole = hexConsole_->Create(10, 10, 1004 - 20, 700 - 60);
        
        // Connect console to log messages
        if (hwndConsole) {
            hexConsole_->AppendLog(L"RawrXD IDE initialized");
            hexConsole_->AppendLog(L"System ready for hotpatching");
        }
        
        // Create Hotpatch Manager
        hotpatchManager_ = std::make_unique<HotpatchManager>();
        
        // Register callbacks
        hotpatchManager_->SetOnLogMessage([this](const String& msg) {
            if (hexConsole_) {
                hexConsole_->AppendLog(msg);
            }
        });
        
        hotpatchManager_->SetOnPatchProgress([this](int current, int total) {
            if (hexConsole_) {
                String progress = L"Progress: " + std::to_wstring(current) + L"/" + std::to_wstring(total);
                hexConsole_->AppendLog(progress);
            }
        });
        
        hotpatchManager_->SetOnPatchComplete([this](const PatchResult& result) {
            if (hexConsole_) {
                if (result.success) {
                    hexConsole_->AppendLogFormatted(L"SUCCESS", result.message, RGB(0, 255, 0));
                } else {
                    hexConsole_->AppendLogFormatted(L"ERROR", result.message, RGB(255, 0, 0));
                }
            }
        });
        
        hotpatchManager_->SetOnPatchError([this](const String& error) {
            if (hexConsole_) {
                hexConsole_->AppendLogFormatted(L"ERROR", error, RGB(255, 100, 100));
            }
        });
        
        // Create Hotpatch button
        CreateControlsPanel();
    }
    
    void CreateControlsPanel() {
        int buttonY = 710;
        int buttonWidth = 100;
        int buttonHeight = 30;
        int buttonX = 10;
        
        // Hotpatch button
        HWND hwndButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Hotpatch",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            buttonX, buttonY, buttonWidth, buttonHeight,
            hwndMain_,
            (HMENU)1001,
            hInstance_,
            nullptr
        );
        
        if (hwndButton) {
            // Set button font
            HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH, L"Segoe UI");
            SendMessageW(hwndButton, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
        
        // Clear button
        hwndButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Clear",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            buttonX + buttonWidth + 10, buttonY, buttonWidth, buttonHeight,
            hwndMain_,
            (HMENU)1002,
            hInstance_,
            nullptr
        );
        
        if (hwndButton) {
            HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH, L"Segoe UI");
            SendMessageW(hwndButton, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Event Handlers
    // ─────────────────────────────────────────────────────────────────────────
    
    LRESULT OnCreate() {
        return 0;
    }
    
    LRESULT OnSize(int width, int height) {
        if (hexConsole_ && hexConsole_->GetHandle()) {
            SetWindowPos(hexConsole_->GetHandle(), HWND_TOP, 
                        10, 10, width - 20, height - 60, SWP_NOZORDER);
        }
        return 0;
    }
    
    void OnHotpatchButtonClick() {
        if (hotpatchManager_) {
            if (hexConsole_) {
                hexConsole_->AppendLog(L"");
                hexConsole_->AppendLog(L"=== Starting Hotpatch ===");
            }
            
            hotpatchManager_->PerformHotpatch();
        }
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Static Window Procedure
    // ─────────────────────────────────────────────────────────────────────────
    
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        MainWindow* pThis = nullptr;
        
        if (uMsg == WM_CREATE) {
            CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
            pThis->hwndMain_ = hwnd;
        } else {
            pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        
        if (pThis) {
            return pThis->HandleMessage(uMsg, wParam, lParam);
        }
        
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Helper Macros
    // ─────────────────────────────────────────────────────────────────────────
    
    #define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
    #define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
    
    // ─────────────────────────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────────────────────────
    
    HINSTANCE hInstance_;
    HWND hwndMain_;
    std::unique_ptr<HexConsole> hexConsole_;
    std::unique_ptr<HotpatchManager> hotpatchManager_;
};

} // namespace Win32
} // namespace RawrXD

#endif // RAWRXD_WIN32_MAIN_WINDOW_HPP
