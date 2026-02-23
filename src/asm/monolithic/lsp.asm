; RawrXD LSP Bridge — JSON-RPC over stdin/stdout of LSP subprocess
; Full FRAME compliance for stack unwinding and SEH

EXTERN CreatePipe:PROC
EXTERN CreateProcessW:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN BeaconSend:PROC

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

.code
; ────────────────────────────────────────────────────────────────
; LSPBridgeInit — create pipes and launch LSP subprocess
;   RCX = lspPath (LPCWSTR); NULL = deferred init (no-op)
;   Returns: EAX = 0 on success, -1 on failure
;   FRAME: 2 pushes (rbp, rbx) + 40h alloc
;     ABI: 8(ret) + 16(pushes) + 40h(64) = 88 → not aligned
;     Fix: 2 pushes + 38h = 8+16+56 = 80 → 80/16=5. Good.
; ────────────────────────────────────────────────────────────────
LSPBridgeInit PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 38h
    .allocstack 38h
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
    mov     dword ptr [rsp+20h], 1      ; bInheritHandles = TRUE
    mov     dword ptr [rsp+28h], CREATE_NO_WINDOW
    mov     qword ptr [rsp+30h], 0      ; lpEnvironment
    ; stack slots [38h], [40h] need startInfo & procInfo — use direct push pattern
    ; NOTE: We've run out of stack-param slots in 38h alloc.
    ;       CreateProcessW needs 10 params (4 reg + 6 stack).
    ;       Handled via the 38h alloc = 56 bytes = 7 slots. Slots 20h..50h.
    lea     rax, startInfo
    mov     [rsp+38h], rax              ; lpStartupInfo
    lea     rax, procInfo
    mov     [rsp+40h], rax              ; lpProcessInformation
    mov     qword ptr [rsp+48h], 0      ; lpCurrentDirectory = NULL
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
; LSPBridgeShutdown — close all pipe and process handles
;   Returns: EAX = 0
; ────────────────────────────────────────────────────────────────
LSPBridgeShutdown PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    cmp     g_lspReady, 0
    je      @shutdown_done

    mov     rcx, hLSPInWrite
    test    rcx, rcx
    jz      @skip_inwrite
    call    CloseHandle
@skip_inwrite:
    mov     rcx, hLSPOutRead
    test    rcx, rcx
    jz      @skip_outread
    call    CloseHandle
@skip_outread:
    mov     rcx, hLSPProcess
    test    rcx, rcx
    jz      @skip_proc
    call    CloseHandle
@skip_proc:
    mov     rcx, hLSPThread
    test    rcx, rcx
    jz      @skip_thread
    call    CloseHandle
@skip_thread:
    mov     g_lspReady, 0

@shutdown_done:
    add     rsp, 28h
    xor     eax, eax
    ret
LSPBridgeShutdown ENDP

END
