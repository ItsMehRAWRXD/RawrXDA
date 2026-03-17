; pifabric_ai_training.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC AiTraining_Init
PUBLIC AiTraining_Start
PUBLIC AiTraining_Stop
PUBLIC AiTraining_Status

.data
g_bTraining DWORD 0

.code

AiTraining_Init PROC
    mov g_bTraining,0
    mov eax,1
    ret
AiTraining_Init ENDP

AiTraining_Start PROC
    mov g_bTraining,1
    mov eax,1
    ret
AiTraining_Start ENDP

AiTraining_Stop PROC
    mov g_bTraining,0
    mov eax,1
    ret
AiTraining_Stop ENDP

AiTraining_Status PROC
    mov eax,g_bTraining
    ret
AiTraining_Status ENDP

END