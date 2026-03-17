; pifabric_gguf_memory_map.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC GgufMemoryMap_Init
PUBLIC GgufMemoryMap_AddRegion
PUBLIC GgufMemoryMap_Clear
PUBLIC GgufMemoryMap_FindRegion

.data
MAX_REGIONS EQU 128

Region STRUCT
    base DWORD ?
    size DWORD ?
Region ENDS

GgufMemoryMap STRUCT
    count DWORD ?
    regions Region MAX_REGIONS DUP(<0,0>)
GgufMemoryMap ENDS

g_MemMap GgufMemoryMap <0>

.code

GgufMemoryMap_Init PROC
    mov g_MemMap.count,0
    mov eax,1
    ret
GgufMemoryMap_Init ENDP

GgufMemoryMap_AddRegion PROC base:DWORD,size:DWORD
    mov eax,g_MemMap.count
    cmp eax,MAX_REGIONS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Region
    mov edx,OFFSET g_MemMap.regions
    add edx,ecx
    mov [edx].Region.base,base
    mov [edx].Region.size,size
    inc g_MemMap.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
GgufMemoryMap_AddRegion ENDP

GgufMemoryMap_Clear PROC
    mov g_MemMap.count,0
    mov eax,1
    ret
GgufMemoryMap_Clear ENDP

GgufMemoryMap_FindRegion PROC base:DWORD
    mov ecx,g_MemMap.count
    mov edx,OFFSET g_MemMap.regions
@loop:
    test ecx,ecx
    jz @fail
    mov eax,[edx].Region.base
    cmp eax,base
    je @found
    add edx,SIZEOF Region
    dec ecx
    jmp @loop
@found:
    mov eax,edx
    ret
@fail:
    xor eax,eax
    ret
GgufMemoryMap_FindRegion ENDP

END