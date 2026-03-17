; pifabric_ai_options.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC AiOptions_Init
PUBLIC AiOptions_Set
PUBLIC AiOptions_Get
PUBLIC AiOptions_Clear

.data
MAX_OPTIONS EQU 32

Option STRUCT
    id DWORD ?
    value DWORD ?
Option ENDS

Options STRUCT
    count DWORD ?
    items Option MAX_OPTIONS DUP(<0,0>)
Options ENDS

g_Options Options <0>

.code

AiOptions_Init PROC
    mov g_Options.count,0
    mov eax,1
    ret
AiOptions_Init ENDP

AiOptions_Set PROC id:DWORD,value:DWORD
    mov eax,g_Options.count
    cmp eax,MAX_OPTIONS
    jae @fail
    mov ecx,eax
    imul ecx,SIZEOF Option
    mov edx,OFFSET g_Options.items
    add edx,ecx
    mov [edx].Option.id,id
    mov [edx].Option.value,value
    inc g_Options.count
    mov eax,1
    ret
@fail:
    xor eax,eax
    ret
AiOptions_Set ENDP

AiOptions_Get PROC id:DWORD
    mov ecx,g_Options.count
    mov edx,OFFSET g_Options.items
@loop:
    test ecx,ecx
    jz @fail
    mov eax,[edx].Option.id
    cmp eax,id
    je @found
    add edx,SIZEOF Option
    dec ecx
    jmp @loop
@found:
    mov eax,[edx].Option.value
    ret
@fail:
    xor eax,eax
    ret
AiOptions_Get ENDP

AiOptions_Clear PROC
    mov g_Options.count,0
    mov eax,1
    ret
AiOptions_Clear ENDP

END