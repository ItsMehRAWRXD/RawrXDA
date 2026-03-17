; pifabric_compression_replay.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC CompressionReplay_Init
PUBLIC CompressionReplay_AddEvent
PUBLIC CompressionReplay_Play
PUBLIC CompressionReplay_Clear

.data
MAX_EVENTS EQU 64

ReplayEvent STRUCT
    tick DWORD ?
    id   DWORD ?
ReplayEvent ENDS

Replay STRUCT
    count DWORD ?
    events ReplayEvent MAX_EVENTS DUP(<0,0>)
Replay ENDS

g_Replay Replay <0>

.code

CompressionReplay_Init PROC
    mov g_Replay.count,0
    mov eax,1
    ret
CompressionReplay_Init ENDP

CompressionReplay_AddEvent PROC tick:DWORD,id:DWORD
    mov eax,g_Replay.count
    cmp eax,MAX_EVENTS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF ReplayEvent
    mov edx,OFFSET g_Replay.events
    add edx,ecx
    mov [edx].ReplayEvent.tick,tick
    mov [edx].ReplayEvent.id,id
    inc g_Replay.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
CompressionReplay_AddEvent ENDP

CompressionReplay_Play PROC
    mov eax,g_Replay.count
    ret
CompressionReplay_Play ENDP

CompressionReplay_Clear PROC
    mov g_Replay.count,0
    mov eax,1
    ret
CompressionReplay_Clear ENDP

END