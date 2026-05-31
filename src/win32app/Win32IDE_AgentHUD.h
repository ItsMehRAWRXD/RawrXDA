#pragma once
#include <windows.h>
#include <string>
#include <functional>

namespace RawrXD::UX {

class AgentExecutionHUD {
public:
    static AgentExecutionHUD& instance();
    
    bool create(HWND hwndParent);
    void destroy();
    
    void showToolExecuting(const std::string& toolName, const std::string& args);
    void completeTool(const std::string& result, bool success);
    void updateTelemetry();
    void hide();
    
    std::function<void()> onCancelRequested;
    
    bool isVisible() const { return m_visible; }
    
private:
    AgentExecutionHUD() = default;
    
    HWND m_hwnd = nullptr;
    HWND m_hwndProgress = nullptr;
    HWND m_hwndStatus = nullptr;
    HWND m_hwndCancel = nullptr;
    bool m_visible = false;
    
    std::string m_currentTool;
    
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void positionNearCursor();
};

} // namespace RawrXD::UX
