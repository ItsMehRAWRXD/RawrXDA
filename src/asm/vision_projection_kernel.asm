; ============================================================================
; vision_projection_kernel.asm — AVX2 SIMD Vision Projection Kernel
; ============================================================================
; High-performance MASM x64 kernels for vision model inference operations:
;
;   1. vision_project_avx2      — Matrix-vector projection (patch → embedding)
;   2. vision_normalize_avx2    — L2 normalization of embedding vectors
;   3. vision_dot_product_avx2  — SIMD dot product for similarity
;   4. vision_scale_avx2        — Scalar multiplication of vector
;   5. vision_add_avx2          — Element-wise vector addition
;   6. vision_softmax_avx2      — Row-wise softmax for attention weights
;
; Calling convention: Microsoft x64 (RCX, RDX, R8, R9, stack)
; All functions preserve RBX, RSI, RDI, R12-R15, XMM6-XMM15.
; Requires AVX2 + FMA (vfmadd231ps).
;
; Integration: Called by VisionEncoder::inferVisionModel and
;              VisionQuantizedEncoder when RAWR_HAS_MASM is defined.
; ============================================================================

.code

OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; ============================================================================
; vision_project_avx2 — Project input through weight matrix
; ============================================================================
; Computes: output[r] = sum_c(weight[r*cols + c] * input[c]) for r in [0, rows)
;
; Parameters:
;   RCX = const float* weight   (row-major [rows × cols])
;   RDX = const float* input    (vector [cols])
;   R8  = float* output         (vector [rows])
;   R9  = uint32_t rows         (output dimension)
;   [rsp+40] = uint32_t cols    (input dimension / inner loop count)
;
; Uses AVX2 vfmadd231ps for fused multiply-add (8 floats at a time).
; ============================================================================

vision_project_avx2 PROC

    ; Prologue — save non-volatile registers
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40             ; Shadow space + alignment

    ; Parameters
    mov     rsi, rcx            ; rsi = weight base pointer
    mov     rdi, rdx            ; rdi = input vector
    mov     r12, r8             ; r12 = output vector
    mov     r13d, r9d           ; r13d = rows (output dim)
    ; 5th parameter: 7 pushes (56) + sub rsp 40 = 96 bytes shifted from entry
    ; At function entry, 5th param is at [rsp+40], so now at [rsp + 96 + 40]
    mov     r14d, DWORD PTR [rsp + 136]  ; r14d = cols (input dim)

    test    r13d, r13d
    jz      project_done
    test    r14d, r14d
    jz      project_done

    ; Row loop: for each output row
    xor     r15d, r15d          ; r15d = current row index

project_row_loop:
    cmp     r15d, r13d
    jge     project_done

    ; Compute output[r15] = dot(weight[r15 * cols ..], input)
    ; Weight row pointer: rsi + r15 * cols * 4
    mov     eax, r15d
    imul    eax, r14d           ; eax = r15 * cols
    lea     rbx, [rsi + rax*4]  ; rbx = &weight[r15 * cols]

    ; Accumulator: 8-wide FP32 = 256 bits
    vxorps  ymm0, ymm0, ymm0   ; ymm0 = accumulator

    xor     ecx, ecx            ; ecx = column index

    ; AVX2 loop: process 8 floats per iteration
    mov     eax, r14d
    and     eax, 0FFFFFFF8h     ; eax = cols & ~7 (aligned count)
    test    eax, eax
    jz      project_scalar_loop

project_avx2_loop:
    cmp     ecx, eax
    jge     project_scalar_loop

    vmovups ymm1, YMMWORD PTR [rbx + rcx*4]    ; 8 weight values
    vmovups ymm2, YMMWORD PTR [rdi + rcx*4]    ; 8 input values
    vfmadd231ps ymm0, ymm1, ymm2               ; ymm0 += weight * input

    add     ecx, 8
    jmp     project_avx2_loop

project_scalar_loop:
    ; Handle remaining columns (< 8)
    cmp     ecx, r14d
    jge     project_reduce

    vmovss  xmm1, DWORD PTR [rbx + rcx*4]
    vmovss  xmm2, DWORD PTR [rdi + rcx*4]
    vfmadd231ss xmm0, xmm1, xmm2

    inc     ecx
    jmp     project_scalar_loop

project_reduce:
    ; Horizontal sum of ymm0: 8 → 1
    vextractf128 xmm1, ymm0, 1         ; xmm1 = upper 128 bits
    vaddps  xmm0, xmm0, xmm1           ; xmm0 = [a+e, b+f, c+g, d+h]
    vhaddps xmm0, xmm0, xmm0           ; xmm0 = [(a+e)+(b+f), (c+g)+(d+h), ...]
    vhaddps xmm0, xmm0, xmm0           ; xmm0 = [total, ...]

    ; Store result
    vmovss  DWORD PTR [r12 + r15*4], xmm0

    inc     r15d
    jmp     project_row_loop

project_done:
    vzeroupper

    add     rsp, 40
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

vision_project_avx2 ENDP

; ============================================================================
; vision_normalize_avx2 — L2 Normalize a float vector in-place
; ============================================================================
; Computes: v[i] = v[i] / sqrt(sum(v[j]^2))
;
; Parameters:
;   RCX = float* vector    (in-place normalization)
;   RDX = uint32_t length  (number of elements)
;
; Returns: XMM0 = L2 norm (before normalization)
; ============================================================================

vision_normalize_avx2 PROC

    push    rbx
    sub     rsp, 32

    mov     rbx, rcx            ; rbx = vector pointer
    mov     ecx, edx            ; ecx = length

    test    ecx, ecx
    jz      norm_done_zero

    ; Step 1: Compute sum of squares
    vxorps  ymm0, ymm0, ymm0   ; ymm0 = accumulator

    xor     eax, eax            ; eax = index
    mov     edx, ecx
    and     edx, 0FFFFFFF8h     ; edx = aligned length

    test    edx, edx
    jz      norm_sq_scalar

norm_sq_avx2:
    cmp     eax, edx
    jge     norm_sq_scalar

    vmovups ymm1, YMMWORD PTR [rbx + rax*4]
    vfmadd231ps ymm0, ymm1, ymm1               ; acc += v[i] * v[i]

    add     eax, 8
    jmp     norm_sq_avx2

norm_sq_scalar:
    cmp     eax, ecx
    jge     norm_reduce

    vmovss  xmm1, DWORD PTR [rbx + rax*4]
    vfmadd231ss xmm0, xmm1, xmm1

    inc     eax
    jmp     norm_sq_scalar

norm_reduce:
    ; Horizontal sum
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    ; xmm0 = sum of squares
    ; Check for near-zero (avoid division by zero)
    vcomiss xmm0, DWORD PTR [norm_epsilon]
    jb      norm_done_zero

    ; sqrt
    vsqrtss xmm2, xmm0, xmm0   ; xmm2 = L2 norm
    vmovss  xmm3, xmm2, xmm2   ; Save norm for return

    ; Compute 1/norm and broadcast
    mov     eax, 03F800000h     ; 1.0f
    vmovd   xmm4, eax
    vdivss  xmm4, xmm4, xmm2   ; xmm4 = 1.0 / norm
    vbroadcastss ymm4, xmm4     ; ymm4 = [invNorm, invNorm, ...]

    ; Step 2: Multiply all elements by invNorm
    xor     eax, eax
    mov     edx, ecx
    and     edx, 0FFFFFFF8h

    test    edx, edx
    jz      norm_div_scalar

norm_div_avx2:
    cmp     eax, edx
    jge     norm_div_scalar

    vmovups ymm1, YMMWORD PTR [rbx + rax*4]
    vmulps  ymm1, ymm1, ymm4
    vmovups YMMWORD PTR [rbx + rax*4], ymm1

    add     eax, 8
    jmp     norm_div_avx2

norm_div_scalar:
    cmp     eax, ecx
    jge     norm_done

    vmovss  xmm1, DWORD PTR [rbx + rax*4]
    vmulss  xmm1, xmm1, xmm4
    vmovss  DWORD PTR [rbx + rax*4], xmm1

    inc     eax
    jmp     norm_div_scalar

norm_done:
    vmovss  xmm0, xmm3, xmm3   ; Return L2 norm in xmm0
    vzeroupper
    add     rsp, 32
    pop     rbx
    ret

norm_done_zero:
    vxorps  xmm0, xmm0, xmm0   ; Return 0
    vzeroupper
    add     rsp, 32
    pop     rbx
    ret

vision_normalize_avx2 ENDP

; ============================================================================
; vision_dot_product_avx2 — SIMD Dot Product of two float vectors
; ============================================================================
; Computes: result = sum(a[i] * b[i])
;
; Parameters:
;   RCX = const float* a
;   RDX = const float* b
;   R8D = uint32_t length
;
; Returns: XMM0 = dot product result
; ============================================================================

vision_dot_product_avx2 PROC

    push    rbx

    mov     rbx, rcx            ; rbx = a
    mov     rcx, rdx            ; rcx = b
    mov     edx, r8d            ; edx = length

    vxorps  ymm0, ymm0, ymm0   ; accumulator

    test    edx, edx
    jz      dot_done

    xor     eax, eax
    mov     r8d, edx
    and     r8d, 0FFFFFFF8h     ; aligned count

    ; AVX2 loop: 8 floats per iteration with FMA
    test    r8d, r8d
    jz      dot_scalar

dot_avx2:
    cmp     eax, r8d
    jge     dot_scalar

    vmovups ymm1, YMMWORD PTR [rbx + rax*4]
    vmovups ymm2, YMMWORD PTR [rcx + rax*4]
    vfmadd231ps ymm0, ymm1, ymm2

    add     eax, 8
    jmp     dot_avx2

dot_scalar:
    cmp     eax, edx
    jge     dot_reduce

    vmovss  xmm1, DWORD PTR [rbx + rax*4]
    vmovss  xmm2, DWORD PTR [rcx + rax*4]
    vfmadd231ss xmm0, xmm1, xmm2

    inc     eax
    jmp     dot_scalar

dot_reduce:
    ; Horizontal sum
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    vzeroupper
    pop     rbx
    ret

dot_done:
    vxorps  xmm0, xmm0, xmm0
    vzeroupper
    pop     rbx
    ret

vision_dot_product_avx2 ENDP

; ============================================================================
; vision_scale_avx2 — Multiply vector by scalar
; ============================================================================
; Computes: v[i] *= scalar  for all i
;
; Parameters:
;   RCX = float* vector    (in-place)
;   XMM1 = float scalar    (via register, Windows x64 convention for 2nd float)
;   R8D = uint32_t length
; ============================================================================

vision_scale_avx2 PROC

    vbroadcastss ymm2, xmm1    ; Broadcast scalar to all 8 lanes

    xor     eax, eax
    mov     edx, r8d
    and     edx, 0FFFFFFF8h

    test    edx, edx
    jz      scale_scalar

scale_avx2:
    cmp     eax, edx
    jge     scale_scalar

    vmovups ymm0, YMMWORD PTR [rcx + rax*4]
    vmulps  ymm0, ymm0, ymm2
    vmovups YMMWORD PTR [rcx + rax*4], ymm0

    add     eax, 8
    jmp     scale_avx2

scale_scalar:
    cmp     eax, r8d
    jge     scale_done

    vmovss  xmm0, DWORD PTR [rcx + rax*4]
    vmulss  xmm0, xmm0, xmm1
    vmovss  DWORD PTR [rcx + rax*4], xmm0

    inc     eax
    jmp     scale_scalar

scale_done:
    vzeroupper
    ret

vision_scale_avx2 ENDP

; ============================================================================
; vision_add_avx2 — Element-wise vector addition: dst[i] += src[i] * weight
; ============================================================================
; Computes: dst[i] += src[i] * weight  for all i
;
; Parameters:
;   RCX = float* dst       (accumulator, in-place)
;   RDX = const float* src
;   XMM2 = float weight    (scaling factor for src)
;   R9D = uint32_t length
; ============================================================================

vision_add_avx2 PROC

    push    rbx
    mov     rbx, rcx            ; dst
    mov     rcx, rdx            ; src
    mov     edx, r9d            ; length

    vbroadcastss ymm3, xmm2    ; Broadcast weight

    xor     eax, eax
    mov     r8d, edx
    and     r8d, 0FFFFFFF8h

    test    r8d, r8d
    jz      add_scalar

add_avx2:
    cmp     eax, r8d
    jge     add_scalar

    vmovups ymm0, YMMWORD PTR [rbx + rax*4]    ; dst
    vmovups ymm1, YMMWORD PTR [rcx + rax*4]    ; src
    vfmadd231ps ymm0, ymm1, ymm3               ; dst += src * weight
    vmovups YMMWORD PTR [rbx + rax*4], ymm0

    add     eax, 8
    jmp     add_avx2

add_scalar:
    cmp     eax, edx
    jge     add_done

    vmovss  xmm0, DWORD PTR [rbx + rax*4]
    vmovss  xmm1, DWORD PTR [rcx + rax*4]
    vfmadd231ss xmm0, xmm1, xmm2
    vmovss  DWORD PTR [rbx + rax*4], xmm0

    inc     eax
    jmp     add_scalar

add_done:
    vzeroupper
    pop     rbx
    ret

vision_add_avx2 ENDP

; ============================================================================
; Data Section — Constants
; ============================================================================

.data
ALIGN 16
norm_epsilon DD 034000000h      ; 1e-7f (float threshold for near-zero norm)

; ============================================================================
; Public Exports
; ============================================================================

PUBLIC vision_project_avx2
PUBLIC vision_normalize_avx2
PUBLIC vision_dot_product_avx2
PUBLIC vision_scale_avx2
PUBLIC vision_add_avx2

END
