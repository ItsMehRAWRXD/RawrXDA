// ============================================================================
// Execution Mode Toolbar - Win32 Owner-Draw Segmented Control
// Shadow | Normal | Unsafe | Kernel
// ============================================================================

#pragma once
#include <windows.h>
#include <string>

namespace RawrXD {
namespace UI {

enum class ExecMode : int {
    Shadow = 0,   // Read-only / propose only
    Normal,       // Confirm before apply
    Unsafe,       // Auto-apply if tests pass
    Kernel        // System-level, hard gate
};

// Custom message broadcast to main window
#ifndef WM_EXEC_MODE_CHANGED
#define WM_EXEC_MODE_CHANGED (WM_APP + 0x200)
#endif
#ifndef WM_AGENT_APPLY_PATCH
#define WM_AGENT_APPLY_PATCH (WM_APP + 0x300)
#endif

struct ExecModeColors {
    static inline const COLORREF Shadow = RGB(160, 160, 160);
    static inline const COLORREF Normal = RGB(60, 140, 255);
    static inline const COLORREF Unsafe = RGB(255, 140, 0);
    static inline const COLORREF Kernel = RGB(220, 50, 50);
    static inline const COLORREF InactiveBg = RGB(40, 40, 40);
    static inline const COLORREF InactiveText = RGB(180, 180, 180);
    static constexpr COLORREF Border = RGB(80, 80, 80);
};

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

    // Keyboard shortcuts (Ctrl+Shift+1-4)
    bool HandleAccelerator(HWND hWnd, WPARAM wParam);

    // Status text for status bar
    std::wstring GetModeLabel() const;
    std::wstring GetModeDescription() const;

private:
    void DrawButton(LPDRAWITEMSTRUCT dis);
    void BroadcastMode();
    bool ConfirmEscalation(HWND hWnd, ExecMode newMode);

    HWND m_buttons[4] = {};
    ExecMode m_mode = ExecMode::Normal;
    HWND m_parent = nullptr;
    bool m_created = false;

    static constexpr int kButtonWidth = 90;
    static constexpr int kButtonHeight = 28;
    static constexpr int kButtonSpacing = 4;
    static constexpr int kBaseId = 1000;
};

// Global accessor (singleton per IDE instance)
ExecModeToolbar* GetExecModeToolbar();
void SetExecModeToolbar(ExecModeToolbar* toolbar);

} // namespace UI
} // namespace RawrXD
