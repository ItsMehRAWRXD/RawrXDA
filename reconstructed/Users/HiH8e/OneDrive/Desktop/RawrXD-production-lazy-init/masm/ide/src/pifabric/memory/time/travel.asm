; pifabric_memory_time_travel.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC MemoryTimeTravel_Init
PUBLIC MemoryTimeTravel_Rewind
PUBLIC MemoryTimeTravel_FastForward
PUBLIC MemoryTimeTravel_Clear

.data
MAX_HISTORY EQU 64

HistoryEntry STRUCT
    tick DWORD ?
    addr DWORD ?
    size DWORD ?
HistoryEntry ENDS

MemoryHistory STRUCT
    count DWORD ?
    entries HistoryEntry MAX_HISTORY DUP(<0,0,0>)
MemoryHistory ENDS

g_History MemoryHistory <0>

.code

MemoryTimeTravel_Init PROC
    mov g_History.count,0
    mov eax,1
    ret
MemoryTimeTravel_Init ENDP

MemoryTimeTravel_Rewind PROC ticks:DWORD
    ; simplistic rewind: decrement count
    mov eax,g_History.count
    sub eax,ticks
    cmp eax,0
    jl @zero
    mov g_History.count,eax
    mov eax,1
    ret
@zero:
    mov g_History.count,0
    xor eax,eax
    ret
MemoryTimeTravel_Rewind ENDP

MemoryTimeTravel_FastForward PROC ticks:DWORD
    ; simplistic fast forward: increment count
    mov eax,g_History.count
    add eax,ticks
    cmp eax,MAX_HISTORY
    jae @max
    mov g_History.count,eax
    mov eax,1
    ret
@max:
    mov g_History.count,MAX_HISTORY
    xor eax,eax
    ret
MemoryTimeTravel_FastForward ENDP

MemoryTimeTravel_Clear PROC
    mov g_History.count,0
    mov eax,1
    ret
MemoryTimeTravel_Clear ENDP

END