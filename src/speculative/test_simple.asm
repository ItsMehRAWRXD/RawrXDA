code
TestSimple PROC
    push rbp
    mov rbp, rsp
    sub rsp, 56
    mov rsp, rbp
    pop rbp
    ret
TestSimple ENDP
END
