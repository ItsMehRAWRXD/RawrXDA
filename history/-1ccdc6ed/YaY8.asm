; ============================================================================
; Phase 4 Integration Stubs - Minimal implementations to satisfy linking
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

.data
szBackendStub db "StubBackend",0

.code

; Export with exact decoration matching linker expectations
public _InitializeLLMClient@0
public _SwitchLLMBackend@4
public _GetCurrentBackendName@4
public _CleanupLLMClient@0

public _InitializeAgenticLoop@0
public _StartAgenticLoop@4
public _StopAgenticLoop@0
public _GetAgentStatus@0
public _CleanupAgenticLoop@0

public _InitializeChatInterface@4
public _CleanupChatInterface@0
public _ProcessUserMessage@4

_InitializeLLMClient@0 proc
    mov eax, 1
    ret
_InitializeLLMClient@0 endp

_SwitchLLMBackend@4 proc
    mov eax, 1
    ret
_SwitchLLMBackend@4 endp

_GetCurrentBackendName@4 proc
    mov eax, 1
    ret
_GetCurrentBackendName@4 endp

_CleanupLLMClient@0 proc
    mov eax, 1
    ret
_CleanupLLMClient@0 endp

_InitializeAgenticLoop@0 proc
    mov eax, 1
    ret
_InitializeAgenticLoop@0 endp

_StartAgenticLoop@4 proc
    mov eax, 1
    ret
_StartAgenticLoop@4 endp

_StopAgenticLoop@0 proc
    mov eax, 1
    ret
_StopAgenticLoop@0 endp

_GetAgentStatus@0 proc
    mov eax, 0
    ret
_GetAgentStatus@0 endp

_CleanupAgenticLoop@0 proc
    mov eax, 1
    ret
_CleanupAgenticLoop@0 endp

_InitializeChatInterface@4 proc
    mov eax, 1
    ret
_InitializeChatInterface@4 endp

_CleanupChatInterface@0 proc
    mov eax, 1
    ret
_CleanupChatInterface@0 endp

_ProcessUserMessage@4 proc
    mov eax, 1
    ret
_ProcessUserMessage@4 endp

end