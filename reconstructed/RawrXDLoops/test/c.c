#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    MessageBoxA(NULL, "Hello from C!", "Test", MB_ICONINFORMATION);
    ExitProcess(42);
    return 0;
}
