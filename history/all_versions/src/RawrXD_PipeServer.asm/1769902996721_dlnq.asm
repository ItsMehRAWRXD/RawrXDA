; ============================================================================
; RawrXD Pipe Server
; ============================================================================

option casemap:none

EXTERN CreateNamedPipeW : PROC
EXTERN CloseHandle : PROC
EXTERN ConnectNamedPipe : PROC
EXTERN DisconnectNamedPipe : PROC

.data
align 16
g_hPipe     dq 0
PipeName    dw '\','\','.','\','p','i','p','e','\','R','a','w','r','X','D','_','I','P','C',0

.code

; ═══════════════════════════════════════════════════════════════════════════════
; StartPipeServer
; Creates a named pipe instance. Returns Handle in RAX (or 0 on fail).
; ═══════════════════════════════════════════════════════════════════════════════
StartPipeServer PROC FRAME
    push rbx
    sub rsp, 80
    .endprolog
    
    ; CreateNamedPipeW(
    ;   name,
    ;   PIPE_ACCESS_DUPLEX,
    ;   PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
    ;   PIPE_UNLIMITED_INSTANCES,
    ;   4096, 4096,
    ;   0,
    ;   NULL);
    
    ; Stack Args from 8 to 4
    mov qword ptr [rsp + 32 + 24], 0    ; lpSecurityAttributes (Arg 8)
    mov qword ptr [rsp + 32 + 16], 0    ; nDefaultTimeOut (Arg 7)
    mov dword ptr [rsp + 32 + 8], 4096  ; nInBufferSize (Arg 6)
    mov dword ptr [rsp + 32], 4096      ; nOutBufferSize (Arg 5)
    
    mov r9d, 255                        ; nMaxInstances (Arg 4) (PIPE_UNLIMITED_INSTANCES)
    mov r8d, 6                          ; dwPipeMode (Arg 3) (PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT)
                                        ; 4 | 2 | 0 = 6
    mov edx, 3                          ; dwOpenMode (Arg 2) (PIPE_ACCESS_DUPLEX = 3)
    lea rcx, PipeName                   ; lpName (Arg 1)
    
    call CreateNamedPipeW
    
    cmp rax, -1
    je @fail
    
    mov g_hPipe, rax
    
    ; Optional: Launch thread here to ConnectNamedPipe?
    ; For now, return handle, let caller Connect.
    
    add rsp, 80
    pop rbx
    ret
    
@fail:
    xor eax, eax
    add rsp, 80
    pop rbx
    ret
StartPipeServer ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; StopPipeServer
; Closes the global pipe handle.
; ═══════════════════════════════════════════════════════════════════════════════
StopPipeServer PROC FRAME
    push rbx
    sub rsp, 32
    .endprolog
    
    cmp g_hPipe, 0
    jz @ret
    
    mov rcx, g_hPipe
    call DisconnectNamedPipe
    
    mov rcx, g_hPipe
    call CloseHandle
    
    mov g_hPipe, 0
    
@ret:
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
StopPipeServer ENDP

END
