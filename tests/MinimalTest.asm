; Minimal test to verify toolchain
extern ExitProcess : proc
.code
main proc
    sub     rsp, 48
    xor     rax, rax
    add     rsp, 48
    ret
main endp
end
