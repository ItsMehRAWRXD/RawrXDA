; ============================================================================
; IDE_09_INFER.asm - Inference backend context creation
; ============================================================================
include IDE_INC.ASM

; External from inferenceBackend_init.asm
EXTERN InferenceBackend_CreateInferenceContext:PROC

PUBLIC IDEInfer_CreateInferenceContext

.code

IDEInfer_CreateInferenceContext PROC hModel:DWORD
    push hModel
    call InferenceBackend_CreateInferenceContext
    ret
IDEInfer_CreateInferenceContext ENDP

END
