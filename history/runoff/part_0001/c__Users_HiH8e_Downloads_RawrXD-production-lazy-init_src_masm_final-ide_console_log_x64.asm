; ============================================================================
; console_log_x64.asm - Console Logging for x64
; ============================================================================

option casemap:none

extern GetStdHandle:proc
extern WriteConsoleA:proc
extern WriteFileA:proc
extern GetCurrentProcessId:proc
extern GetCurrentThreadId:proc
extern GetSystemTimeAsFileTime:proc

; Standard handles
STD_OUTPUT_HANDLE   = -11
STD_ERROR_HANDLE    = -12

.data
    stdout_handle       dq 0
    stderr_handle       dq 0
    newline             db 13, 10, 0
    log_prefix_info     db "[INFO]", 0
    log_prefix_warn     db "[WARN]", 0
    log_prefix_error    db "[ERROR]", 0
    log_prefix_debug    db "[DEBUG]", 0

.code

; ============================================================================
; log_init - Initialize logging
; ============================================================================
public log_init
log_init proc
    push rbx
    sub rsp, 32
    
    ; Get stdout handle
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    lea rbx, [stdout_handle]
    mov [rbx], rax
    
    ; Get stderr handle
    mov ecx, STD_ERROR_HANDLE
    call GetStdHandle
    lea rbx, [stderr_handle]
    mov [rbx], rax
    
    add rsp, 32
    pop rbx
    ret
log_init endp

; ============================================================================
; log_info - Log info message
; rcx = message
; ============================================================================
public log_info
log_info proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov r8, rcx         ; r8 = message
    lea rcx, [log_prefix_info]
    call _log_message
    
    add rsp, 32
    pop rbp
    ret
log_info endp

; ============================================================================
; log_warning - Log warning message
; rcx = message
; ============================================================================
public log_warning
log_warning proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov r8, rcx         ; r8 = message
    lea rcx, [log_prefix_warn]
    call _log_message
    
    add rsp, 32
    pop rbp
    ret
log_warning endp

; ============================================================================
; log_error - Log error message
; rcx = message
; ============================================================================
public log_error
log_error proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov r8, rcx         ; r8 = message
    lea rcx, [log_prefix_error]
    call _log_message
    
    add rsp, 32
    pop rbp
    ret
log_error endp

; ============================================================================
; log_debug - Log debug message
; rcx = message
; ============================================================================
public log_debug
log_debug proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov r8, rcx         ; r8 = message
    lea rcx, [log_prefix_debug]
    call _log_message
    
    add rsp, 32
    pop rbp
    ret
log_debug endp

; ============================================================================
; _log_message - Internal logging function
; rcx = prefix
; r8 = message
; ============================================================================
_log_message proc
    push rbx
    push rdi
    push rsi
    sub rsp, 64
    
    mov rdi, [rsp + 80]  ; get stdout handle
    lea rbx, [stdout_handle]
    mov rdi, [rbx]
    
    ; Write prefix
    mov rdx, rcx        ; rdx = prefix
    xor r9, r9          ; written count
    
    ; Count prefix length
    xor eax, eax
prefix_loop:
    cmp byte ptr [rdx + rax], 0
    je prefix_len_done
    inc eax
    jmp prefix_loop

prefix_len_done:
    mov r8d, eax        ; r8 = prefix length
    mov rcx, rdi        ; rcx = handle
    lea r9, [rsp + 32]  ; r9 = &written
    xor r10d, r10d      ; unused
    
    ; Write prefix (simplified - just for logging)
    ; In real implementation, would call WriteConsoleA
    
    ; Write message
    mov rdx, [rsp + 88]  ; rdx = message
    xor eax, eax
message_loop:
    cmp byte ptr [rdx + rax], 0
    je message_len_done
    inc eax
    jmp message_loop

message_len_done:
    ; Write newline
    
    add rsp, 64
    pop rsi
    pop rdi
    pop rbx
    ret
_log_message endp

end
