#pragma once
#include "RawrXD_Window.h"
#include "RawrXD_SignalSlot.h"
#include <vector>
#include <string>

namespace RawrXD {
struct ModelInfo {
    std::wstring name;
    std::wstring path;
    size_t params;
    size_t memoryRequired;
};
class ModelRouterWidget : public Window {
    Signal<const ModelInfo&> onModelSelected;
    Signal<> onRefreshRequested;
    HWND hList;
    HWND hRefreshBtn;
    HWND hLoadBtn;
    std::vector<ModelInfo> models;
public:
    ModelRouterWidget() : Window(L"RawrXD_ModelRouter"), hList(nullptr), hRefreshBtn(nullptr), hLoadBtn(nullptr) {}
    void Create(HWND parent, const RECT& rect) {
        Window::Create(parent, rect, WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN);
        hRefreshBtn = CreateWindowExW(0, L"BUTTON", L"Refresh", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 10, 10, 100, 30, m_hwnd, (HMENU)401, GetModuleHandle(nullptr), nullptr);
        hLoadBtn = CreateWindowExW(0, L"BUTTON", L"Load Model", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_DISABLED, 120, 10, 100, 30, m_hwnd, (HMENU)402, GetModuleHandle(nullptr), nullptr);
        hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD|WS_VISIBLE|LBS_NOTIFY|WS_VSCROLL|LBS_HASSTRINGS, 10, 50, rect.right-rect.left-20, rect.bottom-rect.top-60, m_hwnd, (HMENU)403, GetModuleHandle(nullptr), nullptr);
        SetFont(hRefreshBtn); SetFont(hLoadBtn); SetFont(hList);
    }
    void SetModels(const std::vector<ModelInfo>& m) {
        models = m;
        SendMessageW(hList, LB_RESETCONTENT, 0, 0);
        for(const auto& mi : models) {
            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)(mi.name + L" (" + std::to_wstring(mi.params) + L")").c_str());
        }
    }
    void AddModel(const ModelInfo& mi) {
        models.push_back(mi);
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)(mi.name + L" (" + std::to_wstring(mi.params) + L")").c_str());
    }
protected:
    LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp) override {
        if(msg == WM_COMMAND) {
            if(LOWORD(wp) == 401) { onRefreshRequested.emit(); return 0; }
            if(LOWORD(wp) == 402) {
                int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                if(sel >= 0 && sel < (int)models.size()) onModelSelected.emit(models[sel]);
                return 0;
            }
            if(LOWORD(wp) == 403 && HIWORD(wp) == LBN_SELCHANGE) {
                EnableWindow(hLoadBtn, TRUE);
                return 0;
            }
        }
        if(msg == WM_SIZE) {
            int w = LOWORD(lp), h = HIWORD(lp);
            SetWindowPos(hList, nullptr, 10, 50, w-20, h-60, SWP_NOZORDER);
        }
        return Window::WndProc(msg, wp, lp);
    }
};
} // namespace RawrXD
