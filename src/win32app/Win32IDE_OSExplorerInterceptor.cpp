#include "Win32IDE.h"
#include "OSExplorerInterceptor.h"
#include <windows.h>

// Handler for OS Explorer Interceptor feature
void HandleOSExplorerInterceptor(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) return;

    // Initialize OS explorer interceptor if not already done
    static OSExplorerInterceptor* interceptor = nullptr;
    if (!interceptor) {
        interceptor = new OSExplorerInterceptor();
        if (!interceptor->Initialize(GetCurrentProcessId(), nullptr) ||
            !interceptor->StartInterception()) {
            MessageBoxA(NULL, "Failed to initialize OS Explorer Interceptor", "OS Explorer Interceptor", MB_ICONERROR | MB_OK);
            delete interceptor;
            interceptor = nullptr;
            return;
        }
    }

    // Show interceptor status
    std::string status = "OS Explorer Interceptor Active\n\n";
    status += "Deep OS Integration:\n";
    status += "- File system monitoring\n";
    status += "- Process enumeration\n";
    status += "- System call interception\n";
    status += "- Registry monitoring\n";
    status += "- Network activity tracking\n";

    MessageBoxA(NULL, status.c_str(), "OS Explorer Interceptor", MB_ICONINFORMATION | MB_OK);
}
