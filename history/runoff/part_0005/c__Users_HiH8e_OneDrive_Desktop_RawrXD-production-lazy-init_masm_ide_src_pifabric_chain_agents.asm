; pifabric_chain_agents.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC ChainAgents_Init
PUBLIC ChainAgents_Add
PUBLIC ChainAgents_GetCount
PUBLIC ChainAgents_Clear

.data
MAX_AGENTS EQU 32

Agent STRUCT
    id   DWORD ?
    mode DWORD ?
Agent ENDS

Agents STRUCT
    count DWORD ?
    items Agent MAX_AGENTS DUP(<0,0>)
Agents ENDS

g_Agents Agents <0>

.code

ChainAgents_Init PROC
    mov g_Agents.count,0
    mov eax,1
    ret
ChainAgents_Init ENDP

ChainAgents_Add PROC id:DWORD,mode:DWORD
    mov eax,g_Agents.count
    cmp eax,MAX_AGENTS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Agent
    mov edx,OFFSET g_Agents.items
    add edx,ecx
    mov [edx].Agent.id,id
    mov [edx].Agent.mode,mode
    inc g_Agents.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
ChainAgents_Add ENDP

ChainAgents_GetCount PROC
    mov eax,g_Agents.count
    ret
ChainAgents_GetCount ENDP

ChainAgents_Clear PROC
    mov g_Agents.count,0
    mov eax,1
    ret
ChainAgents_Clear ENDP

END