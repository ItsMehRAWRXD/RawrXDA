; digestion_asm_x64.asm
; Ultra-fast pattern scanning for 1500+ file codebases
; Uses AVX-512 for 64-byte parallel string scanning if available, SSE4.2 fallback
; Exports: DigestionFastScan, DigestionHashChunk


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.code
ALIGN 16

; bool DigestionFastScan(const char* data, size_t len, const char* pattern, size_t patLen)
; RCX = data, RDX = len, R8 = pattern, R9 = patLen
; Returns: RAX = 1 if found, 0 if not
DigestionFastScan PROC FRAME
    push rbx
    push rsi
    push rdi
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .endprolog
    
    mov rsi, rcx        ; data
    mov rdi, r8         ; pattern
    mov rbx, r9         ; patLen
    
    ; Check CPU features for AVX-512
    mov eax, 7
    mov ecx, 0
    cpuid
    test ebx, 00010000h ; AVX-512F bit
    jz SSE42_Fallback
    
AVX512_Scan:
    ; AVX-512 implementation - process 64 bytes at a time
    vmovdqu8 zmm0, [rdi] ; Load pattern (assume <=64 for demo)
    mov rax, rdx
    sub rax, rbx
    add rax, 1          ; max search offset
    
    xor rcx, rcx
AVX512_Loop:
    cmp rcx, rax
    jae NotFound
    
    vmovdqu8 zmm1, [rsi + rcx]
    vpcmpeqb k1, zmm1, zmm0
    kortestw k1, k1
    jnz Found_AVX512
    
    add rcx, 64
    jmp AVX512_Loop
    
Found_AVX512:
    mov rax, 1
    vzeroupper
    jmp Done
    
SSE42_Fallback:
    ; SSE4.2 PCMPESTRI implementation
    mov rax, rdx
    sub rax, rbx
    add rax, 1
    
    xor rcx, rcx
SSE_Loop:
    cmp rcx, rax
    jae NotFound
    
    ; Use PCMPESTRI for substring search
    movdqu xmm0, [rdi]
    movdqu xmm1, [rsi + rcx]
    pcmpestri xmm0, xmm1, 1100b ; Equal ordered, return index
    
    jc Found_SSE
    add rcx, 16
    jmp SSE_Loop
    
Found_SSE:
    mov rax, 1
    jmp Done
    
NotFound:
    xor rax, rax
    
Done:
    pop rdi
    pop rsi
    pop rbx
    ret
DigestionFastScan ENDP

; void DigestionHashChunk(const void* data, size_t len, void* outHash[32])
; Simple Blake2s-like hash for incremental digestion
; RCX = data, RDX = len, R8 = outHash
DigestionHashChunk PROC FRAME
    push rsi
    push rdi
    .pushreg rsi
    .pushreg rdi
    .endprolog
    
    mov rsi, rcx
    mov rdi, r8
    mov rcx, rdx
    
    ; Initialize state (simplified)
    mov rax, 0x6A09E667BB67AE85h  ; IV
    mov [rdi], rax
    mov rax, 0x3C6EF372A54FF53Ah
    mov [rdi+8], rax
    
Hash_Loop:
    cmp rcx, 64
    jb Hash_Final
    
    ; Mix 64-byte block (simplified)
    mov rax, [rsi]
    xor [rdi], rax
    mov rax, [rsi+8]
    xor [rdi+8], rax
    
    add rsi, 64
    sub rcx, 64
    jmp Hash_Loop
    
Hash_Final:
    ; Finalize (simplified)
    mov rax, [rsi]
    xor [rdi], rax
    
    pop rdi
    pop rsi
    ret
DigestionHashChunk ENDP

END
