; RCE_Engine_test.asm - Extract key instructions to test
; Test various instruction patterns from the RCE Engine

push rbx
push rsi
push rdi
push r12
push r13

mov rdi, rcx
mov rsi, rdx
mov r12, r8
mov r13, r9

xor eax, eax
mov rcx, 4

add rsi, 48
sub r12, 48

pop r13
pop r12
pop rdi
pop rsi
pop rbx
ret
