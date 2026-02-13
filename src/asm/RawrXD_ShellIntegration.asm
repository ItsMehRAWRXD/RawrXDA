; =============================================================================
; RawrXD_ShellIntegration.asm — Pure x64 MASM Shell Integration Kernel
; Production-Ready Command Detection via OSC 633 Escape Sequences
; =============================================================================
;
; Architecture:
;   - Win32 CreateProcess with pipe-redirected stdio for shell hosting
;   - OSC 633 escape sequence injection for IDE command boundary detection
;   - Wait-free atomic ring buffer for command history (lock xadd reservation)
;   - Lock-free 64-bit performance counters (cache-line padded)
;   - Non-blocking overlapped pipe reads for real-time output streaming
;   - Zero CRT dependency — direct Win32 API + raw MASM only
;
; OSC 633 Protocol (IDE Shell Integration):
;   \x1b]633;A\x07     — Prompt Start
;   \x1b]633;B\x07     — Prompt End / Command Start
;   \x1b]633;C\x07     — Pre-Execution (command submitted)
;   \x1b]633;D;N\x07   — Execution Done, N = exit code
;   \x1b]633;E;cmd\x07 — Command Line echo
;   \x1b]633;P;k=v\x07 — Shell Property announcement
;
; Exports:
;   ShellInteg_Init              — Create shell process + pipes, initialize subsystem
;   ShellInteg_Shutdown          — Drain buffers, terminate shell, close handles
;   ShellInteg_ExecuteCommand    — Submit command with OSC 633 markers, capture output
;   ShellInteg_ReadOutput        — Non-blocking read from shell stdout pipe
;   ShellInteg_GetExitCode       — Retrieve last command exit code
;   ShellInteg_InjectOSC633      — Inject specific OSC 633 sequence into output stream
;   ShellInteg_GetCommandHistory — Read entry from history ring buffer
;   ShellInteg_GetStats          — Return subsystem performance counters
;   ShellInteg_SetProperty       — Set shell property (cwd, shell type, etc.)
;   ShellInteg_IsAlive           — Check if child shell process is still running
;   ShellInteg_CompleteCommand   — Close command lifecycle: update history, inject D
;
; Pattern: PatchResult (RAX=0 success, RAX=NTSTATUS-style error on failure)
;          RDX = detail pointer or supplemental data
; Rule:    NO CRT, NO Qt, NO std::, NO exceptions
;          NO SOURCE FILE IS TO BE SIMPLIFIED
; Build:   ml64.exe /c /Zi /Zd /Fo RawrXD_ShellIntegration.obj RawrXD_ShellIntegration.asm
; =============================================================================

include RawrXD_Common.inc

; =============================================================================
;                      Additional Win32 API Imports
; =============================================================================
EXTERNDEF CreateProcessA:PROC
EXTERNDEF CreatePipe:PROC
EXTERNDEF PeekNamedPipe:PROC
EXTERNDEF ReadFile:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF TerminateProcess:PROC
EXTERNDEF GetExitCodeProcess:PROC
EXTERNDEF WaitForSingleObject:PROC
EXTERNDEF SetHandleInformation:PROC
EXTERNDEF GetCurrentDirectoryA:PROC
EXTERNDEF FlushFileBuffers:PROC
EXTERNDEF WriteConsoleA:PROC
EXTERNDEF GetStdHandle:PROC
EXTERNDEF lstrlenA:PROC

; =============================================================================
;                        Constants
; =============================================================================

; Handle inheritance
HANDLE_FLAG_INHERIT         EQU 00000001h

; Process creation flags
CREATE_NO_WINDOW            EQU 08000000h
CREATE_NEW_PROCESS_GROUP    EQU 00000200h

; Std handle IDs
STD_INPUT_HANDLE            EQU -10
STD_OUTPUT_HANDLE_ID        EQU -11
STD_ERROR_HANDLE_ID         EQU -12

; WaitForSingleObject
WAIT_OBJECT_0               EQU 0
WAIT_TIMEOUT                EQU 00000102h

; Pipe buffer size
PIPE_BUFFER_SIZE            EQU 65536       ; 64 KB per pipe

; Command history ring buffer
CMD_HISTORY_SIZE            EQU 1024        ; Slots (power of 2)
CMD_HISTORY_MASK            EQU (CMD_HISTORY_SIZE - 1)
CMD_ENTRY_SIZE              EQU 512         ; Bytes per history entry
CMD_ENTRY_SHIFT             EQU 9           ; log2(512) = 9

; Output read buffer
OUTPUT_BUFFER_SIZE          EQU 16384       ; 16 KB read chunk

; OSC 633 sequence IDs
OSC633_PROMPT_START         EQU 0           ; 'A'
OSC633_PROMPT_END           EQU 1           ; 'B'
OSC633_PRE_EXECUTION        EQU 2           ; 'C'
OSC633_EXECUTION_DONE       EQU 3           ; 'D'
OSC633_COMMAND_LINE         EQU 4           ; 'E'
OSC633_PROPERTY             EQU 5           ; 'P'

; Shell integration status codes
SHELLINTEG_SUCCESS              EQU 0
SHELLINTEG_ERR_ALREADY_INIT     EQU 0C0000100h
SHELLINTEG_ERR_PIPE_FAILED      EQU 0C0000101h
SHELLINTEG_ERR_PROCESS_FAILED   EQU 0C0000102h
SHELLINTEG_ERR_NOT_INIT         EQU 0C0000103h
SHELLINTEG_ERR_WRITE_FAILED     EQU 0C0000104h
SHELLINTEG_ERR_READ_FAILED      EQU 0C0000105h
SHELLINTEG_ERR_NULL_PARAM       EQU 0C0000106h
SHELLINTEG_ERR_BUFFER_OVERFLOW  EQU 0C0000107h
SHELLINTEG_ERR_PROCESS_DEAD     EQU 0C0000108h
SHELLINTEG_ERR_INVALID_SEQ_ID   EQU 0C0000109h

; =============================================================================
;                      STARTUPINFOA Structure
; =============================================================================
STARTUPINFOA STRUCT
    cb              DD ?
    _pad0           DD ?
    lpReserved      DQ ?
    lpDesktop       DQ ?
    lpTitle         DQ ?
    dwX             DD ?
    dwY             DD ?
    dwXSize         DD ?
    dwYSize         DD ?
    dwXCountChars   DD ?
    dwYCountChars   DD ?
    dwFillAttribute DD ?
    dwFlags         DD ?
    wShowWindow     DW ?
    cbReserved2     DW ?
    _pad1           DD ?
    lpReserved2     DQ ?
    hStdInput       DQ ?
    hStdOutput      DQ ?
    hStdError       DQ ?
STARTUPINFOA ENDS

; STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW
STARTF_USESTDHANDLES    EQU 00000100h
STARTF_USESHOWWINDOW   EQU 00000001h
SW_HIDE                 EQU 0

; =============================================================================
;                  PROCESS_INFORMATION Structure
; =============================================================================
PROCESS_INFORMATION STRUCT
    hProcess    DQ ?
    hThread     DQ ?
    dwProcessId DD ?
    dwThreadId  DD ?
PROCESS_INFORMATION ENDS

; =============================================================================
;                  SECURITY_ATTRIBUTES Structure
; =============================================================================
SECURITY_ATTRIBUTES STRUCT
    nLength             DD ?
    _pad0               DD ?
    lpSecurityDescriptor DQ ?
    bInheritHandle      DD ?
    _pad1               DD ?
SECURITY_ATTRIBUTES ENDS

; =============================================================================
;                  Command History Entry Structure
; =============================================================================
; Layout per CMD_ENTRY_SIZE (512 bytes):
;   Offset  0: QWORD  timestamp (GetTickCount64 value)
;   Offset  8: DWORD  exit_code
;   Offset 12: DWORD  output_length (bytes captured)
;   Offset 16: DWORD  command_length
;   Offset 20: DWORD  flags (reserved)
;   Offset 24: QWORD  duration_ms
;   Offset 32: BYTE[224] command_text (null-terminated)
;   Offset 256: BYTE[256] output_snippet (null-terminated, first 255 chars)

CMD_HIST_OFF_TIMESTAMP      EQU 0
CMD_HIST_OFF_EXITCODE       EQU 8
CMD_HIST_OFF_OUTLEN         EQU 12
CMD_HIST_OFF_CMDLEN         EQU 16
CMD_HIST_OFF_FLAGS          EQU 20
CMD_HIST_OFF_DURATION       EQU 24
CMD_HIST_OFF_CMDTEXT        EQU 32
CMD_HIST_OFF_OUTSNIPPET     EQU 256
CMD_HIST_CMD_MAXLEN         EQU 223    ; 224 - 1 for null terminator
CMD_HIST_OUT_MAXLEN         EQU 255    ; 256 - 1 for null terminator

; =============================================================================
;              Cache-Line Padded Counters Segment
; =============================================================================

SHELLINTEG_CL SEGMENT ALIGN(16) 'DATA'

    PUBLIC g_ShellInteg_MetricStart
    g_ShellInteg_MetricStart    dq 05348454C4C494E54h  ; "SHELLINT" sentinel
                                db 56 dup(0)

    PUBLIC g_SI_Counter_CmdsExecuted
    g_SI_Counter_CmdsExecuted   dq 0
                                db 56 dup(0)

    PUBLIC g_SI_Counter_CmdsDetected
    g_SI_Counter_CmdsDetected   dq 0
                                db 56 dup(0)

    PUBLIC g_SI_Counter_BytesRead
    g_SI_Counter_BytesRead      dq 0
                                db 56 dup(0)

    PUBLIC g_SI_Counter_BytesWritten
    g_SI_Counter_BytesWritten   dq 0
                                db 56 dup(0)

    PUBLIC g_SI_Counter_OSC633Injected
    g_SI_Counter_OSC633Injected dq 0
                                db 56 dup(0)

    PUBLIC g_SI_Counter_PipeReadErrors
    g_SI_Counter_PipeReadErrors dq 0
                                db 56 dup(0)

    PUBLIC g_SI_Counter_PipeWriteErrors
    g_SI_Counter_PipeWriteErrors dq 0
                                db 56 dup(0)

    PUBLIC g_SI_Counter_ProcessRestarts
    g_SI_Counter_ProcessRestarts dq 0
                                db 56 dup(0)

    PUBLIC g_ShellInteg_MetricEnd
    g_ShellInteg_MetricEnd      dq 0454E44534849454Ch  ; "ENDSHIEL" sentinel
                                db 56 dup(0)

SHELLINTEG_CL ENDS

; =============================================================================
;                          Data Segment
; =============================================================================
.data

    ; =========================================================================
    ; Initialization state
    ; =========================================================================
    ALIGN 8
    g_SI_Initialized        dq  0       ; Boolean: subsystem ready
    g_SI_ShellAlive         dq  0       ; Boolean: child process is running
    g_SI_LastExitCode       dd  0       ; Last captured exit code
    g_SI_LastExitCodePad    dd  0       ; Alignment padding

    ; =========================================================================
    ; Pipe handles (parent side)
    ; =========================================================================
    ALIGN 8
    g_hStdinWrite           dq  INVALID_HANDLE_VALUE    ; Write end -> child stdin
    g_hStdoutRead           dq  INVALID_HANDLE_VALUE    ; Read end <- child stdout
    g_hStderrRead           dq  INVALID_HANDLE_VALUE    ; Read end <- child stderr

    ; Child-side handles (closed after CreateProcess)
    g_hStdinRead            dq  INVALID_HANDLE_VALUE
    g_hStdoutWrite          dq  INVALID_HANDLE_VALUE
    g_hStderrWrite          dq  INVALID_HANDLE_VALUE

    ; =========================================================================
    ; Process handles
    ; =========================================================================
    ALIGN 8
    g_hChildProcess         dq  INVALID_HANDLE_VALUE
    g_hChildThread          dq  INVALID_HANDLE_VALUE
    g_dwChildPID            dd  0
    g_dwChildTID            dd  0

    ; =========================================================================
    ; Command History Ring Buffer
    ; =========================================================================
    ALIGN 16
    g_CmdHistoryBuffer      db (CMD_HISTORY_SIZE * CMD_ENTRY_SIZE) dup(0)
    ALIGN 8
    g_CmdHistoryHead        dq  0       ; Producer index (atomic xadd)
    g_CmdHistoryTail        dq  0       ; Consumer read pointer

    ; =========================================================================
    ; Timing for current command
    ; =========================================================================
    g_CmdStartTick          dq  0       ; GetTickCount64 at command submit

    ; =========================================================================
    ; Critical section for pipe I/O serialization
    ; =========================================================================
    ALIGN 8
    g_SI_PipeLock           CRITICAL_SECTION <>

    ; =========================================================================
    ; Output read buffer (temporary)
    ; =========================================================================
    ALIGN 16
    g_OutputReadBuf         db OUTPUT_BUFFER_SIZE dup(0)

    ; =========================================================================
    ; OSC 633 Escape Sequences (pre-built, null-terminated)
    ; =========================================================================
    ALIGN 8
    ; \x1b]633;A\x07 — Prompt Start
    szOSC633_A              db 1Bh, ']', '6', '3', '3', ';', 'A', 07h, 0
    szOSC633_A_Len          equ ($ - szOSC633_A - 1)

    ; \x1b]633;B\x07 — Prompt End / Command Input Start
    szOSC633_B              db 1Bh, ']', '6', '3', '3', ';', 'B', 07h, 0
    szOSC633_B_Len          equ ($ - szOSC633_B - 1)

    ; \x1b]633;C\x07 — Pre-Execution
    szOSC633_C              db 1Bh, ']', '6', '3', '3', ';', 'C', 07h, 0
    szOSC633_C_Len          equ ($ - szOSC633_C - 1)

    ; \x1b]633;D; — Execution Done prefix (exit code appended at runtime)
    szOSC633_D_Prefix       db 1Bh, ']', '6', '3', '3', ';', 'D', ';', 0
    szOSC633_D_PrefixLen    equ ($ - szOSC633_D_Prefix - 1)

    ; \x1b]633;E; — Command Line echo prefix (command appended at runtime)
    szOSC633_E_Prefix       db 1Bh, ']', '6', '3', '3', ';', 'E', ';', 0
    szOSC633_E_PrefixLen    equ ($ - szOSC633_E_Prefix - 1)

    ; \x1b]633;P; — Property prefix (key=value appended at runtime)
    szOSC633_P_Prefix       db 1Bh, ']', '6', '3', '3', ';', 'P', ';', 0
    szOSC633_P_PrefixLen    equ ($ - szOSC633_P_Prefix - 1)

    ; BEL terminator
    szBEL                   db 07h, 0

    ; CRLF for command submission
    szCRLF                  db 0Dh, 0Ah, 0

    ; =========================================================================
    ; Default shell command line
    ; =========================================================================
    szDefaultShell          db "cmd.exe /Q", 0      ; /Q = echo off
    szPowerShell            db "powershell.exe -NoLogo -NoProfile -NonInteractive", 0

    ; Echo-off marker injected after shell spawn
    szEchoOff               db "@echo off", 0Dh, 0Ah, 0

    ; Exit code query command (cmd.exe: echo %ERRORLEVEL%)
    szExitCodeQuery         db "echo %ERRORLEVEL%", 0Dh, 0Ah, 0

    ; =========================================================================
    ; Log / Status Strings
    ; =========================================================================
    szSI_InitOk             db "[ShellInteg] Subsystem initialized", 0Dh, 0Ah, 0
    szSI_ShutdownOk         db "[ShellInteg] Subsystem shutdown", 0Dh, 0Ah, 0
    szSI_CmdSubmitted       db "[ShellInteg] Command submitted", 0Dh, 0Ah, 0
    szSI_ProcessDead        db "[ShellInteg] Child process terminated", 0Dh, 0Ah, 0
    szSI_PipeFail           db "[ShellInteg] Pipe creation failed", 0Dh, 0Ah, 0
    szSI_ProcFail           db "[ShellInteg] Process creation failed", 0Dh, 0Ah, 0

    ; Scratch buffer for building dynamic OSC sequences
    ALIGN 8
    g_SI_ScratchBuf         db 1024 dup(0)

    ; Integer-to-ascii scratch for exit codes
    g_SI_ItoBuf             db 32 dup(0)

; =============================================================================
;                           Code Segment
; =============================================================================
.code

; =============================================================================
; ShellInteg_Init — Initialize the shell integration subsystem
;
; Creates anonymous pipes for stdin/stdout/stderr redirection, launches a
; child shell process (cmd.exe by default), and initializes the command
; history ring buffer + performance counters.
;
; RCX = Pointer to shell command line (null-terminated), or NULL for default
; Returns: RAX = 0 on success, SHELLINTEG_ERR_* on failure
;          RDX = detail string pointer
; =============================================================================
PUBLIC ShellInteg_Init
ShellInteg_Init PROC FRAME
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
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 512            ; Local frame (generously sized)
    .allocstack 512
    .endprolog

    ; ---- Guard: already initialized? ----
    cmp     qword ptr [g_SI_Initialized], 1
    jne     @si_init_continue
    lea     rdx, szSI_InitOk
    mov     eax, SHELLINTEG_ERR_ALREADY_INIT
    jmp     @si_init_exit

@si_init_continue:
    ; Save shell command line choice
    test    rcx, rcx
    jnz     @si_have_shell
    lea     rcx, szDefaultShell
@si_have_shell:
    mov     r12, rcx            ; r12 = shell command line

    ; ---- Initialize critical section for pipe I/O ----
    lea     rcx, g_SI_PipeLock
    sub     rsp, 32
    call    InitializeCriticalSection
    add     rsp, 32

    ; ---- Build SECURITY_ATTRIBUTES for inheritable handles ----
    ; sa is on stack at [rbp - 32]
    lea     rax, [rbp - 32]
    mov     dword ptr [rax], 24                 ; nLength = sizeof(SECURITY_ATTRIBUTES)
    mov     dword ptr [rax + 4], 0              ; padding
    mov     qword ptr [rax + 8], 0              ; lpSecurityDescriptor = NULL
    mov     dword ptr [rax + 16], 1             ; bInheritHandle = TRUE
    mov     dword ptr [rax + 20], 0             ; padding
    mov     r13, rax                            ; r13 = &sa

    ; ---- Create stdin pipe ----
    ; CreatePipe(&hStdinRead, &hStdinWrite, &sa, PIPE_BUFFER_SIZE)
    lea     rcx, g_hStdinRead
    lea     rdx, g_hStdinWrite
    mov     r8, r13
    mov     r9d, PIPE_BUFFER_SIZE
    sub     rsp, 32
    call    CreatePipe
    add     rsp, 32
    test    eax, eax
    jz      @si_init_pipe_fail

    ; Make parent's write end non-inheritable
    mov     rcx, qword ptr [g_hStdinWrite]
    mov     edx, HANDLE_FLAG_INHERIT
    xor     r8d, r8d                            ; flags = 0 (remove inheritance)
    sub     rsp, 32
    call    SetHandleInformation
    add     rsp, 32

    ; ---- Create stdout pipe ----
    lea     rcx, g_hStdoutRead
    lea     rdx, g_hStdoutWrite
    mov     r8, r13
    mov     r9d, PIPE_BUFFER_SIZE
    sub     rsp, 32
    call    CreatePipe
    add     rsp, 32
    test    eax, eax
    jz      @si_init_pipe_fail

    ; Make parent's read end non-inheritable
    mov     rcx, qword ptr [g_hStdoutRead]
    mov     edx, HANDLE_FLAG_INHERIT
    xor     r8d, r8d
    sub     rsp, 32
    call    SetHandleInformation
    add     rsp, 32

    ; ---- Create stderr pipe ----
    lea     rcx, g_hStderrRead
    lea     rdx, g_hStderrWrite
    mov     r8, r13
    mov     r9d, PIPE_BUFFER_SIZE
    sub     rsp, 32
    call    CreatePipe
    add     rsp, 32
    test    eax, eax
    jz      @si_init_pipe_fail

    ; Make parent's read end non-inheritable
    mov     rcx, qword ptr [g_hStderrRead]
    mov     edx, HANDLE_FLAG_INHERIT
    xor     r8d, r8d
    sub     rsp, 32
    call    SetHandleInformation
    add     rsp, 32

    ; ---- Build STARTUPINFOA ----
    ; si is on stack at [rbp - 136] (104 bytes = sizeof STARTUPINFOA, aligned)
    lea     rdi, [rbp - 136]
    xor     eax, eax
    mov     ecx, 104               ; Zero out STARTUPINFOA
    rep stosb

    lea     rax, [rbp - 136]
    mov     dword ptr [rax], 104                            ; cb = sizeof
    mov     dword ptr [rax + STARTUPINFOA.dwFlags], STARTF_USESTDHANDLES OR STARTF_USESHOWWINDOW
    mov     word ptr [rax + STARTUPINFOA.wShowWindow], SW_HIDE

    ; Set child stdio handles
    mov     rcx, qword ptr [g_hStdinRead]
    mov     qword ptr [rax + STARTUPINFOA.hStdInput], rcx
    mov     rcx, qword ptr [g_hStdoutWrite]
    mov     qword ptr [rax + STARTUPINFOA.hStdOutput], rcx
    mov     rcx, qword ptr [g_hStderrWrite]
    mov     qword ptr [rax + STARTUPINFOA.hStdError], rcx

    mov     r14, rax                ; r14 = &si

    ; ---- Build PROCESS_INFORMATION ----
    ; pi is on stack at [rbp - 168] (24 bytes + alignment)
    lea     rdi, [rbp - 168]
    xor     eax, eax
    mov     ecx, 32
    rep stosb
    lea     r15, [rbp - 168]        ; r15 = &pi

    ; ---- CreateProcessA ----
    ; CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE,
    ;                CREATE_NO_WINDOW, NULL, NULL, &si, &pi)
    xor     ecx, ecx                   ; lpApplicationName = NULL
    mov     rdx, r12                    ; lpCommandLine
    xor     r8, r8                      ; lpProcessAttributes = NULL
    xor     r9, r9                      ; lpThreadAttributes = NULL
    sub     rsp, 80                     ; shadow + 6 stack args
    mov     dword ptr [rsp + 32], 1     ; bInheritHandles = TRUE
    mov     dword ptr [rsp + 40], CREATE_NO_WINDOW  ; dwCreationFlags
    mov     qword ptr [rsp + 48], 0     ; lpEnvironment = NULL
    mov     qword ptr [rsp + 56], 0     ; lpCurrentDirectory = NULL
    mov     qword ptr [rsp + 64], r14   ; lpStartupInfo
    mov     qword ptr [rsp + 72], r15   ; lpProcessInformation
    call    CreateProcessA
    add     rsp, 80
    test    eax, eax
    jz      @si_init_proc_fail

    ; Store process info
    mov     rax, qword ptr [r15 + PROCESS_INFORMATION.hProcess]
    mov     qword ptr [g_hChildProcess], rax
    mov     rax, qword ptr [r15 + PROCESS_INFORMATION.hThread]
    mov     qword ptr [g_hChildThread], rax
    mov     eax, dword ptr [r15 + PROCESS_INFORMATION.dwProcessId]
    mov     dword ptr [g_dwChildPID], eax
    mov     eax, dword ptr [r15 + PROCESS_INFORMATION.dwThreadId]
    mov     dword ptr [g_dwChildTID], eax

    ; ---- Close child-side pipe handles (parent doesn't need them) ----
    mov     rcx, qword ptr [g_hStdinRead]
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
    mov     qword ptr [g_hStdinRead], INVALID_HANDLE_VALUE

    mov     rcx, qword ptr [g_hStdoutWrite]
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
    mov     qword ptr [g_hStdoutWrite], INVALID_HANDLE_VALUE

    mov     rcx, qword ptr [g_hStderrWrite]
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
    mov     qword ptr [g_hStderrWrite], INVALID_HANDLE_VALUE

    ; ---- Reset counters and history ----
    xor     eax, eax
    mov     qword ptr [g_SI_Counter_CmdsExecuted], rax
    mov     qword ptr [g_SI_Counter_CmdsDetected], rax
    mov     qword ptr [g_SI_Counter_BytesRead], rax
    mov     qword ptr [g_SI_Counter_BytesWritten], rax
    mov     qword ptr [g_SI_Counter_OSC633Injected], rax
    mov     qword ptr [g_SI_Counter_PipeReadErrors], rax
    mov     qword ptr [g_SI_Counter_PipeWriteErrors], rax
    mov     qword ptr [g_SI_Counter_ProcessRestarts], rax
    mov     qword ptr [g_CmdHistoryHead], rax
    mov     qword ptr [g_CmdHistoryTail], rax
    mov     dword ptr [g_SI_LastExitCode], eax

    ; ---- Mark subsystem as live ----
    mov     qword ptr [g_SI_ShellAlive], 1
    mov     qword ptr [g_SI_Initialized], 1

    ; ---- Inject initial OSC 633;A (Prompt Start) to output stream ----
    mov     ecx, OSC633_PROMPT_START
    xor     edx, edx            ; no payload
    xor     r8d, r8d
    call    ShellInteg_InjectOSC633

    ; ---- Debug log ----
    lea     rcx, szSI_InitOk
    sub     rsp, 32
    call    OutputDebugStringA
    add     rsp, 32

    ; ---- Success ----
    xor     eax, eax
    lea     rdx, szSI_InitOk
    jmp     @si_init_exit

@si_init_pipe_fail:
    lea     rcx, szSI_PipeFail
    sub     rsp, 32
    call    OutputDebugStringA
    add     rsp, 32
    mov     eax, SHELLINTEG_ERR_PIPE_FAILED
    lea     rdx, szSI_PipeFail
    jmp     @si_init_exit

@si_init_proc_fail:
    lea     rcx, szSI_ProcFail
    sub     rsp, 32
    call    OutputDebugStringA
    add     rsp, 32
    ; Clean up pipes we already opened
    call    si_close_all_pipes
    mov     eax, SHELLINTEG_ERR_PROCESS_FAILED
    lea     rdx, szSI_ProcFail

@si_init_exit:
    lea     rsp, [rbp]
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ShellInteg_Init ENDP

; =============================================================================
; ShellInteg_Shutdown — Tear down shell integration subsystem
;
; Drains pipe buffers, sends exit to child shell, waits briefly for
; termination, force-terminates if needed, closes all handles.
;
; Returns: RAX = 0 on success
;          RDX = detail string pointer
; =============================================================================
PUBLIC ShellInteg_Shutdown
ShellInteg_Shutdown PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 128
    .allocstack 128
    .endprolog

    ; Guard: not initialized
    cmp     qword ptr [g_SI_Initialized], 0
    jne     @si_shut_go
    mov     eax, SHELLINTEG_ERR_NOT_INIT
    lea     rdx, szSI_ProcessDead
    jmp     @si_shut_exit

@si_shut_go:
    ; ---- Inject final OSC 633;D;0 (Done) ----
    mov     ecx, OSC633_EXECUTION_DONE
    lea     rdx, g_SI_ItoBuf
    mov     byte ptr [rdx], '0'
    mov     byte ptr [rdx + 1], 0
    xor     r8d, r8d
    call    ShellInteg_InjectOSC633

    ; ---- Send "exit" command to child shell ----
    cmp     qword ptr [g_SI_ShellAlive], 0
    je      @si_shut_no_exit_cmd

    ; Write "exit\r\n" to stdin pipe
    lea     rsi, [rsp + 64]
    mov     byte ptr [rsi], 'e'
    mov     byte ptr [rsi+1], 'x'
    mov     byte ptr [rsi+2], 'i'
    mov     byte ptr [rsi+3], 't'
    mov     byte ptr [rsi+4], 0Dh
    mov     byte ptr [rsi+5], 0Ah
    mov     byte ptr [rsi+6], 0

    mov     rcx, qword ptr [g_hStdinWrite]
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @si_shut_no_exit_cmd
    mov     rdx, rsi                    ; lpBuffer
    mov     r8d, 6                      ; nBytesToWrite
    lea     r9, [rsp + 80]              ; lpBytesWritten
    sub     rsp, 40
    mov     qword ptr [rsp + 32], 0     ; lpOverlapped
    call    WriteFile
    add     rsp, 40

@si_shut_no_exit_cmd:
    ; ---- Wait 2 seconds for graceful exit ----
    mov     rcx, qword ptr [g_hChildProcess]
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @si_shut_skip_wait
    mov     edx, 2000                   ; 2000 ms
    sub     rsp, 32
    call    WaitForSingleObject
    add     rsp, 32
    cmp     eax, WAIT_OBJECT_0
    je      @si_shut_skip_terminate

    ; Force terminate if still running
    mov     rcx, qword ptr [g_hChildProcess]
    mov     edx, 1                      ; exit code
    sub     rsp, 32
    call    TerminateProcess
    add     rsp, 32

@si_shut_skip_terminate:
@si_shut_skip_wait:

    ; ---- Get final exit code ----
    mov     rcx, qword ptr [g_hChildProcess]
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @si_shut_close
    lea     rdx, [rsp + 88]
    sub     rsp, 32
    call    GetExitCodeProcess
    add     rsp, 32
    test    eax, eax
    jz      @si_shut_close
    mov     eax, dword ptr [rsp + 88]
    mov     dword ptr [g_SI_LastExitCode], eax

@si_shut_close:
    ; ---- Close process handles ----
    mov     rcx, qword ptr [g_hChildProcess]
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @si_shut_close_thread
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
    mov     qword ptr [g_hChildProcess], INVALID_HANDLE_VALUE

@si_shut_close_thread:
    mov     rcx, qword ptr [g_hChildThread]
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @si_shut_close_pipes
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
    mov     qword ptr [g_hChildThread], INVALID_HANDLE_VALUE

@si_shut_close_pipes:
    call    si_close_all_pipes

    ; ---- Delete critical section ----
    lea     rcx, g_SI_PipeLock
    sub     rsp, 32
    call    DeleteCriticalSection
    add     rsp, 32

    ; ---- Mark as shut down ----
    mov     qword ptr [g_SI_Initialized], 0
    mov     qword ptr [g_SI_ShellAlive], 0

    lea     rcx, szSI_ShutdownOk
    sub     rsp, 32
    call    OutputDebugStringA
    add     rsp, 32

    xor     eax, eax
    lea     rdx, szSI_ShutdownOk

@si_shut_exit:
    add     rsp, 128
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ShellInteg_Shutdown ENDP

; =============================================================================
; ShellInteg_ExecuteCommand — Submit a command with full OSC 633 lifecycle
;
; Sequence:
;   1. Inject OSC 633;B (Prompt End / command about to start)
;   2. Inject OSC 633;E;<command> (echo command line)
;   3. Inject OSC 633;C (Pre-Execution)
;   4. Write command + CRLF to child stdin pipe
;   5. Record start timestamp for duration tracking
;   6. Atomic increment of commands-executed counter
;   7. Write entry to command history ring buffer
;
; RCX = Pointer to command string (null-terminated)
; RDX = Command string length (0 = auto-detect via lstrlenA)
; Returns: RAX = 0 on success, error code on failure
;          RDX = detail string pointer
; =============================================================================
PUBLIC ShellInteg_ExecuteCommand
ShellInteg_ExecuteCommand PROC FRAME
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
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 384
    .allocstack 384
    .endprolog

    ; ---- Validate state ----
    cmp     qword ptr [g_SI_Initialized], 0
    jne     @si_exec_check_alive
    mov     eax, SHELLINTEG_ERR_NOT_INIT
    xor     edx, edx
    jmp     @si_exec_exit

@si_exec_check_alive:
    cmp     qword ptr [g_SI_ShellAlive], 0
    jne     @si_exec_check_params
    mov     eax, SHELLINTEG_ERR_PROCESS_DEAD
    lea     rdx, szSI_ProcessDead
    jmp     @si_exec_exit

@si_exec_check_params:
    test    rcx, rcx
    jnz     @si_exec_params_ok
    mov     eax, SHELLINTEG_ERR_NULL_PARAM
    xor     edx, edx
    jmp     @si_exec_exit

@si_exec_params_ok:
    mov     r12, rcx            ; r12 = command string pointer
    mov     r13, rdx            ; r13 = command string length (may be 0)

    ; Auto-detect length if caller passed 0
    test    r13, r13
    jnz     @si_exec_have_len
    mov     rcx, r12
    sub     rsp, 32
    call    lstrlenA
    add     rsp, 32
    mov     r13, rax
@si_exec_have_len:

    ; ---- Acquire pipe lock ----
    lea     rcx, g_SI_PipeLock
    sub     rsp, 32
    call    EnterCriticalSection
    add     rsp, 32

    ; ---- Record start timestamp ----
    sub     rsp, 32
    call    GetTickCount64
    add     rsp, 32
    mov     qword ptr [g_CmdStartTick], rax
    mov     r14, rax            ; r14 = start tick

    ; ---- 1. Inject OSC 633;B (Prompt End / Command Start) ----
    mov     ecx, OSC633_PROMPT_END
    xor     edx, edx
    xor     r8d, r8d
    call    ShellInteg_InjectOSC633

    ; ---- 2. Inject OSC 633;E;<command> (Command Line echo) ----
    mov     ecx, OSC633_COMMAND_LINE
    mov     rdx, r12            ; payload = command text
    mov     r8, r13             ; payload length
    call    ShellInteg_InjectOSC633

    ; ---- 3. Inject OSC 633;C (Pre-Execution) ----
    mov     ecx, OSC633_PRE_EXECUTION
    xor     edx, edx
    xor     r8d, r8d
    call    ShellInteg_InjectOSC633

    ; ---- 4. Write command + CRLF to stdin pipe ----
    ; Build: [command][CRLF] in scratch buffer
    lea     rdi, g_SI_ScratchBuf

    ; Copy command
    mov     rsi, r12
    mov     rcx, r13
    ; Bounds check: command + 2 (CRLF) must fit in 1024
    lea     rax, [r13 + 2]
    cmp     rax, 1024
    jb      @si_exec_copy_cmd
    ; Truncate to fit
    mov     r13, 1021           ; 1024 - 2 (CRLF) - 1 (null)
    mov     rcx, r13

@si_exec_copy_cmd:
    rep movsb
    ; Append CRLF
    mov     byte ptr [rdi], 0Dh
    mov     byte ptr [rdi + 1], 0Ah
    inc     rdi
    inc     rdi

    ; Calculate write length
    lea     rax, g_SI_ScratchBuf
    sub     rdi, rax            ; rdi = total bytes to write
    mov     r15, rdi            ; r15 = write length

    ; WriteFile(hStdinWrite, buf, len, &written, NULL)
    mov     rcx, qword ptr [g_hStdinWrite]
    lea     rdx, g_SI_ScratchBuf
    mov     r8, r15
    lea     r9, [rbp - 16]      ; lpBytesWritten
    sub     rsp, 40
    mov     qword ptr [rsp + 32], 0  ; lpOverlapped = NULL
    call    WriteFile
    add     rsp, 40
    test    eax, eax
    jz      @si_exec_write_fail

    ; NOTE: FlushFileBuffers on anonymous pipes is unnecessary and degrades
    ; latency. Pipe writes are committed on WriteFile completion.
    ; (Removed per production audit — see AUDIT item #8)

    ; Update bytes-written counter
    mov     rax, r15
    lock xadd qword ptr [g_SI_Counter_BytesWritten], rax

    ; ---- 5. Atomic increment commands executed ----
    mov     rax, 1
    lock xadd qword ptr [g_SI_Counter_CmdsExecuted], rax

    ; ---- 6. Write command to history ring buffer ----
    call    si_record_command_history

    ; ---- Release pipe lock ----
    lea     rcx, g_SI_PipeLock
    sub     rsp, 32
    call    LeaveCriticalSection
    add     rsp, 32

    ; Debug log
    lea     rcx, szSI_CmdSubmitted
    sub     rsp, 32
    call    OutputDebugStringA
    add     rsp, 32

    xor     eax, eax
    lea     rdx, szSI_CmdSubmitted
    jmp     @si_exec_exit

@si_exec_write_fail:
    lock inc qword ptr [g_SI_Counter_PipeWriteErrors]

    ; Release lock even on failure
    lea     rcx, g_SI_PipeLock
    sub     rsp, 32
    call    LeaveCriticalSection
    add     rsp, 32

    mov     eax, SHELLINTEG_ERR_WRITE_FAILED
    xor     edx, edx

@si_exec_exit:
    lea     rsp, [rbp]
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ShellInteg_ExecuteCommand ENDP

; =============================================================================
; ShellInteg_ReadOutput — Non-blocking read from shell stdout pipe
;
; Peeks the pipe first; if data available, reads up to buffer size.
; Injects OSC 633;D;<exitcode> when command completion is detected.
;
; RCX = Output buffer pointer
; RDX = Output buffer capacity (bytes)
; Returns: RAX = bytes read (0 if nothing available), or negative on error
;          RDX = 0
; =============================================================================
PUBLIC ShellInteg_ReadOutput
ShellInteg_ReadOutput PROC FRAME
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
    sub     rsp, 96
    .allocstack 96
    .endprolog

    ; Validate state & params
    cmp     qword ptr [g_SI_Initialized], 0
    je      @si_read_not_init
    test    rcx, rcx
    jz      @si_read_null
    test    rdx, rdx
    jz      @si_read_null

    mov     r12, rcx            ; r12 = output buffer
    mov     r13, rdx            ; r13 = buffer capacity

    ; ---- Acquire pipe lock ----
    lea     rcx, g_SI_PipeLock
    sub     rsp, 32
    call    EnterCriticalSection
    add     rsp, 32

    ; ---- PeekNamedPipe to check if data is available ----
    ; FIX (Audit #1): Use fixed stack slot that survives sub rsp/add rsp.
    ; We store totalBytesAvail at [rsp + 64] BEFORE adjusting rsp.
    ; After the call, [rsp + 64] is still valid because the 96-byte
    ; frame from .allocstack hasn't moved.
    mov     rcx, qword ptr [g_hStdoutRead]
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @si_read_dead

    ; Zero the output slots first (use fixed offsets within our 96-byte frame)
    mov     dword ptr [rsp + 64], 0     ; totalBytesAvail
    mov     dword ptr [rsp + 68], 0     ; bytesLeftThisMessage

    xor     edx, edx                ; lpBuffer = NULL (just peeking)
    xor     r8d, r8d                ; nBufferSize = 0
    xor     r9d, r9d                ; lpBytesRead = NULL
    sub     rsp, 48
    lea     rax, [rsp + 48 + 64]    ; &totalBytesAvail (compensate for sub rsp, 48)
    mov     qword ptr [rsp + 32], rax
    lea     rax, [rsp + 48 + 68]    ; &bytesLeftThisMessage
    mov     qword ptr [rsp + 40], rax
    call    PeekNamedPipe
    add     rsp, 48
    test    eax, eax
    jz      @si_read_pipe_err

    ; Check available bytes (slot survived — same rsp base)
    mov     eax, dword ptr [rsp + 64]   ; totalBytesAvail
    test    eax, eax
    jz      @si_read_empty

    ; ---- ReadFile: min(available, capacity) ----
    mov     rbx, rax
    cmp     rbx, r13
    jbe     @si_read_use_avail
    mov     rbx, r13                    ; Cap to buffer capacity
@si_read_use_avail:

    mov     rcx, qword ptr [g_hStdoutRead]
    mov     rdx, r12                    ; lpBuffer
    mov     r8, rbx                     ; nBytesToRead
    lea     r9, [rsp + 56]              ; lpBytesRead
    sub     rsp, 40
    mov     qword ptr [rsp + 32], 0     ; lpOverlapped = NULL
    call    ReadFile
    add     rsp, 40
    test    eax, eax
    jz      @si_read_pipe_err

    ; Get actual bytes read
    mov     rax, qword ptr [rsp + 56]
    mov     rbx, rax                    ; rbx = bytes read

    ; Update bytes-read counter (atomic)
    lock xadd qword ptr [g_SI_Counter_BytesRead], rax

    ; Null-terminate if room
    cmp     rbx, r13
    jae     @si_read_no_term
    mov     byte ptr [r12 + rbx], 0
@si_read_no_term:

    ; NOTE (Audit #5): CmdsDetected is NOT incremented here.
    ; It was semantically incorrect to count every read chunk as a
    ; "detected command". CmdsDetected is now incremented only in
    ; ShellInteg_CompleteCommand when OSC633;D fires, confirming
    ; an actual command boundary.

    ; Release lock
    lea     rcx, g_SI_PipeLock
    sub     rsp, 32
    call    LeaveCriticalSection
    add     rsp, 32

    mov     rax, rbx            ; Return bytes read
    xor     edx, edx
    jmp     @si_read_exit

@si_read_empty:
    lea     rcx, g_SI_PipeLock
    sub     rsp, 32
    call    LeaveCriticalSection
    add     rsp, 32
    xor     eax, eax            ; 0 bytes available
    xor     edx, edx
    jmp     @si_read_exit

@si_read_dead:
    lea     rcx, g_SI_PipeLock
    sub     rsp, 32
    call    LeaveCriticalSection
    add     rsp, 32
    mov     qword ptr [g_SI_ShellAlive], 0
    mov     rax, -1
    xor     edx, edx
    jmp     @si_read_exit

@si_read_pipe_err:
    lock inc qword ptr [g_SI_Counter_PipeReadErrors]
    lea     rcx, g_SI_PipeLock
    sub     rsp, 32
    call    LeaveCriticalSection
    add     rsp, 32
    mov     rax, -1
    xor     edx, edx
    jmp     @si_read_exit

@si_read_not_init:
    mov     rax, -1
    xor     edx, edx
    jmp     @si_read_exit

@si_read_null:
    mov     rax, -1
    xor     edx, edx

@si_read_exit:
    add     rsp, 96
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ShellInteg_ReadOutput ENDP

; =============================================================================
; ShellInteg_GetExitCode — Retrieve exit code of the last command / child process
;
; Returns: EAX = exit code (STILL_ACTIVE = 259 if process running)
; =============================================================================
PUBLIC ShellInteg_GetExitCode
ShellInteg_GetExitCode PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     qword ptr [g_SI_Initialized], 0
    je      @si_ec_fallback

    mov     rcx, qword ptr [g_hChildProcess]
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @si_ec_fallback

    lea     rdx, [rsp + 32]             ; &exitCode on stack
    sub     rsp, 32
    call    GetExitCodeProcess
    add     rsp, 32
    test    eax, eax
    jz      @si_ec_fallback

    mov     eax, dword ptr [rsp + 32]
    mov     dword ptr [g_SI_LastExitCode], eax
    jmp     @si_ec_done

@si_ec_fallback:
    mov     eax, dword ptr [g_SI_LastExitCode]

@si_ec_done:
    add     rsp, 48
    pop     rbx
    ret
ShellInteg_GetExitCode ENDP

; =============================================================================
; ShellInteg_InjectOSC633 — Inject an OSC 633 escape sequence
;
; Builds the appropriate escape sequence and writes it to the IDE's
; output stream (parent stdout) for command detection.
;
; ECX = Sequence ID (OSC633_PROMPT_START..OSC633_PROPERTY)
; RDX = Payload string pointer (for E, D, P sequences), or NULL
; R8  = Payload length (0 = auto-detect if RDX != NULL)
; Returns: RAX = 0 on success
; =============================================================================
PUBLIC ShellInteg_InjectOSC633
ShellInteg_InjectOSC633 PROC FRAME
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
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     r12d, ecx           ; r12 = sequence ID
    mov     r13, rdx            ; r13 = payload (may be NULL)
    mov     r14, r8             ; r14 = payload length

    ; Validate sequence ID
    cmp     r12d, OSC633_PROPERTY
    ja      @si_osc_invalid_id

    ; Auto-detect payload length if needed
    test    r13, r13
    jz      @si_osc_no_payload_len
    test    r14, r14
    jnz     @si_osc_have_payload
    mov     rcx, r13
    sub     rsp, 32
    call    lstrlenA
    add     rsp, 32
    mov     r14, rax
    jmp     @si_osc_have_payload

@si_osc_no_payload_len:
    xor     r14d, r14d

@si_osc_have_payload:

    ; Build sequence into scratch buffer
    lea     rdi, [rsp + 64]     ; Local scratch (64 bytes for sequence)

    ; ---- Switch on sequence ID ----
    cmp     r12d, OSC633_PROMPT_START
    je      @si_osc_simple_A
    cmp     r12d, OSC633_PROMPT_END
    je      @si_osc_simple_B
    cmp     r12d, OSC633_PRE_EXECUTION
    je      @si_osc_simple_C
    cmp     r12d, OSC633_EXECUTION_DONE
    je      @si_osc_with_payload_D
    cmp     r12d, OSC633_COMMAND_LINE
    je      @si_osc_with_payload_E
    cmp     r12d, OSC633_PROPERTY
    je      @si_osc_with_payload_P
    jmp     @si_osc_invalid_id

    ; ---- Simple sequences (no payload) ----
@si_osc_simple_A:
    lea     rsi, szOSC633_A
    mov     ecx, szOSC633_A_Len
    jmp     @si_osc_write_simple

@si_osc_simple_B:
    lea     rsi, szOSC633_B
    mov     ecx, szOSC633_B_Len
    jmp     @si_osc_write_simple

@si_osc_simple_C:
    lea     rsi, szOSC633_C
    mov     ecx, szOSC633_C_Len

@si_osc_write_simple:
    ; Write pre-built sequence to parent stdout
    mov     rbx, rcx            ; rbx = sequence length
    ; Get parent stdout handle
    mov     ecx, STD_OUTPUT_HANDLE_ID
    sub     rsp, 32
    call    GetStdHandle
    add     rsp, 32
    cmp     rax, INVALID_HANDLE_VALUE
    je      @si_osc_done_ok     ; No console, silently succeed

    ; WriteFile(hStdout, sequence, len, &written, NULL)
    mov     rcx, rax            ; handle
    mov     rdx, rsi            ; buffer
    mov     r8, rbx             ; length
    lea     r9, [rsp + 56]      ; &written
    sub     rsp, 40
    mov     qword ptr [rsp + 32], 0
    call    WriteFile
    add     rsp, 40

    lock inc qword ptr [g_SI_Counter_OSC633Injected]
    jmp     @si_osc_done_ok

    ; ---- Sequences with payload (D, E, P) ----
@si_osc_with_payload_D:
    lea     rsi, szOSC633_D_Prefix
    mov     ebx, szOSC633_D_PrefixLen
    jmp     @si_osc_build_payload

@si_osc_with_payload_E:
    lea     rsi, szOSC633_E_Prefix
    mov     ebx, szOSC633_E_PrefixLen
    jmp     @si_osc_build_payload

@si_osc_with_payload_P:
    lea     rsi, szOSC633_P_Prefix
    mov     ebx, szOSC633_P_PrefixLen

@si_osc_build_payload:
    ; Assemble: [prefix][payload][BEL] into g_SI_ScratchBuf
    ; (reusing global scratch — caller holds pipe lock for E/D/P)
    push    rdi
    lea     rdi, g_SI_ScratchBuf

    ; Copy prefix
    push    rcx
    mov     rcx, rbx
    push    rsi
    rep movsb
    pop     rsi
    pop     rcx

    ; Copy payload
    test    r13, r13
    jz      @si_osc_no_payload_copy
    push    rsi
    mov     rsi, r13
    mov     rcx, r14
    ; Bounds check: prefix + payload + 1 (BEL) < 1024
    lea     rax, [rbx + r14 + 1]
    cmp     rax, 1024
    jb      @si_osc_payload_fits
    ; Truncate payload
    mov     r14, 1024
    sub     r14, rbx
    dec     r14                 ; Room for BEL
    mov     rcx, r14
@si_osc_payload_fits:
    rep movsb
    pop     rsi
@si_osc_no_payload_copy:

    ; Append BEL (0x07)
    mov     byte ptr [rdi], 07h
    inc     rdi

    ; Calculate total length
    lea     rax, g_SI_ScratchBuf
    sub     rdi, rax
    mov     rbx, rdi            ; rbx = total byte count
    pop     rdi

    ; Write to parent stdout
    mov     ecx, STD_OUTPUT_HANDLE_ID
    sub     rsp, 32
    call    GetStdHandle
    add     rsp, 32
    cmp     rax, INVALID_HANDLE_VALUE
    je      @si_osc_done_ok

    mov     rcx, rax
    lea     rdx, g_SI_ScratchBuf
    mov     r8, rbx
    lea     r9, [rsp + 56]
    sub     rsp, 40
    mov     qword ptr [rsp + 32], 0
    call    WriteFile
    add     rsp, 40

    lock inc qword ptr [g_SI_Counter_OSC633Injected]
    jmp     @si_osc_done_ok

@si_osc_invalid_id:
    mov     eax, SHELLINTEG_ERR_INVALID_SEQ_ID
    jmp     @si_osc_exit

@si_osc_done_ok:
    xor     eax, eax

@si_osc_exit:
    add     rsp, 128
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ShellInteg_InjectOSC633 ENDP

; =============================================================================
; ShellInteg_GetCommandHistory — Read an entry from the command history ring
;
; RCX = Index into history (0 = most recent, 1 = previous, etc.)
; RDX = Output buffer pointer (must be >= CMD_ENTRY_SIZE bytes)
; Returns: RAX = 0 on success, error if index out of range
;          Buffer filled with raw CMD_ENTRY_SIZE bytes on success
; =============================================================================
PUBLIC ShellInteg_GetCommandHistory
ShellInteg_GetCommandHistory PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Validate
    cmp     qword ptr [g_SI_Initialized], 0
    je      @si_hist_not_init
    test    rdx, rdx
    jz      @si_hist_null

    mov     rbx, rcx            ; rbx = requested index (0 = latest)
    mov     rdi, rdx            ; rdi = output buffer

    ; FIX (Audit #7): Validate requested index against available entries.
    ; If index >= min(head, CMD_HISTORY_SIZE), return error.
    mov     rax, qword ptr [g_CmdHistoryHead]
    test    rax, rax
    jz      @si_hist_empty      ; No commands recorded yet

    ; Compute depth = min(head, CMD_HISTORY_SIZE)
    mov     rcx, rax
    cmp     rcx, CMD_HISTORY_SIZE
    jbe     @si_hist_depth_ok
    mov     rcx, CMD_HISTORY_SIZE
@si_hist_depth_ok:
    cmp     rbx, rcx
    jae     @si_hist_empty      ; Requested index exceeds available entries

    ; Compute actual slot: (head - 1 - index) & mask
    dec     rax                 ; head - 1
    sub     rax, rbx            ; - index
    and     rax, CMD_HISTORY_MASK

    ; Calculate buffer offset = slot * CMD_ENTRY_SIZE
    shl     rax, CMD_ENTRY_SHIFT
    lea     rsi, g_CmdHistoryBuffer
    add     rsi, rax

    ; Copy CMD_ENTRY_SIZE bytes to output
    mov     ecx, CMD_ENTRY_SIZE
    rep movsb

    xor     eax, eax
    jmp     @si_hist_exit

@si_hist_not_init:
    mov     eax, SHELLINTEG_ERR_NOT_INIT
    jmp     @si_hist_exit

@si_hist_null:
    mov     eax, SHELLINTEG_ERR_NULL_PARAM
    jmp     @si_hist_exit

@si_hist_empty:
    mov     eax, SHELLINTEG_ERR_BUFFER_OVERFLOW
    ; No entries available

@si_hist_exit:
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ShellInteg_GetCommandHistory ENDP

; =============================================================================
; ShellInteg_GetStats — Return subsystem performance counters
;
; RCX = Pointer to output struct (9 QWORDs = 72 bytes minimum)
;       [0] = CmdsExecuted
;       [1] = CmdsDetected
;       [2] = BytesRead
;       [3] = BytesWritten
;       [4] = OSC633Injected
;       [5] = PipeReadErrors
;       [6] = PipeWriteErrors
;       [7] = ProcessRestarts
;       [8] = HistoryDepth (number of entries in ring buffer)
; Returns: RAX = 0 on success
; =============================================================================
PUBLIC ShellInteg_GetStats
ShellInteg_GetStats PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog

    test    rcx, rcx
    jz      @si_stats_null

    ; Acquire-fence reads of all counters
    mov     rax, qword ptr [g_SI_Counter_CmdsExecuted]
    mov     qword ptr [rcx], rax

    mov     rax, qword ptr [g_SI_Counter_CmdsDetected]
    mov     qword ptr [rcx + 8], rax

    mov     rax, qword ptr [g_SI_Counter_BytesRead]
    mov     qword ptr [rcx + 16], rax

    mov     rax, qword ptr [g_SI_Counter_BytesWritten]
    mov     qword ptr [rcx + 24], rax

    mov     rax, qword ptr [g_SI_Counter_OSC633Injected]
    mov     qword ptr [rcx + 32], rax

    mov     rax, qword ptr [g_SI_Counter_PipeReadErrors]
    mov     qword ptr [rcx + 40], rax

    mov     rax, qword ptr [g_SI_Counter_PipeWriteErrors]
    mov     qword ptr [rcx + 48], rax

    mov     rax, qword ptr [g_SI_Counter_ProcessRestarts]
    mov     qword ptr [rcx + 56], rax

    ; History depth = min(head, CMD_HISTORY_SIZE)
    mov     rax, qword ptr [g_CmdHistoryHead]
    cmp     rax, CMD_HISTORY_SIZE
    jbe     @si_stats_depth_ok
    mov     rax, CMD_HISTORY_SIZE
@si_stats_depth_ok:
    mov     qword ptr [rcx + 64], rax

    xor     eax, eax
    jmp     @si_stats_exit

@si_stats_null:
    mov     eax, SHELLINTEG_ERR_NULL_PARAM

@si_stats_exit:
    add     rsp, 32
    pop     rbx
    ret
ShellInteg_GetStats ENDP

; =============================================================================
; ShellInteg_SetProperty — Announce a shell property via OSC 633;P
;
; Injects \x1b]633;P;<key>=<value>\x07 into the output stream.
; Used for cwd, shell type, PID, and other IDE-consumable properties.
;
; RCX = Key string (null-terminated, e.g., "Cwd")
; RDX = Value string (null-terminated, e.g., "C:\Users\Dev")
; Returns: RAX = 0 on success
; =============================================================================
PUBLIC ShellInteg_SetProperty
ShellInteg_SetProperty PROC FRAME
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
    sub     rsp, 96
    .allocstack 96
    .endprolog

    test    rcx, rcx
    jz      @si_prop_null
    test    rdx, rdx
    jz      @si_prop_null

    mov     r12, rcx            ; r12 = key
    mov     r13, rdx            ; r13 = value

    ; Build "key=value" in local scratch
    lea     rdi, [rsp + 48]     ; scratch area on stack

    ; Copy key
    mov     rsi, r12
@si_prop_copy_key:
    lodsb
    test    al, al
    jz      @si_prop_key_done
    stosb
    jmp     @si_prop_copy_key
@si_prop_key_done:

    ; Append '='
    mov     byte ptr [rdi], '='
    inc     rdi

    ; Copy value
    mov     rsi, r13
@si_prop_copy_val:
    lodsb
    test    al, al
    jz      @si_prop_val_done
    stosb
    jmp     @si_prop_copy_val
@si_prop_val_done:
    mov     byte ptr [rdi], 0   ; Null-terminate

    ; Calculate payload length
    lea     rax, [rsp + 48]
    sub     rdi, rax
    mov     r8, rdi             ; r8 = payload length

    ; Inject via OSC 633;P
    mov     ecx, OSC633_PROPERTY
    lea     rdx, [rsp + 48]
    call    ShellInteg_InjectOSC633

    xor     eax, eax
    jmp     @si_prop_exit

@si_prop_null:
    mov     eax, SHELLINTEG_ERR_NULL_PARAM

@si_prop_exit:
    add     rsp, 96
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ShellInteg_SetProperty ENDP

; =============================================================================
; ShellInteg_IsAlive — Check if child shell process is still running
;
; Returns: EAX = 1 if alive, 0 if dead/exited
; =============================================================================
PUBLIC ShellInteg_IsAlive
ShellInteg_IsAlive PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     qword ptr [g_SI_Initialized], 0
    je      @si_alive_no

    mov     rcx, qword ptr [g_hChildProcess]
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @si_alive_no

    ; WaitForSingleObject with 0 timeout = instant poll
    xor     edx, edx            ; dwMilliseconds = 0
    sub     rsp, 32
    call    WaitForSingleObject
    add     rsp, 32

    cmp     eax, WAIT_TIMEOUT
    je      @si_alive_yes

    ; Process has exited — get exit code
    ; FIX (Audit #6): Use fixed local slot [rsp + 36] within our 48-byte
    ; frame. The slot must survive the sub rsp, 32 / add rsp, 32 dance.
    ; We point rdx at [rsp + 36] BEFORE the sub rsp, then after the call
    ; and add rsp the same [rsp + 36] is still our exit code.
    mov     dword ptr [rsp + 36], 0     ; Pre-zero slot
    mov     rcx, qword ptr [g_hChildProcess]
    lea     rdx, [rsp + 36]             ; Fixed slot in our 48-byte frame
    sub     rsp, 32
    call    GetExitCodeProcess
    add     rsp, 32
    test    eax, eax
    jz      @si_alive_no
    mov     eax, dword ptr [rsp + 36]   ; Read from same fixed slot
    mov     dword ptr [g_SI_LastExitCode], eax

    ; Mark shell as dead
    mov     qword ptr [g_SI_ShellAlive], 0

    ; Inject final OSC 633;D with exit code
    call    si_inject_done_with_exitcode

    ; Debug notification
    lea     rcx, szSI_ProcessDead
    sub     rsp, 32
    call    OutputDebugStringA
    add     rsp, 32

@si_alive_no:
    xor     eax, eax
    jmp     @si_alive_exit

@si_alive_yes:
    mov     eax, 1
    mov     qword ptr [g_SI_ShellAlive], 1

@si_alive_exit:
    add     rsp, 48
    pop     rbx
    ret
ShellInteg_IsAlive ENDP

; =============================================================================
; ShellInteg_CompleteCommand — Close command lifecycle after execution
;
; Called when a command finishes (exit code available). Updates the most
; recent history ring entry with exit code + duration, injects OSC 633;D,
; and increments CmdsDetected.
;
; This is the ONLY correct place to increment CmdsDetected — here we have
; a confirmed command boundary (the command actually completed).
;
; RCX = Exit code (DWORD, zero-extended)
; Returns: RAX = 0 on success
;          RDX = 0
;
; Lock-Free History Patch Path:
;   We compute the LAST slot written (head - 1) and patch it in-place.
;   Because only one command can be "completing" at a time per shell
;   instance, this is safe without a mutex. The atomic counter update
;   is still lock-prefixed for cross-thread visibility.
; =============================================================================
PUBLIC ShellInteg_CompleteCommand
ShellInteg_CompleteCommand PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 80
    .allocstack 80
    .endprolog

    ; Guard: must be initialized
    cmp     qword ptr [g_SI_Initialized], 0
    je      @si_cc_not_init

    mov     r12d, ecx               ; r12d = exit code

    ; Store as last exit code
    mov     dword ptr [g_SI_LastExitCode], r12d

    ; ---- 1. Compute duration = now - g_CmdStartTick ----
    sub     rsp, 32
    call    GetTickCount64
    add     rsp, 32
    sub     rax, qword ptr [g_CmdStartTick]    ; rax = duration_ms
    mov     rbx, rax                            ; rbx = duration_ms

    ; ---- 2. Lock-free patch of the last history entry ----
    ; Slot index = (head - 1) & mask
    mov     rax, qword ptr [g_CmdHistoryHead]
    test    rax, rax
    jz      @si_cc_skip_history     ; No entries yet

    dec     rax
    and     rax, CMD_HISTORY_MASK
    shl     rax, CMD_ENTRY_SHIFT

    lea     rdi, g_CmdHistoryBuffer
    add     rdi, rax                ; rdi = entry base

    ; Patch exit code
    mov     dword ptr [rdi + CMD_HIST_OFF_EXITCODE], r12d

    ; Patch duration
    mov     qword ptr [rdi + CMD_HIST_OFF_DURATION], rbx

    ; Mark flags = 1 (completed)
    mov     dword ptr [rdi + CMD_HIST_OFF_FLAGS], 1

@si_cc_skip_history:

    ; ---- 3. Inject OSC 633;D;<exitcode> ----
    call    si_inject_done_with_exitcode

    ; ---- 4. Increment CmdsDetected (only here — confirmed boundary) ----
    lock inc qword ptr [g_SI_Counter_CmdsDetected]

    ; ---- 5. Inject OSC 633;A (next Prompt Start) ----
    mov     ecx, OSC633_PROMPT_START
    xor     edx, edx
    xor     r8d, r8d
    call    ShellInteg_InjectOSC633

    xor     eax, eax
    xor     edx, edx
    jmp     @si_cc_exit

@si_cc_not_init:
    mov     eax, SHELLINTEG_ERR_NOT_INIT
    xor     edx, edx

@si_cc_exit:
    add     rsp, 80
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ShellInteg_CompleteCommand ENDP

; =============================================================================
;                     Internal Helper Functions
; =============================================================================

; -----------------------------------------------------------------------------
; si_close_all_pipes — Close all pipe handles (parent side)
; Preserves: rdi, rsi, r12-r15
; Clobbers:  rax, rcx, rdx, r8, r9
; -----------------------------------------------------------------------------
si_close_all_pipes PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Close each handle if valid, set to INVALID_HANDLE_VALUE after
    lea     rbx, g_hStdinWrite
    call    si_close_one_handle
    lea     rbx, g_hStdoutRead
    call    si_close_one_handle
    lea     rbx, g_hStderrRead
    call    si_close_one_handle
    lea     rbx, g_hStdinRead
    call    si_close_one_handle
    lea     rbx, g_hStdoutWrite
    call    si_close_one_handle
    lea     rbx, g_hStderrWrite
    call    si_close_one_handle

    add     rsp, 40
    pop     rbx
    ret
si_close_all_pipes ENDP

; -----------------------------------------------------------------------------
; si_close_one_handle — Close handle pointed to by RBX, set to INVALID
; -----------------------------------------------------------------------------
si_close_one_handle PROC
    mov     rcx, qword ptr [rbx]
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @si_coh_skip
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
    mov     qword ptr [rbx], INVALID_HANDLE_VALUE
@si_coh_skip:
    ret
si_close_one_handle ENDP

; -----------------------------------------------------------------------------
; si_record_command_history — Write current command into history ring buffer
;
; Expects:
;   r12 = command text pointer
;   r13 = command text length
;   r14 = start timestamp (GetTickCount64)
;
; Uses wait-free lock xadd to reserve a slot in the ring buffer.
; -----------------------------------------------------------------------------
si_record_command_history PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 64
    .allocstack 64
    .endprolog

    ; Reserve slot atomically
    mov     rax, 1
    lock xadd qword ptr [g_CmdHistoryHead], rax
    ; rax = old head (our slot index)
    and     rax, CMD_HISTORY_MASK
    shl     rax, CMD_ENTRY_SHIFT

    lea     rdi, g_CmdHistoryBuffer
    add     rdi, rax            ; rdi = entry base

    ; Zero out the entry first
    push    rdi
    xor     eax, eax
    mov     ecx, CMD_ENTRY_SIZE
    rep stosb
    pop     rdi

    ; Fill timestamp
    mov     rax, r14
    mov     qword ptr [rdi + CMD_HIST_OFF_TIMESTAMP], rax

    ; Exit code = 0 initially (updated when command completes)
    mov     dword ptr [rdi + CMD_HIST_OFF_EXITCODE], 0

    ; Command length (capped)
    mov     rax, r13
    cmp     rax, CMD_HIST_CMD_MAXLEN
    jbe     @si_hist_len_ok
    mov     rax, CMD_HIST_CMD_MAXLEN
@si_hist_len_ok:
    mov     dword ptr [rdi + CMD_HIST_OFF_CMDLEN], eax
    mov     rbx, rax            ; rbx = bytes to copy

    ; Copy command text
    lea     rsi, [r12]
    push    rdi
    add     rdi, CMD_HIST_OFF_CMDTEXT
    mov     rcx, rbx
    rep movsb
    mov     byte ptr [rdi], 0   ; Null-terminate
    pop     rdi

    ; Duration = 0 initially (updated on completion)
    mov     qword ptr [rdi + CMD_HIST_OFF_DURATION], 0

    add     rsp, 64
    pop     rdi
    pop     rsi
    pop     rbx
    ret
si_record_command_history ENDP

; -----------------------------------------------------------------------------
; si_inject_done_with_exitcode — Build and inject OSC 633;D;<exitcode>
;
; Reads g_SI_LastExitCode, converts to ASCII decimal, injects via
; ShellInteg_InjectOSC633(OSC633_EXECUTION_DONE, ascii_string, len)
; -----------------------------------------------------------------------------
si_inject_done_with_exitcode PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Convert exit code to decimal ASCII
    mov     eax, dword ptr [g_SI_LastExitCode]
    lea     rdi, g_SI_ItoBuf
    call    si_uint32_to_ascii  ; rdi = buf, eax = value, returns length in ecx

    ; Inject OSC 633;D
    mov     ecx, OSC633_EXECUTION_DONE
    lea     rdx, g_SI_ItoBuf
    xor     r8d, r8d            ; auto-detect length
    call    ShellInteg_InjectOSC633

    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
si_inject_done_with_exitcode ENDP

; -----------------------------------------------------------------------------
; si_uint32_to_ascii — Convert unsigned 32-bit integer to decimal ASCII
;
; Input:  EAX = value to convert
;         RDI = output buffer (at least 11 bytes)
; Output: Buffer filled with decimal string, null-terminated
;         ECX = string length (excluding null)
; Clobbers: rax, rcx, rdx, rdi
; -----------------------------------------------------------------------------
si_uint32_to_ascii PROC
    push    rbx
    push    rsi

    mov     ebx, eax            ; ebx = original value
    lea     rsi, [rdi + 10]     ; rsi = end of buffer
    mov     byte ptr [rsi], 0   ; Null terminate
    dec     rsi
    xor     ecx, ecx            ; digit count

    test    ebx, ebx
    jnz     @si_itoa_loop

    ; Special case: value is 0
    mov     byte ptr [rdi], '0'
    mov     byte ptr [rdi + 1], 0
    mov     ecx, 1
    pop     rsi
    pop     rbx
    ret

@si_itoa_loop:
    test    ebx, ebx
    jz      @si_itoa_reverse

    mov     eax, ebx
    xor     edx, edx
    mov     r8d, 10
    div     r8d                 ; eax = quotient, edx = remainder
    mov     ebx, eax
    add     dl, '0'
    mov     byte ptr [rsi], dl
    dec     rsi
    inc     ecx
    jmp     @si_itoa_loop

@si_itoa_reverse:
    ; Digits are in reverse order at [rsi+1..rsi+ecx]
    ; Copy forward to rdi
    inc     rsi                 ; rsi = first digit
    push    rcx                 ; save length
    push    rdi
@si_itoa_copy:
    test    ecx, ecx
    jz      @si_itoa_copy_done
    mov     al, byte ptr [rsi]
    mov     byte ptr [rdi], al
    inc     rsi
    inc     rdi
    dec     ecx
    jmp     @si_itoa_copy
@si_itoa_copy_done:
    mov     byte ptr [rdi], 0   ; Null terminate
    pop     rdi
    pop     rcx                 ; restore length

    pop     rsi
    pop     rbx
    ret
si_uint32_to_ascii ENDP

END
