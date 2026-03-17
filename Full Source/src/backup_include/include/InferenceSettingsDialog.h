#pragma once
#include "../RawrXD_Window.h"
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

namespace RawrXD {
struct InferenceConfig {
    int maxTokens = 2048;
    float temperature = 0.7f;
    float topP = 0.9f;
    int threads = 8;
    bool useGPU = true;
    bool useQuantization = true;
};
class InferenceSettingsDialog : public Window {
    Signal<const InferenceConfig&> onConfigChanged;
    InferenceConfig config;
    HWND hMaxTokens;
    HWND hTemperature;
    HWND hTopP;
    HWND hThreads;
    HWND hGPU;
    HWND hQuant;
public:
    InferenceSettingsDialog() : Window(L"RawrXD_InferenceSettings") {}
    void Create(HWND parent) {
        RECT rc; GetClientRect(parent, &rc);
        Window::Create(parent, rc, WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN);
        int y = 10;
        CreateLabel(10, y, L"Max Tokens:"); hMaxTokens = CreateSpinner(120, y, 1, 8192, config.maxTokens); y += 30;
        CreateLabel(10, y, L"Temperature:"); hTemperature = CreateTrackbar(120, y, 0, 100, (int)(config.temperature*100)); y += 30;
        CreateLabel(10, y, L"Top P:"); hTopP = CreateTrackbar(120, y, 0, 100, (int)(config.topP*100)); y += 30;
        CreateLabel(10, y, L"Threads:"); hThreads = CreateSpinner(120, y, 1, 64, config.threads); y += 30;
        hGPU = CreateCheckbox(10, y, L"Use GPU", config.useGPU); y += 25;
        hQuant = CreateCheckbox(10, y, L"Quantization", config.useQuantization);
    }
    InferenceConfig GetConfig() const { return config; }
    void SetConfig(const InferenceConfig& c) { config = c; UpdateControls(); }
protected:
    void CreateLabel(int x, int y, const wchar_t* txt) {
        CreateWindowExW(0, L"STATIC", txt, WS_CHILD|WS_VISIBLE|SS_LEFT, x, y, 100, 20, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    }
    HWND CreateSpinner(int x, int y, int min, int max, int val) {
        HWND h = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(val).c_str(), WS_CHILD|WS_VISIBLE|ES_NUMBER, x, y, 60, 20, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        CreateWindowExW(0, L"UPDOWN_CLASS", nullptr, WS_CHILD|WS_VISIBLE|UDS_AUTOBUDDY|UDS_SETBUDDYINT|UDS_ALIGNRIGHT, x+60, y, 20, 20, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        return h;
    }
    HWND CreateTrackbar(int x, int y, int min, int max, int val) {
        return CreateWindowExW(0, TRACKBAR_CLASS, nullptr, WS_CHILD|WS_VISIBLE|TBS_AUTOTICKS, x, y, 150, 20, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    }
    HWND CreateCheckbox(int x, int y, const wchar_t* txt, bool checked) {
        HWND h = CreateWindowExW(0, L"BUTTON", txt, WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, x, y, 150, 20, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        SendMessageW(h, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
        return h;
    }
    void UpdateControls() {
        SetDlgItemInt(m_hwnd, GetDlgCtrlID(hMaxTokens), config.maxTokens, FALSE);
        SendMessageW(hTemperature, TBM_SETPOS, TRUE, (int)(config.temperature*100));
        SendMessageW(hTopP, TBM_SETPOS, TRUE, (int)(config.topP*100));
    }
    void SaveConfig() {
        config.maxTokens = GetDlgItemInt(m_hwnd, GetDlgCtrlID(hMaxTokens), nullptr, FALSE);
        config.temperature = SendMessageW(hTemperature, TBM_GETPOS, 0, 0) / 100.0f;
        config.topP = SendMessageW(hTopP, TBM_GETPOS, 0, 0) / 100.0f;
        config.threads = GetDlgItemInt(m_hwnd, GetDlgCtrlID(hThreads), nullptr, FALSE);
        config.useGPU = SendMessageW(hGPU, BM_GETCHECK, 0, 0) == BST_CHECKED;
        config.useQuantization = SendMessageW(hQuant, BM_GETCHECK, 0, 0) == BST_CHECKED;
        onConfigChanged.emit(config);
    }
};
} // namespace RawrXD
