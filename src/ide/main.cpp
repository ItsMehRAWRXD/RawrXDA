#include "win32_ide.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
    try {
        Win32IDE ide(hInstance);
        if (!ide.Create(L"RawrXD Win32 IDE", 800, 600)) {
            return 0;
        }
        return ide.Run();
    } catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    return 0;
}
