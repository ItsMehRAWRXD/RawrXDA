#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// RawrXD_RingBuffer_Consumer.asm
// -----------------------------------------------------------------------------
BOOL RingBufferConsumer_Initialize(HWND hWndOutput, void* pVocabTable);
void RingBufferConsumer_Shutdown();
// Internal thread proc: DWORD WINAPI ConsumerThreadProc(LPVOID lpParam);

// -----------------------------------------------------------------------------
// RawrXD_HTTP_Router.asm
// -----------------------------------------------------------------------------
BOOL HttpRouter_Initialize();
// Internal: void HttpAcceptorThread();
// Internal: void HttpWorkerThread();

// -----------------------------------------------------------------------------
// RawrXD_Model_StateMachine.asm
// -----------------------------------------------------------------------------
void ModelState_Initialize();
void* ModelState_AcquireInstance(const char* modelPath);
BOOL ModelState_Transition(void* pInstance, DWORD targetState);
BOOL ModelState_BeginStream(void* pInstance);
void ModelState_EndStream(void* pInstance);
void ModelState_Release(void* pInstance);

// -----------------------------------------------------------------------------
// RawrXD_Swarm_Orchestrator.asm
// -----------------------------------------------------------------------------
BOOL Swarm_Initialize();
UINT64 Swarm_SubmitJob(void* pJob); // Returns JobId
// Internal: void SwarmSchedulerThread();
// Internal: void VramMonitorThread();
// Internal: void* SelectOptimalModel(void* pJob);

// -----------------------------------------------------------------------------
// RawrXD_Agentic_Router.asm
// -----------------------------------------------------------------------------
void AgentRouter_Initialize();
DWORD AgentRouter_ClassifyIntent(const char* text, DWORD length); // Returns IntentId
DWORD AgentRouter_SelectTool(DWORD intentId); // Returns ToolId
BOOL AgentRouter_ExecuteTask(void* pTask);
void AgentRouter_CancelTask(void* pTask);

// -----------------------------------------------------------------------------
// RawrXD_Streaming_Formatter.asm
// -----------------------------------------------------------------------------
DWORD StreamFormatter_Create(SOCKET s, DWORD formatType); // Returns formatter index
void StreamFormatter_WriteToken(DWORD formatterIdx, const char* data, DWORD len);
void StreamFormatter_FlushChunk(DWORD formatterIdx);
void StreamFormatter_Close(DWORD formatterIdx);

// -----------------------------------------------------------------------------
// RawrXD_JSON_Parser.asm
// -----------------------------------------------------------------------------
// Returns length of extracted value, 0 if not found
UINT64 Json_ExtractField(const char* json, UINT64 len, const char* fieldName, char* outBuf, UINT64 outCap);

#ifdef __cplusplus
}
#endif
