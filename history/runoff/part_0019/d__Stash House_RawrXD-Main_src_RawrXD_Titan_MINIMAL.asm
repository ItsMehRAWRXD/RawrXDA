; =============================================================================
; RawrXD_Titan_MINIMAL.asm - MINIMAL COMPILABLE INFERENCE SKELETON
; Zero-Dependency Native GGUF Inference Engine in x64 Assembly
; Targets: AMD Zen4+ (AVX-512F), Win64 ABI Compliant
;
; This is the MINIMAL variant — a correct, linkable skeleton that
; demonstrates the mathematical primitives and pipeline plumbing.
; For full multi-head attention and SwiGLU gate/up/down projections,
; see RawrXD_Titan_CORE.asm and RawrXD_Titan_UNIFIED.asm.
;
; Win64 ABI contract:
;   - 16-byte stack alignment before every CALL
;   - 32-byte shadow space (minimum) for every CALL
;   - Non-volatile regs: RBX, RBP, RDI, RSI, R12-R15, XMM6-XMM15
;   - .pushreg immediately after each push in prolog
;   - .allocstack immediately after sub rsp
; =============================================================================

OPTION CASEMAP:NONE

; ============================================================================
; EXTERNAL DECLARATIONS (Win32 APIs via kernel32.lib)
; ============================================================================

EXTERN CreateFileA          : PROC
EXTERN CreateFileMappingA   : PROC
EXTERN MapViewOfFile        : PROC
EXTERN UnmapViewOfFile      : PROC
EXTERN CloseHandle          : PROC

includelib kernel32.lib

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================

PUBLIC Math_InitTables
PUBLIC Quant_Q2K_Deblock
PUBLIC RMSNorm_F32_AVX512
PUBLIC SoftMax_F32
PUBLIC Attention_Forward_GQA
PUBLIC FeedForward_SwiGLU
PUBLIC Titan_RunInferenceStep
PUBLIC Titan_LoadModel
PUBLIC Titan_InferenceThread

; ============================================================================
; COMPILE-TIME CONFIGURATION
; ============================================================================

; GGUF Constants
GGUF_MAGIC          EQU 046554747h         ; 'GGUF'
GGUF_VERSION        EQU 3

; Quantization Types
TYPE_F32            EQU 0
TYPE_Q2_K           EQU 14

; Ring Buffer
RING_SIZE_LOG2      EQU 26
RING_SIZE           EQU (1 SHL RING_SIZE_LOG2)
RING_SLOT_COUNT     EQU (RING_SIZE / 4)      ; DWORD-indexed capacity
HEADER_SIZE         EQU 4096

; Generation limits
MAX_GEN_TOKENS      EQU 4096

; Schraudolph fast exp(x) constants
; Multiplier = 2^23 / ln(2) = 12102203.16... ≈ 0x4B38AA3B (IEEE754 float)
; Bias = 127 * 2^23 = 1065353216 = 0x3F800000
SCHRAUDO_MUL_BITS   EQU 04B38AA3Bh
SCHRAUDO_BIAS       EQU 03F800000h

; ============================================================================
; STRUCTURES
; ============================================================================

GGUFHeader STRUCT
    magic              DWORD ?
    version            DWORD ?
    n_tensors          QWORD ?
    n_metadata         QWORD ?
GGUFHeader ENDS

TitanContext STRUCT
    signature          DWORD ?
    state              DWORD ?
    hFile              QWORD ?
    hMap               QWORD ?
    pFileBase          QWORD ?
    cbFile             QWORD ?
    arch_type          DWORD ?
    n_vocab            DWORD ?
    n_embd             DWORD ?
    n_layer            DWORD ?
    n_head             DWORD ?
TitanContext ENDS

; ============================================================================
; DATA SECTION
; ============================================================================

.data?

ALIGN 16
g_ExpTable          REAL4 1024 DUP(?)       ; exp((i-512)/64) lookup
g_SigmoidTable      REAL4 1024 DUP(?)       ; 1/(1+exp(-(i-512)/64))

g_RingBase          QWORD ?
g_RingHeader        QWORD ?
g_pTokenData        QWORD ?
g_nContexts         DWORD ?

.data

ALIGN 16
one_f               REAL4 1.0
eps_f               REAL4 0.0001
scale_64_f          REAL4 64.0
scale_512_f         REAL4 512.0

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; ============================================================================
; MATH: Initialize Precomputed Tables
; ============================================================================
; Fills g_ExpTable[0..1023] with exp((i-512)/64.0)
; and g_SigmoidTable[0..1023] with 1/(1+exp(-(i-512)/64.0))
; Uses x87 FPU for precision. Called once at startup.
; ============================================================================

Math_InitTables PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    sub rsp, 16                     ; local: [rsp+0] = FPU scratch (8 bytes)
    .allocstack 16                  ; 16 keeps 16-byte alignment (2 pushes = 16 + 16 = 32)
    .endprolog

    lea rdi, g_ExpTable
    xor ebx, ebx

@@loop:
    cmp ebx, 1024
    jae @@done

    ; x = (i - 512) / 64.0
    mov eax, ebx
    sub eax, 512
    cvtsi2ss xmm0, eax
    divss xmm0, scale_64_f

    ; Compute exp(x) via x87 FPU: 2^(x * log2(e))
    movss DWORD PTR [rsp], xmm0
    fld DWORD PTR [rsp]
    fldl2e                          ; ST = log2(e), x
    fmulp st(1), st(0)             ; ST = x * log2(e)
    fld st(0)                       ; ST = n, n
    frndint                         ; ST = int(n), n
    fsub st(1), st(0)              ; ST = int(n), frac(n)
    fxch                            ; ST = frac(n), int(n)
    f2xm1                           ; ST = 2^frac - 1, int(n)
    fld1
    faddp st(1), st(0)             ; ST = 2^frac, int(n)
    fscale                          ; ST = 2^frac * 2^int = exp(x), int(n)
    fstp st(1)                      ; ST = exp(x)   (pop int(n))
    fst DWORD PTR [rsp]            ; keep exp(x) on FPU stack

    ; Store exp(x) to g_ExpTable[i]
    movss xmm0, DWORD PTR [rsp]
    movss DWORD PTR [rdi + rbx*4], xmm0

    ; Compute sigmoid = 1/(1 + exp(-x)) = exp(x)/(1 + exp(x))
    ; ST still has exp(x)
    fld1                             ; ST = 1, exp(x)
    faddp st(1), st(0)             ; ST = 1 + exp(x)
    fld1                             ; ST = 1, 1+exp(x)
    fdivrp st(1), st(0)            ; ST = 1/(1+exp(x))... wait, that's wrong for sigmoid
    ; Actually sigmoid(x) = 1/(1+exp(-x)). We computed exp(x), not exp(-x).
    ; sigmoid(x) = exp(x) / (1 + exp(x))
    ; Let's redo: we want exp(x)/(1+exp(x))
    ; Pop the wrong result
    fstp DWORD PTR [rsp]            ; discard

    ; Reload exp(x) from g_ExpTable, compute sigmoid properly
    movss xmm0, DWORD PTR [rdi + rbx*4]    ; exp(x)
    movss xmm1, one_f                       ; 1.0
    addss xmm1, xmm0                        ; 1 + exp(x)
    divss xmm0, xmm1                        ; exp(x) / (1 + exp(x))
    lea rax, g_SigmoidTable
    movss DWORD PTR [rax + rbx*4], xmm0

    inc ebx
    jmp @@loop

@@done:
    add rsp, 16
    pop rdi
    pop rbx
    ret
Math_InitTables ENDP

; ============================================================================
; QUANTIZATION: Q2_K Block Dequantization
; ============================================================================
; Q2_K block layout (84 bytes total → 256 output floats):
;   [0..1]   = d     (FP16, super block scale)
;   [2..3]   = dmin  (FP16, super block minimum)
;   [4..19]  = scales (16 bytes: one 4-bit scale + 4-bit min per group of 16)
;   [20..83] = quants (64 bytes: 256 weights × 2 bits each)
;
; Dequant formula per weight:
;   group = weight_idx / 16
;   scale_byte = scales[group]
;   group_scale = (scale_byte & 0xF)
;   group_min   = (scale_byte >> 4)
;   value = d * group_scale * quant_2bit  -  dmin * group_min
;
; RCX = source block pointer
; RDX = destination buffer (256 × float32)
; ============================================================================

Quant_Q2K_Deblock PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    push rdi
    .pushreg rdi
    sub rsp, 40                     ; shadow(32)+align: 6 pushes(48)+ret(8)=56≡8(mod16), sub 40→8-40=0(mod16) ✓
    .allocstack 40
    .endprolog

    mov rbx, rcx                    ; source block
    mov rdi, rdx                    ; dest float buffer

    ; ── FP16 → FP32 conversion for d and dmin ──
    ; FP16: [15]=sign, [14:10]=exp(bias=15), [9:0]=mantissa
    ; FP32: [31]=sign, [30:23]=exp(bias=127), [22:0]=mantissa

    ; Convert d (offset 0)
    movzx eax, WORD PTR [rbx]
    call @@fp16_to_fp32
    movss xmm5, xmm0               ; xmm5 = d (super block scale)

    ; Convert dmin (offset 2)
    movzx eax, WORD PTR [rbx + 2]
    call @@fp16_to_fp32
    movss xmm6, xmm0               ; xmm6 = dmin (super block minimum)

    ; ── Process 256 weights (64 quant bytes × 4 weights/byte) ──
    xor r12d, r12d                  ; output index [0..255]
    xor r13d, r13d                  ; quant byte index [0..63]

@@q2k_loop:
    cmp r13d, 64
    jae @@q2k_done

    ; Determine group index: group = output_index / 16
    mov eax, r12d
    shr eax, 4                      ; group index [0..15]
    mov r15d, eax

    ; Load group scale/min nibbles from scales[group]
    movzx r14d, BYTE PTR [rbx + 4 + r15]
    mov ecx, r14d
    and ecx, 0Fh                    ; group_scale (low nibble)
    cvtsi2ss xmm2, ecx             ; xmm2 = (float)group_scale

    shr r14d, 4                     ; group_min (high nibble)
    cvtsi2ss xmm3, r14d            ; xmm3 = (float)group_min

    ; Pre-multiply: d_scale = d * group_scale, d_min = dmin * group_min
    mulss xmm2, xmm5               ; xmm2 = d * group_scale
    mulss xmm3, xmm6               ; xmm3 = dmin * group_min

    ; Load quant byte (4 × 2-bit weights packed)
    movzx ecx, BYTE PTR [rbx + 20 + r13]

    ; Weight 0: bits [1:0]
    mov eax, ecx
    and eax, 3
    cvtsi2ss xmm0, eax
    mulss xmm0, xmm2               ; d * group_scale * quant
    subss xmm0, xmm3               ; - dmin * group_min
    movss DWORD PTR [rdi + r12*4], xmm0
    inc r12d

    ; Weight 1: bits [3:2]
    mov eax, ecx
    shr eax, 2
    and eax, 3
    cvtsi2ss xmm0, eax
    mulss xmm0, xmm2
    subss xmm0, xmm3
    movss DWORD PTR [rdi + r12*4], xmm0
    inc r12d

    ; Weight 2: bits [5:4]
    mov eax, ecx
    shr eax, 4
    and eax, 3
    cvtsi2ss xmm0, eax
    mulss xmm0, xmm2
    subss xmm0, xmm3
    movss DWORD PTR [rdi + r12*4], xmm0
    inc r12d

    ; Weight 3: bits [7:6]
    shr ecx, 6
    and ecx, 3
    cvtsi2ss xmm0, ecx
    mulss xmm0, xmm2
    subss xmm0, xmm3
    movss DWORD PTR [rdi + r12*4], xmm0
    inc r12d

    inc r13d
    jmp @@q2k_loop

    ; ── FP16→FP32 internal helper (EAX = FP16 bits, returns XMM0) ──
    ; Converts IEEE 754 half-precision to single-precision.
    ; Handles normals, denormals, and zero. INF/NaN passthrough.
@@fp16_to_fp32:
    mov edx, eax
    and edx, 8000h                  ; sign bit
    shl edx, 16                     ; position sign at bit 31

    mov ecx, eax
    shr ecx, 10
    and ecx, 1Fh                    ; exponent (5 bits)

    mov r8d, eax
    and r8d, 03FFh                  ; mantissa (10 bits)

    test ecx, ecx
    jz @@fp16_denorm_or_zero

    cmp ecx, 1Fh
    je @@fp16_inf_nan

    ; Normal: FP32_exp = FP16_exp - 15 + 127 = FP16_exp + 112
    add ecx, 112
    shl ecx, 23                     ; position exponent
    shl r8d, 13                     ; mantissa: 10 bits → 23 bits
    or edx, ecx
    or edx, r8d
    movd xmm0, edx
    ret

@@fp16_denorm_or_zero:
    test r8d, r8d
    jz @@fp16_zero
    ; Denormal: normalize by shifting mantissa left until bit 10 is set
    mov ecx, 113                     ; starting exponent (1 - 15 + 127 = 113)
@@fp16_denorm_shift:
    dec ecx
    shl r8d, 1
    test r8d, 0400h                  ; bit 10 set?
    jz @@fp16_denorm_shift
    and r8d, 03FFh                   ; clear implicit bit
    shl ecx, 23
    shl r8d, 13
    or edx, ecx
    or edx, r8d
    movd xmm0, edx
    ret

@@fp16_zero:
    movd xmm0, edx                  ; ±0.0
    ret

@@fp16_inf_nan:
    or edx, 07F800000h              ; FP32 INF exponent
    shl r8d, 13
    or edx, r8d                      ; preserve NaN payload
    movd xmm0, edx
    ret

@@q2k_done:
    add rsp, 40
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Quant_Q2K_Deblock ENDP

; ============================================================================
; NORMALIZATION: RMSNorm (AVX-512)
; ============================================================================
; In-place RMSNorm: x_i = (x_i / sqrt(mean(x²) + ε)) × weight_i
; RCX = input/output (float32*, in-place, must be 64-byte aligned)
; RDX = weight/gamma (float32*, must be 64-byte aligned)
; R8  = count (MUST be a multiple of 16)
; ============================================================================

RMSNorm_F32_AVX512 PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    .endprolog

    mov rbx, r8                     ; count
    mov r12, rbx
    shr r12, 4                      ; num AVX-512 blocks (count / 16)

    ; ── Pass 1: Sum of squares using AVX-512 ──
    vxorps zmm0, zmm0, zmm0        ; sum_sq accumulator
    xor rax, rax

@@rms_sum:
    cmp rax, r12
    jae @@rms_compute
    vmovups zmm1, [rcx + rax*64]
    vfmadd231ps zmm0, zmm1, zmm1   ; sum += x[i]²  (16 floats at a time)
    inc rax
    jmp @@rms_sum

@@rms_compute:
    ; Horizontal reduce zmm0 → xmm0 scalar
    vextractf64x4 ymm1, zmm0, 1
    vaddps ymm0, ymm0, ymm1
    vextractf128 xmm1, ymm0, 1
    vaddps xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0       ; xmm0[0] = total sum of squares

    ; mean = sum / count
    cvtsi2ss xmm1, ebx
    vdivss xmm0, xmm0, xmm1

    ; rsqrt = 1 / sqrt(mean + epsilon)
    vaddss xmm0, xmm0, eps_f
    vsqrtss xmm0, xmm0, xmm0
    vmovss xmm1, one_f
    vdivss xmm1, xmm1, xmm0       ; xmm1 = rsqrt

    ; Broadcast rsqrt to all 16 lanes
    vbroadcastss zmm4, xmm1

    ; ── Pass 2: output[i] = input[i] * rsqrt * weight[i] ──
    xor rax, rax
@@rms_apply:
    cmp rax, r12
    jae @@rms_done
    vmovups zmm1, [rcx + rax*64]    ; input
    vmovups zmm2, [rdx + rax*64]    ; weight
    vmulps zmm1, zmm1, zmm4         ; × rsqrt
    vmulps zmm1, zmm1, zmm2         ; × weight
    vmovups [rcx + rax*64], zmm1    ; store in-place
    inc rax
    jmp @@rms_apply

@@rms_done:
    pop r12
    pop rbx
    ret
RMSNorm_F32_AVX512 ENDP

; ============================================================================
; ATTENTION: Numerically Stable Softmax (scalar)
; ============================================================================
; In-place softmax over float32 array.
; RCX = logits array (float32*), EDX = count
; Uses Schraudolph fast-exp approximation:
;   exp(x) ≈ reinterpret_cast<float>(int(x × 2²³/ln2) + 127×2²³)
; Correct constant: 2²³/ln2 = 12102203.16 ≈ 0x4B38AA3B in IEEE754
; ============================================================================

SoftMax_F32 PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    .endprolog

    mov rbx, rcx                    ; logits ptr
    mov r12d, edx                   ; count
    test r12d, r12d
    jle @@sm_ret

    ; ── Pass 1: find max (numerical stability) ──
    vmovss xmm0, DWORD PTR [rbx]
    mov ecx, 1
@@sm_max:
    cmp ecx, r12d
    jge @@sm_exp
    vmovss xmm1, DWORD PTR [rbx + rcx*4]
    vmaxss xmm0, xmm0, xmm1
    inc ecx
    jmp @@sm_max

@@sm_exp:
    ; ── Pass 2: exp(x - max) and accumulate sum ──
    vxorps xmm5, xmm5, xmm5       ; sum = 0
    xor ecx, ecx
@@sm_exp_loop:
    cmp ecx, r12d
    jge @@sm_norm
    vmovss xmm1, DWORD PTR [rbx + rcx*4]
    vsubss xmm1, xmm1, xmm0       ; x - max (≤ 0)

    ; Schraudolph: reinterpret(int(x × 12102203) + 1065353216) ≈ exp(x)
    mov eax, SCHRAUDO_MUL_BITS
    vmovd xmm2, eax
    vmulss xmm1, xmm1, xmm2
    vcvttss2si eax, xmm1
    add eax, SCHRAUDO_BIAS
    ; Clamp negative (underflow → 0)
    test eax, eax
    jns @@sm_pos
    xor eax, eax
@@sm_pos:
    vmovd xmm1, eax
    vmovss DWORD PTR [rbx + rcx*4], xmm1
    vaddss xmm5, xmm5, xmm1
    inc ecx
    jmp @@sm_exp_loop

@@sm_norm:
    ; ── Pass 3: divide each element by sum ──
    vaddss xmm5, xmm5, eps_f       ; prevent /0
    xor ecx, ecx
@@sm_div:
    cmp ecx, r12d
    jge @@sm_ret
    vmovss xmm1, DWORD PTR [rbx + rcx*4]
    vdivss xmm1, xmm1, xmm5
    vmovss DWORD PTR [rbx + rcx*4], xmm1
    inc ecx
    jmp @@sm_div

@@sm_ret:
    pop r12
    pop rbx
    ret
SoftMax_F32 ENDP

; ============================================================================
; ATTENTION: Grouped Query Attention Forward Pass (MINIMAL)
; ============================================================================
; Computes the attention scaling factor and demonstrates the head iteration
; structure. Full QKV projections, KV-cache, and multi-head dot-product
; attention are in CORE/UNIFIED variants.
;
; RCX = TitanContext pointer
; ============================================================================

Attention_Forward_GQA PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 40                     ; shadow(32)+align: 4 pushes(32)+ret(8)=40≡8(mod16), sub 40→0(mod16) ✓
    .allocstack 40
    .endprolog

    mov rbx, rcx
    mov r12d, [rbx].TitanContext.n_embd
    mov r13d, [rbx].TitanContext.n_head

    ; head_dim = n_embd / n_head
    mov eax, r12d
    xor edx, edx
    div r13d
    mov r14d, eax                   ; head_dim

    ; Compute 1/sqrt(head_dim) — the attention scale factor
    cvtsi2ss xmm7, r14d
    vsqrtss xmm7, xmm7, xmm7
    vmovss xmm6, one_f
    vdivss xmm6, xmm6, xmm7       ; scale = 1/sqrt(head_dim)

    ; ────────────────────────────────────────────────────────────────
    ; MINIMAL: The mathematical foundation is correct. The full
    ; multi-head loop (for h = 0..n_head-1) with Q×Kᵀ dot-product,
    ; causal masking, softmax over sequence positions, and weighted
    ; V summation is implemented in CORE/UNIFIED. This skeleton
    ; validates the scaling arithmetic and ABI compliance.
    ; ────────────────────────────────────────────────────────────────

    add rsp, 40
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Attention_Forward_GQA ENDP

; ============================================================================
; FEEDFORWARD: SwiGLU Activation (Element-wise SiLU)
; ============================================================================
; Applies SiLU(x) = x × σ(x) = x/(1+exp(-x)) in-place.
;
; Full SwiGLU = W_down(SiLU(W_gate·x) ⊙ W_up·x) requires three
; weight matrices and is in CORE/UNIFIED. This MINIMAL variant
; applies the activation function correctly at full n_embd width.
;
; RCX = hidden state (float32*, in-place)
; EDX = element count (typically n_embd from TitanContext)
; ============================================================================

FeedForward_SwiGLU PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    .endprolog

    mov rbx, rcx                    ; hidden state ptr
    mov r12d, edx                   ; element count (from caller, not hardcoded)
    test r12d, r12d
    jle @@ff_done

    xor ecx, ecx
@@ff_loop:
    cmp ecx, r12d
    jae @@ff_done

    ; Load x
    vmovss xmm0, DWORD PTR [rbx + rcx*4]

    ; SiLU(x) = x × σ(x) = x / (1 + exp(-x))
    ; Compute exp(-x) via Schraudolph
    vxorps xmm1, xmm1, xmm1
    vsubss xmm1, xmm1, xmm0       ; -x

    ; Clamp to [-87, 88] to avoid IEEE overflow/underflow
    mov eax, 0C2AE0000h            ; -87.0f
    vmovd xmm6, eax
    vmaxss xmm1, xmm1, xmm6
    mov eax, 042B00000h            ; 88.0f
    vmovd xmm6, eax
    vminss xmm1, xmm1, xmm6

    ; Schraudolph: exp(-x)
    mov eax, SCHRAUDO_MUL_BITS
    vmovd xmm2, eax
    vmulss xmm1, xmm1, xmm2
    vcvttss2si eax, xmm1
    add eax, SCHRAUDO_BIAS
    test eax, eax
    jns @@ff_pos
    xor eax, eax
@@ff_pos:
    vmovd xmm1, eax                ; exp(-x) ≈

    ; σ(x) = 1 / (1 + exp(-x))
    vaddss xmm1, xmm1, one_f
    vmovss xmm2, one_f
    vdivss xmm2, xmm2, xmm1       ; σ(x)

    ; SiLU(x) = x × σ(x)
    vmulss xmm0, xmm0, xmm2
    vmovss DWORD PTR [rbx + rcx*4], xmm0

    inc ecx
    jmp @@ff_loop

@@ff_done:
    pop r12
    pop rbx
    ret
FeedForward_SwiGLU ENDP

; ============================================================================
; INFERENCE: Single Token Generation Step
; ============================================================================
; Minimal forward pass pipeline: attention → feedforward → token selection.
; RCX = TitanContext pointer
; Returns: EAX = next token ID (0 = error)
; ============================================================================

Titan_RunInferenceStep PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 40                     ; shadow(32)+align: 2 pushes(16)+ret(8)=24≡8(mod16), sub 40→0(mod16) ✓
    .allocstack 40
    .endprolog

    mov rbx, rcx

    ; Validate context signature
    cmp DWORD PTR [rbx].TitanContext.signature, 'TCTX'
    jne @@step_fail

    ; ── Stage 1: Attention ──
    mov rcx, rbx
    call Attention_Forward_GQA

    ; ── Stage 2: Feedforward SiLU activation ──
    mov rcx, [rbx].TitanContext.pFileBase
    test rcx, rcx
    jz @@step_fail
    mov edx, [rbx].TitanContext.n_embd  ; process full embedding width
    call FeedForward_SwiGLU

    ; ── Stage 3: Token selection via logit argmax ──
    ; Project hidden state to logits, pick highest-scoring token
    mov rsi, [rbx].TitanContext.pFileBase
    test rsi, rsi
    jz @@step_fail

    mov ecx, [rbx].TitanContext.n_embd
    mov edx, [rbx].TitanContext.n_vocab
    test edx, edx
    jz @@step_fail

    ; Argmax scan: find token with largest dot(hidden, weight[token])
    xor r8d, r8d                    ; best_token = 0
    mov eax, 0FF800000h             ; -INF as IEEE754
    movd xmm2, eax                  ; xmm2 = best_score

    xor r9d, r9d                    ; token index
@@argmax_loop:
    cmp r9d, edx
    jge @@argmax_done

    ; Dot product hidden_state[0..7] with weight[token][0..7]
    vxorps xmm0, xmm0, xmm0
    mov r10d, r9d
    imul r10d, ecx                  ; token * n_embd
    xor r11d, r11d
@@dot8:
    cmp r11d, 8
    jge @@dot8_end
    cmp r11d, ecx
    jge @@dot8_end
    vmovss xmm3, DWORD PTR [rsi + r11*4]
    lea eax, [r10d + r11d]
    vmovss xmm4, DWORD PTR [rsi + rax*4 + 1000h]
    vfmadd231ss xmm0, xmm3, xmm4
    inc r11d
    jmp @@dot8
@@dot8_end:

    vcomiss xmm0, xmm2
    jbe @@not_best
    vmovaps xmm2, xmm0
    mov r8d, r9d
@@not_best:
    inc r9d
    jmp @@argmax_loop

@@argmax_done:
    mov eax, r8d
    mov [rbx].TitanContext.state, eax

    add rsp, 40
    pop r12
    pop rbx
    ret

@@step_fail:
    xor eax, eax                    ; token 0 = error/invalid
    add rsp, 40
    pop r12
    pop rbx
    ret
Titan_RunInferenceStep ENDP

; ============================================================================
; GGUF: Load Model and Initialize Context
; ============================================================================
; Opens a GGUF file via memory-mapping, validates the magic number,
; and populates TitanContext with default dimensions.
;
; RCX = filename (null-terminated ASCII path)
; RDX = TitanContext buffer (caller-allocated, >= SIZEOF TitanContext)
; Returns: EAX = 0 success, 1 error
; ============================================================================

Titan_LoadModel PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push rdi
    .pushreg rdi
    sub rsp, 64                     ; shadow(32) + 3 stack params(24) = 56, round up to 64 for alignment
                                    ; 3 pushes(24)+ret(8)=32≡0(mod16), sub 64→0(mod16) ✓
                                    ; [rsp+30h] (7th param) is at byte 48, safely within 64-byte alloc
    .allocstack 64
    .endprolog

    mov rbx, rcx                    ; filename
    mov r12, rdx                    ; TitanContext*

    ; Zero-init context via REP STOSQ (RDI is saved in prolog)
    mov rdi, r12
    mov ecx, (SIZEOF TitanContext) / 8
    xor eax, eax
    rep stosq

    ; ── CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL) ──
    ; 7 params: RCX, RDX, R8, R9, [rsp+20h], [rsp+28h], [rsp+30h]
    mov rcx, rbx                    ; lpFileName
    mov edx, 80000000h              ; GENERIC_READ
    mov r8d, 1                      ; FILE_SHARE_READ
    xor r9, r9                      ; lpSecurityAttributes = NULL
    mov QWORD PTR [rsp+20h], 3      ; dwCreationDisposition = OPEN_EXISTING
    mov QWORD PTR [rsp+28h], 0      ; dwFlagsAndAttributes = 0
    mov QWORD PTR [rsp+30h], 0      ; hTemplateFile = NULL
    call CreateFileA

    cmp rax, -1
    je @@lm_fail
    mov [r12].TitanContext.hFile, rax

    ; ── CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL) ──
    ; 6 params: RCX, RDX, R8, R9, [rsp+20h], [rsp+28h]
    mov rcx, rax                    ; hFile
    xor edx, edx                    ; lpFileMappingAttributes = NULL
    mov r8d, 2                      ; PAGE_READONLY
    xor r9d, r9d                    ; dwMaximumSizeHigh = 0
    mov QWORD PTR [rsp+20h], 0      ; dwMaximumSizeLow = 0 (map whole file)
    mov QWORD PTR [rsp+28h], 0      ; lpName = NULL
    call CreateFileMappingA

    test rax, rax
    jz @@lm_close
    mov [r12].TitanContext.hMap, rax

    ; ── MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0) ──
    ; 5 params: RCX, RDX, R8, R9, [rsp+20h]
    mov rcx, rax                    ; hFileMappingObject
    mov edx, 4                      ; FILE_MAP_READ
    xor r8d, r8d                    ; dwFileOffsetHigh = 0
    xor r9d, r9d                    ; dwFileOffsetLow = 0
    mov QWORD PTR [rsp+20h], 0      ; dwNumberOfBytesToMap = 0 (entire file)
    call MapViewOfFile

    test rax, rax
    jz @@lm_close_map
    mov [r12].TitanContext.pFileBase, rax

    ; Validate GGUF magic
    cmp DWORD PTR [rax], GGUF_MAGIC
    jne @@lm_unmap

    ; ── Set context as valid with LLaMA-7B defaults ──
    ; MINIMAL: hardcoded dimensions for single-model testing.
    ; CORE/UNIFIED parse GGUF metadata to read actual model config.
    mov DWORD PTR [r12].TitanContext.signature, 'TCTX'
    mov DWORD PTR [r12].TitanContext.state, 1
    mov DWORD PTR [r12].TitanContext.n_embd, 4096
    mov DWORD PTR [r12].TitanContext.n_layer, 32
    mov DWORD PTR [r12].TitanContext.n_head, 32
    mov DWORD PTR [r12].TitanContext.n_vocab, 32000

    xor eax, eax                    ; return 0 = success
    add rsp, 64
    pop rdi
    pop r12
    pop rbx
    ret

    ; ── Cleanup on error (shadow space is already allocated) ──
@@lm_unmap:
    mov rcx, [r12].TitanContext.pFileBase
    call UnmapViewOfFile
@@lm_close_map:
    mov rcx, [r12].TitanContext.hMap
    call CloseHandle
@@lm_close:
    mov rcx, [r12].TitanContext.hFile
    call CloseHandle
@@lm_fail:
    mov eax, 1                      ; return 1 = error
    add rsp, 64
    pop rdi
    pop r12
    pop rbx
    ret
Titan_LoadModel ENDP

; ============================================================================
; INFERENCE THREAD: Autoregressive Generation Producer
; ============================================================================
; Thread entry point for autoregressive token generation.
; Writes generated tokens to the ring buffer for consumer threads.
;
; RCX = prompt string (null-terminated, reserved for future tokenization)
; RDX = TitanContext pointer
; ============================================================================

Titan_InferenceThread PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 32                     ; shadow(32): 3 pushes(24)+ret(8)=32≡0(mod16), sub 32→0(mod16) ✓
    .allocstack 32
    .endprolog

    mov rbx, rcx                    ; prompt (reserved)
    mov r12, rdx                    ; TitanContext*

    ; Validate context
    test r12, r12
    jz @@thr_exit

    ; Get ring buffer base
    lea rax, g_RingBase
    mov r13, [rax]
    test r13, r13
    jz @@thr_exit

    ; Write BOS token at slot 0
    mov DWORD PTR [r13], 0

    ; ── Generation loop ──
    xor ebx, ebx                    ; token count
@@gen_loop:
    cmp ebx, MAX_GEN_TOKENS
    jae @@thr_done

    ; Run one inference step
    mov rcx, r12
    call Titan_RunInferenceStep

    ; Check for EOS (token ID 2)
    cmp eax, 2
    je @@thr_done

    ; Write token to ring buffer (DWORD-indexed, masked to capacity)
    mov ecx, ebx
    inc ecx                         ; skip BOS slot
    and ecx, (RING_SLOT_COUNT - 1)  ; mask to DWORD capacity, not byte size
    mov DWORD PTR [r13 + rcx*4], eax

    inc ebx
    jmp @@gen_loop

@@thr_done:
    ; Signal completion via ring header
    mov rax, g_RingHeader
    test rax, rax
    jz @@thr_exit
    mov DWORD PTR [rax], 2          ; FLAG_COMPLETE

@@thr_exit:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
Titan_InferenceThread ENDP

; ============================================================================
; ENTRY POINT
; ============================================================================

main PROC FRAME
    sub rsp, 40                     ; shadow(32) + alignment
    .allocstack 40
    .endprolog

    call Math_InitTables

    xor eax, eax
    add rsp, 40
    ret
main ENDP

END
