#include <windows.h>
#include <cstdio>
#include <cstdlib>

// Forward declarations for IDE subsystems
extern "C" int RawrXD_IDE_Main(HINSTANCE hInstance, int nCmdShow);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    
    // Attach console for debug output if launched from terminal
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
    }
    
    fprintf(stderr, "[RawrXD] WinMain entry - initializing IDE subsystems\\n");
    
    // Attempt to launch the full IDE; fall back to diagnostic MessageBox
    int result = RawrXD_IDE_Main(hInstance, nCmdShow);
    if (result != 0) {
        MessageBoxA(nullptr,
            "RawrXD IDE exited with an error.\\nCheck stderr or log output for details.",
            "RawrXD IDE", MB_OK | MB_ICONWARNING);
    }
    
    return result;
}
