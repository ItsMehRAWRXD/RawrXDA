#include <windows.h>
#include "logging/logger.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Logger logger("StubMain", "logs/");
    logger.info("STUB: WinMain entered");
    
    MessageBoxA(nullptr, "Stub main reached!", "SUCCESS", MB_OK);
    
    logger.info("STUB: MessageBox shown");
    
    return 0;
}
