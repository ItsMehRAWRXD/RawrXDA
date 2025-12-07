; RCX = src, RDX = src_len, R8 = dst, R9 = dst_max_len
; returns unpacked len in RAX, 0 on error
PUBLIC  AsmInflate
AsmInflate PROC
    ; your optimised inflate here
    mov     rax, r8          ; fake result – replace
    ret
AsmInflate ENDP

PUBLIC  AsmDeflate
AsmDeflate PROC
    ; your optimised deflate here
    mov     rax, rdx
    ret
AsmDeflate ENDP
END
