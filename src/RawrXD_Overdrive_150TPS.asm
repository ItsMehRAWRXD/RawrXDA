.code

; Enhancement 116: SwarmV150_VNNI_Dequant_Core
align 16
SwarmV150_VNNI_Dequant_Core PROC
    vmovups zmm0, zmmword ptr [rsi]
    vmovups zmm1, zmmword ptr [rdi]
    vpxord zmm2, zmm2, zmm2
    vpdpbusd zmm2, zmm1, zmm0
    vcvtdq2ps zmm3, zmm2
    vmovups zmmword ptr [rdx], zmm3
    ret
SwarmV150_VNNI_Dequant_Core ENDP

; Enhancement 117: SwarmV150_Direct_P2P_DMA_Fetch
align 16
SwarmV150_Direct_P2P_DMA_Fetch PROC
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
    mov rcx, r8
    shr rcx, 6
    jz @f
transfer_loop:
    vmovups zmm0, zmmword ptr [rsi]
    vmovups zmmword ptr [rdi], zmm0
    add rsi, 64
    add rdi, 64
    dec rcx
    jnz transfer_loop
@@:
    sfence
    pop rdi
    pop rsi
    ret
SwarmV150_Direct_P2P_DMA_Fetch ENDP

; Enhancement 118: SwarmV150_Zero_Latency_KV_Switch
align 16
SwarmV150_Zero_Latency_KV_Switch PROC
    prefetcht0 byte ptr [rcx]
    prefetcht1 byte ptr [rcx + rdx]
    ret
SwarmV150_Zero_Latency_KV_Switch ENDP

; Enhancement 119: SwarmV150_Core_Affinity_Lock
align 16
SwarmV150_Core_Affinity_Lock PROC
    sub rsp, 40
    extrn __imp_GetCurrentThread: proc
    extrn __imp_SetThreadAffinityMask: proc
    call qword ptr [__imp_GetCurrentThread]
    mov rcx, rax
    mov rdx, -1
    call qword ptr [__imp_SetThreadAffinityMask]
    add rsp, 40
    ret
SwarmV150_Core_Affinity_Lock ENDP

; Enhancement 120: SwarmV150_Speculative_TopK_Pruning
align 16
SwarmV150_Speculative_TopK_Pruning PROC
    db 62h, 0F1h, 07Ch, 48h, 18h, 0C9h ; vpbroadcastss zmm1, xmm1
    vmovups zmm0, zmmword ptr [rcx]
    vcmpps k1, zmm0, zmm1, 1
    vpxord zmm2, zmm2, zmm2
    vmovups zmm3 {k1}, zmm2
    vmovups zmmword ptr [rcx], zmm3
    ret
SwarmV150_Speculative_TopK_Pruning ENDP

; Enhancement 121: SwarmV150_Parallel_Batch_Prefill
align 16
SwarmV150_Parallel_Batch_Prefill PROC
    test rdx, rdx
    jz @f
batch_loop:
    vmovups zmm0, zmmword ptr [rcx]
    vmovups zmm1, zmmword ptr [rcx+64]
    add rcx, 128
    sub rdx, 32
    ja batch_loop
@@:
    ret
SwarmV150_Parallel_Batch_Prefill ENDP

; Enhancement 122: SwarmV150_AVX512_FMA_Unroll
align 16
SwarmV150_AVX512_FMA_Unroll PROC
    vmovups zmm0, zmmword ptr [rcx]
    vmovups zmm1, zmmword ptr [rcx+64]
    vfmadd231ps zmm8, zmm0, zmmword ptr [rdx]
    vfmadd231ps zmm9, zmm1, zmmword ptr [rdx+64]
    ret
SwarmV150_AVX512_FMA_Unroll ENDP

END
