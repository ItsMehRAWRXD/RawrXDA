; pifabric_ai_optimizer.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC AiOptimizer_Init
PUBLIC AiOptimizer_SetParam
PUBLIC AiOptimizer_GetParam
PUBLIC AiOptimizer_Clear

.data
MAX_PARAMS EQU 16

Param STRUCT
    id DWORD ?
    value DWORD ?
Param ENDS

Optimizer STRUCT
    count DWORD ?
    params Param MAX_PARAMS DUP(<0,0>)
Optimizer ENDS

g_Optimizer Optimizer <0>

.code

AiOptimizer_Init PROC
    mov g_Optimizer.count,0
    mov eax,1
    ret
AiOptimizer_Init ENDP

AiOptimizer_SetParam PROC id:DWORD,value:DWORD
    mov eax,g_Optimizer.count
    cmp eax,MAX_PARAMS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Param
    mov edx,OFFSET g_Optimizer.params
    add edx,ecx
    mov [edx].Param.id,id
    mov [edx].Param.value,value
    inc g_Optimizer.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
AiOptimizer_SetParam ENDP

AiOptimizer_GetParam PROC id:DWORD
    mov ecx,g_Optimizer.count
    mov edx,OFFSET g_Optimizer.params
@loop:
    test ecx,ecx
    jz @fail
    mov eax,[edx].Param.id
    cmp eax,id
    je @found
    add edx,SIZEOF Param
    dec ecx
    jmp @loop
@found:
    mov eax,[edx].Param.value
    ret
@fail:
    xor eax,eax
    ret
AiOptimizer_GetParam ENDP

AiOptimizer_Clear PROC
    mov g_Optimizer.count,0
    mov eax,1
    ret
AiOptimizer_Clear ENDP

END