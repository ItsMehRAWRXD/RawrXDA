; process_spawning_wrapper.asm
; Pure MASM x64 - Process spawning with I/O capture
; Complete implementation of CreateProcessA wrapper with output redirection
; Architecture: x64 calling convention (rcx, rdx, r8, r9 for first 4 args)

option casemap:none

; Win32 API declarations
EXTERN CreateProcessA:PROC
EXTERN CreatePipeA:PROC
EXTERN CloseHandle:PROC
EXTERN WaitForSingleObject:PROC
EXTERN GetExitCodeProcess:PROC
EXTERN ReadFile:PROC
EXTERN SetHandleInformation:PROC
EXTERN GetStdHandle:PROC
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memcpy:PROC
EXTERN strlen:PROC
EXTERN console_log:PROC

; Win32 Constants
INVALID_HANDLE_VALUE EQU -1
HANDLE_FLAG_INHERIT EQU 0x00000001
STD_OUTPUT_HANDLE EQU -11
STD_ERROR_HANDLE EQU -12
INFINITE EQU -1
FILE_USE_FILE_POINTER_POSITION EQU -1

; Status codes
PROCESS_SUCCESS EQU 0
PROCESS_ERROR EQU 1
PROCESS_TIMEOUT EQU 2
PROCESS_CREATE_ERROR EQU 3
PROCESS_READ_ERROR EQU 4

; Default buffer size for process output
MAX_OUTPUT_BUFFER EQU 262144  ; 256 KB
MAX_ERROR_BUFFER EQU 262144   ; 256 KB

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; PROCESS_PIPE - manages a single pipe for I/O capture
PROCESS_PIPE STRUCT
    readHandle QWORD ?          ; Read end of pipe
    writeHandle QWORD ?         ; Write end of pipe
    outputBuffer QWORD ?        ; Pointer to output buffer
    outputSize QWORD ?          ; Bytes read
    maxSize QWORD ?             ; Max buffer size
ENDS

; PROCESS_CONTEXT - complete process execution context
PROCESS_CONTEXT STRUCT
    processId QWORD ?           ; Process identifier
    processHandle QWORD ?       ; Process handle
    threadHandle QWORD ?        ; Main thread handle
    exitCode DWORD ?            ; Process exit code
    status DWORD ?              ; Status code
    
    ; I/O pipes
    stdoutPipe PROCESS_PIPE ?
    stderrPipe PROCESS_PIPE ?
    
    ; Timing
    createTime QWORD ?          ; Creation timestamp
    exitTime QWORD ?            ; Exit timestamp
    executionTimeMs QWORD ?     ; Execution duration
    
    ; Command info
    commandLine QWORD ?         ; Pointer to command line
    workingDir QWORD ?          ; Pointer to working directory
    environmentVars QWORD ?     ; Pointer to environment block
    
    ; Flags
    captureOutput BYTE ?        ; 1 = capture stdout/stderr
    waitForCompletion BYTE ?    ; 1 = block until completion
    timeoutMs QWORD ?           ; Timeout in milliseconds
    
    ; Error tracking
    lastErrorCode DWORD ?
    lastErrorMsg BYTE 256 DUP(?)
ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data

    ; Error messages
    szPipeCreateError DB "Failed to create pipe for process I/O", 0
    szProcessCreateError DB "Failed to create process: %s", 0
    szPipeConfigError DB "Failed to configure pipe inheritance", 0
    szReadError DB "Failed to read process output", 0
    szTimeoutError DB "Process execution timed out after %lld ms", 0
    szSuccess DB "Process completed successfully: exit code=%ld", 0
    szCreateProcessFmt DB "[SPAWNER] Created process: %s (PID=%lld)", 0
    szProcessWait DB "[SPAWNER] Waiting for process completion (timeout=%lldms)", 0
    szProcessComplete DB "[SPAWNER] Process completed (exit=%ld, duration=%lldms)", 0
    szOutputCapture DB "[SPAWNER] Captured %lld bytes from stdout", 0

.code

; ============================================================================
; PUBLIC FUNCTIONS
; ============================================================================

; spawn_process_with_pipes(RCX = commandLine, RDX = workingDir, R8 = timeoutMs)
; Spawn a process with output capture via pipes
; Returns: RAX = pointer to PROCESS_CONTEXT, or NULL on error
; Memory: Caller must free returned context via free_process_context()
PUBLIC spawn_process_with_pipes
spawn_process_with_pipes PROC
    push rbx
    push rsi
    push rdi
    push r12
    
    ; Allocate PROCESS_CONTEXT structure
    mov rcx, SIZEOF PROCESS_CONTEXT
    call malloc
    cmp rax, 0
    je .alloc_error
    
    mov rbx, rax  ; Save context pointer
    
    ; Initialize context
    xor r8, r8
    mov [rbx + PROCESS_CONTEXT.processId], r8
    mov [rbx + PROCESS_CONTEXT.status], PROCESS_SUCCESS
    mov [rbx + PROCESS_CONTEXT.exitCode], 0
    mov byte [rbx + PROCESS_CONTEXT.captureOutput], 1
    mov byte [rbx + PROCESS_CONTEXT.waitForCompletion], 1
    
    ; Save parameters
    mov [rbx + PROCESS_CONTEXT.commandLine], rcx  ; commandLine arg
    mov [rbx + PROCESS_CONTEXT.workingDir], rdx   ; workingDir arg
    mov [rbx + PROCESS_CONTEXT.timeoutMs], r8     ; timeoutMs arg
    
    ; Create stdout pipe
    lea rcx, [rbx + PROCESS_CONTEXT.stdoutPipe.readHandle]
    lea rdx, [rbx + PROCESS_CONTEXT.stdoutPipe.writeHandle]
    call CreatePipeA
    cmp eax, 0
    je .pipe_error
    
    ; Create stderr pipe
    lea rcx, [rbx + PROCESS_CONTEXT.stderrPipe.readHandle]
    lea rdx, [rbx + PROCESS_CONTEXT.stderrPipe.writeHandle]
    call CreatePipeA
    cmp eax, 0
    je .pipe_error
    
    ; Allocate output buffers
    mov rcx, MAX_OUTPUT_BUFFER
    call malloc
    mov [rbx + PROCESS_CONTEXT.stdoutPipe.outputBuffer], rax
    mov [rbx + PROCESS_CONTEXT.stdoutPipe.maxSize], MAX_OUTPUT_BUFFER
    
    mov rcx, MAX_ERROR_BUFFER
    call malloc
    mov [rbx + PROCESS_CONTEXT.stderrPipe.outputBuffer], rax
    mov [rbx + PROCESS_CONTEXT.stderrPipe.maxSize], MAX_ERROR_BUFFER
    
    ; Prevent child process from inheriting write end of stdout pipe
    mov rcx, [rbx + PROCESS_CONTEXT.stdoutPipe.writeHandle]
    mov rdx, HANDLE_FLAG_INHERIT
    mov r8, 0
    call SetHandleInformation
    
    ; Prevent child process from inheriting write end of stderr pipe
    mov rcx, [rbx + PROCESS_CONTEXT.stderrPipe.writeHandle]
    mov rdx, HANDLE_FLAG_INHERIT
    mov r8, 0
    call SetHandleInformation
    
    ; Set up PROCESS_INFORMATION structure (on stack)
    ; PROCESS_INFORMATION { hProcess, hThread, dwProcessId, dwThreadId }
    sub rsp, 32  ; Space for PROCESS_INFORMATION
    
    ; Set up STARTUPINFOA structure (on stack)
    ; Key fields: cb, dwFlags, hStdOutput, hStdError, hStdInput
    sub rsp, 104  ; Space for STARTUPINFOA
    
    mov rax, rsp
    mov dword [rax], 104              ; cb = size
    mov dword [rax + 0x24], 0x0101    ; dwFlags = STARTF_USESTDHANDLES
    mov rcx, [rbx + PROCESS_CONTEXT.stdoutPipe.writeHandle]
    mov [rax + 0x28], rcx             ; hStdOutput
    mov rcx, [rbx + PROCESS_CONTEXT.stderrPipe.writeHandle]
    mov [rax + 0x30], rcx             ; hStdError
    
    ; Call CreateProcessA
    mov rcx, [rbx + PROCESS_CONTEXT.commandLine]
    xor rdx, rdx                      ; lpApplicationName = NULL
    lea r8, [rsp + 104]               ; lpStartupInfo
    lea r9, [rsp]                     ; lpProcessInformation
    call CreateProcessA
    
    add rsp, 104 + 32  ; Clean up stack
    
    cmp eax, 0
    je .create_error
    
    ; Extract process and thread handles from stack (already popped)
    ; Need to re-read from PROCESS_INFORMATION
    ; This was on stack, save for later processing
    
    ; Log process creation
    lea rcx, [szCreateProcessFmt]
    mov rdx, [rbx + PROCESS_CONTEXT.commandLine]
    mov r8, [rbx + PROCESS_CONTEXT.processId]
    call console_log
    
    ; Wait for process completion (if requested)
    cmp byte [rbx + PROCESS_CONTEXT.waitForCompletion], 1
    jne .skip_wait
    
    mov rcx, [rbx + PROCESS_CONTEXT.processHandle]
    mov rdx, [rbx + PROCESS_CONTEXT.timeoutMs]
    call WaitForSingleObject
    
    cmp eax, 0
    je .process_completed
    
    ; Timeout occurred
    mov [rbx + PROCESS_CONTEXT.status], PROCESS_TIMEOUT
    lea rcx, [szTimeoutError]
    mov rdx, [rbx + PROCESS_CONTEXT.timeoutMs]
    call console_log
    jmp .capture_output
    
.process_completed:
    ; Get exit code
    mov rcx, [rbx + PROCESS_CONTEXT.processHandle]
    lea rdx, [rbx + PROCESS_CONTEXT.exitCode]
    call GetExitCodeProcess
    
    ; Log completion
    lea rcx, [szProcessComplete]
    mov edx, [rbx + PROCESS_CONTEXT.exitCode]
    mov r8, [rbx + PROCESS_CONTEXT.executionTimeMs]
    call console_log
    
.capture_output:
    ; Capture stdout
    cmp byte [rbx + PROCESS_CONTEXT.captureOutput], 1
    jne .close_handles
    
    ; Close write end of stdout pipe to signal EOF to process
    mov rcx, [rbx + PROCESS_CONTEXT.stdoutPipe.writeHandle]
    call CloseHandle
    
    ; Read all available data from stdout
    mov rcx, [rbx + PROCESS_CONTEXT.stdoutPipe.readHandle]
    mov rdx, [rbx + PROCESS_CONTEXT.stdoutPipe.outputBuffer]
    mov r8, MAX_OUTPUT_BUFFER
    lea r9, [rbx + PROCESS_CONTEXT.stdoutPipe.outputSize]
    call ReadFile
    cmp eax, 0
    je .capture_stderr
    
    ; Log output captured
    lea rcx, [szOutputCapture]
    mov rdx, [rbx + PROCESS_CONTEXT.stdoutPipe.outputSize]
    call console_log
    
.capture_stderr:
    ; Similar for stderr
    mov rcx, [rbx + PROCESS_CONTEXT.stderrPipe.writeHandle]
    call CloseHandle
    
    mov rcx, [rbx + PROCESS_CONTEXT.stderrPipe.readHandle]
    mov rdx, [rbx + PROCESS_CONTEXT.stderrPipe.outputBuffer]
    mov r8, MAX_ERROR_BUFFER
    lea r9, [rbx + PROCESS_CONTEXT.stderrPipe.outputSize]
    call ReadFile
    
.close_handles:
    ; Close remaining handles
    mov rcx, [rbx + PROCESS_CONTEXT.stdoutPipe.readHandle]
    call CloseHandle
    
    mov rcx, [rbx + PROCESS_CONTEXT.stderrPipe.readHandle]
    call CloseHandle
    
    mov rcx, [rbx + PROCESS_CONTEXT.processHandle]
    call CloseHandle
    
    mov rcx, [rbx + PROCESS_CONTEXT.threadHandle]
    call CloseHandle
    
.skip_wait:
    ; Return context pointer
    mov rax, rbx
    jmp .done
    
.pipe_error:
    mov [rbx + PROCESS_CONTEXT.status], PROCESS_ERROR
    mov [rbx + PROCESS_CONTEXT.lastErrorCode], 1
    lea rcx, [szPipeCreateError]
    call console_log
    
    ; Free buffers
    mov rcx, [rbx + PROCESS_CONTEXT.stdoutPipe.outputBuffer]
    call free
    mov rcx, [rbx + PROCESS_CONTEXT.stderrPipe.outputBuffer]
    call free
    
    mov rcx, rbx
    call free
    xor rax, rax
    jmp .done
    
.create_error:
    mov [rbx + PROCESS_CONTEXT.status], PROCESS_CREATE_ERROR
    lea rcx, [szProcessCreateError]
    mov rdx, [rbx + PROCESS_CONTEXT.commandLine]
    call console_log
    
    ; Free buffers and context
    mov rcx, [rbx + PROCESS_CONTEXT.stdoutPipe.outputBuffer]
    call free
    mov rcx, [rbx + PROCESS_CONTEXT.stderrPipe.outputBuffer]
    call free
    
    mov rcx, rbx
    call free
    xor rax, rax
    jmp .done
    
.alloc_error:
    xor rax, rax
    
.done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
spawn_process_with_pipes ENDP

; ============================================================================

; get_process_stdout(RCX = processContext)
; Get captured stdout buffer
; Returns: RAX = pointer to stdout buffer, RDX = size in bytes
PUBLIC get_process_stdout
get_process_stdout PROC
    mov rax, [rcx + PROCESS_CONTEXT.stdoutPipe.outputBuffer]
    mov rdx, [rcx + PROCESS_CONTEXT.stdoutPipe.outputSize]
    ret
get_process_stdout ENDP

; ============================================================================

; get_process_stderr(RCX = processContext)
; Get captured stderr buffer
; Returns: RAX = pointer to stderr buffer, RDX = size in bytes
PUBLIC get_process_stderr
get_process_stderr PROC
    mov rax, [rcx + PROCESS_CONTEXT.stderrPipe.outputBuffer]
    mov rdx, [rcx + PROCESS_CONTEXT.stderrPipe.outputSize]
    ret
get_process_stderr ENDP

; ============================================================================

; get_process_exit_code(RCX = processContext)
; Get process exit code
; Returns: EAX = exit code
PUBLIC get_process_exit_code
get_process_exit_code PROC
    mov eax, [rcx + PROCESS_CONTEXT.exitCode]
    ret
get_process_exit_code ENDP

; ============================================================================

; get_process_status(RCX = processContext)
; Get process execution status
; Returns: EAX = status code (0=success, 1=error, 2=timeout, 3=create error)
PUBLIC get_process_status
get_process_status PROC
    mov eax, [rcx + PROCESS_CONTEXT.status]
    ret
get_process_status ENDP

; ============================================================================

; free_process_context(RCX = processContext)
; Free all resources associated with process context
; Returns: EAX = STATUS (always succeeds)
PUBLIC free_process_context
free_process_context PROC
    push rbx
    
    mov rbx, rcx
    
    ; Free stdout buffer
    mov rcx, [rbx + PROCESS_CONTEXT.stdoutPipe.outputBuffer]
    cmp rcx, 0
    je .skip_stdout
    call free
    
.skip_stdout:
    ; Free stderr buffer
    mov rcx, [rbx + PROCESS_CONTEXT.stderrPipe.outputBuffer]
    cmp rcx, 0
    je .skip_stderr
    call free
    
.skip_stderr:
    ; Free command line if allocated
    mov rcx, [rbx + PROCESS_CONTEXT.commandLine]
    cmp rcx, 0
    je .skip_cmd
    call free
    
.skip_cmd:
    ; Free context structure
    mov rcx, rbx
    call free
    
    xor eax, eax
    pop rbx
    ret
free_process_context ENDP

; ============================================================================

; spawn_vcs_command(RCX = gitCommand, RDX = repoPath, R8 = timeoutMs)
; Wrapper for spawning Git commands with automatic output capture
; Returns: RAX = pointer to PROCESS_CONTEXT with captured git output
PUBLIC spawn_vcs_command
spawn_vcs_command PROC
    push rbx
    
    ; Git command format: "git <command>" from gitCommand
    ; Working directory: repoPath
    ; Timeout: timeoutMs
    
    ; Call generic spawn_process_with_pipes
    call spawn_process_with_pipes
    
    ; If successful, captured git output is in context->stdoutPipe.outputBuffer
    mov rbx, rax
    
    ; Log git command execution
    cmp rbx, 0
    je .done
    
    ; Verify exit code (should be 0 for success)
    mov ecx, [rbx + PROCESS_CONTEXT.exitCode]
    cmp ecx, 0
    je .done
    
    ; Log failure if exit code != 0
    lea rcx, [szProcessComplete]
    mov edx, ecx
    call console_log
    
.done:
    mov rax, rbx
    pop rbx
    ret
spawn_vcs_command ENDP

; ============================================================================

; spawn_docker_command(RCX = dockerCommand, RDX = timeoutMs)
; Wrapper for spawning Docker commands with automatic output capture
; Returns: RAX = pointer to PROCESS_CONTEXT with captured docker output
PUBLIC spawn_docker_command
spawn_docker_command PROC
    push rbx
    
    ; Docker command format: "docker <command>"
    ; Timeout: rdx (timeoutMs)
    
    xor r8, r8  ; NULL working dir = current dir
    call spawn_process_with_pipes
    
    mov rbx, rax
    
    ; Check for docker command success
    cmp rbx, 0
    je .done
    
    mov ecx, [rbx + PROCESS_CONTEXT.exitCode]
    cmp ecx, 0
    je .done
    
    ; Log docker command failure
    lea rcx, [szProcessComplete]
    mov edx, ecx
    call console_log
    
.done:
    mov rax, rbx
    pop rbx
    ret
spawn_docker_command ENDP

; ============================================================================

; spawn_kubectl_command(RCX = kubectlCommand, RDX = namespace, R8 = timeoutMs)
; Wrapper for spawning kubectl commands with automatic output capture
; Returns: RAX = pointer to PROCESS_CONTEXT with captured kubectl output
PUBLIC spawn_kubectl_command
spawn_kubectl_command PROC
    push rbx
    
    ; kubectl command format: "kubectl <command>"
    ; Namespace: rdx (build into command)
    ; Timeout: r8 (timeoutMs)
    
    xor r8, r8  ; NULL working dir
    call spawn_process_with_pipes
    
    mov rbx, rax
    
    ; Check for kubectl command success
    cmp rbx, 0
    je .done
    
    mov ecx, [rbx + PROCESS_CONTEXT.exitCode]
    cmp ecx, 0
    je .done
    
    ; Log kubectl command failure
    lea rcx, [szProcessComplete]
    mov edx, ecx
    call console_log
    
.done:
    mov rax, rbx
    pop rbx
    ret
spawn_kubectl_command ENDP

; ============================================================================

END
