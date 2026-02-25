#pragma once

#include "RawrXD_Window.h"
#include "RawrXD_SignalSlot.h"
#include "todo_manager.h"
#include <vector>
#include <string>

namespace RawrXD {

class TodoDock : public Window {
public:
    // ── Signals ──────────────────────────────────────────────────
    Signal<const std::string&, const std::string&> onFileOpenRequested;
    Signal<const std::string&>                     onTodoAdded;
    Signal<const std::string&>                     onTodoCompleted;
    Signal<const std::string&>                     onTodoRemoved;

    explicit TodoDock(TodoManager* mgr = nullptr)
        : Window(L"RawrXD_TodoDock"), todoManager_(mgr),
          hList_(nullptr), hAddBtn_(nullptr), hEdit_(nullptr),
          hCompleteBtn_(nullptr), hRemoveBtn_(nullptr), hScanBtn_(nullptr) {}

    void Create(HWND parent, const RECT& rect) {
        Window::Create(parent, rect, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);

        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;

        // Toolbar row (top)
        hEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, w - 180, 25, m_hwnd, (HMENU)101, GetModuleHandle(nullptr), nullptr);

        hAddBtn_ = CreateWindowExW(0, L"BUTTON", L"+ Add",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            w - 175, 0, 55, 25, m_hwnd, (HMENU)102, GetModuleHandle(nullptr), nullptr);

        hCompleteBtn_ = CreateWindowExW(0, L"BUTTON", L"\xE2\x9C\x93",  // checkmark
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            w - 115, 0, 35, 25, m_hwnd, (HMENU)103, GetModuleHandle(nullptr), nullptr);

        hRemoveBtn_ = CreateWindowExW(0, L"BUTTON", L"X",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            w - 75, 0, 35, 25, m_hwnd, (HMENU)104, GetModuleHandle(nullptr), nullptr);

        hScanBtn_ = CreateWindowExW(0, L"BUTTON", L"Scan",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            w - 35, 0, 35, 25, m_hwnd, (HMENU)105, GetModuleHandle(nullptr), nullptr);

        // List (fills remaining space)
        hList_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_HASSTRINGS | WS_VSCROLL,
            0, 30, w, h - 30, m_hwnd, (HMENU)106, GetModuleHandle(nullptr), nullptr);

        SetFont(hEdit_);
        SetFont(hAddBtn_);
        SetFont(hCompleteBtn_);
        SetFont(hRemoveBtn_);
        SetFont(hScanBtn_);
        SetFont(hList_);

        loadTodos();
    }

    void refreshTodos() { loadTodos(); }

    void addTodoItem(const std::wstring& text) {
        SendMessageW(hList_, LB_ADDSTRING, 0, (LPARAM)text.c_str());
        std::string narrow(text.begin(), text.end());
        onTodoAdded.emit(narrow);
    }

protected:
    LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp) override {
        if (msg == WM_COMMAND) {
            switch (LOWORD(wp)) {
                case 102: { // Add
                    wchar_t buf[512];
                    GetWindowTextW(hEdit_, buf, 512);
                    if (wcslen(buf) > 0) {
                        addTodoItem(buf);
                        SetWindowTextW(hEdit_, L"");
                    }
                    return 0;
                }
                case 103: { // Complete
                    int sel = (int)SendMessageW(hList_, LB_GETCURSEL, 0, 0);
                    if (sel >= 0) {
                        wchar_t buf[256];
                        SendMessageW(hList_, LB_GETTEXT, sel, (LPARAM)buf);
                        std::string narrow(std::wstring(buf).begin(), std::wstring(buf).end());
                        onTodoCompleted.emit(narrow);
                        SendMessageW(hList_, LB_DELETESTRING, sel, 0);
                    }
                    return 0;
                }
                case 104: { // Remove
                    int sel = (int)SendMessageW(hList_, LB_GETCURSEL, 0, 0);
                    if (sel >= 0) {
                        wchar_t buf[256];
                        SendMessageW(hList_, LB_GETTEXT, sel, (LPARAM)buf);
                        std::string narrow(std::wstring(buf).begin(), std::wstring(buf).end());
                        onTodoRemoved.emit(narrow);
                        SendMessageW(hList_, LB_DELETESTRING, sel, 0);
                    }
                    return 0;
                }
                case 105: // Scan
                    if (todoManager_) { /* todoManager_->scanWorkspace(); */ }
                    return 0;
                case 106:
                    if (HIWORD(wp) == LBN_DBLCLK) {
                        int sel = (int)SendMessageW(hList_, LB_GETCURSEL, 0, 0);
                        if (sel >= 0) {
                            wchar_t buf[256];
                            SendMessageW(hList_, LB_GETTEXT, sel, (LPARAM)buf);
                            std::string narrow(std::wstring(buf).begin(), std::wstring(buf).end());
                            onFileOpenRequested.emit(narrow, narrow);
                        }
                    }
                    return 0;
            }
        }
        if (msg == WM_SIZE) {
            int w = LOWORD(lp), h = HIWORD(lp);
            SetWindowPos(hEdit_, nullptr, 0, 0, w - 180, 25, SWP_NOZORDER);
            SetWindowPos(hAddBtn_, nullptr, w - 175, 0, 55, 25, SWP_NOZORDER);
            SetWindowPos(hCompleteBtn_, nullptr, w - 115, 0, 35, 25, SWP_NOZORDER);
            SetWindowPos(hRemoveBtn_, nullptr, w - 75, 0, 35, 25, SWP_NOZORDER);
            SetWindowPos(hScanBtn_, nullptr, w - 35, 0, 35, 25, SWP_NOZORDER);
            SetWindowPos(hList_, nullptr, 0, 30, w, h - 30, SWP_NOZORDER);
            return 0;
        }
        return Window::WndProc(msg, wp, lp);
    }

private:
    void loadTodos() {
        if (!hList_) return;
        SendMessageW(hList_, LB_RESETCONTENT, 0, 0);
        // If manager available, populate from it
    }

    HWND hList_;
    HWND hAddBtn_;
    HWND hEdit_;
    HWND hCompleteBtn_;
    HWND hRemoveBtn_;
    HWND hScanBtn_;
    TodoManager* todoManager_;
};

} // namespace RawrXD

