; pifabric_memory_snapshot.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC MemorySnapshot_Init
PUBLIC MemorySnapshot_Take
PUBLIC MemorySnapshot_Restore
PUBLIC MemorySnapshot_Clear

.data
MAX_SNAPSHOTS EQU 32

Snapshot STRUCT
    id   DWORD ?
    addr DWORD ?
    size DWORD ?
Snapshot ENDS

MemorySnapshots STRUCT
    count DWORD ?
    snaps Snapshot MAX_SNAPSHOTS DUP(<0,0,0>)
MemorySnapshots ENDS

g_Snapshots MemorySnapshots <0>

.code

MemorySnapshot_Init PROC
    mov g_Snapshots.count,0
    mov eax,1
    ret
MemorySnapshot_Init ENDP

MemorySnapshot_Take PROC id:DWORD,addr:DWORD,size:DWORD
    mov eax,g_Snapshots.count
    cmp eax,MAX_SNAPSHOTS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Snapshot
    mov edx,OFFSET g_Snapshots.snaps
    add edx,ecx
    mov [edx].Snapshot.id,id
    mov [edx].Snapshot.addr,addr
    mov [edx].Snapshot.size,size
    inc g_Snapshots.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
MemorySnapshot_Take ENDP

MemorySnapshot_Restore PROC id:DWORD
    mov ecx,g_Snapshots.count
    mov edx,OFFSET g_Snapshots.snaps
@loop:
    test ecx,ecx
    jz @fail
    mov eax,[edx].Snapshot.id
    cmp eax,id
    je @found
    add edx,SIZEOF Snapshot
    dec ecx
    jmp @loop
@found:
    mov eax,edx
    ret
@fail:
    xor eax,eax
    ret
MemorySnapshot_Restore ENDP

MemorySnapshot_Clear PROC
    mov g_Snapshots.count,0
    mov eax,1
    ret
MemorySnapshot_Clear ENDP

END