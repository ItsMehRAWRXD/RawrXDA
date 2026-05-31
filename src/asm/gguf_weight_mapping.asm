; ============================================================================
; GGUF Weight Offset Mapping for MASM Kernels
; ============================================================================
; Maps GGUF tensor layouts to MASM-optimized memory layouts for GPU SDMA
; ============================================================================

.DATA

; GGUF Header offsets (v3 format)
GGUF_MAGIC              EQU 0x46554747      ; "GGUF" little-endian
GGUF_VERSION            EQU 3
GGUF_TENSOR_COUNT_OFF   EQU 24            ; After header
GGUF_KV_COUNT_OFF       EQU 32

; Tensor info structure offsets
TENSOR_NAME_LEN_OFF     EQU 0
TENSOR_NAME_OFF         EQU 4
TENSOR_N_DIMS_OFF       EQU 8
TENSOR_DIMS_OFF         EQU 12
TENSOR_TYPE_OFF         EQU 60
TENSOR_OFFSET_OFF       EQU 64
TENSOR_INFO_SIZE        EQU 72

; GGML types to byte sizes
GGML_TYPE_F32           EQU 0
GGML_TYPE_F16           EQU 1
GGML_TYPE_Q4_0          EQU 2
GGML_TYPE_Q4_1          EQU 3
GGML_TYPE_Q5_0          EQU 6
GGML_TYPE_Q5_1          EQU 7
GGML_TYPE_Q8_0          EQU 8
GGML_TYPE_Q8_1          EQU 9

; Type sizes in bytes
SIZE_F32                EQU 4
SIZE_F16                EQU 2
SIZE_Q4_0               EQU 18            ; 32 weights + 2 scales per block
SIZE_Q4_1               EQU 20            ; 32 weights + 2 scales + 2 mins

; Transformer block tensor offsets (relative to block base)
; For LLaMA-style architecture
OFF_ATTN_NORM           EQU 0             ; RMSNorm weights
OFF_ATTN_Q              EQU 4096          ; Q projection (4096 * 4096 * 2 bytes for F16)
OFF_ATTN_K              EQU 33554432      ; K projection
OFF_ATTN_V              EQU 67108864      ; V projection
OFF_ATTN_O              EQU 100663296     ; Output projection
OFF_FFN_NORM            EQU 134217728     ; FFN RMSNorm
OFF_FFN_GATE            EQU 134221824     ; Gate projection (SwiGLU)
OFF_FFN_UP              EQU 167772160     ; Up projection
OFF_FFN_DOWN            EQU 201326592     ; Down projection

; Block size for 7B model (32 layers)
BLOCK_SIZE_7B           EQU 536870912     ; 512MB per block

.CODE

; ============================================================================
; MapGGUFToMASMLayout
; 
; Maps GGUF tensor offsets to MASM-optimized layout for SDMA
; 
; Parameters:
;   RCX = GGUF file base address
;   RDX = Output MASM layout buffer
;   R8  = Number of layers
; 
; Returns:
;   RAX = Number of tensors mapped
; ============================================================================
MapGGUFToMASMLayout PROC
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    
    mov     r12, rcx            ; GGUF base
    mov     r13, rdx            ; Output buffer
    mov     r14, r8             ; Layer count
    xor     r15, r15            ; Tensor counter
    
    ; Verify magic
    mov     eax, DWORD PTR [r12]
    cmp     eax, GGUF_MAGIC
    jne     @@error
    
    ; Get tensor count
    mov     rbx, QWORD PTR [r12 + GGUF_TENSOR_COUNT_OFF]
    test    rbx, rbx
    jz      @@done
    
    ; Skip to tensor info (after header + KV pairs)
    mov     rcx, QWORD PTR [r12 + GGUF_KV_COUNT_OFF]
    shl     rcx, 4              ; Rough estimate for KV size
    lea     r12, [r12 + 64 + rcx]  ; Skip header
    
@@tensor_loop:
    cmp     r15, rbx
    jge     @@done
    
    ; Read tensor info
    mov     eax, DWORD PTR [r12 + TENSOR_NAME_LEN_OFF]
    mov     ecx, DWORD PTR [r12 + TENSOR_TYPE_OFF]
    mov     rdx, QWORD PTR [r12 + TENSOR_OFFSET_OFF]
    
    ; Map to MASM layout
    mov     [r13 + 0], eax      ; Name length
    mov     [r13 + 4], ecx      ; Type
    mov     [r13 + 8], rdx      ; Offset
    
    ; Calculate aligned size for SDMA
    call    CalculateAlignedSize
    mov     [r13 + 16], rax     ; Aligned size
    
    ; Advance
    add     r12, TENSOR_INFO_SIZE
    add     r13, 32             ; Output entry size
    inc     r15
    jmp     @@tensor_loop
    
@@done:
    mov     rax, r15
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
    
@@error:
    xor     rax, rax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
MapGGUFToMASMLayout ENDP

; ============================================================================
; CalculateAlignedSize
; 
; Calculates SDMA-aligned size for tensor
; 
; Parameters:
;   ECX = GGML type
;   RDX = Element count
; 
; Returns:
;   RAX = Aligned size in bytes
; ============================================================================
CalculateAlignedSize PROC
    cmp     ecx, GGML_TYPE_F32
    je      @@f32
    cmp     ecx, GGML_TYPE_F16
    je      @@f16
    cmp     ecx, GGML_TYPE_Q4_0
    je      @@q4_0
    cmp     ecx, GGML_TYPE_Q8_0
    je      @@q8_0
    
    ; Default: F32
@@f32:
    mov     rax, rdx
    shl     rax, 2              ; *4
    jmp     @@align
    
@@f16:
    mov     rax, rdx
    shl     rax, 1              ; *2
    jmp     @@align
    
@@q4_0:
    ; 32 4-bit weights per block
    mov     rax, rdx
    shr     rax, 5              ; /32 blocks
    imul    rax, SIZE_Q4_0
    jmp     @@align
    
@@q8_0:
    mov     rax, rdx
    shr     rax, 6              ; /64 blocks
    imul    rax, 136            ; 128 weights + 8 scale
    
@@align:
    ; Align to 256 bytes for SDMA
    add     rax, 255
    and     rax, NOT 255
    ret
CalculateAlignedSize ENDP

; ============================================================================
; GetTransformerBlockOffset
; 
; Returns memory offset for transformer block component
; 
; Parameters:
;   RCX = Layer index (0-31)
;   RDX = Component type (0=attn_norm, 1=q, 2=k, 3=v, 4=o, 5=ffn_norm, etc.)
; 
; Returns:
;   RAX = Byte offset from model base
; ============================================================================
GetTransformerBlockOffset PROC
    push    rbx
    
    ; Calculate block base
    mov     rbx, BLOCK_SIZE_7B
    imul    rbx, rcx            ; Block base
    
    ; Add component offset
    cmp     edx, 0
    je      @@attn_norm
    cmp     edx, 1
    je      @@attn_q
    cmp     edx, 2
    je      @@attn_k
    cmp     edx, 3
    je      @@attn_v
    cmp     edx, 4
    je      @@attn_o
    cmp     edx, 5
    je      @@ffn_norm
    cmp     edx, 6
    je      @@ffn_gate
    cmp     edx, 7
    je      @@ffn_up
    cmp     edx, 8
    je      @@ffn_down
    
    xor     rax, rax
    pop     rbx
    ret
    
@@attn_norm:
    mov     rax, rbx
    add     rax, OFF_ATTN_NORM
    pop     rbx
    ret
    
@@attn_q:
    mov     rax, rbx
    add     rax, OFF_ATTN_Q
    pop     rbx
    ret
    
@@attn_k:
    mov     rax, rbx
    add     rax, OFF_ATTN_K
    pop     rbx
    ret
    
@@attn_v:
    mov     rax, rbx
    add     rax, OFF_ATTN_V
    pop     rbx
    ret
    
@@attn_o:
    mov     rax, rbx
    add     rax, OFF_ATTN_O
    pop     rbx
    ret
    
@@ffn_norm:
    mov     rax, rbx
    add     rax, OFF_FFN_NORM
    pop     rbx
    ret
    
@@ffn_gate:
    mov     rax, rbx
    add     rax, OFF_FFN_GATE
    pop     rbx
    ret
    
@@ffn_up:
    mov     rax, rbx
    add     rax, OFF_FFN_UP
    pop     rbx
    ret
    
@@ffn_down:
    mov     rax, rbx
    add     rax, OFF_FFN_DOWN
    pop     rbx
    ret
GetTransformerBlockOffset ENDP

END
