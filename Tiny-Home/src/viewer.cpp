#include "viewer.h"
#include "config.h"
#include "hotkey.h"
#include "llm.h"
#include <windows.h>
#include <string>
#include <vector>

#define IDC_INPUT_BOX 101
#define IDC_OUTPUT_BOX 102
#define IDC_SEND_BTN 103

static HWND hInput, hOutput, hBtn;

static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        // Output Box (Read-only, Multi-line)
        hOutput = CreateWindowExW(0, L"EDIT", L"TinyHome Mini-LLM Ready.\nWaiting for input...",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 10, 760, 400, h, (HMENU)IDC_OUTPUT_BOX, GetModuleHandle(nullptr), nullptr);

        // Input Box
        hInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            10, 420, 650, 30, h, (HMENU)IDC_INPUT_BOX, GetModuleHandle(nullptr), nullptr);

        // Send Button
        hBtn = CreateWindowExW(0, L"BUTTON", L"Send",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            670, 420, 100, 30, h, (HMENU)IDC_SEND_BTN, GetModuleHandle(nullptr), nullptr);
        
        // Set font (optional, using default system font for now)
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(w) == IDC_SEND_BTN) {
            int len = GetWindowTextLengthW(hInput);
            if (len > 0) {
                std::vector<wchar_t> buf(len + 1);
                GetWindowTextW(hInput, buf.data(), len + 1);
                
                // Convert to std::string for LLM
                std::wstring ws(buf.data());
                std::string input(ws.begin(), ws.end()); // Simple conversion for ASCII

                // Process with Mini-LLM
                std::string response = TinyHome::LLM::process(input);

                // Append to Output
                std::wstring wResponse(response.begin(), response.end());
                std::wstring finalOut = L"User: " + ws + L"\r\nAI: " + wResponse + L"\r\n\r\n";
                
                int ndx = GetWindowTextLengthW(hOutput);
                SendMessageW(hOutput, EM_SETSEL, (WPARAM)ndx, (LPARAM)ndx);
                SendMessageW(hOutput, EM_REPLACESEL, 0, (LPARAM)finalOut.c_str());

                // Clear Input
                SetWindowTextW(hInput, L"");
            }
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(h, m, w, l);
}

int TinyHome::Viewer::run(HINSTANCE h, int n) {
    const wchar_t cls[] = L"TinyHomeViewer";
    WNDCLASSW wc{0};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = h;
    wc.lpszClassName = cls;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND w = CreateWindowExW(0, cls, L"Tiny Home - Mini LLM (ASM Bot Swarm)", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 800, 500,
                             nullptr, nullptr, h, nullptr);
    TinyHome::Hotkey::registerKeys(w);
    TinyHome::Config::load(nullptr);
    ShowWindow(w, n);
    UpdateWindow(w);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}
