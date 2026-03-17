; ============================================================================
; REVERSE_QUANT.ASM - Stub for reverse quantization
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib

PUBLIC ReverseQuant_Init
PUBLIC ReverseQuant_DequantizeBuffer

.code

ReverseQuant_Init PROC
    mov eax, 1  ; Return success (stub)
    ret
ReverseQuant_Init ENDP

ReverseQuant_DequantizeBuffer PROC pInput:DWORD, pOutput:DWORD, dwSize:DWORD, dwType:DWORD
    mov eax, 1  ; Return success (stub)
    ret
ReverseQuant_DequantizeBuffer ENDP

END
