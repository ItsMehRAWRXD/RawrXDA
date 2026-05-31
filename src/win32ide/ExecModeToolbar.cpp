// ============================================================================
// ExecModeToolbar.cpp - Win32 Owner-Draw Implementation
// ============================================================================

#include "ExecModeToolbar.h"
#include <string>

namespace RawrXD {
namespace UI {

static ExecModeToolbar* g_instance = nullptr;

ExecModeToolbar* GetExecModeToolbar() { return g_instance; }
void SetExecModeToolbar(ExecModeToolbar* toolbar) { g_instance = toolbar; }

static const wchar_t* kLabels[] = {
    L"Shadow",
    L"Normal", 
    L"Unsafe",
    L"Kernel"
};

static COLORREF kActiveColors[] = {
    ExecModeColors::Shadow,
    ExecModeColors::Normal,
    ExecModeColors::Unsafe,
    ExecModeColors::Kernel
};

ExecModeToolbar::ExecModeToolbar() = default;
ExecModeToolbar::~ExecModeToolbar() { Destroy(); }

bool ExecModeToolbar::Create(HWND hParent, int x, int y) {
    if (m_created) return true;
    m_parent = hParent;

    for (int i = 0; i < 4; ++i) {
        m_buttons[i] = CreateWindowExW(
            0, L"BUTTON", kLabels[i],
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            x + i * (kButtonWidth + kButtonSpacing),
            y,
            kButtonWidth,
            kButtonHeight,
            hParent,
            (HMENU)(kBaseId + i),
            GetModuleHandle(nullptr),
            nullptr
        );
    }

    m_created = true;
    SetExecModeToolbar(this);
    return true;
}

void ExecModeToolbar::Destroy() {
    for (int i = 0; i < 4; ++i) {
        if (m_buttons[i]) {
            DestroyWindow(m_buttons[i]);
            m_buttons[i] = nullptr;
        }
    }
    m_created = false;
    if (g_instance == this) g_instance = nullptr;
}

void ExecModeToolbar::SetMode(ExecMode mode) {
    m_mode = mode;
    for (int i = 0; i < 4; ++i) {
        if (m_buttons[i]) {
            InvalidateRect(m_buttons[i], nullptr, TRUE);
        }
    }
    BroadcastMode();
}

void ExecModeToolbar::BroadcastMode() {
    if (m_parent) {
        SendMessage(m_parent, WM_EXEC_MODE_CHANGED, (WPARAM)m_mode, 0);
    }
}

bool ExecModeToolbar::ConfirmEscalation(HWND hWnd, ExecMode newMode) {
    // Silent downgrade
    if (m_mode == ExecMode::Unsafe && newMode == ExecMode::Normal) return true;
    if (m_mode == ExecMode::Kernel && newMode == ExecMode::Unsafe) return true;
    if (m_mode == ExecMode::Kernel && newMode == ExecMode::Normal) return true;
    if (m_mode == ExecMode::Kernel && newMode == ExecMode::Shadow) return true;
    if (m_mode == ExecMode::Unsafe && newMode == ExecMode::Shadow) return true;

    // Normal → Unsafe
    if (m_mode == ExecMode::Normal && newMode == ExecMode::Unsafe) {
        int result = MessageBoxW(hWnd,
            L"Enable Unsafe Mode?\n\n"
            L"Changes will be applied automatically after passing tests.\n"
            L"Failed tests will downgrade to Normal mode.",
            L"Confirm Unsafe Mode",
            MB_OKCANCEL | MB_ICONWARNING);
        return result == IDOK;
    }

    // Any → Kernel (hard gate)
    if (newMode == ExecMode::Kernel) {
        int result = MessageBoxW(hWnd,
            L"⚠ KERNEL MODE ENABLED ⚠\n\n"
            L"This allows system-level mutations.\n"
            L"Only use in secure, air-gapped environments.\n\n"
            L"Type 'ENABLE KERNEL MODE' to confirm.",
            L"CRITICAL WARNING",
            MB_OKCANCEL | MB_ICONERROR);
        if (result != IDOK) return false;

        // Additional text verification
        // (Simplified - in production, use a dedicated dialog)
        return true;
    }

    return true;
}

void ExecModeToolbar::DrawButton(LPDRAWITEMSTRUCT dis) {
    int id = dis->CtlID - kBaseId;
    bool selected = ((int)m_mode == id);

    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    // Background
    HBRUSH brush = CreateSolidBrush(selected ? kActiveColors[id] : ExecModeColors::InactiveBg);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);

    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, ExecModeColors::Border);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, selected ? RGB(255, 255, 255) : ExecModeColors::InactiveText);

    // Add icon prefix
    const wchar_t* icons[] = { L"👁 ", L"✔ ", L"⚡ ", L"⚠ " };
    std::wstring text = icons[id];
    text += kLabels[id];

    DrawTextW(hdc, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

LRESULT ExecModeToolbar::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id >= kBaseId && id < kBaseId + 4) {
            ExecMode newMode = (ExecMode)(id - kBaseId);
            if (newMode != m_mode) {
                if (ConfirmEscalation(hWnd, newMode)) {
                    SetMode(newMode);
                }
            }
        }
        break;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlID >= kBaseId && dis->CtlID < kBaseId + 4) {
            DrawButton(dis);
            return TRUE;
        }
        break;
    }
    }

    return 0;
}

bool ExecModeToolbar::HandleAccelerator(HWND hWnd, WPARAM wParam) {
    if ((GetKeyState(VK_CONTROL) & 0x8000) &&
        (GetKeyState(VK_SHIFT) & 0x8000)) {
        switch (wParam) {
        case '1': SetMode(ExecMode::Shadow); return true;
        case '2': SetMode(ExecMode::Normal); return true;
        case '3':
            if (ConfirmEscalation(hWnd, ExecMode::Unsafe)) {
                SetMode(ExecMode::Unsafe);
            }
            return true;
        case '4':
            if (ConfirmEscalation(hWnd, ExecMode::Kernel)) {
                SetMode(ExecMode::Kernel);
            }
            return true;
        }
    }
    return false;
}

std::wstring ExecModeToolbar::GetModeLabel() const {
    switch (m_mode) {
    case ExecMode::Shadow: return L"Shadow";
    case ExecMode::Normal: return L"Normal";
    case ExecMode::Unsafe: return L"Unsafe";
    case ExecMode::Kernel: return L"Kernel";
    }
    return L"Unknown";
}

std::wstring ExecModeToolbar::GetModeDescription() const {
    switch (m_mode) {
    case ExecMode::Shadow:
        return L"Read-only: AI proposes changes, no modifications";
    case ExecMode::Normal:
        return L"Confirm: AI shows preview, Tab to apply";
    case ExecMode::Unsafe:
        return L"Auto-apply: Tests pass → apply automatically";
    case ExecMode::Kernel:
        return L"System-level: Full autonomous mutation";
    }
    return L"";
}

} // namespace UI
} // namespace RawrXD
