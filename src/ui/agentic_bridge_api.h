#pragma once

#include <cstdint>

#ifdef __cplusplus
#include "rawrxd_ipc_protocol.h"
extern "C" {
#endif

// Disasm/Symbol/Module handlers (implemented in agentic_bridge.cpp)
void RawrXD_Disasm_HandleReq(void* req);
void RawrXD_Symbol_HandleReq(void* req);
void RawrXD_Module_HandleReq(void* req);

// Result getters for bridge to send DATA_DISASM / DATA_SYMBOL / MOD_LOAD to UI
#ifdef __cplusplus
const rawrxd::ipc::MsgDisasmChunk* RawrXD_Disasm_GetLastResult(uint32_t* outCount);
#else
const void* RawrXD_Disasm_GetLastResult(uint32_t* outCount);
#endif
uint64_t RawrXD_Symbol_GetLastResult(void);
uint32_t RawrXD_Module_GetLastResult(const void** outEntries);

// Debugger watch (implemented in agentic_bridge.cpp)
void RawrXD_Debugger_AddWatch(uint64_t addr, uint64_t len, const char* name);
void RawrXD_Debugger_RemoveWatch(uint64_t addr);
void RawrXD_Debugger_RemoveWatchString(const char* name);
uint32_t RawrXD_Debugger_GetWatchCount(void);
int RawrXD_Debugger_GetWatchAt(uint32_t index, uint64_t* outAddr, uint64_t* outLen, char* outName, uint32_t nameBufSize);

#ifdef __cplusplus
}
#endif
