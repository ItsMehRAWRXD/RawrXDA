; Minimal test with WriteString
extern GetStdHandle : proc
extern WriteFile    : proc
extern ExitProcess  : proc
.data
    msg db "Hello", 13, 10
    len equ $ - msg
.code
WriteString proc
    push    rbx
    push    rdi
    sub     rsp, 48
    mov     rbx, rcx
    mov     rdi, rdx
    mov     rcx, -11
    call    GetStdHandle
    mov     r10, rax
    mov     rcx, r10
    mov     rdx, rbx
    mov     r8, rdi
    lea     r9, [rsp + 40]
    mov     qword ptr [rsp + 32], 0
    call    WriteFile
    add     rsp, 48
    pop     rdi
    pop     rbx
    ret
WriteString endp

main proc
    sub     rsp, 48
    lea     rcx, msg
    mov     rdx, len
    call    WriteString
    xor     rax, rax
    add     rsp, 48
    ret
main endp
end
