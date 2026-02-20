; ============================================================================
; RawrXD Pipe Server
; ============================================================================

option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


EXTERN CreateNamedPipeW : PROC
EXTERN CloseHandle : PROC
EXTERN ConnectNamedPipe : PROC
EXTERN DisconnectNamedPipe : PROC
EXTERN ReadFile : PROC
EXTERN WriteFile : PROC
EXTERN FlushFileBuffers : PROC

EXTERN Titan_SubmitPrompt : PROC
EXTERN g_InputState : DWORD
EXTERN g_OutputLength : DWORD
EXTERN Sleep : PROC
EXTERN g_OutputBuffer : BYTE

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

; ═══════════════════════════════════════════════════════════════════════════════
; Pipe_Read
; RCX = Buffer, RDX = Size
; Returns bytes read in RAX
; ═══════════════════════════════════════════════════════════════════════════════
Pipe_Read PROC FRAME
    push rbx
    sub rsp, 48
    .endprolog
    
    mov rbx, 0 ; Bytes Read
    
    cmp g_hPipe, 0
    jz @read_fail
    
    ; ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped)
    mov r9, rcx ; Buffer (logic mismatch, RCX is Arg1, RDX is Arg2)
                ; Win64 ABI: RCX=Arg1, RDX=Arg2, R8=Arg3, R9=Arg4
    
    ; Function signature: Pipe_Read(void* buffer, uint32_t size)
    ; RCX = Buffer
    ; RDX = Size
    
    mov r8, rdx             ; nNumberOfBytesToRead
    mov rdx, rcx            ; lpBuffer
    mov rcx, g_hPipe        ; hFile
    
    lea r9, [rsp + 40]      ; lpNumberOfBytesRead
    mov qword ptr [rsp + 32], 0 ; lpOverlapped
    
    call ReadFile
    
    test rax, rax
    jz @read_fail
    
    mov eax, dword ptr [rsp + 40] ; Return bytes read
    jmp @read_ret
    
@read_fail:
    xor eax, eax

@read_ret:
    add rsp, 48
    pop rbx
    ret
Pipe_Read ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Pipe_Write
; RCX = Buffer, RDX = Size
; Returns bytes written in RAX
; ═══════════════════════════════════════════════════════════════════════════════
Pipe_Write PROC FRAME
    push rbx
    sub rsp, 48
    .endprolog
    
    cmp g_hPipe, 0
    jz @write_fail
    
    ; WriteFile(g_hPipe, buffer, size, &bytesWritten, NULL)
    
    mov r8, rdx         ; Size
    mov rdx, rcx        ; Buffer
    mov rcx, g_hPipe    ; Handle
    
    lea r9, [rsp + 40]  ; BytesWritten
    mov qword ptr [rsp + 32], 0 ; Overlapped
    
    call WriteFile
    
    test rax, rax
    jz @write_fail
    
    mov rcx, g_hPipe
    call FlushFileBuffers
    
    mov eax, dword ptr [rsp + 40]
    jmp @write_ret
    
@write_fail:
    xor eax, eax

@write_ret:
    add rsp, 48
    pop rbx
    ret
Pipe_Write ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Pipe_WaitForClient
; Blocks until a client connects.
; ═══════════════════════════════════════════════════════════════════════════════
Pipe_WaitForClient PROC FRAME
    push rbx
    sub rsp, 32
    .endprolog
    
    cmp g_hPipe, 0
    jz @wait_fail
    
    mov rcx, g_hPipe
    mov rdx, 0 ; NULL overlapped
    call ConnectNamedPipe
    
    ; Returns non-zero on success, or 0 if failed (unless ERROR_PIPE_CONNECTED)
    ; In simple blocking mode, we assume success or fail.
    
    mov eax, 1 ; Success
    add rsp, 32
    pop rbx
    ret

@wait_fail:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
Pipe_WaitForClient ENDP

PUBLIC StartPipeServer
PUBLIC StopPipeServer
PUBLIC Pipe_Read
PUBLIC Pipe_Write
PUBLIC Pipe_WaitForClient        

; ═══════════════════════════════════════════════════════════════════════════════
; Pipe_RunServer
; Main Loop: Accept -> Read -> Infer -> reply -> Disconnect
; ═══════════════════════════════════════════════════════════════════════════════
Pipe_RunServer PROC FRAME
    push rbx
    sub rsp, 4144 ; 4096 buffer + shadow + alignment
    .endprolog
    
@server_global_loop:
    call Pipe_WaitForClient
    test eax, eax
    jz @server_retry ; If fail, just retry accept
    
    ; Read Prompt
    lea rcx, [rsp+32] ; Buffer
    mov rdx, 4096     ; Max Size
    call Pipe_Read
    
    test eax, eax
    jz @server_disconnect
    
    ; Submit to Titan Engine
    lea rcx, [rsp+32] ; String Ptr
    mov edx, eax      ; Length
    call Titan_SubmitPrompt
    
    ; Wait for inference to complete (Busy Wait with Sleep)
@wait_for_titan:
    mov eax, [g_InputState]
    test eax, eax
    jz @titan_done
    
    mov rcx, 1
    call Sleep
    jmp @wait_for_titan
    
@titan_done:
    ; Write Response from g_OutputBuffer
    lea rcx, g_OutputBuffer 
    
    ; Use g_OutputLength (Tokens * 4)
    mov edx, [g_OutputLength]
    
@write_resp:
    ; Buffer in RCX, Len in RDX
    call Pipe_Write
    
@server_disconnect:
    mov rcx, g_hPipe
    call DisconnectNamedPipe
    
@server_retry:
    jmp @server_global_loop
    
    add rsp, 4144
    pop rbx
    ret
Pipe_RunServer ENDP

PUBLIC Pipe_RunServer

END
