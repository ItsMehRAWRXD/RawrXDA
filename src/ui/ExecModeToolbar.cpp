#include "ExecModeToolbar.h"
#include <string>

namespace RawrXD::UI {

static const wchar_t* kLabels[] = {
    L"Shadow",
    L"Normal", 
    L"Unsafe",
    L"Kernel"
};

static COLORREF kColors[] = {
    RGB(160, 160, 160),  // Shadow (gray)
    RGB(60, 140, 255),   // Normal (blue)
    RGB(255, 140, 0),    // Unsafe (orange)
    RGB(220, 50, 50)     // Kernel (red)
};

static ExecModeToolbar* g_instance = nullptr;

ExecModeToolbar::ExecModeToolbar() {
    if (!g_instance) {
        g_instance = this;
    }
}

ExecModeToolbar::~ExecModeToolbar() {
    Destroy();
    if (g_instance == this) {
        g_instance = nullptr;
    }
}

ExecModeToolbar* ExecModeToolbar::GetInstance() {
    return g_instance;
}

bool ExecModeToolbar::Create(HWND hParent, int x, int y) {
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
            (HMENU)(1000 + i),
            GetModuleHandle(nullptr),
            nullptr
        );
    }

    return true;
}

void ExecModeToolbar::Destroy() {
    for (int i = 0; i < 4; ++i) {
        if (m_buttons[i]) {
            DestroyWindow(m_buttons[i]);
            m_buttons[i] = nullptr;
        }
    }
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

void ExecModeToolbar::DrawButton(LPDRAWITEMSTRUCT dis) {
    int id = dis->CtlID - 1000;
    bool selected = ((int)m_mode == id);

    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    // Background
    HBRUSH brush = CreateSolidBrush(selected ? kColors[id] : RGB(40, 40, 40));
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);

    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, selected ? RGB(255, 255, 255) : RGB(180, 180, 180));

    DrawTextW(hdc, kLabels[id], -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

LRESULT ExecModeToolbar::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id >= 1000 && id < 1004) {
            ExecMode newMode = (ExecMode)(id - 1000);

            // Prevent accidental escalation
            if (m_mode == ExecMode::Normal && newMode == ExecMode::Unsafe) {
                if (MessageBoxW(hWnd,
                    L"Enable Unsafe Mode?\nChanges will auto-apply without confirmation.",
                    L"Confirm Escalation",
                    MB_OKCANCEL | MB_ICONWARNING) != IDOK)
                    return 0;
            }

            if (newMode == ExecMode::Kernel) {
                if (MessageBoxW(hWnd,
                    L"Kernel Mode enables system-level mutation.\nThis is dangerous and irreversible.\n\nContinue?",
                    L"Critical Warning",
                    MB_OKCANCEL | MB_ICONERROR) != IDOK)
                    return 0;
            }

            SetMode(newMode);
        }
        break;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlID >= 1000 && dis->CtlID < 1004) {
            DrawButton(dis);
            return TRUE;
        }
        break;
    }

    case WM_KEYDOWN: {
        if (GetKeyState(VK_CONTROL) & 0x8000 &&
            GetKeyState(VK_SHIFT) & 0x8000) {
            switch (wParam) {
            case '1': SetMode(ExecMode::Shadow); break;
            case '2': SetMode(ExecMode::Normal); break;
            case '3': SetMode(ExecMode::Unsafe); break;
            case '4': SetMode(ExecMode::Kernel); break;
            }
        }
        break;
    }
    }

    return 0;
}

// Global accessors
ExecMode GetExecMode() {
    if (g_instance) {
        return g_instance->GetMode();
    }
    return ExecMode::Normal;
}

void SetExecMode(ExecMode mode) {
    if (g_instance) {
        g_instance->SetMode(mode);
    }
}

} // namespace RawrXD::UI
