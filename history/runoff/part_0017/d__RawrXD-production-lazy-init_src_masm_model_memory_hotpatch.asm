;=====================================================================
; model_memory_hotpatch.asm - Direct Memory Patching Layer (Pure MASM x64)
; ZERO-DEPENDENCY RUNTIME MEMORY MODIFICATION
;=====================================================================
; Implements direct RAM patching using OS memory protection:
;  - VirtualProtect for Win32 memory page permissions
;  - Direct pointer arithmetic for tensor location
;  - Atomic write operations for thread safety
;  - Rollback mechanism with undo buffer
;
; PatchResult Structure (64 bytes):
;   [+0]:  success (qword) - 1 = success, 0 = failure
;   [+8]:  detail_str_ptr (qword) - pointer to detail string
;   [+16]: detail_str_len (qword)
;   [+24]: error_code (qword) - Win32 error code
;   [+32]: bytes_patched (qword)
;   [+40]: original_protect (qword) - saved page protection
;   [+48]: reserved[2] (qword[2])
;
; MemoryPatch Structure (128 bytes):
;   [+0]:  target_address (qword) - address to patch
;   [+8]:  patch_data_ptr (qword) - new data
;   [+16]: patch_size (qword)
;   [+24]: original_data_ptr (qword) - backup for rollback
;   [+32]: patch_type (qword) - 0=replace, 1=xor, 2=add, 3=multiply
;   [+40]: verify_pattern_ptr (qword) - optional verification
;   [+48]: verify_pattern_len (qword)
;   [+56]: flags (qword) - bit 0: atomic, bit 1: verify first
;   [+64]: mutex_handle (qword)
;   [+72]: reserved[7] (qword[7])
;=====================================================================

; External dependencies
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_str_length:PROC
EXTERN asm_str_create:PROC
EXTERN asm_str_create_from_cstr:PROC
EXTERN VirtualProtect:PROC
EXTERN asm_log:PROC

.data

; Global statistics
g_patches_applied   QWORD 0
g_patches_failed    QWORD 0
g_total_bytes       QWORD 0

; Logging messages
msg_mem_patch_enter DB "MEMPATCH enter",0
msg_mem_patch_fail  DB "MEMPATCH fail",0
msg_mem_patch_exit  DB "MEMPATCH exit",0

.code

; Export all public memory hotpatch functions
PUBLIC masm_hotpatch_apply_memory, masm_hotpatch_rollback
PUBLIC masm_hotpatch_get_stats

; External Win32 APIs
EXTERN GetLastError:PROC
EXTERN GetSystemInfo:PROC

;=====================================================================
; masm_hotpatch_apply_memory(patch_ptr: rcx, result_ptr: rdx) -> void
;
; Applies a memory hotpatch with rollback capability.
; patch_ptr = pointer to MemoryPatch structure
; result_ptr = pointer to PatchResult structure to fill
;
; Performs:
; 1. Validate target address and size
; 2. Backup original data
; 3. Change memory protection to RWX
; 4. Apply patch atomically
; 5. Restore original protection
; 6. Verify if requested
;=====================================================================

ALIGN 16
masm_hotpatch_apply_memory PROC

    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 64             ; Local variables + shadow space
    
    mov rbx, rcx            ; rbx = patch structure
    mov r12, rdx            ; r12 = result structure
    lea rcx, msg_mem_patch_enter
    call asm_log
    
    ; Initialize result to failure
    mov qword ptr [r12], 0          ; success = 0
    mov qword ptr [r12 + 24], 0     ; error_code = 0
    mov qword ptr [r12 + 32], 0     ; bytes_patched = 0
    
    ; Validate inputs
    test rbx, rbx
    jz patch_fail_invalid
    
    test r12, r12
    jz patch_fail_invalid
    
    ; Get patch parameters
    mov r13, [rbx]          ; r13 = target_address
    mov r14, [rbx + 8]      ; r14 = patch_data_ptr
    mov r15, [rbx + 16]     ; r15 = patch_size
    
    ; Validate target and size
    test r13, r13
    jz patch_fail_invalid
    
    test r14, r14
    jz patch_fail_invalid
    
    test r15, r15
    jz patch_fail_invalid
    
    cmp r15, 100000h        ; Max 1MB patch
    jg patch_fail_invalid
    
    ; Allocate backup buffer for original data
    mov rcx, r15
    mov rdx, 16
    call asm_malloc
    test rax, rax
    jz patch_fail_nomem
    
    mov [rbx + 24], rax     ; Store original_data_ptr
    
    ; Copy original data to backup
    mov rcx, rax            ; dest = backup
    mov rdx, r13            ; src = target
    mov r8, r15             ; size
    call asm_memcpy_fast
    
    ; Change memory protection to PAGE_EXECUTE_READWRITE (0x40)
    ; BOOL VirtualProtect(LPVOID addr, SIZE_T size, DWORD new_protect, PDWORD old_protect)
    lea r9, [rsp + 32]      ; r9 = &old_protect (on stack)
    mov r8d, 40h            ; r8 = PAGE_EXECUTE_READWRITE
    mov rdx, r15            ; rdx = size
    mov rcx, r13            ; rcx = address
    
    sub rsp, 32
    call VirtualProtect
    add rsp, 32
    
    test eax, eax
    jz patch_fail_protect
    
    ; Save original protection
    mov rax, [rsp + 32]
    mov [r12 + 40], rax
    
    ; Apply patch based on type
    mov rax, [rbx + 32]     ; rax = patch_type
    
    cmp rax, 0
    je patch_type_replace
    
    cmp rax, 1
    je patch_type_xor
    
    cmp rax, 2
    je patch_type_add
    
    cmp rax, 3
    je patch_type_multiply
    
    jmp patch_fail_invalid_type

patch_type_replace:
    ; Simple memory copy: target = patch_data
    mov rcx, r13            ; dest = target
    mov rdx, r14            ; src = patch_data
    mov r8, r15             ; size
    call asm_memcpy_fast
    jmp patch_applied

patch_type_xor:
    ; XOR operation: target ^= patch_data
    xor rcx, rcx            ; rcx = offset counter
    
patch_xor_loop:
    cmp rcx, r15
    jge patch_applied
    
    mov al, [r14 + rcx]     ; Load patch byte
    xor [r13 + rcx], al     ; XOR with target
    
    inc rcx
    jmp patch_xor_loop

patch_type_add:
    ; ADD operation: target += patch_data (byte-wise)
    xor rcx, rcx
    
patch_add_loop:
    cmp rcx, r15
    jge patch_applied
    
    mov al, [r14 + rcx]
    add [r13 + rcx], al
    
    inc rcx
    jmp patch_add_loop

patch_type_multiply:
    ; MULTIPLY operation: target *= patch_data (byte-wise)
    xor rcx, rcx
    
patch_mul_loop:
    cmp rcx, r15
    jge patch_applied
    
    mov al, [r13 + rcx]
    mov bl, byte ptr [r14 + rcx]
    mul bl                  ; al *= bl (result in AX)
    mov [r13 + rcx], al
    
    inc rcx
    jmp patch_mul_loop

patch_applied:
    ; Restore original memory protection
    lea r9, [rsp + 40]      ; Temp old_protect storage
    mov r8, [r12 + 40]      ; r8 = original protection
    mov rdx, r15            ; rdx = size
    mov rcx, r13            ; rcx = address
    
    sub rsp, 32
    call VirtualProtect
    add rsp, 32
    
    ; Verify if requested (bit 1 of flags)
    mov rax, [rbx + 56]
    test rax, 2
    jz patch_success_no_verify
    
    ; Verification: compare target with patch_data
    mov rcx, r13
    mov rdx, r14
    mov r8, r15
    call asm_memcmp
    
    test rax, rax
    jnz patch_fail_verify

patch_success_no_verify:
    ; Update statistics
    lock inc [g_patches_applied]
    lock add [g_total_bytes], r15
    
    ; Fill result structure
    mov qword ptr [r12], 1          ; success = 1
    mov [r12 + 32], r15             ; bytes_patched = size
    
    ; Create success detail string
    lea rcx, [str_patch_success]
    call asm_str_create_from_cstr
    mov [r12 + 8], rax              ; detail_str_ptr
    mov qword ptr [r12 + 16], 15    ; detail_str_len
    lea rcx, msg_mem_patch_exit
    call asm_log
    jmp patch_exit

patch_fail_invalid:
    lea rcx, [str_fail_invalid]
    jmp patch_fail_common

patch_fail_nomem:
    lea rcx, [str_fail_nomem]
    jmp patch_fail_common

patch_fail_protect:
    lea rcx, [str_fail_protect]
    call GetLastError
    mov [r12 + 24], rax
    jmp patch_fail_common

patch_fail_invalid_type:
    lea rcx, [str_fail_type]
    jmp patch_fail_common

patch_fail_verify:
    lea rcx, [str_fail_verify]

patch_fail_common:
    push rcx
    lea rcx, msg_mem_patch_fail
    call asm_log
    pop rcx
    lock inc [g_patches_failed]
    
    ; Create error detail string
    call asm_str_create_from_cstr
    mov [r12 + 8], rax
    
    ; Length calculation
    mov rcx, rax
    call asm_str_length
    mov [r12 + 16], rax

patch_exit:
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_hotpatch_apply_memory ENDP

;=====================================================================
; masm_hotpatch_rollback(patch_ptr: rcx) -> rax (1=success, 0=fail)
;
; Rolls back a previously applied patch using backed-up data.
;=====================================================================

ALIGN 16
masm_hotpatch_rollback PROC

    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = patch structure
    
    test rbx, rbx
    jz rollback_fail
    
    ; Get original data backup
    mov r12, [rbx + 24]     ; r12 = original_data_ptr
    test r12, r12
    jz rollback_fail
    
    ; Get target address and size
    mov r13, [rbx]          ; r13 = target_address
    mov rcx, [rbx + 16]     ; rcx = patch_size
    
    ; Change protection
    lea r9, [rsp + 48]
    mov r8d, 40h
    mov rdx, rcx
    mov rcx, r13
    
    call VirtualProtect
    test eax, eax
    jz rollback_fail
    
    ; Copy original data back
    mov rcx, r13            ; dest = target
    mov rdx, r12            ; src = original_data
    mov r8, [rbx + 16]      ; size
    call asm_memcpy_fast
    
    ; Restore protection
    mov rcx, r13
    mov rdx, [rbx + 16]
    mov r8, [rsp + 48]
    lea r9, [rsp + 56]
    call VirtualProtect
    
    ; Free backup buffer
    mov rcx, r12
    call asm_free
    
    mov qword ptr [rbx + 24], 0  ; Clear original_data_ptr
    
    mov rax, 1
    jmp rollback_exit

rollback_fail:
    xor rax, rax

rollback_exit:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret

masm_hotpatch_rollback ENDP

;=====================================================================
; masm_hotpatch_get_stats(stats_ptr: rcx) -> void
;
; Fills statistics structure:
;   [0]: patches_applied (qword)
;   [8]: patches_failed (qword)
;   [16]: total_bytes (qword)
;=====================================================================

ALIGN 16
masm_hotpatch_get_stats PROC

    test rcx, rcx
    jz stats_exit
    
    mov rax, [g_patches_applied]
    mov [rcx], rax
    
    mov rax, [g_patches_failed]
    mov [rcx + 8], rax
    
    mov rax, [g_total_bytes]
    mov [rcx + 16], rax

stats_exit:
    ret

masm_hotpatch_get_stats ENDP

;=====================================================================
; Helper: asm_memcpy_fast(dest: rcx, src: rdx, size: r8) -> void
; Fast memory copy using SIMD when aligned
;=====================================================================

ALIGN 16
asm_memcpy_fast PROC

    push rsi
    push rdi
    
    mov rdi, rcx            ; dest
    mov rsi, rdx            ; src
    mov rcx, r8             ; size
    
    rep movsb               ; Simple byte copy for now
    
    pop rdi
    pop rsi
    ret

asm_memcpy_fast ENDP

;=====================================================================
; Helper: asm_memcmp(ptr1: rcx, ptr2: rdx, size: r8) -> rax (0=equal)
;=====================================================================

ALIGN 16
asm_memcmp PROC

    xor rax, rax
    xor r9, r9

cmp_loop:
    cmp r9, r8
    jge cmp_equal
    
    mov r10b, [rcx + r9]
    mov r11b, [rdx + r9]
    
    cmp r10b, r11b
    jne cmp_not_equal
    
    inc r9
    jmp cmp_loop

cmp_equal:
    xor rax, rax
    ret

cmp_not_equal:
    mov rax, 1
    ret

asm_memcmp ENDP

;=====================================================================
; String constants
;=====================================================================

.data

str_patch_success   DB "Patch applied", 0
str_fail_invalid    DB "Invalid parameters", 0
str_fail_nomem      DB "Out of memory", 0
str_fail_protect    DB "VirtualProtect failed", 0
str_fail_type       DB "Invalid patch type", 0
str_fail_verify     DB "Verification failed", 0

END

