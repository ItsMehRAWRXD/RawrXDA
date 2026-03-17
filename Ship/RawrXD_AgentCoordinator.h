// RawrXD_AgentCoordinator.h — C API for multi-agent coordinator (pipeline dequeue, etc.)
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* AgentCoordinatorHandle;

AgentCoordinatorHandle CreateAgentCoordinator(void);
void DestroyAgentCoordinator(AgentCoordinatorHandle coord);
BOOL AgentCoordinator_Initialize(AgentCoordinatorHandle coord);
void AgentCoordinator_Shutdown(AgentCoordinatorHandle coord);
int AgentCoordinator_SubmitTask(AgentCoordinatorHandle coord, const wchar_t* description, int priority);
int AgentCoordinator_GetPendingTaskCount(AgentCoordinatorHandle coord);
BOOL AgentCoordinator_TryDequeueTask(AgentCoordinatorHandle coord, wchar_t* descBuffer, int descBufferLen, int* outPriority);

#ifdef __cplusplus
}
#endif
