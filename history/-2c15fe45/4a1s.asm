; ============================================================================
; AGENT_SYSTEM_CORE.ASM - Minimal agent system core (safe init only)
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib

PUBLIC AgentSystem_Init

.data
    g_bAgentInit     dd 0
    g_hAgentStopEvt  dd 0
    g_csAgent        CRITICAL_SECTION <>
    szAgentInitMsg   db "Agent system initialized",0

.code

AgentSystem_Init PROC
    ; Idempotent initialization
    cmp g_bAgentInit, 1
    je @ok

    ; Stop event for future lifecycle management (manual-reset, initially non-signaled)
    invoke CreateEventA, NULL, TRUE, FALSE, NULL
    mov g_hAgentStopEvt, eax
    test eax, eax
    jz @fail

    invoke InitializeCriticalSection, ADDR g_csAgent
    mov g_bAgentInit, 1
    invoke OutputDebugStringA, ADDR szAgentInitMsg

@ok:
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret
AgentSystem_Init ENDP

END
