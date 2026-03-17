; pifabric_orchestra_sessions.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC OrchestraSessions_Init
PUBLIC OrchestraSessions_Add
PUBLIC OrchestraSessions_GetCount
PUBLIC OrchestraSessions_Clear

.data
MAX_SESSIONS EQU 32

Session STRUCT
    id   DWORD ?
    state DWORD ?
Session ENDS

Sessions STRUCT
    count DWORD ?
    items Session MAX_SESSIONS DUP(<0,0>)
Sessions ENDS

g_Sessions Sessions <0>

.code

OrchestraSessions_Init PROC
    mov g_Sessions.count,0
    mov eax,1
    ret
OrchestraSessions_Init ENDP

OrchestraSessions_Add PROC id:DWORD,state:DWORD
    mov eax,g_Sessions.count
    cmp eax,MAX_SESSIONS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Session
    mov edx,OFFSET g_Sessions.items
    add edx,ecx
    mov [edx].Session.id,id
    mov [edx].Session.state,state
    inc g_Sessions.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
OrchestraSessions_Add ENDP

OrchestraSessions_GetCount PROC
    mov eax,g_Sessions.count
    ret
OrchestraSessions_GetCount ENDP

OrchestraSessions_Clear PROC
    mov g_Sessions.count,0
    mov eax,1
    ret
OrchestraSessions_Clear ENDP

END