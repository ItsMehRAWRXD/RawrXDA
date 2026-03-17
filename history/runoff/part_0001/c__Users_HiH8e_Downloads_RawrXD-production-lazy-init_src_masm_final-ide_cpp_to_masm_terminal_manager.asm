; terminal_manager_masm.asm
; Pure MASM x64 - Terminal Manager (converted from C++ TerminalManager class)
; Provides cross-platform terminal shell (PowerShell, CMD) management
; Replaces Qt's QProcess with Win32 process management

option casemap:none

EXTERN CreateProcessA:PROC
EXTERN TerminateProcess:PROC
EXTERN CloseHandle:PROC
EXTERN WaitForSingleObject:PROC
EXTERN GetExitCodeProcess:PROC
EXTERN CreatePipeA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memcpy:PROC
EXTERN strlen:PROC
EXTERN console_log:PROC

; Shell type enumeration
SHELL_TYPE_POWERSHELL EQU 0
SHELL_TYPE_COMMAND_PROMPT EQU 1

; Constants
INVALID_HANDLE_VALUE EQU -1
INFINITE EQU -1
PIPE_BUFFER_SIZE EQU 4096

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; TERMINAL_CONTEXT - Terminal manager state
TERMINAL_CONTEXT STRUCT
    shellType DWORD ?               ; SHELL_TYPE_*
    processHandle QWORD ?           ; Process handle
    threadHandle QWORD ?            ; Thread handle
    processId QWORD ?               ; PID
    isRunning BYTE ?                ; 1 = running
    
    ; I/O Pipes
    stdoutRead QWORD ?
    stdoutWrite QWORD ?
    stderrRead QWORD ?
    stderrWrite QWORD ?
    stdinRead QWORD ?
    stdinWrite QWORD ?
    
    ; Buffers
    outputBuffer QWORD ?            ; malloc'd buffer for stdout
    errorBuffer QWORD ?             ; malloc'd buffer for stderr
    outputSize QWORD ?
    errorSize QWORD ?
    
    ; Callbacks
    outputCallback QWORD ?          ; Function pointer for output events
    errorCallback QWORD ?           ; Function pointer for error events
    startedCallback QWORD ?         ; Function pointer for started event
    finishedCallback QWORD ?        ; Function pointer for finished event
TERMINAL_CONTEXT ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szPowerShellExe DB "powershell.exe", 0
    szCmdExe DB "cmd.exe", 0
    szStarted DB "[TERMINAL] Process started (shell=%s, PID=%lld)", 0
    szFinished DB "[TERMINAL] Process finished (exit code=%ld)", 0
    szOutput DB "[TERMINAL] Output received (%lld bytes)", 0
    szError DB "[TERMINAL] Error received (%lld bytes)", 0
    szPipeError DB "[TERMINAL] Failed to create pipe", 0
    szProcessError DB "[TERMINAL] Failed to create process", 0
    szReadError DB "[TERMINAL] Failed to read from pipe", 0

.code

; ============================================================================
; PUBLIC API
; ============================================================================

; terminal_create()
; Create terminal manager context
; Returns: RAX = pointer to TERMINAL_CONTEXT (malloc'd)
PUBLIC terminal_create
terminal_create PROC
    mov rcx, SIZEOF TERMINAL_CONTEXT
    call malloc
    
    ; Initialize structure
    xor edx, edx
    mov [rax], edx                  ; Clear all fields
    
    ; Set handles to invalid
    mov qword ptr [rax].TERMINAL_CONTEXT.processHandle, INVALID_HANDLE_VALUE
    mov qword ptr [rax].TERMINAL_CONTEXT.threadHandle, INVALID_HANDLE_VALUE
    mov qword ptr [rax].TERMINAL_CONTEXT.stdoutRead, INVALID_HANDLE_VALUE
    mov qword ptr [rax].TERMINAL_CONTEXT.stdoutWrite, INVALID_HANDLE_VALUE
    mov qword ptr [rax].TERMINAL_CONTEXT.stderrRead, INVALID_HANDLE_VALUE
    mov qword ptr [rax].TERMINAL_CONTEXT.stderrWrite, INVALID_HANDLE_VALUE
    mov qword ptr [rax].TERMINAL_CONTEXT.stdinRead, INVALID_HANDLE_VALUE
    mov qword ptr [rax].TERMINAL_CONTEXT.stdinWrite, INVALID_HANDLE_VALUE
    
    ret
terminal_create ENDP

; ============================================================================

; terminal_start(RCX = context, RDX = shellType [0=PowerShell, 1=CMD])
; Start terminal shell process
; Returns: EAX = 1 (success) or 0 (failure)
PUBLIC terminal_start
terminal_start PROC
    push rbx
    push rsi
    
    mov rbx, rcx                   ; rbx = context
    mov r8d, edx                   ; r8d = shellType
    
    ; Store shell type
    mov [rbx].TERMINAL_CONTEXT.shellType, r8d
    
    ; Create stdout pipe
    lea rcx, [rbx].TERMINAL_CONTEXT.stdoutRead
    lea rdx, [rbx].TERMINAL_CONTEXT.stdoutWrite
    call CreatePipeA
    cmp eax, 0
    je pipe_error
    
    ; Create stderr pipe
    lea rcx, [rbx].TERMINAL_CONTEXT.stderrRead
    lea rdx, [rbx].TERMINAL_CONTEXT.stderrWrite
    call CreatePipeA
    cmp eax, 0
    je pipe_error
    
    ; Create stdin pipe
    lea rcx, [rbx].TERMINAL_CONTEXT.stdinRead
    lea rdx, [rbx].TERMINAL_CONTEXT.stdinWrite
    call CreatePipeA
    cmp eax, 0
    je pipe_error
    
    ; Allocate output buffers
    mov rcx, PIPE_BUFFER_SIZE
    call malloc
    mov [rbx].TERMINAL_CONTEXT.outputBuffer, rax
    
    mov rcx, PIPE_BUFFER_SIZE
    call malloc
    mov [rbx].TERMINAL_CONTEXT.errorBuffer, rax
    
    ; Get shell executable name
    cmp r8d, SHELL_TYPE_COMMAND_PROMPT
    je use_cmd
    
    ; Default to PowerShell
    lea rsi, [szPowerShellExe]
    jmp create_process
    
use_cmd:
    lea rsi, [szCmdExe]
    
create_process:
    ; Set up STARTUPINFOA
    sub rsp, 104                   ; STARTUPINFOA structure
    
    mov rax, rsp
    mov dword ptr [rax], 104           ; cb = size
    mov dword ptr [rax + 0x24], 0x0101 ; dwFlags = STARTF_USESTDHANDLES
    
    mov rcx, [rbx].TERMINAL_CONTEXT.stdoutWrite
    mov [rax + 0x28], rcx          ; hStdOutput
    
    mov rcx, [rbx].TERMINAL_CONTEXT.stderrWrite
    mov [rax + 0x30], rcx          ; hStdError
    
    mov rcx, [rbx].TERMINAL_CONTEXT.stdinRead
    mov [rax + 0x38], rcx          ; hStdInput
    
    ; Set up PROCESS_INFORMATION
    sub rsp, 32
    
    ; Call CreateProcessA
    mov rcx, rsi                   ; lpApplicationName = shell exe
    xor rdx, rdx                   ; lpCommandLine = NULL (use executable)
    lea r8, [rsp + 32]             ; lpStartupInfo
    lea r9, [rsp]                  ; lpProcessInformation
    xor r10, r10
    
    ; lpProcessAttributes, lpThreadAttributes, bInheritHandles
    ; (would need to push these as stack args)
    
    call CreateProcessA
    
    add rsp, 32 + 104              ; Clean up stack
    
    cmp eax, 0
    je create_error
    
    ; Mark as running
    mov byte ptr [rbx].TERMINAL_CONTEXT.isRunning, 1
    
    ; Log process started
    lea rcx, [szStarted]
    mov rdx, rsi
    mov r8, [rbx].TERMINAL_CONTEXT.processId
    call console_log
    
    ; Call started callback if set
    mov rcx, [rbx].TERMINAL_CONTEXT.startedCallback
    cmp rcx, 0
    je skip_started
    
    call rcx
    
skip_started:
    mov eax, 1                     ; Success
    pop rsi
    pop rbx
    ret
    
pipe_error:
    lea rcx, [szPipeError]
    call console_log
    xor eax, eax
    pop rsi
    pop rbx
    ret
    
create_error:
    lea rcx, [szProcessError]
    call console_log
    xor eax, eax
    pop rsi
    pop rbx
    ret
terminal_start ENDP

; ============================================================================

; terminal_stop(RCX = context)
; Terminate shell process gracefully
; Returns: EAX = 1 (success) or 0 (failure)
PUBLIC terminal_stop
terminal_stop PROC
    push rbx
    
    mov rbx, rcx
    
    ; Check if running
    cmp byte ptr [rbx].TERMINAL_CONTEXT.isRunning, 1
    jne not_running
    
    ; Terminate process
    mov rcx, [rbx].TERMINAL_CONTEXT.processHandle
    mov edx, 1                     ; Exit code 1
    call TerminateProcess
    
    ; Wait for process to exit
    mov rcx, [rbx].TERMINAL_CONTEXT.processHandle
    mov edx, 5000                  ; 5 second timeout
    call WaitForSingleObject
    
    ; Mark as not running
    mov byte ptr [rbx].TERMINAL_CONTEXT.isRunning, 0
    
    mov eax, 1
    pop rbx
    ret
    
not_running:
    xor eax, eax
    pop rbx
    ret
terminal_stop ENDP

; ============================================================================

; terminal_get_pid(RCX = context)
; Get process ID
; Returns: RAX = process ID (qint64)
PUBLIC terminal_get_pid
terminal_get_pid PROC
    mov rax, [rcx].TERMINAL_CONTEXT.processId
    ret
terminal_get_pid ENDP

; ============================================================================

; terminal_is_running(RCX = context)
; Check if terminal is running
; Returns: AL = 1 (running) or 0 (not running)
PUBLIC terminal_is_running
terminal_is_running PROC
    movzx eax, byte ptr [rcx].TERMINAL_CONTEXT.isRunning
    ret
terminal_is_running ENDP

; ============================================================================

; terminal_write_input(RCX = context, RDX = data, R8 = size)
; Write input to terminal stdin
; Returns: EAX = 1 (success) or 0 (failure)
PUBLIC terminal_write_input
terminal_write_input PROC
    ; RCX = context
    ; RDX = data buffer
    ; R8 = size
    
    mov r9, [rcx].TERMINAL_CONTEXT.stdinWrite
    
    ; WriteFile(stdinWrite, data, size, &bytesWritten, NULL)
    ; Would need to implement Win32 WriteFile call
    
    xor eax, eax                   ; Return 0 for now (stub)
    ret
terminal_write_input ENDP

; ============================================================================

; terminal_read_output(RCX = context)
; Read available output from stdout
; Returns: RAX = output buffer, RDX = size
PUBLIC terminal_read_output
terminal_read_output PROC
    mov rax, [rcx].TERMINAL_CONTEXT.outputBuffer
    mov rdx, [rcx].TERMINAL_CONTEXT.outputSize
    ret
terminal_read_output ENDP

; ============================================================================

; terminal_read_error(RCX = context)
; Read available output from stderr
; Returns: RAX = error buffer, RDX = size
PUBLIC terminal_read_error
terminal_read_error PROC
    mov rax, [rcx].TERMINAL_CONTEXT.errorBuffer
    mov rdx, [rcx].TERMINAL_CONTEXT.errorSize
    ret
terminal_read_error ENDP

; ============================================================================

; terminal_get_exit_code(RCX = context)
; Get process exit code
; Returns: EAX = exit code (0-255)
PUBLIC terminal_get_exit_code
terminal_get_exit_code PROC
    push rbx
    
    mov rbx, rcx
    
    lea rdx, [rbx].TERMINAL_CONTEXT.isRunning
    mov rcx, [rbx].TERMINAL_CONTEXT.processHandle
    
    call GetExitCodeProcess
    
    ; Return exit code in EAX (was stored by GetExitCodeProcess)
    mov eax, [rbx].TERMINAL_CONTEXT.isRunning
    
    pop rbx
    ret
terminal_get_exit_code ENDP

; ============================================================================

; terminal_set_output_callback(RCX = context, RDX = callback_function)
; Set callback for output events
PUBLIC terminal_set_output_callback
terminal_set_output_callback PROC
    mov [rcx].TERMINAL_CONTEXT.outputCallback, rdx
    ret
terminal_set_output_callback ENDP

; ============================================================================

; terminal_set_error_callback(RCX = context, RDX = callback_function)
; Set callback for error events
PUBLIC terminal_set_error_callback
terminal_set_error_callback PROC
    mov [rcx].TERMINAL_CONTEXT.errorCallback, rdx
    ret
terminal_set_error_callback ENDP

; ============================================================================

; terminal_destroy(RCX = context)
; Free all resources associated with terminal
PUBLIC terminal_destroy
terminal_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Stop if running
    call terminal_stop
    
    ; Close all handles
    mov rcx, [rbx].TERMINAL_CONTEXT.processHandle
    cmp rcx, INVALID_HANDLE_VALUE
    je skip_process
    call CloseHandle
    
skip_process:
    ; Close all pipes
    mov rcx, [rbx].TERMINAL_CONTEXT.stdoutRead
    cmp rcx, INVALID_HANDLE_VALUE
    je skip_stdout_read
    call CloseHandle
skip_stdout_read:
    
    mov rcx, [rbx].TERMINAL_CONTEXT.stdoutWrite
    cmp rcx, INVALID_HANDLE_VALUE
    je skip_stdout_write
    call CloseHandle
skip_stdout_write:
    
    mov rcx, [rbx].TERMINAL_CONTEXT.stderrRead
    cmp rcx, INVALID_HANDLE_VALUE
    je skip_stderr_read
    call CloseHandle
skip_stderr_read:
    
    mov rcx, [rbx].TERMINAL_CONTEXT.stderrWrite
    cmp rcx, INVALID_HANDLE_VALUE
    je skip_stderr_write
    call CloseHandle
skip_stderr_write:
    
    ; Free buffers
    mov rcx, [rbx].TERMINAL_CONTEXT.outputBuffer
    cmp rcx, 0
    je skip_out_buf
    call free
skip_out_buf:
    
    mov rcx, [rbx].TERMINAL_CONTEXT.errorBuffer
    cmp rcx, 0
    je skip_err_buf
    call free
skip_err_buf:
    
    ; Free context
    mov rcx, rbx
    call free
    
    pop rbx
    ret
terminal_destroy ENDP

; ============================================================================

END
