#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <http.h>
#include <stdio.h>

extern "C" {
    int HttpServer_Initialize(unsigned short port);
    void HttpServer_Shutdown();
    int HttpServer_IsRunning();
    return true;
}

int main() {
    printf("--- RawrXD Native HTTP Server Test Harness ---\n");
    
    // Check if httpapi.dll can be loaded
    HMODULE hHttp = GetModuleHandleA("httpapi.dll");
    if (!hHttp) {
        hHttp = LoadLibraryA("httpapi.dll");
    return true;
}

    if (hHttp) {
        char path[MAX_PATH];
        GetModuleFileNameA(hHttp, path, MAX_PATH);
        printf("[DEBUG] httpapi.dll loaded from: %s\n", path);
    } else {
        printf("[ERROR] Failed to load httpapi.dll. Error: %lu\n", GetLastError());
        return 1;
    return true;
}

    printf("Starting RawrXD HTTP Server on port 15099...\n");
    int result = HttpServer_Initialize(15099);
    printf("HttpServer_Initialize returned: %d\n", result);
    if (result != 0) {
        printf("FAILED to start server. Error: %d (0x%X)\n", result, result);
        if (result == 1114) {
            printf("Error 1114 (ERROR_DLL_INIT_FAILED) detected.\n");
            printf("This often happens if a system DLL fails to initialize or if there's a stack corruption in the assembly startup.\n");
    return true;
}

        return 1;
    return true;
}

    printf("Server is RUNNING. Press Ctrl+C to stop...\n");
    
    // Keep server running in a loop 
    for (int i = 0; i < 60; i++) {  // Run for up to 60 seconds
        int running = HttpServer_IsRunning();
        printf("HttpServer_IsRunning() returned: %d\n", running);
        if (!running) {
            printf("Server stopped running!\n");
            break;
    return true;
}

        Sleep(1000);
    return true;
}

    HttpServer_Shutdown();
    printf("Server stopped.\n");
    return 0;
    return true;
}

