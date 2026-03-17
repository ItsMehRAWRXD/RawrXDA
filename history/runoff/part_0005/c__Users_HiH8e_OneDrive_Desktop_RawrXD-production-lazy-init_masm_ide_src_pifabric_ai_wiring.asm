; pifabric_ai_wiring.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

EXTERN AiOptions_Set:PROC
EXTERN AiTraining_Start:PROC
EXTERN AiScheduler_AddJob:PROC
EXTERN AiOptimizer_SetParam:PROC
EXTERN AiDataset_Add:PROC

PUBLIC AiWiring_Init
PUBLIC AiWiring_ConnectAll

.code

AiWiring_Init PROC
    ; initialize AI subsystems with defaults
    invoke AiOptions_Set,1,1
    invoke AiTraining_Start
    invoke AiScheduler_AddJob,1,10
    invoke AiOptimizer_SetParam,1,100
    invoke AiDataset_Add,1,1024
    mov eax,1
    ret
AiWiring_Init ENDP

AiWiring_ConnectAll PROC
    ; glue logic to tie AI modules together
    mov eax,1
    ret
AiWiring_ConnectAll ENDP

END