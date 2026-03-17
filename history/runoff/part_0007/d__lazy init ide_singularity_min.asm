option casemap:none

EXTERN ExitProcess:PROC

.code
_start PROC
    ; Minimal self-sovereign entry using PEB access example
    ; Read PEB pointer (no external DLL calls)
    mov     rax, gs:[60h]
    ; Align stack and exit cleanly
    sub     rsp, 28h
    xor     ecx, ecx
    call    ExitProcess
_start ENDP

END
