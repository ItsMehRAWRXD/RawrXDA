#include "gui_main_enhanced.h"
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    RawrXD::GUIMainEnhanced gui;
    
    if (!gui.initialize(hInstance)) {
        MessageBox(NULL, TEXT("Failed to initialize RawrXD GUI"), TEXT("Error"), MB_ICONERROR);
        return 1;
    }
    
    return gui.run();
}
