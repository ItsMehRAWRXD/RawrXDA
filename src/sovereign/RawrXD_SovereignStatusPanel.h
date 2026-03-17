#pragma once
//=============================================================================
// RawrXD_SovereignStatusPanel.h
// Win32 "Sovereign Agentic Core" status panel for IDE sidebar
// Displays: cycle count, status, agent states, heal counter
//=============================================================================

#include <windows.h>
#include <string>
#include "SovereignCoreWrapper.hpp"

namespace RawrXD::UI {

class SovereignStatusPanel {
public:
    SovereignStatusPanel();
    ~SovereignStatusPanel();
    
    // Create Win32 control
    HWND create(
        HWND hParent,
        int x, int y, int cx, int cy,
        HMENU hMenu = nullptr
    );
    
    // Destroy control
    void destroy();
    
    // Update display from sovereign core
    void refresh();
    
    // Enable/disable sovereign in IDE
    void setSovereignEnabled(bool enabled);
    
    // Get window handle
    HWND getHWND() const;

private:
    HWND m_hWnd;
    HWND m_hLabelCycles;
    HWND m_hLabelStatus;
    HWND m_hLabelAgents;
    HWND m_hLabelHeals;
    HWND m_hButtonToggle;
    HWND m_hButtonManualCycle;
    HWND m_hListAgents;
    
    Sovereign::SovereignCore& m_core;
    Sovereign::SovereignIDEBridge& m_bridge;
    
    // Windows message handlers
    static LRESULT CALLBACK WindowProc(
        HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam
    );
    
    LRESULT onMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void onPaint();
    void onButtonToggle();
    void onButtonManualCycle();
    
    // Helper to format status text
    std::string formatStatusText() const;
    std::string formatAgentText() const;
};

} // namespace RawrXD::UI
