;======================================================================
; main.asm - Minimal x64 console entry to validate toolchain
;======================================================================

EXTERN ExitProcess:PROC

.CODE

main PROC
    xor ecx, ecx                ; exit code 0
    call ExitProcess
    ret
main ENDP

END
