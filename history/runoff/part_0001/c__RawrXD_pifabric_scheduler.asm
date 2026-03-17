; pifabric_scheduler.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc

PUBLIC  PiFabric_ScheduleTick
PUBLIC  PiFabric_ScheduleModel
PUBLIC  PiFabric_ScheduleReset

PiFabricScheduleEntry STRUCT
    hFabric     DWORD ?
    dwPriority  DWORD ?
    dwFlags     DWORD ?
PiFabricScheduleEntry ENDS

.data
g_ScheduleTable   PiFabricScheduleEntry 32 DUP(<0,0,0>)
g_dwScheduleCount dd 0

.code

PiFabric_ScheduleModel PROC hFabric:DWORD, dwPriority:DWORD
    mov eax, g_dwScheduleCount
    cmp eax, 32
    jae @full
    mov ecx, SIZEOF PiFabricScheduleEntry
    mov edx, OFFSET g_ScheduleTable
    imul eax, ecx
    add edx, eax
    mov [edx].PiFabricScheduleEntry.hFabric,   hFabric
    mov [edx].PiFabricScheduleEntry.dwPriority,dwPriority
    mov [edx].PiFabricScheduleEntry.dwFlags,   0
    inc g_dwScheduleCount
    mov eax, 1
    ret
@full:
    xor eax, eax
    ret
PiFabric_ScheduleModel ENDP

PiFabric_ScheduleTick PROC
    xor eax, eax
    ret
PiFabric_ScheduleTick ENDP

PiFabric_ScheduleReset PROC
    mov g_dwScheduleCount, 0
    mov eax, 1
    ret
PiFabric_ScheduleReset ENDP

END