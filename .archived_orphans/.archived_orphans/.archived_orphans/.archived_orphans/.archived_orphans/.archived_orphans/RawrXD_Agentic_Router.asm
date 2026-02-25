; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Agentic_Router.asm
; Stub implementation for Agentic Router
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

.CODE

AgentRouter_Initialize PROC FRAME
    ; Initialize the agentic task router
    ; Sets up dispatch table and worker thread pool
    ; Returns: RAX = 1 on success, 0 on failure
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    ; Allocate dispatch table: 16 entries x 16 bytes (handler_ptr + context)
    mov rcx, 0                       ; lpAddress
    mov edx, 256                     ; 16 * 16
    mov r8d, 3000h                   ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 04h                     ; PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @@ar_init_fail
    mov rbx, rax
    
    ; Zero the table
    mov rdi, rax
    mov ecx, 64                      ; 256/4
    xor eax, eax
    rep stosd
    
    ; Store dispatch table pointer globally
    lea rcx, [g_DispatchTable]
    mov [rcx], rbx
    
    ; Initialize task queue critical section
    lea rcx, [g_RouterCS]
    call InitializeCriticalSection
    
    ; Mark router as initialized
    lea rcx, [g_RouterInitialized]
    mov DWORD PTR [rcx], 1
    
    mov rax, 1
    add rsp, 48
    pop rbx
    ret
    
@@ar_init_fail:
    xor eax, eax
    add rsp, 48
    pop rbx
    ret
AgentRouter_Initialize ENDP

AgentRouter_ExecuteTask PROC FRAME
    ; Execute a task through the agentic router
    ; RCX = pointer to AGENT_TASK structure:
    ;   +0: DWORD task_type (0=inference, 1=tokenize, 2=embed, 3=decode)
    ;   +8: QWORD input_ptr
    ;   +16: QWORD input_size
    ;   +24: QWORD output_ptr
    ;   +32: QWORD output_size
    ; Returns: RAX = 1 on success, 0 on failure
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx                     ; task ptr
    test rbx, rbx
    jz @@ar_exec_fail
    
    ; Validate router is initialized
    lea rcx, [g_RouterInitialized]
    cmp DWORD PTR [rcx], 1
    jne @@ar_exec_fail
    
    ; Enter critical section
    lea rcx, [g_RouterCS]
    call EnterCriticalSection
    
    ; Get task type and bounds-check
    mov eax, DWORD PTR [rbx]
    cmp eax, 4
    jae @@ar_exec_unlock_fail
    
    ; Look up handler in dispatch table
    mov rcx, [g_DispatchTable]
    test rcx, rcx
    jz @@ar_exec_unlock_fail
    
    shl eax, 4                       ; * 16 bytes per entry
    mov r12, QWORD PTR [rcx + rax]   ; handler function ptr
    test r12, r12
    jz @@ar_exec_unlock_fail
    
    ; Leave CS before calling handler
    lea rcx, [g_RouterCS]
    call LeaveCriticalSection
    
    ; Call the handler: handler(task_ptr)
    mov rcx, rbx
    call r12
    
    mov rax, 1
    add rsp, 40
    pop r12
    pop rbx
    ret
    
@@ar_exec_unlock_fail:
    lea rcx, [g_RouterCS]
    call LeaveCriticalSection
    
@@ar_exec_fail:
    xor eax, eax
    add rsp, 40
    pop r12
    pop rbx
    ret
AgentRouter_ExecuteTask ENDP

PUBLIC AgentRouter_Initialize
PUBLIC AgentRouter_ExecuteTask

END