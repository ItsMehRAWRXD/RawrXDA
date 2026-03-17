; pifabric_chain_scenarios.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC ChainScenarios_Init
PUBLIC ChainScenarios_Add
PUBLIC ChainScenarios_GetCount
PUBLIC ChainScenarios_Clear

.data
MAX_SCENARIOS EQU 32

Scenario STRUCT
    id   DWORD ?
    type DWORD ?
Scenario ENDS

Scenarios STRUCT
    count DWORD ?
    items Scenario MAX_SCENARIOS DUP(<0,0>)
Scenarios ENDS

g_Scenarios Scenarios <0>

.code

ChainScenarios_Init PROC
    mov g_Scenarios.count,0
    mov eax,1
    ret
ChainScenarios_Init ENDP

ChainScenarios_Add PROC id:DWORD,type:DWORD
    mov eax,g_Scenarios.count
    cmp eax,MAX_SCENARIOS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Scenario
    mov edx,OFFSET g_Scenarios.items
    add edx,ecx
    mov [edx].Scenario.id,id
    mov [edx].Scenario.type,type
    inc g_Scenarios.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
ChainScenarios_Add ENDP

ChainScenarios_GetCount PROC
    mov eax,g_Scenarios.count
    ret
ChainScenarios_GetCount ENDP

ChainScenarios_Clear PROC
    mov g_Scenarios.count,0
    mov eax,1
    ret
ChainScenarios_Clear ENDP

END
