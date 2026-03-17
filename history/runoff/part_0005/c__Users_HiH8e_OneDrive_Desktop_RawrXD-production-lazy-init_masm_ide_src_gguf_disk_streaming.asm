; ============================================================================
; GGUF_DISK_STREAMING.ASM - Stub for disk streaming
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib

PUBLIC DiscStream_OpenModel
PUBLIC DiscStream_ReadChunk

.code

DiscStream_OpenModel PROC pPath:DWORD
    xor eax, eax  ; Return NULL (stub)
    ret
DiscStream_OpenModel ENDP

DiscStream_ReadChunk PROC hStream:DWORD, pBuffer:DWORD, dwSize:DWORD
    xor eax, eax  ; Return 0 bytes read (stub)
    ret
DiscStream_ReadChunk ENDP

END
