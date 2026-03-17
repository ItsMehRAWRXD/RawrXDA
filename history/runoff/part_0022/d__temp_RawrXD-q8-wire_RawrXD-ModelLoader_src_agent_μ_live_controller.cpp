/**
 * @file μ_live_controller.cpp
 * @brief Live IDE controller implementation - Production
 */

#include "μ_live_controller.hpp"
#include <shellapi.h>
#include <shlwapi.h>
#include <cstdio>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

// Define macros for lParam unpacking if not already defined
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

static LiveIDEController* g_live_controller = nullptr;

void LiveIDEController::Initialize(HWND ide_window) {
    ide_hwnd_ = ide_window;
    
    // Propose initial layout - the standard 3-panel + menu + breadcrumb layout
    
    // Node 0: Menu bar area
    RECT menu = {0, 0, 1200, 24};
    schematic_.ProposeNode(menu, RGB(200, 200, 200), 0x10, "MENU/ROUTER", "menu_bar", 0x4000);
    
    // Node 1: Breadcrumb area
    RECT breadcrumb = {0, 24, 1200, 48};
    schematic_.ProposeNode(breadcrumb, RGB(230, 230, 200), 0x6, "BREADCRUMB", "breadcrumb", 0x5000);
    
    // Node 2: File explorer (left panel)
    RECT explorer = {0, 48, 250, 600};
    schematic_.ProposeNode(explorer, RGB(100, 149, 237), 0x3, "β-EXPLORER", "file_explorer", 0x1000);
    
    // Node 3: Editor/Display (center panel)
    RECT editor = {250, 48, 950, 600};
    schematic_.ProposeNode(editor, RGB(255, 182, 193), 0x4, "DISPLAY", "editor", 0x8000);
    
    // Node 4: Settings/Properties (right panel)
    RECT settings = {950, 48, 1200, 600};
    schematic_.ProposeNode(settings, RGB(255, 218, 185), 0x38, "δ-SETTINGS", "settings_panel", 0x6000);
    
    // Node 5: Status bar (bottom)
    RECT status = {0, 600, 1200, 624};
    schematic_.ProposeNode(status, RGB(220, 220, 220), 0x4, "STATUS", "status_bar", 0x9000);
    
    // Node 6: Chat/Output panel (below or side)
    RECT chat = {250, 600, 950, 780};
    schematic_.ProposeNode(chat, RGB(220, 220, 255), 0x4, "CHAT/OUTPUT", "chat_panel", 0x7000);
    
    // Wire: Menu → All components
    schematic_.ProposeWire(0, 1, RGB(100, 100, 100), 0x10);
    schematic_.ProposeWire(0, 2, RGB(100, 100, 100), 0x10);
    schematic_.ProposeWire(0, 3, RGB(100, 100, 100), 0x10);
    schematic_.ProposeWire(0, 4, RGB(100, 100, 100), 0x10);
    schematic_.ProposeWire(0, 5, RGB(100, 100, 100), 0x10);
    
    // Wire: Breadcrumb ↔ Explorer
    schematic_.ProposeWire(1, 2, RGB(150, 150, 100), 0x2);
    
    // Wire: Explorer → Editor
    schematic_.ProposeWire(2, 3, RGB(144, 238, 144), 0x2);
    
    // Wire: Editor → Settings
    schematic_.ProposeWire(3, 4, RGB(255, 255, 224), 0x8);
    
    // Wire: Editor → Chat
    schematic_.ProposeWire(3, 6, RGB(144, 144, 238), 0x4);
    
    // Set up callbacks for real-time application
    schematic_.SetCallbacks(
        [this](const LiveNode& n) { OnApproved(n); },
        [this](const LiveNode& n) { OnRejected(n); },
        [this](const LiveNode& n, int x, int y) { OnMoved(n, x, y); },
        [this]() { OnCommitted(); }
    );
    
    ShowViewer();
}

void LiveIDEController::ShowViewer() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = ViewerProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "LiveSchematicViewer";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.style = CS_VREDRAW | CS_HREDRAW;
    RegisterClassA(&wc);
    
    viewer_hwnd_ = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "LiveSchematicViewer", "IDE LAYOUT - APPROVE OR REJECT",
        WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1240, 860,
        nullptr, nullptr, wc.hInstance, nullptr
    );
    
    SetWindowLongPtrA(viewer_hwnd_, GWLP_USERDATA, (LONG_PTR)this);
    schematic_.SetHWND(viewer_hwnd_);
    
    // Set timer for rendering
    SetTimer(viewer_hwnd_, 1, 16, nullptr); // 60 FPS
    
    ShowWindow(viewer_hwnd_, SW_SHOW);
    UpdateWindow(viewer_hwnd_);
}

LRESULT CALLBACK LiveIDEController::ViewerProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    LiveIDEController* self = nullptr;
    
    if (uMsg == WM_CREATE) {
        // Will be set by Initialize
        return 0;
    }
    
    self = (LiveIDEController*)GetWindowLongPtrA(hWnd, GWLP_USERDATA);
    if (!self) return DefWindowProcA(hWnd, uMsg, wParam, lParam);
    
    switch (uMsg) {
        case WM_TIMER: {
            if (wParam == 1) {
                self->schematic_.Render();
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            BitBlt(hdc, 0, 0, 1220, 820, self->schematic_.GetDC(), 0, 0, SRCCOPY);
            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            self->schematic_.OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            self->schematic_.OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        
        case WM_LBUTTONUP: {
            self->schematic_.OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
        
        case WM_RBUTTONDOWN: {
            self->schematic_.OnRButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        
        case WM_MBUTTONDOWN: {
            self->schematic_.OnMButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        
        case WM_KEYDOWN: {
            if (wParam == 'G') {
                RECT ghost = {0, 0, 120, 50};
                self->schematic_.SetGhostNode(ghost, RGB(180, 180, 180), 0x4, "[NEW]");
            } else {
                self->schematic_.OnKeyDown(wParam);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        
        case WM_KEYUP: {
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        
        case WM_CLOSE: {
            if (!self->committed_) {
                int r = MessageBoxA(hWnd, 
                    "Approve all changes before closing?\n\n"
                    "YES = Apply and close\n"
                    "NO = Discard changes and close\n"
                    "CANCEL = Keep editing",
                    "Uncommitted Changes", MB_YESNOCANCEL | MB_ICONQUESTION);
                
                if (r == IDYES) {
                    self->schematic_.Commit();
                    self->committed_ = true;
                    self->waiting_for_approval_ = false;
                } else if (r == IDNO) {
                    self->waiting_for_approval_ = false;
                } else {
                    return 0; // Cancel
                }
            }
            KillTimer(hWnd, 1);
            DestroyWindow(hWnd);
            return 0;
        }
        
        case WM_DESTROY: {
            self->viewer_hwnd_ = nullptr;
            return 0;
        }
    }
    
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

void LiveIDEController::OnApproved(const LiveNode& n) {
    char buf[512];
    snprintf(buf, sizeof(buf), "[APPROVED] %s (%s) - Applying to IDE", n.label.c_str(), n.identifier.c_str());
    OutputDebugStringA(buf);
    
    // Real-time application to IDE if IDE exists
    if (ide_hwnd_ && n.control_id >= 0) {
        HWND hCtrl = GetDlgItem(ide_hwnd_, n.control_id);
        if (hCtrl) {
            SetWindowPos(hCtrl, nullptr,
                n.bounds.left, n.bounds.top,
                n.bounds.right - n.bounds.left,
                n.bounds.bottom - n.bounds.top,
                SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }
}

void LiveIDEController::OnRejected(const LiveNode& n) {
    char buf[512];
    snprintf(buf, sizeof(buf), "[REJECTED] %s (%s) - Hiding from IDE", n.label.c_str(), n.identifier.c_str());
    OutputDebugStringA(buf);
    
    if (ide_hwnd_ && n.control_id >= 0) {
        HWND hCtrl = GetDlgItem(ide_hwnd_, n.control_id);
        if (hCtrl) {
            ShowWindow(hCtrl, SW_HIDE);
        }
    }
}

void LiveIDEController::OnMoved(const LiveNode& n, int x, int y) {
    // Live preview of movement
    char buf[256];
    snprintf(buf, sizeof(buf), "[MOVED] %s to (%d,%d)", n.label.c_str(), x, y);
    OutputDebugStringA(buf);
    
    if (n.state == ApprovalState::APPROVED && ide_hwnd_ && n.control_id >= 0) {
        OnApproved(n);
    }
}

void LiveIDEController::OnCommitted() {
    OutputDebugStringA("[COMMITTED] All approved layout changes applied to IDE");
    committed_ = true;
    waiting_for_approval_ = false;
}

void LiveIDEController::Finalize() {
    auto approved = schematic_.GetApprovedNodes();
    
    char buf[256];
    snprintf(buf, sizeof(buf), "[FINALIZE] Applying %zu approved nodes to IDE", approved.size());
    OutputDebugStringA(buf);
    
    for (const auto& n : approved) {
        if (ide_hwnd_ && n.control_id >= 0) {
            HWND hCtrl = GetDlgItem(ide_hwnd_, n.control_id);
            if (hCtrl) {
                SetWindowPos(hCtrl, nullptr,
                    n.bounds.left, n.bounds.top,
                    n.bounds.right - n.bounds.left,
                    n.bounds.bottom - n.bounds.top,
                    SWP_NOZORDER | SWP_SHOWWINDOW);
                
                // Force redraw
                InvalidateRect(hCtrl, nullptr, TRUE);
                UpdateWindow(hCtrl);
            }
        }
    }
    
    committed_ = true;
    waiting_for_approval_ = false;
}

// Global instance helper
LiveIDEController* GetLiveController() {
    return g_live_controller;
}

void SetLiveController(LiveIDEController* ctrl) {
    g_live_controller = ctrl;
}
