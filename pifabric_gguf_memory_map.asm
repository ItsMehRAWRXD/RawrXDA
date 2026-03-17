; pifabric_gguf_memory_map.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC  GGUF_MemoryMap_Load
PUBLIC  GGUF_MemoryMap_Unmap

.code

GGUF_MemoryMap_Load PROC USES esi edi lpPath:DWORD
    ; Memory map GGUF file (stub)
    mov eax, 1
    ret
GGUF_MemoryMap_Load ENDP

GGUF_MemoryMap_Unmap PROC USES esi edi pBase:DWORD
    ; Unmap memory (stub)
    mov eax, 1
    ret
GGUF_MemoryMap_Unmap ENDP

END