#pragma once
// FixedDockWidgets.h — Win32 dock widgets rebased onto RawrXD::Window
// Replaces 5 broken Qt-void classes with proper vtable inheritance
// No Qt. No exceptions. C++20 only.

#include "../RawrXD_Window.h"
#include "../RawrXD_SignalSlot.h"
#include <vector>
#include <string>

namespace RawrXD {

// ---------------------------------------------------------------------------
// TodoDock — Task tracking panel with checkbox list
// ---------------------------------------------------------------------------
struct TodoItem {
    std::string text;
    bool done = false;
};

class TodoDock : public Window {
    Signal<const TodoItem&> itemToggled;
    std::vector<TodoItem> items;
    HWND hList = nullptr;

public:
    explicit TodoDock(Window* parent) : Window(parent) {
        create(parent, "Todo", WS_CHILD | WS_VISIBLE);
        hList = CreateWindowW(L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
            0, 0, 300, 400, nativeHandle(), nullptr, nullptr, nullptr);
        itemToggled.connect([this](const TodoItem&) { Refresh(); });
    }

    void AddItem(const std::string& text) {
        items.push_back({text, false});
        SendMessageA(hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }

    void ToggleItem(int index) {
        if (index >= 0 && index < static_cast<int>(items.size())) {
            items[index].done = !items[index].done;
            itemToggled.emit(items[index]);
        }
    }

    void Refresh() { InvalidateRect(nativeHandle(), nullptr, TRUE); }

    const std::vector<TodoItem>& GetItems() const { return items; }
    auto& OnItemToggled() { return itemToggled; }

protected:
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override {
        if (msg == WM_COMMAND && HIWORD(wParam) == LBN_DBLCLK) {
            int sel = static_cast<int>(SendMessageA(hList, LB_GETCURSEL, 0, 0));
            if (sel != LB_ERR) ToggleItem(sel);
            return 0;
        }
        return Window::handleMessage(msg, wParam, lParam);
    }
};

// ---------------------------------------------------------------------------
// ObservabilityDashboard — Profiler/metrics display panel
// ---------------------------------------------------------------------------
class ObservabilityDashboard : public Window {
    Signal<> metricsUpdated;
    void* profilerData = nullptr;

public:
    explicit ObservabilityDashboard(Window* parent) : Window(parent) {
        create(parent, "Observability", WS_CHILD | WS_VISIBLE);
    }

    void AttachProfiler(void* p) {
        profilerData = p;
        metricsUpdated.emit();
    }

    void* GetProfilerData() const { return profilerData; }
    auto& OnMetricsUpdated() { return metricsUpdated; }
};

// ---------------------------------------------------------------------------
// TrainingDialog — Model fine-tuning dialog with progress bar
// ---------------------------------------------------------------------------
class TrainingDialog : public Window {
    Signal<float> epochComplete;
    HWND hProgress = nullptr;

public:
    explicit TrainingDialog(Window* parent) : Window(parent) {
        create(parent, "Training", WS_OVERLAPPEDWINDOW);
        hProgress = CreateWindowW(L"msctls_progress32", nullptr,
            WS_CHILD | WS_VISIBLE, 10, 10, 280, 30,
            nativeHandle(), nullptr, nullptr, nullptr);
        SendMessageW(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    }

    void SetProgress(int pct) {
        SendMessageW(hProgress, PBM_SETPOS, pct, 0);
    }

    void NotifyEpochComplete(float loss) {
        epochComplete.emit(loss);
    }

    auto& OnEpochComplete() { return epochComplete; }
};

// ---------------------------------------------------------------------------
// TrainingProgressDock — Inline training status panel
// ---------------------------------------------------------------------------
class TrainingProgressDock : public Window {
    HWND hStatus = nullptr;

public:
    explicit TrainingProgressDock(Window* parent) : Window(parent) {
        create(parent, "Progress", WS_CHILD | WS_VISIBLE);
        hStatus = CreateWindowW(L"STATIC", L"Idle",
            WS_CHILD | WS_VISIBLE, 4, 4, 280, 24,
            nativeHandle(), nullptr, nullptr, nullptr);
    }

    void SetStatus(const char* s) {
        SetWindowTextA(hStatus, s);
    }
};

// ---------------------------------------------------------------------------
// ModelRouterWidget — Model selection dropdown
// ---------------------------------------------------------------------------
class ModelRouterWidget : public Window {
    Signal<const std::string&> modelSelected;
    HWND hCombo = nullptr;

public:
    explicit ModelRouterWidget(Window* parent) : Window(parent) {
        create(parent, "Router", WS_CHILD | WS_VISIBLE);
        hCombo = CreateWindowW(L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            0, 0, 300, 200, nativeHandle(), nullptr, nullptr, nullptr);
    }

    void AddModel(const std::string& name) {
        SendMessageA(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
    }

    auto& OnModelSelected() { return modelSelected; }

protected:
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override {
        if (msg == WM_COMMAND && HIWORD(wParam) == CBN_SELCHANGE) {
            int sel = static_cast<int>(SendMessageA(hCombo, CB_GETCURSEL, 0, 0));
            if (sel != CB_ERR) {
                char buf[256] = {};
                SendMessageA(hCombo, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(buf));
                modelSelected.emit(std::string(buf));
            }
            return 0;
        }
        return Window::handleMessage(msg, wParam, lParam);
    }
};

} // namespace RawrXD
