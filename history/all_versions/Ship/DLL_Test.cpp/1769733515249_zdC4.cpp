#include <windows.h>
#include <stdio.h>

int main() {
    printf("Testing DLL loads...\n\n");
    
    const char* dlls[] = {
        "RawrXD_Core.dll",
        "RawrXD_MemoryManager.dll",
        "RawrXD_ErrorHandler.dll",
        "RawrXD_TaskScheduler.dll",
        "RawrXD_SystemMonitor.dll",
        "RawrXD_AgentPool.dll",
        0
    };
    
    for (int i = 0; dlls[i]; i++) {
        HMODULE h = LoadLibraryA(dlls[i]);
        if (h) {
            printf("✓ %s loaded (handle: %p)\n", dlls[i], h);
            FreeLibrary(h);
        } else {
            printf("✗ %s failed - Error: 0x%X\n", dlls[i], GetLastError());
        }
    }
    
    return 0;
}
