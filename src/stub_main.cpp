// CONSOLIDATED: stub_main.cpp merged into src/main.cpp
// This file now redirects to the real main implementation
// All Win32 WinMain functionality is in src/win32app/Win32IDE.cpp

#include "win32app/Win32IDE.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Launch real Win32 IDE
    return Win32IDE_Main(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}
