; pifabric_memory_profiler.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC MemoryProfiler_Init
PUBLIC MemoryProfiler_Record
PUBLIC MemoryProfiler_Report
PUBLIC MemoryProfiler_Clear

.data
MAX_RECORDS EQU 256

MemRecord STRUCT
    addr DWORD ?
    size DWORD ?
MemRecord ENDS

MemoryProfile STRUCT
    count DWORD ?
    records MemRecord MAX_RECORDS DUP(<0,0>)
MemoryProfile ENDS

g_Profile MemoryProfile <0>

.code

MemoryProfiler_Init PROC
    mov g_Profile.count,0
    mov eax,1
    ret
MemoryProfiler_Init ENDP

MemoryProfiler_Record PROC addr:DWORD,size:DWORD
    mov eax,g_Profile.count
    cmp eax,MAX_RECORDS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF MemRecord
    mov edx,OFFSET g_Profile.records
    add edx,ecx
    mov [edx].MemRecord.addr,addr
    mov [edx].MemRecord.size,size
    inc g_Profile.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
MemoryProfiler_Record ENDP

MemoryProfiler_Report PROC
    mov eax,g_Profile.count
    ret
MemoryProfiler_Report ENDP

MemoryProfiler_Clear PROC
    mov g_Profile.count,0
    mov eax,1
    ret
MemoryProfiler_Clear ENDP

END