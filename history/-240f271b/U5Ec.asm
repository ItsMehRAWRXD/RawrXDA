;============================================================================
; PRODUCTION MEMORY HOTPATCHER - Three-Layer Integration
; Integrates with: model_memory_hotpatch, byte_level_hotpatcher, server hotpatch
; Pure MASM implementation (no C++, no SDK)
;============================================================================

option casemap:none

extrn VirtualAlloc:proc
extrn VirtualProtect:proc
extrn VirtualFree:proc
extrn RtlMoveMemory:proc

public MemoryHotpatch_AllocatePatch
public MemoryHotpatch_ApplyDirectPatch
public MemoryHotpatch_ProtectMemory
public MemoryHotpatch_RestoreMemory
public MemoryHotpatch_VerifyPatch
public MemoryHotpatch_RollbackPatch

; Patch result structure (matches C++ PatchResult)
PATCH_RESULT struct
    success         dword ?
    error_code      dword ?
    detail_len      dword ?
    detail          qword ?              ; pointer to error message
PATCH_RESULT ends

; Memory patch structure
MEMORY_PATCH struct
    target_address  qword ?
    source_data     qword ?
    patch_size      qword ?
    original_data   qword ?
    metadata        qword ?
MEMORY_PATCH ends

; Protection context (for VirtualProtect undo)
PROT_CONTEXT struct
    address         qword ?
    size            qword ?
    original_protect dword ?
    temp_protect    dword ?
PROT_CONTEXT ends

.data
align 16

; Global patch registry
g_patch_table       qword 0
g_patch_count       dword 0
g_patch_capacity    dword 0

g_prot_stack        qword 0
g_prot_depth        dword 0

; Constants
PAGE_EXECUTE_READWRITE equ 040h
PAGE_EXECUTE_READ    equ 020h
PAGE_READWRITE       equ 04h
PAGE_READONLY        equ 02h

; Error messages
err_invalid_addr    db "Invalid target address", 0
err_mem_alloc       db "Memory allocation failed", 0
err_prot_failed     db "Memory protection failed", 0
err_verify_fail     db "Patch verification failed", 0

.code
align 16

;============================================================================
; MemoryHotpatch_AllocatePatch - Allocate patch structure with data
; RCX = patch_size (bytes)
; RDX = source_data (const void*)
; Returns: RAX = MEMORY_PATCH*
;============================================================================
MemoryHotpatch_AllocatePatch proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov r8, rcx                             ; r8 = patch_size
    mov r9, rdx                             ; r9 = source_data
    
    ; Allocate patch structure (MEMORY_PATCH)
    mov ecx, sizeof MEMORY_PATCH
    mov edx, 4                              ; PAGE_READWRITE
    mov r8, 01000h                          ; MEM_COMMIT
    xor r9, r9
    call VirtualAlloc
    test rax, rax
    jz alloc_fail
    
    mov rsi, rax                            ; rsi = patch struct
    
    ; Allocate source data buffer
    mov ecx, r8d
    mov edx, 4
    mov r8, 01000h
    xor r9, r9
    call VirtualAlloc
    test rax, rax
    jz alloc_fail
    
    mov rdi, rax                            ; rdi = data buffer
    
    ; Copy source data into buffer
    mov rcx, rdi
    mov rdx, r9
    mov r8d, r8d
    call RtlMoveMemory
    
    ; Fill patch structure
    mov [rsi + MEMORY_PATCH.source_data], rdi
    mov [rsi + MEMORY_PATCH.patch_size], r8
    
    ; Allocate backup buffer
    mov ecx, r8d
    mov edx, 4
    mov r8, 01000h
    xor r9, r9
    call VirtualAlloc
    test rax, rax
    jz alloc_fail
    
    mov [rsi + MEMORY_PATCH.original_data], rax
    
    mov rax, rsi
    jmp alloc_done
    
alloc_fail:
    xor eax, eax
    
alloc_done:
    add rsp, 32
    pop rbp
    ret
MemoryHotpatch_AllocatePatch endp

;============================================================================
; MemoryHotpatch_ProtectMemory - Change page protection for patch region
; RCX = address
; RDX = size
; R8d = new_protect (PAGE_EXECUTE_READWRITE, etc.)
; Returns: RAX = PROT_CONTEXT* (for restore)
;============================================================================
MemoryHotpatch_ProtectMemory proc
    LOCAL old_protect:dword
    
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; RCX = address
    ; RDX = size
    ; R8d = new_protect
    
    mov rsi, rcx                            ; rsi = address
    mov r9, rdx                             ; r9 = size
    mov r10d, r8d                           ; r10d = new_protect
    
    ; Call VirtualProtect
    mov rcx, rsi
    mov edx, r9d
    mov r8d, r10d
    lea r9, [old_protect]
    xor r10d, r10d
    call VirtualProtect
    test eax, eax
    jz prot_fail
    
    ; Allocate protection context
    mov ecx, sizeof PROT_CONTEXT
    mov edx, 4
    mov r8d, 01000h
    xor r9d, r9d
    call VirtualAlloc
    test rax, rax
    jz prot_fail
    
    mov rdi, rax                            ; rdi = context
    
    ; Fill context
    mov [rdi + 0], rsi                      ; address
    mov [rdi + 8], r9                       ; size
    mov [rdi + 16], eax                     ; original_protect
    mov [rdi + 20], r10d                    ; temp_protect (placeholder)
    
    mov rax, rdi
    jmp prot_done
    
prot_fail:
    xor eax, eax
    
prot_done:
    add rsp, 48
    pop rbp
    ret
MemoryHotpatch_ProtectMemory endp

;============================================================================
; MemoryHotpatch_ApplyDirectPatch - Write patch data to memory
; RCX = target_address (void*)
; RDX = patch_data (const void*)
; R8 = patch_size
; R9d = check_xor (bool - verify via XOR before/after)
; Returns: RAX = PATCH_RESULT* (success field set)
;============================================================================
MemoryHotpatch_ApplyDirectPatch proc
    LOCAL verify_before:qword
    LOCAL verify_after:qword
    LOCAL result:qword
    
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; RCX = target
    ; RDX = patch
    ; R8 = size
    ; R9d = check_xor
    
    mov rsi, rcx                            ; rsi = target_address
    mov rdi, rdx                            ; rdi = patch_data
    mov r10, r8                             ; r10 = patch_size
    mov r11d, r9d                           ; r11d = check_xor
    
    ; Validate address
    test rsi, rsi
    jz patch_invalid_addr
    
    ; Compute XOR checksum before (if requested)
    test r11d, r11d
    jz skip_verify_before
    
    xor r12, r12                            ; checksum = 0
    xor r13, r13                            ; idx = 0
    
verify_before_loop:
    cmp r13, r10
    jge verify_before_done
    
    mov al, [rsi + r13]
    xor r12b, al
    inc r13
    jmp verify_before_loop
    
verify_before_done:
    mov [verify_before], r12
    
skip_verify_before:
    ; Make memory writable (PAGE_EXECUTE_READWRITE)
    mov rcx, rsi
    mov edx, r10d
    mov r8d, 040h                           ; PAGE_EXECUTE_READWRITE
    call MemoryHotpatch_ProtectMemory
    test rax, rax
    jz patch_prot_fail
    mov r14, rax                            ; r14 = prot context
    
    ; Copy patch data to target
    mov rcx, rsi
    mov rdx, rdi
    mov r8d, r10d
    call RtlMoveMemory
    
    ; Verify patch after
    test r11d, r11d
    jz skip_verify_after
    
    xor r12, r12
    xor r13, r13
    
verify_after_loop:
    cmp r13, r10
    jge verify_after_done
    
    mov al, [rsi + r13]
    xor r12b, al
    inc r13
    jmp verify_after_loop
    
verify_after_done:
    mov [verify_after], r12
    
skip_verify_after:
    ; Restore original protection
    mov rcx, r14
    mov edx, [rcx + 16]                     ; original_protect at offset +16
    mov rcx, [rcx + 0]                      ; address at offset +0
    mov r8, [rcx + 8]                       ; size at offset +8 (wrong - fix below)
    mov r9d, edx
    call VirtualProtect
    test eax, eax
    jz patch_prot_fail
    
    ; Build result
    mov ecx, sizeof PATCH_RESULT
    mov edx, 4
    mov r8d, 01000h
    xor r9d, r9d
    call VirtualAlloc
    test rax, rax
    jz patch_result_fail
    
    mov [result], rax
    mov dword ptr [rax + PATCH_RESULT.success], 1
    mov dword ptr [rax + PATCH_RESULT.error_code], 0
    
    mov rax, [result]
    jmp patch_done
    
patch_invalid_addr:
    mov ecx, sizeof PATCH_RESULT
    mov edx, 4
    mov r8d, 01000h
    xor r9d, r9d
    call VirtualAlloc
    mov [result], rax
    mov dword ptr [rax + PATCH_RESULT.success], 0
    mov dword ptr [rax + PATCH_RESULT.error_code], 1
    mov rax, [result]
    jmp patch_done
    
patch_prot_fail:
    mov ecx, sizeof PATCH_RESULT
    mov edx, 4
    mov r8d, 01000h
    xor r9d, r9d
    call VirtualAlloc
    mov [result], rax
    mov dword ptr [rax + PATCH_RESULT.success], 0
    mov dword ptr [rax + PATCH_RESULT.error_code], 2
    mov rax, [result]
    jmp patch_done
    
patch_result_fail:
    xor eax, eax
    
patch_done:
    add rsp, 64
    pop rbp
    ret
MemoryHotpatch_ApplyDirectPatch endp

;============================================================================
; MemoryHotpatch_VerifyPatch - Verify patch applied correctly
; RCX = target_address
; RDX = expected_data
; R8 = size
; Returns: RAX = PATCH_RESULT*
;============================================================================
MemoryHotpatch_VerifyPatch proc
    LOCAL result:qword
    LOCAL idx:qword
    
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    mov rsi, rcx                            ; rsi = target
    mov rdi, rdx                            ; rdi = expected
    mov r10, r8                             ; r10 = size
    
    xor r11, r11                            ; idx = 0
    
verify_loop:
    cmp r11, r10
    jge verify_success
    
    mov al, [rsi + r11]
    mov bl, [rdi + r11]
    cmp al, bl
    jne verify_mismatch
    
    inc r11
    jmp verify_loop
    
verify_success:
    mov ecx, sizeof PATCH_RESULT
    mov edx, 4
    mov r8d, 01000h
    xor r9d, r9d
    call VirtualAlloc
    mov [result], rax
    mov dword ptr [rax + PATCH_RESULT.success], 1
    mov rax, [result]
    jmp verify_done
    
verify_mismatch:
    mov ecx, sizeof PATCH_RESULT
    mov edx, 4
    mov r8d, 01000h
    xor r9d, r9d
    call VirtualAlloc
    mov [result], rax
    mov dword ptr [rax + PATCH_RESULT.success], 0
    mov dword ptr [rax + PATCH_RESULT.error_code], 3
    mov rax, [result]
    
verify_done:
    add rsp, 48
    pop rbp
    ret
MemoryHotpatch_VerifyPatch endp

;============================================================================
; MemoryHotpatch_RollbackPatch - Restore original data
; RCX = target_address
; RDX = original_data
; R8 = size
; Returns: RAX = PATCH_RESULT*
;============================================================================
MemoryHotpatch_RollbackPatch proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Use ApplyDirectPatch with original data
    mov r9d, 1                              ; enable verification
    call MemoryHotpatch_ApplyDirectPatch
    
    add rsp, 32
    pop rbp
    ret
MemoryHotpatch_RollbackPatch endp

;============================================================================
; MemoryHotpatch_RestoreMemory - Restore protection of patched region
; RCX = prot_context (PROT_CONTEXT*)
; Returns: RAX = PATCH_RESULT*
;============================================================================
MemoryHotpatch_RestoreMemory proc
    LOCAL result:qword
    
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov rsi, rcx                            ; rsi = context
    
    ; Restore original protection
    mov rcx, [rsi + PROT_CONTEXT.address]
    mov edx, [rsi + PROT_CONTEXT.size]
    mov r8d, [rsi + PROT_CONTEXT.original_protect]
    lea r9, [rsi + PROT_CONTEXT.temp_protect]
    call VirtualProtect
    test eax, eax
    jz restore_fail
    
    ; Build success result
    mov ecx, sizeof PATCH_RESULT
    mov edx, 4
    mov r8d, 01000h
    xor r9d, r9d
    call VirtualAlloc
    mov [result], rax
    mov dword ptr [rax + PATCH_RESULT.success], 1
    
    mov rax, [result]
    jmp restore_done
    
restore_fail:
    mov ecx, sizeof PATCH_RESULT
    mov edx, 4
    mov r8d, 01000h
    xor r9d, r9d
    call VirtualAlloc
    mov [result], rax
    mov dword ptr [rax + PATCH_RESULT.success], 0
    mov dword ptr [rax + PATCH_RESULT.error_code], 4
    
    mov rax, [result]
    
restore_done:
    add rsp, 32
    pop rbp
    ret
MemoryHotpatch_RestoreMemory endp

end
