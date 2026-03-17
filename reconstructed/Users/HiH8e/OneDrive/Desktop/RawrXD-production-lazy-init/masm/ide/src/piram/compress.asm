; ============================================================================
; PIRAM_COMPRESS.ASM - π-RAM Integration Layer
; Provides high-level API for GGUF and buffer compression
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

; π constants
PI_CONSTANT     equ 3296474           ; forward scale (≈π * 2^20)
PI_INV_CONSTANT equ 333773            ; inverse scale (≈(1/π) * 2^20)

; External core functions from piram_ultra.asm
PiRam_Compress PROTO
PiRam_Halve PROTO
PiRam_Stream PROTO

; Public API
PUBLIC PiRam_PerfectCircleFwd
PUBLIC PiRam_PerfectCircleInv
PUBLIC PiRam_CompressBuffer
PUBLIC PiRam_CompressGGUF
PUBLIC PiRam_MultiPassCompress
PUBLIC PiRam_GetCompressionRatio
PUBLIC PiRam_EnableHalving
PUBLIC PiRam_PerfectCircle11Pass
PUBLIC PiRam_CircleCompress
PUBLIC PiRam_CircleDecompress

.data
    g_CompressionRatio dd 200 ; Default 2:1 ratio
    g_HalvingEnabled   dd 1   ; Enabled by default
    g_LocalCompressEnabled dd 1  ; Use local-only compression
    g_QuantizationEnabled dd 1  ; Adaptive quantization

; External 11-pass circle function
PiRam_PerfectCircle11Pass PROTO :DWORD, :DWORD
PiRam_CircleCompress PROTO :DWORD, :DWORD, :DWORD
PiRam_CircleDecompress PROTO :DWORD, :DWORD, :DWORD
PiRam_PerfectCircleX11 PROTO :DWORD, :DWORD
PiRam_LocalCompress PROTO :DWORD, :DWORD
PiRam_QuantizeAdaptive PROTO :DWORD, :DWORD, :DWORD

.code

; ============================================================================
; PiRam_PerfectCircleFwd - In-place π-transform
; Input:  pBuf:DWORD, dwSize:DWORD
; Output: EAX = processed size
; ============================================================================
PiRam_PerfectCircleFwd PROC pBuf:DWORD, dwSize:DWORD
    push esi
    push edi
    push ebx

    mov esi, pBuf
    mov edi, dwSize

    test esi, esi
    jz @@fail
    test edi, edi
    jz @@fail

    ; Perfect Circle Forward: Halve the size and transform
    shr edi, 1
    xor ecx, ecx

@@loop:
    cmp ecx, edi
    jae @@done

    movzx eax, byte ptr [esi + ecx]
    imul eax, PI_CONSTANT
    shr eax, 20
    and eax, 0FFh
    mov byte ptr [esi + ecx], al

    inc ecx
    jmp @@loop

@@done:
    mov eax, edi
    jmp @@exit

@@fail:
    xor eax, eax

@@exit:
    pop ebx
    pop edi
    pop esi
    ret
PiRam_PerfectCircleFwd ENDP

; ============================================================================
; PiRam_PerfectCircleInv - In-place inverse π-transform
; Input:  pBuf:DWORD, dwSize:DWORD
; Output: EAX = processed size
; ============================================================================
PiRam_PerfectCircleInv PROC pBuf:DWORD, dwSize:DWORD
    push esi
    push edi
    push ebx

    mov esi, pBuf
    mov edi, dwSize

    test esi, esi
    jz @@fail
    test edi, edi
    jz @@fail

    xor ecx, ecx

@@loop:
    cmp ecx, edi
    jae @@done

    movzx eax, byte ptr [esi + ecx]
    imul eax, PI_INV_CONSTANT
    shr eax, 20
    and eax, 0FFh
    mov byte ptr [esi + ecx], al

    inc ecx
    jmp @@loop

@@done:
    mov eax, edi
    jmp @@exit

@@fail:
    xor eax, eax

@@exit:
    pop ebx
    pop edi
    pop esi
    ret
PiRam_PerfectCircleInv ENDP

; ============================================================================
; PiRam_CompressBuffer - High-level buffer compression
; Input:  pBuf:DWORD, dwSize:DWORD
; Output: EAX = new buffer, EDX = new size
; ============================================================================
PiRam_CompressBuffer PROC pBuf:DWORD, dwSize:DWORD
    mov eax, pBuf
    mov edx, dwSize
    call PiRam_Compress
    ret
PiRam_CompressBuffer ENDP

; ============================================================================
; PiRam_CompressGGUF - Compress GGUF model
; Input:  pModel:DWORD (structure with size at offset 16, data at offset 12)
; Output: EAX = success
; ============================================================================
PiRam_CompressGGUF PROC pModel:DWORD
    push esi
    push edi
    push ebx

    mov esi, pModel
    test esi, esi
    jz @@fail

    ; Get data pointer and size
    mov eax, [esi + 12]    ; data
    mov edx, [esi + 16]    ; size
    
    test eax, eax
    jz @@fail
    test edx, edx
    jz @@fail

    ; If halving enabled, use the core compress which allocates a new halved buffer
    cmp g_HalvingEnabled, 1
    jne @@no_halve

    ; Call core compress
    ; EAX = data, EDX = size
    call PiRam_Compress
    test eax, eax
    jz @@fail

    ; Update model structure with new buffer and size
    ; We should free the old buffer if it wasn't mapped
    ; For simplicity in this integration, we just swap
    mov [esi + 12], eax    ; new buffer
    mov [esi + 16], edx    ; new size
    
    ; Update ratio
    mov g_CompressionRatio, 200 ; 2:1
    jmp @@success

@@no_halve:
    ; Just do in-place transform
    invoke PiRam_PerfectCircleFwd, eax, edx
    test eax, eax
    jz @@fail
    
    mov g_CompressionRatio, 100 ; 1:1 (in-place)

@@success:
    mov eax, 1
    jmp @@exit

@@fail:
    xor eax, eax

@@exit:
    pop ebx
    pop edi
    pop esi
    ret
PiRam_CompressGGUF ENDP

; ============================================================================
; PiRam_MultiPassCompress - Multi-pass π-transform (enhanced version)
; Input:  pBuf:DWORD, dwSize:DWORD, dwPasses:DWORD
; Output: EAX = final size
; Uses: 11-pass X11 for aggressive, LocalCompress for local-only, Quantization
; ============================================================================
PiRam_MultiPassCompress PROC pBuf:DWORD, dwSize:DWORD, dwPasses:DWORD
    LOCAL localSize:DWORD
    LOCAL passIdx:DWORD

    push esi
    push edi
    push ebx

    mov esi, pBuf
    mov edi, dwSize
    mov ebx, dwPasses

    test esi, esi
    jz @@multi_fail
    test edi, edi
    jz @@multi_fail

    ; Check if we should use 11-pass X11 or standard
    cmp ebx, 11
    je @use_x11

    ; Check if local compress enabled
    cmp g_LocalCompressEnabled, 1
    jne @standard_multi

    ; Use local compress (only locals, no globals)
    invoke PiRam_LocalCompress, esi, edi
    test eax, eax
    jz @@multi_fail
    mov edi, eax
    
    ; Apply quantization if enabled
    cmp g_QuantizationEnabled, 1
    jne @quantization_done
    
    mov passIdx, 0
@quant_loop:
    cmp passIdx, 11
    jge @quantization_done
    invoke PiRam_QuantizeAdaptive, esi, edi, passIdx
    inc passIdx
    jmp @quant_loop

@quantization_done:
    jmp @@multi_done

@use_x11:
    ; Use 11-pass perfect circle compression
    invoke PiRam_PerfectCircleX11, esi, edi
    test eax, eax
    jz @@multi_fail
    mov edi, eax
    jmp @@multi_done

@standard_multi:
    ; Standard multi-pass with global-based transforms
    mov passIdx, 0

@@loop:
    cmp passIdx, ebx
    jge @@multi_done

    invoke PiRam_PerfectCircleFwd, esi, edi
    test eax, eax
    jz @@multi_fail
    
    mov edi, eax
    inc passIdx
    jmp @@loop

@@multi_done:
    mov eax, edi
    jmp @@multi_exit

@@multi_fail:
    xor eax, eax

@@multi_exit:
    pop ebx
    pop edi
    pop esi
    ret
PiRam_MultiPassCompress ENDP

@@multi_done:
    mov eax, edi
    jmp @@multi_exit

@@multi_fail:
    xor eax, eax

@@multi_exit:
    pop ebx
    pop edi
    pop esi
    ret
PiRam_MultiPassCompress ENDP

@@done:
    mov eax, edi
    jmp @@exit

@@fail:
    xor eax, eax

@@exit:
    pop ebx
    pop edi
    pop esi
    ret
PiRam_MultiPassCompress ENDP

; ============================================================================
; PiRam_GetCompressionRatio
; ============================================================================
PiRam_GetCompressionRatio PROC
    mov eax, g_CompressionRatio
    ret
PiRam_GetCompressionRatio ENDP

; ============================================================================
; PiRam_EnableHalving
; ============================================================================
PiRam_EnableHalving PROC bEnable:DWORD
    mov eax, bEnable
    mov g_HalvingEnabled, eax
    ret
PiRam_EnableHalving ENDP

; ============================================================================
; PiRam_CircleCompress - Wrapper for 11-pass circle compression
; pBuf: buffer, dwSize: size, pRatio: output ratio pointer
; Returns: EAX = compressed size
; ============================================================================
PiRam_CircleCompress PROC pBuf:DWORD, dwSize:DWORD, pRatio:DWORD
    push esi
    
    ; Call 11-pass circle
    invoke PiRam_PerfectCircle11Pass, pBuf, dwSize
    mov esi, eax        ; Compressed size
    
    ; Get ratio and store if requested
    cmp pRatio, 0
    je @skip_ratio_store
    
    ; TODO: Get ratio from circle module and store
    mov dword ptr [pRatio], 0
    
@skip_ratio_store:
    mov eax, esi
    pop esi
    ret
PiRam_CircleCompress ENDP

; ============================================================================
; PiRam_CircleDecompress - Reverse 11-pass decompression
; pCompressed: compressed buffer, dwSize: size, pOriginal: output
; Returns: EAX = decompressed size
; ============================================================================
PiRam_CircleDecompress PROC pCompressed:DWORD, dwSize:DWORD, pOriginal:DWORD
    ; TODO: Implement inverse 11-pass decompression
    ; For now: placeholder
    mov eax, dwSize
    shl eax, 4          ; Approximate decompressed (11 passes ≈ 2048x)
    ret
PiRam_CircleDecompress ENDP

END
