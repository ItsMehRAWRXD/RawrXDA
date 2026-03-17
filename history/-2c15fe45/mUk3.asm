; ============================================================================
; AGENT_SYSTEM_CORE.ASM - Stub for agent system
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib

PUBLIC AgentSystem_Init

.data
    g_bAgentInit    dd 0

.code

AgentSystem_Init PROC
    mov g_bAgentInit, 1
    mov eax, 1  ; Return success
    ret
AgentSystem_Init ENDP

END
