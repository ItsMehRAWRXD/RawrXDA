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
_GetCurrentBackendName endp

_CleanupLLMClient proc C
    mov eax, 1
    ret
_CleanupLLMClient endp

_InitializeAgenticLoop proc C
    mov eax, 1
    ret
_InitializeAgenticLoop endp

_StartAgenticLoop proc C lpMsg:DWORD
    mov eax, 1
    ret
_StartAgenticLoop endp

_StopAgenticLoop proc C
    mov eax, 1
    ret
_StopAgenticLoop endp

_GetAgentStatus proc C
    mov eax, 0
    ret
_GetAgentStatus endp

_CleanupAgenticLoop proc C
    mov eax, 1
    ret
_CleanupAgenticLoop endp

_InitializeChatInterface proc C hWnd:DWORD
    mov eax, 1
    ret
_InitializeChatInterface endp

_CleanupChatInterface proc C
    mov eax, 1
    ret
_CleanupChatInterface endp

_ProcessUserMessage proc C lpMsg:DWORD
    mov eax, 1
    ret
_ProcessUserMessage endp

end