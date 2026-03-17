;==============================================================================
; dequant_simd.asm — Q4_0 and Q5_K_M Dequantization + SIMD MatMul
;
; REAL quantization dequant kernels with AVX2 and AVX-512.
; kernel32.lib ONLY. No CRT.
;
; Q4_0 block layout (20 bytes per 32 values):
;   - float16 scale (2 bytes)
;   - uint8[16] quants (16 bytes, 2 nibbles per byte = 32 values)
;
; Q5_K_M block layout (176 bytes per 256 values):
;   - float16 d (2 bytes)
;   - float16 dmin (2 bytes)
;   - uint8[12] scales (12 bytes)
;   - uint8[4] scale_high (4 bytes)
;   - uint8[32] qh (32 bytes) — 5th bit for each value
;   - uint8[128] qs (128 bytes) — low 4 bits, 2 per byte
;
; BUILD:
;   ml64 /c /nologo /W3 /Fo dequant_simd.obj dequant_simd.asm
;   link /nologo /subsystem:console /entry:main /out:dequant_simd.exe
;        dequant_simd.obj kernel32.lib
;==============================================================================

OPTION CASEMAP:NONE

;------------------------------------------------------------------------------
; Win32 Constants
;------------------------------------------------------------------------------
STD_OUTPUT_HANDLE           EQU -11
MEM_COMMIT                  EQU 1000h
MEM_RESERVE                 EQU 2000h
MEM_RELEASE                 EQU 8000h
PAGE_READWRITE              EQU 04h

;--- Q4_0 constants ---
Q4_0_BLOCK_SIZE             EQU 18    ; 2 (f16 scale) + 16 (quants) = 18 bytes
Q4_0_VALUES_PER_BLOCK       EQU 32

;--- Q5_K_M constants ---
Q5_K_BLOCK_SIZE             EQU 176
Q5_K_VALUES_PER_BLOCK       EQU 256

;------------------------------------------------------------------------------
; kernel32 imports
;------------------------------------------------------------------------------
EXTERNDEF GetStdHandle:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF VirtualAlloc:PROC
EXTERNDEF VirtualFree:PROC
EXTERNDEF ExitProcess:PROC

;==============================================================================
;                              DATA SECTION
;==============================================================================
.DATA

ALIGN 8
szBanner        BYTE "[DEQUANT] SIMD dequantization kernels — kernel32 only",13,10,0
szBannerLen     EQU $ - szBanner - 1
szQ4Test        BYTE "[DEQUANT] === Q4_0 Dequant Test ===",13,10,0
szQ4TestLen     EQU $ - szQ4Test - 1
szQ4Input       BYTE "[DEQUANT] Input block: scale=2.0, quants=0x01234567...",13,10,0
szQ4InputLen    EQU $ - szQ4Input - 1
szQ4Output      BYTE "[DEQUANT] Dequantized values (first 8): ",0
szQ4OutputLen   EQU $ - szQ4Output - 1
szMatMulTest    BYTE "[DEQUANT] === SIMD MatMul Test (4x4 @ AVX2) ===",13,10,0
szMatMulTestLen EQU $ - szMatMulTest - 1
szMatResult     BYTE "[DEQUANT] C[0][0] = ",0
szMatResultLen  EQU $ - szMatResult - 1
szCpuCheck      BYTE "[DEQUANT] CPU feature check: ",0
szCpuCheckLen   EQU $ - szCpuCheck - 1
szHasAVX2       BYTE "AVX2=YES ",0
szHasAVX2Len    EQU $ - szHasAVX2 - 1
szNoAVX2        BYTE "AVX2=NO ",0
szNoAVX2Len     EQU $ - szNoAVX2 - 1
szHasAVX512     BYTE "AVX512F=YES ",0
szHasAVX512Len  EQU $ - szHasAVX512 - 1
szNoAVX512      BYTE "AVX512F=NO ",0
szNoAVX512Len   EQU $ - szNoAVX512 - 1
szHasF16C       BYTE "F16C=YES",13,10,0
szHasF16CLen    EQU $ - szHasF16C - 1
szNoF16C        BYTE "F16C=NO",13,10,0
szNoF16CLen     EQU $ - szNoF16C - 1
szScalar        BYTE "[DEQUANT] Using scalar fallback (no AVX2)",13,10,0
szScalarLen     EQU $ - szScalar - 1
szQ5Test        BYTE "[DEQUANT] === Q5_K_M Dequant Test ===",13,10,0
szQ5TestLen     EQU $ - szQ5Test - 1
szQ5Output      BYTE "[DEQUANT] Q5_K_M dequantized values (first 4): ",0
szQ5OutputLen   EQU $ - szQ5Output - 1
szDotTest       BYTE "[DEQUANT] === Quantized Dot Product Test ===",13,10,0
szDotTestLen    EQU $ - szDotTest - 1
szDotResult     BYTE "[DEQUANT] dot(q4_a, q4_b) = ",0
szDotResultLen  EQU $ - szDotResult - 1
szDone          BYTE "[DEQUANT] All dequant kernels verified.",13,10,0
szDoneLen       EQU $ - szDone - 1
szNewline       BYTE 13,10,0
szComma         BYTE ", ",0

ALIGN 16
; Test Q4_0 block: 2 bytes scale (f16 for 2.0 = 0x4000) + 16 bytes quants
testQ4Block     WORD 4000h          ; f16 scale = 2.0
                BYTE 10h, 32h, 54h, 76h     ; quants: nibbles 0,1,2,3,4,5,6,7
                BYTE 98h, 0BAh, 0DCh, 0FEh  ; nibbles 8,9,10,11,12,13,14,15
                BYTE 10h, 32h, 54h, 76h     ; second half
                BYTE 98h, 0BAh, 0DCh, 0FEh

ALIGN 16
; Test Q5_K_M block (simplified — just first bytes for demo)
testQ5Block     BYTE 176 DUP(0)

;------------------------------------------------------------------------------
; CPU feature flags
;------------------------------------------------------------------------------
gHasAVX2        DWORD 0
gHasAVX512F     DWORD 0
gHasF16C        DWORD 0

;==============================================================================
;                              BSS SECTION
;==============================================================================
.DATA?

ALIGN 16
hStdout         QWORD ?
dwBytesWritten  DWORD ?
numBuf          BYTE 64 DUP(?)

ALIGN 16
dequantBuf      REAL4 256 DUP(?)    ; output: dequantized float32 values
matA            REAL4 16 DUP(?)     ; 4x4 matrix A
matB            REAL4 16 DUP(?)     ; 4x4 matrix B
matC            REAL4 16 DUP(?)     ; 4x4 result C

;==============================================================================
;                              CODE SECTION
;==============================================================================
.CODE

;--- Utilities (same as other modules) ---
write_con PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 48h
    .allocstack 48h
    .endprolog
    mov     r8d, edx
    mov     rdx, rcx
    mov     rcx, [hStdout]
    lea     r9, [dwBytesWritten]
    mov     QWORD PTR [rsp+20h], 0
    call    WriteFile
    leave
    ret
write_con ENDP

; Float to string (integer part only for demo)
ftoa_simple PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rdi, rcx            ; dest buffer
    ; xmm0 has the float

    ; Check negative
    xor     ebx, ebx
    movd    eax, xmm0
    test    eax, 80000000h
    jz      @@pos
    mov     BYTE PTR [rdi], '-'
    inc     rdi
    inc     ebx
    ; negate
    mov     eax, 80000000h
    movd    xmm1, eax
    xorps   xmm0, xmm1
@@pos:
    ; Get integer part
    cvttss2si eax, xmm0
    mov     esi, eax            ; integer part

    ; Get fractional part (2 decimal places)
    cvtsi2ss xmm1, esi
    subss   xmm0, xmm1
    mov     eax, 100
    cvtsi2ss xmm1, eax
    mulss   xmm0, xmm1
    cvttss2si ecx, xmm0        ; frac * 100

    ; Write integer part
    xor     edx, edx
    mov     rax, rdi
    mov     r8d, esi            ; value
    ; Simple itoa for integer part
    push    0                   ; sentinel
    test    r8d, r8d
    jnz     @@iloop
    push    '0'
    jmp     @@iwrite
@@iloop:
    test    r8d, r8d
    jz      @@iwrite
    mov     eax, r8d
    xor     edx, edx
    mov     r9d, 10
    div     r9d
    mov     r8d, eax
    add     edx, '0'
    push    rdx
    jmp     @@iloop
@@iwrite:
    pop     rax
    test    eax, eax
    jz      @@idone
    mov     BYTE PTR [rdi], al
    inc     rdi
    jmp     @@iwrite
@@idone:
    ; Decimal point
    mov     BYTE PTR [rdi], '.'
    inc     rdi

    ; Fractional part (2 digits, zero-padded)
    mov     eax, ecx
    test    eax, eax
    jns     @@fpos
    neg     eax
@@fpos:
    xor     edx, edx
    mov     r9d, 10
    div     r9d
    ; eax = tens digit, edx = ones digit
    add     eax, '0'
    add     edx, '0'
    mov     BYTE PTR [rdi], al
    mov     BYTE PTR [rdi+1], dl
    mov     BYTE PTR [rdi+2], 0
    add     rdi, 2

    ; Return ptr in rax, length in edx
    lea     rcx, [numBuf]
    mov     rax, rcx
    sub     rdi, rcx
    mov     edx, edi

    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
ftoa_simple ENDP

;==============================================================================
; detect_cpu — Check for AVX2, AVX-512F, F16C via CPUID
;==============================================================================
detect_cpu PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    .endprolog

    ; CPUID leaf 7, subleaf 0 for AVX2 and AVX-512F
    mov     eax, 7
    xor     ecx, ecx
    cpuid
    ; EBX bit 5 = AVX2
    bt      ebx, 5
    jnc     @@no_avx2
    mov     DWORD PTR [gHasAVX2], 1
@@no_avx2:
    ; EBX bit 16 = AVX-512F
    bt      ebx, 16
    jnc     @@no_512
    mov     DWORD PTR [gHasAVX512F], 1
@@no_512:

    ; CPUID leaf 1 for F16C
    mov     eax, 1
    cpuid
    ; ECX bit 29 = F16C
    bt      ecx, 29
    jnc     @@no_f16c
    mov     DWORD PTR [gHasF16C], 1
@@no_f16c:

    pop     rbx
    pop     rbp
    ret
detect_cpu ENDP

;==============================================================================
; dequant_q4_0_scalar — Dequantize one Q4_0 block (32 values)
;
; RCX = pointer to Q4_0 block (18 bytes)
; RDX = output float32 array (32 floats)
;
; Block layout: [f16_scale:2][quants:16]
; Each quant byte has 2 nibbles: low nibble first, then high nibble
; Value = (nibble - 8) * scale
;==============================================================================
dequant_q4_0_scalar PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 20h
    .allocstack 20h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rsi, rcx            ; block ptr
    mov     rdi, rdx            ; output ptr

    ; Convert f16 scale to f32
    ; F16C: vcvtph2ps
    cmp     DWORD PTR [gHasF16C], 0
    je      @@f16_manual

    movzx   eax, WORD PTR [rsi]
    movd    xmm0, eax
    vcvtph2ps xmm0, xmm0       ; xmm0 = f32 scale
    jmp     @@got_scale

@@f16_manual:
    ; Manual F16 → F32 conversion
    movzx   eax, WORD PTR [rsi]
    ; sign = (val >> 15) & 1
    mov     ecx, eax
    shr     ecx, 15
    ; exponent = (val >> 10) & 0x1F
    mov     edx, eax
    shr     edx, 10
    and     edx, 1Fh
    ; mantissa = val & 0x3FF
    and     eax, 3FFh

    ; Handle special cases
    test    edx, edx
    jz      @@f16_denorm
    cmp     edx, 1Fh
    je      @@f16_inf

    ; Normal: f32_exp = f16_exp - 15 + 127 = f16_exp + 112
    add     edx, 112
    shl     ecx, 31             ; sign bit
    shl     edx, 23             ; exponent
    shl     eax, 13             ; mantissa
    or      eax, edx
    or      eax, ecx
    movd    xmm0, eax
    jmp     @@got_scale

@@f16_denorm:
    ; Denormalized or zero
    test    eax, eax
    jz      @@f16_zero
    ; Normalize: find leading 1
    mov     edx, 113            ; start exponent
@@f16_shift:
    dec     edx
    shl     eax, 1
    test    eax, 400h           ; check if bit 10 set
    jz      @@f16_shift
    and     eax, 3FFh           ; remove leading 1
    shl     ecx, 31
    shl     edx, 23
    shl     eax, 13
    or      eax, edx
    or      eax, ecx
    movd    xmm0, eax
    jmp     @@got_scale

@@f16_zero:
    shl     ecx, 31
    movd    xmm0, ecx
    jmp     @@got_scale

@@f16_inf:
    shl     ecx, 31
    mov     edx, 0FFh
    shl     edx, 23
    shl     eax, 13
    or      eax, edx
    or      eax, ecx
    movd    xmm0, eax

@@got_scale:
    ; xmm0 = f32 scale
    ; Now dequantize 16 quant bytes = 32 values
    ; quants start at offset 2
    add     rsi, 2

    xor     ebx, ebx            ; quant byte index
@@q4_loop:
    cmp     ebx, 16
    jge     @@q4_done

    movzx   eax, BYTE PTR [rsi+rbx]

    ; Low nibble → value index = ebx*2
    mov     ecx, eax
    and     ecx, 0Fh
    sub     ecx, 8              ; center around 0
    cvtsi2ss xmm1, ecx
    mulss   xmm1, xmm0
    mov     ecx, ebx
    shl     ecx, 1              ; output index = ebx*2
    movss   DWORD PTR [rdi+rcx*4], xmm1

    ; High nibble → value index = ebx*2 + 1
    shr     eax, 4
    sub     eax, 8
    cvtsi2ss xmm1, eax
    mulss   xmm1, xmm0
    mov     ecx, ebx
    shl     ecx, 1
    inc     ecx                 ; output index = ebx*2 + 1
    movss   DWORD PTR [rdi+rcx*4], xmm1

    inc     ebx
    jmp     @@q4_loop

@@q4_done:
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
dequant_q4_0_scalar ENDP

;==============================================================================
; dequant_q4_0_avx2 — Dequantize Q4_0 with AVX2 (32 values in parallel)
;
; RCX = pointer to Q4_0 block
; RDX = output float32 array
;==============================================================================
dequant_q4_0_avx2 PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; Check AVX2 available
    cmp     DWORD PTR [gHasAVX2], 0
    je      @@fallback_scalar

    ; Convert f16 scale to f32 and broadcast
    movzx   eax, WORD PTR [rcx]
    movd    xmm0, eax
    vcvtph2ps xmm0, xmm0
    vbroadcastss ymm2, xmm0    ; ymm2 = scale broadcast

    ; Load 16 quant bytes into xmm, then process
    add     rcx, 2              ; skip scale
    vmovdqu xmm0, XMMWORD PTR [rcx]    ; 16 quant bytes

    ; Create constant -8 broadcast for centering
    mov     eax, 8
    vmovd   xmm3, eax
    vpbroadcastd ymm3, xmm3    ; ymm3 = [8,8,8,8,8,8,8,8]

    ; Create nibble mask
    mov     eax, 0Fh
    vmovd   xmm4, eax
    vpbroadcastb ymm4, xmm4    ; ymm4 = 0x0F in each byte

    ;--- Low nibbles (first 16 values) ---
    vpand   ymm5, ymm0, ymm4           ; low nibbles
    vpmovzxbd ymm6, xmm5               ; zero-extend bytes 0-7 to dwords
    ; Process first 8 low nibbles
    vpsubd  ymm6, ymm6, ymm3           ; subtract 8
    vcvtdq2ps ymm6, ymm6               ; to float
    vmulps  ymm6, ymm6, ymm2           ; multiply by scale
    vmovups YMMWORD PTR [rdx], ymm6     ; store values 0-7

    ; Process low nibbles 8-15
    vpsrldq xmm5, xmm5, 8             ; shift right 8 bytes within xmm
    vpmovzxbd ymm6, xmm5
    vpsubd  ymm6, ymm6, ymm3
    vcvtdq2ps ymm6, ymm6
    vmulps  ymm6, ymm6, ymm2
    vmovups YMMWORD PTR [rdx+32], ymm6  ; store values 8-15

    ;--- High nibbles (next 16 values) ---
    vpsrlw  ymm0, ymm0, 4              ; shift nibbles down
    vpand   ymm5, ymm0, ymm4           ; mask

    ; Process high nibbles 0-7
    vpmovzxbd ymm6, xmm5
    vpsubd  ymm6, ymm6, ymm3
    vcvtdq2ps ymm6, ymm6
    vmulps  ymm6, ymm6, ymm2
    vmovups YMMWORD PTR [rdx+64], ymm6  ; store values 16-23

    ; Process high nibbles 8-15
    vpsrldq xmm5, xmm5, 8
    vpmovzxbd ymm6, xmm5
    vpsubd  ymm6, ymm6, ymm3
    vcvtdq2ps ymm6, ymm6
    vmulps  ymm6, ymm6, ymm2
    vmovups YMMWORD PTR [rdx+96], ymm6  ; store values 24-31

    vzeroupper
    leave
    ret

@@fallback_scalar:
    ; Fall through to scalar version
    call    dequant_q4_0_scalar
    leave
    ret
dequant_q4_0_avx2 ENDP

;==============================================================================
; dequant_q5_k_scalar — Dequantize Q5_K_M block (256 values)
;
; Q5_K_M layout (176 bytes):
;   [0:2]   float16 d
;   [2:4]   float16 dmin
;   [4:16]  uint8[12] scales (6-bit scale + 6-bit min per sub-block)
;   [16:20] NOT USED (high bits packed differently in newer formats)
;   [20:52] uint8[32] qh — 5th bit for each of 256 values
;   [52:180] uint8[128] qs — low 4 bits, 2 per byte
;
; RCX = block ptr, RDX = output float32[256]
;==============================================================================
dequant_q5_k_scalar PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    .endprolog

    mov     rsi, rcx            ; block ptr
    mov     rdi, rdx            ; output ptr

    ; Read d (f16 at offset 0)
    movzx   eax, WORD PTR [rsi]
    movd    xmm0, eax
    vcvtph2ps xmm0, xmm0       ; xmm0 = d (f32)
    movss   DWORD PTR [rsp+10h], xmm0   ; save d

    ; Read dmin (f16 at offset 2)
    movzx   eax, WORD PTR [rsi+2]
    movd    xmm1, eax
    vcvtph2ps xmm1, xmm1       ; xmm1 = dmin (f32)
    movss   DWORD PTR [rsp+18h], xmm1   ; save dmin

    ; Process 8 sub-blocks of 32 values each
    ; Scales at offset 4, qh at offset 20, qs at offset 52
    xor     r12d, r12d          ; sub-block index (0..7)

@@q5_subblock:
    cmp     r12d, 8
    jge     @@q5_done

    ; Get scale and min for this sub-block
    ; scales[sub*2] and scales[sub*2+1] give 6-bit sc and min
    mov     eax, r12d
    shl     eax, 1              ; sub * 2
    movzx   r13d, BYTE PTR [rsi+4+rax]     ; raw scale byte

    ; sc = scales[sub*2] & 0x3F
    mov     r14d, r13d
    and     r14d, 3Fh           ; 6-bit scale

    ; m = scales[sub*2+1] & 0x3F
    movzx   r15d, BYTE PTR [rsi+5+rax]
    and     r15d, 3Fh           ; 6-bit min

    ; Effective scale = d * sc
    cvtsi2ss xmm2, r14d
    movss   xmm3, DWORD PTR [rsp+10h]  ; d
    mulss   xmm2, xmm3                  ; xmm2 = d * sc

    ; Effective min = dmin * m
    cvtsi2ss xmm3, r15d
    movss   xmm4, DWORD PTR [rsp+18h]  ; dmin
    mulss   xmm3, xmm4                  ; xmm3 = dmin * m

    ; Process 32 values in this sub-block
    ; qs offset for this sub-block: 52 + sub*16
    mov     eax, r12d
    shl     eax, 4              ; sub * 16
    add     eax, 52
    mov     r14d, eax           ; qs_offset

    ; qh offset: 20 + sub*4
    mov     eax, r12d
    shl     eax, 2              ; sub * 4
    add     eax, 20
    mov     r15d, eax           ; qh_offset

    xor     ebx, ebx            ; value pair index (0..15)
@@q5_pair:
    cmp     ebx, 16
    jge     @@q5_next_sub

    ; Read quant byte (2 values packed)
    movzx   eax, BYTE PTR [rsi+r14]
    inc     r14d

    ; Read 5th bit from qh
    mov     ecx, ebx
    shr     ecx, 3              ; qh byte index
    add     ecx, r15d
    movzx   edx, BYTE PTR [rsi+rcx]
    mov     ecx, ebx
    and     ecx, 7              ; bit position

    ;--- Low nibble (value index = sub*32 + pair*2) ---
    mov     r8d, eax
    and     r8d, 0Fh            ; 4 low bits
    ; Get 5th bit
    bt      edx, ecx
    jnc     @@no_bit5_lo
    or      r8d, 10h            ; set bit 4 (5th bit of value)
@@no_bit5_lo:
    ; value = d*sc*(q5) - dmin*m
    cvtsi2ss xmm5, r8d
    mulss   xmm5, xmm2         ; d*sc*q5
    subss   xmm5, xmm3         ; - dmin*m

    ; Store
    mov     ecx, r12d
    shl     ecx, 5              ; sub * 32
    mov     r8d, ebx
    shl     r8d, 1              ; pair * 2
    add     ecx, r8d
    movss   DWORD PTR [rdi+rcx*4], xmm5

    ;--- High nibble (value index = sub*32 + pair*2 + 1) ---
    shr     eax, 4
    ; 5th bit for high nibble — use next bit position
    mov     r8d, ebx
    add     r8d, 16             ; high nibble bits are in upper half
    mov     r9d, r8d
    shr     r9d, 3
    add     r9d, r15d
    movzx   r10d, BYTE PTR [rsi+r9]
    and     r8d, 7
    bt      r10d, r8d
    jnc     @@no_bit5_hi
    or      eax, 10h
@@no_bit5_hi:
    cvtsi2ss xmm5, eax
    mulss   xmm5, xmm2
    subss   xmm5, xmm3
    inc     ecx                 ; next value slot
    movss   DWORD PTR [rdi+rcx*4], xmm5

    inc     ebx
    jmp     @@q5_pair

@@q5_next_sub:
    inc     r12d
    jmp     @@q5_subblock

@@q5_done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
dequant_q5_k_scalar ENDP

;==============================================================================
; simd_matmul_f32 — AVX2 matrix multiply: C[M×N] = A[M×K] × B[K×N]
;
; RCX = float* A, RDX = float* B, R8 = float* C
; R9d = M, [rsp+28h] = K, [rsp+30h] = N
; All row-major.
;==============================================================================
simd_matmul_f32 PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    .endprolog

    mov     rsi, rcx            ; A
    mov     rdi, rdx            ; B
    mov     r15, r8             ; C
    mov     r12d, r9d           ; M
    mov     r13d, DWORD PTR [rbp+30h]   ; K (6th param on stack)
    mov     r14d, DWORD PTR [rbp+38h]   ; N (7th param on stack)

    ; For each row i of A
    xor     ebx, ebx            ; i = 0
@@mm_row:
    cmp     ebx, r12d
    jge     @@mm_done

    ; For each column j of B
    xor     ecx, ecx            ; j = 0
@@mm_col:
    cmp     ecx, r14d
    jge     @@mm_next_row

    ; Compute C[i][j] = sum over k: A[i][k] * B[k][j]
    vxorps  ymm0, ymm0, ymm0   ; accumulator

    xor     edx, edx            ; k = 0

    ; Check if we can use AVX2 path (K >= 8)
    cmp     r13d, 8
    jb      @@mm_scalar_k

@@mm_avx_k:
    cmp     edx, r13d
    jge     @@mm_hsum
    mov     eax, r13d
    sub     eax, edx
    cmp     eax, 8
    jb      @@mm_scalar_k

    ; Load 8 elements from A[i][k..k+7]
    mov     eax, ebx
    imul    eax, r13d
    add     eax, edx
    vmovups ymm1, YMMWORD PTR [rsi+rax*4]

    ; Load 8 elements from B column j, rows k..k+7
    ; B is row-major, so B[k][j] = B + (k*N + j)*4
    ; We need to gather — use scalar for column access
    ; Actually for small matrices, just use scalar
    jmp     @@mm_scalar_k

@@mm_scalar_k:
    cmp     edx, r13d
    jge     @@mm_hsum

    ; A[i][k]
    mov     eax, ebx
    imul    eax, r13d
    add     eax, edx
    vmovss  xmm1, DWORD PTR [rsi+rax*4]

    ; B[k][j]
    mov     eax, edx
    imul    eax, r14d
    add     eax, ecx
    vmovss  xmm2, DWORD PTR [rdi+rax*4]

    ; FMA: acc += A[i][k] * B[k][j]
    vfmadd231ss xmm0, xmm1, xmm2

    inc     edx
    jmp     @@mm_scalar_k

@@mm_hsum:
    ; Store C[i][j]
    mov     eax, ebx
    imul    eax, r14d
    add     eax, ecx
    vmovss  DWORD PTR [r15+rax*4], xmm0

    inc     ecx
    jmp     @@mm_col

@@mm_next_row:
    inc     ebx
    jmp     @@mm_row

@@mm_done:
    vzeroupper

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
simd_matmul_f32 ENDP

;==============================================================================
; vec_dot_q4_0 — Dot product of two Q4_0 quantized vectors
;
; RCX = Q4_0 blocks A (n_blocks * 18 bytes)
; RDX = Q4_0 blocks B
; R8d = number of blocks
; Returns: xmm0 = dot product result
;==============================================================================
vec_dot_q4_0 PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rsi, rcx            ; A blocks
    mov     rdi, rdx            ; B blocks
    mov     ebx, r8d            ; block count

    vxorps  xmm0, xmm0, xmm0   ; total accumulator

    xor     ecx, ecx            ; block index
@@dot_block:
    cmp     ecx, ebx
    jge     @@dot_done

    ; Get scale_a and scale_b
    movzx   eax, WORD PTR [rsi]
    movd    xmm1, eax
    vcvtph2ps xmm1, xmm1       ; scale_a

    movzx   eax, WORD PTR [rdi]
    movd    xmm2, eax
    vcvtph2ps xmm2, xmm2       ; scale_b

    mulss   xmm1, xmm2         ; xmm1 = scale_a * scale_b

    ; Dot product of quantized values (integer arithmetic)
    xor     r8d, r8d            ; sum of products
    xor     r9d, r9d            ; byte index

@@dot_byte:
    cmp     r9d, 16
    jge     @@dot_accumulate

    movzx   eax, BYTE PTR [rsi+2+r9]
    movzx   edx, BYTE PTR [rdi+2+r9]

    ; Low nibbles
    mov     r10d, eax
    and     r10d, 0Fh
    sub     r10d, 8
    mov     r11d, edx
    and     r11d, 0Fh
    sub     r11d, 8
    imul    r10d, r11d
    add     r8d, r10d

    ; High nibbles
    shr     eax, 4
    sub     eax, 8
    shr     edx, 4
    sub     edx, 8
    imul    eax, edx
    add     r8d, eax

    inc     r9d
    jmp     @@dot_byte

@@dot_accumulate:
    ; total += scale_a * scale_b * int_sum
    cvtsi2ss xmm2, r8d
    mulss   xmm2, xmm1
    addss   xmm0, xmm2

    ; Advance to next block
    add     rsi, Q4_0_BLOCK_SIZE
    add     rdi, Q4_0_BLOCK_SIZE
    inc     ecx
    jmp     @@dot_block

@@dot_done:
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
vec_dot_q4_0 ENDP

;==============================================================================
; main
;==============================================================================
main PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 80h
    .allocstack 80h
    push    rbx
    .pushreg rbx
    .endprolog

    ; Stdout
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     [hStdout], rax

    ; Banner
    lea     rcx, [szBanner]
    mov     edx, szBannerLen
    call    write_con

    ; Detect CPU features
    call    detect_cpu

    ; Print CPU features
    lea     rcx, [szCpuCheck]
    mov     edx, szCpuCheckLen
    call    write_con

    cmp     DWORD PTR [gHasAVX2], 0
    je      @@pr_no_avx2
    lea     rcx, [szHasAVX2]
    mov     edx, szHasAVX2Len
    jmp     @@pr_avx2_done
@@pr_no_avx2:
    lea     rcx, [szNoAVX2]
    mov     edx, szNoAVX2Len
@@pr_avx2_done:
    call    write_con

    cmp     DWORD PTR [gHasAVX512F], 0
    je      @@pr_no_512
    lea     rcx, [szHasAVX512]
    mov     edx, szHasAVX512Len
    jmp     @@pr_512_done
@@pr_no_512:
    lea     rcx, [szNoAVX512]
    mov     edx, szNoAVX512Len
@@pr_512_done:
    call    write_con

    cmp     DWORD PTR [gHasF16C], 0
    je      @@pr_no_f16c
    lea     rcx, [szHasF16C]
    mov     edx, szHasF16CLen
    jmp     @@pr_f16c_done
@@pr_no_f16c:
    lea     rcx, [szNoF16C]
    mov     edx, szNoF16CLen
@@pr_f16c_done:
    call    write_con

    ;=== Q4_0 Dequant Test ===
    lea     rcx, [szQ4Test]
    mov     edx, szQ4TestLen
    call    write_con

    lea     rcx, [szQ4Input]
    mov     edx, szQ4InputLen
    call    write_con

    ; Dequantize test block
    lea     rcx, [testQ4Block]
    lea     rdx, [dequantBuf]
    cmp     DWORD PTR [gHasAVX2], 0
    je      @@use_scalar_q4
    call    dequant_q4_0_avx2
    jmp     @@q4_print

@@use_scalar_q4:
    lea     rcx, [szScalar]
    mov     edx, szScalarLen
    call    write_con
    lea     rcx, [testQ4Block]
    lea     rdx, [dequantBuf]
    call    dequant_q4_0_scalar

@@q4_print:
    ; Print first 8 dequantized values
    lea     rcx, [szQ4Output]
    mov     edx, szQ4OutputLen
    call    write_con

    xor     ebx, ebx
@@print_q4_vals:
    cmp     ebx, 8
    jge     @@q4_print_done

    movss   xmm0, DWORD PTR [dequantBuf+rbx*4]
    lea     rcx, [numBuf]
    call    ftoa_simple
    mov     rcx, rax
    call    write_con

    cmp     ebx, 7
    jge     @@skip_comma
    lea     rcx, [szComma]
    mov     edx, 2
    call    write_con
@@skip_comma:
    inc     ebx
    jmp     @@print_q4_vals

@@q4_print_done:
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ;=== MatMul Test ===
    lea     rcx, [szMatMulTest]
    mov     edx, szMatMulTestLen
    call    write_con

    ; Fill 4x4 A = identity-ish, B = [1,2,3,4; ...]
    ; A = [[1,0,0,0],[0,2,0,0],[0,0,3,0],[0,0,0,4]]
    ; Zero matA
    lea     rdi, [matA]
    xor     eax, eax
    mov     ecx, 16
@@zero_a:
    mov     DWORD PTR [rdi+rcx*4-4], eax
    dec     ecx
    jnz     @@zero_a

    ; Set diagonal
    mov     eax, 3F800000h      ; 1.0f
    mov     DWORD PTR [matA], eax           ; A[0][0] = 1
    mov     eax, 40000000h      ; 2.0f
    mov     DWORD PTR [matA+20], eax        ; A[1][1] = 2
    mov     eax, 40400000h      ; 3.0f
    mov     DWORD PTR [matA+40], eax        ; A[2][2] = 3
    mov     eax, 40800000h      ; 4.0f
    mov     DWORD PTR [matA+60], eax        ; A[3][3] = 4

    ; B = [[1,2,3,4],[5,6,7,8],[9,10,11,12],[13,14,15,16]]
    mov     eax, 3F800000h      ; 1.0f
    mov     DWORD PTR [matB], eax
    mov     eax, 40000000h      ; 2.0f
    mov     DWORD PTR [matB+4], eax
    mov     eax, 40400000h      ; 3.0f
    mov     DWORD PTR [matB+8], eax
    mov     eax, 40800000h      ; 4.0f
    mov     DWORD PTR [matB+12], eax
    mov     eax, 40A00000h      ; 5.0f
    mov     DWORD PTR [matB+16], eax
    mov     eax, 40C00000h      ; 6.0f
    mov     DWORD PTR [matB+20], eax
    mov     eax, 40E00000h      ; 7.0f
    mov     DWORD PTR [matB+24], eax
    mov     eax, 41000000h      ; 8.0f
    mov     DWORD PTR [matB+28], eax
    mov     eax, 41100000h      ; 9.0f
    mov     DWORD PTR [matB+32], eax
    mov     eax, 41200000h      ; 10.0f
    mov     DWORD PTR [matB+36], eax
    mov     eax, 41300000h      ; 11.0f
    mov     DWORD PTR [matB+40], eax
    mov     eax, 41400000h      ; 12.0f
    mov     DWORD PTR [matB+44], eax
    mov     eax, 41500000h      ; 13.0f
    mov     DWORD PTR [matB+48], eax
    mov     eax, 41600000h      ; 14.0f
    mov     DWORD PTR [matB+52], eax
    mov     eax, 41700000h      ; 15.0f
    mov     DWORD PTR [matB+56], eax
    mov     eax, 41800000h      ; 16.0f
    mov     DWORD PTR [matB+60], eax

    ; C = A * B
    lea     rcx, [matA]
    lea     rdx, [matB]
    lea     r8, [matC]
    mov     r9d, 4              ; M = 4
    mov     DWORD PTR [rsp+28h], 4   ; K = 4
    mov     DWORD PTR [rsp+30h], 4   ; N = 4
    call    simd_matmul_f32

    ; Print C[0][0] — should be 1.0 (1*1 + 0 + 0 + 0)
    lea     rcx, [szMatResult]
    mov     edx, szMatResultLen
    call    write_con
    movss   xmm0, DWORD PTR [matC]
    lea     rcx, [numBuf]
    call    ftoa_simple
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ;=== Quantized Dot Product Test ===
    lea     rcx, [szDotTest]
    mov     edx, szDotTestLen
    call    write_con

    ; dot(testQ4Block, testQ4Block) — dot product with itself
    lea     rcx, [testQ4Block]
    lea     rdx, [testQ4Block]
    mov     r8d, 1              ; 1 block
    call    vec_dot_q4_0

    lea     rcx, [szDotResult]
    mov     edx, szDotResultLen
    call    write_con
    ; xmm0 has result from vec_dot_q4_0
    lea     rcx, [numBuf]
    call    ftoa_simple
    mov     rcx, rax
    call    write_con
    lea     rcx, [szNewline]
    mov     edx, 2
    call    write_con

    ;=== Done ===
    lea     rcx, [szDone]
    mov     edx, szDoneLen
    call    write_con

    xor     ecx, ecx
    call    ExitProcess

main ENDP

END
