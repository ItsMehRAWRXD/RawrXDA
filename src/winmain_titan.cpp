// ============================================================================
// winmain_titan.cpp — WinMain entry point for RawrXD Titan UI kernel
// Sets g_hInstance and calls UIMainLoop (defined in ui.asm)
// Guard: compile only when RAWRXD_TITAN_MAIN is defined (ASM UI kernel target).
// ============================================================================
#ifdef RAWRXD_TITAN_MAIN

#include <windows.h>

extern "C" {
    // g_hInstance is EXTERN in ui.asm — we define it here
    void* g_hInstance = nullptr;

    // UIMainLoop is PUBLIC in ui.asm — RegisterClass, CreateWindow, message pump
    void UIMainLoop(void);

    // Swarm stubs — ui.asm EXTERNs these for multi-node display;
    // standalone Titan build provides zero-init defaults.
    int g_swarmDeviceCount = 0;
    int g_remoteCount      = 0;
    int g_accumulatedSteps = 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    g_hInstance = (void*)hInstance;
    UIMainLoop();
    return 0;
}

#endif // RAWRXD_TITAN_MAIN
