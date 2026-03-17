; ============================================================================
; IDE_07_PIRAM.asm - PIRAM tensor compression hooks
; ============================================================================
include IDE_INC.ASM

; External from piram_hooks.asm
EXTERN PiramHooks_CompressTensor:PROC
EXTERN PiramHooks_DecompressTensor:PROC

PUBLIC IDEPiram_CompressTensor
PUBLIC IDEPiram_DecompressTensor

.code

IDEPiram_CompressTensor PROC
    call PiramHooks_CompressTensor
    ret
IDEPiram_CompressTensor ENDP

IDEPiram_DecompressTensor PROC
    call PiramHooks_DecompressTensor
    ret
IDEPiram_DecompressTensor ENDP

END
