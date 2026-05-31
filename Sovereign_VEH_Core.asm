; ==============================================================================
; Sovereign VEH Core - Silent Signal Interceptor
; ==============================================================================
; Vectored Exception Handler for intercepting hardware breakpoints and other
; CPU exceptions without process termination.
;
; Architecture:
;   - First-handler VEH registration
;   - EXCEPTION_SINGLE_STEP interception (0x80000004)
;   - CONTEXT inspection and modification
;   - Silent execution continuation
;
; Exports:
;   INSTALL_VEH         - Register vectored exception handler
;   REMOVE_VEH          - Unregister vectored exception handler
;   EXCEPTION_HANDLER   - Core exception callback
;   GET_LAST_EXCEPTION  - Retrieve last exception info
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; External Imports
; ==============================================================================
EXTERN AddVectoredExceptionHandler : PROC
EXTERN RemoveVectoredExceptionHandler : PROC
EXTERN ExitProcess : PROC

; ==============================================================================
; Exception Constants
; ==============================================================================
EXCEPTION_SINGLE_STEP       equ 80000004h
EXCEPTION_BREAKPOINT        equ 80000003h
EXCEPTION_ACCESS_VIOLATION  equ 0C0000005h
EXCEPTION_CONTINUE_EXECUTION equ -1
EXCEPTION_CONTINUE_SEARCH    equ 0

; ==============================================================================
; Exception Info Structure
; ==============================================================================
ExceptionInfo struc
    ExceptionCode   dd ?
    ExceptionAddr   dq ?
    ThreadId        dd ?
    Padding         dd ?
    Rax             dq ?
    Rcx             dq ?
    Rdx             dq ?
    Rbx             dq ?
    Rsp             dq ?
    Rbp             dq ?
    Rsi             dq ?
    Rdi             dq ?
    Rip             dq ?
    Dr0             dq ?
    Dr1             dq ?
    Dr2             dq ?
    Dr3             dq ?
    Dr6             dq ?
    Dr7             dq ?
ExceptionInfo ends

; ==============================================================================
; Data Section
; ==============================================================================
.data
align 16
    g_VehHandle dq 0
    g_LastException ExceptionInfo <>
    g_HandlerInstalled dd 0
    
    ; Statistics
    g_ExceptionCount dd 0
    g_SingleStepCount dd 0
    g_AccessViolationCount dd 0

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; EXCEPTION_HANDLER - Core VEH callback
; ==============================================================================
; Input:  RCX = PEXCEPTION_POINTERS
; Output: RAX = EXCEPTION_CONTINUE_EXECUTION or EXCEPTION_CONTINUE_SEARCH
; ==============================================================================
EXCEPTION_HANDLER proc
    ; Save non-volatile registers
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    ; Get exception pointers
    mov r12, rcx              ; R12 = PEXCEPTION_POINTERS
    
    ; Get exception record
    mov r13, [r12]            ; R13 = PEXCEPTION_RECORD
    
    ; Get exception code
    mov eax, [r13 + 0]        ; ExceptionCode
    mov [g_LastException.ExceptionCode], eax
    
    ; Increment total exception count
    lock inc dword ptr [g_ExceptionCount]
    
    ; Check exception type
    cmp eax, EXCEPTION_SINGLE_STEP
    je handle_single_step
    cmp eax, EXCEPTION_BREAKPOINT
    je handle_breakpoint
    cmp eax, EXCEPTION_ACCESS_VIOLATION
    je handle_access_violation
    
    ; Unknown exception - continue search
    jmp continue_search
    
handle_single_step:
    ; Hardware breakpoint hit!
    lock inc dword ptr [g_SingleStepCount]
    
    ; Get context record
    mov r14, [r12 + 8]        ; R14 = PCONTEXT
    
    ; Save key registers to exception info
    mov rax, [r14 + 0F8h]     ; RIP
    mov [g_LastException.Rip], rax
    mov [g_LastException.ExceptionAddr], rax
    
    mov rax, [r14 + 0F0h]     ; RSP
    mov [g_LastException.Rsp], rax
    
    mov rax, [r14 + 078h]     ; RAX
    mov [g_LastException.Rax], rax
    
    mov rax, [r14 + 080h]     ; RCX
    mov [g_LastException.Rcx], rax
    
    mov rax, [r14 + 088h]     ; RDX
    mov [g_LastException.Rdx], rax
    
    ; Save debug registers
    mov rax, [r14 + 098h]     ; DR0
    mov [g_LastException.Dr0], rax
    mov rax, [r14 + 0A0h]     ; DR1
    mov [g_LastException.Dr1], rax
    mov rax, [r14 + 0A8h]     ; DR2
    mov [g_LastException.Dr2], rax
    mov rax, [r14 + 0B0h]     ; DR3
    mov [g_LastException.Dr3], rax
    mov rax, [r14 + 0B8h]     ; DR6
    mov [g_LastException.Dr6], rax
    mov rax, [r14 + 0C0h]     ; DR7
    mov [g_LastException.Dr7], rax
    
    ; Clear debug status (DR6) to prevent re-triggering
    mov qword ptr [r14 + 0B8h], 0
    
    ; Continue execution (skip the breakpoint instruction)
    ; The RIP is already pointing past the instruction
    mov eax, EXCEPTION_CONTINUE_EXECUTION
    jmp handler_exit
    
handle_breakpoint:
    ; Software breakpoint (INT 3)
    lock inc dword ptr [g_ExceptionCount]
    
    ; Get context
    mov r14, [r12 + 8]
    
    ; Advance RIP past the INT 3 instruction (1 byte)
    mov rax, [r14 + 0F8h]
    inc rax
    mov [r14 + 0F8h], rax
    
    mov eax, EXCEPTION_CONTINUE_EXECUTION
    jmp handler_exit
    
handle_access_violation:
    ; Access violation - log and continue search
    lock inc dword ptr [g_AccessViolationCount]
    
    ; Get faulting address
    mov rax, [r13 + 10h]      ; ExceptionInformation[0] = read/write
    mov rdx, [r13 + 18h]      ; ExceptionInformation[1] = faulting address
    mov [g_LastException.ExceptionAddr], rdx
    
    jmp continue_search
    
continue_search:
    xor eax, eax              ; EXCEPTION_CONTINUE_SEARCH
    
handler_exit:
    ; Restore non-volatile registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
EXCEPTION_HANDLER endp

; ==============================================================================
; INSTALL_VEH - Register vectored exception handler
; ==============================================================================
; Input:  None
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
INSTALL_VEH proc
    sub rsp, 40
    
    ; Check if already installed
    cmp dword ptr [g_HandlerInstalled], 0
    jne already_installed
    
    ; Register as first handler
    mov ecx, 1                    ; FirstHandler = TRUE
    lea rdx, EXCEPTION_HANDLER
    call AddVectoredExceptionHandler
    test rax, rax
    jz install_fail
    
    ; Save handle
    mov [g_VehHandle], rax
    mov dword ptr [g_HandlerInstalled], 1
    
    mov eax, 1
    jmp install_exit
    
already_installed:
    mov eax, 1
    jmp install_exit
    
install_fail:
    xor eax, eax
    
install_exit:
    add rsp, 40
    ret
INSTALL_VEH endp

; ==============================================================================
; REMOVE_VEH - Unregister vectored exception handler
; ==============================================================================
; Input:  None
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
REMOVE_VEH proc
    sub rsp, 40
    
    ; Check if installed
    cmp dword ptr [g_HandlerInstalled], 0
    je not_installed
    
    ; Remove handler
    mov rcx, [g_VehHandle]
    call RemoveVectoredExceptionHandler
    test eax, eax
    jz remove_fail
    
    ; Clear state
    mov qword ptr [g_VehHandle], 0
    mov dword ptr [g_HandlerInstalled], 0
    
    mov eax, 1
    jmp remove_exit
    
not_installed:
    mov eax, 1
    jmp remove_exit
    
remove_fail:
    xor eax, eax
    
remove_exit:
    add rsp, 40
    ret
REMOVE_VEH endp

; ==============================================================================
; GET_LAST_EXCEPTION - Retrieve last exception info
; ==============================================================================
; Input:  RCX = Pointer to output buffer (sizeof ExceptionInfo)
; Output: RAX = 1 always
; ==============================================================================
GET_LAST_EXCEPTION proc
    ; Copy exception info to output buffer
    lea rdx, g_LastException
    
    ; Copy all fields
    mov eax, [rdx + ExceptionInfo.ExceptionCode]
    mov [rcx + ExceptionInfo.ExceptionCode], eax
    
    mov rax, [rdx + ExceptionInfo.ExceptionAddr]
    mov [rcx + ExceptionInfo.ExceptionAddr], rax
    
    mov rax, [rdx + ExceptionInfo.Rax]
    mov [rcx + ExceptionInfo.Rax], rax
    
    mov rax, [rdx + ExceptionInfo.Rcx]
    mov [rcx + ExceptionInfo.Rcx], rax
    
    mov rax, [rdx + ExceptionInfo.Rdx]
    mov [rcx + ExceptionInfo.Rdx], rax
    
    mov rax, [rdx + ExceptionInfo.Rbx]
    mov [rcx + ExceptionInfo.Rbx], rax
    
    mov rax, [rdx + ExceptionInfo.Rsp]
    mov [rcx + ExceptionInfo.Rsp], rax
    
    mov rax, [rdx + ExceptionInfo.Rbp]
    mov [rcx + ExceptionInfo.Rbp], rax
    
    mov rax, [rdx + ExceptionInfo.Rsi]
    mov [rcx + ExceptionInfo.Rsi], rax
    
    mov rax, [rdx + ExceptionInfo.Rdi]
    mov [rcx + ExceptionInfo.Rdi], rax
    
    mov rax, [rdx + ExceptionInfo.Rip]
    mov [rcx + ExceptionInfo.Rip], rax
    
    mov rax, [rdx + ExceptionInfo.Dr0]
    mov [rcx + ExceptionInfo.Dr0], rax
    
    mov rax, [rdx + ExceptionInfo.Dr1]
    mov [rcx + ExceptionInfo.Dr1], rax
    
    mov rax, [rdx + ExceptionInfo.Dr2]
    mov [rcx + ExceptionInfo.Dr2], rax
    
    mov rax, [rdx + ExceptionInfo.Dr3]
    mov [rcx + ExceptionInfo.Dr3], rax
    
    mov rax, [rdx + ExceptionInfo.Dr6]
    mov [rcx + ExceptionInfo.Dr6], rax
    
    mov rax, [rdx + ExceptionInfo.Dr7]
    mov [rcx + ExceptionInfo.Dr7], rax
    
    mov eax, 1
    ret
GET_LAST_EXCEPTION endp

; ==============================================================================
; GET_EXCEPTION_STATS - Get exception statistics
; ==============================================================================
; Input:  RCX = Pointer to output buffer (12 bytes)
;         [RCX+0] = Total exception count
;         [RCX+4] = Single step count
;         [RCX+8] = Access violation count
; Output: RAX = 1 always
; ==============================================================================
GET_EXCEPTION_STATS proc
    mov eax, [g_ExceptionCount]
    mov [rcx + 0], eax
    mov eax, [g_SingleStepCount]
    mov [rcx + 4], eax
    mov eax, [g_AccessViolationCount]
    mov [rcx + 8], eax
    mov eax, 1
    ret
GET_EXCEPTION_STATS endp

end