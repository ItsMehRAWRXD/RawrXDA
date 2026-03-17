; pifabric_orchestra_rehearsal.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC OrchestraRehearsal_Init
PUBLIC OrchestraRehearsal_AddStep
PUBLIC OrchestraRehearsal_GetCount
PUBLIC OrchestraRehearsal_Clear

.data
MAX_STEPS EQU 64

Step STRUCT
    id   DWORD ?
    action DWORD ?
Step ENDS

Rehearsal STRUCT
    count DWORD ?
    steps Step MAX_STEPS DUP(<0,0>)
Rehearsal ENDS

g_Rehearsal Rehearsal <0>

.code

OrchestraRehearsal_Init PROC
    mov g_Rehearsal.count,0
    mov eax,1
    ret
OrchestraRehearsal_Init ENDP

OrchestraRehearsal_AddStep PROC id:DWORD,action:DWORD
    mov eax,g_Rehearsal.count
    cmp eax,MAX_STEPS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Step
    mov edx,OFFSET g_Rehearsal.steps
    add edx,ecx
    mov [edx].Step.id,id
    mov [edx].Step.action,action
    inc g_Rehearsal.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
OrchestraRehearsal_AddStep ENDP

OrchestraRehearsal_GetCount PROC
    mov eax,g_Rehearsal.count
    ret
OrchestraRehearsal_GetCount ENDP

OrchestraRehearsal_Clear PROC
    mov g_Rehearsal.count,0
    mov eax,1
    ret
OrchestraRehearsal_Clear ENDP

END