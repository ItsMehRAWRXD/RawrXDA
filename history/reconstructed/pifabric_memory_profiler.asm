; pifabric_memory_profiler.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc

PUBLIC  MemoryProfiler_Start
PUBLIC  MemoryProfiler_Stop
PUBLIC  MemoryProfiler_Report

.code

MemoryProfiler_Start PROC
    mov eax, 1
    ret
MemoryProfiler_Start ENDP

MemoryProfiler_Stop PROC
    mov eax, 1
    ret
MemoryProfiler_Stop ENDP

MemoryProfiler_Report PROC
    mov eax, 1
    ret
MemoryProfiler_Report ENDP

END