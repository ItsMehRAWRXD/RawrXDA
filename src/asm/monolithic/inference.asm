; RawrXD Inference Engine — Monolithic Module
; Exports: InferenceEngineInit, RunInference, TokenGenerate

PUBLIC InferenceEngineInit
PUBLIC RunInference
PUBLIC TokenGenerate
PUBLIC ClearKVCache
PUBLIC g_modelbase

.data?
align 16
; KV cache: use 64KB placeholder here; runtime can VirtualAlloc 2GB if needed
g_kvcache     db 10000h dup(?)
g_modelbase   dq ?
g_quantscale  dd ?

.data
align 8
g_hasAVX      dd 0

.const
GGUF_MAGIC    dd 046554747h
Q4_0_ID      equ 2

.code
; ────────────────────────────────────────────────────────────────
; InferenceEngineInit — AVX detection, KV cache init
;   cpuid clobbers EAX/EBX/ECX/EDX — rbx is non-volatile, must save.
; ────────────────────────────────────────────────────────────────
InferenceEngineInit PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; Detect AVX (CPUID.01H:ECX bit 28)
    mov     eax, 1
    cpuid
    test    ecx, 10000000h
    jz      @no_avx

    mov     g_hasAVX, 1
    lea     rax, g_kvcache
    mov     g_modelbase, rax
    xor     eax, eax
    jmp     @done

@no_avx:
    ; AVX not required for stub — still init KV cache base
    lea     rax, g_kvcache
    mov     g_modelbase, rax
    xor     eax, eax

@done:
    add     rsp, 20h
    pop     rbx
    ret
InferenceEngineInit ENDP

; ────────────────────────────────────────────────────────────────
; RunInference — generate N tokens into output buffer
;   RCX = pContext, EDX = tokenCount, R8 = pOutputBuf
; ────────────────────────────────────────────────────────────────
RunInference PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rsi, rcx
    mov     edi, edx
    mov     rbx, r8

    test    edi, edi
    jz      @run_done

@token_loop:
    call    TokenGenerate
    mov     dword ptr [rbx], eax
    add     rbx, 4
    dec     edi
    jnz     @token_loop

@run_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    xor     eax, eax
    ret
RunInference ENDP

; ────────────────────────────────────────────────────────────────
; TokenGenerate — read next token from model base (stub)
;   Returns token ID in EAX (0 = no model loaded)
; ────────────────────────────────────────────────────────────────
TokenGenerate PROC
    mov     rax, g_modelbase
    test    rax, rax
    jz      @stub
    mov     eax, dword ptr [rax]
    ret
@stub:
    xor     eax, eax
    ret
TokenGenerate ENDP

; ─── X+4: Clear KV cache on hotswap (when preserveKV=0) ─────────────────────
ClearKVCache PROC FRAME
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    lea     rdi, g_kvcache
    mov     rcx, 10000h / 8
    xor     eax, eax
    rep stosq
    add     rsp, 20h
    pop     rdi
    ret
ClearKVCache ENDP

END
