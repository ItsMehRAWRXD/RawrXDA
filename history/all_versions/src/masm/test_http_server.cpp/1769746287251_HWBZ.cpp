#include <windows.h>
#include <stdio.h>

extern "C" {
    int HttpServer_Initialize(unsigned short port);
    void HttpServer_Shutdown();
    int HttpServer_IsRunning();
}

int main() {
    printf("Starting RawrXD HTTP Server on port 23959...\n");
    
    int result = HttpServer_Initialize(23959);
    if (result != 0) {
        printf("FAILED to start server. Error: %d (0x%X)\n", result, result);
        printf("Common errors:\n");
        printf("  5 (0x5)   = Access Denied - Run as Admin or reserve URL\n");
        printf("  87 (0x57) = Invalid Parameter - Wrong HTTP version struct\n");
        printf("  183 (0xB7) = Already Exists - Port in use\n");
        return 1;
    }
    
    printf("Server running! Press Enter to stop...\n");
    getchar();
    
    HttpServer_Shutdown();
    printf("Server stopped.\n");
    return 0;
}
