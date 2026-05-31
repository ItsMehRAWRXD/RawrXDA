; ============================================================================
; sovereign_expert_gating.asm
; MoE Expert Selection & On-Demand Commitment
; ============================================================================
; Purpose: Select active experts from gating logits and commit their
;          tensor regions from reservation pool (MEM_COMMIT only active experts)
;          Remaining experts stay MEM_RESERVE (no physical RAM)
; ============================================================================

; --- External Win32 Symbols ---
extern VirtualAlloc              : proc
extern VirtualFree               : proc
extern GetLargePageMinimum       : proc
extern OutputDebugStringA        : proc

; --- External RawrXD Symbols ---
extern SovMem_SpinAcquire        : proc
extern SovMem_SpinRelease        : proc
extern SovMem_TLS_GetActivationBuffer : proc

; --- Constants ---
MEM_COMMIT                equ 00001000h
PAGE_READWRITE            equ 04h
MEM_DECOMMIT             equ 00004000h
kMaxExpertsPerMoE         equ 128
kExpertTensorBytes        equ 134217728  ; 128 MB per expert (example)

; --- Data Section ---
.data
    ; State tracking
    g_activeExpertMask     dd 0            ; Bitmask of currently active experts (up to 32)
    g_reservedExpertBases  dq kMaxExpertsPerMoE dup(0)  ; Base address per expert
    g_commitLockSlot       dd 7            ; Lock slot for expert commitment coordination

    ; Strings for debug output
    info_expert_selected   db "[ExpertGate] Expert %d selected (total active: %d)", 0
    info_commit_begin      db "[ExpertGate] Committing expert tensor region", 0
    info_commit_ok         db "[ExpertGate] Expert commitment successful", 0
    err_commit_failed      db "[ExpertGate] Expert commitment failed", 0

; --- Code Section ---
.code

; ============================================================================
; SovExp_InitializeExpertReservations
; Reserve address space for all experts (without committing RAM)
;
; Parameters:
;   RCX = Number of experts (typically 128 for large MoE)
;   RDX = Base address for expert tensor pool (0 = auto)
;   R8  = Bytes per expert (typically 128 MB)
;
; Returns:
;   RAX = 1 if all reservations succeeded, 0 otherwise
; ============================================================================
SovExp_InitializeExpertReservations proc
    sub rsp, 56
    mov [rsp+48], rbx
    mov [rsp+40], r12
    mov [rsp+32], r13
    
    mov r12, rcx                ; R12 = number of experts
    mov r13, r8                 ; R13 = bytes per expert
    xor rbx, rbx                ; RBX = expert index
    
    cmp r12, kMaxExpertsPerMoE
    jle _init_loop
    mov r12, kMaxExpertsPerMoE
    
_init_loop:
    cmp rbx, r12
    jge _init_done
    
    ; Reserve memory for this expert (no physical RAM committed yet)
    mov rcx, r13                ; Size (bytes per expert)
    mov edx, 00002000h          ; MEM_RESERVE only (no physical commitment)
    mov r8d, 01h                ; PAGE_NOACCESS (reserved but not accessible)
    xor r9, r9                  ; lpAddress = NULL (auto)
    call VirtualAlloc
    
    test rax, rax
    jz _init_failed
    
    ; Store base address
    lea rdx, g_reservedExpertBases
    mov [rdx + rbx*8], rax
    
    inc rbx
    jmp _init_loop
    
_init_done:
    xor eax, eax                ; Return SUCCESS (0 = success in this context, but let's use 1)
    mov eax, 1
    jmp _init_cleanup
    
_init_failed:
    xor eax, eax                ; Return FAIL
    
_init_cleanup:
    mov rbx, [rsp+48]
    mov r12, [rsp+40]
    mov r13, [rsp+32]
    add rsp, 56
    ret
SovExp_InitializeExpertReservations endp

; ============================================================================
; SovExp_SelectExpertsByGating
; Given gating logits, select top-K experts and mark for activation
;
; Parameters:
;   RCX = Pointer to gating logits (floats, one per expert)
;   RDX = Number of experts
;   R8  = Top-K (e.g., 2 for sparse MoE)
;
; Returns:
;   RAX = Bitmask of selected experts (for sparse MoE, up to 32 experts)
; ============================================================================
SovExp_SelectExpertsByGating proc
    sub rsp, 56
    mov [rsp+48], rbx
    mov [rsp+40], r12
    
    mov r12, rcx                ; R12 = gating logits
    mov rbx, rdx                ; RBX = num experts
    mov r9, r8                  ; R9 = top-K
    
    cmp rbx, 32
    jle _select_valid_count
    mov rbx, 32                 ; Cap at 32 for bitmask
    
_select_valid_count:
    ; Simple selection: mark first K experts as active.
    ; (In production: sort logits and select top-K.)
    xor eax, eax                ; RAX = bitmask
    xor r10d, r10d              ; R10D = index
    
_select_loop:
    cmp r10, r9
    jge _select_done

    ; Set bit at index R10 in EAX.
    ; Keep this deterministic and avoid clobbering the loop index.
    bts eax, r10d

    inc r10
    jmp _select_loop
    
_select_done:
    ; Store result
    mov g_activeExpertMask, eax
    
    mov rbx, [rsp+48]
    mov r12, [rsp+40]
    add rsp, 56
    ret
SovExp_SelectExpertsByGating endp

; ============================================================================
; SovExp_CommitSelectedExperts
; For each selected expert, commit its tensor from reservation pool
; Uses spinlock to prevent concurrent commitment races
;
; Returns:
;   RAX = 1 if all commits succeeded, 0 otherwise
; ============================================================================
SovExp_CommitSelectedExperts proc
    sub rsp, 48
    mov [rsp+40], r12
    
    ; Acquire spinlock
    mov ecx, g_commitLockSlot
    call SovMem_SpinAcquire
    test rax, rax
    jz _commit_lock_fail
    
    ; Get active expert mask
    mov r12d, g_activeExpertMask
    xor r10d, r10d              ; R10D = expert index
    
_commit_loop:
    cmp r10, 32
    jge _commit_release

    ; Check if expert R10 is active
    bt r12d, r10d
    jnc _commit_next

    ; Expert is active - commit its tensor
    lea rax, g_reservedExpertBases
    mov rsi, [rax + r10*8]      ; RSI = base address for expert
    test rsi, rsi
    jz _commit_next

    ; Commit: VirtualAlloc with MEM_COMMIT on the reserved region
    mov rcx, rsi                 ; lpAddress
    mov edx, kExpertTensorBytes  ; dwSize
    mov r8d, MEM_COMMIT
    mov r9d, PAGE_READWRITE
    call VirtualAlloc

    test rax, rax
    jz _commit_failed
    
_commit_next:
    inc r10
    jmp _commit_loop
    
_commit_release:
    ; Release spinlock
    mov ecx, g_commitLockSlot
    call SovMem_SpinRelease
    
    mov rax, 1
    jmp _commit_done
    
_commit_failed:
    ; Release spinlock on failure
    mov ecx, g_commitLockSlot
    call SovMem_SpinRelease
    
    xor eax, eax
    
_commit_lock_fail:
    xor eax, eax
    
_commit_done:
    mov r12, [rsp+40]
    add rsp, 48
    ret
SovExp_CommitSelectedExperts endp

; ============================================================================
; SovExp_GetActivatedTensorBase
; Retrieve the activated (committed) tensor address for a selected expert
;
; Parameters:
;   RCX = Expert index (0-based)
;
; Returns:
;   RAX = Base address of activated tensor, or 0 if not committed
; ============================================================================
SovExp_GetActivatedTensorBase proc
    ; Verify expert is in active mask
    mov eax, g_activeExpertMask
    mov edx, 1
    bt eax, ecx
    jnc _get_base_none
    
    ; Retrieve base address
    lea rax, g_reservedExpertBases
    mov rax, [rax + rcx*8]
    ret
    
_get_base_none:
    xor eax, eax
    ret
SovExp_GetActivatedTensorBase endp

; ============================================================================
; SovExp_DecommitUnselectedExperts
; Release commitment for inactive experts (optional: free physical RAM)
; Only decommit, do not release reservation (keep address space reserved)
;
; Returns:
;   RAX = Number of experts decommitted
; ============================================================================
SovExp_DecommitUnselectedExperts proc
    sub rsp, 40
    mov [rsp+32], r12
    
    mov r12d, g_activeExpertMask
    xor r11d, r11d              ; R11D = count decommitted
    xor r10d, r10d              ; R10D = expert index
    
_decommit_loop:
    cmp r10, 32
    jge _decommit_done

    ; Check if expert is NOT active
    bt r12d, r10d
    jc _decommit_next

    ; Expert is inactive - decommit its tensor
    lea rdx, g_reservedExpertBases
    mov rsi, [rdx + r10*8]      ; RSI = base address
    test rsi, rsi
    jz _decommit_next

    ; VirtualFree with MEM_DECOMMIT (not MEM_RELEASE)
    ; This frees physical RAM but keeps address space reserved
    mov rcx, rsi                 ; lpAddress
    mov rdx, kExpertTensorBytes  ; dwSize
    mov r8d, MEM_DECOMMIT
    call VirtualFree

    test eax, eax
    jz _decommit_next
    inc r11d                     ; Increment count only on success
    
_decommit_next:
    inc r10
    jmp _decommit_loop
    
_decommit_done:
    mov eax, r11d
    mov r12, [rsp+32]
    add rsp, 40
    ret
SovExp_DecommitUnselectedExperts endp

end
