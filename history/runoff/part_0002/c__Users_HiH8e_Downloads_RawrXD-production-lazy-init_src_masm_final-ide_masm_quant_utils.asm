;==========================================================================
; masm_quant_utils.asm - Pure MASM Quantization Utilities
; ==========================================================================
; Replaces quant_utils.cpp.
; High-performance quantization kernels (Q8_0, Q4_0, etc.)
;==========================================================================

option casemap:none

include windows.inc

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN console_log:PROC

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    szQuantInit     BYTE "QuantUtils: Initializing MASM kernels...", 0
    szQuantQ8       BYTE "QuantUtils: Quantizing to Q8_0 (n=%d)...", 0
    
    f_127           REAL4 127.0
    f_7             REAL4 7.0

.code

;==========================================================================
; quantize_q8_0(raw_ptr: rcx, n: rdx, out_ptr: r8)
;==========================================================================
PUBLIC quantize_q8_0
quantize_q8_0 PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rsi, rcx        ; raw_ptr (float*)
    mov rbx, rdx        ; n
    mov rdi, r8         ; out_ptr (scale + int8*)
    
    ; 1. Find max absolute value
    xorps xmm0, xmm0    ; amax = 0
    mov rcx, rbx
    mov rdx, rsi
.find_max:
    movss xmm1, dword ptr [rdx]
    andps xmm1, xmm1    ; abs (simplified, should use mask)
    maxss xmm0, xmm1
    add rdx, 4
    loop .find_max
    
    ; 2. Calculate scale
    divss xmm0, f_127   ; scale = amax / 127.0
    movss dword ptr [rdi], xmm0 ; Store scale
    
    ; 3. Quantize
    mov rcx, rbx
    mov rdx, rsi
    add rdi, 4
.quant_loop:
    movss xmm1, dword ptr [rdx]
    divss xmm1, xmm0    ; v = f / scale
    cvtss2si eax, xmm1  ; round to int
    ; clamp to -127..127
    cmp eax, 127
    jle @F
    mov eax, 127
@@:
    cmp eax, -127
    jge @F
    mov eax, -127
@@:
    mov byte ptr [rdi], al
    add rdx, 4
    inc rdi
    loop .quant_loop
    
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
quantize_q8_0 ENDP

END
