#include <windows.h>
extern "C" void Start_Native_UI(HINSTANCE hInstance);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    Start_Native_UI(hInstance);
    return 0;
}
