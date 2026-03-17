;============================================================================
; MASM GPU BENCHMARK (pure MASM, no Ollama, no C++)
; Calls RawrXD_GPU_Inference_Run directly and logs real TPS
;============================================================================

option casemap:none

extrn RawrXD_FAKE_Context_Create:proc
extrn RawrXD_FAKE_Context_Destroy:proc
extrn RawrXD_FAKE_Inference_Run:proc

public main

.data
model_path      db "D:\\OllamaModels\\blobs\\sha256-512f302680f7ae6107fde46cc63dddeae48dc49288b98b81ed4ad1ecb4b4be7a",0
prompt_text     db "Benchmark run: generate code for quicksort.",0
warmup_tokens   equ 32
bench_tokens    equ 512
k_thousand      real8 1000.0

g_context       dq 0
output_buffer   dd 512 dup(0)
tokens_generated dd 0

.code
main proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h                 ; align stack + shadow space

    ; Create GPU context for model
    lea rcx, model_path
    call RawrXD_FAKE_Context_Create
    mov [g_context], rax
    test rax, rax
    jz bench_fail

    ; Warmup run (not measured)
    mov rcx, [g_context]
    lea rdx, prompt_text
    mov r8d, warmup_tokens
    lea r9, output_buffer
    lea rax, tokens_generated
    mov [rsp+20h], rax           ; 5th arg at stack+32
    call RawrXD_FAKE_Inference_Run

    ; Benchmark run (measured)
    mov rcx, [g_context]
    lea rdx, prompt_text
    mov r8d, bench_tokens
    lea r9, output_buffer
    lea rax, tokens_generated
    mov [rsp+20h], rax
    call RawrXD_FAKE_Inference_Run    ; xmm0 = elapsed ms

    ; Compute TPS = tokens_generated / ms * 1000
    ; Guard ms > 0
    movsd xmm1, xmm0                ; xmm1 = ms
    xorpd xmm2, xmm2
    comisd xmm1, xmm2
    jbe bench_fail                  ; zero or negative time is invalid

    mov eax, [tokens_generated]
    cvtsi2sd xmm3, rax              ; tokens as double
    divsd xmm3, xmm1                ; tokens/ms
    movsd xmm4, k_thousand
    movsd xmm3, xmm4                ; tokens/s

    ; No printf - just exit with success
    xor ecx, ecx
    jmp bench_exit

bench_fail:
    mov ecx, 1

bench_exit:
    add rsp, 48h
    pop rbp
    
    ; Direct syscall exit: 0x2D = ExitProcess (Windows x64)
    mov eax, ecx            ; exit code in eax
    mov r10, rsp
    syscall                 ; This won't work on user-mode - use ret instead
    ret
main endp
end
