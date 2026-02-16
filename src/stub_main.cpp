// CONSOLIDATED: stub_main.cpp redirects to real main implementation
#include "win32app/Win32IDE.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return Win32IDE_Main(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}
