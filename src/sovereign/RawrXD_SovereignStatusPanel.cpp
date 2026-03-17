//=============================================================================
// RawrXD_SovereignStatusPanel.cpp
// Win32 implementation of sovereign status display
//=============================================================================

#include "RawrXD_SovereignStatusPanel.h"
#include <sstream>
#include <iomanip>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace RawrXD::UI {

// Global map for message routing (Windows classic pattern)
static std::unordered_map<HWND, SovereignStatusPanel*> g_panelMap;

SovereignStatusPanel::SovereignStatusPanel()
    : m_hWnd(nullptr),
      m_hLabelCycles(nullptr),
      m_hLabelStatus(nullptr),
      m_hLabelAgents(nullptr),
      m_hLabelHeals(nullptr),
      m_hButtonToggle(nullptr),
      m_hButtonManualCycle(nullptr),
      m_hListAgents(nullptr),
      m_core(Sovereign::SovereignCore::getInstance()),
      m_bridge(Sovereign::SovereignIDEBridge::getInstance())
{
    m_core.initialize(1);
}

SovereignStatusPanel::~SovereignStatusPanel() {
    if (m_hWnd) {
        destroy();
    }
}

HWND SovereignStatusPanel::create(
    HWND hParent,
    int x, int y, int cx, int cy,
    HMENU hMenu
)
{
    // Register window class if needed
    static ATOM panelClass = 0;
    if (!panelClass) {
        WNDCLASSA wc = {};
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = sizeof(void*);  // Store 'this' pointer
        wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = "RawrXD_SovereignStatusPanelClass";
        panelClass = RegisterClassA(&wc);
    }
    
    // Create main window
    m_hWnd = CreateWindowA(
        "RawrXD_SovereignStatusPanelClass",
        "Sovereign Agentic Core",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        x, y, cx, cy,
        hParent,
        hMenu,
        GetModuleHandleA(nullptr),
        nullptr
    );
    
    if (!m_hWnd) return nullptr;
    
    // Store 'this' in window user data for message routing
    SetWindowLongPtrA(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);
    g_panelMap[m_hWnd] = this;
    
    // Create child controls (labels, buttons)
    int controlY = 10;
    
    // Cycles label
    m_hLabelCycles = CreateWindowA(
        "STATIC",
        "Cycle: 0 | Heals: 0",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, controlY, cx - 20, 20,
        m_hWnd, nullptr,
        GetModuleHandleA(nullptr), nullptr
    );
    controlY += 25;
    
    // Status label
    m_hLabelStatus = CreateWindowA(
        "STATIC",
        "Status: IDLE",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, controlY, cx - 20, 20,
        m_hWnd, nullptr,
        GetModuleHandleA(nullptr), nullptr
    );
    controlY += 25;
    
    // Agents label
    m_hLabelAgents = CreateWindowA(
        "STATIC",
        "Agents: 1 active",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, controlY, cx - 20, 20,
        m_hWnd, nullptr,
        GetModuleHandleA(nullptr), nullptr
    );
    controlY += 25;
    
    // Toggle button
    m_hButtonToggle = CreateWindowA(
        "BUTTON",
        "Enable Sovereign",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, controlY, (cx - 30) / 2, 25,
        m_hWnd, (HMENU)101,
        GetModuleHandleA(nullptr), nullptr
    );
    controlY += 30;
    
    // Manual cycle button
    m_hButtonManualCycle = CreateWindowA(
        "BUTTON",
        "Run Cycle",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10 + (cx - 30) / 2 + 10, controlY - 30, (cx - 30) / 2, 25,
        m_hWnd, (HMENU)102,
        GetModuleHandleA(nullptr), nullptr
    );
    
    return m_hWnd;
}

void SovereignStatusPanel::destroy() {
    if (m_hWnd) {
        g_panelMap.erase(m_hWnd);
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

void SovereignStatusPanel::refresh() {
    if (!m_hWnd) return;
    
    // Get current stats
    auto stats = m_core.getStats();
    auto agents = m_core.getAgentStates();
    
    // Update cycles label
    char cyclesBuf[128];
    snprintf(cyclesBuf, sizeof(cyclesBuf),
        "Cycle: %llu | Heals: %llu",
        stats.cycleCount, stats.healCount
    );
    SetWindowTextA(m_hLabelCycles, cyclesBuf);
    
    // Update status label
    const char* statusStr = "IDLE";
    switch (stats.status) {
        case Sovereign::SovereignCore::Status::COMPILING:
            statusStr = "COMPILING"; break;
        case Sovereign::SovereignCore::Status::FIXING:
            statusStr = "FIXING"; break;
        case Sovereign::SovereignCore::Status::SYNCING:
            statusStr = "SYNCING"; break;
        default: break;
    }
    char statusBuf[128];
    snprintf(statusBuf, sizeof(statusBuf),
        "Status: %s | Running: %s",
        statusStr,
        m_core.isRunning() ? "YES" : "NO"
    );
    SetWindowTextA(m_hLabelStatus, statusBuf);
    
    // Update agents label
    char agentsBuf[128];
    snprintf(agentsBuf, sizeof(agentsBuf),
        "Agents: %u active",
        (unsigned)agents.size()
    );
    SetWindowTextA(m_hLabelAgents, agentsBuf);
}

void SovereignStatusPanel::setSovereignEnabled(bool enabled) {
    if (enabled && !m_core.isRunning()) {
        m_core.startAutonomousLoop();
        SetWindowTextA(m_hButtonToggle, "Disable Sovereign");
    } else if (!enabled && m_core.isRunning()) {
        m_core.stopAutonomousLoop();
        SetWindowTextA(m_hButtonToggle, "Enable Sovereign");
    }
}

HWND SovereignStatusPanel::getHWND() const {
    return m_hWnd;
}

LRESULT CALLBACK SovereignStatusPanel::WindowProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam
)
{
    // Retrieve 'this' pointer
    SovereignStatusPanel* pThis = nullptr;
    
    if (uMsg == WM_CREATE) {
        // On create, extract from CREATESTRUCT
        CREATESTRUCTA* pCreate = reinterpret_cast<CREATESTRUCTA*>(lParam);
        pThis = reinterpret_cast<SovereignStatusPanel*>(pCreate->lpCreateParams);
        SetWindowLongPtrA(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = reinterpret_cast<SovereignStatusPanel*>(
            GetWindowLongPtrA(hWnd, GWLP_USERDATA)
        );
    }
    
    if (pThis) {
        return pThis->onMessage(uMsg, wParam, lParam);
    }
    
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

LRESULT SovereignStatusPanel::onMessage(
    UINT uMsg, WPARAM wParam, LPARAM lParam
)
{
    switch (uMsg) {
        case WM_PAINT:
            onPaint();
            return 0;
        
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch (LOWORD(wParam)) {
                    case 101:
                        onButtonToggle();
                        break;
                    case 102:
                        onButtonManualCycle();
                        break;
                }
            }
            return 0;
        
        case WM_TIMER:
            refresh();
            return 0;
        
        default:
            return DefWindowProcA(m_hWnd, uMsg, wParam, lParam);
    }
}

void SovereignStatusPanel::onPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hWnd, &ps);
    
    // Light background
    RECT rc;
    GetClientRect(m_hWnd, &rc);
    FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
    
    EndPaint(m_hWnd, &ps);
}

void SovereignStatusPanel::onButtonToggle() {
    setSovereignEnabled(!m_core.isRunning());
    refresh();
}

void SovereignStatusPanel::onButtonManualCycle() {
    m_core.runCycle();
    refresh();
}

} // namespace RawrXD::UI
