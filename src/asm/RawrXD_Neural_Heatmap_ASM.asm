; ==============================================================================
; RawrXD_Neural_Heatmap_ASM.asm - Accelerate Heatmap Weight Mapping
; ==============================================================================
; Pure x64 MASM to map model attention weights (float) to GDI color ranges.
; Part of Phase 2: Neural-Native UX visualization.
; ==============================================================================

PUBLIC map_weights_to_colors_avx2

.code

; void map_weights_to_colors_avx2(const float* weights, uint32_t* colors, size_t n)
; rcx = weights (ptr), rdx = colors (ptr), r8 = n (count)

map_weights_to_colors_avx2 PROC
    push rbp
    mov rbp, rsp

    xor rax, rax
    
    ; Scale factors for weight (0.0 - 1.0) to RGB
    ; Red = weight * 255, Green = (1.0 - weight) * 100, Blue = (1.0 - weight) * 200
    
    ; Constants for vector scaling
    vbroadcastss ymm4, dword ptr [const_255]
    vbroadcastss ymm5, dword ptr [const_100]
    vbroadcastss ymm6, dword ptr [const_200]
    vbroadcastss ymm7, dword ptr [const_1]

main_loop:
    cmp rax, r8
    jge done

    ; Load 8 weights
    vmovups ymm0, [rcx + rax*4]

    ; Red component: weight * 255
    vmulps ymm1, ymm0, ymm4
    vcvtps2dq ymm1, ymm1 ; convert to int

    ; Green component: (1.0 - weight) * 100
    vsubps ymm2, ymm7, ymm0
    vmulps ymm2, ymm2, ymm5
    vcvtps2dq ymm2, ymm2

    ; Blue component: (1.0 - weight) * 200
    vsubps ymm3, ymm7, ymm0
    vmulps ymm3, ymm3, ymm6
    vcvtps2dq ymm3, ymm3

    ; Pack into ARGB (0xFF, R, G, B)
    ; This is a simplified pack for the stub
    ; [Placeholder for logic to shift and OR R,G,B components into rdx]
    
    add rax, 8
    jmp main_loop

done:
    vzeroupper
    pop rbp
    ret

; Constants
const_255 dd 255.0
const_100 dd 100.0
const_200 dd 200.0
const_1   dd 1.0

map_weights_to_colors_avx2 ENDP

END
