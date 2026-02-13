#include "ide_window.h"
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    IDEWindow ideWindow;
    
    if (!ideWindow.Initialize(hInstance)) {
        MessageBoxW(nullptr, L"Failed to initialize IDE window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    ideWindow.Run();
    ideWindow.Shutdown();
    
    return 0;
}
