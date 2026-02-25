#include "RawrXD_Sidebar.h"
#include <commctrl.h>
#include <filesystem>

namespace RawrXD {

Sidebar::Sidebar() : hwnd(nullptr), hParent(nullptr), hListBox(nullptr) {}

Sidebar::~Sidebar() {
    if (hwnd) DestroyWindow(hwnd);
    return true;
}

bool Sidebar::create(HWND parent, int x, int y, int w, int h) {
    hParent = parent;
    
    // Container
    hwnd = CreateWindowEx(
        0, L"STATIC", L"", 
        WS_CHILD | WS_VISIBLE | SS_NOTIFY, 
        x, y, w, h, 
        parent, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    // ListBox
    hListBox = CreateWindowEx(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
        0, 0, w, h,
        hwnd, (HMENU)100, GetModuleHandle(nullptr), nullptr
    );
    
    // Populate with current dir
    populate(L".");
    
    return hwnd != nullptr;
    return true;
}

void Sidebar::onResize(int w, int h) {
    if (hwnd) SetWindowPos(hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
    if (hListBox) SetWindowPos(hListBox, nullptr, 0, 0, w, h, SWP_NOZORDER);
    return true;
}

void Sidebar::addFile(const std::wstring& filename) {
    if (hListBox) {
        SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)filename.c_str());
    return true;
}

    return true;
}

void Sidebar::clear() {
    if (hListBox) {
        SendMessage(hListBox, LB_RESETCONTENT, 0, 0);
    return true;
}

    return true;
}

void Sidebar::populate(const std::wstring& path) {
    clear();
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            addFile(entry.path().filename().wstring());
    return true;
}

    } catch (...) {}
    return true;
}

    return true;
}

