; ═══════════════════════════════════════════════════════════════════
; RawrXD Task Runner — Task provider & launch configurations
; Integrates with: beacon.asm (slot 6), ui.asm (task panel)
; Exports: Task_Init, Task_LoadConfig, Task_Run, Task_Kill,
;          Task_GetOutput, Task_GetCount
;
; Architecture: Manages an array of task configurations (label,
; command, args, cwd). Tasks are executed as child processes with
; stdout/stderr captured into a ring buffer. UI polls for output
; via Task_GetOutput. JSON config parsing uses minimal key scanner.
; ═══════════════════════════════════════════════════════════════════

; ── Win32 API imports ────────────────────────────────────────────
EXTERN CreateProcessA:PROC
EXTERN CreatePipe:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN CreateThread:PROC
EXTERN TerminateProcess:PROC
EXTERN WaitForSingleObject:PROC
EXTERN GetExitCodeProcess:PROC
EXTERN CreateFileA:PROC
EXTERN GetFileSize:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN lstrlenA:PROC

; ── Cross-module imports ─────────────────────────────────────────
EXTERN BeaconSend:PROC
EXTERN g_hHeap:QWORD
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC

; ── Exports ──────────────────────────────────────────────────────
PUBLIC Task_Init
PUBLIC Task_LoadConfig
PUBLIC Task_Run
PUBLIC Task_Kill
PUBLIC Task_GetOutput
PUBLIC Task_GetCount

; ── Constants ────────────────────────────────────────────────────
HEAP_ZERO_MEMORY        equ 8
INFINITE_WAIT           equ 0FFFFFFFFh
CREATE_NO_WINDOW        equ 8000000h
STARTF_USESTDHANDLES    equ 100h
GENERIC_READ            equ 80000000h
FILE_SHARE_READ         equ 1
OPEN_EXISTING           equ 3

; Security attributes size
SA_SIZE                 equ 24

; Task config structure (640 bytes per task):
;   offset 0:   label[64]     (64 bytes, ASCII)
;   offset 64:  command[256]  (256 bytes, ASCII)
;   offset 320: args[256]     (256 bytes, ASCII)
;   offset 576: cwd[48]       (48 bytes, ASCII — short path)
;   offset 624: isBackground  (DWORD)
;   offset 628: group         (DWORD, 0=none, 1=build, 2=test)
;   offset 632: reserved      (QWORD)
TASK_CONFIG_SIZE        equ 640
MAX_TASKS               equ 50

; Task group IDs
TASK_GROUP_NONE         equ 0
TASK_GROUP_BUILD        equ 1
TASK_GROUP_TEST         equ 2

; Output ring buffer size (64KB)
OUTPUT_RING_SIZE        equ 10000h
OUTPUT_RING_MASK        equ 0FFFFh

; STARTUPINFOA size
STARTUPINFOA_SIZE       equ 104

; Beacon event IDs (slot 6)
TASK_BEACON_SLOT        equ 6
TASK_EVT_CONFIG_LOADED  equ 04001h
TASK_EVT_STARTED        equ 04002h
TASK_EVT_COMPLETED      equ 04003h
TASK_EVT_TERMINATED     equ 04004h
TASK_EVT_OUTPUT         equ 04010h

; Pipe buffer
PIPE_BUF_SIZE           equ 4096

; File read buffer
FILE_BUF_SIZE           equ 8192

; ── SECURITY_ATTRIBUTES struct ───────────────────────────────────
SECURITY_ATTRIBUTES STRUCT
    nLength             dd ?
    lpSecurityDescriptor dq ?
    bInheritHandle      dd ?
    padding             dd ?
SECURITY_ATTRIBUTES ENDS

; ── STARTUPINFOA struct ──────────────────────────────────────────
STARTUPINFOA STRUCT
    cb              dd ?
    lpReserved      dq ?
    lpDesktop       dq ?
    lpTitle         dq ?
    dwX             dd ?
    dwY             dd ?
    dwXSize         dd ?
    dwYSize         dd ?
    dwXCountChars   dd ?
    dwYCountChars   dd ?
    dwFillAttribute dd ?
    dwFlags         dd ?
    wShowWindow     dw ?
    cbReserved2     dw ?
    lpReserved2     dq ?
    hStdInput       dq ?
    hStdOutput      dq ?
    hStdError       dq ?
STARTUPINFOA ENDS

; ── PROCESS_INFORMATION struct ───────────────────────────────────
PROCESS_INFORMATION STRUCT
    hProcess        dq ?
    hThread         dq ?
    dwProcessId     dd ?
    dwThreadId      dd ?
PROCESS_INFORMATION ENDS

; ═════════════════════════════════════════════════════════════════
.data
align 8
g_pTaskConfigs      dq 0               ; Heap pointer to config array
g_taskCount         dd 0               ; Number of loaded task configs
g_hActiveProcess    dq 0               ; Currently running task process
g_hReadPipe         dq 0               ; Stdout read pipe (active task)
g_hWritePipe        dq 0               ; Stdout write pipe (active task)
g_activeTaskIdx     dd -1              ; Index of running task (-1 = none)
g_exitCode          dd 0               ; Last task exit code

; Output ring buffer pointers
g_pOutputRing       dq 0               ; Heap pointer to ring buffer
g_ringWritePos      dd 0               ; Write position (mod RING_SIZE)
g_ringReadPos       dd 0               ; Read position (mod RING_SIZE)

.data?
align 16
; Command line assembly buffer (512 bytes)
g_cmdLineBuf        db 512 dup(?)
; File read buffer for config loading
g_fileBuf           db FILE_BUF_SIZE dup(?)
; Pipe read buffer
g_pipeReadBuf       db PIPE_BUF_SIZE dup(?)

.const
szConfigLoaded      db "Tasks config loaded",0
szTaskStarted       db "Task started",0
szTaskCompleted     db "Task completed",0
szTaskTerminated    db "Task terminated",0
szDefaultLabel      db "build",0
szDefaultCommand    db "powershell.exe",0
szDefaultArgs       db "-ExecutionPolicy Bypass -File .\\scripts\\build_monolithic.ps1",0

; ═════════════════════════════════════════════════════════════════
.code

; ────────────────────────────────────────────────────────────────
; Task_Init — Allocate config array and output ring buffer
;   No args. Returns EAX=0 success, -1 failure.
;
; Stack: 0 pushes, sub 28h (40)
;   8+40 = 48 → 48 mod 16 = 0 ✓
; ────────────────────────────────────────────────────────────────
Task_Init PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Allocate task config array: MAX_TASKS * TASK_CONFIG_SIZE = 32000 bytes
    mov     rcx, g_hHeap
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, MAX_TASKS * TASK_CONFIG_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      @ti_fail
    mov     g_pTaskConfigs, rax

    ; Allocate output ring buffer: 64KB
    mov     rcx, g_hHeap
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, OUTPUT_RING_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      @ti_fail
    mov     g_pOutputRing, rax

    ; Reset state
    mov     g_taskCount, 0
    mov     g_hActiveProcess, 0
    mov     g_activeTaskIdx, -1
    mov     g_ringWritePos, 0
    mov     g_ringReadPos, 0
    mov     g_exitCode, 0

    ; Register default "build" task
    call    Task_RegisterDefaults

    add     rsp, 28h
    xor     eax, eax
    ret

@ti_fail:
    add     rsp, 28h
    mov     eax, -1
    ret
Task_Init ENDP

; ────────────────────────────────────────────────────────────────
; Task_RegisterDefaults — Add built-in tasks (build, test, audit)
;   Internal helper. No args.
;
; Stack: 1 push (rbx) + 28h (40)
;   8+8+40 = 56 → 56 mod 16 = 8 → need 30h (48)
;   8+8+48 = 64 → 64 mod 16 = 0 ✓
; ────────────────────────────────────────────────────────────────
Task_RegisterDefaults PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; ── Task 0: "build" ──
    mov     rax, g_pTaskConfigs         ; &configs[0]

    ; Label
    mov     rcx, rax
    lea     rdx, szDefaultLabel
    call    lstrcpyA

    ; Command
    mov     rax, g_pTaskConfigs
    lea     rcx, [rax+64]
    lea     rdx, szDefaultCommand
    call    lstrcpyA

    ; Args
    mov     rax, g_pTaskConfigs
    lea     rcx, [rax+320]
    lea     rdx, szDefaultArgs
    call    lstrcpyA

    ; Group = BUILD
    mov     rax, g_pTaskConfigs
    mov     dword ptr [rax+628], TASK_GROUP_BUILD

    mov     g_taskCount, 1

    ; ── Task 1: "smoke test" ──
    mov     rax, g_pTaskConfigs
    add     rax, TASK_CONFIG_SIZE       ; &configs[1]

    mov     rcx, rax
    lea     rdx, szSmokeLabel
    call    lstrcpyA

    mov     rax, g_pTaskConfigs
    add     rax, TASK_CONFIG_SIZE
    lea     rcx, [rax+64]
    lea     rdx, szDefaultCommand
    call    lstrcpyA

    mov     rax, g_pTaskConfigs
    add     rax, TASK_CONFIG_SIZE
    lea     rcx, [rax+320]
    lea     rdx, szSmokeArgs
    call    lstrcpyA

    mov     rax, g_pTaskConfigs
    add     rax, TASK_CONFIG_SIZE
    mov     dword ptr [rax+628], TASK_GROUP_TEST

    mov     g_taskCount, 2

    ; ── Task 2: "audit" ──
    mov     rax, g_pTaskConfigs
    mov     rbx, rax
    add     rbx, TASK_CONFIG_SIZE
    add     rbx, TASK_CONFIG_SIZE       ; &configs[2]

    mov     rcx, rbx
    lea     rdx, szAuditLabel
    call    lstrcpyA

    lea     rcx, [rbx+64]
    lea     rdx, szDefaultCommand
    call    lstrcpyA

    lea     rcx, [rbx+320]
    lea     rdx, szAuditArgs
    call    lstrcpyA

    mov     dword ptr [rbx+628], TASK_GROUP_TEST

    mov     g_taskCount, 3

    add     rsp, 30h
    pop     rbx
    ret
Task_RegisterDefaults ENDP

; ────────────────────────────────────────────────────────────────
; Task_LoadConfig — Load tasks from a JSON config file
;   RCX = pJsonPath (LPCSTR, path to tasks.json)
;         If NULL, keeps defaults only.
;   Returns: EAX = number of tasks loaded
;
; Minimal JSON parser: scans for "label":" and "command":" keys.
;
; Stack: 3 pushes (rbx, rsi, rdi) + 58h (88)
;   8+24+88 = 120 → 120 mod 16 = 8 → need 60h (96)
;   8+24+96 = 128 → 128 mod 16 = 0 ✓
; ────────────────────────────────────────────────────────────────
Task_LoadConfig PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 60h
    .allocstack 60h
    .endprolog

    test    rcx, rcx
    jz      @lc_default

    mov     rbx, rcx                    ; rbx = pJsonPath

    ; Open config file
    ; CreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
    ;   lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile)
    mov     rcx, rbx                    ; lpFileName
    mov     edx, GENERIC_READ           ; dwDesiredAccess
    mov     r8d, FILE_SHARE_READ        ; dwShareMode
    xor     r9, r9                      ; lpSecurityAttributes = NULL
    mov     dword ptr [rsp+20h], OPEN_EXISTING
    mov     dword ptr [rsp+28h], 0      ; dwFlagsAndAttributes = 0
    mov     qword ptr [rsp+30h], 0      ; hTemplateFile = NULL
    call    CreateFileA
    cmp     rax, -1
    je      @lc_default                 ; file not found, keep defaults
    mov     rsi, rax                    ; rsi = hFile

    ; Get file size
    mov     rcx, rsi
    xor     edx, edx
    call    GetFileSize
    cmp     eax, FILE_BUF_SIZE - 1
    jg      @lc_close                   ; file too large, skip
    mov     edi, eax                    ; edi = fileSize

    ; Read entire file
    mov     rcx, rsi                    ; hFile
    lea     rdx, g_fileBuf              ; lpBuffer
    mov     r8d, edi                    ; nNumberOfBytesToRead
    lea     r9, [rsp+40h]              ; lpNumberOfBytesRead
    mov     qword ptr [rsp+20h], 0     ; lpOverlapped = NULL
    call    ReadFile

    ; Null-terminate
    lea     rax, g_fileBuf
    mov     byte ptr [rax + rdi], 0

    ; Close file
    mov     rcx, rsi
    call    CloseHandle

    ; ── Parse JSON: scan for task objects ──
    ; Look for "label" keys, extract value as task label
    ; Look for "command" keys, extract value as task command
    ; Simple state machine: find '"label"', skip to ':', skip '"', copy until '"'
    lea     rsi, g_fileBuf              ; rsi = scan pointer
    xor     ebx, ebx                    ; ebx = current task index offset

@lc_scan:
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @lc_parse_done

    cmp     al, '"'
    jne     @lc_scan_next

    ; Check if this is "label" key
    cmp     byte ptr [rsi+1], 'l'
    jne     @lc_check_cmd
    cmp     byte ptr [rsi+2], 'a'
    jne     @lc_check_cmd
    cmp     byte ptr [rsi+3], 'b'
    jne     @lc_check_cmd
    cmp     byte ptr [rsi+4], 'e'
    jne     @lc_check_cmd
    cmp     byte ptr [rsi+5], 'l'
    jne     @lc_check_cmd
    cmp     byte ptr [rsi+6], '"'
    jne     @lc_check_cmd

    ; Found "label" — skip to value
    add     rsi, 7                      ; past "label"
    call    Task_SkipToValue            ; rsi → first char of value
    ; Copy value to current task's label field
    mov     eax, g_taskCount
    cmp     eax, MAX_TASKS
    jge     @lc_parse_done
    movsxd  rcx, eax
    imul    rcx, TASK_CONFIG_SIZE
    add     rcx, g_pTaskConfigs         ; rcx = &config[count].label
    call    Task_CopyValue              ; copies from rsi to rcx, advances rsi
    jmp     @lc_scan

@lc_check_cmd:
    ; Check if this is "command" key
    cmp     byte ptr [rsi+1], 'c'
    jne     @lc_check_args
    cmp     byte ptr [rsi+2], 'o'
    jne     @lc_check_args
    cmp     byte ptr [rsi+3], 'm'
    jne     @lc_check_args

    ; Found "command" or "cmd" — skip to value
    ; Find closing quote of key
@lc_find_cmd_end:
    inc     rsi
    cmp     byte ptr [rsi], '"'
    jne     @lc_find_cmd_end
    inc     rsi                         ; past closing quote
    call    Task_SkipToValue
    ; Copy to current task's command field
    mov     eax, g_taskCount
    cmp     eax, MAX_TASKS
    jge     @lc_parse_done
    movsxd  rcx, eax
    imul    rcx, TASK_CONFIG_SIZE
    add     rcx, g_pTaskConfigs
    add     rcx, 64                     ; offset to command field
    call    Task_CopyValue
    ; After getting both label and command, increment task count
    inc     g_taskCount
    jmp     @lc_scan

@lc_check_args:
    ; Skip other keys
@lc_scan_next:
    inc     rsi
    jmp     @lc_scan

@lc_parse_done:
@lc_close:
@lc_default:
    ; Notify UI
    xor     ecx, ecx
    lea     rdx, szConfigLoaded
    mov     r8d, TASK_EVT_CONFIG_LOADED
    call    BeaconSend

    mov     eax, g_taskCount
    add     rsp, 60h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Task_LoadConfig ENDP

; ────────────────────────────────────────────────────────────────
; Task_SkipToValue — Advance RSI past ':' and whitespace to opening '"'
;   Advances RSI past the opening quote.
;   Returns with RSI pointing to first char of string value.
; ────────────────────────────────────────────────────────────────
Task_SkipToValue PROC
@stv_colon:
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @stv_done
    cmp     al, ':'
    je      @stv_past_colon
    inc     rsi
    jmp     @stv_colon
@stv_past_colon:
    inc     rsi
@stv_ws:
    movzx   eax, byte ptr [rsi]
    cmp     al, ' '
    je      @stv_skip
    cmp     al, 9                       ; tab
    je      @stv_skip
    cmp     al, '"'
    je      @stv_quote
    jmp     @stv_done
@stv_skip:
    inc     rsi
    jmp     @stv_ws
@stv_quote:
    inc     rsi                         ; skip opening quote
@stv_done:
    ret
Task_SkipToValue ENDP

; ────────────────────────────────────────────────────────────────
; Task_CopyValue — Copy string value from RSI to RCX until '"' or null
;   RSI = source (inside JSON string value)
;   RCX = destination buffer
;   Max 62 bytes copied. Advances RSI past closing quote.
; ────────────────────────────────────────────────────────────────
Task_CopyValue PROC
    xor     edx, edx                    ; byte counter
@cv_loop:
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @cv_done
    cmp     al, '"'
    je      @cv_end_quote
    cmp     edx, 62
    jge     @cv_end_quote               ; truncate
    mov     byte ptr [rcx + rdx], al
    inc     edx
    inc     rsi
    jmp     @cv_loop
@cv_end_quote:
    inc     rsi                         ; skip closing quote
@cv_done:
    mov     byte ptr [rcx + rdx], 0     ; null terminate
    ret
Task_CopyValue ENDP

; ────────────────────────────────────────────────────────────────
; Task_Run — Execute task by index
;   ECX = taskIndex (0-based)
;   Returns: EAX = 1 started, 0 failure
;
; Stack: 3 pushes (rbx, rsi, rdi) + 100h (256)
;   8+24+256 = 288 → 288 mod 16 = 0 ✓
; ────────────────────────────────────────────────────────────────
Task_Run PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 100h
    .allocstack 100h
    .endprolog

    mov     ebx, ecx                    ; ebx = taskIndex

    ; Bounds check
    cmp     ebx, g_taskCount
    jge     @tr_fail

    ; Check not already running
    mov     rax, g_hActiveProcess
    test    rax, rax
    jnz     @tr_fail

    ; Get config pointer
    movsxd  rax, ebx
    imul    rax, TASK_CONFIG_SIZE
    add     rax, g_pTaskConfigs
    mov     rsi, rax                    ; rsi = &config[taskIndex]

    ; Reset output ring
    mov     g_ringWritePos, 0
    mov     g_ringReadPos, 0

    ; Build command line: command + " " + args
    lea     rdi, g_cmdLineBuf
    lea     rcx, g_cmdLineBuf
    lea     rdx, [rsi+64]              ; command field
    call    lstrcpyA

    ; Check if args field is non-empty
    movzx   eax, byte ptr [rsi+320]
    test    al, al
    jz      @tr_no_args

    lea     rcx, g_cmdLineBuf
    lea     rdx, szSpace
    call    lstrcatA

    lea     rcx, g_cmdLineBuf
    lea     rdx, [rsi+320]             ; args field
    call    lstrcatA

@tr_no_args:
    ; Create pipe for stdout/stderr capture
    ; SECURITY_ATTRIBUTES at [rsp+30h]
    mov     dword ptr [rsp+30h], SA_SIZE
    mov     qword ptr [rsp+38h], 0
    mov     dword ptr [rsp+40h], 1      ; bInheritHandle = TRUE

    lea     rcx, [rsp+50h]              ; &hReadPipe
    lea     rdx, [rsp+58h]              ; &hWritePipe
    lea     r8, [rsp+30h]               ; &sa
    xor     r9d, r9d
    call    CreatePipe
    test    eax, eax
    jz      @tr_fail

    mov     rax, qword ptr [rsp+50h]
    mov     g_hReadPipe, rax
    mov     rax, qword ptr [rsp+58h]
    mov     g_hWritePipe, rax

    ; STARTUPINFOA at [rsp+60h] (104 bytes)
    lea     rdi, [rsp+60h]
    xor     eax, eax
    mov     ecx, 26                     ; 104/4
    rep     stosd
    mov     dword ptr [rsp+60h], STARTUPINFOA_SIZE
    mov     dword ptr [rsp+60h+2Ch], STARTF_USESTDHANDLES

    ; Redirect stdout and stderr to write pipe
    mov     rax, g_hWritePipe
    mov     qword ptr [rsp+60h+50h], rax    ; hStdOutput
    mov     qword ptr [rsp+60h+58h], rax    ; hStdError

    ; PROCESS_INFORMATION at [rsp+0D0h]
    mov     qword ptr [rsp+0D0h], 0
    mov     qword ptr [rsp+0D8h], 0
    mov     qword ptr [rsp+0E0h], 0

    ; CreateProcessA
    xor     ecx, ecx                    ; lpApplicationName = NULL
    lea     rdx, g_cmdLineBuf           ; lpCommandLine
    xor     r8, r8                      ; lpProcessAttributes
    xor     r9, r9                      ; lpThreadAttributes
    mov     dword ptr [rsp+20h], 1      ; bInheritHandles = TRUE
    mov     dword ptr [rsp+28h], CREATE_NO_WINDOW
    mov     qword ptr [rsp+30h], 0      ; lpEnvironment
    mov     qword ptr [rsp+38h], 0      ; lpCurrentDirectory
    lea     rax, [rsp+60h]
    mov     qword ptr [rsp+40h], rax    ; lpStartupInfo
    lea     rax, [rsp+0D0h]
    mov     qword ptr [rsp+48h], rax    ; lpProcessInformation
    xor     ecx, ecx
    lea     rdx, g_cmdLineBuf
    xor     r8, r8
    xor     r9, r9
    call    CreateProcessA
    test    eax, eax
    jz      @tr_close_pipes

    ; Save process handle
    mov     rax, qword ptr [rsp+0D0h]
    mov     g_hActiveProcess, rax
    mov     g_activeTaskIdx, ebx

    ; Close write end in parent
    mov     rcx, g_hWritePipe
    call    CloseHandle
    mov     g_hWritePipe, 0

    ; Spawn output reader thread
    xor     ecx, ecx                    ; lpThreadAttributes
    xor     edx, edx                    ; dwStackSize
    lea     r8, Task_OutputReader        ; lpStartAddress
    mov     r9, g_hReadPipe             ; lpParameter = hReadPipe
    mov     dword ptr [rsp+20h], 0      ; dwCreationFlags
    mov     qword ptr [rsp+28h], 0      ; lpThreadId
    call    CreateThread

    ; Notify UI
    xor     ecx, ecx
    lea     rdx, [rsi]                  ; task label
    mov     r8d, TASK_EVT_STARTED
    call    BeaconSend

    mov     eax, 1
    add     rsp, 100h
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@tr_close_pipes:
    mov     rcx, g_hReadPipe
    call    CloseHandle
    mov     rcx, g_hWritePipe
    call    CloseHandle

@tr_fail:
    xor     eax, eax
    add     rsp, 100h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Task_Run ENDP

; ────────────────────────────────────────────────────────────────
; Task_OutputReader — Background thread: read pipe into ring buffer
;   RCX = hPipe (passed as lpParameter)
;
; Stack: 1 push (rbx) + 38h (56)
;   8+8+56 = 72 → 72 mod 16 = 8 → need 40h (64)
;   8+8+64 = 80 → 80 mod 16 = 0 ✓
; ────────────────────────────────────────────────────────────────
Task_OutputReader PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    mov     rbx, rcx                    ; rbx = hPipe

@tor_read:
    ; ReadFile(hPipe, buffer, bufSize, &bytesRead, NULL)
    mov     rcx, rbx
    lea     rdx, g_pipeReadBuf
    mov     r8d, PIPE_BUF_SIZE - 1
    lea     r9, [rsp+30h]              ; &bytesRead
    mov     qword ptr [rsp+20h], 0     ; lpOverlapped
    call    ReadFile
    test    eax, eax
    jz      @tor_done

    mov     ecx, dword ptr [rsp+30h]   ; bytesRead
    test    ecx, ecx
    jz      @tor_done

    ; Write bytes into ring buffer
    mov     rdi, g_pOutputRing
    lea     rsi, g_pipeReadBuf
    xor     edx, edx                    ; source offset
@tor_copy:
    cmp     edx, ecx
    jge     @tor_notify
    movzx   eax, byte ptr [rsi + rdx]
    mov     r8d, g_ringWritePos
    mov     byte ptr [rdi + r8], al
    inc     r8d
    and     r8d, OUTPUT_RING_MASK
    mov     g_ringWritePos, r8d
    inc     edx
    jmp     @tor_copy

@tor_notify:
    ; Send output event via Beacon (incremental)
    xor     ecx, ecx
    lea     rdx, g_pipeReadBuf
    mov     r8d, TASK_EVT_OUTPUT
    call    BeaconSend
    jmp     @tor_read

@tor_done:
    ; Pipe closed → process finished
    mov     rcx, rbx
    call    CloseHandle

    ; Wait for process exit
    mov     rcx, g_hActiveProcess
    test    rcx, rcx
    jz      @tor_send_complete
    mov     edx, INFINITE_WAIT
    call    WaitForSingleObject

    ; Get exit code
    mov     rcx, g_hActiveProcess
    lea     rdx, g_exitCode
    call    GetExitCodeProcess

    ; Close process handle
    mov     rcx, g_hActiveProcess
    call    CloseHandle
    mov     g_hActiveProcess, 0
    mov     g_activeTaskIdx, -1

@tor_send_complete:
    ; Notify UI: task completed
    xor     ecx, ecx
    lea     rdx, szTaskCompleted
    mov     r8d, TASK_EVT_COMPLETED
    call    BeaconSend

    add     rsp, 40h
    pop     rbx
    xor     eax, eax
    ret
Task_OutputReader ENDP

; ────────────────────────────────────────────────────────────────
; Task_Kill — Terminate the currently running task
;   No args. Returns EAX = 1 killed, 0 no task running.
;
; Stack: 0 pushes, sub 28h
;   8+40 = 48 → 0 ✓
; ────────────────────────────────────────────────────────────────
Task_Kill PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     rcx, g_hActiveProcess
    test    rcx, rcx
    jz      @tk_none

    ; TerminateProcess(hProcess, exitCode=1)
    mov     edx, 1
    call    TerminateProcess

    ; Close handle
    mov     rcx, g_hActiveProcess
    call    CloseHandle
    mov     g_hActiveProcess, 0
    mov     g_activeTaskIdx, -1

    ; Close read pipe if still open
    mov     rcx, g_hReadPipe
    test    rcx, rcx
    jz      @tk_no_pipe
    call    CloseHandle
    mov     g_hReadPipe, 0
@tk_no_pipe:

    ; Notify UI
    xor     ecx, ecx
    lea     rdx, szTaskTerminated
    mov     r8d, TASK_EVT_TERMINATED
    call    BeaconSend

    add     rsp, 28h
    mov     eax, 1
    ret

@tk_none:
    add     rsp, 28h
    xor     eax, eax
    ret
Task_Kill ENDP

; ────────────────────────────────────────────────────────────────
; Task_GetOutput — Read available output from ring buffer
;   RCX = pBuffer (destination, caller-allocated)
;   EDX = maxLen (max bytes to copy)
;   Returns: EAX = bytes copied
; ────────────────────────────────────────────────────────────────
Task_GetOutput PROC
    mov     r8, rcx                     ; r8 = pBuffer
    mov     r9d, edx                    ; r9d = maxLen
    xor     eax, eax                    ; bytes copied

    mov     r10, g_pOutputRing
    test    r10, r10
    jz      @go_done

@go_loop:
    cmp     eax, r9d
    jge     @go_done

    mov     ecx, g_ringReadPos
    cmp     ecx, g_ringWritePos
    je      @go_done                    ; ring empty

    movzx   edx, byte ptr [r10 + rcx]
    mov     byte ptr [r8 + rax], dl
    inc     ecx
    and     ecx, OUTPUT_RING_MASK
    mov     g_ringReadPos, ecx
    inc     eax
    jmp     @go_loop

@go_done:
    ret
Task_GetOutput ENDP

; ────────────────────────────────────────────────────────────────
; Task_GetCount — Return number of registered tasks
;   Returns: EAX = g_taskCount
; ────────────────────────────────────────────────────────────────
Task_GetCount PROC
    mov     eax, g_taskCount
    ret
Task_GetCount ENDP

; ═════════════════════════════════════════════════════════════════
.const
szSpace             db " ",0
szSmokeLabel        db "smoke-test",0
szSmokeArgs         db "-ExecutionPolicy Bypass -File .\\scripts\\smoke_runtime.ps1",0
szAuditLabel        db "audit",0
szAuditArgs         db "-ExecutionPolicy Bypass -File .\\scripts\\audit_entropy.ps1",0

END
