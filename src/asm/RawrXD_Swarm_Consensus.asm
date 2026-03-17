;-------------------------------------------------------------------------------
; RawrXD_Swarm_Consensus.asm
; MASM64 - High-speed SIMD Logit Consensus Kernel
; Part of Phase 16: Byzantine Fault Tolerance (BFT) for Distributed Swarm
;-------------------------------------------------------------------------------

option casemap:none

.code

;-------------------------------------------------------------------------------
; RawrXD_Swarm_CompareLogitsAVX2
; RCX = Ptr to Primary Node Logits (Float32)
; RDX = Ptr to Secondary Node Logits (Float32)
; R8  = Number of elements (N)
; R9  = Epsilon (Tolerance Threshold) - passed as Float in XMM3
; Returns: RAX = Number of divergent elements
;-------------------------------------------------------------------------------
RawrXD_Swarm_CompareLogitsAVX2 proc
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    
    xor rax, rax            ; Dissent counter (return value)
    mov r10, r8             ; Copy count
    shr r10, 3              ; Process 8 floats at a time (AVX2: 256-bit)
    jz @tail_check

    ; Broadcast epsilon to all slots in XMM register
    vbroadcastss ymm2, xmm3 ; YMM2 = [eps, eps, eps, eps, eps, eps, eps, eps]

@loop_avx:
    vmovups ymm0, ymmword ptr [rcx] ; Load primary logits
    vmovups ymm1, ymmword ptr [rdx] ; Load secondary logits
    
    vsubps ymm3, ymm0, ymm1         ; ymm3 = nodeA - nodeB
    
    ; Manual Absolute Value (masking sign bit)
    vpcmpeqd ymm4, ymm4, ymm4       ; Set all bits to 1
    vpsrld   ymm4, ymm4, 1          ; ymm4 = 0x7FFFFFFF... (sign bit mask)
    vpand    ymm3, ymm3, ymm4       ; Clean absolute value

    vcmpps ymm5, ymm3, ymm2, 0eh    ; CMP_GT (ymm5 = 0xFFFFFFFF if |diff| > epsilon)
    vmovmskps r11d, ymm5            ; Extract bitmask
    
    popcnt r11d, r11d               ; Count bits (divergent elements in this batch)
    add rax, r11                    ; Accumulate dissent

    add rcx, 32                     ; Next 8 floats
    add rdx, 32
    dec r10
    jnz @loop_avx

@tail_check:
    and r8, 7                       ; Handle remaining elements (< 8)
    jz @exit

@loop_tail:
    vmovss xmm0, dword ptr [rcx]
    vmovss xmm1, dword ptr [rdx]
    vsubss xmm4, xmm0, xmm1
    
    ; abs xmm4
    vpcmpeqd xmm5, xmm5, xmm5
    vpsrld   xmm5, xmm5, 1
    vpand    xmm4, xmm4, xmm5
    
    vucomiss xmm4, xmm3             ; Compare against epsilon (XMM3)
    jbe @next_tail
    inc rax                         ; Dissent++

@next_tail:
    add rcx, 4
    add rdx, 4
    dec r8
    jnz @loop_tail

@exit:
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RawrXD_Swarm_CompareLogitsAVX2 endp

end
