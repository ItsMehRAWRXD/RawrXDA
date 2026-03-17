#include <windows.h>
#include <iostream>
#include "webview2_bridge.hpp"
#include "../agent/agent_self_healing_orchestrator.hpp"

// External ASM entry point from rawrxd_ui_dispatcher.asm
// Note: MASM symbols are usually case-sensitive/normalized via .CODE naming
extern "C" void rawrxd_run_ui_loop();
extern "C" LRESULT CALLBACK rawrxd_ui_wndproc(HWND, UINT, WPARAM, LPARAM);

const wchar_t CLASS_NAME[] = L"RawrXDZeroBloatIDE";

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    // Register window class (no Qt QApplication)
    WNDCLASS wc = {};
    wc.lpfnWndProc = (WNDPROC)rawrxd_ui_wndproc; // Fix: Case-matched ASM symbol
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    // Create main IDE window
    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"RawrXD v14.6 Zero Bloat",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1600, 900,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) {
        return 0;
    }

    // Initialize WebView2 bridge (replaces QWebEngineView)
    // Singleton access or instance based on architecture
    auto& bridge = rawrxd::ui::WebView2Bridge::getInstance();
    if (!bridge.initialize(hwnd)) {
        // Zero-Touch Repair: Attempt to audit and fix WebView2 bridge if it fails
        std::cerr << "[ZeroTouch] WebView2 init failed. Scanning for UI degradation..." << std::endl;
        AgentSelfHealingOrchestrator::instance().runHealingCycle();
        
        MessageBox(nullptr, L"WebView2 init failed. Ensure Evergreen Runtime is installed.", L"RawrXD Engine Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Run native message loop (replaces QApplication::exec())
    // This calls into rawrxd_run_ui_loop() in ASM (src/asm/rawrxd_ui_dispatcher.asm)
    rawrxd_run_ui_loop();

    bridge.shutdown();
    return 0;
}
