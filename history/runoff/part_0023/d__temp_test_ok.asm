; test_ok.asm - Minimal boot-ready for solo compiler
push rax
push rbx
mov rax, 0x1234
pop rbx
pop rax
nop
ret