; RawrXD_RDNA3_3X_Virtualizer.asm
; 3X Virtual Memory Expander
; Extends virtual address space 3x

.DATA
VA_MULTIPLIER equ 3

.CODE

; RDNA3_3X_Virtualize PROC
; Maps virtual addresses 3x
RDNA3_3X_Virtualize PROC
    push rbp
    mov rbp, rsp

    ; Virtualize address (multiply by 3)
    mov rax, rcx
    imul rax, VA_MULTIPLIER

    leave
    ret
RDNA3_3X_Virtualize ENDP

END