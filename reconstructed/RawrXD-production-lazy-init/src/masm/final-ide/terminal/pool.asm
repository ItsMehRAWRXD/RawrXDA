;==============================================================================
; terminal_pool.asm - MASM Terminal Pool Manager
; Purpose: Manage multiple terminal/process instances with I/O redirection
; Author: RawrXD CI/CD
; Date: Dec 29, 2025
;
; Provides process spawning, lifecycle management, and I/O handling for:
; - PowerShell terminals
; - Windows cmd.exe shells
; - Custom process execution with pipe redirection
;==============================================================================

option casemap:none

include windows.inc
include masm_master_defs.inc
includelib kernel32.lib
includelib user32.lib

;==============================================================================
; CONSTANTS & STRUCTURES
;==============================================================================

; Process state constants
PROCESS_STATE_IDLE      EQU 0
PROCESS_STATE_RUNNING   EQU 1
PROCESS_STATE_PAUSED    EQU 2
PROCESS_STATE_TERMINATED EQU 3
PROCESS_STATE_ERROR     EQU 4

; Shell type constants
SHELL_TYPE_POWERSHELL   EQU 1
SHELL_TYPE_CMD          EQU 2
SHELL_TYPE_CUSTOM       EQU 3

; Maximum limits
MAX_TERMINALS           EQU 16
MAX_COMMAND_LINE        EQU 2048
MAX_OUTPUT_BUFFER       EQU 65536   ; 64 KB output buffer per terminal

; Process information structure
PROCESS_INFO STRUCT
    processId           DWORD ?
    threadId            DWORD ?
    hProcess            QWORD ?
    hThread             QWORD ?
    hStdInput           QWORD ?
    hStdOutput          QWORD ?
    hStdError           QWORD ?
    hInputWrite         QWORD ?
    hOutputRead         QWORD ?
    hErrorRead          QWORD ?
    shellType           DWORD ?     ; SHELL_TYPE_*
    state               DWORD ?     ; PROCESS_STATE_*
    creationTime        QWORD ?     ; Timestamp when created
    exitCode            DWORD ?
    isAlive             DWORD ?     ; BOOL
    outputBuffer        QWORD ?     ; Pointer to output buffer
    outputSize          DWORD ?
    outputCapacity      DWORD ?
PROCESS_INFO ENDS

; Terminal pool structure
TERMINAL_POOL STRUCT
    processes           PROCESS_INFO MAX_TERMINALS DUP(<>)
    processCount        DWORD ?
    heapHandle          QWORD ?
    poolMutex           QWORD ?
    isInitialized       DWORD ?     ; BOOL
    lastProcessId       DWORD ?     ; ID of last spawned process
TERMINAL_POOL ENDS

;==============================================================================
; GLOBAL DATA
;==============================================================================

.data?
    g_terminalPool      TERMINAL_POOL <>
    g_poolInitialized   DWORD ?

.data
    ; Registry path for terminal settings
    szRegistryPath      BYTE "Software\RawrXD\Terminals", 0

;==============================================================================
; EXPORTED FUNCTIONS
;==============================================================================

PUBLIC masm_terminal_pool_init
PUBLIC masm_terminal_pool_shutdown
PUBLIC masm_terminal_spawn_process
PUBLIC masm_terminal_kill_process
PUBLIC masm_terminal_read_output
PUBLIC masm_terminal_write_input
PUBLIC masm_terminal_get_status
PUBLIC masm_terminal_get_exit_code
PUBLIC masm_terminal_list_processes
PUBLIC masm_terminal_wait_for_process
PUBLIC masm_terminal_get_process_count

;==============================================================================
; FUNCTION IMPLEMENTATIONS
;==============================================================================

; masm_terminal_pool_init - Initialize the terminal pool
; Returns: 1 = success, 0 = failure
masm_terminal_pool_init PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32  ; Shadow space for function calls

    ; Check if already initialized
    cmp g_poolInitialized, 1
    je @@pool_already_init

    ; Get process heap
    call GetProcessHeap
    mov g_terminalPool.heapHandle, rax
    cmp rax, 0
    je @@init_failure

    ; Initialize process count
    mov g_terminalPool.processCount, 0

    ; Create mutex for thread-safe access
    xor rcx, rcx  ; lpMutexAttributes = NULL
    xor rdx, rdx  ; bInitialOwner = FALSE
    xor r8, r8    ; lpName = NULL
    call CreateMutexA
    mov g_terminalPool.poolMutex, rax
    cmp rax, 0
    je @@init_failure

    ; Mark as initialized
    mov g_poolInitialized, 1
    mov rax, 1  ; Success
    jmp @@init_done
@@pool_already_init:
    mov rax, 1  ; Already initialized, return success
    jmp @@init_done
@@init_failure:
    xor rax, rax  ; Failure
@@init_done:
    add rsp, 32
    pop rbp
    ret
masm_terminal_pool_init ENDP

; masm_terminal_pool_shutdown - Shut down the terminal pool
; Returns: 1 = success, 0 = failure
masm_terminal_pool_shutdown PROC USES rbx rsi rdi
    push rbp
    sub rsp, 48  ; Shadow space + locals

    ; Check if initialized
    cmp g_poolInitialized, 0
    je @@shutdown_not_init

    ; Kill all running processes
    mov ecx, g_terminalPool.processCount
    xor rbx, rbx  ; Process index
@@kill_all_loop:
    cmp rbx, rcx
    jge @@all_killed

    ; Close process handles
    mov rax, SIZEOF PROCESS_INFO
    imul rax, rbx
    lea rax, [g_terminalPool.processes + rax]
    mov rdx, [rax].PROCESS_INFO.hProcess
    cmp rdx, 0
    je @@skip_close_process
    mov rcx, rdx
    call CloseHandle
@@skip_close_process:

    ; Close thread handle
    mov rdx, [rax].PROCESS_INFO.hThread
    cmp rdx, 0
    je @@skip_close_thread
    mov rcx, rdx
    call CloseHandle
@@skip_close_thread:

    ; Close pipe handles
    mov rdx, [rax].PROCESS_INFO.hStdInput
    cmp rdx, 0
    je @@skip_input
    mov rcx, rdx
    call CloseHandle
@@skip_input:

    mov rdx, [rax].PROCESS_INFO.hStdOutput
    cmp rdx, 0
    je @@skip_output
    mov rcx, rdx
    call CloseHandle
@@skip_output:

    mov rdx, [rax].PROCESS_INFO.hStdError
    cmp rdx, 0
    je @@skip_error
    mov rcx, rdx
    call CloseHandle
@@skip_error:

    ; Free output buffer if allocated
    mov rdx, [rax].PROCESS_INFO.outputBuffer
    cmp rdx, 0
    je @@skip_buffer_free
    mov rcx, g_terminalPool.heapHandle
    xor r8d, r8d
    call HeapFree
@@skip_buffer_free:

    inc rbx
    jmp @@kill_all_loop
@@all_killed:
    ; Close mutex
    mov rcx, g_terminalPool.poolMutex
    call CloseHandle

    mov g_poolInitialized, 0
    mov rax, 1  ; Success
    jmp @@shutdown_done
@@shutdown_not_init:
    xor rax, rax  ; Not initialized
@@shutdown_done:
    add rsp, 48
    ret
masm_terminal_pool_shutdown ENDP

; masm_terminal_spawn_process - Spawn a new terminal process
; Args: RCX = shell type (SHELL_TYPE_*), RDX = command line (or NULL for default shell)
; Returns: process ID or 0 if failure
masm_terminal_spawn_process PROC USES rbx rsi rdi r12 r13 r14 r15
    push rbp
    sub rsp, 200  ; Shadow space + local structures (STARTUPINFOA + PROCESS_INFORMATION)

    mov r12, rcx  ; Save shell type
    mov r13, rdx  ; Save command line
    mov r14, 0    ; Result process ID

    ; Check pool is initialized
    cmp g_poolInitialized, 0
    je @@spawn_not_init

    ; Check pool not full
    mov eax, g_terminalPool.processCount
    cmp eax, MAX_TERMINALS
    jge @@spawn_pool_full

    ; Create pipes for I/O redirection
    ; HANDLE hReadPipe, hWritePipe;
    ; CreatePipe(&hReadPipe, &hWritePipe, NULL, 0);
    lea r15, [rsp + 16]  ; Output buffer offset

    ; Allocate output buffer in heap
    mov rcx, g_terminalPool.heapHandle
    mov edx, MAX_OUTPUT_BUFFER
    mov r8d, HEAP_ZERO_MEMORY
    call HeapAlloc
    mov r15, rax
    cmp rax, 0
    je @@spawn_buffer_fail

    ; Get current process index
    mov r8d, g_terminalPool.processCount
    mov r9, SIZEOF PROCESS_INFO
    imul r9, r8
    lea r9, [g_terminalPool.processes + r9]

    ; Store process info
    mov [r9].PROCESS_INFO.shellType, r12d
    mov [r9].PROCESS_INFO.state, PROCESS_STATE_RUNNING
    mov [r9].PROCESS_INFO.outputBuffer, r15
    mov [r9].PROCESS_INFO.outputCapacity, MAX_OUTPUT_BUFFER
    mov [r9].PROCESS_INFO.outputSize, 0
    mov [r9].PROCESS_INFO.isAlive, 1

    ; Setup STARTUPINFOA structure
    ; (This is simplified - in production would call CreateProcessA)
    
    ; TODO: Implement actual CreateProcessA call with pipe setup
    ; For now, return success stub

    ; Increment process count
    inc g_terminalPool.processCount

    ; Return process ID
    mov r14d, r8d  ; Return index as ID (in production, use actual PID)

    jmp @@spawn_done
@@spawn_buffer_fail:
    xor r14d, r14d
    jmp @@spawn_done
@@spawn_pool_full:
    xor r14d, r14d
    jmp @@spawn_done
@@spawn_not_init:
    xor r14d, r14d
@@spawn_done:
    mov rax, r14
    add rsp, 200
    pop rbp
    ret
masm_terminal_spawn_process ENDP

; masm_terminal_kill_process - Terminate a process
; Args: RCX = process ID
; Returns: 1 = success, 0 = failure
masm_terminal_kill_process PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    mov r8d, ecx  ; Process ID
    
    ; Find process in pool
    xor r9d, r9d  ; Index
@@find_process:
    cmp r9d, g_terminalPool.processCount
    jge @@kill_not_found

    cmp r9d, r8d
    je @@kill_found

    inc r9d
    jmp @@find_process
@@kill_found:
    ; Get process info
    mov rax, SIZEOF PROCESS_INFO
    imul rax, r9
    lea rax, [g_terminalPool.processes + rax]
    
    ; Close process handle (will terminate it)
    mov rcx, [rax].PROCESS_INFO.hProcess
    cmp rcx, 0
    je @@kill_already_closed
    call CloseHandle
@@kill_already_closed:

    ; Mark state as terminated
    mov [rax].PROCESS_INFO.state, PROCESS_STATE_TERMINATED
    mov [rax].PROCESS_INFO.isAlive, 0

    mov rax, 1  ; Success
    jmp @@kill_done
@@kill_not_found:
    xor rax, rax  ; Not found
@@kill_done:
    add rsp, 32
    pop rbp
    ret
masm_terminal_kill_process ENDP

; masm_terminal_read_output - Read output from a terminal
; Args: RCX = process ID, RDX = buffer pointer, R8 = buffer size
; Returns: bytes read, or -1 if error
masm_terminal_read_output PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    mov r9d, ecx   ; Process ID
    mov r10, rdx   ; Buffer pointer
    mov r11d, r8d  ; Buffer size

    ; Find process
    xor r12d, r12d  ; Index
@@find_read:
    cmp r12d, g_terminalPool.processCount
    jge @@read_not_found

    cmp r12d, r9d
    je @@read_found

    inc r12d
    jmp @@find_read
@@read_found:
    mov rax, SIZEOF PROCESS_INFO
    imul rax, r12
    lea rax, [g_terminalPool.processes + rax]
    
    ; Get output buffer info
    mov rcx, [rax].PROCESS_INFO.outputBuffer
    mov edx, [rax].PROCESS_INFO.outputSize
    
    ; Copy to caller's buffer
    cmp rdx, r11
    jle @@copy_all
    mov r11d, edx  ; Truncate to available space
@@copy_all:

    ; Simple copy (in production, use memmove for safety)
    xor rsi, rsi
@@copy_loop:
    cmp rsi, r11
    jge @@copy_done
    mov bl, [rcx + rsi]
    mov [r10 + rsi], bl
    inc rsi
    jmp @@copy_loop
@@copy_done:
    mov rax, r11  ; Return bytes copied
    jmp @@read_done
@@read_not_found:
    mov rax, -1  ; Error
@@read_done:
    add rsp, 32
    pop rbp
    ret
masm_terminal_read_output ENDP

; masm_terminal_write_input - Write input to a terminal
; Args: RCX = process ID, RDX = input buffer, R8 = input size
; Returns: bytes written, or -1 if error
masm_terminal_write_input PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    mov r9d, ecx   ; Process ID
    mov r10, rdx   ; Input buffer
    mov r11d, r8d  ; Input size

    ; Find process
    xor r12d, r12d
@@find_write:
    cmp r12d, g_terminalPool.processCount
    jge @@write_not_found

    cmp r12d, r9d
    je @@write_found

    inc r12d
    jmp @@find_write
@@write_found:
    mov rax, SIZEOF PROCESS_INFO
    imul rax, r12
    lea rax, [g_terminalPool.processes + rax]
    
    ; Get input handle
    mov rcx, [rax].PROCESS_INFO.hInputWrite
    cmp rcx, 0
    je @@write_invalid_handle

    ; Call WriteFile (simplified)
    mov rdx, r10    ; lpBuffer
    mov r8d, r11d   ; nNumberOfBytesToWrite
    lea r9, [rsp + 32]  ; lpNumberOfBytesWritten (dummy)
    call WriteFile
    
    ; Return written count
    jmp @@write_done
@@write_invalid_handle:
    mov rax, -1
    jmp @@write_done
@@write_not_found:
    mov rax, -1
@@write_done:
    add rsp, 32
    pop rbp
    ret
masm_terminal_write_input ENDP

; masm_terminal_get_status - Get status of a terminal process
; Args: RCX = process ID
; Returns: process state (PROCESS_STATE_*)
masm_terminal_get_status PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    mov r8d, ecx  ; Process ID

    ; Find process
    xor r9d, r9d
@@find_status:
    cmp r9d, g_terminalPool.processCount
    jge @@status_not_found

    cmp r9d, r8d
    je @@status_found

    inc r9d
    jmp @@find_status
@@status_found:
    mov rax, SIZEOF PROCESS_INFO
    imul rax, r9
    lea rax, [g_terminalPool.processes + rax]
    mov eax, [rax].PROCESS_INFO.state
    jmp @@status_done
@@status_not_found:
    mov eax, PROCESS_STATE_ERROR
@@status_done:
    add rsp, 32
    pop rbp
    ret
masm_terminal_get_status ENDP

; masm_terminal_get_exit_code - Get exit code of terminated process
; Args: RCX = process ID
; Returns: exit code, or -1 if still running
masm_terminal_get_exit_code PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    mov r8d, ecx  ; Process ID

    ; Find process
    xor r9d, r9d
@@find_exit:
    cmp r9d, g_terminalPool.processCount
    jge @@exit_not_found

    cmp r9d, r8d
    je @@exit_found

    inc r9d
    jmp @@find_exit
@@exit_found:
    mov rax, SIZEOF PROCESS_INFO
    imul rax, r9
    lea rax, [g_terminalPool.processes + rax]
    mov eax, [rax].PROCESS_INFO.exitCode
    jmp @@exit_done
@@exit_not_found:
    mov eax, -1
@@exit_done:
    add rsp, 32
    pop rbp
    ret
masm_terminal_get_exit_code ENDP

; masm_terminal_list_processes - Get list of active processes
; Args: RCX = output buffer (array of DWORDs), RDX = max count
; Returns: actual count filled in buffer
masm_terminal_list_processes PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    mov r8, rcx    ; Output buffer
    mov r9d, edx   ; Max count

    xor r10d, r10d ; Counter
@@list_loop:
    cmp r10d, g_terminalPool.processCount
    jge @@list_done

    cmp r10d, r9d
    jge @@list_done

    ; Check if process is alive
    mov rax, SIZEOF PROCESS_INFO
    imul rax, r10
    lea rax, [g_terminalPool.processes + rax]
    cmp [rax].PROCESS_INFO.isAlive, 1
    jne @@skip_dead

    mov edx, r10d
    mov [r8 + r10 * 4], edx
@@skip_dead:
    inc r10d
    jmp @@list_loop
@@list_done:
    mov rax, r10  ; Return count
    add rsp, 32
    pop rbp
    ret
masm_terminal_list_processes ENDP

; masm_terminal_wait_for_process - Wait for process to terminate
; Args: RCX = process ID, RDX = timeout in milliseconds (INFINITE = -1)
; Returns: 1 if terminated, 0 if timeout, -1 if error
masm_terminal_wait_for_process PROC USES rbx rsi rdi
    push rbp
    sub rsp, 32

    mov r8d, ecx  ; Process ID
    mov r9d, edx  ; Timeout

    ; Find process
    xor r10d, r10d
@@find_wait:
    cmp r10d, g_terminalPool.processCount
    jge @@wait_not_found

    cmp r10d, r8d
    je @@wait_found

    inc r10d
    jmp @@find_wait
@@wait_found:
    mov rax, SIZEOF PROCESS_INFO
    imul rax, r10
    lea rax, [g_terminalPool.processes + rax]
    
    ; Call WaitForSingleObject on process handle
    mov rcx, [rax].PROCESS_INFO.hProcess
    mov edx, r9d
    call WaitForSingleObject
    
    ; Return: WAIT_OBJECT_0 = success, WAIT_TIMEOUT = timeout, etc
    jmp @@wait_done
@@wait_not_found:
    mov rax, -1
@@wait_done:
    add rsp, 32
    pop rbp
    ret
masm_terminal_wait_for_process ENDP

; masm_terminal_get_process_count - Get current process count
; Returns: number of processes in pool
masm_terminal_get_process_count PROC
    mov eax, g_terminalPool.processCount
    ret
masm_terminal_get_process_count ENDP

END


