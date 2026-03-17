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

    void  LSP_Engine_Entry();
    void  LSP_Transport_Write(LSP_SERVER_INSTANCE* instance, const char* json_payload);
    void  LSP_Shim_ShowMessage(const char* message, uint32_t type);
    void* LSP_Internal_Dispatch(void* context, void* params);
    void  LSP_Init_Handshake(LSP_SERVER_INSTANCE* instance);
    void  LSP_Handshake_Sequence(LSP_SERVER_INSTANCE* instance);
}
