; ============================================================================
; PIRAM_HOOKS.ASM - Stub for compression hooks
; ============================================================================

.386
.model flat, C
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib

PUBLIC PiramHooks_CompressTensor
PUBLIC PiramHooks_DecompressTensor

.code

PiramHooks_CompressTensor PROC pTensor:DWORD, pOut:DWORD, dwSize:DWORD
    mov eax, 1  ; Return success (stub)
    ret
PiramHooks_CompressTensor ENDP

PiramHooks_DecompressTensor PROC pCompressed:DWORD, pOut:DWORD, dwSize:DWORD
    mov eax, 1  ; Return success (stub)
    ret
PiramHooks_DecompressTensor ENDP

END
