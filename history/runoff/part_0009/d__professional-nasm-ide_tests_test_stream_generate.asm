; test_stream_generate.asm - Basic streaming generation test
BITS 64
DEFAULT REL

extern ollama_init
extern ollama_generate
extern ollama_close
extern strlen

struc OllamaRequest
    .model_ptr:         resq 1
    .model_len:         resq 1
    .prompt_ptr:        resq 1
    .prompt_len:        resq 1
    .system_ptr:        resq 1
    .system_len:        resq 1
    .context_ptr:       resq 1
    .context_len:       resq 1
    .messages_ptr:      resq 1
    .messages_count:    resq 1
    .temperature:       resd 1
    .top_p:             resd 1
    .top_k:             resd 1
    .num_predict:       resd 1
    .stream:            resb 1
    .raw:               resb 1
    .use_chat_api:      resb 1
    .padding:           resb 1
    .size:
endstruc

struc OllamaResponse
    .response_ptr:      resq 1
    .response_len:      resq 1
    .response_capacity: resq 1
    .context_ptr:       resq 1
    .context_len:       resq 1
    .done:              resb 1
    .padding:           resb 7
    .total_duration:    resq 1
    .load_duration:     resq 1
    .prompt_eval_count: resq 1
    .prompt_eval_dur:   resq 1
    .eval_count:        resq 1
    .eval_duration:     resq 1
    .size:
endstruc

global _start

section .bss
req:            resb OllamaRequest.size
resp:           resb OllamaResponse.size
resp_buf:       resb 8192

section .rodata
model_name:     db "llama2",0
prompt_text:    db "Hello from streaming test",0
msg_header:     db "Streaming output:",10,0
msg_fail:       db "Generation failed",10,0

section .text
_start:
    xor rdi, rdi
    xor rsi, rsi
    call ollama_init
    test rax, rax
    jz .exit

    ; Prepare request
    lea rbx, [req]
    lea rax, [model_name]
    mov [rbx + OllamaRequest.model_ptr], rax
    mov qword [rbx + OllamaRequest.model_len], 6
    lea rax, [prompt_text]
    mov [rbx + OllamaRequest.prompt_ptr], rax
    mov qword [rbx + OllamaRequest.prompt_len], 26
    mov dword [rbx + OllamaRequest.temperature], 700
    mov dword [rbx + OllamaRequest.top_p], 900
    mov dword [rbx + OllamaRequest.top_k], 40
    mov dword [rbx + OllamaRequest.num_predict], 64
    mov byte [rbx + OllamaRequest.stream], 1

    ; Prepare response
    lea rcx, [resp]
    lea rax, [resp_buf]
    mov [rcx + OllamaResponse.response_ptr], rax
    mov qword [rcx + OllamaResponse.response_capacity], 8192
    mov byte [rcx + OllamaResponse.done], 0

    ; Call generate
    mov rdi, rbx
    mov rsi, rcx
    call ollama_generate
    test rax, rax
    jz .gen_fail

    ; Print header
    lea rdi, [msg_header]
    call write_str
    ; Print response buffer
    mov rdi, [rcx + OllamaResponse.response_ptr]
    call write_str
    jmp .cleanup

.gen_fail:
    lea rdi, [msg_fail]
    call write_str

.cleanup:
    call ollama_close

.exit:
    mov rax, 60
    xor rdi, rdi
    syscall

write_str:
    push rdi
    call strlen
    mov rdx, rax
    pop rsi
    mov rdi, 1
    mov rax, 1
    syscall
    ret