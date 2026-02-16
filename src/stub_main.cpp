#include <windows.h>
#include "logging/logger.h"
#include "win32app/Win32IDE.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Logger logger("StubMain", "logs/");
    logger.info("WinMain entered");

    const int exitCode = Win32IDE_Main(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
    logger.info("WinMain exit code {}", exitCode);

    return exitCode;
}
