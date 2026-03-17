; pifabric_memory_compact.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc

PUBLIC  PiFabric_Compact

.code

PiFabric_Compact PROC
    mov eax, 1
    ret
PiFabric_Compact ENDP

END