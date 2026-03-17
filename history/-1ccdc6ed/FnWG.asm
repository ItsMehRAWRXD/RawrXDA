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

public _InitializeLLMClient
public _SwitchLLMBackend
public _GetCurrentBackendName
public _CleanupLLMClient

public _InitializeAgenticLoop
public _StartAgenticLoop
public _StopAgenticLoop
public _GetAgentStatus
public _CleanupAgenticLoop

public _InitializeChatInterface
public _CleanupChatInterface
public _ProcessUserMessage

_InitializeLLMClient proc
    mov eax, 1
    ret
_InitializeLLMClient endp

_SwitchLLMBackend proc backendIndex:DWORD
    ; accept backend index, do nothing
    mov eax, 1
    ret
_SwitchLLMBackend endp

_GetCurrentBackendName proc lpBuf:DWORD
    ; write a stub backend name
    invoke lstrcpy, lpBuf, addr szBackendStub
    mov eax, 1
    ret
_GetCurrentBackendName endp

_CleanupLLMClient proc
    mov eax, 1
    ret
_CleanupLLMClient endp

_InitializeAgenticLoop proc
    mov eax, 1
    ret
_InitializeAgenticLoop endp

_StartAgenticLoop proc lpMsg:DWORD
    mov eax, 1
    ret
_StartAgenticLoop endp

_StopAgenticLoop proc
    mov eax, 1
    ret
_StopAgenticLoop endp

_GetAgentStatus proc
    mov eax, 0
    ret
_GetAgentStatus endp

_CleanupAgenticLoop proc
    mov eax, 1
    ret
_CleanupAgenticLoop endp

_InitializeChatInterface proc hWnd:DWORD
    mov eax, 1
    ret
_InitializeChatInterface endp

_CleanupChatInterface proc
    mov eax, 1
    ret
_CleanupChatInterface endp

_ProcessUserMessage proc lpMsg:DWORD
    mov eax, 1
    ret
_ProcessUserMessage endp

end