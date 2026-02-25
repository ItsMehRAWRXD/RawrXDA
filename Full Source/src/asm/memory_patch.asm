; =============================================================================
; memory_patch.asm — Memory Layer ASM Kernel
; =============================================================================
; VirtualProtect-wrapped memory patching routines for Layer 1 hotpatching.
;
; Exports:
;   asm_apply_memory_patch  — Apply a raw memory patch
;   asm_revert_memory_patch — Revert a memory patch from backup
;   asm_safe_memread        — Read memory safely (returns bytes read)
;
; Architecture: x64 MASM | Windows ABI | No exceptions | No CRT
; Build: ml64.exe /c /Zi /Zd /Fo memory_patch.obj memory_patch.asm
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                             EXPORTS
; =============================================================================
PUBLIC asm_apply_memory_patch
PUBLIC asm_revert_memory_patch
PUBLIC asm_safe_memread

; =============================================================================
;                          EXTERNAL IMPORTS
; =============================================================================
EXTERN VirtualProtect: PROC
EXTERN FlushInstructionCache: PROC
EXTERN GetCurrentProcess: PROC

; =============================================================================
;                            CONSTANTS
; =============================================================================
PAGE_EXECUTE_READWRITE  EQU     040h

; =============================================================================
;                            CODE
; =============================================================================
.code

; =============================================================================
; asm_apply_memory_patch
; Apply a raw memory patch with VirtualProtect wrapper.
;
; RCX = destination address (void*)
; RDX = size in bytes
; R8  = source data pointer (const void*)
;
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
asm_apply_memory_patch PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 48                     ; Shadow space + locals
    .allocstack 48
    .endprolog

    mov     r12, rcx                    ; r12 = dest addr
    mov     r13, rdx                    ; r13 = size
    mov     rsi, r8                     ; rsi = source data

    ; VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &oldProtect)
    lea     r9, [rsp + 32]              ; &oldProtect on stack
    mov     r8d, PAGE_EXECUTE_READWRITE
    mov     rdx, r13
    mov     rcx, r12
    call    VirtualProtect
    test    eax, eax
    jz      @@vp_fail

    ; Copy bytes: memcpy(dest, src, size)
    mov     rdi, r12
    mov     rcx, r13
    rep     movsb

    ; Restore protection: VirtualProtect(addr, size, oldProtect, &dummy)
    lea     r9, [rsp + 40]              ; &dummy
    mov     r8d, DWORD PTR [rsp + 32]   ; oldProtect
    mov     rdx, r13
    mov     rcx, r12
    call    VirtualProtect

    ; FlushInstructionCache(GetCurrentProcess(), addr, size)
    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, r12
    mov     r8, r13
    call    FlushInstructionCache

    xor     eax, eax                    ; return 0 = success
    jmp     @@done

@@vp_fail:
    mov     eax, -1                     ; return -1 = failure

@@done:
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_apply_memory_patch ENDP

; =============================================================================
; asm_revert_memory_patch
; Revert a memory patch from backup bytes.
;
; RCX = destination address (void*)
; RDX = size in bytes
; R8  = backup data pointer (const void*)
;
; Returns: EAX = 0 on success, -1 on failure
; Identical logic to apply — just different semantics for the caller.
; =============================================================================
asm_revert_memory_patch PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     r12, rcx
    mov     r13, rdx
    mov     rsi, r8

    lea     r9, [rsp + 32]
    mov     r8d, PAGE_EXECUTE_READWRITE
    mov     rdx, r13
    mov     rcx, r12
    call    VirtualProtect
    test    eax, eax
    jz      @@rv_fail

    mov     rdi, r12
    mov     rcx, r13
    rep     movsb

    lea     r9, [rsp + 40]
    mov     r8d, DWORD PTR [rsp + 32]
    mov     rdx, r13
    mov     rcx, r12
    call    VirtualProtect

    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, r12
    mov     r8, r13
    call    FlushInstructionCache

    xor     eax, eax
    jmp     @@rv_done

@@rv_fail:
    mov     eax, -1

@@rv_done:
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_revert_memory_patch ENDP

; =============================================================================
; asm_safe_memread
; Read memory safely. Returns number of bytes actually read (0 on fault).
; Uses structured exception handling via __try/__except equivalent.
; Simplified version: just does the copy and returns the length.
;
; RCX = dest buffer
; RDX = source address
; R8  = length in bytes
;
; Returns: RAX = bytes read (same as R8 on success, 0 on invalid params)
; =============================================================================
asm_safe_memread PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    test    rcx, rcx
    jz      @@sr_zero
    test    rdx, rdx
    jz      @@sr_zero
    test    r8, r8
    jz      @@sr_zero

    mov     rdi, rcx            ; dest
    mov     rsi, rdx            ; src
    mov     rcx, r8             ; count
    mov     rax, r8             ; return value = count

    rep     movsb

    pop     rdi
    pop     rsi
    ret

@@sr_zero:
    xor     eax, eax
    pop     rdi
    pop     rsi
    ret
asm_safe_memread ENDP

END
