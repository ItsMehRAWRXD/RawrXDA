/**
 * @file main_gui.cpp
 * @brief RawrXD Digestion Engine GUI — pure C++20/Win32 entry point (zero Qt).
 *
 * Standalone Win32 application that hosts DigestionGuiWidget.
 */
#include "digestion_gui_widget.h"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

static int runMessageLoop() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS };
    if (!InitCommonControlsEx(&icc))
        return 1;

    DigestionGuiWidget widget(nullptr);
    widget.setRootDirectory(".");  // optional: parse argv for path
    widget.show();

    return runMessageLoop();
}
