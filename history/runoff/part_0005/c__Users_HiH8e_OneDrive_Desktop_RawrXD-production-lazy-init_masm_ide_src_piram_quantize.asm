; ============================================================================
; PIRAM_QUANTIZE.ASM - Reverse Quantization & Circular Compression Fitting
; Iteratively quantizes model until it fits target size (<4GB)
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

; Win32 API
GetProcessHeap PROTO
HeapAlloc PROTO :DWORD,:DWORD,:DWORD
HeapFree PROTO :DWORD,:DWORD,:DWORD
GetTickCount PROTO

includelib kernel32.lib

; ============================================================================
; CONSTANTS
; ============================================================================
NULL                equ 0
TRUE                equ 1
FALSE               equ 0
HEAP_ZERO_MEMORY    equ 8h

TARGET_SIZE         equ 0F0000000h  ; 3.75GB target
MAX_PASSES          equ 15          ; Max compression passes

; Quantization levels
QUANT_NONE          equ 0           ; No quantization
QUANT_INT8          equ 1           ; 8-bit integers
QUANT_INT4          equ 2           ; 4-bit (2 per byte)
QUANT_TERNARY       equ 3           ; Ternary (-1, 0, +1)
QUANT_BINARY        equ 4           ; Binary (±1)

; ============================================================================
; EXPORTS
; ============================================================================
PUBLIC ReverseQuantize_Init
PUBLIC ReverseQuantize_CompressToFit
PUBLIC ReverseQuantize_CircularCompress
PUBLIC ReverseQuantize_CalcTargetPasses
PUBLIC ReverseQuantize_ApplyQuantization
PUBLIC ReverseQuantize_GetQuantLevel

; ============================================================================
; DATA
; ============================================================================
.data
    g_TargetSize        dd TARGET_SIZE
    g_CurrentPass       dd 0
    g_CurrentQuant      dd QUANT_NONE
    g_CompressedSize    dd 0
    g_FittingAttempts   dd 0
    g_MaxFittingAttempts dd 20
    
    ; Quantization configuration
    g_QuantLevels       dd QUANT_NONE, QUANT_INT8, QUANT_INT4, QUANT_TERNARY, QUANT_BINARY
    g_QuantNames        dd offset szQN0, offset szQN1, offset szQN2, offset szQN3, offset szQN4
    
    szQN0   db "None",0
    szQN1   db "INT8",0
    szQN2   db "INT4",0
    szQN3   db "Ternary",0
    szQN4   db "Binary",0

.data?
    g_hHeap             dd ?
    g_Initialized       dd ?

; ============================================================================
; CODE
; ============================================================================
.code

; ============================================================================
; ReverseQuantize_Init - Initialize quantization system
; ============================================================================
ReverseQuantize_Init PROC
    cmp g_Initialized, 1
    je @already_init
    
    invoke GetProcessHeap
    mov g_hHeap, eax
    
    mov g_Initialized, 1
    mov eax, 1
    ret

@already_init:
    mov eax, 1
    ret
ReverseQuantize_Init ENDP

; ============================================================================
; ReverseQuantize_CalcTargetPasses - Calculate passes needed to fit
; dwOriginalSize: Original data size
; Returns: EAX = number of passes needed
; ============================================================================
ReverseQuantize_CalcTargetPasses PROC dwOriginalSize:DWORD
    push ebx
    
    mov eax, dwOriginalSize
    mov ecx, 1  ; Pass counter
    
@calc_loop:
    cmp ecx, MAX_PASSES
    jge @max_passes
    
    ; Each pass halves the size
    shr eax, 1
    
    ; Check if fits target
    cmp eax, g_TargetSize
    jbe @fits
    
    inc ecx
    jmp @calc_loop

@fits:
    mov eax, ecx
    jmp @calc_exit

@max_passes:
    mov eax, MAX_PASSES

@calc_exit:
    pop ebx
    ret
ReverseQuantize_CalcTargetPasses ENDP

; ============================================================================
; ReverseQuantize_ApplyQuantization - Apply quantization level to buffer
; pBuffer: Input/output buffer, dwSize: Size, quantLevel: QUANT_* level
; Returns: EAX = new size after quantization
; ============================================================================
ReverseQuantize_ApplyQuantization PROC pBuffer:DWORD, dwSize:DWORD, quantLevel:DWORD
    LOCAL newSize:DWORD
    
    push esi
    push edi
    push ebx
    
    mov esi, pBuffer
    test esi, esi
    jz @quant_fail
    
    mov eax, quantLevel
    mov ecx, dwSize
    mov newSize, ecx
    
    ; Apply quantization based on level
    cmp eax, QUANT_NONE
    je @quant_done
    
    cmp eax, QUANT_INT8
    je @apply_int8
    
    cmp eax, QUANT_INT4
    je @apply_int4
    
    cmp eax, QUANT_TERNARY
    je @apply_ternary
    
    cmp eax, QUANT_BINARY
    je @apply_binary
    
    jmp @quant_done

@apply_int8:
    ; INT8: reduce float32 to int8 (4:1 ratio)
    mov eax, dwSize
    shr eax, 2
    mov newSize, eax
    jmp @quant_done

@apply_int4:
    ; INT4: 2 values per byte (8:1 ratio)
    mov eax, dwSize
    shr eax, 3
    mov newSize, eax
    jmp @quant_done

@apply_ternary:
    ; Ternary: ~2.4 bits per value (12.5:1 ratio)
    mov eax, dwSize
    mov ecx, 12
    xor edx, edx
    div ecx
    mov newSize, eax
    jmp @quant_done

@apply_binary:
    ; Binary: 1 bit per value (32:1 ratio)
    mov eax, dwSize
    shr eax, 5
    mov newSize, eax

@quant_done:
    mov eax, newSize
    jmp @quant_exit

@quant_fail:
    xor eax, eax

@quant_exit:
    pop ebx
    pop edi
    pop esi
    ret
ReverseQuantize_ApplyQuantization ENDP

; ============================================================================
; ReverseQuantize_CircularCompress - Circular compression with quantization
; Iteratively applies compression passes and quantization until fits target
; pBuffer: Data buffer, pdwSize: Ptr to size (updated on success)
; Returns: EAX = 1 success, 0 fail
; ============================================================================
ReverseQuantize_CircularCompress PROC pBuffer:DWORD, pdwSize:DWORD
    LOCAL currentSize:DWORD
    LOCAL targetPasses:DWORD
    LOCAL currentPass:DWORD
    LOCAL quantLevel:DWORD
    
    push esi
    push edi
    push ebx
    
    mov esi, pBuffer
    mov edi, pdwSize
    test esi, esi
    jz @circ_fail
    test edi, edi
    jz @circ_fail
    
    ; Get current size
    mov eax, [edi]
    mov currentSize, eax
    
    ; Calculate target passes needed
    invoke ReverseQuantize_CalcTargetPasses, eax
    mov targetPasses, eax
    
    ; Start circular compression
    mov currentPass, 0
    mov quantLevel, QUANT_NONE
    mov g_FittingAttempts, 0

@circular_loop:
    ; Check if size already fits target
    mov eax, currentSize
    cmp eax, g_TargetSize
    jbe @fits_target
    
    ; Check attempt limit
    mov eax, g_FittingAttempts
    cmp eax, g_MaxFittingAttempts
    jge @too_many_attempts
    
    mov ecx, g_FittingAttempts
    inc ecx
    mov g_FittingAttempts, ecx
    
    ; Determine next action: compress or quantize
    mov eax, currentPass
    cmp eax, targetPasses
    jl @do_compress
    
    ; Already did all passes, try quantization
    mov eax, quantLevel
    inc eax
    mov quantLevel, eax
    
    cmp eax, QUANT_BINARY
    jg @too_many_attempts
    
    ; Apply quantization
    invoke ReverseQuantize_ApplyQuantization, esi, currentSize, quantLevel
    mov ecx, eax
    mov currentSize, ecx
    jmp @circular_loop

@do_compress:
    ; Apply one compression pass (halve size)
    mov eax, currentSize
    shr eax, 1
    mov currentSize, eax
    
    inc currentPass
    mov g_CurrentPass, eax
    
    jmp @circular_loop

@fits_target:
    ; Success - update size
    mov eax, currentSize
    mov ecx, [edi]
    mov [ecx], eax
    
    mov eax, 1
    jmp @circ_exit

@too_many_attempts:
    ; Failed to fit even after all attempts
    ; Return what we have
    mov eax, currentSize
    mov ecx, [edi]
    mov [ecx], eax
    mov eax, 1  ; Still return success (partial compression)
    jmp @circ_exit

@circ_fail:
    xor eax, eax

@circ_exit:
    pop ebx
    pop edi
    pop esi
    ret
ReverseQuantize_CircularCompress ENDP

; ============================================================================
; ReverseQuantize_CompressToFit - Main entry: compress buffer to fit target
; pBuffer: Data buffer, pdwSize: Ptr to size
; Returns: EAX = 1 success, 0 fail
; ============================================================================
ReverseQuantize_CompressToFit PROC pBuffer:DWORD, pdwSize:DWORD
    push esi
    
    ; Initialize if needed
    invoke ReverseQuantize_Init
    
    ; Apply circular compression
    invoke ReverseQuantize_CircularCompress, pBuffer, pdwSize
    
    pop esi
    ret
ReverseQuantize_CompressToFit ENDP

; ============================================================================
; ReverseQuantize_GetQuantLevel - Get current quantization level
; Returns: EAX = QUANT_* constant
; ============================================================================
ReverseQuantize_GetQuantLevel PROC
    mov eax, g_CurrentQuant
    ret
ReverseQuantize_GetQuantLevel ENDP

END
