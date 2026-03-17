; ============================================================================
; IDE_06_GGUF_LOADER.asm - GGUF unified loader and disk streaming
; ============================================================================
include IDE_INC.ASM

; External from gguf_loader_unified.asm and gguf_disk_streaming.asm
EXTERN GgufUnified_LoadModelAutomatic:PROC
EXTERN GgufUnified_LoadModel:PROC
EXTERN DiscStream_OpenModel:PROC
EXTERN DiscStream_ReadChunk:PROC

PUBLIC IDEGguf_LoadModelAutomatic
PUBLIC IDEGguf_LoadModel
PUBLIC IDEDiscStream_OpenModel
PUBLIC IDEDiscStream_ReadChunk

.code

IDEGguf_LoadModelAutomatic PROC
    call GgufUnified_LoadModelAutomatic
    ret
IDEGguf_LoadModelAutomatic ENDP

IDEGguf_LoadModel PROC pPath:DWORD
    push pPath
    call GgufUnified_LoadModel
    ret
IDEGguf_LoadModel ENDP

IDEDiscStream_OpenModel PROC pPath:DWORD
    push pPath
    call DiscStream_OpenModel
    ret
IDEDiscStream_OpenModel ENDP

IDEDiscStream_ReadChunk PROC hStream:DWORD
    push hStream
    call DiscStream_ReadChunk
    ret
IDEDiscStream_ReadChunk ENDP

END
