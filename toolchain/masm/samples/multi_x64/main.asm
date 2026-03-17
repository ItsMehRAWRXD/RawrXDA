EXTERN ExitProcess : PROC
EXTERN helper : PROC

.CODE
main PROC
    sub rsp, 28h
    call helper
    xor ecx, ecx
    call ExitProcess
main ENDP
END
