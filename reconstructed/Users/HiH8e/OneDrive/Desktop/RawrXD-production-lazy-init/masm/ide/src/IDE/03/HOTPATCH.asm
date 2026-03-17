; ============================================================================
; IDE_03_HOTPATCH.asm - HotPatch engine with heap management
; ============================================================================
include IDE_INC.ASM

; External from model_hotpatch_engine.asm
EXTERN HotPatch_Init:PROC
EXTERN HotPatch_RegisterModel:PROC
EXTERN HotPatch_SwapModel:PROC
EXTERN HotPatch_StreamedLoadModel:PROC
EXTERN HotPatch_SetStreamCap:PROC
EXTERN HotPatch_CacheModel:PROC
EXTERN HotPatch_WarmupModel:PROC

PUBLIC IDEHotPatch_Init
PUBLIC IDEHotPatch_RegisterModel
PUBLIC IDEHotPatch_SwapModel
PUBLIC IDEHotPatch_StreamedLoadModel
PUBLIC IDEHotPatch_SetStreamCap
PUBLIC IDEHotPatch_CacheModel
PUBLIC IDEHotPatch_WarmupModel

.code

IDEHotPatch_Init PROC
    call HotPatch_Init
    ret
IDEHotPatch_Init ENDP

IDEHotPatch_RegisterModel PROC hModel:DWORD
    push hModel
    call HotPatch_RegisterModel
    ret
IDEHotPatch_RegisterModel ENDP

IDEHotPatch_SwapModel PROC hOld:DWORD, hNew:DWORD
    push hNew
    push hOld
    call HotPatch_SwapModel
    ret
IDEHotPatch_SwapModel ENDP

IDEHotPatch_StreamedLoadModel PROC pPath:DWORD
    push pPath
    call HotPatch_StreamedLoadModel
    ret
IDEHotPatch_StreamedLoadModel ENDP

IDEHotPatch_SetStreamCap PROC dwCap:DWORD
    push dwCap
    call HotPatch_SetStreamCap
    ret
IDEHotPatch_SetStreamCap ENDP

IDEHotPatch_CacheModel PROC hModel:DWORD
    push hModel
    call HotPatch_CacheModel
    ret
IDEHotPatch_CacheModel ENDP

IDEHotPatch_WarmupModel PROC hModel:DWORD
    push hModel
    call HotPatch_WarmupModel
    ret
IDEHotPatch_WarmupModel ENDP

END
