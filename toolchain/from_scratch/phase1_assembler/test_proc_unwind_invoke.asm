EXTERN ExitProcess:PROC
EXTERN MessageBoxA:PROC

.code
DemoProc PROC
    .pushreg rbp
    .allocstack 20h
    .setframe rbp, 20h
    .endprolog
    invoke MessageBoxA, 0, 0, 0, 0
    xor ecx, ecx
    call ExitProcess
DemoProc ENDP
END
