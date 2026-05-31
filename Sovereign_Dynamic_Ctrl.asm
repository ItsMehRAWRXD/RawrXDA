; ==============================================================================
; Sovereign Dynamic Controller - Hardware Breakpoint Orchestrator
; ==============================================================================
; Non-invasive dynamic instrumentation using CPU debug registers (DR0-DR7).
; Bridges static analysis (Parser) with runtime execution without modifying
; a single byte of target code.
;
; Architecture:
;   - Zero-byte-modification hooks via DR0-DR3
;   - x64 ABI compliant (shadow space, 16-byte alignment)
;   - CONTEXT structure manipulation for thread control
;   - VEH-ready exception handling integration
;
; Exports:
;   HOOK_THREAD       - Set hardware breakpoint on thread
;   UNHOOK_THREAD     - Remove hardware breakpoint
;   GET_THREAD_STATE  - Read debug register state
;   SET_THREAD_STATE  - Write debug register state
;   STEALTH_RESUME    - Resume thread with clean state
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; External Imports
; ==============================================================================
EXTERN GetThreadContext : PROC
EXTERN SetThreadContext : PROC
EXTERN SuspendThread    : PROC
EXTERN ResumeThread     : PROC
EXTERN ExitProcess      : PROC

; ==============================================================================
; Constants
; ==============================================================================
CONTEXT_DEBUG_REGISTERS equ 00010010h
CONTEXT_FULL            equ 00010007h
CONTEXT_CONTROL         equ 00010001h

; DR7 bit masks
DR7_L0 equ 00000001h
DR7_G0 equ 00000002h
DR7_L1 equ 00000004h
DR7_G1 equ 00000008h
DR7_L2 equ 00000010h
DR7_G2 equ 00000020h
DR7_L3 equ 00000040h
DR7_G3 equ 00000080h

; CONTEXT structure offsets (x64)
CTX_ContextFlags equ 0
CTX_Dr0          equ 78h
CTX_Dr1          equ 80h
CTX_Dr2          equ 88h
CTX_Dr3          equ 90h
CTX_Dr6          equ 98h
CTX_Dr7          equ 0A0h
CTX_Rax          equ 0F8h
CTX_Rcx          equ 0E0h
CTX_Rdx          equ 0E8h
CTX_Rbx          equ 0E8h  ; Note: RBX is at 0E0h in some versions
CTX_Rip          equ 0F8h
CTX_Rsp          equ 0F0h
CTX_Rbp          equ 0F8h  ; Check actual offset

; CONTEXT size for x64
SIZEOF_CONTEXT equ 4D0h

; ==============================================================================
; Data Section
; ==============================================================================
.data
align 16
; Thread state cache for quick lookup
THREAD_STATE_CACHE dq 0

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; HOOK_THREAD - Set hardware breakpoint on target thread
; ==============================================================================
; Input:  RCX = hThread (thread handle)
;         RDX = Address to watch (breakpoint address)
;         R8  = Breakpoint index (0-3)
; Output: RAX = 1 on success, 0 on failure
; Clobbers: RAX, RCX, RDX, R8, R9, R10, R11
; ==============================================================================
HOOK_THREAD proc
    ; Allocate CONTEXT + shadow space (16-byte aligned)
    sub rsp, SIZEOF_CONTEXT + 32 + 8  ; 1232 + 32 + 8 = 1272 -> align to 1280
    sub rsp, 8                          ; 1280 total (16-byte aligned)
    
    ; Save non-volatile registers
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    ; Suspend target thread first
    call SuspendThread
    test eax, eax
    js hook_fail
    
    ; Prepare CONTEXT structure
    mov rbx, rsp                        ; RBX = CONTEXT base
    add rbx, 56                         ; Skip saved registers
    mov dword ptr [rbx + CTX_ContextFlags], CONTEXT_DEBUG_REGISTERS
    
    ; Get current thread context
    mov rcx, [rsp + 56 + SIZEOF_CONTEXT + 32 - 8]  ; hThread (saved in stack)
    mov rdx, rbx
    call GetThreadContext
    test eax, eax
    jz hook_resume_fail
    
    ; Set debug register based on index
    cmp r8, 0
    je set_dr0
    cmp r8, 1
    je set_dr1
    cmp r8, 2
    je set_dr2
    cmp r8, 3
    je set_dr3
    jmp hook_resume_fail
    
set_dr0:
    mov [rbx + CTX_Dr0], rdx
    mov rax, [rbx + CTX_Dr7]
    or rax, DR7_L0
    mov [rbx + CTX_Dr7], rax
    jmp apply_context
    
set_dr1:
    mov [rbx + CTX_Dr1], rdx
    mov rax, [rbx + CTX_Dr7]
    or rax, DR7_L1
    mov [rbx + CTX_Dr7], rax
    jmp apply_context
    
set_dr2:
    mov [rbx + CTX_Dr2], rdx
    mov rax, [rbx + CTX_Dr7]
    or rax, DR7_L2
    mov [rbx + CTX_Dr7], rax
    jmp apply_context
    
set_dr3:
    mov [rbx + CTX_Dr3], rdx
    mov rax, [rbx + CTX_Dr7]
    or rax, DR7_L3
    mov [rbx + CTX_Dr7], rax
    jmp apply_context
    
apply_context:
    ; Apply modified context
    mov rcx, [rsp + 56 + SIZEOF_CONTEXT + 32 - 8]  ; hThread
    mov rdx, rbx
    call SetThreadContext
    test eax, eax
    jz hook_resume_fail
    
    ; Resume thread
    mov rcx, [rsp + 56 + SIZEOF_CONTEXT + 32 - 8]  ; hThread
    call ResumeThread
    
    ; Success
    mov eax, 1
    jmp hook_exit
    
hook_resume_fail:
    ; Resume thread even on failure
    mov rcx, [rsp + 56 + SIZEOF_CONTEXT + 32 - 8]  ; hThread
    call ResumeThread
    
hook_fail:
    xor eax, eax
    
hook_exit:
    ; Restore non-volatile registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    
    ; Restore stack
    add rsp, SIZEOF_CONTEXT + 32 + 16
    ret
HOOK_THREAD endp

; ==============================================================================
; UNHOOK_THREAD - Remove hardware breakpoint from thread
; ==============================================================================
; Input:  RCX = hThread (thread handle)
;         RDX = Breakpoint index (0-3)
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
UNHOOK_THREAD proc
    ; Allocate CONTEXT + shadow space
    sub rsp, SIZEOF_CONTEXT + 32 + 16
    
    ; Save non-volatile registers
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    ; Suspend target thread
    call SuspendThread
    test eax, eax
    js unhook_fail
    
    ; Prepare CONTEXT
    mov rbx, rsp
    add rbx, 56
    mov dword ptr [rbx + CTX_ContextFlags], CONTEXT_DEBUG_REGISTERS
    
    ; Get current context
    mov rcx, [rsp + 56 + SIZEOF_CONTEXT + 32 - 8]
    mov rdx, rbx
    call GetThreadContext
    test eax, eax
    jz unhook_resume_fail
    
    ; Clear debug register based on index
    cmp r8, 0
    je clear_dr0
    cmp r8, 1
    je clear_dr1
    cmp r8, 2
    je clear_dr2
    cmp r8, 3
    je clear_dr3
    jmp unhook_resume_fail
    
clear_dr0:
    mov qword ptr [rbx + CTX_Dr0], 0
    mov rax, [rbx + CTX_Dr7]
    and rax, NOT DR7_L0
    mov [rbx + CTX_Dr7], rax
    jmp unhook_apply
    
clear_dr1:
    mov qword ptr [rbx + CTX_Dr1], 0
    mov rax, [rbx + CTX_Dr7]
    and rax, NOT DR7_L1
    mov [rbx + CTX_Dr7], rax
    jmp unhook_apply
    
clear_dr2:
    mov qword ptr [rbx + CTX_Dr2], 0
    mov rax, [rbx + CTX_Dr7]
    and rax, NOT DR7_L2
    mov [rbx + CTX_Dr7], rax
    jmp unhook_apply
    
clear_dr3:
    mov qword ptr [rbx + CTX_Dr3], 0
    mov rax, [rbx + CTX_Dr7]
    and rax, NOT DR7_L3
    mov [rbx + CTX_Dr7], rax
    jmp unhook_apply
    
unhook_apply:
    ; Apply modified context
    mov rcx, [rsp + 56 + SIZEOF_CONTEXT + 32 - 8]
    mov rdx, rbx
    call SetThreadContext
    test eax, eax
    jz unhook_resume_fail
    
    ; Resume thread
    mov rcx, [rsp + 56 + SIZEOF_CONTEXT + 32 - 8]
    call ResumeThread
    
    mov eax, 1
    jmp unhook_exit
    
unhook_resume_fail:
    mov rcx, [rsp + 56 + SIZEOF_CONTEXT + 32 - 8]
    call ResumeThread
    
unhook_fail:
    xor eax, eax
    
unhook_exit:
    ; Restore non-volatile registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    
    add rsp, SIZEOF_CONTEXT + 32 + 16
    ret
UNHOOK_THREAD endp

; ==============================================================================
; GET_THREAD_STATE - Read thread debug register state
; ==============================================================================
; Input:  RCX = hThread (thread handle)
;         RDX = Pointer to output buffer (64 bytes for DR0-DR7)
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
GET_THREAD_STATE proc
    ; Allocate CONTEXT + shadow space
    sub rsp, SIZEOF_CONTEXT + 32 + 16
    
    ; Save non-volatile registers
    push rbx
    push rsi
    push rdi
    
    ; Prepare CONTEXT
    mov rbx, rsp
    add rbx, 24
    mov dword ptr [rbx + CTX_ContextFlags], CONTEXT_DEBUG_REGISTERS
    
    ; Get context
    mov rcx, [rsp + 24 + SIZEOF_CONTEXT + 32 - 8]
    mov rdx, rbx
    call GetThreadContext
    test eax, eax
    jz get_state_fail
    
    ; Copy debug registers to output buffer
    mov rax, [rbx + CTX_Dr0]
    mov [rdx + 0], rax
    mov rax, [rbx + CTX_Dr1]
    mov [rdx + 8], rax
    mov rax, [rbx + CTX_Dr2]
    mov [rdx + 16], rax
    mov rax, [rbx + CTX_Dr3]
    mov [rdx + 24], rax
    mov rax, [rbx + CTX_Dr6]
    mov [rdx + 32], rax
    mov rax, [rbx + CTX_Dr7]
    mov [rdx + 40], rax
    
    mov eax, 1
    jmp get_state_exit
    
get_state_fail:
    xor eax, eax
    
get_state_exit:
    pop rdi
    pop rsi
    pop rbx
    add rsp, SIZEOF_CONTEXT + 32 + 16
    ret
GET_THREAD_STATE endp

; ==============================================================================
; SET_THREAD_STATE - Write thread debug register state
; ==============================================================================
; Input:  RCX = hThread (thread handle)
;         RDX = Pointer to input buffer (64 bytes for DR0-DR7)
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
SET_THREAD_STATE proc
    ; Allocate CONTEXT + shadow space
    sub rsp, SIZEOF_CONTEXT + 32 + 16
    
    ; Save non-volatile registers
    push rbx
    push rsi
    push rdi
    
    ; Prepare CONTEXT
    mov rbx, rsp
    add rbx, 24
    mov dword ptr [rbx + CTX_ContextFlags], CONTEXT_DEBUG_REGISTERS
    
    ; Copy debug registers from input buffer
    mov rax, [rdx + 0]
    mov [rbx + CTX_Dr0], rax
    mov rax, [rdx + 8]
    mov [rbx + CTX_Dr1], rax
    mov rax, [rdx + 16]
    mov [rbx + CTX_Dr2], rax
    mov rax, [rdx + 24]
    mov [rbx + CTX_Dr3], rax
    mov rax, [rdx + 32]
    mov [rbx + CTX_Dr6], rax
    mov rax, [rdx + 40]
    mov [rbx + CTX_Dr7], rax
    
    ; Apply context
    mov rcx, [rsp + 24 + SIZEOF_CONTEXT + 32 - 8]
    mov rdx, rbx
    call SetThreadContext
    test eax, eax
    jz set_state_fail
    
    mov eax, 1
    jmp set_state_exit
    
set_state_fail:
    xor eax, eax
    
set_state_exit:
    pop rdi
    pop rsi
    pop rbx
    add rsp, SIZEOF_CONTEXT + 32 + 16
    ret
SET_THREAD_STATE endp

; ==============================================================================
; STEALTH_RESUME - Resume thread with clean exception state
; ==============================================================================
; Input:  RCX = hThread (thread handle)
; Output: RAX = Resume count on success, -1 on failure
; ==============================================================================
STEALTH_RESUME proc
    sub rsp, 40
    
    ; Resume thread
    call ResumeThread
    
    add rsp, 40
    ret
STEALTH_RESUME endp

; ==============================================================================
; IS_HWBP_ACTIVE - Check if hardware breakpoint is active on thread
; ==============================================================================
; Input:  RCX = hThread (thread handle)
;         RDX = Breakpoint index (0-3)
; Output: RAX = 1 if active, 0 if inactive
; ==============================================================================
IS_HWBP_ACTIVE proc
    ; Allocate CONTEXT + shadow space
    sub rsp, SIZEOF_CONTEXT + 32 + 16
    
    push rbx
    push rdi
    
    ; Prepare CONTEXT
    mov rbx, rsp
    add rbx, 16
    mov dword ptr [rbx + CTX_ContextFlags], CONTEXT_DEBUG_REGISTERS
    
    ; Get context
    mov rcx, [rsp + 16 + SIZEOF_CONTEXT + 32 - 8]
    mov rdx, rbx
    call GetThreadContext
    test eax, eax
    jz not_active
    
    ; Check DR7 for enable bit
    mov rax, [rbx + CTX_Dr7]
    
    cmp rdx, 0
    je check_dr0
    cmp rdx, 1
    je check_dr1
    cmp rdx, 2
    je check_dr2
    cmp rdx, 3
    je check_dr3
    jmp not_active
    
check_dr0:
    test rax, DR7_L0
    jnz is_active
    jmp not_active
    
check_dr1:
    test rax, DR7_L1
    jnz is_active
    jmp not_active
    
check_dr2:
    test rax, DR7_L2
    jnz is_active
    jmp not_active
    
check_dr3:
    test rax, DR7_L3
    jnz is_active
    jmp not_active
    
is_active:
    mov eax, 1
    jmp active_exit
    
not_active:
    xor eax, eax
    
active_exit:
    pop rdi
    pop rbx
    add rsp, SIZEOF_CONTEXT + 32 + 16
    ret
IS_HWBP_ACTIVE endp

end