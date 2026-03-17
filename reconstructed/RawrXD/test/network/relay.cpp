#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

// NetworkRelay DLL function declarations
extern "C" {
    typedef void* (__stdcall* RelayEngine_Init_t)();
    typedef int (__stdcall* RelayEngine_CreateContext_t)(void* pool, SOCKET client, SOCKET target, void* entry);
    typedef void (__stdcall* RelayEngine_RunBiDirectional_t)(int contextId);
    typedef void (__stdcall* RelayEngine_GetStats_t)(void* entry, void* outStats);
}

int main() {
    // Load NetworkRelay.dll
    HMODULE hRelay = LoadLibraryA("NetworkRelay.dll");
    if (!hRelay) {
        std::cerr << "Failed to load NetworkRelay.dll" << std::endl;
        return 1;
    }

    // Get function pointers
    auto RelayEngine_Init = (RelayEngine_Init_t)GetProcAddress(hRelay, "RelayEngine_Init");
    auto RelayEngine_CreateContext = (RelayEngine_CreateContext_t)GetProcAddress(hRelay, "RelayEngine_CreateContext");
    auto RelayEngine_RunBiDirectional = (RelayEngine_RunBiDirectional_t)GetProcAddress(hRelay, "RelayEngine_RunBiDirectional");

    if (!RelayEngine_Init || !RelayEngine_CreateContext || !RelayEngine_RunBiDirectional) {
        std::cerr << "Failed to get function pointers" << std::endl;
        FreeLibrary(hRelay);
        return 1;
    }

    // Initialize relay engine
    void* pool = RelayEngine_Init();
    if (!pool) {
        std::cerr << "Failed to initialize relay engine" << std::endl;
        FreeLibrary(hRelay);
        return 1;
    }

    std::cout << "NetworkRelay initialized successfully!" << std::endl;
    std::cout << "Ready for benchmarking against ngrok/cloudflared" << std::endl;

    // For now, just test initialization
    // TODO: Add full socket relay test

    FreeLibrary(hRelay);
    return 0;
}