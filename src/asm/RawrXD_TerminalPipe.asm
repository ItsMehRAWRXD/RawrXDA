; =============================================================================
; RawrXD_TerminalPipe.asm — Terminal Pipe + Observation Ring Buffer + Tool
;                            Dispatch Jump Table
; =============================================================================
; Production x64 MASM module for the Agentic Orchestrator "Level 2" nervous
; system.  Replaces the C++ CreateProcess/pipe/ReadFile hot path with
; zero‑overhead Win32 syscall wrappers, a lock‑free circular observation
; buffer, and a branch‑free tool dispatch jump table.
;
; Build (standalone verify):
;   ml64.exe /c /Zi /Zd /I src\asm RawrXD_TerminalPipe.asm
;
; Link: object is consumed by the Win32IDE target via CMake ASM_MASM.
; =============================================================================

OPTION CASEMAP:NONE

; ---------------------------------------------------------------------------
; Project includes (shared constants, macros, Win64 extern decls)
; ---------------------------------------------------------------------------
INCLUDE RawrXD_Common.inc
INCLUDE rawrxd_win64.inc

; ---------------------------------------------------------------------------
; Additional Win32 API imports not in common includes
; ---------------------------------------------------------------------------
EXTERNDEF CreateProcessA:PROC
EXTERNDEF PeekNamedPipe:PROC
EXTERNDEF ReadFile:PROC
EXTERNDEF CreatePipe:PROC
EXTERNDEF SetHandleInformation:PROC
EXTERNDEF WaitForSingleObject:PROC
EXTERNDEF GetExitCodeProcess:PROC
EXTERNDEF TerminateProcess:PROC
EXTERNDEF GetProcessHeap:PROC
EXTERNDEF HeapAlloc:PROC
EXTERNDEF HeapFree:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF QueryPerformanceCounter:PROC
EXTERNDEF InitializeCriticalSection:PROC
EXTERNDEF EnterCriticalSection:PROC
EXTERNDEF LeaveCriticalSection:PROC
EXTERNDEF DeleteCriticalSection:PROC
EXTERNDEF MessageBoxA:PROC

; ---------------------------------------------------------------------------
; Constants
; ---------------------------------------------------------------------------
; Observation ring buffer
OBS_RING_SIZE           EQU 4096        ; power‑of‑2 for fast masking
OBS_RING_MASK           EQU (OBS_RING_SIZE - 1)
OBS_SNAPSHOT_SIZE       EQU 2048        ; max chars returned to LLM context

; Pipe buffer sizes
PIPE_BUF_SIZE           EQU 4096

; Tool dispatch
MAX_TOOLS               EQU 32

; Process creation
STARTF_USESTDHANDLES    EQU 00000100h
CREATE_NO_WINDOW        EQU 08000000h
HANDLE_FLAG_INHERIT     EQU 00000001h
WAIT_TIMEOUT            EQU 00000102h
STILL_ACTIVE            EQU 00000103h

; MessageBox buttons / return
MB_YESNO                EQU 00000004h
IDYES                   EQU 6

; Heap flags
HEAP_ZERO_MEMORY        EQU 00000008h

; ---------------------------------------------------------------------------
; Structures
; ---------------------------------------------------------------------------

; Win64 SECURITY_ATTRIBUTES (24 bytes)
SECURITY_ATTRIBUTES STRUCT
    nLength             DWORD ?
    lpSecurityDescriptor QWORD ?
    bInheritHandle      DWORD ?
    _pad0               DWORD ?         ; alignment
SECURITY_ATTRIBUTES ENDS

; Win64 STARTUPINFOA (104 bytes)
STARTUPINFOA STRUCT
    cb              DWORD ?
    _pad0           DWORD ?
    lpReserved      QWORD ?
    lpDesktop       QWORD ?
    lpTitle         QWORD ?
    dwX             DWORD ?
    dwY             DWORD ?
    dwXSize         DWORD ?
    dwYSize         DWORD ?
    dwXCountChars   DWORD ?
    dwYCountChars   DWORD ?
    dwFillAttribute DWORD ?
    dwFlags         DWORD ?
    wShowWindow     WORD  ?
    cbReserved2     WORD  ?
    _pad1           DWORD ?
    lpReserved2     QWORD ?
    hStdInput       QWORD ?
    hStdOutput      QWORD ?
    hStdError       QWORD ?
STARTUPINFOA ENDS

; Win64 PROCESS_INFORMATION (24 bytes)
PROCESS_INFORMATION STRUCT
    hProcess    QWORD ?
    hThread     QWORD ?
    dwProcessId DWORD ?
    dwThreadId  DWORD ?
PROCESS_INFORMATION ENDS

; Tool entry for the dispatch jump table
TOOL_DISPATCH_ENTRY STRUCT
    nameHash    QWORD ?                 ; FNV‑1a hash of tool name
    handler     QWORD ?                 ; pointer to handler proc
TOOL_DISPATCH_ENTRY ENDS

; =============================================================================
;                               DATA
; =============================================================================
.DATA
ALIGN 16

; --- Observation Ring Buffer ------------------------------------------------
g_obsRing           DB OBS_RING_SIZE DUP(0)
g_obsHead           QWORD 0             ; write cursor (producer)
g_obsTail           QWORD 0             ; read cursor (consumer)
g_obsLock           DB 40 DUP(0)        ; CRITICAL_SECTION (40 bytes on x64)
g_obsInitialized    DWORD 0

; --- Snapshot scratch buffer ------------------------------------------------
g_obsSnapshot       DB OBS_SNAPSHOT_SIZE DUP(0)

; --- Pipe handles for child process ----------------------------------------
g_hChildStdOutRead  QWORD 0
g_hChildStdOutWrite QWORD 0
g_hChildStdErrRead  QWORD 0
g_hChildStdErrWrite QWORD 0
g_hChildProcess     QWORD 0
g_hChildThread      QWORD 0
g_childExitCode     DWORD 0

; --- Pipe read scratch buffer -----------------------------------------------
g_pipeBuf           DB PIPE_BUF_SIZE DUP(0)

; --- Tool Dispatch Table ----------------------------------------------------
g_toolTable         TOOL_DISPATCH_ENTRY MAX_TOOLS DUP(<>)
g_toolCount         DWORD 0

; --- HITL gate strings ------------------------------------------------------
szHITL_Caption      DB "Agent Safety Gate", 0
szHITL_Text         DB "The agent wants to execute a command.", 13, 10
                    DB "Allow this action?", 0

; =============================================================================
;                               CODE
; =============================================================================
.CODE

; =====================================================================
; RawrXD_ObsRing_Init
; =====================================================================
; Initialize the observation ring buffer and its lock.
; No args.  Returns RAX = 0 on success.
; =====================================================================
RawrXD_ObsRing_Init PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 32
    .allocstack 32
    .endprolog

    cmp     g_obsInitialized, 0
    jnz     @already

    ; Zero head / tail
    xor     rax, rax
    mov     g_obsHead, rax
    mov     g_obsTail, rax

    ; InitializeCriticalSection(&g_obsLock)
    lea     rcx, g_obsLock
    call    InitializeCriticalSection

    mov     g_obsInitialized, 1
    xor     rax, rax
    jmp     @done

@already:
    xor     rax, rax

@done:
    add     rsp, 32
    pop     rbp
    ret
RawrXD_ObsRing_Init ENDP

; =====================================================================
; RawrXD_ObsRing_Push
; =====================================================================
; Append bytes to the observation ring (thread‑safe).
; RCX = pointer to data
; RDX = byte count
; Returns RAX = bytes actually written.
; =====================================================================
RawrXD_ObsRing_Push PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rsi, rcx            ; src pointer
    mov     rdi, rdx            ; byte count

    ; EnterCriticalSection(&g_obsLock)
    lea     rcx, g_obsLock
    call    EnterCriticalSection

    ; Write byte‑by‑byte into ring (simple & correct; hot‑path is < 4 KB)
    xor     rbx, rbx            ; bytes written counter
    test    rdi, rdi
    jz      @push_unlock

    lea     r8, g_obsRing       ; RIP‑relative base address

@push_loop:
    cmp     rbx, rdi
    jae     @push_unlock

    ; Load write cursor, mask, write byte, advance
    mov     rax, g_obsHead
    and     rax, OBS_RING_MASK
    movzx   ecx, BYTE PTR [rsi + rbx]
    mov     BYTE PTR [r8 + rax], cl
    inc     g_obsHead

    ; If head catches tail, advance tail (oldest byte dropped)
    mov     rax, g_obsHead
    sub     rax, g_obsTail
    cmp     rax, OBS_RING_SIZE
    jb      @push_no_wrap
    inc     g_obsTail
@push_no_wrap:
    inc     rbx
    jmp     @push_loop

@push_unlock:
    ; LeaveCriticalSection(&g_obsLock)
    lea     rcx, g_obsLock
    call    LeaveCriticalSection

    mov     rax, rbx            ; return bytes written

    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
RawrXD_ObsRing_Push ENDP

; =====================================================================
; RawrXD_ObsRing_Snapshot
; =====================================================================
; Copy the last N bytes (up to OBS_SNAPSHOT_SIZE) from the ring into
; g_obsSnapshot and return a pointer + length.
; No args.
; Returns: RAX = pointer to snapshot buffer (null‑terminated)
;          RDX = byte count copied (excluding NUL)
; =====================================================================
RawrXD_ObsRing_Snapshot PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; EnterCriticalSection
    lea     rcx, g_obsLock
    call    EnterCriticalSection

    ; Compute available bytes
    mov     rax, g_obsHead
    sub     rax, g_obsTail
    ; Clamp to snapshot size - 1 (leave room for NUL)
    mov     rcx, OBS_SNAPSHOT_SIZE - 1
    cmp     rax, rcx
    cmova   rax, rcx
    mov     rbx, rax            ; rbx = count to copy

    ; Determine start position in ring
    mov     rsi, g_obsHead
    sub     rsi, rbx            ; start = head - count

    ; Copy into g_obsSnapshot
    lea     rdi, g_obsSnapshot
    lea     r8, g_obsRing       ; RIP‑relative base address
    xor     rcx, rcx
@snap_loop:
    cmp     rcx, rbx
    jae     @snap_done
    mov     rax, rsi
    add     rax, rcx
    and     rax, OBS_RING_MASK
    movzx   edx, BYTE PTR [r8 + rax]
    mov     BYTE PTR [rdi + rcx], dl
    inc     rcx
    jmp     @snap_loop

@snap_done:
    ; NUL‑terminate
    mov     BYTE PTR [rdi + rbx], 0

    ; LeaveCriticalSection
    lea     rcx, g_obsLock
    call    LeaveCriticalSection

    lea     rax, g_obsSnapshot
    mov     rdx, rbx

    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
RawrXD_ObsRing_Snapshot ENDP

; =====================================================================
; RawrXD_ObsRing_Destroy
; =====================================================================
; Tear down the observation ring lock.
; =====================================================================
RawrXD_ObsRing_Destroy PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 32
    .allocstack 32
    .endprolog

    cmp     g_obsInitialized, 0
    je      @obs_destroy_done
    lea     rcx, g_obsLock
    call    DeleteCriticalSection
    mov     g_obsInitialized, 0

@obs_destroy_done:
    add     rsp, 32
    pop     rbp
    ret
RawrXD_ObsRing_Destroy ENDP

; =====================================================================
; RawrXD_TermPipe_Execute
; =====================================================================
; Execute a command line and capture stdout+stderr into the observation
; ring buffer.  This is the "Level 2 perception" hot path.
;
; RCX = pointer to null‑terminated command string (ANSI)
; RDX = timeout in milliseconds (0 = INFINITE)
; Returns: RAX = child exit code, or -1 on CreateProcess failure
; =====================================================================
RawrXD_TermPipe_Execute PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
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
    ; 7 pushes + return addr = 64 bytes (16‑aligned)
    ; Need space for: SECURITY_ATTRIBUTES (24) + STARTUPINFOA (104) +
    ; PROCESS_INFORMATION (24) + locals (64) + shadow (32) = 248
    ; Round up to 256 (256 mod 16 = 0 → stays aligned)
    sub     rsp, 256
    .allocstack 256
    .endprolog

    mov     r12, rcx            ; r12 = command line
    mov     r13, rdx            ; r13 = timeout ms

    ; ---------------------------------------------------------------
    ; Create stdout pipe
    ; ---------------------------------------------------------------
    ; SECURITY_ATTRIBUTES at [rbp-24]
    lea     rax, [rbp - 24]
    mov     DWORD PTR [rax], 24                     ; nLength
    mov     QWORD PTR [rax + 8], 0                  ; lpSecurityDescriptor = NULL
    mov     DWORD PTR [rax + 16], 1                 ; bInheritHandle = TRUE

    ; CreatePipe(&hReadOut, &hWriteOut, &sa, PIPE_BUF_SIZE)
    lea     rcx, g_hChildStdOutRead
    lea     rdx, g_hChildStdOutWrite
    lea     r8, [rbp - 24]          ; &sa
    mov     r9d, PIPE_BUF_SIZE
    call    CreatePipe
    test    eax, eax
    jz      @exec_fail

    ; CreatePipe(&hReadErr, &hWriteErr, &sa, PIPE_BUF_SIZE)
    lea     rcx, g_hChildStdErrRead
    lea     rdx, g_hChildStdErrWrite
    lea     r8, [rbp - 24]
    mov     r9d, PIPE_BUF_SIZE
    call    CreatePipe
    test    eax, eax
    jz      @exec_fail_close_out

    ; Ensure the read handles are NOT inherited
    ; SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0)
    mov     rcx, g_hChildStdOutRead
    mov     edx, HANDLE_FLAG_INHERIT
    xor     r8d, r8d
    call    SetHandleInformation

    mov     rcx, g_hChildStdErrRead
    mov     edx, HANDLE_FLAG_INHERIT
    xor     r8d, r8d
    call    SetHandleInformation

    ; ---------------------------------------------------------------
    ; Fill STARTUPINFOA at [rbp-128]
    ; ---------------------------------------------------------------
    lea     rdi, [rbp - 128]
    ; Zero the struct (104 bytes)
    xor     eax, eax
    mov     ecx, 104
    lea     rdi, [rbp - 128]
@zero_si:
    mov     BYTE PTR [rdi], al
    inc     rdi
    dec     ecx
    jnz     @zero_si

    lea     rdi, [rbp - 128]
    mov     DWORD PTR [rdi], 104                        ; cb = sizeof(STARTUPINFOA)
    mov     DWORD PTR [rdi + STARTUPINFOA.dwFlags], STARTF_USESTDHANDLES
    mov     rax, g_hChildStdOutWrite
    mov     QWORD PTR [rdi + STARTUPINFOA.hStdOutput], rax
    mov     rax, g_hChildStdErrWrite
    mov     QWORD PTR [rdi + STARTUPINFOA.hStdError], rax

    ; ---------------------------------------------------------------
    ; Fill PROCESS_INFORMATION at [rbp-160]
    ; ---------------------------------------------------------------
    lea     rbx, [rbp - 160]
    xor     eax, eax
    mov     QWORD PTR [rbx], rax
    mov     QWORD PTR [rbx + 8], rax
    mov     QWORD PTR [rbx + 16], rax

    ; ---------------------------------------------------------------
    ; CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW,
    ;                NULL, NULL, &si, &pi)
    ; ---------------------------------------------------------------
    ; 10 args: first 4 in registers, rest on stack
    sub     rsp, 96                 ; shadow(32) + 6 stack args (48) + align(16)
    xor     rcx, rcx                ; lpApplicationName = NULL
    mov     rdx, r12                ; lpCommandLine
    xor     r8, r8                  ; lpProcessAttributes = NULL
    xor     r9, r9                  ; lpThreadAttributes = NULL
    mov     QWORD PTR [rsp + 32], 1             ; bInheritHandles = TRUE
    mov     QWORD PTR [rsp + 40], CREATE_NO_WINDOW ; dwCreationFlags
    mov     QWORD PTR [rsp + 48], 0            ; lpEnvironment = NULL
    mov     QWORD PTR [rsp + 56], 0            ; lpCurrentDirectory = NULL
    lea     rax, [rbp - 128]
    mov     QWORD PTR [rsp + 64], rax          ; lpStartupInfo
    lea     rax, [rbp - 160]
    mov     QWORD PTR [rsp + 72], rax          ; lpProcessInformation
    call    CreateProcessA
    add     rsp, 96
    test    eax, eax
    jz      @exec_fail_close_all

    ; Save handles
    lea     rbx, [rbp - 160]
    mov     rax, QWORD PTR [rbx + PROCESS_INFORMATION.hProcess]
    mov     g_hChildProcess, rax
    mov     rax, QWORD PTR [rbx + PROCESS_INFORMATION.hThread]
    mov     g_hChildThread, rax

    ; Close write ends so ReadFile will get EOF when child exits
    mov     rcx, g_hChildStdOutWrite
    call    CloseHandle
    mov     g_hChildStdOutWrite, 0
    mov     rcx, g_hChildStdErrWrite
    call    CloseHandle
    mov     g_hChildStdErrWrite, 0

    ; ---------------------------------------------------------------
    ; Read loop: drain stdout + stderr into observation ring
    ; ---------------------------------------------------------------
@read_loop:
    ; Check stdout
    lea     r14, g_pipeBuf
    xor     r15d, r15d          ; r15 = bytesRead

    ; PeekNamedPipe(hReadOut, NULL, 0, NULL, &avail, NULL)
    sub     rsp, 48
    mov     rcx, g_hChildStdOutRead
    xor     rdx, rdx
    xor     r8d, r8d
    xor     r9, r9
    lea     rax, [rbp - 168]    ; local avail dword
    mov     QWORD PTR [rsp + 32], rax
    mov     QWORD PTR [rsp + 40], 0
    call    PeekNamedPipe
    add     rsp, 48
    test    eax, eax
    jz      @check_stderr

    mov     eax, DWORD PTR [rbp - 168]
    test    eax, eax
    jz      @check_stderr

    ; ReadFile(hReadOut, g_pipeBuf, PIPE_BUF_SIZE, &bytesRead, NULL)
    sub     rsp, 48
    mov     rcx, g_hChildStdOutRead
    lea     rdx, g_pipeBuf
    mov     r8d, PIPE_BUF_SIZE
    lea     r9, [rbp - 176]     ; &bytesRead
    mov     QWORD PTR [rsp + 32], 0
    call    ReadFile
    add     rsp, 48
    test    eax, eax
    jz      @check_stderr

    ; Push to observation ring
    lea     rcx, g_pipeBuf
    mov     edx, DWORD PTR [rbp - 176]
    ; edx write zero-extends into rdx automatically on x64
    call    RawrXD_ObsRing_Push

@check_stderr:
    ; PeekNamedPipe on stderr
    sub     rsp, 48
    mov     rcx, g_hChildStdErrRead
    xor     rdx, rdx
    xor     r8d, r8d
    xor     r9, r9
    lea     rax, [rbp - 168]
    mov     QWORD PTR [rsp + 32], rax
    mov     QWORD PTR [rsp + 40], 0
    call    PeekNamedPipe
    add     rsp, 48
    test    eax, eax
    jz      @check_process

    mov     eax, DWORD PTR [rbp - 168]
    test    eax, eax
    jz      @check_process

    ; ReadFile stderr
    sub     rsp, 48
    mov     rcx, g_hChildStdErrRead
    lea     rdx, g_pipeBuf
    mov     r8d, PIPE_BUF_SIZE
    lea     r9, [rbp - 176]
    mov     QWORD PTR [rsp + 32], 0
    call    ReadFile
    add     rsp, 48
    test    eax, eax
    jz      @check_process

    ; Push stderr into same ring (agent sees everything)
    lea     rcx, g_pipeBuf
    mov     edx, DWORD PTR [rbp - 176]
    ; edx write zero-extends into rdx automatically on x64
    call    RawrXD_ObsRing_Push

@check_process:
    ; Is the child still alive?
    mov     rcx, g_hChildProcess
    xor     edx, edx            ; 0 ms = poll
    call    WaitForSingleObject
    cmp     eax, WAIT_TIMEOUT
    je      @read_loop          ; still running → keep draining

    ; Process exited — do one final drain
    ; (ReadFile will return FALSE once pipe is empty + child gone)

    ; GetExitCodeProcess
    mov     rcx, g_hChildProcess
    lea     rdx, g_childExitCode
    call    GetExitCodeProcess

    ; ---------------------------------------------------------------
    ; Cleanup handles
    ; ---------------------------------------------------------------
    mov     rcx, g_hChildStdOutRead
    call    CloseHandle
    mov     g_hChildStdOutRead, 0
    mov     rcx, g_hChildStdErrRead
    call    CloseHandle
    mov     g_hChildStdErrRead, 0
    mov     rcx, g_hChildThread
    call    CloseHandle
    mov     g_hChildThread, 0
    mov     rcx, g_hChildProcess
    call    CloseHandle
    mov     g_hChildProcess, 0

    ; Return exit code
    mov     eax, g_childExitCode
    movsxd  rax, eax
    jmp     @exec_done

@exec_fail_close_all:
    mov     rcx, g_hChildStdErrRead
    call    CloseHandle
    mov     rcx, g_hChildStdErrWrite
    call    CloseHandle
@exec_fail_close_out:
    mov     rcx, g_hChildStdOutRead
    call    CloseHandle
    mov     rcx, g_hChildStdOutWrite
    call    CloseHandle
@exec_fail:
    mov     rax, -1

@exec_done:
    add     rsp, 256
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rbx
    pop     rbp
    ret
RawrXD_TermPipe_Execute ENDP

; =====================================================================
; RawrXD_HITL_Gate
; =====================================================================
; Human‑In‑The‑Loop safety gate.  Shows a Yes/No message box.
; RCX = hWndParent (0 for desktop)
; RDX = pointer to description string (ANSI)
; Returns: RAX = 1 if approved, 0 if denied
; =====================================================================
RawrXD_HITL_Gate PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; MessageBoxA(hWnd, lpText, lpCaption, MB_YESNO)
    ; rcx = hWnd (already in RCX)
    ; rdx = text (already in RDX)
    lea     r8, szHITL_Caption
    mov     r9d, MB_YESNO
    call    MessageBoxA

    cmp     eax, IDYES
    sete    al
    movzx   eax, al

    add     rsp, 32
    pop     rbp
    ret
RawrXD_HITL_Gate ENDP

; =====================================================================
; FNV‑1a 64‑bit Hash  (internal helper)
; =====================================================================
; RCX = pointer to null‑terminated string
; Returns RAX = 64‑bit hash
; =====================================================================
FNV1a_Hash64 PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .endprolog

    mov     rax, 0CBF29CE484222325h     ; FNV offset basis
    mov     r8,  00000100000001B3h      ; FNV prime

@fnv_loop:
    movzx   edx, BYTE PTR [rcx]
    test    dl, dl
    jz      @fnv_done
    xor     al, dl
    imul    rax, r8
    inc     rcx
    jmp     @fnv_loop

@fnv_done:
    pop     rbp
    ret
FNV1a_Hash64 ENDP

; =====================================================================
; RawrXD_ToolTable_Register
; =====================================================================
; Register a tool handler in the dispatch jump table.
; RCX = pointer to tool name (null‑terminated ANSI)
; RDX = pointer to handler proc
; Returns: RAX = slot index, or -1 if table full
; =====================================================================
RawrXD_ToolTable_Register PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    mov     rbp, rsp
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     r12, rdx            ; save handler ptr

    ; Hash the tool name
    ; rcx already = name ptr
    call    FNV1a_Hash64
    mov     rbx, rax            ; rbx = nameHash

    ; Check capacity
    mov     eax, g_toolCount
    cmp     eax, MAX_TOOLS
    jae     @reg_full

    ; Write entry
    mov     ecx, eax
    imul    ecx, SIZEOF TOOL_DISPATCH_ENTRY
    lea     rdx, g_toolTable
    add     rdx, rcx
    mov     QWORD PTR [rdx + TOOL_DISPATCH_ENTRY.nameHash], rbx
    mov     QWORD PTR [rdx + TOOL_DISPATCH_ENTRY.handler], r12

    ; Return index, then increment count
    mov     eax, g_toolCount
    inc     g_toolCount

    add     rsp, 32
    pop     r12
    pop     rbx
    pop     rbp
    ret

@reg_full:
    mov     rax, -1
    add     rsp, 32
    pop     r12
    pop     rbx
    pop     rbp
    ret
RawrXD_ToolTable_Register ENDP

; =====================================================================
; RawrXD_ToolTable_Dispatch
; =====================================================================
; Look up a tool by name hash and jump to its handler.  This replaces
; the C++ if/else-if chain with a table scan + indirect call.
;
; RCX = pointer to tool name (null‑terminated ANSI)
; RDX = pointer to argument struct (tool‑specific, passed through)
; Returns: RAX = handler return value, or -1 if tool not found
; =====================================================================
RawrXD_ToolTable_Dispatch PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    mov     rbp, rsp
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     r12, rdx            ; save arg pointer

    ; Hash the requested tool name
    call    FNV1a_Hash64
    mov     rbx, rax            ; rbx = target hash

    ; Scan table
    xor     ecx, ecx
    lea     r13, g_toolTable
@disp_scan:
    cmp     ecx, g_toolCount
    jae     @disp_not_found

    mov     rax, QWORD PTR [r13 + TOOL_DISPATCH_ENTRY.nameHash]
    cmp     rax, rbx
    je      @disp_found

    add     r13, SIZEOF TOOL_DISPATCH_ENTRY
    inc     ecx
    jmp     @disp_scan

@disp_found:
    ; Call handler(arg)
    mov     rcx, r12            ; arg pointer
    call    QWORD PTR [r13 + TOOL_DISPATCH_ENTRY.handler]
    jmp     @disp_done

@disp_not_found:
    mov     rax, -1

@disp_done:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret
RawrXD_ToolTable_Dispatch ENDP

; =====================================================================
; RawrXD_TermPipe_ExecuteWithHITL
; =====================================================================
; High‑level entry:  HITL gate → execute → observation snapshot.
; Wraps the full "Act + Observe" half of the agentic loop.
;
; RCX = hWndParent (for MessageBox; 0 = desktop)
; RDX = command line (ANSI)
; R8  = timeout ms (0 = INFINITE)
; Returns: RAX = exit code (-1 fail, -2 denied by HITL)
;          On success, observation snapshot is in g_obsSnapshot.
; =====================================================================
RawrXD_TermPipe_ExecuteWithHITL PROC FRAME
    push    rbp
    .pushreg rbp
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    mov     rbp, rsp
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     r12, rcx            ; hWnd
    mov     r13, rdx            ; cmdLine
    mov     r14, r8             ; timeout

    ; HITL gate
    mov     rcx, r12
    mov     rdx, r13            ; show the command as description
    call    RawrXD_HITL_Gate
    test    eax, eax
    jz      @hitl_denied

    ; Execute
    mov     rcx, r13
    mov     rdx, r14
    call    RawrXD_TermPipe_Execute
    mov     r12, rax            ; save exit code

    ; Take snapshot for LLM context
    call    RawrXD_ObsRing_Snapshot
    ; RAX = snapshot ptr, RDX = length (caller can use these)

    mov     rax, r12            ; return exit code
    jmp     @hitl_done

@hitl_denied:
    mov     rax, -2

@hitl_done:
    add     rsp, 32
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    ret
RawrXD_TermPipe_ExecuteWithHITL ENDP

END
