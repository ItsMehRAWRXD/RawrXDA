// RawrXD_NativeModelBridge.dll - Stub Implementation
// This is a placeholder DLL until the MASM version can be compiled with ml64
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>

extern "C" {

__declspec(dllexport) int LoadModelNative(const char* filepath) {
    OutputDebugStringA("[ModelBridge Stub] LoadModelNative called - not implemented\n");
    return -1;  // Failure
}

__declspec(dllexport) void* GetTokenEmbedding(int tokenId, int layerId) {
    OutputDebugStringA("[ModelBridge Stub] GetTokenEmbedding called - not implemented\n");
    return nullptr;
}

__declspec(dllexport) int ForwardPassASM(void* tokens, int count, void* output) {
    OutputDebugStringA("[ModelBridge Stub] ForwardPassASM called - not implemented\n");
    return -1;  // Failure
}

__declspec(dllexport) void CleanupMathTables() {
    OutputDebugStringA("[ModelBridge Stub] CleanupMathTables called\n");
}

}  // extern "C"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        OutputDebugStringA("[ModelBridge Stub DLL] Loaded (temporary stub)\n");
    }
    return TRUE;
}
