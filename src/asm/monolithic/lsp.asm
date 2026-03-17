; RawrXD LSP Bridge — JSON-RPC over stdin/stdout of LSP subprocess
; Full FRAME compliance for stack unwinding and SEH

EXTERN CreatePipe:PROC
EXTERN CreateProcessW:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN BeaconSend:PROC
EXTERN PeekNamedPipe:PROC
EXTERN Sleep:PROC
EXTERN GetTickCount64:PROC
EXTERN VirtualFree:PROC

PUBLIC LSPBridgeInit
PUBLIC LSPSendRequest
PUBLIC LSPBridgeShutdown

.data?
hLSPInRead    dq ?
hLSPInWrite   dq ?
hLSPOutRead   dq ?
hLSPOutWrite  dq ?
hLSPProcess   dq ?
hLSPThread    dq ?
g_lspReady    dd ?
g_lspState    dd ?

; LSP allocated buffer pointers
g_lspReqBuf   dq ?
g_lspRspBuf   dq ?
g_lspDiagBuf  dq ?

; Telemetry
g_lspShutdownCount dd ?
align 8
g_lspShutdownTs    dq ?

; SECURITY_ATTRIBUTES (24 bytes on x64)
align 8
saInherit     dq 3 dup(?)

; STARTUPINFOW (104 bytes on x64)
align 8
startInfo     db 104 dup(?)

; PROCESS_INFORMATION (24 bytes)
align 8
procInfo      db 24 dup(?)

.const
STARTF_USESTDHANDLES equ 100h
CREATE_NO_WINDOW     equ 8000000h
SA_SIZE              equ 24
STARTUPINFO_SIZE     equ 104
LSP_BEACON_SLOT      equ 4
LSP_SHUTDOWN_MSG_LEN equ 66
LSP_EXIT_MSG_LEN     equ 55
MEM_RELEASE          equ 8000h

; Pre-built JSON-RPC shutdown request (Content-Length header + body)
szLspShutdownMsg db 'Content-Length: 44', 13, 10, 13, 10
                 db '{', '"jsonrpc":"2.0","id":1,"method":"shutdown"', '}'
szLspExitMsg     db 'Content-Length: 33', 13, 10, 13, 10
                 db '{', '"jsonrpc":"2.0","method":"exit"', '}'

.code
; ────────────────────────────────────────────────────────────────
; LSPBridgeInit — create pipes and launch LSP subprocess
;   RCX = lspPath (LPCWSTR); NULL = deferred init (no-op)
;   Returns: EAX = 0 on success, -1 on failure
;   FRAME: 2 pushes (rbp, rbx) + 58h alloc
;     ABI: 8(ret) + 16(pushes) + 58h(88) = 112 → 112/16=7. Good.
;     CreateProcessW: 10 params → 4 regs + 6 stack (20h..48h) = needs 50h min
; ────────────────────────────────────────────────────────────────
LSPBridgeInit PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 50h
    .allocstack 50h
    .endprolog

    mov     rbx, rcx                    ; rbx = lspPath
    test    rbx, rbx
    jz      @lsp_deferred               ; NULL path = deferred init

    ; ── Set up SECURITY_ATTRIBUTES for inheritable handles ──
    lea     rcx, saInherit
    mov     dword ptr [rcx], SA_SIZE     ; nLength = 24
    mov     qword ptr [rcx+8], 0        ; lpSecurityDescriptor = NULL
    mov     dword ptr [rcx+16], 1       ; bInheritHandle = TRUE

    ; ── Create stdin pipe (parent writes → child reads) ──
    lea     rcx, hLSPInRead             ; pRead
    lea     rdx, hLSPInWrite            ; pWrite
    lea     r8, saInherit               ; lpPipeAttributes
    xor     r9d, r9d                    ; nSize = 0 (default)
    call    CreatePipe
    test    eax, eax
    jz      @lsp_fail

    ; ── Create stdout pipe (child writes → parent reads) ──
    lea     rcx, hLSPOutRead
    lea     rdx, hLSPOutWrite
    lea     r8, saInherit
    xor     r9d, r9d
    call    CreatePipe
    test    eax, eax
    jz      @lsp_fail_close_in

    ; ── Zero STARTUPINFOW ──
    lea     rdi, startInfo
    mov     rcx, STARTUPINFO_SIZE / 8
    xor     eax, eax
    rep stosq

    ; Fill STARTUPINFOW fields
    lea     rax, startInfo
    mov     dword ptr [rax], STARTUPINFO_SIZE   ; cb
    mov     ecx, STARTF_USESTDHANDLES
    mov     dword ptr [rax+60], ecx             ; dwFlags (offset 60 on x64)
    mov     rcx, hLSPInRead
    mov     [rax+64], rcx                       ; hStdInput
    mov     rcx, hLSPOutWrite
    mov     [rax+72], rcx                       ; hStdOutput
    mov     [rax+80], rcx                       ; hStdError = same as stdout

    ; ── CreateProcessW ──
    xor     ecx, ecx                    ; lpApplicationName = NULL
    mov     rdx, rbx                    ; lpCommandLine = lspPath
    xor     r8d, r8d                    ; lpProcessAttributes
    xor     r9d, r9d                    ; lpThreadAttributes
    mov     dword ptr [rsp+20h], 1      ; bInheritHandles = TRUE          (param 5)
    mov     dword ptr [rsp+28h], CREATE_NO_WINDOW ;                      (param 6)
    mov     qword ptr [rsp+30h], 0      ; lpEnvironment = NULL           (param 7)
    mov     qword ptr [rsp+38h], 0      ; lpCurrentDirectory = NULL      (param 8)
    lea     rax, startInfo
    mov     [rsp+40h], rax              ; lpStartupInfo                  (param 9)
    lea     rax, procInfo
    mov     [rsp+48h], rax              ; lpProcessInformation           (param 10)
    call    CreateProcessW
    test    eax, eax
    jz      @lsp_fail_close_all

    ; Save process/thread handles
    lea     rax, procInfo
    mov     rcx, [rax]                  ; hProcess
    mov     hLSPProcess, rcx
    mov     rcx, [rax+8]                ; hThread
    mov     hLSPThread, rcx

    ; Close child-side pipe ends (parent doesn't need them)
    mov     rcx, hLSPInRead
    call    CloseHandle
    mov     rcx, hLSPOutWrite
    call    CloseHandle

    mov     g_lspReady, 1

@lsp_deferred:
    lea     rsp, [rbp]
    pop     rdi
    pop     rbx
    pop     rbp
    xor     eax, eax
    ret

@lsp_fail_close_all:
    mov     rcx, hLSPOutRead
    call    CloseHandle
    mov     rcx, hLSPOutWrite
    call    CloseHandle
@lsp_fail_close_in:
    mov     rcx, hLSPInRead
    call    CloseHandle
    mov     rcx, hLSPInWrite
    call    CloseHandle
@lsp_fail:
    mov     g_lspReady, 0
    lea     rsp, [rbp]
    pop     rdi
    pop     rbx
    pop     rbp
    mov     eax, -1
    ret
LSPBridgeInit ENDP

; ────────────────────────────────────────────────────────────────
; LSPSendRequest — write a JSON-RPC request to the LSP pipe
;   RCX = pRequest (pointer to UTF-8 buffer)
;   EDX = length (bytes)
;   Returns: EAX = bytes written, or 0 on failure
;   FRAME: 1 push (rbp) + 30h alloc
;     ABI: 8 + 8 + 30h = 64 → 64/16=4. Good.
; ────────────────────────────────────────────────────────────────
LSPSendRequest PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Guard: LSP not ready
    cmp     g_lspReady, 0
    je      @send_fail

    ; WriteFile(hLSPInWrite, pRequest, length, &bytesWritten, NULL)
    mov     r8d, edx                    ; nBytesToWrite
    mov     rdx, rcx                    ; lpBuffer
    mov     rcx, hLSPInWrite            ; hFile
    lea     r9, [rsp+20h]               ; lpNumberOfBytesWritten
    mov     qword ptr [rsp+28h], 0      ; lpOverlapped = NULL
    call    WriteFile
    test    eax, eax
    jz      @send_fail
    mov     eax, dword ptr [rsp+20h]    ; return bytes written
    jmp     @send_done

@send_fail:
    xor     eax, eax
@send_done:
    lea     rsp, [rbp]
    pop     rbp
    ret
LSPSendRequest ENDP

; ────────────────────────────────────────────────────────────────
; LSPBridgeShutdown — graceful JSON-RPC shutdown + full cleanup
;   Returns: EAX = 0 on clean shutdown, -1 on timeout / send failure
;   FRAME: 4 pushes (rbp,rbx,rsi,rdi) + 148h alloc
;     ABI: 8(ret) + 32(pushes) + 148h(328) = 368 → 368/16=23. Aligned.
;     PeekNamedPipe: 6 params → 4 regs + 2 stack (20h,28h)
;     Local readBuffer[256] at [rsp+48h..147h]
;     bytesWritten at [rsp+30h], totalAvail at [rsp+38h]
;     shutdownResult at [rsp+40h]
; ────────────────────────────────────────────────────────────────
LSPBridgeShutdown PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 148h
    .allocstack 148h
    .endprolog

    ; ── Step 1: Check if LSP bridge is initialized (g_lspState != 0) ──
    cmp     g_lspReady, 0
    je      @sd_not_init

    ; Assume success; overwrite on timeout/failure
    mov     dword ptr [rsp+40h], 0      ; shutdownResult = 0

    ; ── Step 2: Send JSON-RPC "shutdown" request via pipe ──
    ; Content-Length: 44\r\n\r\n{"jsonrpc":"2.0","id":1,"method":"shutdown"}
    mov     rcx, hLSPInWrite            ; hFile = pipe write handle
    lea     rdx, szLspShutdownMsg       ; lpBuffer
    mov     r8d, LSP_SHUTDOWN_MSG_LEN   ; nNumberOfBytesToWrite = 66
    lea     r9, [rsp+30h]               ; lpNumberOfBytesWritten
    mov     qword ptr [rsp+20h], 0      ; lpOverlapped = NULL
    call    WriteFile
    test    eax, eax
    jz      @sd_write_fail

    ; ── Step 3: Wait for shutdown response — 5-second poll via PeekNamedPipe ──
    call    GetTickCount64
    mov     rsi, rax                    ; rsi = poll-start timestamp

@sd_poll:
    ; PeekNamedPipe(hLSPOutRead, NULL, 0, NULL, &totalAvail, NULL)
    mov     rcx, hLSPOutRead            ; hNamedPipe
    xor     edx, edx                    ; lpBuffer = NULL
    xor     r8d, r8d                    ; nBufferSize = 0
    xor     r9d, r9d                    ; lpBytesRead = NULL
    lea     rax, [rsp+38h]
    mov     [rsp+20h], rax              ; lpTotalBytesAvail
    mov     qword ptr [rsp+28h], 0      ; lpBytesLeftThisMessage = NULL
    call    PeekNamedPipe
    test    eax, eax
    jz      @sd_check_elapsed           ; peek failed → check timeout

    cmp     dword ptr [rsp+38h], 0      ; any bytes available?
    jne     @sd_drain_response

@sd_check_elapsed:
    call    GetTickCount64
    sub     rax, rsi                    ; elapsed ms
    cmp     rax, 5000                   ; 5 000 ms deadline
    jae     @sd_timeout

    mov     ecx, 50                     ; Sleep(50) — yield between polls
    call    Sleep
    jmp     @sd_poll

@sd_drain_response:
    ; ReadFile to consume the JSON-RPC shutdown response
    mov     rcx, hLSPOutRead            ; hFile
    lea     rdx, [rsp+48h]              ; lpBuffer (256-byte local)
    mov     r8d, 100h                   ; nNumberOfBytesToRead = 256
    lea     r9, [rsp+38h]               ; lpNumberOfBytesRead
    mov     qword ptr [rsp+20h], 0      ; lpOverlapped = NULL
    call    ReadFile
    jmp     @sd_exit_notify

@sd_timeout:
    mov     dword ptr [rsp+40h], -1     ; shutdownResult = -1 (timeout)
    jmp     @sd_exit_notify

@sd_write_fail:
    mov     dword ptr [rsp+40h], -1     ; shutdownResult = -1 (write err)

@sd_exit_notify:
    ; ── Step 4: Send "exit" notification (no id — JSON-RPC notification) ──
    mov     rcx, hLSPInWrite
    test    rcx, rcx
    jz      @sd_close_handles           ; pipe already gone
    lea     rdx, szLspExitMsg           ; lpBuffer
    mov     r8d, LSP_EXIT_MSG_LEN       ; nNumberOfBytesToWrite = 55
    lea     r9, [rsp+30h]               ; &bytesWritten
    mov     qword ptr [rsp+20h], 0
    call    WriteFile                   ; result ignored — shutting down

    ; ── Step 5: Close pipe handles (hPipeRead, hPipeWrite, hProcess, hThread) ──
@sd_close_handles:
    mov     rcx, hLSPInWrite
    test    rcx, rcx
    jz      @sd_c1
    call    CloseHandle
@sd_c1:
    mov     rcx, hLSPOutRead
    test    rcx, rcx
    jz      @sd_c2
    call    CloseHandle
@sd_c2:
    mov     rcx, hLSPProcess
    test    rcx, rcx
    jz      @sd_c3
    call    CloseHandle
@sd_c3:
    mov     rcx, hLSPThread
    test    rcx, rcx
    jz      @sd_c4
    call    CloseHandle
@sd_c4:

    ; ── Step 6: Free allocated buffers (request, response, diagnostic) ──
    mov     rcx, g_lspReqBuf
    test    rcx, rcx
    jz      @sd_f1
    xor     edx, edx                    ; dwSize = 0 (required for MEM_RELEASE)
    mov     r8d, MEM_RELEASE            ; dwFreeType = 8000h
    call    VirtualFree
@sd_f1:
    mov     rcx, g_lspRspBuf
    test    rcx, rcx
    jz      @sd_f2
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@sd_f2:
    mov     rcx, g_lspDiagBuf
    test    rcx, rcx
    jz      @sd_f3
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@sd_f3:

    ; ── Step 7: Zero out all LSP state globals ──
    xor     eax, eax
    mov     g_lspReady, eax
    mov     g_lspState, eax
    mov     qword ptr hLSPInRead, 0
    mov     qword ptr hLSPInWrite, 0
    mov     qword ptr hLSPOutRead, 0
    mov     qword ptr hLSPOutWrite, 0
    mov     qword ptr hLSPProcess, 0
    mov     qword ptr hLSPThread, 0
    mov     qword ptr g_lspReqBuf, 0
    mov     qword ptr g_lspRspBuf, 0
    mov     qword ptr g_lspDiagBuf, 0

    ; ── Step 8: Update telemetry — increment shutdown count, stamp timestamp ──
    lock inc dword ptr g_lspShutdownCount
    call    GetTickCount64
    mov     g_lspShutdownTs, rax        ; shutdown timestamp (ms since boot)

    ; Return shutdownResult (0 = clean, -1 = timeout)
    mov     eax, dword ptr [rsp+40h]
    jmp     @sd_epilogue

@sd_not_init:
    xor     eax, eax                    ; not initialized → return 0 (no-op)

@sd_epilogue:
    lea     rsp, [rbp]
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
LSPBridgeShutdown ENDP

END
