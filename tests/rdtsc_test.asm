; Minimal rdtsc test
option casemap:none
extern ExitProcess : proc
.code
main proc
    rdtsc
    mov     rcx, 0
    call    ExitProcess
main endp
end
