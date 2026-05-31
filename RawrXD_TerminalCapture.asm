; =============================================================================
; RawrXD_TerminalCapture.asm
; Upgrade 1: Capture child-process stdout + stderr into an agent observation buffer
;
; Creates an anonymous pipe, spawns a child process with redirected handles,
; reads all output into a caller-supplied buffer, then feeds the total byte count
; back so the orchestrator can inject the result into the next Think prompt.
;
; PUBLIC API
;   PipeCapture_Run(pCmdLine, pOutBuf, dwBufSize, pdwBytesRead) -> DWORD
;       RCX  LPSTR       pCmdLine      – ANSI command; mutable (CreateProcessA may modify)
;       RDX  LPBYTE      pOutBuf       – caller buffer that receives captured output
;       R8   DWORD       dwBufSize     – byte capacity of pOutBuf
;       R9   LPDWORD     pdwBytesRead  – receives total bytes captured (may be NULL)
;       Returns 0 on success, Win32 error code on failure
; =============================================================================

OPTION CASEMAP:NONE

; ---- External Win32 APIs ----------------------------------------------------
EXTERN CreatePipe:PROC
EXTERN SetHandleInformation:PROC
EXTERN CreateProcessA:PROC
EXTERN WaitForSingleObject:PROC
EXTERN GetLastError:PROC
EXTERN CloseHandle:PROC
EXTERN ReadFile:PROC

; ---- Constants --------------------------------------------------------------
STARTF_USESTDHANDLES    EQU 0100h        ; si.dwFlags bit: use std handles
HANDLE_FLAG_INHERIT     EQU 1
CREATE_NO_WINDOW        EQU 08000000h    ; child has no console window
INFINITE_WAIT           EQU 0FFFFFFFFh

; ---- SECURITY_ATTRIBUTES field offsets (x64 – pointer-aligned layout) ------
SA_nLength              EQU  0           ; DWORD  (4 bytes)
;                            4           ; padding (4 bytes)
SA_lpSecDesc            EQU  8           ; QWORD pointer (8 bytes)
SA_bInheritHandle       EQU 16           ; BOOL / DWORD (4 bytes)
;                           20           ; padding (4 bytes)
SIZEOF_SA               EQU 24

; ---- STARTUPINFOA field offsets (x64) ---------------------------------------
SI_cb                   EQU   0          ; DWORD
;                             4          ; padding to 8
SI_lpReserved           EQU   8          ; QWORD
SI_lpDesktop            EQU  16          ; QWORD
SI_lpTitle              EQU  24          ; QWORD
SI_dwX                  EQU  32          ; DWORD
SI_dwY                  EQU  36          ; DWORD
SI_dwXSize              EQU  40          ; DWORD
SI_dwYSize              EQU  44          ; DWORD
SI_dwXCountChars        EQU  48          ; DWORD
SI_dwYCountChars        EQU  52          ; DWORD
SI_dwFillAttribute      EQU  56          ; DWORD
SI_dwFlags              EQU  60          ; DWORD
SI_wShowWindow          EQU  64          ; WORD
SI_cbReserved2          EQU  66          ; WORD
;                            68          ; padding to 72
SI_lpReserved2          EQU  72          ; QWORD
SI_hStdInput            EQU  80          ; QWORD (HANDLE)
SI_hStdOutput           EQU  88          ; QWORD (HANDLE)
SI_hStdError            EQU  96          ; QWORD (HANDLE)
SIZEOF_SI               EQU 104

; ---- PROCESS_INFORMATION field offsets (x64) --------------------------------
PI_hProcess             EQU   0          ; QWORD (HANDLE)
PI_hThread              EQU   8          ; QWORD (HANDLE)
PI_dwProcessId          EQU  16          ; DWORD
PI_dwThreadId           EQU  20          ; DWORD
SIZEOF_PI               EQU  24

; =============================================================================
; Stack layout for PipeCapture_Run
;
;   Function entry:  RSP = X - 8  (return addr pushed by CALL)
;   push rbx:        RSP = X - 16
;   sub rsp, 336:    RSP = X - 352   (352 % 16 == 0 => 16-byte aligned for inner calls)
;
;   [rsp +  0 ..  31]  32 bytes  – shadow space for callees
;   [rsp + 32 ..  79]  48 bytes  – stack arg slots for >4-arg calls (slots 5–10)
;   [rsp + 80 ..  87]   8 bytes  – saved pCmdLine  (RCX)
;   [rsp + 88 ..  95]   8 bytes  – saved pOutBuf   (RDX)
;   [rsp + 96 .. 103]   8 bytes  – saved dwBufSize (R8, stored as QWORD)
;   [rsp +104 .. 111]   8 bytes  – saved pdwBytesRead (R9)
;   [rsp +112 .. 119]   8 bytes  – qTotalRead
;   [rsp +120 .. 127]   8 bytes  – hRead
;   [rsp +128 .. 135]   8 bytes  – hWrite
;   [rsp +136 .. 143]   8 bytes  – cbNow (DWORD @ +136, pad @ +140)
;   [rsp +144 .. 167]  24 bytes  – SECURITY_ATTRIBUTES
;   [rsp +168 .. 271] 104 bytes  – STARTUPINFOA
;   [rsp +272 .. 295]  24 bytes  – PROCESS_INFORMATION
;   [rsp +296 .. 303]   8 bytes  – qRetVal
;   [rsp +304 .. 335]  32 bytes  – alignment padding to fill 336
; =============================================================================

.code

PipeCapture_Run PROC FRAME
    push rbx
    .pushreg rbx
    sub  rsp, 336
    .allocstack 336
    .endprolog

    ; ---- Persist volatile parameters (clobbered by Win32 calls below) ------
    mov  qword ptr [rsp + 80],  rcx     ; pCmdLine
    mov  qword ptr [rsp + 88],  rdx     ; pOutBuf
    mov  dword ptr [rsp + 96],  r8d     ; dwBufSize (32-bit; slot is 8 bytes)
    mov  qword ptr [rsp +104],  r9      ; pdwBytesRead

    ; ---- Initialise locals --------------------------------------------------
    xor  eax, eax
    mov  qword ptr [rsp +112], rax      ; qTotalRead = 0
    mov  qword ptr [rsp +120], rax      ; hRead      = 0 (not yet open)
    mov  qword ptr [rsp +128], rax      ; hWrite     = 0 (not yet open)

    ; ---- Zero SECURITY_ATTRIBUTES (3 QWORDs) --------------------------------
    mov  qword ptr [rsp +144], rax
    mov  qword ptr [rsp +152], rax
    mov  qword ptr [rsp +160], rax
    ; Set required fields
    mov  dword ptr [rsp +144 + SA_nLength],         SIZEOF_SA
    mov  dword ptr [rsp +144 + SA_bInheritHandle],  1   ; TRUE – child inherits

    ; ---- Zero STARTUPINFOA (13 QWORDs = 104 bytes) -------------------------
    lea  rbx, [rsp +168]
    mov  qword ptr [rbx +  0], rax
    mov  qword ptr [rbx +  8], rax
    mov  qword ptr [rbx + 16], rax
    mov  qword ptr [rbx + 24], rax
    mov  qword ptr [rbx + 32], rax
    mov  qword ptr [rbx + 40], rax
    mov  qword ptr [rbx + 48], rax
    mov  qword ptr [rbx + 56], rax
    mov  qword ptr [rbx + 64], rax
    mov  qword ptr [rbx + 72], rax
    mov  qword ptr [rbx + 80], rax
    mov  qword ptr [rbx + 88], rax
    mov  qword ptr [rbx + 96], rax
    ; Set required fields (rbx → &si)
    mov  dword ptr [rbx + SI_cb],      SIZEOF_SI
    mov  dword ptr [rbx + SI_dwFlags], STARTF_USESTDHANDLES

    ; ---- Zero PROCESS_INFORMATION (3 QWORDs) --------------------------------
    mov  qword ptr [rsp +272], rax
    mov  qword ptr [rsp +280], rax
    mov  qword ptr [rsp +288], rax

    ; ---- CreatePipe(&hRead, &hWrite, &sa, 0) --------------------------------
    lea  rcx, [rsp +120]            ; &hRead
    lea  rdx, [rsp +128]            ; &hWrite
    lea  r8,  [rsp +144]            ; &sa
    xor  r9d, r9d                   ; nSize = 0 (default pipe buffer)
    call CreatePipe
    test eax, eax
    jz   pcr_fail_lasterr

    ; ---- Mark the read end non-inheritable so child cannot hold it open -----
    mov  rcx, qword ptr [rsp +120]  ; hRead
    mov  edx, HANDLE_FLAG_INHERIT
    xor  r8d, r8d
    call SetHandleInformation
    ; failure is non-critical; continue regardless

    ; ---- Wire hWrite into STARTUPINFOA (rbx still → &si) -------------------
    mov  rax, qword ptr [rsp +128]  ; hWrite
    mov  qword ptr [rbx + SI_hStdOutput], rax
    mov  qword ptr [rbx + SI_hStdError],  rax
    ; si.hStdInput stays NULL – child inherits parent console stdin (safe default)

    ; ---- CreateProcessA (10 args: 4 in regs, 6 on stack) -------------------
    ; Stack slots [rsp+32..79] are reserved for the 5th–10th arguments
    mov  qword ptr [rsp + 32], 1                ; bInheritHandles = TRUE
    mov  qword ptr [rsp + 40], CREATE_NO_WINDOW ; dwCreationFlags
    mov  qword ptr [rsp + 48], 0                ; lpEnvironment      = NULL
    mov  qword ptr [rsp + 56], 0                ; lpCurrentDirectory = NULL
    lea  rax, [rsp +168]                         ; lpStartupInfo      = &si
    mov  qword ptr [rsp + 64], rax
    lea  rax, [rsp +272]                         ; lpProcessInformation = &pi
    mov  qword ptr [rsp + 72], rax
    ; Reg args
    xor  rcx, rcx                               ; lpApplicationName  = NULL
    mov  rdx, qword ptr [rsp + 80]              ; lpCommandLine      = pCmdLine
    xor  r8,  r8                                ; lpProcessAttributes = NULL
    xor  r9,  r9                                ; lpThreadAttributes = NULL
    call CreateProcessA
    test eax, eax
    jz   pcr_fail_lasterr

    ; ---- Close the write end in the parent ----------------------------------
    ; If we keep hWrite open, ReadFile will never see EOF.
    mov  rcx, qword ptr [rsp +128]
    call CloseHandle
    xor  eax, eax
    mov  qword ptr [rsp +128], rax              ; mark hWrite as closed

    ; ---- Read loop: drain the pipe into pOutBuf -----------------------------
pcr_read_loop:
    mov  rax, qword ptr [rsp +112]          ; qTotalRead
    mov  r10d, dword ptr [rsp + 96]         ; dwBufSize (zero-ext to 64-bit)
    cmp  rax, r10
    jge  pcr_read_done                      ; buffer full – stop

    mov  r11, qword ptr [rsp + 88]          ; pOutBuf
    add  r11, rax                           ; cursor = pOutBuf + totalRead

    ; remaining = bufSize - totalRead  (fits in DWORD since both are ≤ DWORD_MAX)
    sub  r10, rax                           ; r10 = remaining bytes

    ; Set cbNow = 0 before ReadFile
    xor  eax, eax
    mov  dword ptr [rsp +136], eax

    ; ReadFile(hRead, cursor, remaining, &cbNow, NULL)
    mov  rcx, qword ptr [rsp +120]          ; hRead
    mov  rdx, r11                           ; cursor
    mov  r8d, r10d                          ; remaining (DWORD)
    lea  r9,  [rsp +136]                    ; &cbNow
    mov  qword ptr [rsp +32], 0             ; lpOverlapped = NULL
    call ReadFile
    test eax, eax
    jz   pcr_read_done                      ; ERROR_BROKEN_PIPE or other = EOF

    mov   eax, dword ptr [rsp +136]         ; cbNow (mov eax auto-zero-extends to rax)
    test  eax, eax
    jz    pcr_read_done                     ; 0-byte read = true EOF

    add   qword ptr [rsp +112], rax         ; qTotalRead += cbNow
    jmp   pcr_read_loop

pcr_read_done:
    ; ---- Wait for child process to exit -------------------------------------
    mov  rcx, qword ptr [rsp +272]          ; pi.hProcess
    test rcx, rcx
    jz   pcr_write_result
    mov  edx, INFINITE_WAIT
    call WaitForSingleObject

pcr_write_result:
    ; ---- Store total bytes back through pdwBytesRead pointer ----------------
    mov  r10, qword ptr [rsp +104]          ; pdwBytesRead
    test r10, r10
    jz   pcr_null_term
    mov  rax, qword ptr [rsp +112]
    mov  dword ptr [r10], eax

pcr_null_term:
    ; ---- Null-terminate output buffer (if space available) ------------------
    mov  rax, qword ptr [rsp +112]          ; totalRead
    mov  r10d, dword ptr [rsp + 96]         ; bufSize
    cmp  rax, r10
    jge  pcr_success
    mov  r11, qword ptr [rsp + 88]
    add  r11, rax
    mov  byte ptr [r11], 0

pcr_success:
    mov  qword ptr [rsp +296], 0            ; retVal = 0 (success)
    jmp  pcr_cleanup

pcr_fail_lasterr:
    call GetLastError
    mov  qword ptr [rsp +296], rax          ; retVal = Win32 error code

    ; ---- Cleanup: close all open handles ------------------------------------
pcr_cleanup:
    mov  rcx, qword ptr [rsp +120]          ; hRead
    test rcx, rcx
    jz   pcr_cl_write
    call CloseHandle

pcr_cl_write:
    mov  rcx, qword ptr [rsp +128]          ; hWrite (0 if already closed)
    test rcx, rcx
    jz   pcr_cl_proc
    call CloseHandle

pcr_cl_proc:
    mov  rcx, qword ptr [rsp +272]          ; pi.hProcess
    test rcx, rcx
    jz   pcr_cl_thread
    call CloseHandle

pcr_cl_thread:
    mov  rcx, qword ptr [rsp +280]          ; pi.hThread
    test rcx, rcx
    jz   pcr_done
    call CloseHandle

pcr_done:
    mov  rax, qword ptr [rsp +296]          ; load final return value
    add  rsp, 336
    pop  rbx
    ret

PipeCapture_Run ENDP

PUBLIC PipeCapture_Run

END
