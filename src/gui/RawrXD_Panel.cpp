#include "RawrXD_Panel.h"

namespace RawrXD {

Panel::Panel() : hwnd(nullptr), hParent(nullptr), hEdit(nullptr) {}

Panel::~Panel() {
    if (hwnd) DestroyWindow(hwnd);
}

bool Panel::create(HWND parent, int x, int y, int w, int h) {
    hParent = parent;
    
    hwnd = CreateWindowEx(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE,
        x, y, w, h,
        parent, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    hEdit = CreateWindowEx(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 0, w, h,
        hwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    // Set font to monospace if possible (skip for brevity, defaults to sys font)
    
    return hwnd != nullptr;
}

void Panel::onResize(int w, int h) {
    if (hwnd) SetWindowPos(hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
    if (hEdit) SetWindowPos(hEdit, nullptr, 0, 0, w, h, SWP_NOZORDER);
}

void Panel::log(const std::wstring& msg) {
    if (hEdit) {
        int len = GetWindowTextLength(hEdit);
        SendMessage(hEdit, EM_SETSEL, len, len);
        SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM)msg.c_str());
        SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    }
}

void Panel::clear() {
    if (hEdit) SetWindowText(hEdit, L"");
}

}
