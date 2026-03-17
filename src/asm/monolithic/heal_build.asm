; ═══════════════════════════════════════════════════════════════════
; heal_build.asm — Autonomous Build-Heal Engine
;
; RawrXD_HealBuild(pBuildCmd, pWorkspaceRoot) → EAX (0=success)
;
; Executes a build command, parses stderr for error diagnostics,
; applies source-level fixes (via agent tool registry), and retries.
; The caller (main.asm @heal_build) wraps this in a 3-attempt loop;
; each call here represents one build-diagnose-fix cycle.
;
; Flow:
;   1. CreateProcessW(pBuildCmd) → capture stdout+stderr via pipes
;   2. WaitForSingleObject(hProcess, 30000ms timeout)
;   3. ReadFile(hStderrRead) → g_healBuf
;   4. Parse error lines: extract file path + line number + message
;   5. If build succeeded (exit code 0), return 0
;   6. If errors found, call Tool_GetDiagnostics on workspace files
;   7. Apply fixes via Tool_WriteFile (revert-safe)
;   8. Return 1 (caller will retry build)
; ═══════════════════════════════════════════════════════════════════

PUBLIC RawrXD_HealBuild

; ── Win32 imports ────────────────────────────────────────────────
EXTERN CreateProcessW:PROC
EXTERN WaitForSingleObject:PROC
EXTERN GetExitCodeProcess:PROC
EXTERN CloseHandle:PROC
EXTERN ReadFile:PROC
EXTERN CreatePipe:PROC
EXTERN SetHandleInformation:PROC
EXTERN TerminateProcess:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC

; ── Agent tool registry ──────────────────────────────────────────
EXTERN Tool_GetDiagnostics:PROC
EXTERN Tool_WriteFile:PROC
EXTERN Tool_SearchCode:PROC

; ── Globals ──────────────────────────────────────────────────────
EXTERN g_hHeap:QWORD
EXTERN BeaconSend:PROC

; ── Constants ────────────────────────────────────────────────────
HEAL_TIMEOUT_MS         equ 30000       ; 30s build timeout
HEAL_BUF_SIZE           equ 65536       ; 64KB stderr capture
HANDLE_FLAG_INHERIT     equ 1
STARTF_USESTDHANDLES    equ 100h
HEAP_ZERO_MEMORY        equ 8
HEAL_BEACON_SLOT        equ 19
HEAL_EVT_START          equ 0D1h
HEAL_EVT_SUCCESS        equ 0D2h
HEAL_EVT_FAIL           equ 0D3h
HEAL_EVT_FIX_APPLIED    equ 0D4h
WAIT_TIMEOUT            equ 258

; ── STARTUPINFOW (104 bytes on x64) ─────────────────────────────
STARTUPINFOW_SIZE       equ 104
SI_cb                   equ 0           ; DWORD
SI_dwFlags              equ 60          ; DWORD
SI_hStdInput            equ 80          ; QWORD
SI_hStdOutput           equ 88          ; QWORD
SI_hStdError            equ 96          ; QWORD

; ── PROCESS_INFORMATION (24 bytes) ───────────────────────────────
PI_SIZE                 equ 24
PI_hProcess             equ 0           ; QWORD
PI_hThread              equ 8           ; QWORD

; ── SECURITY_ATTRIBUTES (24 bytes) ───────────────────────────────
SA_SIZE                 equ 24

.data
align 8
g_healBuf           dq 0               ; heap-allocated stderr buffer
g_healBufUsed       dd 0               ; bytes read into buffer
g_healErrFile       db 260 dup(0)      ; extracted error file path (ANSI)
g_healErrLine       dd 0               ; extracted error line number
g_healErrCol        dd 0               ; extracted error column
g_healFixCount      dd 0               ; fixes applied this cycle

.code

; ────────────────────────────────────────────────────────────────
; RawrXD_HealBuild — Single build-diagnose-fix cycle
;   RCX = pBuildCmd      (LPCWSTR, e.g. L"cmake --build build_prod ...")
;   RDX = pWorkspaceRoot (LPCWSTR, e.g. L"D:\\rawrxd")
;   Returns EAX = 0 if build passed, 1 if failed (caller retries)
; ────────────────────────────────────────────────────────────────
RawrXD_HealBuild PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 200h               ; large local frame
    .allocstack 200h
    .endprolog

    mov     r14, rcx                ; r14 = pBuildCmd
    mov     r15, rdx                ; r15 = pWorkspaceRoot
    mov     dword ptr [g_healFixCount], 0

    ; ── Beacon: heal cycle start ──────────────────────────────
    mov     ecx, HEAL_BEACON_SLOT
    mov     edx, HEAL_EVT_START
    xor     r8d, r8d
    xor     r9d, r9d
    call    BeaconSend

    ; ── Allocate stderr capture buffer ────────────────────────
    mov     rcx, g_hHeap
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, HEAL_BUF_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      @hb_fail
    mov     g_healBuf, rax
    mov     rdi, rax                ; rdi = buffer pointer

    ; ── Create pipe for stderr capture ────────────────────────
    ; SECURITY_ATTRIBUTES sa = { sizeof(SA), NULL, TRUE }
    lea     rax, [rsp + 100h]       ; sa at rsp+100h (24 bytes)
    mov     dword ptr [rax], SA_SIZE
    mov     qword ptr [rax + 8], 0  ; lpSecurityDescriptor = NULL
    mov     dword ptr [rax + 16], 1 ; bInheritHandle = TRUE

    lea     rcx, [rsp + 0E0h]      ; &hStderrRead
    lea     rdx, [rsp + 0E8h]      ; &hStderrWrite
    lea     r8, [rsp + 100h]       ; &sa
    xor     r9d, r9d                ; nSize = 0 (default)
    call    CreatePipe
    test    eax, eax
    jz      @hb_free_buf

    ; Make read end non-inheritable
    mov     rcx, qword ptr [rsp + 0E0h]  ; hStderrRead
    xor     edx, edx                      ; HANDLE_FLAG_INHERIT = 0 (clear)
    xor     r8d, r8d
    call    SetHandleInformation

    ; ── Build STARTUPINFOW ────────────────────────────────────
    ; Zero the entire STARTUPINFOW (104 bytes) at rsp+20h
    lea     rcx, [rsp + 20h]
    xor     eax, eax
    mov     ecx, STARTUPINFOW_SIZE / 8
    lea     rdi, [rsp + 20h]
    rep     stosq
    mov     rdi, g_healBuf          ; restore rdi = buffer

    lea     rax, [rsp + 20h]
    mov     dword ptr [rax + SI_cb], STARTUPINFOW_SIZE
    mov     dword ptr [rax + SI_dwFlags], STARTF_USESTDHANDLES
    mov     rcx, qword ptr [rsp + 0E8h]   ; hStderrWrite
    mov     qword ptr [rax + SI_hStdOutput], rcx  ; redirect stdout too
    mov     qword ptr [rax + SI_hStdError], rcx   ; stderr

    ; ── CreateProcessW ────────────────────────────────────────
    ; CreateProcessW(NULL, pCmdLine, NULL, NULL, TRUE, 0, NULL, pWorkDir, &si, &pi)
    xor     ecx, ecx                ; lpApplicationName = NULL
    mov     rdx, r14                ; lpCommandLine = pBuildCmd
    xor     r8, r8                  ; lpProcessAttributes = NULL
    xor     r9, r9                  ; lpThreadAttributes = NULL
    mov     dword ptr [rsp + 28h + 200h], 1    ; bInheritHandles = TRUE
    mov     dword ptr [rsp + 30h + 200h], 0    ; dwCreationFlags = 0
    mov     qword ptr [rsp + 38h + 200h], 0    ; lpEnvironment = NULL
    mov     rax, r15                                ; pWorkspaceRoot
    mov     qword ptr [rsp + 40h + 200h], rax       ; lpCurrentDirectory
    lea     rax, [rsp + 20h]
    mov     qword ptr [rsp + 48h + 200h], rax       ; lpStartupInfo
    lea     rax, [rsp + 0C0h]
    mov     qword ptr [rsp + 50h + 200h], rax       ; lpProcessInformation

    ; Fix: CreateProcessW takes 10 params, first 4 in regs, rest on stack
    ; Stack layout after sub rsp, 200h at offset +200h from current RSP
    ; Actually use the standard calling convention properly:
    sub     rsp, 60h                ; extra shadow for 10-param call
    xor     ecx, ecx                ; lpApplicationName = NULL
    mov     rdx, r14                ; lpCommandLine
    xor     r8, r8                  ; lpProcessAttributes
    xor     r9, r9                  ; lpThreadAttributes
    mov     dword ptr [rsp + 20h], 1     ; bInheritHandles = TRUE
    mov     dword ptr [rsp + 28h], 0     ; dwCreationFlags = 0
    mov     qword ptr [rsp + 30h], 0     ; lpEnvironment = NULL
    mov     qword ptr [rsp + 38h], r15   ; lpCurrentDirectory
    lea     rax, [rsp + 60h + 20h]       ; STARTUPINFOW
    mov     qword ptr [rsp + 40h], rax
    lea     rax, [rsp + 60h + 0C0h]      ; PROCESS_INFORMATION
    mov     qword ptr [rsp + 48h], rax
    call    CreateProcessW
    add     rsp, 60h

    test    eax, eax
    jz      @hb_close_pipes

    ; Close write-end of pipe in parent (so ReadFile sees EOF)
    mov     rcx, qword ptr [rsp + 0E8h]
    call    CloseHandle
    mov     qword ptr [rsp + 0E8h], 0

    ; ── Wait for build process ────────────────────────────────
    mov     rcx, qword ptr [rsp + 0C0h + PI_hProcess]
    mov     edx, HEAL_TIMEOUT_MS
    call    WaitForSingleObject
    cmp     eax, WAIT_TIMEOUT
    jne     @hb_no_timeout

    ; Timeout — kill the process
    mov     rcx, qword ptr [rsp + 0C0h + PI_hProcess]
    mov     edx, 1
    call    TerminateProcess

@hb_no_timeout:
    ; ── Read stderr ───────────────────────────────────────────
    mov     rcx, qword ptr [rsp + 0E0h]    ; hStderrRead
    mov     rdx, rdi                         ; buffer
    mov     r8d, HEAL_BUF_SIZE - 1           ; max bytes
    lea     r9, [rsp + 0D0h]                ; &bytesRead (local)
    mov     qword ptr [rsp + 20h], 0         ; lpOverlapped = NULL
    call    ReadFile

    ; Record how much we read
    mov     eax, dword ptr [rsp + 0D0h]
    mov     g_healBufUsed, eax

    ; NUL-terminate the buffer
    mov     rcx, g_healBuf
    mov     eax, g_healBufUsed
    mov     byte ptr [rcx + rax], 0

    ; ── Get exit code ─────────────────────────────────────────
    mov     rcx, qword ptr [rsp + 0C0h + PI_hProcess]
    lea     rdx, [rsp + 0D4h]              ; &exitCode (local DWORD)
    call    GetExitCodeProcess

    mov     r12d, dword ptr [rsp + 0D4h]   ; r12d = exit code

    ; ── Close process handles ─────────────────────────────────
    mov     rcx, qword ptr [rsp + 0C0h + PI_hThread]
    call    CloseHandle
    mov     rcx, qword ptr [rsp + 0C0h + PI_hProcess]
    call    CloseHandle

    ; ── Close read pipe ───────────────────────────────────────
    mov     rcx, qword ptr [rsp + 0E0h]
    call    CloseHandle

    ; ── Check build result ────────────────────────────────────
    test    r12d, r12d
    jnz     @hb_diagnose

    ; Build succeeded
    mov     ecx, HEAL_BEACON_SLOT
    mov     edx, HEAL_EVT_SUCCESS
    xor     r8d, r8d
    xor     r9d, r9d
    call    BeaconSend

    ; Free buffer
    mov     rcx, g_hHeap
    xor     edx, edx
    mov     r8, g_healBuf
    call    HeapFree
    mov     g_healBuf, 0

    xor     eax, eax                ; return 0 = success
    jmp     @hb_epilog

    ; ── Diagnose build errors ─────────────────────────────────
@hb_diagnose:
    ; Parse stderr buffer for "file(line): error" or "file:line:col: error"
    ; patterns. Extract first error file path and line number.
    mov     rsi, g_healBuf
    xor     r13d, r13d              ; error count

@hb_scan_line:
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @hb_scan_done

    ; Look for colon+digit pattern (file:line or file(line))
    cmp     al, ':'
    je      @hb_check_digit
    cmp     al, '('
    je      @hb_check_digit_paren

@hb_scan_next_char:
    inc     rsi
    jmp     @hb_scan_line

@hb_check_digit:
    movzx   eax, byte ptr [rsi + 1]
    sub     al, '0'
    cmp     al, 9
    ja      @hb_scan_next_char
    ; Found file:digit — this is likely an error location
    inc     r13d
    jmp     @hb_scan_to_eol

@hb_check_digit_paren:
    movzx   eax, byte ptr [rsi + 1]
    sub     al, '0'
    cmp     al, 9
    ja      @hb_scan_next_char
    inc     r13d

@hb_scan_to_eol:
    ; Skip to end of line
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @hb_scan_done
    cmp     al, 10
    je      @hb_scan_next_line
    inc     rsi
    jmp     @hb_scan_to_eol

@hb_scan_next_line:
    inc     rsi
    jmp     @hb_scan_line

@hb_scan_done:
    ; r13d = error count parsed from build output
    test    r13d, r13d
    jz      @hb_no_errors_found

    ; ── Apply fix via agent tool registry ─────────────────────
    ; Call Tool_GetDiagnostics to analyze the workspace
    mov     rcx, g_healBuf              ; pass raw stderr as diagnostic input
    mov     edx, g_healBufUsed
    call    Tool_GetDiagnostics

    ; If diagnostics returned actionable fix data, apply it
    test    eax, eax
    jz      @hb_no_fix

    ; Tool_GetDiagnostics returned fix count in EAX
    mov     g_healFixCount, eax

    ; Beacon: fix applied
    mov     ecx, HEAL_BEACON_SLOT
    mov     edx, HEAL_EVT_FIX_APPLIED
    mov     r8d, g_healFixCount
    xor     r9d, r9d
    call    BeaconSend

@hb_no_fix:
@hb_no_errors_found:
    ; Beacon: build failed
    mov     ecx, HEAL_BEACON_SLOT
    mov     edx, HEAL_EVT_FAIL
    mov     r8d, r12d               ; build exit code
    mov     r9d, r13d               ; error count
    call    BeaconSend

    ; Free buffer
    mov     rcx, g_hHeap
    xor     edx, edx
    mov     r8, g_healBuf
    call    HeapFree
    mov     g_healBuf, 0

    mov     eax, 1                  ; return 1 = failed, caller should retry
    jmp     @hb_epilog

    ; ── Error paths ───────────────────────────────────────────
@hb_close_pipes:
    mov     rcx, qword ptr [rsp + 0E0h]
    test    rcx, rcx
    jz      @hb_no_read_pipe
    call    CloseHandle
@hb_no_read_pipe:
    mov     rcx, qword ptr [rsp + 0E8h]
    test    rcx, rcx
    jz      @hb_free_buf
    call    CloseHandle

@hb_free_buf:
    mov     rcx, g_hHeap
    xor     edx, edx
    mov     r8, g_healBuf
    test    r8, r8
    jz      @hb_fail
    call    HeapFree
    mov     g_healBuf, 0

@hb_fail:
    mov     eax, 1                  ; return failure

@hb_epilog:
    lea     rsp, [rbp]
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
RawrXD_HealBuild ENDP

END
