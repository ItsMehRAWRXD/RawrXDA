#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

class OutlinePanel
{
public:
    OutlinePanel() = default;
    ~OutlinePanel() = default;

    void Create(HWND hParent) {
        if (!hParent) return;
        hWnd_ = CreateWindowExW(
            0, L"LISTBOX", L"Outline",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
            0, 0, 200, 400,
            hParent, nullptr, GetModuleHandle(nullptr), nullptr);
    }
    void UpdateSymbols(const std::vector<std::string>& symbols) {
        if (!hWnd_) return;
        SendMessageW(hWnd_, LB_RESETCONTENT, 0, 0);
        for (const auto& s : symbols) {
            int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
            std::wstring ws(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), len);
            SendMessageW(hWnd_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(ws.c_str()));
        }
    }
    void SetSelectionCallback(std::function<void(int)> cb) {
        onSelect_ = cb;
    }

private:
    HWND hWnd_ = nullptr;
    std::function<void(int)> onSelect_;
};
