#pragma once
// IDEMainWindow_Migrated.h — Qt-to-Win32 bridge: RawrXD::Window-based IDE shell
// Replaces QMainWindow-derived IDEMainWindow with native Win32 implementation
// No Qt. No exceptions. C++20 only.

#include "RawrXD_Window.h"
#include "ide_orchestrator.h"
#include "ui/FixedDockWidgets.h"
#include "EventBus.h"
#include <memory>
#include <windows.h>
#include <commctrl.h>

class IDEMainWindowNative : public RawrXD::Window {
    IDEOrchestrator* m_orch = nullptr;
    std::unique_ptr<RawrXD::TodoDock> m_todo;
    std::unique_ptr<RawrXD::ObservabilityDashboard> m_obs;
    std::unique_ptr<RawrXD::ModelRouterWidget> m_modelRouter;
    HMENU m_hMenu = nullptr;
    HWND m_hStatus = nullptr;

    // Menu command IDs
    enum MenuCmd : UINT {
        ID_FILE_NEW     = 1001,
        ID_FILE_OPEN    = 1002,
        ID_FILE_SAVE    = 1003,
        ID_FILE_EXIT    = 1004,
        ID_TOOLS_BUILD  = 2001,
        ID_TOOLS_PATCH  = 2002,
        ID_HELP_ABOUT   = 9001
    };

public:
    IDEMainWindowNative() : Window() {
        create(nullptr, "RawrXD IDE", WS_OVERLAPPEDWINDOW);

        // Build menu bar
        m_hMenu = CreateMenu();
        HMENU hFile = CreatePopupMenu();
        AppendMenuA(hFile, MF_STRING, ID_FILE_NEW,  "&New");
        AppendMenuA(hFile, MF_STRING, ID_FILE_OPEN, "&Open");
        AppendMenuA(hFile, MF_STRING, ID_FILE_SAVE, "&Save");
        AppendMenuA(hFile, MF_SEPARATOR, 0, nullptr);
        AppendMenuA(hFile, MF_STRING, ID_FILE_EXIT, "E&xit");
        AppendMenuA(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFile), "&File");

        HMENU hTools = CreatePopupMenu();
        AppendMenuA(hTools, MF_STRING, ID_TOOLS_BUILD, "&Build");
        AppendMenuA(hTools, MF_STRING, ID_TOOLS_PATCH, "&Hotpatch");
        AppendMenuA(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hTools), "&Tools");

        SetMenu(nativeHandle(), m_hMenu);

        // Status bar
        m_hStatus = CreateWindowW(L"msctls_statusbar32", nullptr,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, nativeHandle(), nullptr, nullptr, nullptr);

        // Dock widgets
        m_todo = std::make_unique<RawrXD::TodoDock>(this);
        m_obs = std::make_unique<RawrXD::ObservabilityDashboard>(this);
        m_modelRouter = std::make_unique<RawrXD::ModelRouterWidget>(this);

        // Wire security events to status bar
        RawrXD::EventBus::Get().SecurityAuthRequired.connect([this](bool ok) {
            SetStatus(ok ? "Auth OK" : "Auth Failed");
        });

        // Wire build progress
        RawrXD::EventBus::Get().BuildFinished.connect([this](const std::string& target, bool ok) {
            SetStatus(ok ? ("Build OK: " + target).c_str() : "Build FAILED");
        });
    }

    ~IDEMainWindowNative() {
        delete m_orch;
    }

    void SetStatus(const char* s) {
        SendMessageA(m_hStatus, SB_SETTEXTA, 0, reinterpret_cast<LPARAM>(s));
    }

    void SetOrchestrator(IDEOrchestrator* orch) { m_orch = orch; }
    IDEOrchestrator* GetOrchestrator() { return m_orch; }

    RawrXD::TodoDock* GetTodoDock() { return m_todo.get(); }
    RawrXD::ObservabilityDashboard* GetObsDashboard() { return m_obs.get(); }
    RawrXD::ModelRouterWidget* GetModelRouter() { return m_modelRouter.get(); }

protected:
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override {
        if (msg == WM_COMMAND) {
            switch (LOWORD(wParam)) {
                case ID_FILE_EXIT: PostQuitMessage(0); return 0;
                default: break;
            }
        }
        if (msg == WM_SIZE) {
            // Resize status bar
            SendMessageW(m_hStatus, WM_SIZE, 0, 0);
        }
        return Window::handleMessage(msg, wParam, lParam);
    }
};

// Migration alias: drop-in replacement for old Qt IDEMainWindow
#define IDEMainWindow IDEMainWindowNative
