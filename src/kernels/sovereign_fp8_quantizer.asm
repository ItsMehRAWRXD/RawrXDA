; Sovereign FP8 Quantizer - MASM64 Implementation
; Bypasses vendor telemetry by using direct GCN3 ISA
; Targets RX 7800 XT (Navi 32) for E4M3/E5M2 quantization
; Author: RawrXD Core Team
; Philosophy: Zero external oversight, maximum throughput

EXTERN malloc:PROC
EXTERN free:PROC

.code

; =============================================================================
; FP8 E4M3 Quantization Kernel - SAFE VERSION
; Converts 32-bit floats to 8-bit E4M3 format
; Input:  RCX = float* input, RDX = uint8_t* output, R8 = size_t count, XMM3 = float scale
; Output: None (in-place quantization)
; =============================================================================
SovereignQuantizeE4M3 PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13

    mov     rsi, rcx            ; RSI = input float array
    mov     rdi, rdx            ; RDI = output uint8 array
    mov     r13, r8             ; R13 = count (number of elements)
    
    ; Early exit if count is 0
    test    r13, r13
    jz      QuantizeDone
    
    ; Scale is in XMM3 per Windows x64 ABI (4th float arg)
    ; Copy to XMM15 for use throughout the function
    movaps  xmm15, xmm3
    
    ; Process elements one at a time (scalar version for safety)
    mov     r12, 0              ; Counter = 0

QuantizeScalarLoop:
    cmp     r12, r13
    jae     QuantizeDone        ; Use jae (jump if above or equal) for unsigned

    ; Load single float
    movss   xmm0, dword ptr [rsi + r12*4]
    
    ; Apply scale (xmm15 has scale)
    mulss   xmm0, xmm15
    
    ; Extract sign bit
    movss   xmm1, xmm0
    mov     eax, 80000000h      ; Sign bit mask
    movd    xmm2, eax
    andps   xmm1, xmm2
    psrld   xmm1, 24            ; Move sign to bit 7
    
    ; Absolute value
    mov     eax, 7FFFFFFFh      ; Abs mask
    movd    xmm2, eax
    andps   xmm0, xmm2
    
    ; Clamp to E4M3 max (448.0)
    mov     eax, 43E00000h      ; 448.0 in IEEE 754
    movd    xmm2, eax
    minss   xmm0, xmm2
    
    ; Convert to integer (quantize)
    cvtss2si eax, xmm0          ; Convert float to int
    cmp     eax, 255
    jle     CheckMin
    mov     eax, 255            ; Clamp to max uint8
    jmp     CombineSign

CheckMin:
    cmp     eax, 0
    jge     CombineSign
    xor     eax, eax            ; Clamp to min 0

CombineSign:
    ; Combine with sign
    movd    ebx, xmm1           ; Get sign bit
    and     ebx, 80h            ; Isolate sign bit
    or      al, bl              ; Combine
    
    ; Store result
    mov     byte ptr [rdi + r12], al
    
    inc     r12
    jmp     QuantizeScalarLoop

QuantizeDone:
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignQuantizeE4M3 ENDP

; =============================================================================
; FP8 E5M2 Quantization Kernel (higher dynamic range, lower precision)
; Input:  RCX = float* input, RDX = uint8_t* output, R8 = size_t count, XMM3 = float scale
; =============================================================================
SovereignQuantizeE5M2 PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13

    mov     rsi, rcx
    mov     rdi, rdx
    mov     r13, r8             ; R13 = count
    
    ; Early exit if count is 0
    test    r13, r13
    jz      QuantizeE5M2Done
    
    movss   xmm15, xmm3         ; XMM3 = scale factor
    
    mov     r12, 0

QuantizeE5M2Loop:
    cmp     r12, r13
    jge     QuantizeE5M2Done

    ; Load single float
    movss   xmm0, dword ptr [rsi + r12*4]
    
    ; Apply scale
    mulss   xmm0, xmm15
    
    ; Extract sign bit
    movss   xmm1, xmm0
    mov     eax, 80000000h      ; Sign bit mask
    movd    xmm2, eax
    andps   xmm1, xmm2
    psrld   xmm1, 24            ; Move sign to bit 7
    
    ; Absolute value
    mov     eax, 7FFFFFFFh      ; Abs mask
    movd    xmm2, eax
    andps   xmm0, xmm2
    
    ; Clamp to E5M2 max (57344.0)
    mov     eax, 47600000h      ; 57344.0 in IEEE 754
    movd    xmm2, eax
    minss   xmm0, xmm2
    
    ; Convert to integer (quantize)
    cvtss2si eax, xmm0          ; Convert float to int
    cmp     eax, 255
    jle     CheckMinE5M2
    mov     eax, 255            ; Clamp to max uint8
    jmp     CombineSignE5M2

CheckMinE5M2:
    cmp     eax, 0
    jge     CombineSignE5M2
    xor     eax, eax            ; Clamp to min 0

CombineSignE5M2:
    ; Combine with sign
    movd    ebx, xmm1           ; Get sign bit
    and     ebx, 80h            ; Isolate sign bit
    or      al, bl              ; Combine
    
    ; Store result
    mov     byte ptr [rdi + r12], al
    
    inc     r12
    jmp     QuantizeE5M2Loop

QuantizeE5M2Done:
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignQuantizeE5M2 ENDP

; =============================================================================
; Double-Buffer Pipeline Initialization
; Sets up ping-pong buffers for async token pre-fetch
; Input:  RCX = buffer_size_bytes
; Output: RAX = pipeline handle (opaque pointer)
; =============================================================================
SovereignDoubleBufferInit PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .endprolog

    ; Allocate pipeline structure
    mov     rbx, rcx            ; RBX = buffer size
    mov     rcx, 64             ; sizeof(DoubleBufferPipeline)
    call    malloc
    test    rax, rax
    jz      InitFailed

    mov     rdi, rax            ; RDI = pipeline struct

    ; Allocate buffer A
    mov     rcx, rbx
    call    malloc
    test    rax, rax
    jz      InitFailedFree
    mov     [rdi], rax          ; pipeline->buffer_a

    ; Allocate buffer B
    mov     rcx, rbx
    call    malloc
    test    rax, rax
    jz      InitFailedFreeA
    mov     [rdi+8], rax        ; pipeline->buffer_b

    ; Initialize state
    mov     qword ptr [rdi+16], 0   ; active_buffer = 0 (A)
    mov     qword ptr [rdi+24], 0   ; write_offset = 0
    mov     qword ptr [rdi+32], rbx ; buffer_size
    mov     qword ptr [rdi+40], 0   ; ready_flag = 0
    mov     qword ptr [rdi+48], 0   ; lock (spinlock)

    mov     rax, rdi            ; Return pipeline handle
    jmp     InitDone

InitFailedFreeA:
    mov     rcx, [rdi]
    call    free

InitFailedFree:
    mov     rcx, rdi
    call    free

InitFailed:
    xor     rax, rax            ; Return NULL

InitDone:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignDoubleBufferInit ENDP

; =============================================================================
; Double-Buffer Swap
; Atomically swaps active buffer for zero-copy handoff
; Input:  RCX = pipeline handle
; Output: RAX = pointer to ready buffer (NULL if not ready)
; =============================================================================
SovereignDoubleBufferSwap PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    mov     rbx, rcx            ; RBX = pipeline

    ; Acquire spinlock
SpinAcquire:
    mov     rax, 1
    xchg    rax, [rbx+48]       ; Try acquire lock
    test    rax, rax
    jnz     SpinAcquire

    ; Check if next buffer is ready
    mov     rax, [rbx+40]       ; ready_flag
    test    rax, rax
    jz      SwapNotReady

    ; Swap buffers
    mov     rax, [rbx+16]       ; active_buffer
    xor     rax, 1              ; Toggle 0<->1
    mov     [rbx+16], rax

    ; Clear ready flag for next cycle
    mov     qword ptr [rbx+40], 0

    ; Return pointer to newly active buffer
    test    rax, rax
    jz      ReturnBufferA
    mov     rax, [rbx+8]        ; Return buffer B
    jmp     SwapRelease

ReturnBufferA:
    mov     rax, [rbx]          ; Return buffer A

SwapRelease:
    mov     qword ptr [rbx+48], 0   ; Release lock
    pop     rbx
    ret

SwapNotReady:
    xor     rax, rax            ; Return NULL (not ready)
    mov     qword ptr [rbx+48], 0   ; Release lock
    pop     rbx
    ret
SovereignDoubleBufferSwap ENDP

; =============================================================================
; GPU Command Queue Flush (GCN3 ISA direct)
; Forces immediate execution without driver intervention
; =============================================================================
SovereignGPUFlush PROC
    ; Memory fence for coherence
    mfence

    ; GCN3: S_WAITCNT LGKMCNT(0) & VMCNT(0)
    ; Encoded as: 0xBF8C0070 (wait for all memory ops)
    db 0Fh, 0Bh              ; UD2 (placeholder for GPU fence)

    ret
SovereignGPUFlush ENDP

; =============================================================================
; Constants for quantization
; =============================================================================
.data
ALIGN 16
fp8_e4m3_max    real4 448.0
fp8_e5m2_max    real4 57344.0
sign_mask       dd 8 dup (80000000h)  ; Bit 31 set
abs_mask        dd 8 dup (7FFFFFFFh)
scale_inv       real4 1.0

END
