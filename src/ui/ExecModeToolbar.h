#pragma once
#include <windows.h>

namespace RawrXD::UI {

enum class ExecMode : int {
    Shadow = 0,
    Normal,
    Unsafe,
    Kernel
};

// Custom message broadcast to main window when mode changes
#define WM_EXEC_MODE_CHANGED (WM_APP + 0x200)

class ExecModeToolbar {
public:
    ExecModeToolbar();
    ~ExecModeToolbar();

    bool Create(HWND hParent, int x, int y);
    void Destroy();

    ExecMode GetMode() const { return m_mode; }
    void SetMode(ExecMode mode);

    // Message handling - call from parent WndProc
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Static accessor for global instance
    static ExecModeToolbar* GetInstance();

private:
    void DrawButton(LPDRAWITEMSTRUCT dis);
    void BroadcastMode();

    HWND m_parent = nullptr;
    HWND m_buttons[4] = {};
    ExecMode m_mode = ExecMode::Normal;

    static constexpr int kButtonWidth = 90;
    static constexpr int kButtonHeight = 28;
    static constexpr int kButtonSpacing = 4;
};

// Global accessor
ExecMode GetExecMode();
void SetExecMode(ExecMode mode);

} // namespace RawrXD::UI
