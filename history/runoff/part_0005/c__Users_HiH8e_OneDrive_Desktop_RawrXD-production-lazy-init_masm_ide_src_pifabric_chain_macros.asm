; pifabric_chain_macros.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC ChainMacros_Init
PUBLIC ChainMacros_Add
PUBLIC ChainMacros_GetCount
PUBLIC ChainMacros_Clear

.data
MAX_MACROS EQU 32

Macro STRUCT
    id   DWORD ?
    code DWORD ?
Macro ENDS

Macros STRUCT
    count DWORD ?
    items Macro MAX_MACROS DUP(<0,0>)
Macros ENDS

g_Macros Macros <0>

.code

ChainMacros_Init PROC
    mov g_Macros.count,0
    mov eax,1
    ret
ChainMacros_Init ENDP

ChainMacros_Add PROC id:DWORD,code:DWORD
    mov eax,g_Macros.count
    cmp eax,MAX_MACROS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Macro
    mov edx,OFFSET g_Macros.items
    add edx,ecx
    mov [edx].Macro.id,id
    mov [edx].Macro.code,code
    inc g_Macros.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
ChainMacros_Add ENDP

ChainMacros_GetCount PROC
    mov eax,g_Macros.count
    ret
ChainMacros_GetCount ENDP

ChainMacros_Clear PROC
    mov g_Macros.count,0
    mov eax,1
    ret
ChainMacros_Clear ENDP

END
