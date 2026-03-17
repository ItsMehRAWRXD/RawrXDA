.code
option casemap:none

; =========================================================================================
; RawrXD KV Cache Manager (AVX-512)
; Handles Paged Attention KV Blocks
; =========================================================================================

public KVCache_Update_AVX512
public KVCache_Retrieve_AVX512

; -----------------------------------------------------------------------------------------
; KVCache_Update_AVX512(float* cache, const float* src, int pos, int head_dim)
; RCX = cache base
; RDX = src vector
; R8 = position index
; R9 = head dimension
; -----------------------------------------------------------------------------------------
KVCache_Update_AVX512 proc frame
    push rbp
    mov rbp, rsp
    .endprolog

    ; Calculate offset: pos * head_dim * sizeof(float)
    imul r8, r9
    shl r8, 2 ; * 4 bytes
    add rcx, r8 ; dest ptr
    
    ; Copy loop (using ZMM)
    ; Assuming head_dim is multiple of 16 for AVX-512 (512 bit = 16 floats)
copy_loop:
    cmp r9, 0
    jle done
    
    vmovups zmm0, zmmword ptr [rdx]
    vmovups zmmword ptr [rcx], zmm0
    
    add rdx, 64
    add rcx, 64
    sub r9, 16
    jmp copy_loop

done:
    mov rsp, rbp
    pop rbp
    ret
KVCache_Update_AVX512 endp

; -----------------------------------------------------------------------------------------
; KVCache_Retrieve_AVX512(float* cache, float* dst, int pos, int head_dim)
; RCX = cache base
; RDX = dst vector (output)
; R8 = position index
; R9 = head dimension (must be multiple of 16 for AVX-512)
; -----------------------------------------------------------------------------------------
KVCache_Retrieve_AVX512 proc frame
    push rbp
    mov rbp, rsp
    .endprolog

    ; Calculate offset: pos * head_dim * sizeof(float)
    imul r8, r9
    shl r8, 2                   ; * 4 bytes per float
    add rcx, r8                 ; src ptr = cache + offset

    ; Copy loop (using ZMM registers — 64 bytes = 16 floats per iteration)
retrieve_loop:
    cmp r9, 0
    jle retrieve_done

    vmovups zmm0, zmmword ptr [rcx]
    vmovups zmmword ptr [rdx], zmm0

    add rcx, 64
    add rdx, 64
    sub r9, 16
    jmp retrieve_loop

retrieve_done:
    mov rsp, rbp
    pop rbp
    ret
KVCache_Retrieve_AVX512 endp

end
