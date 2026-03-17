.CODE
Titan_ExecuteComputeKernel PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    .endprolog
    pop rbp
    ret
Titan_ExecuteComputeKernel ENDP

Titan_PerformDMA PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    .endprolog
    mov eax, 0
    pop rbp
    ret
Titan_PerformDMA ENDP
END
