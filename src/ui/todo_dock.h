#pragma once
#include "RawrXD_Window.h"
#include "RawrXD_SignalSlot.h"
#include <vector>
#include <string>
#include <functional>

namespace RawrXD {
struct TodoItem {
    std::wstring text;
    bool done;
    int priority;
};
class TodoDock : public Window {
    Signal<const TodoItem&> onTodoToggled;
    Signal<const TodoItem&> onTodoDeleted;
    Signal<const std::wstring&> onTodoAdded;
    std::vector<TodoItem> items;
    HWND hList;
    HWND hAddBtn;
    HWND hEdit;
public:
    TodoDock() : Window(L"RawrXD_TodoDock"), hList(nullptr), hAddBtn(nullptr), hEdit(nullptr) {}
    void Create(HWND parent, const RECT& rect) {
        Window::Create(parent, rect, WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN);
        hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, 
            WS_CHILD|WS_VISIBLE|LBS_NOTIFY|LBS_HASSTRINGS|WS_VSCROLL,
            0, 0, rect.right-rect.left, rect.bottom-rect.top-30, 
            m_hwnd, (HMENU)101, GetModuleHandle(nullptr), nullptr);
        hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", 
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
            0, rect.bottom-rect.top-28, rect.right-rect.left-80, 25,
            m_hwnd, (HMENU)102, GetModuleHandle(nullptr), nullptr);
        hAddBtn = CreateWindowExW(0, L"BUTTON", L"+", 
            WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            rect.right-rect.left-75, rect.bottom-rect.top-28, 75, 25,
            m_hwnd, (HMENU)103, GetModuleHandle(nullptr), nullptr);
        SetFont(hList);
        SetFont(hEdit);
        SetFont(hAddBtn);
    }
    void AddTodo(const std::wstring& text, int prio=0) {
        items.push_back({text, false, prio});
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)text.c_str());
        onTodoAdded.emit(text);
    }
    void ToggleSelected() {
        int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
        if(sel >= 0 && sel < (int)items.size()) {
            items[sel].done = !items[sel].done;
            onTodoToggled.emit(items[sel]);
            InvalidateRect(hList, nullptr, TRUE);
        }
    }
protected:
    LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp) override {
        if(msg == WM_COMMAND && HIWORD(wp) == BN_CLICKED && LOWORD(wp) == 103) {
            wchar_t buf[256];
            GetWindowTextW(hEdit, buf, 256);
            if(wcslen(buf) > 0) { AddTodo(buf); SetWindowTextW(hEdit, L""); }
            return 0;
        }
        if(msg == WM_SIZE) {
            int w = LOWORD(lp), h = HIWORD(lp);
            SetWindowPos(hList, nullptr, 0, 0, w, h-30, SWP_NOZORDER);
            SetWindowPos(hEdit, nullptr, 0, h-28, w-80, 25, SWP_NOZORDER);
            SetWindowPos(hAddBtn, nullptr, w-75, h-28, 75, 25, SWP_NOZORDER);
            return 0;
        }
        return Window::WndProc(msg, wp, lp);
    }
};
} // namespace RawrXD
