#pragma once
#include "RawrXD_Window.h"
#include "RawrXD_SignalSlot.h"
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

namespace RawrXD {
enum class TrainingState { Idle, Loading, Training, Saving, Error };
class TrainingDialog : public Window {
    Signal<> onStartTraining;
    Signal<> onStopTraining;
    Signal<> onSaveModel;
    HWND hProgress;
    HWND hStatus;
    HWND hStartBtn;
    HWND hStopBtn;
    HWND hSaveBtn;
    TrainingState state;
    float progressPercent;
public:
    TrainingDialog() : Window(L"RawrXD_TrainingDlg"), hProgress(nullptr), hStatus(nullptr), hStartBtn(nullptr), hStopBtn(nullptr), hSaveBtn(nullptr), state(TrainingState::Idle), progressPercent(0.0f) {}
    void Create(HWND parent, const RECT& rect) {
        Window::Create(parent, rect, WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN);
        hStartBtn = CreateWindowExW(0, L"BUTTON", L"Start", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 10, 10, 80, 30, m_hwnd, (HMENU)301, GetModuleHandle(nullptr), nullptr);
        hStopBtn = CreateWindowExW(0, L"BUTTON", L"Stop", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_DISABLED, 100, 10, 80, 30, m_hwnd, (HMENU)302, GetModuleHandle(nullptr), nullptr);
        hSaveBtn = CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_DISABLED, 190, 10, 80, 30, m_hwnd, (HMENU)303, GetModuleHandle(nullptr), nullptr);
        hProgress = CreateWindowExW(0, PROGRESS_CLASS, nullptr, WS_CHILD|WS_VISIBLE|PBS_SMOOTH, 10, 50, rect.right-rect.left-20, 20, m_hwnd, (HMENU)304, GetModuleHandle(nullptr), nullptr);
        hStatus = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Ready", WS_CHILD|WS_VISIBLE|SS_LEFT, 10, 80, rect.right-rect.left-20, 60, m_hwnd, (HMENU)305, GetModuleHandle(nullptr), nullptr);
        SendMessageW(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SetFont(hStartBtn); SetFont(hStopBtn); SetFont(hSaveBtn); SetFont(hStatus);
    }
    void SetProgress(float pct) {
        progressPercent = pct;
        SendMessageW(hProgress, PBM_SETPOS, (WPARAM)(int)pct, 0);
        if(pct >= 100.0f) { state = TrainingState::Idle; UpdateUI(); }
    }
    void SetStatus(const std::wstring& txt) { SetWindowTextW(hStatus, txt.c_str()); }
    void SetState(TrainingState s) { state = s; UpdateUI(); }
protected:
    void UpdateUI() {
        EnableWindow(hStartBtn, state == TrainingState::Idle);
        EnableWindow(hStopBtn, state == TrainingState::Training);
        EnableWindow(hSaveBtn, state == TrainingState::Idle && progressPercent > 0);
    }
    LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp) override {
        if(msg == WM_COMMAND) {
            switch(LOWORD(wp)) {
                case 301: state = TrainingState::Training; onStartTraining.emit(); UpdateUI(); return 0;
                case 302: state = TrainingState::Idle; onStopTraining.emit(); UpdateUI(); return 0;
                case 303: onSaveModel.emit(); return 0;
            }
        }
        if(msg == WM_SIZE) {
            int w = LOWORD(lp);
            SetWindowPos(hProgress, nullptr, 10, 50, w-20, 20, SWP_NOZORDER);
            SetWindowPos(hStatus, nullptr, 10, 80, w-20, 60, SWP_NOZORDER);
        }
        return Window::WndProc(msg, wp, lp);
    }
};
} // namespace RawrXD
