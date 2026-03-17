#include "DisasmBridge.cpp"
#include "ModuleBridge.cpp"
#include "SymbolResolver.cpp"
#include "debugger_core.hpp"
#include "webview2_bridge.hpp"
#include <cstdio>
#include <cstring>
#include <shellapi.h>
#include <windows.h>


#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

namespace rawrxd::ui
{
void MainLoop();
}

// ============================================================================
// CLI Argument Parser — intercepts --help / --version before GUI init
// Allocates a console for output when running in SUBSYSTEM:WINDOWS mode
// ============================================================================
static bool handleCLI(LPWSTR cmdLine)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 2)
    {
        LocalFree(argv);
        return false;
    }

    for (int i = 1; i < argc; ++i)
    {
        if (wcscmp(argv[i], L"--version") == 0)
        {
            AttachConsole(ATTACH_PARENT_PROCESS);
            FILE* fout = nullptr;
            freopen_s(&fout, "CONOUT$", "w", stdout);
            printf("RawrXD v15.0-GOLD\n");
            printf("TITAN 800B Distributed Inference Engine\n");
            printf("Agentic Bridge: ACTIVE\n");
            printf("Build: %s %s\n", __DATE__, __TIME__);
            printf("Subsystem: Win32 (Zero Bloat)\n");
            if (fout)
                fclose(fout);
            LocalFree(argv);
            return true;
        }
        if (wcscmp(argv[i], L"--help") == 0)
        {
            AttachConsole(ATTACH_PARENT_PROCESS);
            FILE* fout = nullptr;
            freopen_s(&fout, "CONOUT$", "w", stdout);
            printf("RawrXD v15.0-GOLD — Sovereign AI IDE\n\n");
            printf("Usage: rawrxd.exe [OPTIONS]\n\n");
            printf("Options:\n");
            printf("  --version           Show version and exit\n");
            printf("  --help              Show this help and exit\n");
            printf("  --ide               Launch IDE (default)\n");
            printf("  --mode=local        Local inference mode\n");
            printf("  --model=<path>      Path to .gguf model file\n");
            printf("  --prompt=<text>     Prompt for local inference\n");
            printf("  --ollama-test       Test Ollama bridge on :11434\n");
            printf("  --verbose           Enable verbose logging\n");
            if (fout)
                fclose(fout);
            LocalFree(argv);
            return true;
        }
    }
    LocalFree(argv);
    return false;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR cmdLine, int nCmdShow)
{
    // ---- CLI gate: handle --help / --version before any GUI ----
    if (handleCLI(cmdLine))
        return 0;

    // ---- GUI path: register window class and launch IDE ----
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInst;
    wc.lpszClassName = L"RawrXD_v15_0";
    wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"RawrXD_v15_0", L"RawrXD v15.0-GOLD | Sovereign AI IDE", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1600, 900, nullptr, nullptr, hInst, nullptr);

    if (!hwnd)
        return 1;

    // Always show window visibly; ignore launcher nCmdShow when hidden/minimized so the IDE
    // never opens minimized or disappears (e.g. shortcut "Run minimized").
    int show = nCmdShow;
    if (show == SW_HIDE || show == SW_SHOWMINIMIZED || show == SW_MINIMIZE || show == SW_SHOWMINNOACTIVE ||
        show == SW_SHOWNA)
        show = SW_SHOWNORMAL;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    auto& bridge = rawrxd::ui::WebView2Bridge::getInstance();
    if (bridge.initialize(hwnd))
    {
        rawrxd::ui::MainLoop();
    }

    return 0;
}
