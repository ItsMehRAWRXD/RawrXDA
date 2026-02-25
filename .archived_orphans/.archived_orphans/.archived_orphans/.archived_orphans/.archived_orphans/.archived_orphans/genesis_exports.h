#pragma once
#include <windows.h>

extern "C" {

/* GenesisP0_VulkanCompute.asm */
int VulkanCompute_Initialize(void* hInst);
int VulkanCompute_Dispatch(unsigned x, unsigned y, unsigned z);
void VulkanCompute_Cleanup(void);

/* GenesisP0_ExtensionHost.asm */
int ExtHost_Initialize(void);
int ExtHost_LoadExtension(const char* path);
void ExtHost_Unload(int idx);
int ExtHost_ExecuteCommand(int idx, const char* cmd);

/* GenesisP0_WhiteScreenGuard.asm */
int WSG_Initialize(HWND hwnd);
int WSG_Tick(void);
void WSG_ForceRedraw(void);
void WSG_Shutdown(void);

/* GenesisP0_SelfHosting.asm */
int SelfHost_CompileGenesis(const char* sourcePath);
int SelfHost_VerifyBootstrap(void);

/* GenesisP0_AiBackendBridge.asm */
int AIBridge_Initialize(const char* host, unsigned short port);
int AIBridge_QueryModel(const char* model, const char* prompt, char* outBuf, unsigned bufSize);
void AIBridge_StreamResponse(void* callback);
void AIBridge_Cleanup(void);

/* GenesisP0_BuildOrchestrator.asm */
int BuildOrc_Init(unsigned maxParallel);
int BuildOrc_AddJob(void* func, void* context);
int BuildOrc_ExecuteParallel(void);
int BuildOrc_WaitAll(unsigned timeoutMs);
void BuildOrc_Shutdown(void);

}
