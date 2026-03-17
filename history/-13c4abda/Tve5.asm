.code
PUBLIC AsmGetTicks

AsmGetTicks PROC
    rdtsc
    shl rdx, 32
    or rax, rdx
    ret
AsmGetTicks ENDP

END
