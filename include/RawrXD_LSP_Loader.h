#pragma once
#include <stdint.h>

extern "C" {
    typedef struct _LSP_SERVER_INSTANCE {
        void* hProcess;
        void* hStdInWrite;
        void* hStdOutRead;
        uint32_t dwProcessId;
        void* pMessageBuffer;
        uintptr_t Lock;
    } LSP_SERVER_INSTANCE;

    // Handshake context (opaque pointer to LSP_HANDSHAKE_CTX)
    typedef void* LSP_HANDSHAKE_HANDLE;

    // Basic LSP functions
    void  LSP_Engine_Entry();
    void  LSP_Transport_Write(LSP_SERVER_INSTANCE* instance, const char* json_payload);
    void  LSP_Shim_ShowMessage(const char* message, uint32_t type);
    void* LSP_Internal_Dispatch(void* context, void* params);
    void  LSP_Init_Handshake(LSP_SERVER_INSTANCE* instance);
    void  LSP_Handshake_Sequence(LSP_SERVER_INSTANCE* instance);

    // Extended handshake functions
    LSP_HANDSHAKE_HANDLE LSP_Handshake_Create();
    void LSP_Handshake_Destroy(LSP_HANDSHAKE_HANDLE handle);
    int  LSP_Handshake_DetectServer(LSP_HANDSHAKE_HANDLE handle, const char* serverCommand);
    int  LSP_Handshake_CheckTimeout(LSP_HANDSHAKE_HANDLE handle);
}

