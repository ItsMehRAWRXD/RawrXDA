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

public InitializeLLMClient
public SwitchLLMBackend
public GetCurrentBackendName
public CleanupLLMClient

public InitializeAgenticLoop
public StartAgenticLoop
public StopAgenticLoop
public GetAgentStatus
public CleanupAgenticLoop

public InitializeChatInterface
public CleanupChatInterface
public ProcessUserMessage

InitializeLLMClient proc
    mov eax, 1
    ret
InitializeLLMClient endp

SwitchLLMBackend proc backendIndex:DWORD
    ; accept backend index, do nothing
    mov eax, 1
    ret
SwitchLLMBackend endp

GetCurrentBackendName proc lpBuf:DWORD
    ; write a stub backend name
    invoke lstrcpy, lpBuf, addr szBackendStub
    mov eax, 1
    ret
GetCurrentBackendName endp

CleanupLLMClient proc
    mov eax, 1
    ret
CleanupLLMClient endp

InitializeAgenticLoop proc
    mov eax, 1
    ret
InitializeAgenticLoop endp

StartAgenticLoop proc lpMsg:DWORD
    mov eax, 1
    ret
StartAgenticLoop endp

StopAgenticLoop proc
    mov eax, 1
    ret
StopAgenticLoop endp

GetAgentStatus proc
    mov eax, 0
    ret
GetAgentStatus endp

CleanupAgenticLoop proc
    mov eax, 1
    ret
CleanupAgenticLoop endp

InitializeChatInterface proc hWnd:DWORD
    mov eax, 1
    ret
InitializeChatInterface endp

CleanupChatInterface proc
    mov eax, 1
    ret
CleanupChatInterface endp

ProcessUserMessage proc lpMsg:DWORD
    mov eax, 1
    ret
ProcessUserMessage endp

end