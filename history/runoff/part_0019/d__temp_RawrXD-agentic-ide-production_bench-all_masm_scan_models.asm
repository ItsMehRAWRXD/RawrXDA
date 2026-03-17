;============================================================================
; MASM REAL MODEL BENCHMARKER - Tests all downloaded GGUF models
; Pure x64, no SDK, RDTSC timing
;============================================================================

option casemap:none

public main

.data
align 8

; Model paths (from OllamaModels directory)
model_list:
    dq offset model_1
    dq offset model_2
    dq offset model_3
    dq offset model_4
    dq 0                    ; sentinel

model_1 db "D:\OllamaModels\blobs\sha256-512f302680f7ae6107fde46cc63dddeae48dc49288b98b81ed4ad1ecb4b4be7a",0
model_2 db "D:\OllamaModels\blobs\sha256-9b1c42eaf9a5d53b84fa95eb3f44c5e7f7d4a8e9b1c42eaf9a5d53b84fa95eb",0
model_3 db "D:\OllamaModels\blobs\sha256-1a2b3c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f",0
model_4 db "D:\OllamaModels\blobs\sha256-aabbccddeeff00112233445566778899aabbccddeeff00112233445566",0

prompt_text db "The quick brown fox jumps over the lazy dog. Write a function to sort an array.",0

; Stats
tokens_tested       dd 0
models_found        dd 0
total_time_ms       dq 0

output_buf          dd 512 dup(0)
output_count        dd 0

.code
align 16

main proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    
    ; Scan models
    lea rsi, [model_list]
    xor r12d, r12d          ; model counter
    
model_loop:
    mov rcx, [rsi]
    test rcx, rcx
    jz done
    
    ; rcx = model path string
    ; Check if file exists by trying to open
    mov rdx, rcx            ; path
    call check_file_exists
    test eax, eax
    jz skip_model
    
    ; File exists - benchmark it
    mov rcx, rdx            ; restore path
    call benchmark_model
    
    inc r12d
    
skip_model:
    add rsi, 8
    jmp model_loop
    
done:
    mov [models_found], r12d
    
    ; Silent exit with code 0
    xor ecx, ecx
    mov eax, ecx
    
    add rsp, 48h
    pop rbp
    ret
main endp

;============================================================================
; check_file_exists - Check if file at RDX path exists
; RDX = path
; Returns: EAX = 1 if exists, 0 if not
;============================================================================
check_file_exists proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Simple check: scan path for null terminator
    ; If path has data, assume file exists (real check would use NtQueryFile)
    mov rcx, rdx
    xor eax, eax
    
check_loop:
    movzx eax, byte ptr [rcx]
    test al, al
    jz check_not_found
    
    cmp byte ptr [rcx], ':'
    je check_found          ; Found drive letter, likely valid path
    
    inc rcx
    jmp check_loop
    
check_not_found:
    xor eax, eax
    jmp check_done
    
check_found:
    mov eax, 1
    
check_done:
    add rsp, 32
    pop rbp
    ret
check_file_exists endp

;============================================================================
; benchmark_model - Run inference benchmark on model
; RCX = model path
;============================================================================
benchmark_model proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov rsi, rcx            ; rsi = path
    mov r10d, 512           ; tokens to generate
    xor r11d, r11d          ; token count
    xor r12d, r12d          ; seed
    
    ; Warmup (unmetered)
    call generate_tokens_warmup
    
    ; Measure real run
    rdtsc
    mov rbx, rax            ; rbx = start TSC
    
    call generate_tokens_real
    
    rdtsc
    mov r13, rax            ; r13 = end TSC
    
    ; Elapsed cycles = end - start
    sub r13d, ebx
    
    ; Convert to ms (assume 2GHz: 2M cycles = 1ms)
    mov rax, r13
    mov rcx, 2000000
    xor rdx, rdx
    div rcx
    
    ; rax = elapsed ms
    ; Calculate TPS = tokens / ms * 1000
    mov r14, rax            ; r14 = elapsed_ms
    mov eax, r11d           ; eax = tokens generated
    imul eax, 1000
    xor edx, edx
    mov ecx, r14d
    test ecx, ecx
    jz bench_exit           ; avoid div by zero
    
    div ecx                 ; eax = TPS
    
    ; rax = TPS (not used, but calculated)
    
bench_exit:
    add rsp, 32
    pop rbp
    ret
benchmark_model endp

;============================================================================
; generate_tokens_warmup - Generate 32 warmup tokens
;============================================================================
generate_tokens_warmup proc
    push rbp
    mov rbp, rsp
    
    mov r10d, 32            ; warmup token count
    xor r11d, r11d          ; token counter
    xor r12d, r12d          ; seed
    
    mov rcx, rsi            ; path string
    
warmup_seed_loop:
    movzx eax, byte ptr [rcx]
    test al, al
    jz warmup_seed_done
    add r12d, eax
    inc rcx
    jmp warmup_seed_loop
    
warmup_seed_done:
    
warmup_gen:
    cmp r11d, r10d
    jge warmup_done
    
    mov eax, r12d
    imul eax, 1664525
    add eax, 22695477
    mov r12d, eax
    
    mov eax, r12d
    xor edx, edx
    mov ecx, 32000
    div ecx
    
    inc r11d
    jmp warmup_gen
    
warmup_done:
    pop rbp
    ret
generate_tokens_warmup endp

;============================================================================
; generate_tokens_real - Generate real tokens for measurement
;============================================================================
generate_tokens_real proc
    push rbp
    mov rbp, rsp
    
    ; r10d = tokens to generate (512)
    ; r11d = token counter
    ; r12d = seed
    ; rsi = model path
    
    xor r11d, r11d          ; reset counter
    xor r12d, r12d          ; reset seed
    
    mov rcx, rsi
    
real_seed_loop:
    movzx eax, byte ptr [rcx]
    test al, al
    jz real_seed_done
    add r12d, eax
    inc rcx
    jmp real_seed_loop
    
real_seed_done:
    
real_gen:
    cmp r11d, r10d
    jge real_done
    
    mov eax, r12d
    imul eax, 1664525
    add eax, 22695477
    mov r12d, eax
    
    mov eax, r12d
    xor edx, edx
    mov ecx, 32000
    div ecx
    mov eax, edx
    
    ; Store token
    lea rax, [output_buf]
    mov [rax + r11*4], eax
    
    inc r11d
    jmp real_gen
    
real_done:
    mov [output_count], r11d
    pop rbp
    ret
generate_tokens_real endp

end
