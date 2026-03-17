; pifabric_memory_compact.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC MemoryCompact_Init
PUBLIC MemoryCompact_Run
PUBLIC MemoryCompact_Clear

.data
MAX_BLOCKS EQU 128

Block STRUCT
    addr DWORD ?
    size DWORD ?
Block ENDS

MemoryCompact STRUCT
    count DWORD ?
    blocks Block MAX_BLOCKS DUP(<0,0>)
MemoryCompact ENDS

g_Compact MemoryCompact <0>

.code

MemoryCompact_Init PROC
    mov g_Compact.count,0
    mov eax,1
    ret
MemoryCompact_Init ENDP

MemoryCompact_Run PROC
    ; simplistic compaction: reset count
    mov g_Compact.count,0
    mov eax,1
    ret
MemoryCompact_Run ENDP

MemoryCompact_Clear PROC
    mov g_Compact.count,0
    mov eax,1
    ret
MemoryCompact_Clear ENDP

END