#include <windows.h>
#include <unknwn.h>
#include <WebView2.h>
#include <iostream>
#include "webview2_bridge.hpp"
#include "rawrxd_ipc_protocol.h"
#include "../agent/agent_self_healing_orchestrator.hpp"

extern "C" {
    // Assembly exports from RawrXD_IPC_Dispatcher.asm
    void __stdcall RawrXD_DispatchIPC(const void* pData, uint32_t size);

    // Export MainLoop as C-callable to avoid mangling issues
    void RawrXD_UIMainLoop() {
        MSG msg;
        // Zero-Touch Loop: Periodically audit the environment's health
        uint64_t lastAudit = GetTickCount64();
        const uint64_t AUDIT_INTERVAL_MS = 15000; // 15s internal scan

        while (true) {
            // Priority: Check for system degradation if IDE is active
            if (GetTickCount64() - lastAudit > AUDIT_INTERVAL_MS) {
                lastAudit = GetTickCount64();
                SelfHealReport report = AgentSelfHealingOrchestrator::instance().runHealingCycle();
                if (report.bugsFixed > 0) {
                    std::cerr << "[ZeroTouch] MainLoop: Recovered " << report.bugsFixed << " stability points." << std::endl;
                }
            }

            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) break;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            } else {
                // Yield to prevent pegging 100% CPU in the non-blocking loop
                Sleep(1);
            }
        }
    }
}

namespace rawrxd::ui {

void MainLoop() {
    RawrXD_UIMainLoop();
}

// Global or static instance to handle WebView2 events
void ProcessWebView2Message(ICoreWebView2* webview, PCWSTR message) {
    // This is where we bridge WebView2 JSON/String messages to our MASM IPC
    // For v14.7.3, we'll assume a simple binary blob or packed string
    // In a real implementation, you'd parse JSON here.
    // For now, we'll pass it to the dispatcher.
    
    // Example: if message starts with "RAW:", pass the rest as binary
    // This keeps the C++ side "zero bloat".
    RawrXD_DispatchIPC(message, (uint32_t)wcslen(message) * sizeof(wchar_t));
}

} // namespace rawrxd::ui
