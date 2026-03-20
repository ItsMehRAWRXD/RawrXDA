#include "Win32IDE.h"
#include <windows.h>

// Handler for OS Explorer Interceptor feature
void HandleOSExplorerInterceptor(void* idePtr) {
    (void)idePtr;
    MessageBoxA(nullptr,
                "OS Explorer Interceptor is unavailable in this MinGW lane.",
                "OS Explorer Interceptor",
                MB_ICONINFORMATION | MB_OK);
}
