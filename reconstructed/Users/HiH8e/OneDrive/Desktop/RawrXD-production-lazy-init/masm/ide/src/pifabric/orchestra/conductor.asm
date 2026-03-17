; pifabric_orchestra_conductor.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC OrchestraConductor_Init
PUBLIC OrchestraConductor_AddCue
PUBLIC OrchestraConductor_Run
PUBLIC OrchestraConductor_Clear

.data
MAX_CUES EQU 64

Cue STRUCT
    id   DWORD ?
    action DWORD ?
Cue ENDS

Conductor STRUCT
    count DWORD ?
    cues Cue MAX_CUES DUP(<0,0>)
Conductor ENDS

g_Conductor Conductor <0>

.code

OrchestraConductor_Init PROC
    mov g_Conductor.count,0
    mov eax,1
    ret
OrchestraConductor_Init ENDP

OrchestraConductor_AddCue PROC id:DWORD,action:DWORD
    mov eax,g_Conductor.count
    cmp eax,MAX_CUES
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Cue
    mov edx,OFFSET g_Conductor.cues
    add edx,ecx
    mov [edx].Cue.id,id
    mov [edx].Cue.action,action
    inc g_Conductor.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
OrchestraConductor_AddCue ENDP

OrchestraConductor_Run PROC
    mov eax,g_Conductor.count
    ret
OrchestraConductor_Run ENDP

OrchestraConductor_Clear PROC
    mov g_Conductor.count,0
    mov eax,1
    ret
OrchestraConductor_Clear ENDP

END
