; =========================
; FILE: rawr_rt.inc
; =========================
option casemap:none

RAWR_SHADOWSPACE        equ 20h

NULL                    equ 0
TRUE                    equ 1
FALSE                   equ 0

; No custom types, use native ML64 keywords (qword, dword, etc.)

RAWR_PROLOGUE macro locals_bytes:req
    sub     rsp, (RAWR_SHADOWSPACE + locals_bytes + 8)
endm

RAWR_EPILOGUE macro locals_bytes:req
    add     rsp, (RAWR_SHADOWSPACE + locals_bytes + 8)
    ret
endm

RAWR_SAVE_NONVOL macro
    push    rbx
    push    rbp
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
endm

RAWR_RESTORE_NONVOL macro
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
endm
