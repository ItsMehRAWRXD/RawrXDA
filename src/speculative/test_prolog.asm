code
TestProc PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 56
    .allocstack 56
    .endprolog
    mov rsp, rbp
    pop rbp
    ret
TestProc ENDP
END
