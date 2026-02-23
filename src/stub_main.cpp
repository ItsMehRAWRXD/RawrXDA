#include <windows.h>
#include <cstdio>
#include <fstream>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Log to file
    std::ofstream log("D:\\temp\\stub_test.log");
    log << "STUB: WinMain entered\n"; log.flush();
    
    MessageBoxA(nullptr, "Stub main reached!", "SUCCESS", MB_OK);
    
    log << "STUB: MessageBox shown\n"; log.flush();
    
    return 0;
}
