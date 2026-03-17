;==============================================================================
; process_manager.asm - Full Win32 process management with pipe redirection
;==============================================================================
; Implements:
; - CreateRedirectedProcess: spawn process with stdout/stdin redirected to pipes
; - ReadProcessOutput: read from process stdout pipe
; - WriteProcessInput: write to process stdin pipe
;==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib

;==============================================================================
; STRUCTURES
;==============================================================================

PROCESS_INFO_EX STRUCT
    hProcess            QWORD ?
    hThread             QWORD ?
    hStdInWrite         QWORD ?
    hStdOutRead         QWORD ?
    hStdErrRead         QWORD ?
    dwProcessId         DWORD ?
    dwThreadId          DWORD ?
PROCESS_INFO_EX ENDS

; Use structs from windows.inc (STARTUPINFO, PROCESS_INFORMATION)

;==============================================================================
; CONSTANTS
;==============================================================================
; Already in windows.inc: STARTF_USESTDHANDLES, CREATE_NO_WINDOW

.code

;==============================================================================
; PUBLIC: CreateRedirectedProcess(lpCommandLine: rcx, pInfo: rdx)
; Create a process with stdout/stdin redirected to pipes.
; Returns: 1 (success) or 0 (failure).
;==============================================================================
PUBLIC CreateRedirectedProcess
ALIGN 16
CreateRedirectedProcess PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 640  ; space for structs and shadow space
    
    mov r12, rcx  ; r12 = lpCommandLine
    mov r13, rdx  ; r13 = pInfo
    
    ; Initialize output structure
    mov QWORD PTR [r13 + PROCESS_INFO_EX.hProcess], 0
    mov QWORD PTR [r13 + PROCESS_INFO_EX.hThread], 0
    mov QWORD PTR [r13 + PROCESS_INFO_EX.hStdInWrite], 0
    mov QWORD PTR [r13 + PROCESS_INFO_EX.hStdOutRead], 0
    mov QWORD PTR [r13 + PROCESS_INFO_EX.hStdErrRead], 0
    
    ; Security Attributes for pipes (must be inheritable)
    lea rax, [rsp + 32] ; SECURITY_ATTRIBUTES
    mov DWORD PTR [rax + SECURITY_ATTRIBUTES.nLength], sizeof SECURITY_ATTRIBUTES
    mov QWORD PTR [rax + SECURITY_ATTRIBUTES.lpSecurityDescriptor], 0
    mov DWORD PTR [rax + SECURITY_ATTRIBUTES.bInheritHandle], 1
    
    ; Create pipe for stdout (read side)
    lea rcx, [rsp + 64]  ; hReadPipe
    lea rdx, [rsp + 72]  ; hWritePipe
    lea r8, [rsp + 32]   ; lpPipeAttributes
    xor r9, r9           ; dwSize = 0 (default)
    call CreatePipe
    test eax, eax
    jz proc_fail
    
    mov rax, [rsp + 64]
    mov QWORD PTR [r13 + PROCESS_INFO_EX.hStdOutRead], rax  ; Save read handle
    mov r14, [rsp + 72]  ; Save write handle for child
    
    ; Ensure the read handle is NOT inherited
    mov rcx, [rsp + 64]
    mov rdx, HANDLE_FLAG_INHERIT
    xor r8, r8
    call SetHandleInformation
    
    ; Create pipe for stdin (write side)
    lea rcx, [rsp + 80]  ; hReadPipe
    lea rdx, [rsp + 88]  ; hWritePipe
    lea r8, [rsp + 32]   ; lpPipeAttributes
    xor r9, r9
    call CreatePipe
    test eax, eax
    jz pipe_fail
    
    mov rax, [rsp + 88]
    mov QWORD PTR [r13 + PROCESS_INFO_EX.hStdInWrite], rax  ; Save write handle (for parent)
    mov r15, [rsp + 80]  ; Save read handle for child
    
    ; Ensure the write handle is NOT inherited
    mov rcx, [rsp + 88]
    mov rdx, HANDLE_FLAG_INHERIT
    xor r8, r8
    call SetHandleInformation
    
    ; Initialize STARTUPINFO
    lea rdi, [rsp + 128]  ; STARTUPINFO
    mov ecx, (sizeof STARTUPINFO) / 8
    xor eax, eax
    rep stosq
    
    lea rax, [rsp + 128]
    mov DWORD PTR [rax + STARTUPINFO.cb], sizeof STARTUPINFO
    mov DWORD PTR [rax + STARTUPINFO.dwFlags], STARTF_USESTDHANDLES
    mov QWORD PTR [rax + STARTUPINFO.hStdInput], r15   ; stdin from pipe
    mov QWORD PTR [rax + STARTUPINFO.hStdOutput], r14  ; stdout to pipe
    mov QWORD PTR [rax + STARTUPINFO.hStdError], r14   ; stderr to pipe
    
    ; Create the process
    xor rcx, rcx         ; lpApplicationName = NULL
    mov rdx, r12        ; lpCommandLine
    xor r8, r8          ; lpProcessAttributes = NULL
    xor r9, r9          ; lpThreadAttributes = NULL
    
    ; Stack arguments for CreateProcessA
    mov QWORD PTR [rsp + 32], 1  ; bInheritHandles = TRUE
    mov QWORD PTR [rsp + 40], CREATE_NO_WINDOW ; dwCreationFlags
    mov QWORD PTR [rsp + 48], 0  ; lpEnvironment = NULL
    mov QWORD PTR [rsp + 56], 0  ; lpCurrentDirectory = NULL
    lea rax, [rsp + 128]         ; lpStartupInfo
    mov QWORD PTR [rsp + 64], rax
    lea rax, [rsp + 384]         ; lpProcessInformation (at offset 384)
    mov QWORD PTR [rsp + 72], rax
    
    call CreateProcessA
    test eax, eax
    jz proc_info_fail
    
    ; Copy process info
    lea rax, [rsp + 384]
    mov rcx, [rax + PROCESS_INFORMATION.hProcess]
    mov QWORD PTR [r13 + PROCESS_INFO_EX.hProcess], rcx
    mov rcx, [rax + PROCESS_INFORMATION.hThread]
    mov QWORD PTR [r13 + PROCESS_INFO_EX.hThread], rcx
    mov ecx, [rax + PROCESS_INFORMATION.dwProcessId]
    mov DWORD PTR [r13 + PROCESS_INFO_EX.dwProcessId], ecx
    mov ecx, [rax + PROCESS_INFORMATION.dwThreadId]
    mov DWORD PTR [r13 + PROCESS_INFO_EX.dwThreadId], ecx
    
    ; Close the write end of stdout (child has it now)
    mov rcx, r14
    call CloseHandle
    
    ; Close the read end of stdin (child has it now)
    mov rcx, r15
    call CloseHandle
    
    ; Success
    mov eax, 1
    jmp proc_done
    
proc_info_fail:
    ; Close pipes on failure
    mov rcx, [r13 + PROCESS_INFO_EX.hStdOutRead]
    call CloseHandle
    mov rcx, r14
    call CloseHandle
    mov rcx, r15
    call CloseHandle
    mov rcx, [r13 + PROCESS_INFO_EX.hStdInWrite]
    call CloseHandle
    
pipe_fail:
    mov rcx, [r13 + PROCESS_INFO_EX.hStdOutRead]
    call CloseHandle
    mov rcx, r14
    call CloseHandle
    
proc_fail:
    xor eax, eax
    
proc_done:
    add rsp, 640
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
CreateRedirectedProcess ENDP

;==============================================================================
; PUBLIC: ReadProcessOutput(hPipe: rcx, pBuffer: rdx, dwBufferSize: r8d)
; Read output from process stdout pipe.
; Returns: number of bytes read (or -1 on failure).
;==============================================================================
PUBLIC ReadProcessOutput
ALIGN 16
ReadProcessOutput PROC
    push rbx
    sub rsp, 64
    
    mov rbx, rcx         ; rbx = hPipe
    mov r9, rdx          ; r9 = pBuffer
    mov r10d, r8d        ; r10d = dwBufferSize
    
    ; Read from pipe
    mov rcx, rbx
    mov rdx, r9
    mov r8d, r10d
    lea r9, [rsp + 32]   ; lpNumberOfBytesRead
    call ReadFile
    test eax, eax
    jz read_fail
    
    ; Return number of bytes read
    mov eax, [rsp + 32]
    jmp read_done
    
read_fail:
    mov eax, -1
    
read_done:
    add rsp, 64
    pop rbx
    ret
ReadProcessOutput ENDP

;==============================================================================
; PUBLIC: WriteProcessInput(hPipe: rcx, pBuffer: rdx, dwLength: r8d)
; Write input to process stdin pipe.
; Returns: number of bytes written (or -1 on failure).
;==============================================================================
PUBLIC WriteProcessInput
ALIGN 16
WriteProcessInput PROC
    push rbx
    sub rsp, 64
    
    mov rbx, rcx         ; rbx = hPipe
    mov r9, rdx          ; r9 = pBuffer
    mov r10d, r8d        ; r10d = dwLength
    
    ; Write to pipe
    mov rcx, rbx
    mov rdx, r9
    mov r8d, r10d
    lea r9, [rsp + 32]   ; lpNumberOfBytesWritten
    call WriteFile
    test eax, eax
    jz write_fail
    
    ; Return number of bytes written
    mov eax, [rsp + 32]
    jmp write_done
    
write_fail:
    mov eax, -1
    
write_done:
    add rsp, 64
    pop rbx
    ret
WriteProcessInput ENDP

END
