; pifabric_thread_pool.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC  PiFabric_ThreadPool_Init
PUBLIC  PiFabric_ThreadPool_Queue
PUBLIC  PiFabric_ThreadPool_Shutdown

PiFabricTask STRUCT
    pFunc   DWORD ?
    pCtx    DWORD ?
PiFabricTask ENDS

.data
g_hThreads      dd 0,0,0,0,0,0,0,0
g_dwThreadCount dd 0

.code

PiFabric_ThreadPool_Init PROC dwCount:DWORD
    mov eax, dwCount
    test eax, eax
    jz   @set1
    cmp eax, 8
    jbe  @have
@set1:
    mov eax, 4
@have:
    mov g_dwThreadCount, eax
    mov eax, 1
    ret
PiFabric_ThreadPool_Init ENDP

PiFabric_ThreadPool_Queue PROC pTask:DWORD
    mov eax, 1
    ret
PiFabric_ThreadPool_Queue ENDP

PiFabric_ThreadPool_Shutdown PROC
    xor eax, eax
    ret
PiFabric_ThreadPool_Shutdown ENDP

END