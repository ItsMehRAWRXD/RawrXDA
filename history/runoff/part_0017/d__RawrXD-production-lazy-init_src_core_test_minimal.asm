; Test minimal MASM64 program to isolate crash
.code

EXTERN ExitProcess: PROC

main PROC
    sub rsp, 28h
    xor ecx, ecx
    call ExitProcess
main ENDP

END
