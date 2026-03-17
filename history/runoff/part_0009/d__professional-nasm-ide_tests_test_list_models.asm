; test_list_models.asm - Basic test for ollama_list_models
BITS 64
DEFAULT REL

extern ollama_init
extern ollama_list_models
extern ollama_close
extern strlen

global _start

section .bss
models_buffer: resb 2048

section .text
_start:
    ; Initialize (NULL host, 0 port)
    xor rdi, rdi
    xor rsi, rsi
    call ollama_init
    test rax, rax
    jz .fail

    ; List models (max 50)
    lea rdi, [models_buffer]
    mov rsi, 50
    call ollama_list_models
    mov rbx, rax            ; count

    ; Simple output: model count
    lea rdi, [msg_models]
    call write_str
    mov rdi, rbx
    call write_int
    lea rdi, [newline]
    call write_str

    ; Close
    call ollama_close
    jmp .exit

.fail:
    lea rdi, [msg_fail]
    call write_str

.exit:
    mov rax, 60
    xor rdi, rdi
    syscall

; write_str: RDI=ptr
write_str:
    call strlen
    mov rdx, rax
    mov rsi, rdi        ; buffer pointer
    mov rdi, 1          ; stdout
    mov rax, 1          ; write syscall
    syscall
    ret

; write_int: RDI=value
write_int:
    push rbx
    push rcx
    push rdx
    mov rax, rdi
    mov rbx, 10
    xor rcx, rcx
    cmp rax, 0
    jne .wi_count
    lea rsi, [int_zero]
    jmp .wi_print
.wi_count:
    .wi_loop1:
        xor rdx, rdx
        div rbx
        push rdx
        inc rcx
        test rax, rax
        jnz .wi_loop1
    mov rax, rcx
    lea rsi, [int_buf]
    add rsi, rcx
    mov byte [rsi], 0
    .wi_loop2:
        dec rsi
        pop rdx
        add dl, '0'
        mov [rsi], dl
        dec rcx
        jnz .wi_loop2
.wi_print:
    mov rdi, rsi
    call write_str
    pop rdx
    pop rcx
    pop rbx
    ret

section .data
msg_models: db "Models count: ",0
msg_fail:   db "Initialization failed",10,0
newline:    db 10,0
int_zero:   db "0",0
int_buf:    times 20 db 0