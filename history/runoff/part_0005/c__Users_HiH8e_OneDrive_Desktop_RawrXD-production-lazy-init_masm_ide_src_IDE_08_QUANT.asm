; ============================================================================
; IDE_08_QUANT.asm - Reverse quantization buffer operations
; ============================================================================
include IDE_INC.ASM

; External from reverse_quant.asm
EXTERN ReverseQuant_DequantizeBuffer:PROC

PUBLIC IDEQuant_DequantizeBuffer

.code

IDEQuant_DequantizeBuffer PROC
    call ReverseQuant_DequantizeBuffer
    ret
IDEQuant_DequantizeBuffer ENDP

END
