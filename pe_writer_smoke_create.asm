OPTION CASEMAP:NONE
EXTERN PEWriter_CreateExecutable:PROC
EXTERN ExitProcess:PROC
.code
main PROC
    sub rsp,28h
    mov rcx,140000000h
    mov rdx,1000h
    call PEWriter_CreateExecutable
    test rax,rax
    jz fail
    xor ecx,ecx
    call ExitProcess
fail:
    mov ecx,1
    call ExitProcess
main ENDP
END
