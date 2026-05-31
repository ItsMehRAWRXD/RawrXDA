#include "Win32IDE_AgentHUD.h"
#include "../engine/global_runtime_orchestrator.h"
#include <commctrl.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "comctl32.lib")

namespace RawrXD::UX {

AgentExecutionHUD& AgentExecutionHUD::instance() {
    static AgentExecutionHUD inst;
    return inst;
}

bool AgentExecutionHUD::create(HWND hwndParent) {
    if (m_hwnd) return true;
    
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"RawrXD_AgentHUD";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Will paint in WM_PAINT
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);
    
    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_LAYERED,
        L"RawrXD_AgentHUD",
        L"Agent Tool",
        WS_POPUP | WS_BORDER,
        0, 0, 420, 160,
        hwndParent, nullptr, GetModuleHandle(nullptr), this
    );
    
    if (!m_hwnd) return false;

    // Use alpha for cleaner look
    SetLayeredWindowAttributes(m_hwnd, 0, 235, LWA_ALPHA);
    
    // Progress Bar (Marquee)
    m_hwndProgress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_MARQUEE,
        10, 45, 400, 10, m_hwnd, reinterpret_cast<HMENU>(1), GetModuleHandle(nullptr), nullptr);
    SendMessage(m_hwndProgress, PBM_SETMARQUEE, TRUE, 50);
    
    // Status text (Static)
    m_hwndStatus = CreateWindowExW(0, L"STATIC", L"Ready",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 10, 320, 25, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    
    HFONT hFont = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    SendMessage(m_hwndStatus, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    
    // Cancel button
    m_hwndCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        340, 10, 70, 25, m_hwnd, reinterpret_cast<HMENU>(2), GetModuleHandle(nullptr), nullptr);
    
    m_visible = false;
    ShowWindow(m_hwnd, SW_HIDE);

    // Refresh telemetry every 100ms when visible
    SetTimer(m_hwnd, 2, 100, nullptr);
    
    return true;
}

void AgentExecutionHUD::showToolExecuting(const std::string& toolName, const std::string& args) {
    m_currentTool = toolName;
    std::wstring status = L"Running: " + std::wstring(toolName.begin(), toolName.end());
    
    if (!args.empty() && args.length() < 30) {
        status += L" (" + std::wstring(args.begin(), args.end()) + L")";
    }
    
    SetWindowTextW(m_hwndStatus, status.c_str());
    positionNearCursor();
    ShowWindow(m_hwnd, SW_SHOW);
    m_visible = true;
}

void AgentExecutionHUD::updateTelemetry() {
    if (m_visible) {
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void AgentExecutionHUD::completeTool(const std::string& result, bool success) {
    std::wstring msg = success ? L"Completed: " : L"Failed: ";
    msg += std::wstring(m_currentTool.begin(), m_currentTool.end());
    SetWindowTextW(m_hwndStatus, msg.c_str());
    
    SendMessage(m_hwndProgress, PBM_SETMARQUEE, FALSE, 0);
    SetWindowLong(m_hwndProgress, GWL_STYLE, GetWindowLong(m_hwndProgress, GWL_STYLE) & ~PBS_MARQUEE);
    SendMessage(m_hwndProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(m_hwndProgress, PBM_SETPOS, 100, 0);
    
    // Auto-hide after 2 seconds
    SetTimer(m_hwnd, 1, 2000, nullptr);
}

void AgentExecutionHUD::hide() {
    ShowWindow(m_hwnd, SW_HIDE);
    m_visible = false;
    SendMessage(m_hwndProgress, PBM_SETMARQUEE, TRUE, 50); // Reset for next use
}

void AgentExecutionHUD::positionNearCursor() {
    POINT pt;
    if (GetCursorPos(&pt)) {
        SetWindowPos(m_hwnd, HWND_TOPMOST, pt.x + 20, pt.y + 20, 0, 0, SWP_NOSIZE);
    }
}

LRESULT CALLBACK AgentExecutionHUD::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    auto* self = reinterpret_cast<AgentExecutionHUD*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);
    
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Draw Background
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            HBRUSH hbg = CreateSolidBrush(RGB(20, 20, 20));
            FillRect(hdc, &clientRect, hbg);
            DeleteObject(hbg);

            // Fetch Orchestrator State
            auto state = GlobalRuntimeOrchestrator::Get().GetCurrentState();
            
            HFONT hFont = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Consolas");
            HGDIOBJ hOldFont = SelectObject(hdc, hFont);
            SetTextColor(hdc, RGB(0, 255, 128)); // Matrix Green
            SetBkMode(hdc, TRANSPARENT);

            std::wstringstream ss;
            ss << std::fixed << std::setprecision(2);
            ss << L"ENGINE TELEMETRY (Adaptive Optimizer)\n";
            ss << L"--------------------------------------\n";
            ss << L"Speculative Depth (N): " << state.optimal_speculate_n << L" [" << state.momentum_n << L"]\n";
            ss << L"Acceptance Rate: " << (state.avg_acceptance_rate * 100.0f) << L"%\n";
            ss << L"Memory Pressure: " << (state.current_pressure * 100.0f) << L"% (dP/dt: " << state.pressure_derivative << L")\n";
            ss << L"Cache Reuse Prob: " << (state.cache_reuse_prob * 100.0f) << L"%\n";
            ss << L"Score: " << state.performance_score << L" (W_Thr: " << state.dyn_w_throughput << L", W_Mem: " << state.dyn_w_memory_risk << L")\n";
            ss << L"Avg N: " << state.telemetry.avg_n << L" | Pulses: " << state.telemetry.pulses << L" | Overrides: " << state.telemetry.overrides;

            RECT textRect = { 10, 60, 410, 150 };
            DrawTextW(hdc, ss.str().c_str(), -1, &textRect, DT_LEFT);

            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == 2 && self->onCancelRequested) {
                self->onCancelRequested();
                self->hide();
            }
            return 0;
        case WM_TIMER:
            if (wParam == 1) {
                self->hide();
                KillTimer(hwnd, 1);
            } else if (wParam == 2) {
                self->updateTelemetry();
            }
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void AgentExecutionHUD::destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

} // namespace RawrXD::UX
