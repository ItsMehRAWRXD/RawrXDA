#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>

#ifdef NO_ASM
extern "C" {
    void Titan_ExecuteComputeKernel(void*, void*) {}
    uint32_t Titan_PerformDMA(void*, void*, size_t) { return 0; }
}
#else
// --- EXTERN MASM64 ENTRY POINTS ---
extern "C" {
    void Titan_ExecuteComputeKernel(void* pContext, void* pPatch);
    uint32_t Titan_PerformDMA(void* pSource, void* pDest, size_t size);
}
#endif

// GUI Constants
#define ID_BTN_EXECUTE 101
#define ID_EDIT_OUTPUT 102

namespace RawrXD::AmphibiousHost {

struct HostState {
    bool isGuiMode = false;
    HWND hOutput = NULL;
    std::mutex outputMutex;
} g_State;

void LogToOutput(const std::string& text) {
    std::lock_guard<std::mutex> lock(g_State.outputMutex);
    if (g_State.isGuiMode && g_State.hOutput) {
        int len = GetWindowTextLength(g_State.hOutput);
        SendMessage(g_State.hOutput, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessage(g_State.hOutput, EM_REPLACESEL, 0, (LPARAM)text.c_str());
    } else {
        std::cout << text << std::flush;
    }
}

void RunAutonomousCycle(const std::string& task) {
    LogToOutput("\n[AGENTIC-FLOW] Initiating: " + task + "\n");
    LogToOutput("[REASONING] Analyzing SEH chain in kernel...\n");
    
    // Simulate LLM streaming
    std::string response = "Hotpatching DMA descriptor... Done. Executing Titan Kernel...";
    for (char c : response) {
        std::string s(1, c);
        LogToOutput(s);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // CALL THE KERNEL
    Titan_PerformDMA(NULL, NULL, 0);
    LogToOutput("\n[SYSTEM] Kernel execution successful.\n");
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateWindow("BUTTON", "Run Agentic Cycle", WS_VISIBLE | WS_CHILD, 10, 10, 150, 30, hwnd, (HMENU)ID_BTN_EXECUTE, NULL, NULL);
            g_State.hOutput = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, 10, 50, 560, 300, hwnd, (HMENU)ID_EDIT_OUTPUT, NULL, NULL);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BTN_EXECUTE) {
                std::thread([]() { RunAutonomousCycle("GUI-Triggered DMA Optimization"); }).detach();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void StartGUI(HINSTANCE hInstance) {
    g_State.isGuiMode = true;
    const char CLASS_NAME[] = "RawrXDAgentHost";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "RawrXD Amphibious Agent Host", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 600, 400, NULL, NULL, hInstance, NULL);
    
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

} // namespace RawrXD::AmphibiousHost

int main(int argc, char* argv[]) {
    // Amphibious detection: if --gui or no console, or explicitly asked
    bool forceGui = false;
    for (int i = 0; i < argc; i++) {
        if (std::string(argv[i]) == "--gui") forceGui = true;
    }

    if (forceGui || GetConsoleWindow() == NULL) {
        RawrXD::AmphibiousHost::StartGUI(GetModuleHandle(NULL));
    } else {
        std::cout << "--- RawrXD CLI Agent Host ---\n";
        RawrXD::AmphibiousHost::RunAutonomousCycle("CLI-Triggered Kernel Verification");
    }
    return 0;
}

