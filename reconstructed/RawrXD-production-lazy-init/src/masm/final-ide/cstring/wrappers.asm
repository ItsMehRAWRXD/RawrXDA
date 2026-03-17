; ==========================================================================
; cstring_wrappers.asm - Minimal C runtime string/memory helpers (Pure MASM)
; ==========================================================================
; Provides standard symbol names expected by some MASM modules:
;   - memcpy
;   - memset
;   - strlen
; All are implemented without CRT dependencies.
;
; Win64 calling convention:
;   memcpy(dest=rcx, src=rdx, size=r8) -> rax=dest
;   memset(dest=rcx, value=edx, size=r8) -> rax=dest
;   strlen(str=rcx) -> rax=len
; ==========================================================================

option casemap:none

.code

PUBLIC memcpy
memcpy PROC
    mov rax, rcx
    test r8, r8
    jz memcpy_done

    mov r9, r8
    cld
    rep movsb

memcpy_done:
    ret
memcpy ENDP

PUBLIC memset
memset PROC
    mov rax, rcx
    test r8, r8
    jz memset_done

    mov r9, r8
    mov al, dl
    cld
    rep stosb

memset_done:
    ret
memset ENDP

PUBLIC strlen
strlen PROC
    xor rax, rax
    test rcx, rcx
    jz strlen_done

strlen_loop:
    cmp byte ptr [rcx + rax], 0
    je strlen_done
    inc rax
    jmp strlen_loop

strlen_done:
    ret
strlen ENDP

END
