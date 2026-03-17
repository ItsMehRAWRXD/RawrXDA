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

; Use ALIAS to map the double-underscore symbols to single-underscore
ALIAS <__InitializeLLMClient@0@0> = <_InitializeLLMClient@0>
ALIAS <__SwitchLLMBackend@4@0> = <_SwitchLLMBackend@4>
ALIAS <__GetCurrentBackendName@4@0> = <_GetCurrentBackendName@4>
ALIAS <__CleanupLLMClient@0@0> = <_CleanupLLMClient@0>
ALIAS <__InitializeAgenticLoop@0@0> = <_InitializeAgenticLoop@0>
ALIAS <__StartAgenticLoop@4@0> = <_StartAgenticLoop@4>
ALIAS <__StopAgenticLoop@0@0> = <_StopAgenticLoop@0>
ALIAS <__GetAgentStatus@0@0> = <_GetAgentStatus@0>
ALIAS <__CleanupAgenticLoop@0@0> = <_CleanupAgenticLoop@0>
ALIAS <__InitializeChatInterface@4@0> = <_InitializeChatInterface@4>
ALIAS <__CleanupChatInterface@0@0> = <_CleanupChatInterface@0>
ALIAS <__ProcessUserMessage@4@0> = <_ProcessUserMessage@4>

public __InitializeLLMClient@0@0

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