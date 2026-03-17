; ===============================================================================
; RawrXD_TextEditor_MLInference.asm
; Wire to ML inference: Ctrl+Space triggers CLI with current line
; ===============================================================================

OPTION CASEMAP:NONE

EXTERN CreateProcessA:PROC
EXTERN CreatePipeA:PROC
EXTERN SetHandleInformation:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN WaitForSingleObject:PROC
EXTERN GetExitCodeProcess:PROC
EXTERN OutputDebugStringA:PROC
EXTERN lstrlenA:PROC

.data
    ALIGN 16
    szCLIPath           db "D:\rawrxd\build\amphibious-complete\RawrXD_Amphibious_CLI.exe", 0
    szMLReady           db "[ML] Inference ready - Press Ctrl+Space to suggest", 0
    szMLInvoking        db "[ML] Invoking inference on: %s", 0
    szMLResult          db "[ML] Inference result: %s", 0
    szMLTimeout         db "[ML] Inference timeout (>5 seconds)", 0
    szMLError           db "[ML] Inference failed - CLI not found", 0

    g_hMLProcess        dq 0        ; Child process handle
    g_hMLStdoutRead     dq 0        ; Stdout read pipe
    g_hMLStdoutWrite    dq 0        ; Stdout write pipe
    g_hMLStdinRead      dq 0        ; Stdin read pipe
    g_hMLStdinWrite     dq 0        ; Stdin write pipe
    g_MLOutputBuffer    dq 0        ; Output from ML
    g_MLOutputSize      dq 0        ; Size of output

.code

; ===============================================================================
; MLInference_Initialize - Set up pipes and prepare for inference
; Returns: rax = 1 if success, 0 if failed
; ===============================================================================
MLInference_Initialize PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 80
    .allocstack 80
    .endprolog

    ; Create stdout pipe
    lea rcx, [rsp + 0x20]           ; hReadPipe
    lea rdx, [rsp + 0x28]           ; hWritePipe
    call CreatePipeA
    
    test eax, eax
    jz MLInitFail
    
    mov rax, [rsp + 0x20]
    mov g_hMLStdoutRead, rax
    mov rax, [rsp + 0x28]
    mov g_hMLStdoutWrite, rax
    
    ; Make write handle inheritable
    mov rcx, [rsp + 0x28]           ; hWritePipe
    mov rdx, 1                      ; HANDLE_FLAG_INHERIT
    mov r8, 1                       ; TRUE
    call SetHandleInformation
    
    ; Create stdin pipe
    lea rcx, [rsp + 0x30]           ; hReadPipe
    lea rdx, [rsp + 0x38]           ; hWritePipe
    call CreatePipeA
    
    test eax, eax
    jz MLInitFail
    
    mov rax, [rsp + 0x30]
    mov g_hMLStdinRead, rax
    mov rax, [rsp + 0x38]
    mov g_hMLStdinWrite, rax
    
    ; Make read handle inheritable
    mov rcx, [rsp + 0x30]           ; hReadPipe
    mov rdx, 1
    mov r8, 1
    call SetHandleInformation
    
    lea rcx, szMLReady
    call OutputDebugStringA
    mov eax, 1
    jmp MLInitDone
    
MLInitFail:
    lea rcx, szMLError
    call OutputDebugStringA
    xor eax, eax
    
MLInitDone:
    add rsp, 80
    pop rbx
    pop rbp
    ret
MLInference_Initialize ENDP

; ===============================================================================
; MLInference_Invoke - Execute CLI with current line as input
; rcx = current line text (null-terminated)
; Returns: rax = output buffer (or 0 if error)
; ===============================================================================
MLInference_Invoke PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 0xA0
    .allocstack 0xA0
    .endprolog

    mov rbx, rcx                    ; rbx = input line
    
    ; Debug: log input
    lea rcx, szMLInvoking
    mov rdx, rbx
    call OutputDebugStringA
    
    ; Allocate output buffer (4KB)
    lea rax, [rsp + 0x40]           ; Use stack as temporary buffer
    mov g_MLOutputBuffer, rax
    
    ; Create child process
    lea rcx, szCLIPath              ; lpApplicationName
    xor rdx, rdx                    ; lpCommandLine = NULL
    xor r8, r8                      ; lpProcessAttributes = NULL
    xor r9, r9                      ; lpThreadAttributes = NULL
    mov qword ptr [rsp + 0x20], 1   ; bInheritHandles = TRUE
    mov qword ptr [rsp + 0x28], 0x08000000 ; dwCreationFlags = CREATE_NO_WINDOW
    mov qword ptr [rsp + 0x30], 0   ; lpEnvironment = NULL
    mov qword ptr [rsp + 0x38], 0   ; lpCurrentDirectory = NULL
    
    lea rax, [rsp + 0x50]           ; STARTUPINFO structure
    mov qword ptr [rax], 0x68       ; cb = 104 (size of STARTUPINFO)
    mov qword ptr [rax + 0x38], 6   ; dwFlags = STARTF_USESTDHANDLES
    mov rdx, [g_hMLStdinRead]
    mov qword ptr [rax + 0x40], rdx ; hStdInput = pipe read
    mov rdx, [g_hMLStdoutWrite]
    mov qword ptr [rax + 0x48], rdx ; hStdOutput = pipe write
    mov qword ptr [rax + 0x50], rdx ; hStdError = pipe write
    
    lea r10, [rsp + 0xA0 - 0x40]    ; PROCESS_INFORMATION
    mov qword ptr [rsp + 0x20], r10
    lea rax, [rsp + 0x50]
    mov qword ptr [rsp + 0x28], rax ; lpStartupInfo
    
    call CreateProcessA
    
    test eax, eax
    jz MLInvokeFail
    
    ; Store process handle
    mov rax, [rsp + 0xA0 - 0x40]
    mov g_hMLProcess, rax
    
    ; Wait for process (5 second timeout = 5000 ms)
    mov rcx, rax
    mov rdx, 5000                   ; 5 second timeout
    call WaitForSingleObject
    
    cmp eax, 0                      ; WAIT_OBJECT_0 = success
    jne MLInvokeTimeout
    
    ; Read output from stdout
    mov rcx, [g_hMLStdoutRead]
    mov rdx, [g_MLOutputBuffer]
    mov r8, 0x1000                 ; 4KB max read
    lea r9, [rsp + 0x30]           ; Bytes read
    mov qword ptr [rsp + 0x30], 0
    
    call ReadFile
    
    test eax, eax
    jz MLInvokeReadFail
    
    ; Null-terminate output
    mov rax, [rsp + 0x30]           ; Bytes read
    add rax, [g_MLOutputBuffer]
    mov byte ptr [rax], 0
    
    ; Log result
    mov rcx, [g_MLOutputBuffer]
    lea rdx, szMLResult
    call OutputDebugStringA
    
    mov rax, [g_MLOutputBuffer]
    jmp MLInvokeDone
    
MLInvokeTimeout:
    lea rcx, szMLTimeout
    call OutputDebugStringA
    xor eax, eax
    jmp MLInvokeDone
    
MLInvokeReadFail:
MLInvokeFail:
    lea rcx, szMLError
    call OutputDebugStringA
    xor eax, eax
    
MLInvokeDone:
    ; Close process handle
    cmp qword ptr [g_hMLProcess], 0
    je MLInvokeDoneSkip
    
    mov rcx, [g_hMLProcess]
    call CloseHandle
    mov qword ptr [g_hMLProcess], 0
    
MLInvokeDoneSkip:
    add rsp, 0xA0
    pop r12
    pop rbx
    pop rbp
    ret
MLInference_Invoke ENDP

; ===============================================================================
; MLInference_Cleanup - Close all pipes
; ===============================================================================
MLInference_Cleanup PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog

    cmp qword ptr [g_hMLStdoutRead], 0
    je SkipStdoutRead
    mov rcx, [g_hMLStdoutRead]
    call CloseHandle
    mov qword ptr [g_hMLStdoutRead], 0
SkipStdoutRead:

    cmp qword ptr [g_hMLStdoutWrite], 0
    je SkipStdoutWrite
    mov rcx, [g_hMLStdoutWrite]
    call CloseHandle
    mov qword ptr [g_hMLStdoutWrite], 0
SkipStdoutWrite:

    cmp qword ptr [g_hMLStdinRead], 0
    je SkipStdinRead
    mov rcx, [g_hMLStdinRead]
    call CloseHandle
    mov qword ptr [g_hMLStdinRead], 0
SkipStdinRead:

    cmp qword ptr [g_hMLStdinWrite], 0
    je SkipStdinWrite
    mov rcx, [g_hMLStdinWrite]
    call CloseHandle
    mov qword ptr [g_hMLStdinWrite], 0
SkipStdinWrite:

    add rsp, 32
    pop rbp
    ret
MLInference_Cleanup ENDP

PUBLIC MLInference_Initialize
PUBLIC MLInference_Invoke
PUBLIC MLInference_Cleanup
PUBLIC g_hMLProcess
PUBLIC g_hMLStdoutRead
PUBLIC g_hMLStdoutWrite
PUBLIC g_MLOutputBuffer
PUBLIC g_MLOutputSize

END
