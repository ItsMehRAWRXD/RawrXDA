; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Model_StateMachine.asm
; Stub implementation for Model State Machine
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

.CODE

ModelState_Initialize PROC FRAME
    ; Initialize model state machine
    ; Sets initial state to STATE_UNLOADED (0)
    ; Allocates state transition table
    ; Returns: RAX = 1 on success
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    ; States: 0=UNLOADED, 1=LOADING, 2=READY, 3=INFERRING, 4=ERROR, 5=UNLOADING
    ; Allocate state context: 64 bytes
    mov rcx, 0
    mov edx, 64
    mov r8d, 3000h
    mov r9d, 04h
    call VirtualAlloc
    test rax, rax
    jz @@msi_fail
    
    mov rbx, rax
    lea rcx, [g_ModelStateCtx]
    mov [rcx], rbx
    
    ; Initialize fields
    mov DWORD PTR [rbx], 0           ; current_state = UNLOADED
    mov DWORD PTR [rbx+4], 0         ; prev_state
    mov QWORD PTR [rbx+8], 0         ; model_instance ptr
    mov QWORD PTR [rbx+16], 0        ; error_code
    mov DWORD PTR [rbx+24], 0        ; transition_count
    
    ; Initialize CS for thread-safe transitions
    lea rcx, [g_StateCS]
    call InitializeCriticalSection
    
    mov rax, 1
    add rsp, 48
    pop rbx
    ret
    
@@msi_fail:
    xor eax, eax
    add rsp, 48
    pop rbx
    ret
ModelState_Initialize ENDP

ModelState_Transition PROC FRAME
    ; Transition model to new state with validation
    ; RCX = new_state (0-5)
    ; Returns: RAX = 1 if transition valid, 0 if rejected
    push rbx
    .pushreg rbx
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov ebx, ecx                     ; new_state
    
    ; Bounds check
    cmp ebx, 5
    ja @@mst_reject
    
    ; Enter CS
    lea rcx, [g_StateCS]
    call EnterCriticalSection
    
    mov rcx, [g_ModelStateCtx]
    test rcx, rcx
    jz @@mst_unlock_reject
    
    ; Get current state
    mov eax, DWORD PTR [rcx]         ; current_state
    
    ; Validate transition (simplified state machine):
    ; UNLOADED(0) -> LOADING(1) only
    ; LOADING(1)  -> READY(2) or ERROR(4)
    ; READY(2)    -> INFERRING(3) or UNLOADING(5)
    ; INFERRING(3)-> READY(2) or ERROR(4)
    ; ERROR(4)    -> UNLOADING(5) or UNLOADED(0)
    ; UNLOADING(5)-> UNLOADED(0)
    
    cmp eax, 0                       ; UNLOADED
    jne @@mst_check1
    cmp ebx, 1                       ; -> LOADING only
    jne @@mst_unlock_reject
    jmp @@mst_apply
    
@@mst_check1:
    cmp eax, 1                       ; LOADING
    jne @@mst_check2
    cmp ebx, 2                       ; -> READY
    je @@mst_apply
    cmp ebx, 4                       ; -> ERROR
    je @@mst_apply
    jmp @@mst_unlock_reject
    
@@mst_check2:
    cmp eax, 2                       ; READY
    jne @@mst_check3
    cmp ebx, 3                       ; -> INFERRING
    je @@mst_apply
    cmp ebx, 5                       ; -> UNLOADING
    je @@mst_apply
    jmp @@mst_unlock_reject
    
@@mst_check3:
    cmp eax, 3                       ; INFERRING
    jne @@mst_check4
    cmp ebx, 2                       ; -> READY
    je @@mst_apply
    cmp ebx, 4                       ; -> ERROR
    je @@mst_apply
    jmp @@mst_unlock_reject
    
@@mst_check4:
    cmp eax, 4                       ; ERROR
    jne @@mst_check5
    cmp ebx, 5                       ; -> UNLOADING
    je @@mst_apply
    cmp ebx, 0                       ; -> UNLOADED
    je @@mst_apply
    jmp @@mst_unlock_reject
    
@@mst_check5:
    cmp eax, 5                       ; UNLOADING
    jne @@mst_unlock_reject
    cmp ebx, 0                       ; -> UNLOADED
    jne @@mst_unlock_reject
    
@@mst_apply:
    ; Save prev state, set new
    mov DWORD PTR [rcx+4], eax       ; prev_state = old current
    mov DWORD PTR [rcx], ebx         ; current_state = new
    inc DWORD PTR [rcx+24]           ; transition_count++
    
    lea rcx, [g_StateCS]
    call LeaveCriticalSection
    
    mov rax, 1
    add rsp, 40
    pop rbx
    ret
    
@@mst_unlock_reject:
    lea rcx, [g_StateCS]
    call LeaveCriticalSection
    
@@mst_reject:
    xor eax, eax
    add rsp, 40
    pop rbx
    ret
ModelState_Transition ENDP

ModelState_AcquireInstance PROC FRAME
    ; Acquire model instance pointer (thread-safe)
    ; Returns: RAX = model instance pointer, NULL if not in READY/INFERRING state
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    lea rcx, [g_StateCS]
    call EnterCriticalSection
    
    mov rcx, [g_ModelStateCtx]
    test rcx, rcx
    jz @@msa_fail
    
    ; Only return instance if in READY(2) or INFERRING(3) state
    mov eax, DWORD PTR [rcx]
    cmp eax, 2
    je @@msa_ok
    cmp eax, 3
    je @@msa_ok
    jmp @@msa_fail
    
@@msa_ok:
    mov rax, QWORD PTR [rcx+8]       ; model_instance ptr
    push rax
    lea rcx, [g_StateCS]
    call LeaveCriticalSection
    pop rax
    add rsp, 40
    ret
    
@@msa_fail:
    lea rcx, [g_StateCS]
    call LeaveCriticalSection
    xor eax, eax
    add rsp, 40
    ret
ModelState_AcquireInstance ENDP

PUBLIC ModelState_Initialize
PUBLIC ModelState_Transition
PUBLIC ModelState_AcquireInstance

END