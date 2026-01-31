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

class MainWindow {
public:
    static const UINT WM_HOTPATCH_CLICK = WM_USER + 1;
    
    explicit MainWindow(HINSTANCE hInstance = nullptr) 
        : hInstance_(hInstance), hwndMain_(nullptr), 
          hexConsole_(nullptr), hotpatchManager_(nullptr) {}
    
    virtual ~MainWindow() { Destroy(); }
    
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
        
        InitializeComponents();
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
    
    HexConsole* GetHexConsole() { return hexConsole_.get(); }
    HotpatchManager* GetHotpatchManager() { return hotpatchManager_.get(); }
    
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_SIZE: {
                int width = (int)(short)LOWORD(lParam);
                int height = (int)(short)HIWORD(lParam);
                if (hexConsole_ && hexConsole_->GetHandle()) {
                    SetWindowPos(hexConsole_->GetHandle(), HWND_TOP, 
                                10, 10, width - 20, height - 60, SWP_NOZORDER);
                }
                return 0;
            }
                
            case WM_COMMAND: {
                UINT id = LOWORD(wParam);
                if (id == 1001 && hotpatchManager_) {
                    if (hexConsole_) {
                        hexConsole_->AppendLog(L"");
                        hexConsole_->AppendLog(L"=== Starting Hotpatch ===");
                    }
                    hotpatchManager_->PerformHotpatch();
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
    void InitializeComponents() {
        if (!hwndMain_) return;
        
        hexConsole_ = std::make_unique<HexConsole>(hwndMain_);
        HWND hwndConsole = hexConsole_->Create(10, 10, 1004 - 20, 700 - 60);
        
        if (hwndConsole) {
            hexConsole_->AppendLog(L"RawrXD IDE initialized");
            hexConsole_->AppendLog(L"System ready for hotpatching");
        }
        
        hotpatchManager_ = std::make_unique<HotpatchManager>();
        
        hotpatchManager_->SetOnLogMessage([this](const String& msg) {
            if (hexConsole_) {
                hexConsole_->AppendLog(msg);
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
        
        CreateControlsPanel();
    }
    
    void CreateControlsPanel() {
        int buttonY = 710;
        int buttonWidth = 100;
        int buttonHeight = 30;
        
        HWND hwndButton = CreateWindowExW(
            0, L"BUTTON", L"Hotpatch",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, buttonY, buttonWidth, buttonHeight,
            hwndMain_, (HMENU)1001, hInstance_, nullptr
        );
        
        if (hwndButton) {
            HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH, L"Segoe UI");
            SendMessageW(hwndButton, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
    }
    
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
    
    HINSTANCE hInstance_;
    HWND hwndMain_;
    std::unique_ptr<HexConsole> hexConsole_;
    std::unique_ptr<HotpatchManager> hotpatchManager_;
};

} // namespace Win32
} // namespace RawrXD

#endif // RAWRXD_WIN32_MAIN_WINDOW_HPP
